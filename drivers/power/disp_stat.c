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
 * 1.0   Hunsup Jung <hunsup.jung@samsung.com>       2017.10.23                *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 2.0   Junho Jang <vincent.jang@samsung.com>       Add event_count, max_time    *
 * ---- -------------------------------------------- ------------------------- *
 */

#include <linux/debugfs.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/power/disp_stat.h>

#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

enum aod_state {
	AOD_OFF,
	AOD_ENTER,
	AOD_UPDATE_REQ,
	AOD_UPDATE_DONE,
	AOD_EXIT,
};

struct fb_stat {
	ktime_t total_time;
	ktime_t time[NUM_DIST_STAT_BR_LEVELS];
	ktime_t max_time;
	ktime_t max_time_stamp;
	ktime_t last_time;
	unsigned long event_count;
#ifdef CONFIG_ENERGY_MONITOR
	ktime_t emon_time[NUM_DIST_STAT_BR_LEVELS];
#endif
};

struct aod_stat {
	ktime_t total_time;
	ktime_t max_time;
	ktime_t max_time_stamp;
	ktime_t last_time;
	unsigned long event_count;
#ifdef CONFIG_ENERGY_MONITOR
	ktime_t emon_time;
#endif
};

struct disp_stat {
	int fb_state;
	struct fb_stat fb[FB_BLANK_POWERDOWN + 1];

	int aod_state;
	struct aod_stat aod[AOD_EXIT + 1];

	unsigned int num_br_lv_table;
	unsigned int *br_lv_table;
	unsigned int brightness; /* current user requested brightness */
	int br_lv; /* current user requested brightness level */

	struct notifier_block fb_notif;
#ifdef CONFIG_SLEEP_MONITOR
	struct timespec prev_boot_time;
	struct timespec prev_disp_time;
	struct timespec prev_aod_time;
#endif
	 struct dentry *disp_stat_dentry;
};

static struct disp_stat *disp_stat;

#ifdef CONFIG_SLEEP_MONITOR
#define SLP_MON_AOD_MASK	0xffff0000
#define SLP_MON_DISP_MASK	0xffff
#define SLP_MON_AOD_SHIFT	16

static int disp_stat_sleep_monitor_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	struct timespec curr_boot_time = {0, 0};
	struct timespec curr_disp_time = {0, 0};
	struct timespec curr_aod_time = {0, 0};
	int slp_mon_boot_time;
	int slp_mon_disp_time;
	int slp_mon_aod_time;
	int hit = DEVICE_UNKNOWN;

	get_monotonic_boottime(&curr_boot_time);
	curr_disp_time = disp_stat_get_fb_total_time();
	curr_aod_time = disp_stat_get_aod_total_time();

	slp_mon_boot_time =
		timespec_sub(curr_boot_time, disp_stat->prev_boot_time).tv_sec;
	slp_mon_disp_time =
		timespec_sub(curr_disp_time, disp_stat->prev_disp_time).tv_sec;
	slp_mon_aod_time =
		timespec_sub(curr_aod_time, disp_stat->prev_aod_time).tv_sec;

	if (slp_mon_boot_time < 0)
		slp_mon_boot_time = 0;
	if (slp_mon_disp_time < 0)
		slp_mon_disp_time = 0;
	if (slp_mon_aod_time < 0)
		slp_mon_aod_time = 0;

	disp_stat->prev_boot_time = curr_boot_time;
	disp_stat->prev_disp_time = curr_disp_time;
	disp_stat->prev_aod_time = curr_aod_time;

	*raw_val = (slp_mon_aod_time << SLP_MON_AOD_SHIFT) & SLP_MON_AOD_MASK;
	*raw_val |= slp_mon_disp_time & SLP_MON_DISP_MASK;

	pr_debug("%s: disp_time: %d, aod_time: %d\n",
			__func__, slp_mon_disp_time, slp_mon_aod_time);

	if (slp_mon_boot_time)
		hit = slp_mon_disp_time * hit / slp_mon_boot_time;

	return hit;
}

