/* include/linux/sensorhub_stat.h
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SENSORHUB_STAT_H
#define __SENSORHUB_STAT_H

/* Contains definitions for sensorhub resource tracking per sensorhub lib number. */
#ifdef CONFIG_SENSORHUB_STAT
int sensorhub_stat_rcv(const char *dataframe, int size);
#else
#define sensorhub_stat_rcv(const char *dataframe, int size) do {} while (0);
#endif

#endif /* __SENSORHUB_STAT_H */
