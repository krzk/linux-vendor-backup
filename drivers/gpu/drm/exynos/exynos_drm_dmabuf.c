/* exynos_drm_dmabuf.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * Author: Inki Dae <inki.dae@samsung.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"
#include "drm.h"
#include "exynos_drm.h"
#include "exynos_drm_drv.h"
#include "exynos_drm_gem.h"
#include "exynos_drm_iommu.h"

#include <linux/dma-buf.h>
#include <linux/dmabuf-sync.h>
#include <linux/rmap.h>
#include <linux/ksm.h>

struct exynos_drm_dmabuf_attachment {
	struct sg_table sgt;
	enum dma_data_direction dir;
	bool is_mapped;
};

static int exynos_gem_attach_dma_buf(struct dma_buf *dmabuf,
					struct device *dev,
					struct dma_buf_attachment *attach)
{
	struct exynos_drm_dmabuf_attachment *exynos_attach;

	exynos_attach = kzalloc(sizeof(*exynos_attach), GFP_KERNEL);
	if (!exynos_attach)
		return -ENOMEM;

	exynos_attach->dir = DMA_NONE;
	attach->priv = exynos_attach;

	return 0;
}

static void exynos_gem_detach_dma_buf(struct dma_buf *dmabuf,
					struct dma_buf_attachment *attach)
{
	struct exynos_drm_dmabuf_attachment *exynos_attach = attach->priv;
	struct exynos_drm_gem_obj *gem_obj = dmabuf->priv;
	struct exynos_drm_gem_buf *buf = gem_obj->buffer;
	struct sg_table *sgt;

	if (!exynos_attach)
		return;

	sgt = &exynos_attach->sgt;

	if (exynos_attach->dir != DMA_NONE)
		dma_unmap_sg_attrs(attach->dev, sgt->sgl, sgt->nents,
				exynos_attach->dir, &buf->dma_attrs);

	sg_free_table(sgt);
	kfree(exynos_attach);
	attach->priv = NULL;
}

static struct sg_table *
		exynos_gem_map_dma_buf(struct dma_buf_attachment *attach,
					enum dma_data_direction dir)
{
	struct exynos_drm_dmabuf_attachment *exynos_attach = attach->priv;
	struct exynos_drm_gem_obj *gem_obj = attach->dmabuf->priv;
	struct drm_device *dev = gem_obj->base.dev;
	struct exynos_drm_gem_buf *buf;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt = NULL;
	unsigned int i;
	int nents, ret;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/* just return current sgt if already requested. */
	if (exynos_attach->dir == dir && exynos_attach->is_mapped)
		return &exynos_attach->sgt;

	buf = gem_obj->buffer;
	if (!buf) {
		DRM_ERROR("buffer is null.\n");
		return ERR_PTR(-ENOMEM);
	}

	sgt = &exynos_attach->sgt;

	ret = sg_alloc_table(sgt, buf->sgt->orig_nents, GFP_KERNEL);
	if (ret) {
		DRM_ERROR("failed to alloc sgt.\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_lock(&dev->struct_mutex);

	rd = buf->sgt->sgl;
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
		nents = dma_map_sg_attrs(attach->dev, sgt->sgl,
						sgt->orig_nents, dir,
						&buf->dma_attrs);
		if (!nents) {
			DRM_ERROR("failed to map sgl with iommu.\n");
			sgt = ERR_PTR(-EIO);
			goto err_unlock;
		}
	}

	exynos_attach->is_mapped = true;
	exynos_attach->dir = dir;
	attach->priv = exynos_attach;

	DRM_DEBUG_PRIME("buffer size = 0x%lx\n", buf->size);

err_unlock:
	mutex_unlock(&dev->struct_mutex);
	return sgt;
}

static void exynos_gem_unmap_dma_buf(struct dma_buf_attachment *attach,
						struct sg_table *sgt,
						enum dma_data_direction dir)
{
	/* Nothing to do. */
}

static int exynos_gem_begin_cpu_access(struct dma_buf *dmabuf, size_t start,
					size_t len, enum dma_data_direction dir)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = dmabuf->priv;
	struct exynos_drm_gem_buf *buf = exynos_gem_obj->buffer;
	struct drm_device *drm_dev = exynos_gem_obj->base.dev;

	if (dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, &buf->dma_attrs))
		return 0;

	/* TODO. need to optimize cache operation. */
	dma_sync_sg_for_cpu(drm_dev->dev, buf->sgt->sgl, buf->sgt->orig_nents,
				dir);

	return 0;
}

static void exynos_gem_end_cpu_access(struct dma_buf *dmabuf, size_t start,
					size_t len, enum dma_data_direction dir)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = dmabuf->priv;
	struct exynos_drm_gem_buf *buf = exynos_gem_obj->buffer;
	struct drm_device *drm_dev = exynos_gem_obj->base.dev;

	if (dma_get_attr(DMA_ATTR_SKIP_CPU_SYNC, &buf->dma_attrs))
		return;

	/* TODO. need to optimize cache operation. */
	dma_sync_sg_for_device(drm_dev->dev, buf->sgt->sgl,
				buf->sgt->orig_nents, dir);
}

