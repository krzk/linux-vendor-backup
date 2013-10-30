/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 * Hyungwon Hwang <human.hwang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <video/display.h>
#include <video/mipi_display.h>
#include <video/videomode.h>
#include <video/of_videomode.h>

#define NUM_REGULATOR_CONSUMERS	2

#define MIN_BRIGHTNESS		(0)
#define MAX_BRIGHTNESS		(255)

#define SCAN_FROM_LEFT_TO_RIGHT 0
#define SCAN_FROM_RIGHT_TO_LEFT 1
#define SCAN_FROM_TOP_TO_BOTTOM 0
#define SCAN_FROM_BOTTOM_TO_TOP 1

#define to_panel(p)	container_of(p, struct l5f31188, entity)

enum cabc_mode {
	CABC_OFF,
	USER_INTERFACE_IMAGE,
	STILL_PICTURE,
	MOVING_IMAGE
};

struct l5f31188_platform_data {
	unsigned long width;		/* Panel width in mm */
	unsigned long height;		/* Panel height in mm */
	struct videomode mode;

	/* time needed while resetting */
	unsigned int reset_delay1;

	/* time needed after reset */
	unsigned int reset_delay2;

	/* stable time needing to become panel power on. */
	unsigned int power_on_delay;

	/* time needed after display off */
	unsigned int power_off_delay1;

	/* time needed after sleep off */
	unsigned int power_off_delay2;
};

struct l5f31188 {
	struct display_entity entity;
	struct device *dev;

	struct l5f31188_platform_data *pdata;
	struct backlight_device *bd;
	struct regulator_bulk_data regs[NUM_REGULATOR_CONSUMERS];

	unsigned int power;
	enum cabc_mode cabc_mode;

	unsigned int reset_gpio;
};

static void l5f31188_delay(unsigned int msecs)
{
	/* refer from documentation/timers/timers-howto.txt */
	if (msecs < 20)
		usleep_range(msecs * 1000, (msecs + 1) * 1000);
	else
		msleep(msecs);
}

