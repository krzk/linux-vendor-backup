/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Jinyoung Jeon <jy0.jeon@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#include "drmP.h"
#include "drm_backlight.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <plat/map-base.h>

#include "regs-sc.h"
#include <drm/exynos_drm.h>
#include "exynos_drm_ipp.h"
#include "exynos_drm_sc.h"

#define DEBUG

/*
 * SC stands for SCaler and
 * supports image scaler/rotator and input/output DMA operations.
 * input DMA reads image data from the memory.
 * output DMA writes image data to memory.
 * SC supports image rotation and image effect functions.
 *
 * M2M operation : supports crop/scale/rotation/csc/blending/
 * dithering/color fill so on.
 * Memory ----> SC H/W ----> Memory.
 */

/*
 * TODO
 * 1. check suspend/resume api if needed.
 * 2. need to check use case platform_device_id.
 * 3. check src/dst size with, height.
 * 4. need to add supported list in prop_list.
 * 5. how can we fix different C src set_size.
 */

#define SC_MAX_DEVS	1
#define SC_MAX_SRC		8
#define SC_MAX_DST		32
#define SC_RESET_TIMEOUT	50
#define SC_CLK_RATE	166750000
#define SC_REG_SZ		32
#define SC_WIDTH_ITU_709	1280
#define SC_CROP_MAX		16384
#define SC_CROP_MIN		16
#define SC_SCALE_MAX	16384
#define SC_SCALE_MIN		16
#define SC_RATIO(x, y)	((65536 * x) / y)
#define SC_COEF_RATIO	7
#define SC_COEF_PHASE	9
#define SC_COEF_ATTR	16
#define SC_COEF_H_8T	8
#define SC_COEF_V_4T	4
#define SC_COEF_DEPTH	3
#define SC_UP_MAX		SC_RATIO(1, 16)
#define SC_DOWN_MIN		SC_RATIO(4, 1)
#define SC_DOWN_SWMIN		SC_RATIO(16, 1)

#define get_sc_context(dev)	platform_get_drvdata(to_platform_device(dev))
#define get_ctx_from_ippdrv(ippdrv)	container_of(ippdrv,\
		struct sc_context, ippdrv);
#define sc_read(offset)		readl(ctx->regs + (offset))
#define sc_write(cfg, offset)	writel(cfg, ctx->regs + (offset))

/* definition of csc type */
enum sc_csc_type {
	CSC_TYPE_NO,
	CSC_TYPE_Y2R,
	CSC_TYPE_R2Y,
};

enum sc_version {
	SC_VER_3250,
	SC_VER_5260,
	SC_VER_5410,
};

/*
 * A structure of scaler.
 *
 * @range: narrow, wide.
 * @hratio: the scaler's horizontal ratio.
 * @vratio: the scaler's vertical ratio.
 */
struct sc_scaler {
	bool	range;
	unsigned long hratio;
	unsigned long vratio;
};

/*
 * A structure of sc driver data.
 *
 * @aclk: name of system clock.
 * @pclk: name of SFR clock.
 * @e_version: specifies scaler version.
 */
struct sc_driverdata {
	char	*aclk;
	char	*pclk;
	enum sc_version e_version;
};

/*
 * A structure of sc context.
 *
 * @ippdrv: prepare initialization using ippdrv.
 * @regs_res: register resources.
 * @regs: memory mapped io registers.
 * @sc_clk: sc a clock.
 * @sc_clk: sc p clock.
 * @sc: scaler infomations.
 * @id: sc id.
 * @irq: irq number.
 * @cur_buf_id: id of current buffer.
 * @nplanar: number of planar.
 * @pre_multi: premultiplied format.
 * @rotation: supports rotation of src.
 */
struct sc_context {
	struct exynos_drm_ippdrv	ippdrv;
	struct resource	*regs_res;
	void __iomem	*regs;
	struct clk	*sc_clk;
	struct sc_scaler	sc;
	int	id;
	int	irq;
	int	cur_buf_id[EXYNOS_DRM_OPS_MAX];
	int	nplanar[EXYNOS_DRM_OPS_MAX];
	bool	pre_multi;
	bool	rotation;
};

