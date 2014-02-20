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

#define LOT_ID_LEN  5

struct exynos_asv_common
{
  char lot_name[LOT_ID_LEN];
  struct asv_info *asv_list;
  unsigned int nr_mem;
  void __iomem *base;
};

extern int exynos5250_asv_init (struct exynos_asv_common *exynos_info);
extern int exynos5410_asv_init (struct exynos_asv_common *exynos_info);
#endif /* __EXYNOS_ASV_D_H */
