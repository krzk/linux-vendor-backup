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
#include <media/videobuf2-dma-contig.h>
#include <linux/delay.h>

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

/* Init params for pipeline devices */
static const struct sensor_param init_sensor_param = {
	.frame_rate = {
		.frame_rate = 30,
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
		.frametime_max = 33333,
	},
	.dma1_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.dma2_input = {
		.cmd = DMA_INPUT_COMMAND_DISABLE,
	},
	.aa = {
		.cmd = ISP_AA_COMMAND_START,
	},
	.flash = {
		.cmd = ISP_FLASH_COMMAND_DISABLE,
		.redeye = ISP_FLASH_REDEYE_DISABLE,
	},
	.awb = {
		.cmd = ISP_AWB_COMMAND_AUTO,
	},
	.effect = {
		.cmd = ISP_IMAGE_EFFECT_DISABLE,
	},
	.iso = {
		.cmd = ISP_ISO_COMMAND_AUTO,
	},
	.adjust = {
		.cmd = ISP_ADJUST_COMMAND_AUTO,
	},
	.metering = {
		.cmd = ISP_METERING_COMMAND_CENTER,
		.win_width = DEFAULT_CAPTURE_STILL_WIDTH,
		.win_height = DEFAULT_CAPTURE_STILL_HEIGHT,
	},
	.afc = {
		.cmd = ISP_AFC_COMMAND_AUTO,
	},
	.otf_output = {
		.cmd = OTF_OUTPUT_COMMAND_ENABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = OTF_OUTPUT_FORMAT_YUV444,
		.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT,
		.order = OTF_OUTPUT_ORDER_BAYER_GR_BG,
	},
	.dma1_output = {
		.cmd = DMA_OUTPUT_COMMAND_DISABLE,
		.width = DEFAULT_CAPTURE_STILL_WIDTH,
		.height = DEFAULT_CAPTURE_STILL_HEIGHT,
		.format = DMA_INPUT_FORMAT_YUV444,
		.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT,
		.plane = 1,
		.order = DMA_INPUT_ORDER_YCBCR,
	},
	.dma2_output = {
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
		.bitwidth = OTF_INPUT_BIT_WIDTH_8BIT,
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

	/* SCP scaler */
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
	fimc_is_scaler_subdev_destroy(&p->scaler[SCALER_SCP]);

	return 0;
}

int fimc_is_pipeline_init(struct fimc_is_pipeline *pipeline,
			unsigned int instance, void *is_ctx)
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
	pipeline->force_down = false;
	for (i = 0; i < FIMC_IS_NUM_COMPS; i++)
		pipeline->comp_state[i] = 0;

	spin_lock_init(&pipeline->slock_buf);
	init_waitqueue_head(&pipeline->wait_q);
	mutex_init(&pipeline->pipe_lock);
	mutex_init(&pipeline->pipe_scl_lock);

	ret = fimc_is_pipeline_create_subdevs(pipeline);
	if (ret) {
		dev_err(pipeline->dev, "Subdev creation failed\n");
		return -EINVAL;
	}

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
	unsigned int offset;

	/* Allocate fw memory */
	minfo->fw.vaddr = dma_alloc_coherent(pipeline->dev,
			FIMC_IS_A5_MEM_SIZE + FIMC_IS_A5_SEN_SIZE,
			&minfo->fw.paddr, GFP_KERNEL);
	if (minfo->fw.vaddr == NULL)
		return -ENOMEM;
	minfo->fw.size = FIMC_IS_A5_MEM_SIZE + FIMC_IS_A5_SEN_SIZE;

	/* FW memory should be 64MB aligned */
	if ((u32)minfo->fw.paddr & FIMC_IS_FW_BASE_MASK) {
		dev_err(pipeline->dev, "FW memory not 64MB aligned\n");
		dma_free_coherent(pipeline->dev, minfo->fw.size,
				minfo->fw.vaddr,
				minfo->fw.paddr);
		return -EIO;
	}

	/* Assigning memory regions */
	offset = FIMC_IS_A5_MEM_SIZE - FIMC_IS_REGION_SIZE;
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
	minfo->shot.vaddr = dma_alloc_coherent(pipeline->dev,
			sizeof(struct camera2_shot),
			&minfo->shot.paddr, GFP_KERNEL);
	if (minfo->shot.vaddr == NULL) {
		dma_free_coherent(pipeline->dev, minfo->fw.size,
				minfo->fw.vaddr,
				minfo->fw.paddr);
		return -ENOMEM;
	}
	minfo->shot.size = sizeof(struct camera2_shot);

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
	if (fw_blob->size > FIMC_IS_A5_MEM_SIZE + FIMC_IS_A5_SEN_SIZE) {
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

static int fimc_is_pipeline_power(struct fimc_is_pipeline *pipeline, int on)
{
	int ret = 0;
	struct fimc_is *is = pipeline->is;
	struct device *dev = &is->pdev->dev;

	if (on) {
		/* force poweroff setting */
		if (pipeline->force_down)
			fimc_is_pipeline_forcedown(pipeline, false);

		/* FIMC-IS local power enable */
		ret = pm_runtime_get_sync(dev);
		if (ret)
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
	} else {
		/* disable A5 */
		pmu_is_write(0x10000, is, PMUREG_ISP_ARM_OPTION);

		/* A5 power off*/
		pmu_is_write(0x0, is, PMUREG_ISP_ARM_CONFIGURATION);

		/* Check A5 power off status register */
		ret = fimc_is_pipeline_wait_timeout(pipeline, on);
		if (ret) {
			dev_err(pipeline->dev, "A5 power off failed\n");
			fimc_is_pipeline_forcedown(pipeline, true);
		}

		pmu_is_write(0x0, is, PMUREG_CMU_RESET_ISP_SYS_PWR_REG);

		/* FIMC-IS local power down */
		pm_runtime_put_sync(dev);
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

static int fimc_is_pipeline_isp_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct isp_param *isp_param = &pipeline->is_region->parameter.isp;
	struct fimc_is *is = pipeline->is;
	unsigned long index[2] = {0};
	unsigned int sensor_width, sensor_height, scc_width, scc_height;
	unsigned int crop_x, crop_y, isp_width, isp_height;
	unsigned int sensor_ratio, output_ratio;
	int ret;

	/* Crop calculation */
	sensor_width = pipeline->sensor->width;
	sensor_height = pipeline->sensor->height;
	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;
	isp_width = sensor_width;
	isp_height = sensor_height;
	crop_x = crop_y = 0;

	sensor_ratio = sensor_width * 1000 / sensor_height;
	output_ratio = scc_width * 1000 / scc_height;

	if (sensor_ratio == output_ratio) {
		isp_width = sensor_width;
		isp_height = sensor_height;
	} else if (sensor_ratio < output_ratio) {
		isp_height = (sensor_width * scc_height) / scc_width;
		isp_height = ALIGN(isp_height, 2);
		crop_y = ((sensor_height - isp_height) >> 1) & ~1U;
	} else {
		isp_width = (sensor_height * scc_width) / scc_height;
		isp_width = ALIGN(isp_width, 4);
		crop_x =  ((sensor_width - isp_width) >> 1) & ~1U;
	}
	pipeline->isp_width = isp_width;
	pipeline->isp_height = isp_height;

	isp_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	isp_param->otf_output.width = pipeline->sensor->width;
	isp_param->otf_output.height = pipeline->sensor->height;
	isp_param->otf_output.format = OTF_OUTPUT_FORMAT_YUV444;
	isp_param->otf_output.bitwidth = OTF_OUTPUT_BIT_WIDTH_12BIT;
	isp_param->otf_output.order = OTF_INPUT_ORDER_BAYER_GR_BG;
	__set_bit(PARAM_ISP_OTF_OUTPUT, index);

	isp_param->dma1_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	__set_bit(PARAM_ISP_DMA1_OUTPUT, index);

	isp_param->dma2_output.cmd = DMA_OUTPUT_COMMAND_DISABLE;
	__set_bit(PARAM_ISP_DMA2_OUTPUT, index);

	if (enable)
		isp_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		isp_param->control.bypass = CONTROL_BYPASS_ENABLE;
	isp_param->control.cmd = CONTROL_COMMAND_START;
	isp_param->control.run_mode = 1;
	__set_bit(PARAM_ISP_CONTROL, index);

	isp_param->dma1_input.cmd = DMA_INPUT_COMMAND_BUF_MNGR;
	isp_param->dma1_input.width = sensor_width;
	isp_param->dma1_input.height = sensor_height;
	isp_param->dma1_input.dma_crop_offset_x = crop_x;
	isp_param->dma1_input.dma_crop_offset_y = crop_y;
	isp_param->dma1_input.dma_crop_width = isp_width;
	isp_param->dma1_input.dma_crop_height = isp_height;
	isp_param->dma1_input.bayer_crop_offset_x = 0;
	isp_param->dma1_input.bayer_crop_offset_y = 0;
	isp_param->dma1_input.bayer_crop_width = 0;
	isp_param->dma1_input.bayer_crop_height = 0;
	isp_param->dma1_input.user_min_frametime = 0;
	isp_param->dma1_input.user_max_frametime = 66666;
	isp_param->dma1_input.wide_frame_gap = 1;
	isp_param->dma1_input.frame_gap = 4096;
	isp_param->dma1_input.line_gap = 45;
	isp_param->dma1_input.order = DMA_INPUT_ORDER_GR_BG;
	isp_param->dma1_input.plane = 1;
	isp_param->dma1_input.buffer_number = 1;
	isp_param->dma1_input.buffer_address = 0;
	isp_param->dma1_input.reserved[1] = 0;
	isp_param->dma1_input.reserved[2] = 0;
	if (pipeline->isp.fmt->fourcc == V4L2_PIX_FMT_SGRBG8)
		isp_param->dma1_input.bitwidth = DMA_INPUT_BIT_WIDTH_8BIT;
	else if (pipeline->isp.fmt->fourcc == V4L2_PIX_FMT_SGRBG10)
		isp_param->dma1_input.bitwidth = DMA_INPUT_BIT_WIDTH_10BIT;
	else
		isp_param->dma1_input.bitwidth = DMA_INPUT_BIT_WIDTH_12BIT;
	__set_bit(PARAM_ISP_DMA1_INPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}

	set_bit(COMP_ENABLE, &pipeline->comp_state[IS_ISP]);

	return 0;
}

static int fimc_is_pipeline_drc_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct drc_param *drc_param = &pipeline->is_region->parameter.drc;
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned long index[2] = {0};

	if (enable)
		drc_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		drc_param->control.bypass = CONTROL_BYPASS_ENABLE;
	__set_bit(PARAM_DRC_CONTROL, index);

	drc_param->otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
	drc_param->otf_input.width = pipeline->isp_width;
	drc_param->otf_input.height = pipeline->isp_height;
	__set_bit(PARAM_DRC_OTF_INPUT, index);

	drc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	drc_param->otf_output.width = pipeline->isp_width;
	drc_param->otf_output.height = pipeline->isp_height;
	__set_bit(PARAM_DRC_OTF_OUTPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}
	if (enable)
		set_bit(COMP_ENABLE, &pipeline->comp_state[IS_DRC]);
	else
		clear_bit(COMP_ENABLE, &pipeline->comp_state[IS_DRC]);

	return 0;
}

static int fimc_is_pipeline_scc_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct scalerc_param *scc_param =
		&pipeline->is_region->parameter.scalerc;
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned long index[2] = {0};
	unsigned int scc_width, scc_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		scc_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		scc_param->control.bypass = CONTROL_BYPASS_ENABLE;
	__set_bit(PARAM_SCALERC_CONTROL, index);

	scc_param->otf_input.cmd = OTF_INPUT_COMMAND_ENABLE;
	scc_param->otf_input.width = pipeline->isp_width;
	scc_param->otf_input.height = pipeline->isp_height;
	__set_bit(PARAM_SCALERC_OTF_INPUT, index);

	/* SCC OUTPUT */
	scc_param->input_crop.cmd = SCALER_CROP_COMMAND_ENABLE;
	scc_param->input_crop.pos_x = 0;
	scc_param->input_crop.pos_y = 0;
	scc_param->input_crop.crop_width = pipeline->isp_width;
	scc_param->input_crop.crop_height = pipeline->isp_height;
	scc_param->input_crop.in_width = pipeline->isp_width;
	scc_param->input_crop.in_height = pipeline->isp_height;
	scc_param->input_crop.out_width = scc_width;
	scc_param->input_crop.out_height = scc_height;
	__set_bit(PARAM_SCALERC_INPUT_CROP, index);

	scc_param->output_crop.cmd = SCALER_CROP_COMMAND_DISABLE;
	scc_param->output_crop.pos_x = 0;
	scc_param->output_crop.pos_y = 0;
	scc_param->output_crop.crop_width = scc_width;
	scc_param->output_crop.crop_height = scc_height;
	__set_bit(PARAM_SCALERC_OUTPUT_CROP, index);

	scc_param->otf_output.cmd = OTF_OUTPUT_COMMAND_ENABLE;
	scc_param->otf_output.width = scc_width;
	scc_param->otf_output.height = scc_height;
	__set_bit(PARAM_SCALERC_OTF_OUTPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}
	set_bit(COMP_ENABLE, &pipeline->comp_state[IS_SCC]);
	return 0;
}

static int fimc_is_pipeline_odc_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct odc_param *odc_param = &pipeline->is_region->parameter.odc;
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned long index[2] = {0};
	unsigned int scc_width, scc_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		odc_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		odc_param->control.bypass = CONTROL_BYPASS_ENABLE;
	__set_bit(PARAM_ODC_CONTROL, index);

	odc_param->otf_input.width = scc_width;
	odc_param->otf_input.height = scc_height;
	__set_bit(PARAM_ODC_OTF_INPUT, index);

	odc_param->otf_output.width = scc_width;
	odc_param->otf_output.height = scc_height;
	__set_bit(PARAM_ODC_OTF_OUTPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}
	if (enable)
		set_bit(COMP_ENABLE, &pipeline->comp_state[IS_ODC]);
	else
		clear_bit(COMP_ENABLE, &pipeline->comp_state[IS_ODC]);

	return 0;
}

