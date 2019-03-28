/* linux/arch/arm/mach-exynos/include/mach/asv-exynos5433.h
*
* Copyright (c) 2014 Samsung Electronics Co., Ltd.
*              http://www.samsung.com/
*
* EXYNOS5433 - Adaptive Support Voltage Header file
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/

#ifndef EXYNOS5433_ASV_H__
#define EXYNOS5433_ASV_H__

#include <linux/errno.h>

struct exynos_asv;

#ifdef CONFIG_EXYNOS_ASV_ARM64
int exynos5433_asv_init(struct exynos_asv *asv);
#else
static inline int exynos5433_asv_init(struct exynos_asv *asv)
{
	return -ENOTSUPP;
}
#endif

enum sysc_dvfs_level {
	SYSC_DVFS_L0 = 0,
	SYSC_DVFS_L1,
	SYSC_DVFS_L2,
	SYSC_DVFS_L3,
	SYSC_DVFS_L4,
	SYSC_DVFS_L5,
	SYSC_DVFS_L6,
	SYSC_DVFS_L7,
	SYSC_DVFS_L8,
	SYSC_DVFS_L9,
	SYSC_DVFS_L10,
	SYSC_DVFS_L11,
	SYSC_DVFS_L12,
	SYSC_DVFS_L13,
	SYSC_DVFS_L14,
	SYSC_DVFS_L15,
	SYSC_DVFS_L16,
	SYSC_DVFS_L17,
	SYSC_DVFS_L18,
	SYSC_DVFS_L19,
	SYSC_DVFS_L20,
	SYSC_DVFS_L21,
	SYSC_DVFS_L22,
	SYSC_DVFS_L23,
	SYSC_DVFS_L24,
};

#define SYSC_DVFS_END_LVL_EGL		SYSC_DVFS_L24
#define SYSC_DVFS_END_LVL_KFC		SYSC_DVFS_L19
#define SYSC_DVFS_END_LVL_G3D		SYSC_DVFS_L10
#define SYSC_DVFS_END_LVL_MIF		SYSC_DVFS_L10
#define SYSC_DVFS_END_LVL_INT		SYSC_DVFS_L7
#define SYSC_DVFS_END_LVL_DISP		SYSC_DVFS_L4
#define SYSC_DVFS_END_LVL_CAM		SYSC_DVFS_L11
#define MAX_ASV_GROUP			16

enum sysc_dvfs_sel {
	SYSC_DVFS_EGL,
	SYSC_DVFS_KFC,
	SYSC_DVFS_INT,
	SYSC_DVFS_MIF,
	SYSC_DVFS_G3D,
	SYSC_DVFS_CAM,
	SYSC_DVFS_NUM
};

#endif /* EXYNOS5433_ASV_H__ */
