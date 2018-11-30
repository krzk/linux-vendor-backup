/*
 * To log which process used alarm.
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
 * Jeongsup Jeong <jeongsup.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/suspend.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/sort.h>
#include <linux/power/energy_monitor.h>
#endif

#include "alarm_history.h"

struct alarm_history {
	struct list_head entry;
	spinlock_t lock;

	unsigned int pid;
	char name[ALMH_NAME_LENGTH];

	ktime_t last_create_time;
	ktime_t last_expire_time;
	ktime_t last_wakeup_time;

	unsigned long create_count;
	unsigned long expire_count;
	unsigned long wakeup_count;

#ifdef CONFIG_ENERGY_MONITOR
	unsigned long emon_wakeup_count;
	unsigned long emon_expire_count;
#endif
};

static LIST_HEAD(almh_list);
static struct dentry *alarm_history_dentry;
static unsigned int rtc_irq;

#ifdef CONFIG_ENERGY_MONITOR
static int alarm_history_emon_cmp_func(const void *a, const void *b)
{
	struct alarm_expire_stat *pa = (struct alarm_expire_stat *)(a);
	struct alarm_expire_stat *pb = (struct alarm_expire_stat *)(b);
	return (pb->wakeup_count - pa->wakeup_count);
}

void alarm_history_get_stat(int type,
	struct alarm_expire_stat *alarm_expire, size_t n)
{
	struct alarm_history *iter;
	unsigned long wakeup_count, expire_count;
	int i = 0;

	for (i = 0; i < n; i++) {
		memset(&alarm_expire[i].comm, 0, TASK_COMM_LEN);
		alarm_expire[i].wakeup_count = 0;
		alarm_expire[i].expire_count = 0;
	}

	i = 0;
	rcu_read_lock();
	list_for_each_entry_rcu(iter, &almh_list, entry) {
		wakeup_count = iter->wakeup_count - iter->emon_wakeup_count;
		expire_count = iter->expire_count - iter->emon_expire_count;

		if (expire_count == 0)
			continue;

		if (i < n) {
			memcpy(&alarm_expire[i].comm, &iter->name, ALMH_NAME_LENGTH);
			alarm_expire[i].wakeup_count = wakeup_count;
			alarm_expire[i].expire_count = expire_count;

			i++;
			if (i == n)
				sort(&alarm_expire[0],
					n,
					sizeof(struct alarm_expire_stat),
					alarm_history_emon_cmp_func, NULL);
		} else {
			if (wakeup_count > alarm_expire[n-1].wakeup_count) {
				memcpy(&alarm_expire[n-1].comm, &iter->name, ALMH_NAME_LENGTH);
				alarm_expire[n-1].wakeup_count = wakeup_count;
				alarm_expire[n-1].expire_count = expire_count;

				sort(&alarm_expire[0],
					n,
					sizeof(struct alarm_expire_stat),
					alarm_history_emon_cmp_func, NULL);
			}
		}

		if (type != ENERGY_MON_TYPE_DUMP) {
			iter->emon_wakeup_count = iter->wakeup_count;
			iter->emon_expire_count = iter->expire_count;
		}
	}
	rcu_read_unlock();

	if (i < n && i != 0)
		sort(&alarm_expire[0],
			n,
			sizeof(struct alarm_expire_stat),
			alarm_history_emon_cmp_func, NULL);
}
#endif

/* Initial alarm history list */
static void init_alarm_history(struct alarm_history *almh)
{
	spin_lock_init(&almh->lock);

	almh->pid = 0;
	memset(almh->name, 0, ALMH_NAME_LENGTH);

	almh->last_create_time = ktime_set(0, 0);
	almh->last_expire_time = ktime_set(0, 0);
	almh->last_wakeup_time = ktime_set(0, 0);

	almh->create_count= 0;
	almh->expire_count= 0;
	almh->wakeup_count= 0;
#ifdef CONFIG_ENERGY_MONITOR

	almh->emon_wakeup_count = 0;
	almh->emon_expire_count = 0;
#endif
}

void alarm_history_set_rtc_irq(void)
{
	rtc_irq = 1;
}


