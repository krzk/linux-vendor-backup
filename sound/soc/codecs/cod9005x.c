/*
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/samsung/abox.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/completion.h>

#include <linux/mfd/samsung/s2mpw02-regulator.h>
#include <soc/samsung/acpm_mfd.h>
#include <sound/cod9005x.h>
#include <sound/samsung/abox.h>
#include "cod9005x.h"

#define COD9005X_SAMPLE_RATE_48KHZ	48000
#define COD9005X_SAMPLE_RATE_192KHZ	192000

#define COD9005X_MAX_IRQ_CHK_BITS	7
#define COD9005X_START_IRQ_CHK_BIT	2
#define COD9005X_MJ_DET_INVALID		(-1)

#define COD9005x_AVC_SLOPE_PARAM_MIN 0x00
#define COD9005x_AVC_SLOPE_PARAM_MAX 0xFF
#define COD9005x_AVC_SLOPE_PARAM1_DEFAULT 0x03
#define COD9005x_AVC_SLOPE_PARAM2_DEFAULT 0x1F

#define CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG 1
#ifdef CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG
#ifdef dev_dbg
#undef dev_dbg
#endif
#ifdef dev_info
#undef dev_info
#endif
#if 1 /* if: print option */
#define dev_dbg dev_err
#define dev_info dev_err
#else /* else: print option */
static void no_dev_dbg(void *v, char *s, ...)
{
}
#define dev_dbg no_dev_dbg
#define dev_info no_dev_dbg
#endif /* endif: print option */
#endif

/* Forward Declarations */
static int cod9005x_disable(struct device *dev);
static int cod9005x_enable(struct device *dev);
static inline void cod9005x_usleep(unsigned int u_sec)
{
	usleep_range(u_sec, u_sec + 10);
}

static int cod9005x_pmic_read_reg(u8 reg, u8 *dest)
{
	int ret;

	ret = exynos_acpm_read_reg(PMIC_SPEEDY_ADDR, reg, dest);

	if (ret) {
		pr_err("[%s] acpm ipc read fail!\n", __func__);
		return ret;
	}

	return 0;
}

static int cod9005x_pmic_write_reg(u8 reg, u8 value)
{
	int ret;

	ret = exynos_acpm_write_reg(PMIC_SPEEDY_ADDR, reg, value);

	if (ret) {
		pr_err("[%s] acpm ipc write fail!\n", __func__);
		return ret;
	}

	return ret;
}

/**
 * Return value:
 * true: if the register value cannot be cached, hence we have to read from the
 * hardware directly
 * false: if the register value can be read from cache
 */
static bool cod9005x_volatile_register(struct device *dev, unsigned int reg)
{
	/**
	 * For all the registers for which we want to restore the value during
	 * regcache_sync operation, we need to return true here. For registers
	 * whose value need not be cached and restored should return false here.
	 *
	 * For the time being, let us cache the value of all registers other
	 * than the IRQ pending and IRQ status registers.
	 */
	switch (reg) {
	case COD9005X_01_IRQ1PEND ... COD9005X_04_IRQ4PEND:
	case COD9005X_60_IRQ_SENSOR:
	case COD9005X_67_CED_CHK1 ... COD9005X_69_CED_CHK3:
	case COD9005X_80_PDB_ACC1 ... COD9005X_81_PDB_ACC2:
	case COD9005X_94_BST_CTL:
		return true;
	default:
		return false;
	}
}

/**
 * Return value:
 * true: if the register value can be read
 * flase: if the register cannot be read
 */
static bool cod9005x_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case COD9005X_01_IRQ1PEND ... COD9005X_0F_IO_CTRL1:
	case COD9005X_10_PD_REF ... COD9005X_1D_SV_DA:
	case COD9005X_32_VOL_SPK ... COD9005X_36_CTRL_SPK:
	case COD9005X_40_DIGITAL_POWER ... COD9005X_4C_AVOLR1_SUB:
	case COD9005X_50_DAC1 ... COD9005X_54_AVC1:
	case COD9005X_60_IRQ_SENSOR:
	case COD9005X_67_CED_CHK1 ... COD9005X_69_CED_CHK3:
	case COD9005X_6C_SW_RESERV:
	case COD9005X_71_CLK2_COD ... COD9005X_77_CHOP_DA:
	case COD9005X_80_PDB_ACC1 ... COD9005X_81_PDB_ACC2:
	case COD9005X_92_BOOST_HR0 ... COD9005X_9F_BGC4_2:
	case COD9005X_B0_CTR_SPK_MU1 ... COD9005X_B2_CTR_SPK_MU3:
	case COD9005X_F2_AVC_PARAM18 ... COD9005X_F3_AVC_PARAM19:
		return true;
	default:
		return false;
	}
}

static bool cod9005x_writeable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case COD9005X_01_IRQ1PEND ... COD9005X_0F_IO_CTRL1:
	case COD9005X_10_PD_REF ... COD9005X_1D_SV_DA:
	case COD9005X_32_VOL_SPK ... COD9005X_36_CTRL_SPK:
	case COD9005X_40_DIGITAL_POWER ... COD9005X_4C_AVOLR1_SUB:
	case COD9005X_50_DAC1 ... COD9005X_54_AVC1:
	case COD9005X_6C_SW_RESERV:
	case COD9005X_71_CLK2_COD ... COD9005X_77_CHOP_DA:
	case COD9005X_80_PDB_ACC1 ... COD9005X_81_PDB_ACC2:
	case COD9005X_92_BOOST_HR0 ... COD9005X_94_BST_CTL:
	case COD9005X_97_VBAT_M0 ... COD9005X_99_TEST_VBAT:
	case COD9005X_9D_BGC4_0 ... COD9005X_9F_BGC4_2:
	case COD9005X_B0_CTR_SPK_MU1 ... COD9005X_B2_CTR_SPK_MU3:
	case COD9005X_F2_AVC_PARAM18 ... COD9005X_F3_AVC_PARAM19:
		return true;
	default:
		return false;
	}
}

const struct regmap_config cod9005x_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	/* "speedy" string should be described in the name field
	 *  this will be used for the speedy inteface,
	 *  when read/write operations are used in the regmap driver.
	 * APM functions will be called instead of the I2C
	 * refer to the "drivers/base/regmap/regmap-i2c.c
	 */
	.name = "speedy, COD9005X",
	.max_register = COD9005X_MAX_REGISTER,
	.readable_reg = cod9005x_readable_register,
	.writeable_reg = cod9005x_writeable_register,
	.volatile_reg = cod9005x_volatile_register,

	.use_single_rw = true,
	.cache_type = REGCACHE_RBTREE,
};

