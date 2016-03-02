/*
 * Bluetooth Broadcom GPIO and Low Power Mode control
 *
 *  Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *  Copyright (C) 2011 Google, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/serial_core.h>
#include <linux/wakelock.h>

#include <asm/mach-types.h>

#include <mach/gpio.h>
#include <plat/gpio-cfg.h>

#ifdef CONFIG_SYSTEM_LOAD_ANALYZER
#include <linux/load_analyzer.h>
#endif

#define GPIO_BT_HOST_WAKE		EXYNOS3_GPX2(6)

#define GPIO_BT_EN			EXYNOS3_GPE0(0)
#define GPIO_BT_WAKE			EXYNOS3_GPX3(1)

#define BT_LPM_ENABLE
#define BT_FEM_ENABLE

#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
#define GPIO_BT_FEM_EN			EXYNOS3_GPE1(6)
#include <asm/system_info.h>
#endif

static struct rfkill *bt_rfkill;

struct bcm_bt_lpm {
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct hrtimer enter_lpm_rx_timer;
	ktime_t enter_lpm_rx_delay;

	struct uart_port *uport;

	struct wake_lock host_wake_lock;
	struct wake_lock bt_wake_lock;
	char host_wake_lock_name[15];
	char bt_wake_lock_name[15];
} bt_lpm;

int bt_is_running;

EXPORT_SYMBOL(bt_is_running);

static int bt_is_rx_running;
static int bt_is_tx_running;

extern int s3c_gpio_slp_cfgpin(unsigned int pin, unsigned int config);
extern int s3c_gpio_slp_setpull_updown(unsigned int pin, unsigned int config);

static unsigned int bt_uart_on_table[][4] = {
	{EXYNOS3_GPA0(0), 2, 2, S3C_GPIO_PULL_NONE},
	{EXYNOS3_GPA0(1), 2, 2, S3C_GPIO_PULL_NONE},
	{EXYNOS3_GPA0(2), 2, 2, S3C_GPIO_PULL_NONE},
	{EXYNOS3_GPA0(3), 2, 2, S3C_GPIO_PULL_NONE},
};

void bt_config_gpio_table(int array_size, unsigned int (*gpio_table)[4])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(gpio_table[i][1]));
		s3c_gpio_setpull(gpio, gpio_table[i][3]);
		if (gpio_table[i][2] != 2)
			gpio_set_value(gpio, gpio_table[i][2]);
	}
}

void bt_uart_rts_ctrl(int flag)
{
	if (!gpio_get_value(GPIO_BT_EN))
		return;
	if (flag) {
		/* BT RTS Set to HIGH */
		s3c_gpio_cfgpin(EXYNOS3_GPA0(3), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(EXYNOS3_GPA0(3), S3C_GPIO_PULL_NONE);
		gpio_set_value(EXYNOS3_GPA0(3), 1);
	} else {
		/* restore BT RTS state */
		s3c_gpio_cfgpin(EXYNOS3_GPA0(3), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(EXYNOS3_GPA0(3), S3C_GPIO_PULL_NONE);
	}
}

EXPORT_SYMBOL(bt_uart_rts_ctrl);

static void update_bt_is_running(void)
{
	if (bt_is_rx_running || bt_is_tx_running)
		bt_is_running = 1;
	else
		bt_is_running = 0;

}

