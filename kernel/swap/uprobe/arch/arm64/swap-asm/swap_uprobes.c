/*
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
 * Copyright (C) Samsung Electronics, 2014
 *
 * 2014         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/types.h>
#include <asm/traps.h>
#include <uprobe/swap_uprobes.h>
#include <kprobe/swap_slots.h>
#include <kprobe/swap_td_raw.h>
#include <kprobe/swap_kprobes_deps.h>	/* FIXME: remove it */
#include <swap-asm/swap_probes.h>
#include <arch/arm/uprobe/swap_uprobe.h>
#include <swap-asm/dbg_interface.h>
#include "uprobes-arm64.h"


#define BRK_BP			0x45
#define BRK_PSEUDO_SS		0x54
#define BRK_URP			0x67
#define BRK64_OPCODE_BP		MAKE_BRK(BRK_BP)
#define BRK64_OPCODE_PSEUDO_SS	MAKE_BRK(BRK_PSEUDO_SS)
#define BRK64_OPCODE_URP	MAKE_BRK(BRK_URP)


enum arch_mode {
	AM_UNKNOWN,
	AM_THUMB,
	AM_ARM,
	AM_ARM64
};

#define ARM64_MODE_VADDR_MASK		((unsigned long)1 << 63)

static enum arch_mode get_arch_mode(unsigned long vaddr)
{
	if (vaddr & 1)
		return AM_THUMB;

	if (vaddr & ARM64_MODE_VADDR_MASK)
		return AM_ARM64;

	return AM_ARM;
}

static unsigned long get_real_addr(unsigned long vaddr)
{
	return vaddr & ~(ARM64_MODE_VADDR_MASK | 1);
}


struct uprobe_ctlblk {
	struct uprobe *p;
};

static struct td_raw td_raw;

static struct uprobe_ctlblk *current_ctlblk(void)
{
	return (struct uprobe_ctlblk *)swap_td_raw(&td_raw, current);
}

static struct uprobe *get_current_uprobe(void)
{
	return current_ctlblk()->p;
}

static void set_current_uprobe(struct uprobe *p)
{
	current_ctlblk()->p = p;
}

static void reset_current_uprobe(void)
{
	set_current_uprobe(NULL);
}


static int prepare_uretprobe_arm64(struct uretprobe_instance *ri,
				   struct pt_regs *regs)
{
	unsigned long bp_addr = (unsigned long)(ri->rp->up.insn +
						URP_RET_BREAK_IDX);
	unsigned long ret_addr = regs->regs[30] | ARM64_MODE_VADDR_MASK;

	ri->sp = (kprobe_opcode_t *)regs->sp;
	ri->ret_addr = (kprobe_opcode_t *)ret_addr;

	/* replace the return address (regs[30] - lr) */
	regs->regs[30] = bp_addr;

	return 0;
}

int arch_prepare_uretprobe(struct uretprobe_instance *ri,
			   struct pt_regs *regs)
{
	if (get_arch_mode((unsigned long)ri->rp->up.addr) == AM_ARM64)
		return prepare_uretprobe_arm64(ri, regs);
	else
		return prepare_uretprobe_arm(ri, regs);
}

static void arch_opcode_analysis_uretprobe_arm64(struct uretprobe *rp)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
}

void arch_opcode_analysis_uretprobe(struct uretprobe *rp)
{
	if (get_arch_mode((unsigned long)rp->up.addr) == AM_ARM64)
		arch_opcode_analysis_uretprobe_arm64(rp);
	else
		arch_opcode_analysis_uretprobe_arm(rp);
}

