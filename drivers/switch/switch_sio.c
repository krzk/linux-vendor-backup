/*
 *  drivers/switch/switch_sio.c
 *
 * Copyright (C) 2009 Samsung Electronics Co. Ltd.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/kdev_t.h>
#include <plat/mux.h>
#include <mach/hardware.h>
#include <mach/sec_param.h>
#if defined(CONFIG_USB_ANDROID)
#include <linux/usb/android_composite.h>

#undef PARAM_CONSOLE_MODE

extern void android_usb_set_connected(int connected);
#endif


#if defined(CONFIG_FSA9480_MICROUSB) || defined(CONFIG_MAX14577_MICROUSB)
extern void mcirousb_usbpath_change(int usb_path);
#endif

extern int get_real_usbic_state(void);
extern int set_usbmenu_mode(void);

extern struct class *sec_class;
extern void (*sec_set_param_value)(int idx, void *value);
extern void (*sec_get_param_value)(int idx, void *value);

extern struct device *sio_switch_dev;

typedef enum
{
	USB_SW_AP,
	USB_SW_CP
} USB_SWITCH_MODE;

typedef enum
{
	UART_SW_AP,
	UART_SW_CP
} UART_SWITCH_MODE;

typedef enum
{
	AP_USB_MODE,
	AP_UART_MODE,
	CP_USB_MODE,
	CP_UART_MODE,
} USB_UART_SW_MODE_TYPE;

/* 1 : PDA, 2 : MODEM */
#define SWITCH_PDA			1
#define SWITCH_MODEM		2

#define USBSTATE_TA_CHARGER		3
#define USBSTATE_USB_CHARGER		2
#define USBSTATE_USB_CABLE		1
#define USBSTATE_NO_DEVICE		0
#define USBSTATE_PHONE_USB		-1


#ifndef GPIO_LEVEL_LOW
#define GPIO_LEVEL_LOW   0
#endif
#ifndef GPIO_LEVEL_HIGH
#define GPIO_LEVEL_HIGH  1
#endif

#define USB_SEL_MASK    (1 << 0)
#define UART_SEL_MASK   (1 << 1)

static int usb_path = SWITCH_PDA;
static int uart_current_owner = SWITCH_MODEM;

struct delayed_work sio_switch_init_work;

static void sio_switch_gpio_init(void)
{
    /*do not gpio init for prevent lockup when boot up.*/

	if (gpio_request(OMAP_GPIO_UART_SEL, "UART_SEL"))
	{
		printk(KERN_ERR "Filed to request OMAP_GPIO_UART_SEL!\n");
	}
	gpio_direction_output(OMAP_GPIO_UART_SEL, GPIO_LEVEL_LOW);

#ifdef CONFIG_MACH_SAMSUNG_HERON
//	if (gpio_request(OMAP_GPIO_USBSW_I2C_SCL, "USB_SEL"))	// Unknown pin by SJ
#else
	if (gpio_request(OMAP_GPIO_CP_VBUS_EN, "USB_SEL"))
#endif
	{
#ifdef CONFIG_MACH_SAMSUNG_HERON
		printk(KERN_ERR "Filed to request OMAP_GPIO_USBSW_I2C_SCL!\n");
#else
		printk(KERN_ERR "Filed to request OMAP_GPIO_CP_VBUS_EN!\n");
#endif

	}
#ifdef CONFIG_MACH_SAMSUNG_HERON
//	gpio_direction_output(OMAP_GPIO_USBSW_I2C_SCL, GPIO_LEVEL_LOW);	//Unknown pin by SJ
#else
#if defined(CONFIG_CHN_KERNEL_STE_LATONA)
	gpio_direction_output(OMAP_GPIO_CP_VBUS_EN, GPIO_LEVEL_HIGH);
#else
	gpio_direction_output(OMAP_GPIO_CP_VBUS_EN, GPIO_LEVEL_LOW);
#endif	
#endif

}

