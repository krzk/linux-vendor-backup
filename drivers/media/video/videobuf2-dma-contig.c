/*
 * videobuf2-dma-contig.c - DMA contig memory allocator for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>

#include <asm/cacheflush.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>

struct vb2_dc_conf {
	struct device		*dev;
	unsigned int		flags;
};

struct vb2_dc_buf {
	struct device			*dev;
	void				*vaddr;
	struct page			**pages;
	unsigned long			size;
	struct dma_attrs		dma_attrs;
	dma_addr_t			dma_addr;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	atomic_t			refcount;
	struct sg_table			sgt_base;

	/* USERPTR related */
	struct vm_area_struct		*vma;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
};

/*********************************************/
/*        scatterlist table functions        */
/*********************************************/


static void vb2_dc_sgt_foreach_page(struct sg_table *sgt,
	void (*cb)(struct page *pg))
{
	struct scatterlist *s;
	unsigned int i;

	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		struct page *page = sg_page(s);
		unsigned int n_pages = PAGE_ALIGN(s->offset + s->length)
			>> PAGE_SHIFT;
		unsigned int j;

		for (j = 0; j < n_pages; ++j, ++page)
			cb(page);
	}
}

static unsigned long vb2_dc_get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected = sg_dma_address(s) + sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static void *vb2_dc_cookie(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return &buf->dma_addr;
}

static void *vb2_dc_vaddr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	unsigned int nr_pages;

	if (buf->vaddr)
		return buf->vaddr;

	if (buf->db_attach)
		buf->vaddr = dma_buf_vmap(buf->db_attach->dmabuf);
	else if (is_vb2_iommu_supported(buf->dev)) {
		nr_pages = buf->size >> PAGE_SHIFT;
		buf->vaddr = vmap(buf->pages, nr_pages, VM_MAP,
				pgprot_writecombine(PAGE_KERNEL));
	} else
		return NULL;

	if (IS_ERR_OR_NULL(buf->vaddr)) {
		dev_err(buf->dev,
			"failed to vmap pages to kernel space.\n");
		buf->vaddr = NULL;
		return NULL;
	}

	return buf->vaddr;
}

static unsigned int vb2_dc_num_users(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static void vb2_dc_prepare(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* DMABUF exporter will flush the cache for us */
	if (!sgt || buf->db_attach)
		return;

	if (dma_get_attr(DMA_ATTR_NON_CONSISTENT, &buf->dma_attrs)) {
		if (buf->size < SZ_1M)
			dma_sync_sg_for_device(buf->dev, sgt->sgl, sgt->nents,
					buf->dma_dir);
		else {
			flush_cache_all();
			outer_flush_all();
		}
	}
}

static void vb2_dc_finish(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* DMABUF exporter will flush the cache for us */
	if (!sgt || buf->db_attach)
		return;

	if (dma_get_attr(DMA_ATTR_NON_CONSISTENT, &buf->dma_attrs)) {
		if (buf->size < SZ_1M)
			dma_sync_sg_for_cpu(buf->dev, sgt->sgl, sgt->nents,
					buf->dma_dir);
		else {
			flush_cache_all();
			outer_flush_all();
		}
	}
}

/*********************************************/
/*        callbacks for MMAP buffers         */
/*********************************************/

static void vb2_dc_put(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	if (!atomic_dec_and_test(&buf->refcount))
		return;

	sg_free_table(&buf->sgt_base);
	if (is_vb2_iommu_supported(buf->dev)) {
		vunmap(buf->vaddr);
		dma_free_attrs(buf->dev, buf->size, buf->pages, buf->dma_addr,
				&buf->dma_attrs);
	} else
		dma_free_attrs(buf->dev, buf->size, buf->vaddr, buf->dma_addr,
				&buf->dma_attrs);

	kfree(buf);
}

static void *vb2_dc_alloc(void *alloc_ctx, unsigned long size, int write, int plane)
{
	struct vb2_dc_conf *conf = alloc_ctx;
	struct device *dev = conf->dev;
	struct vb2_dc_buf *buf;
	void *addr;
	int ret = -ENOMEM;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	init_dma_attrs(&buf->dma_attrs);
	dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &buf->dma_attrs);

	if (conf->flags & VB2_SET_CACHEABLE)
		dma_set_attr(DMA_ATTR_NON_CONSISTENT, &buf->dma_attrs);
	else {
		dma_set_attr(DMA_ATTR_WRITE_COMBINE, &buf->dma_attrs);
		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &buf->dma_attrs);
	}

	addr = dma_alloc_attrs(dev, size, &buf->dma_addr, GFP_KERNEL,
				     &buf->dma_attrs);
	if (!addr) {
		dev_err(dev, "dma_alloc_coherent of size %ld failed\n", size);
		goto fail_buf;
	}

	WARN_ON(buf->dma_addr & ~PAGE_MASK);

	ret = dma_get_sgtable(dev, &buf->sgt_base, addr, buf->dma_addr,
		size, &buf->dma_attrs);
	if (ret < 0) {
		dev_err(dev, "failed to get scatterlist from DMA API\n");
		goto fail_dma;
	}

	if (is_vb2_iommu_supported(dev))
		buf->pages = addr;
	else
		buf->vaddr = addr;

	buf->dev = dev;
	buf->size = size;
	buf->dma_sgt = &buf->sgt_base;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vb2_dc_put;
	buf->handler.arg = buf;

	atomic_inc(&buf->refcount);

	return buf;

