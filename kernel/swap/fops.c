#include <linux/rwsem.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <swap/swap_fops.h>


static HLIST_HEAD(fops_head);
static DECLARE_RWSEM(fops_sem);

int swap_fops_counter;

int swap_fops_reg(struct swap_fops_hooks *hooks)
{
	if (!try_module_get(hooks->owner))
		return -ENODEV;

	INIT_HLIST_NODE(&hooks->node);

	down_write(&fops_sem);
	hlist_add_head(&hooks->node, &fops_head);
	++swap_fops_counter;
	up_write(&fops_sem);

	return 0;
}
EXPORT_SYMBOL_GPL(swap_fops_reg);

void swap_fops_unreg(struct swap_fops_hooks *hooks)
{
	down_write(&fops_sem);
	--swap_fops_counter;
	hlist_del(&hooks->node);
	up_write(&fops_sem);

	module_put(hooks->owner);
}
EXPORT_SYMBOL_GPL(swap_fops_unreg);

void call_fops_filp_close(struct file *filp)
{
	down_read(&fops_sem);
	if (swap_fops_counter) {
		struct swap_fops_hooks *hooks;

		hlist_for_each_entry(hooks, &fops_head, node)
			hooks->filp_close(filp);
	}
	up_read(&fops_sem);
}
