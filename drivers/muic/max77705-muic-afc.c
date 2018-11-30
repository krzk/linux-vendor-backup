/*
 * max77705-muic.c - MUIC driver for the Maxim 77705
 *
 *  Copyright (C) 2015 Samsung Electronics
 *  Insun Choi <insun77.choi@samsung.com>
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include <linux/muic/max77705-muic.h>
#include <linux/ccic/max77705_usbc.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif

bool max77705_muic_check_is_enable_afc(struct max77705_muic_data *muic_data, muic_attached_dev_t new_dev)
{
	int ret = false;

	if (new_dev == ATTACHED_DEV_TA_MUIC) {
		if (muic_data->pdata->afc_disable) {
			pr_info("%s AFC Disable(%d) by USER!, skip AFC\n",
				__func__, muic_data->pdata->afc_disable);
		} else if (!muic_data->is_charger_ready) {
			pr_info("%s Charger is not ready(%d), skip AFC\n",
				__func__, muic_data->is_charger_ready);
#if defined(CONFIG_CCIC_NOTIFIER)
		} else if (muic_data->usbc_pdata->fac_water_enable) {
			pr_info("%s fac_water_enable(%d), skip AFC\n", __func__,
				muic_data->usbc_pdata->fac_water_enable);
#endif /* CONFIG_CCIC_NOTIFIER */
#if defined(CONFIG_MUIC_MAX77705_CCIC)
		} else if (muic_data->afc_water_disable) {
			pr_info("%s water detected(%d), skip AFC\n", __func__,
				muic_data->afc_water_disable);
#endif /* CONFIG_MUIC_MAX77705_CCIC */
		} else {
			ret = true;
		}
	}

	return ret;
}

void max77705_muic_afc_hv_set(struct max77705_muic_data *muic_data, int voltage)
{
	struct max77705_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data write_data;
	u8 tx_byte;

	switch (voltage) {
	case 5:
		tx_byte = 0x08;
		break;
	case 9:
		tx_byte = 0x46;
		break;
	default:
		pr_info("%s:%s invalid value(%d), return\n", MUIC_DEV_NAME,
				__func__, voltage);
		return;
	}

	pr_info("%s:%s voltage(%d)\n", MUIC_DEV_NAME, __func__, voltage);

	init_usbc_cmd_data(&write_data);
	write_data.opcode = COMMAND_AFC_RESULT_READ;
	write_data.write_length = 1;
	write_data.write_data[0] = tx_byte;
	write_data.read_length = 10;

	max77705_usbc_opcode_write(usbc_pdata, &write_data);
}

void max77705_muic_qc_hv_set(struct max77705_muic_data *muic_data, int voltage)
{
	struct max77705_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data write_data;
	u8 dpdndrv;

	switch (voltage) {
	case 5:
		dpdndrv = 0x04;
		break;
	case 9:
		dpdndrv = 0x09;
		break;
	default:
		pr_info("%s:%s invalid value(%d), return\n", MUIC_DEV_NAME,
				__func__, voltage);
		return;
	}

	pr_info("%s:%s voltage(%d)\n", MUIC_DEV_NAME, __func__, voltage);

	init_usbc_cmd_data(&write_data);
	write_data.opcode = COMMAND_QC_2_0_SET;
	write_data.write_length = 1;
	write_data.write_data[0] = dpdndrv;
	write_data.read_length = 2;

	max77705_usbc_opcode_write(usbc_pdata, &write_data);
}

static void max77705_muic_handle_detect_dev_mpnack(struct max77705_muic_data *muic_data)
{
	struct max77705_usbc_platform_data *usbc_pdata = muic_data->usbc_pdata;
	usbc_cmd_data write_data;
	u8 dpdndrv = 0x09;

	init_usbc_cmd_data(&write_data);
	write_data.opcode = COMMAND_QC_2_0_SET;
	write_data.write_length = 1;
	write_data.write_data[0] = dpdndrv;
	write_data.read_length = 2;

	max77705_usbc_opcode_write(usbc_pdata, &write_data);
}

