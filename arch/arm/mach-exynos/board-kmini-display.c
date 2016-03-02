/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/lcd.h>
#include <linux/clk.h>

#include <plat/backlight.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-serial.h>
#include <plat/fb.h>
#include <plat/fb-core.h>
#include <plat/regs-fb.h>
#include <plat/backlight.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_FB_MIPI_DSIM
#include <plat/dsim.h>
#include <plat/mipi_dsi.h>
#endif

#include <mach/map.h>
#include <mach/spi-clocks.h>

#include <video/platform_lcd.h>

#define SMDK4270_HBP		18
#define SMDK4270_HFP		20
#define SMDK4270_VBP		8
#define SMDK4270_VFP		14
#define SMDK4270_HSP		4
#define SMDK4270_VSW		2
#define SMDK4270_XRES		720
#define SMDK4270_YRES		1280
#define SMDK4270_VIRTUAL_X	720
#define SMDK4270_VIRTUAL_Y	1280 * 2
#define SMDK4270_WIDTH		56
#define SMDK4270_HEIGHT		99
#define SMDK4270_MAX_BPP	32
#define SMDK4270_DEFAULT_BPP	24

static struct s3c_fb_pd_win smdk4270_fb_win0 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win1 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win2 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win3 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static struct s3c_fb_pd_win smdk4270_fb_win4 = {
	.win_mode = {
		.left_margin	= SMDK4270_HBP,
		.right_margin	= SMDK4270_HFP,
		.upper_margin	= SMDK4270_VBP,
		.lower_margin	= SMDK4270_VFP,
		.hsync_len	= SMDK4270_HSP,
		.vsync_len	= SMDK4270_VSW,
		.xres		= SMDK4270_XRES,
		.yres		= SMDK4270_YRES,
	},
	.virtual_x		= SMDK4270_VIRTUAL_X,
	.virtual_y		= SMDK4270_VIRTUAL_Y,
	.width			= SMDK4270_WIDTH,
	.height			= SMDK4270_HEIGHT,
	.max_bpp		= SMDK4270_MAX_BPP,
	.default_bpp		= SMDK4270_DEFAULT_BPP,
};

static int mipi_lcd_power_control(struct mipi_dsim_device *dsim,
		unsigned int power)
{
	/* reset */
	gpio_request_one(EXYNOS4_GPX2(4), GPIOF_OUT_INIT_HIGH, "GPX2");
	usleep_range(20000, 21000);
	if (power) {
		gpio_set_value(EXYNOS4_GPX2(4), 0);
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS4_GPX2(4), 1);
		msleep(50);
		gpio_free(EXYNOS4_GPX2(4));
	} else {
		gpio_set_value(EXYNOS4_GPX2(4), 0);
		usleep_range(20000, 21000);
		gpio_set_value(EXYNOS4_GPX2(4), 1);
		usleep_range(20000, 21000);
		gpio_free(EXYNOS4_GPX2(4));
	}
	usleep_range(20000, 21000);
	return 1;
}

static void s5p_mipi_dsi_bl_on_off(unsigned int power)
{
}

static void s5p_mipi_dsi_lcd_reset(void)
{
	gpio_request_one(EXYNOS4_GPX2(4), GPIOF_OUT_INIT_HIGH, "GPX2");
	usleep_range(5000, 5000);
	gpio_set_value(EXYNOS4_GPX2(4), 0);
	usleep_range(5000, 5000);
	gpio_set_value(EXYNOS4_GPX2(4), 1);
	msleep(100);
	gpio_free(EXYNOS4_GPX2(4));
}

static void s5p_mipi_dsi_lcd_power_on_off(unsigned int power)
{
	struct regulator *regulator_vlcd_1v8;
	struct regulator *regulator_vlcd_3v0;

	regulator_vlcd_1v8 = regulator_get(NULL, "vlcd_1v8");
	if (IS_ERR(regulator_vlcd_1v8)) {
		pr_info("%s: failed to get %s\n", __func__, "vlcd_1v8");
		return;
	}
	regulator_vlcd_3v0 = regulator_get(NULL, "vlcd_3v0");
	if (IS_ERR(regulator_vlcd_3v0)) {
		pr_info("%s: failed to get %s\n", __func__, "vlcd_3v0");
		return;
	}
	if (power){
		regulator_enable(regulator_vlcd_1v8);
		msleep(10);
		regulator_enable(regulator_vlcd_3v0);
	}else {
		regulator_disable(regulator_vlcd_3v0);
		regulator_disable(regulator_vlcd_1v8);
		/*LCD RESET low*/
		gpio_request_one(EXYNOS4_GPX2(4),GPIOF_OUT_INIT_LOW, "GPX2");
		gpio_free(EXYNOS4_GPX2(4));
	}
	regulator_put(regulator_vlcd_3v0);
	regulator_put(regulator_vlcd_1v8);
	msleep(1);
}

static void mipi_lcd_set_power(struct plat_lcd_data *pd,
		unsigned int power)
{
}

static struct plat_lcd_data smdk4270_mipi_lcd_data = {
	.set_power	= mipi_lcd_set_power,
};

static struct platform_device smdk4270_mipi_lcd = {
	.name			= "platform-lcd",
	.dev.platform_data	= &smdk4270_mipi_lcd_data,
};

static void exynos_fimd_gpio_setup_24bpp(void)
{
	unsigned int reg = 0;
	/*
	 * Set DISP1BLK_CFG register for Display path selection
	 * ---------------------
	 *  0 | MIE/MDNIE
	 *  1 | FIMD : selected
	 */
	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg &= ~(1 << 1);
	reg |= (1 << 1);
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
}

