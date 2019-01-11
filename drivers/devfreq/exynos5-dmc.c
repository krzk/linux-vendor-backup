/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019, Samsung
 */

#include <asm/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#define DRIVER_DESC "Driver for Exynos5 Dynamic Memory Controller dynamic \
			frequency and voltage change"

#define EXYNOS5422_REV_0 (0x1)
#define EXYNOS5422_PROD_REV_MAIN_MASK (0xf0)
#define EXYNOS5422_PROD_REV_SUB_MASK (0xf)

#define EXYNOS5_DREXI_TIMINGAREF	(0x0030)
#define EXYNOS5_DREXI_TIMINGROW0	(0x0034)
#define EXYNOS5_DREXI_TIMINGDATA0	(0x0038)
#define EXYNOS5_DREXI_TIMINGPOWER0	(0x003C)
#define EXYNOS5_DREXI_TIMINGROW1	(0x00E4)
#define EXYNOS5_DREXI_TIMINGDATA1	(0x00E8)
#define EXYNOS5_DREXI_TIMINGPOWER1	(0x00EC)

#define EXYNOS5_DREXI_MEMCTRL		(0x0004)
#define EXYNOS5_DREXI_DIRECTCMD		(0x0010)
#define EXYNOS5_DREXI_TIMINGAREF	(0x0030)
#define EXYNOS5_DREXI_TIMINGSETSW	(0x00E0)
#define EXYNOS5_DREXI_MRSTATUS		(0x0054)
#define EXYNOS5_DREXI_QOSCONTROL8	(0x00A0)
#define EXYNOS5_DREXI_BRBRSVCONTROL	(0x0100)
#define EXYNOS5_DREXI_BP_CONTROL0	(0x0210)
#define EXYNOS5_DREXI_BP_CONTROL1	(0x0220)
#define EXYNOS5_DREXI_BP_CONTROL2	(0x0230)
#define EXYNOS5_DREXI_BP_CONTROL3	(0x0240)

#define EXYNOS5_LPDDR3PHY_CON3                  (0x0A20)
#define EXYNOS5_TIMING_SET_SWI                  (1UL << 28)

#define AREF_NORMAL			(0x2e)

#define EXYNOS5_TIMING_USE_SET               (1UL << 4)
#define EXYNOS5_TIMING_SET_SW_CON               (1UL)

#define EXYNOS5_CLK_MUX_STAT_CDREX		(0x400)
#define EXYNOS5_MCLK_CDREX_SEL_BPLL		(1UL)
#define EXYNOS5_MCLK_CDREX_SEL_MX_MSPLL		(2UL)
#define EXYNOS5_CLKSRC_CDREX_SEL_SHIFT		(4)
#define EXYNOS5_MCLK_CDREX_MASK			(0x7)

#define EXYNOS5_CLK_SRC_CDREX		(0x200)
#define DMC_PAUSE_CTRL			(0x91C)
#define DMC_PAUSE_ENABLE			(1UL)
#define SELF_REFRESH_MASK		(0x20UL)
#define SR_CMD_EXIT_CHIP0		(0x08000000)
#define SR_CMD_EXIT_CHIP1		(0x08100000)
#define CMD_SR_ENTER		(0x04000000)
#define CMD_SR_EXIT		(0x08000000)
#define CMD_CHIP0		(0x00000000)
#define CMD_CHIP1		(0x00100000)
#define USE_MX_MSPLL_TIMINGS	(1)
#define USE_BPLL_TIMINGS	(0)

#define DMC_REG_VOLT_STEP	0

#define IS_MEM_2GB(val) \
	(						\
	 (((val) & 0xf0) & 0x20) ? 1 :			\
		(((val) & 0xf0) & 0x30) ? 1 : 0		\
	)

#define EXYNOS5_POP_OPTIONS(val) \
		(((val >> 4) & 0x3UL) << 4)
#define EXYNOS5_DDR_TYPE(val) \
		(((val >> 14) & 0x1UL))

#define CHIP_PROD_ID	(0)
#define CHIP_PKG_ID	(4)

#define PMCNT_CONST_RATIO_MUL 15
#define PMCNT_CONST_RATIO_DIV 10

/**
 * enum dmc_slot_id - An enum with slots in DMC
 */
enum dmc_slot_id {
	DMC0_0,
	DMC0_1,
	DMC1_0,
	DMC1_1,
	DMC_SLOTS_END
};

/**
 * struct dmc_slot_info - Describes DMC's slot
 *
 * The structure holds DMC's slot name which is part of the device name
 * provided in DT. Each slot has particular share of the DMC bandwidth.
 * To abstract the model performance and values in performance counters,
 * fields 'ratio_mul' and 'ratio_div' are used in calculation algorithm
 * for each slot. Please check the corresponding function with the algorithm,
 * to see how these variables are used.
 */
struct dmc_slot_info {
	char *name;
	int id;
	int ratio_mul;
	int ratio_div;
};

/**
 * struct dmc_opp_table - Operating level desciption
 *
 * Covers frequency and voltage settings of the DMC operating mode.
 */
struct dmc_opp_table {
	unsigned long freq_khz;
	unsigned long volt_uv;
};

/**
 * struct dram_param - Parameters for the external memory chip
 *
 * Covers timings settings for a particular memory chip's operating frequency.
 */
struct dram_param {
	unsigned int timing_row;
	unsigned int timing_data;
	unsigned int timing_power;
};

/**
 * struct exynos5_dmc - main structure describing DMC device
 *
 * The main structure for the Dynamic Memory Controller which covers clocks,
 * memory regions, HW information, parameters and current operating mode.
 */
