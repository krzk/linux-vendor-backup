/*
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"
#include "drm_crtc_helper.h"

#include <drm/exynos_drm.h>
#include "exynos_drm_drv.h"
#include "exynos_drm_crtc.h"
#include "exynos_drm_encoder.h"
#include "exynos_drm_connector.h"

#define MAX_EDID 256
#define to_exynos_connector(x)	container_of(x, struct exynos_drm_connector,\
				drm_connector)

struct exynos_drm_connector_work {
	struct work_struct	work;
	struct drm_exynos_send_dpms_event	*event;
	struct drm_connector	*connector;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	bool early_dpms;
#endif
};

struct exynos_drm_connector {
	struct drm_connector	drm_connector;
	uint32_t		encoder_id;
	struct exynos_drm_manager *manager;
	uint32_t		dpms;
	uint32_t		panel_dpms;
	struct workqueue_struct	*dpms_wq;
	struct exynos_drm_connector_work	*dpms_work;
	struct completion	dpms_comp;
	struct mutex	dpms_lock;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	struct notifier_block	nb_ctrl;
	bool	early_dpms;
#endif
};

struct drm_exynos_send_dpms_event {
	struct drm_pending_event	base;
	struct drm_exynos_dpms_event	event;
};

/* convert exynos_video_timings to drm_display_mode */
static inline void
convert_to_display_mode(struct drm_display_mode *mode,
			struct exynos_drm_panel_info *panel)
{
	struct fb_videomode *timing = &panel->timing;
	DRM_DEBUG_KMS("%s\n", __FILE__);

	mode->clock = timing->pixclock / 1000;
	mode->vrefresh = timing->refresh;

	mode->hdisplay = timing->xres;
	mode->hsync_start = mode->hdisplay + timing->right_margin;
	mode->hsync_end = mode->hsync_start + timing->hsync_len;
	mode->htotal = mode->hsync_end + timing->left_margin;

	mode->vdisplay = timing->yres;
	mode->vsync_start = mode->vdisplay + timing->lower_margin;
	mode->vsync_end = mode->vsync_start + timing->vsync_len;
	mode->vtotal = mode->vsync_end + timing->upper_margin;
	mode->width_mm = panel->width_mm;
	mode->height_mm = panel->height_mm;

	if (timing->vmode & FB_VMODE_INTERLACED)
		mode->flags |= DRM_MODE_FLAG_INTERLACE;

	if (timing->vmode & FB_VMODE_DOUBLE)
		mode->flags |= DRM_MODE_FLAG_DBLSCAN;
}

/* convert drm_display_mode to exynos_video_timings */
static inline void
convert_to_video_timing(struct fb_videomode *timing,
			struct drm_display_mode *mode)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	memset(timing, 0, sizeof(*timing));

	timing->pixclock = mode->clock * 1000;
	timing->refresh = drm_mode_vrefresh(mode);

	timing->xres = mode->hdisplay;
	timing->right_margin = mode->hsync_start - mode->hdisplay;
	timing->hsync_len = mode->hsync_end - mode->hsync_start;
	timing->left_margin = mode->htotal - mode->hsync_end;

	timing->yres = mode->vdisplay;
	timing->lower_margin = mode->vsync_start - mode->vdisplay;
	timing->vsync_len = mode->vsync_end - mode->vsync_start;
	timing->upper_margin = mode->vtotal - mode->vsync_end;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		timing->vmode = FB_VMODE_INTERLACED;
	else
		timing->vmode = FB_VMODE_NONINTERLACED;

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		timing->vmode |= FB_VMODE_DOUBLE;
}

#ifdef CONFIG_SLP_FAKE_WVGA_DRM_MODE_SUPPORT
static void exynos_drm_connector_support_fake_mode(unsigned int fake_hdisplay,
		unsigned int fake_vdisplay, struct drm_display_mode *mode)
{
	if (!mode)
		return;

	/* change LCD physical size */
	mode->width_mm *= (fake_hdisplay * 100 / mode->hdisplay) / 100;
	mode->height_mm *= (fake_vdisplay * 100 / mode->vdisplay) / 100;

	/* change resoultion */
	mode->hdisplay = fake_hdisplay;
	mode->vdisplay = fake_vdisplay;
}
#endif

