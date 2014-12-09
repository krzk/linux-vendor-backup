/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "ssp.h"
#include <linux/of_gpio.h>

extern unsigned int system_rev;

u8 ssp_magnetic_pdc[] = {110, 85, 171, 71, 203, 195, 0, 67,\
			208, 56, 175, 244, 206, 213, 0, 92, 250, 0,\
			55, 48, 189, 252, 171, 243, 13, 45, 250};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssp_early_suspend(struct early_suspend *handler);
static void ssp_late_resume(struct early_suspend *handler);
#endif

void ssp_enable(struct ssp_data *data, bool enable)
{
	ssp_dbg("enable = %d, old enable = %d\n",enable, data->bSspShutdown);

	if (enable && data->bSspShutdown) {
		data->bSspShutdown = false;
		enable_irq(data->iIrq);
		enable_irq_wake(data->iIrq);
	} else if (!enable && !data->bSspShutdown) {
		data->bSspShutdown = true;
		disable_irq(data->iIrq);
		disable_irq_wake(data->iIrq);
	} else {
		ssp_err("error / enable = %d, old enable = %d\n",
					enable, data->bSspShutdown);
	}
}
/************************************************************************/
/* interrupt happened due to transition/change of SSP MCU		*/
/************************************************************************/

static irqreturn_t sensordata_irq_thread_fn(int iIrq, void *dev_id)
{
	struct ssp_data *data = dev_id;

	select_irq_msg(data);
	data->uIrqCnt++;

	return IRQ_HANDLED;
}

/*************************************************************************/
/* initialize sensor hub						 */
/*************************************************************************/

static void initialize_variable(struct ssp_data *data)
{
	int iSensorIndex;

	for (iSensorIndex = 0; iSensorIndex < SENSOR_MAX; iSensorIndex++) {
		data->adDelayBuf[iSensorIndex] = DEFUALT_POLLING_DELAY;
		data->batchLatencyBuf[iSensorIndex] = 0;
		data->batchOptBuf[iSensorIndex] = 0;
		data->aiCheckStatus[iSensorIndex] = INITIALIZATION_STATE;
	}

	data->adDelayBuf[BIO_HRM_LIB] = (100 * NSEC_PER_MSEC);

	atomic_set(&data->aSensorEnable, 0);
	data->iLibraryLength = 0;
	data->uSensorState = 0;

	data->uResetCnt = 0;
	data->uTimeOutCnt = 0;
	data->uComFailCnt = 0;
	data->uIrqCnt = 0;

	data->bSspShutdown = true;
	data->bAccelAlert = false;
	data->bLpModeEnabled = false;
	data->bTimeSyncing = true;

	data->accelcal.x = 0;
	data->accelcal.y = 0;
	data->accelcal.z = 0;

	data->gyrocal.x = 0;
	data->gyrocal.y = 0;
	data->gyrocal.z = 0;

	data->uGyroDps = GYROSCOPE_DPS500;
	data->uIr_Current = DEFUALT_IR_CURRENT;

	data->mcu_device = NULL;
	data->acc_device = NULL;
	data->gyro_device = NULL;
#ifdef CONFIG_SENSORS_SSP_ADPD142
	data->hrm_device = NULL;
#endif

	data->voice_device = NULL;
	data->bMcuDumpMode = ssp_check_sec_dump_mode();
	INIT_LIST_HEAD(&data->pending_list);

	initialize_function_pointer(data);
}

