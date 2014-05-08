/*
 * Samsung S5P/EXYNOS SoC series USB PHY driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Author: Kamil Debski <k.debski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include "phy-exynos-usb.h"

/* Exynos USB PHY registers */

/* PHY power control */
#define EXYNOS_4210_UPHYPWR			0x0

#define EXYNOS_4210_UPHYPWR_PHY0_SUSPEND	(1 << 0)
#define EXYNOS_4210_UPHYPWR_PHY0_PWR		(1 << 3)
#define EXYNOS_4210_UPHYPWR_PHY0_OTG_PWR	(1 << 4)
#define EXYNOS_4210_UPHYPWR_PHY0_SLEEP		(1 << 5)
#define EXYNOS_4210_UPHYPWR_PHY0	( \
	EXYNOS_4210_UPHYPWR_PHY0_SUSPEND | \
	EXYNOS_4210_UPHYPWR_PHY0_PWR | \
	EXYNOS_4210_UPHYPWR_PHY0_OTG_PWR | \
	EXYNOS_4210_UPHYPWR_PHY0_SLEEP)

#define EXYNOS_4210_UPHYPWR_PHY1_SUSPEND	(1 << 6)
#define EXYNOS_4210_UPHYPWR_PHY1_PWR		(1 << 7)
#define EXYNOS_4210_UPHYPWR_PHY1_SLEEP		(1 << 8)
#define EXYNOS_4210_UPHYPWR_PHY1 ( \
	EXYNOS_4210_UPHYPWR_PHY1_SUSPEND | \
	EXYNOS_4210_UPHYPWR_PHY1_PWR | \
	EXYNOS_4210_UPHYPWR_PHY1_SLEEP)

#define EXYNOS_4210_UPHYPWR_HSCI0_SUSPEND	(1 << 9)
#define EXYNOS_4210_UPHYPWR_HSCI0_SLEEP		(1 << 10)
#define EXYNOS_4210_UPHYPWR_HSCI0 ( \
	EXYNOS_4210_UPHYPWR_HSCI0_SUSPEND | \
	EXYNOS_4210_UPHYPWR_HSCI0_SLEEP)

#define EXYNOS_4210_UPHYPWR_HSCI1_SUSPEND	(1 << 11)
#define EXYNOS_4210_UPHYPWR_HSCI1_SLEEP		(1 << 12)
#define EXYNOS_4210_UPHYPWR_HSCI1 ( \
	EXYNOS_4210_UPHYPWR_HSCI1_SUSPEND | \
	EXYNOS_4210_UPHYPWR_HSCI1_SLEEP)

/* PHY clock control */
#define EXYNOS_4210_UPHYCLK			0x4

#define EXYNOS_4210_UPHYCLK_PHYFSEL_MASK	(0x3 << 0)
#define EXYNOS_4210_UPHYCLK_PHYFSEL_48MHZ	(0x0 << 0)
#define EXYNOS_4210_UPHYCLK_PHYFSEL_24MHZ	(0x3 << 0)
#define EXYNOS_4210_UPHYCLK_PHYFSEL_12MHZ	(0x2 << 0)

#define EXYNOS_4210_UPHYCLK_PHY0_ID_PULLUP	(0x1 << 2)
#define EXYNOS_4210_UPHYCLK_PHY0_COMMON_ON	(0x1 << 4)
#define EXYNOS_4210_UPHYCLK_PHY1_COMMON_ON	(0x1 << 7)

/* PHY reset control */
#define EXYNOS_4210_UPHYRST			0x8

#define EXYNOS_4210_URSTCON_PHY0		(1 << 0)
#define EXYNOS_4210_URSTCON_OTG_HLINK		(1 << 1)
#define EXYNOS_4210_URSTCON_OTG_PHYLINK		(1 << 2)
#define EXYNOS_4210_URSTCON_PHY1_ALL		(1 << 3)
#define EXYNOS_4210_URSTCON_PHY1_P0		(1 << 4)
#define EXYNOS_4210_URSTCON_PHY1_P1P2		(1 << 5)
#define EXYNOS_4210_URSTCON_HOST_LINK_ALL	(1 << 6)
#define EXYNOS_4210_URSTCON_HOST_LINK_P0	(1 << 7)
#define EXYNOS_4210_URSTCON_HOST_LINK_P1	(1 << 8)
#define EXYNOS_4210_URSTCON_HOST_LINK_P2	(1 << 9)

