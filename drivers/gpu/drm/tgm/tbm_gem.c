/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	JinYoung Jeon <jy0.jeon@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <tbm_gem.h>

int gv_gem_alloc_size;

static void update_vm_cache_attr(struct tbm_gem_object *obj,
					struct vm_area_struct *vma)
{
	DRM_DEBUG_KMS("flags = 0x%x\n", obj->flags);

	/* non-cachable as default. */
	if (obj->flags & TBM_BO_CACHABLE)
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	else if (obj->flags & TBM_BO_WC)
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

static int check_gem_flags(unsigned int flags)
{
	/* TODO */
	return 0;
}

struct tbm_gem_buf *tbm_init_buf(struct drm_device *dev,
						unsigned int size)
{
	struct tbm_gem_buf *buffer;

	DRM_DEBUG_KMS("desired size = 0x%x\n", size);

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		DRM_ERROR("failed to allocate tbm_gem_buf.\n");
		return NULL;
	}

	buffer->size = size;
	return buffer;
}

struct tbm_gem_object *tbm_gem_obj_init(struct drm_device *dev,
						      unsigned long size)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	tbm_gem_obj = kzalloc(sizeof(*tbm_gem_obj), GFP_KERNEL);
	if (!tbm_gem_obj) {
		DRM_ERROR("failed to allocate tbm_gem_object\n");
		return NULL;
	}

	tbm_gem_obj->size = size;
	obj = &tbm_gem_obj->base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(tbm_gem_obj);
		return NULL;
	}

	return tbm_gem_obj;
}

int tbm_alloc_buf(struct drm_device *dev,
		struct tbm_gem_buf *buf, unsigned int flags)
{
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	int ret;

	if (!tbm_priv->gem_bufer_alloc) {
		ret = -EPERM;
		goto out;
	}

	ret = tbm_priv->gem_bufer_alloc(dev, flags, buf);
out:
	return ret;
}

void tbm_free_buf(struct drm_device *dev,
		unsigned int flags, struct tbm_gem_buf *buffer)
{
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;

	if (tbm_priv->gem_bufer_dealloc)
		tbm_priv->gem_bufer_dealloc(dev, flags, buffer);
}

void tbm_fini_buf(struct drm_device *dev,
				struct tbm_gem_buf *buffer)
{
	DRM_DEBUG_KMS("%s.\n", __FILE__);

	if (!buffer) {
		DRM_DEBUG_KMS("buffer is null.\n");
		return;
	}

	kfree(buffer);
	buffer = NULL;
}

static void tbm_gem_register_pid(struct drm_file *file_priv)
{
	struct tgm_drv_file_private *driver_priv = file_priv->driver_priv;

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

struct tbm_gem_object *tbm_gem_create_obj(struct drm_device *dev,
						unsigned int flags,
						unsigned long size)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct tbm_gem_buf *buf;
	int ret;

	if (!size) {
		DRM_ERROR("invalid size.\n");
		return ERR_PTR(-EINVAL);
	}

	size = roundup_gem_size(size, flags);

	ret = check_gem_flags(flags);
	if (ret)
		return ERR_PTR(ret);

	buf = tbm_init_buf(dev, size);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	tbm_gem_obj = tbm_gem_obj_init(dev, size);
	if (!tbm_gem_obj) {
		ret = -ENOMEM;
		goto err_fini_buf;
	}

	tbm_gem_obj->buffer = buf;

	/* set memory type and cache attribute from user side. */
	tbm_gem_obj->flags = flags;
	buf->obj = &tbm_gem_obj->base;

	ret = tbm_alloc_buf(dev, buf, flags);
	if (ret < 0) {
		drm_gem_object_release(&tbm_gem_obj->base);
		goto err_fini_buf;
	}

	return tbm_gem_obj;

err_fini_buf:
	tbm_fini_buf(dev, buf);
	return ERR_PTR(ret);
}

