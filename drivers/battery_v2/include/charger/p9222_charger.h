/*
 * p9222_charger.h
 * Samsung p9222 Charger Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __p9222_CHARGER_H
#define __p9222_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include <linux/battery/sec_charging_common.h>

/* REGISTER MAPS */
#define P9222_CHIP_ID_L_REG					0x00
#define P9222_CHIP_ID_H_REG					0x01
#define P9222_CHIP_REVISION_REG				0x02
#define P9222_CUSTOMER_ID_REG				0x03
#define P9222_OTP_FW_MAJOR_REV_L_REG		0x04
#define P9222_OTP_FW_MAJOR_REV_H_REG		0x05
#define P9222_OTP_FW_MINOR_REV_L_REG		0x06
#define P9222_OTP_FW_MINOR_REV_H_REG		0x07
#define P9222_SRAM_FW_MAJOR_REV_L_REG		0x1C
#define P9222_SRAM_FW_MAJOR_REV_H_REG		0x1D
#define P9222_SRAM_FW_MINOR_REV_L_REG		0x1E
#define P9222_SRAM_FW_MINOR_REV_H_REG		0x1F
#define P9222_EPT_REG						0x06
#define P9222_ADC_VRECT_REG					0x07
#define P9222_ADC_IOUT_REG					0x08
#define P9222_ADC_VOUT_REG					0x09
#define P9222_ADC_DIE_TEMP_REG				0x0A
#define P9222_ADC_ALLGN_X_REG				0x0B
#define P9222_ADC_ALIGN_Y_REG				0x0C
#define P9222_INT_STATUS_L_REG				0x34
#define P9222_INT_STATUS_H_REG				0x35
#define P9222_INT_L_REG						0x36
#define P9222_INT_H_REG						0x37
#define P9222_INT_ENABLE_L_REG				0x38
#define P9222_INT_ENABLE_H_REG				0x39
#define P9222_CHG_STATUS_REG				0x3A
#define P9222_END_POWER_TRANSFER_REG		0x3B
#define P9222_ADC_VOUT_L_REG				0x3C
#define P9222_ADC_VOUT_H_REG				0x3D
#define P9222_VOUT_SET_REG					0x3E
#define P9222_VRECT_SET_REG					0x3F
#define P9222_ADC_VRECT_L_REG				0x40
#define P9222_ADC_VRECT_H_REG				0x41
#define P9222_ADC_TX_ISENSE_L_REG			0x42
#define P9222_ADC_TX_ISENSE_H_REG			0x43
#define P9222_ADC_RX_IOUT_L_REG				0x44
#define P9222_ADC_RX_IOUT_H_REG				0x45
#define P9222_ADC_DIE_TEMP_L_REG			0x46
#define P9222_ADC_DIE_TEMP_H_REG			0x47
#define P9222_OP_FREQ_L_REG					0x48
#define P9222_OP_FREQ_H_REG					0x49
#define P9222_ILIM_SET_REG					0x4A
#define P9222_ADC_ALLIGN_X_REG				0x4B
#define P9222_ADC_ALLIGN_Y_REG				0x4C
#define P9222_SYS_OP_MODE_REG				0x4D
#define P9222_COMMAND_REG					0x4E
#define P9222_PACKET_HEADER					0x50
#define P9222_RX_DATA_COMMAND				0x51
#define P9222_RX_DATA_VALUE0				0x52
#define P9222_RX_DATA_VALUE1				0x53
#define P9222_RX_DATA_VALUE2				0x54
#define P9222_RX_DATA_VALUE3				0x55
#define P9222_INT_CLEAR_L_REG				0x56
#define P9222_INT_CLEAR_H_REG				0x57
#define P9222_TX_DATA_COMMAND				0x58
#define P9222_TX_DATA_VALUE0				0x59
#define P9222_TX_DATA_VALUE1				0x5a
#define P9222_RXID_0_REG					0x5C
#define P9222_RXID_1_REG					0x5D
#define P9222_RXID_2_REG					0x5E
#define P9222_RXID_3_REG					0x5F
#define P9222_RXID_4_REG					0x60
#define P9222_RXID_5_REG					0x61
#define P9222_WPC_FOD_0A_REG				0x68
#define P9222_WPC_FLAG_REG					0x80
#define P9222_VBAT_L_REG					0x88
#define P9222_VBAT_H_REG					0x89
#define P9222_ADC_VBAT_L_REG				0x8A
#define P9222_ADC_VBAT_H_REG				0x8B
#define P9222_BATT_VOLTAGE_HEADROOM_L		0x90
#define P9222_BATT_VOLTAGE_HEADROOM_H		0x91
#define P9222_VOUT_LOWER_THRESHOLD			0x92
#define P9222_IDT_BATTERY_MODE				0x96
#define P9222_AP_BATTERY_MODE				0x9A
#define P9222_VRECT_TARGET_VOL_L			0xB0
#define P9222_VRECT_TARGET_VOL_H			0xB1

