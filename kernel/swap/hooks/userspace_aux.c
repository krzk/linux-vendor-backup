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
#include <swap/hook_usaux.h>


struct hook_usaux *hook_usaux_user;
static DECLARE_RWSEM(hook_sem);

struct hook_usaux *swap_hook_usaux_get(void)
{
	struct hook_usaux *hook = NULL;

	down_read(&hook_sem);
	if (hook_usaux_user) {
		hook = hook_usaux_user;
		__module_get(hook->owner);
	} else {
		up_read(&hook_sem);
	}

	return hook;
}
void swap_hook_usaux_put(struct hook_usaux *hook)
{
	module_put(hook->owner);
	up_read(&hook_sem);
}

int hook_usaux_set(struct hook_usaux *hook)
{
	int ret = 0;

	down_write(&hook_sem);
	if (hook_usaux_user) {
		ret = -EBUSY;
		goto unlock;
	}

	if (!try_module_get(hook->owner)) {
		ret = -ENODEV;
		goto unlock;
	}

	hook_usaux_user = hook;

unlock:
	up_write(&hook_sem);

	return ret;
}
EXPORT_SYMBOL_GPL(hook_usaux_set);

void hook_usaux_reset(void)
{
	down_write(&hook_sem);
	if (hook_usaux_user)
		module_put(hook_usaux_user->owner);
	hook_usaux_user = NULL;
	up_write(&hook_sem);
}
EXPORT_SYMBOL_GPL(hook_usaux_reset);
