/*
 * Exynos PHY driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Author: Tomasz Stanislawski <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>

#define EXYNOS_PHY_ENABLE	(1 << 0)

static int exynos_phy_power_on(struct phy *phy)
{
	void __iomem *reg = phy_get_drvdata(phy);
	u32 val;

	val = readl(reg);
	val |= EXYNOS_PHY_ENABLE;
	writel(val, reg);

	return 0;
}

static int exynos_phy_power_off(struct phy *phy)
{
	void __iomem *reg = phy_get_drvdata(phy);
	u32 val;

	val = readl(reg);
	val &= ~EXYNOS_PHY_ENABLE;
	writel(val, reg);

	return 0;
}

static struct phy_ops exynos_phy_ops = {
	.power_on	= exynos_phy_power_on,
	.power_off	= exynos_phy_power_off,
	.owner		= THIS_MODULE,
};

static const u32 exynos4210_offsets[] = {
	0x0700, /* HDMI_PHY */
	0x070C, /* DAC_PHY */
	0x0718, /* ADC_PHY */
	0x071C, /* PCIE_PHY */
	0x0720, /* SATA_PHY */
	~0, /* end mark */
};

static const u32 exynos4412_offsets[] = {
	0x0700, /* HDMI_PHY */
	0x0718, /* ADC_PHY */
	~0, /* end mark */
};

static const struct of_device_id exynos_phy_of_match[] = {
	{ .compatible = "exynos4210-phy", .data = exynos4210_offsets},
	{ .compatible = "exynos4412-phy", .data = exynos4412_offsets},
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_phy_of_match);

static struct phy *exynos_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct phy **phys = dev_get_drvdata(dev);
	int index = args->args[0];
	int i;

	/* verify if index is valid */
	for (i = 0; i <= index; ++i)
		if (!phys[i])
			return ERR_PTR(-ENODEV);

	return phys[index];
}

static int exynos_phy_probe(struct platform_device *pdev)
{
	const struct of_device_id *of_id = of_match_device(
		of_match_ptr(exynos_phy_of_match), &pdev->dev);
	const u32 *offsets = of_id->data;
	int count;
	struct device *dev = &pdev->dev;
	struct phy **phys;
	struct resource *res;
	void __iomem *regs;
	int i;
	struct phy_provider *phy_provider;

	/* count number of phys to create */
	for (count = 0; offsets[count] != ~0; ++count)
		;

	phys = devm_kzalloc(dev, (count + 1) * sizeof(phys[0]), GFP_KERNEL);
	if (!phys)
		return -ENOMEM;

	dev_set_drvdata(dev, phys);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	regs = devm_ioremap(dev, res->start, res->end - res->start);
	if (!regs) {
		dev_err(dev, "failed to ioremap registers\n");
		return -EFAULT;
	}

	/* NOTE: last entry in phys[] is NULL */
	for (i = 0; i < count; ++i) {
		phys[i] = devm_phy_create(dev, &exynos_phy_ops, NULL);
		if (IS_ERR(phys[i])) {
			dev_err(dev, "failed to create PHY %d\n", i);
			return PTR_ERR(phys[i]);
		}
		phy_set_drvdata(phys[i], regs + offsets[i]);
	}

	phy_provider = devm_of_phy_provider_register(dev, exynos_phy_xlate);
	if (IS_ERR(phy_provider)) {
		dev_err(dev, "failed to register PHY provider\n");
		return PTR_ERR(phy_provider);
	}

	dev_info(dev, "added %d phys\n", count);

	return 0;
}

static struct platform_driver exynos_phy_driver = {
	.probe	= exynos_phy_probe,
	.driver = {
		.of_match_table	= exynos_phy_of_match,
		.name  = "exynos-phy",
		.owner = THIS_MODULE,
	}
};
module_platform_driver(exynos_phy_driver);

MODULE_DESCRIPTION("Exynos PHY driver");
MODULE_AUTHOR("Tomasz Stanislawski <t.stanislaws@samsung.com>");
MODULE_LICENSE("GPL v2");