static int exynos_drm_connector_get_modes(struct drm_connector *connector)
{
	struct exynos_drm_connector *exynos_connector =
					to_exynos_connector(connector);
	struct exynos_drm_manager *manager = exynos_connector->manager;
	struct exynos_drm_display_ops *display_ops = manager->display_ops;
	unsigned int count;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (!display_ops) {
		DRM_DEBUG_KMS("display_ops is null.\n");
		return 0;
	}

	/*
	 * if get_edid() exists then get_edid() callback of hdmi side
	 * is called to get edid data through i2c interface else
	 * get timing from the FIMD driver(display controller).
	 *
	 * P.S. in case of lcd panel, count is always 1 if success
	 * because lcd panel has only one mode.
	 */
	if (display_ops->get_edid) {
		int ret;
		void *edid;

		edid = kzalloc(MAX_EDID, GFP_KERNEL);
		if (!edid) {
			DRM_ERROR("failed to allocate edid\n");
			return 0;
		}

		ret = display_ops->get_edid(manager->dev, connector,
						edid, MAX_EDID);
		if (ret < 0) {
			DRM_ERROR("failed to get edid data.\n");
			kfree(edid);
			edid = NULL;
			return 0;
		}

		drm_mode_connector_update_edid_property(connector, edid);
		count = drm_add_edid_modes(connector, edid);
		kfree(edid);
	} else {
		int i;
		struct exynos_drm_panel_info *panel;

		if (display_ops->get_panel)
			panel = display_ops->get_panel(manager->dev);
		else
			return 0;

		for (i = 0; i < panel->mode_count; i++) {
			struct drm_display_mode *mode =
				drm_mode_create(connector->dev);

			if (!mode) {
				DRM_ERROR("failed to create mode.\n");
				return 0;
			}

			panel->timing.refresh = panel->mode[i].refresh;
			panel->timing.vmode = panel->mode[i].vmode;

			convert_to_display_mode(mode, panel);
			connector->display_info.width_mm = mode->width_mm;
			connector->display_info.height_mm = mode->height_mm;

#ifdef CONFIG_SLP_FAKE_WVGA_DRM_MODE_SUPPORT
			if (connector->connector_type == DRM_MODE_CONNECTOR_LVDS)
				exynos_drm_connector_support_fake_mode(480, 800, mode);
#endif

			mode->type = DRM_MODE_TYPE_DRIVER |
				DRM_MODE_TYPE_PREFERRED;
			drm_mode_set_name(mode);
			drm_mode_probed_add(connector, mode);
		}

		count = panel->mode_count;
	}

	return count;
}

static int exynos_drm_connector_mode_valid(struct drm_connector *connector,
					    struct drm_display_mode *mode)
{
	struct exynos_drm_connector *exynos_connector =
					to_exynos_connector(connector);
	struct exynos_drm_manager *manager = exynos_connector->manager;
	struct exynos_drm_display_ops *display_ops = manager->display_ops;
	struct fb_videomode timing;
	int ret = MODE_BAD;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	convert_to_video_timing(&timing, mode);

	if (display_ops && display_ops->check_timing)
		if (!display_ops->check_timing(manager->dev, (void *)&timing))
			ret = MODE_OK;

	return ret;
}

struct drm_encoder *exynos_drm_best_encoder(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct exynos_drm_connector *exynos_connector =
					to_exynos_connector(connector);
	struct drm_mode_object *obj;
	struct drm_encoder *encoder;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	obj = drm_mode_object_find(dev, exynos_connector->encoder_id,
				   DRM_MODE_OBJECT_ENCODER);
	if (!obj) {
		DRM_DEBUG_KMS("Unknown ENCODER ID %d\n",
				exynos_connector->encoder_id);
		return NULL;
	}

	encoder = obj_to_encoder(obj);

	return encoder;
}

static struct drm_connector_helper_funcs exynos_connector_helper_funcs = {
	.get_modes	= exynos_drm_connector_get_modes,
	.mode_valid	= exynos_drm_connector_mode_valid,
	.best_encoder	= exynos_drm_best_encoder,
};

static bool exynos_drm_check_dpms(int cur, int next)
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

