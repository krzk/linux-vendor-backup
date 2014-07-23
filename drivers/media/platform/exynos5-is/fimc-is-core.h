/*
 * Samsung EXYNOS5/EXYNOS3 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *  Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef FIMC_IS_CORE_H_
#define FIMC_IS_CORE_H_

#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>
#include <media/s5p_fimc.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>

#define FIMC_IS_DRV_NAME		"exynos5-fimc-is"

#define FIMC_IS_EXYNOS3250		0
#define FIMC_IS_EXYNOS5			1

#define FIMC_IS_COMMAND_TIMEOUT		(10 * HZ)
#define FIMC_IS_STARTUP_TIMEOUT		(3 * HZ)
#define FIMC_IS_SHUTDOWN_TIMEOUT	(10 * HZ)

/* EXYNOS5 */
#define FW_SHARED_OFFSET		(0x8c0000)
#define DEBUG_CNT			(500 * 1024)
#define DEBUGCTL_OFFSET			(0x8bd000)
#define DEBUG_FCOUNT			(0x8c64c0)

#define FIMC_IS_MAX_INSTANCES		1

#define FIMC_IS_NUM_SENSORS		2
#define FIMC_IS_NUM_PIPELINES		1

#define FIMC_IS_MAX_PLANES		3
#define FIMC_IS_NUM_SCALERS		2

enum fimc_is_gate_clks {
	IS_CLK_ISP,
	IS_CLK_MCU_ISP,
	IS_CLK_ISP_DIV0,
	IS_CLK_ISP_DIV1,
	IS_CLK_ISP_DIVMPWM,
	IS_CLK_MCU_ISP_DIV0,
	IS_CLK_MCU_ISP_DIV1,
        IS_CLK_CAM1,
        IS_CLK_GATE_MAX,
};

enum fimc_is_exynos3250_parent_clks {
        IS_CLK_MOUT_ACLK400_MCUISP_SUB = IS_CLK_GATE_MAX,
        IS_CLK_DIV_ACLK400_MCUISP,
        IS_CLK_MOUT_ACLK400_MCUISP,
        IS_CLK_MOUT_ACLK266_0,
        IS_CLK_MOUT_ACLK266,
        IS_CLK_DIV_ACLK266,
        IS_CLK_MOUT_ACLK266_SUB,
        IS_CLK_DIV_MPLL_PRE,
        IS_CLK_FIN_PLL,
        IS_CLKS_MAX,
};

/* Video capture states */
enum fimc_is_video_state {
	STATE_INIT,
	STATE_BUFS_ALLOCATED,
	STATE_RUNNING,
};

enum fimc_is_scaler_id {
	SCALER_SCC,
	SCALER_SCP
};

enum fimc_is_sensor_pos {
	SENSOR_CAM0,
	SENSOR_CAM1
};

enum fimc_is_buf_state {
	FIMC_IS_BUF_IDLE,
	FIMC_IS_BUF_QUEUED,
	FIMC_IS_BUF_ACTIVE,
	FIMC_IS_BUF_DONE,
	FIMC_IS_BUF_INVALID,
};

struct fimc_is_buf {
	struct vb2_buffer vb;
	unsigned int state;
	struct list_head list;
	unsigned int paddr[FIMC_IS_MAX_PLANES];
};

struct fimc_is_memory {
	/* physical base address */
	dma_addr_t paddr;
	/* virtual base address */
	void *vaddr;
	/* total length */
	unsigned int size;
};

struct fimc_is_meminfo {
	struct fimc_is_memory	fw;
	struct fimc_is_memory	shot;
	struct fimc_is_memory	region;
	struct fimc_is_memory	shared;
};

struct fimc_is_drvdata {
	unsigned int	num_instances;
	char		*fw_name;
        unsigned int	fw_version;
        unsigned int	clk_mask;
        unsigned int	subip_mask;
        unsigned long	variant;
	unsigned int	master_node:1;
};

/**
 * struct fimc_is_fmt - the driver's internal color format data
 * @name: format description
 * @fourcc: the fourcc code for this format
 * @depth: number of bytes per pixel
 * @num_planes: number of planes for this color format
 */
struct fimc_is_fmt {
	char		*name;
	unsigned int	fourcc;
	unsigned int	depth[FIMC_IS_MAX_PLANES];
	unsigned int	num_planes;
};

/**
 * struct fimc_is_event_listener  - internal representation
 * 				    of frame-related events listener
 * @private_data: Listener's private data
 * @event_handler: Event handler to be triggered upon upcoming
 * 		   events
 * @link: list link
 */
struct fimc_is_event_listener {
        void * private_data;
        void (*event_handler)(struct fimc_is_event_listener *, unsigned int, void*);
        struct list_head link;
};


#ifdef CONFIG_VIDEO_SAMSUNG_EXYNOS5_FIMC_IS
int fimc_is_init(void);
void fimc_is_cleanup(void);
#else
int fimc_is_init(void) { return 0; }
void fimc_is_cleanup(void) { };
#endif

#endif
