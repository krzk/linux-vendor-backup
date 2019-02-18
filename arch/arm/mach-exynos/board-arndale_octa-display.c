/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <video/platform_lcd.h>
#include <video/s5p-dp.h>

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/dp.h>
#include <plat/backlight.h>
#include <plat/gpio-cfg.h>

#include <mach/map.h>

#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#endif

#if defined(CONFIG_LCD_MIPI_S6E8AA0)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	if (power) {
		/* Reset */
		gpio_request_one(EXYNOS5420_GPJ4(3),
				GPIOF_OUT_INIT_HIGH, "GPJ4");
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPJ4(3), 0);
		usleep_range(5000, 6000);
		gpio_set_value(EXYNOS5420_GPJ4(3), 1);
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPJ4(3));
		/* Power */
		gpio_request_one(EXYNOS5420_GPH0(0),
				GPIOF_OUT_INIT_HIGH, "GPH0");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPH0(0));
	} else {
		/* Reset */
		gpio_request_one(EXYNOS5420_GPJ4(3),
				GPIOF_OUT_INIT_LOW, "GPJ4");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPJ4(3));
		/* Power */
		gpio_request_one(EXYNOS5420_GPH0(0),
				GPIOF_OUT_INIT_LOW, "GPH0");
		usleep_range(5000, 6000);
		gpio_free(EXYNOS5420_GPH0(0));
	}
	return 1;
}

#define ARNDALE_OCTA_HBP (0x18)
#define ARNDALE_OCTA_HFP (0x18)
#define ARNDALE_OCTA_HFP_DSIM (0x28)
#define ARNDALE_OCTA_HSP (2)
#define ARNDALE_OCTA_VBP (1)
#define ARNDALE_OCTA_VFP (0xD)
#define ARNDALE_OCTA_VSW (2)
#define ARNDALE_OCTA_XRES (1920)
#define ARNDALE_OCTA_YRES (1080)
#define ARNDALE_OCTA_VIRTUAL_X (1920)
#define ARNDALE_OCTA_VIRTUAL_Y (1080 * 2)
#define ARNDALE_OCTA_WIDTH (71)
#define ARNDALE_OCTA_HEIGHT (114)
#define ARNDALE_OCTA_MAX_BPP (32)
#define ARNDALE_OCTA_DEFAULT_BPP (24)

static struct s3c_fb_pd_win arndale_octa_fb_win0 = {
	.win_mode = {
		.left_margin	= ARNDALE_OCTA_HBP,
		.right_margin	= ARNDALE_OCTA_HFP,
		.upper_margin	= ARNDALE_OCTA_VBP,
		.lower_margin	= ARNDALE_OCTA_VFP,
		.hsync_len	= ARNDALE_OCTA_HSP,
		.vsync_len	= ARNDALE_OCTA_VSW,
		.xres		= ARNDALE_OCTA_XRES,
		.yres		= ARNDALE_OCTA_YRES,
	},
	.virtual_x		= ARNDALE_OCTA_VIRTUAL_X,
	.virtual_y		= ARNDALE_OCTA_VIRTUAL_Y,
	.width			= ARNDALE_OCTA_WIDTH,
	.height			= ARNDALE_OCTA_HEIGHT,
	.max_bpp		= ARNDALE_OCTA_MAX_BPP,
	.default_bpp		= ARNDALE_OCTA_DEFAULT_BPP,
};
#elif defined(CONFIG_LCD_MIPI_TC358764)
static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
			unsigned int power)
{
	/* reset */
	gpio_request_one(EXYNOS5_GPX1(7), GPIOF_OUT_INIT_HIGH, "GPX1");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX1(7), 0);
		mdelay(20);
		gpio_set_value(EXYNOS5_GPX1(7), 1);
		mdelay(20);
		gpio_free(EXYNOS5_GPX1(7));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX1(7), 0);
		mdelay(20);
		gpio_set_value(EXYNOS5_GPX1(7), 1);
		mdelay(20);
		gpio_free(EXYNOS5_GPX1(7));
	}   
	mdelay(20);

	/* power */
	gpio_request_one(EXYNOS5_GPX1(4), GPIOF_OUT_INIT_LOW, "GPX1");
	if (power) {
		/* fire nRESET on power up */
		gpio_set_value(EXYNOS5_GPX1(4), 1);
		gpio_free(EXYNOS5_GPX1(4));
	} else {
		/* fire nRESET on power off */
		gpio_set_value(EXYNOS5_GPX1(4), 0);
		gpio_free(EXYNOS5_GPX1(4));
	}

	return 1;
}

