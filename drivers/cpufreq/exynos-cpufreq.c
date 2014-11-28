/*
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - CPU frequency scaling support for EXYNOS series
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include <plat/cpu.h>

#include "exynos-cpufreq.h"

static struct exynos_dvfs_info *exynos_info;

static struct regulator *arm_regulator;
static struct cpufreq_freqs freqs;

static unsigned int locking_frequency;
static bool frequency_locked;
static DEFINE_MUTEX(cpufreq_lock);

static int exynos_verify_speed(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy,
					      exynos_info->freq_table);
}

static unsigned int exynos_getspeed(unsigned int cpu)
{
	return clk_get_rate(exynos_info->cpu_clk) / 1000;
}

static int exynos_cpufreq_get_index(unsigned int freq)
{
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	int index;

	for (index = 0;
		freq_table[index].frequency != CPUFREQ_TABLE_END; index++)
		if (freq_table[index].frequency == freq)
			break;

	if (freq_table[index].frequency == CPUFREQ_TABLE_END)
		return -EINVAL;

	return index;
}

static int exynos_cpufreq_scale(unsigned int target_freq)
{
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	unsigned int *volt_table = exynos_info->volt_table;
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);
	unsigned int arm_volt, safe_arm_volt = 0;
	unsigned int mpll_freq_khz = exynos_info->mpll_freq_khz;
	int index, old_index;
	int ret = 0;

	freqs.old = policy->cur;
	freqs.new = target_freq;

	if (freqs.new == freqs.old)
		goto out;

	/*
	 * The policy max have been changed so that we cannot get proper
	 * old_index with cpufreq_frequency_table_target(). Thus, ignore
	 * policy and get the index from the raw freqeuncy table.
	 */
	old_index = exynos_cpufreq_get_index(freqs.old);
	if (old_index < 0) {
		ret = old_index;
		goto out;
	}

	index = exynos_cpufreq_get_index(target_freq);
	if (index < 0) {
		ret = index;
		goto out;
	}

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * required voltage level
	 */
	if (exynos_info->need_apll_change != NULL) {
		if (exynos_info->need_apll_change(old_index, index) &&
		   (freq_table[index].frequency < mpll_freq_khz) &&
		   (freq_table[old_index].frequency < mpll_freq_khz))
			safe_arm_volt = volt_table[exynos_info->pll_safe_idx];
	}
	arm_volt = volt_table[index];

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	/* When the new frequency is higher than current frequency */
	if ((freqs.new > freqs.old) && !safe_arm_volt) {
		/* Firstly, voltage up to increase frequency */
		ret = regulator_set_voltage(arm_regulator, arm_volt, arm_volt);
		if (ret) {
			pr_err("%s: failed to set cpu voltage to %d\n",
				__func__, arm_volt);
			goto out;
		}
	}

	if (safe_arm_volt) {
		ret = regulator_set_voltage(arm_regulator, safe_arm_volt,
				      safe_arm_volt);
		if (ret) {
			pr_err("%s: failed to set cpu voltage to %d\n",
				__func__, safe_arm_volt);
			goto out;
		}
	}

	exynos_info->set_freq(old_index, index);

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);

	/* When the new frequency is lower than current frequency */
	if ((freqs.new < freqs.old) ||
	   ((freqs.new > freqs.old) && safe_arm_volt)) {
		/* down the voltage after frequency change */
		regulator_set_voltage(arm_regulator, arm_volt,
				arm_volt);
		if (ret) {
			pr_err("%s: failed to set cpu voltage to %d\n",
				__func__, arm_volt);
			goto out;
		}
	}

out:

	cpufreq_cpu_put(policy);

	return ret;
}

static int exynos_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_frequency_table *freq_table = exynos_info->freq_table;
	unsigned int index;
	unsigned int new_freq;
	int ret = 0;

	mutex_lock(&cpufreq_lock);

	if (frequency_locked)
		goto out;

	if (cpufreq_frequency_table_target(policy, freq_table,
					   target_freq, relation, &index)) {
		ret = -EINVAL;
		goto out;
	}

	new_freq = freq_table[index].frequency;

	ret = exynos_cpufreq_scale(new_freq);

out:
	mutex_unlock(&cpufreq_lock);

	return ret;
}

#ifdef CONFIG_PM
static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	return 0;
}
#endif

