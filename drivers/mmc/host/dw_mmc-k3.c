/*
 * Copyright (c) 2013 Linaro Ltd.
 * Copyright (c) 2013 Hisilicon Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"

#define DRIVER_NAME "dwmmc_k3"

enum dw_mci_k3_type {
	DW_MCI_TYPE_HI4511,
};

static struct dw_mci_k3_compatible {
	char				*compatible;
	enum dw_mci_k3_type		type;
} k3_compat[] = {
	{
		.compatible	= "hisilicon,hi4511-dw-mshc",
		.type		= DW_MCI_TYPE_HI4511,
	},
};

struct dw_mci_k3_priv_data {
	enum dw_mci_k3_type	type;
	int			old_timing;
	u32			id;
	u32			gpio_cd;
	u32			clken_reg;
	u32			clken_bit;
	u32			sam_sel_reg;
	u32			sam_sel_bit;
	u32			drv_sel_reg;
	u32			drv_sel_bit;
	u32			div_reg;
	u32			div_bit;
};

static void __iomem *pctrl;
static DEFINE_SPINLOCK(mmc_tuning_lock);
static int k3_tuning_config[][8][6] = {
	/* bus_clk, div, drv_sel, sam_sel_max, sam_sel_min, input_clk */
	{
		{180000000, 6, 6, 13, 13, 25000000},	/* 0: LEGACY 400k */
		{0},					/* 1: MMC_HS */
		{360000000, 6, 4, 2, 0, 50000000  },	/* 2: SD_HS */
		{180000000, 6, 4, 13, 13, 25000000},	/* 3: SDR12 */
		{360000000, 6, 4, 2, 0, 50000000  },	/* 4: SDR25 */
		{720000000, 6, 1, 9, 4, 100000000 },	/* 5: SDR50 */
		{0},					/* 6: SDR104 */
		{360000000, 7, 1, 3, 0, 50000000  },	/* 7: DDR50 */
	}, {
		{26000000, 1, 1, 3, 3, 13000000  },	/* 0: LEGACY 400k */
		{360000000, 6, 3, 3, 1, 50000000 },	/* 1: MMC_HS*/
		{0},					/* 2: SD_HS */
		{0},					/* 3: SDR12 */
		{26000000, 1, 1, 3, 3, 13000000	 },     /* 4: SDR25 */
		{360000000, 6, 3, 3, 1, 50000000 },	/* 5: SDR50 */
		{0},					/* 6: SDR104 */
		{720000000, 6, 4, 8, 4, 100000000},	/* 7: DDR50 */
	},
};

static void dw_mci_k3_set_timing(struct dw_mci_k3_priv_data *priv,
				int idx, int sam, int drv, int div)
{
	unsigned int clken_reg = priv->clken_reg;
	unsigned int clken_bit = priv->clken_bit;
	unsigned int sam_sel_reg = priv->sam_sel_reg;
	unsigned int sam_sel_bit = priv->sam_sel_bit;
	unsigned int drv_sel_reg = priv->drv_sel_reg;
	unsigned int drv_sel_bit = priv->drv_sel_bit;
	unsigned int div_reg = priv->div_reg;
	unsigned int div_bit = priv->div_bit;
	int i = 0;
	unsigned int temp_reg;
	unsigned long flags;

	spin_lock_irqsave(&mmc_tuning_lock, flags);

	/* disable clock */
	temp_reg = readl(pctrl + clken_reg);
	temp_reg &= ~(1<<clken_bit);
	writel(temp_reg, pctrl + clken_reg);

	temp_reg = readl(pctrl + sam_sel_reg);
	if (sam >= 0) {
		/* set sam delay */
		for (i = 0; i < 4; i++) {
			if (sam % 2)
				temp_reg |= 1<<(sam_sel_bit + i);
			else
				temp_reg &= ~(1<<(sam_sel_bit + i));
			sam = sam >> 1;
		}
	}
	writel(temp_reg, pctrl + sam_sel_reg);

	temp_reg = readl(pctrl + drv_sel_reg);
	if (drv >= 0) {
		/* set drv delay */
		for (i = 0; i < 4; i++) {
			if (drv % 2)
				temp_reg |= 1<<(drv_sel_bit + i);
			else
				temp_reg &= ~(1<<(drv_sel_bit + i));
			drv = drv >> 1;
		}
	}
	writel(temp_reg, pctrl + drv_sel_reg);

	temp_reg = readl(pctrl + div_reg);
	if (div >= 0) {
		/* set drv delay */
		for (i = 0; i < 3; i++) {
			if (div % 2)
				temp_reg |= 1<<(div_bit + i);
			else
				temp_reg &= ~(1<<(div_bit + i));
			div = div >> 1;
		}
	}
	writel(temp_reg, pctrl + div_reg);

	/* enable clock */
	temp_reg = readl(pctrl + clken_reg);
	temp_reg |= 1<<clken_bit;
	writel(temp_reg, pctrl + clken_reg);

	spin_unlock_irqrestore(&mmc_tuning_lock, flags);
}

