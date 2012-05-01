/* exynos_drm_gem.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
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

#include <drm/exynos_drm.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/dmabuf-sync.h>
#include <linux/rmap.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_gem.h"
#include "exynos_drm_buf.h"

static unsigned int convert_to_vm_err_msg(int msg)
{
	unsigned int out_msg;

	switch (msg) {
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
		out_msg = VM_FAULT_NOPAGE;
		break;

	case -ENOMEM:
		out_msg = VM_FAULT_OOM;
		break;

	default:
		out_msg = VM_FAULT_SIGBUS;
		break;
	}

	return out_msg;
}

static int check_gem_flags(unsigned int flags)
{
	if (flags & ~(EXYNOS_BO_MASK)) {
		DRM_ERROR("invalid flags.\n");
		return -EINVAL;
	}

	return 0;
}

static int check_cache_flags(unsigned int flags)
{
	if (flags & ~(EXYNOS_DRM_CACHE_SEL_MASK | EXYNOS_DRM_CACHE_OP_MASK)) {
		DRM_ERROR("invalid flags.\n");
		return -EINVAL;
	}

	return 0;
}

static void update_vm_cache_attr(struct exynos_drm_gem_obj *obj,
					struct vm_area_struct *vma)
{
	DRM_DEBUG_KMS("flags = 0x%x\n", obj->flags);

	/* non-cachable as default. */
	if (obj->flags & EXYNOS_BO_CACHABLE)
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	else if (obj->flags & EXYNOS_BO_WC)
		vma->vm_page_prot =
			pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	else
		vma->vm_page_prot =
			pgprot_noncached(vm_get_page_prot(vma->vm_flags));
}

static unsigned long roundup_gem_size(unsigned long size, unsigned int flags)
{
	/* TODO */

	return roundup(size, PAGE_SIZE);
}

static int exynos_drm_gem_map_buf(struct drm_gem_object *obj,
					struct vm_area_struct *vma,
					unsigned long f_vaddr,
					pgoff_t page_offset)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = to_exynos_gem_obj(obj);
	struct exynos_drm_gem_buf *buf = exynos_gem_obj->buffer;
	struct scatterlist *sgl;
	unsigned long pfn;
	int i;

	if (!buf->sgt)
		return -EINTR;

	if (page_offset >= (buf->size >> PAGE_SHIFT)) {
		DRM_ERROR("invalid page offset\n");
		return -EINVAL;
	}

	sgl = buf->sgt->sgl;
	for_each_sg(buf->sgt->sgl, sgl, buf->sgt->nents, i) {
		if (page_offset < (sgl->length >> PAGE_SHIFT))
			break;
		page_offset -=	(sgl->length >> PAGE_SHIFT);
	}

	pfn = __phys_to_pfn(sg_phys(sgl)) + page_offset;

	return vm_insert_mixed(vma, f_vaddr, pfn);
}

static int exynos_drm_gem_handle_create(struct drm_gem_object *obj,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	int ret;

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv, obj, handle);
	if (ret)
		return ret;

	DRM_DEBUG_KMS("gem handle = 0x%x\n", *handle);

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

void exynos_drm_gem_register_pid(struct drm_file *file_priv)
{
	struct drm_exynos_file_private *driver_priv = file_priv->driver_priv;

	if (!driver_priv->pid && !driver_priv->tgid) {
		driver_priv->pid = task_pid_nr(current);
		driver_priv->tgid = task_tgid_nr(current);
	} else {
		if (driver_priv->pid != task_pid_nr(current))
			DRM_DEBUG_KMS("wrong pid: %ld, %ld\n",
					(unsigned long)driver_priv->pid,
					(unsigned long)task_pid_nr(current));
		if (driver_priv->tgid != task_tgid_nr(current))
			DRM_DEBUG_KMS("wrong tgid: %ld, %ld\n",
					(unsigned long)driver_priv->tgid,
					(unsigned long)task_tgid_nr(current));
	}
}

