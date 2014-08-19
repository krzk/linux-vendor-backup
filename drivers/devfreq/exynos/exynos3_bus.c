/*
 * drivers/devfreq/exynos/exynos3_bus.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 * Chanwoo Choi <cw00.choi@samsung.com>
 *
 * based on drivers/devfreqw/exynos/exynos4_bus.c
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *
 * EXYNOS3250 - Memory/Bus clock frequency scaling support in DEVFREQ framework
 * This version supports EXYNOS3250 only.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/opp.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/suspend.h>

#include <mach/regs-clock.h>

#include "exynos_ppmu.h"
#include "../governor.h"

#define EX3250_BUS_SATURATION_RATIO	40
#define EX3250_INT_REGULATOR_NAME	"vdd_int"
#define EX3250_MIF_REGULATOR_NAME	"vdd_mif"

#define EX3250_INTERVAL_SAFEVOLT	25000	/* 25mV */

/*
 * FIXME: exynos3250_bus driver need the ASV (Adaptive Supply Voltage) version
 * the method for getting asv value should use global variabel because there
 * is no proper subsystem for ASV.
 */
#define exynos_result_of_asv		0

enum exynos3_busf_type {
	TYPE_BUSF_UNKNOWN = 0,
	TYPE_BUSF_EXYNOS3250_MIF,
	TYPE_BUSF_EXYNOS3250_INT,
};

enum busfreq_level {
	LV_0,
	LV_1,
	LV_2,
	LV_3,
	LV_4,
	LV_5,

	LV_END,
};

enum exynos_ppmu_mif {
	PPMU_DMC0 = 0,
	PPMU_DMC1,

	PPMU_MIF_END,
};

enum exynos_ppmu_int {
	PPMU_LEFT = 0,
	PPMU_RIGHT,

	PPMU_INT_END,
};

static const char *exynos_mif_ppmu_clk_name[] = {
	[PPMU_DMC0]	= "ppmudmc0",
	[PPMU_DMC1]	= "ppmudmc1",
};

static const char *exynos_int_ppmu_clk_name[] = {
	[PPMU_LEFT]	= "ppmuleft",
	[PPMU_RIGHT]	= "ppmuright",
};

/**
 * struct busfreq_opp_info - opp information for bus
 * @rate:	Frequency in hertz
 * @volt:	Voltage in microvolts corresponding to this OPP
 */
struct busfreq_opp_info {
	unsigned long rate;
	unsigned long volt;
};

struct busfreq_data {
	struct devfreq_dev_profile *profile;
	enum exynos3_busf_type type;
	struct device *dev;
	struct devfreq *devfreq;
	bool disabled;

	struct busfreq_opp_info curr_opp_info;

	struct regulator *regulator_vdd;
	struct exynos_ppmu *ppmu;
	struct clk **clk_ppmu;
	int ppmu_end;

	struct bus_opp_table *opp_table;
	int opp_table_end;

	struct notifier_block pm_notifier;
	struct mutex lock;

	/* Dividers calculated early */
	unsigned int dmc_div_table[LV_END];

	unsigned int lbus_div_table[LV_END];
	unsigned int rbus_div_table[LV_END];
	unsigned int top_div_table[LV_END];
	unsigned int acp0_div_table[LV_END];
	unsigned int mfc_div_table[LV_END];
};

static unsigned int mif_asv_volt_exynos3250[][15] = {
	/*  ASV0    ASV1    ASV2    ASV3    ASV4    ASV5    ASV6    ASV7    ASV8    ASV9   ASV10   ASV11   ASV12   ASV13   ASV14 */
	{ 875000, 875000, 850000, 825000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000},	/* L0 - 400MHz */
	{ 825000, 825000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000},	/* L1 - 200MHz */
	{ 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000},	/* L2 - 133MHz */
	{ 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000},	/* L3 - 100MHz */
	{ 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000, 800000},	/* L4 - 50MHz  */
};

static unsigned int int_asv_volt_exynos3250[][15] = {
	/*  ASV0     ASV1    ASV2    ASV3    ASV4    ASV5    ASV6    ASV7    ASV8    ASV9   ASV10   ASV11   ASV12   ASV13   ASV14 */
	{ 950000,  950000, 925000, 925000, 900000, 875000, 875000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000},	/* L0 - 135MHz */
	{ 950000,  950000, 925000, 925000, 900000, 875000, 875000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000},	/* L1 - 134MHz */
	{ 925000,  925000, 900000, 900000, 875000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000},	/* L2 - 133MHz */
	{ 850000,  850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000},	/* L3 - 100MHz */
	{ 850000,  850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000},	/* L4 - 80MHz  */
	{ 850000,  850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000, 850000},	/* L5 - 50MHz  */
};

static struct bus_opp_table exynos3250_mifclk_table[] = {
	{LV_0,	400000,	875000},
	{LV_1,	200000,	800000},
	{LV_2,	133000,	800000},
	{LV_3,	100000,	800000},
	{LV_4,	 50000,	800000},
};

static struct bus_opp_table exynos3250_intclk_table[] = {
	{LV_0,	135000,	950000},
	{LV_1,	134000,	950000},
	{LV_2,	133000, 925000},
	{LV_3,	100000, 850000},
	{LV_4,	80000,  850000},
	{LV_5,	50000,  850000},
};

