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
#define EXYNOS_4212_UPHYPWR			0x0

#define EXYNOS_4212_UPHYPWR_DEV_SUSPEND		(1 << 0)
#define EXYNOS_4212_UPHYPWR_DEV_PWR		(1 << 3)
#define EXYNOS_4212_UPHYPWR_DEV_OTG_PWR		(1 << 4)
#define EXYNOS_4212_UPHYPWR_DEV_SLEEP		(1 << 5)
#define EXYNOS_4212_UPHYPWR_DEV	( \
	EXYNOS_4212_UPHYPWR_DEV_SUSPEND | \
	EXYNOS_4212_UPHYPWR_DEV_PWR | \
	EXYNOS_4212_UPHYPWR_DEV_OTG_PWR | \
	EXYNOS_4212_UPHYPWR_DEV_SLEEP)

#define EXYNOS_4212_UPHYPWR_HOST_SUSPEND	(1 << 6)
#define EXYNOS_4212_UPHYPWR_HOST_PWR		(1 << 7)
#define EXYNOS_4212_UPHYPWR_HOST_SLEEP		(1 << 8)
#define EXYNOS_4212_UPHYPWR_HOST ( \
	EXYNOS_4212_UPHYPWR_HOST_SUSPEND | \
	EXYNOS_4212_UPHYPWR_HOST_PWR | \
	EXYNOS_4212_UPHYPWR_HOST_SLEEP)

#define EXYNOS_4212_UPHYPWR_HSCI0_SUSPEND	(1 << 9)
#define EXYNOS_4212_UPHYPWR_HSCI0_PWR		(1 << 10)
#define EXYNOS_4212_UPHYPWR_HSCI0_SLEEP		(1 << 11)
#define EXYNOS_4212_UPHYPWR_HSCI0 ( \
	EXYNOS_4212_UPHYPWR_HSCI0_SUSPEND | \
	EXYNOS_4212_UPHYPWR_HSCI0_PWR | \
	EXYNOS_4212_UPHYPWR_HSCI0_SLEEP)

#define EXYNOS_4212_UPHYPWR_HSCI1_SUSPEND	(1 << 12)
#define EXYNOS_4212_UPHYPWR_HSCI1_PWR		(1 << 13)
#define EXYNOS_4212_UPHYPWR_HSCI1_SLEEP		(1 << 14)
#define EXYNOS_4212_UPHYPWR_HSCI1 ( \
	EXYNOS_4212_UPHYPWR_HSCI1_SUSPEND | \
	EXYNOS_4212_UPHYPWR_HSCI1_PWR | \
	EXYNOS_4212_UPHYPWR_HSCI1_SLEEP)

/* PHY clock control */
#define EXYNOS_4212_UPHYCLK			0x4

#define EXYNOS_4212_UPHYCLK_PHYFSEL_MASK	(0x7 << 0)
#define EXYNOS_4212_UPHYCLK_PHYFSEL_9MHZ6	(0x0 << 0)
#define EXYNOS_4212_UPHYCLK_PHYFSEL_10MHZ	(0x1 << 0)
#define EXYNOS_4212_UPHYCLK_PHYFSEL_12MHZ	(0x2 << 0)
#define EXYNOS_4212_UPHYCLK_PHYFSEL_19MHZ2	(0x3 << 0)
#define EXYNOS_4212_UPHYCLK_PHYFSEL_20MHZ	(0x4 << 0)
#define EXYNOS_4212_UPHYCLK_PHYFSEL_24MHZ	(0x5 << 0)
#define EXYNOS_4212_UPHYCLK_PHYFSEL_50MHZ	(0x7 << 0)

#define EXYNOS_4212_UPHYCLK_PHY0_ID_PULLUP	(0x1 << 3)
#define EXYNOS_4212_UPHYCLK_PHY0_COMMON_ON	(0x1 << 4)
#define EXYNOS_4212_UPHYCLK_PHY1_COMMON_ON	(0x1 << 7)

#define EXYNOS_4212_UPHYCLK_HSIC_REFCLK_MASK	(0x7f << 10)
#define EXYNOS_4212_UPHYCLK_HSIC_REFCLK_12MHZ	(0x24 << 10)
#define EXYNOS_4212_UPHYCLK_HSIC_REFCLK_15MHZ	(0x1c << 10)
#define EXYNOS_4212_UPHYCLK_HSIC_REFCLK_16MHZ	(0x1a << 10)
#define EXYNOS_4212_UPHYCLK_HSIC_REFCLK_19MHZ2	(0x15 << 10)
#define EXYNOS_4212_UPHYCLK_HSIC_REFCLK_20MHZ	(0x14 << 10)

