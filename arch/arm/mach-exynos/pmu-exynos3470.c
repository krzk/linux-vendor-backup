/* linux/arch/arm/mach-exynos/pmu.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/gpio.h>

#include <plat/gpio-cfg.h>

#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pmu.h>
#include <asm/cputype.h>

static struct exynos_pmu_conf *exynos_pmu_config;

static struct exynos_pmu_conf exynos4210_pmu_config[] = {
	/* { .reg = address, .val = { AFTR, LPA, SLEEP } */
	{ EXYNOS4_ARM_CORE0_LOWPWR,		{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE0,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL0,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_ARM_CORE1_LOWPWR,		{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE1,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL1,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_ARM_COMMON_LOWPWR,		{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_L2_0_LOWPWR,			{ 0x2, 0x2, 0x3 } },
	{ EXYNOS4_L2_1_LOWPWR,			{ 0x2, 0x2, 0x3 } },
	{ EXYNOS4_CMU_ACLKSTOP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_SCLKSTOP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_APLL_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_MPLL_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_VPLL_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_EPLL_SYSCLK_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_GPS_ALIVE_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_GPSALIVE_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_CAM_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_TV_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MFC_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_G3D_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_LCD0_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4210_CMU_CLKSTOP_LCD1_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MAUDIO_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_GPS_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_CAM_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_TV_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_MFC_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_G3D_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_LCD0_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4210_CMU_RESET_LCD1_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_MAUDIO_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_GPS_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_TOP_BUS_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_TOP_RETENTION_LOWPWR,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4_TOP_PWR_LOWPWR,		{ 0x3, 0x0, 0x3 } },
	{ EXYNOS4_LOGIC_RESET_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_ONENAND_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4210_MODIMIF_MEM_LOWPWR,	{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_G2D_ACP_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_USBOTG_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_HSMMC_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_CSSYS_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_SECSS_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4210_PCIE_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4210_SATA_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_DRAM_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MAUDIO_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_GPIO_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_UART_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCA_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCB_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_EBIA_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_EBIB_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_ISOLATION_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_ALV_SEL_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_XUSBXTI_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_XXTI_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_EXT_REGULATOR_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_GPIO_MODE_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_GPIO_MODE_MAUDIO_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CAM_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_TV_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MFC_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_G3D_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_LCD0_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4210_LCD1_LOWPWR,		{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MAUDIO_LOWPWR,		{ 0x7, 0x7, 0x0 } },
	{ EXYNOS4_GPS_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_GPS_ALIVE_LOWPWR,		{ 0x7, 0x0, 0x0 } },
	{ PMU_TABLE_END,},
};

static struct exynos_pmu_conf exynos4412_pmu_config[] = {
	{ EXYNOS4_ARM_CORE0_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_ARM_CORE1_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_ARM_CORE2_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_ARM_CORE3_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4X12_ISP_ARM_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_DIS_IRQ_ISP_ARM_LOCAL_LOWPWR,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4X12_DIS_IRQ_ISP_ARM_CENTRAL_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_ARM_COMMON_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_L2_0_LOWPWR,				{ 0x0, 0x0, 0x3 } },
	{ EXYNOS4X12_ARM_L2_0_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_L2_1_LOWPWR,				{ 0x0, 0x0, 0x3 } },
	{ EXYNOS4X12_ARM_L2_1_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_CMU_ACLKSTOP_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_SCLKSTOP_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_CMU_ACLKSTOP_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_SCLKSTOP_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_RESET_COREBLK_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_APLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_MPLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_VPLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_EPLL_SYSCLK_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_MPLLUSER_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_GPS_ALIVE_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_GPSALIVE_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_CAM_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_TV_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MFC_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_G3D_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_LCD0_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_CLKSTOP_ISP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MAUDIO_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_GPS_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_CAM_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_TV_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_MFC_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_G3D_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_LCD0_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_RESET_ISP_LOWPWR,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_MAUDIO_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_GPS_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_TOP_BUS_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_TOP_RETENTION_LOWPWR,			{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4_TOP_PWR_LOWPWR,			{ 0x3, 0x0, 0x3 } },
	{ EXYNOS4X12_TOP_BUS_COREBLK_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_TOP_RETENTION_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4X12_TOP_PWR_COREBLK_LOWPWR,		{ 0x3, 0x0, 0x3 } },
	{ EXYNOS4_LOGIC_RESET_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_OSCCLK_GATE_LOWPWR,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4X12_LOGIC_RESET_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_OSCCLK_GATE_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4X12_HSI_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_HSI_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_G2D_ACP_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_G2D_ACP_MEM_OPTION,		{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_USBOTG_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_USBOTG_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_HSMMC_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_HSMMC_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_CSSYS_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_CSSYS_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_SECSS_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_SECSS_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4X12_ROTATOR_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_ROTATOR_MEM_OPTION,		{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_DRAM_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_GPIO_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_UART_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCA_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCB_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_EBIA_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_EBIB_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_PAD_RETENTION_GPIO_COREBLK_LOWPWR, { 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_ISOLATION_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_PAD_ISOLATION_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_ALV_SEL_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_XUSBXTI_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_XXTI_LOWPWR,				{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_EXT_REGULATOR_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_GPIO_MODE_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_GPIO_MODE_COREBLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_GPIO_MODE_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_TOP_ASB_RESET_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_TOP_ASB_ISOLATION_LOWPWR,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4_CAM_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_TV_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MFC_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_G3D_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_LCD0_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4X12_ISP_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MAUDIO_LOWPWR,			{ 0x7, 0x7, 0x0 } },
	{ EXYNOS4_GPS_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_GPS_ALIVE_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_SYSCLK_ISP_LOWPWR,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_SYSCLK_GPS_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ PMU_TABLE_END,},
};

static struct exynos_pmu_conf exynos4212_pmu_config[] = {
	{ EXYNOS4_ARM_CORE0_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE0,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL0,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_ARM_CORE1_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE1,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL1,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4X12_ISP_ARM_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_DIS_IRQ_ISP_ARM_LOCAL_LOWPWR,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4X12_DIS_IRQ_ISP_ARM_CENTRAL_LOWPWR,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_ARM_COMMON_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_L2_0_LOWPWR,				{ 0x0, 0x0, 0x3 } },
	/* XXX_OPTION register should be set other field */
	{ EXYNOS4X12_ARM_L2_0_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_L2_1_LOWPWR,				{ 0x0, 0x0, 0x3 } },
	{ EXYNOS4X12_ARM_L2_1_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_CMU_ACLKSTOP_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_SCLKSTOP_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_DRAM_FREQ_DOWN_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_DDRPHY_DLLOFF_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_LPDDR_PHY_DLL_LOCK_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_CMU_ACLKSTOP_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_SCLKSTOP_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_RESET_COREBLK_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_APLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_MPLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_VPLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_EPLL_SYSCLK_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_MPLLUSER_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_GPS_ALIVE_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_GPSALIVE_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_CAM_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_TV_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MFC_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_G3D_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_LCD0_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_CLKSTOP_ISP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MAUDIO_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_GPS_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_CAM_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_TV_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_MFC_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_G3D_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_LCD0_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_RESET_ISP_LOWPWR,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CMU_RESET_GPS_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_TOP_BUS_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_TOP_RETENTION_LOWPWR,			{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4_TOP_PWR_LOWPWR,			{ 0x3, 0x0, 0x3 } },
	{ EXYNOS4X12_TOP_BUS_COREBLK_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_TOP_RETENTION_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4X12_TOP_PWR_COREBLK_LOWPWR,		{ 0x3, 0x0, 0x3 } },
	{ EXYNOS4_LOGIC_RESET_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_OSCCLK_GATE_LOWPWR,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4X12_LOGIC_RESET_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_OSCCLK_GATE_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4_ONENAND_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_ONENAND_MEM_OPTION,		{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4X12_HSI_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_HSI_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_G2D_ACP_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_G2D_ACP_MEM_OPTION,		{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_USBOTG_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_USBOTG_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_HSMMC_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_HSMMC_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_CSSYS_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_CSSYS_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_SECSS_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_SECSS_MEM_OPTION,			{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4X12_ROTATOR_MEM_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_ROTATOR_MEM_OPTION,		{ 0x10, 0x10, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_DRAM_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_GPIO_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_UART_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCA_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCB_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_EBIA_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_EBIB_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_PAD_RETENTION_GPIO_COREBLK_LOWPWR, { 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_ISOLATION_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_PAD_ISOLATION_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_ALV_SEL_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_XUSBXTI_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_XXTI_LOWPWR,				{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_EXT_REGULATOR_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_GPIO_MODE_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_GPIO_MODE_COREBLK_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_GPIO_MODE_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_TOP_ASB_RESET_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_TOP_ASB_ISOLATION_LOWPWR,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4_CAM_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_TV_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MFC_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_G3D_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_LCD0_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4X12_ISP_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MAUDIO_LOWPWR,			{ 0x7, 0x7, 0x0 } },
	{ EXYNOS4_GPS_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_GPS_ALIVE_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_SYSCLK_ISP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_SYSCLK_GPS_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ PMU_TABLE_END,},
};

static struct exynos_pmu_conf exynos4270_pmu_config[] = {
	{ EXYNOS4_ARM_CORE0_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE0,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL0,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_ARM_CORE1_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE1,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL1,			{ 0x0, 0x0, 0x0 } },
#ifdef CONFIG_SOC_EXYNOS3470
	{ EXYNOS4_ARM_CORE2_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE2,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL2,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_ARM_CORE3_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_DIS_IRQ_CORE3,			{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_DIS_IRQ_CENTRAL3,			{ 0x0, 0x0, 0x0 } },
#endif
	{ EXYNOS4X12_ISP_ARM_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_DIS_IRQ_ISP_ARM_LOCAL_LOWPWR,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4X12_DIS_IRQ_ISP_ARM_CENTRAL_LOWPWR,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS4_ARM_COMMON_LOWPWR,			{ 0x0, 0x0, 0x2 } },
	{ EXYNOS4_L2_0_LOWPWR,				{ 0x0, 0x0, 0x3 } },
	{ EXYNOS4X12_ARM_L2_0_OPTION,			{ 0x1, 0x1, 0x0 } },
#ifdef CONFIG_SOC_EXYNOS4270
	{ EXYNOS4_L2_1_LOWPWR,				{ 0x0, 0x0, 0x3 } },
	{ EXYNOS4X12_ARM_L2_1_OPTION,			{ 0x1, 0x1, 0x0 } },
#endif
	{ EXYNOS4_CMU_ACLKSTOP_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_SCLKSTOP_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_DRAM_FREQ_DOWN_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_DDRPHY_DLLOFF_LOWPWR,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS4X12_LPDDR_PHY_DLL_LOCK_LOWPWR,		{ 0x1, 0x0, 0x1 } }, /* LPM3 */
	{ EXYNOS4X12_CMU_ACLKSTOP_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_SCLKSTOP_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_RESET_COREBLK_LOWPWR,		{ 0x1, 0x1, 0x0 } }, /* LPM3 */
	{ EXYNOS4_APLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_MPLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4270_BPLL_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* 4270 */
	{ EXYNOS4_VPLL_SYSCLK_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_EPLL_SYSCLK_LOWPWR,			{ 0x1, 0x1, 0x0 } },
#ifdef CONFIG_SOC_EXYNOS4270
	{ EXYNOS4270_MPHY_PLL_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* 4270 */
#endif
	{ EXYNOS4X12_MPLLUSER_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* LPM3 */
	{ EXYNOS4270_BPLLUSER_SYSCLK_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* LPM3 */
	{ EXYNOS4_CMU_CLKSTOP_CAM_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_TV_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MFC_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_G3D_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_LCD0_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_CLKSTOP_ISP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_CLKSTOP_MAUDIO_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_CAM_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_TV_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_MFC_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_G3D_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_LCD0_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4X12_CMU_RESET_ISP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_CMU_RESET_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_TOP_BUS_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4_TOP_RETENTION_LOWPWR,			{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4_TOP_PWR_LOWPWR,			{ 0x3, 0x0, 0x3 } },
	{ EXYNOS4X12_TOP_BUS_COREBLK_LOWPWR,		{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_TOP_RETENTION_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } }, /* LPM3 */
	{ EXYNOS4X12_TOP_PWR_COREBLK_LOWPWR,		{ 0x3, 0x0, 0x3 } }, /* LPM3 */
	{ EXYNOS4_LOGIC_RESET_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_OSCCLK_GATE_LOWPWR,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS4X12_LOGIC_RESET_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } }, /* LPM3 */
	{ EXYNOS4X12_OSCCLK_GATE_COREBLK_LOWPWR,	{ 0x1, 0x0, 0x1 } }, /* LPM3 */
	{ EXYNOS4_G2D_ACP_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_G2D_ACP_MEM_OPTION,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_USBOTG_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_USBOTG_MEM_OPTION,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_CSSYS_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_CSSYS_MEM_OPTION,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_SECSS_MEM_LOWPWR,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS4X12_SECSS_MEM_OPTION,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4270_IRAM_MEM_LOWPWR,			{ 0x3, 0x3, 0x0 } }, /* 4270 */
	{ EXYNOS4270_IRAM_MEM_OPTION,			{ 0x1, 0x1, 0x0 } }, /* 4270 */
	{ EXYNOS4_PAD_RETENTION_DRAM_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_GPIO_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_UART_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCA_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_MMCB_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4270_PAD_RETENTION_MMC2_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* 4270 */
	{ EXYNOS4270_PAD_RETENTION_MMC3_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* 4270 */
	{ EXYNOS4270_PAD_RETENTION_SPI_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* 4270 */
	{ EXYNOS4_PAD_RETENTION_EBIA_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_EBIB_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4270_PAD_RETENTION_JTAG_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* 4270 */
	{ EXYNOS4_PAD_RETENTION_ISOLATION_LOWPWR,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_PAD_RETENTION_ALV_SEL_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_XUSBXTI_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_XXTI_LOWPWR,				{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4_EXT_REGULATOR_LOWPWR,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4270_EXT_REGULATOR_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } }, /* LPM3 */
	{ EXYNOS4_GPIO_MODE_LOWPWR,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS4_GPIO_MODE_MAUDIO_LOWPWR,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS4X12_TOP_ASB_RESET_LOWPWR,		{ 0x1, 0x1, 0x0 } }, /* LPM3 */
	{ EXYNOS4X12_TOP_ASB_ISOLATION_LOWPWR,		{ 0x1, 0x0, 0x0 } }, /* LPM3 */
	{ EXYNOS4270_TOP_ASB_RESET_COREBLK_LOWPWR,	{ 0x1, 0x1, 0x0 } }, /* LPM3 */
	{ EXYNOS4270_TOP_ASB_ISOLATION_COREBLK_LOWPWR,  { 0x1, 0x0, 0x0 } }, /* LPM3 */
	{ EXYNOS4_CAM_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_TV_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MFC_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_G3D_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_LCD0_LOWPWR,				{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4X12_ISP_LOWPWR,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS4_MAUDIO_LOWPWR,			{ 0x7, 0x7, 0x0 } },
	{ EXYNOS4X12_CMU_SYSCLK_ISP_LOWPWR,		{ 0x1, 0x0, 0x0 } },
	{ PMU_TABLE_END,},
};

static struct exynos_pmu_conf exynos5250_pmu_config[] = {
	/* { .reg = address, .val = { AFTR, LPA, SLEEP } */
	{ EXYNOS5_ARM_CORE0_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS5_DIS_IRQ_ARM_CORE0_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_DIS_IRQ_ARM_CORE0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_ARM_CORE1_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS5_DIS_IRQ_ARM_CORE1_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_DIS_IRQ_ARM_CORE1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_FSYS_ARM_SYS_PWR_REG,				{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_DIS_IRQ_FSYS_ARM_CENTRAL_SYS_PWR_REG,		{ 0x1, 0x1, 0x1} },
	{ EXYNOS5_ISP_ARM_SYS_PWR_REG,				{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_DIS_IRQ_ISP_ARM_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_DIS_IRQ_ISP_ARM_CENTRAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_ARM_COMMON_SYS_PWR_REG,			{ 0x0, 0x0, 0x2} },
	{ EXYNOS5_ARM_L2_SYS_PWR_REG,				{ 0x0, 0x0, 0x3} },
	{ EXYNOS5_CMU_ACLKSTOP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_SCLKSTOP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_CMU_ACLKSTOP_SYSMEM_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_SCLKSTOP_SYSMEM_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_RESET_SYSMEM_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_DRAM_FREQ_DOWN_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_DDRPHY_DLLOFF_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_DDRPHY_DLLLOCK_SYS_PWR_REG,			{ 0x1, 0x1, 0x1} },
	{ EXYNOS5_APLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_MPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_VPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_EPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_BPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_GPLL_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_MPLLUSER_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_BPLLUSER_SYSCLK_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_TOP_BUS_SYS_PWR_REG,				{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_TOP_RETENTION_SYS_PWR_REG,			{ 0x1, 0x0, 0x1} },
	{ EXYNOS5_TOP_PWR_SYS_PWR_REG,				{ 0x3, 0x0, 0x3} },
	{ EXYNOS5_TOP_BUS_SYSMEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_TOP_RETENTION_SYSMEM_SYS_PWR_REG,		{ 0x1, 0x0, 0x1} },
	{ EXYNOS5_TOP_PWR_SYSMEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x3} },
	{ EXYNOS5_LOGIC_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_OSCCLK_GATE_SYS_PWR_REG,			{ 0x1, 0x0, 0x1} },
	{ EXYNOS5_LOGIC_RESET_SYSMEM_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_OSCCLK_GATE_SYSMEM_SYS_PWR_REG,		{ 0x1, 0x0, 0x1} },
	{ EXYNOS5_USBOTG_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_G2D_MEM_SYS_PWR_REG,				{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_USBDRD_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_SDMMC_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_CSSYS_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_SECSS_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_ROTATOR_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_INTRAM_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_INTROM_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_JPEG_MEM_SYS_PWR_REG,				{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_HSI_MEM_SYS_PWR_REG,				{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_MCUIOP_MEM_SYS_PWR_REG,			{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_SATA_MEM_SYS_PWR_REG,				{ 0x3, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_DRAM_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_MAU_SYS_PWR_REG,		{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_PAD_RETENTION_GPIO_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_UART_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_MMCA_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_MMCB_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_EBIA_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_EBIB_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_SPI_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_RETENTION_GPIO_SYSMEM_SYS_PWR_REG,	{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_ISOLATION_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_ISOLATION_SYSMEM_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_PAD_ALV_SEL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_XUSBXTI_SYS_PWR_REG,				{ 0x1, 0x1, 0x1} },
	{ EXYNOS5_XXTI_SYS_PWR_REG,				{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_EXT_REGULATOR_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_GPIO_MODE_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_GPIO_MODE_SYSMEM_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_GPIO_MODE_MAU_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_TOP_ASB_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x1} },
	{ EXYNOS5_TOP_ASB_ISOLATION_SYS_PWR_REG,		{ 0x1, 0x0, 0x1} },
	{ EXYNOS5_GSCL_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS5_ISP_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS5_MFC_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS5_G3D_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS5_DISP1_SYS_PWR_REG,				{ 0x7, 0x0, 0x0} },
	{ EXYNOS5_MAU_SYS_PWR_REG,				{ 0x7, 0x7, 0x0} },
	{ EXYNOS5_CMU_CLKSTOP_GSCL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_CLKSTOP_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_CMU_CLKSTOP_MFC_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_CLKSTOP_G3D_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_CLKSTOP_DISP1_SYS_PWR_REG,		{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_CLKSTOP_MAU_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_CMU_SYSCLK_GSCL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_SYSCLK_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_CMU_SYSCLK_MFC_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_SYSCLK_G3D_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_SYSCLK_DISP1_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_SYSCLK_MAU_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ EXYNOS5_CMU_RESET_GSCL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_RESET_ISP_SYS_PWR_REG,			{ 0x0, 0x0, 0x0} },
	{ EXYNOS5_CMU_RESET_MFC_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_RESET_G3D_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_RESET_DISP1_SYS_PWR_REG,			{ 0x1, 0x0, 0x0} },
	{ EXYNOS5_CMU_RESET_MAU_SYS_PWR_REG,			{ 0x1, 0x1, 0x0} },
	{ PMU_TABLE_END,},
};

void __iomem *exynos4_list_disable_pmu_reg[] = {
	EXYNOS4X12_CMU_SYSCLK_ISP_LOWPWR,
	EXYNOS4X12_CMU_RESET_ISP_LOWPWR,
};

void __iomem *exynos5_list_feed[] = {
	EXYNOS5_ARM_CORE0_OPTION,
	EXYNOS5_ARM_CORE1_OPTION,
	EXYNOS5_ARM_COMMON_OPTION,
	EXYNOS5_GSCL_OPTION,
	EXYNOS5_ISP_OPTION,
	EXYNOS5_MFC_OPTION,
	EXYNOS5_G3D_OPTION,
	EXYNOS5_DISP1_OPTION,
	EXYNOS5_MAU_OPTION,
	EXYNOS5_TOP_PWR_OPTION,
	EXYNOS5_TOP_PWR_SYSMEM_OPTION,
};


void __iomem *exynos3470_list_feed[] = {
	EXYNOS_ARM_CORE_OPTION(0),
	EXYNOS_ARM_CORE_OPTION(1),
	EXYNOS_ARM_CORE_OPTION(2),
	EXYNOS_ARM_CORE_OPTION(3),
	EXYNOS4_ARM_COMMON_OPTION,
	EXYNOS4_TOP_PWR_OPTION,
	EXYNOS4_CORE_TOP_PWR_OPTION,
	EXYNOS4_CAM_OPTION,
	EXYNOS4_TV_OPTION,
	EXYNOS4_MFC_OPTION,
	EXYNOS4_G3D_OPTION,
	EXYNOS4_LCD0_OPTION,
	EXYNOS4_ISP_OPTION,
	EXYNOS4_MAUDIO_OPTION,
};

void __iomem *exynos5_list_diable_wfi_wfe[] = {
	EXYNOS5_FSYS_ARM_OPTION,
	EXYNOS5_ISP_ARM_OPTION,
};

void __iomem *exynos5_list_disable_pmu_reg[] = {
	EXYNOS5_CMU_SYSCLK_ISP_SYS_PWR_REG,
	EXYNOS5_CMU_RESET_ISP_SYS_PWR_REG,
};

static void exynos5_init_pmu(void)
{
	unsigned int i;
	unsigned int tmp;

	/* Enable only SC_FEEDBACK */
	for (i = 0 ; i < ARRAY_SIZE(exynos5_list_feed) ; i++) {
		tmp = __raw_readl(exynos5_list_feed[i]);
		tmp &= ~(EXYNOS5_USE_SC_COUNTER);
		tmp |= EXYNOS5_USE_SC_FEEDBACK;
		__raw_writel(tmp, exynos5_list_feed[i]);
	}

	/*
	 * SKIP_DEACTIVATE_ACEACP_IN_PWDN_BITFIELD Enable
	 */
	tmp = __raw_readl(EXYNOS5_ARM_COMMON_OPTION);
	tmp |= EXYNOS5_SKIP_DEACTIVATE_ACEACP_IN_PWDN;
	__raw_writel(tmp, EXYNOS5_ARM_COMMON_OPTION);

	/*
	 * Disable WFI/WFE on XXX_OPTION
	 */
	for (i = 0 ; i < ARRAY_SIZE(exynos5_list_diable_wfi_wfe) ; i++) {
		tmp = __raw_readl(exynos5_list_diable_wfi_wfe[i]);
		tmp &= ~(EXYNOS5_OPTION_USE_STANDBYWFE |
			 EXYNOS5_OPTION_USE_STANDBYWFI);
		__raw_writel(tmp, exynos5_list_diable_wfi_wfe[i]);
	}
}

static void exynos3470_init_pmu(void)
{
	unsigned int i;
	unsigned int tmp;

	/* Enable only SC_FEEDBACK */
	for (i = 0 ; i < ARRAY_SIZE(exynos3470_list_feed) ; i++) {
		tmp = __raw_readl(exynos3470_list_feed[i]);
		tmp &= ~(EXYNOS5_USE_SC_COUNTER);
		tmp |= EXYNOS5_USE_SC_FEEDBACK;
		__raw_writel(tmp, exynos3470_list_feed[i]);
	}
}

void exynos_sys_powerdown_conf(enum sys_powerdown mode)
{
	unsigned int i;

	if (soc_is_exynos5250())
		exynos5_init_pmu();

	for (i = 0; (exynos_pmu_config[i].reg != PMU_TABLE_END) ; i++)
		__raw_writel(exynos_pmu_config[i].val[mode],
				exynos_pmu_config[i].reg);

		exynos3470_init_pmu();

		exynos_sys_powerdown_conf_mif(mode);

		if (mode == SYS_SLEEP) {
			__raw_writel(0x00000BB8, EXYNOS4270_XUSBXTI_DURATION);
			__raw_writel(0x00000BB8, EXYNOS4270_XXTI_DURATION);
			__raw_writel(0x00001D4C, EXYNOS4270_EXT_REGULATOR_DURATION);
			__raw_writel(0x00001D4C, EXYNOS4270_EXT_REGULATOR_COREBLK_DURATION);
			__raw_writel(0x00015F90, EXYNOS4270_LOGIC_RESET_CPBLK_DURATION);
			__raw_writel(0x000005DC, EXYNOS4270_REGULATOR_CPBLK_DURATION);
		}
}

void exynos_xxti_sys_powerdown(bool enable)
{
	unsigned int value;
	void __iomem *base;

	base = soc_is_exynos5250() ? EXYNOS5_XXTI_SYS_PWR_REG :
				     EXYNOS4_XXTI_LOWPWR;

	value = __raw_readl(base);

	if (enable)
		value |= EXYNOS_SYS_PWR_CFG;
	else
		value &= ~EXYNOS_SYS_PWR_CFG;

	__raw_writel(value, base);
}

#ifndef CONFIG_SOC_EXYNOS3470
void exynos_reset_assert_ctrl(bool on)
{
	unsigned int i;
	unsigned int option;

	for (i = 0; i < num_possible_cpus(); i++) {
		option = __raw_readl(EXYNOS_ARM_CORE_OPTION(i));
		option = on ? (option | EXYNOS_USE_DELAYED_RESET_ASSERTION) :
				   (option & ~EXYNOS_USE_DELAYED_RESET_ASSERTION);
		__raw_writel(option, EXYNOS_ARM_CORE_OPTION(i));
	}
}
#else
void exynos_reset_assert_ctrl(bool on) {}
#endif

void exynos_l2_common_pwr_ctrl(void)
{
	unsigned int i;
	unsigned int option;

	for (i = 0; i < CLUSTER_NUM; i++) {
		if ((__raw_readl(EXYNOS_COMMON_STATUS(i)) & 0x3) == 0) {
			option = __raw_readl(EXYNOS_COMMON_CONFIGURATION(i));
			option |= EXYNOS_L2_COMMON_PWR_EN;
			__raw_writel(option, EXYNOS_COMMON_CONFIGURATION(i));
		}
	}
}

void exynos_set_core_flag(void)
{
	int cluster_id = (read_cpuid_mpidr() >> 8) & 0xf;

	if (cluster_id)
		__raw_writel(ARM, EXYNOS_IROM_DATA2);
	else
		__raw_writel(KFC, EXYNOS_IROM_DATA2);
}


void exynos_pmu_wdt_control(bool on, unsigned int pmu_wdt_reset_type)
{
	unsigned int value;

	/*
	 * When SYS_WDTRESET is set, watchdog timer reset request is ignored
	 * by power management unit.
	 */
	if (pmu_wdt_reset_type == PMU_WDT_RESET_TYPE0) {
		pmu_wdt_reset_type = EXYNOS_SYS_WDTRESET;
	} else {
		pr_err("Failed to %s pmu wdt reset\n",
				on ? "enable" : "disable");
		return;
	}
	value = __raw_readl(EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	if (on)
		value &= ~pmu_wdt_reset_type;
	else
		value |= pmu_wdt_reset_type;
	__raw_writel(value, EXYNOS_AUTOMATIC_WDT_RESET_DISABLE);
	value = __raw_readl(EXYNOS_MASK_WDT_RESET_REQUEST);
	if (on)
		value &= ~pmu_wdt_reset_type;
	else
		value |= pmu_wdt_reset_type;
	__raw_writel(value, EXYNOS_MASK_WDT_RESET_REQUEST);
}

static int __init exynos_pmu_init(void)
{
	unsigned int value, i;

	if (soc_is_exynos4210()) {
		exynos_pmu_config = exynos4210_pmu_config;
		pr_info("EXYNOS4210 PMU Initialize\n");
	} else if (soc_is_exynos4212()) {
		exynos_pmu_config = exynos4212_pmu_config;
		pr_info("EXYNOS4212 PMU Initialize\n");
	} else if (soc_is_exynos4412()) {
		/* Follow registers should be set with 0x0 */
		for (i = 0; i < ARRAY_SIZE(exynos4_list_disable_pmu_reg); i++)
			__raw_writel(0x0, exynos4_list_disable_pmu_reg[i]);

		exynos_pmu_config = exynos4412_pmu_config;
		pr_info("EXYNOS4412 PMU Initialize\n");
	} else if (soc_is_exynos3470()) {
#ifdef CONFIG_REGULATOR_S2MPU01A
		/* Set GPM0(1) PDCFG register for PMIC */
		s5p_gpio_set_pd_cfg(EXYNOS4_GPM0(1), 0x1);
#endif
		value = (EXYNOS4_USE_STANDBY_WFI0 |\
				EXYNOS4_USE_STANDBY_WFI1 |\
				EXYNOS4_USE_STANDBY_WFI2 |\
				EXYNOS4_USE_STANDBY_WFI3 |\
				EXYNOS4_USE_STANDBY_WFE0 |\
				EXYNOS4_USE_STANDBY_WFE1 |\
				EXYNOS4_USE_STANDBY_WFE2 |\
				EXYNOS4_USE_STANDBY_WFE3);
		__raw_writel(value, EXYNOS_CENTRAL_SEQ_OPTION);

		/*
		* Set PSHOLD port for ouput high
		*/
		value = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
		value |= EXYNOS_PS_HOLD_OUTPUT_HIGH;
		__raw_writel(value, EXYNOS_PS_HOLD_CONTROL);
		/*
		 * Enable signal for PSHOLD port
		 */
		value = __raw_readl(EXYNOS_PS_HOLD_CONTROL);
		value |= EXYNOS_PS_HOLD_EN;
		__raw_writel(value, EXYNOS_PS_HOLD_CONTROL);

		exynos4_pmu_cp_init();

		exynos_pmu_config = exynos4270_pmu_config;
		pr_info("EXYNOS3470 PMU Initialize\n");
	} else if (soc_is_exynos5250()) {
		/* Initialize for using delay reset assertion */
		exynos_reset_assert_ctrl(true);

		/*
		 * Set logic reset duration
		 */
		value = __raw_readl(EXYNOS5_LOGIC_RESET_DURATION3);
		value &= ~EXYNOS5_DUR_WAIT_RESET_MASK;
		value |= EXYNOS5_DUR_WAIT_RESET_MIN;
		__raw_writel(value, EXYNOS5_LOGIC_RESET_DURATION3);

		/*
		 * Follow registers should be set with 0x0
		 */
		for (i = 0; i < ARRAY_SIZE(exynos5_list_disable_pmu_reg); i++)
			__raw_writel(0x0, exynos5_list_disable_pmu_reg[i]);

		exynos_pmu_config = exynos5250_pmu_config;
		pr_info("EXYNOS5250 PMU Initialize\n");
	} else {
		pr_info("EXYNOS: PMU not supported\n");
	}

	return 0;
}
arch_initcall(exynos_pmu_init);
