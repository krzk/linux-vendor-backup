/*
 * debugfs file to keep track of suspend
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Sangin Lee <sangin78.lee@samsung.com>
 * Sanghyeon Lee <sirano06.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the impliesd warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/power/sleep_monitor.h>
#include <linux/power_supply.h>
#include <linux/rtc.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/suspend.h>


enum SLEEP_MONITOR_DEBUG_LEVEL {
	SLEEP_MONITOR_DEBUG_LABEL =  BIT(0),
	SLEEP_MONITOR_DEBUG_INFO = BIT(1),
	SLEEP_MONITOR_DEBUG_ERR = BIT(2),
	SLEEP_MONITOR_DEBUG_DEVICE = BIT(3),
	SLEEP_MONITOR_DEBUG_DEBUG = BIT(4),
	SLEEP_MONITOR_DEBUG_WORK = BIT(5),
	SLEEP_MONITOR_DEBUG_INIT_TIMER = BIT(6),
	SLEEP_MONITOR_DEBUG_READ_SUS_RES_TIME = BIT(7),
};

#define sleep_mon_dbg(debug_level_mask, fmt, ...)				\
do {									\
	if (debug_level & debug_level_mask)	\
		pr_info("%s[%d]" fmt, SLEEP_MONITOR_DEBUG_PREFIX, 	\
						debug_level_mask, ##__VA_ARGS__);		\
} while (0)


typedef struct sleep_monitor_device {
	struct sleep_monitor_ops *sm_ops;
	void *priv;
	int check_level;
	bool skip_device;
	char *device_name;
	ktime_t sus_res_time[2];
} sleep_monitor_device;

static char *type_text[] = {
	"suspend", "resume",
};

static sleep_monitor_device slp_mon[SLEEP_MONITOR_NUM_MAX] = {
	/* CAUTION!!! Need to sync with SLEEP_MONITOR_DEVICE in sleep_monitor.h */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"BT      " }, /* SLEEP_MONITOR_BT */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"WIFI    " }, /* SLEEP_MONITOR_WIFI */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"MODEM   " }, /* SLEEP_MONITOR_MODEM */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"GPS     " }, /* SLEEP_MONITOR_GPS */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"NFC     " }, /* SLEEP_MONITOR_NFC */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SENSOR  " }, /* SLEEP_MONITOR_SENSOR */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SENSOR1 " }, /* SLEEP_MONITOR_SENSOR1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"AUDIO   " }, /* SLEEP_MONITOR_AUDIO */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPA    " }, /* SLEEP_MONITOR_SAPA */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPA1   " }, /* SLEEP_MONITOR_SAPA1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPB    " }, /* SLEEP_MONITOR_SAPB */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"SAPB1   " }, /* SLEEP_MONITOR_SAPB1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"CONHR   " }, /* SLEEP_MONITOR_CONHR */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"KEY     " }, /* SLEEP_MONITOR_KEY */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"TEMP    " }, /* SLEEP_MONITOR_TEMP */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"CPU_UTIL" }, /* SLEEP_MONITOR_CPU_UTIL */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"LCD     " }, /* SLEEP_MONITOR_LCD */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"TSP     " }, /* SLEEP_MONITOR_TSP */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"ROTARY  " }, /* SLEEP_MONITOR_ROTARY */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"REGUL   " }, /* SLEEP_MONITOR_REGULATOR */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"REGUL1  " }, /* SLEEP_MONITOR_REGULATOR1 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"PMDOMAIN" }, /* SLEEP_MONITOR_PMDOMAINS */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"BATTERY " }, /* SLEEP_MONITOR_BATTERY */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"DEV23   " }, /* SLEEP_MONITOR_DEV23 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"DEV24   " }, /* SLEEP_MONITOR_DEV24 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"DEV25   " }, /* SLEEP_MONITOR_DEV25 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"DEV26   " }, /* SLEEP_MONITOR_DEV26 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"DEV27   " }, /* SLEEP_MONITOR_DEV27 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"DEV28   " }, /* SLEEP_MONITOR_DEV28 */
	{NULL, NULL, SLEEP_MONITOR_CHECK_HARD, SLEEP_MONITOR_BOOLEAN_FALSE,"TCP     " }, /* SLEEP_MONITOR_TCP */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"SYST    " }, /* SLEEP_MONITOR_SYS_TIME */
	{NULL, NULL, SLEEP_MONITOR_CHECK_SOFT, SLEEP_MONITOR_BOOLEAN_FALSE,"RTCT    " }, /* SLEEP_MONITOR_RTC_TIME */
};
static int debug_level = SLEEP_MONITOR_DEBUG_LABEL |
					SLEEP_MONITOR_DEBUG_INFO |
					SLEEP_MONITOR_DEBUG_ERR |
					SLEEP_MONITOR_DEBUG_DEVICE |
					SLEEP_MONITOR_DEBUG_INIT_TIMER;

