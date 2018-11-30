/* drivers/battery_2/s2mpw02_charger.c
 * S2MPW02 Charger Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/mfd/samsung/s2mpw02.h>
#include <linux/mfd/samsung/s2mpw02-regulator.h>
#include <linux/power/s2mpw02_charger.h>
#include <linux/version.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/wakelock.h>
#include <linux/sort.h>
#include <../drivers/battery_v2/include/sec_charging_common.h>
#include <linux/mfd/samsung/rtc-s2mp.h>
#include <linux/soc/samsung/exynos-regs-pmu.h>
#include <soc/samsung/exynos-pmu.h>


#define ENABLE_MIVR 1
#define MINVAL(a, b) ((a <= b) ? a : b)
#define HEALTH_DEBOUNCE_CNT 3

#ifndef EN_TEST_READ
#define EN_TEST_READ 1
#endif

struct s2mpw02_additional_curr {
	u8 fast_chg_reg_val;
	u8 add_chg_reg_val;
	u8 additional_chg_set;
	int chg_current;
};

struct s2mpw02_charger_log_data {
	u8	reg_data[32];
	int chg_status;
	int health;
};

struct s2mpw02_charger_data {
	struct s2mpw02_dev	*iodev;
	struct i2c_client       *client;
	struct device *dev;
	struct s2mpw02_platform_data *s2mpw02_pdata;
	struct delayed_work	charger_work;
	struct delayed_work init_work;
	struct delayed_work usb_work;
	struct delayed_work rid_work;
	struct delayed_work acok_work;

	struct workqueue_struct *charger_wqueue;
	struct power_supply	*psy_chg;
	struct power_supply_desc	psy_chg_desc;
	s2mpw02_charger_platform_data_t *pdata;
	int dev_id;
	int charging_current;
	int topoff_current;
	int charge_mode;
	int cable_type;
	bool is_charging;
	bool is_usb_ready;
	struct mutex io_lock;

	/* register programming */
	int reg_addr;
	int reg_data;

	bool full_charged;
	bool ovp;
	bool factory_mode;

	int unhealth_cnt;
	int status;
	int onoff;

	/* s2mpw02 */
	int irq_det_bat;
	int irq_chg;
	int irq_tmrout;

	int irq_uart_off;
	int irq_uart_on;
	int irq_usb_off;
	int irq_usb_on;
	int irq_uart_cable;
	int irq_fact_leakage;
	int irq_jigon;
	int irq_acokf;
	int irq_acokr;
	int irq_rid_attach;
#if defined(CONFIG_MUIC_NOTIFIER)
	muic_attached_dev_t	muic_dev;
#endif
	struct s2mpw02_additional_curr *chg_curr_table;
	int table_size;
	struct s2mpw02_charger_log_data log_data;
};

static enum power_supply_property s2mpw02_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
};

static int s2mpw02_get_charging_health(struct s2mpw02_charger_data *charger);
static void s2mpw02_topoff_status_setting(struct s2mpw02_charger_data *charger);
static void s2mpw02_muic_detect_handler(struct s2mpw02_charger_data *charger, bool is_on);
static void s2mpw02_set_additional(struct s2mpw02_charger_data *charger, int n, int onoff);
static int
s2mpw02_charge_table_search(struct s2mpw02_charger_data *charger, int chg_current);
static void s2mpw02_muic_attach_dev(
	struct s2mpw02_charger_data *charger, muic_attached_dev_t muic_dev);

static char *s2mpw02_supplied_to[] = {
	"s2mpw02-charger",
};

static void s2mpw02_test_read(struct s2mpw02_charger_data *charger)
{
	static int cnt = 0;
	u8 data;
	char str[256] = {0,};
	int i;
	bool print_log = false;

	for (i = S2MPW02_CHG_REG_INT1M; i <= S2MPW02_CHG_REG_CTRL12; i++) {
		s2mpw02_read_reg(charger->client, i, &data);
		if (data != charger->log_data.reg_data[i])
			print_log = true;
		charger->log_data.reg_data[i] = data;
		sprintf(str+strlen(str), "%x:%02x ", i, data);
	}

	if (!print_log) {
		if (cnt++ > 10)
			print_log = true;
	}

	if (print_log) {
		pr_info("%s: %s\n", __func__, str);
		cnt = 0;
	}
}

static void s2mpw02_enable_charger_switch(struct s2mpw02_charger_data *charger,
		int onoff)
{
	if (onoff > 0) {
		pr_info("%s: turn on charger\n", __func__);
		s2mpw02_update_reg(charger->client, S2MPW02_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);

		/* if set S2MPW02_CHG_REG_CHG_OTP12 to 0x8A, Q4 FET set depend on ACOK.
		 * ACOK on use small FET, ACOK off use small & big FET
		 */
		s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP12, 0x8A);
	} else {
		charger->full_charged = false;
		if (!charger->factory_mode) {
			pr_info("%s: turn off charger\n", __func__);

			/* if set S2MPW02_CHG_REG_CHG_OTP12 to 0x8B Q4 FET use small & big.
			 * if S2MPW02_CHG_REG_CHG_OTP12 is 0x8A while discharging,
			 * Vsys can drop when heavy load is occured.
			 */
			s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP12, 0x8B);
			s2mpw02_update_reg(charger->client, S2MPW02_CHG_REG_CTRL1, 0, EN_CHG_MASK);
		}
	}
}

static void s2mpw02_set_regulation_voltage(struct s2mpw02_charger_data *charger,
		int float_voltage)
{
	unsigned int data;

	pr_info("%s: float_voltage %d\n", __func__, float_voltage);

	if (float_voltage <= 4050)
		data = 0x0;
	else if (float_voltage > 4050 && float_voltage <= 4450)
		data = (int)((float_voltage - 4050) * 10 / 125);
	else
		data = 0x1F;

	s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL2, data << CV_SEL_SHIFT, CV_SEL_MASK);
}

