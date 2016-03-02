/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <asm/irq.h>

#include <plat/devs.h>

#include <mach/irqs.h>
#include <mach/map.h>
#include <mach/tmu.h>

static struct resource tmu_resource[] = {
	[0] = DEFINE_RES_MEM(S5P_PA_TMU, SZ_64K),
	[1] = DEFINE_RES_IRQ(EXYNOS5_IRQ_TMU),
};

struct platform_device exynos_device_tmu = {
	.name	= "exynos_tmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tmu_resource),
	.resource	= tmu_resource,
};

static struct resource tmu_resource_4270[] = {
	[0] = DEFINE_RES_MEM(EXYNOS4_PA_TMU, SZ_64K),
	[1] = DEFINE_RES_IRQ(EXYNOS4_IRQ_TMU),
};

struct platform_device exynos4270_device_tmu = {
	.name	= "exynos4-tmu",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(tmu_resource_4270),
	.resource	= tmu_resource_4270,
};

int exynos_tmu_get_irqno(int num)
{
#if defined(CONFIG_SOC_EXYNOS4270)
	return platform_get_irq(&exynos4270_device_tmu, num);
#else
	return platform_get_irq(&exynos_device_tmu, num);
#endif
}

struct tmu_info *exynos_tmu_get_platdata(void)
{
#if defined(CONFIG_SOC_EXYNOS4270)
	return platform_get_drvdata(&exynos4270_device_tmu);
#else
	return platform_get_drvdata(&exynos_device_tmu);
#endif
}

void __init exynos_tmu_set_platdata(struct tmu_data *pd)
{
	struct tmu_data *npd;

	if (pd == NULL) {
		pr_err("%s: no platform data supplied\n", __func__);
		return;
	}

	npd = kmemdup(pd, sizeof(struct tmu_data), GFP_KERNEL);
	if (npd == NULL)
		pr_err("%s: no memory for platform data\n", __func__);

	exynos4270_device_tmu.dev.platform_data = npd;
}