/* Scaling coefficient value */
static const int sc_coef_8t[SC_COEF_RATIO][SC_COEF_ATTR][SC_COEF_H_8T] = {
	{
		/* 8:8  or zoom-in */
		{0, 0, 0, 0, 128, 0, 0, 0},
		{0, 1, -2, 7, 127, -6, 2, -1},
		{0, 1, -5, 16, 125, -12, 4, -1},
		{0, 2, -8, 25, 120, -15, 5, -1},
		{-1, 3, -10, 35, 114, -18, 6, -1},
		{-1, 4, -13, 46, 107, -20, 6, -1},
		{-1, 5, -16, 57, 99, -21, 7, -2},
		{-1, 5, -18, 68, 89, -20, 6, -1},
		{-1, 6, -20, 79, 79, -20, 6, -1},
		{-1, 6, -20, 89, 68, -18, 5, -1},
		{-2, 7, -21, 99, 57, -16, 5, -1},
		{-1, 6, -20, 107, 46, -13, 4, -1},
		{-1, 6, -18, 114, 35, -10, 3, -1},
		{-1, 5, -15, 120, 25, -8, 2, 0},
		{-1, 4, -12, 125, 16, -5, 1, 0},
		{-1, 2, -6, 127, 7, -2, 1, 0}
	}, {
		/* 8:7 Zoom-out */
		{0, 3, -8, 13,	111, 14, -8, 3},
		{-1, 3, -10, 21, 112, 7, -6, 2},
		{-1, 4, -12, 28, 110, 1, -4, 2},
		{-1, 4, -13, 36, 106, -3, -2, 1},
		{-1, 4, -15, 44, 103, -7, -1, 1},
		{-1, 4, -16, 53, 97, -11, 1, 1},
		{-1, 4, -16, 61, 91, -13, 2, 0},
		{-1, 4, -17, 69, 85, -15, 3, 0},
		{0, 3, -16, 77, 77, -16, 3, 0},
		{0, 3, -15, 85, 69, -17, 4, -1},
		{0, 2, -13, 91, 61, -16, 4, -1},
		{1, 1, -11, 97, 53, -16, 4, -1},
		{1, -1, -7, 103, 44, -15, 4, -1},
		{1, -2, -3, 106, 36, -13, 4, -1},
		{2, -4, 1, 110,	28, -12, 4, -1},
		{2, -6, 7, 112,	21, -10, 3, -1}
	}, {
		/* 8:6 Zoom-out */
		{0, 2, -11, 25, 96, 25, -11, 2},
		{0, 2, -12, 31, 96, 19, -10, 2},
		{0, 2, -12, 37, 94, 14, -9, 2},
		{0, 1, -12, 43, 92, 10, -8, 2},
		{0, 1, -12, 49, 90, 5, -7, 2},
		{1, 0, -12, 55, 86, 1, -5, 2},
		{1, -1, -11, 61, 82, -2, -4, 2},
		{1, -1, -9, 67, 77, -5, -3, 1},
		{1, -2, -7, 72, 72, -7, -2, 1},
		{1, -3, -5, 77, 67, -9, -1, 1},
		{2, -4, -2, 82, 61, -11, -1, 1},
		{2, -5, 1, 86, 55, -12, 0, 1},
		{2, -7, 5, 90, 49, -12, 1, 0},
		{2, -8, 10, 92, 43, -12, 1, 0},
		{2, -9, 14, 94, 37, -12, 2, 0},
		{2, -10, 19, 96, 31, -12, 2, 0}
	}, {
		/* 8:5 Zoom-out */
		{0, -1, -8, 33, 80, 33, -8, -1},
		{1, -2, -7, 37, 80, 28, -8, -1},
		{1, -2, -7, 41, 79, 24, -8, 0},
		{1, -3, -6, 46, 78, 20, -8, 0},
		{1, -3, -4, 50, 76, 16, -8, 0},
		{1, -4, -3, 54, 74, 13, -7, 0},
		{1, -5, -1, 58, 71, 10, -7, 1},
		{1, -5, 1, 62, 68, 6, -6, 1},
		{1, -6, 4, 65, 65, 4, -6, 1},
		{1, -6, 6, 68, 62, 1, -5, 1},
		{1, -7, 10, 71, 58, -1, -5, 1},
		{0, -7, 13, 74, 54, -3, -4, 1},
		{0, -8, 16, 76, 50, -4, -3, 1},
		{0, -8, 20, 78, 46, -6, -3, 1},
		{0, -8, 24, 79, 41, -7, -2, 1},
		{-1, -8, 28, 80, 37, -7, -2, 1}
	}, {
		/* 8:4 Zoom-out */
		{0, -3, 0, 35, 64, 35, 0, -3},
		{0, -3, 1, 38, 64, 32, -1, -3},
		{0, -3, 2, 41, 63, 29, -2, -2},
		{0, -4, 4, 43, 63, 27, -3, -2},
		{0, -4, 6, 46, 61, 24, -3, -2},
		{0, -4, 7, 49, 60, 21, -3, -2},
		{-1, -4, 9, 51, 59, 19, -4, -1},
		{-1, -4, 12, 53, 57, 16, -4, -1},
		{-1, -4, 14, 55, 55, 14, -4, -1},
		{-1, -4, 16, 57, 53, 12, -4, -1},
		{-1, -4, 19, 59, 51, 9, -4, -1},
		{-2, -3, 21, 60, 49, 7, -4, 0},
		{-2, -3, 24, 61, 46, 6, -4, 0},
		{-2, -3, 27, 63, 43, 4, -4, 0},
		{-2, -2, 29, 63, 41, 2, -3, 0},
		{-3, -1, 32, 64, 38, 1, -3, 0}
	}, {
		/* 8:3 Zoom-out */
		{0, -1, 8, 33, 48, 33, 8, -1},
		{-1, -1, 9, 35, 49, 31, 7, -1},
		{-1, -1, 10, 36, 49, 30, 6, -1},
		{-1, -1, 12, 38, 48, 28, 5, -1},
		{-1, 0, 13, 39, 48, 26, 4, -1},
		{-1, 0, 15, 41, 47, 24, 3, -1},
		{-1, 0, 16, 42, 47, 23, 2, -1},
		{-1, 1, 18, 43, 45, 21, 2, -1},
		{-1, 1, 19, 45, 45, 19, 1, -1},
		{-1, 2, 21, 45, 43, 18, 1, -1},
		{-1, 2, 23, 47, 42, 16, 0, -1},
		{-1, 3, 24, 47, 41, 15, 0, -1},
		{-1, 4, 26, 48, 39, 13, 0, -1},
		{-1, 5, 28, 48, 38, 12, -1, -1},
		{-1, 6, 30, 49, 36, 10, -1, -1},
		{-1, 7, 31, 49, 35, 9, -1, -1}
	}, {
		/* 8:2 Zoom-out */
		{0, 2, 13, 30, 38, 30, 13, 2},
		{0, 3, 14, 30, 38, 29, 12, 2},
		{0, 3, 15, 31, 38, 28, 11, 2},
		{0, 4, 16, 32, 38, 26, 10, 2},
		{0, 4, 17, 33, 37, 26, 10, 1},
		{0, 5, 18, 34, 37, 24, 9, 1},
		{0, 5, 19, 34, 37, 24, 8, 1},
		{1, 6, 20, 35, 36, 22, 7, 1},
		{1, 6, 21, 36, 36, 21, 6, 1},
		{1, 7, 22, 36, 35, 20, 6, 1},
		{1, 8, 24, 37, 34, 19, 5, 0},
		{1, 9, 24, 37, 34, 18, 5, 0},
		{1, 10, 26, 37, 33, 17, 4, 0},
		{2, 10, 26, 38, 32, 16, 4, 0},
		{2, 11, 28, 38, 31, 15, 3, 0},
		{2, 12, 29, 38, 30, 14, 3, 0}
	}
};

static const int sc_coef_4t[SC_COEF_RATIO][SC_COEF_ATTR][SC_COEF_V_4T] = {
	{
		/* 8:8  or zoom-in */
		{0, 0, 128, 0},
		{0, 5, 127, -4},
		{-1, 11, 124, -6},
		{-1, 19, 118, -8},
		{-2, 27, 111, -8},
		{-3, 37, 102, -8},
		{-4, 48, 92, -8},
		{-5, 59, 81, -7},
		{-6, 70, 70, -6},
		{-7, 81, 59, -5},
		{-8, 92, 48, -4},
		{-8, 102, 37, -3},
		{-8, 111, 27, -2},
		{-8, 118, 19, -1},
		{-6, 124, 11, -1},
		{-4, 127, 5, 0}
	}, {
		/* 8:7 Zoom-out  */
		{0, 8, 112, 8},
		{-1, 14, 111, 4},
		{-2, 20, 109, 1},
		{-2, 27, 105, -2},
		{-3, 34, 100, -3},
		{-3, 43, 93, -5},
		{-4, 51, 86, -5},
		{-4, 60, 77, -5},
		{-5, 69, 69, -5},
		{-5, 77, 60, -4},
		{-5, 86, 51, -4},
		{-5, 93, 43, -3},
		{-3, 100, 34, -3},
		{-2, 105, 27, -2},
		{1, 109, 20, -2},
		{4, 111, 14, -1}
	}, {
		/* 8:6 Zoom-out  */
		{0, 16, 96, 16},
			{-2, 21, 97, 12},
			{-2, 26, 96, 8},
			{-2, 32, 93, 5},
			{-2, 39, 89, 2},
			{-2, 46, 84, 0},
			{-3, 53, 79, -1},
			{-2, 59, 73, -2},
			{-2, 66, 66, -2},
			{-2, 73, 59, -2},
			{-1, 79, 53, -3},
			{0, 84, 46, -2},
			{2, 89, 39, -2},
			{5, 93, 32, -2},
			{8, 96, 26, -2},
			{12, 97, 21, -2}
	}, {
		/* 8:5 Zoom-out  */
		{0, 22, 84, 22},
			{-1, 26, 85, 18},
			{-1, 31, 84, 14},
			{-1, 36, 82, 11},
			{-1, 42, 79, 8},
			{-1, 47, 76, 6},
			{0, 52, 72, 4},
			{0, 58, 68, 2},
			{1, 63, 63, 1},
			{2, 68, 58, 0},
			{4, 72, 52, 0},
			{6, 76, 47, -1},
			{8, 79, 42, -1},
			{11, 82, 36, -1},
			{14, 84, 31, -1},
			{18, 85, 26, -1}
	}, {
		/* 8:4 Zoom-out  */
		{0, 26, 76, 26},
			{0, 30, 76, 22},
			{0, 34, 75, 19},
			{1, 38, 73, 16},
			{1, 43, 71, 13},
			{2, 47, 69, 10},
			{3, 51, 66, 8},
			{4, 55, 63, 6},
			{5, 59, 59, 5},
			{6, 63, 55, 4},
			{8, 66, 51, 3},
			{10, 69, 47, 2},
			{13, 71, 43, 1},
			{16, 73, 38, 1},
			{19, 75, 34, 0},
			{22, 76, 30, 0}
	}, {
		/* 8:3 Zoom-out */
		{0, 29, 70, 29},
		{2, 32, 68, 26},
		{2, 36, 67, 23},
		{3, 39, 66, 20},
		{3, 43, 65, 17},
		{4, 46, 63, 15},
		{5, 50, 61, 12},
		{7, 53, 58, 10},
		{8, 56, 56, 8},
		{10, 58, 53, 7},
		{12, 61, 50, 5},
		{15, 63, 46, 4},
		{17, 65, 43, 3},
		{20, 66, 39, 3},
		{23, 67, 36, 2},
		{26, 68, 32, 2}
	}, {
		/* 8:2 Zoom-out  */
		{0, 32, 64, 32},
		{3, 34, 63, 28},
		{4, 37, 62, 25},
		{4, 40, 62, 22},
		{5, 43, 61, 19},
		{6, 46, 59, 17},
		{7, 48, 58, 15},
		{9, 51, 55, 13},
		{11, 53, 53, 11},
		{13, 55, 51, 9},
		{15, 58, 48, 7},
		{17, 59, 46, 6},
		{19, 61, 43, 5},
		{22, 62, 40, 4},
		{25, 62, 37, 4},
		{28, 63, 34, 3}
	},
};

