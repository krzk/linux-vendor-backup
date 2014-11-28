/*
 * MIPI-DSI based s6e3ha2 AMOLED 5.7 inch panel driver.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * Donghwa Lee <dh09.lee@samsung.com>
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

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

struct s6e3ha2 {
	struct device *dev;
	struct drm_panel panel;

	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	struct gpio_desc *panel_en_gpio;
	struct videomode vm;
	u32 width_mm;
	u32 height_mm;

	/* This field is tested by functions directly accessing DSI bus before
	 * transfer, transfer is skipped if it is set. In case of transfer
	 * failure or unexpected response the field is set to error value.
	 * Such construct allows to eliminate many checks in higher level
	 * functions.
	 */
	int error;
};

static inline struct s6e3ha2 *panel_to_s6e3ha2(struct drm_panel *panel)
{
	return container_of(panel, struct s6e3ha2, panel);
}

static void s6e3ha2_clear_error(struct s6e3ha2 *ctx)
{
	ctx->error = 0;
}

static void s6e3ha2_dcs_write(struct s6e3ha2 *ctx, const u8 cmd,
		const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_dcs_write(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing dcs seq: %*ph\n", ret,
							(int)len, data);
		ctx->error = ret;
	}
}

#define s6e3ha2_dcs_write_seq_static(ctx, cmd, seq...) \
({\
	static const u8 c = cmd;\
	static const u8 d[] = { seq };\
	s6e3ha2_dcs_write(ctx, c, d, ARRAY_SIZE(d));\
})

static void s6e3ha2_test_key_on_f0(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf0, 0x5a, 0x5a);
}

static void s6e3ha2_test_key_off_f0(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf0, 0xa5, 0xa5);
}

static void s6e3ha2_test_key_on_fc(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfc, 0x5a, 0x5a);
}

static void s6e3ha2_test_key_off_fc(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfc, 0xa5, 0xa5);
}

static void s6e3ha2_single_dsi_set1(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf2, 0x67);
}

static void s6e3ha2_single_dsi_set2(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf9, 0x09);
}

static void s6e3ha2_calibration_set1(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfd, 0x1c);
}

static void s6e3ha2_calibration_set2(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0x20, 0x39);
}

static void s6e3ha2_calibration_set3(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0xa0);
}

static void s6e3ha2_calibration_set4(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0x20);
}

static void s6e3ha2_calibration_set5(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xce,
			0x03, 0x3B, 0x12, 0x62, 0x40, 0x80, 0xC0, 0x28, 0x28,
			0x28, 0x28, 0x39, 0xC5);
}

static void s6e3ha2_aor_control(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb2, 0x03, 0x10);
}

static void s6e3ha2_caps_elvss_set(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb6, 0x9c, 0x0a);
}

static void s6e3ha2_acl_off(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0x55, 0x00);
}

static void s6e3ha2_acl_off_opr(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb5, 0x40);
}

static void s6e3ha2_test_global(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb0, 0x07);
}

static void s6e3ha2_test(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb8, 0x19);
}

static void s6e3ha2_touch_hsync_on1(struct s6e3ha2 *ctx) {
	s6e3ha2_dcs_write_seq_static(ctx,
			0xbd, 0x33, 0x11, 0x02, 0x16, 0x02, 0x16);
}

static void s6e3ha2_pentile_control(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xc0, 0x00, 0x00, 0xd8, 0xd8);
}

static void s6e3ha2_poc_global(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb0, 0x20);
}

static void s6e3ha2_poc_setting(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xfe, 0x08);
}

static void s6e3ha2_pcd_set_off(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xcc, 0x40, 0x51);
}

static void s6e3ha2_err_fg_set(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xed, 0x44);
}

static void s6e3ha2_hbm_off(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0x53, 0x00);
}

static void s6e3ha2_te_start_setting(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xb9, 0x10, 0x09, 0xff, 0x00, 0x09);
}


static void s6e3ha2_gamma_update(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, 0xf7, 0x03);
}

static void s6e3ha2_brightness_set(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx,
		0xca, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x00, 0x00);
}

