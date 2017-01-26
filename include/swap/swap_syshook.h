#ifndef _SWAP_SYSHOOK_H
#define _SWAP_SYSHOOK_H


struct pt_regs;
struct task_struct;


#ifdef CONFIG_SWAP_SYSHOOK

#include <linux/list.h>
#include <linux/sched.h>

struct syshook_probe {
	struct hlist_node node;
	void (*sys_entry)(struct syshook_probe *, struct pt_regs *regs);
	void (*sys_exit)(struct syshook_probe *, struct pt_regs *regs);
};

int swap_syshook_reg(struct syshook_probe *p, unsigned long nr_call);
void swap_syshook_unreg(struct syshook_probe *p);

#ifdef CONFIG_COMPAT
int swap_syshook_reg_compat(struct syshook_probe *p, unsigned long nr_call);
static inline void swap_syshook_unreg_compat(struct syshook_probe *p)
{
	swap_syshook_unreg(p);
}
#endif /* CONFIG_COMPAT */

/* private interface */
static inline void swap_syshook_update(struct task_struct *p)
{
	if (test_thread_flag(TIF_SWAP_SYSHOOK))
		set_tsk_thread_flag(p, TIF_SWAP_SYSHOOK);
	else
		clear_tsk_thread_flag(p, TIF_SWAP_SYSHOOK);
}

void swap_syshook_entry(struct pt_regs *regs);
void swap_syshook_exit(struct pt_regs *regs);

#else /* CONFIG_SWAP_SYSHOOK */

static inline void swap_syshook_update(struct task_struct *p) {}
static inline void swap_syshook_entry(struct pt_regs *regs) {}
static inline void swap_syshook_exit(struct pt_regs *regs) {}

#endif /* CONFIG_SWAP_SYSHOOK */


#endif /* _SWAP_SYSHOOK_H */
