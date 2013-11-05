/*
 * Samsung EXYNOS5 FIMC-IS (Imaging Subsystem) driver
*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Arun Kumar K <arun.kk@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/videodev2.h>

#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-of.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>

#include "fimc-is.h"
#include "fimc-is-i2c.h"

#define CLK_MCU_ISP_DIV0_FREQ	(200 * 1000000)
#define CLK_MCU_ISP_DIV1_FREQ	(100 * 1000000)
#define CLK_ISP_DIV0_FREQ	(134 * 1000000)
#define CLK_ISP_DIV1_FREQ	(68 * 1000000)
#define CLK_ISP_DIVMPWM_FREQ	(34 * 1000000)

static const char * const fimc_is_clock_name[] = {
	[IS_CLK_ISP]		= "isp",
	[IS_CLK_MCU_ISP]	= "mcu_isp",
	[IS_CLK_ISP_DIV0]	= "isp_div0",
	[IS_CLK_ISP_DIV1]	= "isp_div1",
	[IS_CLK_ISP_DIVMPWM]	= "isp_divmpwm",
	[IS_CLK_MCU_ISP_DIV0]	= "mcu_isp_div0",
	[IS_CLK_MCU_ISP_DIV1]	= "mcu_isp_div1",
};

static void fimc_is_put_clocks(struct fimc_is *is)
{
	int i;

	for (i = 0; i < IS_CLK_MAX_NUM; i++) {
		if (IS_ERR(is->clock[i]))
			continue;
		clk_unprepare(is->clock[i]);
		clk_put(is->clock[i]);
		is->clock[i] = ERR_PTR(-EINVAL);
	}
}

static int fimc_is_get_clocks(struct fimc_is *is)
{
	struct device *dev = &is->pdev->dev;
	int i, ret;

	for (i = 0; i < IS_CLK_MAX_NUM; i++) {
		is->clock[i] = clk_get(dev, fimc_is_clock_name[i]);
		if (IS_ERR(is->clock[i]))
			goto err;
		ret = clk_prepare(is->clock[i]);
		if (ret < 0) {
			clk_put(is->clock[i]);
			is->clock[i] = ERR_PTR(-EINVAL);
			goto err;
		}
	}
	return 0;
err:
	fimc_is_put_clocks(is);
	dev_err(dev, "Failed to get clock: %s\n", fimc_is_clock_name[i]);
	return -ENXIO;
}

static int fimc_is_configure_clocks(struct fimc_is *is)
{
	int i, ret;

	for (i = 0; i < IS_CLK_MAX_NUM; i++)
		is->clock[i] = ERR_PTR(-EINVAL);

	ret = fimc_is_get_clocks(is);
	if (ret)
		return ret;

	/* Set rates */
	ret = clk_set_rate(is->clock[IS_CLK_MCU_ISP_DIV0],
			CLK_MCU_ISP_DIV0_FREQ);
	if (ret)
		return ret;
	ret = clk_set_rate(is->clock[IS_CLK_MCU_ISP_DIV1],
			CLK_MCU_ISP_DIV1_FREQ);
	if (ret)
		return ret;
	ret = clk_set_rate(is->clock[IS_CLK_ISP_DIV0], CLK_ISP_DIV0_FREQ);
	if (ret)
		return ret;
	ret = clk_set_rate(is->clock[IS_CLK_ISP_DIV1], CLK_ISP_DIV1_FREQ);
	if (ret)
		return ret;
	return clk_set_rate(is->clock[IS_CLK_ISP_DIVMPWM],
			CLK_ISP_DIVMPWM_FREQ);
}

static void fimc_is_pipelines_destroy(struct fimc_is *is)
{
	int i;

	for (i = 0; i < is->drvdata->num_instances; i++)
		fimc_is_pipeline_destroy(&is->pipeline[i]);
}

static int fimc_is_parse_sensor_config(struct fimc_is *is, unsigned int index,
						struct device_node *node)
{
	struct fimc_is_sensor *sensor = &is->sensor[index];
	u32 tmp = 0;
	int ret;

	sensor->drvdata = exynos5_is_sensor_get_drvdata(node);
	if (!sensor->drvdata) {
		dev_err(&is->pdev->dev, "no driver data found for: %s\n",
							 node->full_name);
		return -EINVAL;
	}

	node = v4l2_of_get_next_endpoint(node, NULL);
	if (!node)
		return -ENXIO;

	node = v4l2_of_get_remote_port(node);
	if (!node)
		return -ENXIO;

	/* Use MIPI-CSIS channel id to determine the ISP I2C bus index. */
	ret = of_property_read_u32(node, "reg", &tmp);
	if (ret < 0) {
		dev_err(&is->pdev->dev, "reg property not found at: %s\n",
							 node->full_name);
		return ret;
	}

	sensor->i2c_bus = tmp - FIMC_INPUT_MIPI_CSI2_0;
	return 0;
}

static int fimc_is_parse_sensor(struct fimc_is *is)
{
	struct device_node *i2c_bus, *child;
	int ret, index = 0;

	for_each_compatible_node(i2c_bus, NULL, FIMC_IS_I2C_COMPATIBLE) {
		for_each_available_child_of_node(i2c_bus, child) {
			ret = fimc_is_parse_sensor_config(is, index, child);

			if (ret < 0 || index >= FIMC_IS_NUM_SENSORS) {
				of_node_put(child);
				return ret;
			}
			index++;
		}
	}
	return 0;
}

static struct fimc_is_drvdata exynos5250_drvdata = {
	.num_instances	= 1,
	.fw_name	= "exynos5_fimc_is_fw.bin",
};

