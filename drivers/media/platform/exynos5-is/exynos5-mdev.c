/*
 * EXYNOS5 SoC series camera host interface media device driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Shaik Ameer Basha <shaik.ameer@samsung.com>
 * Arun Kumar K <arun.kk@samsung.com>
 *
 * This driver is based on exynos4-is media device driver written by
 * Sylwester Nawrocki <s.nawrocki@samsung.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/media-device.h>
#include <media/s5p_fimc.h>
#include <media/v4l2-async.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-of.h>

#include "exynos5-mdev.h"
#include "fimc-is.h"

#define BAYER_CLK_NAME "sclk_bayer"

/**
 * fimc_pipeline_prepare - update pipeline information with subdevice pointers
 * @me: media entity terminating the pipeline
 *
 * Caller holds the graph mutex.
 */
static void fimc_pipeline_prepare(struct fimc_pipeline *p,
				  struct media_entity *me)
{
	struct v4l2_subdev *sd;
	int i;

	for (i = 0; i < IDX_MAX; i++)
		p->subdevs[i] = NULL;

	while (1) {
		struct media_pad *pad = NULL;

		/* Find remote source pad */
		for (i = 0; i < me->num_pads; i++) {
			struct media_pad *spad = &me->pads[i];
			if (!(spad->flags & MEDIA_PAD_FL_SINK))
				continue;
			pad = media_entity_remote_pad(spad);
			if (pad)
				break;
		}

		if (pad == NULL ||
		    media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV) {
			break;
		}
		sd = media_entity_to_v4l2_subdev(pad->entity);

		switch (sd->grp_id) {
		case GRP_ID_FIMC_IS_SENSOR:
		case GRP_ID_SENSOR:
			p->subdevs[IDX_SENSOR] = sd;
			break;
		case GRP_ID_CSIS:
			p->subdevs[IDX_CSIS] = sd;
			break;
		case GRP_ID_FLITE:
			p->subdevs[IDX_FLITE] = sd;
			break;
		default:
			pr_warn("%s: Unknown subdev grp_id: %#x\n",
				__func__, sd->grp_id);
		}
		me = &sd->entity;
		if (me->num_pads == 1)
			break;
	}

	/*
	 * For using FIMC-IS firmware controlled sensors, ISP subdev
	 * has to be initialized along with pipeline0 devices.
	 * So an ISP subdev from a free ISP pipeline is assigned to
	 * this pipeline.
	 */
	if (p->subdevs[IDX_SENSOR] &&
		(p->subdevs[IDX_SENSOR]->grp_id == GRP_ID_FIMC_IS_SENSOR)) {
		struct fimc_pipeline_isp *p_isp;

		list_for_each_entry(p_isp, p->isp_pipelines, list) {
			if (!p_isp->in_use) {
				p->subdevs[IDX_FIMC_IS] =
					p_isp->subdevs[IDX_ISP];
				p_isp->in_use = true;
				break;
			}
		}
	}
}

/**
 * fimc_pipeline_cleanup - update pipeline information with regard to
 * 			   associated subdevs upon pipeline being closed
 * 			   explicitly or as a result of an unexpected
 * 			   failure while processing
 * @p: the pipeline object
 *
 * Caller holds the graph mutex.
 */
static void fimc_pipeline_cleanup(struct fimc_pipeline *p)
{
	if (p->subdevs[IDX_SENSOR]->grp_id == GRP_ID_FIMC_IS_SENSOR) {
		struct fimc_pipeline_isp *p_isp;

		list_for_each_entry(p_isp, p->isp_pipelines, list) {
			if (p_isp->subdevs[IDX_ISP] ==
					p->subdevs[IDX_FIMC_IS]) {
				p->subdevs[IDX_FIMC_IS] = NULL;
				p_isp->in_use = false;
				break;
			}
		}
	}
}

/**
 * __subdev_set_power - change power state of a single subdev
 * @sd: subdevice to change power state for
 * @on: 1 to enable power or 0 to disable
 *
 * Return result of s_power subdev operation or -ENXIO if sd argument
 * is NULL. Return 0 if the subdevice does not implement s_power.
 */
static int __subdev_set_power(struct v4l2_subdev *sd, int on)
{
	int *use_count;
	int ret;

	if (sd == NULL)
		return -ENXIO;

	use_count = &sd->entity.use_count;
	if (on && (*use_count)++ > 0)
		return 0;
	else if (!on && (*use_count == 0 || --(*use_count) > 0))
		return 0;
	ret = v4l2_subdev_call(sd, core, s_power, on);

	return ret != -ENOIOCTLCMD ? ret : 0;
}

/**
 * fimc_pipeline_s_power - change power state of all pipeline subdevs
 * @fimc: fimc device terminating the pipeline
 * @state: true to power on, false to power off
 *
 * Needs to be called with the graph mutex held.
 */
static int fimc_pipeline_s_power(struct fimc_pipeline *p, bool state)
{
	int i;
	int ret;
	struct fimc_is_isp *isp_dev;

	if (p->subdevs[IDX_SENSOR] == NULL)
		return -ENXIO;

	/*
	 * If sensor is firmware controlled IS-sensor,
	 * set sensor sd to isp context.
	 */
	if (p->subdevs[IDX_FIMC_IS]) {
		isp_dev = v4l2_get_subdevdata(p->subdevs[IDX_FIMC_IS]);
		isp_dev->sensor_sd = p->subdevs[IDX_SENSOR];
	}

	for (i = 0; i < IDX_MAX; i++) {
		unsigned int idx = state ? i : (IDX_MAX - 1) - i;

		ret = __subdev_set_power(p->subdevs[idx], state);
		if (ret < 0 && ret != -ENXIO)
			goto rollback;
	}
	return 0;
rollback:
	for (; state && i >= 0; --i) {
		unsigned int idx = state ? i : (IDX_MAX - 1) - i;
		 __subdev_set_power(p->subdevs[idx], !state);
	}
	return ret;
}