/*
 * Clock Divider Data for Exynos3250's MIF block
 */
static unsigned int exynos3250_mif_clkdiv_dmc1[][3] = {
	/*
	 * Clock divider value for following
	 * { SCLK_DMC, ACLK_DMCD, ACLK_DMCP }
	 */

	{ 0, 1, 1 },		/* DMC1 L0: 400MHz */
	{ 1, 1, 1 },		/* DMC1 L1: 200MHz */
	{ 2, 1, 1 },		/* DMC1 L2: 133MHz */
	{ 3, 1, 1 },		/* DMC1 L3: 100MHz */
	{ 7, 1, 1 },		/* DMC1 L4:  50MHz */
};

/*
 * Clock Divider Data for Exynos3250's INT block
 */
static unsigned int exynos3250_int_clkdiv_top[][5] = {
	/*
	 * Clock divider value for following
	 * { ACLK_266, ACLK_160, ACLK_200, ACLK_100, ACLK_400 }
	 */

	{ 2, 3, 2, 7, 0 },	/* TOP L0: 135MHz */
	{ 3, 3, 2, 7, 0 },	/* TOP L1: 134MHz */
	{ 7, 3, 2, 7, 7 },	/* TOP L2: 133MHz */
	{ 7, 4, 3, 7, 7 },	/* TOP L3: 100MHz */
	{ 7, 5, 4, 7, 7 },	/* TOP L4:  80MHz */
	{ 7, 7, 7, 7, 7 },	/* TOP L5:  50MHz */
};

static unsigned int exynos3250_int_clkdiv_lbus[][2] = {
	/*
	 * Clock divider value for following
	 * { ACLK_GDL, ACLK_GPL }
	 */

	{ 2, 1 },		/* LEFT BUS L0: 135MHz */
	{ 2, 1 },		/* LEFT BUS L1: 134MHz */
	{ 2, 1 },		/* LEFT BUS L2: 133MHz */
	{ 3, 1 },		/* LEFT BUS L3: 100MHz */
	{ 3, 1 },		/* LEFT BUS L4:  80MHz */
	{ 3, 1 },		/* LEFT BUS L5:  50MHz */
};

static unsigned int exynos3250_int_clkdiv_rbus[][2] = {
	/*
	 * Clock divider value for following
	 * { ACLK_GDR, ACLK_GPR }
	 */

	{ 2, 1 },		/* RIGHT BUS L0: 135MHz */
	{ 2, 1 },		/* RIGHT BUS L1: 134MHz */
	{ 2, 1 },		/* RIGHT BUS L2: 133MHz */
	{ 3, 1 },		/* RIGHT BUS L3: 100MHz */
	{ 3, 1 },		/* RIGHT BUS L4:  80MHz */
	{ 3, 1 },		/* RIGHT BUS L5:  50MHz */
};

static unsigned int exynos3250_int_clkdiv_acp0[][5] = {
	/*
	 * Clock divider value for following
	 * { ACLK_ACP, PCLK_ACP, ACP_DMC, ACP_CORED, ACP_COREP }
	 */

	{ 7, 1, 3, 1, 1},	/* ACP BUS L0: 135MHz */
	{ 7, 1, 3, 1, 1},	/* ACP BUS L1: 134MHz */
	{ 7, 1, 3, 1, 1},	/* ACP BUS L2: 133MHz */
	{ 7, 1, 3, 1, 1},	/* ACP BUS L3: 100MHz */
	{ 7, 1, 3, 1, 1},	/* ACP BUS L4:  80MHz */
	{ 7, 1, 3, 1, 1},	/* ACP BUS L5:  50MHz */
};

static unsigned int exynos3250_int_clkdiv_sclk_mfc[][1] = {
	/*
	 * Clock divider value for following
	 * { SCLK_MFC }
	 */

	{ 1 },			/* SCLK_MFC BUS L0: 135MHz */
	{ 1 },			/* SCLK_MFC BUS L1: 134MHz */
	{ 1 },			/* SCLK_MFC BUS L2: 133MHz */
	{ 2 },			/* SCLK_MFC BUS L3: 100MHz */
	{ 3 },			/* SCLK_MFC BUS L4:  80MHz */
	{ 4 },			/* SCLK_MFC BUS L5:  50MHz */
};

/*
 * PPMU helper function to read PPMU cycle count
 */
static void busfreq_mon_reset(struct busfreq_data *data)
{
	unsigned int i;

	for (i = 0; i < data->ppmu_end; i++) {
		void __iomem *ppmu_base = data->ppmu[i].hw_base;

		/* Reset the performance and cycle counters */
		exynos_ppmu_reset(ppmu_base);

		/* Setup count registers to monitor read/write transactions */
		data->ppmu[i].event[PPMU_PMNCNT3] = RDWR_DATA_COUNT;
		exynos_ppmu_setevent(ppmu_base, PPMU_PMNCNT3,
					data->ppmu[i].event[PPMU_PMNCNT3]);

		exynos_ppmu_start(ppmu_base);
	}
}

