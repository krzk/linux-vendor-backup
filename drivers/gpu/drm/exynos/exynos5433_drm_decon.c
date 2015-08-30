/* drivers/gpu/drm/exynos5433_drm_decon.c
 *
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 * Authors:
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Hyungwon Hwang <human.hwang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundationr
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <video/exynos5433_decon.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_crtc.h"
#include "exynos_drm_plane.h"
#include "exynos_drm_iommu.h"

#define WINDOWS_NR	3
#define MIN_FB_WIDTH_FOR_16WORD_BURST	128

static const char * const decon_clks_name[] = {
	"pclk",
	"aclk_decon",
	"aclk_smmu_decon0x",
	"aclk_xiu_decon0x",
	"pclk_smmu_decon0x",
	"sclk_decon_vclk",
	"sclk_decon_eclk",
};

static const uint32_t decon_formats[] = {
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
};

struct exynos5433_decon_driver_data {
	enum exynos_drm_output_type type;
	enum exynos_drm_trigger_type trg_type;
	unsigned int nr_window;
	unsigned int first_win;
	unsigned int lcdblk_offset;
	unsigned int lcdblk_te_unmask_shift;
};

struct decon_context {
	struct device			*dev;
	struct drm_device		*drm_dev;
	struct exynos_drm_crtc          *crtc;
	struct exynos_drm_plane		planes[WINDOWS_NR];
	void __iomem			*addr;
	struct regmap			*sysreg;
	struct clk			*clks[ARRAY_SIZE(decon_clks_name)];
	int				pipe;
	bool				suspended;

#define BIT_CLKS_ENABLED		0
#define BIT_IRQS_ENABLED		1
	unsigned long			enabled;
	bool				i80_if;
	wait_queue_head_t		wait_vsync_queue;
	atomic_t			wait_vsync_event;
	atomic_t			win_updated;
	struct exynos5433_decon_driver_data *drv_data;
};

static const struct of_device_id exynos5433_decon_driver_dt_match[];

static inline void decon_set_bits(struct decon_context *ctx, u32 reg, u32 mask,
				  u32 val)
{
	val &= mask;
	val |= readl(ctx->addr + reg) & ~mask;
	writel(val, ctx->addr + reg);
}

static void decon_wait_for_vblank(struct exynos_drm_crtc *crtc)
{
	struct decon_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	atomic_set(&ctx->wait_vsync_event, 1);

	/*
	 * wait for FIMD to signal VSYNC interrupt or return after
	 * timeout which is set to 50ms (refresh rate of 20).
	 */
	if (!wait_event_timeout(ctx->wait_vsync_queue,
				!atomic_read(&ctx->wait_vsync_event),
				HZ/20))
		DRM_DEBUG_KMS("vblank wait timed out.\n");
}

static int decon_enable_vblank(struct exynos_drm_crtc *crtc)
{
	struct decon_context *ctx = crtc->ctx;
	u32 val;

	if (ctx->suspended)
		return -EPERM;

	if (!test_and_set_bit(BIT_IRQS_ENABLED, &ctx->enabled)) {
		val = VIDINTCON0_INTEN;
		if (ctx->i80_if)
			val |= VIDINTCON0_FRAMEDONE;
		else
			val |= VIDINTCON0_INTFRMEN;

		writel(val, ctx->addr + DECON_VIDINTCON0);
	}

	return 0;
}

static void decon_disable_vblank(struct exynos_drm_crtc *crtc)
{
	struct decon_context *ctx = crtc->ctx;

	if (ctx->suspended)
		return;

	if (test_and_clear_bit(BIT_IRQS_ENABLED, &ctx->enabled))
		writel(0, ctx->addr + DECON_VIDINTCON0);
}

static void decon_setup_trigger(struct decon_context *ctx)
{
	enum exynos_drm_trigger_type trg_type = ctx->drv_data->trg_type;
	u32 val;

	if (!ctx->i80_if && trg_type != EXYNOS_DISPLAY_HW_TRIGGER)
		return;

	if (trg_type == EXYNOS_DISPLAY_HW_TRIGGER)
		val = TRIGCON_TRIGEN_PER_F | TRIGCON_TRIGEN_F | TRIGCON_TE_AUTO_MASK
			| TRIGCON_HWTRIGMASK_I80_RGB | TRIGCON_HWTRIGEN_I80_RGB;
	else
		val = TRIGCON_TRIGEN_PER_F | TRIGCON_TRIGEN_F
			| TRIGCON_TE_AUTO_MASK | TRIGCON_SWTRIGEN;

	writel(val, ctx->addr + DECON_TRIGCON);
}

