/* /linux/drivers/misc/modem_if/modem_modemctl_device_xmm6262.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Copyright (C) 2010 Samsung Electronics.
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

#define DEBUG

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <linux/platform_data/modem.h>
#include "modem_prj.h"

static void xmm_gpio_revers_bias_clear(struct modem_ctl *mc)
{
	gpio_direction_output(mc->gpio_pda_active, 0);
	gpio_direction_output(mc->gpio_cp_dump_int, 0);
	gpio_direction_output(mc->mdm_data->link_pm_data->gpio_link_active, 0);
	gpio_direction_output(mc->mdm_data->link_pm_data->gpio_link_slavewake, 0);

	gpio_direction_output(mc->gpio_reset_req_n, 0); /* added by K */
	gpio_direction_output(mc->gpio_cp_on, 0); /* added by K */
	gpio_direction_output(mc->gpio_cp_reset, 0); /* added by K */

	if (!IS_ERR(mc->pinctrl_off))
		pinctrl_select_state(mc->pinctrl, mc->pinctrl_off);

/*
	if (umts_modem_data.gpio_sim_detect)
		gpio_direction_output(umts_modem_data.gpio_sim_detect, 0);
*/

	msleep(20);
}

static void xmm_gpio_revers_bias_restore(struct modem_ctl *mc)
{
/*
	unsigned gpio_sim_detect = umts_modem_data.gpio_sim_detect;
*/

	if (!IS_ERR(mc->pinctrl_active))
		pinctrl_select_state(mc->pinctrl, mc->pinctrl_active);

	gpio_direction_input(mc->gpio_cp_dump_int);

/*	if (gpio_sim_detect) {
		gpio_direction_input(gpio_sim_detect);
		s3c_gpio_cfgpin(gpio_sim_detect, S3C_GPIO_SFN(0xF));
		s3c_gpio_setpull(gpio_sim_detect, S3C_GPIO_PULL_NONE);
		irq_set_irq_type(gpio_to_irq(gpio_sim_detect),
				IRQ_TYPE_EDGE_BOTH);
		enable_irq_wake(gpio_to_irq(gpio_sim_detect));
	} */
}

static int xmm6262_on(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_cp_reset || !mc->gpio_cp_on || !mc->gpio_reset_req_n) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	xmm_gpio_revers_bias_clear(mc);

	/* TODO */
	gpio_set_value(mc->gpio_reset_req_n, 0);
	gpio_set_value(mc->gpio_cp_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);
	msleep(100);
	gpio_set_value(mc->gpio_cp_reset, 1);
	/* If XMM6262 was connected with C2C, AP wait 50ms to BB Reset*/
	msleep(50);
	gpio_set_value(mc->gpio_reset_req_n, 1);

	gpio_set_value(mc->gpio_cp_on, 1);
	udelay(60);
	gpio_set_value(mc->gpio_cp_on, 0);
	msleep(20);

	xmm_gpio_revers_bias_restore(mc);

	gpio_set_value(mc->gpio_pda_active, 1);

	mc->phone_state = STATE_BOOTING;

	return 0;
}

static int xmm6262_off(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_cp_reset || !mc->gpio_cp_on) {
		mif_err("no gpio data\n");
		return -ENXIO;
	}

	gpio_set_value(mc->gpio_cp_on, 0);
	gpio_set_value(mc->gpio_cp_reset, 0);

	xmm_gpio_revers_bias_clear(mc);

	return 0;
}

static int xmm6262_reset(struct modem_ctl *mc)
{
	mif_info("\n");

	if (!mc->gpio_cp_reset || !mc->gpio_reset_req_n)
		return -ENXIO;

	xmm_gpio_revers_bias_clear(mc);

	gpio_set_value(mc->gpio_cp_reset, 0);
	gpio_set_value(mc->gpio_reset_req_n, 0);

	mc->phone_state = STATE_OFFLINE;

	msleep(20);

	gpio_set_value(mc->gpio_cp_reset, 1);
	/* TODO: check the reset timming with C2C connection */
	udelay(160);

	gpio_set_value(mc->gpio_reset_req_n, 1);
	udelay(100);

	gpio_set_value(mc->gpio_cp_on, 1);
	udelay(60);
	gpio_set_value(mc->gpio_cp_on, 0);
	msleep(20);

	xmm_gpio_revers_bias_restore(mc);

/* vvv added by Kamil */
	gpio_direction_input(mc->gpio_cp_dump_int);

	gpio_set_value(mc->gpio_pda_active, 1);

	msleep(10);

	gpio_set_value(mc->mdm_data->link_pm_data->gpio_link_active, 1);
	gpio_set_value(mc->mdm_data->link_pm_data->gpio_link_slavewake, 1);
/* ^^^ added by Kamil */

	mc->phone_state = STATE_BOOTING;

	return 0;
}

