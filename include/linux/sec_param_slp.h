/*
 * include/linux/sec_param_slp.h
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _LINUX_MISC_SEC_PARAM_SLP_H
#define _LINUX_MISC_SEC_PARAM_SLP_H

#include <linux/device.h>

struct sec_param_data {
	unsigned int boot_alarm_set;
	unsigned int boot_alarm_value_l;
	unsigned int boot_alarm_value_h;
};

enum sec_param_index {
	param_index_boot_alarm_set,
	param_index_boot_alarm_value_l,
	param_index_boot_alarm_value_h,
};

struct sec_param_dev_attr {
	struct device_attribute dev_attr;
	const char *node_name;
	enum sec_param_index index;
};


#define to_sec_param_dev_attr(_dev_attr) \
	container_of(_dev_attr, struct sec_param_dev_attr, dev_attr)

extern bool sec_param_get(enum sec_param_index index, void *value);
extern bool sec_param_set(enum sec_param_index index, void *value);

#endif /* _LINUX_MISC_SEC_PARAM_SLP_H */