static void decon_update(struct decon_context *ctx)
{
	decon_set_bits(ctx, DECON_UPDATE, STANDALONE_UPDATE_F, ~0);
}

static void decon_wait_for_update(struct decon_context *ctx)
{
	/* wait up to duration of 2 frames */
	unsigned long timeout = jiffies + msecs_to_jiffies(2 * 1000 / 60);

	while (readl(ctx->addr + DECON_UPDATE) & STANDALONE_UPDATE_F) {
		if (time_is_after_jiffies(timeout))
			break;
		usleep_range(500, 1000);
	}
}

static void decon_commit(struct exynos_drm_crtc *crtc)
{
	struct decon_context *ctx = crtc->ctx;
	struct drm_display_mode *mode = &crtc->base.mode;
	bool interlaced = false;
	u32 val;

	decon_wait_for_update(ctx);

	/* enable clock gate */
	val = CMU_CLKGAGE_MODE_SFR_F | CMU_CLKGAGE_MODE_MEM_F;
	writel(val, ctx->addr + DECON_CMU);

	if (ctx->drv_data->type == EXYNOS_DISPLAY_TYPE_HDMI) {
		mode->crtc_hsync_start = mode->crtc_hdisplay + 10;
		mode->crtc_hsync_end = mode->crtc_htotal - 92;
		mode->crtc_vsync_start = mode->crtc_vdisplay + 1;
		mode->crtc_vsync_end = mode->crtc_vsync_start + 1;
		if (mode->flags & DRM_MODE_FLAG_INTERLACE)
			interlaced = true;
	}

	decon_setup_trigger(ctx);

	val = VIDOUT_LCD_ON;
	if (interlaced)
		val |= VIDOUT_INTERLACE_EN_F;

	if (ctx->i80_if)
		val |= VIDOUT_COMMAND_IF;
	else
		val |= VIDOUT_RGB_IF;
	writel(val, ctx->addr + DECON_VIDOUTCON0);

	if (interlaced)
		val = VIDTCON2_LINEVAL(mode->vdisplay / 2 - 1) |
			VIDTCON2_HOZVAL(mode->hdisplay - 1);
	else
		val = VIDTCON2_LINEVAL(mode->vdisplay - 1) |
			VIDTCON2_HOZVAL(mode->hdisplay - 1);
	writel(val, ctx->addr + DECON_VIDTCON2);

	if (!ctx->i80_if) {
		val = VIDTCON00_VBPD_F(
				mode->crtc_vtotal - mode->crtc_vsync_end - 1) |
			VIDTCON00_VFPD_F(
				mode->crtc_vsync_start - mode->crtc_vdisplay - 1);
		writel(val, ctx->addr + DECON_VIDTCON00);

		val = VIDTCON01_VSPW_F(
				mode->crtc_vsync_end - mode->crtc_vsync_start - 1);
		writel(val, ctx->addr + DECON_VIDTCON01);

		val = VIDTCON10_HBPD_F(
				mode->crtc_htotal - mode->crtc_hsync_end - 1) |
			VIDTCON10_HFPD_F(
				mode->crtc_hsync_start - mode->crtc_hdisplay - 1);
		writel(val, ctx->addr + DECON_VIDTCON10);

		val = VIDTCON11_HSPW_F(
				mode->crtc_hsync_end - mode->crtc_hsync_start - 1);
		writel(val, ctx->addr + DECON_VIDTCON11);
	} else {
		struct exynos5433_decon_driver_data *drv_data =
							ctx->drv_data;
		unsigned int te_unmask = drv_data->trg_type;

		/* set te unmask */
		if (ctx->sysreg && regmap_update_bits(ctx->sysreg,
		    drv_data->lcdblk_offset,
		    0x1 << drv_data->lcdblk_te_unmask_shift,
		    te_unmask << drv_data->lcdblk_te_unmask_shift)) {
			DRM_ERROR("Failed to update sysreg.\n");
			return;
		}
	}
	/* enable output and display signal */
	decon_set_bits(ctx, DECON_VIDCON0, VIDCON0_ENVID | VIDCON0_ENVID_F, ~0);
	decon_update(ctx);
}

