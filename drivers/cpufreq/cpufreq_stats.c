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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/percpu.h>
#include <linux/kobject.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <asm/cputime.h>

static spinlock_t cpufreq_stats_lock;

struct cpufreq_stats {
	unsigned int cpu;
	unsigned int total_trans;
	unsigned long long  last_time;
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

static DEFINE_PER_CPU(struct cpufreq_stats *, cpufreq_stats_table);

struct cpufreq_stats_attribute {
	struct attribute attr;
	ssize_t(*show) (struct cpufreq_stats *, char *);
};

static int cpufreq_stats_update(unsigned int cpu)
{
	struct cpufreq_stats *stat;
	unsigned long long cur_time;

	cur_time = get_jiffies_64();
	spin_lock(&cpufreq_stats_lock);
	stat = per_cpu(cpufreq_stats_table, cpu);
	if (stat->time_in_state)
		stat->time_in_state[stat->last_index] +=
			cur_time - stat->last_time;
	stat->last_time = cur_time;
	spin_unlock(&cpufreq_stats_lock);
	return 0;
}

static ssize_t show_total_trans(struct cpufreq_policy *policy, char *buf)
{
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, policy->cpu);
	if (!stat)
		return 0;
	return sprintf(buf, "%d\n",
			per_cpu(cpufreq_stats_table, stat->cpu)->total_trans);
}

static ssize_t show_time_in_state(struct cpufreq_policy *policy, char *buf)
{
	ssize_t len = 0;
	int i;
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, policy->cpu);
	if (!stat)
		return 0;
	cpufreq_stats_update(stat->cpu);
	for (i = 0; i < stat->state_num; i++) {
		len += sprintf(buf + len, "%u %llu\n", stat->freq_table[i],
			(unsigned long long)
			jiffies_64_to_clock_t(stat->time_in_state[i]));
	}
	return len;
}

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
static ssize_t show_trans_table(struct cpufreq_policy *policy, char *buf)
{
	ssize_t len = 0;
	int i, j;

	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, policy->cpu);
	if (!stat)
		return 0;
	cpufreq_stats_update(stat->cpu);
	len += snprintf(buf + len, PAGE_SIZE - len, "   From  :    To\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "         : ");
	for (i = 0; i < stat->state_num; i++) {
		if (len >= PAGE_SIZE)
			break;
		len += snprintf(buf + len, PAGE_SIZE - len, "%9u ",
				stat->freq_table[i]);
	}
	if (len >= PAGE_SIZE)
		return PAGE_SIZE;

	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	for (i = 0; i < stat->state_num; i++) {
		if (len >= PAGE_SIZE)
			break;

		len += snprintf(buf + len, PAGE_SIZE - len, "%9u: ",
				stat->freq_table[i]);

		for (j = 0; j < stat->state_num; j++)   {
			if (len >= PAGE_SIZE)
				break;
			len += snprintf(buf + len, PAGE_SIZE - len, "%9u ",
					stat->trans_table[i*stat->max_state+j]);
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
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, policy->cpu);
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
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, policy->cpu);
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
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, policy->cpu);
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
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, cpu);

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
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, cpu);

	if (stat) {
		pr_debug("Remove load_table debugfs file\n");
		debugfs_remove(stat->debugfs_load_table);
	}
}

static void cpufreq_stats_store_load_table(struct cpufreq_freqs *freq,
					   unsigned long val)
{
	struct cpufreq_stats *stat;
	int cpu, last_idx;

	stat = per_cpu(cpufreq_stats_table, freq->cpu);
	if (!stat)
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
		last_idx = stat->load_last_index;

		stat->load_table[last_idx].time = freq->time;
		stat->load_table[last_idx].old = freq->old;
		stat->load_table[last_idx].new = freq->old;
		for_each_present_cpu(cpu)
			stat->load_table[last_idx].load[cpu] = freq->load[cpu];

		if (++stat->load_last_index == stat->load_max_index)
			stat->load_last_index = 0;
		break;
	}

	spin_unlock(&cpufreq_stats_lock);
}

