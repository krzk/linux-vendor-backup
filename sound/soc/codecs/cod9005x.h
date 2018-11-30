/*
 * Copyright (c) 2014 Samsung Electronics Co. Ltd.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _COD9005X_H
#define _COD9005X_H

#include <linux/completion.h>

#include <sound/soc.h>
#include <linux/switch.h>
#include <linux/iio/consumer.h>

#define CONFIG_SND_SOC_SAMSUNG_VERBOSE_DEBUG 1

extern const struct regmap_config cod9005x_regmap;
extern int cod9005x_jack_mic_register(struct snd_soc_codec *codec);

#define COD9005X_OTP_MAX_REG				0x0F
#define COD9005X_MAX_REGISTER				0xF6
#define COD9005X_OTP_REG_WRITE_START		0xD0

#define COD9005X_REGCACHE_SYNC_START_REG	0x00
#define COD9005X_REGCACHE_SYNC_END_REG		0xF6

struct cod9005x_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct device *dev;

	struct regulator *vdd;
	struct regulator *vdd2;

	int sysclk;
	int asyncclk;

	int num_inputs;
	int int_gpio;

	struct pinctrl *pinctrl;
	unsigned int spk_ena:2;

	unsigned int regulator_count;
	bool is_suspend;
	bool is_probe_done;
	struct mutex jackdet_lock;
	struct switch_dev sdev;
	struct completion initialize_complete;
	int irq_val[10];
	struct input_dev *input;
	struct mutex key_lock;
	struct timer_list timer;
	unsigned short i2c_addr;
	unsigned char otp_reg[COD9005X_OTP_MAX_REG];
	unsigned int mic_bias_ldo_voltage;
	unsigned int aifrate;
	unsigned int avc_slope_param1;
	unsigned int avc_slope_param2;
	bool update_fw;
	int gdet_adc_delay;
	struct iio_channel *jack_adc;
	struct mutex adc_mute_lock;
	struct workqueue_struct *adc_mute_wq;
	struct work_struct adc_mute_work;
	int adc_pin;
	unsigned int lvol;
	unsigned int rvol;
};

/*
 * Helper macros for creating bitmasks
 */
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)


/*
 * Register values.
*/

/* PMIC DMIC BIAS Register */
#define PMIC_SPEEDY_ADDR				0x01
#define PMIC_DMIC_BIAS_REG				0x63
#define PMIC_DMIC_BIAS_ON				0xC3
#define PMIC_DMIC_BIAS_OFF				0x03
#define DMIC_BIAS_EN_MASK				BIT(6)
#define PMIC_BOOST_REG					0x21
#define PMIC_BOOST_ON					0xE1
#define PMIC_BOOST_OFF					0x21

/* Audio IRQ and Status */
#define COD9005X_00_BASE_REG			0x00
#define COD9005X_01_IRQ1PEND			0x01
#define COD9005X_02_IRQ2PEND			0x02
#define COD9005X_03_IRQ3PEND			0x03
#define COD9005X_04_IRQ4PEND			0x04
#define COD9005X_07_IRQ3M				0x07
#define COD9005X_08_IRQ4M				0x08
#define COD9005X_0E_IO_CTRL0			0x0E
#define COD9005X_0F_IO_CTRL1			0x0F

/* Power Down, Reference circuit */
#define COD9005X_10_PD_REF				0x10
#define COD9005X_14_PD_DA1				0x14
#define COD9005X_15_PD_DA2				0x15
#define COD9005X_18_PWAUTO_DA			0x18
#define COD9005X_19_CTRL_REF			0x19
#define COD9005X_1B_SDM_CTR				0x1B
#define COD9005X_1D_SV_DA				0x1D

/* Playback Path */
#define COD9005X_32_VOL_SPK				0x32
#define COD9005X_36_CTRL_SPK			0x36