/**
 * __fimc_pipeline_open - update the pipeline information, enable power
 *                        of all pipeline subdevs and the sensor clock
 * @me: media entity to start graph walk with
 * @prepare: true to walk the current pipeline and acquire all subdevs
 *
 * Called with the graph mutex held.
 */
static int __fimc_pipeline_open(struct exynos_media_pipeline *ep,
				struct media_entity *me, bool prepare)
{
	struct fimc_pipeline *p = to_fimc_pipeline(ep);
	struct v4l2_subdev *sd;
	struct fimc_source_info *si;
	struct fimc_md *fmd;
	int ret;

	if (WARN_ON(p == NULL || me == NULL))
		return -EINVAL;

	if (prepare)
		fimc_pipeline_prepare(p, me);

	sd = p->subdevs[IDX_SENSOR];
	if (sd == NULL)
		return -EINVAL;

	si = v4l2_get_subdev_hostdata(sd);
	fmd = entity_to_fimc_mdev(&sd->entity);

	if (!IS_ERR(fmd->clk_bayer)) {
		ret = clk_prepare_enable(fmd->clk_bayer);
		if (ret < 0)
			return ret;
	}

	ret = fimc_pipeline_s_power(p, 1);
	if (ret) {
		if (!IS_ERR(fmd->clk_bayer))
			clk_disable_unprepare(fmd->clk_bayer);
		fimc_pipeline_cleanup(p);
	}
	return ret;
}

/**
 * __fimc_pipeline_close - disable the sensor clock and pipeline power
 * @fimc: fimc device terminating the pipeline
 *
 * Disable power of all subdevs and turn the external sensor clock off.
 */
static int __fimc_pipeline_close(struct exynos_media_pipeline *ep)
{
	struct fimc_pipeline *p = to_fimc_pipeline(ep);
	struct v4l2_subdev *sd = p ? p->subdevs[IDX_SENSOR] : NULL;
	struct fimc_source_info *si;
	struct fimc_md *fmd;
	int ret = 0;

	if (WARN_ON(sd == NULL))
		return -EINVAL;

	if (p->subdevs[IDX_SENSOR]) {
		ret = fimc_pipeline_s_power(p, 0);
		if(ret)
			goto leave;
	}

	si = v4l2_get_subdev_hostdata(sd);
	fmd = entity_to_fimc_mdev(&sd->entity);

	if (!IS_ERR(fmd->clk_bayer))
		clk_disable_unprepare(fmd->clk_bayer);

	fimc_pipeline_cleanup(p);
leave:
	return ret == -ENXIO ? 0 : ret;
}

/**
 * __fimc_pipeline_s_stream - call s_stream() on pipeline subdevs
 * @pipeline: video pipeline structure
 * @on: passed as the s_stream() callback argument
 */
static int __fimc_pipeline_s_stream(struct exynos_media_pipeline *ep, bool on)
{
	struct fimc_pipeline *p = to_fimc_pipeline(ep);
	int i, ret;

	if (p->subdevs[IDX_SENSOR] == NULL)
		return -ENODEV;

	for (i = 0; i < IDX_MAX; i++) {
		unsigned int idx = on ? i : (IDX_MAX - 1) - i;

		ret = v4l2_subdev_call(p->subdevs[idx], video, s_stream, on);

		if (ret < 0 && ret != -ENOIOCTLCMD && ret != -ENODEV)
			return ret;
	}
	return 0;
}

/* Media pipeline operations for the FIMC/FIMC-LITE video device driver */
static const struct exynos_media_pipeline_ops exynos5_pipeline0_ops = {
	.open		= __fimc_pipeline_open,
	.close		= __fimc_pipeline_close,
	.set_stream	= __fimc_pipeline_s_stream,
};

static struct exynos_media_pipeline *fimc_md_pipeline_create(
						struct fimc_md *fmd)
{
	struct fimc_pipeline *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	list_add_tail(&p->list, &fmd->pipelines);

	p->isp_pipelines = &fmd->isp_pipelines;
	p->ep.ops = &exynos5_pipeline0_ops;
	return &p->ep;
}

static struct exynos_media_pipeline *fimc_md_isp_pipeline_create(
						struct fimc_md *fmd)
{
	struct fimc_pipeline_isp *p;

	p = kzalloc(sizeof(*p), GFP_KERNEL);
	if (!p)
		return NULL;

	list_add_tail(&p->list, &fmd->isp_pipelines);

	p->in_use = false;
	return &p->ep;
}

static void fimc_md_pipelines_free(struct fimc_md *fmd)
{
	while (!list_empty(&fmd->pipelines)) {
		struct fimc_pipeline *p;

		p = list_entry(fmd->pipelines.next, typeof(*p), list);
		list_del(&p->list);
		kfree(p);
	}
	while (!list_empty(&fmd->isp_pipelines)) {
		struct fimc_pipeline_isp *p;

		p = list_entry(fmd->isp_pipelines.next, typeof(*p), list);
		list_del(&p->list);
		kfree(p);
	}
}