static void s2mpw02_set_fast_charging_current(struct s2mpw02_charger_data *charger,
		int charging_current)
{
	int data;
	int ret;
	u8 add_chg, add_chg_reg_val;
	int chg_current;
	struct i2c_client *client = charger->client;

	ret = s2mpw02_charge_table_search(charger, charging_current);
	data = charger->chg_curr_table[ret].fast_chg_reg_val;
	add_chg = charger->chg_curr_table[ret].additional_chg_set;
	add_chg_reg_val = charger->chg_curr_table[ret].add_chg_reg_val;
	chg_current = charger->chg_curr_table[ret].chg_current;

	pr_info("%s: fast charge current  %d (%d %d %d %d)\n",
		__func__, charging_current, data, add_chg, add_chg_reg_val, chg_current);

	s2mpw02_update_reg(client, S2MPW02_CHG_REG_CTRL2, data << CC_SEL_SHIFT,
			CC_SEL_MASK);

	if (add_chg)
		s2mpw02_set_additional(charger, add_chg_reg_val, 1);
	else
		s2mpw02_set_additional(charger, 0, 0);
}

static int fast_charging_current[] = {30, 75, 150, 175, 200, 250, 300, 350};

static int s2mpw02_get_fast_charging_current(struct s2mpw02_charger_data *charger)
{
	int ret;
	u8 data, data2;

	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_CTRL2, &data);
	if (ret < 0)
		return ret;

	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_CTRL3, &data2);
	data = (data & CC_SEL_MASK) >> CC_SEL_SHIFT;
	ret = fast_charging_current[data];

	if (data2 & FORCED_ADD_ON_MASK) {
		if (charger->dev_id < EVT_1) {
			s2mpw02_read_reg(charger->client, S2MPW02_CHG_ADD_CURR, &data2);
			data2 = (data2 & T_ADD_MASK) >> T_ADD_SHIFT;
		} else {
			s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_CTRL4, &data2);
			data2 = (data2 & T_ADD_EVT1_MASK) >> T_ADD_EVT1_SHIFT;
		}

		ret += ret * (data2 / 8);
	}
	pr_debug("%s = %d\n", __func__, ret);
	return ret;
}

int eoc_current[16] = {
	10, 12, 15, 17, 20, 22, 25, 27, 30, 32, 35, 40, 45, 50, 55, 60};

static int s2mpw02_get_current_eoc_setting(struct s2mpw02_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_CTRL5, &data);
	if (ret < 0)
		return ret;

	data = (data & EOC_I_SEL_MASK) >> EOC_I_SEL_SHIFT;

	if (data > 0xf)
		data = 0xf;

	pr_info("%s: data(0x%x), top-off current	%d\n",
		__func__, data, eoc_current[data]);

	return eoc_current[data];
}

static void s2mpw02_set_topoff_current(struct i2c_client *i2c, int current_limit)
{
	int data;
	int i;
	int len = sizeof(eoc_current)/sizeof(int);

	for (i = 0; i < len; i++) {
		if (current_limit <= eoc_current[i]) {
			data = i;
			break;
		}
	}

	if (i >= len)
		data = len - 1;

	pr_err("%s: top-off current	%d data=0x%x\n", __func__, current_limit, data);

	s2mpw02_update_reg(i2c, S2MPW02_CHG_REG_CTRL5, data << EOC_I_SEL_SHIFT,
			EOC_I_SEL_MASK);
}

static void s2mpw02_set_additional(struct s2mpw02_charger_data *charger, int n, int onoff)
{
	pr_info("%s: n (%d), onoff (%d)\n", __func__, n, onoff);

	if (onoff == 1) {
		/* Apply additional charging current */
		if (charger->dev_id < EVT_1) {
			s2mpw02_update_reg(charger->client,
				S2MPW02_CHG_ADD_CURR, n << T_ADD_SHIFT, T_ADD_MASK);
		} else {
			s2mpw02_update_reg(charger->client,
				S2MPW02_CHG_REG_CTRL4, n << T_ADD_EVT1_SHIFT, T_ADD_EVT1_MASK);
		}

		/* Additional charging path On */
		s2mpw02_update_reg(charger->client,
		S2MPW02_CHG_REG_CTRL3, FORCED_ADD_ON_MASK, FORCED_ADD_ON_MASK);

	} else if (onoff == 0) {
		/* Additional charging path Off */
		s2mpw02_update_reg(charger->client,
		S2MPW02_CHG_REG_CTRL3, 0 << FORCED_ADD_ON_SHIFT, FORCED_ADD_ON_MASK);

		/* Restore addition charging current */
		if (charger->dev_id < EVT_1) {
			s2mpw02_update_reg(charger->client,
				S2MPW02_CHG_ADD_CURR, n << T_ADD_SHIFT, T_ADD_MASK);
		} else {
			s2mpw02_update_reg(charger->client,
				S2MPW02_CHG_REG_CTRL4, n << T_ADD_EVT1_SHIFT, T_ADD_EVT1_MASK);
		}
	}
}

static void s2mpw02_set_forced_jig_mode(struct s2mpw02_charger_data *charger)
{
	/* set forced jig mode to measure current */
	s2mpw02_update_reg(charger->client,
		S2MPW02_CHG_REG_CTRL3, FORCED_JIG_MODE_MASK, FORCED_JIG_MODE_MASK);
}

enum {
	S2MPW02_MIVR_4200MV = 0,
	S2MPW02_MIVR_4400MV,
	S2MPW02_MIVR_4600MV,
	S2MPW02_MIVR_4800MV,
};

static void
s2mpw02_set_bypass_mode(struct s2mpw02_charger_data *charger, int rid, int mode)
{
	u8 data1, data2;

	if (rid > CHG_RID_CTRL_MAX_NUM) {
		pr_err("%s: invalid rid (%d)\n", __func__, rid);
		return;
	}

	if (mode > CHG_MODE_MAX_NUM) {
		pr_err("%s: invalid mode (%d)\n", __func__, mode);
		return;
	}

	pr_info("%s: rid (%d)\n", __func__, rid);

	switch (mode) {
	case CHG_MODE_NORMAL:
		s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL9, BIT(rid), BIT(rid));
		s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL10, BIT(0), BIT(rid));
		break;
	case CHG_MODE_BYPASS:
		s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL9, BIT(0), BIT(rid));
		s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL10, BIT(rid), BIT(rid));
		break;
	case CHG_MODE_REGULATION:
		s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL9, BIT(0), BIT(rid));
		s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL10, BIT(0), BIT(rid));
		break;
	default:
		break;
	}

	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_CTRL9, &data1);
	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_CTRL10, &data2);
	pr_info("%s: rid ctrl1(%02x) ctrl2(%02x)\n", __func__, data1, data2);
}