fail_dma:
	dma_free_attrs(buf->dev, buf->size, addr, buf->dma_addr,
		       &buf->dma_attrs);

fail_buf:
	kfree(buf);

	return ERR_PTR(ret);
}

static int vb2_dc_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_dc_buf *buf = buf_priv;
	int ret;

	if (!buf) {
		printk(KERN_ERR "No buffer to map\n");
		return -EINVAL;
	}

	/*
	 * dma_mmap_* uses vm_pgoff as in-buffer offset, but we want to
	 * map whole buffer
	 */
	vma->vm_pgoff = 0;

	if (is_vb2_iommu_supported(buf->dev))
		ret = dma_mmap_attrs(buf->dev, vma, buf->pages, buf->dma_addr,
				     buf->size, &buf->dma_attrs);
	else
		ret = dma_mmap_attrs(buf->dev, vma, buf->vaddr, buf->dma_addr,
				     buf->size, &buf->dma_attrs);

	if (ret) {
		printk(KERN_ERR "Remapping memory failed, error: %d\n", ret);
		return ret;
	}

	vma->vm_flags		|= VM_DONTEXPAND | VM_RESERVED;
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	printk(KERN_DEBUG "%s: mapped dma addr 0x%08lx at 0x%08lx, size %ld\n",
		__func__, (unsigned long)buf->dma_addr, vma->vm_start,
		buf->size);

	return 0;
}

/*********************************************/
/*         DMABUF ops for exporters          */
/*********************************************/

struct vb2_dc_attachment {
	struct sg_table sgt;
	enum dma_data_direction dir;
	bool is_mapped;
};

static int vb2_dc_dmabuf_ops_attach(struct dma_buf *dbuf, struct device *dev,
	struct dma_buf_attachment *dbuf_attach)
{
	struct vb2_dc_attachment *attach;

	attach = kzalloc(sizeof *attach, GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	attach->dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;
}

static void vb2_dc_dmabuf_ops_detach(struct dma_buf *dbuf,
	struct dma_buf_attachment *db_attach)
{
	struct vb2_dc_buf *buf = dbuf->priv;
	struct vb2_dc_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	/* checking if scaterlist was ever mapped */
	if (attach->dir != DMA_NONE)
		dma_unmap_sg_attrs(db_attach->dev, sgt->sgl, sgt->nents,
				attach->dir, &buf->dma_attrs);
	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}

static struct sg_table *vb2_dc_dmabuf_ops_map(
	struct dma_buf_attachment *db_attach, enum dma_data_direction dir)
{
	struct dma_buf *dbuf = db_attach->dmabuf;
	struct vb2_dc_buf *buf = dbuf->priv;
	struct vb2_dc_attachment *attach = db_attach->priv;
	struct sg_table *sgt;
	struct scatterlist *rd, *wr;
	int ret;
	unsigned int i;

	/* return previously mapped sg table */
	if (attach->dir == dir && attach->is_mapped)
		return &attach->sgt;

	/* reattaching is not allowed */
	if (WARN_ON(attach->dir != DMA_NONE))
		return ERR_PTR(-EBUSY);

	sgt = &attach->sgt;

	/* Copy the buf->base_sgt scatter list to the attachment, as we can't
	 * map the same scatter list to multiple attachments at the same time.
	 */
	ret = sg_alloc_table(sgt, buf->sgt_base.orig_nents, GFP_KERNEL);
	if (ret)
		return ERR_PTR(-ENOMEM);