static void exynos_read_ppmu(struct busfreq_data *data)
{
	int i, j;

	for (i = 0; i < data->ppmu_end; i++) {
		void __iomem *ppmu_base = data->ppmu[i].hw_base;

		exynos_ppmu_stop(ppmu_base);

		/* Update local data from PPMU */
		data->ppmu[i].ccnt = __raw_readl(ppmu_base + PPMU_CCNT);

		for (j = PPMU_PMNCNT0; j < PPMU_PMNCNT_MAX; j++) {
			if (data->ppmu[i].event[j] == 0)
				data->ppmu[i].count[j] = 0;
			else
				data->ppmu[i].count[j] =
					exynos_ppmu_read(ppmu_base, j);
		}
	}

	busfreq_mon_reset(data);
}

static int exynos_get_busier_ppmu(struct busfreq_data *data)
{
	unsigned int count = 0;
	int i, j, busy = 0;

	for (i = 0; i < data->ppmu_end; i++) {
		for (j = PPMU_PMNCNT0; j < PPMU_PMNCNT_MAX; j++) {
			if (data->ppmu[i].count[j] > count) {
				count = data->ppmu[i].count[j];
				busy = i;
			}
		}
	}

	return busy;
}

/*
 * Set voltage/clock for memory bus
 */
static int exynos3250_bus_set_clk(struct busfreq_data *data,
				  struct busfreq_opp_info *new_opp_info)
{
	u32 clk_div;
	int index;

	for (index = 0; index < data->opp_table_end; index++)
		if (new_opp_info->rate == data->opp_table[index].clk)
			break;

	if (index == data->opp_table_end)
		return -EINVAL;

	switch (data->type) {
	case TYPE_BUSF_EXYNOS3250_MIF:
		/*
		 * Set new divider for CLK_DIV_DMC1 and Wait until divider
		 * status is changed to stable state.
		 */
		clk_div = data->dmc_div_table[index];
		__raw_writel(clk_div, EXYNOS3_CLKDIV_DMC1);

		do {
			clk_div = __raw_readl(EXYNOS3_CLKDIV_STAT_DMC0);
		} while (clk_div & (EXYNOS3_CLKDIV_STAT_DMC0_DMC_MASK |
				    EXYNOS3_CLKDIV_STAT_DMC0_DMCD_MASK |
				    EXYNOS3_CLKDIV_STAT_DMC0_DMCP_MASK));
		break;
	case TYPE_BUSF_EXYNOS3250_INT:
		/*
		 * Set new divider for CLK_DIV_LEFTBUS and Wait until divider
		 * status is changed to stable state.
		 */
		clk_div = data->lbus_div_table[index];
		__raw_writel(clk_div, EXYNOS3_CLKDIV_LEFTBUS);

		do {
			clk_div = __raw_readl(EXYNOS3_CLKDIV_STAT_LEFTBUS);
		} while (clk_div & (EXYNOS3_CLKDIV_STAT_LEFTBUS_GDL_MASK |
				    EXYNOS3_CLKDIV_STAT_LEFTBUS_GPL_MASK));

		/*
		 * Set new divider for CLK_DIV_RIGHTBUS and Wait until divider
		 * status is changed to stable state.
		 */
		clk_div = data->rbus_div_table[index];
		__raw_writel(clk_div, EXYNOS3_CLKDIV_RIGHTBUS);

		do {
			clk_div = __raw_readl(EXYNOS3_CLKDIV_STAT_RIGHTBUS);
		} while (clk_div & (EXYNOS3_CLKDIV_STAT_RIGHTBUS_GDR_MASK |
				    EXYNOS3_CLKDIV_STAT_RIGHTBUS_GPR_MASK));

		/*
		 * Set new divider for CLK_DIV_TOP and Wait until divider
		 * status is changed to stable state.
		 */
		clk_div = data->top_div_table[index];
		__raw_writel(clk_div, EXYNOS3_CLKDIV_TOP);

		do {
			clk_div = __raw_readl(EXYNOS3_CLKDIV_STAT_TOP);
		} while (clk_div & (EXYNOS3_CLKDIV_STAT_TOP_ACLK_266_MASK |
				    EXYNOS3_CLKDIV_STAT_TOP_ACLK_160_MASK |
				    EXYNOS3_CLKDIV_STAT_TOP_ACLK_200_MASK |
				    EXYNOS3_CLKDIV_STAT_TOP_ACLK_100_MASK |
				    EXYNOS3_CLKDIV_STAT_TOP_ACLK_400_MASK));

		/*
		 * Set new divider for CLK_DIV_ACP0 and Wait until divider
		 * status is changed to stable state.
		 */
		clk_div = data->acp0_div_table[index];
		__raw_writel(clk_div, EXYNOS3_CLKDIV_ACP0);

		do {
			clk_div = __raw_readl(EXYNOS3_CLKDIV_STAT_ACP0);
		} while (clk_div & (EXYNOS3_CLKDIV_STAT_ACP0_ACP_MASK |
				    EXYNOS3_CLKDIV_STAT_ACP0_PCLK_MASK |
				    EXYNOS3_CLKDIV_STAT_ACP0_DMC_MASK |
				    EXYNOS3_CLKDIV_STAT_ACP0_CORED_MASK |
				    EXYNOS3_CLKDIV_STAT_ACP0_COREP_MASK));

		/*
		 * Set new divider for CLK_DIV_MFC and Wait until divider
		 * status is changed to stable state.
		 */
		clk_div = data->mfc_div_table[index];
		__raw_writel(clk_div, EXYNOS3_CLKDIV_MFC);

		do {
			clk_div = __raw_readl(EXYNOS3_CLKDIV_STAT_MFC);
		} while (clk_div & (EXYNOS3_CLKDIV_STAT_MFC_MFC_MASK));

		break;
	default:
		dev_err(data->dev, "Unknown device type %d\n", data->type);
		return -EINVAL;
	};

