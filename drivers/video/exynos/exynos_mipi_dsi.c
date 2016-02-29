/* linux/drivers/video/exynos/exynos_mipi_dsi.c
 *
 * Samsung SoC MIPI-DSIM driver.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *
 * InKi Dae, <inki.dae@samsung.com>
 * Donghwa Lee, <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/memory.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <drm/exynos_drm.h>

#include <video/exynos_mipi_dsim.h>

#include <plat/fb.h>

#include "exynos_mipi_dsi_common.h"
#include "exynos_mipi_dsi_lowlevel.h"

struct mipi_dsim_ddi {
	int				bus_id;
	struct list_head		list;
	struct mipi_dsim_lcd_device	*dsim_lcd_dev;
	struct mipi_dsim_lcd_driver	*dsim_lcd_drv;
};

struct mipi_dsim_partial_region {
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
};

static LIST_HEAD(dsim_ddi_list);
static DEFINE_MUTEX(mipi_dsim_lock);
#define dev_to_dsim(a)	platform_get_drvdata(to_platform_device(a))
static struct mipi_dsim_platform_data *to_dsim_plat(struct platform_device
							*pdev)
{
	return pdev->dev.platform_data;
}

static int exynos_mipi_regulator_enable(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_driverdata *ddata = dsim->ddata;

	int ret;

	mutex_lock(&dsim->lock);
	ret = regulator_bulk_enable(ddata->num_supply, ddata->supplies);
	mutex_unlock(&dsim->lock);

	dev_dbg(dsim->dev, "MIPI regulator enable success.\n");
	return ret;
}

static int exynos_mipi_regulator_disable(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_driverdata *ddata = dsim->ddata;
	int ret;

	mutex_lock(&dsim->lock);
	ret = regulator_bulk_disable(ddata->num_supply, ddata->supplies);
	mutex_unlock(&dsim->lock);

	return ret;
}

/* update all register settings to MIPI DSI controller. */
static void exynos_mipi_update_cfg(struct mipi_dsim_device *dsim)
{

	exynos_mipi_dsi_init_dsim(dsim);
	exynos_mipi_dsi_init_link(dsim);

	if (!atomic_read(&dsim->pwr_gate))
		usleep_range(5000, 5000);

	exynos_mipi_dsi_set_hs_enable(dsim, 0);

	exynos_mipi_dsi_standby(dsim, 0);

	/* set display timing. */
	exynos_mipi_dsi_set_display_mode(dsim, dsim->dsim_config);

	exynos_mipi_dsi_standby(dsim, 1);

	exynos_mipi_dsi_init_interrupt(dsim);
}

static int exynos_mipi_dsi_early_blank_mode(struct mipi_dsim_device *dsim,
		int power)
{
	int ret = 0;

	pr_info("[mipi]pw_off:dpms[%d]lpm[%d]",
		dsim->dpms, dsim->lp_mode);

	if (dsim->lp_mode) {
		pr_err("%s:invalid lp_mode[%d]\n", __func__, dsim->lp_mode);
		ret = -EINVAL;
		goto out;
	}

	if (pm_runtime_suspended(dsim->dev))
		pr_info("%s:already suspended\n", __func__);

	switch (power) {
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		if (dsim->dpms == FB_BLANK_UNBLANK ||
			dsim->dpms == FB_BLANK_NORMAL) {
			ret = pm_runtime_put_sync(dsim->dev);
			if (ret)
				pr_info("%s:rpm_put[%d]\n", __func__, ret);
			dsim->dpms = power;
		} else {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
		}
		break;
	default:
		pr_err("%s:invalid power mode[%d].\n", __func__, power);
		ret = -EINVAL;
		break;
	}

out:
	pr_info("[mipi]pw_off:done:dpms[%d]lpm[%d]ret[%d]",
		dsim->dpms, dsim->lp_mode, ret);

	return ret;
}