static int bcm4334w_bt_rfkill_set_power(void *data, bool blocked)
{
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	if (!blocked) {
		pr_info("[BT] Bluetooth Power On.\n");

		/* This logic is made for new BCM4343W uart logic */
		s3c_gpio_cfgpin(EXYNOS3_GPA0(3), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(EXYNOS3_GPA0(3), S3C_GPIO_PULL_NONE);
		gpio_set_value(EXYNOS3_GPA0(3), 0);

#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
		if (system_rev < 0x07) {
			gpio_set_value(GPIO_BT_FEM_EN, 1);
			msleep(50);
		}
#endif
		gpio_set_value(GPIO_BT_EN, 1);
		bt_is_running = 1;

		msleep(100);
		bt_config_gpio_table(ARRAY_SIZE(bt_uart_on_table),
					bt_uart_on_table);
#ifdef CONFIG_SYSTEM_LOAD_ANALYZER
		store_external_load_factor(CONN_BT_ENABLED, 1);
#endif
	} else {
		pr_info("[BT] Bluetooth Power Off.\n");
		bt_is_running = 0;

		gpio_set_value(GPIO_BT_EN, 0);
#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
		if (system_rev < 0x07)
			gpio_set_value(GPIO_BT_FEM_EN, 0);
#endif
#ifdef CONFIG_SYSTEM_LOAD_ANALYZER
		store_external_load_factor(CONN_BT_ENABLED, 0);
#endif
	}

	bt_is_rx_running =0;
	bt_is_tx_running =0;

	return 0;
}

static const struct rfkill_ops bcm4334w_bt_rfkill_ops = {
	.set_block = bcm4334w_bt_rfkill_set_power,
};

#ifdef BT_LPM_ENABLE
static void set_wake_locked(int wake)
{
	if (wake)
		wake_lock(&bt_lpm.bt_wake_lock);

	gpio_set_value(GPIO_BT_WAKE, wake);
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer)
{
	if (bt_lpm.uport != NULL)
		set_wake_locked(0);

	bt_is_tx_running = 0;
	update_bt_is_running();

	wake_lock_timeout(&bt_lpm.bt_wake_lock, HZ/2);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart enter_rx_lpm(struct hrtimer *timer)
{
	pr_debug("[BT] Unlock bt_is_rx_running");
	bt_is_rx_running = 0;
	update_bt_is_running();

	pr_debug("[BT]bt_is_running =%d (tx=%d,rx=%d)", bt_is_running,
				bt_is_tx_running, bt_is_rx_running);
	return HRTIMER_NORESTART;
}


void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport)
{
	bt_lpm.uport = uport;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	bt_is_tx_running = 1;
	update_bt_is_running();

	set_wake_locked(1);

	pr_debug("[BT] bcm_bt_lpm_exit_lpm_locked\n");
	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);
}

static void update_host_wake_locked(int host_wake)
{
	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_rx_timer);

	if (host_wake) {
		pr_debug("[BT] ISR is high");
		bt_is_rx_running = 1;
		update_bt_is_running();

		wake_lock(&bt_lpm.host_wake_lock);
	} else  {
		/* Take a timed wakelock, so that upper layers can take it.
		 * The chipset deasserts the hostwake lock, when there is no
		 * more data to send.
		 */
		wake_lock_timeout(&bt_lpm.host_wake_lock, HZ/4*3); /* 750ms */

		pr_debug("[BT] ISR is low");
		/* udpate bt status value after timeout */
		hrtimer_start(&bt_lpm.enter_lpm_rx_timer,
				bt_lpm.enter_lpm_rx_delay, HRTIMER_MODE_REL);
	}
}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;

	host_wake = gpio_get_value(GPIO_BT_HOST_WAKE);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}

	update_host_wake_locked(host_wake);

	return IRQ_HANDLED;
}

static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int irq;
	int ret;

	/* For Tx */
	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;


	/* For Rx */
	hrtimer_init(&bt_lpm.enter_lpm_rx_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_rx_delay = ktime_set(0, NSEC_PER_SEC/2);  /* 0.5 sec */
	bt_lpm.enter_lpm_rx_timer.function = enter_rx_lpm;

	bt_lpm.host_wake = 0;
	bt_is_running = 0;

	snprintf(bt_lpm.host_wake_lock_name, sizeof(bt_lpm.host_wake_lock_name),
			"BT_host_wake");
	wake_lock_init(&bt_lpm.host_wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.host_wake_lock_name);

	snprintf(bt_lpm.bt_wake_lock_name, sizeof(bt_lpm.bt_wake_lock_name),
			"BT_bt_wake");
	wake_lock_init(&bt_lpm.bt_wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.bt_wake_lock_name);

	irq = gpio_to_irq(GPIO_BT_HOST_WAKE);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
		"bt host_wake", NULL);
	if (ret) {
		pr_err("[BT] Request_host wake irq failed.\n");
		return ret;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		pr_err("[BT] Set_irq_wake failed.\n");
		return ret;
	}

	return 0;
}
#endif

static int bcm4334w_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;
#ifdef BT_LPM_ENABLE
	int ret;