/* Digital Function */
#define COD9005X_40_DIGITAL_POWER		0x40
#define COD9005X_41_IF1_FORMAT1			0x41
#define COD9005X_42_IF1_FORMAT2			0x42
#define COD9005X_43_IF1_FORMAT3			0x43
#define COD9005X_44_IF1_FORMAT4			0x44
#define COD9005X_45_IF1_FORMAT5			0x45
#define COD9005X_46_ADC1				0x46
#define COD9005X_47_AVOLL1				0x47
#define COD9005X_48_AVOLR1				0x48
#define COD9005X_49_DMIC1				0X49
#define COD9005X_4A_DMIC2				0x4A
#define COD9005X_4B_AVOLL1_SUB			0x4B
#define COD9005X_4C_AVOLR1_SUB			0x4C

#define COD9005X_50_DAC1				0x50
#define COD9005X_51_DVOLL				0x51
#define COD9005X_52_DVOLR				0x52
#define COD9005X_54_AVC1				0x54

/* Sensor Detection & IRQ control */
#define COD9005X_60_IRQ_SENSOR			0x60

/* Test Mode Selection */
#define COD9005X_67_CED_CHK1			0x67
#define COD9005X_68_CED_CHK2			0x68
#define COD9005X_69_CED_CHK3			0x69

/* SW Reserverd */
#define COD9005X_6C_SW_RESERV			0x6C

/* Testing Register */
#define COD9005X_71_CLK2_COD			0x71
#define COD9005X_72_CLK3_COD			0x72
#define COD9005X_77_CHOP_DA				0x77

/* Accessory Detection */
#define COD9005X_80_PDB_ACC1            0x80
#define COD9005X_81_PDB_ACC2            0x81

/* Boost Control Register */
#define COD9005X_92_BOOST_HR0			0x92
#define COD9005X_93_BOOST_HR1			0x93
#define COD9005X_94_BST_CTL				0x94
#define COD9005X_95_FG_DAT_RO0			0x95
#define COD9005x_96_FG_DAT_RO1			0x96
#define COD9005X_97_VBAT_M0				0x97
#define COD9005X_98_VBAT_M1				0x98
#define COD9005X_99_TEST_VBAT			0x99
#define COD9005X_9A_VSUM_RO0			0x9A
#define COD9005X_9B_VSUM_RO1			0x9B
#define COD9005X_9C_BOOST_CTR_RO		0x9C
#define COD9005X_9D_BGC4_0				0x9D
#define COD9005X_9E_BGC4_1				0x9E
#define COD9005X_9F_BGC4_2				0x9F

/* SPK Audio Sequence */
#define COD9005X_B0_CTR_SPK_MU1			0xB0
#define COD9005X_B1_CTR_SPK_MU2			0xB1
#define COD9005X_B2_CTR_SPK_MU3			0xB2

/* AVC Slope Parameter */
#define COD9005X_F2_AVC_PARAM18			0xF2
#define COD9005X_F3_AVC_PARAM19			0xF3

/* COD9005X_03_IRQ3PEND */
#define IRQ3_ERR_FLG_BCK_R				BIT(7)
#define IRQ3_ERR_FLG_XBCK_R				BIT(6)
#define IRQ3_ERR_FLG_LRCK_R				BIT(5)
#define IRQ3_ERR_FLG_XLRCK_R			BIT(4)
#define IRQ3_AUTO_MUTE_R				BIT(3)
#define IRQ3_ING_SEQ_R					BIT(2)
#define IRQ3_ERR_FLG_FG_R				BIT(1)
#define IRQ3_APON_SPK_R					BIT(0)

/* COD9005X_04_IRQ4PEND */
#define IRQ4_ERR_FLG_BCK_F				BIT(7)
#define IRQ4_ERR_FLG_XBCK_F				BIT(6)
#define IRQ4_ERR_FLG_LRCK_F				BIT(5)
#define IRQ4_ERR_FLG_XLRCK_F			BIT(4)
#define IRQ4_AUTO_MUTE_F				BIT(3)
#define IRQ4_ING_SEQ_F					BIT(2)
#define IRQ4_ERR_FLG_FG_F				BIT(1)
#define IRQ4_APON_SPK_F					BIT(0)

/* COD9005X_07_IRQ3M */
#define IRQ3M_ERR_FLG_BCK_R_M_SHIFT			7
#define IRQ3M_ERR_FLG_BCK_R_M_MASK			BIT(IRQ3M_ERR_FLG_BCK_R_M_SHIFT)

