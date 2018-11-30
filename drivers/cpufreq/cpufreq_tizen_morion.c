/*
 *  drivers/cpufreq/cpufreq_tizen_morion.c
 *
 *  Copyright (C)  2001 Russell King
 *            (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *                      Jun Nakajima <jun.nakajima@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/percpu-defs.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/trm.h>
#include <linux/cpuidle.h>
#if defined(CONFIG_SYSTEM_LOAD_ANALYZER)
#include <linux/load_analyzer.h>
#endif

#include "cpufreq_tizen_morion.h"

/*
 * dbs is used in this file as a shortform for demandbased switching
 * It helps to keep variable names smaller, simpler
 */

#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MAX_SAMPLING_DOWN_FACTOR		(100000)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)
#define MIN_FREQUENCY_UP_THRESHOLD		(11)
#define MAX_FREQUENCY_UP_THRESHOLD		(100)

/*
 * The polling frequency of this governor depends on the capability of
 * the processor. Default polling frequency is 1000 times the transition
 * latency of the processor. The governor will work on any processor with
 * transition latency <= 10mS, using appropriate sampling
 * rate.
 * For CPUs with transition latency > 10mS (mostly drivers with CPUFREQ_ETERNAL)
 * this governor will not work.
 * All times here are in uS.
 */
#define MIN_SAMPLING_RATE_RATIO			(2)

static struct morion_ops morion_ops;
static unsigned int default_powersave_bias;
static int needed_cpu_online(unsigned int cpu_load[], unsigned int cpu_freq_mhz);

/* Cpu hotplug work struct */
struct cpu_hotplug_work_s {
	struct workqueue_struct *tizen_morion_cpu_hotplug_work;
	struct work_struct sim_hotplug_work;
};

/* Cpu boost work struct */
struct booting_cpu_boost_s {
	struct workqueue_struct *booting_cpu_boost_wq;
	struct delayed_work booting_cpu_boost_work;
	struct pm_qos_request pm_qos_cpu_online_min_req;
	struct pm_qos_request pm_qos_min_cpu;
};

#define CPU_BOOTING_TIME	40
#define PM_QOS_BOOTING_CPU_BOOST_FREQ	1144000
#define PM_QOS_BOOTING_CPU_BOOST_CORE	2

static struct booting_cpu_boost_s booting_cpu_boost;
static struct cpu_hotplug_work_s sim_cpu_hotplug;
int hotplug_work_enabled = 0;

static void booting_cpu_boost_completed(struct work_struct *work)
{
	pr_info("%s: +++", __func__);

	if (pm_qos_request_active(&booting_cpu_boost.pm_qos_cpu_online_min_req))
		pm_qos_remove_request(&booting_cpu_boost.pm_qos_cpu_online_min_req);

	if (pm_qos_request_active(&booting_cpu_boost.pm_qos_min_cpu))
		pm_qos_remove_request(&booting_cpu_boost.pm_qos_min_cpu);

#if defined(CONFIG_SLP_BUSY_LEVEL)
	cpu_busy_level_enable = 1;
#endif

	pr_info("%s: ---", __func__);
}

/*
 * Not all CPUs want IO time to be accounted as busy; this depends on how
 * efficient idling at a higher frequency/voltage is.
 * Pavel Machek says this is not so for various generations of AMD and old
 * Intel systems.
 * Mike Chan (android.com) claims this is also not true for ARM.
 * Because of this, whitelist specific known (series) of CPUs by default, and
 * leave all others up to the user.
 */
static int should_io_be_busy(void)
{
#if defined(CONFIG_X86)
	/*
	 * For Intel, Core 2 (model 15) and later have an efficient idle.
	 */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_INTEL &&
			boot_cpu_data.x86 == 6 &&
			boot_cpu_data.x86_model >= 15)
		return 1;
#endif
	return 0;
}

/*
 * Find right freq to be set now with powersave_bias on.
 * Returns the freq_hi to be used right now and will set freq_hi_delay_us,
 * freq_lo, and freq_lo_delay_us in percpu area for averaging freqs.
 */
