/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	JinYoung Jeon <jy0.jeon@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drm/drmP.h>
#include <tdm.h>
#include <tdm_irq.h>
#ifdef CONFIG_DRM_TDM_IRQ_EXYNOS
#include <tdm_irq_exynos.h>
#endif

static void
tdm_vblank_work(struct work_struct *work)
{
	struct tdm_vbl_work *vbl_work =
		(struct tdm_vbl_work *)work;
	struct tdm_private *tdm_priv = vbl_work->tdm_priv;
	struct tdm_nb_event *event = vbl_work->event;
	long int en;

	en = (long int)event->data;
	DRM_DEBUG("[vbl_work][%s]\n", en ? "on" : "off");

	tdm_nb_send_event(TDM_NOTI_VSYNC_CTRL,
		(void *)event);

	DRM_DEBUG("[vbl_work][%s]done\n", en ? "on" : "off");
	complete_all(&tdm_priv->vbl_comp);

	return;
}

static int tdm_irq_prepare_vblank(struct drm_device *drm_dev, int crtc, struct drm_file *file_priv)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	unsigned long irqflags;
	bool enabled;

	if (crtc >= TDM_CRTC_MAX) {
		DRM_ERROR("crtc[%d]\n", crtc);
		return -EINVAL;
	}

	spin_lock_irqsave(&drm_dev->vbl_lock, irqflags);
	enabled = drm_dev->vblank[crtc].enabled;
	spin_unlock_irqrestore(&drm_dev->vbl_lock, irqflags);

	if (!file_priv->is_master && !enabled) {
		DRM_DEBUG("[pre_vbl_%d]r[%d]p[%d]VBL_OFF\n", crtc,
			atomic_read(&drm_dev->vblank[crtc].refcount),
			atomic_read(&tdm_priv->vbl_permission[crtc]));
		return -EACCES;
	}

	if (file_priv->is_master)
		atomic_set(&tdm_priv->vbl_permission[crtc], 0);
	else
		atomic_inc(&tdm_priv->vbl_permission[crtc]);

	DRM_DEBUG("[pre_vbl_%d]en[%d]r[%d]p[%d]\n", crtc, enabled,
		atomic_read(&drm_dev->vblank[crtc].refcount),
		atomic_read(&tdm_priv->vbl_permission[crtc]));

	if (atomic_read(&tdm_priv->vbl_permission[crtc]) > VBL_MAX_PERMISSION) {
		DRM_DEBUG("[limit_vbl_%d]r[%d]p[%d]\n", crtc,
			atomic_read(&drm_dev->vblank[crtc].refcount),
			atomic_read(&tdm_priv->vbl_permission[crtc]));
		return -EACCES;
	}

	if (!enabled) {
		int ret, wait = !completion_done(&tdm_priv->vbl_comp);

		if (wait)
			DRM_INFO("[pre_vbl_%d]wait\n", crtc);

		ret = wait_for_completion_timeout(
				&tdm_priv->vbl_comp,
			msecs_to_jiffies(50));
		if (ret < 0)
			DRM_INFO("%s:ret[%d]\n", __func__, ret);
		else if (!ret)
			DRM_INFO("%s:timeout\n", __func__);
		if (ret <= 0)
			complete_all(&tdm_priv->vbl_comp);

		if (wait)
			DRM_INFO("[pre_vbl_%d]wait_done\n", crtc);
	}

	return 0;
}

static int tdm_irq_queue_vblank(struct tdm_private *tdm_priv, struct work_struct *work)
{
	int ret = 0;

	if (!completion_done(&tdm_priv->vbl_comp)) {
		DRM_INFO("[queue_vbl]completion:busy\n");
		ret = -EBUSY;
		goto out;
	}

	reinit_completion(&tdm_priv->vbl_comp);
	if (!queue_work(tdm_priv->vbl_wq, (struct work_struct *)work)) {
		DRM_INFO("[queue_vbl]queue_work:busy\n");
		ret = -EBUSY;
	}

out:
	return ret;
}

static int tdm_irq_enable_vblank(struct drm_device *drm_dev, int crtc)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	struct tdm_vbl_work *vbl_work = tdm_priv->vbl_work;
	int ret;

	if (crtc >= TDM_CRTC_MAX) {
		DRM_ERROR("crtc[%d]\n", crtc);
		return -EINVAL;
	}

	switch (crtc) {
	case TDM_CRTC_PRIMARY:
		if (tdm_priv->dpms[crtc] != DRM_MODE_DPMS_ON) {
			DRM_INFO("[on_vbl_%d]DPMS_OFF\n", crtc);
			return -EPERM;
		}

		vbl_work->event->data = (void *)1;
		ret = tdm_irq_queue_vblank(tdm_priv, (struct work_struct *)vbl_work);
		break;
	case TDM_CRTC_VIRTUAL:
		ret = tdm_irq_queue_vblank(tdm_priv, &tdm_priv->virtual_vbl_work);
		break;
	default:
		break;
	}

	DRM_DEBUG("[on_vbl_%d]r[%d]%s\n", crtc,
		atomic_read(&drm_dev->vblank[crtc].refcount), ret ? "[busy]" : "");

	return ret;
}

static void tdm_irq_disable_vblank(struct drm_device *drm_dev, int crtc)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	struct tdm_vbl_work *vbl_work = tdm_priv->vbl_work;
	int ret;

	if (crtc >= TDM_CRTC_MAX) {
		DRM_ERROR("crtc[%d]\n", crtc);
		return;
	}

	switch (crtc) {
	case TDM_CRTC_PRIMARY:
		vbl_work->event->data = (void *)0;
		ret = tdm_irq_queue_vblank(tdm_priv, (struct work_struct *)vbl_work);
		break;
	default:
		break;
	}

	DRM_DEBUG("[off_vbl_%d]r[%d]%s\n", crtc,
		atomic_read(&drm_dev->vblank[crtc].refcount), ret ? "[busy]" : "");

	return;
}

