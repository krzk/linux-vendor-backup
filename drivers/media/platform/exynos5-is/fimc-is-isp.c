/*
 * Samsung EXYNOS5250/EXYNOS3 FIMC-IS (Imaging Subsystem) driver
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

#define ISP_DRV_NAME "fimc-is-isp"

static const struct fimc_is_fmt formats[] = {
	{
		.name           = "Bayer GR-BG 8bits",
		.fourcc         = V4L2_PIX_FMT_SGRBG8,
		.depth		= { 8 },
		.num_planes     = 1,
	},
	{
		.name           = "Bayer GR-BG 10bits",
		.fourcc         = V4L2_PIX_FMT_SGRBG10,
		.depth		= { 16 },
		.num_planes     = 1,
	},
	{
		.name           = "Bayer GR-BG 12bits",
		.fourcc         = V4L2_PIX_FMT_SGRBG12,
		.depth		= { 16 },
		.num_planes     = 1,
	},
};
#define NUM_FORMATS ARRAY_SIZE(formats)

#define ISP_VIDEO_NUM_CTRL	FIMC_IS_CID_MAX

static const struct fimc_is_fmt *find_format(struct v4l2_format *f)
{
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++)
		if (formats[i].fourcc == f->fmt.pix_mp.pixelformat)
			return &formats[i];
	return NULL;
}

static int isp_video_output_start_streaming(struct vb2_queue *vq,
					unsigned int count)
{
	struct fimc_is_isp *isp = vb2_get_drv_priv(vq);

	set_bit(STATE_RUNNING, &isp->output_state);
	return 0;
}

static void isp_video_output_stop_streaming(struct vb2_queue *vq)
{
	struct fimc_is_isp *isp = vb2_get_drv_priv(vq);
	struct fimc_is_buf *buf;

	/* Release unused buffers */
	fimc_is_pipeline_buf_lock(isp->pipeline);
	while (!list_empty(&isp->wait_queue)) {
		buf = fimc_is_isp_wait_queue_get(isp);
		buf->state = FIMC_IS_BUF_INVALID;
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}
	while (!list_empty(&isp->run_queue)) {
		buf = fimc_is_isp_run_queue_get(isp);
		buf->state = FIMC_IS_BUF_INVALID;
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
	}
	fimc_is_pipeline_buf_unlock(isp->pipeline);
	clear_bit(STATE_RUNNING, &isp->output_state);
	return;
}

static int isp_video_output_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *pfmt,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], void *allocators[])
{
	struct fimc_is_isp *isp = vb2_get_drv_priv(vq);
	const struct fimc_is_fmt *fmt = isp->fmt;
	unsigned int wh, i;

	if (!fmt)
		return -EINVAL;

	*num_planes = fmt->num_planes;
	wh = isp->width * isp->height;

	for (i = 0; i < *num_planes; i++) {
		allocators[i] = isp->alloc_ctx;
		sizes[i] = (wh * fmt->depth[i]) / 8;
	}
	return 0;
}

static int isp_video_output_buffer_init(struct vb2_buffer *vb)
{
	struct fimc_is_buf *buf = container_of(vb, struct fimc_is_buf, vb);

	buf->paddr[0] = vb2_dma_contig_plane_dma_addr(vb, 0);
	buf->state = FIMC_IS_BUF_IDLE;
	return 0;
}

static int isp_video_output_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct fimc_is_isp *isp = vb2_get_drv_priv(vq);
	unsigned long size;

	size = (isp->width * isp->height * isp->fmt->depth[0]) / 8;
	if (vb2_plane_size(vb, 0) < size) {
		v4l2_err(&isp->subdev, "User buffer too small (%ld < %ld)\n",
			 vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}
	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void isp_video_output_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct fimc_is_isp *isp = vb2_get_drv_priv(vq);
	struct fimc_is_buf *buf = container_of(vb, struct fimc_is_buf, vb);

	fimc_is_pipeline_buf_lock(isp->pipeline);
	fimc_is_isp_wait_queue_add(isp, buf);
	fimc_is_pipeline_buf_unlock(isp->pipeline);
	/* Call shot command */
	fimc_is_pipeline_shot_safe(isp->pipeline);
}

