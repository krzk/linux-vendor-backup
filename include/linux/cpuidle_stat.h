/*
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CPUIDLE_STAT_H
#define CPUIDLE_STAT_H __FILE__

#include <linux/ktime.h>
#include <linux/cpuidle.h>
#include <linux/psci.h>

#include <asm/cputype.h>
#include <soc/samsung/exynos-powermode.h>

/*
 * cpuidle major state
 */
#define CPU_IDLE_C1		0
#define CPU_IDLE_C2		1
#define CPU_IDLE_LPM		2

#define has_idle_sub_state(_state)	( _state > CPU_IDLE_C1 ? 1 : 0)

/*
 * C2 sub state
 */
#define C2_CPD			PSCI_CLUSTER_SLEEP
#define C2_SICD			PSCI_SYSTEM_IDLE
#define C2_SICD_CPD		PSCI_SYSTEM_IDLE_CLUSTER_SLEEP

/*
 * LPM sub state
 */
#define	LPM_SICD		SYS_SICD
#define	LPM_AFTR		SYS_AFTR
#define	LPM_STOP		SYS_STOP
#define	LPM_LPD			SYS_LPD

#define to_cluster(cpu)		MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1)

struct cpuidle_stat_state_usage {
	unsigned int entry_count;
	unsigned int early_wakeup_count;
	ktime_t time;
};

struct cpuidle_stat_info {
	ktime_t last_entry_time;
	int cur_state;
	int state_count;

	struct cpuidle_stat_state_usage *usage;
};

extern void cpuidle_stat_start(int cpu, int state, int sub_state);
extern void cpuidle_stat_finish(int cpuid, int early_wakeup);
extern void cpuidle_stat_register(struct cpuidle_driver *drv);

#ifdef CONFIG_CPU_IDLE
extern void cpuidle_stat_collect_idle_ip(int mode,
				int index, unsigned int idle_ip);
extern void cpuidle_stat_collect_cpus_busy(void);
extern void cpuidle_stat_collect_cp_busy(void);
extern void cpuidle_stat_collect_jig_block(void);
#else
extern inline void cpuidle_stat_collect_idle_ip(int mode,
				int index, unsigned int idle_ip)
{
	/* Nothing to do */
}
extern void cpuidle_stat_collect_cpus_busy(void)
{
	/* Nothing to do */
}
extern void cpuidle_stat_collect_cp_busy(void)
{
	/* Nothing to do */
}

extern void cpuidle_stat_collect_jig_block(void)
{
	/* Nothing to do */
}
#endif

#ifdef CONFIG_ENERGY_MONITOR
 struct emon_cpuidle_info {
	ktime_t total_idle_time;
	struct cpuidle_stat_state_usage usage[2]; /* TODO: Currently we only support 2 states */
 };

 struct emon_lpm_info {
	 struct cpuidle_stat_state_usage usage; /* TODO: Currently we only support sicd */

	 /* blocker
	 u64 idle_ip_pending[NUM_SYS_POWERDOWN][NUM_IDLE_IP][IDLE_IP_REG_SIZE];
	 u64 cpus_busy_stat;
	 u64 cp_busy_stat; */
 };

 struct emon_cpuidle_stat {
 	int cpu_count;
	int state_count;
	ktime_t total_stat_time;
	struct emon_cpuidle_info cpuidle[2]; /* TODO: Currently we only support 2 cpus */
	struct emon_lpm_info lpm;
 };

struct emon_lpm_blocker {
	char *name;
	u64 count;
};

int cpuidle_stats_get_stats(int type, struct emon_cpuidle_stat *cpuidle_stat);
void cpuidle_stats_get_blocker(int type,
		struct emon_lpm_blocker *blocker_array,  size_t n);

#endif

#endif /* CPUIDLE_STAT_H */
