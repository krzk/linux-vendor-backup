/*
 * To log which wakeup source prevent AP sleep.
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Sanghyeon Lee <sirano06.lee@samsung.com>
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


#include <linux/power/sleep_monitor.h>
#include <linux/power/slp_mon_ws_dev.h>
#include <linux/rcupdate.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>


struct slp_mon_ws {
	struct list_head entry;
	char ws_name[SLEEP_MON_WS_NAME_LENGTH];
	ktime_t prevent_time;
};

static LIST_HEAD(slp_mon_ws_list);

static int ws_idx[SLEEP_MON_WS_ARRAY_SIZE];
static int ws_num;
static int idx_cnt;

void init_ws_data(void)
{
	int i = 0;

	for(i = 0; i < ws_num; i++)
		ws_idx[i] = 0;
	ws_num = 0;
}

int add_slp_mon_ws_list(char *name, ktime_t prevent_time)
{
	struct slp_mon_ws *ws_ins, *iter;
	idx_cnt = 0;

	rcu_read_lock();
	list_for_each_entry_rcu(iter, &slp_mon_ws_list, entry) {
		if (!strncmp(iter->ws_name, name, SLEEP_MON_WS_NAME_LENGTH - 1)) {
			ws_idx[ws_num] = idx_cnt++;
			ws_num++;
			iter->prevent_time = prevent_time;
			rcu_read_unlock();
			return 0;
		}
		idx_cnt++;
	}
	rcu_read_unlock();

	ws_ins = kmalloc(sizeof(struct slp_mon_ws), GFP_KERNEL);
	if (!ws_ins)
		return -ENOMEM;
	memset(ws_ins->ws_name, 0, SLEEP_MON_WS_NAME_LENGTH);

	ws_idx[ws_num] = idx_cnt;
	ws_num++;
	idx_cnt++;
	strncpy(ws_ins->ws_name, name, SLEEP_MON_WS_NAME_LENGTH);
	ws_ins->ws_name[SLEEP_MON_WS_NAME_LENGTH - 1] = 0;
	ws_ins->prevent_time = prevent_time;
	list_add_tail(&ws_ins->entry, &slp_mon_ws_list);

	return 1;
}


static int slp_mon_ws_cb(void *priv, unsigned int *raw_val,
								int check_level, int caller_type)
{
	struct slp_mon_ws *iter;
	int i = 0;

	if (ws_num < 1)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &slp_mon_ws_list, entry) {
			if (ws_idx[0] == i) {
				*raw_val |= i << SLEEP_MON_WS_IDX_BIT;
				if (ktime_to_ms(iter->prevent_time) > SLEEP_MON_WS_PREVENT_TIME_MAX)
					iter->prevent_time =
					ktime_set(0, SLEEP_MON_WS_PREVENT_TIME_MAX * 1000);
				*raw_val |= ktime_to_ms(iter->prevent_time);
				break;
			}
			i++;
		}
		rcu_read_unlock();
	}

	if (ws_num == 1)
		init_ws_data();

	return 1;
}

static int slp_mon_ws1_cb(void *priv, unsigned int *raw_val,
								int check_level, int caller_type)
{
	struct slp_mon_ws *iter;
	int i = 0;

	if (ws_num < 2)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &slp_mon_ws_list, entry) {
			if (ws_idx[1] == i) {
				*raw_val |= i << SLEEP_MON_WS_IDX_BIT;
				if (ktime_to_ms(iter->prevent_time) > SLEEP_MON_WS_PREVENT_TIME_MAX)
					iter->prevent_time =
					ktime_set(0, SLEEP_MON_WS_PREVENT_TIME_MAX * 1000);
				*raw_val |= ktime_to_ms(iter->prevent_time);
				break;
			}
			i++;
		}
		rcu_read_unlock();
	}

	if (ws_num == 2)
		init_ws_data();

	return 1;
}

static int slp_mon_ws2_cb(void *priv, unsigned int *raw_val,
								int check_level, int caller_type)
{
	struct slp_mon_ws *iter;
	int i = 0;

	if (ws_num < 3)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &slp_mon_ws_list, entry) {
			if (ws_idx[2] == i) {
				*raw_val |= i << SLEEP_MON_WS_IDX_BIT;
				if (ktime_to_ms(iter->prevent_time) > SLEEP_MON_WS_PREVENT_TIME_MAX)
					iter->prevent_time =
					ktime_set(0, SLEEP_MON_WS_PREVENT_TIME_MAX * 1000);
				*raw_val |= ktime_to_ms(iter->prevent_time);
				break;
			}
			i++;
		}
		rcu_read_unlock();
	}

	if (ws_num == 3)
		init_ws_data();

	return 1;
}

static int slp_mon_ws3_cb(void *priv, unsigned int *raw_val,
								int check_level, int caller_type)
{
	struct slp_mon_ws *iter;
	int i = 0;

	if (ws_num < 4)
		return 0;

	if (caller_type == SLEEP_MONITOR_CALL_SUSPEND) {
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &slp_mon_ws_list, entry) {
			if (ws_idx[3] == i) {
				*raw_val |= i << SLEEP_MON_WS_IDX_BIT;
				if (ktime_to_ms(iter->prevent_time) > SLEEP_MON_WS_PREVENT_TIME_MAX)
					iter->prevent_time =
					ktime_set(0, SLEEP_MON_WS_PREVENT_TIME_MAX * 1000);
				*raw_val |= ktime_to_ms(iter->prevent_time);
				break;
			}
			i++;
		}
		rcu_read_unlock();
	}
	if (ws_num == 4)
		init_ws_data();

	return 1;
}

static struct sleep_monitor_ops slp_mon_ws_dev = {
	.read_cb_func = slp_mon_ws_cb,
};

static struct sleep_monitor_ops slp_mon_ws_dev1 = {
	.read_cb_func = slp_mon_ws1_cb,
};

static struct sleep_monitor_ops slp_mon_ws_dev2 = {
	.read_cb_func = slp_mon_ws2_cb,
};

static struct sleep_monitor_ops slp_mon_ws_dev3 = {
	.read_cb_func = slp_mon_ws3_cb,
};

static ssize_t slp_mon_read_ws_list(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
	struct slp_mon_ws *iter;
	char *buf = NULL;
	ssize_t ret = 0;
	int i = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, PAGE_SIZE);

	if (*ppos == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%08x]%s", special_key,
						get_type_marker(SLEEP_MONITOR_CALL_WS_LIST));
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &slp_mon_ws_list, entry) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%s/", iter->ws_name);
			i++;
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret,"\n");
		rcu_read_unlock();
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

static const struct file_operations slp_mon_ws_list_ops = {
        .read           = slp_mon_read_ws_list,
};

static int slp_mon_ws_dev_init(void)
{
	pr_info("%s\n", __func__);
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev, SLEEP_MONITOR_WS);
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev1, SLEEP_MONITOR_WS1);
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev2, SLEEP_MONITOR_WS2);
	sleep_monitor_register_ops(NULL, &slp_mon_ws_dev3, SLEEP_MONITOR_WS3);

	if (slp_mon_d) {
		if (!debugfs_create_file("slp_mon_ws", S_IRUSR
			, slp_mon_d, NULL, &slp_mon_ws_list_ops)) \
				pr_err("%s : debugfs_create_file, error\n", "slp_mon_ws");
	} else
		pr_info("%s - dentry not defined\n", __func__);

	return 0;
}

static void slp_mon_ws_dev_exit(void)
{
	pr_info("%s\n", __func__);

}

module_init(slp_mon_ws_dev_init);
module_exit(slp_mon_ws_dev_exit);