static int tbm_gem_handle_create(struct drm_gem_object *obj,
					struct drm_file *file_priv,
					unsigned int *handle)
{
	int ret;

	ret = drm_gem_handle_create(file_priv, obj, handle);
	if (ret)
		return ret;

	DRM_DEBUG_KMS("gem handle = 0x%x\n", *handle);

	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

void tbm_gem_destroy(struct tbm_gem_object *tbm_gem_obj)
{
	struct drm_gem_object *obj;
	struct tbm_gem_buf *buf;

	obj = &tbm_gem_obj->base;
	buf = tbm_gem_obj->buffer;

	if (!buf->pages)
		return;

	gv_gem_alloc_size -= (int)tbm_gem_obj->buffer->size;

	DRM_DEBUG("%s:obj[%p]sz[%d]to[%d]\n", "gf", obj,
		(int)tbm_gem_obj->buffer->size, gv_gem_alloc_size);

#ifdef GEM_DEBUG_LOG
	gem_save_info(GEM_DESTROY, obj);
#endif

	tbm_free_buf(obj->dev, tbm_gem_obj->flags, buf);

	tbm_fini_buf(obj->dev, buf);
	tbm_gem_obj->buffer = NULL;

	drm_gem_free_mmap_offset(obj);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(tbm_gem_obj);
	tbm_gem_obj = NULL;

	DRM_DEBUG("%s:done\n", "gem_free");
}

int tbm_gem_create_ioctl(struct drm_device *dev, void *data,
				struct drm_file *file_priv)
{
	struct tbm_gem_create *args = data;
	struct drm_gem_object *obj;
	struct tbm_gem_object *tbm_gem_obj;
	int ret;

	tbm_gem_obj = tbm_gem_create_obj(dev, args->flags, args->size);
	if (IS_ERR(tbm_gem_obj))
		return PTR_ERR(tbm_gem_obj);

	obj = &tbm_gem_obj->base;
	ret = tbm_gem_handle_create(obj, file_priv, &args->handle);
	if (ret) {
		tbm_gem_destroy(tbm_gem_obj);
		return ret;
	}

	tbm_gem_register_pid(file_priv);

#ifdef GEM_DEBUG_LOG
	gem_save_info(GEM_CREATE, obj);
#endif

	gv_gem_alloc_size += (int)tbm_gem_obj->buffer->size;

	DRM_DEBUG("%s:sz[%d %d]f[0x%x]h[%d]obj[%p]to[%d]\n",
		"ga", (int)args->size, (int)tbm_gem_obj->buffer->size,
		args->flags, args->handle, obj, gv_gem_alloc_size);

	return 0;
}

static int tbm_gem_mmap_buffer(struct file *filp,
				      struct vm_area_struct *vma)
{
	struct drm_gem_object *obj = filp->private_data;
	struct tbm_gem_object *tbm_gem_obj = to_tbm_gem_obj(obj);
	struct tbm_gem_buf *buffer;
	unsigned long pfn, vm_size;

	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);

	update_vm_cache_attr(tbm_gem_obj, vma);

	vm_size = vma->vm_end - vma->vm_start;

	/*
	 * a buffer contains information to physically continuous memory
	 * allocated by user request or at framebuffer creation.
	 */
	buffer = tbm_gem_obj->buffer;

	/* check if user-requested size is valid. */
	if (vm_size > buffer->size)
		return -EINVAL;

	if (IS_TBM_NONCONTIG_BUF(tbm_gem_obj->flags)) {
		unsigned long addr = vma->vm_start;
		unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
		struct scatterlist *sg;
		int i;

		for_each_sg(buffer->sgt->sgl, sg, buffer->sgt->nents, i) {
			struct page *page = sg_page(sg);
			unsigned long remainder = vma->vm_end - addr;
			unsigned long len = sg->length;

			if (offset >= sg->length) {
				offset -= sg->length;
				continue;
			} else if (offset) {
				page += offset / PAGE_SIZE;
				len = sg->length - offset;
				offset = 0;
			}
			len = min(len, remainder);
			remap_pfn_range(vma, addr, page_to_pfn(page), len,
					vma->vm_page_prot);
			addr += len;
			if (addr >= vma->vm_end) {
				break;
			}
		}
	} else {
		/*
		 * get page frame number to physical memory to be mapped
		 * to user space.
		 */
		pfn = ((unsigned long)tbm_gem_obj->buffer->dma_addr) >>
								PAGE_SHIFT;

		DRM_DEBUG_KMS("pfn = 0x%lx\n", pfn);

		if (remap_pfn_range(vma, vma->vm_start, pfn, vm_size,
					vma->vm_page_prot)) {
			DRM_ERROR("failed to remap pfn range.\n");
			return -EAGAIN;
		}
	}

	return 0;
}

