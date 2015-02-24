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

#define READ_ID1		0xDA
#define READ_ID2		0xDB
#define READ_ID3		0xDC

#define MCS_LEVEL2_KEY		0xf0
#define MCS_MTP_KEY		0xf1
#define MCS_MTP_SET3		0xd4

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DEFAULT_BRIGHTNESS	80

#define GAMMA_LEVEL_NUM		30
#define NUM_GAMMA_STEPS		9
#define GAMMA_CMD_CNT		28

struct s6e63j0x03 {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *bl_dev;

	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	u32 power_on_delay;
	u32 power_off_delay;
	u32 reset_delay;
	u32 init_delay;
	bool flip_horizontal;
	bool flip_vertical;
	struct videomode vm;
	unsigned int width_mm;
	unsigned int height_mm;
};

static const unsigned char gamma_tbl[NUM_GAMMA_STEPS][GAMMA_CMD_CNT] = {
	{	/* Gamma 10 */
		0x00, 0x00, 0x00, 0x7f, 0x7f, 0x7f, 0x52, 0x6b, 0x6f, 0x26,
		0x28, 0x2d, 0x28, 0x26, 0x27, 0x33, 0x34, 0x32, 0x36, 0x36,
		0x35, 0x00, 0xab, 0x00, 0xae, 0x00, 0xbf
	},
	{	/* gamma 30 */
		0x00, 0x00, 0x00, 0x70, 0x7f, 0x7f, 0x4e, 0x64, 0x69, 0x26,
		0x27, 0x2a, 0x28, 0x29, 0x27, 0x31, 0x32, 0x31, 0x35, 0x34,
		0x35, 0x00, 0xc4, 0x00, 0xca, 0x00, 0xdc
	},
	{	/* gamma 60 */
		0x00, 0x00, 0x00, 0x65, 0x7b, 0x7d, 0x5f, 0x67, 0x68, 0x2a,
		0x28, 0x29, 0x28, 0x2a, 0x27, 0x31, 0x2f, 0x30, 0x34, 0x33,
		0x34, 0x00, 0xd9, 0x00, 0xe4, 0x00, 0xf5
	},
	{	/* gamma 90 */
		0x00, 0x00, 0x00, 0x4d, 0x6f, 0x71, 0x67, 0x6a, 0x6c, 0x29,
		0x28, 0x28, 0x28, 0x29, 0x27, 0x30, 0x2e, 0x30, 0x32, 0x31,
		0x31, 0x00, 0xea, 0x00, 0xf6, 0x01, 0x09
	},
	{	/* gamma 120 */
		0x00, 0x00, 0x00, 0x3d, 0x66, 0x68, 0x69, 0x69, 0x69, 0x28,
		0x28, 0x27, 0x28, 0x28, 0x27, 0x30, 0x2e, 0x2f, 0x31, 0x31,
		0x30, 0x00, 0xf9, 0x01, 0x05, 0x01, 0x1b
	},
	{	/* gamma 150 */
		0x00, 0x00, 0x00, 0x31, 0x51, 0x53, 0x66, 0x66, 0x67, 0x28,
		0x29, 0x27, 0x28, 0x27, 0x27, 0x2e, 0x2d, 0x2e, 0x31, 0x31,
		0x30, 0x01, 0x04, 0x01, 0x11, 0x01, 0x29
	},
	{	/* gamma 200 */
		0x00, 0x00, 0x00, 0x2f, 0x4f, 0x51, 0x67, 0x65, 0x65, 0x29,
		0x2a, 0x28, 0x27, 0x25, 0x26, 0x2d, 0x2c, 0x2c, 0x30, 0x30,
		0x30, 0x01, 0x14, 0x01, 0x23, 0x01, 0x3b
	},
	{	/* gamma 240 */
		0x00, 0x00, 0x00, 0x2c, 0x4d, 0x50, 0x65, 0x63, 0x64, 0x2a,
		0x2c, 0x29, 0x26, 0x24, 0x25, 0x2c, 0x2b, 0x2b, 0x30, 0x30,
		0x30, 0x01, 0x1e, 0x01, 0x2f, 0x01, 0x47
	},
	{	/* gamma 300 */
		0x00, 0x00, 0x00, 0x38, 0x61, 0x64, 0x65, 0x63, 0x64, 0x28,
		0x2a, 0x27, 0x26, 0x23, 0x25, 0x2b, 0x2b, 0x2a, 0x30, 0x2f,
		0x30, 0x01, 0x2d, 0x01, 0x3f, 0x01, 0x57
	}
};