/* Isolation, configured in the power management unit */
#define EXYNOS_4210_USB_ISOL_DEVICE_OFFSET	0x0
#define EXYNOS_4210_USB_ISOL_DEVICE		(1 << 0)
#define EXYNOS_4210_USB_ISOL_HOST_OFFSET	0x4
#define EXYNOS_4210_USB_ISOL_HOST		(1 << 0)

/* USBYPHY1 Floating prevention */
#define EXYNOS_4210_UPHY1CON			0x34
#define EXYNOS_4210_UPHY1CON_FLOAT_PREVENTION	0x1

enum exynos4210_phy_id {
	EXYNOS4210_DEVICE,
	EXYNOS4210_HOST,
	EXYNOS4210_HSIC0,
	EXYNOS4210_HSIC1,
	EXYNOS4210_NUM_PHYS,
};

/* exynos4210_rate_to_clk() converts the supplied clock rate to the value that
 * can be written to the phy register. */
static u32 exynos4210_rate_to_clk(unsigned long rate)
{
	unsigned int clksel;

	switch (rate) {
	case 12 * MHZ:
		clksel = EXYNOS_4210_UPHYCLK_PHYFSEL_12MHZ;
		break;
	case 24 * MHZ:
		clksel = EXYNOS_4210_UPHYCLK_PHYFSEL_24MHZ;
		break;
	case 48 * MHZ:
		clksel = EXYNOS_4210_UPHYCLK_PHYFSEL_48MHZ;
		break;
	default:
		clksel = CLKSEL_ERROR;
	}

	return clksel;
}

static void exynos4210_isol(struct uphy_instance *inst, bool on)
{
	struct uphy_driver *drv = inst->drv;
	u32 offset;
	u32 mask;
	u32 tmp;

	if (!drv->reg_isol)
		return;

	switch (inst->cfg->id) {
	case EXYNOS4210_DEVICE:
		offset = EXYNOS_4210_USB_ISOL_DEVICE_OFFSET;
		mask = EXYNOS_4210_USB_ISOL_DEVICE;
		break;
	case EXYNOS4210_HOST:
		offset = EXYNOS_4210_USB_ISOL_HOST_OFFSET;
		mask = EXYNOS_4210_USB_ISOL_HOST;
		break;
	default:
		return;
	};

	tmp = readl(drv->reg_isol + offset);
	if (on)
		tmp &= ~mask;
	else
		tmp |= mask;
	writel(tmp, drv->reg_isol + offset);
}

static void exynos4210_phy_pwr(struct uphy_instance *inst, bool on)
{
	struct uphy_driver *drv = inst->drv;
	u32 rstbits = 0;
	u32 phypwr = 0;
	u32 rst;
	u32 pwr;

	switch (inst->cfg->id) {
	case EXYNOS4210_DEVICE:
		phypwr =	EXYNOS_4210_UPHYPWR_PHY0;
		rstbits =	EXYNOS_4210_URSTCON_PHY0;
		break;
	case EXYNOS4210_HOST:
		phypwr =	EXYNOS_4210_UPHYPWR_PHY1;
		rstbits =	EXYNOS_4210_URSTCON_PHY1_ALL |
				EXYNOS_4210_URSTCON_PHY1_P0 |
				EXYNOS_4210_URSTCON_PHY1_P1P2 |
				EXYNOS_4210_URSTCON_HOST_LINK_ALL |
				EXYNOS_4210_URSTCON_HOST_LINK_P0;
		writel(on, drv->reg_phy + EXYNOS_4210_UPHY1CON);
		break;
	case EXYNOS4210_HSIC0:
		phypwr =	EXYNOS_4210_UPHYPWR_HSCI0;
		rstbits =	EXYNOS_4210_URSTCON_PHY1_P1P2 |
				EXYNOS_4210_URSTCON_HOST_LINK_P1;
		break;
	case EXYNOS4210_HSIC1:
		phypwr =	EXYNOS_4210_UPHYPWR_HSCI1;
		rstbits =	EXYNOS_4210_URSTCON_PHY1_P1P2 |
				EXYNOS_4210_URSTCON_HOST_LINK_P2;
		break;
	};

	if (on) {
		writel(inst->clk, drv->reg_phy + EXYNOS_4210_UPHYCLK);

		pwr = readl(drv->reg_phy + EXYNOS_4210_UPHYPWR);
		pwr &= ~phypwr;
		writel(pwr, drv->reg_phy + EXYNOS_4210_UPHYPWR);

		rst = readl(drv->reg_phy + EXYNOS_4210_UPHYRST);
		rst |= rstbits;
		writel(rst, drv->reg_phy + EXYNOS_4210_UPHYRST);
		udelay(10);
		rst &= ~rstbits;
		writel(rst, drv->reg_phy + EXYNOS_4210_UPHYRST);
	} else {
		pwr = readl(drv->reg_phy + EXYNOS_4210_UPHYPWR);
		pwr |= phypwr;
		writel(pwr, drv->reg_phy + EXYNOS_4210_UPHYPWR);
	}
}

