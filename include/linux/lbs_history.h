/* include/linux/lbs_history.h
 *
 * Copyright (C) 2017 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.07.21>              *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 2.0   Junho Jang <vincent.jang@samsung.com>        <2018.05.04>              *
 *                                                   refactoring           *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef __LBS_HISTORY_H
#define __LBS_HISTORY_H

enum lbs_method {
	LBS_METHOD_GPS = 0,
	LBS_METHOD_WPS,
	LBS_METHOD_AGPS,
	LBS_METHOD_GEOFENCE,
	LBS_METHOD_MOCK,
	LBS_METHOD_PASSIVE,
	LBS_METHOD_FUSED,
	LBS_METHOD_REMOTE_GPS,
	LBS_METHOD_REMOTE_WPS,
	LBS_METHOD_BATCH_GPS,
	LBS_METHOD_BATCH_REMOTE_GPS,
	LBS_METHOD_GPS_BATCH,
	LBS_METHOD_MAX,
};

struct lbs_usage {
	ktime_t total_time;
	ktime_t max_time;
	ktime_t max_time_stamp;
	ktime_t last_time;
	unsigned long active_count;
	unsigned int active;
};

struct lbs_stat_emon {
	u32 usid;
	uid_t uid;
	u32 sid;
	ktime_t total_time;
	struct lbs_usage usage[LBS_METHOD_MAX];
};

#ifdef CONFIG_LBS_HISTORY
extern int lbs_stat_get_stat(int type,
		struct lbs_stat_emon *emon_stat, size_t n);
#else
static inline int lbs_stat_get_stat(int type,
		struct lbs_stat_emon *emon_stat, size_t n);
{	return 0;}
#endif
#endif /* __LBS_HISTORY_H */
