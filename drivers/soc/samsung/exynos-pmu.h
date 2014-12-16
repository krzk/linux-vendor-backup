/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header for EXYNOS PMU Driver support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_PMU_PRIV_H
#define __EXYNOS_PMU_PRIV_H

#include <linux/io.h>

#define PMU_TABLE_END	(-1U)

extern void __iomem *pmu_base_addr;

struct exynos_pmu_conf {
	unsigned int offset;
	u8 val[NUM_SYS_POWERDOWN];
};

struct exynos_pmu_conf_extra {
	u32 offset;
	u32 val[NUM_SYS_POWERDOWN];
};

struct exynos_pmu_data {
	const struct exynos_pmu_conf *pmu_config;
	const struct exynos_pmu_conf_extra *pmu_config_extra;

	void (*pmu_init)(void);
	void (*powerdown_conf)(enum sys_powerdown);
	void (*powerdown_conf_extra)(enum sys_powerdown);
	void (*powerup_conf)(enum sys_powerdown);
};

static inline void pmu_raw_writel(u32 val, u32 offset)
{
	writel_relaxed(val, pmu_base_addr + offset);
}

static inline u32 pmu_raw_readl(u32 offset)
{
	return readl_relaxed(pmu_base_addr + offset);
}
#endif /* __EXYNOS_PMU_PRIV_H */
