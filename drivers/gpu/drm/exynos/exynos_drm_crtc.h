/* exynos_drm_crtc.h
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

#ifndef _EXYNOS_DRM_CRTC_H_
#define _EXYNOS_DRM_CRTC_H_
#include <linux/input.h>

struct exynos_drm_partial_pos;

int exynos_drm_crtc_create(struct drm_device *dev, unsigned int nr);
int exynos_drm_crtc_prepare_vblank(struct drm_device *dev, int crtc, struct drm_file *file_priv);
int exynos_drm_crtc_enable_vblank(struct drm_device *dev, int crtc);
void exynos_drm_crtc_disable_vblank(struct drm_device *dev, int crtc);
int exynos_drm_crtc_wake_vblank(struct drm_device *dev, int crtc, const char *str);
void exynos_drm_crtc_finish_pageflip(struct drm_device *dev, int crtc);
int exynos_drm_crtc_set_partial_region(struct drm_crtc *crtc,
					struct exynos_drm_partial_pos *pos);
void change_to_full_screen_mode(struct drm_crtc *crtc,
					struct drm_framebuffer *fb);
void request_crtc_partial_update(struct drm_device *dev, int pipe);
int exynos_drm_get_pendingflip(struct drm_device *dev, int crtc);
int exynos_drm_wait_finish_pageflip(struct drm_device *dev, int crtc);
int exynos_drm_crtc_get_dpms(struct drm_crtc *crtc);
void exynos_drm_crtc_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value);
bool exynos_drm_crtc_input_match(struct input_handler *handler,
		struct input_dev *dev);
int exynos_drm_crtc_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id);
void exynos_drm_crtc_input_disconnect(struct input_handle *handle);
int exynos_drm_crtc_lp_mode_enable(struct drm_device *dev, bool enable);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_NOTIFY
int exynos_drm_secure_notify(struct notifier_block *this, unsigned long cmd, void *_data);
#endif
#endif