/**
 * TLV_DB_SCALE_ITEM
 *
 * (TLV: Threshold Limit Value)
 *
 * For various properties, the dB values don't change linearly with respect to
 * the digital value of related bit-field. At most, they are quasi-linear,
 * that means they are linear for various ranges of digital values. Following
 * table define such ranges of various properties.
 *
 * TLV_DB_RANGE_HEAD(num)

 * num defines the number of linear ranges of dB values.
 *
 * s0, e0, TLV_DB_SCALE_ITEM(min, step, mute),
 * s0: digital start value of this range (inclusive)
 * e0: digital end valeu of this range (inclusive)
 * min: dB value corresponding to s0
 * step: the delta of dB value in this range
 * mute: ?
 *
 * Example:
 *	TLV_DB_RANGE_HEAD(3),
 *	0, 1, TLV_DB_SCALE_ITEM(-2000, 2000, 0),
 *	2, 4, TLV_DB_SCALE_ITEM(1000, 1000, 0),
 *	5, 6, TLV_DB_SCALE_ITEM(3800, 8000, 0),
 *
 * The above code has 3 linear ranges with following digital-dB mapping.
 * (0...6) -> (-2000dB, 0dB, 1000dB, 2000dB, 3000dB, 3800dB, 4600dB),
 *
 * DECLARE_TLV_DB_SCALE
 *
 * This macro is used in case where there is a linear mapping between
 * the digital value and dB value.
 *
 * DECLARE_TLV_DB_SCALE(name, min, step, mute)
 *
 * name: name of this dB scale
 * min: minimum dB value corresponding to digital 0
 * step: the delta of dB value
 * mute: ?
 *
 * NOTE: The information is mostly for user-space consumption, to be viewed
 * alongwith amixer.
 */

/**
 * cod9005x_dvol_adc_dac_tlv
 *
 * Map as per data-sheet:
 * 0x00 ~ 0xE0 : +42dB to -70dB, step 0.5dB
 * 0xE1 ~ 0xE5 : -72dB to -80dB, step 2dB
 * 0xE6 : -82.4dB
 * 0xE7 ~ 0xE9 : -84.3dB to -96.3dB, step 6dB
 *
 * When the map is in descending order, we need to set the invert bit
 * and arrange the map in ascending order. The offsets are calculated as
 * (max - offset).
 *
 * offset_in_table = max - offset_actual;
 *
 * DAC_DAL, reg(0x51), shift(0), width(8), invert(1), max(0xE9)
 * DAC_DAR, reg(0x52), shift(0), width(8), invert(1), max(0xE9)
 */
static const unsigned int cod9005x_dvol_adc_dac_tlv[] = {
	TLV_DB_RANGE_HEAD(4),
	0x00, 0x03, TLV_DB_SCALE_ITEM(-9630, 600, 0),
	0x04, 0x04, TLV_DB_SCALE_ITEM(-8240, 0, 0),
	0x05, 0x09, TLV_DB_SCALE_ITEM(-8000, 200, 0),
	0x0A, 0xE9, TLV_DB_SCALE_ITEM(-7000, 50, 0),
};

/**
 * DMIC GAIN
 *
 * Selection digital mic gain through conversion code level.
 */
static const char * const cod9005x_dmic_code_level_text[] = {
	"0", "1", "2", "3",
	"4", "5", "6", "7"
};

static const struct soc_enum cod9005x_dmic2_gain_code_level_enum =
	SOC_ENUM_SINGLE(COD9005X_4A_DMIC2, DMIC_GAIN_SHIFT,
			ARRAY_SIZE(cod9005x_dmic_code_level_text),
			cod9005x_dmic_code_level_text);

/**
 * mono_mix_mode
 *
 * Selecting the Mode of Mono Mixer (inside DAC block)
 */
static const char * const cod9005x_mono_mix_mode_text[] = {
	"Disable", "R", "L", "LR-Invert",
	"(L+R)/2", "L+R"
};

static const struct soc_enum cod9005x_mono_mix_mode_enum =
SOC_ENUM_SINGLE(COD9005X_50_DAC1, DAC1_MONOMIX_SHIFT,
		ARRAY_SIZE(cod9005x_mono_mix_mode_text),
		cod9005x_mono_mix_mode_text);

static void cod9005x_dac_mute(struct snd_soc_codec *codec, bool on)
{
	dev_dbg(codec->dev, "%s called, %s\n", __func__,
			on ? "Mute" : "Unmute");

	if (on)
		snd_soc_update_bits(codec, COD9005X_50_DAC1,
				DAC1_SOFT_MUTE_MASK, DAC1_SOFT_MUTE_MASK);
	else
		snd_soc_update_bits(codec, COD9005X_50_DAC1,
				DAC1_SOFT_MUTE_MASK, 0x0);
}

static void cod9005x_adc_digital_mute(struct snd_soc_codec *codec, bool on)
{
	dev_dbg(codec->dev, "%s called, %s\n", __func__,
			on ? "Mute" : "Unmute");

	if (on)
		snd_soc_update_bits(codec, COD9005X_46_ADC1,
				ADC1_MUTE_AD_EN_MASK, ADC1_MUTE_AD_EN_MASK);
	else
		snd_soc_update_bits(codec, COD9005X_46_ADC1,
				ADC1_MUTE_AD_EN_MASK, 0x0);
}

static int dac_soft_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	unsigned long dac_control;
	bool soft_mute_flag;

	dac_control = snd_soc_read(codec, COD9005X_50_DAC1);
	soft_mute_flag = dac_control & DAC1_SOFT_MUTE_MASK;

	ucontrol->value.integer.value[0] = soft_mute_flag;

	return 0;
}

static int dac_soft_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value)
		/* enable soft mute */
		cod9005x_dac_mute(codec, SOFT_MUTE_ENABLE);
	else
		/* diable soft mute */
		cod9005x_dac_mute(codec, SOFT_MUTE_DISABLE);

	dev_info(codec->dev, "%s: soft mute : %s\n", __func__,
			(value) ? "on":"off");

	return 0;
}

static int adc_digital_mute_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	unsigned long adc_control;
	bool digital_mute_flag;

	adc_control = snd_soc_read(codec, COD9005X_46_ADC1);
	digital_mute_flag = adc_control & ADC1_MUTE_AD_EN_MASK;

	ucontrol->value.integer.value[0] = digital_mute_flag;

	return 0;
}

static int adc_digital_mute_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value)
		/* enable soft mute */
		cod9005x_adc_digital_mute(codec, ADC1_MUTE_ENABLE);
	else
		/* diable soft mute */
		cod9005x_adc_digital_mute(codec, ADC1_MUTE_DISABLE);

	dev_info(codec->dev, "%s: digital mute : %s\n", __func__,
			(value) ? "on":"off");

	return 0;
}

static int codec_enable_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = !(cod9005x->is_suspend);

	return 0;
}

static int codec_enable_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value)
		cod9005x_enable(codec->dev);
	else
		cod9005x_disable(codec->dev);

	dev_info(codec->dev, "%s: codec enable : %s\n", __func__,
			(value) ? "on":"off");

	return 0;
}

static int dmic_clk_off_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	unsigned long digital_power;
	bool dmic_clk_flag;

	digital_power = snd_soc_read(codec, COD9005X_40_DIGITAL_POWER);
	dmic_clk_flag = digital_power & DMIC_CLK0_EN_MASK;

	ucontrol->value.integer.value[0] = dmic_clk_flag;

	return 0;
}

