/*
 *  p9222_charger.c
 *  Samsung p9222 Charger Driver
 *
 *  Copyright (C) 2015 Samsung Electronics
 * Yeongmi Ha <yeongmi86.ha@samsung.com>
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

#include <linux/errno.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/ctype.h>
#include <linux/firmware.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#include <linux/alarmtimer.h>
#include <linux/wakelock.h>
#include <../drivers/battery_v2/include/sec_charging_common.h>
#include <../drivers/battery_v2/include/charger/p9222_charger.h>
#include <../drivers/battery_v2/include/sec_battery.h>

/* Vout stabilization time is about 1.5sec after Vrect ocured. */
#define VOUT_STABLILZATION_TIME		1500

#define WPC_AUTH_DELAY_TIME			25
#define UNKNOWN_TX_ID				0xFF

#define ENABLE 1
#define DISABLE 0
#define CMD_CNT 3

#define P9222_I_OUT_OFFSET		12

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
#define P9222S_FW_SDCARD_BIN_PATH	"sdcard/p9222_otp.bin"
#define P9222S_OTP_FW_HEX_PATH		"idt/p9222_otp.bin"
#define P9222S_SRAM_FW_HEX_PATH		"idt/p9222_sram.bin"
#endif

//bool sleep_mode = false;

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	POWER_SUPPLY_PROP_MANUFACTURER,
#endif
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
	POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_ENERGY_AVG,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION,
	POWER_SUPPLY_PROP_TEMP,
};

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
static int p9222_otp_update = 0;
static u8 adc_cal = 0;
#endif

unsigned int batt_booting_chk = 1;
struct device *wpc_device;

extern unsigned int lpcharge;

static int
p9222_wpc_auth_delayed_work(struct p9222_charger_data *charger, int sec);
static int p9222_ap_battery_monitor(struct p9222_charger_data *charger, int mode);
static int p9222_full_charge(struct p9222_charger_data *charger);
static void p9222_wpc_request_tx_id(struct p9222_charger_data *charger, int cnt);

static int p9222_reg_read(struct i2c_client *client, u16 reg, u8 *val)
{
	struct p9222_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	struct i2c_msg msg[2];
	u8 wbuf[2];
	u8 rbuf[2];

	msg[0].addr = client->addr;
	msg[0].flags = client->flags & I2C_M_TEN;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	wbuf[0] = (reg & 0xFF00) >> 8;
	wbuf[1] = (reg & 0xFF);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = rbuf;

	mutex_lock(&charger->io_lock);
	ret = i2c_transfer(client->adapter, msg, 2);
	mutex_unlock(&charger->io_lock);
	if (ret < 0)
	{
		dev_err(&client->dev, "%s read err reg(0x%x) ret(%d)\n",
			__func__, reg, ret);
		return -1;
	}
	*val = rbuf[0];

#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	if(!p9222_otp_update)
#endif
	{
		pr_debug("%s reg = 0x%x, data = 0x%x\n", __func__, reg, *val);
	}

	return ret;
}

static int p9222_reg_write(struct i2c_client *client, u16 reg, u8 val)
{
	struct p9222_charger_data *charger = i2c_get_clientdata(client);
	int ret;
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };

	mutex_lock(&charger->io_lock);
	ret = i2c_master_send(client, data, 3);
	mutex_unlock(&charger->io_lock);
	if (ret < 3) {
		dev_err(&client->dev, "%s write err reg(0x%x) ret(%d)\n",
			__func__, reg, ret);
		return ret < 0 ? ret : -EIO;
	}

	pr_debug("%s reg = 0x%x, data = 0x%x \n", __func__, reg, val);

	return 0;
}

static int p9222_reg_update(struct i2c_client *client, u16 reg, u8 val, u8 mask)
{
	struct p9222_charger_data *charger = i2c_get_clientdata(client);
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };
	u8 data2;
	int ret;

	ret = p9222_reg_read(client, reg, &data2);
	if (ret >= 0) {
		u8 old_val = data2 & 0xff;
		u8 new_val = (val & mask) | (old_val & (~mask));
		data[2] = new_val;

		mutex_lock(&charger->io_lock);
		ret = i2c_master_send(client, data, 3);
		mutex_unlock(&charger->io_lock);
		if (ret < 3) {
			dev_err(&client->dev, "%s: i2c write error, reg: 0x%x, ret: %d\n",
				__func__, reg, ret);
			return ret < 0 ? ret : -EIO;
		}
	}
	p9222_reg_read(client, reg, &data2);

	pr_debug("%s reg = 0x%x, data = 0x%x, mask = 0x%x\n", __func__, reg, val, mask);

	return ret;
}

static int p9222_is_on_pad(struct p9222_charger_data *charger)
{
	int ret;
	int wpc_det = charger->pdata->wpc_det;
	ret = (gpio_get_value(wpc_det) == 0) ? WPC_ON_PAD : WPC_OFF_PAD;
	return ret;
}

static int p9222_get_adc(struct p9222_charger_data *charger, int adc_type)
{
	int ret = 0;
	u8 data[2] = {0,};

	switch (adc_type) {
		case P9222_ADC_VOUT:
			ret = p9222_reg_read(charger->client, P9222_ADC_VOUT_L_REG, &data[0]);
			ret = p9222_reg_read(charger->client, P9222_ADC_VOUT_H_REG, &data[1]);
			if(ret >= 0 ) {
				//data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8));
			} else
				ret = 0;
			break;
		case P9222_ADC_VRECT:
			ret = p9222_reg_read(charger->client, P9222_ADC_VRECT_L_REG, &data[0]);
			ret = p9222_reg_read(charger->client, P9222_ADC_VRECT_H_REG, &data[1]);
			if(ret >= 0 ) {
				//data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8));
			} else
				ret = 0;
			break;
		case P9222_ADC_TX_ISENSE:
			ret = p9222_reg_read(charger->client, P9222_ADC_TX_ISENSE_L_REG, &data[0]);
			ret = p9222_reg_read(charger->client, P9222_ADC_TX_ISENSE_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8)); // need to check
			} else
				ret = 0;
			break;
		case P9222_ADC_RX_IOUT:
			ret = p9222_reg_read(charger->client, P9222_ADC_RX_IOUT_L_REG, &data[0]);
			ret = p9222_reg_read(charger->client, P9222_ADC_RX_IOUT_H_REG, &data[1]);
			if(ret >= 0 ) {
				ret = (data[0] | (data[1] << 8));
			} else
				ret = 0;
			break;
		case P9222_ADC_DIE_TEMP:
			ret = p9222_reg_read(charger->client, P9222_ADC_DIE_TEMP_L_REG, &data[0]);
			ret = p9222_reg_read(charger->client, P9222_ADC_DIE_TEMP_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8)); // need to check
			} else
				ret = 0;
			break;

		case P9222_ADC_ALLIGN_X:
			ret = p9222_reg_read(charger->client, P9222_ADC_ALLIGN_X_REG, &data[0]);
			if(ret >= 0 ) {
				ret = data[0]; // need to check
			} else
				ret = 0;
			break;

		case P9222_ADC_ALLIGN_Y:
			ret = p9222_reg_read(charger->client, P9222_ADC_ALLIGN_Y_REG, &data[0]);
			if(ret >= 0 ) {
				ret = data[0]; // need to check
			} else
				ret = 0;
			break;
		case P9222_ADC_OP_FRQ:
			ret = p9222_reg_read(charger->client, P9222_OP_FREQ_L_REG, &data[0]);
			if(ret < 0 ) {
				ret = 0;
				return ret;
			}
			ret = p9222_reg_read(charger->client, P9222_OP_FREQ_H_REG, &data[1]);
			if(ret >= 0 )
				ret = (data[0] | (data[1] << 8));
			else
				ret = 0;
			break;
		case P9222_ADC_VBAT_RAW:
			ret = p9222_reg_read(charger->client, P9222_VBAT_L_REG, &data[0]);
			ret = p9222_reg_read(charger->client, P9222_VBAT_H_REG, &data[1]);
			if(ret >= 0 ) {
				//data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8)); // need to check
			} else
				ret = 0;
			break;
		case P9222_ADC_VBAT:
			ret = p9222_reg_read(charger->client, P9222_ADC_VBAT_L_REG, &data[0]);
			ret = p9222_reg_read(charger->client, P9222_ADC_VBAT_H_REG, &data[1]);
			if(ret >= 0 ) {
				data[1] &= 0x0f;
				ret = (data[0] | (data[1] << 8)); // need to check
			} else
				ret = 0;
			break;
		default:
			break;
	}

	return ret;
}

static void p9222_fod_set(struct p9222_charger_data *charger)
{
	int i = 0;

	pr_info("%s \n", __func__);
	if(charger->pdata->fod_data_check) {
		for(i=0; i< P9222_NUM_FOD_REG; i++)
			p9222_reg_write(charger->client, P9222_WPC_FOD_0A_REG+i, charger->pdata->fod_data[i]);
	}
}

static void p9222_set_cmd_reg(struct p9222_charger_data *charger, u8 val, u8 mask)
{
	u8 temp = 0;
	int ret = 0, i = 0;

	do {
		pr_info("%s \n", __func__);
		ret = p9222_reg_update(charger->client, P9222_COMMAND_REG, val, mask); // command
		if(ret >= 0) {
			msleep(250);
			ret = p9222_reg_read(charger->client, P9222_COMMAND_REG, &temp); // check out set bit exists
			if(ret < 0 || i > 3 )
				break;
		}
		i++;
	}while(temp != 0);
}

void p9222_send_eop(struct p9222_charger_data *charger, int healt_mode)
{
	int i = 0;
	int ret = 0;

	switch(healt_mode) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_OVERHEATLIMIT:
		case POWER_SUPPLY_HEALTH_COLD:
			if(charger->pdata->cable_type == SEC_WIRELESS_PAD_PMA) {
				pr_info("%s pma mode \n", __func__);
				for(i = 0; i < CMD_CNT; i++) {
					ret = p9222_reg_write(charger->client,
						P9222_END_POWER_TRANSFER_REG, P9222_EPT_END_OF_CHG);
					if(ret >= 0) {
						p9222_set_cmd_reg(charger,
							P9222_CMD_SEND_EOP_MASK, P9222_CMD_SEND_EOP_MASK);
						msleep(250);
					} else
						break;
				}
			} else {
				pr_info("%s wpc mode \n", __func__);
				for(i = 0; i < CMD_CNT; i++) {
					ret = p9222_reg_write(charger->client,
						P9222_END_POWER_TRANSFER_REG, P9222_EPT_OVER_TEMP);
					if(ret >= 0) {
						p9222_set_cmd_reg(charger,
							P9222_CMD_SEND_EOP_MASK, P9222_CMD_SEND_EOP_MASK);
						msleep(250);
					} else
						break;
				}
			}
			break;
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
#if 0
			pr_info("%s ept-reconfigure \n", __func__);
			ret = p9222_reg_write(charger->client, P9222_END_POWER_TRANSFER_REG, P9222_EPT_RECONFIG);
			if(ret >= 0) {
				p9222_set_cmd_reg(charger, P9222_CMD_SEND_EOP_MASK, P9222_CMD_SEND_EOP_MASK);
				msleep(250);
			}
#endif
			break;
		default:
			break;
	}
}

int p9222_send_cs100(struct p9222_charger_data *charger)
{
	int i = 0;
	int ret = 0;

	for(i = 0; i < CMD_CNT; i++) {
		ret = p9222_reg_write(charger->client, P9222_CHG_STATUS_REG, 100);
		if(ret >= 0) {
			p9222_set_cmd_reg(charger, P9222_CMD_SEND_CHG_STS_MASK, P9222_CMD_SEND_CHG_STS_MASK);
			msleep(250);
			ret = 1;
		} else {
			ret = -1;
			break;
		}
	}
	return ret;
}

