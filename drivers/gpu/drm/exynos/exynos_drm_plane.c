/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <drm/drmP.h>

#include <drm/exynos_drm.h>
#include <drm/drm_plane_helper.h>
#include "exynos_drm_drv.h"
#include "exynos_drm_crtc.h"
#include "exynos_drm_fb.h"
#include "exynos_drm_gem.h"
#include "exynos_drm_plane.h"

/*
 * This function is to get X or Y size shown via screen. This needs length and
 * start position of CRTC.
 *
 *      <--- length --->
 * CRTC ----------------
 *      ^ start        ^ end
 *
 * There are six cases from a to f.
 *
 *             <----- SCREEN ----->
 *             0                 last
 *   ----------|------------------|----------
 * CRTCs
 * a -------
 *        b -------
 *        c --------------------------
 *                 d --------
 *                           e -------
 *                                  f -------
 */
static int exynos_plane_get_size(int start, unsigned length, unsigned last)
{
	int end = start + length;
	int size = 0;

	if (start <= 0) {
		if (end > 0)
			size = min_t(unsigned, end, last);
	} else if (start <= last) {
		size = min_t(unsigned, last - start, length);
	}

	return size;
}

int exynos_check_plane(struct drm_plane *plane, struct drm_framebuffer *fb)
{
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);
	int nr;
	int i;

	nr = exynos_drm_fb_get_buf_cnt(fb);
	for (i = 0; i < nr; i++) {
		struct exynos_drm_gem_buf *buffer = exynos_drm_fb_buffer(fb, i);

		if (!buffer) {
			DRM_DEBUG_KMS("buffer is null\n");
			return -EFAULT;
		}

		exynos_plane->dma_addr[i] = buffer->dma_addr + fb->offsets[i];

		DRM_DEBUG_KMS("buffer: %d, dma_addr = 0x%lx\n",
				i, (unsigned long)exynos_plane->dma_addr[i]);
	}

	return 0;
}

void exynos_plane_mode_set(struct drm_plane *plane, struct drm_crtc *crtc,
			  struct drm_framebuffer *fb, int crtc_x, int crtc_y,
			  unsigned int crtc_w, unsigned int crtc_h,
			  uint32_t src_x, uint32_t src_y,
			  uint32_t src_w, uint32_t src_h)
{
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);
	unsigned int actual_w;
	unsigned int actual_h;

	actual_w = exynos_plane_get_size(crtc_x, crtc_w, crtc->mode.hdisplay);
	actual_h = exynos_plane_get_size(crtc_y, crtc_h, crtc->mode.vdisplay);

	if (crtc_x < 0) {
		if (actual_w)
			src_x -= crtc_x;
		crtc_x = 0;
	}

	if (crtc_y < 0) {
		if (actual_h)
			src_y -= crtc_y;
		crtc_y = 0;
	}

	/* set ratio */
	exynos_plane->h_ratio = (src_w << 16) / crtc_w;
	exynos_plane->v_ratio = (src_h << 16) / crtc_h;

	/* set drm framebuffer data. */
	exynos_plane->src_x = src_x;
	exynos_plane->src_y = src_y;
	exynos_plane->src_width = (actual_w * exynos_plane->h_ratio) >> 16;
	exynos_plane->src_height = (actual_h * exynos_plane->v_ratio) >> 16;
	exynos_plane->fb_width = fb->width;
	exynos_plane->fb_height = fb->height;
	exynos_plane->bpp = fb->bits_per_pixel;
	exynos_plane->pitch = fb->pitches[0];
	exynos_plane->pixel_format = fb->pixel_format;

	/* set plane range to be displayed. */
	exynos_plane->crtc_x = crtc_x;
	exynos_plane->crtc_y = crtc_y;
	exynos_plane->crtc_width = actual_w;
	exynos_plane->crtc_height = actual_h;

	/* set drm mode data. */
	exynos_plane->mode_width = crtc->mode.hdisplay;
	exynos_plane->mode_height = crtc->mode.vdisplay;
	exynos_plane->refresh = crtc->mode.vrefresh;
	exynos_plane->scan_flag = crtc->mode.flags;

	DRM_DEBUG_KMS("plane : offset_x/y(%d,%d), width/height(%d,%d)",
			exynos_plane->crtc_x, exynos_plane->crtc_y,
			exynos_plane->crtc_width, exynos_plane->crtc_height);

	plane->crtc = crtc;
}

#ifdef CONFIG_DRM_DMA_SYNC
static void exynos_plane_update_cb(struct drm_reservation_cb *rcb, void *params)
{
	struct exynos_drm_plane *exynos_plane = params;
	struct exynos_drm_crtc *exynos_crtc =
					to_exynos_crtc(exynos_plane->base.crtc);

	if (exynos_crtc->ops->win_commit)
		exynos_crtc->ops->win_commit(exynos_crtc,
					     exynos_plane->zpos);

	exynos_plane->update_pending = false;

	if (exynos_plane->pending_fence) {
		drm_fence_signal_and_put(&exynos_plane->fence);
		exynos_plane->fence = exynos_plane->pending_fence;
		exynos_plane->pending_fence = NULL;
	}

	/* TODO */
}
#endif

#ifdef CONFIG_DRM_DMA_SYNC
static int exynos_plane_fence(struct exynos_drm_plane *plane,
			      struct exynos_drm_gem_obj *obj)
{
	struct drm_crtc *crtc = plane->base.crtc;
	struct exynos_drm_crtc *exynos_crtc;
	struct reservation_object *resv;
	struct fence *fence;
	int ret;