#if ENABLE_MIVR
/* charger input regulation voltage setting */
static void s2mpw02_set_mivr_level(struct s2mpw02_charger_data *charger)
{
	int mivr = S2MPW02_MIVR_4600MV;

	s2mpw02_update_reg(charger->client,
			S2MPW02_CHG_REG_CTRL5, mivr << IVR_V_SEL_SHIFT, IVR_V_SEL_MASK);
}
#endif /*ENABLE_MIVR*/

/* here is set init charger data */
static bool s2mpw02_chg_init(struct s2mpw02_charger_data *charger)
{
	dev_info(&charger->client->dev, "%s : DEV ID : 0x%x\n", __func__,
			charger->dev_id);

	/* Factory_mode initialization */
	charger->factory_mode = false;

	if (charger->dev_id < EVT_1) {
		/* change Top-off detection debounce time to max 0x25[3:2]=11 */
		s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP4, 0x3D);
	} else {
		/* S2MPW02_CHG_REG_CHG_OTP4 is write only reg. so update reg is not valid.
		 * default top off debounce time is 1536ms(0xFD)
		 * change Top-off detection debounce time to 192ms 0x25[3:2]=00
		 */
		s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP4, 0xF1);
	}

	/* Top-off Timer Disable 0x16[6]=1 */
	s2mpw02_update_reg(charger->client, S2MPW02_CHG_REG_CTRL11,
		NO_TIMEOUT_30M_TM_MASK, NO_TIMEOUT_30M_TM_MASK);

	/* Watchdog timer disable */
	s2mpw02_update_reg(charger->client, S2MPW02_CHG_REG_CTRL8, 0x00, WDT_EN_MASK);

	/* Manual reset enable */
	s2mpw02_update_reg(charger->client, S2MPW02_CHG_REG_CTRL7, MRST_EN_MASK, MRST_EN_MASK);

	/* RID detect debounce time : 200msec -> 100msec */
	s2mpw02_update_reg(charger->client, S2MPW02_CHG_REG_CTRL9, 0x00, TDB_RID_MASK);

	/* RID detect debounce time : 200msec -> 100msec */
	s2mpw02_update_reg(charger->client, 0x14, 0x00, 0xC0);

	return true;
}

static int s2mpw02_get_charging_status(struct s2mpw02_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	u8 chg_sts, pmic_sts;

	s2mpw02_topoff_status_setting(charger);

	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS1, &chg_sts);
	if (ret < 0)
		return status;
	ret = s2mpw02_read_reg(charger->iodev->pmic, S2MPW02_PMIC_REG_STATUS1, &pmic_sts);
	dev_info(charger->dev, "%s : charger status : 0x%x, pmic status : 0x%x\n",
			__func__, chg_sts, pmic_sts);

	if (charger->full_charged) {
		dev_info(charger->dev, "%s POWER_SUPPLY_STATUS_FULL(0x%x)\n", __func__, chg_sts);
		return POWER_SUPPLY_STATUS_FULL;
	}

	if ((pmic_sts & ACOK_STATUS_MASK) && (chg_sts & CIN2BAT_STATUS_MASK) &&
		(chg_sts & CHGSTS_STATUS_MASK) && !(chg_sts & CHG_DONE_STATUS_MASK))
		status = POWER_SUPPLY_STATUS_CHARGING;
	else if ((pmic_sts & ACOK_STATUS_MASK) && (chg_sts & CIN2BAT_STATUS_MASK) &&
		!(chg_sts & CHGSTS_STATUS_MASK) && !(chg_sts & CHG_DONE_STATUS_MASK))
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else if ((chg_sts & CHGSTS_STATUS_MASK) && (chg_sts & CHG_DONE_STATUS_MASK))
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;

	return status;
}

static int s2mpw02_get_charge_type(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;
	u8 data;

	ret = s2mpw02_read_reg(iic, S2MPW02_CHG_REG_STATUS1, &data);
	if (ret < 0) {
		dev_err(&iic->dev, "%s fail\n", __func__);
		return ret;
	}

	switch (data & CHGSTS_STATUS_MASK) {
	case CHGSTS_STATUS_MASK:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}

	return status;
}

static bool s2mpw02_get_batt_present(struct i2c_client *iic)
{
	int ret;
	u8 data;

	ret = s2mpw02_read_reg(iic, S2MPW02_CHG_REG_STATUS2, &data);
	if (ret < 0)
		return false;

	return (data & DET_BAT_STATUS_MASK) ? true : false;
}

static int s2mpw02_get_charging_health(struct s2mpw02_charger_data *charger)
{
	int ret;
	u8 chg_sts1;

	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS1, &chg_sts1);

	pr_debug("[%s] chg_sts1: 0x%x\n " , __func__, chg_sts1);
	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (chg_sts1 & CHG_ACOK_STATUS_MASK) {
		charger->ovp = false;
		charger->unhealth_cnt = 0;
		pr_debug("[%s] POWER_SUPPLY_HEALTH_GOOD\n", __func__);
		return POWER_SUPPLY_HEALTH_GOOD;
	}

	if((chg_sts1 & CHGVINOVP_STATUS_MASK) && (chg_sts1 & CIN2BAT_STATUS_MASK)) {
		pr_err("[%s] POWER_SUPPLY_HEALTH_OVERVOLTAGE, unhealth_cnt %d\n " ,
			__func__, charger->unhealth_cnt);
		if (charger->unhealth_cnt < HEALTH_DEBOUNCE_CNT)
			return POWER_SUPPLY_HEALTH_GOOD;
		charger->unhealth_cnt++;
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
	return POWER_SUPPLY_HEALTH_GOOD;
}

static int s2mpw02_monitor_work_debug_info(struct s2mpw02_charger_data *charger)
{
	struct s2mpw02_charger_data *c = charger;

	pr_info("%s charging(%d) chg_current(%d) chg_sts(%d) eoc(%d) health(%d) "
		"float(%d) top_off(%d) cv_status(%d) in2bat(%d)\n",
		__func__,
		c->is_charging,
		c->charging_current,
		c->log_data.chg_status,
		c->topoff_current,
		c->log_data.health,
		charger->pdata->chg_float_voltage,
		!!(c->log_data.reg_data[S2MPW02_CHG_REG_STATUS1] & TOP_OFF_STATUS_MASK),
		!!(c->log_data.reg_data[S2MPW02_CHG_REG_STATUS3] & CV_OK_STATUS_MASK),
		!!(c->log_data.reg_data[S2MPW02_CHG_REG_STATUS1] & CIN2BAT_STATUS_MASK)
		);
#if EN_TEST_READ
	s2mpw02_test_read(charger);
#endif
	return 0;
}

