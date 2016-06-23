/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <plat/map-base.h>

#include <drm/drmP.h>
#include <drm/exynos_drm.h>
#include "regs-gsc.h"
#include "exynos_drm_ipp.h"
#include "exynos_drm_gsc.h"
#ifdef CONFIG_DRM_EXYNOS_DBG
#include "exynos_drm_dbg.h"
#endif

/*
 * GSC stands for General SCaler and
 * supports image scaler/rotator and input/output DMA operations.
 * input DMA reads image data from the memory.
 * output DMA writes image data to memory.
 * GSC supports image rotation and image effect functions.
 *
 * M2M operation : supports crop/scale/rotation/csc so on.
 * Memory ----> GSC H/W ----> Memory.
 * Writeback operation : supports cloned screen with FIMD.
 * FIMD ----> GSC H/W ----> Memory.
 * Output operation : supports direct display using local path.
 * Memory ----> GSC H/W ----> FIMD, Mixer.
 */

/*
 * TODO
 * 1. check suspend/resume api if needed.
 * 2. need to check use case platform_device_id.
 * 3. check src/dst size with, height.
 * 4. added check_prepare api for right register.
 * 5. need to add supported list in prop_list.
 * 6. check prescaler/scaler optimization.
 */

#define GSC_MAX_DEVS	4
#define GSC_MAX_SRC		4
#define GSC_MAX_DST		16
#define GSC_RESET_TIMEOUT	50
#ifdef CONFIG_DRM_EXYNOS_DBG
#define GSC_MAX_REG	128
#endif
#define GSC_BUF_STOP	1
#define GSC_BUF_START	2
#define GSC_REG_SZ		16
#define GSC_WIDTH_ITU_709	1280
#define GSC_SC_UP_MAX_RATIO		65536
#define GSC_SC_DOWN_RATIO_7_8		74898
#define GSC_SC_DOWN_RATIO_6_8		87381
#define GSC_SC_DOWN_RATIO_5_8		104857
#define GSC_SC_DOWN_RATIO_4_8		131072
#define GSC_SC_DOWN_RATIO_3_8		174762
#define GSC_SC_DOWN_RATIO_2_8		262144
#define GSC_POLY_SC_DOWN_MAX		4
#define GSC_PRE_SC_DOWN_MAX		4
#define GSC_REFRESH_MIN	12
#define GSC_REFRESH_MAX	60
#define GSC_CROP_MAX	8192
#define GSC_CROP_MIN	32
#define GSC_SCALE_MAX	4224
#define GSC_SCALE_MIN	32
#define GSC_COEF_RATIO	7
#define GSC_COEF_PHASE	9
#define GSC_COEF_ATTR	16
#define GSC_COEF_H_8T	8
#define GSC_COEF_V_4T	4
#define GSC_COEF_DEPTH	3

#define get_gsc_context(dev)	platform_get_drvdata(to_platform_device(dev))
#define get_ctx_from_ippdrv(ippdrv)	container_of(ippdrv,\
					struct gsc_context, ippdrv);
#define gsc_read(offset)		readl(ctx->regs + (offset))
#define gsc_write(cfg, offset)	writel(cfg, ctx->regs + (offset))

/*
 * A structure of scaler.
 *
 * @range: narrow, wide.
 * @pre_shfactor: pre sclaer shift factor.
 * @pre_hratio: horizontal ratio of the prescaler.
 * @pre_vratio: vertical ratio of the prescaler.
 * @main_hratio: the main scaler's horizontal ratio.
 * @main_vratio: the main scaler's vertical ratio.
 */
struct gsc_scaler {
	bool	range;
	u32	pre_shfactor;
	u32	pre_hratio;
	u32	pre_vratio;
	unsigned long main_hratio;
	unsigned long main_vratio;
};

/*
 * A structure of scaler capability.
 *
 * find user manual 49.2 features.
 * @tile_w: tile mode or rotation width.
 * @tile_h: tile mode or rotation height.
 * @w: other cases width.
 * @h: other cases height.
 */
struct gsc_capability {
	/* tile or rotation */
	u32	tile_w;
	u32	tile_h;
	/* other cases */
	u32	w;
	u32	h;
};

/*
 * A structure of gsc driver data.
 *
 * @gsc_clk: name of gsc clock.
 */
struct gsc_driverdata {
	char	*gsc_clk;
};

/*
 * A structure of gsc context.
 *
 * @ippdrv: prepare initialization using ippdrv.
 * @regs_res: register resources.
 * @regs: memory mapped io registers.
 * @lock: locking of operations.
 * @gsc_clk: gsc gate clock.
 * @sc: scaler infomations.
 * @id: gsc id.
 * @irq: irq number.
 * @rotation: supports rotation of src.
 */
struct gsc_context {
	struct exynos_drm_ippdrv	ippdrv;
	struct resource	*regs_res;
	void __iomem	*regs;
	struct mutex	lock;
	struct clk	*gsc_clk;
	struct gsc_scaler	sc;
	struct gsc_driverdata	*ddata;
	int	id;
	int	irq;
	bool	rotation;
};

