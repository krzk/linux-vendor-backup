/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Authors:
 *	YoungJun Cho <yj44.cho@samsung.com>
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundationr
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include <drm/drmP.h>
#include <drm/exynos_drm.h>
#include "regs-rotator.h"
#include "exynos_drm.h"
#include "exynos_drm_ipp.h"
#include "exynos_drm_dbg.h"

/*
 * Rotator supports image crop/rotator and input/output DMA operations.
 * input DMA reads image data from the memory.
 * output DMA writes image data to memory.
 *
 * M2M operation : supports crop/scale/rotation/csc so on.
 * Memory ----> Rotator H/W ----> Memory.
 */

/*
 * TODO
 * 1. check suspend/resume api if needed.
 * 2. need to check use case platform_device_id.
 * 3. check src/dst size with, height.
 * 4. need to add supported list in prop_list.
 */

#ifdef CONFIG_DRM_EXYNOS_DBG
#define ROT_MAX_REG	128
#endif
#define get_rot_context(dev)	platform_get_drvdata(to_platform_device(dev))
#define get_ctx_from_ippdrv(ippdrv)	container_of(ippdrv,\
					struct rot_context, ippdrv);
#define rot_read(offset)		readl(rot->regs + (offset))
#define rot_write(cfg, offset)	writel(cfg, rot->regs + (offset))

enum rot_irq_status {
	ROT_IRQ_STATUS_COMPLETE	= 8,
	ROT_IRQ_STATUS_ILLEGAL	= 9,
};

/*
 * A structure of limitation.
 *
 * @min_w: minimum width.
 * @min_h: minimum height.
 * @max_w: maximum width.
 * @max_h: maximum height.
 * @align: align size.
 */
struct rot_limit {
	u32	min_w;
	u32	min_h;
	u32	max_w;
	u32	max_h;
	u32	align;
};

/*
 * A structure of limitation table.
 *
 * @ycbcr420_2p: case of YUV.
 * @rgb888: case of RGB.
 */
struct rot_limit_table {
	struct rot_limit	ycbcr420_2p;
	struct rot_limit	rgb888;
};

/*
 * A structure of rotator context.
 * @ippdrv: prepare initialization using ippdrv.
 * @regs_res: register resources.
 * @regs: memory mapped io registers.
 * @clock: rotator gate clock.
 * @limit_tbl: limitation of rotator.
 * @irq: irq number.
 * @cur_buf_id: current operation buffer id.
 */
struct rot_context {
	struct exynos_drm_ippdrv	ippdrv;
	struct resource	*regs_res;
	void __iomem	*regs;
	struct clk	*clock;
	struct rot_limit_table	*limit_tbl;
	int	irq;
	int	cur_buf_id[EXYNOS_DRM_OPS_MAX];
};

static void rotator_reg_set_irq(struct rot_context *rot, bool enable)
{
	u32 val = rot_read(ROT_CONFIG);

	if (enable == true)
		val |= ROT_CONFIG_IRQ;
	else
		val &= ~ROT_CONFIG_IRQ;

	rot_write(val, ROT_CONFIG);
}

static u32 rotator_reg_get_fmt(struct rot_context *rot)
{
	u32 val = rot_read(ROT_CONTROL);

	val &= ROT_CONTROL_FMT_MASK;

	return val;
}

static enum rot_irq_status rotator_reg_get_irq_status(struct rot_context *rot)
{
	u32 val = rot_read(ROT_STATUS);

	val = ROT_STATUS_IRQ(val);

	if (val == ROT_STATUS_IRQ_VAL_COMPLETE)
		return ROT_IRQ_STATUS_COMPLETE;

	return ROT_IRQ_STATUS_ILLEGAL;
}