void p9222_send_packet(struct p9222_charger_data *charger, u8 header, u8 rx_data_com, u8 *data_val, int data_size)
{
	int ret;
	int i;
	ret = p9222_reg_write(charger->client, P9222_PACKET_HEADER, header);
	ret = p9222_reg_write(charger->client, P9222_RX_DATA_COMMAND, rx_data_com);

	for(i = 0; i< data_size; i++) {
		ret = p9222_reg_write(charger->client, P9222_RX_DATA_VALUE0 + i, data_val[i]);
	}
	p9222_set_cmd_reg(charger, P9222_CMD_SEND_RX_DATA_MASK, P9222_CMD_SEND_RX_DATA_MASK);
}

static bool p9222_check_chg_in_status(struct p9222_charger_data *charger)
{
	union power_supply_propval value;
	bool ret = false;

	psy_do_property(charger->pdata->charger_name, get,
		POWER_SUPPLY_PROP_POWER_NOW, value);

	if (value.intval == ACOK_INPUT)
		ret = true;

	return ret;
}

static bool p9222_wait_chg_in_ok(struct p9222_charger_data *charger, int retry)
{
	bool chg_in_ok;
	do {
		if (p9222_is_on_pad(charger) == WPC_OFF_PAD) {
			pr_info("%s pad is off!\n", __func__);
			break;
		}

		chg_in_ok = p9222_check_chg_in_status(charger);

		pr_info("%s chg_in_ok = %d vout = %d retry = %d\n",
			__func__, chg_in_ok, charger->vout_status, retry);

		if (chg_in_ok)
			return true;
		msleep(300);

		retry--;
	} while(retry > 0);

	return false;
}

static void p9222_rx_ic_reset(struct p9222_charger_data *charger)
{
	int ping_durationg =
		(charger->pdata->ping_duration * 2) - (charger->pdata->ping_duration / 4);

	pr_info("%s ping_durationg = %d\n", __func__, ping_durationg);

	if (gpio_is_valid(charger->pdata->wpc_en)) {
		wake_lock(&charger->wpc_wake_lock);

		/* rx_ic reset need 5 pings when wpc_en high status */
		gpio_direction_output(charger->pdata->wpc_en, 1);
		msleep(ping_durationg);
		gpio_direction_output(charger->pdata->wpc_en, 0);
		wake_unlock(&charger->wpc_wake_lock);

		charger->charge_mode = P9222_CHARGE_MODE_NONE;
		p9222_wait_chg_in_ok(charger, 8);
	}
}

static void p9222_power_hold_mode_set(struct p9222_charger_data *charger, int set)
{
	int i = 0;

	if (charger->support_power_hold == 0) {
		pr_info("%s power hold not supported pad!\n", __func__);
		return;
	}

	pr_info("%s set = %d power_hold_mode = %d tx_id = %x\n", __func__,
		set, charger->power_hold_mode, charger->tx_id);

	wake_lock(&charger->wpc_wake_lock);

	if (set) {
		if (charger->power_hold_mode == 0) {
			charger->power_hold_mode = 1;
			for (i = 0; i < 3; i++) {
				p9222_reg_write(charger->client, P9222_PACKET_HEADER, 0x28);
				p9222_reg_write(charger->client, P9222_RX_DATA_COMMAND, 0x18);
				p9222_reg_write(charger->client, P9222_RX_DATA_VALUE0, 0x01);
				p9222_reg_write(charger->client, P9222_COMMAND_REG, 0x01);
				msleep(200);
			}
		}
	} else {
		if (charger->power_hold_mode == 1) {
			charger->power_hold_mode = 0;
			if (gpio_is_valid(charger->pdata->wpc_en)) {

				/* power hold mode exit need 2 pings when wpc_en high status */
				gpio_direction_output(charger->pdata->wpc_en, 1);
				msleep(charger->pdata->ping_duration * 2);
				gpio_direction_output(charger->pdata->wpc_en, 0);

				if (charger->wc_w_state == WPC_OFF_PAD) {
					pr_err("%s wpc detached!!\n", __func__);
					wake_unlock(&charger->wpc_wake_lock);
					return;
				}

				if (p9222_wait_chg_in_ok(charger, 10) == true) {
					/* for stable ap mode wirte after acok inserted */
					msleep(300);
					charger->charge_mode = P9222_CHARGE_MODE_NONE;

					if (p9222_check_chg_in_status(charger) == 0) {
						p9222_rx_ic_reset(charger);
						pr_err("%s chg_in err!\n", __func__);
					}
				} else {
					p9222_rx_ic_reset(charger);
				}
			} else {
				pr_err("%s wpc_en is invalid gpio!!\n", __func__);
			}
		}
	}

	wake_unlock(&charger->wpc_wake_lock);
	pr_info("%s end\n", __func__);
}

static int p9222_get_firmware_version(struct p9222_charger_data *charger, int firm_mode)
{
	int version = -1;
	int ret;
	u8 otp_fw_major[2] = {0,};
	u8 otp_fw_minor[2] = {0,};
	u8 tx_fw_major[2] = {0,};
	u8 tx_fw_minor[2] = {0,};

	if (p9222_is_on_pad(charger) == WPC_OFF_PAD)
		return version;

	switch (firm_mode) {
		case P9222_RX_FIRMWARE:
			ret = p9222_reg_read(charger->client, P9222_OTP_FW_MAJOR_REV_L_REG, &otp_fw_major[0]);
			ret = p9222_reg_read(charger->client, P9222_OTP_FW_MAJOR_REV_H_REG, &otp_fw_major[1]);
			if (ret >= 0) {
				version =  otp_fw_major[0] | (otp_fw_major[1] << 8);
			}
			pr_debug("%s rx major firmware version 0x%x \n", __func__, version);

			ret = p9222_reg_read(charger->client, P9222_OTP_FW_MINOR_REV_L_REG, &otp_fw_minor[0]);
			ret = p9222_reg_read(charger->client, P9222_OTP_FW_MINOR_REV_H_REG, &otp_fw_minor[1]);
			if (ret >= 0) {
				version =  otp_fw_minor[0] | (otp_fw_minor[1] << 8);
			}
			pr_debug("%s rx minor firmware version 0x%x \n", __func__, version);
			break;
		case P9222_TX_FIRMWARE:
			ret = p9222_reg_read(charger->client, P9222_SRAM_FW_MAJOR_REV_L_REG, &tx_fw_major[0]);
			ret = p9222_reg_read(charger->client, P9222_SRAM_FW_MAJOR_REV_H_REG, &tx_fw_major[1]);
			if (ret >= 0) {
				version =  tx_fw_major[0] | (tx_fw_major[1] << 8);
			}
			pr_debug("%s tx major firmware version 0x%x \n", __func__, version);

			ret = p9222_reg_read(charger->client, P9222_SRAM_FW_MINOR_REV_L_REG, &tx_fw_minor[0]);
			ret = p9222_reg_read(charger->client, P9222_SRAM_FW_MINOR_REV_H_REG, &tx_fw_minor[1]);
			if (ret >= 0) {
				version =  tx_fw_minor[0] | (tx_fw_minor[1] << 8);
			}
			pr_debug("%s tx minor firmware version 0x%x \n", __func__, version);
			break;
		default:
			pr_err("%s Wrong firmware mode \n", __func__);
			version = -1;
			break;
	}

	return version;
}

static void p9222_wpc_detach_check(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_detach_chk_work.work);
	int cnt = 0;
	bool is_charging;
	union power_supply_propval value;

	while (cnt < 1) {
		if (p9222_is_on_pad(charger) == WPC_ON_PAD) {
			is_charging = p9222_check_chg_in_status(charger);
			if ((charger->power_hold_mode == 0) && (is_charging == 0)) {
				pr_info("%s toggle wpc_en for acok glitch WA\n", __func__);
				p9222_rx_ic_reset(charger);
			}
		} else {
			psy_do_property(charger->pdata->battery_name, get,
				POWER_SUPPLY_PROP_ONLINE, value);
			pr_info("%s pad off!! cable_type(%d)\n", __func__, value.intval);

			if (is_wireless_type(value.intval)) {
				pr_info("%s send wpc_det_work\n", __func__);
				queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);
			}
			break;
		}
		cnt++;
	}
}

static void p9222_wpc_power_hold_check(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, power_hold_chk_work.work);

	if ((charger->power_hold_mode == 1) &&
		p9222_check_chg_in_status(charger)) {
		pr_err("%s abnormal chg status!! go to vout off or phm!!\n", __func__);
		charger->power_hold_mode = 0;
		p9222_full_charge(charger);
	}

	wake_unlock(&charger->power_hold_chk_lock);
}

static void p9222_wpc_wait_vout(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_wait_vout_work.work);
	union power_supply_propval value;
	static int cnt = 0;

	if (charger->wc_w_state == WPC_OFF_PAD) {
		wake_unlock(&charger->wpc_wait_vout_lock);
		cnt = 0;
		return;
	}

	psy_do_property(charger->pdata->charger_name, get,
		POWER_SUPPLY_PROP_POWER_NOW, value);

	pr_info("%s ACOK = %d cnt = %d\n", __func__, value.intval, cnt);

	if (value.intval == ACOK_INPUT) {
		cnt = 0;
		charger->charge_mode = P9222_CHARGE_MODE_NONE;
		p9222_ap_battery_monitor(charger, WPC_MODE_NORMAL);

		/* for stable charge acok insertd -> 0.5 sec delay -> charge on */
		msleep(500);

		if (charger->wc_w_state == WPC_OFF_PAD) {
			wake_unlock(&charger->wpc_wait_vout_lock);
			return;
		}

		value.intval = SEC_WIRELESS_PAD_WPC;
		psy_do_property(charger->pdata->charger_name, set,
			POWER_SUPPLY_EXT_PROP_WPC_ONLINE, value);

		/* if acok is inserted */
		charger->pdata->cable_type = P9222_PAD_MODE_WPC;
		value.intval = SEC_WIRELESS_PAD_WPC;
		psy_do_property(charger->pdata->wireless_name, set,
				POWER_SUPPLY_PROP_ONLINE, value);
	} else {
		if (cnt < 30) {
			cnt++;
			queue_delayed_work(charger->wqueue,
				&charger->wpc_wait_vout_work, msecs_to_jiffies(100));
			return;
		} else {
			pr_err("%s ACOK is not inserted!!\n", __func__);
			cnt = 0;
		}
	}

	wake_unlock(&charger->wpc_wait_vout_lock);
}

static void p9222_wpc_init(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_init_work.work);
	union power_supply_propval acok;
	int ret;
	int fw_ver;

	wake_lock(&charger->wpc_wake_lock);

	psy_do_property(charger->pdata->charger_name, get,
		POWER_SUPPLY_PROP_POWER_NOW, acok);

	pr_info("%s wpc sts val:%x acok:%d\n", __func__, charger->wc_w_state, acok.intval);

	if (charger->wc_w_state == WPC_ON_PAD) {
		if (acok.intval == 0) {
			if (charger->pdata->can_vbat_monitoring) {
				/* VRECT TARGET VOLTAGE = 5449mV(0x0427) */
				p9222_reg_write(charger->client, P9222_VRECT_TARGET_VOL_L, 0X27);
				p9222_reg_write(charger->client, P9222_VRECT_TARGET_VOL_H, 0X04);
			} else {
				p9222_reg_write(charger->client, P9222_VRECT_SET_REG, 126);
				p9222_reg_update(charger->client, 0x81, 0x80, 0x80); //set 7th bit in 0x81 to power on
			}
			p9222_reg_update(charger->client, P9222_COMMAND_REG,
				P9222_CMD_TOGGLE_LDO_MASK, P9222_CMD_TOGGLE_LDO_MASK);
		}

		/* check again firmare version support vbat monitoring */
		if (!charger->pdata->otp_firmware_ver) {
			fw_ver = p9222_get_firmware_version(charger, P9222_RX_FIRMWARE);
			if (fw_ver > 0) {
				pr_debug("%s rx major firmware version 0x%x\n", __func__, fw_ver);
				charger->pdata->otp_firmware_ver = fw_ver;
			}
		}
		queue_delayed_work(charger->wqueue,
				&charger->wpc_wait_vout_work,
				msecs_to_jiffies(100));
		schedule_delayed_work(&charger->wpc_isr_work, 0);

		ret = enable_irq_wake(charger->client->irq);
		if (ret < 0)
			pr_err("%s: Failed to Enable Wakeup Source(%d)\n", __func__, ret);
	}

	if (charger->wc_w_state == WPC_ON_PAD)
		p9222_wpc_auth_delayed_work(charger, WPC_AUTH_DELAY_TIME);

	wake_unlock(&charger->wpc_wake_lock);
}

