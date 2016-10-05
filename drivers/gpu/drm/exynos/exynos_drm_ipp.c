/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <plat/map-base.h>

#include <drm/drmP.h>
#include <drm/exynos_drm.h>
#include "exynos_drm_drv.h"
#include "exynos_drm_gem.h"
#include "exynos_drm_ipp.h"
#include "exynos_drm_iommu.h"

/*
 * IPP stands for Image Post Processing and
 * supports image scaler/rotator and input/output DMA operations.
 * using FIMC, GSC, Rotator, so on.
 * IPP is integration device driver of same attribute h/w
 */

/*
 * TODO
 * 1. expand command control id.
 * 2. integrate	property and config.
 * 3. removed send_event id check routine.
 * 4. compare send_event id if needed.
 * 5. free subdrv_remove notifier callback list if needed.
 * 6. need to check subdrv_open about multi-open.
 * 7. need to power_on implement power and sysmmu ctrl.
 */

#define IPP_MAX_MEM	10
#define IPP_STR_LEN	16
#define get_ipp_context(dev)	platform_get_drvdata(to_platform_device(dev))

/*
 * A structure of event.
 *
 * @base: base of event.
 * @event: ipp event.
 */
struct drm_exynos_ipp_send_event {
	struct drm_pending_event	base;
	struct drm_exynos_ipp_event	event;
};

/*
 * A structure of memory node.
 *
 * @list: list head to memory queue information.
 * @ops_id: id of operations.
 * @prop_id: id of property.
 * @buf_id: id of buffer.
 * @buf_info: gem objects and dma address, size.
 * @filp: a pointer to drm_file.
 */
struct drm_exynos_ipp_mem_node {
	struct list_head	list;
	enum drm_exynos_ops_id	ops_id;
	u32	prop_id;
	u32	buf_id;
	struct drm_exynos_ipp_buf_info	buf_info;
	struct drm_file		*filp;
};

/*
 * A structure of ipp context.
 *
 * @subdrv: prepare initialization using subdrv.
 * @ipp_lock: lock for synchronization of access to ipp_idr.
 * @prop_lock: lock for synchronization of access to prop_idr.
 * @ipp_idr: ipp driver idr.
 * @prop_idr: property idr.
 * @num_drv: quantity of ipp drivers.
 */
struct ipp_context {
	struct exynos_drm_subdrv	subdrv;
	struct mutex	ipp_lock;
	struct mutex	prop_lock;
	struct idr	ipp_idr;
	struct idr	prop_idr;
	u32	num_drv;
};

static LIST_HEAD(exynos_drm_ippdrv_list);
static DEFINE_MUTEX(exynos_drm_ippdrv_lock);
static BLOCKING_NOTIFIER_HEAD(exynos_drm_ippnb_list);

#define IPP_AGING_LOG
#ifdef IPP_AGING_LOG
#define IPP_LOG_INFO_MAX 128

enum ipp_log_state {
	IPP_LOG_GET_SYNC,
	IPP_LOG_QUEUE,
	IPP_LOG_WORK,
	IPP_LOG_EVENT,
	IPP_LOG_PUT_SYNC,
	IPP_LOG_MAX,
};

struct ipp_debug_log {
	unsigned long long time;
	pid_t pid;
};

static atomic_t ipp_log_idx[IPP_LOG_MAX] = {
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1),
		ATOMIC_INIT(-1) };
static struct ipp_debug_log ipp_log_info[IPP_LOG_MAX][IPP_LOG_INFO_MAX];

static inline void ipp_save_timing(enum ipp_log_state log_state)
{
	int i;

	if (log_state >= IPP_LOG_MAX)
		return;

	i = atomic_inc_return(&ipp_log_idx[log_state]);
	if (i >= IPP_LOG_INFO_MAX) {
		atomic_set(&ipp_log_idx[log_state], -1);
		i = 0;
	}

	ipp_log_info[log_state][i].time = cpu_clock(raw_smp_processor_id());
	ipp_log_info[log_state][i].pid = task_pid_nr(current);
}
#endif

static int ipp_start_property(struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node);
static int ipp_stop_property(struct drm_device *drm_dev,
		struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node);
static int ipp_send_event(struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node, int *buf_id);

int exynos_drm_ippdrv_register(struct exynos_drm_ippdrv *ippdrv)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ippdrv)
		return -EINVAL;

	mutex_lock(&exynos_drm_ippdrv_lock);
	list_add(&ippdrv->drv_list, &exynos_drm_ippdrv_list);
	mutex_unlock(&exynos_drm_ippdrv_lock);

	return 0;
}

int exynos_drm_ippdrv_unregister(struct exynos_drm_ippdrv *ippdrv)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ippdrv)
		return -EINVAL;

	mutex_lock(&exynos_drm_ippdrv_lock);
	list_del(&ippdrv->drv_list);
	mutex_unlock(&exynos_drm_ippdrv_lock);

	return 0;
}

static int ipp_create_id(struct idr *id_idr, struct mutex *lock, void *obj,
		u32 *idp)
{
	int ret;

	DRM_DEBUG_KMS("%s\n", __func__);

again:
	/* ensure there is space available to allocate a handle */
	if (idr_pre_get(id_idr, GFP_KERNEL) == 0) {
		DRM_ERROR("failed to get idr.\n");
		return -ENOMEM;
	}

	/* do the allocation under our mutexlock */
	mutex_lock(lock);
	ret = idr_get_new_above(id_idr, obj, 1, (int *)idp);
	mutex_unlock(lock);
	if (ret == -EAGAIN)
		goto again;

	return ret;
}

static void ipp_remove_id(struct idr *id_idr, struct mutex *lock, u32 id)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	mutex_lock(lock);

	/* Release reference and decrement refcount. */
	idr_remove(id_idr, id);
	mutex_unlock(lock);
}

static void *ipp_find_obj(struct idr *id_idr, struct mutex *lock, u32 id)
{
	void *obj;

	DRM_DEBUG_KMS("%s:id[%d]\n", __func__, id);

	mutex_lock(lock);

	/* find object using handle */
	obj = idr_find(id_idr, id);
	if (!obj) {
		DRM_ERROR("failed to find object.\n");
		mutex_unlock(lock);
		return ERR_PTR(-ENODEV);
	}

	mutex_unlock(lock);

	return obj;
}

static int ipp_handle_cmd_work(struct device *dev,
		struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_work *cmd_work,
		struct drm_exynos_ipp_cmd_node *c_node)
{
	if (!work_pending((struct work_struct *)cmd_work)) {
		cmd_work->ippdrv = ippdrv;
		cmd_work->c_node = c_node;
		if (!queue_work(c_node->cmd_workq,
			(struct work_struct *)cmd_work)) {
			DRM_INFO("%s:busy to queue_work.\n", __func__);
			return -EBUSY;
		}
	}

	return 0;
}

static int exynos_drm_ipp_ctrl_iommu(struct exynos_drm_ippdrv *ippdrv,
	bool enable)
{
	int ret;

	DRM_INFO("%s:ipp_id[%d]iommu_on[%d]enable[%d]\n",
		__func__, ippdrv->ipp_id, ippdrv->iommu_on, enable);

	if (!is_drm_iommu_supported(ippdrv->drm_dev))
		return 0;

	if (enable == ippdrv->iommu_on)
		return 0;

	if (enable) {
		ret = drm_iommu_attach_device(ippdrv->drm_dev, ippdrv->dev);
		if (ret) {
			DRM_ERROR("failed to activate iommu.\n");
			return ret;
		}
	} else
		drm_iommu_detach_device(ippdrv->drm_dev, ippdrv->dev);

	ippdrv->iommu_on = enable;

	return 0;
}

static inline int ipp_get_link_count(struct exynos_drm_ippdrv *ippdrv)
{
	struct drm_exynos_ipp_cmd_node *c_node;
	int count = 0;

	if (!list_empty(&ippdrv->cmd_list)) {
		list_for_each_entry(c_node, &ippdrv->cmd_list, list)
			count++;
	}

	return count;
}

#ifdef CONFIG_DRM_EXYNOS_IPP_TC
static int ipp_move_cmd_node(struct drm_exynos_ipp_cmd_node *c_node,
	struct exynos_drm_ippdrv *src, struct exynos_drm_ippdrv *dst)
{
	struct drm_exynos_ipp_cmd_work *cmd_work;
	int ret, protect = c_node->property.protect;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (list_empty(&src->cmd_list))
		return -EINVAL;

	DRM_INFO("%s:prop_id[%d]ippdrv[%d,0x%x->%d,0x%x]\n",
		__func__, c_node->property.prop_id,
		src->ipp_id, (int)src, dst->ipp_id, (int)dst);

	if (c_node->state == IPP_STATE_START) {
		cmd_work = c_node->stop_work;
		cmd_work->ctrl = IPP_CTRL_STOP;
		ret = ipp_handle_cmd_work(src->parent_dev, src, cmd_work,
			c_node);
		if (ret) {
			DRM_ERROR("failed to handle_cmd_work.\n");
			return ret;
		}

		if (!wait_for_completion_timeout(&c_node->stop_complete,
		    msecs_to_jiffies(300))) {
			DRM_ERROR("timeout stop:prop_id[%d]\n",
				c_node->property.prop_id);
		}
	}

	list_del(&c_node->list);

	if (list_empty(&src->cmd_list)) {
		if (!protect && src->iommu_on)
			exynos_drm_ipp_ctrl_iommu(src, false);
		src->c_node = NULL;
	}

	list_add_tail(&c_node->list, &dst->cmd_list);
	if (!protect && !dst->iommu_on)
		exynos_drm_ipp_ctrl_iommu(dst, true);

	return 0;
}

static struct exynos_drm_ippdrv *
	ipp_get_migration_driver(struct drm_exynos_ipp_property *property)
{
	struct exynos_drm_ippdrv *ippdrv = NULL;

	DRM_DEBUG_KMS("%s\n", __func__);

	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, drv_list) {
		if (ippdrv->dedicated)
			continue;

		if (ippdrv->check_property &&
		    ippdrv->check_property(ippdrv->dev, property))
			continue;

		DRM_DEBUG_KMS("%s:ippdrv[0x%x]ipp_id[%d]\n", __func__,
			(int)ippdrv, ippdrv->ipp_id);
		return ippdrv;
	}

	return ERR_PTR(-ENODEV);
}