#define BIT_VAL(x, e, s)	(((x) & ((1 << ((e) - (s) + 1)) - 1)) << (s))
#define COORDINATE_X(x)		BIT_VAL((x), 23, 12)
#define COORDINATE_Y(x)		BIT_VAL((x), 11, 0)

static void decon_win_set_pixfmt(struct decon_context *ctx, unsigned int win)
{
	struct exynos_drm_plane *plane = &ctx->planes[win];
	unsigned long val;
	int padding;

	val = readl(ctx->addr + DECON_WINCONx(win));
	val &= ~WINCONx_BPPMODE_MASK;

	switch (plane->pixel_format) {
	case DRM_FORMAT_XRGB1555:
		val |= WINCONx_BPPMODE_16BPP_I1555;
		val |= WINCONx_HAWSWP_F;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	case DRM_FORMAT_RGB565:
		val |= WINCONx_BPPMODE_16BPP_565;
		val |= WINCONx_HAWSWP_F;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	case DRM_FORMAT_XRGB8888:
		val |= WINCONx_BPPMODE_24BPP_888;
		val |= WINCONx_WSWP_F;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	case DRM_FORMAT_ARGB8888:
		val |= WINCONx_BPPMODE_32BPP_A8888;
		val |= WINCONx_WSWP_F | WINCONx_BLD_PIX_F | WINCONx_ALPHA_SEL_F;
		val |= WINCONx_BURSTLEN_16WORD;
		break;
	default:
		DRM_ERROR("Proper pixel format is not set\n");
		return;
	}

	DRM_DEBUG_KMS("bpp = %u\n", plane->bpp);

	/*
	 * In case of exynos, setting dma-burst to 16Word causes permanent
	 * tearing for very small buffers, e.g. cursor buffer. Burst Mode
	 * switching which is based on plane size is not recommended as
	 * plane size varies a lot towards the end of the screen and rapid
	 * movement causes unstable DMA which results into iommu crash/tear.
	 */

	padding = (plane->pitch / (plane->bpp >> 3)) - plane->fb_width;
	if (plane->fb_width + padding < MIN_FB_WIDTH_FOR_16WORD_BURST) {
		val &= ~WINCONx_BURSTLEN_MASK;
		val |= WINCONx_BURSTLEN_8WORD;
	}

	writel(val, ctx->addr+ DECON_WINCONx(win));
}

static void decon_shadow_protect_win(struct decon_context *ctx, unsigned int win,
					bool protect)
{
	u32 val;

	val = readl(ctx->addr + DECON_SHADOWCON);

	if (protect)
		val |= SHADOWCON_Wx_PROTECT(win);
	else
		val &= ~SHADOWCON_Wx_PROTECT(win);

	writel(val, ctx->addr + DECON_SHADOWCON);
}

