/* linux/drivers/video/backlight/s6d6aa1.c
 *
 * MIPI-DSI based s6d6aa1 TFT-LCD 4.77 inch panel driver.
 *
 * Joongmock Shin <jmock.shin@samsung.com>
 * Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/backlight.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/lcd.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/wait.h>

#include <video/display.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/panel-s6d6aa1.h>

#define MAX_BRIGHTNESS		69
#define DSCTL_VFLIP		1 << 7
#define DSCTL_HFLIP		1 << 6

/* white magic mode */
enum wm_mode {
	WM_MODE_MIN = 0x00,
	WM_MODE_NORMAL = WM_MODE_MIN,
	WM_MODE_CONSERVATIVE,
	WM_MODE_MEDIUM,
	WM_MODE_AGGRESSIVE,
	WM_MODE_OUTDOOR,
	WM_MODE_MAX = WM_MODE_OUTDOOR
};

struct s6d6aa1 {
	struct display_entity entity;
	struct device *dev;

	struct s6d6aa1_platform_data *pdata;
	struct lcd_device *ld;
	struct backlight_device *bd;
	struct regulator_bulk_data supplies[2];

	unsigned int	ver;
	unsigned int	power;
	enum wm_mode	wm_mode;

	unsigned int reset_gpio;
};

#define to_panel(p)	container_of(p, struct s6d6aa1, entity)