struct exynos5_dmc {
	struct device *dev;
	struct devfreq *df;
	struct devfreq_simple_ondemand_data gov_data;
	void __iomem *base_drexi0;
	void __iomem *base_drexi1;
	void __iomem *base_clk;
	void __iomem *chip_id;
	struct mutex lock;
	unsigned long curr_rate;
	unsigned long curr_volt;
	const struct dmc_opp_table *opp;
	const struct dmc_opp_table *opp_bypass;
	int opp_count;
	const struct dram_param *dram_param;
	const struct dram_param *dram_bypass_param;
	int dram_param_count;
	unsigned int prod_rev;
	unsigned int pkg_rev;
	unsigned int mem_info;
	struct regulator *vdd_mif;
	struct clk *fout_spll;
	struct clk *fout_bpll;
	struct clk *mout_spll;
	struct clk *mout_bpll;
	struct clk *mout_mclk_cdrex;
	struct clk *dout_clk2x_phy0;
	struct clk *mout_mx_mspll_ccore;
	struct clk *mx_mspll_ccore_phy;
	struct clk *mout_mx_mspll_ccore_phy;
	struct devfreq_event_dev **counter;
	int num_counters;
	bool counters_enabled;
};

/**
 * exynos5_counters_fname() - Macro generating function for event devices
 * @f:		function name suffix
 *
 * Macro which generates needed function for manipulation of event devices.
 * It aims to avoid code duplication relaying on similar prefix and function
 * parameters in the devfreq event device framework functions.
 */
#define exynos5_counters_fname(f)				\
static int exynos5_counters_##f(struct exynos5_dmc *dmc)	\
{								\
	int i, ret;						\
								\
	for (i = 0; i < dmc->num_counters; i++) {		\
		if (!dmc->counter[i])				\
			continue;				\
		ret = devfreq_event_##f(dmc->counter[i]);	\
		if (ret < 0)					\
			return ret;				\
	}							\
	return 0;						\
}
exynos5_counters_fname(set_event);
exynos5_counters_fname(enable_edev);
exynos5_counters_fname(disable_edev);

/**
 * dmc_opp_exynos5422 - Array with frequency and voltage values
 *
 * Operating points for  Exynos5422 SoC revisions.
 * The order and sizeof the array has a meaning and is tightly connected with
 * DRAM parameters in arrays bellow.
 */
static const struct dmc_opp_table dmc_opp_exynos5422[] = {
	{825000, 1050000},
	{728000, 1037500},
	{633000, 1012500},
	{543000, 937500},
	{413000, 887500},
	{275000, 875000},
	{206000, 875000},
	{165000, 875000},
};

/**
 * dmc_opp_bypass_exynos5422 - frequency and voltage level for temporary mode
 */
static const struct dmc_opp_table dmc_opp_bypass_exynos5422 = {400000, 887500};

/**
 * dram_param_exynos5422 - DRAM timings for particular HW setup
 *
 * Operating parameters for DRAM memory running with different clock frequency.
 * The order is the same as in 'dmc_opp_table' above, the highest frequency
 * is first.
 * These settings are needed for proper operation of the DRAM memory with
 * corresponding frequency. They are calculated for Exynos5422 revision 0
 * with 2GB LPDDR3 memory chip.
 */
static const struct dram_param dram_param_exynos5422[] = {
	{0x365A9713, 0x4740085E, 0x543A0446},
	{0x30598651, 0x3730085E, 0x4C330336},
	{0x2A48758F, 0x3730085E, 0x402D0335},
	{0x244764CD, 0x3730085E, 0x38270335},
	{0x1B35538A, 0x2720085E, 0x2C1D0225},
	{0x12244287, 0x2720085E, 0x1C140225},
	{0x112331C6, 0x2720085E, 0x180F0225},
	{0x11223185, 0x2720085E, 0x140C0225},
};


/**
 * Operating parameters for DRAM memory running on temporary clock 400MHz during
 * switching frequency on the main clock. This variable provides timings for
 * Exynos5422 SoC revision 0 and DRAM 2GB chip.
 */
static const struct dram_param dram_bypass_param_exynos5422 = {
	0x365a9713, 0x4740085e, 0x543a0446
};

/**
 * dmc_slot - An array which holds DMC's slots information
 *
 * The array is used in algorithm calculating slots performance and usage
 * based on performance counters' values. The values i.e. 15/10=1.5 correspond
 * to slot share in the DMC channel, which has 2.0 abstract width.
 */
static const struct dmc_slot_info dmc_slot[] = {
	{"dmc0_0", DMC0_0, 15, 10},
	{"dmc0_1", DMC0_1, 5, 10},
	{"dmc1_0", DMC1_0, 10, 10},
	{"dmc1_1", DMC1_0, 10, 10},
};

/**
 * revision_show() - Shows revision information of the DMC device
 * @dev:	device for which the information is going to be shown
 * @attr:	file attributes from the sysfs
 * @buf:	destination buffer provided by sysfs
 *
 * Simple function providing information about DMC HW revision
 */
static ssize_t revision_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	ssize_t res = 0;
	struct exynos5_dmc *dmc = dev_get_drvdata(dev->parent);
	const char rev[] = "DMC on SoC rev. prod id 0x%08x, pkg id 0x%08x\n";
	int rev_len = sizeof(rev) + 10;

	res += snprintf(&buf[res], rev_len, rev, dmc->prod_rev, dmc->pkg_rev);
	return res;
}

static DEVICE_ATTR_RO(revision);

static struct attribute *env_attributes[] = {
	&dev_attr_revision.attr,
	NULL
};

static struct attribute_group env_group = {
	.name = "dmc_info",
	.attrs = env_attributes,
};

/**
 * find_target_freq_id() - Finds requested frequency in local DMC configuration
 * @dmc:	device for which the information is checked
 * @target_rate:	requested frequency in KHz
 *
 * Seeks in the local DMC driver structure for the requested frequency value
 * and returns index or error value.
 */