static int p9222_temperature_check(struct p9222_charger_data *charger)
{
	if (charger->temperature >= charger->pdata->tx_off_high_temp) {
		/* send TX watchdog command in high temperature */
		pr_err("%s:HIGH TX OFF TEMP:%d\n", __func__, charger->temperature);
		p9222_reg_write(charger->client, P9222_PACKET_HEADER, 0x18);
		p9222_reg_write(charger->client, P9222_RX_DATA_COMMAND, 0xe7);
		p9222_reg_write(charger->client, P9222_COMMAND_REG, 0x01);
		p9222_reg_write(charger->client, P9222_PACKET_HEADER, 0x18);
		p9222_reg_write(charger->client, P9222_RX_DATA_COMMAND, 0xe7);
		p9222_reg_write(charger->client, P9222_COMMAND_REG, 0x01);
	}

	return 0;
}

static int p9222_monitor_work(struct p9222_charger_data *charger)
{
	int vrect;
	int vout;
	int freq;
	int temp;
	int iout;
	int vbat;
	u8 ap_mode, vbat_monitor;
	int capacity;
	union power_supply_propval value = {0, };

	if (p9222_is_on_pad(charger) == WPC_ON_PAD) {
		value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE;
		psy_do_property(charger->pdata->fuelgauge_name, get,
			POWER_SUPPLY_PROP_CAPACITY, value);
		capacity = value.intval;

		vrect = p9222_get_adc(charger, P9222_ADC_VRECT);
		vout = p9222_get_adc(charger, P9222_ADC_VOUT);
		freq = p9222_get_adc(charger, P9222_ADC_OP_FRQ);
		temp = p9222_get_adc(charger, P9222_ADC_DIE_TEMP);
		iout = p9222_get_adc(charger, P9222_ADC_RX_IOUT);
		vbat = p9222_get_adc(charger, P9222_ADC_VBAT_RAW);

		p9222_reg_read(charger->client, P9222_AP_BATTERY_MODE, &ap_mode);
		p9222_reg_read(charger->client, P9222_WPC_FLAG_REG, &vbat_monitor);

		pr_info("%s vrect(%dmV) vout(%dmV) iout(%dmA) vbat(%dmV) ap_mode(0x%02x) SOC(%d) "
			"pwr_hold(%d) chg_type(%d) chg_mode(%d) "
			"fw(%x) vbat_monitor(%x) tx_id(%x) freq(%d) temp(%d)\n",
			__func__, vrect, vout, iout, vbat, ap_mode, capacity,
			charger->power_hold_mode, charger->charger_type, charger->charge_mode,
			charger->pdata->otp_firmware_ver, vbat_monitor,
			charger->tx_id, freq, temp);
	} else {
		pr_info("%s pad off!\n", __func__);
	}

	return 0;
}

static int p9222_monitor_wdt_kick(struct p9222_charger_data *charger)
{
	if ((charger->pdata->watchdog_test == false) &&
		(charger->wc_w_state == WPC_ON_PAD)) {
		p9222_reg_update(charger->client, P9222_COMMAND_REG,
			P9222_CMD_SS_WATCHDOG_MASK, P9222_CMD_SS_WATCHDOG_MASK);
	}
	return 0;
}

static int p9222_write_ap_mode(struct p9222_charger_data *charger, int ap_mode)
{
	u8 data, i;
	int ret = -1;

	if (charger->curr_measure_mode == 1) {
		pr_info("%s current_measure_mode. skip ap mode setting!\n", __func__);
		return 0;
	}

	if (charger->need_margin == 1) {
		charger->need_margin = 0;
		msleep(150);
	}

	for (i = 0; i < 3; i++) {
		p9222_reg_write(charger->client, P9222_AP_BATTERY_MODE, ap_mode);
		p9222_reg_read(charger->client, P9222_AP_BATTERY_MODE, &data);

		if (ap_mode == data) {
			ret = 0;
			goto end;
		}
		msleep(100);
	}
end:
	pr_info("%s ap_mode %x read data = %x\n",
		__func__, ap_mode, data);
	return ret;
}

static int
p9222_compatible_cc_charge(struct p9222_charger_data *charger, bool prev_chk)
{
	int ret = 0;

	if (charger->store_mode) {
		ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_BATT_MODE);
	}

	if (prev_chk == false) {
		ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_CC_MODE);
	} else {
		if (charger->charge_mode != P9222_CHARGE_MODE_CC)
			ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_CC_MODE);
	}
	return ret;
}

static int
p9222_compatible_cv_charge(struct p9222_charger_data *charger, bool prev_chk)
{
	int ret = 0;

	if (prev_chk == false) {
		ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_CC_CV_MODE);
	} else {
		if (charger->charge_mode != P9222_CHARGE_MODE_CV)
			ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_CC_CV_MODE);
	}
	return ret;
}

static int
p9222_incompatible_cc_charge(struct p9222_charger_data *charger, bool prev_chk)
{
	int ret = 0;

	if (prev_chk == false) {
		ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_INCOMPATIBLE_CC_MODE);
	} else {
		if (charger->charge_mode != P9222_CHARGE_MODE_CC)
			ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_INCOMPATIBLE_CC_MODE);
	}
	return ret;
}

static int
p9222_incompatible_cv_charge(struct p9222_charger_data *charger, bool prev_chk)
{
	int ret = 0;

	if (prev_chk == false) {
		ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_INCOMPATIBLE_CC_CV_MODE);
	} else {
		if (charger->charge_mode != P9222_CHARGE_MODE_CV)
			ret = p9222_write_ap_mode(charger, P9222_AP_BATTERY_INCOMPATIBLE_CC_CV_MODE);
	}
	return ret;
}

static int p9222_power_hold_full_charge(struct p9222_charger_data *charger)
{
	pr_info("%s\n", __func__);
	p9222_power_hold_mode_set(charger, 1);
	return 0;
}

static int p9222_power_hold_re_charge(struct p9222_charger_data *charger)
{
	pr_info("%s\n", __func__);
	p9222_power_hold_mode_set(charger, 0);
	return 0;
}

static int p9222_normal_full_charge(struct p9222_charger_data *charger)
{
	pr_info("%s\n", __func__);
	wake_lock(&charger->wpc_wake_lock);
	p9222_ap_battery_monitor(charger, WPC_MODE_VOUT_OFF);
	msleep(200);
	p9222_reg_update(charger->client, P9222_COMMAND_REG,
		P9222_CMD_TOGGLE_LDO_MASK, P9222_CMD_TOGGLE_LDO_MASK);
	charger->power_hold_mode = 1;
	wake_unlock(&charger->wpc_wake_lock);
	return 0;
}

static int p9222_normal_re_charge(struct p9222_charger_data *charger)
{
	int i, vrect;

	pr_info("%s\n", __func__);
	wake_lock(&charger->wpc_wake_lock);

	p9222_reg_update(charger->client, P9222_COMMAND_REG,
		P9222_CMD_TOGGLE_LDO_MASK, P9222_CMD_TOGGLE_LDO_MASK);
	p9222_ap_battery_monitor(charger, WPC_MODE_IDT);

	if (p9222_wait_chg_in_ok(charger, 10) == true) {
		for (i = 0; i < 15; i++) {
			vrect = p9222_get_adc(charger, P9222_ADC_VRECT);
			pr_info("%s %d vrect = %d\n", __func__, i, vrect);

			/* IDT mode raise Vrect to 5.5V
			 * If CEP 10 is adjusted, IDT mode raise Vrect to 4.95V
			 * therefore, considering set marging wait Vrect until over 4.9V
			 */
			if (vrect > 4900)
				break;

			msleep(100);
		}
	}
	charger->power_hold_mode = 0;

	pr_info("%s end\n", __func__);
	wake_unlock(&charger->wpc_wake_lock);
	return 0;
}

static int p9222_cc_charge(struct p9222_charger_data *charger, bool prev_chk)
{
	int ret = -1;
	enum p9222_charger_pad_type type = charger->charger_type;

	if (charger->charge_cb_func[type]->cc_chg) {
		ret = charger->charge_cb_func[type]->cc_chg(charger, prev_chk);
		charger->charge_mode = P9222_CHARGE_MODE_CC;
	} else
		pr_err("%s invalid func\n", __func__);
	return ret;
}

static int p9222_cv_charge(struct p9222_charger_data *charger, bool prev_chk)
{
	int ret = -1;
	enum p9222_charger_pad_type type = charger->charger_type;

	if (charger->charge_cb_func[type]->cv_chg) {
		ret = charger->charge_cb_func[type]->cv_chg(charger, prev_chk);
		charger->charge_mode = P9222_CHARGE_MODE_CV;
	} else
		pr_err("%s invalid func\n", __func__);
	return ret;
}

static int p9222_full_charge(struct p9222_charger_data *charger)
{
	int ret = -1;
	int i;
	enum p9222_charger_pad_type type = charger->charger_type;
	union power_supply_propval value = {0, };

	pr_info("%s type = %d charger->charge_mode = %d\n",
		__func__, type, charger->charge_mode);

	if (charger->tx_id == 0) {
		for (i = 0; i < 50; i++) {
			if (charger->tx_id != 0) {
				break;
			}
			pr_info("%s %d tx_id = %d\n", __func__, i, charger->tx_id);
			msleep(100);
		}
	}

	mutex_lock(&charger->charger_lock);

	if (charger->charge_cb_func[type]->full_chg)  {
		ret = charger->charge_cb_func[type]->full_chg(charger);
		charger->charge_mode = P9222_CHARGE_MODE_OFF;
		charger->pdata->is_charging = 0;
		value.intval = SEC_WIRELESS_PAD_NONE;
		psy_do_property(charger->pdata->charger_name, set,
			POWER_SUPPLY_EXT_PROP_WPC_ONLINE, value);
	} else
		pr_err("%s invalid func\n", __func__);

	mutex_unlock(&charger->charger_lock);
	return ret;
}

static int p9222_re_charge(struct p9222_charger_data *charger)
{
	int ret = -1;
	bool chg_in_ok;
	union power_supply_propval value = {0, };
	enum p9222_charger_pad_type type = charger->charger_type;

	chg_in_ok = p9222_check_chg_in_status(charger);

	if (chg_in_ok) {
		pr_info("%s skip. already charge started!\n", __func__);
		return 0;
	}

	pr_info("%s type = %d charger->charge_mode = %d\n",
		__func__, type, charger->charge_mode);

	mutex_lock(&charger->charger_lock);

	if (charger->charge_cb_func[type]->re_chg) {
		charger->charge_mode = P9222_CHARGE_MODE_NONE;
		ret = charger->charge_cb_func[type]->re_chg(charger);

		if (p9222_is_on_pad(charger) == WPC_ON_PAD) {
			charger->pdata->is_charging = 1;
			charger->need_margin = 1;
			value.intval = SEC_WIRELESS_PAD_WPC;
			psy_do_property(charger->pdata->charger_name, set,
				POWER_SUPPLY_EXT_PROP_WPC_ONLINE, value);

			p9222_wpc_request_tx_id(charger, 3);
		}
	} else
		pr_err("%s invalid func\n", __func__);

	mutex_unlock(&charger->charger_lock);
	return ret;
}

static void p9222_cc_cv_charge_select(
	struct p9222_charger_data *charger, int capacity, bool prev_check)
{
	if (capacity >= charger->pdata->cc_cv_threshold) {
		p9222_cv_charge(charger, prev_check);
	} else {
		/* to avoid setting cc mode when once cv mode entered and not detached */
		if ((prev_check == true) && (charger->charge_mode == P9222_CHARGE_MODE_CV)) {
			pr_info("%s forcely set cv mode for temp capacity drop\n", __func__);
			p9222_cv_charge(charger, prev_check);
		} else {
			p9222_cc_charge(charger, prev_check);
		}
	}
}

