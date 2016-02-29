/*
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifndef _DISPFREQ_H_
#define _DISPFREQ_H_

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <plat/clock.h>
#include <plat/clock-clksrc.h>

#define reg_mask(shift, size)	((0xffffffff >> (32 - size)) << shift)

struct dispfreq_device;

struct dispfreq_ops {
	struct clk *(*get_clk)(struct dispfreq_device *dd);
	int (*set_clk)(struct dispfreq_device *dd,
		struct clksrc_clk *clksrc, int div);
	int (*get_fimd_div)(struct dispfreq_device *dd);
	u32 (*get_refresh)(struct dispfreq_device *dd);
	int (*get_pm_state)(struct dispfreq_device *dd);
};

struct dispfreq_properties {
	struct fb_videomode *timing;
	u32 refresh;
	u32 max_refresh;
	u32 min_refresh;
};

struct dispfreq_info {
	u32 hz;
	u32 vclk;
	u32 cmu_div;
	u32 pixclock;
};

struct dispfreq_device {
	struct dispfreq_properties props;

	struct mutex ops_lock;
	const struct dispfreq_ops *ops;

	u32 vscr;
	struct device dev;
};

#define to_dispfreq_device(obj) container_of(obj, struct dispfreq_device, dev)

extern struct dispfreq_device *dispfreq_device_register(const char *name,
		struct device *parent_dev, void *devdata,
		const struct dispfreq_ops *ops,
		const struct dispfreq_properties *props);
extern void dispfreq_device_unregister(struct dispfreq_device *dd);

static inline void *dispfreq_get_data(struct dispfreq_device *dd_dev)
{
	return dev_get_drvdata(&dd_dev->dev);
}

#endif /* _DISPFREQ_H_ */
