/* exynos_drm_fimd.c
 *
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include "drmP.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/dispfreq.h>
#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/dispfreq.h>
#include <linux/irq.h>

#include <video/exynos_mipi_dsim.h>
#include <drm/exynos_drm.h>
#include <drm/drm_backlight.h>
#include <plat/regs-fb-v4.h>
#include <mach/map.h>

#ifdef CONFIG_MDNIE_SUPPORT
#include <plat/fimd_lite_ext.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#include "exynos_drm_drv.h"
#include "exynos_drm_fbdev.h"
#include "exynos_drm_crtc.h"
#include "exynos_drm_iommu.h"
#include "exynos_drm_plane.h"
#ifdef CONFIG_DRM_EXYNOS_IPP
#include "exynos_drm_ipp.h"
#endif
#ifdef CONFIG_DRM_EXYNOS_DBG
#include "exynos_drm_dbg.h"
#endif
#include "exynos_drm_fimd.h"

#ifdef CONFIG_EXYNOS_SMIES
#include <plat/smies.h>
#endif
#include <linux/firmware.h>

#ifdef FIMD_AGING_LOG
static inline void fimd_save_timing(enum fimd_log_state log_state)
{
	int i;

	if (log_state >= FIMD_LOG_MAX)
		return;

	i = atomic_inc_return(&fimd_log_idx[log_state]);
	if (i >= FIMD_LOG_INFO_MAX) {
		atomic_set(&fimd_log_idx[log_state], -1);
		i = 0;
	}

	fimd_log_info[log_state][i].time = cpu_clock(raw_smp_processor_id());
	fimd_log_info[log_state][i].pid = task_pid_nr(current);
}
#endif

static inline struct fimd_driver_data *drm_fimd_get_driver_data(
	struct platform_device *pdev)
{
	return (struct fimd_driver_data *)
		platform_get_device_id(pdev)->driver_data;
}

static int fimd_cleanup_event(struct fimd_context *ctx, const char *str)
{
	struct drm_device *drm_dev;
	int val, ret = 0;

	if (!(ctx && ctx->drm_dev))
		return 0;

	drm_dev = ctx->drm_dev;
	exynos_drm_wait_finish_pageflip(drm_dev, ctx->pipe);
	fimd_wait_for_done(ctx->dev);

	val = atomic_read(&ctx->wait_done_event);
	if (val) {
		DRM_INFO("[clean:%s]wait_done_event[%d]\n", str, val);
		atomic_set(&ctx->wait_done_event, 0);
		ret = -EBUSY;
	}

	val = atomic_read(&ctx->win_update);
	if (val) {
		DRM_INFO("[clean:%s]win_update[%d]\n", str, val);
		atomic_set(&ctx->win_update, 0);
		ret = -EBUSY;
	}

	val = exynos_drm_get_pendingflip(drm_dev, ctx->pipe);
	if (val) {
		DRM_INFO("[clean:%s]pendingflip[%d]\n", str, val);
		exynos_drm_crtc_finish_pageflip(drm_dev, ctx->pipe);
		ret = -EBUSY;
	}

	val = atomic_read(&drm_dev->vblank_refcount[ctx->pipe]);
	if (val) {
		DRM_INFO("[clean:%s]vblank_refcount[%d]\n", str, val);
		drm_handle_vblank(drm_dev, ctx->pipe);
		ret = -EBUSY;
	}

	val = atomic_read(&ctx->wait_vsync_event);
	if (val) {
		DRM_INFO("[clean:%s]wait_vsync_event[%d]\n", str, val);
		ret = -EBUSY;
	}

	DRM_DEBUG("[clean:%s]done[%d]\n", str, ret);

	return ret;
}

static int fimd_check_event(struct fimd_context *ctx, const char *str)
{
	struct drm_device *drm_dev;
	int val, ret = 0;

	if (!(ctx && ctx->drm_dev))
		return 0;

	drm_dev = ctx->drm_dev;
	val = atomic_read(&ctx->wait_done_event);
	if (val) {
		DRM_INFO("[dbg:%s]wait_done_event[%d]\n", str, val);
		ret = -EBUSY;
	}

	val = atomic_read(&ctx->win_update);
	if (val) {
		DRM_INFO("[dbg:%s]win_update[%d]\n", str, val);
		ret = -EBUSY;
	}

	val = exynos_drm_get_pendingflip(drm_dev, ctx->pipe);
	if (val) {
		DRM_INFO("[dbg:%s]pendingflip[%d]\n", str, val);
		ret = -EBUSY;
	}

	val = atomic_read(&drm_dev->vblank_refcount[ctx->pipe]);
	if (val) {
		DRM_INFO("[dbg:%s]vblank_refcount[%d]\n", str, val);
		ret = -EBUSY;
	}

	val = atomic_read(&ctx->wait_vsync_event);
	if (val) {
		DRM_INFO("[dbg:%s]wait_vsync_event[%d]\n", str, val);
		ret = -EBUSY;
	}

	return ret;
}

static bool fimd_is_lpm(struct fimd_context *ctx)
{
	struct device *dev = ctx->dev;
	return pm_runtime_suspended(dev);
}

static void fimd_wait_lpm_set(struct fimd_context *ctx, const char *str)
{
	int ret, wait = !completion_done(&ctx->lpm_comp);

	if (!ctx->pm_gating_on)
		return;

	if (wait)
		DRM_DEBUG("%s[%s]\n", "wait_lpm", str);

	ret = wait_for_completion_timeout(
			&ctx->lpm_comp,
		msecs_to_jiffies(200));
	if (ret < 0)
		ret = -ERESTARTSYS;
	else if (!ret)
		DRM_INFO("%s:timeout[%s]\n", "wait_lpm", str);
	if (ret <= 0)
		complete_all(&ctx->lpm_comp);

	if (wait)
		DRM_DEBUG("%s:done[%s]\n", "wait_lpm", str);
}

static void fimd_set_lp_mode(struct fimd_context *ctx, bool set)
{
	struct mipi_dsim_device *dsim =
		platform_get_drvdata(ctx->disp_bus_pdev);
	int ret = 0;

	if (!ctx->pm_gating_on)
		return;

	mutex_lock(&ctx->lock);

	DRM_DEBUG("[%s_ops]dpms[%d]lpm[%d]\n",
		set ? "set_lpm" : "unset_lpm", ctx->dpms, fimd_is_lpm(ctx));

	if (fimd_is_lpm(ctx) == set) {
		DRM_DEBUG("[%s_ops]bypass\n", set ? "set_lpm" : "unset_lpm");
		goto bypass;
	}

#ifdef FIMD_AGING_LOG
		fimd_save_timing(set ? FIMD_LOG_SET_LPM : FIMD_LOG_UNSET_LPM);
#endif

	if (set) {
		if (fimd_check_event(ctx, "set_lpm"))
			ctx->dbg_cnt = 6;

		exynos_drm_wait_finish_pageflip(ctx->drm_dev, ctx->pipe);
		fimd_wait_for_done(ctx->dev);

		if (dsim && dsim->master_ops->set_lp_mode)
			ret = dsim->master_ops->set_lp_mode(dsim, true);

		ret = fimd_runtime_put_sync(ctx->dev);
		if (ret) {
			DRM_ERROR("suspend:ret[%d]\n", ret);
			if (ret < 0)
				goto recover;
		}

		if (fimd_check_event(ctx, "set_lpm:done")) {
			ctx->dbg_cnt = 6;
			if (dsim && dsim->master_ops->set_dbg_en)
				dsim->master_ops->set_dbg_en(dsim, true);
		}
	}else {
		ret = fimd_runtime_get_sync(ctx->dev);
		if (ret) {
			DRM_ERROR("resume:ret[%d]\n", ret);
			goto bypass;
		}

		if (dsim && dsim->master_ops->set_lp_mode)
			dsim->master_ops->set_lp_mode(dsim, false);
	}

	DRM_DEBUG("[%s_ops]done:dpms[%d]lpm[%d]\n",
		set ? "set_lpm" : "unset_lpm", ctx->dpms, fimd_is_lpm(ctx));

	mutex_unlock(&ctx->lock);

	return;

recover:
	fimd_check_event(ctx, "set_lpm:recover");

	if (dsim && dsim->master_ops->set_lp_mode) {
		ret = dsim->master_ops->set_lp_mode(dsim, false);
		DRM_INFO("%s:mipi gate recover:ret[%d]",
			__func__, ret);
	}

bypass:
	DRM_DEBUG("[%s_ops]bypass:dpms[%d]lpm[%d]\n",
		set ? "set_lpm" : "unset_lpm", ctx->dpms, fimd_is_lpm(ctx));

	mutex_unlock(&ctx->lock);

	return;
}

static int fimd_lp_mode_enable(struct device *dev, bool enable)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	bool lpm = fimd_is_lpm(ctx);

	DRM_INFO("%s:dpms[%d]lpm[%d]cur_mode[%d]\n",
			__func__, ctx->dpms, lpm, ctx->pm_gating_on);

	if (ctx->pm_gating_on == enable) {
		DRM_ERROR("invalid mode set[%d]\n", enable);
		return -EINVAL;
	}

	ctx->pm_gating_on = enable;

	DRM_INFO("%s:done:dpms[%d]lpm[%d]cur_mode[%d]\n",
			__func__, ctx->dpms, lpm, ctx->pm_gating_on);

	return 0;
}

static int fimd_set_runtime_activate(struct device *dev, const char *str)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct drm_device *drm_dev = ctx->drm_dev;
	int crtc = ctx->pipe, ret = 0;

	if (!ctx->pm_gating_on)
		return 0;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	if (ctx->secure_mode) {
		DRM_INFO("[rt_act:bypass]dpms[%d]lpm[%d][%s]secure\n",
			ctx->dpms, fimd_is_lpm(ctx), str);
		return 0;
	}
#endif

	if (!completion_done(&ctx->lpm_comp)) {
		DRM_INFO("[rt_act:wait]dpms[%d]lpm[%d][%s]\n",
			ctx->dpms, fimd_is_lpm(ctx), str);
		fimd_wait_lpm_set(ctx, str);
	}

	if (fimd_is_lpm(ctx)) {
		DRM_INFO("[rt_act:wake]dpms[%d]lpm[%d][%s]\n",
			ctx->dpms, fimd_is_lpm(ctx), str);

		if (completion_done(&ctx->lpm_comp))
			INIT_COMPLETION(ctx->lpm_comp);

		if (!exynos_drm_crtc_wake_vblank(drm_dev, crtc, str))
			fimd_wait_lpm_set(ctx, str);
	}

	/* ToDo: should be removed it next time */
	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		ret = -EACCES;
	}

	return ret;
}

#ifdef CONFIG_EXYNOS_SMIES
#ifdef CONFIG_EXYNOS_SMIES_RUNTIME_ACTIVE
static int fimd_set_smies_activate(struct device *dev, bool enable)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct device *smies_device = ctx->smies_device;

	atomic_set(&ctx->smies_active, enable);

	if (!fimd_is_lpm(ctx)) {
		if (atomic_read(&ctx->smies_active))
			ctx->smies_on(smies_device);
		else
			ctx->smies_off(smies_device);
	}

	return 0;
}
#else
static int fimd_set_smies_mode(struct device *dev, int mode)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct device *smies_device = ctx->smies_device;

	if (!fimd_is_lpm(ctx))
		ctx->smies_mode(smies_device, mode);

	return 0;
}
#endif
#endif

static void fimd_update_gamma(struct device *dev, bool enable)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	int i;

	if (!ctx->reg_gamma || !(ctx->reg_gain))
		return;

	if (enable) {
		for (i = 0; i < 33; i++)
			writel(ctx->reg_gamma[i], ctx->regs + (GAMMALUT_START + (i<<2)));

		writel(ctx->reg_gain, ctx->regs + CGAINCON);
		writel(ctx->vidcon3, ctx->regs + VIDCON3);
	} else
		writel(0, ctx->regs + VIDCON3);
}

static int fimd_set_gamma_acivate(struct device *dev, bool enable)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	DRM_INFO("%s: dpms[%d]lpm[%d] gamma[%d]\n",
			__func__, ctx->dpms, fimd_is_lpm(ctx), enable);

	atomic_set(&ctx->gamma_on, enable);

	if (!fimd_is_lpm(ctx))
		fimd_update_gamma(dev, enable);

	return 0;
}

static void fimd_update_panel_refresh(struct device *dev, unsigned int rate)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct exynos_drm_panel_info *panel = ctx->panel;

	/* panel notify the self refresh rate to fimd
	* so that fimd decide whether it skip te signal or not.
	*/
	panel->self_refresh = rate;
}

static void fimd_trigger(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_driver_data *driver_data = ctx->driver_data;
	void *timing_base = ctx->regs + driver_data->timing_base;
	struct drm_device *drm_dev = ctx->drm_dev;
	struct mipi_dsim_device *dsim;
	u32 reg;
	bool vbl_en;
	static bool first_open = true;
	int wait_done;

	if (!ctx->i80_if)
		return;

	if (first_open) {
		if (!ctx->drm_dev || !ctx->drm_dev->open_count)
			return;

		first_open = false;
	}

	/*
	 * no_trigger == 1 means that MIPI-DSI tries to transfer some commands
	 * to lcd panel. So do not trigger.
	 */
	if (ctx->no_trigger)
		return;

	vbl_en = drm_dev->vblank_enabled[ctx->pipe];
	dsim = platform_get_drvdata(ctx->disp_bus_pdev);
	if (dsim && dsim->master_ops->prepare)
		dsim->master_ops->prepare(dsim);

	/*
	 * FIMD has to guarantee not to power off until trigger action is
	 * completed, i.e, has to skip i80 time out event handler during
	 * triggering mode. So needs triggering flag to check it.
	 * And the triggering flag is set just before setting i80 frame done
	 * interrupt and unset just after unsetting i80 frame done interrupt.
	 */

	/* enter triggering mode */
	atomic_inc(&ctx->wait_done_event);

	wait_done = atomic_read(&ctx->wait_done_event);
	if (wait_done > 1) {
		DRM_INFO("%s:wait_done[%d]\n", __func__, wait_done);
		ctx->dbg_cnt = 6;
	}

	if (ctx->dbg_cnt) {
		DRM_INFO("TRIG[0x%x 0x%x]c[%d]\n",
			readl(ctx->regs + VIDINTCON0),
			readl(ctx->regs + TRIGCON),
			ctx->dbg_cnt);
	}

	/* set i80 fame done interrupt */
	reg = readl(ctx->regs + VIDINTCON0);
	reg |= (VIDINTCON0_INT_ENABLE | VIDINTCON0_INT_I80IFDONE |
						VIDINTCON0_INT_SYSMAINCON);
	writel(reg, ctx->regs + VIDINTCON0);

	reg = readl(timing_base + TRIGCON);

	if (!vbl_en)
		DRM_INFO("%s:vbl_off[0x%x]\n", __func__, reg);

	reg |= 1 << 0 | 1 << 1;
	writel(reg, timing_base + TRIGCON);
}

