/*
 * Samsung SoC MIPI DSI Master driver.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *
 * Contacts: Tomasz Figa <t.figa@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <video/display.h>
#include <video/exynos_dsi.h>
#include <video/mipi_display.h>
#include <video/videomode.h>

#define DSIM_STATUS_REG		0x0	/* Status register */
#define DSIM_SWRST_REG		0x4	/* Software reset register */
#define DSIM_CLKCTRL_REG	0x8	/* Clock control register */
#define DSIM_TIMEOUT_REG	0xc	/* Time out register */
#define DSIM_CONFIG_REG		0x10	/* Configuration register */
#define DSIM_ESCMODE_REG	0x14	/* Escape mode register */

/* Main display image resolution register */
#define DSIM_MDRESOL_REG	0x18
#define DSIM_MVPORCH_REG	0x1c	/* Main display Vporch register */
#define DSIM_MHPORCH_REG	0x20	/* Main display Hporch register */
#define DSIM_MSYNC_REG		0x24	/* Main display sync area register */

/* Sub display image resolution register */
#define DSIM_SDRESOL_REG	0x28
#define DSIM_INTSRC_REG		0x2c	/* Interrupt source register */
#define DSIM_INTMSK_REG		0x30	/* Interrupt mask register */
#define DSIM_PKTHDR_REG		0x34	/* Packet Header FIFO register */
#define DSIM_PAYLOAD_REG	0x38	/* Payload FIFO register */
#define DSIM_RXFIFO_REG		0x3c	/* Read FIFO register */
#define DSIM_FIFOTHLD_REG	0x40	/* FIFO threshold level register */
#define DSIM_FIFOCTRL_REG	0x44	/* FIFO status and control register */

/* FIFO memory AC characteristic register */
#define DSIM_PLLCTRL_REG	0x4c	/* PLL control register */
#define DSIM_PLLTMR_REG		0x50	/* PLL timer register */
#define DSIM_PHYACCHR_REG	0x54	/* D-PHY AC characteristic register */
#define DSIM_PHYACCHR1_REG	0x58	/* D-PHY AC characteristic register1 */

/* DSIM_STATUS */
#define DSIM_STOP_STATE_DAT(x)		(((x) & 0xf) << 0)
#define DSIM_STOP_STATE_CLK		(1 << 8)
#define DSIM_TX_READY_HS_CLK		(1 << 10)
#define DSIM_PLL_STABLE			(1 << 31)

/* DSIM_SWRST */
#define DSIM_FUNCRST			(1 << 16)
#define DSIM_SWRST			(1 << 0)

/* DSIM_TIMEOUT */
#define DSIM_LPDR_TOUT(x)		((x) << 0)
#define DSIM_BTA_TOUT(x)		((x) << 16)

/* DSIM_CLKCTRL */
#define DSIM_ESC_PRESCALER(x)		(((x) & 0xffff) << 0)
#define DSIM_ESC_PRESCALER_MASK		(0xffff << 0)
#define DSIM_LANE_ESC_CLK_EN_CLK	(1 << 19)
#define DSIM_LANE_ESC_CLK_EN_DATA(x)	(((x) & 0xf) << 20)
#define DSIM_LANE_ESC_CLK_EN_DATA_MASK	(0xf << 20)
#define DSIM_BYTE_CLKEN			(1 << 24)
#define DSIM_BYTE_CLK_SRC(x)		(((x) & 0x3) << 25)
#define DSIM_BYTE_CLK_SRC_MASK		(0x3 << 25)
#define DSIM_PLL_BYPASS			(1 << 27)
#define DSIM_ESC_CLKEN			(1 << 28)
#define DSIM_TX_REQUEST_HSCLK		(1 << 31)

/* DSIM_CONFIG */
#define DSIM_LANE_EN_CLK		(1 << 0)
#define DSIM_LANE_EN(x)			(((x) & 0xf) << 1)
#define DSIM_NUM_OF_DATA_LANE(x)	(((x) & 0x3) << 5)
#define DSIM_SUB_PIX_FORMAT(x)		(((x) & 0x7) << 8)
#define DSIM_MAIN_PIX_FORMAT_MASK	(0x7 << 12)
#define DSIM_MAIN_PIX_FORMAT_RGB888	(0x7 << 12)
#define DSIM_MAIN_PIX_FORMAT_RGB666	(0x6 << 12)
#define DSIM_MAIN_PIX_FORMAT_RGB666_P	(0x5 << 12)
#define DSIM_MAIN_PIX_FORMAT_RGB565	(0x4 << 12)
#define DSIM_SUB_VC			(((x) & 0x3) << 16)
#define DSIM_MAIN_VC			(((x) & 0x3) << 18)
#define DSIM_HSA_MODE			(1 << 20)
#define DSIM_HBP_MODE			(1 << 21)
#define DSIM_HFP_MODE			(1 << 22)
#define DSIM_HSE_MODE			(1 << 23)
#define DSIM_AUTO_MODE			(1 << 24)
#define DSIM_VIDEO_MODE			(1 << 25)
#define DSIM_BURST_MODE			(1 << 26)
#define DSIM_SYNC_INFORM		(1 << 27)
#define DSIM_EOT_DISABLE		(1 << 28)
#define DSIM_MFLUSH_VS			(1 << 29)

/* DSIM_ESCMODE */
#define DSIM_TX_TRIGGER_RST		(1 << 4)
#define DSIM_TX_LPDT_LP			(1 << 6)
#define DSIM_CMD_LPDT_LP		(1 << 7)
#define DSIM_FORCE_BTA			(1 << 16)
#define DSIM_FORCE_STOP_STATE		(1 << 20)
#define DSIM_STOP_STATE_CNT(x)		(((x) & 0x7ff) << 21)
#define DSIM_STOP_STATE_CNT_MASK	(0x7ff << 21)