/*
 * A structure of csc table.
 *
 * @narrow_601: narrow 601.
 * @wide_601: wide 601.
 * @narrow_709: narrow 709.
 * @wide_709: wide 709.
 */
struct sc_csc_tab {
	int narrow_601[9];
	int wide_601[9];
	int narrow_709[9];
	int wide_709[9];
};

/* CSC(Color Space Conversion) coefficient value */
static struct sc_csc_tab sc_no_csc = {
	{ 0x200, 0x000, 0x000, 0x000, 0x200, 0x000, 0x000, 0x000, 0x200 },
};

static struct sc_csc_tab sc_y2r = {
	/* (0,1) 601 Narrow */
	{ 0x254, 0x000, 0x331, 0x254, 0xF38, 0xE60, 0x254, 0x409, 0x000 },
	/* (0,1) 601 Wide */
	{ 0x200, 0x000, 0x2BE, 0x200, 0xF54, 0xE9B, 0x200, 0x377, 0x000 },
	/* (0,1) 709 Narrow */
	{ 0x254, 0x000, 0x331, 0x254, 0xF38, 0xE60, 0x254, 0x409, 0x000 },
	/* (0,1) 709 Wide */
	{ 0x200, 0x000, 0x314, 0x200, 0xFA2, 0xF15, 0x200, 0x3A2, 0x000 },
};

static struct sc_csc_tab sc_r2y = {
	/* (1,0) 601 Narrow */
	{ 0x084, 0x102, 0x032, 0xFB4, 0xF6B, 0x0E1, 0x0E1, 0xF44, 0xFDC },
	/* (1,0) 601 Wide  */
	{ 0x099, 0x12D, 0x03A, 0xFA8, 0xF52, 0x106, 0x106, 0xF25, 0xFD6 },
	/* (1,0) 709 Narrow */
	{ 0x05E, 0x13A, 0x020, 0xFCC, 0xF53, 0x0E1, 0x0E1, 0xF34, 0xFEC },
	/* (1,0) 709 Wide */
	{ 0x06D, 0x16E, 0x025, 0xFC4, 0xF36, 0x106, 0x106, 0xF12, 0xFE8 },
};

static struct sc_csc_tab *sc_csc_list[] = {
	[CSC_TYPE_NO] = &sc_no_csc,
	[CSC_TYPE_Y2R] = &sc_y2r,
	[CSC_TYPE_R2Y] = &sc_r2y,
};

#define SC_BLENDING_INV_BIT_OFFSET	0x10

/* define of blending compare */
enum sc_blending_comp {
	ONE = 0,
	SRC_A,
	SRC_C,
	DST_A,
	INV_SA = 0x11,
	INV_SC,
	INV_DA,
	ZERO = 0xff,
};

/*
 * A structure of blending value.
 *
 * @src_color: source color.
 * @src_alpha: source alpha.
 * @dst_color: destination color.
 * @dst_alpha: destination alpha.
 */
struct sc_blending_val {
	u32 src_color;
	u32 src_alpha;
	u32 dst_color;
	u32 dst_alpha;
};

static struct sc_blending_val sc_blending_tbl[] = {
	/* Sc,	 Sa,	Dc,	Da */
	{ZERO,	 ZERO,	ZERO,	ZERO},		/* CLEAR */
	{ONE,	 ONE,	ZERO,	ZERO},		/* SRC */
	{ZERO,	 ZERO,	ONE,	ONE},		/* DST */
	{ONE,	 ONE,	INV_SA,	INV_SA},	/* SRC_OVER */
	{INV_DA, ONE,	ONE,	INV_SA},	/* DST_OVER */
	{DST_A,	 DST_A,	ZERO,	ZERO},		/* SRC_IN */
	{ZERO,	 ZERO,	SRC_A,	SRC_A},		/* DST_IN */
	{INV_DA, INV_DA, ZERO,	ZERO},		/* SRC_OUT */
	{ZERO,	 ZERO,	INV_SA,	INV_SA},	/* DST_OUT */
	{DST_A,	 ZERO,	INV_SA,	ONE},		/* SRC_ATOP */
	{INV_DA, ONE,	SRC_A,	ZERO},		/* DST_ATOP */
	{INV_DA, ONE,	INV_SA,	ONE},		/* XOR: need to WA */
	{INV_DA, ONE,	INV_SA,	INV_SA},	/* DARKEN */
	{INV_DA, ONE,	INV_SA,	INV_SA},	/* LIGHTEN */
	{INV_DA, ONE,	INV_SA,	INV_SA},	/* MULTIPLY */
	{ONE,	 ONE,	INV_SC,	INV_SA},	/* SCREEN */
	{ONE,	 ONE,	ONE,	ONE},		/* ADD */
};

static int sc_sw_reset(struct sc_context *ctx)
{
	u32 cfg;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* s/w reset */
	cfg = sc_read(SCALER_CFG);
	cfg |= SCALER_CFG_SOFT_RST;
	cfg &= ~((1 << 10) | (1 << 9));
	sc_write(cfg, SCALER_CFG);

	return 0;
}

static void sc_handle_irq(struct sc_context *ctx, bool enable)
{
	u32 cfg;

	DRM_DEBUG_KMS("%s:enable[%d]\n", __func__, enable);

	cfg = sc_read(SCALER_INT_EN);

	if (enable)
		cfg |= SCALER_INT_EN_FRAME_END;
	else
		cfg &= ~SCALER_INT_EN_FRAME_END;

	sc_write(cfg, SCALER_INT_EN);
}

static bool sc_check_rgb(u32 fmt)
{
	bool is_rgb = false;

	switch (fmt) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		is_rgb = true;
		break;
	default:
		break;
	}

	return is_rgb;
}

static int sc_set_planar_addr(struct drm_exynos_ipp_buf_info *buf_info,
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
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
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

static int sc_src_set_fmt_nplanar(struct sc_context *ctx, u32 fmt)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;

	DRM_DEBUG_KMS("%s:fmt[0x%x]\n", __func__, fmt);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
		ctx->nplanar[EXYNOS_DRM_OPS_SRC] = 1;
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV12MT:
		ctx->nplanar[EXYNOS_DRM_OPS_SRC] = 2;
		break;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		ctx->nplanar[EXYNOS_DRM_OPS_SRC] = 3;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid number of planar 0x%x.\n", fmt);
		return -EINVAL;
	}

	return 0;
}

