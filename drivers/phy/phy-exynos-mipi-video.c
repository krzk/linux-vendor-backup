/*
 * Samsung S5P/EXYNOS SoC series MIPI CSIS/DSIM DPHY driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Author: Sylwester Nawrocki <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon/exynos4-pmu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/spinlock.h>
#include <linux/mfd/syscon.h>

enum exynos_mipi_phy_id {
	EXYNOS_MIPI_PHY_ID_NONE = -1,
	EXYNOS_MIPI_PHY_ID_CSIS0,
	EXYNOS_MIPI_PHY_ID_DSIM0,
	EXYNOS_MIPI_PHY_ID_CSIS1,
	EXYNOS_MIPI_PHY_ID_DSIM1,
	EXYNOS_MIPI_PHY_ID_CSIS2,
	EXYNOS_MIPI_PHYS_NUM
};

enum exynos_mipi_phy_regmap_id {
	EXYNOS_MIPI_REGMAP_PMU,
	EXYNOS_MIPI_REGMAP_DISP,
	EXYNOS_MIPI_REGMAP_CAM0,
	EXYNOS_MIPI_REGMAP_CAM1,
	EXYNOS_MIPI_REGMAPS_NUM
};

struct mipi_phy_device_desc
{
	int num_phys;
	int num_regmaps;
	const char *regmap_names[EXYNOS_MIPI_REGMAPS_NUM];
	struct exynos_mipi_phy_desc {
		enum exynos_mipi_phy_id	coupled_id;
		u32 enable_val;
		int enable_reg;
		enum exynos_mipi_phy_regmap_id enable_map;
		u32 reset_val;
		int reset_reg;
		enum exynos_mipi_phy_regmap_id reset_map;
	} phys[EXYNOS_MIPI_PHYS_NUM];
};

struct exynos_mipi_phy {
	spinlock_t slock;
	struct regmap *regmaps[EXYNOS_MIPI_REGMAPS_NUM];
	int num_phys;
	struct exynos_mipi_phy_instance {
		struct phy *phy;
		unsigned int index;
		const struct exynos_mipi_phy_desc *data;
	} phys[EXYNOS_MIPI_PHYS_NUM];
};

static int __is_running(const struct exynos_mipi_phy_desc *data,
			struct exynos_mipi_phy *state)
{
	u32 val;
	regmap_read(state->regmaps[data->reset_map], data->reset_reg, &val);
	return val & data->reset_val;
}

static int __set_phy_state(const struct exynos_mipi_phy_desc *data,
			   struct exynos_mipi_phy *state, unsigned int on)
{
	u32 val;

	spin_lock(&state->slock);

	/* PHY PMU disable */
	if (!on && data->coupled_id >= 0 &&
	    __is_running(state->phys[data->coupled_id].data, state)) {
		regmap_read(state->regmaps[data->enable_map], data->enable_reg,
			    &val);
		val &= ~data->enable_val;
		regmap_write(state->regmaps[data->enable_map], data->enable_reg,
			     val);
	}

	/* PHY reset */
	regmap_read(state->regmaps[data->reset_map], data->reset_reg,
		    &val);
	val = on ? (val | data->reset_val) : (val & ~data->reset_val);
	regmap_write(state->regmaps[data->reset_map], data->reset_reg,
		     val);

	/* PHY PMU enable */
	if (on) {
		regmap_read(state->regmaps[data->enable_map], data->enable_reg,
			    &val);
		val |= data->enable_val;
		regmap_write(state->regmaps[data->enable_map], data->enable_reg,
			     val);
	}

	spin_unlock(&state->slock);

	return 0;
}

#define to_mipi_video_phy(desc) \
	container_of((desc), struct exynos_mipi_phy, phys[(desc)->index]);

static int exynos_mipi_video_phy_power_on(struct phy *phy)
{
	struct exynos_mipi_phy_instance *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_state(phy_desc->data, state, 1);
}

static int exynos_mipi_video_phy_power_off(struct phy *phy)
{
	struct exynos_mipi_phy_instance *phy_desc = phy_get_drvdata(phy);
	struct exynos_mipi_phy *state = to_mipi_video_phy(phy_desc);

	return __set_phy_state(phy_desc->data, state, 0);
}

static struct phy *exynos_mipi_video_phy_xlate(struct device *dev,
					struct of_phandle_args *args)
{
	struct exynos_mipi_phy *state = dev_get_drvdata(dev);

	if (WARN_ON(args->args[0] >= state->num_phys))
		return ERR_PTR(-ENODEV);

	return state->phys[args->args[0]].phy;
}

static struct phy_ops exynos_mipi_video_phy_ops = {
	.power_on	= exynos_mipi_video_phy_power_on,
	.power_off	= exynos_mipi_video_phy_power_off,
	.owner		= THIS_MODULE,
};

