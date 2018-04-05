/* linux/arch/arm/mach-exynos/setup-fimc-is.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/of_gpio.h>
#include "exynos-fimc-is.h"
#include "fimc-is-core.h"

struct platform_device; /* don't need the contents */

/*------------------------------------------------------*/
/*		Common control									*/
/*------------------------------------------------------*/
int exynos_fimc_is_print_cfg(struct device *dev, u32 channel)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* utility function to set parent with DT */
int fimc_is_set_parent_dt(struct device *dev, const char *child,
			  const char *parent)
{
	struct clk *p;
	struct clk *c;

	p = devm_clk_get(dev, parent);
	if (IS_ERR(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c = devm_clk_get(dev, child);
	if (IS_ERR(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	return clk_set_parent(c, p);
}

/* utility function to set rate with DT */
int fimc_is_set_rate_dt(struct device *dev, const char *conid,
			unsigned int rate)
{
	struct clk *target;

	target = devm_clk_get(dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	return clk_set_rate(target, rate);
}

/* utility function to get rate with DT */
unsigned int fimc_is_get_rate_dt(struct device *dev, const char *conid)
{
	struct clk *target;
	unsigned int rate_target;

	target = devm_clk_get(dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	pr_info("%s : %d\n", conid, rate_target);

	return rate_target;
}

/* utility function to eable with DT */
unsigned int fimc_is_enable_dt(struct device *dev, const char *conid)
{
	struct clk *target;

	target = devm_clk_get(dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_prepare(target);

	return clk_enable(target);
}

/* utility function to disable with DT */
void  fimc_is_disable_dt(struct device *dev, const char *conid)
{
	struct clk *target;

	target = devm_clk_get(dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
	}

	clk_disable(target);
	clk_unprepare(target);
}

static int exynos5430_cfg_clk_isp_pll_on(struct device *dev)
{
	pr_info("%s\n", __func__);

	fimc_is_enable_dt(dev, "fout_isp_pll");
	fimc_is_set_parent_dt(dev, "mout_isp_pll", "fout_isp_pll");

	return 0;
}

static int exynos5430_cfg_clk_isp_pll_off(struct device *dev)
{
	pr_info("%s\n", __func__);

	fimc_is_set_parent_dt(dev, "mout_isp_pll", "fin_pll");
	fimc_is_disable_dt(dev, "fout_isp_pll");

	return 0;
}

int exynos5430_cfg_clk_div_max(struct device *dev)
{

	fimc_is_set_parent_dt(dev, "mout_aclk_cam0_552_user", "oscclk");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam0_400_user", "oscclk");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam0_333_user", "oscclk");

	/* MIPI-CSIS PHY */
	fimc_is_set_parent_dt(dev, "mout_phyclk_rxbyteclkhs0_s4", "oscclk");
	fimc_is_set_parent_dt(dev, "mout_phyclk_rxbyteclkhs0_s2a", "oscclk");

	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(dev, "mout_aclk_cam1_552_user", "oscclk");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam1_400_user", "oscclk");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam1_333_user", "oscclk");

	/* MIPI-CSIS PHY */
	fimc_is_set_parent_dt(dev, "mout_phyclk_rxbyteclkhs0_s2b", "oscclk");

	/* SCLK */

	/* SCLK_UART */
	fimc_is_set_parent_dt(dev, "mout_sclk_isp_uart", "oscclk");
	fimc_is_set_rate_dt(dev, "dout_sclk_isp_uart", 1);
	fimc_is_set_parent_dt(dev, "mout_sclk_isp_uart_user", "oscclk");

	/* CAM1 */
	/* C-A5 */
	fimc_is_set_rate_dt(dev, "dout_atclk_cam1", 1);
	fimc_is_set_rate_dt(dev, "dout_pclk_dbg_cam1", 1);

	fimc_is_set_parent_dt(dev, "mout_aclk_isp_400_user", "oscclk");
	fimc_is_set_parent_dt(dev, "mout_aclk_isp_dis_400_user", "oscclk");

	return 0;
}

int exynos5430_cfg_clk_sclk(struct device *dev)
{
	/* SCLK_SPI1 */
	fimc_is_set_parent_dt(dev, "mout_sclk_isp_spi1", "mout_bus_pll_user");
	fimc_is_set_rate_dt(dev, "dout_sclk_isp_spi1_a", 275 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_sclk_isp_spi1_b", 46 * 1000000);
	fimc_is_set_parent_dt(dev, "mout_sclk_isp_spi1_user", "sclk_isp_spi1_top");

	return 0;
}

int exynos5430_cfg_clk_cam0(struct device *dev)
{
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(dev, "mout_aclk_cam0_552_user", "aclk_cam0_552");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam0_400_user", "aclk_cam0_400");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam0_333_user", "aclk_cam0_333");
	fimc_is_set_parent_dt(dev, "mout_phyclk_rxbyteclkhs0_s4", "phyclk_rxbyteclkhs0_s4");
	fimc_is_set_parent_dt(dev, "mout_phyclk_rxbyteclkhs0_s2a", "phyclk_rxbyteclkhs0_s2a");
	fimc_is_set_parent_dt(dev, "mout_phyclk_rxbyteclkhs0_s2b", "phyclk_rxbyteclkhs0_s2b");

	/* LITE A */
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_a_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_a_b", "mout_aclk_lite_a_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_lite_a", 552 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_lite_a", 276 * 1000000);

	/* LITE B */
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_b_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_b_b", "mout_aclk_lite_b_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_lite_b", 552 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_lite_b", 276 * 1000000);

	/* LITE D */
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_d_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_d_b", "mout_aclk_lite_d_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_lite_d", 552 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_lite_d", 276 * 1000000);

	/* LITE C PIXELASYNC */
	fimc_is_set_parent_dt(dev, "mout_sclk_pixelasync_lite_c_init_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_sclk_pixelasync_lite_c_init_b", "mout_sclk_pixelasync_lite_c_init_a");
	fimc_is_set_rate_dt(dev, "dout_sclk_pixelasync_lite_c_init", 552 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_pixelasync_lite_c", 276 * 1000000);

	fimc_is_set_parent_dt(dev, "mout_sclk_pixelasync_lite_c_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_sclk_pixelasync_lite_c_b", "mout_aclk_cam0_333_user");
	fimc_is_set_rate_dt(dev, "dout_sclk_pixelasync_lite_c", 333 * 1000000);

	/* 3AA 0 */
	fimc_is_set_parent_dt(dev, "mout_aclk_3aa0_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_3aa0_b", "mout_aclk_3aa0_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_3aa0", 552 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_3aa0", 276 * 1000000);

	/* 3AA 0 */
	fimc_is_set_parent_dt(dev, "mout_aclk_3aa1_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_3aa1_b", "mout_aclk_3aa1_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_3aa1", 552 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_3aa1", 276 * 1000000);

	/* CSI 0 */
	fimc_is_set_parent_dt(dev, "mout_aclk_csis0_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_csis0_b", "mout_aclk_csis0_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_csis0", 552 * 1000000);

	/* CSI 1 */
	fimc_is_set_parent_dt(dev, "mout_aclk_csis1_a", "mout_aclk_cam0_552_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_csis1_b", "mout_aclk_csis1_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_csis1", 552 * 1000000);

	/* CAM0 400 */
	fimc_is_set_parent_dt(dev, "mout_aclk_cam0_400", "mout_aclk_cam0_400_user");
	fimc_is_set_rate_dt(dev, "dout_aclk_cam0_400", 413 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_aclk_cam0_200", 207 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_cam0_50", 52 * 1000000);

	return 0;
}

int exynos5430_cfg_clk_cam1(struct device *dev)
{
	struct fimc_is_core *core = dev_get_drvdata(dev);
	/*
	 * It's not clear to me why, but the front camera doesn't work properly
	 * (only empty buffers dequeued) when this ACLK_CAM1_552_USER mux is
	 * reconfigured here. So as an ugly workaround this mux is temporarily
	 * left untouched when the front sensor video node is opened.
	 *
	 * FIXME: remove the 'if' below if the root cause is found.
	 */
	/* USER_MUX_SEL */
	if (core->id == 0)
		fimc_is_set_parent_dt(dev, "mout_aclk_cam1_552_user",
				      "aclk_cam1_552");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam1_400_user", "aclk_cam1_400");
	fimc_is_set_parent_dt(dev, "mout_aclk_cam1_333_user", "aclk_cam1_333");

	/* C-A5 */
	fimc_is_set_rate_dt(dev, "dout_atclk_cam1", 276 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_dbg_cam1", 138 * 1000000);

	/* LITE A */
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_c_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_lite_c_b", "mout_aclk_cam1_333_user");
	fimc_is_set_rate_dt(dev, "dout_aclk_lite_c", 333 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_lite_c", 166 * 1000000);

	/* FD */
	fimc_is_set_parent_dt(dev, "mout_aclk_fd_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_fd_b", "mout_aclk_fd_a");
	fimc_is_set_rate_dt(dev, "dout_aclk_fd", 413 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_fd", 207 * 1000000);

	/* CSI 2 */
	fimc_is_set_parent_dt(dev, "mout_aclk_csis2_a", "mout_aclk_cam1_400_user");
	fimc_is_set_parent_dt(dev, "mout_aclk_csis2_b", "mout_aclk_cam1_333_user");
	fimc_is_set_rate_dt(dev, "dout_aclk_csis2_a", 333 * 1000000);

	/* MPWM */
	fimc_is_set_rate_dt(dev, "dout_pclk_cam1_166", 167 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_cam1_83", 84 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_sclk_isp_mpwm", 84 * 1000000);

	/* CAM1 QE CLK GATE */
	fimc_is_enable_dt(dev, "gate_bts_fd");
	fimc_is_disable_dt(dev, "gate_bts_fd");

	return 0;
}

int exynos5430_cfg_clk_cam1_spi(struct device *dev)
{
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(dev, "mout_aclk_cam1_333_user", "aclk_cam1_333");

	/* SPI */
	fimc_is_set_rate_dt(dev, "dout_pclk_cam1_83", 84 * 1000000);

	return 0;
}

int exynos5430_cfg_clk_isp(struct device *dev)
{
	/* CMU_ISP */
	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(dev, "mout_aclk_isp_400_user", "aclk_isp_400");
	fimc_is_set_parent_dt(dev, "mout_aclk_isp_dis_400_user", "aclk_isp_dis_400");
	/* ISP */
	fimc_is_set_rate_dt(dev, "dout_aclk_isp_c_200", 207 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_aclk_isp_d_200", 207 * 1000000);
	fimc_is_set_rate_dt(dev, "dout_pclk_isp", 83 * 1000000);
	/* DIS */
	fimc_is_set_rate_dt(dev, "dout_pclk_isp_dis", 207 * 1000000);

	/* ISP QE CLK GATE */
	fimc_is_enable_dt(dev, "gate_bts_3dnr");
	fimc_is_enable_dt(dev, "gate_bts_dis1");
	fimc_is_enable_dt(dev, "gate_bts_dis0");
	fimc_is_enable_dt(dev, "gate_bts_scalerc");
	fimc_is_enable_dt(dev, "gate_bts_drc");
	fimc_is_disable_dt(dev, "gate_bts_3dnr");
	fimc_is_disable_dt(dev, "gate_bts_dis1");
	fimc_is_disable_dt(dev, "gate_bts_dis0");
	fimc_is_disable_dt(dev, "gate_bts_scalerc");
	fimc_is_disable_dt(dev, "gate_bts_drc");

	return 0;
}

int exynos5430_fimc_is_cfg_clk(struct device *dev)
{
	pr_debug("%s\n", __func__);

	exynos5430_cfg_clk_isp_pll_on(dev);
	exynos5430_cfg_clk_div_max(dev);

	/* initialize Clocks */
	exynos5430_cfg_clk_sclk(dev);
	exynos5430_cfg_clk_cam0(dev);
	exynos5430_cfg_clk_cam1(dev);
	exynos5430_cfg_clk_isp(dev);

	return 0;
}

static int exynos_fimc_is_sensor_iclk_init(struct device *dev)
{
	fimc_is_enable_dt(dev, "aclk_csis0");
	fimc_is_enable_dt(dev, "pclk_csis0");
	fimc_is_enable_dt(dev, "aclk_csis1");
	fimc_is_enable_dt(dev, "pclk_csis1");
	fimc_is_enable_dt(dev, "gate_csis2");
	fimc_is_enable_dt(dev, "gate_lite_a");
	fimc_is_enable_dt(dev, "gate_lite_b");
	fimc_is_enable_dt(dev, "gate_lite_d");
	fimc_is_enable_dt(dev, "gate_lite_c");
	fimc_is_enable_dt(dev, "gate_lite_freecnt");

	return 0;
}

static int exynos_fimc_is_sensor_iclk_deinit(struct device *dev)
{
	fimc_is_disable_dt(dev, "aclk_csis0");
	fimc_is_disable_dt(dev, "pclk_csis0");
	fimc_is_disable_dt(dev, "aclk_csis1");
	fimc_is_disable_dt(dev, "pclk_csis1");
	fimc_is_disable_dt(dev, "gate_csis2");
	fimc_is_disable_dt(dev, "gate_lite_a");
	fimc_is_disable_dt(dev, "gate_lite_b");
	fimc_is_disable_dt(dev, "gate_lite_d");
	fimc_is_disable_dt(dev, "gate_lite_c");
	fimc_is_disable_dt(dev, "gate_lite_freecnt");

	return 0;
}

int exynos5430_fimc_is_clk_on(struct device *dev)
{
	pr_debug("%s\n", __func__);

	exynos_fimc_is_sensor_iclk_init(dev);
	exynos_fimc_is_sensor_iclk_deinit(dev);

	return 0;
}

int exynos5430_fimc_is_clk_off(struct device *dev)
{
	pr_debug("%s\n", __func__);

	exynos5430_cfg_clk_div_max(dev);
	exynos5430_cfg_clk_isp_pll_off(dev);

	return 0;
}

int exynos5430_fimc_is_print_clk(struct device *dev)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos5430_fimc_is_print_pwr(struct device *dev)
{
	pr_debug("%s\n", __func__);

	return 0;
}


/* Wrapper functions */
int exynos_fimc_is_cfg_clk(struct device *dev)
{
	exynos5430_fimc_is_cfg_clk(dev);
	return 0;
}

int exynos_fimc_is_cfg_cam_clk(struct device *dev)
{
	exynos5430_cfg_clk_sclk(dev);
	exynos5430_cfg_clk_cam1_spi(dev);
	return 0;
}

int exynos_fimc_is_clk_on(struct device *dev)
{
	exynos5430_fimc_is_clk_on(dev);
	return 0;
}

int exynos_fimc_is_clk_off(struct device *dev)
{
	exynos5430_fimc_is_clk_off(dev);
	return 0;
}

int exynos_fimc_is_print_clk(struct device *dev)
{
	exynos5430_fimc_is_print_clk(dev);
	return 0;
}

int exynos_fimc_is_print_pwr(struct device *dev)
{
	exynos5430_fimc_is_print_pwr(dev);
	return 0;
}