static void exynos_dmabuf_release(struct dma_buf *dmabuf)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = dmabuf->priv;
	struct drm_gem_object *obj = &exynos_gem_obj->base;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/*
	 * exynos_dmabuf_release() call means that file object's
	 * f_count is 0 and it calls drm_gem_object_handle_unreference()
	 * to drop the references that these values had been increased
	 * at drm_prime_handle_to_fd()
	 */
	if (obj->export_dma_buf == dmabuf) {
		obj->export_dma_buf = NULL;

		/*
		 * drop this gem object refcount to release allocated buffer
		 * and resources.
		 */
		drm_gem_object_unreference_unlocked(obj);
		return;
	}
}

static void *exynos_gem_dmabuf_kmap_atomic(struct dma_buf *dma_buf,
						unsigned long page_num)
{
	/* TODO */

	return NULL;
}

static void exynos_gem_dmabuf_kunmap_atomic(struct dma_buf *dma_buf,
						unsigned long page_num,
						void *addr)
{
	/* TODO */
}

static void *exynos_gem_dmabuf_kmap(struct dma_buf *dma_buf,
					unsigned long page_num)
{
	/* TODO */

	return NULL;
}

static void exynos_gem_dmabuf_kunmap(struct dma_buf *dma_buf,
					unsigned long page_num, void *addr)
{
	/* TODO */
}

static int exynos_gem_dmabuf_mmap(struct dma_buf *dma_buf,
	struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = dma_buf->priv;
	struct drm_device *dev = obj->dev;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	mutex_unlock(&dev->struct_mutex);
	if (ret < 0)
		return ret;

	return exynos_drm_gem_mmap_obj(obj, vma);
}

static void *exynos_gem_dmabuf_vmap(struct dma_buf *dmabuf)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = dmabuf->priv;
	struct drm_device *drm_dev = exynos_gem_obj->base.dev;
	struct exynos_drm_gem_buf *buffer = exynos_gem_obj->buffer;

	if (is_drm_iommu_supported(drm_dev)) {
		unsigned int nr_pages = buffer->size >> PAGE_SHIFT;

		buffer->kvaddr = vmap(buffer->pages, nr_pages, VM_MAP,
					pgprot_writecombine(PAGE_KERNEL));
	} else {
		phys_addr_t dma_addr = buffer->dma_addr;

		if (dma_addr)
			buffer->kvaddr = phys_to_virt(dma_addr);
		else
			buffer->kvaddr = (void __iomem *)NULL;
	}

	if (!buffer->kvaddr) {
		DRM_ERROR("Failed to vmap pages to kernel space.\n");
		return ERR_PTR(-ENOMEM);
	}

	return buffer->kvaddr;
}

static void exynos_gem_dmabuf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = dmabuf->priv;
	struct drm_device *drm_dev = exynos_gem_obj->base.dev;
	struct exynos_drm_gem_buf *buffer = exynos_gem_obj->buffer;

	if (is_drm_iommu_supported(drm_dev) && buffer->kvaddr)
		if (buffer->kvaddr == vaddr) {
			vunmap(buffer->kvaddr);
			buffer->kvaddr = NULL;
		}
}

#ifdef CONFIG_DMABUF_SYNC_PROFILE
static void exynos_gem_dmabuf_munmap(struct dma_buf *dmabuf)
{
	struct dmabuf_sync_reservation *robj = dmabuf->sync;
	struct vm_area_struct *vma = robj->vma;

	if (!vma)
		return;

	down_read(&current->mm->mmap_sem);

	if (unlikely(vma->vm_flags & VM_NONLINEAR)) {
		struct zap_details details = {
			.nonlinear_vma = vma,
			.last_index = ULONG_MAX,
		};

		zap_page_range(vma, vma->vm_start,
				vma->vm_end - vma->vm_start, &details);
	} else
		zap_page_range(vma, vma->vm_start, vma->vm_end - vma->vm_start,
				NULL);

	robj->vma = NULL;
	robj->dirty = 0;
	up_read(&current->mm->mmap_sem);
}
#endif

