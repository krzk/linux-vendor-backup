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


#ifndef _LINUX_SWAP_HOOK_SIGNAL_H
#define _LINUX_SWAP_HOOK_SIGNAL_H


#ifdef CONFIG_SWAP_HOOK_SIGNAL

#include <linux/list.h>
#include <linux/compiler.h>

struct module;
struct ksignal;

struct hook_signal {
	struct hlist_node node;
	struct module *owner;
	void (*hook)(struct ksignal *ksig);
};

int hook_signal_reg(struct hook_signal *hook);
void hook_signal_unreg(struct hook_signal *hook);


/* private interface */
extern int __hook_signal_counter;
void __hook_signal(struct ksignal *ksig);

static inline void swap_hook_signal(struct ksignal *ksig)
{
	if (unlikely(__hook_signal_counter))
		__hook_signal(ksig);
}

#else /* CONFIG_SWAP_HOOK_SIGNAL */

static inline void swap_hook_signal(struct ksignal *ksig) {}

#endif /* CONFIG_SWAP_HOOK_SIGNAL */


#endif /* _LINUX_SWAP_HOOK_SIGNAL_H */
