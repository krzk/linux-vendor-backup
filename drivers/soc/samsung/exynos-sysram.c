// SPDX-License-Identifier: GPL-2.0
//
// based on arch/arm/mach-exynos/suspend.c
// Copyright (c) 2018 Samsung Electronics Co., Ltd.
//
// Exynos Power Management support driver

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/kernel.h>

#include <asm/io.h>

#include <linux/soc/samsung/exynos-pm.h>

void __iomem *sysram_base_addr __ro_after_init;
void __iomem *sysram_ns_base_addr __ro_after_init;

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
