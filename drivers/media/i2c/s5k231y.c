/*
 * Samsung Dynamic Vision Sensor driver
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 * Author: sunghwansoon <hs2704.sung@samsung.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define SENSOR_NAME "S5K231Y"

#define DEBUG
#include <linux/device.h>

struct s5k231y_state {
	struct v4l2_subdev	subdev;
	struct v4l2_mbus_framefmt fmt;
	struct media_pad	pad;	/* for media device pad */
	struct i2c_client	*client;
};

struct reg_value {
	u16 reg_addr;
	u8 val;
	u8 mask;
	u32 delay_ms;
};

static const struct reg_value s5k231y_init_setting_vga_500[] = {
	{0x3908, 0x40, 0}, /* set dphy_auto_enable */
	{0x000d, 0x24, 1}, /* re-write bias */
	{0x3500, 0x01, 0}, /* bypass actdcs*/
	{0x3600, 0x00, 0}, /* shist off */
	{0x3300, 0x01, 0}, /* bypass crop */
	{0x3043, 0x01, 0}, /* bypass esp */
	{0x001c, 0x08, 0}, /* low sensitivity */
	{0x001e, 0x00, 0}, /* low sensitivity */
	{0x3255, 0x0c, 0},
	{0x000f, 0x05, 0},
	{0x301e, 0x00, 0}, /* clr para_out_en_r */
	{0x3010, 0x01, 0}, /* PLL_S : 1000MHz --> 500MHz */
	{0x3924, 0x08, 0}, /* lpx(0), band(8) */
	{0x3000, 0x02, 1}, /* set dvs mode to active mode*/
	{0x3266, 0x08, 0}, /* boot sequence */
	{0x3255, 0x0c, 0}, /* enable global hold and global reset */
	{0x000f, 0x07, 0},
	{0x0010, 0xff, 0},
	{0x0013, 0x03, 0}, /* default value : 0x01 */
	{0x0014, 0x7f, 0}, /* default value : 0x02 */
	{0x0015, 0x00, 0}, /* disable optical black */
	{0x000c, 0x14, 0},
	{0x000b, 0x06, 0},
	{0x0012, 0x7d, 0},
	{0x001c, 0x00, 0}, /* Dummy NUM ON, Default : 04 */
	{0x001e, 0x08, 0}, /* Dummy NUM nOFF, Default : 04*/
	{0x3248, 0x00, 0},
	{0x323c, 0x04, 0},
	{0x3240, 0x00, 0},
	{0x3241, 0x00, 0},
	{0x3242, 0x02, 0},
	{0x3243, 0x32, 0},
	{0x3244, 0x00, 0},
	{0x3245, 0x00, 0},
	{0x3246, 0x00, 0},
	{0x3247, 0x32, 0},
	{0x324b, 0x0a, 0},
	{0x324c, 0x08, 0},
	{0x324d, 0x14, 0},
	{0x324e, 0x04, 0},
	{0x324f, 0x08, 0},
	{0x3251, 0x0f, 0},
	{0x3253, 0x0c, 0},
	{0x3254, 0x10, 0},
	{0x3256, 0x00, 0},
	{0x3257, 0x00, 0},
	{0x3258, 0x01, 0},
	{0x3259, 0x32, 1},
	{0x3238, 0x01, 0}, /* reset timestamp */
	{0x3238, 0x00, 0}, /* restart timestamp */
	{0x390b, 0x08, 0}, /* set use_big_endian */
	{0x3901, 0x1e, 0}, /* set sci2_header : id(yuv422-8bit) */
	{0x391a, 0x10, 0}, /* set frm_cnt *CHECK* */
	{0x3900, 0xF0, 0}, /* set mipi_enable | 4-lane | use_fram */
};

static inline struct s5k231y_state *to_state
	(struct v4l2_subdev *subdev)
{
	struct s5k231y_state *state =
		container_of(subdev, struct s5k231y_state, subdev);
	return state;
}

static inline struct i2c_client *to_client
	(struct v4l2_subdev *subdev)
{
	return (struct i2c_client *)v4l2_get_subdevdata(subdev);
}

static int sensor_231y_write_reg(struct i2c_client *client, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;
	buf[2] = val;

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.buf = buf;
	msg.len = sizeof(buf);

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		dev_err(&client->dev, "%s: error: reg=%x, val=%x\n",
			__func__, reg, val);
		return ret;
	}

	return 0;
}

static int sensor_231y_read_reg(struct i2c_client *client, u16 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	buf[0] = reg >> 8;
	buf[1] = reg & 0xff;

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].buf = buf;
	msg[0].len = sizeof(buf);

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].buf = buf;
	msg[1].len = 1;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		return ret;

	*val = buf[0];
	return 0;
}

