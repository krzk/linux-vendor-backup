/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_file.c
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
#include "sspt_file.h"
#include "sspt_page.h"
#include "sspt_proc.h"
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/sched.h>
#include <kprobe/swap_kprobes_deps.h>

static int calculation_hash_bits(int cnt)
{
	int bits;
	for (bits = 1; cnt >>= 1; ++bits);

	return bits;
}

/**
 * @brief Create sspt_file struct
 *
 * @param dentry Dentry of file
 * @param page_cnt Size of hash-table
 * @return Pointer to the created sspt_file struct
 */
struct sspt_file *sspt_file_create(struct dentry *dentry, int page_cnt)
{
	struct sspt_file *obj = kmalloc(sizeof(*obj), GFP_ATOMIC);

	if (obj) {
		int i, table_size;
		INIT_LIST_HEAD(&obj->list);
		obj->proc = NULL;
		obj->dentry = dentry;
		obj->loaded = 0;
		obj->vm_start = 0;
		obj->vm_end = 0;

		obj->page_probes_hash_bits = calculation_hash_bits(page_cnt);//PAGE_PROBES_HASH_BITS;
		table_size = (1 << obj->page_probes_hash_bits);

		obj->page_probes_table = kmalloc(sizeof(*obj->page_probes_table)*table_size, GFP_ATOMIC);
		if (obj->page_probes_table == NULL) {
			printk("Cannot allocate memory for page probes table\n");
			kfree(obj);
			return NULL;
		}
		for (i = 0; i < table_size; ++i) {
			INIT_HLIST_HEAD(&obj->page_probes_table[i]);
		}
	}

	return obj;
}

/**
 * @brief Remove sspt_file struct
 *
 * @param file remove object
 * @return Void
 */
void sspt_file_free(struct sspt_file *file)
{
	struct hlist_head *head;
	struct sspt_page *page;
	int i, table_size = (1 << file->page_probes_hash_bits);
	struct hlist_node *n;
	DECLARE_NODE_PTR_FOR_HLIST(p);

	for (i = 0; i < table_size; ++i) {
		head = &file->page_probes_table[i];
		swap_hlist_for_each_entry_safe(page, p, n, head, hlist) {
			hlist_del(&page->hlist);
			sspt_page_free(page);
		}
	}

	kfree(file->page_probes_table);
	kfree(file);
}

static void sspt_add_page(struct sspt_file *file, struct sspt_page *page)
{
	page->file = file;
	hlist_add_head(&page->hlist, &file->page_probes_table[hash_ptr((void *)page->offset,
				file->page_probes_hash_bits)]);
}

static struct sspt_page *sspt_find_page(struct sspt_file *file, unsigned long offset)
{
	struct hlist_head *head;
	struct sspt_page *page;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	head = &file->page_probes_table[hash_ptr((void *)offset, file->page_probes_hash_bits)];
	swap_hlist_for_each_entry(page, node, head, hlist) {
		if (page->offset == offset) {
			return page;
		}
	}

	return NULL;
}

static struct sspt_page *sspt_find_page_or_new(struct sspt_file *file, unsigned long offset)
{
	struct sspt_page *page = sspt_find_page(file, offset);

	if (page == NULL) {
		page = sspt_page_create(offset);
		if (page == NULL) {
			printk("Cannot create new page\n");
			return NULL;
		}
		sspt_add_page(file, page);
	}

	return page;
}

/**
 * @brief Get sspt_page from sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param page Page address
 * @return Pointer to the sspt_page struct
 */
struct sspt_page *sspt_find_page_mapped(struct sspt_file *file, unsigned long page)
{
	unsigned long offset;

	if (file->vm_start > page || file->vm_end < page) {
		// TODO: or panic?!
		printk("ERROR: file_p[vm_start..vm_end] <> page: file_p[vm_start=%lx, vm_end=%lx, d_iname=%s] page=%lx\n",
				file->vm_start, file->vm_end, file->dentry->d_iname, page);
		return NULL;
	}

	offset = page - file->vm_start;

	return sspt_find_page(file, offset);
}

/**
 * @brief Add instruction pointer to sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param offset File offset
 * @param args Function arguments
 * @param ret_type Return type
 * @return Void
 */