static void isp_video_output_buffer_cleanup(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct fimc_is_isp *isp = vb2_get_drv_priv(vq);
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

	fimc_is_pipeline_buf_lock(isp->pipeline);
	list_del(&fbuf->list);
	if (fbuf->state == FIMC_IS_BUF_QUEUED)
		--isp->wait_queue_cnt;
	else /* FIMC_IS_BUF_ACTIVE */
		--isp->run_queue_cnt;
	fbuf->state = FIMC_IS_BUF_INVALID;
	fimc_is_pipeline_buf_unlock(isp->pipeline);

}

static const struct vb2_ops isp_video_output_qops = {
	.queue_setup	 = isp_video_output_queue_setup,
	.buf_init	 = isp_video_output_buffer_init,
	.buf_prepare	 = isp_video_output_buffer_prepare,
	.buf_queue	 = isp_video_output_buffer_queue,
	.wait_prepare	 = vb2_ops_wait_prepare,
	.wait_finish	 = vb2_ops_wait_finish,
	.buf_cleanup	 = isp_video_output_buffer_cleanup,
	.start_streaming = isp_video_output_start_streaming,
	.stop_streaming	 = isp_video_output_stop_streaming,
};

static const struct v4l2_file_operations isp_video_output_fops = {
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
static int isp_querycap_output(struct file *file, void *priv,
					struct v4l2_capability *cap)
{
	strncpy(cap->driver, ISP_DRV_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, ISP_DRV_NAME, sizeof(cap->card) - 1);
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
			ISP_DRV_NAME);
	cap->device_caps = V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int isp_enum_fmt_mplane(struct file *file, void *priv,
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

static int isp_g_fmt_mplane(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct fimc_is_isp *isp = video_drvdata(file);
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *plane_fmt = &pixm->plane_fmt[0];
	const struct fimc_is_fmt *fmt = isp->fmt;

	plane_fmt->bytesperline = (isp->width * fmt->depth[0]) / 8;
	plane_fmt->sizeimage = plane_fmt->bytesperline * isp->height;
	memset(plane_fmt->reserved, 0, sizeof(plane_fmt->reserved));

	pixm->num_planes = fmt->num_planes;
	pixm->pixelformat = fmt->fourcc;
	pixm->width = isp->width;
	pixm->height = isp->height;
	pixm->field = V4L2_FIELD_NONE;
	pixm->colorspace = V4L2_COLORSPACE_JPEG;
	memset(pixm->reserved, 0, sizeof(pixm->reserved));

	return 0;
}

static int isp_try_fmt_mplane(struct file *file, void *fh,
		struct v4l2_format *f)
{
	const struct fimc_is_fmt *fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct v4l2_plane_pix_format *plane_fmt = &pixm->plane_fmt[0];

	fmt = find_format(f);
	if (!fmt)
		fmt = (struct fimc_is_fmt *) &formats[0];

        v4l_bound_align_image(&pixm->width, ISP_MIN_WIDTH, ISP_MAX_WIDTH, 0,
                        &pixm->height, ISP_MIN_HEIGHT, ISP_MAX_HEIGHT, 0,
			0);

	plane_fmt->bytesperline = (pixm->width * fmt->depth[0]) / 8;
	plane_fmt->sizeimage = (pixm->width * pixm->height *
				fmt->depth[0]) / 8;
	memset(plane_fmt->reserved, 0, sizeof(plane_fmt->reserved));

	pixm->num_planes = fmt->num_planes;
	pixm->pixelformat = fmt->fourcc;
	pixm->colorspace = V4L2_COLORSPACE_JPEG;
	pixm->field = V4L2_FIELD_NONE;
	memset(pixm->reserved, 0, sizeof(pixm->reserved));

	return 0;
}

static int isp_s_fmt_mplane(struct file *file, void *priv,
		struct v4l2_format *f)
{
	struct fimc_is_isp *isp = video_drvdata(file);
	const struct fimc_is_fmt *fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int ret;

	if (test_bit(STATE_RUNNING, &isp->output_state))
		return -EBUSY;

	ret = isp_try_fmt_mplane(file, priv, f);
	if (ret)
		return ret;

	/* Get format type */
	fmt = find_format(f);
	if (!fmt) {
		fmt = &formats[0];
		pixm->pixelformat = fmt->fourcc;
		pixm->num_planes = fmt->num_planes;
	}

	isp->fmt = fmt;
	isp->width = pixm->width;
	isp->height = pixm->height;
	isp->size_image = pixm->plane_fmt[0].sizeimage;
	set_bit(STATE_INIT, &isp->output_state);
	return 0;
}

static int isp_reqbufs(struct file *file, void *priv,
		struct v4l2_requestbuffers *reqbufs)
{
	struct fimc_is_isp *isp = video_drvdata(file);
	int ret;

	if (test_bit(STATE_RUNNING, &isp->output_state))
		return -EBUSY;

	if (test_bit(STATE_BUFS_ALLOCATED, &isp->output_state)
	&& !(reqbufs->count)){
		ret = vb2_reqbufs(&isp->vbq, reqbufs);
		if (!ret)
			clear_bit(STATE_BUFS_ALLOCATED, &isp->output_state);
		return ret;
	}

	reqbufs->count = max_t(u32, FIMC_IS_ISP_REQ_BUFS_MIN, reqbufs->count);
	ret = vb2_ioctl_reqbufs(file, priv, reqbufs);
	if (ret) {
		v4l2_err(&isp->subdev, "vb2 req buffers failed\n");
		return ret;
	}

	if (reqbufs->count < FIMC_IS_ISP_REQ_BUFS_MIN) {
		reqbufs->count = 0;
		vb2_reqbufs(&isp->vbq, reqbufs);
		return -ENOMEM;
	}
	set_bit(STATE_BUFS_ALLOCATED, &isp->output_state);
	return 0;
}

static const struct v4l2_ioctl_ops isp_video_output_ioctl_ops = {
	.vidioc_querycap		= isp_querycap_output,
	.vidioc_enum_fmt_vid_out_mplane	= isp_enum_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane	= isp_try_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= isp_s_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane	= isp_g_fmt_mplane,
	.vidioc_reqbufs			= isp_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_streamon		= vb2_ioctl_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
};

static int isp_subdev_registered(struct v4l2_subdev *sd)
{
	struct fimc_is_isp *isp = v4l2_get_subdevdata(sd);
	struct vb2_queue *q = &isp->vbq;
	struct video_device *vfd = &isp->vfd;
	int ret;

	mutex_init(&isp->video_lock);
	mutex_init(&isp->lock);

	memset(vfd, 0, sizeof(*vfd));
	snprintf(vfd->name, sizeof(vfd->name), "fimc-is-isp.output");

	vfd->fops = &isp_video_output_fops;
	vfd->ioctl_ops = &isp_video_output_ioctl_ops;
	vfd->v4l2_dev = sd->v4l2_dev;
	vfd->release = video_device_release_empty;
	vfd->lock = &isp->video_lock;
	vfd->queue = q;
	vfd->vfl_dir = VFL_DIR_TX;
	set_bit(V4L2_FL_USE_FH_PRIO, &vfd->flags);

	memset(q, 0, sizeof(*q));
	q->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	q->io_modes = VB2_MMAP | VB2_DMABUF;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	q->ops = &isp_video_output_qops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct fimc_is_buf);
	q->drv_priv = isp;
	q->lock = &isp->video_lock;
	ret = vb2_queue_init(q);
	if (ret < 0)
		return ret;

	isp->vd_pad.flags = MEDIA_PAD_FL_SINK;
	ret = media_entity_init(&vfd->entity, 1, &isp->vd_pad, 0);
	if (ret < 0)
		return ret;

	video_set_drvdata(vfd, isp);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		media_entity_cleanup(&vfd->entity);
		return ret;
	}
	v4l2_info(sd->v4l2_dev, "Registered %s as /dev/%s\n",
		  vfd->name, video_device_node_name(vfd));
	return 0;
}

