/*
 * Renesas R61505-based Display Panels
 *
 * Copyright (C) 2012 Renesas Solutions Corp.
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __PANEL_S6E8AX0_H__
#define __PANEL_S6E8AX0_H__

#include <video/videomode.h>

struct s6d6aa1_platform_data {
	unsigned long width;		/* Panel width in mm */
	unsigned long height;		/* Panel height in mm */
	struct videomode mode;

	/* reset lcd panel device. */
	int (*reset)(struct device *dev);

	/* it indicates whether lcd panel was enabled
	   from bootloader or not. */
	int lcd_enabled;
	/* it means delay for stable time when it becomes low to high
	   or high to low that is dependent on whether reset gpio is
	   low active or high active. */
	unsigned int reset_delay;
	/* stable time needing to become lcd power on. */
	unsigned int power_on_delay;
	/* stable time needing to become lcd power off. */
	unsigned int power_off_delay;
	/* panel is reversed */
	bool flip_vertical;
	bool flip_horizontal;
};

#endif /* __PANEL_S6E8AX0_H__ */