	rd = buf->sgt_base.sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	/*
	 * This exception considers the case that some process requests
	 * dmabuf import to 3d gpu(mali or sgx) driver.
	 * The 3d driver has its own iommu hardware unit and also iommu
	 * mapping table so the buffer imported into 3d driver doesn't need
	 * to be mapped with exynos drm-common iommu mapping table.
	 *
	 * Caution:
	 * - To ignore dma_map_sg call, caller driver should set
	 *	DMA_NONE flag with dma_buf_map_attachment call.
	 */
	if (dir != DMA_NONE) {
		/* mapping new sglist to the client */
		ret = dma_map_sg_attrs(db_attach->dev, sgt->sgl,
				sgt->orig_nents, dir, &buf->dma_attrs);
		if (ret <= 0) {
			printk(KERN_ERR "failed to map scatterlist\n");
			sg_free_table(sgt);
			return ERR_PTR(-EIO);
		}
	}

	attach->is_mapped = true;
	attach->dir = dir;
	db_attach->priv = attach;

	return sgt;
}

static void vb2_dc_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
	struct sg_table *sgt, enum dma_data_direction dir)
{
	/* nothing to be done here */
}

static void vb2_dc_dmabuf_ops_release(struct dma_buf *dbuf)
{
	/* drop reference obtained in vb2_dc_get_dmabuf */
	vb2_dc_put(dbuf->priv);
}

static void *vb2_dc_dmabuf_ops_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	struct vb2_dc_buf *buf = dbuf->priv;

	return buf->vaddr + pgnum * PAGE_SIZE;
}

static void *vb2_dc_dmabuf_ops_vmap(struct dma_buf *dbuf)
{
	struct vb2_dc_buf *buf = dbuf->priv;

	return buf->vaddr;
}

static int vb2_dc_dmabuf_ops_mmap(struct dma_buf *dbuf,
	struct vm_area_struct *vma)
{
	/* Dummy support for mmap */
	return -ENOTTY;
}

static struct dma_buf_ops vb2_dc_dmabuf_ops = {
	.attach = vb2_dc_dmabuf_ops_attach,
	.detach = vb2_dc_dmabuf_ops_detach,
	.map_dma_buf = vb2_dc_dmabuf_ops_map,
	.unmap_dma_buf = vb2_dc_dmabuf_ops_unmap,
	.kmap = vb2_dc_dmabuf_ops_kmap,
	.kmap_atomic = vb2_dc_dmabuf_ops_kmap,
	.vmap = vb2_dc_dmabuf_ops_vmap,
	.mmap = vb2_dc_dmabuf_ops_mmap,
	.release = vb2_dc_dmabuf_ops_release,
};

static struct dma_buf *vb2_dc_get_dmabuf(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct dma_buf *dbuf;

	dbuf = dma_buf_export(buf, &vb2_dc_dmabuf_ops, buf->size, 0);
	if (IS_ERR(dbuf))
		return NULL;

	/* dmabuf keeps reference to vb2 buffer */
	atomic_inc(&buf->refcount);

	return dbuf;
}

/*********************************************/
/*       callbacks for USERPTR buffers       */
/*********************************************/

static inline int vma_is_io(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & (VM_IO | VM_PFNMAP));
}

static int vb2_dc_get_user_pages(unsigned long start, struct page **pages,
	int n_pages, struct vm_area_struct *vma, int write)
{
	if (vma_is_io(vma)) {
		unsigned int i;

		for (i = 0; i < n_pages; ++i, start += PAGE_SIZE) {
			unsigned long pfn;
			int ret = follow_pfn(vma, start, &pfn);

			if (ret) {
				printk(KERN_ERR "no page for address %lu\n",
					start);
				return ret;
			}
			pages[i] = pfn_to_page(pfn);
		}
	} else {
		int n;

		n = get_user_pages(current, current->mm, start & PAGE_MASK,
			n_pages, write, 1, pages, NULL);
		/* negative error means that no page was pinned */
		n = max(n, 0);
		if (n != n_pages) {
			printk(KERN_ERR "got only %d of %d user pages\n",
				n, n_pages);
			while (n)
				put_page(pages[--n]);
			return -EFAULT;
		}
	}

	return 0;
}

static void vb2_dc_put_dirty_page(struct page *page)
{
	set_page_dirty_lock(page);
	put_page(page);
}

