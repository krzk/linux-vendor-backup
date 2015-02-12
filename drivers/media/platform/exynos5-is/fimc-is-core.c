/*
 * Samsung EXYNOS5 FIMC-IS (Imaging Subsystem) driver
*
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
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
#include "exynos5-mdev.h"
#include "fimc-is-regs.h"

#define CLK_MCU_ISP_DIV0_FREQ		(200 * 1000000)
#define CLK_MCU_ISP_DIV1_FREQ		(100 * 1000000)
#define CLK_ISP_DIV0_FREQ		(150 * 1000000)
#define CLK_ISP_DIV1_FREQ		(50  * 1000000)
#define CLK_ISP_DIVMPWM_FREQ		(50 /*75*/  * 1000000)
#define CLK_MCU_ISP_ACLK_400_FREQ	(400 * 1000000)
#define CLK_DIV_ACLK_266_FREQ		(/*300*/ 100 * 1000000)
#define EXYNOS3250_CLK_ISP_DIV_FREQ	(50 * 1000000)

#define FIMC_IS_EXYNOS5_CLK_MASK	0x7f
#define FIMC_IS_EXYNOS3250_CLK_MASK	0xff

static const char * const fimc_is_clock_name[] = {
	[IS_CLK_ISP]		= "isp",
	[IS_CLK_MCU_ISP]	= "mcu_isp",
	[IS_CLK_ISP_DIV0]	= "isp_div0",
	[IS_CLK_ISP_DIV1]	= "isp_div1",
	[IS_CLK_ISP_DIVMPWM]	= "isp_divmpwm",
	[IS_CLK_MCU_ISP_DIV0]	= "mcu_isp_div0",
	[IS_CLK_MCU_ISP_DIV1]	= "mcu_isp_div1",
        /* Exynos 3250 */
        [IS_CLK_CAM1]		= "cam1",
        /* ACLK 400 MCUISP */
        [IS_CLK_MOUT_ACLK400_MCUISP_SUB] = "mout_aclk400_mcuisp_sub",
        [IS_CLK_DIV_ACLK400_MCUISP]      = "div_aclk400_mcuisp",
        [IS_CLK_MOUT_ACLK400_MCUISP]     = "mout_aclk400_mcuisp",
        /* ACLK 266 */
        [IS_CLK_MOUT_ACLK266_0]          = "mout_aclk266_0",
        [IS_CLK_MOUT_ACLK266]            = "mout_aclk266",
        [IS_CLK_DIV_ACLK266]             = "div_aclk266",
        [IS_CLK_MOUT_ACLK266_SUB]        = "mout_aclk266_sub",
        [IS_CLK_DIV_MPLL_PRE]            =  "div_mpll_pre",
        [IS_CLK_FIN_PLL]                 = "fin_pll",
};

#define FIMC_IS_CLK_ERR(dev, i , action) \
        dev_err(dev, "Failed to "#action" for %s clock\n", \
                fimc_is_clock_name[i]);

static int fimc_is_exyno3250_clk_cfg(struct fimc_is *is)
{
        int ret;

        /* DIV 400 MCUISP -> MUX ACLK 400 MCUISP SUB */
        ret = clk_set_parent(is->clocks[IS_CLK_MOUT_ACLK400_MCUISP_SUB],
                             is->clocks[IS_CLK_DIV_ACLK400_MCUISP]);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_MOUT_ACLK400_MCUISP_SUB,
                                set parentness);
                return ret;
	}

        ret = clk_set_rate(is->clocks[IS_CLK_DIV_ACLK400_MCUISP],
                           CLK_MCU_ISP_ACLK_400_FREQ);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_DIV_ACLK400_MCUISP,
                                set rate);
                return ret;
        }

        ret = clk_set_rate(is->clocks[IS_CLK_MCU_ISP_DIV0],
                           CLK_MCU_ISP_DIV0_FREQ);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_MCU_ISP_DIV0,
                                set rate);
                return ret;
	}

        ret = clk_set_rate(is->clocks[IS_CLK_MCU_ISP_DIV1],
                           CLK_MCU_ISP_DIV1_FREQ);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_MCU_ISP_DIV1,
                                set rate);
                return ret;
        }

        /* ACLK 266 */
        clk_set_parent( is->clocks[IS_CLK_MOUT_ACLK266_0],
                         is->clocks[IS_CLK_DIV_MPLL_PRE]);

        if (ret) {
                 FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_MOUT_ACLK266_0,
				 ser rate);
                 return ret;
        }

        ret = clk_set_parent(is->clocks[IS_CLK_MOUT_ACLK266],
                             is->clocks[IS_CLK_MOUT_ACLK266_0]);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_MOUT_ACLK266,
                                set parentness);
		return ret;
        }

        ret = clk_set_parent(is->clocks[IS_CLK_MOUT_ACLK266_SUB],
                             is->clocks[IS_CLK_DIV_ACLK266]);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_MOUT_ACLK266_SUB,
                                set parentness);
                return ret;
        }

        ret = clk_set_rate(is->clocks[IS_CLK_DIV_ACLK266],
                           CLK_DIV_ACLK_266_FREQ);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_DIV_ACLK266,
                                set rate);
                return ret;
        }

        ret = clk_set_rate(is->clocks[IS_CLK_ISP_DIV0],
                           EXYNOS3250_CLK_ISP_DIV_FREQ);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev,  IS_CLK_ISP_DIV0,
                                set rate);
                return ret;
        }

        ret = clk_set_rate(is->clocks[IS_CLK_ISP_DIV1],
                        EXYNOS3250_CLK_ISP_DIV_FREQ);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_ISP_DIV1,
                                set rate);
                return ret;
        }

        ret = clk_set_rate(is->clocks[IS_CLK_ISP_DIVMPWM], CLK_ISP_DIVMPWM_FREQ);
        if (ret) {
                FIMC_IS_CLK_ERR(&is->pdev->dev, IS_CLK_ISP_DIVMPWM,
                                set rate);
                return ret;
        }

        return ret;

}