static const struct file_operations tbm_gem_fops = {
	.mmap = tbm_gem_mmap_buffer,
};

int tbm_gem_mmap_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct tbm_gem_mmap *args = data;
	struct drm_gem_object *obj;
	unsigned long addr;

	DRM_DEBUG("%s\n", __func__);

	if (!(dev->driver->driver_features & DRIVER_GEM)) {
		DRM_ERROR("does not support GEM.\n");
		return -ENODEV;
	}

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object:h[%d]\n", args->handle);
		return -EINVAL;
	}

	obj->filp->f_op = &tbm_gem_fops;
	obj->filp->private_data = obj;

	addr = vm_mmap(obj->filp, 0, args->size,
			PROT_READ | PROT_WRITE, MAP_SHARED, 0);

	drm_gem_object_unreference_unlocked(obj);

	if (IS_ERR_VALUE(addr))
		return (int)addr;

	args->mapped = addr;

	DRM_DEBUG("%s:h[%d]sz[%d]obj[%p]va[0x%x]\n", "gem_mmap",
		args->handle, (int)args->size, obj, (int)args->mapped);

	return 0;
}

int tbm_gem_get_ioctl(struct drm_device *dev, void *data,
				      struct drm_file *file_priv)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct tbm_gem_info *args = data;
	struct drm_gem_object *obj;

	mutex_lock(&dev->struct_mutex);

	obj = drm_gem_object_lookup(dev, file_priv, args->handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	tbm_gem_obj = to_tbm_gem_obj(obj);

	args->flags = tbm_gem_obj->flags;
	args->size = tbm_gem_obj->size;

	drm_gem_object_unreference(obj);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

int tbm_gem_prime_fd_to_handle(struct drm_device *dev,
		struct drm_file *file_priv, int prime_fd, uint32_t *handle)
{
	int ret;

	ret = drm_gem_prime_fd_to_handle(dev, file_priv, prime_fd, handle);
	if (ret < 0)
		goto out;

	tbm_gem_register_pid(file_priv);

out:
	return ret;
}


int tbm_gem_cpu_prep_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct tbm_gem_cpu_access *args = data;
	struct drm_gem_object *obj;
	struct tbm_gem_object *tbm_gem_obj;
	struct tbm_gem_buf *buffer;
#ifdef CONFIG_DRM_DMA_SYNC
	struct tgm_drv_private *dev_priv = dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	struct reservation_object *resv;
	struct fence *fence;
	bool wait_resv = false;
	int ret = 0;
#endif

	DRM_DEBUG("%s:h[%d]\n", __func__, args->handle);

	obj = drm_gem_object_lookup(dev, file, args->handle);
	if (!obj) {
		DRM_ERROR("Failed to lookup gem object:hdl[%d]\n", args->handle);
		return -EINVAL;
	}

	tbm_gem_obj = to_tbm_gem_obj(obj);
	buffer = tbm_gem_obj->buffer;

#ifdef CONFIG_DRM_DMA_SYNC
	if (!obj->dma_buf)
		goto no_fence;

	resv = obj->dma_buf->resv;
	if (!resv)
		goto no_fence;

	ww_mutex_lock(&resv->lock, NULL);
	if (!reservation_object_test_signaled_rcu(resv, true))
		wait_resv = true;

	DRM_DEBUG("%s:buf[%p]%s\n", "cpu_prep", obj->dma_buf, wait_resv ? "[wait]" : "");

	if (wait_resv) {
		long lret;
		struct reservation_object_list *robj_list;
		struct fence *excl, *shared;

		ww_mutex_unlock(&resv->lock);

		if (file->filp->f_flags & O_NONBLOCK)
			goto no_fence;

		lret = reservation_object_wait_timeout_rcu(resv, true, false,
							msecs_to_jiffies(300));
		if (!lret)
			DRM_ERROR("timeout:hdl[%d]obj[%p]\n", args->handle, obj);
		else if (lret < 0) {
			DRM_ERROR("failed to wait resv obj:ret[%ld]hdl[%d]obj[%p]\n",
				lret, args->handle, obj);
			drm_gem_object_unreference_unlocked(obj);
			ret = lret;
			return ret;
		}

		ww_mutex_lock(&resv->lock, NULL);

		robj_list = reservation_object_get_list(resv);
		excl = reservation_object_get_excl(resv);
		if (excl) {
			if (!fence_is_signaled(excl)) {
				lret = fence_wait_timeout(excl, true, msecs_to_jiffies(300));
				DRM_INFO("%s:wait:excl:buf[%p]lret[%ld]\n", "cpu_prep",
					obj->dma_buf, lret);
			}
		}

		if (robj_list) {
			int i;

			for (i = 0; i < robj_list->shared_count; i++) {
				shared = rcu_dereference_protected(
						robj_list->shared[i],
						reservation_object_held(resv));
				if (!fence_is_signaled(shared)) {
					lret = fence_wait_timeout(shared, true,
						msecs_to_jiffies(300));
					DRM_INFO("%s:wait:shared:idx[%d]buf[%p]lret[%ld]\n",
						"cpu_prep", i, obj->dma_buf, lret);
				}
			}
		}
	}

	ret = reservation_object_reserve_shared(resv);
	if (ret < 0) {
		DRM_ERROR("failed to resv:ret[%d]hdl[%d]obj[%p]\n",
			ret, args->handle, obj);
		goto err_fence;
	}

	fence = drm_sw_fence_new(tbm_priv->gem_fence_context,
				 atomic_add_return(1, &tbm_priv->gem_fence_seqno));
	if (IS_ERR(fence)) {
		ret = PTR_ERR(fence);
		DRM_ERROR("Failed to create fence:ret[%d]hdl[%d]obj[%p]\n",
			ret, args->handle, obj);
		goto err_fence;
	}

	tbm_gem_obj->pending_fence = fence;
	reservation_object_add_excl_fence(resv, tbm_gem_obj->pending_fence);
	DRM_DEBUG("%s_done:buf[%p]fence[%p]\n", "cpu_prep", obj->dma_buf, fence);

	ww_mutex_unlock(&resv->lock);

no_fence:
#endif
	if (tbm_gem_obj->flags & TBM_BO_CACHABLE)
		dma_sync_sg_for_cpu(dev->dev, buffer->sgt->sgl,
				    buffer->sgt->nents, DMA_FROM_DEVICE);

	drm_gem_object_unreference_unlocked(obj);

	DRM_DEBUG("%s:h[%d]obj[%p]done\n", "cpu_prep", args->handle, obj);

	return 0;

#ifdef CONFIG_DRM_DMA_SYNC
err_fence:
	ww_mutex_unlock(&resv->lock);
	drm_gem_object_unreference_unlocked(obj);
	return ret;
#endif
}