static void isp_subdev_unregistered(struct v4l2_subdev *sd)
{
	struct fimc_is_isp *isp = v4l2_get_subdevdata(sd);

	if (isp && video_is_registered(&isp->vfd))
		video_unregister_device(&isp->vfd);
}

static const struct v4l2_subdev_internal_ops isp_subdev_internal_ops = {
	.registered = isp_subdev_registered,
	.unregistered = isp_subdev_unregistered,
};

static struct fimc_is_sensor *fimc_is_get_sensor(struct fimc_is *is,
		int sensor_id)
{
	int i;

	for (i = 0; i < FIMC_IS_NUM_SENSORS; i++) {
		if (is->sensor[i].drvdata->id == sensor_id)
			return &is->sensor[i];
	}
	return NULL;
}

static int isp_s_power(struct v4l2_subdev *sd, int on)
{
	struct fimc_is_isp *isp = v4l2_get_subdevdata(sd);
	struct fimc_is *is = isp->pipeline->is;
	struct v4l2_subdev *sensor_sd = isp->sensor_sd;
	struct fimc_is_sensor *sensor;
	const struct sensor_drv_data *sdata;
	int ret;

	if (!sensor_sd)
		return -EINVAL;

	if (on) {
		ret = pm_runtime_get_sync(&is->pdev->dev);
		if (ret < 0)
			return ret;

		sdata = exynos5_is_sensor_get_drvdata(sensor_sd->dev->of_node);
		sensor = fimc_is_get_sensor(is, sdata->id);

		ret = fimc_is_pipeline_open(isp->pipeline, sensor);
		if (ret) {
			v4l2_err(&isp->subdev, "Pipeline open failed\n");
		} else {
			ret = v4l2_ctrl_handler_setup(&isp->ctrls.handler);
			if (ret)
				v4l2_err(&isp->subdev, "Failed to setup controls\n");
		}

	} else {
		ret = fimc_is_pipeline_close(isp->pipeline);
		if (ret)
			v4l2_err(&isp->subdev, "Pipeline close failed\n");
		pm_runtime_put_sync(&is->pdev->dev);
	}

	return ret;
}