	return 0;
}

static int exynos3_bus_set_volt(struct busfreq_data *data,
				struct busfreq_opp_info *new_opp_info,
				struct busfreq_opp_info *old_opp_info)
{
	int ret, max_safevolt;

	switch (data->type) {
	case TYPE_BUSF_EXYNOS3250_MIF:
		max_safevolt = new_opp_info->volt + EX3250_INTERVAL_SAFEVOLT;
		break;
	case TYPE_BUSF_EXYNOS3250_INT:
		/*
		 * FIXME: exynos3_bus.c has voltage setting issue
		 * for INT block. If INT block's voltage is changed
		 * before completed platform boot, mmc happen timing
		 * issue or stop platform boot. After fix it,
		 * max_safevolt of INT block should add 'INTERVAL_SAFEVOLT'.
		 */
		max_safevolt = new_opp_info->volt;
		break;
	default:
		dev_err(data->dev, "Unknown device type %d\n", data->type);
		return -EINVAL;
	}

	ret = regulator_set_voltage(data->regulator_vdd, new_opp_info->volt,
				    max_safevolt);
	if (ret < 0) {
		dev_err(data->dev, "Failed to set voltage %d\n", data->type);
		regulator_set_voltage(data->regulator_vdd, old_opp_info->volt,
				      max_safevolt);
	}

	return 0;
}

/*
 * Define internal function of structure devfreq_dev_profile
 */
static int exynos3_bus_target(struct device *dev, unsigned long *_freq,
			      u32 flags)
{
	struct busfreq_data *data = dev_get_drvdata(dev);
	struct busfreq_opp_info	new_opp_info;
	unsigned long old_freq, new_freq;
	struct opp *opp;
	int ret = 0;

	if (data->disabled)
		goto out;

	/* Get new opp-info instance according to new busfreq clock */
	rcu_read_lock();
	opp = devfreq_recommended_opp(dev, _freq, flags);
	if (IS_ERR_OR_NULL(opp)) {
		dev_err(dev, "Failed to get recommed opp instance\n");
		rcu_read_unlock();
		return PTR_ERR(opp);
	}
	new_opp_info.rate = opp_get_freq(opp);
	new_opp_info.volt = opp_get_voltage(opp);
	rcu_read_unlock();

	old_freq = data->curr_opp_info.rate;
	new_freq = new_opp_info.rate;
	if (old_freq == new_freq)
		return 0;

	/* Change voltage/clock according to new busfreq level */
	mutex_lock(&data->lock);

	dev_dbg(dev, "%ld Hz -> %ld Hz\n", old_freq, new_freq);

	if (old_freq < new_freq) {
		ret = exynos3_bus_set_volt(data, &new_opp_info,
					   &data->curr_opp_info);
		if (ret < 0) {
			dev_err(dev, "Failed to set voltage %d\n", data->type);
			goto out;
		}
	}

	ret = exynos3250_bus_set_clk(data, &new_opp_info);
	if (ret < 0) {
		dev_err(dev, "Failed to set bus clock %d\n", data->type);
		goto out;
	}

	if (old_freq > new_freq) {
		ret = exynos3_bus_set_volt(data, &new_opp_info,
					   &data->curr_opp_info);
		if (ret < 0) {
			dev_err(dev, "Failed to set voltage %d\n", data->type);
			goto out;
		}
	}

	data->curr_opp_info = new_opp_info;
out:
	mutex_unlock(&data->lock);

	return ret;
}

static int exynos3_bus_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct busfreq_data *data = dev_get_drvdata(dev);
	int busier;

	/* Read PPMU total cycle count and Read/Write count */
	exynos_read_ppmu(data);

	/* Get busier PPMU device among various PPMU */
	busier = exynos_get_busier_ppmu(data);
	stat->current_frequency = data->curr_opp_info.rate;

	/* Number of cycles spent on memory access */
	stat->busy_time = data->ppmu[busier].count[PPMU_PMNCNT3];
	stat->busy_time *= 100 / EX3250_BUS_SATURATION_RATIO;
	stat->total_time = data->ppmu[busier].ccnt;

	/* If the counters have overflown, retry */
	if (data->ppmu[busier].ccnt_overflow ||
		data->ppmu[busier].count_overflow[0])
		return -EAGAIN;

	return 0;
}

static void exynos3_bus_exit(struct device *dev)
{
	struct busfreq_data *data = dev_get_drvdata(dev);
	int i;

	/*
	 * Un-map memory man and disable regulator/clocks
	 * to prevent power leakage.
	 */
	regulator_disable(data->regulator_vdd);

	for (i = 0; i < data->ppmu_end; i++) {
		if (data->clk_ppmu[i])
			clk_disable_unprepare(data->clk_ppmu[i]);
	}

	for (i = 0; i < data->ppmu_end; i++) {
		if (data->ppmu[i].hw_base)
			iounmap(data->ppmu[i].hw_base);

	}
}