static int ipp_commit_migration(struct ipp_context *ctx,
	struct exynos_drm_ippdrv *src, bool traffic_ctrl)
{
	struct exynos_drm_ippdrv *ippdrv, *tippdrv, *dst;
	struct drm_exynos_ipp_cmd_node *c_node, *tc_node;
	struct drm_exynos_ipp_property *property;
	int diff, ret = 0;
	bool move = true;

	DRM_DEBUG_KMS("%s\n", __func__);

	list_for_each_entry_safe(c_node, tc_node, &src->cmd_list, list) {
		property = &c_node->property;

		list_for_each_entry_safe(ippdrv, tippdrv,
		    &exynos_drm_ippdrv_list, drv_list) {
			if (ippdrv == src || ippdrv->dedicated)
				continue;

			if (ippdrv->check_property &&
			    ippdrv->check_property(ippdrv->dev, property))
				continue;

			dst = ippdrv;

			/*
			* Traffic control policy is as below.
			* 1. If one property could be assigned various ippdrv,
			*	assigns it to ippdrv has lower id.
			* 2. If multi opeation is running on one ippdrv, even if
			*	other available ippdrv is idle, migrates running
			*	c_node to idle ippdrv.
			* 3. If multi opeation is running on above two ippdrv
			*	and linked counts are different, migrates c_node
			*	from higher to available lower ippdrv.
			*/
			if (traffic_ctrl) {
				diff = ipp_get_link_count(src) -
				    ipp_get_link_count(dst);
				if (((src->ipp_id > dst->ipp_id) &&
				    (diff < 1))  ||
				    ((src->ipp_id < dst->ipp_id) &&
				    (diff < 2)))
					move = false;
			}

			if (move) {
				if (!ipp_move_cmd_node(c_node, src, dst))
					ret++;
			}
		}
	}

	DRM_DEBUG_KMS("%s:moved[%d]\n", __func__, ret);

	return ret;
}

static int ipp_traffic_control(struct ipp_context *ctx)
{
	struct exynos_drm_ippdrv *ippdrv;
	int ipp_id, sc, ret = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	for (ipp_id = ctx->num_drv; ipp_id > 0; ipp_id--) {
		ippdrv = ipp_find_obj(&ctx->ipp_idr, &ctx->ipp_lock, ipp_id);
		if (IS_ERR_OR_NULL(ippdrv)) {
			DRM_ERROR("not found ipp%d driver.\n", ipp_id);
			break;
		}

		sc  = ipp_get_link_count(ippdrv);
		if (!sc || ippdrv->dedicated)
			continue;

		ret += ipp_commit_migration(ctx, ippdrv, true);
	}

	DRM_DEBUG_KMS("%s:moved[%d]\n", __func__, ret);

	return ret;
}
#endif

static inline bool ipp_check_dedicated(struct exynos_drm_ippdrv *ippdrv,
		enum drm_exynos_ipp_cmd	cmd)
{
	/*
	 * check dedicated flag and WB, OUTPUT operation with
	 * power on state.
	 */
	if (ippdrv->dedicated || (!ipp_is_m2m_cmd(cmd) &&
	    ippdrv->c_node))
		return true;

	return false;
}

static struct exynos_drm_ippdrv *ipp_find_driver(struct ipp_context *ctx,
		struct drm_exynos_ipp_property *property)
{
	struct exynos_drm_ippdrv *ippdrv;
	u32 ipp_id = property->ipp_id;

	DRM_INFO("%s:ipp_id[%d]cmd[%d]\n", __func__, ipp_id, property->cmd);

	if (ipp_id) {
		/* find ipp driver using idr */
		ippdrv = ipp_find_obj(&ctx->ipp_idr, &ctx->ipp_lock,
			ipp_id);
		if (IS_ERR_OR_NULL(ippdrv)) {
			DRM_ERROR("not found ipp%d driver.\n", ipp_id);
			return ippdrv;
		}

		/*
		 * WB, OUTPUT opertion not supported multi-operation.
		 * so, make dedicated state at set property ioctl.
		 * when ipp driver finished operations, clear dedicated flags.
		 */
		if (ipp_check_dedicated(ippdrv, property->cmd)) {
			DRM_ERROR("already used choose device.\n");
			return ERR_PTR(-EBUSY);
		}

		/*
		 * This is necessary to find correct device in ipp drivers.
		 * ipp drivers have different abilities,
		 * so need to check property.
		 */
		if (ippdrv->check_property &&
		    ippdrv->check_property(ippdrv->dev, property)) {
			DRM_ERROR("not support property.\n");
			return ERR_PTR(-EINVAL);
		}

		return ippdrv;
	} else {
		/*
		 * This case is search all ipp driver for finding.
		 * user application don't set ipp_id in this case,
		 * so ipp subsystem search correct driver in driver list.
		 */
		struct exynos_drm_ippdrv *tippdrv = NULL;
		int sc, dc = 0;

#ifdef CONFIG_DRM_EXYNOS_IPP_TC
retry:
#endif
		list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, drv_list) {
			sc = ipp_get_link_count(ippdrv);

			if (ipp_check_dedicated(ippdrv, property->cmd)) {
				DRM_DEBUG_KMS("%s:used device.\n", __func__);
				continue;
			}

			if (ippdrv->check_property &&
			    ippdrv->check_property(ippdrv->dev, property)) {
				DRM_DEBUG_KMS("%s:not support property.\n",
					__func__);
				continue;
			}

			if (!tippdrv || (tippdrv && dc > sc)) {
				tippdrv = ippdrv;
				dc = sc;
			}
		}

		if (tippdrv)
			return tippdrv;

#ifdef CONFIG_DRM_EXYNOS_IPP_TC
		/*
		 * All of the ipp subdrivers are running.
		 * In this case, ipp tries to search availabe driver to assign
		 * incoming property.
		 * If found, migrate one ippdrv to other suitable driver,
		 * and then retry to assign new property on drained driver.
		 */
		tippdrv = ipp_get_migration_driver(property);
		if (!IS_ERR_OR_NULL(tippdrv)) {
			if (ipp_get_link_count(tippdrv) ==
			    ipp_commit_migration(ctx, tippdrv, false))
				goto retry;
		}
#endif
		DRM_ERROR("not support ipp driver operations.\n");
	}

	return ERR_PTR(-ENODEV);
}

static struct exynos_drm_ippdrv *ipp_find_drv_by_handle(u32 prop_id)
{
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node;
	int count = 0;

	DRM_DEBUG_KMS("%s:prop_id[%d]\n", __func__, prop_id);

	if (list_empty(&exynos_drm_ippdrv_list)) {
		DRM_DEBUG_KMS("%s:ippdrv_list is empty.\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	/*
	 * This case is search ipp driver by prop_id handle.
	 * sometimes, ipp subsystem find driver by prop_id.
	 * e.g PAUSE state, queue buf, command contro.
	 */
	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, drv_list) {
		DRM_DEBUG_KMS("%s:count[%d]ippdrv[0x%x]\n", __func__,
			count++, (int)ippdrv);

		if (!list_empty(&ippdrv->cmd_list)) {
			list_for_each_entry(c_node, &ippdrv->cmd_list, list)
				if (c_node->property.prop_id == prop_id)
					return ippdrv;
		}
	}

	return ERR_PTR(-ENODEV);
}

int exynos_drm_ipp_get_property(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct device *dev = priv->dev;
	struct ipp_context *ctx = get_ipp_context(dev);
	struct drm_exynos_ipp_prop_list *prop_list = data;
	struct exynos_drm_ippdrv *ippdrv;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ctx) {
		DRM_ERROR("invalid context.\n");
		return -EINVAL;
	}

	if (!prop_list) {
		DRM_ERROR("invalid property parameter.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("%s:ipp_id[%d]\n", __func__, prop_list->ipp_id);

	/*
	 * Supports ippdrv list count for user application.
	 * First step user application getting ippdrv count.
	 * and second step getting ippdrv capability using ipp_id.
	 */
	prop_list->num_drv = ctx->num_drv;
	prop_list->version = 1;

	if (prop_list->ipp_id) {
		/*
		 * Getting ippdrv capability by ipp_id.
		 * some deivce not supported wb, output interface.
		 * so, user application detect correct ipp driver
		 * using this ioctl.
		 */
		ippdrv = ipp_find_obj(&ctx->ipp_idr, &ctx->ipp_lock,
						prop_list->ipp_id);
		if (IS_ERR_OR_NULL(ippdrv)) {
			DRM_ERROR("not found ipp%d driver.\n",
					prop_list->ipp_id);
			return -EINVAL;
		}

		memcpy(&prop_list->capability, ippdrv->capability,
			sizeof(struct drm_exynos_ipp_capability));

		prop_list->link_count = ipp_get_link_count(ippdrv);
		prop_list->cur_cmd = ippdrv->c_node->property.cmd;
	}

	return 0;
}

static void ipp_print_property(struct drm_exynos_ipp_property *property,
		int idx)
{
	struct drm_exynos_ipp_config *config = &property->config[idx];
	struct drm_exynos_pos *pos = &config->pos;
	struct drm_exynos_sz *sz = &config->sz;

	DRM_INFO("%s:prop_id[%d]ops[%s]fmt[0x%x]\n",
		__func__, property->prop_id, idx ? "dst" : "src", config->fmt);

	DRM_INFO("%s:pos[%d %d %d %d]sz[%d %d]f[%d]r[%d]\n",
		__func__, pos->x, pos->y, pos->w, pos->h,
		sz->hsize, sz->vsize, config->flip, config->degree);
}

static int ipp_find_and_set_property(struct drm_exynos_ipp_property *property)
{
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node;
	u32 prop_id = property->prop_id;

	DRM_DEBUG_KMS("%s:prop_id[%d]\n", __func__, prop_id);

	ippdrv = ipp_find_drv_by_handle(prop_id);
	if (IS_ERR_OR_NULL(ippdrv)) {
		DRM_ERROR("failed to get ipp driver.\n");
		return -EINVAL;
	}

	/*
	 * Find command node using command list in ippdrv.
	 * when we find this command no using prop_id.
	 * return property information set in this command node.
	 */
	list_for_each_entry(c_node, &ippdrv->cmd_list, list) {
		if ((c_node->property.prop_id == prop_id) &&
		    (c_node->state == IPP_STATE_STOP)) {
			DRM_DEBUG_KMS("%s:found cmd[%d]ippdrv[0x%x]\n",
				__func__, property->cmd, (int)ippdrv);

			c_node->property = *property;
			return 0;
		}
	}

	DRM_ERROR("failed to search property.\n");

	return -EINVAL;
}

static struct workqueue_struct *ipp_create_work_queue(char *str, u32 prop_id)
{
	struct workqueue_struct *workq;
	char name[IPP_STR_LEN];

	DRM_DEBUG_KMS("%s\n", __func__);

	memset(name, 0, IPP_STR_LEN);
	sprintf(name, "ipp_%s_%d", str, prop_id);

	workq = create_singlethread_workqueue(name);
	if (!workq) {
		DRM_ERROR("failed to create workq.\n");
		return ERR_PTR(-ENOMEM);
	}

	return workq;
}

static struct drm_exynos_ipp_cmd_work *ipp_create_cmd_work(void)
{
	struct drm_exynos_ipp_cmd_work *cmd_work;

	DRM_DEBUG_KMS("%s\n", __func__);

	cmd_work = kzalloc(sizeof(*cmd_work), GFP_KERNEL);
	if (!cmd_work) {
		DRM_ERROR("failed to alloc cmd_work.\n");
		return ERR_PTR(-ENOMEM);
	}

	INIT_WORK((struct work_struct *)cmd_work, ipp_sched_cmd);

	return cmd_work;
}

static struct drm_exynos_ipp_event_info *ipp_create_event_info(void)
{
	struct drm_exynos_ipp_event_info *event;

	DRM_DEBUG_KMS("%s\n", __func__);

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		DRM_ERROR("failed to alloc event.\n");
		return ERR_PTR(-ENOMEM);
	}

	return event;
}

int exynos_drm_ipp_set_property(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct device *dev = priv->dev;
	struct ipp_context *ctx = get_ipp_context(dev);
	struct drm_exynos_ipp_property *property = data;
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node;
	int ret, i;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ctx) {
		DRM_ERROR("invalid context.\n");
		return -EINVAL;
	}