static unsigned int generic_powersave_bias_target(struct cpufreq_policy *policy,
		unsigned int freq_next, unsigned int relation)
{
	unsigned int freq_req, freq_reduc, freq_avg;
	unsigned int freq_hi, freq_lo;
	unsigned int index;
	unsigned int delay_hi_us;
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct morion_policy_dbs_info *dbs_info = to_dbs_info(policy_dbs);
	struct dbs_data *dbs_data = policy_dbs->dbs_data;
	struct morion_dbs_tuners *morion_tuners = dbs_data->tuners;
	struct cpufreq_frequency_table *freq_table = policy->freq_table;

	if (!freq_table) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_delay_us = 0;
		return freq_next;
	}

	index = cpufreq_frequency_table_target(policy, freq_next, relation);
	freq_req = freq_table[index].frequency;
	freq_reduc = freq_req * morion_tuners->powersave_bias / 1000;
	freq_avg = freq_req - freq_reduc;

	/* Find freq bounds for freq_avg in freq_table */
	index = cpufreq_table_find_index_h(policy, freq_avg);
	freq_lo = freq_table[index].frequency;
	index = cpufreq_table_find_index_l(policy, freq_avg);
	freq_hi = freq_table[index].frequency;

	/* Find out how long we have to be in hi and lo freqs */
	if (freq_hi == freq_lo) {
		dbs_info->freq_lo = 0;
		dbs_info->freq_lo_delay_us = 0;
		return freq_lo;
	}
	delay_hi_us = (freq_avg - freq_lo) * dbs_data->sampling_rate;
	delay_hi_us += (freq_hi - freq_lo) / 2;
	delay_hi_us /= freq_hi - freq_lo;
	dbs_info->freq_hi_delay_us = delay_hi_us;
	dbs_info->freq_lo = freq_lo;
	dbs_info->freq_lo_delay_us = dbs_data->sampling_rate - delay_hi_us;
	return freq_hi;
}

static void morion_powersave_bias_init(struct cpufreq_policy *policy)
{
	struct morion_policy_dbs_info *dbs_info = to_dbs_info(policy->governor_data);

	dbs_info->freq_lo = 0;
}

static void dbs_freq_increase(struct cpufreq_policy *policy, unsigned int freq)
{
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct dbs_data *dbs_data = policy_dbs->dbs_data;
	struct morion_dbs_tuners *morion_tuners = dbs_data->tuners;

	if (morion_tuners->powersave_bias)
		freq = morion_ops.powersave_bias_target(policy, freq,
				CPUFREQ_RELATION_H);
	else if (policy->cur == policy->max)
		return;

	__cpufreq_driver_target(policy, freq, morion_tuners->powersave_bias ?
			CPUFREQ_RELATION_L : CPUFREQ_RELATION_H);
}

unsigned int SLP_DVFS_UP_OPT_LEVEL[] = {
	598000,
	676000,
	754000,
	839000,
	904000,
	1014000,
	1099000,
	1144000,
};

unsigned int SLP_DVFS_DOWN_OPT_LEVEL[] = {
	449000,
	598000,
	676000,
	754000,
	839000,
	904000,
	1014000,
	1099000,
	1144000,
};

enum {
	FIND_OPT_FREQ,
	INC_FREQ,
};

unsigned int slp_dvfs_get_opt_freq(unsigned int mode, unsigned int org_freq)
{
	int i;
	unsigned int opt_freq = org_freq;

	switch (mode) {
		case FIND_OPT_FREQ:
			for (i = 0; i < ARRAY_SIZE(SLP_DVFS_DOWN_OPT_LEVEL); i++) {
				if (org_freq <= SLP_DVFS_DOWN_OPT_LEVEL[i]) {
					opt_freq = SLP_DVFS_DOWN_OPT_LEVEL[i];
					break;
				}
			}
			break;

		case INC_FREQ:
			for (i = 0; i < ARRAY_SIZE(SLP_DVFS_UP_OPT_LEVEL); i++) {
				if (org_freq < SLP_DVFS_UP_OPT_LEVEL[i]) {
					opt_freq = SLP_DVFS_UP_OPT_LEVEL[i];
					break;
				}
			}
			break;
	}

	return opt_freq;
}

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu, u64 *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static int get_index(int cnt, int ring_size, int diff)
{
	int ret = 0, modified_diff;

	if ((diff > ring_size) || (diff * (-1) > ring_size))
		modified_diff = diff % ring_size;
	else
		modified_diff = diff;

	ret = (ring_size + cnt + modified_diff) % ring_size;

	return ret;
}

