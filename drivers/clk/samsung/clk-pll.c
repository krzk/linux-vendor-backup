/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Copyright (c) 2013 Linaro Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains the utility functions to register the pll clocks.
*/

#include <linux/errno.h>
#include "clk.h"
#include "clk-pll.h"

/*
 * PLL35xx Clock Type
 */

#define PLL35XX_MDIV_MASK       (0x3FF)
#define PLL35XX_PDIV_MASK       (0x3F)
#define PLL35XX_SDIV_MASK       (0x7)
#define PLL35XX_MDIV_SHIFT      (16)
#define PLL35XX_PDIV_SHIFT      (8)
#define PLL35XX_SDIV_SHIFT      (0)

#define PLL35XX_PLL_LOCK        0x0
#define PLL35XX_PLL_LOCK_CONST  270
#define PLL35XX_PLL_CON0        0x100
#define PLL35XX_LOCKED_SHIFT    29
#define PLL35XX_LOCKED          (1 << PLL35XX_LOCKED_SHIFT)

struct samsung_clk_pll35xx {
	struct clk_hw		hw;
	const void __iomem	*base_reg;
	struct pll_pms          *pms;
};

static int get_index(unsigned long rate, struct pll_pms *pms)
{
	int i;

	for (i = 0; pms[i].f_out != F_OUT_INVAL; i++)
		if (pms[i].f_out <= rate)
			return i;

	return -EINVAL;
}

static inline unsigned long samsung_pll35xx_calc_f_out(u64 f_in,
							    int p, int m, int s)
{
	f_in *= m;
	do_div(f_in, (p << s));

	return (unsigned long) f_in;
}

#define to_clk_pll35xx(_hw) container_of(_hw, struct samsung_clk_pll35xx, hw)

static inline void samsung_pll35xx_get_mps(struct samsung_clk_pll35xx *pll,
					   u32 *m, u32 *p, u32 *s)
{
	u32 pll_con = __raw_readl(pll->base_reg + PLL35XX_PLL_CON0);

	*m = (pll_con >> PLL35XX_MDIV_SHIFT) & PLL35XX_MDIV_MASK;
	*p = (pll_con >> PLL35XX_PDIV_SHIFT) & PLL35XX_PDIV_MASK;
	*s = (pll_con >> PLL35XX_SDIV_SHIFT) & PLL35XX_SDIV_MASK;
}

static unsigned long samsung_pll35xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll35xx *pll = to_clk_pll35xx(hw);
	u32 mdiv, pdiv, sdiv;
	samsung_pll35xx_get_mps(pll, &mdiv, &pdiv, &sdiv);

	return samsung_pll35xx_calc_f_out(parent_rate, pdiv, mdiv, sdiv);
}

static long samsung_pll35xx_round_rate(struct clk_hw *hw,
				unsigned long drate, unsigned long *prate)
{
	struct samsung_clk_pll35xx *pll = to_clk_pll35xx(hw);
	struct pll_pms *pms = pll->pms;
	int i;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return samsung_pll35xx_recalc_rate(hw, *prate);
	}

	for (i = 0; pms[i].f_out != F_OUT_INVAL; i++)
		if (drate >= pms[i].f_out)
			return pms[i].f_out;

	return samsung_pll35xx_recalc_rate(hw, *prate);
}