	if (!property) {
		DRM_ERROR("invalid property parameter.\n");
		return -EINVAL;
	}

	/*
	 * This is log print for user application property.
	 * user application set various property.
	 */
	for_each_ipp_ops(i)
		ipp_print_property(property, i);

	/*
	 * set property ioctl generated new prop_id.
	 * but in this case already asigned prop_id using old set property.
	 * e.g PAUSE state. this case supports find current prop_id and use it
	 * instead of allocation.
	 */
	if (property->prop_id) {
		DRM_DEBUG_KMS("%s:prop_id[%d]\n", __func__, property->prop_id);
		return ipp_find_and_set_property(property);
	}

	/* find ipp driver using ipp id */
	ippdrv = ipp_find_driver(ctx, property);
	if (IS_ERR_OR_NULL(ippdrv)) {
		DRM_ERROR("failed to get ipp driver.\n");
		return -EINVAL;
	}

	/* allocate command node */
	c_node = kzalloc(sizeof(*c_node), GFP_KERNEL);
	if (!c_node) {
		DRM_ERROR("failed to allocate map node.\n");
		return -ENOMEM;
	}

	/* create property id */
	ret = ipp_create_id(&ctx->prop_idr, &ctx->prop_lock, c_node,
		&property->prop_id);
	if (ret) {
		DRM_ERROR("failed to create id.\n");
		goto err_clear;
	}

	DRM_INFO("%s:created prop_id[%d]cmd[%d]ippdrv[0x%x]ipp_id[%d]type[%d]\n",
		__func__, property->prop_id, property->cmd,
		(int)ippdrv, ippdrv->ipp_id, property->type);

	/* stored property information and ippdrv in private data */
	c_node->priv = priv;
	c_node->property = *property;
	c_node->state = IPP_STATE_IDLE;

	if (property->type & IPP_EVENT_DRIVEN) {
		/*
		 * create single thread for ipp command and event.
		 * IPP supports command thread for user process.
		 * user process make command node using set property ioctl.
		 * and make start_work and send this work to command thread.
		 * and then this command thread start property.
		 */
		c_node->cmd_workq = ipp_create_work_queue("cmd", property->prop_id);
		if (IS_ERR_OR_NULL(c_node->cmd_workq)) {
			DRM_ERROR("failed to create cmd workq.\n");
			goto err_clear;
		}

		c_node->start_work = ipp_create_cmd_work();
		if (IS_ERR_OR_NULL(c_node->start_work)) {
			DRM_ERROR("failed to create start work.\n");
			goto err_cmd_workq;
		}

		c_node->stop_work = ipp_create_cmd_work();
		if (IS_ERR_OR_NULL(c_node->stop_work)) {
			DRM_ERROR("failed to create stop work.\n");
			goto err_free_start;
		}

		init_completion(&c_node->stop_complete);
	}

	c_node->event = ipp_create_event_info();
	if (IS_ERR_OR_NULL(c_node->event)) {
		DRM_ERROR("failed to create event work.\n");
		goto err_free_stop;
	}

	mutex_init(&c_node->cmd_lock);
	mutex_init(&c_node->mem_lock);
	mutex_init(&c_node->event_lock);
	init_completion(&c_node->start_complete);

	for_each_ipp_ops(i)
		INIT_LIST_HEAD(&c_node->mem_list[i]);

	INIT_LIST_HEAD(&c_node->event_list);
	list_splice_init(&priv->event_list, &c_node->event_list);
	list_add_tail(&c_node->list, &ippdrv->cmd_list);

	/* make dedicated state without m2m */
	if (!ipp_is_m2m_cmd(property->cmd))
		ippdrv->dedicated = true;

#ifdef CONFIG_DRM_EXYNOS_IPP_TC
	ipp_traffic_control(ctx);
#endif

	return 0;

err_free_stop:
	if (property->type & IPP_EVENT_DRIVEN)
		kfree(c_node->stop_work);
err_free_start:
	if (property->type & IPP_EVENT_DRIVEN)
		kfree(c_node->start_work);
err_cmd_workq:
	if (property->type & IPP_EVENT_DRIVEN)
		destroy_workqueue(c_node->cmd_workq);
err_clear:
	kfree(c_node);
	return ret;
}

static void ipp_clean_cmd_node(struct ipp_context *ctx,
		struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node)
{
	struct drm_exynos_ipp_property *property = &c_node->property;

	DRM_DEBUG_KMS("%s\n", __func__);

	mutex_lock(&c_node->cmd_lock);

	ippdrv->dedicated = false;

	/* delete list */
	list_del(&c_node->list);

	if (list_empty(&ippdrv->cmd_list)) {
		if (!property->protect)
			exynos_drm_ipp_ctrl_iommu(ippdrv, false);

		ippdrv->c_node = NULL;
	}

	mutex_unlock(&c_node->cmd_lock);

	ipp_remove_id(&ctx->prop_idr, &ctx->prop_lock,
		property->prop_id);

	/* destroy mutex */
	mutex_destroy(&c_node->cmd_lock);
	mutex_destroy(&c_node->mem_lock);
	mutex_destroy(&c_node->event_lock);

	if (property->type & IPP_EVENT_DRIVEN) {
		destroy_workqueue(c_node->cmd_workq);

		/* free command node */
		kfree(c_node->start_work);
		kfree(c_node->stop_work);
	}

	kfree(c_node->event);
	kfree(c_node);
}

static int ipp_check_mem_list(struct drm_exynos_ipp_cmd_node *c_node)
{
	struct drm_exynos_ipp_property *property = &c_node->property;
	struct drm_exynos_ipp_mem_node *m_node;
	struct list_head *head;
	int ret, i, count[EXYNOS_DRM_OPS_MAX] = { 0, };

	DRM_DEBUG_KMS("%s\n", __func__);

	for_each_ipp_ops(i) {
		/* source/destination memory list */
		head = &c_node->mem_list[i];

		if (list_empty(head)) {
			DRM_DEBUG_KMS("%s:%s memory empty.\n", __func__,
				i ? "dst" : "src");
			continue;
		}

		/* find memory node entry */
		list_for_each_entry(m_node, head, list) {
			DRM_DEBUG_KMS("%s:%s,count[%d]m_node[0x%x]\n", __func__,
				i ? "dst" : "src", count[i], (int)m_node);
			count[i]++;
		}
	}

	DRM_DEBUG_KMS("%s:min[%d]max[%d]\n", __func__,
		min(count[EXYNOS_DRM_OPS_SRC], count[EXYNOS_DRM_OPS_DST]),
		max(count[EXYNOS_DRM_OPS_SRC], count[EXYNOS_DRM_OPS_DST]));

	if (max(count[EXYNOS_DRM_OPS_SRC], count[EXYNOS_DRM_OPS_DST]) >
		IPP_MAX_MEM) {
		for_each_ipp_ops(i)
			DRM_ERROR("Too many mem_list [%s %d]]\n",
				i ? "dst" : "src", count[i]);
		dump_stack();
		panic("%s\n", __func__);
	}

	/*
	 * M2M operations should be need paired memory address.
	 * so, need to check minimum count about src, dst.
	 * other case not use paired memory, so use maximum count
	 */
	if (ipp_is_m2m_cmd(property->cmd))
		ret = min(count[EXYNOS_DRM_OPS_SRC],
			count[EXYNOS_DRM_OPS_DST]);
	else
		ret = max(count[EXYNOS_DRM_OPS_SRC],
			count[EXYNOS_DRM_OPS_DST]);

	return ret;
}

static struct drm_exynos_ipp_mem_node
		*ipp_find_mem_node(struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_queue_buf *qbuf)
{
	struct drm_exynos_ipp_mem_node *m_node;
	struct list_head *head;
	int count = 0;

	DRM_DEBUG_KMS("%s:buf_id[%d]\n", __func__, qbuf->buf_id);

	/* source/destination memory list */
	head = &c_node->mem_list[qbuf->ops_id];

	/* find memory node from memory list */
	list_for_each_entry(m_node, head, list) {
		DRM_DEBUG_KMS("%s:count[%d]m_node[0x%x]\n",
			__func__, count++, (int)m_node);

		/* compare buffer id */
		if (m_node->buf_id == qbuf->buf_id)
			return m_node;
	}

	return NULL;
}