static int fimc_is_exynos5_clk_cfg(struct fimc_is *is)
{
        int ret;

	/* Set rates */
        ret = clk_set_rate(is->clocks[IS_CLK_MCU_ISP_DIV0],
			CLK_MCU_ISP_DIV0_FREQ);
	if (ret)
		return ret;
        ret = clk_set_rate(is->clocks[IS_CLK_MCU_ISP_DIV1],
			CLK_MCU_ISP_DIV1_FREQ);
	if (ret)
		return ret;
        ret = clk_set_rate(is->clocks[IS_CLK_ISP_DIV0], CLK_ISP_DIV0_FREQ);
	if (ret)
		return ret;
        ret = clk_set_rate(is->clocks[IS_CLK_ISP_DIV1], CLK_ISP_DIV1_FREQ);
	if (ret)
		return ret;
        return clk_set_rate(is->clocks[IS_CLK_ISP_DIVMPWM],
			CLK_ISP_DIVMPWM_FREQ);
}

static void fimc_is_put_clocks(struct fimc_is *is)
{
        int i;

        for (i = IS_CLK_GATE_MAX; i < IS_CLKS_MAX; ++i) {
                if (!IS_ERR(is->clocks[i]))
                        clk_unprepare(is->clocks[i]);

        }
        for (i = 0; i < IS_CLKS_MAX; ++i) {
                if(!IS_ERR(is->clocks[i])) {
                        clk_put(is->clocks[i]);
                        is->clocks[i] = ERR_PTR(-EINVAL);
                }
        }
}

static int fimc_is_get_clocks(struct fimc_is *is)
{
        int i, ret;

        for (i = 0; i < IS_CLKS_MAX; ++i)
                is->clocks[i] = ERR_PTR(-EINVAL);

        for (i = 0; i < IS_CLK_GATE_MAX; ++i) {
                if (!(is->drvdata->clk_mask & (0x1 << i)))
                        continue;
                is->clocks[i] = clk_get(&is->pdev->dev,
                                fimc_is_clock_name[i]);
                if (IS_ERR(is->clocks[i])) {
                        dev_err(&is->pdev->dev,
                                "Failed to acquire clock : %s\n",
                                fimc_is_clock_name[i]);
                        ret = PTR_ERR(is->clocks[i]);
                        goto rollback;
                }
        }

        if (is->drvdata->variant == FIMC_IS_EXYNOS3250) {
                for (i = IS_CLK_GATE_MAX; i < IS_CLKS_MAX ; ++i) {
                        is->clocks[i] = clk_get(&is->pdev->dev,
                                fimc_is_clock_name[i]);

                        if (IS_ERR(is->clocks[i])) {
                                dev_err(&is->pdev->dev,
                                        "Failed to acquire clock: %s\n",
                                         fimc_is_clock_name[i]);
                                ret = PTR_ERR(is->clocks[i]);
                                goto rollback;
                        }

                        ret =  clk_prepare(is->clocks[i]);

                        if (ret) {
                                dev_err(&is->pdev->dev,
                                        "Failed to prepare clock %s\n",
                                        fimc_is_clock_name[i]);
                                goto rollback;
                        }
                }
        }

        return 0;

rollback:
        fimc_is_put_clocks(is);
        return ret;
}

