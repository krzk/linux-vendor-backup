/*
 * Samsung EXYNOS5 FIMC-IS (Imaging Subsystem) driver
*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Arun Kumar K <arun.kk@samsung.com>
 * Kil-yeon Lim <kilyeon.im@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is.h"
#include "fimc-is-pipeline.h"
#include "fimc-is-metadata.h"
#include "fimc-is-regs.h"
#include "fimc-is-cmd.h"
#include "fimc-is-fw.h"
#include <media/videobuf2-dma-contig.h>
#include <linux/delay.h>
#include <asm/firmware.h>

/* Default setting values */
#define DEFAULT_PREVIEW_STILL_WIDTH		1280
#define DEFAULT_PREVIEW_STILL_HEIGHT		720
#define DEFAULT_CAPTURE_VIDEO_WIDTH		1920
#define DEFAULT_CAPTURE_VIDEO_HEIGHT		1080
#define DEFAULT_CAPTURE_STILL_WIDTH		2560
#define DEFAULT_CAPTURE_STILL_HEIGHT		1920
#define DEFAULT_CAPTURE_STILL_CROP_WIDTH	2560
#define DEFAULT_CAPTURE_STILL_CROP_HEIGHT	1440
#define DEFAULT_PREVIEW_VIDEO_WIDTH		640
#define DEFAULT_PREVIEW_VIDEO_HEIGHT		480

#define FIMC_IS_SENSOR_ID		0x00
#define FIMC_IS_SENSOR_I2C_CHAN 	0x02
#define FIMC_IS_SENSOR_I2C_ADDR 	0x03
#define FIMC_IS_SESNOR_I2C_SPEED 	0x04
#define FIMC_IS_SENSOR_MIPI_LANES 	0x3f
#define FIMC_IS_SENSOR_I2C_SCLK		0x44

#define FIMC_IS_PARAMS_MASK_ALL		0xffffffffUL
#define FIMC_IS_PARAMS_ALL		64

#define FIMC_IS_SHOT_SIZE		0x8000

#define FIMC_SCC_MAX_RESIZE_RATIO	4
#define FIMC_SCC_MIN_RESIZE_RATIO	16


/* Init params for pipeline devices */
static const struct sensor_param init_sensor_param = {
	.frame_rate = {
		.frame_rate = 30,
	},
};

static const struct taa_param init_taa_param = {
        .control= {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_DISABLE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_BAYER,
		.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
        .dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
        .dma_output = {
                .cmd = DMA_OUTPUT_COMMAND_DISABLE,
	},
};

static const struct isp_param init_isp_param = {
        .control = {
                .cmd = CONTROL_COMMAND_START,
                .bypass = CONTROL_BYPASS_DISABLE,
	},
        .otf_input = {
                .cmd = OTF_INPUT_COMMAND_DISABLE,
                .width = DEFAULT_CAPTURE_STILL_WIDTH,
                .height = DEFAULT_CAPTURE_STILL_HEIGHT,
                .format = OTF_INPUT_FORMAT_BAYER,
                .bitwidth = OTF_INPUT_BIT_WIDTH_10BIT,
                .order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
        .dma_input = {
                .cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
        .__dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = 1,
		.order = DMA_INPUT_ORDER_YCBCR,
	},
        .dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_BAYER,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT,
		.plane = 1,
		.order = DMA_OUTPUT_ORDER_GB_BG,
		.dma_out_mask = 0xffffffff,
	},
};

static const struct drc_param init_drc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = 1,
		.order = DMA_INPUT_ORDER_YCBCR,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
                .bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
};

static const struct scalerc_param init_scalerc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.crop_width = DEFAULT_CAPTURE_STILL_CROP_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_CROP_HEIGHT,
		.in_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.in_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.out_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.out_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
	},
	.output_crop = {
		.cmd = SCALER_CROP_COMMAND_DISABLE,
		.crop_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.crop_height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV420,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = 3,
		.order = DMA_OUTPUT_ORDER_NONE,
		.dma_out_mask = 0xffff,
	},
};

static const struct odc_param init_odc_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
		.err = CONTROL_ERROR_NONE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
};

static const struct dis_param init_dis_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV422,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
};

static const struct tdnr_param init_tdnr_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV422,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = DMA_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = 2,
		.order = DMA_OUTPUT_ORDER_CBCR,
		.dma_out_mask = 0xffff,
	},
};

static const struct scalerp_param init_scalerp_param = {
	.control = {
		.cmd = CONTROL_COMMAND_START,
		.bypass = CONTROL_BYPASS_ENABLE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
	.input_crop = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.crop_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.crop_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.in_width = DEFAULT_CAPTURE_VIDEO_WIDTH,
		.in_height = DEFAULT_CAPTURE_VIDEO_HEIGHT,
		.out_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.out_height = DEFAULT_PREVIEW_STILL_HEIGHT,
	},
	.output_crop = {
		.cmd = SCALER_CROP_COMMAND_DISABLE,
		.crop_width = DEFAULT_PREVIEW_STILL_WIDTH,
		.crop_height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
	.dma_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV420,
		.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT,
		.plane = 3,
		.order = DMA_OUTPUT_ORDER_NONE,
		.dma_out_mask = 0xffff,
	},
};

static const struct fd_param init_fd_param = {
	.control = {
		.cmd = CONTROL_COMMAND_STOP,
		.bypass = CONTROL_BYPASS_DISABLE,
	},
	.otf_input = {
		.cmd = OTF_INPUT_COMMAND_ENABLE,
		.width = DEFAULT_PREVIEW_STILL_WIDTH,
		.height = DEFAULT_PREVIEW_STILL_HEIGHT,
		.format = OTF_INPUT_FORMAT_YUV444,
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
		.order = OTF_INPUT_ORDER_BAYER_GR_BG,
	},
	.dma_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.config = {
		.cmd = FD_CONFIG_COMMAND_MAXIMUM_NUMBER |
			FD_CONFIG_COMMAND_ROLL_ANGLE |
			FD_CONFIG_COMMAND_YAW_ANGLE |
			FD_CONFIG_COMMAND_SMILE_MODE |
			FD_CONFIG_COMMAND_BLINK_MODE |
			FD_CONFIG_COMMAND_EYES_DETECT |
			FD_CONFIG_COMMAND_MOUTH_DETECT |
			FD_CONFIG_COMMAND_ORIENTATION |
			FD_CONFIG_COMMAND_ORIENTATION_VALUE,
		.max_number = CAMERA2_MAX_FACES,
		.roll_angle = FD_CONFIG_ROLL_ANGLE_FULL,
		.yaw_angle = FD_CONFIG_YAW_ANGLE_45_90,
		.smile_mode = FD_CONFIG_SMILE_MODE_DISABLE,
		.blink_mode = FD_CONFIG_BLINK_MODE_DISABLE,
		.eye_detect = FD_CONFIG_EYES_DETECT_ENABLE,
		.mouth_detect = FD_CONFIG_MOUTH_DETECT_DISABLE,
		.orientation = FD_CONFIG_ORIENTATION_DISABLE,
	},
};

struct workqueue_struct *fimc_is_pipeline_workqueue;

#define NSEC_PER_MSEC   1000000L
#define FIMC_IS_MIN_FRAME_DURATION (5 * (NSEC_PER_MSEC))

#define __fimc_is_has_subip(p, subip) ((p)->subip_mask & (1 << (subip)))

static int fimc_is_pipeline_create_subdevs(struct fimc_is_pipeline *pipeline)
{
	struct fimc_is *is = pipeline->is;
	int ret;

	/* ISP */
	ret = fimc_is_isp_subdev_create(&pipeline->isp,
			is->alloc_ctx, pipeline);
	if (ret)
		return ret;

	/* SCC scaler */
	ret = fimc_is_scaler_subdev_create(&pipeline->scaler[SCALER_SCC],
			SCALER_SCC, is->alloc_ctx, pipeline);
	if (ret)
		return ret;

        /* SCP scaler  - if supported */
        if (pipeline->subip_state[IS_SCP] != COMP_INVALID)
                ret = fimc_is_scaler_subdev_create(&pipeline->scaler[SCALER_SCP],
                                        SCALER_SCP, is->alloc_ctx, pipeline);
	if (ret)
		return ret;

	return ret;
}

static int fimc_is_pipeline_unregister_subdevs(struct fimc_is_pipeline *p)
{
	fimc_is_isp_subdev_destroy(&p->isp);
	fimc_is_scaler_subdev_destroy(&p->scaler[SCALER_SCC]);
        if (p->subip_state[IS_SCP] != COMP_INVALID)
                fimc_is_scaler_subdev_destroy(&p->scaler[SCALER_SCP]);

	return 0;
}

void fimc_is_pipeline_notify(struct fimc_is_event_listener *listener,
                             unsigned int event_id,
                             void *arg)
{
        struct fimc_is_pipeline *pipeline = (struct fimc_is_pipeline*)listener->private_data;
        struct fimc_is *is = pipeline->is;
        if (EXYNOS_FIMC_IS_FRAME_DONE == event_id) {
                ktime_t now = ktime_get();
                long long delta = (pipeline->frame_duration)
                                ? ktime_to_ns(ktime_sub(now, pipeline->last_frame))
                                : FIMC_IS_MIN_FRAME_DURATION;
                pipeline->frame_duration = max(pipeline->frame_duration, delta);
                ++pipeline->fcount;
                fimc_is_itf_notify_frame_done(&is->interface);
                pipeline->last_frame = now;
        }
}


static void fimc_is_pipeline_work_fn(struct work_struct*);
static void fimc_is_pipeline_wdg_work_fn(struct work_struct*);

static int fimc_is_pipelien_init_workqueue(void)
{
        fimc_is_pipeline_workqueue = alloc_workqueue("fimc-is", WQ_FREEZABLE,0);
        return fimc_is_pipeline_workqueue ? 0 : -ENOMEM;
}