void exynos_drm_gem_destroy(struct exynos_drm_gem_obj *exynos_gem_obj)
{
	struct drm_gem_object *obj;
	struct exynos_drm_gem_buf *buf;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	obj = &exynos_gem_obj->base;
	buf = exynos_gem_obj->buffer;

	DRM_DEBUG_KMS("handle count = %d\n", atomic_read(&obj->handle_count));

	DRM_DEBUG("%s:obj[0x%x]addr[0x%x]ref[%d][%s]\n", __func__,
		(int)obj, (int)exynos_gem_obj->buffer->dma_addr,
		atomic_read(&obj->handle_count),
		obj->import_attach ? "imported" : "free");

	/*
	 * do not release memory region from exporter.
	 *
	 * the region will be released by exporter
	 * once dmabuf's refcount becomes 0.
	 */
	if (obj->import_attach)
		goto out;

	exynos_drm_free_buf(obj->dev, exynos_gem_obj->flags, buf);

out:
	exynos_drm_fini_buf(obj->dev, buf);
	exynos_gem_obj->buffer = NULL;

	if (obj->map_list.map)
		drm_gem_free_mmap_offset(obj);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(exynos_gem_obj);
	exynos_gem_obj = NULL;
}

unsigned long exynos_drm_gem_get_size(struct drm_device *dev,
						unsigned int gem_handle,
						struct drm_file *file_priv)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(dev, file_priv, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return 0;
	}

	exynos_gem_obj = to_exynos_gem_obj(obj);

	drm_gem_object_unreference_unlocked(obj);

	return exynos_gem_obj->buffer->size;
}


struct exynos_drm_gem_obj *exynos_drm_gem_init(struct drm_device *dev,
						      unsigned long size)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	exynos_gem_obj = kzalloc(sizeof(*exynos_gem_obj), GFP_KERNEL);
	if (!exynos_gem_obj) {
		DRM_ERROR("failed to allocate exynos gem object\n");
		return NULL;
	}

	exynos_gem_obj->size = size;
	obj = &exynos_gem_obj->base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(exynos_gem_obj);
		return NULL;
	}

	exynos_gem_obj->pid = task_pid_nr(current);
	exynos_gem_obj->tgid = task_tgid_nr(current);

	DRM_DEBUG_KMS("created file object = 0x%x\n", (unsigned int)obj->filp);

	return exynos_gem_obj;
}

struct exynos_drm_gem_obj *exynos_drm_gem_create(struct drm_device *dev,
						unsigned int flags,
						unsigned long size)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct exynos_drm_gem_buf *buf;
	unsigned long packed_size = size;
	int ret;

	if (!size) {
		DRM_ERROR("invalid size.\n");
		return ERR_PTR(-EINVAL);
	}

	size = roundup_gem_size(size, flags);
	DRM_DEBUG_KMS("%s\n", __FILE__);

	ret = check_gem_flags(flags);
	if (ret)
		return ERR_PTR(ret);

	buf = exynos_drm_init_buf(dev, size);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	exynos_gem_obj = exynos_drm_gem_init(dev, size);
	if (!exynos_gem_obj) {
		ret = -ENOMEM;
		goto err_fini_buf;
	}

	exynos_gem_obj->packed_size = packed_size;
	exynos_gem_obj->buffer = buf;

	/* set memory type and cache attribute from user side. */
	exynos_gem_obj->flags = flags;

	ret = exynos_drm_alloc_buf(dev, buf, flags);
	if (ret < 0)
		goto err_gem_fini;

	return exynos_gem_obj;

err_gem_fini:
	drm_gem_object_release(&exynos_gem_obj->base);
	kfree(exynos_gem_obj);
err_fini_buf:
	exynos_drm_fini_buf(dev, buf);
	return ERR_PTR(ret);
}

int exynos_drm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	struct drm_exynos_gem_create *args = data;
	struct exynos_drm_gem_obj *exynos_gem_obj;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	exynos_gem_obj = exynos_drm_gem_create(dev, args->flags, args->size);
	if (IS_ERR(exynos_gem_obj))
		return PTR_ERR(exynos_gem_obj);

	ret = exynos_drm_gem_handle_create(&exynos_gem_obj->base, file_priv,
			&args->handle);
	if (ret) {
		exynos_drm_gem_destroy(exynos_gem_obj);
		return ret;
	}

	exynos_drm_gem_register_pid(file_priv);

	DRM_DEBUG("%s:hdl[%d]sz[%d]f[0x%x]obj[0x%x]addr[0x%x]\n",
		__func__,args->handle, (int)args->size, args->flags,
		(int)&exynos_gem_obj->base,
		(int)exynos_gem_obj->buffer->dma_addr);

	return 0;
}

