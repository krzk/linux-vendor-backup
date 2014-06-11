/* drivers/gpu/mali400/mali/platform/exynos/exynos.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali400 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file exynos.c
 * Platform specific Mali driver functions for the exynos platforms
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mali/mali_utgard.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#ifdef CONFIG_CPU_FREQ
#include <mach/asv-exynos.h>
#endif

#include "mali_kernel_common.h"
#include "mali_osk.h"
#ifdef CONFIG_MALI400_PROFILING
#include "mali_osk_profiling.h"
#endif
#include "exynos.h"

struct mali_exynos_dvfs_step {
	unsigned int	rate;
	unsigned int	min_uv;
	unsigned int	max_uv;
	unsigned int	downthreshold;
	unsigned int	upthreshold;
};

struct mali_exynos_variant {
	const struct mali_exynos_dvfs_step	*steps;
	unsigned int				nr_steps;
};

struct mali_exynos_drvdata {
	struct device				*dev;

	const struct mali_exynos_dvfs_step	*steps;
	unsigned int				nr_steps;

	struct clk				*pll;
	struct clk				*mux1;
	struct clk				*mux2;
	struct clk				*sclk;
	struct clk				*smmu;
	struct clk				*g3d;

	struct regulator			*vdd_g3d;

	mali_power_mode				power_mode;
	unsigned int				dvfs_step;
	unsigned int				load;

	struct work_struct			dvfs_work;
	struct workqueue_struct			*dvfs_workqueue;
};

extern struct platform_device *mali_platform_device;
static struct mali_exynos_drvdata *mali;

static void mali_exynos_update_dvfs(struct mali_gpu_utilization_data *data);

static const struct mali_gpu_device_data mali_exynos_gpu_data = {
	.shared_mem_size	=	256 * 1024 * 1024, /* 256MB */
	.fb_start		=	0x40000000,
	.fb_size		=	0xb1000000,
	.utilization_interval	=	100, /* 100ms */
	.utilization_callback	=	mali_exynos_update_dvfs,
};

#define MALI_DVFS_STEP(freq, min_uv, max_uv, down, up) \
	{freq * 1000000, min_uv, max_uv, (255 * down) / 100, (255 * up) / 100}

static const struct mali_exynos_dvfs_step mali_exynos_dvfs_step_3250[] = {
	MALI_DVFS_STEP(134,       0,       0,  0, 100)
};

static const struct mali_exynos_dvfs_step mali_exynos_dvfs_step_3472[] = {
	MALI_DVFS_STEP(160,  875000,  875000,  0,  70),
	MALI_DVFS_STEP(266,  875000,  875000, 62,  90),
	MALI_DVFS_STEP(350,  925000,  925000, 85,  90),
	MALI_DVFS_STEP(440, 1000000, 1000000, 85, 100)
};

static const struct mali_exynos_dvfs_step mali_exynos_dvfs_step_4210[] = {
	MALI_DVFS_STEP(160,  950000,  975000,  0,  85),
	MALI_DVFS_STEP(266, 1000000, 1025000, 75, 100)
};

static const struct mali_exynos_dvfs_step mali_exynos_dvfs_step_4x12[] = {
	MALI_DVFS_STEP(160,  875000,  900000,  0,  70),
	MALI_DVFS_STEP(266,  900000,  925000, 62,  90),
	MALI_DVFS_STEP(350,  950000,  975000, 85,  90),
	MALI_DVFS_STEP(440, 1025000, 1050000, 85, 100)
};

static const struct mali_exynos_dvfs_step mali_exynos_dvfs_step_4x12_prime[] = {
	MALI_DVFS_STEP(160,  875000,  900000,  0,  70),
	MALI_DVFS_STEP(266,  900000,  925000, 62,  90),
	MALI_DVFS_STEP(350,  950000,  975000, 85,  90),
	MALI_DVFS_STEP(440, 1025000, 1050000, 85,  90),
	MALI_DVFS_STEP(533, 1075000, 1100000, 85, 100)
};

static const struct mali_exynos_variant mali_exynos_variant_3250 = {
	.steps		=	mali_exynos_dvfs_step_3250,
	.nr_steps	=	ARRAY_SIZE(mali_exynos_dvfs_step_3250),
};

static const struct mali_exynos_variant mali_exynos_variant_3472 = {
	.steps		=	mali_exynos_dvfs_step_3472,
	.nr_steps	=	ARRAY_SIZE(mali_exynos_dvfs_step_3472),
};

