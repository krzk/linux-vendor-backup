/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Author: Tarek Dakhran <t.dakhran@samsung.com>
 *
 * Copyright (c) 2014 Hardkernel Co., Ltd.
 * Author: Hakjoo Kim <ruppi.kim@hardkernel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos5410 SoC.
*/

#include <dt-bindings/clock/exynos5410.h>

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk.h"

#define APLL_LOCK		0x0
#define APLL_CON0		0x100
#define CPLL_LOCK		0x10020
#define CPLL_CON0		0x10120
#define MPLL_LOCK		0x4000
#define MPLL_CON0		0x4100
#define BPLL_LOCK		0x20010
#define BPLL_CON0		0x20110
#define KPLL_LOCK		0x28000
#define KPLL_CON0		0x28100
#define VPLL_LOCK		0x10050
#define VPLL_CON0		0x10140
#define DPLL_LOCK		0x10030
#define DPLL_CON0		0x10128
#define EPLL_LOCK		0x10040
#define EPLL_CON0		0x10130
#define IPLL_LOCK		0x10060
#define IPLL_CON0		0x10150
#define GATE_IP_ACP		0x8800


#define SRC_CPU			0x200
#define DIV_CPU0		0x500
#define SRC_CPERI1		0x4204
#define DIV_TOP0		0x10510
#define DIV_TOP1		0x10514
#define DIV_TOP2		0x10518
#define DIV_FSYS1		0x1054c
#define DIV_FSYS2		0x10550
#define DIV_PERIC0		0x10558
#define DIV_PERIC1		0x1055c
#define DIV_PERIC2		0x10560
#define DIV_PERIC3		0x10564
#define DIV_PERIC4		0x10568
#define DIV_PERIC5		0x1056C
#define DIV_DISP1_0		0x1052c
#define DIV_MAU			0x10544
#define DIV_FSYS0		0x10548
#define SRC_TOP0		0x10210
#define SRC_TOP1		0x10214
#define SRC_TOP2		0x10218
#define SRC_TOP3		0x1021c
#define SRC_FSYS		0x10244
#define SRC_DISP1_0		0x1022c
#define SRC_MAU			0x10240
#define SRC_PERIC0		0x10250
#define SRC_PERIC1		0x10254
#define SRC_MASK_DISP1_0	0x1032c
#define SRC_MASK_MAU		0x10334
#define SRC_MASK_FSYS		0x10340
#define SRC_MASK_PERIC0		0x10350
#define GATE_BUS_FSYS0		0x10740
#define GATE_TOP_SCLK_GSCL	0x10820
#define GATE_TOP_SCLK_DISP1	0x10828
#define GATE_TOP_SCLK_MAU	0x1083c
#define GATE_TOP_SCLK_FSYS	0x10840
#define GATE_TOP_SCLK_PERIC	0x10850
#define GATE_IP_FSYS		0x10944
#define GATE_IP_PERIC		0x10950
#define GATE_IP_PERIS		0x10960
#define GATE_IP_DISP1		0x10928
#define GATE_IP_GSCL0		0x10910
#define GATE_IP_GSCL1		0x10920
#define GATE_IP_MFC		0x1092c
#define GATE_IP_GEN		0x10934
#define SRC_CDREX		0x20200
#define SRC_KFC			0x28200
#define DIV_KFC0		0x28500

/* list of PLLs */
enum exynos5410_plls {
	apll, cpll, mpll,
	bpll, kpll, vpll,
	dpll, epll, ipll,
	nr_plls			/* number of PLLs */
};

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
static unsigned long exynos5410_clk_regs[] __initdata = {
	SRC_CPU,
	DIV_CPU0,
	SRC_CPERI1,
	SRC_TOP0,
	SRC_TOP1,
	SRC_TOP2,
	SRC_TOP3,
	SRC_MAU,
	SRC_FSYS,
	SRC_PERIC0,
	SRC_PERIC1,
	DIV_TOP0,
	DIV_TOP1,
	DIV_TOP2,
	DIV_MAU,
	DIV_FSYS0,
	DIV_FSYS1,
	DIV_FSYS2,
	DIV_PERIC0,
	DIV_PERIC1,
	DIV_PERIC2,
	DIV_PERIC3,
	DIV_PERIC4,
	SRC_MASK_FSYS,
	SRC_MASK_PERIC0,
	GATE_BUS_FSYS0,
	GATE_IP_FSYS,
	GATE_IP_PERIC,
	GATE_IP_PERIS,
	GATE_IP_GSCL0,
	GATE_IP_GSCL1,
	GATE_IP_MFC,
	GATE_IP_GEN,
	GATE_IP_ACP,
	GATE_TOP_SCLK_GSCL,
	GATE_TOP_SCLK_DISP1,
	GATE_TOP_SCLK_MAU,
	GATE_TOP_SCLK_FSYS,
	GATE_TOP_SCLK_PERIC,
	SRC_CDREX,
	SRC_KFC,
	DIV_KFC0,
};