static int sc_src_set_fmt(struct device *dev, u32 fmt)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:fmt[0x%x]\n", __func__, fmt);

	cfg = sc_read(SCALER_SRC_CFG);
	cfg &= ~(SCALER_CFG_TILE_EN|SCALER_CFG_FMT_MASK);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
		cfg |= SCALER_CFG_FMT_RGB565;
		break;
	case DRM_FORMAT_ARGB1555:
		cfg |= SCALER_CFG_FMT_ARGB1555;
		break;
	case DRM_FORMAT_ARGB4444:
		cfg |= SCALER_CFG_FMT_ARGB4444;
		break;
	case DRM_FORMAT_XRGB8888:
		cfg |= (SCALER_CFG_FMT_ARGB8888 | SCALER_CFG_FMT_P_ARGB8888);
		break;
	case DRM_FORMAT_ARGB8888:
		cfg |= SCALER_CFG_FMT_ARGB8888;
		break;
	case DRM_FORMAT_YUYV:
		cfg |= SCALER_CFG_FMT_YUYV;
		break;
	case DRM_FORMAT_YVYU:
		cfg |= SCALER_CFG_FMT_YVYU;
		break;
	case DRM_FORMAT_UYVY:
		cfg |= SCALER_CFG_FMT_UYVY;
		break;
	case DRM_FORMAT_NV21:
		cfg |= SCALER_CFG_FMT_YCRCB420_2P;
		break;
	case DRM_FORMAT_NV61:
		cfg |= SCALER_CFG_FMT_YCRCB422_2P;
		break;
	case DRM_FORMAT_NV12:
		cfg |= SCALER_CFG_FMT_YCBCR420_2P;
		break;
	case DRM_FORMAT_NV16:
		cfg |= SCALER_CFG_FMT_YCBCR422_2P;
		break;
	case DRM_FORMAT_NV12MT:
		cfg |= (SCALER_CFG_FMT_YCBCR420_2P | SCALER_CFG_TILE_EN);
		break;
	case DRM_FORMAT_YUV422:
		cfg |= SCALER_CFG_FMT_YCBCR422_3P;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		cfg |= SCALER_CFG_FMT_YCBCR420_3P;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_SRC_CFG);

	return sc_src_set_fmt_nplanar(ctx, fmt);
}

static int sc_src_set_transf(struct device *dev,
		enum drm_exynos_degree degree,
		enum drm_exynos_flip flip, bool *swap)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:degree[%d]flip[0x%x]\n", __func__,
			degree, flip);

	cfg = sc_read(SCALER_ROT_CFG);
	cfg &= ~(SCALER_FLIP_MASK | SCALER_ROT_MASK);

	switch (degree) {
	case EXYNOS_DRM_DEGREE_0:
		if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN;
		if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN;
		break;
	case EXYNOS_DRM_DEGREE_90:
		if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN | SCALER_ROT_270;
		else if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN | SCALER_ROT_270;
		else
			cfg |= SCALER_ROT_270;
		break;
	case EXYNOS_DRM_DEGREE_180:
		cfg |= SCALER_ROT_180;
		break;
	case EXYNOS_DRM_DEGREE_270:
		cfg |= SCALER_ROT_90;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_ROT_CFG);

	ctx->rotation = cfg & (SCALER_ROT_90 | SCALER_ROT_270) ? 1 : 0;
	*swap = ctx->rotation;

	return 0;
}

static int sc_src_set_size(struct device *dev, int swap,
		struct drm_exynos_pos *pos, struct drm_exynos_sz *sz)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct drm_exynos_pos img_pos = *pos;
	u32 cfg;

	if (swap) {
		img_pos.w = pos->h;
		img_pos.h = pos->w;
	}

	DRM_DEBUG_KMS("%s:x[%d]y[%d]w[%d]h[%d]\n",
			__func__, pos->x, pos->y, pos->w, pos->h);

	/* pixel offset */
	cfg = (SCALER_SRC_YX(pos->x) |
			SCALER_SRC_YY(pos->y));
	sc_write(cfg, SCALER_SRC_Y_POS);

	/* cropped size */
	cfg = (SCALER_SRC_W(pos->w) |
			SCALER_SRC_H(pos->h));
	sc_write(cfg, SCALER_SRC_WH);

	DRM_DEBUG_KMS("%s:swap[%d]hsize[%d]vsize[%d]\n",
			__func__, swap, sz->hsize, sz->vsize);

	/* span size */
	cfg = sc_read(SCALER_SRC_SPAN);
	cfg &= ~(SCALER_SRC_CSPAN_MASK |
			SCALER_SRC_YSPAN_MASK);

	cfg |= sz->hsize;

	if (ctx->nplanar[EXYNOS_DRM_OPS_SRC] == 2)
		cfg |= sz->hsize << 16;

	if (ctx->nplanar[EXYNOS_DRM_OPS_SRC] == 3)
		cfg |= (sz->hsize >> 1) << 16;

	sc_write(cfg, SCALER_SRC_SPAN);

	return 0;
}

static int sc_src_set_addr(struct device *dev,
		struct drm_exynos_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_exynos_ipp_buf_type buf_type)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property;
	struct drm_exynos_ipp_config *config;
	int ret;

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;
	if (!property) {
		DRM_ERROR("failed to get property.\n");
		return -EINVAL;
	}

	DRM_DEBUG_KMS("%s:prop_id[%d]buf_id[%d]buf_type[%d]\n", __func__,
			property->prop_id, buf_id, buf_type);

	/* Set current buf_id */
	ctx->cur_buf_id[EXYNOS_DRM_OPS_SRC] = buf_id;

	if (buf_id > SC_MAX_SRC) {
		dev_info(ippdrv->dev, "inavlid buf_id %d.\n", buf_id);
		return -ENOMEM;
	}

	/* address register set */
	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		config = &property->config[EXYNOS_DRM_OPS_SRC];
		ret = sc_set_planar_addr(buf_info, config->fmt, &config->sz);
		if (ret) {
			dev_err(dev, "failed to set plane src addr.\n");
			return ret;
		}

		sc_write(buf_info->base[EXYNOS_DRM_PLANAR_Y],
				SCALER_SRC_Y_BASE);
		sc_write(buf_info->base[EXYNOS_DRM_PLANAR_CB],
				SCALER_SRC_CB_BASE);
		sc_write(buf_info->base[EXYNOS_DRM_PLANAR_CR],
				SCALER_SRC_CR_BASE);

		break;
	case IPP_BUF_DEQUEUE:
	default:
		/* bypass */
		break;
	}

	return 0;
}

static struct exynos_drm_ipp_ops sc_src_ops = {
	.set_fmt = sc_src_set_fmt,
	.set_transf = sc_src_set_transf,
	.set_size = sc_src_set_size,
	.set_addr = sc_src_set_addr,
};

static int sc_dst_set_fmt_nplanar(struct sc_context *ctx, u32 fmt)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:fmt[0x%x]\n", __func__, fmt);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		cfg = sc_read(SCALER_CFG);
		cfg &= ~SCALER_CFG_CSC_Y_OFFSET_DST;
		sc_write(cfg, SCALER_CFG);
		ctx->nplanar[EXYNOS_DRM_OPS_DST] = 1;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
		ctx->nplanar[EXYNOS_DRM_OPS_DST] = 1;
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV12MT:
		ctx->nplanar[EXYNOS_DRM_OPS_DST] = 2;
		break;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		ctx->nplanar[EXYNOS_DRM_OPS_DST] = 3;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	return 0;
}