static int samsung_pll35xx_set_rate(struct clk_hw *hw, unsigned long drate,
				unsigned long prate)
{
	struct samsung_clk_pll35xx *pll = to_clk_pll35xx(hw);
	u32 p = 0, m = 0, s = 0, tmp = 0;
	struct pll_pms *pms = pll->pms;
	u32 p_cur, m_cur, s_cur;
	int index;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return -ENOTSUPP;
	}

	index = get_index(drate, pll->pms);
	p = pms[index].p;
	m = pms[index].m;
	s = pms[index].s;

	samsung_pll35xx_get_mps(pll, &m_cur, &p_cur, &s_cur);

	if (p == p_cur && m == m_cur) {
		tmp = __raw_readl(pll->base_reg + PLL35XX_PLL_CON0);
		tmp &= ~(PLL35XX_SDIV_MASK << PLL35XX_SDIV_SHIFT);
		tmp |= s << PLL35XX_SDIV_SHIFT;
		__raw_writel(tmp, (u32 *)(pll->base_reg  + PLL35XX_PLL_CON0));

		return 0;
	}

	/* Define PLL lock time */
	__raw_writel((p * PLL35XX_PLL_LOCK_CONST),
		     (u32*) (pll->base_reg + PLL35XX_PLL_LOCK));

	/* Change PLL PMS */
	tmp = __raw_readl(pll->base_reg + PLL35XX_PLL_CON0);
	tmp &= ~((PLL35XX_PDIV_MASK << PLL35XX_PDIV_SHIFT) |
		(PLL35XX_MDIV_MASK << PLL35XX_MDIV_SHIFT) |
		(PLL35XX_SDIV_MASK << PLL35XX_SDIV_SHIFT));
	tmp |= (p << PLL35XX_PDIV_SHIFT) | (m << PLL35XX_MDIV_SHIFT) |
		(s << PLL35XX_SDIV_SHIFT);
	__raw_writel(tmp, (u32*) (pll->base_reg  + PLL35XX_PLL_CON0));

	/* Wait for locking */
	do {
		cpu_relax();
		tmp = __raw_readl(pll->base_reg + PLL35XX_PLL_CON0);
	} while (!(tmp & PLL35XX_LOCKED));

	return 0;
}

static const struct clk_ops samsung_pll35xx_clk_ops = {
	.recalc_rate = samsung_pll35xx_recalc_rate,
	.round_rate = samsung_pll35xx_round_rate,
	.set_rate = samsung_pll35xx_set_rate,
};

struct clk * __init
samsung_clk_register_pll35xx(const char *name,
			     const char *pname, const void __iomem *base_reg,
			     struct pll_pms *pms)
{
	struct samsung_clk_pll35xx *pll;
	struct clk *clk;
	struct clk_init_data init;
	int i;

	if (!pms) {
		pr_err("%s:  %s\n", __func__, name);
		return NULL;
	}

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll35xx_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->base_reg = base_reg;
	pll->pms = pms;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	/* Fill in received frequency table */
	for (i = 0; pms[i].f_out != F_OUT_INVAL; i++)
		pms[i].f_out =
			samsung_pll35xx_calc_f_out(clk_get_rate(clk_get_parent(clk)),
							pms[i].p,
							pms[i].m,
							pms[i].s);

	return clk;
}

/*
 * PLL36xx Clock Type
 */

#define PLL36XX_KDIV_MASK	(0xFFFF)
#define PLL36XX_MDIV_MASK	(0x1FF)
#define PLL36XX_PDIV_MASK	(0x3F)
#define PLL36XX_SDIV_MASK	(0x7)
#define PLL36XX_MFR_MASK	0xff
#define PLL36XX_MRR_MASK	0x1f
#define PLL36XX_KDIV_SHIFT	0
#define PLL36XX_MDIV_SHIFT	(16)
#define PLL36XX_PDIV_SHIFT	(8)
#define PLL36XX_SDIV_SHIFT	(0)
#define PLL36XX_MFR_SHIFT	16
#define PLL36XX_MRR_SHIFT	24

#define PLL36XX_PLL_LOCK	0x0
#define PLL36XX_PLL_CON0	0x100
#define PLL36XX_PLL_CON1	0x104
#define PLL36XX_PLL_CON2	0x108

#define PLL36XX_PLL_LOCK_CONST	3000
#define PLL36XX_PLL_CON0_LOCKED	(1 << 29)


struct samsung_clk_pll36xx {
	struct clk_hw	hw;
	void __iomem	*base;
	struct pll_pms	*pms;
};

