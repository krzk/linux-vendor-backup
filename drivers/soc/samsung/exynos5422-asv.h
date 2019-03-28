// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Samsung Exynos 5422 SoC Adaptive Supply Voltage support
 */

#ifndef __EXYNOS5422_ASV_H
#define __EXYNOS5422_ASV_H

#include <linux/errno.h>

struct exynos_asv;

#ifdef CONFIG_EXYNOS_ASV_ARM
int exynos5422_asv_init(struct exynos_asv *asv);
#else
static inline int exynos5422_asv_init(struct exynos_asv *asv)
{
	return -ENOTSUPP;
}
#endif

#endif /* __EXYNOS5422_ASV_H */
