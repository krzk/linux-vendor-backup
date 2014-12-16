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

#ifndef __EXYNOS_PMU_H
#define __EXYNOS_PMU_H

#if defined(CONFIG_EXYNOS_EXTRA_PWR_MODES)
enum sys_powerdown {
	SYS_AFTR,
	SYS_STOP,
	SYS_DSTOP,
	SYS_DSTOP_PSR,
	SYS_LPD,
	SYS_LPA,
	SYS_ALPA,
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};
#else
enum sys_powerdown {
	SYS_AFTR,
	SYS_LPA,
	SYS_SLEEP,
	NUM_SYS_POWERDOWN,
};
#endif

extern void exynos_sys_powerdown_conf(enum sys_powerdown mode);
extern void exynos_sys_powerup_conf(enum sys_powerdown mode);

#endif /* __EXYNOS_PMU_H */
