/*
 *  drivers/cpufreq/cpufreq_stats_tizen.c
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/cputime.h>
#include <linux/suspend.h>
#include <linux/cpufreq_stats_tizen.h>
#ifdef CONFIG_GPUFREQ_STAT
#include <linux/gpufreq_stat.h>
#endif
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif

static DEFINE_SPINLOCK(cpufreq_stats_tizen_lock);

static struct cpufreq_stats cpufreq_stat_tizen;
#ifdef CONFIG_GPUFREQ_STAT
static struct gpufreq_stat *gpufreq_stat;
#endif

#ifdef CONFIG_ENERGY_MONITOR
static u64 *emon_time_in_state;

/*struct cpufreq_stats_tizen *cpufreq_stats_tizen_alloc_stats_table(void)
{
 	struct cpufreq_stats_tizen *stats_table = NULL;
	struct cpufreq_stats *stats_tizen = &cpufreq_stat_tizen;
	unsigned int alloc_size;

	if (!stats_tizen || !stats_tizen->time_in_state)
		return stats_table;

	stats_table = kzalloc(sizeof(*stats_table), GFP_KERNEL);
	if (!stats_table)
		return stats_table;

	stats_table->state_num = stats_tizen->state_num;

	alloc_size = stats_tizen->max_state * sizeof(int) + stats_tizen->max_state * sizeof(u64);
	stats_table->time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!stats_table->time_in_state)
		goto fail;

	stats_table->freq_table = stats_tizen->freq_table;

	return stats_table;
fail:
	kfree(stats_table);
	return NULL;
}

void cpufreq_stats_tizen_free_stats_table(
					struct cpufreq_stats_tizen *stats_table)
{
	kfree(stats_table->time_in_state);
	kfree(stats_table);
}*/

int cpufreq_stats_tizen_get_stats(int type,
 					struct cpufreq_stats_tizen *cpufreq_stats)
{
	unsigned int i;
	struct cpufreq_policy *policy;
	struct cpufreq_stats *stats;
	struct cpufreq_stats *stats_tizen = &cpufreq_stat_tizen;

	if (!cpufreq_stats)
		return -EINVAL;

	if (!emon_time_in_state  || !stats_tizen)
		return -ENOMEM;

	policy = cpufreq_cpu_get(0);
	if (!policy)
		return -EAGAIN;

	if (policy->fast_switch_enabled) {
		cpufreq_cpu_put(policy);
		return -EBUSY;
	}

	stats = policy->stats;
	cpufreq_stats_update(stats);
	for (i = 0; i < stats->state_num; i++) {
		cpufreq_stats->state_num = stats->state_num;
		cpufreq_stats->time_in_state[i] =
			stats->time_in_state[i] - emon_time_in_state[i];
		if (type != ENERGY_MON_TYPE_DUMP)
			emon_time_in_state[i] = stats->time_in_state[i];
	}
	cpufreq_cpu_put(policy);

	cpufreq_stats->freq_table = stats_tizen->freq_table;

	return 0;
}
#endif

static int freq_table_get_index(struct cpufreq_stats *stats, unsigned int freq)
{
	int index;
	for (index = 0; index < stats->max_state; index++)
		if (stats->freq_table[index] == freq)
			return index;
	return -1;
}

static int __cpufreq_stats_tizen_create_table(struct cpufreq_policy *policy)
{
	unsigned int i = 0, count = 0;
	struct cpufreq_stats *stats = &cpufreq_stat_tizen;
	unsigned int alloc_size;
	struct cpufreq_frequency_table *pos, *table;

	/* We need cpufreq table for creating stats table */
	table = policy->freq_table;
	if (unlikely(!table))
		return -1;

	/* stats not initialized */
	if (!policy->stats)
		return -1;

	/* Find total allocation size */
	cpufreq_for_each_valid_entry(pos, table)
		count++;

	alloc_size = count * sizeof(int) + count * sizeof(u64);

	/* Allocate memory for time_in_state/freq_table in one go */
	stats->time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!stats->time_in_state)
		return -1;
