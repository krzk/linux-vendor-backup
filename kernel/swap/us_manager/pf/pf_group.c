/*
 *  SWAP uprobe manager
 *  modules/us_manager/pf/pf_group.c
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


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include "proc_filters.h"
#include <us_manager/img/img_proc.h>
#include <us_manager/img/img_file.h>
#include <us_manager/img/img_ip.h>
#include <us_manager/sspt/sspt_proc.h>
#include <us_manager/helper.h>
#include <writer/swap_writer_module.h>

struct pf_group {
	struct list_head list;
	struct img_proc *i_proc;
	struct proc_filter filter;

	/* TODO: proc_list*/
	struct list_head proc_list;
};

struct pl_struct {
	struct list_head list;
	struct sspt_proc *proc;
};

static LIST_HEAD(pfg_list);

/* struct pl_struct */
static struct pl_struct *create_pl_struct(struct sspt_proc *proc)
{
	struct pl_struct *pls = kmalloc(sizeof(*pls), GFP_KERNEL);
	if (pls) {
		INIT_LIST_HEAD(&pls->list);
		pls->proc = proc;
	} else {
		printk("Cannot allocate memory for pl_struct\n");
	}
	return pls;
}

static void free_pl_struct(struct pl_struct *pls)
{
	kfree(pls);
}

static void add_pl_struct(struct pf_group *pfg, struct pl_struct *pls)
{
	list_add(&pls->list, &pfg->proc_list);
}

static void del_pl_struct(struct pl_struct *pls)
{
	list_del(&pls->list);
}

void copy_proc_form_img_to_sspt(struct img_proc *i_proc, struct sspt_proc *proc)
{
	struct sspt_file *file;
	struct img_file *i_file;
	struct img_ip *i_ip;

	list_for_each_entry(i_file, &i_proc->file_list, list) {
		file = sspt_proc_find_file_or_new(proc, i_file->dentry);
		if (file == NULL) {
			pr_warn("Cannot alloc memory for sspt_file struct!\n");
			return;
		}

		list_for_each_entry(i_ip, &i_file->ip_list, list)
			sspt_file_add_ip(file, i_ip->addr, i_ip->args,
					 i_ip->ret_type);
	}
}

static struct pl_struct *find_pl_struct(struct pf_group *pfg,
					struct task_struct *task)
{
	struct pl_struct *pls;

	list_for_each_entry(pls, &pfg->proc_list, list) {
		if (pls->proc->tgid == task->tgid)
			return pls;
	}

	return NULL;
}

static struct sspt_proc *get_proc_by_pfg(struct pf_group *pfg,
					 struct task_struct *task)
{
	struct pl_struct *pls;

	pls = find_pl_struct(pfg, task);
	if (pls)
		return pls->proc;

	return NULL;
}

static struct sspt_proc *new_proc_by_pfg(struct pf_group *pfg,
					 struct task_struct *task)
{
	struct pl_struct *pls;
	struct sspt_proc *proc;

	proc = sspt_proc_get_by_task_or_new(task, pfg->filter.priv);
	if (proc) {
		copy_proc_form_img_to_sspt(pfg->i_proc, proc);
		sspt_proc_add_filter(proc, pfg);

		pls = create_pl_struct(proc);
		add_pl_struct(pfg, pls);
	} else {
		printk("sspt_proc_get_by_task return NULL\n");
	}
	return proc;
}
/* struct pl_struct */

static struct pf_group *create_pfg(void)
{
	struct pf_group *pfg = kmalloc(sizeof(*pfg), GFP_KERNEL);

	if (pfg == NULL)
		return NULL;

	pfg->i_proc = create_img_proc();
	if (pfg->i_proc == NULL)
		goto create_pfg_fail;

	INIT_LIST_HEAD(&pfg->list);
	memset(&pfg->filter, 0, sizeof(pfg->filter));
	INIT_LIST_HEAD(&pfg->proc_list);

	return pfg;

create_pfg_fail:

	kfree(pfg);

	return NULL;
}

static void free_pfg(struct pf_group *pfg)
{
	struct pl_struct *pl;

	free_img_proc(pfg->i_proc);
	free_pf(&pfg->filter);
	list_for_each_entry(pl, &pfg->proc_list, list)
		sspt_proc_del_filter(pl->proc, pfg);
	kfree(pfg);
}

static void add_pfg_by_list(struct pf_group *pfg)
{
	list_add(&pfg->list, &pfg_list);
}

static void del_pfg_by_list(struct pf_group *pfg)
{
	list_del(&pfg->list);
}

static void first_install(struct task_struct *task, struct sspt_proc *proc,
			  struct pf_group *pfg)
{
	struct dentry *dentry;

	dentry = (struct dentry *)get_pf_priv(&pfg->filter);
	if (dentry == NULL) {
		dentry = task->mm->exe_file ?
			 task->mm->exe_file->f_dentry :
			 NULL;
	}