static int p9222_ap_battery_monitor(struct p9222_charger_data *charger, int mode)
{
	union power_supply_propval value = {0, };
	int capacity;
	u8 ap_battery_mode;
	int ret = 0;
	struct power_supply *batt_psy = NULL;

	batt_psy = get_power_supply_by_name(charger->pdata->battery_name);
	if (batt_psy == NULL)
		return 0;

	psy_do_property(charger->pdata->battery_name, get,
		POWER_SUPPLY_EXT_PROP_BOOT_BOOST_CHARGE, value);

	if ((value.intval == 1) && (mode != WPC_MODE_BOOT)) {
		pr_info("%s return by boot boost charge!!\n", __func__);
		return ret;
	}

	value.intval = SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE;
	psy_do_property(charger->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CAPACITY, value);
	capacity = value.intval;

	ret = p9222_reg_read(charger->client, P9222_AP_BATTERY_MODE,
		&ap_battery_mode);

	switch (mode) {
	case WPC_MODE_BOOT:
		p9222_write_ap_mode(charger, P9222_AP_BATTERY_BOOT_MODE);
		charger->charge_mode = P9222_CHARGE_MODE_NONE;
		break;
	case WPC_MODE_IDT:
		p9222_write_ap_mode(charger, P9222_AP_BATTERY_IDT_MODE);
		charger->charge_mode = P9222_CHARGE_MODE_NONE;
		break;
	case WPC_MODE_VOUT_OFF:
		p9222_write_ap_mode(charger, P9222_AP_BATTERY_INCOMPATIBLE_PHP_MODE);
		break;
	case WPC_MODE_ATTACH:
		p9222_cc_cv_charge_select(charger, capacity, false);
		break;
	case WPC_MODE_SWELL_ENTER:
	case WPC_MODE_SWELL_EXIT:
	case WPC_MODE_WAKEUP:
		if (ap_battery_mode == P9222_AP_BATTERY_IDT_MODE)
			p9222_cc_cv_charge_select(charger, capacity, false);

		p9222_write_ap_mode(charger, P9222_AP_BATTERY_WAKEUP_MODE);
		if (mode != WPC_MODE_WAKEUP)
			msleep(200);
		break;
	case WPC_MODE_NORMAL:
		if (charger->pdata->otp_firmware_ver == 0x125) {
			if (charger->wpc_wakeup_wa == 0)
				p9222_cc_cv_charge_select(charger, capacity, true);
		} else {
			p9222_cc_cv_charge_select(charger, capacity, true);
		}
		break;
	default:
		break;
	};

	ret = p9222_reg_read(charger->client, P9222_AP_BATTERY_MODE,
		&ap_battery_mode);

	if (ret < 0 || ap_battery_mode == 0xFF) {
		ret = -1;
		pr_err("%s ret = %d ap_battery_mode = 0x%x\n",
			__func__, ret, ap_battery_mode);
	}

	return ret;
}

static int p9222_power_hold_mode_monitor(struct p9222_charger_data *charger)
{
	union power_supply_propval value = {0, };
	int health;
	int status;
	int chg_mode;
	int swelling_mode;
	int slate_mode;

	wake_lock(&charger->wpc_wake_lock);

	psy_do_property(charger->pdata->battery_name, get,
		POWER_SUPPLY_PROP_HEALTH, value);
	health = value.intval;

	psy_do_property(charger->pdata->battery_name, get,
		POWER_SUPPLY_PROP_STATUS, value);
	status = value.intval;

	psy_do_property(charger->pdata->battery_name, get,
		POWER_SUPPLY_PROP_CHARGE_NOW, value);
	chg_mode = value.intval;

	psy_do_property(charger->pdata->battery_name, get,
		POWER_SUPPLY_EXT_PROP_SWELLING_MODE, value);
	swelling_mode = value.intval;

	psy_do_property(charger->pdata->battery_name, get,
		POWER_SUPPLY_EXT_PROP_SLATE_MODE, value);
	slate_mode = value.intval;

	if (charger->wc_w_state == WPC_OFF_PAD) {
		pr_err("%s exit by pad off!!\n", __func__);
		goto end;
	}

	pr_info("%s health(%d %d) sts(%d %d) chg_mode(%d %d) swell(%d %d)\n",
		__func__,
		charger->battery_health, health,
		charger->battery_status, status,
		charger->battery_chg_mode, chg_mode,
		charger->swelling_mode, swelling_mode);

	if ((chg_mode == SEC_BATTERY_CHARGING_NONE) &&
		(charger->battery_chg_mode == SEC_BATTERY_CHARGING_2ND)){
		p9222_full_charge(charger);
	} else if (((health == POWER_SUPPLY_HEALTH_OVERHEAT) ||
		(health == POWER_SUPPLY_HEALTH_COLD)) &&
		(charger->battery_health == POWER_SUPPLY_HEALTH_GOOD)) {
		p9222_full_charge(charger);
	} else if ((charger->battery_status == POWER_SUPPLY_STATUS_NOT_CHARGING) &&
		(status == POWER_SUPPLY_STATUS_CHARGING)) {
		psy_do_property(charger->pdata->battery_name, get,
			POWER_SUPPLY_EXT_PROP_COOL_DOWN_MODE, value);
		if (value.intval == 0)
			p9222_re_charge(charger);
	}

	if (charger->swelling_mode != swelling_mode) {
		if ((charger->swelling_mode != SWELLING_MODE_FULL) &&
			(swelling_mode == SWELLING_MODE_FULL)) {
			p9222_full_charge(charger);
		}
	}

	if (charger->slate_mode != slate_mode) {
		if (slate_mode == 1) {
			p9222_full_charge(charger);
		}
	}

end:
	charger->battery_health = health;
	charger->battery_status = status;
	charger->battery_chg_mode = chg_mode;
	charger->swelling_mode = swelling_mode;
	charger->slate_mode = slate_mode;

	wake_unlock(&charger->wpc_wake_lock);

	return 0;
}

static int p9222_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct p9222_charger_data *charger =
		power_supply_get_drvdata(psy);
	int ret;
	enum power_supply_ext_property ext_psp = psp;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			ret = p9222_get_firmware_version(charger, P9222_RX_FIRMWARE);
			pr_info("%s rx major firmware version 0x%x \n", __func__, ret);

			if (ret >= 0)
				val->intval = 1;
			else
				val->intval = 0;

			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
		case POWER_SUPPLY_PROP_HEALTH:
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		case POWER_SUPPLY_PROP_CHARGE_POWERED_OTG_CONTROL:
			return -ENODATA;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
				break;
		case POWER_SUPPLY_PROP_ONLINE:
			pr_debug("%s cable_type =%d \n ", __func__, charger->pdata->cable_type);
			val->intval = charger->pdata->cable_type;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = p9222_is_on_pad(charger);
			pr_debug("%s is on chg pad = %d \n ", __func__, val->intval);
			break;
		case POWER_SUPPLY_PROP_TEMP:
			val->intval = charger->temperature;
			break;
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			val->intval = charger->pdata->charging_mode;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			if(charger->pdata->ic_on_mode || (charger->wc_w_state == WPC_ON_PAD)) {
				val->intval = p9222_get_adc(charger, P9222_ADC_RX_IOUT);
			} else
				val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_ENERGY_NOW: /* vout */
			if(charger->pdata->ic_on_mode || (charger->wc_w_state == WPC_ON_PAD)) {
				val->intval = p9222_get_adc(charger, P9222_ADC_VOUT);
			} else
				val->intval = 0;
			break;

		case POWER_SUPPLY_PROP_ENERGY_AVG: /* vrect */
			if(charger->pdata->ic_on_mode || (charger->wc_w_state == WPC_ON_PAD)) {
				val->intval = p9222_get_adc(charger, P9222_ADC_VRECT);
			} else
				val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			pr_info("%s charger->pdata->is_charging = %d\n",
				__func__, charger->pdata->is_charging);
			val->intval = charger->pdata->is_charging;
			break;
		case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
			switch (ext_psp) {
				case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
					p9222_monitor_work(charger);
					val->intval = 0;
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

static int p9222_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct p9222_charger_data *charger =
		power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = psp;
	int vout_status = 0;

	pr_debug("%s: psp=%d val=%d\n", __func__, psp, val->intval);

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			if(val->intval == POWER_SUPPLY_STATUS_FULL) {
				pr_info("%s set green led \n", __func__);
				charger->pdata->cs100_status = p9222_send_cs100(charger);
				charger->pdata->cs100_status = p9222_send_cs100(charger);
			}
			break;
		case POWER_SUPPLY_PROP_CHARGE_TYPE:
			if (p9222_is_on_pad(charger) == WPC_ON_PAD) {
				msleep(250);
				pr_info("%s: Charger interrupt occured during lpm \n", __func__);
				queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);
			}
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			break;
		case POWER_SUPPLY_PROP_TEMP:
			charger->temperature = val->intval;
			p9222_temperature_check(charger);
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
			break;
		case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
			if(val->intval) {
				charger->pdata->ic_on_mode = true;
			} else {
				charger->pdata->ic_on_mode = false;
			}
			break;
		case POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION:
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			break;
		case POWER_SUPPLY_PROP_ENERGY_NOW:
			break;
		case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
			switch (ext_psp) {
				case POWER_SUPPLY_EXT_PROP_WDT_KICK:
					p9222_monitor_wdt_kick(charger);
					break;
				case POWER_SUPPLY_EXT_PROP_WIRELESS_CHARGE_MONITOR:
					p9222_ap_battery_monitor(charger, val->intval);
					p9222_power_hold_mode_monitor(charger);
					break;
				case POWER_SUPPLY_EXT_PROP_IS_RECHARGE:
					vout_status = p9222_check_chg_in_status(charger);
					pr_info("%s pwr_hold = %d vout_status = %d intval = %d\n",
						__func__, charger->power_hold_mode,
						vout_status, val->intval);
					if ((vout_status == 0) && (val->intval == 1)) {
						wake_lock(&charger->wpc_wake_lock);
						p9222_re_charge(charger);
						wake_unlock(&charger->wpc_wake_lock);
					}
					break;
				case POWER_SUPPLY_EXT_PROP_WPC_VOUT_OFF:
					vout_status = p9222_check_chg_in_status(charger);
					if (vout_status == 1) {
						wake_lock(&charger->wpc_wake_lock);
						p9222_full_charge(charger);
						wake_unlock(&charger->wpc_wake_lock);
					}
					break;
				case POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA:
					break;
				case POWER_SUPPLY_EXT_PROP_STORE_MODE:
					charger->store_mode = val->intval;
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

static void p9222_wpc_opfq_work(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_opfq_work.work);

	u16 op_fq;
	u8 pad_mode;
	union power_supply_propval value;

	p9222_reg_read(charger->client, P9222_SYS_OP_MODE_REG, &pad_mode);
	if (pad_mode == P9222_PAD_MODE_WPC) {
		op_fq = p9222_get_adc(charger, P9222_ADC_OP_FRQ);
			pr_info("%s: Operating FQ %dkHz(0x%x)\n", __func__, op_fq, op_fq);
		if (op_fq > 190) { /* wpc threshold 190kHz */
				pr_info("%s: Reset M0\n",__func__);
				p9222_reg_write(charger->client, 0x3040, 0x80); /*restart M0 */

				charger->pdata->opfq_cnt++;
				if (charger->pdata->opfq_cnt <= CMD_CNT) {
				queue_delayed_work(charger->wqueue,
					&charger->wpc_opfq_work, msecs_to_jiffies(10000));
					return;
				}
			}
	} else if (pad_mode == P9222_PAD_MODE_PMA) {
			charger->pdata->cable_type = P9222_PAD_MODE_PMA;
			value.intval = SEC_WIRELESS_PAD_PMA;
			psy_do_property(charger->pdata->wireless_name,
				set, POWER_SUPPLY_PROP_ONLINE, value);
	}

	charger->pdata->opfq_cnt = 0;
	wake_unlock(&charger->wpc_opfq_lock);
}

