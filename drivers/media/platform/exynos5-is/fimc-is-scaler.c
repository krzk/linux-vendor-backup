/*
 * Samsung EXYNOS5250 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *  Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "fimc-is.h"
#include "fimc-is-metadata.h"

#define IS_SCALER_DRV_NAME "fimc-is-scaler"

#define FLIP_X_AXIS			0
#define FLIP_Y_AXIS			1


#define SCALER_ROTATE_90_CW		90
#define SCALER_ROTATE_180_CW		180
#define SCALER_ROTATE_270_CW		270

#define SCALER_CTRL_NUM			4

static const struct fimc_is_fmt formats[] = {
	{
		.name           = "YUV 4:2:0 3p MultiPlanar",
		.fourcc         = V4L2_PIX_FMT_YUV420M,
		.depth		= {8, 2, 2},
		.num_planes     = 3,
	},
	{
		.name           = "YUV 4:2:0 2p MultiPlanar",
		.fourcc         = V4L2_PIX_FMT_NV12M,
		.depth		= {8, 4},
		.num_planes     = 2,
	},
	{
		.name           = "YUV 4:2:2 1p MultiPlanar",
		.fourcc         = V4L2_PIX_FMT_NV16,
		.depth		= {16},
		.num_planes     = 1,
	},
};
#define NUM_FORMATS ARRAY_SIZE(formats)

static const struct fimc_is_fmt *find_format(struct v4l2_format *f)
{
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (formats[i].fourcc == f->fmt.pix_mp.pixelformat)
			return &formats[i];
	}
	return NULL;
}

static int scaler_video_capture_start_streaming(struct vb2_queue *vq,
					unsigned int count)
{
	struct fimc_is_scaler *ctx = vb2_get_drv_priv(vq);
	int ret;

	ret = fimc_is_pipeline_scaler_start(ctx->pipeline,
			ctx->scaler_id,
			vq->num_buffers,
			ctx->fmt->num_planes);
	if (ret) {
		v4l2_err(&ctx->subdev, "Scaler start failed.\n");
		return -EINVAL;
	}

	ret = v4l2_ctrl_handler_setup(&ctx->ctrls.handler);
	if (ret) {
		v4l2_err(&ctx->subdev, "Failed to setup scaler controls.\n");
		return -EINVAL;
	}

	set_bit(STATE_RUNNING, &ctx->capture_state);
	return 0;
}

static void scaler_video_capture_stop_streaming(struct vb2_queue *vq)
{
	struct fimc_is_scaler *ctx = vb2_get_drv_priv(vq);
	struct fimc_is_buf *buf;
	int ret;

        if (!test_bit(STATE_RUNNING, &ctx->capture_state))
                return;

        ret = fimc_is_pipeline_scaler_stop(ctx->pipeline, ctx->scaler_id);
        if (ret)
                v4l2_info(&ctx->subdev, "Scaler already stopped.\n");

	/* Release un-used buffers */
	fimc_is_pipeline_buf_lock(ctx->pipeline);
	while (!list_empty(&ctx->wait_queue)) {
		buf = fimc_is_scaler_wait_queue_get(ctx);
		buf->state = FIMC_IS_BUF_INVALID;
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}
	while (!list_empty(&ctx->run_queue)) {
		buf = fimc_is_scaler_run_queue_get(ctx);
		buf->state = FIMC_IS_BUF_INVALID;
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}
        fimc_is_pipeline_buf_unlock(ctx->pipeline);

	clear_bit(STATE_RUNNING, &ctx->capture_state);
}

static int scaler_video_capture_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *pfmt,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], void *allocators[])
{
	struct fimc_is_scaler *ctx = vb2_get_drv_priv(vq);
	const struct fimc_is_fmt *fmt = ctx->fmt;
	unsigned int wh;
	int i;

	if (!fmt)
		return -EINVAL;

	*num_planes = fmt->num_planes;
	wh = ctx->width * ctx->height;

	for (i = 0; i < *num_planes; i++) {
		allocators[i] = ctx->alloc_ctx;
		sizes[i] = (wh * fmt->depth[i]) / 8;
	}
	return 0;
}