/* Parse port node and register as a sub-device any sensor specified there. */
static int fimc_md_parse_port_node(struct fimc_md *fmd,
				   struct device_node *port,
				   unsigned int index)
{
	struct device_node *rem, *ep, *np;
	struct fimc_source_info *pd;
	struct v4l2_of_endpoint endpoint;

	pd = &fmd->sensor[index].pdata;

	/* Assume here a port node can have only one endpoint node. */
	ep = of_get_next_child(port, NULL);
	if (!ep)
		return 0;

	v4l2_of_parse_endpoint(ep, &endpoint);
	if (WARN_ON(endpoint.port == 0) || index >= FIMC_MAX_SENSORS)
		return -EINVAL;

	pd->mux_id = (endpoint.port - 1) & 0x1;

	rem = v4l2_of_get_remote_port_parent(ep);
	of_node_put(ep);
	if (rem == NULL) {
		v4l2_info(&fmd->v4l2_dev, "Remote device at %s not found\n",
							ep->full_name);
		return 0;
	}

	if (fimc_input_is_parallel(endpoint.port)) {
		if (endpoint.bus_type == V4L2_MBUS_PARALLEL)
			pd->sensor_bus_type = FIMC_BUS_TYPE_ITU_601;
		else
			pd->sensor_bus_type = FIMC_BUS_TYPE_ITU_656;
		pd->flags = endpoint.bus.parallel.flags;
	} else if (fimc_input_is_mipi_csi(endpoint.port)) {
		/*
		 * MIPI CSI-2: only input mux selection and
		 * the sensor's clock frequency is needed.
		 */
		pd->sensor_bus_type = FIMC_BUS_TYPE_MIPI_CSI2;
	} else {
		v4l2_err(&fmd->v4l2_dev, "Wrong port id (%u) at node %s\n",
			 endpoint.port, rem->full_name);
	}

	np = of_get_parent(rem);

	if (np && !of_node_cmp(np->name, "i2c-isp"))
		pd->fimc_bus_type = FIMC_BUS_TYPE_ISP_WRITEBACK;
	else
		pd->fimc_bus_type = pd->sensor_bus_type;

	if (WARN_ON(index >= ARRAY_SIZE(fmd->sensor)))
		return -EINVAL;

	fmd->sensor[index].asd.match_type = V4L2_ASYNC_MATCH_OF;
	fmd->sensor[index].asd.match.of.node = rem;
	fmd->async_subdevs[index] = &fmd->sensor[index].asd;

	fmd->num_sensors++;

	of_node_put(rem);
	return 0;
}

/* Register all SoC external sub-devices */
static int fimc_md_of_sensors_register(struct fimc_md *fmd,
				       struct device_node *np)
{
	struct device_node *parent = fmd->dev->of_node;
	struct device_node *node, *ports;
	int index, sensor_index = 0;
	int ret;

	/* Attach sensors linked to MIPI CSI-2 receivers */
	for (index = 0; index < FIMC_NUM_MIPI_CSIS; index++) {
		struct device_node *port;

		if (fmd->csis[index].of_node == NULL)
			continue;

		/* The csis node can have only port subnode. */
		port = of_get_next_child(fmd->csis[index].of_node, NULL);
		if (!port)
			continue;

		ret = fimc_md_parse_port_node(fmd, port, sensor_index++);
		if (ret < 0)
			return ret;
	}

	/* Attach sensors listed in the parallel-ports node */
	ports = of_get_child_by_name(parent, "parallel-ports");
	if (!ports)
		return 0;

	for_each_child_of_node(ports, node) {
		ret = fimc_md_parse_port_node(fmd, node, index);
		if (ret < 0)
			break;
		index++;
	}

	return 0;
}

static int __of_get_csis_id(struct device_node *np)
{
	u32 reg = 0;

	np = of_get_child_by_name(np, "port");
	if (!np)
		return -EINVAL;
	of_property_read_u32(np, "reg", &reg);
	return reg - FIMC_INPUT_MIPI_CSI2_0;
}

/*
 * MIPI-CSIS, FIMC-IS and FIMC-LITE platform devices registration.
 */

static int register_fimc_lite_entity(struct fimc_md *fmd,
				     struct fimc_lite *fimc_lite)
{
	struct v4l2_subdev *sd;
	struct exynos_media_pipeline *ep;
	int ret;

	if (WARN_ON(fimc_lite->index >= FIMC_LITE_MAX_DEVS ||
		    fmd->fimc_lite[fimc_lite->index]))
		return -EBUSY;

	sd = &fimc_lite->subdev;
	sd->grp_id = GRP_ID_FLITE;

	ep = fimc_md_pipeline_create(fmd);
	if (!ep)
		return -ENOMEM;

	v4l2_set_subdev_hostdata(sd, ep);

	ret = v4l2_device_register_subdev(&fmd->v4l2_dev, sd);
	if (!ret)
		fmd->fimc_lite[fimc_lite->index] = fimc_lite;
	else
		v4l2_err(&fmd->v4l2_dev, "Failed to register FIMC.LITE%d\n",
			 fimc_lite->index);
	return ret;
}

static int register_csis_entity(struct fimc_md *fmd,
				struct platform_device *pdev,
				struct v4l2_subdev *sd)
{
	struct device_node *node = pdev->dev.of_node;
	int id, ret;

	id = node ? __of_get_csis_id(node) : max(0, pdev->id);