static inline unsigned long samsung_pll36xx_calc_f_out(u64 f_in,
						u32 p, u32 m, u32 s, u32 k)
{
	f_in *= (m << 16) + k;
	do_div(f_in, (p << s));

	return (unsigned long)(f_in >> 16);
}

#define to_clk_pll36xx(_hw) container_of(_hw, struct samsung_clk_pll36xx, hw)

static unsigned long samsung_pll36xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll36xx *pll = to_clk_pll36xx(hw);
	u32 mdiv, pdiv, sdiv, pll_con0, pll_con1;
	s16 kdiv;

	pll_con0 = __raw_readl(pll->base + PLL36XX_PLL_CON0);
	pll_con1 = __raw_readl(pll->base + PLL36XX_PLL_CON1);
	mdiv = (pll_con0 >> PLL36XX_MDIV_SHIFT) & PLL36XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL36XX_PDIV_SHIFT) & PLL36XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL36XX_SDIV_SHIFT) & PLL36XX_SDIV_MASK;
	kdiv = (s16)(pll_con1 & PLL36XX_KDIV_MASK);

	return samsung_pll36xx_calc_f_out(parent_rate, pdiv, mdiv, sdiv, kdiv);
}

static long samsung_pll36xx_round_rate(struct clk_hw *hw,
				unsigned long drate, unsigned long *prate)
{
	struct samsung_clk_pll36xx *pll = to_clk_pll36xx(hw);
	struct pll_pms *pms = pll->pms;
	int i;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return samsung_pll36xx_recalc_rate(hw, *prate);
	}

	i = get_index(drate, pms);
	if (i >= 0)
		return pms[i].f_out;

	return samsung_pll36xx_recalc_rate(hw, *prate);
}

static int samsung_pll36xx_set_rate(struct clk_hw *hw, unsigned long drate,
				unsigned long prate)
{
	struct samsung_clk_pll36xx *pll = to_clk_pll36xx(hw);
	struct pll_pms *pms = pll->pms;
	u32 tmp;
	int index;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return -ENOTSUPP;
	}

	index = get_index(drate, pms);
	if (index < 0)
		return index;

	/* Define PLL lock time */
	__raw_writel(pms[index].p * PLL36XX_PLL_LOCK_CONST,
					pll->base + PLL36XX_PLL_LOCK);

	/* Change PLL divisors */
	tmp = __raw_readl(pll->base + PLL36XX_PLL_CON0);
	tmp &= ~((PLL36XX_PDIV_MASK << PLL36XX_PDIV_SHIFT) |
		(PLL36XX_MDIV_MASK << PLL36XX_MDIV_SHIFT) |
		(PLL36XX_SDIV_MASK << PLL36XX_SDIV_SHIFT));
	tmp |= (pms[index].p << PLL36XX_PDIV_SHIFT) |
		(pms[index].m << PLL36XX_MDIV_SHIFT) |
		(pms[index].s << PLL36XX_SDIV_SHIFT);
	__raw_writel(tmp, pll->base + PLL36XX_PLL_CON0);

	tmp = __raw_readl(pll->base + PLL36XX_PLL_CON1);
	tmp &= ~((PLL36XX_KDIV_MASK << PLL36XX_KDIV_SHIFT) |
		(PLL36XX_MFR_MASK << PLL36XX_MFR_SHIFT) |
		(PLL36XX_MRR_MASK << PLL36XX_MRR_SHIFT));
	tmp |= (pms[index].k << PLL36XX_KDIV_SHIFT) |
		(pms[index].mrr << PLL36XX_MRR_SHIFT) |
		(pms[index].mfr << PLL36XX_MFR_SHIFT);
	__raw_writel(tmp, pll->base + PLL36XX_PLL_CON1);

	/* Wait for locking */
	do {
		cpu_relax();
		tmp = __raw_readl(pll->base + PLL36XX_PLL_CON0);
	} while (!(tmp & PLL36XX_PLL_CON0_LOCKED));

	return 0;
}