int fimc_is_pipeline_init(struct fimc_is_pipeline *pipeline,
                        unsigned int instance,
                        void *is_ctx)
{
	struct fimc_is *is = is_ctx;
	unsigned int i;
	int ret;

	if (test_bit(PIPELINE_INIT, &pipeline->state))
		return -EINVAL;

	/* Initialize context variables */
	pipeline->instance = instance;
	pipeline->is = is;
	pipeline->dev = &is->pdev->dev;
	pipeline->minfo = &is->minfo;
	pipeline->state = 0;
        pipeline->subip_mask = is->drvdata->subip_mask;
	pipeline->force_down = false;
	for (i = 0; i < FIMC_IS_NUM_COMPS; i++)
                pipeline->subip_state[i] = __fimc_is_has_subip(pipeline, i);

	spin_lock_init(&pipeline->slock_buf);
	init_waitqueue_head(&pipeline->wait_q);
	mutex_init(&pipeline->pipe_lock);
	mutex_init(&pipeline->pipe_scl_lock);

        ret = fimc_is_pipelien_init_workqueue();
        if (ret) {
                dev_err(pipeline->dev, "Failed to initialize workqueue\n");
                return -EINVAL;
        }

	ret = fimc_is_pipeline_create_subdevs(pipeline);
	if (ret) {
		dev_err(pipeline->dev, "Subdev creation failed\n");
		return -EINVAL;
	}

        pipeline->listener.private_data = pipeline;
        pipeline->listener.event_handler = fimc_is_pipeline_notify;
        INIT_LIST_HEAD(&pipeline->listener.link);

        INIT_WORK(&pipeline->work, fimc_is_pipeline_work_fn);
        INIT_DEFERRABLE_WORK(&pipeline->wdg_work, fimc_is_pipeline_wdg_work_fn);

	set_bit(PIPELINE_INIT, &pipeline->state);
	return 0;
}

int fimc_is_pipeline_destroy(struct fimc_is_pipeline *pipeline)
{
	if (!pipeline)
		return -EINVAL;

	if (!test_bit(PIPELINE_INIT, &pipeline->state))
		return -EINVAL;
	return fimc_is_pipeline_unregister_subdevs(pipeline);
}

static int fimc_is_pipeline_initmem(struct fimc_is_pipeline *pipeline)
{
	struct fimc_is_meminfo *minfo = pipeline->minfo;
        struct fimc_is_fw_mem fw_minfo;
	unsigned int offset;

        if (FIMC_IS_FW_CALL_OP(mem_config, &fw_minfo)){
                dev_err(pipeline->dev,
                        "Failed to get firmware memory layout\n");
                return -EINVAL;
        }
	/* Allocate fw memory */
        minfo->fw.vaddr = dma_zalloc_coherent(pipeline->dev, fw_minfo.fw_size,
			&minfo->fw.paddr, GFP_KERNEL);
	if (minfo->fw.vaddr == NULL)
		return -ENOMEM;
        minfo->fw.size = fw_minfo.fw_size;

	/* FW memory should be 64MB aligned */
	if ((u32)minfo->fw.paddr & FIMC_IS_FW_BASE_MASK) {
		dev_err(pipeline->dev, "FW memory not 64MB aligned\n");
		dma_free_coherent(pipeline->dev, minfo->fw.size,
				minfo->fw.vaddr,
				minfo->fw.paddr);
		return -EIO;
	}

	/* Assigning memory regions */
        offset = fw_minfo.region_offset;
	minfo->region.paddr = minfo->fw.paddr + offset;
	minfo->region.vaddr = minfo->fw.vaddr + offset;
	pipeline->is_region = (struct is_region *)minfo->region.vaddr;

	minfo->shared.paddr = minfo->fw.paddr +
		(unsigned int)((void *)&pipeline->is_region->shared[0] -
		 minfo->fw.vaddr);
	minfo->shared.vaddr = minfo->fw.vaddr +
		(unsigned int)((void *)&pipeline->is_region->shared[0] -
		 minfo->fw.vaddr);

	/* Allocate shot buffer */
        minfo->shot.vaddr = dma_zalloc_coherent(pipeline->dev,
                        fw_minfo.shot_size,
			&minfo->shot.paddr, GFP_KERNEL);
	if (minfo->shot.vaddr == NULL) {
		dma_free_coherent(pipeline->dev, minfo->fw.size,
				minfo->fw.vaddr,
				minfo->fw.paddr);
		return -ENOMEM;
	}
        minfo->shot.size = fw_minfo.shot_size;
        pipeline->config.metadata_mode = fw_minfo.meta_type;
	return 0;
}

static void fimc_is_pipeline_freemem(struct fimc_is_pipeline *pipeline)
{
	struct fimc_is_meminfo *minfo = pipeline->minfo;
	if (minfo->fw.vaddr)
		dma_free_coherent(pipeline->dev, minfo->fw.size,
				minfo->fw.vaddr,
				minfo->fw.paddr);
	if (minfo->shot.vaddr)
		dma_free_coherent(pipeline->dev, minfo->shot.size,
				minfo->shot.vaddr,
				minfo->shot.paddr);
}

static int fimc_is_pipeline_load_firmware(struct fimc_is_pipeline *pipeline)
{
	const struct firmware *fw_blob;
	struct fimc_is *is = pipeline->is;
	int ret;

	ret = request_firmware(&fw_blob, is->drvdata->fw_name, &is->pdev->dev);
	if (ret) {
		dev_err(pipeline->dev, "Firmware file not found\n");
		return -EINVAL;
	}
        if (fw_blob->size > pipeline->minfo->fw.size) {
		dev_err(pipeline->dev, "Firmware file too big\n");
		release_firmware(fw_blob);
		return -ENOMEM;
	}

	memcpy(pipeline->minfo->fw.vaddr, fw_blob->data, fw_blob->size);
	wmb();
	release_firmware(fw_blob);

	return 0;
}

static void fimc_is_pipeline_forcedown(struct fimc_is_pipeline *pipeline,
		bool on)
{
	struct fimc_is *is = pipeline->is;
	if (on) {
		pmu_is_write(0x0, is, PMUREG_ISP_ARM_OPTION);
		pmu_is_write(0x1cf82000, is, PMUREG_ISP_LOW_POWER_OFF);
		pipeline->force_down = true;
	} else {
		pmu_is_write(0xffffffff, is, PMUREG_ISP_ARM_OPTION);
		pmu_is_write(0x8, is, PMUREG_ISP_LOW_POWER_OFF);
		pipeline->force_down = false;
	}
}

static int fimc_is_pipeline_wait_timeout(struct fimc_is_pipeline *pipeline,
					int on)
{
	struct fimc_is *is = pipeline->is;
	u32 timeout = FIMC_IS_A5_TIMEOUT;

	while ((pmu_is_read(is, PMUREG_ISP_ARM_STATUS) & 0x1) != on) {
		if (timeout == 0)
			return -ETIME;
		timeout--;
		udelay(1);
	}
	return 0;
}

static inline void fimc_is_pipeline_reset_isp(struct fimc_is_pipeline *pipeline)
{
        struct fimc_is *is = pipeline->is;

        if (is->drvdata->variant == FIMC_IS_EXYNOS3250)
                pmu_is_write(0x0, is, EXYNOS3250_PMUREG_CMU_RESET_ISP);
        else if (is->drvdata->variant == FIMC_IS_EXYNOS5)
                pmu_is_write(0x0, is, EXYNOS5_PMUREG_CMU_RESET_ISP);
}

static inline int fimc_is_pipeline_setup_gic(void)
{
        int i;
        unsigned int reg_val;
        int ret = 0;

        call_firmware_op(writesfr, (PA_FIMC_IS_GIC_C + 0x4),0x000000ff);
        for (i = 0; i < 3; i++)
                call_firmware_op(writesfr,
                                 (PA_FIMC_IS_GIC_D + 0x80 + (i * 4)),
                                 0xffffffff);
        for (i = 0; i < 18; i++)
                call_firmware_op(writesfr,
                                 (PA_FIMC_IS_GIC_D + 0x400 + (i * 4)),
                                 0x10101010);
        ret = call_firmware_op(readsfr,
                               (PA_FIMC_IS_GIC_C + 0x4),
                                &reg_val);
        if (ret || reg_val != 0xfc) {
                pr_err("Failed to setup IS GIC\n");
                ret = -EINVAL;
        }
        return ret;
}

static int fimc_is_pipeline_power(struct fimc_is_pipeline *pipeline, int on)
{
	int ret = 0;
	struct fimc_is *is = pipeline->is;
	struct device *dev = &is->pdev->dev;

	if (on) {
		/* force poweroff setting */
		if (pipeline->force_down)
			fimc_is_pipeline_forcedown(pipeline, false);

                if (is->drvdata->variant == FIMC_IS_EXYNOS3250)
                        ret = fimc_is_pipeline_setup_gic();
                if (ret < 0)
                        return ret;

		/* FIMC-IS local power enable */
		ret = pm_runtime_get_sync(dev);
                if (ret < 0)
			return ret;

		/* A5 start address setting */
		writel(pipeline->minfo->fw.paddr, is->interface.regs + BBOAR);

		/* A5 power on*/
		pmu_is_write(0x1, is, PMUREG_ISP_ARM_CONFIGURATION);

		/* enable A5 */
		pmu_is_write(0x00018000, is, PMUREG_ISP_ARM_OPTION);
		ret = fimc_is_pipeline_wait_timeout(pipeline, on);
		if (ret)
			dev_err(pipeline->dev, "A5 power on failed\n");
                else
                        set_bit(PIPELINE_POWER, &pipeline->state);

	} else {
		/* disable A5 */
                if( !test_bit(PIPELINE_RESET, &pipeline->state))
                        pmu_is_write(0x10000, is, PMUREG_ISP_ARM_OPTION);
                else
                        pmu_is_write(0x00000, is, PMUREG_ISP_ARM_OPTION);

		/* A5 power off*/
                pmu_is_write(0x0, is, PMUREG_ISP_ARM_OPTION);
		pmu_is_write(0x0, is, PMUREG_ISP_ARM_CONFIGURATION);

		/* Check A5 power off status register */
		ret = fimc_is_pipeline_wait_timeout(pipeline, on);
		if (ret) {
			dev_err(pipeline->dev, "A5 power off failed\n");
			fimc_is_pipeline_forcedown(pipeline, true);
		}

                fimc_is_pipeline_reset_isp(pipeline);

		/* FIMC-IS local power down */
		pm_runtime_put_sync(dev);
                clear_bit(PIPELINE_POWER, &pipeline->state);
	}

	return ret;
}

