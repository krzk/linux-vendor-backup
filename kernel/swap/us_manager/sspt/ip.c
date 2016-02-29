/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/ip.c
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
 * 2013         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */

#include <linux/slab.h>
#include "ip.h"
#include "sspt_page.h"
#include "sspt_file.h"
#include <writer/swap_writer_module.h>
#include <us_manager/us_manager.h>


static int entry_handler(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp && get_quiet() == QT_OFF) {
		struct us_ip *ip = container_of(rp, struct us_ip, retprobe);
		const char *fmt = ip->args;
		unsigned long addr = (unsigned long)ip->orig_addr;

		entry_event(fmt, addr, regs, PT_US, PST_NONE);
	}

	return 0;
}

static int ret_handler(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;

	if (rp && get_quiet() == QT_OFF) {
		struct us_ip *ip = container_of(rp, struct us_ip, retprobe);
		unsigned long addr = (unsigned long)ip->orig_addr;
		unsigned long ret_addr = (unsigned long)ri->ret_addr;

		exit_event(ip->ret_type, regs, PT_US, PST_NONE, addr, ret_addr);
	}

	return 0;
}

/**
 * @brief Create us_ip struct
 *
 * @param offset Function offset from the beginning of the page
 * @param args Function arguments
 * @param ret_type Return type
 * @return Pointer to the created us_ip struct
 */
struct us_ip *create_ip(unsigned long offset, const char *args, char ret_type)
{
	size_t len = strlen(args) + 1;
	struct us_ip *ip = kmalloc(sizeof(*ip) + len, GFP_ATOMIC);

	if (ip != NULL) {
		memset(ip, 0, sizeof(*ip));

		INIT_LIST_HEAD(&ip->list);
		ip->offset = offset;
		ip->args = (char *)ip + sizeof(*ip);
		ip->ret_type = ret_type;

		/* copy args */
		memcpy(ip->args, args, len);

		/* retprobe */
		ip->retprobe.handler = ret_handler;
		ip->retprobe.entry_handler = entry_handler;
	} else {
		printk("Cannot kmalloc in create_ip function!\n");
	}

	return ip;
}

/**
 * @brief Remove us_ip struct
 *
 * @param ip remove object
 * @return Void
 */
void free_ip(struct us_ip *ip)
{
	kfree(ip);
}
