/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "fimc-is-fw.h"
#include "fimc-is.h"
#include "fimc-is-metadata.h"

#define V120 FIMC_IS_FW_V120
#define V130 FIMC_IS_FW_V130

#define DECLARE_FW_TYPE(v, type) \
        struct param_ ## v ## _ ## type
#define DECLARE_FW_SPECIFIC_TYPE(v,t) \
                        DECLARE_FW_TYPE(v, t)
#define FW_TYPE(v,t) DECLARE_FW_SPECIFIC_TYPE(v, t)

#define SENSOR_WIDTH_PADDING		16
#define SENSOR_HEIGHT_PADDING		10

static const struct fimc_is_fw_ops fimc_is_fw_def_ops;

const struct fimc_is_fw_ops *fimc_is_fw_ops = &fimc_is_fw_def_ops;

/**
 * @defgroup FIMC_IS_FW_V120_SUPPORT
 * FIMC-IS firmware versrion 12x support
 *
 * @{
 */

/**
 * @brief FIMC IS sub-block OTF input confgiuration
 * parameters specific for given firmware version
 */
DECLARE_FW_SPECIFIC_TYPE(V120,otf_input_ext) {
        u32	crop_offset_x;
        u32	crop_offset_y;
        u32	crop_width;
        u32	crop_height;
        u32	frametime_min;
        u32	frametime_max;
};

/**
 * @brief FIMC IS sub-block DMA input confgiuration
 * parameters specific for given firmware version
 */
DECLARE_FW_SPECIFIC_TYPE(V120, dma_input_ext) {
        u32	bayer_crop_offset_x;
        u32	bayer_crop_offset_y;
        u32	bayer_crop_width;
        u32	bayer_crop_height;
        u32	dma_crop_offset_x;
        u32	dma_crop_offset_y;
        u32	dma_crop_width;
        u32	dma_crop_height;
        u32	user_min_frametime;
        u32	user_max_frametime;
        u32	wide_frame_gap;
        u32	frame_gap;
        u32	line_gap;
        u32	reserved[2];
};

/**
 * @brief FIMC IS ISP sub-block confgiuration
 * parameters specific for given firmware version
 *
 * @note FIMC-IS frimware version 120+ combines
 * 	 configuration parameters for ISP with
 * 	 3AA sub-block
 */
DECLARE_FW_SPECIFIC_TYPE(V120, isp_ext) {
        struct param_control		control;
        struct param_otf_input		otf_input;
        struct param_dma_input		dma1_input;
        struct param_dma_input		dma2_input;
        struct param_isp_aa		aa;
        struct param_isp_flash		flash;
        struct param_isp_awb		awb;
        struct param_isp_imageeffect	effect;
        struct param_isp_iso		iso;
        struct param_isp_adjust		adjust;
        struct param_isp_metering	metering;
        struct param_isp_afc		afc;
        struct param_otf_output		otf_output;
        struct param_dma_output		dma1_output;
        struct param_dma_output		dma2_output;
};

/**
 * @brief FIMC-IS firmware version 12x specific data
 * 	  Informative purposes only
 */
#define FW_V120_SETFILE_SIZE            0xc0d8
#define FW_V120_TDNR_MEM_SIZE           (1920 * 1080 * 4)
#define FW_V120_SHARED_REG_ADDR         0x008c0000

/**
 * @brief Generic info on FIMC-IS shared memory layout
 */
static const struct fimc_is_fw_mem fimc_is_fw_120_mem = {
        .fw_size 	= 0xb00000, /* FW: 0x00a00000 + SENSOR: 0x00100000 */
        .region_offset 	= 0x9fb000, /* FW:0x00a00000 - REGION:0x5000 */
        .dbg_offset	= 0x840000,
        .shot_size	= sizeof(struct camera2_shot_base),
        .meta_type	= CAMERA2_SHOT_BASE_MODE,
};

