/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * EXYNOS5433 PMU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/bug.h>
#include <linux/of_address.h>
#include <linux/soc/samsung/exynos-regs-pmu.h>
#include <linux/soc/samsung/exynos-pmu.h>

#include "exynos-pmu.h"

static struct exynos_pmu_conf exynos5433_pmu_config[] = {
	/*
	 * { .offset = address,
	 *   .val    = { AFTR, STOP, DSTOP, DSTOP_PSR, LPD, LPA, ALPA, SLEEP } }
	 */
	{ .offset = EXYNOS7_ATLAS_CPU0_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_ATLAS_CPU0_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_ATLAS_CPU1_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_ATLAS_CPU1_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_ATLAS_CPU2_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_ATLAS_CPU2_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_ATLAS_CPU3_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_ATLAS_CPU3_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_APOLLO_CPU0_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_APOLLO_CPU0_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_APOLLO_CPU1_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_APOLLO_CPU1_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_APOLLO_CPU2_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_APOLLO_CPU2_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_APOLLO_CPU3_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_DIS_IRQ_APOLLO_CPU3_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_ATLAS_NONCPU_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS7_APOLLO_NONCPU_SYS_PWR_REG,
	  .val    = { 0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8 } },
	{ .offset = EXYNOS5433_A5IS_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_DIS_IRQ_A5IS_LOCAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_DIS_IRQ_A5IS_CENTRAL_SYS_PWR_REG,
	  .val    = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_ATLAS_L2_SYS_PWR_REG,
	  .val    = { 0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7 } },
	{ .offset = EXYNOS7_APOLLO_L2_SYS_PWR_REG,
	  .val    = { 0x0, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7 } },
	{ .offset = EXYNOS7_CLKSTOP_CMU_TOP_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_CLKRUN_CMU_TOP_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_RESET_CMU_TOP_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_RESET_CPUCLKSTOP_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS7_CLKSTOP_CMU_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_CLKRUN_CMU_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS7_RESET_CMU_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ .offset =  EXYNOS7_DDRPHY_DLLLOCK_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1 } },
	{ .offset = EXYNOS7_DISABLE_PLL_CMU_TOP_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_DISABLE_PLL_AUD_PLL_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS7_DISABLE_PLL_CMU_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_TOP_BUS_SYS_PWR_REG,
	  .val    = { 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_TOP_RETENTION_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 } },
	{ .offset = EXYNOS7_TOP_PWR_SYS_PWR_REG,
	  .val    = { 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3 } },
	{ .offset = EXYNOS7_TOP_BUS_MIF_SYS_PWR_REG,
	  .val    = { 0x7, 0x0, 0x0, 0x0, 0x7, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_TOP_RETENTION_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1 } },
	{ .offset = EXYNOS7_TOP_PWR_MIF_SYS_PWR_REG,
	  .val    = { 0x3, 0x3, 0x0, 0x0, 0x3, 0x0, 0x0, 0x3 } },
	{ .offset = EXYNOS7_LOGIC_RESET_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_OSCCLK_GATE_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 } },
	{ .offset = EXYNOS7_SLEEP_RESET_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS7_LOGIC_RESET_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_OSCCLK_GATE_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1 } },
	{ .offset = EXYNOS7_SLEEP_RESET_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS7_MEMORY_TOP_SYS_PWR_REG,
	  .val    = { 0x3, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_PAD_RETENTION_LPDDR3_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_RETENTION_JTAG_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_RETENTION_TOP_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_RETENTION_UART_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_RETENTION_EBIA_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_RETENTION_EBIB_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_RETENTION_SPI_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_RETENTION_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_ISOLATION_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 } },
	{ .offset = EXYNOS5433_PAD_RETENTION_USBXTI_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_PAD_RETENTION_BOOTLDO_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_ISOLATION_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1 } },
	{ .offset = EXYNOS7_PAD_RETENTION_FSYSGENIO_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_PAD_ALV_SEL_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_XXTI_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS7_XXTI26_SYS_PWR_REG,
	  .val    = { 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_EXT_REGULATOR_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS7_GPIO_MODE_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_GPIO_MODE_FSYS0_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_GPIO_MODE_MIF_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_GPIO_MODE_AUD_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x0, 0x0, 0x0, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS5433_GSCL_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_CAM0_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_MSCL_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_G3D_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_DISP_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0xF, 0xF, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_CAM1_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_AUD_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0xF, 0xF, 0x0 } },
	{ .offset = EXYNOS5433_FSYS_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_BUS2_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0xF, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_G2D_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_ISP0_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS7_MFC_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_HEVC_SYS_PWR_REG,
	  .val    = { 0xF, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } },
	{ .offset = EXYNOS5433_RESET_SLEEP_FSYS_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = EXYNOS5433_RESET_SLEEP_BUS2_SYS_PWR_REG,
	  .val    = { 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0 } },
	{ .offset = PMU_TABLE_END, },
};