dma_addr_t *exynos_drm_gem_get_dma_addr(struct drm_device *dev,
					unsigned int gem_handle,
					struct drm_file *filp)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return ERR_PTR(-EINVAL);
	}

	exynos_gem_obj = to_exynos_gem_obj(obj);

	DRM_DEBUG("%s:hdl[%d]obj[0x%x]addr[0x%x]\n",
		__func__,gem_handle, (int)obj,
		(int)exynos_gem_obj->buffer->dma_addr);

	return &exynos_gem_obj->buffer->dma_addr;
}

phys_addr_t exynos_drm_gem_get_phys_addr(struct drm_device *dev,
					unsigned int gem_handle,
					struct drm_file *filp)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_gem_object *obj;
	struct sg_table *sgt;

	obj = drm_gem_object_lookup(dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return 0;
	}

	exynos_gem_obj = to_exynos_gem_obj(obj);
	sgt = exynos_gem_obj->buffer->sgt;

	if ((exynos_gem_obj->flags & EXYNOS_BO_NONCONTIG) || !sgt) {
		drm_gem_object_unreference_unlocked(obj);
		DRM_ERROR("failed to get physical address.\n");
		return 0;
	}

	return sg_phys(sgt->sgl);
}

int exynos_drm_gem_put_dma_addr(struct drm_device *dev,
					unsigned int gem_handle,
					struct drm_file *filp)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return -EINVAL;
	}

	exynos_gem_obj = to_exynos_gem_obj(obj);

	drm_gem_object_unreference_unlocked(obj);

	DRM_DEBUG("%s:hdl[%d]obj[0x%x]\n",
		__func__,gem_handle, (int)obj);

	/*
	 * decrease obj->refcount one more time because we has already
	 * increased it at exynos_drm_gem_get_dma_addr().
	 */
	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

void *exynos_drm_gem_get_dmabuf(struct drm_device *dev,
					unsigned int gem_handle,
					struct drm_file *filp)
{
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return ERR_PTR(-EINVAL);
	}

	drm_gem_object_unreference_unlocked(obj);

	return obj->export_dma_buf;
}

int exynos_drm_gem_map_offset_ioctl(struct drm_device *dev, void *data,
				    struct drm_file *file_priv)
{
	struct drm_exynos_gem_map_off *args = data;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	DRM_DEBUG_KMS("handle = 0x%x, offset = 0x%lx\n",
			args->handle, (unsigned long)args->offset);

	if (!(dev->driver->driver_features & DRIVER_GEM)) {
		DRM_ERROR("does not support GEM.\n");
		return -ENODEV;
	}

	return exynos_drm_gem_dumb_map_offset(file_priv, dev, args->handle,
			&args->offset);
}

static struct drm_file *exynos_drm_find_drm_file(struct drm_device *drm_dev,
							struct file *filp)
{
	struct drm_file *file_priv;

	/* find drm_file from filelist. */
	list_for_each_entry(file_priv, &drm_dev->filelist, lhead)
		if (file_priv->filp == filp)
			return file_priv;

	WARN_ON(1);

	return ERR_PTR(-EFAULT);
}

static int exynos_drm_gem_mmap_buffer(struct file *filp,
				      struct vm_area_struct *vma)
{
	struct exynos_drm_gem_obj *exynos_gem_obj = filp->private_data;
	struct drm_gem_object *obj = &exynos_gem_obj->base;
	struct exynos_drm_gem_buf *buffer;
	struct drm_device *dev = obj->dev;
	struct drm_file *file_priv;
	unsigned long vm_size;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	vma->vm_flags |= (VM_IO | VM_RESERVED);
#ifdef CONFIG_DMABUF_SYNC_PROFILE
	vma->vm_flags |= VM_MIXEDMAP;
#endif
	vma->vm_private_data = obj;
	vma->vm_ops = obj->dev->driver->gem_vm_ops;