#ifdef CONFIG_ENERGY_MONITOR
	emon_time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!emon_time_in_state)
		pr_warn("%s could not allocate memory for emon_time_in_state\n",
			__func__);
#endif

	stats->freq_table = (unsigned int *)(stats->time_in_state + count);

	stats->max_state = count;

	/* Find valid-unique entries */
	cpufreq_for_each_valid_entry(pos, table)
		if (freq_table_get_index(stats, pos->frequency) == -1)
			stats->freq_table[i++] = pos->frequency;

	stats->state_num = i;
	stats->last_time = get_jiffies_64();
	stats->last_index = freq_table_get_index(stats, policy->cur);

	pr_info("%s finish\n", __func__);

	return 0;
}

static void cpufreq_stats_tizen_create_table(unsigned int cpu)
{
	int ret;
	struct cpufreq_policy *policy;

	/*
	 * "likely(!policy)" because normally cpufreq_stats will be registered
	 * before cpufreq driver
	 */
	policy = cpufreq_cpu_get(cpu);
	if (likely(!policy))
		return;

	ret = __cpufreq_stats_tizen_create_table(policy);
	if (ret < 0)
		pr_warn("%s could not allocate memory for time_in_state/freq_table\n",
			__func__);

	cpufreq_cpu_put(policy);

	pr_info("%s finish\n", __func__);
}

static void __cpufreq_stats_tizen_free_table(struct cpufreq_policy *policy)
{
	struct cpufreq_stats *stats_tizen = &cpufreq_stat_tizen;

	if (!policy->stats)
		return;

	if (!stats_tizen)
		return;

	pr_debug("%s: Free cpufre_stat_tizen table\n", __func__);

	kfree(stats_tizen->time_in_state);
	stats_tizen->time_in_state = NULL;
	stats_tizen->freq_table = NULL;
#ifdef CONFIG_ENERGY_MONITOR
	kfree(emon_time_in_state);
	emon_time_in_state = NULL;
#endif

	pr_info("%s finish\n", __func__);
}

static void cpufreq_stats_tizen_free_table(unsigned int cpu)
{
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return;

	__cpufreq_stats_tizen_free_table(policy);

	cpufreq_cpu_put(policy);

	pr_info("%s finish\n", __func__);
}

static void cpufreq_stats_tizen_update_stat_slpmon(void)
{
	unsigned int i;
	struct cpufreq_policy *policy;
	struct cpufreq_stats *stats;
	struct cpufreq_stats *stats_tizen = &cpufreq_stat_tizen;

	policy = cpufreq_cpu_get(0);
	if (!policy)
		return;

	stats = policy->stats;
	cpufreq_stats_update(stats);
	if (stats_tizen && stats_tizen->time_in_state) {
		for (i = 0; i < stats->state_num; i++) {
			spin_lock(&cpufreq_stats_tizen_lock);
			stats_tizen->time_in_state[i] = stats->time_in_state[i];
			spin_unlock(&cpufreq_stats_tizen_lock);
			pr_debug("%s: %llu %llu\n",
					__func__,
					stats->time_in_state[i],
					stats_tizen->time_in_state[i]);
		}
	}

	cpufreq_cpu_put(policy);
}

static void cpufreq_stats_tizen_print_stat(void)
{
	unsigned int i;
	struct cpufreq_policy *policy;
	struct cpufreq_stats *stats;
	struct cpufreq_stats *stats_tizen = &cpufreq_stat_tizen;

	policy = cpufreq_cpu_get(0);
	if (!policy)
		return;

	if (policy->fast_switch_enabled) {
		cpufreq_cpu_put(policy);
		return;
	}

	stats = policy->stats;
	cpufreq_stats_update(stats);
	if (stats_tizen && stats_tizen->time_in_state) {
		pr_cont("[cfs]");
		for (i = 0; i < stats->state_num; i++) {
			pr_cont("[%llu]", (unsigned long long)
				jiffies_to_msecs(stats->time_in_state[i]) -
				jiffies_to_msecs(stats_tizen->time_in_state[i]));
		}
	}

	cpufreq_cpu_put(policy);
	pr_cont("\n");
}

