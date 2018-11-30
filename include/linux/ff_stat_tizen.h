/*
 *  include/linux/ff_stat_tizen.h
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __FF_STAT_TIZEN_H
#define __FF_STAT_TIZEN_H

#include <linux/input.h>

#ifdef CONFIG_ENERGY_MONITOR
 struct ff_stat_emon {
	 ktime_t total_time;
	 unsigned long play_count;
 };

extern int ff_stat_tizen_get_stat(int type, struct ff_stat_emon *emon_stat);
#endif
#endif /* __FF_STAT_TIZEN_H */

