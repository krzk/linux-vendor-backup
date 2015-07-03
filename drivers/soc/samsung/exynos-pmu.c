/*
 * Copyright (c) 2011-2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/soc/samsung/exynos-regs-pmu.h>
#include <linux/soc/samsung/exynos-pmu.h>

#include <asm/system_misc.h>
#include "exynos-pmu.h"

void __iomem *pmu_base_addr;

struct exynos_pmu_context {
	struct device *dev;
	int pwr_key;
	const struct exynos_pmu_data *pmu_data;
};

static struct exynos_pmu_context *pmu_context;

/********************************************************
 *		Reset, Power off control		*
 ********************************************************/
static inline void exynos_sw_reset(void)
{
	pmu_raw_writel(0x1, EXYNOS_SWRESET);
	pr_emerg("%s: waiting for reboot\n", __func__);
	while (1);
}

static void exynos_reboot(enum reboot_mode mode, const char *cmd)
{
	local_irq_disable();

	pr_debug("%s (%d, %s)\n", __func__, mode, cmd ? cmd : "(null)");

	/* Don't enter lpm mode */
	pmu_raw_writel(0x12345678, S5P_INFORM2);

	if (!cmd) {
		pmu_raw_writel(REBOOT_MODE_PREFIX |
				REBOOT_MODE_NONE, S5P_INFORM3);
	} else {
		unsigned long value;
		if (!strcmp(cmd, "fota"))
			pmu_raw_writel(REBOOT_MODE_PREFIX | REBOOT_MODE_FOTA,
			       S5P_INFORM3);
		else if (!strcmp(cmd, "fota_bl"))
			pmu_raw_writel(REBOOT_MODE_PREFIX |
					REBOOT_MODE_FOTA_BL, S5P_INFORM3);
		else if (!strcmp(cmd, "recovery"))
			pmu_raw_writel(REBOOT_MODE_PREFIX |
					REBOOT_MODE_RECOVERY, S5P_INFORM3);
		else if (!strcmp(cmd, "download"))
			pmu_raw_writel(REBOOT_MODE_PREFIX |
					REBOOT_MODE_DOWNLOAD, S5P_INFORM3);
		else if (!strcmp(cmd, "upload"))
			pmu_raw_writel(REBOOT_MODE_PREFIX |
					REBOOT_MODE_UPLOAD, S5P_INFORM3);
		else if (!strcmp(cmd, "secure"))
			pmu_raw_writel(REBOOT_MODE_PREFIX |
					REBOOT_MODE_SECURE, S5P_INFORM3);
		else if (!strncmp(cmd, "debug", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			pmu_raw_writel(REBOOT_SET_PREFIX |
					REBOOT_SET_DEBUG | value, S5P_INFORM3);
		else if (!strncmp(cmd, "swsel", 5)
			 && !kstrtoul(cmd + 5, 0, &value))
			pmu_raw_writel(REBOOT_SET_PREFIX |
					REBOOT_SET_SWSEL | value, S5P_INFORM3);
		else if (!strncmp(cmd, "sud", 3)
			 && !kstrtoul(cmd + 3, 0, &value))
			pmu_raw_writel(REBOOT_SET_PREFIX |
					REBOOT_SET_SUD | value, S5P_INFORM3);
		else if (!strncmp(cmd, "emergency", 9))
			pmu_raw_writel(0, S5P_INFORM3);
		else
			pmu_raw_writel(REBOOT_MODE_PREFIX |
					REBOOT_MODE_NONE, S5P_INFORM3);
	}

	exynos_sw_reset();
}

static void exynos_power_off(void)
{
	int poweroff_try = 0;
	int pwr_key = of_get_named_gpio(pmu_context->dev->of_node,
					"power-key-gpio", 0);

	while (poweroff_try < 5) {
		/* Check power button and try to power off 5 times */
		if (!pwr_key ||	(pwr_key && gpio_get_value(pwr_key))) {
			pr_emerg("%s: Try to power off\n", __func__);

			/* Power off :
			 * Set PS_HOLD to low (BIT(9) of PS_HOLD_CONTROL)
			 */
			pmu_raw_writel((pmu_raw_readl(S5P_PS_HOLD_CONTROL) &
					0xFFFFFEFF), S5P_PS_HOLD_CONTROL);

			pr_emerg("%s: Failed to power off, retry it. (%d)\n",
				__func__, poweroff_try++);
		} else {
			pr_info("%s: PowerButton is not released.\n", __func__);
		}
		mdelay(1000);
	}

	exynos_sw_reset();
}

void exynos_sys_powerdown_conf(enum sys_powerdown mode)
{
	unsigned int i;

	const struct exynos_pmu_data *pmu_data = pmu_context->pmu_data;

	if (pmu_data->powerdown_conf)
		pmu_data->powerdown_conf(mode);

	if (pmu_data->pmu_config) {
		for (i = 0; (pmu_data->pmu_config[i].offset != PMU_TABLE_END); i++)
			pmu_raw_writel(pmu_data->pmu_config[i].val[mode],
					pmu_data->pmu_config[i].offset);
	}

	if (pmu_data->powerdown_conf_extra)
		pmu_data->powerdown_conf_extra(mode);

	if (pmu_data->pmu_config_extra) {
		for (i = 0; pmu_data->pmu_config_extra[i].offset != PMU_TABLE_END; i++)
			pmu_raw_writel(pmu_data->pmu_config_extra[i].val[mode],
					pmu_data->pmu_config_extra[i].offset);
	}
}

void exynos_sys_powerup_conf(enum sys_powerdown mode)
{
	const struct exynos_pmu_data *pmu_data = pmu_context->pmu_data;

	if (pmu_data->powerup_conf)
		pmu_data->powerup_conf(mode);
}

/*
 * PMU platform driver and devicetree bindings.
 */
static const struct of_device_id exynos_pmu_of_device_ids[] = {
	{
		.compatible = "samsung,exynos5433-pmu",
		.data = &exynos5433_pmu_data,
	},
	{ /*sentinel*/ },
};

static int exynos_pmu_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pmu_base_addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(pmu_base_addr))
		return PTR_ERR(pmu_base_addr);

	pmu_context = devm_kzalloc(&pdev->dev,
			sizeof(struct exynos_pmu_context),
			GFP_KERNEL);
	if (!pmu_context) {
		dev_err(dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}
	pmu_context->dev = dev;

	match = of_match_node(exynos_pmu_of_device_ids, dev->of_node);

	pmu_context->pmu_data = match->data;

	if (pmu_context->pmu_data->pmu_init)
		pmu_context->pmu_data->pmu_init();

	platform_set_drvdata(pdev, pmu_context);

	arm_pm_restart = exynos_reboot;

	pm_power_off = exynos_power_off;

	dev_dbg(dev, "Exynos PMU Driver probe done\n");
	return 0;
}

static struct platform_driver exynos_pmu_driver = {
	.driver  = {
		.name   = "exynos-pmu",
		.of_match_table = exynos_pmu_of_device_ids,
	},
	.probe = exynos_pmu_probe,
};

static int __init exynos_pmu_init(void)
{
	return platform_driver_register(&exynos_pmu_driver);

}
postcore_initcall(exynos_pmu_init);