void exynos_drm_display_power(struct drm_connector *connector, int mode)
{
	struct drm_encoder *encoder = exynos_drm_best_encoder(connector);
	struct exynos_drm_connector *exynos_connector;
	struct exynos_drm_manager *manager = exynos_drm_get_manager(encoder);
	struct exynos_drm_display_ops *display_ops = manager->display_ops;

	exynos_connector = to_exynos_connector(connector);

	if (exynos_connector->dpms == mode) {
		DRM_DEBUG_KMS("desired dpms mode is same as previous one.\n");
		return;
	}

	if (display_ops && display_ops->power_on)
		display_ops->power_on(manager->dev, mode);

	exynos_connector->dpms = mode;
}

static void exynos_drm_connector_dpms(struct drm_connector *connector,
					int mode)
{
	struct exynos_drm_connector *exynos_connector =
					to_exynos_connector(connector);

	if (exynos_connector->dpms != exynos_connector->panel_dpms)
		DRM_INFO("%s:different dpms[%d %d]\n", __func__,
			exynos_connector->dpms, exynos_connector->panel_dpms);

	if (exynos_connector->dpms == DRM_MODE_DPMS_SUSPEND &&
		mode == DRM_MODE_DPMS_OFF) {
		DRM_INFO("%s[%d]standby:set\n", "connector_dpms",
			exynos_connector->dpms);
		drm_helper_connector_dpms(connector, DRM_MODE_DPMS_STANDBY);
		exynos_connector->panel_dpms = mode;
		DRM_INFO("%s[%d]standby:done\n", "connector_dpms",
			exynos_connector->dpms);
	}

	if (!exynos_drm_check_dpms(exynos_connector->dpms, mode))
		return;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
	if (mode == DRM_MODE_DPMS_OFF)
		trustedui_set_mode(TRUSTEDUI_MODE_OFF);
#endif

	/*
	 * in case that drm_crtc_helper_set_mode() is called,
	 * encoder/crtc->funcs->dpms() will be just returned
	 * because they already were DRM_MODE_DPMS_ON so only
	 * exynos_drm_display_power() will be called.
	 */
	drm_helper_connector_dpms(connector, mode);

	exynos_connector->panel_dpms = mode;
	exynos_drm_display_power(connector, mode);

	if (mode == DRM_MODE_DPMS_ON)
		exynos_drm_crtc_wake_vblank(connector->dev, 0, "dpms");
}

static int exynos_drm_connector_fill_modes(struct drm_connector *connector,
				unsigned int max_width, unsigned int max_height)
{
	struct exynos_drm_connector *exynos_connector =
					to_exynos_connector(connector);
	struct exynos_drm_manager *manager = exynos_connector->manager;
	struct exynos_drm_manager_ops *ops = manager->ops;
	unsigned int width, height;

	width = max_width;
	height = max_height;

	/*
	 * if specific driver want to find desired_mode using maxmum
	 * resolution then get max width and height from that driver.
	 */
	if (ops && ops->get_max_resol)
		ops->get_max_resol(manager->dev, &width, &height);

	return drm_helper_probe_single_connector_modes(connector, width,
							height);
}

/* get detection status of display device. */
static enum drm_connector_status
exynos_drm_connector_detect(struct drm_connector *connector, bool force)
{
	struct exynos_drm_connector *exynos_connector =
					to_exynos_connector(connector);
	struct exynos_drm_manager *manager = exynos_connector->manager;
	struct exynos_drm_display_ops *display_ops =
					manager->display_ops;
	enum drm_connector_status status = connector_status_disconnected;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (display_ops && display_ops->is_connected) {
		if (display_ops->is_connected(manager->dev))
			status = connector_status_connected;
		else
			status = connector_status_disconnected;
	}

	return status;
}

static void exynos_drm_connector_destroy(struct drm_connector *connector)
{
	struct exynos_drm_connector *exynos_connector =
		to_exynos_connector(connector);
	struct exynos_drm_connector_work *dpms_work =
		exynos_connector->dpms_work;

	DRM_DEBUG_KMS("%s\n", __FILE__);
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	display_early_dpms_nb_unregister(&exynos_connector->nb_ctrl);
#endif
	kfree(dpms_work);
	destroy_workqueue(exynos_connector->dpms_wq);
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(exynos_connector);
}

