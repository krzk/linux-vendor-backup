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


#include <linux/errno.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <swap/hook_taskdata.h>


static HLIST_HEAD(td_head);
static DEFINE_SPINLOCK(td_lock);
int hook_taskdata_counter;

int hook_taskdata_reg(struct hook_taskdata *hook)
{
	if (!try_module_get(hook->owner))
		return -ENODEV;

	INIT_HLIST_NODE(&hook->node);

	spin_lock(&td_lock);
	hlist_add_head(&hook->node, &td_head);
	++hook_taskdata_counter;
	spin_unlock(&td_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(hook_taskdata_reg);

void hook_taskdata_unreg(struct hook_taskdata *hook)
{
	spin_lock(&td_lock);
	--hook_taskdata_counter;
	hlist_del(&hook->node);
	spin_unlock(&td_lock);

	module_put(hook->owner);
}
EXPORT_SYMBOL_GPL(hook_taskdata_unreg);

void hook_taskdata_put_task(struct task_struct *task)
{
	spin_lock(&td_lock);
	if (hook_taskdata_counter) {
		struct hook_taskdata *hook;

		hlist_for_each_entry(hook, &td_head, node)
			hook->put_task(task);
	}
	spin_unlock(&td_lock);
}
