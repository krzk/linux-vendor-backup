/*
 * Copyright (C) 2017 SAMSUNG, Inc.
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
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Hunsup Jung <hunsup.jung@samsung.com>       2017.10.23                *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 2.0   Junho Jang <vincent.jang@samsung.com>       Add event_count, max_time    *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef _DISP_STAT_H
#define _DISP_STAT_H

/* TODO:  Currently we only support 10 backlight levels */
#define NUM_DIST_STAT_BR_LEVELS 10

struct disp_stat_emon {
	ktime_t fb_time[NUM_DIST_STAT_BR_LEVELS];
	ktime_t aod_time;
};

#ifdef CONFIG_DISP_STAT
extern struct timespec disp_stat_get_fb_total_time(void);
extern void disp_stat_aod_update(int state);
extern struct timespec disp_stat_get_aod_total_time(void);
extern int disp_stat_get_stat(int type, struct disp_stat_emon *emon_stat);
#else
static inline struct timespec disp_stat_get_fb_total_time(void)
{
	struct timespec ts_null = {0, 0};

	return ts_null;
}
static inline void disp_stat_aod_update(int state) {}
static inline struct timespec disp_stat_get_aod_total_time(void)
{
	struct timespec ts_null = {0, 0};

	return ts_null;
}
static inline int disp_stat_get_stat(int type,
	struct disp_stat_emon *emon_stat)
{	return 0;}
#endif /* CONFIG_DISP_STAT */
#endif /* _DISP_STAT_H */
