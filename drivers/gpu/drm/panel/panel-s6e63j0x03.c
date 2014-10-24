/*
 * MIPI-DSI based S6E63J0X03 AMOLED lcd 1.63 inch panel driver.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * Inki Dae, <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/backlight.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#define GAMMA_LEVEL_NUM		30
#define MTP_ID_LEN		6
/* Manufacturer Command Set */
#define MCS_MTP_ID		0xD3

#define MCS_MTP_SET3		0xd4
#define MCS_MTP_KEY		0xf1

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DEFAULT_BRIGHTNESS	80

#define GAMMA_CMD_CNT	28
#define MAX_GAMMA_CNT	11

struct s6e63j0x03 {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *bl_dev;

	struct regulator_bulk_data supplies[2];
	int reset_gpio;
	int te_gpio;
	u32 power_on_delay;
	u32 power_off_delay;
	u32 reset_delay;
	u32 init_delay;
	bool flip_horizontal;
	bool flip_vertical;
	struct videomode vm;
	u32 width_mm;
	u32 height_mm;

	int power;

	/* This field is tested by functions directly accessing DSI bus before
	 * transfer, transfer is skipped if it is set. In case of transfer
	 * failure or unexpected response the field is set to error value.
	 * Such construct allows to eliminate many checks in higher level
	 * functions.
	 */
	int error;
};

static const unsigned char GAMMA_10[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x7f, 0x7f, 0x7f, 0x52, 0x6b, 0x6f, 0x26, 0x28, 0x2d,
	0x28, 0x26, 0x27, 0x33, 0x34, 0x32, 0x36, 0x36, 0x35, 0x00, 0xab, 0x00,
	0xae, 0x00, 0xbf
};

static const unsigned char GAMMA_30[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x70, 0x7f, 0x7f, 0x4e, 0x64, 0x69, 0x26, 0x27, 0x2a,
	0x28, 0x29, 0x27, 0x31, 0x32, 0x31, 0x35, 0x34, 0x35, 0x00, 0xc4, 0x00,
	0xca, 0x00, 0xdc
};

static const unsigned char GAMMA_60[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x65, 0x7b, 0x7d, 0x5f, 0x67, 0x68, 0x2a, 0x28, 0x29,
	0x28, 0x2a, 0x27, 0x31, 0x2f, 0x30, 0x34, 0x33, 0x34, 0x00, 0xd9, 0x00,
	0xe4, 0x00, 0xf5
};

static const unsigned char GAMMA_90[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x4d, 0x6f, 0x71, 0x67, 0x6a, 0x6c, 0x29, 0x28, 0x28,
	0x28, 0x29, 0x27, 0x30, 0x2e, 0x30, 0x32, 0x31, 0x31, 0x00, 0xea, 0x00,
	0xf6, 0x01, 0x09
};

static const unsigned char GAMMA_120[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x3d, 0x66, 0x68, 0x69, 0x69, 0x69, 0x28, 0x28, 0x27,
	0x28, 0x28, 0x27, 0x30, 0x2e, 0x2f, 0x31, 0x31, 0x30, 0x00, 0xf9, 0x01,
	0x05, 0x01, 0x1b
};

static const unsigned char GAMMA_150[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x31, 0x51, 0x53, 0x66, 0x66, 0x67, 0x28, 0x29, 0x27,
	0x28, 0x27, 0x27, 0x2e, 0x2d, 0x2e, 0x31, 0x31, 0x30, 0x01, 0x04, 0x01,
	0x11, 0x01, 0x29
};

static const unsigned char GAMMA_200[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x2f, 0x4f, 0x51, 0x67, 0x65, 0x65, 0x29, 0x2a, 0x28,
	0x27, 0x25, 0x26, 0x2d, 0x2c, 0x2c, 0x30, 0x30, 0x30, 0x01, 0x14, 0x01,
	0x23, 0x01, 0x3b
};