/* PHY reset control */
#define EXYNOS_4212_UPHYRST			0x8

#define EXYNOS_4212_URSTCON_DEVICE		(1 << 0)
#define EXYNOS_4212_URSTCON_OTG_HLINK		(1 << 1)
#define EXYNOS_4212_URSTCON_OTG_PHYLINK		(1 << 2)
#define EXYNOS_4212_URSTCON_HOST_PHY		(1 << 3)
#define EXYNOS_4212_URSTCON_PHY1		(1 << 4)
#define EXYNOS_4212_URSTCON_HSIC0		(1 << 5)
#define EXYNOS_4212_URSTCON_HSIC1		(1 << 6)
#define EXYNOS_4212_URSTCON_HOST_LINK_ALL	(1 << 7)
#define EXYNOS_4212_URSTCON_HOST_LINK_P0	(1 << 8)
#define EXYNOS_4212_URSTCON_HOST_LINK_P1	(1 << 9)
#define EXYNOS_4212_URSTCON_HOST_LINK_P2	(1 << 10)

/* Isolation, configured in the power management unit */
#define EXYNOS_4212_USB_ISOL_OFFSET		0x0
#define EXYNOS_4212_USB_ISOL_OTG		(1 << 0)
#define EXYNOS_4212_USB_ISOL_HSIC0_OFFSET	0x4
#define EXYNOS_4212_USB_ISOL_HSIC0		(1 << 0)
#define EXYNOS_4212_USB_ISOL_HSIC1_OFFSET	0x8
#define EXYNOS_4212_USB_ISOL_HSIC1		(1 << 0)

enum exynos4x12_phy_id {
	EXYNOS4212_DEVICE,
	EXYNOS4212_HOST,
	EXYNOS4212_HSIC0,
	EXYNOS4212_HSIC1,
	EXYNOS4212_NUM_PHYS,
};

/* exynos4212_rate_to_clk() converts the supplied clock rate to the value that
 * can be written to the phy register. */
static u32 exynos4212_rate_to_clk(unsigned long rate)
{
	unsigned int clksel;

	/* EXYNOS_4212_UPHYCLK_PHYFSEL_MASK */

	switch (rate) {
	case 9600 * KHZ:
		clksel = EXYNOS_4212_UPHYCLK_PHYFSEL_9MHZ6;
		break;
	case 10 * MHZ:
		clksel = EXYNOS_4212_UPHYCLK_PHYFSEL_10MHZ;
		break;
	case 12 * MHZ:
		clksel = EXYNOS_4212_UPHYCLK_PHYFSEL_12MHZ;
		break;
	case 19200 * KHZ:
		clksel = EXYNOS_4212_UPHYCLK_PHYFSEL_19MHZ2;
		break;
	case 20 * MHZ:
		clksel = EXYNOS_4212_UPHYCLK_PHYFSEL_20MHZ;
		break;
	case 24 * MHZ:
		clksel = EXYNOS_4212_UPHYCLK_PHYFSEL_24MHZ;
		break;
	case 50 * MHZ:
		clksel = EXYNOS_4212_UPHYCLK_PHYFSEL_50MHZ;
		break;
	default:
		clksel = CLKSEL_ERROR;
	}

	return clksel;
}

