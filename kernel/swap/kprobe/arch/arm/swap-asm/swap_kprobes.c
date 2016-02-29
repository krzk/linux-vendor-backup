/**
 * kprobe/arch/asm-arm/swap_kprobes.c
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: initial implementation for ARM/MIPS
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial implementation; Support x86.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for separating core and arch parts
 * @author Alexander Shirshikov <a.shirshikov@samsung.com>: initial implementation for Thumb
 * @author Stanislav Andreev <s.andreev@samsung.com>: added time debug profiling support; BUG() message fix
 * @author Stanislav Andreev <s.andreev@samsung.com>: redesign of kprobe functionality -
 * kprobe_handler() now called via undefined instruction hooks
 * @author Stanislav Andreev <s.andreev@samsung.com>: hash tables search implemented for uprobes
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
 * Copyright (C) Samsung Electronics, 2006-2014
 *
 * @section DESCRIPTION
 *
 * SWAP kprobe implementation for ARM architecture.
 */

#include <linux/module.h>
#include <linux/mm.h>

#include "swap_kprobes.h"
#include "trampoline_arm.h"
#include <kprobe/swap_kprobes.h>

#include <kprobe/swap_kdebug.h>
#include <kprobe/swap_slots.h>
#include <kprobe/swap_kprobes_deps.h>
#include <ksyms/ksyms.h>

#include <asm/cacheflush.h>
#include <asm/traps.h>
#include <asm/ptrace.h>
#include <linux/list.h>
#include <linux/hash.h>

#define SUPRESS_BUG_MESSAGES            /**< Debug-off definition */

#define sign_extend(x, signbit) ((x) | (0 - ((x) & (1 << (signbit)))))
#define branch_displacement(insn) sign_extend(((insn) & 0xffffff) << 2, 25)


static void (*__swap_register_undef_hook)(struct undef_hook *hook);
static void (*__swap_unregister_undef_hook)(struct undef_hook *hook);

static unsigned long get_addr_b(unsigned long insn, unsigned long addr)
{
	/* real position less then PC by 8 */
	return (kprobe_opcode_t)((long)addr + 8 + branch_displacement(insn));
}

static int prep_pc_dep_insn_execbuf(kprobe_opcode_t *insns,
				    kprobe_opcode_t insn, int uregs)
{
	int i;

	if (uregs & 0x10) {
		int reg_mask = 0x1;
		//search in reg list
		for (i = 0; i < 13; i++, reg_mask <<= 1) {
			if (!(insn & reg_mask))
				break;
		}
	} else {
		for (i = 0; i < 13; i++) {
			if ((uregs & 0x1) && (ARM_INSN_REG_RN(insn) == i))
				continue;
			if ((uregs & 0x2) && (ARM_INSN_REG_RD(insn) == i))
				continue;
			if ((uregs & 0x4) && (ARM_INSN_REG_RS(insn) == i))
				continue;
			if ((uregs & 0x8) && (ARM_INSN_REG_RM(insn) == i))
				continue;
			break;
		}
	}

	if (i == 13) {
		DBPRINTF ("there are no free register %x in insn %lx!", uregs, insn);
		return -EINVAL;
	}
	DBPRINTF ("prep_pc_dep_insn_execbuf: using R%d, changing regs %x", i, uregs);

	// set register to save
	ARM_INSN_REG_SET_RD(insns[0], i);
	// set register to load address to
	ARM_INSN_REG_SET_RD(insns[1], i);
	// set instruction to execute and patch it
	if (uregs & 0x10) {
		ARM_INSN_REG_CLEAR_MR(insn, 15);
		ARM_INSN_REG_SET_MR(insn, i);
	} else {
		if ((uregs & 0x1) && (ARM_INSN_REG_RN(insn) == 15))
			ARM_INSN_REG_SET_RN(insn, i);
		if ((uregs & 0x2) && (ARM_INSN_REG_RD(insn) == 15))
			ARM_INSN_REG_SET_RD(insn, i);
		if ((uregs & 0x4) && (ARM_INSN_REG_RS(insn) == 15))
			ARM_INSN_REG_SET_RS(insn, i);
		if ((uregs & 0x8) && (ARM_INSN_REG_RM(insn) == 15))
			ARM_INSN_REG_SET_RM(insn, i);
	}

	insns[UPROBES_TRAMP_INSN_IDX] = insn;
	// set register to restore
	ARM_INSN_REG_SET_RD(insns[3], i);

	return 0;
}