static const unsigned char GAMMA_240[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x2c, 0x4d, 0x50, 0x65, 0x63, 0x64, 0x2a, 0x2c, 0x29,
	0x26, 0x24, 0x25, 0x2c, 0x2b, 0x2b, 0x30, 0x30, 0x30, 0x01, 0x1e, 0x01,
	0x2f, 0x01, 0x47
};

static const unsigned char GAMMA_300[] = {
	MCS_MTP_SET3,
	0x00, 0x00, 0x00, 0x38, 0x61, 0x64, 0x65, 0x63, 0x64, 0x28, 0x2a, 0x27,
	0x26, 0x23, 0x25, 0x2b, 0x2b, 0x2a, 0x30, 0x2f, 0x30, 0x01, 0x2d, 0x01,
	0x3f, 0x01, 0x57
};

static const unsigned char *gamma_tbl[MAX_GAMMA_CNT] = {
	GAMMA_10,
	GAMMA_30,
	GAMMA_60,
	GAMMA_90,
	GAMMA_120,
	GAMMA_150,
	GAMMA_200,
	GAMMA_200,
	GAMMA_240,
	GAMMA_300,
	GAMMA_300
};

static inline struct s6e63j0x03 *panel_to_s6e63j0x03(struct drm_panel *panel)
{
	return container_of(panel, struct s6e63j0x03, panel);
}

static int s6e63j0x03_clear_error(struct s6e63j0x03 *ctx)
{
	int ret = ctx->error;

	ctx->error = 0;
	return ret;
}

static void s6e63j0x03_dcs_write(struct s6e63j0x03 *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_dcs_write(dsi, dsi->channel, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d writing dcs seq: %*ph\n", ret, len,
			data);
		ctx->error = ret;
	}
}

static int s6e63j0x03_dcs_read(struct s6e63j0x03 *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret;

	if (ctx->error < 0)
		return ctx->error;

	ret = mipi_dsi_dcs_read(dsi, dsi->channel, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

#define s6e63j0x03_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	s6e63j0x03_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

#define s6e63j0x03_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	s6e63j0x03_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static void s6e63j0x03_test_level_1_key_on(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xf0, 0x5a, 0x5a);
}

static void s6e63j0x03_test_level_2_key_on(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xf1, 0x5a, 0x5a);
}

static void s6e63j0x03_porch_adjustment(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xf2, 0x1c, 0x28);
}

static void s6e63j0x03_frame_freq(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xb5, 0x00, 0x02, 0x00);
}

static void s6e63j0x03_mem_addr_set_0(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0x2a, 0x00, 0x14, 0x01, 0x53);
}

static void s6e63j0x03_mem_addr_set_1(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0x2b, 0x00, 0x00, 0x01, 0x3f);
}

static void s6e63j0x03_ltps_timming_set_0_60hz(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx,
			0xf8, 0x08, 0x08, 0x08, 0x17, 0x00, 0x2a, 0x02,
			0x26, 0x00, 0x00, 0x02, 0x00, 0x00);
}

static void s6e63j0x03_ltps_timming_set_1(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xf7, 0x02);
}

static void s6e63j0x03_param_pos_te_edge(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xb0, 0x01);
}

static void s6e63j0x03_te_rising_edge(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xe2, 0x0f);
}

static void s6e63j0x03_param_pos_default(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xb0, 0x00);
}

static void s6e63j0x03_elvss_cond(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xb1, 0x00, 0x09);
}

static void s6e63j0x03_set_pos(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0x36, 0x40);
}

static void s6e63j0x03_white_brightness_default(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0x51, 0xff);
}

static void s6e63j0x03_white_ctrl(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0x53, 0x20);
}

static void s6e63j0x03_acl_off(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0x55, 0x00);
}

static void s6e63j0x03_test_level_2_key_off(struct s6e63j0x03 *ctx)
{
	s6e63j0x03_dcs_write_seq_static(ctx, 0xf1, 0xa5, 0xa5);
}

