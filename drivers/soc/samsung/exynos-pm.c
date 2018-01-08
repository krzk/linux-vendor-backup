// SPDX-License-Identifier: GPL-2.0
//
// based on arch/arm/mach-exynos/suspend.c
// Copyright (c) 2018 Samsung Electronics Co., Ltd.
//
// Exynos Power Management support driver

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_fdt.h>
#include <linux/kernel.h>
#include <linux/regulator/machine.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <linux/psci.h>

#include <asm/cpuidle.h>
#include <asm/io.h>
#include <asm/suspend.h>

#include <linux/soc/samsung/exynos-pm.h>
#include <linux/soc/samsung/exynos-pmu.h>

/*
 * The struct exynos_pm_data contains the callbacks of
 * both struct platform_suspend_ops and syscore_ops.
 * This structure is listed according to the call order,
 * because the callback call order for the two structures is mixed.
 */
struct exynos_pm_data {
	int (*prepare)(void);			/* for platform_suspend_ops */
	int (*suspend)(void);			/* for syscore_ops */
	int (*enter)(suspend_state_t state);	/* for platform_suspend_ops */
	void (*resume)(void);			/* for syscore_ops */
	void (*finish)(void);			/* for platform_suspend_ops */
};

static struct platform_suspend_ops exynos_pm_suspend_ops;
static struct syscore_ops exynos_pm_syscore_ops;
static const struct exynos_pm_data *pm_data  __ro_after_init;

void __iomem *sysram_base_addr __ro_after_init;
void __iomem *sysram_ns_base_addr __ro_after_init;

static int exynos_pm_prepare(void)
{
	int ret;

	/*
	 * REVISIT: It would be better if struct platform_suspend_ops
	 * .prepare handler get the suspend_state_t as a parameter to
	 * avoid hard-coding the suspend to mem state. It's safe to do
	 * it now only because the suspend_valid_only_mem function is
	 * used as the .valid callback used to check if a given state
	 * is supported by the platform anyways.
	 */
	ret = regulator_suspend_prepare(PM_SUSPEND_MEM);
	if (ret) {
		pr_err("Failed to prepare regulators for suspend (%d)\n", ret);
		return ret;
	}

	if (pm_data->prepare) {
		ret = pm_data->prepare();
		if (ret) {
			pr_err("Failed to prepare for suspend (%d)\n", ret);
			return ret;
		}
	}

	return 0;
}

static int exynos_pm_suspend(void)
{
	if (pm_data->suspend)
		return pm_data->suspend();

	return 0;
}

static int exynos_pm_enter(suspend_state_t state)
{
	int ret;

	exynos_sys_powerdown_conf(SYS_SLEEP);

	ret = pm_data->enter(state);
	if (ret) {
		pr_err("Failed to enter sleep\n");
		return ret;
	}

	return 0;
}

static void exynos_pm_resume(void)
{
	if (pm_data->resume)
		pm_data->resume();
}

static void exynos_pm_finish(void)
{
	int ret;

	ret = regulator_suspend_finish();
	if (ret)
		pr_warn("Failed to resume regulators from suspend (%d)\n", ret);

	if (pm_data->finish)
		pm_data->finish();
}

/*
 * Split the data between ARM architectures because it is relatively big
 * and useless on other arch.
 */
#ifdef CONFIG_EXYNOS_PMU_ARM_DRIVERS
#define exynos_pm_data_arm_ptr(data)	(&data)
#else
#define exynos_pm_data_arm_ptr(data)	NULL
#endif

static int exynos5433_pm_suspend(unsigned long unused)
{
	/*
	 * Exynos5433 uses PSCI v0.1 which provides the only one
	 * entry point (psci_ops.cpu_suspend) for both cpuidle and
	 * suspend-to-RAM. Also, PSCI v0.1 needs the specific 'power_state'
	 * parameter for the suspend mode. In order to enter suspend mode,
	 * Exynos5433 calls the 'psci_ops.cpu_suspend' with '0x3010000'
	 * power_state parameter.
	 *
	 * '0x3010000' means that both cluster and system are going to enter
	 * the power-down state as following:
	 * - [25:24] 0x3 : Indicate the cluster and system.
	 * - [16]    0x1 : Indicate power-down state.
	 */
	return psci_ops.cpu_suspend(0x3010000, __pa_symbol(cpu_resume));
}

static int exynos5433_pm_suspend_enter(suspend_state_t state)
{
	if (!sysram_ns_base_addr)
		return -EINVAL;

	__raw_writel(virt_to_phys(cpu_resume), sysram_ns_base_addr + 0x8);
	__raw_writel(EXYNOS_SLEEP_MAGIC, sysram_ns_base_addr + 0xc);

	return cpu_suspend(0, exynos5433_pm_suspend);
}

const struct exynos_pm_data exynos5433_pm_data = {
	.enter		= exynos5433_pm_suspend_enter,
};

static const struct of_device_id exynos_pm_of_device_ids[] = {
	{
		.compatible = "samsung,exynos5433-pmu",
		.data = exynos_pm_data_arm_ptr(exynos5433_pm_data),
	},
	{ /*sentinel*/ },
};

void __init exynos_sysram_init(void)
{
	struct device_node *np;

	for_each_compatible_node(np, NULL, "samsung,exynos4210-sysram") {
		if (!of_device_is_available(np))
			continue;
		sysram_base_addr = of_iomap(np, 0);
		break;
	}

	for_each_compatible_node(np, NULL, "samsung,exynos4210-sysram-ns") {
		if (!of_device_is_available(np))
			continue;
		sysram_ns_base_addr = of_iomap(np, 0);
		break;
	}
}

static int __init exynos_pm_init(void)
{
	const struct of_device_id *match;
	struct device_node *np;

	np = of_find_matching_node_and_match(NULL,
					exynos_pm_of_device_ids, &match);
	if (!np) {
		pr_err("Failed to find PMU node for Exynos Power-Management\n");
		return -ENODEV;
	}
	pm_data = (const struct exynos_pm_data *) match->data;

	exynos_sysram_init();

	exynos_pm_suspend_ops.valid	= suspend_valid_only_mem;
	exynos_pm_suspend_ops.prepare	= exynos_pm_prepare;
	exynos_pm_syscore_ops.suspend	= exynos_pm_suspend;
	exynos_pm_suspend_ops.enter	= exynos_pm_enter;
	exynos_pm_syscore_ops.resume	= exynos_pm_resume;
	exynos_pm_suspend_ops.finish	= exynos_pm_finish;

	register_syscore_ops(&exynos_pm_syscore_ops);
	suspend_set_ops(&exynos_pm_suspend_ops);

	return 0;
}
postcore_initcall(exynos_pm_init);