static bool fimd_display_is_connected(struct device *dev)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	/* TODO. */

	return true;
}

static void *fimd_get_panel(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	DRM_DEBUG("%s:rot[%d]res[%d %d]\n",
		__func__, ctx->panel->mode->rotate,
		ctx->panel->timing.xres, ctx->panel->timing.yres);

	return ctx->panel;
}

static int fimd_check_timing(struct device *dev, void *timing)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	/* TODO. */

	return 0;
}

static inline void fimd_win_blank_ctrl(struct fimd_context *ctx, int win, bool blank)
{
	u32 val =  readl(ctx->regs + WINxMAP(win));

	if (blank)
		writel(WINxMAP_MAP, ctx->regs + WINxMAP(win));
	else
		writel(0, ctx->regs + WINxMAP(win));

	DRM_INFO("%s[%d]win[%d]val[0x%x->0x%x]bl[%d]\n", "blank_ctrl",
		blank, win, val, readl(ctx->regs + WINxMAP(win)), ctx->bl_on);
}

#ifdef CONFIG_MDNIE_SUPPORT
static void exynos_drm_mdnie_mode_stop(struct fimd_context *ctx)
{
	struct s5p_fimd_ext_driver *mdnie_drv, *fimd_lite_drv;
	u32 cfg;
	int count = STOP_TIMEOUT;

	DRM_DEBUG_KMS("%s\n", __func__);

	mdnie_drv = to_fimd_ext_driver(mdnie->dev.driver);
	fimd_lite_drv = to_fimd_ext_driver(fimd_lite_dev->dev.driver);

	/*
	 * stop operations - FIMD, FIMD-lite
	 * 1. stop FIMD by "per-frame off"
	 * 2. wait until bit[0] cleared, maximum 20ms for 60Hz
	 *	If one frame transferred, fimd will be stoppted automatically
	 * 3. stop FIMD-lite by "direct-off"
	 * 4. clear setup FIMD-lite
	 * 5. disable FIMD-lite clock
	 * 6. clear dualrgb register to mDNIe mode
	 * 7. clear system register
	 */
	fimd_lite_dev->enabled = false;

	/* 1. stop FIMD "per-frame off" */
	cfg = readl(ctx->regs + VIDCON0);
	cfg &= ~(VIDCON0_ENVID_F);
	writel(cfg, ctx->regs + VIDCON0);

	/* 2. wait stop complete */
	while (count--) {
		cfg = readl(ctx->regs + VIDCON0);
		if (!(cfg & VIDCON0_ENVID_F))
			break;
		cpu_relax();
		usleep_range(1000, 2000);
	}

	if (!count)
		DRM_ERROR("failed to stop FIMD.\n");

	/* 3. stop FIMD-lite "direct-off" */
	if (fimd_lite_drv->stop)
		fimd_lite_drv->stop(fimd_lite_dev);

	/* 4. clear setup FIMD-lite */
	if (fimd_lite_drv->setup)
		fimd_lite_drv->setup(fimd_lite_dev, 0);

	/* 5. clear setup mDNIe */
	if (mdnie_drv)
		mdnie_drv->setup(mdnie, 0);

	/* 6. disable FIMD-lite clock */
	if (fimd_lite_drv->power_off)
		fimd_lite_drv->power_off(fimd_lite_dev);

	/* 7. clear dualrgb register to mDNIe mode. */
	cfg = readl(ctx->regs + DPCLKCON);
	cfg &= ~(0x3 << 0);
	writel(cfg, ctx->regs + DPCLKCON);

	/* 8. clear system register. */
	cfg = readl(S3C_VA_SYS + 0x210);
	/* MIE_LBLK0 is MIE. */
	cfg &= ~(0 << 0);
	/* FIMDBYPASS_LBLK0 is FIMD bypass. */
	cfg |= 1 << 1;
	writel(cfg, S3C_VA_SYS + 0x210);

	DRM_DEBUG_KMS("%s:done.\n", __func__);
}

static int exynos_drm_mdnie_mode_start(struct fimd_context *ctx)
{
	struct s5p_fimd_ext_driver *mdnie_drv, *fimd_lite_drv;
	u32 cfg;
	int count = STOP_TIMEOUT;

	DRM_DEBUG_KMS("%s\n", __func__);

	mdnie_drv = to_fimd_ext_driver(mdnie->dev.driver);
	fimd_lite_drv = to_fimd_ext_driver(fimd_lite_dev->dev.driver);
	/*
	 * start operations - FIMD, FIMD-lite
	 * 1. stop FIMD "per-frame off
	 * 2. wait until bit[0] cleared, maximum 20ms for 60Hz
	 *	If one frame transferred, fimd will be stoppted automatically
	 * 3. all polarity values should be 0 for mDNIe
	 * 4. set VCLK free run control
	 * 5. set system register
	 * 6. set dualrgb register to mDNIe mode
	 * 7. enable FIMD-lite clock
	 * 8. setup FIMD-lite
	 * 9. setup mDNIe
	 * 10. start FIMD-lite
	 * 11. start FIMD
	 */
	/* 1. stop FIMD "per-frame off" */
	cfg = readl(ctx->regs + VIDCON0);
	cfg &= ~(VIDCON0_ENVID_F);
	writel(cfg, ctx->regs + VIDCON0);
	/* 2. wait stop complete */
	while (count--) {
		cfg = readl(ctx->regs + VIDCON0);
		if (!(cfg & VIDCON0_ENVID_F))
			break;
		cpu_relax();
		usleep_range(1000, 2000);
	}

	if (!count)
		DRM_ERROR("failed to stop FIMD.\n");

	/* 3. all polarity values should be 0 for mDNIe. */
	cfg = readl(ctx->regs + VIDCON1);
	cfg &= ~(VIDCON1_INV_VCLK | VIDCON1_INV_HSYNC |
		VIDCON1_INV_VSYNC | VIDCON1_INV_VDEN |
		VIDCON1_VCLK_MASK);
	writel(cfg, ctx->regs + VIDCON1);
	/* 4. set VCLK free run control */
	cfg = readl(ctx->regs + VIDCON0);
	cfg |= VIDCON0_VLCKFREE;
	writel(cfg, ctx->regs + VIDCON0);

	/* 5. set system register. */
	cfg = readl(S3C_VA_SYS + 0x210);
	cfg &= ~(0x1 << 13);
	cfg &= ~(0x1 << 12);
	cfg &= ~(0x3 << 10);
	/* MIE_LBLK0 is mDNIe. */
	cfg |= 0x1 << 0;
	/* FIMDBYPASS_LBLK0 is MIE/mDNIe. */
	cfg &= ~(0x1 << 1);
	writel(cfg, S3C_VA_SYS + 0x210);
	/* 6. set dualrgb register to mDNIe mode */
	cfg = readl(ctx->regs + DPCLKCON);
	cfg &= ~(0x3 << 0);
	cfg |= 0x3 << 0;
	writel(cfg, ctx->regs + DPCLKCON);

	/* 7. enable FIMD-lite clock */
	if (fimd_lite_drv && fimd_lite_drv->power_on)
		fimd_lite_drv->power_on(fimd_lite_dev);

	/* 8. setup FIMD-lite */
	if (fimd_lite_drv)
		fimd_lite_drv->setup(fimd_lite_dev, 1);

	/* 9. setup mDNIe */
	if (mdnie_drv)
		mdnie_drv->setup(mdnie, 1);
	/* 10. start FIMD-lite */
	if (fimd_lite_drv && fimd_lite_drv->start)
		fimd_lite_drv->start(fimd_lite_dev);
	/* 11. start FIMD */
	cfg = readl(ctx->regs + VIDCON0);
	cfg |= VIDCON0_ENVID | VIDCON0_ENVID_F;
	writel(cfg, ctx->regs + VIDCON0);

	fimd_lite_dev->enabled = true;

	DRM_DEBUG_KMS("%s:done.\n", __func__);

	return 0;
}
#endif

static int fimd_display_power_on(struct device *dev, int mode)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data;
	int win;

	mutex_lock(&ctx->lock);

	DRM_INFO("fimd_bl:mode[%d]dpms[%d]lpm[%d]\n",
		mode, ctx->dpms, fimd_is_lpm(ctx));

	ctx->dbg_cnt = 6;

	switch (mode) {
	case DRM_MODE_DPMS_ON:
#ifdef CONFIG_MDNIE_SUPPORT
		if (fimd_lite_dev)
			exynos_drm_mdnie_mode_start(ctx);
#endif
		drm_bl_dpms(mode);
		ctx->bl_on = true;
		fimd_check_event(ctx, "bl:on");
		break;
	case DRM_MODE_DPMS_STANDBY:
 	case DRM_MODE_DPMS_SUSPEND:
 		break;
	case DRM_MODE_DPMS_OFF:
		fimd_check_event(ctx, "bl:off");
		fimd_cleanup_event(ctx, "bl:off");

		if (fimd_is_lpm(ctx)) {
			mutex_unlock(&ctx->lock);
			fimd_set_lp_mode(ctx, false);
			mutex_lock(&ctx->lock);
		}

		ctx->bl_on = false;
		for (win = 0; win < WINDOWS_NR; win++) {
			win_data = &ctx->win_data[win];
			if (win_data->enabled)
				fimd_win_blank_ctrl(ctx, win, true);
		}

		drm_bl_dpms(mode);
		break;
	default:
		DRM_INFO("%s:invalid mode[%d]\n", __func__, mode);
		break;
	}

	DRM_INFO("fimd_bl:done:mode[%d]dpms[%d]lpm[%d]\n",
		mode, ctx->dpms, fimd_is_lpm(ctx));

	mutex_unlock(&ctx->lock);

	return 0;
}

static void fimd_partial_resolution(struct device *dev,
					struct exynos_drm_partial_pos *pos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_driver_data *driver_data = ctx->driver_data;
	void *timing_base = ctx->regs + driver_data->timing_base;
	u32 val;

	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		return;
	}

	DRM_DEBUG_KMS("%s:res[%d %d]\n", __func__, pos->w, pos->h);

	/* setup horizontal and vertical display size. */
	val = VIDTCON2_LINEVAL(pos->h - 1) | VIDTCON2_HOZVAL(pos->w - 1) |
	       VIDTCON2_LINEVAL_E(pos->h - 1) | VIDTCON2_HOZVAL_E(pos->w - 1);

	writel(val, timing_base + VIDTCON2);
}

static void fimd_request_partial_update(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	unsigned long flags;

	DRM_DEBUG_KMS("%s\n", __func__);

	spin_lock_irqsave(&ctx->win_updated_lock, flags);
	atomic_set(&ctx->win_update, 1);
	spin_unlock_irqrestore(&ctx->win_updated_lock, flags);
	atomic_set(&ctx->partial_requested, 1);
}

static void fimd_adjust_partial_region(struct device *dev,
					struct exynos_drm_partial_pos *pos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct exynos_drm_panel_info *panel = ctx->panel;
	struct fb_videomode *timing = &panel->timing;
	unsigned int old_x;
	unsigned int old_w;

	old_x = pos->x;
	old_w = pos->w;

	/*
	 * if pos->x is bigger than 0, adjust pos->x and pos->w roughly.
	 *
	 * ######################################### <- image
	 *	|	|		|
	 *	|	before(x)	before(w)
	 *	after(x)		after(w)
	 */
	if (pos->x > 0) {
		pos->x = ALIGN(pos->x, WIDTH_LIMIT) - WIDTH_LIMIT;
		pos->w += WIDTH_LIMIT;
	}

	/*
	 * pos->w should be WIDTH_LIMIT if pos->w is 0, and also pos->x should
	 * be adjusted properly if pos->x is bigger than WIDTH_LIMIT.
	 */
	if (pos->w == 0) {
		if (pos->x > WIDTH_LIMIT)
			pos->x -= WIDTH_LIMIT;
		pos->w = WIDTH_LIMIT;
	} else
		pos->w = ALIGN(pos->w, WIDTH_LIMIT);

	/*
	 * pos->x + pos->w should be smaller than horizontal size of display.
	 * If not so, page fault exception will be occurred.
	 */
	if (pos->x + pos->w > timing->xres)
		pos->w = timing->xres - pos->x;

	DRM_DEBUG_KMS("%s:adjusted:x[%d->%d]w[%d->%d]\n",
			__func__, old_x, pos->x, old_w, pos->w);
}

static int fimd_set_partial_region(struct device *dev,
					struct exynos_drm_partial_pos *pos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct mipi_dsim_device *dsim;

	DRM_DEBUG_KMS("%s:pos[%d %d %d %d]\n",
		__func__, pos->x, pos->y, pos->w, pos->h);

	dsim = platform_get_drvdata(ctx->disp_bus_pdev);
	if (dsim && dsim->master_ops->set_partial_region)
		dsim->master_ops->set_partial_region(dsim, pos);

	return 0;
}

static struct exynos_drm_display_ops fimd_display_ops = {
	.type = EXYNOS_DISPLAY_TYPE_LCD,
	.is_connected = fimd_display_is_connected,
	.get_panel = fimd_get_panel,
	.check_timing = fimd_check_timing,
	.power_on = fimd_display_power_on,
	.set_partial_region = fimd_set_partial_region,
};