	file_priv = exynos_drm_find_drm_file(dev, filp);
	if (IS_ERR(file_priv))
		return PTR_ERR(file_priv);

	/* restore it to drm_file. */
	filp->private_data = file_priv;

	/* restore it to drivier's fops. */
	filp->f_op = fops_get(dev->driver->fops);

	/* drm_file is used by drm_vm_open/close_lock functions */
	vma->vm_file->private_data = filp->private_data;

	vm_size = vma->vm_end - vma->vm_start;

	/*
	 * a buffer contains information to physically continuous memory
	 * allocated by user request or at framebuffer creation.
	 */
	buffer = exynos_gem_obj->buffer;

	/* check if user-requested size is valid. */
	if (vm_size > buffer->size)
		return -EINVAL;

#ifdef CONFIG_TIMA_IOMMU_OPT
	if (vma->vm_end - vma->vm_start) {
		/* iommu optimization- needs to be turned ON from
		 * the tz side.
		 */
		cpu_v7_tima_iommu_opt(vma->vm_start, vma->vm_end, (unsigned long)vma->vm_mm->pgd);
	}
#endif  /* CONFIG_TIMA_IOMMU_OPT */

	if (exynos_gem_obj->flags & EXYNOS_BO_CACHABLE)
		dma_set_attr(DMA_ATTR_NON_CONSISTENT, &buffer->dma_attrs);

#ifdef CONFIG_DMABUF_SYNC_PROFILE
	/*
	 * Do not map user space with physical memory so that the pages
	 * can be mapped with user space at page fault handler.
	 * This page fault is used to collect profiling data to memory access
	 * by CPU.
	 */
	if (!dma_get_attr(DMA_ATTR_NON_CONSISTENT, &buffer->dma_attrs))
		vma->vm_page_prot = dma_get_attr(DMA_ATTR_WRITE_COMBINE,
						&buffer->dma_attrs) ?
			pgprot_writecombine(vma->vm_page_prot) :
			pgprot_dmacoherent(vma->vm_page_prot);
#else
	ret = dma_mmap_attrs(dev->dev, vma, buffer->pages,
				buffer->dma_addr, buffer->size,
				&buffer->dma_attrs);
	if (ret < 0) {
		DRM_ERROR("failed to mmap.\n");
		return ret;
	}
#endif

	/*
	 * drm_gem_object_unreference and drm_vm_close_locked functions
	 * will be called by drm_vm_close when munmap is called by user.
	 */
	drm_gem_object_reference(obj);

	drm_vm_open_locked(dev, vma);

	return 0;
}

static const struct file_operations exynos_drm_gem_fops = {
	.mmap = exynos_drm_gem_mmap_buffer,
};

int exynos_drm_gem_mmap_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct drm_exynos_gem_mmap *args = data;
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_gem_object *obj;
	struct mm_struct *mm = current->mm;
	unsigned long addr;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (!(dev->driver->driver_features & DRIVER_GEM)) {
		DRM_ERROR("does not support GEM.\n");
		return -ENODEV;
	}

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return -EINVAL;
	}

	/*
	 * This function has to use do_mmap() instead of vm_mmap() for deadlock
	 * problem. The sys_munmap() downs mmap_sem first, then locks drm
	 * struct_mutex in dma_buf_release() at last.
	 * So this function also has to down mmap_sem before locks drm
	 * struct_mutex for lock pair.
	 */
	down_write(&mm->mmap_sem);
	/*
	 * We have to use gem object fops and gem object for specific mapper,
	 * but do_mmap() can deliver only filp.
	 * So set filp->f_op to gem object fops, filp->private_data to gem
	 * object for temporary way, then restore them in f_op->mmap().
	 * It is important to keep lock until restoration the settings
	 * to prevent others from misuse of filp->f_op or filp->private_data.
	 */
	mutex_lock(&dev->struct_mutex);

	file_priv->filp->f_op = &exynos_drm_gem_fops;
	exynos_gem_obj = to_exynos_gem_obj(obj);
	file_priv->filp->private_data = exynos_gem_obj;

	addr = do_mmap(file_priv->filp, 0, args->size,
			PROT_READ | PROT_WRITE, MAP_SHARED, 0);

	drm_gem_object_unreference(obj);

	if (IS_ERR_VALUE(addr)) {
		/* check filp->f_op, filp->private_data are restored */
		if (file_priv->filp->f_op == &exynos_drm_gem_fops) {
			file_priv->filp->f_op = fops_get(dev->driver->fops);
			file_priv->filp->private_data = file_priv;
		}
		mutex_unlock(&dev->struct_mutex);
		up_write(&mm->mmap_sem);
		return (int)addr;
	}

	mutex_unlock(&dev->struct_mutex);
	up_write(&mm->mmap_sem);

	args->mapped = addr;

	DRM_DEBUG("%s:hdl[%d]sz[%d]obj[0x%x]mapped[0x%x]\n", __func__,
		args->handle, (int)args->size, (int)obj, (int)args->mapped);

	return 0;
}

