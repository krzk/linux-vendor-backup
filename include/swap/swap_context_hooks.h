#ifndef _LINUX_SWAP_CONTEXT_HOOKS_H
#define _LINUX_SWAP_CONTEXT_HOOKS_H


#ifdef CONFIG_SWAP_CONTEXT_HOOKS

#include <linux/compiler.h>


struct swap_ctx_hook {
	struct hlist_node node;
	void (*hook_switch_to)(struct task_struct *prev,
			       struct task_struct *next);
};


extern int ctx_hook_nr;

int swap_context_hooks_reg(struct swap_ctx_hook *hook);
void swap_context_hooks_unreg(struct swap_ctx_hook *hook);


/* private interface */
void swap_call_switch_to_hooks(struct task_struct *prev,
			       struct task_struct *next);


static inline void swap_switch_to(struct task_struct *prev,
				  struct task_struct *next)
{
	if (unlikely(ctx_hook_nr)) {
		swap_call_switch_to_hooks(prev, next);
	}
}

#else /* CONFIG_SWAP_CONTEXT_HOOKS */

static inline void swap_switch_to(struct task_struct *prev,
				      struct task_struct *next)
{
}
#endif /* CONFIG_SWAP_CONTEXT_HOOKS */

#endif /* _LINUX_SWAP_CONTEXT_HOOKS_H */