static bool fimd_check_dpms(int cur, int next)
{
	bool ret = false;

	switch (cur) {
	case DRM_MODE_DPMS_ON:
		if (next == DRM_MODE_DPMS_SUSPEND ||
			next == DRM_MODE_DPMS_OFF)
			ret = true;
		break;
	case DRM_MODE_DPMS_STANDBY:
		if (next == DRM_MODE_DPMS_ON ||
			next == DRM_MODE_DPMS_SUSPEND ||
			next == DRM_MODE_DPMS_OFF)
			ret = true;
		break;
	case DRM_MODE_DPMS_SUSPEND:
		if (next == DRM_MODE_DPMS_ON ||
			next == DRM_MODE_DPMS_STANDBY)
			ret = true;
		break;
	case DRM_MODE_DPMS_OFF:
		if (next == DRM_MODE_DPMS_ON ||
			next == DRM_MODE_DPMS_STANDBY)
			ret = true;
		break;
	default:
		break;
	}

	DRM_INFO("%s[%d->%d]%s\n", __func__, cur, next, ret ? "" : "[invalid]");

	return ret;
}

static void fimd_dpms(struct device *dev, int mode)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	int dpms;

	mutex_lock(&ctx->lock);
	dpms = ctx->dpms;

	DRM_INFO("%s[%d->%d]lpm[%d]\n",
		"dpms", dpms, mode, fimd_is_lpm(ctx));
	fimd_check_event(ctx, "dpms");

	ctx->dbg_cnt = 6;

#ifdef FIMD_AGING_LOG
	fimd_save_timing(FIMD_LOG_DPMS);
#endif

	if (!fimd_check_dpms(ctx->dpms, mode))
		goto out;

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		ctx->dpms = mode;
#ifdef CONFIG_SLEEP_MONITOR
		ctx->act_cnt++;
#endif
		if (fimd_runtime_get_sync(dev))
			ctx->dpms = dpms;
		break;
	case DRM_MODE_DPMS_STANDBY:
		ctx->dpms = mode;
		if (fimd_runtime_get_sync(dev))
			ctx->dpms = dpms;
		else {
			drm_bl_dpms(mode);
			ctx->bl_on = true;
		}
		break;
	case DRM_MODE_DPMS_SUSPEND:
		fimd_cleanup_event(ctx, "dpms:suspend");

		if (fimd_is_lpm(ctx)) {
			mutex_unlock(&ctx->lock);
			fimd_set_lp_mode(ctx, false);
			mutex_lock(&ctx->lock);
		}

		ctx->bl_on = false;
		drm_bl_dpms(mode);

		fimd_runtime_put_sync(dev);
		ctx->dpms = mode;
		break;
	case DRM_MODE_DPMS_OFF:
		fimd_runtime_put_sync(dev);
		ctx->dpms = mode;
		break;
	default:
		DRM_INFO("%s:invalid mode[%d]\n", __func__, mode);
		break;
	}

out:
	fimd_check_event(ctx, "dpms:done");
	DRM_INFO("%s:done[%d]lpm[%d]\n",
		"dpms", ctx->dpms, fimd_is_lpm(ctx));

	mutex_unlock(&ctx->lock);
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
static void fimd_window_suspend(struct fimd_context *ctx)
{
	struct fimd_win_data *win_data;
	int i;

	for (i = 0; i < WINDOWS_NR; i++) {
		if (i == ctx->default_win)
			continue;

		win_data = &ctx->win_data[i];
		if (win_data->enabled) {
			win_data->resume = true;
			fimd_win_disable(ctx->dev, i);
		}
	}
}

static void fimd_window_resume(struct fimd_context *ctx)
{
	struct fimd_win_data *win_data;
	int i;

	for (i = 0; i < WINDOWS_NR; i++) {
		if (i == ctx->default_win)
			continue;

		win_data = &ctx->win_data[i];
		if (win_data->resume) {
			fimd_win_enable(ctx->dev, i);
			win_data->resume = false;
		}
	}
}
#endif

static void fimd_apply(struct device *subdrv_dev)
{
	struct fimd_context *ctx = get_fimd_context(subdrv_dev);
	struct exynos_drm_manager *mgr = ctx->subdrv.manager;
	struct exynos_drm_manager_ops *mgr_ops = mgr->ops;
	struct exynos_drm_overlay_ops *ovl_ops = mgr->overlay_ops;
	struct fimd_win_data *win_data;
	int i;

	DRM_DEBUG("%s\n", __func__);

	atomic_set(&ctx->do_apply, 1);

	for (i = 0; i < WINDOWS_NR; i++) {
		win_data = &ctx->win_data[i];
		if (win_data->enabled && (ovl_ops && ovl_ops->commit))
			ovl_ops->commit(subdrv_dev, i);
	}

	if (mgr_ops && mgr_ops->commit)
		mgr_ops->commit(subdrv_dev);

	atomic_set(&ctx->do_apply, 0);
	DRM_DEBUG("%s:done\n", __func__);
}

static void fimd_set_fix_vclock(struct fimd_context *ctx)
{
	struct fimd_driver_data *driver_data = ctx->driver_data;
	u32 val;

	DRM_DEBUG_KMS("%s\n", __func__);

	writel(REG_CLKGATE_MODE_NON_CLOCK_GATE,
			ctx->regs + REG_CLKGATE_MODE);

	val = readl(ctx->regs + driver_data->timing_base + VIDCON1);
	val &= ~VIDCON1_VCLK_MASK;
	val |= VIDCON1_VCLK_RUN;
	writel(val, ctx->regs + driver_data->timing_base + VIDCON1);
}

static void fimd_sysreg_setup(struct fimd_context *ctx)
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
#if !defined(CONFIG_EXYNOS_SMIES_ENABLE_BOOTTIME)
	if (!atomic_read(&ctx->smies_active))
		reg |= (1 << 1);
#endif
	__raw_writel(reg, S3C_VA_SYS + 0x0210);

	reg = __raw_readl(S3C_VA_SYS + 0x0210);
	reg &= ~(3 << 10);
	reg |= (1 << 10);
	__raw_writel(reg, S3C_VA_SYS + 0x0210);
}

static void fimd_commit(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct exynos_drm_panel_info *panel = ctx->panel;
	struct fb_videomode *timing = &panel->timing;
	struct fimd_driver_data *driver_data = ctx->driver_data;
	void *timing_base = ctx->regs + driver_data->timing_base;
	u32 val;

	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		return;
	}

	DRM_DEBUG_KMS("%s\n", __func__);

	if (ctx->i80_if) {
		/* If it's not mipi module, also set RSPOL bit to high. */
		val = ctx->i80ifcon | I80IFEN_ENABLE;

		writel(val, timing_base + I80IFCONFAx(0));

		/* Disable auto frame rate. */
		writel(0, timing_base + I80IFCONFBx(0));

		val = readl(timing_base + VIDOUT_CON);
		val &= ~VIDOUT_CON_F_MASK;
		val |= VIDOUT_CON_F_I80_LDI0;
		writel(val, timing_base + VIDOUT_CON);

		/* Set i80 interface bit to LCDBLK_CFG register. */
		fimd_sysreg_setup(ctx);
	} else {
		/* setup polarity values from machine code. */
		writel(ctx->vidcon1, timing_base + VIDCON1);

		/* setup vertical timing values. */
		val = VIDTCON0_VBPD(timing->upper_margin - 1) |
		       VIDTCON0_VFPD(timing->lower_margin - 1) |
		       VIDTCON0_VSPW(timing->vsync_len - 1);
		writel(val, timing_base + VIDTCON0);

		/* setup horizontal timing values.  */
		val = VIDTCON1_HBPD(timing->left_margin - 1) |
		       VIDTCON1_HFPD(timing->right_margin - 1) |
		       VIDTCON1_HSPW(timing->hsync_len - 1);
		writel(val, timing_base + VIDTCON1);
	}

	/* setup horizontal and vertical display size. */
	val = VIDTCON2_LINEVAL(timing->yres - 1) |
	       VIDTCON2_HOZVAL(timing->xres - 1);
	writel(val, timing_base + VIDTCON2);

#ifdef CONFIG_EXYNOS_SMIES
	val = readl(timing_base + VIDTCON3);
	val |= VIDTCON3_VSYNCEN;
	writel(val, timing_base + VIDTCON3);
#endif

	if (driver_data->use_vclk)
		fimd_set_fix_vclock(ctx);

	/* setup clock source, clock divider, enable dma. */
	val = ctx->vidcon0;
	val &= ~(VIDCON0_CLKVAL_F_MASK | VIDCON0_CLKDIR);

	if (ctx->clkdiv > 1)
		val |= VIDCON0_CLKVAL_F(ctx->clkdiv - 1) | VIDCON0_CLKDIR;
	else
		val &= ~VIDCON0_CLKDIR;	/* 1:1 clock */

#ifdef CONFIG_EXYNOS_SMIES
	val |= VIDCON0_HIVCLK | VIDCON0_VCLKFREE;
#endif

	if (!ctx->mdnie_enabled) {
		/*
		 * fields of register with prefix '_F' would be updated
		 * at vsync(same as dma start)
		 */
		val |= VIDCON0_ENVID | VIDCON0_ENVID_F;
		writel(val, ctx->regs + VIDCON0);
	}
}

static int fimd_vblank_ctrl(struct fimd_context *ctx, bool enable)
{
	u32 val;

	if (enable) {
		val = readl(ctx->regs + VIDINTCON0);

		val |= (VIDINTCON0_INT_ENABLE | VIDINTCON0_INT_FRAME);
#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
			val |= VIDINTCON0_INT_FIFO_EN;
			val |= VIDINTCON0_FIFOLEVEL_EMPTY;
#endif

		val &= ~VIDINTCON0_FRAMESEL0_MASK;
		val |= VIDINTCON0_FRAMESEL0_VSYNC;
		val &= ~VIDINTCON0_FRAMESEL1_MASK;
		val |= VIDINTCON0_FRAMESEL1_NONE;
		DRM_DEBUG("vbl_on_reg[0x%x->0x%x]\n",
			readl(ctx->regs + VIDINTCON0), val);

		writel(val, ctx->regs + VIDINTCON0);
	} else {
		val = readl(ctx->regs + VIDINTCON0);

		val &= ~(VIDINTCON0_INT_ENABLE | VIDINTCON0_INT_FRAME);
#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
			val &= ~VIDINTCON0_INT_FIFO_EN;
			val &= ~VIDINTCON0_FIFOLEVEL_EMPTY;
#endif

		DRM_DEBUG("vbl_off_reg[0x%x->0x%x]\n",
			readl(ctx->regs + VIDINTCON0), val);

		writel(val, ctx->regs + VIDINTCON0);
	}

	return 0;
}

static void fimd_lpmode_work(struct work_struct *work)
{
	struct fimd_lpm_work *lpm_work =
		(struct fimd_lpm_work *)work;
	struct fimd_context *ctx = lpm_work->ctx;
	bool  set_lpm = lpm_work->enable_lpm;

	DRM_DEBUG("[%s]:dpms[%d]lpm[%d]\n",
		set_lpm ? "set_lpm" : "unset_lpm",
		ctx->dpms, fimd_is_lpm(ctx));

	fimd_set_lp_mode(ctx, set_lpm);

	DRM_INFO("[%s]done:dpms[%d]lpm[%d]\n",
		set_lpm ? "set_lpm" : "unset_lpm",
		ctx->dpms, fimd_is_lpm(ctx));

	complete_all(&ctx->lpm_comp);

	return;
}

static void fimd_prepare_vblank(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct drm_device *drm_dev = ctx->drm_dev;

	DRM_DEBUG("[pre_vbl]r[%d]dpms[%d]lpm[%d]\n",
		atomic_read(&drm_dev->vblank_refcount[ctx->pipe]),
		ctx->dpms, fimd_is_lpm(ctx));

	fimd_wait_lpm_set(ctx, "pre_vbl");

	return;
}

static int fimd_enable_vblank(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct drm_device *drm_dev = ctx->drm_dev;
	struct fimd_lpm_work *lpm_work = ctx->lpm_work;

	if (completion_done(&ctx->lpm_comp))
		INIT_COMPLETION(ctx->lpm_comp);

	lpm_work->enable_lpm = false;
	lpm_work->set_vblank = true;
	if (!queue_work(ctx->fimd_wq, (struct work_struct *)
			ctx->lpm_work))
		DRM_INFO("%s:busy to queue_work.\n", __func__);

	DRM_INFO("[on_vbl]r[%d]dpms[%d]lpm[%d]\n",
		atomic_read(&drm_dev->vblank_refcount[ctx->pipe]),
		ctx->dpms, fimd_is_lpm(ctx));

	return 0;
}

static void fimd_disable_vblank(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct drm_device *drm_dev = ctx->drm_dev;
	struct fimd_lpm_work *lpm_work = ctx->lpm_work;

	if (!ctx->bl_on) {
		DRM_INFO("[off_vbl]r[%d]bypass:bl_off\n",
			atomic_read(&drm_dev->vblank_refcount[ctx->pipe]));
		return;
	}

	if (completion_done(&ctx->lpm_comp))
		INIT_COMPLETION(ctx->lpm_comp);

	lpm_work->enable_lpm = true;
	lpm_work->set_vblank = true;
	if (!queue_work(ctx->fimd_wq, (struct work_struct *)
			ctx->lpm_work))
		DRM_INFO("%s:busy to queue_work.\n", __func__);

	DRM_INFO("[off_vbl]r[%d]dpms[%d]lpm[%d]\n",
		atomic_read(&drm_dev->vblank_refcount[ctx->pipe]),
		ctx->dpms, fimd_is_lpm(ctx));

	return;
}

