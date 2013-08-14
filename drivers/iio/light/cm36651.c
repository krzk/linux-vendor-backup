/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>

#define	CM36651_VENDOR		"CAPELLA"
#define	CHIP_ID			"CM36651"

#define I2C_M_WR	0 /* for i2c Write */
#define I2c_M_RD	1 /* for i2c Read */

/* slave addresses */
#define CM36651_ALS	0x30 /* 7bits : 0x18 */
#define CM36651_PS	0x32 /* 7bits : 0x19 */

/* Ambient light sensor */
#define CS_CONF1	0x00
#define CS_CONF2	0x01
#define CS_CONF3	0x06

#define RED		0x00
#define GREEN		0x01
#define BLUE		0x02
#define WHITE		0x03

/* Proximity sensor */
#define PS_CONF1	0x00
#define PS_THD		0x01
#define PS_CANC		0x02
#define PS_CONF2	0x03

#define ALS_REG_NUM	3
#define PS_REG_NUM	4
#define ALS_CHANNEL_NUM	4
#define INITIAL_THD	0x09
#define SCAN_MODE_LIGHT	0
#define SCAN_MODE_PROX	1

enum {
	LIGHT_EN,
	PROXIMITY_EN,
	PROXIMITY_EV_EN,
};

enum cm36651_cmd {
	READ_RAW_LIGHT,
	READ_RAW_PROXIMITY,
	PROX_EV_EN,
	PROX_EV_DIS,
};

enum {
	CLOSE_PROXIMITY,
	FAR_PROXIMITY,
};

/* register settings */
static u8 als_reg_setting[ALS_REG_NUM][2] = {
	{0x00, 0x04},	/* CS_CONF1 */
	{0x01, 0x08},	/* CS_CONF2 */
	{0x06, 0x00}	/* CS_CONF3 */
};

static u8 ps_reg_setting[PS_REG_NUM][2] = {
	{0x00, 0x3C},	/* PS_CONF1 */
	{0x01, 0x09},	/* PS_THD */
	{0x02, 0x00},	/* PS_CANC */
	{0x03, 0x13},	/* PS_CONF2 */
};

struct cm36651_data {
	const struct cm36651_platform_data *pdata;
	struct i2c_client *client;
	struct mutex lock;
	struct regulator *vled_reg;
	unsigned long flags;
	wait_queue_head_t data_ready_queue;
	u8 temp;
	u16 color[4];
};

int cm36651_i2c_read_byte(struct cm36651_data *cm36651, u8 addr, u8 *val)
{
	int ret = 0;
	struct i2c_msg msg[1];
	struct i2c_client *client = cm36651->client;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	/* send slave address & command */
	msg->addr = addr >> 1;
	msg->flags = I2C_M_RD;
	msg->len = 1;
	msg->buf = val;

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret != 1) {
		dev_err(&client->dev, "i2c transfer error ret=%d\n", ret);
		ret = -EIO;
	}

	return 0;
}

int cm36651_i2c_read_word(struct cm36651_data *cm36651, u8 addr,
							u8 command, u16 *val)
{
	int ret = 0;
	struct i2c_client *client = cm36651->client;
	struct i2c_msg msg[2];
	unsigned char data[2] = {0,};
	u16 value = 0;

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	/* send slave address & command */
	msg[0].addr = addr >> 1;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &command;

	/* read word data */
	msg[1].addr = addr >> 1;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) {
		dev_err(&client->dev, "i2c transfer error ret=%d\n", ret);
		ret = -EIO;
	}
	value = (u16)data[1];
	*val = (value << 8) | (u16)data[0];

	return 0;
}

int cm36651_i2c_write_byte(struct cm36651_data *cm36651, u8 addr,
							 u8 command, u8 val)
{
	int ret = 0;
	struct i2c_client *client = cm36651->client;
	struct i2c_msg msg[1];
	unsigned char data[2];

	if ((client == NULL) || (!client->adapter))
		return -ENODEV;

	data[0] = command;
	data[1] = val;

	/* send slave address & command */
	msg->addr = addr >> 1;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret != 1) {
		dev_err(&client->dev, "i2c transfer error ret=%d\n", ret);
		ret = -EIO;
	}

	return 0;
}

