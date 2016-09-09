/* drivers/gpu/mali400/mali/platform/exynos4270/exynos4.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali400 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file exynos4.c
 * Platform specific Mali driver functions for the exynos 4XXX based platforms
 */
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#include "mali_kernel_linux.h"
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>

#include <linux/pm_qos.h>
#include "mali_executor.h"

#if defined(CONFIG_MALI_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>
#endif

#include <linux/irq.h>
#include "exynos3_pmm.h"

static struct mali_gpu_device_data mali_gpu_data = {
	.shared_mem_size = 256 * 1024 * 1024, /* 256MB */
	.fb_start = 0x40000000,
	.fb_size = 0xb1000000,
	.control_interval = 1000, /* 1000ms */
#ifdef CONFIG_MALI_DVFS
	.get_clock_info = exynos3_get_clock_info,
	.get_freq = exynos3_get_freq,
	.set_freq = exynos3_set_freq,
#endif
};

int mali_platform_device_init(struct platform_device *device)
{
	int num_pp_cores = 2;
	int err = -1;

	/* After kernel 3.15 device tree will default set dev
	 * related parameters in of_platform_device_create_pdata.
	 * But kernel changes from version to version,
	 * For example 3.10 didn't include device->dev.dma_mask parameter setting,
	 * if we didn't include here will cause dma_mapping error,
	 * but in kernel 3.15 it include  device->dev.dma_mask parameter setting,
	 * so it's better to set must need paramter by DDK itself.
	 */
	if (!device->dev.dma_mask)
		device->dev.dma_mask = &device->dev.coherent_dma_mask;
#ifndef CONFIG_ARM64
	device->dev.archdata.dma_ops = &arm_dma_ops;
#endif

	err = platform_device_add_data(device, &mali_gpu_data, sizeof(mali_gpu_data));

	if (0 == err) {
		err = mali_platform_init(device);
		if (err)
			return err;

#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(device->dev), 1000);
		pm_runtime_use_autosuspend(&(device->dev));
#endif
		pm_runtime_enable(&(device->dev));
#endif
		MALI_DEBUG_ASSERT(0 < num_pp_cores);
	}

#if defined(CONFIG_MALI_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
	/* Get thermal zone */
	gpu_tz = thermal_zone_get_zone_by_name("soc_thermal");
	if (IS_ERR(gpu_tz)) {
		MALI_DEBUG_PRINT(2, ("Error getting gpu thermal zone (%ld), not yet ready?\n",
				     PTR_ERR(gpu_tz)));
		gpu_tz = NULL;

		err =  -EPROBE_DEFER;
	}
#endif

	return err;
}

int mali_platform_device_deinit(struct platform_device *device)
{
	MALI_IGNORE(device);

	MALI_DEBUG_PRINT(4, ("mali_platform_device_deinit() called\n"));

	mali_platform_deinit(device);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&(device->dev));
#endif

	return 0;
}