static void fimd_wait_for_vblank(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct mipi_dsim_device *dsim =
		platform_get_drvdata(ctx->disp_bus_pdev);

	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		return;
	}

	atomic_set(&ctx->wait_vsync_event, 1);

	/*
	 * wait for FIMD to signal VSYNC interrupt or return after
	 * timeout which is set to 50ms (refresh rate of 20).
	 */
	if (!wait_event_timeout(ctx->wait_vsync_queue,
				!atomic_read(&ctx->wait_vsync_event),
				DRM_HZ/20)) {
		DRM_ERROR("vblank wait timed out.\n");
		ctx->dbg_cnt = 6;
		if (dsim && dsim->master_ops->set_dbg_en)
			dsim->master_ops->set_dbg_en(dsim, true);
	}
}

static int fimd_wait_for_done(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct mipi_dsim_device *dsim =
		platform_get_drvdata(ctx->disp_bus_pdev);
	int wait_done = atomic_read(&ctx->wait_done_event);

	if (fimd_is_lpm(ctx)) {
		DRM_INFO("%s:bypass:lpm\n", "wait_done");
		return -EINVAL;
	}

	if (wait_done)
		DRM_INFO("%s[%d]\n", "wait_done", wait_done);

	if (!wait_event_timeout(ctx->wait_done_queue,
				!atomic_read(&ctx->wait_done_event),
				DRM_HZ/20)) {
		ctx->dbg_cnt = 6;
		DRM_ERROR("timed out[%d]\n", wait_done);
		if (dsim && dsim->master_ops->set_dbg_en)
			dsim->master_ops->set_dbg_en(dsim, true);
		return -ETIME;
	}

	if (wait_done)
		DRM_INFO("%s[%d]done\n", "wait_done", wait_done);

	return 0;
}

static void fimd_stop_trigger(struct device *dev,
					bool stop)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		return;
	}

	DRM_INFO("stop_tr[%d->%d]\n", ctx->no_trigger, stop);

	ctx->no_trigger = stop;
}

static int fimd_te_handler(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct drm_device *drm_dev = ctx->drm_dev;
	struct mipi_dsim_device *dsim =
		platform_get_drvdata(ctx->disp_bus_pdev);
	unsigned long flags;
	int crtc = ctx->pipe, wait_done;
	unsigned long bus_lock_flags;

	DRM_DEBUG_KMS("\n");

	/* check the crtc is detached already from encoder */
	if (crtc < 0 || !drm_dev) {
		DRM_ERROR("crtc is detached\n");
		return IRQ_HANDLED;
	}

	/*
	 * The ctx->win_update has to be protected not to change until
	 * fimd_trigger() is called.
	 */
	spin_lock_irqsave(&ctx->win_updated_lock, flags);

       if (ctx->dbg_cnt) {
	   	DRM_INFO("te_irq:c[%d]\n", ctx->dbg_cnt--);
		fimd_check_event(ctx, "te_irq");
       }

	/* set wait te event to zero and wake up queue. */
	if (atomic_read(&ctx->wait_vsync_event)) {
		atomic_set(&ctx->wait_vsync_event, 0);
		DRM_WAKEUP(&ctx->wait_vsync_queue);

		/*
		 * If ctx->win_update = 1 then set it to 0 so that
		 * fimd_trigger isn't called.
		 * This resolves page fault issus that it can be occurred
		 * when rmfb is requested between win_commit and te interrupt
		 * handler.
		 */
		if (atomic_read(&ctx->win_update)) {
			atomic_set(&ctx->win_update, 0);
			spin_unlock_irqrestore(&ctx->win_updated_lock, flags);
			goto out_handle_vblank;
		}
	}

	/*
	 * If there is previous pageflip request, do trigger and handle the
	 * pageflip event so that current framebuffer can be updated into GRAM
	 * of lcd panel.
	 */
	if (atomic_read(&ctx->win_update)) {
		spin_unlock_irqrestore(&ctx->win_updated_lock, flags);
		/*
		 * skip in triggering mode because multiple triggering can
		 * cause panel reset
		 */
		 wait_done = atomic_read(&ctx->wait_done_event);

		if (!wait_done) {
			if (atomic_read(&ctx->partial_requested)) {
				request_crtc_partial_update(drm_dev, crtc);
				atomic_set(&ctx->partial_requested, 0);
			}

			spin_lock_irqsave(&ctx->win_updated_lock,
						flags);
			atomic_set(&ctx->win_update, 0);
			spin_unlock_irqrestore(&ctx->win_updated_lock,
						flags);
			spin_lock_irqsave(&dsim->bus_lock, bus_lock_flags);
			fimd_trigger(ctx->dev);
			spin_unlock_irqrestore(&dsim->bus_lock, bus_lock_flags);
		} else
			DRM_INFO("%s:wait_done[%d]\n", __func__, wait_done);

		spin_lock_irqsave(&ctx->win_updated_lock, flags);
	}

	spin_unlock_irqrestore(&ctx->win_updated_lock, flags);

out_handle_vblank:
	if (exynos_drm_get_pendingflip(drm_dev, crtc))
		exynos_drm_crtc_finish_pageflip(drm_dev, crtc);

	/*
	 * Return vblank event to user process only if vblank interrupt
	 * is enabled.
	 */
	if (atomic_read(&drm_dev->vblank_refcount[crtc]))
		drm_handle_vblank(drm_dev, crtc);

	return 0;
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
static void fimd_wait_secure_work(struct fimd_context *ctx)
{
	int ret;

	DRM_DEBUG("%s\n", __func__);

	ret = wait_for_completion_timeout(
			&ctx->secure_comp,
		msecs_to_jiffies(5000));
	if (ret < 0)
		ret = -ERESTARTSYS;
	else if (!ret)
		DRM_INFO("%s:timeout\n", __func__);
	if (ret <= 0)
		complete_all(&ctx->secure_comp);

	DRM_INFO("%s:done\n", __func__);
}

static void fimd_secure_work(struct work_struct *work)
{
	struct fimd_secure_work *secure_work =
		(struct fimd_secure_work *)work;
	struct fimd_context *ctx = secure_work->ctx;
	struct drm_device *drm_dev = ctx->drm_dev;
	struct mipi_dsim_device *dsim =
		platform_get_drvdata(ctx->disp_bus_pdev);
	bool  secure_mode = secure_work->secure_mode;

	DRM_INFO("%s:secure_mode[%d->%d]dpms[%d]lpm[%d]\n",
		__func__, ctx->secure_mode, secure_mode,
		ctx->dpms, fimd_is_lpm(ctx));
	fimd_check_event(ctx, "secure_work");

	if (secure_mode) {
		ctx->secure_mode = true;
		fimd_wait_for_vblank(ctx->dev);
		fimd_set_lp_mode(ctx, false);

		fimd_stop_trigger(ctx->dev, true);
		fimd_lp_mode_enable(ctx->dev, false);
		fimd_window_suspend(ctx);
		disable_irq(ctx->irq);

		if (is_drm_iommu_supported(drm_dev))
			fimd_iommu_ctrl(ctx, false);

		fimd_stop_trigger(ctx->dev, false);
		fimd_trigger(ctx->dev);
		atomic_set(&ctx->wait_done_event, 0);

		if (dsim && dsim->master_ops->set_secure_mode)
			dsim->master_ops->set_secure_mode(dsim, true);
	} else {
		fimd_stop_trigger(ctx->dev, true);

		if (dsim && dsim->master_ops->set_secure_mode)
			dsim->master_ops->set_secure_mode(dsim, false);

		fimd_window_resume(ctx);

		if (is_drm_iommu_supported(drm_dev))
			fimd_iommu_ctrl(ctx, true);

		fimd_stop_trigger(ctx->dev, false);
		enable_irq(ctx->irq);
		fimd_lp_mode_enable(ctx->dev, true);

		fimd_apply(ctx->dev);
		fimd_trigger(ctx->dev);
		ctx->secure_mode = false;
	}

	fimd_check_event(ctx, "secure_work:done");
	DRM_INFO("%s:done:secure_mode[%d->%d]dpms[%d]lpm[%d]\n",
		__func__, ctx->secure_mode, secure_mode,
		ctx->dpms, fimd_is_lpm(ctx));

	complete_all(&ctx->secure_comp);

	return;
}

static int fimd_secure_mode_change(struct device *dev, int mode)
 {
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_secure_work *secure_work = ctx->secure_work;

	DRM_INFO("%s:mode[0x%x]dpms[%d]lpm[%d]\n",
		__func__, mode, ctx->dpms, fimd_is_lpm(ctx));

	ctx->dbg_cnt = 60;

	if (completion_done(&ctx->secure_comp))
		INIT_COMPLETION(ctx->secure_comp);

	if (mode == TRUSTEDUI_MODE_OFF)
		secure_work->secure_mode = false;
	else
		secure_work->secure_mode = true;

	if (!queue_work(ctx->fimd_wq, (struct work_struct *)
			ctx->secure_work))
		DRM_INFO("%s:busy to queue_work.\n", __func__);

	fimd_wait_secure_work(ctx);

	DRM_INFO("%s:done:dpms[%d]lpm[%d]\n",
		__func__, ctx->dpms, fimd_is_lpm(ctx));

	return 0;
 }
 #endif

static int fimd_subdrv_probe(struct drm_device *drm_dev, struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct exynos_drm_panel_info *panel = ctx->panel;
	int pipe = 0;

	DRM_INFO("%s\n", __func__);

	ctx->drm_dev = drm_dev;
	ctx->pipe = pipe;

	/*
	 * enable drm irq mode.
	 * - with irq_enabled = 1, we can use the vblank feature.
	 *
	 * P.S. note that we wouldn't use drm irq handler but
	 *	just specific driver own one instead because
	 *	drm framework supports only one irq handler.
	 */
	drm_dev->irq_enabled = 1;

	/*
	 * with vblank_disable_allowed = 1, vblank interrupt will be disabled
	 * by drm timer once a current process gives up ownership of
	 * vblank event.(after drm_vblank_put function is called)
	 */
	drm_dev->vblank_disable_allowed = 1;

	if (panel->mode->rotate)
		swap(panel->timing.xres, panel->timing.yres);

	return 0;
}

static void fimd_subdrv_remove(struct drm_device *drm_dev, struct device *dev)
{
	DRM_INFO("%s\n", __func__);

	/* detach this sub driver from iommu mapping if supported. */
	if (is_drm_iommu_supported(drm_dev))
		drm_iommu_detach_device(drm_dev, dev);

	/* TODO. */
}

static struct exynos_drm_manager_ops fimd_manager_ops = {
	.dpms = fimd_dpms,
	.apply = fimd_apply,
	.commit = fimd_commit,
	.prepare_vblank = fimd_prepare_vblank,
	.enable_vblank = fimd_enable_vblank,
	.disable_vblank = fimd_disable_vblank,
	.wait_for_vblank = fimd_wait_for_vblank,
	.lp_mode_enable = fimd_lp_mode_enable,
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	.secure_mode_change = fimd_secure_mode_change,
#endif

};

static void fimd_win_mode_set(struct device *dev,
			      struct exynos_drm_overlay *overlay)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct exynos_drm_panel_info *panel = ctx->panel;
	struct fb_videomode *timing = &panel->timing;
	struct fimd_win_data *win_data;
	struct mipi_dsim_device *dsim;
	int win;
	unsigned long offset;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!overlay) {
		dev_err(dev, "overlay is NULL\n");
		return;
	}

	win = overlay->zpos;
	if (win == DEFAULT_ZPOS)
		win = ctx->default_win;

	if (win < 0 || win >= WINDOWS_NR)
		return;

	offset = overlay->fb_x * (overlay->bpp >> 3);
	offset += overlay->fb_y * overlay->pitch;

	DRM_DEBUG_KMS("offset = 0x%lx, pitch = %x\n", offset, overlay->pitch);

	win_data = &ctx->win_data[win];

	win_data->offset_x = overlay->crtc_x;
	win_data->offset_y = overlay->crtc_y;
	win_data->ovl_width = overlay->crtc_width;
	win_data->ovl_height = overlay->crtc_height;
	win_data->fb_width = overlay->fb_width;
	win_data->fb_height = overlay->fb_height;
	win_data->base_dma_addr = overlay->dma_addr[0];
	win_data->dma_addr = overlay->dma_addr[0] + offset;
	win_data->bpp = overlay->bpp;
	win_data->pixel_format = overlay->pixel_format;
	win_data->buf_offsize = (overlay->fb_width - overlay->crtc_width) *
				(overlay->bpp >> 3);
	win_data->line_size = overlay->crtc_width * (overlay->bpp >> 3);
	win_data->local_path = overlay->local_path;
	win_data->refresh = overlay->refresh;

	if (timing->refresh != win_data->refresh) {
		/* FIXME: fimd clock configure ? */

		/* MIPI configuration */
		dsim = platform_get_drvdata(ctx->disp_bus_pdev);
		if (dsim && dsim->master_ops->set_refresh_rate)
			dsim->master_ops->set_refresh_rate(dsim,
				win_data->refresh);

		timing->refresh = win_data->refresh;
	}

	DRM_DEBUG("%s:win[%d]xy[%d %d]ovl[%d %d]fb[%d %d]f[0x%x]a[0x%x]sz[%d %d]\n",
		__func__, win,
		win_data->offset_x, win_data->offset_y,
		win_data->ovl_width, win_data->ovl_height,
		win_data->fb_width, win_data->fb_height,
		win_data->pixel_format, (int)win_data->base_dma_addr,
		win_data->buf_offsize, win_data->line_size);
}

static void fimd_win_set_fifo_cfg(struct device *dev, unsigned int win)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	u32 val;

	val = readl(ctx->regs + WINCON(win));
	val &= ~(WINCON_DATAPATH_MASK | WINCONx_BITSWP |
		WINCONx_BYTSWP | WINCONx_HAWSWP | WINCONx_WSWP |
		WINCON0_BPPMODE_MASK | WINCONx_BURSTLEN_MASK);

	val |= (WINCON_DATAPATH_LOCAL | WINCONx_INRGB_RGB |
		WINCON0_BPPMODE_24BPP_888);

	writel(val, ctx->regs + WINCON(win));
}

