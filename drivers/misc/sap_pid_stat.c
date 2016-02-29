/* drivers/misc/sap_pid_stat.c
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
#include <linux/sap_pid_stat.h>
#include <linux/suspend.h>
#include <linux/seq_file.h>
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif /* CONFIG_SLEEP_MONITOR */

#include <asm/uaccess.h>

#define SAP_STAT_PREFIX	"sap_stat:  "

static ktime_t resume_time;

static DEFINE_SPINLOCK(sap_pid_lock);
static LIST_HEAD(sap_pid_list);
static DEFINE_SPINLOCK(sap_snapshot_lock);
static LIST_HEAD(sap_snapshot_list);
static struct proc_dir_entry *parent;

#ifdef CONFIG_SLEEP_MONITOR
#define SAP_STAT_SLEEP_MON_RAW_LENGTH			0xf
#define SAP_STAT_SLEEP_MON_RAW_LENGTH_SHIFT		4

typedef enum {
	SAPID_ALRAM = 0,
	SAPID_CALL,
	SAPID_SETTING,
	SAPID_MESSAGES,
	SAPID_SHEALTH,
	SAPID_TEXTTEMPLATE,
	SAPID_CALENDAR,
	SAPID_CONTACTS,
	SAPID_GALLERYTRANSFER,
	SAPID_GALLERYRECEIVER,
	SAPID_MUSICCONTROLLER,
	SAPID_MUSICTRANSFER,
	SAPID_VOICEMEMO,
	SAPID_CONTEXT,
	SAPID_WEATHER,
	SAPID_EMAILNOTI,
	SAPID_FMP,
	SAPID_WMS,
	SAPID_LBS,
	SAPID_BCMSERVICE,
	SAPID_WEBPROXY,
	SAPID_NOTIFICATION,
	SAPID_FILETRANSFER,
	SAPID_SERVICECAPABILITY,
	SAPID_NONE = -1
} sapid_t;
#endif

 enum sap_stat_type {
	SAP_STAT_FULL,
	SAP_STAT_SNAPSHOT,
};

#define ASP_ID_LEN 15
typedef struct {
	char id[ASP_ID_LEN+1];
} aspid_t;

struct sap_pid_stat {
	struct list_head link;
#ifdef CONFIG_SLEEP_MONITOR
	sapid_t sapid;
#endif
	aspid_t aspid;
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

static int sap_stat_pm_suspend_prepare_cb(ktime_t ktime)
{
	int entry_cnt =0;
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count, transmit_count;
	struct sap_pid_stat *entry;

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, link) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			atomic_inc(&entry->activity_count);
			entry_cnt++;
			snd_bytes = (unsigned int) (atomic_read(&entry->snd_post_suspend) + INT_MIN);
			rcv_bytes = (unsigned int) (atomic_read(&entry->rcv_post_suspend) + INT_MIN);
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
			transmit_count = snd_count + rcv_count;

			pr_info(SAP_STAT_PREFIX"%d: %2d %-16s\t%u\t%u\t%u\t%u\t%u\t%lld\t%lld\t%lld\n",
						suspend_stats.success, entry_cnt,
						entry->aspid.id, transmit_count,
						snd_count, snd_bytes,
						rcv_count, rcv_bytes,
						ktime_to_ms(ktime),
						ktime_to_ms(entry->last_transmit),
						ktime_to_ms(ktime_sub(entry->last_transmit, ktime)));
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return 0;
}