static int fimc_is_pipeline_load_setfile(struct fimc_is_pipeline *pipeline,
		unsigned int setfile_addr,
		unsigned char *setfile_name)
{
	struct fimc_is *is = pipeline->is;
	const struct firmware *fw_blob;
	int ret;

	ret = request_firmware(&fw_blob, setfile_name, &is->pdev->dev);
	if (ret) {
		dev_err(pipeline->dev, "Setfile %s not found\n", setfile_name);
		return ret;
	}

	memcpy(pipeline->minfo->fw.vaddr + setfile_addr,
			fw_blob->data, fw_blob->size);
	wmb();
	release_firmware(fw_blob);

	return 0;
}

static int fimc_is_pipeline_setfile(struct fimc_is_pipeline *pipeline)
{
	struct fimc_is *is = pipeline->is;
	struct fimc_is_sensor *sensor = pipeline->sensor;
	int ret;
	unsigned int setfile_addr;

	/* Get setfile addr from HW */
	ret = fimc_is_itf_get_setfile_addr(&is->interface,
			pipeline->instance, &setfile_addr);
	if (ret) {
		dev_err(pipeline->dev, "Get setfile addr failed\n");
		return ret;
	}

	if (!sensor->drvdata->setfile_name)
		return -EINVAL;

	/* Load setfile */
	ret = fimc_is_pipeline_load_setfile(pipeline, setfile_addr,
			sensor->drvdata->setfile_name);
	if (ret) {
		dev_err(pipeline->dev, "Load setfile failed\n");
		return ret;
	}

	/* Send HW command */
	ret = fimc_is_itf_load_setfile(&is->interface, pipeline->instance);
	if (ret) {
		dev_err(pipeline->dev, "HW Load setfile failed\n");
		return ret;
	}

	return 0;
}


static void fimc_is_pipeline_isp_working_area(struct fimc_is_pipeline *pipeline,
                                              struct v4l2_rect *crop_rect)
{
        unsigned int sensor_width, sensor_height, isp_width, isp_height;
	unsigned int sensor_ratio, output_ratio;
        unsigned int margin_width, margin_height;
                /* Crop calculation */
        sensor_width    = pipeline->sensor->width;
        sensor_height   = pipeline->sensor->height;
        isp_width 	= pipeline->isp.width;
        isp_height	= pipeline->isp.height;

        crop_rect->width  = sensor_width;
        crop_rect->height = sensor_height;

	sensor_ratio = sensor_width * 1000 / sensor_height;
        output_ratio = isp_width * 1000 / isp_height;

        crop_rect->top = ((pipeline->sensor->height - sensor_height) >> 1) & ~1U;
        crop_rect->left = ((pipeline->sensor->width - sensor_width) >> 1) & ~1U;

        if (sensor_ratio < output_ratio) {
                crop_rect->height = (sensor_width * isp_height) / isp_width;
                crop_rect->height = ALIGN(crop_rect->height, 2);
                crop_rect->top    = ((sensor_height - crop_rect->height) >> 1) & ~1U;
        } else  if (sensor_ratio > output_ratio) {
                crop_rect->width = (sensor_height * isp_width) / isp_height;
                crop_rect->width = ALIGN(crop_rect->width, 4);
                crop_rect->left  =  ((sensor_width - crop_rect->width) >> 1) & ~1U;
	}

        margin_width = sensor_width - isp_width;
        margin_height = sensor_height - isp_height;
        crop_rect->width -= (margin_width <  SENSOR_WIDTH_PADDING)
                          ? SENSOR_WIDTH_PADDING
                          : margin_width;
        crop_rect->height -= (margin_height < SENSOR_HEIGHT_PADDING)
                           ? SENSOR_HEIGHT_PADDING
                           : margin_height;


}

static int fimc_is_pipeline_3aa_setparams(struct fimc_is_pipeline *pipeline,
                                          struct v4l2_rect* crop_reg,
                                          unsigned int enable)
{
        struct taa_param  *params      = &pipeline->is_region->parameter.taa;
        /* Set the control over 3AA / ISP */
        /* Control */
        params->control.cmd      = CONTROL_COMMAND_START;
        params->control.bypass   = CONTROL_BYPASS_DISABLE;
        params->control.run_mode = 1;
        __set_bit(PARAM_3AA_CONTROL, pipeline->config.params);
        /* OTF input */
        params->otf_input.cmd    = OTF_INPUT_COMMAND_DISABLE;
        params->otf_input.width  = pipeline->sensor->width;
        params->otf_input.height = pipeline->sensor->height;

        switch (pipeline->isp.fmt->fourcc) {
        case V4L2_PIX_FMT_SGRBG8:
                params->otf_input.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT;
                params->dma_input.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT;
                break;
        case V4L2_PIX_FMT_SGRBG10:
                params->otf_input.bitwidth = OTF_INPUT_BIT_WIDTH_10BIT;
                params->dma_input.bitwidth = DMA_INPUT_BIT_WIDTH_10BIT;
                break;
        default:
                params->otf_input.bitwidth = OTF_INPUT_BIT_WIDTH_12BIT;
                params->dma_input.bitwidth = DMA_INPUT_BIT_WIDTH_12BIT;
                break;
        }

        params->otf_input.format   = OTF_INPUT_FORMAT_BAYER;
        params->otf_input.order    = OTF_INPUT_ORDER_BAYER_GR_BG;

        FIMC_IS_FW_CALL_OP(tweak_param, PARAM_OTF_INPUT,
                           &params->otf_input, crop_reg);

        __set_bit(PARAM_3AA_OTF_INPUT, pipeline->config.params);

        /* DMA input */
        params->dma_input.cmd = DMA_INPUT_COMMAND_BUF_MNGR;
        params->dma_input.width  = pipeline->sensor->width;
        params->dma_input.height = pipeline->sensor->height;
        params->dma_input.format   = DMA_INPUT_FORMAT_BAYER;
        params->dma_input.order    = DMA_INPUT_ORDER_GR_BG;
        params->dma_input.plane    = 1;
        params->dma_input.buffer_number  = 0;
        params->dma_input.buffer_address = 0;

        FIMC_IS_FW_CALL_OP(tweak_param, PARAM_DMA_INPUT,
                           &params->dma_input, crop_reg);

        __set_bit(PARAM_3AA_VDMA1_INPUT, pipeline->config.params);

        /* DMA output - post BDS output */
        params->dma_bds_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
        params->dma_bds_output.width  = crop_reg->width;
        params->dma_bds_output.height = crop_reg->height;
        params->dma_bds_output.buffer_number  = 0;
        params->dma_bds_output.buffer_address = 0;
        params->dma_bds_output.dma_out_mask   = 0;
        params->dma_bds_output.bitwidth        = DMA_OUTPUT_BIT_WIDTH_12BIT;
        params->dma_bds_output.notify_dma_done = 1;
        params->dma_bds_output.format	    = DMA_INPUT_FORMAT_BAYER;
        __set_bit(PARAM_3AA_VDMA2_OUTPUT, pipeline->config.params);

        return 0;
}

static int fimc_is_pipeline_isp_setparams(struct fimc_is_pipeline *pipeline,
                struct v4l2_rect* crop_reg, unsigned int enable)
{
        struct isp_param *params = &pipeline->is_region->parameter.isp;

        pipeline->isp_width  = crop_reg->width;
        pipeline->isp_height = crop_reg->height;

        params->control.cmd = 1;
        params->control.run_mode = 1;

        /* OTF OUTPUT */
        params->otf_output.cmd      = OTF_OUTPUT_COMMAND_ENABLE;
        params->otf_output.width    = crop_reg->width;
        params->otf_output.height   = crop_reg->height;

        params->otf_output.format= OTF_OUTPUT_FORMAT_YUV422;
        params->otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_8BIT;
        params->otf_output.order = OTF_OUTPUT_ORDER_BAYER_GR_BG;
        params->dma_output.format = DMA_OUTPUT_FORMAT_YUV422;
        params->dma_output.bitwidth = DMA_OUTPUT_BIT_WIDTH_8BIT;
        params->dma_output.order = DMA_OUTPUT_ORDER_CRYCBY;
        __set_bit(PARAM_ISP_OTF_OUTPUT, pipeline->config.params);
        /* DMA OUTPUT */
        params->__dma_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
        __set_bit(PARAM_ISP_DMA1_OUTPUT, pipeline->config.params);
        params->dma_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
        __set_bit(PARAM_ISP_DMA2_OUTPUT, pipeline->config.params);

        /* Set dma output  - 3AA capture */
        params->dma_output.cmd = 0; //FIMC_IS_PARAM_ENABLE;
        params->dma_output.width  = crop_reg->width;
        params->dma_output.height = crop_reg->height;
        params->dma_output.buffer_number  = 0;
        params->dma_output.buffer_address = 0;
        params->dma_output.dma_out_mask   = 0;
        params->dma_output.plane	   = 1;
        params->dma_output.bitwidth = DMA_OUTPUT_BIT_WIDTH_12BIT;
        params->dma_output.format   = DMA_OUTPUT_FORMAT_YUV422;
        params->dma_output.order    = DMA_OUTPUT_ORDER_CRYCBY;
        params->dma_output.reserved[0] = 2;
        params->dma_output.notify_dma_done = DMA_OUTPUT_NOTIFY_DMA_DONE_ENBABLE;
        __set_bit(PARAM_ISP_DMA2_OUTPUT, pipeline->config.params);

