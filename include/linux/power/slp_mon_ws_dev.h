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

#ifndef _SLP_MON_WS_DEV_H
#define _SLP_MON_WS_DEV_H

#include <linux/ktime.h>

enum SLEEP_MON_WS_DEV_STATUS {
	SLP_MON_WS_NOT_ACTIVE = 0,
	SLP_MON_WS_ACTIVE = 1,
};

#define SLEEP_MON_WS_ARRAY_SIZE 4
#define SLEEP_MON_WS_NAME_LENGTH 15

#define SLEEP_MON_WS_IDX_BIT 24
#define SLEEP_MON_WS_PREVENT_TIME_MAX BIT(SLEEP_MON_WS_IDX_BIT) - 1

#define UNKNOWN_IRQ_NAME "UNKNOWN"

extern int add_slp_mon_ws_list(char *name, ktime_t prevent_time);
extern void init_ws_data(void);
#endif /* _SLP_MON_WS_DEV_H */