	if (WARN_ON(id < 0 || id >= CSIS_MAX_ENTITIES))
		return -ENOENT;

	if (WARN_ON(fmd->csis[id].sd))
		return -EBUSY;

	sd->grp_id = GRP_ID_CSIS;
	ret = v4l2_device_register_subdev(&fmd->v4l2_dev, sd);
	if (ret < 0) {
		v4l2_err(&fmd->v4l2_dev,
			 "Failed to register MIPI-CSIS.%d (%d)\n", id, ret);
		return ret;
	}

	fmd->csis[id].sd = sd;
	fmd->csis[id].of_node = node;
	return ret;
}

static int register_fimc_is_entity(struct fimc_md *fmd,
				     struct fimc_is *is)
{
	struct v4l2_subdev *subdev;
	struct exynos_media_pipeline *ep;
	struct fimc_pipeline_isp *p;
	struct video_device *vdev;
	int ret, i;

	for (i = 0; i < is->drvdata->num_instances; i++) {

		ep = fimc_md_isp_pipeline_create(fmd);
		if (!ep)
			return -ENOMEM;
		p = to_fimc_isp_pipeline(ep);

		/* FIMC ISP */
		subdev = fimc_is_isp_get_sd(is, i);
		subdev->grp_id = GRP_ID_FIMC_IS;

		v4l2_set_subdev_hostdata(subdev, ep);
		ret = v4l2_device_register_subdev(&fmd->v4l2_dev, subdev);
		if (ret)
			v4l2_err(&fmd->v4l2_dev,
				"Failed to register ISP subdev\n");
		p->subdevs[IDX_ISP] = subdev;

		/* Create default links: vdev -> ISP */
		vdev = fimc_is_isp_get_vfd(is, i);
		ret = media_entity_create_link(&subdev->entity,
					ISP_SD_PAD_SINK_DMA,
					&vdev->entity, 0,
					MEDIA_LNK_FL_IMMUTABLE |
					MEDIA_LNK_FL_ENABLED);
		if (ret)
			return ret;

		/* FIMC SCC */
		subdev = fimc_is_scc_get_sd(is, i);
		subdev->grp_id = GRP_ID_FIMC_IS;
		v4l2_set_subdev_hostdata(subdev, ep);
		ret = v4l2_device_register_subdev(&fmd->v4l2_dev, subdev);
		if (ret)
			v4l2_err(&fmd->v4l2_dev,
					"Failed to register SCC subdev\n");
		p->subdevs[IDX_SCC] = subdev;
		/* Create default links: SCC -> vdev */
		vdev = fimc_is_scc_get_vfd(is, i);
		ret = media_entity_create_link(&subdev->entity,
					SCALER_SD_PAD_SRC_DMA,
					&vdev->entity, 0,
					MEDIA_LNK_FL_IMMUTABLE |
					MEDIA_LNK_FL_ENABLED);
		if (ret)
			return ret;

		/* Link: ISP -> SCC */
		ret = media_entity_create_link(&p->subdevs[IDX_ISP]->entity,
					ISP_SD_PAD_SRC,
					&subdev->entity, SCALER_SD_PAD_SINK,
					MEDIA_LNK_FL_IMMUTABLE |
					MEDIA_LNK_FL_ENABLED);
		if (ret)
			return ret;

		/* FIMC SCP */
                if (is->pipeline[i].subip_state[IS_SCP] != COMP_INVALID) {
                        subdev = fimc_is_scp_get_sd(is, i);
                        subdev->grp_id = GRP_ID_FIMC_IS;
                        v4l2_set_subdev_hostdata(subdev, ep);
                        ret = v4l2_device_register_subdev(&fmd->v4l2_dev, subdev);
                        if (ret)
                                v4l2_err(&fmd->v4l2_dev,
                                         "Failed to register SCP subdev\n");
                        p->subdevs[IDX_SCP] = subdev;
                        /* Create default links: SCP -> vdev */
                        vdev = fimc_is_scp_get_vfd(is, i);
                                ret = media_entity_create_link(&subdev->entity,
                                                SCALER_SD_PAD_SRC_DMA,
                                                &vdev->entity, 0,
                                                MEDIA_LNK_FL_IMMUTABLE |
                                                MEDIA_LNK_FL_ENABLED);
                                if (ret)
                                        return ret;

                        /* Link SCC -> SCP */
                        ret = media_entity_create_link(&p->subdevs[IDX_SCC]->entity,
					SCALER_SD_PAD_SRC_FIFO,
					&subdev->entity, SCALER_SD_PAD_SINK,
					MEDIA_LNK_FL_IMMUTABLE |
					MEDIA_LNK_FL_ENABLED);
                        if (ret)
                                return ret;

                }
	}
	fmd->is = is;

	return ret;
}

