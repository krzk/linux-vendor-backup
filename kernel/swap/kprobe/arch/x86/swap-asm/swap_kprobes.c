/**
 * arch/asm-x86/swap_kprobes.c
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation;
 * Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
 * @author Stanislav Andreev <s.andreev@samsung.com>: added time debug profiling support; BUG() message fix
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * @section COPYRIGHT
 *
 * Copyright (C) IBM Corporation, 2002, 2004
 *
 * @section DESCRIPTION
 *
 * SWAP krpobes arch-dependend part for x86.
 */

#include<linux/module.h>
#include <linux/kdebug.h>

#include "swap_kprobes.h"
#include <kprobe/swap_kprobes.h>

#include <kprobe/swap_kdebug.h>
#include <kprobe/swap_slots.h>
#include <kprobe/swap_kprobes_deps.h>
#define SUPRESS_BUG_MESSAGES                    /**< Debug-off definition. */


static int (*swap_fixup_exception)(struct pt_regs * regs);
static void *(*swap_text_poke)(void *addr, const void *opcode, size_t len);
static void (*swap_show_registers)(struct pt_regs * regs);


/** Stack address. */
#define stack_addr(regs) ((unsigned long *)kernel_stack_pointer(regs))


#define SWAP_SAVE_REGS_STRING			\
	/* Skip cs, ip, orig_ax and gs. */	\
	"subl $16, %esp\n"			\
	"pushl %fs\n"				\
	"pushl %es\n"				\
	"pushl %ds\n"				\
	"pushl %eax\n"				\
	"pushl %ebp\n"				\
	"pushl %edi\n"				\
	"pushl %esi\n"				\
	"pushl %edx\n"				\
	"pushl %ecx\n"				\
	"pushl %ebx\n"
#define SWAP_RESTORE_REGS_STRING		\
	"popl %ebx\n"				\
	"popl %ecx\n"				\
	"popl %edx\n"				\
	"popl %esi\n"				\
	"popl %edi\n"				\
	"popl %ebp\n"				\
	"popl %eax\n"				\
	/* Skip ds, es, fs, gs, orig_ax, and ip. Note: don't pop cs here*/\
	"addl $24, %esp\n"


/*
 * Function return probe trampoline:
 *      - init_kprobes() establishes a probepoint here
 *      - When the probed function returns, this probe
 *        causes the handlers to fire
 */
void swap_kretprobe_trampoline(void);
__asm(
	".global swap_kretprobe_trampoline	\n"
	"swap_kretprobe_trampoline:		\n"
	"pushf					\n"
	SWAP_SAVE_REGS_STRING
	"movl %esp, %eax			\n"
	"call trampoline_probe_handler_x86	\n"
	/* move eflags to cs */
	"movl 56(%esp), %edx			\n"
	"movl %edx, 52(%esp)			\n"
	/* replace saved flags with true return address. */
	"movl %eax, 56(%esp)			\n"
	SWAP_RESTORE_REGS_STRING
	"popf					\n"
	"ret					\n"
);

/* insert a jmp code */
static __always_inline void set_jmp_op (void *from, void *to)
{
	struct __arch_jmp_op
	{
		char op;
		long raddr;
	} __attribute__ ((packed)) * jop;
	jop = (struct __arch_jmp_op *) from;
	jop->raddr = (long) (to) - ((long) (from) + 5);
	jop->op = RELATIVEJUMP_INSTRUCTION;
}

/**
 * @brief Check if opcode can be boosted.
 *
 * @param opcodes Opcode to check.
 * @return Non-zero if opcode can be boosted.
 */