static unsigned int const exynos5433_list_feed[] = {
	EXYNOS7_ATLAS_NONCPU_OPTION,
	EXYNOS7_APOLLO_NONCPU_OPTION,
	EXYNOS7_TOP_PWR_OPTION,
	EXYNOS7_TOP_PWR_MIF_OPTION,
	EXYNOS5433_AUD_OPTION,
	EXYNOS5433_CAM0_OPTION,
	EXYNOS7_DISP_OPTION,
	EXYNOS5433_G2D_OPTION,
	EXYNOS5433_G3D_OPTION,
	EXYNOS5433_HEVC_OPTION,
	EXYNOS5433_MSCL_OPTION,
	EXYNOS5433_MFC_OPTION,
	EXYNOS5433_GSCL_OPTION,
	EXYNOS5433_FSYS_OPTION,
	EXYNOS5433_ISP_OPTION,
	EXYNOS5433_BUS2_OPTION,
};

static void exynos5433_pmu_init(void)
{
	int cluster, cpu;
	int tmp, i;

	/* Enable non retention flip-flop reset for wakeup */
	tmp = pmu_raw_readl(EXYNOS5433_PMU_SPARE0);
	tmp |= EXYNOS5433_EN_NONRET_RESET;
	pmu_raw_writel(tmp, EXYNOS5433_PMU_SPARE0);

	 /* Enable only SC_FEEDBACK for the register list */
	for (i = 0 ; i < ARRAY_SIZE(exynos5433_list_feed) ; i++) {
		tmp = pmu_raw_readl(exynos5433_list_feed[i]);
		tmp &= ~EXYNOS5_USE_SC_COUNTER;
		tmp |= EXYNOS5_USE_SC_FEEDBACK;
		pmu_raw_writel(tmp, exynos5433_list_feed[i]);
	}

	/*
	 * Disable automatic L2 flush, Disable L2 retention and
	 * Enable STANDBYWFIL2, ACE/ACP
	 */
	for (cluster = 0; cluster < 2; cluster++) {
		tmp = pmu_raw_readl(EXYNOS7_ATLAS_L2_OPTION + (cluster * 0x20));
		tmp &= ~(EXYNOS7_USE_AUTO_L2FLUSHREQ | EXYNOS7_USE_RETENTION);
		/* For ATLAS */
		if (cluster == 0)
			tmp |= (EXYNOS7_USE_STANDBYWFIL2 |
				EXYNOS7_USE_DEACTIVATE_ACE |
				EXYNOS7_USE_DEACTIVATE_ACP);
		pmu_raw_writel(tmp, EXYNOS7_ATLAS_L2_OPTION + (cluster * 0x20));
	}

	/*
	 * Enable both SC_COUNTER and SC_FEEDBACK for the CPUs
	 * Use STANDBYWFI and SMPEN to indicate that core is ready to enter
	 * low power mode
	 */
	for (cpu = 0; cpu < 8; cpu++) {
		tmp = pmu_raw_readl(EXYNOS7_CPU_OPTION(cpu));
		tmp |= (EXYNOS5_USE_SC_FEEDBACK | EXYNOS5_USE_SC_COUNTER);
		tmp |= EXYNOS7_USE_SMPEN;
		tmp |= EXYNOS7_USE_STANDBYWFI;
		tmp &= ~EXYNOS7_USE_STANDBYWFE;
		pmu_raw_writel(tmp, EXYNOS7_CPU_OPTION(cpu));

		tmp = pmu_raw_readl(EXYNOS7_CPU_DURATION(cpu));
		tmp |= EXYNOS7_DUR_WAIT_RESET;
		tmp &= ~EXYNOS7_DUR_SCALL;
		tmp |= EXYNOS7_DUR_SCALL_VALUE;
		pmu_raw_writel(tmp, EXYNOS7_CPU_DURATION(cpu));
	}

	/* Skip atlas block power-off during automatic power down sequence */
	tmp = pmu_raw_readl(EXYNOS7_ATLAS_CPUSEQUENCER_OPTION);
	tmp |= EXYNOS7_SKIP_BLK_PWR_DOWN;
	pmu_raw_writel(tmp, EXYNOS7_ATLAS_CPUSEQUENCER_OPTION);

	/* Limit in-rush current during local power up of cores */
	tmp = pmu_raw_readl(EXYNOS7_UP_SCHEDULER);
	tmp |= EXYNOS7_ENABLE_ATLAS_CPU;
	pmu_raw_writel(tmp, EXYNOS7_UP_SCHEDULER);
}