static const unsigned char prepare_cmds1[][15] = {
	{ 3, 0xf2, 0x1c, 0x28 },		/* porch_adjustment */
	{ 4, 0xb5, 0x00, 0x02, 0x00 },		/* frame_freq */
	{ 5, 0x2a, 0x00, 0x14, 0x01, 0x53 },	/* mem_addr_set_0 */
	{ 5, 0x2b, 0x00, 0x00, 0x01, 0x3f },	/* mem_addr_set_1 */
	{ 14, 0xf8, 0x08, 0x08, 0x08, 0x17,	/* ltps_timming_set_0_60hz */
	      0x00, 0x2a, 0x02, 0x26,
	      0x00, 0x00, 0x02, 0x00, 0x00 },
	{ 2, 0xf7, 0x02 },			/* ltps_timming_set_1 */
	{ 2, 0xb0, 0x01 },			/* param_pos_te_edge */
	{ 2, 0xe2, 0x0f },			/* te_rising_edge */
	{ 2, 0xb0, 0x00 },			/* param_pos_default */
	{ 1, MIPI_DCS_EXIT_SLEEP_MODE },
};

static const  unsigned char prepare_cmds2[][4] = {
	{ 3, 0xb1, 0x00, 0x09 },	/* elvss_cond */
	{ 2, 0x36, 0x40 },		/* set_pos */
	{ 2, 0x51, 0xff },		/* white_brightness_default */
	{ 2, 0x53, 0x20 },		/* white_ctrl */
	{ 2, 0x55, 0x00 },		/* acl_off */
	{ 1, MIPI_DCS_SET_TEAR_ON },
};

static inline struct s6e63j0x03 *panel_to_s6e63j0x03(struct drm_panel *panel)
{
	return container_of(panel, struct s6e63j0x03, panel);
}

static inline ssize_t s6e63j0x03_dcs_write_seq(struct s6e63j0x03 *ctx,
			const u8 cmd, const u8 *seq, const unsigned char len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	return mipi_dsi_dcs_write(dsi, cmd, seq, len);
}

static inline int s6e63j0x03_enable_lv2_command(struct s6e63j0x03 *ctx)
{
	unsigned char seq[] = { 0x5a, 0x5a };

	return s6e63j0x03_dcs_write_seq(ctx, MCS_LEVEL2_KEY, seq,
							ARRAY_SIZE(seq));
}

static inline int s6e63j0x03_apply_mtp_key(struct s6e63j0x03 *ctx, bool on)
{
	unsigned char seq1[3] = { 0x5a, 0x5a };
	unsigned char seq2[3] = { 0xa5, 0xa5 };

	return s6e63j0x03_dcs_write_seq(ctx,  MCS_MTP_KEY, on ? seq1 : seq2,
					ARRAY_SIZE(seq1));
}

static int s6e63j0x03_read_mtp_id(struct s6e63j0x03 *ctx)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char cmds[3] = { READ_ID1, READ_ID2, READ_ID3 };
	unsigned char id[3];
	int ret;
	int i;

	for (i = 0; i < 3; i++) {
		ret = mipi_dsi_dcs_read(dsi, cmds[i], &id[i], 1);

		if (ret < 0)
			return ret;
	}

	dev_info(ctx->dev, "ID: 0x%02x, 0x%02x, 0x%02x\n", id[0], id[1], id[2]);

	return 0;
}

static int s6e63j0x03_power_on(struct s6e63j0x03 *ctx)
{
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	msleep(ctx->power_on_delay);

	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1000, 2000);
	gpiod_set_value(ctx->reset_gpio, 1);

	usleep_range(ctx->reset_delay * 1000, (ctx->reset_delay + 1) * 1000);

	return 0;
}

static int s6e63j0x03_power_off(struct s6e63j0x03 *ctx)
{
	int ret;

	gpiod_set_value(ctx->reset_gpio, 0);

	ret = regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0)
		return ret;

	return 0;
}

static int s6e63j0x03_get_brightness(struct backlight_device *bl_dev)
{
	return bl_dev->props.brightness;
}

static unsigned int s6e63j0x03_get_brightness_index(unsigned int brightness)
{
	unsigned int index;

	index = brightness / (MAX_BRIGHTNESS / NUM_GAMMA_STEPS);

	if (index >= NUM_GAMMA_STEPS)
		index = NUM_GAMMA_STEPS - 1;

	return index;
}

static int s6e63j0x03_update_gamma(struct s6e63j0x03 *ctx,
					unsigned int brightness)
{
	struct backlight_device *bl_dev = ctx->bl_dev;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned int index = s6e63j0x03_get_brightness_index(brightness);
	int ret;

	ret = s6e63j0x03_apply_mtp_key(ctx, true);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_dcs_write(dsi, MCS_MTP_SET3, gamma_tbl[index],
							GAMMA_CMD_CNT);
	if (ret < 0)
		return ret;

	ret = s6e63j0x03_apply_mtp_key(ctx, false);
	if (ret < 0)
		return ret;

	bl_dev->props.brightness = brightness;

	return 0;
}

static int s6e63j0x03_set_brightness(struct backlight_device *bl_dev)
{
	struct s6e63j0x03 *ctx = (struct s6e63j0x03 *)bl_get_data(bl_dev);
	unsigned int brightness = bl_dev->props.brightness;
	int ret;

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bl_dev->props.max_brightness) {
		dev_err(ctx->dev, "Invalid brightness: %u\n", brightness);
		return -EINVAL;
	}

	if (bl_dev->props.power > FB_BLANK_NORMAL) {
		dev_err(ctx->dev,
			"panel must be at least in fb blank normal state\n");
		return -EPERM;
	}

	ret = s6e63j0x03_update_gamma(ctx, brightness);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct backlight_ops s6e63j0x03_bl_ops = {
	.get_brightness = s6e63j0x03_get_brightness,
	.update_status = s6e63j0x03_set_brightness,
};