/* 8-tap Filter Coefficient */
static const int h_coef_8t[GSC_COEF_RATIO][GSC_COEF_ATTR][GSC_COEF_H_8T] = {
	{	/* Ratio <= 65536 (~8:8) */
		{  0,  0,   0, 128,   0,   0,  0,  0 },
		{ -1,  2,  -6, 127,   7,  -2,  1,  0 },
		{ -1,  4, -12, 125,  16,  -5,  1,  0 },
		{ -1,  5, -15, 120,  25,  -8,  2,  0 },
		{ -1,  6, -18, 114,  35, -10,  3, -1 },
		{ -1,  6, -20, 107,  46, -13,  4, -1 },
		{ -2,  7, -21,  99,  57, -16,  5, -1 },
		{ -1,  6, -20,  89,  68, -18,  5, -1 },
		{ -1,  6, -20,  79,  79, -20,  6, -1 },
		{ -1,  5, -18,  68,  89, -20,  6, -1 },
		{ -1,  5, -16,  57,  99, -21,  7, -2 },
		{ -1,  4, -13,  46, 107, -20,  6, -1 },
		{ -1,  3, -10,  35, 114, -18,  6, -1 },
		{  0,  2,  -8,  25, 120, -15,  5, -1 },
		{  0,  1,  -5,  16, 125, -12,  4, -1 },
		{  0,  1,  -2,   7, 127,  -6,  2, -1 }
	}, {	/* 65536 < Ratio <= 74898 (~8:7) */
		{  3, -8,  14, 111,  13,  -8,  3,  0 },
		{  2, -6,   7, 112,  21, -10,  3, -1 },
		{  2, -4,   1, 110,  28, -12,  4, -1 },
		{  1, -2,  -3, 106,  36, -13,  4, -1 },
		{  1, -1,  -7, 103,  44, -15,  4, -1 },
		{  1,  1, -11,  97,  53, -16,  4, -1 },
		{  0,  2, -13,  91,  61, -16,  4, -1 },
		{  0,  3, -15,  85,  69, -17,  4, -1 },
		{  0,  3, -16,  77,  77, -16,  3,  0 },
		{ -1,  4, -17,  69,  85, -15,  3,  0 },
		{ -1,  4, -16,  61,  91, -13,  2,  0 },
		{ -1,  4, -16,  53,  97, -11,  1,  1 },
		{ -1,  4, -15,  44, 103,  -7, -1,  1 },
		{ -1,  4, -13,  36, 106,  -3, -2,  1 },
		{ -1,  4, -12,  28, 110,   1, -4,  2 },
		{ -1,  3, -10,  21, 112,   7, -6,  2 }
	}, {	/* 74898 < Ratio <= 87381 (~8:6) */
		{ 2, -11,  25,  96, 25, -11,   2,  0 },
		{ 2, -10,  19,  96, 31, -12,   2,  0 },
		{ 2,  -9,  14,  94, 37, -12,   2,  0 },
		{ 2,  -8,  10,  92, 43, -12,   1,  0 },
		{ 2,  -7,   5,  90, 49, -12,   1,  0 },
		{ 2,  -5,   1,  86, 55, -12,   0,  1 },
		{ 2,  -4,  -2,  82, 61, -11,  -1,  1 },
		{ 1,  -3,  -5,  77, 67,  -9,  -1,  1 },
		{ 1,  -2,  -7,  72, 72,  -7,  -2,  1 },
		{ 1,  -1,  -9,  67, 77,  -5,  -3,  1 },
		{ 1,  -1, -11,  61, 82,  -2,  -4,  2 },
		{ 1,   0, -12,  55, 86,   1,  -5,  2 },
		{ 0,   1, -12,  49, 90,   5,  -7,  2 },
		{ 0,   1, -12,  43, 92,  10,  -8,  2 },
		{ 0,   2, -12,  37, 94,  14,  -9,  2 },
		{ 0,   2, -12,  31, 96,  19, -10,  2 }
	}, {	/* 87381 < Ratio <= 104857 (~8:5) */
		{ -1,  -8, 33,  80, 33,  -8,  -1,  0 },
		{ -1,  -8, 28,  80, 37,  -7,  -2,  1 },
		{  0,  -8, 24,  79, 41,  -7,  -2,  1 },
		{  0,  -8, 20,  78, 46,  -6,  -3,  1 },
		{  0,  -8, 16,  76, 50,  -4,  -3,  1 },
		{  0,  -7, 13,  74, 54,  -3,  -4,  1 },
		{  1,  -7, 10,  71, 58,  -1,  -5,  1 },
		{  1,  -6,  6,  68, 62,   1,  -5,  1 },
		{  1,  -6,  4,  65, 65,   4,  -6,  1 },
		{  1,  -5,  1,  62, 68,   6,  -6,  1 },
		{  1,  -5, -1,  58, 71,  10,  -7,  1 },
		{  1,  -4, -3,  54, 74,  13,  -7,  0 },
		{  1,  -3, -4,  50, 76,  16,  -8,  0 },
		{  1,  -3, -6,  46, 78,  20,  -8,  0 },
		{  1,  -2, -7,  41, 79,  24,  -8,  0 },
		{  1,  -2, -7,  37, 80,  28,  -8, -1 }
	}, {	/* 104857 < Ratio <= 131072 (~8:4) */
		{ -3,   0, 35,  64, 35,   0,  -3,  0 },
		{ -3,  -1, 32,  64, 38,   1,  -3,  0 },
		{ -2,  -2, 29,  63, 41,   2,  -3,  0 },
		{ -2,  -3, 27,  63, 43,   4,  -4,  0 },
		{ -2,  -3, 24,  61, 46,   6,  -4,  0 },
		{ -2,  -3, 21,  60, 49,   7,  -4,  0 },
		{ -1,  -4, 19,  59, 51,   9,  -4, -1 },
		{ -1,  -4, 16,  57, 53,  12,  -4, -1 },
		{ -1,  -4, 14,  55, 55,  14,  -4, -1 },
		{ -1,  -4, 12,  53, 57,  16,  -4, -1 },
		{ -1,  -4,  9,  51, 59,  19,  -4, -1 },
		{  0,  -4,  7,  49, 60,  21,  -3, -2 },
		{  0,  -4,  6,  46, 61,  24,  -3, -2 },
		{  0,  -4,  4,  43, 63,  27,  -3, -2 },
		{  0,  -3,  2,  41, 63,  29,  -2, -2 },
		{  0,  -3,  1,  38, 64,  32,  -1, -3 }
	}, {	/* 131072 < Ratio <= 174762 (~8:3) */
		{ -1,   8, 33,  48, 33,   8,  -1,  0 },
		{ -1,   7, 31,  49, 35,   9,  -1, -1 },
		{ -1,   6, 30,  49, 36,  10,  -1, -1 },
		{ -1,   5, 28,  48, 38,  12,  -1, -1 },
		{ -1,   4, 26,  48, 39,  13,   0, -1 },
		{ -1,   3, 24,  47, 41,  15,   0, -1 },
		{ -1,   2, 23,  47, 42,  16,   0, -1 },
		{ -1,   2, 21,  45, 43,  18,   1, -1 },
		{ -1,   1, 19,  45, 45,  19,   1, -1 },
		{ -1,   1, 18,  43, 45,  21,   2, -1 },
		{ -1,   0, 16,  42, 47,  23,   2, -1 },
		{ -1,   0, 15,  41, 47,  24,   3, -1 },
		{ -1,   0, 13,  39, 48,  26,   4, -1 },
		{ -1,  -1, 12,  38, 48,  28,   5, -1 },
		{ -1,  -1, 10,  36, 49,  30,   6, -1 },
		{ -1,  -1,  9,  35, 49,  31,   7, -1 }
	}, {	/* 174762 < Ratio <= 262144 (~8:2) */
		{  2,  13, 30,  38, 30,  13,   2,  0 },
		{  2,  12, 29,  38, 30,  14,   3,  0 },
		{  2,  11, 28,  38, 31,  15,   3,  0 },
		{  2,  10, 26,  38, 32,  16,   4,  0 },
		{  1,  10, 26,  37, 33,  17,   4,  0 },
		{  1,   9, 24,  37, 34,  18,   5,  0 },
		{  1,   8, 24,  37, 34,  19,   5,  0 },
		{  1,   7, 22,  36, 35,  20,   6,  1 },
		{  1,   6, 21,  36, 36,  21,   6,  1 },
		{  1,   6, 20,  35, 36,  22,   7,  1 },
		{  0,   5, 19,  34, 37,  24,   8,  1 },
		{  0,   5, 18,  34, 37,  24,   9,  1 },
		{  0,   4, 17,  33, 37,  26,  10,  1 },
		{  0,   4, 16,  32, 38,  26,  10,  2 },
		{  0,   3, 15,  31, 38,  28,  11,  2 },
		{  0,   3, 14,  30, 38,  29,  12,  2 }
	}
};

/* 4-tap Filter Coefficient */
static const int v_coef_4t[GSC_COEF_RATIO][GSC_COEF_ATTR][GSC_COEF_V_4T] = {
	{	/* Ratio <= 65536 (~8:8) */
		{  0, 128,   0,  0 },
		{ -4, 127,   5,  0 },
		{ -6, 124,  11, -1 },
		{ -8, 118,  19, -1 },
		{ -8, 111,  27, -2 },
		{ -8, 102,  37, -3 },
		{ -8,  92,  48, -4 },
		{ -7,  81,  59, -5 },
		{ -6,  70,  70, -6 },
		{ -5,  59,  81, -7 },
		{ -4,  48,  92, -8 },
		{ -3,  37, 102, -8 },
		{ -2,  27, 111, -8 },
		{ -1,  19, 118, -8 },
		{ -1,  11, 124, -6 },
		{  0,   5, 127, -4 }
	}, {	/* 65536 < Ratio <= 74898 (~8:7) */
		{  8, 112,   8,  0 },
		{  4, 111,  14, -1 },
		{  1, 109,  20, -2 },
		{ -2, 105,  27, -2 },
		{ -3, 100,  34, -3 },
		{ -5,  93,  43, -3 },
		{ -5,  86,  51, -4 },
		{ -5,  77,  60, -4 },
		{ -5,  69,  69, -5 },
		{ -4,  60,  77, -5 },
		{ -4,  51,  86, -5 },
		{ -3,  43,  93, -5 },
		{ -3,  34, 100, -3 },
		{ -2,  27, 105, -2 },
		{ -2,  20, 109,  1 },
		{ -1,  14, 111,  4 }
	}, {	/* 74898 < Ratio <= 87381 (~8:6) */
		{ 16,  96,  16,  0 },
		{ 12,  97,  21, -2 },
		{  8,  96,  26, -2 },
		{  5,  93,  32, -2 },
		{  2,  89,  39, -2 },
		{  0,  84,  46, -2 },
		{ -1,  79,  53, -3 },
		{ -2,  73,  59, -2 },
		{ -2,  66,  66, -2 },
		{ -2,  59,  73, -2 },
		{ -3,  53,  79, -1 },
		{ -2,  46,  84,  0 },
		{ -2,  39,  89,  2 },
		{ -2,  32,  93,  5 },
		{ -2,  26,  96,  8 },
		{ -2,  21,  97, 12 }
	}, {	/* 87381 < Ratio <= 104857 (~8:5) */
		{ 22,  84,  22,  0 },
		{ 18,  85,  26, -1 },
		{ 14,  84,  31, -1 },
		{ 11,  82,  36, -1 },
		{  8,  79,  42, -1 },
		{  6,  76,  47, -1 },
		{  4,  72,  52,  0 },
		{  2,  68,  58,  0 },
		{  1,  63,  63,  1 },
		{  0,  58,  68,  2 },
		{  0,  52,  72,  4 },
		{ -1,  47,  76,  6 },
		{ -1,  42,  79,  8 },
		{ -1,  36,  82, 11 },
		{ -1,  31,  84, 14 },
		{ -1,  26,  85, 18 }
	}, {	/* 104857 < Ratio <= 131072 (~8:4) */
		{ 26,  76,  26,  0 },
		{ 22,  76,  30,  0 },
		{ 19,  75,  34,  0 },
		{ 16,  73,  38,  1 },
		{ 13,  71,  43,  1 },
		{ 10,  69,  47,  2 },
		{  8,  66,  51,  3 },
		{  6,  63,  55,  4 },
		{  5,  59,  59,  5 },
		{  4,  55,  63,  6 },
		{  3,  51,  66,  8 },
		{  2,  47,  69, 10 },
		{  1,  43,  71, 13 },
		{  1,  38,  73, 16 },
		{  0,  34,  75, 19 },
		{  0,  30,  76, 22 }
	}, {	/* 131072 < Ratio <= 174762 (~8:3) */
		{ 29,  70,  29,  0 },
		{ 26,  68,  32,  2 },
		{ 23,  67,  36,  2 },
		{ 20,  66,  39,  3 },
		{ 17,  65,  43,  3 },
		{ 15,  63,  46,  4 },
		{ 12,  61,  50,  5 },
		{ 10,  58,  53,  7 },
		{  8,  56,  56,  8 },
		{  7,  53,  58, 10 },
		{  5,  50,  61, 12 },
		{  4,  46,  63, 15 },
		{  3,  43,  65, 17 },
		{  3,  39,  66, 20 },
		{  2,  36,  67, 23 },
		{  2,  32,  68, 26 }
	}, {	/* 174762 < Ratio <= 262144 (~8:2) */
		{ 32,  64,  32,  0 },
		{ 28,  63,  34,  3 },
		{ 25,  62,  37,  4 },
		{ 22,  62,  40,  4 },
		{ 19,  61,  43,  5 },
		{ 17,  59,  46,  6 },
		{ 15,  58,  48,  7 },
		{ 13,  55,  51,  9 },
		{ 11,  53,  53, 11 },
		{  9,  51,  55, 13 },
		{  7,  48,  58, 15 },
		{  6,  46,  59, 17 },
		{  5,  43,  61, 19 },
		{  4,  40,  62, 22 },
		{  4,  37,  62, 25 },
		{  3,  34,  63, 28 }
	}
};