static int sensor_231y_load_regs(struct v4l2_subdev *subdev,
			    const struct reg_value *regs, int size)
{
	unsigned int i;
	u32 delay_ms;
	u16 reg_addr;
	u8 val;
	int ret = 0;
	struct i2c_client *client = to_client(subdev);

	for (i = 0; i < size; ++i, ++regs) {
		delay_ms = regs->delay_ms;
		reg_addr = regs->reg_addr;
		val = regs->val;

		ret = sensor_231y_write_reg(client, reg_addr, val);
		if (ret)
			break;

		if (delay_ms)
			usleep_range(1000*delay_ms, 1000*delay_ms+100);
	}

	return ret;
}

static int sensor_231y_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct i2c_client *client = to_client(subdev);
	u8 id = 0;

	WARN_ON(!subdev);

	dev_info(&client->dev, "%s start\n", __func__);

	ret = sensor_231y_read_reg(client, 0x3052, &id);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read id\n");
		goto p_err;
	}

	dev_info(&client->dev, "id=0x%02x\n", id);

	/* init member */
	ret = sensor_231y_load_regs(subdev, s5k231y_init_setting_vga_500,
				ARRAY_SIZE(s5k231y_init_setting_vga_500));
	if (ret < 0) {
		dev_err(&client->dev, "init_reg failed\n");
		goto p_err;
	}

	ret = sensor_231y_read_reg(client, 0x001c, &id);
	if (ret < 0) {
		dev_err(&client->dev, "failed to read id\n");
		goto p_err;
	}
	dev_info(&client->dev, "id=0x%02x\n", id);
	dev_info(&client->dev, "%s end\n", __func__);

	msleep(100);

p_err:
	return ret;
}

static int sensor_231y_s_stream(struct v4l2_subdev *subdev,
	int enable)
{
	int ret = 0;
	struct i2c_client *client = to_client(subdev);

	dev_info(&client->dev, "%s %d\n", __func__, enable);

	if (enable) {
		ret = sensor_231y_init(subdev, 1);
		if (ret) {
			dev_err(&client->dev, "stream_on is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	return ret;
}

static int sensor_231y_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	return 0;
}

static int sensor_231y_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	return 0;
}

static int sensor_231y_subdev_registered(struct v4l2_subdev *sd)
{
	return 0;
}

static void sensor_231y_subdev_unregistered(struct v4l2_subdev *sd)
{
}

static int sensor_231y_link_setup(struct media_entity *entity,
		const struct media_pad *local,
		const struct media_pad *remote, u32 flags)
{
	return 0;
}

static int sensor_231y_s_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_pad_config *cfg,
			struct v4l2_subdev_format *fmt)
{
	return 0;
}

static struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt		= sensor_231y_s_fmt,
};

static const struct v4l2_subdev_core_ops core_ops = {
	.init		= sensor_231y_init,
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = sensor_231y_s_stream,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad	= &pad_ops,
};

static const struct v4l2_subdev_internal_ops internal_ops = {
	.open = sensor_231y_subdev_open,
	.close = sensor_231y_subdev_close,
	.registered = sensor_231y_subdev_registered,
	.unregistered = sensor_231y_subdev_unregistered,
};

static const struct media_entity_operations media_ops = {
	.link_setup = sensor_231y_link_setup,
};

int sensor_231y_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct v4l2_subdev *subdev_module;
	struct s5k231y_state *s5k231y_state;

	s5k231y_state = devm_kzalloc(&client->dev, sizeof(struct s5k231y_state),
		GFP_KERNEL);
	if (!s5k231y_state) {
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_module = &s5k231y_state->subdev;
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE,
		"%s", SENSOR_NAME);

	v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);

	s5k231y_state->pad.flags = MEDIA_PAD_FL_SOURCE;

	subdev_module->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	subdev_module->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	subdev_module->internal_ops = &internal_ops;
	subdev_module->entity.ops = &media_ops;

	ret = media_entity_init(&subdev_module->entity, 1,
		&s5k231y_state->pad, 0);
	if (ret < 0) {
		dev_err(&client->dev, "%s, failed\n", __func__);
		return ret;
	}

	s5k231y_state->client = client;

p_err:
	dev_info(&client->dev, "(%d)\n", ret);
	return ret;
}

static int sensor_231y_remove(struct i2c_client *client)
{
	int ret = 0;
	struct v4l2_subdev *subdev = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);

	return ret;
}

static const struct i2c_device_id sensor_231y_idt[] = {
	{ SENSOR_NAME, 0 },
	{ }
};

static struct i2c_driver sensor_231y_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
	},
	.probe	= sensor_231y_probe,
	.remove	= sensor_231y_remove,
	.id_table = sensor_231y_idt
};

module_i2c_driver(sensor_231y_driver);

MODULE_AUTHOR("Hwansoon Sung <hs2704.sung@samsung.com>");
MODULE_DESCRIPTION("Sensor 231y driver");
MODULE_LICENSE("GPL v2");