/**
 * @brief Get the memory layout for given FIMC-IS firmware version
 */
static int fimc_is_fw_120_mem_cfg(struct fimc_is_fw_mem *memcfg)
{
        if(!memcfg)
                return -EINVAL;
        memcpy(memcfg, &fimc_is_fw_120_mem, sizeof(fimc_is_fw_120_mem));
        return 0;
}

/**
 * @brief Tweak FIMC-IS sub-block OTF input settings
 */
static int fimc_is_fw_120_tweak_dma(struct param_dma_input *dma_input,
                                    struct v4l2_rect* crop_reg)
{
        FW_TYPE(V120, dma_input_ext) *params;
        params = (FW_TYPE(V120, dma_input_ext)*)dma_input->reserved;

        params->dma_crop_offset_x = 0;
        params->dma_crop_offset_y = 0;
        params->dma_crop_width    = dma_input->width;
        params->dma_crop_height   = dma_input->height;
        /* Bayer crop */
        params->bayer_crop_offset_x = crop_reg->left;
        params->bayer_crop_offset_y = crop_reg->top;
        params->bayer_crop_width    = crop_reg->width;
        params->bayer_crop_height   = crop_reg->height;

        params->user_min_frametime = 0;
        params->user_max_frametime = 1000000;

        params->wide_frame_gap = 1;
        params->frame_gap = 4096;
        params->line_gap = 45;
        /*
         * [0] : sensor size same as dma input size
         * [x] : reserved field for sensor size
         */
        params->reserved[1] = 0;
        params->reserved[2] = 0;
        return 0;
}

/**
 * @brief Tweak FIMC-IS ISP sub-block settings
 */
static int fimc_is_fw_120_tweak_isp(struct taa_param *taa_param,
                                    struct v4l2_rect* crop_reg)
{
        FW_TYPE(V120, isp_ext) *params;
        params = (FW_TYPE(V120, isp_ext)*)(taa_param);

        params->aa.cmd = ISP_AA_COMMAND_START;
        params->flash.cmd = ISP_FLASH_COMMAND_DISABLE;
        params->flash.redeye = ISP_FLASH_REDEYE_DISABLE;
        params->awb.cmd = ISP_AWB_COMMAND_AUTO;
        params->effect.cmd = ISP_IMAGE_EFFECT_DISABLE;
        params->iso.cmd = ISP_ISO_COMMAND_AUTO;
        params->adjust.cmd = ISP_ADJUST_COMMAND_AUTO;
        params->metering.cmd = ISP_METERING_COMMAND_CENTER;
        params->metering.win_width = params->dma1_input.width;
        params->metering.win_height = params->dma1_input.height;
        params->afc.cmd = ISP_AFC_COMMAND_AUTO;

        return 0;
}

/**
 * @brief Tweak configuration parameters for requested FIMC-IS sub-block
 */
static int fimc_is_fw_120_tweak_params(unsigned int type, void* base_addr, void *data)
{
        int ret = -EINVAL;

        if (!base_addr)
                return ret;
        if(PARAM_DMA_INPUT == type)
                ret = fimc_is_fw_120_tweak_dma(base_addr, data);
        else if (PARAM_ISP_CAM == type)
                ret = fimc_is_fw_120_tweak_isp(base_addr, data);
        return ret;
}

/**
 * @brief Setup inittial camera shot configuration
 */
static int fimc_is_fw_120_config_shot(void * addr)
{
        struct camera2_shot_base *shot = (struct camera2_shot_base*)addr;
        if (!addr)
                return -EINVAL;
        shot->magicnumber    = FIMC_IS_MAGIC_NUMBER;
        shot->ctl.aa.mode    = AA_CONTROL_AUTO;
        shot->ctl.aa.ae_mode = AA_AEMODE_ON;
        return 0;
}

