/*
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
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
#include <linux/pm_runtime.h>

#include <drm/drmP.h>
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

/*
 * DBG supports debug infromation about exynos drm.
 * supports sysfs device attribute creating.
 * supports dump register of each device.
 */

#define MAX_DEV	10
#define MAX_REG	128

/*
 * A structure of fimc context.
 *
 * @drv_list: list head for registed sub driver information.
 * @dev: platform device.
 * @dev_attr: device created files for debugging.
 * @size: device created files size.
 * @regs: memory mapped io registers.
 * @regs_dump: memory dump of memory mapped io registers.
 */
struct exynos_dbg_drv {
	struct list_head drv_list;
	struct device *dev;
	struct device_attribute *dev_attr;
	int size;
	void __iomem	*regs;
	u32	*regs_dump;
};

/*
 * A structure of debug context.
 *
 * @nh_panic: atomic notifier header for panic.
 * @nb_panic: notifier block for panic.
 * @name: device name.
 * @regs_dump: stored register pointer for easy debuggin.
 */
struct dbg_context {
	struct atomic_notifier_head	*nh_panic;
	struct notifier_block	nb_panic;
	const char *name[MAX_DEV];
	u32	*regs_dump[MAX_DEV];
};

static LIST_HEAD(exynos_drm_dbg_list);
static DEFINE_MUTEX(exynos_drm_dbg_lock);

#define get_dbg_context(dev)	platform_get_drvdata(to_platform_device(dev))

int exynos_drm_dbg_register(struct device *dev,
		struct device_attribute *dev_attr,
		int size, void __iomem *regs)
{
	struct exynos_dbg_drv *dbgdrv;
	struct kobject *dev_kobj = &dev->kobj;

	DRM_DEBUG_KMS("%s:name[%s]\n", __func__, dev_kobj->name);

	dbgdrv = devm_kzalloc(dev, sizeof(*dbgdrv), GFP_KERNEL);
	if (!dbgdrv) {
		DRM_ERROR("failed to alloc memory.\n");
		return -ENOMEM;
	}

	dbgdrv->dev = dev;
	dbgdrv->dev_attr = dev_attr;
	dbgdrv->size = size;
	dbgdrv->regs = regs;
	dbgdrv->regs_dump = devm_kzalloc(dev, MAX_REG * sizeof(u32),
		GFP_KERNEL);

	mutex_lock(&exynos_drm_dbg_lock);
	list_add_tail(&dbgdrv->drv_list, &exynos_drm_dbg_list);
	mutex_unlock(&exynos_drm_dbg_lock);

	return 0;
}

int exynos_drm_dbg_unregister(struct device *dev,
		struct device_attribute *dev_attr)
{
	struct exynos_dbg_drv *dbgdrv, *tdbgdrv;

	DRM_DEBUG_KMS("%s\n", __func__);

	list_for_each_entry_safe(dbgdrv, tdbgdrv,
	    &exynos_drm_dbg_list, drv_list) {
		if (dbgdrv->dev == dev &&
		    dbgdrv->dev_attr == dev_attr) {
			mutex_lock(&exynos_drm_dbg_lock);
			list_del(&dbgdrv->drv_list);
			mutex_unlock(&exynos_drm_dbg_lock);
		}
	}

	return 0;
}

static int dbg_notifier_panic(struct notifier_block *this,
		unsigned long ununsed, void *panic_str)
{
	struct dbg_context *ctx = container_of(this,
		struct dbg_context, nb_panic);
	struct exynos_dbg_drv *dbgdrv;
	int count = 0;

	list_for_each_entry(dbgdrv, &exynos_drm_dbg_list, drv_list) {
		DRM_DEBUG_KMS("%s:count[%d]name[%s]\n", __func__,
			count, ctx->name[count]);
		count++;

		if (!pm_runtime_suspended(dbgdrv->dev))
			memcpy(dbgdrv->regs_dump, dbgdrv->regs,
				MAX_REG * sizeof(u32));
	}

	return 0;
}

static int dbg_dev_create_files(struct exynos_dbg_drv *dbgdrv)
{
	int ret = 0, i;

	DRM_DEBUG_KMS("%s\n", __func__);

	for (i = 0; i < dbgdrv->size; i++) {
		ret = device_create_file(dbgdrv->dev, &dbgdrv->dev_attr[i]);
		if (ret)
			break;
	}

	if (ret < 0)
		dev_err(dbgdrv->dev, "failed to add sysfs entries.\n");

	return ret;
}

static int __devinit dbg_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dbg_context *ctx;
	struct exynos_dbg_drv *dbgdrv;
	struct kobject *dev_kobj;
	int ret = 0, count = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	list_for_each_entry(dbgdrv, &exynos_drm_dbg_list, drv_list) {
		if (dbgdrv->dev_attr)
			dbg_dev_create_files(dbgdrv);

		dev_kobj = &dbgdrv->dev->kobj;
		ctx->name[count] = dev_kobj->name;
		ctx->regs_dump[count] = dbgdrv->regs_dump;
		count++;

		/*
		 * supports MAX_DEV for direct debugging with ramdump.
		 * but we can search dbgdrv from exynos_drm_dbg_list
		 * so, please ignore this error handling.
		 */
		if (count >= MAX_DEV)
			DRM_ERROR("no more space regs_dump.\n");
	}

#ifdef CONFIG_SEC_DEBUG
	/* ToDo: need to mapping sec_panic_notifier_list!*/
#else
	ctx->nh_panic = &panic_notifier_list;
#endif

	if (!ctx->nh_panic) {
		dev_err(dev, "could not find notifier_list.\n");
		goto err;
	}

	ctx->nb_panic.notifier_call = dbg_notifier_panic;
	ret = atomic_notifier_chain_register(ctx->nh_panic, &ctx->nb_panic);
	if (ret) {
		dev_err(dev, "could not register panic notify callback.\n");
		goto err;
	}

	platform_set_drvdata(pdev, ctx);

	dev_info(dev, "drm dbg registered successfully.\n");

	return 0;

err:
	devm_kfree(dev, ctx);
	return ret;
}

static int __devexit dbg_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dbg_context *ctx = get_dbg_context(dev);

	DRM_DEBUG_KMS("%s\n", __func__);

	atomic_notifier_chain_unregister(ctx->nh_panic, &ctx->nb_panic);

	return 0;
}

struct platform_driver dbg_driver = {
	.probe		= dbg_probe,
	.remove		= __devexit_p(dbg_remove),
	.driver		= {
		.name	= "exynos-drm-dbg",
		.owner	= THIS_MODULE,
	},
};