static void decon_win_commit(struct exynos_drm_crtc *crtc, unsigned int win)
{
	struct decon_context *ctx = crtc->ctx;
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct exynos_drm_plane *plane;
	u32 val;

	if (win < drv_data->first_win || win >= WINDOWS_NR)
		return;

	plane = &ctx->planes[win];

	/* If suspended, enable this on resume */
	if (ctx->suspended) {
		plane->resume = true;
		return;
	}

	decon_wait_for_update(ctx);

	decon_shadow_protect_win(ctx, win, true);

	/* FIXME: padding? padding = (plane->pitch / (plane->bpp >> 3)) - plane->fb_width */

	if (crtc->base.mode.flags & DRM_MODE_FLAG_INTERLACE) {
		val = COORDINATE_X(plane->crtc_x) | COORDINATE_Y(plane->crtc_y / 2);
		writel(val, ctx->addr + DECON_VIDOSDxA(win));

		val = COORDINATE_X(plane->crtc_x + plane->crtc_width - 1) |
			COORDINATE_Y((plane->crtc_y + plane->crtc_height) / 2 - 1);
		writel(val, ctx->addr + DECON_VIDOSDxB(win));
	} else {
		val = COORDINATE_X(plane->crtc_x) | COORDINATE_Y(plane->crtc_y);
		writel(val, ctx->addr + DECON_VIDOSDxA(win));

		val = COORDINATE_X(plane->crtc_x + plane->crtc_width - 1) |
			COORDINATE_Y(plane->crtc_y + plane->crtc_height - 1);
		writel(val, ctx->addr + DECON_VIDOSDxB(win));
	}

	val = VIDOSD_Wx_ALPHA_R_F(0x0) | VIDOSD_Wx_ALPHA_G_F(0x0) |
		VIDOSD_Wx_ALPHA_B_F(0x0);
	writel(val, ctx->addr + DECON_VIDOSDxC(win));

	val = VIDOSD_Wx_ALPHA_R_F(0x0) | VIDOSD_Wx_ALPHA_G_F(0x0) |
		VIDOSD_Wx_ALPHA_B_F(0x0);
	writel(val, ctx->addr + DECON_VIDOSDxD(win));

	val = plane->dma_addr[0] + plane->src_x * (plane->bpp >> 3) +
	      plane->pitch * plane->src_y;
	writel(val, ctx->addr + DECON_VIDW0xADD0B0(win));

	val += plane->pitch * plane->crtc_height;
	writel(val, ctx->addr + DECON_VIDW0xADD1B0(win));

	if (ctx->drv_data->type == EXYNOS_DISPLAY_TYPE_HDMI)
		val = BIT_VAL(plane->pitch -
			plane->crtc_width * (plane->bpp >> 3), 29, 15)
			| BIT_VAL(plane->crtc_width * (plane->bpp >> 3), 14, 0);
	else
		val = BIT_VAL(plane->pitch -
			plane->crtc_width * (plane->bpp >> 3), 27, 14)
			| BIT_VAL(plane->crtc_width * (plane->bpp >> 3), 13, 0);
	writel(val, ctx->addr + DECON_VIDW0xADD2(win));

	decon_win_set_pixfmt(ctx, win);

	/* window enable */
	val = readl(ctx->addr + DECON_WINCONx(win));
	val |= WINCONx_ENWIN_F;
	writel(val, ctx->addr + DECON_WINCONx(win));

	decon_shadow_protect_win(ctx, win, false);

	decon_update(ctx);

	if (ctx->i80_if && drv_data->trg_type == EXYNOS_DISPLAY_SW_TRIGGER)
		atomic_set(&ctx->win_updated, 1);

	plane->enabled = true;
}

static void decon_win_disable(struct exynos_drm_crtc *crtc, unsigned int win)
{
	struct decon_context *ctx = crtc->ctx;
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct exynos_drm_plane *plane;
	u32 val;

	if (win < drv_data->first_win || win >= WINDOWS_NR)
		return;

	plane = &ctx->planes[win];

	if (ctx->suspended) {
		plane->resume = false;
		return;
	}

	decon_wait_for_update(ctx);

	decon_shadow_protect_win(ctx, win, true);

	/* window disable */
	val = readl(ctx->addr + DECON_WINCONx(win));
	val &= ~WINCONx_ENWIN_F;
	writel(val, ctx->addr + DECON_WINCONx(win));

	decon_shadow_protect_win(ctx, win, false);

	decon_update(ctx);

	plane->enabled = false;
}

static void decon_window_suspend(struct decon_context *ctx)
{
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct exynos_drm_plane *plane;
	int i;

	for (i = drv_data->first_win; i < WINDOWS_NR; i++) {
		plane = &ctx->planes[i];
		plane->resume = plane->enabled;
		if (plane->enabled)
			decon_win_disable(ctx->crtc, i);
	}
}

static void decon_window_resume(struct decon_context *ctx)
{
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct exynos_drm_plane *plane;
	int i;

	for (i = drv_data->first_win; i < WINDOWS_NR; i++) {
		plane = &ctx->planes[i];
		plane->enabled = plane->resume;
		plane->resume = false;
	}
}

static void decon_apply(struct decon_context *ctx)
{
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct exynos_drm_plane *plane;
	int i;

	for (i = drv_data->first_win; i < WINDOWS_NR; i++) {
		plane = &ctx->planes[i];
		if (plane->enabled)
			decon_win_commit(ctx->crtc, i);
		else
			decon_win_disable(ctx->crtc, i);
	}

	decon_commit(ctx->crtc);
}

