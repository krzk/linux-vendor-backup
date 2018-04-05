/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/i2c.h>

#include "fimc-is-video.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-companion.h"
#include "fimc-is-sec-define.h"
#include "fimc-is-device-ois.h"
#include "fimc-is-companion-dt.h"

extern int fimc_is_comp_video_probe(void *data);

int fimc_is_companion_wait(struct fimc_is_device_companion *device)
{
	int ret = 0;

	ret = wait_event_timeout(device->init_wait_queue,
		(device->companion_status == FIMC_IS_COMPANION_OPENDONE),
		FIMC_IS_COMPANION_TIMEOUT);
	if (ret) {
		ret = 0;
	} else {
		err("timeout");
		device->companion_status = FIMC_IS_COMPANION_IDLE;
		ret = -ETIME;
	}

	return ret;
}

static void fimc_is_companion_wakeup(struct fimc_is_device_companion *device)
{
	wake_up(&device->init_wait_queue);
}

static int fimc_is_companion_mclk_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->dev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (test_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state)) {
		err("%s : already clk on", __func__);
		goto p_err;
	}

	if (!pdata->mclk_on) {
		err("mclk_on is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_on(device->dev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		err("mclk_on failed(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_mclk_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;

	BUG_ON(!device);
	BUG_ON(!device->dev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state)) {
		err("%s : already clk off", __func__);
		goto p_err;
	}

	if (!pdata->mclk_off) {
		err("mclk_off is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->mclk_off(device->dev, pdata->scenario, pdata->mclk_ch);
	if (ret) {
		err("mclk_off failed(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_iclk_on(struct fimc_is_device_companion *device)
{
	struct exynos_platform_fimc_is_sensor *pdata;
	int ret = 0;

	BUG_ON(!device);
	BUG_ON(!device->dev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (test_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state)) {
		err("%s : already clk on", __func__);
		goto p_err;
	}

	if (!pdata->iclk_cfg) {
		err("iclk_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!pdata->iclk_on) {
		err("iclk_on is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_cfg(device->dev, pdata->scenario, 0);
	if (ret) {
		err("iclk_cfg failed(%d)", ret);
		goto p_err;
	}

	ret = pdata->iclk_on(device->dev, pdata->scenario, 0);
	if (ret) {
		err("iclk_on failed(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_iclk_off(struct fimc_is_device_companion *device)
{
	struct exynos_platform_fimc_is_sensor *pdata;
	int ret = 0;

	BUG_ON(!device);
	BUG_ON(!device->dev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state)) {
		err("%s : already clk off", __func__);
		goto p_err;
	}

	if (!pdata->iclk_off) {
		err("iclk_off is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->iclk_off(device->dev, pdata->scenario, 0);
	if (ret) {
		err("iclk_off failed(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_gpio_on(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_from_info *sysfs_finfo;
	struct exynos_sensor_pin (*pin_ctrls)[2][GPIO_CTRL_MAX];
	struct fimc_is_core *core;

	BUG_ON(!device);
	BUG_ON(!device->dev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;
	pin_ctrls = pdata->pin_ctrls;
	core = dev_get_drvdata(fimc_is_dev);

	if (test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio on", __func__);
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	core->running_rear_camera = true;

	if(core->use_sensor_dynamic_voltage_mode) {
		fimc_is_sec_get_sysfs_finfo(&sysfs_finfo);
		if (pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][0].name != NULL &&
			!strcmp(pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][0].name, "CAM_SEN_A2.8V_AP")) {
			if (sysfs_finfo->header_ver[1] == '1' && sysfs_finfo->header_ver[2] == '6' && sysfs_finfo->header_ver[4] == 'L') {
				pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][0].voltage = 2950000;
				info("LSI Sensor EVT2.4 voltage(%d)\n", pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][0].voltage);
			} else {
				pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][0].voltage = 2800000;
			}
		}

		if (pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][1].name != NULL &&
			!strcmp(pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][1].name, "CAM_SEN_CORE_1.2V_AP")) {
			if (sysfs_finfo->header_ver[1] == '1' && sysfs_finfo->header_ver[2] == '6' && sysfs_finfo->header_ver[4] == 'S') {
				pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][1].voltage = 1050000;
				info("SONY Sensor  voltage(%d)\n", pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][1].voltage);
			} else {
				pin_ctrls[pdata->scenario][GPIO_SCENARIO_ON][1].voltage = 1200000;
			}
		}
	}

	ret = pdata->gpio_cfg(device->dev, pdata->scenario, GPIO_SCENARIO_ON);
	if (ret) {
		err("gpio_cfg failed(%d)", ret);
		goto p_err;
	}

	set_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	return ret;
}

static int fimc_is_companion_gpio_off(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct exynos_platform_fimc_is_sensor *pdata;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);

	BUG_ON(!device);
	BUG_ON(!device->dev);
	BUG_ON(!device->pdata);

	pdata = device->pdata;

	if (!test_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state)) {
		err("%s : already gpio off", __func__);
		goto p_err;
	}

	if (!pdata->gpio_cfg) {
		err("gpio_cfg is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = pdata->gpio_cfg(device->dev, pdata->scenario, GPIO_SCENARIO_OFF);
	if (ret) {
		err("gpio_cfg failed(%d)", ret);
		goto p_err;
	}

	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

p_err:
	core->running_rear_camera = false;

	return ret;
}


int fimc_is_companion_open(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct fimc_is_core *core;
	/* Workaround for Host to use ISP-SPI. Will be removed later.*/
	struct fimc_is_spi_gpio *spi_gpio;
	static char companion_fw_name[100];
	static char master_setf_name[100];
	static char mode_setf_name[100];
	static char fw_name[100];
	static char setf_name[100];

	BUG_ON(!device);

	core = dev_get_drvdata(fimc_is_dev);
	spi_gpio = &core->spi_gpio;

	if (test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already open");
		ret = -EMFILE;
		goto p_err;
	}

	device->companion_status = FIMC_IS_COMPANION_OPENNING;
	core->running_rear_camera = true;
	pm_runtime_get_sync(device->dev);
	ret = fimc_is_sec_fw_sel(core, device->dev, fw_name, setf_name, false);
	if (ret < 0) {
		err("failed to select firmware (%d)", ret);
		goto p_err_pm;
	}
	ret = fimc_is_sec_concord_fw_sel(core, device->dev, companion_fw_name,
					 master_setf_name, mode_setf_name);

	/* TODO: loading firmware */
	fimc_is_s_int_comb_isp(core, false, INTMR2_INTMCIS(22));

	fimc_is_set_spi_config(spi_gpio, FIMC_IS_SPI_FUNC, false);

	if (fimc_is_comp_is_valid(core) == 0) {
		ret = fimc_is_comp_loadfirm(core);
		if (ret) {
			err("fimc_is_comp_loadfirm() fail");
			goto p_err_pm;
                }
		ret = fimc_is_comp_loadcal(core);
		if (ret) {
			err("fimc_is_comp_loadcal() fail");
		}

		fimc_is_power_binning(core);

		ret = fimc_is_comp_loadsetf(core);
		if (ret) {
			err("fimc_is_comp_loadsetf() fail");
			goto p_err_pm;
		}
	}

	set_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	device->companion_status = FIMC_IS_COMPANION_OPENDONE;
	fimc_is_companion_wakeup(device);

	if(core->use_ois) {
		if (!core->ois_ver_read) {
			fimc_is_ois_check_fw(core);
		}

		fimc_is_ois_exif_data(core);
	}

	info("[COMP:D] %s(%d)status(%d)\n", __func__, ret, device->companion_status);
	return ret;

p_err_pm:
	pm_runtime_put_sync(device->dev);

p_err:
	err("[COMP:D] open fail(%d)status(%d)", ret, device->companion_status);
	return ret;
}

int fimc_is_companion_close(struct fimc_is_device_companion *device)
{
	int ret = 0;
	struct fimc_is_core *core = dev_get_drvdata(fimc_is_dev);

	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}

	BUG_ON(!device);

	if (!test_bit(FIMC_IS_COMPANION_OPEN, &device->state)) {
		err("already close");
		ret = -EMFILE;
		goto p_err;
	}

	pm_runtime_put_sync(device->dev);
#if 0
	if (core != NULL && !test_bit(FIMC_IS_ISCHAIN_POWER_ON, &core->state)) {
		u32 timeout;
		warn("only companion device closing after open..");
		timeout = 2000;
		while ((readl(PMUREG_CAM1_STATUS) & 0x1) && timeout) {
			timeout--;
			usleep_range(1000, 1000);
			if (!(timeout % 100))
				warn("wait for CAM1 power down..(%d)", timeout);
		}
		if (timeout == 0)
			err("CAM1 power down failed(CAM1:0x%08x, A5:0x%08x)\n",
					readl(PMUREG_CAM1_STATUS), readl(PMUREG_ISP_ARM_STATUS));
	}
#endif

	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);

p_err:
	core->running_rear_camera = false;
	device->companion_status = FIMC_IS_COMPANION_IDLE;
	info("[COMP:D] %s(%d)\n", __func__, ret);
	return ret;
}

static int fimc_is_companion_suspend(struct device *dev)
{
	info("%s\n", __func__);

	return 0;
}

static int fimc_is_companion_resume(struct device *dev)
{
	info("%s\n", __func__);

	return 0;
}

int fimc_is_companion_runtime_suspend(struct device *dev)
{
	int ret = 0;
	struct fimc_is_device_companion *device;
	struct fimc_is_core *core;

	info("%s\n", __func__);

	core = dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}
	device = dev_get_drvdata(dev);
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto err_dev_null;
	}

	/* gpio uninit */
	ret = fimc_is_companion_gpio_off(device);
	if (ret) {
		err("fimc_is_companion_gpio_off failed(%d)", ret);
		goto p_err;
	}

	/* periperal internal clock off */
	ret = fimc_is_companion_iclk_off(device);
	if (ret) {
		err("fimc_is_companion_iclk_off failed(%d)", ret);
		goto p_err;
	}

	/* companion clock off */
	ret = fimc_is_companion_mclk_off(device);
	if (ret) {
		err("fimc_is_companion_mclk_off failed(%d)", ret);
		goto p_err;
	}

p_err:
	info("[COMP:D] %s(%d)\n", __func__, ret);
err_dev_null:
	return ret;
}

int fimc_is_companion_runtime_resume(struct device *dev)
{
	struct fimc_is_device_companion *device;
	struct fimc_is_core *core;
	int ret = 0;

	core = dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core is NULL");
		return -EINVAL;
	}
	device = dev_get_drvdata(dev);
	if (!device) {
		err("device is NULL");
		return -EINVAL;
	}

	/* Sensor clock on */
	ret = fimc_is_companion_mclk_on(device);
	if (ret) {
		err("fimc_is_companion_mclk_on failed(%d)", ret);
		goto p_err;
	}

	/* gpio init */
	ret = fimc_is_companion_gpio_on(device);
	if (ret) {
		err("fimc_is_companion_gpio_on failed(%d)", ret);
		goto p_err;
	}

	/* periperal internal clock on */
	ret = fimc_is_companion_iclk_on(device);
	if (ret) {
		err("fimc_is_companion_iclk_on failed(%d)", ret);
		goto p_err;
	}
p_err:
	info("[COMP:D] %s(%d)\n", __func__, ret);
	return ret;
}

static const struct dev_pm_ops fimc_is_companion_pm_ops = {
	.suspend		= fimc_is_companion_suspend,
	.resume			= fimc_is_companion_resume,
	.runtime_suspend	= fimc_is_companion_runtime_suspend,
	.runtime_resume		= fimc_is_companion_runtime_resume,
};

static int fimc_is_companion_probe(struct i2c_client *client)
{
	static bool probe_retried = false;
	struct device *dev = &client->dev;
	struct fimc_is_device_companion *device;
	struct fimc_is_core *core;
	int ret;

	if (!fimc_is_dev)
		goto probe_defer;

	core = dev_get_drvdata(fimc_is_dev);
	if (!core)
		goto probe_defer;

	device = devm_kzalloc(dev, sizeof(*device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;

	init_waitqueue_head(&device->init_wait_queue);

	device->companion_status = FIMC_IS_COMPANION_IDLE;

	ret = fimc_is_companion_parse_dt(dev);
	if (ret) {
		err("parsing device tree failed(%d)", ret);
		goto probe_defer;
	}

	device->dev = dev;
	device->i2c_client = client;
	device->private_data = core;
	device->regs = core->regs;
	device->pdata = dev_get_platdata(dev);
	dev_set_drvdata(dev, device);
	device_init_wakeup(dev, true);
	core->companion = device;
	core->pin_ois_en = device->pdata->pin_ois_en;

	/* init state */
	clear_bit(FIMC_IS_COMPANION_OPEN, &device->state);
	clear_bit(FIMC_IS_COMPANION_MCLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_ICLK_ON, &device->state);
	clear_bit(FIMC_IS_COMPANION_GPIO_ON, &device->state);

	device->v4l2_dev = &core->v4l2_dev;

	ret = fimc_is_mem_probe(&device->mem, core->pdev);
	if (ret) {
		err("fimc_is_mem_probe failed(%d)", ret);
		goto probe_defer;
	}

	ret = fimc_is_comp_video_probe(device);
	if (ret) {
		err("fimc_is_companion_video_probe failed(%d)", ret);
		goto probe_defer;
	}

	pm_runtime_enable(dev);

	info("[COMP:D] %s(%d)\n", __func__, ret);

	pr_info("%s %s: fimc_is_i2c0 driver probed!\n",
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

probe_defer:
	if (probe_retried) {
		err("probe has already been retried!!");
		BUG();
	}

	probe_retried = true;
	err("core device is not yet probed");
	return -EPROBE_DEFER;
}

static int fimc_is_companion_remove(struct i2c_client *client)
{
	return 0;
}

static struct of_device_id fimc_is_companion_of_match[] = {
	{ .compatible = "samsung,s5c73c1" },
	{ },
};
MODULE_DEVICE_TABLE(of, fimc_is_i2c0_dt_ids);

static struct i2c_driver fimc_is_i2c0_driver = {
	.driver = {
		.name = "fimc-is-companion",
		.of_match_table = fimc_is_companion_of_match,
		.pm = &fimc_is_companion_pm_ops,
	},
	.probe_new = fimc_is_companion_probe,
	.remove = fimc_is_companion_remove,
};
module_i2c_driver(fimc_is_i2c0_driver);

MODULE_AUTHOR("Wooki Min<wooki.min@samsung.com>");
MODULE_DESCRIPTION("Exynos FIMC_IS_COMPANION driver");
MODULE_LICENSE("GPL");