static struct v4l2_subdev_core_ops isp_core_ops = {
	.s_power = isp_s_power,
};

static int isp_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct fimc_is_isp *isp = v4l2_get_subdevdata(sd);
	struct fimc_is *is = isp->pipeline->is;
	struct v4l2_subdev *sensor_sd = isp->sensor_sd;
	struct v4l2_subdev_format fmt;
	const struct sensor_drv_data *sdata;
	struct fimc_is_sensor *sensor;
	int ret;

	if (!sensor_sd)
		return -EINVAL;

	if (enable) {
		sdata = exynos5_is_sensor_get_drvdata(sensor_sd->dev->of_node);
		sensor = fimc_is_get_sensor(is, sdata->id);
		/* Retrieve the sensor format */
		fmt.pad = 0;
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(sensor_sd, pad, get_fmt, NULL, &fmt);
		if (ret)
			return ret;

                sensor->width = fmt.format.width;
                sensor->height = fmt.format.height;
		sensor->pixel_width = fmt.format.width;
		sensor->pixel_height = fmt.format.height;

		/* Check sensor resolution match */
                if ((sensor->width >= sdata->pixel_width) ||
                    (sensor->height >= sdata->pixel_height)) {
                        v4l2_err(sd, "The sensor does not support "
                                     "requested resolution: %dx%d vs %dx%d\n",
                                     sensor->width, sensor->height,
				     sdata->pixel_width, sdata->pixel_height);
                        return -EINVAL;
                }

                /* ISP supports only bayer cropping and scaling down */
                if ((sensor->pixel_width < isp->width) ||
                    (sensor->pixel_height < isp->height)) {
			v4l2_err(sd, "Resolution mismatch\n");
			return -EPIPE;
		}

		ret = fimc_is_pipeline_start(isp->pipeline, 1);
		if (ret)
			v4l2_err(sd, "Pipeline start failed.\n");
	} else {
		ret = fimc_is_pipeline_stop(isp->pipeline, 1);
		if (ret)
			v4l2_err(sd, "Pipeline stop failed.\n");
	}

	return ret;
}

static const struct v4l2_subdev_video_ops isp_video_ops = {
	.s_stream       = isp_s_stream,
};

static const s64 isp_video_iso_qmenu[] = {
	80, 100, 200, 400, 800,
};

static inline int isp_video_set_color_ctrl(struct fimc_is_isp *isp,
				    unsigned int id,
				    unsigned long val)
{
	/*
	 * @TODO: Change it to queueing the requestes and setting in at one go
	 * instead of making function call per each control
	 */
	return fimc_is_pipeline_update_shot_ctrl(isp->pipeline,
				  id, val);
}