static int dmic_clk_off_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value) {
		snd_soc_write(codec, COD9005X_0F_IO_CTRL1, 0x2A);
		snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
				DMIC_CLK1_EN_MASK|DMIC_CLK0_EN_MASK,
				DMIC_CLK1_EN_MASK|DMIC_CLK0_EN_MASK);
	} else {
		snd_soc_write(codec, COD9005X_0F_IO_CTRL1, 0x00);
		snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
				DMIC_CLK1_EN_MASK|DMIC_CLK0_EN_MASK, 0);
	}

	dev_info(codec->dev, "%s: dmic clk : %s\n", __func__,
			(value) ? "on":"off");

	return 0;
}

static int dmic_bias_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u8 dmic_bias;
	int dmic_bias_flag;

	cod9005x_pmic_read_reg(PMIC_DMIC_BIAS_REG, &dmic_bias);
	dmic_bias_flag = dmic_bias & DMIC_BIAS_EN_MASK;

	ucontrol->value.integer.value[0] = dmic_bias_flag;

	return 0;
}

static int dmic_bias_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int value = ucontrol->value.integer.value[0];

	if (value)
		cod9005x_pmic_write_reg(PMIC_DMIC_BIAS_REG, PMIC_DMIC_BIAS_ON);
	else
		cod9005x_pmic_write_reg(PMIC_DMIC_BIAS_REG, PMIC_DMIC_BIAS_OFF);

	dev_info(codec->dev, "%s: dmic bias : %s\n", __func__,
			(value) ? "on":"off");

	return 0;
}

/**
 * struct snd_kcontrol_new cod9005x_snd_control
 *
 * Every distinct bit-fields within the CODEC SFR range may be considered
 * as a control elements. Such control elements are defined here.
 *
 * Depending on the access mode of these registers, different macros are
 * used to define these control elements.
 *
 * SOC_ENUM: 1-to-1 mapping between bit-field value and provided text
 * SOC_SINGLE: Single register, value is a number
 * SOC_SINGLE_TLV: Single register, value corresponds to a TLV scale
 * SOC_SINGLE_TLV_EXT: Above + custom get/set operation for this value
 * SOC_SINGLE_RANGE_TLV: Register value is an offset from minimum value
 * SOC_DOUBLE: Two bit-fields are updated in a single register
 * SOC_DOUBLE_R: Two bit-fields in 2 different registers are updated
 */

/**
 * All the data goes into cod9005x_snd_controls.
 * All path inter-connections goes into cod9005x_dapm_routes
 */
static const struct snd_kcontrol_new cod9005x_snd_controls[] = {
	SOC_SINGLE_TLV("ADC Left Gain", COD9005X_4B_AVOLL1_SUB,
			AD_DVOL_SHIFT,
			AD_DVOL_MAXNUM, 1, cod9005x_dvol_adc_dac_tlv),

	SOC_SINGLE_TLV("ADC Right Gain", COD9005X_4C_AVOLR1_SUB,
			AD_DVOL_SHIFT,
			AD_DVOL_MAXNUM, 1, cod9005x_dvol_adc_dac_tlv),

	SOC_DOUBLE_R_TLV("DAC Gain", COD9005X_51_DVOLL, COD9005X_52_DVOLR,
			DA_DVOL_SHIFT,
			DA_DVOL_MAXNUM, 1, cod9005x_dvol_adc_dac_tlv),

	SOC_ENUM("MonoMix Mode", cod9005x_mono_mix_mode_enum),

	SOC_ENUM("DMIC1 Volume", cod9005x_dmic2_gain_code_level_enum),

	SOC_SINGLE_EXT("Codec Enable", SND_SOC_NOPM, 0, 1, 0,
			codec_enable_get, codec_enable_put),

	SOC_SINGLE_EXT("DMIC1 Switch", SND_SOC_NOPM, 0, 1, 0,
			dmic_clk_off_get, dmic_clk_off_put),

	SOC_SINGLE_EXT("DMIC Bias", SND_SOC_NOPM, 0, 1, 0,
			dmic_bias_get, dmic_bias_put),

	SOC_SINGLE_EXT("DAC Soft Mute", SND_SOC_NOPM, 0, 1, 0,
			dac_soft_mute_get, dac_soft_mute_put),

	SOC_SINGLE_EXT("ADC Digital Mute", SND_SOC_NOPM, 0, 1, 0,
			adc_digital_mute_get, adc_digital_mute_put),
};

static int dac_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Boost control enable */
		snd_soc_write(codec, COD9005X_94_BST_CTL, 0x08);

		/* Mute scheme selection */
		snd_soc_update_bits(codec, COD9005X_B0_CTR_SPK_MU1,
				SEL_AMU_MASK, SEL_AMU_MASK);

		cod9005x_dac_mute(codec, SOFT_MUTE_ENABLE);

		/* DAC digital power On */
		snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
				PDB_DACDIG_MASK, PDB_DACDIG_MASK);

		/* DAC digital Reset On/Off */
		snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
				RSTB_DAT_DA_MASK, 0);
		snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
				RSTB_DAT_DA_MASK, RSTB_DAT_DA_MASK);

		/* LRCLK half-period count */
		snd_soc_read(codec, COD9005X_67_CED_CHK1);
		snd_soc_read(codec, COD9005X_68_CED_CHK2);

		/* BCLK half-period count */
		snd_soc_read(codec, COD9005X_69_CED_CHK3);

		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* DAC digital Reset Off */
		snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
				RSTB_DAT_DA_MASK, 0x0);

		/* DAC digital power Off */
		snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
				PDB_DACDIG_MASK, 0x0);

		/* Boost control disable */
		snd_soc_write(codec, COD9005X_94_BST_CTL, 0x01);

		break;

	default:
		break;
	}

	return 0;
}

static int cod9005x_dmic_capture_init(struct snd_soc_codec *codec)
{
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called.\n", __func__);

	/* enable ADC digital mute before configuring ADC */
	mutex_lock(&cod9005x->adc_mute_lock);
	cod9005x_adc_digital_mute(codec, ADC1_MUTE_ENABLE);
	mutex_unlock(&cod9005x->adc_mute_lock);

	/* DMIC CLK ON */
	snd_soc_write(codec, COD9005X_0F_IO_CTRL1, 0x2A);
	snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
			DMIC_CLK1_EN_MASK|DMIC_CLK0_EN_MASK,
			DMIC_CLK1_EN_MASK|DMIC_CLK0_EN_MASK);

	/* Recording Digital Power on */
	snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
			PDB_ADCDIG_MASK, PDB_ADCDIG_MASK);

	/* Recording Digital Reset on/off */
	snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
			RSTB_DAT_AD_MASK, 0x0);
	snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
			RSTB_DAT_AD_MASK, RSTB_DAT_AD_MASK);

	return 0;
}