static void usb_api_set_usb_switch(USB_SWITCH_MODE usb_switch)
{
	if(usb_switch == USB_SW_CP)
	{
#if defined(CONFIG_USB_ANDROID)
		android_usb_set_connected(0);
#endif
		//USB_SEL GPIO Set High => CP USB enable
#ifdef CONFIG_MACH_SAMSUNG_HERON
		gpio_set_value(OMAP_GPIO_USBSW_I2C_SCL, GPIO_LEVEL_HIGH);
#else
		gpio_set_value(OMAP_GPIO_CP_VBUS_EN, GPIO_LEVEL_HIGH);
#endif

#if defined(CONFIG_CHN_KENEL_LATONA_TEMP)
#if defined(CONFIG_CHN_KERNEL_CPNAND)
		/* Start. Heekwon Ko 20101204: Step for switching USB to CP side */
		/* 1. Press CP Download Key, 2. change USB pass to CP side */
		/* 3. Power on CP, 4. Reset CP */
		/* The mdelay(20) is arbitrary value for securing stability */
#if !defined(CONFIG_CHN_KERNEL_STE_LATONA)		
		/* Press CP Download key. GPIO 56, High */
		gpio_set_value(OMAP_GPIO_CP_DOWNLOAD, GPIO_LEVEL_HIGH);
#else
		/* Press CP Download key. GPIO 14, High */
		gpio_set_value(OMAP_GPIO_USB_PHONE_DOWNLOAD, GPIO_LEVEL_HIGH);
#endif		
		mdelay(20);
#endif
#endif

#if defined(CONFIG_FSA9480_MICROUSB) || defined(CONFIG_MAX14577_MICROUSB)
		mcirousb_usbpath_change(1);
#endif
		usb_path = SWITCH_MODEM;

#if defined(CONFIG_CHN_KENEL_LATONA_TEMP)
#if defined(CONFIG_CHN_KERNEL_CPNAND)
		/* CP power on. GPIO 167, High -> Low */
		/* The Spec. duration is 62msec between High and Low */
		/* mdelay(200) is 100msec */
		gpio_set_value(OMAP_GPIO_PHONE_ON, GPIO_LEVEL_HIGH);
		mdelay(200);
		gpio_set_value(OMAP_GPIO_PHONE_ON, GPIO_LEVEL_LOW);

		mdelay(20);
		
		/* CP reset. GPIO 43, Low -> High */
		/* The mdelay(100) is arbitrary value for securing stability. */
		gpio_set_value(OMAP_GPIO_CP_RST, GPIO_LEVEL_LOW);
		mdelay(100);
		gpio_set_value(OMAP_GPIO_CP_RST, GPIO_LEVEL_HIGH);
		/* End. Heekwon Ko 20101204: Step for switching USB to CP side */
#endif	
#endif
	}
	else
	{
#ifdef CONFIG_MACH_SAMSUNG_HERON
		//USB_SEL GPIO Set Low => AP USB enable
		gpio_set_value(OMAP_GPIO_USBSW_I2C_SCL, GPIO_LEVEL_LOW);
#else
		//USB_SEL GPIO Set Low => AP USB enable
#if defined(CONFIG_CHN_KERNEL_STE_LATONA)
	gpio_direction_output(OMAP_GPIO_CP_VBUS_EN, GPIO_LEVEL_HIGH);
#else
	gpio_direction_output(OMAP_GPIO_CP_VBUS_EN, GPIO_LEVEL_LOW);
#endif
#endif

#if defined(CONFIG_FSA9480_MICROUSB) || defined(CONFIG_MAX14577_MICROUSB)
		mcirousb_usbpath_change(0);
#endif
#if defined(CONFIG_USB_ANDROID)
		android_usb_set_connected(1);
#endif
		usb_path = SWITCH_PDA;
	}
}

static void sio_switch_config(USB_UART_SW_MODE_TYPE sio_mode)
{
	switch (sio_mode)
	{
		case AP_USB_MODE:
			usb_api_set_usb_switch(USB_SW_AP);
			break;
		case CP_USB_MODE:
			usb_api_set_usb_switch(USB_SW_CP);
			break;
		case AP_UART_MODE:
			gpio_set_value(OMAP_GPIO_UART_SEL, GPIO_LEVEL_HIGH);
			break;
		case CP_UART_MODE:
			gpio_set_value(OMAP_GPIO_UART_SEL, GPIO_LEVEL_LOW);
			break;
		default:
			printk("sio_switch_config error");
			break;
	}
}