static int exynos_mipi_dsi_blank_mode(struct mipi_dsim_device *dsim, int power)
{
	int ret = 0;

	pr_info("[mipi]pw_on:dpms[%d]lpm[%d]",
		dsim->dpms, dsim->lp_mode);

	if (dsim->lp_mode) {
		pr_err("%s:invalid lp_mode[%d]\n", __func__, dsim->lp_mode);
		ret = -EINVAL;
		goto out;
	}

	switch (power) {
	case FB_BLANK_UNBLANK:
		if (dsim->dpms == FB_BLANK_NORMAL ||
			dsim->dpms == FB_BLANK_VSYNC_SUSPEND ||
			dsim->dpms == FB_BLANK_POWERDOWN) {
			atomic_set(&dsim->dpms_on, 1);

			if (dsim->dpms == FB_BLANK_NORMAL &&
				!pm_runtime_suspended(dsim->dev)) {
				pr_debug("%s:bypass get_sync\n", __func__);
				dsim->dpms = FB_BLANK_UNBLANK;
				ret = 0;
			} else {
				dsim->dpms = FB_BLANK_UNBLANK;
				ret = pm_runtime_get_sync(dsim->dev);
				if (ret)
					pr_info("%s:rpm_get[%d]\n", __func__, ret);
			}

			atomic_set(&dsim->dpms_on, 0);
		} else {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
		}
		break;
	case FB_BLANK_NORMAL:
		if (dsim->dpms == FB_BLANK_VSYNC_SUSPEND ||
			dsim->dpms == FB_BLANK_POWERDOWN) {
			atomic_set(&dsim->dpms_on, 1);

			dsim->dpms = FB_BLANK_NORMAL;
			ret = pm_runtime_get_sync(dsim->dev);
			if (ret)
				pr_info("%s:rpm_get[%d]\n", __func__, ret);

			atomic_set(&dsim->dpms_on, 0);
		} else {
			pr_err("%s:invalid dpms state.\n", __func__);
			ret = -EINVAL;
		}
		break;
	default:
		pr_err("%s:invalid power mode[%d].\n", __func__, power);
		ret = -EINVAL;
		break;
	}

out:
	pr_info("[mipi]pw_on:done:dpms[%d]lpm[%d]ret[%d]",
		dsim->dpms, dsim->lp_mode, ret);

	return ret;
}

int exynos_mipi_dsi_register_lcd_device(struct mipi_dsim_lcd_device *lcd_dev)
{
	struct mipi_dsim_ddi *dsim_ddi;

	if (!lcd_dev->name) {
		pr_err("dsim_lcd_device name is NULL.\n");
		return -EFAULT;
	}

	dsim_ddi = kzalloc(sizeof(struct mipi_dsim_ddi), GFP_KERNEL);
	if (!dsim_ddi) {
		pr_err("failed to allocate dsim_ddi object.\n");
		return -ENOMEM;
	}

	dsim_ddi->dsim_lcd_dev = lcd_dev;

	mutex_lock(&mipi_dsim_lock);
	list_add_tail(&dsim_ddi->list, &dsim_ddi_list);
	mutex_unlock(&mipi_dsim_lock);

	return 0;
}

struct mipi_dsim_ddi *exynos_mipi_dsi_find_lcd_device
		(struct mipi_dsim_lcd_driver *lcd_drv)
{
	struct mipi_dsim_ddi *dsim_ddi, *next;
	struct mipi_dsim_lcd_device *lcd_dev;

	mutex_lock(&mipi_dsim_lock);

	list_for_each_entry_safe(dsim_ddi, next, &dsim_ddi_list, list) {

		lcd_dev = dsim_ddi->dsim_lcd_dev;
		if (!lcd_dev)
			continue;

		if ((strcmp(lcd_drv->name, lcd_dev->name)) == 0) {
			/**
			 * bus_id would be used to identify
			 * connected bus.
			 */
			dsim_ddi->bus_id = lcd_dev->bus_id;
			mutex_unlock(&mipi_dsim_lock);

			return dsim_ddi;
		}

		list_del(&dsim_ddi->list);
		kfree(dsim_ddi);
	}

	mutex_unlock(&mipi_dsim_lock);

	return NULL;
}

int exynos_mipi_dsi_register_lcd_driver(struct mipi_dsim_lcd_driver *lcd_drv)
{
	struct mipi_dsim_ddi *dsim_ddi;

	if (!lcd_drv->name) {
		pr_err("dsim_lcd_driver name is NULL.\n");
		return -EFAULT;
	}

	dsim_ddi = exynos_mipi_dsi_find_lcd_device(lcd_drv);
	if (!dsim_ddi) {
		pr_err("mipi_dsim_ddi object not found.\n");
		return -EFAULT;
	}

	dsim_ddi->dsim_lcd_drv = lcd_drv;

	pr_info("registered panel driver(%s) to mipi-dsi driver.\n",
		lcd_drv->name);

	return 0;

}