static unsigned long arch_get_trampoline_addr_arm64(struct uprobe *p,
						    struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

/**
 * @brief Gets trampoline address.
 *
 * @param p Pointer to the kprobe.
 * @param regs Pointer to CPU register data.
 * @return Trampoline address.
 */
unsigned long arch_get_trampoline_addr(struct uprobe *p, struct pt_regs *regs)
{
	if (get_arch_mode((unsigned long)p->addr) == AM_ARM64)
		return arch_get_trampoline_addr_arm64(p, regs);
	else
		return arch_get_trampoline_addr_arm(p, regs);
}

static unsigned long arch_tramp_by_ri_arm64(struct uretprobe_instance *ri)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

unsigned long arch_tramp_by_ri(struct uretprobe_instance *ri)
{
	if (get_arch_mode((unsigned long)ri->rp->up.addr) == AM_ARM64)
		return arch_tramp_by_ri_arm64(ri);
	else
		return arch_tramp_by_ri_arm(ri);
}

static int arch_disarm_urp_inst_arm64(struct uretprobe_instance *ri,
				      struct task_struct *task)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

/**
 * @brief Disarms uretprobe instance.
 *
 * @param ri Pointer to the uretprobe instance
 * @param task Pointer to the task for which the uretprobe instance
 * @return 0 on success,\n
 * negative error code on error.
 */
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task)
{
	if (get_arch_mode((unsigned long)ri->rp->up.addr) == AM_ARM64)
		return arch_disarm_urp_inst_arm64(ri, task);
	else
		return arch_disarm_urp_inst_arm(ri, task);
}

void arch_set_orig_ret_addr(unsigned long orig_ret_addr, struct pt_regs *regs)
{
	if (get_arch_mode(orig_ret_addr) == AM_ARM64) {
		regs->pc = get_real_addr(orig_ret_addr);
	} else {
		set_orig_ret_addr_arm(orig_ret_addr, regs);
	}
}

static int make_urp_arm64(struct uprobe *p)
{
	u32 *utramp, urp_brk;

	if (p->insn == NULL) {
		utramp = swap_slot_alloc(p->sm);
		if (utramp == NULL)
			return -ENOMEM;

		p->insn = utramp;
	}

	utramp = p->insn;
	urp_brk = BRK64_OPCODE_URP;

	if (!write_proc_vm_atomic(p->task,
				  (long)&utramp[URP_RET_BREAK_IDX],
				  &urp_brk, sizeof(urp_brk))) {
		pr_err("%s failed to write memory %p!\n", __func__, utramp);
	}

	return 0;
}

static __maybe_unused enum dbg_code urp_handler(struct pt_regs *regs, unsigned int esr)
{
	struct uprobe *p;
	unsigned long pc = regs->pc;
	unsigned long insn_addr = pc - sizeof(u32) * URP_RET_BREAK_IDX;

	p = get_uprobe_by_insn_slot((void *)insn_addr, current->tgid, regs);
	if (p == NULL) {
		pr_err("no_uretprobe: Not one of ours: let "
		       "kernel handle it %lx\n", pc);
		return DBG_ERROR;
	}

	local_irq_enable();
	trampoline_uprobe_handler(p, regs);

	return DBG_HANDLED;
}

static int arch_arm_uprobe_arm64(struct uprobe *p)
{
	int ret;
	unsigned long vaddr = (unsigned long)p->addr;
	unsigned long raddr = get_real_addr(vaddr);
	u32 insn = BRK64_OPCODE_BP;

	ret = write_proc_vm_atomic(p->task, raddr, &insn, 4);
	if (!ret) {
		pr_err("failed to write memory addr=%lx\n", vaddr);
		return -EACCES;
	}

	return 0;
}

static void arch_disarm_uprobe_arm64(struct uprobe *p,
				     struct task_struct *task)
{
	unsigned long vaddr = (unsigned long)p->addr;
	unsigned long raddr = get_real_addr(vaddr);
	int ret;

	ret = write_proc_vm_atomic(task, raddr, &p->opcode, 4);
	if (!ret)
		pr_err("failed to write memory vaddr=%lx\n", vaddr);
}

int arch_arm_uprobe(struct uprobe *p)
{
	int ret;
	unsigned long vaddr = (unsigned long)p->addr;

	switch (get_arch_mode(vaddr)) {
	case AM_THUMB:
	case AM_ARM:
		ret = arch_arm_uprobe_arm(p);
		break;
	case AM_ARM64:
		ret = arch_arm_uprobe_arm64(p);
		break;
	default:
		pr_err("Error: unknown mode vaddr=%lx\n", vaddr);
		return -EINVAL;
	}

	return ret;
}

