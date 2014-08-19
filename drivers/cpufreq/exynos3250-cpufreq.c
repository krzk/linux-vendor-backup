/*
 * Copyright (c) 2013-2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS3250 - CPU frequency scaling support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>

#include <mach/regs-clock.h>

#include "exynos-cpufreq.h"

#define ASV_GROUP_NR	15

static struct clk *cpu_clk;
static struct clk *mout_core;
static struct clk *mout_mpll;
static struct clk *mout_apll;

static unsigned int exynos3250_volt_table[] = {
	1050000, 1050000, 1000000, 950000, 900000, 900000,
	900000,  900000,  900000, 900000, 900000,
};

/* ASV setting value */
static const unsigned int arm_asv_voltage_3250[][ASV_GROUP_NR] = {
	/*   ASV0     ASV1     ASV2     ASV3     ASV4     ASV5     ASV6     ASV7     ASV8    ASV9    ASV10    ASV11    ASV12    ASV13    ASV14 */
	{ 1150000, 1150000, 1125000, 1100000, 1075000, 1050000, 1025000, 1000000, 1000000, 975000,  975000,  950000,  950000,  925000,  925000},	/* L0 - Unused  */
	{ 1150000, 1150000, 1125000, 1100000, 1075000, 1050000, 1025000, 1000000, 1000000, 975000,  975000,  950000,  950000,  925000,  925000},	/* L1 - 1000MHz */
	{ 1112500, 1112500, 1087500, 1062500, 1037500, 1012500, 987500,  962500,  962500,  937500,  937500,  912500,  912500,  887500,  887500},	/* L2 - 900MHz  */
	{ 1075000, 1075000, 1050000, 1025000, 1000500, 975000,  950000,  925000,  925000,  900000,  900000,  875000,  875000,  850000,  850000},	/* L3 - 800MHz  */
	{ 1037500, 1037500, 1012500, 987500,  962500,  937500,  912500,  887500,  887500,  862500,  862500,  850000,  850000,  850000,  850000},	/* L4 - 700MHz  */
	{ 1000000, 1000000, 975000,  950000,  925000,  900000,  875000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000},	/* L5 - 600MHz  */
	{ 962500,  962500,  937500,  912500,  887500,  862500,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000},	/* L6 - 500MHz  */
	{ 925000,  925000,  900000,  875000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000},	/* L7 - 400MHz  */
	{ 887500,  887500,  862500,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000},	/* L8 - 300MHz  */
	{ 850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000},	/* L9 - 200MHz  */
	{ 850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000,  850000},	/* L10- 100MHz  */
};

static struct cpufreq_frequency_table exynos3250_freq_table[] = {
	{L0, CPUFREQ_ENTRY_INVALID},
	{L1, 1000 * 1000},
	{L2,  900 * 1000},
	{L3,  800 * 1000},
	{L4,  700 * 1000},
	{L5,  600 * 1000},
	{L6,  500 * 1000},
	{L7,  400 * 1000},
	{L8,  300 * 1000},
	{L9,  200 * 1000},
	{L10,  100 * 1000},
	{0, CPUFREQ_TABLE_END},
};

static struct apll_freq apll_freq_3250[] = {
	/*
	 * values:
	 * freq
	 * clock divider for CORE, COREM, ATB, PCLK_DBG, APLL, CORE2
	 * clock divider for COPY, HPM, RESERVED
	 * PLL M, P, S
	 */
	APLL_FREQ_EXYNOS3(1000, 0, 1, 4, 7, 1, 0, 7, 7, 0, 250, 3, 1),
	APLL_FREQ_EXYNOS3(1000, 0, 1, 4, 7, 1, 0, 7, 7, 0, 250, 3, 1),
	APLL_FREQ_EXYNOS3(900,  0, 1, 3, 7, 1, 0, 7, 7, 0, 300, 4, 1),
	APLL_FREQ_EXYNOS3(800,  0, 1, 3, 7, 1, 0, 7, 7, 0, 200, 3, 1),
	APLL_FREQ_EXYNOS3(700,  0, 1, 3, 7, 1, 0, 7, 7, 0, 175, 3, 1),
	APLL_FREQ_EXYNOS3(600,  0, 1, 3, 7, 1, 0, 7, 7, 0, 400, 4, 2),
	APLL_FREQ_EXYNOS3(500,  0, 1, 3, 7, 1, 0, 7, 7, 0, 250, 3, 2),
	APLL_FREQ_EXYNOS3(400,  0, 1, 3, 7, 1, 0, 7, 7, 0, 200, 3, 2),
	APLL_FREQ_EXYNOS3(300,  0, 1, 3, 5, 1, 0, 7, 7, 0, 400, 4, 3),
	APLL_FREQ_EXYNOS3(200,  0, 1, 3, 3, 1, 0, 7, 7, 0, 200, 3, 3),
	APLL_FREQ_EXYNOS3(100,  0, 1, 1, 1, 1, 0, 7, 7, 0, 200, 3, 4),
};

