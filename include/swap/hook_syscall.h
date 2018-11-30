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


#ifndef _SWAP_HOOK_SYSCALL_H
#define _SWAP_HOOK_SYSCALL_H


struct pt_regs;
struct task_struct;


#include <linux/list.h>
#include <linux/errno.h>


struct hook_syscall {
	struct hlist_node node;
	void (*entry)(struct hook_syscall *self, struct pt_regs *regs);
	void (*exit)(struct hook_syscall *self, struct pt_regs *regs);
};

#ifdef CONFIG_SWAP_HOOK_SYSCALL

int hook_syscall_reg(struct hook_syscall *hook, unsigned long nr_call);
void hook_syscall_unreg(struct hook_syscall *hook);

# ifdef CONFIG_COMPAT
int hook_syscall_reg_compat(struct hook_syscall *hook, unsigned long nr_call);
static inline void hook_syscall_unreg_compat(struct hook_syscall *hook)
{
	hook_syscall_unreg(hook);
}
# endif /* CONFIG_COMPAT */

#else /* CONFIG_SWAP_HOOK_SYSCALL */

static inline int hook_syscall_reg(struct hook_syscall *hook,
				   unsigned long nr_call)
{
	return -ENOSYS;
}

static inline void hook_syscall_unreg(struct hook_syscall *hook) {}

# ifdef CONFIG_COMPAT
static inline int hook_syscall_reg_compat(struct hook_syscall *hook,
					  unsigned long nr_call)
{
	return -ENOSYS;
}

static inline void hook_syscall_unreg_compat(struct hook_syscall *hook) {}
# endif /* CONFIG_COMPAT */

#endif /* CONFIG_SWAP_HOOK_SYSCALL */


#endif /* _SWAP_HOOK_SYSCALL_H */