/* DSIM_MDRESOL */
#define DSIM_MAIN_STAND_BY		(1 << 31)
#define DSIM_MAIN_VRESOL(x)		(((x) & 0x7ff) << 16)
#define DSIM_MAIN_HRESOL(x)		(((x) & 0X7ff) << 0)

/* DSIM_MVPORCH */
#define DSIM_CMD_ALLOW(x)		((x) << 28)
#define DSIM_STABLE_VFP(x)		((x) << 16)
#define DSIM_MAIN_VBP(x)		((x) << 0)
#define DSIM_CMD_ALLOW_MASK		(0xf << 28)
#define DSIM_STABLE_VFP_MASK		(0x7ff << 16)
#define DSIM_MAIN_VBP_MASK		(0x7ff << 0)

/* DSIM_MHPORCH */
#define DSIM_MAIN_HFP(x)		((x) << 16)
#define DSIM_MAIN_HBP(x)		((x) << 0)
#define DSIM_MAIN_HFP_MASK		((0xffff) << 16)
#define DSIM_MAIN_HBP_MASK		((0xffff) << 0)

/* DSIM_MSYNC */
#define DSIM_MAIN_VSA(x)		((x) << 22)
#define DSIM_MAIN_HSA(x)		((x) << 0)
#define DSIM_MAIN_VSA_MASK		((0x3ff) << 22)
#define DSIM_MAIN_HSA_MASK		((0xffff) << 0)

/* DSIM_SDRESOL */
#define DSIM_SUB_STANDY(x)		((x) << 31)
#define DSIM_SUB_VRESOL(x)		((x) << 16)
#define DSIM_SUB_HRESOL(x)		((x) << 0)
#define DSIM_SUB_STANDY_MASK		((0x1) << 31)
#define DSIM_SUB_VRESOL_MASK		((0x7ff) << 16)
#define DSIM_SUB_HRESOL_MASK		((0x7ff) << 0)

/* DSIM_INTSRC */
#define DSIM_INT_PLL_STABLE		(1 << 31)
#define DSIM_INT_SW_RST_RELEASE		(1 << 30)
#define DSIM_INT_SFR_FIFO_EMPTY		(1 << 29)
#define DSIM_INT_BTA			(1 << 25)
#define DSIM_INT_FRAME_DONE		(1 << 24)
#define DSIM_INT_RX_TIMEOUT		(1 << 21)
#define DSIM_INT_BTA_TIMEOUT		(1 << 20)
#define DSIM_INT_RX_DONE		(1 << 18)
#define DSIM_INT_RX_TE			(1 << 17)
#define DSIM_INT_RX_ACK			(1 << 16)
#define DSIM_INT_RX_ECC_ERR		(1 << 15)
#define DSIM_INT_RX_CRC_ERR		(1 << 14)

/* DSIM_FIFOCTRL */
#define DSIM_FULL_H_SFR			(1 << 23)

/* DSIM_PHYACCHR */
#define DSIM_AFC_EN			(1 << 14)
#define DSIM_AFC_CTL(x)			(((x) & 0x7) << 5)

/* DSIM_PLLCTRL */
#define DSIM_FREQ_BAND(x)		((x) << 24)
#define DSIM_PLL_EN			(1 << 23)
#define DSIM_PLL_P(x)			((x) << 13)
#define DSIM_PLL_M(x)			((x) << 4)
#define DSIM_PLL_S(x)			((x) << 1)

#define DSI_MAX_BUS_WIDTH		4
#define DSI_NUM_VIRTUAL_CHANNELS	4
#define DSI_TX_FIFO_SIZE		2048
#define DSI_RX_FIFO_SIZE		256
#define DSI_XFER_TIMEOUT_MS		100
#define DSI_RX_FIFO_EMPTY		0x30800002

enum exynos_dsi_transfer_type {
	EXYNOS_DSI_TX,
	EXYNOS_DSI_RX,
};

struct exynos_dsi_transfer {
	struct list_head list;
	struct completion completed;
	int result;
	u8 type;
	u8 data[2];

	const u8 *tx_payload;
	u16 tx_len;
	u16 tx_done;

	u8 *rx_payload;
	u16 rx_len;
	u16 rx_done;
};

struct exynos_dsi {
	struct video_source out;
	struct mipi_dsi_interface_params params;
	bool streaming;
	bool enabled;

	struct platform_device *pdev;
	struct phy *phy;
	struct device *dev;
	struct resource *res;
	struct clk *pll_clk;
	struct clk *bus_clk;
	unsigned int irq;
	void __iomem *reg_base;
	struct regulator_bulk_data supplies[2];
	struct exynos_dsi_platform_data *pd;

	spinlock_t transfer_lock;
	struct list_head transfer_list;
};

#define src_to_dsi(src)		container_of(src, struct exynos_dsi, out)

/*
 * H/W control
 */

static void exynos_dsi_reset(struct exynos_dsi *dsi)
{
	writel(DSIM_SWRST, dsi->reg_base + DSIM_SWRST_REG);

	udelay(10);
}

#ifndef MHZ
#define MHZ	(1000*1000)
#endif

static const unsigned long freq_bands[] = {
	100 * MHZ, 120 * MHZ, 160 * MHZ, 200 * MHZ,
	270 * MHZ, 320 * MHZ, 390 * MHZ, 450 * MHZ,
	510 * MHZ, 560 * MHZ, 640 * MHZ, 690 * MHZ,
	770 * MHZ, 870 * MHZ, 950 * MHZ,
};

static const int afc_settings[] = {
	1, 0, 3, 2, 5, 4,
};