struct mipi_dsim_ddi *exynos_mipi_dsi_bind_lcd_ddi
		(struct mipi_dsim_device *dsim, const char *name)
{
	struct mipi_dsim_ddi *dsim_ddi, *next;
	struct mipi_dsim_lcd_driver *lcd_drv;
	struct mipi_dsim_lcd_device *lcd_dev;
	int ret;

	mutex_lock(&dsim->lock);

	list_for_each_entry_safe(dsim_ddi, next, &dsim_ddi_list, list) {
		lcd_drv = dsim_ddi->dsim_lcd_drv;
		lcd_dev = dsim_ddi->dsim_lcd_dev;
		if (!lcd_drv || !lcd_dev ||
			(dsim->id != dsim_ddi->bus_id))
				continue;

		dev_dbg(dsim->dev, "lcd_drv->id = %d, lcd_dev->id = %d\n",
				lcd_drv->id, lcd_dev->id);
		dev_dbg(dsim->dev, "lcd_dev->bus_id = %d, dsim->id = %d\n",
				lcd_dev->bus_id, dsim->id);

		if ((strcmp(lcd_drv->name, name) == 0)) {
			lcd_dev->master = dsim;

			lcd_dev->dev.parent = dsim->dev;
			dev_set_name(&lcd_dev->dev, "%s", lcd_drv->name);

			ret = device_register(&lcd_dev->dev);
			if (ret < 0) {
				dev_err(dsim->dev,
					"can't register %s, status %d\n",
					dev_name(&lcd_dev->dev), ret);
				mutex_unlock(&dsim->lock);

				return NULL;
			}

			dsim->dsim_lcd_dev = lcd_dev;
			dsim->dsim_lcd_drv = lcd_drv;

			mutex_unlock(&dsim->lock);

			return dsim_ddi;
		}
	}

	mutex_unlock(&dsim->lock);

	return NULL;
}

static int exynos_mipi_dsi_set_refresh_rate(struct mipi_dsim_device *dsim,
	int refresh)
{
	struct mipi_dsim_lcd_driver *client_drv = dsim->dsim_lcd_drv;
	struct mipi_dsim_lcd_device *client_dev = dsim->dsim_lcd_dev;

	if (client_drv && client_drv->set_refresh_rate)
		client_drv->set_refresh_rate(client_dev, refresh);

	return 0;
}

static int exynos_mipi_dsi_set_partial_region(struct mipi_dsim_device *dsim,
	void *pos)
{
	struct mipi_dsim_lcd_driver *client_drv = dsim->dsim_lcd_drv;
	struct mipi_dsim_lcd_device *client_dev = dsim->dsim_lcd_dev;
	struct mipi_dsim_partial_region *region = pos;

	if (client_drv && client_drv->set_partial_region) {
		exynos_mipi_dsi_set_main_disp_resol(dsim, region->w, region->h);
		client_drv->set_partial_region(client_dev, region->x, region->y,
						region->w, region->h);
	}

	return 0;
}

static int exynos_mipi_dsi_prepare(struct mipi_dsim_device *dsim)
{
	struct mipi_dsim_config *cfg = dsim->dsim_config;

	if (cfg->e_interface == DSIM_COMMAND) {
		atomic_set(&dsim->in_trigger, 1);
		atomic_inc(&dsim->bus_img_req_cnt);
		exynos_mipi_dsi_set_hs_enable(dsim, 1);
	}

	return 0;
}

int exynos_mipi_dsi_set_lp_mode(struct mipi_dsim_device *dsim,
		bool enable)
{
	int ret = 0;

	pr_debug("[mipi][%s]dpms[%d]lpm[%d]",
		enable ? "set_lpm" : "unset_lpm", dsim->dpms, dsim->lp_mode);

	if (dsim->dpms != FB_BLANK_UNBLANK) {
		pr_err("%s:invalid dpms[%d]\n", __func__, dsim->dpms);
		ret = -EINVAL;
		goto out;
	}

	if (dsim->lp_mode == enable) {
		pr_err("%s:invalid lp_mode[%d]\n", __func__, dsim->lp_mode);
		ret = -EINVAL;
		goto out;
	}

	atomic_set(&dsim->pwr_gate, 1);

	if (enable) {
		if (pm_runtime_suspended(dsim->dev))
			pr_info("%s:already suspended\n", __func__);

		ret = pm_runtime_put_sync(dsim->dev);
		if (ret)
			pr_info("%s:rpm_put[%d]\n", __func__, ret);
	} else {
		if (!pm_runtime_suspended(dsim->dev))
			pr_info("%s:already resumed\n", __func__);

		ret = pm_runtime_get_sync(dsim->dev);
		if (ret)
			pr_info("%s:rpm_get[%d]\n", __func__, ret);
	}

out:
	atomic_set(&dsim->pwr_gate, 0);

	pr_debug("[mipi][%s]done:dpms[%d]lpm[%d]ret[%d]", enable ?
		"set_lpm" : "unset_lpm", dsim->dpms, dsim->lp_mode, ret);

	return ret;
}