static int fimc_md_register_platform_entity(struct fimc_md *fmd,
					    struct device_node *node,
					    int plat_entity)
{
	struct platform_device *pdev = NULL;
	struct device *dev;
	int ret = -EPROBE_DEFER;
	void *drvdata;

	if (node) {
		pdev = of_find_device_by_node(node);
		if (!pdev)
			return -ENOENT;
	}
	dev = &pdev->dev;

	/* Lock to ensure dev->driver won't change. */
	device_lock(dev);

	if (!dev->driver || !try_module_get(dev->driver->owner))
		goto dev_unlock;

	drvdata = dev_get_drvdata(dev);
	/* Some subdev didn't probe succesfully id drvdata is NULL */
	if (drvdata) {
		switch (plat_entity) {
		case IDX_FLITE:
			ret = register_fimc_lite_entity(fmd, drvdata);
			break;
		case IDX_CSIS:
			ret = register_csis_entity(fmd, pdev, drvdata);
			break;
		case IDX_FIMC_IS:
			ret = register_fimc_is_entity(fmd, drvdata);
			break;
		default:
			ret = -ENODEV;
		}
	}

	module_put(dev->driver->owner);
dev_unlock:
	device_unlock(dev);
	if (ret == -EPROBE_DEFER)
		dev_info(fmd->dev, "deferring %s device registration\n",
			dev_name(dev));
	else if (ret < 0)
		dev_err(fmd->dev, "%s device registration failed (%d)\n",
			dev_name(dev), ret);
	if (pdev)
		put_device(&pdev->dev);

	return ret;
}

/* Register Exynos3250 FIMC-LITE and CSIS platform sub-devices */
static int __register_fimc_is_sub_entities(struct fimc_md *fmd)
{
	struct device_node *node;
	int ret = 0;

	for_each_available_child_of_node(fmd->dev->of_node, node) {
		int plat_entity = -1;

		if (!strcmp(node->name, "csis"))
			plat_entity = IDX_CSIS;
		else if (!strcmp(node->name, "fimc-lite"))
			plat_entity = IDX_FLITE;

		if (plat_entity < 0)
			continue;

		ret = fimc_md_register_platform_entity(fmd, node,
						       plat_entity);
		if (ret < 0)
			break;
	}

	return ret;
}

/* Register FIMC-LITE, CSIS and FIMC-IS media entities */
static int fimc_md_register_platform_entities(struct fimc_md *fmd)
{
	struct device_node *master = fmd->dev->of_node;
	struct device_node *node;
	int ret = 0;
	int i;

	if (of_device_is_compatible(master, "samsung,exynos3250-fimc-is")) {
		void *drvdata;

		ret = __register_fimc_is_sub_entities(fmd);
		if (ret < 0)
			return ret;

		drvdata = dev_get_drvdata(fmd->dev);
		if (!drvdata)
			return -EINVAL; /* this shouldn't happen */

		return register_fimc_is_entity(fmd, drvdata);
	} else {
		/* Register MIPI-CSIS entities */
		for (i = 0; i < FIMC_NUM_MIPI_CSIS; i++) {

			node = of_parse_phandle(master, "samsung,csis", i);
			if (!node || !of_device_is_available(node))
				continue;

			ret = fimc_md_register_platform_entity(fmd, node,
							       IDX_CSIS);
			if (ret < 0)
				break;
		}

		/* Register FIMC-LITE entities */
		for (i = 0; i < FIMC_NUM_FIMC_LITE; i++) {

			node = of_parse_phandle(master, "samsung,fimc-lite", i);
			if (!node || !of_device_is_available(node))
				continue;

			ret = fimc_md_register_platform_entity(fmd, node,
							       IDX_FLITE);
			if (ret < 0)
				break;
		}

		/* Register fimc-is entity */
		node = of_parse_phandle(master, "samsung,fimc-is", 0);
		if (!node || !of_device_is_available(node))
			return 0;

		return fimc_md_register_platform_entity(fmd, node, IDX_FIMC_IS);
	}
}

static void fimc_md_unregister_entities(struct fimc_md *fmd)
{
	int i;
	struct fimc_is *is;

	for (i = 0; i < FIMC_LITE_MAX_DEVS; i++) {
		if (fmd->fimc_lite[i] == NULL)
			continue;
		v4l2_device_unregister_subdev(&fmd->fimc_lite[i]->subdev);
		fmd->fimc_lite[i] = NULL;
	}
	for (i = 0; i < CSIS_MAX_ENTITIES; i++) {
		if (fmd->csis[i].sd == NULL)
			continue;
		v4l2_device_unregister_subdev(fmd->csis[i].sd);
		module_put(fmd->csis[i].sd->owner);
		fmd->csis[i].sd = NULL;
	}
	for (i = 0; i < fmd->num_sensors; i++)
		fmd->sensor[i].subdev = NULL;

	if (!fmd->is)
		return;
	/* Unregistering FIMC-IS entities */
	is = fmd->is;
	for (i = 0; i < is->drvdata->num_instances; i++) {
		struct v4l2_subdev *subdev;

		subdev = fimc_is_isp_get_sd(is, i);
		v4l2_device_unregister_subdev(subdev);

		subdev = fimc_is_scc_get_sd(is, i);
		v4l2_device_unregister_subdev(subdev);

                if (is->pipeline[i].subip_state[IS_SCP] != COMP_INVALID){
                        subdev = fimc_is_scp_get_sd(is, i);
                        v4l2_device_unregister_subdev(subdev);
                }
	}

	v4l2_info(&fmd->v4l2_dev, "Unregistered all entities\n");
}

/**
 * __fimc_md_create_fimc_links - create links to all FIMC entities
 * @fmd: fimc media device
 * @source: the source entity to create links to all fimc entities from
 * @sensor: sensor subdev linked to FIMC[fimc_id] entity, may be null
 * @pad: the source entity pad index
 * @link_mask: bitmask of the fimc devices for which link should be enabled
 */
