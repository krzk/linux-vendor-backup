/*
 *  drivers/cpufreq/cpufreq_lab.c
 *
 *  LAB(Legacy Application Boost) cpufreq governor
 *
 *  Copyright (C) SAMSUNG Electronics. CO.
 *		Jonghwa Lee <jonghw3.lee@samusng.com>
 *		Lukasz Majewski <l.majewski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu-defs.h>
#include <linux/sysfs.h>
#include <linux/tick.h>
#include <linux/types.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>

#include "cpufreq_governor.h"

#define DEF_FREQUENCY_DOWN_DIFFERENTIAL		(10)
#define DEF_FREQUENCY_UP_THRESHOLD		(80)
#define DEF_SAMPLING_DOWN_FACTOR		(1)
#define MICRO_FREQUENCY_DOWN_DIFFERENTIAL	(3)
#define MICRO_FREQUENCY_UP_THRESHOLD		(95)
#define MICRO_FREQUENCY_MIN_SAMPLE_RATE		(10000)

#define MAX_HIST		5
#define FREQ_STEP		50000
#define IDLE_THRESHOLD		90

/* Pre-calculated summation of weight, 0.5
 * 1
 * 1 + 0.5^1 = 1.5
 * 1 + 0.5^1 + 0.5^2 = 1.75
 * 1 + 0.5^1 + 0.5^2 + 0.5^3 = 1.87
 * 1 + 0.5^1 + 0.5^2 + 0.5^3 + 0.5^4 = 1.93
 */
static int history_weight_sum[] = { 100, 150, 175, 187, 193 };

static unsigned int idle_avg[NR_CPUS];
static unsigned int idle_hist[NR_CPUS][MAX_HIST];

static DEFINE_PER_CPU(struct lb_cpu_dbs_info_s, lb_cpu_dbs_info);

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_LAB
static struct cpufreq_governor cpufreq_gov_lab;
#endif

/* Single polynomial approx -> all CPUs busy */
static int a_all = -6, b_all = 1331;
/* Single polynomial approx -> one CPUs busy */
static int a_one = 10, b_one = 205;
/* Single polynomial approx -> 2,3... CPUs busy */
static int a_rest = 4, b_rest1 = 100, b_rest2 = 300;
/* Polynomial divider */
static int poly_div = 1024;

static void dbs_freq_increase(struct cpufreq_policy *p, unsigned int freq)
{
	if (p->cur == freq)
		return;

	__cpufreq_driver_target(p, freq, CPUFREQ_RELATION_L);
}

/* Calculate average of idle time with weighting 50% less to older one.
 * With weight, average can be affected by current phase more rapidly than
 * normal average. And it also has tolerance for temporary fluctuation of
 * idle time as normal average has.
 *
 * Weigted average = sum(ai * wi) / sum(wi)
 */
static inline int cpu_idle_calc_avg(unsigned int *p, int size)
{
	int i, sum;

	for (i = 0, sum = 0; i < size; p++, i++) {
		sum += *p;
		*p >>= 1;
	}
	sum *= 100;

	return (int) (sum / history_weight_sum[size - 1]);
}

/*
 * LAB governor policy adjustement
 */
static void lb_check_cpu(int cpu, unsigned int load_freq)
{
	struct lb_cpu_dbs_info_s *dbs_info = &per_cpu(lb_cpu_dbs_info, cpu);
	struct cpufreq_policy *policy = dbs_info->cdbs.cur_policy;
	int i, idx, idle_cpus = 0, b = 0;
	static int cnt = 0;
	unsigned int freq = 0;

	idx = cnt++ % MAX_HIST;

	for_each_possible_cpu(i) {
		struct lb_cpu_dbs_info_s *dbs_cpu_info =
			&per_cpu(lb_cpu_dbs_info, i);

		idle_hist[i][idx] = dbs_cpu_info->idle_time;
		idle_avg[i] = cpu_idle_calc_avg(idle_hist[i],
					cnt < MAX_HIST ? cnt : MAX_HIST);

		if (idle_avg[i] > IDLE_THRESHOLD)
			idle_cpus++;
	}

	if (idle_cpus < 0 || idle_cpus > NR_CPUS) {
		pr_warn("idle_cpus: %d out of range\n", idle_cpus);
		return;
	}

	if (idle_cpus == 0) {
		/* Full load -> reduce freq */
		freq = policy->max * (a_all * load_freq + b_all) / poly_div;
	} else if (idle_cpus == NR_CPUS) {
		/* Idle cpus */
		freq = policy->min;
	} else if (idle_cpus == (NR_CPUS - 1)) {
		freq = policy->max * (a_one * load_freq + b_one) / poly_div;
	} else {
		/* Adjust frequency with number of available CPUS */
		/* smaller idle_cpus -> smaller frequency */
		b = ((idle_cpus - 1) * b_rest1) + b_rest2;
		freq = policy->max * (a_rest * load_freq + b) / poly_div;
	}
#if 0 
	if (!idx)
		pr_info("p->max:%d,freq: %d,idle_cpus: %d,avg : %d %d %d %d load_f: %d\n",
		       policy->max, freq, idle_cpus, idle_avg[0], idle_avg[1],
			idle_avg[2], idle_avg[3], load_freq);
#endif

	dbs_freq_increase(policy, freq);
}