static void l5f31188_sleep_in(struct video_source *source)
{

	const unsigned char data_to_send[] = {
		0x10, 0x00
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_sleep_out(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0x11, 0x00
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_set_gamma(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0x26, 0x00
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_display_off(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0x28, 0x00
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_display_on(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0x29, 0x00
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_ctl_memory_access(struct video_source *source,
		int h_direction, int v_direction)
{
	const unsigned char data_to_send[] = {
		0x36, ((h_direction & 0x1) << 1) | (v_direction & 0x1)
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_set_pixel_format(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0x3A, 0x70
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_write_disbv(struct video_source *source,
		unsigned int brightness)
{
	const unsigned char data_to_send[] = {
		0x51, brightness
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_write_ctrld(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0x53, 0x2C
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_write_cabc(struct video_source *source,
		enum cabc_mode cabc_mode)
{
	const unsigned char data_to_send[] = {
		0x55, cabc_mode
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_write_cabcmb(struct video_source *source,
		unsigned int min_brightness)
{
	const unsigned char data_to_send[] = {
		0x5E, min_brightness
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_set_extension(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0xB9, 0xFF, 0x83, 0x94
	};

	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_set_dgc_lut(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0xC1, 0x01, 0x00, 0x04, 0x0E, 0x18, 0x1E,
		0x26, 0x2F, 0x36, 0x3E, 0x47, 0x4E, 0x56,
		0x5D, 0x65, 0x6D, 0x75, 0x7D, 0x84, 0x8C,
		0x94, 0x9C, 0xA4, 0xAD, 0xB5, 0xBD, 0xC5,
		0xCC, 0xD4, 0xDE, 0xE5, 0xEE, 0xF7, 0xFF,
		0x3F, 0x9A, 0xCE, 0xD4, 0x21, 0xA1, 0x26,
		0x54, 0x00, 0x00, 0x04, 0x0E, 0x19, 0x1F,
		0x27, 0x30, 0x37, 0x40, 0x48, 0x50, 0x58,
		0x60, 0x67, 0x6F, 0x77, 0x7F, 0x87, 0x8F,
		0x97, 0x9F, 0xA7, 0xB0, 0xB8, 0xC0, 0xC8,
		0xCE, 0xD8, 0xE0, 0xE7, 0xF0, 0xF7, 0xFF,
		0x3C, 0xEB, 0xFD, 0x2F, 0x66, 0xA8, 0x2C,
		0x46, 0x00, 0x00, 0x04, 0x0E, 0x18, 0x1E,
		0x26, 0x30, 0x38, 0x41, 0x4A, 0x52, 0x5A,
		0x62, 0x6B, 0x73, 0x7B, 0x83, 0x8C, 0x94,
		0x9C, 0xA5, 0xAD, 0xB6, 0xBD, 0xC5, 0xCC,
		0xD4, 0xDD, 0xE3, 0xEB, 0xF2, 0xF9, 0xFF,
		0x3F, 0xA4, 0x8A, 0x8F, 0xC7, 0x33, 0xF5,
		0xE9, 0x00
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_set_tcon(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0xC7, 0x00, 0x20
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_set_ptba(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0xBF, 0x06, 0x10
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static void l5f31188_set_eco(struct video_source *source)
{
	const unsigned char data_to_send[] = {
		0xC6, 0x0C
	};
	dsi_dcs_write(source, 0,
			data_to_send, ARRAY_SIZE(data_to_send));
}

static int l5f31188_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}


static int l5f31188_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct l5f31188 *panel = bl_get_data(bd);
	enum display_entity_state state;
	int ret;

	if (bd->props.power == FB_BLANK_POWERDOWN
			|| bd->props.state & BL_CORE_SUSPENDED)
		state = DISPLAY_ENTITY_STATE_OFF;
	else if (bd->props.power != FB_BLANK_UNBLANK)
		state = DISPLAY_ENTITY_STATE_STANDBY;
	else
		state = DISPLAY_ENTITY_STATE_ON;

	if (panel->power == FB_BLANK_POWERDOWN) {
		dev_err(panel->dev,
				"panel off: brightness set failed.\n");
		return -EINVAL;
	}

	if (brightness < 0 || brightness > bd->props.max_brightness) {
		dev_err(panel->dev, "panel brightness should be 0 to %d.\n",
				MAX_BRIGHTNESS);
		return -EINVAL;
	}

	ret = display_entity_set_state(&panel->entity, state);
	if (ret)
		return ret;

	if (state == DISPLAY_ENTITY_STATE_OFF)
		return 0;

	l5f31188_write_disbv(panel->entity.source, brightness);

	return 0;
}

static const struct backlight_ops l5f31188_backlight_ops = {
	.get_brightness = l5f31188_get_brightness,
	.update_status = l5f31188_set_brightness,
};

static void l5f31188_set_sequence(struct l5f31188 *panel)
{
	int brightness = panel->bd->props.brightness;
	struct video_source *source = panel->entity.source;

	l5f31188_set_extension(source);
	l5f31188_set_dgc_lut(source);

	l5f31188_set_eco(source);
	l5f31188_set_tcon(source);
	l5f31188_set_ptba(source);
	l5f31188_set_gamma(source);
	l5f31188_ctl_memory_access(source, SCAN_FROM_LEFT_TO_RIGHT,
			SCAN_FROM_TOP_TO_BOTTOM);
	l5f31188_set_pixel_format(source);
	l5f31188_write_disbv(source, brightness);
	l5f31188_write_ctrld(source);
	l5f31188_write_cabc(source, 0x0);
	l5f31188_write_cabcmb(source, 0x0);

	l5f31188_sleep_out(source);
	l5f31188_delay(panel->pdata->power_on_delay);
	l5f31188_display_on(source);

	dev_info(panel->dev, "%s:done.\n", __func__);
}

#ifdef CONFIG_OF
static int l5f31188_reset(struct device *dev)
{
	struct l5f31188 *panel = dev_get_drvdata(dev);

	gpio_set_value(panel->reset_gpio, 0);
	l5f31188_delay(panel->pdata->reset_delay1);
	gpio_set_value(panel->reset_gpio, 1);
	l5f31188_delay(panel->pdata->reset_delay2);

	return 0;
}

static struct l5f31188_platform_data *l5f31188_parse_dt(struct l5f31188 *panel)
{
	struct device_node *node = panel->dev->of_node;
	struct l5f31188_platform_data *data;
	const __be32 *prop_data;

	data = devm_kzalloc(panel->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(panel->dev, "failed to allocate platform data.\n");
		return NULL;
	}

	if (of_get_videomode(node, &data->mode, 0)) {
		dev_err(panel->dev, "failed to read video mode from DT\n");
		return NULL;
	}

	panel->reset_gpio = of_get_named_gpio(node, "reset-gpio", 0);
	if (panel->reset_gpio < 0)
		return NULL;

	prop_data = of_get_property(node, "width", NULL);
	if (!prop_data)
		return NULL;
	data->width = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "height", NULL);
	if (!prop_data)
		return NULL;
	data->height = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "reset-delay1", NULL);
	if (!prop_data)
		return NULL;
	data->reset_delay1 = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "reset-delay2", NULL);
	if (!prop_data)
		return NULL;
	data->reset_delay2 = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "power-on-delay", NULL);
	if (!prop_data)
		return NULL;
	data->power_on_delay = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "power-off-delay1", NULL);
	if (!prop_data)
		return NULL;
	data->power_off_delay1 = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "power-off-delay2", NULL);
	if (!prop_data)
		return NULL;
	data->power_off_delay2 = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "vdd-supply", NULL);
	if (!prop_data)
		return NULL;

	return data;
}

static struct of_device_id l5f31188_of_match[] = {
	{ .compatible = "samsung,l5f31188" },
	{ }
};

MODULE_DEVICE_TABLE(of, l5f31188_of_match);
#else
static struct l5f31188_platform_data *l5f31188_parse_dt(struct l5f31188 *panel)
{
	return NULL;
}
#endif

static const struct display_entity_interface_params l5f31188_params = {
	.type = DISPLAY_ENTITY_INTERFACE_DSI,
	.p.dsi = {
		.format = DSI_FMT_RGB888,
		.mode = DSI_MODE_VIDEO | DSI_MODE_VIDEO_BURST
			| DSI_MODE_VIDEO_HFP | DSI_MODE_VIDEO_HBP
			| DSI_MODE_VIDEO_HSA | DSI_MODE_EOT_PACKET
			| DSI_MODE_VSYNC_FLUSH,
		.data_lanes = 0xf,
		.hs_clk_freq = 500000000,
		.esc_clk_freq = 10000000,
	},
};

static void l5f31188_power_on(struct l5f31188 *panel)
{
	struct video_source *src = panel->entity.source;
	int ret;

	ret = regulator_bulk_enable(NUM_REGULATOR_CONSUMERS,
			panel->regs);
	if (ret != 0) {
		dev_err(panel->dev, "failed to enable regulators (%d)\n", ret);
		return;
	}

	/* panel reset */
	l5f31188_reset(panel->dev);

	src->ops.dsi->enable(src);

	l5f31188_set_sequence(panel);

	return;
}

static void l5f31188_power_off(struct l5f31188 *panel)
{
	struct video_source *src = panel->entity.source;

	l5f31188_display_off(src);
	l5f31188_delay(panel->pdata->power_off_delay1);
	l5f31188_sleep_in(src);
	l5f31188_delay(panel->pdata->power_off_delay2);

	src->ops.dsi->disable(src);

	regulator_bulk_disable(NUM_REGULATOR_CONSUMERS,
			panel->regs);
}

static int l5f31188_set_state(struct display_entity *entity,
		enum display_entity_state state)
{
	struct l5f31188 *panel = to_panel(entity);
	struct video_source *src = panel->entity.source;

	switch (state) {
	case DISPLAY_ENTITY_STATE_OFF:
		if (entity->state == DISPLAY_ENTITY_STATE_ON)
			src->common_ops->set_stream(src,
					DISPLAY_ENTITY_STREAM_STOPPED);

		l5f31188_power_off(panel);
		break;
	case DISPLAY_ENTITY_STATE_ON:
		if (entity->state == DISPLAY_ENTITY_STATE_OFF)
			l5f31188_power_on(panel);

		src->common_ops->set_stream(src,
				DISPLAY_ENTITY_STREAM_CONTINUOUS);
		break;
	default:
		break;
	}

	return 0;
}

static int l5f31188_get_modes(struct display_entity *entity,
		const struct videomode **modes)
{
	struct l5f31188 *panel = to_panel(entity);

	*modes = &panel->pdata->mode;
	return 1;
}

static int l5f31188_get_size(struct display_entity *entity,
		unsigned int *width, unsigned int *height)
{
	struct l5f31188 *panel = to_panel(entity);

	*width = panel->pdata->width;
	*height = panel->pdata->height;
	return 0;
}

static int l5f31188_get_params(struct display_entity *entity,
		struct display_entity_interface_params *params)
{
	*params = l5f31188_params;
	return 0;
}

static const struct display_entity_control_ops l5f31188_control_ops = {
	.set_state = l5f31188_set_state,
	.get_modes = l5f31188_get_modes,
	.get_size = l5f31188_get_size,
	.get_params = l5f31188_get_params,
};

static void l5f31188_release(struct display_entity *entity)
{
	struct l5f31188 *panel = to_panel(entity);

	backlight_device_unregister(panel->bd);
	regulator_bulk_free(NUM_REGULATOR_CONSUMERS, panel->regs);
	kfree(panel);
}

static int l5f31188_probe(struct platform_device *pdev)
{
	struct l5f31188 *panel;
	int ret;

	panel = kzalloc(sizeof(struct l5f31188), GFP_KERNEL);
	if (!panel) {
		dev_err(&pdev->dev,
				"failed to allocate l5f31188 structure.\n");
		return -ENOMEM;
	}

	panel->dev = &pdev->dev;
	panel->pdata = 
		(struct l5f31188_platform_data *)pdev->dev.platform_data;

	if (!panel->pdata) {
		panel->pdata = l5f31188_parse_dt(panel);
		if (!panel->pdata) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return -ENODEV;
		}
	}

	panel->regs[0].supply = "vddi";
	panel->regs[1].supply = "vdd";

	ret = regulator_bulk_get(&pdev->dev, NUM_REGULATOR_CONSUMERS,
			panel->regs);
	if (ret != 0) {
		dev_err(panel->dev, "failed to get regulators (%d)\n", ret);
		goto err_regulator_bulk_get;
	}

	panel->bd = backlight_device_register("l5f31188_bd", &pdev->dev, panel,
			&l5f31188_backlight_ops, NULL);
	if (IS_ERR(panel->bd)) {
		dev_err(&pdev->dev, "failed to register backlight ops.\n");
		ret = PTR_ERR(panel->bd);
		goto err_backlight_register;
	}

	panel->cabc_mode = CABC_OFF;
	panel->bd->props.max_brightness = MAX_BRIGHTNESS;
	panel->bd->props.brightness = MAX_BRIGHTNESS;

	panel->entity.of_node = pdev->dev.of_node;
	panel->entity.dev = &pdev->dev;
	panel->entity.release = l5f31188_release;
	panel->entity.ops = &l5f31188_control_ops;

	platform_set_drvdata(pdev, panel);

	ret = display_entity_register(&panel->entity);
	if (ret < 0)
		goto err_display_register;

	display_entity_set_state(&panel->entity, DISPLAY_ENTITY_STATE_ON);

	dev_dbg(&pdev->dev, "probed l5f31188 panel driver.\n");


	return 0;

err_display_register:
	backlight_device_unregister(panel->bd);
err_backlight_register:
	regulator_bulk_free(NUM_REGULATOR_CONSUMERS, panel->regs);
err_regulator_bulk_get:
	kfree(panel);

	return ret;
}

static int l5f31188_remove(struct platform_device *pdev)
{
	struct l5f31188 *panel = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	display_entity_unregister(&panel->entity);

	return 0;
}

static int l5f31188_suspend(struct device *dev)
{
	return 0;
}

static int l5f31188_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops l5f31188_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(l5f31188_suspend, l5f31188_resume)
};

static struct platform_driver l5f31188_driver = {
	.probe = l5f31188_probe,
	.remove = l5f31188_remove,
	.driver = {
		.name = "panel_l5f31188",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(l5f31188_of_match),
		.pm = &l5f31188_pm_ops,
	}
};

module_platform_driver(l5f31188_driver);

MODULE_AUTHOR("Hyungwon Hwang<human.hwang@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based l5f31188 TFT-LCD Panel Driver");
MODULE_LICENSE("GPL");