static inline int isp_video_set_sensor_ctrl(struct fimc_is_isp *isp,
					    unsigned int id,
					    unsigned long val)
{
	/* @TODO: Make some reasonable adjustment between the settings */
	switch (id) {
	case FIMC_IS_CID_EXPOSURE:
		val *= 1000000;
		break;
	case FIMC_IS_CID_ISO:
		val = isp_video_iso_qmenu[val];
		break;
	case FIMC_IS_CID_ISO_MODE:
		val = val ? AA_ISOMODE_AUTO : AA_ISOMODE_MANUAL;
		break;
	}
	return fimc_is_pipeline_update_shot_ctrl(isp->pipeline,
				  id, val);
}

static const unsigned int awb_mode_map[] = {
	[V4L2_WHITE_BALANCE_MANUAL]        = AA_AWBMODE_OFF,
	[V4L2_WHITE_BALANCE_AUTO]          = AA_AWBMODE_WB_AUTO,
	[V4L2_WHITE_BALANCE_INCANDESCENT]  = AA_AWBMODE_WB_INCANDESCENT,
	[V4L2_WHITE_BALANCE_FLUORESCENT]   = AA_AWBMODE_WB_FLUORESCENT,
	[V4L2_WHITE_BALANCE_FLUORESCENT_H] = AA_AWBMODE_WB_WARM_FLUORESCENT,
	[V4L2_WHITE_BALANCE_DAYLIGHT]      = AA_AWBMODE_WB_DAYLIGHT,
	[V4L2_WHITE_BALANCE_CLOUDY]        = AA_AWBMODE_WB_CLOUDY_DAYLIGHT,
	[V4L2_WHITE_BALANCE_SHADE]         = AA_AWBMODE_WB_SHADE,
};

static inline int isp_video_set_awb_ctrl(struct fimc_is_isp *isp,
					 unsigned int id,
					 unsigned long val)
{
	if (id == FIMC_IS_CID_AWB)
		val = val  ? AA_AWBMODE_WB_AUTO : AA_AWBMODE_OFF;
	else {
		/* Boundary check should be performed by v4l2 */
		if (val >= ARRAY_SIZE(awb_mode_map))
			return -EINVAL;
		val = awb_mode_map[val];
		if (!val)
			val = AA_AWBMODE_WB_AUTO;
	}

	return fimc_is_pipeline_update_shot_ctrl(isp->pipeline,
				  id, val);
}

static inline int isp_video_set_focus_ctrl(struct fimc_is_isp *isp,
					  unsigned int id,
					  unsigned long val)
{
	if (id == FIMC_IS_CID_FOCUS_MODE)
		val = val ? AA_AFMODE_AUTO : AA_AFMODE_OFF;

	else
		val = (val == V4L2_AUTO_FOCUS_RANGE_NORMAL)
		      ? AA_AFMODE_CONTINUOUS_PICTURE
		      : AA_AFMODE_MACRO;

	return fimc_is_pipeline_update_shot_ctrl(isp->pipeline,
				  id, val);

}

static const unsigned int scene_map[] = {
	[V4L2_SCENE_MODE_NONE]		= AA_SCENE_MODE_UNSUPPORTED,
	[V4L2_SCENE_MODE_BEACH_SNOW]	= AA_SCENE_MODE_BEACH,
	[V4L2_SCENE_MODE_CANDLE_LIGHT]	= AA_SCENE_MODE_CANDLELIGHT,
	[V4L2_SCENE_MODE_FIREWORKS]	= AA_SCENE_MODE_FIREWORKS,
	[V4L2_SCENE_MODE_LANDSCAPE]	= AA_SCENE_MODE_LANDSCAPE,
	[V4L2_SCENE_MODE_NIGHT]		= AA_SCENE_MODE_NIGHT,
	[V4L2_SCENE_MODE_PARTY_INDOOR]	= AA_SCENE_MODE_PARTY,
	[V4L2_SCENE_MODE_PORTRAIT]	= AA_SCENE_MODE_PORTRAIT,
	[V4L2_SCENE_MODE_SPORTS]	= AA_SCENE_MODE_SPORTS,
	[V4L2_SCENE_MODE_SUNSET]	= AA_SCENE_MODE_SUNSET,
};

#define ISP_SCENE_MODE_SKIP_MASK	     \
	(1 << (V4L2_SCENE_MODE_BACKLIGHT)  | \
	1 << (V4L2_SCENE_MODE_DAWN_DUSK)   | \
	1 << (V4L2_SCENE_MODE_FALL_COLORS) | \
	1 << (V4L2_SCENE_MODE_TEXT ))