static int find_target_freq_idx(struct exynos5_dmc *dmc,
				unsigned long target_rate)
{
	int i;

	for (i = 0; i < dmc->opp_count; i++)
		if (dmc->opp[i].freq_khz <= target_rate)
			return i;

	return -EINVAL;
}

/**
 * exynos5_get_chip_info() - Gets chip ID information
 * @dmc:	device for which the information is checked
 *
 * Function wrapper for getting the chip ID information.
 */
static void exynos5_read_chip_info(struct exynos5_dmc *dmc)
{
	unsigned int val;

	val = readl(dmc->chip_id + CHIP_PROD_ID);
	dmc->prod_rev = val;

	val = readl(dmc->chip_id + CHIP_PKG_ID);
	dmc->pkg_rev = val;

	dmc->mem_info = EXYNOS5_POP_OPTIONS(val);
	dmc->mem_info |= EXYNOS5_DDR_TYPE(val);
}

/**
 * exynos5_get_chip_info() - Gets chip ID information
 * @dmc:	device for which the information is checked
 *
 * Function wrapper for getting the chip ID information.
 */
static int exynos5_get_chip_info(struct exynos5_dmc *dmc)
{
	exynos5_read_chip_info(dmc);

	dev_info(dmc->dev, "memory type %#x\n", dmc->mem_info);

	return 0;
}

/**
 * exynos5_dmc_pause_on_switching() - Controls a pause feature in DMC
 * @dmc:	device which is used for changing this feature
 * @set:	a boolean state passing enable/disable request
 *
 * There is a need of pausing DREX DMC when divider or MUX in clock tree
 * changes its configuration. In such situation access to the memory is blocked
 * in DMC automatically. This feature is used when clock frequency change
 * request appears and touches clock tree.
 */
static int exynos5_dmc_pause_on_switching(struct exynos5_dmc *dmc, bool set)
{
	unsigned int val;

	val = readl(dmc->base_clk + DMC_PAUSE_CTRL);
	if (set)
		val |= DMC_PAUSE_ENABLE;
	else
		val &= ~DMC_PAUSE_ENABLE;
	writel(val, dmc->base_clk + DMC_PAUSE_CTRL);

	return 0;
}

/**
 * exynos5_dmc_chip_revision_settings() - Chooses proper DMC's configuration
 * @dmc:	device for which is going to be checked and configured
 *
 * Function checks the HW product information in order to choose proper
 * configuration for DMC frequency, voltage and DRAM timings.
 */
static int exynos5_dmc_chip_revision_settings(struct exynos5_dmc *dmc)
{
	int res;

	res = exynos5_get_chip_info(dmc);
	if (res)
		return res;

	if (!IS_MEM_2GB(dmc->mem_info)) {
		dev_warn(dmc->dev, "DRAM memory type not supported\n");
		return -EINVAL;
	}

	dmc->dram_param = dram_param_exynos5422;

	dmc->dram_param_count = ARRAY_SIZE(dram_param_exynos5422);

	dmc->dram_bypass_param = &dram_bypass_param_exynos5422;

	dmc->opp = dmc_opp_exynos5422;
	dmc->opp_count = ARRAY_SIZE(dmc_opp_exynos5422);

	dmc->opp_bypass = &dmc_opp_bypass_exynos5422;

	return 0;
}

/**
 * exynos5_init_freq_table() - Initialized PM OPP framework
 * @dev:	devfreq device for which the OPP table is going to be
 *		initialized
 * @dmc:	DMC device for which the frequencies are used for OPP init
 * @profile:	devfreq device's profile
 *
 * Populate the devfreq device's OPP table based on current frequency, voltage.
 */
static int exynos5_init_freq_table(struct device *dev, struct exynos5_dmc *dmc,
				   struct devfreq_dev_profile *profile)
{
	int i, ret;

	for (i = 0; i < dmc->opp_count; i++) {
		ret = dev_pm_opp_add(dev, dmc->opp[i].freq_khz,
				     dmc->opp[i].volt_uv);
		if (ret) {
			dev_warn(dev, "failed to add opp %uHz %umV\n", 1, 1);
			while (i-- > 0)
				dev_pm_opp_remove(dev, dmc->opp[i].freq_khz);
			return ret;
		}
	}

	return 0;
}

/**
 * exynos5_set_bypass_dram_timings() - Low-level changes of the DRAM timings
 * @dmc:	device for which the new settings is going to be applied
 * @param:	DRAM parameters which passes timing data
 *
 * Low-level function for changing timings for DRAM memory clocking from
 * 'bypass' clock source (fixed frequency @400MHz).
 * It uses timing bank registers set 1.
 */
static void exynos5_set_bypass_dram_timings(struct exynos5_dmc *dmc,
					    const struct dram_param *param)
{

	writel(AREF_NORMAL, dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGAREF);

	writel(param->timing_row,
	       dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGROW1);
	writel(param->timing_row,
	       dmc->base_drexi1 + EXYNOS5_DREXI_TIMINGROW1);
	writel(param->timing_data,
	       dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGDATA1);
	writel(param->timing_data,
	       dmc->base_drexi1 + EXYNOS5_DREXI_TIMINGDATA1);
	writel(param->timing_power,
	       dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGPOWER1);
	writel(param->timing_power,
	       dmc->base_drexi1 + EXYNOS5_DREXI_TIMINGPOWER1);
}


/**
 * exynos5_dram_change_timings() - Low-level changes of the DRAM final timings
 * @dmc:	device for which the new settings is going to be applied
 * @target_rate:	target frequency of the DMC
 *
 * Low-level function for changing timings for DRAM memory operating from main
 * clock source (BPLL), which can have different frequencies. Thus, each
 * frequency must have corresponding timings register values in order to keep
 * the needed delays.
 * It uses timing bank registers set 0.
 */
