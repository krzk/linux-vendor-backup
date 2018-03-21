/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/videodev2.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/of.h>

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"

static dma_addr_t plane_addr(struct vb2_buffer *vb, u32 plane_no)
{
	return vb2_dma_contig_plane_dma_addr(vb, plane_no);
}

static void * plane_kvaddr(struct vb2_buffer *vb, u32 plane_no)
{
	void *kvaddr = vb2_plane_vaddr(vb, plane_no);

	return kvaddr;
}

int vb2_null_attach_iommu(void *alloc_ctx)
{
	return 0;
}

void vb2_null_detach_iommu(void *alloc_ctx)
{

}

void vb2_null_set_cached(void *ctx, bool cached)
{

}

void vb2_null_destroy_context(void *ctx)
{

}

static const struct fimc_is_vb2 fimc_is_vb2_dc = {
	.ops		= &vb2_dma_contig_memops,
	.cleanup	= vb2_null_destroy_context,
	.plane_addr	= plane_addr,
	.plane_kvaddr	= plane_kvaddr,
	.resume		= vb2_null_attach_iommu,
	.suspend	= vb2_null_detach_iommu,
	.set_cacheable	= vb2_null_set_cached,
};

int fimc_is_mem_probe(struct fimc_is_mem *this, struct platform_device *pdev)
{
	this->vb2 = &fimc_is_vb2_dc;

	this->pdev = pdev;
	return 0;
}
