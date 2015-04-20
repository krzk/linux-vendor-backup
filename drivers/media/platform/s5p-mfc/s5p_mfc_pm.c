/*
 * linux/drivers/media/platform/s5p-mfc/s5p_mfc_pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include "s5p_mfc_common.h"
#include "s5p_mfc_debug.h"
#include "s5p_mfc_pm.h"

#define CLK_DEBUG

static struct s5p_mfc_pm *pm;
static struct s5p_mfc_dev *p_dev;

#ifdef CLK_DEBUG
static atomic_t clk_ref;
#endif

int s5p_mfc_init_pm(struct s5p_mfc_dev *dev)
{
	int i, ret = 0;
	struct device *kdev = &dev->plat_dev->dev;

	pm = &dev->pm;
	p_dev = dev;

	pm->num_clocks = dev->variant->num_clocks;
	pm->clk_names = dev->variant->clk_names;

	/* clock control */
	for (i = 0; i < pm->num_clocks; i++) {
		pm->clocks[i] = devm_clk_get(kdev, pm->clk_names[i]);
		if (IS_ERR(pm->clocks[i])) {
			mfc_err("Failed to get clock: %s\n",
				pm->clk_names[i]);
			ret = PTR_ERR(pm->clocks[i]);
			goto err;
		}

		ret = clk_prepare(pm->clocks[i]);
		if (ret < 0) {
			mfc_err("clock prepare failed for clock: %s\n",
				pm->clk_names[i]);
			i++;
			goto err;
		}
	}

	atomic_set(&pm->power, 0);
#ifdef CONFIG_PM
	pm->device = &dev->plat_dev->dev;
	pm_runtime_enable(pm->device);
#endif
#ifdef CLK_DEBUG
	atomic_set(&clk_ref, 0);
#endif
	return 0;
err:
	while (--i > 0)
		clk_unprepare(pm->clocks[i]);

	return ret;
}

void s5p_mfc_final_pm(struct s5p_mfc_dev *dev)
{
	int i;
	for (i = 0; i < pm->num_clocks; i++)
		clk_unprepare(pm->clocks[i]);

#ifdef CONFIG_PM
	pm_runtime_disable(pm->device);
#endif
}

int s5p_mfc_clock_on(void)
{
	int i, ret = 0;
#ifdef CLK_DEBUG
	atomic_inc(&clk_ref);
	mfc_debug(3, "+ %d\n", atomic_read(&clk_ref));
#endif

	for (i = 0; i < pm->num_clocks; i++) {
		int ret = clk_enable(pm->clocks[i]);
		if (ret) {
			while (--i > 0)
				clk_disable(pm->clocks[i]);
			return ret;
		}
	}
	return ret;
}

void s5p_mfc_clock_off(void)
{
	int i;
#ifdef CLK_DEBUG
	atomic_dec(&clk_ref);
	mfc_debug(3, "- %d\n", atomic_read(&clk_ref));
#endif
	for (i = pm->num_clocks - 1; i >= 0; i--)
		clk_disable(pm->clocks[i]);
}

int s5p_mfc_power_on(void)
{
#ifdef CONFIG_PM
	return pm_runtime_get_sync(pm->device);
#else
	atomic_set(&pm->power, 1);
	return 0;
#endif
}

int s5p_mfc_power_off(void)
{
#ifdef CONFIG_PM
	return pm_runtime_put_sync(pm->device);
#else
	atomic_set(&pm->power, 0);
	return 0;
#endif
}


