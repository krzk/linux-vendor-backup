/* exynos_drm_fb.c
 *
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
#include "drm_crtc.h"
#include "drm_crtc_helper.h"
#include "drm_fb_helper.h"

#include "exynos_drm.h"
#include "exynos_drm_drv.h"
#include "exynos_drm_fb.h"
#include "exynos_drm_gem.h"
#include "exynos_drm_iommu.h"
#include "exynos_drm_encoder.h"
#include "exynos_drm_crtc.h"
#include "exynos_drm_connector.h"

#include <linux/dmabuf-sync.h>

static int check_fb_gem_memory_type(struct drm_device *drm_dev,
				struct exynos_drm_gem_obj *exynos_gem_obj)
{
	unsigned int flags;

	/*
	 * if exynos drm driver supports iommu then framebuffer can use
	 * all the buffer types.
	 */
	if (is_drm_iommu_supported(drm_dev))
		return 0;

	flags = exynos_gem_obj->flags;

	/*
	 * without iommu support, not support physically non-continuous memory
	 * for framebuffer.
	 */
	if (IS_NONCONTIG_BUFFER(flags)) {
		DRM_ERROR("cannot use this gem memory type for fb.\n");
		return -EINVAL;
	}

	return 0;
}

static void exynos_drm_fb_destroy(struct drm_framebuffer *fb)
{
	struct exynos_drm_fb *exynos_fb = to_exynos_fb(fb);
	unsigned int i;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* make sure that overlay data are updated before relesing fb. */
	exynos_drm_encoder_complete_scanout(fb);

	drm_framebuffer_cleanup(fb);

	for (i = 0; i < ARRAY_SIZE(exynos_fb->exynos_gem_obj); i++) {
		struct drm_gem_object *obj;

		if (exynos_fb->exynos_gem_obj[i] == NULL)
			continue;

		obj = &exynos_fb->exynos_gem_obj[i]->base;
		drm_gem_object_unreference_unlocked(obj);
	}

	kfree(exynos_fb);
	exynos_fb = NULL;
}

static int exynos_drm_fb_create_handle(struct drm_framebuffer *fb,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	struct exynos_drm_fb *exynos_fb = to_exynos_fb(fb);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* This fb should have only one gem handle. */
	if (WARN_ON(exynos_fb->buf_cnt != 1))
		return -EINVAL;

	return drm_gem_handle_create(file_priv,
			&exynos_fb->exynos_gem_obj[0]->base, handle);
}

static int exynos_drm_fb_create_handle2(struct drm_framebuffer *fb,
						struct drm_file *file_priv,
						unsigned int *handles,
						int num_handles)
{
	struct exynos_drm_fb *exynos_fb = to_exynos_fb(fb);
	struct drm_gem_object *obj;
	int ret = 0, i;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/*
	 * consider specific case
	 * - DRM_FORMAT_NV12
	 *   : drm_format_num_planes() returns 2
	 *   : exynos_drm_format_num_buffers() will return 1
	 *     if the memory region to each handle is contiguous each other
	 *     and return 2 if not so
	 */
	if (num_handles != exynos_fb->buf_cnt) {
		unsigned int handle;

		obj = &exynos_fb->exynos_gem_obj[0]->base;
		ret = drm_gem_handle_create(file_priv, obj, &handle);
		if (ret) {
			DRM_ERROR("failed to create handle\n");
			return ret;
		}

		for (i = 0; i < num_handles; i++)
			handles[i] = handle;

		return ret;
	}

	for (i = 0; i < exynos_fb->buf_cnt; i++) {
		obj = &exynos_fb->exynos_gem_obj[i]->base;
		ret = drm_gem_handle_create(file_priv, obj, &handles[i]);
		if (ret) {
			DRM_ERROR("failed to create handle\n");
			goto err;
		}
	}

	return ret;

err:
	for (i = 0; i < exynos_fb->buf_cnt; i++)
		if (handles[i])
			drm_gem_handle_delete(file_priv, handles[i]);
	return ret;
}

void update_partial_region(struct drm_crtc *crtc,
					struct exynos_drm_partial_pos *pos)
{
	struct drm_device *dev = crtc->dev;
	struct list_head *connector_list = &dev->mode_config.connector_list;
	struct drm_connector *connector;