enum gsc_outputpath {
	GSC_DMA,
	GSC_MIXER,
	GSC_FIMD,
};

static int gsc_sw_reset(struct gsc_context *ctx)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property = &c_node->property;
	u32 cfg;
	int count = GSC_RESET_TIMEOUT, in_pp_cnt = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* s/w reset */
	cfg = (GSC_SW_RESET_SRESET);
	gsc_write(cfg, GSC_SW_RESET);

	/* wait s/w reset complete */
	while (count--) {
		cfg = gsc_read(GSC_SW_RESET);
		if (!cfg)
			break;
		usleep_range(1000, 2000);
	}

	if (cfg) {
		DRM_ERROR("failed to reset gsc h/w.\n");
		return -EBUSY;
	}

	if (ipp_is_output_cmd(property->cmd))
		in_pp_cnt = GSC_MAX_SRC;

	/* reset sequence */
	cfg = gsc_read(GSC_IN_BASE_ADDR_Y_MASK);
	cfg |= (GSC_IN_BASE_ADDR_MASK |
		GSC_IN_BASE_ADDR_PINGPONG(in_pp_cnt));
	gsc_write(cfg, GSC_IN_BASE_ADDR_Y_MASK);
	gsc_write(cfg, GSC_IN_BASE_ADDR_CB_MASK);
	gsc_write(cfg, GSC_IN_BASE_ADDR_CR_MASK);

	cfg = gsc_read(GSC_OUT_BASE_ADDR_Y_MASK);
	cfg |= (GSC_OUT_BASE_ADDR_MASK |
		GSC_OUT_BASE_ADDR_PINGPONG(0));
	gsc_write(cfg, GSC_OUT_BASE_ADDR_Y_MASK);
	gsc_write(cfg, GSC_OUT_BASE_ADDR_CB_MASK);
	gsc_write(cfg, GSC_OUT_BASE_ADDR_CR_MASK);

	return 0;
}

static void gsc_set_gscblk_fimd_wb(struct gsc_context *ctx, bool enable)
{
	u32 gscblk_cfg;

	DRM_DEBUG_KMS("%s\n", __func__);

	gscblk_cfg = readl(SYSREG_GSCBLK_CFG1);

	if (enable)
		gscblk_cfg |= GSC_BLK_DISP1WB_DEST(ctx->id) |
				GSC_BLK_GSCL_WB_IN_SRC_SEL(ctx->id) |
				GSC_BLK_SW_RESET_WB_DEST(ctx->id);
	else
		gscblk_cfg |= GSC_BLK_PXLASYNC_LO_MASK_WB(ctx->id);

	writel(gscblk_cfg, SYSREG_GSCBLK_CFG1);
}

static void gsc_set_local_output_path(int id, int out, bool on)
{
	u32 cfg = readl(SYSREG_GSCBLK_CFG0);

	if (out == GSC_FIMD) {
		if (on)
			cfg |= (GSC_OUT_DST_FIMD_SEL(id));
		else
			cfg &= ~((GSC_OUT_DST_FIMD_SEL(id)));
	} else if (out == GSC_MIXER) {
		if (on)
			cfg |= (GSC_OUT_DST_MXR_SEL(id));
		else
			cfg &= ~((GSC_OUT_DST_MXR_SEL(id)));
	}
	writel(cfg, SYSREG_GSCBLK_CFG0);
}

static void gsc_handle_irq(struct gsc_context *ctx, bool enable,
		bool overflow, bool done)
{
	u32 cfg;

	DRM_DEBUG_KMS("%s:enable[%d]overflow[%d]level[%d]\n", __func__,
			enable, overflow, done);

	cfg = gsc_read(GSC_IRQ);
	cfg |= (GSC_IRQ_OR_MASK | GSC_IRQ_FRMDONE_MASK);

	if (enable)
		cfg |= GSC_IRQ_ENABLE;
	else
		cfg &= ~GSC_IRQ_ENABLE;

	if (overflow)
		cfg &= ~GSC_IRQ_OR_MASK;
	else
		cfg |= GSC_IRQ_OR_MASK;

	if (done)
		cfg &= ~GSC_IRQ_FRMDONE_MASK;
	else
		cfg |= GSC_IRQ_FRMDONE_MASK;

	gsc_write(cfg, GSC_IRQ);
}

static int gsc_set_planar_addr(struct drm_exynos_ipp_buf_info *buf_info,
		u32 fmt, struct drm_exynos_sz *sz)
{
	dma_addr_t *base[EXYNOS_DRM_PLANAR_MAX];
	uint64_t size[EXYNOS_DRM_PLANAR_MAX];
	uint64_t ofs[EXYNOS_DRM_PLANAR_MAX];
	bool bypass = false;
	uint64_t tsize = 0;
	int i;

	for_each_ipp_planar(i) {
		base[i] = &buf_info->base[i];
		size[i] = buf_info->size[i];
		ofs[i] = 0;
		tsize += size[i];
		DRM_DEBUG_KMS("%s:base[%d][0x%x]s[%d][%llu]\n", __func__,
			i, *base[i], i, size[i]);
	}

	if (!tsize) {
		DRM_INFO("%s:failed to get buffer size.\n", __func__);
		return 0;
	}

	switch (fmt) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV12M:
		ofs[0] = sz->hsize * sz->vsize;
		ofs[1] = ofs[0] >> 1;
		if (*base[0] && *base[1]) {
			if (size[0] + size[1] < ofs[0] + ofs[1])
				goto err_info;
			bypass = true;
		}
		break;
	case DRM_FORMAT_NV12MT:
		ofs[0] = ALIGN(ALIGN(sz->hsize, 128) *
				ALIGN(sz->vsize, 32), SZ_8K);
		ofs[1] = ALIGN(ALIGN(sz->hsize, 128) *
				ALIGN(sz->vsize >> 1, 32), SZ_8K);
		if (*base[0] && *base[1]) {
			if (size[0] + size[1] < ofs[0] + ofs[1])
				goto err_info;
			bypass = true;
		}
		break;
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		ofs[0] = sz->hsize * sz->vsize;
		ofs[1] = ofs[2] = ofs[0] >> 2;
		if (*base[0] && *base[1] && *base[2]) {
			if (size[0]+size[1]+size[2] < ofs[0]+ofs[1]+ofs[2])
				goto err_info;
			bypass = true;
		}
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
		ofs[0] = sz->hsize * sz->vsize << 2;
		if (*base[0]) {
			if (size[0] < ofs[0])
				goto err_info;
		}
		bypass = true;
		break;
	default:
		bypass = true;
		break;
	}

	if (!bypass) {
		*base[1] = *base[0] + ofs[0];
		if (ofs[1] && ofs[2])
			*base[2] = *base[1] + ofs[1];
	}

	DRM_DEBUG_KMS("%s:y[0x%x],cb[0x%x],cr[0x%x]\n", __func__,
		*base[0], *base[1], *base[2]);

	return 0;

err_info:
	DRM_ERROR("invalid size for fmt[0x%x]\n", fmt);

	for_each_ipp_planar(i) {
		base[i] = &buf_info->base[i];
		size[i] = buf_info->size[i];

		DRM_ERROR("base[%d][0x%x]s[%d][%llu]ofs[%d][%llu]\n",
			i, *base[i], i, size[i], i, ofs[i]);
	}

	return -EINVAL;
}

static int gsc_src_set_fmt(struct device *dev, u32 fmt)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:fmt[0x%x]\n", __func__, fmt);

	cfg = gsc_read(GSC_IN_CON);
	cfg &= ~(GSC_IN_RGB_TYPE_MASK | GSC_IN_YUV422_1P_ORDER_MASK |
		 GSC_IN_CHROMA_ORDER_MASK | GSC_IN_FORMAT_MASK |
		 GSC_IN_TILE_TYPE_MASK | GSC_IN_TILE_MODE |
		 GSC_IN_CHROM_STRIDE_SEL_MASK | GSC_IN_RB_SWAP_MASK);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
		cfg |= GSC_IN_RGB565;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
		cfg |= GSC_IN_XRGB8888;
		break;
	case DRM_FORMAT_BGRX8888:
		cfg |= (GSC_IN_XRGB8888 | GSC_IN_RB_SWAP);
		break;
	case DRM_FORMAT_YUYV:
		cfg |= (GSC_IN_YUV422_1P |
			GSC_IN_YUV422_1P_ORDER_LSB_Y |
			GSC_IN_CHROMA_ORDER_CBCR);
		break;
	case DRM_FORMAT_YVYU:
		cfg |= (GSC_IN_YUV422_1P |
			GSC_IN_YUV422_1P_ORDER_LSB_Y |
			GSC_IN_CHROMA_ORDER_CRCB);
		break;
	case DRM_FORMAT_UYVY:
		cfg |= (GSC_IN_YUV422_1P |
			GSC_IN_YUV422_1P_OEDER_LSB_C |
			GSC_IN_CHROMA_ORDER_CBCR);
		break;
	case DRM_FORMAT_VYUY:
		cfg |= (GSC_IN_YUV422_1P |
			GSC_IN_YUV422_1P_OEDER_LSB_C |
			GSC_IN_CHROMA_ORDER_CRCB);
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
		cfg |= (GSC_IN_CHROMA_ORDER_CRCB |
			GSC_IN_YUV420_2P);
		break;
	case DRM_FORMAT_YUV422:
		cfg |= GSC_IN_YUV422_3P;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		cfg |= GSC_IN_YUV420_3P;
		break;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV12M:
		cfg |= (GSC_IN_CHROMA_ORDER_CBCR |
			GSC_IN_YUV420_2P);
		break;
	case DRM_FORMAT_NV12MT:
		cfg |= (GSC_IN_YUV420_2P |GSC_IN_TILE_C_16x8 |
			GSC_IN_TILE_MODE);
		break;
	case DRM_FORMAT_YUV444:
		break;
	default:
		dev_err(ippdrv->dev, "inavlid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	gsc_write(cfg, GSC_IN_CON);

	return 0;
}