void arch_disarm_uprobe(struct uprobe *p, struct task_struct *task)
{
	unsigned long vaddr = (unsigned long)p->addr;

	switch (get_arch_mode(vaddr)) {
	case AM_THUMB:
	case AM_ARM:
		arch_disarm_uprobe_arm(p, task);
		break;
	case AM_ARM64:
		arch_disarm_uprobe_arm64(p, task);
		break;
	default:
		pr_err("Error: unknown mode vaddr=%lx\n", vaddr);
		break;
	}
}


static void arch_prepare_simulate_arm64(struct uprobe *p)
{
	if (p->ainsn.prepare)
		p->ainsn.prepare(p, &p->ainsn);
}

static void arch_prepare_ss_arm64(struct uprobe *p)
{
	/* prepare insn slot */
	p->ainsn.tramp_arm64[0] = p->opcode;
	p->ainsn.tramp_arm64[1] = BRK64_OPCODE_PSEUDO_SS;

}

static int arch_prepare_uprobe_arm64(struct uprobe *p)
{
	struct task_struct *task = p->task;
	unsigned long vaddr = (unsigned long)p->addr;
	unsigned long raddr = get_real_addr(vaddr);
	u32 insn;

	if (!read_proc_vm_atomic(task, raddr, &insn, sizeof(insn))) {
		pr_err("failed to read memory %lx!\n", raddr);
		return -EINVAL;
	}

	p->ainsn.matrioshka_flags = 0;

	switch (arm64_uprobe_decode_insn(insn, &p->ainsn)) {
	case INSN_REJECTED:	/* insn not supported */
		return -EINVAL;

	case INSN_GOOD_NO_SLOT:	/* insn need simulation */
		p->insn = NULL;
		p->ainsn.matrioshka_flags |= MF_ARM64_SIMUL;
		arch_prepare_simulate_arm64(p);
		break;

	case INSN_GOOD:		/* instruction uses slot */
		p->insn = NULL;
		p->ainsn.matrioshka_flags = MF_ARM64_EMUL;

		arch_prepare_ss_arm64(p);
		break;
	};

	return 0;
}

int arch_prepare_uprobe(struct uprobe *p)
{
	int ret;

	if (get_arch_mode((unsigned long)p->addr) == AM_ARM64) {
		ret = arch_prepare_uprobe_arm64(p);
	} else {
		ret = arch_prepare_uprobe_arm(p);
	}

	if (!ret) {
		/* for uretprobe */
		add_uprobe_table(p);
	}

	return ret;
}

void arch_remove_uprobe(struct uprobe *p)
{
	swap_slot_free(p->sm, p->insn);
}

static void simulate_insn_arm64(struct uprobe *p, struct pt_regs *regs)
{
	if (p->ainsn.handler)
		p->ainsn.handler(p->opcode, (long)p->addr, regs);

	reset_current_uprobe();
}

static void setup_ss_arm64(struct uprobe *p, struct pt_regs *regs)
{
	/* set trampoline */
	regs->pc = (u64)p->insn;

	set_current_uprobe(p);
}

static void setup_matrioshka_arm64(struct uprobe *p, struct pt_regs *regs)
{
	int m;

	m = p->ainsn.matrioshka_flags & MF_ARM64_MASK;
	switch (m) {
	case MF_ARM64_SIMUL:
		simulate_insn_arm64(p, regs);
		break;
	case MF_ARM64_EMUL:
		setup_ss_arm64(p, regs);
		break;
	default:
		pr_err("ERROR: unknown matrioshka mode(%x) for ARM64\n", m);
	}
}

static int make_trampoline_arm64(struct uprobe *p, struct pt_regs *regs)
{
	u32 *tramp, *utramp;

	tramp = p->ainsn.tramp_arm64;

	utramp = swap_slot_alloc(p->sm);
	if (utramp == NULL) {
		pr_err("ERROR: cannot allocate trampoline\n");
		return -ENOMEM;
	}

	if (!write_proc_vm_atomic(p->task, (unsigned long)utramp, tramp,
				  UPROBES_TRAMP_LEN))
		pr_err("failed to write memory %p!\n", utramp);

	p->insn = utramp;

	return 0;
}

