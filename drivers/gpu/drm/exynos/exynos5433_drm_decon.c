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

#include <video/exynos5433_decon.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_crtc.h"
#include "exynos_drm_iommu.h"

#define WINDOWS_NR	3
#define MIN_FB_WIDTH_FOR_16WORD_BURST	128

struct decon_tv_mode {
	u32 xres;
	u32 yres;
	u32 vbp;
	u32 vfp;
	u32 vsa;
	u32 hbp;
	u32 hfp;
	u32 hsa;
	u32 fps;
	u32 interlace_bit;
};

struct decon_win_data {
	unsigned int 	offset_x;
	unsigned int 	offset_y;
	unsigned int	ovl_x;
	unsigned int	ovl_y;
	unsigned int	ovl_width;
	unsigned int	ovl_height;
	unsigned int	fb_width;
	unsigned int	fb_height;
	unsigned int	fb_pitch;
	unsigned int	bpp;
	unsigned int	pixel_format;
	dma_addr_t	dma_addr;
	bool		enabled;
	bool		resume;
};

struct exynos5433_decon_driver_data {
	enum exynos_drm_output_type type;
	unsigned int nr_window;
	unsigned int first_win;
};

struct decon_context {
	struct device			*dev;
	struct drm_device		*drm_dev;
	struct exynos_drm_crtc          *crtc;
	void __iomem			*addr;
	struct clk			*clks[6];
	struct decon_win_data		win_data[WINDOWS_NR];
	int				pipe;
	bool				suspended;

#define BIT_CLKS_ENABLED		0
#define BIT_IRQS_ENABLED		1
	unsigned long			enabled;
	bool				i80_if;
	atomic_t			win_updated;
	struct exynos5433_decon_driver_data *drv_data;
};

static const char * const decon_clks_name[] = {
	"aclk_decon",
	"aclk_smmu_decon0x",
	"aclk_xiu_decon0x",
	"pclk_smmu_decon0x",
	"sclk_decon_vclk",
	"sclk_decon_eclk",
};

static const struct of_device_id exynos5433_decon_driver_dt_match[];

static struct decon_tv_mode decon_tv_modes[] = {
	{ 720, 480, 43, 1, 1, 92, 10, 36, 0 },
	{ 720, 576, 47, 1, 1, 92, 10, 42, 0 },
	{ 1280, 720, 28, 1, 1, 192, 10, 168, 0 },
	{ 1280, 720, 28, 1, 1, 92, 10, 598, 0 },
	{ 1920, 1080, 43, 1, 1, 92, 10, 178, 0 },
	{ 1920, 1080, 43, 1, 1, 92, 10, 618, 0 },
	{ 1920, 1080, 43, 1, 1, 92, 10, 178, 0 },
	{ 1920, 1080, 43, 1, 1, 92, 10, 618, 0 },
	{ 1920, 1080, 43, 1, 1, 92, 10, 728, 0 },
	{ 1920, 540, 20, 1, 1, 92, 10, 178, 1 },
	{ 3840, 2160, 88, 1, 1, 458, 10, 92, 0 },
	{ 3840, 2160, 88, 1, 1, 1338, 10, 92, 0 },
	{ 3840, 2160, 88, 1, 1, 1558, 10, 92, 0 },
	{ 4096, 2160, 88, 1, 1, 1302, 10, 92, 0 },
};

static inline void decon_set_bits(struct decon_context *ctx, u32 reg, u32 mask,
				  u32 val)
{
	val &= mask;
	val |= readl(ctx->addr + reg) & ~mask;
	writel(val, ctx->addr + reg);
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
	u32 val = TRIGCON_TRIGEN_PER_F | TRIGCON_TRIGEN_F |
			TRIGCON_TE_AUTO_MASK | TRIGCON_SWTRIGEN;
	writel(val, ctx->addr + DECON_TRIGCON);
}

