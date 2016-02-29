/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_page.c
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

#include "sspt.h"
#include "sspt_page.h"
#include "sspt_file.h"
#include "ip.h"
#include <linux/slab.h>
#include <linux/list.h>

/**
 * @brief Create sspt_page struct
 *
 * @param offset File ofset
 * @return Pointer to the created sspt_page struct
 */
struct sspt_page *sspt_page_create(unsigned long offset)
{
	struct sspt_page *obj = kmalloc(sizeof(*obj), GFP_ATOMIC);
	if (obj) {
		INIT_LIST_HEAD(&obj->ip_list_inst);
		INIT_LIST_HEAD(&obj->ip_list_no_inst);
		obj->offset = offset;
		spin_lock_init(&obj->lock);
		obj->file = NULL;
		INIT_HLIST_NODE(&obj->hlist);
	}

	return obj;
}

/**
 * @brief Remove sspt_page struct
 *
 * @param page remove object
 * @return Void
 */
void sspt_page_free(struct sspt_page *page)
{
	struct us_ip *ip, *n;

	list_for_each_entry_safe(ip, n, &page->ip_list_inst, list) {
		list_del(&ip->list);
		free_ip(ip);
	}

	list_for_each_entry_safe(ip, n, &page->ip_list_no_inst, list) {
		list_del(&ip->list);
		free_ip(ip);
	}

	kfree(page);
}

static void sspt_list_add_ip(struct sspt_page *page, struct us_ip *ip)
{
	list_add(&ip->list, &page->ip_list_no_inst);
	ip->page = page;
}

static void sspt_list_del_ip(struct us_ip *ip)
{
	list_del(&ip->list);
}

/**
 * @brief Add instruction pointer to sspt_page
 *
 * @param page Pointer to the sspt_page struct
 * @param ip Pointer to the us_ip struct
 * @return Void
 */
void sspt_add_ip(struct sspt_page *page, struct us_ip *ip)
{
	ip->offset &= ~PAGE_MASK;

	sspt_list_add_ip(page, ip);
}

/**
 * @brief Del instruction pointer from sspt_page
 *
 * @param ip Pointer to the us_ip struct
 * @return Void
 */
void sspt_del_ip(struct us_ip *ip)
{
	sspt_list_del_ip(ip);
	free_ip(ip);
}

/**
 * @brief Check if probes are set on the page
 *
 * @param page Pointer to the sspt_page struct
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int sspt_page_is_installed(struct sspt_page *page)
{
	int empty;

	spin_lock(&page->lock);
	empty = list_empty(&page->ip_list_inst);
	spin_unlock(&page->lock);

	return !empty;
}

/**
 * @brief Install probes on the page
 *
 * @param page Pointer to the sspt_page struct
 * @param file Pointer to the sspt_file struct
 * @return Error code
 */
int sspt_register_page(struct sspt_page *page, struct sspt_file *file)
{
	int err = 0;
	struct us_ip *ip, *n;
	struct list_head ip_list_tmp;
	unsigned long addr;

	spin_lock(&page->lock);
	if (list_empty(&page->ip_list_no_inst)) {
		struct task_struct *task = page->file->proc->task;

		printk("page %lx in %s task[tgid=%u, pid=%u] already installed\n",
				page->offset, file->dentry->d_iname, task->tgid, task->pid);
		goto unlock;
	}

	INIT_LIST_HEAD(&ip_list_tmp);
	list_replace_init(&page->ip_list_no_inst, &ip_list_tmp);
	spin_unlock(&page->lock);

	list_for_each_entry_safe(ip, n, &ip_list_tmp, list) {
		/* set uprobe address */
		addr = file->vm_start + page->offset + ip->offset;

		ip->orig_addr = addr;
		ip->retprobe.up.kp.addr = (kprobe_opcode_t *)addr;

		err = sspt_register_usprobe(ip);
		if (err) {
			list_del(&ip->list);
			free_ip(ip);
			continue;
		}
	}

	spin_lock(&page->lock);
	list_splice(&ip_list_tmp, &page->ip_list_inst);

unlock:
	spin_unlock(&page->lock);

	return 0;
}

/**
 * @brief Uninstall probes on the page
 *
 * @param page Pointer to the sspt_page struct
 * @param flag Action for probes
 * @param task Pointer to the task_struct struct
 * @return Error code
 */
int sspt_unregister_page(struct sspt_page *page,
			 enum US_FLAGS flag,
			 struct task_struct *task)
{
	int err = 0;
	struct us_ip *ip;
	struct list_head ip_list_tmp, *head;

	spin_lock(&page->lock);
	if (list_empty(&page->ip_list_inst)) {
		spin_unlock(&page->lock);
		return 0;
	}

	INIT_LIST_HEAD(&ip_list_tmp);
	list_replace_init(&page->ip_list_inst, &ip_list_tmp);

	spin_unlock(&page->lock);

	list_for_each_entry(ip, &ip_list_tmp, list) {
		err = sspt_unregister_usprobe(task, ip, flag);
		if (err != 0) {
			//TODO: ERROR
			break;
		}
	}

	head = (flag == US_DISARM) ? &page->ip_list_inst : &page->ip_list_no_inst;

	spin_lock(&page->lock);

	list_splice(&ip_list_tmp, head);
	spin_unlock(&page->lock);

	return err;
}
