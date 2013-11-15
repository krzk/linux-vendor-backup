/* Common Exynos ASV(Adaptive Supply Voltage) driver
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power/asv-driver.h>
#include "exynos-asv.h"

static int exynos_asv_probe(struct platform_device *pdev)
{
	struct device_node *chip_id;
	struct exynos_asv_common *exynos_asv_info;
	void __iomem *base;
	int ret = 0;

	exynos_asv_info = devm_kzalloc(&pdev->dev, sizeof(*exynos_asv_info),
				       GFP_KERNEL);
	if (!exynos_asv_info)
		return -ENOMEM;

	chip_id = of_find_compatible_node(NULL, NULL,
						"samsung,exynos4210-chipid");
	if (!chip_id) {
		pr_err("%s: unable to find chipid\n", __func__);
		return -ENODEV;
	}

	base = of_iomap(chip_id, 0);
	if (!base) {
		dev_err(&pdev->dev, "unable to map chip_id register\n");
		ret = -ENOMEM;
		goto err_map;
	}

	exynos_asv_info->base = base;

	/* call SoC specific intialisation routine */
	if (of_machine_is_compatible("samsung,exynos5250")) {
		ret = exynos5250_asv_init(exynos_asv_info);
		if (ret) {
			pr_err("exynos5250_asv_init failed : %d\n", ret);
			goto err;
		}
	}

	register_asv_member(exynos_asv_info->asv_list, exynos_asv_info->nr_mem);

err:
	iounmap(base);
err_map:
	of_node_put(chip_id);

	return ret;
}

static struct platform_driver exynos_asv_platdrv = {
	.driver = {
		.name	= "exynos-asv",
		.owner	= THIS_MODULE,
	},
	.probe		= exynos_asv_probe,
};
module_platform_driver(exynos_asv_platdrv);

MODULE_AUTHOR("Yadwinder Singh Brar<yadi.brar@samsung.com>");
MODULE_DESCRIPTION("Exynos ASV driver");
MODULE_LICENSE("GPL");