static int sleep_monitor_enable = 1;
static struct timer_list timer;
#define SLEEP_MONITOR_MAX_MONITOR_INTERVAL 3600 /* second */
static unsigned int monitor_interval = 0; /* 0: Disable, 1~3600: Enable . default value need to set to 0 or > 30 */
static struct task_struct *monitor_kthread;
/**
* @brief : Register sleep monitor ops by devices.
*
* @param-ops : Sleep_monitor_ops pointer. It need to implemetns call back function.
* @param-device_type : It's device enum. Many device separated by this. It's located in sleep_monitor.h
*
* @return 0 on success, otherwise a negative error value.
*
*/
int sleep_monitor_register_ops(void *priv, struct sleep_monitor_ops *ops, int device_type)
{
	if (device_type < 0 && device_type >= SLEEP_MONITOR_NUM_MAX) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "device type error\n");
		return -1;
	}

	if (priv)
		slp_mon[device_type].priv = priv;

	if (ops) {
		if (ops->read_cb_func || ops->read64_cb_func)
			slp_mon[device_type].sm_ops = ops;
		else {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "read cb function error\n");
			return -1;
		}
	} else {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "ops not valid\n");
		return -1;
	}
	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEVICE,
		"sleep_monitor register success type:%d(%s)\n", device_type,
									slp_mon[device_type].device_name);
	return 0;
}
EXPORT_SYMBOL_GPL(sleep_monitor_register_ops);

/**
* @brief : Unregister sleep monitor ops by devices.
*
* @param-device_type : It's device enum. Each device separated by this. It's located in sleep_monitor.h
*
* @return 0 on success, otherwise a negative error value.
*
*/
int sleep_monitor_unregister_ops(int device_type)
{
	if (device_type < 0 && device_type >= SLEEP_MONITOR_NUM_MAX) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "device type error\n");
		return -1;
	}
	slp_mon[device_type].priv = NULL;
	slp_mon[device_type].sm_ops = NULL;
	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEVICE, "sleep_monitor unregister success\n");
	return 0;

}
EXPORT_SYMBOL_GPL(sleep_monitor_unregister_ops);

/**
* @brief : Return result as raw format from devices.
*
* @param-pretty_group : It's assigned value as raw format.
*
* @return 0 on success, otherwise a negative error value.
*
*/

int sleep_monitor_get_raw_value(int *raw_value)
{
	int i = 0, ret = 0;

	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if (SLEEP_MONITOR_BOOLEAN_TRUE == slp_mon[i].skip_device)
			continue;
		if (slp_mon[i].sm_ops && slp_mon[i].sm_ops->read_cb_func) {
			ret = slp_mon[i].sm_ops->read_cb_func(slp_mon[i].priv, &raw_value[i],
									slp_mon[i].check_level, SLEEP_MONITOR_CALL_ETC);
			if (ret < 0) {
				sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, 
				"%s device cb func error: %d\n", slp_mon[i].device_name, ret);
				raw_value[i] = DEVICE_ERR_1;
			}
		} else {
			raw_value[i] = DEVICE_ERR_1;
		}
	}
	return 0;
}
EXPORT_SYMBOL_GPL(sleep_monitor_get_raw_value);

static char* get_type_marker(int type)
{
	switch (type) {
		case SLEEP_MONITOR_CALL_SUSPEND : return "+";
		case SLEEP_MONITOR_CALL_RESUME : return "-";
		case SLEEP_MONITOR_CALL_ETC: return "";
		default : return "\0";
	}
}

