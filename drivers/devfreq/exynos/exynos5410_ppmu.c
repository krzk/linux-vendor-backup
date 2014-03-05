/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *	Jonghwan Choi <Jonghwan Choi@samsung.com>
 *
 * EXYNOS - PPMU polling support
 *	This version supports EXYNOS5250 only. This changes bus frequencies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/hrtimer.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <mach/map.h>

#include "exynos_ppmu.h"
#include "exynos5410_ppmu.h"

enum exynos5410_ppmu_list {
	PPMU_DDR_C,
	PPMU_DDR_R1,
	PPMU_DDR_L,
	PPMU_RIGHT,
	PPMU_CPU,
	PPMU_END,
};

static DEFINE_SPINLOCK(exynos5410_ppmu_lock);
static LIST_HEAD(exynos5410_ppmu_handle_list);

struct exynos5410_ppmu_handle {
	struct list_head node;
	struct exynos_ppmu ppmu[PPMU_END];
};

static struct exynos_ppmu ppmu[PPMU_END] = {
	[PPMU_DDR_C] = {
		.hw_base = S5P_VA_PPMU_DDR_C,
	},
	[PPMU_DDR_R1] = {
		.hw_base = S5P_VA_PPMU_DDR_R1,
	},
	[PPMU_DDR_L] = {
		.hw_base = S5P_VA_PPMU_DDR_L,
	},
	[PPMU_RIGHT] = {
		.hw_base = S5P_VA_PPMU_RIGHT,
	},
	[PPMU_CPU] = {
		.hw_base = S5P_VA_PPMU_CPU,
	},
};

static void exynos5410_ppmu_reset(struct exynos_ppmu *ppmu)
{
	unsigned long flags;

	void __iomem *ppmu_base = ppmu->hw_base;

	/* Reset PPMU */
	exynos_ppmu_reset(ppmu_base);

	/* Set PPMU Event */
	ppmu->event[PPMU_PMNCNT0] = RD_DATA_COUNT;
	exynos_ppmu_setevent(ppmu_base, PPMU_PMNCNT0,
			ppmu->event[PPMU_PMNCNT0]);
	ppmu->event[PPMU_PMCCNT1] = WR_DATA_COUNT;
	exynos_ppmu_setevent(ppmu_base, PPMU_PMCCNT1,
			ppmu->event[PPMU_PMCCNT1]);
	ppmu->event[PPMU_PMNCNT3] = RDWR_DATA_COUNT;
	exynos_ppmu_setevent(ppmu_base, PPMU_PMNCNT3,
			ppmu->event[PPMU_PMNCNT3]);

	local_irq_save(flags);
	ppmu->reset_time = ktime_get();
	/* Start PPMU */
	exynos_ppmu_start(ppmu_base);
	local_irq_restore(flags);
}

static void exynos5410_ppmu_read(struct exynos_ppmu *ppmu)
{
	int j;
	unsigned long flags;
	ktime_t read_time;
	ktime_t t;
	u32 reg;

	void __iomem *ppmu_base = ppmu->hw_base;

	local_irq_save(flags);
	read_time = ktime_get();
	/* Stop PPMU */
	exynos_ppmu_stop(ppmu_base);
	local_irq_restore(flags);

	/* Update local data from PPMU */
	ppmu->ccnt = __raw_readl(ppmu_base + PPMU_CCNT);
	reg = __raw_readl(ppmu_base + PPMU_FLAG);
	ppmu->ccnt_overflow = reg & PPMU_CCNT_OVERFLOW;

	for (j = PPMU_PMNCNT0; j < PPMU_PMNCNT_MAX; j++) {
		if (ppmu->event[j] == 0)
			ppmu->count[j] = 0;
		else
			ppmu->count[j] = exynos_ppmu_read(ppmu_base, j);
	}
	t = ktime_sub(read_time, ppmu->reset_time);
	ppmu->ns = ktime_to_ns(t);
}