static int sap_stat_pm_post_suspend_cb(ktime_t ktime)
{
	unsigned long flags;
	struct sap_pid_stat *entry;

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, link) {
		if (entry->last_transmit.tv64 > ktime.tv64) {
			/* Reset counter, so we can track SAP traffic during next post suspend. */
			atomic_set(&entry->rcv_post_suspend, INT_MIN);
			atomic_set(&entry->snd_post_suspend, INT_MIN);
			atomic_set(&entry->rcv_count_post_suspend, INT_MIN);
			atomic_set(&entry->snd_count_post_suspend, INT_MIN);
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	return 0;
}

#ifdef CONFIG_SLEEP_MONITOR
/**
 * Maps sapid type to SAP aspid.
 */
static aspid_t sapid_to_aspid[] = {
	[SAPID_ALRAM] = {"alarm"},
	[SAPID_CALL] = {"callhandler"},
	[SAPID_SETTING] = {"setting"},
	[SAPID_MESSAGES] = {"minimessage"},
	[SAPID_SHEALTH] = {"watch_pedometer"},
	[SAPID_TEXTTEMPLATE] = {"texttemplate"},
	[SAPID_CALENDAR] = {"calendar"},
	[SAPID_CONTACTS] = {"b2contact"},
	[SAPID_GALLERYTRANSFER] = {"transfer"},
	[SAPID_GALLERYRECEIVER] = {"receiver"},
	[SAPID_MUSICCONTROLLER] = {"music"},
	[SAPID_MUSICTRANSFER] = {"musictransfer"},
	[SAPID_VOICEMEMO] = {"voicememo"},
	[SAPID_CONTEXT] = {"context"},
	[SAPID_WEATHER] = {"weather"},
	[SAPID_EMAILNOTI] = {"emailnotificat"},
	[SAPID_FMP] = {"fmp"},
	[SAPID_WMS] = {"hostmanager"},
	[SAPID_LBS] = {"lbs-server"},
	[SAPID_BCMSERVICE] = {"bcmservice"},
	[SAPID_WEBPROXY] = {"webproxy"},
	[SAPID_NOTIFICATION] = {"NotificationSe"},
	[SAPID_FILETRANSFER] = {"filetransfer"},
	[SAPID_SERVICECAPABILITY] = {"ServiceCapabil"},
};

static sapid_t sap_stat_aspid_to_sapid(aspid_t *asp)
{
	int i;
	sapid_t sapid = SAPID_NONE;

	for (i=0; i < ARRAY_SIZE(sapid_to_aspid); i++)
		if (!strcmp(asp->id, sapid_to_aspid[i].id))
			return (sapid_t)i;

	return sapid;
}



static int sap_stat_sleep_monitor_a_read64_cb(void *priv,
							long long *raw_val, int check_level, int caller_type)
{
	int entry_cnt =0;
	unsigned long flags;
	unsigned int snd_count, rcv_count, transmit_count;
	struct sap_pid_stat *entry;
	ktime_t *resume_time = (ktime_t *)priv;

#ifdef CONFIG_SLEEP_MONITOR
	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND)
		sap_stat_pm_suspend_prepare_cb(*resume_time);

	if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		sap_stat_pm_post_suspend_cb(*resume_time);
		*resume_time =  ktime_get();
	}
#endif
	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, link) {
		if (entry->last_transmit.tv64 > resume_time->tv64) {
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
			transmit_count = snd_count + rcv_count;

			if (entry->sapid >= 0 && entry->sapid < 16) {
				entry_cnt++;
				if (transmit_count > SAP_STAT_SLEEP_MON_RAW_LENGTH)
					transmit_count = SAP_STAT_SLEEP_MON_RAW_LENGTH;
				*raw_val = *raw_val |
					((long long)(transmit_count&SAP_STAT_SLEEP_MON_RAW_LENGTH) <<
					(entry->sapid*SAP_STAT_SLEEP_MON_RAW_LENGTH_SHIFT));
			}
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	pr_debug(SAP_STAT_PREFIX"%d:0x%llx\n", entry_cnt, *raw_val);

	 return entry_cnt;
}

static int sap_stat_sleep_monitor_b_read64_cb(void *priv,
							long long *raw_val, int check_level, int caller_type)
{
	int entry_cnt =0;
	unsigned long flags;
	unsigned int snd_count, rcv_count, transmit_count;
	struct sap_pid_stat *entry;
	ktime_t *resume_time = (ktime_t *)priv;

	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, link) {
		if (entry->last_transmit.tv64 > resume_time->tv64) {
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
			transmit_count = snd_count + rcv_count;

			if (entry->sapid >= 16) {
				entry_cnt++;
				if (transmit_count > SAP_STAT_SLEEP_MON_RAW_LENGTH)
					transmit_count = SAP_STAT_SLEEP_MON_RAW_LENGTH;
				*raw_val = *raw_val |
					((long long)(transmit_count&SAP_STAT_SLEEP_MON_RAW_LENGTH) <<
					((entry->sapid-16)*SAP_STAT_SLEEP_MON_RAW_LENGTH_SHIFT));
			}
		}
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	pr_debug(SAP_STAT_PREFIX"0x%llx\n", *raw_val);

	 return entry_cnt;
}

static struct sleep_monitor_ops sap_stat_sleep_monitor_a_ops = {
	.read64_cb_func = sap_stat_sleep_monitor_a_read64_cb,
};
static struct sleep_monitor_ops sap_stat_sleep_monitor_b_ops = {
	.read64_cb_func = sap_stat_sleep_monitor_b_read64_cb,
};
#endif /* CONFIG_SLEEP_MONITOR */

static struct sap_pid_stat *find_sap_stat(aspid_t *asp,  int type)
{
	unsigned long flags;
	struct sap_pid_stat *entry;

	if (type == SAP_STAT_SNAPSHOT) {
		spin_lock_irqsave(&sap_snapshot_lock, flags);
		list_for_each_entry(entry, &sap_snapshot_list, link) {
			if (!strcmp(asp->id, entry->aspid.id)) {
				spin_unlock_irqrestore(&sap_snapshot_lock, flags);
				return entry;
			}
		}
		spin_unlock_irqrestore(&sap_snapshot_lock, flags);
	} else {
		spin_lock_irqsave(&sap_pid_lock, flags);
		list_for_each_entry(entry, &sap_pid_list, link) {
			if (!strcmp(asp->id, entry->aspid.id)) {
				spin_unlock_irqrestore(&sap_pid_lock, flags);
				return entry;
			}
		}
		spin_unlock_irqrestore(&sap_pid_lock, flags);
	}
	return NULL;
}

static int sap_snd_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len;
	unsigned int bytes;
	char *p = page;
	struct sap_pid_stat *sap_pid_entry = (struct sap_pid_stat *) data;
	if (!data)
		return 0;

	bytes = (unsigned int) (atomic_read(&sap_pid_entry->snd) + INT_MIN);
	p += sprintf(p, "%u\n", bytes);
	len = (p - page) - off;
	*eof = (len <= count) ? 1 : 0;
	*start = page + off;
	return len;
}