static irqreturn_t rotator_irq_handler(int irq, void *arg)
{
	struct rot_context *rot = arg;
	struct exynos_drm_ippdrv *ippdrv = &rot->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property = &c_node->property;
	struct drm_exynos_ipp_event_info *event = c_node->event;
	enum rot_irq_status irq_status;
	u32 val;

	/* Get execution result */
	irq_status = rotator_reg_get_irq_status(rot);

	/* clear status */
	val = rot_read(ROT_STATUS);
	val |= ROT_STATUS_IRQ_PENDING((u32)irq_status);
	rot_write(val, ROT_STATUS);

	if (c_node->state == IPP_STATE_STOP) {
		DRM_ERROR("invalid state:prop_id[%d]\n", property->prop_id);
		return IRQ_HANDLED;
	}

	if (irq_status == ROT_IRQ_STATUS_COMPLETE) {
		event->ippdrv = ippdrv;
		event->buf_id[EXYNOS_DRM_OPS_DST] =
			rot->cur_buf_id[EXYNOS_DRM_OPS_DST];
		ippdrv->sched_event(event);
	} else
		DRM_ERROR("the SFR is set illegally\n");

	return IRQ_HANDLED;
}

static void rotator_align_size(struct rot_context *rot, u32 fmt, u32 *hsize,
		u32 *vsize)
{
	struct rot_limit_table *limit_tbl = rot->limit_tbl;
	struct rot_limit *limit;
	u32 mask, val;

	/* Get size limit */
	if (fmt == ROT_CONTROL_FMT_RGB888)
		limit = &limit_tbl->rgb888;
	else
		limit = &limit_tbl->ycbcr420_2p;

	/* Get mask for rounding to nearest aligned val */
	mask = ~((1 << limit->align) - 1);

	/* Set aligned width */
	val = ROT_ALIGN(*hsize, limit->align, mask);
	if (val < limit->min_w)
		*hsize = ROT_MIN(limit->min_w, mask);
	else if (val > limit->max_w)
		*hsize = ROT_MAX(limit->max_w, mask);
	else
		*hsize = val;

	/* Set aligned height */
	val = ROT_ALIGN(*vsize, limit->align, mask);
	if (val < limit->min_h)
		*vsize = ROT_MIN(limit->min_h, mask);
	else if (val > limit->max_h)
		*vsize = ROT_MAX(limit->max_h, mask);
	else
		*vsize = val;
}

static int rotator_src_set_fmt(struct device *dev, u32 fmt)
{
	struct rot_context *rot = dev_get_drvdata(dev);
	u32 val;

	val = rot_read(ROT_CONTROL);
	val &= ~ROT_CONTROL_FMT_MASK;

	switch (fmt) {
	case DRM_FORMAT_NV12:
		val |= ROT_CONTROL_FMT_YCBCR420_2P;
		break;
	case DRM_FORMAT_XRGB8888:
		val |= ROT_CONTROL_FMT_RGB888;
		break;
	default:
		DRM_ERROR("invalid image format\n");
		return -EINVAL;
	}

	rot_write(val, ROT_CONTROL);

	return 0;
}

static inline bool rotator_check_reg_fmt(u32 fmt)
{
	if ((fmt == ROT_CONTROL_FMT_YCBCR420_2P) ||
	    (fmt == ROT_CONTROL_FMT_RGB888))
		return true;

	return false;
}

static int rotator_src_set_size(struct device *dev, int swap,
		struct drm_exynos_pos *pos,
		struct drm_exynos_sz *sz)
{
	struct rot_context *rot = dev_get_drvdata(dev);
	u32 fmt, hsize, vsize;
	u32 val;

	/* Get format */
	fmt = rotator_reg_get_fmt(rot);
	if (!rotator_check_reg_fmt(fmt)) {
		DRM_ERROR("invalid format.\n");
		return -EINVAL;
	}

	/* Align buffer size */
	hsize = sz->hsize;
	vsize = sz->vsize;
	rotator_align_size(rot, fmt, &hsize, &vsize);

	/* Set buffer size configuration */
	val = ROT_SET_BUF_SIZE_H(vsize) | ROT_SET_BUF_SIZE_W(hsize);
	rot_write(val, ROT_SRC_BUF_SIZE);

