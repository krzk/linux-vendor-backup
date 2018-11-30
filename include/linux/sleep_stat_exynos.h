/*
 *  include/linux/sleep_stat_exynos.h
 *
 *  Copyright (C) 2018 Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SLEEP_STAT_EXYNOS_H
#define __SLEEP_STAT_EXYNOS_H

 struct sleep_stat_exynos {
 	int fail;
	int failed_freeze;
	int failed_prepare;
	int failed_suspend;
	int failed_suspend_late;
	int failed_suspend_noirq;
	int suspend_success;
	int acpm_sleep_early_wakeup;
	int acpm_sleep_soc_down;
	int acpm_sleep_mif_down;
	int acpm_sicd_early_wakeup;
	int acpm_sicd_soc_down;
	int acpm_sicd_mif_down;
 };

#ifdef CONFIG_SLEEP_STAT_EXYNOS
extern int sleep_stat_exynos_get_stat(int type,
 					struct sleep_stat_exynos *sleep_stat);
#else
static inline int sleep_stat_exynos_get_stat(int type,
 					struct sleep_stat_exynos *sleep_stat) {	return 0;}
#endif

#endif /* __SLEEP_STAT_EXYNOS_H */