static int exynos4210_power_on(struct uphy_instance *inst)
{
	struct uphy_driver *drv = inst->drv;

	if (inst->state == STATE_ON) {
		dev_err(drv->dev, "usb phy \"%s\" already on",
							inst->cfg->label);
		return -ENODEV;
	}
	inst->state = STATE_ON;
	inst->ref_cnt++;
	if (inst->ref_cnt > 1)
		return 0;

	exynos4210_isol(inst, 0);
	exynos4210_phy_pwr(inst, 1);

	/* Power on the device, as it is necessary for HSIC to work */
	if (inst->cfg->id == EXYNOS4210_HOST) {
		struct uphy_instance *device =
					&drv->uphy_instances[EXYNOS4210_DEVICE];
		device->ref_cnt++;
		if (device->ref_cnt > 1)
			return 0;
		exynos4210_phy_pwr(device, 1);
		exynos4210_isol(device, 0);
	}

	return 0;
}

static int exynos4210_power_off(struct uphy_instance *inst)
{
	struct uphy_driver *drv = inst->drv;

	if (inst->state == STATE_OFF) {
		dev_err(drv->dev, "usb phy \"%s\" already off",
							inst->cfg->label);
		return -EINVAL;
	}

	inst->state = STATE_OFF;
	inst->ref_cnt--;
	if (inst->ref_cnt > 0)
		return 0;

	exynos4210_phy_pwr(inst, 0);
	exynos4210_isol(inst, 1);

	if (inst->cfg->id == EXYNOS4210_HOST) {
		struct uphy_instance *device =
					&drv->uphy_instances[EXYNOS4210_DEVICE];
		device->ref_cnt--;
		if (device->ref_cnt > 0)
			return 0;
		exynos4210_phy_pwr(device, 0);
		exynos4210_isol(device, 1);
	}

	return 0;
}


static const struct common_phy exynos4210_phys[] = {
	{
		.label		= "device",
		.type		= PHY_DEVICE,
		.id		= EXYNOS4210_DEVICE,
		.rate_to_clk	= exynos4210_rate_to_clk,
		.power_on	= exynos4210_power_on,
		.power_off	= exynos4210_power_off,
	},
	{
		.label		= "host",
		.type		= PHY_HOST,
		.id		= EXYNOS4210_HOST,
		.rate_to_clk	= exynos4210_rate_to_clk,
		.power_on	= exynos4210_power_on,
		.power_off	= exynos4210_power_off,
	},
	{
		.label		= "hsic0",
		.type		= PHY_HOST,
		.id		= EXYNOS4210_HSIC0,
		.rate_to_clk	= exynos4210_rate_to_clk,
		.power_on	= exynos4210_power_on,
		.power_off	= exynos4210_power_off,
	},
	{
		.label		= "hsic1",
		.type		= PHY_HOST,
		.id		= EXYNOS4210_HSIC1,
		.rate_to_clk	= exynos4210_rate_to_clk,
		.power_on	= exynos4210_power_on,
		.power_off	= exynos4210_power_off,
	},
	{},
};

const struct uphy_config exynos4210_uphy_config = {
	.cpu		= TYPE_EXYNOS4210,
	.num_phys	= EXYNOS4210_NUM_PHYS,
	.phys		= exynos4210_phys,
};