static int fimc_is_pipeline_dis_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct dis_param *dis_param = &pipeline->is_region->parameter.dis;
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned long index[2] = {0};
	unsigned int scc_width, scc_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		dis_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		dis_param->control.bypass = CONTROL_BYPASS_ENABLE;
	__set_bit(PARAM_DIS_CONTROL, index);

	/* DIS INPUT */
	dis_param->otf_input.width = scc_width;
	dis_param->otf_input.height = scc_height;
	__set_bit(PARAM_DIS_OTF_INPUT, index);

	/* DIS OUTPUT */
	dis_param->otf_output.width = scc_width;
	dis_param->otf_output.height = scc_height;
	__set_bit(PARAM_DIS_OTF_OUTPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}
	if (enable)
		set_bit(COMP_ENABLE, &pipeline->comp_state[IS_DIS]);
	else
		clear_bit(COMP_ENABLE, &pipeline->comp_state[IS_DIS]);

	return 0;
}

static int fimc_is_pipeline_3dnr_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct tdnr_param *tdnr_param = &pipeline->is_region->parameter.tdnr;
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned long index[2] = {0};
	unsigned int scc_width, scc_height;

	scc_width = pipeline->scaler[SCALER_SCC].width;
	scc_height = pipeline->scaler[SCALER_SCC].height;

	if (enable)
		tdnr_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		tdnr_param->control.bypass = CONTROL_BYPASS_ENABLE;
	__set_bit(PARAM_TDNR_CONTROL, index);

	tdnr_param->otf_input.width = scc_width;
	tdnr_param->otf_input.height = scc_height;
	__set_bit(PARAM_TDNR_OTF_INPUT, index);

	tdnr_param->dma_output.width = scc_width;
	tdnr_param->dma_output.height = scc_height;
	__set_bit(PARAM_TDNR_DMA_OUTPUT, index);

	tdnr_param->otf_output.width = scc_width;
	tdnr_param->otf_output.height = scc_height;
	__set_bit(PARAM_TDNR_OTF_OUTPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}
	if (enable)
		set_bit(COMP_ENABLE, &pipeline->comp_state[IS_TDNR]);
	else
		clear_bit(COMP_ENABLE, &pipeline->comp_state[IS_TDNR]);

	return 0;
}