static int ipp_set_mem_node(struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_mem_node *m_node)
{
	struct exynos_drm_ipp_ops *ops = NULL;
	int ret = 0;

	DRM_DEBUG_KMS("%s:node[0x%x]\n", __func__, (int)m_node);

	if (!m_node) {
		DRM_ERROR("invalid queue node.\n");
		return -EFAULT;
	}

	DRM_DEBUG_KMS("%s:ops_id[%d]\n", __func__, m_node->ops_id);

	/* get operations callback */
	ops = ippdrv->ops[m_node->ops_id];
	if (!ops) {
		DRM_ERROR("not support ops.\n");
		return -EFAULT;
	}

	/* set address and enable irq */
	if (ops->set_addr) {
		ret = ops->set_addr(ippdrv->dev, &m_node->buf_info,
			m_node->buf_id, IPP_BUF_ENQUEUE);
		if (ret) {
			DRM_ERROR("failed to set addr.\n");
			return ret;
		}
	}

	return ret;
}

static int ipp_put_mem_node(struct drm_device *drm_dev,
		struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_mem_node *m_node)
{
	int i;

	DRM_DEBUG_KMS("%s:node[0x%x]\n", __func__, (int)m_node);

	if (!m_node) {
		DRM_ERROR("invalid dequeue node.\n");
		return -EFAULT;
	}

	DRM_DEBUG_KMS("%s:ops_id[%d]\n", __func__, m_node->ops_id);

	/* put gem buffer */
	for_each_ipp_planar(i) {
		unsigned long handle = m_node->buf_info.handles[i];
		if (handle) {
			if (exynos_drm_gem_put_dma_addr(drm_dev, handle,
							m_node->filp)) {
				DRM_ERROR("prop_id[%d][%s]hd[%d]\n",
					c_node->property.prop_id,
					m_node->ops_id ? "dst" : "src",
					(int)handle);
				WARN_ON(1);
			}
		}
	}

	/* delete list in queue */
	list_del(&m_node->list);
	kfree(m_node);

	return 0;
}

static struct drm_exynos_ipp_mem_node
		*ipp_get_mem_node(struct drm_device *drm_dev,
		struct drm_file *file,
		struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_queue_buf *qbuf)
{
	struct drm_exynos_ipp_mem_node *m_node;
	struct drm_exynos_ipp_buf_info buf_info;
	void *addr;
	unsigned long size;
	int protect = c_node->property.protect, i;

	DRM_DEBUG_KMS("%s\n", __func__);

	m_node = kzalloc(sizeof(*m_node), GFP_KERNEL);
	if (!m_node) {
		DRM_ERROR("failed to allocate queue node.\n");
		return ERR_PTR(-ENOMEM);
	}

	/* clear base address for error handling */
	memset(&buf_info, 0x0, sizeof(buf_info));

	/* operations, buffer id */
	m_node->ops_id = qbuf->ops_id;
	m_node->prop_id = qbuf->prop_id;
	m_node->buf_id = qbuf->buf_id;
	INIT_LIST_HEAD(&m_node->list);

	DRM_DEBUG_KMS("%s:m_node[0x%x]ops_id[%d]\n", __func__,
		(int)m_node, qbuf->ops_id);
	DRM_DEBUG_KMS("%s:prop_id[%d]buf_id[%d]\n", __func__,
		qbuf->prop_id, m_node->buf_id);

	for_each_ipp_planar(i) {
		DRM_DEBUG_KMS("%s:i[%d]handle[0x%x]\n", __func__,
			i, qbuf->handle[i]);

		/* get dma address by handle */
		if (qbuf->handle[i]) {
			if (protect)
				addr = (void *) exynos_drm_gem_get_phys_addr(
					drm_dev, qbuf->handle[i], file);
			else
				addr = exynos_drm_gem_get_dma_addr(drm_dev,
						qbuf->handle[i], file);

			if (IS_ERR_OR_NULL(addr)) {
				DRM_ERROR("protect[%d]:failed to get addr.\n",
					protect);
				ipp_put_mem_node(drm_dev, c_node, m_node);
				return ERR_PTR(-EFAULT);
			}

			size = exynos_drm_gem_get_size(drm_dev,
						qbuf->handle[i], file);
			if (!size) {
				DRM_ERROR("failed to get size.\n");
				ipp_put_mem_node(drm_dev, c_node, m_node);
				return ERR_PTR(-EINVAL);
			}

			buf_info.handles[i] = qbuf->handle[i];
			buf_info.base[i] = protect ?
				(dma_addr_t) addr : *(dma_addr_t *) addr;
			buf_info.size[i] = (uint64_t) size;
			DRM_DEBUG_KMS("%s:i[%d]base[0x%x]hd[0x%x]sz[%d]\n",
				__func__, i, buf_info.base[i],
				(int)buf_info.handles[i],
				(int)buf_info.size[i]);
		}
	}

	m_node->filp = file;
	m_node->buf_info = buf_info;
	mutex_lock(&c_node->mem_lock);
	list_add_tail(&m_node->list, &c_node->mem_list[qbuf->ops_id]);
	mutex_unlock(&c_node->mem_lock);

	return m_node;
}

static void ipp_free_event(struct drm_pending_event *event)
{
	kfree(event);
}

static int ipp_get_event(struct drm_device *drm_dev,
		struct drm_file *file,
		struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_queue_buf *qbuf)
{
	struct drm_exynos_ipp_send_event *e;
	unsigned long flags;

	DRM_DEBUG_KMS("%s:ops_id[%d]buf_id[%d]\n", __func__,
		qbuf->ops_id, qbuf->buf_id);

	e = kzalloc(sizeof(*e), GFP_KERNEL);

	if (!e) {
		DRM_ERROR("failed to allocate event.\n");
		spin_lock_irqsave(&drm_dev->event_lock, flags);
		file->event_space += sizeof(e->event);
		spin_unlock_irqrestore(&drm_dev->event_lock, flags);
		return -ENOMEM;
	}

	/* make event */
	e->event.base.type = DRM_EXYNOS_IPP_EVENT;
	e->event.base.length = sizeof(e->event);
	e->event.user_data = qbuf->user_data;
	e->event.prop_id = qbuf->prop_id;
	e->event.buf_id[EXYNOS_DRM_OPS_DST] = qbuf->buf_id;
	e->base.event = &e->event.base;
	e->base.file_priv = file;
	e->base.destroy = ipp_free_event;

	mutex_lock(&c_node->event_lock);
	list_add_tail(&e->base.link, &c_node->event_list);
	mutex_unlock(&c_node->event_lock);

	return 0;
}

static void ipp_put_event(struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_queue_buf *qbuf)
{
	struct drm_exynos_ipp_send_event *e, *te;
	int count = 0;

	DRM_INFO("%s\n", __func__);

	mutex_lock(&c_node->event_lock);
	if (list_empty(&c_node->event_list)) {
		DRM_INFO("%s:event_list is empty.\n", __func__);
		goto out_unlock;
	}

	list_for_each_entry_safe(e, te, &c_node->event_list, base.link) {
		DRM_INFO("%s:count[%d]e[0x%x]\n",
			__func__, count++, (int)e);

		/*
		 * quf == NULL condition means all event deletion.
		 * stop operations want to delete all event list.
		 * another case delete only same buf id.
		 */
		if (!qbuf) {
			/* delete list */
			list_del(&e->base.link);
			kfree(e);
			e = NULL;
		}

		/* compare buffer id */
		if (qbuf && (qbuf->buf_id ==
		    e->event.buf_id[EXYNOS_DRM_OPS_DST])) {
			/* delete list */
			list_del(&e->base.link);
			kfree(e);
			e = NULL;
			goto out_unlock;
		}
	}

out_unlock:
	mutex_unlock(&c_node->event_lock);
	return;
}

static int ipp_queue_buf_with_run(struct device *dev,
		struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_mem_node *m_node,
		struct drm_exynos_ipp_queue_buf *qbuf)
{
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_property *property;
	struct exynos_drm_ipp_ops *ops;
	int ret;

	DRM_DEBUG_KMS("%s\n", __func__);

	ippdrv = ipp_find_drv_by_handle(qbuf->prop_id);
	if (IS_ERR_OR_NULL(ippdrv)) {
		DRM_ERROR("failed to get ipp driver.\n");
		return -EFAULT;
	}

	ops = ippdrv->ops[qbuf->ops_id];
	if (!ops) {
		DRM_ERROR("failed to get ops.\n");
		return -EFAULT;
	}

	property = &c_node->property;

	if (c_node->state != IPP_STATE_START) {
		DRM_INFO("%s:bypass for invalid state.\n" , __func__);
		return 0;
	}

	/*
	 * If set destination buffer and enabled clock,
	 * then m2m operations need start operations at queue_buf
	 */
	if (!ipp_is_wb_cmd(property->cmd)) {
		if (property->type & IPP_EVENT_DRIVEN) {
			struct drm_exynos_ipp_cmd_work *cmd_work = c_node->start_work;

			mutex_lock(&c_node->mem_lock);
			if (!ipp_check_mem_list(c_node)) {
				DRM_INFO("%s:empty memory.\n", __func__);
				mutex_unlock(&c_node->mem_lock);
				return 0;
			}

			cmd_work->ctrl = IPP_CTRL_PLAY;
			ret = ipp_handle_cmd_work(dev, ippdrv, cmd_work, c_node);
			if (ret)
				DRM_INFO("%s:failed to cmd_work.\n", __func__);

			mutex_unlock(&c_node->mem_lock);
		} else {
			mutex_lock(&ippdrv->drv_lock);

			if (completion_done(&c_node->start_complete))
				INIT_COMPLETION(c_node->start_complete);

			ret = ipp_start_property(ippdrv, c_node);
			if (ret) {
				DRM_INFO("%s:failed to start property:prop_id[%d]\n",
					__func__, c_node->property.prop_id);
				ipp_stop_property(ippdrv->drm_dev, ippdrv, c_node);
			}

			mutex_unlock(&ippdrv->drv_lock);
		}
	} else {
		ret = ipp_set_mem_node(ippdrv, c_node, m_node);
		if (ret)
			DRM_ERROR("failed to set m node.\n");
	}

	return ret;
}

static void ipp_clean_queue_buf(struct drm_device *drm_dev,
		struct drm_exynos_ipp_cmd_node *c_node,
		struct drm_exynos_ipp_queue_buf *qbuf)
{
	struct drm_exynos_ipp_mem_node *m_node, *tm_node;
	int ret;
	int i, cnt = 0, hdl = 0;

	DRM_INFO("%s:ops_id[%d]buf_id[%d]\n", __func__,
		qbuf->ops_id, qbuf->buf_id);