static void dw_mci_k3_tun(struct dw_mci *host, int id, int index)
{
	struct dw_mci_k3_priv_data *priv = host->priv;
	int ret;

	if (!pctrl)
		return;

	if (priv->old_timing == index)
		return;

	ret = clk_set_rate(host->ciu_clk, k3_tuning_config[id][index][0]);
	if (ret)
		dev_err(host->dev, "clk_set_rate failed\n");

	dw_mci_k3_set_timing(priv, id,
			(k3_tuning_config[id][index][3] +
			k3_tuning_config[id][index][4]) / 2,
			k3_tuning_config[id][index][2],
			k3_tuning_config[id][index][1]);

	host->bus_hz = k3_tuning_config[id][index][5];
	priv->old_timing = index;
}

static void dw_mci_k3_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	struct dw_mci_k3_priv_data *priv = host->priv;
	int id = priv->id;

	if (priv->type == DW_MCI_TYPE_HI4511)
		dw_mci_k3_tun(host, id, ios->timing);
}

static int dw_mci_k3_setup_clock(struct dw_mci *host)
{
	struct dw_mci_k3_priv_data *priv = host->priv;

	if (priv->type == DW_MCI_TYPE_HI4511)
		dw_mci_k3_tun(host, priv->id, MMC_TIMING_LEGACY);

	return 0;
}


static irqreturn_t dw_mci_k3_card_detect(int irq, void *data)
{
	struct dw_mci *host = (struct dw_mci *)data;

	queue_work(host->card_workqueue, &host->card_work);
	return IRQ_HANDLED;
};

static int dw_mci_k3_get_cd(struct dw_mci *host, u32 slot_id)
{
	unsigned int status;
	struct dw_mci_k3_priv_data *priv = host->priv;

	status = !gpio_get_value(priv->gpio_cd);
	return status;
}

static int dw_mci_k3_parse_dt(struct dw_mci *host)
{
	struct dw_mci_k3_priv_data *priv;
	struct device_node *np = host->dev->of_node;
	u32 data[2];
	int ret, i;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(host->dev, "mem alloc failed for private data\n");
		return -ENOMEM;
	}
	priv->id = of_alias_get_id(host->dev->of_node, "mshc");
	priv->old_timing = -1;
	host->priv = priv;

	for (i = 0; i < ARRAY_SIZE(k3_compat); i++) {
		if (of_device_is_compatible(host->dev->of_node,
					k3_compat[i].compatible))
			priv->type = k3_compat[i].type;
	}

	if (priv->type == DW_MCI_TYPE_HI4511) {
		if (!pctrl) {
			struct device_node *node;

			node = of_find_compatible_node(NULL, NULL,
						"hisilicon,pctrl");
			pctrl = of_iomap(node, 0);
		}

		ret = of_property_read_u32_array(np,
				"clken-reg", data, 2);
		if (!ret) {
			priv->clken_reg = data[0];
			priv->clken_bit = data[1];
		}

		ret = of_property_read_u32_array(np,
				"drv-sel-reg", data, 2);
		if (!ret) {
			priv->drv_sel_reg = data[0];
			priv->drv_sel_bit = data[1];
		}

		ret = of_property_read_u32_array(np,
				"sam-sel-reg", data, 2);
		if (!ret) {
			priv->sam_sel_reg = data[0];
			priv->sam_sel_bit = data[1];
		}

		ret = of_property_read_u32_array(np,
				"div-reg", data, 2);
		if (!ret) {
			priv->div_reg = data[0];
			priv->div_bit = data[1];
		}
	}

	return 0;
}

