/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/cpuidle_stat.h>
#include <linux/suspend.h>
#include <linux/ktime.h>

#include <asm/page.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#ifdef CONFIG_ENERGY_MONITOR
#include <linux/sort.h>
#include <linux/power/energy_monitor.h>
#endif

static bool cpuidle_stat_started;
static ktime_t cpuidle_stat_start_time;

/*
 * "idle_stat" contains profiling data for per cpu idle state which
 * declared in cpuidle driver.
 */
static DEFINE_PER_CPU(struct cpuidle_stat_info, idle_stat);

/*
 * "cpd_info" contains profiling data for CPD(Cluster Power Down) which
 * is subordinate to C2 state idle. Each cluster has one element in
 * cpd_stat[].
 */
static struct cpuidle_stat_info cpd_stat[MAX_CLUSTER];

/*
 * "lpm_stat" contains profiling data for LPM(Low Power Mode). LPM
 * comprises many system power mode such AFTR, ALPA.
 */
static struct cpuidle_stat_info lpm_stat;

/*
 * "idle_ip_pending" contains which blocks to enter system power mode.
 * It is used by only SICD/SICD_CPD and ALPA.
 */
static u64 idle_ip_pending[NUM_SYS_POWERDOWN][NUM_IDLE_IP][IDLE_IP_REG_SIZE];

/*
 * "idle_ip_list" contains IP name in IDLE_IP
 */
static char *idle_ip_list[NUM_IDLE_IP][IDLE_IP_REG_SIZE];

/*
 * "cpus_busy_stat" is the count that cpus blocks to enter system power mode.
 */
static u64 cpus_busy_stat;

/*
 * "cp_busy_stat" is the count that cp blocks to enter system power mode.
 */
static u64 cp_busy_stat;

/*
 * "jig_block_stat" is the count that jig blocks to enter system power mode.
 */
static u64 jig_block_stat;

#ifdef CONFIG_ENERGY_MONITOR
static ktime_t emon_start_time;
static DEFINE_PER_CPU(struct cpuidle_stat_info, emon_idle_stat);
static struct cpuidle_stat_info emon_lpm_stat;
static u64 emon_idle_ip_pending[NUM_SYS_POWERDOWN][NUM_IDLE_IP][IDLE_IP_REG_SIZE];

int cpuidle_stats_get_stats(int type, struct emon_cpuidle_stat *cpuidle_stat)
{
	struct cpuidle_stat_info *emon;
	struct cpuidle_stat_info *stat;
	int i, cpu;
	int state_count = per_cpu(idle_stat, 0).state_count;
	ktime_t now = ktime_get();

	if (!cpuidle_stat)
		return -EINVAL;

	cpuidle_stat->total_stat_time = ktime_sub(now, emon_start_time);
	if (type != ENERGY_MON_TYPE_DUMP)
		emon_start_time = now;

	/* TODO: Currently we only support 2 states */
	if (state_count > 2)
		state_count = 2;
	cpuidle_stat->state_count = state_count;

	for (i = 0; i < state_count; i++) {
		for_each_possible_cpu(cpu) {
			stat = &per_cpu(idle_stat, cpu);
			emon = &per_cpu(emon_idle_stat, cpu);

			/* TODO: Currently we only support 2 cpus */
			if (cpu < 2) {
				cpuidle_stat->cpuidle[cpu].usage[i].entry_count =
					stat->usage[i].entry_count - emon->usage[i].entry_count;
				cpuidle_stat->cpuidle[cpu].usage[i].early_wakeup_count =
					stat->usage[i].early_wakeup_count - emon->usage[i].early_wakeup_count;
				cpuidle_stat->cpuidle[cpu].usage[i].time =
					ktime_sub(stat->usage[i].time, emon->usage[i].time);
			}

			if (type != ENERGY_MON_TYPE_DUMP) {
				emon->usage[i].entry_count = stat->usage[i].entry_count;
				emon->usage[i].early_wakeup_count = stat->usage[i].early_wakeup_count;
				emon->usage[i].time = stat->usage[i].time;
			}
		}
	}

	cpuidle_stat->cpu_count = 0;
	for_each_possible_cpu(cpu) {
		ktime_t idle_sum = ktime_set(0, 0);

		/* TODO: Currently we only support 2 cpus */
		if (cpu < 2) {
			for (i = 0; i < state_count; i++)
				idle_sum = ktime_add(idle_sum, cpuidle_stat->cpuidle[cpu].usage[i].time);

			cpuidle_stat->cpuidle[cpu].total_idle_time = idle_sum;
			cpuidle_stat->cpu_count++;
		}
	}

	/* TODO: Currently we only support sicd */
	//for_each_syspower_mode(i) {
	for (i = 0; i < 1; i++) {
		stat = &lpm_stat;
		emon = &emon_lpm_stat;

		cpuidle_stat->lpm.usage.entry_count =
			stat->usage[i].entry_count - emon->usage[i].entry_count;
		cpuidle_stat->lpm.usage.early_wakeup_count =
			stat->usage[i].early_wakeup_count - emon->usage[i].early_wakeup_count;
		cpuidle_stat->lpm.usage.time =
			ktime_sub(stat->usage[i].time, emon->usage[i].time);

		if (type != ENERGY_MON_TYPE_DUMP) {
			emon->usage[i].entry_count = stat->usage[i].entry_count ;
			emon->usage[i].early_wakeup_count = stat->usage[i].early_wakeup_count;
			emon->usage[i].time = stat->usage[i].time;
		}
	}

	return 0;
}