	wmb();

        set_bit(COMP_ENABLE, &pipeline->subip_state[IS_ISP]);

	return 0;
}

static int fimc_is_pipeline_drc_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct drc_param *drc_param = &pipeline->is_region->parameter.drc;

	if (enable)
		drc_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		drc_param->control.bypass = CONTROL_BYPASS_ENABLE;
        __set_bit(PARAM_DRC_CONTROL, pipeline->config.params);

	drc_param->otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
	drc_param->otf_input.width = pipeline->isp_width;
	drc_param->otf_input.height = pipeline->isp_height;
        __set_bit(PARAM_DRC_OTF_INPUT, pipeline->config.params);

	drc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	drc_param->otf_output.width = pipeline->isp_width;
	drc_param->otf_output.height = pipeline->isp_height;
        __set_bit(PARAM_DRC_OTF_OUTPUT, pipeline->config.params);

	wmb();

	if (enable)
                set_bit(COMP_ENABLE, &pipeline->subip_state[IS_DRC]);
	else
                clear_bit(COMP_ENABLE, &pipeline->subip_state[IS_DRC]);

	return 0;
}

static int fimc_is_pipeline_scc_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct scalerc_param *scc_param =
		&pipeline->is_region->parameter.scalerc;
	unsigned int scc_width, scc_height;

        scc_width  = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		scc_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		scc_param->control.bypass = CONTROL_BYPASS_ENABLE;
        __set_bit(PARAM_SCALERC_CONTROL, pipeline->config.params);

	scc_param->otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
        scc_param->otf_input.width  = pipeline->isp_width;
	scc_param->otf_input.height = pipeline->isp_height;
        __set_bit(PARAM_SCALERC_OTF_INPUT, pipeline->config.params);
	/* SCC OUTPUT */
	scc_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_param->input_crop.pos_x = 0;
	scc_param->input_crop.pos_y = 0;
        scc_param->input_crop.crop_width  = pipeline->isp_width;
	scc_param->input_crop.crop_height = pipeline->isp_height;
        scc_param->input_crop.in_width   = pipeline->isp_width;
        scc_param->input_crop.in_height  = pipeline->isp_height;
        scc_param->input_crop.out_width  = scc_width;
	scc_param->input_crop.out_height = scc_height;
        __set_bit(PARAM_SCALERC_INPUT_CROP, pipeline->config.params);

	scc_param->output_crop.cmd = SCALER_CROP_COMMAND_DISABLE;
	scc_param->output_crop.pos_x = 0;
	scc_param->output_crop.pos_y = 0;
	scc_param->output_crop.crop_width = scc_width;
	scc_param->output_crop.crop_height = scc_height;
        __set_bit(PARAM_SCALERC_OUTPUT_CROP, pipeline->config.params);

	scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	scc_param->otf_output.width = scc_width;
	scc_param->otf_output.height = scc_height;
        __set_bit(PARAM_SCALERC_OTF_OUTPUT, pipeline->config.params);

        scc_param->dma_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
        scc_param->dma_output.dma_out_mask = 0;
        scc_param->dma_output.buffer_number = 0;
        scc_param->dma_output.buffer_address = 0;
        scc_param->dma_output.plane = 0;
        scc_param->dma_output.width = scc_width;
        scc_param->dma_output.height = scc_height;
         __set_bit(PARAM_SCALERC_DMA_OUTPUT, pipeline->config.params);
        scc_param->effect.cmd = 0;
        scc_param->effect.arbitrary_cb = 128;
        scc_param->effect.arbitrary_cr = 128;
        scc_param->effect.yuv_range = 0;
        __set_bit(PARAM_SCALERC_IMAGE_EFFECT, pipeline->config.params);

	wmb();
        set_bit(COMP_ENABLE, &pipeline->subip_state[IS_SCC]);
	return 0;
}

static int fimc_is_pipeline_odc_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct odc_param *odc_param = &pipeline->is_region->parameter.odc;
	unsigned int scc_width, scc_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		odc_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		odc_param->control.bypass = CONTROL_BYPASS_ENABLE;
        __set_bit(PARAM_ODC_CONTROL, pipeline->config.params);

	odc_param->otf_input.width = scc_width;
	odc_param->otf_input.height = scc_height;
        __set_bit(PARAM_ODC_OTF_INPUT, pipeline->config.params);

	odc_param->otf_output.width = scc_width;
	odc_param->otf_output.height = scc_height;
        __set_bit(PARAM_ODC_OTF_OUTPUT, pipeline->config.params);

	wmb();

	if (enable)
                set_bit(COMP_ENABLE, &pipeline->subip_state[IS_ODC]);
	else
                clear_bit(COMP_ENABLE, &pipeline->subip_state[IS_ODC]);

	return 0;
}

static int fimc_is_pipeline_dis_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct dis_param *dis_param = &pipeline->is_region->parameter.dis;
	unsigned int scc_width, scc_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		dis_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		dis_param->control.bypass = CONTROL_BYPASS_ENABLE;
        __set_bit(PARAM_DIS_CONTROL, pipeline->config.params);

	/* DIS INPUT */
	dis_param->otf_input.width = scc_width;
	dis_param->otf_input.height = scc_height;
        __set_bit(PARAM_DIS_OTF_INPUT, pipeline->config.params);

	/* DIS OUTPUT */
	dis_param->otf_output.width = scc_width;
	dis_param->otf_output.height = scc_height;
        __set_bit(PARAM_DIS_OTF_OUTPUT, pipeline->config.params);

	wmb();

	if (enable)
                set_bit(COMP_ENABLE, &pipeline->subip_state[IS_DIS]);
	else
                clear_bit(COMP_ENABLE, &pipeline->subip_state[IS_DIS]);

	return 0;
}

static int fimc_is_pipeline_3dnr_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct tdnr_param *tdnr_param = &pipeline->is_region->parameter.tdnr;
	unsigned int scc_width, scc_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		tdnr_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		tdnr_param->control.bypass = CONTROL_BYPASS_ENABLE;
        __set_bit(PARAM_TDNR_CONTROL, pipeline->config.params);

	tdnr_param->otf_input.width = scc_width;
	tdnr_param->otf_input.height = scc_height;
        __set_bit(PARAM_TDNR_OTF_INPUT, pipeline->config.params);

	tdnr_param->dma_output.width = scc_width;
	tdnr_param->dma_output.height = scc_height;
        __set_bit(PARAM_TDNR_DMA_OUTPUT, pipeline->config.params);

	tdnr_param->otf_output.width = scc_width;
	tdnr_param->otf_output.height = scc_height;
        __set_bit(PARAM_TDNR_OTF_OUTPUT, pipeline->config.params);

	wmb();

	if (enable)
                set_bit(COMP_ENABLE, &pipeline->subip_state[IS_TDNR]);
	else
                clear_bit(COMP_ENABLE, &pipeline->subip_state[IS_TDNR]);

	return 0;
}

static int fimc_is_pipeline_scp_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct scalerp_param *scp_param =
		&pipeline->is_region->parameter.scalerp;

	unsigned int scc_width, scc_height;
	unsigned int scp_width, scp_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;
	scp_width = pipeline->scaler[SCALER_SCP].width;
	scp_height = pipeline->scaler[SCALER_SCP].height;

	if (enable)
		scp_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		scp_param->control.bypass = CONTROL_BYPASS_ENABLE;
        __set_bit(PARAM_SCALERP_CONTROL, pipeline->config.params);

	/* SCP Input */
	scp_param->otf_input.width = scc_width;
	scp_param->otf_input.height = scc_height;
        __set_bit(PARAM_SCALERP_OTF_INPUT, pipeline->config.params);

	/* SCP Output */
	scp_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scp_param->input_crop.pos_x = 0;
	scp_param->input_crop.pos_y = 0;
	scp_param->input_crop.crop_width = scc_width;
	scp_param->input_crop.crop_height = scc_height;
	scp_param->input_crop.in_width = scc_width;
	scp_param->input_crop.in_height = scc_height;
	scp_param->input_crop.out_width = scp_width;
	scp_param->input_crop.out_height = scp_height;
        __set_bit(PARAM_SCALERP_INPUT_CROP, pipeline->config.params);

	scp_param->output_crop.cmd = SCALER_CROP_COMMAND_DISABLE;
        __set_bit(PARAM_SCALERP_OUTPUT_CROP, pipeline->config.params);

	scp_param->otf_output.width = scp_width;
	scp_param->otf_output.height = scp_height;
        __set_bit(PARAM_SCALERP_OTF_OUTPUT, pipeline->config.params);

	wmb();
        set_bit(COMP_ENABLE, &pipeline->subip_state[IS_SCP]);

	return 0;
}

static int fimc_is_pipeline_fd_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct fd_param *fd_param = &pipeline->is_region->parameter.fd;

	if (enable)
		fd_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		fd_param->control.bypass = CONTROL_BYPASS_ENABLE;
        __set_bit(PARAM_FD_CONTROL, pipeline->config.params);

        if (pipeline->subip_state[IS_SCP] != COMP_INVALID) {
                fd_param->otf_input.width = pipeline->scaler[SCALER_SCP].width;
                fd_param->otf_input.height = pipeline->scaler[SCALER_SCP].height;
        } else {
                fd_param->otf_input.width = pipeline->scaler[SCALER_SCC].width;
                fd_param->otf_input.height = pipeline->scaler[SCALER_SCC].height;
        }
        __set_bit(PARAM_FD_OTF_INPUT, pipeline->config.params);

	wmb();

	if (enable)
                set_bit(COMP_ENABLE, &pipeline->subip_state[IS_FD]);
	else
                clear_bit(COMP_ENABLE, &pipeline->subip_state[IS_FD]);

	return 0;
}

