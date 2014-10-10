/*
 * driver/ice4_fpga IRDA fpga driver
 *
 * Copyright (C) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include "ice4_irda.h"


#define IRDA_NAME		"ice4-irda"

#define IRDA_TEST_CODE_SIZE	141
#define IRDA_TEST_CODE_ADDR	0x00
#define MAX_SIZE		2048
#define READ_LENGTH		8

#define DUMMY_BIT_COUNT		49

struct ice4_fpga_data {
	struct i2c_client	*client;
	struct device		*dev;
	struct device		*sys_dev;
	struct regulator	*ir_regulator;
	struct mutex		mutex;
	struct clk		*mclk;
	struct work_struct	work_download;
	struct {
		unsigned char addr;
		unsigned char data[MAX_SIZE];
	} i2c_block_transfer;
	int count;
	int dev_id;
	int ir_freq;
	int ir_sum;

	int gpio_irda_irq;
	int gpio_creset;
	int gpio_fpga_rst_n;
	int gpio_cdone;
	int gpio_sda;
	int gpio_scl;

	struct class *sec_class;
};

static int ack_number;
static int count_number;

static int ice4_irda_check_cdone(struct ice4_fpga_data *data)
{
	int gpio_status = gpio_get_value(data->gpio_cdone);
	/* Device in Operation when CDONE='1'; Device Failed when CDONE='0'. */

	if (gpio_status != 1) {
		dev_info(data->dev, "CDONE_FAIL %d\n", gpio_status);
		return 0;
	}

	return 1;
}

/* When IR test does not work, we need to check some gpios' status */
static void print_fpga_gpio_status(struct ice4_fpga_data *data)
{
	dev_info(data->dev, "CDONE : %d\n",
				gpio_get_value(data->gpio_cdone));
	dev_info(data->dev, "RST_N : %d\n",
				gpio_get_value(data->gpio_fpga_rst_n));
	dev_info(data->dev, "CRESET_B : %d\n",
				gpio_get_value(data->gpio_creset));
}

/* sysfs node ir_send */
static void ir_remocon_work(struct ice4_fpga_data *data, int count)
{
	struct i2c_client *client = data->client;

	int buf_size = count + 2;
	int ret;
	int emission_time;
	int ack_pin_onoff;
	int retry_count = 0;

	data->i2c_block_transfer.addr = 0x00;

	data->i2c_block_transfer.data[0] = count >> 8;
	data->i2c_block_transfer.data[1] = count & 0xff;

	if (count_number >= 100)
		count_number = 0;

	count_number++;

	dev_info(data->dev, "total buf_size: %d\n", buf_size);

	mutex_lock(&data->mutex);

	buf_size++;
	ret = i2c_master_send(client,
		(unsigned char *) &(data->i2c_block_transfer), buf_size);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client,
		(unsigned char *) &(data->i2c_block_transfer), buf_size);
		if (ret < 0) {
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
			print_fpga_gpio_status(data);
		}
	}

	mdelay(10);

	ack_pin_onoff = 0;

	if (gpio_get_value(data->gpio_irda_irq)) {
		dev_info(data->dev, "%d Checksum NG!\n", count_number);
		ack_pin_onoff = 1;
	} else {
		dev_info(data->dev, "%d Checksum OK!\n", count_number);
		ack_pin_onoff = 2;
	}
	ack_number = ack_pin_onoff;

	mutex_unlock(&data->mutex);

	data->count = 2;

	emission_time = (1000 * (data->ir_sum) / (data->ir_freq));
	if (emission_time > 0)
		msleep(emission_time);

	dev_info(data->dev, "emission_time = %d\n", emission_time);

	while (!gpio_get_value(data->gpio_irda_irq)) {
		mdelay(10);
		dev_info(data->dev, "%d try to check IRDA_IRQ\n", retry_count);
		retry_count++;

		if (retry_count > 5)
			break;
	}

	if (gpio_get_value(data->gpio_irda_irq)) {
		dev_info(data->dev, "%d Sending IR OK!\n", count_number);
		ack_pin_onoff = 4;
	} else {
		dev_info(data->dev, "%d Sending IR NG!\n", count_number);
		ack_pin_onoff = 2;
	}

	ack_number += ack_pin_onoff;

	data->ir_freq = 0;
	data->ir_sum = 0;
}