static int cpuidle_stats_emon_cmp_func(const void *a, const void *b)
{
	struct emon_lpm_blocker *pa = (struct emon_lpm_blocker *)(a);
	struct emon_lpm_blocker *pb = (struct emon_lpm_blocker *)(b);
	return (pb->count - pa->count);
}

void cpuidle_stats_get_blocker(int type,
		struct emon_lpm_blocker *blocker_array,  size_t n)
{
	int i, idle_ip, bit;
	u64 emon_count;

	if (!blocker_array)
		return;

	for (i = 0; i < n; i++) {
		blocker_array[i].name = NULL;
		blocker_array[i].count = 0;
	}

	i = 0;
	/* TODO: Currently we only support sicd */
	//for_each_syspower_mode(i) {
	for_each_idle_ip(idle_ip) {
		for (bit = 0; bit < IDLE_IP_REG_SIZE; bit++) {
			emon_count = idle_ip_pending[0][idle_ip][bit] -emon_idle_ip_pending[0][idle_ip][bit];

			if (emon_count == 0)
				continue;

			if (i < n) {
				blocker_array[i].name = idle_ip_list[idle_ip][bit];
				blocker_array[i].count = emon_count;
				i++;
				if (i == n)
					sort(&blocker_array[0],
						n,
						sizeof(struct emon_lpm_blocker),
						cpuidle_stats_emon_cmp_func, NULL);
			} else {
				if (emon_count > blocker_array[n-1].count) {
					blocker_array[n-1].name = idle_ip_list[idle_ip][bit];
					blocker_array[n-1].count = emon_count;
					sort(&blocker_array[0],
						n,
						sizeof(struct emon_lpm_blocker),
						cpuidle_stats_emon_cmp_func, NULL);
				}
			}

			if (type != ENERGY_MON_TYPE_DUMP)
				emon_idle_ip_pending[0][idle_ip][bit] = idle_ip_pending[0][idle_ip][bit];
		}
	}

	if (i < n && i != 0)
		sort(&blocker_array[0],
			n,
			sizeof(struct emon_lpm_blocker),
			cpuidle_stats_emon_cmp_func, NULL);
}
#endif

/************************************************************************
 *                              Profiling                               *
 ************************************************************************/
/*
 * If cpu does not enter idle state, cur_state has -EINVAL. By this,
 * profiler can be aware of cpu state.
 */
#define state_entered(state)	(((int)state < (int)0) ? 0 : 1)

static void enter_idle_state(struct cpuidle_stat_info *info,
					int state, ktime_t now)
{
	if (state_entered(info->cur_state))
		return;

	info->cur_state = state;
	info->last_entry_time = now;

	info->usage[state].entry_count++;
}

static void exit_idle_state(struct cpuidle_stat_info *info,
					int state, ktime_t now,
					int earlywakeup)
{
	ktime_t diff;

	if (!state_entered(info->cur_state))
		return;

	info->cur_state = -EINVAL;

	if (earlywakeup) {
		/*
		 * If cpu cannot enter power mode, residency time
		 * should not be updated.
		 */
		info->usage[state].early_wakeup_count++;
		return;
	}

	diff = ktime_sub(now, info->last_entry_time);
	info->usage[state].time = ktime_add(info->usage[state].time, diff);
}