static int exynos_drm_connector_set_property(struct drm_connector *connector,
					struct drm_property *property,
					uint64_t val)
{
	struct drm_device *dev = connector->dev;
	struct exynos_drm_private *dev_priv = dev->dev_private;
	struct exynos_drm_connector *exynos_connector =
		to_exynos_connector(connector);

	DRM_DEBUG_KMS("%s\n", __func__);

	if (dev_priv->connector_panel_dpms_property == property) {
		uint32_t panel_dpms = val;

		if (exynos_connector->panel_dpms == panel_dpms)
			return 0;

		switch (panel_dpms) {
		case DRM_MODE_DPMS_ON ... DRM_MODE_DPMS_OFF:
			exynos_connector->panel_dpms = panel_dpms;
			exynos_drm_display_power(connector, panel_dpms);
			break;
		default:
			return -EINVAL;
		}

		return 0;
	}

	return -EINVAL;
}

static struct drm_connector_funcs exynos_connector_funcs = {
	.dpms		= exynos_drm_connector_dpms,
	.fill_modes	= exynos_drm_connector_fill_modes,
	.detect		= exynos_drm_connector_detect,
	.destroy	= exynos_drm_connector_destroy,
	.set_property	= exynos_drm_connector_set_property,
};

static const struct drm_prop_enum_list panel_dpms_names[] = {
	{ DRM_MODE_DPMS_ON, "On" },
	{ DRM_MODE_DPMS_STANDBY, "Standby" },
	{ DRM_MODE_DPMS_SUSPEND, "Suspend" },
	{ DRM_MODE_DPMS_OFF, "Off" },
};

static void exynos_drm_connector_attach_property
		(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct exynos_drm_private *dev_priv = dev->dev_private;
	struct drm_property *prop;

	DRM_DEBUG_KMS("%s\n", __func__);

	prop = dev_priv->connector_panel_dpms_property;
	if (!prop) {
		prop = drm_property_create_enum(dev, 0, "panel",
				panel_dpms_names, ARRAY_SIZE(panel_dpms_names));
		if (!prop) {
			DRM_ERROR("failed to create panel property.\n");
			return;
		}

		dev_priv->connector_panel_dpms_property = prop;
	}

	drm_object_attach_property(&connector->base, prop, 0);
}

struct drm_connector *exynos_drm_connector_create(struct drm_device *dev,
						   struct drm_encoder *encoder)
{
	struct exynos_drm_connector *exynos_connector;
	struct exynos_drm_manager *manager = exynos_drm_get_manager(encoder);
	struct drm_connector *connector;
	struct workqueue_struct	*dpms_wq;
	struct exynos_drm_connector_work *dpms_work;
	int type;
	int err;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	exynos_connector = kzalloc(sizeof(*exynos_connector), GFP_KERNEL);
	if (!exynos_connector) {
		DRM_ERROR("failed to allocate connector\n");
		return NULL;
	}

	connector = &exynos_connector->drm_connector;

	switch (manager->display_ops->type) {
	case EXYNOS_DISPLAY_TYPE_HDMI:
		type = DRM_MODE_CONNECTOR_HDMIA;
		connector->interlace_allowed = true;
		connector->polled = DRM_CONNECTOR_POLL_HPD;
		break;
	case EXYNOS_DISPLAY_TYPE_LCD:
		type = DRM_MODE_CONNECTOR_LVDS;
		connector->interlace_allowed = true;
		break;
	case EXYNOS_DISPLAY_TYPE_VIDI:
		type = DRM_MODE_CONNECTOR_VIRTUAL;
		connector->polled = DRM_CONNECTOR_POLL_HPD;
		break;
	default:
		type = DRM_MODE_CONNECTOR_Unknown;
		break;
	}

	drm_connector_init(dev, connector, &exynos_connector_funcs, type);
	drm_connector_helper_add(connector, &exynos_connector_helper_funcs);

	err = drm_sysfs_connector_add(connector);
	if (err)
		goto err_connector;

	exynos_connector->encoder_id = encoder->base.id;
	exynos_connector->manager = manager;
	exynos_connector->dpms = DRM_MODE_DPMS_OFF;
	connector->dpms = DRM_MODE_DPMS_OFF;
	connector->encoder = encoder;