static void lb_dbs_timer(struct work_struct *work)
{
	struct lb_cpu_dbs_info_s *dbs_info =
		container_of(work, struct lb_cpu_dbs_info_s, cdbs.work.work);
	unsigned int cpu = dbs_info->cdbs.cur_policy->cpu;
	struct lb_cpu_dbs_info_s *core_dbs_info = &per_cpu(lb_cpu_dbs_info,
			cpu);
	struct dbs_data *dbs_data = dbs_info->cdbs.cur_policy->governor_data;
	struct lb_dbs_tuners *lb_tuners = dbs_data->tuners;
	int delay;

	mutex_lock(&core_dbs_info->cdbs.timer_mutex);

	dbs_check_cpu(dbs_data, cpu);

	delay = delay_for_sampling_rate(lb_tuners->sampling_rate
						* core_dbs_info->rate_mult);

	gov_queue_work(dbs_data, dbs_info->cdbs.cur_policy, delay, false);
	mutex_unlock(&core_dbs_info->cdbs.timer_mutex);
}

/************************** sysfs interface ************************/
static struct common_dbs_data lb_dbs_cdata;

/**
 * update_sampling_rate - update sampling rate effective immediately if needed.
 * @new_rate: new sampling rate
 *
 * If new rate is smaller than the old, simply updating
 * dbs_tuners_int.sampling_rate might not be appropriate. For example, if the
 * original sampling_rate was 1 second and the requested new sampling rate is 10
 * ms because the user needs immediate reaction from lab governor, but not
 * sure if higher frequency will be required or not, then, the governor may
 * change the sampling rate too late; up to 1 second later. Thus, if we are
 * reducing the sampling rate, we need to make the new value effective
 * immediately.
 */
static void update_sampling_rate(struct dbs_data *dbs_data,
			unsigned int new_rate)
{
	struct lb_dbs_tuners *lb_tuners = dbs_data->tuners;
	int cpu;

	lb_tuners->sampling_rate = new_rate = max(new_rate,
			dbs_data->min_sampling_rate);

	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy;
		struct lb_cpu_dbs_info_s *dbs_info;
		unsigned long next_sampling, appointed_at;

		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;
		if (policy->governor != &cpufreq_gov_lab) {
			cpufreq_cpu_put(policy);
			continue;
		}
		dbs_info = &per_cpu(lb_cpu_dbs_info, cpu);
		cpufreq_cpu_put(policy);

		mutex_lock(&dbs_info->cdbs.timer_mutex);

		if (!delayed_work_pending(&dbs_info->cdbs.work)) {
			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			continue;
		}

		next_sampling = jiffies + usecs_to_jiffies(new_rate);
		appointed_at = dbs_info->cdbs.work.timer.expires;

		if (time_before(next_sampling, appointed_at)) {

			mutex_unlock(&dbs_info->cdbs.timer_mutex);
			cancel_delayed_work_sync(&dbs_info->cdbs.work);
			mutex_lock(&dbs_info->cdbs.timer_mutex);

			schedule_delayed_work_on(cpu, &dbs_info->cdbs.work,
					usecs_to_jiffies(new_rate));

		}
		mutex_unlock(&dbs_info->cdbs.timer_mutex);
	}
}