static const struct fimc_is_fw_ops fimc_is_fw_120_ops = {
        .mem_config  = fimc_is_fw_120_mem_cfg,
        .tweak_param = fimc_is_fw_120_tweak_params,
        .config_shot = fimc_is_fw_120_config_shot,
};
/** @} */ /* End of FIMC_IS_FW_V120_SUPPORT */

/**
 * @defgroup FIMC_IS_FW_V130_SUPPORT
 * FIMC-IS firmware versrion 13x support
 *
 * @{
 */

/**
 * @brief FIMC IS sub-block OTF input confgiuration
 * parameters specific for given firmware version
 */
DECLARE_FW_SPECIFIC_TYPE(V130,otf_input_ext) {
        u32	sensor_binning_ratio_x;
        u32	sensor_binning_ratio_y;
        u32	bns_binning_enable;
        u32	bns_binning_ratio_x;
        u32	bns_binning_ratio_y;
        u32	bns_margin_left;
        u32	bns_binning_top;
        u32	bns_output_width;
        u32	bns_output_height;
        u32	crop_enable;
        u32	crop_offset_x;
        u32	crop_offset_y;
        u32	crop_width;
        u32	crop_height;
        u32	bds_out_enable;
        u32	bds_out_width;
        u32	bds_out_height;
        u32	frametime_min;
        u32	frametime_max;
        u32	scaler_path_sel;
};

/**
 * @brief FIMC IS sub-block DMA input confgiuration
 * parameters specific for given firmware version
 */
DECLARE_FW_SPECIFIC_TYPE(V130, dma_input_ext) {
        u32	sensor_binning_ratio_x; /* ex(x1: 1000, x0.5: 2000) */
        u32	sensor_binning_ratio_y;
        u32	dma_crop_enable;
        u32	dma_crop_offset_x;
        u32	dma_crop_offset_y;
        u32	dma_crop_width;
        u32	dma_crop_height;
        u32	bayer_crop_enable;
        u32	bayer_crop_offset_x;
        u32	bayer_crop_offset_y;
        u32	bayer_crop_width;
        u32	bayer_crop_height;
        u32	bds_out_enable;
        u32	bds_out_width;
        u32	bds_out_height;
        u32	user_min_frametime;
        u32	user_max_frametime;
        u32	reserved[2];
};

/**
 * @brief FIMC-IS firmware version 13x specific data
 * 	  Informative purposes only
 */
#define FW_V130_SETFILE_SIZE		0x00140000
#define FW_V130_SHARED_REG_ADDR		0x013C0000

/**
 * @brief Generic info on FIMC-IS shared memory layout
 */
static const struct fimc_is_fw_mem fimc_is_fw_130_mem = {
        .fw_size 	= 0x01400000,
        .region_offset 	= 0x13fb000, /* FW: 0x01400000 - REGION: 0x00005000 */
        .dbg_offset	= 0x01340000,
        .shot_size 	= sizeof(struct camera2_shot),
        .meta_type	= CAMERA2_SHOT_EXT_MODE,
};

/**
 * @brief FIMC-IS FW supported cam controls
 */
static const unsigned long metadata2_ctrl[] = {
	/* FIMC_IS_CID_BRIGHTNESS	*/ 0x00a8,
	/* FIMC_IS_CID_CONTRAST		*/ 0x00ac,
	/* FIMC_IS_CID_SATURATION	*/ 0x00a4,
	/* FIMC_IS_CID_HUE		*/ 0x00a0,
	/* FIMC_IS_CID_AWB		*/ 0x0458,
	/* FIMC_IS_CID_EXPOSURE]	*/ 0x0038,
	/* FIMC_IS_CID_FOCUS_MODE	*/ 0x0470,
	/* FIMC_IS_CID_FOCUS_RANGE	*/ 0x0020,
	/* FIMC_IS_CID_AWB_MODE		*/ 0x0458,
	/* FIMC_IS_CID_ISO		*/ 0x0048,
	/* FIMC_IS_CID_ISO_MODE		*/ 0x3818,
	/* FIMC_IS_CID_SCENE		*/ 0x0424,
	/* FIMC_IS_CID_COLOR_MODE	*/ 0x0078,
};

