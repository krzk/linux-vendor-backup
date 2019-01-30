// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Samsung Exynos SoC Adaptive Supply Voltage support
 */

#include <linux/bitrev.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/slab.h>

#include "exynos-asv.h"
#include "exynos5422-asv.h"
#include "exynos5433-asv.h"

#ifndef MHZ
#define MHZ 1000000U
#endif

static struct exynos_asv *exynos_asv;

int exynos_asv_update_cpu_opp(struct device *cpu)
{
	struct exynos_asv_subsys *subsys = NULL;
	struct dev_pm_opp *opp;
	unsigned int opp_freq;
	int i;

	if (of_device_is_compatible(cpu->of_node,
				    exynos_asv->arm.cpu_dt_compat))
		subsys = &exynos_asv->arm;
	else if (of_device_is_compatible(cpu->of_node,
					 exynos_asv->kfc.cpu_dt_compat))
		subsys = &exynos_asv->kfc;

	if (!subsys)
		return -EINVAL;

	for (i = 0; i < subsys->dvfs_nr; i++) {
		unsigned int new_voltage;
		unsigned int voltage;
		int timeout = 1000;
		int err;

		opp_freq = subsys->asv_table[i][0];

		opp = dev_pm_opp_find_freq_exact(cpu, opp_freq * MHZ, true);
		if (IS_ERR(opp)) {
			pr_info("%s cpu%d opp%d, freq: %u missing\n",
				__func__, cpu->id, i, opp_freq);

			continue;
		}

		voltage = dev_pm_opp_get_voltage(opp);
		new_voltage = exynos_asv->opp_get_voltage(subsys, i, opp_freq,
							 voltage);
		dev_pm_opp_put(opp);

		opp_freq *= MHZ;
		dev_pm_opp_remove(cpu, opp_freq);

		while (--timeout) {
			opp = dev_pm_opp_find_freq_exact(cpu, opp_freq, true);
			if (IS_ERR(opp))
				break;
			dev_pm_opp_put(opp);
			msleep(1);
		}

		err = dev_pm_opp_add(cpu, opp_freq, new_voltage);
		if (err < 0)
			pr_err("%s: Failed to add OPP %u Hz/%u uV for cpu%d (%d)\n",
			       __func__, opp_freq, new_voltage, cpu->id, err);
	}

	return 0;
}

static int exynos_asv_update_opp(void)
{
	struct opp_table *last_opp_table = NULL;
	struct device *cpu;
	int ret, cpuid;

	for_each_possible_cpu(cpuid) {
		struct opp_table *opp_table;

		cpu = get_cpu_device(cpuid);
		if (!cpu)
			continue;

		opp_table = dev_pm_opp_get_opp_table(cpu);
		if (IS_ERR(opp_table))
			continue;

		if (!last_opp_table || opp_table != last_opp_table) {
			last_opp_table = opp_table;

			ret = exynos_asv_update_cpu_opp(cpu);
			if (ret < 0)
				pr_err("%s: Couldn't udate OPPs for cpu%d\n",
				       __func__, cpuid);
		}

		dev_pm_opp_put_opp_table(opp_table);
	}

	return	0;
}

static int __init exynos_asv_init(void)
{
	int ret;

	exynos_asv = kcalloc(1, sizeof(struct exynos_asv), GFP_KERNEL);
	if (!exynos_asv)
		return -ENOMEM;

	if (of_machine_is_compatible("samsung,exynos5800") ||
	    of_machine_is_compatible("samsung,exynos5420"))
		ret = exynos5422_asv_init(exynos_asv);
	else if (of_machine_is_compatible("samsung,exynos5433"))
		ret = exynos5433_asv_init(exynos_asv);
	else
		return 0;

	if (ret < 0)
		return ret;

	return exynos_asv_update_opp();
}
late_initcall(exynos_asv_init)
