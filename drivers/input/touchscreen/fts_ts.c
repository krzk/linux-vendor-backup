/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

#include "fts_ts.h"

int fts_write_reg(struct fts_ts_info *info,
		unsigned char *reg, unsigned short num_com)
{
	struct i2c_client *client = info->client;
	struct i2c_msg xfer_msg[2];
	int ret;

	if (info->touch_stopped) {
		dev_err(&client->dev, "Sensor stopped\n");
		return 0;
	}

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = num_com;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	ret = i2c_transfer(client->adapter, xfer_msg, 1);

	return ret;
}

int fts_read_reg(struct fts_ts_info *info, unsigned char *reg, int cnum,
		 unsigned char *buf, int num)
{
	struct i2c_client *client = info->client;
	struct i2c_msg xfer_msg[2];
	int ret;

	if (info->touch_stopped) {
		dev_err(&client->dev, "Sensor stopped\n");
		return 0;
	}

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = buf;

	ret = i2c_transfer(client->adapter, xfer_msg, 2);

	return ret;
}

void fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	struct i2c_client *client = info->client;
	unsigned char reg_addr = 0;
	int ret = 0;

	reg_addr = cmd;
	ret = fts_write_reg(info, &reg_addr, 1);

	dev_dbg(&client->dev, "FTS Command (%02X) , ret = %d\n", cmd, ret);
}

void fts_systemreset(struct fts_ts_info *info)
{
	struct i2c_client *client = info->client;
	unsigned char reg_addr[4] = { 0xB6, 0x00, 0x23, 0x01 };

	dev_dbg(&client->dev, "FTS SystemReset\n");

	fts_write_reg(info, &reg_addr[0], 4);
	msleep(20);
}

static void fts_interrupt_set(struct fts_ts_info *info, int enable)
{
	struct i2c_client *client = info->client;
	unsigned char reg_addr[4] = { 0xB6, 0x00, 0x1C, enable };

	if (enable)
		dev_dbg(&client->dev, "FTS INT Enable\n");
	else
		dev_dbg(&client->dev, "FTS INT Disable\n");

	fts_write_reg(info, &reg_addr[0], 4);
}

static void fts_get_version_info(struct fts_ts_info *info)
{
	struct i2c_client *client = info->client;
	unsigned char reg_addr[3];
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;

	fts_command(info, FTS_CMD_RELEASEINFO);

	memset(data, 0x00, FTS_EVENT_SIZE);

	reg_addr[0] = READ_ONE_EVENT;

	while (fts_read_reg(info, &reg_addr[0], 1, (unsigned char *)data,
				FTS_EVENT_SIZE)) {
		if (data[0] == EVENTID_INTERNAL_RELEASE_INFO) {
			info->fw_ver_ic = (data[3] << 8) + data[4];
			info->config_ver_ic = (data[6] << 8) + data[5];
		} else if (data[0] == EVENTID_EXTERNAL_RELEASE_INFO) {
			info->fw_main_ver_ic = (data[1] << 8) + data[2];
			break;
		}

		if (retry++ > FTS_RETRY_COUNT) {
			dev_err(&client->dev,
					"Time out to get ic information\n");
			break;
		}
	}

	dev_info(&client->dev,
		"FW ver: 0x%04x, Config ver: 0x%04x, FW main ver 0x%04x\n",
		info->fw_ver_ic, info->config_ver_ic, info->fw_main_ver_ic);
}

static int fts_wait_for_ready(struct fts_ts_info *info)
{
	int rc;
	unsigned char reg_addr;
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;
	int err_cnt = 0;

	memset(data, 0x0, FTS_EVENT_SIZE);

	reg_addr = READ_ONE_EVENT;
	rc = -1;
	while (fts_read_reg(info, &reg_addr, 1,
				(unsigned char *)data, FTS_EVENT_SIZE)) {

		if (data[0] == EVENTID_CONTROLLER_READY) {
			rc = 0;
			break;
		}

		if (data[0] == EVENTID_ERROR) {
			if (err_cnt++ > 32) {
				rc = -FTS_ERROR_EVENT_ID;
				break;
			}
			continue;
		}

		if (retry++ > FTS_RETRY_COUNT) {
			rc = -FTS_ERROR_TIMEOUT;
			dev_err(&info->client->dev, "Time Over\n");
			break;
		}
		msleep(20);
	}

	return rc;
}