static const struct clk_ops samsung_pll36xx_clk_ops = {
	.recalc_rate = samsung_pll36xx_recalc_rate,
	.round_rate = samsung_pll36xx_round_rate,
	.set_rate = samsung_pll36xx_set_rate,
};

struct clk * __init samsung_clk_register_pll36xx(const char *name,
		const char *pname, void __iomem *base, struct pll_pms *pms)
{
	struct samsung_clk_pll36xx *pll;
	struct clk *clk;
	struct clk_init_data init;
	unsigned long parent_rate;
	unsigned int i;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll36xx_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->base = base;
	pll->pms = pms;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	/* Fill in received frequency table */
	parent_rate = clk_get_rate(clk_get_parent(clk));
	for (i = 0; pms[i].f_out != F_OUT_INVAL; i++)
		pms[i].f_out = samsung_pll36xx_calc_f_out(parent_rate,
					pms[i].p, pms[i].m, pms[i].s, pms[i].k);

	return clk;
}

/*
 * PLL45xx Clock Type
 */

#define PLL45XX_MDIV_MASK	(0x3FF)
#define PLL45XX_PDIV_MASK	(0x3F)
#define PLL45XX_SDIV_MASK	(0x7)
#define PLL45XX_AFC_MASK	0x1f
#define PLL45XX_MDIV_SHIFT	(16)
#define PLL45XX_PDIV_SHIFT	(8)
#define PLL45XX_SDIV_SHIFT	(0)
#define PLL45XX_AFC_SHIFT	0

#define PLL45XX_PLL_LOCK        0x0
#define PLL45XX_PLL_LOCK_CONST  7200
#define PLL45XX_PLL_CON0        0x100
#define PLL45XX_PLL_CON1        0x104
#define PLL45XX_LOCKED_SHIFT    29
#define PLL45XX_LOCKED          (1 << PLL35XX_LOCKED_SHIFT)

struct samsung_clk_pll45xx {
	struct clk_hw		hw;
	enum pll45xx_type	type;
	void __iomem		*base_reg;
	struct pll_pms		*pms;
};

#define to_clk_pll45xx(_hw) container_of(_hw, struct samsung_clk_pll45xx, hw)

static inline unsigned long samsung_pll45xx_calc_f_out(
		struct samsung_clk_pll45xx *pll, u64 f_in, int p, int m, int s)
{
	if (pll->type == pll_4508)
		s = s - 1;

	f_in *= m;
	do_div(f_in, (p << s));

	return (unsigned long)f_in;
}

static unsigned long samsung_pll45xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll45xx *pll = to_clk_pll45xx(hw);
	u32 mdiv, pdiv, sdiv, pll_con;

	pll_con = __raw_readl(pll->base_reg + PLL45XX_PLL_CON0);
	mdiv = (pll_con >> PLL45XX_MDIV_SHIFT) & PLL45XX_MDIV_MASK;
	pdiv = (pll_con >> PLL45XX_PDIV_SHIFT) & PLL45XX_PDIV_MASK;
	sdiv = (pll_con >> PLL45XX_SDIV_SHIFT) & PLL45XX_SDIV_MASK;

	return samsung_pll45xx_calc_f_out(pll, parent_rate, pdiv, mdiv, sdiv);
}

static long samsung_pll45xx_round_rate(struct clk_hw *hw,
				unsigned long drate, unsigned long *prate)
{
	struct samsung_clk_pll45xx *pll = to_clk_pll45xx(hw);
	struct pll_pms *pms = pll->pms;
	int i;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return samsung_pll45xx_recalc_rate(hw, *prate);
	}

	for (i = 0; pms[i].f_out != F_OUT_INVAL; i++)
		if (drate >= pms[i].f_out)
			return pms[i].f_out;

	return samsung_pll45xx_recalc_rate(hw, *prate);
}