static void p9222_wpc_id_request_work(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_id_request_work.work);
	int pending_work = delayed_work_pending(&charger->wpc_auth_check_work);

	pr_info("%s pending_work = %d wpc_id_request_step = %d\n",
		__func__, pending_work, charger->wpc_id_request_step);

	if (charger->wpc_id_request_step > 1) {
		wake_unlock(&charger->wpc_id_request_lock);
		return;
	}

	if (p9222_is_on_pad(charger) == WPC_ON_PAD) {
		if (!pending_work)
			p9222_wpc_request_tx_id(charger, 3);
	}

	wake_unlock(&charger->wpc_id_request_lock);

	pending_work = delayed_work_pending(&charger->wpc_auth_check_work);

	pr_info("%s 2nd pending_work = %d wpc_id_request_step = %d\n",
		__func__, pending_work, charger->wpc_id_request_step);

	if (!pending_work &&
		(charger->wpc_id_request_step == 0) &&
		(p9222_is_on_pad(charger) == WPC_ON_PAD)) {
		charger->wpc_id_request_step++;
		wake_lock_timeout(&charger->wpc_id_request_lock, 15 * HZ);
		queue_delayed_work(charger->wqueue,
			&charger->wpc_id_request_work, msecs_to_jiffies(10000));
	}
}

static void p9222_wpc_wa_work(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_wakeup_wa_work.work);
	u8 data;
	p9222_reg_read(charger->client, P9222_AP_BATTERY_MODE, &data);

	pr_info("%s ap_mode = %x\n", __func__, data);

	charger->wpc_wakeup_wa = 0;
	p9222_ap_battery_monitor(charger, WPC_MODE_NORMAL);
	wake_unlock(&charger->wpc_wakeup_wa_lock);
}

static void p9222_wpc_det_work(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_det_work.work);
	int wc_w_state, prev_wc_state;
	union power_supply_propval value;
	u8 pad_mode;
	u8 vrect;

	wake_lock(&charger->wpc_wake_lock);

	wc_w_state = p9222_is_on_pad(charger);

	prev_wc_state = charger->wc_w_state;
	charger->wc_w_state = wc_w_state;

	if ((prev_wc_state == WPC_OFF_PAD) && (wc_w_state == WPC_ON_PAD)) {
		charger->pdata->vout_status = P9222_VOUT_5V;

		/* set fod value */
		if(charger->pdata->fod_data_check)
			p9222_fod_set(charger);

		/* enable Mode Change INT */
		p9222_reg_update(charger->client, P9222_INT_ENABLE_L_REG,
						P9222_STAT_MODE_CHANGE_MASK, P9222_STAT_MODE_CHANGE_MASK);

		/* read vrect adjust */
		p9222_reg_read(charger->client, P9222_VRECT_SET_REG, &vrect);

		pr_info("%s: wpc activated\n",__func__);

		/* read pad mode */
		p9222_reg_read(charger->client, P9222_SYS_OP_MODE_REG, &pad_mode);
		if(pad_mode == P9222_SYS_MODE_PMA) {
			charger->pdata->cable_type = P9222_PAD_MODE_PMA;
			value.intval = SEC_WIRELESS_PAD_PMA;
			psy_do_property(charger->pdata->wireless_name, set,
					POWER_SUPPLY_PROP_ONLINE, value);
		} else {
			cancel_delayed_work(&charger->wpc_wait_vout_work);
			wake_lock_timeout(&charger->wpc_wait_vout_lock, HZ * 5);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_wait_vout_work,
				msecs_to_jiffies(VOUT_STABLILZATION_TIME));

			wake_lock(&charger->wpc_opfq_lock);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_opfq_work, msecs_to_jiffies(10000));

			/* TX FW 0707 does not send TX ID interrupt somtimes.
			 * If TX ID interrupt not occured, send TX ID request after 3sec.
			 * TX ID interrupt occured after 1 ~ 1.5c after PDETB low.
			 */
			charger->wpc_id_request_step = 0;
			wake_lock_timeout(&charger->wpc_id_request_lock, 5 * HZ);
			queue_delayed_work(charger->wqueue,
				&charger->wpc_id_request_work, msecs_to_jiffies(3000));

			p9222_wpc_auth_delayed_work(charger, WPC_AUTH_DELAY_TIME);
		}
		charger->pdata->is_charging = 1;
	} else if ((prev_wc_state == WPC_ON_PAD) &&
		(wc_w_state == WPC_OFF_PAD)) {

		charger->pdata->cable_type = P9222_PAD_MODE_NONE;
		charger->pdata->is_charging = 0;
		charger->pdata->vout_status = P9222_VOUT_0V;
		charger->pdata->opfq_cnt = 0;
		charger->charge_mode = P9222_CHARGE_MODE_NONE;
		charger->power_hold_mode = 0;
		charger->support_power_hold = 0;
		charger->wpc_wakeup_wa = 0;
		charger->tx_id = 0x0;

		if (charger->incompatible_tx) {
			charger->incompatible_tx = 0;
			value.intval = 0;
			psy_do_property(charger->pdata->battery_name, set,
				POWER_SUPPLY_EXT_PROP_INCOMPATIBLE_WPC, value);
		}

		value.intval = SEC_WIRELESS_PAD_NONE;
		psy_do_property(charger->pdata->wireless_name, set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc deactivated, set V_INT as PD\n",__func__);

		cancel_delayed_work(&charger->wpc_isr_work);

		if(delayed_work_pending(&charger->wpc_opfq_work)) {
			wake_unlock(&charger->wpc_opfq_lock);
			cancel_delayed_work(&charger->wpc_opfq_work);
		}

		if(delayed_work_pending(&charger->wpc_wait_vout_work)) {
			wake_unlock(&charger->wpc_wait_vout_lock);
			cancel_delayed_work(&charger->wpc_wait_vout_work);
		}

		if (wake_lock_active(&charger->wpc_auth_check_lock))
			wake_unlock(&charger->wpc_auth_check_lock);

		if (delayed_work_pending(&charger->wpc_auth_check_work))
			cancel_delayed_work(&charger->wpc_auth_check_work);

		if (wake_lock_active(&charger->wpc_id_request_lock))
			wake_unlock(&charger->wpc_id_request_lock);

		if (delayed_work_pending(&charger->wpc_id_request_work))
			cancel_delayed_work(&charger->wpc_id_request_work);

		charger->wpc_id_request_step = 0;
	}

	pr_info("%s: w(%d to %d)\n", __func__, prev_wc_state, wc_w_state);
	wake_unlock(&charger->wpc_wake_lock);
}

static enum alarmtimer_restart p9222_wpc_ph_mode_alarm(struct alarm *alarm, ktime_t now)
{
	struct p9222_charger_data *charger = container_of(alarm,
				struct p9222_charger_data, polling_alarm);

	union power_supply_propval value;

	value.intval = false;
	psy_do_property(charger->pdata->charger_name, set,
		POWER_SUPPLY_PROP_PRESENT, value);
	pr_info("%s: wpc ph mode ends.\n", __func__);
	wake_unlock(&charger->wpc_wake_lock);

	return ALARMTIMER_NORESTART;
}

static int p9222_compatible_tx_check(struct p9222_charger_data *charger, int id)
{
	int i;
	for (i = 0; i < charger->pdata->num_compatible_tx; i++) {
		pr_info("%s input id = %x table_id %x\n", __func__, id,
				charger->pdata->charger_type[i].compatible_tx_id);
		if (id == charger->pdata->charger_type[i].compatible_tx_id) {
			pr_info("%s id = %x %d\n", __func__, id,
				charger->pdata->charger_type[i].support_power_hold);
			charger->support_power_hold =
				charger->pdata->charger_type[i].support_power_hold;
			return 1;
		}
	}
	charger->support_power_hold = 0;
	return 0;
}

static void p9222_wpc_request_tx_id(struct p9222_charger_data *charger, int cnt)
{
	int i;
	for (i = 0; i < cnt; i++) {
		pr_info("%s requst TX-ID to TX (cnt %d)\n", __func__, i);
		p9222_reg_write(charger->client, P9222_PACKET_HEADER, P9222_HEADER_AFC_CONF); // using 2 bytes packet property
		p9222_reg_write(charger->client, P9222_RX_DATA_COMMAND, P9222_RX_DATA_COM_REQ_TX_ID);
		p9222_reg_write(charger->client, P9222_RX_DATA_VALUE0, 0x0);
		p9222_reg_write(charger->client, P9222_COMMAND_REG, P9222_CMD_SEND_RX_DATA);

		if (cnt > 1)
			msleep(300);
	}
}

static void p9222_curr_measure_work(struct work_struct *work)
{
	struct p9222_charger_data *charger = container_of(work,
		struct p9222_charger_data, curr_measure_work.work);
	union power_supply_propval value = {0,};
	int i = 0;
	int acok_cnt = 0;
	bool valid_acok = false;

	pr_info("%s\n", __func__);

	while (i < 100) {
		if (p9222_is_on_pad(charger) == WPC_OFF_PAD) {
			pr_info("%s pad is off!\n", __func__);
			break;
		}

		if (p9222_check_chg_in_status(charger) == true) {
			if (acok_cnt > 10) {
				pr_info("%s valid acok. go to measurement mode\n", __func__);
				valid_acok = true;
				break;
			}
			acok_cnt++;
		} else {
			acok_cnt = 0;
		}
		i++;
		msleep(100);
	}

	if (!valid_acok) {
		pr_err("%s acok not stable.\n", __func__);
		return;
	}

	p9222_write_ap_mode(charger, P9222_AP_BATTERY_CURR_MEASURE_MODE);
	charger->curr_measure_mode = 1;

	psy_do_property(charger->pdata->charger_name, set,
		POWER_SUPPLY_EXT_PROP_FORCED_JIG_MODE, value);
}

static void p9222_wpc_isr_work(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_isr_work.work);

	u8 cmd_data, val_data, data;
	union power_supply_propval value;

	if (charger->wc_w_state == WPC_OFF_PAD) {
		pr_info("%s wc_w_state is 0. exit wpc_isr_work.\n",__func__);
		return;
	}

	if (p9222_check_chg_in_status(charger) == false) {
		pr_info("%s acok is 0. exit wpc_isr_work.\n",__func__);
		return;
	}

	wake_lock(&charger->wpc_wake_lock);

	p9222_reg_read(charger->client, P9222_TX_DATA_COMMAND, &cmd_data);
	p9222_reg_read(charger->client, P9222_TX_DATA_VALUE0, &val_data);

	pr_info("%s cmd(%x) val(%x)\n", __func__, cmd_data, val_data);
	charger->tx_id = val_data;

	charger->tx_id_checked = 0;

	if (cmd_data == P9222_TX_DATA_COM_TX_ID) {
		if (p9222_compatible_tx_check(charger, val_data)) {
			pr_info("%s data = 0x%x, compatible TX pad\n", __func__, val_data);
			value.intval = 1;
			psy_do_property(charger->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);
			psy_do_property(charger->pdata->battery_name, set,
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);

			if (charger->support_power_hold)
				charger->charger_type = P9222_CHARGER_TYPE_COMPATIBLE;
			else
				charger->charger_type = P9222_CHARGER_TYPE_MULTI;

			value.intval = 0;
			charger->incompatible_tx = 0;
			psy_do_property(charger->pdata->battery_name, set,
				POWER_SUPPLY_EXT_PROP_INCOMPATIBLE_WPC, value);

			charger->tx_id_checked = 1;
		} else if (val_data == P9222_FACTORY_MODE_TX_ID) {
			pr_info("%s id 0x%x current measure TX pad\n", __func__, val_data);
			charger->charger_type = P9222_CHARGER_TYPE_COMPATIBLE;
			schedule_delayed_work(&charger->curr_measure_work,
				msecs_to_jiffies(2000));
		} else {
			charger->charger_type = P9222_CHARGER_TYPE_INCOMPATIBLE;
			pr_info("%s incompatible TX pad\n", __func__);
		}

		p9222_reg_read(charger->client, P9222_INT_L_REG, &data);
		if (data & P9222_STAT_TX_DATA_RECEIVED_MASK) {
			p9222_reg_write(charger->client, P9222_INT_CLEAR_L_REG,
				P9222_INT_TX_DATA_RECEIVED);
		}

	} else if (cmd_data == 0x19) {
		pr_info("%s incompatible TX pad\n", __func__);
		charger->charger_type = P9222_CHARGER_TYPE_INCOMPATIBLE;

		p9222_reg_read(charger->client, P9222_INT_L_REG, &data);
		if (data & P9222_STAT_TX_DATA_RECEIVED_MASK) {
			p9222_reg_write(charger->client, P9222_INT_CLEAR_L_REG,
				P9222_INT_TX_DATA_RECEIVED);
		}
	}

	p9222_wpc_request_tx_id(charger, 3);
	wake_unlock(&charger->wpc_wake_lock);

	p9222_wpc_auth_delayed_work(charger, WPC_AUTH_DELAY_TIME);
}

