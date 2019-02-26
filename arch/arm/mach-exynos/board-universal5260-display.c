/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/lcd.h>

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/gpio-cfg.h>

#include <mach/exynos-mipiphy.h>
#include <mach/map.h>
#include <mach/setup-disp-clock.h>

#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#include <mach/regs-pmu.h>
#include <plat/regs-mipidsim.h>
#endif
#include "board-universal5260.h"
#include "display-exynos5260.h"

unsigned int lcdtype;
EXPORT_SYMBOL(lcdtype);
static int __init lcdtype_setup(char *str)
{
	get_option(&str, &lcdtype);
	return 1;
}
__setup("lcdtype=", lcdtype_setup);

phys_addr_t bootloaderfb_start;
EXPORT_SYMBOL(bootloaderfb_start);
phys_addr_t bootloaderfb_size;
EXPORT_SYMBOL(bootloaderfb_size);

static int __init bootloaderfb_start_setup(char *str)
{
	get_option(&str, &bootloaderfb_start);
#if defined(CONFIG_LCD_MIPI_D6EA8061) || defined(CONFIG_LCD_MIPI_S6E8AA0A02) || defined(CONFIG_LCD_MIPI_HX8394)
	bootloaderfb_size = 720 * 1280 * 4;
#else
	bootloaderfb_start = 0;	/* disable for copying bootloaderfb */
	bootloaderfb_size = 0;
#endif
	return 1;
}
__setup("s3cfb.bootloaderfb=", bootloaderfb_start_setup);

#if defined(CONFIG_LCD_MIPI_D6EA8061)
#define DSIM_NO_DATA_LANE	DSIM_DATA_LANE_4
#define DPHY_PLL_P		3	/* 488Mbps */
#define DPHY_PLL_M		61
#define DPHY_PLL_S		1
#elif defined(CONFIG_LCD_MIPI_S6E8AA0A02)
#define DSIM_NO_DATA_LANE	DSIM_DATA_LANE_4
#define DPHY_PLL_P		3	/* 500Mbps */
#define DPHY_PLL_M		125
#define DPHY_PLL_S		2
#elif defined(CONFIG_LCD_MIPI_HX8394)
#define DSIM_NO_DATA_LANE	DSIM_DATA_LANE_4
#define DPHY_PLL_P		3	/* 500Mbps */
#define DPHY_PLL_M		125
#define DPHY_PLL_S		2
#endif

#if defined(CONFIG_LCD_MIPI_D6EA8061)
static struct panel_info display_info = {
	.name = "ea8061",
	.refresh = 59,
	.xres = 720,
	.yres = 1280,
	.hbp = 115,
	.hfp = 26,
	.hsw = 4,
	.vbp = 1,
	.vfp = 13,
	.vsw = 2,
	.width_mm = 68,		/* 68.4 */
	.height_mm = 122,	/* 121.6 */
};

static struct regulator_bulk_data panel_supplies[] = {
	{ .supply = "vcc_lcd_1.8v" },
	{ .supply = "vcc_lcd_3.0v" }
};

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	int ret;

	ret = regulator_bulk_get(NULL, ARRAY_SIZE(panel_supplies), panel_supplies);
	if (ret) {
		pr_err("%s: failed to get regulators: %d\n", __func__, ret);
		return ret;
	}

	if (enable)
		regulator_bulk_enable(ARRAY_SIZE(panel_supplies), panel_supplies);
	else {
		usleep_range(10000, 11000);
#if defined(GPIO_MLCD_RST)
		gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_LOW, "GPD1");
		gpio_free(GPIO_MLCD_RST);
		usleep_range(10000, 11000);
#endif
		regulator_bulk_disable(ARRAY_SIZE(panel_supplies), panel_supplies);
	}

	regulator_bulk_free(ARRAY_SIZE(panel_supplies), panel_supplies);

	return 0;
}

static int reset_lcd(struct lcd_device *ld)
{
#if defined(GPIO_MLCD_RST)
	gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_HIGH, "GPD1");

	gpio_set_value(GPIO_MLCD_RST, 0);
	usleep_range(1000, 2000);
	gpio_set_value(GPIO_MLCD_RST, 1);
	usleep_range(1000, 2000);

	gpio_free(GPIO_MLCD_RST);
#endif

	return 0;
}

