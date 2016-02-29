/*
 * videobuf2-dma-coherent.h - DMA coherent memory allocator for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#ifndef _MEDIA_VIDEOBUF2_DMA_COHERENT_H
#define _MEDIA_VIDEOBUF2_DMA_COHERENT_H

#include <media/videobuf2-core.h>
#include <linux/dma-mapping.h>

static inline dma_addr_t
vb2_dma_contig_plane_dma_addr(struct vb2_buffer *vb, unsigned int plane_no)
{
	dma_addr_t *addr = vb2_plane_cookie(vb, plane_no);

	return *addr;
}

static inline bool is_vb2_iommu_supported(struct device *dev)
{
#ifdef CONFIG_ARM_DMA_USE_IOMMU
	return dev->archdata.mapping ? true : false;
#else
	return false;
#endif
}

#define VB2_SET_CACHEABLE	(1 << 0)

void *vb2_dma_contig_init_ctx(struct device *dev);
void vb2_dma_contig_cleanup_ctx(void *alloc_ctx);
void vb2_dma_contig_set_cacheable(void *alloc_ctx, bool cacheable);

extern const struct vb2_mem_ops vb2_dma_contig_memops;

#endif
