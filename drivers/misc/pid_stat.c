/* drivers/misc/pid_stat.c
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/atomic.h>

#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/pid_stat.h>
#include <linux/suspend.h>
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif /* CONFIG_SLEEP_MONITOR */

#include <asm/uaccess.h>

#define PID_STAT_PREFIX	"pid_stat:  "

/*
 * Amount of bytes that can be read from /proc/pid_stat/stat
 */
#define PID_STAT_BYTES_TO_READ	4096

static ktime_t resume_time;

static DEFINE_SPINLOCK(pid_lock);
static LIST_HEAD(pid_list);
static struct proc_dir_entry *parent;

struct pid_stat {
	struct list_head link;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	atomic_t rcv;
	atomic_t snd;
	atomic_t rcv_count;
	atomic_t snd_count;
	atomic_t activity_count;
	atomic_t rcv_post_suspend;
	atomic_t snd_post_suspend;
	atomic_t rcv_count_post_suspend;
	atomic_t snd_count_post_suspend;
	ktime_t last_transmit;
	int suspend_count;
};

static struct pid_stat *find_pid_stat(pid_t pid)
{
	unsigned long flags;
	struct pid_stat *entry;
	struct task_struct *tsk;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (!tsk) {
		rcu_read_unlock();
		return ERR_PTR(-ESRCH);
	}
	rcu_read_unlock();

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, link) {
		if (!strcmp(tsk->comm, entry->comm)) {
			spin_unlock_irqrestore(&pid_lock, flags);
			return entry;
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);
	return NULL;
}

static ssize_t tcp_snd_read_proc(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int bytes;
	ssize_t ret = 0;
	char buf[10];
	struct pid_stat *pid_entry = (struct pid_stat *) PDE_DATA(file_inode(file));

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (!pid_entry)
		return 0;

	if (*ppos == 0) {
		bytes = (unsigned int) (atomic_read(&pid_entry->snd) + INT_MIN);
		ret += snprintf(buf+ret, sizeof(buf) - ret, "%u\n", bytes);
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			return -EFAULT;
		}
		*ppos += ret;
	}

	return ret;
}

static const struct file_operations tcp_snd_fops = {
	.read		= tcp_snd_read_proc,
};

static ssize_t tcp_rcv_read_proc(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int bytes;
	ssize_t ret = 0;
	char buf[10];
	struct pid_stat *pid_entry = (struct pid_stat *) PDE_DATA(file_inode(file));

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (!pid_entry)
		return 0;

	if (*ppos == 0) {
		bytes = (unsigned int) (atomic_read(&pid_entry->rcv) + INT_MIN);
		ret += snprintf(buf+ret, sizeof(buf) - ret, "%u\n", bytes);
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			return -EFAULT;
		}
		*ppos += ret;
	}

	return ret;
}

static const struct file_operations tcp_rcv_fops = {
	.read		= tcp_rcv_read_proc,
};

/* Create a new entry for tracking the specified pid. */
static struct pid_stat *create_stat(pid_t pid)
{
	unsigned long flags;
	struct task_struct *tsk;
	struct pid_stat *new_sap_pid;
	struct proc_dir_entry *entry;

	/* Create the pid stat struct and append it to the list. */
	if ((new_sap_pid = kmalloc(sizeof(struct pid_stat), GFP_KERNEL)) == NULL)
		return NULL;

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (!tsk) {
		rcu_read_unlock();
		kfree(new_sap_pid);
		return ERR_PTR(-ESRCH);
	}
	rcu_read_unlock();
	memcpy(new_sap_pid->comm, tsk->comm, TASK_COMM_LEN);

	/* Counters start at INT_MIN, so we can track 4GB of traffic. */
	atomic_set(&new_sap_pid->rcv, INT_MIN);
	atomic_set(&new_sap_pid->snd, INT_MIN);
	atomic_set(&new_sap_pid->rcv_count, INT_MIN);
	atomic_set(&new_sap_pid->snd_count, INT_MIN);
	atomic_set(&new_sap_pid->activity_count, INT_MIN);
	atomic_set(&new_sap_pid->rcv_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->snd_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->rcv_count_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->snd_count_post_suspend, INT_MIN);

	spin_lock_irqsave(&pid_lock, flags);
	list_add_tail(&new_sap_pid->link, &pid_list);
	spin_unlock_irqrestore(&pid_lock, flags);

	entry = proc_mkdir(new_sap_pid->comm, parent);

	/* Keep reference to pid_stat so we know what pid to read stats from. */
	proc_create_data("tcp_snd", S_IRUSR, entry , &tcp_snd_fops,
		(void *) new_sap_pid);