/* Define devfreq_dev_profile for MIF block */
static struct devfreq_dev_profile exynos3_busfreq_mif_profile = {
	.initial_freq	= 400000,
	.polling_ms	= 100,
	.target		= exynos3_bus_target,
	.get_dev_status	= exynos3_bus_get_dev_status,
	.exit		= exynos3_bus_exit,
};

/* Define devfreq_dev_profile for INT block */
static struct devfreq_dev_profile exynos3_busfreq_int_profile = {
	.initial_freq	= 135000,
	.polling_ms	= 100,
	.target		= exynos3_bus_target,
	.get_dev_status	= exynos3_bus_get_dev_status,
	.exit		= exynos3_bus_exit,
};

static struct devfreq_simple_ondemand_data exynos3_busfreq_ondemand_data = {
	.upthreshold	= 40,
};

static int exynos3_busfreq_pm_notifier_event(struct notifier_block *this,
					     unsigned long event, void *ptr)
{
	struct busfreq_data *data = container_of(this, struct busfreq_data,
						 pm_notifier);
	struct opp *opp;
	struct busfreq_opp_info	new_opp_info;
	unsigned long maxfreq = ULONG_MAX;
	int err = 0;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&data->lock);

		data->disabled = true;

		rcu_read_lock();
		opp = opp_find_freq_floor(data->dev, &maxfreq);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			dev_err(data->dev, "%s: unable to find a min freq\n",
				__func__);
			mutex_unlock(&data->lock);
			return PTR_ERR(opp);
		}
		new_opp_info.rate = opp_get_freq(opp);
		new_opp_info.volt = opp_get_voltage(opp);
		rcu_read_unlock();

		err = exynos3_bus_set_volt(data, &new_opp_info,
					   &data->curr_opp_info);
		if (err) {
			mutex_unlock(&data->lock);
			return err;
		}

		err = exynos3250_bus_set_clk(data, &new_opp_info);
		if (err) {
			mutex_unlock(&data->lock);
			return err;
		}

		data->curr_opp_info = new_opp_info;

		mutex_unlock(&data->lock);
			return err;
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		/* Reactivate */
		mutex_lock(&data->lock);
		data->disabled = false;
		mutex_unlock(&data->lock);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static int exynos3250_busfreq_init_table(struct busfreq_data *data)
{
	int i, ret;
	int asv = exynos_result_of_asv;
	u32 clk_div, clk_div_lbus, clk_div_rbus;
	u32 clk_div_top, clk_div_acp0, clk_div_mfc;

	switch (data->type) {
	case TYPE_BUSF_EXYNOS3250_MIF:
		/* CLK_DIV_DMC1 */
		clk_div = __raw_readl(EXYNOS3_CLKDIV_DMC1);
		for (i = 0; i < data->opp_table_end; i++) {
			clk_div &= ~(EXYNOS3_CLKDIV_DMC1_DMC_MASK  |
				     EXYNOS3_CLKDIV_DMC1_DMCD_MASK |
				     EXYNOS3_CLKDIV_DMC1_DMCP_MASK);

			clk_div |= ((exynos3250_mif_clkdiv_dmc1[i][0] <<
				     EXYNOS3_CLKDIV_DMC1_DMC_SHIFT) |
				    (exynos3250_mif_clkdiv_dmc1[i][1] <<
				     EXYNOS3_CLKDIV_DMC1_DMCD_SHIFT) |
				    (exynos3250_mif_clkdiv_dmc1[i][2] <<
				     EXYNOS3_CLKDIV_DMC1_DMCP_SHIFT));

			data->dmc_div_table[i] = clk_div;

			/* Support ASV mode for MIF voltage table */
			data->opp_table[i].volt = mif_asv_volt_exynos3250[i][asv];
		}
		break;
	case TYPE_BUSF_EXYNOS3250_INT:
		clk_div_lbus = __raw_readl(EXYNOS3_CLKDIV_LEFTBUS);
		clk_div_rbus = __raw_readl(EXYNOS3_CLKDIV_RIGHTBUS);
		clk_div_top  = __raw_readl(EXYNOS3_CLKDIV_TOP);
		clk_div_acp0 = __raw_readl(EXYNOS3_CLKDIV_ACP0);
		clk_div_mfc  = __raw_readl(EXYNOS3_CLKDIV_MFC);

		for (i = 0; i < data->opp_table_end; i++) {
			/* CLK_DIV_LEFTBUS */
			clk_div_lbus &= ~(EXYNOS3_CLKDIV_LEFTBUS_GDL_MASK |
					  EXYNOS3_CLKDIV_LEFTBUS_GPL_MASK);

			clk_div_lbus |= ((exynos3250_int_clkdiv_lbus[i][0] <<
					  EXYNOS3_CLKDIV_LEFTBUS_GDL_SHIFT) |
					 (exynos3250_int_clkdiv_lbus[i][1] <<
					  EXYNOS3_CLKDIV_LEFTBUS_GPL_SHIFT));

			data->lbus_div_table[i] = clk_div_lbus;

			/* CLK_DIV_RIGHTBUS */
			clk_div_rbus &= ~(EXYNOS3_CLKDIV_RIGHTBUS_GDR_MASK  |
					  EXYNOS3_CLKDIV_RIGHTBUS_GPR_MASK);

			clk_div_rbus |= ((exynos3250_int_clkdiv_rbus[i][0] <<
					  EXYNOS3_CLKDIV_RIGHTBUS_GDR_SHIFT) |
					 (exynos3250_int_clkdiv_rbus[i][1] <<
					  EXYNOS3_CLKDIV_RIGHTBUS_GPR_SHIFT));

			data->rbus_div_table[i] = clk_div_rbus;

			/* CLK_DIV_TOP */
			clk_div_top &= ~(EXYNOS3_CLKDIV_TOP_ACLK_266_MASK |
					 EXYNOS3_CLKDIV_TOP_ACLK_160_MASK |
					 EXYNOS3_CLKDIV_TOP_ACLK_200_MASK |
					 EXYNOS3_CLKDIV_TOP_ACLK_100_MASK |
					 EXYNOS3_CLKDIV_TOP_ACLK_400_MASK);

			clk_div_top |= ((exynos3250_int_clkdiv_top[i][0] <<
					 EXYNOS3_CLKDIV_TOP_ACLK_266_SHIFT) |
					(exynos3250_int_clkdiv_top[i][1] <<
					 EXYNOS3_CLKDIV_TOP_ACLK_160_SHIFT) |
					(exynos3250_int_clkdiv_top[i][2] <<
					 EXYNOS3_CLKDIV_TOP_ACLK_200_SHIFT) |
					(exynos3250_int_clkdiv_top[i][3] <<
					 EXYNOS3_CLKDIV_TOP_ACLK_100_SHIFT) |
					(exynos3250_int_clkdiv_top[i][4] <<
					 EXYNOS3_CLKDIV_TOP_ACLK_400_SHIFT));

			data->top_div_table[i] = clk_div_top;

			/* CLK_DIV_ACP0 */
			clk_div_acp0 &= ~(EXYNOS3_CLKDIV_ACP0_MASK |
					  EXYNOS3_CLKDIV_ACP0_PCLK_MASK |
					  EXYNOS3_CLKDIV_ACP0_DMC_MASK |
					  EXYNOS3_CLKDIV_ACP0_CORED_MASK |
					  EXYNOS3_CLKDIV_ACP0_COREP_MASK);

			clk_div_acp0 |= ((exynos3250_int_clkdiv_acp0[i][0] <<
					  EXYNOS3_CLKDIV_ACP0_SHIFT) |
					 (exynos3250_int_clkdiv_acp0[i][1] <<
					  EXYNOS3_CLKDIV_ACP0_PCLK_SHIFT) |
					 (exynos3250_int_clkdiv_acp0[i][2] <<
					  EXYNOS3_CLKDIV_ACP0_DMC_SHIFT) |
					 (exynos3250_int_clkdiv_acp0[i][3] <<
					  EXYNOS3_CLKDIV_ACP0_CORED_SHIFT) |
					 (exynos3250_int_clkdiv_acp0[i][4] <<
					  EXYNOS3_CLKDIV_ACP0_COREP_SHIFT));

			data->acp0_div_table[i] = clk_div_acp0;

			/* CLK_DIV_MFC */
			clk_div_mfc &= ~(EXYNOS3_CLKDIV_MFC_MASK);

			clk_div_mfc |= ((exynos3250_int_clkdiv_sclk_mfc[i][0] <<
					EXYNOS3_CLKDIV_MFC_SHIFT));

			data->mfc_div_table[i] = clk_div_mfc;

			/*
			 * FIXME: exynos3_bus.c has voltage setting issue
			 * for INT block. If INT block's voltage is changed
			 * before completed platform boot, mmc happen timing
			 * issue or stop platform boot. After fix it,
			 * exynos3_bus.c will apply ASV mode for INT block.
			 */
			/* Support ASV mode for INT voltage table */
			data->opp_table[i].volt
				= int_asv_volt_exynos3250[i][asv];
		}
		break;
	default:
		dev_err(data->dev, "Unknown device type\n");
		return -EINVAL;
	}

	/* Add OPP entry including the voltage/clock of busfreq level */
	for (i = 0; i < data->opp_table_end; i++) {
		ret = opp_add(data->dev,
			      data->opp_table[i].clk,
			      data->opp_table[i].volt);
		if (ret < 0) {
			dev_err(data->dev, "Failed to add opp entry(%ld,%ld)\n",
				data->opp_table[i].clk,
				data->opp_table[i].volt);
			return ret;
		}
	}

	return 0;
}

