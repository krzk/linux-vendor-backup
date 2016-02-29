#ifndef __IP__
#define __IP__

/**
 * @file us_manager/sspt/ip.h
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

#include <linux/list.h>
#include <uprobe/swap_uprobes.h>

struct sspt_page;

/**
 * @struct us_ip
 * @breaf Image of instrumentation pointer for specified process
 */
struct us_ip {
	struct list_head list;		/**< For sspt_page */
	struct sspt_page *page;		/**< Pointer on the page (parent) */

	struct uretprobe retprobe;	/**< uretprobe */
	char *args;			/**< Function arguments */
	char ret_type;			/**< Return type */
	unsigned long orig_addr;	/**< Function address */

	unsigned long offset;		/**< Page offset */
};


struct us_ip *create_ip(unsigned long offset, const char *args, char ret_type);
void free_ip(struct us_ip *ip);

#endif /* __IP__ */
