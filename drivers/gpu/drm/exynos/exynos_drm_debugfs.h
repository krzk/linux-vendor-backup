/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * Author: YoungJun Cho <yj44.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundationr
 */

#ifndef _EXYNOS_DRM_DEBUGFS_H_
#define _EXYNOS_DRM_DEBUGFS_H_

#if defined(CONFIG_DEBUG_FS)
extern int exynos_drm_debugfs_init(struct drm_minor *minor);
extern void exynos_drm_debugfs_cleanup(struct drm_minor *minor);
#else
static inline int exynos_drm_debugfs_init(struct drm_minor *minor)
{
	return 0;
}

static inline void exynos_drm_debugfs_cleanup(struct drm_minor *minor)
{
}
#endif

#endif	/* _EXYNOS_DRM_DEBUGFS_H_ */