static void decon_reset(struct decon_context *ctx)
{
	writel(VIDCON0_SWRESET, ctx->addr + DECON_VIDCON0);

	decon_set_bits(ctx, DECON_VIDCON0, VIDCON0_CLKVALUP | VIDCON0_VLCKFREE,
		       ~0);
	decon_set_bits(ctx, DECON_CMU, CMU_CLKGATE_MASK,
		       CMU_CLKGAGE_MODE_SFR_F | CMU_CLKGAGE_MODE_MEM_F);
	decon_set_bits(ctx, DECON_BLENDCON, BLENDCON_NEW_8BIT_ALPHA_VALUE, ~0);
	decon_set_bits(ctx, DECON_VIDOUTCON0, VIDOUT_LCD_ON, ~0);
	decon_set_bits(ctx, DECON_VIDCON1, VIDCON1_VCLK_RUN_VDEN_DISABLE,
							VIDCON1_VCLK_MASK);
	decon_set_bits(ctx, DECON_CRCCTRL, CRCCTRL_MASK,
		       CRCCTRL_CRCEN | CRCCTRL_CRCSTART_F | CRCCTRL_CRCCLKEN);
}

static void decon_dpms_on(struct decon_context *ctx)
{
	int ret;
	int i;

	if (!ctx->suspended)
		return;

	pm_runtime_get_sync(ctx->dev);
	ctx->suspended = false;

	for (i = 0; i < ARRAY_SIZE(decon_clks_name); i++) {
		ret = clk_prepare_enable(ctx->clks[i]);
		if (ret < 0)
			goto err;
	}

	set_bit(BIT_CLKS_ENABLED, &ctx->enabled);

	decon_reset(ctx);

	if (ctx->drv_data->type == EXYNOS_DISPLAY_TYPE_HDMI)
		exynos_hdmiphy_enable(ctx->crtc);

	decon_window_resume(ctx);
	decon_apply(ctx);

	return;
err:
	while (--i >= 0)
		clk_disable_unprepare(ctx->clks[i]);

	ctx->suspended = true;
}

static void decon_swreset(struct decon_context *ctx)
{
	unsigned int tries;

	writel(0, ctx->addr + DECON_VIDCON0);
	for (tries = 2000; tries; --tries) {
		if (~readl(ctx->addr + DECON_VIDCON0) & VIDCON0_STOP_STATUS)
			break;
		udelay(10);
	}

	WARN(tries == 0, "failed to disable DECON\n");

	writel(VIDCON0_SWRESET, ctx->addr + DECON_VIDCON0);
	for (tries = 2000; tries; --tries) {
		if (~readl(ctx->addr + DECON_VIDCON0) & VIDCON0_SWRESET)
			break;
		udelay(10);
	}

	WARN(tries == 0, "failed to software reset DECON\n");
}

static void decon_dpms_off(struct decon_context *ctx)
{
	int i;

	if (ctx->suspended)
		return;

	clear_bit(BIT_CLKS_ENABLED, &ctx->enabled);
	decon_window_suspend(ctx);
	decon_swreset(ctx);

	for (i = ARRAY_SIZE(decon_clks_name) - 1; i >= 0; i--)
		clk_disable_unprepare(ctx->clks[i]);

	pm_runtime_put_sync(ctx->dev);
	ctx->suspended = true;
}

static void decon_dpms(struct exynos_drm_crtc *crtc, int mode)
{
	struct decon_context *ctx = crtc->ctx;

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		decon_dpms_on(ctx);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		decon_dpms_off(ctx);
		break;
	default:
		DRM_DEBUG_KMS("unspecified mode %d\n", mode);
		break;
	}
}

void decon_te_irq_handler(struct exynos_drm_crtc *crtc)
{
	struct decon_context *ctx = crtc->ctx;
	enum exynos_drm_trigger_type trg_type = ctx->drv_data->trg_type;

	if (!test_bit(BIT_CLKS_ENABLED, &ctx->enabled) ||
	    (trg_type == EXYNOS_DISPLAY_HW_TRIGGER))
		return;

	if (atomic_add_unless(&ctx->win_updated, -1, 0))
		decon_set_bits(ctx, DECON_TRIGCON, TRIGCON_SWTRIGCMD, ~0);
}

static struct exynos_drm_crtc_ops decon_crtc_ops = {
	.dpms			= decon_dpms,
	.enable_vblank		= decon_enable_vblank,
	.disable_vblank		= decon_disable_vblank,
	.wait_for_vblank	= decon_wait_for_vblank,
	.commit			= decon_commit,
	.win_commit		= decon_win_commit,
	.win_disable		= decon_win_disable,
	.te_handler		= decon_te_irq_handler,
};