static int exynos5_dram_change_timings(struct exynos5_dmc *dmc,
				       unsigned long target_rate)
{
	int idx;


	for (idx = 0; idx < dmc->dram_param_count; idx++)
		if (dmc->opp[idx].freq_khz <= target_rate)
			break;

	if (idx >= dmc->dram_param_count)
		return -EINVAL;

	writel(AREF_NORMAL, dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGAREF);

	writel(dmc->dram_param[idx].timing_row,
	       dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGROW0);
	writel(dmc->dram_param[idx].timing_row,
	       dmc->base_drexi1 + EXYNOS5_DREXI_TIMINGROW0);
	writel(dmc->dram_param[idx].timing_data,
	       dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGDATA0);
	writel(dmc->dram_param[idx].timing_data,
	       dmc->base_drexi1 + EXYNOS5_DREXI_TIMINGDATA0);
	writel(dmc->dram_param[idx].timing_power,
	       dmc->base_drexi0 + EXYNOS5_DREXI_TIMINGPOWER0);
	writel(dmc->dram_param[idx].timing_power,
	       dmc->base_drexi1 + EXYNOS5_DREXI_TIMINGPOWER0);

	return 0;
}

/**
 * exynos5_switch_timing_regs() - Changes bank register set for DRAM timings
 * @dmc:	device for which the new settings is going to be applied
 * @set:	boolean variable passing set value
 *
 * Changes the register set, which holds timing parameters.
 * There is two register sets: 0 and 1. The register set 0
 * is used in normal operation when the clock is provided from main PLL.
 * The bank register set 1 is used when the main PLL frequency is going to be
 * changed and the clock is taken from alternative, stable source.
 * This function switches between these banks according to the
 * currently used clock source.
 */
static void exynos5_switch_timing_regs(struct exynos5_dmc *dmc, bool set)
{
	unsigned int reg;

	reg = readl(dmc->base_clk + EXYNOS5_LPDDR3PHY_CON3);

	if (set)
		reg |= EXYNOS5_TIMING_SET_SWI;
	else
		reg &= ~EXYNOS5_TIMING_SET_SWI;

	writel(reg, dmc->base_clk + EXYNOS5_LPDDR3PHY_CON3);
}

/*
 * Change clock parent for MUX_CORE_SEL and the main clock for DMC.
 * The mux takes two clock sources: main BPLL and mx_mspll ('bypass').
 */
static int exynos5_dmc_change_clock_parent(struct exynos5_dmc *dmc,
					   struct clk *parent,
					   unsigned int parent_selection_id)
{
	unsigned int reg = 0;

	reg = readl(dmc->base_clk + EXYNOS5_CLK_SRC_CDREX);
	if (clk_set_parent(dmc->mout_mclk_cdrex, parent)) {
		dev_err(dmc->dev, "Couldn't change parent of mclk_cdrex\n");
		return -EINVAL;
	}

	for ( ; reg != parent_selection_id; ) {
		cpu_relax();
		reg = readl(dmc->base_clk + EXYNOS5_CLK_MUX_STAT_CDREX);
		reg >>= EXYNOS5_CLKSRC_CDREX_SEL_SHIFT;
		reg &= EXYNOS5_MCLK_CDREX_MASK;
	}

	return 0;
}


/**
 * exynos5_dmc_change_voltage() - Changes the voltage regulator value
 * @dmc:	device for which it is going to be set
 * @target_volt:	new voltage which is chosen to be final
 *
 * Main function for changing voltage on the VDD_MIF regulator.
 */
static int exynos5_dmc_change_voltage(struct exynos5_dmc *dmc,
				      unsigned long target_volt)
{
	int ret;

	ret = regulator_set_voltage(dmc->vdd_mif, target_volt,
				    target_volt + DMC_REG_VOLT_STEP);

	if (ret)
		return ret;

	dmc->curr_volt = target_volt;

	return 0;
}

/**
 * exynos5_dmc_align_target_voltage() - Sets the final voltage for the DMC
 * @dmc:	device for which it is going to be set
 * @target_volt:	new voltage which is chosen to be final
 *
 * Function tries to align voltage to the safe level for 'normal' mode.
 * It checks the need of higher voltage and changes the value. The target
 * voltage might be lower that currently set and still the system will be
 * stable.
 */
static int exynos5_dmc_align_target_voltage(struct exynos5_dmc *dmc,
					    unsigned long target_volt)
{
	int ret = 0;

	if (dmc->curr_volt > target_volt)
		ret = exynos5_dmc_change_voltage(dmc, target_volt);

	return ret;
}

/**
 * exynos5_dmc_align_bypass_voltage() - Sets the voltage for the DMC
 * @dmc:	device for which it is going to be set
 * @target_volt:	new voltage which is chosen to be final
 *
 * Function tries to align voltage to the safe level for the 'bypass' mode.
 * It checks the need of higher voltage and changes the value.
 * The target voltage must not be less than currently needed, because
 * for current frequency the device might become unstable.
 */
static int exynos5_dmc_align_bypass_voltage(struct exynos5_dmc *dmc,
					    unsigned long target_volt)
{
	int ret = 0;
	unsigned long bypass_volt = dmc->opp_bypass->volt_uv;

	target_volt = max(bypass_volt, target_volt);

	if (dmc->curr_volt >= target_volt)
		return 0;

	ret = exynos5_dmc_change_voltage(dmc, target_volt);

	return ret;
}

/**
 * exynos5_dmc_align_bypass_dram_timings() - Chooses and sets DRAM timings
 * @dmc:	device for which it is going to be set
 * @target_rate:	new frequency which is chosen to be final
 *
 * Function changes the DRAM timings for the temporary 'bypass' mode.
 */
static int exynos5_dmc_align_bypass_dram_timings(struct exynos5_dmc *dmc,
						 unsigned long target_rate)
{
	int idx = find_target_freq_idx(dmc, target_rate);

	if (idx < 0)
		return -EINVAL;

	exynos5_set_bypass_dram_timings(dmc, dmc->dram_bypass_param);

	return 0;
}