static int cod9005x_dmic_capture_deinit(struct snd_soc_codec *codec)
{
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called.\n", __func__);

	/* DMIC CLK OFF */
	snd_soc_write(codec, COD9005X_0F_IO_CTRL1, 0x00);
	snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
			DMIC_CLK1_EN_MASK|DMIC_CLK0_EN_MASK, 0);

	/* Recording Digital Reset on */
	snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
			RSTB_DAT_AD_MASK, 0x0);

	/* Recording Digital  Power off */
	snd_soc_update_bits(codec, COD9005X_40_DIGITAL_POWER,
			PDB_ADCDIG_MASK, 0x0);

	/* disable ADC digital mute after configuring ADC */
	mutex_lock(&cod9005x->adc_mute_lock);
	cod9005x_adc_digital_mute(codec, ADC1_MUTE_DISABLE);
	mutex_unlock(&cod9005x->adc_mute_lock);

	return 0;
}

static int dadc_ev(struct snd_soc_dapm_widget *w, struct snd_kcontrol *kcontrol,
		int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;

	case SND_SOC_DAPM_POST_PMU:
		/* disable ADC digital mute after configuring ADC */
		queue_work(cod9005x->adc_mute_wq, &cod9005x->adc_mute_work);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* enable ADC digital mute before configuring ADC */
		mutex_lock(&cod9005x->adc_mute_lock);
		cod9005x_adc_digital_mute(codec, ADC1_MUTE_ENABLE);
		mutex_unlock(&cod9005x->adc_mute_lock);
		break;

	default:
		break;
	}

	return 0;
}

/*
static int cod9005x_mute_mic(struct snd_soc_codec *codec, bool on)
{
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called, %s\n", __func__, on ? "Mute" : "Unmute");

	if (on) {
		mutex_lock(&cod9005x->adc_mute_lock);
		cod9005x_adc_digital_mute(codec, true);
		mutex_unlock(&cod9005x->adc_mute_lock);
	} else {
		mutex_lock(&cod9005x->adc_mute_lock);
		cod9005x_adc_digital_mute(codec, false);
		mutex_unlock(&cod9005x->adc_mute_lock);
	}

	return 0;
}
*/

static int cod9005_power_on_dmic1(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD9005X_49_DMIC1,
			SEL_DMIC_L_MASK | SEL_DMIC_R_MASK,
			DMIC1L << SEL_DMIC_L_SHIFT | DMIC1L << SEL_DMIC_R_SHIFT);
	return 0;
}

static int cod9005_power_off_dmic1(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD9005X_49_DMIC1,
			SEL_DMIC_L_MASK | SEL_DMIC_R_MASK,
			NOTUSE << SEL_DMIC_L_SHIFT | NOTUSE << SEL_DMIC_R_SHIFT);

	return 0;
}

static int cod9005_power_on_dmic2(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD9005X_49_DMIC1,
			SEL_DMIC_L_MASK | SEL_DMIC_R_MASK,
			DMIC2L << SEL_DMIC_L_SHIFT | DMIC2L << SEL_DMIC_R_SHIFT);

	return 0;
}

static int cod9005_power_off_dmic2(struct snd_soc_codec *codec)
{
	dev_dbg(codec->dev, "%s called\n", __func__);

	snd_soc_update_bits(codec, COD9005X_49_DMIC1,
			SEL_DMIC_L_MASK | SEL_DMIC_R_MASK,
			NOTUSE << SEL_DMIC_L_SHIFT | NOTUSE << SEL_DMIC_R_SHIFT);

	return 0;
}

static int dvmid_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s called.\n", __func__);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod9005x_dmic_capture_init(codec);
		break;

	case SND_SOC_DAPM_POST_PMU:
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod9005x_dmic_capture_deinit(codec);
		break;

	default:
		break;
	}

	return 0;
}

static int dmic1_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod9005_power_on_dmic1(codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod9005_power_off_dmic1(codec);
		break;

	default:
		break;
	}

	return 0;
}

static int dmic2_pga_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		cod9005_power_on_dmic2(codec);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		cod9005_power_off_dmic2(codec);
		break;

	default:
		break;
	}

	return 0;
}

static int spkdrv_ev(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_info(codec->dev, "%s called, event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* SPK Path Auto Power On */
		snd_soc_update_bits(codec, COD9005X_18_PWAUTO_DA,
				DLYST_DA_MASK|APW_SPK_MASK, DLYST_DA_MASK|APW_SPK_MASK);

		msleep(105);

		/* DCT pre-charge Off */
		snd_soc_update_bits(codec, COD9005X_14_PD_DA1, EN_DCTL_PREQ_MASK, 0);

		cod9005x_dac_mute(codec, SOFT_MUTE_DISABLE);
		break;

	case SND_SOC_DAPM_PRE_PMD:

		/* For remove speaker off noise, speaker turn off before bclk disable */
		cod9005x_dac_mute(codec, SOFT_MUTE_ENABLE);
		snd_soc_update_bits(codec, COD9005X_18_PWAUTO_DA,
				DLYST_DA_MASK|APW_SPK_MASK, 0);
		cod9005x_usleep(8000);

		/* DCT pre-charge On */
		snd_soc_update_bits(codec, COD9005X_14_PD_DA1,
				EN_DCTL_PREQ_MASK, EN_DCTL_PREQ_MASK);

		break;

	default:
		break;
	}

	return 0;
}

/* DMIC1 DMIC1 L [6:4] */
static const char * const cod9005x_dmicl_src1[] = {
	"Zero Data1", "Zero Data2", "Zero Data3", "Not Use",
	"DMIC1L DMIC1L", "DMIC1R DMIC1L", "DMIC2L DMIC1L", "DMIC2R DMIC1L"
};

static SOC_ENUM_SINGLE_DECL(cod9005x_dmicl_enum1, COD9005X_49_DMIC1,
		SEL_DMIC_L_SHIFT, cod9005x_dmicl_src1);

static const struct snd_kcontrol_new cod9005x_dmicl_mux1 =
		SOC_DAPM_ENUM("DMICL Mux1", cod9005x_dmicl_enum1);

/* DMIC1 DMIC1 R [2:0] */
static const char * const cod9005x_dmicr_src1[] = {
	"Zero Data1", "Zero Data2", "Zero Data3", "Not Use",
	"DMIC1L DMIC1R", "DMIC1R DMIC1R", "DMIC2L DMIC1R", "DMIC2R DMIC1R"
};

static SOC_ENUM_SINGLE_DECL(cod9005x_dmicr_enum1, COD9005X_49_DMIC1,
		SEL_DMIC_R_SHIFT, cod9005x_dmicr_src1);

static const struct snd_kcontrol_new cod9005x_dmicr_mux1 =
		SOC_DAPM_ENUM("DMICR Mux1", cod9005x_dmicr_enum1);


/* SEL_ADC0 [1:0] */
static const char * const cod9005x_adc_dat_src0[] = {
	"ADC1", "ADC2", "ADC3", "Off"
};

static SOC_ENUM_SINGLE_DECL(cod9005x_adc_dat_enum0, COD9005X_44_IF1_FORMAT4,
		SEL_ADC0_SHIFT, cod9005x_adc_dat_src0);