/*
 * C2 subordinate state such as CPD and SICD can be entered by many cpus.
 * The variables which contains these idle states need to keep
 * synchronization.
 */
static DEFINE_SPINLOCK(substate_lock);

void __cpuidle_stat_start(int cpu, int state, int substate)
{
	struct cpuidle_stat_info *info = &per_cpu(idle_stat, cpu);
	ktime_t now = ktime_get();

	/*
	 * Start to profile idle state. idle_stat is per-CPU variable,
	 * it does not need to synchronization.
	 */
	enter_idle_state(info, state, now);

	/* Start to profile subordinate idle state. */
	if (substate) {
		spin_lock(&substate_lock);

		/*
		 * SICD is a system power mode and also C2 subordinate
		 * state becuase it is entered by C2 entry cpu. On this
		 * count, in case of SICD or SICD_CPD, profiler updates
		 * lpm_stat although idle state is C2.
		 */
		if (state == CPU_IDLE_C2) {
			switch (substate) {
			case C2_CPD:
				info = &cpd_stat[to_cluster(cpu)];
				enter_idle_state( info, 0, now);
				break;
			case C2_SICD:
				info = &lpm_stat;
				enter_idle_state(info, LPM_SICD, now);
				break;
			}
		} else if (state == CPU_IDLE_LPM) {
			info = &lpm_stat;
			enter_idle_state(info, substate, now);
		}

		spin_unlock(&substate_lock);
	}
}

void cpuidle_stat_start(int cpu, int state, int substate)
{
	/*
	 * Return if profile is not started
	 */
	if (!cpuidle_stat_started)
		return;

	__cpuidle_stat_start(cpu, state, substate);
}

void __cpuidle_stat_finish(int cpu, int earlywakeup)
{
	struct cpuidle_stat_info *info = &per_cpu(idle_stat, cpu);
	int state = info->cur_state;
	ktime_t now = ktime_get();

	exit_idle_state(info, state, now, earlywakeup);

	spin_lock(&substate_lock);

	/*
	 * Subordinate state can be wakeup by many cpus. We cannot predict
	 * which cpu wakeup from idle state, profiler always try to update
	 * residency time of subordinate state. To avoid duplicate updating,
	 * exit_idle_state() checks validation.
	 */
	if (has_idle_sub_state(state)) {
		info = &cpd_stat[to_cluster(cpu)];
		exit_idle_state(info, info->cur_state, now, earlywakeup);

		info = &lpm_stat;
		exit_idle_state(info, info->cur_state, now, earlywakeup);
	}

	spin_unlock(&substate_lock);
}

void cpuidle_stat_finish(int cpu, int earlywakeup)
{
	/*
	 * Return if profile is not started
	 */
	if (!cpuidle_stat_started)
		return;

	__cpuidle_stat_finish(cpu, earlywakeup);
}

/*
 * Before system enters system power mode, it checks idle-ip status. Its
 * status is conveyed to cpuidle_stat_collect_idle_ip().
 */
void cpuidle_stat_collect_idle_ip(int mode, int index,
						unsigned int idle_ip)
{
	int i;

	/*
	 * Return if profile is not started
	 */
	if (!cpuidle_stat_started)
		return;

	for (i = 0; i < IDLE_IP_REG_SIZE; i++) {
		/*
		 * If bit of idle_ip has 1, IP corresponding to its bit
		 * is not idle.
		 */
		if (idle_ip & (1 << i)) {
			idle_ip_pending[mode][index][i]++;
		}
	}
}

void cpuidle_stat_collect_cpus_busy()
{
	cpus_busy_stat++;
}

void cpuidle_stat_collect_cp_busy()
{
	cp_busy_stat++;
}

void cpuidle_stat_collect_jig_block()
{
	jig_block_stat++;
}

/************************************************************************
 *                            Profile control                           *
 ************************************************************************/
static void clear_time(ktime_t *time)
{
	time->tv64 = 0;
}

static void clear_profile_info(struct cpuidle_stat_info *info)
{
	memset(info->usage, 0,
		sizeof(struct cpuidle_stat_state_usage) * info->state_count);

	clear_time(&info->last_entry_time);
	info->cur_state = -EINVAL;
}