/**
 * exynos5_dmc_switch_to_bypass_configuration() - Switching to temporary clock
 * @dmc:	DMC device for which the switching is going to happen
 * @target_rate:	new frequency which is going to be set as a final
 * @target_volt:	new voltage which is going to be set as a final
 *
 * Function configures DMC and clocks for operating in temporary 'bypass' mode.
 * This mode is used only temporary but if required, changes voltage and timings
 * for DRAM chips. It switches the main clock to stable clock source for the
 * period of the main PLL reconfiguration.
 */
static int exynos5_dmc_switch_to_bypass_configuration(struct exynos5_dmc *dmc,
				   unsigned long target_rate,
				   unsigned long target_volt)
{
	int ret;

	/*
	 * Having higher voltage for a particular frequency does not harm
	 * the chip. Use it for the temporary frequency change when one
	 * voltage manipulation might be avoided.
	 */
	ret = exynos5_dmc_align_bypass_voltage(dmc, target_volt);
	if (ret)
		return ret;

	/*
	 * Longer delays for DRAM does not cause crash, the opposite does.
	 */
	ret = exynos5_dmc_align_bypass_dram_timings(dmc, target_rate);
	if (ret)
		return ret;

	/*
	 * Delays are long enough, so use them for the new coming clock.
	 */
	exynos5_switch_timing_regs(dmc, USE_MX_MSPLL_TIMINGS);

	/*
	 * Voltage is set at least to a level needed for this frequency,
	 * so switching clock source is safe now.
	 */
	clk_prepare_enable(dmc->fout_spll);
	clk_prepare_enable(dmc->mout_spll);
	clk_prepare_enable(dmc->mout_mx_mspll_ccore);
	ret = exynos5_dmc_change_clock_parent(dmc, dmc->mout_mx_mspll_ccore,
					      EXYNOS5_MCLK_CDREX_SEL_MX_MSPLL);
	return ret;
}

/**
 * exynos5_dmc_change_freq_and_volt() - Changes voltage and frequency of the DMC
 * using safe procedure
 * @dmc:	device for which the frequency is going to be changed
 * @target_rate:	requested new frequency
 * @target_volt:	requested voltage which corresponds to the new frequency
 *
 * The DMC frequency change procedure requires a few steps.
 * The main requirement is to change the clock source in the clk mux
 * for the time of main clock PLL locking. The assumption is that the
 * alternative clock source set as parent is stable.
 * The second parent's clock frequency is fixed to 400MHz, it is named 'bypass'
 * clock. This requires alignment in DRAM timing parameters for the new
 * T-period. There is two bank sets for keeping DRAM
 * timings: set 0 and set 1. The set 0 is used when main clock source is
 * chosen. The 2nd set of regs is used for 'bypass' clock. Switching between
 * the two bank sets is part of the process.
 * The voltage must also be aligned to the minimum required level. There is
 * this intermediate step with switching to 'bypass' parent clock source.
 * if the old voltage is lower, it requires an increase of the voltage level.
 * The complexity of the voltage manipulation is hidden in low level function.
 * In this function there is last alignment of the voltage level at the end.
 */
static int
exynos5_dmc_change_freq_and_volt(struct exynos5_dmc *dmc,
				 unsigned long target_rate,
				 unsigned long target_volt)
{
	int ret;

	ret = exynos5_dmc_switch_to_bypass_configuration(dmc, target_rate,
							 target_volt);
	if (ret)
		return ret;

	/* We are safe to increase the timings for current bypass frequency.
	 * Thanks to this the settings we be ready for the upcoming clock source
	 * change. */
	exynos5_dram_change_timings(dmc, target_rate);

	clk_set_rate(dmc->fout_bpll, target_rate * 1000);

	exynos5_switch_timing_regs(dmc, USE_BPLL_TIMINGS);

	ret = exynos5_dmc_change_clock_parent(dmc, dmc->mout_bpll,
					      EXYNOS5_MCLK_CDREX_SEL_BPLL);
	if (ret)
		return ret;

	clk_disable_unprepare(dmc->mout_mx_mspll_ccore);
	clk_disable_unprepare(dmc->mout_spll);
	clk_disable_unprepare(dmc->fout_spll);
	/* Make sure if the voltage is not from 'bypass' settings and align to
	 * the right level for power efficiency */
	ret = exynos5_dmc_align_target_voltage(dmc, target_volt);

	return ret;
}

/**
 * exynos5_dmc_get_volt_freq() - Gets the frequency and voltage from the OPP
 * table.
 * @dev:	device for which the frequency is going to be changed
 * @freq:       requested frequency in KHz
 * @target_rate:	returned frequency which is the same or lower than
 *			requested
 * @target_volt:	returned voltage which corresponds to the returned
 *			frequency
 *
 * Function gets requested frequency and checks OPP framework for needed
 * frequency and voltage. It populates the values 'target_rate' and
 * 'target_volt' or returns error value when OPP framework fails.
 */
static int exynos5_dmc_get_volt_freq(struct device *dev, unsigned long *freq,
				     unsigned long *target_rate,
				     unsigned long *target_volt, u32 flags)
{
	struct dev_pm_opp *opp;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	*target_rate = dev_pm_opp_get_freq(opp);
	*target_volt = dev_pm_opp_get_voltage(opp);
	dev_pm_opp_put(opp);

	return 0;
}

/**
 * exynos5_dmc_target() - Function responsible for changing frequency of DMC
 * @dev:	device for which the frequency is going to be changed
 * @freq:	requested frequency in KHz
 * @flags:	flags provided for this frequency change request
 *
 * An entry function provided to the devfreq framework which provides frequency
 * change of the DMC. The function gets the possible rate from OPP table based
 * on requested frequency. It calls the next function responsible for the
 * frequency and voltage change. In case of failure, does not set 'curr_rate'
 * and returns error value to the framework.
 */
