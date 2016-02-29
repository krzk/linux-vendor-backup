/*
 * partron_ofm.h -PARTRON Optical Finger Mouse driver
 * For use with MS37C01A parts.
 *
 * Copyright 2011 PARTRON Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 #ifndef _LINUX_OFM_DRIVER_H
#define _LINUX_OFM_DRIVER_H

#define OFM_NAME "tizen_rotary"

struct ofm_platform_data {
	unsigned int	motion_pin;
	unsigned int	powerdown_pin;
	unsigned int	standby_pin;
	bool		always_on;
#ifdef OFM_DOME_BUTTON
	unsigned int	button_pin;
#endif
};

struct ofm {
	struct	i2c_client *client;
	struct	input_dev *input_dev;
	struct	ofm_platform_data *pdata;
	struct	wake_lock wake_lock;
	struct	device *sec_dev;
	struct delayed_work dwork_motion;
	wait_queue_head_t wait_q;

	bool	wakeup_state;
	bool	always_on;
	bool	power_state;
	int	motion_irq;
	int	x_sum;
	int	y_sum;
	int	motion_direction;
#ifdef OFM_DOME_BUTTON
	struct	hrtimer	timer_click;
	struct	hrtimer	timer_long;
	struct	hrtimer	timer_clear;
#endif
#ifdef OFM_KEY_MODE
	struct	mutex	ops_lock;
	ktime_t		dclick_time;
	ktime_t		lclick_time;
	ktime_t		clear_time;
	unsigned int	x_level;
	unsigned int	y_level;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

#endif