static int s2mpw02_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	unsigned char status;
	struct s2mpw02_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mpw02_get_charging_status(charger);
		charger->log_data.chg_status = val->intval;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mpw02_get_charging_health(charger);
		charger->log_data.health = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 2000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			val->intval = s2mpw02_get_fast_charging_current(charger);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = s2mpw02_get_fast_charging_current(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		val->intval = s2mpw02_get_current_eoc_setting(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mpw02_get_charge_type(charger->client);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = charger->pdata->chg_float_voltage;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mpw02_get_batt_present(charger->client);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_POWER_NOW:
		s2mpw02_read_reg(charger->iodev->pmic, S2MPW02_PMIC_REG_STATUS1, &status);
		pr_info("%s: pm status : 0x%x\n", __func__, status);
		if (status & ACOK_STATUS_MASK)
			val->intval = ACOK_INPUT;
		else
			val->intval = ACOK_NO_INPUT;
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
				val->intval = 0;
				s2mpw02_monitor_work_debug_info(charger);
				break;
			case POWER_SUPPLY_EXT_PROP_CHG_IN_OK:
				s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS2, &status);
				val->intval = (status & A2D_CHGINOK_STATUS_MASK) ? 1 : 0;
				break;
			default:
				break;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpw02_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mpw02_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = psp;

	struct device *dev = charger->dev;
/*	int previous_cable_type = charger->cable_type; */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
				charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			dev_dbg(dev, "%s Type Battery\n", __func__);
			if (!charger->pdata->charging_current_table)
				return -EINVAL;
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			dev_dbg(dev, "%s OTG not supported\n", __func__);
		} else {
			dev_info(dev, "%s Set charging, Cable type = %d\n",
				 __func__, charger->cable_type);
#if ENABLE_MIVR
			s2mpw02_set_mivr_level(charger);
#endif /*ENABLE_MIVR*/
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		dev_dbg(dev, "%s set current[%d]\n", __func__, val->intval);
		charger->charging_current = val->intval;
		s2mpw02_set_fast_charging_current(charger, charger->charging_current);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		dev_dbg(dev, "%s float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mpw02_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
		break;
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		charger->topoff_current = val->intval;
		s2mpw02_set_topoff_current(charger->client, charger->topoff_current);
		dev_dbg(dev, "%s chg eoc current = %dmA is_charging %d\n",
				__func__, val->intval, charger->is_charging);
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		dev_dbg(dev, "%s OTG not supported\n", __func__);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		charger->charge_mode = val->intval;
		pr_info("%s: CHG_ENABLE(%d)\n", __func__, charger->charge_mode);
		switch (charger->charge_mode) {
		case SEC_BAT_CHG_MODE_BUCK_OFF:
		case SEC_BAT_CHG_MODE_CHARGING_OFF:
			charger->is_charging = false;
			break;
		case SEC_BAT_CHG_MODE_CHARGING:
			charger->is_charging = true;
			break;
		}
		s2mpw02_enable_charger_switch(charger, charger->is_charging);
		break;
	case POWER_SUPPLY_PROP_AUTHENTIC:
		if (val->intval) {
			s2mpw02_set_bypass_mode(charger, CHG_RID_CTRL_523K, CHG_MODE_BYPASS);
		}
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_WPC_ONLINE:
				if (val->intval == SEC_WIRELESS_PAD_WPC) {
					pr_info("%s: Wireless TA connected\n", __func__);
					s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_WIRELESS_TA_MUIC);
				} else if (val->intval == SEC_WIRELESS_PAD_NONE) {
					pr_info("%s: Wireless TA disconnected\n", __func__);
					muic_notifier_detach_attached_dev(charger->muic_dev);
					charger->muic_dev = ATTACHED_DEV_NONE_MUIC;
				}
				break;
			case POWER_SUPPLY_EXT_PROP_FORCED_JIG_MODE:
				s2mpw02_set_forced_jig_mode(charger);
				break;
			default:
				break;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mpw02_factory_mode_setting(struct s2mpw02_charger_data *charger)
{
	charger->factory_mode = true;
	pr_info("%s, factory mode\n", __func__);
}

static void s2mpw02_topoff_status_setting(struct s2mpw02_charger_data *charger)
{
	u8 chg_sts1, chg_sts3;

	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS1, &chg_sts1);
	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS3, &chg_sts3);

	if ((chg_sts1 & TOP_OFF_STATUS_MASK) && (chg_sts3 & CV_OK_STATUS_MASK)) {
		pr_info("%s : top_off status!\n", __func__);
		charger->full_charged = true;
	} else {
		charger->full_charged = false;
	}
}

#if 0
static irqreturn_t s2mpw02_chg_isr(int irq, void *data)
{
	struct s2mpw02_charger_data *charger = data;
	u8 val, valm;

	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS1, &val);
	pr_info("[DEBUG]%s, %02x\n" , __func__, val);
	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_INT1M, &valm);
	pr_info("%s : CHG_INT1 ---> 0x%x\n", __func__, valm);

	if ((val & TOP_OFF_STATUS_MASK) && (val & CHGSTS_STATUS_MASK)) {
		pr_info("%s : top_off status~!\n", __func__);
		charger->full_charged = true;
	}

	return IRQ_HANDLED;
}
#endif

static irqreturn_t s2mpw02_tmrout_isr(int irq, void *data)
{
	struct s2mpw02_charger_data *charger = data;
	u8 val;

	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS2, &val);
	if (val & TMROUT_STATUS_MASK) {
		/* Timer out status */
		pr_info("%s, fast-charging timeout, timer clear\n", __func__);
		s2mpw02_enable_charger_switch(charger, false);
		msleep(100);
		s2mpw02_enable_charger_switch(charger, true);
	}
	return IRQ_HANDLED;
}

static void s2mpw02_muic_attach_dev(
	struct s2mpw02_charger_data *charger, muic_attached_dev_t muic_dev)
{
#if defined(CONFIG_MUIC_NOTIFIER)
	charger->muic_dev = muic_dev;
	muic_notifier_attach_attached_dev(charger->muic_dev);
#endif

}