static int arch_check_insn_arm(unsigned long insn)
{
	/* check instructions that can change PC by nature */
	if (
	 /* ARM_INSN_MATCH(UNDEF, insn) || */
	    ARM_INSN_MATCH(AUNDEF, insn) ||
	    ARM_INSN_MATCH(SWI, insn) ||
	    ARM_INSN_MATCH(BREAK, insn) ||
	    ARM_INSN_MATCH(BXJ, insn)) {
		goto bad_insn;
#ifndef CONFIG_CPU_V7
	/* check instructions that can write result to PC */
	} else if ((ARM_INSN_MATCH(DPIS, insn) ||
		    ARM_INSN_MATCH(DPRS, insn) ||
		    ARM_INSN_MATCH(DPI, insn) ||
		    ARM_INSN_MATCH(LIO, insn) ||
		    ARM_INSN_MATCH(LRO, insn)) &&
		   (ARM_INSN_REG_RD(insn) == 15)) {
		goto bad_insn;
#endif /* CONFIG_CPU_V7 */
	/* check special instruction loads store multiple registers */
	} else if ((ARM_INSN_MATCH(LM, insn) || ARM_INSN_MATCH(SM, insn)) &&
			/* store PC or load to PC */
		   (ARM_INSN_REG_MR(insn, 15) ||
			 /* store/load with PC update */
		    ((ARM_INSN_REG_RN(insn) == 15) && (insn & 0x200000)))) {
		goto bad_insn;
	}

	return 0;

bad_insn:
	return -EFAULT;
}

static int make_branch_tarmpoline(unsigned long addr, unsigned long insn,
				  unsigned long *tramp)
{
	int ok = 0;

	/* B */
	if (ARM_INSN_MATCH(B, insn) &&
	    !ARM_INSN_MATCH(BLX1, insn)) {
		/* B check can be false positive on BLX1 instruction */
		memcpy(tramp, b_cond_insn_execbuf, KPROBES_TRAMP_LEN);
		tramp[KPROBES_TRAMP_RET_BREAK_IDX] = BREAKPOINT_INSTRUCTION;
		tramp[0] |= insn & 0xf0000000;
		tramp[6] = get_addr_b(insn, addr);
		tramp[7] = addr + 4;
		ok = 1;
	/* BX, BLX (Rm) */
	} else if (ARM_INSN_MATCH(BX, insn) ||
		   ARM_INSN_MATCH(BLX2, insn)) {
		memcpy(tramp, b_r_insn_execbuf, KPROBES_TRAMP_LEN);
		tramp[0] = insn;
		tramp[KPROBES_TRAMP_RET_BREAK_IDX] = BREAKPOINT_INSTRUCTION;
		tramp[7] = addr + 4;
		ok = 1;
	/* BL, BLX (Off) */
	} else if (ARM_INSN_MATCH(BLX1, insn)) {
		memcpy(tramp, blx_off_insn_execbuf, KPROBES_TRAMP_LEN);
		tramp[0] |= 0xe0000000;
		tramp[1] |= 0xe0000000;
		tramp[KPROBES_TRAMP_RET_BREAK_IDX] = BREAKPOINT_INSTRUCTION;
		tramp[6] = get_addr_b(insn, addr) +
			   2 * (insn & 01000000) + 1; /* jump to thumb */
		tramp[7] = addr + 4;
		ok = 1;
	/* BL */
	} else if (ARM_INSN_MATCH(BL, insn)) {
		memcpy(tramp, blx_off_insn_execbuf, KPROBES_TRAMP_LEN);
		tramp[0] |= insn & 0xf0000000;
		tramp[1] |= insn & 0xf0000000;
		tramp[KPROBES_TRAMP_RET_BREAK_IDX] = BREAKPOINT_INSTRUCTION;
		tramp[6] = get_addr_b(insn, addr);
		tramp[7] = addr + 4;
		ok = 1;
	}

	return ok;
}