static int gsc_src_set_transf(struct device *dev,
		enum drm_exynos_degree degree,
		enum drm_exynos_flip flip, bool *swap)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:degree[%d]flip[0x%x]\n", __func__,
		degree, flip);

	cfg = gsc_read(GSC_IN_CON);
	cfg &= ~GSC_IN_ROT_MASK;

	switch (degree) {
	case EXYNOS_DRM_DEGREE_0:
		if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= GSC_IN_ROT_XFLIP;
		if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= GSC_IN_ROT_YFLIP;
		break;
	case EXYNOS_DRM_DEGREE_90:
		if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= GSC_IN_ROT_90_XFLIP;
		else if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= GSC_IN_ROT_90_YFLIP;
		else
			cfg |= GSC_IN_ROT_90;
		break;
	case EXYNOS_DRM_DEGREE_180:
		cfg |= GSC_IN_ROT_180;
		break;
	case EXYNOS_DRM_DEGREE_270:
		cfg |= GSC_IN_ROT_270;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	gsc_write(cfg, GSC_IN_CON);

	ctx->rotation = (cfg &	GSC_IN_ROT_90) ? 1 : 0;
	*swap = ctx->rotation;

	return 0;
}

static int gsc_src_set_size(struct device *dev, int swap,
		struct drm_exynos_pos *pos, struct drm_exynos_sz *sz)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct drm_exynos_pos img_pos = *pos;
	struct gsc_scaler *sc = &ctx->sc;
	u32 cfg;

	DRM_DEBUG_KMS("%s:swap[%d]x[%d]y[%d]w[%d]h[%d]\n",
		__func__, swap, pos->x, pos->y, pos->w, pos->h);

	if (swap) {
		img_pos.w = pos->h;
		img_pos.h = pos->w;
	}

	/* pixel offset */
	cfg = (GSC_SRCIMG_OFFSET_X(img_pos.x) |
		GSC_SRCIMG_OFFSET_Y(img_pos.y));
	gsc_write(cfg, GSC_SRCIMG_OFFSET);

	/* cropped size */
	cfg = (GSC_CROPPED_WIDTH(img_pos.w) |
		GSC_CROPPED_HEIGHT(img_pos.h));
	gsc_write(cfg, GSC_CROPPED_SIZE);

	DRM_DEBUG_KMS("%s:hsize[%d]vsize[%d]\n",
		__func__, sz->hsize, sz->vsize);

	/* original size */
	cfg = gsc_read(GSC_SRCIMG_SIZE);
	cfg &= ~(GSC_SRCIMG_HEIGHT_MASK |
		GSC_SRCIMG_WIDTH_MASK);

	cfg |= (GSC_SRCIMG_WIDTH(sz->hsize) |
		GSC_SRCIMG_HEIGHT(sz->vsize));

	gsc_write(cfg, GSC_SRCIMG_SIZE);

	cfg = gsc_read(GSC_IN_CON);
	cfg &= ~GSC_IN_RGB_TYPE_MASK;

	DRM_DEBUG_KMS("%s:width[%d]range[%d]\n",
		__func__, pos->w, sc->range);

	if (pos->w >= GSC_WIDTH_ITU_709)
		if (sc->range)
			cfg |= GSC_IN_RGB_HD_WIDE;
		else
			cfg |= GSC_IN_RGB_HD_NARROW;
	else
		if (sc->range)
			cfg |= GSC_IN_RGB_SD_WIDE;
		else
			cfg |= GSC_IN_RGB_SD_NARROW;

	gsc_write(cfg, GSC_IN_CON);

	return 0;
}

void gsc_set_in_pingpong_update(struct gsc_context *ctx)
{
	u32 cfg = gsc_read(GSC_ENABLE);

	cfg |= GSC_ENABLE_IN_PP_UPDATE;
	gsc_write(cfg, GSC_ENABLE);
}

static int gsc_src_set_buf_seq(struct gsc_context *ctx, u32 buf_id,
		enum drm_exynos_ipp_buf_type buf_type)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	bool masked;
	u32 cfg;
	u32 mask = 0x00000001 << buf_id;

	DRM_DEBUG_KMS("%s:buf_id[%d]buf_type[%d]\n", __func__,
		buf_id, buf_type);

	/* mask register set */
	cfg = gsc_read(GSC_IN_BASE_ADDR_Y_MASK);

	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		masked = false;
		break;
	case IPP_BUF_DEQUEUE:
		masked = true;
		break;
	default:
		dev_err(ippdrv->dev, "invalid buf ctrl parameter.\n");
		return -EINVAL;
	}

	/* sequence id */
	cfg &= ~mask;
	cfg |= masked << buf_id;
	gsc_write(cfg, GSC_IN_BASE_ADDR_Y_MASK);
	gsc_write(cfg, GSC_IN_BASE_ADDR_CB_MASK);
	gsc_write(cfg, GSC_IN_BASE_ADDR_CR_MASK);

	return 0;
}

static int gsc_src_set_addr(struct device *dev,
		struct drm_exynos_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_exynos_ipp_buf_type buf_type)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property;
	struct drm_exynos_ipp_config *config;
	int ret;

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EFAULT;
	}

	property = &c_node->property;

	DRM_DEBUG_KMS("%s:prop_id[%d]buf_id[%d]buf_type[%d]\n", __func__,
		property->prop_id, buf_id, buf_type);

	/* address register set */
	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		config = &property->config[EXYNOS_DRM_OPS_SRC];
		ret = gsc_set_planar_addr(buf_info, config->fmt, &config->sz);
		if (ret) {
			dev_err(dev, "failed to set plane src addr.\n");
			return ret;
		}

		gsc_write(buf_info->base[EXYNOS_DRM_PLANAR_Y],
			GSC_IN_BASE_ADDR_Y(buf_id));
		gsc_write(buf_info->base[EXYNOS_DRM_PLANAR_CB],
			GSC_IN_BASE_ADDR_CB(buf_id));
		gsc_write(buf_info->base[EXYNOS_DRM_PLANAR_CR],
			GSC_IN_BASE_ADDR_CR(buf_id));
		if (ipp_is_output_cmd(property->cmd))
			gsc_set_in_pingpong_update(ctx);
		break;
	case IPP_BUF_DEQUEUE:
	default:
		/* bypass */
		break;
	}

	return gsc_src_set_buf_seq(ctx, buf_id, buf_type);
}

static struct exynos_drm_ipp_ops gsc_src_ops = {
	.set_fmt = gsc_src_set_fmt,
	.set_transf = gsc_src_set_transf,
	.set_size = gsc_src_set_size,
	.set_addr = gsc_src_set_addr,
};

static int gsc_dst_set_fmt(struct device *dev, u32 fmt)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:fmt[0x%x]\n", __func__, fmt);

	cfg = gsc_read(GSC_OUT_CON);
	cfg &= ~(GSC_OUT_RGB_TYPE_MASK | GSC_OUT_YUV422_1P_ORDER_MASK |
		 GSC_OUT_CHROMA_ORDER_MASK | GSC_OUT_FORMAT_MASK |
		 GSC_OUT_CHROM_STRIDE_SEL_MASK | GSC_OUT_RB_SWAP_MASK |
		 GSC_OUT_GLOBAL_ALPHA_MASK);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
		cfg |= GSC_OUT_RGB565;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
		cfg |= GSC_OUT_XRGB8888;
		break;
	case DRM_FORMAT_BGRX8888:
		cfg |= (GSC_OUT_XRGB8888 | GSC_OUT_RB_SWAP);
		break;
	case DRM_FORMAT_YUYV:
		cfg |= (GSC_OUT_YUV422_1P |
			GSC_OUT_YUV422_1P_ORDER_LSB_Y |
			GSC_OUT_CHROMA_ORDER_CBCR);
		break;
	case DRM_FORMAT_YVYU:
		cfg |= (GSC_OUT_YUV422_1P |
			GSC_OUT_YUV422_1P_ORDER_LSB_Y |
			GSC_OUT_CHROMA_ORDER_CRCB);
		break;
	case DRM_FORMAT_UYVY:
		cfg |= (GSC_OUT_YUV422_1P |
			GSC_OUT_YUV422_1P_OEDER_LSB_C |
			GSC_OUT_CHROMA_ORDER_CBCR);
		break;
	case DRM_FORMAT_VYUY:
		cfg |= (GSC_OUT_YUV422_1P |
			GSC_OUT_YUV422_1P_OEDER_LSB_C |
			GSC_OUT_CHROMA_ORDER_CRCB);
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
		cfg |= (GSC_OUT_CHROMA_ORDER_CRCB | GSC_OUT_YUV420_2P);
		break;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		cfg |= GSC_OUT_YUV420_3P;
		break;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV12M:
		cfg |= (GSC_OUT_CHROMA_ORDER_CBCR |
			GSC_OUT_YUV420_2P);
		break;
	case DRM_FORMAT_NV12MT:
		cfg |= (GSC_OUT_YUV420_2P |GSC_OUT_TILE_C_16x8 |
			GSC_OUT_TILE_MODE);
		break;
	case DRM_FORMAT_YUV444:
		cfg |= GSC_OUT_YUV444;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	gsc_write(cfg, GSC_OUT_CON);

	return 0;
}

