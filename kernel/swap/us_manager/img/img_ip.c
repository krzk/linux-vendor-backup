/*
 *  SWAP uprobe manager
 *  modules/us_manager/img/img_ip.c
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


#include "img_ip.h"
#include <linux/slab.h>

/**
 * @brief Create img_ip struct
 *
 * @param addr Function address
 * @param args Function arguments
 * @param ret_type Return type
 * @return Pointer to the created img_ip struct
 */
struct img_ip *create_img_ip(unsigned long addr, const char *args,
			     char ret_type)
{
	struct img_ip *ip;
	size_t len;

	ip = kmalloc(sizeof(*ip), GFP_KERNEL);
	if (ip == NULL) {
		printk("Error: cannot allocate memory for ip\n");
		return NULL;
	}
	INIT_LIST_HEAD(&ip->list);
	ip->addr = addr;

	/* copy args */
	len = strlen(args) + 1;
	ip->args = kmalloc(len, GFP_KERNEL);
	if (ip->args == NULL) {
		printk("Error: cannot allocate memory for ip args\n");
		kfree(ip);
		return NULL;
	}
	memcpy(ip->args, args, len);

	ip->ret_type = ret_type;

	return ip;
}

/**
 * @brief Remove img_ip struct
 *
 * @param ip remove object
 * @return Void
 */
void free_img_ip(struct img_ip *ip)
{
	kfree(ip->args);
	kfree(ip);
}

/**
 * @brief For debug
 *
 * @param ip Pointer to the img_ip struct
 * @return Void
 */

/* debug */
void img_ip_print(struct img_ip *ip)
{
	printk("###            addr=8%lx, args=%s\n", ip->addr, ip->args);
}
/* debug */