#if defined(CONFIG_S2MPW02_RID_DETECT)
static void s2mpw02_muic_init_detect(struct work_struct *work)
{
	struct s2mpw02_charger_data *charger =
		container_of(work, struct s2mpw02_charger_data, init_work.work);

	int ret;
	unsigned char pmic_sts1, chg_sts2, chg_sts4;

	/* check when booting after USB connected */
	ret = s2mpw02_read_reg(charger->iodev->pmic, S2MPW02_PMIC_REG_STATUS1, &pmic_sts1);
	if (ret < 0) {
		pr_err("%s chg status1 read fail\n", __func__);
	}
	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS2, &chg_sts2);
	if (ret < 0) {
		pr_err("%s chg status2 read fail\n", __func__);
	}
	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS4, &chg_sts4);
	if (ret < 0) {
		pr_err("%s chg status4 read fail\n", __func__);
	}

	pr_info("%s: pmic_sts1, chg_sts2, chg_sts4: 0x%x, 0x%x, 0x%x\n",
		__func__, pmic_sts1, chg_sts2, chg_sts4);

	if (pmic_sts1 & ACOK_STATUS_MASK) {
		charger->onoff = 1;
		if (chg_sts4 & UART_CABLE_MASK) {
			s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_USB_MUIC);
			pr_info("%s: USB connected\n", __func__);
		} else {
			if (chg_sts4 & UART_BOOT_OFF_MASK) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_UART_OFF_MUIC);
				pr_info("%s: JIG UART OFF(523K) connected VBUS ON\n", __func__);
			} else if (chg_sts4 & UART_BOOT_ON_MASK) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_UART_ON_MUIC);
				pr_info("%s: JIG UART ON(619K) connected\n", __func__);
			} else if (chg_sts2 & A2D_CHGINOK_STATUS_MASK) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_TA_MUIC);
				pr_info("%s: Wired TA connected\n", __func__);
			} else {
#if 0
				union power_supply_propval val = {0,};
				psy_do_property(charger->pdata->wireless_charger_name, get,
					POWER_SUPPLY_PROP_PRESENT, val);

				if (val.intval == WPC_ON_PAD) {
					s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_WIRELESS_TA_MUIC);
					pr_info("%s: Wireless TA connected\n", __func__);
				}
#endif
			}
		}
	} else {
		if (chg_sts4 & UART_BOOT_OFF_MASK) {
			s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_UART_OFF_MUIC);
			pr_info("%s: JIG UART OFF(523K) connected VBUS OFF\n", __func__);
		}
	}
}

static void s2mpw02_muic_usb_detect(struct work_struct *work)
{
	struct s2mpw02_charger_data *charger =
		container_of(work, struct s2mpw02_charger_data, usb_work.work);

	int ret;
	unsigned char pmic_sts1, chg_sts4;

	charger->is_usb_ready = true;
	/* check when booting after USB connected */
	ret = s2mpw02_read_reg(charger->iodev->pmic, S2MPW02_PMIC_REG_STATUS1, &pmic_sts1);
	if (ret < 0) {
		pr_err("%s pmic_sts1 read fail\n", __func__);
	}
	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS4, &chg_sts4);
	if (ret < 0) {
		pr_err("%s chg_sts4 read fail\n", __func__);
	}

	if (charger->dev_id < EVT_1) {
		/* detach with RID W/A EVT0 only */
		if(chg_sts4 & UART_CABLE_MASK) {
			pr_info("%s 150K attach\n", __func__);
			s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP1, 0x60);
			s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP14, 0x08);
		}
	}

	pr_info("%s pmic_sts1:0x%x chg chg_sts4:0x%x\n", __func__, pmic_sts1, chg_sts4);
	if (pmic_sts1 & ACOK_STATUS_MASK) {
		charger->onoff = 1;
		if(chg_sts4 & UART_CABLE_MASK) {
			if (charger->is_usb_ready) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_USB_MUIC);
			}
			pr_info("%s: USB connected\n", __func__);
		} else if (chg_sts4 & USB_BOOT_ON_MASK) {
			if (charger->is_usb_ready) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_USB_ON_MUIC);
			}
			pr_info("%s: JIG USB ON(301K) connected\n", __func__);
		} else if (chg_sts4 & USB_BOOT_OFF_MASK) {
			if (charger->is_usb_ready) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_USB_OFF_MUIC);
			}
			pr_info("%s: JIG USB OFF(255K) connected\n", __func__);
		}
	}
}

static void s2mpw02_muic_rid_check(struct work_struct *work)
{
	struct s2mpw02_charger_data *charger =
		container_of(work, struct s2mpw02_charger_data, rid_work.work);
	unsigned char chg_sts4;

	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS4, &chg_sts4);
	pr_info("%s: chg_sts4 0x%x\n", __func__, chg_sts4);

	if (!((chg_sts4 & USB_BOOT_ON_MASK) || (chg_sts4 & USB_BOOT_OFF_MASK) ||
		(chg_sts4 & UART_BOOT_ON_MASK) || (chg_sts4 & UART_BOOT_OFF_MASK) ||
		(chg_sts4 & UART_CABLE_MASK) || (chg_sts4 & FACT_LEAKAGE_MASK))) {
		charger->factory_mode = false;
		pr_info("%s: factory mode[%d]\n", __func__, charger->factory_mode);
	}
}

static void s2mpw02_muic_attach_detect(struct work_struct *work)
{
	struct s2mpw02_charger_data *charger =
		container_of(work, struct s2mpw02_charger_data, acok_work.work);

	s2mpw02_muic_detect_handler(charger, true);
}

