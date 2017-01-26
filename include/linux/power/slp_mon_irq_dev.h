/*
 * To log which interrupt wake AP up.
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Sanghyeon Lee <sirano06.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _SLP_MON_IRQ_DEV_H
#define _SLP_MON_IRQ_DEV_H

enum SLEEP_MON_IRQ_DEV_STATUS {
	SLP_MON_IRQ_NOT_ACTIVE = 0,
	SLP_MON_IRQ_ACTIVE = 1,
};

#define SLEEP_MON_IRQ_NAME_LENGTH 15
#define UNKNOWN_IRQ_NAME "UNKNOWN"
#define UNKNOWN_IRQ_NAME_LENGTH 7

extern int add_slp_mon_irq_list(int irq, char *name);

#endif /* _SLP_MON_IRQ_DEV_H */