static struct s3c_fb_platdata smdk4270_lcd0_pdata __initdata = {
	.win[0]		= &smdk4270_fb_win0,
	.win[1]		= &smdk4270_fb_win1,
	.win[2]		= &smdk4270_fb_win2,
	.win[3]		= &smdk4270_fb_win3,
	.win[4]		= &smdk4270_fb_win4,
	.default_win	= 0,
	.vidcon0	= VIDCON0_VIDOUT_RGB | VIDCON0_PNRMODE_RGB,
	.vidcon1	= VIDCON1_INV_VCLK,
	.setup_gpio	= exynos_fimd_gpio_setup_24bpp,
	.backlight_ctrl = s5p_mipi_dsi_bl_on_off,
	.dsim_on        = s5p_mipi_dsi_enable_by_fimd,
	.dsim_off       = s5p_mipi_dsi_disable_by_fimd,
	.dsim_displayon	= s5p_mipi_dsi_displayon_by_fimd,
	.dsim0_device   = &s5p_device_mipi_dsim0.dev,
	.bootlogo = false,
};

#ifdef CONFIG_FB_MIPI_DSIM
#define DSIM_L_MARGIN	SMDK4270_HBP
#define DSIM_R_MARGIN	SMDK4270_HFP
#define DSIM_UP_MARGIN	SMDK4270_VBP
#define DSIM_LOW_MARGIN	SMDK4270_VFP
#define DSIM_HSYNC_LEN	SMDK4270_HSP
#define DSIM_VSYNC_LEN	SMDK4270_VSW
#define DSIM_WIDTH	SMDK4270_XRES
#define DSIM_HEIGHT	SMDK4270_YRES

static struct mipi_dsim_config dsim_info = {
	.e_interface		= DSIM_VIDEO,
	.e_pixel_format		= DSIM_24BPP_888,
	/* main frame fifo auto flush at VSYNC pulse */
	.auto_flush		= false,
	.eot_disable		= false,
	.auto_vertical_cnt	= false,

	.hse = false,
	.hfp = true,
	.hbp = false,
	.hsa = false,

	.e_no_data_lane = DSIM_DATA_LANE_4,
	.e_byte_clk	= DSIM_PLL_OUT_DIV8,
	.e_burst_mode	= DSIM_BURST,

	/* bit clock 480Hz */
	.p = 4,
	.m = 163,
	.s = 1,

	.esc_clk	= 20 * 1000000, /* escape clk : 20MHz */

	/* stop state holding counter after bta change count 0 ~ 0xfff */
	.stop_holding_cnt	= 0xfff,
	.bta_timeout		= 0xff,		/* bta timeout 0 ~ 0xff */
	.rx_timeout		= 0xffff,	/* lp rx timeout 0 ~ 0xffff */
	.pll_stable_time	= 0xffffff,

	.dsim_ddi_pd = &ea8061v_mipi_lcd_driver,
};

static struct mipi_dsim_lcd_config dsim_lcd_info = {
	.rgb_timing.left_margin		= DSIM_L_MARGIN,
	.rgb_timing.right_margin	= DSIM_R_MARGIN,
	.rgb_timing.upper_margin	= DSIM_UP_MARGIN,
	.rgb_timing.lower_margin	= DSIM_LOW_MARGIN,
	.rgb_timing.hsync_len		= DSIM_HSYNC_LEN,
	.rgb_timing.vsync_len		= DSIM_VSYNC_LEN,
	.rgb_timing.stable_vfp		= 1,
	.rgb_timing.cmd_allow		= 0,
	.cpu_timing.cs_setup		= 0,
	.cpu_timing.wr_setup		= 1,
	.cpu_timing.wr_act		= 0,
	.cpu_timing.wr_hold		= 0,
	.lcd_size.width			= DSIM_WIDTH,
	.lcd_size.height		= DSIM_HEIGHT,
};

static struct s5p_platform_mipi_dsim dsim_platform_data = {
	.clk_name		= "dsim0",
	.dsim_config		= &dsim_info,
	.dsim_lcd_config	= &dsim_lcd_info,
	.mipi_power   = mipi_lcd_power_control,
	.part_reset		= s5p_dsim_part_reset,
	.init_d_phy     	= s5p_dsim_init_d_phy,
	.get_fb_frame_done	= NULL,
	.trigger		= NULL,

	.backlight_on_off	= s5p_mipi_dsi_bl_on_off,
	.lcd_reset		= s5p_mipi_dsi_lcd_reset,
	.lcd_power_on_off	= s5p_mipi_dsi_lcd_power_on_off,

	.delay_for_stabilization = 600,
	.bootlogo = false,
};
#endif

static struct platform_device *smdk4270_display_devices[] __initdata = {
#ifdef CONFIG_FB_MIPI_DSIM
	&smdk4270_mipi_lcd,
	&s5p_device_mipi_dsim0,
#endif
	&s5p_device_fimd0,
};

static int __init setup_early_bootlogo(char *str)
{
	dsim_platform_data.bootlogo = false;
	smdk4270_lcd0_pdata.bootlogo = false;
	return 0;
}
early_param("bootlogo", setup_early_bootlogo);

void __init exynos4_smdk4270_display_init(void)
{
	s5p_dsim0_set_platdata(&dsim_platform_data);

	s5p_fimd0_set_platdata(&smdk4270_lcd0_pdata);

	platform_add_devices(smdk4270_display_devices,
			ARRAY_SIZE(smdk4270_display_devices));

	exynos4_fimd_setup_clock(&s5p_device_fimd0.dev, "sclk_fimd",
			"mout_mpll_user_top", 200 * MHZ);
}