static void fimd_win_set_pixfmt(struct device *dev, unsigned int win)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data = &ctx->win_data[win];
	unsigned long val;

	DRM_DEBUG_KMS("%s\n", __func__);

	val = readl(ctx->regs + WINCON(win));
	val &= ~(WINCON0_BPPMODE_MASK | WINCON_DATAPATH_MASK  |
		WINCONx_BURSTLEN_MASK);

	switch (win_data->pixel_format) {
	case DRM_FORMAT_C8:
		val |= WINCON0_BPPMODE_8BPP_PALETTE;
		val |= WINCONx_BURSTLEN_8WORD;
		val |= WINCONx_BYTSWP;
		break;
	case DRM_FORMAT_XRGB1555:
		val |= WINCON0_BPPMODE_16BPP_1555;
		val |= WINCONx_HAWSWP;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	case DRM_FORMAT_RGB565:
		val |= WINCON0_BPPMODE_16BPP_565;
		val |= WINCONx_HAWSWP;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	case DRM_FORMAT_XRGB8888:
		val |= WINCON0_BPPMODE_24BPP_888;
		val |= WINCONx_WSWP;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	case DRM_FORMAT_ARGB8888:
		val |= WINCON1_BPPMODE_25BPP_A1888
			| WINCON1_BLD_PIX | WINCON1_ALPHA_SEL;
		val |= WINCONx_WSWP;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	default:
		DRM_DEBUG_KMS("invalid pixel size so using unpacked 24bpp.\n");

		val |= WINCON0_BPPMODE_24BPP_888;
		val |= WINCONx_WSWP;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	}

	DRM_DEBUG_KMS("bpp = %d\n", win_data->bpp);

	writel(val, ctx->regs + WINCON(win));
}

static void fimd_win_set_colkey(struct device *dev, unsigned int win)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	unsigned int keycon0 = 0, keycon1 = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	keycon0 = ~(WxKEYCON0_KEYBL_EN | WxKEYCON0_KEYEN_F |
			WxKEYCON0_DIRCON) | WxKEYCON0_COMPKEY(0);

	keycon1 = WxKEYCON1_COLVAL(0xffffffff);

	writel(keycon0, ctx->regs + WKEYCON0_BASE(win));
	writel(keycon1, ctx->regs + WKEYCON1_BASE(win));
}

static int fimd_check_min_width(struct fimd_win_data *win_data, u32 *min_width)
{
	int burst_length, bpp;

	bpp = (win_data->bpp >> 3);

	switch (win_data->bpp) {
	case 1:
		burst_length = 4;
		break;
	case 2 ... 8:
		burst_length = 8;
		break;
	case 16 ... 32:
		burst_length = 16;
		break;
	default:
		return -EINVAL;
	}

	*min_width = burst_length * 8 / bpp;

	DRM_DEBUG_KMS("%s:min_width[%d]\n", __func__, *min_width);

	return 0;
}

/* FIXME: need to check pixel align */
static int fimd_check_align_width(struct fimd_win_data *win_data, int width)
{
	int pixel_align, bpp, bytes_align = 8;

	bpp = (win_data->bpp >> 3);
	pixel_align = bytes_align / bpp;

	DRM_DEBUG_KMS("%s:width[%d]pixel_align[%d]\n",
		__func__, width, pixel_align);

	if (width % pixel_align) {
		DRM_ERROR("failed to set width[%d]pixel_align[%d]\n",
			width, pixel_align);
		return -EINVAL;
	}

	return 0;
}

static int fimd_check_restriction(struct fimd_win_data *win_data)
{
	u32 min_width;
	int ret;

	DRM_DEBUG_KMS("%s:line_size[%d]buf_offsize[%d]\n",
		__func__, win_data->line_size, win_data->buf_offsize);
	DRM_DEBUG_KMS("%s:offset_x[%d]ovl_width[%d]\n",
		__func__, win_data->offset_x, win_data->ovl_width);

	/*
	 * You should align the sum of PAGEWIDTH_F and
	 * OFFSIZE_F double-word (8 byte) boundary.
	 */
	if (VIDW0nADD2_DWORD_CHECK(win_data->line_size,
	    win_data->buf_offsize)) {
		DRM_ERROR("failed to check double-word align.\n");
		return -EINVAL;
	}

	ret = fimd_check_align_width(win_data, win_data->ovl_width);
	if (ret) {
		DRM_ERROR("failed to check pixel align.\n");
		return -EINVAL;
	}

	ret = fimd_check_min_width(win_data, &min_width);
	if (ret) {
		DRM_ERROR("failed to check minimum width.\n");
		return -EINVAL;
	}

	if (win_data->ovl_width < min_width) {
		DRM_INFO("%s:invalid min %d width %d.\n", __func__,
			win_data->ovl_width, min_width);
		return 0;
	}

	return 0;
}

static inline void fimd_win_channel_ctrl(struct fimd_context *ctx, int win,
		bool enable)
{
	unsigned long val;

	val = readl(ctx->regs + WINCON(win));
	val &= ~WINCONx_ENWIN;
	if (enable)
		val |= WINCONx_ENWIN;
	writel(val, ctx->regs + WINCON(win));
}

static inline void fimd_win_shadow_ctrl(struct fimd_context *ctx,
		int win, int flags)
{
	struct fimd_win_data *win_data;
	unsigned long val;

	win_data = &ctx->win_data[win];

	val = readl(ctx->regs + SHADOWCON);
	val &= ~SHADOWCON_WINx_PROTECT(win);

	if (flags & FIMD_SC_PROTECT)
		val |= SHADOWCON_WINx_PROTECT(win);
	else {
		if (!(flags & FIMD_SC_BYPASS_CH_CTRL)) {
			val &= ~(SHADOWCON_CHx_ENABLE(win) |
				SHADOWCON_CHx_LOCAL_ENABLE(win));

			if (flags & FIMD_SC_CH_ENABLE) {
				val |= SHADOWCON_CHx_ENABLE(win);
				if (win_data->local_path)
					val |= SHADOWCON_CHx_LOCAL_ENABLE(win);
			}
		}
	}

	writel(val, ctx->regs + SHADOWCON);
}

static void fimd_win_commit(struct device *dev, int zpos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct drm_device *drm_dev = ctx->subdrv.drm_dev;
	struct fimd_win_data *win_data;
	int win = zpos;
	unsigned long val, alpha, size;
	u32 br_x, br_y;

	if (fimd_is_lpm(ctx))
		DRM_INFO("%s:need to unset lpm\n", __func__);

#ifdef FIMD_AGING_LOG
	fimd_save_timing(FIMD_LOG_WIN_COMMIT);
#endif

	if (!atomic_read(&ctx->do_apply) &&
		!atomic_read(&ctx->partial_requested))
		fimd_set_runtime_activate(ctx->dev, "commit");

	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		return;
	}

	DRM_DEBUG("%s:wait_done[%d]\n", __func__,
			atomic_read(&ctx->wait_done_event));

	if (win == DEFAULT_ZPOS)
		win = ctx->default_win;

	if (win < 0 || win >= WINDOWS_NR)
		return;

	win_data = &ctx->win_data[win];

	if (fimd_check_restriction(win_data)) {
		DRM_ERROR("failed to check %d window restriction.\n", win);
		return;
	}

	br_x = win_data->offset_x + win_data->ovl_width;
	if (br_x)
		br_x--;

	br_y = win_data->offset_y + win_data->ovl_height;
	if (br_y)
		br_y--;

	/*
	 * SHADOWCON register is used for enabling timing.
	 *
	 * for example, once only width value of a register is set,
	 * if the dma is started then fimd hardware could malfunction so
	 * with protect window setting, the register fields with prefix '_F'
	 * wouldn't be updated at vsync also but updated once unprotect window
	 * is set.
	 */

	/* protect windows */
	fimd_win_shadow_ctrl(ctx, win, FIMD_SC_PROTECT);

	val =  readl(ctx->regs + WINxMAP(win));
	if (!ctx->bl_on && !(val & WINxMAP_MAP))
		fimd_win_blank_ctrl(ctx, win, true);
	else if (ctx->bl_on && (val & WINxMAP_MAP) &&
		!atomic_read(&ctx->do_apply))
		fimd_win_blank_ctrl(ctx, win, false);

	/* buffer start address */
	val = (unsigned long)win_data->dma_addr;
	writel(val, ctx->regs + VIDWx_BUF_START(win, 0));

	/* buffer end address */
	size = win_data->fb_width * win_data->ovl_height * (win_data->bpp >> 3);
	val = (unsigned long)(win_data->dma_addr + size);
	writel(val, ctx->regs + VIDWx_BUF_END(win, 0));

	DRM_DEBUG_KMS("start addr = 0x%lx, end addr = 0x%lx, size = 0x%lx\n",
			(unsigned long)win_data->dma_addr, val, size);
	DRM_DEBUG_KMS("ovl_width = %d, ovl_height = %d\n",
			win_data->ovl_width, win_data->ovl_height);

	/* buffer size */
	val = VIDW_BUF_SIZE_OFFSET(win_data->buf_offsize) |
		VIDW_BUF_SIZE_PAGEWIDTH(win_data->line_size) |
		VIDW_BUF_SIZE_OFFSET_E(win_data->buf_offsize) |
		VIDW_BUF_SIZE_PAGEWIDTH_E(win_data->line_size);
	writel(val, ctx->regs + VIDWx_BUF_SIZE(win, 0));

	DRM_DEBUG_KMS("%s:win[%d]x[%d]y[%d]w[%d]h[%d]\n", __func__,
		win, win_data->offset_x, win_data->offset_y,
		win_data->ovl_width, win_data->ovl_height);

	/* OSD position */
	val = VIDOSDxA_TOPLEFT_X(win_data->offset_x) |
		VIDOSDxA_TOPLEFT_Y(win_data->offset_y) |
		VIDOSDxA_TOPLEFT_X_E(win_data->offset_x) |
		VIDOSDxA_TOPLEFT_Y_E(win_data->offset_y);
	writel(val, ctx->regs + VIDOSD_A(win));

	val = VIDOSDxB_BOTRIGHT_X(br_x) | VIDOSDxB_BOTRIGHT_Y(br_y) |
		VIDOSDxB_BOTRIGHT_X_E(br_x) | VIDOSDxB_BOTRIGHT_Y_E(br_y);

	writel(val, ctx->regs + VIDOSD_B(win));

	DRM_DEBUG_KMS("osd pos: tx = %d, ty = %d, bx = %d, by = %d\n",
			win_data->offset_x, win_data->offset_y, br_x, br_y);

	/* hardware window 0 doesn't support alpha channel. */
	if (win != 0) {
		/* OSD alpha */
		alpha = VIDOSDxC_ALPHA0_R_H(0x0) |
			VIDOSDxC_ALPHA0_G_H(0x0) |
			VIDOSDxC_ALPHA0_B_H(0x0) |
			VIDOSDxC_ALPHA1_R_H(0xff) |
			VIDOSDxC_ALPHA1_G_H(0xff) |
			VIDOSDxC_ALPHA1_B_H(0xff);

		writel(alpha, ctx->regs + VIDOSD_C(win));
	}

	/* OSD size */
	if (win != 3 && win != 4) {
		u32 offset = VIDOSD_D(win);
		if (win == 0)
			offset = VIDOSD_C_SIZE_W0;
		val = win_data->ovl_width * win_data->ovl_height;
		writel(val, ctx->regs + offset);

		DRM_DEBUG_KMS("osd size = 0x%x\n", (unsigned int)val);
	}

	if (!win_data->local_path)
		fimd_win_set_pixfmt(dev, win);
	else
		fimd_win_set_fifo_cfg(dev, win);

	/* hardware window 0 doesn't support color key. */
	if (win != 0)
		fimd_win_set_colkey(dev, win);

	if (is_drm_iommu_supported(drm_dev) && (!ctx->iommu_on)) {
		/* wincon */
		fimd_win_channel_ctrl(ctx, win, false);

		/* unprotect windows */
		fimd_win_shadow_ctrl(ctx, win, FIMD_SC_BYPASS_CH_CTRL);
	} else {
		/* wincon */
		fimd_win_channel_ctrl(ctx, win, true);

		/* Enable DMA channel and unprotect windows */
		fimd_win_shadow_ctrl(ctx, win, FIMD_SC_CH_ENABLE);
	}

	win_data->enabled = true;

	if (ctx->i80_if) {
		unsigned long flags;

		spin_lock_irqsave(&ctx->win_updated_lock, flags);

		if (!atomic_read(&ctx->do_apply))
			atomic_set(&ctx->win_update, 1);

		spin_unlock_irqrestore(&ctx->win_updated_lock, flags);
	}

	DRM_DEBUG("%s:win[%d]a[0x%x]val[%d %d]dpms[%d]bl_on[%d]\n",
		"commit", win,
		readl(ctx->regs + VIDWx_BUF_START(win, 0)),
		atomic_read(&ctx->win_update),
		atomic_read(&ctx->do_apply),
		ctx->dpms, ctx->bl_on);
}

#ifdef CONFIG_DRM_EXYNOS_FIMD_QOS
static void fimd_qos_ctrl(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data;
	int win, val, count = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	for (win = 0; win < WINDOWS_NR; win++) {
		win_data = &ctx->win_data[win];
		if (win_data->enabled)
			count++;
	}

	switch (count) {
	case 2:
		val = 160160;
		break;
	case 3 ... WINDOWS_NR:
		val = 267160;
		break;
	default:
		val = 0;
		break;
	}

	DRM_DEBUG_KMS("%s:count[%d]val[%d]\n", __func__, count, val);

	pm_qos_update_request(&ctx->pm_ovly_qos, val);
}
#endif

static int fimd_iommu_ctrl(struct fimd_context *ctx, bool on)
{
	struct device *client = ctx->subdrv.dev;
	struct drm_device *drm_dev = ctx->subdrv.drm_dev;
	int ret;

	/* check if iommu is enabled or not and first call or not. */
	if (!is_drm_iommu_supported(drm_dev))
		return 0;

	DRM_INFO("%s[%d->%d]\n", __func__, ctx->iommu_on, on);

	if (ctx->iommu_on == on)
		return 0;

	fimd_wait_for_vblank(client);

	if (on) {
		/* enable fimd's iommu. */
		ret = drm_iommu_attach_device(drm_dev, client);
		if (ret < 0) {
			DRM_ERROR("failed to enable iommu.\n");
			return ret;
		}
	} else
		drm_iommu_detach_device(drm_dev, client);

	ctx->iommu_on = on;

	DRM_INFO("%s[%d->%d]done\n", __func__, ctx->iommu_on, on);

	return 0;
}