#define P9222_NUM_FOD_REG					12

#define P9222_CHIP_ID_MAJOR_1_REG			0x0070
#define P9222_CHIP_ID_MAJOR_0_REG			0x0074
#define P9222_CHIP_ID_MINOR_REG				0x0078
#define P9222_RAW_IOUT_L_REG				0x007A
#define P9222_RAW_IOUT_H_REG				0x007B

#define P9222_LDO_EN_REG					0x301c

#define P9222_AP_BATTERY_IDT_MODE					0x00
#define P9222_AP_BATTERY_BOOT_MODE					0x01
/* P9222_AP_BATTERY_BATT_MODE Vout = Vbat + 0.3V Vrect = Vout + 0x1V */
#define P9222_AP_BATTERY_BATT_MODE					0x02
#define P9222_AP_BATTERY_CC_CV_MODE					0x06
#define P9222_AP_BATTERY_INCOMPATIBLE_CC_CV_MODE	0x07
#define P9222_AP_BATTERY_CC_MODE					0x10
#define P9222_AP_BATTERY_INCOMPATIBLE_CC_MODE		0x11
#define P9222_AP_BATTERY_WAKEUP_MODE				0x12
#define P9222_AP_BATTERY_INCOMPATIBLE_PHP_MODE		0x13
#define P9222_AP_BATTERY_CURR_MEASURE_MODE			0xF0



/* Chip Revision and Font Register, Chip_Rev (0x02) */
#define P9222_CHIP_GRADE_MASK				0x0f
#define P9222_CHIP_REVISION_MASK			0xf0

/* Status Registers, Status_L (0x34), Status_H (0x35) */
#define P9222_STAT_VOUT_SHIFT				7
#define P9222_STAT_STAT_VRECT_SHIFT			6
#define P9222_STAT_MODE_CHANGE_SHIFT		5
#define P9222_STAT_TX_DATA_RECEIVED_SHIFT	4
#define P9222_STAT_OVER_TEMP_SHIFT			2
#define P9222_STAT_OVER_VOL_SHIFT			1
#define P9222_STAT_OVER_CURR_SHIFT			0
#define P9222_STAT_VOUT_MASK				(1 << P9222_STAT_VOUT_SHIFT)
#define P9222_STAT_STAT_VRECT_MASK			(1 << P9222_STAT_STAT_VRECT_SHIFT)
#define P9222_STAT_MODE_CHANGE_MASK			(1 << P9222_STAT_MODE_CHANGE_SHIFT)
#define P9222_STAT_TX_DATA_RECEIVED_MASK	(1 << P9222_STAT_TX_DATA_RECEIVED_SHIFT)
#define P9222_STAT_OVER_TEMP_MASK			(1 << P9222_STAT_OVER_TEMP_SHIFT)
#define P9222_STAT_OVER_VOL_MASK			(1 << P9222_STAT_OVER_VOL_SHIFT)
#define P9222_STAT_OVER_CURR_MASK			(1 << P9222_STAT_OVER_CURR_SHIFT)

#define P9222_STAT_TX_OVER_CURR_SHIFT		6
#define P9222_STAT_TX_OVER_TEMP_SHIFT		5
#define P9222_STAT_TX_FOD_SHIFT				4
#define P9222_STAT_TX_CONNECT_SHIFT			3