static int sap_rcv_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len;
	unsigned int bytes;
	char *p = page;
	struct sap_pid_stat *sap_pid_entry = (struct sap_pid_stat *) data;
	if (!data)
		return 0;

	bytes = (unsigned int) (atomic_read(&sap_pid_entry->rcv) + INT_MIN);
	p += sprintf(p, "%u\n", bytes);
	len = (p - page) - off;
	*eof = (len <= count) ? 1 : 0;
	*start = page + off;
	return len;
}

/* Create a new entry for tracking the specified aspid. */
static struct sap_pid_stat *create_stat(aspid_t *asp, int type)
{
	unsigned long flags;
	struct sap_pid_stat *new_sap_pid;
	struct proc_dir_entry *entry;

	/* Create the pid stat struct and append it to the list. */
	if ((new_sap_pid = kmalloc(sizeof(struct sap_pid_stat), GFP_KERNEL)) == NULL)
		return NULL;

	memcpy(new_sap_pid->aspid.id, asp->id, sizeof(aspid_t));
#ifdef CONFIG_SLEEP_MONITOR
	new_sap_pid->sapid = sap_stat_aspid_to_sapid(asp);
#endif

	/* Counters start at INT_MIN, so we can track 4GB of SAP traffic. */
	atomic_set(&new_sap_pid->rcv, INT_MIN);
	atomic_set(&new_sap_pid->snd, INT_MIN);
	atomic_set(&new_sap_pid->rcv_count, INT_MIN);
	atomic_set(&new_sap_pid->snd_count, INT_MIN);
	atomic_set(&new_sap_pid->activity_count, INT_MIN);
	atomic_set(&new_sap_pid->rcv_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->snd_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->rcv_count_post_suspend, INT_MIN);
	atomic_set(&new_sap_pid->snd_count_post_suspend, INT_MIN);

	if (type == SAP_STAT_SNAPSHOT) {
		spin_lock_irqsave(&sap_snapshot_lock, flags);
		list_add_tail(&new_sap_pid->link, &sap_snapshot_list);
		spin_unlock_irqrestore(&sap_snapshot_lock, flags);
	} else {
		spin_lock_irqsave(&sap_pid_lock, flags);
		list_add_tail(&new_sap_pid->link, &sap_pid_list);
		spin_unlock_irqrestore(&sap_pid_lock, flags);

		entry = proc_mkdir(new_sap_pid->aspid.id, parent);

		/* Keep reference to sap_pid_stat so we know what aspid to read stats from. */
		create_proc_read_entry("sap_snd", S_IRUSR, entry , sap_snd_read_proc,
			(void *) new_sap_pid);

		create_proc_read_entry("sap_rcv", S_IRUSR, entry, sap_rcv_read_proc,
			(void *) new_sap_pid);
	}

	return new_sap_pid;
}