static int fts_init(struct fts_ts_info *info)
{
	struct i2c_client *client = info->client;
	unsigned char val[16];
	unsigned char reg_addr[8];
	int rc;

	fts_systemreset(info);

	rc = fts_wait_for_ready(info);
	if (rc == -FTS_ERROR_EVENT_ID)
		dev_err(&client->dev, "Failed to wait for ready\n");

	fts_get_version_info(info);

	fts_command(info, SLEEPOUT);
	fts_command(info, SENSEON);
	fts_command(info, FORCECALIBRATION);
	fts_command(info, FLUSHBUFFER);

	fts_interrupt_set(info, INT_ENABLE);

	memset(val, 0x0, 4);
	reg_addr[0] = READ_STATUS;
	fts_read_reg(info, reg_addr, 1, (unsigned char *)val, 4);

	dev_dbg(&client->dev, "FTS Initialized\n");

	return 0;
}

void fts_release_all_finger(struct fts_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;

	for (i = 0; i < FINGER_MAX; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

		if ((info->finger[i].state == EVENTID_ENTER_POINTER) ||
			(info->finger[i].state == EVENTID_MOTION_POINTER)) {

			dev_dbg(&client->dev,
				"[RA] tID:%d mc: %d\n",
				i, info->finger[i].mcount);
		}

		info->finger[i].state = EVENTID_LEAVE_POINTER;
		info->finger[i].mcount = 0;
	}

	input_report_key(info->input_dev, BTN_TOUCH, 0);
	input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

	input_sync(info->input_dev);
}

static unsigned char fts_event_handler_type_b(struct fts_ts_info *info,
		unsigned char data[], unsigned char left_event)
{
	struct i2c_client *client = info->client;
	unsigned char id = 0, event_id = 0;
	unsigned char last_left_event = 0;
	int x = 0, y = 0;
	int bw = 0, bh = 0, palm = 0, sumsize = 0;

	event_id = data[0] & 0x0F;

	switch (event_id) {
	case EVENTID_MOTION_POINTER:
		x = data[1] + ((data[2] & 0x0f) << 8);
		y = ((data[2] & 0xf0) >> 4) + (data[3] << 4);
		bw = data[4];
		bh = data[5];
		palm = (data[6] >> 7) & 0x01;
		sumsize = (data[6] & 0x7f) << 1;

		input_mt_slot(info->input_dev, id);
		input_mt_report_slot_state(info->input_dev,
				MT_TOOL_FINGER, 1 + (palm << 1));
		input_report_key(info->input_dev, BTN_TOUCH, 1);
		input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
		input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);

		input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR,
								max(bw, bh));
		input_report_abs(info->input_dev, ABS_MT_TOUCH_MINOR,
								min(bw, bh));

		dev_dbg(&client->dev, "Pressed x: %d, y: %d\n", x, y);
		break;
	case EVENTID_LEAVE_POINTER:
		input_mt_slot(info->input_dev, id);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
		input_report_key(info->input_dev, BTN_TOUCH, 0);

		dev_dbg(&client->dev, "Released\n");
		break;
	default:
		break;
	}

	if (event_id == EVENTID_ENTER_POINTER)
		dev_dbg(&client->dev, "[P] id: %d\n", id);
	else if (event_id == EVENTID_LEAVE_POINTER) {
		dev_dbg(&client->dev, "[R] id: %d mc: %d\n",
				id, info->finger[id].mcount);
		info->finger[id].mcount = 0;
	} else if (event_id == EVENTID_MOTION_POINTER)
		info->finger[id].mcount++;

	info->finger[id].state = event_id;
	input_sync(info->input_dev);

	return last_left_event;
}

static irqreturn_t fts_interrupt_handler(int irq, void *handle)
{
	struct fts_ts_info *info = handle;
	unsigned char reg_addr[4] = {0xb6, 0x00, 0x45, READ_ALL_EVENT};
	unsigned short evtcount = 0;

	evtcount = 0;
	fts_read_reg(info, &reg_addr[0], 3, (unsigned char *)&evtcount, 2);
	evtcount = evtcount >> 10;

	if (evtcount > FTS_FIFO_MAX)
		evtcount = FTS_FIFO_MAX;

	if (evtcount > 0) {
		memset(info->data, 0x0, FTS_EVENT_SIZE * evtcount);
		fts_read_reg(info, &reg_addr[3], 1, (unsigned char *)info->data,
				FTS_EVENT_SIZE * evtcount);
		fts_event_handler_type_b(info, info->data, evtcount);
	}

	return IRQ_HANDLED;
}