/* Add alarm history to list */
int add_alarm_history(int type, unsigned int pid)
{
	struct task_struct *task;
	char name[ALMH_NAME_LENGTH] = {0};
	struct alarm_history *almh, *iter;
	int exist = 0, wakeup = 0;

	/* Get name */
	rcu_read_lock();
	task = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (task)
		strncpy(name, task->comm, strlen(task->comm) + 1);
	else
		strncpy(name, "no_task", strlen("no_task") + 1);

	pr_debug("[ALMH] %s: name:%s, pid: %u\n",
				__func__, name, pid);

	/* Searching by name or pid */
	rcu_read_lock();
	if (!strncmp(name, "no_task", strlen("no_task"))) {
		list_for_each_entry_rcu(iter, &almh_list, entry) {
			if (iter->pid == pid) {
				exist = 1;
				break;
			}
		}
	} else {
		list_for_each_entry_rcu(iter, &almh_list, entry) {
			if (!strncmp(iter->name, name, ALMH_NAME_LENGTH - 1)) {
				exist = 1;
				break;
			}
		}
	}
	rcu_read_unlock();

	if(exist) {
		/* Update flag and time */
		rcu_read_lock();
		if (type == ALARM_HISTORY_TYPE_CREATE) {
			iter->create_count++;
			iter->last_create_time = ktime_get();
		}
		else if (type == ALARM_HISTORY_TYPE_EXPIRE) {
			if (rtc_irq) {
				iter->wakeup_count++;
				iter->last_wakeup_time = ktime_get();
				rtc_irq = 0;
			}
			iter->expire_count++;
			iter->last_expire_time = ktime_get();
		}
		else
 			pr_err("[ALMH] type error: %d\n", type);
		rcu_read_unlock();
	} else {
		/* Add new instance */
		almh = kmalloc(sizeof(struct alarm_history), GFP_KERNEL);
		if (!almh) {
			pr_err("[ALMH] fail to get new instance");
			return -ENOMEM;
		}

		init_alarm_history(almh);

		almh->pid = pid;
		strncpy(almh->name, name, ALMH_NAME_LENGTH - 1);
		almh->name[ALMH_NAME_LENGTH - 1] = 0;

		if (type == ALARM_HISTORY_TYPE_CREATE) {
			almh->create_count++;
			almh->last_create_time = ktime_get();
		}
		else if (type == ALARM_HISTORY_TYPE_EXPIRE) {
			if (rtc_irq) {
				almh->wakeup_count++;
				almh->last_wakeup_time = ktime_get();
				rtc_irq = 0;
				wakeup = 1;
			}
			almh->expire_count++;
			almh->last_expire_time = ktime_get();
		}
		else
			pr_err("[ALMH] type error: %d\n", type);

		rcu_read_lock();
		list_add_tail(&almh->entry, &almh_list);
		rcu_read_unlock();
	}

	if (exist)
		almh = iter;

	if (type == ALARM_HISTORY_TYPE_CREATE)
		pr_info("[ALMH] create[%d]: %s(%d)\n",
			(int)almh->create_count, almh->name, almh->pid);
	else if (wakeup)
		pr_info("[ALMH] expire[%d]+wakeup[%d]: %s(%d)\n",
			(int)almh->expire_count,
			(int)almh->wakeup_count, almh->name, almh->pid);
	else
		pr_info("[ALMH] expire[%d]: %s(%d)\n",
			(int)almh->expire_count, almh->name, almh->pid);

	return 0;
}

static int print_alarm_history(struct seq_file *m,
		struct alarm_history *almh, unsigned int idx)
{
	unsigned long flags;
	unsigned long create_count;
	unsigned long expire_count;
	unsigned long wakeup_count;
	int ret = 1;

	create_count = almh->create_count;
	expire_count = almh->expire_count;
	wakeup_count = almh->wakeup_count;

	spin_lock_irqsave(&almh->lock, flags);
	seq_printf(m,	"%-8d%-16s%-8u  "
			"%-12lu  %-12lu  %-12lu  "
			"%-12lld  %-12lld  %-12lld  ",
			idx, almh->name, almh->pid,
			create_count, expire_count, wakeup_count,
			ktime_to_ms(almh->last_create_time),
			ktime_to_ms(almh->last_expire_time),
			ktime_to_ms(almh->last_wakeup_time));
	seq_printf(m, "\n");
	spin_unlock_irqrestore(&almh->lock, flags);

	return ret;
}

static int alarm_history_show(struct seq_file *m, void *unused)
{
	struct alarm_history *iter;
	unsigned int idx = 1;

	seq_puts(m,	"idx     name            pid       "
			"create_count  expire_count  wakeup_count  "
			"last_create   last_expire   last_wakeup   ");
	seq_puts(m, "\n");

	rcu_read_lock();
	list_for_each_entry_rcu(iter, &almh_list, entry)
		print_alarm_history(m, iter, idx++);
	rcu_read_unlock();

	return 0;
}

static int alarm_history_open(struct inode *inode, struct file *file)
{
	return single_open(file, alarm_history_show, NULL);
}

static const struct  file_operations alarm_history_fops = {
	.owner = THIS_MODULE,
	.open = alarm_history_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int alarm_history_init(void)
{
	pr_debug("%s\n", __func__);

	alarm_history_dentry = debugfs_create_file("alarm_history",
		S_IRUGO, NULL, NULL, &alarm_history_fops);

	rtc_irq = 0;

	return 0;
}

static void alarm_history_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(alarm_history_init);
module_exit(alarm_history_exit);