static int sap_stat_snd(aspid_t *asp, int size)
{
	struct sap_pid_stat *entry;
	if ((entry = find_sap_stat(asp, SAP_STAT_FULL)) == NULL &&
		((entry = create_stat(asp, SAP_STAT_FULL)) == NULL)) {
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

static int sap_stat_rcv(aspid_t *asp, int size)
{
	struct sap_pid_stat *entry;
	if ((entry = find_sap_stat(asp, SAP_STAT_FULL)) == NULL &&
		((entry = create_stat(asp, SAP_STAT_FULL)) == NULL)) {
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

static ssize_t sap_stat_snd_write_proc(struct file *file, const char __user *buf,
			    size_t count, loff_t *offs)
{
	const char *str;
	char pid_buf[128];
	size_t buf_size = min(count, sizeof(pid_buf) - 1);
	aspid_t aspid;
	int bytes = 0;
	size_t len;
	int ret = 0;

	if (count > sizeof(pid_buf))
		return -EINVAL;

	memset(pid_buf, 0, sizeof(pid_buf));
	if (copy_from_user(pid_buf, buf, buf_size))
		return -EFAULT;
	str = pid_buf;

	pr_debug(SAP_STAT_PREFIX"%s: user buf=%s: count=%d\n",
				__func__, buf, count);

	while (*str && !isspace(*str))
		str++;

	len = str - pid_buf;
	if (!len || len > ASP_ID_LEN)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a byte string appended. */
		ret = kstrtos32(skip_spaces(str), 10, &bytes);
		if (ret)
			return -EINVAL;
	}

	memset(&aspid, 0, sizeof(aspid_t));
	memcpy(&aspid, pid_buf, len);

	pr_debug(SAP_STAT_PREFIX"%s: aspid=%s: byte=%d: len=%d\n",
				__func__, aspid.id, bytes, len);

	if (bytes > 0)
		sap_stat_snd(&aspid, bytes);

	return count;
}

static const struct file_operations sap_stat_snd_fops = {
	.open = nonseekable_open,
	.write = sap_stat_snd_write_proc,
	.llseek = no_llseek,
};

static ssize_t sap_stat_rcv_write_proc(struct file *file, const char __user *buf,
			    size_t count, loff_t *offs)
{
	const char *str;
	char pid_buf[128];
	size_t buf_size = min(count, sizeof(pid_buf) - 1);
	aspid_t aspid;
	int bytes = 0;
	size_t len;
	int ret = 0;

	if (count > sizeof(pid_buf))
		return -EINVAL;

	memset(pid_buf, 0, sizeof(pid_buf));
	if (copy_from_user(pid_buf, buf, buf_size))
		return -EFAULT;
	str = pid_buf;

	pr_debug(SAP_STAT_PREFIX"%s: user buf=%s: count=%d\n",
				__func__, buf, count);

	while (*str && !isspace(*str))
		str++;

	len = str - pid_buf;
	if (!len || len > ASP_ID_LEN)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a byte string appended. */
		ret = kstrtos32(skip_spaces(str), 10, &bytes);
		if (ret)
			return -EINVAL;
	}

	memset(&aspid, 0, sizeof(aspid_t));
	memcpy(&aspid, pid_buf, len);

	pr_debug(SAP_STAT_PREFIX"%s: aspid=%s: byte=%d: len=%d\n",
				__func__, aspid.id, bytes, len);

	if (bytes > 0)
		sap_stat_rcv(&aspid, bytes);

	return count;
}


static const struct file_operations sap_stat_rcv_fops = {
	.open = nonseekable_open,
	.write = sap_stat_rcv_write_proc,
	.llseek = no_llseek,
};

static int sap_stat_snapshot_show(struct seq_file *m, void *v)
{
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count, transmit_count;
	unsigned int total_snd_bytes, total_rcv_bytes;
	unsigned int total_snd_count, total_rcv_count, total_transmit_count;
	unsigned int activity_count;
	struct sap_pid_stat *entry;

	seq_printf(m, "name\t\tcount\t\t"
		"snd_count\tsnd_bytes\trcv_count\trcv_bytes\t"
		"activity_count\ttotal_transmit\ttotal_snd_count\ttotal_snd_bytes\t"
		"total_rcv_count\ttotal_rcv_bytes\tlast_transmit\tsuspend_count\n");

	spin_lock_irqsave(&sap_snapshot_lock, flags);
	list_for_each_entry(entry, &sap_snapshot_list, link) {
		snd_bytes = (unsigned int) (atomic_read(&entry->snd_post_suspend) + INT_MIN);
		rcv_bytes = (unsigned int) (atomic_read(&entry->rcv_post_suspend) + INT_MIN);
		snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
		rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);
		transmit_count = snd_count + rcv_count;

		activity_count =  (unsigned int) (atomic_read(&entry->activity_count) + INT_MIN);

		total_snd_bytes = (unsigned int) (atomic_read(&entry->snd) + INT_MIN);
		total_rcv_bytes = (unsigned int) (atomic_read(&entry->rcv) + INT_MIN);
		total_snd_count = (unsigned int) (atomic_read(&entry->snd_count) + INT_MIN);
		total_rcv_count = (unsigned int) (atomic_read(&entry->rcv_count) + INT_MIN);
		total_transmit_count = total_snd_count + total_rcv_count;

		seq_printf(m, "%-16s%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%lld\t\t%d\n",
					entry->aspid.id, transmit_count,
					snd_count, snd_bytes,
					rcv_count, rcv_bytes,
					activity_count,
					total_transmit_count,
					total_snd_count, total_snd_bytes,
					total_rcv_count, total_rcv_bytes,
					ktime_to_ms(entry->last_transmit),
					entry->suspend_count);
	}
	spin_unlock_irqrestore(&sap_snapshot_lock, flags);

	return 0;
}

static int sap_stat_snapshot_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sap_stat_snapshot_show, NULL);
}