static int exynos_mipi_dsi_set_runtime_active
		(struct mipi_dsim_device *dsim)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;
	int ret = 0;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->set_runtime_activate)
		ret = src_pd->set_runtime_activate(&src_pdev->dev, "mipi_dsi");

	return ret;
}

static int exynos_mipi_dsi_set_smies_active
		(struct mipi_dsim_device *dsim, bool enable)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;
	int ret = 0;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	if (dsim->dpms != FB_BLANK_UNBLANK) {
		pr_err("%s:invalid dpms[%d]\n", __func__, dsim->dpms);
		ret = -EINVAL;
		goto out;
	}

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->set_smies_activate) {
		if (!dsim->lp_mode) {
			if (src_pd->wait_for_frame_done)
				src_pd->wait_for_frame_done(&src_pdev->dev);
		}
		if (enable) {
			exynos_mipi_dsi_standby(dsim, 0);
			ret = src_pd->set_smies_activate(&src_pdev->dev, enable);
			exynos_mipi_dsi_standby(dsim, 1);
		} else
			ret = src_pd->set_smies_activate(&src_pdev->dev, enable);
	}

out:
	return ret;
}

static int exynos_mipi_dsi_set_smies_mode
		(struct mipi_dsim_device *dsim, int mode)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;
	int ret = 0;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->set_smies_mode)
		ret = src_pd->set_smies_mode(&src_pdev->dev, mode);

	return ret;
}

static int exynos_mipi_dsi_set_secure_mode
		(struct mipi_dsim_device *dsim, bool enable)
{
	pr_info("[mipi][%s][%d->%d]\n", "set_secure_mode",
		dsim->secure_mode, enable);

	if (dsim->secure_mode == enable)
		return -EINVAL;

	dsim->secure_mode = enable;

	return 0;
}

static void exynos_mipi_dsi_set_dbg_en
		(struct mipi_dsim_device *dsim, bool enable)
{
	/*TODO: usage for dbg_cnt */
	if (enable)
		dsim->dbg_cnt = 6;
	else
		dsim->dbg_cnt = 0;

}

static void exynos_mipi_dsi_notify_panel_self_refresh
		(struct mipi_dsim_device *dsim, unsigned int rate)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return;

	src_pd = src_pdev->dev.platform_data;
	if (src_pd && src_pd->update_panel_refresh)
		src_pd->update_panel_refresh(&src_pdev->dev, rate);

}

static void exynos_mipi_dsi_power_gate(struct mipi_dsim_device *dsim,
		bool enable)
{
	struct platform_device *pdev = to_platform_device(dsim->dev);
	struct mipi_dsim_lcd_driver *client_drv = dsim->dsim_lcd_drv;
	struct mipi_dsim_lcd_device *client_dev = dsim->dsim_lcd_dev;

	if (enable) {
		if (client_drv && client_drv->te_active)
			client_drv->te_active(client_dev, enable);

		exynos_mipi_regulator_enable(dsim);

		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, true);
		clk_enable(dsim->clock);

		exynos_mipi_update_cfg(dsim);
	} else {
		if (client_drv && client_drv->te_active)
			client_drv->te_active(client_dev, enable);

		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, false);
		clk_disable(dsim->clock);

		exynos_mipi_regulator_disable(dsim);
	}
}