#define ARNDALE_OCTA_HBP (0x18)
#define ARNDALE_OCTA_HFP (0x18)
#define ARNDALE_OCTA_HFP_DSIM (0x28)
#define ARNDALE_OCTA_HSP (2)
#define ARNDALE_OCTA_VBP (1)
#define ARNDALE_OCTA_VFP (0xD)
#define ARNDALE_OCTA_VSW (2)
#define ARNDALE_OCTA_XRES (1024)
#define ARNDALE_OCTA_YRES (600)
#define ARNDALE_OCTA_VIRTUAL_X (1024)
#define ARNDALE_OCTA_VIRTUAL_Y (600 * 2)
#define ARNDALE_OCTA_WIDTH (71)
#define ARNDALE_OCTA_HEIGHT (114)
#define ARNDALE_OCTA_MAX_BPP (32)
#define ARNDALE_OCTA_DEFAULT_BPP (24)

static struct s3c_fb_pd_win arndale_octa_fb_win0 = {
	.win_mode = {
		.left_margin	= ARNDALE_OCTA_HBP,
		.right_margin	= ARNDALE_OCTA_HFP,
		.upper_margin	= ARNDALE_OCTA_VBP,
		.lower_margin	= ARNDALE_OCTA_VFP,
		.hsync_len	= ARNDALE_OCTA_HSP,
		.vsync_len	= ARNDALE_OCTA_VSW,
		.xres		= ARNDALE_OCTA_XRES,
		.yres		= ARNDALE_OCTA_YRES,
	},
	.virtual_x		= ARNDALE_OCTA_VIRTUAL_X,
	.virtual_y		= ARNDALE_OCTA_VIRTUAL_Y,
	.width			= ARNDALE_OCTA_WIDTH,
	.height			= ARNDALE_OCTA_HEIGHT,
	.max_bpp		= ARNDALE_OCTA_MAX_BPP,
	.default_bpp		= ARNDALE_OCTA_DEFAULT_BPP,
};
#elif defined(CONFIG_S5P_DP)
static void dp_lcd_set_power(struct plat_lcd_data *pd,
				unsigned int power)
{
	/* LCD_EN: GPH0_7 */
	gpio_request(EXYNOS5420_GPH0(7), "GPH0");

	/* LCD_EN: GPH0_7 */
	gpio_direction_output(EXYNOS5420_GPH0(7), power);

	msleep(90);

	gpio_free(EXYNOS5420_GPH0(7));
#ifndef CONFIG_BACKLIGHT_PWM
	/* LCD_PWM_IN_2.8V: LCD_B_PWM, GPB2_0 */
	gpio_request(EXYNOS5420_GPB2(0), "GPB2");

	gpio_direction_output(EXYNOS5420_GPB2(0), power);

	gpio_free(EXYNOS5420_GPB2(0));
#endif
}

static struct plat_lcd_data arndale_octa_dp_lcd_data = {
	.set_power	= dp_lcd_set_power,
};

static struct platform_device arndale_octa_dp_lcd = {
	.name	= "platform-lcd",
	.dev	= {
		.parent		= &s5p_device_fimd1.dev,
		.platform_data	= &arndale_octa_dp_lcd_data,
	},
};

static struct s3c_fb_pd_win arndale_octa_fb_win0 = {
	.win_mode = {
		.left_margin	= 80,
		.right_margin	= 48,
		.upper_margin	= 37,
		.lower_margin	= 3,
		.hsync_len	= 32,
		.vsync_len	= 6,
		.xres		= 2560,
		.yres		= 1600,
	},
	.virtual_x		= 2560,
	.virtual_y		= 1640 * 2,
	.max_bpp		= 32,
	.default_bpp		= 24,
};
#endif

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;

