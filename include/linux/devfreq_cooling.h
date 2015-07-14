/*
 *  linux/include/linux/devfreq_cooling.h
 *
 *  Copyright (C) 2015 Samsung Electronics Co., Ltd
 *  Author: Chanwoo Choi <cw00.choi@samsung.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __DEVFREQ_COOLING_H__
#define __DEVFREQ_COOLING_H__

#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/devfreq.h>

#ifdef CONFIG_DEVFREQ_THERMAL
/**
 * devfreq_cooling_register - function to create cpufreq cooling device.
 * @devfreq:	the devfreq instance.
 */
struct thermal_cooling_device *
devfreq_cooling_register(struct devfreq *devfreq);

/**
 * of_devfreq_cooling_register - create cpufreq cooling device based on DT.
 * @np:		a valid struct device_node to the cooling device tree node
 * @devfreq:	the devfreq instance.
 */
#ifdef CONFIG_THERMAL_OF
struct thermal_cooling_device *
of_devfreq_cooling_register(struct device_node *np, struct devfreq *devfreq);
#else
static inline struct thermal_cooling_device *
of_devfreq_cooling_register(struct device_node *np, struct devfreq *devfreq)
{
	return ERR_PTR(-ENOSYS);
}
#endif

/**
 * devfreq_cooling_unregister - function to remove cpufreq cooling device.
 * @cdev: thermal cooling device pointer.
 */
void devfreq_cooling_unregister(struct thermal_cooling_device *cdev);

unsigned long devfreq_cooling_get_level(unsigned int cpu, unsigned int freq);
#else /* !CONFIG_DEVFREQ_THERMAL */
static inline struct thermal_cooling_device *
devfreq_cooling_register(struct devfreq *devfreq)
{
	return ERR_PTR(-ENOSYS);
}
static inline struct thermal_cooling_device *
of_devfreq_cooling_register(struct device_node *np, struct devfreq *devfreq)
{
	return ERR_PTR(-ENOSYS);
}

static inline
void devfreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
#endif	/* CONFIG_DEVFREQ_THERMAL */
#endif /* __DEVFREQ_COOLING_H__ */