void sspt_file_add_ip(struct sspt_file *file, unsigned long offset,
		      const char *args, char ret_type)
{
	struct sspt_page *page = sspt_find_page_or_new(file, offset & PAGE_MASK);

	// FIXME: delete ip
	struct us_ip *ip = create_ip(offset, args, ret_type);
	if (ip)
		sspt_add_ip(page, ip);
	else
		printk("ERROR: cannot create ip \n");
}

/**
 * @brief Get sspt_page from sspt_file (look)
 *
 * @param file Pointer to the sspt_file struct
 * @param offset_addr File offset
 * @return Pointer to the sspt_page struct
 */
struct sspt_page *sspt_get_page(struct sspt_file *file, unsigned long offset_addr)
{
	unsigned long offset = offset_addr & PAGE_MASK;
	struct sspt_page *page = sspt_find_page_or_new(file, offset);

	spin_lock(&page->lock);

	return page;
}

/**
 * @brief Put sspt_page (unlook)
 *
 * @param file Pointer to the sspt_page struct
 * @return void
 */
void sspt_put_page(struct sspt_page *page)
{
	spin_unlock(&page->lock);
}

/**
 * @brief Check install sspt_file (legacy code, it is need remove)
 *
 * @param file Pointer to the sspt_file struct
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int sspt_file_check_install_pages(struct sspt_file *file)
{
	int i, table_size;
	struct sspt_page *page;
	struct hlist_head *head;
	struct hlist_node *tmp;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	table_size = (1 << file->page_probes_hash_bits);
	for (i = 0; i < table_size; ++i) {
		head = &file->page_probes_table[i];
		swap_hlist_for_each_entry_safe(page, node, tmp, head, hlist) {
			if (sspt_page_is_installed(page)) {
				return 1;
			}
		}
	}

	return 0;
}

/**
 * @brief Install sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @return Void
 */
void sspt_file_install(struct sspt_file *file)
{
	struct sspt_page *page = NULL;
	struct hlist_head *head = NULL;
	int i, table_size = (1 << file->page_probes_hash_bits);
	unsigned long page_addr;
	struct mm_struct *mm;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	for (i = 0; i < table_size; ++i) {
		head = &file->page_probes_table[i];
		swap_hlist_for_each_entry_rcu(page, node, head, hlist) {
			page_addr = file->vm_start + page->offset;
			if (page_addr < file->vm_start ||
			    page_addr >= file->vm_end)
				continue;

			mm = page->file->proc->task->mm;
			if (page_present(mm, page_addr))
				sspt_register_page(page, file);
		}
	}
}

/**
 * @brief Uninstall sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param task Pointer to the task_stract struct
 * @param flag Action for probes
 * @return Void
 */
int sspt_file_uninstall(struct sspt_file *file, struct task_struct *task, enum US_FLAGS flag)
{
	int i, err = 0;
	int table_size = (1 << file->page_probes_hash_bits);
	struct sspt_page *page;
	struct hlist_head *head;
	struct hlist_node *tmp;
	DECLARE_NODE_PTR_FOR_HLIST(node);

	for (i = 0; i < table_size; ++i) {
		head = &file->page_probes_table[i];
		swap_hlist_for_each_entry_safe (page, node, tmp, head, hlist) {
			err = sspt_unregister_page(page, flag, task);
			if (err != 0) {
				printk("ERROR sspt_file_uninstall: err=%d\n", err);
				return err;
			}
		}
	}

	if (flag != US_DISARM) {
		file->loaded = 0;
	}

	return err;
}

/**
 * @brief Set mapping for sspt_file
 *
 * @param file Pointer to the sspt_file struct
 * @param vma Pointer to the vm_area_struct struct
 * @return Void
 */
void sspt_file_set_mapping(struct sspt_file *file, struct vm_area_struct *vma)
{
	file->vm_start = vma->vm_start;
	file->vm_end = vma->vm_end;

//	ptr_pack_task_event_info(task, DYN_LIB_PROBE_ID, RECORD_ENTRY, "dspdd",
//				 task->tgid, file->dentry->d_iname, vma->vm_start,
//				 vma->vm_end - vma->vm_start, 0);
}