static void decon_clear_channel(struct decon_context *ctx)
{
	int win, i, ret;
	u32 val;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	for (i = 0; i < ARRAY_SIZE(decon_clks_name); i++) {
		ret = clk_prepare_enable(ctx->clks[i]);
		if (ret < 0)
			goto err;
	}

	decon_wait_for_update(ctx);

	for (win = 0; win < WINDOWS_NR; win++) {
		/* shadow update disable */
		val = readl(ctx->addr + DECON_SHADOWCON);
		val |= SHADOWCON_Wx_PROTECT(win);
		writel(val, ctx->addr + DECON_SHADOWCON);

		/* window disable */
		val = readl(ctx->addr + DECON_WINCONx(win));
		val &= ~WINCONx_ENWIN_F;
		writel(val, ctx->addr + DECON_WINCONx(win));

		/* shadow update enable */
		val = readl(ctx->addr + DECON_SHADOWCON);
		val &= ~SHADOWCON_Wx_PROTECT(win);
		writel(val, ctx->addr + DECON_SHADOWCON);
	}

	decon_update(ctx);

	atomic_set(&ctx->wait_vsync_event, 1);

	/*
	 * wait for FIMD to signal VSYNC interrupt or return after
	 * timeout which is set to 50ms (refresh rate of 20).
	 */
	if (!wait_event_timeout(ctx->wait_vsync_queue,
				!atomic_read(&ctx->wait_vsync_event),
				HZ/20))
		DRM_DEBUG_KMS("vblank wait timed out.\n");

err:
	while (--i >= 0)
		clk_disable_unprepare(ctx->clks[i]);
}

static int decon_bind(struct device *dev, struct device *master, void *data)
{
	struct decon_context *ctx = dev_get_drvdata(dev);
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct drm_device *drm_dev = data;
	struct exynos_drm_private *priv = drm_dev->dev_private;
	struct exynos_drm_plane *exynos_plane;
	enum drm_plane_type type;
	unsigned int zpos;
	int ret;

	ctx->drm_dev = drm_dev;
	ctx->pipe = priv->pipe++;

	/* attach this sub driver to iommu mapping if supported. */
	if (is_drm_iommu_supported(drm_dev)) {
		/*
		 * If any channel is already active, iommu will throw
		 * a PAGE FAULT when enabled. So clear any channel if enabled.
		 */
		decon_clear_channel(ctx);
		drm_iommu_attach_device(drm_dev, ctx->dev);
	}

	for (zpos = 0; zpos < WINDOWS_NR; zpos++) {
		type = (zpos == drv_data->first_win) ? DRM_PLANE_TYPE_PRIMARY :
			DRM_PLANE_TYPE_OVERLAY;
		ret = exynos_plane_init(drm_dev, &ctx->planes[zpos],
				1 << ctx->pipe, type, decon_formats,
				ARRAY_SIZE(decon_formats), zpos);
		if (ret)
			return ret;
	}

	exynos_plane = &ctx->planes[drv_data->first_win];
	ctx->crtc = exynos_drm_crtc_create(drm_dev, &exynos_plane->base,
			ctx->pipe, ctx->drv_data->type, &decon_crtc_ops, ctx);
	if (IS_ERR(ctx->crtc)) {
		priv->pipe--;
		return PTR_ERR(ctx->crtc);
	}

	return 0;
}

static void decon_unbind(struct device *dev, struct device *master, void *data)
{
	struct decon_context *ctx = dev_get_drvdata(dev);

	decon_dpms(ctx->crtc, DRM_MODE_DPMS_OFF);

	/* detach this sub driver from iommu mapping if supported. */
	if (is_drm_iommu_supported(ctx->drm_dev))
		drm_iommu_detach_device(ctx->drm_dev, ctx->dev);
}

static const struct component_ops decon_component_ops = {
	.bind	= decon_bind,
	.unbind = decon_unbind,
};

static irqreturn_t decon_irq_handler(int irq, void *dev_id)
{
	struct decon_context *ctx = dev_id;
	u32 val;

	if (!test_bit(BIT_CLKS_ENABLED, &ctx->enabled))
		goto out;

	val = readl(ctx->addr + DECON_VIDINTCON1);
	val &= ctx->i80_if ? VIDINTCON1_INTFRMDONEPEND : VIDINTCON1_INTFRMPEND;
	if (val) {
		writel(val, ctx->addr + DECON_VIDINTCON1);
		drm_handle_vblank(ctx->drm_dev, ctx->pipe);
		exynos_drm_crtc_finish_pageflip(ctx->drm_dev, ctx->pipe);
	}

out:
	if (atomic_read(&ctx->wait_vsync_event)) {
		atomic_set(&ctx->wait_vsync_event, 0);
		wake_up(&ctx->wait_vsync_queue);
	}

	return IRQ_HANDLED;
}