static int __fimc_md_create_fimc_sink_links(struct fimc_md *fmd,
					    struct media_entity *source,
					    struct v4l2_subdev *sensor,
					    int pad, int link_mask)
{
	struct fimc_source_info *si = NULL;
	struct media_entity *sink;
	unsigned int flags = 0;
	int i, ret = 0;

	if (sensor) {
		si = v4l2_get_subdev_hostdata(sensor);
		/* Skip direct FIMC links in the logical FIMC-IS sensor path */
		if (si && si->fimc_bus_type == FIMC_BUS_TYPE_ISP_WRITEBACK)
			ret = 1;
	}

	for (i = 0; i < FIMC_LITE_MAX_DEVS; i++) {
		if (!fmd->fimc_lite[i])
			continue;

		flags = ((1 << i) & link_mask) ? MEDIA_LNK_FL_ENABLED : 0;

		sink = &fmd->fimc_lite[i]->subdev.entity;
		ret = media_entity_create_link(source, pad, sink,
					       FLITE_SD_PAD_SINK, flags);
		if (ret)
			return ret;

		/* Notify FIMC-LITE subdev entity */
		ret = media_entity_call(sink, link_setup, &sink->pads[0],
					&source->pads[pad], flags);
		if (ret)
			break;

		v4l2_info(&fmd->v4l2_dev, "created link [%s] -> [%s]\n",
			  source->name, sink->name);
	}
	return 0;
}

/* Create links from FIMC-LITE source pads to other entities */
static int __fimc_md_create_flite_source_links(struct fimc_md *fmd)
{
	struct media_entity *source, *sink;
	int i, ret = 0;

	for (i = 0; i < FIMC_LITE_MAX_DEVS; i++) {
		struct fimc_lite *fimc = fmd->fimc_lite[i];
		struct v4l2_subdev *isp_subdev;

		if (fimc == NULL)
			continue;

		source = &fimc->subdev.entity;
		sink = &fimc->ve.vdev.entity;
		/* FIMC-LITE's subdev and video node */
		ret = media_entity_create_link(source, FLITE_SD_PAD_SOURCE_DMA,
					       sink, 0,
					       MEDIA_LNK_FL_IMMUTABLE |
					       MEDIA_LNK_FL_ENABLED);
		if (ret)
			break;
		/* FIMC-LITE's subdev and FIMC ISP's */
		if (i >= fmd->is->drvdata->num_instances)
			continue;

		isp_subdev = fimc_is_isp_get_sd(fmd->is, i);
		sink = &isp_subdev->entity;
		ret = media_entity_create_link(source, FLITE_SD_PAD_SOURCE_ISP,
					       sink, ISP_SD_PAD_SINK_OTF,
					       MEDIA_LNK_FL_ENABLED);
		if (ret)
			break;
	}

	return ret;
}

/**
 * fimc_md_create_links - create default links between registered entities
 *
 * Parallel interface sensor entities are connected directly to FIMC capture
 * entities. The sensors using MIPI CSIS bus are connected through immutable
 * link with CSI receiver entity specified by mux_id. Any registered CSIS
 * entity has a link to each registered FIMC capture entity. Enabled links
 * are created by default between each subsequent registered sensor and
 * subsequent FIMC capture entity. The number of default active links is
 * determined by the number of available sensors or FIMC entities,
 * whichever is less.
 */
static int fimc_md_create_links(struct fimc_md *fmd)
{
	struct v4l2_subdev *csi_sensors[CSIS_MAX_ENTITIES] = { NULL };
	struct v4l2_subdev *sensor, *csis;
	struct fimc_source_info *pdata;
	struct media_entity *source;
	int i, pad, fimc_id = 0, ret = 0;
	u32 flags, link_mask = 0;

	for (i = 0; i < fmd->num_sensors; i++) {
		if (fmd->sensor[i].subdev == NULL)
			continue;

		sensor = fmd->sensor[i].subdev;
		pdata = v4l2_get_subdev_hostdata(sensor);
		if (!pdata)
			continue;

		source = NULL;

		switch (pdata->sensor_bus_type) {
		case FIMC_BUS_TYPE_MIPI_CSI2:
			if (WARN(pdata->mux_id >= CSIS_MAX_ENTITIES,
				"Wrong CSI channel id: %d\n", pdata->mux_id))
				return -EINVAL;

			csis = fmd->csis[pdata->mux_id].sd;
			if (WARN(csis == NULL,
				 "MIPI-CSI interface specified "
				 "but s5p-csis module is not loaded!\n"))
				return -EINVAL;

			pad = sensor->entity.num_pads - 1;
			ret = media_entity_create_link(&sensor->entity, pad,
					      &csis->entity, CSIS_PAD_SINK,
					      MEDIA_LNK_FL_IMMUTABLE |
					      MEDIA_LNK_FL_ENABLED);
			if (ret)
				return ret;

			v4l2_info(&fmd->v4l2_dev, "created link [%s] => [%s]\n",
				  sensor->entity.name, csis->entity.name);

			source = NULL;
			csi_sensors[pdata->mux_id] = sensor;
			break;

		case FIMC_BUS_TYPE_ITU_601...FIMC_BUS_TYPE_ITU_656:
			source = &sensor->entity;
			pad = 0;
			break;

		default:
			v4l2_err(&fmd->v4l2_dev, "Wrong bus_type: %x\n",
				 pdata->sensor_bus_type);
			return -EINVAL;
		}
		if (source == NULL)
			continue;

		link_mask = 1 << fimc_id++;
		ret = __fimc_md_create_fimc_sink_links(fmd, source, sensor,
						       pad, link_mask);
	}

	for (i = 0; i < CSIS_MAX_ENTITIES; i++) {
		if (fmd->csis[i].sd == NULL)
			continue;

		source = &fmd->csis[i].sd->entity;
		pad = CSIS_PAD_SOURCE;
		sensor = csi_sensors[i];

		link_mask = 1 << fimc_id++;
		ret = __fimc_md_create_fimc_sink_links(fmd, source, sensor,
						       pad, link_mask);
	}

	/*
	 * Create immutable links between each FIMC-LITE's subdev
	 * and video node
	 */
	flags = MEDIA_LNK_FL_IMMUTABLE | MEDIA_LNK_FL_ENABLED;

	ret = __fimc_md_create_flite_source_links(fmd);
	if (ret < 0)
		return ret;

	return 0;
}

