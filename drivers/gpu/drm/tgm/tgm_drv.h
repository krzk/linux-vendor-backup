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

#ifndef _TGM_DRV_H_
#define _TGM_DRV_H_

#include <tdm.h>
#include <tbm.h>

enum tgm_drv_dev_type {
	TGM_DRV_DEV_TYPE_NONE,
	TGM_DRV_DEV_TYPE_IRQ,
};

struct tgm_drv_private {
	struct drm_device *drm_dev;
	struct tdm_private *tdm_priv;
	struct tbm_private *tbm_priv;
};

struct tgm_drv_file_private {
	pid_t tgid;
};

int tgm_drv_component_add(struct device *dev,
		enum tgm_drv_dev_type dev_type);
void tgm_drv_component_del(struct device *dev,
		enum tgm_drv_dev_type dev_type);

#endif /* _TGM_DRV_H_ */
