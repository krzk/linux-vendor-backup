/*
 * Exynos - Adaptive Supply Voltage Driver Header File
 *
 * copyright (c) 2013 samsung electronics co., ltd.
 *		http://www.samsung.com/
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license version 2 as
 * published by the free software foundation.
*/

#ifndef __EXYNOS_ASV_D_H
#define __EXYNOS_ASV_D_H __FILE__

struct exynos_asv_common {
	struct asv_info *asv_list;
	unsigned int nr_mem;
	void __iomem *base;
};

#endif	/* __EXYNOS_ASV_D_H */
