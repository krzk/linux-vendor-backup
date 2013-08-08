/* exynos_drm_iommu_init.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef _EXYNOS_DRM_IOMMU_INIT_H_
#define _EXYNOS_DRM_IOMMU_INIT_H_

#define EXYNOS_DRM_INITIAL_MAPPING_VAL	ERR_PTR(-EAGAIN)

#ifdef CONFIG_ARM_DMA_USE_IOMMU
int exynos_drm_iommu_register(struct platform_driver **drivers, int count);
int exynos_drm_iommu_unregister(void);
#else
static inline int exynos_drm_iommu_register(struct platform_driver **drivers, int count)
{
	return 0;
}
static inline int exynos_drm_iommu_unregister(void)
{
	return 0;
}
#endif

#endif
