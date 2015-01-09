/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/regulator/driver.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/i2c/cypress-touchkey.h>

static int touchkey_led_status;
static int touchled_cmd_reversed;

static int cypress_touchkey_power(struct touchkey_i2c *tkey_i2c, int on)
{
	struct device *dev = &tkey_i2c->client->dev;
	int ret = 0;

	if (tkey_i2c->enabled == on) {
		dev_err(dev, "Power state is same.\n");
		return ret;
	}

	if (on) {
		ret = regulator_enable(tkey_i2c->regulator_pwr);
		if (ret) {
			dev_err(dev, "Failed to enable power regulator\n");
			return ret;
		}

		ret = regulator_enable(tkey_i2c->regulator_led);
		if (ret) {
			dev_err(dev, "Failed to enable power regulator\n");
			return ret;
		}
	} else {
		if (regulator_is_enabled(tkey_i2c->regulator_pwr))
			regulator_disable(tkey_i2c->regulator_pwr);

		if (regulator_is_enabled(tkey_i2c->regulator_led))
			regulator_disable(tkey_i2c->regulator_led);
	}

	if (regulator_is_enabled(tkey_i2c->regulator_pwr) == !!on &&
		regulator_is_enabled(tkey_i2c->regulator_led) == !!on) {
		tkey_i2c->enabled = on;
	} else {
		dev_err(dev, "regulator_is_enabled value error!\n");
		ret = -1;
	}

	return ret;
}
static int touchkey_stop(struct touchkey_i2c *tkey_i2c)
{
	struct i2c_client *client = tkey_i2c->client;
	struct touchkey_platform_data *pdata = tkey_i2c->pdata;
	int i;

	if (!tkey_i2c->enabled) {
		dev_err(&tkey_i2c->client->dev, "Touch key already disabled\n");
		goto err_stop_out;
	}

	disable_irq(client->irq);

	/* release keys */
	for (i = 0; i < 2; i++) {
		input_report_key(tkey_i2c->input_dev,
				 pdata->keycodes[i], 0);
	}
	input_sync(tkey_i2c->input_dev);

	tkey_i2c->status_update = false;

	if (touchkey_led_status == TK_CMD_LED_ON)
		touchled_cmd_reversed = 1;

	cypress_touchkey_power(tkey_i2c, false);

err_stop_out:
	return 0;
}

static int touchkey_start(struct touchkey_i2c *tkey_i2c)
{
	struct i2c_client *client = tkey_i2c->client;

	tkey_i2c->enabled = true;

	if (touchled_cmd_reversed) {
		touchled_cmd_reversed = 0;
		i2c_smbus_write_i2c_block_data(client, BASE_REG, 1,
						(u8 *) &touchkey_led_status);
		dev_err(&client->dev, "Turning LED is reserved\n");
		msleep(30);
	}

	return 0;
}
static irqreturn_t touchkey_interrupt(int irq, void *dev_id)
{
	struct touchkey_i2c *tkey_i2c = dev_id;
	struct touchkey_platform_data *pdata = tkey_i2c->pdata;
	struct i2c_client *client = tkey_i2c->client;
	u8 data[3];
	int ret;
	int keycode_type = 0;
	int pressed;

	ret = i2c_smbus_read_i2c_block_data(client, KEYCODE_REG, 1, data);
	if (ret < 0)
		return IRQ_HANDLED;

	keycode_type = (data[0] & TK_BIT_KEYCODE);
	pressed = !(data[0] & TK_BIT_PRESS_EV);

	if (keycode_type <= 0 || keycode_type > 2) {
		dev_dbg(&tkey_i2c->client->dev, "keycode_type err\n");
		return IRQ_HANDLED;
	}

	input_report_key(tkey_i2c->input_dev,
			 pdata->keycodes[keycode_type - 1], pressed);
	input_sync(tkey_i2c->input_dev);

	dev_dbg(&client->dev, "pressed:%d, %d\n",
			pressed, pdata->keycodes[keycode_type - 1]);

	return IRQ_HANDLED;
}

static struct touchkey_platform_data *cypress_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct touchkey_platform_data *pdata;
	struct device_node *np = dev->of_node;
	int ret;

	if (!np)
		return ERR_PTR(-ENOENT);

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	dev->platform_data = pdata;

	ret = of_property_read_u32(np, "cypress,ic-stabilizing-time",
						&pdata->stabilizing_time);
	if (ret) {
		dev_err(dev, "Failed to ic-stabilizing-time %d\n", ret);
		pdata->stabilizing_time = 150;
	}

	if (of_find_property(np, "cypress,led_by_ldo", NULL))
		pdata->led_by_ldo = true;

	of_property_read_u32_array(np, "linux,code",
			pdata->keycodes, ARRAY_SIZE(pdata->keycodes));

	return pdata;
}