#define CPU_RUNNING_TASK_NUM 25
static unsigned int cpu_run_task[CPU_RUNNING_TASK_NUM];
static unsigned int cpu_run_task_cnt;

static void store_run_task(unsigned int run_task_num)
{
	unsigned int cnt;

	if (++cpu_run_task_cnt >= CPU_RUNNING_TASK_NUM)
		cpu_run_task_cnt = 0;

	cnt = cpu_run_task_cnt;

	cpu_run_task[cnt] = run_task_num;
}

static int get_run_task(unsigned int sample_num)
{
	unsigned int loop = 0, cnt = 0, running_task = 0;
	unsigned int  sum_of_running_task = 0;

	cnt = cpu_run_task_cnt;

	for (loop = 0; loop < sample_num ; loop++) {
		sum_of_running_task += cpu_run_task[cnt];
		cnt = get_index(cnt, CPU_RUNNING_TASK_NUM, -1);
	}

	if (sample_num > 0)
		running_task = sum_of_running_task / sample_num;

	return running_task;
}

#define CPU_AVG_FREQ_NUM 25

struct cpu_load_freq_time_tag {
	unsigned int cpu_load_freq;
	unsigned int time;
};

struct cpu_load_freq_time_tag cpu_load_freq_time[CPU_AVG_FREQ_NUM];

static unsigned int cpu_avg_freq_cnt;
static void store_avg_load_freq(unsigned int cpu_load_freq, unsigned int ms_time)
{
	unsigned int cnt;

	if (++cpu_avg_freq_cnt >= CPU_AVG_FREQ_NUM)
		cpu_avg_freq_cnt = 0;
	cnt = cpu_avg_freq_cnt;

	cpu_load_freq_time[cnt].cpu_load_freq = cpu_load_freq;
	cpu_load_freq_time[cnt].time = ms_time;
}

static int get_sample_num_from_time(unsigned int time)
{
	unsigned int loop = 0, cnt = 0;
	unsigned int sum_of_time = 0;

	cnt = cpu_avg_freq_cnt;

	for (loop = 0; (sum_of_time < time) && (loop < CPU_AVG_FREQ_NUM) ; loop++) {
		sum_of_time += cpu_load_freq_time[cnt].time;
		cnt = get_index(cnt, CPU_AVG_FREQ_NUM, -1);
	}

	return loop;
}

static int get_avg_load_freq(unsigned int ms_time)
{
	unsigned int sample_num;
	unsigned int loop = 0, cnt = 0, avg_load_freq = 0;
	unsigned long sum_of_load_freq = 0, sum_of_time = 0;

	cnt = cpu_avg_freq_cnt;

	sample_num = get_sample_num_from_time(ms_time);

	for (loop = 0; loop < sample_num ; loop++) {
		sum_of_load_freq += (cpu_load_freq_time[cnt].cpu_load_freq * cpu_load_freq_time[cnt].time);
		sum_of_time += cpu_load_freq_time[cnt].time;

		cnt = get_index(cnt, CPU_AVG_FREQ_NUM, -1);
	}

	if (sum_of_time > 0)
		avg_load_freq = (int) sum_of_load_freq / sum_of_time;

	return avg_load_freq;
}

extern unsigned int not_w_aftr_cause;
static unsigned int in_real_idle;