static int scaler_video_capture_buffer_init(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct fimc_is_scaler *ctx = vb2_get_drv_priv(vq);
	struct fimc_is_buf *buf = container_of(vb, struct fimc_is_buf, vb);
	const struct fimc_is_fmt *fmt;
	int i;

	fmt = ctx->fmt;
	for (i = 0; i < fmt->num_planes; i++)
		buf->paddr[i] = vb2_dma_contig_plane_dma_addr(vb, i);
	buf->state = FIMC_IS_BUF_IDLE;
	return 0;
}

static int scaler_video_capture_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct fimc_is_scaler *ctx = vb2_get_drv_priv(vq);
	int i;

	for (i = 0; i < ctx->fmt->num_planes; i++) {
		unsigned long size = (ctx->width * ctx->height *
					ctx->fmt->depth[i]) / 8;

		if (vb2_plane_size(vb, i) < size) {
			v4l2_err(&ctx->subdev,
				 "User buffer too small (%ld < %ld)\n",
				 vb2_plane_size(vb, i), size);
			return -EINVAL;
		}
		vb2_set_plane_payload(vb, i, size);
	}

	return 0;
}

static void scaler_video_capture_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct fimc_is_scaler *ctx = vb2_get_drv_priv(vq);
	struct fimc_is_buf *buf = container_of(vb, struct fimc_is_buf, vb);

	/* Add buffer to the wait queue */
	fimc_is_pipeline_buf_lock(ctx->pipeline);
	fimc_is_scaler_wait_queue_add(ctx, buf);
	fimc_is_pipeline_buf_unlock(ctx->pipeline);
}

static void scaler_video_capture_buffer_cleanup(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct fimc_is_scaler *ctx = vb2_get_drv_priv(vq);
	struct fimc_is_buf *fbuf = container_of(vb, struct fimc_is_buf, vb);


	/*
	 * Skip the cleanup case the buffer
	 * is no longer accessible through any of the queues
	 * or has not been queued at all.
	 */
	if (fbuf->state == FIMC_IS_BUF_IDLE ||
	    fbuf->state == FIMC_IS_BUF_INVALID ||
	    fbuf->state == FIMC_IS_BUF_DONE)
		return;

	fimc_is_pipeline_buf_lock(ctx->pipeline);
	list_del(&fbuf->list);
	if (fbuf->state == FIMC_IS_BUF_QUEUED)
		--ctx->wait_queue_cnt;
	else /* FIMC_IS_BUF_ACTIVE */
		--ctx->run_queue_cnt;
	fbuf->state = FIMC_IS_BUF_INVALID;
	fimc_is_pipeline_buf_unlock(ctx->pipeline);
}

static const struct vb2_ops scaler_video_capture_qops = {
	.queue_setup		= scaler_video_capture_queue_setup,
	.buf_init		= scaler_video_capture_buffer_init,
	.buf_prepare		= scaler_video_capture_buffer_prepare,
	.buf_queue		= scaler_video_capture_buffer_queue,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
	.buf_cleanup		= scaler_video_capture_buffer_cleanup,
	.start_streaming	= scaler_video_capture_start_streaming,
	.stop_streaming		= scaler_video_capture_stop_streaming,
};

static const struct v4l2_file_operations scaler_video_capture_fops = {
	.owner		= THIS_MODULE,
	.open		= v4l2_fh_open,
	.release	= vb2_fop_release,
	.poll		= vb2_fop_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= vb2_fop_mmap,
};

/*
 * Video node ioctl operations
 */
static int scaler_querycap_capture(struct file *file, void *priv,
					struct v4l2_capability *cap)
{
	struct fimc_is_scaler *ctx = video_drvdata(file);
	char *name = (ctx->scaler_id == SCALER_SCC) ?
				"fimc-is-scc" : "fimc-is-scp";