static int gsc_dst_set_transf(struct device *dev,
		enum drm_exynos_degree degree,
		enum drm_exynos_flip flip, bool *swap)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:degree[%d]flip[0x%x]\n", __func__,
		degree, flip);

	cfg = gsc_read(GSC_IN_CON);
	cfg &= ~GSC_IN_ROT_MASK;

	switch (degree) {
	case EXYNOS_DRM_DEGREE_0:
		if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= GSC_IN_ROT_XFLIP;
		if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= GSC_IN_ROT_YFLIP;
		break;
	case EXYNOS_DRM_DEGREE_90:
		if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= GSC_IN_ROT_90_XFLIP;
		else if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= GSC_IN_ROT_90_YFLIP;
		else
			cfg |= GSC_IN_ROT_90;
		break;
	case EXYNOS_DRM_DEGREE_180:
		cfg |= GSC_IN_ROT_180;
		break;
	case EXYNOS_DRM_DEGREE_270:
		cfg |= GSC_IN_ROT_270;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	gsc_write(cfg, GSC_IN_CON);

	ctx->rotation = (cfg &	GSC_IN_ROT_90) ? 1 : 0;
	*swap = ctx->rotation;

	return 0;
}

static int gsc_get_ratio_shift(u32 src, u32 dst, u32 *ratio)
{
	DRM_DEBUG_KMS("%s:src[%d]dst[%d]\n", __func__, src, dst);

	if ((dst > src) || (dst >= src / GSC_POLY_SC_DOWN_MAX)) {
		*ratio = 1;
		return 0;
	}

	if ((src / GSC_POLY_SC_DOWN_MAX / GSC_PRE_SC_DOWN_MAX) > dst) {
		DRM_ERROR("exceeded max scale down(1/16).");
		return -EINVAL;
	}

	*ratio = (dst > (src / 8)) ? 2 : 4;

	return 0;
}

static void gsc_get_prescaler_shfactor(u32 hratio, u32 vratio, u32 *shfactor)
{
	if (hratio == 4 && vratio == 4)
		*shfactor = 4;
	else if ((hratio == 4 && vratio == 2) ||
		 (hratio == 2 && vratio == 4))
		*shfactor = 3;
	else if ((hratio == 4 && vratio == 1) ||
		 (hratio == 1 && vratio == 4) ||
		 (hratio == 2 && vratio == 2))
		*shfactor = 2;
	else if (hratio == 1 && vratio == 1)
		*shfactor = 0;
	else
		*shfactor = 1;
}

static int gsc_set_prescaler(struct gsc_context *ctx, struct gsc_scaler *sc,
		struct drm_exynos_pos *src, struct drm_exynos_pos *dst)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;
	u32 src_w, src_h, dst_w, dst_h;
	int ret = 0;

	src_w = src->w;
	src_h = src->h;

	if (ctx->rotation) {
		dst_w = dst->h;
		dst_h = dst->w;
	} else {
		dst_w = dst->w;
		dst_h = dst->h;
	}

	ret = gsc_get_ratio_shift(src_w, dst_w, &sc->pre_hratio);
	if (ret) {
		dev_err(ippdrv->dev, "failed to get ratio horizontal.\n");
		return ret;
	}

	ret = gsc_get_ratio_shift(src_h, dst_h, &sc->pre_vratio);
	if (ret) {
		dev_err(ippdrv->dev, "failed to get ratio vertical.\n");
		return ret;
	}

	DRM_DEBUG_KMS("%s:pre_hratio[%d]pre_vratio[%d]\n",
		__func__, sc->pre_hratio, sc->pre_vratio);

	sc->main_hratio = (src_w << 16) / dst_w;
	sc->main_vratio = (src_h << 16) / dst_h;

	DRM_DEBUG_KMS("%s:main_hratio[%ld]main_vratio[%ld]\n",
		__func__, sc->main_hratio, sc->main_vratio);

	gsc_get_prescaler_shfactor(sc->pre_hratio, sc->pre_vratio,
		&sc->pre_shfactor);

	DRM_DEBUG_KMS("%s:pre_shfactor[%d]\n", __func__,
		sc->pre_shfactor);

	cfg = (GSC_PRESC_SHFACTOR(sc->pre_shfactor) |
		GSC_PRESC_H_RATIO(sc->pre_hratio) |
		GSC_PRESC_V_RATIO(sc->pre_vratio));
	gsc_write(cfg, GSC_PRE_SCALE_RATIO);

	return ret;
}

static void gsc_set_h_coef(struct gsc_context *ctx, unsigned long main_hratio)
{
	int i, j, k, sc_ratio;

	if (main_hratio <= GSC_SC_UP_MAX_RATIO)
		sc_ratio = 0;
	else if (main_hratio <= GSC_SC_DOWN_RATIO_7_8)
		sc_ratio = 1;
	else if (main_hratio <= GSC_SC_DOWN_RATIO_6_8)
		sc_ratio = 2;
	else if (main_hratio <= GSC_SC_DOWN_RATIO_5_8)
		sc_ratio = 3;
	else if (main_hratio <= GSC_SC_DOWN_RATIO_4_8)
		sc_ratio = 4;
	else if (main_hratio <= GSC_SC_DOWN_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < GSC_COEF_PHASE; i++)
		for (j = 0; j < GSC_COEF_H_8T; j++)
			for (k = 0; k < GSC_COEF_DEPTH; k++)
				gsc_write(h_coef_8t[sc_ratio][i][j],
					GSC_HCOEF(i, j, k));
}

static void gsc_set_v_coef(struct gsc_context *ctx, unsigned long main_vratio)
{
	int i, j, k, sc_ratio;

	if (main_vratio <= GSC_SC_UP_MAX_RATIO)
		sc_ratio = 0;
	else if (main_vratio <= GSC_SC_DOWN_RATIO_7_8)
		sc_ratio = 1;
	else if (main_vratio <= GSC_SC_DOWN_RATIO_6_8)
		sc_ratio = 2;
	else if (main_vratio <= GSC_SC_DOWN_RATIO_5_8)
		sc_ratio = 3;
	else if (main_vratio <= GSC_SC_DOWN_RATIO_4_8)
		sc_ratio = 4;
	else if (main_vratio <= GSC_SC_DOWN_RATIO_3_8)
		sc_ratio = 5;
	else
		sc_ratio = 6;

	for (i = 0; i < GSC_COEF_PHASE; i++)
		for (j = 0; j < GSC_COEF_V_4T; j++)
			for (k = 0; k < GSC_COEF_DEPTH; k++)
				gsc_write(v_coef_4t[sc_ratio][i][j],
					GSC_VCOEF(i, j, k));
}

static void gsc_set_scaler(struct gsc_context *ctx, struct gsc_scaler *sc)
{
	u32 cfg;

	DRM_DEBUG_KMS("%s:main_hratio[%ld]main_vratio[%ld]\n",
		__func__, sc->main_hratio, sc->main_vratio);

	gsc_set_h_coef(ctx, sc->main_hratio);
	cfg = GSC_MAIN_H_RATIO_VALUE(sc->main_hratio);
	gsc_write(cfg, GSC_MAIN_H_RATIO);

	gsc_set_v_coef(ctx, sc->main_vratio);
	cfg = GSC_MAIN_V_RATIO_VALUE(sc->main_vratio);
	gsc_write(cfg, GSC_MAIN_V_RATIO);
}

static int gsc_dst_set_size(struct device *dev, int swap,
		struct drm_exynos_pos *pos, struct drm_exynos_sz *sz)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct drm_exynos_pos img_pos = *pos;
	struct gsc_scaler *sc = &ctx->sc;
	u32 cfg;

	DRM_DEBUG_KMS("%s:swap[%d]x[%d]y[%d]w[%d]h[%d]\n",
		__func__, swap, pos->x, pos->y, pos->w, pos->h);

	if (swap) {
		img_pos.w = pos->h;
		img_pos.h = pos->w;
	}

	/* pixel offset */
	cfg = (GSC_DSTIMG_OFFSET_X(pos->x) |
		GSC_DSTIMG_OFFSET_Y(pos->y));
	gsc_write(cfg, GSC_DSTIMG_OFFSET);

	/* scaled size */
	cfg = (GSC_SCALED_WIDTH(img_pos.w) | GSC_SCALED_HEIGHT(img_pos.h));
	gsc_write(cfg, GSC_SCALED_SIZE);

	DRM_DEBUG_KMS("%s:hsize[%d]vsize[%d]\n",
		__func__, sz->hsize, sz->vsize);

	/* original size */
	cfg = gsc_read(GSC_DSTIMG_SIZE);
	cfg &= ~(GSC_DSTIMG_HEIGHT_MASK |
		GSC_DSTIMG_WIDTH_MASK);
	cfg |= (GSC_DSTIMG_WIDTH(sz->hsize) |
		GSC_DSTIMG_HEIGHT(sz->vsize));
	gsc_write(cfg, GSC_DSTIMG_SIZE);

	cfg = gsc_read(GSC_OUT_CON);
	cfg &= ~GSC_OUT_RGB_TYPE_MASK;

	DRM_DEBUG_KMS("%s:width[%d]range[%d]\n",
		__func__, pos->w, sc->range);

	if (pos->w >= GSC_WIDTH_ITU_709)
		if (sc->range)
			cfg |= GSC_OUT_RGB_HD_WIDE;
		else
			cfg |= GSC_OUT_RGB_HD_NARROW;
	else
		if (sc->range)
			cfg |= GSC_OUT_RGB_SD_WIDE;
		else
			cfg |= GSC_OUT_RGB_SD_NARROW;

	gsc_write(cfg, GSC_OUT_CON);

	return 0;
}