static irqreturn_t p9222_wpc_det_irq_thread(int irq, void *irq_data)
{
	struct p9222_charger_data *charger = irq_data;

	pr_info("%s !\n",__func__);

	while(charger->wqueue == NULL) {
		pr_err("%s wait wqueue created!!\n", __func__);
		msleep(10);
	}

	queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);

	return IRQ_HANDLED;
}

static void p9222_enable_vbat_monitoring(struct p9222_charger_data *charger)
{
	if (charger->pdata->can_vbat_monitoring) {
		p9222_reg_write(charger->client, P9222_VRECT_SET_REG, 9);
		p9222_reg_update(charger->client, P9222_WPC_FLAG_REG,
			P9222_VBAT_MONITORING_MODE, P9222_VBAT_MONITORING_MODE_MASK);
	}
}

static irqreturn_t p9222_wpc_irq_thread(int irq, void *irq_data)
{
	struct p9222_charger_data *charger = irq_data;
	int ret;
	u8 irq_src[2];
	u8 reg_data;

	wake_lock(&charger->wpc_wake_lock);
	mutex_lock(&charger->charger_lock);

	pr_info("%s\n", __func__);

	/* check again firmare version support vbat monitoring */
	if (!charger->pdata->otp_firmware_ver) {
		ret = p9222_get_firmware_version(charger, P9222_RX_FIRMWARE);
		if (ret > 0) {
			pr_debug("%s rx major firmware version 0x%x\n", __func__, ret);
			charger->pdata->otp_firmware_ver = ret;
		}
	}

	p9222_enable_vbat_monitoring(charger);

	ret = p9222_reg_read(charger->client, P9222_INT_L_REG, &irq_src[0]);
	ret = p9222_reg_read(charger->client, P9222_INT_H_REG, &irq_src[1]);

	if (ret < 0) {
		pr_err("%s: Failed to read interrupt source: %d\n",
			__func__, ret);
		wake_unlock(&charger->wpc_wake_lock);
		mutex_unlock(&charger->charger_lock);
		return IRQ_NONE;
	}

	pr_info("%s: interrupt source(0x%x)\n", __func__, irq_src[1] << 8 | irq_src[0]);

	if(irq_src[0] & P9222_STAT_MODE_CHANGE_MASK) {
		pr_info("%s MODE CHANGE IRQ ! \n", __func__);
		ret = p9222_reg_read(charger->client, P9222_SYS_OP_MODE_REG, &reg_data);
	}

	if(irq_src[0] & P9222_STAT_VOUT_MASK) {
		pr_info("%s Vout IRQ ! \n", __func__);
		ret = p9222_reg_read(charger->client, P9222_INT_STATUS_L_REG, &reg_data);
		if (reg_data & P9222_INT_STAT_VOUT)
			charger->vout_status = 1;
		else
			charger->vout_status = 0;
		pr_info("%s vout_status = %d\n", __func__, charger->vout_status);
	}

	if(irq_src[0] & P9222_STAT_TX_DATA_RECEIVED_MASK) {
		pr_info("%s TX RECIVED IRQ ! \n", __func__);
		if(!delayed_work_pending(&charger->wpc_isr_work) &&
			charger->pdata->cable_type != P9222_PAD_MODE_WPC_AFC )
			schedule_delayed_work(&charger->wpc_isr_work, msecs_to_jiffies(1000));
	}

	if(irq_src[1] & P9222_STAT_OVER_CURR_MASK) {
		pr_info("%s OVER CURRENT IRQ ! \n", __func__);
	}

	if(irq_src[1] & P9222_STAT_OVER_TEMP_MASK) {
		pr_info("%s OVER TEMP IRQ ! \n", __func__);
	}

	if(irq_src[1] & P9222_STAT_TX_CONNECT_MASK) {
		pr_info("%s TX CONNECT IRQ ! \n", __func__);
		charger->pdata->tx_status = SEC_TX_POWER_TRANSFER;
	}

	if ((irq_src[1] << 8 | irq_src[0]) != 0) {
		msleep(5);

		/* clear intterupt */
		p9222_reg_write(charger->client, P9222_INT_CLEAR_L_REG, irq_src[0]);
		p9222_reg_write(charger->client, P9222_INT_CLEAR_H_REG, irq_src[1]);
		p9222_set_cmd_reg(charger, 0x20, P9222_CMD_CLEAR_INT_MASK); // command
	}
	/* debug */
	wake_unlock(&charger->wpc_wake_lock);
	mutex_unlock(&charger->charger_lock);

	return IRQ_HANDLED;
}

enum {
	WPC_FW_VER = 0,
	WPC_IBUCK,
	WPC_WATCHDOG,
	WPC_ADDR,
	WPC_SIZE,
	WPC_DATA,
};


static struct device_attribute p9222_attributes[] = {
	SEC_WPC_ATTR(fw),
	SEC_WPC_ATTR(ibuck),
	SEC_WPC_ATTR(watchdog_test),
	SEC_WPC_ATTR(addr),
	SEC_WPC_ATTR(size),
	SEC_WPC_ATTR(data),
};

ssize_t sec_wpc_show_attrs(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct p9222_charger_data *charger =
		power_supply_get_drvdata(psy);

	const ptrdiff_t offset = attr - p9222_attributes;
	u8 data;
	int i, count = 0;

	switch (offset) {
		case WPC_FW_VER:
			{
				int ret =0;
				int version =0;

				ret = p9222_get_firmware_version(charger, P9222_RX_FIRMWARE);

				if (ret >= 0) {
					version =  ret;
				}
				pr_info("%s rx major firmware version 0x%x \n", __func__, version);

				count = sprintf(buf, "%x\n", version);
			}
			break;
		case WPC_IBUCK:
			{
				int ibuck =0;
				ibuck = p9222_get_adc(charger, P9222_ADC_RX_IOUT);
				ibuck -= P9222_I_OUT_OFFSET;
				pr_info("%s raw iout %dmA\n", __func__, ibuck);
				count = sprintf(buf, "%d\n", ibuck);
			}
			break;
		case WPC_WATCHDOG:
			pr_info("%s: watchdog test [%d] \n", __func__, charger->pdata->watchdog_test);
			count = sprintf(buf, "%d\n", charger->pdata->watchdog_test);
			break;
		case WPC_ADDR:
			count = sprintf(buf, "0x%x\n", charger->addr);
			break;
		case WPC_SIZE:
			count = sprintf(buf, "0x%x\n", charger->size);
			break;
		case WPC_DATA:
			if (charger->size == 0)
				charger->size = 1;

			for (i = 0; i < charger->size; i++) {
				if (p9222_reg_read(charger->client, charger->addr+i, &data) < 0) {
					dev_info(charger->dev,
							"%s: read fail\n", __func__);
					count += sprintf(buf+count, "addr: 0x%x read fail\n", charger->addr+i);
					continue;
				}
				count += sprintf(buf+count, "addr: 0x%x, data: 0x%x\n", charger->addr+i,data);
			}
			break;
		default:
			break;
		}
	return count;
}

ssize_t sec_wpc_store_attrs(
					struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct p9222_charger_data *charger =
		power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - p9222_attributes;
	int x, ret = -EINVAL;;

	switch (offset) {
		case WPC_WATCHDOG:
			sscanf(buf, "%d\n", &x);
			if (x == 1)
				charger->pdata->watchdog_test = true;
			else if (x == 0)
				charger->pdata->watchdog_test = false;
			else
				pr_debug("%s: non valid input\n", __func__);
			pr_info("%s: watchdog test is set to %d\n", __func__, charger->pdata->watchdog_test);
			ret = count;
			break;
		case WPC_ADDR:
			if (sscanf(buf, "0x%x\n", &x) == 1) {
				charger->addr = x;
			}
			ret = count;
			break;
		case WPC_SIZE:
			if (sscanf(buf, "%d\n", &x) == 1) {
				charger->size = x;
			}
			ret = count;
			break;
		case WPC_DATA:
			if (sscanf(buf, "0x%x", &x) == 1) {
				u8 data = x;
				if (p9222_reg_write(charger->client, charger->addr, data) < 0)
				{
					dev_info(charger->dev,
							"%s: addr: 0x%x write fail\n", __func__, charger->addr);
				}
			}
			ret = count;
			break;
		default:
			break;
	}
	return ret;
}