#define P9222_STAT_TX_OVER_CURR_MASK		(1 << P9222_STAT_TX_OVER_CURR_SHIFT)
#define P9222_STAT_TX_OVER_TEMP_MASK		(1 << P9222_STAT_TX_OVER_TEMP_SHIFT)
#define P9222_STAT_TX_FOD_MASK				(1 << P9222_STAT_TX_FOD_SHIFT)
#define P9222_STAT_TX_CONNECT_MASK			(1 << P9222_STAT_TX_CONNECT_SHIFT)

/* Interrupt Registers, INT_L (0x36), INT_H (0x37) */
#define P9222_INT_STAT_VOUT					P9222_STAT_VOUT_MASK
#define P9222_INT_STAT_VRECT				P9222_STAT_STAT_VRECT_MASK
#define P9222_INT_MODE_CHANGE				P9222_STAT_MODE_CHANGE_MASK
#define P9222_INT_TX_DATA_RECEIVED			P9222_STAT_TX_DATA_RECEIVED_MASK
#define P9222_INT_OVER_VOLT					P9222_STAT_OVER_VOL_MASK
#define P9222_INT_OVER_CURR					P9222_STAT_OVER_CURR_MASK

#define P9222_INT_OVER_TEMP					P9222_STAT_OVER_TEMP_MASK
#define P9222_INT_TX_OVER_CURR				P9222_STAT_TX_OVER_CURR_MASK
#define P9222_INT_TX_OVER_TEMP				P9222_STAT_TX_OVER_TEMP_MASK
#define P9222_INT_TX_FOD					P9222_STAT_TX_FOD_MASK
#define P9222_INT_TX_CONNECT				P9222_STAT_TX_CONNECT_MASK

/* End of Power Transfer Register, EPT (0x3B) (RX only) */
#define P9222_EPT_UNKNOWN					0
#define P9222_EPT_END_OF_CHG				1
#define P9222_EPT_INT_FAULT					2
#define P9222_EPT_OVER_TEMP					3
#define P9222_EPT_OVER_VOL					4
#define P9222_EPT_OVER_CURR					5
#define P9222_EPT_BATT_FAIL					6
#define P9222_EPT_RECONFIG					7

/* System Operating Mode Register,Sys_Op_Mode (0x4D) */
#define P9222_SYS_MODE_INIT					0
#define P9222_SYS_MODE_WPC					1
#define P9222_SYS_MODE_PMA					2
#define P9222_SYS_MODE_MISSING_BACK			3
#define P9222_SYS_MODE_TX					4
#define P9222_SYS_MODE_WPC_RX				5
#define P9222_SYS_MODE_PMA_RX				6
#define P9222_SYS_MODE_MISSING				7


/* Command Register, COM(0x4E) */
#define P9222_CMD_SS_WATCHDOG_SHIFT			7
#define P9222_CMD_CLEAR_INT_SHIFT			5
#define P9222_CMD_SEND_CHG_STS_SHIFT		4
#define P9222_CMD_SEND_EOP_SHIFT			3
#define P9222_CMD_SET_TX_MODE_SHIFT			2
#define P9222_CMD_TOGGLE_LDO_SHIFT			1
#define P9222_CMD_SEND_RX_DATA_SHIFT		0
#define P9222_CMD_SS_WATCHDOG_MASK			(1 << P9222_CMD_SS_WATCHDOG_SHIFT)
#define P9222_CMD_CLEAR_INT_MASK			(1 << P9222_CMD_CLEAR_INT_SHIFT)
#define P9222_CMD_SEND_CHG_STS_MASK			(1 << P9222_CMD_SEND_CHG_STS_SHIFT)
#define P9222_CMD_SEND_EOP_MASK				(1 << P9222_CMD_SEND_EOP_SHIFT)
#define P9222_CMD_SET_TX_MODE_MASK			(1 << P9222_CMD_SET_TX_MODE_SHIFT)
#define P9222_CMD_TOGGLE_LDO_MASK			(1 << P9222_CMD_TOGGLE_LDO_SHIFT)
#define P9222_CMD_SEND_RX_DATA_MASK			(1 << P9222_CMD_SEND_RX_DATA_SHIFT)