static int sc_dst_set_fmt(struct device *dev, u32 fmt)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:fmt[0x%x]\n", __func__, fmt);

	cfg = sc_read(SCALER_DST_CFG);
	cfg &= ~(SCALER_CFG_SWAP_MASK|SCALER_CFG_FMT_MASK);

	switch (fmt) {
	case DRM_FORMAT_RGB565:
		cfg |= SCALER_CFG_FMT_RGB565;
		break;
	case DRM_FORMAT_ARGB1555:
		cfg |= SCALER_CFG_FMT_ARGB1555;
		break;
	case DRM_FORMAT_ARGB4444:
		cfg |= SCALER_CFG_FMT_ARGB4444;
		break;
	case DRM_FORMAT_XRGB8888:
		cfg |= (SCALER_CFG_FMT_ARGB8888 | SCALER_CFG_FMT_P_ARGB8888);
		ctx->pre_multi = true;
		break;
	case DRM_FORMAT_ARGB8888:
		cfg |= SCALER_CFG_FMT_ARGB8888;
		break;
	case DRM_FORMAT_YUYV:
		cfg |= SCALER_CFG_FMT_YUYV;
		break;
	case DRM_FORMAT_YVYU:
		cfg |= SCALER_CFG_FMT_YVYU;
		break;
	case DRM_FORMAT_UYVY:
		cfg |= SCALER_CFG_FMT_UYVY;
		break;
	case DRM_FORMAT_NV21:
		cfg |= SCALER_CFG_FMT_YCRCB420_2P;
		break;
	case DRM_FORMAT_NV61:
		cfg |= SCALER_CFG_FMT_YCRCB422_2P;
		break;
	case DRM_FORMAT_NV12:
		cfg |= SCALER_CFG_FMT_YCBCR420_2P;
		break;
	case DRM_FORMAT_NV16:
		cfg |= SCALER_CFG_FMT_YCBCR422_2P;
		break;
	case DRM_FORMAT_NV12MT:
		cfg |= (SCALER_CFG_FMT_YCBCR420_2P | SCALER_CFG_TILE_EN);
		break;
	case DRM_FORMAT_YUV422:
		cfg |= SCALER_CFG_FMT_YCBCR422_3P;
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		cfg |= SCALER_CFG_FMT_YCBCR420_3P;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid target yuv order 0x%x.\n", fmt);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_DST_CFG);

	return sc_dst_set_fmt_nplanar(ctx, fmt);
}

static int sc_dst_set_transf(struct device *dev,
		enum drm_exynos_degree degree,
		enum drm_exynos_flip flip, bool *swap)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;

	DRM_DEBUG_KMS("%s:degree[%d]flip[0x%x]\n", __func__, degree, flip);

	cfg = sc_read(SCALER_ROT_CFG);
	cfg &= ~(SCALER_FLIP_MASK | SCALER_ROT_MASK);

	switch (degree) {
	case EXYNOS_DRM_DEGREE_0:
		if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN;
		if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN;
		break;
	case EXYNOS_DRM_DEGREE_90:
		if (flip & EXYNOS_DRM_FLIP_HORIZONTAL)
			cfg |= SCALER_FLIP_X_EN | SCALER_ROT_270;
		else if (flip & EXYNOS_DRM_FLIP_VERTICAL)
			cfg |= SCALER_FLIP_Y_EN | SCALER_ROT_270;
		else
			cfg |= SCALER_ROT_270;
		break;
	case EXYNOS_DRM_DEGREE_180:
		cfg |= SCALER_ROT_180;
		break;
	case EXYNOS_DRM_DEGREE_270:
		cfg |= SCALER_ROT_90;
		break;
	default:
		dev_err(ippdrv->dev, "inavlid degree value %d.\n", degree);
		return -EINVAL;
	}

	sc_write(cfg, SCALER_ROT_CFG);

	ctx->rotation = cfg &
		(SCALER_ROT_90 | SCALER_ROT_270) ? 1 : 0;

	*swap = ctx->rotation;

	return 0;
}

static int sc_set_csc_coef(struct sc_context *ctx, struct sc_scaler *sc,
		u32 *fmt, int width)
{
	enum sc_csc_type csc_type = CSC_TYPE_NO;
	int *csc_eq_val, i, j;
	u32 cfg;
	bool itu_709;

	DRM_DEBUG_KMS("%s:sfmt[0x%x]dfmt[0x%x]range[%d]width[%d]\n", __func__,
			fmt[EXYNOS_DRM_OPS_SRC], fmt[EXYNOS_DRM_OPS_DST],
			sc->range, width);

	if (fmt[EXYNOS_DRM_OPS_SRC] == fmt[EXYNOS_DRM_OPS_DST]) {
		csc_eq_val = sc_csc_list[csc_type]->narrow_601;
	} else {
		if (width >= SC_WIDTH_ITU_709)
			itu_709 = true;
		else
			itu_709 = false;

		cfg = sc_read(SCALER_CFG);
		if (sc_check_rgb(fmt[EXYNOS_DRM_OPS_DST])) {
			csc_type = CSC_TYPE_Y2R;
			if (sc->range)
				cfg &= ~SCALER_CFG_CSC_Y_OFFSET_SRC;
			else
				cfg |= SCALER_CFG_CSC_Y_OFFSET_SRC;
		} else {
			csc_type = CSC_TYPE_R2Y;
			if (sc->range)
				cfg |= SCALER_CFG_CSC_Y_OFFSET_DST;
			else
				cfg &= ~SCALER_CFG_CSC_Y_OFFSET_DST;
		}
		sc_write(cfg, SCALER_CFG);

		if (itu_709) {
			if (sc->range)
				csc_eq_val = sc_csc_list[csc_type]->wide_709;
			else
				csc_eq_val = sc_csc_list[csc_type]->narrow_709;
		} else {
			if (sc->range)
				csc_eq_val = sc_csc_list[csc_type]->wide_601;
			else
				csc_eq_val = sc_csc_list[csc_type]->narrow_601;
		}
	}

	for (i = 0, j = 0; i < SC_COEF_PHASE; i++, j += 4) {
		cfg = sc_read(SCALER_CSC_COEF00 + j);
		cfg &= ~SCALER_CSC_COEF_MASK;
		cfg |= csc_eq_val[i];
		sc_write(cfg, SCALER_CSC_COEF00 + j);
	}

	return 0;
}

static void sc_set_scaler_ratio(struct sc_context *ctx, struct sc_scaler *sc)
{
	u32 cfg;

	DRM_DEBUG_KMS("%s:hratio[%ld]vratio[%ld]\n",
			__func__, sc->hratio, sc->vratio);

	cfg = sc_read(SCALER_H_RATIO);
	cfg &= ~SCALER_RATIO_MASK;
	cfg |= sc->hratio;
	sc_write(cfg, SCALER_H_RATIO);

	cfg = sc_read(SCALER_V_RATIO);
	cfg &= ~SCALER_RATIO_MASK;
	cfg |= sc->vratio;
	sc_write(cfg, SCALER_V_RATIO);
}

static void sc_set_h_coef(struct sc_context *ctx, int coef)
{
	u32 phase, tab, cnt = 0;
	u32 cfg, val_h, val_l;

	DRM_DEBUG_KMS("%s:coef[%d]\n", __func__, coef);

	for (phase = 0; phase < SC_COEF_PHASE; phase++) {
		for (tab = SC_COEF_H_8T; tab > 0; tab -= 2, cnt++) {
			val_h = sc_coef_8t[coef][phase][tab - 1] & 0x1FF;
			val_l = sc_coef_8t[coef][phase][tab - 2] & 0x1FF;
			cfg = (val_h << 16) | (val_l << 0);
			sc_write(cfg, SCALER_YHCOEF + cnt * 0x4);
			sc_write(cfg, SCALER_CHCOEF + cnt * 0x4);
		}
	}
}

static void sc_set_v_coef(struct sc_context *ctx, int coef)
{
	u32 phase, tab, cnt = 0;
	u32 cfg, val_h, val_l;

	DRM_DEBUG_KMS("%s:coef[%d]\n", __func__, coef);

	for (phase = 0; phase < SC_COEF_PHASE; phase++) {
		for (tab = SC_COEF_V_4T; tab > 0; tab -= 2, cnt++) {
			val_h = sc_coef_4t[coef][phase][tab - 1] & 0x1FF;
			val_l = sc_coef_4t[coef][phase][tab - 2] & 0x1FF;
			cfg = (val_h << 16) | (val_l << 0);
			sc_write(cfg, SCALER_YVCOEF + cnt * 0x4);
			sc_write(cfg, SCALER_CVCOEF + cnt * 0x4);
		}
	}
}