	list_for_each_entry(connector, connector_list, head) {
		if (!connector->encoder)
			return;
		if (crtc == connector->encoder->crtc) {
			/* Update partial region to connector device. */
			exynos_drm_connector_set_partial_region(connector, pos);

			/* Update partial region to crtc device. */
			exynos_drm_crtc_set_partial_region(crtc, pos);
		}
	}
}

void exynos_drm_fb_request_partial_update(struct drm_crtc *crtc,
						struct drm_framebuffer *fb)
{
	struct exynos_drm_fb *exynos_fb = to_exynos_fb(fb);

	update_partial_region(crtc, &exynos_fb->part_pos);

	atomic_set(&exynos_fb->partial, 0);
}

bool check_fb_partial_update(struct drm_framebuffer *fb)
{
	struct exynos_drm_fb *exynos_fb;

	exynos_fb = to_exynos_fb(fb);

	return atomic_read(&exynos_fb->partial);
}

struct exynos_drm_partial_pos *get_partial_pos(struct drm_framebuffer *fb)
{
	struct exynos_drm_fb *exynos_fb = to_exynos_fb(fb);

	return &exynos_fb->part_pos;
}

static void exynos_drm_fb_adjust_clips(struct exynos_drm_partial_pos *pos,
		struct drm_clip_rect *clips, unsigned int num_clips)
{
	unsigned int index;
	unsigned int min_sx = 0;
	unsigned int min_sy = 0;
	unsigned int max_bx = 0;
	unsigned int max_by = 0;

	for (index = 0; index < num_clips; index++) {
		min_sx = min_t(unsigned int, clips[index].x1, min_sx);
		if (min_sx == 0 && index == 0)
			min_sx = clips[index].x1;

		min_sy = min_t(unsigned int, clips[index].y1, min_sy);
		if (min_sy == 0 && index == 0)
			min_sy = clips[index].y1;

		max_bx = max_t(unsigned int, clips[index].x2, max_bx);
		max_by = max_t(unsigned int, clips[index].y2, max_by);

		DRM_DEBUG_KMS("[%d] x1(%d) y1(%d) x2(%d) y2(%d)\n",
				index, clips[index].x1, clips[index].y1,
				clips[index].x2, clips[index].y2);
	}

	pos->x = min_sx;
	pos->y = min_sy;
	pos->w = max_bx - min_sx;
	pos->h = max_by - min_sy;

	DRM_DEBUG_KMS("partial region: x1(%d) y1(%d) x2(%d) y2(%d)\n",
			pos->x, pos->y, pos->w, pos->h);
}

static int exynos_drm_fb_dirty(struct drm_framebuffer *fb,
				struct drm_file *file_priv, unsigned flags,
				unsigned color, struct drm_clip_rect *clips,
				unsigned num_clips)
{
	struct exynos_drm_fb *exynos_fb = to_exynos_fb(fb);

	if (!num_clips || !clips)
		return -EINVAL;

	/*
	 * Get maximum region to all clip regions
	 * if one more clip regions exsit.
	 */
	if (num_clips > 1) {
		DRM_DEBUG_KMS("use only one clip region.\n");

		exynos_drm_fb_adjust_clips(&exynos_fb->part_pos, clips,
						num_clips);
		goto out;
	}

	exynos_fb->part_pos.x = clips[0].x1;
	exynos_fb->part_pos.y = clips[0].y1;
	exynos_fb->part_pos.w = clips[0].x2 - clips[0].x1;
	exynos_fb->part_pos.h = clips[0].y2 - clips[0].y1;

out:
	DRM_DEBUG_KMS("%s: x1 = %d, y1 = %d, w = %d, h = %d\n",
			__func__, exynos_fb->part_pos.x, exynos_fb->part_pos.y,
			exynos_fb->part_pos.w, exynos_fb->part_pos.h);

	atomic_set(&exynos_fb->partial, 1);

	return 0;
}

static struct drm_framebuffer_funcs exynos_drm_fb_funcs = {
	.destroy	= exynos_drm_fb_destroy,
	.create_handle	= exynos_drm_fb_create_handle,
	.create_handle2	= exynos_drm_fb_create_handle2,
	.dirty		= exynos_drm_fb_dirty,
};