static int exynos3250_busfreq_parse_dt(struct busfreq_data *data)
{
	struct device *dev = data->dev;
	struct device_node *np = dev->of_node;
	char regulator_name[DEVFREQ_NAME_LEN];
	const char **clk_name;
	int i, ret;

	switch (data->type) {
	case TYPE_BUSF_EXYNOS3250_MIF:
		data->profile		= &exynos3_busfreq_mif_profile;
		data->ppmu_end		= PPMU_MIF_END;
		data->opp_table		= exynos3250_mifclk_table;
		data->opp_table_end	= ARRAY_SIZE(exynos3250_mifclk_table);

		clk_name		= exynos_mif_ppmu_clk_name;
		strcpy(regulator_name,	EX3250_MIF_REGULATOR_NAME);
		break;
	case TYPE_BUSF_EXYNOS3250_INT:
		data->profile		= &exynos3_busfreq_int_profile;
		data->ppmu_end		= PPMU_INT_END;
		data->opp_table		= exynos3250_intclk_table;
		data->opp_table_end	= ARRAY_SIZE(exynos3250_intclk_table);

		clk_name		= exynos_int_ppmu_clk_name;
		strcpy(regulator_name,	EX3250_INT_REGULATOR_NAME);

		break;
	default:
		dev_err(dev, "Unknown device id %d\n", data->type);
		return -EINVAL;
	};

	/* Allocate memory for ppmu register/clock according to ppmu count */
	data->ppmu = devm_kzalloc(dev,
				  sizeof(struct exynos_ppmu) * data->ppmu_end,
				  GFP_KERNEL);
	if (!data->ppmu) {
		dev_err(dev, "Failed to allocate memory for exynos_ppmu\n");
		return -ENOMEM;
	}

	data->clk_ppmu = devm_kzalloc(dev,
				      sizeof(struct clk *) * data->ppmu_end,
				      GFP_KERNEL);
	if (!data->clk_ppmu) {
		dev_err(dev, "Failed to allocate memory for ppmu clock\n");
		return -ENOMEM;
	}

	/* Maps the memory mapped IO to control PPMU register */
	for (i = 0; i < data->ppmu_end; i++) {
		data->ppmu[i].hw_base = of_iomap(np, i);
		if (IS_ERR_OR_NULL(data->ppmu[i].hw_base)) {
			dev_err(dev, "Failed to map memory region\n");
			data->ppmu[i].hw_base = NULL;
			ret = -EINVAL;
			goto err_iomap;
		}
	}

	/*
	 * Get PPMU's clocks to control them. But, if PPMU's clocks
	 * is default 'pass' state, this driver don't need control
	 * PPMU's clock.
	 */
	for (i = 0; i < data->ppmu_end; i++) {
		data->clk_ppmu[i] = devm_clk_get(dev, clk_name[i]);
		if (IS_ERR_OR_NULL(data->clk_ppmu[i])) {
			dev_warn(dev, "Cannot get %s clock\n", clk_name[i]);
			data->clk_ppmu[i] = NULL;
		}

		ret = clk_prepare_enable(data->clk_ppmu[i]);
		if (ret < 0) {
			dev_warn(dev, "Cannot enable %s clock\n", clk_name[i]);
			data->clk_ppmu[i] = NULL;
			goto err_clocks;
		}
	}

	/* Get regulators to control voltage of int/mif block */
	data->regulator_vdd = devm_regulator_get(dev, regulator_name);
	if (IS_ERR(data->regulator_vdd)) {
		dev_err(dev, "Failed to get the regulator \"%s\"\n",
			regulator_name);
		ret = -EINVAL;
		goto err_clocks;
	}

	ret = regulator_enable(data->regulator_vdd);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulator\n");
		goto err_clocks;
	}

	return 0;