#define P9222_CMD_SEND_RX_DATA				P9222_CMD_SEND_RX_DATA_MASK

/* Proprietary Packet Header Register, PPP_Header(0x50) */
#define P9222_HEADER_END_SIG_STRENGTH		0x01
#define P9222_HEADER_END_POWER_TRANSFER		0x02
#define P9222_HEADER_END_CTR_ERROR			0x03
#define P9222_HEADER_END_RECEIVED_POWER		0x04
#define P9222_HEADER_END_CHARGE_STATUS		0x05
#define P9222_HEADER_POWER_CTR_HOLD_OFF		0x06
#define P9222_HEADER_AFC_CONF				0x28
#define P9222_HEADER_CONFIGURATION			0x51
#define P9222_HEADER_IDENTIFICATION			0x71
#define P9222_HEADER_EXTENDED_IDENT			0x81

/* RX Data Command Register, RX Data_COM (0x51) */
#define P9222_RX_DATA_COM_UNKNOWN			0x00
#define P9222_RX_DATA_COM_REQ_TX_ID			0x01
#define P9222_RX_DATA_COM_CHG_STATUS		0x05
#define P9222_RX_DATA_COM_AFC_SET			0x06
#define P9222_RX_DATA_COM_AFC_DEBOUCE		0x07
#define P9222_RX_DATA_COM_SID_TAG			0x08
#define P9222_RX_DATA_COM_SID_TOKEN			0x09
#define P9222_RX_DATA_COM_TX_STANBY			0x0a
#define P9222_RX_DATA_COM_LED_CTRL			0x0b
#define P9222_RX_DATA_COM_REQ_AFC			0x0c
#define P9222_RX_DATA_COM_FAN_CTRL			0x0d

/* TX Data Command Register, TX Data_COM (0x58) */
#define P9222_TX_DATA_COM_UNKNOWN			0x00
#define P9222_TX_DATA_COM_TX_ID				0x01
#define P9222_TX_DATA_COM_AFC_TX			0x02
#define P9222_TX_DATA_COM_ACK				0x03
#define P9222_TX_DATA_COM_NAK				0x04

/* Flag Register, (0x80) */
#define P9222_WATCHDOG_DIS_SHIFT			5
#define P9222_WATCHDOG_DIS_MASK			(1 << P9222_WATCHDOG_DIS_SHIFT)
#define P9222_VBAT_MONITORING_MODE_SHIFT	7
#define P9222_VBAT_MONITORING_MODE_MASK		(1 << P9222_VBAT_MONITORING_MODE_SHIFT)

#define P9222_VBAT_MONITORING_MODE			P9222_VBAT_MONITORING_MODE_MASK

/* IDT Battery Mode Register(0x96) */
#define P9222_IDT_BATTERY_INITIAL_MODE		0x01

/* END POWER TRANSFER CODES IN WPC */
#define P9222_EPT_CODE_UNKOWN				0x00
#define P9222_EPT_CODE_CHARGE_COMPLETE		0x01
#define P9222_EPT_CODE_INTERNAL_FAULT		0x02
#define P9222_EPT_CODE_OVER_TEMPERATURE		0x03
#define P9222_EPT_CODE_OVER_VOLTAGE			0x04
#define P9222_EPT_CODE_OVER_CURRENT			0x05
#define P9222_EPT_CODE_BATTERY_FAILURE		0x06
#define P9222_EPT_CODE_RECONFIGURE			0x07
#define P9222_EPT_CODE_NO_RESPONSE			0x08

#define P9222_POWER_MODE_MASK				(0x1 << 0)
#define P9222_SEND_USER_PKT_DONE_MASK		(0x1 << 7)
#define P9222_SEND_USER_PKT_ERR_MASK		(0x3 << 5)
#define P9222_SEND_ALIGN_MASK				(0x1 << 3)
#define P9222_SEND_EPT_CC_MASK				(0x1 << 0)
#define P9222_SEND_EOC_MASK					(0x1 << 0)

