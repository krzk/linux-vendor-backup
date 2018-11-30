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


#ifndef _LINUX_SWAP_HOOK_TASKDATA_H
#define _LINUX_SWAP_HOOK_TASKDATA_H


#ifdef CONFIG_SWAP_HOOK_TASKDATA

#include <linux/list.h>
#include <linux/compiler.h>

struct module;
struct task_struct;

struct hook_taskdata {
	struct hlist_node node;
	struct module *owner;
	void (*put_task)(struct task_struct *task);
};

int hook_taskdata_reg(struct hook_taskdata *hook);
void hook_taskdata_unreg(struct hook_taskdata *hook);

/* private interface */
extern int hook_taskdata_counter;
void hook_taskdata_put_task(struct task_struct *task);

static inline void swap_taskdata_put_task(struct task_struct *task)
{
	if (unlikely(hook_taskdata_counter))
		hook_taskdata_put_task(task);
}

#else /* CONFIG_SWAP_HOOK_TASKDATA */

static inline void swap_taskdata_put_task(struct task_struct *task) {}

#endif /* CONFIG_SWAP_HOOK_TASKDATA */


#endif /* _LINUX_SWAP_HOOK_TASKDATA_H */