static ssize_t remocon_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	unsigned int value;
	int count, i, ret;

	ret = regulator_enable(data->ir_regulator);
	if (ret)
		dev_err(data->dev, "Cannot enable regulator\n");
	usleep_range(2500, 3000);

	dev_info(data->dev, "ir_send called\n");

	for (i = 0; i < MAX_SIZE; i++) {
		if (sscanf(buf++, "%u", &value) == 1) {
			if (value == 0 || buf == '\0')
				break;

			if (data->count == 2) {
				data->ir_freq = value;
				data->i2c_block_transfer.data[2] = value >> 16;
				data->i2c_block_transfer.data[3]
							= (value >> 8) & 0xFF;
				data->i2c_block_transfer.data[4] = value & 0xFF;

				data->count += 3;
			} else {
				data->ir_sum += value;
				count = data->count;
				data->i2c_block_transfer.data[count]
								= value >> 8;
				data->i2c_block_transfer.data[count+1]
								= value & 0xFF;
				data->count += 2;
			}

			while (value > 0) {
				buf++;
				value /= 10;
			}
		} else {
			break;
		}
	}

	ir_remocon_work(data, data->count);

	regulator_disable(data->ir_regulator);
	if (ret)
		dev_err(data->dev, "Cannot disable regulator\n");
	return size;
}

static ssize_t remocon_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int i;
	char *bufp = buf;

	for (i = 5; i < MAX_SIZE - 1; i++) {
		if (data->i2c_block_transfer.data[i] == 0
			&& data->i2c_block_transfer.data[i+1] == 0)
			break;
		else
			bufp += sprintf(bufp, "%u,",
					data->i2c_block_transfer.data[i]);
	}
	return strlen(buf);
}

static DEVICE_ATTR(ir_send, 0664, remocon_show, remocon_store);

/* sysfs node ir_send_result */
static ssize_t remocon_ack(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);

	dev_info(data->dev, "ack_number = %d\n", ack_number);

	if (ack_number == 6)
		return sprintf(buf, "1\n");

	return sprintf(buf, "0\n");
}

static DEVICE_ATTR(ir_send_result, 0664, remocon_ack, NULL);

static int irda_read_device_info(struct ice4_fpga_data *data)
{
	struct i2c_client *client = data->client;
	u8 buf_ir_test[8];
	int ret;

	dev_info(data->dev, "%s: called\n", __func__);

	ret = i2c_master_recv(client, buf_ir_test, READ_LENGTH);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	dev_info(data->dev, "buf_ir dev_id: 0x%02x, 0x%02x\n",
			buf_ir_test[2], buf_ir_test[3]);
	ret = data->dev_id = (buf_ir_test[2] << 8 | buf_ir_test[3]);

	return ret;
}

/* sysfs node check_ir */
static ssize_t check_ir_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int ret;

	ret = irda_read_device_info(data);

	return snprintf(buf, 4, "%d\n", ret);
}

static DEVICE_ATTR(check_ir, 0664, check_ir_show, NULL);

static ssize_t toggle_rst_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	static int high;

	dev_info(data->dev, "GPIO_FPGA_RST_N(%d) will be %d\n",
			data->gpio_fpga_rst_n, high);
	gpio_set_value(data->gpio_fpga_rst_n, high);

	high = !high;

	return size;
}

static DEVICE_ATTR(toggle_rst, 0664, NULL, toggle_rst_store);

/* sysfs node irda_test */
static ssize_t irda_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i;
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct {
		unsigned char addr;
		unsigned char data[IRDA_TEST_CODE_SIZE];
	} i2c_block_transfer;
	unsigned char BSR_data[IRDA_TEST_CODE_SIZE] = {
		0x00, 0x8D, 0x00, 0x96, 0x00, 0x01, 0x50, 0x00,
		0xA8, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F
	};

	if (gpio_get_value(data->gpio_cdone) != 1) {
		dev_err(data->dev, "cdone fail !!\n");
		return 1;
	}

	dev_info(data->dev, "IRDA test code start\n");

	/* make data for sending */
	for (i = 0; i < IRDA_TEST_CODE_SIZE; i++)
		i2c_block_transfer.data[i] = BSR_data[i];

	/* sending data by I2C */
	i2c_block_transfer.addr = IRDA_TEST_CODE_ADDR;
	ret = i2c_master_send(client, (unsigned char *)&i2c_block_transfer,
			IRDA_TEST_CODE_SIZE);
	if (ret < 0) {
		dev_err(data->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client,
		(unsigned char *) &i2c_block_transfer, IRDA_TEST_CODE_SIZE);
		if (ret < 0)
			dev_err(data->dev, "%s: err2 %d\n", __func__, ret);
	}

	return size;
}

static ssize_t irda_test_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return strlen(buf);
}

static DEVICE_ATTR(irda_test, 0664, irda_test_show, irda_test_store);

static struct attribute *sec_ir_attributes[] = {
	&dev_attr_ir_send.attr,
	&dev_attr_ir_send_result.attr,
	&dev_attr_check_ir.attr,
	&dev_attr_irda_test.attr,
	&dev_attr_toggle_rst.attr,
	NULL,
};