static int fimc_is_pipeline_scp_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct scalerp_param *scp_param =
		&pipeline->is_region->parameter.scalerp;
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned long index[2] = {0};
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
	__set_bit(PARAM_SCALERP_CONTROL, index);

	/* SCP Input */
	scp_param->otf_input.width = scc_width;
	scp_param->otf_input.height = scc_height;
	__set_bit(PARAM_SCALERP_OTF_INPUT, index);

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
	__set_bit(PARAM_SCALERP_INPUT_CROP, index);

	scp_param->output_crop.cmd = SCALER_CROP_COMMAND_DISABLE;
	__set_bit(PARAM_SCALERP_OUTPUT_CROP, index);

	scp_param->otf_output.width = scp_width;
	scp_param->otf_output.height = scp_height;
	__set_bit(PARAM_SCALERP_OTF_OUTPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}
	set_bit(COMP_ENABLE, &pipeline->comp_state[IS_SCP]);

	return 0;
}

static int fimc_is_pipeline_fd_setparams(struct fimc_is_pipeline *pipeline,
		unsigned int enable)
{
	struct fd_param *fd_param = &pipeline->is_region->parameter.fd;
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned long index[2] = {0};

	if (enable)
		fd_param->control.bypass = CONTROL_BYPASS_DISABLE;
	else
		fd_param->control.bypass = CONTROL_BYPASS_ENABLE;
	__set_bit(PARAM_FD_CONTROL, index);

	fd_param->otf_input.width = pipeline->scaler[SCALER_SCP].width;
	fd_param->otf_input.height = pipeline->scaler[SCALER_SCP].height;
	__set_bit(PARAM_FD_OTF_INPUT, index);

	wmb();
	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret) {
		dev_err(pipeline->dev, "%s failed\n", __func__);
		return -EINVAL;
	}
	if (enable)
		set_bit(COMP_ENABLE, &pipeline->comp_state[IS_FD]);
	else
		clear_bit(COMP_ENABLE, &pipeline->comp_state[IS_FD]);

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