#define P9222_PTK_ERR_NO_ERR				0x00
#define P9222_PTK_ERR_ERR					0x01
#define P9222_PTK_ERR_ILLEGAL_HD			0x02
#define P9222_PTK_ERR_NO_DEF				0x03

#define P9222_VOUT_5V_VAL					0x0f
#define P9222_VOUT_6V_VAL					0x19
#define P9222_VOUT_7V_VAL					0x23
#define P9222_VOUT_8V_VAL					0x2d
#define P9222_VOUT_9V_VAL					0x37

#define P9222_VOUT_TO_VAL(x)				((x - 3500) / 100)

#define P9222_FW_RESULT_DOWNLOADING			2
#define P9222_FW_RESULT_PASS				1
#define P9222_FW_RESULT_FAIL				0

#define  VOUT_LOWER_4_2V        0x07
#define  VOUT_LOWER_4_3V        0x08
#define  VOUT_LOWER_4_4V        0x09
#define  VOUT_LOWER_4_5V        0x0A
#define  VOUT_LOWER_4_6V	0x0B
#define  VOUT_LOWER_4_7V	0x0C
#define  VOUT_LOWER_4_8V	0x0D


enum {
    P9222_EVENT_IRQ = 0,
    P9222_IRQS_NR,
};

enum {
    P9222_PAD_MODE_NONE = 0,
    P9222_PAD_MODE_WPC,
    P9222_PAD_MODE_WPC_AFC,
    P9222_PAD_MODE_PMA,
    P9222_PAD_MODE_TX,
};

/* vout settings */
enum {
    P9222_VOUT_0V = 0,
    P9222_VOUT_5V,
    P9222_VOUT_6V,
    P9222_VOUT_9V,
    P9222_VOUT_CV_CALL,
    P9222_VOUT_CC_CALL,
    P9222_VOUT_9V_STEP,
};

enum {
    P9222_ADC_VOUT = 0,
    P9222_ADC_VRECT,
    P9222_ADC_TX_ISENSE,
    P9222_ADC_RX_IOUT,
    P9222_ADC_DIE_TEMP,
    P9222_ADC_ALLIGN_X,
    P9222_ADC_ALLIGN_Y,
    P9222_ADC_OP_FRQ,
    P9222_ADC_VBAT_RAW,
    P9222_ADC_VBAT,
};

enum {
	P9222_END_SIG_STRENGTH = 0,
	P9222_END_POWER_TRANSFER,
	P9222_END_CTR_ERROR,
	P9222_END_RECEIVED_POWER,
	P9222_END_CHARGE_STATUS,
	P9222_POWER_CTR_HOLD_OFF,
	P9222_AFC_CONF_5V,
	P9222_AFC_CONF_9V,
	P9222_CONFIGURATION,
	P9222_IDENTIFICATION,
	P9222_EXTENDED_IDENT,
	P9222_LED_CONTROL_ON,
	P9222_LED_CONTROL_OFF,
	P9222_FAN_CONTROL_ON,
	P9222_FAN_CONTROL_OFF,
	P9222_REQUEST_AFC_TX,
};

enum {
	P9222_WATCHDOG = 0,
	P9222_EOP_HIGH_TEMP,
	P9222_VOUT_ON,
	P9222_VOUT_OFF,
};

enum p9222_irq_source {
	TOP_INT = 0,
};

enum p9222_chip_rev {
	P9222_A_GRADE_IC = 0,
	P9222_B_GRADE_IC,
	P9222_C_GRADE_IC,
	P9222_D_GRADE_IC,
};

enum p9222_irq {
	P9222_IRQ_STAT_VOUT = 0,
	P9222_IRQ_STAT_VRECT,
	P9222_IRQ_MODE_CHANGE,
	P9222_IRQ_TX_DATA_RECEIVED,
	P9222_IRQ_OVER_VOLT,
	P9222_IRQ_OVER_CURR,
	P9222_IRQ_OVER_TEMP,
	P9222_IRQ_TX_OVER_CURR,
	P9222_IRQ_TX_OVER_TEMP,
	P9222_IRQ_TX_FOD,
	P9222_IRQ_TX_CONNECT,
	P9222_IRQ_NR,
};