static int cm36651_setup_reg(struct cm36651_data *cm36651)
{
	struct i2c_client *client = cm36651->client;
	int ret = 0, i = 0;
	u8 tmp = 0;

	/* ALS initialization */
	for (i = 0; i < ALS_REG_NUM; i++) {
		ret = cm36651_i2c_write_byte(cm36651, CM36651_ALS,
			als_reg_setting[i][0], als_reg_setting[i][1]);
		if (ret < 0)
			goto err_setup_reg;
	}

	/* PS initialization */
	for (i = 0; i < PS_REG_NUM; i++) {
		ret = cm36651_i2c_write_byte(cm36651, CM36651_PS,
			ps_reg_setting[i][0], ps_reg_setting[i][1]);
		if (ret < 0)
			goto err_setup_reg;
	}

	/* printing the inital proximity value with no contact */
	ret = cm36651_i2c_read_byte(cm36651, CM36651_PS, &tmp);
	if (ret < 0)
		goto err_setup_reg;

	dev_dbg(&client->dev, "initial proximity value = %d\n", tmp);

	/* turn off */
	cm36651_i2c_write_byte(cm36651, CM36651_ALS, CS_CONF1, 0x01);
	cm36651_i2c_write_byte(cm36651, CM36651_PS, PS_CONF1, 0x01);

	return 0;

err_setup_reg:
	dev_err(&client->dev, "cm36651 register failed. %d\n", ret);
	return ret;
}

static ssize_t cm36651_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", CM36651_VENDOR);
}

static ssize_t proximity_thresh_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "prox_threshold = %d\n", ps_reg_setting[1][1]);
}

static ssize_t proximity_thresh_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct iio_dev *dev_info = dev_to_iio_dev(dev);
	struct cm36651_data *cm36651 = iio_priv(dev_info);
	struct i2c_client *client = cm36651->client;
	u8 thresh_value = INITIAL_THD;
	int ret = 0;

	ret = kstrtou8(buf, 10, &thresh_value);
	if (ret < 0)
		dev_err(dev, "kstrtoint failed\n");

	ps_reg_setting[1][1] = thresh_value;
	ret = cm36651_i2c_write_byte(cm36651, CM36651_PS,
			PS_THD, ps_reg_setting[1][1]);
	if (ret < 0) {
		dev_err(dev, "PS reg is failed. %d\n", ret);
		return ret;
	}
	dev_info(&client->dev, "new threshold = 0x%x\n", ps_reg_setting[1][1]);
	msleep(150);

	return size;
}

static int cm36651_read_output(struct cm36651_data *cm36651,
						u8 address, int *val)
{
	struct i2c_client *client = cm36651->client;
	int i = 0, ret = -EINVAL;
	u8 prox_val;

	switch (address) {
	case CM36651_ALS:
		for (i = 0; i < ALS_CHANNEL_NUM; i++) {
			ret = cm36651_i2c_read_word(cm36651, address,
							i, &cm36651->color[i]);
			if (ret < 0)
				goto read_err;
		}

		dev_info(&client->dev, "%d, %d, %d, %d\n",
			cm36651->color[0]+1, cm36651->color[1]+1,
			cm36651->color[2]+1, cm36651->color[3]+1);
		break;
	case CM36651_PS:
		ret = cm36651_i2c_read_byte(cm36651, address, &prox_val);
		if (ret < 0)
			goto read_err;

		dev_info(&client->dev, "%d\n", prox_val);
		break;
	}

	ret = cm36651_i2c_write_byte(cm36651, address, 0x00, 0x01);
	if (ret < 0)
		goto write_err;

	return ret;

read_err:
	dev_err(&client->dev, "fail to read sensor value");
	return ret;
write_err:
	dev_err(&client->dev, "fail to write register value");
	return ret;
}

static irqreturn_t cm36651_irq_handler(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct cm36651_data *cm36651 = iio_priv(indio_dev);
	struct i2c_client *client = cm36651->client;
	int ev_dir, val, ret;
	u64 ev_code;

	ret =  cm36651_i2c_read_byte(cm36651, CM36651_PS, &cm36651->temp);
	if (ret < 0) {
		dev_err(&client->dev, "read data is failed. %d\n", ret);
		return ret;
	}

	if (cm36651->temp < ps_reg_setting[1][1]) {
		ev_dir = IIO_EV_DIR_RISING;
		val = FAR_PROXIMITY;
	} else {
		ev_dir = IIO_EV_DIR_FALLING;
		val = CLOSE_PROXIMITY;
	}

	ev_code = IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY, READ_RAW_PROXIMITY,
						IIO_EV_TYPE_THRESH, ev_dir);

	iio_push_event(indio_dev, ev_code, iio_get_time_ns());
	cm36651_i2c_read_byte(cm36651, CM36651_PS, &cm36651->temp);
	dev_info(&client->dev, "val: %d, ps_data: %d(close:0, far:1)\n",
							val, cm36651->temp);

	return IRQ_HANDLED;
}

