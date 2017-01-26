/*
 * To log which interrupt wake AP up.
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Sanghyeon Lee <sirano06.lee@samsung.com>
 * Hunsup Jung <hunsup.jung@samsung.com>
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

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <asm/uaccess.h>

#include <linux/power/slp_mon_irq_dev.h>
#include <linux/power/sleep_monitor.h>

struct slp_mon_irq {
	struct list_head entry;
	char irq_name[SLEEP_MON_IRQ_NAME_LENGTH];
	int irq;
	int active;
};

static LIST_HEAD(slp_mon_irq_list);

/* Add irq to list for sleep monitor */
int add_slp_mon_irq_list(int irq, char *name)
{
	struct slp_mon_irq *irq_ins, *iter;
	char irq_name[SLEEP_MON_IRQ_NAME_LENGTH] = {0,};

	/* filter out UNKNOWN IRQ */
	if (!strncmp(name, UNKNOWN_IRQ_NAME, UNKNOWN_IRQ_NAME_LENGTH)) {
		snprintf(irq_name, SLEEP_MON_IRQ_NAME_LENGTH, "%d\n", irq);
	} else
		strncpy(irq_name, name, SLEEP_MON_IRQ_NAME_LENGTH);
	irq_name[SLEEP_MON_IRQ_NAME_LENGTH - 1] = 0;

	/* Already added irq */
	rcu_read_lock();
	list_for_each_entry_rcu(iter, &slp_mon_irq_list, entry) {
		if (!strncmp(iter->irq_name, irq_name, SLEEP_MON_IRQ_NAME_LENGTH - 1)) {
			iter->active = SLP_MON_IRQ_ACTIVE;
			rcu_read_unlock();
			return 0;
		}
	}

	/* Add new irq */
	irq_ins = kmalloc(sizeof(struct slp_mon_irq), GFP_ATOMIC);
	if (!irq_ins) {
		rcu_read_unlock();
		return -ENOMEM;
	}
	irq_ins->irq = irq;
	strncpy(irq_ins->irq_name, irq_name, SLEEP_MON_IRQ_NAME_LENGTH);
	irq_ins->irq_name[SLEEP_MON_IRQ_NAME_LENGTH - 1] = 0;
	irq_ins->active = SLP_MON_IRQ_ACTIVE;
	list_add_tail(&irq_ins->entry, &slp_mon_irq_list);
	rcu_read_unlock();

	return 0;
}

/* To get wakeup reason(irq), called by sleep monitor */
static int slp_mon_irq_cb(void *priv, unsigned int *raw_val,
								int check_level, int caller_type)
{
	struct slp_mon_irq *iter;
	int hit = 0;
	int idx = 0;

	if (caller_type == SLEEP_MONITOR_CALL_RESUME) {
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &slp_mon_irq_list, entry) {
			if (iter->active == SLP_MON_IRQ_ACTIVE) {
				*raw_val = idx;
				iter->active = SLP_MON_IRQ_NOT_ACTIVE;
				hit = 1;
				break;
			}
			idx++;
		}
		rcu_read_unlock();
	}

	return hit;
}

static struct sleep_monitor_ops slp_mon_irq_dev = {
	.read_cb_func = slp_mon_irq_cb,
};

/* Print irq list for sleep monitor */
static ssize_t slp_mon_read_irq_list(struct file *file,
        char __user *buffer, size_t count, loff_t *ppos)
{
	struct slp_mon_irq *iter;
	char *buf = NULL;
	ssize_t ret = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, PAGE_SIZE);

	if (*ppos == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[%08x]%s", special_key,
					get_type_marker(SLEEP_MONITOR_CALL_IRQ_LIST));
		rcu_read_lock();
		list_for_each_entry_rcu(iter, &slp_mon_irq_list, entry) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"%s/", iter->irq_name);
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

static const struct file_operations slp_mon_irq_list_ops = {
        .read           = slp_mon_read_irq_list,
};

static int slp_mon_irq_dev_init(void)
{
	pr_info("%s\n", __func__);
	sleep_monitor_register_ops(NULL, &slp_mon_irq_dev,	SLEEP_MONITOR_IRQ);

	if (slp_mon_d) {
		if (!debugfs_create_file("slp_mon_irq", S_IRUSR
			, slp_mon_d, NULL, &slp_mon_irq_list_ops)) \
				pr_err("%s : debugfs_create_file, error\n", "slp_mon_irq");
	} else
		pr_info("%s - dentry not defined\n", __func__);

	return 0;
}

static void slp_mon_irq_dev_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(slp_mon_irq_dev_init);
module_exit(slp_mon_irq_dev_exit);