static void vb2_dc_put_userptr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	dma_unmap_sg(buf->dev, sgt->sgl, sgt->orig_nents, buf->dma_dir);
	if (!vma_is_io(buf->vma))
		vb2_dc_sgt_foreach_page(sgt, vb2_dc_put_dirty_page);

	sg_free_table(sgt);
	kfree(sgt);
	vb2_put_vma(buf->vma);
	kfree(buf);
}

static void *vb2_dc_get_userptr(void *alloc_ctx, unsigned long vaddr,
					unsigned long size, int write, int plane)
{
	struct vb2_dc_conf *conf = alloc_ctx;
	struct vb2_dc_buf *buf;
	unsigned long start;
	unsigned long end;
	unsigned long offset;
	struct page **pages;
	int n_pages;
	int ret = 0;
	struct vm_area_struct *vma;
	struct sg_table *sgt;
	unsigned long contig_size;
	unsigned long dma_align = dma_get_cache_alignment();

	/*
	 * DMA transfers are not reliable to buffers which
	 * are not cache line aligned!
	 */
	if (vaddr & (dma_align - 1)) {
		printk(KERN_ERR "user ptr buffer must be aligned to %d bytes\n",
		       (int) dma_align);
		return ERR_PTR(-EINVAL);
	}

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = conf->dev;
	buf->dma_dir = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	start = vaddr & PAGE_MASK;
	offset = vaddr & ~PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);
	n_pages = (end - start) >> PAGE_SHIFT;

	pages = kmalloc(n_pages * sizeof pages[0], GFP_KERNEL);
	if (!pages) {
		ret = -ENOMEM;
		printk(KERN_ERR "failed to allocate pages table\n");
		goto fail_buf;
	}

	/* current->mm->mmap_sem is taken by videobuf2 core */
	vma = find_vma(current->mm, vaddr);
	if (!vma) {
		printk(KERN_ERR "no vma for address %lu\n", vaddr);
		ret = -EFAULT;
		goto fail_pages;
	}

	if ((vma->vm_end < vaddr + size) && (vma_is_io(vma))) {
		printk(KERN_ERR "vma at %lu is too small for %lu bytes\n",
			vaddr, size);
		ret = -EFAULT;
		goto fail_pages;
	}

	buf->vma = vb2_get_vma(vma);
	if (!buf->vma) {
		printk(KERN_ERR "failed to copy vma\n");
		ret = -ENOMEM;
		goto fail_pages;
	}

	/* extract page list from userspace mapping */
	ret = vb2_dc_get_user_pages(start, pages, n_pages, vma, write);
	if (ret) {
		printk(KERN_ERR "failed to get user pages\n");
		goto fail_vma;
	}

	sgt = kzalloc(sizeof *sgt, GFP_KERNEL);
	if (!sgt) {
		printk(KERN_ERR "failed to allocate sg table\n");
		ret = -ENOMEM;
		goto fail_get_user_pages;
	}

	ret = sg_alloc_table_from_pages(sgt, pages, n_pages,
		offset, size, GFP_KERNEL);
	if (ret) {
		printk(KERN_ERR "failed to initialize sg table\n");
		goto fail_sgt;
	}

	/* pages are no longer needed */
	kfree(pages);
	pages = NULL;

	sgt->nents = dma_map_sg(buf->dev, sgt->sgl, sgt->orig_nents,
		buf->dma_dir);
	if (sgt->nents <= 0) {
		printk(KERN_ERR "failed to map scatterlist\n");
		ret = -EIO;
		goto fail_sgt_init;
	}

	contig_size = vb2_dc_get_contiguous_size(sgt);
	if (contig_size < size) {
		printk(KERN_ERR "contiguous mapping is too small %lu/%lu\n",
			contig_size, size);
		ret = -EFAULT;
		goto fail_map_sg;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->size = size;
	buf->dma_sgt = sgt;

	return buf;

fail_map_sg:
	dma_unmap_sg(buf->dev, sgt->sgl, sgt->nents, buf->dma_dir);

fail_sgt_init:
	if (!vma_is_io(buf->vma))
		vb2_dc_sgt_foreach_page(sgt, put_page);
	sg_free_table(sgt);

fail_sgt:
	kfree(sgt);

fail_get_user_pages:
	if (pages && !vma_is_io(buf->vma))
		while (n_pages)
			put_page(pages[--n_pages]);

fail_vma:
	vb2_put_vma(buf->vma);

fail_pages:
	kfree(pages); /* kfree is NULL-proof */

fail_buf:
	kfree(buf);

	return ERR_PTR(ret);
}