static int cm36651_set_operation_mode(struct cm36651_data *cm36651,
						enum cm36651_cmd cmd)
{
	struct i2c_client *client = cm36651->client;
	int ret = 0;
	int i;

	switch (cmd) {
	case READ_RAW_LIGHT:
		ret = cm36651_i2c_write_byte(cm36651, CM36651_ALS,
							CS_CONF1, 0x04);
		break;
	case READ_RAW_PROXIMITY:
		ret = cm36651_i2c_write_byte(cm36651, CM36651_PS,
							PS_CONF1, 0x3C);
		break;
	case PROX_EV_EN:
		if (test_bit(PROXIMITY_EV_EN, &cm36651->flags)) {
			dev_err(&client->dev, "Aleady enable state\n");
			return -EINVAL;
		}
		set_bit(PROXIMITY_EV_EN, &cm36651->flags);

		/* enable setting */
		for (i = 0; i < 4; i++) {
			cm36651_i2c_write_byte(cm36651, CM36651_PS,
				ps_reg_setting[i][0], ps_reg_setting[i][1]);
		}
		enable_irq(client->irq);
		break;
	case PROX_EV_DIS:
		if (!test_bit(PROXIMITY_EV_EN, &cm36651->flags)) {
			dev_err(&client->dev, "Aleady disable state\n");
			return -EINVAL;
		}
		clear_bit(PROXIMITY_EV_EN, &cm36651->flags);
		disable_irq(client->irq);

		/* disable setting */
		cm36651_i2c_write_byte(cm36651, CM36651_PS, PS_CONF1, 0x01);
		break;
	}
	return ret;
}

static int cm36651_read_channel(struct cm36651_data *cm36651,
				struct iio_chan_spec const *chan, int *val)
{
	struct i2c_client *client = cm36651->client;
	enum cm36651_cmd cmd = 0;
	int ret;

	switch (chan->scan_index) {
	case SCAN_MODE_LIGHT:
		cmd = READ_RAW_LIGHT;
		break;
	case SCAN_MODE_PROX:
		cmd = READ_RAW_PROXIMITY;
		break;
	}

	ret = cm36651_set_operation_mode(cm36651, cmd);
	if (ret < 0) {
		dev_err(&client->dev, "cm36651 set operation mode failed\n");
		return ret;
	}

	msleep(50);
	ret = cm36651_read_output(cm36651, chan->address, val);
	if (ret < 0) {
		dev_err(&client->dev, "cm36651 read output failed\n");
		return ret;
	}

	return 0;
}

static int cm36651_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct cm36651_data *cm36651 = iio_priv(indio_dev);
	int ret = -EINVAL;

	mutex_lock(&cm36651->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = cm36651_read_channel(cm36651, chan, val);
		break;
	}
	mutex_unlock(&cm36651->lock);

	return ret;
}

static int cm36651_read_event_val(struct iio_dev *indio_dev,
					u64 event_code, int *val)
{
	struct cm36651_data *cm36651 = iio_priv(indio_dev);
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	int event_type = IIO_EVENT_CODE_EXTRACT_TYPE(event_code);

	if (event_type != IIO_EV_TYPE_THRESH ||	chan_type != IIO_PROXIMITY)
		return -EINVAL;

	*val = cm36651->temp;
	return 0;
}

static int cm36651_write_event_config(struct iio_dev *indio_dev,
					u64 event_code, int state)
{
	struct cm36651_data *cm36651 = iio_priv(indio_dev);
	enum cm36651_cmd cmd;
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	int ret = -EINVAL;

	mutex_lock(&cm36651->lock);

	if (chan_type == IIO_PROXIMITY) {
		cmd = state ? PROX_EV_EN : PROX_EV_DIS;
		ret = cm36651_set_operation_mode(cm36651, cmd);
	}

	mutex_unlock(&cm36651->lock);

	return ret;
}

