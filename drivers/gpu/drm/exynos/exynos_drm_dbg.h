/*
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef _EXYNOS_DRM_DBG_H_
#define _EXYNOS_DRM_DBG_H_

#ifdef CONFIG_DRM_EXYNOS_DBG
extern int exynos_drm_dbg_register(struct device *dev,
		struct device_attribute *dev_attr,
		int size, void __iomem *regs);
extern int exynos_drm_dbg_unregister(struct device *dev,
		struct device_attribute *dev_attr);
#else
static inline int exynos_drm_dbg_register(struct device *dev,
		struct device_attribute *dev_attr,
		int size, void __iomem *regs)
{
	return -ENODEV;
}

static inline int exynos_drm_dbg_unregister(struct device *dev,
		struct device_attribute *dev_attr)
{
	return -ENODEV;
}
#endif

#endif /* _EXYNOS_DRM_DBG_H_ */