#ifdef CONFIG_GPUFREQ_STAT
static void freq_stats_tizen_print_gpu(void)
{
	int i;
	int err;

	err = gpufreq_stat_get_stat_delta(gpufreq_stat);
	if (err)
		return;

	pr_cont("[gfs]");
	for (i = 0; i < gpufreq_stat->table_size; i++)
		pr_cont("[%llu]",
				(unsigned long long)
				jiffies_to_msecs(gpufreq_stat->table[i].time));
	pr_cont("\n");

	return;
}
#else
static void freq_stats_tizen_print_gpu(void) {};
#endif

static int cpufreq_stats_tizen_pm_suspend_prepare_cb(void)
{
	cpufreq_stats_tizen_print_stat();
	freq_stats_tizen_print_gpu();

	return 0;
}

static int cpufreq_stats_tizen_pm_post_suspend_cb(void)
{
	cpufreq_stats_tizen_update_stat_slpmon();

	return 0;
}

static int cpufreq_stats_tizen_pm_notifier(struct notifier_block *nb,
		unsigned long event, void *dummy)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		cpufreq_stats_tizen_pm_suspend_prepare_cb();
		break;
	case PM_POST_SUSPEND:
		cpufreq_stats_tizen_pm_post_suspend_cb();
		break;
	  default:
		break;
	}
	return 0;
}

static int cpufreq_stat_tizen_notifier_policy(struct notifier_block *nb,
		unsigned long val, void *data)
{
	int ret = 0;
	struct cpufreq_policy *policy = data;

	if (val == CPUFREQ_CREATE_POLICY)
		ret = __cpufreq_stats_tizen_create_table(policy);
	else if (val == CPUFREQ_REMOVE_POLICY)
		__cpufreq_stats_tizen_free_table(policy);

	return ret;
}

static struct notifier_block notifier_policy_block = {
	.notifier_call = cpufreq_stat_tizen_notifier_policy
};

static struct notifier_block pm_notifier_block = {
	.notifier_call = cpufreq_stats_tizen_pm_notifier,
};

static int __init cpufreq_stats_tizen_init(void)
{
	int ret;

	cpufreq_stats_tizen_create_table(0);

	ret = cpufreq_register_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
	if (ret) {
		cpufreq_stats_tizen_free_table(0);
		return ret;
	}

	ret = register_pm_notifier(&pm_notifier_block);
	if (ret < 0) {
		cpufreq_stats_tizen_free_table(0);
		cpufreq_unregister_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
		return ret;
	}

#ifdef CONFIG_GPUFREQ_STAT
	gpufreq_stat = gpufreq_stat_alloc_stat_table();
	if (!gpufreq_stat)
		return -ENOMEM;
#endif

	return 0;
}
static void __exit cpufreq_stats_tizen_exit(void)
{
	cpufreq_stats_tizen_free_table(0);
	unregister_pm_notifier(&pm_notifier_block);
	cpufreq_unregister_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
#ifdef CONFIG_GPUFREQ_STAT
	gpufreq_stat_free_stat_table(gpufreq_stat);
#endif
}

MODULE_AUTHOR("Junho Jang <vincent.jang@samsung.com>");
MODULE_DESCRIPTION("'cpufreq_stats_tizen' - A driver to export cpufreq stats "
				"for tizen through sysfs filesystem");
MODULE_LICENSE("GPL");

module_init(cpufreq_stats_tizen_init);
module_exit(cpufreq_stats_tizen_exit);
