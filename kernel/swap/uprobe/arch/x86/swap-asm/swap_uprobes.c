/**
 * uprobe/arch/asm-x86/swap_uprobes.c
 * @author Alexey Gerenkov <a.gerenkov@samsung.com> User-Space Probes initial
 * implementation; Support x86/ARM/MIPS for both user and kernel spaces.
 * @author Ekaterina Gorelkina <e.gorelkina@samsung.com>: redesign module for
 * separating core and arch parts
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
 * Copyright (C) Samsung Electronics, 2006-2010
 *
 * @section DESCRIPTION
 *
 * Arch-dependent uprobe interface implementation for x86.
 */


#include <linux/kdebug.h>

#include <kprobe/swap_slots.h>
#include <uprobe/swap_uprobes.h>

#include "swap_uprobes.h"


/**
 * @struct uprobe_ctlblk
 * @brief Uprobe control block
 */
struct uprobe_ctlblk {
        unsigned long flags;            /**< Flags */
        struct kprobe *p;               /**< Pointer to the uprobe's kprobe */
};

static unsigned long trampoline_addr(struct uprobe *up)
{
	return (unsigned long)(up->kp.ainsn.insn +
			       UPROBES_TRAMP_RET_BREAK_IDX);
}

static DEFINE_PER_CPU(struct uprobe_ctlblk, ucb) = { 0, NULL };

static struct kprobe *get_current_probe(void)
{
	return __get_cpu_var(ucb).p;
}

static void set_current_probe(struct kprobe *p)
{
	__get_cpu_var(ucb).p = p;
}

static void reset_current_probe(void)
{
	set_current_probe(NULL);
}

static void save_current_flags(struct pt_regs *regs)
{
	__get_cpu_var(ucb).flags = regs->EREG(flags);
}

static void restore_current_flags(struct pt_regs *regs)
{
	regs->EREG(flags) &= ~IF_MASK;
	regs->EREG(flags) |= __get_cpu_var(ucb).flags & IF_MASK;
}

/**
 * @brief Prepares uprobe for x86.
 *
 * @param up Pointer to the uprobe.
 * @return 0 on success,\n
 * -1 on error.
 */
int arch_prepare_uprobe(struct uprobe *up)
{
	int ret = 0;
	struct kprobe *p = up2kp(up);
	struct task_struct *task = up->task;
	u8 *tramp = up->atramp.tramp;
	enum { call_relative_opcode = 0xe8 };

	if (!read_proc_vm_atomic(task, (unsigned long)p->addr,
				 tramp, MAX_INSN_SIZE))
		panic("failed to read memory %p!\n", p->addr);
	/* TODO: this is a workaround */
	if (tramp[0] == call_relative_opcode) {
		printk("cannot install probe: 1st instruction is call\n");
		return -1;
	}

	tramp[UPROBES_TRAMP_RET_BREAK_IDX] = BREAKPOINT_INSTRUCTION;

	/* TODO: remove dual info */
	p->opcode = tramp[0];

	p->ainsn.boostable = swap_can_boost(tramp) ? 0 : -1;

	return ret;
}

/**
 * @brief Jump pre-handler.
 *
 * @param p Pointer to the uprobe's kprobe.
 * @param regs Pointer to CPU register data.
 * @return 0.
 */
int setjmp_upre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct uprobe *up = container_of(p, struct uprobe, kp);
	struct ujprobe *jp = container_of(up, struct ujprobe, up);
	kprobe_pre_entry_handler_t pre_entry = (kprobe_pre_entry_handler_t)jp->pre_entry;
	entry_point_t entry = (entry_point_t)jp->entry;
	unsigned long args[6];

	/* FIXME some user space apps crash if we clean interrupt bit */
	//regs->EREG(flags) &= ~IF_MASK;
	trace_hardirqs_off();

	/* read first 6 args from stack */
	if (!read_proc_vm_atomic(current, regs->EREG(sp) + 4, args, sizeof(args)))
		panic("failed to read user space func arguments %lx!\n", regs->EREG(sp) + 4);

	if (pre_entry)
		p->ss_addr[smp_processor_id()] = (kprobe_opcode_t *)
						 pre_entry(jp->priv_arg, regs);

	if (entry)
		entry(args[0], args[1], args[2], args[3], args[4], args[5]);
	else
		arch_ujprobe_return();

	return 0;
}

/**
 * @brief Prepares uretprobe for x86.
 *
 * @param ri Pointer to the uretprobe instance.
 * @param regs Pointer to CPU register data.
 * @return Void.
 */
void arch_prepare_uretprobe(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	/* Replace the return addr with trampoline addr */
	unsigned long ra = trampoline_addr(&ri->rp->up);
	ri->sp = (kprobe_opcode_t *)regs->sp;

	if (!read_proc_vm_atomic(current, regs->EREG(sp), &(ri->ret_addr), sizeof(ri->ret_addr)))
		panic("failed to read user space func ra %lx!\n", regs->EREG(sp));

	if (!write_proc_vm_atomic(current, regs->EREG(sp), &ra, sizeof(ra)))
		panic("failed to write user space func ra %lx!\n", regs->EREG(sp));
}