static int samsung_pll45xx_set_rate(struct clk_hw *hw, unsigned long drate,
				unsigned long prate)
{
	struct samsung_clk_pll45xx *pll = to_clk_pll45xx(hw);
	u32 p = 0, m = 0, s = 0, tmp = 0;
	u32 afc;
	struct pll_pms *pms = pll->pms;
	int index;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return -ENOTSUPP;
	}

	index = get_index(drate, pll->pms);

	/* Define PLL lock time */
	__raw_writel(PLL45XX_PLL_LOCK_CONST, pll->base_reg + PLL45XX_PLL_LOCK);

	/* Change PLL PMS */
	p = pms[index].p;
	m = pms[index].m;
	s = pms[index].s;
	afc = pms[index].afc;

	tmp = __raw_readl(pll->base_reg + PLL45XX_PLL_CON0);
	tmp &= ~((PLL45XX_PDIV_MASK << PLL45XX_PDIV_SHIFT) |
		(PLL45XX_MDIV_MASK << PLL45XX_MDIV_SHIFT) |
		(PLL45XX_SDIV_MASK << PLL45XX_SDIV_SHIFT));
	tmp |= (p << PLL45XX_PDIV_SHIFT) | (m << PLL45XX_MDIV_SHIFT) |
		(s << PLL45XX_SDIV_SHIFT);
	__raw_writel(tmp, pll->base_reg + PLL45XX_PLL_CON0);

	tmp = __raw_readl(pll->base_reg + PLL45XX_PLL_CON1);
	tmp &= ~(PLL45XX_AFC_MASK << PLL45XX_AFC_SHIFT);
	tmp |= (afc << PLL45XX_AFC_SHIFT);
	__raw_writel(tmp, pll->base_reg + PLL45XX_PLL_CON1);

	/* Wait for locking */
	do {
		cpu_relax();
		tmp = __raw_readl(pll->base_reg + PLL45XX_PLL_CON0);
	} while (!(tmp & PLL45XX_LOCKED));

	return 0;
}

static const struct clk_ops samsung_pll45xx_clk_ops = {
	.recalc_rate = samsung_pll45xx_recalc_rate,
	.round_rate = samsung_pll45xx_round_rate,
	.set_rate = samsung_pll45xx_set_rate,
};

struct clk * __init samsung_clk_register_pll45xx(const char *name,
			const char *pname, void __iomem *base_reg,
			enum pll45xx_type type, struct pll_pms *pms)
{
	struct samsung_clk_pll45xx *pll;
	struct clk *clk;
	struct clk_init_data init;
	unsigned long parent_rate;
	int i;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll45xx_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->base_reg = base_reg;
	pll->type = type;
	pll->pms = pms;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	/* Fill in received frequency table */
	parent_rate = clk_get_rate(clk_get_parent(clk));
	for (i = 0; pms[i].f_out != F_OUT_INVAL; i++)
		pms[i].f_out = samsung_pll45xx_calc_f_out(pll, parent_rate,
						pms[i].p, pms[i].m, pms[i].s);

	return clk;
}

/*
 * PLL46xx Clock Type
 */

#define PLL46XX_MDIV_MASK	(0x1FF)
#define PLL46XX_PDIV_MASK	(0x3F)
#define PLL46XX_SDIV_MASK	(0x7)
#define PLL46XX_MDIV_SHIFT	(16)
#define PLL46XX_PDIV_SHIFT	(8)
#define PLL46XX_SDIV_SHIFT	(0)

#define PLL46XX_KDIV_MASK	(0xFFFF)
#define PLL4650C_KDIV_MASK	(0xFFF)
#define PLL46XX_MFR_MASK	0x3f
#define PLL46XX_MRR_MASK	0x1f
#define PLL46XX_KDIV_SHIFT	(0)
#define PLL46XX_MFR_SHIFT	16
#define PLL46XX_MRR_SHIFT	24

#define PLL46XX_PLL_LOCK	0x0
#define PLL46XX_PLL_CON0	0x100
#define PLL46XX_PLL_CON1	0x104
#define PLL4600X_PLL_CON0	0x4
#define PLL4600X_PLL_CON1	0x8

#define PLL46XX_PLL_LOCK_CONST	3000
#define PLL46XX_PLL_CON0_LOCKED	(1 << 29)

