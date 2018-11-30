/* drivers/gpu/arm/.../platform/gpu_utilization.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_utilization.c
 * DVFS
 */

#include <mali_kbase.h>

#include "mali_kbase_platform.h"
#include "gpu_control.h"
#include "gpu_dvfs_handler.h"
#include "gpu_ipa.h"
#ifdef CONFIG_MALI_SEC_HWCNT
#include "gpu_hwcnt_sec.h"
#endif

extern struct kbase_device *pkbdev;

/* MALI_SEC_INTEGRATION */
#ifdef CONFIG_MALI_DVFS
extern int gpu_pm_get_dvfs_utilisation(struct kbase_device *kbdev, int *, int *);
static void gpu_dvfs_update_utilization(struct kbase_device *kbdev)
{
	unsigned long flags;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);

#if defined(CONFIG_MALI_DVFS) && defined(CONFIG_CPU_THERMAL_IPA)
	if (platform->time_tick < platform->gpu_dvfs_time_interval) {
		platform->time_tick++;
		platform->time_busy += kbdev->pm.backend.metrics.time_busy;
		platform->time_idle += kbdev->pm.backend.metrics.time_idle;
	} else {
		platform->time_busy = kbdev->pm.backend.metrics.time_busy;
		platform->time_idle = kbdev->pm.backend.metrics.time_idle;
		platform->time_tick = 0;
	}
#endif /* CONFIG_MALI_DVFS && CONFIG_CPU_THERMAL_IPA */

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

	platform->env_data.utilization = gpu_pm_get_dvfs_utilisation(kbdev, 0, 0);

	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

#if defined(CONFIG_MALI_DVFS) && defined(CONFIG_CPU_THERMAL_IPA)
	gpu_ipa_dvfs_calc_norm_utilisation(kbdev);
#endif /* CONFIG_MALI_DVFS && CONFIG_CPU_THERMAL_IPA */
}
#endif /* CONFIG_MALI_DVFS */

#ifdef CONFIG_MALI_DVFS
int gpu_dvfs_reset_env_data(struct kbase_device *kbdev)
{
	unsigned long flags;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	DVFS_ASSERT(platform);
	/* reset gpu utilization value */
	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	kbdev->pm.backend.metrics.time_idle = kbdev->pm.backend.metrics.time_idle + kbdev->pm.backend.metrics.time_busy;
	kbdev->pm.backend.metrics.time_busy = 0;
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 0;
}

int gpu_dvfs_calculate_env_data(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	static int polling_period = 0;

	DVFS_ASSERT(platform);

	gpu_dvfs_update_utilization(kbdev);

	polling_period -= platform->polling_speed;
	if (polling_period > 0)
		return 0;

	if (platform->dvs_is_enabled == true)
		return 0;

#ifdef CONFIG_MALI_SEC_HWCNT
	if (kbdev->hwcnt.is_hwcnt_attach == true && kbdev->hwcnt.is_hwcnt_gpr_enable == false) {
		polling_period = platform->hwcnt_polling_speed;
		if (!gpu_control_is_power_on(kbdev))
			return 0;
		mutex_lock(&kbdev->hwcnt.mlock);
		if (platform->cur_clock >= platform->gpu_max_clock_limit || platform->hwcnt_profile == true) {
			if (kbdev->vendor_callbacks->hwcnt_update) {
				kbdev->vendor_callbacks->hwcnt_update(kbdev);
				dvfs_hwcnt_get_resource(kbdev);
				dvfs_hwcnt_utilization_equation(kbdev);
			}
		}
		mutex_unlock(&kbdev->hwcnt.mlock);
	}
#endif
	return 0;
}
#endif
