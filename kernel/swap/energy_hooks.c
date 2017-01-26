#include <linux/rwsem.h>
#include <linux/module.h>
#include <swap/swap_energy_hooks.h>
#include <linux/list.h>


struct swap_energy_hooks *swap_nrg_hooks;
static DECLARE_RWSEM(energy_hooks_sem);

struct swap_energy_hooks *swap_energy_hooks_get(void)
{
	struct swap_energy_hooks *hooks = NULL;
	down_read(&energy_hooks_sem);
	if (swap_nrg_hooks)
		hooks = swap_nrg_hooks;
	else
		up_read(&energy_hooks_sem);

	return hooks;
}
void swap_energy_hooks_put(void)
{
	up_read(&energy_hooks_sem);
}

int swap_energy_hooks_set(struct swap_energy_hooks *hooks)
{
	int ret = 0;

	down_write(&energy_hooks_sem);
	if (swap_nrg_hooks) {
		ret = -EBUSY;
		goto unlock;
	}

	swap_nrg_hooks = hooks;

unlock:
	up_write(&energy_hooks_sem);
	return ret;
}
EXPORT_SYMBOL_GPL(swap_energy_hooks_set);

void swap_energy_hooks_unset(void)
{
	down_write(&energy_hooks_sem);
	swap_nrg_hooks = NULL;
	up_write(&energy_hooks_sem);
}
EXPORT_SYMBOL_GPL(swap_energy_hooks_unset);
