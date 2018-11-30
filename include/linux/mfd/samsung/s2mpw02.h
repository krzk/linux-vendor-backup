/*
 * s2mpw02.h - Driver for the s2mpw02
 *
 *  Copyright (C) 2018 Samsung Electrnoics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __S2MPW02_MFD_H__
#define __S2MPW02_MFD_H__
#include <linux/platform_device.h>
#include <linux/regmap.h>
#if defined(CONFIG_CHARGER_S2MPW02)
#include <../drivers/battery_v2/include/sec_charging_common.h>
#endif
#include <linux/i2c.h>
#define S2MPW02_REG_INVALID             (0xff)

/* PMIC Top-Level Registers */
#define	S2MPW02_PMIC_REG_PMICID		0x01
#define	S2MPW02_PMIC_REG_INTSRC		0x20
#define	S2MPW02_PMIC_REG_INTSRC_MASK	0x21

#define S2MPW02_IRQSRC_PMIC		(1 << 0)
#define S2MPW02_IRQSRC_CHG		(1 << 1)
#define S2MPW02_IRQSRC_FG		(1 << 2)
#define S2MPW02_IRQSRC_CODEC		(1 << 3)

#define MFD_DEV_NAME "s2mpw02"

enum s2mpw02_irq_source {
	PMIC_INT1 = 0,
	PMIC_INT2,
	PMIC_INT3,
#if defined(CONFIG_CHARGER_S2MPW02)
	CHG_INT1,
	CHG_INT2,
	CHG_INT3,
	CHG_INT4,
#endif
#if defined(CONFIG_FUELGAUGE_S2MPW02)
	FG_INT,
#endif
	S2MPW02_IRQ_GROUP_NR,
};

#define S2MPW02_NUM_IRQ_PMIC_REGS	3
#define S2MPW02_NUM_IRQ_CHG_REGS	4
#define S2MPW02_NUM_IRQ_FG_REGS		1

#define S2MPW02_PMIC_VERSION_MASK	0x7
#define S2MPW02_PMIC_OTP_MASK		0x78
#define S2MPW02_PMIC_OTP_SHIFT		3

enum s2mpw02_irq {
	/* PMIC */
	S2MPW02_PMIC_IRQ_PWRONR_INT1,
	S2MPW02_PMIC_IRQ_PWRONF_INT1,
	S2MPW02_PMIC_IRQ_JIGONBF_INT1,
	S2MPW02_PMIC_IRQ_JIGONBR_INT1,
	S2MPW02_PMIC_IRQ_ACOKBF_INT1,
	S2MPW02_PMIC_IRQ_ACOKBR_INT1,
	S2MPW02_PMIC_IRQ_PWRON1S_INT1,
	S2MPW02_PMIC_IRQ_MRB_INT1,

	S2MPW02_PMIC_IRQ_RTC60S_INT2,
	S2MPW02_PMIC_IRQ_RTCA1_INT2,
	S2MPW02_PMIC_IRQ_RTCA0_INT2,
	S2MPW02_PMIC_IRQ_SMPL_INT2,
	S2MPW02_PMIC_IRQ_RTC1S_INT2,
	S2MPW02_PMIC_IRQ_WTSR_INT2,
	S2MPW02_PMIC_IRQ_WRSTB_INT2,

	S2MPW02_PMIC_IRQ_120C_INT3,
	S2MPW02_PMIC_IRQ_140C_INT3,
	S2MPW02_PMIC_IRQ_TSD_INT3,
#ifdef CONFIG_CHARGER_S2MPW02
	/* Charger */
	S2MPW02_CHG_IRQ_RECHG_INT1,
	S2MPW02_CHG_IRQ_CHGDONE_INT1,
	S2MPW02_CHG_IRQ_TOPOFF_INT1,
	S2MPW02_CHG_IRQ_PRECHG_INT1,
	S2MPW02_CHG_IRQ_CHGSTS_INT1,
	S2MPW02_CHG_IRQ_CIN2BAT_INT1,
	S2MPW02_CHG_IRQ_CHGVINOVP_INT1,
	S2MPW02_CHG_IRQ_CHGVIN_INT1,

	S2MPW02_CHG_IRQ_BAT_LVL_ON_INT2,
	S2MPW02_CHG_IRQ_TMROUT_INT2,
	S2MPW02_CHG_IRQ_CHGR_TSD_INT2,
	S2MPW02_CHG_IRQ_ADPATH_INT2,
	S2MPW02_CHG_IRQ_FCHG_INT2,
	S2MPW02_CHG_IRQ_A2D_CHGINOK_INT2,
	S2MPW02_CHG_IRQ_BATDET_INT2,

	S2MPW02_CHG_IRQ_WDT_INT3,

