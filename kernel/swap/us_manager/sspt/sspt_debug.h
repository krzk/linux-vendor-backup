#ifndef __SSPT_DEBUG__
#define __SSPT_DEBUG__

/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_debug.h
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

#include <kprobe/swap_kprobes_deps.h>

static inline void print_jprobe(struct jprobe *jp)
{
	printk("###         JP: entry=%lx, pre_entry=%lx\n",
			(unsigned long)jp->entry, (unsigned long)jp->pre_entry);
}

static inline void print_retprobe(struct uretprobe *rp)
{
	printk("###         RP: handler=%lx\n",
			(unsigned long)rp->handler);
}

static inline void print_ip(struct us_ip *ip, int i)
{
	printk("###       addr[%2d]=%lx, R_addr=%lx\n",
				i, (unsigned long)ip->offset,
				(unsigned long)ip->retprobe.up.kp.addr);
	print_retprobe(&ip->retprobe);
}

static inline void print_page_probes(const struct sspt_page *page)
{
	int i = 0;
	struct us_ip *ip;

	printk("###     offset=%lx\n", page->offset);
	printk("###     no install:\n");
	list_for_each_entry(ip, &page->ip_list_no_inst, list) {
		print_ip(ip, i);
		++i;
	}

	printk("###     install:\n");
	list_for_each_entry(ip, &page->ip_list_inst, list) {
		print_ip(ip, i);
		++i;
	}
}

static inline void print_file_probes(const struct sspt_file *file)
{
	int i;
	unsigned long table_size;
	struct sspt_page *page = NULL;
	struct hlist_head *head = NULL;
	static unsigned char *NA = "N/A";
	unsigned char *name;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	if (file == NULL) {
		printk("### file_p == NULL\n");
		return;
	}

	table_size = (1 << file->page_probes_hash_bits);
	name = (file->dentry) ? file->dentry->d_iname : NA;

	printk("### print_file_probes: path=%s, d_iname=%s, table_size=%lu, vm_start=%lx\n",
			file->dentry->d_iname, name, table_size, file->vm_start);

	for (i = 0; i < table_size; ++i) {
		head = &file->page_probes_table[i];
		swap_hlist_for_each_entry_rcu(page, node, head, hlist) {
			print_page_probes(page);
		}
	}
}

static inline void print_proc_probes(const struct sspt_proc *proc)
{
	struct sspt_file *file;

	printk("### print_proc_probes\n");
	list_for_each_entry(file, &proc->file_list, list) {
		print_file_probes(file);
	}
	printk("### print_proc_probes\n");
}

/*
static inline void print_inst_us_proc(const inst_us_proc_t *task_inst_info)
{
	int i;
	int cnt = task_inst_info->libs_count;
	printk(  "### BUNDLE PRINT START ###\n");
	printk("\n### BUNDLE PRINT START ###\n");
	printk("### task_inst_info.libs_count=%d\n", cnt);

	for (i = 0; i < cnt; ++i) {
		int j;

		us_proc_lib_t *lib = &task_inst_info->p_libs[i];
		int cnt_j = lib->ips_count;
		char *path = lib->path;
		printk("###     path=%s, cnt_j=%d\n", path, cnt_j);

		for (j = 0; j < cnt_j; ++j) {
			us_proc_ip_t *ips = &lib->p_ips[j];
			unsigned long offset = ips->offset;
			printk("###         offset=%lx\n", offset);
		}
	}
	printk("### BUNDLE PRINT  END  ###\n");
}
*/

#endif /* __SSPT_DEBUG__ */
