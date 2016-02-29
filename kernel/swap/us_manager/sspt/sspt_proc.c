/*
 *  Dynamic Binary Instrumentation Module based on KProbes
 *  modules/driver/sspt/sspt_proc.c
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
#include "sspt_proc.h"
#include "sspt_page.h"
#include "sspt_feature.h"
#include "sspt_filter.h"
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <us_manager/us_slot_manager.h>
#include <writer/swap_writer_module.h>


#ifdef CONFIG_ARM
#define mm_read_lock(task, mm, atomic, lock)			\
	mm = task->mm;						\
	lock = 0

#define mm_read_unlock(mm, atomic, lock)
#else /* CONFIG_ARM */
#define mm_read_lock(task, mm, atomic, lock)			\
	mm = atomic ? task->active_mm : get_task_mm(task); 	\
	if (mm == NULL) {					\
		/* FIXME: */					\
		panic("ERRR mm_read_lock: mm == NULL\n");	\
	}							\
								\
	if (atomic) {						\
		lock = down_read_trylock(&mm->mmap_sem);	\
	} else {						\
		lock = 1;					\
		down_read(&mm->mmap_sem);			\
	}

#define mm_read_unlock(mm, atomic, lock) 			\
	if (lock) {						\
		up_read(&mm->mmap_sem);				\
	}							\
								\
	if (!atomic) {						\
		mmput(mm);					\
	}
#endif /* CONFIG_ARM */


static LIST_HEAD(proc_probes_list);
static DEFINE_RWLOCK(sspt_proc_rwlock);

void sspt_proc_del_all_filters(struct sspt_proc *proc);

/**
 * @brief Global read lock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_read_lock(void)
{
	read_lock(&sspt_proc_rwlock);
}

/**
 * @brief Global read unlock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_read_unlock(void)
{
	read_unlock(&sspt_proc_rwlock);
}

/**
 * @brief Global write lock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_write_lock(void)
{
	write_lock(&sspt_proc_rwlock);
}

/**
 * @brief Global write unlock for sspt_proc
 *
 * @return Void
 */
void sspt_proc_write_unlock(void)
{
	write_unlock(&sspt_proc_rwlock);
}


/**
 * @brief Create sspt_proc struct
 *
 * @param task Pointer to the task_struct struct
 * @param priv Private data
 * @return Pointer to the created sspt_proc struct
 */
struct sspt_proc *sspt_proc_create(struct task_struct *task, void *priv)
{
	struct sspt_proc *proc = kmalloc(sizeof(*proc), GFP_ATOMIC);

	if (proc) {
		proc->feature = sspt_create_feature();
		if (proc->feature == NULL) {
			kfree(proc);
			return NULL;
		}

		INIT_LIST_HEAD(&proc->list);
		proc->tgid = task->tgid;
		proc->task = task->group_leader;
		proc->sm = create_sm_us(task);
		proc->first_install = 0;
		INIT_LIST_HEAD(&proc->file_list);
		INIT_LIST_HEAD(&proc->filter_list);

		/* add to list */
		list_add(&proc->list, &proc_probes_list);
	}

	return proc;
}

/**
 * @brief Remove sspt_proc struct
 *
 * @param proc remove object
 * @return Void
 */

/* called with sspt_proc_write_lock() */
void sspt_proc_free(struct sspt_proc *proc)
{
	struct sspt_file *file, *n;

	/* delete from list */
	list_del(&proc->list);

	list_for_each_entry_safe(file, n, &proc->file_list, list) {
		list_del(&file->list);
		sspt_file_free(file);
	}

	sspt_destroy_feature(proc->feature);

	terminate_msg(proc->task);
	free_sm_us(proc->sm);
	kfree(proc);
}

/**
 * @brief Get sspt_proc by task
 *
 * @param task Pointer on the task_struct struct
 * @return Pointer on the sspt_proc struct
 */
struct sspt_proc *sspt_proc_get_by_task(struct task_struct *task)
{
	struct sspt_proc *proc, *tmp;