#if defined(CONFIG_S5P_DP)
	/* Set Hotplug detect for DP */
	gpio_request(EXYNOS5420_GPX0(7), "GPX0");
	s3c_gpio_cfgpin(EXYNOS5420_GPX0(7), S3C_GPIO_SFN(3));
#endif

	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * FIMD of DISP1_BLK Bypass selection : DISP1BLK_CFG[15]
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg &= ~(1 << 15);	/* To save other reset values */
	reg |= (1 << 15);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
#if defined(CONFIG_S5P_DP)
	/* Reference clcok selection for DPTX_PHY: PAD_OSC_IN */
	reg = __raw_readl(S3C_VA_SYS + 0x04d4);
	reg &= ~(1 << 0);
	__raw_writel(reg, S3C_VA_SYS + 0x04d4);

	/* DPTX_PHY: XXTI */
	reg = __raw_readl(S3C_VA_SYS + 0x04d8);
	reg &= ~(1 << 3);
	__raw_writel(reg, S3C_VA_SYS + 0x04d8);
#endif
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 *
	 * MIC of DISP1_BLK Bypass selection: DISP1BLK_CFG[11]
	 * --------------------
	 *  0 | MIC
	 *  1 | Bypass : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0214);
	reg |= (1 << 11);
	__raw_writel(reg, S3C_VA_SYS + 0x0214);
}

static struct s3c_fb_platdata arndale_octa_lcd1_pdata __initdata = {
	.win[0]		= &arndale_octa_fb_win0,
	.win[1]		= &arndale_octa_fb_win0,
	.win[2]		= &arndale_octa_fb_win0,
	.win[3]		= &arndale_octa_fb_win0,
	.win[4]		= &arndale_octa_fb_win0,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
#if defined(CONFIG_S5P_DP)
	.vidcon1	= 0,
#else
	.vidcon1	= VIDCON1_INV_VCLK,
#endif
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.ip_version	= EXYNOS5_813,
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN ARNDALE_OCTA_HBP
#define DSIM_R_MARGIN ARNDALE_OCTA_HFP_DSIM
#define DSIM_UP_MARGIN ARNDALE_OCTA_VBP
#define DSIM_LOW_MARGIN ARNDALE_OCTA_VFP
#define DSIM_HSYNC_LEN ARNDALE_OCTA_HSP
#define DSIM_VSYNC_LEN ARNDALE_OCTA_VSW
#define DSIM_WIDTH ARNDALE_OCTA_XRES
#define DSIM_HEIGHT ARNDALE_OCTA_YRES

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= DSIM_L_MARGIN,
	.rgb_timing.right_margin	= DSIM_R_MARGIN,
	.rgb_timing.upper_margin	= DSIM_UP_MARGIN,
	.rgb_timing.lower_margin	= DSIM_LOW_MARGIN,
	.rgb_timing.hsync_len		= DSIM_HSYNC_LEN,
	.rgb_timing.vsync_len		= DSIM_VSYNC_LEN,
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 1,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
};

#if defined(CONFIG_LCD_MIPI_S6E8AA0)
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	= false,
	.eot_disable	= false,
	.auto_vertical_cnt = true,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 4,
	.m = 80,
	.s = 2,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 7 * 1000000, /* escape clk : 7MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &s6e8aa0_mipi_lcd_driver,
};

#elif defined(CONFIG_LCD_MIPI_TC358764)
static struct mipi_dsim_config dsim_info = {
	.e_interface	= DSIM_VIDEO,
	.e_pixel_format = DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush	= false,
	.eot_disable	= false,
	.auto_vertical_cnt = true,
	.hse = false,
	.hfp = false,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	.p = 4,
	.m = 80,
	.s = 2,

	/* D-PHY PLL stable time spec :min = 200usec ~ max 400usec */
	.pll_stable_time = 500,

	.esc_clk = 7 * 1000000, /* escape clk : 7MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt = 0x0fff,
	.bta_timeout = 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout = 0xffff,		/* lp rx timeout 0 ~ 0xffff */

	.dsim_ddi_pd = &tc358764_mipi_lcd_driver,
};
#endif

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim1",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,

	.mipi_power		= mipi_lcd_power_control,
	.part_reset		= NULL,
	.init_d_phy		= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,

	/*
	 * The stable time of needing to write data on SFR
	 * when the mipi mode becomes LP mode.
	 */
	.delay_for_stabilization = 600,
};
#endif