	mutex_lock(&c_node->mem_lock);
	if (!list_empty(&c_node->mem_list[qbuf->ops_id])) {
		/* delete list */
		list_for_each_entry_safe(m_node, tm_node,
			&c_node->mem_list[qbuf->ops_id], list) {
			if (m_node->buf_id == qbuf->buf_id &&
			    m_node->ops_id == qbuf->ops_id) {
				for_each_ipp_planar(i) {
					hdl = m_node->buf_info.handles[i];
					if (hdl)
						DRM_INFO(
							"%s:put cnt[%d]hdl[%d]\n",
							__func__, ++cnt, hdl);
				}

				if (cnt > IPP_MAX_MEM) {
					dump_stack();
					panic("%s\n", __func__);
				}

				ret = ipp_put_mem_node(drm_dev, c_node, m_node);
				if (ret)
					DRM_ERROR("failed to put m_node.\n");
			}
		}
	}
	mutex_unlock(&c_node->mem_lock);
}

int exynos_drm_ipp_queue_buf(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct device *dev = priv->dev;
	struct ipp_context *ctx = get_ipp_context(dev);
	struct drm_exynos_ipp_queue_buf *qbuf = data;
	struct drm_exynos_ipp_cmd_node *c_node;
	struct drm_exynos_ipp_property *property;
	struct drm_exynos_ipp_mem_node *m_node;
	int ret;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!qbuf) {
		DRM_ERROR("invalid buf parameter.\n");
		return -EINVAL;
	}

	if (qbuf->ops_id >= EXYNOS_DRM_OPS_MAX) {
		DRM_ERROR("invalid ops parameter.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("%s:prop_id[%d]ops_id[%s]buf_id[%d]buf_type[%d]\n",
		__func__, qbuf->prop_id, qbuf->ops_id ? "dst" : "src",
		qbuf->buf_id, qbuf->buf_type);

#ifdef IPP_AGING_LOG
	ipp_save_timing(IPP_LOG_QUEUE);
#endif

	/* find command node */
	c_node = ipp_find_obj(&ctx->prop_idr, &ctx->prop_lock,
		qbuf->prop_id);
	if (IS_ERR_OR_NULL(c_node)) {
		DRM_ERROR("failed to get cmd node prop_id[%d]\n",
			qbuf->prop_id);
		return -EFAULT;
	}

	property = &c_node->property;

	/* buffer control */
	switch (qbuf->buf_type) {
	case IPP_BUF_ENQUEUE:
		/* get memory node */
		m_node = ipp_get_mem_node(drm_dev, file, c_node, qbuf);
		if (IS_ERR(m_node)) {
			DRM_ERROR("failed to get m_node.\n");
			return PTR_ERR(m_node);
		}

		/*
		 * first step get event for destination buffer.
		 * and second step when M2M case run with destination buffer
		 * if needed.
		 */
		if (qbuf->ops_id == EXYNOS_DRM_OPS_DST ||
		    ipp_is_output_cmd(property->cmd)) {
			/* get event for destination buffer */
			ret = ipp_get_event(drm_dev, file, c_node, qbuf);
			if (ret) {
				DRM_ERROR("failed to get event.\n");
				goto err_clean_node;
			}

			/*
			 * M2M case run play control for streaming feature.
			 * other case set address and waiting.
			 */
			ret = ipp_queue_buf_with_run(dev, c_node, m_node, qbuf);
			if (ret) {
				DRM_ERROR("failed to run command.\n");
				goto err_put_event;
			}
		}
		break;
	case IPP_BUF_DEQUEUE:
		mutex_lock(&c_node->cmd_lock);

		/* put event for destination buffer */
		if (qbuf->ops_id == EXYNOS_DRM_OPS_DST ||
		    ipp_is_output_cmd(property->cmd))
			ipp_put_event(c_node, qbuf);

		ipp_clean_queue_buf(drm_dev, c_node, qbuf);

		mutex_unlock(&c_node->cmd_lock);
		break;
	default:
		DRM_ERROR("invalid buffer control.\n");
		return -EINVAL;
	}

	return 0;

err_put_event:
	DRM_ERROR("clean event.\n");
	ipp_put_event(c_node, qbuf);

err_clean_node:
	DRM_ERROR("clean memory nodes.\n");
	ipp_clean_queue_buf(drm_dev, c_node, qbuf);

	return ret;
}

static bool exynos_drm_ipp_check_valid(struct device *dev,
		enum drm_exynos_ipp_ctrl ctrl, enum drm_exynos_ipp_state state)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	switch (ctrl) {
	case IPP_CTRL_PLAY:
		if (state != IPP_STATE_IDLE)
			goto err_status;
		break;
	case IPP_CTRL_STOP:
		if (state == IPP_STATE_STOP)
			goto err_status;
		break;
	case IPP_CTRL_PAUSE:
		if (state != IPP_STATE_START)
			goto err_status;
		break;
	case IPP_CTRL_RESUME:
		if (state != IPP_STATE_STOP)
			goto err_status;
		break;
	default:
		DRM_ERROR("invalid state.\n");
		goto err_status;
		break;
	}

	return true;

err_status:
	DRM_ERROR("invalid status:ctrl[%d]state[%d]\n", ctrl, state);
	return false;
}

int exynos_drm_ipp_cmd_ctrl(struct drm_device *drm_dev, void *data,
		struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct exynos_drm_ippdrv *ippdrv = NULL;
	struct device *dev = priv->dev;
	struct ipp_context *ctx = get_ipp_context(dev);
	struct drm_exynos_ipp_cmd_ctrl *cmd_ctrl = data;
	struct drm_exynos_ipp_cmd_work *cmd_work;
	struct drm_exynos_ipp_cmd_node *c_node;
	struct drm_exynos_ipp_property	*property;
	int ret = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!ctx) {
		DRM_ERROR("invalid context.\n");
		return -EINVAL;
	}

	if (!cmd_ctrl) {
		DRM_ERROR("invalid control parameter.\n");
		return -EINVAL;
	}

	DRM_INFO("%s:ctrl[%d]prop_id[%d]\n", __func__,
		cmd_ctrl->ctrl, cmd_ctrl->prop_id);

	ippdrv = ipp_find_drv_by_handle(cmd_ctrl->prop_id);
	if (IS_ERR(ippdrv)) {
		DRM_ERROR("failed to get ipp driver.\n");
		return PTR_ERR(ippdrv);
	}

	c_node = ipp_find_obj(&ctx->prop_idr, &ctx->prop_lock,
		cmd_ctrl->prop_id);
	if (IS_ERR_OR_NULL(c_node)) {
		DRM_ERROR("failed to get cmd node prop_id[%d]\n",
			cmd_ctrl->prop_id);
		return -EINVAL;
	}

	property = &c_node->property;
	if (!exynos_drm_ipp_check_valid(ippdrv->dev, cmd_ctrl->ctrl,
	    c_node->state)) {
		DRM_ERROR("invalid state.\n");
		return -EINVAL;
	}

	switch (cmd_ctrl->ctrl) {
	case IPP_CTRL_PLAY:
		c_node->state = IPP_STATE_START;

		if (!property->protect)
			exynos_drm_ipp_ctrl_iommu(ippdrv, true);

		if (property->type & IPP_EVENT_DRIVEN) {
			cmd_work = c_node->start_work;
			cmd_work->ctrl = cmd_ctrl->ctrl;
			ret = ipp_handle_cmd_work(dev, ippdrv, cmd_work, c_node);
			if (ret)
				DRM_INFO("%s:failed to cmd_work.\n", __func__);
		} else {
			if ( ipp_start_property(ippdrv, c_node))
				DRM_INFO("%s:failed to start property:prop_id[%d]\n",
					__func__, property->prop_id);
		}
		break;
	case IPP_CTRL_STOP:
		c_node->state = IPP_STATE_STOP;

		if (property->type & IPP_EVENT_DRIVEN) {
			cmd_work = c_node->stop_work;
			cmd_work->ctrl = cmd_ctrl->ctrl;

			ret = ipp_handle_cmd_work(dev, ippdrv, cmd_work, c_node);
			if (ret)
				DRM_ERROR("failed to handle_cmd_work.\n");

			if (!wait_for_completion_timeout(&c_node->stop_complete,
			    msecs_to_jiffies(300))) {
				DRM_ERROR("timeout stop:prop_id[%d]\n",
					property->prop_id);
			}
		} else {
			ret = ipp_stop_property(ippdrv->drm_dev, ippdrv,
				c_node);
			if (ret) {
				DRM_ERROR("failed to stop property.\n");
				goto err;
			}
		}

		ipp_clean_cmd_node(ctx, ippdrv, c_node);

#ifdef CONFIG_DRM_EXYNOS_IPP_TC
		ipp_traffic_control(ctx);
#endif
		break;
	case IPP_CTRL_PAUSE:
		c_node->state = IPP_STATE_STOP;
		if (property->type & IPP_EVENT_DRIVEN) {
			cmd_work = c_node->stop_work;
			cmd_work->ctrl = cmd_ctrl->ctrl;
			ret = ipp_handle_cmd_work(dev, ippdrv, cmd_work, c_node);
			if (ret)
				DRM_INFO("%s:failed to cmd_work.\n", __func__);

			if (!wait_for_completion_timeout(&c_node->stop_complete,
			    msecs_to_jiffies(200))) {
				DRM_ERROR("timeout stop:prop_id[%d]\n",
					property->prop_id);
			}
		} else {
			ret = ipp_stop_property(ippdrv->drm_dev, ippdrv,
				c_node);
			if (ret) {
				DRM_ERROR("failed to stop property.\n");
				goto err;
			}
		}
		break;
	case IPP_CTRL_RESUME:
		c_node->state = IPP_STATE_START;

		if (property->type & IPP_EVENT_DRIVEN) {
			cmd_work = c_node->start_work;
			cmd_work->ctrl = cmd_ctrl->ctrl;
			ret = ipp_handle_cmd_work(dev, ippdrv, cmd_work, c_node);
			if (ret)
				DRM_INFO("%s:failed to cmd_work.\n", __func__);
		}  else {
			if ( ipp_start_property(ippdrv, c_node))
				DRM_INFO("%s:failed to start property:prop_id[%d]\n",
					__func__, property->prop_id);
		}
		break;
	default:
		DRM_ERROR("could not support this state currently.\n");
		return -EINVAL;
	}

	DRM_INFO("%s:done ctrl[%d]prop_id[%d]\n", __func__,
		cmd_ctrl->ctrl, cmd_ctrl->prop_id);