static void decon_commit(struct exynos_drm_crtc *crtc)
{
	struct decon_context *ctx = crtc->ctx;
	struct drm_display_mode *mode = &crtc->base.mode;
	struct decon_tv_mode *decon_tv_mode = NULL;
	u32 val;
	int i;

	/* enable clock gate */
	val = CMU_CLKGAGE_MODE_SFR_F | CMU_CLKGAGE_MODE_MEM_F;
	writel(val, ctx->addr + DECON_CMU);

	if (ctx->drv_data->type == EXYNOS_DISPLAY_TYPE_HDMI) {
		for (i = 0; i < ARRAY_SIZE(decon_tv_modes); ++i) {
			decon_tv_mode = &decon_tv_modes[i];
			if (mode->hdisplay != decon_tv_mode->xres ||
				mode->vdisplay != decon_tv_mode->yres)
				continue;

			mode->crtc_hdisplay = decon_tv_mode->xres;
			mode->crtc_hsync_start = mode->crtc_hdisplay +
						decon_tv_mode->hfp;
			mode->crtc_hsync_end = mode->crtc_hsync_start +
						decon_tv_mode->hsa;
			mode->crtc_htotal = mode->crtc_hsync_end +
						decon_tv_mode->hbp;

			mode->crtc_vdisplay = decon_tv_mode->yres;
			mode->crtc_vsync_start = mode->crtc_vdisplay +
						decon_tv_mode->vfp;
			mode->crtc_vsync_end = mode->crtc_vsync_start +
						decon_tv_mode->vsa;
			mode->crtc_vtotal = mode->crtc_vsync_end +
						decon_tv_mode->vbp;
			break;
		}
	}

	/* lcd on and use command if */
	val = VIDOUT_LCD_ON;
	//	if (!decon_tv_mode)
	//		val |= (decon_tv_mode->interlace_bit << 28);
	if (ctx->i80_if)
		val |= VIDOUT_COMMAND_IF;
	else
		val |= VIDOUT_RGB_IF;
	writel(val, ctx->addr + DECON_VIDOUTCON0);

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
	}

	decon_setup_trigger(ctx);

	/* enable output and display signal */
	decon_set_bits(ctx, DECON_VIDCON0, VIDCON0_ENVID | VIDCON0_ENVID_F, ~0);
}

#define COORDINATE_X(x)		(((x) & 0xfff) << 12)
#define COORDINATE_Y(x)		((x) & 0xfff)
#define OFFSIZE(x)		(((x) & 0x3fff) << 14)
#define PAGEWIDTH(x)		((x) & 0x3fff)

static void decon_win_mode_set(struct exynos_drm_crtc *crtc,
			       struct exynos_drm_plane *plane)
{
	struct decon_context *ctx = crtc->ctx;
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct decon_win_data *win_data;
	unsigned int win;

	if (!plane) {
		DRM_ERROR("plane is NULL\n");
		return;
	}

	win = plane->zpos;
	if (win == DEFAULT_ZPOS)
		win = drv_data->first_win;

	if (win < drv_data->first_win || win >= WINDOWS_NR)
		return;

	win_data = &ctx->win_data[win];

	win_data->offset_x = plane->fb_x;
	win_data->offset_y = plane->fb_y;
	win_data->fb_width = plane->fb_width;
	win_data->fb_height = plane->fb_height;
	win_data->fb_pitch = plane->pitch;
	win_data->ovl_x = plane->crtc_x;
	win_data->ovl_y = plane->crtc_y;
	win_data->ovl_width = plane->crtc_width;
	win_data->ovl_height = plane->crtc_height;
	win_data->dma_addr = plane->dma_addr[0];
	win_data->bpp = plane->bpp;
	win_data->pixel_format = plane->pixel_format;
}