static void s6e63j0x03_apply_mtp_key(struct s6e63j0x03 *ctx, bool on)
{
	if (on)
		s6e63j0x03_dcs_write_seq(ctx, MCS_MTP_KEY, 0x5a, 0x5a);
	else
		s6e63j0x03_dcs_write_seq(ctx, MCS_MTP_KEY, 0xa5, 0xa5);
}

static void s6e63j0x03_set_maximum_return_packet_size(struct s6e63j0x03 *ctx,
							unsigned int size)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	const struct mipi_dsi_host_ops *ops = dsi->host->ops;

	if (ops && ops->transfer) {
		unsigned char buf[] = {size, 0};
		struct mipi_dsi_msg msg = {
			.channel = dsi->channel,
			.type = MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE,
			.tx_len = sizeof(buf),
			.tx_buf = buf
		};

		ops->transfer(dsi->host, &msg);
	}
}

static void s6e63j0x03_read_mtp_id(struct s6e63j0x03 *ctx)
{
	unsigned char id[MTP_ID_LEN];
	int ret;

	s6e63j0x03_test_level_2_key_on(ctx);

	s6e63j0x03_set_maximum_return_packet_size(ctx, MTP_ID_LEN);
	ret = s6e63j0x03_dcs_read(ctx, MCS_MTP_ID, id, MTP_ID_LEN);

	s6e63j0x03_test_level_2_key_off(ctx);

	if (ret < MTP_ID_LEN || id[3] != 0x22) {
		dev_err(ctx->dev, "failed to read id,\n");
		return;
	}

	dev_info(ctx->dev, "ID: 0x%02x, 0x%02x, 0x%02x\n", id[3], id[2], id[5]);
}

static void s6e63j0x03_set_sequence(struct s6e63j0x03 *ctx)
{
	/* Test key enable */
	s6e63j0x03_test_level_1_key_on(ctx);
	s6e63j0x03_test_level_2_key_on(ctx);

	s6e63j0x03_porch_adjustment(ctx);
	s6e63j0x03_frame_freq(ctx);
	s6e63j0x03_frame_freq(ctx);
	s6e63j0x03_mem_addr_set_0(ctx);
	s6e63j0x03_mem_addr_set_1(ctx);

	s6e63j0x03_ltps_timming_set_0_60hz(ctx);
	s6e63j0x03_ltps_timming_set_1(ctx);

	s6e63j0x03_param_pos_te_edge(ctx);
	s6e63j0x03_te_rising_edge(ctx);
	s6e63j0x03_param_pos_default(ctx);

	s6e63j0x03_dcs_write_seq_static(ctx, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(120);

	/* etc condition setting */
	s6e63j0x03_elvss_cond(ctx);
	s6e63j0x03_set_pos(ctx);

	/* brightness setting */
	s6e63j0x03_white_brightness_default(ctx);
	s6e63j0x03_white_ctrl(ctx);
	s6e63j0x03_acl_off(ctx);

	s6e63j0x03_dcs_write_seq_static(ctx, MIPI_DCS_SET_TEAR_ON);
	s6e63j0x03_test_level_2_key_off(ctx);

	/* display on */
	s6e63j0x03_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_ON);
}