static struct sleep_monitor_ops disp_stat_sleep_monitor_ops = {
	.read_cb_func = disp_stat_sleep_monitor_cb,
};
#endif

static int disp_stat_find_br_lv(int brightness)
{
	int i;

	if (disp_stat->num_br_lv_table > 0)
		for (i = 0; i < disp_stat->num_br_lv_table; i++) {
			if (disp_stat->br_lv_table[i] == brightness)
				return i;
		}

	return -1;
}

static void disp_stat_fb_update(int state, int brightness)
{
	ktime_t now, max_time, active_time;

	if (unlikely(state > FB_BLANK_POWERDOWN)) {
		pr_err("%s:invalid state %d\n", __func__, state);
		return;
	}

	pr_info("%s:%d,%d,%d\n", __func__, disp_stat->fb_state, state, brightness);

	now = ktime_get();
	max_time = disp_stat->fb[disp_stat->fb_state].max_time;
	active_time = ktime_sub(now, disp_stat->fb[disp_stat->fb_state].last_time);
	disp_stat->fb[disp_stat->fb_state].total_time =
		ktime_add(disp_stat->fb[disp_stat->fb_state].total_time, active_time);

	if (disp_stat->fb_state == FB_BLANK_UNBLANK) {
		int br_lv;

		if (brightness < 0) {
			brightness = disp_stat->brightness;
			pr_info("%s: use stored brightness(%d) in aod case\n",
				__func__, brightness);
		}

		br_lv = disp_stat_find_br_lv(brightness);

		if (br_lv >= 0) {
			disp_stat->fb[disp_stat->fb_state].time[br_lv] =
				ktime_add(disp_stat->fb[disp_stat->fb_state].time[br_lv], active_time);
			disp_stat->br_lv = br_lv;
			disp_stat->brightness = brightness;
		}
	}

	if (ktime_to_ns(active_time) > ktime_to_ns(max_time)) {
		disp_stat->fb[disp_stat->fb_state].max_time = active_time;
		disp_stat->fb[disp_stat->fb_state].max_time_stamp = now;
	}

	pr_debug("%s:[%d] %11lld %11lld %11lld %11lld %11lld\n",__func__,
		disp_stat->fb_state,
		ktime_to_ms(now),
		ktime_to_ms(active_time),
		ktime_to_ms(disp_stat->fb[disp_stat->fb_state].total_time),
		ktime_to_ms(max_time),
		ktime_to_ms(disp_stat->fb[disp_stat->fb_state].max_time_stamp));

	disp_stat->fb[state].last_time = now;
	disp_stat->fb[state].event_count++;
	disp_stat->fb_state = state;
}

void disp_stat_aod_update(int state)
{
	ktime_t now, max_time, active_time;

	if (unlikely(state > AOD_EXIT)) {
		pr_err("%s:invalid state %d\n", __func__, state);
		return;
	}

	if (state == AOD_ENTER) {
		disp_stat_fb_update(FB_BLANK_POWERDOWN, -1);
		pr_info("%s:%d,%d\n", __func__, disp_stat->aod_state, state);
	} else {
		pr_info("%s:%d,%d\n", __func__, disp_stat->aod_state, state);
		disp_stat_fb_update(FB_BLANK_UNBLANK, -1);
	}

	now = ktime_get_boottime();
	max_time = disp_stat->aod[disp_stat->aod_state].max_time;
	active_time = ktime_sub(now, disp_stat->aod[disp_stat->aod_state].last_time);
	disp_stat->aod[disp_stat->aod_state].total_time =
		ktime_add(disp_stat->aod[disp_stat->aod_state].total_time, active_time);

	if (ktime_to_ns(active_time) > ktime_to_ns(max_time)) {
		disp_stat->aod[disp_stat->aod_state].max_time = active_time;
		disp_stat->aod[disp_stat->aod_state].max_time_stamp = now;
	}

	pr_debug("%s:[%d] %11lld %11lld %11lld %11lld %11lld\n",__func__,
		disp_stat->aod_state,
		ktime_to_ms(now),
		ktime_to_ms(active_time),
		ktime_to_ms(disp_stat->aod[disp_stat->aod_state].total_time),
		ktime_to_ms(max_time),
		ktime_to_ms(disp_stat->aod[disp_stat->aod_state].max_time_stamp));

	disp_stat->aod[state].last_time = now;
	disp_stat->aod[state].event_count++;
	disp_stat->aod_state = state;
}

