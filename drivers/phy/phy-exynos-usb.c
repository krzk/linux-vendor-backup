/*
 * Samsung S5P/EXYNOS SoC series USB PHY driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Author: Kamil Debski <k.debski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include "phy-exynos-usb.h"

static int exynos_uphy_power_on(struct phy *phy)
{
	struct uphy_instance *inst = phy_get_drvdata(phy);
	struct uphy_driver *drv = inst->drv;
	int ret;

	dev_info(drv->dev, "Request to power_on \"%s\" usb phy\n",
							inst->cfg->label);
	ret = clk_prepare_enable(drv->clk);
	if (ret)
		return ret;
	if (inst->cfg->power_on) {
		spin_lock(&drv->lock);
		ret = inst->cfg->power_on(inst);
		spin_unlock(&drv->lock);
	}
	clk_disable_unprepare(drv->clk);
	return ret;
}

static int exynos_uphy_power_off(struct phy *phy)
{
	struct uphy_instance *inst = phy_get_drvdata(phy);
	struct uphy_driver *drv = inst->drv;
	int ret;

	dev_info(drv->dev, "Request to power_off \"%s\" usb phy\n",
							inst->cfg->label);
	ret = clk_prepare_enable(drv->clk);
	if (ret)
		return ret;
	if (inst->cfg->power_off) {
		spin_lock(&drv->lock);
		ret = inst->cfg->power_off(inst);
		spin_unlock(&drv->lock);
	}
	clk_disable_unprepare(drv->clk);
	return ret;
}

static struct phy_ops exynos_uphy_ops = {
	.power_on	= exynos_uphy_power_on,
	.power_off	= exynos_uphy_power_off,
	.owner		= THIS_MODULE,
};

static struct phy *exynos_uphy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct uphy_driver *drv;

	drv = dev_get_drvdata(dev);
	if (!drv)
		return ERR_PTR(-EINVAL);

	if (WARN_ON(args->args[0] >= drv->cfg->num_phys))
		return ERR_PTR(-ENODEV);

	return drv->uphy_instances[args->args[0]].phy;
}

static const struct of_device_id exynos_uphy_of_match[];

static int exynos_uphy_probe(struct platform_device *pdev)
{
	struct uphy_driver *drv;
	struct device *dev = &pdev->dev;
	struct resource *mem;
	struct phy_provider *phy_provider;

	const struct of_device_id *match;
	const struct uphy_config *cfg;
	struct clk *clk;

	int i;

	match = of_match_node(exynos_uphy_of_match, pdev->dev.of_node);
	if (!match) {
		dev_err(dev, "of_match_node() failed\n");
		return -EINVAL;
	}
	cfg = match->data;
	if (!cfg) {
		dev_err(dev, "Failed to get configuration\n");
		return -EINVAL;
	}

	drv = devm_kzalloc(dev, sizeof(struct uphy_driver) +
		cfg->num_phys * sizeof(struct uphy_instance), GFP_KERNEL);

	if (!drv) {
		dev_err(dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	dev_set_drvdata(dev, drv);
	spin_lock_init(&drv->lock);

	drv->cfg = cfg;
	drv->dev = dev;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	drv->reg_phy = devm_ioremap_resource(dev, mem);
	if (IS_ERR(drv->reg_phy)) {
		dev_err(dev, "Failed to map register memory (phy)\n");
		return PTR_ERR(drv->reg_phy);
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	drv->reg_isol = devm_ioremap_resource(dev, mem);
	if (IS_ERR(drv->reg_isol)) {
		dev_err(dev, "Failed to map register memory (isolation)\n");
		return PTR_ERR(drv->reg_isol);
	}

	switch (drv->cfg->cpu) {
	case TYPE_EXYNOS4210:
	case TYPE_EXYNOS4212:
		mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		drv->reg_mode = devm_ioremap_resource(dev, mem);
		if (IS_ERR(drv->reg_mode)) {
			dev_err(dev, "Failed to map register memory (mode switch)\n");
			return PTR_ERR(drv->reg_mode);
		}
		break;
	default:
		break;
	}

	phy_provider = devm_of_phy_provider_register(dev,
							exynos_uphy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(drv->dev, "Failed to register phy provider\n");
		return PTR_ERR(phy_provider);
	}

	drv->clk = devm_clk_get(dev, "phy");
	if (IS_ERR(drv->clk)) {
		dev_err(dev, "Failed to get clock of phy controller\n");
		return PTR_ERR(drv->clk);
	}

	for (i = 0; i < drv->cfg->num_phys; i++) {
		char *label = drv->cfg->phys[i].label;
		struct uphy_instance *p = &drv->uphy_instances[i];

		dev_info(dev, "Creating phy \"%s\"\n", label);
		p->phy = devm_phy_create(dev, &exynos_uphy_ops, NULL);
		if (IS_ERR(p->phy)) {
			dev_err(drv->dev, "Failed to create uphy \"%s\"\n",
						label);
			return PTR_ERR(p->phy);
		}

		p->cfg = &drv->cfg->phys[i];
		p->drv = drv;
		phy_set_drvdata(p->phy, p);

		clk = clk_get(dev, p->cfg->label);
		if (IS_ERR(clk)) {
			dev_err(dev, "Failed to get clock of \"%s\" phy\n",
								p->cfg->label);
			return PTR_ERR(drv->clk);
		}

		p->rate = clk_get_rate(clk);

		if (p->cfg->rate_to_clk) {
			p->clk = p->cfg->rate_to_clk(p->rate);
			if (p->clk == CLKSEL_ERROR) {
				dev_err(dev, "Clock rate (%ld) not supported\n",
								p->rate);
				clk_put(clk);
				return -EINVAL;
			}
		}
		clk_put(clk);
	}

	return 0;
}

#ifdef CONFIG_PHY_EXYNOS4210_USB
extern const struct uphy_config exynos4210_uphy_config;
#endif

#ifdef CONFIG_PHY_EXYNOS4212_USB
extern const struct uphy_config exynos4212_uphy_config;
#endif

static const struct of_device_id exynos_uphy_of_match[] = {
#ifdef CONFIG_PHY_EXYNOS4210_USB
	{
		.compatible = "samsung,exynos4210-usbphy",
		.data = &exynos4210_uphy_config,
	},
#endif
#ifdef CONFIG_PHY_EXYNOS4212_USB
	{
		.compatible = "samsung,exynos4212-usbphy",
		.data = &exynos4212_uphy_config,
	},
#endif
	{ },
};

static struct platform_driver exynos_uphy_driver = {
	.probe	= exynos_uphy_probe,
	.driver = {
		.of_match_table	= exynos_uphy_of_match,
		.name		= "exynos-usbphy-new",
		.owner		= THIS_MODULE,
	}
};

module_platform_driver(exynos_uphy_driver);
MODULE_DESCRIPTION("Samsung S5P/EXYNOS SoC USB PHY driver");
MODULE_AUTHOR("Kamil Debski <k.debski@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos-uphy-new");