/* for sysfs control (/sys/class/sec/switch/usb_sel) */
static ssize_t usb_sel_show
(
	struct device *dev,
	struct device_attribute *attr,
	char *buf
)
{
	return snprintf(buf, PAGE_SIZE, "USB Switch : %s\n", (usb_path == SWITCH_PDA) ? "PDA" : "MODEM");
}

static ssize_t usb_sel_store
(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t size
)
{
	int switch_sel;
	int path_save = 1;

	if (sec_get_param_value)
	{
		sec_get_param_value(__SWITCH_SEL, &switch_sel);
	}

	if(strstr(buf, "PDA") || strstr(buf, "pda"))
	{
		if(usb_path != SWITCH_PDA)
		{
			sio_switch_config(AP_USB_MODE);
		}
		usb_path = SWITCH_PDA;
		switch_sel |= USB_SEL_MASK;
		printk("[USB Switch] Path : PDA\n");
	}
	else if(strstr(buf, "MODEM") || strstr(buf, "modem"))
	{
		if(usb_path != SWITCH_MODEM)
		{
			sio_switch_config(CP_USB_MODE);
		}
		usb_path = SWITCH_MODEM;
		switch_sel &= ~USB_SEL_MASK;
		printk("[USB Switch] Path : MODEM\n");
	}

	if(strstr(buf, "NOSAVE") || strstr(buf, "nosave"))
	{
		path_save = 0;
		printk("[USB Switch] path is not saved\n");
	}

	if(path_save)
	{
		if (sec_set_param_value)
		{
			sec_set_param_value(__SWITCH_SEL, &switch_sel);
		}
	}

	return size;
}

/* for sysfs control (/sys/class/sec/switch/uart_sel) */
static ssize_t uart_switch_show
(
	struct device *dev,
	struct device_attribute *attr,
	char *buf
)
{
	if ( uart_current_owner == SWITCH_PDA )
	{
		return snprintf(buf, PAGE_SIZE, "[UART Switch] Current UART owner = PDA\n");	
	}
	else if ( uart_current_owner == SWITCH_MODEM )
	{
		return snprintf(buf, PAGE_SIZE, "[UART Switch] Current UART owner = MODEM\n");
	}

	return 0;
}

static ssize_t uart_switch_store
(
	struct device *dev,
	struct device_attribute *attr,
	const char *buf,
	size_t size
)
{
	u8 switch_sel;
#ifdef PARAM_CONSOLE_MODE
	u8 console_mode;
#endif
	int path_save = 1;

	if (sec_get_param_value)
	{
		sec_get_param_value(__SWITCH_SEL, &switch_sel);
#ifdef PARAM_CONSOLE_MODE
		sec_get_param_value(__CONSOLE_MODE, &console_mode);
#endif
	}

	if (strstr(buf, "PDA") || strstr(buf, "pda"))
	{
		if(uart_current_owner != SWITCH_PDA)
		{
			sio_switch_config(AP_UART_MODE);
		}
		uart_current_owner = SWITCH_PDA;
		switch_sel |= UART_SEL_MASK;
#ifdef PARAM_CONSOLE_MODE
		console_mode = 1;
#endif
		printk("[UART Switch] Path : PDA\n");
	}
	else if (strstr(buf, "MODEM") || strstr(buf, "modem"))
	{
		if(uart_current_owner != SWITCH_MODEM)
		{
			sio_switch_config(CP_UART_MODE);
		}
		uart_current_owner = SWITCH_MODEM;
		switch_sel &= ~UART_SEL_MASK;
#ifdef PARAM_CONSOLE_MODE
		console_mode = 0;
#endif
		printk("[UART Switch] Path : MODEM\n");
	}

	if(strstr(buf, "NOSAVE") || strstr(buf, "nosave"))
	{
		path_save = 0;
		printk("[UART Switch] path is not saved\n");
	}

	if(path_save)
	{
		if (sec_set_param_value)
		{
			sec_set_param_value(__SWITCH_SEL, &switch_sel);
#ifdef PARAM_CONSOLE_MODE
			sec_set_param_value(__CONSOLE_MODE, &console_mode);
#endif
		}
	}

	return size;
}

