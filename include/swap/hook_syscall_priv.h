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


#ifndef _SWAP_HOOK_SYSCALL_PRIV_H
#define _SWAP_HOOK_SYSCALL_PRIV_H


struct pt_regs;
struct task_struct;


#ifdef CONFIG_SWAP_HOOK_SYSCALL

#include <linux/sched.h>

static inline void swap_hook_syscall_update(struct task_struct *p)
{
        if (test_thread_flag(TIF_SWAP_HOOK_SYSCALL))
                set_tsk_thread_flag(p, TIF_SWAP_HOOK_SYSCALL);
        else
                clear_tsk_thread_flag(p, TIF_SWAP_HOOK_SYSCALL);
}

void swap_hook_syscall_entry(struct pt_regs *regs);
void swap_hook_syscall_exit(struct pt_regs *regs);

#else /* CONFIG_SWAP_HOOK_SYSCALL */

static inline void swap_hook_syscall_update(struct task_struct *p) {}
static inline void swap_hook_syscall_entry(struct pt_regs *regs) {}
static inline void swap_hook_syscall_exit(struct pt_regs *regs) {}

#endif /* CONFIG_SWAP_HOOK_SYSCALL */


#endif /* _SWAP_HOOK_SYSCALL_PRIV_H */