static ktime_t _disp_stat_get_aod_active_time(int state)
{
	ktime_t active_time = ktime_set(0, 0);

	if (disp_stat->aod_state == state) {
		ktime_t now = ktime_get_boottime();

		active_time = ktime_sub(now, disp_stat->aod[state].last_time);
	}

	pr_debug("%s:[%d,%d] %11lld\n",__func__,
		disp_stat->aod_state, state,
		ktime_to_ms(active_time));

	return active_time;
}

static ktime_t _disp_stat_get_aod_total_time(int state)
{
	ktime_t total_time, active_time = ktime_set(0, 0);

	total_time = disp_stat->aod[state].total_time;

	if (disp_stat->aod_state == state) {
		active_time = _disp_stat_get_aod_active_time(state);
		total_time = ktime_add(total_time, active_time);
	}

	pr_debug("%s:[%d,%d] %11lld %11lld %11lld\n",__func__,
		disp_stat->aod_state, state,
		ktime_to_ms(disp_stat->aod[state].total_time),
		ktime_to_ms(active_time),
		ktime_to_ms(total_time));

	return total_time;
}

struct timespec disp_stat_get_aod_total_time(void)
{
	ktime_t total_time;

	total_time = _disp_stat_get_aod_total_time(AOD_ENTER);

	return ktime_to_timespec(total_time);
}

static ktime_t disp_stat_get_aod_max_time(int state)
{
	ktime_t max_time, active_time = ktime_set(0, 0);

	max_time = disp_stat->aod[state].max_time;

	if (disp_stat->aod_state == state) {
		ktime_t now = ktime_get_boottime();

		active_time = ktime_sub(now, disp_stat->aod[state].last_time);
		if (active_time.tv64 > max_time.tv64)
			max_time = active_time;
	}

	pr_debug("%s:[%d,%d] %11lld %11lld %11lld\n",__func__,
		disp_stat->aod_state, state,
		ktime_to_ms(disp_stat->aod[state].max_time),
		ktime_to_ms(active_time),
		ktime_to_ms(max_time));

	return max_time;
}

static ktime_t _disp_stat_get_fb_active_time(int state)
{
	ktime_t active_time = ktime_set(0, 0);

	if (disp_stat->fb_state == state) {
		ktime_t now = ktime_get();

		active_time = ktime_sub(now, disp_stat->fb[state].last_time);
	}

	pr_debug("%s:[%d,%d] %11lld\n",__func__,
		disp_stat->fb_state, state,
		ktime_to_ms(active_time));

	return active_time;
}

static ktime_t _disp_stat_get_fb_time(int state, int br_lv)
{
	ktime_t time, active_time = ktime_set(0, 0);

	time = disp_stat->fb[state].time[br_lv];

	if (disp_stat->fb_state == state && disp_stat->br_lv == br_lv) {
		active_time = _disp_stat_get_fb_active_time(state);
		time = ktime_add(time, active_time);
	}

	pr_debug("%s:[%d,%d][%d,%d] %11lld %11lld %11lld\n", __func__,
		disp_stat->fb_state, state, disp_stat->br_lv, br_lv,
		ktime_to_ms(disp_stat->fb[state].time[br_lv]),
		ktime_to_ms(active_time),
		ktime_to_ms(time));

	return time;
}

