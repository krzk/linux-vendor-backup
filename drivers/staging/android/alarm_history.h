/*
 * To log which process cleared alarm.
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
 * Jeongsup Jeong <jeongsup.jeong@samsung.com>
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

#ifndef _ALARM_HISTORY_H
#define _ALARM_HISTORY_H

#include <linux/ktime.h>

#define ALMH_ARRAY_SIZE 4
#define ALMH_NAME_LENGTH 15
#define ALMH_IDX_BIT 24
#define ALMH_PREVENT_TIME_MAX BIT(ALMH_IDX_BIT) - 1

enum {
	ALARM_HISTORY_TYPE_CREATE = 0,
	ALARM_HISTORY_TYPE_EXPIRE,
};

#ifdef CONFIG_ENERGY_MONITOR
struct alarm_expire_stat {
	char comm[TASK_COMM_LEN];
	unsigned long expire_count;
	unsigned long wakeup_count;
};

void alarm_history_get_stat(int type,
	struct alarm_expire_stat *alarm_expire, size_t n);
#endif

#ifdef CONFIG_ALARM_HISTORY
extern void alarm_history_set_rtc_irq(void);
extern int add_alarm_history(int type, unsigned int pid);
#else
static inline void alarm_history_set_rtc_irq(void) { return 0; }
static inline int add_alarm_history(int type, unsigned int pid)
{ return 0; }
#endif /* CONFIG_ALARM_HISTORY */

#endif /* _ALARM_HISTORY_H */