/* list of all parent clocks */
PNAME(apll_p)		= { "fin_pll", "fout_apll", };
PNAME(bpll_p)		= { "fin_pll", "fout_bpll", };
PNAME(cpll_p)		= { "fin_pll", "fout_cpll" };
PNAME(mpll_p)		= { "fin_pll", "fout_mpll", };
PNAME(kpll_p)		= { "fin_pll", "fout_kpll", };
PNAME(dpll_p)		= { "fin_pll", "fout_dpll", };
PNAME(epll_p)		= { "fin_pll", "fout_epll", };
PNAME(ipll_p)		= { "fin_pll", "fout_ipll", };

PNAME(mout_cpu_p)	= { "mout_apll", "sclk_mpll", };
PNAME(mout_kfc_p)	= { "mout_kpll", "sclk_mpll", };
PNAME(mout_vpllsrc_p)	= { "fin_pll", "sclk_hdmi27m" };
PNAME(mout_vpll_p)	= { "mout_vpllsrc", "fout_vpll" };
PNAME(mout_hdmi_p)	= { "div_hdmi_pixel", "sclk_hdmiphy" };
PNAME(mout_usbd3_p)	= { "sclk_mpll_bpll", "fin_pll" };

PNAME(mpll_user_p)	= { "fin_pll", "sclk_mpll", };
PNAME(bpll_user_p)	= { "fin_pll", "sclk_bpll", };
PNAME(mpll_bpll_p)	= { "sclk_mpll_muxed", "sclk_bpll_muxed", };
PNAME(cpll_mpll_p)	= { "sclk_cpll", "sclk_mpll_muxed", };
PNAME(aclk200_disp1_p) = { "fin_pll", "div_aclk200", };
PNAME(aclk300_disp0_p) = { "fin_pll", "div_aclk300_disp0", };
PNAME(aclk300_disp1_p) = { "fin_pll", "div_aclk300_disp1", };
PNAME(aclk300_gscl_p) = { "fin_pll", "div_aclk300_gscl", };
PNAME(aclk300_jpeg_p) = { "fin_pll", "div_aclk300_jpeg", };
PNAME(aclk333_sub_p) = { "fin_pll", "div_aclk333", };

PNAME(group2_p)		= { "fin_pll", "fin_pll", "sclk_hdmi27m", "sclk_dptxphy",
			"sclk_uhostphy", "sclk_hdmiphy", "sclk_mpll_bpll",
			 "sclk_dpll", "sclk_vpll", "sclk_cpll" };

PNAME(audio0_p)	= { "cdclk0", "fin_pll", "sclk_hdmi27m",
		  "sclk_dptxphy", "sclk_uhostphy", "sclk_hdmiphy",
		  "sclk_mpll_bpll", "sclk_epll", "sclk_vpll", "sclk_cpll" };
PNAME(audio1_p)	= { "cdclk1", "fin_pll", "sclk_hdmi27m",
		  "sclk_dptxphy", "sclk_uhostphy", "sclk_hdmiphy",
		  "sclk_mpll_bpll", "sclk_epll", "sclk_vpll", "sclk_cpll" };
PNAME(audio2_p)	= { "cdclk2", "fin_pll", "sclk_hdmi27m",
		  "sclk_dptxphy", "sclk_uhostphy", "sclk_hdmiphy",
		  "sclk_mpll_bpll", "sclk_epll", "sclk_vpll", "sclk_cpll" };
PNAME(spdif_p)	= { "dout_audio0", "dout_audio1", "dout_audio2",
		  "spdif_extclk" };
PNAME(maudio0_p)	= { "cdclk0", "fin_pll", "sclk_hdmi27m",
		  "sclk_dptxphy", "sclk_uhostphy", "sclk_hdmiphy",
		  "sclk_mpll_bpll", "sclk_epll", "sclk_vpll", "sclk_cpll" };


static const struct samsung_pll_rate_table apll_tbl[] = {
	/* sorted in descending order */
	/* PLL_35XX_RATE(rate, m, p, s) */
	PLL_35XX_RATE(2100000000, 175, 2, 0),
	PLL_35XX_RATE(2000000000, 250, 3, 0),
	PLL_35XX_RATE(1900000000, 475, 6, 0),
	PLL_35XX_RATE(1800000000, 225, 3, 0),
	PLL_35XX_RATE(1700000000, 425, 6, 0),
	PLL_35XX_RATE(1600000000, 200, 3, 0),
	PLL_35XX_RATE(1500000000, 250, 4, 0),
	PLL_35XX_RATE(1400000000, 175, 3, 0),
	PLL_35XX_RATE(1300000000, 325, 6, 0),
	PLL_35XX_RATE(1200000000, 100, 2, 0),
	PLL_35XX_RATE(1100000000, 275, 3, 1),
	PLL_35XX_RATE(1000000000, 250, 3, 1),
	PLL_35XX_RATE(900000000, 150, 2, 1),
	PLL_35XX_RATE(800000000, 200, 3, 1),
	PLL_35XX_RATE(700000000, 175, 3, 1),
	PLL_35XX_RATE(600000000, 100, 2, 1),
	PLL_35XX_RATE(500000000, 250, 3, 2),
	PLL_35XX_RATE(400000000, 200, 3, 2),
	PLL_35XX_RATE(300000000, 100, 2, 2),
	PLL_35XX_RATE(200000000, 200, 3, 3),
};