int initialize_mcu(struct ssp_data *data)
{
	int iRet = 0;

	clean_pending_list(data);

	iRet = get_chipid(data);
	ssp_info("MCU device ID = %d, reading ID = %d\n",
						DEVICE_ID, iRet);
	if (iRet != DEVICE_ID) {
		if (iRet < 0) {
			ssp_err("MCU is not working : 0x%x\n", iRet);
		} else {
			ssp_err("MCU identification failed\n");
			iRet = -ENODEV;
		}
		goto out;
	}

	iRet = set_sensor_position(data);
	if (iRet < 0) {
		ssp_err("set_sensor_position failed\n");
		goto out;
	}

	iRet = set_magnetic_static_matrix(data);
	if (iRet < 0)
		ssp_err("set_magnetic_static_matrix failed\n");

	iRet = get_fuserom_data(data);
	if (iRet < 0)
		ssp_err("get_fuserom_data failed\n");

	data->uSensorState = get_sensor_scanning_info(data);
	if (data->uSensorState == 0) {
		ssp_err("get_sensor_scanning_info failed\n");
		iRet = ERROR;
		goto out;
	}
	data->uCurFirmRev = get_firmware_rev(data);
	ssp_info("MCU Firm Rev : New = %8u\n", data->uCurFirmRev);

	iRet = ssp_send_cmd(data, MSG2SSP_AP_MCU_DUMP_CHECK, 0);
out:
	return iRet;
}

static int initialize_irq(struct ssp_data *data)
{
	int iRet;

	data->iIrq = gpio_to_irq(data->mcu_int1);
	if (data->iIrq < 0) {
		ssp_err("Failed to requesting IRQ\n");
		return -ENXIO;
	}

	iRet = request_threaded_irq(data->iIrq, NULL, sensordata_irq_thread_fn,
				    IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
				    "SSP_Int", data);
	if (iRet < 0) {
		ssp_err("request_irq(%d) failed for gpio %d (%d)\n",
						data->iIrq, data->iIrq, iRet);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(data->iIrq);
	return 0;

err_request_irq:
	return iRet;
}

static void work_function_firmware_update(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
				struct ssp_data, work_firmware);
	int iRet;

	ssp_info("firmware update...\n");

	iRet = forced_to_download_binary(data, KERNEL_BINARY);
	if (iRet < 0) {
		ssp_err("forced_to_download_binary failed!\n");
		return;
	}

	queue_refresh_task(data, SSP_SW_RESET_TIME);

	if (data->check_lpmode() == true) {
		data->bLpModeEnabled = true;
		ssp_dbg("LPM Charging...\n");
	} else {
		data->bLpModeEnabled = false;
		ssp_dbg("Normal Booting OK\n");
	}

	ssp_info("firmware update done!\n");
}

static void work_function_mcu_state(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work*)work,
				struct ssp_data, work_mcu_state);
	int iRet;

	ssp_info("MCU state check!\n");

	/* check boot loader binary */
	data->fw_dl_state = check_fwbl(data);
	switch (data->fw_dl_state) {
	case FW_DL_STATE_FAIL:
		data->bSspShutdown = true;
		break;
	case FW_DL_STATE_NONE:
		iRet = initialize_mcu(data);
		if (iRet < SUCCESS) {
			ssp_err("initialize_mcu retry\n");
			data->uResetCnt++;
			toggle_mcu_reset(data);
			msleep(SSP_SW_RESET_TIME);
			initialize_mcu(data);
		}
		break;
	case FW_DL_STATE_NEED_TO_SCHEDULE:
		ssp_info("Firmware update is scheduled\n");
		schedule_delayed_work(&data->work_firmware,
			msecs_to_jiffies(1000));
		data->fw_dl_state = FW_DL_STATE_SCHEDULED;
		break;
	default:
		break;
	}
}

static int check_ap_rev(void)
{
	return system_rev;
}

static int ssp_check_lpmode(void)
{
	/* FIXME: sensor-hub must check lpmode for low-power mode.
	 * but, the following code was hardcoded. So, following
	 * comment have to be modified after resolving the hardcoded issue.
	 */
	/*
	if (lpcharge == 0x01)
		return true;
	else
		return false;
	*/
	return false;
}