static const struct snd_kcontrol_new cod9005x_adc_dat_mux0 =
		SOC_DAPM_ENUM("ADC DAT Mux0", cod9005x_adc_dat_enum0);

/* SEL_ADC1 [3:2] */
static const char * const cod9005x_adc_dat_src1[] = {
	"ADC1", "ADC2", "ADC3", "Off"
};

static SOC_ENUM_SINGLE_DECL(cod9005x_adc_dat_enum1, COD9005X_44_IF1_FORMAT4,
		SEL_ADC1_SHIFT, cod9005x_adc_dat_src1);

static const struct snd_kcontrol_new cod9005x_adc_dat_mux1 =
		SOC_DAPM_ENUM("ADC DAT Mux1", cod9005x_adc_dat_enum1);

static const struct snd_kcontrol_new spk_on[] = {
	SOC_DAPM_SINGLE("SPK On", COD9005X_6C_SW_RESERV,
			EN_SPK_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new dmic1_on[] = {
	SOC_DAPM_SINGLE("DMIC1 On", COD9005X_6C_SW_RESERV,
			EN_DMIC1_SHIFT, 1, 0),
};

static const struct snd_kcontrol_new dmic2_on[] = {
	SOC_DAPM_SINGLE("DMIC2 On", COD9005X_6C_SW_RESERV,
			EN_DMIC2_SHIFT, 1, 0),
};

static const struct snd_soc_dapm_widget cod9005x_dapm_widgets[] = {
	SND_SOC_DAPM_SWITCH("SPK", SND_SOC_NOPM, 0, 0, spk_on),
	SND_SOC_DAPM_SWITCH("DMIC1", SND_SOC_NOPM, 0, 0, dmic1_on),
	SND_SOC_DAPM_SWITCH("DMIC2", SND_SOC_NOPM, 0, 0, dmic2_on),

	SND_SOC_DAPM_SUPPLY("DVMID", SND_SOC_NOPM, 0, 0, dvmid_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUT_DRV_E("SPKDRV", SND_SOC_NOPM, 0, 0, NULL, 0,
			spkdrv_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_PGA_E("DMIC1_PGA", SND_SOC_NOPM, 0, 0,
			NULL, 0, dmic1_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_PGA_E("DMIC2_PGA", SND_SOC_NOPM, 0, 0,
			NULL, 0, dmic2_pga_ev,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),
#if 0
	SND_SOC_DAPM_MUX("DMICL Mux1", SND_SOC_NOPM, 0, 0,
			&cod9005x_dmicl_mux1),
	SND_SOC_DAPM_MUX("DMICR Mux1", SND_SOC_NOPM, 0, 0,
			&cod9005x_dmicr_mux1),
#endif
	SND_SOC_DAPM_MUX("ADC DAT Mux0", SND_SOC_NOPM, 0, 0,
			&cod9005x_adc_dat_mux0),
	SND_SOC_DAPM_MUX("ADC DAT Mux1", SND_SOC_NOPM, 0, 0,
			&cod9005x_adc_dat_mux1),

	SND_SOC_DAPM_DAC_E("DAC", "AIF Playback", SND_SOC_NOPM, 0, 0,
			dac_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_DAC_E("DAC", "AIF2 Playback", SND_SOC_NOPM, 0, 0,
			dac_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_ADC_E("DADC", "AIF Capture", SND_SOC_NOPM, 0, 0,
			dadc_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_ADC_E("DADC", "AIF2 Capture", SND_SOC_NOPM, 0, 0,
			dadc_ev, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("SPKOUTLN"),
	SND_SOC_DAPM_OUTPUT("AIF4OUT"),

	SND_SOC_DAPM_INPUT("IN1L"),
	SND_SOC_DAPM_INPUT("IN2L"),
	SND_SOC_DAPM_INPUT("AIF4IN"),
};

static const struct snd_soc_dapm_route cod9005x_dapm_routes[] = {
	/* Sink, Control, Source */
	{"SPKDRV", NULL, "DAC"},
	{"SPK", "SPK On", "SPKDRV"},
	{"SPKOUTLN", NULL, "SPK"},

	{"DAC", NULL, "AIF Playback"},
	{"DAC", NULL, "AIF2 Playback"},

	{"DMIC1_PGA", NULL, "IN1L"},
	{"DMIC1_PGA", NULL, "DVMID"},
	{"DMIC1", "DMIC1 On", "DMIC1_PGA"},
#if 0
	{"DMICL Mux1", "DMIC1L DMIC1L", "DMIC1"},
	{"DMICL Mux1", "DMIC1R DMIC1L", "DMIC1"},
	{"DMICR Mux1", "DMIC1L DMIC1R", "DMIC1"},
	{"DMICR Mux1", "DMIC1R DMIC1R", "DMIC1"},
	{"DMICL Mux1", "DMIC2L DMIC1L", "DMIC1"},
	{"DMICL Mux1", "DMIC2R DMIC1L", "DMIC1"},
	{"DMICR Mux1", "DMIC2L DMIC1R", "DMIC1"},
	{"DMICR Mux1", "DMIC2R DMIC1R", "DMIC1"},
#endif
	{"DMIC2_PGA", NULL, "IN2L"},
	{"DMIC2_PGA", NULL, "DVMID"},
	{"DMIC2", "DMIC2 On", "DMIC2_PGA"},
#if 0
	{"DMICL Mux1", "DMIC1L DMIC1L", "DMIC2"},
	{"DMICL Mux1", "DMIC1R DMIC1L", "DMIC2"},
	{"DMICR Mux1", "DMIC1L DMIC1R", "DMIC2"},
	{"DMICR Mux1", "DMIC1R DMIC1R", "DMIC2"},
	{"DMICL Mux1", "DMIC2L DMIC1L", "DMIC2"},
	{"DMICL Mux1", "DMIC2R DMIC1L", "DMIC2"},
	{"DMICR Mux1", "DMIC2L DMIC1R", "DMIC2"},
	{"DMICR Mux1", "DMIC2R DMIC1R", "DMIC2"},

	{"DADC", NULL, "DMICL Mux1"},
	{"DADC", NULL, "DMICR Mux1"},
#endif
	{"DADC", NULL, "DMIC1"},
	{"DADC", NULL, "DMIC2"},

	{"AIF Capture", NULL, "DADC"},
	{"AIF2 Capture", NULL, "DADC"},
};

static int cod9005x_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s called. fmt: %d\n", __func__, fmt);

	return 0;
}

static int cod9005x_dai_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{

	int ret;
	struct snd_soc_codec *codec = dai->codec;
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	/* Boost control enable */
	ret = regulator_enable(cod9005x->vdd2);

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

static void cod9005x_dai_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret;
	struct snd_soc_codec *codec = dai->codec;
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	/* Boost control disable */
	ret = regulator_disable(cod9005x->vdd2);

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);
}


static int cod9005x_dai_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;

	/* For remove speaker off noise, speaker turn off before bclk disable */
	if (substream->stream == 0) {
		cod9005x_dac_mute(codec, SOFT_MUTE_ENABLE);
		snd_soc_update_bits(codec, COD9005X_18_PWAUTO_DA,
				DLYST_DA_MASK|APW_SPK_MASK, 0);
		cod9005x_usleep(8000);
	}

	dev_dbg(codec->dev, "(%s) %s completed\n",
			substream->stream ? "C" : "P", __func__);

	return 0;
}

static void cod9005x_adc_mute_work(struct work_struct *work)
{
	struct cod9005x_priv *cod9005x =
		container_of(work, struct cod9005x_priv, adc_mute_work);
	struct snd_soc_codec *codec = cod9005x->codec;

	dev_dbg(codec->dev, "%s called.\n", __func__);

	mutex_lock(&cod9005x->adc_mute_lock);
	msleep(100);
	cod9005x_adc_digital_mute(codec, ADC1_MUTE_DISABLE);
	mutex_unlock(&cod9005x->adc_mute_lock);
}

static int cod9005x_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);
	unsigned int cur_aifrate, width;

	cur_aifrate = params_rate(params);
	width = params_width(params);

	dev_dbg(codec->dev, "%s called. width: %d, cur_aifrate: %d\n",
			__func__, width, cur_aifrate);

	if (cod9005x->aifrate != cur_aifrate) {
		if (cur_aifrate == COD9005X_SAMPLE_RATE_192KHZ)
			dev_dbg(codec->dev, "%s called. UHQA Mode\n", __func__);
		if (cur_aifrate == COD9005X_SAMPLE_RATE_48KHZ)
			dev_dbg(codec->dev, "%s called. Normal Mode\n", __func__);
		cod9005x->aifrate = cur_aifrate;
	}

	return 0;
}

static const struct snd_soc_dai_ops cod9005x_dai_ops = {
	.set_fmt = cod9005x_dai_set_fmt,
	.startup = cod9005x_dai_startup,
	.shutdown = cod9005x_dai_shutdown,
	.hw_params = cod9005x_dai_hw_params,
	.hw_free = cod9005x_dai_hw_free,
};

#define COD9005X_RATES		SNDRV_PCM_RATE_8000_192000

#define COD9005X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE |		\
		SNDRV_PCM_FMTBIT_S20_3LE |	\
		SNDRV_PCM_FMTBIT_S24_LE |	\
		SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver cod9005x_dai[] = {
	{
		.name = "cod9005x-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = COD9005X_RATES,
			.formats = COD9005X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = COD9005X_RATES,
			.formats = COD9005X_FORMATS,
		},
		.ops = &cod9005x_dai_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "cod9005x-aif2",
		.id = 2,
		.playback = {
			.stream_name = "AIF2 Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = COD9005X_RATES,
			.formats = COD9005X_FORMATS,
		},
		.capture = {
			.stream_name = "AIF2 Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = COD9005X_RATES,
			.formats = COD9005X_FORMATS,
		},
		.ops = &cod9005x_dai_ops,
		.symmetric_rates = 1,
	},
};

static int cod9005x_regulators_enable(struct snd_soc_codec *codec)
{
	int ret;
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	cod9005x->regulator_count++;
	ret = regulator_enable(cod9005x->vdd);
	dev_dbg(codec->dev, "%s regulator: %d\n",
			__func__, cod9005x->regulator_count);

	return ret;
}

static void cod9005x_regulators_disable(struct snd_soc_codec *codec)
{
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	cod9005x->regulator_count--;
	regulator_disable(cod9005x->vdd);
	dev_dbg(codec->dev, "%s regulator: %d\n",
			__func__, cod9005x->regulator_count);
}

static void cod9005x_reset_io_selector_bits(struct snd_soc_codec *codec)
{
	/* Reset output selector bits */
	snd_soc_update_bits(codec, COD9005X_77_CHOP_DA, EN_SPK_CHOP_MASK, 0x0);
}

/**
 * cod9005x_codec_initialize : To be called if f/w update fails
 *
 * In case the firmware is not present or corrupt, we should still be able to
 * run the codec with decent parameters. This values are updated as per the
 * latest stable firmware.
 *
 * The values provided in this function are hard-coded register values, and we
 * need not update these values as per bit-fields.
 */
static void cod9005x_codec_initialize(void *context)
{
	struct snd_soc_codec *codec = (struct snd_soc_codec *)context;
    struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s called, setting defaults\n", __func__);

#ifdef CONFIG_PM
	pm_runtime_get_sync(codec->dev);
#else
	cod9005x_enable(codec->dev);
#endif

	snd_soc_write(codec, COD9005X_80_PDB_ACC1, 0x02);
	cod9005x_usleep(15000);

	/* DCT pre-charge On */
	snd_soc_write(codec, COD9005X_14_PD_DA1, 0x20);

	/* IO Setting for DMIC */
	snd_soc_write(codec, COD9005X_0F_IO_CTRL1, 0x00);
	snd_soc_write(codec, COD9005X_40_DIGITAL_POWER, 0x58);

	/* I2S interface setting */
	snd_soc_write(codec, COD9005X_41_IF1_FORMAT1, 0x00);
	snd_soc_write(codec, COD9005X_42_IF1_FORMAT2, 0x10);
	snd_soc_write(codec, COD9005X_43_IF1_FORMAT3, 0x20);
	snd_soc_write(codec, COD9005X_44_IF1_FORMAT4, 0xE4);
	snd_soc_write(codec, COD9005X_45_IF1_FORMAT5, 0x10);

	/* DMIC control setting */
	snd_soc_write(codec, COD9005X_49_DMIC1, 0x33);
	snd_soc_write(codec, COD9005X_4A_DMIC2, 0x14);

	/* ADC digital volume setting */
	snd_soc_write(codec, COD9005X_4B_AVOLL1_SUB, 0x54);
	snd_soc_write(codec, COD9005X_4C_AVOLR1_SUB, 0x54);

	/* DAC digital volume setting */
	snd_soc_write(codec, COD9005X_51_DVOLL, 0x68);
	snd_soc_write(codec, COD9005X_52_DVOLR, 0x68);

	/* DAC, ADC digital mute */
	snd_soc_write(codec, COD9005X_50_DAC1, 0x02);
	snd_soc_write(codec, COD9005X_46_ADC1, 0x0D);

	/* AVC Slope Parameter Setting */
	snd_soc_write(codec, COD9005X_F2_AVC_PARAM18, cod9005x->avc_slope_param1);
	snd_soc_write(codec, COD9005X_F3_AVC_PARAM19, cod9005x->avc_slope_param2);

	/* All boot time hardware access is done. Put the device to sleep. */
#ifdef CONFIG_PM
	pm_runtime_put_sync(codec->dev);
#else
	cod9005x_disable(codec->dev);
#endif
}

/**
 * cod9005x_post_fw_update_success: To be called after f/w update
 *
 * The firmware may be enabling some of the path and power registers which are
 * used during path enablement. We need to keep the values of these registers
 * consistent so that the functionality of the codec driver doesn't change
 * because of the firmware.
 */

static void cod9005x_regmap_sync(struct device *dev)
{
	struct cod9005x_priv *cod9005x = dev_get_drvdata(dev);
	unsigned char reg[COD9005X_MAX_REGISTER] = {0,};
	int i;

	/* Read from Cache */
	for (i = 0; i < COD9005X_REGCACHE_SYNC_END_REG; i++)
		if (cod9005x_readable_register(dev, i) &&
				(!cod9005x_volatile_register(dev, i)))
			reg[i] = (unsigned char)
				snd_soc_read(cod9005x->codec, i);

	regcache_cache_bypass(cod9005x->regmap, true);

	snd_soc_write(cod9005x->codec, COD9005X_40_DIGITAL_POWER,
			reg[COD9005X_40_DIGITAL_POWER]);

	/* Update HW */
	for (i = 0; i < COD9005X_REGCACHE_SYNC_END_REG ; i++)
		if (cod9005x_writeable_register(dev, i) &&
				(!cod9005x_volatile_register(dev, i)))
			snd_soc_write(cod9005x->codec, i, reg[i]);

	regcache_cache_bypass(cod9005x->regmap, false);
}

static void cod9005x_reg_restore(struct snd_soc_codec *codec)
{
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, COD9005X_80_PDB_ACC1,
			EN_ACC_CLK_MASK, EN_ACC_CLK_MASK);

	/* Give 15ms delay before storing the otp values */
	cod9005x_usleep(15000);

	if (!cod9005x->is_probe_done) {
		cod9005x_regmap_sync(codec->dev);
		cod9005x_reset_io_selector_bits(codec);
	} else
		cod9005x_regmap_sync(codec->dev);
}

static void cod9005x_i2c_parse_dt(struct cod9005x_priv *cod9005x)
{
	struct device *dev = cod9005x->dev;
	struct device_node *np = dev->of_node;
	unsigned int slope_param;
	int ret;

	cod9005x->int_gpio = of_get_gpio(np, 0);

	dev_dbg(dev, "Bias voltage values: ldo = %d\n",
			cod9005x->mic_bias_ldo_voltage);

	if (of_find_property(dev->of_node,
				"update-firmware", NULL))
		cod9005x->update_fw = true;
	else
		cod9005x->update_fw = false;

	/* Tuning AVC slope parameter */
	ret = of_property_read_u32(dev->of_node, "avc-slope-param1", &slope_param);
	if (!ret)
		if (slope_param < COD9005x_AVC_SLOPE_PARAM_MIN)
			cod9005x->avc_slope_param1 = COD9005x_AVC_SLOPE_PARAM_MIN;
		else if (slope_param > COD9005x_AVC_SLOPE_PARAM_MAX)
			cod9005x->avc_slope_param1 = COD9005x_AVC_SLOPE_PARAM_MAX;
		else
			cod9005x->avc_slope_param1 = slope_param;
	else
		cod9005x->avc_slope_param1 = COD9005x_AVC_SLOPE_PARAM1_DEFAULT;

	ret = of_property_read_u32(dev->of_node, "avc-slope-param2", &slope_param);
	if (!ret)
		if (slope_param < COD9005x_AVC_SLOPE_PARAM_MIN)
			cod9005x->avc_slope_param2 = COD9005x_AVC_SLOPE_PARAM_MIN;
		else if (slope_param > COD9005x_AVC_SLOPE_PARAM_MAX)
			cod9005x->avc_slope_param2 = COD9005x_AVC_SLOPE_PARAM_MAX;
		else
			cod9005x->avc_slope_param2 = slope_param;
	else
		cod9005x->avc_slope_param2 = COD9005x_AVC_SLOPE_PARAM2_DEFAULT;
}

/*
struct codec_notifier_struct {
	struct cod9005x_priv *cod9005x;
};
static struct codec_notifier_struct codec_notifier_t;

static void  cod9005x_notifier_handler(struct notifier_block *nb,
		unsigned long insert,
		void *data)
{
	struct codec_notifier_struct *codec_notifier_data = data;
	struct cod9005x_priv *cod9005x = codec_notifier_data->cod9005x;
	unsigned int stat1, pend1, pend2, pend3, pend4;

	mutex_lock(&cod9005x->key_lock);

	pend1 = cod9005x->irq_val[0];
	pend2 = cod9005x->irq_val[1];
	pend3 = cod9005x->irq_val[2];
	pend4 = cod9005x->irq_val[3];
	stat1 = cod9005x->irq_val[4];

	dev_dbg(cod9005x->dev,
			"[DEBUG] %s , line %d 01: %02x, 02:%02x, 03:%02x, 04:%02x, stat1:%02x\n",
			__func__, __LINE__, pend1, pend2, pend3, pend4, stat1);

	mutex_unlock(&cod9005x->key_lock);
}
static BLOCKING_NOTIFIER_HEAD(cod9005x_notifier);

int cod9005x_register_notifier(struct notifier_block *n,
		struct cod9005x_priv *cod9005x)
{
	int ret;

	codec_notifier_t.cod9005x = cod9005x;
	ret = blocking_notifier_chain_register(&cod9005x_notifier, n);
	if (ret < 0)
		pr_err("[DEBUG] %s(%d)\n", __func__, __LINE__);
	return ret;
}

void cod9005x_call_notifier(int irq1, int irq2, int irq3, int irq4, int status1,
		int param1, int param2, int param3, int param4, int param5)
{
	struct cod9005x_priv *cod9005x = codec_notifier_t.cod9005x;

	dev_dbg(cod9005x->dev,
			"[DEBUG] %s(%d)  0x1: %02x 0x2: %02x 0x3: %02x 0x4: %02x\n",
			__func__, __LINE__, irq1, irq2, irq3, irq4);

	cod9005x->irq_val[0] = irq1;
	cod9005x->irq_val[1] = irq2;
	cod9005x->irq_val[2] = irq3;
	cod9005x->irq_val[3] = irq4;
	cod9005x->irq_val[4] = status1;

	blocking_notifier_call_chain(&cod9005x_notifier, 0, &codec_notifier_t);
}
EXPORT_SYMBOL(cod9005x_call_notifier);
struct notifier_block codec_notifier;
*/

static int cod9005x_codec_probe(struct snd_soc_codec *codec)
{
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "9005x CODEC_PROBE: (*) %s\n", __func__);
	cod9005x->codec = codec;

	cod9005x->vdd = devm_regulator_get(codec->dev, "vdd_ldo27");
	if (IS_ERR(cod9005x->vdd)) {
		dev_warn(codec->dev, "failed to get regulator vdd\n");
		return PTR_ERR(cod9005x->vdd);
	}


	cod9005x->vdd2 = devm_regulator_get(codec->dev, "vdd_codec");
	if (IS_ERR(cod9005x->vdd2)) {
		dev_warn(codec->dev, "failed to get regulator vdd2\n");
		return PTR_ERR(cod9005x->vdd2);
	}
#ifdef CONFIG_PM
	pm_runtime_get_sync(codec->dev);
#else
	cod9005x_enable(codec->dev);
#endif

	cod9005x->is_probe_done = true;

	INIT_WORK(&cod9005x->adc_mute_work, cod9005x_adc_mute_work);
	cod9005x->adc_mute_wq = create_singlethread_workqueue("adc_mute_wq");
	if (cod9005x->adc_mute_wq == NULL) {
		dev_err(codec->dev, "Failed to create adc_mute_wq\n");
		return -ENOMEM;
	}

	cod9005x->aifrate = COD9005X_SAMPLE_RATE_48KHZ;

	cod9005x_i2c_parse_dt(cod9005x);

	mutex_init(&cod9005x->adc_mute_lock);

	/*
	 * interrupt pin should be shared with pmic.
	 * so codec driver use notifier because of knowing
	 * the interrupt status from mfd.
	 */
//	codec_notifier.notifier_call = cod9005x_notifier_handler,
//		cod9005x_register_notifier(&codec_notifier, cod9005x);

//	set_codec_notifier_flag();

	msleep(20);

	cod9005x_codec_initialize(codec);

	/* it should be modify to move machine driver */
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "SPKOUTLN");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "IN1L");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "IN2L");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF2 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF2 Capture");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));