static void s6e3ha2_panel_init(struct s6e3ha2 *ctx)
{
	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_EXIT_SLEEP_MODE);

	usleep_range(5000, 6000);

	s6e3ha2_test_key_on_f0(ctx);
	s6e3ha2_single_dsi_set1(ctx);
	s6e3ha2_single_dsi_set2(ctx);

	/* calibration enable */
	s6e3ha2_test_key_on_fc(ctx);
	s6e3ha2_calibration_set1(ctx);
	s6e3ha2_calibration_set2(ctx);
	s6e3ha2_calibration_set3(ctx);
	s6e3ha2_calibration_set4(ctx);
	s6e3ha2_calibration_set5(ctx);

	msleep(120);

	/* common setting */
	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_TEAR_ON);

	s6e3ha2_touch_hsync_on1(ctx);
	s6e3ha2_pentile_control(ctx);
	s6e3ha2_poc_global(ctx);
	s6e3ha2_poc_setting(ctx);
	s6e3ha2_test_key_off_fc(ctx);

	/* pcd setting off for TB */
	s6e3ha2_pcd_set_off(ctx);
	s6e3ha2_err_fg_set(ctx);
	s6e3ha2_te_start_setting(ctx);

	/* brightness setting */
	s6e3ha2_brightness_set(ctx);
	s6e3ha2_aor_control(ctx);
	s6e3ha2_caps_elvss_set(ctx);
	s6e3ha2_gamma_update(ctx);
	s6e3ha2_acl_off(ctx);
	s6e3ha2_acl_off_opr(ctx);
	s6e3ha2_hbm_off(ctx);

	/* elvss temp compensation */
	s6e3ha2_test_global(ctx);
	s6e3ha2_test(ctx);

	s6e3ha2_test_key_off_f0(ctx);
	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_ON);
}

static int s6e3ha2_power_off(struct s6e3ha2 *ctx)
{
	return regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
}

static int s6e3ha2_disable(struct drm_panel *panel)
{
	return 0;
}

static int s6e3ha2_unprepare(struct drm_panel *panel)
{
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);

	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	s6e3ha2_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(40);

	s6e3ha2_clear_error(ctx);

	return s6e3ha2_power_off(ctx);
}

static int s6e3ha2_power_on(struct s6e3ha2 *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	msleep(25);

	gpiod_set_value(ctx->panel_en_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->panel_en_gpio, 1);

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 6000);

	return 0;
}
static int s6e3ha2_prepare(struct drm_panel *panel)
{
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);
	int ret;

	ret = s6e3ha2_power_on(ctx);
	if (ret < 0)
		return ret;

	s6e3ha2_panel_init(ctx);
	if (ctx->error < 0)
		s6e3ha2_unprepare(panel);

	return ret;
}

static int s6e3ha2_enable(struct drm_panel *panel)
{
	return 0;
}

static int s6e3ha2_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct s6e3ha2 *ctx = panel_to_s6e3ha2(panel);
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

static const struct drm_panel_funcs s6e3ha2_drm_funcs = {
	.disable = s6e3ha2_disable,
	.unprepare = s6e3ha2_unprepare,
	.prepare = s6e3ha2_prepare,
	.enable = s6e3ha2_enable,
	.get_modes = s6e3ha2_get_modes,
};

static int s6e3ha2_parse_dt(struct s6e3ha2 *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_get_videomode(np, &ctx->vm, 0);
	if (ret < 0)
		return ret;

	of_property_read_u32(np, "panel-width-mm", &ctx->width_mm);
	of_property_read_u32(np, "panel-height-mm", &ctx->height_mm);

	return 0;
}

static int s6e3ha2_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct s6e3ha2 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(struct s6e3ha2), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = s6e3ha2_parse_dt(ctx);
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

	ctx->reset_gpio = devm_gpiod_get(dev, "reset");
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	ret = gpiod_direction_output(ctx->reset_gpio, 1);
	if (ret < 0) {
		dev_err(dev, "cannot configure reset-gpios %d\n", ret);
		return ret;
	}

	ctx->panel_en_gpio = devm_gpiod_get(dev, "panel-en");
	if (IS_ERR(ctx->panel_en_gpio)) {
		dev_err(dev, "cannot get panel-en-gpios %ld\n",
			PTR_ERR(ctx->panel_en_gpio));
		return PTR_ERR(ctx->panel_en_gpio);
	}
	ret = gpiod_direction_output(ctx->panel_en_gpio, 1);
	if (ret < 0) {
		dev_err(dev, "cannot configure panel-en-gpios %d\n", ret);
		return ret;
	}

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &s6e3ha2_drm_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	return ret;
}

static int s6e3ha2_remove(struct mipi_dsi_device *dsi)
{
	struct s6e3ha2 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static struct of_device_id s6e3ha2_of_match[] = {
	{ .compatible = "samsung,s6e3ha2" },
	{ }
};
MODULE_DEVICE_TABLE(of, s6e3ha2_of_match);

static struct mipi_dsi_driver s6e3ha2_driver = {
	.probe = s6e3ha2_probe,
	.remove = s6e3ha2_remove,
	.driver = {
		.name = "panel_s6e3ha2",
		.owner = THIS_MODULE,
		.of_match_table = s6e3ha2_of_match,
	},
};
module_mipi_dsi_driver(s6e3ha2_driver);

MODULE_AUTHOR("Donghwa Lee <dh09.lee@samsung.com>");
MODULE_AUTHOR("Hyungwon Hwang <human.hwang@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based s6e3ha2 AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