static struct lcd_platform_data panel_pdata = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
	.reset_delay = 25000,
	.power_on_delay = 10000,
};
#elif defined(CONFIG_LCD_MIPI_S6E8AA0A02)
static struct panel_info  display_info = {
	.name = "s6e8aa0a02",
	.refresh = 60,
	.limit = 44,
	.xres = 720,
	.yres = 1280,
	.hbp = 14,
	.hfp = 116,
	.hsw = 5,
	.vbp = 1,
	.vfp = 13,
	.vsw = 2,
	.width_mm = 60,		/* 59.76 */
	.height_mm = 106,	/* 106.24 */
};

static struct regulator_bulk_data panel_supplies_evt0[] = {
	{ .supply = "vcc_lcd_3.3v" },
	{ .supply = "led_vdd_3.3v" }
};
static struct regulator_bulk_data panel_supplies[] = {
	{ .supply = "vcc_lcd_3.3v" },
};

extern unsigned int system_rev;

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	int ret;

	if (system_rev < 0x02) {
		ret = regulator_bulk_get(NULL, ARRAY_SIZE(panel_supplies_evt0), panel_supplies_evt0);
		if (ret) {
			pr_err("%s: failed to get regulators: %d\n", __func__, ret);
			return ret;
		}

		if (enable) {
			gpio_request_one(GPIO_LCD_22V_EN, GPIOF_OUT_INIT_HIGH, "GPC4");
			gpio_set_value(GPIO_LCD_22V_EN, 1);
			gpio_free(GPIO_LCD_22V_EN);
			regulator_bulk_enable(ARRAY_SIZE(panel_supplies_evt0), panel_supplies_evt0);
		} else {
#if defined(GPIO_MLCD_RST)
			gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_LOW, "GPD1");
			gpio_free(GPIO_MLCD_RST);
			usleep_range(10000, 11000);
#endif
			gpio_request_one(GPIO_LCD_22V_EN, GPIOF_OUT_INIT_LOW, "GPC4");
			gpio_set_value(GPIO_LCD_22V_EN, 0);
			gpio_free(GPIO_LCD_22V_EN);

			regulator_bulk_disable(ARRAY_SIZE(panel_supplies_evt0), panel_supplies_evt0);
		}

		regulator_bulk_free(ARRAY_SIZE(panel_supplies_evt0), panel_supplies_evt0);

		return 0;
	} else {
		ret = regulator_bulk_get(NULL, ARRAY_SIZE(panel_supplies), panel_supplies);
		if (ret) {
			pr_err("%s: failed to get regulators: %d\n", __func__, ret);
			return ret;
		}

		if (enable) {
			gpio_request_one(GPIO_LCD_22V_EN, GPIOF_OUT_INIT_HIGH, "GPC4");
			gpio_set_value(GPIO_LCD_22V_EN, 1);
			gpio_free(GPIO_LCD_22V_EN);
			regulator_bulk_enable(ARRAY_SIZE(panel_supplies), panel_supplies);
		} else {
#if defined(GPIO_MLCD_RST)
			gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_LOW, "GPD1");
			gpio_free(GPIO_MLCD_RST);
			usleep_range(10000, 11000);
#endif
			gpio_request_one(GPIO_LCD_22V_EN, GPIOF_OUT_INIT_LOW, "GPC4");
			gpio_set_value(GPIO_LCD_22V_EN, 0);
			gpio_free(GPIO_LCD_22V_EN);

			regulator_bulk_disable(ARRAY_SIZE(panel_supplies), panel_supplies);
		}
		regulator_bulk_free(ARRAY_SIZE(panel_supplies), panel_supplies);

		return 0;
	}
}

static int reset_lcd(struct lcd_device *ld)
{
#if defined(GPIO_MLCD_RST)
	gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_HIGH, "GPD1");

	gpio_set_value(GPIO_MLCD_RST, 0);
	usleep_range(1000, 2000);
	gpio_set_value(GPIO_MLCD_RST, 1);
	usleep_range(1000, 2000);

	gpio_free(GPIO_MLCD_RST);
#endif

	return 0;
}

static struct lcd_platform_data panel_pdata = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
	.reset_delay = 10000,
	.power_on_delay = 25000,
};
#elif defined(CONFIG_LCD_MIPI_HX8394)
static struct panel_info  display_info = {
	.name = "hx8394",
	.refresh = 60,
	.xres = 720,
	.yres = 1280,
	.hbp = 62,
	.hfp = 28,
	.hsw = 36,
	.vbp = 20,
	.vfp = 9,
	.vsw = 2,
	.width_mm = 60,		/* 59.76 */
	.height_mm = 106,	/* 106.24 */
};

static struct regulator_bulk_data panel_supplies[] = {
	{ .supply = "vcc_lcd_1.8v" },
	{ .supply = "vcc_lcd_3.0v" }
};