static int sc_get_scale_filter(u32 ratio)
{
	int filter;

	if (ratio <= 65536)
		filter = 0;	/* 8:8 or zoom-in */
	else if (ratio <= 74898)
		filter = 1;	/* 8:7 zoom-out */
	else if (ratio <= 87381)
		filter = 2;	/* 8:6 zoom-out */
	else if (ratio <= 104857)
		filter = 3;	/* 8:5 zoom-out */
	else if (ratio <= 131072)
		filter = 4;	/* 8:4 zoom-out */
	else if (ratio <= 174762)
		filter = 5;	/* 8:3 zoom-out */
	else
		filter = 6;	/* 8:2 zoom-out */

	return filter;
}

static int sc_set_scaler_coef(struct sc_context *ctx, struct sc_scaler *sc)
{
	int hcoef, vcoef;

	DRM_DEBUG_KMS("%s\n", __func__);

	hcoef = sc_get_scale_filter(sc->hratio);
	vcoef = sc_get_scale_filter(sc->vratio);

	sc_set_h_coef(ctx, hcoef);
	sc_set_v_coef(ctx, vcoef);

	return 0;
}

static int sc_set_scaler(struct sc_context *ctx, struct sc_scaler *sc,
		struct drm_exynos_pos *src, struct drm_exynos_pos *dst)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (ctx->rotation) {
		sc->hratio = SC_RATIO(src->h, dst->w);
		sc->vratio = SC_RATIO(src->w, dst->h);
	} else{
		sc->hratio = SC_RATIO(src->w, dst->w);
		sc->vratio = SC_RATIO(src->h, dst->h);
	}

	DRM_DEBUG_KMS("%s:hratio[%ld]vratio[%ld]\n",
			__func__, sc->hratio, sc->vratio);

	sc_set_scaler_coef(ctx, &ctx->sc);
	sc_set_scaler_ratio(ctx, &ctx->sc);

	return 0;
}

static void sc_set_dithering(struct  sc_context *ctx,
		enum drm_exynos_ipp_dithering dithering)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg, val;

	DRM_DEBUG_KMS("%s:dithering[%d]\n", __func__, dithering);

	val = dithering;

	cfg = sc_read(SCALER_DITH_CFG);
	cfg &= ~(SCALER_DITH_R_MASK | SCALER_DITH_G_MASK | SCALER_DITH_B_MASK);
	cfg |= (val << SCALER_DITH_R_SHIFT) |
		(val << SCALER_DITH_G_SHIFT) |
		(val << SCALER_DITH_B_SHIFT);
	sc_write(cfg, SCALER_DITH_CFG);
}

static void sc_get_blending_value(u32 *cfg, u32 val, bool pre_multi)
{
	u32 tmp;

	*cfg &= ~(SCALER_SEL_INV_MASK | SCALER_SEL_MASK |
			SCALER_OP_SEL_INV_MASK | SCALER_OP_SEL_MASK);

	if (val == 0xff)
		*cfg |= (1 << SCALER_SEL_INV_SHIFT);
	else {
		if (pre_multi)
			*cfg |= (1 << SCALER_SEL_SHIFT);
		else
			*cfg |= (2 << SCALER_SEL_SHIFT);
		tmp = val & 0xf;
		*cfg |= (tmp << SCALER_OP_SEL_SHIFT);
	}

	if (val >= SC_BLENDING_INV_BIT_OFFSET)
		*cfg |= (1 << SCALER_OP_SEL_INV_SHIFT);
}

static void sc_set_blending(struct  sc_context *ctx,
		enum drm_exynos_ipp_blending blending, bool pre_multi)
{
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	u32 cfg;
	int idx = blending - 1;

	DRM_DEBUG_KMS("%s:blending[%d]\n", __func__, blending);

	if (blending == IPP_BLENDING_NO)
		return;

	if (blending >= IPP_BLENDING_MAX) {
		dev_err(ippdrv->dev, "invalid blending value %d.\n", blending);
		return;
	}

	cfg = sc_read(SCALER_CFG);
	cfg |= SCALER_CFG_BLEND_EN;
	sc_write(cfg, SCALER_CFG);

	cfg = sc_read(SCALER_SRC_BLEND_COLOR);
	sc_get_blending_value(&cfg, sc_blending_tbl[idx].src_color, pre_multi);
	sc_write(cfg, SCALER_SRC_BLEND_COLOR);

	cfg = sc_read(SCALER_SRC_BLEND_ALPHA);
	sc_get_blending_value(&cfg, sc_blending_tbl[idx].src_alpha, 1);
	sc_write(cfg, SCALER_SRC_BLEND_ALPHA);

	cfg = sc_read(SCALER_DST_BLEND_COLOR);
	sc_get_blending_value(&cfg, sc_blending_tbl[idx].dst_color, pre_multi);
	sc_write(cfg, SCALER_DST_BLEND_COLOR);

	cfg = sc_read(SCALER_DST_BLEND_ALPHA);
	sc_get_blending_value(&cfg, sc_blending_tbl[idx].dst_alpha, 1);
	sc_write(cfg, SCALER_DST_BLEND_ALPHA);

	/*
	 * If dst format is non-premultiplied format
	 * and blending operation is enabled,
	 * result image should be divided by alpha value
	 * because the result is always pre-multiplied.
	 */
	if (!pre_multi) {
		cfg = sc_read(SCALER_CFG);
		cfg |= SCALER_CFG_BL_DIV_ALPHA_EN;
		sc_write(cfg, SCALER_CFG);
	}
}

static void sc_set_color_fill(struct sc_context *ctx, u32 color)
{
	u32 cfg = sc_read(SCALER_CFG);

	cfg |= SCALER_CFG_FILL_EN;
	sc_write(cfg, SCALER_CFG);

	cfg = color;
	sc_write(cfg, SCALER_FILL_COLOR);

}

static int sc_dst_set_size(struct device *dev, int swap,
		struct drm_exynos_pos *pos, struct drm_exynos_sz *sz)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct drm_exynos_pos img_pos = *pos;
	u32 cfg;

	DRM_DEBUG_KMS("%s:swap[%d]x[%d]y[%d]w[%d]h[%d]\n",
			__func__, swap, pos->x, pos->y, pos->w, pos->h);

	if (swap) {
		img_pos.w = pos->h;
		img_pos.h = pos->w;
	}

	/* pixel offset */
	cfg = (SCALER_DST_X(pos->x) | SCALER_DST_Y(pos->y));
	sc_write(cfg, SCALER_DST_POS);

	/* scaled size */
	cfg = (SCALER_DST_W(pos->w) | SCALER_DST_H(pos->h));
	sc_write(cfg, SCALER_DST_WH);

	DRM_DEBUG_KMS("%s:hsize[%d]vsize[%d]\n",
			__func__, sz->hsize, sz->vsize);

	/* span size */
	cfg = sc_read(SCALER_DST_SPAN);
	cfg &= ~(SCALER_DST_CSPAN_MASK | SCALER_DST_YSPAN_MASK);

	cfg |= sz->hsize;

	if (ctx->nplanar[EXYNOS_DRM_OPS_DST] == 2)
		cfg |= sz->hsize << 16;

	if (ctx->nplanar[EXYNOS_DRM_OPS_DST] == 3)
		cfg |= (sz->hsize >> 1) << 16;

	sc_write(cfg, SCALER_DST_SPAN);

	return 0;
}

