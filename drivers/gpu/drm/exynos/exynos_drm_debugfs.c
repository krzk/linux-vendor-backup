/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 * Author: YoungJun Cho <yj44.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>

#include <linux/debugfs.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_gem.h"

struct exynos_drm_debugfs_gem_info_data {
	struct drm_file	*filp;
	struct seq_file	*m;
};

static int exynos_drm_debugfs_gem_one_info(int id, void *ptr, void *data)
{
	struct exynos_drm_debugfs_gem_info_data *gem_info_data =
				(struct exynos_drm_debugfs_gem_info_data *)data;
	struct drm_file *filp = gem_info_data->filp;
	struct drm_exynos_file_private *file_priv = filp->driver_priv;
	struct drm_gem_object *obj = (struct drm_gem_object *)ptr;
	struct exynos_drm_gem_obj *exynos_gem_obj = to_exynos_gem_obj(obj);
	struct exynos_drm_gem_buf *buf = exynos_gem_obj->buffer;

	seq_printf(gem_info_data->m,
			"%5d\t%5d\t%4d\t%4d\t%4d\t0x%08lx\t0x%x\t%4d\t%4d\t"
			"%4d\t0x%p\n",
				pid_nr(filp->pid),
				file_priv->tgid,
				id,
				atomic_read(&obj->refcount.refcount),
				obj->handle_count,
				exynos_gem_obj->size,
				exynos_gem_obj->flags,
				buf->pfnmap,
				obj->dma_buf ? 1 : 0,
				obj->import_attach ? 1 : 0,
				obj);

	return 0;
}

static int exynos_drm_debugfs_gem_info(struct seq_file *m, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)m->private;
	struct drm_minor *minor = node->minor;
	struct drm_device *drm_dev = minor->dev;
	struct exynos_drm_debugfs_gem_info_data gem_info_data;
	struct drm_file *filp;

	gem_info_data.m = m;

	seq_puts(m, "pid\ttgid\thandle\tref_cnt\thdl_cnt\tsize\t\tflags\t"
			"pfnmap\texport\timport\tobj_addr\n");

	mutex_lock(&drm_dev->struct_mutex);
	list_for_each_entry(filp, &drm_dev->filelist, lhead) {
		gem_info_data.filp = filp;

		spin_lock(&filp->table_lock);
		idr_for_each(&filp->object_idr, exynos_drm_debugfs_gem_one_info,
				&gem_info_data);
		spin_unlock(&filp->table_lock);
	}
	mutex_unlock(&drm_dev->struct_mutex);

	return 0;
}

static struct drm_info_list exynos_drm_debugfs_list[] = {
	{"gem_info",	exynos_drm_debugfs_gem_info,	DRIVER_GEM},
};
#define EXYNOS_DRM_DEBUGFS_ENTRIES	ARRAY_SIZE(exynos_drm_debugfs_list)

int exynos_drm_debugfs_init(struct drm_minor *minor)
{
	return drm_debugfs_create_files(exynos_drm_debugfs_list,
					EXYNOS_DRM_DEBUGFS_ENTRIES,
					minor->debugfs_root, minor);
}

void exynos_drm_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files(exynos_drm_debugfs_list,
					EXYNOS_DRM_DEBUGFS_ENTRIES, minor);
}
