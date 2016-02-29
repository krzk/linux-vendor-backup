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

#ifndef _EXYNOS_DRM_FB_H_
#define _EXYNOS_DRM_FB_H

#define to_exynos_fb(x)	container_of(x, struct exynos_drm_fb, fb)

#define IS_PART_REGION_CHANGED(old, new)	(old->x != new->x || \
							old->y != new->y || \
							old->w != new->w || \
							old->h != new->h)

/*
 * exynos specific framebuffer structure.
 *
 * @fb: drm framebuffer obejct.
 * @buf_cnt: a buffer count to drm framebuffer.
 * @exynos_gem_obj: array of exynos specific gem object containing a gem object.
 */
struct exynos_drm_fb {
	struct drm_framebuffer		fb;
	unsigned int			buf_cnt;
	struct exynos_drm_gem_obj	*exynos_gem_obj[MAX_FB_BUFFER];
	atomic_t			partial;
	struct exynos_drm_partial_pos	part_pos;
};

struct drm_framebuffer *
exynos_drm_framebuffer_init(struct drm_device *dev,
			    struct drm_mode_fb_cmd2 *mode_cmd,
			    struct drm_gem_object *obj);

/* get memory information of a drm framebuffer */
struct exynos_drm_gem_buf *exynos_drm_fb_buffer(struct drm_framebuffer *fb,
						 int index);

void exynos_drm_mode_config_init(struct drm_device *dev);

/* set a buffer count to drm framebuffer. */
void exynos_drm_fb_set_buf_cnt(struct drm_framebuffer *fb,
						unsigned int cnt);

/* get a buffer count to drm framebuffer. */
unsigned int exynos_drm_fb_get_buf_cnt(struct drm_framebuffer *fb);

#ifdef CONFIG_DMABUF_SYNC
void *exynos_drm_dmabuf_sync_work(struct drm_framebuffer *fb);
#else
static inline void *exynos_drm_dmabuf_sync_work(struct drm_framebuffer *fb)
{
	return ERR_PTR(0);
}
#endif

bool check_fb_partial_update(struct drm_framebuffer *fb);

struct exynos_drm_partial_pos *get_partial_pos(struct drm_framebuffer *fb);

void update_partial_region(struct drm_crtc *crtc,
					struct exynos_drm_partial_pos *pos);

void exynos_drm_fb_request_partial_update(struct drm_crtc *crtc,
						struct drm_framebuffer *fb);

#endif
