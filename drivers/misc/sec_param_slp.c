/*
 *  drivers/misc/sec_param_slp.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sec_param_slp.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/sec_class.h>

#define PARAM_RD	0
#define PARAM_WR	1

#define SEC_PARAM_BLOCK_PATH_LEN	256
/* Max size limited by 4K, it depends on bootloader */
#define SEC_PARAM_BLOCK_MAX_SIZE	0x1000

/* single global instance */
struct sec_param_data *param_data;
char param_block[SEC_PARAM_BLOCK_PATH_LEN] = CONFIG_SEC_PARAM_BLOCK_PATH;

static bool sec_param_operation(void *value,
		int size, int direction)
{
	/* Read from PARAM(parameter) partition  */
	struct file *filp;
	mm_segment_t fs;
	int ret = true;
	int flag = (direction == PARAM_WR) ? (O_RDWR | O_SYNC) : O_RDONLY;
	loff_t total_size, offset;

	if (!param_block[0]) {
		pr_err("%s:can't find param block path\n", __func__);
		return false;
	}

	if (size > SEC_PARAM_BLOCK_MAX_SIZE) {
		pr_err("%s:can't support large SIZE\n", __func__);
		return false;
	}

	filp = filp_open(param_block, flag, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n",
				__func__, PTR_ERR(filp));
		return false;
	}

	fs = get_fs();
	set_fs(get_ds());

	/* find total size of param block */
	total_size = filp->f_op->llseek(filp, 0, SEEK_END);
	if (total_size < 0) {
		pr_err("%s FAIL LLSEEK to find end\n", __func__);
		ret = false;
		goto sec_param_debug_out;
	}

	offset = total_size - CONFIG_SEC_PARAM_BLOCK_EOFFSET;
	ret = filp->f_op->llseek(filp, offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s FAIL LLSEEK to set offset\n", __func__);
		ret = false;
		goto sec_param_debug_out;
	}

	if (direction == PARAM_RD)
		ret = filp->f_op->read(filp, (char __user *)value,
				size, &filp->f_pos);
	else if (direction == PARAM_WR)
		ret = filp->f_op->write(filp, (char __user *)value,
				size, &filp->f_pos);

	pr_debug("%s: %p %d %d done\n", __func__, value, size, direction);

sec_param_debug_out:
	set_fs(fs);
	filp_close(filp, NULL);
	return ret;
}

static bool sec_param_open(void)
{
	int ret = true;

	if (param_data != NULL)
		return true;

	param_data = kmalloc(sizeof(struct sec_param_data), GFP_KERNEL);

	ret = sec_param_operation(param_data,
			sizeof(struct sec_param_data), PARAM_RD);

	if (!ret) {
		kfree(param_data);
		param_data = NULL;
		pr_err("%s PARAM OPEN FAIL\n", __func__);
		return false;
	}

	return ret;

}

bool sec_param_get(enum sec_param_index index, void *value)
{
	int ret = true;
	ret = sec_param_open();
	if (!ret)
		return ret;

	switch (index) {
	case param_index_boot_alarm_set:
		memcpy(value, &(param_data->boot_alarm_set),
				sizeof(unsigned int));
		break;

	case param_index_boot_alarm_value_l:
		memcpy(value, &(param_data->boot_alarm_value_l),
				sizeof(unsigned int));
		break;

	case param_index_boot_alarm_value_h:
		memcpy(value, &(param_data->boot_alarm_value_h),
				sizeof(unsigned int));
		break;

	default:
		pr_info("%s: Unsupported index(%d)\n", __func__, index);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(sec_param_get);

bool sec_param_set(enum sec_param_index index, void *value)
{
	int ret = true;

	ret = sec_param_open();
	if (!ret)
		return ret;

	switch (index) {
	case param_index_boot_alarm_set:
		memcpy(&(param_data->boot_alarm_set),
				value, sizeof(unsigned int));
		break;

	case param_index_boot_alarm_value_l:
		memcpy(&(param_data->boot_alarm_value_l),
				value, sizeof(unsigned int));
		break;

	case param_index_boot_alarm_value_h:
		memcpy(&(param_data->boot_alarm_value_h),
				value, sizeof(unsigned int));
		break;

	default:
		pr_info("%s: Unsupported index(%d)\n", __func__, index);
		return false;
	}

	return sec_param_operation(param_data,
			sizeof(struct sec_param_data), PARAM_WR);
}
EXPORT_SYMBOL(sec_param_set);

/* ##########################################################
 * #
 * # SEC PARAM Driver sysfs files
 * ##########################################################
 * */
static struct device *sec_param_dev;

static struct sec_param_dev_attr param_devs[] = {
	{
		.node_name = "balarm_set",
		.index = param_index_boot_alarm_set,
	},
	{
		.node_name = "balarm_vall",
		.index = param_index_boot_alarm_value_l,
	},
	{
		.node_name = "balarm_valh",
		.index = param_index_boot_alarm_value_h,
	},
};

static ssize_t sec_param_dev_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_param_dev_attr *devattr = to_sec_param_dev_attr(attr);
	unsigned int temp;

	sec_param_get(devattr->index, &temp);

	return snprintf(buf, PAGE_SIZE, "%s:0x%X\n", devattr->node_name, temp);
}

#ifdef CONFIG_SLP_KERNEL_ENG
static ssize_t sec_param_dev_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	struct sec_param_dev_attr *devattr = to_sec_param_dev_attr(attr);
	unsigned int temp;
	int ret;

	ret = kstrtouint(buf, 16, &temp);
	if (ret)
		return ret;

	if (!sec_param_set(devattr->index, &temp))
		pr_err("%s: can't save param(%s)\n",
			__func__, devattr->node_name);

	return cnt;
}
#endif

static int __init sec_param_sysfs_init(void)
{
	int ret = 0, i;

	if (!sec_class) {
		printk(KERN_INFO "Can't find sec_class for sec_param");
		return 0;
	}

	sec_param_dev = device_create(sec_class,
					NULL, 0, NULL, "sec_param");

	if (IS_ERR(sec_param_dev)) {
		printk(KERN_ERR "Failed to create device(sec_param_dev)!!");
		return -ENODEV;
	}

	/* create each param nodes */
	for (i = 0; i < ARRAY_SIZE(param_devs); i++) {
		param_devs[i].dev_attr.attr.name = param_devs[i].node_name;
		param_devs[i].dev_attr.show = sec_param_dev_show;

	#ifdef CONFIG_SLP_KERNEL_ENG
		param_devs[i].dev_attr.store = sec_param_dev_store;
		param_devs[i].dev_attr.attr.mode = S_IRUGO | S_IWUGO;
	#else
		param_devs[i].dev_attr.store = NULL;
		param_devs[i].dev_attr.attr.mode = S_IWUGO;
	#endif

		sysfs_attr_init(&param_devs[i].dev_attr.attr);
		ret = device_create_file(sec_param_dev,
				&param_devs[i].dev_attr);
		if (ret)
			pr_err("%s: Failed to create node(%s): %d\n",
				__func__, param_devs[i].node_name, ret);
	}

	return 0;
}
fs_initcall(sec_param_sysfs_init);