	proc_create_data("tcp_rcv", S_IRUSR, entry, &tcp_rcv_fops,
		(void *) new_sap_pid);

	return new_sap_pid;
}

int pid_stat_tcp_snd(pid_t pid, int size)
{
	struct pid_stat *entry;
	if ((entry = find_pid_stat(pid)) == NULL &&
		((entry = create_stat(pid)) == NULL)) {
			return -1;
	}
	atomic_add(size, &entry->snd);
	atomic_inc(&entry->snd_count);
	entry->last_transmit = ktime_get();
	atomic_add(size, &entry->snd_post_suspend);
	atomic_inc(&entry->snd_count_post_suspend);
	entry->suspend_count = suspend_stats.success;
	return 0;
}

int pid_stat_tcp_rcv(pid_t pid, int size)
{
	struct pid_stat *entry;
	if ((entry = find_pid_stat(pid)) == NULL &&
		((entry = create_stat(pid)) == NULL)) {
			return -1;
	}
	atomic_add(size, &entry->rcv);
	atomic_inc(&entry->rcv_count);
	entry->last_transmit = ktime_get();
	atomic_add(size, &entry->rcv_post_suspend);
	atomic_inc(&entry->rcv_count_post_suspend);
	entry->suspend_count = suspend_stats.success;
	return 0;
}

static ssize_t pid_stat_read_proc(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count, transmit_count;
	unsigned int total_snd_bytes, total_rcv_bytes;
	unsigned int total_snd_count, total_rcv_count, total_transmit_count;
	unsigned int activity_count;
	ssize_t ret = 0;
	char *buf;
	struct pid_stat *entry;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kzalloc(PID_STAT_BYTES_TO_READ, GFP_KERNEL);
	if (!buf) {
		pr_err(PID_STAT_PREFIX"%s can not allocate buffer\n",
			__func__);
		return -ENOMEM;
	}

	if (*ppos == 0) {
		ret += snprintf(buf+ret, PID_STAT_BYTES_TO_READ - ret, "name\t\tcount\t\t"
			"snd_count\tsnd_bytes\trcv_count\trcv_bytes\t"
			"activity_count\tttotal_transmit\ttotal_snd_count\ttotal_snd_bytes\t"
			"total_rcv_count\ttotal_rcv_bytes\tlast_transmit\tsuspend_count\n");
		spin_lock_irqsave(&pid_lock, flags);
		list_for_each_entry(entry, &pid_list, link) {
			snd_bytes = (unsigned int) (atomic_read(&entry->snd_post_suspend) + INT_MIN);
			rcv_bytes = (unsigned int) (atomic_read(&entry->rcv_post_suspend) + INT_MIN);
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
			transmit_count = snd_count + rcv_count;

			activity_count = (unsigned int) (atomic_read(&entry->activity_count) + INT_MIN);

			total_snd_bytes = (unsigned int) (atomic_read(&entry->snd) + INT_MIN);
			total_rcv_bytes = (unsigned int) (atomic_read(&entry->rcv) + INT_MIN);
			total_snd_count = (unsigned int) (atomic_read(&entry->snd_count) + INT_MIN);
			total_rcv_count = (unsigned int) (atomic_read(&entry->rcv_count) + INT_MIN);
			total_transmit_count = total_snd_count + total_rcv_count;

			if (ret < PID_STAT_BYTES_TO_READ)
				ret += snprintf(buf+ret, PID_STAT_BYTES_TO_READ - ret,
					"%-16s%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%lld\t\t%d\n",
							entry->comm, transmit_count,
							snd_count, snd_bytes,
							rcv_count, rcv_bytes,
							activity_count,
							total_transmit_count,
							total_snd_count, total_snd_bytes,
							total_rcv_count, total_rcv_bytes,
							ktime_to_ms(entry->last_transmit),
							entry->suspend_count);
		}
		spin_unlock_irqrestore(&pid_lock, flags);
	}

	if (ret >= 0) {
		if (ret > PID_STAT_BYTES_TO_READ)
			ret = PID_STAT_BYTES_TO_READ;
		if (copy_to_user(buffer, buf, ret)) {
			return -EFAULT;
		}
		*ppos += ret;
	}
	kfree(buf);

	return ret;
}

static const struct file_operations pid_stat_fops = {
	.read		= pid_stat_read_proc,
};

