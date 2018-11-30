/*
 * Copyright (C) 2017 SAMSUNG, Inc.
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
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Hunsup Jung <hunsup.jungsamsung.com>        <2017.07.21>              *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 2.0   Junho Jang <vincent.jang@samsung.com>        <2018.05.04>              *
 *                                                   refactoring           *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/security.h>

#include <linux/lbs_history.h>
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/sort.h>
#include <linux/power/energy_monitor.h>
#endif

#define LBS_STAT_PREFIX "lbs_stat: "
#define MAX_BUFFER_SIZE 128
#define REQ_TYPE_LEN 16

struct lbs_stat {
	struct list_head list;
	u32 usid;
	u32 sid;
	uid_t uid;
	ktime_t total_time;
	ktime_t total_gps_time;
	struct lbs_usage usage[LBS_METHOD_MAX];

#ifdef CONFIG_ENERGY_MONITOR
	ktime_t emon_total_time;
	struct lbs_usage emon_usage[LBS_METHOD_MAX];
#endif
};

static LIST_HEAD(lbs_stat_list);
static DEFINE_SPINLOCK(lbs_stat_lock);

static char *lbs_method_str[] = {
	"GPS", /* 0 */
	"WPS", /* 1 */
	"AGPS", /* 2 */
	"GEOFENCE", /* 3 */
	"MOCK", /* 4 */
	"PASSIVE", /* 5 */
	"FUSED", /* 6 */
	"REMOTE_GPS", /* 7 */
	"REMOTE_WPS", /* 8 */
	"BAT::GPS", /* 9 */
	"BAT::REMOTE_GPS", /* 10 */
	"GPSBATCH" /* 11 */
	"UNKNOWN",
};

#ifdef CONFIG_ENERGY_MONITOR
static int lbs_stat_emon_cmp_func(const void *a, const void *b)
{
	struct lbs_stat_emon *pa = (struct lbs_stat_emon *)(a);
	struct lbs_stat_emon *pb = (struct lbs_stat_emon *)(b);
	return ktime_compare(pb->total_time, pa->total_time);
}

int lbs_stat_get_stat(int type, struct lbs_stat_emon *emon_stat, size_t n)
{
	int ret = 0;
	int i = 0, j;
	unsigned long flags;
	struct lbs_stat *entry;
	ktime_t total_time, active_time;
	ktime_t zero = ktime_set(0,0);
	struct lbs_usage temp[LBS_METHOD_MAX];

	if (!emon_stat)
		return -EINVAL;

	memset(emon_stat, 0, sizeof(struct lbs_stat_emon) * n);

	list_for_each_entry(entry, &lbs_stat_list, list) {
		total_time = zero;
		memset(temp, 0, sizeof(temp));

		for (j = 0; j < LBS_METHOD_MAX; j++) {
			if (entry->usage[j].active_count == 0)
				continue;

			total_time = ktime_add(total_time, entry->emon_usage[j].total_time);
			temp[j].total_time = entry->emon_usage[j].total_time;
			if (entry->usage[j].active) {
				ktime_t now = ktime_get();

				active_time = ktime_sub(now, entry->usage[j].last_time);
				total_time = ktime_add(total_time, active_time);
				temp[j].total_time = ktime_add(total_time, active_time);
			}
		}

		if (ktime_compare(total_time, zero) == 0)
			continue;

		if (i < n) {
			emon_stat[i].usid = entry->usid;
			emon_stat[i].uid = entry->uid;
			emon_stat[i].sid = entry->sid;
			emon_stat[i].total_time = total_time;
			memcpy(emon_stat[i].usage, temp, sizeof(temp));
			i++;
			if (i == n)
				sort(&emon_stat[0],
					n,
					sizeof(struct lbs_stat_emon),
					lbs_stat_emon_cmp_func, NULL);
		} else {

			if (ktime_compare(total_time,
					emon_stat[n-1].total_time) == 1) {
				emon_stat[n-1].usid = entry->usid;
				emon_stat[n-1].uid = entry->uid;
				emon_stat[n-1].sid = entry->sid;
				emon_stat[n-1].total_time = total_time;
				memcpy(emon_stat[i].usage, temp, sizeof(temp));
				sort(&emon_stat[0],
					n,
					sizeof(struct lbs_stat_emon),
					lbs_stat_emon_cmp_func, NULL);
			}
		}

		if (type != ENERGY_MON_TYPE_DUMP) {
			spin_lock_irqsave(&lbs_stat_lock, flags);
			entry->emon_total_time = zero;
			memset(entry->emon_usage, 0, sizeof(struct lbs_usage) * LBS_METHOD_MAX);
			spin_unlock_irqrestore(&lbs_stat_lock, flags);
		}
	}

	if (i < n && i != 0)
		sort(&emon_stat[0],
			n,
			sizeof(struct lbs_stat_emon),
			lbs_stat_emon_cmp_func, NULL);

	return ret;
}
#endif