int exynos_drm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv)
{	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_exynos_gem_info *args = data;
	struct drm_gem_object *obj;

	mutex_lock(&dev->struct_mutex);

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	exynos_gem_obj = to_exynos_gem_obj(obj);

	args->flags = exynos_gem_obj->flags;
	args->size = exynos_gem_obj->size;

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static int exynos_gem_l1_cache_ops(struct drm_device *drm_dev,
					struct drm_exynos_gem_cache_op *op) {
	if (op->flags & EXYNOS_DRM_CACHE_FSH_ALL) {
		/*
		 * cortex-A9 core has individual l1 cache so flush l1 caches
		 * for all cores but other cores should be considered later.
		 * TODO
		 */
		if (op->flags & EXYNOS_DRM_ALL_CORES)
			flush_all_cpu_caches();
		else
			__cpuc_flush_user_all();

	} else if (op->flags & EXYNOS_DRM_CACHE_FSH_RANGE) {
		struct vm_area_struct *vma;

		down_read(&current->mm->mmap_sem);
		vma = find_vma(current->mm, op->usr_addr);
		up_read(&current->mm->mmap_sem);

		if (!vma) {
			DRM_ERROR("failed to get vma.\n");
			return -EFAULT;
		}

		__cpuc_flush_user_range(op->usr_addr, op->usr_addr + op->size,
					vma->vm_flags);
	}

	return 0;
}

static int exynos_gem_l2_cache_ops(struct drm_device *drm_dev,
				struct drm_file *filp,
				struct drm_exynos_gem_cache_op *op)
{
	if (op->flags & EXYNOS_DRM_CACHE_FSH_RANGE ||
			op->flags & EXYNOS_DRM_CACHE_INV_RANGE ||
			op->flags & EXYNOS_DRM_CACHE_CLN_RANGE) {
		unsigned long virt_start = op->usr_addr, pfn;
		phys_addr_t phy_start, phy_end;
		struct vm_area_struct *vma;
		int ret;

		down_read(&current->mm->mmap_sem);
		vma = find_vma(current->mm, op->usr_addr);
		up_read(&current->mm->mmap_sem);

		if (!vma) {
			DRM_ERROR("failed to get vma.\n");
			return -EFAULT;
		}

		/*
		 * Range operation to l2 cache(PIPT)
		 */
		if (vma && (vma->vm_flags & VM_PFNMAP)) {
			ret = follow_pfn(vma, virt_start, &pfn);
			if (ret < 0) {
				DRM_ERROR("failed to get pfn.\n");
				return ret;
			}

			/*
			 * the memory region with VM_PFNMAP is contiguous
			 * physically so do range operagion just one time.
			 */
			phy_start = pfn << PAGE_SHIFT;
			phy_end = phy_start + op->size;

			if (op->flags & EXYNOS_DRM_CACHE_FSH_RANGE)
				outer_flush_range(phy_start, phy_end);
			else if (op->flags & EXYNOS_DRM_CACHE_INV_RANGE)
				outer_inv_range(phy_start, phy_end);
			else if (op->flags & EXYNOS_DRM_CACHE_CLN_RANGE)
				outer_clean_range(phy_start, phy_end);

			return 0;
		} else {
			struct exynos_drm_gem_obj *exynos_obj;
			struct exynos_drm_gem_buf *buf;
			struct drm_gem_object *obj;
			struct scatterlist *sgl;
			unsigned int npages, i = 0;

			mutex_lock(&drm_dev->struct_mutex);

			obj = drm_gem_object_lookup(drm_dev, filp,
							op->gem_handle);
			if (!obj) {
				DRM_ERROR("failed to lookup gem object.\n");
				mutex_unlock(&drm_dev->struct_mutex);
				return -EINVAL;
			}

			exynos_obj = to_exynos_gem_obj(obj);
			buf = exynos_obj->buffer;
			npages = buf->sgt->nents;
			sgl = buf->sgt->sgl;

			drm_gem_object_unreference(obj);
			mutex_unlock(&drm_dev->struct_mutex);

			/*
			 * in this case, the memory region is non-contiguous
			 * physically  so do range operation to all the pages.
			 */
			while (i < npages) {
				phy_start = sg_dma_address(sgl);
				phy_end = phy_start + sgl->length;

				if (op->flags & EXYNOS_DRM_CACHE_FSH_RANGE)
					outer_flush_range(phy_start, phy_end);
				else if (op->flags & EXYNOS_DRM_CACHE_INV_RANGE)
					outer_inv_range(phy_start, phy_end);
				else if (op->flags & EXYNOS_DRM_CACHE_CLN_RANGE)
					outer_clean_range(phy_start, phy_end);

				i++;
				sgl = sg_next(sgl);
			}

			return 0;
		}
	}

	if (op->flags & EXYNOS_DRM_CACHE_FSH_ALL)
		outer_flush_all();
	else if (op->flags & EXYNOS_DRM_CACHE_INV_ALL)
		outer_inv_all();
	else if (op->flags & EXYNOS_DRM_CACHE_CLN_ALL)
		outer_clean_all();
	else {
		DRM_ERROR("invalid l2 cache operation.\n");
		return -EINVAL;
	}


	return 0;
}

struct vm_area_struct *exynos_gem_get_vma(struct vm_area_struct *vma)
{
	struct vm_area_struct *vma_copy;

	vma_copy = kmalloc(sizeof(*vma_copy), GFP_KERNEL);
	if (!vma_copy)
		return NULL;

	if (vma->vm_ops && vma->vm_ops->open)
		vma->vm_ops->open(vma);

	if (vma->vm_file)
		get_file(vma->vm_file);

	memcpy(vma_copy, vma, sizeof(*vma));

	vma_copy->vm_mm = NULL;
	vma_copy->vm_next = NULL;
	vma_copy->vm_prev = NULL;

	return vma_copy;
}

void exynos_gem_put_vma(struct vm_area_struct *vma)
{
	if (!vma)
		return;

	if (vma->vm_ops && vma->vm_ops->close)
		vma->vm_ops->close(vma);

	if (vma->vm_file)
		fput(vma->vm_file);

	kfree(vma);
}

int exynos_gem_get_pages_from_userptr(unsigned long start,
						unsigned int npages,
						struct page **pages,
						struct vm_area_struct *vma)
{
	int get_npages;

	/* the memory region mmaped with VM_PFNMAP. */
	if (vma_is_io(vma)) {
		unsigned int i;

		for (i = 0; i < npages; ++i, start += PAGE_SIZE) {
			unsigned long pfn;
			int ret = follow_pfn(vma, start, &pfn);
			if (ret)
				return ret;

			pages[i] = pfn_to_page(pfn);
		}

		if (i != npages) {
			DRM_ERROR("failed to get user_pages.\n");
			return -EINVAL;
		}

		return 0;
	}

	get_npages = get_user_pages(current, current->mm, start,
					npages, 1, 1, pages, NULL);
	get_npages = max(get_npages, 0);
	if (get_npages != npages) {
		DRM_ERROR("failed to get user_pages.\n");
		while (get_npages)
			put_page(pages[--get_npages]);
		return -EFAULT;
	}

	return 0;
}

void exynos_gem_put_pages_to_userptr(struct page **pages,
					unsigned int npages,
					struct vm_area_struct *vma)
{
	if (!vma_is_io(vma)) {
		unsigned int i;

		for (i = 0; i < npages; i++) {
			set_page_dirty_lock(pages[i]);

			/*
			 * undo the reference we took when populating
			 * the table.
			 */
			put_page(pages[i]);
		}
	}
}

int exynos_gem_map_sgt_with_dma(struct drm_device *drm_dev,
				struct sg_table *sgt,
				enum dma_data_direction dir)
{
	int nents;

	mutex_lock(&drm_dev->struct_mutex);

	nents = dma_map_sg(drm_dev->dev, sgt->sgl, sgt->nents, dir);
	if (!nents) {
		DRM_ERROR("failed to map sgl with dma.\n");
		mutex_unlock(&drm_dev->struct_mutex);
		return nents;
	}

	mutex_unlock(&drm_dev->struct_mutex);
	return 0;
}

void exynos_gem_unmap_sgt_from_dma(struct drm_device *drm_dev,
				struct sg_table *sgt,
				enum dma_data_direction dir)
{
	dma_unmap_sg(drm_dev->dev, sgt->sgl, sgt->nents, dir);
}

int exynos_drm_gem_cache_op_ioctl(struct drm_device *drm_dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_exynos_gem_cache_op *op = data;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	ret = check_cache_flags(op->flags);
	if (ret)
		return -EINVAL;

	/*
	 * do cache operation for all cache range if op->size is bigger
	 * than SZ_1M because cache range operation with bit size has
	 * big cost.
	 */
	if (op->size >= SZ_1M) {
		if (op->flags & EXYNOS_DRM_CACHE_FSH_RANGE) {
			if (op->flags & EXYNOS_DRM_L1_CACHE)
				__cpuc_flush_user_all();

			if (op->flags & EXYNOS_DRM_L2_CACHE)
				outer_flush_all();

			return 0;
		} else if (op->flags & EXYNOS_DRM_CACHE_INV_RANGE) {
			if (op->flags & EXYNOS_DRM_L2_CACHE)
				outer_flush_all();

			return 0;
		} else if (op->flags & EXYNOS_DRM_CACHE_CLN_RANGE) {
			if (op->flags & EXYNOS_DRM_L2_CACHE)
				outer_clean_all();

			return 0;
		}
	}

	if (op->flags & EXYNOS_DRM_L1_CACHE ||
			op->flags & EXYNOS_DRM_ALL_CACHES) {
		ret = exynos_gem_l1_cache_ops(drm_dev, op);
		if (ret < 0)
			goto err;
	}

	if (op->flags & EXYNOS_DRM_L2_CACHE ||
			op->flags & EXYNOS_DRM_ALL_CACHES)
		ret = exynos_gem_l2_cache_ops(drm_dev, file_priv, op);
err:
	return ret;
}

int exynos_drm_gem_init_object(struct drm_gem_object *obj)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	return 0;
}