static void tdm_handle_vblank(struct drm_device *drm_dev,
			enum tdm_crtc_id crtc)
{
	DRM_DEBUG("%s\n", __func__);

	if (!drm_handle_vblank(drm_dev, crtc))
		DRM_ERROR("handle_vblank:crtc[%d]\n", crtc);
	return;
 }

static irqreturn_t tdm_irq_handler(int irq, void *dev_id)
{
	struct drm_device *drm_dev = (struct drm_device *)dev_id;
	struct drm_vblank_crtc *vblank;
	enum tdm_crtc_id crtc = TDM_CRTC_PRIMARY;
	unsigned long irqflags;

	DRM_DEBUG("%s\n", __func__);

#ifdef CONFIG_ENABLE_DEFAULT_TRACERS
	if (tracing_is_on()) {
		static bool t;

		__trace_printk(0, "C|-50|VSYNC|%d\n", t ? --t : ++t);
	}
#endif

	spin_lock_irqsave(&drm_dev->vblank_time_lock, irqflags);
	vblank = &drm_dev->vblank[crtc];

	if (!vblank->enabled) {
		spin_unlock_irqrestore(&drm_dev->vblank_time_lock, irqflags);
		goto out;
	}

	spin_unlock_irqrestore(&drm_dev->vblank_time_lock, irqflags);
	tdm_handle_vblank(drm_dev, crtc);
out:
	return IRQ_HANDLED;
}

static void tdm_irq_preinstall(struct drm_device *drm_dev)
{
	DRM_DEBUG("%s\n", __func__);
}

static int tdm_irq_postinstall(struct drm_device *drm_dev)
{
	DRM_DEBUG("%s\n", __func__);

	return 0;
}

static void tdm_irq_uninstall(struct drm_device *drm_dev)
{
	DRM_INFO("%s\n", __func__);
}

void tdm_virtual_vblank_handler(struct work_struct *work)
{
	struct tdm_private *tdm_priv = container_of(work,
		struct tdm_private, virtual_vbl_work);
	struct drm_device *dev = tdm_priv->drm_dev;
	enum tdm_crtc_id crtc = TDM_CRTC_VIRTUAL;

	DRM_INFO("[virtual_vbl_%d]\n", crtc);

	usleep_range(tdm_priv->virtual_vbl_us, tdm_priv->virtual_vbl_us + 1000);
	tdm_handle_vblank(dev, crtc);
	schedule_work(&tdm_priv->virtual_vbl_work);
}

int tdm_irq_init(struct drm_device *drm_dev)
{
	struct drm_driver *drm_drv = drm_dev->driver;
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tdm_private *tdm_priv = dev_priv->tdm_priv;
	struct tdm_vbl_work *vbl_work;
	int i, ret;

	ret = drm_vblank_init(drm_dev, tdm_priv->num_crtcs);
	if (ret) {
		DRM_ERROR("failed to init vblank.\n");
		return ret;
	}

	drm_drv->get_vblank_counter = drm_vblank_count;
	drm_drv->get_irq = drm_platform_get_irq;
	drm_drv->prepare_vblank = tdm_irq_prepare_vblank;
	drm_drv->enable_vblank = tdm_irq_enable_vblank;
	drm_drv->disable_vblank = tdm_irq_disable_vblank;
	drm_drv->irq_handler = tdm_irq_handler;
	drm_drv->irq_preinstall = tdm_irq_preinstall;
	drm_drv->irq_postinstall = tdm_irq_postinstall;
	drm_drv->irq_uninstall = tdm_irq_uninstall;
	drm_dev->irq_enabled = true;

	tdm_priv->vbl_wq = create_singlethread_workqueue("tdm_vbl_wq");
	if (!tdm_priv->vbl_wq)
		DRM_ERROR("failed to create workq.\n");

	vbl_work = kzalloc(sizeof(*vbl_work), GFP_KERNEL);
	if (!vbl_work) {
		DRM_ERROR("failed to alloc vbl_work.\n");
		return -ENOMEM;
	}

	vbl_work->event = kzalloc(sizeof(struct tdm_nb_event), GFP_KERNEL);
	if (!vbl_work->event) {
		DRM_ERROR("failed to alloc event.\n");
		kfree(vbl_work);
		return -ENOMEM;
	}

	vbl_work->tdm_priv = tdm_priv;
	tdm_priv->vbl_work = vbl_work;
	INIT_WORK((struct work_struct *)tdm_priv->vbl_work,
			tdm_vblank_work);
	INIT_WORK(&tdm_priv->virtual_vbl_work,
			tdm_virtual_vblank_handler);
	init_completion(&tdm_priv->vbl_comp);
	complete_all(&tdm_priv->vbl_comp);

	for (i = 0; i < TDM_CRTC_MAX; i++)
		atomic_set(&tdm_priv->vbl_permission[i], 0);

	tdm_priv->virtual_vbl_us = (unsigned long int)(VBLANK_INTERVAL(VBLANK_DEF_HZ));
	drm_dev->vblank_disable_allowed = 1;
	drm_dev->max_vblank_count = 0xffffff; /* only 24 bits of frame count */

#ifdef CONFIG_DRM_TDM_IRQ_EXYNOS
	ret = tdm_irq_exynos_init(drm_dev);
#endif

	return ret;
}