static ktime_t _disp_stat_get_fb_total_time(int state)
{
	ktime_t total_time, active_time = ktime_set(0, 0);

	total_time = disp_stat->fb[state].total_time;

	if (disp_stat->fb_state == state) {
		active_time = _disp_stat_get_fb_active_time(state);
		total_time = ktime_add(total_time, active_time);
	}

	pr_debug("%s:[%d,%d] %11lld %11lld %11lld\n",__func__,
		disp_stat->fb_state, state,
		ktime_to_ms(disp_stat->fb[state].total_time),
		ktime_to_ms(active_time),
		ktime_to_ms(total_time));

	return total_time;
}

struct timespec disp_stat_get_fb_total_time(void)
{
	ktime_t total_time;

	total_time = _disp_stat_get_fb_total_time(FB_BLANK_UNBLANK);

	return ktime_to_timespec(total_time);
}

static ktime_t disp_stat_get_fb_max_time(int state)
{
	ktime_t max_time, active_time = ktime_set(0, 0);

	max_time = disp_stat->fb[state].max_time;

	if (disp_stat->fb_state == state) {
		ktime_t now = ktime_get();

		active_time = ktime_sub(now, disp_stat->fb[state].last_time);
		if (active_time.tv64 > max_time.tv64)
			max_time = active_time;
	}

	pr_debug("%s:[%d,%d] %11lld %11lld %11lld\n",__func__,
		disp_stat->fb_state, state,
		ktime_to_ms(disp_stat->fb[state].max_time),
		ktime_to_ms(active_time),
		ktime_to_ms(max_time));

	return max_time;
}

#ifdef CONFIG_ENERGY_MONITOR
int disp_stat_get_stat(int type, struct disp_stat_emon *emon_stat)
{
	int i;
	ktime_t aod_total_time;
	ktime_t fb_time[NUM_DIST_STAT_BR_LEVELS] = {0,};

	if (!emon_stat)
		return -EINVAL;

	memset(emon_stat, 0, sizeof(*emon_stat));

	aod_total_time = _disp_stat_get_aod_total_time(AOD_ENTER);
	emon_stat->aod_time = ktime_sub(aod_total_time,
		disp_stat->aod[AOD_ENTER].emon_time);

	for (i = 0; i < disp_stat->num_br_lv_table; i++) {
		fb_time[i] = _disp_stat_get_fb_time(FB_BLANK_UNBLANK, i);
		emon_stat->fb_time[i] = ktime_sub(fb_time[i],
			disp_stat->fb[FB_BLANK_UNBLANK].emon_time[i]);
	}

	if (type != ENERGY_MON_TYPE_DUMP) {
		disp_stat->aod[AOD_ENTER].emon_time = aod_total_time;
		for (i = 0; i < disp_stat->num_br_lv_table; i++)
			disp_stat->fb[FB_BLANK_UNBLANK].emon_time[i] = fb_time[i];
	}

	return 0;
}
#endif

static int calculate_permile(s64 residency, s64 profile_time)
{
	if (!residency)
		return 0;

	residency *= 1000;
	do_div(residency, profile_time);

	return residency;
}