static int exynos5_dmc_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct exynos5_dmc *dmc = dev_get_drvdata(dev);
	unsigned long target_rate = 0;
	unsigned long target_volt = 0;
	int ret;

	ret = exynos5_dmc_get_volt_freq(dev, freq, &target_rate, &target_volt,
					flags);
	if (ret)
		return ret;

	if (target_rate == dmc->curr_rate)
		return 0;

	mutex_lock(&dmc->lock);

	ret = exynos5_dmc_change_freq_and_volt(dmc, target_rate, target_volt);

	if (ret) {
		mutex_unlock(&dmc->lock);
		return ret;
	}

	dmc->curr_rate = target_rate;

	mutex_unlock(&dmc->lock);
	return 0;
}

/**
 * exynos5_cnt_name_match() - Tries to match 'edev' with the right device index
 * @edev:       event device for which the name is going to be matched
 *
 * Function matches the name of the 'edev' counter device with known devices
 * with configured ratios and shares of the DMC channels.
 * When the name is matched, it returns the index for the proper device.
 */
static int exynos5_cnt_name_match(struct devfreq_event_dev *edev)
{
	int i;
	int id = -ENODEV;

	for (i = 0; i < ARRAY_SIZE(dmc_slot); i++) {
		if (strstr(edev->desc->name, dmc_slot[i].name))
			return i;
	}

	return id;
}

/**
 * exynos5_cnt_calculate() - Calculates the values of performance counters.
 * @edev:	event device for which the counter is used for calculation
 * @cnt:	raw counter value
 * @cnt_norm:	counter value normalized to DMC performance ratio for a proper
 *		channel or virtual channel
 *
 * Function calculates normalized value for the raw counter. The raw counter
 * value does not show real channel usage. The DMC splits not equally the
 * bandwidth for the channels. The function checks the type of the 'edev'
 * counter and calculates the normalized value based on the 'shares' of the
 * bandwidth set in the controller.
 */
static int exynos5_cnt_calculate(struct devfreq_event_dev *edev,
				 unsigned long cnt, u64 *cnt_norm)
{
	int idx;

	idx = exynos5_cnt_name_match(edev);
	if (idx < 0)
		return idx;

	*cnt_norm = cnt;

	if (!(dmc_slot[idx].ratio_mul == dmc_slot[idx].ratio_div)) {
		*cnt_norm = *cnt_norm * dmc_slot[idx].ratio_mul;
		*cnt_norm = div_u64(*cnt_norm, dmc_slot[idx].ratio_div);
	}

	*cnt_norm = *cnt_norm * PMCNT_CONST_RATIO_MUL;
	*cnt_norm = div_u64(*cnt_norm, PMCNT_CONST_RATIO_DIV);

	return idx;
}

/**
 * exynos5_counters_get() - Gets the performance counters values.
 * @dmc:	device for which the counters are going to be checked
 * @load_count:	variable which is populated with counter value
 * @total_count:	variable which is used as 'wall clock' reference
 *
 * Function which provides performance counters values. It sums up counters for
 * two DMC channels. The 'total_count' is used as a reference and max value.
 * The ratio 'load_count/total_count' shows the busy percentage [0%, 100%].
 */
static int exynos5_counters_get(struct exynos5_dmc *dmc,
				unsigned long *load_count,
				unsigned long *total_count)
{
	unsigned long load_dmc[2] = {0, 0};
	unsigned long total = 0;
	u64 load = 0;
	struct devfreq_event_data event;
	int ret, i, idx;

	for (i = 0; i < dmc->num_counters; i++) {
		if (!dmc->counter[i])
			continue;

		ret = devfreq_event_get_event(dmc->counter[i], &event);
		if (ret < 0)
			return ret;

		idx = exynos5_cnt_calculate(dmc->counter[i], event.load_count,
					    &load);
		if (idx < 0)
			continue;

		if (idx == DMC0_0 || idx == DMC0_1)
			load_dmc[0] += load;
		else
			load_dmc[1] += load;

		if (total < event.total_count) {
			total = event.total_count;
		}
	}

	*load_count = load_dmc[0] + load_dmc[1];
	*total_count = total;

	return 0;
}

/**
 * exynos5_dmc_get_status() - Read current DMC performance statistics.
 * @dev:	device for which the statistics are requested
 * @stat:	structure which has statistic fields
 *
 * Function reads the DMC performance counters and calculates 'busy_time'
 * and 'total_time'. To protect from overflow, the values are shifted right
 * by 10. After read out the counters are setup to count again.
 */
static int exynos5_dmc_get_status(struct device *dev,
				  struct devfreq_dev_status *stat)
{
	struct exynos5_dmc *dmc = dev_get_drvdata(dev);
	unsigned long load, total;
	int ret;
	bool cnt_en;

	mutex_lock(&dmc->lock);
	cnt_en = dmc->counters_enabled;
	mutex_unlock(&dmc->lock);
	if (!cnt_en) {
		dev_warn(dev, "performance counters needed, but not present\n");
		return -EAGAIN;
	}

	ret = exynos5_counters_get(dmc, &load, &total);
	if (ret < 0)
		return -EINVAL;

	/* To protect from overflow in calculation ratios, divide by 1024 */
	stat->busy_time = load >> 10;
	stat->total_time = total >> 10;

	ret = exynos5_counters_set_event(dmc);
	if (ret < 0) {
		dev_err(dmc->dev, "could not set event counter\n");
		return ret;
	}

	return 0;
}

/**
 * exynos5_dmc_get_cur_freq() - Function returns current DMC frequency
 * @dev:	device for which the framework checks operating frequency
 * @freq:	returned frequency value
 *
 * It returns the currently used frequency of the DMC. The real operating
 * frequency might be lower when the clock source value could not be divided
 * to the requested value.
 */
