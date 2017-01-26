#include <linux/errno.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <swap/swap_td_hooks.h>


static HLIST_HEAD(td_head);
static DEFINE_SPINLOCK(td_lock);
int td_hoos_counter;

int swap_td_hooks_reg(struct swap_td_hooks *hooks)
{
	if (!try_module_get(hooks->owner))
		return -ENODEV;

	INIT_HLIST_NODE(&hooks->node);

	spin_lock(&td_lock);
	hlist_add_head(&hooks->node, &td_head);
	++td_hoos_counter;
	spin_unlock(&td_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(swap_td_hooks_reg);

void swap_td_hooks_unreg(struct swap_td_hooks *hooks)
{
	spin_lock(&td_lock);
	--td_hoos_counter;
	hlist_del(&hooks->node);
	spin_unlock(&td_lock);

	module_put(hooks->owner);
}
EXPORT_SYMBOL_GPL(swap_td_hooks_unreg);

void call_put_task_hooks(struct task_struct *task)
{
	spin_lock(&td_lock);
	if (td_hoos_counter) {
		struct swap_td_hooks *hooks;

		hlist_for_each_entry(hooks, &td_head, node)
			hooks->put_task(task);
	}
	spin_unlock(&td_lock);
}