#define IRQ3M_ERR_FLG_XBCK_R_M_SHIFT		6
#define IRQ3M_ERR_FLG_XBCK_R_M_MASK			BIT(IRQ3M_ERR_FLG_XBCK_R_M_SHIFT)

#define IRQ3M_ERR_FLG_LRCK_R_M_SHIFT		5
#define IRQ3M_ERR_FLG_LRCK_R_M_MASK			BIT(IRQ3M_ERR_FLG_LRCK_R_M_SHIFT)

#define IRQ3M_ERR_FLG_XLRCK_R_M_SHIFT		4
#define IRQ3M_ERR_FLG_XLRCK_R_M_MASK		BIT(IRQ3M_ERR_FLG_XLRCK_R_M_SHIFT)

#define IRQ3M_AUTO_MUTE_R_M_SHIFT			3
#define IRQ3M_AUTO_MUTE_R_M_MASK			BIT(IRQ3M_AUTO_MUTE_R_M_SHIFT)

#define IRQ3M_ING_SEQ_R_M_SHIFT				2
#define IRQ3M_ING_SEQ_R_M_MASK				BIT(IRQ3M_ING_SEQ_R_M_SHIFT)

#define IRQ3M_ERR_FLG_FG_R_M_SHIFT			1
#define IRQ3M_ERR_FLG_FG_R_M_MASK			BIT(IRQ3M_ERR_FLG_FG_R_M_SHIFT)

#define IRQ3M_APON_SPK_R_M_SHIFT			0
#define IRQ3M_APON_SPK_R_M_MASK				BIT(IRQ3M_APON_SPK_R_M_SHIFT)

#define IRQ3M_MASK_ALL						0xFF

/* COD9005X_08_IRQ4M */
#define IRQ4M_ERR_FLG_BCK_F_M_SHIFT			7
#define IRQ4M_ERR_FLG_BCK_F_M_MASK			BIT(IRQ4M_ERR_FLG_BCK_F_M_SHIFT)

#define IRQ4M_ERR_FLG_XBCK_F_M_SHIFT		6
#define IRQ4M_ERR_FLG_XBCK_F_M_MASK			BIT(IRQ4M_ERR_FLG_XBCK_F_M_SHIFT)

#define IRQ4M_ERR_FLG_LRCK_F_M_SHIFT		5
#define IRQ4M_ERR_FLG_LRCK_F_M_MASK			BIT(IRQ4M_ERR_FLG_LRCK_F_M_SHIFT)

#define IRQ4M_ERR_FLG_XLRCK_F_M_SHIFT		4
#define IRQ4M_ERR_FLG_XLRCK_F_M_MASK		BIT(IRQ4M_ERR_FLG_XLRCK_F_M_SHIFT)

#define IRQ4M_AUTO_MUTE_F_M_SHIFT			3
#define IRQ4M_AUTO_MUTE_F_M_MASK			BIT(IRQ4M_AUTO_MUTE_F_M_SHIFT)

#define IRQ4M_ING_SEQ_F_M_SHIFT				2
#define IRQ4M_ING_SEQ_F_M_MASK				BIT(IRQ4M_ING_SEQ_F_M_SHIFT)

#define IRQ4M_ERR_FLG_FG_F_M_SHIFT			1
#define IRQ4M_ERR_FLG_FG_F_M_MASK			BIT(IRQ4M_ERR_FLG_FG_F_M_SHIFT)

#define IRQ4M_APON_SPK_F_M_SHIFT			0
#define IRQ4M_APON_SPK_F_M_MASK				BIT(IRQ4M_APON_SPK_F_M_SHIFT)

#define IRQ4M_MASK_ALL						0xFF

/* COD9005X_0E_IO_CTRL0 */
#define IO_CTRL0_SDOUT_ST				BIT(2)
#define IO_CTRL0_DMIC_CLK_ST1			BIT(1)
#define IO_CTRL0_DMIC_CLK_ST0			BIT(0)