#ifdef CONFIG_PM
	pm_runtime_put_sync(codec->dev);
#else
	cod9005x_disable(codec->dev);
#endif
	return 0;
}

static int cod9005x_codec_remove(struct snd_soc_codec *codec)
{
	struct cod9005x_priv *cod9005x = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "(*) %s called\n", __func__);
	if (cod9005x->int_gpio) {
		free_irq(gpio_to_irq(cod9005x->int_gpio), cod9005x);
		gpio_free(cod9005x->int_gpio);
	}

	cod9005x_regulators_disable(codec);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_cod9005x = {
	.probe = cod9005x_codec_probe,
	.remove = cod9005x_codec_remove,
	.ignore_pmdown_time = true,
	.idle_bias_off = true,

	.component_driver = {
		.controls = cod9005x_snd_controls,
		.num_controls = ARRAY_SIZE(cod9005x_snd_controls),
		.dapm_widgets = cod9005x_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(cod9005x_dapm_widgets),
		.dapm_routes = cod9005x_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(cod9005x_dapm_routes),
	}
};

static int cod9005x_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct cod9005x_priv *cod9005x;
	struct pinctrl *pinctrl;
	int ret;

	cod9005x = kzalloc(sizeof(struct cod9005x_priv), GFP_KERNEL);
	if (cod9005x == NULL)
		return -ENOMEM;
	cod9005x->dev = &i2c->dev;
	cod9005x->i2c_addr = i2c->addr;
	cod9005x->is_probe_done = false;
	cod9005x->regulator_count = 0;

	cod9005x->regmap = devm_regmap_init_i2c(i2c, &cod9005x_regmap);
	if (IS_ERR(cod9005x->regmap)) {
		dev_err(&i2c->dev, "Failed to allocate regmap: %li\n",
				PTR_ERR(cod9005x->regmap));
		ret = -ENOMEM;
		goto err;
	}

	regcache_mark_dirty(cod9005x->regmap);

	pinctrl = devm_pinctrl_get(&i2c->dev);
	if (IS_ERR(pinctrl)) {
		dev_warn(&i2c->dev, "did not get pins for codec: %li\n",
				PTR_ERR(pinctrl));
	} else {
		cod9005x->pinctrl = pinctrl;
		dev_err(&i2c->dev, "cod9005x_i2c_probe pinctrl\n");
	}

	i2c_set_clientdata(i2c, cod9005x);

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_cod9005x,
			cod9005x_dai, ARRAY_SIZE(cod9005x_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
		goto err;
	}