static int __fimc_md_modify_pipeline(struct media_entity *entity, bool enable)
{
	struct exynos_video_entity *ve;
	struct fimc_pipeline *p;
	struct video_device *vdev;
	int ret;

	vdev = media_entity_to_video_device(entity);
	if (vdev->entity.use_count == 0)
		return 0;

	ve = vdev_to_exynos_video_entity(vdev);
	p = to_fimc_pipeline(ve->pipe);
	/*
	 * Nothing to do if we are disabling the pipeline, some link
	 * has been disconnected and p->subdevs array is cleared now.
	 */
	if (!enable && p->subdevs[IDX_SENSOR] == NULL)
		return 0;

	if (enable)
		ret = __fimc_pipeline_open(ve->pipe, entity, true);
	else
		ret = __fimc_pipeline_close(ve->pipe);

	if (ret == 0 && !enable)
		memset(p->subdevs, 0, sizeof(p->subdevs));

	return ret;
}

/* Locking: called with entity->parent->graph_mutex mutex held. */
static int __fimc_md_modify_pipelines(struct media_entity *entity, bool enable)
{
	struct media_entity *entity_err = entity;
	struct media_entity_graph graph;
	int ret;

	/*
	 * Walk current graph and call the pipeline open/close routine for each
	 * opened video node that belongs to the graph of entities connected
	 * through active links. This is needed as we cannot power on/off the
	 * subdevs in random order.
	 */
	media_entity_graph_walk_start(&graph, entity);

	while ((entity = media_entity_graph_walk_next(&graph))) {
		if (media_entity_type(entity) != MEDIA_ENT_T_DEVNODE)
			continue;

		ret  = __fimc_md_modify_pipeline(entity, enable);

		if (ret < 0)
			goto err;
	}

	return 0;
 err:
	media_entity_graph_walk_start(&graph, entity_err);

	while ((entity_err = media_entity_graph_walk_next(&graph))) {
		if (media_entity_type(entity_err) != MEDIA_ENT_T_DEVNODE)
			continue;

		__fimc_md_modify_pipeline(entity_err, !enable);

		if (entity_err == entity)
			break;
	}

	return ret;
}

static int fimc_md_link_notify(struct media_link *link, unsigned int flags,
				unsigned int notification)
{
	struct media_entity *sink = link->sink->entity;
	int ret = 0;

	/* Before link disconnection */
	if (notification == MEDIA_DEV_NOTIFY_PRE_LINK_CH) {
		if (!(flags & MEDIA_LNK_FL_ENABLED))
			ret = __fimc_md_modify_pipelines(sink, false);
		else
			; /* TODO: Link state change validation */
	/* After link activation */
	} else if (notification == MEDIA_DEV_NOTIFY_POST_LINK_CH &&
		   (link->flags & MEDIA_LNK_FL_ENABLED)) {
		ret = __fimc_md_modify_pipelines(sink, true);
	}

	return ret ? -EPIPE : 0;
}

static int subdev_notifier_bound(struct v4l2_async_notifier *notifier,
				 struct v4l2_subdev *subdev,
				 struct v4l2_async_subdev *asd)
{
	struct fimc_md *fmd = notifier_to_fimc_md(notifier);
	struct fimc_sensor_info *si = NULL;
	int i;

	/* Find platform data for this sensor subdev */
	for (i = 0; i < ARRAY_SIZE(fmd->sensor); i++) {
		if (fmd->sensor[i].asd.match.of.node == subdev->dev->of_node)
			si = &fmd->sensor[i];
	}

	if (si == NULL)
		return -EINVAL;

	v4l2_set_subdev_hostdata(subdev, &si->pdata);

	if (si->pdata.fimc_bus_type == FIMC_BUS_TYPE_ISP_WRITEBACK)
		subdev->grp_id = GRP_ID_FIMC_IS_SENSOR;
	else
		subdev->grp_id = GRP_ID_SENSOR;

	si->subdev = subdev;

	v4l2_info(&fmd->v4l2_dev, "Registered sensor subdevice: %s (%d)\n",
		  subdev->name, fmd->num_sensors);

	fmd->num_sensors++;

	return 0;
}

static int subdev_notifier_complete(struct v4l2_async_notifier *notifier)
{
	struct fimc_md *fmd = notifier_to_fimc_md(notifier);
	int ret;

	mutex_lock(&fmd->media_dev.graph_mutex);

	ret = fimc_md_create_links(fmd);
	if (ret < 0)
		goto unlock;

	ret = v4l2_device_register_subdev_nodes(&fmd->v4l2_dev);
unlock:
	mutex_unlock(&fmd->media_dev.graph_mutex);
	return ret;
}

