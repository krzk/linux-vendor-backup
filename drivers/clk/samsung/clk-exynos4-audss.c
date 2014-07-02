/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Driver for Exynos4 SoC Audio Subsystem clocks.
*/
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>

#include "clk.h"

#define AUDSS_CLKSRC			0x00
#define AUDSS_CLKDIV			0x04
#define AUDSS_CLKGATE			0x08

/* IP Clock Gate 0 Registers */
#define EXYNOS_AUDSS_CLKGATE_RP		(1 << 0)
#define EXYNOS_AUDSS_CLKGATE_I2SBUS	(1 << 2)
#define EXYNOS_AUDSS_CLKGATE_I2S_SPEC	(1 << 3)
#define EXYNOS_AUDSS_CLKGATE_PCMBUS	(1 << 4)
#define EXYNOS_AUDSS_CLKGATE_PCM_SPEC	(1 << 5)
#define EXYNOS_AUDSS_CLKGATE_GPIO	(1 << 6)
#define EXYNOS_AUDSS_CLKGATE_UART	(1 << 7)
#define EXYNOS_AUDSS_CLKGATE_TIMER	(1 << 8)

#define CLK_MOUT_AUDSS			0
#define CLK_DOUT_RP			1
#define CLK_DOUT_AUD_BUS		2
#define CLK_MOUT_I2S			3
#define CLK_DOUT_I2SCLK0		4
#define CLK_I2S0			5
#define CLK_PCM0			6
#define AUDSS_CLK_MAX			7

static const char *mux_audss_p[] __initconst = {
	"xxti", "fout_epll"
};
static const char *mux_i2s_p[] __initconst = {
	"mout_audss", "iiscdclk0", "sclk_audio0"
};

static struct clk_onecell_data clk_data;
static void __iomem *io_base;
static struct clk *clks[AUDSS_CLK_MAX];

#ifdef CONFIG_PM_SLEEP
static unsigned long reg_save[][2] = {
	{ AUDSS_CLKSRC,  0 },
	{ AUDSS_CLKDIV,  0 },
	{ AUDSS_CLKGATE, 0 },
};

static int samsung_audss_clk_suspend(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reg_save); i++)
		reg_save[i][1] = readl(io_base + reg_save[i][0]);

	return 0;
}

static void samsung_audss_clk_resume(void)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(reg_save); i++)
		writel(reg_save[i][1], io_base + reg_save[i][0]);
}

static struct syscore_ops samsung_audss_clk_syscore_ops = {
	.suspend = samsung_audss_clk_suspend,
	.resume	 = samsung_audss_clk_resume,
};
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_OF
static struct of_device_id audss_of_match[] __initdata = {
	{ .compatible = "samsung,exynos4-audss-clock" },
	{ },
};
#endif

static DEFINE_SPINLOCK(audss_clk_lock);

static int __init samsung_audss_clk_init(void)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, audss_of_match);
	if (!node)
		return -ENODEV;

	io_base = of_iomap(node, 0);
	if (WARN_ON(!io_base))
		return -ENOMEM;

	clks[CLK_MOUT_AUDSS] = clk_register_mux(NULL, "mout_audss",
		mux_audss_p, ARRAY_SIZE(mux_audss_p), CLK_SET_RATE_PARENT,
		io_base + AUDSS_CLKSRC, 0, 1, 0, &audss_clk_lock);
	clks[CLK_MOUT_I2S] = clk_register_mux(NULL, "mout_i2s0",
		mux_i2s_p, ARRAY_SIZE(mux_i2s_p), CLK_SET_RATE_PARENT,
		io_base + AUDSS_CLKSRC, 2, 2, 0, &audss_clk_lock);

	clks[CLK_DOUT_RP] = clk_register_divider(NULL, "dout_rp",
		"mout_audss", CLK_SET_RATE_PARENT, io_base + AUDSS_CLKDIV,
		0, 4, 0, &audss_clk_lock);
	clks[CLK_DOUT_AUD_BUS] = clk_register_divider(NULL, "dout_aud_bus",
		"dout_rp", CLK_SET_RATE_PARENT, io_base + AUDSS_CLKDIV,
		4, 4, 0, &audss_clk_lock);
	clks[CLK_DOUT_I2SCLK0] = clk_register_divider(NULL, "dout_i2s0",
		"mout_i2s0", CLK_SET_RATE_PARENT, io_base + AUDSS_CLKDIV,
		8, 4, 0, &audss_clk_lock);

	/* TODO: Add gate clocks */

	clk_data.clks = clks;
	clk_data.clk_num = ARRAY_SIZE(clks);
	of_clk_add_provider(node, of_clk_src_onecell_get, &clk_data);

	register_syscore_ops(&samsung_audss_clk_syscore_ops);
	return 0;
}
postcore_initcall(samsung_audss_clk_init);