/**
 * @brief Get the memory layout for given FIMC-IS firmware version
 */
static int fimc_is_fw_130_mem_cfg(struct fimc_is_fw_mem *memcfg)
{
        if(!memcfg)
                return -EINVAL;
        memcpy(memcfg, &fimc_is_fw_130_mem, sizeof(fimc_is_fw_130_mem));
        return 0;
}

/**
 * @brief Tweak FIMC-IS sub-block OTF input settings
 */
static int fimc_is_fw_130_tweak_otf(struct param_otf_input * otf_input,
                                    struct v4l2_rect* crop_reg)
{
        FW_TYPE(V130, otf_input_ext) *params;
        params = (FW_TYPE(V130,otf_input_ext)*)otf_input->reserved;

        params->crop_enable     = 1;
        params->crop_offset_x   = crop_reg->left;
        params->crop_offset_y   = crop_reg->top;
        params->crop_width      = crop_reg->width;
        params->crop_height     = crop_reg->height;

        params->frametime_min = 0;
        params->frametime_max = 1000000;

        return 0;
}

/**
 * @brief Tweak FIMC-IS sub-block DMA input settings
 */
static int fimc_is_fw_130_tweak_dma(struct param_dma_input *dma_input,
                                    struct v4l2_rect* crop_reg)
{
        /* Currently only ISP dma input is supported */
        FW_TYPE(V130, dma_input_ext) *params;
        params = (FW_TYPE(V130, dma_input_ext)*)dma_input->reserved;

        params->dma_crop_enable   = 1;
        params->dma_crop_offset_x = 0;
        params->dma_crop_offset_y = 0;
        params->dma_crop_width    = crop_reg->width;
        params->dma_crop_height   = crop_reg->height;
        /* Bayer crop */
        params->bayer_crop_enable   = 1;
        params->bayer_crop_offset_x = crop_reg->left;
        params->bayer_crop_offset_y = crop_reg->top;
        params->bayer_crop_width    = crop_reg->width;
        params->bayer_crop_height   = crop_reg->height;
        /* Bundle destination size */
        params->bds_out_enable = 1;
        params->bds_out_width  = crop_reg->width;
        params->bds_out_height = crop_reg->height;

        params->user_min_frametime = 0;
        params->user_max_frametime = 1000000;
        /*
         * [0] : sensor size same as dma input size
         * [x] : reserved field for sensor size
         */
        params->reserved[1] = 0;
        params->reserved[2] = 0;

        return 0;
}

/**
 * @brief Tweak configuration parameters for requested FIMC-IS sub-block
 */
static int fimc_is_fw_130_tweak_params(unsigned int type, void* base_addr, void *data)
{
        int ret = -EINVAL;

        if (!base_addr)
                return ret;
        if (PARAM_OTF_INPUT == type)
                ret = fimc_is_fw_130_tweak_otf(base_addr, data);
        else
                if(PARAM_DMA_INPUT == type)
                        ret = fimc_is_fw_130_tweak_dma(base_addr, data);
        return ret;
}

/**
 * @brief Setup inittial camera shot configuration
 */