static DEVICE_ATTR(usb_sel, S_IRUGO | S_IWUGO | S_IRUSR | S_IWUSR, usb_sel_show, usb_sel_store);
static DEVICE_ATTR(uart_sel, S_IRUGO | S_IWUGO | S_IRUSR | S_IWUSR, uart_switch_show, uart_switch_store);

static void sio_switch_init_worker(struct work_struct *ignored)
{
	int switch_sel;

	if (sec_get_param_value) {
		sec_get_param_value(__SWITCH_SEL, &switch_sel);
		cancel_delayed_work(&sio_switch_init_work);

#ifdef CONFIG_USB_ANDROID_SAMSUNG_MENU_SEL		
		/*set usb menu mode*/
		set_usbmenu_mode();
#endif
	} else {
		schedule_delayed_work(&sio_switch_init_work, msecs_to_jiffies(100));		
		return;
	}

	if (switch_sel & USB_SEL_MASK)
		{
		usb_path = SWITCH_PDA;
		sio_switch_config(AP_USB_MODE);
		}
	else
		usb_path = SWITCH_MODEM;

	if (switch_sel & UART_SEL_MASK)
		uart_current_owner = SWITCH_PDA;
	else
		uart_current_owner = SWITCH_MODEM;

	if (uart_current_owner == SWITCH_PDA)
	{
		sio_switch_config(AP_UART_MODE);
	}
	else if (uart_current_owner == SWITCH_MODEM)
	{
		printk("----------------- Cutting off PDA UART ---------------------\n");
		sio_switch_config(CP_UART_MODE);
	}

	if (usb_path == SWITCH_MODEM)
	{
		sio_switch_config(CP_USB_MODE);
	}
}

static int sio_switch_probe(struct platform_device *pdev)
{
	int ret = 0;

	sio_switch_gpio_init();

#if 0
	sio_switch_dev = device_create(sec_class, NULL, MKDEV(0, 0), NULL, "switch");

	if (IS_ERR(sio_switch_dev))
	{
		pr_err("Failed to create device(sio_switch)!\n");
		ret = PTR_ERR(sio_switch_dev);
		return ret;
	}
#endif

	if ((ret = device_create_file(sio_switch_dev, &dev_attr_uart_sel)) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_uart_sel.attr.name);
		goto err_uart;
	}

	if ((ret = device_create_file(sio_switch_dev, &dev_attr_usb_sel)) < 0)
	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_sel.attr.name);
		goto err_usb;
	}

	INIT_DELAYED_WORK(&sio_switch_init_work, sio_switch_init_worker);
	schedule_delayed_work(&sio_switch_init_work, msecs_to_jiffies(200));

	printk("[%s]: initialized\n", pdev->name);

	return 0;

err_usb:
	device_remove_file(sio_switch_dev, &dev_attr_uart_sel);

err_uart:
	device_destroy((struct class *)sio_switch_dev, MKDEV(0, 0));

	return ret;
}

static int __devexit sio_switch_remove(struct platform_device *pdev)
{
	device_remove_file(sio_switch_dev, &dev_attr_uart_sel);
	device_remove_file(sio_switch_dev, &dev_attr_usb_sel);
	device_destroy((struct class *)sio_switch_dev, MKDEV(0, 0));

	return 0;
}

static struct platform_driver sio_switch_driver = {
	.probe		= sio_switch_probe,
	.remove		= __devexit_p(sio_switch_remove),
	.driver		= {
		.name	= "switch-sio",
	},
};

static int __init sio_switch_init(void)
{
	return platform_driver_register(&sio_switch_driver);
}

static void __exit sio_switch_exit(void)
{
	platform_driver_unregister(&sio_switch_driver);
}

module_init(sio_switch_init);
module_exit(sio_switch_exit);

MODULE_AUTHOR("SAMSUNG ELECTRONICS CO., LTD");
MODULE_DESCRIPTION("SIO Switch driver");
MODULE_LICENSE("GPL");