static void exynos5410_ppmu_add(struct exynos_ppmu *to, struct exynos_ppmu *from)
{
	int i;
	int j;

	for (i = 0; i < PPMU_END; i++) {
		for (j = PPMU_PMNCNT0; j < PPMU_PMNCNT_MAX; j++)
			to[i].count[j] += from[i].count[j];

		to[i].ccnt += from[i].ccnt;
		if (to[i].ccnt < from[i].ccnt)
			to[i].ccnt_overflow = true;

		to[i].ns += from[i].ns;

		if (from[i].ccnt_overflow)
			to[i].ccnt_overflow = true;
	}
}

static void exynos5410_ppmu_handle_clear(struct exynos5410_ppmu_handle *handle)
{
	memset(&handle->ppmu, 0, sizeof(struct exynos_ppmu) * PPMU_END);
}

static void exynos5410_ppmu_update(void)
{
	int i;
	struct exynos5410_ppmu_handle *handle;

	for (i = 0; i < PPMU_END; i++) {
		exynos5410_ppmu_read(&ppmu[i]);
		exynos5410_ppmu_reset(&ppmu[i]);
	}

	list_for_each_entry(handle, &exynos5410_ppmu_handle_list, node)
		exynos5410_ppmu_add(handle->ppmu, ppmu);
}

static int exynos5410_ppmu_get_filter(enum exynos_ppmu_sets filter,
	enum exynos5410_ppmu_list *start, enum exynos5410_ppmu_list *end)
{
	switch (filter) {
	case PPMU_SET_DDR:
		*start = PPMU_DDR_C;
		*end = PPMU_DDR_L;
		break;
	case PPMU_SET_RIGHT:
		*start = PPMU_RIGHT;
		*end = PPMU_RIGHT;
		break;
	case PPMU_SET_CPU:
		*start = PPMU_CPU;
		*end = PPMU_CPU;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int exynos5410_ppmu_get_busy(struct exynos5410_ppmu_handle *handle,
	enum exynos_ppmu_sets filter)
{
	unsigned long flags;
	int i;
	int busy = 0;
	int temp;
	enum exynos5410_ppmu_list start;
	enum exynos5410_ppmu_list end;
	int ret;

	ret = exynos5410_ppmu_get_filter(filter, &start, &end);
	if (ret < 0)
		return ret;

	spin_lock_irqsave(&exynos5410_ppmu_lock, flags);

	exynos5410_ppmu_update();

	for (i = start; i <= end; i++) {
		if (handle->ppmu[i].ccnt_overflow) {
			busy = -EOVERFLOW;
			break;
		}
		temp = handle->ppmu[i].count[PPMU_PMNCNT3] * 100;
		if (handle->ppmu[i].ccnt > 0)
			temp /= handle->ppmu[i].ccnt;
		if (temp > busy)
			busy = temp;
	}

	exynos5410_ppmu_handle_clear(handle);

	spin_unlock_irqrestore(&exynos5410_ppmu_lock, flags);

	return busy;
}

void exynos5410_ppmu_put(struct exynos5410_ppmu_handle *handle)
{
	unsigned long flags;

	spin_lock_irqsave(&exynos5410_ppmu_lock, flags);

	list_del(&handle->node);

	spin_unlock_irqrestore(&exynos5410_ppmu_lock, flags);

	kfree(handle);
}

struct exynos5410_ppmu_handle *exynos5410_ppmu_get(void)
{
	struct exynos5410_ppmu_handle *handle;
	unsigned long flags;

	handle = kzalloc(sizeof(struct exynos5410_ppmu_handle), GFP_KERNEL);
	if (!handle)
		return NULL;

	spin_lock_irqsave(&exynos5410_ppmu_lock, flags);

	exynos5410_ppmu_update();
	list_add_tail(&handle->node, &exynos5410_ppmu_handle_list);

	spin_unlock_irqrestore(&exynos5410_ppmu_lock, flags);

	return handle;
}
