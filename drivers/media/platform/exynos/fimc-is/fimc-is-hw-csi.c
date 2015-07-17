/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>


#include "fimc-is-config.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "fimc-is-hw.h"

/* CSIS global control */
#define S5PCSIS_CTRL					(0x00)
#define S5PCSIS_CTRL_DPDN_SWAP_CLOCK_DEFAULT		(0 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP_CLOCK			(1 << 31)
#define S5PCSIS_CTRL_DPDN_SWAP_DATA_DEFAULT		(0 << 30)
#define S5PCSIS_CTRL_DPDN_SWAP_DATA			(1 << 30)
#define S5PCSIS_CTRL_INTERLEAVE_MODE(x)			((x & 0x3) << 22)
#define S5PCSIS_CTRL_ALIGN_32BIT			(1 << 20)
#define S5PCSIS_CTRL_UPDATE_SHADOW(x)			((1 << (x)) << 16)
#define S5PCSIS_CTRL_WCLK_EXTCLK			(1 << 8)
#define S5PCSIS_CTRL_RESET				(1 << 4)
#define S5PCSIS_CTRL_NUMOFDATALANE(x)			((x) << 2)
#define S5PCSIS_CTRL_ENABLE				(1 << 0)

/* D-PHY control */
#define S5PCSIS_DPHYCTRL				(0x04)
#define S5PCSIS_DPHYCTRL_DPHY_ON(lanes)			((~(0x1f << (lanes + 1))) & 0x1f)
#define S5PCSIS_DPHYCTRL_HSS_MASK			(0xff << 24)
#define S5PCSIS_DPHYCTRL_CLKSETTLEMASK			(0x3 << 22)

/* Configuration */
#define S5PCSIS_CONFIG					(0x08)
#define S5PCSIS_CONFIG_CH1				(0x40)
#define S5PCSIS_CONFIG_CH2				(0x50)
#define S5PCSIS_CONFIG_CH3				(0x60)
#define S5PCSIS_CFG_LINE_INTERVAL(x)			(x << 26)
#define S5PCSIS_CFG_START_INTERVAL(x)			(x << 20)
#define S5PCSIS_CFG_END_INTERVAL(x)			(x << 8)
#define S5PCSIS_CFG_FMT_YCBCR422_8BIT			(0x1e << 2)
#define S5PCSIS_CFG_FMT_RAW8				(0x2a << 2)
#define S5PCSIS_CFG_FMT_RAW10				(0x2b << 2)
#define S5PCSIS_CFG_FMT_RAW12				(0x2c << 2)
/* User defined formats, x = 1...4 */
#define S5PCSIS_CFG_FMT_USER(x)				((0x30 + x - 1) << 2)
#define S5PCSIS_CFG_FMT_MASK				(0x3f << 2)
#define S5PCSIS_CFG_VIRTUAL_CH(x)			(x << 0)
#define S5PCSIS_CFG_NR_LANE_MASK			(3)

/* Interrupt mask. */
#define S5PCSIS_INTMSK					(0x10)
#define S5PCSIS_INTMSK_EN_ALL				(0xf1101117)
#define S5PCSIS_INTMSK_EVEN_BEFORE			(1 << 31)
#define S5PCSIS_INTMSK_EVEN_AFTER			(1 << 30)
#define S5PCSIS_INTMSK_ODD_BEFORE			(1 << 29)
#define S5PCSIS_INTMSK_ODD_AFTER			(1 << 28)
#define S5PCSIS_INTMSK_FRAME_START_CH3			(1 << 27)
#define S5PCSIS_INTMSK_FRAME_START_CH2			(1 << 26)
#define S5PCSIS_INTMSK_FRAME_START_CH1			(1 << 25)
#define S5PCSIS_INTMSK_FRAME_START_CH0			(1 << 24)
#define S5PCSIS_INTMSK_FRAME_END_CH3			(1 << 23)
#define S5PCSIS_INTMSK_FRAME_END_CH2			(1 << 22)
#define S5PCSIS_INTMSK_FRAME_END_CH1			(1 << 21)
#define S5PCSIS_INTMSK_FRAME_END_CH0			(1 << 20)
#define S5PCSIS_INTMSK_ERR_SOT_HS			(1 << 16)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH3			(1 << 15)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH2			(1 << 14)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH1			(1 << 13)
#define S5PCSIS_INTMSK_ERR_LOST_FS_CH0			(1 << 12)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH3			(1 << 11)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH2			(1 << 10)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH1			(1 << 9)
#define S5PCSIS_INTMSK_ERR_LOST_FE_CH0			(1 << 8)
#define S5PCSIS_INTMSK_ERR_OVER_CH3			(1 << 7)
#define S5PCSIS_INTMSK_ERR_OVER_CH2			(1 << 6)
#define S5PCSIS_INTMSK_ERR_OVER_CH1			(1 << 5)
#define S5PCSIS_INTMSK_ERR_OVER_CH0			(1 << 4)
#define S5PCSIS_INTMSK_ERR_ECC				(1 << 2)
#define S5PCSIS_INTMSK_ERR_CRC				(1 << 1)
#define S5PCSIS_INTMSK_ERR_ID				(1 << 0)

