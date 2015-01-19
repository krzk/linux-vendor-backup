/*
 * Copyright (C) 2015 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <mali_kbase.h>

/* TODO: support DVFS */

struct mali_data {
	struct device	*dev;
	struct clk	*g3d;
	struct clk	*aclk_g3d;
	struct regulator	*vdd_g3d;
};

static int exynos5422_platform_init(struct kbase_device *kbdev)
{
	struct device *dev = kbdev->dev;
	struct mali_data *mali;
	int ret;

	mali = devm_kzalloc(dev, sizeof(*mali), GFP_KERNEL);
	if (!mali)
		return -ENOMEM;

	mali->dev = dev;

	/* TODO: check g3d power domain */

	mali->g3d = devm_clk_get(dev, "g3d");
	if (IS_ERR(mali->g3d)) {
		dev_err(dev, "Failed to get g3d clk\n");
		return PTR_ERR(mali->g3d);
	}

	/* TODO: check clock hierarchy & clock rate */
	mali->aclk_g3d = devm_clk_get(dev, "aclk_g3d");
	if (IS_ERR(mali->aclk_g3d)) {
		dev_err(dev, "Failed to get aclk_g3d clk\n");
		return PTR_ERR(mali->aclk_g3d);
	}

	mali->vdd_g3d = devm_regulator_get(dev, "vdd_g3d");
	if (IS_ERR(mali->vdd_g3d)) {
		dev_err(dev, "Failed to get vdd_g3d regulator\n");
		return PTR_ERR(mali->vdd_g3d);
	}

	ret = clk_prepare_enable(mali->g3d);
	if (ret < 0) {
		dev_err(dev, "Failed to enable g3d clk\n");
		return ret;
	}

	ret = clk_prepare_enable(mali->aclk_g3d);
	if (ret < 0) {
		dev_err(dev, "Failed to enable aclk_g3d clk\n");
		goto err;
	}

	/* TODO: check regulator voltage */
	ret = regulator_enable(mali->vdd_g3d);
	if (ret < 0) {
		dev_err(dev, "Failed to enable vdd_g3d regulator\n");
		goto err_aclk_g3d;
	}

	kbdev->platform_context = mali;

	return 0;

err_aclk_g3d:
	clk_disable_unprepare(mali->aclk_g3d);
err:
	clk_disable_unprepare(mali->g3d);
	return ret;
}

static void exynos5422_platform_term(struct kbase_device *kbdev)
{
	struct mali_data *mali = kbdev->platform_context;

	regulator_disable(mali->vdd_g3d);
	clk_disable_unprepare(mali->aclk_g3d);
	clk_disable_unprepare(mali->g3d);

	kbdev->platform_context = NULL;
}

static struct kbase_platform_funcs_conf exynos5422_platform_funcs = {
	.platform_init_func = exynos5422_platform_init,
	.platform_term_func = exynos5422_platform_term,
};

static const struct kbase_attribute exynos5422_attributes[] = {
	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		(uintptr_t)&exynos5422_platform_funcs,
	}, {
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		500, /* 500ms before cancelling stuck jobs */
	}, {
		KBASE_CONFIG_ATTR_END,
		0,
	}
};

static struct kbase_platform_config exynos5422_platform_config = {
	.attributes = exynos5422_attributes,
};

struct kbase_platform_config *kbase_get_platform_config(void)
{
	return &exynos5422_platform_config;
}