	exynos_crtc = to_exynos_crtc(crtc);
	resv = obj->base.dma_buf->resv;

	ww_mutex_lock(&resv->lock, NULL);

	ret = reservation_object_reserve_shared(resv);
	if (ret < 0) {
		DRM_ERROR("Reserving space for shared fence failed: %d\n", ret);
		goto err_mutex;
	}

	fence = drm_sw_fence_new(plane->fence_context,
				 atomic_add_return(1, &plane->fence_seqno));
	if (IS_ERR(fence)) {
		ret = PTR_ERR(fence);
		DRM_ERROR("Failed to create fence: %d\n", ret);
		goto err_mutex;
	}

	plane->update_pending = true;
	plane->pending_fence = fence;

	reservation_object_add_shared_fence(resv, plane->pending_fence);

	if (!reservation_object_test_signaled_rcu(resv, false)) {
		drm_reservation_cb_init(&plane->rcb, exynos_plane_update_cb, plane);
		ret = drm_reservation_cb_add(&plane->rcb, resv, false);
		if (ret < 0) {
			DRM_ERROR("Adding reservation to callback failed: %d\n", ret);
			goto err_fence;
		}

		drm_reservation_cb_done(&plane->rcb);
	} else {
		exynos_plane_update_cb(&plane->rcb, plane);
	}

	ww_mutex_unlock(&resv->lock);

	return 0;

err_fence:
	fence_put(plane->pending_fence);
	plane->pending_fence = NULL;
	plane->update_pending = false;
err_mutex:
	ww_mutex_unlock(&resv->lock);

	return ret;
}
#endif

int
exynos_update_plane(struct drm_plane *plane, struct drm_crtc *crtc,
		     struct drm_framebuffer *fb, int crtc_x, int crtc_y,
		     unsigned int crtc_w, unsigned int crtc_h,
		     uint32_t src_x, uint32_t src_y,
		     uint32_t src_w, uint32_t src_h)
{

	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(crtc);
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);
	struct exynos_drm_gem_obj *obj;
	int ret;

	if (exynos_plane->update_pending)
		return -EBUSY;

	ret = exynos_check_plane(plane, fb);
	if (ret < 0)
		return ret;

	exynos_plane_mode_set(plane, crtc, fb, crtc_x, crtc_y,
			      crtc_w, crtc_h, src_x >> 16, src_y >> 16,
			      src_w >> 16, src_h >> 16);

	obj = exynos_drm_fb_gem_obj(fb, 0);
	if (IS_ERR(obj)) {
		/* there should be no a such case. */
		WARN_ON(1);
		return PTR_ERR(obj);
	}

#ifdef CONFIG_DRM_DMA_SYNC
	if (!obj->base.dma_buf || !obj->base.dma_buf->resv) {
		if (exynos_crtc->ops->win_commit)
			exynos_crtc->ops->win_commit(exynos_crtc,
						     exynos_plane->zpos);
		return 0;
	}

	return exynos_plane_fence(exynos_plane, obj);
#else
	if (exynos_crtc->ops->win_commit)
		exynos_crtc->ops->win_commit(exynos_crtc, exynos_plane->zpos);

	return 0;
#endif
}

static int exynos_disable_plane(struct drm_plane *plane)
{
	struct exynos_drm_plane *exynos_plane = to_exynos_plane(plane);
	struct exynos_drm_crtc *exynos_crtc = to_exynos_crtc(plane->crtc);

	exynos_plane->update_pending = false;

	if (exynos_crtc && exynos_crtc->ops->win_disable)
		exynos_crtc->ops->win_disable(exynos_crtc,
					      exynos_plane->zpos);

	return 0;
}

static struct drm_plane_funcs exynos_plane_funcs = {
	.update_plane	= exynos_update_plane,
	.disable_plane	= exynos_disable_plane,
	.destroy	= drm_plane_cleanup,
};

static void exynos_plane_attach_zpos_property(struct drm_plane *plane,
					      unsigned int zpos)
{
	struct drm_device *dev = plane->dev;
	struct exynos_drm_private *dev_priv = dev->dev_private;
	struct drm_property *prop;

	prop = dev_priv->plane_zpos_property;
	if (!prop) {
		prop = drm_property_create_range(dev, DRM_MODE_PROP_IMMUTABLE,
						 "zpos", 0, MAX_PLANE - 1);
		if (!prop)
			return;

		dev_priv->plane_zpos_property = prop;
	}

	drm_object_attach_property(&plane->base, prop, zpos);
}

int exynos_plane_init(struct drm_device *dev,
		      struct exynos_drm_plane *exynos_plane,
		      unsigned long possible_crtcs, enum drm_plane_type type,
		      const uint32_t *formats, unsigned int fcount,
		      unsigned int zpos)
{
	int err;

	err = drm_universal_plane_init(dev, &exynos_plane->base, possible_crtcs,
				       &exynos_plane_funcs, formats, fcount,
				       type);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		return err;
	}

	exynos_plane->zpos = zpos;

	exynos_plane_attach_zpos_property(&exynos_plane->base, zpos);

#ifdef CONFIG_DRM_DMA_SYNC
	exynos_plane->fence_context = fence_context_alloc(1);
	atomic_set(&exynos_plane->fence_seqno, 0);
#endif

	return 0;
}
