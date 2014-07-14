/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd
 * Kil-yeon Lim <kilyeon.im@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CMD_H
#define FIMC_IS_CMD_H

#define IS_COMMAND_VER 122 /* IS COMMAND VERSION 1.22 */

enum is_cmd {
	/* HOST -> IS */
        HIC_COMMAND_BEGIN = 0x1,
        HIC_PREVIEW_STILL = HIC_COMMAND_BEGIN,
	HIC_PREVIEW_VIDEO,
	HIC_CAPTURE_STILL,
	HIC_CAPTURE_VIDEO,
	HIC_PROCESS_START,
	HIC_PROCESS_STOP,
	HIC_STREAM_ON,
	HIC_STREAM_OFF,
	HIC_SHOT,
	HIC_GET_STATIC_METADATA,
	HIC_SET_CAM_CONTROL,
	HIC_GET_CAM_CONTROL,
	HIC_SET_PARAMETER,
	HIC_GET_PARAMETER,
	HIC_SET_A5_MEM_ACCESS,
        HIC_SET_A5_MAP = HIC_SET_A5_MEM_ACCESS,
	RESERVED2,
        /* HIC_SET_A5_UNMAP supported by FW v.130 nad higher */
        HIC_SET_A5_UNMAP = RESERVED2,
	HIC_GET_STATUS,
	/* SENSOR PART*/
        HIC_OPEN_SENSOR ,
	HIC_CLOSE_SENSOR,
	HIC_SIMMIAN_INIT,
	HIC_SIMMIAN_WRITE,
	HIC_SIMMIAN_READ,
	HIC_POWER_DOWN,
	HIC_GET_SET_FILE_ADDR,
	HIC_LOAD_SET_FILE,
	HIC_MSG_CONFIG,
	HIC_MSG_TEST,
	/* IS -> HOST */
        HIC_ISP_I2C_CONTROL,
        HIC_CALIBRATE_ACTUATOR,
        HIC_GET_IP_STATUS, /* 30 */
        HIC_I2C_CONTROL_LOCK,
        HIC_SYSTEM_CONTROL,
        HIC_SENSOR_MODE_CHANGE,
        HIC_MSG_SENSOR_END,
        HIC_COMMAND_END = HIC_MSG_SENSOR_END,
        /* IS -> HOST */
        IHC_COMMAND_BEGIN = 0x1000,
        IHC_GET_SENSOR_NUMBER = IHC_COMMAND_BEGIN,
	/* Parameter1 : Address of space to copy a setfile */
	/* Parameter2 : Space szie */
	IHC_SET_SHOT_MARK,
	/* PARAM1 : a frame number */
	/* PARAM2 : confidence level(smile 0~100) */
	/* PARMA3 : confidence level(blink 0~100) */
	IHC_SET_FACE_MARK,
	/* PARAM1 : coordinate count */
	/* PARAM2 : coordinate buffer address */
	IHC_FRAME_DONE,
	/* PARAM1 : frame start number */
	/* PARAM2 : frame count */
	IHC_AA_DONE,
	IHC_NOT_READY,
        IHC_FLASH_READY,
        /* F/W version >= 1.32 */
        IHC_REPORT_ERR,
        IHC_COMMAND_END
};

enum is_reply {
	ISR_DONE	= 0x2000,
	ISR_NDONE
};

enum is_scenario_id {
	ISS_PREVIEW_STILL,
	ISS_PREVIEW_VIDEO,
	ISS_CAPTURE_STILL,
	ISS_CAPTURE_VIDEO,
	ISS_END
};

enum is_subscenario_id {
	ISS_SUB_SCENARIO_STILL,
	ISS_SUB_SCENARIO_VIDEO,
	ISS_SUB_SCENARIO_SCENE1,
        /* 2: dual still preview */
        ISS_SUB_SCENARIO_DUAL_STILL       = ISS_SUB_SCENARIO_SCENE1,
	ISS_SUB_SCENARIO_SCENE2,
        /* 3: dual video */
        ISS_SUB_SCENARIO_DUAL_VIDEO       = ISS_SUB_SCENARIO_SCENE2,
	ISS_SUB_SCENARIO_SCENE3,
        /* 4: video high speed */
        ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED = ISS_SUB_SCENARIO_SCENE3,
        /* 5: still capture */
        ISS_SUB_SCENARIO_STILL_CAPTURE    = 5,

        ISS_SUB_END,
};

enum is_system_control_id {
        IS_SYS_CLOCK_GATE = 0,
        IS_SYS_END,
};

enum is_system_control_cmd {
        SYS_CONTROL_DISABLE = 0,
        SYS_CONTROL_ENABLE  = 1,
};

struct is_setfile_header_element {
	u32 binary_addr;
	u32 binary_size;
};

struct is_setfile_header {
	struct is_setfile_header_element isp[ISS_END];
	struct is_setfile_header_element drc[ISS_END];
	struct is_setfile_header_element fd[ISS_END];
};

#endif