static const struct mali_exynos_variant mali_exynos_variant_4210 = {
	.steps		=	mali_exynos_dvfs_step_4210,
	.nr_steps	=	ARRAY_SIZE(mali_exynos_dvfs_step_4210),
};

static const struct mali_exynos_variant mali_exynos_variant_4x12 = {
	.steps		=	mali_exynos_dvfs_step_4x12,
	.nr_steps	=	ARRAY_SIZE(mali_exynos_dvfs_step_4x12),
};

static const struct mali_exynos_variant mali_exynos_variant_4x12_prime = {
	.steps		=	mali_exynos_dvfs_step_4x12_prime,
	.nr_steps	=	ARRAY_SIZE(mali_exynos_dvfs_step_4x12_prime),
};

/* PegaW1 */
int mali_gpu_clk, mali_gpu_vol;
unsigned int mali_dvfs_utilization;

/* export GPU frequency as a read-only parameter so that it can be read in /sys */
module_param(mali_gpu_clk, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mali_gpu_clk, "Mali Current Clock");
module_param(mali_gpu_vol, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mali_gpu_vol, "Mali Current Voltage");

const struct of_device_id mali_of_matches[] = {
	{
		.compatible	=	"samsung,exynos3250-g3d",
		.data		=	&mali_exynos_variant_3250,
	},
	{
		.compatible	=	"samsung,exynos3472-g3d",
		.data		=	&mali_exynos_variant_3472,
	},
	{
		.compatible	=	"samsung,exynos4210-g3d",
		.data		=	&mali_exynos_variant_4210,
	},
	{
		.compatible	=	"samsung,exynos4x12-g3d",
		.data		=	&mali_exynos_variant_4x12,
	},
	{
		.compatible	=	"samsung,exynos4x12_prime-g3d",
		.data		=	&mali_exynos_variant_4x12_prime,
	},
	{ /* Sentinel */ }
};

#ifdef CONFIG_MALI400_PROFILING
static inline void _mali_osk_profiling_add_gpufreq_event(int rate, int vol)
{
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
		 MALI_PROFILING_EVENT_CHANNEL_GPU |
		 MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
		 rate, vol, 0, 0, 0);
}
#else
static inline void _mali_osk_profiling_add_gpufreq_event(int rate, int vol)
{
}
#endif

static inline int mali_exynos_regulator_set_voltage(struct regulator *vdd_g3d,
				const struct mali_exynos_dvfs_step *next)
{
	unsigned int min_uv, max_uv;

#ifdef CONFIG_CPU_FREQ
	min_uv = max_uv = get_match_vol(ID_G3D, next->rate / 1000);
#else
	min_uv = next->min_uv;
	max_uv = next->max_uv;
#endif

	return regulator_set_voltage(vdd_g3d, min_uv, max_uv);
}

static void mali_exynos_set_dvfs_step(struct mali_exynos_drvdata *mali,
							unsigned int step)
{
	const struct mali_exynos_dvfs_step *next = &mali->steps[step];

	if (step > mali->dvfs_step) {
		if (mali->vdd_g3d)
			mali_exynos_regulator_set_voltage(mali->vdd_g3d, next);
		clk_set_rate(mali->sclk, next->rate);
	} else {
		clk_set_rate(mali->sclk, next->rate);
		if (mali->vdd_g3d)
			mali_exynos_regulator_set_voltage(mali->vdd_g3d, next);
	}

	mali_gpu_clk = (int)(clk_get_rate(mali->sclk) / 1000000);
	if (mali->vdd_g3d)
		mali_gpu_vol =
			(int)(regulator_get_voltage(mali->vdd_g3d) / 1000);
	else
		mali_gpu_vol = 0;

	_mali_osk_profiling_add_gpufreq_event(mali_gpu_clk, mali_gpu_vol);

	mali->dvfs_step = step;
}

static void mali_exynos_dvfs_work(struct work_struct *work)
{
	struct mali_exynos_drvdata *mali = container_of(work,
					struct mali_exynos_drvdata, dvfs_work);
	unsigned int step = mali->dvfs_step;
	const struct mali_exynos_dvfs_step *cur = &mali->steps[step];

	if (mali->load > cur->upthreshold)
		++step;
	else if (mali->load < cur->downthreshold)
		--step;

	BUG_ON(step >= mali->nr_steps);

	if (step != mali->dvfs_step)
		mali_exynos_set_dvfs_step(mali, step);
}