/* COD9005X_0F_IO_CTRL1 */
#define IO_CTRL1_SDOUT_DAT_TN			BIT(5)
#define IO_CTRL1_SDOUT_DAT_EN			BIT(4)
#define IO_CTRL1_DMIC_CLK_TN1			BIT(3)
#define IO_CTRL1_DMIC_CLK_EN1			BIT(2)
#define IO_CTRL1_DMIC_CLK_TN0			BIT(1)
#define IO_CTRL1_DMIC_CLK_EN0			BIT(0)

/* COD9005X_10_PD_REF */
#define PDB_VMID_SHIFT					5
#define PDB_VMID_MASK					BIT(PDB_VMID_SHIFT)

#define PDB_IGEN_SHIFT					4
#define PDB_IGEN_MASK					BIT(PDB_IGEN_SHIFT)

/* COD9005X_14_PD_DA1 */
#define EN_DCTL_PREQ_SHIFT				5
#define EN_DCTL_PREQ_MASK				BIT(EN_DCTL_PREQ_SHIFT)

#define PDB_DCT_SHIFT					3
#define PDB_DCT_MASK					BIT(PDB_DCT_SHIFT)

#define RESETB_DCT_SHIFT				1
#define RESETB_DCT_MASK					BIT(RESETB_DCT_SHIFT)

/* COD9005X_15_PD_DA2 */
#define EN_MUTEB_SHIFT					4
#define EN_MUTEB_MASK					BIT(EN_MUTEB_SHIFT)

#define PDB_SPK_BIAS_SHIFT				2
#define PDB_SPK_BIAS_MASK				BIT(PDB_SPK_BIAS_SHIFT)

#define PDB_SPK_DAMP_SHIFT				1
#define PDB_SPK_DAMP_MASK				BIT(PDB_SPK_DAMP_SHIFT)

#define PDB_SPK_PROT_SHIFT				0
#define PDB_SPK_PROT_MASK				BIT(PDB_SPK_PROT_SHIFT)

/* COD9005X_18_PWAUTO_DA */
#define DLYST_DA_SHIFT					5
#define DLYST_DA_MASK					BIT(DLYST_DA_SHIFT)

#define APW_SPK_SHIFT					2
#define APW_SPK_MASK					BIT(APW_SPK_SHIFT)

/* COD9005X_19_CTRL_REF */
#define CTMF_VMID_SHIFT					6
#define CTMF_VMID_WIDTH					2
#define CTMF_VMID_MASK					MASK(CTMF_VMID_WIDTH, CTMF_VMID_SHIFT)

#define CTMF_VMID_OPEN					3
#define CTMF_VMID_500K_OM       		2
#define CTMF_VMID_50K_OM        		1
#define CTMF_VMID_5K_OM         		0

/* COD9005X_1B_SDM_CTR */
#define DSDM_OFFSET_SHIFT				0
#define DSDM_OFFSET_WIDTH				2
#define DSDM_OFFSET_MASK				MASK(DSDM_OFFSET_WIDTH, \
										DSDM_OFFSET_SHIFT)

#define DSDM_OFFSET_8					3
#define DSDM_OFFSET_4   	    		2
#define DSDM_OFFSET_2	        		1
#define DSDM_OFFSET_0	         		0

/* COD9005X_1D_SV_DA */
#define EN_SPK_SV_SHIFT					6
#define EN_SPK_SV_MASK					BIT(EN_SPK_SV_SHIFT)

/* COD9005X_36_CTRL_SPK */
#define EN_SPK_SDN_SHIFT				6
#define EN_SPK_SDN_MASK					BIT(EN_SPK_SDN_SHIFT)

#define CTMF_SPK_OSC_SHIFT				4
#define CTMF_SPK_OSC_WIDTH				2
#define CTMF_SPK_OSC_MASK				MASK(CTMF_SPK_OSC_WIDTH, \
										CTMF_SPK_OSC_SHIFT)

#define CTMF_SPK_OSC_ON					3
#define CTMF_SPK_OSC_OFF				0

#define EN_SPK_CAR_SHIFT				3
#define EN_SPK_CAR_MASK					BIT(CTMF_SPK_OSC_ON)