static const struct mipi_phy_device_desc s5pv210_mipi_phy = {
	.num_regmaps = 1,
	.regmap_names = {"syscon"},
	.num_phys = 4,
	.phys = {
		{
			/* EXYNOS_MIPI_PHY_ID_CSIS0 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_DSIM0,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(0),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = EXYNOS4_MIPI_PHY_SRESETN,
			.reset_reg = EXYNOS4_MIPI_PHY_ENABLE,
			.reset_map = EXYNOS_MIPI_REGMAP_PMU,
		}, {
			/* EXYNOS_MIPI_PHY_ID_DSIM0 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_CSIS0,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(0),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = EXYNOS4_MIPI_PHY_MRESETN,
			.reset_reg = EXYNOS4_MIPI_PHY_CONTROL(0),
			.reset_map = EXYNOS_MIPI_REGMAP_PMU,
		}, {
			/* EXYNOS_MIPI_PHY_ID_CSIS1 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_DSIM1,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(1),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = EXYNOS4_MIPI_PHY_SRESETN,
			.reset_reg = EXYNOS4_MIPI_PHY_CONTROL(1),
			.reset_map = EXYNOS_MIPI_REGMAP_PMU,
		}, {
			/* EXYNOS_MIPI_PHY_ID_DSIM1 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_CSIS1,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(1),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = EXYNOS4_MIPI_PHY_MRESETN,
			.reset_reg = EXYNOS4_MIPI_PHY_CONTROL(1),
			.reset_map = EXYNOS_MIPI_REGMAP_PMU,
		},
	},
};

static const struct mipi_phy_device_desc exynos5433_mipi_phy = {
	.num_regmaps = 4,
	.regmap_names = {
		"samsung,pmu-syscon",
		"samsung,disp-sysreg",
		"samsung,cam0-sysreg",
		"samsung,cam1-sysreg"
	},
	.num_phys = 5,
	.phys = {
		{
			/* EXYNOS_MIPI_PHY_ID_CSIS0 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_DSIM0,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(0),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = 0x1 << 0,
			.reset_reg = 0x1014,
			.reset_map = EXYNOS_MIPI_REGMAP_CAM0,
		}, {
			/* EXYNOS_MIPI_PHY_ID_DSIM0 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_CSIS0,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(0),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = 0x1 << 0,
			.reset_reg = 0x100C,
			.reset_map = EXYNOS_MIPI_REGMAP_DISP,
		}, {
			/* EXYNOS_MIPI_PHY_ID_CSIS1 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_NONE,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(1),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = 0x1 << 1,
			.reset_reg = 0x1014,
			.reset_map = EXYNOS_MIPI_REGMAP_CAM0,
		}, {
			/* EXYNOS_MIPI_PHY_ID_DSIM1 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_NONE,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(1),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = 0x1 << 1,
			.reset_reg = 0x100C,
			.reset_map = EXYNOS_MIPI_REGMAP_DISP,
		}, {
			/* EXYNOS_MIPI_PHY_ID_CSIS2 */
			.coupled_id = EXYNOS_MIPI_PHY_ID_NONE,
			.enable_val = EXYNOS4_MIPI_PHY_ENABLE,
			.enable_reg = EXYNOS4_MIPI_PHY_CONTROL(2),
			.enable_map = EXYNOS_MIPI_REGMAP_PMU,
			.reset_val = 0x1 << 0,
			.reset_reg = 0x1020,
			.reset_map = EXYNOS_MIPI_REGMAP_CAM1,
		},
	},
};

static const struct of_device_id exynos_mipi_video_phy_of_match[] = {
	{
		.compatible = "samsung,s5pv210-mipi-video-phy",
		.data = &s5pv210_mipi_phy,
	}, {
		.compatible = "samsung,exynos5433-mipi-video-phy",
		.data = &exynos5433_mipi_phy,
	},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, exynos_mipi_video_phy_of_match);

static int exynos_mipi_video_phy_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct mipi_phy_device_desc *phy_dev;
	struct exynos_mipi_phy *state;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct phy_provider *phy_provider;
	unsigned int i;

	state = devm_kzalloc(dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	match = of_match_node(exynos_mipi_video_phy_of_match, np);
	if (!match) {
		dev_err(dev, "of_match_node() failed\n");
		return -EINVAL;
	}
	phy_dev = match->data;

	for (i = 0; i < phy_dev->num_regmaps; i++) {
		state->regmaps[i] = syscon_regmap_lookup_by_phandle(np,
					phy_dev->regmap_names[i]);
		if (IS_ERR(state->regmaps[i]))
			return PTR_ERR(state->regmaps[i]);
	}
	state->num_phys = phy_dev->num_phys;

	dev_set_drvdata(dev, state);
	spin_lock_init(&state->slock);

	for (i = 0; i < state->num_phys; i++) {
		struct phy *phy = devm_phy_create(dev, NULL,
						  &exynos_mipi_video_phy_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create PHY %d\n", i);
			return PTR_ERR(phy);
		}

		state->phys[i].phy = phy;
		state->phys[i].index = i;
		state->phys[i].data = &phy_dev->phys[i];
		phy_set_drvdata(phy, &state->phys[i]);
	}

	phy_provider = devm_of_phy_provider_register(dev,
					exynos_mipi_video_phy_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}



static struct platform_driver exynos_mipi_video_phy_driver = {
	.probe	= exynos_mipi_video_phy_probe,
	.driver = {
		.of_match_table	= exynos_mipi_video_phy_of_match,
		.name  = "exynos-mipi-video-phy",
	}
};
module_platform_driver(exynos_mipi_video_phy_driver);

MODULE_DESCRIPTION("Samsung S5P/EXYNOS SoC MIPI CSI-2/DSI PHY driver");
MODULE_AUTHOR("Sylwester Nawrocki <s.nawrocki@samsung.com>");
MODULE_LICENSE("GPL v2");
