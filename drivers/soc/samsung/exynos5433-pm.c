/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Exynos5433 suspend-to-ram support driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/of_address.h>
#include <linux/soc/samsung/exynos-pm.h>
#include <asm/io.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>

void __iomem *sysram_ns_base;

static int exynos5433_pm_suspend(void)
{
	__raw_writel(virt_to_phys(cpu_resume), sysram_ns_base + 0x8);
	__raw_writel(EXYNOS_CHECK_SLEEP, sysram_ns_base + 0xC);

	return arm_cpuidle_suspend(0);
}

static struct exynos_pm_ops exynos5433_pm_ops = {
	.suspend = exynos5433_pm_suspend,
};

static int __init exynos5433_pm_init(void)
{
	struct device_node *of_node;

	pr_info("Exynos suspend support initialize\n");

	of_node = of_find_compatible_node(0, 0, "samsung,exynos5433-sysram-ns");
	if (!of_node)
		return -ENODEV;

	sysram_ns_base = of_iomap(of_node, 0);
	exynos_pm_init(&exynos5433_pm_ops);

	return 0;
}
arch_initcall(exynos5433_pm_init);