	down_read(&task->mm->mmap_sem);
	proc_info_msg(task, dentry);
	up_read(&task->mm->mmap_sem);

#ifdef CONFIG_ARM
	down_write(&task->mm->mmap_sem);
	sspt_proc_install(proc);
	up_write(&task->mm->mmap_sem);
#else /* CONFIG_ARM */
	sspt_proc_install(proc);
#endif /* CONFIG_ARM */
}

static void subsequent_install(struct task_struct *task,
			       struct sspt_proc *proc, unsigned long page_addr)
{
	if (!page_addr)
		return;
#ifdef CONFIG_ARM
	down_write(&task->mm->mmap_sem);
	sspt_proc_install_page(proc, page_addr);
	up_write(&task->mm->mmap_sem);
#else /* CONFIG_ARM */
	sspt_proc_install_page(proc, page_addr);
#endif /* CONFIG_ARM */
}

/**
 * @brief Get pf_group struct by dentry
 *
 * @param dentry Dentry of file
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_by_dentry(struct dentry *dentry, void *priv)
{
	struct pf_group *pfg;

	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_by_dentry(&pfg->filter, dentry))
			return pfg;
	}

	pfg = create_pfg();
	if (pfg == NULL)
		return NULL;

	set_pf_by_dentry(&pfg->filter, dentry, priv);

	add_pfg_by_list(pfg);

	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_by_dentry);

/**
 * @brief Get pf_group struct by TGID
 *
 * @param tgid Thread group ID
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_by_tgid(pid_t tgid, void *priv)
{
	struct pf_group *pfg;

	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_by_tgid(&pfg->filter, tgid))
			return pfg;
	}

	pfg = create_pfg();
	if (pfg == NULL)
		return NULL;

	set_pf_by_tgid(&pfg->filter, tgid, priv);

	add_pfg_by_list(pfg);

	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_by_tgid);

/**
 * @brief Get pf_group struct by comm
 *
 * @param comm Task comm
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_by_comm(char *comm, void *priv)
{
	struct pf_group *pfg;

	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_by_comm(&pfg->filter, comm))
			return pfg;
	}

	pfg = create_pfg();
	if (pfg == NULL)
		return NULL;

	set_pf_by_comm(&pfg->filter, comm, priv);

	add_pfg_by_list(pfg);

	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_by_comm);

/**
 * @brief Get pf_group struct for each process
 *
 * @param priv Private data
 * @return Pointer on pf_group struct
 */
struct pf_group *get_pf_group_dumb(void *priv)
{
	struct pf_group *pfg;

	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_pf_dumb(&pfg->filter))
			return pfg;
	}

	pfg = create_pfg();
	if (pfg == NULL)
		return NULL;

	set_pf_dumb(&pfg->filter, priv);

	add_pfg_by_list(pfg);

	return pfg;
}
EXPORT_SYMBOL_GPL(get_pf_group_dumb);

/**
 * @brief Put pf_group struct
 *
 * @param pfg Pointer to the pf_group struct
 * @return Void
 */
void put_pf_group(struct pf_group *pfg)
{

}

/**
 * @brief Register prober for pf_grpup struct
 *
 * @param pfg Pointer to the pf_group struct
 * @param dentry Dentry of file
 * @param offset Function offset
 * @param args Function arguments
 * @param ret_type Return type
 * @return Error code
 */
int pf_register_probe(struct pf_group *pfg, struct dentry *dentry,
		      unsigned long offset, const char *args, char ret_type)
{
	return img_proc_add_ip(pfg->i_proc, dentry, offset, args, ret_type);
}
EXPORT_SYMBOL_GPL(pf_register_probe);

/**
 * @brief Unregister prober from pf_grpup struct
 *
 * @param pfg Pointer to the pf_group struct
 * @param dentry Dentry of file
 * @param offset Function offset
 * @return Error code
 */
int pf_unregister_probe(struct pf_group *pfg, struct dentry *dentry,
			unsigned long offset)
{
	return img_proc_del_ip(pfg->i_proc, dentry, offset);
}
EXPORT_SYMBOL_GPL(pf_unregister_probe);

/**
 * @brief Check the task, to meet the filter criteria
 *
 * @prarm task Pointer on the task_struct struct
 * @return
 *       - 0 - false
 *       - 1 - true
 */
int check_task_on_filters(struct task_struct *task)
{
	struct pf_group *pfg;

	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_task_f(&pfg->filter, task))
			return 1;
	}

	return 0;
}

/**
 * @brief Check task and install probes on demand
 *
 * @prarm task Pointer on the task_struct struct
 * @return Void
 */
