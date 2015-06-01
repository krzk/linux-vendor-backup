/*
 *  drivers/cpufreq/cpufreq_stats.c
 *
 *  Copyright (C) 2003-2004 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>.
 *  (C) 2004 Zou Nan hai <nanhai.zou@intel.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cputime.h>

static spinlock_t cpufreq_stats_lock;

struct cpufreq_stats {
	unsigned int total_trans;
	unsigned long long last_time;
	unsigned int max_state;
	unsigned int state_num;
	unsigned int last_index;
	u64 *time_in_state;
	unsigned int *freq_table;
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	unsigned int *trans_table;
#endif

	/* Debugfs file for load_table */
	struct cpufreq_freqs *load_table;
	unsigned int load_last_index;
	unsigned int load_max_index;
	struct dentry *debugfs_load_table;
};

static int cpufreq_stats_update(struct cpufreq_stats *stats)
{
	unsigned long long cur_time = get_jiffies_64();

	spin_lock(&cpufreq_stats_lock);
	stats->time_in_state[stats->last_index] += cur_time - stats->last_time;
	stats->last_time = cur_time;
	spin_unlock(&cpufreq_stats_lock);
	return 0;
}

static ssize_t show_total_trans(struct cpufreq_policy *policy, char *buf)
{
	return sprintf(buf, "%d\n", policy->stats->total_trans);
}