/**
 * @brief Disarms uretprobe on x86 arch.
 *
 * @param ri Pointer to the uretprobe instance.
 * @param task Pointer to the task for which the probe.
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task)
{
	int len;
	unsigned long ret_addr;
	unsigned long sp = (unsigned long)ri->sp;
	unsigned long tramp_addr = trampoline_addr(&ri->rp->up);
	len = read_proc_vm_atomic(task, sp, &ret_addr, sizeof(ret_addr));
	if (len != sizeof(ret_addr)) {
		printk("---> %s (%d/%d): failed to read stack from %08lx\n",
		       task->comm, task->tgid, task->pid, sp);
		return -EFAULT;
	}

	if (tramp_addr == ret_addr) {
		len = write_proc_vm_atomic(task, sp, &ri->ret_addr,
					   sizeof(ri->ret_addr));
		if (len != sizeof(ri->ret_addr)) {
			printk("---> %s (%d/%d): failed to write "
			       "orig_ret_addr to %08lx",
			       task->comm, task->tgid, task->pid, sp);
			return -EFAULT;
		}
	} else {
		printk("---> %s (%d/%d): trampoline NOT found at sp = %08lx\n",
		       task->comm, task->tgid, task->pid, sp);
		return -ENOENT;
	}

	return 0;
}

/**
 * @brief Gets trampoline address.
 *
 * @param p Pointer to the uprobe's kprobe.
 * @param regs Pointer to CPU register data.
 * @return Trampoline address.
 */
unsigned long arch_get_trampoline_addr(struct kprobe *p, struct pt_regs *regs)
{
	return trampoline_addr(kp2up(p));
}

/**
 * @brief Restores return address.
 *
 * @param orig_ret_addr Original return address.
 * @param regs Pointer to CPU register data.
 * @return Void.
 */
void arch_set_orig_ret_addr(unsigned long orig_ret_addr, struct pt_regs *regs)
{
	regs->EREG(ip) = orig_ret_addr;
}

/**
 * @brief Removes uprobe.
 *
 * @param up Pointer to the target uprobe.
 * @return Void.
 */
void arch_remove_uprobe(struct uprobe *up)
{
	struct kprobe *p = up2kp(up);

	swap_slot_free(up->sm, p->ainsn.insn);
}

static void set_user_jmp_op(void *from, void *to)
{
	struct __arch_jmp_op
	{
		char op;
		long raddr;
	} __attribute__ ((packed)) jop;

	jop.raddr = (long)(to) - ((long)(from) + 5);
	jop.op = RELATIVEJUMP_INSTRUCTION;

	if (!write_proc_vm_atomic(current, (unsigned long)from, &jop, sizeof(jop)))
		panic("failed to write jump opcode to user space %p!\n", from);
}

static void resume_execution(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
	unsigned long *tos, tos_dword = 0;
	unsigned long copy_eip = (unsigned long)p->ainsn.insn;
	unsigned long orig_eip = (unsigned long)p->addr;
	kprobe_opcode_t insns[2];

	regs->EREG(flags) &= ~TF_MASK;

	tos = (unsigned long *)&tos_dword;
	if (!read_proc_vm_atomic(current, regs->EREG(sp), &tos_dword, sizeof(tos_dword)))
		panic("failed to read dword from top of the user space stack %lx!\n", regs->EREG(sp));

	if (!read_proc_vm_atomic(current, (unsigned long)p->ainsn.insn, insns, 2 * sizeof(kprobe_opcode_t)))
		panic("failed to read first 2 opcodes of instruction copy from user space %p!\n", p->ainsn.insn);

	switch (insns[0]) {
		case 0x9c:		/* pushfl */
			*tos &= ~(TF_MASK | IF_MASK);
			*tos |= flags & (TF_MASK | IF_MASK);
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

			if (!write_proc_vm_atomic(current, regs->EREG (sp), &tos_dword, sizeof(tos_dword)))
				panic("failed to write dword to top of the user space stack %lx!\n", regs->EREG (sp));

			goto no_change;
		case 0xff:
			if ((insns[1] & 0x30) == 0x10) {
				/*
				 * call absolute, indirect
				 * Fix return addr; eip is correct.
				 * But this is not boostable
				 */
				*tos = orig_eip + (*tos - copy_eip);

				if (!write_proc_vm_atomic(current, regs->EREG(sp), &tos_dword, sizeof(tos_dword)))
					panic("failed to write dword to top of the user space stack %lx!\n", regs->EREG(sp));

				goto no_change;
			} else if (((insns[1] & 0x31) == 0x20) || /* jmp near, absolute indirect */
				   ((insns[1] & 0x31) == 0x21)) {
				/* jmp far, absolute indirect */
				/* eip is correct. And this is boostable */
				p->ainsn.boostable = 1;
				goto no_change;
			}
		case 0xf3:
			if (insns[1] == 0xc3)
				/* repz ret special handling: no more changes */
				goto no_change;
			break;
		default:
			break;
	}

	if (!write_proc_vm_atomic(current, regs->EREG(sp), &tos_dword, sizeof(tos_dword)))
		panic("failed to write dword to top of the user space stack %lx!\n", regs->EREG(sp));

	if (p->ainsn.boostable == 0) {
		if ((regs->EREG(ip) > copy_eip) && (regs->EREG(ip) - copy_eip) + 5 < MAX_INSN_SIZE) {
			/*
			 * These instructions can be executed directly if it
			 * jumps back to correct address.
			 */
			set_user_jmp_op((void *) regs->EREG(ip), (void *)orig_eip + (regs->EREG(ip) - copy_eip));
			p->ainsn.boostable = 1;
		} else {
			p->ainsn.boostable = -1;
		}
	}

	regs->EREG(ip) = orig_eip + (regs->EREG(ip) - copy_eip);

no_change:
	return;
}

