/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Joong-Mock Shin <jmock.shin@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

 #ifndef _LINUX_OFM_DRIVER_H
#define _LINUX_OFM_DRIVER_H

#define ROTARY_NAME "tizen_rotary"
#define HALL_NAME "tizen_detent"
#define BUFF_SIZE_MAX	16

struct rotary_platform_data {
	unsigned int	motion_pin;
	unsigned int	powerdown_pin;
	unsigned int	standby_pin;
	bool		always_on;
	int		hall_a_pin;
	int		hall_b_pin;
};

struct rotary_ddata {
	struct	device *sec_dev;
	struct	i2c_client *client;
	struct	input_dev *mouse_dev;
	struct	input_dev *hall_dev;
	struct	rotary_platform_data *pdata;
	struct	wake_lock wake_lock;
	struct	delayed_work detect_dwork;
	struct	delayed_work motion_dwork;
	struct	mutex hall_mutex;
	atomic_t	ofm_pending;
	wait_queue_head_t wait_q;
	int	buff[BUFF_SIZE_MAX];
	int	buff_cnt;
	int	hall_a_irq;
	int	hall_b_irq;
	bool	hall_resume_state;
	bool	power_state;
	bool	a_status;
	bool	b_status;
	int	last_status;
	int	last_x_sum;
	int	hall_sum;
	bool	wakeup_state;
	bool	always_on;
	int	motion_irq;
	int	x_sum;
	int	y_sum;
	int 	prev_x_sum;
};
#endif