#define EN_SPK_DAMP_SHIFT				2
#define EN_SPK_DAMP_MASK				BIT(EN_SPK_DAMP_SHIFT)

#define CTMI_SPK_PROT_SHIFT				0
#define CTMI_SPK_PROT_WIDTH				2
#define CTMI_SPK_PROT_MASK				MASK(CTMI_SPK_PROT_WIDTH, \
										CTMI_SPK_PROT_SHIFT)

#define CTMI_SPK_PROT_1_9				3
#define CTMI_SPK_PROT_1_6				2
#define CTMI_SPK_PROT_1_3				1
#define CTMI_SPK_PROT_1_0				0

/* COD9005X_40_DIGITAL_POWER */
#define PDB_ADCDIG_SHIFT				7
#define PDB_ADCDIG_MASK					BIT(PDB_ADCDIG_SHIFT)

#define RSTB_DAT_AD_SHIFT				6
#define RSTB_DAT_AD_MASK				BIT(RSTB_DAT_AD_SHIFT)

#define PDB_DACDIG_SHIFT				5
#define PDB_DACDIG_MASK					BIT(PDB_DACDIG_SHIFT)

#define RSTB_DAT_DA_SHIFT				4
#define RSTB_DAT_DA_MASK				BIT(RSTB_DAT_DA_SHIFT)

#define SYS_RSTB_SHIFT					3
#define SYS_RSTB_MASK					BIT(SYS_RSTB_SHIFT)

#define RSTB_OVFW_DA_SHIFT				2
#define RSTB_OVFW_DA_MASK				BIT(RSTB_OVFW_DA_SHIFT)

#define DMIC_CLK1_EN_SHIFT				1
#define DMIC_CLK1_EN_MASK				BIT(DMIC_CLK1_EN_SHIFT)

#define DMIC_CLK0_EN_SHIFT				0
#define DMIC_CLK0_EN_MASK				BIT(DMIC_CLK0_EN_SHIFT)

/* COD9005X_41_IF1_FORMAT1 */
#define I2S1_DF_SHIFT					4
#define I2S1_DF_WIDTH					2
#define I2S1_DF_MASK					MASK(I2S1_DF_WIDTH, I2S1_DF_SHIFT)

#define BCLK_POL_SHIFT					1
#define BCLK_POL_MASK					BIT(BCLK_POL_SHIFT)

#define LRCLK_POL_SHIFT					0
#define LRCLK_POL_MASK					BIT(LRCLK_POL_SHIFT)

#define LRCLK_L_LOW_R_HIGH				1
#define LRCLK_L_HIGH_R_LOW				0

/* COD9005X_42_IF1_FORMAT2 */
#define I2S1_XFS_MODE_SHIFT				7
#define I2S1_XFS_MODE_MASK				BIT(I2S1_XFS_MODE_SHIFT)

#define I2S1_XFS_MODE_BLCK_256FS_DN		0
#define I2S1_XFS_MODE_BLCK_256FS_UP		1

#define I2S1_DL_SHIFT					0
#define I2S1_DL_WIDTH					6
#define I2S1_DL_MASK					MASK(I2S1_DL_WIDTH, I2S1_DL_SHIFT)

#define I2S1_DL_8BIT					0x8
#define I2S1_DL_16BIT					0x10
#define I2S1_DL_20BIT					0x14
#define I2S1_DL_24BIT					0x18
#define I2S1_DL_32BIT					0x20

/* COD9005X_43_IF1_FORMAT3 */
#define I2S1_XFS_SHIFT					0
#define I2S1_XFS_WIDTH					8
#define I2S1_XFS_MASK					MASK(I2S1_XFS_WIDTH, I2S1_XFS_SHIFT)

#define I2S1_XFS_16FS					0x10
#define I2S1_XFS_32FS					0x20
#define I2S1_XFS_48FS					0x30
#define I2S1_XFS_64FS					0x40
#define I2S1_XFS_96FS					0x60
#define I2S1_XFS_128FS					0x80