struct p9222_irq_data {
	int mask;
	enum p9222_irq_source group;
};

enum p9222_firmware_mode {
	P9222_RX_FIRMWARE = 0,
	P9222_TX_FIRMWARE,
};

enum p9222_read_mode {
	P9222_IC_GRADE = 0,
	P9222_IC_VERSION,
	P9222_IC_VENDOR,
};

enum p9222_headroom {
	P9222_HEADROOM_0 = 0,
	P9222_HEADROOM_1, /* 0.277V */
	P9222_HEADROOM_2, /* 0.497V */
	P9222_HEADROOM_3, /* 0.650V */
};

enum p9222_charger_pad_type {
	P9222_CHARGER_TYPE_COMPATIBLE = 0,
	P9222_CHARGER_TYPE_INCOMPATIBLE,
	P9222_CHARGER_TYPE_MULTI,
	P9222_CHARGER_TYPE_NUM_MAX
};

enum p9222_charge_mode_type {
	P9222_CHARGE_MODE_NONE = 0,
	P9222_CHARGE_MODE_CC,
	P9222_CHARGE_MODE_CV,
	P9222_CHARGE_MODE_OFF
};


enum sec_wpc_setting_mode {
	SEC_INITIAL_MODE =0,
	SEC_SWELLING_MODE,
	SEC_LOW_BATT_MODE,
	SEC_MID_BATT_MODE,
	SEC_HIGH_BATT_MODE,
	SEC_VERY_HIGH_BATT_MODE,
	SEC_FULL_NONE_MODE,
	SEC_FULL_NORMAL_MODE,
	SEC_SWELLING_LDU_MODE,
	SEC_LOW_BATT_LDU_MODE,
	SEC_FULL_NONE_LDU_MODE,
	SEC_FULL_NORMAL_LDU_MODE,
	SEC_INCOMPATIBLE_TX_MODE,
	SEC_VBAT_MONITORING_DISABLE_MODE,
	SEC_INCOMPATIBLE_TX_HIGH_BATT_MODE,
	SEC_INCOMPATIBLE_TX_FULL_MODE,
	SEC_WPC_MODE_NUM,
};

/* for mesurement current leakage */
#define P9222_FACTORY_MODE_TX_ID			0x5F

struct p9222_ppp_info {
	u8 header;
	u8 rx_data_com;
	u8 data_val[4];
	int data_size;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct p9222_irq_data p9222_irqs[] = {
	DECLARE_IRQ(P9222_IRQ_STAT_VOUT,	TOP_INT, 1 << 0),
};

struct p9222_sec_mode_config_data {
	int vrect;
	int vout;
};

struct p9222_charger_type {
	int compatible_tx_id;
	int support_power_hold;
};

struct p9222_chg_type_desc;

struct p9222_charger_platform_data {
	char *wireless_charger_name;
	char *charger_name;
	char *fuelgauge_name;
	char *wireless_name;
	char *battery_name;

	int wpc_det;
	int irq_wpc_det;
	int wpc_int;
	int irq_wpc_int;
	u32 irq_gpio_flags;
	int wpc_en;
	int cc_cv_threshold; /* cc/cv threshold capacity level */
	int cs100_status;
	int vout_status;
	int wireless_cc_cv;
	int siop_level;
	int cable_type;
	bool default_voreg;
	int is_charging;
	int *fod_data;
	int fod_data_check;
	bool ic_on_mode;
	int hw_rev_changed; /* this is only for noble/zero2 */
	int on_mst_wa;		/* this is only for Zero2, There is zinitix leakage */
	int otp_firmware_result;
	int tx_firmware_result;
	int wc_ic_grade;
	int wc_ic_rev;
	int otp_firmware_ver;
	int tx_firmware_ver;
	int vout;
	int vrect;
	int tx_status;
	int wpc_cv_call_vout;
	int wpc_cc_call_vout;
	int opfq_cnt;
	bool watchdog_test;
	unsigned int charging_mode;