static int s6e63j0x03_power_on(struct s6e63j0x03 *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	usleep_range(ctx->power_on_delay * 1000,
			(ctx->power_on_delay + 1) * 1000);

	gpio_set_value(ctx->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpio_set_value(ctx->reset_gpio, 1);

	usleep_range(ctx->reset_delay * 1000,
			(ctx->reset_delay + 1) * 1000);

	return 0;
}

static int s6e63j0x03_power_off(struct s6e63j0x03 *ctx)
{
	int ret;

	gpio_set_value(ctx->reset_gpio, 0);

	ret = regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	return 0;
}

static int s6e63j0x03_get_brightness(struct backlight_device *bl_dev)
{
	return bl_dev->props.brightness;
}

static int s6e63j0x03_get_brightness_index(unsigned int brightness)
{
	int index;

	switch (brightness) {
	case 0 ... 20:
		index = 0;
		break;
	case 21 ... 40:
		index = 2;
		break;
	case 41 ... 60:
		index = 4;
		break;
	case 61 ... 80:
		index = 6;
		break;
	default:
		index = 10;
		break;
	}

	return index;
}

static void s6e63j0x03_update_gamma(struct s6e63j0x03 *ctx,
					unsigned int brightness)
{
	int index = s6e63j0x03_get_brightness_index(brightness);

	s6e63j0x03_apply_mtp_key(ctx, true);

	s6e63j0x03_dcs_write(ctx, gamma_tbl[index], GAMMA_CMD_CNT);

	s6e63j0x03_apply_mtp_key(ctx, false);
}

static int s6e63j0x03_set_brightness(struct backlight_device *bl_dev)
{
	struct s6e63j0x03 *ctx = (struct s6e63j0x03 *)bl_get_data(bl_dev);
	unsigned int brightness = bl_dev->props.brightness;

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bl_dev->props.max_brightness) {
		dev_err(ctx->dev, "brightness[%u] is invalid value\n",
			brightness);
		return -EINVAL;
	}

	if (ctx->power > FB_BLANK_UNBLANK) {
		dev_err(ctx->dev, "panel is not in fb unblank state\n");
		return -EPERM;
	}

	s6e63j0x03_update_gamma(ctx, brightness);

	return 0;
}

static const struct backlight_ops s6e63j0x03_bl_ops = {
	.get_brightness = s6e63j0x03_get_brightness,
	.update_status = s6e63j0x03_set_brightness,
};

static int s6e63j0x03_enable(struct drm_panel *panel);

static int s6e63j0x03_disable(struct drm_panel *panel)
{
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);
	int ret;

	s6e63j0x03_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	s6e63j0x03_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);

	s6e63j0x03_clear_error(ctx);

	usleep_range(ctx->power_off_delay * 1000,
			(ctx->power_off_delay + 1) * 1000);

	ret = s6e63j0x03_power_off(ctx);
	if (ret < 0)
		return s6e63j0x03_enable(panel);

	ctx->power = FB_BLANK_POWERDOWN;

	return ret;
}

static int s6e63j0x03_enable(struct drm_panel *panel)
{
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);
	int ret;

	ret = s6e63j0x03_power_on(ctx);
	if (ret < 0)
		return ret;

	s6e63j0x03_read_mtp_id(ctx);

	s6e63j0x03_set_sequence(ctx);
	ret = ctx->error;

	if (ret < 0)
		return s6e63j0x03_disable(panel);

	ctx->power = FB_BLANK_UNBLANK;

	return ret;
}

static int s6e63j0x03_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_create(connector->dev);
	if (!mode) {
		DRM_ERROR("failed to create a new display mode\n");
		return 0;
	}

	drm_display_mode_from_videomode(&ctx->vm, mode);
	mode->width_mm = ctx->width_mm;
	mode->height_mm = ctx->height_mm;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs s6e63j0x03_drm_funcs = {
	.disable = s6e63j0x03_disable,
	.enable = s6e63j0x03_enable,
	.get_modes = s6e63j0x03_get_modes,
};

static int s6e63j0x03_parse_dt(struct s6e63j0x03 *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_get_videomode(np, &ctx->vm, 0);
	if (ret < 0)
		return ret;

	of_property_read_u32(np, "power-on-delay", &ctx->power_on_delay);
	of_property_read_u32(np, "power-off-delay", &ctx->power_off_delay);
	of_property_read_u32(np, "reset-delay", &ctx->reset_delay);
	of_property_read_u32(np, "init-delay", &ctx->init_delay);

	ctx->flip_horizontal = of_property_read_bool(np, "flip-horizontal");
	ctx->flip_vertical = of_property_read_bool(np, "flip-vertical");


	ctx->te_gpio = of_get_named_gpio(dev->of_node, "te-gpios", 0);
	if (ctx->te_gpio < 0)
		return ctx->te_gpio;

	ret = devm_gpio_request(dev, ctx->te_gpio, "te-gpio");
	if (ret) {
		dev_err(dev, "failed to request te-gpio\n");
		return ret;
	}

	return gpio_direction_input(ctx->te_gpio);
}