/* COD9005X_44_IF1_FORMAT4 */
#define SEL_ADC3_SHIFT					6
#define SEL_ADC3_WIDTH					2
#define SEL_ADC3_MASK					MASK(SEL_ADC3_WIDTH, SEL_ADC3_SHIFT)

#define SEL_ADC2_SHIFT					4
#define SEL_ADC2_WIDTH					2
#define SEL_ADC2_MASK					MASK(SEL_ADC2_WIDTH, SEL_ADC2_SHIFT)

#define SEL_ADC1_SHIFT					2
#define SEL_ADC1_WIDTH					2
#define SEL_ADC1_MASK					MASK(SEL_ADC1_WIDTH, SEL_ADC1_SHIFT)

#define SEL_ADC0_SHIFT					0
#define SEL_ADC0_WIDTH					2
#define SEL_ADC0_MASK					MASK(SEL_ADC0_WIDTH, SEL_ADC0_SHIFT)

#define FUEL_GAUGE_OUTPUT_DATA			3
#define ADC_RCH_OUTPUT_DATA				1
#define ADC_LCH_OUTPUT_DATA				0

/* COD9005X_45_IF1_FORMAT5 */
#define SEL1_DAC1_SHIFT					4
#define SEL1_DAC1_WIDTH					2
#define SEL1_DAC1_MASK					MASK(SEL1_DAC1_WIDTH, SEL1_DAC1_SHIFT)

#define SEL1_DAC0_SHIFT					0
#define SEL1_DAC0_WIDTH					2
#define SEL1_DAC0_MASK					MASK(SEL1_DAC0_WIDTH, SEL1_DAC0_SHIFT)

#define DAC1_OUT						0
#define DAC2_OUT						1
#define DAC3_OUT						2
#define DAC4_OUT						3

/* COD9005X_46_ADC1 */
#define ADC1_MUTE_AD_SOFT_SHIFT			7
#define ADC1_MUTE_AD_SPFT_MASK			BIT(ADC1_MUTE_AD_SOFT_SHIFT)

#define ADC1_HPF_EN_SHIFT				3
#define ADC1_HPF_EN_MASK				BIT(ADC1_HPF_EN_SHIFT)

#define ADC1_HPF_SEL_SHIFT				1
#define ADC1_HPF_SEL_WIDTH				2
#define ADC1_HPF_SEL_MASK				MASK(ADC1_HPF_SEL_WIDTH, \
										ADC1_HPF_SEL_SHIFT)

#define ADC1_HPF_SEL_238HZ				0
#define ADC1_HPF_SEL_200HZ				1
#define ADC1_HPF_SEL_100HZ				2
#define ADC1_HPF_SEL_14_9HZ				3

#define ADC1_MUTE_AD_EN_SHIFT			0
#define ADC1_MUTE_AD_EN_MASK			BIT(ADC1_MUTE_AD_EN_SHIFT)

#define ADC1_MUTE_ENABLE				1
#define ADC1_MUTE_DISABLE				0

/* COD9005X_49_DMIC1 */
#define DMIC_POL1_SHIFT					7
#define DMIC_POL1_MASK					BIT(DMIC_POL1_SHIFT)

#define SEL_DMIC_L_SHIFT				4
#define SEL_DMIC_L_WIDTH				3
#define SEL_DMIC_L_MASK					MASK(SEL_DMIC_L_WIDTH, SEL_DMIC_L_SHIFT)

#define DMIC_POL2_SHIFT					3
#define DMIC_POL2_MASK					BIT(DMIC_POL2_SHIFT)

#define SEL_DMIC_R_SHIFT				0

#define SEL_DMIC_R_WIDTH				3
#define SEL_DMIC_R_MASK					MASK(SEL_DMIC_R_WIDTH, SEL_DMIC_R_SHIFT)

#define DMIC_NORMAL_POL					0
#define DMIC_INVERTED_POL				1

#define ZERODATA						2
#define NOTUSE							3
#define DMIC1L							4
#define DMIC1R							5
#define DMIC2L							6
#define DMIC2R							7

/* COD9005X_4A_DMIC2 */
#define DMIC_GAIN_SHIFT					4
#define DMIC_GAIN_WIDTH					3
#define DMIC_GAIN_MASK					MASK(DMIC_GAIN_WIDTH, DMIC_GAIN_SHIFT)