static int sec_wpc_create_attrs(struct device *dev)
{
	unsigned long i;
	int rc;

	for (i = 0; i < ARRAY_SIZE(p9222_attributes); i++) {
		rc = device_create_file(dev, &p9222_attributes[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	while (i--)
		device_remove_file(dev, &p9222_attributes[i]);
create_attrs_succeed:
	return rc;
}

static void p9222_wpc_auth_check_work(struct work_struct *work)
{
	struct p9222_charger_data *charger =
		container_of(work, struct p9222_charger_data, wpc_auth_check_work.work);

	union power_supply_propval value;
	u8 cmd_data, val_data;

	pr_info("%s wc_w_state=%d\n", __func__, charger->wc_w_state);

	if (charger->wc_w_state == WPC_OFF_PAD) {
		wake_unlock(&charger->wpc_auth_check_lock);
		return;
	}

	if (p9222_check_chg_in_status(charger) == false) {
		pr_err("%s acok is not valid!!\n", __func__);
		goto out;
	}

	if (charger->tx_id_checked == 0) {
		p9222_reg_read(charger->client, P9222_TX_DATA_COMMAND, &cmd_data);
		p9222_reg_read(charger->client, P9222_TX_DATA_VALUE0, &val_data);
		charger->tx_id = val_data;

		pr_info("%s cmd_data = %x val_data = %x\n", __func__, cmd_data, val_data);

		if (cmd_data == P9222_TX_DATA_COM_TX_ID) {
			if (p9222_compatible_tx_check(charger, val_data)) {
				pr_info("%s data = 0x%x, compatible TX pad\n", __func__, val_data);
				value.intval = 1;
				psy_do_property(charger->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);
				psy_do_property(charger->pdata->battery_name, set,
					POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX, value);

				if (charger->support_power_hold)
					charger->charger_type = P9222_CHARGER_TYPE_COMPATIBLE;
				else
					charger->charger_type = P9222_CHARGER_TYPE_MULTI;

				value.intval = 0;
				charger->incompatible_tx = 0;
				psy_do_property(charger->pdata->battery_name, set,
					POWER_SUPPLY_EXT_PROP_INCOMPATIBLE_WPC, value);
			} else {
				value.intval = 1;
				charger->incompatible_tx = 1;
				psy_do_property(charger->pdata->battery_name, set,
					POWER_SUPPLY_EXT_PROP_INCOMPATIBLE_WPC, value);
				charger->charger_type = P9222_CHARGER_TYPE_INCOMPATIBLE;
			}
		} else {
			value.intval = 1;
			charger->incompatible_tx = 1;
			psy_do_property(charger->pdata->battery_name, set,
				POWER_SUPPLY_EXT_PROP_INCOMPATIBLE_WPC, value);
			charger->charger_type = P9222_CHARGER_TYPE_INCOMPATIBLE;
		}
	}


	charger->tx_id_checked = 0;
	p9222_wpc_request_tx_id(charger, 3);
out:
	if (p9222_compatible_tx_check(charger, charger->tx_id) == 0) {
		value.intval = 1;
		charger->incompatible_tx = 1;
		psy_do_property(charger->pdata->battery_name, set,
			POWER_SUPPLY_EXT_PROP_INCOMPATIBLE_WPC, value);
		charger->charger_type = P9222_CHARGER_TYPE_INCOMPATIBLE;
		if (charger->tx_id == 0)
			charger->tx_id = UNKNOWN_TX_ID;
	}
	wake_unlock(&charger->wpc_auth_check_lock);
}

static int
p9222_wpc_auth_delayed_work(struct p9222_charger_data *charger, int sec)
{
	int delay = sec * 1000;
	int lock_timeout = sec + (sec / 2);

	pr_info("%s delay=%d lock_timeout = %d\n", __func__, delay, lock_timeout);

	cancel_delayed_work(&charger->wpc_auth_check_work);

	if (!wake_lock_active(&charger->wpc_auth_check_lock))
		wake_lock_timeout(&charger->wpc_auth_check_lock, lock_timeout * HZ);

	schedule_delayed_work(&charger->wpc_auth_check_work,
		msecs_to_jiffies(delay));
	return 0;
}

static struct p9222_chg_type_desc charger_type_compatilbe = {
	.charger_type = P9222_CHARGER_TYPE_COMPATIBLE,
	.charger_type_name = "compatible",
	.cc_chg = p9222_compatible_cc_charge,
	.cv_chg = p9222_compatible_cv_charge,
	.full_chg = p9222_power_hold_full_charge,
	.re_chg = p9222_power_hold_re_charge,
};

static struct p9222_chg_type_desc charger_type_incompatilbe = {
	.charger_type = P9222_CHARGER_TYPE_INCOMPATIBLE,
	.charger_type_name = "incompatible",
	.cc_chg = p9222_incompatible_cc_charge,
	.cv_chg = p9222_incompatible_cv_charge,
	.full_chg = p9222_normal_full_charge,
	.re_chg = p9222_normal_re_charge,
};

static struct p9222_chg_type_desc charger_type_multi = {
	.charger_type = P9222_CHARGER_TYPE_MULTI,
	.charger_type_name = "multi",
	.cc_chg = p9222_compatible_cc_charge,
	.cv_chg = p9222_compatible_cv_charge,
	.full_chg = p9222_normal_full_charge,
	.re_chg = p9222_normal_re_charge,
};

static int p9222_wpc_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	struct p9222_charger_data *charger =
		container_of(nb, struct p9222_charger_data, wpc_nb);

	pr_info("%s action=%lu, attached_dev=%d\n", __func__, action, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_TA_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			p9222_reg_update(charger->client,
				P9222_WPC_FLAG_REG, 0x0, P9222_WATCHDOG_DIS_MASK);
			p9222_get_firmware_version(charger, P9222_RX_FIRMWARE);
			p9222_get_firmware_version(charger, P9222_TX_FIRMWARE);
		}
		break;
	case ATTACHED_DEV_WIRELESS_TA_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s MUIC_NOTIFY_CMD_DETACH\n", __func__);
			charger->is_recharge = 0;
			schedule_delayed_work(&charger->wpc_detach_chk_work,
				msecs_to_jiffies(400));
		} else if (action == MUIC_NOTIFY_CMD_ATTACH) {
			p9222_enable_vbat_monitoring(charger);
		}
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			if (p9222_is_on_pad(charger) == WPC_ON_PAD) {
				if (charger->power_hold_mode == 1) {
					wake_lock(&charger->power_hold_chk_lock);
					schedule_delayed_work(&charger->power_hold_chk_work,
						msecs_to_jiffies(3000));
				}
				p9222_enable_vbat_monitoring(charger);
			}
		} else if (action == MUIC_NOTIFY_CMD_DETACH) {
			pr_info("%s MUIC_NOTIFY_CMD_DETACH\n", __func__);
			charger->is_recharge = 0;
			schedule_delayed_work(&charger->wpc_detach_chk_work,
				msecs_to_jiffies(400));
		}
		break;
	default:
		break;
	}

	return 0;
}

static int p9222_parse_dt(struct device *dev, p9222_charger_platform_data_t *pdata)
{

	struct device_node *np = dev->of_node;
	const u32 *p;
	int len, ret, i;
	enum of_gpio_flags irq_gpio_flags;
	/*changes can be added later, when needed*/

	ret = of_property_read_string(np,
		"p9222,charger_name", (char const **)&pdata->charger_name);
	if (ret) {
		pr_info("%s: Charger name is Empty\n", __func__);
		pdata->charger_name = "sec-charger";
	}

	ret = of_property_read_string(np,
		"p9222,fuelgauge_name", (char const **)&pdata->fuelgauge_name);
	if (ret) {
		pr_info("%s: Fuelgauge name is Empty\n", __func__);
		pdata->fuelgauge_name = "sec-fuelgauge";
	}

	ret = of_property_read_string(np,
		"p9222,battery_name", (char const **)&pdata->battery_name);
	if (ret) {
		pr_info("%s: battery_name is Empty\n", __func__);
		pdata->battery_name = "battery";
	}

	ret = of_property_read_string(np,
		"p9222,wireless_name", (char const **)&pdata->wireless_name);
	if (ret) {
		pr_info("%s: wireless_name is Empty\n", __func__);
		pdata->wireless_name = "wireless";
	}

	ret = of_property_read_string(np, "p9222,wireless_charger_name",
		(char const **)&pdata->wireless_charger_name);
	if (ret) {
		pr_info("%s: wireless_charger_name is Empty\n", __func__);
		pdata->wireless_charger_name = "wpc";
	}

	/* wpc_det */
	ret = pdata->wpc_det = of_get_named_gpio_flags(np, "p9222,wpc_det",
			0, &irq_gpio_flags);
	if (ret < 0) {
		dev_err(dev, "%s : can't get wpc_det\r\n", __func__);
	} else {
		if (gpio_is_valid(pdata->wpc_det)) {
		    ret = gpio_request(pdata->wpc_det, "wpc_det");
		    if (ret) {
		        dev_err(dev, "%s : can't get wpc_det\r\n", __func__);
		    } else {
		    	ret = gpio_direction_input(pdata->wpc_det);
				if (ret) {
					dev_err(dev, "%s : wpc_det set input fail\r\n", __func__);
				}
				pdata->irq_wpc_det = gpio_to_irq(pdata->wpc_det);
				pr_info("%s wpc_det = 0x%x, irq_wpc_det = 0x%x (%d)\n", __func__,
					pdata->wpc_det, pdata->irq_wpc_det, pdata->irq_wpc_det);
		    }
		}
	}

	/* wpc_int */
	ret = pdata->wpc_int = of_get_named_gpio_flags(np, "p9222,wpc_int",
			0, &irq_gpio_flags);
	if (ret < 0) {
		dev_err(dev, "%s : can't wpc_int\r\n", __FUNCTION__);
	} else {
		pdata->irq_wpc_int = gpio_to_irq(pdata->wpc_int);
		pr_info("%s wpc_int = 0x%x, irq_wpc_int = 0x%x \n",__func__,
			pdata->wpc_int, pdata->irq_wpc_int);
	}

	/* wpc_en */
	ret = pdata->wpc_en = of_get_named_gpio_flags(np, "p9222,wpc_en",
			0, &irq_gpio_flags);
	if (ret < 0) {
		dev_err(dev, "%s : can't wpc_en\r\n", __FUNCTION__);
	} else {
		if (gpio_is_valid(pdata->wpc_en)) {
		    ret = gpio_request(pdata->wpc_en, "wpc_en");
		    if (ret) {
		        dev_err(dev, "%s : can't get wpc_en\r\n", __func__);
		    } else {
		    	gpio_direction_output(pdata->wpc_en, 0);
		    }
		}
	}

	ret = of_property_read_u32(np, "p9222,cc_cv_threshold",
		&pdata->cc_cv_threshold);
	if (ret < 0) {
		pr_err("%s error reading cc_cv_threshold %d\n", __func__, ret);
		pdata->cc_cv_threshold = 88;
	}

	pdata->can_vbat_monitoring = of_property_read_bool(np, "p9222,vbat-monitoring");

	p = of_get_property(np, "p9222,charger_type", &len);
	if (p) {
		pdata->num_compatible_tx = len / sizeof(struct p9222_charger_type);
		pdata->charger_type = kzalloc(len, GFP_KERNEL);

		ret = of_property_read_u32_array(np, "p9222,charger_type",
				 (u32 *)pdata->charger_type, len/sizeof(u32));
		if (ret) {
			pr_err("%s: failed to read p9222,charger_type: %d\n", __func__, ret);
			kfree(pdata->charger_type);
			pdata->charger_type = NULL;
			pdata->num_compatible_tx = 0;
		} else {
			for (i = 0; i < pdata->num_compatible_tx; i++)
				pr_info("charger_type tx_id = 0x%02x power_hold = %d\n",
					pdata->charger_type[i].compatible_tx_id,
					pdata->charger_type[i].support_power_hold);
		}
	}

	p = of_get_property(np, "p9222,sec_mode_data", &len);
	if (p) {
		pdata->num_sec_mode = len / sizeof(struct p9222_sec_mode_config_data);
		pdata->sec_mode_config_data = kzalloc(len, GFP_KERNEL);
		ret = of_property_read_u32_array(np, "p9222,sec_mode_data",
				 (u32 *)pdata->sec_mode_config_data, len/sizeof(u32));
		if (ret) {
			pr_err("%s: failed to read p9222,sec_mode_data: %d\n", __func__, ret);
			kfree(pdata->sec_mode_config_data);
			pdata->sec_mode_config_data = NULL;
			pdata->num_sec_mode = 0;
		}
		pr_err("%s: num_sec_mode : %d\n", __func__, pdata->num_sec_mode);
		for (len = 0; len < pdata->num_sec_mode; ++len)
			pr_err("mode %d : vrect:%d, vout:%d\n",
				len, pdata->sec_mode_config_data[len].vrect, pdata->sec_mode_config_data[len].vout);
	} else
		pr_err("%s: there is no p9222,sec_mode_data\n", __func__);

	ret = of_property_read_u32(np, "p9222,tx-off-high-temp",
		&pdata->tx_off_high_temp);
	if (ret) {
		pr_info("%s : TX-OFF-TEMP is Empty\n", __func__);
		pdata->tx_off_high_temp = INT_MAX;
	}

	ret = of_property_read_u32(np, "p9222,ping_duration",
		&pdata->ping_duration);
	if (ret) {
		pr_info("%s : ping_duration Empty\n", __func__);
		pdata->ping_duration = 350;
	}

	return 0;
}

static const struct power_supply_desc wpc_power_supply_desc = {
	.name = "wpc",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_charger_props,
	.num_properties = ARRAY_SIZE(sec_charger_props),
	.get_property = p9222_chg_get_property,
	.set_property = p9222_chg_set_property,
};