static int initialize_gpio(struct device *dev, struct ssp_data *pdata)
{
	int ret;

	ret = devm_gpio_request_one(dev,
			pdata->mcu_int1, GPIOF_IN, "mcu-ap-int1");
	if (ret) {
		ssp_err("Cannot request mcu-int1 gpio\n");
		return ret;
	}

	ret = devm_gpio_request_one(dev,
			pdata->mcu_int2, GPIOF_IN, "mcu-ap-int2");
	if (ret) {
		ssp_err("Cannot request mcu-int2 gpio\n");
		return ret;
	}

	ret = devm_gpio_request_one(dev,
			pdata->ap_int, GPIOF_OUT_INIT_HIGH, "ap-mcu-int");
	if (ret) {
		ssp_err("Cannot request ap-int gpio\n");
		return ret;
	}

	ret = devm_gpio_request_one(dev,
			pdata->rst, GPIOF_OUT_INIT_HIGH, "mcu-reset");
	if (ret) {
		ssp_err("Cannot request rst gpio\n");
		return ret;
	}

	return 0;
}

static int ssp_parse_dt(struct device *dev, struct ssp_data *pdata)
{
	struct device_node *node = dev->of_node;
	int iRet;

	pdata->mcu_int1 = of_get_named_gpio(node, "mcu-ap-int1", 0);
	if (pdata->mcu_int1 < 0) {
		ssp_err("Cannot get mcu-ap-int1 gpio\n");
		return -EINVAL;
	}

	pdata->mcu_int2 = of_get_named_gpio(node, "mcu-ap-int2", 0);
	if (pdata->mcu_int2 < 0) {
		ssp_err("Cannot get mcu-ap-int2 gpio\n");
		return -EINVAL;
	}

	pdata->ap_int = of_get_named_gpio(node, "ap-mcu-int", 0);
	if (pdata->ap_int < 0) {
		ssp_err("Cannot get ap-mcu-int gpio\n");
		return -EINVAL;
	}

	pdata->rst = of_get_named_gpio(node, "mcu-reset", 0);
	if (pdata->rst < 0) {
		ssp_err("Cannot get mcu-reset gpio\n");
		return -EINVAL;
	}

	iRet = of_property_read_u32(node, "ssp,acc-position",
				&pdata->accel_position);
	if (iRet)
		pdata->accel_position = 0;

	iRet = of_property_read_u32(node, "ssp,mag-position",
				&pdata->mag_position);
	if (iRet)
		pdata->mag_position = 0;

	ssp_info("position : accel = %d, mag = %d\n",
		pdata->accel_position, pdata->mag_position);

	return 0;
}