static ssize_t store_sampling_rate(struct dbs_data *dbs_data, const char *buf,
				size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	update_sampling_rate(dbs_data, input);
	return count;
}

show_store_one(lb, sampling_rate);
gov_sys_pol_attr_rw(sampling_rate);

static struct attribute *dbs_attributes_gov_sys[] = {
	&sampling_rate_gov_sys.attr,
	NULL
};

static struct attribute_group lb_attr_group_gov_sys = {
	.attrs = dbs_attributes_gov_sys,
	.name = "lab",
};

static struct attribute *dbs_attributes_gov_pol[] = {
	&sampling_rate_gov_pol.attr,
	NULL
};

static struct attribute_group lb_attr_group_gov_pol = {
	.attrs = dbs_attributes_gov_pol,
	.name = "lab",
};

/************************** sysfs end ************************/

static int lb_init(struct dbs_data *dbs_data)
{
	struct lb_dbs_tuners *tuners;
	u64 idle_time;
	int cpu;

	tuners = kzalloc(sizeof(struct od_dbs_tuners), GFP_KERNEL);
	if (!tuners) {
		pr_err("%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	cpu = get_cpu();
	idle_time = get_cpu_idle_time_us(cpu, NULL);
	put_cpu();
	if (idle_time != -1ULL) {
		/* Idle micro accounting is supported. Use finer thresholds */
		tuners->up_threshold = MICRO_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = MICRO_FREQUENCY_UP_THRESHOLD -
			MICRO_FREQUENCY_DOWN_DIFFERENTIAL;
		/*
		 * In nohz/micro accounting case we set the minimum frequency
		 * not depending on HZ, but fixed (very low). The deferred
		 * timer might skip some samples if idle/sleeping as needed.
		*/
		dbs_data->min_sampling_rate = MICRO_FREQUENCY_MIN_SAMPLE_RATE;
	} else {
		tuners->up_threshold = DEF_FREQUENCY_UP_THRESHOLD;
		tuners->adj_up_threshold = DEF_FREQUENCY_UP_THRESHOLD -
			DEF_FREQUENCY_DOWN_DIFFERENTIAL;

		/* For correct statistics, we need 10 ticks for each measure */
		dbs_data->min_sampling_rate = MIN_SAMPLING_RATE_RATIO *
			jiffies_to_usecs(10);
	}

	tuners->sampling_down_factor = DEF_SAMPLING_DOWN_FACTOR;
	tuners->ignore_nice = 0;

	dbs_data->tuners = tuners;
	mutex_init(&dbs_data->mutex);
	return 0;
}

static void lb_exit(struct dbs_data *dbs_data)
{
	kfree(dbs_data->tuners);
}

define_get_cpu_dbs_routines(lb_cpu_dbs_info);

static struct common_dbs_data lb_dbs_data = {
	.governor = GOV_LAB,
	.attr_group_gov_sys = &lb_attr_group_gov_sys,
	.attr_group_gov_pol = &lb_attr_group_gov_pol,
	.get_cpu_cdbs = get_cpu_cdbs,
	.get_cpu_dbs_info_s = get_cpu_dbs_info_s,
	.gov_dbs_timer = lb_dbs_timer,
	.gov_check_cpu = lb_check_cpu,
	.init = lb_init,
	.exit = lb_exit,
};

static int lb_cpufreq_governor_dbs(struct cpufreq_policy *policy,
		unsigned int event)
{
	return cpufreq_governor_dbs(policy, &lb_dbs_data, event);
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_LAB
static
#endif
struct cpufreq_governor cpufreq_gov_lab = {
	.name			= "lab",
	.governor		= lb_cpufreq_governor_dbs,
	.max_transition_latency	= TRANSITION_LATENCY_LIMIT,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	return cpufreq_register_governor(&cpufreq_gov_lab);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_lab);
}

MODULE_AUTHOR("Jonghwa Lee <jonghwa3.lee@samsung.com>");
MODULE_AUTHOR("Lukasz Majewski <l.majewski@samsung.com>");
MODULE_DESCRIPTION("'cpufreq_lab' - A dynamic cpufreq governor for "
		"Legacy Application Boosting");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_LAB
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
