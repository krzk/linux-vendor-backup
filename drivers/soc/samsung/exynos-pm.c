/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Exynos Power Management support driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/syscore_ops.h>
#include <linux/soc/samsung/exynos-pmu.h>
#include <linux/soc/samsung/exynos-pm.h>

static struct exynos_pm_ops *pm_ops;

static int exynos_suspend_prepare(void)
{
	if (pm_ops->prepare)
		return pm_ops->prepare();

	return 0;
}

static int exynos_suspend_enter(suspend_state_t state)
{
	int ret;

	exynos_sys_powerdown_conf(SYS_SLEEP);

	ret = pm_ops->suspend();
	if (ret) {
		pr_err("Failed to enter sleep\n");
		return ret;
	}

	return 0;
}

static struct platform_suspend_ops exynos_suspend_ops = {
	.valid = suspend_valid_only_mem,
	.prepare = exynos_suspend_prepare,
	.enter	= exynos_suspend_enter,
};

static int exynos_pm_syscore_suspend(void)
{
	if (pm_ops->prepare_late)
		return pm_ops->prepare_late();

	return 0;
}

static void exynos_pm_syscore_resume(void)
{
	exynos_sys_powerup_conf(SYS_SLEEP);

	if (pm_ops->finish)
		pm_ops->finish();
}

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend = exynos_pm_syscore_suspend,
	.resume = exynos_pm_syscore_resume,
};

void __init exynos_pm_init(struct exynos_pm_ops *ops)
{
	/* Suspend callback is mandatory */
	if (!ops || !ops->suspend)
		return;

	pm_ops = ops;
	register_syscore_ops(&exynos_pm_syscore_ops);
	suspend_set_ops(&exynos_suspend_ops);
}