static int freq_table_get_index(struct cpufreq_stats *stat, unsigned int freq)
{
	int index;
	for (index = 0; index < stat->max_state; index++)
		if (stat->freq_table[index] == freq)
			return index;
	return -1;
}

/* should be called late in the CPU removal sequence so that the stats
 * memory is still available in case someone tries to use it.
 */
static void cpufreq_stats_free_table(unsigned int cpu)
{
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table, cpu);

	if (stat) {
		pr_debug("%s: Free stat table\n", __func__);
		kfree(stat->time_in_state);
		kfree(stat);
		per_cpu(cpufreq_stats_table, cpu) = NULL;
	}
}

/* must be called early in the CPU removal sequence (before
 * cpufreq_remove_dev) so that policy is still valid.
 */
static void cpufreq_stats_free_sysfs(unsigned int cpu)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return;

	if (!cpufreq_frequency_get_table(cpu))
		goto put_ref;

	if (!policy_is_shared(policy)) {
		pr_debug("%s: Free sysfs stat\n", __func__);
		sysfs_remove_group(&policy->kobj, &stats_attr_group);
	}

put_ref:
	cpufreq_cpu_put(policy);
}

static int cpufreq_stats_create_table(struct cpufreq_policy *policy,
		struct cpufreq_frequency_table *table)
{
	unsigned int i, j, count = 0, ret = 0;
	struct cpufreq_stats *stat;
	struct cpufreq_policy *data;
	unsigned int alloc_size;
	unsigned int cpu = policy->cpu;

	if (per_cpu(cpufreq_stats_table, cpu)) {
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

		return -EBUSY;
	}

	stat = kzalloc(sizeof(struct cpufreq_stats), GFP_KERNEL);
	if ((stat) == NULL)
		return -ENOMEM;

	data = cpufreq_cpu_get(cpu);
	if (data == NULL) {
		ret = -EINVAL;
		goto error_get_fail;
	}

	ret = sysfs_create_group(&data->kobj, &stats_attr_group);
	if (ret)
		goto error_out;

	stat->cpu = cpu;
	per_cpu(cpufreq_stats_table, cpu) = stat;

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;
		count++;
	}

	alloc_size = count * sizeof(int) + count * sizeof(u64);

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	alloc_size += count * count * sizeof(int);
#endif
	stat->max_state = count;
	stat->time_in_state = kzalloc(alloc_size, GFP_KERNEL);
	if (!stat->time_in_state) {
		ret = -ENOMEM;
		goto error_out;
	}
	stat->freq_table = (unsigned int *)(stat->time_in_state + count);

#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
	stat->trans_table = stat->freq_table + count;
#endif
	j = 0;
	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;
		if (freq_table_get_index(stat, freq) == -1)
			stat->freq_table[j++] = freq;
	}
	stat->state_num = j;
	spin_lock(&cpufreq_stats_lock);
	stat->last_time = get_jiffies_64();
	stat->last_index = freq_table_get_index(stat, policy->cur);

	ret = cpufreq_stats_create_debugfs(data);
	if (ret < 0) {
		spin_unlock(&cpufreq_stats_lock);
		ret = -EINVAL;
		goto error_out;
	}

	spin_unlock(&cpufreq_stats_lock);
	cpufreq_cpu_put(data);
	return 0;
error_out:
	cpufreq_cpu_put(data);
error_get_fail:
	kfree(stat);
	per_cpu(cpufreq_stats_table, cpu) = NULL;
	return ret;
}

static void cpufreq_stats_update_policy_cpu(struct cpufreq_policy *policy)
{
	struct cpufreq_stats *stat = per_cpu(cpufreq_stats_table,
			policy->last_cpu);

	pr_debug("Updating stats_table for new_cpu %u from last_cpu %u\n",
			policy->cpu, policy->last_cpu);
	per_cpu(cpufreq_stats_table, policy->cpu) = per_cpu(cpufreq_stats_table,
			policy->last_cpu);
	per_cpu(cpufreq_stats_table, policy->last_cpu) = NULL;
	stat->cpu = policy->cpu;
}