	S2MPW02_CHG_IRQ_JIGON_INT4,
	S2MPW02_CHG_IRQ_RID_ATTACH_INT4,
	S2MPW02_CHG_IRQ_FACT_LEAK_INT4,
	S2MPW02_CHG_IRQ_UART_BOOT_ON_INT4,
	S2MPW02_CHG_IRQ_UART_BOOT_OFF_INT4,
	S2MPW02_CHG_IRQ_USB_BOOT_ON_INT4,
	S2MPW02_CHG_IRQ_USB_BOOT_OFF_INT4,
	S2MPW02_CHG_IRQ_UART_CABLE_INT4,
#endif
#ifdef CONFIG_FUELGAUGE_S2MPW02
	/* Fuelgauge */
	S2MPW02_FG_IRQ_VBAT_L_INT,
	S2MPW02_FG_IRQ_SOC_L_INT,
	S2MPW02_FG_IRQ_IDLE_ST_INT,
	S2MPW02_FG_IRQ_INIT_ST_INT,
#endif
	S2MPW02_IRQ_NR,
};

enum sec_device_type {
	S2MPW02X,
};

struct s2mpw02_dev {
	struct device *dev;
	struct i2c_client *i2c;
	struct i2c_client *pmic;
	struct i2c_client *rtc;
	struct i2c_client *codec;
	struct i2c_client *charger;
	struct i2c_client *fuelgauge;
	struct i2c_client *close;
	struct mutex i2c_lock;
	struct apm_ops *ops;

	int type;
	int device_type;
	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	struct mutex irqlock;
	int irq_masks_cur[S2MPW02_IRQ_GROUP_NR];
	int irq_masks_cache[S2MPW02_IRQ_GROUP_NR];
	int topoff_mask_status;

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct s2mpw02_platform_data *pdata;
};

enum s2mpw02_types {
	TYPE_S2MPW02,
};

extern int s2mpw02_irq_init(struct s2mpw02_dev *s2mpw02);
extern void s2mpw02_irq_exit(struct s2mpw02_dev *s2mpw02);

/* S2MPW02 shared i2c API function */
extern int s2mpw02_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mpw02_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpw02_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mpw02_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mpw02_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mpw02_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mpw02_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#if defined(CONFIG_CHARGER_S2MPW02)
typedef struct s2mpw02_charger_platform_data {
	sec_charging_current_t *charging_current_table;
	int chg_float_voltage;
	char *charger_name;
	char *fuelgauge_name;
	char *wireless_charger_name;
	bool chg_eoc_dualpath;
	uint32_t is_1MHz_switching:1;
	sec_battery_full_charged_t full_check_type;
	/* 2nd full check */
	 sec_battery_full_charged_t full_check_type_2nd;
} s2mpw02_charger_platform_data_t;
#endif


enum S2MPW02_chip_revision {
	/*
	 * OTP and Trim data is not fused on EVT 0
	 * OTP and Trim data is written BL2
	 */
	EVT_0 = 0x0,

	/*
	 * OTP is fused but Trim data is written default value on EVT_0_OTP_NO_TRIM
	 * Trim data is written BL2
	 */
	EVT_0_OTP_NO_TRIM,

	/* OTP and Trim data is fused on EVT 0.5 */
	EVT_0_5,

	/* 3 OTP added
	 *	. UVLO 2.9V->3.1V
	 *	. FG Sensing Point VBATT->VSYS
	 *	. ACOK Debounce time 24ms->96ms
	 */
	EVT_0_5_OTP_CHANGE,
	EVT_1,

	/* EVT 1 otp changed
	 * 1. change FG voltage sensing Vsys to Vbat
	 */
	EVT_1_OTP_1,
	EVT_2,
	EVT_3,
};

/*
 * sec_opmode_data - regulator operation mode data
 * @id: regulator id
 * @mode: regulator operation mode
 */
struct sec_opmode_data {
	int id;
	unsigned int mode;
};

/**
 * struct sec_wtsr_smpl - settings for WTSR/SMPL
 * @wtsr_en:		WTSR Function Enable Control
 * @smpl_en:		SMPL Function Enable Control
 * @wtsr_timer_val:	Set the WTSR timer Threshold
 * @smpl_timer_val:	Set the SMPL timer Threshold
 * @check_jigon:	if this value is true, do not enable SMPL function when
 *			JIGONB is low(JIG cable is attached)
 */
struct sec_wtsr_smpl {
	bool wtsr_en;
	bool smpl_en;
	int wtsr_timer_val;
	int smpl_timer_val;
	bool check_jigon;
};

struct s2mpw02_platform_data {
	/* IRQ */
	int	irq_base;
	int	irq_gpio;
	bool	wakeup;

	int	num_regulators;
	struct	s2mpw02_regulator_data *regulators;
	struct	sec_opmode_data		*opmode;
	struct	mfd_cell *sub_devices;
	int	num_subdevs;

	int	device_type;
	int	ono;
	int	buck_ramp_delay;
	bool	dvs_en;

	/* ---- RTC ---- */
	struct	sec_wtsr_smpl *wtsr_smpl;
	struct	rtc_time *init_time;
	int	osc_bias_up;
	int	cap_sel;
	int	osc_xin;
	int	osc_xout;

	bool	use_i2c_speedy;
	bool	cache_data;
};
#endif /* __S2MPW02_MFD_H__ */