#define LEVEL1							1
#define LEVEL2							2
#define LEVEL3							3
#define LEVEL4							4
#define LEVEL5							5
#define LEVEL6							6
#define LEVEL7							7

#define DMIC_OSR_SHIFT					2
#define DMIC_OSR_WIDTH					2
#define DMIC_OSR_MASK					MASK(DMIC_OSR_WIDTH, DMIC_OSR_SHIFT)

#define OSR128							0
#define OSR64							1
#define OSR32							2
#define OSR16							3

/**
 * COD9005X_4B_AVOLL1_SUB, COD9005X_4C_AVOLR1_SUB
 */
#define AD_DVOL_SHIFT					0
#define AD_DVOL_WIDTH					8
#define AD_DVOL_MAXNUM					0xEA

/* COD9005X_50_DAC1 */
#define DAC1_DEEM_SHIFT					7
#define DAC1_DEEM_MASK					BIT(DAC1_DEEM_SHIFT)

#define DAC1_MONOMIX_SHIFT				4
#define DAC1_MONOMIX_WIDTH				3
#define DAC1_MONOMIX_MASK				MASK(DAC1_MONOMIX_WIDTH, \
										DAC1_MONOMIX_SHIFT)

#define DAC1_DISABLE					0
#define DAC1_R_MONO						1
#define DAC1_L_MONO 					2
#define DAC1_LR_SWAP					3
#define DAC1_LR_BY_2_MONO				4
#define DAC1_LR_MONO					5

#define DAC1_SOFT_MUTE_SHIFT			1
#define DAC1_SOFT_MUTE_MASK				BIT(DAC1_SOFT_MUTE_SHIFT)

#define SOFT_MUTE_ENABLE				1
#define SOFT_MUTE_DISABLE				0

/**
 * COD9005X_51_DVOLL, COD9005X_52_DVOLR
 */
#define DA_DVOL_SHIFT					0
#define DA_DVOL_WIDTH					8
#define DA_DVOL_MAXNUM 					0xEA

/* COD9005X_54_AVC1 */
#define DAC_VOL_BYPS_SHIFT	7
#define DAC_VOL_BYPS_MASK	BIT(DAC_VOL_BYPS_SHIFT)

#define AVC_CON_FLAG_SHIFT	6
#define AVC_CON_FLAG_MASK	BIT(AVC_CON_FLAG_SHIFT)

#define AVC_VA_FLAG_SHIFT	5
#define AVC_VA_FLAG_MASK	BIT(AVC_VA_FLAG_SHIFT)

#define AVC_MU_FLAG_SHIFT	4
#define AVC_MU_FLAG_MASK	BIT(AVC_MU_FLAG_SHIFT)

#define AVC_BYPS_SHIFT		3
#define AVC_BYPS_MASK		BIT(AVC_BYPS_SHIFT)

#define AVC_VA_EN_SHIFT		2
#define AVC_VA_EN_MASK		BIT(AVC_VA_EN_SHIFT)

#define AVC_MU_EN_SHIFT		1
#define AVC_MU_EN_MASK		BIT(AVC_MU_EN_SHIFT)

#define AVC_EN_SHIFT		0
#define AVC_EN_MASK			BIT(AVC_EN_SHIFT)

/* COD9005X_60_IRQ_SENSOR */
#define ST_ERR_FLG_BCK_SHIFT			7
#define ST_ERR_FLG_BCK_MASK				BIT(ST_ERR_FLG_BCK_SHIFT)

#define ST_ERR_FLG_XBCK_SHIFT			6
#define	ST_ERR_FLG_XBCK_MASK			BIT(ST_ERR_FLG_XBCK_SHIFT)

#define ST_ERR_FLG_LRCK_SHIFT			5
#define ST_ERR_FLG_LRCK_MASK			BIT(ST_ERR_FLG_LRCK_SHIFT)

#define ST_ERR_FLG_XLRCK_SHIFT			4
#define ST_ERR_FLG_XLRCK_MASK			BIT(ST_ERR_FLG_XLRCK_SHIFT)