#ifdef CONFIG_S5P_DP
static struct video_info arndale_octa_dp_config = {
	.name			= "WQXGA(2560x1600) LCD, for SMDK TEST",

	.h_sync_polarity	= 0,
	.v_sync_polarity	= 0,
	.interlaced		= 0,

	.color_space		= COLOR_RGB,
	.dynamic_range		= VESA,
	.ycbcr_coeff		= COLOR_YCBCR601,
	.color_depth		= COLOR_8,

	.link_rate		= LINK_RATE_2_70GBPS,
	.lane_count		= LANE_COUNT4,
};

static void s5p_dp_backlight_on(void)
{
	/* LED_BACKLIGHT_RESET: GPX1_5 */
	gpio_request(EXYNOS5420_GPX1(5), "GPX1");

	gpio_direction_output(EXYNOS5420_GPX1(5), 1);
	usleep_range(20000, 21000);

	gpio_free(EXYNOS5420_GPX1(5));
}

static void s5p_dp_backlight_off(void)
{
	/* LED_BACKLIGHT_RESET: GPX1_5 */
	gpio_request(EXYNOS5420_GPX1(5), "GPX1");

	gpio_direction_output(EXYNOS5420_GPX1(5), 0);
	usleep_range(20000, 21000);

	gpio_free(EXYNOS5420_GPX1(5));
}

static struct s5p_dp_platdata arndale_octa_dp_data __initdata = {
	.video_info	= &arndale_octa_dp_config,
	.phy_init	= s5p_dp_phy_init,
	.phy_exit	= s5p_dp_phy_exit,
	.backlight_on	= s5p_dp_backlight_on,
	.backlight_off	= s5p_dp_backlight_off,
};
#endif

static struct platform_device *arndale_octa_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim1,
#endif
	&s5p_device_fimd1,
#ifdef CONFIG_S5P_DP
	&s5p_device_dp,
	&arndale_octa_dp_lcd,
#endif
};

#ifdef CONFIG_BACKLIGHT_PWM
/* LCD Backlight data */
static struct samsung_bl_gpio_info arndale_octa_bl_gpio_info = {
	.no = EXYNOS5420_GPB2(0),
	.func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data arndale_octa_bl_data = {
	.pwm_id = 0,
	.pwm_period_ns = 30000,
};
#endif

void __init exynos5_arndale_octa_display_init(void)
{
#ifdef CONFIG_FB_MIPI_DSIM
	s5p_dsim1_set_platdata(&dsim_platform_data);
#endif
#ifdef CONFIG_S5P_DP
	s5p_dp_set_platdata(&arndale_octa_dp_data);
#endif
	s5p_fimd1_set_platdata(&arndale_octa_lcd1_pdata);
#ifdef CONFIG_BACKLIGHT_PWM
	samsung_bl_set(&arndale_octa_bl_gpio_info, &arndale_octa_bl_data);
#endif
	platform_add_devices(arndale_octa_display_devices,
			ARRAY_SIZE(arndale_octa_display_devices));
#ifdef CONFIG_S5P_DP
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_rpll", 266 * MHZ);
#endif
#ifdef CONFIG_FB_MIPI_DSIM
	/* RPLL rate is 300Mhz, 300/5=60Hz */
	exynos5_fimd1_setup_clock(&s5p_device_fimd1.dev,
			"sclk_fimd", "mout_fimd1", 67 * MHZ);
#endif
}