static int ssp_probe(struct spi_device *spi)
{
	struct ssp_data *data;
	struct ssp_platform_data *pdata;
	int iRet = 0;

	ssp_info(" is called\n");

	data = devm_kzalloc(&spi->dev, sizeof(struct ssp_data), GFP_KERNEL);
	if (!data) {
		ssp_err("failed to allocate memory for data\n");
		goto err_setup;
	}

	if (spi->dev.of_node) {
		iRet = ssp_parse_dt(&spi->dev, data);
		if (iRet) {
			ssp_err("Failed to parse dt\n");
			goto err_setup;
		}

		iRet = initialize_gpio(&spi->dev, data);
		if (iRet) {
			ssp_err("Failed to initialize gpio\n");
			goto err_setup;
		}

		data->check_lpmode = ssp_check_lpmode;
		data->ap_rev = check_ap_rev();
		ssp_info("ap_rev = %d", data->ap_rev);

		/* Get sensor positon */
		data->mag_matrix_size = ARRAY_SIZE(ssp_magnetic_pdc);
		data->mag_matrix = ssp_magnetic_pdc;

	} else {
		pdata = spi->dev.platform_data;
		if (pdata == NULL) {
			ssp_err("platform_data is null\n");
			return -ENOMEM;
		}

		data->rst = pdata->rst;
		data->ap_int = pdata->ap_int;
		data->mcu_int1 = pdata->mcu_int1;
		data->mcu_int2 = pdata->mcu_int2;
		data->iIrq = pdata->irq;
		data->check_lpmode = pdata->check_lpmode;
#ifdef CONFIG_SENSORS_SSP_ADPD142
		data->hrm_sensor_power = pdata->hrm_sensor_power;
#endif

		ssp_dbg("rst = %d, ap_int = %d,"\
			" mcu_int1 = %d, mcu_int2 = %d\n",
			(int)data->rst, (int)data->ap_int,
			(int)data->mcu_int1, (int)data->mcu_int2);

		/* AP system_rev */
		if (pdata->check_ap_rev)
			data->ap_rev = pdata->check_ap_rev();
		else
			data->ap_rev = 0;
		ssp_info("system Rev = 0x%x\n", data->ap_rev);

		/* Get sensor positions */
		if (pdata->get_positions) {
			pdata->get_positions(&data->accel_position,
				&data->mag_position);
		} else {
			data->accel_position = 0;
			data->mag_position = 0;
		}
		if (pdata->mag_matrix) {
			data->mag_matrix_size = pdata->mag_matrix_size;
			data->mag_matrix = pdata->mag_matrix;
		}
	}

	spi->mode = SPI_MODE_1;
	if (spi_setup(spi)) {
		ssp_err("failed to setup spi\n");
		goto err_setup;
	}

	data->bProbeIsDone = false;
	data->fw_dl_state = FW_DL_STATE_NONE;
	data->spi = spi;
	spi_set_drvdata(spi, data);

#ifdef CONFIG_SENSORS_SSP_STM
	mutex_init(&data->comm_mutex);
	mutex_init(&data->pending_mutex);
#endif

	initialize_variable(data);
	INIT_DELAYED_WORK(&data->work_firmware, work_function_firmware_update);
	INIT_DELAYED_WORK(&data->work_mcu_state, work_function_mcu_state);

	iRet = initialize_input_dev(data);
	if (iRet < 0) {
		ssp_err("could not create input device\n");
		goto err_input_register_device;
	}

	iRet = initialize_debug_timer(data);
	if (iRet < 0) {
		ssp_err("could not create workqueue\n");
		goto err_create_workqueue;
	}

	iRet = intialize_lpm_motion(data);
	if (iRet < 0) {
		ssp_err("could not create workqueue\n");
		goto err_create_lpm_motion;
	}

	iRet = initialize_irq(data);
	if (iRet < 0) {
		ssp_err("could not create irq\n");
		goto err_setup_irq;
	}

	iRet = initialize_sysfs(data);
	if (iRet < 0) {
		ssp_err("could not create sysfs\n");
		goto err_sysfs_create;
	}

	iRet = initialize_event_symlink(data);
	if (iRet < 0) {
		ssp_err("could not create symlink\n");
		goto err_symlink_create;
	}

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	/* init sensorhub device */
	iRet = ssp_sensorhub_initialize(data);
	if (iRet < 0) {
		ssp_err("ssp_sensorhub_initialize err(%d)", iRet);
		ssp_sensorhub_remove(data);
	}
#endif
	ssp_enable(data, true);
	schedule_delayed_work(&data->work_mcu_state,
			msecs_to_jiffies(200));

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.suspend = ssp_early_suspend;
	data->early_suspend.resume = ssp_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
	ssp_info("probe success!\n");

	enable_debug_timer(data);
	data->bProbeIsDone = true;

	if (data->check_lpmode() == true) {
		ssp_charging_motion(data, 1);
		data->bLpModeEnabled = true;
		ssp_dbg("LPM Charging...\n");
	} else {
		data->bLpModeEnabled = false;
		ssp_dbg("Normal Booting OK\n");
	}

	return 0;

err_symlink_create:
	remove_sysfs(data);
err_sysfs_create:
	free_irq(data->iIrq, data);
err_setup_irq:
	destroy_workqueue(data->lpm_motion_wq);
err_create_lpm_motion:
	destroy_workqueue(data->debug_wq);
err_create_workqueue:
	remove_input_dev(data);
err_input_register_device:
#ifdef CONFIG_SENSORS_SSP_STM
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
#endif

err_setup:
	ssp_err("probe failed!\n");
	return iRet;
}

