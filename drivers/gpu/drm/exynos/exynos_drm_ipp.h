/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

 #include "exynos_drm_drv.h"

#ifndef _EXYNOS_DRM_IPP_H_
#define _EXYNOS_DRM_IPP_H_

#define for_each_ipp_ops(pos)	\
	for (pos = 0; pos < EXYNOS_DRM_OPS_MAX; pos++)
#define for_each_ipp_planar(pos)	\
	for (pos = 0; pos < EXYNOS_DRM_PLANAR_MAX; pos++)

#define ipp_is_m2m_cmd(c)	(c == IPP_CMD_M2M)
#define ipp_is_wb_cmd(c)	(c == IPP_CMD_WB)
#define ipp_is_output_cmd(c)	(c == IPP_CMD_OUTPUT)

/* definition of state */
enum drm_exynos_ipp_state {
	IPP_STATE_IDLE,
	IPP_STATE_START,
	IPP_STATE_STOP,
};

/* definition of extend function */
enum drm_exynos_ipp_ext {
	IPP_GET_LCD_WIDTH,
	IPP_GET_LCD_HEIGHT,
	IPP_SET_WRITEBACK,
	IPP_SET_OUTPUT,
};

/*
 * A structure of command work information.
 * @work: work structure.
 * @ippdrv: current work ippdrv.
 * @c_node: command node information.
 * @ctrl: command control.
 */
struct drm_exynos_ipp_cmd_work {
	struct work_struct	work;
	struct exynos_drm_ippdrv	*ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node;
	enum drm_exynos_ipp_ctrl	ctrl;
};

/*
 * A structure of command node.
 *
 * @priv: IPP private infomation.
 * @list: list head to command queue information.
 * @event_list: list head of event.
 * @mem_list: list head to source,destination memory queue information.
 * @cmd_lock: lock for synchronization of access to ioctl.
 * @mem_lock: lock for synchronization of access to memory nodes.
 * @event_lock: lock for synchronization of access to scheduled event.
 * @start_complete: completion of start of command.
 * @stop_complete: completion of stop of command.
 * @property: property information.
 * @cmd_workq: command work queue.
 * @start_work: start command work structure.
 * @stop_work: stop command work structure.
 * @event: event information structure.
 * @state: state of command node.
 */
struct drm_exynos_ipp_cmd_node {
	struct exynos_drm_ipp_private *priv;
	struct list_head	list;
	struct list_head	event_list;
	struct list_head	mem_list[EXYNOS_DRM_OPS_MAX];
	struct mutex	cmd_lock;
	struct mutex	mem_lock;
	struct mutex	event_lock;
	struct completion	start_complete;
	struct completion	stop_complete;
	struct drm_exynos_ipp_property	property;
	struct workqueue_struct	*cmd_workq;
	struct drm_exynos_ipp_cmd_work *start_work;
	struct drm_exynos_ipp_cmd_work *stop_work;
	struct drm_exynos_ipp_event_info *event;
	enum drm_exynos_ipp_state	state;
};

/*
 * A structure of buffer information.
 *
 * @handles: Y, Cb, Cr each gem object.
 * @base: Y, Cb, Cr each planar address.
 * @size: Y, Cb, Cr each planar size.
 */
struct drm_exynos_ipp_buf_info {
	unsigned long	handles[EXYNOS_DRM_PLANAR_MAX];
	dma_addr_t	base[EXYNOS_DRM_PLANAR_MAX];
	uint64_t	size[EXYNOS_DRM_PLANAR_MAX];
};

/*
 * A structure of wb setting infomation.
 *
 * @enable: enable flag for wb.
 * @refresh: HZ of the refresh rate.
 */
struct drm_exynos_ipp_set_wb {
	__u32	enable;
	__u32	refresh;
};

/*
 * A structure of event information.
 *
 * @ippdrv: current work ippdrv.
 * @buf_id: id of src, dst buffer.
 */
struct drm_exynos_ipp_event_info {
	struct exynos_drm_ippdrv *ippdrv;
	u32	buf_id[EXYNOS_DRM_OPS_MAX];
};

/*
 * A structure of source,destination operations.
 *
 * @set_fmt: set format of image.
 * @set_transf: set transform(rotations, flip).
 * @set_size: set size of region.
 * @set_addr: set address for dma.
 */
struct exynos_drm_ipp_ops {
	int (*set_fmt)(struct device *dev, u32 fmt);
	int (*set_transf)(struct device *dev,
		enum drm_exynos_degree degree,
		enum drm_exynos_flip flip, bool *swap);
	int (*set_size)(struct device *dev, int swap,
		struct drm_exynos_pos *pos, struct drm_exynos_sz *sz);
	int (*set_addr)(struct device *dev,
		 struct drm_exynos_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_exynos_ipp_buf_type buf_type);
};

