/*
 * phy.h -- generic phy header file
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __DRIVERS_PHY_H
#define __DRIVERS_PHY_H

#include <linux/err.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>

struct phy;

/**
 * struct phy_ops - set of function pointers for performing phy operations
 * @init: operation to be performed for initializing phy
 * @exit: operation to be performed while exiting
 * @power_on: powering on the phy
 * @power_off: powering off the phy
 * @owner: the module owner containing the ops
 */
struct phy_ops {
	int	(*init)(struct phy *phy);
	int	(*exit)(struct phy *phy);
	int	(*power_on)(struct phy *phy);
	int	(*power_off)(struct phy *phy);
	struct module *owner;
};

/**
 * struct phy - represents the phy device
 * @dev: phy device
 * @id: id of the phy
 * @ops: function pointers for performing phy operations
 * @label: label given to the phy
 * @mutex: mutex to protect phy_ops
 * @init_count: used to protect when the PHY is used by multiple consumers
 * @power_count: used to protect when the PHY is used by multiple consumers
 */
struct phy {
	struct device		dev;
	int			id;
	const struct phy_ops	*ops;
	const char		*label;
	struct mutex		mutex;
	int			init_count;
	int			power_count;
};

/**
 * struct phy_provider - represents the phy provider
 * @dev: phy provider device
 * @owner: the module owner having of_xlate
 * @of_xlate: function pointer to obtain phy instance from phy pointer
 * @list: to maintain a linked list of PHY providers
 */
struct phy_provider {
	struct device		*dev;
	struct module		*owner;
	struct list_head	list;
	struct phy * (*of_xlate)(struct device *dev,
		struct of_phandle_args *args);
};

#define	to_phy(dev)	(container_of((dev), struct phy, dev))

#define	of_phy_provider_register(dev, xlate)	\
	__of_phy_provider_register((dev), THIS_MODULE, (xlate))

#define	devm_of_phy_provider_register(dev, xlate)	\
	__of_phy_provider_register((dev), THIS_MODULE, (xlate))

static inline void phy_set_drvdata(struct phy *phy, void *data)
{
	dev_set_drvdata(&phy->dev, data);
}

static inline void *phy_get_drvdata(struct phy *phy)
{
	return dev_get_drvdata(&phy->dev);
}

#if IS_ENABLED(CONFIG_GENERIC_PHY)
extern struct phy *phy_get(struct device *dev, const char *string);
extern struct phy *devm_phy_get(struct device *dev, const char *string);
extern void phy_put(struct phy *phy);
extern void devm_phy_put(struct device *dev, struct phy *phy);
extern struct phy *of_phy_simple_xlate(struct device *dev,
	struct of_phandle_args *args);
extern struct phy *phy_create(struct device *dev, u8 id,
	const struct phy_ops *ops, const char *label);
extern struct phy *devm_phy_create(struct device *dev, u8 id,
	const struct phy_ops *ops, const char *label);
extern void phy_destroy(struct phy *phy);
extern void devm_phy_destroy(struct device *dev, struct phy *phy);
extern struct phy_provider *__of_phy_provider_register(struct device *dev,
	struct module *owner, struct phy * (*of_xlate)(struct device *dev,
	struct of_phandle_args *args));
extern struct phy_provider *__devm_of_phy_provider_register(struct device *dev,
	struct module *owner, struct phy * (*of_xlate)(struct device *dev,
	struct of_phandle_args *args));
extern void of_phy_provider_unregister(struct phy_provider *phy_provider);
extern void devm_of_phy_provider_unregister(struct device *dev,
	struct phy_provider *phy_provider);
#else
static inline struct phy *phy_get(struct device *dev, const char *string)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *devm_phy_get(struct device *dev, const char *string)
{
	return ERR_PTR(-ENOSYS);
}

static inline void phy_put(struct phy *phy)
{
}

static inline void devm_phy_put(struct device *dev, struct phy *phy)
{
}

static inline struct phy *of_phy_simple_xlate(struct device *dev,
	struct of_phandle_args *args)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *phy_create(struct device *dev, u8 id,
	const struct phy_ops *ops, const char *label)
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy *devm_phy_create(struct device *dev, u8 id,
	const struct phy_ops *ops, const char *label)
{
	return ERR_PTR(-ENOSYS);
}

static inline void phy_destroy(struct phy *phy)
{
}

static inline void devm_phy_destroy(struct device *dev, struct phy *phy)
{
}

