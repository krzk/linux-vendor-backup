/**
 * webprobe/webprobe.c
 * @author Ruslan Soloviev
 *
 * @section LICENSE
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
 * @section COPYRIGHT
 *
 * Copyright (C) Samsung Electronics, 2014
 *
 * @section DESCRIPTION
 *
 * Web application profiling
 */


#include <us_manager/us_manager.h>
#include <us_manager/sspt/sspt_ip.h>
#include <us_manager/probes/register_probes.h>
#include <us_manager/sspt/sspt.h>
#include <uprobe/swap_uprobes.h>
#include <parser/msg_cmd.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <master/swap_initializer.h>

#include "webprobe_debugfs.h"
#include "webprobe_prof.h"
#include "web_msg.h"


static unsigned long inspserver_addr_local;
static unsigned long willexecute_addr_local;
static unsigned long didexecute_addr_local;

static int webprobe_copy(struct probe_info *dest,
			 const struct probe_info *source)
{
	memcpy(dest, source, sizeof(*source));

	return 0;
}

static void webprobe_cleanup(struct probe_info *probe_i)
{
}

static struct uprobe *webprobe_get_uprobe(struct sspt_ip *ip)
{
	return &ip->retprobe.up;
}

static int webprobe_register_probe(struct sspt_ip *ip)
{
	return swap_register_uretprobe(&ip->retprobe);
}

static void webprobe_unregister_probe(struct sspt_ip *ip, int disarm)
{
	if (ip->orig_addr == inspserver_addr_local)
		web_func_inst_remove(INSPSERVER_START);
	else if (ip->orig_addr == willexecute_addr_local)
		web_func_inst_remove(WILL_EXECUTE);
	else if (ip->orig_addr == didexecute_addr_local)
		web_func_inst_remove(DID_EXECUTE);

	__swap_unregister_uretprobe(&ip->retprobe, disarm);
}

static int web_entry_handler(struct uretprobe_instance *ri,
			     struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;
	struct sspt_ip *ip;
	unsigned long vaddr, page_vaddr;
	struct vm_area_struct *vma;

	if (rp == NULL)
		return 0;

	ip = container_of(rp, struct sspt_ip, retprobe);
	vaddr = (unsigned long)ip->orig_addr;
	page_vaddr = vaddr & PAGE_MASK;

	vma = find_vma_intersection(current->mm, page_vaddr, page_vaddr + 1);
	if (vma && check_vma(vma)) {
		unsigned long addr = vaddr - vma->vm_start;
		struct dentry *d = vma->vm_file->f_path.dentry;

		if (addr == web_prof_addr(WILL_EXECUTE) &&
		    d == web_prof_lib_dentry()) {
			willexecute_addr_local = ip->orig_addr;
			web_msg_entry(regs);
		} else if (addr == web_prof_addr(DID_EXECUTE) &&
			   d == web_prof_lib_dentry()) {
			didexecute_addr_local = ip->orig_addr;
			web_msg_exit(regs);
		}
	}

	return 0;
}


static int web_ret_handler(struct uretprobe_instance *ri, struct pt_regs *regs)
{
	struct uretprobe *rp = ri->rp;
	struct sspt_ip *ip;
	unsigned long vaddr, page_vaddr;
	struct vm_area_struct *vma;

	if (rp == NULL)
		return 0;

	ip = container_of(rp, struct sspt_ip, retprobe);
	vaddr = (unsigned long)ip->orig_addr;
	page_vaddr = vaddr & PAGE_MASK;

	vma = find_vma_intersection(current->mm, page_vaddr, page_vaddr + 1);
	if (vma && check_vma(vma)) {
		unsigned long addr = vaddr - vma->vm_start;
		struct dentry *d = vma->vm_file->f_path.dentry;

		if (addr == web_prof_addr(INSPSERVER_START) &&
		    d == web_prof_lib_dentry()) {
			inspserver_addr_local = ip->orig_addr;
			set_wrt_launcher_port((int)regs_return_value(regs));
		}
	}

	return 0;
}

static void webprobe_init(struct sspt_ip *ip)
{
	ip->retprobe.entry_handler = web_entry_handler;
	ip->retprobe.handler = web_ret_handler;
	ip->retprobe.maxactive = 0;
}

static void webprobe_uninit(struct sspt_ip *ip)
{
	webprobe_cleanup(&ip->desc->info);
}


static struct probe_iface webprobe_iface = {
	.init = webprobe_init,
	.uninit = webprobe_uninit,
	.reg = webprobe_register_probe,
	.unreg = webprobe_unregister_probe,
	.get_uprobe = webprobe_get_uprobe,
	.copy = webprobe_copy,
	.cleanup = webprobe_cleanup
};

static int webprobe_module_init(void)
{
	int ret = 0;

	ret = swap_register_probe_type(SWAP_WEBPROBE, &webprobe_iface);
	if (ret)
		pr_err("Cannot register probe type SWAP_WEBPROBE\n");

	return ret;
}

static void webprobe_module_exit(void)
{
	swap_unregister_probe_type(SWAP_WEBPROBE);
}

SWAP_LIGHT_INIT_MODULE(NULL, webprobe_module_init, webprobe_module_exit,
		       webprobe_debugfs_init, webprobe_debugfs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SWAP webprobe");
MODULE_AUTHOR("Ruslan Soloviev <r.soloviev@samsung.com>"
	      "Anastasia Lyupa <a.lyupa@samsung.com>");
