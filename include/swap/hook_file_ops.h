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


#ifndef _LINUX_SWAP_FILE_OPS_H
#define _LINUX_SWAP_FILE_OPS_H


struct file;


#ifdef CONFIG_SWAP_HOOK_FILE_OPS

#include <linux/list.h>

struct module;

struct swap_hook_fops {
	struct hlist_node node;
	struct module *owner;
	void (*filp_close)(struct file *filp);
};

int swap_hook_fops_reg(struct swap_hook_fops *hook);
void swap_hook_fops_unreg(struct swap_hook_fops *hook);


/* private interface */
extern int swap_fops_counter;
void call_fops_filp_close(struct file *filp);

static inline void swap_fops_filp_close(struct file *filp)
{
	if (unlikely(swap_fops_counter))
		call_fops_filp_close(filp);
}

#else /* CONFIG_SWAP_HOOK_FILE_OPS */

static inline void swap_fops_filp_close(struct file *filp) {}

#endif /* CONFIG_SWAP_HOOK_FILE_OPS */


#endif /* _LINUX_SWAP_FILE_OPS_H */