void fimc_is_pipeline_buf_lock(struct fimc_is_pipeline *pipeline)
{
	spin_lock_irqsave(&pipeline->slock_buf, pipeline->slock_flags);
}

void fimc_is_pipeline_buf_unlock(struct fimc_is_pipeline *pipeline)
{
	spin_unlock_irqrestore(&pipeline->slock_buf, pipeline->slock_flags);
}

int fimc_is_pipeline_set_params(struct fimc_is_pipeline *pipeline)
{
	int ret;
        struct v4l2_rect crop_rect = {0,};

        fimc_is_pipeline_isp_working_area(pipeline, &crop_rect);
	/* Enabling basic components in pipeline */
        ret = fimc_is_pipeline_3aa_setparams(pipeline, &crop_rect, true);
        if (!ret)
                ret = fimc_is_pipeline_isp_setparams(pipeline, &crop_rect, true);
	if (!ret)
		ret = fimc_is_pipeline_drc_setparams(pipeline, false);
	if (!ret)
		ret = fimc_is_pipeline_scc_setparams(pipeline, true);
        if (!ret && pipeline->subip_state[IS_ODC] != COMP_INVALID)
		ret = fimc_is_pipeline_odc_setparams(pipeline, false);
        if (!ret && pipeline->subip_state[IS_DIS] != COMP_INVALID)
		ret = fimc_is_pipeline_dis_setparams(pipeline, false);
        if (!ret && pipeline->subip_state[IS_TDNR] != COMP_INVALID)
		ret = fimc_is_pipeline_3dnr_setparams(pipeline, false);
        if (!ret && pipeline->subip_state[IS_SCP] != COMP_INVALID)
		ret = fimc_is_pipeline_scp_setparams(pipeline, true);
	if (!ret)
		ret = fimc_is_pipeline_fd_setparams(pipeline, false);
	if (ret)
		dev_err(pipeline->dev, "Pipeline setparam failed\n");

	return ret;
}


static void fimc_is_pipeline_scalerc_validate(struct fimc_is_pipeline* pipeline,
                                              struct v4l2_rect *scc_reg)
{
        struct v4l2_rect scc_input  = {0,};
        struct v4l2_rect scc_output = {0,};
        struct fimc_is_scaler *scc = &pipeline->scaler[SCALER_SCC];
        struct scalerc_param *scc_params = &pipeline->is_region->parameter.scalerc;
        const struct fimc_is_fmt *fmt = scc->fmt;

        scc_input.width  = scc_params->otf_input.width;
        scc_input.height = scc_params->otf_input.height;

        scc_output.width  = scc->width;
        scc_output.height = scc->height;

        if (scc_input.width * FIMC_SCC_MAX_RESIZE_RATIO < scc_output.width) {
                scc_reg->width = scc_input.width * FIMC_SCC_MAX_RESIZE_RATIO;
                v4l2_warn(&scc->subdev,
                        "Cannot scale up beyond max scale ratio [x4]\n");
        }
        if (scc_input.height * FIMC_SCC_MAX_RESIZE_RATIO < scc_output.height) {
                scc_reg->height = scc_input.height * FIMC_SCC_MAX_RESIZE_RATIO;
                v4l2_warn(&scc->subdev,
                        "Cannot scale up beyond max scale ratio [x4]\n");
        }
        if (scc_input.width > scc_output.width * FIMC_SCC_MIN_RESIZE_RATIO) {
                scc_reg->width = scc_input.width / FIMC_SCC_MAX_RESIZE_RATIO;
                v4l2_warn(&scc->subdev,
                        "Cannot scale down beyond min scale ratio [1/16]\n");
        }
        if (scc_input.height > scc_output.height * FIMC_SCC_MAX_RESIZE_RATIO) {
                scc_reg->height = scc_input.height / FIMC_SCC_MAX_RESIZE_RATIO;
                v4l2_warn(&scc->subdev,
                        "Cannot scale down beyond min scale ratio [1/16]\n");
        }

        scc_reg->width  -= scc_reg->width & 0x7;
        scc_reg->height -= scc_reg->height & 0x7;

        switch(fmt->fourcc) {
        case V4L2_PIX_FMT_YUV420M:
        case V4L2_PIX_FMT_NV12M:
                scc_reg->width  -= scc_reg->width & 0xf;
                scc_reg->height -= scc_reg->height & 0xf;
        case V4L2_PIX_FMT_NV16:
                break;
        default:
                break;
        }
}

int fimc_is_pipeline_set_scaler_effect(struct fimc_is_pipeline *pipeline,
				   unsigned int rotation)
{
	struct fimc_is *is = pipeline->is;
	struct scalerp_param *scaler_param =
		&pipeline->is_region->parameter.scalerp;
	unsigned long index[2] = {0};
	unsigned int paused = 0;
	int ret = 0;

	if (!test_bit(PIPELINE_OPEN, &pipeline->state)
	|| !test_bit(PIPELINE_PREPARE, &pipeline->state)) {
		dev_err(pipeline->dev, "Pipeline not opened.\n");
		return -EINVAL;
	}

	mutex_lock(&pipeline->pipe_scl_lock);

	if (test_bit(PIPELINE_START, &pipeline->state)) {
		ret = fimc_is_pipeline_stop(pipeline, 0);
		if (ret) {
			dev_err(pipeline->dev, "Not able to stop pipeline\n");
			goto leave;
		}
		paused = 1;
	}

	if (SCALER_ROTATE &rotation ) {
		scaler_param->rotation.cmd = SCALER_ROTATION_CMD_CLOCKWISE90;
		__set_bit(PARAM_SCALERP_ROTATION, index);
	}

	if ((SCALER_FLIP_X | SCALER_FLIP_Y) & rotation)
		 __set_bit(PARAM_SCALERP_FLIP, index);

	if (SCALER_FLIP_X & rotation )
		scaler_param->flip.cmd = SCALER_FLIP_COMMAND_X_MIRROR;
	if (SCALER_FLIP_Y & rotation )
		 scaler_param->flip.cmd |= SCALER_FLIP_COMMAND_Y_MIRROR;

	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
				ISS_PREVIEW_STILL, index[0], index[1]);

	if (paused) {
		ret = fimc_is_pipeline_start(pipeline, 0);
		if (ret)
			dev_err(pipeline->dev,
				"Failed to restart the pipeline.\n");
	}
leave:
	mutex_unlock(&pipeline->pipe_scl_lock);
	return ret;
}

int fimc_is_pipeline_scaler_start(struct fimc_is_pipeline *pipeline,
		enum fimc_is_scaler_id scaler_id,
		unsigned int num_bufs,
		unsigned int num_planes)
{
	struct fimc_is *is = pipeline->is;
	struct scalerp_param *scp_param =
		&pipeline->is_region->parameter.scalerp;
	struct scalerc_param *scc_param =
		&pipeline->is_region->parameter.scalerc;
	struct param_dma_output *dma_output;
        struct param_otf_output *otf_output;
	const struct fimc_is_fmt *fmt;
        struct fimc_is_scaler* scaler;
	unsigned int region_index;
        unsigned long *subip_state;
        struct v4l2_rect scaler_reg = {0,};
        int ret = 0;
	unsigned int pipe_start_flag = 0;
	unsigned int i, buf_mask = 0;
	unsigned long index[2] = {0};

        if (!test_bit(PIPELINE_OPEN, &pipeline->state)
        || !test_bit(PIPELINE_PREPARE, &pipeline->state)) {
		dev_err(pipeline->dev, "Pipeline not opened.\n");
		return -EINVAL;
	}

	mutex_lock(&pipeline->pipe_scl_lock);

	if (test_bit(PIPELINE_START, &pipeline->state)) {
		/*
		 * Pipeline is started.
		 * Stop it now to set params and start again
		 */
		ret = fimc_is_pipeline_stop(pipeline, 0);
		if (ret) {
			dev_err(pipeline->dev, "Not able to stop pipeline\n");
			goto exit;
		}
		pipe_start_flag = 1;
	}

	if (scaler_id == SCALER_SCC) {
                scaler = &pipeline->scaler[SCALER_SCC];
                /* Validate the resize ratio : 1/16 to 4 */
                scaler_reg.width = scaler->width;
                scaler_reg.height = scaler->height;
                fimc_is_pipeline_scalerc_validate(pipeline, &scaler_reg);
                dma_output   = &scc_param->dma_output;
		region_index = FIMC_IS_SCC_REGION_INDEX;
                subip_state  = &pipeline->subip_state[IS_SCC];
		__set_bit(PARAM_SCALERC_DMA_OUTPUT, index);
                otf_output = &scc_param->otf_output;
                scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_DISABLE;
                __set_bit(PARAM_SCALERC_OTF_OUTPUT, index);
	} else {
                scaler = &pipeline->scaler[SCALER_SCP];
                scaler_reg.width = scaler->width;
                scaler_reg.height = scaler->height;
		dma_output = &scp_param->dma_output;
                otf_output = &scp_param->otf_output;
                subip_state = &pipeline->subip_state[IS_SCP];
		region_index = FIMC_IS_SCP_REGION_INDEX;
		__set_bit(PARAM_SCALERP_DMA_OUTPUT, index);
                __set_bit(PARAM_SCALERP_OTF_OUTPUT, index);
	}

	for (i = 0; i < num_bufs; i++)
		buf_mask |= BIT(i);

        fmt = scaler->fmt;
        if (num_planes < fmt->num_planes ) {
                v4l2_warn(&scaler->subdev,
                          "Insufficient number of planes!\n");
                goto exit;
        }

        dma_output->cmd           = DMA_OUTPUT_COMMAND_ENABLE;
        dma_output->dma_out_mask  = buf_mask;
	dma_output->buffer_number = num_bufs;
        dma_output->plane         = fmt->num_planes;
        dma_output->width         = scaler_reg.width;
        dma_output->height        = scaler_reg.height;

        switch (fmt->fourcc){
        case V4L2_PIX_FMT_YUV420M:
        case V4L2_PIX_FMT_NV12M:
		dma_output->format = DMA_OUTPUT_FORMAT_YUV420;
                dma_output->order  = DMA_OUTPUT_ORDER_CRCB;
                break;
        case V4L2_PIX_FMT_NV16:
		dma_output->format = DMA_OUTPUT_FORMAT_YUV422;
                dma_output->order  = DMA_OUTPUT_ORDER_CRYCBY;
        default:
                v4l2_info(&scaler->subdev,
                         "Unsupported format.\n");
                break;
        }

	dma_output->buffer_address = pipeline->minfo->shared.paddr +
				region_index * sizeof(unsigned int);


        if (is->drvdata->variant == FIMC_IS_EXYNOS3250)
                dma_output->reserved[0] = SCALER_DMA_OUT_SCALED;
        else
                dma_output->reserved[0] = SCALER_DMA_OUT_UNSCALED;

	__set_bit(PARAM_SCALERC_DMA_OUTPUT, index);

        otf_output->cmd = OTF_OUTPUT_COMMAND_DISABLE;
        otf_output->width = scaler_reg.width;
        otf_output->height = scaler_reg.height;

	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
                        ISS_PREVIEW_STILL, index[0], index[1]);
	if (ret)
		dev_err(pipeline->dev, "fimc_is_itf_set_param is fail\n");
        else{
                set_bit(COMP_START, subip_state);
        }
	if (pipe_start_flag) {
		ret = fimc_is_pipeline_start(pipeline, 0);
		if (ret)
			dev_err(pipeline->dev,
				"Not able to start pipeline back\n");
	}