	err = drm_mode_connector_attach_encoder(connector, encoder);
	if (err) {
		DRM_ERROR("failed to attach a connector to a encoder\n");
		goto err_sysfs;
	}

	exynos_drm_connector_attach_property(connector);

	dpms_wq = create_singlethread_workqueue("dpms");
	if (!dpms_wq) {
		DRM_ERROR("failed to create workq.\n");
		goto err_attach;
	}
	exynos_connector->dpms_wq = dpms_wq;

	dpms_work = kzalloc(sizeof(*dpms_work), GFP_KERNEL);
	if (!dpms_work) {
		DRM_ERROR("failed to alloc dpms_work.\n");
		goto err_wq;
	}

	dpms_work->connector= connector;
	exynos_connector->dpms_work = dpms_work;
	INIT_WORK((struct work_struct *)exynos_connector->dpms_work,
		exynos_drm_dpms_work);

	init_completion(&exynos_connector->dpms_comp);

	mutex_init(&exynos_connector->dpms_lock);

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	exynos_connector->nb_ctrl.notifier_call = display_early_dpms_notifier_ctrl;
	if (display_early_dpms_nb_register(&exynos_connector->nb_ctrl))
		DRM_ERROR("failed to register for early dpms\n");
#endif

	DRM_DEBUG_KMS("connector has been created\n");

	return connector;

err_wq:
	destroy_workqueue(dpms_wq);
err_attach:
	drm_mode_connector_detach_encoder(connector, encoder);
err_sysfs:
	drm_sysfs_connector_remove(connector);
err_connector:
	drm_connector_cleanup(connector);
	kfree(exynos_connector);
	return NULL;
}

int exynos_drm_connector_set_partial_region(struct drm_connector *connector,
					struct exynos_drm_partial_pos *pos)
{
	struct exynos_drm_connector *exynos_connector =
					to_exynos_connector(connector);
	struct exynos_drm_manager *manager = exynos_connector->manager;
	struct exynos_drm_display_ops *display_ops =
					manager->display_ops;

	return display_ops->set_partial_region(manager->dev, pos);
}

void exynos_drm_free_dpms_event(struct drm_pending_event *event)
{
	DRM_INFO("%s:base[0x%x]\n", "free_dpms_evt", (int)event);

	kfree(event);
}

void *exynos_drm_get_dpms_event(struct exynos_drm_connector *exynos_connector,
		struct drm_file *file_priv,
#ifdef CONFIG_DISPLAY_EARLY_DPMS
		bool early_dpms,
#endif
		struct drm_exynos_dpms *req)
{
	struct drm_connector *connector = &exynos_connector->drm_connector;
	struct drm_device *dev = connector->dev;
	struct drm_exynos_send_dpms_event *e = NULL;
	unsigned long flags;

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e) {
		DRM_ERROR("failed to allocate event.\n");
#ifdef CONFIG_DISPLAY_EARLY_DPMS
		if (early_dpms) {
			goto out;
		}
#endif
		spin_lock_irqsave(&dev->event_lock, flags);
		file_priv->event_space += sizeof(e->event);
		spin_unlock_irqrestore(&dev->event_lock, flags);
		goto out;
	}

	e->event.base.type = DRM_EXYNOS_DPMS_EVENT;
	e->event.base.length = sizeof(e->event);
	e->event.connector_id = req->connector_id;
	e->event.dpms = req->dpms;
	e->event.user_data = req->user_data;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	if (early_dpms)
		goto bypass;
#endif

	e->base.event = &e->event.base;
	e->base.file_priv = file_priv;
	e->base.destroy =  exynos_drm_free_dpms_event;

bypass:
	DRM_INFO("%s:base[0x%x]dpms[%d]data[0x%x]\n",
		"get_dpms_evt", (int)&e->base, e->event.dpms,
		(int)e->event.user_data);

out:
	return e;
}

void exynos_drm_put_dpms_event(struct exynos_drm_connector *exynos_connector,
		struct drm_exynos_send_dpms_event *e)
{
	struct drm_connector *connector = &exynos_connector->drm_connector;
	struct drm_device *dev = connector->dev;
	unsigned long flags;

	DRM_INFO("%s:base[0x%x]dpms[%d]data[0x%x]\n",
		"put_dpms_evt", (int)&e->base, e->event.dpms,
		(int)e->event.user_data);