/**
 * @brief Creates ARM trampoline.
 *
 * @param addr Probe address.
 * @param insn Instuction at this address.
 * @param tramp Pointer to memory for trampoline.
 * @return 0 on success, error code on error.
 */
int arch_make_trampoline_arm(unsigned long addr, unsigned long insn,
			     unsigned long *tramp)
{
	int ret, uregs, pc_dep;

	if (addr & 0x03) {
		printk("Error in %s at %d: attempt to register uprobe "
		       "at an unaligned address\n", __FILE__, __LINE__);
		return -EINVAL;
	}

	ret = arch_check_insn_arm(insn);
	if (ret)
		return ret;

	if (make_branch_tarmpoline(addr, insn, tramp))
		return 0;

	uregs = pc_dep = 0;
	/* Rm */
	if (ARM_INSN_MATCH(CLZ, insn)) {
		uregs = 0xa;
		if (ARM_INSN_REG_RM(insn) == 15)
			pc_dep = 1;
	/* Rn, Rm ,Rd */
	} else if (ARM_INSN_MATCH(DPIS, insn) || ARM_INSN_MATCH(LRO, insn) ||
	    ARM_INSN_MATCH(SRO, insn)) {
		uregs = 0xb;
		if ((ARM_INSN_REG_RN(insn) == 15) ||
		    (ARM_INSN_REG_RM(insn) == 15) ||
		    (ARM_INSN_MATCH(SRO, insn) &&
		     (ARM_INSN_REG_RD(insn) == 15))) {
			pc_dep = 1;
		}
	/* Rn ,Rd */
	} else if (ARM_INSN_MATCH(DPI, insn) || ARM_INSN_MATCH(LIO, insn) ||
		   ARM_INSN_MATCH(SIO, insn)) {
		uregs = 0x3;
		if ((ARM_INSN_REG_RN(insn) == 15) ||
		    (ARM_INSN_MATCH(SIO, insn) &&
		    (ARM_INSN_REG_RD(insn) == 15))) {
			pc_dep = 1;
		}
	/* Rn, Rm, Rs */
	} else if (ARM_INSN_MATCH(DPRS, insn)) {
		uregs = 0xd;
		if ((ARM_INSN_REG_RN(insn) == 15) ||
		    (ARM_INSN_REG_RM(insn) == 15) ||
		    (ARM_INSN_REG_RS(insn) == 15)) {
			pc_dep = 1;
		}
	/* register list */
	} else if (ARM_INSN_MATCH(SM, insn)) {
		uregs = 0x10;
		if (ARM_INSN_REG_MR(insn, 15)) {
			pc_dep = 1;
		}
	}

	/* check instructions that can write result to SP and uses PC */
	if (pc_dep && (ARM_INSN_REG_RD(insn) == 13)) {
		printk("Error in %s at %d: instruction check failed (arm)\n",
		       __FILE__, __LINE__);
		return -EFAULT;
	}

	if (unlikely(uregs && pc_dep)) {
		memcpy(tramp, pc_dep_insn_execbuf, KPROBES_TRAMP_LEN);
		if (prep_pc_dep_insn_execbuf(tramp, insn, uregs) != 0) {
			printk("Error in %s at %d: failed "
			       "to prepare exec buffer for insn %lx!",
			       __FILE__, __LINE__, insn);
			return -EINVAL;
		}

		tramp[6] = addr + 8;
	} else {
		memcpy(tramp, gen_insn_execbuf, KPROBES_TRAMP_LEN);
		tramp[KPROBES_TRAMP_INSN_IDX] = insn;
	}

	/* TODO: remove for kprobe */
	tramp[KPROBES_TRAMP_RET_BREAK_IDX] = BREAKPOINT_INSTRUCTION;
	tramp[7] = addr + 4;

	return 0;
}
EXPORT_SYMBOL_GPL(arch_make_trampoline_arm);