static int exynos_mipi_dsi_te_handler(struct mipi_dsim_device *dsim)
{
	struct exynos_drm_fimd_pdata *src_pd;
	struct platform_device *src_pdev;
	struct exynos_drm_panel_info *panel;
	struct fb_videomode *timing;
	unsigned long cmd_lock_flags;

	src_pdev = dsim->src_pdev;
	if (!src_pdev)
		return -ENOMEM;

	src_pd = src_pdev->dev.platform_data;
	panel = src_pd->panel;
	timing = &panel->timing;

	if (dsim->dbg_cnt) {
		dsim->dbg_cnt--;
		pr_info("[mipi][pa_te]dbg[%d]busreq[%d]\n", dsim->dbg_cnt,
			atomic_read(&dsim->bus_cmd_req_cnt));
	}
	if (spin_trylock_irqsave(&dsim->cmd_lock, cmd_lock_flags)) {
		if (atomic_read(&dsim->bus_cmd_req_cnt)) {
			atomic_set(&dsim->te_skip, 0);
			spin_unlock_irqrestore(&dsim->cmd_lock, cmd_lock_flags);
			pr_err("[mipi]busy for cmd transfer. req[%d]\n",
				atomic_read(&dsim->bus_cmd_req_cnt));
			return -EBUSY;
		}
		spin_unlock_irqrestore(&dsim->cmd_lock, cmd_lock_flags);
	} else {
		pr_err("%s: failed to get cmd lock.\n", __func__);
		return -EBUSY;
	}
	/*
	 * This spin lock prevents processes from requesting cmd and image
	 * data transfers at the same time.
	 */

	/*
	 * If refresh is low, pending condition check.
	 *
	 * This supports low framerate without panel flicker.
	 */
	if (atomic_read(&dsim->te_skip) &&
	    timing->refresh < panel->self_refresh) {
		atomic_set(&dsim->te_skip, 0);
		return 0;
	}

	if (src_pd->te_handler)
		src_pd->te_handler(&src_pdev->dev);

	atomic_set(&dsim->te_skip, 1);

	return 0;
}

/* define MIPI-DSI Master operations. */
static struct mipi_dsim_master_ops master_ops = {
	.cmd_read = exynos_mipi_dsi_rd_data,
	.cmd_write = exynos_mipi_dsi_wr_data,
	.atomic_cmd_write = exynos_mipi_dsi_atomic_wr_data,
	.cmd_set_begin = exynos_mipi_dsi_begin_data_set,
	.cmd_set_end = exynos_mipi_dsi_end_data_set,
	.te_handler = exynos_mipi_dsi_te_handler,
	.set_early_blank_mode = exynos_mipi_dsi_early_blank_mode,
	.set_blank_mode = exynos_mipi_dsi_blank_mode,
	.set_refresh_rate = exynos_mipi_dsi_set_refresh_rate,
	.update_panel_refresh = exynos_mipi_dsi_notify_panel_self_refresh,
	.prepare = exynos_mipi_dsi_prepare,
	.set_lp_mode = exynos_mipi_dsi_set_lp_mode,
	.set_runtime_active = exynos_mipi_dsi_set_runtime_active,
	.set_partial_region = exynos_mipi_dsi_set_partial_region,
	.set_smies_active = exynos_mipi_dsi_set_smies_active,
	.set_smies_mode = exynos_mipi_dsi_set_smies_mode,
	.set_secure_mode = exynos_mipi_dsi_set_secure_mode,
	.set_dbg_en =  exynos_mipi_dsi_set_dbg_en,
};