void max77705_muic_handle_detect_dev_afc(struct max77705_muic_data *muic_data, unsigned char *data)
{
	int result = data[1];
	int vbadc = data[2];
	muic_attached_dev_t new_afc_dev = muic_data->attached_dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
	bool afc_err = false;
	bool afc_nack = false;
#endif

	pr_info("%s:%s result:0x%x vbadc:0x%x rxbyte:0x%x %x %x %x %x %x %x %x\n", MUIC_DEV_NAME,
			__func__, data[1], data[2], data[3], data[4], data[5],
			data[6], data[7], data[8], data[9], data[10]);

	switch (result) {
	case 0:
		pr_info("%s:%s AFC Success, vbadc(%d)\n", MUIC_DEV_NAME, __func__, vbadc);

		if (vbadc >= MAX77705_VBADC_4_5V_TO_5_5V &&
				vbadc <= MAX77705_VBADC_6_5V_TO_7_5V)
			new_afc_dev = ATTACHED_DEV_AFC_CHARGER_5V_MUIC;
		else if (vbadc >= MAX77705_VBADC_7_5V_TO_8_5V &&
				vbadc <= MAX77705_VBADC_8_5V_TO_9_5V)
			new_afc_dev = ATTACHED_DEV_AFC_CHARGER_9V_MUIC;
#if defined(CONFIG_USB_HW_PARAM)
		else
			afc_err = true;
#endif

		if (new_afc_dev != muic_data->attached_dev) {
			muic_notifier_attach_attached_dev(new_afc_dev);
			muic_data->attached_dev = new_afc_dev;
		}
		break;
	case 1:
		pr_info("%s:%s No CHGIN\n", MUIC_DEV_NAME, __func__);
		break;
	case 2:
		pr_info("%s:%s Not High Voltage DCP\n", MUIC_DEV_NAME, __func__);
		break;
	case 3:
		pr_info("%s:%s Not DCP\n", MUIC_DEV_NAME, __func__);
		break;
	case 4:
		pr_info("%s:%s MPing NACK\n", MUIC_DEV_NAME, __func__);
		max77705_muic_handle_detect_dev_mpnack(muic_data);
		break;
	case 5:
		pr_info("%s:%s Unsupported TX data\n", MUIC_DEV_NAME, __func__);
		break;
	case 6:
		pr_info("%s:%s Vbus is not changed with 3 continuous ping\n",
				MUIC_DEV_NAME, __func__);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	case 7:
		pr_info("%s:%s Vbus is not changed in 1sec\n",
				MUIC_DEV_NAME, __func__);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	case 8:
		pr_info("%s:%s CC-Vbus Short case\n", MUIC_DEV_NAME, __func__);
		break;
	case 9:
		pr_info("%s:%s SBU-Gnd Short case\n", MUIC_DEV_NAME, __func__);
		break;
	case 11:
		pr_info("%s:%s Not Rp 56K\n", MUIC_DEV_NAME, __func__);
		break;
	case 16:
		pr_info("%s:%s A parity check failed during resceiving data\n",
				MUIC_DEV_NAME, __func__);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	case 17:
		pr_info("%s:%s The slave does not respond to the master ping\n",
				MUIC_DEV_NAME, __func__);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	case 18:
		pr_info("%s:%s RX buffer overflow is detected\n",
				MUIC_DEV_NAME, __func__);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	default:
		pr_info("%s:%s AFC error(%d)\n", MUIC_DEV_NAME, __func__, result);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	}

#if defined(CONFIG_USB_HW_PARAM)
	if (o_notify) {
		if (afc_err && !afc_nack)
			inc_hw_param(o_notify, USB_MUIC_AFC_ERROR_COUNT);
		if (afc_nack) {
			inc_hw_param(o_notify, USB_MUIC_AFC_ERROR_COUNT);
			inc_hw_param(o_notify, USB_MUIC_AFC_NACK_COUNT);
		}
	}
#endif
}

void max77705_muic_handle_detect_dev_qc(struct max77705_muic_data *muic_data, unsigned char *data)
{
	int result = data[1];
	int vbadc = data[2];
	muic_attached_dev_t new_afc_dev = muic_data->attached_dev;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
	bool afc_err = false;
	bool afc_nack = false;
#endif

	pr_info("%s:%s result:0x%x vbadc:0x%x\n", MUIC_DEV_NAME,
			__func__, data[1], data[2]);

	switch (result) {
	case 0:
		pr_info("%s:%s QC2.0 Success, vbadc(%d)\n", MUIC_DEV_NAME, __func__, vbadc);

		if (vbadc >= MAX77705_VBADC_4_5V_TO_5_5V &&
				vbadc <= MAX77705_VBADC_6_5V_TO_7_5V)
			new_afc_dev = ATTACHED_DEV_QC_CHARGER_5V_MUIC;
		else if (vbadc >= MAX77705_VBADC_7_5V_TO_8_5V &&
				vbadc <= MAX77705_VBADC_8_5V_TO_9_5V)
			new_afc_dev = ATTACHED_DEV_QC_CHARGER_9V_MUIC;
#if defined(CONFIG_USB_HW_PARAM)
		else
			afc_err = true;
#endif

		if (new_afc_dev != muic_data->attached_dev) {
			muic_notifier_attach_attached_dev(new_afc_dev);
			muic_data->attached_dev = new_afc_dev;
		}
		break;
	case 1:
		pr_info("%s:%s No CHGIN\n", MUIC_DEV_NAME, __func__);
		break;
	case 2:
		pr_info("%s:%s Not High Voltage DCP\n",
				MUIC_DEV_NAME, __func__);
		break;
	case 3:
		pr_info("%s:%s Not DCP\n", MUIC_DEV_NAME, __func__);
		break;
	case 6:
		pr_info("%s:%s Vbus is not changed with 3 continuous ping\n",
				MUIC_DEV_NAME, __func__);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	case 7:
		pr_info("%s:%s Vbus is not changed in 1 sec\n",
				MUIC_DEV_NAME, __func__);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	default:
		pr_info("%s:%s QC2.0 error(%d)\n", MUIC_DEV_NAME, __func__, result);
#if defined(CONFIG_USB_HW_PARAM)
		afc_nack = true;
#endif
		break;
	}

#if defined(CONFIG_USB_HW_PARAM)
	if (o_notify) {
		if (afc_err && !afc_nack)
			inc_hw_param(o_notify, USB_MUIC_AFC_ERROR_COUNT);
		if (afc_nack) {
			inc_hw_param(o_notify, USB_MUIC_AFC_ERROR_COUNT);
			inc_hw_param(o_notify, USB_MUIC_AFC_NACK_COUNT);
		}
	}
#endif
}
