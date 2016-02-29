#ifndef __SSPT__
#define __SSPT__

/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt.h
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

#include "ip.h"
#include "sspt_page.h"
#include "sspt_file.h"
#include "sspt_proc.h"
#include "sspt_debug.h"
#include <uprobe/swap_uprobes.h>


#include <us_manager/us_manager.h>
#include <us_manager/pf/pf_group.h>


static inline int check_vma(struct vm_area_struct *vma)
{
	return vma->vm_file &&
	       !(vma->vm_pgoff != 0 ||
		 !(vma->vm_flags & VM_EXEC) ||
		 !(vma->vm_flags & (VM_READ | VM_MAYREAD)));
}

static inline int sspt_register_usprobe(struct us_ip *ip)
{
	int ret;

	/* for retuprobe */
	ip->retprobe.up.task = ip->page->file->proc->task;
	ip->retprobe.up.sm = ip->page->file->proc->sm;

	ret = swap_register_uretprobe(&ip->retprobe);
	if (ret) {
		struct sspt_file *file = ip->page->file;
		char *name = file->dentry->d_iname;
		unsigned long addr = (unsigned long)ip->retprobe.up.kp.addr;
		unsigned long offset = addr - file->vm_start;

		printk("swap_register_uretprobe() failure %d (%s:%lx|%lx)\n",
		       ret, name, offset, (unsigned long)ip->retprobe.up.kp.opcode);
	}

	return ret;
}

static inline int do_unregister_usprobe(struct us_ip *ip, int disarm)
{
	__swap_unregister_uretprobe(&ip->retprobe, disarm);

	return 0;
}

static inline int sspt_unregister_usprobe(struct task_struct *task, struct us_ip *ip, enum US_FLAGS flag)
{
	int err = 0;

	switch (flag) {
	case US_UNREGS_PROBE:
		err = do_unregister_usprobe(ip, 1);
		break;
	case US_DISARM:
		disarm_uprobe(&ip->retprobe.up.kp, task);
		break;
	case US_UNINSTALL:
		err = do_unregister_usprobe(ip, 0);
		break;
	default:
		panic("incorrect value flag=%d", flag);
	}

	return err;
}

#endif /* __SSPT__ */