exit:
	mutex_unlock(&pipeline->pipe_scl_lock);
	return ret;
}

int fimc_is_pipeline_scaler_stop(struct fimc_is_pipeline *pipeline,
		enum fimc_is_scaler_id scaler_id)
{
	struct fimc_is *is = pipeline->is;
	struct scalerp_param *scp_param =
		&pipeline->is_region->parameter.scalerp;
	struct scalerc_param *scc_param =
		&pipeline->is_region->parameter.scalerc;
	struct param_dma_output *dma_output;
	unsigned int pipe_start_flag = 0;
	unsigned long index[2] = {0};
        unsigned long *subip_state;
	int ret;

	if (!test_bit(PIPELINE_OPEN, &pipeline->state))
		return -EINVAL;

	mutex_lock(&pipeline->pipe_scl_lock);

	if (test_bit(PIPELINE_START, &pipeline->state)) {
		/*
		 * Pipeline is started.
		 * Stop it now to set params and start again
		 */
		ret = fimc_is_pipeline_stop(pipeline, 0);
		if (ret) {
			dev_err(pipeline->dev, "Not able to stop pipeline\n");
			goto exit;
		}
		pipe_start_flag = 1;
	}

        subip_state = (scaler_id == SCALER_SCC) ?
                &pipeline->subip_state[IS_SCC] : &pipeline->subip_state[IS_SCP];

        if (!test_bit(COMP_START, subip_state)) {
		ret = 0;
		goto exit;
	}

	if (scaler_id == SCALER_SCC) {
		dma_output = &scc_param->dma_output;
		__set_bit(PARAM_SCALERC_DMA_OUTPUT, index);
	} else {
		dma_output = &scp_param->dma_output;
		__set_bit(PARAM_SCALERP_DMA_OUTPUT, index);
	}
	dma_output->cmd = DMA_OUTPUT_COMMAND_DISABLE;

	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
                        ISS_PREVIEW_STILL, index[0], index[1]);
	if (ret)
		dev_err(pipeline->dev, "fimc_is_itf_set_param is fail\n");
	else
                clear_bit(COMP_START, subip_state);

	if (pipe_start_flag) {
		ret = fimc_is_pipeline_start(pipeline, 0);
		if (ret)
			dev_err(pipeline->dev,
				"Not able to start pipeline back\n");
	}
exit:
	mutex_unlock(&pipeline->pipe_scl_lock);
	return ret;
}

void fimc_is_pipeline_config_shot(struct fimc_is_pipeline *pipeline)
{
        FIMC_IS_FW_CALL_OP(config_shot, pipeline->minfo->shot.vaddr);
}

int fimc_is_pipeline_update_shot_ctrl(struct fimc_is_pipeline *pipeline,
                                     unsigned int config_id,
                                     unsigned long val)
{
       /* @TODO: Exynos5 support (ISP configuration)*/
       if (!test_bit(PIPELINE_OPEN, &pipeline->state))
	       return -EAGAIN;

       return FIMC_IS_FW_CALL_OP(set_control,
                                 pipeline->minfo->shot.vaddr,
                                 config_id, val);
}

int fimc_is_pipeline_shot_safe(struct fimc_is_pipeline *pipeline)
{
	int ret;
	mutex_lock(&pipeline->pipe_lock);
	ret = fimc_is_pipeline_shot(pipeline);
	mutex_unlock(&pipeline->pipe_lock);
	return ret;
}

static void fimc_is_pipeline_shot_cancel(struct fimc_is_pipeline *pipeline)
{
        struct fimc_is_buf *buffer;
        struct fimc_is_isp *isp = &pipeline->isp;
        struct fimc_is_scaler *scaler;

        fimc_is_pipeline_buf_lock(pipeline);
        /* Release ISP buffers */
        while (!list_empty(&isp->wait_queue)) {
                buffer = fimc_is_isp_wait_queue_get(isp);
		buffer->state = FIMC_IS_BUF_INVALID;
                vb2_buffer_done(&buffer->vb, VB2_BUF_STATE_ERROR);
        }

        if(!list_empty(&isp->run_queue)) {
                while (!list_empty(&isp->run_queue)) {
                        buffer = fimc_is_isp_run_queue_get(isp);
			buffer->state = FIMC_IS_BUF_INVALID;
                        vb2_buffer_done(&buffer->vb, VB2_BUF_STATE_ERROR);
                }
        }
        /* Release scaler buffers */
        scaler = &pipeline->scaler[SCALER_SCC];
        if (test_bit(COMP_START, &pipeline->subip_state[IS_SCC])) {
                while (!list_empty(&scaler->wait_queue)) {
                        buffer = fimc_is_scaler_wait_queue_get(scaler);
			buffer->state = FIMC_IS_BUF_INVALID;
                        vb2_buffer_done(&buffer->vb, VB2_BUF_STATE_ERROR);
                }
                while (!list_empty(&scaler->run_queue)) {
                        buffer = fimc_is_scaler_run_queue_get(scaler);
			buffer->state = FIMC_IS_BUF_INVALID;
                        vb2_buffer_done(&buffer->vb, VB2_BUF_STATE_ERROR);
                }
        }
        scaler = &pipeline->scaler[SCALER_SCP];
        if (test_bit(COMP_START, &pipeline->subip_state[IS_SCP])) {
                while (!list_empty(&scaler->wait_queue)) {
                        buffer = fimc_is_scaler_wait_queue_get(scaler);
			buffer->state = FIMC_IS_BUF_INVALID;
                        vb2_buffer_done(&buffer->vb, VB2_BUF_STATE_ERROR);
                }
                while (!list_empty(&scaler->run_queue)) {
                        buffer = fimc_is_scaler_run_queue_get(scaler);
			buffer->state = FIMC_IS_BUF_INVALID;
                        vb2_buffer_done(&buffer->vb, VB2_BUF_STATE_ERROR);
                }
        }
        fimc_is_pipeline_buf_unlock(pipeline);
}