/* The next 2 tables for CPLL and DPLL were copies from
 * the "clock-exynos5410.c" file in the "3.4.y" branch
 */
static const struct samsung_pll_rate_table cpll_tbl[] = {
	/* sorted in descending order */
	/* PLL_35XX_RATE(rate, m, p, s) */
	PLL_35XX_RATE(666000000, 222, 4, 1),
	PLL_35XX_RATE(640000000, 160, 3, 1),
	PLL_35XX_RATE(320000000, 160, 3, 2)
};


static const struct samsung_pll_rate_table dpll_tbl[] = {
	/* sorted in descending order */
	/* PLL_35XX_RATE(rate, m, p, s) */
	PLL_35XX_RATE(600000000, 200, 4, 1)
};



static const struct samsung_pll_rate_table kpll_tbl[] = {
	/* sorted in descending order */
	/* PLL_35XX_RATE(rate, m, p, s) */
	PLL_35XX_RATE(1500000000, 250, 4, 0),
	PLL_35XX_RATE(1400000000, 175, 3, 0),
	PLL_35XX_RATE(1300000000, 325, 6, 0),
	PLL_35XX_RATE(1200000000, 100, 2, 0),
	PLL_35XX_RATE(1100000000, 275, 3, 1),
	PLL_35XX_RATE(1000000000, 250, 3, 1),
	PLL_35XX_RATE(900000000, 150, 2, 1),
	PLL_35XX_RATE(800000000, 200, 3, 1),
	PLL_35XX_RATE(700000000, 175, 3, 1),
	PLL_35XX_RATE(600000000, 100, 2, 1),
	PLL_35XX_RATE(500000000, 250, 3, 2),
	PLL_35XX_RATE(400000000, 200, 3, 2),
	PLL_35XX_RATE(300000000, 100, 2, 2),
	PLL_35XX_RATE(200000000, 200, 3, 3),
};


/*
 * The Exynos 5410 VPLL Clock is actually an PLL_2650,
 * which is very similar to the PLL_36XX, except for the size
 * of MDIV field. This field should have 10 bits and not 9.
 * However, since the parameter MDIV for the table below
 * is never above 2^9 - 1 = 511, we are in good shape.
 */
static const struct samsung_pll_rate_table vpll_tbl[] = {
	/* sorted in descending order */
	/* PLL_36XX_RATE(rate, m, p, s, k) */
	PLL_36XX_RATE(880000000, 220, 3, 1, 0),
	PLL_36XX_RATE(640000000, 160, 3, 1, 0),
	PLL_36XX_RATE(532000000, 133, 3, 1, 0),
	PLL_36XX_RATE(480000000, 240, 3, 2, 0),
	PLL_36XX_RATE(440000000, 220, 3, 2, 0),
	PLL_36XX_RATE(350000000, 175, 3, 2, 0),
	PLL_36XX_RATE(333000000, 111, 2, 2, 0),
	PLL_36XX_RATE(266000000, 133, 3, 2, 0),
	PLL_36XX_RATE(177000000, 118, 2, 3, 0),
	PLL_36XX_RATE(123500000, 330, 4, 4, 0),
	PLL_36XX_RATE( 89000000, 178, 3, 4, 0)
};



static const struct samsung_pll_rate_table epll_tbl[] = {
	/* sorted in descending order */
	/* PLL_36XX_RATE(rate, m, p, s, k) */
	PLL_36XX_RATE(600000000, 100, 2, 1,     0),
	PLL_36XX_RATE(400000000, 200, 3, 2,     0),
	PLL_36XX_RATE(200000000, 200, 3, 3,     0),
	PLL_36XX_RATE(180633600, 301, 5, 3,  3670),
	PLL_36XX_RATE( 67737600, 452, 5, 5, 27263),
	PLL_36XX_RATE( 49152000, 197, 3, 5, 25690),
	PLL_36XX_RATE( 45158400, 181, 3, 5, 24012),
};


static const struct samsung_pll_rate_table ipll_tbl[] = {
	/* sorted in descending order */
	/* PLL_35XX_RATE(rate, m, p, s, k) */
	PLL_35XX_RATE(864000000, 288, 4, 1),
	PLL_35XX_RATE(666000000, 222, 4, 1),
	PLL_35XX_RATE(432000000, 288, 4, 2),
};