static ssize_t show_time_in_state(struct cpufreq_policy *policy, char *buf)
{
	struct cpufreq_stats *stats = policy->stats;
	ssize_t len = 0;
	int i;

	cpufreq_stats_update(stats);
	for (i = 0; i < stats->state_num; i++) {
		len += sprintf(buf + len, "%u %llu\n", stats->freq_table[i],
			(unsigned long long)
			jiffies_64_to_clock_t(stats->time_in_state[i]));
	}
	return len;
}

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
static ssize_t show_trans_table(struct cpufreq_policy *policy, char *buf)
{
	struct cpufreq_stats *stats = policy->stats;
	ssize_t len = 0;
	int i, j;

	len += snprintf(buf + len, PAGE_SIZE - len, "   From  :    To\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "         : ");
	for (i = 0; i < stats->state_num; i++) {
		if (len >= PAGE_SIZE)
			break;
		len += snprintf(buf + len, PAGE_SIZE - len, "%9u ",
				stats->freq_table[i]);
	}
	if (len >= PAGE_SIZE)
		return PAGE_SIZE;

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	for (i = 0; i < stats->state_num; i++) {
		if (len >= PAGE_SIZE)
			break;

		len += snprintf(buf + len, PAGE_SIZE - len, "%9u: ",
				stats->freq_table[i]);

		for (j = 0; j < stats->state_num; j++) {
			if (len >= PAGE_SIZE)
				break;
			len += snprintf(buf + len, PAGE_SIZE - len, "%9u ",
					stats->trans_table[i*stats->max_state+j]);
		}
		if (len >= PAGE_SIZE)
			break;
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	}
	if (len >= PAGE_SIZE)
		return PAGE_SIZE;
	return len;
}
cpufreq_freq_attr_ro(trans_table);
#endif

cpufreq_freq_attr_ro(total_trans);
cpufreq_freq_attr_ro(time_in_state);

static struct attribute *default_attrs[] = {
	&total_trans.attr,
	&time_in_state.attr,
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	&trans_table.attr,
#endif
	NULL
};
static struct attribute_group stats_attr_group = {
	.attrs = default_attrs,
	.name = "stats"
};

#define MAX_LINE_SIZE		255
static ssize_t load_table_read(struct file *file, char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct cpufreq_policy *policy = file->private_data;
	struct cpufreq_stats *stat = policy->stats;
	struct cpufreq_freqs *load_table = stat->load_table;
	ssize_t len = 0;
	char *buf;
	int i, cpu, ret;

	buf = kzalloc(MAX_LINE_SIZE * stat->load_max_index, GFP_KERNEL);
	if (!buf)
		return 0;

	spin_lock(&cpufreq_stats_lock);
	len += sprintf(buf + len, "%-10s %-12s %-12s ", "Time(ms)",
						    "Old Freq(Hz)",
						    "New Freq(Hz)");
	for_each_cpu(cpu, policy->cpus)
		len += sprintf(buf + len, "%3s%d ", "CPU", cpu);
	len += sprintf(buf + len, "\n");

	i = stat->load_last_index;
	do {
		len += sprintf(buf + len, "%-10lld %-12d %-12d ",
				load_table[i].time,
				load_table[i].old,
				load_table[i].new);

		for_each_cpu(cpu, policy->cpus)
			len += sprintf(buf + len, "%-4d ",
					load_table[i].load[cpu]);
		len += sprintf(buf + len, "\n");

		if (++i == stat->load_max_index)
			i = 0;
	} while (i != stat->load_last_index);
	spin_unlock(&cpufreq_stats_lock);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return ret;
}

static const struct file_operations load_table_fops = {
	.read		= load_table_read,
	.open		= simple_open,
	.llseek		= no_llseek,
};

static int cpufreq_stats_reset_debugfs(struct cpufreq_policy *policy)
{
	struct cpufreq_stats *stat = policy->stats;
	int i;

	if (!stat->load_table)
		return -EINVAL;

	/* Reset previous data of load_table debugfs file */
	stat->load_last_index = 0;
	for (i = 0; i < stat->load_max_index; i++)
		memset(&stat->load_table[i], 0, sizeof(*stat->load_table));

	return 0;
}

static int cpufreq_stats_create_debugfs(struct cpufreq_policy *policy)
{
	struct cpufreq_stats *stat = policy->stats;
	unsigned int j, size, idx;
	int ret = 0;

	if (!stat)
		return -EINVAL;

	if (!policy->cpu_debugfs)
		return -EINVAL;

	stat->load_last_index = 0;
	stat->load_max_index = CONFIG_NR_CPU_LOAD_STORAGE;

	/* Allocate memory for storage of CPUs load */
	size = sizeof(*stat->load_table) * stat->load_max_index;
	stat->load_table = kzalloc(size, GFP_KERNEL);
	if (!stat->load_table)
		return -ENOMEM;

	/* Find proper index of cpu_debugfs array for cpu */
	idx = 0;
	for_each_cpu(j, policy->related_cpus) {
		if (j == policy->cpu)
			break;
		idx++;
	}

	/* Create debugfs directory and file for cpufreq */
	stat->debugfs_load_table = debugfs_create_file("load_table", S_IWUSR,
					policy->cpu_debugfs[idx],
					policy, &load_table_fops);
	if (!stat->debugfs_load_table) {
		ret = -ENOMEM;
		goto err;
	}

	pr_debug("Created debugfs file for CPU%d \n", policy->cpu);

	return 0;
err:
	kfree(stat->load_table);
	return ret;
}

/*
 * This function should be called late in the CPU removal sequence so that
 * the stats memory is still available in case someone tries to use it.
 */
static void cpufreq_stats_free_load_table(unsigned int cpu)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	struct cpufreq_stats *stat = policy->stats;

	if (stat) {
		pr_debug("Free memory of load_table\n");
		kfree(stat->load_table);
	}
}

/*
 * This function must be called early in the CPU removal sequence
 * (before cpufreq_remove_dev) so that policy is still valid.
 */
static void cpufreq_stats_free_debugfs(unsigned int cpu)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	struct cpufreq_stats *stat = policy->stats;

	if (stat) {
		pr_debug("Remove load_table debugfs file\n");
		debugfs_remove(stat->debugfs_load_table);
	}
}

