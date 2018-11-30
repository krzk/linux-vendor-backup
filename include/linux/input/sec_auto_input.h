/*
 * include/linux/input/sec_auto_input.h
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/sec_sysfs.h>

#define DEVICE_NAME "sec_auto_input"

#define BUF_MAX 4096
#define EV_DELAY 0xff
#define DEFAULT_MAX_X 360
#define DEFAULT_MAX_Y 360
#define DEFAULT_MAX_Z 3000
#define FINGER_NUM 2
#define MT_TOOL_MAX 2


struct auto_input_data {
	struct device *dev;
	struct input_dev *input_dev;
	struct class *auto_input_class;
	int max_x;
	int max_y;
	int enabled;
	int sync_end;
	int prev_slot;
};