static struct dma_buf_ops exynos_dmabuf_ops = {
	.attach			= exynos_gem_attach_dma_buf,
	.detach			= exynos_gem_detach_dma_buf,
	.map_dma_buf		= exynos_gem_map_dma_buf,
	.unmap_dma_buf		= exynos_gem_unmap_dma_buf,
	.begin_cpu_access	= exynos_gem_begin_cpu_access,
	.end_cpu_access		= exynos_gem_end_cpu_access,
	.kmap			= exynos_gem_dmabuf_kmap,
	.kmap_atomic		= exynos_gem_dmabuf_kmap_atomic,
	.kunmap			= exynos_gem_dmabuf_kunmap,
	.kunmap_atomic		= exynos_gem_dmabuf_kunmap_atomic,
	.mmap			= exynos_gem_dmabuf_mmap,
	.release		= exynos_dmabuf_release,
	.vmap			= exynos_gem_dmabuf_vmap,
	.vunmap			= exynos_gem_dmabuf_vunmap,
#ifdef CONFIG_DMABUF_SYNC_PROFILE
	.munmap			= exynos_gem_dmabuf_munmap,
#endif
};

struct dma_buf *exynos_dmabuf_prime_export(struct drm_device *drm_dev,
				struct drm_gem_object *obj, int flags)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = to_exynos_gem_obj(obj);

	return dma_buf_export(exynos_gem_obj, &exynos_dmabuf_ops,
				exynos_gem_obj->base.size, O_RDWR);
}

struct drm_gem_object *exynos_dmabuf_prime_import(struct drm_device *drm_dev,
				struct dma_buf *dma_buf)
{
	struct dma_buf_attachment *attach = NULL, *db_attach;
	struct sg_table *sgt;
	struct scatterlist *sgl;
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct exynos_drm_gem_buf *buffer;
	int ret;

	DRM_DEBUG_PRIME("%s\n", __FILE__);

	/* is this one of own objects? */
	if (dma_buf->ops == &exynos_dmabuf_ops) {
		struct drm_gem_object *obj;

		exynos_gem_obj = dma_buf->priv;
		obj = &exynos_gem_obj->base;

		/* is it from our device? */
		if (obj->dev == drm_dev) {
			/*
			 * Importing dmabuf exported from out own gem increases
			 * refcount on gem itself instead of f_count of dmabuf.
			 */
			drm_gem_object_reference(obj);
			dma_buf_put(dma_buf);
			return obj;
		}
	}

	/* check it is already imported */
	mutex_lock(&dma_buf->lock);
	list_for_each_entry(db_attach, &dma_buf->attachments, node) {
		if (db_attach->dev == drm_dev->dev) {
			/* found attachment for same device */
			attach = db_attach;
			break;
		}
	}
	mutex_unlock(&dma_buf->lock);

	if (attach) {
		struct drm_gem_object *obj;

		exynos_gem_obj = attach->importer_priv;
		obj = &exynos_gem_obj->base;

		DRM_DEBUG_PRIME("found dmabuf attach with exynos gem[%p]\n",
				exynos_gem_obj);

		drm_gem_object_reference(obj);
		dma_buf_put(dma_buf);
		return obj;
	}

	attach = dma_buf_attach(dma_buf, drm_dev->dev);
	if (IS_ERR(attach))
		return ERR_PTR(-EINVAL);

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sgt)) {
		ret = PTR_ERR(sgt);
		goto err_buf_detach;
	}

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		DRM_ERROR("failed to allocate exynos_drm_gem_buf.\n");
		ret = -ENOMEM;
		goto err_unmap_attach;
	}

	exynos_gem_obj = exynos_drm_gem_init(drm_dev, dma_buf->size);
	if (!exynos_gem_obj) {
		ret = -ENOMEM;
		goto err_free_buffer;
	}

	sgl = sgt->sgl;

	buffer->size = dma_buf->size;
	buffer->dma_addr = sg_dma_address(sgl);

	if (sgt->nents == 1) {
		/* always physically continuous memory if sgt->nents is 1. */
		exynos_gem_obj->flags |= EXYNOS_BO_CONTIG;
	} else {
		/*
		 * this case could be CONTIG or NONCONTIG type but for now
		 * sets NONCONTIG.
		 * TODO. we have to find a way that exporter can notify
		 * the type of its own buffer to importer.
		 */
		exynos_gem_obj->flags |= EXYNOS_BO_NONCONTIG;
	}

	exynos_gem_obj->buffer = buffer;
	buffer->sgt = sgt;
	exynos_gem_obj->base.import_attach = attach;
	attach->importer_priv = exynos_gem_obj;

	DRM_DEBUG_PRIME("dma_addr = 0x%x, size = 0x%lx\n", buffer->dma_addr,
								buffer->size);

	return &exynos_gem_obj->base;

err_free_buffer:
	kfree(buffer);
	buffer = NULL;
err_unmap_attach:
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
err_buf_detach:
	dma_buf_detach(dma_buf, attach);
	return ERR_PTR(ret);
}

int exynos_drm_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle)
{
	int ret;

	ret = drm_gem_prime_fd_to_handle(dev, file_priv, prime_fd, handle);
	if (ret < 0)
		goto out;

	exynos_drm_gem_register_pid(file_priv);

out:
	return ret;
}

MODULE_AUTHOR("Inki Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC DRM DMABUF Module");
MODULE_LICENSE("GPL");