static int gsc_dst_get_buf_seq(struct gsc_context *ctx)
{
	u32 cfg, i, buf_num = GSC_REG_SZ;
	u32 mask = 0x00000001;

	cfg = gsc_read(GSC_OUT_BASE_ADDR_Y_MASK);

	for (i = 0; i < GSC_REG_SZ; i++)
		if (cfg & (mask << i))
			buf_num--;

	DRM_DEBUG_KMS("%s:buf_num[%d]\n", __func__, buf_num);

	return buf_num;
}

static int gsc_dst_set_buf_seq(struct gsc_context *ctx, u32 buf_id,
		enum drm_exynos_ipp_buf_type buf_type)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	bool masked;
	u32 cfg;
	u32 mask = 0x00000001 << buf_id;
	int ret = 0;

	DRM_DEBUG_KMS("%s:buf_id[%d]buf_type[%d]\n", __func__,
		buf_id, buf_type);

	mutex_lock(&ctx->lock);

	/* mask register set */
	cfg = gsc_read(GSC_OUT_BASE_ADDR_Y_MASK);

	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		masked = false;
		break;
	case IPP_BUF_DEQUEUE:
		masked = true;
		break;
	default:
		dev_err(ippdrv->dev, "invalid buf ctrl parameter.\n");
		ret =  -EINVAL;
		goto err_unlock;
	}

	/* sequence id */
	cfg &= ~mask;
	cfg |= masked << buf_id;
	gsc_write(cfg, GSC_OUT_BASE_ADDR_Y_MASK);
	gsc_write(cfg, GSC_OUT_BASE_ADDR_CB_MASK);
	gsc_write(cfg, GSC_OUT_BASE_ADDR_CR_MASK);

	/* interrupt enable */
	if (buf_type == IPP_BUF_ENQUEUE &&
	    gsc_dst_get_buf_seq(ctx) >= GSC_BUF_START)
		gsc_handle_irq(ctx, true, false, true);

	/* interrupt disable */
	if (buf_type == IPP_BUF_DEQUEUE &&
	    gsc_dst_get_buf_seq(ctx) <= GSC_BUF_STOP)
		gsc_handle_irq(ctx, false, false, true);

err_unlock:
	mutex_unlock(&ctx->lock);
	return ret;
}

static int gsc_dst_set_addr(struct device *dev,
		struct drm_exynos_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_exynos_ipp_buf_type buf_type)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property;
	struct drm_exynos_ipp_config *config;
	int ret;

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EFAULT;
	}

	property = &c_node->property;

	DRM_DEBUG_KMS("%s:prop_id[%d]buf_id[%d]buf_type[%d]\n", __func__,
		property->prop_id, buf_id, buf_type);

	if (buf_id > GSC_MAX_DST) {
		dev_info(ippdrv->dev, "inavlid buf_id %d.\n", buf_id);
		return -EINVAL;
	}

	/* address register set */
	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		config = &property->config[EXYNOS_DRM_OPS_DST];
		ret = gsc_set_planar_addr(buf_info, config->fmt, &config->sz);
		if (ret) {
			dev_err(dev, "failed to set plane dst addr.\n");
			return ret;
		}

		gsc_write(buf_info->base[EXYNOS_DRM_PLANAR_Y],
			GSC_OUT_BASE_ADDR_Y(buf_id));
		gsc_write(buf_info->base[EXYNOS_DRM_PLANAR_CB],
			GSC_OUT_BASE_ADDR_CB(buf_id));
		gsc_write(buf_info->base[EXYNOS_DRM_PLANAR_CR],
			GSC_OUT_BASE_ADDR_CR(buf_id));
		break;
	case IPP_BUF_DEQUEUE:
		gsc_write(0x0, GSC_OUT_BASE_ADDR_Y(buf_id));
		gsc_write(0x0, GSC_OUT_BASE_ADDR_CB(buf_id));
		gsc_write(0x0, GSC_OUT_BASE_ADDR_CR(buf_id));
		break;
	default:
		/* bypass */
		break;
	}

	return gsc_dst_set_buf_seq(ctx, buf_id, buf_type);
}

static struct exynos_drm_ipp_ops gsc_dst_ops = {
	.set_fmt = gsc_dst_set_fmt,
	.set_transf = gsc_dst_set_transf,
	.set_size = gsc_dst_set_size,
	.set_addr = gsc_dst_set_addr,
};

static int gsc_clk_ctrl(struct device *dev, bool enable)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	int ret = 0;

	if (!c_node) {
		DRM_ERROR("enable[%d]:c_node is empty.\n", enable);
		return -EPERM;
	}

	DRM_DEBUG_KMS("%s:enable[%d]\n", __func__, enable);

	if (enable) {
		ret = clk_enable(ctx->gsc_clk);
		if (ret)
			DRM_ERROR("enable:ret[%d]\n", ret);
	}
	else
		clk_disable(ctx->gsc_clk);

	return ret;
}

static int gsc_get_src_buf_index(struct gsc_context *ctx)
{
	u32 cfg, curr_index, i;
	u32 buf_id = GSC_MAX_SRC;
	int ret;

	DRM_DEBUG_KMS("%s:gsc id[%d]\n", __func__, ctx->id);

	cfg = gsc_read(GSC_IN_BASE_ADDR_Y_MASK);
	curr_index = GSC_IN_CURR_GET_INDEX(cfg);

	for (i = curr_index; i < GSC_MAX_SRC; i++) {
		if (!((cfg >> i) & 0x1)) {
			buf_id = i;
			break;
		}
	}

	if (buf_id == GSC_MAX_SRC) {
		DRM_ERROR("failed to get in buffer index.\n");
		return -EINVAL;
	}

	ret = gsc_src_set_buf_seq(ctx, buf_id, IPP_BUF_DEQUEUE);
	if (ret < 0) {
		DRM_ERROR("failed to dequeue.\n");
		return ret;
	}

	DRM_DEBUG_KMS("%s:cfg[0x%x]curr_index[%d]buf_id[%d]\n", __func__, cfg,
		curr_index, buf_id);

	return buf_id;
}

static int gsc_get_dst_buf_index(struct gsc_context *ctx)
{
	u32 cfg, curr_index, i;
	u32 buf_id = GSC_MAX_DST;
	int ret;

	DRM_DEBUG_KMS("%s:gsc id[%d]\n", __func__, ctx->id);

	cfg = gsc_read(GSC_OUT_BASE_ADDR_Y_MASK);
	curr_index = GSC_OUT_CURR_GET_INDEX(cfg);

	for (i = curr_index; i < GSC_MAX_DST; i++) {
		if (!((cfg >> i) & 0x1)) {
			buf_id = i;
			break;
		}
	}

	if (buf_id == GSC_MAX_DST) {
		DRM_ERROR("failed to get out buffer index.\n");
		return -EINVAL;
	}

	ret = gsc_dst_set_buf_seq(ctx, buf_id, IPP_BUF_DEQUEUE);
	if (ret < 0) {
		DRM_ERROR("failed to dequeue.\n");
		return ret;
	}

	DRM_DEBUG_KMS("%s:cfg[0x%x]curr_index[%d]buf_id[%d]\n", __func__, cfg,
		curr_index, buf_id);

	return buf_id;
}