void exynos_drm_gem_free_object(struct drm_gem_object *obj)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct exynos_drm_gem_buf *buf;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	exynos_gem_obj = to_exynos_gem_obj(obj);
	buf = exynos_gem_obj->buffer;

	if (obj->import_attach)
		drm_prime_gem_destroy(obj, buf->sgt);

	exynos_drm_gem_destroy(to_exynos_gem_obj(obj));
}

int exynos_drm_gem_dumb_create(struct drm_file *file_priv,
			       struct drm_device *dev,
			       struct drm_mode_create_dumb *args)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/*
	 * alocate memory to be used for framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_CREATE_DUMB command.
	 */

	args->pitch = args->width * ((args->bpp + 7) / 8);
	args->size = args->pitch * args->height;

	exynos_gem_obj = exynos_drm_gem_create(dev, args->flags, args->size);
	if (IS_ERR(exynos_gem_obj))
		return PTR_ERR(exynos_gem_obj);

	ret = exynos_drm_gem_handle_create(&exynos_gem_obj->base, file_priv,
			&args->handle);
	if (ret) {
		exynos_drm_gem_destroy(exynos_gem_obj);
		return ret;
	}

	exynos_drm_gem_register_pid(file_priv);

	return 0;
}

int exynos_drm_gem_dumb_map_offset(struct drm_file *file_priv,
				   struct drm_device *dev, uint32_t handle,
				   uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	mutex_lock(&dev->struct_mutex);

	/*
	 * get offset of memory allocated for drm framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_MAP_DUMB command.
	 */

	obj = drm_gem_object_lookup(dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
		goto unlock;
	}

	if (!obj->map_list.map) {
		ret = drm_gem_create_mmap_offset(obj);
		if (ret)
			goto out;
	}

	*offset = (u64)obj->map_list.hash.key << PAGE_SHIFT;
	DRM_DEBUG_KMS("offset = 0x%lx\n", (unsigned long)*offset);

out:
	drm_gem_object_unreference(obj);
unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int exynos_drm_gem_dumb_destroy(struct drm_file *file_priv,
				struct drm_device *dev,
				unsigned int handle)
{
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/*
	 * obj->refcount and obj->handle_count are decreased and
	 * if both them are 0 then exynos_drm_gem_free_object()
	 * would be called by callback to release resources.
	 */
	ret = drm_gem_handle_delete(file_priv, handle);
	if (ret < 0) {
		DRM_ERROR("failed to delete drm_gem_handle.\n");
		return ret;
	}

	return 0;
}

void exynos_drm_gem_close_object(struct drm_gem_object *obj,
				struct drm_file *file)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* TODO */
}