	strncpy(cap->driver, name, sizeof(cap->driver) - 1);
	strncpy(cap->card, name, sizeof(cap->card) - 1);
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
			name);
	cap->device_caps = V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int scaler_enum_fmt_mplane(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	const struct fimc_is_fmt *fmt;

	if (f->index >= NUM_FORMATS)
		return -EINVAL;

	fmt = &formats[f->index];
	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static int scaler_g_fmt_mplane(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct fimc_is_scaler *ctx = video_drvdata(file);
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	const struct fimc_is_fmt *fmt = ctx->fmt;
	int i;

	for (i = 0; i < fmt->num_planes; i++) {
		struct v4l2_plane_pix_format *plane_fmt = &pixm->plane_fmt[i];
		plane_fmt->bytesperline = (ctx->width * fmt->depth[i]) / 8;
		plane_fmt->sizeimage = plane_fmt->bytesperline * ctx->height;
		memset(plane_fmt->reserved, 0, sizeof(plane_fmt->reserved));
	}

	pixm->num_planes = fmt->num_planes;
	pixm->pixelformat = fmt->fourcc;
	pixm->width = ctx->width;
	pixm->height = ctx->height;
	pixm->field = V4L2_FIELD_NONE;
	pixm->colorspace = V4L2_COLORSPACE_JPEG;
	memset(pixm->reserved, 0, sizeof(pixm->reserved));

	return 0;
}

static int scaler_try_fmt_mplane(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	const struct fimc_is_fmt *fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	u32 i;

	fmt = find_format(f);
	if (!fmt)
		fmt = &formats[0];

	v4l_bound_align_image(&pixm->width, SCALER_MIN_WIDTH,
			SCALER_MAX_WIDTH, 0,
			&pixm->height, SCALER_MIN_HEIGHT,
			SCALER_MAX_HEIGHT, 0, 0);

	for (i = 0; i < fmt->num_planes; i++) {
		struct v4l2_plane_pix_format *plane_fmt = &pixm->plane_fmt[i];

		plane_fmt->bytesperline = (pixm->width * fmt->depth[i]) / 8;
		plane_fmt->sizeimage = (pixm->width * pixm->height *
					fmt->depth[i]) / 8;
		memset(plane_fmt->reserved, 0, sizeof(plane_fmt->reserved));
	}
	pixm->num_planes = fmt->num_planes;
	pixm->pixelformat = fmt->fourcc;
	pixm->colorspace = V4L2_COLORSPACE_JPEG;
	pixm->field = V4L2_FIELD_NONE;
	memset(pixm->reserved, 0, sizeof(pixm->reserved));

	return 0;
}

static int scaler_s_fmt_mplane(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct fimc_is_scaler *ctx = video_drvdata(file);
	const struct fimc_is_fmt *fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int ret;

	if (test_bit(STATE_RUNNING, &ctx->capture_state))
		return -EBUSY;

	ret = scaler_try_fmt_mplane(file, priv, f);
	if (ret)
		return ret;

	/* Get format type */
	fmt = find_format(f);
	if (!fmt) {
		fmt = &formats[0];
		pixm->pixelformat = fmt->fourcc;
		pixm->num_planes = fmt->num_planes;
	}

	/* Save values to context */
	ctx->fmt = fmt;
	ctx->width = pixm->width;
	ctx->height = pixm->height;
	set_bit(STATE_INIT, &ctx->capture_state);
	return 0;
}

static int scaler_reqbufs(struct file *file, void *priv,
		struct v4l2_requestbuffers *reqbufs)
{
	struct fimc_is_scaler *ctx = video_drvdata(file);
	int ret;

	if (test_bit(STATE_RUNNING, &ctx->capture_state))
		return -EBUSY;

	if (test_bit(STATE_BUFS_ALLOCATED, &ctx->capture_state) &&
	    !reqbufs->count) {
		/*
		 * Release previously allocated buffers
		 * vb2 will make sure the buffers can be freed safely.
		 */
		ret = vb2_reqbufs(&ctx->vbq, reqbufs);
		if (!ret)
			clear_bit(STATE_BUFS_ALLOCATED, &ctx->capture_state);
		return ret;
	}
	reqbufs->count = max_t(u32, FIMC_IS_SCALER_REQ_BUFS_MIN,
				reqbufs->count);
	ret = vb2_ioctl_reqbufs(file, priv, reqbufs);
	if (ret) {
		v4l2_err(&ctx->subdev, "vb2 req buffers failed\n");
		return ret;
	}

	if (reqbufs->count < FIMC_IS_SCALER_REQ_BUFS_MIN) {
		reqbufs->count = 0;
                vb2_reqbufs(&ctx->vbq, reqbufs);
		return -ENOMEM;
	}
	set_bit(STATE_BUFS_ALLOCATED, &ctx->capture_state);
	return 0;
}

static const struct v4l2_ioctl_ops scaler_video_capture_ioctl_ops = {
	.vidioc_querycap		= scaler_querycap_capture,
	.vidioc_enum_fmt_vid_cap_mplane	= scaler_enum_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane	= scaler_try_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= scaler_s_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= scaler_g_fmt_mplane,
	.vidioc_reqbufs			= scaler_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
};

static int scaler_video_set_flip(struct fimc_is_scaler *ctx,
				 unsigned int val,
				 unsigned int axis)
{
	if (ctx->scaler_id != SCALER_SCP)
		return -EINVAL;

	if (axis == FLIP_X_AXIS) {
		if (val)
			ctx->ctrls.rotation |= SCALER_FLIP_X;
		else
			ctx->ctrls.rotation &= ~SCALER_FLIP_X;
	} else {
		if (val)
			ctx->ctrls.rotation |= SCALER_FLIP_Y;
		else
			ctx->ctrls.rotation &= ~SCALER_FLIP_Y;
	}

	return fimc_is_pipeline_set_scaler_effect(ctx->pipeline,
					ctx->ctrls.rotation);
	return 0;
}

static int scler_video_set_rotate(struct fimc_is_scaler *ctx,
				  unsigned int val)
{
	int ret = 0;

	if (ctx->scaler_id != SCALER_SCP)
		return -EINVAL;

	if (!val) {
		ctx->ctrls.rotation &= ~SCALER_ROTATE;
	} else {
		switch (val) {
		case SCALER_ROTATE_90_CW:
			ctx->ctrls.rotation |= SCALER_ROTATE;
			break;

		case SCALER_ROTATE_180_CW:
			ctx->ctrls.rotation |= SCALER_FLIP_X | SCALER_FLIP_Y;
			break;

		case SCALER_ROTATE_270_CW:
			ctx->ctrls.rotation = SCALER_ROTATE |
					     SCALER_FLIP_X | SCALER_FLIP_Y;
			break;
		}
	}
	ret = fimc_is_pipeline_set_scaler_effect(ctx->pipeline,
						ctx->ctrls.rotation);
	return ret;
}

static const unsigned int color_mode_map[] = {
	[V4L2_COLORFX_NONE] = V4L2_COLORFX_NONE,
	[V4L2_COLORFX_BW] = COLOR_CORRECTION_MODE_EFFECT_MONO,
	[V4L2_COLORFX_SEPIA] = COLOR_CORRECTION_MODE_EFFECT_SEPIA,
	[V4L2_COLORFX_NEGATIVE] = COLOR_CORRECTION_MODE_EFFECT_NEGATIVE,
	[V4L2_COLORFX_EMBOSS] = COLORCORRECTION_MODE_EFFECT_EMBOSS,
	[V4L2_COLORFX_SKETCH] = COLORCORRECTION_MODE_EFFECT_SKETCH,
	[V4L2_COLORFX_AQUA] = COLOR_CORRECTION_MODE_EFFECT_AQUA,
	[V4L2_COLORFX_ART_FREEZE] = COLOR_CORRECTION_MODE_EFFECT_POSTERIZE,
	[V4L2_COLORFX_SOLARIZATION] = COLOR_CORRECTION_MODE_EFFECT_SOLARIZE,
	[V4L2_COLORFX_ANTIQUE] = COLORCORRECTION_MODE_EFFECT_WARM_VINTAGE,
};

/* Unsupported color modes  */
#define SCALER_COLOR_MODE_SKIP_MASK	(1 << (V4L2_COLORFX_GRASS_GREEN) | \
		1 << (V4L2_COLORFX_SKIN_WHITEN)	| \
		1 << (V4L2_COLORFX_SKY_BLUE)	| \
		1 << (V4L2_COLORFX_SILHOUETTE)	| \
		1 << (V4L2_COLORFX_VIVID)	| \
		1 << (V4L2_COLORFX_SET_CBCR))