/**
* @brief : Return result as pretty format from devices.
*
* @param-pretty_group : It's assigned value as pretty format.
*
* @return 0 on success, otherwise a negative error value.
*
*/
int sleep_monitor_get_pretty(int *pretty_group, int type)
{
	int i = 0, temp_pretty = 0, ret = 0;
	int mask = 0, shift = 0, offset = 0;
	int raw_value[SLEEP_MONITOR_NUM_MAX] = {0,};
	char* buf = NULL;
	static int first_time = 1;
	ktime_t diff_time[2] = {ktime_set(0,0), };

	buf = (char*)kmalloc(SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char), GFP_KERNEL);

	if (!buf)
		return -1;

	memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));
	if (!sleep_monitor_enable) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG, "sleep_monitor is not enabled\n");
		kfree(buf);
		return -1;
	}

	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if (SLEEP_MONITOR_BOOLEAN_TRUE == slp_mon[i].skip_device)
			continue;

		if ((SLEEP_MONITOR_DEBUG_READ_SUS_RES_TIME & debug_level) &&
						type != SLEEP_MONITOR_CALL_ETC)
			diff_time[0] = ktime_get();

		if (slp_mon[i].sm_ops) {
			if (slp_mon[i].sm_ops->read_cb_func)
				temp_pretty = slp_mon[i].sm_ops->read_cb_func(slp_mon[i].priv,
										&raw_value[i], slp_mon[i].check_level, type);
			else if (slp_mon[i].sm_ops->read64_cb_func)
				temp_pretty = slp_mon[i].sm_ops->read64_cb_func(slp_mon[i].priv,
									(long long *)&raw_value[i], slp_mon[i].check_level, type);
			else
				temp_pretty = 0x0;
		} else {
			temp_pretty = 0x0;
		}

		if ((SLEEP_MONITOR_DEBUG_READ_SUS_RES_TIME & debug_level) &&
							(type != SLEEP_MONITOR_CALL_ETC)) {
			diff_time[1] = ktime_get();
			slp_mon[i].sus_res_time[type] = ktime_sub(diff_time[1], diff_time[0]);
			pr_info("[slp_mon]-%s %s - %lld(ns)\n",type_text[type], slp_mon[i].device_name, ktime_to_ns(slp_mon[i].sus_res_time[type]));
		}

		if (temp_pretty < 0) {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR,
				"%s device cb func error: %d\n", slp_mon[i].device_name, temp_pretty);
			temp_pretty = DEVICE_UNKNOWN;
		}

		mask = (1 << SLEEP_MONITOR_DEVICE_BIT_WIDTH) - 1;
		shift = offset;
		temp_pretty &= mask;
		temp_pretty <<= shift;
		pretty_group[i/(SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE)] += temp_pretty;
		if (offset == (SLEEP_MONITOR_BIT_INT_SIZE - SLEEP_MONITOR_DEVICE_BIT_WIDTH) )
			offset = 0;
		else
			offset += SLEEP_MONITOR_DEVICE_BIT_WIDTH;
	}


	/* Print device name at first time or debug level is enabled */
	if (first_time || (debug_level & SLEEP_MONITOR_DEBUG_DEBUG)) {
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			if((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == 0)
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%s[%02d-%02d]", get_type_marker(type), i, i + 7);

			ret += snprintf(buf + ret, PAGE_SIZE-ret, "%8s/",slp_mon[i].device_name);
			if (((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == SLEEP_MONITOR_NUM_DEVICE_RAW_VAL - 1) && (i != 1)) {
				sleep_mon_dbg(SLEEP_MONITOR_DEBUG_LABEL, "%s\n", buf);
				memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));
				ret = 0;
			}
		}
		first_time = 0;
	}

	for (i = SLEEP_MONITOR_GROUP_SIZE - 1; i >= 0; i--) {
		ret += snprintf(buf + ret,PAGE_SIZE-ret, "%08x/",pretty_group[i]);
	}
	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_INFO, "%s[pretty@%04d]%s\n",
					get_type_marker(type), suspend_stats.success + 1, buf);

	memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));
	ret = 0;

	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if ((i%SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == 0)
			ret += snprintf(buf + ret,PAGE_SIZE-ret,
				"%s[%02d-%02d]", get_type_marker(type), i, i+7);
		ret += snprintf(buf + ret,PAGE_SIZE-ret, "%08x/",raw_value[i]);
		if (((i %SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == SLEEP_MONITOR_NUM_DEVICE_RAW_VAL-1) && (i != 1)) {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_INFO, "%s\n", buf);
			memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));
			ret = 0;
		}
	}

	kfree(buf);
	return 0;
}
EXPORT_SYMBOL_GPL(sleep_monitor_get_pretty);


