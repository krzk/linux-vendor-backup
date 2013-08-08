/* exynos_drm_iommu_init.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drmP.h>
#include <drm/exynos_drm.h>

#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/kref.h>

#include <asm/dma-iommu.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_iommu_init.h"

static struct platform_driver **exynos_drm_subdrivers;
static int exynos_drm_subdrivers_count;

static int exynos_iommu_hook_driver(struct notifier_block *nb,
					     unsigned long val, void *p)
{
	struct device *dev = p;
	int i;

	switch (val) {
	case BUS_NOTIFY_BIND_DRIVER:
		for (i=0; i < exynos_drm_subdrivers_count; i++) {
			if (dev->driver == &exynos_drm_subdrivers[i]->driver) {
				dev->archdata.mapping = EXYNOS_DRM_INITIAL_MAPPING_VAL;
				break;
			}
		}
		break;

	case BUS_NOTIFY_UNBOUND_DRIVER:
	case BUS_NOTIFY_BIND_FAILED:
		for (i=0; i < exynos_drm_subdrivers_count; i++) {
			if (dev->driver == &exynos_drm_subdrivers[i]->driver) {
				dev->archdata.mapping = NULL;
				break;
			}
		}
	}
	return 0;
}

static struct notifier_block exynos_drm_iommu_notifier = {
	.notifier_call = &exynos_iommu_hook_driver,
	.priority = 100,
};

int exynos_drm_iommu_register(struct platform_driver **drivers, int count)
{
	exynos_drm_subdrivers = drivers;
	exynos_drm_subdrivers_count = count;
	return bus_register_notifier(&platform_bus_type, &exynos_drm_iommu_notifier);
}

int exynos_drm_iommu_unregister(void)
{
	return bus_register_notifier(&platform_bus_type, &exynos_drm_iommu_notifier);
}