int fimc_is_pipeline_setparams(struct fimc_is_pipeline *pipeline)
{
	int ret;

	/* Enabling basic components in pipeline */
	ret = fimc_is_pipeline_isp_setparams(pipeline, true);
	if (!ret)
		ret = fimc_is_pipeline_drc_setparams(pipeline, false);
	if (!ret)
		ret = fimc_is_pipeline_scc_setparams(pipeline, true);
	if (!ret)
		ret = fimc_is_pipeline_odc_setparams(pipeline, false);
	if (!ret)
		ret = fimc_is_pipeline_dis_setparams(pipeline, false);
	if (!ret)
		ret = fimc_is_pipeline_3dnr_setparams(pipeline, false);
	if (!ret)
		ret = fimc_is_pipeline_scp_setparams(pipeline, true);
	if (!ret)
		ret = fimc_is_pipeline_fd_setparams(pipeline, false);
	if (ret)
		dev_err(pipeline->dev, "Pipeline setparam failed\n");

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
	const struct fimc_is_fmt *fmt;
	unsigned int region_index;
	unsigned long *comp_state;
	int ret;
	unsigned int pipe_start_flag = 0;
	unsigned int i, buf_mask = 0;
	unsigned long index[2] = {0};
	enum fimc_is_scaler_id id;

	if (!test_bit(PIPELINE_OPEN, &pipeline->state)) {
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
		dma_output = &scc_param->dma_output;
		region_index = FIMC_IS_SCC_REGION_INDEX;
		comp_state = &pipeline->comp_state[IS_SCC];
		id = SCALER_SCC;
		__set_bit(PARAM_SCALERC_DMA_OUTPUT, index);
	} else {
		dma_output = &scp_param->dma_output;
		comp_state = &pipeline->comp_state[IS_SCP];
		region_index = FIMC_IS_SCP_REGION_INDEX;
		id = SCALER_SCP;
		__set_bit(PARAM_SCALERP_DMA_OUTPUT, index);
	}

	for (i = 0; i < num_bufs; i++)
		buf_mask |= BIT(i);

	fmt = pipeline->scaler[id].fmt;
	dma_output->cmd = DMA_OUTPUT_COMMAND_ENABLE;
	dma_output->dma_out_mask = buf_mask;
	dma_output->buffer_number = num_bufs;
	dma_output->plane = num_planes;
	dma_output->width = pipeline->scaler[id].width;
	dma_output->height = pipeline->scaler[id].height;
	if ((fmt->fourcc == V4L2_PIX_FMT_YUV420M) ||
			(fmt->fourcc == V4L2_PIX_FMT_NV12M))
		dma_output->format = DMA_OUTPUT_FORMAT_YUV420;
	else if (fmt->fourcc == V4L2_PIX_FMT_NV16)
		dma_output->format = DMA_OUTPUT_FORMAT_YUV422;

	dma_output->buffer_address = pipeline->minfo->shared.paddr +
				region_index * sizeof(unsigned int);
	__set_bit(PARAM_SCALERC_DMA_OUTPUT, index);

	ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
			index[0], index[1]);
	if (ret)
		dev_err(pipeline->dev, "fimc_is_itf_set_param is fail\n");
	else
		set_bit(COMP_START, comp_state);

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
	unsigned long *comp_state;
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

	comp_state = (scaler_id == SCALER_SCC) ?
		&pipeline->comp_state[IS_SCC] : &pipeline->comp_state[IS_SCP];

	if (!test_bit(COMP_START, comp_state)) {
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
			index[0], index[1]);
	if (ret)
		dev_err(pipeline->dev, "fimc_is_itf_set_param is fail\n");
	else
		clear_bit(COMP_START, comp_state);

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
	struct camera2_shot *shot =
		(struct camera2_shot *)pipeline->minfo->shot.vaddr;

	shot->magicnumber = FIMC_IS_MAGIC_NUMBER;

	shot->ctl.aa.mode = AA_CONTROL_AUTO;
	shot->ctl.aa.ae_mode = AA_AEMODE_ON;
}