static int show_device_raw_status(struct seq_file *s, void *data)
{
	int raw_value[SLEEP_MONITOR_NUM_MAX] = {0,};
	int pretty_value[SLEEP_MONITOR_NUM_MAX] = {0,};
	int oneline_pretty_value[SLEEP_MONITOR_GROUP_SIZE] = {0,};
	int i = 0, mask = 0, shift = 0, offset = 0, temp_pretty = 0;
	bool is_read64 = SLEEP_MONITOR_BOOLEAN_FALSE;

	if (!sleep_monitor_enable) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG, "sleep_monitor disable\n");
		return -1;
	}

	seq_printf(s, " idx dev     pretty    raw  read suspend time   read resumetime             cb\n");

	for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
		if (SLEEP_MONITOR_BOOLEAN_TRUE == slp_mon[i].skip_device)
			continue;

		if (slp_mon[i].sm_ops) {
			if (slp_mon[i].sm_ops->read_cb_func) {
				pretty_value[i] = slp_mon[i].sm_ops->read_cb_func(slp_mon[i].priv,
										&raw_value[i],slp_mon[i].check_level, SLEEP_MONITOR_CALL_ETC);
				is_read64 = SLEEP_MONITOR_BOOLEAN_FALSE;
			} else if(slp_mon[i].sm_ops->read64_cb_func) {
				pretty_value[i] = slp_mon[i].sm_ops->read64_cb_func(slp_mon[i].priv,
									(long long*)&raw_value[i],slp_mon[i].check_level, SLEEP_MONITOR_CALL_ETC);
				is_read64 = SLEEP_MONITOR_BOOLEAN_TRUE;
			} else
				pretty_value[i] = 0x0;
		} else {
			pretty_value[i] = 0x0;
		}

		temp_pretty = pretty_value[i];
		mask = (1 << SLEEP_MONITOR_DEVICE_BIT_WIDTH) -1;
		shift = offset;
		temp_pretty &= mask;
		temp_pretty <<= shift;
		oneline_pretty_value[i/(SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE)] += temp_pretty;

		if (offset == (SLEEP_MONITOR_BIT_INT_SIZE - SLEEP_MONITOR_DEVICE_BIT_WIDTH) )
			offset = 0;
		else
			offset += SLEEP_MONITOR_DEVICE_BIT_WIDTH;

		if (SLEEP_MONITOR_BOOLEAN_FALSE == is_read64)
			seq_printf(s, "%2d %s (0x%x)  0x%08x %8lld(ns)  %8lld(ns)         %pf\n", i, slp_mon[i].device_name,
					pretty_value[i],raw_value[i], ktime_to_ns(slp_mon[i].sus_res_time[0]),
					ktime_to_ns(slp_mon[i].sus_res_time[1]), (slp_mon[i].sm_ops != NULL) ?
					slp_mon[i].sm_ops->read_cb_func : NULL );
		else
			seq_printf(s, "%2d %s (0x%x)  0x%08x %8lld(ns)  %8lld(ns)          %pf\n", i, slp_mon[i].device_name,
					pretty_value[i],raw_value[i], ktime_to_ns(slp_mon[i].sus_res_time[0]),
					ktime_to_ns(slp_mon[i].sus_res_time[1]), slp_mon[i].sm_ops->read64_cb_func);

		is_read64 = SLEEP_MONITOR_BOOLEAN_FALSE;
	}

	for (i = SLEEP_MONITOR_GROUP_SIZE-1; i >= 0 ; i--) {
		seq_printf(s, "%08x/", oneline_pretty_value[i]);
	}
	seq_printf(s, "\n");

	return 0;
}