static struct attribute_group sec_ir_attr_group = {
	.attrs = sec_ir_attributes,
};

static int ice4_irda_parse_dt(struct device *dev)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	struct device_node *node = dev->of_node;
	struct clk *parent, *out_mux;
	int ret;

	data->gpio_irda_irq = of_get_named_gpio(node, "irda-gpio", 0);
	if (!gpio_is_valid(data->gpio_irda_irq)) {
		dev_err(dev, "Cannot get irda-gpio\n");
		return -EINVAL;
	}

	data->gpio_fpga_rst_n = of_get_named_gpio(node, "fpga-reset-gpio", 0);
	if (!gpio_is_valid(data->gpio_fpga_rst_n)) {
		dev_err(dev, "Cannot get fpga-reset-gpio\n");
		return -EINVAL;
	}

	data->gpio_creset = of_get_named_gpio(node, "creset-gpio", 0);
	if (!gpio_is_valid(data->gpio_creset)) {
		dev_err(dev, "Cannot get creset-gpio\n");
		return -EINVAL;
	}

	data->gpio_cdone = of_get_named_gpio(node, "cdone-gpio", 0);
	if (!gpio_is_valid(data->gpio_cdone)) {
		dev_err(dev, "Cannot get cdone-gpio\n");
		return -EINVAL;
	}

	data->gpio_sda = of_get_named_gpio(node, "irda-sda-gpio", 0);
	if (!gpio_is_valid(data->gpio_sda)) {
		dev_err(dev, "Cannot get irda-sda-gpio\n");
		return -EINVAL;
	}

	data->gpio_scl = of_get_named_gpio(node, "irda-scl-gpio", 0);
	if (!gpio_is_valid(data->gpio_scl)) {
		dev_err(dev, "Cannot get irda-scl-gpio\n");
		return -EINVAL;
	}

	data->ir_regulator = devm_regulator_get(dev, "ir");
	if (IS_ERR(data->ir_regulator)) {
		dev_err(dev, "Cannot get ir regulator\n");
		return PTR_ERR(data->ir_regulator);
	}

	parent = clk_get(dev, "parent")	;
	if (IS_ERR(parent)) {
		dev_err(dev, "Cannot get parent clk\n");
		return PTR_ERR(parent);
	}

	out_mux = devm_clk_get(dev, "out-mux");
	if (IS_ERR(out_mux)) {
		dev_err(dev, "Cannot get out-mux clk\n");
		return PTR_ERR(out_mux);
	}

	data->mclk = devm_clk_get(data->dev, "out");
	if (IS_ERR(data->mclk)) {
		dev_err(dev, "Cannot get out clk\n");
		return PTR_ERR(data->mclk);
	}

	ret = clk_set_parent(out_mux, parent);
	if (ret) {
		dev_err(dev, "Cannot clk set parent: mout_mux\n");
		return ret;
	}

	ret = clk_prepare_enable(data->mclk);
	if (ret) {
		dev_err(dev, "Cannot clk enalbe : out");
		return ret;
	}

	return 0;
}

static int ice4_irda_gpio_configuration(struct device *dev)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int ret;

	ret = devm_gpio_request_one(dev, data->gpio_irda_irq,
				GPIOF_IN, "irda-gpio");
	if (ret) {
		dev_err(dev, "Cannot request irda-gpio\n");
		goto err_gpio_request;
	}

	ret = devm_gpio_request_one(dev, data->gpio_fpga_rst_n,
				GPIOF_OUT_INIT_LOW, "fpga-reset-gpio");
	if (ret) {
		dev_err(dev, "Cannot request fpga-reset-gpio\n");
		goto err_gpio_request;
	}

	ret = devm_gpio_request_one(dev, data->gpio_creset,
				GPIOF_OUT_INIT_HIGH, "creset-gpio");
	if (ret) {
		dev_err(dev, "Cannot request creset-gpio");
		goto err_gpio_request;
	}

	ret = devm_gpio_request_one(dev, data->gpio_cdone,
				GPIOF_IN, "cdone-gpio");
	if (ret) {
		dev_err(dev, "Cannot request cdone-gpio");
		goto err_gpio_request;
	}

	return 0;

err_gpio_request:
	return ret;
}