static void fimd_win_enable(struct device *dev, int zpos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct drm_device *drm_dev = ctx->subdrv.drm_dev;
	int win = zpos;
#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
	u32 val;
#endif

	if (win == DEFAULT_ZPOS)
		win = ctx->default_win;

	if (win < 0 || win >= WINDOWS_NR)
		return;

	DRM_INFO("%s:win[%d]dpms[%d]lpm[%d]\n", __func__,
		win, ctx->dpms, fimd_is_lpm(ctx));

	if (fimd_set_runtime_activate(dev, "win_on")) {
		DRM_ERROR("invalid:lpm\n");
		return;
	}

	 if (is_drm_iommu_supported(drm_dev) && (!ctx->iommu_on)) {
		/* now try to change it into iommu mode. */
		fimd_iommu_ctrl(ctx, true);

		/* protect windows */
		fimd_win_shadow_ctrl(ctx, win, FIMD_SC_PROTECT);

		/* wincon */
		fimd_win_channel_ctrl(ctx, win, true);

		/* Enable DMA channel and unprotect windows */
		fimd_win_shadow_ctrl(ctx, win, FIMD_SC_CH_ENABLE);
	 }

#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
	val = readl(ctx->regs + VIDINTCON0);

	switch (win) {
	case 0:
		val |= VIDINTCON0_FIFIOSEL_WINDOW0;
		break;
	case 1:
		val |= VIDINTCON0_FIFIOSEL_WINDOW1;
		break;
	case 2:
		val |= VIDINTCON0_FIFIOSEL_WINDOW2;
		break;
	case 3:
		val |= VIDINTCON0_FIFIOSEL_WINDOW3;
		break;
	case 4:
		val |= VIDINTCON0_FIFIOSEL_WINDOW4;
		break;
	default:
		break;
	}

	writel(val, ctx->regs + VIDINTCON0);
#endif

#ifdef CONFIG_DRM_EXYNOS_FIMD_QOS
	fimd_qos_ctrl(dev);
#endif
}

static void fimd_win_disable(struct device *dev, int zpos)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data;
	int win = zpos;
#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
	u32 val;
#endif

	DRM_DEBUG_KMS("%s\n", __func__);

	if (win == DEFAULT_ZPOS)
		win = ctx->default_win;

	if (win < 0 || win >= WINDOWS_NR)
		return;

	win_data = &ctx->win_data[win];

	DRM_INFO("%s:win[%d]dpms[%d]lpm[%d]\n", __func__,
		win, ctx->dpms, fimd_is_lpm(ctx));

	if (ctx->dpms > DRM_MODE_DPMS_STANDBY) {
		DRM_INFO("%s:invalid:dpms[%d]\n", __func__, ctx->dpms);
		goto out;
	}

	if (fimd_set_runtime_activate(dev, "win_off")) {
		DRM_ERROR("invalid:lpm\n");
		goto out;
	}

	/* protect windows */
	fimd_win_shadow_ctrl(ctx, win, FIMD_SC_PROTECT);

	/* wincon */
	fimd_win_channel_ctrl(ctx, win, false);

	/* Disable DMA channel and unprotect windows */
	fimd_win_shadow_ctrl(ctx, win, FIMD_SC_CH_DISABLE);

#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
	val = readl(ctx->regs + VIDINTCON0);

	switch (win) {
	case 0:
		val &= ~VIDINTCON0_FIFIOSEL_WINDOW0;
		break;
	case 1:
		val &= ~VIDINTCON0_FIFIOSEL_WINDOW1;
		break;
	case 2:
		val &= ~VIDINTCON0_FIFIOSEL_WINDOW2;
		break;
	case 3:
		val &= ~VIDINTCON0_FIFIOSEL_WINDOW3;
		break;
	case 4:
		val &= ~VIDINTCON0_FIFIOSEL_WINDOW4;
		break;
	default:
		break;
	}

	writel(val, ctx->regs + VIDINTCON0);
#endif

#ifdef CONFIG_DRM_EXYNOS_FIMD_QOS
	fimd_qos_ctrl(dev);
#endif

out:
	win_data->enabled = false;
}

static struct exynos_drm_overlay_ops fimd_overlay_ops = {
	.mode_set = fimd_win_mode_set,
	.commit = fimd_win_commit,
	.enable = fimd_win_enable,
	.disable = fimd_win_disable,
	.partial_resolution = fimd_partial_resolution,
	.request_partial_update = fimd_request_partial_update,
	.adjust_partial_region = fimd_adjust_partial_region,
};

static struct exynos_drm_manager fimd_manager = {
	.pipe		= -1,
	.ops		= &fimd_manager_ops,
	.overlay_ops	= &fimd_overlay_ops,
	.display_ops	= &fimd_display_ops,
};

#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
static void fimd_check_fifo(struct fimd_context *ctx)
{
	struct fimd_win_data *win_data;
	int win;

	DRM_DEBUG_KMS("%s\n", __func__);

	for (win = 0; win < WINDOWS_NR; win++) {
		win_data = &ctx->win_data[win];
		if (win_data->enabled)
			DRM_ERROR("occured %d FIFO empty interrupt.\n", win);
	}
}
#endif

static irqreturn_t fimd_irq_handler(int irq, void *dev_id)
{
	struct fimd_context *ctx = (struct fimd_context *)dev_id;
	struct exynos_drm_subdrv *subdrv = &ctx->subdrv;
	struct exynos_drm_manager *manager = subdrv->manager;
	struct drm_device *drm_dev = ctx->drm_dev;
	u32 val, clear_bit;

#ifdef FIMD_AGING_LOG
	fimd_save_timing(FIMD_LOG_ISR);
#endif

	val = readl(ctx->regs + VIDINTCON1);

	DRM_DEBUG_KMS("[%s]:val[0x%08x]\n", __func__, val);

	clear_bit = ctx->i80_if ? VIDINTCON1_INT_I180 : VIDINTCON1_INT_FRAME;

	if (val & clear_bit)
		writel(clear_bit, ctx->regs + VIDINTCON1);

	/* check the crtc is detached already from encoder */
	if (manager->pipe < 0) {
		DRM_DEBUG("crtc is detached\n");
		goto out;
	}

	if (!ctx->i80_if) {
		drm_handle_vblank(drm_dev, ctx->pipe);
		exynos_drm_crtc_finish_pageflip(drm_dev, ctx->pipe);

		/* set wait vsync event to zero and wake up queue. */
		if (atomic_read(&ctx->wait_vsync_event)) {
			atomic_set(&ctx->wait_vsync_event, 0);
			DRM_WAKEUP(&ctx->wait_vsync_queue);
		}
	} else {
		/* unset i80 fame done interrupt */
		val = readl(ctx->regs + VIDINTCON0);
		val &= ~(VIDINTCON0_INT_I80IFDONE | VIDINTCON0_INT_SYSMAINCON);

		writel(val, ctx->regs + VIDINTCON0);

		/* exit triggering mode */
		if (atomic_read(&ctx->wait_done_event)) {
			atomic_set(&ctx->wait_done_event, 0);
			DRM_WAKEUP(&ctx->wait_done_queue);
		}
	}

#ifdef CONFIG_DRM_EXYNOS_FIMD_DEBUG_FIFO
	if (val & VIDINTCON1_INT_FIFO) {
		writel(VIDINTCON1_INT_FIFO, ctx->regs + VIDINTCON1);
		fimd_check_fifo(ctx);
	}
#endif

       if (ctx->dbg_cnt) {
	   	DRM_INFO("fimd_irq:c[%d]\n", ctx->dbg_cnt--);
		fimd_check_event(ctx, "fimd_irq");
       }
out:
	return IRQ_HANDLED;
}

static void fimd_clear_win(struct fimd_context *ctx, int win)
{
	u32 val;

	DRM_INFO("%s:win[%d]\n", __func__, win);

	writel(0, ctx->regs + WINCON(win));
	writel(0, ctx->regs + VIDOSD_A(win));
	writel(0, ctx->regs + VIDOSD_B(win));
	writel(0, ctx->regs + VIDOSD_C(win));

	if (win == 1 || win == 2)
		writel(0, ctx->regs + VIDOSD_D(win));

	writel(0, ctx->regs + VIDWx_BUF_START(win, 0));
	writel(0, ctx->regs + VIDWx_BUF_END(win, 0));

	val = readl(ctx->regs + SHADOWCON);
	val &= ~(SHADOWCON_CHx_ENABLE(win) |
		SHADOWCON_WINx_PROTECT(win));
	writel(val, ctx->regs + SHADOWCON);
}

static void fimd_set_full_screen_mode(struct fimd_context *ctx)
{
	struct exynos_drm_panel_info *panel = ctx->panel;
	struct fb_videomode *timing = &panel->timing;
	struct fimd_win_data *win_data;
	int i;

	/* FixMe: need to check page-fault */
	for (i = 0; i < WINDOWS_NR; i++) {
		win_data = &ctx->win_data[i];

		/*
		 * Change overlay region to full screen
		 * if the overly was enabled.
		 */
		if (i == ctx->default_win &&
			win_data->enabled) {
			win_data->offset_x = 0;
			win_data->offset_y = 0;
			win_data->ovl_width = timing->xres;
			win_data->ovl_height = timing->yres;
			win_data->dma_addr = win_data->base_dma_addr;
			win_data->buf_offsize = (win_data->fb_width - timing->xres) *
							(win_data->bpp >> 3);
			win_data->line_size = timing->xres * (win_data->bpp >> 3);
			DRM_DEBUG("%s:win[%d]\n", __func__, i);
		}
	}
}

static int fimd_clock(struct fimd_context *ctx, bool enable)
{
	DRM_DEBUG("%s:enable[%d]\n", __func__, enable);

	if (enable) {
		int ret;

		ret = clk_enable(ctx->bus_clk);
		if (ret < 0)
			return ret;

		ret = clk_enable(ctx->lcd_clk);
		if  (ret < 0) {
			clk_disable(ctx->bus_clk);
			return ret;
		}
	} else {
		clk_disable(ctx->lcd_clk);
		clk_disable(ctx->bus_clk);
	}

	return 0;
}

static int fimd_active(struct fimd_context *ctx, bool enable)
{
	struct exynos_drm_subdrv *subdrv = &ctx->subdrv;
	struct device *dev = subdrv->dev;
	int ret = 0;

	if (enable != false && enable != true) {
		ret = -EINVAL;
		goto err;
	}

	if (enable) {
		int ret;

		ret = fimd_clock(ctx, true);
		if (ret < 0)
			goto err;

		fimd_vblank_ctrl(ctx, true);

		fimd_apply(dev);
	} else {
#ifdef CONFIG_MDNIE_SUPPORT
		ctx->mdnie_enabled = false;

		if (fimd_lite_dev)
			exynos_drm_mdnie_mode_stop(ctx);
#endif
		fimd_clock(ctx, false);

		fimd_vblank_ctrl(ctx, false);
		fimd_set_full_screen_mode(ctx);
	}

err:
	if (ret) {
		DRM_ERROR("enable[%d]ret[%d]\n", enable,ret);
		fimd_check_event(ctx, "fimd_active:err");
	}

	return ret;
}

static int fimd_configure_clocks(struct fimd_context *ctx, struct device *dev)
{
	struct exynos_drm_panel_info *panel = ctx->panel;
	struct fb_videomode *timing = &panel->timing;
	unsigned long clk;
	int ret;

	ctx->bus_clk = clk_get(dev, "fimd");
	if (IS_ERR(ctx->bus_clk)) {
		dev_err(dev, "failed to get bus clock\n");
		ret = PTR_ERR(ctx->bus_clk);
		goto err_disable_block_clk;
	}

	ret = clk_prepare(ctx->bus_clk);
	if (ret) {
		dev_err(dev, "failed to prepare bus clock\n");
		goto err_disable_block_clk;
	}

	ctx->lcd_clk = clk_get(dev, "sclk_fimd");
	if (IS_ERR(ctx->lcd_clk)) {
		dev_err(dev, "failed to get lcd clock\n");
		ret = PTR_ERR(ctx->lcd_clk);
		goto err_unprepare_bus_clk;
	}

	ret = clk_prepare(ctx->lcd_clk);
	if (ret) {
		dev_err(dev, "failed to prepare lcd clock\n");
		goto err_unprepare_bus_clk;
	}

	clk = clk_get_rate(ctx->lcd_clk);
	if (clk == 0) {
		dev_err(dev, "error getting sclk_fimd clock rate\n");
		ret = -EINVAL;
		goto err_unprepare_lcd_clk;
	}

	if (timing->pixclock == 0) {
		unsigned long c;
		if (ctx->i80_if) {
			c = timing->xres * timing->yres *
			    (timing->cs_setup_time +
			    timing->wr_setup_time + timing->wr_act_time +
			    timing->wr_hold_time + 1);

			/*
			 * Basically, the refresh rate of TE and vsync is 60Hz.
			 * In case of using i80 lcd panel, we need to do fimd
			 * trigger so that graphics ram in the lcd panel to be
			 * updated.
			 *
			 * And, we do fimd trigger every time TE interrupt
			 * handler occurs to resolve tearing issue on the lcd
			 * panel. However, this way doesn't avoid the tearing
			 * issue because fimd i80 frame done interrupt doesn't
			 * occur since fimd trigger before next TE interrupt.
			 *
			 * So makes video clock speed up two times so that the
			 * fimd i80 frame done interrupt can occur prior to
			 * next TE interrupt.
			 */
			c *= 2;
		} else {
			c = timing->xres + timing->left_margin + timing->right_margin +
			    timing->hsync_len;
			c *= timing->yres + timing->upper_margin + timing->lower_margin +
			     timing->vsync_len;
		}

		timing->pixclock = c * FIMD_HIGH_FRAMERATE;
		if (timing->pixclock == 0) {
			dev_err(dev, "incorrect display timings\n");
			ret = -EINVAL;
			goto err_unprepare_lcd_clk;
		}
	}

	ctx->clkdiv = DIV_ROUND_UP(clk, timing->pixclock);
	if (ctx->clkdiv > 256) {
		dev_warn(dev, "calculated pixel clock divider too high (%u), lowered to 256\n",
			 ctx->clkdiv);
		ctx->clkdiv = 256;
	}
	timing->pixclock = clk / ctx->clkdiv;
	DRM_INFO("%s:pixel clock = %d, clkdiv = %d\n", __func__, timing->pixclock,
		       ctx->clkdiv);

	return 0;

err_unprepare_lcd_clk:
	clk_unprepare(ctx->lcd_clk);
err_unprepare_bus_clk:
	clk_unprepare(ctx->bus_clk);
err_disable_block_clk:

	return ret;
}