err_clocks:
	for (i = 0; i < data->ppmu_end; i++) {
		if (data->clk_ppmu[i])
			clk_disable_unprepare(data->clk_ppmu[i]);
	}
err_iomap:
	for (i = 0; i < data->ppmu_end; i++) {
		if (data->ppmu[i].hw_base)
			iounmap(data->ppmu[i].hw_base);
	}

	return ret;
}

static struct of_device_id exynos3_busfreq_id_match[] = {
	{
		.compatible = "samsung,exynos3250-busfreq-mif",
		.data = (void *)TYPE_BUSF_EXYNOS3250_MIF,
	}, {
		.compatible = "samsung,exynos3250-busfreq-int",
		.data = (void *)TYPE_BUSF_EXYNOS3250_INT,
	},
	{},
};

static int exynos3_busfreq_get_driver_data(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;

	match = of_match_node(exynos3_busfreq_id_match, dev->of_node);
	if (!match)
		return -ENODEV;
	return (int)match->data;
}

static int exynos3_busfreq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct busfreq_data *data;
	struct opp *opp;
	int ret = 0, i;

	if (!dev->of_node)
		return -EINVAL;

	data = devm_kzalloc(dev, sizeof(struct busfreq_data), GFP_KERNEL);
	if (!data) {
		dev_err(dev, "Failed to allocate memory for busfreq_data\n");
		return -ENOMEM;
	}
	data->type = exynos3_busfreq_get_driver_data(pdev);
	data->dev = dev;
	mutex_init(&data->lock);
	platform_set_drvdata(pdev, data);

	switch (data->type) {
	case TYPE_BUSF_EXYNOS3250_MIF:
	case TYPE_BUSF_EXYNOS3250_INT:
		/* Parse dt data to get register/clock/regulator */
		ret = exynos3250_busfreq_parse_dt(data);
		if (ret < 0) {
			dev_err(dev, "Failed to parse dt for resource %d\n",
				data->type);
			return ret;
		}

		/* Initialize Memory Bus Voltage/Frequency table */
		ret = exynos3250_busfreq_init_table(data);
		if (ret < 0) {
			dev_err(dev, "Failed to initialze volt/freq table %d\n",
				data->type);
			return ret;
		}
		break;
	default:
		dev_err(dev, "Unknown device id %d\n", data->type);
		return -EINVAL;
	}

	/* Find the proper opp instance according to initial bus frequency */
	rcu_read_lock();
	opp = opp_find_freq_floor(dev, &data->profile->initial_freq);
	if (IS_ERR_OR_NULL(opp)) {
		rcu_read_unlock();
		dev_err(dev, "Failed to find initial frequency %lu kHz, %d\n",
			      data->profile->initial_freq, data->type);
		ret = PTR_ERR(opp);
		goto err_opp;
	}
	data->curr_opp_info.rate = opp_get_freq(opp);
	data->curr_opp_info.volt = opp_get_voltage(opp);
	rcu_read_unlock();

	/* Reigster Exynos3250's devfreq instance with 'simple_ondemand' gov */
	data->devfreq = devfreq_add_device(dev, data->profile,
					   "simple_ondemand", NULL);
	if (IS_ERR_OR_NULL(data->devfreq)) {
		dev_err(dev, "Failed to add devfreq device\n");
		ret = PTR_ERR(data->devfreq);
		goto err_opp;
	}
	data->devfreq->data = (void *)&exynos3_busfreq_ondemand_data;

	/*
	 * Start PPMU (Performance Profiling Monitoring Unit) to check
	 * utilization of each IP in the Exynos3 SoC.
	 */
	busfreq_mon_reset(data);

	/* Register opp_notifier for Exynos3 busfreq */
	ret = devfreq_register_opp_notifier(dev, data->devfreq);
	if (ret < 0) {
		dev_err(dev, "Failed to register opp notifier\n");
		goto err_notifier_opp;
	}

	/* Register pm_notifier for Exynos3 busfreq */
	data->pm_notifier.notifier_call = exynos3_busfreq_pm_notifier_event;
	ret = register_pm_notifier(&data->pm_notifier);
	if (ret < 0) {
		dev_err(dev, "Failed to register pm notifier\n");
		goto err_notifier_pm;
	}

	return 0;