	bool can_vbat_monitoring;
	struct p9222_sec_mode_config_data *sec_mode_config_data;
	int num_sec_mode;
	int num_compatible_tx;
	//int *compatible_tx_id;
	struct p9222_charger_type *charger_type;

	int tx_off_high_temp;
	int ping_duration;
};

#define p9222_charger_platform_data_t \
	struct p9222_charger_platform_data

struct p9222_charger_data {
	struct i2c_client				*client;
	struct device					*dev;
	p9222_charger_platform_data_t 	*pdata;
	struct mutex io_lock;
	struct mutex charger_lock;
	const struct firmware *firm_data_bin;

	int wc_w_state;

	struct power_supply *psy_chg;
	struct wake_lock wpc_wake_lock;
	struct wake_lock wpc_vbat_monitoring_enable_lock;
	struct wake_lock wpc_opfq_lock;
	struct wake_lock wpc_auth_check_lock;
	struct workqueue_struct *wqueue;
	struct delayed_work	wpc_det_work;
	struct delayed_work	wpc_opfq_work;
	struct delayed_work	wpc_isr_work;
	struct delayed_work wpc_init_work;
	struct delayed_work wpc_auth_check_work;
	struct delayed_work	curr_measure_work;
	struct delayed_work wpc_detach_chk_work;

	struct wake_lock wpc_id_request_lock;
	struct delayed_work	wpc_id_request_work;
	int wpc_id_request_step;

	struct wake_lock power_hold_chk_lock;
	struct delayed_work power_hold_chk_work;

	struct wake_lock wpc_wait_vout_lock;
	struct delayed_work	wpc_wait_vout_work;

	struct alarm polling_alarm;
	ktime_t last_poll_time;

	struct	notifier_block wpc_nb;

	struct wake_lock wpc_wakeup_wa_lock;
	struct delayed_work	wpc_wakeup_wa_work;
	int wpc_wakeup_wa;

	u16 addr;
	int size;

	int temperature;

	/* sec_battery's battery health */
	int battery_health;

	/* sec_battery's battery status */
	int battery_status;

	/* sec_battery's charge mode */
	int battery_chg_mode;

	/* sec_battery's swelling mode */
	int swelling_mode;

	/* sec_battery's slate(TCT) mode */
	int slate_mode;

	/* sec_battery's store(LDU) mode */
	int store_mode;

	/* check is recharge status */
	int is_recharge;

	/* check power hold status */
	int power_hold_mode;

	/* if set 1, need more sleep marging before ap_mode setting */
	int need_margin;

	/* check power hold status */
	int support_power_hold;

	/* check incompatible tx pad, incompatible tx pad is limited chg currnet */
	int incompatible_tx;

	/* if tx id is 0xF0, p9222 set to current mesurement mode */
	int curr_measure_mode;

	/* Vout(ACOK) status */
	int vout_status;

	/* TX PAD id */
	int tx_id;

	/* TX id checked twice, if tx id checked normarlly, skip second tx id check */
	int tx_id_checked;

	/* charge mode cc/cv mode */
	enum p9222_charge_mode_type charge_mode;

	enum p9222_charger_pad_type charger_type;
	struct p9222_chg_type_desc* charge_cb_func[P9222_CHARGER_TYPE_NUM_MAX];
};

struct p9222_chg_type_desc {
	enum p9222_charger_pad_type charger_type;
	char* charger_type_name;
	int (*cc_chg)(struct p9222_charger_data *charger, bool prev_chk);
	int (*cv_chg)(struct p9222_charger_data *charger, bool prev_chk);
	int (*full_chg)(struct p9222_charger_data *charger);
	int (*re_chg)(struct p9222_charger_data *charger);
};

ssize_t sec_wpc_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t sec_wpc_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SEC_WPC_ATTR(_name)						\
{									\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = sec_wpc_show_attrs,					\
	.store = sec_wpc_store_attrs,					\
}

#endif /* __p9222_CHARGER_H */