int fimc_is_pipeline_shot(struct fimc_is_pipeline *pipeline)
{
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned int rcount, i;
        unsigned int *shared_reg = NULL;
	struct camera2_shot *shot = pipeline->minfo->shot.vaddr;
	struct fimc_is_buf *scc_buf, *scp_buf, *bayer_buf;
        unsigned int shot_paddr = pipeline->minfo->shot.paddr;
        struct camera2_scaler_uctl_base *scaler_ud;

        if (!test_bit(PIPELINE_START, &pipeline->state)) {
                ret = fimc_is_itf_process_on(&is->interface, pipeline->instance);
                if (ret)
                        return -EINVAL;
        }

	if (test_bit(PIPELINE_RUN, &pipeline->state))
		return -EBUSY;

        /**
         * Verify if previous requests didn't result in potentiall error.
         * If so - there is no point to call another shots untill
         * the pipeline gets re-configured properly
         */
        if (test_bit(PIPELINE_RESET, &pipeline->state)) {
                dev_err(pipeline->dev, "Failed on previous frame capture request."
                        " Re-configure prior to further attempts.\n");
                fimc_is_pipeline_shot_cancel(pipeline);
                goto err_exit;
        }

	fimc_is_pipeline_buf_lock(pipeline);

	if (list_empty(&pipeline->isp.wait_queue)) {
		/* No more bayer buffers */
		wake_up(&pipeline->wait_q);
		fimc_is_pipeline_buf_unlock(pipeline);
		return 0;
	}

	set_bit(PIPELINE_RUN, &pipeline->state);

	/* Get bayer input buffer */
	bayer_buf = fimc_is_isp_wait_queue_get(&pipeline->isp);
	if (!bayer_buf) {
		fimc_is_pipeline_buf_unlock(pipeline);
		goto err_exit;
	}
	fimc_is_isp_run_queue_add(&pipeline->isp, bayer_buf);

        shared_reg = (unsigned int*)(pipeline->minfo->shared.vaddr);
        memset(shared_reg + FIMC_IS_SCC_REGION_INDEX *sizeof(unsigned int),
                0, sizeof(unsigned int) * FIMC_IS_MAX_PLANES);
        memset(shared_reg + FIMC_IS_SCP_REGION_INDEX *sizeof(unsigned int),
                0, sizeof(unsigned int) * FIMC_IS_MAX_PLANES);

        scaler_ud = pipeline->config.metadata_mode == CAMERA2_SHOT_BASE_MODE
                  ? &((struct camera2_shot_base*)shot)->uctl.scaler_ud
                  : &shot->uctl.scaler_ud.base;

	/* Get SCC buffer */
        if (test_bit(COMP_START, &pipeline->subip_state[IS_SCC]) &&
		!list_empty(&pipeline->scaler[SCALER_SCC].wait_queue)) {
		scc_buf = fimc_is_scaler_wait_queue_get(
				&pipeline->scaler[SCALER_SCC]);
		if (scc_buf) {
			fimc_is_scaler_run_queue_add(
					&pipeline->scaler[SCALER_SCC],
					scc_buf);
                        if (scc_buf->vb.num_planes > FIMC_IS_MAX_PLANES)
                                dev_dbg(pipeline->dev, "Invalid number of planes!");
                        shared_reg = (unsigned int*)(pipeline->minfo->shared.vaddr
                                     + FIMC_IS_SCC_REGION_INDEX * sizeof(unsigned int));
                        for (i = 0; i < scc_buf->vb.num_planes; i++) {
                                scaler_ud->scc_target_address[i] =
							scc_buf->paddr[i];
                                shared_reg[i] = scc_buf->paddr[i];
                        }
                        set_bit(COMP_RUN, &pipeline->subip_state[IS_SCC]);
		}
	} else {
                dev_err(pipeline->dev, "No SCC buffer available\n");
		for (i = 0; i < 3; i++)
                        scaler_ud->scc_target_address[i] = 0;
	}

	/* Get SCP buffer */
        if (test_bit(COMP_START, &pipeline->subip_state[IS_SCP]) &&
		!list_empty(&pipeline->scaler[SCALER_SCP].wait_queue)) {
		scp_buf = fimc_is_scaler_wait_queue_get(
				&pipeline->scaler[SCALER_SCP]);
		if (scp_buf) {
			fimc_is_scaler_run_queue_add(
					&pipeline->scaler[SCALER_SCP],
					scp_buf);
                        if (scp_buf->vb.num_planes > FIMC_IS_MAX_PLANES)
                                 dev_dbg(pipeline->dev,
					"Invalid number of planes!");
                        shared_reg = (unsigned int*)(pipeline->minfo->shared.vaddr
                                + FIMC_IS_SCP_REGION_INDEX * sizeof(unsigned int));
                        for (i = 0; i < FIMC_IS_MAX_PLANES; i++){
                                scaler_ud->scp_target_address[i] =
							scp_buf->paddr[i];
                                shared_reg[i] = scp_buf->paddr[i];
                        }
                        set_bit(COMP_RUN, &pipeline->subip_state[IS_SCP]);
		}
	} else {
		dev_dbg(pipeline->dev, "No SCP buffer available\n");
		for (i = 0; i < 3; i++)
                        scaler_ud->scp_target_address[i] = 0;
	}
	fimc_is_pipeline_buf_unlock(pipeline);
	/* Send shot command */
	pipeline->fcount++;
	rcount = pipeline->fcount;
	dev_dbg(pipeline->dev,
		"Shot command fcount : %d, Bayer addr : 0x%08x\n",
		pipeline->fcount, bayer_buf->paddr[0]);
	ret = fimc_is_itf_shot_nblk(&is->interface, pipeline->instance,
			bayer_buf->paddr[0],
                        shot_paddr,
			pipeline->fcount,
			rcount);
	if (ret) {
		dev_err(pipeline->dev, "Shot command failed\n");
		goto err_exit;
	}
        queue_delayed_work(fimc_is_pipeline_workqueue, &pipeline->wdg_work,
                           nsecs_to_jiffies(pipeline->frame_duration)*2);
	return 0;

err_exit:
	clear_bit(PIPELINE_RUN, &pipeline->state);
        clear_bit(COMP_RUN, &pipeline->subip_state[IS_SCC]);
        clear_bit(COMP_RUN, &pipeline->subip_state[IS_SCP]);
	return -EINVAL;
}


static int fimc_is_pipeline_set_shotmode(struct fimc_is_pipeline *pipeline)
{
        struct global_param *global_shotmode = &pipeline->is_region->parameter.global;
        global_shotmode->shotmode.cmd = 1;
        global_shotmode->shotmode.skip_frames = 4;
        return 0;
}

static int fimc_is_pipeline_prepare(struct fimc_is_pipeline *pipeline)
{
	int ret = 0;
	struct fimc_is *is = pipeline->is;
        int scenario;
	/* Set pipeline component params */
        ret = fimc_is_pipeline_set_params(pipeline);
	if (ret) {
		dev_err(pipeline->dev, "Set params failed\n");
                goto leave;
        }

        fimc_is_pipeline_set_shotmode(pipeline);

        for(scenario = ISS_PREVIEW_STILL; scenario < ISS_END; ++scenario) {
                ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
                                      scenario,
                                      pipeline->config.params[0],
                                      pipeline->config.params[1]);
                if (ret) {
                        fimc_is_pipeline_reset_isp(pipeline);
                        dev_err(pipeline->dev, "Failed to set parameters\n");
                        goto leave;
                }
	}

	/* Send preview still command */
	ret = fimc_is_itf_preview_still(&is->interface, pipeline->instance);
	if (ret) {
		dev_err(pipeline->dev, "Preview still failed\n");
                goto leave;
	}

	/* Confiture shot memory to A5 */
        ret = fimc_is_itf_map_mem(&is->interface, pipeline->instance,
			pipeline->minfo->shot.paddr,
                        FIMC_IS_SHOT_SIZE);
	if (ret) {
		dev_err(pipeline->dev, "Config A5 mem failed\n");
                goto leave;
	}

	/* Set shot params */
	fimc_is_pipeline_config_shot(pipeline);

        set_bit(PIPELINE_PREPARE, &pipeline->state);
leave:
        return ret;
}

int fimc_is_pipeline_start(struct fimc_is_pipeline *pipeline, int streamon)
{
        int ret = 0;
        struct fimc_is *is = pipeline->is;

        /* Check if open or not */
        if (!test_bit(PIPELINE_OPEN, &pipeline->state)) {
                dev_err(pipeline->dev, "Pipeline not open\n");
                return -EINVAL;
        }

        mutex_lock(&pipeline->pipe_lock);

        /* Check if already started */
        if (test_bit(PIPELINE_START, &pipeline->state))
                goto err_exit;

        /* EXYNOS3250: EVT0 does not support sensor configuration : no sensor mode change ? */
        if (!test_bit(PIPELINE_PREPARE, &pipeline->state))
                ret = fimc_is_pipeline_prepare(pipeline);
        if (ret) {
                dev_err(pipeline->dev,
                        "Failed to initialize IS pipeleine\n");
                goto err_exit;
        }

	/* Process ON command */
	ret = fimc_is_itf_process_on(&is->interface, pipeline->instance);
	if (ret) {
		dev_err(pipeline->dev, "Process on failed\n");
		goto err_exit;
	}

	/* Stream ON */
	if (streamon) {
		ret = fimc_is_itf_stream_on(&is->interface, pipeline->instance);
		if (ret) {
			dev_err(pipeline->dev, "Stream On failed\n");
			goto err_exit;
		}
	}

	/* Set state to START */
	set_bit(PIPELINE_START, &pipeline->state);

	mutex_unlock(&pipeline->pipe_lock);
	return 0;

err_exit:
	mutex_unlock(&pipeline->pipe_lock);
	return ret;
}

int fimc_is_pipeline_stop_fast_path(struct fimc_is_pipeline *pipeline)
{
        unsigned long timeout = FIMC_IS_COMMAND_TIMEOUT/2;

        mutex_lock(&pipeline->pipe_lock);
        /**
         * Give a chance to perform the actual stop
         * but ignore potential errors
         * as IS might not be responsive at all.
         */
        wait_event_timeout(pipeline->scaler[SCALER_SCC].event_q,
                           !test_bit(COMP_RUN, &pipeline->subip_state[IS_SCC]),
                           timeout);
        wait_event_timeout(pipeline->scaler[SCALER_SCC].event_q,
                           !test_bit(COMP_RUN, &pipeline->subip_state[IS_SCC]),
                           timeout);
        wait_event_timeout(pipeline->scaler[SCALER_SCP].event_q,
                           !test_bit(COMP_RUN, &pipeline->subip_state[IS_SCP]),
                           timeout);

        clear_bit(PIPELINE_START, &pipeline->state);
        mutex_unlock(&pipeline->pipe_lock);
        return 0;
}