static int fts_power_ctrl(void *data, bool on)
{
	struct fts_ts_info *info = (struct fts_ts_info *)data;
	const struct fts_i2c_platform_data *pdata = info->board;
	struct device *dev = &info->client->dev;
	int retval = 0;

	if (info->enabled == on)
		return retval;

	dev_dbg(dev, "Touchscreen power ctrl: %s\n", on ? "on" : "off");

	if (on) {
		retval = regulator_enable(pdata->vdd);
		if (retval) {
			dev_err(dev, "Failed to enable vdd: %d\n", retval);
			return retval;
		}
		retval = regulator_enable(pdata->avdd);
		if (retval) {
			dev_err(dev, "Failed to enable avdd: %d\n", retval);
			return retval;
		}
		msleep(20);
	} else {
		if (regulator_is_enabled(pdata->vdd))
			regulator_disable(pdata->vdd);
		if (regulator_is_enabled(pdata->avdd))
			regulator_disable(pdata->avdd);
	}

	info->enabled = on;

	return retval;
}

static void fts_reinit(struct fts_ts_info *info)
{
	fts_wait_for_ready(info);

	fts_systemreset(info);

	fts_wait_for_ready(info);

	fts_command(info, SLEEPOUT);
	msleep(50);

	fts_command(info, SENSEON);
	msleep(50);

	fts_command(info, FLUSHBUFFER);
	fts_interrupt_set(info, INT_ENABLE);
}

static int fts_start_device(struct fts_ts_info *info)
{
	struct i2c_client *client = info->client;

	if (!info->touch_stopped) {
		dev_err(&client->dev, "already power on\n");
		return 0;
	}

	fts_release_all_finger(info);

	if (info->board->power)
		info->board->power(info, true);

	info->touch_stopped = false;
	info->reinit_done = false;
	fts_reinit(info);
	info->reinit_done = true;
	enable_irq(client->irq);

	return 0;
}

static int fts_stop_device(struct fts_ts_info *info)
{
	struct i2c_client *client = info->client;

	if (info->touch_stopped) {
		dev_err(&client->dev, "already power off\n");
		return 0;
	}

	fts_interrupt_set(info, INT_DISABLE);
	disable_irq(client->irq);

	fts_command(info, FLUSHBUFFER);
	fts_command(info, SLEEPIN);
	fts_release_all_finger(info);
	info->touch_stopped = true;

	if (info->board->power)
		info->board->power(info, false);

	return 0;
}

#ifdef USE_OPEN_CLOSE
static int fts_input_open(struct input_dev *dev)
{
	struct fts_ts_info *info = input_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int retval;

	retval = fts_start_device(info);
	if (retval < 0)
		dev_err(&client->dev, "Failed to start device\n");

	return 0;
}

static void fts_input_close(struct input_dev *dev)
{
	struct fts_ts_info *info = input_get_drvdata(dev);

	fts_stop_device(info);

}
#endif

static struct fts_i2c_platform_data *fts_parse_dt(struct device *dev)
{
	struct fts_i2c_platform_data *pdata;
	struct device_node *np = dev->of_node;

	pdata = devm_kzalloc(dev, sizeof(struct fts_i2c_platform_data),
								GFP_KERNEL);
	if (!pdata)
		return NULL;

	if (of_property_read_u32(np, "x-size", &pdata->max_x)) {
		dev_err(dev, "failed to get x-size property\n");
		return NULL;
	};

	if (of_property_read_u32(np, "y-size", &pdata->max_y)) {
		dev_err(dev, "failed to get y-size property\n");
		return NULL;
	};

	return pdata;
}