static int exynos_mipi_dsi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct mipi_dsim_device *dsim;
	struct mipi_dsim_config *dsim_config;
	struct mipi_dsim_platform_data *dsim_pd;
	struct mipi_dsim_driverdata *ddata;
	struct mipi_dsim_ddi *dsim_ddi;
	int ret = -EINVAL;

	dsim = kzalloc(sizeof(struct mipi_dsim_device), GFP_KERNEL);
	if (!dsim) {
		dev_err(&pdev->dev, "failed to allocate dsim object.\n");
		return -ENOMEM;
	}

	dsim->pd = to_dsim_plat(pdev);
	dsim->dev = &pdev->dev;
	dsim->id = pdev->id;
	dsim->src_pdev = dsim->pd->src_pdev;

	/* get mipi_dsim_drvierdata. */
	ddata = (struct mipi_dsim_driverdata *)
			platform_get_device_id(pdev)->driver_data;
	if (ddata == NULL) {
		dev_err(&pdev->dev, "failed to get driver data for dsim.\n");
		goto err_clock_get;
	}
	dsim->ddata = ddata;

	/* get mipi_dsim_platform_data. */
	dsim_pd = (struct mipi_dsim_platform_data *)dsim->pd;
	if (dsim_pd == NULL) {
		dev_err(&pdev->dev, "failed to get platform data for dsim.\n");
		goto err_clock_get;
	}
	/* get mipi_dsim_config. */
	dsim_config = dsim_pd->dsim_config;
	if (dsim_config == NULL) {
		dev_err(&pdev->dev, "failed to get dsim config data.\n");
		goto err_clock_get;
	}

	dsim->dsim_config = dsim_config;
	dsim->master_ops = &master_ops;

	mutex_init(&dsim->lock);

	ret = regulator_bulk_get(&pdev->dev, ddata->num_supply,
			ddata->supplies);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get regulators: %d\n", ret);
		goto err_clock_get;
	}

	dsim->clock = clk_get(&pdev->dev, ddata->clk_name);
	if (IS_ERR(dsim->clock)) {
		dev_err(&pdev->dev, "failed to get dsim clock source\n");
		goto err_clock_get;
	}

	clk_enable(dsim->clock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		goto err_platform_get;
	}

	dsim->res = request_mem_region(res->start, resource_size(res),
					dev_name(&pdev->dev));
	if (!dsim->res) {
		dev_err(&pdev->dev, "failed to request io memory region\n");
		ret = -ENOMEM;
		goto err_mem_region;
	}

	dsim->reg_base = ioremap(res->start, resource_size(res));
	if (!dsim->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}

	/* bind lcd ddi matched with panel name. */
	dsim_ddi = exynos_mipi_dsi_bind_lcd_ddi(dsim, dsim_pd->lcd_panel_name);
	if (!dsim_ddi) {
		dev_err(&pdev->dev, "mipi_dsim_ddi object not found.\n");
		goto err_bind;
	}

	dsim->irq = platform_get_irq(pdev, 0);
	if (dsim->irq < 0) {
		dev_err(&pdev->dev, "failed to request dsim irq resource\n");
		ret = -EINVAL;
		goto err_platform_get_irq;
	}

	INIT_LIST_HEAD(&dsim->cmd_list);
	spin_lock_init(&dsim->atomic_lock);
	spin_lock_init(&dsim->bus_lock);
	spin_lock_init(&dsim->cmd_lock);

	ret = request_irq(dsim->irq, exynos_mipi_dsi_interrupt_handler,
			IRQF_SHARED, pdev->name, dsim);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to request dsim irq\n");
		ret = -EINVAL;
		goto err_bind;
	}

	/* enable interrupt */
	exynos_mipi_dsi_init_interrupt(dsim);

	/* initialize mipi-dsi client(lcd panel). */
	if (dsim_ddi->dsim_lcd_drv && dsim_ddi->dsim_lcd_drv->probe)
		ret = dsim_ddi->dsim_lcd_drv->
				probe(dsim_ddi->dsim_lcd_dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to probe panel\n");
		goto err_bind;
	}

	platform_set_drvdata(pdev, dsim);
	/* in case that mipi got enabled at bootloader. */

	if (dsim_pd->enabled) {
		/* lcd panel power on. */
		if (dsim_ddi->dsim_lcd_drv && dsim_ddi->dsim_lcd_drv->power_on)
			dsim_ddi->dsim_lcd_drv->
				power_on(dsim_ddi->dsim_lcd_dev, 1);

		exynos_mipi_regulator_enable(dsim);
		exynos_mipi_update_cfg(dsim);

		if (dsim_ddi->dsim_lcd_drv && dsim_ddi->dsim_lcd_drv->check_mtp)
			dsim_ddi->dsim_lcd_drv->
				check_mtp(dsim_ddi->dsim_lcd_dev);
	} else {
		/* TODO:
		 * add check_mtp callback function
		 * if mipi dsim is off on bootloader, it causes kernel panic */
		dsim->dpms = FB_BLANK_POWERDOWN;
	}

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	device_set_early_complete(&pdev->dev, EARLY_COMP_MASTER);
#endif

	dsim->probed = true;
	dsim->dbg_cnt = 0;
	dev_info(&pdev->dev, "mipi-dsi driver(%s mode) has been probed.\n",
		(dsim_config->e_interface == DSIM_COMMAND) ?
			"CPU" : "RGB");

	return 0;

err_bind:
	iounmap(dsim->reg_base);

err_ioremap:
	release_mem_region(dsim->res->start, resource_size(dsim->res));

err_mem_region:
	release_resource(dsim->res);

err_platform_get:
	clk_disable(dsim->clock);
	clk_put(dsim->clock);
err_clock_get:
	kfree(dsim);

err_platform_get_irq:
	return ret;
}