static void s6d6aa1_sleep_in(struct s6d6aa1 *lcd)
{
	static const unsigned char data_to_send[] = {
		0x10, 0x00, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_sleep_out(struct s6d6aa1 *lcd)
{
	static const unsigned char data_to_send[] = {
		0x11, 0x00, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_register_access_dis_1(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xF0, 0xA5, 0xA5
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_register_access_dis_2(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xF1, 0xA5, 0xA5
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_apply_level_1_key(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xF0, 0x5A, 0x5A
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_apply_level_2_key(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xF1, 0x5A, 0x5A
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_write_ddb(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xB4, 0x59, 0x10, 0x10, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_bcm_mode(struct s6d6aa1 *lcd)
{
	static const unsigned char data_to_send[] = {
		0xC1, 0x03, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_wrbl_ctl(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xC3, 0x7C, 0x00, 0x22
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_sony_ip_setting(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send1[] = {
		0xC4, 0x7C, 0xE6, 0x7C, 0xE6, 0x7C, 0xE6, 0x7C,
		0x7C, 0x05, 0x0F, 0x1F, 0x01, 0x00, 0x00,
	};
	const unsigned char data_to_send2[] = {
		0xC5, 0x80, 0x80, 0x80, 0x41, 0x43, 0x34,
		0x80, 0x80, 0x01, 0xFF, 0x25, 0x58, 0x50,
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send1, ARRAY_SIZE(data_to_send1));

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send2, ARRAY_SIZE(data_to_send2));
}

static void s6d6aa1_disp_ctl(struct s6d6aa1 *lcd)
{
	static unsigned char data_to_send[3] = {
		0x36, 0x00, 0x00
	};

	if (lcd->pdata->flip_vertical)
		data_to_send[1] |= DSCTL_VFLIP;

	if (lcd->pdata->flip_horizontal)
		data_to_send[1] |= DSCTL_HFLIP;

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_source_ctl(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xF2, 0x03, 0x03, 0x91, 0x85
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_pwr_ctl(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send[] = {
		0xF4, 0x04, 0x0B, 0x07, 0x07, 0x10, 0x14, 0x0D, 0x0C,
		0xAD, 0x00, 0x33, 0x33
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_panel_ctl(struct s6d6aa1 *lcd, int high_freq)
{
	const unsigned char data_to_send[] = {
		0xF6, 0x0B, 0x11, 0x0F, 0x25, 0x0A, 0x00, 0x13, 0x22,
		0x1B, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x03,
		0x12, 0x32, 0x51
	};

	/* ToDo : Low requency control */

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_mount_ctl(struct s6d6aa1 *lcd)
{
	static const unsigned char data_to_send[] = {
		0xF7, 0x00, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static int s6d6aa1_gamma_ctrl(struct s6d6aa1 *lcd)
{
	const unsigned char data_to_send1[] = {
		0xFA, 0x9C, 0xBF, 0x1A, 0xD6, 0xE3, 0xE3, 0x1B,
		0xDA, 0x9B, 0x16, 0x51, 0x12, 0x15, 0xD9, 0x9B,
		0x1A, 0xDD, 0x62, 0x2D, 0x79, 0x6A, 0x2C, 0x7F,
		0x11, 0x09, 0x53, 0x91, 0x09, 0x08, 0xCA, 0x06,
		0x07, 0x89, 0x0D, 0x52, 0xD5, 0x16, 0xD9, 0x1C,
		0x1E, 0x9E, 0xC1, 0x0F, 0xFF, 0xD7, 0x53, 0x61,
		0xE2, 0x5A, 0x9A, 0xDC, 0x5A, 0x97, 0x99, 0x5D,
		0x21, 0xE5, 0x26, 0xE9, 0x2C, 0x2A, 0x2A, 0x4F,
	};

	const unsigned char data_to_send2[] = {
		0xFB, 0x9C, 0xBF, 0x1A, 0xD6, 0xE3, 0xE3, 0x1B,
		0xDA, 0x9B, 0x16, 0x51, 0x12, 0x15, 0xD9, 0x9B,
		0x1A, 0xDD, 0x62, 0x2D, 0x79, 0x6A, 0x2C, 0x7F,
		0x11, 0x09, 0x53, 0x91, 0x09, 0x08, 0xCA, 0x06,
		0x07, 0x89, 0x0D, 0x52, 0xD5, 0x16, 0xD9, 0x1C,
		0x1E, 0x9E, 0xC1, 0x0F, 0xFF, 0xD7, 0x53, 0x61,
		0xE2, 0x5A, 0x9A, 0xDC, 0x5A, 0x97, 0x99, 0x5D,
		0x21, 0xE5, 0x26, 0xE9, 0x2C, 0x2A, 0x2A, 0x4F,
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send1, ARRAY_SIZE(data_to_send1));

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send2, ARRAY_SIZE(data_to_send2));

	return 0;
}

static int s6d6aa1_panel_init(struct s6d6aa1 *lcd)
{
	s6d6aa1_sleep_out(lcd);

	msleep(140);

	s6d6aa1_write_ddb(lcd);
	s6d6aa1_bcm_mode(lcd);
	s6d6aa1_wrbl_ctl(lcd);
	s6d6aa1_sony_ip_setting(lcd);
	s6d6aa1_disp_ctl(lcd);
	s6d6aa1_source_ctl(lcd);
	s6d6aa1_pwr_ctl(lcd);
	s6d6aa1_panel_ctl(lcd, 1);
	s6d6aa1_mount_ctl(lcd);

	s6d6aa1_gamma_ctrl(lcd);

	return 0;
}

static void s6d6aa1_write_disbv(struct s6d6aa1 *lcd,
				unsigned int brightness)
{
	static unsigned char data_to_send[3] = {
		0x51, 0x00, 0x00
	};

	data_to_send[1] = brightness;
	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_write_ctrld(struct s6d6aa1 *lcd)
{
	static const unsigned char data_to_send[] = {
		0x53, 0x2C, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_write_cabc(struct s6d6aa1 *lcd,
			enum wm_mode wm_mode)
{
	static unsigned char data_to_send[3] = {
		0x55, 0x00, 0x00
	};

	data_to_send[1] = wm_mode;
	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_display_on(struct s6d6aa1 *lcd)
{
	static const unsigned char data_to_send[] = {
		0x29, 0x00, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_display_off(struct s6d6aa1 *lcd)
{
	static const unsigned char data_to_send[] = {
		0x28, 0x00, 0x00
	};

	dsi_dcs_write(lcd->entity.source, 0,
		data_to_send, ARRAY_SIZE(data_to_send));
}

static void s6d6aa1_brightness_ctrl(struct s6d6aa1 *lcd,
				unsigned int brightness)
{
	const unsigned int convert_table[] = {
		8, 9, 11, 13, 15, 17, 19, 21, 23, 25,
		27, 29, 31, 33, 35, 37, 39, 41, 43, 45,
		47, 49, 51, 53, 55, 57, 59, 61, 63, 65,
		67, 69, 71, 73, 75, 77, 79, 81, 83, 85,
		87, 89, 91, 93, 95, 97, 99, 101, 103, 105,
		107, 109, 111, 113, 115, 117, 119, 121, 123, 125,
		127, 130, 133, 136, 139, 142, 145, 149, 152, 155,
		158, 161, 164, 167, 170, 173, 176, 179, 182, 185,
		189, 192, 195, 198, 201, 204, 207, 210, 213, 216,
		219, 222, 225, 228, 232, 235, 238, 241, 244, 247,
		250,
	};

	if (brightness > ARRAY_SIZE(convert_table)-1)
		brightness = convert_table[ARRAY_SIZE(convert_table)-1];
	else
		brightness = convert_table[brightness];

	s6d6aa1_write_disbv(lcd, brightness);
}

static int s6d6aa1_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int s6d6aa1_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct s6d6aa1 *lcd = bl_get_data(bd);
	enum display_entity_state state;
	int ret;

	if (bd->props.power == FB_BLANK_POWERDOWN
	    || bd->props.state & BL_CORE_SUSPENDED)
		state = DISPLAY_ENTITY_STATE_OFF;
	else if (bd->props.power != FB_BLANK_UNBLANK)
		state = DISPLAY_ENTITY_STATE_STANDBY;
	else
		state = DISPLAY_ENTITY_STATE_ON;

	if (brightness < 0 || brightness > bd->props.max_brightness) {
		dev_err(lcd->dev,
			"lcd brightness should be between 0 and %d.\n",
			MAX_BRIGHTNESS);
		return -EINVAL;
	}

	ret = display_entity_set_state(&lcd->entity, state);
	if (ret)
		return ret;

	if (state == DISPLAY_ENTITY_STATE_OFF)
		return 0;

	s6d6aa1_brightness_ctrl(lcd, brightness);

	return 0;
}

static const struct backlight_ops s6d6aa1_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = s6d6aa1_get_brightness,
	.update_status = s6d6aa1_set_brightness,
};

static int s6d6aa1_set_power(struct lcd_device *ld, int power)
{
	struct s6d6aa1 *lcd = lcd_get_data(ld);
	enum display_entity_state state;
	int ret = 0;

	switch (power) {
	case FB_BLANK_UNBLANK:
		state = DISPLAY_ENTITY_STATE_ON;
		ret = display_entity_set_state(&lcd->entity, state);
		if (ret)
			goto unlock;

		s6d6aa1_brightness_ctrl(lcd, lcd->bd->props.brightness);
		break;
	case FB_BLANK_POWERDOWN:
		s6d6aa1_brightness_ctrl(lcd, 0);

		state = DISPLAY_ENTITY_STATE_OFF;
		ret = display_entity_set_state(&lcd->entity, state);
		if (ret)
			goto unlock;
		break;
	default:
		state = DISPLAY_ENTITY_STATE_STANDBY;
	}

	lcd->power = power;

unlock:
	return ret;
}

static int s6d6aa1_get_power(struct lcd_device *ld)
{
	struct s6d6aa1 *lcd = lcd_get_data(ld);

	return lcd->power;
}

static struct lcd_ops s6d6aa1_lcd_ops = {
	.set_power = s6d6aa1_set_power,
	.get_power = s6d6aa1_get_power,
};

static int s6d6aa1_check_mtp(struct s6d6aa1 *lcd)
{
	s6d6aa1_apply_level_1_key(lcd);
	s6d6aa1_apply_level_2_key(lcd);

	s6d6aa1_register_access_dis_1(lcd);
	s6d6aa1_register_access_dis_2(lcd);

	return 0;
}

static void s6d6aa1_set_sequence(struct s6d6aa1 *lcd)
{
	struct backlight_device *bd = lcd->bd;
	int brightness = bd->props.brightness;

	s6d6aa1_check_mtp(lcd);
	s6d6aa1_panel_init(lcd);
	s6d6aa1_brightness_ctrl(lcd, brightness);
	s6d6aa1_write_ctrld(lcd);
	s6d6aa1_write_cabc(lcd, lcd->wm_mode);
	s6d6aa1_display_on(lcd);

	dev_info(lcd->dev, "%s:done.\n", __func__);
}

#ifdef CONFIG_OF
static int s6d6aa1_generic_reset(struct device *dev)
{
	struct s6d6aa1 *lcd = dev_get_drvdata(dev);

	gpio_set_value(lcd->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpio_set_value(lcd->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpio_set_value(lcd->reset_gpio, 1);

	return 0;
}

static struct s6d6aa1_platform_data *s6d6aa1_parse_dt(struct s6d6aa1 *lcd)
{
	struct device_node *node = lcd->dev->of_node;
	struct s6d6aa1_platform_data *data;
	const __be32 *prop_data;
	int ret;

	data = devm_kzalloc(lcd->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(lcd->dev, "failed to allocate platform data.\n");
		return NULL;
	}

	ret = of_get_videomode(node, &data->mode, 0);
	if (ret) {
		dev_err(lcd->dev, "failed to read video mode from DT\n");
		return NULL;
	}

	lcd->reset_gpio = of_get_named_gpio(node, "reset-gpio", 0);
	if (lcd->reset_gpio < 0)
		return NULL;

	prop_data = of_get_property(node, "reset-delay", NULL);
	if (!prop_data)
		return NULL;
	data->reset_delay = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "power-on-delay", NULL);
	if (!prop_data)
		return NULL;
	data->power_on_delay = be32_to_cpu(*prop_data);

	if (of_find_property(node, "flip-horizontal", NULL))
		data->flip_horizontal = true;

	if (of_find_property(node, "flip-vertical", NULL))
		data->flip_vertical = true;

	data->reset = s6d6aa1_generic_reset;

	return data;
}

static struct of_device_id s6d6aa1_of_match[] = {
	{ .compatible = "samsung,s6d6aa1" },
	{ }
};

MODULE_DEVICE_TABLE(of, s6d6aa1_of_match);
#else
static struct s6d6aa1_platform_data *s6d6aa1_parse_dt(struct s6d6aa1 *lcd)
{
	return NULL;
}
#endif

static const struct display_entity_interface_params s6d6aa1_params = {
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

static void s6d6aa1_power_on(struct s6d6aa1 *panel)
{
	struct video_source *src = panel->entity.source;

	regulator_bulk_enable(ARRAY_SIZE(panel->supplies),
						panel->supplies);

	msleep(panel->pdata->power_on_delay);

	/* lcd reset */
	if (panel->pdata->reset)
		panel->pdata->reset(panel->dev);

	msleep(panel->pdata->reset_delay);

	src->ops.dsi->enable(src);

	s6d6aa1_set_sequence(panel);
}

static void s6d6aa1_power_off(struct s6d6aa1 *panel)
{
	struct video_source *src = panel->entity.source;

	s6d6aa1_sleep_in(panel);
	s6d6aa1_display_off(panel);

	src->ops.dsi->disable(src);

	regulator_bulk_disable(ARRAY_SIZE(panel->supplies),
						panel->supplies);
}

static int s6d6aa1_set_state(struct display_entity *entity,
			    enum display_entity_state state)
{
	struct s6d6aa1 *panel = to_panel(entity);
	struct video_source *src = panel->entity.source;

	switch (state) {
	case DISPLAY_ENTITY_STATE_OFF:
		if (entity->state == DISPLAY_ENTITY_STATE_ON)
			src->common_ops->set_stream(src,
						DISPLAY_ENTITY_STREAM_STOPPED);
		s6d6aa1_power_off(panel);
		break;

	case DISPLAY_ENTITY_STATE_STANDBY:
		if (entity->state == DISPLAY_ENTITY_STATE_OFF)
			s6d6aa1_power_on(panel);

		s6d6aa1_display_off(panel);
		s6d6aa1_sleep_in(panel);
		msleep(30);

		if (entity->state == DISPLAY_ENTITY_STATE_ON)
			src->common_ops->set_stream(src,
						DISPLAY_ENTITY_STREAM_STOPPED);
		break;

	case DISPLAY_ENTITY_STATE_ON:
		if (entity->state == DISPLAY_ENTITY_STATE_OFF)
			s6d6aa1_power_on(panel);

		src->common_ops->set_stream(src,
					DISPLAY_ENTITY_STREAM_CONTINUOUS);

		if (entity->state == DISPLAY_ENTITY_STATE_STANDBY) {
			s6d6aa1_sleep_out(panel);
			s6d6aa1_display_on(panel);
		}
		break;

	default:
		break;
	}

	return 0;
}

static int s6d6aa1_get_modes(struct display_entity *entity,
			    const struct videomode **modes)
{
	struct s6d6aa1 *panel = to_panel(entity);

	*modes = &panel->pdata->mode;
	return 1;
}

static int s6d6aa1_get_size(struct display_entity *entity,
			   unsigned int *width, unsigned int *height)
{
	struct s6d6aa1 *panel = to_panel(entity);

	*width = panel->pdata->width;
	*height = panel->pdata->height;
	return 0;
}

static int s6d6aa1_get_params(struct display_entity *entity,
				struct display_entity_interface_params *params)
{
	*params = s6d6aa1_params;
	return 0;
}

static const struct display_entity_control_ops s6d6aa1_control_ops = {
	.set_state = s6d6aa1_set_state,
	.get_modes = s6d6aa1_get_modes,
	.get_size = s6d6aa1_get_size,
	.get_params = s6d6aa1_get_params,
};

static void s6d6aa1_release(struct display_entity *entity)
{
	struct s6d6aa1 *panel = to_panel(entity);

	backlight_device_unregister(panel->bd);
	regulator_bulk_free(ARRAY_SIZE(panel->supplies), panel->supplies);
}

static int s6d6aa1_probe(struct platform_device *pdev)
{
	struct s6d6aa1 *lcd;
	int ret;

	lcd = devm_kzalloc(&pdev->dev, sizeof(struct s6d6aa1), GFP_KERNEL);
	if (!lcd) {
		dev_err(&pdev->dev, "failed to allocate s6d6aa1 structure.\n");
		return -ENOMEM;
	}

	lcd->dev = &pdev->dev;
	lcd->pdata = (struct s6d6aa1_platform_data *)pdev->dev.platform_data;

	if (!lcd->pdata) {
		lcd->pdata = s6d6aa1_parse_dt(lcd);
		if (!lcd->pdata) {
			dev_err(&pdev->dev, "failed to find platform data\n");
			return -ENODEV;
		}
	}

	lcd->supplies[0].supply = "vddi";
	lcd->supplies[1].supply = "vdd";
	ret = regulator_bulk_get(&pdev->dev,
				ARRAY_SIZE(lcd->supplies), lcd->supplies);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get regulators: %d\n", ret);
		return ret;
	}

	lcd->ld = lcd_device_register("s6d6aa1", &pdev->dev, lcd,
			&s6d6aa1_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(lcd->ld);
		goto err_lcd_register;
	}

	lcd->bd = backlight_device_register("s6d6aa1", &pdev->dev, lcd,
			&s6d6aa1_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dev_err(&pdev->dev, "failed to register backlight ops.\n");
		ret = PTR_ERR(lcd->bd);
		goto err_backlight_register;
	}

	lcd->wm_mode = WM_MODE_NORMAL;

	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = MAX_BRIGHTNESS;

	lcd->entity.of_node = pdev->dev.of_node;
	lcd->entity.dev = &pdev->dev;
	lcd->entity.release = s6d6aa1_release;
	lcd->entity.ops = &s6d6aa1_control_ops;

	platform_set_drvdata(pdev, lcd);

	ret = display_entity_register(&lcd->entity);
	if (ret < 0)
		goto err_display_register;

	display_entity_set_state(&lcd->entity, DISPLAY_ENTITY_STATE_ON);

	dev_dbg(&pdev->dev, "probed s6d6aa1 panel driver.\n");

	return 0;

err_display_register:
	backlight_device_unregister(lcd->bd);
err_backlight_register:
	lcd_device_unregister(lcd->ld);
err_lcd_register:
	regulator_bulk_free(ARRAY_SIZE(lcd->supplies), lcd->supplies);

	return ret;
}

static int s6d6aa1_remove(struct platform_device *dev)
{
	struct s6d6aa1 *lcd = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);
	display_entity_unregister(&lcd->entity);

	return 0;
}

static int s6d6aa1_suspend(struct device *dev)
{
	return 0;
}

static int s6d6aa1_resume(struct device *dev)
{
	return 0;
}

static struct dev_pm_ops s6d6aa1_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(s6d6aa1_suspend, s6d6aa1_resume)
};

static struct platform_driver s6d6aa1_driver = {
	.probe = s6d6aa1_probe,
	.remove = s6d6aa1_remove,
	.driver = {
		.name = "panel_s6d6aa1",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(s6d6aa1_of_match),
		.pm = &s6d6aa1_pm_ops,
	},
};
module_platform_driver(s6d6aa1_driver);

MODULE_AUTHOR("Joongmock Shin <jmock.shin@samsung.com>");
MODULE_AUTHOR("Eunchul Kim <chulspro.kim@samsung.com>");
MODULE_AUTHOR("Tomasz Figa <t.figa@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based s6d6aa1 TFT-LCD Panel Driver");
MODULE_LICENSE("GPL");