int tbm_gem_cpu_fini_ioctl(struct drm_device *dev, void *data,
				  struct drm_file *file)
{
	struct tbm_gem_cpu_access *args = data;
	struct drm_gem_object *obj;
	struct tbm_gem_object *tbm_gem_obj;
	struct tbm_gem_buf *buffer;

	DRM_DEBUG("%s:h[%d]\n", "cpu_fini", args->handle);

	obj = drm_gem_object_lookup(dev, file, args->handle);
	if (!obj) {
		DRM_ERROR("Failed to lookup gem object:hdl[%d]\n", args->handle);
		return -EINVAL;
	}

	tbm_gem_obj = to_tbm_gem_obj(obj);
	buffer = tbm_gem_obj->buffer;

	if (tbm_gem_obj->flags & TBM_BO_CACHABLE)
		dma_sync_sg_for_device(dev->dev, buffer->sgt->sgl,
				       buffer->sgt->nents, DMA_TO_DEVICE);

#ifdef CONFIG_DRM_DMA_SYNC
	if (tbm_gem_obj->pending_fence) {
		DRM_DEBUG("%s:fence[%p]\n", __func__, tbm_gem_obj->pending_fence);
		drm_fence_signal_and_put(&tbm_gem_obj->pending_fence);
		tbm_gem_obj->pending_fence = NULL;
	}
#endif