static unsigned int need_cpu_online_num;
int needed_cpu_online(unsigned int cpu_load[], unsigned int cpu_freq_mhz)
{
	static unsigned int num_online_cpu;
	static unsigned int num_running_task;
	unsigned int max_cpu_freq = 1144;  // maximum cpu freq=1144Mhz
	unsigned int delta_ms_time;

	unsigned int cpu_load_freq;
	unsigned long long current_t;
	static unsigned long long pre_t;

	current_t = cpu_clock(UINT_MAX);
	if (pre_t == 0)
		pre_t = current_t;

	delta_ms_time = (unsigned int) (div_u64((current_t -pre_t) , 1000000));

	pre_t = current_t;

	cpu_load_freq = ((cpu_load[0] + cpu_load[1]) * (cpu_freq_mhz));

	if (delta_ms_time)
		store_avg_load_freq(cpu_load_freq, delta_ms_time);

	num_running_task = nr_running();
	store_run_task(num_running_task * 100);
#if defined(CONFIG_SYSTEM_LOAD_ANALYZER)
	store_external_load_factor(NR_RUNNING_TASK, num_running_task * 100);
#endif

	num_online_cpu = num_online_cpus();

	need_cpu_online_num = num_online_cpu;

	if ((get_avg_load_freq(5000) < (max_cpu_freq * 8))) {
		in_real_idle = 1;
	} else {
		in_real_idle = 0;
	}

	switch (num_online_cpu) {
		case 1:
			if ((cpu_freq_mhz >= 676) && (cpu_load[0] >= 50) &&
					(get_run_task(5) >= 150)) {
				need_cpu_online_num = 2;
			} else if ((cpu_load[0] == 100) && (get_run_task(1) >= 1000)) {
				need_cpu_online_num = 2;
			}

			break;

		case 2:
			if ((get_run_task(1) <= 200) && (cpu_load[0] < 100) &&
					(get_avg_load_freq(1000) < (max_cpu_freq * 5))) {
				need_cpu_online_num = 1;
			}

			break;

	}

	return need_cpu_online_num;
}

#if defined(HARD_KEY_WAKEUP_BOOSTER)
extern int hard_key_wakeup_boosting;
#endif

static void cpu_sim_hotplug_work(struct work_struct *work)
{

#if defined(HARD_KEY_WAKEUP_BOOSTER)
	if (hard_key_wakeup_boosting)
		return;
#endif

	if ((need_cpu_online_num == 1) && (cpu_online(1)  == 1)){
		cpu_down(1);
	} else if ((need_cpu_online_num == 2) && (cpu_online(1)  == 0)){
		cpu_up(1);
	}
}

/*
 * Every sampling_rate, we check, if current idle time is less than 20%
 * (default), then we try to increase frequency. Else, we adjust the frequency
 * proportional to load.
 */
static void morion_update(struct cpufreq_policy *policy)
{
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct dbs_data *dbs_data = policy_dbs->dbs_data;
	struct morion_dbs_tuners *morion_tuners = dbs_data->tuners;
	unsigned int max_load_freq;
	unsigned int j;
	unsigned int cpu_load[NR_CPUS];
	unsigned int target_cpu_online;
	unsigned int down_differential = DEF_FREQUENCY_DOWN_DIFFERENTIAL;

	/*
	 * Every sampling_rate, we check, if current idle time is less
	 * than 20% (default), then we try to increase frequency
	 * Every sampling_rate, we look for a the lowest
	 * frequency which can sustain the load while keeping idle time over
	 * 30%. If such a frequency exist, we try to decrease to this frequency.
	 *
	 * Any frequency increase takes it to the maximum frequency.
	 * Frequency reduction happens at minimum steps of
	 * 5% (default) of current frequency
	 */
#ifdef CONFIG_SYSTEM_LOAD_ANALYZER
	for (j = 0; j < CPU_NUM; j++)
		cpu_load[j] = 0;
#endif
	/* Get Absolute Load - in terms of freq */
	max_load_freq = 0;

#if defined(BUILD_ERROR)
	dbs_data->up_threshold = cpu_freq_get_threshold();
#endif

	for_each_cpu(j, policy->cpus) {
		struct cpu_dbs_info *j_dbs_info;
		u64 cur_wall_time, cur_idle_time, cur_iowait_time;
		unsigned int idle_time, wall_time, iowait_time;
		unsigned int load, load_freq;
		int freq_avg;

		j_dbs_info = &per_cpu(cpu_dbs, j);

		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time, 0);
		cur_iowait_time = get_cpu_iowait_time(j, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - j_dbs_info->prev_update_time);
		j_dbs_info->prev_update_time = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - j_dbs_info->prev_cpu_idle);
		j_dbs_info->prev_cpu_idle = cur_idle_time;

		iowait_time = (unsigned int)
			(cur_iowait_time - j_dbs_info->prev_cpu_iowait);
		j_dbs_info->prev_cpu_iowait = cur_iowait_time;

		if (dbs_data->ignore_nice_load) {
			u64 cur_nice;
			unsigned long cur_nice_jiffies;

			cur_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE] -
					 j_dbs_info->prev_cpu_nice;
			/*
			 * Assumption: nice time between sampling periods will
			 * be less than 2^32 jiffies for 32 bit sys
			 */
			cur_nice_jiffies = (unsigned long)
					cputime64_to_jiffies64(cur_nice);

			j_dbs_info->prev_cpu_nice = kcpustat_cpu(j).cpustat[CPUTIME_NICE];
			idle_time += jiffies_to_usecs(cur_nice_jiffies);
		}

		/*
		 * For the purpose of ondemand, waiting for disk IO is an
		 * indication that you're performance critical, and not that
		 * the system is actually idle. So subtract the iowait time
		 * from the cpu idle time.
		 */

		if (dbs_data->io_is_busy && idle_time >= iowait_time)
			idle_time -= iowait_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		load = 100 * (wall_time - idle_time) / wall_time;

		freq_avg = policy->cur;

		load_freq = load * freq_avg;
		if (load_freq > max_load_freq)
			max_load_freq = load_freq;