static void mali_exynos_update_dvfs(struct mali_gpu_utilization_data *data)
{
	if (data->utilization_gpu > 255)
		data->utilization_gpu = 255;

	mali->load = data->utilization_gpu;
	mali_dvfs_utilization = data->utilization_gpu;

	queue_work(mali->dvfs_workqueue, &mali->dvfs_work);
}

static _mali_osk_errcode_t
	mali_exynos_enable_clks(struct mali_exynos_drvdata *mali, bool enable)
{
	if (enable) {
		if (clk_prepare_enable(mali->smmu))
			MALI_ERROR(_MALI_OSK_ERR_FAULT);

		if (clk_prepare_enable(mali->g3d)) {
			clk_disable_unprepare(mali->smmu);
			MALI_ERROR(_MALI_OSK_ERR_FAULT);
		}

		if (clk_prepare_enable(mali->sclk)) {
			clk_disable_unprepare(mali->g3d);
			clk_disable_unprepare(mali->smmu);
			MALI_ERROR(_MALI_OSK_ERR_FAULT);
		}
	} else {
		clk_disable_unprepare(mali->sclk);
		clk_disable_unprepare(mali->g3d);
		clk_disable_unprepare(mali->smmu);
	}

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
	if (mali->power_mode == power_mode)
		MALI_SUCCESS;
	/* to avoid multiple clk_disable() call */
	else if ((mali->power_mode > MALI_POWER_MODE_ON) &&
					(power_mode > MALI_POWER_MODE_ON)) {
		mali->power_mode = power_mode;
		MALI_SUCCESS;
	}

	switch (power_mode) {
	case MALI_POWER_MODE_ON:
		mali_exynos_set_dvfs_step(mali, 0);

		if (mali_exynos_enable_clks(mali, true)) {
			MALI_PRINT_ERROR(("fail to enable clocks"));
			MALI_ERROR(_MALI_OSK_ERR_FAULT);
		}

		break;
	case MALI_POWER_MODE_DEEP_SLEEP:
	case MALI_POWER_MODE_LIGHT_SLEEP:
		if (mali_exynos_enable_clks(mali, false)) {
			MALI_PRINT_ERROR(("fail to disable clocks"));
			MALI_ERROR(_MALI_OSK_ERR_FAULT);
		}

		mali_exynos_set_dvfs_step(mali, 0);
		mali_gpu_clk = 0;
		break;
	}

	mali->power_mode = power_mode;

	MALI_SUCCESS;
}

static const struct mali_exynos_variant *
	mali_exynos_get_variant_data(struct platform_device *pdev)
{
	const struct of_device_id *match = of_match_node(mali_of_matches,
							pdev->dev.of_node);
	if (!match)
		return NULL;

	return (const struct mali_exynos_variant *)match->data;
}

static int mali_exynos_rearrange_resources(struct platform_device *pdev)
{
	unsigned int idx;
	unsigned int irq_idx = 0;
	unsigned int mem_idx = 0;
	struct resource *old_res, *new_res;

	old_res = pdev->resource;
	new_res= kzalloc(sizeof(*new_res) * pdev->num_resources, GFP_KERNEL);
	if (WARN_ON(!new_res))
		return -ENOMEM;

	/* Copy first resource, L2 cache memory region. */
	memcpy(new_res, old_res++, sizeof(*new_res));

	/*
	 * The gpu resources from device tree are arranged like below,
	 *	MEM, MEM, ..., IRQ, IRQ, ...
	 * So they should be rearranged like below
	 * because _mali_osk_resource_find function requires,
	 *	MEM, IRQ, MEM, IRQ, ...
	 */
	for (idx = 1; idx < pdev->num_resources; ++idx, ++old_res) {
		if (resource_type(old_res) == IORESOURCE_MEM)
			memcpy(&new_res[1 + 2 * mem_idx++],
					old_res, sizeof(*old_res));
		if (resource_type(old_res) == IORESOURCE_IRQ)
			memcpy(&new_res[2 + 2 * irq_idx++],
					old_res, sizeof(*old_res));
	}

	kfree(pdev->resource);
	pdev->resource = new_res;

	return 0;
}