err_notifier_pm:
	devfreq_unregister_opp_notifier(dev, data->devfreq);
err_notifier_opp:
	devfreq_remove_device(data->devfreq);
	return ret;
err_opp:
	regulator_disable(data->regulator_vdd);

	for (i = 0; i < data->ppmu_end; i++) {
		if (data->clk_ppmu[i])
			clk_disable_unprepare(data->clk_ppmu[i]);
	}

	for (i = 0; i < data->ppmu_end; i++) {
		if (data->ppmu[i].hw_base)
			iounmap(data->ppmu[i].hw_base);
	}

	return ret;
}

static int exynos3_busfreq_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct busfreq_data *data = platform_get_drvdata(pdev);

	/*
	 * devfreq_dev_profile.exit() have to free the resource of this
	 * device driver.
	 */

	/* Unregister all of notifier chain */
	unregister_pm_notifier(&data->pm_notifier);
	devfreq_unregister_opp_notifier(dev, data->devfreq);

	devfreq_remove_device(data->devfreq);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos3_busfreq_resume(struct device *dev)
{
	struct busfreq_data *data = dev_get_drvdata(dev);
	int i;

	/* Enable clock after wake-up from suspend state */
	for (i = 0; i < data->ppmu_end; i++)
		clk_prepare_enable(data->clk_ppmu[i]);

	/* Reset PPMU to check utilization again */
	busfreq_mon_reset(data);

	return 0;
}

static int exynos3_busfreq_suspend(struct device *dev)
{
	struct busfreq_data *data = dev_get_drvdata(dev);
	int i;

	/*
	 * Disable clock before entering suspend state
	 * to reduce leakage power on suspend state.
	 */
	for (i = 0; i < data->ppmu_end; i++)
		clk_disable_unprepare(data->clk_ppmu[i]);

	return 0;
}
#endif

static const struct dev_pm_ops exynos3_busfreq_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos3_busfreq_suspend, exynos3_busfreq_resume)
};

static const struct platform_device_id exynos3_busfreq_id[] = {
	{ "exynos3250-busf-mif", TYPE_BUSF_EXYNOS3250_MIF },
	{ "exynos3250-busf-int", TYPE_BUSF_EXYNOS3250_INT },
	{},
};

static struct platform_driver exynos3_busfreq_driver = {
	.probe		= exynos3_busfreq_probe,
	.remove		= exynos3_busfreq_remove,
	.id_table	= exynos3_busfreq_id,
	.driver		= {
		.name		= "exynos3250-busfreq",
		.owner		= THIS_MODULE,
		.pm		= &exynos3_busfreq_pm,
		.of_match_table	= exynos3_busfreq_id_match,
	},
};

static int __init exynos3_busfreq_init(void)
{
	return platform_driver_register(&exynos3_busfreq_driver);
}
late_initcall(exynos3_busfreq_init);

static void __exit exynos3_busfreq_exit(void)
{
	platform_driver_unregister(&exynos3_busfreq_driver);
}
module_exit(exynos3_busfreq_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EXYNOS3250 INT/MIF busfreq driver with devfreq framework");
MODULE_AUTHOR("Chanwoo Choi <cw00.choi@samsung.com>");