static unsigned long exynos_dsi_pll_find_pms(struct exynos_dsi *dsi,
		unsigned long fin, unsigned long fout, u8 *p, u16 *m, u8 *s)
{
	unsigned long best_freq = 0;
	u32 min_delta = 0xffffffff;
	u8 p_min, p_max;
	u8 _p, best_p;
	u16 _m, best_m;
	u8 _s, best_s;

	p_min = DIV_ROUND_UP(fin, (12 * MHZ));
	p_max = fin / (6 * MHZ);

	for (_p = p_min; _p <= p_max; ++_p) {
		for (_s = 0; _s <= 5; ++_s) {
			u64 tmp;
			u32 delta;
			u16 div;

			tmp = (u64)fout * (_p << _s);
			do_div(tmp, fin);
			_m = tmp;
			if (_m < 41 || _m > 125)
				continue;

			tmp = (u64)_m * fin;
			do_div(tmp, _p);
			if (tmp < 500 * MHZ || tmp > 1000 * MHZ)
				continue;

			tmp = (u64)_m * fin;
			div = (_p << _s);
			do_div(tmp, div);

			delta = abs(fout - tmp);
			if (delta < min_delta) {
				best_p = _p;
				best_m = _m;
				best_s = _s;
				min_delta = delta;
				best_freq = tmp;
			}
		}
	}

	if (best_freq) {
		*p = best_p;
		*m = best_m;
		*s = best_s;
	}

	return best_freq;
}

static unsigned long exynos_dsi_set_pll(struct exynos_dsi *dsi,
							unsigned long freq)
{
	unsigned long fin, fout, fin_pll;
	int timeout;
	u8 p;
	u16 m;
	u8 s;
	int band;
	int afc;
	u32 reg;

	clk_set_rate(dsi->pll_clk, dsi->pd->pll_clk_rate);

	fin = clk_get_rate(dsi->pll_clk);
	if (!fin) {
		dev_err(dsi->dev, "failed to get PLL clock frequency\n");
		return 0;
	}

	dev_dbg(dsi->dev, "PLL input frequency: %lu\n", fin);

	fout = exynos_dsi_pll_find_pms(dsi, fin, freq, &p, &m, &s);
	if (!fout) {
		dev_err(dsi->dev,
			"failed to find PLL coefficients for requested frequency\n");
		return -EFAULT;
	}

	dev_dbg(dsi->dev, "PLL freq %lu, (p %d, m %d, s %d)\n", fout, p, m, s);

	for (band = 0; band < ARRAY_SIZE(freq_bands); ++band)
		if (fout < freq_bands[band])
			break;

	fin_pll = DIV_ROUND_CLOSEST(fin, p);
	fin_pll /= MHZ;
	if (fin_pll > 6)
		fin_pll -= 6;
	else
		fin_pll = 0;
	if (fin_pll >= ARRAY_SIZE(afc_settings))
		fin_pll = ARRAY_SIZE(afc_settings) - 1;

	afc = afc_settings[fin_pll];

	dev_dbg(dsi->dev, "freq band %d, afc_setting %d\n", band, afc);

	writel(dsi->pd->pll_stable_time, dsi->reg_base + DSIM_PLLTMR_REG);

	reg = DSIM_AFC_CTL(afc) | DSIM_AFC_EN;
	writel(reg, dsi->reg_base + DSIM_PHYACCHR_REG);

	reg = DSIM_FREQ_BAND(band) | DSIM_PLL_EN
			| DSIM_PLL_P(p) | DSIM_PLL_M(m) | DSIM_PLL_S(s);
	writel(reg, dsi->reg_base + DSIM_PLLCTRL_REG);

	timeout = 1000;
	do {
		if (timeout-- == 0) {
			dev_err(dsi->dev, "PLL failed to stabilize\n");
			return -EFAULT;
		}
		reg = readl(dsi->reg_base + DSIM_STATUS_REG);
	} while ((reg & DSIM_PLL_STABLE) == 0);

	return fout;
}

static int exynos_dsi_enable_clock(struct exynos_dsi *dsi)
{
	unsigned long hs_clk, byte_clk, esc_clk;
	unsigned long esc_div;
	u32 reg;

	hs_clk = exynos_dsi_set_pll(dsi, dsi->params.hs_clk_freq);
	if (!hs_clk) {
		dev_err(dsi->dev, "failed to configure DSI PLL\n");
		return -EFAULT;
	}

	byte_clk = hs_clk / 8;
	esc_div = DIV_ROUND_UP(byte_clk, dsi->params.esc_clk_freq);
	esc_clk = byte_clk / esc_div;

	if (esc_clk > 20 * MHZ) {
		++esc_div;
		esc_clk = byte_clk / esc_div;
	}

	dev_dbg(dsi->dev, "hs_clk = %lu, byte_clk = %lu, esc_clk = %lu\n",
						hs_clk, byte_clk, esc_clk);

	reg = readl(dsi->reg_base + DSIM_CLKCTRL_REG);
	reg &= ~(DSIM_ESC_PRESCALER_MASK | DSIM_LANE_ESC_CLK_EN_CLK
			| DSIM_LANE_ESC_CLK_EN_DATA_MASK | DSIM_PLL_BYPASS
			| DSIM_BYTE_CLK_SRC_MASK);
	reg |= DSIM_ESC_CLKEN | DSIM_BYTE_CLKEN
			| DSIM_ESC_PRESCALER(esc_div)
			| DSIM_LANE_ESC_CLK_EN_CLK
			| DSIM_LANE_ESC_CLK_EN_DATA(dsi->params.data_lanes)
			| DSIM_BYTE_CLK_SRC(0)
			| DSIM_TX_REQUEST_HSCLK;
	writel(reg, dsi->reg_base + DSIM_CLKCTRL_REG);

	return 0;
}

