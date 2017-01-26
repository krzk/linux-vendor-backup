#ifndef _ASM_ARM64_UPROBES_H
#define _ASM_ARM64_UPROBES_H

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
#include <linux/ptrace.h>
#include <arch/arm/probes/probes.h>
#include <arch/arm/uprobe/swap_uprobe.h>


#define UP_TRAMP_INSN_CNT	3 /* | opcode | ss_bp | urp_bp | */
#define UPROBES_TRAMP_LEN	UP_TRAMP_INSN_CNT * sizeof(u32)
#define URP_RET_BREAK_IDX	2


struct uprobe;
struct arch_insn;
struct uretprobe;
struct task_struct;
struct uretprobe_instance;


typedef unsigned long (uprobes_pstate_check_t)(unsigned long);
typedef unsigned long (uprobes_condition_check_t)(struct uprobe *p,
						  struct pt_regs *);
typedef void (uprobes_prepare_t)(struct uprobe *, struct arch_insn *);
typedef void (uprobes_handler_t)(u32 opcode, long addr, struct pt_regs *);


enum {
	MF_ARM64_SIMUL	= 1 << 0,
	MF_ARM64_EMUL	= 1 << 1,
	MF_ARM64_MASK	= MF_ARM64_SIMUL | MF_ARM64_EMUL,

	MF_ARM_SIMUL	= 1 << 2,
	MF_ARM_EMUL	= 1 << 3,
	MF_ARM_MASK	= MF_ARM_SIMUL | MF_ARM_EMUL,

	MF_THUMB_SIMUL	= 1 << 4,
	MF_THUMB_EMUL	= 1 << 5,
	MF_THUMB_MASK	= MF_THUMB_SIMUL | MF_THUMB_SIMUL,

	MF_SET		= 1 << 6
};


struct arch_insn {
	/* arm */
	struct arch_insn_arm insn;

	/* arm64 */
	unsigned long matrioshka_flags;
	uprobes_pstate_check_t *pstate_cc;
	uprobes_condition_check_t *check_condn;
	uprobes_prepare_t *prepare;
	uprobes_handler_t *handler;

	u32 tramp_arm64[UP_TRAMP_INSN_CNT];
};


typedef u32 uprobe_opcode_t;

#define thumb_mode(regs)	compat_thumb_mode(regs)

static inline u32 swap_get_urp_float(struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

static inline u64 swap_get_urp_double(struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

static inline unsigned long swap_get_uarg(struct pt_regs *regs, unsigned long n)
{
	if (compat_user_mode(regs)) {
		return swap_get_uarg_arm(regs, n);
	} else {
		WARN(1, "not implemented"); /* FIXME: to implement */
		return 0x12345678;
	}
}

static inline void swap_put_uarg(struct pt_regs *regs, unsigned long n,
				 unsigned long val)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
}

int arch_prepare_uprobe(struct uprobe *p);
void arch_remove_uprobe(struct uprobe *p);

static inline int setjmp_upre_handler(struct uprobe *p, struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}

static inline int longjmp_break_uhandler(struct uprobe *p,
					 struct pt_regs *regs)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
	return 0;
}


int arch_arm_uprobe(struct uprobe *p);
void arch_disarm_uprobe(struct uprobe *p, struct task_struct *task);


unsigned long arch_get_trampoline_addr(struct uprobe *p, struct pt_regs *regs);
void arch_set_orig_ret_addr(unsigned long orig_ret_addr, struct pt_regs *regs);
int arch_prepare_uretprobe(struct uretprobe_instance *ri,
			   struct pt_regs *regs);

void arch_opcode_analysis_uretprobe(struct uretprobe *rp);
int arch_disarm_urp_inst(struct uretprobe_instance *ri,
			 struct task_struct *task);

static inline void arch_ujprobe_return(void)
{
	WARN(1, "not implemented"); /* FIXME: to implement */
}

int swap_arch_init_uprobes(void);
void swap_arch_exit_uprobes(void);


#endif /* _ASM_ARM64_UPROBES_H */