void exynos_drm_fb_set_buf_cnt(struct drm_framebuffer *fb,
						unsigned int cnt)
{
	struct exynos_drm_fb *exynos_fb;

	exynos_fb = to_exynos_fb(fb);

	exynos_fb->buf_cnt = cnt;
}

unsigned int exynos_drm_fb_get_buf_cnt(struct drm_framebuffer *fb)
{
	struct exynos_drm_fb *exynos_fb;

	exynos_fb = to_exynos_fb(fb);

	return exynos_fb->buf_cnt;
}

struct drm_framebuffer *
exynos_drm_framebuffer_init(struct drm_device *dev,
			    struct drm_mode_fb_cmd2 *mode_cmd,
			    struct drm_gem_object *obj)
{
	struct exynos_drm_fb *exynos_fb;
	struct exynos_drm_gem_obj *exynos_gem_obj;
	int ret;

	exynos_gem_obj = to_exynos_gem_obj(obj);

	ret = check_fb_gem_memory_type(dev, exynos_gem_obj);
	if (ret < 0) {
		DRM_ERROR("cannot use this gem memory type for fb.\n");
		return ERR_PTR(-EINVAL);
	}

	exynos_fb = kzalloc(sizeof(*exynos_fb), GFP_KERNEL);
	if (!exynos_fb) {
		DRM_ERROR("failed to allocate exynos drm framebuffer\n");
		return ERR_PTR(-ENOMEM);
	}

	drm_helper_mode_fill_fb_struct(&exynos_fb->fb, mode_cmd);
	exynos_fb->exynos_gem_obj[0] = to_exynos_gem_obj(obj);

	ret = drm_framebuffer_init(dev, &exynos_fb->fb, &exynos_drm_fb_funcs);
	if (ret) {
		DRM_ERROR("failed to initialize framebuffer\n");
		return ERR_PTR(ret);
	}

	return &exynos_fb->fb;
}

#ifdef CONFIG_DMABUF_SYNC
void *exynos_drm_dmabuf_sync_work(struct drm_framebuffer *fb)
{
	struct exynos_drm_fb *exynos_fb;
	struct drm_gem_object *obj;
	struct dmabuf_sync *sync;
	unsigned int i;
	int ret = 0;

	sync = dmabuf_sync_init("DRM", NULL, NULL);
	if (IS_ERR(sync)) {
		WARN_ON(1);
		goto out_dmabuf_sync;
	}

	exynos_fb = to_exynos_fb(fb);

	for (i = 0; i < exynos_fb->buf_cnt; i++) {
		if (!exynos_fb->exynos_gem_obj[i]) {
			WARN_ON(1);
			continue;
		}

		obj = &exynos_fb->exynos_gem_obj[i]->base;
		if (!obj->export_dma_buf)
			continue;

		/*
		 * set dmabuf to fence and registers reservation
		 * object to reservation entry.
		 */
		ret = dmabuf_sync_get(sync,
				obj->export_dma_buf,
				DMA_BUF_ACCESS_DMA_R);
		if (WARN_ON(ret < 0))
			continue;

	}

	ret = dmabuf_sync_lock(sync);
	if (ret < 0) {
		dmabuf_sync_put_all(sync);
		dmabuf_sync_fini(sync);
		sync = ERR_PTR(ret);
		goto out_dmabuf_sync;
	}

	/* buffer synchronization is done by wait_for_vblank so just signal. */
	dmabuf_sync_unlock(sync);

	dmabuf_sync_put_all(sync);
	dmabuf_sync_fini(sync);

out_dmabuf_sync:
	return sync;
}
#endif