err:
	return ret;
}

int exynos_drm_ippnb_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(
		&exynos_drm_ippnb_list, nb);
}

int exynos_drm_ippnb_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(
		&exynos_drm_ippnb_list, nb);
}

int exynos_drm_ippnb_send_event(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(
		&exynos_drm_ippnb_list, val, v);
}

static int ipp_set_property(struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_property *property)
{
	struct exynos_drm_ipp_ops *ops = NULL;
	bool swap = false;
	int ret, i;

	if (!property) {
		DRM_ERROR("invalid property parameter.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("%s:prop_id[%d]\n", __func__, property->prop_id);

	if (ipp_runtime_suspended(ippdrv->dev)) {
		if (ipp_runtime_get_sync(ippdrv)) {
			DRM_ERROR("failed to get_sync.\n");
			return -EINVAL;
		}
	}

	/* reset h/w block */
	if (ippdrv->reset &&
	    ippdrv->reset(ippdrv->dev)) {
		DRM_ERROR("failed to reset.\n");
		return -EINVAL;
	}

	/* set source,destination operations */
	for_each_ipp_ops(i) {
		struct drm_exynos_ipp_config *config =
			&property->config[i];

		ops = ippdrv->ops[i];
		if (!ops || !config) {
			DRM_ERROR("not support ops and config.\n");
			return -EINVAL;
		}

		/* set format */
		if (ops->set_fmt) {
			ret = ops->set_fmt(ippdrv->dev, config->fmt);
			if (ret) {
				DRM_ERROR("not support format.\n");
				return ret;
			}
		}

		/* set transform for rotation, flip */
		if (ops->set_transf) {
			ret = ops->set_transf(ippdrv->dev, config->degree,
				config->flip, &swap);
			if (ret) {
				DRM_ERROR("not support tranf.\n");
				return -EINVAL;
			}
		}

		/* set size */
		if (ops->set_size) {
			ret = ops->set_size(ippdrv->dev, swap, &config->pos,
				&config->sz);
			if (ret) {
				DRM_ERROR("not support size.\n");
				return ret;
			}
		}
	}

	return 0;
}

static int ipp_start_property(struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node)
{
	struct drm_exynos_ipp_mem_node *m_node;
	struct drm_exynos_ipp_property *property = &c_node->property;
	struct list_head *head;
	int ret, i;

	DRM_DEBUG_KMS("%s:prop_id[%d]\n", __func__, property->prop_id);

	/* store command info in ippdrv */
	ippdrv->c_node = c_node;

	mutex_lock(&c_node->mem_lock);
	if (!ipp_check_mem_list(c_node)) {
		DRM_INFO("%s:empty memory.\n", __func__);
		ret = -ENOMEM;
		goto err_unlock;
	}

	/* set current property in ippdrv */
	ret = ipp_set_property(ippdrv, property);
	if (ret) {
		DRM_ERROR("failed to set property.\n");
		goto err_unlock;
	}

	/* check command */
	switch (property->cmd) {
	case IPP_CMD_M2M:
		for_each_ipp_ops(i) {
			/* source/destination memory list */
			head = &c_node->mem_list[i];

			m_node = list_first_entry(head,
				struct drm_exynos_ipp_mem_node, list);
			if (!m_node) {
				DRM_ERROR("failed to get node.\n");
				ret = -EFAULT;
				goto err_unlock;
			}

			DRM_DEBUG_KMS("%s:m_node[0x%x]\n",
				__func__, (int)m_node);

			ret = ipp_set_mem_node(ippdrv, c_node, m_node);
			if (ret) {
				DRM_ERROR("failed to set m node.\n");
				goto err_unlock;
			}
		}
		break;
	case IPP_CMD_WB:
		/* destination memory list */
		head = &c_node->mem_list[EXYNOS_DRM_OPS_DST];

		list_for_each_entry(m_node, head, list) {
			ret = ipp_set_mem_node(ippdrv, c_node, m_node);
			if (ret) {
				DRM_ERROR("failed to set m node.\n");
				goto err_unlock;
			}
		}
		break;
	case IPP_CMD_OUTPUT:
		/* source memory list */
		head = &c_node->mem_list[EXYNOS_DRM_OPS_SRC];

		m_node = list_first_entry(head,
			struct drm_exynos_ipp_mem_node, list);
		if (!m_node) {
			DRM_ERROR("failed to get node.\n");
			goto err_unlock;
		}

		DRM_DEBUG_KMS("%s:m_node[0x%x]\n",
			__func__, (int)m_node);

		ret = ipp_set_mem_node(ippdrv, c_node, m_node);
		if (ret) {
			DRM_ERROR("failed to set m node.\n");
			goto err_unlock;
		}
		break;
	default:
		DRM_ERROR("invalid operations.\n");
		ret = -EINVAL;
		goto err_unlock;
	}
	mutex_unlock(&c_node->mem_lock);

	DRM_DEBUG_KMS("%s:cmd[%d]\n", __func__, property->cmd);

	/* start operations */
	if (ippdrv->start) {
		ret = ippdrv->start(ippdrv->dev, property->cmd);
		if (ret) {
			DRM_ERROR("failed to start ops.\n");
			ippdrv->c_node = NULL;
			return ret;
		}
	}

	/*
	 * M2M case supports wait_completion of transfer.
	 * because M2M case supports single unit operation
	 * with multiple queue.
	 * M2M case needs to wait 700ms for completion.
	 * During dpms controling, struct_mutex is preempted
	 * by exynos_drm_encoder_dpms().
	 * Theroefore, exynos_drm_gem_put_dma_addr() takes
	 * over 700ms while dpms control.
	 */
	if (ipp_is_m2m_cmd(property->cmd)) {
		if (!wait_for_completion_timeout
		    (&c_node->start_complete, msecs_to_jiffies(500)))  {
			DRM_ERROR("timeout event:prop_id[%d]\n",
				c_node->property.prop_id);
			ret = -ETIMEDOUT;
		}
	}

	return ret;

err_unlock:
	mutex_unlock(&c_node->mem_lock);
	ippdrv->c_node = NULL;
	return ret;
}

static int ipp_stop_property(struct drm_device *drm_dev,
		struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node)
{
	struct drm_exynos_ipp_mem_node *m_node, *tm_node;
	struct drm_exynos_ipp_property *property = &c_node->property;
	struct list_head *head;
	int ret = 0, i;
	int cnt = 0, j, hdl = 0;

	DRM_INFO("%s:prop_id[%d]\n", __func__, property->prop_id);

	/* put event */
	ipp_put_event(c_node, NULL);

	mutex_lock(&c_node->mem_lock);
	/* check command */
	switch (property->cmd) {
	case IPP_CMD_M2M:
		for_each_ipp_ops(i) {
			/* source/destination memory list */
			head = &c_node->mem_list[i];

			if (list_empty(head)) {
				DRM_DEBUG_KMS("%s:mem_list is empty.\n",
					__func__);
				continue;
			}

			list_for_each_entry_safe(m_node, tm_node,
				head, list) {
				for_each_ipp_planar(j) {
					hdl = m_node->buf_info.handles[j];
					if (hdl)
						DRM_INFO(
							"%s: put cnt[%d][%s]hdl[%d]\n",
							__func__, ++cnt,
							i ? "dst" : "src", hdl);
				}

				if (cnt > IPP_MAX_MEM) {
					dump_stack();
					panic("%s\n", __func__);
				}

				ret = ipp_put_mem_node(drm_dev, c_node,
					m_node);
				if (ret) {
					DRM_ERROR("failed to put m_node.\n");
					goto err_clear;
				}
			}
		}
		break;
	case IPP_CMD_WB:
		/* destination memory list */
		head = &c_node->mem_list[EXYNOS_DRM_OPS_DST];

		if (list_empty(head)) {
			DRM_DEBUG_KMS("%s:mem_list is empty.\n", __func__);
			break;
		}

		list_for_each_entry_safe(m_node, tm_node, head, list) {
			for_each_ipp_planar(j) {
				hdl = m_node->buf_info.handles[j];
				if (hdl)
					DRM_INFO("%s: put cnt[%d][%s]hdl[%d]\n",
						__func__, ++cnt, "dst", hdl);
			}

			if (cnt > IPP_MAX_MEM) {
				dump_stack();
				panic("%s\n", __func__);
			}

			ret = ipp_put_mem_node(drm_dev, c_node, m_node);
			if (ret) {
				DRM_ERROR("failed to put m_node.\n");
				goto err_clear;
			}
		}
		break;
	case IPP_CMD_OUTPUT:
		/* source memory list */
		head = &c_node->mem_list[EXYNOS_DRM_OPS_SRC];

		if (list_empty(head)) {
			DRM_DEBUG_KMS("%s:mem_list is empty.\n", __func__);
			break;
		}

		list_for_each_entry_safe(m_node, tm_node, head, list) {
			for_each_ipp_planar(j) {
				hdl = m_node->buf_info.handles[j];
				if (hdl)
					DRM_INFO("%s: put cnt[%d][%s]hdl[%d]\n",
						__func__, ++cnt, "src", hdl);
			}

			if (cnt > IPP_MAX_MEM) {
				dump_stack();
				panic("%s\n", __func__);
			}

			ret = ipp_put_mem_node(drm_dev, c_node, m_node);
			if (ret) {
				DRM_ERROR("failed to put m_node.\n");
				goto err_clear;
			}
		}
		break;
	default:
		DRM_ERROR("invalid operations.\n");
		ret = -EINVAL;
		goto err_clear;
	}

err_clear:
	mutex_unlock(&c_node->mem_lock);

	if (ipp_runtime_suspended(ippdrv->dev)) {
		ippdrv->c_node = c_node;

		if (ipp_runtime_get_sync(ippdrv)) {
			DRM_ERROR("failed to get_sync.\n");
			return -EINVAL;
		}
	}

	/* stop operations */
	if (ippdrv->stop)
		ippdrv->stop(ippdrv->dev, property->cmd);

	if (!ipp_runtime_suspended(ippdrv->dev))
		ipp_runtime_put_sync(ippdrv);

	return ret;
}

void ipp_sched_cmd(struct work_struct *work)
{
	struct drm_exynos_ipp_cmd_work *cmd_work =
		(struct drm_exynos_ipp_cmd_work *)work;
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node;
	struct drm_exynos_ipp_mem_node *m_node;
	struct drm_exynos_ipp_property *property;
	struct list_head *head;
	u32 tbuf_id[EXYNOS_DRM_OPS_MAX] = {0, };
	int ret, i;

	DRM_DEBUG_KMS("%s\n", __func__);

	ippdrv = cmd_work->ippdrv;
	if (!ippdrv) {
		DRM_ERROR("invalid ippdrv list.\n");
		return;
	}

	mutex_lock(&ippdrv->drv_lock);

	c_node = cmd_work->c_node;
	if (!c_node) {
		DRM_ERROR("invalid command node list.\n");
		goto err_unlock;
	}

	mutex_lock(&c_node->cmd_lock);

	if (completion_done(&c_node->start_complete))
		INIT_COMPLETION(c_node->start_complete);

	property = &c_node->property;

	switch (cmd_work->ctrl) {
	case IPP_CTRL_PLAY:
	case IPP_CTRL_RESUME:
#ifdef IPP_AGING_LOG
		ipp_save_timing(IPP_LOG_WORK);
#endif

		ret = ipp_start_property(ippdrv, c_node);
		if (ret) {
			DRM_INFO("%s:failed to start property:prop_id[%d]\n",
				__func__, c_node->property.prop_id);

			if (ipp_is_m2m_cmd(c_node->property.cmd) &&
				ret == -ETIMEDOUT) {
				if (!ipp_runtime_suspended(ippdrv->dev)) {
					if (ippdrv->stop)
						ippdrv->stop(ippdrv->dev,
							property->cmd);
					ipp_runtime_put_sync(ippdrv);
				}

				for_each_ipp_ops(i) {
					head = &c_node->mem_list[i];

					m_node = list_first_entry(head,
						struct drm_exynos_ipp_mem_node, list);
					if (m_node)
						tbuf_id[i] = m_node->buf_id;
				}

				ipp_send_event(ippdrv, c_node, tbuf_id);
			}
		}

		if (ipp_is_m2m_cmd(property->cmd)) {
			mutex_lock(&c_node->mem_lock);
			if (ipp_check_mem_list(c_node)) {
				struct drm_exynos_ipp_cmd_work *cmd_work =
					c_node->start_work;

				cmd_work->ctrl = IPP_CTRL_PLAY;
				ret = ipp_handle_cmd_work(ippdrv->parent_dev,
					ippdrv, cmd_work, c_node);
				if (ret)
					DRM_INFO(
					"%s:failed to cmd_work.\n", __func__);

			}
			mutex_unlock(&c_node->mem_lock);
		}
		break;
	case IPP_CTRL_STOP:
	case IPP_CTRL_PAUSE:
		ret = ipp_stop_property(ippdrv->drm_dev, ippdrv,
			c_node);
		if (ret) {
			DRM_ERROR("failed to stop property.\n");
			goto err_clear;
		}

		complete(&c_node->stop_complete);
		break;
	default:
		DRM_ERROR("unknown control type\n");
		break;
	}

	DRM_DEBUG_KMS("%s:ctrl[%d] done.\n", __func__, cmd_work->ctrl);

err_clear:
	mutex_unlock(&c_node->cmd_lock);
err_unlock:
	mutex_unlock(&ippdrv->drv_lock);
}

static int ipp_send_event(struct exynos_drm_ippdrv *ippdrv,
		struct drm_exynos_ipp_cmd_node *c_node, int *buf_id)
{
	struct drm_device *drm_dev = ippdrv->drm_dev;
	struct drm_exynos_ipp_property *property = &c_node->property;
	struct drm_exynos_ipp_mem_node *m_node;
	struct drm_exynos_ipp_queue_buf qbuf;
	struct drm_exynos_ipp_send_event *e;
	struct list_head *head;
	struct timeval now;
	unsigned long flags;
	u32 tbuf_id[EXYNOS_DRM_OPS_MAX] = {0, };
	int ret, i;

	for_each_ipp_ops(i)
		DRM_DEBUG_KMS("%s:%s buf_id[%d]\n", __func__,
			i ? "dst" : "src", buf_id[i]);

	if (!drm_dev) {
		DRM_ERROR("failed to get drm_dev.\n");
		return -EINVAL;
	}

	if (!property) {
		DRM_ERROR("failed to get property.\n");
		return -EINVAL;
	}

	mutex_lock(&c_node->event_lock);
	if (list_empty(&c_node->event_list)) {
		DRM_INFO("%s:event list is empty.\n", __func__);
		ret = 0;
		goto err_event_unlock;
	}

	mutex_lock(&c_node->mem_lock);
	if (!ipp_check_mem_list(c_node)) {
		DRM_INFO("%s:empty memory.\n", __func__);
		ret = 0;
		goto err_mem_unlock;
	}

	/* check command */
	switch (property->cmd) {
	case IPP_CMD_M2M:
		for_each_ipp_ops(i) {
			/* source/destination memory list */
			head = &c_node->mem_list[i];

			m_node = list_first_entry(head,
				struct drm_exynos_ipp_mem_node, list);
			if (!m_node) {
				DRM_ERROR("empty memory node.\n");
				ret = -ENOMEM;
				goto err_mem_unlock;
			}

			tbuf_id[i] = m_node->buf_id;
			DRM_DEBUG_KMS("%s:%s buf_id[%d]\n", __func__,
				i ? "dst" : "src", tbuf_id[i]);

			ret = ipp_put_mem_node(drm_dev, c_node, m_node);
			if (ret)
				DRM_ERROR("failed to put m_node.\n");
		}
		break;
	case IPP_CMD_WB:
		/* clear buf for finding */
		memset(&qbuf, 0x0, sizeof(qbuf));
		qbuf.ops_id = EXYNOS_DRM_OPS_DST;
		qbuf.buf_id = buf_id[EXYNOS_DRM_OPS_DST];

		/* get memory node entry */
		m_node = ipp_find_mem_node(c_node, &qbuf);
		if (!m_node) {
			DRM_ERROR("empty memory node.\n");
			ret = -ENOMEM;
			goto err_mem_unlock;
		}

		tbuf_id[EXYNOS_DRM_OPS_DST] = m_node->buf_id;

		ret = ipp_put_mem_node(drm_dev, c_node, m_node);
		if (ret)
			DRM_ERROR("failed to put m_node.\n");
		break;
	case IPP_CMD_OUTPUT:
		/* source memory list */
		head = &c_node->mem_list[EXYNOS_DRM_OPS_SRC];

		m_node = list_first_entry(head,
			struct drm_exynos_ipp_mem_node, list);
		if (!m_node) {
			DRM_ERROR("empty memory node.\n");
			ret = -ENOMEM;
			goto err_mem_unlock;
		}

		tbuf_id[EXYNOS_DRM_OPS_SRC] = m_node->buf_id;

		ret = ipp_put_mem_node(drm_dev, c_node, m_node);
		if (ret)
			DRM_ERROR("failed to put m_node.\n");
		break;
	default:
		DRM_ERROR("invalid operations.\n");
		ret = -EINVAL;
		goto err_mem_unlock;
	}
	mutex_unlock(&c_node->mem_lock);

	if (tbuf_id[EXYNOS_DRM_OPS_DST] != buf_id[EXYNOS_DRM_OPS_DST])
		DRM_ERROR("failed to match buf_id[%d %d]prop_id[%d]\n",
			tbuf_id[1], buf_id[1], property->prop_id);

	/*
	 * command node have event list of destination buffer
	 * If destination buffer enqueue to mem list,
	 * then we make event and link to event list tail.
	 * so, we get first event for first enqueued buffer.
	 */
	e = list_first_entry(&c_node->event_list,
		struct drm_exynos_ipp_send_event, base.link);

	do_gettimeofday(&now);
	DRM_DEBUG_KMS("%s:tv_sec[%ld]tv_usec[%ld]\n"
		, __func__, now.tv_sec, now.tv_usec);
	e->event.tv_sec = now.tv_sec;
	e->event.tv_usec = now.tv_usec;
	e->event.prop_id = property->prop_id;

	/* set buffer id about source destination */
	for_each_ipp_ops(i)
		e->event.buf_id[i] = tbuf_id[i];

	spin_lock_irqsave(&drm_dev->event_lock, flags);
	list_move_tail(&e->base.link, &e->base.file_priv->event_list);
	wake_up_interruptible(&e->base.file_priv->event_wait);
	spin_unlock_irqrestore(&drm_dev->event_lock, flags);
	mutex_unlock(&c_node->event_lock);

	DRM_DEBUG_KMS("%s:done cmd[%d]prop_id[%d]buf_id[%d]\n", __func__,
		property->cmd, property->prop_id, tbuf_id[EXYNOS_DRM_OPS_DST]);

#ifdef IPP_AGING_LOG
	ipp_save_timing(IPP_LOG_EVENT);
#endif

	return 0;

err_mem_unlock:
	mutex_unlock(&c_node->mem_lock);
err_event_unlock:
	mutex_unlock(&c_node->event_lock);
	return ret;
}

void ipp_sched_event(struct drm_exynos_ipp_event_info *ipp_event)
{
	struct drm_exynos_ipp_event_info *event =
		(struct drm_exynos_ipp_event_info *)ipp_event;
	struct exynos_drm_ippdrv *ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node;
	int ret;

	if (!event) {
		DRM_ERROR("failed to get event.\n");
		return;
	}

	DRM_DEBUG_KMS("%s:buf_id[%d]\n", __func__,
		event->buf_id[EXYNOS_DRM_OPS_DST]);

	ippdrv = event->ippdrv;
	if (!ippdrv) {
		DRM_ERROR("failed to get ipp driver.\n");
		return;
	}

	c_node = ippdrv->c_node;
	if (!c_node) {
		DRM_ERROR("failed to get command node.\n");
		return;
	}

	/*
	 * IPP supports command thread, event thread synchronization.
	 * If IPP close immediately from user land, then IPP make
	 * synchronization with command thread, so make complete event.
	 * or going out operations.
	 */
	if (c_node->state != IPP_STATE_START) {
		DRM_INFO("%s:bypass state[%d]prop_id[%d]\n",
			__func__, c_node->state, c_node->property.prop_id);
		goto err_completion;
	}

	if (!ipp_is_wb_cmd(c_node->property.cmd) &&
		!ipp_runtime_suspended(ippdrv->dev))
		ipp_runtime_put_sync(ippdrv);

	ret = ipp_send_event(ippdrv, c_node, event->buf_id);
	if (ret) {
		DRM_ERROR("failed to send event.\n");
		goto err_completion;
	}

err_completion:
	if (ipp_is_m2m_cmd(c_node->property.cmd))
		complete(&c_node->start_complete);
}

static int ipp_subdrv_probe(struct drm_device *drm_dev, struct device *dev)
{
	struct ipp_context *ctx = get_ipp_context(dev);
	struct exynos_drm_ippdrv *ippdrv;
	int ret;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* get ipp driver entry */
	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, drv_list) {
		ippdrv->drm_dev = drm_dev;

		ret = ipp_create_id(&ctx->ipp_idr, &ctx->ipp_lock, ippdrv,
			&ippdrv->ipp_id);
		if (ret) {
			DRM_ERROR("failed to create id.\n");
			goto err_idr;
		}

		if (ippdrv->ipp_id == 0) {
			DRM_ERROR("failed to get ipp_id[%d]\n",
				ippdrv->ipp_id);
			goto err_idr;
		}

		/* store parent device for node */
		ippdrv->parent_dev = dev;

		/* store event handler */
		ippdrv->sched_event = ipp_sched_event;
		INIT_LIST_HEAD(&ippdrv->cmd_list);
		mutex_init(&ippdrv->drv_lock);
		ctx->num_drv++;
		DRM_DEBUG_KMS("%s:count[%d]ippdrv[0x%x]ipp_id[%d]\n", __func__,
			ctx->num_drv, (int)ippdrv, ippdrv->ipp_id);
	}

	return 0;

err_idr:
	idr_remove_all(&ctx->ipp_idr);
	idr_remove_all(&ctx->prop_idr);
	idr_destroy(&ctx->ipp_idr);
	idr_destroy(&ctx->prop_idr);
	return ret;
}

static void ipp_subdrv_remove(struct drm_device *drm_dev, struct device *dev)
{
	struct ipp_context *ctx = get_ipp_context(dev);
	struct exynos_drm_ippdrv *ippdrv;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* get ipp driver entry */
	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, drv_list) {
		ipp_remove_id(&ctx->ipp_idr, &ctx->ipp_lock, ippdrv->ipp_id);
		mutex_destroy(&ippdrv->drv_lock);
		ippdrv->drm_dev = NULL;
		exynos_drm_ippdrv_unregister(ippdrv);
		ctx->num_drv--;
	}
}

