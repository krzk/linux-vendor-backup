/*
 * Copyright (C) 2011 - 2012 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXYNOS5_MDEVICE_H_
#define EXYNOS5_MDEVICE_H_

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <media/media-device.h>
#include <media/media-entity.h>
#include <media/s5p_fimc.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include "fimc-lite.h"
#include "mipi-csis.h"

#define FIMC_MAX_SENSORS	4
#define FIMC_NUM_MIPI_CSIS	2
#define FIMC_NUM_FIMC_LITE	3

enum fimc_subdev_index {
	IDX_SENSOR,
	IDX_CSIS,
	IDX_FLITE,
	IDX_FIMC_IS,
	IDX_MAX,
};

enum fimc_isp_subdev_index {
	IDX_ISP,
	IDX_SCC,
	IDX_SCP,
	IDX_IS_MAX,
};

struct fimc_pipeline {
	struct exynos_media_pipeline ep;
	struct list_head list;
	struct media_entity *vdev_entity;
	struct v4l2_subdev *subdevs[IDX_MAX];
	struct list_head *isp_pipelines;
};

struct fimc_pipeline_isp {
	struct exynos_media_pipeline ep;
	struct list_head list;
	struct v4l2_subdev *subdevs[IDX_IS_MAX];
	bool in_use;
};

struct fimc_csis_info {
	struct v4l2_subdev *sd;
	struct device_node *of_node;
	int id;
};

/**
 * struct fimc_sensor_info - image data source subdev information
 * @pdata: sensor's atrributes passed as media device's platform data
 * @asd: asynchronous subdev registration data structure
 * @subdev: image sensor v4l2 subdev
 * @host: fimc device the sensor is currently linked to
 *
 * This data structure applies to image sensor and the writeback subdevs.
 */
struct fimc_sensor_info {
	struct fimc_source_info pdata;
	struct v4l2_async_subdev asd;
	struct v4l2_subdev *subdev;
	struct fimc_dev *host;
};

/**
 * struct fimc_md - fimc media device information
 * @csis: MIPI CSIS subdevs data
 * @sensor: array of registered sensor subdevs
 * @num_sensors: actual number of registered sensors
 * @clk_bayer: bus clk for external sensors
 * @fimc_lite: array of registered fimc-lite devices
 * @is: fimc-is data structure
 * @media_dev: top level media device
 * @v4l2_dev: top level v4l2_device holding up the subdevs
 * @dev: struct device associated with this media device
 * @slock: spinlock protecting @sensor array
 * @pipelines: list holding pipeline0 (sensor-mipi-flite) instances
 * @isp_pipelines: list holding pipeline1 (isp-scc-scp) instances
 */
struct fimc_md {
	struct fimc_csis_info csis[CSIS_MAX_ENTITIES];
	struct fimc_sensor_info sensor[FIMC_MAX_SENSORS];
	int num_sensors;
	struct clk *clk_bayer;
	struct fimc_lite *fimc_lite[FIMC_LITE_MAX_DEVS];
	struct fimc_is *is;
	struct media_device media_dev;
	struct v4l2_device v4l2_dev;
	struct device *dev;
	struct v4l2_async_notifier subdev_notifier;
	struct v4l2_async_subdev *async_subdevs[FIMC_MAX_SENSORS];

	spinlock_t slock;
	struct list_head pipelines;
	struct list_head isp_pipelines;
};

#define to_fimc_pipeline(_ep) container_of(_ep, struct fimc_pipeline, ep)
#define to_fimc_isp_pipeline(_ep) \
	container_of(_ep, struct fimc_pipeline_isp, ep)

static inline struct fimc_md *entity_to_fimc_mdev(struct media_entity *me)
{
	return me->parent == NULL ? NULL :
		container_of(me->parent, struct fimc_md, media_dev);
}

static inline struct fimc_md *notifier_to_fimc_md(struct v4l2_async_notifier *n)
{
	return container_of(n, struct fimc_md, subdev_notifier);
}

int exynos_camera_register(struct device *dev, struct fimc_md **md);
int exynos_camera_unregister(struct fimc_md *fmd);
#endif