static u32 exynos_drm_format_num_buffers(struct drm_mode_fb_cmd2 *mode_cmd)
{
	unsigned int cnt = 0, handle = 0;

	/*
	 * Normal FBs have just one buffer object
	 * while some FBs have more than one buffers
	 * because of multi space formats like NV12M, NV12MT.
	 *
	 * NV12
	 * handles[0] = base1, offsets[0] = 0
	 * handles[1] = base1, offsets[1] = Y_size
	 *
	 * NV12M, NV12MT
	 * handles[0] = base1, offsets[0] = 0
	 * handles[1] = base2, offsets[1] = 0
	 *
	 * there is no case like handles[0] and handles[2] are same
	 * but handles[1] is different
	 *
	 * So number of spaces can be found from different handles
	 */
	while (cnt < MAX_FB_BUFFER) {
		if (!mode_cmd->handles[cnt] || handle == mode_cmd->handles[cnt])
			break;

		handle = mode_cmd->handles[cnt];
		cnt++;
	}

	return cnt;
}

static struct drm_framebuffer *
exynos_user_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		      struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct drm_gem_object *obj;
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct exynos_drm_fb *exynos_fb;
	int i, ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	exynos_fb = kzalloc(sizeof(*exynos_fb), GFP_KERNEL);
	if (!exynos_fb) {
		DRM_ERROR("failed to allocate exynos drm framebuffer\n");
		return ERR_PTR(-ENOMEM);
	}

	obj = drm_gem_object_lookup(dev, file_priv, mode_cmd->handles[0]);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object\n");
		ret = -ENOENT;
		goto err_free;
	}

	drm_helper_mode_fill_fb_struct(&exynos_fb->fb, mode_cmd);
	exynos_fb->exynos_gem_obj[0] = to_exynos_gem_obj(obj);
	exynos_fb->buf_cnt = exynos_drm_format_num_buffers(mode_cmd);

	DRM_DEBUG_KMS("buf_cnt = %d\n", exynos_fb->buf_cnt);

	for (i = 1; i < exynos_fb->buf_cnt; i++) {
		obj = drm_gem_object_lookup(dev, file_priv,
				mode_cmd->handles[i]);
		if (!obj) {
			DRM_ERROR("failed to lookup gem object\n");
			ret = -ENOENT;
			goto err_unreference;
		}

		exynos_gem_obj = to_exynos_gem_obj(obj);
		exynos_fb->exynos_gem_obj[i] = exynos_gem_obj;

		ret = check_fb_gem_memory_type(dev, exynos_gem_obj);
		if (ret < 0) {
			DRM_ERROR("cannot use this gem memory type for fb.\n");
			goto err_unreference;
		}
	}

	ret = drm_framebuffer_init(dev, &exynos_fb->fb, &exynos_drm_fb_funcs);
	if (ret) {
		DRM_ERROR("failed to init framebuffer.\n");
		goto err_unreference;
	}

	return &exynos_fb->fb;

err_unreference:
	for (i = 0; i < exynos_fb->buf_cnt; i++) {
		exynos_gem_obj = exynos_fb->exynos_gem_obj[i];
		if (exynos_gem_obj)
			drm_gem_object_unreference_unlocked(
							&exynos_gem_obj->base);
	}
err_free:
	kfree(exynos_fb);
	return ERR_PTR(ret);
}

struct exynos_drm_gem_buf *exynos_drm_fb_buffer(struct drm_framebuffer *fb,
						int index)
{
	struct exynos_drm_fb *exynos_fb = to_exynos_fb(fb);
	struct exynos_drm_gem_buf *buffer;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (index >= MAX_FB_BUFFER)
		return NULL;

	buffer = exynos_fb->exynos_gem_obj[index]->buffer;
	if (!buffer)
		return NULL;

	DRM_DEBUG_KMS("dma_addr = 0x%lx\n", (unsigned long)buffer->dma_addr);

	return buffer;
}

static void exynos_drm_output_poll_changed(struct drm_device *dev)
{
	struct exynos_drm_private *private = dev->dev_private;
	struct drm_fb_helper *fb_helper = private->fb_helper;

	if (fb_helper)
		drm_fb_helper_hotplug_event(fb_helper);
}

static struct drm_mode_config_funcs exynos_drm_mode_config_funcs = {
	.fb_create = exynos_user_fb_create,
	.output_poll_changed = exynos_drm_output_poll_changed,
};

void exynos_drm_mode_config_init(struct drm_device *dev)
{
	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	dev->mode_config.max_width = 4096;
	dev->mode_config.max_height = 4096;

	dev->mode_config.funcs = &exynos_drm_mode_config_funcs;
}