static int mali_exynos_get_clks(struct mali_exynos_drvdata *mali)
{
	mali->pll = devm_clk_get(mali->dev, "pll");
	if (WARN_ON(IS_ERR(mali->pll)))
		return PTR_ERR(mali->pll);

	mali->mux1 = devm_clk_get(mali->dev, "mux1");
	if (WARN_ON(IS_ERR(mali->mux1)))
		return PTR_ERR(mali->mux1);

	mali->mux2 = devm_clk_get(mali->dev, "mux2");
	if (WARN_ON(IS_ERR(mali->mux2)))
		return PTR_ERR(mali->mux2);

	mali->sclk = devm_clk_get(mali->dev, "sclk");
	if (WARN_ON(IS_ERR(mali->sclk)))
		return PTR_ERR(mali->sclk);

	mali->smmu = devm_clk_get(mali->dev, "smmu");
	if (WARN_ON(IS_ERR(mali->smmu)))
		return PTR_ERR(mali->smmu);

	mali->g3d = devm_clk_get(mali->dev, "g3d");
	if (WARN_ON(IS_ERR(mali->g3d)))
		return PTR_ERR(mali->g3d);

	clk_set_parent(mali->mux1, mali->pll);
	clk_set_parent(mali->mux2, mali->mux1);
	clk_set_parent(mali->sclk, mali->mux2);

	return 0;
}

static bool
	mali_exynos_check_regulator_is_needed(struct mali_exynos_drvdata *mali)
{
	const struct mali_exynos_dvfs_step *step = &mali->steps[0];

	if (step->min_uv || step->max_uv)
		return true;

	return false;
}

_mali_osk_errcode_t mali_platform_init(void)
{
	struct platform_device *pdev = mali_platform_device;
	const struct mali_exynos_variant *variant;
	int ret;

	if (WARN_ON(!pdev))
		MALI_ERROR(_MALI_OSK_ERR_FAULT);

	if (WARN_ON(!pdev->dev.of_node))
		MALI_ERROR(_MALI_OSK_ERR_FAULT);

	variant = mali_exynos_get_variant_data(pdev);
	if (WARN_ON(!variant))
		MALI_ERROR(_MALI_OSK_ERR_FAULT);

	pdev->dev.platform_data = (void *)&mali_exynos_gpu_data;

	ret = mali_exynos_rearrange_resources(pdev);
	if (ret < 0)
		MALI_ERROR(_MALI_OSK_ERR_FAULT);

	mali = kzalloc(sizeof(*mali), GFP_KERNEL);
	if (WARN_ON(!mali))
		MALI_ERROR(_MALI_OSK_ERR_NOMEM);

	mali->dev = &pdev->dev;
	mali->steps = variant->steps;
	mali->nr_steps = variant->nr_steps;

	ret = mali_exynos_get_clks(mali);
	if (ret < 0) {
		MALI_PRINT_ERROR(("Failed to get Mali clocks"));
		goto err_free_mali;
	}

	mali->vdd_g3d = devm_regulator_get(mali->dev, "vdd");
	if (IS_ERR(mali->vdd_g3d)) {
		if (WARN_ON(mali_exynos_check_regulator_is_needed(mali))) {
			MALI_PRINT_ERROR(("failed to get Mali regulator"));
			goto err_free_mali;
		}

		mali->vdd_g3d = NULL;
	}

	mali->dvfs_workqueue = create_singlethread_workqueue("mali_dvfs");
	if (WARN_ON(!mali->dvfs_workqueue)) {
		MALI_PRINT_ERROR(("failed to create workqueue"));
		goto err_free_mali;
	}

	mali->power_mode = MALI_POWER_MODE_DEEP_SLEEP;

	INIT_WORK(&mali->dvfs_work, mali_exynos_dvfs_work);

	if (mali->vdd_g3d) {
		ret = regulator_enable(mali->vdd_g3d);
		if (WARN_ON(ret))
			goto err_destroy_dvfs_wq;
	}

	mali_exynos_set_dvfs_step(mali, 0);

#if !defined(MALI_FAKE_PLATFORM_DEVICE)
	pm_runtime_set_autosuspend_delay(&pdev->dev, 300);
	pm_runtime_use_autosuspend(&pdev->dev);

	pm_runtime_enable(&pdev->dev);
#endif

	MALI_SUCCESS;

err_destroy_dvfs_wq:
	destroy_workqueue(mali->dvfs_workqueue);
err_free_mali:
	kfree(mali);
	MALI_ERROR(_MALI_OSK_ERR_FAULT);
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	struct platform_device *pdev = mali_platform_device;

#if !defined(MALI_FAKE_PLATFORM_DEVICE)
	pm_runtime_disable(&pdev->dev);
#endif

	if (mali->vdd_g3d)
		regulator_disable(mali->vdd_g3d);

	kfree(mali);

	MALI_SUCCESS;
}