#ifdef CONFIG_PM
	pm_runtime_enable(cod9005x->dev);
#endif

	return ret;

err:
	kfree(cod9005x);
	return ret;
}

static int cod9005x_i2c_remove(struct i2c_client *i2c)
{
	struct cod9005x_priv *cod9005x = dev_get_drvdata(&i2c->dev);

	snd_soc_unregister_codec(&i2c->dev);
	kfree(cod9005x);
	return 0;
}

static int cod9005x_enable(struct device *dev)
{
	struct cod9005x_priv *cod9005x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);

	abox_enable_mclk(true);

	cod9005x_regulators_enable(cod9005x->codec);

	cod9005x->is_suspend = false;

	/* Disable cache_only feature and sync the cache with h/w */
	regcache_cache_only(cod9005x->regmap, false);
	cod9005x_reg_restore(cod9005x->codec);

	return 0;
}

static int cod9005x_disable(struct device *dev)
{
	struct cod9005x_priv *cod9005x = dev_get_drvdata(dev);

	dev_dbg(dev, "(*) %s\n", __func__);

	cod9005x->is_suspend = true;

	/* As device is going to suspend-state, limit the writes to cache */
	regcache_cache_only(cod9005x->regmap, true);

	cod9005x_regulators_disable(cod9005x->codec);

	msleep(250);
	abox_enable_mclk(false);

	return 0;
}