static int pid_stat_pm_suspend_prepare_cb(ktime_t ktime)
{
	int entry_cnt =0;
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count, transmit_count;
	struct pid_stat *entry;

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, link) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			atomic_inc(&entry->activity_count);
			entry_cnt++;
			snd_bytes = (unsigned int) (atomic_read(&entry->snd_post_suspend) + INT_MIN);
			rcv_bytes = (unsigned int) (atomic_read(&entry->rcv_post_suspend) + INT_MIN);
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);

			transmit_count = snd_count + rcv_count;

			pr_info(PID_STAT_PREFIX"%d: %2d %-16s\t%u\t%u\t%u\t%u\t%u\t%u\t%lld\t%lld\t%lld\n",
						suspend_stats.success, entry_cnt,
						entry->comm,
						atomic_read(&entry->activity_count),
						transmit_count,
						snd_count, snd_bytes,
						rcv_count, rcv_bytes,
						ktime_to_ms(ktime),
						ktime_to_ms(entry->last_transmit),
						ktime_to_ms(ktime_sub(entry->last_transmit, ktime)));
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);

	return 0;
}

static int pid_stat_pm_post_suspend_cb(ktime_t ktime)
{
	unsigned long flags;
	struct pid_stat *entry;

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, link) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			/* Reset counter, so we can track  traffic during next post suspend. */
			atomic_set(&entry->rcv_post_suspend, INT_MIN);
			atomic_set(&entry->snd_post_suspend, INT_MIN);
			atomic_set(&entry->rcv_count_post_suspend, INT_MIN);
			atomic_set(&entry->snd_count_post_suspend, INT_MIN);
		}
	}
	spin_unlock_irqrestore(&pid_lock, flags);

	return 0;
}

#ifndef CONFIG_SLEEP_MONITOR
static int pid_stat_pm_notifier(struct notifier_block *nb,
					unsigned long event, void *dummy)
{
	switch (event) {
		case PM_SUSPEND_PREPARE:
			pid_stat_pm_suspend_prepare_cb(resume_time);
			break;
		case PM_POST_SUSPEND:
			pid_stat_pm_post_suspend_cb(resume_time);
			resume_time =  ktime_get();
			break;
		default:
			break;
	}
	return 0;
}

static struct notifier_block pid_stat_notifier_block = {
	.notifier_call = pid_stat_pm_notifier,
};
#endif

#ifdef CONFIG_SLEEP_MONITOR
static int pid_stat_sleep_monitor_read_cb(void *priv,
          unsigned int *raw_val, int check_level, int caller_type)
{
	int mask = 0;
	unsigned long flags;
	unsigned int transmit_count = 0, snd_count = 0, rcv_count = 0;
	unsigned int act_cnt = 0;
	struct pid_stat *entry;
	ktime_t *resume_time = (ktime_t *)priv;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND)
		pid_stat_pm_suspend_prepare_cb(*resume_time);

	if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		pid_stat_pm_post_suspend_cb(*resume_time);
		*resume_time =  ktime_get();
	}

	spin_lock_irqsave(&pid_lock, flags);
	list_for_each_entry(entry, &pid_list, link) {
            if (entry->last_transmit.tv64 > resume_time->tv64) {
			act_cnt += (unsigned int) (atomic_read(&entry->activity_count) + INT_MIN);
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
			transmit_count += snd_count + rcv_count;
            }
        }
	spin_unlock_irqrestore(&pid_lock, flags);

	mask = 0xffffffff;
	*raw_val = (transmit_count > mask) ? mask : transmit_count;

	if (act_cnt == 0)
		return DEVICE_POWER_OFF;
	else {
		return (act_cnt > DEVICE_UNKNOWN) ? DEVICE_UNKNOWN : act_cnt;
	}
}

static struct sleep_monitor_ops pid_stat_sleep_monitor_ops = {
	.read_cb_func = pid_stat_sleep_monitor_read_cb,
};
#endif

static int __init pid_stat_init(void)
{
	struct proc_dir_entry *pe;

	parent = proc_mkdir("pid_stat", NULL);
	if (!parent) {
		pr_err(PID_STAT_PREFIX"pid_stat: failed to create proc entry\n");
		goto fail_pid_stat;
	}

	/* Make interface to read the tcp traffic statistic */
	pe = proc_create("stat", S_IRUGO, parent, &pid_stat_fops);
	if (!pe)
		goto fail_pid_stat_stat;

#ifndef CONFIG_SLEEP_MONITOR
{
	int ret = 0;
	ret = register_pm_notifier(&pid_stat_notifier_block);
	if (ret)
		goto fail_register;
}
#else
	sleep_monitor_register_ops(&resume_time,
		&pid_stat_sleep_monitor_ops,
		SLEEP_MONITOR_TCP);
#endif

	return 0;

#ifndef CONFIG_SLEEP_MONITOR
fail_register:
	remove_proc_entry("stat", NULL);
#endif
fail_pid_stat_stat:
	remove_proc_entry("pid_stat", NULL);
fail_pid_stat:

	return -1;
}

__initcall(pid_stat_init);