static int s6e63j0x03_disable(struct drm_panel *panel)
{
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);
	struct backlight_device *bl_dev = ctx->bl_dev;
	int ret;

	ret = s6e63j0x03_dcs_write_seq(ctx, MIPI_DCS_SET_DISPLAY_OFF, NULL, 0);
	if (ret > 0)
		bl_dev->props.power = FB_BLANK_NORMAL;

	return 0;
}

static int s6e63j0x03_unprepare(struct drm_panel *panel)
{
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);
	struct backlight_device *bl_dev = ctx->bl_dev;
	int ret;

	ret = s6e63j0x03_dcs_write_seq(ctx, MIPI_DCS_ENTER_SLEEP_MODE, NULL, 0);
	if (ret < 0)
		return ret;

	msleep(ctx->power_off_delay);

	ret = s6e63j0x03_power_off(ctx);
	if (ret < 0)
		return ret;

	bl_dev->props.power = FB_BLANK_POWERDOWN;

	return 0;
}

static int s6e63j0x03_prepare(struct drm_panel *panel)
{
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);
	struct backlight_device *bl_dev = ctx->bl_dev;
	int ret;
	int i;

	ret = s6e63j0x03_power_on(ctx);
	if (ret < 0)
		return ret;

	ret = s6e63j0x03_enable_lv2_command(ctx);
	if (ret < 0)
		return ret;

	ret = s6e63j0x03_apply_mtp_key(ctx, true);
	if (ret < 0)
		return ret;

	ret = s6e63j0x03_read_mtp_id(ctx);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(prepare_cmds1); i++) {
		ret = s6e63j0x03_dcs_write_seq(ctx, prepare_cmds1[i][1],
				&prepare_cmds1[i][2], prepare_cmds1[i][0] - 1);
		if (ret < 0)
			return ret;
	}

	msleep(120);

	for (i = 0; i < ARRAY_SIZE(prepare_cmds2); i++) {
		ret = s6e63j0x03_dcs_write_seq(ctx, prepare_cmds2[i][1],
				&prepare_cmds2[i][2], prepare_cmds2[i][0] - 1);
		if (ret < 0)
			return ret;
	}

	ret = s6e63j0x03_apply_mtp_key(ctx, false);
	if (ret < 0)
		return ret;

	bl_dev->props.power = FB_BLANK_NORMAL;

	return 0;
}

static int s6e63j0x03_enable(struct drm_panel *panel)
{
	struct s6e63j0x03 *ctx = panel_to_s6e63j0x03(panel);
	struct backlight_device *bl_dev = ctx->bl_dev;
	int ret;

	ret = s6e63j0x03_dcs_write_seq(ctx, MIPI_DCS_SET_DISPLAY_ON, NULL, 0);
	if (ret > 0)
		bl_dev->props.power = FB_BLANK_UNBLANK;

	return 0;
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
	mode->vrefresh = 30;
	mode->width_mm = ctx->width_mm;
	mode->height_mm = ctx->height_mm;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs s6e63j0x03_funcs = {
	.disable = s6e63j0x03_disable,
	.unprepare = s6e63j0x03_unprepare,
	.prepare = s6e63j0x03_prepare,
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

	return ret;
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

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpio: %ld\n",
				PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &s6e63j0x03_funcs;

	ctx->bl_dev = backlight_device_register("s6e63j0x03", dev, ctx,
						&s6e63j0x03_bl_ops, NULL);
	if (IS_ERR(ctx->bl_dev)) {
		dev_err(dev, "failed to register backlight device\n");
		return PTR_ERR(ctx->bl_dev);
	}

	ctx->bl_dev->props.max_brightness = MAX_BRIGHTNESS;
	ctx->bl_dev->props.brightness = DEFAULT_BRIGHTNESS;
	ctx->bl_dev->props.power = FB_BLANK_POWERDOWN;


	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		goto unregister_backlight;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		goto remove_panel;

	return ret;

remove_panel:
	drm_panel_remove(&ctx->panel);

unregister_backlight:
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

static const struct of_device_id s6e63j0x03_of_match[] = {
	{ .compatible = "samsung,s6e63j0x03" },
	{ }
};
MODULE_DEVICE_TABLE(of, s6e63j0x03_of_match);

static struct mipi_dsi_driver s6e63j0x03_driver = {
	.probe = s6e63j0x03_probe,
	.remove = s6e63j0x03_remove,
	.driver = {
		.name = "panel_s6e63j0x03",
		.of_match_table = s6e63j0x03_of_match,
	},
};
module_mipi_dsi_driver(s6e63j0x03_driver);

MODULE_AUTHOR("Inki Dae <inki.dae@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based s6e8aa0 AMOLED LCD Panel Driver");
MODULE_LICENSE("GPL v2");