static int make_trampoline(struct uprobe *up)
{
	struct kprobe *p = up2kp(up);
	struct task_struct *task = up->task;
	void *tramp;

	tramp = swap_slot_alloc(up->sm);
	if (tramp == 0) {
		printk("trampoline out of memory\n");
		return -ENOMEM;
	}

	if (!write_proc_vm_atomic(task, (unsigned long)tramp,
				  up->atramp.tramp,
				  sizeof(up->atramp.tramp))) {
		swap_slot_free(up->sm, tramp);
		panic("failed to write memory %p!\n", tramp);
		return -EINVAL;
	}

	p->ainsn.insn = tramp;

	return 0;
}

static int uprobe_handler(struct pt_regs *regs)
{
	struct kprobe *p;
	kprobe_opcode_t *addr;
	struct task_struct *task = current;
	pid_t tgid = task->tgid;

	save_current_flags(regs);

	addr = (kprobe_opcode_t *)(regs->EREG(ip) - sizeof(kprobe_opcode_t));
	p = get_ukprobe(addr, tgid);

	if (p == NULL) {
		void *tramp_addr = (void *)addr - UPROBES_TRAMP_RET_BREAK_IDX;

		p = get_ukprobe_by_insn_slot(tramp_addr, tgid, regs);
		if (p == NULL) {
			printk("no_uprobe\n");
			return 0;
		}

		trampoline_uprobe_handler(p, regs);
		return 1;
	} else {
		if (p->ainsn.insn == NULL) {
			struct uprobe *up = kp2up(p);

			make_trampoline(up);

			/* for uretprobe */
			add_uprobe_table(p);
		}

		if (!p->pre_handler || !p->pre_handler(p, regs)) {
			if (p->ainsn.boostable == 1 && !p->post_handler) {
				regs->EREG(ip) = (unsigned long)p->ainsn.insn;
				return 1;
			}

			prepare_singlestep(p, regs);
		}
	}

	set_current_probe(p);

	return 1;
}

static int post_uprobe_handler(struct pt_regs *regs)
{
	struct kprobe *p = get_current_probe();
	unsigned long flags = __get_cpu_var(ucb).flags;

	if (p == NULL)
		return 0;

	resume_execution(p, regs, flags);
	restore_current_flags(regs);

	reset_current_probe();

	return 1;
}

static int uprobe_exceptions_notify(struct notifier_block *self, unsigned long val, void *data)
{
	struct die_args *args = (struct die_args *)data;
	int ret = NOTIFY_DONE;

	if (args->regs == NULL || !user_mode_vm(args->regs))
		return ret;

	switch (val) {
#ifdef CONFIG_KPROBES
		case DIE_INT3:
#else
		case DIE_TRAP:
#endif
			if (uprobe_handler(args->regs))
				ret = NOTIFY_STOP;
			break;
		case DIE_DEBUG:
			if (post_uprobe_handler(args->regs))
				ret = NOTIFY_STOP;
			break;
		default:
			break;
	}

	return ret;
}

static struct notifier_block uprobe_exceptions_nb = {
	.notifier_call = uprobe_exceptions_notify,
	.priority = INT_MAX
};

/**
 * @brief Registers notify.
 *
 * @return register_die_notifier result.
 */
int swap_arch_init_uprobes(void)
{
	return register_die_notifier(&uprobe_exceptions_nb);
}

/**
 * @brief Unregisters notify.
 *
 * @return Void.
 */
void swap_arch_exit_uprobes(void)
{
	unregister_die_notifier(&uprobe_exceptions_nb);
}