static inline int isp_video_set_scene_ctrl(struct fimc_is_isp *isp,
					   unsigned long val)
{
	/* Boundary check should be performed by v4l2 */
	if (val >= ARRAY_SIZE(scene_map))
		return -EINVAL;

	val = scene_map[val];
	if (!val)
		val = AA_SCENE_MODE_UNSUPPORTED;
	/* @TODO: Set remaining controls based on scene chosen*/
	return fimc_is_pipeline_update_shot_ctrl(isp->pipeline,
				  FIMC_IS_CID_SCENE, val);

}

static int isp_video_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct fimc_is_isp *isp = container_of(ctrl->handler,
					       struct fimc_is_isp,
					       ctrls.handler);
	int ret = 0;

	mutex_lock(&isp->lock);

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		isp_video_set_color_ctrl(isp, FIMC_IS_CID_BRIGHTNESS,
					       ctrl->val);
		break;
	case V4L2_CID_CONTRAST:
		isp_video_set_color_ctrl(isp, FIMC_IS_CID_CONTRAST,
					      ctrl->val);
		break;
	case V4L2_CID_SATURATION:
		isp_video_set_color_ctrl(isp, FIMC_IS_CID_SATURATION,
					       ctrl->val);
		break;
	case V4L2_CID_HUE:
		isp_video_set_color_ctrl(isp, FIMC_IS_CID_HUE,
					       ctrl->val);
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		isp_video_set_awb_ctrl(isp, FIMC_IS_CID_AWB,
					     ctrl->val);
		break;
	case V4L2_CID_EXPOSURE_ABSOLUTE:
		isp_video_set_sensor_ctrl(isp, FIMC_IS_CID_EXPOSURE,
						ctrl->val);
		break;
	case V4L2_CID_FOCUS_AUTO:
		isp_video_set_focus_ctrl(isp, FIMC_IS_CID_FOCUS_MODE,
					       ctrl->val);
		break;
	case V4L2_CID_AUTO_FOCUS_RANGE:
		isp_video_set_focus_ctrl(isp, FIMC_IS_CID_FOCUS_RANGE,
					       ctrl->val);
		break;
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
		isp_video_set_awb_ctrl(isp, FIMC_IS_CID_AWB_MODE,
					     ctrl->val);
		break;
	case V4L2_CID_ISO_SENSITIVITY:
		isp_video_set_sensor_ctrl(isp, FIMC_IS_CID_ISO,
						ctrl->val);
		break;
	case V4L2_CID_ISO_SENSITIVITY_AUTO:
		isp_video_set_sensor_ctrl(isp, FIMC_IS_CID_ISO_MODE,
						ctrl->val);
		break;
	case V4L2_CID_SCENE_MODE:
		isp_video_set_scene_ctrl(isp, ctrl->val);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&isp->lock);

	return ret;
}

static const struct v4l2_ctrl_ops isp_video_ctrl_ops = {
	.s_ctrl = isp_video_s_ctrl,
};

