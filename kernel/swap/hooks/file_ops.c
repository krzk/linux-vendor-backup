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
#include <linux/errno.h>
#include <linux/module.h>
#include <swap/hook_file_ops.h>


static HLIST_HEAD(fops_head);
static DECLARE_RWSEM(fops_sem);

int swap_fops_counter;

int swap_hook_fops_reg(struct swap_hook_fops *hook)
{
	if (!try_module_get(hook->owner))
		return -ENODEV;

	INIT_HLIST_NODE(&hook->node);

	down_write(&fops_sem);
	hlist_add_head(&hook->node, &fops_head);
	++swap_fops_counter;
	up_write(&fops_sem);

	return 0;
}
EXPORT_SYMBOL_GPL(swap_hook_fops_reg);

void swap_hook_fops_unreg(struct swap_hook_fops *hook)
{
	down_write(&fops_sem);
	--swap_fops_counter;
	hlist_del(&hook->node);
	up_write(&fops_sem);

	module_put(hook->owner);
}
EXPORT_SYMBOL_GPL(swap_hook_fops_unreg);

void call_fops_filp_close(struct file *filp)
{
	down_read(&fops_sem);
	if (swap_fops_counter) {
		struct swap_hook_fops *hook;

		hlist_for_each_entry(hook, &fops_head, node)
			hook->filp_close(filp);
	}
	up_read(&fops_sem);
}
