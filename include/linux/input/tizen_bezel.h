/*
 * include/linux/input/tizen_bezel.h
 *
 * Copyright (C) 2018 Samsung Electronics Co.Ltd
 * Authors:
 *	Joong-Mock Shin <jmock.shin@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef TIZEN_BEZEL_H
#define TIZEN_BEZEL_H

#define BEZEL_NAME	"tizen_detent"

struct bezel_pdata {
	int gpio_a;
	int gpio_b;
	int gpio_c;
	const char *ldo_name;
};

struct bezel_ddata {
	struct platform_device	*pdev;
	struct input_dev		*input_dev;
	struct device		*rotary_dev;
	struct bezel_pdata		*pdata;
	struct regulator		*power;
	struct mutex		hall_lock;
	struct delayed_work	detect_dwork;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*gpio_config;
	struct work_struct		work;
	int gpio_a;
	int gpio_b;
	int gpio_c;
	int hall_a_irq;
	int hall_b_irq;
	int hall_c_irq;
	int last_status;
	int last_value;
	bool a_status;
	bool b_status;
	bool c_status;
	bool open_state;
	bool resume_state;
	bool probe_done;
	bool factory_mode;
#ifdef CONFIG_SLEEP_MONITOR
	u32 event_cnt;
#endif
};
#endif