static int fimc_is_fw_130_config_shot(void * addr)
{
	struct camera2_shot *shot = (struct camera2_shot *)addr;

	if (!addr)
		return -EINVAL;
	/* REQUEST */
	shot->ctl.request.base.id = 1;
	shot->ctl.request.base.metadatamode = METADATA_MODE_FULL;
	shot->ctl.request.base.framecount   = 0;
	/* LENS */
	shot->ctl.lens.optical_stabilization_mode = OPTICAL_STABILIZATION_MODE_ON;
	shot->ctl.lens.aperture = 1;
	/* HOTPIXEL */
	shot->ctl.hotpixel.mode = PROCESSING_MODE_HIGH_QUALITY;
	/* DEMOSAIC*/
	shot->ctl.demosaic.mode = PROCESSING_MODE_HIGH_QUALITY;
	/* NOISE */
	shot->ctl.noise.mode = PROCESSING_MODE_HIGH_QUALITY;
	shot->ctl.noise.strength = 1;
	/* SHADING */
	shot->ctl.shading.mode = PROCESSING_MODE_HIGH_QUALITY;
	/* GEOMETRIC */
	/* COLOR */
	shot->ctl.color.base.mode = COLOR_CORRECTION_MODE_HIGH_QUALITY;
	shot->ctl.color.base.saturation = 1;
	/* AA */
	shot->ctl.aa.capture_intent = AA_CAPTURE_INTENT_PREVIEW;
	shot->ctl.aa.mode     = AA_CONTROL_AUTO;
	shot->ctl.aa.ae_mode  = AA_AEMODE_ON;

	shot->ctl.aa.ae_anti_banding_mode = AA_AE_ANTIBANDING_AUTO;

	shot->ctl.aa.ae_flash_mode = AA_FLASHMODE_ON;
	shot->ctl.aa.ae_target_fps_range[0] = 15;
	shot->ctl.aa.ae_target_fps_range[1] = 30;
	/* USER CONTROL */
	shot->uctl.u_update_bitmap = 1 /* SENSOR */ | (1<<1) /* FLASH*/;
	shot->uctl.flash_ud.ctl.flash_mode   = CAM2_FLASH_MODE_SINGLE;

	shot->magicnumber = FIMC_IS_MAGIC_NUMBER;

	return 0;
}

static int fimc_is_fw_130_set_control(void *base_addr, unsigned int id,
				      unsigned long val)
{
	unsigned long *ctrl;

	if (!base_addr || id >= FIMC_IS_CID_MAX)
		return -EINVAL;

	ctrl  = (unsigned long *)((char *)base_addr + metadata2_ctrl[id]);
	*ctrl = val;
	return 0;
}

static const struct fimc_is_fw_ops fimc_is_fw_130_ops = {
	.mem_config  = fimc_is_fw_130_mem_cfg,
	.tweak_param = fimc_is_fw_130_tweak_params,
	.config_shot = fimc_is_fw_130_config_shot,
	.set_control = fimc_is_fw_130_set_control,
};
/** @} */ /* End of FIMC_IS_FW_V130_SUPPORT */


/**
 * @defgroup FIMC_IS_FW_SUPPORT
 * FIMC-IS firmware support
 *
 * @{
 */
/**
 * @brief Supported FIMC-IS firmwares
 */
static const struct  fimc_is_fw_config {

        unsigned int 	version;
        const struct 	fimc_is_fw_ops *ops;

} fimc_is_fw_configs[] = {
        {
                .version = FIMC_IS_FW_V120,
                .ops = &fimc_is_fw_120_ops,
        },
        {
                .version = FIMC_IS_FW_V130,
                .ops = &fimc_is_fw_130_ops,
        },
};

/**
 * @brief Set-up appropriate FIMC-IS firmware
 * 	  configuration based on given fimrawre
 * 	  verson
 */
int fimc_is_fw_init_ops(unsigned int fw_version)
{
        int i;
        /*
         * Drop the last digit from the firmware version :
         * e.g.: ver. 130-139 should fall back to 130
         */
        fw_version -= fw_version%10;
        for (i=0; i < ARRAY_SIZE(fimc_is_fw_configs); ++i) {
                if (fimc_is_fw_configs[i].version == fw_version) {
                        fimc_is_fw_ops = fimc_is_fw_configs[i].ops;
                        return 0;
                }
        }
        return -EINVAL;
}
/** @} */ /* End of FIMC_IS_FW_SUPPORT */