#define ST_AUTO_MUTE_SHIFT				3
#define ST_AUTO_MUTE_MASK				BIT(ST_AUTO_MUTE_SHIFT)

#define ST_ING_SEQ_SHIFT				2
#define ST_ING_SEQ_MASK					BIT(ST_ING_SEQ_SHIFT)

#define ST_ERR_FLG_FG_SHIFT				1
#define ST_ERR_FLG_FG_MASK				BIT(ST_ERR_FLG_FG_SHIFT)

#define ST_APON_SPK_SHIFT				0
#define ST_APON_SPK_MASK				BIT(ST_APON_SPK_SHIFT)

/* COD9005X_6C_SW_RESERV */
#define EN_DMIC2_SHIFT  				3
#define EN_DMIC2_MASK   				BIT(EN_DMIC2_SHIFT)

#define EN_DMIC1_SHIFT  				2
#define EN_DMIC1_MASK   				BIT(EN_DMIC1_SHIFT)

#define EN_SPK_SHIFT					1
#define EN_SPK_MASK						BIT(EN_SPK_SHIFT)

/* COD9005X_71_CLK2_COD */
#define SEL_CHCLK_DA_SHIFT				4
#define SEL_CHCLK_DA_WIDTH				2
#define SEL_CHCLK_DA_MASK				MASK(SEL_CHCLK_DA_WIDTH, \
										SEL_CHCLK_DA_SHIFT)

#define CHOP_CLK_1_BY_4					0
#define CHOP_CLK_1_BY_8					1
#define CHOP_CLK_1_BY_16				2
#define CHOP_CLK_1_BY_32				3

#define EN_HALF_CHOP_DA_SHIFT			0
#define EN_HALF_CHOP_DA_WIDTH			2
#define EN_HALF_CHOP_DA_MASK			MASK(EN_HALF_CHOP_DA_WIDTH, \
										EN_HALF_CHOP_DA_SHIFT)

#define PHASE_SEL_ORIG					0
#define PHASE_SEL_1_BY_4				1
#define PHASE_SEL_2_BY_4				2
#define PHASE_SEL_3_BY_4				3

/* COD9005X_72_CLK3_COD */
#define CLK_DA_INV_SHIFT				5
#define CLK_DA_INV_MASK					BIT(CLK_DA_INV_SHIFT)

#define CLK_DACHOP_INV_SHIFT			4
#define CLK_DACHOP_INV_MASK				BIT(CLK_DACHOP_INV_SHIFT)

/* COD9005X_77_CHOP_DA */
#define EN_DCT_CHOP_SHIFT				5
#define EN_DCT_CHOP_MASK				BIT(EN_DCT_CHOP_SHIFT)

#define EN_SPK_CHOP_SHIFT				1
#define EN_SPK_CHOP_MASK				BIT(EN_SPK_CHOP_SHIFT)

/* COD9005X_80_PDB_ACC1 */
#define EN_ACC_CLK_SHIFT     			1
#define EN_ACC_CLK_MASK      			BIT(EN_ACC_CLK_SHIFT)

/* COD9005X_81_PDB_ACC2 */
#define ENB_HVS_SHIFT   				2
#define ENB_HVS_MASK    				BIT(ENB_HVS_SHIFT)

#define EN_OCC_SHIFT					1
#define EN_OCC_MASK						BIT(EN_OCC_SHIFT)

/* COD9005X_94_BST_CTL */
#define BOOST_CTRL_EN_SHIFT				3
#define BOOST_CTRL_EN_MASK				BIT(BOOST_CTRL_EN_SHIFT)

/* COD9005X_B0_CTR_SPK_MU1 */
#define AUTO_RESET_ENB_SHIFT			4
#define AUTO_RESET_ENB_MASK				BIT(AUTO_RESET_ENB_SHIFT)

#define SEL_AMU_SHIFT					1
#define SEL_AMU_MASK					BIT(SEL_AMU_SHIFT)

#define AMU_SPK_SHIFT					0
#define AMU_SPK_MASK					BIT(AMU_SPK_SHIFT)

#endif /* _COD9005X_H */