static int make_matrioshka_arm64(struct uprobe *p, struct pt_regs *regs)
{
	int m, ret = 0;

	m = p->ainsn.matrioshka_flags & MF_ARM64_MASK;
	switch (m) {
	case MF_ARM64_SIMUL:
		break;
	case MF_ARM64_EMUL:
		ret = make_trampoline_arm64(p, regs);
		if (ret)
			disarm_uprobe(p, p->task);
		break;
	default:
		pr_err("ERROR: we are in arm64 mode "
		       "(!) and check instruction was fail "
		       "(%x instruction at %p address)!\n",
		       p->opcode, p->addr);

		disarm_uprobe(p, p->task);
		return -EINVAL;
	}

	return ret;
}

static enum dbg_code uprobe_handler_compat(struct pt_regs *regs)
{
	pr_err("ARM and THUMB modes not supported\n");
	return DBG_ERROR;
}

static enum dbg_code uprobe_handler_arm64(struct pt_regs *regs)
{
	struct uprobe *p;

	p = get_uprobe((void *)regs->pc, current->tgid);
	if (p) {
		if (!(p->ainsn.matrioshka_flags & MF_SET)) {
			p->ainsn.matrioshka_flags |= MF_SET;
			if (make_matrioshka_arm64(p, regs)) {
				pr_err("no_uprobe live\n");
				goto out_ok;
			}

			if (make_urp_arm64(p)) {
				disarm_uprobe(p, p->task);
				pr_err("no_uretprobe\n");
				goto out_ok;
			}
		}

		if (!p->pre_handler || !p->pre_handler(p, regs))
			setup_matrioshka_arm64(p, regs);
	} else {
		return DBG_ERROR;
	}

out_ok:
	return DBG_HANDLED;
}

static __maybe_unused enum dbg_code uprobe_handler(struct pt_regs *regs, unsigned int esr)
{
	local_irq_enable();

	return compat_user_mode(regs) ?
			uprobe_handler_compat(regs) :
			uprobe_handler_arm64(regs);
}

static __maybe_unused enum dbg_code uprobe_ss_handler(struct pt_regs *regs, unsigned int esr)
{
	struct uprobe *p;

	p = get_current_uprobe();
	if (p) {
		regs->pc = (u64)p->addr + 4;
		reset_current_uprobe();
	}

	return DBG_HANDLED;
}


static __maybe_unused struct brk_hook dbg_up_bp = {
	.spsr_mask = PSR_MODE_MASK,
	.spsr_val = PSR_MODE_EL0t,
	.esr_mask = DBG_BRK_ESR_MASK,
	.esr_val = DBG_BRK_ESR(BRK_BP),
	.fn = uprobe_handler,
};

static __maybe_unused struct brk_hook dbg_up_ss = {
	.spsr_mask = PSR_MODE_MASK,
	.spsr_val = PSR_MODE_EL0t,
	.esr_mask = DBG_BRK_ESR_MASK,
	.esr_val = DBG_BRK_ESR(BRK_PSEUDO_SS),
	.fn = uprobe_ss_handler,
};

static __maybe_unused struct brk_hook dbg_urp_bp = {
	.spsr_mask = PSR_MODE_MASK,
	.spsr_val = PSR_MODE_EL0t,
	.esr_mask = DBG_BRK_ESR_MASK,
	.esr_val = DBG_BRK_ESR(BRK_URP),
	.fn = urp_handler,
};


static void arch_prepare_singlestep(struct uprobe *p, struct pt_regs *regs)
{
	if (p->ainsn.insn.handler) {
		regs->pc += 4;
		p->ainsn.insn.handler(p->opcode, &p->ainsn.insn, regs);
	} else {
		regs->pc = (unsigned long)p->insn;
	}
}

static int urp_handler_aarch32(struct pt_regs *regs, pid_t tgid)
{
	struct uprobe *p;
	unsigned long vaddr = regs->pc;
	unsigned long offset_bp = compat_thumb_mode(regs) ?
				  0x1a :
				  4 * PROBES_TRAMP_RET_BREAK_IDX;
	unsigned long tramp_addr = vaddr - offset_bp;
	unsigned long flags;

	local_irq_save(flags);
	p = get_uprobe_by_insn_slot((void *)tramp_addr, tgid, regs);
	if (unlikely(p == NULL)) {
		local_irq_restore(flags);

		pr_info("no_uprobe: Not one of ours: let kernel handle it %lx\n",
			vaddr);
		return 1;
	}

	get_up(p);
	local_irq_restore(flags);
	trampoline_uprobe_handler(p, regs);
	put_up(p);

	return 0;
}