static ssize_t sap_stat_snapshot_write_proc(struct file *file, const char __user *buf,
			    size_t count, loff_t *offs)
{
	char ctl[2];
	unsigned long flags;
	struct sap_pid_stat *entry;
	struct sap_pid_stat *snapshot;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count;
	unsigned int total_snd_bytes, total_rcv_bytes;
	unsigned int total_snd_count, total_rcv_count;
	unsigned int activity_count;

	if (count != 2 || *offs)
		return -EINVAL;

	if (copy_from_user(ctl, buf, count))
		return -EFAULT;

	switch (ctl[0]) {
	case '0':
		break;
	case '1':
		spin_lock_irqsave(&sap_pid_lock, flags);
		list_for_each_entry(entry, &sap_pid_list, link) {
			if ((snapshot = find_sap_stat(&entry->aspid, SAP_STAT_SNAPSHOT)) == NULL &&
				((snapshot = create_stat(&entry->aspid, SAP_STAT_SNAPSHOT)) == NULL)) {
					spin_unlock_irqrestore(&sap_pid_lock, flags);
					return -ENOMEM;
			}
			memcpy(&snapshot->aspid, &entry->aspid, sizeof(aspid_t));

			total_snd_bytes = (unsigned int) (atomic_read(&entry->snd) + INT_MIN);
			total_rcv_bytes = (unsigned int) (atomic_read(&entry->rcv) + INT_MIN);
			total_snd_count = (unsigned int) (atomic_read(&entry->snd_count) + INT_MIN);
			total_rcv_count = (unsigned int) (atomic_read(&entry->rcv_count) + INT_MIN);
			activity_count =  (unsigned int) (atomic_read(&entry->activity_count) + INT_MIN);
			snd_bytes = (unsigned int) (atomic_read(&entry->snd_post_suspend) + INT_MIN);
			rcv_bytes = (unsigned int) (atomic_read(&entry->rcv_post_suspend) + INT_MIN);
			snd_count = (unsigned int) (atomic_read(&entry->snd_count_post_suspend) + INT_MIN);
			rcv_count = (unsigned int) (atomic_read(&entry->rcv_count_post_suspend) + INT_MIN);

			atomic_set(&snapshot->snd, total_snd_bytes + INT_MIN);
			atomic_set(&snapshot->rcv, total_rcv_bytes + INT_MIN);
			atomic_set(&snapshot->snd_count, total_snd_count + INT_MIN);
			atomic_set(&snapshot->rcv_count, total_rcv_count + INT_MIN);
			atomic_set(&snapshot->activity_count, activity_count + INT_MIN);
			atomic_set(&snapshot->snd_post_suspend, snd_bytes + INT_MIN);
			atomic_set(&snapshot->rcv_post_suspend, rcv_bytes + INT_MIN);
			atomic_set(&snapshot->snd_count_post_suspend, snd_count + INT_MIN);
			atomic_set(&snapshot->rcv_count_post_suspend, rcv_count + INT_MIN);

			snapshot->last_transmit = entry->last_transmit;
		}
		spin_unlock_irqrestore(&sap_pid_lock, flags);
		break;
	default:
		count = -EINVAL;
	}

	return count;
}