int fimc_is_pipeline_shot_safe(struct fimc_is_pipeline *pipeline)
{
	int ret;
	mutex_lock(&pipeline->pipe_lock);
	ret = fimc_is_pipeline_shot(pipeline);
	mutex_unlock(&pipeline->pipe_lock);
	return ret;
}

int fimc_is_pipeline_shot(struct fimc_is_pipeline *pipeline)
{
	struct fimc_is *is = pipeline->is;
	int ret;
	unsigned int rcount, i;
	struct camera2_shot *shot = pipeline->minfo->shot.vaddr;
	struct fimc_is_buf *scc_buf, *scp_buf, *bayer_buf;

	if (!test_bit(PIPELINE_START, &pipeline->state))
		return -EINVAL;

	if (test_bit(PIPELINE_RUN, &pipeline->state))
		return -EBUSY;

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

	/* Get SCC buffer */
	if (test_bit(COMP_START, &pipeline->comp_state[IS_SCC]) &&
		!list_empty(&pipeline->scaler[SCALER_SCC].wait_queue)) {
		scc_buf = fimc_is_scaler_wait_queue_get(
				&pipeline->scaler[SCALER_SCC]);
		if (scc_buf) {
			fimc_is_scaler_run_queue_add(
					&pipeline->scaler[SCALER_SCC],
					scc_buf);
			for (i = 0; i < 3; i++)
				shot->uctl.scaler_ud.scc_target_address[i] =
							scc_buf->paddr[i];
			set_bit(COMP_RUN, &pipeline->comp_state[IS_SCC]);
		}
	} else {
		dev_dbg(pipeline->dev, "No SCC buffer available\n");
		for (i = 0; i < 3; i++)
			shot->uctl.scaler_ud.scc_target_address[i] = 0;
	}

	/* Get SCP buffer */
	if (test_bit(COMP_START, &pipeline->comp_state[IS_SCP]) &&
		!list_empty(&pipeline->scaler[SCALER_SCP].wait_queue)) {
		scp_buf = fimc_is_scaler_wait_queue_get(
				&pipeline->scaler[SCALER_SCP]);
		if (scp_buf) {
			fimc_is_scaler_run_queue_add(
					&pipeline->scaler[SCALER_SCP],
					scp_buf);
			for (i = 0; i < 3; i++)
				shot->uctl.scaler_ud.scp_target_address[i] =
							scp_buf->paddr[i];
			set_bit(COMP_RUN, &pipeline->comp_state[IS_SCP]);
		}
	} else {
		dev_dbg(pipeline->dev, "No SCP buffer available\n");
		for (i = 0; i < 3; i++)
			shot->uctl.scaler_ud.scp_target_address[i] = 0;
	}
	fimc_is_pipeline_buf_unlock(pipeline);

	/* Send shot command */
	pipeline->fcount++;
	rcount = pipeline->fcount;
	shot->ctl.request.framecount = pipeline->fcount;
	dev_dbg(pipeline->dev,
		"Shot command fcount : %d, Bayer addr : 0x%08x\n",
		pipeline->fcount, bayer_buf->paddr[0]);
	ret = fimc_is_itf_shot_nblk(&is->interface, pipeline->instance,
			bayer_buf->paddr[0],
			pipeline->minfo->shot.paddr,
			pipeline->fcount,
			rcount);
	if (ret) {
		dev_err(pipeline->dev, "Shot command failed\n");
		goto err_exit;
	}
	return 0;

err_exit:
	clear_bit(PIPELINE_RUN, &pipeline->state);
	clear_bit(COMP_RUN, &pipeline->comp_state[IS_SCC]);
	clear_bit(COMP_RUN, &pipeline->comp_state[IS_SCP]);
	return -EINVAL;
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

	/* Set pipeline component params */
	ret = fimc_is_pipeline_setparams(pipeline);
	if (ret) {
		dev_err(pipeline->dev, "Set params failed\n");
		goto err_exit;
	}

	/* Send preview still command */
	ret = fimc_is_itf_preview_still(&is->interface, pipeline->instance);
	if (ret) {
		dev_err(pipeline->dev, "Preview still failed\n");
		goto err_exit;
	}

	/* Confiture shot memory to A5 */
	ret = fimc_is_itf_cfg_mem(&is->interface, pipeline->instance,
			pipeline->minfo->shot.paddr,
			sizeof(struct camera2_shot));
	if (ret) {
		dev_err(pipeline->dev, "Config A5 mem failed\n");
		goto err_exit;
	}

	/* Set shot params */
	fimc_is_pipeline_config_shot(pipeline);

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

int fimc_is_pipeline_stop(struct fimc_is_pipeline *pipeline, int streamoff)
{
	int ret;
	struct fimc_is *is = pipeline->is;

	mutex_lock(&pipeline->pipe_lock);

	/* Check if started */
	if (!test_bit(PIPELINE_OPEN, &pipeline->state) ||
		!test_bit(PIPELINE_START, &pipeline->state)) {
		ret = -EINVAL;
		goto err_exit;
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
			!test_bit(COMP_RUN, &pipeline->comp_state[IS_SCC]),
			FIMC_IS_COMMAND_TIMEOUT);
	if (!ret) {
		dev_err(pipeline->dev, "SCC timeout");
		ret = -EBUSY;
		goto err_exit;
	}
	ret = wait_event_timeout(pipeline->scaler[SCALER_SCP].event_q,
			!test_bit(COMP_RUN, &pipeline->comp_state[IS_SCP]),
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

int fimc_is_pipeline_open(struct fimc_is_pipeline *pipeline,
			struct fimc_is_sensor *sensor)
{
	struct fimc_is *is = pipeline->is;
	struct is_region *region;
	unsigned long index[2] = {0};
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
	pipeline->sensor = sensor;

	if (is->num_pipelines == 0) {
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
	ret = fimc_is_itf_open_sensor(&is->interface,
			pipeline->instance,
			sensor->drvdata->id,
			sensor->i2c_bus,
			pipeline->minfo->shared.paddr);
	if (ret) {
		dev_err(pipeline->dev, "Open sensor failed\n");
		goto err_exit;
	}

	/* Load setfile */
	ret = fimc_is_pipeline_setfile(pipeline);
	if (ret)
		goto err_exit;

	/* Stream off */
	ret = fimc_is_itf_stream_off(&is->interface, pipeline->instance);
	if (ret)
		goto err_exit;

	/* Process off */
	ret = fimc_is_itf_process_off(&is->interface, pipeline->instance);
	if (ret)
		goto err_exit;

	if (is->num_pipelines == 0) {
		/* Copy init params to FW region */
		memset(&region->parameter, 0x0, sizeof(struct is_param_region));

		region->parameter.sensor = init_sensor_param;
		region->parameter.isp = init_isp_param;
		region->parameter.drc = init_drc_param;
		region->parameter.scalerc = init_scalerc_param;
		region->parameter.odc = init_odc_param;
		region->parameter.dis = init_dis_param;
		region->parameter.tdnr = init_tdnr_param;
		region->parameter.scalerp = init_scalerp_param;
		region->parameter.fd = init_fd_param;
		wmb();

		/* Set all init params to FW */
		index[0] = 0xffffffff;
		index[1] = 0xffffffff;
		ret = fimc_is_itf_set_param(&is->interface, pipeline->instance,
				index[0], index[1]);
		if (ret) {
			dev_err(pipeline->dev, "%s failed\n", __func__);
			return -EINVAL;
		}
	}

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

	if (test_bit(PIPELINE_START, &pipeline->state)) {
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

	clear_bit(PIPELINE_OPEN, &pipeline->state);
	mutex_unlock(&pipeline->pipe_lock);
	return 0;

err_exit:
	mutex_unlock(&pipeline->pipe_lock);
	return ret;
}