int fimc_is_pipeline_stop(struct fimc_is_pipeline *pipeline, int streamoff)
{
        int ret = 0;
	struct fimc_is *is = pipeline->is;

	mutex_lock(&pipeline->pipe_lock);

	/* Check if started */
	if (!test_bit(PIPELINE_OPEN, &pipeline->state) ||
            !test_bit(PIPELINE_START, &pipeline->state)) {
		goto err_exit;
	}

        if (test_bit(PIPELINE_RESET, &pipeline->state)){
                mutex_unlock(&pipeline->pipe_lock);
                return fimc_is_pipeline_stop_fast_path(pipeline);
        }


	/* Wait if any operation is in progress */
	ret = wait_event_timeout(pipeline->wait_q,
			!test_bit(PIPELINE_RUN, &pipeline->state),
			FIMC_IS_COMMAND_TIMEOUT);

	if (!ret) {
		dev_err(pipeline->dev, "Pipeline timeout");
		ret = -EBUSY;
		goto err_exit;
	}

	/* Wait for scaler operations if any to complete */
	ret = wait_event_timeout(pipeline->scaler[SCALER_SCC].event_q,
                        !test_bit(COMP_RUN, &pipeline->subip_state[IS_SCC]),
			FIMC_IS_COMMAND_TIMEOUT);
	if (!ret) {
		dev_err(pipeline->dev, "SCC timeout");
		ret = -EBUSY;
		goto err_exit;
	}

	ret = wait_event_timeout(pipeline->scaler[SCALER_SCP].event_q,
                        !test_bit(COMP_RUN, &pipeline->subip_state[IS_SCP]),
			FIMC_IS_COMMAND_TIMEOUT);
	if (!ret) {
		dev_err(pipeline->dev, "SCP timeout");
		ret = -EBUSY;
		goto err_exit;
	}

	if (streamoff) {
		/* Stream OFF */
		ret = fimc_is_itf_stream_off(&is->interface,
					pipeline->instance);
		if (ret) {
			dev_err(pipeline->dev, "Stream Off failed\n");
			ret = -EINVAL;
			goto err_exit;
		}
	}

	/* Process OFF */
	ret = fimc_is_itf_process_off(&is->interface, pipeline->instance);
	if (ret) {
		dev_err(pipeline->dev, "Process off failed\n");
		ret = -EINVAL;
		goto err_exit;
	}

	/* Clear state */
	clear_bit(PIPELINE_START, &pipeline->state);

err_exit:
	mutex_unlock(&pipeline->pipe_lock);
	return ret;
}


static void fimc_is_setup_sensor_open_extended(struct is_region *region,
                                              struct fimc_is_sensor *sensor)
{
        region->shared[FIMC_IS_SENSOR_ID]         = sensor->drvdata->id;
        region->shared[FIMC_IS_SENSOR_I2C_CHAN]   = sensor->i2c_bus;
        region->shared[FIMC_IS_SENSOR_I2C_ADDR]   = sensor->i2c_slave_addr;
        region->shared[FIMC_IS_SESNOR_I2C_SPEED]  = 400000;
        region->shared[FIMC_IS_SENSOR_MIPI_LANES] = 1;
        region->shared[FIMC_IS_SENSOR_I2C_SCLK]   = 24000000;
}

int fimc_is_pipeline_open(struct fimc_is_pipeline *pipeline,
			struct fimc_is_sensor *sensor)
{
	struct fimc_is *is = pipeline->is;
	struct is_region *region;
	int ret;

	if (!sensor)
		return -EINVAL;

	mutex_lock(&pipeline->pipe_lock);

	if (test_bit(PIPELINE_OPEN, &pipeline->state)) {
		dev_err(pipeline->dev, "Pipeline already open\n");
		ret = -EINVAL;
		goto err_exit;
	}

	pipeline->fcount = 0;
        pipeline->last_frame = ktime_set(0,0);
        pipeline->frame_duration = 0;
	pipeline->sensor = sensor;

	if (is->num_pipelines == 0) {
                /* Setup firmware options */
                ret = fimc_is_fw_init_ops(is->drvdata->fw_version);
                if (ret) {
                        dev_err(pipeline->dev,
                                "Failed to load firmware configuration.");
                        goto err_exit;
                }
		/* Init memory */
		ret = fimc_is_pipeline_initmem(pipeline);
		if (ret) {
			dev_err(pipeline->dev, "Pipeline memory init failed\n");
			goto err_exit;
		}

		/* Load firmware */
		ret = fimc_is_pipeline_load_firmware(pipeline);
		if (ret) {
			dev_err(pipeline->dev, "Firmware load failed\n");
			goto err_fw;
		}

		/* Power ON */
		ret = fimc_is_pipeline_power(pipeline, 1);
		if (ret) {
			dev_err(pipeline->dev, "A5 power on failed\n");
			goto err_fw;
		}

		/* Wait for FW Init to complete */
		ret = fimc_is_itf_wait_init_state(&is->interface);
		if (ret) {
			dev_err(pipeline->dev, "FW init failed\n");
			goto err_fw;
		}
	}

	/* Open Sensor */
	region = pipeline->is_region;

        if (is->drvdata->variant == FIMC_IS_EXYNOS5) {
                ret = fimc_is_itf_open_sensor(&is->interface,
                                              pipeline->instance,
                                              sensor->drvdata->id,
                                              sensor->i2c_bus,
                                              pipeline->minfo->shared.paddr);
        } else {

                fimc_is_setup_sensor_open_extended(region, sensor);
                ret = fimc_is_itf_open_sensor_ext(&is->interface,
                                                  pipeline->instance,
                                                  sensor->drvdata->id,
                                                  pipeline->minfo->shared.paddr);
        }

	if (ret) {
		dev_err(pipeline->dev, "Open sensor failed\n");
                fimc_is_pipeline_power(pipeline, 0);
                goto err_fw;
        }

        if (region->shared[MAX_SHARED_COUNT - 1] != MAGIC_NUMBER) {
                dev_err(pipeline->dev, "Invalid magic number\n");
                goto err_fw;
	}

	/* Load setfile */
	ret = fimc_is_pipeline_setfile(pipeline);
	if (ret)
		goto err_exit;

	if (is->num_pipelines == 0) {
		/* Copy init params to FW region */

		region->parameter.sensor = init_sensor_param;
                region->parameter.taa	= init_taa_param;
		region->parameter.isp = init_isp_param;
		region->parameter.drc = init_drc_param;
		region->parameter.scalerc = init_scalerc_param;
		region->parameter.odc = init_odc_param;
		region->parameter.dis = init_dis_param;
		region->parameter.tdnr = init_tdnr_param;
		region->parameter.scalerp = init_scalerp_param;
		region->parameter.fd = init_fd_param;
                FIMC_IS_FW_CALL_OP(tweak_param, PARAM_ISP_CAM,
                                  &region->parameter.taa, NULL);
		wmb();

		/* Set all init params to FW */
		ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
                                ISS_PREVIEW_STILL, FIMC_IS_PARAMS_MASK_ALL, FIMC_IS_PARAMS_MASK_ALL);
		if (ret) {
			dev_err(pipeline->dev, "%s failed\n", __func__);
			return -EINVAL;
		}
	}

        fimc_is_register_listener(is, &pipeline->listener);

	/* Set state to OPEN */
	set_bit(PIPELINE_OPEN, &pipeline->state);
	is->num_pipelines++;

	mutex_unlock(&pipeline->pipe_lock);
	return 0;

err_fw:
	fimc_is_pipeline_freemem(pipeline);
err_exit:
	mutex_unlock(&pipeline->pipe_lock);
	return ret;
}

int fimc_is_pipeline_close(struct fimc_is_pipeline *pipeline)
{
	int ret;
	struct fimc_is *is = pipeline->is;

	mutex_lock(&pipeline->pipe_lock);

	if (!test_bit(PIPELINE_OPEN, &pipeline->state)) {
		dev_err(pipeline->dev, "Pipeline not opened\n");
		ret = -EINVAL;
		goto err_exit;
	}

        if (test_bit(PIPELINE_START, &pipeline->state)
        && !(test_bit(PIPELINE_RESET, &pipeline->state))) {
		dev_err(pipeline->dev, "Cannot close pipeline when its started\n");
		ret = -EINVAL;
		goto err_exit;
	}

	is->num_pipelines--;
	if (is->num_pipelines == 0) {
		/* FW power off command */
		ret = fimc_is_itf_power_down(&is->interface,
					pipeline->instance);
		if (ret)
			dev_err(pipeline->dev, "FW power down error\n");

		/* Pipeline power off*/
		fimc_is_pipeline_power(pipeline, 0);

		/* Free memory */
		fimc_is_pipeline_freemem(pipeline);
	}

        pipeline->config.params[0] = 0;
        pipeline->config.params[1] = 0;
        pipeline->fcount = 0;
        fimc_is_unregister_listener(is, &pipeline->listener);

	clear_bit(PIPELINE_OPEN, &pipeline->state);
        clear_bit(PIPELINE_PREPARE, &pipeline->state);
        clear_bit(PIPELINE_RESET, &pipeline->state);
	mutex_unlock(&pipeline->pipe_lock);
	return 0;

err_exit:
	mutex_unlock(&pipeline->pipe_lock);
	return ret;
}

static void fimc_is_pipeline_wdg_work_fn(struct work_struct *work)
{
        struct fimc_is_pipeline *pipeline = container_of(work,
                         struct fimc_is_pipeline, wdg_work.work);
        if (test_bit(PIPELINE_RUN, &pipeline->state) ){
                /* Check if the IS is responsive */
                struct fimc_is * is = pipeline->is;
                if (fimc_is_itf_hw_running(&is->interface)) {
                        return;
                }
                clear_bit(PIPELINE_RUN,&pipeline->state);
                fimc_is_pipeline_reset(pipeline);
                fimc_is_pipeline_shot_cancel(pipeline);

        }
}

static void fimc_is_pipeline_work_fn(struct work_struct* work)
{
        struct fimc_is_pipeline *pipeline = container_of(work,
                         struct fimc_is_pipeline, work);

        if (test_bit(PIPELINE_RESET, &pipeline->state)) {
                fimc_is_pipeline_stop(pipeline, 1);
        }
}

void fimc_is_pipeline_reset(struct fimc_is_pipeline *pipeline)
{
        cancel_delayed_work(&pipeline->wdg_work);
        if (!test_bit(PIPELINE_RESET, &pipeline->state)) {
                set_bit(PIPELINE_RESET, &pipeline->state);
                queue_work(fimc_is_pipeline_workqueue, &pipeline->work);
        }

}