static int monitor_thread(void *data)
{
	int pretty_group_buf[SLEEP_MONITOR_GROUP_SIZE] = {0,};
	int thread_cnt = 0;

	while(1) {
		thread_cnt++;
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG,
			"monitor_thread: (count=%d)\n", thread_cnt);

		if (monitor_interval) {
			/* All check routine will be excuted
				when monitor is enabled. (monitor_interval > 0) */
			sleep_monitor_get_pretty(pretty_group_buf, SLEEP_MONITOR_CALL_ETC);
		}
		msleep(100);
		set_current_state(TASK_INTERRUPTIBLE);

		if (monitor_interval)
			schedule_timeout(monitor_interval * HZ);
		else {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_WORK,
				"monior thread is destroyed (pid=%d)\n", monitor_kthread->pid);
			monitor_kthread = NULL;
			break;
		}
	}
	return 0;
}

static int create_monitor_thread(void *data)
{
	if (monitor_kthread)
		return 0;

	monitor_kthread = kthread_run(monitor_thread, NULL, "slp_mon");
	if (IS_ERR(monitor_kthread)) {
			sleep_mon_dbg(SLEEP_MONITOR_DEBUG_ERR, "Fail to create kthread\n");
			return -1;
	}

	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_WORK,
		"monior thread is created (pid=%d)\n", monitor_kthread->pid);

	return 0;

}

static int sleep_monitor_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_device_raw_status, NULL);
}

static ssize_t read_skip_device(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	int i = 0;
	char *buf;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			ret += snprintf(buf + ret,PAGE_SIZE-ret, "%2d %s %d\n", i,
								slp_mon[i].device_name, slp_mon[i].skip_device);
		}
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static ssize_t write_skip_device(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int device = 0;
	int is_skip = 0;

	sscanf(user_buf,"%d--%d",&device, &is_skip);
	if (device >= 0 && device < SLEEP_MONITOR_NUM_MAX) {
		if (is_skip >= 0 && is_skip <= 1) {
			slp_mon[device].skip_device = (is_skip == 0 ? SLEEP_MONITOR_BOOLEAN_FALSE :
								 SLEEP_MONITOR_BOOLEAN_TRUE);
		}
	}
	return count;
}


static ssize_t read_check_device_level(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	int i = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0){
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			if (slp_mon[i].skip_device == SLEEP_MONITOR_BOOLEAN_TRUE)
				continue;
			ret += snprintf(buf + ret,PAGE_SIZE-ret, "%2d %s %d\n", i,
								slp_mon[i].device_name, slp_mon[i].check_level);
		}
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static ssize_t write_check_device_level(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int device = 0, depth = 0;

	sscanf(user_buf,"%d--%d",&device, &depth);
	if (device >= 0 && device < SLEEP_MONITOR_NUM_MAX) {
		slp_mon[device].check_level = depth;
	}
	return count;
}

static ssize_t read_device_pretty_value(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	int i = 0;
	int pretty_group[SLEEP_MONITOR_GROUP_SIZE] = {0,};

	if (!sleep_monitor_enable) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEBUG, "sleep_monitor disable\n");
		return -1;
	}

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		sleep_monitor_get_pretty(pretty_group, SLEEP_MONITOR_CALL_ETC);
		for (i = SLEEP_MONITOR_GROUP_SIZE-1; i >= 0 ; i--) {
			ret += snprintf(buf + ret,PAGE_SIZE-ret, "%08x/",pretty_group[i]);
		}
		ret += snprintf(buf + ret,PAGE_SIZE-ret, "\n");
	}

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static ssize_t read_monitor_interval(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char buf[10];

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0)
		ret += snprintf(buf,sizeof(buf), "%u\n", monitor_interval);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			return -EFAULT;
		}
		*ppos += ret;
	}

	return ret;
}

static ssize_t write_monitor_interval(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	unsigned int new_interval = 0;

	sscanf(user_buf,"%u",&new_interval);

	/* Maximum interval is 1 hour */
	if (new_interval > SLEEP_MONITOR_MAX_MONITOR_INTERVAL)
		new_interval = SLEEP_MONITOR_MAX_MONITOR_INTERVAL;

	monitor_interval = new_interval;

	if (monitor_interval > 0 && !monitor_kthread) {
		create_monitor_thread(NULL);
	} else if ( monitor_interval == 0 && monitor_kthread) {
		sleep_mon_dbg(SLEEP_MONITOR_DEBUG_WORK,
			"monior thread is stopped (pid=%d)\n", monitor_kthread->pid);
		kthread_stop(monitor_kthread);
		monitor_kthread = NULL;
	}

	return count;
}

