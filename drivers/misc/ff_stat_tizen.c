/* drivers/input/ff_stat_tizen.c
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
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
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Junho Jang <vincent.jang@samsung.com>       <2018>                    *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/uaccess.h>
#include <linux/ff-memless_notify.h>

#include <linux/ff_stat_tizen.h>

#ifdef CONFIG_ENERGY_MONITOR
#include <linux/sort.h>
#include <linux/power/energy_monitor.h>
#endif

#define FF_STAT_PREFIX	"ff_stat_tizen: "

struct ff_stat_tizen {
	ktime_t total_time;
	ktime_t last_time;
	unsigned int magnitude;
	unsigned long play_count;
	bool			play;
#ifdef CONFIG_ENERGY_MONITOR
	ktime_t emon_total_time;
	unsigned long emon_play_count;
#endif
};

static struct ff_stat_tizen ff_stat;


#ifdef CONFIG_ENERGY_MONITOR
 int ff_stat_tizen_get_stat(int type, struct ff_stat_emon *emon_stat)
{
	struct ff_stat_tizen *stat = &ff_stat;

	emon_stat->total_time = stat->emon_total_time;
	emon_stat->play_count = stat->emon_play_count;

	if (type != ENERGY_MON_TYPE_DUMP) {
		stat->emon_total_time = ktime_set(0, 0);
		stat->emon_play_count = 0;
	}

	return 0;
}
#endif

static int ff_stat_tizen_update(int event, struct ff_effect *effect)
{
	struct ff_stat_tizen *stat = &ff_stat;
	ktime_t now;

	now = ktime_get();
	switch (effect->type) {
	case FF_RUMBLE:
 		if (event == FF_MEMLESS_EVENT_PLAY) {
			if (!stat->play) {
				stat->play = true;
				stat->play_count++;
				stat->last_time = now;
				stat->magnitude = (u32)effect->u.rumble.strong_magnitude;
#ifdef CONFIG_ENERGY_MONITOR
				stat->emon_play_count++;
#endif
			}
 		} else {
 			if (stat->play) {
				ktime_t duration;

				stat->play = false;
				duration = ktime_sub(now, stat->last_time);
				stat->total_time = ktime_add(stat->total_time, duration);
				stat->last_time = now;
#ifdef CONFIG_ENERGY_MONITOR
				stat->emon_total_time = ktime_add(stat->emon_total_time, duration);
#endif
 			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ff_stat_tizen_show(struct seq_file *m, void *v)
{
	struct ff_stat_tizen *stat = &ff_stat;

	seq_printf(m, "status        play_count    total_time    last_time     magnitude   \n");
	seq_printf(m, "%-u             %-12lu  %-12llu  %-12llu  %-u\n",
				stat->play, stat->play_count,
				ktime_to_ms(stat->total_time), ktime_to_ms(stat->last_time),
				stat->magnitude);

	return 0;
}

static int ff_stat_tizen_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, ff_stat_tizen_show, NULL);
}

static const struct file_operations ff_stat_tizen_fops = {
	.open       = ff_stat_tizen_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = single_release,
};

static int ff_memless_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct ff_effect *effect = (struct ff_effect *)data;

	switch (event) {
	case FF_MEMLESS_EVENT_PLAY:
		ff_stat_tizen_update(event, effect);
		break;
	case FF_MEMLESS_EVENT_STOP:
		ff_stat_tizen_update(event, effect);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block ff_memless_notifier = {
	.notifier_call	= ff_memless_notifier_callback,
};

static int __init ff_stat_tizen_init(void)
{
	int err;
	struct dentry *root;

	err = ff_memless_register_client(&ff_memless_notifier);
	if (err){
		pr_err(FF_STAT_PREFIX"failed to register notifier\n");
		return err;
	}

	root = debugfs_create_dir("ff_stat", NULL);
	if (!root) {
		pr_err(FF_STAT_PREFIX"failed to create ff_stat_tizen debugfs directory\n");
		return -ENOMEM;
	}

	/* Make interface to read the ff statistic */
	if (!debugfs_create_file("stat", 0660, root, NULL, &ff_stat_tizen_fops))
		goto error_debugfs;

	return 0;

error_debugfs:
	debugfs_remove_recursive(root);

	return -1;
}

late_initcall(ff_stat_tizen_init);