	/* Set crop image position configuration */
	val = ROT_CROP_POS_Y(pos->y) | ROT_CROP_POS_X(pos->x);
	rot_write(val, ROT_SRC_CROP_POS);
	val = ROT_SRC_CROP_SIZE_H(pos->h) | ROT_SRC_CROP_SIZE_W(pos->w);
	rot_write(val, ROT_SRC_CROP_SIZE);

	return 0;
}

static int rotator_src_set_addr(struct device *dev,
		struct drm_exynos_ipp_buf_info *buf_info,
		u32 buf_id, enum drm_exynos_ipp_buf_type buf_type)
{
	struct rot_context *rot = dev_get_drvdata(dev);
	dma_addr_t addr[EXYNOS_DRM_PLANAR_MAX];
	u32 val, fmt, hsize, vsize;
	int i;

	/* Set current buf_id */
	rot->cur_buf_id[EXYNOS_DRM_OPS_SRC] = buf_id;

	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		/* Set address configuration */
		for_each_ipp_planar(i)
			addr[i] = buf_info->base[i];

		/* Get format */
		fmt = rotator_reg_get_fmt(rot);
		if (!rotator_check_reg_fmt(fmt)) {
			DRM_ERROR("invalid format.\n");
			return -EINVAL;
		}

		/* Re-set cb planar for NV12 format */
		if ((fmt == ROT_CONTROL_FMT_YCBCR420_2P) &&
		    !addr[EXYNOS_DRM_PLANAR_CB]) {

			val = rot_read(ROT_SRC_BUF_SIZE);
			hsize = ROT_GET_BUF_SIZE_W(val);
			vsize = ROT_GET_BUF_SIZE_H(val);

			/* Set cb planar */
			addr[EXYNOS_DRM_PLANAR_CB] =
				addr[EXYNOS_DRM_PLANAR_Y] + hsize * vsize;
		}

		for_each_ipp_planar(i)
			rot_write(addr[i], ROT_SRC_BUF_ADDR(i));
		break;
	case IPP_BUF_DEQUEUE:
		for_each_ipp_planar(i)
			rot_write(0x0, ROT_SRC_BUF_ADDR(i));
		break;
	default:
		/* Nothing to do */
		break;
	}

	return 0;
}

static int rotator_dst_set_transf(struct device *dev,
		enum drm_exynos_degree degree,
		enum drm_exynos_flip flip, bool *swap)
{
	struct rot_context *rot = dev_get_drvdata(dev);
	u32 val;

	/* Set transform configuration */
	val = rot_read(ROT_CONTROL);
	val &= ~ROT_CONTROL_FLIP_MASK;

	switch (flip) {
	case EXYNOS_DRM_FLIP_VERTICAL:
		val |= ROT_CONTROL_FLIP_VERTICAL;
		break;
	case EXYNOS_DRM_FLIP_HORIZONTAL:
		val |= ROT_CONTROL_FLIP_HORIZONTAL;
		break;
	default:
		/* Flip None */
		break;
	}

	val &= ~ROT_CONTROL_ROT_MASK;

	switch (degree) {
	case EXYNOS_DRM_DEGREE_90:
		val |= ROT_CONTROL_ROT_90;
		break;
	case EXYNOS_DRM_DEGREE_180:
		val |= ROT_CONTROL_ROT_180;
		break;
	case EXYNOS_DRM_DEGREE_270:
		val |= ROT_CONTROL_ROT_270;
		break;
	default:
		/* Rotation 0 Degree */
		break;
	}

	rot_write(val, ROT_CONTROL);

	/* Check degree for setting buffer size swap */
	if ((degree == EXYNOS_DRM_DEGREE_90) ||
	    (degree == EXYNOS_DRM_DEGREE_270))
		*swap = true;
	else
		*swap = false;

	return 0;
}

static int rotator_dst_set_size(struct device *dev, int swap,
		struct drm_exynos_pos *pos,
		struct drm_exynos_sz *sz)
{
	struct rot_context *rot = dev_get_drvdata(dev);
	u32 val, fmt, hsize, vsize;