static int lcd_power_on(struct lcd_device *ld, int enable)
{
	int ret;

	ret = regulator_bulk_get(NULL, ARRAY_SIZE(panel_supplies), panel_supplies);
	if (ret) {
		pr_err("%s: failed to get regulators: %d\n", __func__, ret);
		return ret;
	}

	if (enable)
		regulator_bulk_enable(ARRAY_SIZE(panel_supplies), panel_supplies);
	else {
#if defined(GPIO_MLCD_RST)
		gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_LOW, "GPD1");
		gpio_free(GPIO_MLCD_RST);
		usleep_range(10000, 11000);
#endif
		regulator_bulk_disable(ARRAY_SIZE(panel_supplies), panel_supplies);
	}

	regulator_bulk_free(ARRAY_SIZE(panel_supplies), panel_supplies);

	return 0;
}

static int reset_lcd(struct lcd_device *ld)
{
#if defined(GPIO_MLCD_RST)
	gpio_request_one(GPIO_MLCD_RST, GPIOF_OUT_INIT_HIGH, "GPD1");

	gpio_set_value(GPIO_MLCD_RST, 0);
	usleep_range(1000, 2000);
	gpio_set_value(GPIO_MLCD_RST, 1);
	usleep_range(1000, 2000);

	gpio_free(GPIO_MLCD_RST);
#endif

	return 0;
}

static struct lcd_platform_data panel_pdata = {
	.reset = reset_lcd,
	.power_on = lcd_power_on,
	.reset_delay = 25000,
	.power_on_delay = 10000,
};

#endif

#ifdef CONFIG_FB_MIPI_DSIM
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,

	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	= false,
	.eot_disable	= true,

	.auto_vertical_cnt = false,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_NO_DATA_LANE,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = DPHY_PLL_P,
	.m = DPHY_PLL_M,
	.s = DPHY_PLL_S,

	.pll_stable_time = DPHY_PLL_STABLE_TIME,

	.esc_clk = 16 * MHZ,

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 1,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */
};

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing = {
		.stable_vfp		= 2,
		.cmd_allow		= 4
	},
	.cpu_timing = {
		.cs_setup		= 0,
		.wr_setup		= 1,
		.wr_act			= 0,
		.wr_hold		= 0
	},
	.mipi_ddi_pd			= &panel_pdata,
};

static struct regulator_bulk_data mipi_supplies[] = {
	{ .supply = "vdd10_mipi" },
	{ .supply = "vdd18_mipi" }
};

static int mipi_power_control(struct mipi_dsim_device *dsim, unsigned int enable)
{
	int ret;

	ret = regulator_bulk_get(NULL, ARRAY_SIZE(mipi_supplies), mipi_supplies);
	if (ret) {
		pr_err("%s: failed to get regulators: %d\n", __func__, ret);
		return ret;
	}

	if (enable)
		regulator_bulk_enable(ARRAY_SIZE(mipi_supplies), mipi_supplies);
	else
		regulator_bulk_disable(ARRAY_SIZE(mipi_supplies), mipi_supplies);

	regulator_bulk_free(ARRAY_SIZE(mipi_supplies), mipi_supplies);

	return 0;
}

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim1",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.mipi_power		= mipi_power_control,
	.init_d_phy		= exynos_dsim_phy_enable,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,
	.clock_init		= mipi_dsi_clock_init,
};
#endif

static void universal5260_fimd_gpio_setup_24bpp(void)
{
#ifndef CONFIG_FB_MIPI_DSIM	/* should be fixed with RGB(?) feature */
	gpio_request(EXYNOS5260_GPX0(7), "GPX0");
	s3c_gpio_cfgpin(EXYNOS5260_GPX0(7), S3C_GPIO_SFN(3));
#endif
}

static struct s3c_fb_pd_win exynos_fb_info = {
	.max_bpp		= 32,
	.default_bpp		= 24,
};

static struct s3c_fb_platdata universal5260_lcd1_pdata __initdata = {
	.win[0]		= &exynos_fb_info,
	.win[1]		= &exynos_fb_info,
	.win[2]		= &exynos_fb_info,
	.win[3]		= &exynos_fb_info,
	.win[4]		= &exynos_fb_info,
	.default_win	= 0,
	.clock_init	= s3c_fb_clock_init,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_VCLK,
	.setup_gpio	= universal5260_fimd_gpio_setup_24bpp,
	.ip_version	= EXYNOS5_813,
};

static struct platform_device *universal5260_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
};