static void exynos_dsi_disable_clock(struct exynos_dsi *dsi)
{
	u32 reg;

	reg = readl(dsi->reg_base + DSIM_CLKCTRL_REG);
	reg &= ~(DSIM_LANE_ESC_CLK_EN_CLK | DSIM_LANE_ESC_CLK_EN_DATA_MASK |
			DSIM_ESC_CLKEN | DSIM_BYTE_CLKEN);
	writel(reg, dsi->reg_base + DSIM_CLKCTRL_REG);

	reg = readl(dsi->reg_base + DSIM_PLLCTRL_REG);
	reg &= ~DSIM_PLL_EN;
	writel(reg, dsi->reg_base + DSIM_PLLCTRL_REG);
}

static int exynos_dsi_init_link(struct exynos_dsi *dsi)
{
	u32 reg;
	int timeout;

	/* Initialize FIFO pointers */
	reg = readl(dsi->reg_base + DSIM_FIFOCTRL_REG);
	reg &= ~0x1f;
	writel(reg, dsi->reg_base + DSIM_FIFOCTRL_REG);

	usleep_range(10000, 10000);

	reg |= 0x1f;
	writel(reg, dsi->reg_base + DSIM_FIFOCTRL_REG);

	usleep_range(10000, 10000);

	/* DSI configuration */
	reg = 0;

	if (dsi->params.mode & DSI_MODE_VIDEO) {
		reg |= DSIM_VIDEO_MODE;

		if (!(dsi->params.mode & DSI_MODE_VSYNC_FLUSH))
			reg |= DSIM_MFLUSH_VS;
		if (!(dsi->params.mode & DSI_MODE_EOT_PACKET))
			reg |= DSIM_EOT_DISABLE;
		if (dsi->params.mode & DSI_MODE_VIDEO_SYNC_PULSE)
			reg |= DSIM_SYNC_INFORM;
		if (dsi->params.mode & DSI_MODE_VIDEO_BURST)
			reg |= DSIM_BURST_MODE;
		if (dsi->params.mode & DSI_MODE_VIDEO_AUTO_VERT)
			reg |= DSIM_AUTO_MODE;
		if (dsi->params.mode & DSI_MODE_VIDEO_HSE)
			reg |= DSIM_HSE_MODE;
		if (!(dsi->params.mode & DSI_MODE_VIDEO_HFP))
			reg |= DSIM_HFP_MODE;
		if (!(dsi->params.mode & DSI_MODE_VIDEO_HBP))
			reg |= DSIM_HBP_MODE;
		if (!(dsi->params.mode & DSI_MODE_VIDEO_HSA))
			reg |= DSIM_HSA_MODE;
	}

	switch (dsi->params.format) {
	case DSI_FMT_RGB888:
		reg |= DSIM_MAIN_PIX_FORMAT_RGB888;
		break;
	case DSI_FMT_RGB666:
		reg |= DSIM_MAIN_PIX_FORMAT_RGB666;
		break;
	case DSI_FMT_RGB666_PACKED:
		reg |= DSIM_MAIN_PIX_FORMAT_RGB666_P;
		break;
	case DSI_FMT_RGB565:
		reg |= DSIM_MAIN_PIX_FORMAT_RGB565;
		break;
	default:
		dev_err(dsi->dev, "invalid pixel format\n");
		return -EINVAL;
	}

	switch (dsi->params.data_lanes) {
	case 0x1:
		reg |= DSIM_NUM_OF_DATA_LANE(0);
		break;
	case 0x3:
		reg |= DSIM_NUM_OF_DATA_LANE(1);
		break;
	case 0x7:
		reg |= DSIM_NUM_OF_DATA_LANE(2);
		break;
	case 0xf:
		reg |= DSIM_NUM_OF_DATA_LANE(3);
		break;
	default:
		return -EINVAL;
	}

	writel(reg, dsi->reg_base + DSIM_CONFIG_REG);

	reg |= DSIM_LANE_EN_CLK;
	writel(reg, dsi->reg_base + DSIM_CONFIG_REG);

	reg |= DSIM_LANE_EN(dsi->params.data_lanes);
	writel(reg, dsi->reg_base + DSIM_CONFIG_REG);

	/* Clock configuration */
	exynos_dsi_enable_clock(dsi);

	/* Check clock and data lane state are stop state */
	timeout = 100;
	do {
		if (timeout-- == 0) {
			dev_err(dsi->dev, "waiting for bus lanes timed out\n");
			return -EFAULT;
		}

		reg = readl(dsi->reg_base + DSIM_STATUS_REG);
		if ((reg & DSIM_STOP_STATE_DAT(dsi->params.data_lanes))
		    != DSIM_STOP_STATE_DAT(dsi->params.data_lanes))
			continue;
	} while (!(reg & (DSIM_STOP_STATE_CLK | DSIM_TX_READY_HS_CLK)));

	reg = readl(dsi->reg_base + DSIM_ESCMODE_REG);
	reg &= ~DSIM_STOP_STATE_CNT_MASK;
	reg |= DSIM_STOP_STATE_CNT(dsi->pd->stop_holding_cnt);
	writel(reg, dsi->reg_base + DSIM_ESCMODE_REG);

	reg = DSIM_BTA_TOUT(dsi->pd->bta_timeout)
					| DSIM_LPDR_TOUT(dsi->pd->rx_timeout);
	writel(reg, dsi->reg_base + DSIM_TIMEOUT_REG);

	return 0;
}