	drm_gem_object_unreference_unlocked(obj);

	DRM_DEBUG("%s:h[%d]obj[%p]done\n", "cpu_fini", args->handle, obj);

	return 0;
}

struct dma_buf  *tbm_gem_get_dma_buf(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	struct dma_buf  *dma_buf = NULL;

	if (tbm_priv->gem_get_dma_buf)
		dma_buf = tbm_priv->gem_get_dma_buf(drm_dev, dev,
			gem_handle, filp);

	DRM_DEBUG("%s:h[%d]buf[%p]\n", __func__, gem_handle, dma_buf);

	return dma_buf;
}

int tbm_gem_object_unreference(struct drm_gem_object *obj)
{
	DRM_DEBUG("%s:obj[%p]\n", __func__, obj);

	if (!obj) {
		DRM_ERROR("failed to get drm_gem_object\n");
		return -EINVAL;
	}

	drm_gem_object_unreference_unlocked(obj);

	return 0;
}

dma_addr_t *tbm_gem_get_dma_addr(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
	dma_addr_t *addr = NULL;

	if (tbm_priv->gem_get_dma_addr)
		addr = tbm_priv->gem_get_dma_addr(drm_dev, dev,
			gem_handle, filp);

	return addr;
}

void tbm_gem_put_dma_addr(struct drm_device *drm_dev,
		struct device *dev, unsigned int gem_handle,
		struct drm_file *filp)
{
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;

	if (tbm_priv->gem_put_dma_addr)
		tbm_priv->gem_put_dma_addr(drm_dev, dev,
			gem_handle, filp);
}

unsigned long tbm_gem_get_size(struct drm_device *drm_dev,
						unsigned int gem_handle,
						struct drm_file *filp)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(drm_dev, filp, gem_handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		return 0;
	}

	tbm_gem_obj = to_tbm_gem_obj(obj);
	drm_gem_object_unreference_unlocked(obj);

	return tbm_gem_obj->buffer->size;
}

static int tbm_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	DRM_INFO("%s\n", __func__);

	return 0;
}

static const struct vm_operations_struct tbm_gem_vm_ops = {
	.fault = tbm_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static void tbm_gem_free_object(struct drm_gem_object *obj)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct tbm_gem_buf *buf;

	DRM_DEBUG("%s\n", __func__);

	tbm_gem_obj = to_tbm_gem_obj(obj);
	buf = tbm_gem_obj->buffer;

	if (obj->import_attach)
		drm_prime_gem_destroy(obj, buf->sgt);

	tbm_gem_destroy(to_tbm_gem_obj(obj));
}

static int tbm_gem_dumb_create(struct drm_file *file_priv,
	struct drm_device *drm_dev, struct drm_mode_create_dumb *args)
{
	struct tbm_gem_object *tbm_gem_obj;
	struct drm_gem_object *obj;
	int ret;

	/*
	 * alocate memory to be used for framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_CREATE_DUMB command.
	 */

	args->pitch = args->width * args->bpp >> 3;
	args->size = PAGE_ALIGN(args->pitch * args->height);

	tbm_gem_obj = tbm_gem_create_obj(drm_dev, args->flags, args->size);
	if (IS_ERR(tbm_gem_obj))
		return PTR_ERR(tbm_gem_obj);

	obj = &tbm_gem_obj->base;
	ret = tbm_gem_handle_create(obj, file_priv, &args->handle);
	if (ret)
		tbm_gem_destroy(tbm_gem_obj);

	DRM_INFO("%s:h[%d]obj[%p]a[0x%x]ret[%d]\n", __func__,
		args->handle, obj, (int)tbm_gem_obj->buffer->dma_addr, ret);

	return ret;
}