static int fts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fts_i2c_platform_data *pdata;
	struct fts_ts_info *info;
	static char fts_ts_phys[64] = { 0 };
	int retval;
	int i = 0;

	pdata = dev_get_platdata(&client->dev);
	if (!pdata)
		pdata = fts_parse_dt(&client->dev);

	if (!pdata) {
		dev_err(&client->dev, "Need platform data\n");
		return -EINVAL;
	}

	info = devm_kzalloc(&client->dev,
			sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Failed to check i2c functionality.\n");
		return -ENODEV;
	}

	pdata->power = fts_power_ctrl;
	info->client = client;
	info->board = pdata;
	info->irq_enabled = false;
	info->touch_stopped = false;

	i2c_set_clientdata(client, info);

	pdata->vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(pdata->vdd)) {
		retval = PTR_ERR(pdata->vdd);
		dev_err(&client->dev,
			"Unable to get the IO regulator (%d)\n", retval);
		return retval;
	}

	pdata->avdd = devm_regulator_get(&client->dev, "avdd");
	if (IS_ERR(pdata->avdd)) {
		retval = PTR_ERR(pdata->avdd);
		dev_err(&client->dev,
			"Unable to get the Core regulator (%d)\n", retval);
		return retval;
	}

	info->dev = &info->client->dev;
	info->input_dev = devm_input_allocate_device(&client->dev);
	if (!info->input_dev)
		return -ENOMEM;

	info->input_dev->dev.parent = &client->dev;
	info->input_dev->name = "sec_touchscreen";
	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input0",
		 info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = BUS_I2C;
#ifdef USE_OPEN_CLOSE
	info->input_dev->open = fts_input_open;
	info->input_dev->close = fts_input_close;
#endif
	__set_bit(EV_ABS, info->input_dev->evbit);
	__set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(BTN_TOUCH, info->input_dev->keybit);

	input_mt_init_slots(info->input_dev, FINGER_MAX, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			0, info->board->max_x, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			0, info->board->max_y, 0, 0);
	input_set_abs_params(info->input_dev, ABS_X,
			0, info->board->max_x, 0, 0);
	input_set_abs_params(info->input_dev, ABS_Y,
			0, info->board->max_y, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
				 0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
				 0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_DISTANCE,
				 0, 255, 0, 0);

	input_set_drvdata(info->input_dev, info);
	i2c_set_clientdata(client, info);

	if (info->board->power)
		info->board->power(info, true);

	retval = fts_init(info);
	info->reinit_done = true;

	if (retval) {
		dev_err(&client->dev, "FTS fts_init fail!\n");
		goto err_fts_init;
	}

	retval = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, fts_interrupt_handler,
				IRQF_ONESHOT | IRQF_TRIGGER_LOW,
				FTS_TS_DRV_NAME, info);

	if (retval) {
		dev_err(&client->dev, "Failed to enable attention interrupt\n");
		goto err_fts_init;
	}

	retval = input_register_device(info->input_dev);
	if (retval) {
		dev_err(&client->dev, "FTS input_register_device fail!\n");
		goto err_fts_init;
	}

	for (i = 0; i < FINGER_MAX; i++) {
		info->finger[i].state = EVENTID_LEAVE_POINTER;
		info->finger[i].mcount = 0;
	}

	return 0;

err_fts_init:
	info->board->power(info, false);
	return retval;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	if (info->irq_enabled) {
		disable_irq(client->irq);
		free_irq(client->irq, info);
		info->irq_enabled = false;
	}

	input_mt_destroy_slots(info->input_dev);

	input_unregister_device(info->input_dev);
	info->input_dev = NULL;

	info->board->power(info, false);

	kfree(info);

	return 0;
}

static void fts_shutdown(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	fts_stop_device(info);
}

static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users)
		fts_stop_device(info);

	mutex_unlock(&info->input_dev->mutex);

	return 0;
}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users)
		fts_start_device(info);

	mutex_unlock(&info->input_dev->mutex);

	return 0;
}

static SIMPLE_DEV_PM_OPS(fts_dev_pm_ops, fts_pm_suspend, fts_pm_resume);

static const struct i2c_device_id fts_device_id[] = {
	{ "fts_touch",  0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fts_device_id);

static const struct of_device_id fts_match_table[] = {
	{ .compatible = "stm,fts_touch",},
	{ },
};

static struct i2c_driver fts_i2c_driver = {
	.driver = {
		.name = FTS_TS_DRV_NAME,
		.of_match_table = of_match_ptr(fts_match_table),
		.pm = &fts_dev_pm_ops,

	},
	.probe = fts_probe,
	.remove = fts_remove,
	.shutdown = fts_shutdown,
	.id_table = fts_device_id,
};

module_i2c_driver(fts_i2c_driver);

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics, Inc.");
MODULE_LICENSE("GPL v2");