static int scaler_video_set_color_mode(struct fimc_is_scaler *ctx,
				       unsigned int val)
{
	unsigned int mode;
	int ret = 0;

	if (val >= ARRAY_SIZE(color_mode_map) ||
	    ctx->scaler_id != SCALER_SCC)
		return -EINVAL;

	mode = color_mode_map[val];

	if (!mode && val != V4L2_COLORFX_NONE)
		return -EINVAL;

	if (!ret)
		ret = fimc_is_pipeline_update_shot_ctrl(ctx->pipeline,
			FIMC_IS_CID_COLOR_MODE, mode);
	return ret;
}

static int scaler_video_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct fimc_is_scaler *ctx = container_of(ctrl->handler,
					struct fimc_is_scaler,
					ctrls.handler);
	int ret = 0;

	mutex_lock(&ctx->lock);
	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
		scaler_video_set_flip(ctx, ctrl->val, FLIP_X_AXIS);
		break;
	case V4L2_CID_VFLIP:
		scaler_video_set_flip(ctx, ctrl->val, FLIP_Y_AXIS);
		break;
	case V4L2_CID_COLORFX:
		scaler_video_set_color_mode(ctx, ctrl->val);
		break;
	case V4L2_CID_ROTATE:
		scler_video_set_rotate(ctx, ctrl->val);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&ctx->lock);
	return ret;
}