int exynos_drm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj = vma->vm_private_data;
	struct drm_device *dev = obj->dev;
	unsigned long f_vaddr;
	pgoff_t page_offset;
	int ret;
#ifdef CONFIG_DMABUF_SYNC_PROFILE
	struct dma_buf *dmabuf = obj->export_dma_buf;
	struct dmabuf_sync_reservation *robj = dmabuf->sync;
#endif

	page_offset = ((unsigned long)vmf->virtual_address -
			vma->vm_start) >> PAGE_SHIFT;
	f_vaddr = (unsigned long)vmf->virtual_address;

#ifdef CONFIG_DMABUF_SYNC_PROFILE
	if (dmabuf) {
		if (robj)
			robj->vma = vma;
		else
			BUG_ON(1);

		/*
		 * Collect profiling data one time only in case of first page.
		 */
		if (!robj->dirty) {
			dmabuf_sync_profile_start_cpu(dmabuf);
			robj->dirty = 1;
		}
	}
#endif

	mutex_lock(&dev->struct_mutex);

	ret = exynos_drm_gem_map_buf(obj, vma, f_vaddr, page_offset);
	if (ret < 0)
		DRM_ERROR("failed to map a buffer with user.\n");

	mutex_unlock(&dev->struct_mutex);

#ifdef CONFIG_DMABUF_SYNC_PROFILE
	if (dmabuf) {
		/*
		 * Collect profiling data when CPU tried to access a last page.
		 * TODO. consider certain region from fcntl system call.
		 */
		if ((obj->size >> PAGE_SHIFT) == (page_offset + 1))
			dmabuf_sync_profile_end_cpu(dmabuf);
	}
#endif
	return convert_to_vm_err_msg(ret);
}

int exynos_drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	/* set vm_area_struct. */
	ret = drm_gem_mmap(filp, vma);
	if (ret < 0) {
		DRM_ERROR("failed to mmap.\n");
		return ret;
	}

	obj = vma->vm_private_data;
	exynos_gem_obj = to_exynos_gem_obj(obj);

	ret = check_gem_flags(exynos_gem_obj->flags);
	if (ret) {
		drm_gem_vm_close(vma);
		drm_gem_free_mmap_offset(obj);
		return ret;
	}

	vma->vm_flags &= ~VM_PFNMAP;
	vma->vm_flags |= VM_MIXEDMAP;

	update_vm_cache_attr(exynos_gem_obj, vma);

	return ret;
}