static int cm36651_read_event_config(struct iio_dev *indio_dev, u64 event_code)
{
	struct cm36651_data *cm36651 = iio_priv(indio_dev);
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	int event_en = -EINVAL;

	mutex_lock(&cm36651->lock);

	if (chan_type == IIO_PROXIMITY)
		event_en = test_bit(PROXIMITY_EV_EN, &cm36651->flags);

	mutex_unlock(&cm36651->lock);

	return event_en;
}

static IIO_DEVICE_ATTR(vendor, 0644, cm36651_vendor_show, NULL, 0);
static IIO_DEVICE_ATTR(prox_thresh, 0644, proximity_thresh_show,
					proximity_thresh_store, 1);

static struct attribute *cm36651_attributes[] = {
	&iio_dev_attr_vendor.dev_attr.attr,
	&iio_dev_attr_prox_thresh.dev_attr.attr,
	NULL
};

static struct attribute_group cm36651_attribute_group = {
	.attrs = cm36651_attributes,
};

static const struct iio_chan_spec cm36651_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.shift = 0,
			.storagebits = 16,
		},
		.address = CM36651_ALS,
		.scan_index = SCAN_MODE_LIGHT
	},
	{
		.type = IIO_PROXIMITY,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_type = {
			.sign = 'u',
			.realbits = 8,
			.shift = 0,
			.storagebits = 8,
		},
		.address = CM36651_PS,
		.scan_index = SCAN_MODE_PROX,
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_EITHER)
	},
};

static const struct iio_info cm36651_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &cm36651_read_raw,
	.read_event_value = &cm36651_read_event_val,
	.read_event_config = &cm36651_read_event_config,
	.write_event_config = &cm36651_write_event_config,
	.attrs = &cm36651_attribute_group,
};

static int cm36651_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct cm36651_data *cm36651;
	struct iio_dev *indio_dev;
	unsigned long irqflag;
	int ret;

	dev_info(&client->dev, "cm36651 light/proxymity sensor probe\n");

	indio_dev = iio_device_alloc(sizeof(*cm36651));
	if (indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	cm36651 = iio_priv(indio_dev);

	cm36651->vled_reg = devm_regulator_get(&client->dev, "vled");
	if (IS_ERR(cm36651->vled_reg)) {
		dev_err(&client->dev, "failed to get regulator vled\n");
		ret =  PTR_ERR(cm36651->vled_reg);
		return ret;
	}

	ret = regulator_enable(cm36651->vled_reg);
	if (ret) {
		dev_err(&client->dev, "faile to enable regulator\n");
		goto error_put_reg;
	}

	i2c_set_clientdata(client, indio_dev);

	cm36651->client = client;
	init_waitqueue_head(&cm36651->data_ready_queue);

	ret = cm36651_setup_reg(cm36651);

	mutex_init(&cm36651->lock);
	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = cm36651_channels;
	indio_dev->num_channels = ARRAY_SIZE(cm36651_channels);
	indio_dev->info = &cm36651_info;
	indio_dev->name = id->name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	irqflag = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
	ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
		&cm36651_irq_handler, irqflag, "proximity_int", indio_dev);
	if (ret) {
		dev_err(&client->dev, "failed to request irq\n");
		goto error_ret;
	}

	disable_irq(client->irq);

	ret = iio_device_register(indio_dev);
	if (ret)
		goto exit_free_iio;

	return 0;

error_ret:
	return ret;
error_put_reg:
	regulator_put(cm36651->vled_reg);
	return ret;
exit_free_iio:
	iio_device_free(indio_dev);
	return ret;
}

static int cm36651_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct cm36651_data *cm36651 = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	regulator_disable(cm36651->vled_reg);

	iio_device_free(indio_dev);

	return 0;
}

static const struct i2c_device_id cm36651_id[] = {
	{"cm36651", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cm36651_id);

static const struct of_device_id cm36651_of_match[] = {
	{ .compatible = "capella,cm36651" },
	{ .compatible = "cm36651"},
	{ }
};

static struct i2c_driver cm36651_driver = {
	.driver = {
		.name	= "cm36651",
		.of_match_table = of_match_ptr(cm36651_of_match),
		.owner	= THIS_MODULE,
	},
	.probe		= cm36651_probe,
	.remove		= cm36651_remove,
	.id_table	= cm36651_id,
};

module_i2c_driver(cm36651_driver);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Light/Proximity Sensor device driver for cm36651");
MODULE_LICENSE("GPL v2");