static int cod9005x_sys_suspend(struct device *dev)
{
#ifndef CONFIG_PM
	if (abox_is_on()) {
		dev_dbg(dev, "(*)Don't suspend codec3035x, cp functioning\n");
		return 0;
	}
	dev_dbg(dev, "(*) %s\n", __func__);
	cod9005x_disable(dev);
#endif

	return 0;
}

static int cod9005x_sys_resume(struct device *dev)
{
#ifndef CONFIG_PM
	struct cod9005x_priv *cod9005x = dev_get_drvdata(dev);

	if (!cod9005x->is_suspend) {
		dev_dbg(dev, "(*)codec3035x not resuming, cp functioning\n");
		return 0;
	}
	dev_dbg(dev, "(*) %s\n", __func__);
	cod9005x_enable(dev);
#endif

	return 0;
}

#ifdef CONFIG_PM
static int cod9005x_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "(*) %s\n", __func__);
	cod9005x_enable(dev);

	return 0;
}

static int cod9005x_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "(*) %s\n", __func__);
	cod9005x_disable(dev);

	return 0;
}
#endif

static const struct dev_pm_ops cod9005x_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(
			cod9005x_sys_suspend,
			cod9005x_sys_resume
			)
#ifdef CONFIG_PM
		SET_RUNTIME_PM_OPS(
				cod9005x_runtime_suspend,
				cod9005x_runtime_resume,
				NULL
				)
#endif
};

static const struct i2c_device_id cod9005x_i2c_id[] = {
	{ "cod9005x", 3035 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cod9005x_i2c_id);

const struct of_device_id cod9005x_of_match[] = {
	{ .compatible = "codec,cod9005x", },
	{},
};

static struct i2c_driver cod9005x_i2c_driver = {
	.driver = {
		.name = "cod9005x",
		.owner = THIS_MODULE,
		.pm = &cod9005x_pm,
		.of_match_table = of_match_ptr(cod9005x_of_match),
	},
	.probe = cod9005x_i2c_probe,
	.remove = cod9005x_i2c_remove,
	.id_table = cod9005x_i2c_id,
};

module_i2c_driver(cod9005x_i2c_driver);

MODULE_DESCRIPTION("ASoC COD9005X driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:COD9005X-codec");