struct samsung_clk_pll46xx {
	struct clk_hw		hw;
	enum pll46xx_type	type;
	void __iomem		*base;
	struct pll_pms		*pms;
	unsigned int		con0_offset;
	unsigned int		con1_offset;
};

#define to_clk_pll46xx(_hw) container_of(_hw, struct samsung_clk_pll46xx, hw)

static inline unsigned long samsung_pll46xx_calc_f_out(u64 f_in,
			u32 p, u32 m, u32 s, u32 k, enum pll46xx_type type)
{
	u8 shift = (type == pll_4600 || type == pll_4600x) ? 16 : 10;

	f_in *= (m << shift) + k;
	do_div(f_in, (p << s));

	return (unsigned long)(f_in >> shift);
}

static unsigned long samsung_pll46xx_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll46xx *pll = to_clk_pll46xx(hw);
	u32 mdiv, pdiv, sdiv, kdiv, pll_con0, pll_con1;

	pll_con0 = __raw_readl(pll->base + pll->con0_offset);
	pll_con1 = __raw_readl(pll->base + pll->con1_offset);
	mdiv = (pll_con0 >> PLL46XX_MDIV_SHIFT) & PLL46XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL46XX_PDIV_SHIFT) & PLL46XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL46XX_SDIV_SHIFT) & PLL46XX_SDIV_MASK;
	kdiv = pll->type == pll_4650c ? pll_con1 & PLL4650C_KDIV_MASK :
					pll_con1 & PLL46XX_KDIV_MASK;

	return samsung_pll46xx_calc_f_out(parent_rate,
					pdiv, mdiv, sdiv, kdiv, pll->type);
}

static long samsung_pll46xx_round_rate(struct clk_hw *hw,
				unsigned long drate, unsigned long *prate)
{
	struct samsung_clk_pll46xx *pll = to_clk_pll46xx(hw);
	struct pll_pms *pms = pll->pms;
	int i;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return samsung_pll46xx_recalc_rate(hw, *prate);
	}

	i = get_index(drate, pms);
	if (i >= 0)
		return pms[i].f_out;

	return samsung_pll46xx_recalc_rate(hw, *prate);
}

static int samsung_pll46xx_set_rate(struct clk_hw *hw, unsigned long drate,
				unsigned long prate)
{
	struct samsung_clk_pll46xx *pll = to_clk_pll46xx(hw);
	struct pll_pms *pms = pll->pms;
	u32 tmp;
	int index;

	if (!pms) {
		pr_err("%s: no pms table passed", __func__);
		return -ENOTSUPP;
	}

	index = get_index(drate, pms);
	if (index < 0)
		return index;

	/* Define PLL lock time */
	__raw_writel(pms[index].p * PLL46XX_PLL_LOCK_CONST,
					pll->base + PLL46XX_PLL_LOCK);

	/* Change PLL divisors */
	tmp = __raw_readl(pll->base + pll->con0_offset);
	tmp &= ~((PLL46XX_PDIV_MASK << PLL46XX_PDIV_SHIFT) |
		(PLL46XX_MDIV_MASK << PLL46XX_MDIV_SHIFT) |
		(PLL46XX_SDIV_MASK << PLL46XX_SDIV_SHIFT));
	tmp |= (pms[index].p << PLL46XX_PDIV_SHIFT) |
		(pms[index].m << PLL46XX_MDIV_SHIFT) |
		(pms[index].s << PLL46XX_SDIV_SHIFT);
	__raw_writel(tmp, pll->base + pll->con0_offset);

	tmp = __raw_readl(pll->base + pll->con1_offset);
	tmp &= ~((PLL46XX_KDIV_MASK << PLL46XX_KDIV_SHIFT) |
		(PLL46XX_MFR_MASK << PLL46XX_MFR_SHIFT) |
		(PLL46XX_MRR_MASK << PLL46XX_MRR_SHIFT));
	tmp |= (pms[index].k << PLL46XX_KDIV_SHIFT) |
		(pms[index].mrr << PLL46XX_MRR_SHIFT) |
		(pms[index].mfr << PLL46XX_MFR_SHIFT);
	__raw_writel(tmp, pll->base + pll->con1_offset);

	/* Wait for locking */
	do {
		cpu_relax();
		tmp = __raw_readl(pll->base + pll->con0_offset);
	} while (!(tmp & PLL46XX_PLL_CON0_LOCKED));

	return 0;
}