static int __devexit exynos_mipi_dsi_remove(struct platform_device *pdev)
{
	struct mipi_dsim_device *dsim = platform_get_drvdata(pdev);
	struct mipi_dsim_ddi *dsim_ddi, *next;
	struct mipi_dsim_lcd_driver *dsim_lcd_drv;
	struct mipi_dsim_driverdata *ddata = dsim->ddata;

	iounmap(dsim->reg_base);

	clk_disable(dsim->clock);
	clk_put(dsim->clock);

	release_resource(dsim->res);
	release_mem_region(dsim->res->start, resource_size(dsim->res));

	list_for_each_entry_safe(dsim_ddi, next, &dsim_ddi_list, list) {
		if (dsim_ddi) {
			if (dsim->id != dsim_ddi->bus_id)
				continue;

			dsim_lcd_drv = dsim_ddi->dsim_lcd_drv;

			if (dsim_lcd_drv->remove)
				dsim_lcd_drv->remove(dsim_ddi->dsim_lcd_dev);

			kfree(dsim_ddi);
		}
	}

	regulator_bulk_free(ddata->num_supply, ddata->supplies);
	kfree(dsim);

	return 0;
}

static int exynos_mipi_dsi_power_on(struct mipi_dsim_device *dsim, bool enable)
{
	struct mipi_dsim_lcd_driver *client_drv = dsim->dsim_lcd_drv;
	struct mipi_dsim_lcd_device *client_dev = dsim->dsim_lcd_dev;
	struct platform_device *pdev = to_platform_device(dsim->dev);
	struct platform_device *src_pdev = dsim->src_pdev;
	struct exynos_drm_fimd_pdata *src_pdata = src_pdev->dev.platform_data;
	int ret = 0;

	pr_debug("[mipi]pm[%s]dpms[%d]lpm[%d][%s]\n",
		enable ? "on" : "off", dsim->dpms, dsim->lp_mode,
		atomic_read(&dsim->pwr_gate) ? "lp" : "dpms");

	if (atomic_read(&dsim->pwr_gate)) {
		if (dsim->dpms != FB_BLANK_UNBLANK) {
			pr_err("%s:invalid dpms[%d]\n", __func__, dsim->dpms);
			ret = -EINVAL;
			goto out;
		}

		exynos_mipi_dsi_power_gate(dsim, enable);
		dsim->lp_mode = !enable;
		goto out;
	}

	if (enable) {
		if (!dsim->probed) {
			pr_info("%s:not probed:bypass\n", __func__);
			goto out;
		}

		if (src_pdata->stop_trigger)
			src_pdata->stop_trigger(&src_pdev->dev, true);

		if (client_drv && client_drv->te_active)
			client_drv->te_active(client_dev, true);

		exynos_mipi_regulator_enable(dsim);

		/* enable MIPI-DSI PHY. */
		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, true);

		clk_enable(dsim->clock);

		exynos_mipi_update_cfg(dsim);

		if (client_drv && client_drv->panel_pm_check) {
			bool pm_skip;

			client_drv->panel_pm_check(client_dev, &pm_skip);
			if (pm_skip) {
				if (src_pdata->stop_trigger)
					src_pdata->stop_trigger(&src_pdev->dev, false);
				goto out;
			}
		}

		/* lcd panel power on. */
		if (client_drv && client_drv->power_on)
			client_drv->power_on(client_dev, 1);

		/* lcd panel reset */
		if (client_drv && client_drv->reset)
			client_drv->reset(client_dev, 1);

		/* set lcd panel sequence commands. */
		if (client_drv && client_drv->set_sequence)
			client_drv->set_sequence(client_dev);

		if (src_pdata->stop_trigger)
			src_pdata->stop_trigger(&src_pdev->dev, false);

		if (src_pdata->trigger)
			src_pdata->trigger(&src_pdev->dev);

		if (client_drv && client_drv->display_on)
			client_drv->display_on(client_dev, 1);

		if (client_drv && client_drv->resume)
			client_drv->resume(client_dev);
	} else {
		if (client_drv && client_drv->suspend)
			client_drv->suspend(client_dev);

		if (client_drv && client_drv->te_active)
			client_drv->te_active(client_dev, false);

		/* disable MIPI-DSI PHY. */
		if (dsim->pd->phy_enable)
			dsim->pd->phy_enable(pdev, false);

		clk_disable(dsim->clock);

		exynos_mipi_regulator_disable(dsim);
	}

out:
	pr_debug("[mipi]pm[%s]done:dpms[%d]lpm[%d][%s]ret[%d]\n",
		enable ? "on" : "off", dsim->dpms, dsim->lp_mode,
		atomic_read(&dsim->pwr_gate) ? "lp" : "dpms",
		ret);

	return ret;
}