static int ipp_subdrv_open(struct drm_device *drm_dev, struct device *dev,
		struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv;

	DRM_DEBUG_KMS("%s\n", __func__);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		DRM_ERROR("failed to allocate priv.\n");
		return -ENOMEM;
	}
	priv->dev = dev;
	file_priv->ipp_priv = priv;

	INIT_LIST_HEAD(&priv->event_list);

	DRM_DEBUG_KMS("%s:done priv[0x%x]\n", __func__, (int)priv);

	return 0;
}

static void ipp_subdrv_close(struct drm_device *drm_dev, struct device *dev,
		struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_ipp_private *priv = file_priv->ipp_priv;
	struct ipp_context *ctx = get_ipp_context(dev);
	struct exynos_drm_ippdrv *ippdrv = NULL;
	struct drm_exynos_ipp_cmd_node *c_node, *tc_node;
	struct drm_exynos_ipp_cmd_work *cmd_work;
	int count = 0, ret;

	DRM_DEBUG_KMS("%s:for priv[0x%x]\n", __func__, (int)priv);

	if (list_empty(&exynos_drm_ippdrv_list)) {
		DRM_DEBUG_KMS("%s:ippdrv_list is empty.\n", __func__);
		goto err_clear;
	}

	list_for_each_entry(ippdrv, &exynos_drm_ippdrv_list, drv_list) {
		if (list_empty(&ippdrv->cmd_list))
			continue;

		list_for_each_entry_safe(c_node, tc_node,
			&ippdrv->cmd_list, list) {
			DRM_INFO("%s:count[%d]ippdrv[0x%x]\n",
				__func__, count++, (int)ippdrv);

			if (c_node->priv == priv) {
				/*
				 * userland goto unnormal state. process killed.
				 * and close the file.
				 * so, IPP didn't called stop cmd ctrl.
				 * so, we are make stop operation in this state.
				 */
				if (c_node->state == IPP_STATE_START) {
					DRM_INFO("%s:stop current cmd.\n",
						__func__);
					c_node->state = IPP_STATE_STOP;

					if (c_node->property.type & IPP_EVENT_DRIVEN) {
						cmd_work = c_node->stop_work;
						cmd_work->ctrl = IPP_CTRL_STOP;

						ret = ipp_handle_cmd_work(dev, ippdrv,
							cmd_work, c_node);
						if (ret)
							DRM_ERROR(
							"failed to cmd_work.\n");

						if (!wait_for_completion_timeout(
							&c_node->stop_complete,
							msecs_to_jiffies(300))) {
							DRM_ERROR("timeout stop\n");
						}
					} else {
						ret = ipp_stop_property(ippdrv->drm_dev, ippdrv,
							c_node);
						if (ret)
							DRM_ERROR("failed to stop property.\n");

					}
				}

				ipp_clean_cmd_node(ctx, ippdrv, c_node);
			}
		}
	}

err_clear:
	kfree(priv);
	return;
}