static int p9222_charger_probe(
						struct i2c_client *client,
						const struct i2c_device_id *id)
{
	struct p9222_charger_data *charger;
	p9222_charger_platform_data_t *pdata = client->dev.platform_data;
	struct power_supply_config wpc_cfg = {};
	int ret = 0;

	dev_info(&client->dev,
		"%s: p9222 Charger Driver Loading\n", __func__);

	pdata = devm_kzalloc(&client->dev, sizeof(p9222_charger_platform_data_t), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	ret = p9222_parse_dt(&client->dev, pdata);
	if (ret < 0)
		return ret;

	client->irq = pdata->irq_wpc_int;
	pdata->hw_rev_changed = 1;
	pdata->on_mst_wa=1;
	pdata->wpc_cv_call_vout = 5600;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (charger == NULL) {
		dev_err(&client->dev, "Memory is not enough.\n");
		ret = -ENOMEM;
		goto err_wpc_nomem;
	}
	charger->dev = &client->dev;

	ret = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK);
	if (!ret) {
		ret = i2c_get_functionality(client->adapter);
		dev_err(charger->dev, "I2C functionality is not supported.\n");
		ret = -ENOSYS;
		goto err_i2cfunc_not_support;
	}
	charger->client = client;
	charger->pdata = pdata;

	i2c_set_clientdata(client, charger);

	charger->pdata->ic_on_mode = false;
	charger->pdata->cable_type = P9222_PAD_MODE_NONE;
	charger->pdata->is_charging = 0;

	charger->pdata->otp_firmware_result = P9222_FW_RESULT_DOWNLOADING;
	charger->pdata->tx_firmware_result = P9222_FW_RESULT_DOWNLOADING;
	charger->pdata->tx_status = 0;
	charger->pdata->cs100_status = 0;
	charger->pdata->vout_status = P9222_VOUT_0V;
	charger->pdata->opfq_cnt = 0;
	charger->pdata->watchdog_test = false;
	charger->pdata->charging_mode = SEC_INITIAL_MODE;
	charger->wc_w_state = p9222_is_on_pad(charger);

	charger->power_hold_mode = 0;
	charger->support_power_hold = 0;
	charger->curr_measure_mode = 0;
	charger->charge_mode = P9222_CHARGE_MODE_NONE;
	charger->store_mode = 0;
	charger->wpc_id_request_step = 0;
	charger->tx_id_checked = 0;
	charger->tx_id = 0x0;

	mutex_init(&charger->io_lock);
	mutex_init(&charger->charger_lock);

	wake_lock_init(&charger->wpc_wake_lock, WAKE_LOCK_SUSPEND,
			"wpc_wakelock");
	wake_lock_init(&charger->wpc_vbat_monitoring_enable_lock, WAKE_LOCK_SUSPEND,
			"wpc_vbat_monitoring_enable_lock");
	wake_lock_init(&charger->wpc_auth_check_lock, WAKE_LOCK_SUSPEND,
			"wpc_auth_check_lock");

	charger->temperature = 250;

	/* wpc_det */
	if (charger->pdata->irq_wpc_det >= 0) {
		INIT_DELAYED_WORK(&charger->wpc_det_work, p9222_wpc_det_work);
		INIT_DELAYED_WORK(&charger->wpc_opfq_work, p9222_wpc_opfq_work);

		ret = request_threaded_irq(charger->pdata->irq_wpc_det,
				NULL, p9222_wpc_det_irq_thread,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
				IRQF_ONESHOT,
				"wpd-det-irq", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_supply_unreg;
		}

		ret = enable_irq_wake(charger->pdata->irq_wpc_det);
		if (ret < 0)
			dev_err(&client->dev, "%s: Failed to Enable Wakeup Source(%d)\n",
				__func__, ret);
	}

	/* wpc_irq */
	if (charger->client->irq) {
		p9222_reg_update(charger->client, P9222_INT_ENABLE_L_REG,
			P9222_INT_STAT_VOUT, P9222_STAT_VOUT_MASK);

		INIT_DELAYED_WORK(&charger->wpc_isr_work, p9222_wpc_isr_work);
		ret = gpio_request(pdata->wpc_int, "wpc-irq");
		if (ret) {
			pr_err("%s failed requesting gpio(%d)\n",
				__func__, pdata->wpc_int);
			goto err_supply_unreg;
		}
		ret = request_threaded_irq(charger->client->irq,
				NULL, p9222_wpc_irq_thread,
				IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT,
				"wpc-irq", charger);
		if (ret) {
			dev_err(&client->dev,
				"%s: Failed to Reqeust IRQ\n", __func__);
			goto err_supply_unreg;
		}
	}
	INIT_DELAYED_WORK(&charger->curr_measure_work, p9222_curr_measure_work);
	INIT_DELAYED_WORK(&charger->wpc_wait_vout_work, p9222_wpc_wait_vout);
	INIT_DELAYED_WORK(&charger->wpc_init_work, p9222_wpc_init);
	schedule_delayed_work(&charger->wpc_init_work, msecs_to_jiffies(1000));

	INIT_DELAYED_WORK(&charger->wpc_auth_check_work, p9222_wpc_auth_check_work);
	INIT_DELAYED_WORK(&charger->wpc_detach_chk_work, p9222_wpc_detach_check);
	INIT_DELAYED_WORK(&charger->power_hold_chk_work, p9222_wpc_power_hold_check);
	INIT_DELAYED_WORK(&charger->wpc_id_request_work, p9222_wpc_id_request_work);

	wpc_cfg.drv_data = charger;
	if (!wpc_cfg.drv_data) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_supply_unreg;
	}

	charger->psy_chg = power_supply_register(&client->dev,
		&wpc_power_supply_desc, &wpc_cfg);
	if (IS_ERR(charger->psy_chg)) {
		goto err_supply_unreg;
	}

	ret = sec_wpc_create_attrs(&charger->psy_chg->dev);
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_chg\n", __func__);
		goto err_pdata_free;
	}


	charger->wqueue = create_singlethread_workqueue("p9222_workqueue");
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}

	wake_lock_init(&charger->wpc_opfq_lock, WAKE_LOCK_SUSPEND,
		"wpc_opfq_lock");

	wake_lock_init(&charger->wpc_wait_vout_lock, WAKE_LOCK_SUSPEND,
		"wpc_wait_vout_lock");

	wake_lock_init(&charger->power_hold_chk_lock, WAKE_LOCK_SUSPEND,
		"power_hold_chk_lock");

	wake_lock_init(&charger->wpc_id_request_lock, WAKE_LOCK_SUSPEND,
		"wpc_id_request_lock");

	wake_lock_init(&charger->wpc_wakeup_wa_lock, WAKE_LOCK_SUSPEND,
		"wpc_wakeup_wa_lock");

	INIT_DELAYED_WORK(&charger->wpc_wakeup_wa_work, p9222_wpc_wa_work);
	charger->wpc_wakeup_wa = 0;

	charger->last_poll_time = ktime_get_boottime();
	alarm_init(&charger->polling_alarm, ALARM_BOOTTIME,
		p9222_wpc_ph_mode_alarm);

	charger->charge_cb_func[P9222_CHARGER_TYPE_COMPATIBLE] = &charger_type_compatilbe;
	charger->charge_cb_func[P9222_CHARGER_TYPE_INCOMPATIBLE] = &charger_type_incompatilbe;
	charger->charge_cb_func[P9222_CHARGER_TYPE_MULTI] = &charger_type_multi;

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&charger->wpc_nb, p9222_wpc_handle_notification,
			       MUIC_NOTIFY_DEV_CHARGER);
#endif

	dev_info(&client->dev,
		"%s: p9222 Charger Driver Loaded\n", __func__);

	//device_init_wakeup(charger->dev, 1);
	return 0;
err_pdata_free:
	power_supply_unregister(charger->psy_chg);
err_supply_unreg:
	wake_lock_destroy(&charger->wpc_wake_lock);
	wake_lock_destroy(&charger->wpc_vbat_monitoring_enable_lock);
	wake_lock_destroy(&charger->wpc_auth_check_lock);
	mutex_destroy(&charger->io_lock);
	mutex_destroy(&charger->charger_lock);
err_i2cfunc_not_support:
//irq_base_err:
	kfree(charger);
err_wpc_nomem:
	devm_kfree(&client->dev, pdata);
	return ret;
}

#if defined CONFIG_PM
static int p9222_charger_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct p9222_charger_data *charger = i2c_get_clientdata(i2c);

	pr_info("%s: mode[%d]\n", __func__, charger->pdata->charging_mode);

	if(!charger->pdata->hw_rev_changed) { /* this code is only temporary with non int_ext wpc_det */
		if (device_may_wakeup(charger->dev)){
			enable_irq_wake(charger->pdata->irq_wpc_int);
		}
		disable_irq(charger->pdata->irq_wpc_int);
	} else { /* this is for proper board */
		if (device_may_wakeup(charger->dev)){
			enable_irq_wake(charger->pdata->irq_wpc_int);
			enable_irq_wake(charger->pdata->irq_wpc_det);
		}
		disable_irq(charger->pdata->irq_wpc_int);
		disable_irq(charger->pdata->irq_wpc_det);
	}
	return 0;
}

static int p9222_charger_resume(struct device *dev)
{
	int wc_w_state;
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct p9222_charger_data *charger = i2c_get_clientdata(i2c);

	pr_info("%s: mode[%d] charger->charge_mode %d charger->pdata->otp_firmware_ver = %x\n",
		__func__, charger->pdata->charging_mode, charger->charge_mode,
		charger->pdata->otp_firmware_ver);

	charger->wpc_wakeup_wa = 0;

	if(!charger->pdata->hw_rev_changed) { /* this code is only temporary with non int_ext wpc_det */
		wc_w_state = gpio_get_value(charger->pdata->wpc_det);

		if(charger->wc_w_state != wc_w_state)
			queue_delayed_work(charger->wqueue, &charger->wpc_det_work, 0);
		if (device_may_wakeup(charger->dev)) {
			disable_irq_wake(charger->pdata->irq_wpc_int);
		}
		enable_irq(charger->pdata->irq_wpc_int);
	} else { /* this is for proper board */
		if (device_may_wakeup(charger->dev)) {
			disable_irq_wake(charger->pdata->irq_wpc_int);
			disable_irq_wake(charger->pdata->irq_wpc_det);
		}
		enable_irq(charger->pdata->irq_wpc_int);
		enable_irq(charger->pdata->irq_wpc_det);
	}

	if ((p9222_is_on_pad(charger) == WPC_ON_PAD) &&
		(charger->power_hold_mode == 0)) {
		if (charger->charge_mode == P9222_CHARGE_MODE_CV) {
			if (charger->pdata->otp_firmware_ver == 0x125) {
				wake_lock(&charger->wpc_wakeup_wa_lock);
				charger->wpc_wakeup_wa = 1;
				p9222_ap_battery_monitor(charger, WPC_MODE_BOOT);
				schedule_delayed_work(&charger->wpc_wakeup_wa_work, msecs_to_jiffies(1000));
			} else {
				pr_info("%s set wake-up mode(0x12)\n", __func__);
				p9222_ap_battery_monitor(charger, WPC_MODE_WAKEUP);
			}
		}
	}
	return 0;
}
#else
#define p9222_charger_suspend NULL
#define p9222_charger_resume NULL
#endif

static void p9222_charger_shutdown(struct i2c_client *client)
{
	struct p9222_charger_data *charger = i2c_get_clientdata(client);

	pr_debug("%s \n", __func__);

	if (p9222_is_on_pad(charger) == WPC_ON_PAD) {
		if (charger->power_hold_mode) {
			pr_info("%s exit power hold mode before poweroff\n", __func__);
			p9222_re_charge(charger);
		}

		p9222_ap_battery_monitor(charger, WPC_MODE_IDT);
	}
}


static const struct i2c_device_id p9222_charger_id[] = {
	{"p9222-charger", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, p9222_charger_id);

static struct of_device_id p9222_i2c_match_table[] = {
	{ .compatible = "p9222,i2c"},
	{},
};

const struct dev_pm_ops p9222_charger_pm = {
	.suspend = p9222_charger_suspend,
	.resume = p9222_charger_resume,
};

static struct i2c_driver p9222_charger_driver = {
	.driver = {
		.name	= "p9222-charger",
		.owner	= THIS_MODULE,
#if defined(CONFIG_PM)
		.pm	= &p9222_charger_pm,
#endif /* CONFIG_PM */
		.of_match_table = p9222_i2c_match_table,
	},
	.shutdown	= p9222_charger_shutdown,
	.probe	= p9222_charger_probe,
	//.remove	= p9222_charger_remove,
	.id_table	= p9222_charger_id,
};

static int __init p9222_charger_init(void)
{
	pr_debug("%s \n",__func__);
	return i2c_add_driver(&p9222_charger_driver);
}

static void __exit p9222_charger_exit(void)
{
	pr_debug("%s \n",__func__);
	i2c_del_driver(&p9222_charger_driver);
}

module_init(p9222_charger_init);
module_exit(p9222_charger_exit);

MODULE_DESCRIPTION("Samsung p9222 Charger Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

