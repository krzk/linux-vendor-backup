/* linux/arch/arm/mach-exynos/setup-fimc-sensor.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS-COMPANION gpio and clock configuration
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
#include "exynos-fimc-is-sensor.h"

int exynos5430_fimc_is_companion_iclk_div_max(struct platform_device *pdev)
{
	/* SCLK */
	/* SCLK_SPI0 */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0", "oscclk");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_a", 1);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_b", 1);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0_user", "oscclk");

	return 0;
}

int exynos5430_fimc_is_companion_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	int ret = 0;

	pr_info("clk_cfg:(ch%d),scenario(%d)\n", channel, scenario);

	/* SCLK_SPI0_ISP */
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0", "mout_bus_pll_user");
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_a", 200 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_spi0_b", 100 * 1000000);
	fimc_is_set_parent_dt(pdev, "mout_sclk_isp_spi0_user", "sclk_isp_spi0_top");

	/* I2C */
	/* CMU TOP */
	fimc_is_set_rate_dt(pdev, "dout_aclk_cam1_333", 333 * 1000000);

	/* USER_MUX_SEL */
	fimc_is_set_parent_dt(pdev, "mout_aclk_cam1_333_user", "aclk_cam1_333");
	/* MPWM */
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam1_166", 167 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_pclk_cam1_83", 84 * 1000000);
	fimc_is_set_rate_dt(pdev, "dout_sclk_isp_mpwm", 84 * 1000000);

	return ret;
}

int exynos5430_fimc_is_companion_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	/* SCLK clock enable */
	fimc_is_enable_dt(pdev, "gate_isp_spi0");

	return 0;
}

int exynos5430_fimc_is_companion_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	exynos5430_fimc_is_companion_iclk_div_max(pdev);

	/* SCLK clock disable */
	fimc_is_disable_dt(pdev, "gate_isp_spi0");

	return 0;
}

int exynos5430_fimc_is_companion_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	u32 frequency;
	char mux_name[30];
	char div_a_name[30];
	char div_b_name[30];
	char sclk_name[30];

	pr_debug("%s\n", __func__);

	snprintf(mux_name, sizeof(mux_name), "mout_sclk_isp_sensor%d", channel);
	snprintf(div_a_name, sizeof(div_a_name), "dout_sclk_isp_sensor%d_a", channel);
	snprintf(div_b_name, sizeof(div_b_name), "dout_sclk_isp_sensor%d_b", channel);
	snprintf(sclk_name, sizeof(sclk_name), "sclk_isp_sensor%d", channel);

	fimc_is_set_parent_dt(pdev, mux_name, "oscclk");
	fimc_is_set_rate_dt(pdev, div_a_name, 24 * 1000000);
	fimc_is_set_rate_dt(pdev, div_b_name, 24 * 1000000);
	frequency = fimc_is_get_rate_dt(pdev, sclk_name);

	pr_info("%s(%d, mclk : %d)\n", __func__, channel, frequency);

	return 0;
}

int exynos5430_fimc_is_companion_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* Wrapper functions */
int exynos_fimc_is_companion_iclk_cfg(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	exynos5430_fimc_is_companion_iclk_cfg(pdev, scenario, channel);
	return 0;
}

int exynos_fimc_is_companion_iclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	exynos5430_fimc_is_companion_iclk_on(pdev, scenario, channel);
	return 0;
}

int exynos_fimc_is_companion_iclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	exynos5430_fimc_is_companion_iclk_off(pdev, scenario, channel);
	return 0;
}

int exynos_fimc_is_companion_mclk_on(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	exynos5430_fimc_is_companion_mclk_on(pdev, scenario, channel);
	return 0;
}

int exynos_fimc_is_companion_mclk_off(struct platform_device *pdev,
	u32 scenario,
	u32 channel)
{
	exynos5430_fimc_is_companion_mclk_off(pdev, scenario, channel);
	return 0;
}