static struct clk *fimd_get_clk(struct dispfreq_device *dfd)
{
	struct fimd_context *ctx = dispfreq_get_data(dfd);
	struct clk *clk;

	clk = ctx->lcd_clk;

	return clk;
}

static int fimd_set_clk(struct dispfreq_device *dfd,
		struct clksrc_clk *clksrc, int div)
{
	struct fimd_context *ctx = dispfreq_get_data(dfd);
	u32 mask = reg_mask(clksrc->reg_div.shift, clksrc->reg_div.size);
	u32 reg =  __raw_readl(clksrc->reg_div.reg);
	unsigned int val;

	if ((reg & mask) == (div & mask)) {
		DRM_ERROR("div is same as reg.\n");
		return -EINVAL;
	}

	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		goto out;
	}

	reg &= ~(0xff);
	reg |= div;

	while (1) {
		val = (__raw_readl(ctx->regs + VIDCON1) &
			VIDCON1_VSTATUS_MASK);
		if (val == VIDCON1_VSTATUS_VSYNC) {
			writel(reg, clksrc->reg_div.reg);
			break;
		}
	}

out:
	return 0;
}

static int fimd_get_div(struct dispfreq_device *dfd)
{
	struct fimd_context *ctx = dispfreq_get_data(dfd);
	unsigned int val = -1;

	if (fimd_is_lpm(ctx)) {
		DRM_ERROR("invalid:lpm\n");
		goto out;
	}

	if (ctx->regs) {
		val = readl(ctx->regs + VIDCON0);
		val &= VIDCON0_CLKVAL_F_MASK;
		val >>= VIDCON0_CLKVAL_F_SHIFT;
	}

out:
	return val;
}

static u32 fimd_get_refresh(struct dispfreq_device *dfd)
{
	struct dispfreq_properties *props = &dfd->props;
	struct fb_videomode *timing = props->timing;

	return timing->refresh;
}

static int fimd_get_pm_state(struct dispfreq_device *dfd)
{
	struct fimd_context *ctx = dispfreq_get_data(dfd);

	return !fimd_is_lpm(ctx);
}

static const struct dispfreq_ops fimd_dispfreq_ops = {
	.get_clk = fimd_get_clk,
	.set_clk = fimd_set_clk,
	.get_fimd_div = fimd_get_div,
	.get_refresh = fimd_get_refresh,
	.get_pm_state = fimd_get_pm_state,
};

#ifdef CONFIG_DRM_EXYNOS_IPP
static void fimd_set_writeback(struct fimd_context *ctx, int enable,
		unsigned int refresh)
{
	u32 vidoutcon = readl(ctx->regs + VIDOUT_CON);
	u32 vidcon2 = readl(ctx->regs + VIDCON2);

	DRM_INFO("%s:wb[%d]refresh[%d]\n",
		__func__, enable, refresh);

	vidoutcon &= ~VIDOUT_CON_F_MASK;
	vidcon2 &= ~(VIDCON2_WB_MASK |
			VIDCON2_WB_SKIP_MASK |
			VIDCON2_TVFORMATSEL_HW_SW_MASK);

	if (enable) {
		vidoutcon |= VIDOUT_CON_WB;
		if (ctx->i80_if)
			vidoutcon |= VIDOUT_CON_F_I80_LDI0;
		vidcon2 |= (VIDCON2_WB_ENABLE |
				VIDCON2_TVFORMATSEL_SW);

		if (refresh >= 60 || refresh == 0)
			DRM_INFO("%s:refresh[%d],forced set to 60hz.\n",
				__func__, refresh);
		else if (refresh >= 30)
			vidcon2 |= VIDCON2_WB_SKIP_1_2;
		else if (refresh >= 20)
			vidcon2 |= VIDCON2_WB_SKIP_1_3;
		else if (refresh >= 15)
			vidcon2 |= VIDCON2_WB_SKIP_1_4;
		else
			vidcon2 |= VIDCON2_WB_SKIP_1_5;
	} else {
		if (ctx->i80_if)
			vidoutcon |= VIDOUT_CON_F_I80_LDI0;
		else
			vidoutcon |= VIDOUT_CON_RGB;
		vidcon2 |= VIDCON2_WB_DISABLE;
	}

	writel(vidoutcon, ctx->regs + VIDOUT_CON);
	writel(vidcon2, ctx->regs + VIDCON2);
}

static void fimd_set_output(struct fimd_context *ctx,
		struct exynos_drm_overlay *overlay)
{
	struct fimd_win_data *win_data;
	struct device *dev = ctx->subdrv.dev;

	DRM_DEBUG_KMS("%s:zpos[%d]activated[%d]\n", __func__,
		overlay->zpos, overlay->activated);

	win_data = &ctx->win_data[overlay->zpos];

	if (overlay->activated) {
		fimd_win_mode_set(dev, overlay);
		fimd_win_commit(dev, overlay->zpos);
		fimd_win_enable(dev, overlay->zpos);
	} else
		fimd_win_disable(dev, overlay->zpos);
}

static int fimd_notifier_ctrl(struct notifier_block *this,
			unsigned long event, void *_data)
{
	struct fimd_context *ctx = container_of(this,
				struct fimd_context, nb_ctrl);

	switch (event) {
	case IPP_GET_LCD_WIDTH: {
		struct exynos_drm_panel_info *panel = ctx->panel;
		struct fb_videomode *timing = &panel->timing;
		int *width = (int *)_data;

		*width = timing->xres;
	}
		break;
	case IPP_GET_LCD_HEIGHT: {
		struct exynos_drm_panel_info *panel = ctx->panel;
		struct fb_videomode *timing = &panel->timing;
		int *height = (int *)_data;

		*height = timing->yres;
	}
		break;
	case IPP_SET_WRITEBACK: {
		struct drm_exynos_ipp_set_wb *set_wb =
			(struct drm_exynos_ipp_set_wb *)_data;
		unsigned int refresh = set_wb->refresh;
		int enable = *((int *)&set_wb->enable);

		fimd_set_writeback(ctx, enable, refresh);
	}
		break;
	case IPP_SET_OUTPUT: {
		struct exynos_drm_overlay *overlay =
			(struct exynos_drm_overlay *)_data;

		fimd_set_output(ctx, overlay);
	}
		break;
	default:
		/* ToDo : for checking use case */
		DRM_INFO("%s:event[0x%x]\n", __func__, (unsigned int)event);
		break;
	}

	return NOTIFY_DONE;
}
#endif

#ifdef CONFIG_DRM_EXYNOS_DBG
static int fimd_read_reg(struct fimd_context *ctx, char *buf)
{
	struct fimd_driver_data *driver_data = ctx->driver_data;
	struct resource *res = ctx->regs_res;
	int i, pos = 0;
	u32 cfg;

	pos += sprintf(buf+pos, "0x%.8x | ", res->start);
	for (i = 1; i < FIMD_MAX_REG + 1; i++) {
		cfg = readl(ctx->regs + ((i-1) * sizeof(u32)));
		pos += sprintf(buf+pos, "0x%.8x ", cfg);
		if (i % 4 == 0)
			pos += sprintf(buf+pos, "\n0x%.8x | ",
				res->start + (i * sizeof(u32)));
	}
	pos += sprintf(buf+pos, "\n");

	if (driver_data->timing_base) {
		pos += sprintf(buf+pos, "0x%.8x | ", res->start
			+ driver_data->timing_base);
		for (i = 1; i < FIMD_MAX_REG + 1; i++) {
			cfg = readl(ctx->regs + ctx->driver_data->timing_base
				+ ((i-1) * sizeof(u32)));
			pos += sprintf(buf+pos, "0x%.8x ", cfg);
			if (i % 4 == 0)
				pos += sprintf(buf+pos, "\n0x%.8x | ",
					res->start + (i * sizeof(u32)));
		}

		pos += sprintf(buf+pos, "\n");
	}

	return pos;
}

static ssize_t show_read_reg(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct fimd_context *ctx = get_fimd_context(dev);

	if (!ctx->regs) {
		dev_err(dev, "failed to get current register.\n");
		return -EINVAL;
	}

	return fimd_read_reg(ctx, buf);
}

static ssize_t show_overlay_info(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct fimd_win_data *win_data;
	int win, pos = 0;

	for (win = 0; win < WINDOWS_NR; win++) {
		win_data = &ctx->win_data[win];

		if (win_data->enabled) {
			pos += sprintf(buf+pos, "Overlay %d: ", win);
			pos += sprintf(buf+pos, "x[%d],y[%d],w[%d],h[%d],",
				win_data->offset_x, win_data->offset_y,
				win_data->ovl_width, win_data->ovl_height);
			pos += sprintf(buf+pos, "fb_width[%d],fb_height[%d]\n",
				win_data->fb_width, win_data->fb_height);
		}
	}

	return pos;
}

static int read_tune_reg(struct device *dev, char *buf,
			unsigned int start, unsigned int end)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	int reg_num;
	int cnt, offset;
	int pos = 0;

	cnt = 0;
	reg_num = ((end - start) / 4) + 1;
	pos += sprintf(buf+pos, "reg_num: %d\n", reg_num);
	do {
		offset = start + (cnt * 4);
		pos += sprintf(buf+pos, "%08x ", readl(ctx->regs + offset));
		cnt++;
		if (cnt%4 == 0)
			pos += sprintf(buf+pos, "\n");
	} while (reg_num - cnt);

	pos += sprintf(buf+pos, "\n");

	return pos;
}

static void gamma64_to_reg(struct device *dev, const u8 *data)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	unsigned int val1, val2, val3, val4, val5 = 0 ;
	int ret;
	char *str = NULL;
	int i = 0;
	u32 gamma_64[65];
	u32 reg_gamma[33];

	while ((str = strsep((char **)&data, "\n"))) {
		ret = sscanf(str, "%d,%d,%d,%d,%d,\n",
			&val1, &val2, &val3, &val4, &val5);
		if (ret == 5) {
			gamma_64[i] = val1;
			gamma_64[i + 1] = val2;
			gamma_64[i + 2] = val3;
			gamma_64[i + 3] = val4;
			gamma_64[i + 4] = val5;
			i = i + 5;
			if ((val1 == 0xff) && (val2 == 0) && (val3 == 0)
				&& (val4 == 0) && (val5 == 0))
				break;
		}
	}

	for (i = 0; i < 33; i++) {
		if (i == 32) {
			reg_gamma[32] =  gamma_64[2*i] << 2;
			break;
		}
		reg_gamma[i] = (gamma_64[2*i] << 2) | (gamma_64[2*i + 1] << 18);
	}

	memcpy(ctx->reg_gamma, reg_gamma, sizeof(reg_gamma));

}

static int fimd_get_tune(struct device *dev, const char *name, int mode)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	const struct firmware *fw;
	char fw_path[256];
	int ret;

	mutex_lock(&ctx->lock);
	ret = request_firmware((const struct firmware **)&fw, name, ctx->dev);
	if (ret) {
		dev_err(ctx->dev, "ret: %d, %s: fail to request %s\n",
					ret, __func__, fw_path);
		mutex_unlock(&ctx->lock);
		return ret;
	}
	switch (mode) {
	case TUNE_ENABLE:
		sscanf(fw->data, "0x%x\n", &ctx->vidcon3);
		break;
	case COLOR_GAIN:
		sscanf(fw->data, "0x%x\n", &ctx->reg_gain);
		break;
	case GAMMA_64:
		gamma64_to_reg(dev, fw->data);
		break;
	default:
		break;
	}

	release_firmware(fw);
	mutex_unlock(&ctx->lock);

	return 0;
}

static ssize_t show_fimd_tune(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	int ret = 0;

	for (i = 0; i < FIMD_TUNE_REG_NUM; i++) {
		ret += sprintf(buf + ret, "%s :\n",
				fimd_tune_info[i].name);
		ret += read_tune_reg(dev, buf+ret,
				fimd_tune_info[i].start, fimd_tune_info[i].end);
	}

	return ret;
}

static int store_fimd_tune(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* To Be Designed
	* gamma 16 step and
	* Hue tune.
	*/
	if (!strncmp(buf, "enable", 6))
		fimd_get_tune(dev, "tune_enable.dat", TUNE_ENABLE);
	else if (!strncmp(buf, "gain", 4))
		fimd_get_tune(dev, "color_gain.dat", COLOR_GAIN);
	else if (!strncmp(buf, "gamma_64", 8))
		fimd_get_tune(dev, "gamma_64.dat", GAMMA_64);
	else if (!strncmp(buf, "all", 3)) {
		fimd_get_tune(dev, "tune_enable.dat", TUNE_ENABLE);
		fimd_get_tune(dev, "color_gain.dat", COLOR_GAIN);
		fimd_get_tune(dev, "gamma_64.dat", GAMMA_64);
	} else {
		dev_warn(dev, "invalid command.\n");
		return size;
	}

	fimd_set_gamma_acivate(dev, false);
	fimd_set_gamma_acivate(dev, true);

	return size;
}

static struct device_attribute device_attrs[] = {
	__ATTR(read_reg, S_IRUGO, show_read_reg, NULL),
	__ATTR(overlay_info, S_IRUGO, show_overlay_info, NULL),
	__ATTR(fimd_tune, S_IRUGO | S_IWUGO,  show_fimd_tune, store_fimd_tune),
};
#endif

