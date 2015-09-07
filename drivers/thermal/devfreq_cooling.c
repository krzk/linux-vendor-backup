/*
 *  linux/drivers/thermal/devfreq_cooling.c
 *
 *  Copyright (C) 2015 Samsung Electronics Co., Ltd
 *  Author: Chanwoo Choi <cw00.choi@samsung.com>
 *
 *  This driver is based on drivers/thermal/cpu_cooling.c.
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
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/devfreq.h>
#include <linux/devfreq_cooling.h>

/*
 * Cooling state <-> devfreq frequency
 *
 * Cooling states are translated to frequencies throughout this driver and this
 * is the relation between them.
 *
 * Highest cooling state corresponds to lowest possible frequency.
 *
 * i.e.
 *	level 0 --> 1st Max Freq
 *	level 1 --> 2nd Max Freq
 *	...
 */

/**
 * struct devfreq_cooling_device - data for cooling device with devfreq
 * @cool_dev:	thermal_cooling_device pointer to keep track of the
 *		registered cooling device.
 * @devfreq:	the devfreq instance.
 * @cur_state:	integer value representing the current state of devfreq
 *		cooling	devices.
 * @max_state:	interger value representing the maximum state of devfreq
 *		cooling devices.
 *
 * This structure is required for keeping information of each registered
 * devfreq_cooling_device.
 */
struct devfreq_cooling_device {
	struct thermal_cooling_device *cool_dev;
	struct devfreq *devfreq;

	unsigned int cur_state;
	unsigned int max_state;

	unsigned int *freq_table;	/* In descending order */
};

/* devfreq cooling device callback functions are defined below */

/**
 * devfreq_get_max_state - callback function to get the max cooling state.
 * @cdev:	thermal cooling device pointer.
 * @state:	fill this variable with the max cooling state.
 *
 * Callback for the thermal cooling device to return the devfreq
 * max cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int devfreq_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct devfreq_cooling_device *devfreq_device = cdev->devdata;

	*state = devfreq_device->max_state;

	return 0;
}

/**
 * devfreq_get_cur_state - callback function to get the current cooling state.
 * @cdev:	thermal cooling device pointer.
 * @state:	fill this variable with the current cooling state.
 *
 * Callback for the thermal cooling device to return the devfreq
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int devfreq_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct devfreq_cooling_device *devfreq_device = cdev->devdata;

	*state = devfreq_device->cur_state;

	return 0;
}

/**
 * devfreq_set_cur_state - callback function to set the current cooling state.
 * @cdev:	thermal cooling device pointer.
 * @state:	set this variable to the current cooling state.
 *
 * Callback for the thermal cooling device to change the devfreq
 * current cooling state.
 *
 * Return: 0 on success, an error code otherwise.
 */
static int devfreq_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct devfreq_cooling_device *devfreq_dev = cdev->devdata;
	unsigned int limited_freq;
	int ret;

	/* Request state should be less than max_level */
	if (WARN_ON(state > devfreq_dev->max_state))
		return -EINVAL;

	/* Check if the old cooling action is same as new cooling action */
	if (devfreq_dev->cur_state == state)
		return 0;

	limited_freq = devfreq_dev->freq_table[state];

	devfreq_dev->cur_state = state;

	/* Set the limited frequency to maximum frequency of devfreq */
	devfreq_dev->devfreq->max_freq = limited_freq;
	mutex_lock(&devfreq_dev->devfreq->lock);
	ret = update_devfreq(devfreq_dev->devfreq);
	mutex_unlock(&devfreq_dev->devfreq->lock);

	return ret;
}

/* Bind devfreq callbacks to thermal cooling device ops */
static struct thermal_cooling_device_ops const devfreq_cooling_ops = {
	.get_max_state = devfreq_get_max_state,
	.get_cur_state = devfreq_get_cur_state,
	.set_cur_state = devfreq_set_cur_state,
};

/**
 * __devfreq_cooling_register - helper function to create devfreq cooling device
 * @np:		a valid struct device_node to the cooling device tree node
 * @devfreq:	the devfreq instance.
 *
 * This interface function registers the devfreq cooling device with the name
 * "thermal-devfreq-%x". This api can support multiple instances of devfreq
 * cooling devices. It also gives the opportunity to link the cooling device
 * with a device tree node, in order to bind it via the thermal DT code.
 *
 * Return: a valid struct thermal_cooling_device pointer on success,
 * on failure, it returns a corresponding ERR_PTR().
 */