static u32 find_usid(uid_t uid, u32 sid)
{
	u32 usid;

	if (uid >= 5000)
		usid = uid + sid;
	else
		usid = uid;

	return usid;
}

static char *get_lbs_method_str(int lbs_method)
{
	if (lbs_method < LBS_METHOD_MAX)
		return lbs_method_str[lbs_method];

	return NULL;
}

static int get_lbs_method(char *type)
{
	int i;

	for (i = 0; i < LBS_METHOD_MAX; i++) {
		if (!strcmp(type, lbs_method_str[i]))
			return i;
	}

	return -1;
}

static int lbs_stat_start(int lbs_method, u32 usid, uid_t uid, u32 sid)
{
	struct lbs_stat *lbs, *entry;
	unsigned long flags;

	/* Already added instance */
	spin_lock_irqsave(&lbs_stat_lock, flags);
	list_for_each_entry(entry, &lbs_stat_list, list) {
		if (entry->usid == usid) {
			entry->usage[lbs_method].active = 1;
			entry->usage[lbs_method].active_count++;
			entry->usage[lbs_method].last_time = ktime_get();

			spin_unlock_irqrestore(&lbs_stat_lock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&lbs_stat_lock, flags);

	/* Add new instance */
	lbs = kzalloc(sizeof(struct lbs_stat), GFP_KERNEL);
	if (!lbs)
		return -ENOMEM;

	lbs->usid = usid;
	lbs->uid = uid;
	lbs->sid = sid;

	lbs->usage[lbs_method].active = 1;
	lbs->usage[lbs_method].active_count++;
	lbs->usage[lbs_method].last_time = ktime_get();

	spin_lock_irqsave(&lbs_stat_lock, flags);
	list_add_tail(&lbs->list, &lbs_stat_list);
	spin_unlock_irqrestore(&lbs_stat_lock, flags);

	return 0;
}

static void lbs_stat_stop(int lbs_method, u32 usid, uid_t uid, u32 sid)
{
	struct lbs_stat *entry;
	unsigned long flags;
	ktime_t now, duration;

	spin_lock_irqsave(&lbs_stat_lock, flags);
	list_for_each_entry(entry, &lbs_stat_list, list) {
		if (entry->usid == usid) {
			entry->usage[lbs_method].active = 0;
			now = ktime_get();
			duration = ktime_sub(now, entry->usage[lbs_method].last_time);
			entry->usage[lbs_method].total_time = ktime_add(
				entry->usage[lbs_method].total_time, duration);
			entry->total_time = ktime_add(
				entry->total_time, duration);
			if (lbs_method == LBS_METHOD_GPS ||
				lbs_method == LBS_METHOD_BATCH_GPS) {
				entry->total_gps_time = ktime_add(
					entry->total_gps_time, duration);
			}
#ifdef CONFIG_ENERGY_MONITOR
			entry->emon_usage[lbs_method].total_time = ktime_add(
				entry->emon_usage[lbs_method].total_time, duration);
			entry->emon_total_time = ktime_add
				(entry->emon_total_time, duration);
#endif
			if (ktime_to_ns(duration) > ktime_to_ns(entry->usage[lbs_method].max_time)) {
				entry->usage[lbs_method].max_time = duration;
				entry->usage[lbs_method].max_time_stamp = now;
			}

			spin_unlock_irqrestore(&lbs_stat_lock, flags);
			return;
		}
	}
	spin_unlock_irqrestore(&lbs_stat_lock, flags);
}

static ssize_t lbs_stat_start_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct task_struct *task;
	char buf[MAX_BUFFER_SIZE] = {};
	char *str;

	char type[REQ_TYPE_LEN] = {0};
	int pid = 0, pos = -1;
	int lbs_method;

	uid_t uid;
	u32 usid, sid;

	/* Copy_from_user */
	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(&buf, user_buf, count))
		return -EFAULT;
	buf[count] = '\0';
	str = strstrip(buf);

	/* Request type length is 16 */
	if (sscanf(str, "%d %16s %n", &pid, type, &pos) != 2) {
		pr_err(LBS_STAT_PREFIX"Invalid number of arguments passed\n");
		return -EINVAL;
	}

	/* Check pid */
	if (pid == 0) {
		/* If there's no pid, return -EINVAL */
		pr_err(LBS_STAT_PREFIX"Invalid pid(%d)\n", pid);
		return -EINVAL;
	}

	/* get uid/sid */
	rcu_read_lock();
	task = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (!task) {
		pr_err(LBS_STAT_PREFIX"No task(%d)\n", pid);
		return -EINVAL;
	}
	uid = from_kuid_munged(current_user_ns(), task_uid(task));
	security_task_getsecid(task, &sid);
	usid = find_usid(uid, sid);

	/* Get method type and call start func */
	lbs_method = get_lbs_method(type);
	if (lbs_method < 0) {
		pr_err(LBS_STAT_PREFIX"Invalid method %s\n", type);
		return -EINVAL;
	}

	lbs_stat_start(lbs_method, usid, uid, sid);

	return count;
}

