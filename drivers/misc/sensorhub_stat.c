/* drivers/misc/sensorhub_stat.c
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
#include <linux/sensorhub_stat.h>
#include <linux/suspend.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif /* CONFIG_SLEEP_MONITOR */

#define SENSORHUB_STAT_PREFIX	"sensorhub_stat:  "

#define RESTING_HR_TIMESTAMP_SIZE     4
#define RESTING_HR_ACTIVITY_TYPE_SIZE 3
#define RESTING_HR_HEART_RATE_SIZE    2
#define RESTING_HR_RELIABILTY_SIZE    2
#define RESTING_HR_DURATION_SIZE    1

static ktime_t resume_time;

static DEFINE_SPINLOCK(sensorhub_lock);
static LIST_HEAD(sensorhub_list);
static struct proc_dir_entry *parent;

struct sensorhub_stat {
	struct list_head link;
	struct timespec init_time;
	int lib_number;
	atomic_t success_cnt_post_suspend;
	atomic_t success_dur_post_suspend;
	atomic_t fail_count_post_suspend;
	atomic_t fail_dur_post_suspend;
	atomic_t success_cnt;
	atomic_t success_dur;
	atomic_t fail_cnt;
	atomic_t fail_dur;
	ktime_t ts;
	int suspend_count;
};

static int sensorhub_stat_pm_suspend_prepare_cb(ktime_t ktime)
{
	int entry_cnt =0;
	unsigned long flags;
	unsigned int cnt, succes_cnt, fail_cnt;
	unsigned int dur, success_dur, fail_dur;
	struct sensorhub_stat *entry;

	spin_lock_irqsave(&sensorhub_lock, flags);
	list_for_each_entry(entry, &sensorhub_list, link) {
		if (entry->ts.tv64 > ktime.tv64) {
			entry_cnt++;
			succes_cnt = (unsigned int) (atomic_read(&entry->success_cnt_post_suspend) + INT_MIN);
			success_dur =  (unsigned int) (atomic_read(&entry->success_dur_post_suspend) + INT_MIN);
			fail_cnt =  (unsigned int) (atomic_read(&entry->fail_count_post_suspend) + INT_MIN);
			fail_dur =  (unsigned int) (atomic_read(&entry->fail_dur_post_suspend) + INT_MIN);
			cnt = succes_cnt + fail_cnt;
			dur = success_dur + fail_dur;

			pr_info(SENSORHUB_STAT_PREFIX"%d: %2d %d %u %u %u %u %u %u\n",
						suspend_stats.success, entry_cnt,
						entry->lib_number,
						cnt, dur,
						succes_cnt, success_dur,
						fail_cnt, fail_dur);
		}
	}
	spin_unlock_irqrestore(&sensorhub_lock, flags);

	return 0;
}

static int sensorhub_stat_pm_post_suspend_cb(ktime_t ktime)
{
	unsigned long flags;
	struct sensorhub_stat *entry;

	spin_lock_irqsave(&sensorhub_lock, flags);
	list_for_each_entry(entry, &sensorhub_list, link) {
		if (entry->ts.tv64 > ktime.tv64) {
			/* Reset counter, so we can track sensor activity traffic during next post suspend. */
			atomic_set(&entry->success_cnt_post_suspend, INT_MIN);
			atomic_set(&entry->success_dur_post_suspend, INT_MIN);
			atomic_set(&entry->fail_count_post_suspend, INT_MIN);
			atomic_set(&entry->fail_dur_post_suspend, INT_MIN);
		}
	}
	spin_unlock_irqrestore(&sensorhub_lock, flags);

	return 0;
}

static struct sensorhub_stat *find_sensorhub_stat(int lib_number)
{
	unsigned long flags;
	struct sensorhub_stat *entry;

	spin_lock_irqsave(&sensorhub_lock, flags);
	list_for_each_entry(entry, &sensorhub_list, link) {
		if (lib_number == entry->lib_number) {
			spin_unlock_irqrestore(&sensorhub_lock, flags);
			return entry;
		}
	}
	spin_unlock_irqrestore(&sensorhub_lock, flags);

	return NULL;
}

/* Create a new entry for tracking the specified sensorhub lib#. */
static struct sensorhub_stat *create_stat(int lib_number)
{
	unsigned long flags;
	struct sensorhub_stat *new_sensorhub_lib;

	/* Create the sensorhub stat struct and append it to the list. */
	if ((new_sensorhub_lib = kmalloc(sizeof(struct sensorhub_stat), GFP_KERNEL)) == NULL)
		return NULL;

	new_sensorhub_lib->lib_number = lib_number;