static ssize_t read_dev_name(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	int i = 0;
	char *buf = NULL;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = (char*)kmalloc(4 * SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char), GFP_KERNEL);

	if (!buf)
		return -1;

	memset(buf, 0, SLEEP_MONITOR_ONE_LINE_RAW_SIZE * sizeof(char));

	if (*ppos == 0) {
		for (i = 0; i < SLEEP_MONITOR_NUM_MAX; i++) {
			if((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == 0)
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%s[%02d-%02d]", get_type_marker(SLEEP_MONITOR_CALL_ETC), i, i + 7);

			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%8s/", slp_mon[i].device_name);
			if (((i % SLEEP_MONITOR_NUM_DEVICE_RAW_VAL) == SLEEP_MONITOR_NUM_DEVICE_RAW_VAL - 1) && (i != 1))
				ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		}
	}
	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static void sleep_monitor_booting_info_log(unsigned long data)
{
	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_INIT_TIMER,
					"%lld\n", (long long)current_kernel_time().tv_sec -
							SLEEP_MONITOR_AFTER_BOOTING_TIME);
}

static const struct file_operations sleep_monitor_debug_fops = {
	.open		= sleep_monitor_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations skip_device_fops = {
	.write		= write_skip_device,
	.read		= read_skip_device,
};

static const struct file_operations device_level_fops = {
	.write		= write_check_device_level,
	.read		= read_check_device_level,
};

static const struct file_operations device_pretty_fops = {
	.read		= read_device_pretty_value,
};

static const struct file_operations monitor_interval_fops = {
	.write		= write_monitor_interval,
	.read		= read_monitor_interval,
};

static const struct file_operations dev_name_fops = {
        .read           = read_dev_name,
};

 int __init sleep_monitor_debug_init(void)
{
	struct dentry *d;

	init_timer(&timer);
	timer.function = sleep_monitor_booting_info_log;
	mod_timer(&timer, jiffies + msecs_to_jiffies
						(SLEEP_MONITOR_AFTER_BOOTING_TIME * 1000));
	d = debugfs_create_dir("sleep_monitor", NULL);
	if (d) {
		if (!debugfs_create_file("status", S_IRUSR
			, d, NULL, &sleep_monitor_debug_fops)) \
				pr_err("%s : debugfs_create_file, error\n", "status");

		if (!debugfs_create_file("skip_device", S_IRUSR | S_IWUSR
			, d, NULL, &skip_device_fops))  \
				pr_err("%s : debugfs_create_file, error\n", "skip_device");

		if (!debugfs_create_file("check_device_level", S_IRUSR | S_IWUSR
			, d, NULL, &device_level_fops))  \
				pr_err("%s : debugfs_create_file, error\n", "check_device_level");

		if (!debugfs_create_file("pretty_status", S_IRUSR
			, d, NULL, &device_pretty_fops))  \
				pr_err("%s : debugfs_create_file, error\n", "pretty_status");

		if (!debugfs_create_file("monitor_interval", S_IRUSR | S_IWUSR
			, d, NULL, &monitor_interval_fops))  \
				pr_err("%s : debugfs_create_file, error\n", "monitor_interval");

		if (!debugfs_create_file("dev_name", S_IRUSR
                        , d, NULL, &dev_name_fops))  \
                                pr_err("%s : debugfs_create_file, error\n", "dev_name");

		debugfs_create_u32("debug_level", S_IRUSR | S_IWUSR, d, &debug_level);
		debugfs_create_u32("enable", S_IRUSR | S_IWUSR, d, &sleep_monitor_enable);
	}

	if (monitor_interval > 0 && !monitor_kthread) {
		create_monitor_thread(NULL);
	}

	sleep_mon_dbg(SLEEP_MONITOR_DEBUG_DEVICE, "sleep_monitor initialized\n");
	return 0;
}

postcore_initcall(sleep_monitor_debug_init);
