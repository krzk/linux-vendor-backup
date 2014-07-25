/*
 * Samsung EXYNOS4x12 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *  Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef FIMC_IS_SCALER_H_
#define FIMC_IS_SCALER_H_

#include <linux/sizes.h>
#include <linux/io.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mediabus.h>
#include <media/s5p_fimc.h>

#include "fimc-is-core.h"

#define SCALER_SD_PAD_SINK	0
#define SCALER_SD_PAD_SRC_FIFO	1
#define SCALER_SD_PAD_SRC_DMA	2
#define SCALER_SD_PADS_NUM	3

#define SCALER_MAX_BUFS		32
#define SCALER_MAX_PLANES	3

#define FIMC_IS_SCALER_REQ_BUFS_MIN	2

#define SCALER_DEF_WIDTH	1280
#define SCALER_DEF_HEIGHT	720
#define SCALER_MAX_WIDTH	4808
#define SCALER_MAX_HEIGHT	3356
#define SCALER_MIN_WIDTH	32
#define SCALER_MIN_HEIGHT	32

#define SCALER_ROTATE			(1 << 2)
#define SCALER_FLIP_X			(1 << 0)
#define SCALER_FLIP_Y			(1 << 1)

struct fimc_is_scaler_ctrl {
	struct v4l2_ctrl_handler handler;
	unsigned int rotation;
	unsigned int color_mode;
	unsigned int status;
};
/**
 * struct fimc_is_scaler - fimc-is scaler structure
 * @vfd: video device node
 * @fh: v4l2 file handle
 * @alloc_ctx: videobuf2 memory allocator context
 * @subdev: fimc-is-scaler subdev
 * @vd_pad: media pad for the output video node
 * @subdev_pads: the subdev media pads
 * @ctrl_handler: v4l2 control handler
 * @video_lock: video lock mutex
 * @lock: internal locking
 * @event_q: notifies scaler events
 * @pipeline: pipeline instance for this scaler context
 * @scaler_id: distinguishes scaler preview or scaler codec
 * @vbq: vb2 buffers queue for ISP output video node
 * @wait_queue: list holding buffers waiting to be queued to HW
 * @wait_queue_cnt: wait queue number of buffers
 * @run_queue: list holding buffers queued to HW
 * @run_queue_cnt: run queue number of buffers
 * @capture_bufs: scaler capture buffers array
 * @fmt: capture plane format for scaler
 * @width: user configured output width
 * @height: user configured output height
 * @capture_state: state of the capture video node operations
 */
struct fimc_is_scaler {
	struct video_device		vfd;
	struct v4l2_fh			fh;
	struct vb2_alloc_ctx		*alloc_ctx;
	struct v4l2_subdev		subdev;
	struct media_pad		vd_pad;
	struct media_pad		subdev_pads[SCALER_SD_PADS_NUM];
	struct v4l2_mbus_framefmt	subdev_fmt;
	struct fimc_is_scaler_ctrl	ctrls;

	struct mutex		video_lock;
	struct mutex		lock;
	wait_queue_head_t	event_q;

	struct fimc_is_pipeline	*pipeline;
	enum fimc_is_scaler_id	scaler_id;

	struct vb2_queue	vbq;
	struct list_head	wait_queue;
	unsigned int		wait_queue_cnt;
	struct list_head	run_queue;
	unsigned int		run_queue_cnt;

	const struct fimc_is_fmt *fmt;
	unsigned int		width;
	unsigned int		height;
	unsigned long		capture_state;
};

int fimc_is_scaler_subdev_create(struct fimc_is_scaler *ctx,
		enum fimc_is_scaler_id scaler_id,
		struct vb2_alloc_ctx *alloc_ctx,
		struct fimc_is_pipeline *pipeline);
void fimc_is_scaler_subdev_destroy(struct fimc_is_scaler *scaler);

#endif /* FIMC_IS_SCALER_H_ */