static void reset_profile_record(void)
{
	int i;

	clear_time(&cpuidle_stat_start_time);

	for_each_possible_cpu(i)
		clear_profile_info(&per_cpu(idle_stat, i));

	for_each_cluster(i)
		clear_profile_info(&cpd_stat[i]);

	clear_profile_info(&lpm_stat);

	memset(idle_ip_pending, 0,
		NUM_SYS_POWERDOWN * NUM_IDLE_IP * IDLE_IP_REG_SIZE * sizeof(int));

	cpus_busy_stat = 0;
	cp_busy_stat = 0;
	jig_block_stat = 0;
}

static void call_cpu_start_profile(void *p) {};
static void call_cpu_finish_profile(void *p) {};

static void cpuidle_stat_main_start(void)
{
	if (cpuidle_stat_started) {
		pr_err("cpuidle profile is ongoing\n");
		return;
	}

	reset_profile_record();

	cpuidle_stat_start_time = ktime_get();
#ifdef CONFIG_ENERGY_MONITOR
	emon_start_time = cpuidle_stat_start_time;
#endif

	/* Wakeup all cpus and clear own profile data to start profile */
	preempt_disable();
	smp_call_function(call_cpu_start_profile, NULL, 1);
	preempt_enable();

	cpuidle_stat_started = 1;

	pr_debug("%s\n", __func__);
}

static void cpuidle_stat_main_stop(void)
{
	if (!cpuidle_stat_started) {
		pr_err("CPUIDLE profile does not start yet\n");
		return;
	}

	pr_debug("%s\n", __func__);

	/* Wakeup all cpus to update own profile data to finish profile */
	preempt_disable();
	smp_call_function(call_cpu_finish_profile, NULL, 1);
	preempt_enable();

	cpuidle_stat_started = 0;
}

/*********************************************************************
 *                          Sysfs interface                          *
 *********************************************************************/
#define get_sys_powerdown_str(mode)	sys_powerdown_str[mode]

static int calculate_percent(s64 residency, s64 profile_time)
{
	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile_time);

	return residency;
}

static ktime_t sum_idle_time(struct cpuidle_stat_info *info)
{
	int i;
	ktime_t idle_time =  ktime_set(0, 0);

	for (i = 0; i < info->state_count; i++)
		idle_time = ktime_add(idle_time, info->usage[i].time);

	return idle_time;
}

static ssize_t show_sysfs_result(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;
	int i, cpu, idle_ip, bit;
	struct cpuidle_stat_info *info;
	int state_count;
	s64 cpuidle_stat_total_time_ms;
	s64 temp_time_ms;
	ktime_t now = ktime_get();

	if (cpuidle_stat_started == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cpuidle_stat has not started yet\n");
		return ret;
	}

	cpuidle_stat_total_time_ms = ktime_to_ms(ktime_sub(now, cpuidle_stat_start_time));

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"Profiling Time : %llu ms\n", cpuidle_stat_total_time_ms);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[total idle ratio]\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#cpu      #time    #ratio\n");
	for_each_possible_cpu(cpu) {
		info = &per_cpu(idle_stat, cpu);
		temp_time_ms = ktime_to_ms(sum_idle_time(info));
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"cpu%d %10llums   %3u%%\n",
			cpu,
			temp_time_ms,
			calculate_percent(temp_time_ms, cpuidle_stat_total_time_ms));
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	/*
	 * All profile_info has same state_count. As a representative,
	 * cpu0's is used.
	 */
	state_count = per_cpu(idle_stat, 0).state_count;

	for (i = 0; i < state_count; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[state%d]\n", i);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"#cpu   #entry   #early      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			info = &per_cpu(idle_stat, cpu);
			temp_time_ms = ktime_to_ms(info->usage[i].time);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cpu%d   %5u   %5u   %10llums   %3u%%\n",
				cpu,
				info->usage[i].entry_count,
				info->usage[i].early_wakeup_count,
				temp_time_ms,
				calculate_percent(temp_time_ms, cpuidle_stat_total_time_ms));
		}
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[LPM] - Low Power Mode\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#mode        #entry   #early      #time     #ratio\n");
	//for_each_syspwr_mode(i) {
	for (i = 0; i < 1; i++) {
		temp_time_ms = ktime_to_ms(lpm_stat.usage[i].time);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"%-9s    %5u   %5u   %10lluums   %3u%%\n",
			get_sys_powerdown_str(i),
			lpm_stat.usage[i].entry_count,
			lpm_stat.usage[i].early_wakeup_count,
			temp_time_ms,
			calculate_percent(temp_time_ms, cpuidle_stat_total_time_ms));
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[LPM blockers]\n");
	//for_each_syspwr_mode(i) {
	for (i = 0; i < 1; i++) {
		for_each_idle_ip(idle_ip) {
			for (bit = 0; bit < IDLE_IP_REG_SIZE; bit++) {
				if (idle_ip_pending[i][idle_ip][bit])
					ret += snprintf(buf + ret, PAGE_SIZE -ret,
						"%s block by IDLE_IP%d[%d](%s, count = %lld)\n",
						get_sys_powerdown_str(i),
						idle_ip, bit, idle_ip_list[idle_ip][bit],
						idle_ip_pending[i][idle_ip][bit]);
			}
		}
		if (cpus_busy_stat)
			ret += snprintf(buf + ret, PAGE_SIZE -ret,
				"%s block by cpu, count = %lld)\n",
				get_sys_powerdown_str(i),
				cpus_busy_stat);
		if (cp_busy_stat)
			ret += snprintf(buf + ret, PAGE_SIZE -ret,
				"%s block by cp, count = %lld)\n",
				get_sys_powerdown_str(i),
				cp_busy_stat);
		if (jig_block_stat)
			ret += snprintf(buf + ret, PAGE_SIZE -ret,
				"%s block by jig, count = %lld)\n",
				get_sys_powerdown_str(i),
				jig_block_stat);
	}

	return ret;
}