	/* Get format */
	fmt = rotator_reg_get_fmt(rot);
	if (!rotator_check_reg_fmt(fmt)) {
		DRM_ERROR("invalid format.\n");
		return -EINVAL;
	}

	/* Align buffer size */
	hsize = sz->hsize;
	vsize = sz->vsize;
	rotator_align_size(rot, fmt, &hsize, &vsize);

	/* Set buffer size configuration */
	val = ROT_SET_BUF_SIZE_H(vsize) | ROT_SET_BUF_SIZE_W(hsize);
	rot_write(val, ROT_DST_BUF_SIZE);

	/* Set crop image position configuration */
	val = ROT_CROP_POS_Y(pos->y) | ROT_CROP_POS_X(pos->x);
	rot_write(val, ROT_DST_CROP_POS);

	return 0;
}

static int rotator_dst_set_addr(struct device *dev,
		struct drm_exynos_ipp_buf_info *buf_info,
		u32 buf_id, enum drm_exynos_ipp_buf_type buf_type)
{
	struct rot_context *rot = dev_get_drvdata(dev);
	dma_addr_t addr[EXYNOS_DRM_PLANAR_MAX];
	u32 val, fmt, hsize, vsize;
	int i;

	/* Set current buf_id */
	rot->cur_buf_id[EXYNOS_DRM_OPS_DST] = buf_id;

	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		/* Set address configuration */
		for_each_ipp_planar(i)
			addr[i] = buf_info->base[i];

		/* Get format */
		fmt = rotator_reg_get_fmt(rot);
		if (!rotator_check_reg_fmt(fmt)) {
			DRM_ERROR("invalid format.\n");
			return -EINVAL;
		}

		/* Re-set cb planar for NV12 format */
		if ((fmt == ROT_CONTROL_FMT_YCBCR420_2P) &&
		    !addr[EXYNOS_DRM_PLANAR_CB]) {
			/* Get buf size */
			val = rot_read(ROT_DST_BUF_SIZE);

			hsize = ROT_GET_BUF_SIZE_W(val);
			vsize = ROT_GET_BUF_SIZE_H(val);

			/* Set cb planar */
			addr[EXYNOS_DRM_PLANAR_CB] =
				addr[EXYNOS_DRM_PLANAR_Y] + hsize * vsize;
		}

		for_each_ipp_planar(i)
			rot_write(addr[i], ROT_DST_BUF_ADDR(i));
		break;
	case IPP_BUF_DEQUEUE:
		for_each_ipp_planar(i)
			rot_write(0x0, ROT_DST_BUF_ADDR(i));
		break;
	default:
		/* Nothing to do */
		break;
	}

	return 0;
}

static struct exynos_drm_ipp_ops rot_src_ops = {
	.set_fmt	=	rotator_src_set_fmt,
	.set_size	=	rotator_src_set_size,
	.set_addr	=	rotator_src_set_addr,
};

static struct exynos_drm_ipp_ops rot_dst_ops = {
	.set_transf	=	rotator_dst_set_transf,
	.set_size	=	rotator_dst_set_size,
	.set_addr	=	rotator_dst_set_addr,
};

static int rotator_init_capability(struct exynos_drm_ippdrv *ippdrv)
{
	struct drm_exynos_ipp_capability *capability;

	DRM_DEBUG_KMS("%s\n", __func__);

	capability = devm_kzalloc(ippdrv->dev, sizeof(*capability), GFP_KERNEL);
	if (!capability) {
		DRM_ERROR("failed to alloc capability.\n");
		return -ENOMEM;
	}

	capability->flip = (1 << EXYNOS_DRM_FLIP_VERTICAL) |
				(1 << EXYNOS_DRM_FLIP_HORIZONTAL);
	capability->degree = (1 << EXYNOS_DRM_DEGREE_0) |
				(1 << EXYNOS_DRM_DEGREE_90) |
				(1 << EXYNOS_DRM_DEGREE_180) |
				(1 << EXYNOS_DRM_DEGREE_270);
	capability->csc = 0;
	capability->crop = 0;
	capability->scale = 0;

	ippdrv->capability = capability;

	return 0;
}