static int sc_dst_set_addr(struct device *dev,
		struct drm_exynos_ipp_buf_info *buf_info, u32 buf_id,
		enum drm_exynos_ipp_buf_type buf_type)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property;
	struct drm_exynos_ipp_config *config;
	int ret;

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;

	DRM_DEBUG_KMS("%s:prop_id[%d]buf_id[%d]buf_type[%d]\n", __func__,
			property->prop_id, buf_id, buf_type);

	/* Set current buf_id */
	ctx->cur_buf_id[EXYNOS_DRM_OPS_DST] = buf_id;

	/* address register set */
	switch (buf_type) {
	case IPP_BUF_ENQUEUE:
		config = &property->config[EXYNOS_DRM_OPS_DST];
		ret = sc_set_planar_addr(buf_info, config->fmt, &config->sz);
		if (ret) {
			dev_err(dev, "failed to set plane src addr.\n");
			return ret;
		}

		sc_write(buf_info->base[EXYNOS_DRM_PLANAR_Y],
				SCALER_DST_Y_BASE);
		sc_write(buf_info->base[EXYNOS_DRM_PLANAR_CB],
				SCALER_DST_CB_BASE);
		sc_write(buf_info->base[EXYNOS_DRM_PLANAR_CR],
				SCALER_DST_CR_BASE);
		break;
	case IPP_BUF_DEQUEUE:
	default:
		/* bypass */
		break;
	}

	return 0;
}

static struct exynos_drm_ipp_ops sc_dst_ops = {
	.set_fmt = sc_dst_set_fmt,
	.set_transf = sc_dst_set_transf,
	.set_size = sc_dst_set_size,
	.set_addr = sc_dst_set_addr,
};

static int sc_clk_ctrl(struct sc_context *ctx, bool enable)
{
	int ret = 0;

	DRM_DEBUG_KMS("%s:enable[%d]\n", __func__, enable);

	if (enable)
		ret = clk_enable(ctx->sc_clk);
	else
		clk_disable(ctx->sc_clk);

	return ret;
}

static irqreturn_t sc_irq_handler(int irq, void *dev_id)
{
	struct sc_context *ctx = dev_id;
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property = &c_node->property;
	struct drm_exynos_ipp_event_work *event = c_node->event_work;
	u32 cfg;
	int *buf_id = ctx->cur_buf_id;

	DRM_DEBUG_KMS("%s:sc id[%d]\n", __func__, ctx->id);

	cfg = sc_read(SCALER_INT_STATUS);
	cfg |= SCALER_INT_STATUS_FRAME_END;
	sc_write(cfg, SCALER_INT_STATUS);

	if (c_node->state == IPP_STATE_STOP) {
		DRM_ERROR("invalid state:prop_id[%d]\n", property->prop_id);
		return IRQ_HANDLED;
	}

	DRM_DEBUG_KMS("%s:src buf_id[%d]dst buf_id[%d]\n", __func__,
			buf_id[EXYNOS_DRM_OPS_SRC], buf_id[EXYNOS_DRM_OPS_DST]);

	event->ippdrv = ippdrv;
	event->buf_id[EXYNOS_DRM_OPS_SRC] = buf_id[EXYNOS_DRM_OPS_SRC];
	event->buf_id[EXYNOS_DRM_OPS_DST] = buf_id[EXYNOS_DRM_OPS_DST];
	ippdrv->sched_event(&event->work);

	return IRQ_HANDLED;
}

static int sc_init_prop_list(struct exynos_drm_ippdrv *ippdrv)
{
	struct drm_exynos_ipp_prop_list *prop_list;

	DRM_DEBUG_KMS("%s\n", __func__);

	prop_list = devm_kzalloc(ippdrv->dev, sizeof(*prop_list), GFP_KERNEL);
	if (!prop_list) {
		DRM_ERROR("failed to alloc prop_list.\n");
		return -ENOMEM;
	}

	prop_list->flip = (1 << EXYNOS_DRM_FLIP_VERTICAL) |
		(1 << EXYNOS_DRM_FLIP_HORIZONTAL);
	prop_list->degree = (1 << EXYNOS_DRM_DEGREE_0) |
		(1 << EXYNOS_DRM_DEGREE_90) |
		(1 << EXYNOS_DRM_DEGREE_180) |
		(1 << EXYNOS_DRM_DEGREE_270);
	prop_list->csc = 1;
	prop_list->crop = 1;
	prop_list->crop_max.hsize = SC_CROP_MAX;
	prop_list->crop_max.vsize = SC_CROP_MAX;
	prop_list->crop_min.hsize = SC_CROP_MIN;
	prop_list->crop_min.vsize = SC_CROP_MIN;
	prop_list->scale = 1;
	prop_list->blending = 1;
	prop_list->dithering = 1;
	prop_list->colorfill = 1;
	prop_list->scale_max.hsize = SC_SCALE_MAX;
	prop_list->scale_max.vsize = SC_SCALE_MAX;
	prop_list->scale_min.hsize = SC_SCALE_MIN;
	prop_list->scale_min.vsize = SC_SCALE_MIN;

	ippdrv->prop_list = prop_list;

	return 0;
}

static inline bool sc_check_drm_flip(enum drm_exynos_flip flip)
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

static inline bool sc_check_fmt_limit(struct drm_exynos_ipp_property *property)
{
	struct drm_exynos_ipp_config *src_config =
		&property->config[EXYNOS_DRM_OPS_SRC];
	struct drm_exynos_ipp_config *dst_config =
		&property->config[EXYNOS_DRM_OPS_DST];
	struct drm_exynos_pos src_pos = src_config->pos;
	struct drm_exynos_pos dst_pos = dst_config->pos;
	struct drm_exynos_sz src_sz = src_config->sz;
	struct drm_exynos_sz dst_sz = dst_config->sz;
	unsigned int h_ratio, v_ratio, i;

	if (src_config->fmt == DRM_FORMAT_NV12MT ||
		dst_config->fmt == DRM_FORMAT_NV12MT) {
		if (src_config->degree != EXYNOS_DRM_DEGREE_0) {
			DRM_INFO("%s:not support rotating with tiled format\n",
					__func__);
			return false;
		}

		if ((src_pos.w != dst_pos.w) ||
			(src_sz.hsize != dst_sz.hsize) ||
			(src_pos.h != dst_pos.h) ||
			(src_sz.vsize != dst_sz.vsize)) {
			DRM_INFO("%s:not support scaling with tiled format\n",
					__func__);
			return false;
		}
	}

	for_each_ipp_ops(i) {
		if ((property->config[i].fmt == EXYNOS_DRM_DEGREE_90) ||
			(property->config[i].fmt == EXYNOS_DRM_DEGREE_270))
			swap(src_pos.w, src_pos.h);
	}

	h_ratio = SC_RATIO(src_pos.w, dst_pos.w);
	v_ratio = SC_RATIO(src_pos.h, dst_pos.h);

	if ((h_ratio > SC_DOWN_MIN) ||
		(h_ratio < SC_UP_MAX)) {
		DRM_INFO("%s:width scaling is out of range\n", __func__);
		return false;
	}

	if ((v_ratio > SC_DOWN_MIN) ||
		(v_ratio < SC_UP_MAX)) {
		DRM_INFO("%s:height scaling is out of range\n", __func__);
		return false;
	}

	return true;
}

