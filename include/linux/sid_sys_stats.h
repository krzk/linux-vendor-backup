/*
 *  include/linux/sid_sys_stats.h
 *
 * Copyright (C) 2018 SAMSUNG, Inc.
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
 */

#ifndef __SID_SYS_STATS_H
#define __SID_SYS_STATS_H

#ifdef CONFIG_ENERGY_MONITOR
struct sid_sys_stats {
	u32 usid;
	uid_t uid;
	u32 sid;
	cputime_t utime;
	cputime_t stime;
	cputime_t ttime;
	u32 permil;
};

extern int get_sid_cputime(int type,
			struct sid_sys_stats *stat_array, size_t n);
#else
static inline int get_sid_cputime(int type, struct sid_sys_stats *stat_array)
{ return 0; }
#endif

#endif /* __SID_SYS_STATS_H */