static void exynos3250_set_clkdiv(unsigned int div_index)
{
	unsigned int tmp;

	/* Change Divider - CPU0 */
	tmp = apll_freq_3250[div_index].clk_div_cpu0;

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU);

	while (__raw_readl(EXYNOS4_CLKDIV_STATCPU) & 0x11111111)
		cpu_relax();

	/* Change Divider - CPU1 */
	tmp = apll_freq_3250[div_index].clk_div_cpu1;

	__raw_writel(tmp, EXYNOS4_CLKDIV_CPU1);

	while (__raw_readl(EXYNOS4_CLKDIV_STATCPU1) & 0x111)
		cpu_relax();
}

static void exynos3250_set_apll(unsigned int index)
{
	unsigned int tmp, freq = apll_freq_3250[index].freq * 1000;
	struct clk *clk;

	clk = clk_get_parent(mout_apll);

	/* MUX_CORE_SEL = MPLL, ARMCLK uses MPLL for lock time */
	clk_set_parent(mout_core, mout_mpll);
	do {
		cpu_relax();
		tmp = (__raw_readl(EXYNOS4_CLKMUX_STATCPU)
			>> EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT);
		tmp &= 0x7;
	} while (tmp != 0x2);

	clk_set_rate(clk, freq);

	/* MUX_CORE_SEL = APLL */
	clk_set_parent(mout_core, mout_apll);
	do {
		cpu_relax();
		tmp = __raw_readl(EXYNOS4_CLKMUX_STATCPU);
		tmp &= EXYNOS4_CLKMUX_STATCPU_MUXCORE_MASK;
	} while (tmp != (0x1 << EXYNOS4_CLKSRC_CPU_MUXCORE_SHIFT));
}

static void exynos3250_set_frequency(unsigned int old_index,
				  unsigned int new_index)
{
	if (old_index > new_index) {
		exynos3250_set_clkdiv(new_index);
		exynos3250_set_apll(new_index);
	} else if (old_index < new_index) {
		exynos3250_set_apll(new_index);
		exynos3250_set_clkdiv(new_index);
	}
}

static void __init exynos3250_set_volt_table(void)
{
	unsigned int i;
	int exynos_result_of_asv = 0;

	pr_info("DVFS : VDD_ARM Voltage table set with %d Group\n", exynos_result_of_asv);

	for (i = 0; i < ARRAY_SIZE(exynos3250_volt_table); i++)
		exynos3250_volt_table[i]
			= arm_asv_voltage_3250[i][exynos_result_of_asv];
}

int exynos3250_cpufreq_init(struct exynos_dvfs_info *info)
{
	unsigned long rate;

	exynos3250_set_volt_table();

	cpu_clk = clk_get(info->dev, "div_core2");
	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);

	mout_core = clk_get(info->dev, "mout_core");
	if (IS_ERR(mout_core))
		goto err_moutcore;

	mout_mpll = clk_get(info->dev, "mout_mpll_user_c");
	if (IS_ERR(mout_mpll))
		goto err_mout_mpll;
	rate = clk_get_rate(mout_mpll) / 1000;

	mout_apll = clk_get(info->dev, "mout_apll");
	if (IS_ERR(mout_apll))
		goto err_mout_apll;

	info->mpll_freq_khz = rate;
	info->pll_safe_idx = L5;
	info->cpu_clk = cpu_clk;

	info->volt_table = exynos3250_volt_table;
	info->freq_table = exynos3250_freq_table;
	info->set_freq = exynos3250_set_frequency;

	return 0;

err_mout_apll:
	clk_put(mout_mpll);
err_mout_mpll:
	clk_put(mout_core);
err_moutcore:
	clk_put(cpu_clk);

	pr_debug("%s: failed initialization\n", __func__);
	return -EINVAL;
}
EXPORT_SYMBOL(exynos3250_cpufreq_init);