static unsigned long k3_dwmmc_caps[4] = {
	MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED,
	MMC_CAP_8_BIT_DATA | MMC_CAP_MMC_HIGHSPEED,
	0,
	0,
};

static const struct dw_mci_drv_data k3_drv_data = {
	.caps			= k3_dwmmc_caps,
	.set_ios		= dw_mci_k3_set_ios,
	.setup_clock		= dw_mci_k3_setup_clock,
	.parse_dt		= dw_mci_k3_parse_dt,
};

static const struct of_device_id dw_mci_k3_match[] = {
	{ .compatible = "hisilicon,hi4511-dw-mshc",
			.data = &k3_drv_data, },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_k3_match);

int dw_mci_k3_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data;
	const struct of_device_id *match;
	struct dw_mci *host;
	int gpio, err;

	match = of_match_node(dw_mci_k3_match, pdev->dev.of_node);
	drv_data = match->data;

	err = dw_mci_pltfm_register(pdev, drv_data);
	if (err)
		return err;

	host = platform_get_drvdata(pdev);
	if (host->pdata->quirks & DW_MCI_QUIRK_BROKEN_CARD_DETECTION)
		return 0;

	gpio = of_get_named_gpio(pdev->dev.of_node, "cd-gpio", 0);
	if (gpio_is_valid(gpio)) {
		if (devm_gpio_request(host->dev, gpio, "dw-mci-cd")) {
			dev_err(host->dev, "gpio [%d] request failed\n", gpio);
		} else {
			struct dw_mci_k3_priv_data *priv = host->priv;
			priv->gpio_cd = gpio;
			host->pdata->get_cd = dw_mci_k3_get_cd;
			err = devm_request_irq(host->dev, gpio_to_irq(gpio),
				dw_mci_k3_card_detect,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				DRIVER_NAME, host);
			if (err)
				dev_warn(mmc_dev(host->dev), "request gpio irq error\n");
		}

	} else {
		dev_info(host->dev, "cd gpio not available");
	}
	return 0;
}

static int dw_mci_k3_suspend(struct device *dev)
{
	int ret;
	struct dw_mci *host = dev_get_drvdata(dev);

	ret = dw_mci_suspend(host);
	if (ret)
		return ret;

	return 0;
}

static int dw_mci_k3_resume(struct device *dev)
{
	int ret;
	struct dw_mci *host = dev_get_drvdata(dev);
	struct dw_mci_k3_priv_data *priv = host->priv;

	if (priv->type == DW_MCI_TYPE_HI4511) {
		int id = priv->id;

		priv->old_timing = -1;
		dw_mci_k3_tun(host, id, MMC_TIMING_LEGACY);
	}

	ret = dw_mci_resume(host);
	if (ret)
		return ret;

	return 0;
}

SIMPLE_DEV_PM_OPS(dw_mci_k3_pmops, dw_mci_k3_suspend, dw_mci_k3_resume);

static struct platform_driver dw_mci_k3_pltfm_driver = {
	.probe		= dw_mci_k3_probe,
	.remove		= dw_mci_pltfm_remove,
	.driver		= {
		.name		= DRIVER_NAME,
		.of_match_table	= dw_mci_k3_match,
		.pm		= &dw_mci_k3_pmops,
	},
};

module_platform_driver(dw_mci_k3_pltfm_driver);

MODULE_DESCRIPTION("K3 Specific DW-MSHC Driver Extension");
MODULE_LICENSE("GPL v2");