static int exynos_dsi_set_display_mode(struct exynos_dsi *dsi)
{
	const struct videomode *mode;
	u32 reg;
	int ret;

	ret = display_entity_get_modes(dsi->out.sink, &mode);
	if (ret < 0) {
		dev_err(dsi->dev, "failed to get display video mode\n");
		return ret;
	}

	if (dsi->params.mode & DSI_MODE_VIDEO) {
		reg = DSIM_CMD_ALLOW(dsi->params.cmd_allow)
			| DSIM_STABLE_VFP(mode->vfront_porch)
			| DSIM_MAIN_VBP(mode->vback_porch);
		writel(reg, dsi->reg_base + DSIM_MVPORCH_REG);

		reg = DSIM_MAIN_HFP(mode->hfront_porch)
			| DSIM_MAIN_HBP(mode->hback_porch);
		writel(reg, dsi->reg_base + DSIM_MHPORCH_REG);

		reg = DSIM_MAIN_VSA(mode->vsync_len)
			| DSIM_MAIN_HSA(mode->hsync_len);
		writel(reg, dsi->reg_base + DSIM_MSYNC_REG);
	}

	reg = DSIM_MAIN_HRESOL(mode->hactive) | DSIM_MAIN_VRESOL(mode->vactive);
	writel(reg, dsi->reg_base + DSIM_MDRESOL_REG);

	dev_dbg(dsi->dev, "LCD width = %d, height = %d\n",
						mode->hactive, mode->vactive);

	return 0;
}

static void exynos_dsi_set_display_enable(struct exynos_dsi *dsi, bool enable)
{
	u32 reg;

	reg = readl(dsi->reg_base + DSIM_MDRESOL_REG);
	if (enable)
		reg |= DSIM_MAIN_STAND_BY;
	else
		reg &= ~DSIM_MAIN_STAND_BY;
	writel(reg, dsi->reg_base + DSIM_MDRESOL_REG);
}

/*
 * FIFO
 */

static int exynos_dsi_wait_for_hdr_fifo(struct exynos_dsi *dsi)
{
	int timeout = 20000;

	do {
		u32 reg = readl(dsi->reg_base + DSIM_FIFOCTRL_REG);

		if (!(reg & DSIM_FULL_H_SFR))
			return 0;

		if (!cond_resched())
			usleep_range(950, 1050);
	} while (--timeout);

	return -ETIMEDOUT;
}

static void exynos_dsi_send_to_fifo(struct exynos_dsi *dsi,
					struct exynos_dsi_transfer *xfer)
{
	const u8 *payload = xfer->tx_payload + xfer->tx_done;
	u16 length = xfer->tx_len - xfer->tx_done;
	bool first = !xfer->tx_done;
	u32 reg;

	dev_dbg(dsi->dev,
		"< xfer %p, tx_len %u, tx_done %u, rx_len %u, rx_done %u\n",
		xfer, xfer->tx_len, xfer->tx_done, xfer->rx_len, xfer->rx_done);

	if (length > DSI_TX_FIFO_SIZE)
		length = DSI_TX_FIFO_SIZE;

	xfer->tx_done += length;

	/* Send payload */
	while (length >= 4) {
		reg = (payload[3] << 24) | (payload[2] << 16)
					| (payload[1] << 8) | payload[0];
		writel(reg, dsi->reg_base + DSIM_PAYLOAD_REG);
		payload += 4;
		length -= 4;
	}

	reg = 0;
	switch (length) {
	case 3:
		reg |= payload[2] << 16;
		/* Fall through */
	case 2:
		reg |= payload[1] << 8;
		/* Fall through */
	case 1:
		reg |= payload[0];
		writel(reg, dsi->reg_base + DSIM_PAYLOAD_REG);
		break;
	case 0:
		/* Do nothing */
		break;
	}

	/* Send packet header */
	if (!first)
		return;

	if (xfer->rx_len) {
		reg = (xfer->rx_len << 8)
				| MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE;
		if (exynos_dsi_wait_for_hdr_fifo(dsi)) {
			dev_err(dsi->dev,
					"waiting for header FIFO timed out\n");
			return;
		}
		writel(reg, dsi->reg_base + DSIM_PKTHDR_REG);
	}

	reg = (xfer->data[1] << 16) | (xfer->data[0] << 8) | xfer->type;
	if (exynos_dsi_wait_for_hdr_fifo(dsi)) {
		dev_err(dsi->dev, "waiting for header FIFO timed out\n");
		return;
	}
	writel(reg, dsi->reg_base + DSIM_PKTHDR_REG);
}

static void exynos_dsi_read_from_fifo(struct exynos_dsi *dsi,
					struct exynos_dsi_transfer *xfer)
{
	u8 *payload = xfer->rx_payload + xfer->rx_done;
	bool first = !xfer->rx_done;
	u16 length;
	u32 reg;

	if (first) {
		reg = readl(dsi->reg_base + DSIM_RXFIFO_REG);

		switch (reg & 0x3f) {
		case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_2BYTE:
		case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_2BYTE:
			if (xfer->rx_len >= 2) {
				payload[1] = reg >> 16;
				++xfer->rx_done;
			}
			/* Fall through */
		case MIPI_DSI_RX_GENERIC_SHORT_READ_RESPONSE_1BYTE:
		case MIPI_DSI_RX_DCS_SHORT_READ_RESPONSE_1BYTE:
			payload[0] = reg >> 8;
			++xfer->rx_done;
			xfer->rx_len = xfer->rx_done;
			xfer->result = 0;
			goto clear_fifo;
		}

		length = (reg >> 8) & 0xffff;
		if (length > xfer->rx_len) {
			dev_err(dsi->dev,
				"response too long (expected %u, got %u bytes)\n",
				xfer->rx_len, length);
			xfer->rx_len = 0;
			xfer->rx_done = 0;
			xfer->result = -EFAULT;
			goto clear_fifo;
		}
		if (length < xfer->rx_len)
			xfer->rx_len = length;
	}

	length = xfer->rx_len - xfer->rx_done;
	xfer->rx_done += length;

	/* Receive payload */
	while (length >= 4) {
		reg = readl(dsi->reg_base + DSIM_RXFIFO_REG);
		payload[0] = (reg >>  0) & 0xff;
		payload[1] = (reg >>  8) & 0xff;
		payload[2] = (reg >> 16) & 0xff;
		payload[3] = (reg >> 24) & 0xff;
		payload += 4;
		length -= 4;
	}

