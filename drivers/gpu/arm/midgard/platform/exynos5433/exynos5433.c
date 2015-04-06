/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/regulator/consumer.h>
#include <linux/pm_opp.h>
#include <mali_kbase.h>

struct mali_data {
	struct device		*dev;
	struct regulator	*vdd_g3d;
};

static int exynos5433_platform_init(struct kbase_device *kbdev)
{
	struct device *dev = kbdev->dev;
	struct mali_data *mali;
	int ret;

	mali = devm_kzalloc(dev, sizeof(*mali), GFP_KERNEL);
	if (!mali)
		return -ENOMEM;

	mali->dev = dev;

	/* TODO: check g3d power domain */

	ret = of_init_opp_table(dev);
	if (ret < 0) {
		dev_err(dev, "failed to get OPP table\n");
		return ret;
	}

	mali->vdd_g3d = devm_regulator_get(dev, "vdd_g3d");
	if (IS_ERR(mali->vdd_g3d)) {
		dev_err(dev, "Failed to get vdd_g3d regulator\n");
		ret = PTR_ERR(mali->vdd_g3d);
		goto err;
	}

	/* TODO: check regulator voltage */
	ret = regulator_enable(mali->vdd_g3d);
	if (ret < 0) {
		dev_err(dev, "Failed to enable vdd_g3d regulator\n");
		goto err;
	}

	kbdev->platform_context = mali;

	return 0;

err:
	of_free_opp_table(dev);
	return ret;
}

static void exynos5433_platform_term(struct kbase_device *kbdev)
{
	struct device *dev = kbdev->dev;
	struct mali_data *mali = kbdev->platform_context;

	regulator_disable(mali->vdd_g3d);
	of_free_opp_table(dev);

	kbdev->platform_context = NULL;
}

static struct kbase_platform_funcs_conf exynos5433_platform_funcs = {
	.platform_init_func = exynos5433_platform_init,
	.platform_term_func = exynos5433_platform_term,
};

static const struct kbase_attribute exynos5433_attributes[] = {
	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		(uintptr_t)&exynos5433_platform_funcs,
	}, {
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		500, /* 500ms before cancelling stuck jobs */
	}, {
		KBASE_CONFIG_ATTR_END,
		0,
	}
};

static struct kbase_platform_config exynos5433_platform_config = {
	.attributes = exynos5433_attributes,
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &exynos5433_platform_config;
}