	/* Counters start at INT_MIN, so we can track 4GB of sensor activity. */
	atomic_set(&new_sensorhub_lib->success_cnt_post_suspend, INT_MIN);
	atomic_set(&new_sensorhub_lib->success_dur_post_suspend, INT_MIN);
	atomic_set(&new_sensorhub_lib->fail_count_post_suspend, INT_MIN);
	atomic_set(&new_sensorhub_lib->fail_dur_post_suspend, INT_MIN);
	atomic_set(&new_sensorhub_lib->success_cnt, INT_MIN);
	atomic_set(&new_sensorhub_lib->success_dur, INT_MIN);
	atomic_set(&new_sensorhub_lib->fail_cnt, INT_MIN);
	atomic_set(&new_sensorhub_lib->fail_dur, INT_MIN);

	/* snapshot the current system time for calculating consumed enrergy */
	getnstimeofday(&new_sensorhub_lib->init_time);

	spin_lock_irqsave(&sensorhub_lock, flags);
	list_add_tail(&new_sensorhub_lib->link, &sensorhub_list);
	spin_unlock_irqrestore(&sensorhub_lock, flags);

	return new_sensorhub_lib;
}

// Big Endian
static u32 bytes_to_uint(const char* hub_data, int* cursor, int width)
{
	int i;
	u32 sum = 0;

	if (width > 4)
		pr_info("%s Invalid parameter\n", __func__);

	for (i = 0; i < width; i++) {
		sum = sum << 8;
		sum |= (hub_data[(*cursor)++] & 0xFF);
	}

	return sum;
}

int sensorhub_stat_rcv(const char *dataframe, int size)
{
	int lib_number = (int)dataframe[2];
	struct sensorhub_stat *entry;

	/* For now we only support the sensorhub lib#53 (resting hr) */
	if (lib_number != 53)
		return 0;

	pr_info(SENSORHUB_STAT_PREFIX"%s %d ", __func__, lib_number);

	if ((entry = find_sensorhub_stat(lib_number)) == NULL &&
		((entry = create_stat(lib_number)) == NULL)) {
			return -1;
	}
	entry->ts = ktime_get();

	if (lib_number == 53) {
		int i;
		int cursor = 0;
		int num_data = dataframe[5];
		const char *hub_data =  &dataframe[6];
		u32 fail_cnt = 0;
		u32 ts, activity_type, heart_rate, reliability, duration;

		for (i = 0; i < num_data; i++) {
			ts = bytes_to_uint(hub_data, &cursor, RESTING_HR_TIMESTAMP_SIZE);
			activity_type = bytes_to_uint(hub_data, &cursor, RESTING_HR_ACTIVITY_TYPE_SIZE);
			heart_rate = bytes_to_uint(hub_data, &cursor, RESTING_HR_HEART_RATE_SIZE);
			reliability = bytes_to_uint(hub_data, &cursor, RESTING_HR_RELIABILTY_SIZE);
			duration = bytes_to_uint(hub_data, &cursor, RESTING_HR_DURATION_SIZE);

			if (heart_rate >= 1000) {
				fail_cnt++;
				atomic_add(duration, &entry->fail_dur_post_suspend);
				atomic_add(duration, &entry->fail_dur);
			} else {
				atomic_add(duration, &entry->success_dur_post_suspend);
				atomic_add(duration, &entry->success_dur);
			}
			pr_cont("%u %u %u %u %u ", ts, activity_type, heart_rate, reliability, duration);
		}
		atomic_add(num_data - fail_cnt, &entry->success_cnt_post_suspend);
		atomic_add(num_data - fail_cnt, &entry->success_cnt);
		atomic_add(fail_cnt, &entry->fail_count_post_suspend);
		atomic_add(fail_cnt, &entry->fail_cnt);
		entry->suspend_count = suspend_stats.success;
	}
	pr_info("\n");

	return 0;
}