static void cpufreq_stats_store_load_table(struct cpufreq_freqs *freq,
					   unsigned long val)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(freq->cpu);
	struct cpufreq_stats *stat;
	static int64_t time = 0;
	int cpu, last_idx;

	stat = policy->stats;
	if (!stat)
		return;

	if (!stat->load_table)
		return;

	spin_lock(&cpufreq_stats_lock);

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		if (!stat->load_last_index)
			last_idx = stat->load_max_index - 1;
		else
			last_idx = stat->load_last_index - 1;

		stat->load_table[last_idx].new = freq->new;
		break;
	case CPUFREQ_LOADCHECK:
		if (time == freq->time)
			break;
		time = freq->time;

		last_idx = stat->load_last_index;

		stat->load_table[last_idx].time = freq->time;
		stat->load_table[last_idx].old = freq->old;
		stat->load_table[last_idx].new = freq->old;
		for_each_cpu(cpu, policy->related_cpus)
			stat->load_table[last_idx].load[cpu] = freq->load[cpu];

		if (++stat->load_last_index == stat->load_max_index)
			stat->load_last_index = 0;
		break;
	}

	spin_unlock(&cpufreq_stats_lock);
}

static int freq_table_get_index(struct cpufreq_stats *stats, unsigned int freq)
{
	int index;
	for (index = 0; index < stats->max_state; index++)
		if (stats->freq_table[index] == freq)
			return index;
	return -1;
}

static void __cpufreq_stats_free_table(struct cpufreq_policy *policy)
{
	struct cpufreq_stats *stats = policy->stats;

	/* Already freed */
	if (!stats)
		return;

	pr_debug("%s: Free stats table\n", __func__);

	sysfs_remove_group(&policy->kobj, &stats_attr_group);
	kfree(stats->time_in_state);
	kfree(stats);
	policy->stats = NULL;
}

static void cpufreq_stats_free_table(unsigned int cpu)
{
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return;

	__cpufreq_stats_free_table(policy);

	cpufreq_cpu_put(policy);
}

static int __cpufreq_stats_create_table(struct cpufreq_policy *policy)
{
	unsigned int i = 0, count = 0, ret = -ENOMEM;
	struct cpufreq_stats *stats;
	unsigned int alloc_size;
	unsigned int cpu = policy->cpu;
	struct cpufreq_frequency_table *pos, *table;

	/* We need cpufreq table for creating stats table */
	table = cpufreq_frequency_get_table(cpu);
	if (unlikely(!table))
		return 0;

	/* stats already initialized */
	if (policy->stats) {
		/*
		 * Reset previous data of load_table when updating and changing
		 * cpufreq governor. If specific governor which haven't sent
		 * CPUFREQ_LOADCHECK notification is active, should reset
		 * load_table data as zero(0).
		 */
		ret = cpufreq_stats_reset_debugfs(policy);
		if (ret) {
			pr_err("Failed to reset load_table data of debugfs\n");
			return ret;
		}

		return -EEXIST;
	}

	stats = kzalloc(sizeof(*stats), GFP_KERNEL);
	if (!stats)
		return -ENOMEM;

	/* Find total allocation size */
	cpufreq_for_each_valid_entry(pos, table)
		count++;

	alloc_size = count * sizeof(int) + count * sizeof(u64);

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	alloc_size += count * count * sizeof(int);
#endif

	/* Allocate memory for time_in_state/freq_table/trans_table in one go */
	stats->time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!stats->time_in_state)
		goto free_stat;

	stats->freq_table = (unsigned int *)(stats->time_in_state + count);

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	stats->trans_table = stats->freq_table + count;
#endif

	stats->max_state = count;

	/* Find valid-unique entries */
	cpufreq_for_each_valid_entry(pos, table)
		if (freq_table_get_index(stats, pos->frequency) == -1)
			stats->freq_table[i++] = pos->frequency;

	stats->state_num = i;
	stats->last_time = get_jiffies_64();
	stats->last_index = freq_table_get_index(stats, policy->cur);

	policy->stats = stats;
	ret = cpufreq_stats_create_debugfs(policy);
	if (ret < 0) {
		spin_unlock(&cpufreq_stats_lock);
		ret = -EINVAL;
		goto free_stat;
	}

	ret = sysfs_create_group(&policy->kobj, &stats_attr_group);
	if (!ret)
		return 0;

	/* We failed, release resources */
	policy->stats = NULL;
	kfree(stats->time_in_state);
