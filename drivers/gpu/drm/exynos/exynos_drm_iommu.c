/* exynos_drm_iommu.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 * Author: Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <drmP.h>
#include <drm/exynos_drm.h>

#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/kref.h>

#ifdef CONFIG_ARM_DMA_USE_IOMMU
#include <asm/dma-iommu.h>
#else
#include <linux/dma-iommu.h>
#endif

#include "exynos_drm_drv.h"
#include "exynos_drm_iommu.h"

/*
 * drm_create_iommu_mapping - create a mapping structure
 *
 * @drm_dev: DRM device
 */
int drm_create_iommu_mapping(struct drm_device *drm_dev)
{
	void *mapping = NULL;
	struct exynos_drm_private *priv = drm_dev->dev_private;
	struct device *dev = drm_dev->dev;

	if (!priv->da_start)
		priv->da_start = EXYNOS_DEV_ADDR_START;
	if (!priv->da_space_size)
		priv->da_space_size = EXYNOS_DEV_ADDR_SIZE;

#ifdef CONFIG_ARM_DMA_USE_IOMMU
	mapping = arm_iommu_create_mapping(&platform_bus_type, priv->da_start,
						priv->da_space_size);
#else
	mapping = iommu_dma_create_domain((struct iommu_ops *)platform_bus_type.iommu_ops, priv->da_start, priv->da_space_size);
#endif

	if (IS_ERR(mapping))
		return PTR_ERR(mapping);

	dev->dma_parms = devm_kzalloc(dev, sizeof(*dev->dma_parms),
					GFP_KERNEL);
	if (!dev->dma_parms)
		goto error;

	dma_set_max_seg_size(dev, 0xffffffffu);
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	if (!dev->dma_mask)
		dev->dma_mask = &dev->coherent_dma_mask;
#ifdef CONFIG_ARM_DMA_USE_IOMMU
	dev->archdata.mapping = mapping;
#else
	set_dma_domain(dev, mapping);
	dev->archdata.dma_ops = &iommu_dma_ops;
#endif

	return 0;
error:
#ifdef CONFIG_ARM_DMA_USE_IOMMU
	arm_iommu_release_mapping(mapping);
#else
	iommu_dma_release_domain(mapping);
#endif
	return -ENOMEM;
}

/*
 * drm_release_iommu_mapping - release iommu mapping structure
 *
 * @drm_dev: DRM device
 *
 * if mapping->kref becomes 0 then all things related to iommu mapping
 * will be released
 */
void drm_release_iommu_mapping(struct drm_device *drm_dev)
{
	struct device *dev = drm_dev->dev;

#ifdef CONFIG_ARM_DMA_USE_IOMMU
	arm_iommu_release_mapping(dev->archdata.mapping);
#else
	iommu_dma_release_domain(dev->archdata.dma_domain);
#endif
}

/*
 * drm_iommu_attach_device- attach device to iommu mapping
 *
 * @drm_dev: DRM device
 * @subdrv_dev: device to be attach
 *
 * This function should be called by sub drivers to attach it to iommu
 * mapping.
 */
int drm_iommu_attach_device(struct drm_device *drm_dev,
				struct device *subdrv_dev)
{
	struct device *dev = drm_dev->dev;
	int ret;

#ifdef CONFIG_ARM_DMA_USE_IOMMU
	if (!dev->archdata.mapping) {
#else
	if (!dev->archdata.dma_domain) {
#endif
		DRM_ERROR("iommu_mapping is null.\n");
		return -EFAULT;
	}

	subdrv_dev->dma_parms = devm_kzalloc(subdrv_dev,
					sizeof(*subdrv_dev->dma_parms),
					GFP_KERNEL);
	if (!subdrv_dev->dma_parms)
		return -ENOMEM;

	dma_set_max_seg_size(subdrv_dev, 0xffffffffu);

#ifdef CONFIG_ARM_DMA_USE_IOMMU
	if (subdrv_dev->archdata.mapping)
		arm_iommu_detach_device(subdrv_dev);

	ret = arm_iommu_attach_device(subdrv_dev, dev->archdata.mapping);
#else
	if (subdrv_dev->archdata.dma_domain)
		iommu_dma_detach_device(subdrv_dev);

	ret = iommu_dma_attach_device(subdrv_dev, dev->archdata.dma_domain);
#endif
	if (ret < 0) {
		DRM_DEBUG_KMS("failed iommu attach.\n");
		return ret;
	}

#ifdef CONFIG_ARM_DMA_USE_IOMMU
	/*
	 * Set dma_ops to drm_device just one time.
	 *
	 * The dma mapping api needs device object and the api is used
	 * to allocate physial memory and map it with iommu table.
	 * If iommu attach succeeded, the sub driver would have dma_ops
	 * for iommu and also all sub drivers have same dma_ops.
	 */
	if (!dev->archdata.dma_ops)
		dev->archdata.dma_ops = subdrv_dev->archdata.dma_ops;
#endif

	return 0;
}

/*
 * drm_iommu_detach_device -detach device address space mapping from device
 *
 * @drm_dev: DRM device
 * @subdrv_dev: device to be detached
 *
 * This function should be called by sub drivers to detach it from iommu
 * mapping
 */
void drm_iommu_detach_device(struct drm_device *drm_dev,
				struct device *subdrv_dev)
{
	struct device *dev = drm_dev->dev;
#ifdef CONFIG_ARM_DMA_USE_IOMMU
	struct dma_iommu_mapping *mapping = dev->archdata.mapping;

	if (!mapping || !mapping->domain)
		return;

	iommu_detach_device(mapping->domain, subdrv_dev);
#else
	void *mapping = get_dma_domain(dev);

	if (!mapping)
		return;

	iommu_dma_detach_device(subdrv_dev);
#endif
	drm_release_iommu_mapping(drm_dev);
}