/**
 * exynos_cpufreq_pm_notifier - block CPUFREQ's activities in suspend-resume
 *			context
 * @notifier
 * @pm_event
 * @v
 *
 * While frequency_locked == true, target() ignores every frequency but
 * locking_frequency. The locking_frequency value is the initial frequency,
 * which is set by the bootloader. In order to eliminate possible
 * inconsistency in clock values, we save and restore frequencies during
 * suspend and resume and block CPUFREQ activities. Note that the standard
 * suspend/resume cannot be used as they are too deep (syscore_ops) for
 * regulator actions.
 */
static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	int ret;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&cpufreq_lock);
		frequency_locked = true;
		mutex_unlock(&cpufreq_lock);

		ret = exynos_cpufreq_scale(locking_frequency);
		if (ret < 0)
			return NOTIFY_BAD;

		break;

	case PM_POST_SUSPEND:
		mutex_lock(&cpufreq_lock);
		frequency_locked = false;
		mutex_unlock(&cpufreq_lock);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_nb = {
	.notifier_call = exynos_cpufreq_pm_notifier,
};

static int exynos_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	policy->cur = policy->min = policy->max = exynos_getspeed(policy->cpu);

	cpufreq_frequency_table_get_attr(exynos_info->freq_table, policy->cpu);

	/* set the transition latency value */
	policy->cpuinfo.transition_latency = 100000;

	cpumask_setall(policy->cpus);

	return cpufreq_frequency_table_cpuinfo(policy, exynos_info->freq_table);
}

static int exynos_cpufreq_cpu_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_put_attr(policy->cpu);
	return 0;
}

static struct freq_attr *exynos_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	&cpufreq_freq_attr_scaling_boost_freqs,
	NULL,
};

static struct cpufreq_driver exynos_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= exynos_verify_speed,
	.target		= exynos_target,
	.get		= exynos_getspeed,
	.init		= exynos_cpufreq_cpu_init,
	.exit		= exynos_cpufreq_cpu_exit,
	.name		= "exynos_cpufreq",
	.attr		= exynos_cpufreq_attr,
#ifdef CONFIG_CPU_FREQ_BOOST_SW
	.boost_supported = true,
#endif
#ifdef CONFIG_PM
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
#endif
};

/* Device Tree Support for CPU freq */

int exynos_of_parse_boost(struct exynos_dvfs_info *info,
			  const char *property_name)
{
	struct cpufreq_frequency_table *ft = info->freq_table;
	struct device_node *node = info->dev->of_node;
	unsigned int boost_freq;
	int i;

	if (of_property_read_u32(node, property_name, &boost_freq)) {
		pr_err("%s: Property: %s not found\n", __func__,
		       property_name);
		return -ENODEV;
	}

	/*
	 * Adjust the BOOST setting code to the current cpufreq code
	 * Now we have static table definitions for frequencies, dividers
	 * and PLL parameters (like P M S)
	 *
	 * In the current cpufreq code base only the change of one entry at
	 * frequency table is required.
	 */

	for (i = 0; ft[i].frequency != CPUFREQ_TABLE_END; i++)
		if (ft[i].frequency != CPUFREQ_ENTRY_INVALID)
			break;

	if (--i >= 0) {
		ft[i].index = CPUFREQ_BOOST_FREQ;
		ft[i].frequency = boost_freq;
	} else {
		pr_err("%s: BOOST index: %d out of range\n", __func__, i);
		return -EINVAL;
	}

	pr_debug("%s: BOOST frequency: %d\n", __func__, ft[i].frequency);

	return 0;
}

struct cpufreq_frequency_table *exynos_of_parse_freq_table(
		struct exynos_dvfs_info *info, const char *property_name)
{
	struct device_node *node = info->dev->of_node;
	struct cpufreq_frequency_table *ft, *ret = NULL;
	int len, num, i = 0, k;
	struct property *pp;
	u32 *of_f_tab;

	if (!node)
		return NULL;

	pp = of_find_property(node, property_name, &len);
	if (!pp) {
		pr_debug("%s: Property: %s not found\n", __func__,
			 property_name);
		goto err;
	}

	if (len == 0) {
		pr_debug("%s: Length wrong value!\n", __func__);
		goto err;
	}

	of_f_tab = kzalloc(len, GFP_KERNEL);
	if (!of_f_tab) {
		pr_err("%s: Allocation failed\n", __func__);
		goto err;
	}

	num = len / sizeof(u32);

	if (of_property_read_u32_array(node, pp->name, of_f_tab, num)) {
		pr_err("%s: Property: %s cannot be read!\n", __func__,
		       pp->name);
		goto err_of_f_tab;
	}

