/*
 *  drivers/cpufreq/cpufreq_stats_tizen.h
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CPU_FREQ_STATS_TIZEN_H
#define __CPU_FREQ_STATS_TIZEN_H

#ifdef CONFIG_ENERGY_MONITOR
#define CPU_FREQ_STATS_TIZEN_MAX_STATE_NUM 16

struct cpufreq_stats_tizen {
	unsigned int state_num;
	u64 time_in_state[CPU_FREQ_STATS_TIZEN_MAX_STATE_NUM]; // temp fix
	unsigned int *freq_table;
};

/* struct cpufreq_stats_tizen *cpufreq_stats_tizen_alloc_stats_table(void);
void cpufreq_stats_tizen_free_stats_table(
					struct cpufreq_stats_tizen *stats_table);*/
int cpufreq_stats_tizen_get_stats(int type,
					struct cpufreq_stats_tizen *cpufreq_stats);
#endif

#endif /* __CPU_FREQ_STATS_TIZEN_H */