	spin_lock_irqsave(&dev->event_lock, flags);
	list_add_tail(&e->base.link, &e->base.file_priv->event_list);
	wake_up_interruptible(&e->base.file_priv->event_wait);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	return;
}

void exynos_drm_dpms_work(struct work_struct *work)
{
	struct exynos_drm_connector_work *dpms_work =
		(struct exynos_drm_connector_work *)work;
	struct drm_exynos_send_dpms_event *e = dpms_work->event;
	struct drm_connector *connector = dpms_work->connector;
	struct exynos_drm_connector *exynos_connector =
		to_exynos_connector(connector);
	long nice;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	bool early_dpms = dpms_work->early_dpms;
#endif

	mutex_lock(&exynos_connector->dpms_lock);

	DRM_INFO("%s:base[0x%x]con_id[%d]dpms[%d->%d]data[0x%x]\n",
		"dpms_work", (int)&e->base, e->event.connector_id,
		exynos_connector->dpms, e->event.dpms, (int)e->event.user_data);

	nice = task_nice(current);
	set_user_nice(current, -20);
#ifdef CONFIG_ENABLE_DEFAULT_TRACERS
	if (tracing_is_on())
		__trace_printk(0, "C|-50|DPMS_WORK|%d\n", 1);
#endif

	exynos_drm_connector_dpms(connector, e->event.dpms);

#ifdef CONFIG_DISPLAY_EARLY_DPMS
	if (early_dpms)
		exynos_drm_free_dpms_event(&e->base);
	else
#endif
	exynos_drm_put_dpms_event(exynos_connector, e);

	complete_all(&exynos_connector->dpms_comp);

#ifdef CONFIG_ENABLE_DEFAULT_TRACERS
	if (tracing_is_on())
		__trace_printk(0, "C|-50|DPMS_WORK|%d\n", 0);
#endif
	set_user_nice(current, nice);

	DRM_INFO("%s:base[0x%x]dpms[%d]done\n", "dpms_work",
		(int)&e->base, exynos_connector->dpms);

	mutex_unlock(&exynos_connector->dpms_lock);

	return;
}

int exynos_drm_handle_dpms(struct exynos_drm_connector *exynos_connector,
			struct drm_exynos_dpms *req,
#ifdef CONFIG_DISPLAY_EARLY_DPMS
			bool early_dpms,
#endif
			struct drm_file *file)
{
	struct drm_connector *connector = &exynos_connector->drm_connector;
	int ret = 0;

	if (req->type == DPMS_EVENT_DRIVEN) {
		struct exynos_drm_connector_work *dpms_work;
		struct drm_exynos_send_dpms_event *e;

#ifdef CONFIG_DISPLAY_EARLY_DPMS
		e = exynos_drm_get_dpms_event(exynos_connector, file, early_dpms, req);
#else
		e = exynos_drm_get_dpms_event(exynos_connector, file, req);
#endif
		if (!e) {
			ret = -ENOMEM;
			goto out;
		}

		if (completion_done(&exynos_connector->dpms_comp))
			INIT_COMPLETION(exynos_connector->dpms_comp);

		dpms_work = exynos_connector->dpms_work;
		dpms_work->event = e;
#ifdef CONFIG_DISPLAY_EARLY_DPMS
		dpms_work->early_dpms = early_dpms;

		DRM_INFO("%s[%d]%s\n", "handle_dpms", req->dpms,
			dpms_work->early_dpms ? "[early]" : "");
#endif

		if (!queue_work(exynos_connector->dpms_wq,
				(struct work_struct *)dpms_work))
			DRM_INFO("%s:busy to queue_work.\n", __func__);
	} else
		exynos_drm_connector_dpms(connector, req->dpms);

out:
	return ret;
}

int exynos_drm_control_dpms(struct drm_device *dev,
						void *data,
						struct drm_file *file)
{
	struct drm_exynos_dpms *req = data;
	struct drm_mode_object *obj;
	struct drm_connector *connector;
	struct exynos_drm_connector *exynos_connector;
	int ret = 0;

	DRM_INFO("EXYNOS_DPMS[%d][%s]\n", req->dpms,
		req->type ? "ASYNC" : "SYNC");

