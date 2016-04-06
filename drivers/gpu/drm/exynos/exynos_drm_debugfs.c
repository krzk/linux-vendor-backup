/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors: YoungJun Cho <yj44.cho@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/debugfs.h>

#include "drmP.h"

#include "exynos_drm_drv.h"
#include "exynos_drm_gem.h"

struct exynos_drm_debugfs_gem_info_data {
	struct drm_file *filp;
	struct seq_file *m;
};

static int exynos_drm_debugfs_gem_one_info(int id, void *ptr, void *data)
{
	struct drm_gem_object *obj = (struct drm_gem_object *)ptr;
	struct exynos_drm_debugfs_gem_info_data *gem_info_data = data;
	struct exynos_drm_gem_obj *exynos_gem_obj = to_exynos_gem_obj(obj);
	struct exynos_drm_gem_buf *buf = exynos_gem_obj->buffer;

	seq_printf(gem_info_data->m,
			"%5d\t%5d\t%4d\t%4d\t\t%4d\t0x%08lx\t0x%x\t%4d\t%4d\t\t"
			"%4d\t\t0x%p\t%6d\n",
				exynos_gem_obj->pid,
				exynos_gem_obj->tgid,
				id,
				atomic_read(&obj->refcount.refcount),
				atomic_read(&obj->handle_count),
				exynos_gem_obj->size,
				exynos_gem_obj->flags,
				buf->pfnmap,
				obj->export_dma_buf ? 1 : 0,
				obj->import_attach ? 1 : 0,
				obj,
				obj->name);

	return 0;
}

static int exynos_drm_debugfs_gem_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_device *drm_dev = node->minor->dev;
	struct exynos_drm_debugfs_gem_info_data gem_info_data;

	gem_info_data.m = m;

	seq_printf(gem_info_data.m,
			"pid\ttgid\thandle\trefcount\thcount\tsize\t\tflags\t" \
			"pfnmap\texport_to_fd\timport_from_fd\tobj_addr\t" \
			"name\n");

	mutex_lock(&drm_dev->struct_mutex);

	list_for_each_entry(gem_info_data.filp, &drm_dev->filelist, lhead) {
		spin_lock(&gem_info_data.filp->table_lock);

		/*
		 * [ WIP ] idr_for_each() could not skip when its layers is 0.
		 * That is because current idr_for_each() treats the max as 1
		 * for this case, so it goes into while-loop.
		 * Although this routine was fixed by commit 326cf0f
		 * (idr: fix top layer handling) in 27/2/2013 which makes the
		 * max as 0 then it could skip routine, it is not easy to apply
		 * this patch for huge changes, so adds exception routine
		 * instead.
		 */
		if (!gem_info_data.filp->object_idr.layers) {
			spin_unlock(&gem_info_data.filp->table_lock);
			continue;
		}

		idr_for_each(&gem_info_data.filp->object_idr,
						exynos_drm_debugfs_gem_one_info,
						&gem_info_data);

		spin_unlock(&gem_info_data.filp->table_lock);
	}

	mutex_unlock(&drm_dev->struct_mutex);

	return 0;
}

static struct drm_info_list exynos_drm_debugfs_list[] = {
	{"gem_info", exynos_drm_debugfs_gem_info, DRIVER_GEM},
};
#define EXYNOS_DRM_DEBUGFS_ENTRIES ARRAY_SIZE(exynos_drm_debugfs_list)

int exynos_drm_debugfs_init(struct drm_minor *minor)
{
	drm_debugfs_create_files(exynos_drm_debugfs_list,
					EXYNOS_DRM_DEBUGFS_ENTRIES,
					minor->debugfs_root, minor);

	return 0;
}

void exynos_drm_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(exynos_drm_debugfs_list,
					EXYNOS_DRM_DEBUGFS_ENTRIES, minor);
}