void check_task_and_install(struct task_struct *task)
{
	struct pf_group *pfg;
	struct sspt_proc *proc = NULL;

	list_for_each_entry(pfg, &pfg_list, list) {
		if (check_task_f(&pfg->filter, task) == NULL)
			continue;

		proc = get_proc_by_pfg(pfg, task);
		if (proc) {
			if (sspt_proc_is_filter_new(proc, pfg)) {
				copy_proc_form_img_to_sspt(pfg->i_proc, proc);
				sspt_proc_add_filter(proc, pfg);
			} else {
				printk(KERN_ERR "SWAP US_MANAGER: Error! Trying"
						" to first install filter that "
						"already exists in proc!\n");
				return;
			}
			break;
		}

		if (task->tgid == task->pid) {
			proc = new_proc_by_pfg(pfg, task);
			break;
		}
	}

	if (proc)
		first_install(task, proc, pfg);
}

/**
 * @brief Check task and install probes on demand
 *
 * @prarm task Pointer on the task_struct struct
 * @param page_addr Page fault address
 * @return Void
 */
void call_page_fault(struct task_struct *task, unsigned long page_addr)
{
	struct pf_group *pfg, *pfg_first = NULL;
	struct sspt_proc *proc = NULL;

	list_for_each_entry(pfg, &pfg_list, list) {
		if ((check_task_f(&pfg->filter, task) == NULL))
			continue;

		proc = get_proc_by_pfg(pfg, task);
		if (proc) {
			if (sspt_proc_is_filter_new(proc, pfg)) {
				copy_proc_form_img_to_sspt(pfg->i_proc, proc);
				sspt_proc_add_filter(proc, pfg);
			}
			break;
		}

		if (task->tgid == task->pid) {
			proc = new_proc_by_pfg(pfg, task);
			pfg_first = pfg;
			break;
		}
	}

	if (proc) {
		if (pfg_first) {
			first_install(task, proc, pfg_first);
		} else {
			subsequent_install(task, proc, page_addr);
		}
	}
}

/**
 * @brief Uninstall probes from the sspt_proc struct
 *
 * @prarm proc Pointer on the sspt_proc struct
 * @return Void
 */

/* called with sspt_proc_write_lock() */
void uninstall_proc(struct sspt_proc *proc)
{
	struct task_struct *task = proc->task;
	struct pf_group *pfg;
	struct pl_struct *pls;

	list_for_each_entry(pfg, &pfg_list, list) {
		pls = find_pl_struct(pfg, task);
		if (pls) {
			del_pl_struct(pls);
			free_pl_struct(pls);
		}
	}

	task_lock(task);
	BUG_ON(task->mm == NULL);
	sspt_proc_uninstall(proc, task, US_UNREGS_PROBE);
	task_unlock(task);

	sspt_proc_del_all_filters(proc);
	sspt_proc_free(proc);
}

/**
 * @brief Remove probes from the task on demand
 *
 * @prarm task Pointer on the task_struct struct
 * @return Void
 */
void call_mm_release(struct task_struct *task)
{
	struct sspt_proc *proc;

	sspt_proc_write_lock();

	proc = sspt_proc_get_by_task(task);
	if (proc)
		/* TODO: uninstall_proc - is not atomic context */
		uninstall_proc(proc);

	sspt_proc_write_unlock();
}

/**
 * @brief Legacy code, it is need remove
 *
 * @param addr Page address
 * @return Void
 */
void uninstall_page(unsigned long addr)
{

}

/**
 * @brief Install probes on running processes
 *
 * @return Void
 */
void install_all(void)
{
#if !defined(CONFIG_ARM)
	struct task_struct *task;
	int tmp_oops_in_progress;

	tmp_oops_in_progress = oops_in_progress;
	oops_in_progress = 1;
	rcu_read_lock();
	for_each_process(task) {
		if (task->tgid != task->pid)
			continue;

		if (is_kthread(task))
			continue;

		check_task_and_install(task);
	}
	rcu_read_unlock();
	oops_in_progress = tmp_oops_in_progress;
#endif /* CONFIG_ARM */
}

static void clean_pfg(void)
{
	struct pf_group *pfg, *n;

	list_for_each_entry_safe(pfg, n, &pfg_list, list) {
		del_pfg_by_list(pfg);
		free_pfg(pfg);
	}
}

static void on_each_uninstall_proc(struct sspt_proc *proc, void *data)
{
	uninstall_proc(proc);
}

/**
 * @brief Uninstall probes from all processes
 *
 * @return Void
 */
void uninstall_all(void)
{
	sspt_proc_write_lock();
	on_each_proc_no_lock(on_each_uninstall_proc, NULL);
	sspt_proc_write_unlock();

	clean_pfg();
}

/**
 * @brief For debug
 *
 * @param pfg Pointer to the pf_group struct
 * @return Void
 */

/* debug */
void pfg_print(struct pf_group *pfg)
{
	img_proc_print(pfg->i_proc);
}
EXPORT_SYMBOL_GPL(pfg_print);
/* debug */