static void ssp_shutdown(struct spi_device *spi)
{
	struct ssp_data *data = spi_get_drvdata(spi);

	func_dbg();
	if (data->bProbeIsDone == false)
		return;

	if (data->fw_dl_state >= FW_DL_STATE_SCHEDULED &&
		data->fw_dl_state < FW_DL_STATE_DONE) {
		ssp_err("cancel_delayed_work_sync state = %d\n",
						data->fw_dl_state);
		cancel_delayed_work_sync(&data->work_firmware);
	}

	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_SHUTDOWN, 0))
		ssp_err("MSG2SSP_AP_STATUS_SHUTDOWN failed\n");

	ssp_enable(data, false);
	disable_debug_timer(data);

	clean_pending_list(data);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif

	free_irq(data->iIrq, data);

	remove_event_symlink(data);
	remove_sysfs(data);
	remove_input_dev(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	ssp_sensorhub_remove(data);
#endif

	del_timer_sync(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
	cancel_work_sync(&data->work_lpm_motion);
	destroy_workqueue(data->lpm_motion_wq);
	destroy_workqueue(data->debug_wq);

#ifdef CONFIG_SENSORS_SSP_STM
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
#endif
	toggle_mcu_reset(data);
	ssp_info(" done\n");
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ssp_early_suspend(struct early_suspend *handler)
{
	struct ssp_data *data;
	data = container_of(handler, struct ssp_data, early_suspend);

	func_dbg();
	disable_debug_timer(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	/* give notice to user that AP goes to sleep */
	ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_SLEEP);
	ssp_sleep_mode(data);
	data->uLastAPState = MSG2SSP_AP_STATUS_SLEEP;
#else
	if (atomic_read(&data->aSensorEnable) > 0)
		ssp_sleep_mode(data);
#endif
}

static void ssp_late_resume(struct early_suspend *handler)
{
	struct ssp_data *data;
	data = container_of(handler, struct ssp_data, early_suspend);

	func_dbg();
	enable_debug_timer(data);

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	/* give notice to user that AP goes to sleep */
	ssp_sensorhub_report_notice(data, MSG2SSP_AP_STATUS_WAKEUP);
	ssp_resume_mode(data);
	data->uLastAPState = MSG2SSP_AP_STATUS_WAKEUP;
#else
	if (atomic_read(&data->aSensorEnable) > 0)
		ssp_resume_mode(data);
#endif
}

#else /* no early suspend */

static int ssp_suspend(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ssp_data *data = spi_get_drvdata(spi);

	func_dbg();
	data->uLastResumeState = MSG2SSP_AP_STATUS_SUSPEND;
	disable_debug_timer(data);

	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_SUSPEND, 0))
		ssp_err("MSG2SSP_AP_STATUS_SUSPEND failed\n");
	data->bTimeSyncing = false;
	disable_irq(data->iIrq);
	return 0;
}

static int ssp_resume(struct device *dev)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ssp_data *data = spi_get_drvdata(spi);
	enable_irq(data->iIrq);
	func_dbg();
	enable_debug_timer(data);

	if (SUCCESS != ssp_send_cmd(data, MSG2SSP_AP_STATUS_RESUME, 0))
		ssp_err("MSG2SSP_AP_STATUS_RESUME failed\n");
	data->uLastResumeState = MSG2SSP_AP_STATUS_RESUME;

	return 0;
}

static const struct dev_pm_ops ssp_pm_ops = {
	.suspend = ssp_suspend,
	.resume = ssp_resume
};
#endif /* CONFIG_HAS_EARLYSUSPEND */

static struct of_device_id ssp_of_match[] = {
	{ .compatible = "samsung,ssp-spi", },
	{ },
};

static struct spi_driver ssp_driver = {
	.probe = ssp_probe,
	.shutdown = ssp_shutdown,
	.driver = {
#ifndef CONFIG_HAS_EARLYSUSPEND
		   .pm = &ssp_pm_ops,
#endif
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(ssp_of_match),
		   .name = "ssp-spi"
		},
};

module_spi_driver(ssp_driver);
MODULE_DESCRIPTION("ssp spi driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