static int exynos5_dmc_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct exynos5_dmc *dmc = dev_get_drvdata(dev);

	mutex_lock(&dmc->lock);
	*freq = dmc->curr_rate;
	mutex_unlock(&dmc->lock);

	return 0;
}

/**
 * exynos5_dmc_df_profile - Devfreq governor's profile structure
 *
 * It provides to the devfreq framework needed functions and polling period.
 */
static struct devfreq_dev_profile exynos5_dmc_df_profile = {
	.polling_ms = 500,
	.target = exynos5_dmc_target,
	.get_dev_status = exynos5_dmc_get_status,
	.get_cur_freq = exynos5_dmc_get_cur_freq,
};

/**
 * exynos5_dmc_align_initial_frequency() - Align initial frequency value
 * @dmc:	device for which the frequency is going to be set
 * @bootloader_init_freq:	initial frequency set by the bootloaded in KHz
 *
 * The initial bootloader frequency, which is present during boot, might be
 * different that supported frequency values in the driver. It is possible
 * due to different PLL settings or used PLL as a source.
 * This funtion provides the 'initial_freq' for the devfreq framework
 * statistics engine which supports only registered values. Thus, some alignment
 * must be made.
 */
unsigned long
exynos5_dmc_align_init_freq(struct exynos5_dmc *dmc,
			    unsigned long bootloader_init_freq)
{
	unsigned long aligned_freq;
	int idx;

	idx = find_target_freq_idx(dmc, bootloader_init_freq);
	if (idx >= 0)
		aligned_freq = dmc->opp[idx].freq_khz;
	else
		aligned_freq = dmc->opp[dmc->opp_count - 1].freq_khz;

	return aligned_freq;
}

/**
 * exynos5_dmc_init_clks() - Initialize clocks needed for DMC operation.
 * @dev:	device for which the clocks are setup
 * @dmc:	DMC structure containing needed fields
 *
 * Get the needed clocks defined in DT device, enable and set the right parents.
 * Read current frequency and initialize the initial rate for governor.
 */
static int exynos5_dmc_init_clks(struct device *dev, struct exynos5_dmc *dmc)
{
	int ret;
	unsigned long target_volt = 0;
	unsigned long target_rate = 0;

	dmc->fout_spll = devm_clk_get(dev, "fout_spll");
	if (IS_ERR(dmc->fout_spll))
		return PTR_ERR(dmc->fout_spll);

	dmc->fout_bpll = devm_clk_get(dev, "fout_bpll");
	if (IS_ERR(dmc->fout_bpll))
		return PTR_ERR(dmc->fout_bpll);

	dmc->mout_mclk_cdrex = devm_clk_get(dev, "mout_mclk_cdrex");
	if (IS_ERR(dmc->mout_mclk_cdrex))
		return PTR_ERR(dmc->mout_mclk_cdrex);

	dmc->mout_bpll = devm_clk_get(dev, "mout_bpll");
	if (IS_ERR(dmc->mout_bpll))
		return PTR_ERR(dmc->mout_bpll);

	dmc->mout_mx_mspll_ccore = devm_clk_get(dev, "mout_mx_mspll_ccore");
	if (IS_ERR(dmc->mout_mx_mspll_ccore))
		return PTR_ERR(dmc->mout_mx_mspll_ccore);

	dmc->dout_clk2x_phy0 = devm_clk_get(dev, "dout_clk2x_phy0");
	if (IS_ERR(dmc->dout_clk2x_phy0))
		return PTR_ERR(dmc->dout_clk2x_phy0);

	dmc->mout_spll = devm_clk_get(dev, "ff_dout_spll2");
	if (IS_ERR(dmc->mout_spll))
		return PTR_ERR(dmc->mout_spll);

	/*
	 * Convert frequency to KHz values and set it for the governor.
	 */
	dmc->curr_rate = clk_get_rate(dmc->mout_mclk_cdrex) / 1000;
	dmc->curr_rate = exynos5_dmc_align_init_freq(dmc, dmc->curr_rate);
	exynos5_dmc_df_profile.initial_freq = dmc->curr_rate;

	ret = exynos5_dmc_get_volt_freq(dev, &dmc->curr_rate, &target_rate,
					&target_volt, 0);
	if (ret)
		return ret;

	dmc->curr_volt = target_volt;

	clk_prepare_enable(dmc->mout_spll);
	clk_set_parent(dmc->mout_mx_mspll_ccore, dmc->mout_spll);
	clk_prepare_enable(dmc->mout_mx_mspll_ccore);

	clk_prepare_enable(dmc->fout_bpll);
	clk_prepare_enable(dmc->mout_bpll);

	return 0;
}

/**
 * exynos5_performance_counters_init() - Initializes performance DMC's counters
 * @dmc:	DMC for which it does the setup
 *
 * Initialization of performance counters in DMC for estimating usage.
 * The counter's values are used for calculation of a memory bandwidth and based
 * on that the governor changes the frequency.
 * The counters are not used when the governor is GOVERNOR_USERSPACE.
 */
static int exynos5_performance_counters_init(struct exynos5_dmc *dmc)
{
	int counters_size;
	int ret, i;

	dmc->num_counters = devfreq_event_get_edev_count(dmc->dev);
	if (dmc->num_counters < 0) {
		dev_err(dmc->dev, "could not get devfreq-event counters\n");
		return dmc->num_counters;
	}

	counters_size = sizeof(struct devfreq_event_dev) * dmc->num_counters;
	dmc->counter = devm_kzalloc(dmc->dev, counters_size, GFP_KERNEL);
	if (!dmc->counter)
		return -ENOMEM;

	for (i = 0; i < dmc->num_counters; i++) {
		dmc->counter[i] = devfreq_event_get_edev_by_phandle(dmc->dev, i);
		if (IS_ERR_OR_NULL(dmc->counter[i]))
			return -EPROBE_DEFER;
	}

	ret = exynos5_counters_enable_edev(dmc);
	if (ret < 0) {
		dev_err(dmc->dev, "could not enable event counter\n");
		return ret;
	}

	ret = exynos5_counters_set_event(dmc);
	if (ret < 0) {
		dev_err(dmc->dev, "counld not set event counter\n");
		return ret;
	}

	mutex_lock(&dmc->lock);
	dmc->counters_enabled = true;
	mutex_unlock(&dmc->lock);

	return 0;
}