#endif
	rc = gpio_request(GPIO_BT_EN, "bcm4334w_bten_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_EN request failed.\n");
		return rc;
	}
	rc = gpio_request(GPIO_BT_WAKE, "bcm4334w_btwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_WAKE request failed.\n");
		gpio_free(GPIO_BT_EN);
		return rc;
	}
	rc = gpio_request(GPIO_BT_HOST_WAKE, "bcm4334w_bthostwake_gpio");
	if (unlikely(rc)) {
		pr_err("[BT] GPIO_BT_HOST_WAKE request failed.\n");
		gpio_free(GPIO_BT_WAKE);
		gpio_free(GPIO_BT_EN);
		return rc;
	}

#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
	if (system_rev < 0x07) {
		rc = gpio_request(GPIO_BT_FEM_EN, "bcm4334w_bt_fem_en_gpio");
		if (unlikely(rc)) {
			pr_err("[BT] GPIO_BT_FEM_EN request failed.\n");
			gpio_free(GPIO_BT_HOST_WAKE);
			gpio_free(GPIO_BT_WAKE);
			gpio_free(GPIO_BT_EN);
			return rc;
		}
	}
#endif

	gpio_direction_input(GPIO_BT_HOST_WAKE);
	gpio_direction_output(GPIO_BT_WAKE, 0);
	gpio_direction_output(GPIO_BT_EN, 0);

#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
	if (system_rev < 0x07)
		gpio_direction_output(GPIO_BT_FEM_EN, 0);
#endif

	bt_rfkill = rfkill_alloc("bcm4334w Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &bcm4334w_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		pr_err("[BT] bt_rfkill alloc failed.\n");
		gpio_free(GPIO_BT_HOST_WAKE);
		gpio_free(GPIO_BT_WAKE);
		gpio_free(GPIO_BT_EN);
#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
		if (system_rev < 0x07)
			gpio_free(GPIO_BT_FEM_EN);
#endif
		return -ENOMEM;
	}

	rfkill_init_sw_state(bt_rfkill, 0);

	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		pr_err("[BT] bt_rfkill register failed.\n");
		rfkill_destroy(bt_rfkill);
		gpio_free(GPIO_BT_HOST_WAKE);
		gpio_free(GPIO_BT_WAKE);
		gpio_free(GPIO_BT_EN);
#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
		if (system_rev < 0x07)
			gpio_free(GPIO_BT_FEM_EN);
#endif
		return -1;
	}

	rfkill_set_sw_state(bt_rfkill, true);

#ifdef BT_LPM_ENABLE
	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);

		gpio_free(GPIO_BT_HOST_WAKE);
		gpio_free(GPIO_BT_WAKE);
		gpio_free(GPIO_BT_EN);
#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
		if (system_rev < 0x07)
			gpio_free(GPIO_BT_FEM_EN);
#endif
	}
#endif

	return rc;
}

static int bcm4334w_bluetooth_remove(struct platform_device *pdev)
{
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

#if defined(CONFIG_MACH_WC1) && defined(BT_FEM_ENABLE)
	if (system_rev < 0x07)
		gpio_free(GPIO_BT_FEM_EN);
#endif

	gpio_free(GPIO_BT_EN);
	gpio_free(GPIO_BT_WAKE);
	gpio_free(GPIO_BT_HOST_WAKE);

	wake_lock_destroy(&bt_lpm.host_wake_lock);
	wake_lock_destroy(&bt_lpm.bt_wake_lock);

	return 0;
}

static struct platform_driver bcm4334w_bluetooth_platform_driver = {
	.probe = bcm4334w_bluetooth_probe,
	.remove = bcm4334w_bluetooth_remove,
	.driver = {
		   .name = "bcm4334w_bluetooth",
		   .owner = THIS_MODULE,
		   },
};

static int __init bcm4334w_bluetooth_init(void)
{
	return platform_driver_register(&bcm4334w_bluetooth_platform_driver);
}

static void __exit bcm4334w_bluetooth_exit(void)
{
	platform_driver_unregister(&bcm4334w_bluetooth_platform_driver);
}


module_init(bcm4334w_bluetooth_init);
module_exit(bcm4334w_bluetooth_exit);

MODULE_ALIAS("platform:bcm4334w");
MODULE_DESCRIPTION("bcm4334w_bluetooth");
MODULE_LICENSE("GPL");
