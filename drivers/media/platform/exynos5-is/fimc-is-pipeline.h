/*
 * Samsung EXYNOS5 FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *  Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef FIMC_IS_PIPELINE_H_
#define FIMC_IS_PIPELINE_H_

#include "fimc-is-core.h"
#include "fimc-is-sensor.h"
#include "fimc-is-isp.h"
#include "fimc-is-scaler.h"

#define FIMC_IS_A5_TIMEOUT		1000

#define FIMC_IS_SCP_REGION_INDEX	400
#define FIMC_IS_SCC_REGION_INDEX	447

#define MAX_ODC_INTERNAL_BUF_WIDTH	2560  /* 4808 in HW */
#define MAX_ODC_INTERNAL_BUF_HEIGHT	1920  /* 3356 in HW */
#define SIZE_ODC_INTERNAL_BUF \
	(MAX_ODC_INTERNAL_BUF_WIDTH * MAX_ODC_INTERNAL_BUF_HEIGHT * 3)

#define MAX_DIS_INTERNAL_BUF_WIDTH	2400
#define MAX_DIS_INTERNAL_BUF_HEIGHT	1360
#define SIZE_DIS_INTERNAL_BUF \
	(MAX_DIS_INTERNAL_BUF_WIDTH * MAX_DIS_INTERNAL_BUF_HEIGHT * 2)

#define MAX_3DNR_INTERNAL_BUF_WIDTH	1920
#define MAX_3DNR_INTERNAL_BUF_HEIGHT	1088
#define SIZE_DNR_INTERNAL_BUF \
	(MAX_3DNR_INTERNAL_BUF_WIDTH * MAX_3DNR_INTERNAL_BUF_HEIGHT * 2)

#define NUM_ODC_INTERNAL_BUF		2
#define NUM_DIS_INTERNAL_BUF		5
#define NUM_DNR_INTERNAL_BUF		2

#define FIMC_IS_FW_BASE_MASK		((1 << 26) - 1)

#define FIMC_IS_NUM_COMPS		8

#define FIMC_IS_MAGIC_NUMBER		0x23456789

/*
 * FIMC IS sub-block mask specifying which sub-blocks
 * are available for given SoC.
 * @see is_components for sub-blocks identifiers
 */
#define FIMC_IS_EXYNOS5_SUBBLOCKS	0xff
#define FIMC_IS_EXYNOS3250_SUBBLOCKS	0x87

enum pipeline_state {
	PIPELINE_INIT,
        PIPELINE_POWER,
	PIPELINE_OPEN,
        PIPELINE_PREPARE,
	PIPELINE_START,
	PIPELINE_RUN,
        PIPELINE_RESET,
};

enum is_components {
	IS_ISP,
	IS_DRC,
	IS_SCC,
	IS_ODC,
	IS_DIS,
	IS_TDNR,
	IS_SCP,
	IS_FD
};

enum component_state {
        COMP_INVALID,
	COMP_ENABLE,
	COMP_START,
	COMP_RUN
};

struct fimc_is_config {
        unsigned long 	params[2];
        int		metadata_mode;
};

struct fimc_is_pipeline {
	unsigned long		state;
        unsigned long		subip_state[FIMC_IS_NUM_COMPS];
        unsigned int		subip_mask;
	bool			force_down;
	unsigned int		instance;
	/* Locks the isp / scaler buffers */
	spinlock_t		slock_buf;
	unsigned long		slock_flags;
	wait_queue_head_t	wait_q;
        struct work_struct	work;
        struct delayed_work	wdg_work;
	/* For locking pipeline ops */
	struct mutex		pipe_lock;
	/* For locking scaler ops in pipeline */
	struct mutex		pipe_scl_lock;

	struct fimc_is_meminfo	*minfo;
	struct is_region	*is_region;
	struct device		*dev;

	struct fimc_is		*is;
	struct fimc_is_sensor	*sensor;
	struct fimc_is_isp	isp;
	struct fimc_is_scaler	scaler[FIMC_IS_NUM_SCALERS];
        struct fimc_is_config	config;
        const struct fimc_is_fw_ctrl *fw_ctrl;
        unsigned int		data_flow;
	unsigned int		fcount;
        ktime_t			last_frame;
        long long		frame_duration;
	unsigned int		isp_width;
	unsigned int		isp_height;
        struct fimc_is_event_listener listener;
};

void fimc_is_pipeline_buf_lock(struct fimc_is_pipeline *pipeline);
void fimc_is_pipeline_buf_unlock(struct fimc_is_pipeline *pipeline);
int fimc_is_pipeline_setparams(struct fimc_is_pipeline *pipeline);
int fimc_is_pipeline_scaler_start(struct fimc_is_pipeline *pipeline,
		enum fimc_is_scaler_id scaler_id,
		unsigned int num_bufs,
		unsigned int num_planes);
int fimc_is_pipeline_scaler_stop(struct fimc_is_pipeline *pipeline,
		enum fimc_is_scaler_id scaler_id);
int fimc_is_pipeline_set_scaler_effect(struct fimc_is_pipeline *pipeline,
				   unsigned int rotation);
void fimc_is_pipeline_config_shot(struct fimc_is_pipeline *pipeline);
int fimc_is_pipeline_update_shot_ctrl(struct fimc_is_pipeline *pipeline,
                                     unsigned int id,
                                     unsigned long val);
int fimc_is_pipeline_shot_safe(struct fimc_is_pipeline *pipeline);
int fimc_is_pipeline_shot(struct fimc_is_pipeline *pipeline);
int fimc_is_pipeline_start(struct fimc_is_pipeline *pipeline, int streamon);
int fimc_is_pipeline_stop(struct fimc_is_pipeline *pipeline, int streamoff);
int fimc_is_pipeline_init(struct fimc_is_pipeline *pipeline,
                        unsigned int instance,
                        void *is_ctx);
int fimc_is_pipeline_destroy(struct fimc_is_pipeline *pipeline);
int fimc_is_pipeline_open(struct fimc_is_pipeline *pipeline,
			struct fimc_is_sensor *sensor);
int fimc_is_pipeline_close(struct fimc_is_pipeline *pipeline);
void fimc_is_pipeline_reset(struct fimc_is_pipeline *pipeline);
#endif