static int uprobe_handler_aarch32(struct pt_regs *regs, u32 instr)
{
	int ret = 0;
	struct uprobe *p;
	unsigned long flags;
	unsigned long vaddr = regs->pc | !!compat_thumb_mode(regs);
	pid_t tgid = current->tgid;

	local_irq_enable();

	local_irq_save(flags);
	p = get_uprobe((uprobe_opcode_t *)vaddr, tgid);
	if (p) {
		get_up(p);
		local_irq_restore(flags);

		if (!p->pre_handler || !p->pre_handler(p, regs))
			arch_prepare_singlestep(p, regs);

		put_up(p);
	} else {
		local_irq_restore(flags);
		ret = urp_handler_aarch32(regs, tgid);

		/* check ARM/THUMB CPU mode matches installed probe mode */
		if (ret == 1) {
			vaddr ^= 1;

			local_irq_save(flags);
			p = get_uprobe((uprobe_opcode_t *)vaddr, tgid);
			if (p) {
				get_up(p);
				local_irq_restore(flags);
				pr_err("invalid mode: thumb=%d addr=%p insn=%08x\n",
				       !!compat_thumb_mode(regs), p->addr, p->opcode);
				ret = 0;

				disarm_uprobe(p, current);
				put_up(p);
			} else {
				local_irq_restore(flags);
			}
		}
	}

	return ret;
}


static void (*__register_undef_hook)(struct undef_hook *hook);
static void (*__unregister_undef_hook)(struct undef_hook *hook);

static int undef_hook_once(void)
{
	const char *sym;

	sym = "register_undef_hook";
	__register_undef_hook = (void *)swap_ksyms(sym);
	if (__register_undef_hook == NULL)
		goto not_found;

	sym = "unregister_undef_hook";
	__unregister_undef_hook = (void *)swap_ksyms(sym);
	if (__unregister_undef_hook == NULL)
		goto not_found;

	return 0;

not_found:
	pr_err("ERROR: symbol '%s' not found\n", sym);
	return -ESRCH;

}

static struct undef_hook undef_hook_arm = {
	.instr_mask = 0xffffffff,
	.instr_val = BREAK_ARM,
	.pstate_mask = COMPAT_PSR_MODE_MASK,
	.pstate_val = COMPAT_PSR_MODE_USR,
	.fn = uprobe_handler_aarch32,
};

static struct undef_hook undef_hook_thumb = {
	.instr_mask = 0xffff,
	.instr_val = BREAK_THUMB,
	.pstate_mask = COMPAT_PSR_MODE_MASK,
	.pstate_val = COMPAT_PSR_MODE_USR,
	.fn = uprobe_handler_aarch32,
};

int swap_arch_init_uprobes(void)
{
	int ret;

	ret = undef_hook_once();
	if (ret)
		return ret;

	ret = swap_td_raw_reg(&td_raw, sizeof(struct uprobe_ctlblk));
	if (ret)
		return ret;

#if 0
	/* for aarch64 */
	dbg_brk_hook_reg(&dbg_up_ss);
	dbg_brk_hook_reg(&dbg_up_bp);
	dbg_brk_hook_reg(&dbg_urp_bp);
#endif

	/* for aarch32 */
	__register_undef_hook(&undef_hook_arm);
	__register_undef_hook(&undef_hook_thumb);

	return 0;
}

void swap_arch_exit_uprobes(void)
{
	/* for aarch32 */
	__unregister_undef_hook(&undef_hook_thumb);
	__unregister_undef_hook(&undef_hook_arm);

#if 0
	/* for aarch64 */
	dbg_brk_hook_unreg(&dbg_urp_bp);
	dbg_brk_hook_unreg(&dbg_up_bp);
	dbg_brk_hook_unreg(&dbg_up_ss);
#endif

	swap_td_raw_unreg(&td_raw);
}
