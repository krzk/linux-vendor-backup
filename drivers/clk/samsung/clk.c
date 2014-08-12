/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Copyright (c) 2013 Linaro Ltd.
 * Author: Thomas Abraham <thomas.ab@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file includes utility functions to register clocks to common
 * clock framework for Samsung platforms.
*/

#include <linux/syscore_ops.h>
#include "clk.h"

static LIST_HEAD(samsung_clk_providers);

#ifdef CONFIG_PM_SLEEP
static void samsung_clk_suspend_provider(struct samsung_clk_provider *ctx)
{
	struct samsung_clk_reg_dump *rd = ctx->reg_dump;
	unsigned long i;

	for (i = 0; i < ctx->nr_reg_dump; i++, rd++)
		rd->value = __raw_readl(ctx->reg_base + rd->offset);
}

static void samsung_clk_resume_provider(struct samsung_clk_provider *ctx)
{
	struct samsung_clk_reg_dump *rd = ctx->reg_dump;
	unsigned long i;

	for (i = 0; i < ctx->nr_reg_dump; i++, rd++)
		__raw_writel(rd->value, ctx->reg_base + rd->offset);
}

static int samsung_clk_suspend(void)
{
	struct samsung_clk_provider *ctx;

	list_for_each_entry(ctx, &samsung_clk_providers, list)
		samsung_clk_suspend_provider(ctx);

	return 0;
}

static void samsung_clk_resume(void)
{
	struct samsung_clk_provider *ctx;

	list_for_each_entry(ctx, &samsung_clk_providers, list)
		samsung_clk_resume_provider(ctx);
}

static struct syscore_ops samsung_clk_syscore_ops = {
	.suspend	= samsung_clk_suspend,
	.resume		= samsung_clk_resume,
};
#endif /* CONFIG_PM_SLEEP */

/* setup the essentials required to support clock lookup using ccf */
struct samsung_clk_provider *__init samsung_clk_init(struct device_node *np,
		void __iomem *base,
		unsigned long nr_clks, unsigned long *rdump,
		unsigned long nr_rdump, unsigned long *soc_rdump,
		unsigned long nr_soc_rdump)
{
	struct samsung_clk_provider *ctx;

	ctx = kzalloc(sizeof(struct samsung_clk_provider), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ctx->reg_base = base;
	spin_lock_init(&ctx->lock);

#ifdef CONFIG_PM_SLEEP
	if (rdump && nr_rdump) {
		unsigned int idx;
		ctx->reg_dump = kzalloc(sizeof(struct samsung_clk_reg_dump)
				* (nr_rdump + nr_soc_rdump), GFP_KERNEL);
		if (!ctx->reg_dump) {
			pr_err("%s: memory alloc for register dump failed\n",
					__func__);
			kfree(ctx);
			return NULL;
		}

		for (idx = 0; idx < nr_rdump; idx++)
			ctx->reg_dump[idx].offset = rdump[idx];
		for (idx = 0; idx < nr_soc_rdump; idx++)
			ctx->reg_dump[nr_rdump + idx].offset = soc_rdump[idx];
		ctx->nr_reg_dump = nr_rdump + nr_soc_rdump;

		if (list_empty(&samsung_clk_providers))
			register_syscore_ops(&samsung_clk_syscore_ops);
	}
#endif

	ctx->clk_table = kzalloc(sizeof(struct clk *) * nr_clks,
					GFP_KERNEL);
	if (!ctx->clk_table)
		panic("could not allocate clock lookup table\n");

	list_add_tail(&ctx->list, &samsung_clk_providers);

	if (!np)
		return ctx;

#ifdef CONFIG_OF
	ctx->clk_data.clks = ctx->clk_table;
	ctx->clk_data.clk_num = nr_clks;
	of_clk_add_provider(np, of_clk_src_onecell_get, &ctx->clk_data);
#endif

	return ctx;
}

/* add a clock instance to the clock lookup table used for dt based lookup */
void samsung_clk_add_lookup(struct samsung_clk_provider *ctx,
			    struct clk *clk, unsigned int id)
{
	if (ctx->clk_table && id)
		ctx->clk_table[id] = clk;
}

/* register a list of aliases */
void __init samsung_clk_register_alias(struct samsung_clk_provider *ctx,
					struct samsung_clock_alias *list,
					unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	if (!ctx->clk_table) {
		pr_err("%s: clock table missing\n", __func__);
		return;
	}

	for (idx = 0; idx < nr_clk; idx++, list++) {
		if (!list->id) {
			pr_err("%s: clock id missing for index %d\n", __func__,
				idx);
			continue;
		}

		clk = ctx->clk_data.clks[list->id];
		if (!clk) {
			pr_err("%s: failed to find clock %d\n", __func__,
				list->id);
			continue;
		}

		ret = clk_register_clkdev(clk, list->alias, list->dev_name);
		if (ret)
			pr_err("%s: failed to register lookup %s\n",
					__func__, list->alias);
	}
}

/* register a list of fixed clocks */
void __init samsung_clk_register_fixed_rate(struct samsung_clk_provider *ctx,
		struct samsung_fixed_rate_clock *list, unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_fixed_rate(NULL, list->name,
			list->parent_name, list->flags, list->fixed_rate);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(ctx, clk, list->id);

		/*
		 * Unconditionally add a clock lookup for the fixed rate clocks.
		 * There are not many of these on any of Samsung platforms.
		 */
		ret = clk_register_clkdev(clk, list->name, NULL);
		if (ret)
			pr_err("%s: failed to register clock lookup for %s",
				__func__, list->name);
	}
}