/*********************************************/
/*       callbacks for DMABUF buffers        */
/*********************************************/

static int vb2_dc_map_dmabuf(void *mem_priv, int plane)
{
	struct vb2_dc_buf *buf = mem_priv;
	struct sg_table *sgt;
	unsigned long contig_size;

	if (WARN_ON(!buf->db_attach)) {
		printk(KERN_ERR "trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		printk(KERN_ERR "dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
	if (IS_ERR_OR_NULL(sgt)) {
		printk(KERN_ERR "Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	/* checking if dmabuf is big enough to store contiguous chunk */
	contig_size = vb2_dc_get_contiguous_size(sgt);
	if (contig_size < buf->size) {
		printk(KERN_ERR "contiguous chunk is too small %lu/%lu b\n",
			contig_size, buf->size);
		dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);
		return -EFAULT;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;

	return 0;
}

static void vb2_dc_unmap_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (WARN_ON(!buf->db_attach)) {
		printk(KERN_ERR "trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		printk(KERN_ERR "dmabuf buffer is already unpinned\n");
		return;
	}

	dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);

	buf->dma_addr = 0;
	buf->dma_sgt = NULL;
}

static void vb2_dc_detach_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	if (buf->vaddr)
		dma_buf_vunmap(buf->db_attach->dmabuf, buf->vaddr);

	/* if vb2 works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_addr))
		vb2_dc_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

static void *vb2_dc_attach_dmabuf(void *alloc_ctx, struct dma_buf *dbuf,
	unsigned long size, int write)
{
	struct vb2_dc_conf *conf = alloc_ctx;
	struct vb2_dc_buf *buf;
	struct dma_buf_attachment *dba;

	if (dbuf->size < size)
		return ERR_PTR(-EFAULT);

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = conf->dev;
	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		printk(KERN_ERR "failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}

	buf->dma_dir = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	buf->size = size;
	buf->db_attach = dba;

	return buf;
}

/*********************************************/
/*       DMA CONTIG exported functions       */
/*********************************************/

const struct vb2_mem_ops vb2_dma_contig_memops = {
	.alloc		= vb2_dc_alloc,
	.put		= vb2_dc_put,
	.get_dmabuf	= vb2_dc_get_dmabuf,
	.cookie		= vb2_dc_cookie,
	.vaddr		= vb2_dc_vaddr,
	.mmap		= vb2_dc_mmap,
	.get_userptr	= vb2_dc_get_userptr,
	.put_userptr	= vb2_dc_put_userptr,
	.prepare	= vb2_dc_prepare,
	.finish		= vb2_dc_finish,
	.map_dmabuf	= vb2_dc_map_dmabuf,
	.unmap_dmabuf	= vb2_dc_unmap_dmabuf,
	.attach_dmabuf	= vb2_dc_attach_dmabuf,
	.detach_dmabuf	= vb2_dc_detach_dmabuf,
	.num_users	= vb2_dc_num_users,
};
EXPORT_SYMBOL_GPL(vb2_dma_contig_memops);

void *vb2_dma_contig_init_ctx(struct device *dev)
{
	struct vb2_dc_conf *conf;

	conf = kzalloc(sizeof *conf, GFP_KERNEL);
	if (!conf)
		return ERR_PTR(-ENOMEM);

	conf->dev = dev;

	return conf;
}
EXPORT_SYMBOL_GPL(vb2_dma_contig_init_ctx);

void vb2_dma_contig_cleanup_ctx(void *alloc_ctx)
{
	kfree(alloc_ctx);
}
EXPORT_SYMBOL_GPL(vb2_dma_contig_cleanup_ctx);

void vb2_dma_contig_set_cacheable(void *alloc_ctx, bool cacheable)
{

	struct vb2_dc_conf *conf = alloc_ctx;

	if (cacheable)
		conf->flags |= VB2_SET_CACHEABLE;
	else
		conf->flags &= ~VB2_SET_CACHEABLE;
}
EXPORT_SYMBOL(vb2_dma_contig_set_cacheable);

MODULE_DESCRIPTION("DMA-contig memory handling routines for videobuf2");
MODULE_AUTHOR("Pawel Osciak <pawel@osciak.com>");
MODULE_LICENSE("GPL");