static void s2mpw02_muic_detect_handler(struct s2mpw02_charger_data *charger, bool is_on)
{
	unsigned char chg_sts2, chg_sts4, pmic_sts1;

	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS2, &chg_sts2);
	s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS4, &chg_sts4);
	s2mpw02_read_reg(charger->iodev->pmic, S2MPW02_PMIC_REG_STATUS1, &pmic_sts1);
	pr_info("%s:sts2:0x%x, sts4:0x%x, pmic_sts1:0x%x\n", __func__, chg_sts2, chg_sts4, pmic_sts1);

	if(is_on) {

		/* re-check ACOK_STATUS */
		/* attach event is handled after 150msec delay unlike detach event */
		/* detach event might occur during attach event delay */
		if (!(pmic_sts1 & ACOK_STATUS_MASK)) {
			pr_info("%s already detached during 150msec delay for attach event!\n", __func__);
			return;
		}

		charger->onoff = 1;

		if (charger->dev_id < EVT_1) {
			/* detach with RID W/A EVT0 only */
			if(chg_sts4 & UART_CABLE_MASK) {
				pr_info("%s 150K attach\n", __func__);
				s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP1, 0x60);
				s2mpw02_write_reg(charger->client, S2MPW02_CHG_REG_CHG_OTP14, 0x08);
			}
		}

		if(chg_sts4 & UART_CABLE_MASK) {
			if (charger->is_usb_ready) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_USB_MUIC);
			}
			pr_info("%s: USB connected.\n", __func__);
		} else {
			if (!(chg_sts4 & JIGON_MASK) && !(chg_sts4 & USB_BOOT_ON_MASK) &&
				!(chg_sts4 & USB_BOOT_OFF_MASK) && !(chg_sts4 & UART_BOOT_ON_MASK)
				&& !(chg_sts4 & UART_BOOT_OFF_MASK)) {
				if (chg_sts2 & A2D_CHGINOK_STATUS_MASK) {
					pr_info("%s: Wired TA connected\n", __func__);
#if defined(CONFIG_S2MPW02_TA_DETECT)
					s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_USB_MUIC);
#else
					s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_TA_MUIC);
#endif
				} else {
					s2mpw02_muic_attach_dev(charger,
						ATTACHED_DEV_UNDEFINED_CHARGING_MUIC);
					pr_info("%s: UNDEFINED_CHARGING_MUIC connected\n", __func__);
				}
			} else {
				if ((chg_sts4 & USB_BOOT_ON_MASK) || (chg_sts4 & USB_BOOT_OFF_MASK) ||
					(chg_sts4 & UART_BOOT_ON_MASK) || (chg_sts4 & UART_BOOT_OFF_MASK)) {
					s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_UART_OFF_MUIC);
					pr_info("%s JIG UART OFF(523K) connected \n", __func__);
				}
				if ((chg_sts4 & USB_BOOT_ON_MASK) || (chg_sts4 & USB_BOOT_OFF_MASK)) {
					if (charger->is_usb_ready) {
						s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_USB_ON_MUIC);
					}
					pr_info("%s JIG USB ON(301K) connected \n", __func__);
				}
				pr_info("%s: JIG_ID connected.\n", __func__);
			}
		}
	} else {
		charger->onoff = 0;

#if defined(CONFIG_MUIC_NOTIFIER)
		muic_notifier_detach_attached_dev(charger->muic_dev);
		charger->muic_dev = ATTACHED_DEV_NONE_MUIC;
#endif
	}
}

static irqreturn_t s2mpw02_muic_isr(int irq, void *data)
{
	struct s2mpw02_charger_data *charger = data;

	pr_info("%s: irq:%d\n", __func__, irq);

	if (irq == charger->irq_acokr)
		schedule_delayed_work(&charger->acok_work, msecs_to_jiffies(150));
	else if (irq == charger->irq_acokf) {
		s2mpw02_muic_detect_handler(charger, false);
	} else if (irq == charger->irq_usb_on) {
		/* usb boot on */
		pr_info("%s: usb boot on irq\n", __func__);
		if (charger->onoff == 1) {
			pr_info("%s: usb boot on notify done\n", __func__);

			s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_USB_ON_MUIC);
			if (charger->is_usb_ready) {
				s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_USB_ON_MUIC);
				pr_info("%s: attach muic_dev %d\n", __func__, charger->muic_dev);
			}
			s2mpw02_factory_mode_setting(charger);
		}
	} else if (irq == charger->irq_uart_off) {
		/* uart boot off */
		pr_info("%s: uart boot off irq:%d charger->onoff:%d\n",
			__func__, irq, charger->onoff);

		if (charger->onoff == 1) {
			s2mpw02_factory_mode_setting(charger);
			s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_UART_OFF_MUIC);
		}
	} else if (irq == charger->irq_uart_on) {
		/* uart boot on */
		pr_info("%s: JIG UART ON(619K) connected\n", __func__);
		s2mpw02_factory_mode_setting(charger);
	} else if (irq == charger->irq_usb_off) {
		/* usb boot off */
		pr_info("%s: usb boot off\n", __func__);
		if (charger->onoff == 1) {
			pr_info("%s: JIG USB OFF(255K) connected \n", __func__);
			s2mpw02_factory_mode_setting(charger);
			s2mpw02_muic_attach_dev(charger, ATTACHED_DEV_JIG_USB_OFF_MUIC);
		}
	} else if (irq == charger->irq_jigon) {
		schedule_delayed_work(&charger->rid_work, msecs_to_jiffies(200));

		/* jigon */
		pr_info("%s: jigon irq:%d\n", __func__, irq);
	}
	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _dev_id, _name)				\
do {									\
	ret = request_threaded_irq(_irq, NULL, s2mpw02_muic_isr,	\
				0, _name, _dev_id);	\
	if (ret < 0) {							\
		pr_err("%s: Failed to request s2mpw02 muic IRQ #%d: %d\n",		\
				__func__, _irq, ret);	\
		_irq = 0;						\
	}								\
} while (0)