static const struct file_operations sap_stat_snapshot_fops = {
	.open		= sap_stat_snapshot_open,
	.read		= seq_read,
	.write		= sap_stat_snapshot_write_proc,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int sap_stat_stat_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len;
	unsigned long flags;
	unsigned int snd_bytes, rcv_bytes;
	unsigned int snd_count, rcv_count, transmit_count;
	unsigned int total_snd_bytes, total_rcv_bytes;
	unsigned int total_snd_count, total_rcv_count, total_transmit_count;
	unsigned int activity_count;
	char *p = page;
	struct sap_pid_stat *entry;

	p += sprintf(p, "name\t\tcount\t\t"
		"snd_count\tsnd_bytes\trcv_count\trcv_bytes\t"
		"activity_count\tttotal_transmit\ttotal_snd_count\ttotal_snd_bytes\t"
		"total_rcv_count\ttotal_rcv_bytes\tlast_transmit\tsuspend_count\n");
	spin_lock_irqsave(&sap_pid_lock, flags);
	list_for_each_entry(entry, &sap_pid_list, link) {
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

		p += sprintf(p, "%-16s%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%u\t\t%lld\t\t%d\n",
					entry->aspid.id, transmit_count,
					snd_count, snd_bytes,
					rcv_count, rcv_bytes,
					activity_count,
					total_transmit_count,
					total_snd_count, total_snd_bytes,
					total_rcv_count, total_rcv_bytes,
					ktime_to_ms(entry->last_transmit),
					entry->suspend_count);
	}
	spin_unlock_irqrestore(&sap_pid_lock, flags);

	len = (p - page) - off;
	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

#ifndef CONFIG_SLEEP_MONITOR
static int sap_stat_pm_notifier(struct notifier_block *nb,
								unsigned long event, void *dummy)
{
	switch (event) {
		case PM_SUSPEND_PREPARE:
			sap_stat_pm_suspend_prepare_cb(resume_time);
			break;
		case PM_POST_SUSPEND:
			sap_stat_pm_post_suspend_cb(resume_time);
			resume_time =  ktime_get();
			break;
		  default:
			break;
	}
	return 0;
}

static struct notifier_block sap_stat_notifier_block = {
	.notifier_call = sap_stat_pm_notifier,
};
#endif

static int __init sap_stat_init(void)
{
	struct proc_dir_entry *pe;

	parent = proc_mkdir("sap_pid_stat", NULL);
	if (!parent) {
		pr_err("sap_pid_stat: failed to create proc entry\n");
		goto fail_sap_stat;
	}

	/* Make interface to keep track of SAP traffic from user space */
	pe = proc_create("snd", S_IRUSR, parent, &sap_stat_snd_fops);
	if (!pe)
		goto fail_sap_stat_snd;
	pe = proc_create("rcv", S_IRUSR, parent, &sap_stat_rcv_fops);
	if (!pe)
		goto fail_sap_stat_rcv;

	pe = proc_create("snapshot", S_IRUSR, parent, &sap_stat_snapshot_fops);
	if (!pe)
		goto fail_sap_stat_snapshot;

	pe = create_proc_read_entry("stat", S_IRUSR, parent,
			sap_stat_stat_read_proc, NULL);
	if (!pe)
		goto fail_sap_stat_stat;
#ifndef CONFIG_SLEEP_MONITOR
{
	int ret = 0;
	ret = register_pm_notifier(&sap_stat_notifier_block);
	if (ret)
		goto fail_pm_register;
}
#endif

#ifdef CONFIG_SLEEP_MONITOR
	 sleep_monitor_register_ops(&resume_time,
	 	&sap_stat_sleep_monitor_a_ops,
	 	SLEEP_MONITOR_SAPA);
	 sleep_monitor_register_ops(&resume_time,
	 	&sap_stat_sleep_monitor_b_ops,
	 	SLEEP_MONITOR_SAPB);
#endif /* CONFIG_SLEEP_MONITOR */

	return 0;

#ifndef CONFIG_SLEEP_MONITOR
fail_pm_register:
	remove_proc_entry("stat", parent);
#endif
fail_sap_stat_snapshot:
	remove_proc_entry("snapshot", parent);	
fail_sap_stat_stat:
	remove_proc_entry("rcv", parent);
fail_sap_stat_rcv:
	remove_proc_entry("snd", parent);
fail_sap_stat_snd:
	remove_proc_entry("sap_pid_stat", NULL);
fail_sap_stat:

	return -1;
}

__initcall(sap_stat_init);