#ifdef CONFIG_SYSTEM_LOAD_ANALYZER
		cpu_load[j] = load;
#endif
	}

#ifdef CONFIG_SYSTEM_LOAD_ANALYZER
	store_cpu_load(cpu_load);
#endif

	target_cpu_online = needed_cpu_online(cpu_load, (policy->cur)/1000);
	/* queue hotplug_work to change cpu_online num */
	if (!smp_processor_id() && target_cpu_online != num_online_cpus()) {
		if (hotplug_work_enabled) {
			if (!work_busy(&sim_cpu_hotplug.sim_hotplug_work)) {
				queue_work_on(0, sim_cpu_hotplug.tizen_morion_cpu_hotplug_work,
					&sim_cpu_hotplug.sim_hotplug_work);
			}
		}
	}

	/* Check for frequency increase */
	if (max_load_freq > dbs_data->up_threshold * policy->cur) {
		/* If switching to max speed, apply sampling_down_factor */
		if (policy->cur < policy->max)
			policy_dbs->rate_mult =
				dbs_data->sampling_down_factor;

#if defined(BUILD_ERROR)
		if (cpu_gov_get_up_level() > 0) {
			int target_freq;

			if (policy->cur == 100000) {
				target_freq = 200000;
			}

			dbs_freq_increase(policy, target_freq);

		} else
#endif
		{
			unsigned int opt_freq;
			opt_freq = slp_dvfs_get_opt_freq(INC_FREQ, policy->cur);
			dbs_freq_increase(policy, opt_freq);
		}

		return;
	}

	/* Check for frequency decrease */
	/* if we cannot reduce the frequency anymore, break out early */
	if (policy->cur == policy->min)
		return;

	/*
	 * The optimal frequency is the frequency that is the lowest that
	 * can support the current CPU usage without triggering the up
	 * policy. To be safe, we focus 10 points under the threshold.
	 */
	if (max_load_freq <
			(dbs_data->up_threshold  - down_differential) *
			policy->cur) {
		unsigned int freq_next;
		freq_next = max_load_freq /
				(dbs_data->up_threshold - down_differential);

		/* if cpu is not idle enough or 2 cpu core is online */
		/* cpufreq must keep minimum freq=598000Hz */
		if (((target_cpu_online == 2) && (freq_next < 598000) ) ||
			((target_cpu_online == 1) && (freq_next < 598000) && (!in_real_idle))) {
			freq_next = 598000;
		}

		/* No longer fully busy, reset rate_mult */
		policy_dbs->rate_mult = 1;

		if (freq_next < policy->min)
			freq_next = policy->min;

		if (!morion_tuners->powersave_bias) {
				__cpufreq_driver_target(policy, freq_next,
						CPUFREQ_RELATION_L);
		} else {
			int freq = morion_ops.powersave_bias_target(policy, freq_next,
					CPUFREQ_RELATION_L);
			__cpufreq_driver_target(policy, freq,
				CPUFREQ_RELATION_L);
		}
	}
}