static inline struct phy_provider *__of_phy_provider_register(
	struct device *dev, struct module *owner, struct phy * (*of_xlate)(
	struct device *dev, struct of_phandle_args *args))
{
	return ERR_PTR(-ENOSYS);
}

static inline struct phy_provider *__devm_of_phy_provider_register(struct device
	*dev, struct module *owner, struct phy * (*of_xlate)(struct device *dev,
	struct of_phandle_args *args))
{
	return ERR_PTR(-ENOSYS);
}

static inline void of_phy_provider_unregister(struct phy_provider *phy_provider)
{
}

static inline void devm_of_phy_provider_unregister(struct device *dev,
	struct phy_provider *phy_provider)
{
}
#endif

static inline int phy_pm_runtime_get(struct phy *phy)
{
	if (WARN(IS_ERR(phy), "Invalid PHY reference\n"))
		return -EINVAL;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	return pm_runtime_get(&phy->dev);
}

static inline int phy_pm_runtime_get_sync(struct phy *phy)
{
	if (WARN(IS_ERR(phy), "Invalid PHY reference\n"))
		return -EINVAL;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	return pm_runtime_get_sync(&phy->dev);
}

static inline int phy_pm_runtime_put(struct phy *phy)
{
	if (WARN(IS_ERR(phy), "Invalid PHY reference\n"))
		return -EINVAL;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	return pm_runtime_put(&phy->dev);
}

static inline int phy_pm_runtime_put_sync(struct phy *phy)
{
	if (WARN(IS_ERR(phy), "Invalid PHY reference\n"))
		return -EINVAL;

	if (!pm_runtime_enabled(&phy->dev))
		return -ENOTSUPP;

	return pm_runtime_put_sync(&phy->dev);
}

static inline void phy_pm_runtime_allow(struct phy *phy)
{
	if (WARN(IS_ERR(phy), "Invalid PHY reference\n"))
		return;

	if (!pm_runtime_enabled(&phy->dev))
		return;

	pm_runtime_allow(&phy->dev);
}

static inline void phy_pm_runtime_forbid(struct phy *phy)
{
	if (WARN(IS_ERR(phy), "Invalid PHY reference\n"))
		return;

	if (!pm_runtime_enabled(&phy->dev))
		return;

	pm_runtime_forbid(&phy->dev);
}

static inline int phy_init(struct phy *phy)
{
	int ret;

	ret = phy_pm_runtime_get_sync(phy);
	if (ret < 0 && ret != -ENOTSUPP)
		return ret;

	mutex_lock(&phy->mutex);
	if (phy->init_count++ == 0 && phy->ops->init) {
		ret = phy->ops->init(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy init failed --> %d\n", ret);
			goto out;
		}
	}

out:
	mutex_unlock(&phy->mutex);
	phy_pm_runtime_put(phy);
	return ret;
}

static inline int phy_exit(struct phy *phy)
{
	int ret;

	ret = phy_pm_runtime_get_sync(phy);
	if (ret < 0 && ret != -ENOTSUPP)
		return ret;

	mutex_lock(&phy->mutex);
	if (--phy->init_count == 0 && phy->ops->exit) {
		ret = phy->ops->exit(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy exit failed --> %d\n", ret);
			goto out;
		}
	}

out:
	mutex_unlock(&phy->mutex);
	phy_pm_runtime_put(phy);
	return ret;
}

static inline int phy_power_on(struct phy *phy)
{
	int ret = -ENOTSUPP;

	ret = phy_pm_runtime_get_sync(phy);
	if (ret < 0 && ret != -ENOTSUPP)
		return ret;

	mutex_lock(&phy->mutex);
	if (phy->power_count++ == 0 && phy->ops->power_on) {
		ret = phy->ops->power_on(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy poweron failed --> %d\n", ret);
			goto out;
		}
	}

out:
	mutex_unlock(&phy->mutex);

	return ret;
}

static inline int phy_power_off(struct phy *phy)
{
	int ret = -ENOTSUPP;

	mutex_lock(&phy->mutex);
	if (--phy->power_count == 0 && phy->ops->power_off) {
		ret =  phy->ops->power_off(phy);
		if (ret < 0) {
			dev_err(&phy->dev, "phy poweroff failed --> %d\n", ret);
			goto out;
		}
	}

out:
	mutex_unlock(&phy->mutex);
	phy_pm_runtime_put(phy);

	return ret;
}

#endif /* __DRIVERS_PHY_H */
