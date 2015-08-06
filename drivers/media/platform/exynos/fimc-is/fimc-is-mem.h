/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_MEM_H
#define FIMC_IS_MEM_H

#include <linux/platform_device.h>
#include <media/videobuf2-core.h>

struct fimc_is_minfo {
	dma_addr_t	base;		/* buffer base */
	size_t		size;		/* total length */
	void		*bitproc_buf;
	void		*fw_cookie;

	dma_addr_t	dvaddr;
	void *		kvaddr;
	dma_addr_t	dvaddr_debug;
	void *		kvaddr_debug;
	dma_addr_t	dvaddr_fshared;
	void *		kvaddr_fshared;
	dma_addr_t	dvaddr_region;
	void *		kvaddr_region;
	dma_addr_t	dvaddr_shared; /*shared region of is region*/
	void *		kvaddr_shared;
	dma_addr_t	dvaddr_odc;
	void *		kvaddr_odc;
	dma_addr_t	dvaddr_dis;
	void *		kvaddr_dis;
	dma_addr_t	dvaddr_3dnr;
	void *		kvaddr_3dnr;
};

struct fimc_is_vb2 {
	const struct vb2_mem_ops *ops;
	void *(*init)(struct platform_device *pdev);
	void (*cleanup)(void *alloc_ctx);

	dma_addr_t (*plane_addr)(struct vb2_buffer *vb, u32 plane_no);
	void * (*plane_kvaddr)(struct vb2_buffer *vb, u32 plane_no);

	int (*resume)(void *alloc_ctx);
	void (*suspend)(void *alloc_ctx);

	void (*set_cacheable)(void *alloc_ctx, bool cacheable);
};

struct fimc_is_mem {
	struct platform_device		*pdev;
	struct vb2_alloc_ctx		*alloc_ctx;

	const struct fimc_is_vb2	*vb2;
};

int fimc_is_mem_probe(struct fimc_is_mem *this,
	struct platform_device *pdev);

#endif