/*
 * A structure of ipp driver.
 *
 * @drv_list: list head for registed sub driver information.
 * @parent_dev: parent device information.
 * @dev: platform device.
 * @drm_dev: drm device.
 * @ipp_id: id of ipp driver.
 * @dedicated: dedicated ipp device.
 * @iommu_on: flag to check whether iommu is enabled.
 * @ops: source, destination operations.
 * @c_node: current command information.
 * @cmd_list: list head for command information.
 * @capability: capability information of current ipp driver.
 * @drv_lock: lock for synchronization of access to start operation.
 * @check_property: check property about format, size, buffer.
 * @reset: reset ipp block.
 * @start: ipp each device start.
 * @stop: ipp each device stop.
 * @pm_ctrl: power contrl of each device.
 * @sched_event: work schedule handler.
 */
struct exynos_drm_ippdrv {
	struct list_head	drv_list;
	struct device	*parent_dev;
	struct device	*dev;
	struct drm_device	*drm_dev;
	u32	ipp_id;
	bool	dedicated;
	bool	iommu_on;
	struct exynos_drm_ipp_ops	*ops[EXYNOS_DRM_OPS_MAX];
	struct drm_exynos_ipp_cmd_node *c_node;
	struct list_head	cmd_list;
	struct drm_exynos_ipp_capability *capability;
	struct mutex	drv_lock;

	int (*check_property)(struct device *dev,
		struct drm_exynos_ipp_property *property);
	int (*reset)(struct device *dev);
	int (*start)(struct device *dev, enum drm_exynos_ipp_cmd cmd);
	void (*stop)(struct device *dev, enum drm_exynos_ipp_cmd cmd);
	int (*pm_ctrl)(struct device *dev, bool enable);
	void (*sched_event)(struct drm_exynos_ipp_event_info *ipp_event);
};

#ifdef CONFIG_DRM_EXYNOS_IPP
extern int exynos_drm_ippdrv_register(struct exynos_drm_ippdrv *ippdrv);
extern int exynos_drm_ippdrv_unregister(struct exynos_drm_ippdrv *ippdrv);
extern int exynos_drm_ipp_get_property(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int exynos_drm_ipp_set_property(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int exynos_drm_ipp_queue_buf(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int exynos_drm_ipp_cmd_ctrl(struct drm_device *drm_dev, void *data,
					 struct drm_file *file);
extern int exynos_drm_ippnb_register(struct notifier_block *nb);
extern int exynos_drm_ippnb_unregister(struct notifier_block *nb);
extern int exynos_drm_ippnb_send_event(unsigned long val, void *v);
extern void ipp_sched_cmd(struct work_struct *work);
extern void ipp_sched_event(struct drm_exynos_ipp_event_info *ipp_event);

#ifdef CONFIG_PM_RUNTIME
extern bool ipp_runtime_suspended(struct device *dev);
extern int ipp_runtime_suspend(struct device *dev);
extern int ipp_runtime_resume(struct device *dev);
extern int ipp_runtime_get_sync(struct exynos_drm_ippdrv *ippdrv);
extern int ipp_runtime_put_sync(struct exynos_drm_ippdrv *ippdrv);
#endif

#else
static inline int exynos_drm_ippdrv_register(struct exynos_drm_ippdrv *ippdrv)
{
	return -ENODEV;
}

static inline int exynos_drm_ippdrv_unregister(struct exynos_drm_ippdrv *ippdrv)
{
	return -ENODEV;
}

static inline int exynos_drm_ipp_get_property(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file_priv)
{
	return -ENOTTY;
}

static inline int exynos_drm_ipp_set_property(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file_priv)
{
	return -ENOTTY;
}

static inline int exynos_drm_ipp_queue_buf(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file)
{
	return -ENOTTY;
}

static inline int exynos_drm_ipp_cmd_ctrl(struct drm_device *drm_dev,
						void *data,
						struct drm_file *file)
{
	return -ENOTTY;
}

static inline int exynos_drm_ippnb_register(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int exynos_drm_ippnb_unregister(struct notifier_block *nb)
{
	return -ENODEV;
}

static inline int exynos_drm_ippnb_send_event(unsigned long val, void *v)
{
	return -ENOTTY;
}

#ifdef CONFIG_PM_RUNTIME
static inline bool ipp_runtime_suspended(struct device *dev)
{
	return -ENOTTY;
}

static inline int ipp_runtime_suspend(struct device *dev)
{
	return -ENOTTY;
}

static inline int ipp_runtime_resume(struct device *dev)
{
	return -ENOTTY;
}

static inline int ipp_runtime_get_sync(struct exynos_drm_ippdrv *ippdrv)
{
	return -ENOTTY;
}

static inline int ipp_runtime_put_sync(struct exynos_drm_ippdrv *ippdrv)
{
	return -ENOTTY;
}
#endif

#endif

#endif /* _EXYNOS_DRM_IPP_H_ */

