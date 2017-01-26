#ifndef _LINUX_SWAP_TD_HOOKS_H
#define _LINUX_SWAP_TD_HOOKS_H


#ifdef CONFIG_SWAP_TD_HOOKS

#include <linux/list.h>
#include <linux/compiler.h>

struct module;
struct task_struct;

struct swap_td_hooks {
	struct hlist_node node;
	struct module *owner;
	void (*put_task)(struct task_struct *task);
};

int swap_td_hooks_reg(struct swap_td_hooks *hook);
void swap_td_hooks_unreg(struct swap_td_hooks *hooks);

/* private interface */
extern int td_hoos_counter;
void call_put_task_hooks(struct task_struct *task);

static inline void swap_td_put_task(struct task_struct *task)
{
	if (unlikely(td_hoos_counter))
		call_put_task_hooks(task);
}

#else /* CONFIG_SWAP_TD_HOOKS */

void swap_td_put_task(struct task_struct *task) {}

#endif /* CONFIG_SWAP_TD_HOOKS */


#endif /* _LINUX_SWAP_TD_HOOKS_H */