static void exynos4212_isol(struct uphy_instance *inst, bool on)
{
	struct uphy_driver *drv = inst->drv;
	u32 offset;
	u32 mask;
	u32 tmp;

	if (!drv->reg_isol)
		return;

	switch (inst->cfg->id) {
	case EXYNOS4212_DEVICE:
		offset = EXYNOS_4212_USB_ISOL_OFFSET;
		mask = EXYNOS_4212_USB_ISOL_OTG;
		break;
	case EXYNOS4212_HOST:
		offset = EXYNOS_4212_USB_ISOL_OFFSET;
		mask = EXYNOS_4212_USB_ISOL_OTG;
		break;
	case EXYNOS4212_HSIC0:
		offset = EXYNOS_4212_USB_ISOL_HSIC0_OFFSET;
		mask = EXYNOS_4212_USB_ISOL_HSIC0;
		break;
	case EXYNOS4212_HSIC1:
		offset = EXYNOS_4212_USB_ISOL_HSIC1_OFFSET;
		mask = EXYNOS_4212_USB_ISOL_HSIC1;
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

static void exynos4212_phy_pwr(struct uphy_instance *inst, bool on)
{
	struct uphy_driver *drv = inst->drv;
	u32 rstbits = 0;
	u32 phypwr = 0;
	u32 rst;
	u32 pwr;

	switch (inst->cfg->id) {
	case EXYNOS4212_DEVICE:
		phypwr =	EXYNOS_4212_UPHYPWR_DEV;
		rstbits =	EXYNOS_4212_URSTCON_DEVICE;
		break;
	case EXYNOS4212_HOST:
		phypwr =	EXYNOS_4212_UPHYPWR_HOST;
		rstbits =	EXYNOS_4212_URSTCON_HOST_PHY;
		break;
	case EXYNOS4212_HSIC0:
		phypwr =	EXYNOS_4212_UPHYPWR_HSCI0;
		rstbits =	EXYNOS_4212_URSTCON_HSIC1 |
				EXYNOS_4212_URSTCON_HOST_LINK_P0 |
				EXYNOS_4212_URSTCON_HOST_PHY;
		break;
	case EXYNOS4212_HSIC1:
		phypwr =	EXYNOS_4212_UPHYPWR_HSCI1;
		rstbits =	EXYNOS_4212_URSTCON_HSIC1 |
				EXYNOS_4212_URSTCON_HOST_LINK_P1;
		break;
	};

	if (on) {
		writel(inst->clk, drv->reg_phy + EXYNOS_4212_UPHYCLK);

		pwr = readl(drv->reg_phy + EXYNOS_4212_UPHYPWR);
		pwr &= ~phypwr;
		writel(pwr, drv->reg_phy + EXYNOS_4212_UPHYPWR);

		rst = readl(drv->reg_phy + EXYNOS_4212_UPHYRST);
		rst |= rstbits;
		writel(rst, drv->reg_phy + EXYNOS_4212_UPHYRST);
		udelay(10);
		rst &= ~rstbits;
		writel(rst, drv->reg_phy + EXYNOS_4212_UPHYRST);
	} else {
		pwr = readl(drv->reg_phy + EXYNOS_4212_UPHYPWR);
		pwr |= phypwr;
		writel(pwr, drv->reg_phy + EXYNOS_4212_UPHYPWR);
	}
}

static int exynos4212_power_on(struct uphy_instance *inst)
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

	exynos4212_isol(inst, 0);
	exynos4212_phy_pwr(inst, 1);

	/* Power on the device, as it is necessary for HSIC to work */
	if (inst->cfg->id == EXYNOS4212_HSIC0) {
		struct uphy_instance *device =
					&drv->uphy_instances[EXYNOS4212_DEVICE];
		device->ref_cnt++;
		if (device->ref_cnt > 1)
			return 0;
		exynos4212_phy_pwr(device, 1);
		exynos4212_isol(device, 0);
	}

	return 0;
}

static int exynos4212_power_off(struct uphy_instance *inst)
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

	exynos4212_phy_pwr(inst, 0);
	exynos4212_isol(inst, 1);

	if (inst->cfg->id == EXYNOS4212_HSIC0) {
		struct uphy_instance *device =
					&drv->uphy_instances[EXYNOS4212_DEVICE];
		device->ref_cnt--;
		if (device->ref_cnt > 0)
			return 0;
		exynos4212_phy_pwr(device, 0);
		exynos4212_isol(device, 1);
	}

	return 0;
}


static const struct common_phy exynos4212_phys[] = {
	{
		.label		= "device",
		.type		= PHY_DEVICE,
		.id		= EXYNOS4212_DEVICE,
		.rate_to_clk	= exynos4212_rate_to_clk,
		.power_on	= exynos4212_power_on,
		.power_off	= exynos4212_power_off,
	},
	{
		.label		= "host",
		.type		= PHY_HOST,
		.id		= EXYNOS4212_HOST,
		.rate_to_clk	= exynos4212_rate_to_clk,
		.power_on	= exynos4212_power_on,
		.power_off	= exynos4212_power_off,
	},
	{
		.label		= "hsic0",
		.type		= PHY_HOST,
		.id		= EXYNOS4212_HSIC0,
		.rate_to_clk	= exynos4212_rate_to_clk,
		.power_on	= exynos4212_power_on,
		.power_off	= exynos4212_power_off,
	},
	{
		.label		= "hsic1",
		.type		= PHY_HOST,
		.id		= EXYNOS4212_HSIC1,
		.rate_to_clk	= exynos4212_rate_to_clk,
		.power_on	= exynos4212_power_on,
		.power_off	= exynos4212_power_off,
	},
	{},
};

const struct uphy_config exynos4212_uphy_config = {
	.cpu		= TYPE_EXYNOS4212,
	.num_phys	= EXYNOS4212_NUM_PHYS,
	.phys		= exynos4212_phys,
};

