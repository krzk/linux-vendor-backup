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
 * Copyright (C) Samsung Electronics, 2017
 *
 *    2017      Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 */


#include <linux/rwsem.h>
#include <linux/module.h>
#include <swap/hook_energy.h>
#include <linux/list.h>


struct swap_hook_energy *swap_nrg_hook;
static DECLARE_RWSEM(energy_hook_sem);

struct swap_hook_energy *swap_hook_energy_get(void)
{
	struct swap_hook_energy *hook = NULL;
	down_read(&energy_hook_sem);
	if (swap_nrg_hook)
		hook = swap_nrg_hook;
	else
		up_read(&energy_hook_sem);

	return hook;
}
void swap_hook_energy_put(void)
{
	up_read(&energy_hook_sem);
}

int swap_hook_energy_set(struct swap_hook_energy *hook)
{
	int ret = 0;

	down_write(&energy_hook_sem);
	if (swap_nrg_hook) {
		ret = -EBUSY;
		goto unlock;
	}

	swap_nrg_hook = hook;

unlock:
	up_write(&energy_hook_sem);
	return ret;
}
EXPORT_SYMBOL_GPL(swap_hook_energy_set);

void swap_hook_energy_unset(void)
{
	down_write(&energy_hook_sem);
	swap_nrg_hook = NULL;
	up_write(&energy_hook_sem);
}
EXPORT_SYMBOL_GPL(swap_hook_energy_unset);