static int isp_video_create_controls(struct fimc_is_isp *isp)
{

	struct v4l2_ctrl_handler   *handler = &isp->ctrls.handler;
	const struct v4l2_ctrl_ops *ops     = &isp_video_ctrl_ops;
	struct fimc_is_isp_ctrls   *ctrls   = &isp->ctrls;

	/* Color controls cluster: brightness */
	ctrls->brightness = v4l2_ctrl_new_std(handler, ops,
				V4L2_CID_BRIGHTNESS, 0, 5, 1, 0);
	/* Color controls cluster: contrast */
	ctrls->contrast = v4l2_ctrl_new_std(handler, ops,
				V4L2_CID_CONTRAST, 0, 5, 1, 3);
	/* Color controls cluster : saturation */
	ctrls->saturation = v4l2_ctrl_new_std(handler, ops,
				V4L2_CID_SATURATION, 0, 5, 1, 1);
	/* Color controls cluster: hue */
	ctrls->hue = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_HUE, 0, 5, 1, 0);
	/* White balance control cluster: state */
	ctrls->awb = v4l2_ctrl_new_std(handler, ops,
			V4L2_CID_AUTO_WHITE_BALANCE, 0, 1, 1, 1);
	/* Exposure absolute time control */
	ctrls->exposure = v4l2_ctrl_new_std(handler, ops,
				V4L2_CID_EXPOSURE_ABSOLUTE, 4, 1800, 1, 200);
	/* Focus controls cluster: focus mode */
	ctrls->focus = v4l2_ctrl_new_std(handler, ops,
				V4L2_CID_FOCUS_AUTO, 0, 1, 1, 0);
	/* Focus controls cluster: focus range */
	ctrls->focus_range = v4l2_ctrl_new_std_menu(handler, ops,
				V4L2_CID_AUTO_FOCUS_RANGE,
				V4L2_AUTO_FOCUS_RANGE_MACRO,
				~(1 << V4L2_AUTO_FOCUS_RANGE_NORMAL |
				1 << V4L2_AUTO_FOCUS_RANGE_MACRO),
				V4L2_AUTO_FOCUS_RANGE_NORMAL);
	/* White balance control cluster : mode */
	ctrls->awb_mode = v4l2_ctrl_new_std_menu(handler, ops,
				V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE,
				V4L2_WHITE_BALANCE_SHADE,
				1 << V4L2_WHITE_BALANCE_HORIZON |
				1 << V4L2_WHITE_BALANCE_FLASH,
				V4L2_WHITE_BALANCE_AUTO);
	/* ISO controls cluster: value */
	ctrls->iso = v4l2_ctrl_new_int_menu(handler, ops,
				V4L2_CID_ISO_SENSITIVITY,
				ARRAY_SIZE(isp_video_iso_qmenu) - 1,
				ARRAY_SIZE(isp_video_iso_qmenu)/2 - 1,
				isp_video_iso_qmenu);
	/* ISO controls cluster: mode */
	ctrls->iso_mode = v4l2_ctrl_new_std_menu(handler, ops,
				V4L2_CID_ISO_SENSITIVITY_AUTO,
				0, 0, 0);

	/* Scene mode control */
	ctrls->scene_mode = v4l2_ctrl_new_std_menu(handler, ops,
				V4L2_CID_SCENE_MODE,
				V4L2_SCENE_MODE_SUNSET,
				ISP_SCENE_MODE_SKIP_MASK,
				V4L2_SCENE_MODE_NONE);
	return handler->error;
}

static struct v4l2_subdev_ops isp_subdev_ops = {
	.core = &isp_core_ops,
	.video = &isp_video_ops,
};

int fimc_is_isp_subdev_create(struct fimc_is_isp *isp,
		struct vb2_alloc_ctx *alloc_ctx,
		struct fimc_is_pipeline *pipeline)
{
	struct v4l2_ctrl_handler *handler = &isp->ctrls.handler;
	struct v4l2_subdev *sd = &isp->subdev;
	int ret;

	isp->alloc_ctx = alloc_ctx;
	isp->pipeline = pipeline;
	isp->fmt = &formats[1];
	INIT_LIST_HEAD(&isp->wait_queue);
	INIT_LIST_HEAD(&isp->run_queue);
	isp->width = ISP_DEF_WIDTH;
	isp->height = ISP_DEF_HEIGHT;

	v4l2_subdev_init(sd, &isp_subdev_ops);
	sd->owner = THIS_MODULE;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), ISP_DRV_NAME);

	isp->subdev_pads[ISP_SD_PAD_SINK_DMA].flags = MEDIA_PAD_FL_SINK;
	isp->subdev_pads[ISP_SD_PAD_SINK_OTF].flags = MEDIA_PAD_FL_SINK;
	isp->subdev_pads[ISP_SD_PAD_SRC].flags = MEDIA_PAD_FL_SOURCE;
	ret = media_entity_init(&sd->entity, ISP_SD_PADS_NUM,
			isp->subdev_pads, 0);
	if (ret < 0)
		return ret;

	ret = v4l2_ctrl_handler_init(handler, ISP_VIDEO_NUM_CTRL);
	if (handler->error)
		goto err_ctrl;
	ret = isp_video_create_controls(isp);
	if (ret)
		goto err_ctrl;

	sd->ctrl_handler = handler;
	sd->internal_ops = &isp_subdev_internal_ops;
	v4l2_set_subdevdata(sd, isp);

	return 0;

err_ctrl:
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(handler);
	return ret;
}

void fimc_is_isp_subdev_destroy(struct fimc_is_isp *isp)
{
	struct v4l2_subdev *sd = &isp->subdev;

	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);
	v4l2_ctrl_handler_free(&isp->ctrls.handler);
	v4l2_set_subdevdata(sd, NULL);
}