	/*
	 * Here + 1 is required for CPUFREQ_TABLE_END
	 *
	 * Number of those entries must correspond to the apll_freq_4412 table
	 */
	ft = kzalloc(sizeof(struct cpufreq_frequency_table) *
		     (info->freq_levels + 1), GFP_KERNEL);
	if (!ft) {
		pr_err("%s: Allocation failed\n", __func__);
		goto err_of_f_tab;
	}

	i = info->freq_levels;
	ft[i].index = 0;
	ft[i].frequency = CPUFREQ_TABLE_END;

	for (i--, k = num - 1; i >= 0; i--, k--) {
		ft[i].index = i;
		if (k < 0)
			ft[i].frequency = CPUFREQ_ENTRY_INVALID;
		else
			ft[i].frequency = of_f_tab[k];
	}

	ret = ft;

 err_of_f_tab:
	kfree(of_f_tab);
 err:
	return ret;
}

unsigned int *exynos_of_parse_volt_table(struct exynos_dvfs_info *info,
					 const char *property_name)
{
	struct device_node *node = info->dev->of_node;
	struct property *pp;
	int len, num;
	u32 *of_v_tab;

	if (!node)
		return NULL;

	pp = of_find_property(node, property_name, &len);
	if (!pp) {
		pr_debug("%s: Property: %s not found\n", __func__,
			 property_name);
		goto err;
	}

	if (len == 0) {
		pr_debug("%s: Length wrong value!\n", __func__);
		goto err;
	}

	of_v_tab = kzalloc(len, GFP_KERNEL);
	if (!of_v_tab) {
		pr_err("%s: Allocation failed\n", __func__);
		goto err;
	}

	num = len / sizeof(u32);
	if (of_property_read_u32_array(node, pp->name, of_v_tab, num)) {
		pr_err("%s: Property: %s cannot be read!\n", __func__,
		       pp->name);
		goto err_of_v_tab;
	}

	return of_v_tab;

 err_of_v_tab:
	kfree(of_v_tab);
 err:
	return NULL;
}


#ifdef CONFIG_OF
static struct of_device_id exynos_cpufreq_of_match[] = {
	{ .compatible = "samsung,exynos-cpufreq", },
	{ },
};
#endif

static int exynos_cpufreq_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;

	exynos_info = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
	if (!exynos_info)
		return -ENOMEM;

	exynos_info->dev = &pdev->dev;

	if (soc_is_exynos3250())
		ret = exynos3250_cpufreq_init(exynos_info);
	else if (soc_is_exynos4210())
		ret = exynos4210_cpufreq_init(exynos_info);
	else if (soc_is_exynos4212() || soc_is_exynos4412())
		ret = exynos4x12_cpufreq_init(exynos_info);
	else if (soc_is_exynos5250())
		ret = exynos5250_cpufreq_init(exynos_info);
	else
		return 0;

	if (ret)
		goto err_vdd_arm;

	if (exynos_info->set_freq == NULL) {
		pr_err("%s: No set_freq function (ERR)\n", __func__);
		goto err_vdd_arm;
	}

	arm_regulator = regulator_get(exynos_info->dev, "vdd_arm");
	if (IS_ERR(arm_regulator)) {
		pr_err("%s: failed to get resource vdd_arm\n", __func__);
		goto err_vdd_arm;
	}

	locking_frequency = exynos_getspeed(0);

	register_pm_notifier(&exynos_cpufreq_nb);

	if (cpufreq_register_driver(&exynos_driver)) {
		pr_err("%s: failed to register cpufreq driver\n", __func__);
		goto err_cpufreq;
	}

	return 0;
err_cpufreq:
	unregister_pm_notifier(&exynos_cpufreq_nb);

	regulator_put(arm_regulator);
err_vdd_arm:
	kfree(exynos_info);
	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}

static struct platform_driver exynos_cpufreq_driver = {
	.probe		= exynos_cpufreq_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= "exynos-cpufreq",
		.of_match_table = of_match_ptr(exynos_cpufreq_of_match),
	}
};

static int __init exynos_cpufreq_init(void)
{
	int ret;
	ret = platform_driver_register(&exynos_cpufreq_driver);
	if (ret) {
		pr_err("%s: Failed to register CPUFREQ driver\n", __func__);
	}

	return ret;
}

late_initcall(exynos_cpufreq_init);