static ssize_t lbs_stat_stop_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct task_struct *task;
	char buf[MAX_BUFFER_SIZE] = {};
	char *str;

	char type[REQ_TYPE_LEN] = {0};
	int pid = 0, pos = -1;
	int lbs_method;

	uid_t uid;
	u32 usid, sid;

	/* Copy_from_user */
	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(&buf, user_buf, count))
		return -EFAULT;
	buf[count] = '\0';
	str = strstrip(buf);

	/* Request type length is 16 */
	if (sscanf(str, "%d %16s %n", &pid, type, &pos) != 2) {
		pr_err(LBS_STAT_PREFIX"Invalid number of arguments passed\n");
		return -EINVAL;
	}

	/* Check pid */
	if (pid == 0) {
		/* If there's no pid, return -EINVAL */
		pr_err(LBS_STAT_PREFIX"Invalid pid(%d)\n", pid);
		return -EINVAL;
	}

	/* get uid/sid */
	rcu_read_lock();
	task = find_task_by_vpid(pid);
	rcu_read_unlock();
	if (!task) {
		pr_err(LBS_STAT_PREFIX"No task(%d)\n", pid);
		return -EINVAL;
	}
	uid = from_kuid_munged(current_user_ns(), task_uid(task));
	security_task_getsecid(task, &sid);
	usid = find_usid(uid, sid);

	/* Get method type and call stop func */
	lbs_method = get_lbs_method(type);
	if (lbs_method < 0){
		pr_err(LBS_STAT_PREFIX"Invalid method %s\n", type);
		return -EINVAL;
	}

	lbs_stat_stop(lbs_method, usid, uid, sid);

	return count;
}

static const struct file_operations request_start_fops = {
	.write		= lbs_stat_start_write,
};

static const struct file_operations request_stop_fops = {
	.write		= lbs_stat_stop_write,
};

static const struct file_operations lbs_start_fops = {
	.write		= lbs_stat_start_write,
};

static const struct file_operations lbs_stop_fops = {
	.write		= lbs_stat_stop_write,
};

static int lbs_stat_show(struct seq_file *m, void *v)
{
	struct lbs_stat *entry;
	unsigned long flags;
	ktime_t total_time, max_time, max_time_stamp, active_time;
	int i, active_count;

	seq_printf(m, "name             active_count    active_since    total_time"
				"      max_time        max_time_stamp  last_change\n");

	spin_lock_irqsave(&lbs_stat_lock, flags);
	list_for_each_entry(entry, &lbs_stat_list, list) {
		seq_printf(m, "%-48u %-lld\n",
				entry->usid, ktime_to_ms(entry->total_time));

		for (i = 0; i < LBS_METHOD_MAX; i++) {
			active_count = entry->usage[i].active_count;
			if (active_count == 0)
				continue;

			total_time = entry->usage[i].total_time;
			max_time = entry->usage[i].max_time;
			max_time_stamp = entry->usage[i].max_time_stamp;
			if (entry->usage[i].active) {
				ktime_t now = ktime_get();

				active_time = ktime_sub(now, entry->usage[i].last_time);
				total_time = ktime_add(total_time, active_time);
				if (active_time.tv64 > max_time.tv64) {
					max_time = active_time;
					max_time_stamp = now;
				}
			} else {
				active_time = ktime_set(0, 0);
			}

			seq_printf(m, "%-16s %-16u%-16lld%-16lld%-16lld%-16lld%-16lld\n",
					get_lbs_method_str(i),
					active_count,
					ktime_to_ms(active_time), ktime_to_ms(total_time),
					ktime_to_ms(max_time), ktime_to_ms(max_time_stamp),
					ktime_to_ms(entry->usage[i].last_time));

		}
	}
	spin_unlock_irqrestore(&lbs_stat_lock, flags);

	return 0;
}

static int lbs_stat_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, lbs_stat_show, NULL);
}

static const struct file_operations lbs_stat_fops = {
	.open		= lbs_stat_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init lbs_stat_init(void)
{
	static struct dentry *root;

	pr_debug("%s\n", __func__);

	root = debugfs_create_dir("lbs_stat", NULL);
	if (!root) {
		pr_err(LBS_STAT_PREFIX"failed to create lbs_stat debugfs dir\n");
		return -ENOMEM;
	}
	if (!debugfs_create_file("start", 0220, root, NULL, &lbs_start_fops))
		goto error_debugfs;
	if (!debugfs_create_file("stop", 0220, root, NULL, &lbs_stop_fops))
		goto error_debugfs;
	if (!debugfs_create_file("stat", 0440, root, NULL, &lbs_stat_fops))
		goto error_debugfs;

	return 0;

error_debugfs:
	debugfs_remove_recursive(root);
	return -1;
}

static void lbs_stat_exit(void)
{
	pr_debug("%s\n", __func__);
}

module_init(lbs_stat_init);
module_exit(lbs_stat_exit);