#ifdef CONFIG_SLEEP_MONITOR
int fimd_get_sleep_monitor_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	struct fimd_context *ctx = priv;
	int state = DEVICE_UNKNOWN;
	int mask = (1 << SLEEP_MONITOR_DEVICE_BIT_WIDTH) - 1;
	unsigned int cnt = 0;
	int ret;

	switch (ctx->dpms) {
	case DRM_MODE_DPMS_ON:
	case DRM_MODE_DPMS_STANDBY:
		state = DEVICE_ON_ACTIVE1;
		break;
	case DRM_MODE_DPMS_SUSPEND:
		state = DEVICE_ON_LOW_POWER;
		break;
	case DRM_MODE_DPMS_OFF:
		state = DEVICE_POWER_OFF;
		break;
	default:
		break;
	}

	switch (caller_type) {
	case SLEEP_MONITOR_CALL_SUSPEND:
	case SLEEP_MONITOR_CALL_RESUME:
		cnt = ctx->act_cnt;
		if (state == DEVICE_ON_LOW_POWER)
			cnt++;
		break;
	case SLEEP_MONITOR_CALL_ETC:
		break;
	default:
		break;
	}

	*raw_val = cnt | state << 24;

	/* panel on count 1~15*/
	if (cnt > mask)
		ret = mask;
	else
		ret = cnt;

	DRM_INFO("%s: caller[%d], dpms[%d] panel on[%d], raw[0x%x]\n",
			__func__, caller_type, ctx->dpms, ret, *raw_val);

	ctx->act_cnt = 0;

	return ret;
}

static struct sleep_monitor_ops fimd_sleep_monitor_ops = {
	.read_cb_func = fimd_get_sleep_monitor_cb,
};
#endif

static int __devinit fimd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fimd_context *ctx;
	struct exynos_drm_subdrv *subdrv;
	struct exynos_drm_fimd_pdata *pdata;
	struct exynos_drm_panel_info *panel;
	struct dispfreq_properties props;
	struct fb_videomode *timing;
	struct resource *res;
	struct fimd_lpm_work *lpm_work;
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	struct fimd_secure_work *secure_work;
#endif
	int win;
	int ret = -EINVAL;

	DRM_DEBUG_KMS("%s\n", __func__);

	pdata = pdev->dev.platform_data;
	if (!pdata) {
		dev_err(dev, "no platform data specified\n");
		return -EINVAL;
	}

	panel = pdata->panel;
	if (!panel) {
		dev_err(dev, "panel is null.\n");
		return -EINVAL;
	}

	timing = &panel->timing;
	if (!timing) {
		dev_err(dev, "timing is null.\n");
		return -EINVAL;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->disp_bus_pdev = pdata->disp_bus_pdev;
	pdata->trigger = fimd_trigger;
	pdata->wait_for_frame_done = fimd_wait_for_vblank;
	pdata->stop_trigger = fimd_stop_trigger;
	pdata->set_runtime_activate = fimd_set_runtime_activate;
	pdata->update_panel_refresh = fimd_update_panel_refresh;
	pdata->set_gamma_acivate = fimd_set_gamma_acivate;
	pdata->te_handler= fimd_te_handler;
#ifdef CONFIG_EXYNOS_SMIES
#ifdef CONFIG_EXYNOS_SMIES_RUNTIME_ACTIVE
	pdata->set_smies_activate = fimd_set_smies_activate;
#else
	pdata->set_smies_mode = fimd_set_smies_mode;
	atomic_set(&ctx->smies_active, 1);
#endif
#endif

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "failed to find registers\n");
		ret = -ENOENT;
		goto err_ctx;
	}

	ctx->regs_res = request_mem_region(res->start, resource_size(res),
					   dev_name(dev));
	if (!ctx->regs_res) {
		dev_err(dev, "failed to claim register region\n");
		ret = -ENOENT;
		goto err_ctx;
	}

	ctx->regs = ioremap(res->start, resource_size(res));
	if (!ctx->regs) {
		dev_err(dev, "failed to map registers\n");
		ret = -ENXIO;
		goto err_req_region_io;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						timing->i80en ? "lcd_sys" : "vsync");
	if (!res) {
		dev_err(dev, "irq i80_if irq failed.\n");
		goto err_req_region_irq;
	}

	ctx->irq = res->start;
	ret = request_irq(ctx->irq, fimd_irq_handler, 0, "drm_fimd", ctx);
	if (ret < 0) {
		dev_err(dev, "irq request failed.\n");
		goto err_req_region_irq;
	}

	ctx->driver_data = drm_fimd_get_driver_data(pdev);
	ctx->vidcon0 = pdata->vidcon0;
	ctx->vidcon1 = pdata->vidcon1;
	ctx->i80_if = timing->i80en;
	if (ctx->i80_if) {
		pdata->i80ifcon = (LCD_CS_SETUP(timing->cs_setup_time)
						| LCD_WR_SETUP(timing->wr_setup_time)
						| LCD_WR_ACT(timing->wr_act_time)
						| LCD_WR_HOLD(timing->wr_hold_time)
						| LCD_WR_RS_POL(timing->rs_pol));
		ctx->i80ifcon = pdata->i80ifcon;
	}
	ctx->default_win = pdata->default_win;
	ctx->panel = panel;
	ctx->dev = &pdev->dev;
	ctx->mdnie_enabled = pdata->mdnie_enabled;
#ifdef CONFIG_EXYNOS_SMIES
	ctx->smies_device = pdata->smies_device;
	ctx->smies_on = pdata->smies_on;
	ctx->smies_off=  pdata->smies_off;
	ctx->smies_mode=  pdata->smies_mode;
#endif
	ctx->vidcon3 = pdata->vidcon3;
	ctx->reg_gamma = pdata->reg_gamma;
	ctx->reg_gain =  pdata->reg_gain;

	DRM_INIT_WAITQUEUE(&ctx->wait_vsync_queue);
	atomic_set(&ctx->wait_vsync_event, 0);
	DRM_INIT_WAITQUEUE(&ctx->wait_done_queue);
	atomic_set(&ctx->wait_done_event, 0);

	props.timing = &panel->timing;
	props.refresh = props.timing->refresh;
	props.max_refresh = pdata->max_refresh;
	props.min_refresh = pdata->min_refresh;

#ifdef CONFIG_DISPFREQ_CLASS_DEVICE
	ctx->dfd = dispfreq_device_register("exynos", &pdev->dev, ctx,
			&fimd_dispfreq_ops, &props);
	if (IS_ERR(ctx->dfd)) {
		dev_err(dev, "failed to register dispfreq.\n");
		ret = PTR_ERR(ctx->dfd);
		goto err_req_irq;
	}
#endif

	subdrv = &ctx->subdrv;

	subdrv->dev = dev;
	subdrv->manager = &fimd_manager;
	subdrv->probe = fimd_subdrv_probe;
	subdrv->remove = fimd_subdrv_remove;

#ifdef CONFIG_DRM_EXYNOS_IPP
	ctx->nb_ctrl.notifier_call = fimd_notifier_ctrl;
	ret = exynos_drm_ippnb_register(&ctx->nb_ctrl);
	if (ret) {
		dev_err(dev, "could not register fimd notify callback\n");
		goto err_req_irq;
	}
#endif

#ifdef CONFIG_DRM_EXYNOS_DBG
	exynos_drm_dbg_register(&pdev->dev, device_attrs,
			ARRAY_SIZE(device_attrs), ctx->regs);
	if (ctx->driver_data->timing_base)
		exynos_drm_dbg_register(&pdev->dev, NULL, 0,
				ctx->regs+ctx->driver_data->timing_base);
#endif

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(ctx, &fimd_sleep_monitor_ops,
		SLEEP_MONITOR_LCD);
#endif

	mutex_init(&ctx->lock);
	spin_lock_init(&ctx->win_updated_lock);

	platform_set_drvdata(pdev, ctx);

	ret = fimd_configure_clocks(ctx, dev);
	if (ret)
		goto err_req_irq;

	ctx->dpms = DRM_MODE_DPMS_ON;
#ifdef CONFIG_SLEEP_MONITOR
	ctx->act_cnt = 1;
#endif
	pm_runtime_enable(dev);
	fimd_runtime_get_sync(dev);

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	device_set_early_complete(dev, EARLY_COMP_MASTER);
#endif

	for (win = 0; win < WINDOWS_NR; win++)
		if (win != ctx->default_win)
			fimd_clear_win(ctx, win);

	exynos_drm_subdrv_register(subdrv);

	ctx->fimd_wq= create_singlethread_workqueue("fimd_wq");
	if (!ctx->fimd_wq) {
		DRM_ERROR("failed to create workq.\n");
		goto err_req_irq;
	}

	lpm_work = kzalloc(sizeof(*lpm_work), GFP_KERNEL);
	if (!lpm_work) {
		DRM_ERROR("failed to alloc lpm_work.\n");
		goto err_lp_work;
	}

	lpm_work->ctx = ctx;
	ctx->lpm_work = lpm_work;
	INIT_WORK((struct work_struct *)ctx->lpm_work,
		fimd_lpmode_work);

	init_completion(&ctx->lpm_comp);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	secure_work = kzalloc(sizeof(*secure_work), GFP_KERNEL);
	if (!secure_work) {
		DRM_ERROR("failed to alloc secure_work.\n");
		goto err_mode_work;
	}

	secure_work->ctx = ctx;
	ctx->secure_work = secure_work;
	INIT_WORK((struct work_struct *)ctx->secure_work,
		fimd_secure_work);

	init_completion(&ctx->secure_comp);
#endif

	return 0;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
err_mode_work:
	kfree(lpm_work);
#endif

err_lp_work:
	destroy_workqueue(ctx->fimd_wq);

#ifdef CONFIG_DISPFREQ_CLASS_DEVICE
err_req_irq:
	free_irq(ctx->irq, ctx);
#endif
err_req_region_irq:
	iounmap(ctx->regs);

err_req_region_io:
	release_resource(ctx->regs_res);
	kfree(ctx->regs_res);

err_ctx:
	kfree(ctx);
	return ret;
}

static int __devexit fimd_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fimd_context *ctx = platform_get_drvdata(pdev);
	bool lpm = fimd_is_lpm(ctx);

	DRM_DEBUG_KMS("%s\n", __func__);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	kfree(ctx->secure_work);
#endif

	kfree(ctx->lpm_work);
	destroy_workqueue(ctx->fimd_wq);

	exynos_drm_subdrv_unregister(&ctx->subdrv);

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_LCD);
#endif

#ifdef CONFIG_DRM_EXYNOS_DBG
	exynos_drm_dbg_unregister(&pdev->dev, device_attrs);
	if (ctx->driver_data->timing_base)
		exynos_drm_dbg_unregister(&pdev->dev, NULL);
#endif

#ifdef CONFIG_DRM_EXYNOS_IPP
	exynos_drm_ippnb_unregister(&ctx->nb_ctrl);
#endif

	if (lpm) {
		DRM_ERROR("invalid:lpm[%d]\n", lpm);
		goto out;
	}

	clk_disable(ctx->lcd_clk);
	clk_disable(ctx->bus_clk);

	pm_runtime_set_suspended(dev);
	fimd_runtime_put_sync(dev);
	ctx->dpms = DRM_MODE_DPMS_OFF;

out:
	pm_runtime_disable(dev);

	clk_put(ctx->lcd_clk);
	clk_put(ctx->bus_clk);

	iounmap(ctx->regs);
	release_resource(ctx->regs_res);
	kfree(ctx->regs_res);
	free_irq(ctx->irq, ctx);

	kfree(ctx);

	return 0;
}

static int fimd_runtime_get_sync(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct device *smies_device = ctx->smies_device;
	int ret = 0;

	if (!pm_runtime_suspended(dev)) {
		DRM_INFO("%s:bypass\n", __func__);
		goto out;
	}

	ret = pm_runtime_get_sync(dev);
	if (ret)
		DRM_INFO("%s:ret[%d]\n", __func__, ret);

	ret = fimd_active(ctx, true);
	if (ret) {
		DRM_ERROR("fimd_active:ret[%d]\n", ret);
		goto out;
	}

	if (atomic_read(&ctx->gamma_on))
		fimd_update_gamma(ctx->dev, true);

	if (atomic_read(&ctx->smies_active) && ctx->smies_on)
		ctx->smies_on(smies_device);

out:
	return ret;
}

static int fimd_runtime_put_sync(struct device *dev)
{
	struct fimd_context *ctx = get_fimd_context(dev);
	struct device *smies_device = ctx->smies_device;
	int ret = 0;

	if (pm_runtime_suspended(dev)) {
		DRM_INFO("%s:bypass\n", __func__);
		goto out;
	}

	if (atomic_read(&ctx->smies_active) && ctx->smies_off)
		ctx->smies_off(smies_device);

	if (atomic_read(&ctx->gamma_on))
		fimd_update_gamma(ctx->dev, false);

	ret = fimd_active(ctx, false);
	if (ret) {
		DRM_ERROR("fimd_active:ret[%d]\n", ret);
		goto out;
	}

	ret = pm_runtime_put_sync(dev);
	if (ret)
		DRM_ERROR("ret[%d]\n", ret);

out:
	return ret;
}

static struct platform_device_id fimd_driver_ids[] = {
	{
		.name		= "s3c64xx-fb",
		.driver_data	= (unsigned long)&s3c64xx_fimd_driver_data,
	}, {
		.name		= "exynos3-fb",
		.driver_data	= (unsigned long)&exynos3_fimd_driver_data,
	}, {
		.name		= "exynos4-fb",
		.driver_data	= (unsigned long)&exynos4_fimd_driver_data,
	}, {
		.name		= "exynos5-fb",
		.driver_data	= (unsigned long)&exynos5_fimd_driver_data,
	},
	{},
};

struct platform_driver fimd_driver = {
	.probe		= fimd_probe,
	.remove		= __devexit_p(fimd_remove),
	.id_table       = fimd_driver_ids,
	.driver		= {
		.name	= "exynos3-fb",
		.owner	= THIS_MODULE,
	},
};