#ifdef CONFIG_FB_MIPI_DSIM
static void __init exynos5_setup_dsi_panel_info(void)
{
#if defined(CONFIG_LCD_MIPI_D6EA8061)
	dsim_info.dsim_ddi_pd = &d6ea8061_mipi_lcd_driver;
#elif defined(CONFIG_LCD_MIPI_S6E8AA0A02)
	dsim_info.dsim_ddi_pd = &s6e8aa0a02_mipi_lcd_driver;
#elif defined(CONFIG_LCD_MIPI_HX8394)
	dsim_info.dsim_ddi_pd = &hx8394_mipi_lcd_driver;
#endif
}

static void __init exynos5_setup_dsi_info(struct mipi_dsim_lcd_config *dsi_info, struct panel_info *panel)
{
	dsi_info->rgb_timing.left_margin = panel->hbp;
	dsi_info->rgb_timing.right_margin = panel->hfp;
	dsi_info->rgb_timing.hsync_len = panel->hsw;
	dsi_info->rgb_timing.upper_margin = panel->vbp;
	dsi_info->rgb_timing.lower_margin = panel->vfp;
	dsi_info->rgb_timing.vsync_len = panel->vsw;

	dsi_info->lcd_size.width = panel->xres;
	dsi_info->lcd_size.height = panel->yres;
}
#endif

static void __init exynos5_setup_fb_info(struct s3c_fb_pd_win *fb, struct panel_info *panel)
{
	fb->win_mode.refresh = panel->refresh;
	fb->win_mode.xres = panel->xres;
	fb->win_mode.yres = panel->yres;
	fb->win_mode.left_margin = panel->hbp;
	fb->win_mode.right_margin = panel->hfp;
	fb->win_mode.hsync_len = panel->hsw;
	fb->win_mode.upper_margin = panel->vbp;
	fb->win_mode.lower_margin = panel->vfp;
	fb->win_mode.vsync_len = panel->vsw;
	fb->virtual_x = panel->xres;
	fb->virtual_y = panel->yres * 2;	/* should be fixed, check panstep */
	fb->width = panel->width_mm;
	fb->height = panel->height_mm;
}

#if defined(CONFIG_FB_LCD_FREQ_SWITCH)
struct platform_device lcdfreq_device = {
		.name		= "lcdfreq",
		.id		= -1,
		.dev		= {
			.parent	= &s5p_device_fimd1.dev,
		},
};

static void __init lcdfreq_device_register(void)
{
	int ret;

	lcdfreq_device.dev.platform_data = (void *)display_info.limit;
	ret = platform_device_register(&lcdfreq_device);
	if (ret)
		pr_err("failed to register %s: %d\n", __func__, ret);
}
#endif

void __init exynos5_universal5260_display_init(void)
{
	struct resource *res;
	u32 reg;

	exynos5_setup_fb_info(&exynos_fb_info, &display_info);
#ifdef CONFIG_FB_MIPI_DSIM
	exynos5_setup_dsi_info(&dsim_lcd_info, &display_info);
	exynos5_setup_dsi_panel_info();
	s5p_dsim1_set_platdata(&dsim_platform_data);
	reg = readl(EXYNOS5260_DPTX_PHY_CONTROL);
	reg &= ~EXYNOS5260_DPTX_PHY_ENABLE;
	writel(reg, EXYNOS5260_DPTX_PHY_CONTROL);
#endif

	clk_add_alias("sclk_fimd", "exynos5-fb.1", "sclk_fimd1_128_extclkpl", &s5p_device_fimd1.dev);
	s5p_fimd1_set_platdata(&universal5260_lcd1_pdata);
	platform_add_devices(universal5260_display_devices, ARRAY_SIZE(universal5260_display_devices));

	exynos_fimd_set_rate(&s5p_device_fimd1.dev, "sclk_fimd", "sclk_disp_pixel", &display_info);
#if !defined(CONFIG_S5P_LCD_INIT)
	exynos5_keep_disp_clock(&s5p_device_fimd1.dev);
#endif
#if defined(CONFIG_FB_LCD_FREQ_SWITCH)
	lcdfreq_device_register();
#endif

	res = platform_get_resource(&s5p_device_fimd1, IORESOURCE_MEM, 1);
	if (res && bootloaderfb_start) {
		res->start = bootloaderfb_start;
		res->end = res->start + bootloaderfb_size - 1;
		pr_info("bootloader fb located at %8X-%8X\n", res->start,
				res->end);
	} else {
		pr_err("failed to find bootloader fb resource\n");
	}
}