static unsigned int morion_dbs_timer(struct cpufreq_policy *policy)
{
	struct policy_dbs_info *policy_dbs = policy->governor_data;
	struct dbs_data *dbs_data = policy_dbs->dbs_data;
	struct morion_policy_dbs_info *dbs_info = to_dbs_info(policy_dbs);
	int sample_type = dbs_info->sample_type;

	/* Common NORMAL_SAMPLE setup */
	dbs_info->sample_type = OD_NORMAL_SAMPLE;
	/*
	 * OD_SUB_SAMPLE doesn't make sense if sample_delay_ns is 0, so ignore
	 * it then.
	 */
	if (sample_type == OD_SUB_SAMPLE && policy_dbs->sample_delay_ns > 0) {
		__cpufreq_driver_target(policy, dbs_info->freq_lo,
					CPUFREQ_RELATION_H);
		return dbs_info->freq_lo_delay_us;
	}

	morion_update(policy);

	if (dbs_info->freq_lo) {
		/* Setup timer for SUB_SAMPLE */
		dbs_info->sample_type = OD_SUB_SAMPLE;
		return dbs_info->freq_hi_delay_us;
	}

	return dbs_data->sampling_rate * policy_dbs->rate_mult;
}

/************************** sysfs interface ************************/
static struct dbs_governor morion_dbs_gov;

static ssize_t store_io_is_busy(struct gov_attr_set *attr_set, const char *buf,
				size_t count)
{
	struct dbs_data *dbs_data = to_dbs_data(attr_set);
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;
	dbs_data->io_is_busy = !!input;

	/* we need to re-evaluate prev_cpu_idle */
	gov_update_cpu_data(dbs_data);

	return count;
}

static ssize_t store_up_threshold(struct gov_attr_set *attr_set,
				  const char *buf, size_t count)
{
	struct dbs_data *dbs_data = to_dbs_data(attr_set);
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_FREQUENCY_UP_THRESHOLD ||
			input < MIN_FREQUENCY_UP_THRESHOLD) {
		return -EINVAL;
	}

	dbs_data->up_threshold = input;
	return count;
}

static ssize_t store_sampling_down_factor(struct gov_attr_set *attr_set,
					  const char *buf, size_t count)
{
	struct dbs_data *dbs_data = to_dbs_data(attr_set);
	struct policy_dbs_info *policy_dbs;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1 || input > MAX_SAMPLING_DOWN_FACTOR || input < 1)
		return -EINVAL;

	dbs_data->sampling_down_factor = input;

	/* Reset down sampling multiplier in case it was active */
	list_for_each_entry(policy_dbs, &attr_set->policy_list, list) {
		/*
		 * Doing this without locking might lead to using different
		 * rate_mult values in morion_update() and morion_dbs_timer().
		 */
		mutex_lock(&policy_dbs->timer_mutex);
		policy_dbs->rate_mult = 1;
		mutex_unlock(&policy_dbs->timer_mutex);
	}

	return count;
}

static ssize_t store_ignore_nice_load(struct gov_attr_set *attr_set,
				      const char *buf, size_t count)
{
	struct dbs_data *dbs_data = to_dbs_data(attr_set);
	unsigned int input;
	int ret;

	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	if (input > 1)
		input = 1;

	if (input == dbs_data->ignore_nice_load) { /* nothing to do */
		return count;
	}
	dbs_data->ignore_nice_load = input;

	/* we need to re-evaluate prev_cpu_idle */
	gov_update_cpu_data(dbs_data);

	return count;
}