static int disp_stat_show(struct seq_file *m, void *unused)
{
	int i, j, percent_boot, percent_kernel;
	ktime_t boot_time;
	ktime_t kernel_time;
	ktime_t active_time;
	ktime_t total_time;
	ktime_t max_time;
	ktime_t max_time_stamp;
	ktime_t zero = ktime_set(0,0);

	boot_time = ktime_get_boottime();
	kernel_time = ktime_get();

	seq_puts(m, "display_state    event_count    "
		"boot_time      kernel_time    "
		"active_since   total_time     "
		"permile_b  permile_k    "
		"max_time        max_time_stamp    last_change\n");

	for (i = 0; i < AOD_EXIT + 1; i++) {
		active_time = _disp_stat_get_aod_active_time(i);
		total_time = _disp_stat_get_aod_total_time(i);
		max_time = disp_stat_get_aod_max_time(i);
		max_time_stamp = disp_stat->aod[i].max_time_stamp;
		percent_boot = calculate_permile(ktime_to_ms(total_time),
			ktime_to_ms(boot_time));
		percent_kernel = 0;

		if (ktime_equal(total_time, zero))
			continue;

		seq_printf(m, "aod[%-d]           %-12lu   "
			"%-12llu   %-12llu   "
			"%-12llu   %-12llu   "
			"%-4d       %-4d         "
			"%-12llu    %-12llu      %-12llu\n",
			i, disp_stat->aod[i].event_count,
			ktime_to_ms(boot_time), ktime_to_ms(kernel_time),
			ktime_to_ms(active_time), 	ktime_to_ms(total_time),
			percent_boot, percent_kernel,
			ktime_to_ms(max_time), ktime_to_ms(max_time_stamp),
			ktime_to_ms(disp_stat->aod[i].last_time));
	}

	for (i = 0; i < FB_BLANK_POWERDOWN + 1; i++) {
		active_time = _disp_stat_get_fb_active_time(i);
		total_time = _disp_stat_get_fb_total_time(i);
		max_time = disp_stat_get_fb_max_time(i);
		max_time_stamp = disp_stat->fb[i].max_time_stamp;
		percent_boot = calculate_permile(ktime_to_ms(total_time),
			ktime_to_ms(boot_time));
		percent_kernel = calculate_permile(ktime_to_ms(total_time),
			ktime_to_ms(kernel_time));

		if (ktime_equal(total_time, zero))
			continue;

		seq_printf(m, "fb[%-d]            %-12lu   "
			"%-12llu   %-12llu   "
			"%-12llu   %-12llu   "
			"%-4d       %-4d         "
			"%-12llu    %-12llu      %-12llu\n",
			i, disp_stat->fb[i].event_count,
			ktime_to_ms(boot_time), ktime_to_ms(kernel_time),
			ktime_to_ms(active_time), ktime_to_ms(total_time),
			percent_boot, percent_kernel,
			ktime_to_ms(max_time), ktime_to_ms(max_time_stamp),
			ktime_to_ms(disp_stat->fb[i].last_time));

		if (i == FB_BLANK_UNBLANK) {
			for (j = 0; j < disp_stat->num_br_lv_table; j++) {
				ktime_t time;
				time = _disp_stat_get_fb_time(i, j);

				if (ktime_equal(time, zero))
					continue;

				percent_boot = calculate_permile(ktime_to_ms(time),
					ktime_to_ms(boot_time));
				percent_kernel = calculate_permile(ktime_to_ms(time),
					ktime_to_ms(kernel_time));

				seq_printf(m, "     br[%-3d]     %-12u   "
					"%-12u   %-12u   "
					"%-12d   %-12llu   "
					"%-4d       %-4d         "
					"%-12u    %-12u      %-12u\n",
					disp_stat->br_lv_table[j], 0,
					0, 0,
					0, ktime_to_ms(time),
					percent_boot, percent_kernel,
					0, 0, 0);
			}
		}
	}

	return 0;
}

static int disp_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, disp_stat_show, NULL);
}

static const struct  file_operations disp_stat_fops = {
	.open = disp_stat_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int disp_stat_fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct backlight_device *bd;
	struct fb_event *evdata = data;
	int fb_blank = 0;
	int brightness = 0;

	if (event != FB_EVENT_BLANK && event != FB_EVENT_CONBLANK)
		return 0;

	fb_blank = *(int *)evdata->data;
	bd = backlight_device_get_by_type(BACKLIGHT_RAW);
	if (!bd) {
		pr_info("%s: cat't get backlight dev\n", __func__);
		brightness = -1;
	} else
		brightness = bd->props.brightness;

	pr_info("%s: %d: %d\n", __func__, fb_blank, brightness);

	switch (fb_blank) {
	case FB_BLANK_POWERDOWN:
	case FB_BLANK_NORMAL:
		disp_stat_fb_update(fb_blank, brightness);
		break;
	case FB_BLANK_UNBLANK:
		disp_stat_fb_update(fb_blank, brightness);
		break;
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_HSYNC_SUSPEND:
	default:
		break;
	}

	return 0;
}

