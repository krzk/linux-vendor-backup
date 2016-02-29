/*
 *  SWAP uprobe manager
 *  modules/us_manager/img/img_proc.c
 *
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
 * Copyright (C) Samsung Electronics, 2013
 *
 * 2013	 Vyacheslav Cherkashin: SWAP us_manager implement
 *
 */


#include "img_proc.h"
#include "img_file.h"
#include <linux/slab.h>

static void img_del_file_by_list(struct img_file *file);

/**
 * @brief Create img_proc struct
 *
 * @return Pointer to the created img_proc struct
 */
struct img_proc *create_img_proc(void)
{
	struct img_proc *proc;

	proc = kmalloc(sizeof(*proc), GFP_KERNEL);
	if (proc)
		INIT_LIST_HEAD(&proc->file_list);
	else
		printk("Cannot allocate memory for img_proc\n");

	return proc;
}

/**
 * @brief Remove img_proc struct
 *
 * @param file remove object
 * @return Void
 */
void free_img_proc(struct img_proc *ip)
{
	struct img_file *file, *tmp;

	list_for_each_entry_safe(file, tmp, &ip->file_list, list) {
		img_del_file_by_list(file);
		free_img_file(file);
	}

	kfree(ip);
}

static void img_add_file_by_list(struct img_proc *proc, struct img_file *file)
{
	list_add(&file->list, &proc->file_list);
}

static void img_del_file_by_list(struct img_file *file)
{
	list_del(&file->list);
}

static struct img_file *find_img_file(struct img_proc *proc, struct dentry *dentry)
{
	struct img_file *file;

	list_for_each_entry(file, &proc->file_list, list) {
		if (file->dentry == dentry)
			return file;
	}

	return NULL;
}

/**
 * @brief Add instrumentation pointer
 *
 * @param proc Pointer to the img_proc struct
 * @param dentry Dentry of file
 * @param addr Function address
 * @param args Function address
 * @param ret_type Return type
 * @return Error code
 */
int img_proc_add_ip(struct img_proc *proc, struct dentry *dentry,
		    unsigned long addr, const char *args, char ret_type)
{
	int ret;
	struct img_file *file;

	file = find_img_file(proc, dentry);
	if (file)
		return img_file_add_ip(file, addr, args, ret_type);

	file = create_img_file(dentry);
	if (file == NULL)
		return -ENOMEM;

	ret = img_file_add_ip(file, addr, args, ret_type);
	if (ret) {
		printk("Cannot add ip to img file\n");
		free_img_file(file);
	}
	else
		img_add_file_by_list(proc, file);

	return ret;
}

/**
 * @brief Remove instrumentation pointer
 *
 * @param proc Pointer to the img_proc struct
 * @param dentry Dentry of file
 * @param args Function address
 * @return Error code
 */
int img_proc_del_ip(struct img_proc *proc, struct dentry *dentry, unsigned long addr)
{
	int ret;
	struct img_file *file;

	file = find_img_file(proc, dentry);
	if (file == NULL)
		return -EINVAL;

	ret = img_file_del_ip(file, addr);
	if (ret == 0 && img_file_empty(file)) {
		img_del_file_by_list(file);
		free_img_file(file);
	}

	return ret;
}

/**
 * @brief For debug
 *
 * @param proc Pointer to the img_proc struct
 * @return Void
 */

/* debug */
void img_proc_print(struct img_proc *proc)
{
	struct img_file *file;

	printk("### img_proc_print:\n");
	list_for_each_entry(file, &proc->file_list, list) {
		img_file_print(file);
	}
}
/* debug */