free_stat:
	kfree(stats);

	return ret;
}

static void cpufreq_stats_create_table(unsigned int cpu)
{
	struct cpufreq_policy *policy;

	/*
	 * "likely(!policy)" because normally cpufreq_stats will be registered
	 * before cpufreq driver
	 */
	policy = cpufreq_cpu_get(cpu);
	if (likely(!policy))
		return;

	__cpufreq_stats_create_table(policy);

	cpufreq_cpu_put(policy);
}

static int cpufreq_stat_notifier_policy(struct notifier_block *nb,
		unsigned long val, void *data)
{
	int ret = 0;
	struct cpufreq_policy *policy = data;

	if (val == CPUFREQ_CREATE_POLICY)
		ret = __cpufreq_stats_create_table(policy);
	else if (val == CPUFREQ_REMOVE_POLICY)
		__cpufreq_stats_free_table(policy);

	return ret;
}

static int cpufreq_stat_notifier_trans(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpufreq_policy *policy = cpufreq_cpu_get(freq->cpu);
	struct cpufreq_stats *stats;
	int old_index, new_index;

	if (!policy) {
		pr_err("%s: No policy found\n", __func__);
		return 0;
	}

	if (val != CPUFREQ_POSTCHANGE)
		goto put_policy;

	if (!policy->stats) {
		pr_debug("%s: No stats found\n", __func__);
		goto put_policy;
	}

	stats = policy->stats;

	old_index = stats->last_index;
	new_index = freq_table_get_index(stats, freq->new);

	/* We can't do stats->time_in_state[-1]= .. */
	if (old_index == -1 || new_index == -1)
		goto put_policy;

	if (old_index == new_index)
		goto put_policy;

	cpufreq_stats_update(stats);

	stats->last_index = new_index;
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	stats->trans_table[old_index * stats->max_state + new_index]++;
#endif
	stats->total_trans++;

	cpufreq_stats_store_load_table(freq, CPUFREQ_POSTCHANGE);

put_policy:
	if (val == CPUFREQ_LOADCHECK)
		cpufreq_stats_store_load_table(freq, CPUFREQ_LOADCHECK);

	cpufreq_cpu_put(policy);
	return 0;
}

static struct notifier_block notifier_policy_block = {
	.notifier_call = cpufreq_stat_notifier_policy
};

static struct notifier_block notifier_trans_block = {
	.notifier_call = cpufreq_stat_notifier_trans
};

static int __init cpufreq_stats_init(void)
{
	int ret;
	unsigned int cpu;

	spin_lock_init(&cpufreq_stats_lock);
	ret = cpufreq_register_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
	if (ret)
		return ret;

	for_each_online_cpu(cpu)
		cpufreq_stats_create_table(cpu);

	ret = cpufreq_register_notifier(&notifier_trans_block,
				CPUFREQ_TRANSITION_NOTIFIER);
	if (ret) {
		cpufreq_unregister_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
		for_each_online_cpu(cpu) {
			cpufreq_stats_free_load_table(cpu);
			cpufreq_stats_free_debugfs(cpu);
			cpufreq_stats_free_table(cpu);
		}
		return ret;
	}

	return 0;
}
static void __exit cpufreq_stats_exit(void)
{
	unsigned int cpu;

	cpufreq_unregister_notifier(&notifier_policy_block,
			CPUFREQ_POLICY_NOTIFIER);
	cpufreq_unregister_notifier(&notifier_trans_block,
			CPUFREQ_TRANSITION_NOTIFIER);
	for_each_online_cpu(cpu) {
		cpufreq_stats_free_load_table(cpu);
		cpufreq_stats_free_debugfs(cpu);
		cpufreq_stats_free_table(cpu);
	}
}

MODULE_AUTHOR("Zou Nan hai <nanhai.zou@intel.com>");
MODULE_DESCRIPTION("Export cpufreq stats via sysfs");
MODULE_LICENSE("GPL");

module_init(cpufreq_stats_init);
module_exit(cpufreq_stats_exit);