static const struct of_device_id exynos5_fimc_is_match[] = {
	{
		.compatible = "samsung,exynos5250-fimc-is",
		.data = &exynos5250_drvdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_fimc_is_match);

static void *fimc_is_get_drvdata(struct platform_device *pdev)
{
	struct fimc_is_drvdata *driver_data = NULL;
	const struct of_device_id *match;

	match = of_match_node(exynos5_fimc_is_match,
			pdev->dev.of_node);
	if (match)
		driver_data = (struct fimc_is_drvdata *)match->data;
	return driver_data;
}

static int fimc_is_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct fimc_is *is;
	void __iomem *regs;
	struct device_node *node;
	int irq, ret;
	int i;

	dev_dbg(dev, "FIMC-IS Probe Enter\n");

	if (!dev->of_node)
		return -ENODEV;

	is = devm_kzalloc(&pdev->dev, sizeof(*is), GFP_KERNEL);
	if (!is)
		return -ENOMEM;

	is->pdev = pdev;

	is->drvdata = fimc_is_get_drvdata(pdev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	/* Get the PMU base */
	node = of_parse_phandle(dev->of_node, "samsung,pmu", 0);
	if (!node)
		return -ENODEV;
	is->pmu_regs = of_iomap(node, 0);
	if (!is->pmu_regs)
		return -ENOMEM;

	irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!irq) {
		dev_err(dev, "Failed to get IRQ\n");
		return irq;
	}

	ret = fimc_is_configure_clocks(is);
	if (ret < 0) {
		dev_err(dev, "clocks configuration failed\n");
		goto err_clk;
	}

	platform_set_drvdata(pdev, is);
	pm_runtime_enable(dev);

	is->alloc_ctx = vb2_dma_contig_init_ctx(dev);
	if (IS_ERR(is->alloc_ctx)) {
		ret = PTR_ERR(is->alloc_ctx);
		goto err_vb;
	}

	/* Get IS-sensor contexts */
	ret = fimc_is_parse_sensor(is);
	if (ret < 0)
		goto err_vb;

	/* Initialize FIMC Pipeline */
	for (i = 0; i < is->drvdata->num_instances; i++) {
		ret = fimc_is_pipeline_init(&is->pipeline[i], i, is);
		if (ret < 0)
			goto err_sd;
	}

	/* Initialize FIMC Interface */
	ret = fimc_is_interface_init(&is->interface, regs, irq);
	if (ret < 0)
		goto err_sd;

	/* Probe the peripheral devices  */
	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret < 0)
		goto err_sd;

	dev_dbg(dev, "FIMC-IS registered successfully\n");

	return 0;

err_sd:
	fimc_is_pipelines_destroy(is);
err_vb:
	vb2_dma_contig_cleanup_ctx(is->alloc_ctx);
err_clk:
	fimc_is_put_clocks(is);

	return ret;
}

int fimc_is_clk_enable(struct fimc_is *is)
{
	int ret;

	ret = clk_enable(is->clock[IS_CLK_ISP]);
	if (ret)
		return ret;
	ret = clk_enable(is->clock[IS_CLK_MCU_ISP]);
	if (ret)
		clk_disable(is->clock[IS_CLK_ISP]);
	return ret;
}

void fimc_is_clk_disable(struct fimc_is *is)
{
	clk_disable(is->clock[IS_CLK_ISP]);
	clk_disable(is->clock[IS_CLK_MCU_ISP]);
}

static int fimc_is_pm_resume(struct device *dev)
{
	struct fimc_is *is = dev_get_drvdata(dev);
	int ret;

	ret = fimc_is_clk_enable(is);
	if (ret < 0) {
		dev_err(dev, "Could not enable clocks\n");
		return ret;
	}
	return 0;
}

static int fimc_is_pm_suspend(struct device *dev)
{
	struct fimc_is *is = dev_get_drvdata(dev);

	fimc_is_clk_disable(is);
	return 0;
}

static int fimc_is_runtime_resume(struct device *dev)
{
	return fimc_is_pm_resume(dev);
}

static int fimc_is_runtime_suspend(struct device *dev)
{
	return fimc_is_pm_suspend(dev);
}

#ifdef CONFIG_PM_SLEEP
static int fimc_is_resume(struct device *dev)
{
	/* TODO */
	return 0;
}

static int fimc_is_suspend(struct device *dev)
{
	/* TODO */
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static int fimc_is_remove(struct platform_device *pdev)
{
	struct fimc_is *is = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	pm_runtime_disable(dev);
	pm_runtime_set_suspended(dev);
	fimc_is_pipelines_destroy(is);
	vb2_dma_contig_cleanup_ctx(is->alloc_ctx);
	fimc_is_put_clocks(is);
	return 0;
}

static const struct dev_pm_ops fimc_is_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(fimc_is_suspend, fimc_is_resume)
	SET_RUNTIME_PM_OPS(fimc_is_runtime_suspend, fimc_is_runtime_resume,
			   NULL)
};

static struct platform_driver fimc_is_driver = {
	.probe		= fimc_is_probe,
	.remove		= fimc_is_remove,
	.driver = {
		.name	= FIMC_IS_DRV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &fimc_is_pm_ops,
		.of_match_table = exynos5_fimc_is_match,
	}
};
module_platform_driver(fimc_is_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Arun Kumar K <arun.kk@samsung.com>");
MODULE_DESCRIPTION("Samsung Exynos5 (FIMC-IS) Imaging Subsystem driver");