	if (length) {
		reg = readl(dsi->reg_base + DSIM_RXFIFO_REG);
		switch (length) {
		case 3:
			payload[2] = (reg >> 16) & 0xff;
			/* Fall through */
		case 2:
			payload[1] = (reg >> 8) & 0xff;
			/* Fall through */
		case 1:
			payload[0] = reg & 0xff;
		}
	}

	if (xfer->rx_done == xfer->rx_len)
		xfer->result = 0;

clear_fifo:
	length = DSI_RX_FIFO_SIZE / 4;
	do {
		reg = readl(dsi->reg_base + DSIM_RXFIFO_REG);
		if (reg == DSI_RX_FIFO_EMPTY)
			break;
	} while (--length);
}

/*
 * Transfer
 */

static void exynos_dsi_transfer_start(struct exynos_dsi *dsi)
{
	unsigned long flags;
	struct exynos_dsi_transfer *xfer;
	bool start = false;

again:
	spin_lock_irqsave(&dsi->transfer_lock, flags);

	if (list_empty(&dsi->transfer_list)) {
		spin_unlock_irqrestore(&dsi->transfer_lock, flags);
		return;
	}

	xfer = list_first_entry(&dsi->transfer_list,
					struct exynos_dsi_transfer, list);

	spin_unlock_irqrestore(&dsi->transfer_lock, flags);

	if (xfer->tx_len && xfer->tx_done == xfer->tx_len)
		/* waiting for RX */
		return;

	exynos_dsi_send_to_fifo(dsi, xfer);

	if (xfer->tx_len || xfer->rx_len)
		return;

	xfer->result = 0;
	complete(&xfer->completed);

	spin_lock_irqsave(&dsi->transfer_lock, flags);

	list_del_init(&xfer->list);
	start = !list_empty(&dsi->transfer_list);

	spin_unlock_irqrestore(&dsi->transfer_lock, flags);

	if (start)
		goto again;
}

static bool exynos_dsi_transfer_finish(struct exynos_dsi *dsi)
{
	static unsigned long j;
	struct exynos_dsi_transfer *xfer;
	unsigned long flags;
	bool start = true;

	spin_lock_irqsave(&dsi->transfer_lock, flags);

	if (list_empty(&dsi->transfer_list)) {
		spin_unlock_irqrestore(&dsi->transfer_lock, flags);
		if (printk_timed_ratelimit(&j, 500))
			dev_warn(dsi->dev, "unexpected TX/RX interrupt\n");
		return false;
	}

	xfer = list_first_entry(&dsi->transfer_list,
					struct exynos_dsi_transfer, list);

	spin_unlock_irqrestore(&dsi->transfer_lock, flags);

	dev_dbg(dsi->dev,
		"> xfer %p, tx_len %u, tx_done %u, rx_len %u, rx_done %u\n",
		xfer, xfer->tx_len, xfer->tx_done, xfer->rx_len, xfer->rx_done);

	if (xfer->tx_done != xfer->tx_len)
		return true;

	if (xfer->rx_done != xfer->rx_len)
		exynos_dsi_read_from_fifo(dsi, xfer);

	if (xfer->rx_done != xfer->rx_len)
		return true;

	spin_lock_irqsave(&dsi->transfer_lock, flags);

	list_del_init(&xfer->list);
	start = !list_empty(&dsi->transfer_list);

	spin_unlock_irqrestore(&dsi->transfer_lock, flags);

	if (!xfer->rx_len)
		xfer->result = 0;
	complete(&xfer->completed);

	return start;
}

static void exynos_dsi_remove_transfer(struct exynos_dsi *dsi,
					struct exynos_dsi_transfer *xfer)
{
	unsigned long flags;
	bool start;

	spin_lock_irqsave(&dsi->transfer_lock, flags);

	if (!list_empty(&dsi->transfer_list)
	    && xfer == list_first_entry(&dsi->transfer_list,
					struct exynos_dsi_transfer, list)) {
		list_del_init(&xfer->list);
		start = !list_empty(&dsi->transfer_list);
		spin_unlock_irqrestore(&dsi->transfer_lock, flags);
		if (start)
			exynos_dsi_transfer_start(dsi);
		return;
	}

	list_del_init(&xfer->list);

	spin_unlock_irqrestore(&dsi->transfer_lock, flags);
}

static int exynos_dsi_transfer(struct exynos_dsi *dsi,
					struct exynos_dsi_transfer *xfer)
{
	unsigned long flags;
	bool stopped;

	xfer->tx_done = 0;
	xfer->rx_done = 0;
	xfer->result = -ETIMEDOUT;
	init_completion(&xfer->completed);

	spin_lock_irqsave(&dsi->transfer_lock, flags);

	stopped = list_empty(&dsi->transfer_list);
	list_add_tail(&xfer->list, &dsi->transfer_list);

	spin_unlock_irqrestore(&dsi->transfer_lock, flags);

	if (stopped)
		exynos_dsi_transfer_start(dsi);

	wait_for_completion_timeout(&xfer->completed,
				msecs_to_jiffies(DSI_XFER_TIMEOUT_MS));
	if (xfer->result == -ETIMEDOUT) {
		exynos_dsi_remove_transfer(dsi, xfer);
		dev_err(dsi->dev, "xfer timed out\n");
		return -ETIMEDOUT;
	}

	/* Also covers hardware timeout condition */
	return xfer->result;
}

/*
 * Interrupt handler
 */