static int sc_ippdrv_check_property(struct device *dev,
		struct drm_exynos_ipp_property *property)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_prop_list *cp = ippdrv->prop_list;
	struct drm_exynos_ipp_config *config;
	struct drm_exynos_pos *pos;
	struct drm_exynos_sz *sz;
	bool swap;
	int i;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (property->cmd != IPP_CMD_M2M)
		return -EPERM;

	if (!sc_check_fmt_limit(property))
		return -EPERM;

	for_each_ipp_ops(i) {
		config = &property->config[i];
		pos = &config->pos;
		sz = &config->sz;

		/* check for flip */
		if (!sc_check_drm_flip(config->flip)) {
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
		if ((i == EXYNOS_DRM_OPS_SRC) && (property->cmd == IPP_CMD_WB))
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


static int sc_ippdrv_reset(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct sc_scaler *sc = &ctx->sc;
	int ret;

	DRM_DEBUG_KMS("%s\n", __func__);

	/* reset h/w block */
	ret = sc_sw_reset(ctx);
	if (ret < 0) {
		dev_err(dev, "failed to reset hardware.\n");
		return ret;
	}

	/* scaler setting */
	memset(&ctx->sc, 0x0, sizeof(ctx->sc));
	sc->range = true;

	return 0;
}

static int sc_check_prepare(struct sc_context *ctx)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	return 0;
}

static int sc_ippdrv_start(struct device *dev, enum drm_exynos_ipp_cmd cmd)
{
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;
	struct drm_exynos_ipp_cmd_node *c_node = ippdrv->c_node;
	struct drm_exynos_ipp_property *property;
	struct drm_exynos_ipp_config *config;
	struct drm_exynos_pos img_pos[EXYNOS_DRM_OPS_MAX];
	u32 fmt[EXYNOS_DRM_OPS_MAX];
	u32 cfg;
	int ret, i;

	DRM_DEBUG_KMS("%s:cmd[%d]\n", __func__, cmd);

	if (!c_node) {
		DRM_ERROR("failed to get c_node.\n");
		return -EINVAL;
	}

	property = &c_node->property;

	ret = sc_check_prepare(ctx);
	if (ret) {
		dev_err(dev, "failed to check prepare.\n");
		return ret;
	}

	sc_handle_irq(ctx, true);

	for_each_ipp_ops(i) {
		config = &property->config[i];
		img_pos[i] = config->pos;
		fmt[i] = config->fmt;
	}

	switch (cmd) {
	case IPP_CMD_M2M:
		/* bypass */
		break;
	case IPP_CMD_WB:
	case IPP_CMD_OUTPUT:
	default:
		ret = -EINVAL;
		dev_err(dev, "invalid operations.\n");
		return ret;
	}

	/* set scaler */
	ret = sc_set_scaler(ctx, &ctx->sc,
			&img_pos[EXYNOS_DRM_OPS_SRC],
			&img_pos[EXYNOS_DRM_OPS_DST]);
	if (ret) {
		dev_err(dev, "failed to set precalser.\n");
		return ret;
	}

	/* set coefficient */
	sc_set_csc_coef(ctx, &ctx->sc, fmt, img_pos[EXYNOS_DRM_OPS_DST].w);

	/* set dithering */
	if (property->dithering)
		sc_set_dithering(ctx, property->dithering);

	/* set blending */
	if (property->blending)
		sc_set_blending(ctx, property->blending, ctx->pre_multi);

	/* color fill */
	if (property->color_fill)
		sc_set_color_fill(ctx, property->color_fill);

	cfg = sc_read(SCALER_CFG);
	cfg |= SCALER_CFG_START_CMD;
	sc_write(cfg, SCALER_CFG);

	return 0;
}

static void sc_ippdrv_stop(struct device *dev, enum drm_exynos_ipp_cmd cmd)
{
	struct sc_context *ctx = get_sc_context(dev);
	u32 cfg;

	DRM_DEBUG_KMS("%s:cmd[%d]\n", __func__, cmd);

	switch (cmd) {
	case IPP_CMD_M2M:
		/* bypass */
		break;
	case IPP_CMD_WB:
	case IPP_CMD_OUTPUT:
	default:
		dev_err(dev, "invalid operations.\n");
		break;
	}

	sc_handle_irq(ctx, false);

	cfg = sc_read(SCALER_CFG);
	cfg &= ~SCALER_CFG_START_CMD;
	sc_write(cfg, SCALER_CFG);
}

static int sc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sc_context *ctx;
	struct resource *res;
	struct exynos_drm_ippdrv *ippdrv;
	int ret = -EINVAL;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	/* clock control */
	ctx->sc_clk = devm_clk_get(dev, "mscl");
	if (IS_ERR(ctx->sc_clk)) {
		dev_err(dev, "failed to get sc clock.\n");
		return PTR_ERR(ctx->sc_clk);
	}

	/* resource memory */
	ctx->regs_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ctx->regs = devm_request_and_ioremap(dev, ctx->regs_res);
	if (!ctx->regs) {
		dev_err(dev, "failed to map registers.\n");
		return -EADDRNOTAVAIL;
	}

	/* resource irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "failed to request irq resource.\n");
		return -EINVAL;
	}

	ctx->irq = res->start;
	ret = devm_request_threaded_irq(dev, ctx->irq, NULL, sc_irq_handler,
			IRQF_ONESHOT, "drm_sc", ctx);
	if (ret < 0) {
		dev_err(dev, "failed to request irq.\n");
		return ret;
	}

	/* context initailization */
	ctx->id = pdev->id;

	/* ToDo: iommu enable */
	ippdrv = &ctx->ippdrv;
	ippdrv->dev = dev;
	ippdrv->ops[EXYNOS_DRM_OPS_SRC] = &sc_src_ops;
	ippdrv->ops[EXYNOS_DRM_OPS_DST] = &sc_dst_ops;
	ippdrv->check_property = sc_ippdrv_check_property;
	ippdrv->reset = sc_ippdrv_reset;
	ippdrv->start = sc_ippdrv_start;
	ippdrv->stop = sc_ippdrv_stop;
	ret = sc_init_prop_list(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to init property list.\n");
		return ret;
	}

	DRM_INFO("%s:id[%d]ippdrv[0x%x]\n", __func__, ctx->id, (int)ippdrv);

	platform_set_drvdata(pdev, ctx);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	ret = exynos_drm_ippdrv_register(ippdrv);
	if (ret < 0) {
		dev_err(dev, "failed to register drm sc device.\n");
		goto err_ippdrv_register;
	}

	dev_info(&pdev->dev, "drm sc registered successfully.\n");

	return 0;

err_ippdrv_register:
	pm_runtime_disable(dev);
	return ret;
}

static int sc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sc_context *ctx = get_sc_context(dev);
	struct exynos_drm_ippdrv *ippdrv = &ctx->ippdrv;

	exynos_drm_ippdrv_unregister(ippdrv);

	pm_runtime_set_suspended(dev);
	pm_runtime_disable(dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sc_suspend(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_DEBUG_KMS("%s:id[%d]\n", __func__, ctx->id);

	if (pm_runtime_suspended(dev))
		return 0;

	return sc_clk_ctrl(ctx, false);
}

static int sc_resume(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_DEBUG_KMS("%s:id[%d]\n", __func__, ctx->id);

	if (!pm_runtime_suspended(dev))
		return sc_clk_ctrl(ctx, true);

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sc_runtime_suspend(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_DEBUG_KMS("%s:id[%d]\n", __func__, ctx->id);

	return sc_clk_ctrl(ctx, false);
}

static int sc_runtime_resume(struct device *dev)
{
	struct sc_context *ctx = get_sc_context(dev);

	DRM_DEBUG_KMS("id[%d]\n", ctx->id);

	return sc_clk_ctrl(ctx, true);
}
#endif

static const struct dev_pm_ops sc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc_suspend, sc_resume)
		SET_RUNTIME_PM_OPS(sc_runtime_suspend, sc_runtime_resume, NULL)
};

static const struct of_device_id exynos_drm_sc_of_match[] = {
	{ .compatible = "samsung,exynos3250-sc" },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos_drm_sc_of_match);

struct platform_driver sc_driver = {
	.probe	= sc_probe,
	.remove	= sc_remove,
	.driver	= {
		.name	= "exynos-drm-sc",
		.owner	= THIS_MODULE,
		.pm	= &sc_pm_ops,
		.of_match_table	= of_match_ptr(exynos_drm_sc_of_match),
	},
};