static void decon_win_set_pixfmt(struct decon_context *ctx, unsigned int win)
{
	struct decon_win_data *win_data = &ctx->win_data[win];
	unsigned long val;

	val = readl(ctx->addr + DECON_WINCONx(win));
	val &= ~WINCONx_BPPMODE_MASK;

	switch (win_data->pixel_format) {
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

	DRM_DEBUG_KMS("bpp = %u\n", win_data->bpp);

	/*
	 * In case of exynos, setting dma-burst to 16Word causes permanent
	 * tearing for very small buffers, e.g. cursor buffer. Burst Mode
	 * switching which is based on plane size is not recommended as
	 * plane size varies a lot towards the end of the screen and rapid
	 * movement causes unstable DMA which results into iommu crash/tear.
	 */

	if (win_data->fb_width < MIN_FB_WIDTH_FOR_16WORD_BURST) {
		val &= ~WINCONx_BURSTLEN_MASK;
		val |= WINCONx_BURSTLEN_8WORD;
	}

	writel(val, ctx->addr+ DECON_WINCONx(win));
}

static void decon_shadow_protect_win(struct decon_context *ctx, int win,
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

static void decon_win_commit(struct exynos_drm_crtc *crtc, int zpos)
{
	struct decon_context *ctx = crtc->ctx;
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct decon_win_data *win_data;
	unsigned int win = zpos;
	u32 val;

	if (win == DEFAULT_ZPOS)
		win = drv_data->first_win;

	if (win < drv_data->first_win || win >= WINDOWS_NR)
		return;

	win_data = &ctx->win_data[win];

	/* If suspended, enable this on resume */
	if (ctx->suspended) {
		win_data->resume = true;
		return;
	}

	decon_shadow_protect_win(ctx, win, true);

	val = COORDINATE_X(win_data->ovl_x) | COORDINATE_Y(win_data->ovl_y);
	writel(val, ctx->addr + DECON_VIDOSDxA(win));

	val = COORDINATE_X(win_data->ovl_x + win_data->ovl_width - 1) |
		COORDINATE_Y(win_data->ovl_y + win_data->ovl_height - 1);
	writel(val, ctx->addr + DECON_VIDOSDxB(win));

	val = VIDOSD_Wx_ALPHA_R_F(0x0) | VIDOSD_Wx_ALPHA_G_F(0x0) |
		VIDOSD_Wx_ALPHA_B_F(0x0);
	writel(val, ctx->addr + DECON_VIDOSDxC(win));

	val = VIDOSD_Wx_ALPHA_R_F(0x0) | VIDOSD_Wx_ALPHA_G_F(0x0) |
		VIDOSD_Wx_ALPHA_B_F(0x0);
	writel(val, ctx->addr + DECON_VIDOSDxD(win));

	writel(win_data->dma_addr, ctx->addr + DECON_VIDW0xADD0B0(win));

	val = win_data->dma_addr + win_data->fb_pitch * win_data->ovl_height;
	writel(val, ctx->addr + DECON_VIDW0xADD1B0(win));

	val = OFFSIZE(win_data->fb_pitch -
			win_data->ovl_width * (win_data->bpp >> 3))
		| PAGEWIDTH(win_data->ovl_width * (win_data->bpp >> 3));
	writel(val, ctx->addr + DECON_VIDW0xADD2(win));

	decon_win_set_pixfmt(ctx, win);

	/* window enable */
	val = readl(ctx->addr + DECON_WINCONx(win));
	val |= WINCONx_ENWIN_F;
	writel(val, ctx->addr + DECON_WINCONx(win));

	decon_shadow_protect_win(ctx, win, false);

	/* standalone update */
	val = readl(ctx->addr + DECON_UPDATE);
	val |= STANDALONE_UPDATE_F;
	writel(val, ctx->addr + DECON_UPDATE);

	if (ctx->i80_if)
		atomic_set(&ctx->win_updated, 1);

	win_data->enabled = true;
}

static void decon_win_disable(struct exynos_drm_crtc *crtc, int zpos)
{
	struct decon_context *ctx = crtc->ctx;
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct decon_win_data *win_data;
	unsigned int win = zpos;
	u32 val;

	if (win == DEFAULT_ZPOS)
		win = drv_data->first_win;

	if (win < drv_data->first_win || win >= WINDOWS_NR)
		return;

	win_data = &ctx->win_data[win];

	if (ctx->suspended) {
		win_data->resume = false;
		return;
	}

	decon_shadow_protect_win(ctx, win, true);

	/* window disable */
	val = readl(ctx->addr + DECON_WINCONx(win));
	val &= ~WINCONx_ENWIN_F;
	writel(val, ctx->addr + DECON_WINCONx(win));

	decon_shadow_protect_win(ctx, win, false);

	/* standalone update */
	val = readl(ctx->addr + DECON_UPDATE);
	val |= STANDALONE_UPDATE_F;
	writel(val, ctx->addr + DECON_UPDATE);

	win_data->enabled = false;
}

static void decon_window_suspend(struct decon_context *ctx)
{
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct decon_win_data *win_data;
	int i;

	for (i = drv_data->first_win; i < WINDOWS_NR; i++) {
		win_data = &ctx->win_data[i];
		win_data->resume = win_data->enabled;
		if (win_data->enabled)
			decon_win_disable(ctx->crtc, i);
	}
}

static void decon_window_resume(struct decon_context *ctx)
{
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct decon_win_data *win_data;
	int i;

	for (i = drv_data->first_win; i < WINDOWS_NR; i++) {
		win_data = &ctx->win_data[i];
		win_data->enabled = win_data->resume;
		win_data->resume = false;
	}
}

static void decon_apply(struct decon_context *ctx)
{
	struct exynos5433_decon_driver_data *drv_data = ctx->drv_data;
	struct decon_win_data *win_data;
	int i;

	decon_setup_trigger(ctx);

	for (i = 0; i < WINDOWS_NR; i++) {
		win_data = &ctx->win_data[i];
		if (win_data->enabled)
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
	decon_set_bits(ctx, DECON_VIDCON1, VIDCON1_VCLK_RUN, VIDCON1_VCLK_MASK);
	decon_set_bits(ctx, DECON_CRCCTRL, CRCCTRL_MASK,
		       CRCCTRL_CRCEN | CRCCTRL_CRCSTART_F | CRCCTRL_CRCCLKEN);
	decon_set_bits(ctx, DECON_TRIGCON, TRIGCON_MASK,
		       TRIGCON_TRIGEN_PER_I80_RGB_F | TRIGCON_TRIGEN_I80_RGB_F
		       | TRIGCON_HWTRIGMASK_I80_RGB | TRIGCON_HWTRIGEN_I80_RGB);
}

/* this function will be replaced by clk API call */
#ifdef CONFIG_DRM_EXYNOS_HDMI
void exynos_hdmiphy_enable(struct exynos_drm_crtc *crtc);
#else
static void exynos_hdmiphy_enable(struct exynos_drm_crtc *crtc) {}
#endif

static void decon_dpms_on(struct decon_context *ctx)
{
	int ret;
	int i;

	if (!ctx->suspended)
		return;

	ctx->suspended = false;

	for (i = 0; i < ARRAY_SIZE(decon_clks_name); i++) {
		ret = clk_prepare_enable(ctx->clks[i]);
		if (ret < 0)
			goto err;
	}

	set_bit(BIT_CLKS_ENABLED, &ctx->enabled);

	decon_reset(ctx);
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
	u32 val;

	if (!test_bit(BIT_CLKS_ENABLED, &ctx->enabled))
		return;

	if (atomic_add_unless(&ctx->win_updated, -1, 0)) {
		/* trigger */
		val = readl(ctx->addr + DECON_TRIGCON);
		val |= TRIGCON_SWTRIGCMD;
		writel(val, ctx->addr + DECON_TRIGCON);
	}

	drm_handle_vblank(ctx->drm_dev, ctx->pipe);
}

static struct exynos_drm_crtc_ops decon_crtc_ops = {
	.dpms			= decon_dpms,
	.enable_vblank		= decon_enable_vblank,
	.disable_vblank		= decon_disable_vblank,
	.commit			= decon_commit,
	.win_mode_set		= decon_win_mode_set,
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

		/* standalone update */
		val = readl(ctx->addr + DECON_UPDATE);
		val |= STANDALONE_UPDATE_F;
		writel(val, ctx->addr + DECON_UPDATE);
	}
	/* TODO: wait for possible vsync */
	msleep(50);

err:
	while (--i >= 0)
		clk_disable_unprepare(ctx->clks[i]);
}

static int decon_bind(struct device *dev, struct device *master, void *data)
{
	struct decon_context *ctx = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct exynos_drm_private *priv = drm_dev->dev_private;

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

	ctx->crtc = exynos_drm_crtc_create(drm_dev, ctx->pipe,
			ctx->drv_data->type, &decon_crtc_ops, ctx);
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

static irqreturn_t decon_vsync_irq_handler(int irq, void *dev_id)
{
	struct decon_context *ctx = dev_id;
	u32 val;

	if (!test_bit(BIT_CLKS_ENABLED, &ctx->enabled))
		goto out;

	val = readl(ctx->addr + DECON_VIDINTCON1);
	if (val & VIDINTCON1_INTFRMPEND) {
		drm_handle_vblank(ctx->drm_dev, ctx->pipe);

		/* clear */
		writel(VIDINTCON1_INTFRMPEND, ctx->addr + DECON_VIDINTCON1);
	}

out:
	return IRQ_HANDLED;
}

static irqreturn_t decon_lcd_sys_irq_handler(int irq, void *dev_id)
{
	struct decon_context *ctx = dev_id;
	u32 val;

	if (!test_bit(BIT_CLKS_ENABLED, &ctx->enabled))
		goto out;

	val = readl(ctx->addr + DECON_VIDINTCON1);
	if (val & VIDINTCON1_INTFRMDONEPEND) {
		exynos_drm_crtc_finish_pageflip(ctx->drm_dev, ctx->pipe);

		/* clear */
		writel(VIDINTCON1_INTFRMDONEPEND,
				ctx->addr + DECON_VIDINTCON1);
	}

out:
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

	ret = devm_request_irq(dev, res->start, ctx->i80_if ?
			decon_lcd_sys_irq_handler : decon_vsync_irq_handler, 0,
			"drm_decon", ctx);
	if (ret < 0) {
		dev_err(dev, "lcd_sys irq request failed\n");
		return ret;
	}

	ret = exynos_drm_component_add(dev, EXYNOS_DEVICE_TYPE_CRTC,
				       drv_data->type);
	if (ret < 0)
		return ret;

	platform_set_drvdata(pdev, ctx);

	ret = component_add(dev, &decon_component_ops);
	if (ret < 0) {
		exynos_drm_component_del(dev, EXYNOS_DEVICE_TYPE_CRTC);
		return ret;
	}

	return 0;
}

static int exynos5433_decon_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &decon_component_ops);
	exynos_drm_component_del(&pdev->dev, EXYNOS_DEVICE_TYPE_CRTC);

	return 0;
}

static const struct exynos5433_decon_driver_data exynos5433_decon_int_driver_data = {
	.type = EXYNOS_DISPLAY_TYPE_LCD,
	.first_win = 0,
};

static const struct exynos5433_decon_driver_data exynos5433_decon_ext_driver_data = {
	.type = EXYNOS_DISPLAY_TYPE_HDMI,
	.first_win = 1,
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