static int __init disp_stat_dt_init(void)
{
	struct device_node *np;
	int i;
	int num_br_lv_table;
	unsigned int alloc_size;
	unsigned int *temp_array = NULL;

	np = of_find_node_by_name(NULL, "disp_stat");
	if (!np) {
		pr_err("%s:missing disp_stat in DT\n", __func__);
		goto fail;
	}

	/* alloc memory for br_lv */
	num_br_lv_table = of_property_count_u32_elems(np, "br_lv_table");
	if (num_br_lv_table < 0) {
		pr_err("%s: missing disp_stat,br_lv_table in DT\n", __func__);
		goto fail;
	}
	if (num_br_lv_table > NUM_DIST_STAT_BR_LEVELS) {
		pr_err("%s: num_br_lv_table should be smaller than %d\n",
			__func__, NUM_DIST_STAT_BR_LEVELS);
		goto fail;
	} else
		disp_stat->num_br_lv_table = num_br_lv_table;

	alloc_size = num_br_lv_table * sizeof(unsigned int);
	disp_stat->br_lv_table = kzalloc(alloc_size, GFP_KERNEL);
	if (!disp_stat->br_lv_table)
		goto fail;

	alloc_size = num_br_lv_table * sizeof(unsigned int);
	temp_array = kzalloc(alloc_size, GFP_KERNEL);
	if (!temp_array)
		goto fail;

	/* get br_lv */
	if (!of_property_read_u32_array(np, "br_lv_table", temp_array, num_br_lv_table)) {
		for (i = 0; i < num_br_lv_table; i++) {
			disp_stat->br_lv_table[i] = temp_array[i];
			pr_info("%s: br_lv_table[%d]=%d\n", __func__, i, disp_stat->br_lv_table[i]);
		}
	}
	else
		goto fail;

	kfree(temp_array);

	return 0;
fail:
	kfree(disp_stat->br_lv_table);
	kfree(temp_array);
	disp_stat->num_br_lv_table = 0;
	disp_stat->br_lv_table = NULL;
	pr_err("%s: parsing fail\n", __func__);

	return -1;
}

static int disp_stat_init(void)
{
	unsigned int ret = 0;

	pr_debug("%s\n", __func__);

	disp_stat = kzalloc(sizeof(*disp_stat), GFP_KERNEL);
	if ((disp_stat) == NULL)
		return -ENOMEM;

	ret = disp_stat_dt_init();
	if (ret < 0)
		goto err;

	memset(&disp_stat->fb_notif, 0, sizeof(disp_stat->fb_notif));
	disp_stat->fb_notif.notifier_call = disp_stat_fb_notifier_callback;
	ret = fb_register_client(&disp_stat->fb_notif);
	if (ret)
		goto err_fb;

	disp_stat->disp_stat_dentry = debugfs_create_file("disp_stat",
		0660, NULL, NULL, &disp_stat_fops);
	if (IS_ERR(disp_stat->disp_stat_dentry)) {
		ret =  PTR_ERR(disp_stat->disp_stat_dentry);
		goto err_debugfs;
	}

#ifdef CONFIG_SLEEP_MONITOR
	ret = sleep_monitor_register_ops(NULL, &disp_stat_sleep_monitor_ops,
				SLEEP_MONITOR_DISP);
	if (!ret)
		goto err_slm;
#endif

	disp_stat->fb_state = FB_BLANK_POWERDOWN;
	disp_stat->fb[disp_stat->fb_state].event_count++;
	disp_stat->aod_state = AOD_EXIT;
	disp_stat->aod[disp_stat->aod_state].event_count++;

	return ret;
#ifdef CONFIG_SLEEP_MONITOR
err_slm:
	debugfs_remove(disp_stat->disp_stat_dentry);
#endif
err_debugfs:
	fb_unregister_client(&disp_stat->fb_notif);
err_fb:
err:
	kfree(disp_stat);
	return ret;
}

static void disp_stat_exit(void)
{
	debugfs_remove(disp_stat->disp_stat_dentry);
	kfree(disp_stat);
	pr_debug("%s\n", __func__);
}

postcore_initcall(disp_stat_init);
module_exit(disp_stat_exit);