static inline bool rotator_check_drm_fmt(u32 fmt)
{
	switch (fmt) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_NV12:
		return true;
	default:
		DRM_DEBUG_KMS("%s:not support format\n", __func__);
		return false;
	}
}

static inline bool rotator_check_drm_flip(enum drm_exynos_flip flip)
{
	switch (flip) {
	case EXYNOS_DRM_FLIP_NONE:
	case EXYNOS_DRM_FLIP_VERTICAL:
	case EXYNOS_DRM_FLIP_HORIZONTAL:
	case EXYNOS_DRM_FLIP_BOTH:
		return true;
	default:
		DRM_DEBUG_KMS("%s:invalid flip\n", __func__);
		return false;
	}
}

static int rotator_ippdrv_check_property(struct device *dev,
		struct drm_exynos_ipp_property *property)
{
	struct drm_exynos_ipp_config *src_config =
					&property->config[EXYNOS_DRM_OPS_SRC];
	struct drm_exynos_ipp_config *dst_config =
					&property->config[EXYNOS_DRM_OPS_DST];
	struct drm_exynos_pos *src_pos = &src_config->pos;
	struct drm_exynos_pos *dst_pos = &dst_config->pos;
	struct drm_exynos_sz *src_sz = &src_config->sz;
	struct drm_exynos_sz *dst_sz = &dst_config->sz;
	bool swap = false;

	/* Check command configuration */
	if (!ipp_is_m2m_cmd(property->cmd)) {
		DRM_DEBUG_KMS("%s:not support cmd\n", __func__);
		return -EINVAL;
	}

	/* Check format configuration */
	if (src_config->fmt != dst_config->fmt) {
		DRM_DEBUG_KMS("%s:not support csc feature\n", __func__);
		return -EINVAL;
	}

	if (!rotator_check_drm_fmt(dst_config->fmt)) {
		DRM_DEBUG_KMS("%s:invalid format\n", __func__);
		return -EINVAL;
	}

	/* Check transform configuration */
	if (src_config->degree != EXYNOS_DRM_DEGREE_0) {
		DRM_DEBUG_KMS("%s:not support source-side rotation\n",
			__func__);
		return -EINVAL;
	}

	switch (dst_config->degree) {
	case EXYNOS_DRM_DEGREE_90:
	case EXYNOS_DRM_DEGREE_270:
		swap = true;
	case EXYNOS_DRM_DEGREE_0:
	case EXYNOS_DRM_DEGREE_180:
		/* No problem */
		break;
	default:
		DRM_DEBUG_KMS("%s:invalid degree\n", __func__);
		return -EINVAL;
	}

	if (src_config->flip != EXYNOS_DRM_FLIP_NONE) {
		DRM_DEBUG_KMS("%s:not support source-side flip\n", __func__);
		return -EINVAL;
	}

	if (!rotator_check_drm_flip(dst_config->flip)) {
		DRM_DEBUG_KMS("%s:invalid flip\n", __func__);
		return -EINVAL;
	}

	/* Check size configuration */
	if ((src_pos->x + src_pos->w > src_sz->hsize) ||
		(src_pos->y + src_pos->h > src_sz->vsize)) {
		DRM_DEBUG_KMS("%s:out of source buffer bound\n", __func__);
		return -EINVAL;
	}