int swap_can_boost(kprobe_opcode_t *opcodes)
{
#define W(row,b0,b1,b2,b3,b4,b5,b6,b7,b8,b9,ba,bb,bc,bd,be,bf)		      \
	(((b0##UL << 0x0)|(b1##UL << 0x1)|(b2##UL << 0x2)|(b3##UL << 0x3) |   \
	  (b4##UL << 0x4)|(b5##UL << 0x5)|(b6##UL << 0x6)|(b7##UL << 0x7) |   \
	  (b8##UL << 0x8)|(b9##UL << 0x9)|(ba##UL << 0xa)|(bb##UL << 0xb) |   \
	  (bc##UL << 0xc)|(bd##UL << 0xd)|(be##UL << 0xe)|(bf##UL << 0xf))    \
	 << (row % 32))
	/*
	 * Undefined/reserved opcodes, conditional jump, Opcode Extension
	 * Groups, and some special opcodes can not be boost.
	 */
	static const unsigned long twobyte_is_boostable[256 / 32] = {
		/*      0 1 2 3 4 5 6 7 8 9 a b c d e f         */
		/*      -------------------------------         */
		W (0x00, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0) |	/* 00 */
			W (0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),	/* 10 */
		W (0x20, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) |	/* 20 */
			W (0x30, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),	/* 30 */
		W (0x40, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1) |	/* 40 */
			W (0x50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),	/* 50 */
		W (0x60, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1) |	/* 60 */
			W (0x70, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1),	/* 70 */
		W (0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) |	/* 80 */
			W (0x90, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1),	/* 90 */
		W (0xa0, 1, 1, 0, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1) |	/* a0 */
			W (0xb0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1),	/* b0 */
		W (0xc0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1) |	/* c0 */
			W (0xd0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1),	/* d0 */
		W (0xe0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1) |	/* e0 */
			W (0xf0, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 0)	/* f0 */
			/*      -------------------------------         */
			/*      0 1 2 3 4 5 6 7 8 9 a b c d e f         */
	};
#undef W
	kprobe_opcode_t opcode;
	kprobe_opcode_t *orig_opcodes = opcodes;
retry:
	if (opcodes - orig_opcodes > MAX_INSN_SIZE - 1)
		return 0;
	opcode = *(opcodes++);

	/* 2nd-byte opcode */
	if (opcode == 0x0f)
	{
		if (opcodes - orig_opcodes > MAX_INSN_SIZE - 1)
			return 0;
		return test_bit (*opcodes, twobyte_is_boostable);
	}

	switch (opcode & 0xf0)
	{
		case 0x60:
			if (0x63 < opcode && opcode < 0x67)
				goto retry;	/* prefixes */
			/* can't boost Address-size override and bound */
			return (opcode != 0x62 && opcode != 0x67);
		case 0x70:
			return 0;	/* can't boost conditional jump */
		case 0xc0:
			/* can't boost software-interruptions */
			return (0xc1 < opcode && opcode < 0xcc) || opcode == 0xcf;
		case 0xd0:
			/* can boost AA* and XLAT */
			return (opcode == 0xd4 || opcode == 0xd5 || opcode == 0xd7);
		case 0xe0:
			/* can boost in/out and absolute jmps */
			return ((opcode & 0x04) || opcode == 0xea);
		case 0xf0:
			if ((opcode & 0x0c) == 0 && opcode != 0xf1)
				goto retry;	/* lock/rep(ne) prefix */
			/* clear and set flags can be boost */
			return (opcode == 0xf5 || (0xf7 < opcode && opcode < 0xfe));
		default:
			if (opcode == 0x26 || opcode == 0x36 || opcode == 0x3e)
				goto retry;	/* prefixes */
			/* can't boost CS override and call */
			return (opcode != 0x2e && opcode != 0x9a);
	}
}
EXPORT_SYMBOL_GPL(swap_can_boost);

/*
 * returns non-zero if opcode modifies the interrupt flag.
 */
static int is_IF_modifier (kprobe_opcode_t opcode)
{
	switch (opcode)
	{
		case 0xfa:		/* cli */
		case 0xfb:		/* sti */
		case 0xcf:		/* iret/iretd */
		case 0x9d:		/* popf/popfd */
			return 1;
	}
	return 0;
}

/**
 * @brief Creates trampoline for kprobe.
 *
 * @param p Pointer to kprobe.
 * @param sm Pointer to slot manager
 * @return 0 on success, error code on error.
 */
int swap_arch_prepare_kprobe(struct kprobe *p, struct slot_manager *sm)
{
	/* insn: must be on special executable page on i386. */
	p->ainsn.insn = swap_slot_alloc(sm);
	if (p->ainsn.insn == NULL)
		return -ENOMEM;

	memcpy(p->ainsn.insn, p->addr, MAX_INSN_SIZE);

	p->opcode = *p->addr;
	p->ainsn.boostable = swap_can_boost(p->addr) ? 0 : -1;

	return 0;
}

/**
 * @brief Prepares singlestep for current CPU.
 *
 * @param p Pointer to kprobe.
 * @param regs Pointer to CPU registers data.
 * @return Void.
 */
void prepare_singlestep (struct kprobe *p, struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	if (p->ss_addr[cpu]) {
		regs->EREG(ip) = (unsigned long)p->ss_addr[cpu];
		p->ss_addr[cpu] = NULL;
	}
	else
	{
		regs->EREG (flags) |= TF_MASK;
		regs->EREG (flags) &= ~IF_MASK;
		/*single step inline if the instruction is an int3 */
		if (p->opcode == BREAKPOINT_INSTRUCTION){
			regs->EREG (ip) = (unsigned long) p->addr;
			//printk("break_insn!!!\n");
		}
		else
			regs->EREG (ip) = (unsigned long) p->ainsn.insn;
	}
}
EXPORT_SYMBOL_GPL(prepare_singlestep);

/**
 * @brief Saves previous kprobe.
 *
 * @param kcb Pointer to kprobe_ctlblk struct whereto save current kprobe.
 * @param p_run Pointer to kprobe.
 * @return Void.
 */
void save_previous_kprobe (struct kprobe_ctlblk *kcb, struct kprobe *cur_p)
{
	if (kcb->prev_kprobe.kp != NULL)
	{
		panic("no space to save new probe[]: task = %d/%s, prev %p, current %p, new %p,",
				current->pid, current->comm, kcb->prev_kprobe.kp->addr,
				swap_kprobe_running()->addr, cur_p->addr);
	}


	kcb->prev_kprobe.kp = swap_kprobe_running();
	kcb->prev_kprobe.status = kcb->kprobe_status;

}

/**
 * @brief Restores previous kprobe.
 *
 * @param kcb Pointer to kprobe_ctlblk which contains previous kprobe.
 * @return Void.
 */
void restore_previous_kprobe (struct kprobe_ctlblk *kcb)
{
	__get_cpu_var(swap_current_kprobe) = kcb->prev_kprobe.kp;
	kcb->kprobe_status = kcb->prev_kprobe.status;
	kcb->prev_kprobe.kp = NULL;
	kcb->prev_kprobe.status = 0;
}

/**
 * @brief Sets currently running kprobe.
 *
 * @param p Pointer to currently running kprobe.
 * @param regs Pointer to CPU registers data.
 * @param kcb Pointer to kprobe_ctlblk.
 * @return Void.
 */
void set_current_kprobe (struct kprobe *p, struct pt_regs *regs, struct kprobe_ctlblk *kcb)
{
	__get_cpu_var(swap_current_kprobe) = p;
	DBPRINTF ("set_current_kprobe[]: p=%p addr=%p\n", p, p->addr);
	kcb->kprobe_saved_eflags = kcb->kprobe_old_eflags = (regs->EREG (flags) & (TF_MASK | IF_MASK));
	if (is_IF_modifier (p->opcode))
		kcb->kprobe_saved_eflags &= ~IF_MASK;
}

static int setup_singlestep(struct kprobe *p, struct pt_regs *regs,
			    struct kprobe_ctlblk *kcb)
{
#if !defined(CONFIG_PREEMPT) || defined(CONFIG_PM)
	if (p->ainsn.boostable == 1 && !p->post_handler) {
		/* Boost up -- we can execute copied instructions directly */
		swap_reset_current_kprobe();
		regs->ip = (unsigned long)p->ainsn.insn;
		preempt_enable_no_resched();

		return 1;
	}
#endif // !CONFIG_PREEMPT

	prepare_singlestep(p, regs);
	kcb->kprobe_status = KPROBE_HIT_SS;

	return 1;
}

static int __kprobe_handler(struct pt_regs *regs)
{
	struct kprobe *p = 0;
	int ret = 0, reenter = 0;
	kprobe_opcode_t *addr = NULL;
	struct kprobe_ctlblk *kcb;

	addr = (kprobe_opcode_t *) (regs->EREG (ip) - sizeof (kprobe_opcode_t));

	preempt_disable ();

	kcb = swap_get_kprobe_ctlblk();
	p = swap_get_kprobe(addr);

	/* Check we're not actually recursing */
	if (swap_kprobe_running()) {
		if (p) {
			if (kcb->kprobe_status == KPROBE_HIT_SS && *p->ainsn.insn == BREAKPOINT_INSTRUCTION) {
				regs->EREG(flags) &= ~TF_MASK;
				regs->EREG(flags) |= kcb->kprobe_saved_eflags;
				goto no_kprobe;
			}


			/* We have reentered the kprobe_handler(), since
			 * another probe was hit while within the handler.
			 * We here save the original kprobes variables and
			 * just single step on the instruction of the new probe
			 * without calling any user handlers.
			 */
			save_previous_kprobe (kcb, p);
			set_current_kprobe (p, regs, kcb);
			swap_kprobes_inc_nmissed_count(p);
			prepare_singlestep (p, regs);
			kcb->kprobe_status = KPROBE_REENTER;

			return 1;
		} else {
			if (*addr != BREAKPOINT_INSTRUCTION) {
				/* The breakpoint instruction was removed by
				 * another cpu right after we hit, no further
				 * handling of this interrupt is appropriate
				 */
				regs->EREG(ip) -= sizeof(kprobe_opcode_t);
				ret = 1;
				goto no_kprobe;
			}

			p = __get_cpu_var(swap_current_kprobe);
			if (p->break_handler && p->break_handler(p, regs))
				goto ss_probe;

			goto no_kprobe;
		}
	}

	if (!p) {
		if (*addr != BREAKPOINT_INSTRUCTION) {
			/*
			 * The breakpoint instruction was removed right
			 * after we hit it.  Another cpu has removed
			 * either a probepoint or a debugger breakpoint
			 * at this address.  In either case, no further
			 * handling of this interrupt is appropriate.
			 * Back up over the (now missing) int3 and run
			 * the original instruction.
			 */
			regs->EREG(ip) -= sizeof(kprobe_opcode_t);
			ret = 1;
		}

		if (!p) {
			/* Not one of ours: let kernel handle it */
			DBPRINTF ("no_kprobe");
			goto no_kprobe;
		}
	}

	set_current_kprobe (p, regs, kcb);

	if(!reenter)
		kcb->kprobe_status = KPROBE_HIT_ACTIVE;

	if (p->pre_handler) {
		ret = p->pre_handler(p, regs);
		if (ret)
			return ret;
	}

ss_probe:
	setup_singlestep(p, regs, kcb);

	return 1;

no_kprobe:
	preempt_enable_no_resched ();

	return ret;
}

static int kprobe_handler(struct pt_regs *regs)
{
	int ret;
#ifdef SUPRESS_BUG_MESSAGES
	int swap_oops_in_progress;
	/*
	 * oops_in_progress used to avoid BUG() messages
	 * that slow down kprobe_handler() execution
	 */
	swap_oops_in_progress = oops_in_progress;
	oops_in_progress = 1;
#endif

	ret = __kprobe_handler(regs);

#ifdef SUPRESS_BUG_MESSAGES
	oops_in_progress = swap_oops_in_progress;
#endif

	return ret;
}

/**
 * @brief Probe pre handler.
 *
 * @param p Pointer to fired kprobe.
 * @param regs Pointer to CPU registers data.
 * @return 0.
 */
int swap_setjmp_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct jprobe *jp = container_of (p, struct jprobe, kp);
	kprobe_pre_entry_handler_t pre_entry;
	entry_point_t entry;

	unsigned long addr;
	struct kprobe_ctlblk *kcb = swap_get_kprobe_ctlblk();

	pre_entry = (kprobe_pre_entry_handler_t) jp->pre_entry;
	entry = (entry_point_t) jp->entry;

	kcb->jprobe_saved_regs = *regs;
	kcb->jprobe_saved_esp = stack_addr(regs);
	addr = (unsigned long)(kcb->jprobe_saved_esp);

	/* TBD: As Linus pointed out, gcc assumes that the callee
	 * owns the argument space and could overwrite it, e.g.
	 * tailcall optimization. So, to be absolutely safe
	 * we also save and restore enough stack bytes to cover
	 * the argument area. */
	memcpy(kcb->jprobes_stack, (kprobe_opcode_t *)addr, MIN_STACK_SIZE (addr));
	regs->EREG(flags) &= ~IF_MASK;
	trace_hardirqs_off();
	if (pre_entry)
		p->ss_addr[smp_processor_id()] = (kprobe_opcode_t *)
						 pre_entry(jp->priv_arg, regs);

	regs->EREG(ip) = (unsigned long)(jp->entry);

	return 1;
}

/**
 * @brief Jprobe return end.
 *
 * @return Void.
 */
void swap_jprobe_return_end(void);

/**
 * @brief Jprobe return code.
 *
 * @return Void.
 */
void swap_jprobe_return(void)
{
	struct kprobe_ctlblk *kcb = swap_get_kprobe_ctlblk();

	asm volatile("       xchgl   %%ebx,%%esp     \n"
			"       int3			\n"
			"       .globl swap_jprobe_return_end	\n"
			"       swap_jprobe_return_end:	\n"
			"       nop			\n"::"b" (kcb->jprobe_saved_esp):"memory");
}
EXPORT_SYMBOL_GPL(swap_jprobe_return);

void arch_ujprobe_return(void)
{
}

/*
 * Called after single-stepping.  p->addr is the address of the
 * instruction whose first byte has been replaced by the "int 3"
 * instruction.  To avoid the SMP problems that can occur when we
 * temporarily put back the original opcode to single-step, we
 * single-stepped a copy of the instruction.  The address of this
 * copy is p->ainsn.insn.
 *
 * This function prepares to return from the post-single-step
 * interrupt.  We have to fix up the stack as follows:
 *
 * 0) Except in the case of absolute or indirect jump or call instructions,
 * the new eip is relative to the copied instruction.  We need to make
 * it relative to the original instruction.
 *
 * 1) If the single-stepped instruction was pushfl, then the TF and IF
 * flags are set in the just-pushed eflags, and may need to be cleared.
 *
 * 2) If the single-stepped instruction was a call, the return address
 * that is atop the stack is the address following the copied instruction.
 * We need to make it the address following the original instruction.
 *
 * This function also checks instruction size for preparing direct execution.
 */
static void resume_execution (struct kprobe *p, struct pt_regs *regs, struct kprobe_ctlblk *kcb)
{
	unsigned long *tos;
	unsigned long copy_eip = (unsigned long) p->ainsn.insn;
	unsigned long orig_eip = (unsigned long) p->addr;
	kprobe_opcode_t insns[2];

	regs->EREG (flags) &= ~TF_MASK;

	tos = stack_addr(regs);
	insns[0] = p->ainsn.insn[0];
	insns[1] = p->ainsn.insn[1];

	switch (insns[0])
	{
		case 0x9c:		/* pushfl */
			*tos &= ~(TF_MASK | IF_MASK);
			*tos |= kcb->kprobe_old_eflags;
			break;
		case 0xc2:		/* iret/ret/lret */
		case 0xc3:
		case 0xca:
		case 0xcb:
		case 0xcf:
		case 0xea:		/* jmp absolute -- eip is correct */
			/* eip is already adjusted, no more changes required */
			p->ainsn.boostable = 1;
			goto no_change;
		case 0xe8:		/* call relative - Fix return addr */
			*tos = orig_eip + (*tos - copy_eip);
			break;
		case 0x9a:		/* call absolute -- same as call absolute, indirect */
			*tos = orig_eip + (*tos - copy_eip);
			goto no_change;
		case 0xff:
			if ((insns[1] & 0x30) == 0x10)
			{
				/*
				 * call absolute, indirect
				 * Fix return addr; eip is correct.
				 * But this is not boostable
				 */
				*tos = orig_eip + (*tos - copy_eip);
				goto no_change;
			}
			else if (((insns[1] & 0x31) == 0x20) ||	/* jmp near, absolute indirect */
					((insns[1] & 0x31) == 0x21))
			{		/* jmp far, absolute indirect */
				/* eip is correct. And this is boostable */
				p->ainsn.boostable = 1;
				goto no_change;
			}
		default:
			break;
	}

	if (p->ainsn.boostable == 0)
	{
		if ((regs->EREG (ip) > copy_eip) && (regs->EREG (ip) - copy_eip) + 5 < MAX_INSN_SIZE)
		{
			/*
			 * These instructions can be executed directly if it
			 * jumps back to correct address.
			 */
			set_jmp_op((void *)regs->EREG(ip), (void *)orig_eip + (regs->EREG(ip) - copy_eip));
			p->ainsn.boostable = 1;
		}
		else
		{
			p->ainsn.boostable = -1;
		}
	}

	regs->EREG (ip) = orig_eip + (regs->EREG (ip) - copy_eip);

no_change:
	return;
}

/*
 * Interrupts are disabled on entry as trap1 is an interrupt gate and they
 * remain disabled thoroughout this function.
 */
static int post_kprobe_handler (struct pt_regs *regs)
{
	struct kprobe *cur = swap_kprobe_running();
	struct kprobe_ctlblk *kcb = swap_get_kprobe_ctlblk();

	if (!cur)
		return 0;
	if ((kcb->kprobe_status != KPROBE_REENTER) && cur->post_handler)
	{
		kcb->kprobe_status = KPROBE_HIT_SSDONE;
		cur->post_handler (cur, regs, 0);
	}

	resume_execution (cur, regs, kcb);
	regs->EREG (flags) |= kcb->kprobe_saved_eflags;
#ifndef CONFIG_X86
	trace_hardirqs_fixup_flags (regs->EREG (flags));
#endif // CONFIG_X86
	/*Restore back the original saved kprobes variables and continue. */
	if (kcb->kprobe_status == KPROBE_REENTER)
	{
		restore_previous_kprobe (kcb);
		goto out;
	}
	swap_reset_current_kprobe();
out:
	preempt_enable_no_resched ();

	/*
	 * if somebody else is singlestepping across a probe point, eflags
	 * will have TF set, in which case, continue the remaining processing
	 * of do_debug, as if this is not a probe hit.
	 */
	if (regs->EREG (flags) & TF_MASK)
		return 0;

	return 1;
}

static int kprobe_fault_handler(struct pt_regs *regs, int trapnr)
{
	struct kprobe *cur = swap_kprobe_running();
	struct kprobe_ctlblk *kcb = swap_get_kprobe_ctlblk();

	switch (kcb->kprobe_status)
	{
		case KPROBE_HIT_SS:
		case KPROBE_REENTER:
			/*
			 * We are here because the instruction being single
			 * stepped caused a page fault. We reset the current
			 * kprobe and the eip points back to the probe address
			 * and allow the page fault handler to continue as a
			 * normal page fault.
			 */
			regs->EREG (ip) = (unsigned long) cur->addr;
			regs->EREG (flags) |= kcb->kprobe_old_eflags;
			if (kcb->kprobe_status == KPROBE_REENTER)
				restore_previous_kprobe (kcb);
			else
				swap_reset_current_kprobe();
			preempt_enable_no_resched ();
			break;
		case KPROBE_HIT_ACTIVE:
		case KPROBE_HIT_SSDONE:
			/*
			 * We increment the nmissed count for accounting,
			 * we can also use npre/npostfault count for accouting
			 * these specific fault cases.
			 */
			swap_kprobes_inc_nmissed_count(cur);

			/*
			 * We come here because instructions in the pre/post
			 * handler caused the page_fault, this could happen
			 * if handler tries to access user space by
			 * copy_from_user(), get_user() etc. Let the
			 * user-specified handler try to fix it first.
			 */
			if (cur->fault_handler && cur->fault_handler (cur, regs, trapnr))
				return 1;

			/*
			 * In case the user-specified fault handler returned
			 * zero, try to fix up.
			 */
			if (swap_fixup_exception(regs))
				return 1;

			/*
			 * fixup_exception() could not handle it,
			 * Let do_page_fault() fix it.
			 */
			break;
		default:
			break;
	}
	return 0;
}

static int kprobe_exceptions_notify(struct notifier_block *self,
				    unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *) data;
	int ret = NOTIFY_DONE;

	DBPRINTF ("val = %ld, data = 0x%X", val, (unsigned int) data);

	if (args->regs == NULL || user_mode_vm(args->regs))
		return ret;

	DBPRINTF ("switch (val) %lu %d %d", val, DIE_INT3, DIE_TRAP);
	switch (val)
	{
#ifdef CONFIG_KPROBES
		case DIE_INT3:
#else
		case DIE_TRAP:
#endif
			DBPRINTF ("before kprobe_handler ret=%d %p", ret, args->regs);
			if (kprobe_handler (args->regs))
				ret = NOTIFY_STOP;
			DBPRINTF ("after kprobe_handler ret=%d %p", ret, args->regs);
			break;
		case DIE_DEBUG:
			if (post_kprobe_handler (args->regs))
				ret = NOTIFY_STOP;
			break;
		case DIE_GPF:
			/* swap_kprobe_running() needs smp_processor_id() */
			preempt_disable ();
			if (swap_kprobe_running() &&
			    kprobe_fault_handler(args->regs, args->trapnr))
				ret = NOTIFY_STOP;
			preempt_enable ();
			break;
		default:
			break;
	}
	DBPRINTF ("ret=%d", ret);
	/* if(ret == NOTIFY_STOP) */
	/* 	handled_exceptions++; */

	return ret;
}

static struct notifier_block kprobe_exceptions_nb = {
	.notifier_call = kprobe_exceptions_notify,
	.priority = INT_MAX
};

/**
 * @brief Longjump break handler.
 *
 * @param p Pointer to fired kprobe.
 * @param regs Pointer to CPU registers data.
 * @return 0 on success.
 */
int swap_longjmp_break_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe_ctlblk *kcb = swap_get_kprobe_ctlblk();
	u8 *addr = (u8 *) (regs->EREG (ip) - 1);
	unsigned long stack_addr = (unsigned long) (kcb->jprobe_saved_esp);
	struct jprobe *jp = container_of (p, struct jprobe, kp);

	DBPRINTF ("p = %p\n", p);

	if ((addr > (u8 *)swap_jprobe_return) && 
	    (addr < (u8 *)swap_jprobe_return_end)) {
		if (stack_addr(regs) != kcb->jprobe_saved_esp) {
			struct pt_regs *saved_regs = &kcb->jprobe_saved_regs;
			printk("current esp %p does not match saved esp %p\n",
			       stack_addr(regs), kcb->jprobe_saved_esp);
			printk ("Saved registers for jprobe %p\n", jp);
			swap_show_registers(saved_regs);
			printk ("Current registers\n");
			swap_show_registers(regs);
			panic("BUG");
			//BUG ();
		}
		*regs = kcb->jprobe_saved_regs;
		memcpy ((kprobe_opcode_t *) stack_addr, kcb->jprobes_stack, MIN_STACK_SIZE (stack_addr));
		preempt_enable_no_resched ();
		return 1;
	}

	return 0;
}

/**
 * @brief Arms kprobe.
 *
 * @param p Pointer to target kprobe.
 * @return Void.
 */
void swap_arch_arm_kprobe(struct kprobe *p)
{
	swap_text_poke(p->addr,
		       ((unsigned char[]){BREAKPOINT_INSTRUCTION}), 1);
}

/**
 * @brief Disarms kprobe.
 *
 * @param p Pointer to target kprobe.
 * @return Void.
 */
void swap_arch_disarm_kprobe(struct kprobe *p)
{
	swap_text_poke(p->addr, &p->opcode, 1);
}

static __used void *trampoline_probe_handler_x86(struct pt_regs *regs)
{
	return (void *)trampoline_probe_handler(NULL, regs);
}

/**
 * @brief Prepares kretprobes, saves ret address, makes function return to
 * trampoline.
 *
 * @param ri Pointer to kretprobe_instance.
 * @param regs Pointer to CPU registers data.
 * @return Void.
 */
void swap_arch_prepare_kretprobe(struct kretprobe_instance *ri,
				 struct pt_regs *regs)
{
	unsigned long *ptr_ret_addr = stack_addr(regs);

	/* for __switch_to probe */
	if ((unsigned long)ri->rp->kp.addr == sched_addr) {
		ri->sp = NULL;
		ri->task = (struct task_struct *)regs->dx;
	} else {
		ri->sp = ptr_ret_addr;
	}

	/* Save the return address */
	ri->ret_addr = (unsigned long *)*ptr_ret_addr;

	/* Replace the return addr with trampoline addr */
	*ptr_ret_addr = (unsigned long)&swap_kretprobe_trampoline;
}





/*
 ******************************************************************************
 *                                   kjumper                                  *
 ******************************************************************************
 */
struct kj_cb_data {
	struct pt_regs regs;
	struct kprobe *p;

	jumper_cb_t cb;
	char data[0];
};

static struct kj_cb_data * __used kjump_handler(struct kj_cb_data *data)
{
	/* call callback */
	data->cb(data->data);

	return data;
}

void kjump_trampoline(void);
void kjump_trampoline_int3(void);
__asm(
	"kjump_trampoline:		\n"
	"call	kjump_handler		\n"
	"kjump_trampoline_int3:		\n"
	"nop				\n"	/* for restore_regs_kp */
);

int set_kjump_cb(struct pt_regs *regs, jumper_cb_t cb, void *data, size_t size)
{
	struct kj_cb_data *cb_data;

	cb_data = kmalloc(sizeof(*cb_data) + size, GFP_ATOMIC);
	if (cb_data == NULL)
		return -ENOMEM;

	/* save regs */
	cb_data->regs = *regs;

	cb_data->p = swap_kprobe_running();
	cb_data->cb = cb;

	/* save data */
	if (size)
		memcpy(cb_data->data, data, size);

	/* save pointer cb_data at ax */
	regs->ax = (long)cb_data;

	/* jump to kjump_trampoline */
	regs->ip = (unsigned long)&kjump_trampoline;

	swap_reset_current_kprobe();
	preempt_enable_no_resched();

	return 1;
}
EXPORT_SYMBOL_GPL(set_kjump_cb);

static int restore_regs_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kj_cb_data *data = (struct kj_cb_data *)regs->ax;
	struct kprobe *kp = data->p;
	struct kprobe_ctlblk *kcb = swap_get_kprobe_ctlblk();

	/* restore regs */
	*regs = data->regs;

	/* FIXME: potential memory leak, when process kill */
	kfree(data);

	kcb = swap_get_kprobe_ctlblk();

	set_current_kprobe(kp, regs, kcb);
	setup_singlestep(kp, regs, kcb);

	return 1;
}

static struct kprobe restore_regs_kp = {
	.pre_handler = restore_regs_pre_handler,
	.addr = (kprobe_opcode_t *)&kjump_trampoline_int3,	/* nop */
};

static int kjump_init(void)
{
	int ret;

	ret = swap_register_kprobe(&restore_regs_kp);
	if (ret)
		printk("ERROR: kjump_init(), ret=%d\n", ret);

	return ret;
}

static void kjump_exit(void)
{
	swap_unregister_kprobe(&restore_regs_kp);
}





/*
 ******************************************************************************
 *                                   jumper                                   *
 ******************************************************************************
 */
struct cb_data {
	unsigned long ret_addr;
	unsigned long bx;

	jumper_cb_t cb;
	char data[0];
};

static unsigned long __used get_bx(struct cb_data *data)
{
	return data->bx;
}

static unsigned long __used jump_handler(struct cb_data *data)
{
	unsigned long ret_addr = data->ret_addr;

	/* call callback */
	data->cb(data->data);

	/* FIXME: potential memory leak, when process kill */
	kfree(data);

	return ret_addr;
}

void jump_trampoline(void);
__asm(
	"jump_trampoline:		\n"
	"pushf				\n"
	SWAP_SAVE_REGS_STRING
	"movl	%ebx, %eax		\n"	/* data --> ax */
	"call	get_bx			\n"
	"movl	%eax, (%esp)		\n"	/* restore bx */
	"movl	%ebx, %eax		\n"	/* data --> ax */
	"call	jump_handler		\n"
	/* move flags to cs */
	"movl 56(%esp), %edx		\n"
	"movl %edx, 52(%esp)		\n"
	/* replace saved flags with true return address. */
	"movl %eax, 56(%esp)		\n"
	SWAP_RESTORE_REGS_STRING
	"popf\n"
	"ret\n"
);

unsigned long get_jump_addr(void)
{
	return (unsigned long)&jump_trampoline;
}
EXPORT_SYMBOL_GPL(get_jump_addr);

int set_jump_cb(unsigned long ret_addr, struct pt_regs *regs,
		jumper_cb_t cb, void *data, size_t size)
{
	struct cb_data *cb_data;

	cb_data = kmalloc(sizeof(*cb_data) + size, GFP_ATOMIC);
	if (cb_data == NULL)
		return -ENOMEM;

	/* save data */
	cb_data->ret_addr = ret_addr;
	cb_data->cb = cb;
	cb_data->bx = regs->bx;
	memcpy(cb_data->data, data, size);

	/* save cb_data to bx */
	regs->bx = (long)cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(set_jump_cb);





/**
 * @brief Initializes x86 module deps.
 *
 * @return 0 on success, negative error code on error.
 */
int arch_init_module_deps()
{
	const char *sym;

	sym = "fixup_exception";
	swap_fixup_exception = (void *)swap_ksyms(sym);
	if (swap_fixup_exception == NULL)
		goto not_found;

	sym = "text_poke";
	swap_text_poke = (void *)swap_ksyms(sym);
	if (swap_text_poke == NULL)
		goto not_found;

	sym = "show_registers";
	swap_show_registers = (void *)swap_ksyms(sym);
	if (swap_show_registers == NULL)
		goto not_found;

	return 0;

not_found:
	printk("ERROR: symbol %s(...) not found\n", sym);
	return -ESRCH;
}

/**
 * @brief Initializes kprobes module for ARM arch.
 *
 * @return 0 on success, error code on error.
 */
int swap_arch_init_kprobes(void)
{
	int ret;

	ret = register_die_notifier(&kprobe_exceptions_nb);
	if (ret)
		return ret;

	ret = kjump_init();
	if (ret)
		unregister_die_notifier(&kprobe_exceptions_nb);

	return ret;
}

/**
 * @brief Uninitializes kprobe module.
 *
 * @return Void.
 */
void swap_arch_exit_kprobes(void)
{
	kjump_exit();
	unregister_die_notifier (&kprobe_exceptions_nb);
}