	obj = drm_mode_object_find(dev, req->connector_id,
				   DRM_MODE_OBJECT_CONNECTOR);
	if (!obj) {
		DRM_ERROR("unknown connector_id[%d]\n", req->connector_id);
		ret = -EINVAL;
		goto out;
	}

	connector = obj_to_connector(obj);
	if (!connector) {
		DRM_ERROR("failed to get drm_connector\n");
		ret = -EINVAL;
		goto out;
	}

	exynos_connector = to_exynos_connector(connector);
#ifdef CONFIG_DISPLAY_EARLY_DPMS
	ret = exynos_drm_handle_dpms(exynos_connector, req, false, file);
#else
	ret = exynos_drm_handle_dpms(exynos_connector, req, file);
#endif

out:
	DRM_INFO("EXYNOS_DPMS[%d][%s]ret[%d]\n", req->dpms,
		req->type ? "ASYNC" : "SYNC", ret);

	return ret;
}

#ifdef CONFIG_DISPLAY_EARLY_DPMS
static ATOMIC_NOTIFIER_HEAD(display_early_dpms_nb_list);

int display_early_dpms_nb_register(struct notifier_block *nb)
{
	int ret;

	ret = atomic_notifier_chain_register(
		&display_early_dpms_nb_list, nb);

	DRM_INFO("%s:ret[%d]\n", __func__, ret);

	return ret;
}

int display_early_dpms_nb_unregister(struct notifier_block *nb)
{
	int ret;

	ret = atomic_notifier_chain_unregister(
		&display_early_dpms_nb_list, nb);

	DRM_INFO("%s:ret[%d]\n", __func__, ret);

	return ret;
}

int display_early_dpms_nb_send_event(unsigned long val, void *v)
{
	int ret;

	ret = atomic_notifier_call_chain(
		&display_early_dpms_nb_list, val, v);

	DRM_DEBUG("%s:ret[%d]\n", __func__, ret);

	return ret;
}

int display_early_dpms_notifier_ctrl(struct notifier_block *this,
			unsigned long cmd, void *_data)
{
	struct exynos_drm_connector *exynos_connector = container_of(this,
		struct exynos_drm_connector, nb_ctrl);
	struct drm_connector *connector = &exynos_connector->drm_connector;
	struct display_early_dpms_nb_event *event =
		(struct display_early_dpms_nb_event *)_data;
	enum display_early_dpms_id crtc = event->id;
	int dpms = exynos_connector->dpms, ret = 0;

	switch (cmd) {
	case DISPLAY_EARLY_DPMS_MODE_SET: {
		bool early_dpms = (bool)event->data;

		DRM_INFO("%s:set:crtc[%d]mode[%d]dpms[%d]\n", "early_dpms",
			crtc, early_dpms, dpms);

		if (!exynos_connector->early_dpms && dpms == DRM_MODE_DPMS_OFF)
			exynos_connector->early_dpms = early_dpms;
	}
		break;
	case DISPLAY_EARLY_DPMS_COMMIT: {
		bool on = (bool)event->data;

		DRM_INFO("%s:commit:crtc[%d]on[%d]mode[%d]dpms[%d]\n", "early_dpms",
			crtc, on, exynos_connector->early_dpms, dpms);

		if ((on && exynos_connector->early_dpms && dpms == DRM_MODE_DPMS_OFF &&
			pm_try_early_complete()) || (!on && (dpms == DRM_MODE_DPMS_ON ||
			dpms == DRM_MODE_DPMS_STANDBY))) {
			struct drm_exynos_dpms req;

			req.connector_id = connector->base.id;
			req.dpms = on ? DRM_MODE_DPMS_STANDBY : DRM_MODE_DPMS_OFF;
			req.type = on ? DPMS_EVENT_DRIVEN : DPMS_SYNC_WORK;

			ret = exynos_drm_handle_dpms(exynos_connector, &req, true, NULL);
		}
		exynos_connector->early_dpms = false;
	}
		break;
	default:
		DRM_ERROR("unsupported cmd[0x%x]\n", (int)cmd);
		ret = -EINVAL;
		break;
	}

	if (ret)
		DRM_ERROR("cmd[%d]ret[%d]\n", (int)cmd, ret);
	return ret;
}
#endif