/* fixed rate clocks generated outside the soc */
static struct samsung_fixed_rate_clock exynos5410_fixed_rate_ext_clks[] __initdata = {
	FRATE(CLK_FIN_PLL, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

/* fixed rate clocks generated inside the soc */
static struct samsung_fixed_rate_clock exynos5410_fixed_rate_clks[] __initdata = {
	FRATE(CLK_SCLK_HDMIPHY, "sclk_hdmiphy", NULL, CLK_IS_ROOT, 24000000),
	FRATE(0, "sclk_hdmi27m", NULL, CLK_IS_ROOT, 24000000),
	FRATE(0, "sclk_dptxphy", NULL, CLK_IS_ROOT, 24000000),
	FRATE(0, "sclk_uhostphy", NULL, CLK_IS_ROOT, 48000000),
};

static struct samsung_mux_clock exynos5410_pll_pmux_clks[] __initdata = {
	MUX_A(0, "mout_vpllsrc", mout_vpllsrc_p, SRC_TOP2, 0, 1, "mout_vpllsrc"),
};

static struct samsung_mux_clock exynos5410_mux_clks[] __initdata = {
	MUX_A(0, "mout_apll", apll_p, SRC_CPU, 0, 1, "mout_apll"),
	MUX_A(0, "mout_cpu", mout_cpu_p, SRC_CPU, 16, 1, "mout_cpu"),

	MUX_A(0, "mout_kpll", kpll_p, SRC_KFC, 0, 1, "mout_kpll"),
	MUX_A(0, "mout_kfc", mout_kfc_p, SRC_KFC, 16, 1, "mout_kfc"),

	MUX_A(0, "sclk_mpll", mpll_p, SRC_CPERI1, 8, 1, "mout_mpll"),
	MUX(0, "sclk_mpll_muxed", mpll_user_p, SRC_TOP2, 20, 1),

	MUX(0, "sclk_bpll", bpll_p, SRC_CDREX, 0, 1),
	MUX(0, "sclk_bpll_muxed", bpll_user_p, SRC_TOP2, 24, 1),
	MUX_A(0, "sclk_vpll", mout_vpll_p, SRC_TOP2, 16, 1, "sclk_vpll"),

        MUX(CLK_SCLK_EPLL, "sclk_epll", epll_p, SRC_TOP2, 12, 1),
        MUX(0, "sclk_ipll", ipll_p, SRC_TOP2, 14, 1),

	MUX(0, "sclk_cpll", cpll_p, SRC_TOP2, 8, 1),
	MUX_A(0, "sclk_dpll", dpll_p, SRC_TOP2, 10, 1, "sclk_dpll"),

	MUX(0, "sclk_mpll_bpll", mpll_bpll_p, SRC_TOP1, 20, 1),

	/* MAU Block */
	MUX(0, "mout_maudio0", maudio0_p, SRC_MAU, 0, 4),

	MUX(0, "mout_mmc0", group2_p, SRC_FSYS, 0, 4),
	MUX(0, "mout_mmc1", group2_p, SRC_FSYS, 4, 4),
	MUX(0, "mout_mmc2", group2_p, SRC_FSYS, 8, 4),

	MUX(0, "mout_usbd300", mout_usbd3_p, SRC_FSYS, 28, 1),
	MUX(0, "mout_usbd301", mout_usbd3_p, SRC_FSYS, 29, 1),

	MUX(0, "mout_uart0", group2_p, SRC_PERIC0, 0, 4),
	MUX(0, "mout_uart1", group2_p, SRC_PERIC0, 4, 4),
	MUX(0, "mout_uart2", group2_p, SRC_PERIC0, 8, 4),
	MUX(0, "mout_uart3", group2_p, SRC_PERIC0, 12, 4),


	MUX(0, "mout_spdif", spdif_p, SRC_PERIC0, 8, 2),
	MUX(CLK_MOUT_AUDIO0, "mout_audio0", audio0_p, SRC_PERIC1, 12, 4),
	MUX(0, "mout_audio1", audio1_p, SRC_PERIC1, 0, 4),
	MUX(0, "mout_audio2", audio2_p, SRC_PERIC1, 4, 4),

	MUX(0, "mout_aclk166", mpll_bpll_p, SRC_TOP0, 8, 1),
	MUX(0, "mout_aclk200", mpll_bpll_p, SRC_TOP0, 12, 1),
	MUX(0, "mout_aclk333", cpll_mpll_p, SRC_TOP0, 16, 1),
	MUX(0, "mout_aclk400", mpll_bpll_p, SRC_TOP0, 20, 1),
	MUX(0, "mout_aclk266_gscl", mpll_bpll_p, SRC_TOP1, 28, 1),

	MUX_A(0, "mout_aclk200_disp1", aclk200_disp1_p,
			SRC_TOP3, 4, 1, "aclk200_disp1"),
	MUX(CLK_MOUT_HDMI, "mout_hdmi", mout_hdmi_p, SRC_DISP1_0, 20, 1),
	MUX(0, "mout_fimd1", group2_p, SRC_DISP1_0, 0, 4),
	MUX(0, "mout_aclk300_gscl", aclk300_gscl_p, SRC_TOP3, 17, 1),
	MUX(0, "mout_aclk300_disp0", aclk300_disp0_p, SRC_TOP3, 18, 1),
	MUX_A(0, "mout_aclk300_disp1", aclk300_disp1_p, SRC_TOP3, 19, 1,
		"aclk300_disp1"),
	MUX(0, "mout_aclk400_isp", mpll_bpll_p, SRC_TOP3, 20, 1),
	MUX(0, "mout_aclk333_sub", aclk333_sub_p, SRC_TOP3, 24, 1),
	MUX(0, "mout_aclk300_jpeg", aclk300_jpeg_p, SRC_TOP3, 28, 1),
};

static struct samsung_div_clock exynos5410_div_clks[] __initdata = {
	DIV(0, "div_arm", "mout_cpu", DIV_CPU0, 0, 3),
	DIV(0, "div_arm2", "div_arm", DIV_CPU0, 28, 3),

	DIV(0, "div_acp", "div_arm2", DIV_CPU0, 8, 3),
	DIV(0, "div_cpud", "div_arm2", DIV_CPU0, 4, 3),
	DIV(0, "div_atb", "div_arm2", DIV_CPU0, 16, 3),
	DIV(0, "pclk_dbg", "div_arm2", DIV_CPU0, 20, 3),

	DIV(0, "div_kfc", "mout_kfc", DIV_KFC0, 0, 3),
	DIV(0, "div_aclk", "div_kfc", DIV_KFC0, 4, 3),
	DIV(0, "div_pclk", "div_kfc", DIV_KFC0, 20, 3),
	DIV(0, "sclk_kpll", "mout_kpll", DIV_KFC0, 24, 3),

	DIV(0, "aclk66_pre", "sclk_mpll_muxed", DIV_TOP1, 24, 3),
	DIV(0, "div_aclk66", "aclk66_pre", DIV_TOP0, 0, 3),

	/* Audio Block */
	DIV(0, "dout_maudio0", "mout_maudio0", DIV_MAU, 0, 4),
	DIV(0, "dout_maupcm0", "dout_maudio0", DIV_MAU, 4, 8),

	DIV(CLK_SCLK_USBPHY300, "sclk_usbphy300", "mout_usbd300", DIV_FSYS0, 16, 4),
	DIV(CLK_SCLK_USBPHY301, "sclk_usbphy301", "mout_usbd301", DIV_FSYS0, 20, 4),
	DIV(0, "div_usbd300", "mout_usbd300", DIV_FSYS0, 24, 4),
	DIV(0, "div_usbd301", "mout_usbd301", DIV_FSYS0, 28, 4),

	DIV(0, "div_mmc0", "mout_mmc0", DIV_FSYS1, 0, 4),
	DIV(0, "div_mmc1", "mout_mmc1", DIV_FSYS1, 16, 4),
	DIV(0, "div_mmc2", "mout_mmc2", DIV_FSYS2, 0, 4),

	DIV_F(0, "div_mmc_pre0", "div_mmc0",
			DIV_FSYS1, 8, 8, CLK_SET_RATE_PARENT, 0),
	DIV_F(0, "div_mmc_pre1", "div_mmc1",
			DIV_FSYS1, 24, 8, CLK_SET_RATE_PARENT, 0),
	DIV_F(0, "div_mmc_pre2", "div_mmc2",
			DIV_FSYS2, 8, 8, CLK_SET_RATE_PARENT, 0),

	DIV(0, "div_uart0", "mout_uart0", DIV_PERIC0, 0, 4),
	DIV(0, "div_uart1", "mout_uart1", DIV_PERIC0, 4, 4),
	DIV(0, "div_uart2", "mout_uart2", DIV_PERIC0, 8, 4),
	DIV(0, "div_uart3", "mout_uart3", DIV_PERIC0, 12, 4),

	/* PCM */
	DIV(0, "dout_pcm", "dout_audio0", DIV_MAU, 4, 8),
	DIV(0, "dout_pcm0", "dout_audio2", DIV_PERIC4, 20, 8),
	DIV(0, "dout_pcm1", "dout_audio1", DIV_PERIC4, 4, 8),
	DIV(0, "dout_pcm2", "dout_audio2", DIV_PERIC4, 12, 8),
	/* this seems to overlap with "dout_audio2", which is strange */

	/* Audio - I2S */
	DIV(0, "dout_i2s1", "dout_audio1", DIV_PERIC5, 0, 6),
	DIV(0, "dout_i2s2", "dout_audio2", DIV_PERIC5, 8, 6),
	DIV(0, "dout_audio0", "mout_audio0", DIV_PERIC5, 24, 4),
	DIV(0, "dout_audio1", "mout_audio1", DIV_PERIC4, 0, 4),
	DIV(0, "dout_audio2", "mout_audio2", DIV_PERIC4, 16, 4),

	DIV(0, "div_aclk166", "mout_aclk166", DIV_TOP0, 8, 3),
	DIV_A(0, "div_aclk200", "mout_aclk200", DIV_TOP0, 12, 3, "aclk200"),
	DIV(0, "div_aclk266", "sclk_mpll_muxed", DIV_TOP0, 16, 3),
	DIV(0, "div_aclk333", "mout_aclk333", DIV_TOP0, 20, 3),
	DIV(0, "div_aclk400", "mout_aclk400", DIV_TOP0, 24, 3),
	DIV(0, "div_aclk300_gscl", "sclk_dpll", DIV_TOP2, 8, 3),
	DIV(0, "div_aclk300_disp0", "sclk_dpll", DIV_TOP2, 12, 3),
	DIV_A(0, "div_aclk300_disp1", "sclk_dpll", DIV_TOP2, 16, 3,
		"daclk300disp1"),
	DIV(0, "div_aclk300_jpeg", "sclk_dpll", DIV_TOP2, 17, 3),
	DIV(CLK_DIV_HDMI_PIXEL, "div_hdmi_pixel", "sclk_vpll",
		DIV_DISP1_0, 28, 4),
	DIV(0, "div_fimd1", "mout_fimd1", DIV_DISP1_0, 0, 4),
};

static struct samsung_gate_clock exynos5410_gate_clks[] __initdata = {
	GATE(CLK_MCT, "mct", "div_aclk66", GATE_IP_PERIS, 18, 0, 0),
	GATE(CLK_TMU_APBIF, "tmu_apbif", "div_aclk66",
		GATE_IP_PERIS, 21, CLK_IGNORE_UNUSED, 0),

	GATE(CLK_SCLK_MMC0, "sclk_mmc0", "div_mmc_pre0",
			SRC_MASK_FSYS, 0, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_MMC1, "sclk_mmc1", "div_mmc_pre1",
			SRC_MASK_FSYS, 4, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_MMC2, "sclk_mmc2", "div_mmc_pre2",
			SRC_MASK_FSYS, 8, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_USBD300, "sclk_usbd300", "div_usbd300",
		GATE_TOP_SCLK_FSYS, 9, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_USBD301, "sclk_usbd301", "div_usbd301",
		GATE_TOP_SCLK_FSYS, 10, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_FIMD1, "sclk_fimd1", "div_fimd1",
		GATE_TOP_SCLK_DISP1, 0, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_HDMI, "sclk_hdmi", "mout_hdmi",
		GATE_TOP_SCLK_DISP1, 9, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_PIXEL, "sclk_pixel", "div_hdmi_pixel",
		GATE_TOP_SCLK_DISP1, 10, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_DP1, "sclk_dp1", "mout_aclk300_disp0",
		GATE_TOP_SCLK_DISP1, 20, CLK_SET_RATE_PARENT, 0),

	GATE(CLK_PDMA0, "pdma0", "div_aclk200", GATE_IP_FSYS, 1, 0, 0),
	GATE(CLK_PDMA1, "pdma1", "div_aclk200", GATE_IP_FSYS, 2, 0, 0),

	GATE(CLK_MDMA0, "mdma0", "div_aclk266", GATE_IP_ACP, 1, 0, 0),
	GATE(CLK_MDMA1, "mdma1", "div_aclk266", GATE_IP_GEN, 4, 0, 0),

	GATE(CLK_MMC0, "sdmmc0", "div_aclk200", GATE_BUS_FSYS0, 12, 0, 0),
	GATE(CLK_MMC1, "sdmmc1", "div_aclk200", GATE_BUS_FSYS0, 13, 0, 0),
	GATE(CLK_MMC2, "sdmmc2", "div_aclk200", GATE_BUS_FSYS0, 14, 0, 0),

	GATE(CLK_UART0, "uart0", "div_aclk66", GATE_IP_PERIC, 0, 0, 0),
	GATE(CLK_UART1, "uart1", "div_aclk66", GATE_IP_PERIC, 1, 0, 0),
	GATE(CLK_UART2, "uart2", "div_aclk66", GATE_IP_PERIC, 2, 0, 0),
	GATE(CLK_UART3, "uart3", "div_aclk66", GATE_IP_PERIC, 3, 0, 0),

	GATE(CLK_I2C0, "i2c0", "div_aclk66", GATE_IP_PERIC, 6, 0, 0),
	GATE(CLK_I2C1, "i2c1", "div_aclk66", GATE_IP_PERIC, 7, 0, 0),
	GATE(CLK_I2C2, "i2c2", "div_aclk66", GATE_IP_PERIC, 8, 0, 0),
	GATE(CLK_I2C3, "i2c3", "div_aclk66", GATE_IP_PERIC, 9, 0, 0),
	GATE(CLK_I2C4, "i2c4", "div_aclk66", GATE_IP_PERIC, 10, 0, 0),
	GATE(CLK_I2C5, "i2c5", "div_aclk66", GATE_IP_PERIC, 11, 0, 0),
	GATE(CLK_I2C6, "i2c6", "div_aclk66", GATE_IP_PERIC, 12, 0, 0),
	GATE(CLK_I2C7, "i2c7", "div_aclk66", GATE_IP_PERIC, 13, 0, 0),
	GATE(CLK_I2C_HDMI, "i2c_hdmi", "div_aclk66", GATE_IP_PERIC, 14, 0, 0),

	/* Maudio Block */
	/* Not even sure if this is correct for exynos5410. */
	GATE(CLK_SCLK_MAUDIO0, "sclk_maudio0", "dout_maudio0",
		GATE_TOP_SCLK_MAU, 0, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_MAUPCM0, "sclk_maupcm0", "dout_maupcm0",
		GATE_TOP_SCLK_MAU, 1, CLK_SET_RATE_PARENT, 0),

	GATE(CLK_I2S1, "i2s1", "div_aclk66", GATE_IP_PERIC, 20, 0, 0),
	GATE(CLK_I2S2, "i2s2", "div_aclk66", GATE_IP_PERIC, 21, 0, 0),

	GATE_A(CLK_CHIPID, "chipid", "div_aclk66", GATE_IP_PERIS, 0, 0, 0,
		"chipid"),

	/* Copied from exynos5420, but again this might also
	 * be wrong.
	 */
	GATE(CLK_SCLK_SPDIF, "sclk_spdif", "mout_spdif",
		GATE_TOP_SCLK_PERIC, 9, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_PCM1, "sclk_pcm1", "dout_pcm1",
		GATE_TOP_SCLK_PERIC, 15, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_PCM2, "sclk_pcm2", "dout_pcm2",
		GATE_TOP_SCLK_PERIC, 16, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_I2S1, "sclk_i2s1", "dout_i2s1",
		GATE_TOP_SCLK_PERIC, 17, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_I2S2, "sclk_i2s2", "dout_i2s2",
		GATE_TOP_SCLK_PERIC, 18, CLK_SET_RATE_PARENT, 0),

	GATE(CLK_SCLK_UART0, "sclk_uart0", "div_uart0",
			SRC_MASK_PERIC0, 0, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_UART1, "sclk_uart1", "div_uart1",
			SRC_MASK_PERIC0, 4, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_UART2, "sclk_uart2", "div_uart2",
			SRC_MASK_PERIC0, 8, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_SCLK_UART3, "sclk_uart3", "div_uart3",
			SRC_MASK_PERIC0, 12, CLK_SET_RATE_PARENT, 0),
	GATE(CLK_FIMD1, "fimd1", "mout_aclk300_disp1", GATE_IP_DISP1, 0, 0, 0),
	GATE(CLK_MIE1, "mie1", "mout_aclk300_disp1", GATE_IP_DISP1, 1, 0, 0),
	GATE(CLK_DSIM1, "dsim1", "mout_aclk300_disp1", GATE_IP_DISP1, 3, 0, 0),
	GATE(CLK_DP, "dp", "div_aclk200", GATE_IP_DISP1, 4, 0, 0),
	GATE(CLK_MIXER, "mixer", "mout_aclk200_disp1", GATE_IP_DISP1, 5, 0, 0),
	GATE(CLK_HDMI, "hdmi", "div_aclk66", GATE_IP_DISP1, 6, 0, 0),
	GATE(CLK_GSCL0, "gscl0", "mout_aclk300_disp1", GATE_IP_GSCL0, 0, 0, 0),
	GATE(CLK_GSCL1, "gscl1", "mout_aclk300_disp1", GATE_IP_GSCL0, 1, 0, 0),
	GATE(CLK_GSCL2, "gscl2", "mout_aclk300_disp1", GATE_IP_GSCL0, 1, 0, 0),
	GATE(CLK_GSCL3, "gscl3", "mout_aclk300_disp1", GATE_IP_GSCL0, 1, 0, 0),
	GATE(CLK_GSCL4, "gscl4", "mout_aclk300_disp1", GATE_IP_GSCL1, 15, 0, 0),
	GATE(CLK_MFC, "mfc", "mout_aclk333_sub", GATE_IP_MFC, 0, 0, 0),
	GATE(CLK_SMMU_MFCL, "smmu_mfcl", "mout_aclk333_sub", GATE_IP_MFC, 1, 0, 0),
	GATE(CLK_SMMU_MFCR, "smmu_mfcr", "mout_aclk333_sub", GATE_IP_MFC, 2, 0, 0),
	GATE(CLK_PWM, "pwm", "div_aclk66", GATE_IP_PERIC, 24, 0, 0),
};

static struct samsung_pll_clock exynos5410_plls[nr_plls] __initdata = {
	[apll] = PLL(pll_35xx, CLK_FOUT_APLL, "fout_apll", "fin_pll", APLL_LOCK,
		APLL_CON0, apll_tbl),
	[cpll] = PLL(pll_35xx, CLK_FOUT_CPLL, "fout_cpll", "fin_pll", CPLL_LOCK,
		CPLL_CON0, cpll_tbl),
	[mpll] = PLL(pll_35xx, CLK_FOUT_MPLL, "fout_mpll", "fin_pll", MPLL_LOCK,
		MPLL_CON0, NULL),
	[bpll] = PLL(pll_35xx, CLK_FOUT_BPLL, "fout_bpll", "fin_pll", BPLL_LOCK,
		BPLL_CON0, NULL),
	[kpll] = PLL(pll_35xx, CLK_FOUT_KPLL, "fout_kpll", "fin_pll", KPLL_LOCK,
		KPLL_CON0, kpll_tbl),
	[vpll] = PLL(pll_36xx, CLK_FOUT_VPLL, "fout_vpll", "mout_vpllsrc",
		VPLL_LOCK, VPLL_CON0, vpll_tbl),
	[dpll] = PLL(pll_35xx, CLK_FOUT_DPLL, "fout_dpll", "fin_pll", DPLL_LOCK,
		DPLL_CON0, dpll_tbl),
	[epll] = PLL(pll_36xx, CLK_FOUT_EPLL, "fout_epll", "fin_pll", EPLL_LOCK,
		EPLL_CON0, epll_tbl),
	[ipll] = PLL(pll_35xx, CLK_FOUT_IPLL, "fout_ipll", "fin_pll", IPLL_LOCK,
		IPLL_CON0, ipll_tbl),
};




/* Auxiliary function to set the parent of a clock */
static __init int set_parent_by_name(const char *cname, const char *pname)
{
	struct clk *child_p = NULL;
	struct clk *parent_p = NULL;
	int ret = -1;

	child_p = clk_get(NULL, cname);
	parent_p = clk_get(NULL, pname);

	if (!IS_ERR(child_p) && !IS_ERR(parent_p)) {
		ret = clk_prepare(child_p);
		if (ret == 0) {
			pr_debug ("setting parent of `%s' to `%s'\n",
				cname, pname);

			ret = clk_set_parent(child_p, parent_p);
			if (ret < 0) {
				pr_err("could not set parent `%s' -> `%s'\n",
					cname, pname);
			}
			clk_unprepare(child_p);
		} else {
			pr_err("could not prepare child `%s': %d\n",
				cname, ret);
		}
		clk_put(child_p);
		clk_put(parent_p);
	} else {
		if (IS_ERR(child_p)) {
			pr_err("child lookup `%s' failed: %d\n",
				cname, (int) child_p);
		} else {
			clk_put(child_p);
		}
		if (IS_ERR(parent_p)) {
			pr_err("parent lookup `%s' failed: %d\n",
				pname, (int) parent_p);
		} else {
			clk_put(parent_p);
		}
	}
	return ret;
}

/* Auxiliary function to set the clock rate */
static __init int set_rate_by_name(const char *name, unsigned long rate)
{
	struct clk *clock_p = NULL;
	int ret = -1;

	clock_p = clk_get(NULL, name);

	if (!IS_ERR(clock_p)) {
		ret = clk_prepare(clock_p);
		if (ret == 0) {
			pr_debug ("setting rate of `%s' to %lu\n",
				name, rate);

			ret = clk_set_rate(clock_p, rate);
			if (ret < 0) {
				pr_err("could not set rate `%s' to %lu\n",
					name, rate);
			}
			clk_unprepare(clock_p);
		} else {
			pr_err("could not prepare clock `%s': %d\n",
				name, ret);
		}
		clk_put(clock_p);
	} else {
		pr_err("clock lookup `%s' failed: %d\n",
			name, (int) clock_p);
	}
	return ret;
}

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "samsung,clock-xxti", .data = (void *)0, },
	{ },
};

