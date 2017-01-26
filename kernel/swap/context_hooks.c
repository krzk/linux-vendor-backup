#include <linux/rwsem.h>
#include <linux/module.h>
#include <linux/list.h>
#include <swap/swap_context_hooks.h>


static HLIST_HEAD(ctx_hook_head);
static DEFINE_SPINLOCK(ctx_hook_lock);
int ctx_hook_nr = 0;

static inline void swap_context_hooks_get(void)
{
	spin_lock(&ctx_hook_lock);
}
static inline void swap_context_hooks_put(void)
{
	spin_unlock(&ctx_hook_lock);
}

void swap_call_switch_to_hooks(struct task_struct *prev,
			       struct task_struct *next)
{
	struct swap_ctx_hook *tmp;

	swap_context_hooks_get();
	hlist_for_each_entry(tmp, &ctx_hook_head, node) {
		tmp->hook_switch_to(prev, next);
	}
	swap_context_hooks_put();
}


int swap_context_hooks_reg(struct swap_ctx_hook *hook)
{
	int ret = 0;

	INIT_HLIST_NODE(&hook->node);
	swap_context_hooks_get();

	hlist_add_head(&hook->node, &ctx_hook_head);
	ctx_hook_nr++;

	swap_context_hooks_put();
	return ret;
}
EXPORT_SYMBOL_GPL(swap_context_hooks_reg);

void swap_context_hooks_unreg(struct swap_ctx_hook *hook)
{
	swap_context_hooks_get();
	ctx_hook_nr--;
	if (ctx_hook_nr < 0) {
		pr_err("ERROR: [%s:%d]: ctx_hook_nr < 0\n", __FILE__, __LINE__);
		ctx_hook_nr = 0;
	}
	hlist_del(&hook->node);
	swap_context_hooks_put();
}
EXPORT_SYMBOL_GPL(swap_context_hooks_unreg);
