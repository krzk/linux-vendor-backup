/*
 *  linux/drivers/devfreq/governor_simpleondemand.c
 *
 *  Copyright (C) 2011 Samsung Electronics
 *	MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/pm_qos.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "governor.h"

struct devfreq_exynos_bus_notifier_block {
	struct list_head node;
	struct notifier_block nb;
	struct devfreq *df;
};

static LIST_HEAD(devfreq_exynos_bus_list);
static DEFINE_MUTEX(devfreq_exynos_bus_mutex);

static int devfreq_exynos_bus_notifier(struct notifier_block *nb, unsigned long val,
						void *v)
{
	struct devfreq_exynos_bus_notifier_block *pq_nb;
	struct devfreq_exynos_bus_data *exynos_bus_data;

	pq_nb = container_of(nb, struct devfreq_exynos_bus_notifier_block, nb);

	exynos_bus_data = pq_nb->df->data;

	mutex_lock(&pq_nb->df->lock);
	update_devfreq(pq_nb->df);
	mutex_unlock(&pq_nb->df->lock);

	return NOTIFY_OK;
}

/* Default constants for DevFreq-Simple-Ondemand (DFSO) */
#define DFSO_UPTHRESHOLD	(30)
static int devfreq_exynos_bus_func(struct devfreq *df,
					unsigned long *freq)
{
	struct devfreq_dev_status stat;
	int err = df->profile->get_dev_status(df->dev.parent, &stat);
	unsigned long long a, b;
	unsigned int dfso_upthreshold = DFSO_UPTHRESHOLD;
	struct devfreq_exynos_bus_data *data = df->data;
	unsigned long max = (df->max_freq) ? df->max_freq : UINT_MAX;
	unsigned long pm_qos_min;
	unsigned int usage;

	if (!data)
		return -EINVAL;

	pm_qos_min = pm_qos_request(data->pm_qos_class);

	if (err)
		return err;

	if (data->upthreshold)
		dfso_upthreshold = data->upthreshold;

	if (dfso_upthreshold > 100)
		return -EINVAL;

	if (data->cal_qos_max)
		max = (df->max_freq) ? df->max_freq : 0;

	/* Assume MAX if it is going to be divided by zero */
	if (stat.total_time == 0) {
		if (data->cal_qos_max)
			max = max3(max, data->cal_qos_max, pm_qos_min);
		*freq = max;
		return 0;
	}

	/* Prevent overflow */
	if (stat.busy_time >= (1 << 24) || stat.total_time >= (1 << 24)) {
		stat.busy_time >>= 7;
		stat.total_time >>= 7;
	}

	/* Set MAX if we do not know the initial frequency */
	if (stat.current_frequency == 0) {
		if (data->cal_qos_max)
			max = max3(max, data->cal_qos_max, pm_qos_min);
		*freq = max;
		return 0;
	}

	usage = div64_u64(stat.busy_time * 100, stat.total_time);

	/* Set the desired frequency based on the load */
	a = stat.busy_time;
	a *= stat.current_frequency;
	b = div64_u64(a, stat.total_time);
	b *= 100;
	b = div64_u64(b, dfso_upthreshold);

	if (data->cal_qos_max) {
		if (b > data->cal_qos_max)
			b = data->cal_qos_max;
	}

	*freq = (unsigned long) b;

	/* compare calculated freq and pm_qos_min */
	if (pm_qos_min)
		*freq = max(pm_qos_min, *freq);

	if (df->min_freq && *freq < df->min_freq)
		*freq = df->min_freq;
	if (df->max_freq && *freq > df->max_freq)
		*freq = df->max_freq;

	return 0;
}

static int devfreq_exynos_bus_init(struct devfreq *df)
{
	int ret;
	struct devfreq_exynos_bus_notifier_block *pq_nb;
	struct devfreq_exynos_bus_data *data = df->data;

	if (!data)
		return -EINVAL;

	pq_nb = kzalloc(sizeof(*pq_nb), GFP_KERNEL);
	if (!pq_nb)
		return -ENOMEM;

	pq_nb->df = df;
	pq_nb->nb.notifier_call = devfreq_exynos_bus_notifier;
	INIT_LIST_HEAD(&pq_nb->node);

	ret = pm_qos_add_notifier(data->pm_qos_class, &pq_nb->nb);
	if (ret < 0)
		goto err;

	mutex_lock(&devfreq_exynos_bus_mutex);
	list_add_tail(&pq_nb->node, &devfreq_exynos_bus_list);
	mutex_unlock(&devfreq_exynos_bus_mutex);

	return 0;
err:
	kfree(pq_nb);

	return ret;
}

const struct devfreq_governor devfreq_exynos_bus = {
	.name = "exynos_bus",
	.get_target_freq = devfreq_exynos_bus_func,
	.init = devfreq_exynos_bus_init,
};