/* register a list of fixed factor clocks */
void __init samsung_clk_register_fixed_factor(struct samsung_clk_provider *ctx,
		struct samsung_fixed_factor_clock *list, unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_fixed_factor(NULL, list->name,
			list->parent_name, list->flags, list->mult, list->div);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(ctx, clk, list->id);
	}
}

/* register a list of mux clocks */
void __init samsung_clk_register_mux(struct samsung_clk_provider *ctx,
				struct samsung_mux_clock *list,
				unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_mux(NULL, list->name, list->parent_names,
			list->num_parents, list->flags,
			ctx->reg_base + list->offset,
			list->shift, list->width, list->mux_flags, &ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(ctx, clk, list->id);

		/* register a clock lookup only if a clock alias is specified */
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->alias);
		}
	}
}

/* register a list of div clocks */
void __init samsung_clk_register_div(struct samsung_clk_provider *ctx,
				struct samsung_div_clock *list,
				unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		if (list->table)
			clk = clk_register_divider_table(NULL, list->name,
				list->parent_name, list->flags,
				ctx->reg_base + list->offset,
				list->shift, list->width, list->div_flags,
				list->table, &ctx->lock);
		else
			clk = clk_register_divider(NULL, list->name,
				list->parent_name, list->flags,
				ctx->reg_base + list->offset, list->shift,
				list->width, list->div_flags, &ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		samsung_clk_add_lookup(ctx, clk, list->id);

		/* register a clock lookup only if a clock alias is specified */
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
						list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
						__func__, list->alias);
		}
	}
}

/* register a list of gate clocks */
void __init samsung_clk_register_gate(struct samsung_clk_provider *ctx,
				struct samsung_gate_clock *list,
				unsigned int nr_clk)
{
	struct clk *clk;
	unsigned int idx, ret;

	for (idx = 0; idx < nr_clk; idx++, list++) {
		clk = clk_register_gate(NULL, list->name, list->parent_name,
				list->flags, ctx->reg_base + list->offset,
				list->bit_idx, list->gate_flags, &ctx->lock);
		if (IS_ERR(clk)) {
			pr_err("%s: failed to register clock %s\n", __func__,
				list->name);
			continue;
		}

		/* register a clock lookup only if a clock alias is specified */
		if (list->alias) {
			ret = clk_register_clkdev(clk, list->alias,
							list->dev_name);
			if (ret)
				pr_err("%s: failed to register lookup %s\n",
					__func__, list->alias);
		}

		samsung_clk_add_lookup(ctx, clk, list->id);
	}
}

/*
 * obtain the clock speed of all external fixed clock sources from device
 * tree and register it
 */
#ifdef CONFIG_OF
void __init samsung_clk_of_register_fixed_ext(struct samsung_clk_provider *ctx,
			struct samsung_fixed_rate_clock *fixed_rate_clk,
			unsigned int nr_fixed_rate_clk,
			struct of_device_id *clk_matches)
{
	const struct of_device_id *match;
	struct device_node *clk_np;
	u32 freq;

	for_each_matching_node_and_match(clk_np, clk_matches, &match) {
		if (of_property_read_u32(clk_np, "clock-frequency", &freq))
			continue;
		fixed_rate_clk[(u32)match->data].fixed_rate = freq;
	}
	samsung_clk_register_fixed_rate(ctx, fixed_rate_clk, nr_fixed_rate_clk);
}
#endif

/* utility function to get the rate of a specified clock */
unsigned long _get_rate(const char *clk_name)
{
	struct clk *clk;
	unsigned long rate;

	clk = clk_get(NULL, clk_name);
	if (IS_ERR(clk)) {
		pr_err("%s: could not find clock %s\n", __func__, clk_name);
		return 0;
	}
	rate = clk_get_rate(clk);
	clk_put(clk);
	return rate;
}
