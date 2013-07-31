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

#ifndef _PHY_SAMSUNG_NEW_H
#define _PHY_SAMSUNG_NEW_H

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

#define CLKSEL_ERROR                       -1

#ifndef KHZ
#define KHZ 1000
#endif

#ifndef MHZ
#define MHZ (KHZ * KHZ)
#endif

enum phy_type {
	PHY_DEVICE,
	PHY_HOST,
};

enum samsung_cpu_type {
	TYPE_S3C64XX,
	TYPE_EXYNOS4210,
	TYPE_EXYNOS4212,
	TYPE_EXYNOS5250,
};

enum uphy_state {
	STATE_OFF,
	STATE_ON,
};

struct uphy_driver;
struct uphy_instance;
struct uphy_config;

struct uphy_instance {
	struct uphy_driver *drv;
	struct phy *phy;
	const struct common_phy *cfg;
	enum uphy_state state;
	int ref_cnt;
	u32 clk;
	unsigned long rate;
};

struct uphy_driver {
	struct device *dev;
	spinlock_t lock;
	void __iomem *reg_phy;
	void __iomem *reg_isol;
	void __iomem *reg_mode;
	const struct uphy_config *cfg;
	struct clk *clk;
	struct uphy_instance uphy_instances[0];
};

struct common_phy {
	char *label;
	enum phy_type type;
	unsigned int id;
	u32 (*rate_to_clk)(unsigned long);
	int (*power_on)(struct uphy_instance*);
	int (*power_off)(struct uphy_instance*);
};


struct uphy_config {
	enum samsung_cpu_type cpu;
	int num_phys;
	const struct common_phy *phys;
};

#endif

