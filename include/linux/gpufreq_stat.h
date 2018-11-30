/*
 *  include/linux/gpufreq_stat.h
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __GPUFREQ_STAT_H
#define __GPUFREQ_STAT_H

#define GPUFREQ_STATS_MAX_STATE_NUM 8

struct emon_gpu_dvfs_info {
	 unsigned int clock;
	 unsigned long long time;
};

struct gpufreq_stat {
	struct emon_gpu_dvfs_info table[GPUFREQ_STATS_MAX_STATE_NUM]; // temp fix
	int table_size;
};

#if defined(CONFIG_GPUFREQ_STAT)
extern struct gpufreq_stat *gpufreq_stat_alloc_stat_table(void);
extern void gpufreq_stat_free_stat_table(
 					struct gpufreq_stat *stat_table);
extern int gpufreq_stat_get_stat(int type,
 					struct gpufreq_stat *gpufreq_stat);
extern int gpufreq_stat_get_stat_delta(
					struct gpufreq_stat *gpufreq_stat_delta);
#else
static inline struct gpufreq_stat *gpufreq_stat_alloc_stat_table(void)
{ return 0;};
static inline void gpufreq_stat_free_stat_table(
 					struct gpufreq_stat *stat_table)
{};
static inline int gpufreq_stat_get_stat(int type,
 					struct gpufreq_stat *gpufreq_stat)
{ return 0;};
static inline int gpufreq_stat_get_stat_delta(
					struct gpufreq_stat *gpufreq_stat_delta)
{ return 0;};
#endif
#endif /* __GPUFREQ_STAT_H */