static const struct v4l2_ctrl_ops scaler_video_ctrl_ops = {
	.s_ctrl = scaler_video_s_ctrl,
};

static int scaler_video_capture_create_controls(struct fimc_is_scaler *ctx)
{
	struct v4l2_ctrl_handler   *handler = &ctx->ctrls.handler;
	const struct v4l2_ctrl_ops *ops     = &scaler_video_ctrl_ops;

	/* Flip */
	if (ctx->scaler_id == SCALER_SCP) {
		v4l2_ctrl_new_std(handler, ops,
				 V4L2_CID_HFLIP, 0, 1, 1, 0);

		v4l2_ctrl_new_std(handler, ops,
				 V4L2_CID_VFLIP, 0, 1, 1, 0);
		/* Rotate */
		v4l2_ctrl_new_std(handler, ops,
				V4L2_CID_ROTATE,
				0, 270, 90, 0);
	}
	/* Color effect */
	if (ctx->scaler_id == SCALER_SCC)
		v4l2_ctrl_new_std_menu(handler, ops,
			V4L2_CID_COLORFX,
			V4L2_COLORFX_ANTIQUE,
			SCALER_COLOR_MODE_SKIP_MASK,
			V4L2_COLORFX_NONE);

	return handler->error;
}

static int scaler_subdev_registered(struct v4l2_subdev *sd)
{
	struct fimc_is_scaler *ctx = v4l2_get_subdevdata(sd);
	struct vb2_queue *q = &ctx->vbq;
	struct video_device *vfd = &ctx->vfd;
	int ret;

	mutex_init(&ctx->video_lock);

	memset(vfd, 0, sizeof(*vfd));
	if (ctx->scaler_id == SCALER_SCC)
		snprintf(vfd->name, sizeof(vfd->name), "fimc-is-scaler.codec");
	else
		snprintf(vfd->name, sizeof(vfd->name),
				"fimc-is-scaler.preview");

	vfd->fops = &scaler_video_capture_fops;
	vfd->ioctl_ops = &scaler_video_capture_ioctl_ops;
	vfd->v4l2_dev = sd->v4l2_dev;
	vfd->release = video_device_release_empty;
	vfd->lock = &ctx->video_lock;
	vfd->queue = q;
	vfd->vfl_dir = VFL_DIR_RX;
	set_bit(V4L2_FL_USE_FH_PRIO, &vfd->flags);

	memset(q, 0, sizeof(*q));
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_MMAP | VB2_DMABUF;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->ops = &scaler_video_capture_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct fimc_is_buf);
	q->drv_priv = ctx;
	q->lock = &ctx->video_lock;

	ret = vb2_queue_init(q);
	if (ret < 0)
		return ret;

	ctx->vd_pad.flags = MEDIA_PAD_FL_SINK;
	ret = media_entity_init(&vfd->entity, 1, &ctx->vd_pad, 0);
	if (ret < 0)
		return ret;

	video_set_drvdata(vfd, ctx);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		media_entity_cleanup(&vfd->entity);
		return ret;
	}

	v4l2_info(sd->v4l2_dev, "Registered %s as /dev/%s\n",
		  vfd->name, video_device_node_name(vfd));
	return 0;
}