#ifdef CONFIG_PM
static int exynos_mipi_dsi_suspend(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);

	if (pm_runtime_suspended(dev))
		return 0;

	dev_info(dsim->dev, "%s\n", __func__);

	/*
	 * do not use pm_runtime_suspend(). if pm_runtime_suspend() is
	 * called here, an error would be returned by that interface
	 * because the usage_count of pm runtime is more than 1.
	 */
	return exynos_mipi_dsi_power_on(dsim, false);
}

static int exynos_mipi_dsi_resume(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);

	/*
	 * if entering to sleep when lcd panel is on, the usage_count
	 * of pm runtime would still be 1 so in this case, mipi dsi driver
	 * should be on directly not drawing on pm runtime interface.
	 */
	if (!pm_runtime_suspended(dev)) {
		dev_info(dsim->dev, "%s\n", __func__);
		return exynos_mipi_dsi_power_on(dsim, true);
	}

	return 0;
}
#endif
#ifdef CONFIG_PM_RUNTIME
static int exynos_mipi_dsi_runtime_suspend(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);
	int ret;

	pr_debug("%s\n", __func__);

	ret = exynos_mipi_dsi_power_on(dsim, false);

	pr_debug("%s:ret[%d]\n", __func__, ret);

	return ret;
}

static int exynos_mipi_dsi_runtime_resume(struct device *dev)
{
	struct mipi_dsim_device *dsim = dev_to_dsim(dev);
	int ret;

	pr_debug("%s\n", __func__);

	ret = exynos_mipi_dsi_power_on(dsim, true);

	pr_debug("%s:ret[%d]\n", __func__, ret);

	return ret;
}
#endif

static const struct dev_pm_ops exynos_mipi_dsi_pm_ops = {
	.suspend		= exynos_mipi_dsi_suspend,
	.resume			= exynos_mipi_dsi_resume,
	.runtime_suspend	= exynos_mipi_dsi_runtime_suspend,
	.runtime_resume		= exynos_mipi_dsi_runtime_resume,
};

static struct regulator_bulk_data exynos3_supplies[] = {
	{ .supply = "vap_mipi_1.0v", },
};

static struct mipi_dsim_driverdata exynos3_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos3_supplies),
	.supplies = exynos3_supplies,
};

static struct regulator_bulk_data exynos4_supplies[] = {
	{ .supply = "vdd10", },
	{ .supply = "vdd18", },
};

static struct mipi_dsim_driverdata exynos4_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos4_supplies),
	.supplies = exynos4_supplies,
};

static struct mipi_dsim_driverdata exynos5210_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos4_supplies),
	.supplies = exynos4_supplies,
};

static struct mipi_dsim_driverdata exynos5410_ddata = {
	.clk_name = "dsim0",
	.num_supply = ARRAY_SIZE(exynos4_supplies),
	.supplies = exynos4_supplies,
};

static struct platform_device_id exynos_mipi_driver_ids[] = {
	{
		.name		= "exynos3-mipi",
		.driver_data	= (unsigned long)&exynos3_ddata,
	}, {
		.name		= "exynos4-mipi",
		.driver_data	= (unsigned long)&exynos4_ddata,
	}, {
		.name		= "exynos5250-mipi",
		.driver_data	= (unsigned long)&exynos5210_ddata,
	}, {
		.name		= "exynos5410-mipi",
		.driver_data	= (unsigned long)&exynos5410_ddata,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, exynos_mipi_driver_ids);

static struct platform_driver exynos_mipi_dsi_driver = {
	.probe = exynos_mipi_dsi_probe,
	.remove = __devexit_p(exynos_mipi_dsi_remove),
	.id_table	= exynos_mipi_driver_ids,
	.driver = {
		   .name = "exynos-mipi-dsim",
		   .owner = THIS_MODULE,
		   .pm = &exynos_mipi_dsi_pm_ops,
	},
};

static int exynos_mipi_dsi_register(void)
{
	platform_driver_register(&exynos_mipi_dsi_driver);
	return 0;
}

static void exynos_mipi_dsi_unregister(void)
{
	platform_driver_unregister(&exynos_mipi_dsi_driver);
}

late_initcall(exynos_mipi_dsi_register);
module_exit(exynos_mipi_dsi_unregister);

MODULE_AUTHOR("InKi Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samusung SoC MIPI-DSI driver");
MODULE_LICENSE("GPL");