static void ice4_irda_firmware_download(struct device *dev,
				unsigned char *firmware, int len)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "firmware download start!\n");

	/* fpga change download state */
	gpio_set_value(data->gpio_fpga_rst_n, 0);
	gpio_set_value(data->gpio_creset, 1);
	msleep(10);

	gpio_set_value(data->gpio_creset, 0);
	usleep_range(30, 40);

	gpio_set_value(data->gpio_creset, 1);
	usleep_range(1000, 1100);

	/* firmware download */
	for (i = 0; i < len; i++) {
		int bit;
		unsigned char spibit = *firmware++;

		for (bit = 0; bit < BITS_PER_BYTE; spibit <<= 1, bit++) {
			gpio_set_value(data->gpio_scl, 0);
			gpio_set_value(data->gpio_sda, !!(spibit & 0x80));
			gpio_set_value(data->gpio_scl, 1);
		}
	}

	for (i = 0; i < DUMMY_BIT_COUNT; i++) {
		gpio_set_value(data->gpio_scl, 0);
		udelay(1);
		gpio_set_value(data->gpio_scl, 1);
	}

	gpio_set_value(data->gpio_fpga_rst_n, 1);
	gpio_export(data->gpio_fpga_rst_n, false);

	dev_info(dev, "firmware download done!\n");
}

static void work_function_firmware_download(struct work_struct *work)
{
	struct ice4_fpga_data *data = container_of((struct work_struct *)work,
					struct ice4_fpga_data, work_download);

	ice4_irda_firmware_download(&data->client->dev, fpga_irda_fw,
						ARRAY_SIZE(fpga_irda_fw));

	if (ice4_irda_check_cdone(data))
		dev_err(&data->client->dev, "FPGA FW is loaded!\n");
	else
		dev_err(&data->client->dev, "FPGA FW is NOT loaded!\n");
}

static int ice4_irda_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ice4_fpga_data *data;
	int ret;

	dev_info(&client->dev, "probe start!\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
			"Failed to i2c functionality check err\n");
		return -EIO;
	}

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (NULL == data) {
		dev_err(&client->dev, "Failed to data allocate\n");
		return -ENOMEM;
	}

	data->dev = &client->dev;
	data->client = client;
	mutex_init(&data->mutex);
	data->count = 2;
	i2c_set_clientdata(client, data);

	ret = ice4_irda_parse_dt(&client->dev);
	if (ret) {
		dev_err(&client->dev, "Failed to parse dt\n");
		goto err_parse;
	}

	ret = ice4_irda_gpio_configuration(&client->dev);
	if (ret) {
		dev_err(&client->dev, "Failed to gpio configuration\n");
		goto err_parse;
	}

	INIT_WORK(&data->work_download, work_function_firmware_download);
	schedule_work(&data->work_download);

	data->sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(data->sec_class)) {
		dev_err(&client->dev, "Failed to create class(sec)\n");
		goto err_parse;
	}

	data->sys_dev = device_create(data->sec_class, NULL, 0, data, "sec_ir");
	if (IS_ERR(data->sys_dev)) {
		dev_err(&client->dev,
			"Failed to create ice4_irda_dev device in sec_ir\n");
		goto err_irda_dev;
	}

	if (sysfs_create_group(&data->sys_dev->kobj, &sec_ir_attr_group) < 0) {
		dev_err(&client->dev,
			"Failed to create sysfs group for samsung ir!\n");
		goto err_sysfs_create;
	}

	dev_info(&client->dev, "probe complete\n");

	return 0;
err_sysfs_create:
	device_unregister(data->sys_dev);
err_irda_dev:
	class_destroy(data->sec_class);
err_parse:
	kfree(data);

	return -EINVAL;
}

static int ice4_irda_remove(struct i2c_client *client)
{
	struct ice4_fpga_data *data = i2c_get_clientdata(client);

	sysfs_remove_group(&data->sys_dev->kobj, &sec_ir_attr_group);
	clk_disable_unprepare(data->mclk);
	device_unregister(data->sys_dev);
	class_destroy(data->sec_class);
	kfree(data);

	return 0;
}

#ifdef CONFIG_PM
static int ice4_irda_suspend(struct device *dev)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);

	gpio_set_value(data->gpio_fpga_rst_n, 0);

	return 0;
}

static int ice4_irda_resume(struct device *dev)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);

	gpio_set_value(data->gpio_fpga_rst_n, 1);

	return 0;
}

static const struct dev_pm_ops ice4_fpga_pm_ops = {
	.suspend	= ice4_irda_suspend,
	.resume		= ice4_irda_resume,
};
#endif

static struct of_device_id ice4_irda_of_match[] = {
	{ .compatible = "samsung,ice4-irda", },
	{},
};

static const struct i2c_device_id ice4_irda_id[] = {
	{ IRDA_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ice4_irda_id);


static struct i2c_driver ice4_irda_i2c_driver = {
	.probe = ice4_irda_probe,
	.remove = ice4_irda_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(ice4_irda_of_match),
		.name	= IRDA_NAME,
#ifdef CONFIG_PM
		.pm	= &ice4_fpga_pm_ops,
#endif
	},
	.id_table = ice4_irda_id,
};

module_i2c_driver(ice4_irda_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SEC IRDA driver using ice4 fpga");
