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

#ifndef FIMC_IS_H_
#define FIMC_IS_H_

#include "fimc-is-err.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-pipeline.h"
#include "fimc-is-interface.h"

#define fimc_interface_to_is(p) container_of(p, struct fimc_is, interface)
#define fimc_sensor_to_is(p) container_of(p, struct fimc_is, sensor)

/*
 * Macros used by media dev to get the subdev and vfd
 * is - driver data from pdev
 * pid - pipeline index
 */
#define fimc_is_isp_get_sd(is, pid) (&is->pipeline[pid].isp.subdev)
#define fimc_is_isp_get_vfd(is, pid) (&is->pipeline[pid].isp.vfd)
#define fimc_is_scc_get_sd(is, pid) \
	(&is->pipeline[pid].scaler[SCALER_SCC].subdev)
#define fimc_is_scc_get_vfd(is, pid) \
	(&is->pipeline[pid].scaler[SCALER_SCC].vfd)
#define fimc_is_scp_get_sd(is, pid) \
	(&is->pipeline[pid].scaler[SCALER_SCP].subdev)
#define fimc_is_scp_get_vfd(is, pid) \
	(&is->pipeline[pid].scaler[SCALER_SCP].vfd)
/*
 * is - driver data from pdev
 * sid - sensor index
 */
#define fimc_is_sensor_get_sd(is, sid) (&is->sensor[sid].subdev)


/**
 * struct fimc_is - fimc-is driver private data
 * @pdev: pointer to FIMC-IS platform device
 * @pdata: platform data for FIMC-IS
 * @alloc_ctx: videobuf2 memory allocator context
 * @clock: FIMC-IS clocks
 * @pmu_regs: PMU reg base address
 * @num_pipelines: number of pipelines opened
 * @minfo: internal memory organization info
 * @drvdata: fimc-is driver data
 * @sensor: FIMC-IS sensor context
 * @pipeline: hardware pipeline context
 * @interface: hardware interface context
 */
struct fimc_is {
	struct platform_device		*pdev;

	struct vb2_alloc_ctx		*alloc_ctx;
	struct clk			*clock[IS_CLK_MAX_NUM];
	void __iomem			*pmu_regs;
	unsigned int			num_pipelines;

	struct fimc_is_meminfo		minfo;

	struct fimc_is_drvdata		*drvdata;
	struct fimc_is_sensor		sensor[FIMC_IS_NUM_SENSORS];
	struct fimc_is_pipeline		pipeline[FIMC_IS_NUM_PIPELINES];
	struct fimc_is_interface	interface;
};

/* Queue operations for ISP */
static inline void fimc_is_isp_wait_queue_add(struct fimc_is_isp *isp,
		struct fimc_is_buf *buf)
{
	list_add_tail(&buf->list, &isp->wait_queue);
	isp->wait_queue_cnt++;
}

static inline struct fimc_is_buf *fimc_is_isp_wait_queue_get(
		struct fimc_is_isp *isp)
{
	struct fimc_is_buf *buf;
	buf = list_entry(isp->wait_queue.next,
			struct fimc_is_buf, list);
	list_del(&buf->list);
	isp->wait_queue_cnt--;
	return buf;
}

static inline void fimc_is_isp_run_queue_add(struct fimc_is_isp *isp,
		struct fimc_is_buf *buf)
{
	list_add_tail(&buf->list, &isp->run_queue);
	isp->run_queue_cnt++;
}

static inline struct fimc_is_buf *fimc_is_isp_run_queue_get(
		struct fimc_is_isp *isp)
{
	struct fimc_is_buf *buf;
	buf = list_entry(isp->run_queue.next,
			struct fimc_is_buf, list);
	list_del(&buf->list);
	isp->run_queue_cnt--;
	return buf;
}

/* Queue operations for SCALER */
static inline void fimc_is_scaler_wait_queue_add(struct fimc_is_scaler *scp,
		struct fimc_is_buf *buf)
{
	list_add_tail(&buf->list, &scp->wait_queue);
	scp->wait_queue_cnt++;
}

static inline struct fimc_is_buf *fimc_is_scaler_wait_queue_get(
		struct fimc_is_scaler *scp)
{
	struct fimc_is_buf *buf;
	buf = list_entry(scp->wait_queue.next,
			struct fimc_is_buf, list);
	list_del(&buf->list);
	scp->wait_queue_cnt--;
	return buf;
}

static inline void fimc_is_scaler_run_queue_add(struct fimc_is_scaler *scp,
		struct fimc_is_buf *buf)
{
	list_add_tail(&buf->list, &scp->run_queue);
	scp->run_queue_cnt++;
}

static inline struct fimc_is_buf *fimc_is_scaler_run_queue_get(
		struct fimc_is_scaler *scp)
{
	struct fimc_is_buf *buf;
	buf = list_entry(scp->run_queue.next,
			struct fimc_is_buf, list);
	list_del(&buf->list);
	scp->run_queue_cnt--;
	return buf;
}

static inline void pmu_is_write(u32 v, struct fimc_is *is, unsigned int offset)
{
	writel(v, is->pmu_regs + offset);
}

static inline u32 pmu_is_read(struct fimc_is *is, unsigned int offset)
{
	return readl(is->pmu_regs + offset);
}

#endif
