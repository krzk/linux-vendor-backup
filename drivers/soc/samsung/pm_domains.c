/*
 * Exynos Generic power domain support.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Implementation of Exynos specific power domain control which is used in
 * conjunction with runtime-pm. Support for both device-tree and non-device-tree
 * based power domain support is included.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/io.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_domain.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>

#define INT_LOCAL_PWR_EN	0xF

/*
 * Exynos specific wrapper around the generic power domain
 */
struct exynos_pm_domain {
	struct generic_pm_domain pd;
	void __iomem *base;
	char const *name;
	bool is_off;
	int nr_reparent_clks;
	struct clk *oscclk;
	struct clk **clk;
	struct clk **pclk;
	int nr_asb_clks;
	struct clk **asb_clk;
};

static int exynos_pd_power(struct generic_pm_domain *domain, bool power_on)
{
	struct exynos_pm_domain *pd;
	void __iomem *base;
	u32 timeout, pwr;
	char *op;
	int i;

	pd = container_of(domain, struct exynos_pm_domain, pd);
	base = pd->base;

	for (i = 0; i < pd->nr_asb_clks; i++) {
		if (IS_ERR(pd->asb_clk[i]))
			break;
		clk_prepare_enable(pd->asb_clk[i]);
	}

	/* Set oscclk before powering off a domain*/
	if (!power_on) {
		for (i = 0; i < pd->nr_reparent_clks; i++) {
			if (IS_ERR(pd->clk[i]))
				break;
			if (clk_set_parent(pd->clk[i], pd->oscclk))
				pr_err("%s: error setting oscclk as parent to clock %d\n",
						pd->name, i);
		}
	}

	pwr = power_on ? INT_LOCAL_PWR_EN : 0;
	__raw_writel(pwr, base);

	/* Wait max 1ms */
	timeout = 100;

	while ((__raw_readl(base + 0x4) & INT_LOCAL_PWR_EN) != pwr) {
		if (!timeout) {
			op = (power_on) ? "enable" : "disable";
			pr_err("Power domain %s %s failed\n", domain->name, op);
			return -ETIMEDOUT;
		}
		timeout--;
		cpu_relax();
		usleep_range(80, 100);
	}

	/* Restore clocks after powering on a domain*/
	if (power_on) {
		for (i = 0; i < pd->nr_reparent_clks; i++) {
			if (IS_ERR(pd->clk[i]))
				break;
			if (clk_set_parent(pd->clk[i], pd->pclk[i]))
				pr_err("%s: error setting parent to clock%d\n",
						pd->name, i);
		}
	}

	for (i = 0; i < pd->nr_asb_clks; i++) {
		if (IS_ERR(pd->asb_clk[i]))
			break;
		clk_disable_unprepare(pd->asb_clk[i]);
	}

	return 0;
}

static int exynos_pd_power_on(struct generic_pm_domain *domain)
{
	return exynos_pd_power(domain, true);
}

static int exynos_pd_power_off(struct generic_pm_domain *domain)
{
	return exynos_pd_power(domain, false);
}

static __init int exynos4_pm_init_power_domain(void)
{
	struct device_node *np;
	int nr_clks;

	for_each_compatible_node(np, NULL, "samsung,exynos4210-pd") {
		struct exynos_pm_domain *pd;
		int on, i;

		pd = kzalloc(sizeof(*pd), GFP_KERNEL);
		if (!pd) {
			pr_err("%s: failed to allocate memory for domain\n",
					__func__);
			return -ENOMEM;
		}

		pd->pd.name = kstrdup(np->name, GFP_KERNEL);
		pd->name = pd->pd.name;
		pd->base = of_iomap(np, 0);
		pd->pd.power_off = exynos_pd_power_off;
		pd->pd.power_on = exynos_pd_power_on;

		nr_clks = of_count_phandle_with_args(np, "clocks","#clock-cells");
		if (nr_clks > 0 && !(nr_clks % 2)) {
			nr_clks /= 2;

			pd->oscclk = clk_get(NULL, "xxti");
			if (IS_ERR(pd->oscclk))
				break;

			pd->clk = kcalloc(sizeof(struct clk *),
						nr_clks, GFP_KERNEL);
			pd->pclk = kcalloc(sizeof(struct clk *),
						nr_clks, GFP_KERNEL);
			for (i = 0; i < nr_clks; i++) {
				pd->clk[i] = of_clk_get(np, i);
				if (IS_ERR(pd->clk[i]))
					break;
				pd->pclk[i] = of_clk_get(np, i + 1);
				if (IS_ERR(pd->pclk[i])) {
					clk_put(pd->clk[i]);
					pd->clk[i] = ERR_PTR(-EINVAL);
					break;
				}
			}

			pd->nr_reparent_clks = nr_clks;
		}

		on = __raw_readl(pd->base + 0x4) & INT_LOCAL_PWR_EN;

		pm_genpd_init(&pd->pd, NULL, !on);
		of_genpd_add_provider_simple(np, &pd->pd);
	}

	/* Assign the child power domains to their parents */
	for_each_compatible_node(np, NULL, "samsung,exynos4210-pd") {
		struct generic_pm_domain *child_domain, *parent_domain;
		struct of_phandle_args args;

		args.np = np;
		args.args_count = 0;
		child_domain = of_genpd_get_from_provider(&args);
		if (IS_ERR(child_domain))
			continue;

		if (of_parse_phandle_with_args(np, "power-domains",
					 "#power-domain-cells", 0, &args) != 0)
			continue;

		parent_domain = of_genpd_get_from_provider(&args);
		if (IS_ERR(parent_domain))
			continue;

		if (pm_genpd_add_subdomain(parent_domain, child_domain))
			pr_warn("%s failed to add subdomain: %s\n",
				parent_domain->name, child_domain->name);
		else
			pr_info("%s has as child subdomain: %s.\n",
				parent_domain->name, child_domain->name);
		of_node_put(np);
	}

	return 0;
}
core_initcall(exynos4_pm_init_power_domain);