static void exynos5433_set_wakeupmask(enum sys_powerdown mode)
{
	u32 intmask = 0;

	pmu_raw_writel(exynos_get_eint_wake_mask(), EXYNOS7_EINT_WAKEUP_MASK);
	pmu_raw_writel(0xFFFFFFFF, EXYNOS7_EINT_WAKEUP_MASK_MIF1);

	/* Disable WAKEUP event monitor */
	intmask = pmu_raw_readl(EXYNOS7_WAKEUP_MASK);
	intmask &= ~(1 << 31);
	pmu_raw_writel(intmask, EXYNOS7_WAKEUP_MASK);

	pmu_raw_writel(0xFFFF0000, EXYNOS7_WAKEUP_MASK2);
	pmu_raw_writel(0xFFFF0000, EXYNOS7_WAKEUP_MASK3);
}

static void exynos5433_pmu_central_seq(bool enable)
{
	unsigned int tmp;

	/* central sequencer */
	tmp = pmu_raw_readl(EXYNOS7_CENTRAL_SEQ_CONFIGURATION);
	if (enable)
		tmp &= ~EXYNOS7_CENTRALSEQ_PWR_CFG;
	else
		tmp |= EXYNOS7_CENTRALSEQ_PWR_CFG;
	pmu_raw_writel(tmp, EXYNOS7_CENTRAL_SEQ_CONFIGURATION);

	/* central sequencer MIF */
	tmp = pmu_raw_readl(EXYNOS7_CENTRAL_SEQ_MIF_CONFIGURATION);
	if (enable)
		tmp &= ~EXYNOS7_CENTRALSEQ_PWR_CFG;
	else
		tmp |= EXYNOS7_CENTRALSEQ_PWR_CFG;
	pmu_raw_writel(tmp, EXYNOS7_CENTRAL_SEQ_MIF_CONFIGURATION);
}

static void exynos5433_powerdown_conf(enum sys_powerdown mode)
{
	exynos5433_set_wakeupmask(mode);
	exynos5433_pmu_central_seq(true);
}

static void exynos5433_pmu_pad_retention_release(void)
{
	pmu_raw_writel(PAD_INITIATE_WAKEUP,
			EXYNOS5433_PAD_RETENTION_LPDDR3_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_AUD_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_MMC2_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_TOP_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_UART_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_MMC0_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_MMC1_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_EBIA_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_EBIB_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_SPI_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_MIF_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP,
			EXYNOS5433_PAD_RETENTION_USBXTI_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP,
			EXYNOS5433_PAD_RETENTION_BOOTLDO_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP, EXYNOS7_PAD_RETENTION_UFS_OPTION);
	pmu_raw_writel(PAD_INITIATE_WAKEUP,
			EXYNOS7_PAD_RETENTION_FSYSGENIO_OPTION);
}

static void exynos5433_powerup_conf(enum sys_powerdown mode)
{
	/* Check early wake up*/
	unsigned int wakeup;

	wakeup = pmu_raw_readl(EXYNOS7_CENTRAL_SEQ_CONFIGURATION);
	wakeup &= EXYNOS7_CENTRALSEQ_PWR_CFG;
	if (wakeup)
		/* Proper wakeup*/
		exynos5433_pmu_pad_retention_release();
	else
		/* Early wakeup */
		exynos5433_pmu_central_seq(false);
}

const struct exynos_pmu_data exynos5433_pmu_data = {
	.pmu_config		= exynos5433_pmu_config,
	.pmu_init		= exynos5433_pmu_init,
	.powerdown_conf		= exynos5433_powerdown_conf,
	.powerup_conf		= exynos5433_powerup_conf,
};
