/**
 * @file us_manager/pf/pf_group.h
 * @author Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 * @section LICENSE
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
 * @section COPYRIGHT
 * Copyright (C) Samsung Electronics, 2013
 */


#ifndef _PF_GROUP_H
#define _PF_GROUP_H

#include <linux/types.h>

struct dentry;
struct pf_group;
struct sspt_proc;

struct pf_group *get_pf_group_by_dentry(struct dentry *dentry, void *priv);
struct pf_group *get_pf_group_by_tgid(pid_t tgid, void *priv);
struct pf_group *get_pf_group_by_comm(char *comm, void *priv);
struct pf_group *get_pf_group_dumb(void *priv);
void put_pf_group(struct pf_group *pfg);

int pf_register_probe(struct pf_group *pfg, struct dentry *dentry,
		      unsigned long offset, const char *args, char ret_type);
int pf_unregister_probe(struct pf_group *pfg, struct dentry *dentry,
			unsigned long offset);

void install_all(void);
void uninstall_all(void);

int check_task_on_filters(struct task_struct *task);
void call_page_fault(struct task_struct *task, unsigned long page_addr);
void call_mm_release(struct task_struct *task);
void check_task_and_install(struct task_struct *task);
void uninstall_proc(struct sspt_proc *proc);

void uninstall_page(unsigned long addr);

/* debug */
void pfg_print(struct pf_group *pfg);
/* debug */

#endif /* _PF_GROUP_H */