/* Interrupt source */
#define S5PCSIS_INTSRC					(0x14)
#define S5PCSIS_INTSRC_EVEN_BEFORE			(1 << 31)
#define S5PCSIS_INTSRC_EVEN_AFTER			(1 << 30)
#define S5PCSIS_INTSRC_EVEN				(0x3 << 30)
#define S5PCSIS_INTSRC_ODD_BEFORE			(1 << 29)
#define S5PCSIS_INTSRC_ODD_AFTER			(1 << 28)
#define S5PCSIS_INTSRC_ODD				(0x3 << 28)
#define S5PCSIS_INTSRC_FRAME_START			(0xf << 24)
#define S5PCSIS_INTSRC_FRAME_END			(0xf << 20)
#define S5PCSIS_INTSRC_ERR_SOT_HS			(0xf << 16)
#define S5PCSIS_INTSRC_ERR_LOST_FS			(0xf << 12)
#define S5PCSIS_INTSRC_ERR_LOST_FE			(0xf << 8)
#define S5PCSIS_INTSRC_ERR_OVER				(0xf << 4)
#define S5PCSIS_INTSRC_ERR_ECC				(1 << 2)
#define S5PCSIS_INTSRC_ERR_CRC				(1 << 1)
#define S5PCSIS_INTSRC_ERR_ID				(1 << 0)
#define S5PCSIS_INTSRC_ERRORS				(0xf1111117)

/* Pixel resolution */
#define S5PCSIS_RESOL					(0x2c)
#define CSIS_MAX_PIX_WIDTH				(0xffff)
#define CSIS_MAX_PIX_HEIGHT				(0xffff)

void s5pcsis_enable_interrupts(void __iomem *base_reg,
	struct fimc_is_image *image, bool on)
{
	u32 val = readl(base_reg + S5PCSIS_INTMSK);

	val = on ? val | S5PCSIS_INTMSK_EN_ALL :
		   val & ~S5PCSIS_INTMSK_EN_ALL;

	if (image->format.field == V4L2_FIELD_INTERLACED) {
		if (on) {
			val |= S5PCSIS_INTMSK_FRAME_START_CH2;
			val |= S5PCSIS_INTMSK_FRAME_END_CH2;
		} else {
			val &= ~S5PCSIS_INTMSK_FRAME_START_CH2;
			val &= ~S5PCSIS_INTMSK_FRAME_END_CH2;
		}
	}

	writel(val, base_reg + S5PCSIS_INTMSK);
}

void s5pcsis_reset(void __iomem *base_reg)
{
	u32 val = readl(base_reg + S5PCSIS_CTRL);

	writel(val | S5PCSIS_CTRL_RESET, base_reg + S5PCSIS_CTRL);
	udelay(10);
}

void s5pcsis_system_enable(void __iomem *base_reg, int on, u32 lanes)
{
	u32 val;

	val = readl(base_reg + S5PCSIS_CTRL);

	val |= S5PCSIS_CTRL_WCLK_EXTCLK;

	if (on) {
		val |= S5PCSIS_CTRL_ENABLE;
		val |= S5PCSIS_CTRL_WCLK_EXTCLK;
	} else
		val &= ~S5PCSIS_CTRL_ENABLE;
	writel(val, base_reg + S5PCSIS_CTRL);

	val = readl(base_reg + S5PCSIS_DPHYCTRL);
	if (on)
		val |= S5PCSIS_DPHYCTRL_DPHY_ON(lanes);
	else
		val &= ~S5PCSIS_DPHYCTRL_DPHY_ON(lanes);
	writel(val, base_reg + S5PCSIS_DPHYCTRL);
}

/* Called with the state.lock mutex held */
static void __s5pcsis_set_format(void __iomem *base_reg,
	struct fimc_is_image *image)
{
	u32 val;

	BUG_ON(!image);

	/* Color format */
	val = readl(base_reg + S5PCSIS_CONFIG);

	if (image->format.pixelformat == V4L2_PIX_FMT_SGRBG8)
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW8;
	else
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_RAW10;

	val |= S5PCSIS_CFG_END_INTERVAL(1);
	writel(val, base_reg + S5PCSIS_CONFIG);

	/* Pixel resolution */
	val = (image->window.o_width << 16) | image->window.o_height;
	writel(val, base_reg + S5PCSIS_RESOL);

	/* Output channel2 for DT */
	if (image->format.field == V4L2_FIELD_INTERLACED) {
		val = readl(base_reg + S5PCSIS_CONFIG_CH2);
		val |= S5PCSIS_CFG_VIRTUAL_CH(2);
		val |= S5PCSIS_CFG_END_INTERVAL(1);
		val = (val & ~S5PCSIS_CFG_FMT_MASK) | S5PCSIS_CFG_FMT_USER(1);
		writel(val, base_reg + S5PCSIS_CONFIG_CH2);
	}
}

void s5pcsis_set_hsync_settle(void __iomem *base_reg, u32 settle)
{
	u32 val = readl(base_reg + (S5PCSIS_DPHYCTRL));

	val = (val & ~S5PCSIS_DPHYCTRL_HSS_MASK) | (settle << 24);

	writel(val, base_reg + S5PCSIS_DPHYCTRL);
}

void s5pcsis_set_params(void __iomem *base_reg,
	struct fimc_is_image *image, u32 lanes)
{
	u32 val;

	__s5pcsis_set_format(base_reg, image);

	val = readl(base_reg + S5PCSIS_CTRL);
	val &= ~S5PCSIS_CTRL_ALIGN_32BIT;

	val |= S5PCSIS_CTRL_NUMOFDATALANE(lanes);

	/* Interleaved data */
	if (image->format.field == V4L2_FIELD_INTERLACED) {
		pr_info("set DT only\n");
		val |= S5PCSIS_CTRL_INTERLEAVE_MODE(1); /* DT only */
		val |= S5PCSIS_CTRL_UPDATE_SHADOW(2); /* ch2 shadow reg */
	}

	/* Not using external clock. */
	val &= ~S5PCSIS_CTRL_WCLK_EXTCLK;

	writel(val, base_reg + S5PCSIS_CTRL);

	/* Update the shadow register. */
	val = readl(base_reg + S5PCSIS_CTRL);
	writel(val | S5PCSIS_CTRL_UPDATE_SHADOW(0), base_reg + S5PCSIS_CTRL);
}