static ssize_t show_cpuidle_stat(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;

	ret = show_sysfs_result(kobj, attr, buf);

	return ret;
}

static ssize_t store_cpuidle_stat(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	if (!!input) {
		cpuidle_stat_main_start();
	} else {
		cpuidle_stat_main_stop();
	}

	return count;
}

static ssize_t show_sicd_block_jig(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;
	int sicd_block_jig;

	sicd_block_jig = exynos_get_sicd_block_jig();

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"sicd_block_jig: %d\n", sicd_block_jig);

	return ret;
}

static ssize_t store_sicd_block_jig(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	if (!!input) {
		exynos_set_sicd_block_jig(true);
	} else {
		exynos_set_sicd_block_jig(false);
	}

	return count;
}

static struct kobj_attribute cpuidle_stat_attr =
	__ATTR(stat, 0644, show_cpuidle_stat, store_cpuidle_stat);

static struct kobj_attribute sicd_block_jig =
	__ATTR(sicd_block_jig, 0644, show_sicd_block_jig, store_sicd_block_jig);

static struct attribute *cpuidle_stat_attrs[] = {
	&cpuidle_stat_attr.attr,
	&sicd_block_jig.attr,
	NULL
};

static const struct attribute_group cpuidle_stat_group = {
	.attrs = cpuidle_stat_attrs,
};

/*********************************************************************
 *					 pm handler 				   *
 *********************************************************************/
/* print_idle_info print idle info to kernel log */
static void cpuidle_stat_print_idle_info(void)
{
	int i, idle_ip, bit, cpu;
	struct cpuidle_stat_info *info;
	int state_count;
	s64 cpuidle_stat_time_ms;
	s64 temp_time_ms;
	ktime_t now = ktime_get();

	if (cpuidle_stat_started == 0) {
		pr_info("cpuidle_stat has not started yet\n");
		return;
	}

	cpuidle_stat_time_ms = ktime_to_ms(ktime_sub(now, cpuidle_stat_start_time));

	pr_cont("[cis][id][%llu]",cpuidle_stat_time_ms);
	for_each_possible_cpu(cpu) {
		info = &per_cpu(idle_stat, cpu);
		temp_time_ms = ktime_to_ms(sum_idle_time(info));
		pr_cont("[c%d][%llu][%u]",
			cpu,
			temp_time_ms,
			calculate_percent(temp_time_ms, cpuidle_stat_time_ms));
	}
	pr_cont("\n");

	/*
	 * All idle_stats has same state_count. As a representative,
	 * cpu0's is used.
	 */
	state_count = per_cpu(idle_stat, 0).state_count;
	for (i = 0; i < state_count; i++) {
		pr_cont("[cis][cs%d]", i);
		for_each_possible_cpu(cpu) {
			info = &per_cpu(idle_stat, cpu);
			temp_time_ms = ktime_to_ms(info->usage[i].time);
			pr_cont("[c%d][%u][%u][%llu][%u]",
				cpu,
				info->usage[i].entry_count,
				info->usage[i].early_wakeup_count,
				temp_time_ms,
				calculate_percent(temp_time_ms, cpuidle_stat_time_ms));
		}
		pr_cont("\n");
	}

	//for_each_syspower_mode(i) {
	for (i = 0; i < 1; i++) {
		temp_time_ms = ktime_to_ms(lpm_stat.usage[i].time);
		pr_info("[cis][lp][%u][%u][%llu][%u]\n",
			lpm_stat.usage[i].entry_count,
			lpm_stat.usage[i].early_wakeup_count,
			temp_time_ms,
			calculate_percent(temp_time_ms, cpuidle_stat_time_ms));
	}

	//for_each_syspower_mode(i) {
	for (i = 0; i < 1; i++) {
		for_each_idle_ip(idle_ip) {
			for (bit = 0; bit < IDLE_IP_REG_SIZE; bit++) {
				if (idle_ip_pending[i][idle_ip][bit])
					pr_info("[cis][%s][%lld]\n",
						idle_ip_list[idle_ip][bit],
						idle_ip_pending[i][idle_ip][bit]);
			}
		}
		if (cpus_busy_stat)
			pr_info("[cis][cpus][%lld]\n", cpus_busy_stat);
		if (cp_busy_stat)
			pr_info("[cis][cp][%lld]\n", cp_busy_stat);
		if (jig_block_stat)
			pr_info("[cis][jig][%lld]\n", jig_block_stat);
	}
}