static int cpufreq_stat_notifier_policy(struct notifier_block *nb,
		unsigned long val, void *data)
{
	int ret;
	struct cpufreq_policy *policy = data;
	struct cpufreq_frequency_table *table;
	unsigned int cpu = policy->cpu;

	if (val == CPUFREQ_UPDATE_POLICY_CPU) {
		cpufreq_stats_update_policy_cpu(policy);
		return 0;
	}

	if (val != CPUFREQ_NOTIFY)
		return 0;
	table = cpufreq_frequency_get_table(cpu);
	if (table) {
		ret = cpufreq_stats_create_table(policy, table);
		if (ret)
			return ret;
	}

	return 0;
}

static int cpufreq_stat_notifier_trans(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpufreq_stats *stat;
	int old_index, new_index;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		stat = per_cpu(cpufreq_stats_table, freq->cpu);
		if (!stat)
			return 0;

		old_index = stat->last_index;
		new_index = freq_table_get_index(stat, freq->new);

		/* We can't do stat->time_in_state[-1]= .. */
		if (old_index == -1 || new_index == -1)
			return 0;

		cpufreq_stats_update(freq->cpu);

		if (old_index == new_index)
			return 0;

		spin_lock(&cpufreq_stats_lock);
		stat->last_index = new_index;
#ifdef CONFIG_CPU_FREQ_STAT_DETAILS
		stat->trans_table[old_index * stat->max_state + new_index]++;
#endif
		stat->total_trans++;
		spin_unlock(&cpufreq_stats_lock);

		cpufreq_stats_store_load_table(freq, CPUFREQ_POSTCHANGE);

		break;
	case CPUFREQ_LOADCHECK:
		cpufreq_stats_store_load_table(freq, CPUFREQ_LOADCHECK);
		break;
	}

	return 0;
}

static int __cpuinit cpufreq_stat_cpu_callback(struct notifier_block *nfb,
					       unsigned long action,
					       void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		cpufreq_update_policy(cpu);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		cpufreq_stats_free_debugfs(cpu);
		cpufreq_stats_free_sysfs(cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		cpufreq_stats_free_load_table(cpu);
		cpufreq_stats_free_table(cpu);
		break;
	}
	return NOTIFY_OK;
}

/* priority=1 so this will get called before cpufreq_remove_dev */
static struct notifier_block cpufreq_stat_cpu_notifier __refdata = {
	.notifier_call = cpufreq_stat_cpu_callback,
	.priority = 1,
};

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

	register_hotcpu_notifier(&cpufreq_stat_cpu_notifier);
	for_each_online_cpu(cpu)
		cpufreq_update_policy(cpu);

	ret = cpufreq_register_notifier(&notifier_trans_block,
				CPUFREQ_TRANSITION_NOTIFIER);
	if (ret) {
		cpufreq_unregister_notifier(&notifier_policy_block,
				CPUFREQ_POLICY_NOTIFIER);
		unregister_hotcpu_notifier(&cpufreq_stat_cpu_notifier);
		for_each_online_cpu(cpu)
			cpufreq_stats_free_table(cpu);
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
	unregister_hotcpu_notifier(&cpufreq_stat_cpu_notifier);
	for_each_online_cpu(cpu) {
		cpufreq_stats_free_table(cpu);
		cpufreq_stats_free_debugfs(cpu);
		cpufreq_stats_free_sysfs(cpu);
	}
}

MODULE_AUTHOR("Zou Nan hai <nanhai.zou@intel.com>");
MODULE_DESCRIPTION("'cpufreq_stats' - A driver to export cpufreq stats "
				"through sysfs filesystem");
MODULE_LICENSE("GPL");

module_init(cpufreq_stats_init);
module_exit(cpufreq_stats_exit);