/* register exynos5410 clocks */
static void __init exynos5410_clk_init(struct device_node *np)
{
	void __iomem *reg_base;

	reg_base = of_iomap(np, 0);
	if (!reg_base)
		panic("%s: failed to map registers\n", __func__);

	samsung_clk_init(np, reg_base, CLK_NR_CLKS,
			exynos5410_clk_regs, ARRAY_SIZE(exynos5410_clk_regs),
			NULL, 0);
	samsung_clk_of_register_fixed_ext(exynos5410_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos5410_fixed_rate_ext_clks),
			ext_clk_match);
	samsung_clk_register_mux(exynos5410_pll_pmux_clks,
				ARRAY_SIZE(exynos5410_pll_pmux_clks));
	samsung_clk_register_pll(exynos5410_plls, ARRAY_SIZE(exynos5410_plls),
					reg_base);

	samsung_clk_register_fixed_rate(exynos5410_fixed_rate_clks,
			ARRAY_SIZE(exynos5410_fixed_rate_clks));
	samsung_clk_register_mux(exynos5410_mux_clks,
			ARRAY_SIZE(exynos5410_mux_clks));
	samsung_clk_register_div(exynos5410_div_clks,
			ARRAY_SIZE(exynos5410_div_clks));
	samsung_clk_register_gate(exynos5410_gate_clks,
			ARRAY_SIZE(exynos5410_gate_clks));


	/* Initialize VPLL, DPLL and some other associated clocks
	 * I've added this initialization code here because I do not
	 * know where else to put it.
	 */
	set_parent_by_name("sclk_vpll", "mout_vpllsrc");
	set_rate_by_name("fout_vpll", 350000000);
	set_parent_by_name("sclk_vpll", "fout_vpll");

	set_rate_by_name("fout_dpll", 600000000);
	set_parent_by_name("aclk200_disp1", "aclk200");
	set_parent_by_name("sclk_dpll", "fout_dpll");
	set_parent_by_name("aclk300_disp1", "daclk300disp1");


	pr_debug("Exynos5410: clock setup completed.\n");
}
CLK_OF_DECLARE(exynos5410_clk, "samsung,exynos5410-clock", exynos5410_clk_init);