static void scaler_subdev_unregistered(struct v4l2_subdev *sd)
{
	struct fimc_is_scaler *ctx = v4l2_get_subdevdata(sd);

	if (ctx && video_is_registered(&ctx->vfd))
		video_unregister_device(&ctx->vfd);
}

static const struct v4l2_subdev_internal_ops scaler_subdev_internal_ops = {
	.registered = scaler_subdev_registered,
	.unregistered = scaler_subdev_unregistered,
};

static struct v4l2_subdev_ops scaler_subdev_ops;

int fimc_is_scaler_subdev_create(struct fimc_is_scaler *ctx,
		enum fimc_is_scaler_id scaler_id,
		struct vb2_alloc_ctx *alloc_ctx,
		struct fimc_is_pipeline *pipeline)
{
	struct v4l2_ctrl_handler *handler = &ctx->ctrls.handler;
	struct v4l2_subdev *sd = &ctx->subdev;
	int ret;

	mutex_init(&ctx->lock);

	ctx->scaler_id = scaler_id;
	ctx->alloc_ctx = alloc_ctx;
	ctx->pipeline = pipeline;
	ctx->fmt = &formats[0];
	ctx->width = SCALER_DEF_WIDTH;
	ctx->height = SCALER_DEF_HEIGHT;
	init_waitqueue_head(&ctx->event_q);
	INIT_LIST_HEAD(&ctx->wait_queue);
	INIT_LIST_HEAD(&ctx->run_queue);

	/* Initialize scaler subdev */
	v4l2_subdev_init(sd, &scaler_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	if (scaler_id == SCALER_SCC)
		snprintf(sd->name, sizeof(sd->name), "fimc-is-scc");
	else
		snprintf(sd->name, sizeof(sd->name), "fimc-is-scp");

	ctx->subdev_pads[SCALER_SD_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	ctx->subdev_pads[SCALER_SD_PAD_SRC_FIFO].flags = MEDIA_PAD_FL_SOURCE;
	ctx->subdev_pads[SCALER_SD_PAD_SRC_DMA].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&sd->entity, ISP_SD_PADS_NUM,
			ctx->subdev_pads, 0);
	if (ret < 0)
		return ret;

	ret = v4l2_ctrl_handler_init(handler, SCALER_CTRL_NUM);
	if (handler->error)
		goto err_ctrl;
	ret = scaler_video_capture_create_controls(ctx);
	if (ret)
		goto err_ctrl;

	sd->ctrl_handler = handler;
	sd->internal_ops = &scaler_subdev_internal_ops;
	v4l2_set_subdevdata(sd, ctx);

	return 0;
err_ctrl:
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(handler);
	return ret;
}

void fimc_is_scaler_subdev_destroy(struct fimc_is_scaler *ctx)
{
	struct v4l2_subdev *sd = &ctx->subdev;

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&ctx->ctrls.handler);
	v4l2_set_subdevdata(sd, NULL);
}