/**
 * @brief Creates trampoline for kprobe.
 *
 * @param p Pointer to kprobe.
 * @param sm Pointer to slot manager
 * @return 0 on success, error code on error.
 */
int swap_arch_prepare_kprobe(struct kprobe *p, struct slot_manager *sm)
{
	unsigned long addr = (unsigned long)p->addr;
	unsigned long insn = p->opcode = *p->addr;
	unsigned long *tramp;
	int ret;

	tramp = swap_slot_alloc(sm);
	if (tramp == NULL)
		return -ENOMEM;

	ret = arch_make_trampoline_arm(addr, insn, tramp);
	if (ret) {
		swap_slot_free(sm, tramp);
		return ret;
	}

	flush_icache_range((unsigned long)tramp,
			   (unsigned long)tramp + KPROBES_TRAMP_LEN);

	p->ainsn.insn = tramp;

	return 0;
}

/**
 * @brief Prepares singlestep for current CPU.
 *
 * @param p Pointer to kprobe.
 * @param regs Pointer to CPU registers data.
 * @return Void.
 */
void prepare_singlestep(struct kprobe *p, struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	if (p->ss_addr[cpu]) {
		regs->ARM_pc = (unsigned long)p->ss_addr[cpu];
		p->ss_addr[cpu] = NULL;
	} else {
		regs->ARM_pc = (unsigned long)p->ainsn.insn;
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
void save_previous_kprobe(struct kprobe_ctlblk *kcb, struct kprobe *p_run)
{
	kcb->prev_kprobe.kp = swap_kprobe_running();
	kcb->prev_kprobe.status = kcb->kprobe_status;
}

/**
 * @brief Restores previous kprobe.
 *
 * @param kcb Pointer to kprobe_ctlblk which contains previous kprobe.
 * @return Void.
 */
void restore_previous_kprobe(struct kprobe_ctlblk *kcb)
{
	__get_cpu_var(swap_current_kprobe) = kcb->prev_kprobe.kp;
	kcb->kprobe_status = kcb->prev_kprobe.status;
}

/**
 * @brief Sets currently running kprobe.
 *
 * @param p Pointer to currently running kprobe.
 * @param regs Pointer to CPU registers data.
 * @param kcb Pointer to kprobe_ctlblk.
 * @return Void.
 */
void set_current_kprobe(struct kprobe *p, struct pt_regs *regs, struct kprobe_ctlblk *kcb)
{
	__get_cpu_var(swap_current_kprobe) = p;
	DBPRINTF ("set_current_kprobe: p=%p addr=%p\n", p, p->addr);
}

static int kprobe_handler(struct pt_regs *regs)
{
	struct kprobe *p, *cur;
	struct kprobe_ctlblk *kcb;

	kcb = swap_get_kprobe_ctlblk();
	cur = swap_kprobe_running();
	p = swap_get_kprobe((void *)regs->ARM_pc);

	if (p) {
		if (cur) {
			/* Kprobe is pending, so we're recursing. */
			switch (kcb->kprobe_status) {
			case KPROBE_HIT_ACTIVE:
			case KPROBE_HIT_SSDONE:
				/* A pre- or post-handler probe got us here. */
				swap_kprobes_inc_nmissed_count(p);
				save_previous_kprobe(kcb, NULL);
				set_current_kprobe(p, 0, 0);
				kcb->kprobe_status = KPROBE_REENTER;
				prepare_singlestep(p, regs);
				restore_previous_kprobe(kcb);
				break;
			default:
				/* impossible cases */
				BUG();
			}
		} else {
			set_current_kprobe(p, 0, 0);
			kcb->kprobe_status = KPROBE_HIT_ACTIVE;

			if (!p->pre_handler || !p->pre_handler(p, regs)) {
				kcb->kprobe_status = KPROBE_HIT_SS;
				prepare_singlestep(p, regs);
				swap_reset_current_kprobe();
			}
		}
	} else {
		goto no_kprobe;
	}

	return 0;

no_kprobe:
	printk("no_kprobe: Not one of ours: let kernel handle it %p\n",
			(unsigned long *)regs->ARM_pc);
	return 1;
}

/**
 * @brief Trap handler.
 *
 * @param regs Pointer to CPU register data.
 * @param instr Instruction.
 * @return kprobe_handler result.
 */
int kprobe_trap_handler(struct pt_regs *regs, unsigned int instr)
{
	int ret;
	unsigned long flags;

#ifdef SUPRESS_BUG_MESSAGES
	int swap_oops_in_progress;
	/* oops_in_progress used to avoid BUG() messages
	 * that slow down kprobe_handler() execution */
	swap_oops_in_progress = oops_in_progress;
	oops_in_progress = 1;
#endif

	local_irq_save(flags);
	preempt_disable();
	ret = kprobe_handler(regs);
	preempt_enable_no_resched();
	local_irq_restore(flags);

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
	struct jprobe *jp = container_of(p, struct jprobe, kp);
	kprobe_pre_entry_handler_t pre_entry = (kprobe_pre_entry_handler_t)jp->pre_entry;
	entry_point_t entry = (entry_point_t)jp->entry;
	pre_entry = (kprobe_pre_entry_handler_t)jp->pre_entry;

	if (pre_entry) {
		p->ss_addr[smp_processor_id()] = (void *)
						 pre_entry(jp->priv_arg, regs);
	}

	if (entry) {
		entry(regs->ARM_r0, regs->ARM_r1, regs->ARM_r2,
		      regs->ARM_r3, regs->ARM_r4, regs->ARM_r5);
	} else {
		swap_jprobe_return();
	}

	return 0;
}

/**
 * @brief Jprobe return stub.
 *
 * @return Void.
 */
void swap_jprobe_return(void)
{
}
EXPORT_SYMBOL_GPL(swap_jprobe_return);

/**
 * @brief Break handler stub.
 *
 * @param p Pointer to fired kprobe.
 * @param regs Pointer to CPU registers data.
 * @return 0.
 */
int swap_longjmp_break_handler (struct kprobe *p, struct pt_regs *regs)
{
	return 0;
}
EXPORT_SYMBOL_GPL(swap_longjmp_break_handler);

#ifdef CONFIG_STRICT_MEMORY_RWX
#include "memory_rwx.h"

static void write_u32(unsigned long addr, unsigned long val)
{
	mem_rwx_write_u32(addr, val);
}
#else /* CONFIG_STRICT_MEMORY_RWX */
static void write_u32(unsigned long addr, unsigned long val)
{
	*(long *)addr = val;
	flush_icache_range(addr, addr + sizeof(long));
}
#endif /* CONFIG_STRICT_MEMORY_RWX */

/**
 * @brief Arms kprobe.
 *
 * @param p Pointer to target kprobe.
 * @return Void.
 */
void swap_arch_arm_kprobe(struct kprobe *p)
{
	write_u32((long)p->addr, BREAKPOINT_INSTRUCTION);
}

/**
 * @brief Disarms kprobe.
 *
 * @param p Pointer to target kprobe.
 * @return Void.
 */
void swap_arch_disarm_kprobe(struct kprobe *p)
{
	write_u32((long)p->addr, p->opcode);
}

/**
 * @brief Kretprobe trampoline. Provides jumping to probe handler.
 *
 * @return Void.
 */
void __naked swap_kretprobe_trampoline(void)
{
	__asm__ __volatile__ (
		"stmdb	sp!, {r0 - r11}		\n\t"
		"mov	r1, sp			\n\t"
		"mov	r0, #0			\n\t"
		"bl	trampoline_probe_handler\n\t"
		"mov	lr, r0			\n\t"
		"ldmia	sp!, {r0 - r11}		\n\t"
		"bx	lr			\n\t"
		: : : "memory");
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
	unsigned long *ptr_ret_addr;

	/* for __switch_to probe */
	if ((unsigned long)ri->rp->kp.addr == sched_addr) {
		struct thread_info *tinfo = (struct thread_info *)regs->ARM_r2;

		ptr_ret_addr = (unsigned long *)&tinfo->cpu_context.pc;
		ri->sp = NULL;
		ri->task = tinfo->task;
	} else {
		ptr_ret_addr = (unsigned long *)&regs->ARM_lr;
		ri->sp = (unsigned long *)regs->ARM_sp;
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
	unsigned long ret_addr;

	struct pt_regs regs;

	jumper_cb_t cb;
	char data[0];
};

static struct kj_cb_data * __used kjump_handler(struct kj_cb_data *data)
{
	/* call callback */
	data->cb(data->data);

	return data;
}

/**
 * @brief Trampoline for kjump kprobes.
 *
 * @return Void.
 */
void kjump_trampoline(void);
__asm(
	"kjump_trampoline:		\n"

	"mov	r0, r10			\n"
	"bl	kjump_handler		\n"
	"nop				\n"	/* for kjump_kprobe */
);

/**
 * @brief Registers callback for kjump probes.
 *
 * @param regs Pointer to CPU registers data.
 * @param cb Kjump probe callback of jumper_cb_t type.
 * @param data Pointer to data that should be saved in kj_cb_data.
 * @param size Size of the data.
 * @return 0.
 */
int set_kjump_cb(struct pt_regs *regs, jumper_cb_t cb, void *data, size_t size)
{
	struct kprobe *p;
	struct kj_cb_data *cb_data;

	cb_data = kmalloc(sizeof(*cb_data) + size, GFP_ATOMIC);
	if (cb_data == NULL)
		return -ENOMEM;

	p = swap_kprobe_running();
	p->ss_addr[smp_processor_id()] = (kprobe_opcode_t *)&kjump_trampoline;

	cb_data->ret_addr = (unsigned long)p->ainsn.insn;
	cb_data->cb = cb;

	/* save regs */
	memcpy(&cb_data->regs, regs, sizeof(*regs));

	memcpy(cb_data->data, data, size);

	/* save cb_data to r10 */
	regs->ARM_r10 = (long)cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(set_kjump_cb);

static int kjump_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kj_cb_data *data = (struct kj_cb_data *)regs->ARM_r0;

	/* restore regs */
	memcpy(regs, &data->regs, sizeof(*regs));
	p->ss_addr[smp_processor_id()] = (void *)data->ret_addr;

	/* FIXME: potential memory leak, when process kill */
	kfree(data);

	return 0;
}

static struct kprobe kjump_kprobe = {
	.pre_handler = kjump_pre_handler,
	.addr = (unsigned long *)&kjump_trampoline + 2,	/* nop */
};

static int kjump_init(void)
{
	int ret;

	ret = swap_register_kprobe(&kjump_kprobe);
	if (ret)
		printk("ERROR: kjump_init(), ret=%d\n", ret);

	return ret;
}

static void kjump_exit(void)
{
	swap_unregister_kprobe(&kjump_kprobe);
}





/*
 ******************************************************************************
 *                                   jumper                                   *
 ******************************************************************************
 */
struct cb_data {
	unsigned long ret_addr;
	unsigned long r0;

	jumper_cb_t cb;
	char data[0];
};

static unsigned long __used get_r0(struct cb_data *data)
{
	return data->r0;
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

/* FIXME: restore condition flags */

/**
 * @brief Jumper trampoline.
 *
 * @return Void.
 */
void jump_trampoline(void);
__asm(
	"jump_trampoline:		\n"

	"push	{r0 - r12}		\n"
	"mov	r1, r0			\n"	/* data --> r1 */
	"bl	get_r0			\n"
	"str	r0, [sp]		\n"	/* restore r0 */
	"mov	r0, r1			\n"	/* data --> r0 */
	"bl	jump_handler		\n"
	"mov	lr, r0			\n"
	"pop	{r0 - r12}		\n"
	"bx	lr			\n"
);

/**
 * @brief Get jumper address.
 *
 * @return Jumper address.
 */
unsigned long get_jump_addr(void)
{
	return (unsigned long)&jump_trampoline;
}
EXPORT_SYMBOL_GPL(get_jump_addr);

/**
 * @brief Set jumper probe callback.
 *
 * @param ret_addr Jumper probe return address.
 * @param regs Pointer to CPU registers data.
 * @param cb Jumper callback of jumper_cb_t type.
 * @param data Data that should be stored in cb_data.
 * @param size Size of the data.
 * @return 0.
 */
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
	cb_data->r0 = regs->ARM_r0;
	memcpy(cb_data->data, data, size);

	/* save cb_data to r0 */
	regs->ARM_r0 = (long)cb_data;

	return 0;
}
EXPORT_SYMBOL_GPL(set_jump_cb);




/**
 * @brief Registers hook on specified instruction.
 *
 * @param hook Pointer to struct undef_hook.
 * @return Void.
 */
void swap_register_undef_hook(struct undef_hook *hook)
{
	__swap_register_undef_hook(hook);
}
EXPORT_SYMBOL_GPL(swap_register_undef_hook);

/**
 * @brief Unregisters hook.
 *
 * @param hook Pointer to struct undef_hook.
 * @return Void.
 */
void swap_unregister_undef_hook(struct undef_hook *hook)
{
	__swap_unregister_undef_hook(hook);
}
EXPORT_SYMBOL_GPL(swap_unregister_undef_hook);

// kernel probes hook
static struct undef_hook undef_ho_k = {
	.instr_mask	= 0xffffffff,
	.instr_val	= BREAKPOINT_INSTRUCTION,
	.cpsr_mask	= MODE_MASK,
	.cpsr_val	= SVC_MODE,
	.fn		= kprobe_trap_handler
};

/**
 * @brief Arch-dependend module deps initialization stub.
 *
 * @return 0.
 */
int arch_init_module_deps(void)
{
	const char *sym;
#ifdef CONFIG_STRICT_MEMORY_RWX
	int ret;

	ret = mem_rwx_once();
	if (ret)
		return ret;
#endif /* CONFIG_STRICT_MEMORY_RWX */

	sym = "register_undef_hook";
	__swap_register_undef_hook = (void *)swap_ksyms(sym);
	if (__swap_register_undef_hook == NULL)
		goto not_found;

	sym = "unregister_undef_hook";
	__swap_unregister_undef_hook = (void *)swap_ksyms(sym);
	if (__swap_unregister_undef_hook == NULL)
		goto not_found;

	return 0;

not_found:
	printk("ERROR: symbol '%s' not found\n", sym);
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

	swap_register_undef_hook(&undef_ho_k);

	ret = kjump_init();
	if (ret) {
		swap_unregister_undef_hook(&undef_ho_k);
		return ret;
	}

	return 0;
}

/**
 * @brief Uninitializes kprobe module.
 *
 * @return Void.
 */
void swap_arch_exit_kprobes(void)
{
	kjump_exit();
	swap_unregister_undef_hook(&undef_ho_k);
}

/* export symbol for trampoline_arm.h */
EXPORT_SYMBOL_GPL(gen_insn_execbuf);
EXPORT_SYMBOL_GPL(pc_dep_insn_execbuf);
EXPORT_SYMBOL_GPL(b_r_insn_execbuf);
EXPORT_SYMBOL_GPL(b_cond_insn_execbuf);
EXPORT_SYMBOL_GPL(blx_off_insn_execbuf);