irqreturn_t s6e63j0x03_te_interrupt(int irq, void *dev_id)
{
	struct s6e63j0x03 *ctx = dev_id;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	struct mipi_dsi_host *host = dsi->host;
	const struct mipi_dsi_host_ops *ops = host->ops;

	if (ops && ops->te_handler)
		ops->te_handler(host);

	return IRQ_HANDLED;
}

static int s6e63j0x03_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct s6e63j0x03 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(struct s6e63j0x03), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 1;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_BURST;

	ret = s6e63j0x03_parse_dt(ctx);
	if (ret < 0)
		return ret;

	ctx->supplies[0].supply = "vdd3";
	ctx->supplies[1].supply = "vci";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "failed to get regulators: %d\n", ret);
		return ret;
	}

	ctx->reset_gpio = of_get_named_gpio(dev->of_node, "reset-gpios", 0);
	if (ctx->reset_gpio < 0) {
		dev_err(dev, "cannot get reset-gpios %d\n",
			ctx->reset_gpio);
		return ctx->reset_gpio;
	}

	ret = devm_gpio_request(dev, ctx->reset_gpio, "reset-gpio");
	if (ret) {
		dev_err(dev, "failed to request reset-gpio\n");
		return ret;
	}

	ret = gpio_direction_output(ctx->reset_gpio, 1);
	if (ret < 0) {
		dev_err(dev, "cannot configure reset-gpios %d\n", ret);
		return ret;
	}

	ret = devm_request_irq(ctx->dev, gpio_to_irq(ctx->te_gpio),
				s6e63j0x03_te_interrupt,
				IRQF_TRIGGER_RISING, "TE", ctx);
	if (ret < 0) {
		dev_err(dev, "failed to request te irq.\n");
		return ret;
	}

	ctx->power = FB_BLANK_POWERDOWN;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &s6e63j0x03_drm_funcs;

	ctx->bl_dev = backlight_device_register("s6e63j0x03", dev, ctx,
						&s6e63j0x03_bl_ops, NULL);
	if (IS_ERR(ctx->bl_dev)) {
		dev_err(dev, "failed to register backlight device\n");
		return PTR_ERR(ctx->bl_dev);
	}

	ctx->bl_dev->props.max_brightness = MAX_BRIGHTNESS;
	ctx->bl_dev->props.brightness = DEFAULT_BRIGHTNESS;

	/*
	 * FIXME: probably we should also initialize backlight power:
	 * ctx->bl_dev->props.power = FB_BLANK_POWERDOWN;
	 */

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		goto err_unregister_backlight;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		goto err_remove_panel;

	return ret;

err_remove_panel:
		drm_panel_remove(&ctx->panel);

err_unregister_backlight:
	backlight_device_unregister(ctx->bl_dev);

	return ret;
}

static int s6e63j0x03_remove(struct mipi_dsi_device *dsi)
{
	struct s6e63j0x03 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	backlight_device_unregister(ctx->bl_dev);

	return 0;
}

static struct of_device_id s6e63j0x03_of_match[] = {
	{ .compatible = "samsung,s6e63j0x03" },
	{ }
};
MODULE_DEVICE_TABLE(of, s6e63j0x03_of_match);

static struct mipi_dsi_driver s6e63j0x03_driver = {
	.probe = s6e63j0x03_probe,
	.remove = s6e63j0x03_remove,
	.driver = {
		.name = "panel_s6e63j0x03",
		.owner = THIS_MODULE,
		.of_match_table = s6e63j0x03_of_match,
	},
};
module_mipi_dsi_driver(s6e63j0x03_driver);
