// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 Samsung Electronics Co., Ltd.
// Copyright (c) Jonghwa Lee <jonghwa3.lee@samsung.com>
// Copyright (c) Chanwoo Choi <cw00.choi@samsung.com>
//
// EXYNOS5433 - CPU PMU (Power Management Unit) support

#include <linux/soc/samsung/exynos-regs-pmu.h>
#include <linux/soc/samsung/exynos-pmu.h>

#include "exynos-pmu.h"

static struct exynos_pmu_conf exynos5433_pmu_config[] = {
	/* { .offset = address,	.val = { AFTR, LPA, SLEEP } } */
	{ EXYNOS5433_ATLAS_CPU0_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_ATLAS_CPU0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_ATLAS_CPU1_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_ATLAS_CPU1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_ATLAS_CPU2_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_ATLAS_CPU2_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_ATLAS_CPU3_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_ATLAS_CPU3_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_APOLLO_CPU0_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_APOLLO_CPU0_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_APOLLO_CPU1_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_APOLLO_CPU1_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_APOLLO_CPU2_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_APOLLO_CPU2_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_APOLLO_CPU3_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_DIS_IRQ_APOLLO_CPU3_CENTRAL_SYS_PWR_REG,	{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_ATLAS_NONCPU_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_APOLLO_NONCPU_SYS_PWR_REG,			{ 0x0, 0x0, 0x8 } },
	{ EXYNOS5433_A5IS_SYS_PWR_REG,				{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_DIS_IRQ_A5IS_LOCAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_DIS_IRQ_A5IS_CENTRAL_SYS_PWR_REG,		{ 0x0, 0x0, 0x0 } },
	{ EXYNOS5433_ATLAS_L2_SYS_PWR_REG,			{ 0x0, 0x0, 0x7 } },
	{ EXYNOS5433_APOLLO_L2_SYS_PWR_REG,			{ 0x0, 0x0, 0x7 } },
	{ EXYNOS5433_CLKSTOP_CMU_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_CLKRUN_CMU_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_RESET_CMU_TOP_SYS_PWR_REG,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_RESET_CPUCLKSTOP_SYS_PWR_REG,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_CLKSTOP_CMU_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_CLKRUN_CMU_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_RESET_CMU_MIF_SYS_PWR_REG,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_DDRPHY_DLLLOCK_SYS_PWR_REG,		{ 0x1, 0x1, 0x1 } },
	{ EXYNOS5433_DISABLE_PLL_CMU_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_DISABLE_PLL_AUD_PLL_SYS_PWR_REG,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_DISABLE_PLL_CMU_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_TOP_BUS_SYS_PWR_REG,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS5433_TOP_RETENTION_SYS_PWR_REG,			{ 0x1, 0x0, 0x1 } },
	{ EXYNOS5433_TOP_PWR_SYS_PWR_REG,			{ 0x3, 0x0, 0x3 } },
	{ EXYNOS5433_TOP_BUS_MIF_SYS_PWR_REG,			{ 0x7, 0x0, 0x0 } },
	{ EXYNOS5433_TOP_RETENTION_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS5433_TOP_PWR_MIF_SYS_PWR_REG,			{ 0x3, 0x0, 0x3 } },
	{ EXYNOS5433_LOGIC_RESET_SYS_PWR_REG,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_OSCCLK_GATE_SYS_PWR_REG,			{ 0x1, 0x0, 0x1 } },
	{ EXYNOS5433_SLEEP_RESET_SYS_PWR_REG,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_LOGIC_RESET_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_OSCCLK_GATE_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS5433_SLEEP_RESET_MIF_SYS_PWR_REG,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_MEMORY_TOP_SYS_PWR_REG,			{ 0x3, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_LPDDR3_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_JTAG_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_TOP_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_UART_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_EBIA_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_EBIB_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_SPI_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_ISOLATION_SYS_PWR_REG,			{ 0x1, 0x0, 0x1 } },
	{ EXYNOS5433_PAD_RETENTION_USBXTI_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_RETENTION_BOOTLDO_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_ISOLATION_MIF_SYS_PWR_REG,		{ 0x1, 0x0, 0x1 } },
	{ EXYNOS5433_PAD_RETENTION_FSYSGENIO_SYS_PWR_REG,	{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_PAD_ALV_SEL_SYS_PWR_REG,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_XXTI_SYS_PWR_REG,				{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_XXTI26_SYS_PWR_REG,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_EXT_REGULATOR_SYS_PWR_REG,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_GPIO_MODE_SYS_PWR_REG,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_GPIO_MODE_FSYS0_SYS_PWR_REG,		{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_GPIO_MODE_MIF_SYS_PWR_REG,			{ 0x1, 0x0, 0x0 } },
	{ EXYNOS5433_GPIO_MODE_AUD_SYS_PWR_REG,			{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_GSCL_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_CAM0_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_MSCL_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_G3D_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_DISP_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_CAM1_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_AUD_SYS_PWR_REG,				{ 0xF, 0xF, 0x0 } },
	{ EXYNOS5433_FSYS_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_BUS2_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_G2D_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_ISP0_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_MFC_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_HEVC_SYS_PWR_REG,				{ 0xF, 0x0, 0x0 } },
	{ EXYNOS5433_RESET_SLEEP_FSYS_SYS_PWR_REG,		{ 0x1, 0x1, 0x0 } },
	{ EXYNOS5433_RESET_SLEEP_BUS2_SYS_PWR_REG,		{ 0x1, 0x1, 0x0 } },
	{ PMU_TABLE_END, },
};

static unsigned int const exynos5433_list_feed[] = {
	EXYNOS5433_ATLAS_NONCPU_OPTION,
	EXYNOS5433_APOLLO_NONCPU_OPTION,
	EXYNOS5433_TOP_PWR_OPTION,
	EXYNOS5433_TOP_PWR_MIF_OPTION,
	EXYNOS5433_AUD_OPTION,
	EXYNOS5433_CAM0_OPTION,
	EXYNOS5433_DISP_OPTION,
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

static void exynos5433_set_wakeupmask(enum sys_powerdown mode)
{
	u32 intmask = 0;

	pmu_raw_writel(exynos_get_eint_wake_mask(),
					EXYNOS5433_EINT_WAKEUP_MASK);

	/* Disable WAKEUP event monitor */
	intmask = pmu_raw_readl(EXYNOS5433_WAKEUP_MASK);
	intmask &= ~(1 << 31);
	pmu_raw_writel(intmask, EXYNOS5433_WAKEUP_MASK);

	pmu_raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK2);
	pmu_raw_writel(0xFFFF0000, EXYNOS5433_WAKEUP_MASK3);
}

static void exynos5433_pmu_central_seq(bool enable)
{
	unsigned int tmp;

	tmp = pmu_raw_readl(EXYNOS5433_CENTRAL_SEQ_CONFIGURATION);
	if (enable)
		tmp &= ~EXYNOS5433_CENTRALSEQ_PWR_CFG;
	else
		tmp |= EXYNOS5433_CENTRALSEQ_PWR_CFG;
	pmu_raw_writel(tmp, EXYNOS5433_CENTRAL_SEQ_CONFIGURATION);

	tmp = pmu_raw_readl(EXYNOS5433_CENTRAL_SEQ_MIF_CONFIGURATION);
	if (enable)
		tmp &= ~EXYNOS5433_CENTRALSEQ_PWR_CFG;
	else
		tmp |= EXYNOS5433_CENTRALSEQ_PWR_CFG;
	pmu_raw_writel(tmp, EXYNOS5433_CENTRAL_SEQ_MIF_CONFIGURATION);
}

static void exynos5433_pmu_init(void)
{
	unsigned int tmp;
	int i, cluster, cpu;

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
		tmp = pmu_raw_readl(EXYNOS5433_ATLAS_L2_OPTION + (cluster * 0x20));
		tmp &= ~(EXYNOS5433_USE_AUTO_L2FLUSHREQ | EXYNOS5433_USE_RETENTION);

		if (cluster == 0) {
			tmp |= (EXYNOS5433_USE_STANDBYWFIL2 |
				EXYNOS5433_USE_DEACTIVATE_ACE |
				EXYNOS5433_USE_DEACTIVATE_ACP);
		}
		pmu_raw_writel(tmp, EXYNOS5433_ATLAS_L2_OPTION + (cluster * 0x20));
	}

	/*
	 * Enable both SC_COUNTER and SC_FEEDBACK for the CPUs
	 * Use STANDBYWFI and SMPEN to indicate that core is ready to enter
	 * low power mode
	 */
	for (cpu = 0; cpu < 8; cpu++) {
		tmp = pmu_raw_readl(EXYNOS5433_CPU_OPTION(cpu));
		tmp |= (EXYNOS5_USE_SC_FEEDBACK | EXYNOS5_USE_SC_COUNTER);
		tmp |= EXYNOS5433_USE_SMPEN;
		tmp |= EXYNOS5433_USE_STANDBYWFI;
		tmp &= ~EXYNOS5433_USE_STANDBYWFE;
		pmu_raw_writel(tmp, EXYNOS5433_CPU_OPTION(cpu));

		tmp = pmu_raw_readl(EXYNOS5433_CPU_DURATION(cpu));
		tmp |= EXYNOS5433_DUR_WAIT_RESET;
		tmp &= ~EXYNOS5433_DUR_SCALL;
		tmp |= EXYNOS5433_DUR_SCALL_VALUE;
		pmu_raw_writel(tmp, EXYNOS5433_CPU_DURATION(cpu));
	}

	/* Skip atlas block power-off during automatic power down sequence */
	tmp = pmu_raw_readl(EXYNOS5433_ATLAS_CPUSEQUENCER_OPTION);
	tmp |= EXYNOS5433_SKIP_BLK_PWR_DOWN;
	pmu_raw_writel(tmp, EXYNOS5433_ATLAS_CPUSEQUENCER_OPTION);

	/* Limit in-rush current during local power up of cores */
	tmp = pmu_raw_readl(EXYNOS5433_UP_SCHEDULER);
	tmp |= EXYNOS5433_ENABLE_ATLAS_CPU;
	pmu_raw_writel(tmp, EXYNOS5433_UP_SCHEDULER);
}

static void exynos5433_powerdown_conf(enum sys_powerdown mode)
{
	switch (mode) {
	case SYS_SLEEP:
		exynos5433_set_wakeupmask(mode);
		exynos5433_pmu_central_seq(true);	// enable system-level low-power mode
		break;
	default:
		break;
	};
}

const struct exynos_pmu_data exynos5433_pmu_data = {
	.pmu_config		= exynos5433_pmu_config,
	.pmu_init		= exynos5433_pmu_init,
	.powerdown_conf		= exynos5433_powerdown_conf,
};
