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

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#define GAMMA_LEVEL_NUM		30
#define MTP_ID_LEN		6
/* Manufacturer Command Set */
#define MCS_MTP_ID		0xD3

struct s6e63j0x03 {
	struct device *dev;
	struct drm_panel panel;

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

	int brightness;

	/* This field is tested by functions directly accessing DSI bus before
	 * transfer, transfer is skipped if it is set. In case of transfer
	 * failure or unexpected response the field is set to error value.
	 * Such construct allows to eliminate many checks in higher level
	 * functions.
	 */
	int error;
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

static int s6e63j0x03_disable(struct drm_panel *panel)
{
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);

	s6e63j0x03_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	s6e63j0x03_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);

	s6e63j0x03_clear_error(ctx);

	usleep_range(ctx->power_off_delay * 1000,
			(ctx->power_off_delay + 1) * 1000);

	return s6e63j0x03_power_off(ctx);
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
		s6e63j0x03_disable(panel);

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

	ctx->brightness = GAMMA_LEVEL_NUM - 1;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &s6e63j0x03_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	return ret;
}

static int s6e63j0x03_remove(struct mipi_dsi_device *dsi)
{
	struct s6e63j0x03 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

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