static int s2mpw02_muic_irq_init(struct s2mpw02_charger_data *charger)
{
	int ret = 0;

	if (charger->iodev && (charger->iodev->irq_base > 0)) {
		int irq_base = charger->iodev->irq_base;

		/* request MUIC IRQ */
#if 0
		charger->irq_fact_leakage = irq_base + S2MPW02_CHG_IRQ_FACT_LEAKAGE_INT3;
		REQUEST_IRQ(charger->irq_fact_leakage, charger, "muic-fact_leakage");
#endif
		charger->irq_uart_cable = irq_base + S2MPW02_CHG_IRQ_UART_CABLE_INT4;
		REQUEST_IRQ(charger->irq_uart_cable, charger, "muic-uart_cable");

		/* W/A: jigon cannot be used before EVT1 */
		if (charger->dev_id >= EVT_1) {
			charger->irq_jigon = irq_base + S2MPW02_CHG_IRQ_JIGON_INT4;
			REQUEST_IRQ(charger->irq_jigon, charger, "muic-jigon");
		}

		charger->irq_usb_off = irq_base + S2MPW02_CHG_IRQ_USB_BOOT_OFF_INT4;
		REQUEST_IRQ(charger->irq_usb_off, charger, "muic-usb_off");

		charger->irq_usb_on = irq_base + S2MPW02_CHG_IRQ_USB_BOOT_ON_INT4;
		REQUEST_IRQ(charger->irq_usb_on, charger, "muic-usb_on");

		charger->irq_uart_off = irq_base + S2MPW02_CHG_IRQ_UART_BOOT_OFF_INT4;
		REQUEST_IRQ(charger->irq_uart_off, charger, "muic-uart_off");

		charger->irq_uart_on = irq_base + S2MPW02_CHG_IRQ_UART_BOOT_ON_INT4;
		REQUEST_IRQ(charger->irq_uart_on, charger, "muic-uart_on");

		charger->irq_acokf = irq_base + S2MPW02_PMIC_IRQ_ACOKBF_INT1;
		REQUEST_IRQ(charger->irq_acokf, charger, "muic-acokf");

		charger->irq_acokr = irq_base + S2MPW02_PMIC_IRQ_ACOKBR_INT1;
		REQUEST_IRQ(charger->irq_acokr, charger, "muic-acokr");
	}

	pr_info("%s:usb_off(%d), usb_on(%d), uart_off(%d), uart_on(%d), jig_on(%d), muic-acokf(%d), muic-acokr(%d)\n",
		__func__, charger->irq_usb_off, charger->irq_usb_on, charger->irq_uart_off, charger->irq_uart_on,
		charger->irq_jigon, charger->irq_acokf, charger->irq_acokr);

	return ret;
}

#define FREE_IRQ(_irq, _dev_id, _name)					\
do {									\
	if (_irq) {							\
		free_irq(_irq, _dev_id);				\
		pr_info("%s: IRQ(%d):%s free done\n",	\
				__func__, _irq, _name);			\
	}								\
} while (0)

static void s2mpw02_muic_free_irqs(struct s2mpw02_charger_data *charger)
{
	pr_info("%s\n", __func__);

	/* free MUIC IRQ */
	FREE_IRQ(charger->irq_uart_off, charger, "muic-uart_off");
	FREE_IRQ(charger->irq_uart_on, charger, "muic-uart_on");
	FREE_IRQ(charger->irq_usb_off, charger, "muic-usb_off");
	FREE_IRQ(charger->irq_usb_on, charger, "muic-usb_on");
	FREE_IRQ(charger->irq_uart_cable, charger, "muic-uart_cable");
	FREE_IRQ(charger->irq_fact_leakage, charger, "muic-fact_leakage");
	FREE_IRQ(charger->irq_jigon, charger, "muic-jigon");
}
#endif

#ifdef CONFIG_OF
static int s2mpw02_charger_parse_dt(struct device *dev,
		struct s2mpw02_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mpw02-charger");
	int ret;

	/* SC_CTRL8 , SET_VF_VBAT , Battery regulation voltage setting */
	ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
		"battery,charger_name", (char const **)&pdata->charger_name);

		ret = of_property_read_string(np,
			"battery,wireless_charger_name",
			(char const **)&pdata->wireless_charger_name);
		if (ret)
			pr_info("%s: Wireless charger name is Empty\n", __func__);

		ret = of_property_read_u32(np, "battery,full_check_type",
				&pdata->full_check_type);

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
				&pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n",
						__func__);
	}
	dev_info(dev, "s2mpw02 charger parse dt retval = %d\n", ret);
	return ret;
}
#else
static int s2mpw02_charger_parse_dt(struct device *dev,
		struct s2mpw02_charger_platform_data *pdata)
{
	return -ENOSYS;
}
#endif
/* if need to set s2mpw02 pdata */
static struct of_device_id s2mpw02_charger_match_table[] = {
	{ .compatible = "samsung,s2mpw02-charger",},
	{},
};

static int s2mpw02_charge_table_cmp(const void *a, const void *b)
{
	const struct s2mpw02_additional_curr *curr1 = a;
	const struct s2mpw02_additional_curr *curr2 = b;

	if (curr1->chg_current == curr2->chg_current)
		return curr1->additional_chg_set - curr2->additional_chg_set;

	return curr1->chg_current - curr2->chg_current;
}

static int
s2mpw02_charge_table_search(struct s2mpw02_charger_data *charger, int chg_current)
{
	int i;
	for (i = 0; i < charger->table_size; i++) {
		if (chg_current <= charger->chg_curr_table[i].chg_current) {
			if (i == 0) {
				return i;
			} else {
				if (abs(charger->chg_curr_table[i].chg_current - chg_current) >=
					abs(charger->chg_curr_table[i - 1].chg_current - chg_current)) {
					return i - 1;
				} else {
					return i;
				}

			}
		}
	}
	return i - 1;
}

static int s2mpw02_make_additional_charge_table(struct s2mpw02_charger_data *charger)
{
	int len;
	int add_chg_n_value[] = {1, 2, 3, 4, 5, 6, 7, 8};
	int i, j;
	int cnt = 0;
	int curr;

	len = (ARRAY_SIZE(fast_charging_current) + 1) * ARRAY_SIZE(add_chg_n_value);

	charger->chg_curr_table =
		kmalloc(len * sizeof(struct s2mpw02_additional_curr), GFP_KERNEL);

	if (charger->chg_curr_table == NULL)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(fast_charging_current); i++) {
		curr = fast_charging_current[i];
		charger->chg_curr_table[cnt].chg_current = curr;
		charger->chg_curr_table[cnt].fast_chg_reg_val = i;
		charger->chg_curr_table[cnt].add_chg_reg_val = 0;
		charger->chg_curr_table[cnt].additional_chg_set = 0;
		cnt++;
		for (j = 0; j < ARRAY_SIZE(add_chg_n_value); j++) {
			charger->chg_curr_table[cnt].chg_current =
				(curr * add_chg_n_value[j] / ARRAY_SIZE(add_chg_n_value)) + curr;
			charger->chg_curr_table[cnt].fast_chg_reg_val = i;
			charger->chg_curr_table[cnt].add_chg_reg_val = j;
			charger->chg_curr_table[cnt].additional_chg_set = 1;
			cnt++;
		}
	}

	charger->table_size = cnt;

	sort(charger->chg_curr_table, cnt, sizeof(struct s2mpw02_additional_curr),
		s2mpw02_charge_table_cmp, NULL);

	return 0;
}