static int cpuidle_stat_pm_suspend_prepare_cb(void)
{
	cpuidle_stat_print_idle_info();

	return 0;
}

static int cpuidle_stat_pm_post_suspend_cb(void)
{
	return 0;
}

static int cpuidle_stat_pm_notifier(struct notifier_block *nb,
								unsigned long event, void *dummy)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		cpuidle_stat_pm_suspend_prepare_cb();
		break;
	case PM_POST_SUSPEND:
		cpuidle_stat_pm_post_suspend_cb();
		break;
	  default:
		break;
	}
	return 0;
}

static struct notifier_block cpuidle_stat_notifier_block = {
	.notifier_call = cpuidle_stat_pm_notifier,
};

/*********************************************************************
 *                   Initialize cpuidle profiler                     *
 *********************************************************************/
static void __init cpuidle_stat_info_init(struct cpuidle_stat_info *info,
						int state_count)
{
	int size = sizeof(struct cpuidle_stat_state_usage) * state_count;

	info->state_count = state_count;
	info->usage = kzalloc(size, GFP_KERNEL);
	if (!info->usage) {
		pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
		return;
	}
}

void __init cpuidle_stat_register(struct cpuidle_driver *drv)
{
	int idle_state_count = drv->state_count;
	int i;

	/* Initialize each cpuidle state information */
	for_each_possible_cpu(i) {
		cpuidle_stat_info_init(&per_cpu(idle_stat, i),
						idle_state_count);
#ifdef CONFIG_ENERGY_MONITOR
		cpuidle_stat_info_init(&per_cpu(emon_idle_stat, i),
						idle_state_count);
#endif
	}

	/* Initiailize CPD(Cluster Power Down) information */
	for_each_cluster(i) {
		cpuidle_stat_info_init(&cpd_stat[i], 1);
	}

	/* Initiailize LPM(Low Power Mode) information */
	cpuidle_stat_info_init(&lpm_stat, NUM_SYS_POWERDOWN);
#ifdef CONFIG_ENERGY_MONITOR
	cpuidle_stat_info_init(&emon_lpm_stat, NUM_SYS_POWERDOWN);
#endif

	cpuidle_stat_main_start();
}

static int __init cpuidle_stat_init(void)
{
	struct class *class;
	struct device *dev;

	class = class_create(THIS_MODULE, "cpuidle_stat");
	dev = device_create(class, NULL, 0, NULL, "cpuidle_stat_exynos");

	if (sysfs_create_group(&dev->kobj, &cpuidle_stat_group)) {
		pr_err("CPUIDLE Profiler : error to create sysfs\n");
		return -EINVAL;
	}

	exynos_get_idle_ip_list(idle_ip_list);

	register_pm_notifier(&cpuidle_stat_notifier_block);

	return 0;
}

static void __exit cpuidle_stat_exit(void)
{
	unregister_pm_notifier(&cpuidle_stat_notifier_block);
}

late_initcall(cpuidle_stat_init);
module_exit(cpuidle_stat_exit);