static irqreturn_t exynos_dsi_irq(int irq, void *dev_id)
{
	struct exynos_dsi *dsi = dev_id;
	u32 status;

	status = readl(dsi->reg_base + DSIM_INTSRC_REG);
	writel(status, dsi->reg_base + DSIM_INTSRC_REG);

	dev_dbg(dsi->dev, "%s: status = %08x\n", __func__, status);

	if (status & DSIM_INT_SW_RST_RELEASE) {
		u32 mask = ~(DSIM_INT_RX_DONE | DSIM_INT_SFR_FIFO_EMPTY);
		writel(mask, dsi->reg_base + DSIM_INTMSK_REG);
		return IRQ_HANDLED;
	}

	if (exynos_dsi_transfer_finish(dsi))
		exynos_dsi_transfer_start(dsi);

	return IRQ_HANDLED;
}

/*
 * Display source
 */

static int exynos_dsi_set_stream(struct video_source *src,
					enum video_source_stream_state state)
{
	struct exynos_dsi *dsi = src_to_dsi(src);

	if (pm_runtime_suspended(dsi->dev))
		return -EINVAL;

	switch (state) {
	case DISPLAY_ENTITY_STREAM_STOPPED:
		exynos_dsi_set_display_enable(dsi, false);
		dsi->streaming = false;
		break;

	case DISPLAY_ENTITY_STREAM_CONTINUOUS:
		exynos_dsi_set_display_mode(dsi);
		exynos_dsi_set_display_enable(dsi, true);
		dsi->streaming = true;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/* enable/disable dsi bus */
static int exynos_dsi_enable(struct video_source *src)
{
	struct display_entity_interface_params params;
	struct exynos_dsi *dsi = src_to_dsi(src);
	int ret;

	if (dsi->enabled)
		return 0;

	ret = display_entity_get_params(src->sink, &params);
	if (ret < 0)
		return ret;
	dsi->params = params.p.dsi;

	ret = regulator_bulk_enable(ARRAY_SIZE(dsi->supplies), dsi->supplies);
	if (ret < 0)
		return ret;

	clk_prepare_enable(dsi->bus_clk);
	clk_prepare_enable(dsi->pll_clk);

	phy_power_on(dsi->phy);

	exynos_dsi_reset(dsi);
	exynos_dsi_init_link(dsi);

	dsi->enabled = true;

	return 0;
}

static int exynos_dsi_disable(struct video_source *src)
{
	struct exynos_dsi *dsi = src_to_dsi(src);
	int ret;

	if (!dsi->enabled)
		return 0;

	if (dsi->streaming)
		return -EBUSY;

	dsi->enabled = false;

	exynos_dsi_disable_clock(dsi);

	phy_power_off(dsi->phy);

	clk_disable_unprepare(dsi->pll_clk);
	clk_disable_unprepare(dsi->bus_clk);

	ret = regulator_bulk_disable(ARRAY_SIZE(dsi->supplies), dsi->supplies);
	if (ret < 0)
		return ret;

	return 0;
}

/* data transfer */
static int exynos_dsi_dcs_write(struct video_source *src, int channel,
						const u8 *data, size_t len)
{
	struct exynos_dsi *dsi = src_to_dsi(src);
	struct exynos_dsi_transfer xfer;

	if (!len)
		return -EINVAL;

	switch (len) {
	case 1:
		len = 0;
		xfer.type = MIPI_DSI_DCS_SHORT_WRITE;
		xfer.data[0] = data[0];
		xfer.data[1] = 0;
		break;
	case 2:
		len = 0;
		xfer.type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
		xfer.data[0] = data[0];
		xfer.data[1] = data[1];
		break;
	default:
		xfer.type = MIPI_DSI_DCS_LONG_WRITE;
		xfer.data[0] = len & 0xff;
		xfer.data[1] = len >> 8;
	}

	xfer.tx_payload = data;
	xfer.tx_len = len;
	xfer.rx_len = 0;

	return exynos_dsi_transfer(dsi, &xfer);
}

static int exynos_dsi_dcs_read(struct video_source *src,
				int channel, u8 dcs_cmd, u8 *data, size_t len)
{
	struct exynos_dsi *dsi = src_to_dsi(src);
	struct exynos_dsi_transfer xfer;

	xfer.type = MIPI_DSI_DCS_READ;
	xfer.tx_len = 0;
	xfer.rx_payload = data;
	xfer.rx_len = len;
	xfer.data[0] = dcs_cmd;
	xfer.data[1] = 0x00;

	return exynos_dsi_transfer(dsi, &xfer);
}

static const struct common_video_source_ops exynos_dsi_common_ops = {
	.set_stream = exynos_dsi_set_stream,
};

static const struct dsi_video_source_ops exynos_dsi_ops = {
	.enable = exynos_dsi_enable,
	.disable = exynos_dsi_disable,

	.dcs_write = exynos_dsi_dcs_write,
	.dcs_read = exynos_dsi_dcs_read,
};

#ifdef CONFIG_OF
/*
 * Device Tree
 */

static struct exynos_dsi_platform_data *exynos_dsi_parse_dt(
						struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct exynos_dsi_platform_data *dsi_pd;
	struct device *dev = &pdev->dev;
	const __be32 *prop_data;

	dsi_pd = kzalloc(sizeof(*dsi_pd), GFP_KERNEL);
	if (!dsi_pd) {
		dev_err(dev, "failed to allocate dsi platform data\n");
		return NULL;
	}

	prop_data = of_get_property(node, "samsung,pll-stable-time", NULL);
	if (!prop_data) {
		dev_err(dev, "failed to get pll-stable-time property\n");
		goto err_free_pd;
	}
	dsi_pd->pll_stable_time = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "samsung,stop-holding-count", NULL);
	if (!prop_data) {
		dev_err(dev, "failed to get stop-holding-count property\n");
		goto err_free_pd;
	}
	dsi_pd->stop_holding_cnt = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "samsung,bta-timeout", NULL);
	if (!prop_data) {
		dev_err(dev, "failed to get bta-timeout property\n");
		goto err_free_pd;
	}
	dsi_pd->bta_timeout = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "samsung,rx-timeout", NULL);
	if (!prop_data) {
		dev_err(dev, "failed to get rx-timeout property\n");
		goto err_free_pd;
	}
	dsi_pd->rx_timeout = be32_to_cpu(*prop_data);

	prop_data = of_get_property(node, "samsung,pll-clk-freq", NULL);
	if (!prop_data) {
		dev_err(dev, "failed to get pll-clk-freq property\n");
		goto err_free_pd;
	}
	dsi_pd->pll_clk_rate = be32_to_cpu(*prop_data);

	return dsi_pd;

