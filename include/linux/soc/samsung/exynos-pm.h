// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 Samsung Electronics Co., Ltd.
//
// Header for Exynos Power-Management support driver

#ifndef __LINUX_SOC_EXYNOS_PM_H
#define __LINUX_SOC_EXYNOS_PM_H

/*
 * Magic values for bootloader indicating chosen low power mode.
 * See also Documentation/arm/Samsung/Bootloader-interface.txt
 */
#define EXYNOS_SLEEP_MAGIC	0x00000bad

extern void __iomem *sysram_base_addr;
extern void __iomem *sysram_ns_base_addr;

extern void exynos_sysram_init(void);

#endif /* __LINUX_SOC_EXYNOS_PMU_H */