void fimc_md_capture_event_handler(struct v4l2_subdev *subdev, unsigned int event_id,
                                   void *arg)
{
        struct fimc_md *fmd;

        fmd = entity_to_fimc_mdev(&subdev->entity);
        fimc_is_dispatch_events(fmd->is, event_id, arg);
}

int exynos_camera_register(struct device *dev, struct fimc_md **md)
{
	struct v4l2_device *v4l2_dev;
	struct fimc_md *fmd;
	int ret;

	fmd = devm_kzalloc(dev, sizeof(*fmd), GFP_KERNEL);
	if (!fmd)
		return -ENOMEM;

	spin_lock_init(&fmd->slock);
	fmd->dev = dev;
	INIT_LIST_HEAD(&fmd->pipelines);
	INIT_LIST_HEAD(&fmd->isp_pipelines);

	strlcpy(fmd->media_dev.model, "SAMSUNG EXYNOS5 IS",
		sizeof(fmd->media_dev.model));

	fmd->media_dev.link_notify = fimc_md_link_notify;
	fmd->media_dev.dev = dev;

	v4l2_dev = &fmd->v4l2_dev;
	v4l2_dev->mdev = &fmd->media_dev;
        v4l2_dev->notify = fimc_md_capture_event_handler;
	strlcpy(v4l2_dev->name, "exynos5-fimc-md", sizeof(v4l2_dev->name));

	ret = v4l2_device_register(dev, v4l2_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Failed to register v4l2_device: %d\n", ret);
		return ret;
	}

	ret = media_device_register(&fmd->media_dev);
	if (ret < 0) {
		v4l2_err(v4l2_dev, "Failed to register media dev: %d\n", ret);
		goto err_md;
	}

	fmd->clk_bayer = clk_get(dev, BAYER_CLK_NAME);
	if (IS_ERR(fmd->clk_bayer))
		v4l2_info(v4l2_dev, "Couldn't get clk: " BAYER_CLK_NAME "\n");

	/* Protect the media graph while we're registering entities */
	mutex_lock(&fmd->media_dev.graph_mutex);

	ret = fimc_md_register_platform_entities(fmd);
	if (ret)
		goto err_unlock;

	fmd->num_sensors = 0;
	ret = fimc_md_of_sensors_register(fmd, dev->of_node);
	if (ret)
		goto err_unlock;

	mutex_unlock(&fmd->media_dev.graph_mutex);

	fmd->subdev_notifier.subdevs = fmd->async_subdevs;
	fmd->subdev_notifier.num_subdevs = fmd->num_sensors;
	fmd->subdev_notifier.bound = subdev_notifier_bound;
	fmd->subdev_notifier.complete = subdev_notifier_complete;
	fmd->num_sensors = 0;

	ret = v4l2_async_notifier_register(v4l2_dev, &fmd->subdev_notifier);
	if (ret)
		goto err_clk;

	*md = fmd;
	return 0;

err_unlock:
	mutex_unlock(&fmd->media_dev.graph_mutex);
err_clk:
	if (!IS_ERR(fmd->clk_bayer))
		clk_put(fmd->clk_bayer);
	fimc_md_unregister_entities(fmd);
	media_device_unregister(&fmd->media_dev);
err_md:
	v4l2_device_unregister(&fmd->v4l2_dev);
	return ret;
}

int exynos_camera_unregister(struct fimc_md *fmd)
{
	if (!fmd)
		return 0;

	v4l2_async_notifier_unregister(&fmd->subdev_notifier);

	fimc_md_unregister_entities(fmd);
	fimc_md_pipelines_free(fmd);
	media_device_unregister(&fmd->media_dev);

	if (!IS_ERR(fmd->clk_bayer))
		clk_put(fmd->clk_bayer);

	return 0;
}

static int exynos_camera_probe(struct platform_device *pdev)
{
	struct fimc_md *fmd = NULL;
	int ret;

	ret = exynos_camera_register(&pdev->dev, &fmd);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, fmd);
	return 0;
}

static int exynos_camera_remove(struct platform_device *pdev)
{
	struct fimc_md *fmd = platform_get_drvdata(pdev);

	return exynos_camera_unregister(fmd);
}

static const struct of_device_id exynos_camera_of_match[] = {
	{ .compatible = "samsung,exynos5250-fimc" },
	{ },
};
MODULE_DEVICE_TABLE(of, fimc_md_of_match);

struct platform_driver exynos_camera_driver = {
	.probe		= exynos_camera_probe,
	.remove		= exynos_camera_remove,
	.driver = {
		.of_match_table = exynos_camera_of_match,
		.name		= "exynos-camera",
		.owner		= THIS_MODULE,
	}
};

static int __init exynos_camera_init(void)
{
	int ret;

	request_module("s5p-csis");

	ret = fimc_is_init();
	if (ret < 0)
		return ret;

	ret = platform_driver_register(&exynos_camera_driver);

	if (ret < 0)
		fimc_is_cleanup();

	return ret;
}

static void __exit exynos_camera_exit(void)
{
	platform_driver_unregister(&exynos_camera_driver);
	fimc_is_cleanup();
}

module_init(exynos_camera_init);
module_exit(exynos_camera_exit);

MODULE_AUTHOR("Arun Kumar K <arun.kk@samsung.com>");
MODULE_AUTHOR("Shaik Ameer Basha <shaik.ameer@samsung.com>");
MODULE_DESCRIPTION("Samsung Exynos5/3 SoC series camera subsystem driver");
MODULE_LICENSE("GPL v2");