err_free_pd:
	kfree(dsi_pd);

	return NULL;
}

static struct of_device_id exynos_dsi_of_match[] = {
	{ .compatible = "samsung,exynos4210-mipi-dsi" },
	{ }
};

MODULE_DEVICE_TABLE(of, exynos_dsi_of_match);
#else
static struct exynos_dsi_platform_data *exynos_dsi_parse_dt(
						struct platform_device *pdev)
{
	return NULL;
}
#endif

/*
 * Platform driver
 */

static int exynos_dsi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct exynos_dsi *dsi;
	int ret;

	dsi = devm_kzalloc(&pdev->dev, sizeof(*dsi), GFP_KERNEL);
	if (!dsi) {
		dev_err(&pdev->dev, "failed to allocate dsi object.\n");
		return -ENOMEM;
	}

	spin_lock_init(&dsi->transfer_lock);
	INIT_LIST_HEAD(&dsi->transfer_list);

	dsi->pdev = pdev;
	dsi->dev = &pdev->dev;
	dsi->pd = pdev->dev.platform_data;

	if (dsi->pd == NULL && pdev->dev.of_node)
		dsi->pd = exynos_dsi_parse_dt(pdev);

	if (dsi->pd == NULL) {
		dev_err(&pdev->dev, "failed to get platform data for dsi.\n");
		return -EINVAL;
	}

	dsi->supplies[0].supply = "vdd11";
	dsi->supplies[1].supply = "vdd18";
	ret = devm_regulator_bulk_get(&pdev->dev,
				ARRAY_SIZE(dsi->supplies), dsi->supplies);
	if (ret) {
		dev_err(&pdev->dev, "Failed to get regulators: %d\n", ret);
		return ret;
	}

	dsi->pll_clk = devm_clk_get(&pdev->dev, "pll_clk");
	if (IS_ERR(dsi->pll_clk)) {
		dev_err(&pdev->dev, "failed to get dsi pll input clock\n");
		return -ENODEV;
	}

	dsi->bus_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(dsi->bus_clk)) {
		dev_err(&pdev->dev, "failed to get dsi bus clock\n");
		return -ENODEV;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to get io memory region\n");
		return -ENODEV;
	}

	dsi->reg_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!dsi->reg_base) {
		dev_err(&pdev->dev, "failed to remap io region\n");
		return -ENOMEM;
	}

	dsi->phy = devm_phy_get(&pdev->dev, "dsim");
	if (IS_ERR(dsi->phy))
		return PTR_ERR(dsi->phy);

	platform_set_drvdata(pdev, dsi);

	dsi->irq = platform_get_irq(pdev, 0);
	if (dsi->irq < 0) {
		dev_err(&pdev->dev, "failed to request dsi irq resource\n");
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(&pdev->dev, dsi->irq, NULL,
		exynos_dsi_irq, IRQF_ONESHOT, dev_name(&pdev->dev), dsi);
	if (ret) {
		dev_err(&pdev->dev, "failed to request dsi irq\n");
		return ret;
	}

	dsi->out.name = dev_name(&pdev->dev);
	dsi->out.id = -1;
	dsi->out.of_node = pdev->dev.of_node;
	dsi->out.common_ops = &exynos_dsi_common_ops;
	dsi->out.ops.dsi = &exynos_dsi_ops;

	ret = video_source_register(&dsi->out);
	if (ret) {
		dev_err(&pdev->dev, "failed to register video source\n");
		return ret;
	}

	return 0;
}

static int exynos_dsi_remove(struct platform_device *pdev)
{
	struct exynos_dsi *dsi = platform_get_drvdata(pdev);

	video_source_unregister(&dsi->out);

	return 0;
}

/*
 * Power management
 */

#ifdef CONFIG_PM_SLEEP
static int exynos_dsi_suspend(struct device *dev)
{
	struct exynos_dsi *dsi = dev_get_drvdata(dev);
	struct video_source *src = &dsi->out;
	int ret;

	if (dsi->enabled) {
		ret = display_entity_set_state(src->sink, DISPLAY_ENTITY_STATE_OFF);
		if (ret < 0)
			return -EBUSY;
	}

	return 0;
}

static int exynos_dsi_resume(struct device *dev)
{
	struct exynos_dsi *dsi = dev_get_drvdata(dev);
	struct video_source *src = &dsi->out;
	int ret;

	if (!dsi->enabled) {
		ret = display_entity_set_state(src->sink, DISPLAY_ENTITY_STATE_ON);
		if (ret < 0)
			return -EBUSY;
	}

	return 0;
}
#endif

static const struct dev_pm_ops exynos_dsi_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos_dsi_suspend, exynos_dsi_resume)
};

/*
 * Module
 */

static struct platform_driver exynos_dsi_driver = {
	.probe = exynos_dsi_probe,
	.remove = exynos_dsi_remove,
	.driver = {
		   .name = "exynos-dsi",
		   .owner = THIS_MODULE,
		   .pm = &exynos_dsi_pm_ops,
		   .of_match_table = of_match_ptr(exynos_dsi_of_match),
	},
};

module_platform_driver(exynos_dsi_driver);

MODULE_AUTHOR("Tomasz Figa <t.figa@samsung.com>");
MODULE_DESCRIPTION("Samsung SoC MIPI DSI Master");
MODULE_LICENSE("GPL");
