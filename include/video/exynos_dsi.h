/* include/video/exynos_mipi_dsim.h
 *
 * Platform data header for Samsung SoC MIPI-DSIM.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *
 * InKi Dae <inki.dae@samsung.com>
 * Donghwa Lee <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _EXYNOS_MIPI_DSIM_H
#define _EXYNOS_MIPI_DSIM_H

#include <linux/device.h>

/*
 * struct exynos_dsi_platform_data - interface to platform data
 *	for mipi-dsi driver.
 *
 * TODO
 */
struct exynos_dsi_platform_data {
	unsigned int enabled;
	unsigned int pll_stable_time;
	unsigned long pll_clk_rate;
	unsigned long esc_clk_rate;
	unsigned short stop_holding_cnt;
	unsigned char bta_timeout;
	unsigned short rx_timeout;
};

#endif /* _EXYNOS_MIPI_DSIM_H */