static inline struct exynos5433_decon_driver_data *get_driver_data(
		struct platform_device *pdev)
{
	const struct of_device_id *of_id =
		of_match_device(exynos5433_decon_driver_dt_match, &pdev->dev);

	return (struct exynos5433_decon_driver_data *)of_id->data;
}

static int exynos5433_decon_probe(struct platform_device *pdev)
{
	struct exynos5433_decon_driver_data *drv_data;
	struct device *dev = &pdev->dev;
	struct decon_context *ctx;
	struct resource *res;
	int ret;
	int i;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->suspended = true;
	ctx->dev = dev;

	drv_data = get_driver_data(pdev);
	ctx->drv_data = drv_data;

	if (of_get_child_by_name(dev->of_node, "i80-if-timings"))
		ctx->i80_if = true;

	ctx->sysreg = syscon_regmap_lookup_by_phandle(dev->of_node,
							"samsung,disp-sysreg");
	if (IS_ERR(ctx->sysreg)) {
		dev_warn(dev, "failed to get system register.\n");
		ctx->sysreg = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(decon_clks_name); i++) {
		struct clk *clk;

		clk = devm_clk_get(ctx->dev, decon_clks_name[i]);
		if (IS_ERR(clk))
			return PTR_ERR(clk);

		ctx->clks[i] = clk;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "cannot find IO resource\n");
		return -ENXIO;
	}

	ctx->addr = devm_ioremap_resource(dev, res);
	if (IS_ERR(ctx->addr)) {
		dev_err(dev, "ioremap failed\n");
		return PTR_ERR(ctx->addr);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
			ctx->i80_if ? "lcd_sys" : "vsync");
	if (!res) {
		dev_err(dev, "cannot find IRQ resource\n");
		return -ENXIO;
	}

	ret = devm_request_irq(dev, res->start, decon_irq_handler, 0,
			"drm_decon", ctx);
	if (ret < 0) {
		dev_err(dev, "lcd_sys irq request failed\n");
		return ret;
	}

	init_waitqueue_head(&ctx->wait_vsync_queue);
	atomic_set(&ctx->wait_vsync_event, 0);

	platform_set_drvdata(pdev, ctx);

	pm_runtime_enable(dev);

	ret = component_add(dev, &decon_component_ops);
	if (ret)
		goto err_disable_pm_runtime;

	return 0;

err_disable_pm_runtime:
	pm_runtime_disable(dev);

	return ret;
}

static int exynos5433_decon_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &decon_component_ops);

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct exynos5433_decon_driver_data exynos5433_decon_int_driver_data = {
	.type = EXYNOS_DISPLAY_TYPE_LCD,
	.trg_type = EXYNOS_DISPLAY_HW_TRIGGER,
	.first_win = 0,
	.lcdblk_offset = 0x1004,
	.lcdblk_te_unmask_shift = 13,
};

static const struct exynos5433_decon_driver_data exynos5433_decon_ext_driver_data = {
	.type = EXYNOS_DISPLAY_TYPE_HDMI,
	.trg_type = EXYNOS_DISPLAY_HW_TRIGGER,
	.first_win = 1,
	.lcdblk_offset = 0x1004,
	.lcdblk_te_unmask_shift = 13,
};

static const struct of_device_id exynos5433_decon_driver_dt_match[] = {
	{ .compatible = "samsung,exynos5433-decon",
	  .data = &exynos5433_decon_int_driver_data },
	{ .compatible = "samsung,exynos5433-decon-tv",
	  .data = &exynos5433_decon_ext_driver_data },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5433_decon_driver_dt_match);

struct platform_driver exynos5433_decon_driver = {
	.probe		= exynos5433_decon_probe,
	.remove		= exynos5433_decon_remove,
	.driver		= {
		.name	= "exynos5433-decon",
		.owner	= THIS_MODULE,
		.of_match_table = exynos5433_decon_driver_dt_match,
	},
};