static irqreturn_t gsc_irq_handler(int irq, void *dev_id)
{
	struct gsc_context *ctx = dev_id;
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property = &c_node->property;
	struct drm_exynos_ipp_event_info *event = c_node->event;
	u32 status;
	int buf_id[EXYNOS_DRM_OPS_MAX] = {0, };

	DRM_DEBUG_KMS("%s:gsc id[%d]\n", __func__, ctx->id);

	status = gsc_read(GSC_IRQ);
	if (status & GSC_IRQ_STATUS_OR_IRQ) {
		dev_err(ippdrv->dev, "occured overflow at %d, status 0x%x.\n",
			ctx->id, status);
		gsc_write(GSC_IRQ_STATUS_OR_IRQ, GSC_IRQ);
		return IRQ_NONE;
	}

	if (c_node->state == IPP_STATE_STOP) {
		DRM_ERROR("invalid state:prop_id[%d]\n", property->prop_id);
		return IRQ_HANDLED;
	}

	if (status & GSC_IRQ_STATUS_OR_FRM_DONE) {
		dev_dbg(ippdrv->dev, "occured frame done at %d, status 0x%x.\n",
			ctx->id, status);
		gsc_write(GSC_IRQ_STATUS_OR_FRM_DONE, GSC_IRQ);

		if (!ipp_is_wb_cmd(property->cmd)) {
			buf_id[EXYNOS_DRM_OPS_SRC] = gsc_get_src_buf_index(ctx);
			if (buf_id[EXYNOS_DRM_OPS_SRC] < 0)
				return IRQ_HANDLED;
		}

		if (!ipp_is_output_cmd(property->cmd)) {
			buf_id[EXYNOS_DRM_OPS_DST] = gsc_get_dst_buf_index(ctx);
			if (buf_id[EXYNOS_DRM_OPS_DST] < 0)
				return IRQ_HANDLED;
		}

		DRM_DEBUG_KMS("%s:buf_id_src[%d]buf_id_dst[%d]\n", __func__,
			buf_id[EXYNOS_DRM_OPS_SRC], buf_id[EXYNOS_DRM_OPS_DST]);

		event->ippdrv = ippdrv;
		event->buf_id[EXYNOS_DRM_OPS_SRC] =
			buf_id[EXYNOS_DRM_OPS_SRC];
		event->buf_id[EXYNOS_DRM_OPS_DST] =
			buf_id[EXYNOS_DRM_OPS_DST];
		ippdrv->sched_event(event);
	}

	return IRQ_HANDLED;
}

static int gsc_init_capability(struct exynos_drm_ippdrv *ippdrv)
{
	struct drm_exynos_ipp_capability *capability;

	DRM_DEBUG_KMS("%s\n", __func__);

	capability = devm_kzalloc(ippdrv->dev, sizeof(*capability), GFP_KERNEL);
	if (!capability) {
		DRM_ERROR("failed to alloc capability.\n");
		return -ENOMEM;
	}

	capability->writeback = 1;
	capability->refresh_min = GSC_REFRESH_MIN;
	capability->refresh_max = GSC_REFRESH_MAX;
	capability->flip = (1 << EXYNOS_DRM_FLIP_VERTICAL) |
				(1 << EXYNOS_DRM_FLIP_HORIZONTAL);
	capability->degree = (1 << EXYNOS_DRM_DEGREE_0) |
				(1 << EXYNOS_DRM_DEGREE_90) |
				(1 << EXYNOS_DRM_DEGREE_180) |
				(1 << EXYNOS_DRM_DEGREE_270);
	capability->csc = 1;
	capability->crop = 1;
	capability->crop_max.hsize = GSC_CROP_MAX;
	capability->crop_max.vsize = GSC_CROP_MAX;
	capability->crop_min.hsize = GSC_CROP_MIN;
	capability->crop_min.vsize = GSC_CROP_MIN;
	capability->scale = 1;
	capability->scale_max.hsize = GSC_SCALE_MAX;
	capability->scale_max.vsize = GSC_SCALE_MAX;
	capability->scale_min.hsize = GSC_SCALE_MIN;
	capability->scale_min.vsize = GSC_SCALE_MIN;

	ippdrv->capability = capability;

	return 0;
}

static inline bool gsc_check_drm_flip(enum drm_exynos_flip flip)
{
	switch (flip) {
	case EXYNOS_DRM_FLIP_NONE:
	case EXYNOS_DRM_FLIP_VERTICAL:
	case EXYNOS_DRM_FLIP_HORIZONTAL:
	case EXYNOS_DRM_FLIP_BOTH:
		return true;
	default:
		DRM_DEBUG_KMS("%s:invalid flip\n", __func__);
		return false;
	}
}

static int gsc_ippdrv_check_property(struct device *dev,
		struct drm_exynos_ipp_property *property)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_capability *cp = ippdrv->capability;
	struct drm_exynos_ipp_config *config;
	struct drm_exynos_pos *pos;
	struct drm_exynos_sz *sz;
	bool swap;
	int i;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (property->blending ||
		property->dithering ||
		property->color_fill)
		return -EPERM;

	for_each_ipp_ops(i) {
		if ((i == EXYNOS_DRM_OPS_SRC) &&
			(property->cmd == IPP_CMD_WB))
			continue;

		config = &property->config[i];
		pos = &config->pos;
		sz = &config->sz;

		/* check for flip */
		if (!gsc_check_drm_flip(config->flip)) {
			DRM_ERROR("invalid flip.\n");
			goto err_property;
		}

		/* check for degree */
		switch (config->degree) {
		case EXYNOS_DRM_DEGREE_90:
		case EXYNOS_DRM_DEGREE_270:
			swap = true;
			break;
		case EXYNOS_DRM_DEGREE_0:
		case EXYNOS_DRM_DEGREE_180:
			swap = false;
			break;
		default:
			DRM_ERROR("invalid degree.\n");
			goto err_property;
		}

		/* check for buffer bound */
		if ((pos->x + pos->w > sz->hsize) ||
			(pos->y + pos->h > sz->vsize)) {
			DRM_ERROR("out of buf bound.\n");
			goto err_property;
		}

		/* check for crop */
		if ((i == EXYNOS_DRM_OPS_SRC) && (cp->crop)) {
			if (swap) {
				if ((pos->h < cp->crop_min.hsize) ||
					(sz->vsize > cp->crop_max.hsize) ||
					(pos->w < cp->crop_min.vsize) ||
					(sz->hsize > cp->crop_max.vsize)) {
					DRM_ERROR("out of crop size.\n");
					goto err_property;
				}
			} else {
				if ((pos->w < cp->crop_min.hsize) ||
					(sz->hsize > cp->crop_max.hsize) ||
					(pos->h < cp->crop_min.vsize) ||
					(sz->vsize > cp->crop_max.vsize)) {
					DRM_ERROR("out of crop size.\n");
					goto err_property;
				}
			}
		}

		/* check for scale */
		if ((i == EXYNOS_DRM_OPS_DST) && (cp->scale)) {
			if (swap) {
				if ((pos->h < cp->scale_min.hsize) ||
					(sz->vsize > cp->scale_max.hsize) ||
					(pos->w < cp->scale_min.vsize) ||
					(sz->hsize > cp->scale_max.vsize)) {
					DRM_ERROR("out of scale size.\n");
					goto err_property;
				}
			} else {
				if ((pos->w < cp->scale_min.hsize) ||
					(sz->hsize > cp->scale_max.hsize) ||
					(pos->h < cp->scale_min.vsize) ||
					(sz->vsize > cp->scale_max.vsize)) {
					DRM_ERROR("out of scale size.\n");
					goto err_property;
				}
			}
		}
	}

	return 0;

err_property:
	for_each_ipp_ops(i) {
		if ((i == EXYNOS_DRM_OPS_SRC) &&
			(property->cmd == IPP_CMD_WB))
			continue;

		config = &property->config[i];
		pos = &config->pos;
		sz = &config->sz;

		DRM_ERROR("[%s]f[%d]r[%d]pos[%d %d %d %d]sz[%d %d]\n",
			i ? "dst" : "src", config->flip, config->degree,
			pos->x, pos->y, pos->w, pos->h,
			sz->hsize, sz->vsize);
	}

	return -EINVAL;
}


static int gsc_ippdrv_reset(struct device *dev)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct gsc_scaler *sc = &ctx->sc;
	int ret;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* reset h/w block */
	ret = gsc_sw_reset(ctx);
	if (ret < 0) {
		dev_err(dev, "failed to reset hardware.\n");
		return ret;
	}

	/* scaler setting */
	memset(&ctx->sc, 0x0, sizeof(ctx->sc));
	sc->range = true;

	return 0;
}

static void gsc_send_output_event(struct gsc_context *ctx,
	struct drm_exynos_ipp_config *config)
{
	struct exynos_drm_overlay overlay;

	DRM_DEBUG_KMS("%s\n", __func__);

	memset(&overlay, 0, sizeof(overlay));

	if (config) {
		overlay.crtc_x = config->pos.x;
		overlay.crtc_y = config->pos.y;
		overlay.crtc_width = config->pos.w;
		overlay.crtc_height = config->pos.h;
		overlay.fb_width = config->sz.hsize;
		overlay.fb_height = config->sz.vsize;
		overlay.bpp = 24;
		overlay.activated = true;
	}

	overlay.zpos = ctx->id;
	overlay.local_path = true;

	exynos_drm_ippnb_send_event(IPP_SET_OUTPUT, (void *)&overlay);
}