	if (swap) {
		if ((dst_pos->x + dst_pos->h > dst_sz->vsize) ||
			(dst_pos->y + dst_pos->w > dst_sz->hsize)) {
			DRM_DEBUG_KMS("%s:out of destination buffer bound\n",
				__func__);
			return -EINVAL;
		}

		if ((src_pos->w != dst_pos->h) || (src_pos->h != dst_pos->w)) {
			DRM_DEBUG_KMS("%s:not support scale feature\n",
				__func__);
			return -EINVAL;
		}
	} else {
		if ((dst_pos->x + dst_pos->w > dst_sz->hsize) ||
			(dst_pos->y + dst_pos->h > dst_sz->vsize)) {
			DRM_DEBUG_KMS("%s:out of destination buffer bound\n",
				__func__);
			return -EINVAL;
		}

		if ((src_pos->w != dst_pos->w) || (src_pos->h != dst_pos->h)) {
			DRM_DEBUG_KMS("%s:not support scale feature\n",
				__func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int rotator_ippdrv_start(struct device *dev, enum drm_exynos_ipp_cmd cmd)
{
	struct rot_context *rot = dev_get_drvdata(dev);
	u32 val;

	if (cmd != IPP_CMD_M2M) {
		DRM_ERROR("not support cmd: %d\n", cmd);
		return -EINVAL;
	}

	/* Set interrupt enable */
	rotator_reg_set_irq(rot, true);

	val = rot_read(ROT_CONTROL);
	val |= ROT_CONTROL_START;

	rot_write(val, ROT_CONTROL);

	return 0;
}

#ifdef CONFIG_DRM_EXYNOS_DBG
static int rotator_read_reg(struct rot_context *rot, char *buf)
{
	struct resource *res = rot->regs_res;
	int i, pos = 0;
	u32 cfg;

	pos += sprintf(buf+pos, "0x%.8x | ", res->start);
	for (i = 1; i < ROT_MAX_REG + 1; i++) {
		cfg = rot_read((i-1) * sizeof(u32));
		pos += sprintf(buf+pos, "0x%.8x ", cfg);
		if (i % 4 == 0)
			pos += sprintf(buf+pos, "\n0x%.8x | ",
				res->start + (i * sizeof(u32)));
	}

	pos += sprintf(buf+pos, "\n");

	return pos;
}

static ssize_t show_read_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct rot_context *rot = get_rot_context(dev);

	if (!rot->regs) {
		dev_err(dev, "failed to get current register.\n");
		return -EINVAL;
	}

	return rotator_read_reg(rot, buf);
}

static struct device_attribute device_attrs[] = {
	__ATTR(read_reg, S_IRUGO, show_read_reg, NULL),
};
#endif

static int rotator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rot_context *rot;
	struct exynos_drm_ippdrv *ippdrv;
	int ret;

	rot = devm_kzalloc(dev, sizeof(*rot), GFP_KERNEL);
	if (!rot) {
		dev_err(dev, "failed to allocate rot\n");
		return -ENOMEM;
	}

	rot->limit_tbl = (struct rot_limit_table *)
				platform_get_device_id(pdev)->driver_data;

	rot->regs_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rot->regs = devm_request_and_ioremap(dev, rot->regs_res);
	if (!rot->regs) {
		dev_err(dev, "failed to map register\n");
		return -ENXIO;
	}

	rot->irq = platform_get_irq(pdev, 0);
	if (rot->irq < 0) {
		dev_err(dev, "failed to get irq\n");
		return rot->irq;
	}

	ret = request_threaded_irq(rot->irq, NULL, rotator_irq_handler,
			IRQF_ONESHOT, "drm_rotator", rot);
	if (ret < 0) {
		dev_err(dev, "failed to request irq\n");
		return ret;
	}

	rot->clock = clk_get(dev, "rotator");
	if (IS_ERR_OR_NULL(rot->clock)) {
		dev_err(dev, "failed to get clock\n");
		ret = PTR_ERR(rot->clock);
		goto err_clk_get;
	}

	pm_runtime_enable(dev);

	ippdrv = &rot->ippdrv;
	ippdrv->dev = dev;
	ippdrv->ops[EXYNOS_DRM_OPS_SRC] = &rot_src_ops;
	ippdrv->ops[EXYNOS_DRM_OPS_DST] = &rot_dst_ops;
	ippdrv->check_property = rotator_ippdrv_check_property;
	ippdrv->start = rotator_ippdrv_start;
	ret = rotator_init_capability(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to init property list.\n");
		goto err_ippdrv_register;
	}

	DRM_DEBUG_KMS("%s:ippdrv[0x%x]\n", __func__, (int)ippdrv);

	platform_set_drvdata(pdev, rot);

	ret = exynos_drm_ippdrv_register(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to register drm rotator device\n");
		goto err_ippdrv_register;
	}

#ifdef CONFIG_DRM_EXYNOS_DBG
	exynos_drm_dbg_register(&pdev->dev, device_attrs,
			ARRAY_SIZE(device_attrs), rot->regs);
#endif

	dev_info(dev, "The exynos rotator is probed successfully\n");

	return 0;

err_ippdrv_register:
	devm_kfree(dev, ippdrv->capability);
	pm_runtime_disable(dev);
err_clk_get:
	free_irq(rot->irq, rot);
	return ret;
}

static int rotator_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rot_context *rot = dev_get_drvdata(dev);
	struct exynos_drm_ippdrv *ippdrv = &rot->ippdrv;

#ifdef CONFIG_DRM_EXYNOS_DBG
	exynos_drm_dbg_unregister(&pdev->dev, device_attrs);
#endif

	devm_kfree(dev, ippdrv->capability);
	exynos_drm_ippdrv_unregister(ippdrv);

	pm_runtime_disable(dev);

	free_irq(rot->irq, rot);

	return 0;
}

static struct rot_limit_table rot_limit_tbl = {
	.ycbcr420_2p = {
		.min_w = 32,
		.min_h = 32,
		.max_w = SZ_32K,
		.max_h = SZ_32K,
		.align = 3,
	},
	.rgb888 = {
		.min_w = 8,
		.min_h = 8,
		.max_w = SZ_8K,
		.max_h = SZ_8K,
		.align = 2,
	},
};

static struct platform_device_id rotator_driver_ids[] = {
	{
		.name		= "exynos-rot",
		.driver_data	= (unsigned long)&rot_limit_tbl,
	},
	{},
};

static int rotator_clk_ctrl(struct rot_context *rot, bool enable, bool wait)
{
	struct exynos_drm_ippdrv *ippdrv = &rot->ippdrv;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (enable)
		clk_enable(rot->clock);
	else
		clk_disable(rot->clock);

	if (wait)
		complete(&ippdrv->pm_complete);

	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int rotator_suspend(struct device *dev)
{
	struct rot_context *rot = dev_get_drvdata(dev);

	DRM_DEBUG_KMS("%s\n", __func__);

	if (pm_runtime_suspended(dev))
		return 0;

	return rotator_clk_ctrl(rot, false, true);
}

static int rotator_resume(struct device *dev)
{
	struct rot_context *rot = dev_get_drvdata(dev);

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!pm_runtime_suspended(dev))
		return rotator_clk_ctrl(rot, true, true);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int rotator_runtime_suspend(struct device *dev)
{
	struct rot_context *rot = dev_get_drvdata(dev);

	DRM_DEBUG_KMS("%s\n", __func__);

	return  rotator_clk_ctrl(rot, false, true);
}

static int rotator_runtime_resume(struct device *dev)
{
	struct rot_context *rot = dev_get_drvdata(dev);

	DRM_DEBUG_KMS("%s\n", __func__);

	return  rotator_clk_ctrl(rot, true, true);
}
#endif

static const struct dev_pm_ops rotator_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(rotator_suspend, rotator_resume)
	SET_RUNTIME_PM_OPS(rotator_runtime_suspend, rotator_runtime_resume,
									NULL)
};

struct platform_driver rotator_driver = {
	.probe		= rotator_probe,
	.remove		= rotator_remove,
	.id_table	= rotator_driver_ids,
	.driver		= {
		.name	= "exynos-rot",
		.owner	= THIS_MODULE,
		.pm	= &rotator_pm_ops,
	},
};