	list_for_each_entry_safe(proc, tmp, &proc_probes_list, list) {
		if (proc->tgid == task->tgid) {
			return proc;
		}
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(sspt_proc_get_by_task);

/**
 * @brief Call func() on each proc (no lock)
 *
 * @param func Callback
 * @param data Data for callback
 * @return Void
 */
void on_each_proc_no_lock(void (*func)(struct sspt_proc *, void *), void *data)
{
	struct sspt_proc *proc, *tmp;

	list_for_each_entry_safe(proc, tmp, &proc_probes_list, list) {
		func(proc, data);
	}
}

/**
 * @brief Call func() on each proc
 *
 * @param func Callback
 * @param data Data for callback
 * @return Void
 */
void on_each_proc(void (*func)(struct sspt_proc *, void *), void *data)
{
	sspt_proc_read_lock();
	on_each_proc_no_lock(func, data);
	sspt_proc_read_unlock();
}
EXPORT_SYMBOL_GPL(on_each_proc);

/**
 * @brief Get sspt_proc by task or create sspt_proc
 *
 * @param task Pointer on the task_struct struct
 * @param priv Private data
 * @return Pointer on the sspt_proc struct
 */
struct sspt_proc *sspt_proc_get_by_task_or_new(struct task_struct *task,
					       void *priv)
{
	struct sspt_proc *proc = sspt_proc_get_by_task(task);
	if (proc == NULL) {
		proc = sspt_proc_create(task, priv);
	}

	return proc;
}

/**
 * @brief Free all sspt_proc
 *
 * @return Pointer on the sspt_proc struct
 */
void sspt_proc_free_all(void)
{
	struct sspt_proc *proc, *n;
	list_for_each_entry_safe(proc, n, &proc_probes_list, list) {
		sspt_proc_del_all_filters(proc);
		sspt_proc_free(proc);
	}
}

static void sspt_proc_add_file(struct sspt_proc *proc, struct sspt_file *file)
{
	list_add(&file->list, &proc->file_list);
	file->proc = proc;
}

/**
 * @brief Get sspt_file from sspt_proc by dentry or new
 *
 * @param proc Pointer on the sspt_proc struct
 * @param dentry Dentry of file
 * @return Pointer on the sspt_file struct
 */
struct sspt_file *sspt_proc_find_file_or_new(struct sspt_proc *proc,
					     struct dentry *dentry)
{
	struct sspt_file *file;

	file = sspt_proc_find_file(proc, dentry);
	if (file == NULL) {
		file = sspt_file_create(dentry, 10);
		if (file)
			sspt_proc_add_file(proc, file);
	}

	return file;
}

/**
 * @brief Get sspt_file from sspt_proc by dentry
 *
 * @param proc Pointer on the sspt_proc struct
 * @param dentry Dentry of file
 * @return Pointer on the sspt_file struct
 */
struct sspt_file *sspt_proc_find_file(struct sspt_proc *proc, struct dentry *dentry)
{
	struct sspt_file *file;

	list_for_each_entry(file, &proc->file_list, list) {
		if (dentry == file->dentry) {
			return file;
		}
	}

	return NULL;
}

/**
 * @brief Install probes on the page to monitored process
 *
 * @param proc Pointer on the sspt_proc struct
 * @param page_addr Page address
 * @return Void
 */
void sspt_proc_install_page(struct sspt_proc *proc, unsigned long page_addr)
{
	int lock, atomic;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct task_struct *task = proc->task;

	atomic = in_atomic();
	mm_read_lock(task, mm, atomic, lock);

	vma = find_vma_intersection(mm, page_addr, page_addr + 1);
	if (vma && check_vma(vma)) {
		struct dentry *dentry = vma->vm_file->f_dentry;
		struct sspt_file *file = sspt_proc_find_file(proc, dentry);
		if (file) {
			struct sspt_page *page;
			if (!file->loaded) {
				sspt_file_set_mapping(file, vma);
				file->loaded = 1;
			}

			page = sspt_find_page_mapped(file, page_addr);
			if (page) {
				sspt_register_page(page, file);
			}
		}
	}

	mm_read_unlock(mm, atomic, lock);
}

/**
 * @brief Install probes to monitored process
 *
 * @param proc Pointer on the sspt_proc struct
 * @return Void
 */
void sspt_proc_install(struct sspt_proc *proc)
{
	int lock, atomic;
	struct vm_area_struct *vma;
	struct task_struct *task = proc->task;
	struct mm_struct *mm;

	proc->first_install = 1;

	atomic = in_atomic();
	mm_read_lock(task, mm, atomic, lock);

	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (check_vma(vma)) {
			struct dentry *dentry = vma->vm_file->f_dentry;
			struct sspt_file *file = sspt_proc_find_file(proc, dentry);
			if (file) {
				if (!file->loaded) {
					file->loaded = 1;
					sspt_file_set_mapping(file, vma);
				}

				sspt_file_install(file);
			}
		}
	}

	mm_read_unlock(mm, atomic, lock);
}

/**
 * @brief Uninstall probes to monitored process
 *
 * @param proc Pointer on the sspt_proc struct
 * @param task Pointer on the task_struct struct
 * @param flag Action for probes
 * @return Error code
 */
int sspt_proc_uninstall(struct sspt_proc *proc, struct task_struct *task, enum US_FLAGS flag)
{
	int err = 0;
	struct sspt_file *file;

	list_for_each_entry_rcu(file, &proc->file_list, list) {
		err = sspt_file_uninstall(file, task, flag);
		if (err != 0) {
			printk("ERROR sspt_proc_uninstall: err=%d\n", err);
			return err;
		}
	}

	return err;
}

static int intersection(unsigned long start_a, unsigned long end_a,
			unsigned long start_b, unsigned long end_b)
{
	return start_a < start_b ?
			end_a > start_b :
			start_a < end_b;
}

/**
 * @brief Get sspt_file list by region (remove sspt_file from sspt_proc list)
 *
 * @param proc Pointer on the sspt_proc struct
 * @param head[out] Pointer on the head list
 * @param start Region start
 * @param len Region length
 * @return Error code
 */
int sspt_proc_get_files_by_region(struct sspt_proc *proc,
				  struct list_head *head,
				  unsigned long start, size_t len)
{
	int ret = 0;
	struct sspt_file *file, *n;
	unsigned long end = start + len;

	list_for_each_entry_safe(file, n, &proc->file_list, list) {
		if (intersection(file->vm_start, file->vm_end, start, end)) {
			ret = 1;
			list_move(&file->list, head);
		}
	}

	return ret;
}

/**
 * @brief Insert sspt_file to sspt_proc list
 *
 * @param proc Pointer on the sspt_proc struct
 * @param head Pointer on the head list
 * @return Void
 */
void sspt_proc_insert_files(struct sspt_proc *proc, struct list_head *head)
{
	list_splice(head, &proc->file_list);
}

/**
 * @brief Add sspt_filter to sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @param pfg Pointer to pf_group struct
 * @return Void
 */
void sspt_proc_add_filter(struct sspt_proc *proc, struct pf_group *pfg)
{
	struct sspt_filter *fl;

	fl = sspt_filter_create(pfg);
	if (fl == NULL)
		return;

	list_add(&fl->list, &proc->filter_list);
}

/**
 * @brief Remove sspt_filter from sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @param pfg Pointer to pf_group struct
 * @return Void
 */
void sspt_proc_del_filter(struct sspt_proc *proc, struct pf_group *pfg)
{
	struct sspt_filter *fl, *tmp;

	list_for_each_entry_safe(fl, tmp, &proc->filter_list, list) {
		if (fl->pfg == pfg) {
			list_del(&fl->list);
			sspt_filter_free(fl);
		}
	}
}

/**
 * @brief Remove all sspt_filters from sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @return Void
 */
void sspt_proc_del_all_filters(struct sspt_proc *proc)
{
	struct sspt_filter *fl, *tmp;

	list_for_each_entry_safe(fl, tmp, &proc->filter_list, list) {
		list_del(&fl->list);
		sspt_filter_free(fl);
	}
}

/**
 * @brief Check if sspt_filter is already in sspt_proc list
 *
 * @param proc Pointer to sspt_proc struct
 * @param pfg Pointer to pf_group struct
 * @return Boolean
 */
int sspt_proc_is_filter_new(struct sspt_proc *proc, struct pf_group *pfg)
{
	struct sspt_filter *fl;

	list_for_each_entry(fl, &proc->filter_list, list)
		if (fl->pfg == pfg)
			return 0;

	return 1;
}