/**
 * exynos5_dmc_probe() - Probe function for the DMC driver
 * @pdev:	platform device for which the driver is going to be initialized
 *
 * Initialize basic components: clocks, regulators, performance counters, etc.
 * Read out product version and based on the information setup
 * internal structures for the controller (frequency and voltage) and for DRAM
 * memory parameters: timings for each operating frequency.
 * Register new devfreq device for controlling DVFS of the DMC.
 */
static int exynos5_dmc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct exynos5_dmc *dmc;
	struct device *dev = &pdev->dev;
	struct resource *res;

	dev_info(&pdev->dev, "DMC initializing\n");

	dmc = devm_kzalloc(dev, sizeof(*dmc), GFP_KERNEL);
	if (!dmc)
		return -ENOMEM;

	mutex_init(&dmc->lock);

	dmc->dev = dev;
	platform_set_drvdata(pdev, dmc);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dmc->base_drexi0 = devm_ioremap_resource(dev, res);
	if (IS_ERR(dmc->base_drexi0))
		return PTR_ERR(dmc->base_drexi0);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	dmc->base_drexi1 = devm_ioremap_resource(dev, res);
	if (IS_ERR(dmc->base_drexi1))
		return PTR_ERR(dmc->base_drexi1);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	dmc->base_clk = devm_ioremap_resource(dev, res);
	if (IS_ERR(dmc->base_clk))
		return PTR_ERR(dmc->base_clk);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	dmc->chip_id = devm_ioremap_resource(dev, res);
	if (IS_ERR(dmc->chip_id))
		return PTR_ERR(dmc->chip_id);

	ret = exynos5_dmc_chip_revision_settings(dmc);
	if (ret)
		return ret;

	ret = exynos5_init_freq_table(dev, dmc, &exynos5_dmc_df_profile);
	if (ret)
		return ret;

	dmc->vdd_mif = devm_regulator_get(dev, "vdd_mif");
	if (IS_ERR(dmc->vdd_mif)) {
		ret = PTR_ERR(dmc->vdd_mif);
		dev_warn(dev, "couldn't get regulator\n");
		goto remove_opp_table;
	}

	ret = exynos5_dmc_init_clks(dev, dmc);
	if (ret) {
		dev_warn(dev, "couldn't initialize clocks\n");
		goto remove_opp_table;
	}

	ret = exynos5_dmc_pause_on_switching(dmc, 1);
	if (ret) {
		dev_warn(dev, "couldn't setup pause on switching\n");
		goto remove_clocks;
	}

	ret = exynos5_performance_counters_init(dmc);
	if (ret) {
		dev_warn(dev, "couldn't probe performance counters\n");
		goto remove_clocks;
	}
	/*
	 * Setup default thresholds for the devfreq governor.
	 * The values are chosen based on experiments.
	 */
	dmc->gov_data.upthreshold = 30;
	dmc->gov_data.downdifferential = 5;

	dmc->df = devm_devfreq_add_device(dev, &exynos5_dmc_df_profile,
					  DEVFREQ_GOV_USERSPACE,
					  &dmc->gov_data);

	if (IS_ERR(dmc->df)) {
		ret = PTR_ERR(dmc->df);
		goto err_devfreq_add;
	}

	ret = sysfs_create_group(&dmc->df->dev.kobj, &env_group);
	if (ret) {
		dev_err(dev, "couldn't add sysfs group\n");
		goto err_devfreq_add;
	}

	dev_info(&pdev->dev, "DMC init done\n");

	return 0;

err_devfreq_add:
	exynos5_counters_disable_edev(dmc);
remove_clocks:
	clk_disable_unprepare(dmc->mout_mx_mspll_ccore);
	clk_disable_unprepare(dmc->mout_spll);
remove_opp_table:
	dev_pm_opp_remove_table(&pdev->dev);

	dev_warn(&pdev->dev, "DMC init failed\n");
	return ret;
}

/**
 * exynos5_dmc_remove() - Remove function for the platform device
 * @pdev:	platform device which is going to be removed
 *
 * The function relies on 'devm' framework function which automatically
 * clean the device's resources. It just calls explicitly disable function for
 * the performance counters.
 */
static int exynos5_dmc_remove(struct platform_device *pdev)
{
	struct exynos5_dmc *dmc = dev_get_drvdata(&pdev->dev);

	exynos5_counters_disable_edev(dmc);

	clk_disable_unprepare(dmc->mout_mx_mspll_ccore);
	clk_disable_unprepare(dmc->mout_spll);

	dev_pm_opp_remove_table(&pdev->dev);
	sysfs_remove_group(&dmc->df->dev.kobj, &env_group);

	dev_info(&pdev->dev, "DMC removed\n");

	return 0;
}

static const struct of_device_id exynos5_dmc_of_match[] = {
	{ .compatible = "samsung,exynos5422-dmc", },
	{ },
};
MODULE_DEVICE_TABLE(of, exynos5_dmc_of_match);

static struct platform_driver exynos5_dmc_platdrv = {
	.probe	= exynos5_dmc_probe,
	.remove = exynos5_dmc_remove,
	.driver = {
		.name	= "exynos5-dmc",
		.of_match_table = exynos5_dmc_of_match,
	},
};
module_platform_driver(exynos5_dmc_platdrv);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Samsung");
