/*
 * Samsung EXYNOS5/EXYNOS3 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2012-2014 Samsung Electronics Co., Ltd.
 *  Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef FIMC_IS_ISP_H_
#define FIMC_IS_ISP_H_

#include "fimc-is-core.h"
#include "fimc-is-pipeline.h"

#define FIMC_IS_ISP_REQ_BUFS_MIN	2

#define ISP_SD_PAD_SINK_DMA	0
#define ISP_SD_PAD_SINK_OTF	1
#define ISP_SD_PAD_SRC		2
#define ISP_SD_PADS_NUM		3

#define ISP_DEF_WIDTH		1296
#define ISP_DEF_HEIGHT		732
#define ISP_MAX_WIDTH		4208
#define ISP_MAX_HEIGHT		3120
#define ISP_MIN_WIDTH		32
#define ISP_MIN_HEIGHT		32

#define ISP_MAX_BUFS		2

/**
 * struct fimc_is_isp - ISP context
 * @vfd: video device node
 * @fh: v4l2 file handle
 * @alloc_ctx: videobuf2 memory allocator context
 * @subdev: fimc-is-isp subdev
 * @vd_pad: media pad for the output video node
 * @subdev_pads: the subdev media pads
 * @ctrl_handler: v4l2 control handler
 * @video_lock: video lock mutex
 * @sensor_sd: sensor subdev used with this isp instance
 * @pipeline: pipeline instance for this isp context
 * @vbq: vb2 buffers queue for ISP output video node
 * @wait_queue: list holding buffers waiting to be queued to HW
 * @wait_queue_cnt: wait queue number of buffers
 * @run_queue: list holding buffers queued to HW
 * @run_queue_cnt: run queue number of buffers
 * @output_bufs: isp output buffers array
 * @out_buf_cnt: number of output buffers in use
 * @fmt: output plane format for isp
 * @width: user configured input width
 * @height: user configured input height
 * @size_image: image size in bytes
 * @output_state: state of the output video node operations
 */
struct fimc_is_isp {
	struct video_device		vfd;
	struct v4l2_fh			fh;
	struct vb2_alloc_ctx		*alloc_ctx;
	struct v4l2_subdev		subdev;
	struct media_pad		vd_pad;
	struct media_pad		subdev_pads[ISP_SD_PADS_NUM];
	struct v4l2_ctrl_handler	ctrl_handler;
	struct mutex			video_lock;
	struct v4l2_subdev		*sensor_sd;
	struct fimc_is_pipeline		*pipeline;

	struct vb2_queue		vbq;
	struct list_head		wait_queue;
	unsigned int			wait_queue_cnt;
	struct list_head		run_queue;
	unsigned int			run_queue_cnt;

	const struct fimc_is_fmt	*fmt;
	unsigned int			width;
	unsigned int			height;
	unsigned int			size_image;
	unsigned long			output_state;
};

int fimc_is_isp_subdev_create(struct fimc_is_isp *isp,
		struct vb2_alloc_ctx *alloc_ctx,
		struct fimc_is_pipeline *pipeline);
void fimc_is_isp_subdev_destroy(struct fimc_is_isp *isp);

#endif /* FIMC_IS_ISP_H_ */
