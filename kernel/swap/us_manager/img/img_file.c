/*
 *  SWAP uprobe manager
 *  modules/us_manager/img/img_file.c
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


#include "img_file.h"
#include "img_ip.h"
#include <linux/slab.h>
#include <linux/dcache.h>


static void img_del_ip_by_list(struct img_ip *ip);

/**
 * @brief Create img_file struct
 *
 * @param dentry Dentry of file
 * @return Pointer to the created img_file struct
 */
struct img_file *create_img_file(struct dentry *dentry)
{
	struct img_file *file;

	file = kmalloc(sizeof(*file), GFP_KERNEL);
	if (file) {
		file->dentry = dentry;
		INIT_LIST_HEAD(&file->ip_list);
		INIT_LIST_HEAD(&file->list);
	} else {
		printk("Cannot allocate memory for file\n");
	}

	return file;
}

/**
 * @brief Remove img_file struct
 *
 * @param file remove object
 * @return Void
 */
void free_img_file(struct img_file *file)
{
	struct img_ip *ip, *tmp;

	list_for_each_entry_safe(ip, tmp, &file->ip_list, list) {
		img_del_ip_by_list(ip);
		free_img_ip(ip);
	}

	kfree(file);
}

static void img_add_ip_by_list(struct img_file *file, struct img_ip *ip)
{
	list_add(&ip->list, &file->ip_list);
}

static void img_del_ip_by_list(struct img_ip *ip)
{
	list_del(&ip->list);
}

static struct img_ip *find_img_ip(struct img_file *file, unsigned long addr)
{
	struct img_ip *ip;

	list_for_each_entry(ip, &file->ip_list, list) {
		if (ip->addr == addr)
			return ip;
	}

	return NULL;
}

/**
 * @brief Add instrumentation pointer
 *
 * @param file Pointer to the img_file struct
 * @param addr Function address
 * @param args Function arguments
 * @param ret_type Return type
 * @return Error code
 */
int img_file_add_ip(struct img_file *file, unsigned long addr,
		    const char *args, char ret_type)
{
	struct img_ip *ip;

	ip = find_img_ip(file, addr);
	if (ip) {
		/* ip already exists in img */
		return 0;
	}

	ip = create_img_ip(addr, args, ret_type);
	img_add_ip_by_list(file, ip);

	return 0;
}

/**
 * @brief Delete img_ip struct from img_file struct
 *
 * @param file Pointer to the img_file struct
 * @param addr Function address
 * @return Error code
 */
int img_file_del_ip(struct img_file *file, unsigned long addr)
{
	struct img_ip *ip;

	ip = find_img_ip(file, addr);
	if (ip == NULL) {
		printk("Warning: no ip found in img, addr = %lx\n", addr);
		return -EINVAL;
	}

	img_del_ip_by_list(ip);

	return 0;
}

/**
 * @brief Check on absence img_ip structs in img_file struct
 *
 * @param file Pointer to the img_file struct
 * @return
 *       - 0 - not empty
 *       - 1 - empty
 */
int img_file_empty(struct img_file *file)
{
	return list_empty(&file->ip_list);
}

/**
 * @brief For debug
 *
 * @param file Pointer to the img_file struct
 * @return Void
 */

/* debug */
void img_file_print(struct img_file *file)
{
	struct img_ip *ip;

	printk("###      d_iname=%s\n", file->dentry->d_iname);

	list_for_each_entry(ip, &file->ip_list, list) {
		img_ip_print(ip);
	}
}
/* debug */