static ssize_t store_powersave_bias(struct gov_attr_set *attr_set,
				    const char *buf, size_t count)
{
	struct dbs_data *dbs_data = to_dbs_data(attr_set);
	struct morion_dbs_tuners *morion_tuners = dbs_data->tuners;
	struct policy_dbs_info *policy_dbs;
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);

	if (ret != 1)
		return -EINVAL;

	if (input > 1000)
		input = 1000;

	morion_tuners->powersave_bias = input;

	list_for_each_entry(policy_dbs, &attr_set->policy_list, list)
		morion_powersave_bias_init(policy_dbs->policy);

	return count;
}

gov_show_one_common(sampling_rate);
gov_show_one_common(up_threshold);
gov_show_one_common(sampling_down_factor);
gov_show_one_common(ignore_nice_load);
gov_show_one_common(min_sampling_rate);
gov_show_one_common(io_is_busy);
gov_show_one(morion, powersave_bias);

gov_attr_rw(sampling_rate);
gov_attr_rw(io_is_busy);
gov_attr_rw(up_threshold);
gov_attr_rw(sampling_down_factor);
gov_attr_rw(ignore_nice_load);
gov_attr_rw(powersave_bias);
gov_attr_ro(min_sampling_rate);

static struct attribute *morion_attributes[] = {
	&min_sampling_rate.attr,
	&sampling_rate.attr,
	&up_threshold.attr,
	&sampling_down_factor.attr,
	&ignore_nice_load.attr,
	&powersave_bias.attr,
	&io_is_busy.attr,
	NULL
};

/************************** sysfs end ************************/

static struct policy_dbs_info *morion_alloc(void)
{
	struct morion_policy_dbs_info *dbs_info;

	dbs_info = kzalloc(sizeof(*dbs_info), GFP_KERNEL);
	return dbs_info ? &dbs_info->policy_dbs : NULL;
}

static void morion_free(struct policy_dbs_info *policy_dbs)
{
	kfree(to_dbs_info(policy_dbs));
}

static int morion_init(struct dbs_data *dbs_data)
{
	struct morion_dbs_tuners *tuners;
	u64 idle_time;
	int cpu;

	tuners = kzalloc(sizeof(*tuners), GFP_KERNEL);
	if (!tuners)
		return -ENOMEM;

	cpu = get_cpu();
	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		dbs_data->up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		dbs_data->min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		dbs_data->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;

		/* For correct statistics, we need 10 ticks for each measure */
		dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO *
			jiffies_to_usecs(10);
	}

	dbs_data->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	dbs_data->ignore_nice_load = 0;
	tuners->powersave_bias = default_powersave_bias;
	dbs_data->io_is_busy = should_io_be_busy();

	dbs_data->tuners = tuners;
	return 0;
}

static void morion_exit(struct dbs_data *dbs_data)
{
	kfree(dbs_data->tuners);
}

static void morion_start(struct cpufreq_policy *policy)
{
	struct morion_policy_dbs_info *dbs_info = to_dbs_info(policy->governor_data);

	dbs_info->sample_type = OD_NORMAL_SAMPLE;
	morion_powersave_bias_init(policy);
}

static struct morion_ops morion_ops = {
	.powersave_bias_target = generic_powersave_bias_target,
};

static struct dbs_governor morion_dbs_gov = {
	.gov = CPUFREQ_DBS_GOVERNOR_INITIALIZER("tizen_morion"),
	.kobj_type = { .default_attrs = morion_attributes },
	.gov_dbs_timer = morion_dbs_timer,
	.alloc = morion_alloc,
	.free = morion_free,
	.init = morion_init,
	.exit = morion_exit,
	.start = morion_start,
};

#define CPU_FREQ_GOV_MORION	(&morion_dbs_gov.gov)

static void morion_set_powersave_bias(unsigned int powersave_bias)
{
	unsigned int cpu;
	cpumask_t done;

	default_powersave_bias = powersave_bias;
	cpumask_clear(&done);

	get_online_cpus();
	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct policy_dbs_info *policy_dbs;
		struct dbs_data *dbs_data;
		struct morion_dbs_tuners *morion_tuners;

		if (cpumask_test_cpu(cpu, &done))
			continue;

		policy = cpufreq_cpu_get_raw(cpu);
		if (!policy || policy->governor != CPU_FREQ_GOV_MORION)
			continue;

		policy_dbs = policy->governor_data;
		if (!policy_dbs)
			continue;

		cpumask_or(&done, &done, policy->cpus);

		dbs_data = policy_dbs->dbs_data;
		morion_tuners = dbs_data->tuners;
		morion_tuners->powersave_bias = default_powersave_bias;
	}
	put_online_cpus();
}

