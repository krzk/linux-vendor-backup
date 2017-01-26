#include <linux/rwsem.h>
#include <linux/module.h>
#include <swap/swap_us_hooks.h>


struct swap_us_hooks *swap_us_hooks_user;
static DECLARE_RWSEM(hooks_sem);

struct swap_us_hooks *swap_us_hooks_get(void)
{
	struct swap_us_hooks *hooks = NULL;

	down_read(&hooks_sem);
	if (swap_us_hooks_user) {
		hooks = swap_us_hooks_user;
		__module_get(hooks->owner);
	} else {
		up_read(&hooks_sem);
	}

	return hooks;
}
void swap_us_hooks_put(struct swap_us_hooks *hooks)
{
	module_put(hooks->owner);
	up_read(&hooks_sem);
}

int swap_us_hooks_set(struct swap_us_hooks *hooks)
{
	int ret = 0;

	down_write(&hooks_sem);
	if (swap_us_hooks_user) {
		ret = -EBUSY;
		goto unlock;
	}

	if (!try_module_get(hooks->owner)) {
		ret = -ENODEV;
		goto unlock;
	}

	swap_us_hooks_user = hooks;

unlock:
	up_write(&hooks_sem);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_us_hooks_set);

void swap_us_hooks_reset(void)
{
	down_write(&hooks_sem);
	if (swap_us_hooks_user)
		module_put(swap_us_hooks_user->owner);
	swap_us_hooks_user = NULL;
	up_write(&hooks_sem);
}
EXPORT_SYMBOL_GPL(swap_us_hooks_reset);