static int ipp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ipp_context *ctx;
	struct exynos_drm_subdrv *subdrv;
	int ret;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	DRM_DEBUG_KMS("%s\n", __func__);

	mutex_init(&ctx->ipp_lock);
	mutex_init(&ctx->prop_lock);

	idr_init(&ctx->ipp_idr);
	idr_init(&ctx->prop_idr);

	/* set sub driver informations */
	subdrv = &ctx->subdrv;
	subdrv->dev = dev;
	subdrv->probe = ipp_subdrv_probe;
	subdrv->remove = ipp_subdrv_remove;
	subdrv->open = ipp_subdrv_open;
	subdrv->close = ipp_subdrv_close;

	platform_set_drvdata(pdev, ctx);

	ret = exynos_drm_subdrv_register(subdrv);
	if (ret < 0) {
		DRM_ERROR("failed to register drm ipp device.\n");
		return ret;
	}

	dev_info(&pdev->dev, "drm ipp registered successfully.\n");

	return 0;
}

static int ipp_remove(struct platform_device *pdev)
{
	struct ipp_context *ctx = platform_get_drvdata(pdev);

	DRM_DEBUG_KMS("%s\n", __func__);

	/* unregister sub driver */
	exynos_drm_subdrv_unregister(&ctx->subdrv);

	/* remove,destroy ipp idr */
	idr_remove_all(&ctx->ipp_idr);
	idr_remove_all(&ctx->prop_idr);
	idr_destroy(&ctx->ipp_idr);
	idr_destroy(&ctx->prop_idr);

	mutex_destroy(&ctx->ipp_lock);
	mutex_destroy(&ctx->prop_lock);

	return 0;
}

static int ipp_power_ctrl(struct ipp_context *ctx, bool enable)
{
	DRM_INFO("%s:enable[%d]\n", __func__, enable);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
bool ipp_runtime_suspended(struct device *dev)
{
	return pm_runtime_suspended(dev);
}

int ipp_runtime_suspend(struct device *dev)
{
	struct ipp_context *ctx = get_ipp_context(dev);

	DRM_INFO("%s\n", __func__);

	return ipp_power_ctrl(ctx, false);
}

int ipp_runtime_resume(struct device *dev)
{
	struct ipp_context *ctx = get_ipp_context(dev);

	DRM_INFO("%s\n", __func__);

	return ipp_power_ctrl(ctx, true);
}

int ipp_runtime_get_sync(struct exynos_drm_ippdrv *ippdrv)
{
	int ret = 0;

	if (!ipp_runtime_suspended(ippdrv->dev)) {
		DRM_INFO("%s:same state\n", __func__);
		goto err;
	}

	DRM_DEBUG_KMS("%s\n", __func__);

	ret = pm_runtime_get_sync(ippdrv->dev);
	if (ret < 0) {
		DRM_ERROR("resume:ret[%d]\n", ret);
		goto err;
	}

	if (ippdrv->pm_ctrl) {
		ret = ippdrv->pm_ctrl(ippdrv->dev, true);
		if (ret)
			goto err;
	}

#ifdef IPP_AGING_LOG
	ipp_save_timing(IPP_LOG_GET_SYNC);
#endif

	return 0;

err:
	DRM_ERROR("suspended[%d]\n",
		ipp_runtime_suspended(ippdrv->dev));

	return ret;
}

int ipp_runtime_put_sync(struct exynos_drm_ippdrv *ippdrv)
{
	int ret = 0;

	if (ipp_runtime_suspended(ippdrv->dev)) {
		DRM_INFO("%s:same state\n", __func__);
		goto err;
	}

	DRM_DEBUG_KMS("%s\n", __func__);

	if (ippdrv->pm_ctrl) {
		ret = ippdrv->pm_ctrl(ippdrv->dev, false);
		if (ret)
			goto err;
	}

	ret = pm_runtime_put_sync(ippdrv->dev);
	if (ret < 0) {
		DRM_ERROR("suspend:ret[%d]\n", ret);
		goto err;
	}

#ifdef IPP_AGING_LOG
	ipp_save_timing(IPP_LOG_PUT_SYNC);
#endif

	return 0;

err:
	DRM_ERROR("suspended[%d]\n",
		ipp_runtime_suspended(ippdrv->dev));

	return ret;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int ipp_suspend(struct device *dev)
{
	struct ipp_context *ctx = get_ipp_context(dev);

	DRM_INFO("%s\n", __func__);

	if (ipp_runtime_suspended(dev))
		return 0;

	return ipp_power_ctrl(ctx, false);
}

static int ipp_resume(struct device *dev)
{
	struct ipp_context *ctx = get_ipp_context(dev);

	DRM_INFO("%s\n", __func__);

	if (!ipp_runtime_suspended(dev))
		return ipp_power_ctrl(ctx, true);

	return 0;
}
#endif

static const struct dev_pm_ops ipp_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ipp_suspend, ipp_resume)
	SET_RUNTIME_PM_OPS(ipp_runtime_suspend, ipp_runtime_resume, NULL)
};

struct platform_driver ipp_driver = {
	.probe		= ipp_probe,
	.remove		= ipp_remove,
	.driver		= {
		.name	= "exynos-drm-ipp",
		.owner	= THIS_MODULE,
		.pm	= &ipp_pm_ops,
	},
};

