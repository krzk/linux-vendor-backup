/*
 * Samsung's EXYNOS3 flattened device tree enabled machine
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 * Copyright (c) 2010-2011 Linaro Ltd.
 *		www.linaro.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/serial_core.h>
#include <linux/clocksource.h>

#include <asm/mach/arch.h>

#include "common.h"

static void __init exynos3_dt_map_io(void)
{
	exynos_init_io(NULL, 0);
}

static void __init exynos3_dt_machine_init(void)
{
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

static char const *exynos3_dt_compat[] __initdata = {
	"samsung,exynos3250",
	NULL
};

DT_MACHINE_START(EXYNOS3250_DT, "Samsung Exynos3 (Flattened Device Tree)")
	.smp		= smp_ops(exynos_smp_ops),
	.init_irq	= exynos3_init_irq,
	.map_io		= exynos3_dt_map_io,
	.init_early	= exynos_firmware_init,
	.init_machine	= exynos3_dt_machine_init,
	.init_late	= exynos_init_late,
	.init_time	= exynos_init_time,
	.dt_compat	= exynos3_dt_compat,
	.restart        = exynos4_restart,
MACHINE_END