static int tbm_gem_dumb_map_offset(struct drm_file *file_priv,
	struct drm_device *drm_dev, uint32_t handle, uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	mutex_lock(&drm_dev->struct_mutex);

	/*
	 * get offset of memory allocated for drm framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_MAP_DUMB command.
	 */

	obj = drm_gem_object_lookup(drm_dev, file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
		goto unlock;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_INFO("%s:h[%d]obj[%p]offset[%llu]\n",
		__func__, handle, obj, *offset);

out:
	drm_gem_object_unreference(obj);
unlock:
	mutex_unlock(&drm_dev->struct_mutex);
	return ret;
}

static int tbm_gem_dumb_destroy(struct drm_file *file_priv,
	struct drm_device *dev, unsigned int handle)
{
	int ret;

	/*
	 * obj->refcount and obj->handle_count are decreased and
	 * if both them are 0 then sprd_drm_gem_free_object()
	 * would be called by callback to release resources.
	 */
	ret = drm_gem_handle_delete(file_priv, handle);

	tbm_gem_register_pid(file_priv);

	DRM_INFO("%s:h[%d]ret[%d]\n", __func__, handle, ret);

	return ret;
}

static int tbm_gem_one_info(int id, void *ptr, void *data)
{
	struct drm_gem_object *obj = (struct drm_gem_object *)ptr;
	struct tbm_gem_info_data *gem_info_data = data;
	struct tgm_drv_file_private *file_priv =
			gem_info_data->filp->driver_priv;
	struct tbm_gem_object *tbm_gem_obj;
	struct tbm_gem_buf *buf;

	if (!obj) {
		DRM_ERROR("failed to get drm_gem_object\n");
		return -EFAULT;
	}

	drm_gem_object_reference(obj);

	tbm_gem_obj = to_tbm_gem_obj(obj);
	buf = tbm_gem_obj->buffer;

	seq_printf(gem_info_data->m,
			"%5d\t%5d\t%4d\t%4d\t\t%4d\t0x%08lx\t0x%x\t%4d\t%4d\t\t"
			"%4d\t\t0x%p\t%6d\n",
				file_priv->pid,
				file_priv->tgid,
				id,
				atomic_read(&obj->refcount.refcount) - 1,
				obj->handle_count,
				tbm_gem_obj->size,
				tbm_gem_obj->flags,
				buf->pfnmap,
				0,
				obj->import_attach ? 1 : 0,
				obj,
				obj->name);

	drm_gem_object_unreference(obj);

	return 0;
}

int tbm_gem_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *drm_dev = node->minor->dev;
	struct tbm_gem_info_data gem_info_data;

	gem_info_data.m = m;
	seq_puts(gem_info_data.m,
			"pid\ttgid\thandle\trefcount\thcount\tsize\t\tflags\t"
			"pfnmap\texport_to_fd\timport_from_fd\tobj_addr\t"
			"\tname\n");
	mutex_lock(&drm_dev->struct_mutex);

	list_for_each_entry(gem_info_data.filp, &drm_dev->filelist, lhead) {
		spin_lock(&gem_info_data.filp->table_lock);
		idr_for_each(&gem_info_data.filp->object_idr,
			tbm_gem_one_info, &gem_info_data);
		spin_unlock(&gem_info_data.filp->table_lock);
	}

	mutex_unlock(&drm_dev->struct_mutex);

	return 0;
}

int tbm_gem_init(struct drm_device *drm_dev)
{
	struct drm_driver *drm_drv = drm_dev->driver;
#ifdef CONFIG_DRM_DMA_SYNC
	struct tgm_drv_private *dev_priv = drm_dev->dev_private;
	struct tbm_private *tbm_priv = dev_priv->tbm_priv;
#endif
	int ret = 0;

	DRM_DEBUG("%s\n", __func__);

	drm_drv->gem_free_object = tbm_gem_free_object;
	drm_drv->gem_vm_ops = &tbm_gem_vm_ops;
	drm_drv->dumb_create = tbm_gem_dumb_create;
	drm_drv->dumb_map_offset = tbm_gem_dumb_map_offset;
	drm_drv->dumb_destroy = tbm_gem_dumb_destroy;
	drm_drv->prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	drm_drv->prime_fd_to_handle = tbm_gem_prime_fd_to_handle,
#ifdef CONFIG_DRM_DMA_SYNC
	tbm_priv->gem_fence_context = fence_context_alloc(1);
	atomic_set(&tbm_priv->gem_fence_seqno, 0);
#endif

#ifdef CONFIG_DRM_TBM_GEM_ION
	ret = tbm_gem_ion_init(drm_dev);
#endif

	return ret;
}