static int gsc_ippdrv_start(struct device *dev, enum drm_exynos_ipp_cmd cmd)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property;
	struct drm_exynos_ipp_config *config;
	struct drm_exynos_pos	img_pos[EXYNOS_DRM_OPS_MAX];
	struct drm_exynos_ipp_set_wb set_wb;
	u32 cfg;
	int ret, i;

	DRM_DEBUG_KMS("%s:cmd[%d]\n", __func__, cmd);

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;

	gsc_handle_irq(ctx, true, false, true);

	for_each_ipp_ops(i) {
		config = &property->config[i];
		img_pos[i] = config->pos;
	}

	switch (cmd) {
	case IPP_CMD_M2M:
		/* enable one shot */
		cfg = gsc_read(GSC_ENABLE);
		cfg &= ~(GSC_ENABLE_ON_CLEAR_MASK |
			GSC_ENABLE_CLK_GATE_MODE_MASK);
		cfg |= GSC_ENABLE_ON_CLEAR_ONESHOT;
		gsc_write(cfg, GSC_ENABLE);

		/* src dma memory */
		cfg = gsc_read(GSC_IN_CON);
		cfg &= ~(GSC_IN_PATH_MASK | GSC_IN_LOCAL_SEL_MASK);
		cfg |= GSC_IN_PATH_MEMORY;
		gsc_write(cfg, GSC_IN_CON);

		/* dst dma memory */
		cfg = gsc_read(GSC_OUT_CON);
		cfg |= GSC_OUT_PATH_MEMORY;
		gsc_write(cfg, GSC_OUT_CON);
		break;
	case IPP_CMD_WB:
		set_wb.enable = 1;
		set_wb.refresh = property->refresh_rate;
		gsc_set_gscblk_fimd_wb(ctx, set_wb.enable);
		exynos_drm_ippnb_send_event(IPP_SET_WRITEBACK, (void *)&set_wb);

		/* src local path */
		cfg = gsc_read(GSC_IN_CON);
		cfg &= ~(GSC_IN_PATH_MASK | GSC_IN_LOCAL_SEL_MASK);
		cfg |= (GSC_IN_PATH_LOCAL | GSC_IN_LOCAL_FIMD_WB);
		gsc_write(cfg, GSC_IN_CON);

		/* dst dma memory */
		cfg = gsc_read(GSC_OUT_CON);
		cfg |= GSC_OUT_PATH_MEMORY;
		gsc_write(cfg, GSC_OUT_CON);
		break;
	case IPP_CMD_OUTPUT:
		gsc_set_local_output_path(ctx->id, GSC_FIMD, true);

		/* disable one shot */
		cfg = gsc_read(GSC_ENABLE);
		cfg &= ~(GSC_ENABLE_ON_CLEAR_MASK |
			GSC_ENABLE_CLK_GATE_MODE_MASK);
		cfg |= GSC_ENABLE_CLK_GATE_MODE_FREE;
		gsc_write(cfg, GSC_ENABLE);

		/* src dma memory */
		cfg = gsc_read(GSC_IN_CON);
		cfg &= ~(GSC_IN_PATH_MASK | GSC_IN_LOCAL_SEL_MASK);
		cfg |= GSC_IN_PATH_MEMORY;
		gsc_write(cfg, GSC_IN_CON);

		/* dst local path */
		cfg = gsc_read(GSC_OUT_CON);
		cfg |= GSC_OUT_PATH_LOCAL;
		gsc_write(cfg, GSC_OUT_CON);

		/* send output event to FIMD */
		config = &property->config[EXYNOS_DRM_OPS_DST];
		gsc_send_output_event(ctx, config);
		break;
	default:
		ret = -EINVAL;
		dev_err(dev, "invalid operations.\n");
		return ret;
	}

	ret = gsc_set_prescaler(ctx, &ctx->sc,
		&img_pos[EXYNOS_DRM_OPS_SRC],
		&img_pos[EXYNOS_DRM_OPS_DST]);
	if (ret) {
		dev_err(dev, "failed to set precalser.\n");
		return ret;
	}

	gsc_set_scaler(ctx, &ctx->sc);

	cfg = gsc_read(GSC_ENABLE);
	cfg |= GSC_ENABLE_ON;
	gsc_write(cfg, GSC_ENABLE);

	return 0;
}

static void gsc_ippdrv_stop(struct device *dev, enum drm_exynos_ipp_cmd cmd)
{
	struct gsc_context *ctx = get_gsc_context(dev);
	struct drm_exynos_ipp_set_wb set_wb = {0, 0};
	u32 cfg;

	DRM_DEBUG_KMS("%s:cmd[%d]\n", __func__, cmd);

	switch (cmd) {
	case IPP_CMD_M2M:
		/* bypass */
		break;
	case IPP_CMD_WB:
		gsc_set_gscblk_fimd_wb(ctx, set_wb.enable);
		exynos_drm_ippnb_send_event(IPP_SET_WRITEBACK, (void *)&set_wb);
		break;
	case IPP_CMD_OUTPUT:
		gsc_set_local_output_path(ctx->id, GSC_FIMD, false);
		break;
	default:
		dev_err(dev, "invalid operations.\n");
		break;
	}

	gsc_handle_irq(ctx, false, false, true);

	/* reset sequence */
	gsc_write(0xff, GSC_OUT_BASE_ADDR_Y_MASK);
	gsc_write(0xff, GSC_OUT_BASE_ADDR_CB_MASK);
	gsc_write(0xff, GSC_OUT_BASE_ADDR_CR_MASK);

	cfg = gsc_read(GSC_ENABLE);
	cfg &= ~GSC_ENABLE_ON;
	gsc_write(cfg, GSC_ENABLE);
}

#ifdef CONFIG_DRM_EXYNOS_DBG
static int gsc_read_reg(struct gsc_context *ctx, char *buf)
{
	struct resource *res = ctx->regs_res;
	int i, pos = 0;
	u32 cfg;

	pos += sprintf(buf+pos, "0x%.8x | ", res->start);
	for (i = 1; i < GSC_MAX_REG + 1; i++) {
		cfg = gsc_read((i-1) * sizeof(u32));
		pos += sprintf(buf+pos, "0x%.8x ", cfg);
		if (i % 4 == 0)
			pos += sprintf(buf+pos, "\n0x%.8x | ",
				res->start + (i * sizeof(u32)));
	}

	pos += sprintf(buf+pos, "\n");

	return pos;
}

static ssize_t show_read_reg(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct gsc_context *ctx = get_gsc_context(dev);

	if (!ctx->regs) {
		dev_err(dev, "failed to get current register.\n");
		return -EINVAL;
	}

	return gsc_read_reg(ctx, buf);
}

static struct device_attribute device_attrs[] = {
	__ATTR(read_reg, S_IRUGO, show_read_reg, NULL),
};
#endif

static int gsc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gsc_context *ctx;
	struct resource *res;
	struct exynos_drm_ippdrv *ippdrv;
	struct gsc_driverdata *ddata;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ddata = (struct gsc_driverdata *)
		platform_get_device_id(pdev)->driver_data;

	/* clock control */
	ctx->gsc_clk = clk_get(dev, ddata->gsc_clk);
	if (IS_ERR(ctx->gsc_clk)) {
		dev_err(dev, "failed to get gsc clock.\n");
		return PTR_ERR(ctx->gsc_clk);
	}

	/* resource memory */
	ctx->regs_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctx->regs = devm_request_and_ioremap(dev, ctx->regs_res);
	if (!ctx->regs) {
		dev_err(dev, "failed to map registers.\n");
		return -ENXIO;
	}

	/* resource irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "failed to request irq resource.\n");
		return -ENOENT;
	}

	ctx->irq = res->start;
	ret = request_threaded_irq(ctx->irq, NULL, gsc_irq_handler,
		IRQF_ONESHOT, "drm_gsc", ctx);
	if (ret < 0) {
		dev_err(dev, "failed to request irq.\n");
		return ret;
	}

	/* context initailization */
	ctx->id = pdev->id;
	ctx->ddata = ddata;

	ippdrv = &ctx->ippdrv;
	ippdrv->dev = dev;
	ippdrv->ops[EXYNOS_DRM_OPS_SRC] = &gsc_src_ops;
	ippdrv->ops[EXYNOS_DRM_OPS_DST] = &gsc_dst_ops;
	ippdrv->check_property = gsc_ippdrv_check_property;
	ippdrv->reset = gsc_ippdrv_reset;
	ippdrv->start = gsc_ippdrv_start;
	ippdrv->stop = gsc_ippdrv_stop;
	ippdrv->pm_ctrl = gsc_clk_ctrl;
	ret = gsc_init_capability(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to init property list.\n");
		goto err_get_irq;
	}

	DRM_DEBUG_KMS("%s:id[%d]ippdrv[0x%x]\n", __func__, ctx->id,
		(int)ippdrv);

	mutex_init(&ctx->lock);
	platform_set_drvdata(pdev, ctx);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	ret = exynos_drm_ippdrv_register(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to register drm gsc device.\n");
		goto err_ippdrv_register;
	}

#ifdef CONFIG_DRM_EXYNOS_DBG
	exynos_drm_dbg_register(&pdev->dev, device_attrs,
			ARRAY_SIZE(device_attrs), ctx->regs);
#endif

	dev_info(&pdev->dev, "drm gsc registered successfully.\n");

	return 0;

err_ippdrv_register:
	devm_kfree(dev, ippdrv->capability);
	pm_runtime_disable(dev);
err_get_irq:
	free_irq(ctx->irq, ctx);
	return ret;
}

static int gsc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gsc_context *ctx = get_gsc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;

#ifdef CONFIG_DRM_EXYNOS_DBG
	exynos_drm_dbg_unregister(&pdev->dev, device_attrs);
#endif

	devm_kfree(dev, ippdrv->capability);
	exynos_drm_ippdrv_unregister(ippdrv);
	mutex_destroy(&ctx->lock);

	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);

	free_irq(ctx->irq, ctx);

	return 0;
}

static struct gsc_driverdata exynos_gsc_data = {
	.gsc_clk = "gscl",
};

static struct platform_device_id gsc_driver_ids[] = {
	{
		.name		= "exynos-gsc",
		.driver_data	= (unsigned long)&exynos_gsc_data,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, gsc_driver_ids);

struct platform_driver gsc_driver = {
	.probe		= gsc_probe,
	.remove		= gsc_remove,
	.id_table	= gsc_driver_ids,
	.driver		= {
		.name	= "exynos-drm-gsc",
		.owner	= THIS_MODULE,
	},
};
