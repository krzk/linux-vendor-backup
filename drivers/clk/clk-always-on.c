/*
 * Always-on Clock Domain
 *
 * Copyright (C) 2015 STMicroelectronics – All Rights Reserved
 *
 * Author: Lee Jones <lee.jones@linaro.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/platform_device.h>

static void ao_clock_domain_hog_clock(struct platform_device *pdev, int index)
{
	struct device_node *np = pdev->dev.of_node;
	struct clk *clk;
	int ret;

	clk = of_clk_get(np, index);
	if (IS_ERR(clk)) {
		dev_warn(&pdev->dev, "Failed get clock %s[%d]: %li\n",
			 np->full_name, index, PTR_ERR(clk));
		return;
	}

	ret = clk_prepare_enable(clk);
	if (ret)
		dev_warn(&pdev->dev, "Failed to enable clock: %s\n", __clk_get_name(clk));
}

static int ao_clock_domain_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int nclks, i;

	nclks = of_count_phandle_with_args(np, "clocks", "#clock-cells");

	for (i = 0; i < nclks; i++)
		ao_clock_domain_hog_clock(pdev, i);

	return 0;
}

static const struct of_device_id ao_clock_domain_match[] = {
	{ .compatible = "clk-always-on" },
	{ },
};

static struct platform_driver ao_clock_domain_driver = {
	.probe = ao_clock_domain_probe,
	.driver = {
		.name = "clk-always-on",
		.of_match_table = ao_clock_domain_match,
	},
};

static int __init ao_clock_domain_init(void)
{
	return platform_driver_register(&ao_clock_domain_driver);
}
arch_initcall(ao_clock_domain_init);

static void __exit ao_clock_domain_exit(void)
{
	platform_driver_unregister(&ao_clock_domain_driver);
}
module_exit(ao_clock_domain_exit);