static const struct clk_ops samsung_pll46xx_clk_ops = {
	.recalc_rate = samsung_pll46xx_recalc_rate,
	.round_rate = samsung_pll46xx_round_rate,
	.set_rate = samsung_pll46xx_set_rate,
};

struct clk * __init samsung_clk_register_pll46xx(const char *name,
				const char *pname, void __iomem *base,
				enum pll46xx_type type, struct pll_pms *pms)
{
	struct samsung_clk_pll46xx *pll;
	struct clk *clk;
	struct clk_init_data init;
	unsigned long parent_rate;
	unsigned int i;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll46xx_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->base = base;
	pll->type = type;
	pll->pms = pms;

	if (type == pll_4600x) {
		pll->con0_offset = PLL4600X_PLL_CON0;
		pll->con1_offset = PLL4600X_PLL_CON1;
	} else {
		pll->con0_offset = PLL46XX_PLL_CON0;
		pll->con1_offset = PLL46XX_PLL_CON1;
	}

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	/* Fill in received frequency table */
	parent_rate = clk_get_rate(clk_get_parent(clk));
	for (i = 0; pms[i].f_out != F_OUT_INVAL; i++)
		pms[i].f_out = samsung_pll46xx_calc_f_out(parent_rate,
				pms[i].p, pms[i].m, pms[i].s, pms[i].k, type);

	return clk;
}

/*
 * PLL2550x Clock Type
 */

#define PLL2550X_R_MASK       (0x1)
#define PLL2550X_P_MASK       (0x3F)
#define PLL2550X_M_MASK       (0x3FF)
#define PLL2550X_S_MASK       (0x7)
#define PLL2550X_R_SHIFT      (20)
#define PLL2550X_P_SHIFT      (14)
#define PLL2550X_M_SHIFT      (4)
#define PLL2550X_S_SHIFT      (0)

struct samsung_clk_pll2550x {
	struct clk_hw		hw;
	const void __iomem	*reg_base;
	unsigned long		offset;
};

#define to_clk_pll2550x(_hw) container_of(_hw, struct samsung_clk_pll2550x, hw)

static unsigned long samsung_pll2550x_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct samsung_clk_pll2550x *pll = to_clk_pll2550x(hw);
	u32 r, p, m, s, pll_stat;
	u64 fvco = parent_rate;

	pll_stat = __raw_readl(pll->reg_base + pll->offset * 3);
	r = (pll_stat >> PLL2550X_R_SHIFT) & PLL2550X_R_MASK;
	if (!r)
		return 0;
	p = (pll_stat >> PLL2550X_P_SHIFT) & PLL2550X_P_MASK;
	m = (pll_stat >> PLL2550X_M_SHIFT) & PLL2550X_M_MASK;
	s = (pll_stat >> PLL2550X_S_SHIFT) & PLL2550X_S_MASK;

	fvco *= m;
	do_div(fvco, (p << s));

	return (unsigned long)fvco;
}

static const struct clk_ops samsung_pll2550x_clk_ops = {
	.recalc_rate = samsung_pll2550x_recalc_rate,
};

struct clk * __init samsung_clk_register_pll2550x(const char *name,
			const char *pname, const void __iomem *reg_base,
			const unsigned long offset)
{
	struct samsung_clk_pll2550x *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &samsung_pll2550x_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->reg_base = reg_base;
	pll->offset = offset;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	if (clk_register_clkdev(clk, name, NULL))
		pr_err("%s: failed to register lookup for %s", __func__, name);

	return clk;
}