static int fimc_is_configure_clocks(struct fimc_is* is)
{
        int ret = 0;

        switch(is->drvdata->variant){
        case FIMC_IS_EXYNOS5:
                ret = fimc_is_exynos5_clk_cfg(is);
                break;
        case FIMC_IS_EXYNOS3250:
                ret = fimc_is_exyno3250_clk_cfg(is);
                break;
        default:
                ret = -EINVAL;
                break;
        }
        return ret;
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
        ret  = of_property_read_u32(node, "reg", &tmp);
        if (ret < 0) {
                dev_err(&is->pdev->dev, "reg property not found at: %s\n",
                                        node->full_name);
                return ret;
        }
        sensor->i2c_slave_addr = tmp;

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

static struct fimc_is_drvdata exynos3250_drvdata = {
	.num_instances	= 1,
	.fw_name	= "exynos3_fimc_is_fw.bin",
        .fw_version	= FIMC_IS_FW_V130,
        .clk_mask	= FIMC_IS_EXYNOS3250_CLK_MASK,
        .subip_mask	= FIMC_IS_EXYNOS3250_SUBBLOCKS,
        .variant	= FIMC_IS_EXYNOS3250,
	.master_node	= 1,
};

static struct fimc_is_drvdata exynos5250_drvdata = {
        .num_instances	= 1,
        .fw_name	= "exynos5_fimc_is_fw.bin",
        .fw_version	= FIMC_IS_FW_V120,
        .clk_mask	= FIMC_IS_EXYNOS5_CLK_MASK,
        .subip_mask	= FIMC_IS_EXYNOS5_SUBBLOCKS,
        .variant	= FIMC_IS_EXYNOS5,
};

static const struct of_device_id fimc_is_of_match[] = {
	{
		.compatible	= "samsung,exynos5250-fimc-is",
		.data		= &exynos5250_drvdata,
	}, {
		.compatible	= "samsung,exynos3250-fimc-is",
		.data		= &exynos3250_drvdata,
	},
	{/* sentinel */},
};
MODULE_DEVICE_TABLE(of, fimc_is_of_match);

int fimc_is_clk_enable(struct fimc_is *is)
{
        int i, ret = 0;

        for (i = 0; i < IS_CLK_GATE_MAX; ++i){
                if (IS_ERR(is->clocks[i]))
                        continue;

                ret = clk_prepare_enable(is->clocks[i]);
                if (ret) {
                        dev_err(&is->pdev->dev,
                                "Failed to prepare and enable %s clock\n",
                                fimc_is_clock_name[i]);
                        for (; i >= 0; --i)
                                clk_disable_unprepare(is->clocks[i]);

                }
        }
        return ret;
}

void fimc_is_clk_disable(struct fimc_is *is)
{
        int i;
        for (i = 0; i < IS_CLK_GATE_MAX; ++i)
                if (!IS_ERR(is->clocks[i]))
                        clk_disable_unprepare(is->clocks[i]);
        if (is->drvdata->variant == FIMC_IS_EXYNOS3250) {
                if (clk_set_parent(is->clocks[IS_CLK_MOUT_ACLK400_MCUISP_SUB],
                                   is->clocks[IS_CLK_FIN_PLL]))
                        FIMC_IS_CLK_ERR(&is->pdev->dev,
                                        IS_CLK_MOUT_ACLK400_MCUISP_SUB,
                                        change parentness);
                if (clk_set_parent(is->clocks[IS_CLK_MOUT_ACLK266_SUB],
                                   is->clocks[IS_CLK_FIN_PLL]))
                        FIMC_IS_CLK_ERR(&is->pdev->dev,
                                        IS_CLK_MOUT_ACLK266_SUB,
                                        change parentness);
        }
}

static inline void fimc_is_reset_cmu_isp(struct fimc_is *is)
{
	if (is->drvdata->variant == FIMC_IS_EXYNOS3250)
		pmu_is_write(0x0, is, EXYNOS3250_PMUREG_CMU_RESET_ISP);
}

static int fimc_is_pm_resume(struct device *dev)
{
        struct fimc_is *is = dev_get_drvdata(dev);
        int ret;

        fimc_is_configure_clocks(is);
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
	fimc_is_reset_cmu_isp(is);
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

static int fimc_is_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
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

	match = of_match_node(fimc_is_of_match, dev->of_node);
	if (WARN_ON(!match))
		return -EINVAL;

	is->drvdata	= match->data;
	is->pdev	= pdev;

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

        ret = fimc_is_get_clocks(is);
	if (ret < 0) {
		dev_err(dev, "clocks configuration failed\n");
		goto err_clk;
	}

	platform_set_drvdata(pdev, is);
	pm_runtime_enable(dev);
        if (!pm_runtime_enabled(dev)) {
                ret = fimc_is_runtime_resume(dev);
                if (ret) {
                        dev_err(dev, "Runtime resume failed\n");
                        goto err_clk;
                }

        }

	fimc_is_reset_cmu_isp(is);

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

	if (is->drvdata->master_node) {
		ret = exynos_camera_register(dev, &is->md);
		if (ret < 0)
			goto err_cam;
	}

        INIT_LIST_HEAD(&is->event_listeners);
        spin_lock_init(&is->events_lock);

	dev_dbg(dev, "FIMC-IS registered successfully\n");

	return 0;
err_cam:
	exynos_camera_unregister(is->md);
err_sd:
	fimc_is_pipelines_destroy(is);
err_vb:
	vb2_dma_contig_cleanup_ctx(is->alloc_ctx);
err_clk:
	fimc_is_put_clocks(is);

	return ret;
}

static int fimc_is_remove(struct platform_device *pdev)
{
	struct fimc_is *is = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	exynos_camera_unregister(is->md);

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
		.of_match_table = fimc_is_of_match,
	}
};

int __init fimc_is_init(void)
{
	return platform_driver_register(&fimc_is_driver);
}

void __exit fimc_is_cleanup(void)
{
	platform_driver_unregister(&fimc_is_driver);
}