static int i2c_touchkey_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct touchkey_platform_data *pdata;
	struct touchkey_i2c *tkey_i2c;
	struct input_dev *input_dev;
	int i;
	int ret = 0;

	if (&client->dev.of_node)
		pdata = cypress_parse_dt(client);
	else
		pdata = client->dev.platform_data;

	if (IS_ERR(pdata)) {
		dev_err(&client->dev, "Failed to get plaform data\n");
		return PTR_ERR(pdata);
	}

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_I2C);
	if (!ret) {
		dev_err(&client->dev, "No I2C functionality found\n");
		return -ENODEV;
	}

	tkey_i2c = devm_kzalloc(&client->dev, sizeof(struct touchkey_i2c),
			GFP_KERNEL);
	if (!tkey_i2c)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(&client->dev);
	if (!input_dev)
		return -ENOMEM;

	input_dev->name = "sec_touchkey";
	input_dev->phys = "sec_touchkey/input0";
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;

	/*tkey_i2c*/
	tkey_i2c->pdata = pdata;
	tkey_i2c->input_dev = input_dev;
	tkey_i2c->client = client;
	tkey_i2c->name = "sec_touchkey";
	tkey_i2c->status_update = false;
	tkey_i2c->mc_data.cur_mode = MODE_NORMAL;

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_SYN, input_dev->evbit);

	for (i = 0; i < 2; i++)
		__set_bit(pdata->keycodes[i], input_dev->keybit);

	i2c_set_clientdata(client, tkey_i2c);
	input_set_drvdata(input_dev, tkey_i2c);

	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
					touchkey_interrupt,
					IRQF_TRIGGER_FALLING |
					IRQF_ONESHOT, tkey_i2c->name, tkey_i2c);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to request irq\n");
		return ret;
	}

	ret = input_register_device(input_dev);
	if (ret)
		return ret;

	tkey_i2c->regulator_pwr = devm_regulator_get(&client->dev,
			"VTOUCH_1.8V_AP");
	if (IS_ERR(tkey_i2c->regulator_pwr)) {
		dev_err(&client->dev, "Failed to get power regulator\n");
		return -EINVAL;
	}

	tkey_i2c->regulator_led = devm_regulator_get(&client->dev,
			"VTOUCH_LED_3.3V");
	if (IS_ERR(tkey_i2c->regulator_led)) {
		dev_err(&client->dev, "Failed to get led regulator\n");
		return -EINVAL;
	}

	ret = cypress_touchkey_power(tkey_i2c, true);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to enable power\n");
		return ret;
	}

	if (!tkey_i2c->pdata->boot_on_ldo)
		msleep(pdata->stabilizing_time);

	return 0;
}

void touchkey_shutdown(struct i2c_client *client)
{
	struct touchkey_i2c *tkey_i2c = i2c_get_clientdata(client);

	cypress_touchkey_power(tkey_i2c, false);
}

static int touchkey_suspend(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (tkey_i2c->input_dev->users)
		touchkey_stop(tkey_i2c);

	return 0;
}

static int touchkey_resume(struct device *dev)
{
	struct touchkey_i2c *tkey_i2c = dev_get_drvdata(dev);

	if (tkey_i2c->input_dev->users)
		touchkey_start(tkey_i2c);

	return 0;
}

static SIMPLE_DEV_PM_OPS(touchkey_pm_ops, touchkey_suspend, touchkey_resume);

static const struct i2c_device_id sec_touchkey_id[] = {
	{"sec_touchkey", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sec_touchkey_id);

static struct of_device_id cypress_touchkey_dt_ids[] = {
	{ .compatible = "cypress,cypress_touchkey" },
	{ }
};

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "cypress_touchkey",
		.pm = &touchkey_pm_ops,
		.of_match_table = of_match_ptr(cypress_touchkey_dt_ids),
	},
	.id_table = sec_touchkey_id,
	.probe = i2c_touchkey_probe,
	.shutdown = &touchkey_shutdown,
};

module_i2c_driver(touchkey_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("Touchkey driver for CYPRESS controller");
