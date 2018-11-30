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


#ifndef _LINUX_SWAP_HOOK_SWITCH_TO_H
#define _LINUX_SWAP_HOOK_SWITCH_TO_H


#ifdef CONFIG_SWAP_HOOK_SWITCH_TO

#include <linux/compiler.h>


struct swap_hook_ctx {
	struct hlist_node node;
	void (*hook)(struct task_struct *prev, struct task_struct *next);
};


extern int ctx_hook_nr;

int swap_hook_ctx_reg(struct swap_hook_ctx *hook);
void swap_hook_ctx_unreg(struct swap_hook_ctx *hook);


/* private interface */
void swap_hook_ctx_call(struct task_struct *prev, struct task_struct *next);

static inline void swap_hook_switch_to(struct task_struct *prev,
				       struct task_struct *next)
{
	if (unlikely(ctx_hook_nr)) {
		swap_hook_ctx_call(prev, next);
	}
}

#else /* CONFIG_SWAP_HOOK_SWITCH_TO */

static inline void swap_hook_switch_to(struct task_struct *prev,
				       struct task_struct *next)
{
}
#endif /* CONFIG_SWAP_HOOK_SWITCH_TO */

#endif /* _LINUX_SWAP_HOOK_SWITCH_TO_H */