void morion_register_powersave_bias_handler(unsigned int (*f)
		(struct cpufreq_policy *, unsigned int, unsigned int),
		unsigned int powersave_bias)
{
	morion_ops.powersave_bias_target = f;
	morion_set_powersave_bias(powersave_bias);
}
EXPORT_SYMBOL_GPL(morion_register_powersave_bias_handler);

void morion_unregister_powersave_bias_handler(void)
{
	morion_ops.powersave_bias_target = generic_powersave_bias_target;
	morion_set_powersave_bias(0);
}
EXPORT_SYMBOL_GPL(morion_unregister_powersave_bias_handler);

static int __init cpufreq_gov_dbs_init(void)
{
	/* init cpu hotplug workqueue */
	sim_cpu_hotplug.tizen_morion_cpu_hotplug_work = create_workqueue("cpu_hotplug");

	if (!sim_cpu_hotplug.tizen_morion_cpu_hotplug_work) {
		pr_err("%s cannot create tizen_morion_cpu_hotplug_work\n", __func__);
		goto err_hotplug_wq;
	}

	if (!hotplug_work_enabled) {
		INIT_WORK(&sim_cpu_hotplug.sim_hotplug_work, cpu_sim_hotplug_work);
		hotplug_work_enabled = 1;
	}

	/* init cpu booting cpu boost workqueue */
	booting_cpu_boost.booting_cpu_boost_wq = alloc_workqueue("booting_cpu_boost_work",
												WQ_HIGHPRI, 0);

	if (!booting_cpu_boost.booting_cpu_boost_wq) {
		pr_err("%s cannot create booting_cpu_boost_wq\n", __func__);
		goto err_boost_wq;
	}

	INIT_DELAYED_WORK(&booting_cpu_boost.booting_cpu_boost_work,
		booting_cpu_boost_completed);

	queue_delayed_work(booting_cpu_boost.booting_cpu_boost_wq,
		&booting_cpu_boost.booting_cpu_boost_work, CPU_BOOTING_TIME * HZ);

	/* pm_qos request booting min_cpu_online=2, min_cpu_freq=1099000 */
	/* (enable cpu max core and max freq for booting phase) */
	if (!pm_qos_request_active(&booting_cpu_boost.pm_qos_cpu_online_min_req))
		pm_qos_add_request(&booting_cpu_boost.pm_qos_cpu_online_min_req,
			PM_QOS_CPU_ONLINE_MIN, PM_QOS_BOOTING_CPU_BOOST_CORE);

	if (!pm_qos_request_active(&booting_cpu_boost.pm_qos_min_cpu))
		pm_qos_add_request(&booting_cpu_boost.pm_qos_min_cpu,
			PM_QOS_CLUSTER0_FREQ_MIN, PM_QOS_BOOTING_CPU_BOOST_FREQ);

	return cpufreq_register_governor(CPU_FREQ_GOV_MORION);

err_hotplug_wq:
	destroy_workqueue(sim_cpu_hotplug.tizen_morion_cpu_hotplug_work);
	return -1;
err_boost_wq:
	destroy_workqueue(booting_cpu_boost.booting_cpu_boost_wq);
	return -1;
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(CPU_FREQ_GOV_MORION);
	destroy_workqueue(sim_cpu_hotplug.tizen_morion_cpu_hotplug_work);
	destroy_workqueue(booting_cpu_boost.booting_cpu_boost_wq);
}

MODULE_AUTHOR("Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>");
MODULE_AUTHOR("Alexey Starikovskiy <alexey.y.starikovskiy@intel.com>");
MODULE_DESCRIPTION("'tizen_morion' - A dynamic cpufreq governor for "
	"Low Latency Frequency Transition capable processors");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_TIZEN_MORION
struct cpufreq_governor *cpufreq_default_governor(void)
{
	return CPU_FREQ_GOV_MORION;
}

fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