static int sensorhub_stat_stat_read_proc(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len;
	char *p = page;
	unsigned long flags;
	unsigned int cnt, succes_cnt, fail_cnt;
	unsigned int dur, success_dur, fail_dur;
	unsigned int total_cnt, total_succes_cnt, total_fail_cnt;
	unsigned int total_dur, total_success_dur, total_fail_dur;
	struct timespec current_time, delta;
	struct sensorhub_stat *entry;

	p += sprintf(p, "lib#\tcnt\tdur\tsucces_cnt\tsuccess_dur\tfail_cnt\tfail_dur\t"
		"total_cnt\ttotal_dur\ttotal_succes_cnt\ttotal_success_dur\ttotal_fail_cnt\ttotal_fail_dur\t"
		"suspend_count\telaped_time\n");
	spin_lock_irqsave(&sensorhub_lock, flags);
	list_for_each_entry(entry, &sensorhub_list, link) {
		succes_cnt = (unsigned int) (atomic_read(&entry->success_cnt_post_suspend) + INT_MIN);
		success_dur =  (unsigned int) (atomic_read(&entry->success_dur_post_suspend) + INT_MIN);
		fail_cnt =	(unsigned int) (atomic_read(&entry->fail_count_post_suspend) + INT_MIN);
		fail_dur =	(unsigned int) (atomic_read(&entry->fail_dur_post_suspend) + INT_MIN);
		cnt = succes_cnt + fail_cnt;
		dur = success_dur + fail_dur;

		total_succes_cnt = (unsigned int) (atomic_read(&entry->success_cnt) + INT_MIN);
		total_success_dur =  (unsigned int) (atomic_read(&entry->success_dur) + INT_MIN);
		total_fail_cnt =	(unsigned int) (atomic_read(&entry->fail_cnt) + INT_MIN);
		total_fail_dur =	(unsigned int) (atomic_read(&entry->fail_dur) + INT_MIN);
		total_cnt = total_succes_cnt + total_fail_cnt;
		total_dur = total_success_dur + total_fail_dur;

		getnstimeofday(&current_time);
		delta = timespec_sub(current_time, entry->init_time);

		p += sprintf(p, "%d\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%d\t%ld\n",
					entry->lib_number,
					cnt, dur, succes_cnt, success_dur, fail_cnt, fail_dur,
					total_cnt, total_dur, total_succes_cnt, total_success_dur, total_fail_cnt, total_fail_dur,
					entry->suspend_count, delta.tv_sec);
	}
	spin_unlock_irqrestore(&sensorhub_lock, flags);

	len = (p - page) - off;
	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

#ifndef CONFIG_SLEEP_MONITOR
static int sensorhub_stat_pm_notifier(struct notifier_block *nb,
								unsigned long event, void *dummy)
{
	switch (event) {
		case PM_SUSPEND_PREPARE:
			sensorhub_stat_pm_suspend_prepare_cb(resume_time);
			break;
		case PM_POST_SUSPEND:
			sensorhub_stat_pm_post_suspend_cb(resume_time);
			resume_time =  ktime_get();
			break;
		  default:
			break;
	}
	return 0;
}

static struct notifier_block sensorhub_stat_notifier_block = {
	.notifier_call = sensorhub_stat_pm_notifier,
};
#endif

#ifdef CONFIG_SLEEP_MONITOR
static int continuous_hr_sleep_monitor_a_read32_cb(void *priv,
				unsigned int *raw_val, int check_level, int caller_type)
{
	unsigned long flags;
	unsigned int cnt = 0, success_cnt = 0, fail_cnt = 0;
	unsigned int dur = 0, success_dur = 0, fail_dur = 0;
	int mask = 0;
	struct sensorhub_stat *entry;
	ktime_t *resume_time = (ktime_t*)priv;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND)
		sensorhub_stat_pm_suspend_prepare_cb(*resume_time);

	if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		sensorhub_stat_pm_post_suspend_cb(*resume_time);
		*resume_time =  ktime_get();
	}

	spin_lock_irqsave(&sensorhub_lock, flags);
	list_for_each_entry(entry, &sensorhub_list, link) {
		success_cnt = (unsigned int) (atomic_read(&entry->success_cnt_post_suspend) + INT_MIN);
		success_dur = (unsigned int) (atomic_read(&entry->success_dur_post_suspend) + INT_MIN);
		fail_cnt = (unsigned int) (atomic_read(&entry->fail_count_post_suspend) + INT_MIN);
		fail_dur = (unsigned int) (atomic_read(&entry->fail_dur_post_suspend) + INT_MIN);
	}
	cnt = success_cnt + fail_cnt;
	dur = success_dur + fail_dur;
	spin_unlock_irqrestore(&sensorhub_lock, flags);
	*raw_val = *raw_val | (success_cnt << 28);
	*raw_val = *raw_val | (fail_cnt << 24);
	mask = (2 << 24) - 1;
	*raw_val = *raw_val | ((dur > mask) ? mask : (dur & mask));

	return (cnt > 15) ? 15 : cnt;
}

static struct sleep_monitor_ops continuous_hr_sleep_monitor_a_ops = {
        .read_cb_func = continuous_hr_sleep_monitor_a_read32_cb,
};
#endif

static int __init sensorhub_stat_init(void)
{
	struct proc_dir_entry *pe;

	parent = proc_mkdir("sensorhub_stat", NULL);
	if (!parent) {
		pr_err("sensorhub_stat: failed to create proc entry\n");
		goto fail_sensorhub_stat;
	}

	pe = create_proc_read_entry("stat", S_IRUSR, parent,
			sensorhub_stat_stat_read_proc, NULL);
	if (!pe)
		goto fail_sensorhub_stat_stat;

#ifndef CONFIG_SLEEP_MONITOR
	if (0 != register_pm_notifier(&sensorhub_stat_notifier_block))
		goto fail_pm_register;
#endif

#ifdef CONFIG_SLEEP_MONITOR
         sleep_monitor_register_ops(&resume_time,
                &continuous_hr_sleep_monitor_a_ops,
                SLEEP_MONITOR_CONHR);
#endif /* CONFIG_SLEEP_MONITOR */

	return 0;
#ifndef CONFIG_SLEEP_MONITOR
fail_pm_register:
	remove_proc_entry("stat", parent);
#endif
fail_sensorhub_stat_stat:
	remove_proc_entry("sensorhub_stat", NULL);
fail_sensorhub_stat:

	return -1;
}

__initcall(sensorhub_stat_init);
