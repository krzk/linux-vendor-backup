/* exynos_drm_logo.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * Author: YoungJun Cho <yj44.cho@samsung.com>
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

#include <linux/module.h>

#include "drmP.h"
#include "exynos_drm.h"

#include "exynos_drm_fbdev.h"
#include "exynos_drm_gem.h"
#include "exynos_drm_iommu.h"

/*
 * logo object structure
 *
 * @exynos_gem_obj: exynos specific gem object which contains gem object.
 */
struct exynos_drm_logo_obj {
	struct exynos_drm_gem_obj *exynos_gem_obj;
};

static struct exynos_drm_logo_obj *logo_obj;

static struct exynos_drm_gem_obj __init *exynos_drm_logo_create(
				struct drm_device *drm_dev, unsigned long size)
{
	struct exynos_drm_gem_obj *exynos_gem_obj;
	struct exynos_drm_gem_buf *buffer;

	logo_obj = kzalloc(sizeof(*logo_obj), GFP_KERNEL);
	if (!logo_obj) {
		DRM_ERROR("Failed to allocate logo obj.\n");
		return ERR_PTR(-ENOMEM);
	}

	/*
	 * In ramdump mode, u-boot needs spaces to upload s-boot, u-boot and
	 * to display ramdump information screen without overwriting kernel
	 * memory. This means the buffer has to be physically contiguous.
	 * So we make logo buffer with EXYNOS_BO_CONTIG and it is replaced
	 * in ramdump mode.
	 */
	exynos_gem_obj = exynos_drm_gem_create(drm_dev, EXYNOS_BO_CONTIG, size);
	if (IS_ERR(exynos_gem_obj)) {
		void *ret = exynos_gem_obj;

		kfree(logo_obj);
		return ret;
	}
	logo_obj->exynos_gem_obj = exynos_gem_obj;

	buffer = exynos_gem_obj->buffer;

	if (!buffer->kvaddr) {
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
			DRM_ERROR("Failed to map pages to kernel space.\n");
			exynos_drm_gem_destroy(exynos_gem_obj);
			kfree(logo_obj);
			return ERR_PTR(-ENOMEM);
		}
	}

	return exynos_gem_obj;
}

static void __exit exynos_drm_logo_destroy(struct exynos_drm_gem_obj
								*exynos_gem_obj)
{
	if (logo_obj->exynos_gem_obj == exynos_gem_obj) {
		struct drm_device *drm_dev = exynos_gem_obj->base.dev;
		struct exynos_drm_gem_buf *buffer = exynos_gem_obj->buffer;

		if (is_drm_iommu_supported(drm_dev) && buffer->kvaddr)
			vunmap(buffer->kvaddr);
		exynos_drm_gem_destroy(exynos_gem_obj);
		kfree(logo_obj);
	}
}

static void exynos_drm_logo_clear(struct exynos_drm_gem_obj *exynos_gem_obj)
{
	if (logo_obj->exynos_gem_obj == exynos_gem_obj) {
		struct exynos_drm_gem_buf *buffer = exynos_gem_obj->buffer;
		unsigned long size = exynos_gem_obj->size;

		memset(buffer->kvaddr, 0x00, size);

		clean_dcache_area((void *)buffer->kvaddr, size);
		outer_clean_range(buffer->dma_addr, buffer->dma_addr + size);
	}
}

static int __init exynos_drm_logo_init(void)
{
	exynos_drm_fbdev_gem_ops.create = exynos_drm_logo_create;
	exynos_drm_fbdev_gem_ops.destroy = __exit_p(exynos_drm_logo_destroy);
	exynos_drm_fbdev_gem_ops.clear = exynos_drm_logo_clear;

	return 0;
}

static void __exit exynos_drm_logo_exit(void)
{
	return;
}

subsys_initcall(exynos_drm_logo_init);
module_exit(exynos_drm_logo_exit);

MODULE_AUTHOR("YoungJun Cho <yj44.cho@samsung.com>");
MODULE_DESCRIPTION("Exynos DRM Logo");
MODULE_LICENSE("GPL");