static irqreturn_t phone_active_irq_handler(int irq, void *_mc)
{
	int phone_reset = 0;
	int phone_active_value = 0;
	int cp_dump_value = 0;
	int phone_state = 0;
	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	disable_irq_nosync(mc->irq_phone_active);

	if (!mc->gpio_cp_reset || !mc->gpio_phone_active ||
			!mc->gpio_cp_dump_int) {
		mif_err("no gpio data\n");
		return IRQ_HANDLED;
	}

	phone_reset = gpio_get_value(mc->gpio_cp_reset);
	phone_active_value = gpio_get_value(mc->gpio_phone_active);
	cp_dump_value = gpio_get_value(mc->gpio_cp_dump_int);

	mif_info("PA EVENT : reset =%d, pa=%d, cp_dump=%d\n",
				phone_reset, phone_active_value, cp_dump_value);

	if (phone_reset && phone_active_value)
		phone_state = STATE_BOOTING;
	else if (phone_reset && !phone_active_value) {
		if (cp_dump_value)
			phone_state = STATE_CRASH_EXIT;
		else
			phone_state = STATE_CRASH_RESET;
	} else
		phone_state = STATE_OFFLINE;

	if (mc->iod && mc->iod->modem_state_changed)
		mc->iod->modem_state_changed(mc->iod, phone_state);

	if (mc->bootd && mc->bootd->modem_state_changed)
		mc->bootd->modem_state_changed(mc->bootd, phone_state);

	if (phone_active_value)
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_LOW);
	else
		irq_set_irq_type(mc->irq_phone_active, IRQ_TYPE_LEVEL_HIGH);
	enable_irq(mc->irq_phone_active);

	return IRQ_HANDLED;
}

static irqreturn_t sim_detect_irq_handler(int irq, void *_mc)
{
	struct modem_ctl *mc = (struct modem_ctl *)_mc;

	if (mc->iod && mc->iod->sim_state_changed)
		mc->iod->sim_state_changed(mc->iod,
			gpio_get_value(mc->gpio_sim_detect) == mc->sim_polarity
			);

	return IRQ_HANDLED;
}

static void xmm6262_get_ops(struct modem_ctl *mc)
{
	mc->ops.modem_on = xmm6262_on;
	mc->ops.modem_off = xmm6262_off;
	mc->ops.modem_reset = xmm6262_reset;
}

int xmm6262_init_modemctl_device(struct modem_ctl *mc,
			struct modem_data *pdata)
{
	int ret = 0;
	struct platform_device *pdev;

	mc->gpio_reset_req_n = pdata->gpio_reset_req_n;
	mc->gpio_cp_on = pdata->gpio_cp_on;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_cp_dump_int = pdata->gpio_cp_dump_int;
	mc->gpio_ap_dump_int = pdata->gpio_ap_dump_int;
	mc->gpio_flm_uart_sel = pdata->gpio_flm_uart_sel;
	mc->gpio_cp_warm_reset = pdata->gpio_cp_warm_reset;
	mc->gpio_sim_detect = pdata->gpio_sim_detect;
	mc->sim_polarity = pdata->sim_polarity;

	mc->gpio_revers_bias_clear = pdata->gpio_revers_bias_clear;
	mc->gpio_revers_bias_restore = pdata->gpio_revers_bias_restore;

	pdev = to_platform_device(mc->dev);

	mc->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (!IS_ERR(mc->pinctrl)) {
		mc->pinctrl_off = pinctrl_lookup_state(mc->pinctrl,
							PINCTRL_STATE_DEFAULT);
		mc->pinctrl_active = pinctrl_lookup_state(mc->pinctrl,
							"active");
	} else {
		mc->pinctrl_off = ERR_PTR(-EINVAL);
		mc->pinctrl_active = ERR_PTR(-EINVAL);
	}

	mc->irq_phone_active = gpio_to_irq(mc->gpio_phone_active);

	if (mc->gpio_sim_detect)
		mc->irq_sim_detect = gpio_to_irq(mc->gpio_sim_detect);

	xmm6262_get_ops(mc);

	ret = request_irq(mc->irq_phone_active, phone_active_irq_handler,
				IRQF_NO_SUSPEND | IRQF_TRIGGER_HIGH,
				"phone_active", mc);
	if (ret) {
		mif_err("failed to request_irq:%d\n", ret);
		goto err_phone_active_request_irq;
	}

/* not needed for suspend/resume */
/*	ret = enable_irq_wake(mc->irq_phone_active);
	if (ret) {
		mif_err("failed to enable_irq_wake:%d\n", ret);
		goto err_phone_active_set_wake_irq;
	}
*/
	/* initialize sim_state if gpio_sim_detect exists */
	mc->sim_state.online = false;
	mc->sim_state.changed = false;
	if (mc->gpio_sim_detect) {
		ret = request_irq(mc->irq_sim_detect, sim_detect_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"sim_detect", mc);
		if (ret) {
			mif_err("failed to request_irq: %d\n", ret);
			goto err_sim_detect_request_irq;
		}

		ret = enable_irq_wake(mc->irq_sim_detect);
		if (ret) {
			mif_err("failed to enable_irq_wake: %d\n", ret);
			goto err_sim_detect_set_wake_irq;
		}

		/* initialize sim_state => insert: gpio=0, remove: gpio=1 */
		mc->sim_state.online =
			gpio_get_value(mc->gpio_sim_detect) == mc->sim_polarity;
	}

	return ret;

err_sim_detect_set_wake_irq:
	free_irq(mc->irq_sim_detect, mc);
err_sim_detect_request_irq:
	mc->sim_state.online = false;
	mc->sim_state.changed = false;
err_phone_active_set_wake_irq:
	free_irq(mc->irq_phone_active, mc);
err_phone_active_request_irq:
	return ret;
}