static struct thermal_cooling_device *
__devfreq_cooling_register(struct device_node *np, struct devfreq *devfreq)
{
	struct thermal_cooling_device *cool_dev;
	struct devfreq_cooling_device *devfreq_dev;
	struct devfreq_dev_profile *devfreq_profile = devfreq->profile;
	struct dev_pm_opp *opp;
	static atomic_t devfreq_cooling_no = ATOMIC_INIT(-1);
	char dev_name[THERMAL_NAME_LENGTH];
	unsigned long freq;
	int i;

	devfreq_dev = kzalloc(sizeof(*devfreq_dev), GFP_KERNEL);
	if (!devfreq_dev)
		return ERR_PTR(-ENOMEM);

	rcu_read_lock();
	devfreq_dev->max_state = dev_pm_opp_get_opp_count(devfreq->dev.parent);
	if (devfreq_dev->max_state <= 0) {
		rcu_read_unlock();
		cool_dev = ERR_PTR(-EINVAL);
		goto free_cdev;
	}
	rcu_read_unlock();

	devfreq_dev->max_state -= 1;

	/*
	 * Use the freq_table of devfreq_dev_profile structure
	 * if the devfreq_dev_profile includes already filled frequency table.
	 */
	if (devfreq_profile->freq_table) {
		devfreq_dev->freq_table = devfreq_profile->freq_table;
		goto register_cooling_dev;
	}

	/* Allocate the frequency table and fill it */
	rcu_read_lock();
	devfreq_dev->freq_table = kzalloc(sizeof(*devfreq_dev->freq_table) *
					devfreq_dev->max_state + 1, GFP_KERNEL);
	if (!devfreq_dev->freq_table) {
		rcu_read_unlock();
		cool_dev = ERR_PTR(-ENOMEM);
		goto free_cdev;
	}

	freq = ULONG_MAX;
	for (i = 0; i <= devfreq_dev->max_state; i++, freq--) {
		opp = dev_pm_opp_find_freq_floor(devfreq->dev.parent,
						&freq);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			cool_dev = ERR_PTR(-EINVAL);
			goto free_table;
		}
		devfreq_dev->freq_table[i] = freq;
	}
	rcu_read_unlock();

register_cooling_dev:
	/* Register cooling device with devfreq device */
	snprintf(dev_name, sizeof(dev_name), "thermal-devfreq-%d",
			atomic_inc_return(&devfreq_cooling_no));

	cool_dev = thermal_of_cooling_device_register(np, dev_name, devfreq_dev,
						      &devfreq_cooling_ops);
	if (IS_ERR(cool_dev))
		goto free_table;

	devfreq_dev->devfreq = devfreq;
	devfreq_dev->cool_dev = cool_dev;

	return cool_dev;

free_table:
	kfree(devfreq_dev->freq_table);
free_cdev:
	kfree(devfreq_dev);

	return cool_dev;
}

/**
 * of_devfreq_cooling_register - function to create devfreq cooling device.
 * @np:		a valid struct device_node to the cooling device tree node
 * @devfreq:	the devfreq instance.
 *
 * This interface function registers the devfreq cooling device with the name
 * "thermal-devfreq-%x". This api can support multiple instances of devfreq
 * cooling devices. Using this API, the devfreq cooling device will be
 * linked to the device tree node provided.
 *
 * Return: a valid struct thermal_cooling_device pointer on success,
 * on failure, it returns a corresponding ERR_PTR().
 */
struct thermal_cooling_device *
of_devfreq_cooling_register(struct device_node *np, struct devfreq *devfreq)
{
	if (!np || !devfreq)
		return ERR_PTR(-EINVAL);

	return __devfreq_cooling_register(np, devfreq);
}
EXPORT_SYMBOL_GPL(of_devfreq_cooling_register);

/**
 * devfreq_cooling_register - function to create devfreq cooling device.
 * @devfreq:	the devfreq instance.
 *
 * This interface function registers the devfreq cooling device with the name
 * "thermal-devfreq-%x". This api can support multiple instances of devfreq
 * cooling devices.
 *
 * Return: a valid struct thermal_cooling_device pointer on success,
 * on failure, it returns a corresponding ERR_PTR().
 */
struct thermal_cooling_device *
devfreq_cooling_register(struct devfreq *devfreq)
{
	if (!devfreq)
		return ERR_PTR(-EINVAL);

	return __devfreq_cooling_register(NULL, devfreq);
}
EXPORT_SYMBOL_GPL(devfreq_cooling_register);

/**
 * devfreq_cooling_unregister - function to remove devfreq cooling device.
 * @cdev: thermal cooling device pointer.
 *
 * This interface function unregisters the "thermal-devfreq-%x" cooling device.
 */
void devfreq_cooling_unregister(struct thermal_cooling_device *cdev)
{
	struct devfreq_cooling_device *devfreq_dev;

	if (!cdev)
		return;

	devfreq_dev = cdev->devdata;

	thermal_cooling_device_unregister(devfreq_dev->cool_dev);
	kfree(devfreq_dev->freq_table);
	kfree(devfreq_dev);
}
EXPORT_SYMBOL_GPL(devfreq_cooling_unregister);