static int s2mpw02_charger_probe(struct platform_device *pdev)
{
	struct s2mpw02_dev *s2mpw02 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw02_platform_data *pdata = dev_get_platdata(s2mpw02->dev);
	struct s2mpw02_charger_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;
	u8 chg_sts4;

	pr_info("%s:[BATT] S2MPW02 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->io_lock);

	charger->iodev = s2mpw02;
	charger->dev = &pdev->dev;
	charger->client = s2mpw02->charger;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mpw02_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "sec-charger";

	charger->psy_chg_desc.name           = charger->pdata->charger_name;
	charger->psy_chg_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg_desc.get_property   = s2mpw02_chg_get_property;
	charger->psy_chg_desc.set_property   = s2mpw02_chg_set_property;
	charger->psy_chg_desc.properties     = s2mpw02_charger_props;
	charger->psy_chg_desc.num_properties = ARRAY_SIZE(s2mpw02_charger_props);

	charger->dev_id = s2mpw02->pmic_rev;

#if defined(CONFIG_MUIC_NOTIFIER)
	charger->muic_dev = ATTACHED_DEV_NONE_MUIC;
#endif
	charger->onoff = 0;

	s2mpw02_chg_init(charger);
	s2mpw02_set_additional(charger, 0, 0);

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = s2mpw02_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mpw02_supplied_to);
	charger->psy_chg = power_supply_register(&pdev->dev, &charger->psy_chg_desc, &psy_cfg);
	if (IS_ERR(charger->psy_chg)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(charger->psy_chg);
		goto err_power_supply_register;
	}

	charger->irq_chg = pdata->irq_base + S2MPW02_CHG_IRQ_TOPOFF_INT1;
#if 0
	ret = request_threaded_irq(charger->irq_chg, NULL,
			s2mpw02_chg_isr, 0, "chg-irq", charger);
	if (ret < 0) {
		dev_err(s2mpw02->dev, "%s: Fail to request charger irq in IRQ: %d: %d\n",
					__func__, charger->irq_chg, ret);
		goto err_power_supply_register;
	}
#endif
	charger->irq_tmrout = charger->iodev->irq_base + S2MPW02_CHG_IRQ_TMROUT_INT2;
	ret = request_threaded_irq(charger->irq_tmrout, NULL,
			s2mpw02_tmrout_isr, 0, "tmrout-irq", charger);
	if (ret < 0) {
		dev_err(s2mpw02->dev, "%s: Fail to request charger irq in IRQ: %d: %d\n",
					__func__, charger->irq_tmrout, ret);
		goto err_power_supply_register;
	}
#if EN_TEST_READ
	s2mpw02_test_read(charger);
#endif

#if defined(CONFIG_S2MPW02_RID_DETECT)
	INIT_DELAYED_WORK(&charger->rid_work, s2mpw02_muic_rid_check);

	INIT_DELAYED_WORK(&charger->init_work, s2mpw02_muic_init_detect);
	schedule_delayed_work(&charger->init_work, msecs_to_jiffies(3000));

	charger->is_usb_ready = false;
	INIT_DELAYED_WORK(&charger->usb_work, s2mpw02_muic_usb_detect);
	schedule_delayed_work(&charger->usb_work, msecs_to_jiffies(13000));

	INIT_DELAYED_WORK(&charger->acok_work, s2mpw02_muic_attach_detect);

	ret = s2mpw02_muic_irq_init(charger);
	if (ret) {
		pr_err( "[muic] %s: failed to init muic irq(%d)\n", __func__, ret);
		goto fail_init_irq;
	}
#endif

	if (s2mpw02_make_additional_charge_table(charger) < 0) {
		goto fail_init_irq;
	}

	/* factory_mode setting */
	ret = s2mpw02_read_reg(charger->client, S2MPW02_CHG_REG_STATUS4, &chg_sts4);
	if (ret < 0)
		pr_err("%s chg sts4 read fail\n", __func__);

	if ((chg_sts4 & USB_BOOT_ON_MASK) || (chg_sts4 & USB_BOOT_OFF_MASK) ||
		(chg_sts4 & UART_BOOT_ON_MASK) || (chg_sts4 & UART_BOOT_OFF_MASK) ||
		(chg_sts4 & FACT_LEAKAGE_MASK))
		s2mpw02_factory_mode_setting(charger);

	pr_info("%s S2MPW02 charger driver loaded OK\n", __func__);

	return 0;

#if defined(CONFIG_S2MPW02_RID_DETECT)
fail_init_irq:
	s2mpw02_muic_free_irqs(charger);
#endif
err_power_supply_register:
	destroy_workqueue(charger->charger_wqueue);
	power_supply_unregister(charger->psy_chg);
//	power_supply_unregister(&charger->psy_battery);
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return ret;
}

static int s2mpw02_charger_remove(struct platform_device *pdev)
{
	struct s2mpw02_charger_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mpw02_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mpw02_charger_resume(struct device *dev)
{

	return 0;
}
#else
#define s2mpw02_charger_suspend NULL
#define s2mpw02_charger_resume NULL
#endif

static void s2mpw02_charger_shutdown(struct device *dev)
{
	struct s2mpw02_charger_data *charger = dev_get_drvdata(dev);

	s2mpw02_enable_charger_switch(charger, true);

	pr_info("%s: S2MPW02 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mpw02_charger_pm_ops, s2mpw02_charger_suspend,
		s2mpw02_charger_resume);

static struct platform_driver s2mpw02_charger_driver = {
	.driver         = {
		.name   = "s2mpw02-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mpw02_charger_match_table,
		.pm     = &s2mpw02_charger_pm_ops,
		.shutdown = s2mpw02_charger_shutdown,
	},
	.probe          = s2mpw02_charger_probe,
	.remove		= s2mpw02_charger_remove,
};

static int __init s2mpw02_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mpw02_charger_driver);

	return ret;
}
device_initcall(s2mpw02_charger_init);

static void __exit s2mpw02_charger_exit(void)
{
	platform_driver_unregister(&s2mpw02_charger_driver);
}
module_exit(s2mpw02_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MPW02");
