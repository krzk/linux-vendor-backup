/*
 *  drivers/usb/gadget/menu_setting.c
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
 * author : Grant jung (grant.jung@samsung.com)
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
//#include <mach/mux.h>
#include <mach/hardware.h>
#include <linux/device.h>
#include <linux/usb/android_composite.h>
#include <mach/sec_param.h>

#include "usb_menu_sel.h"

/*for switch dev*/
#define USBMENU_SWITCH_DEV

#ifdef USBMENU_SWITCH_DEV
#include <linux/switch.h>
#endif

/////////////////////////////////////////////////////////////////////////////////
//
// Definitions
//
/*for inform ACM driver is connected*/
#ifdef USBMENU_SWITCH_DEV
//usb indication name for USB connection for acm driver to notify switch driver.
#define ACM_DRIVER_NAME  "usb_acm_gadget" 
#endif

static int usb_path = 0;
static int usb_power = 2;
//static int usb_state = 0;
static char bFactoryReset = 0;
int usb_menu_path = 0;
int usb_connected = 0;
int mtp_mode_on = 0;
int inaskonstatus = 0;

#ifdef USBMENU_SWITCH_DEV
struct switch_dev indicator_dev;
//int mtp_power_off = 0;

/* 0: acm disconnected 1: acm connected*/
int acmusbstatus=0; 
#endif

extern int get_real_usbic_state(void);
extern int get_usbic_state(void);

extern int usb_switch_select(int enable);
extern void ap_usb_power_on(int on);

extern struct class *sec_class;
extern void (*sec_set_param_value)(int idx, void *value);
extern void (*sec_get_param_value)(int idx, void *value);

//static struct device *usbswitch_dev;
extern struct device *sio_switch_dev;

void usb_path_store(int sel)
{
	if (bFactoryReset)
		return;

	/*get param value again before store to sync*/
	if (sec_get_param_value)
	{
	sec_get_param_value(__SWITCH_SEL, &usb_menu_path);
	}

	if (sel == USBSTATUS_ADB)
		usb_menu_path |= USBSTATUS_ADB;
	else if (sel == USBSTATUS_ADB_REMOVE)
		usb_menu_path &= ~USBSTATUS_ADB;
	else {
		usb_menu_path &= (USBSTATUS_UART | USBSTATUS_USB | USBSTATUS_ADB);
		usb_menu_path |= sel;
	}

	printk("[USB] %s USB MENU value is 0x%x\n",__func__,usb_menu_path);

	if (sec_set_param_value)
		{
		sec_set_param_value(__SWITCH_SEL, &usb_menu_path);
		}

}
EXPORT_SYMBOL(usb_path_store);

int get_usbmenupath_value(void)
{
	int result=0;

	/*get param value again before store to sync*/
	if (sec_get_param_value)
	{
		sec_get_param_value(__SWITCH_SEL, &result);
		printk("[USB] %s: USB menu value is %d\n",__func__,result);
	}

	return result;
}
EXPORT_SYMBOL(get_usbmenupath_value);


#ifdef USBMENU_SWITCH_DEV
void UsbIndicator(u8 state)
{
	printk("[UsbIndicator] state=%d \n", state);
	/*it should update before calling switch_set_state*/
	acmusbstatus=state;

	switch_set_state(&indicator_dev, state);
}
#endif


static ssize_t UsbMenuSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	

       /*get value from SWITCH_SEL*/
	   int tempUsbMenu = get_usbmenupath_value();
   
		if (tempUsbMenu & USBSTATUS_ADB) 
			return sprintf(buf, "%s[UsbMenuSel] ACM_ADB_UMS\n", buf);	

		else if (tempUsbMenu & USBSTATUS_UMS) 
			return sprintf(buf, "%s[UsbMenuSel] UMS\n", buf);	
		
		else if (tempUsbMenu & USBSTATUS_SAMSUNG_KIES) 
			return sprintf(buf, "%s[UsbMenuSel] ACM_UMS\n", buf);	
		
		else if (tempUsbMenu & USBSTATUS_MTPONLY) 
			return sprintf(buf, "%s[UsbMenuSel] MTP\n", buf);	
		
		else if (tempUsbMenu & USBSTATUS_ASKON) 
			return sprintf(buf, "%s[UsbMenuSel] ASK\n", buf);	
		
		else if (tempUsbMenu & USBSTATUS_VTP) 
			return sprintf(buf, "%s[UsbMenuSel] VTP\n", buf);	

		return sprintf(buf, "%s[UsbMenuSel] Invaild USB mode!\n", buf);		
}

static ssize_t UsbMenuSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{	
	
	if (strncmp(buf, "KIES", 4) == 0)
	{
		usb_path_store(USBSTATUS_SAMSUNG_KIES);
		usb_switch_select(USBSTATUS_ACMONLY);
	}

	if (strncmp(buf, "MTP", 3) == 0)
	{
		usb_path_store(USBSTATUS_MTPONLY);
		usb_switch_select(USBSTATUS_MTPONLY);
	}

	if (strncmp(buf, "UMS", 3) == 0)
	{
		usb_path_store(USBSTATUS_UMS);
		usb_switch_select(USBSTATUS_UMS);
	}

	if (strncmp(buf, "VTP", 3) == 0)
	{
		usb_path_store(USBSTATUS_VTP);
		usb_switch_select(USBSTATUS_VTP);
	}

	if (strncmp(buf, "ASKON", 5) == 0)
	{		
		usb_path_store(USBSTATUS_ASKON);
		usb_switch_select(USBSTATUS_ASKON);
		inaskonstatus = 0;
	}


	return size;
}

static DEVICE_ATTR(UsbMenuSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, UsbMenuSel_switch_show, UsbMenuSel_switch_store);

static ssize_t AskOnStatus_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	if(inaskonstatus)
		return sprintf(buf, "%s\n", "Blocking");
	else
		return sprintf(buf, "%s\n", "NonBlocking");
	
}

static ssize_t AskOnStatus_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{ 
	return size;
}

static DEVICE_ATTR(AskOnStatus, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, AskOnStatus_switch_show, AskOnStatus_switch_store);


static ssize_t AskOnMenuSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
		return sprintf(buf, "%s[AskOnMenuSel] Port test ready!! \n", buf);	
}

static ssize_t AskOnMenuSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{		
	inaskonstatus = 1;

	printk("[USB] %s\n",__func__);

	if (strncmp(buf, "KIES", 4) == 0)
	{
		usb_switch_select(USBSTATUS_SAMSUNG_KIES);
	}

	if (strncmp(buf, "MTP", 3) == 0)
	{
		mtp_mode_on = 1;
		usb_switch_select(USBSTATUS_MTPONLY);
	}

	if (strncmp(buf, "UMS", 3) == 0)
	{
		usb_switch_select(USBSTATUS_UMS);
	}

	if (strncmp(buf, "VTP", 3) == 0)
	{
		usb_switch_select(USBSTATUS_VTP);
	}

	return size;
}

static DEVICE_ATTR(AskOnMenuSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, AskOnMenuSel_switch_show, AskOnMenuSel_switch_store);


static ssize_t Mtp_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
		return sprintf(buf, "%s[Mtp] MtpDeviceOn \n", buf);	
}

static ssize_t Mtp_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{	
	if (strncmp(buf, "Mtp", 3) == 0)
	{
		/* ACM -> KIES */
		if (usb_menu_path & USBSTATUS_SAMSUNG_KIES)
		{
			usb_switch_select(USBSTATUS_SAMSUNG_KIES);
		}

		/* In Askon Mode, mtp_mode_on : 1 -> MTP Only, mtp_mode_on : 0 -> KIES */
		if (usb_menu_path & USBSTATUS_ASKON)
		{
			if (!mtp_mode_on)	
		usb_switch_select(USBSTATUS_SAMSUNG_KIES);
		}

		/* KIES mtp_mode_on : already power on */
		if(mtp_mode_on)
		{
			printk("[Mtp_switch_store]AP USB power on. \n");
			//s3c_udc_power_up();
			mtp_mode_on = 0;
		}
	}
	else if (strncmp(buf, "OFF", 3) == 0)
	{
			printk("[Mtp_switch_store]AP USB power off. \n");
		    inaskonstatus = 0;
		    mtp_mode_on = 0;
			ap_usb_power_on(0);

	}

	return size;
}

static DEVICE_ATTR(Mtp, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, Mtp_switch_show, Mtp_switch_store);

static int mtpinitstatus=0;
static ssize_t MtpInitStatusSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	if(mtpinitstatus == 2)
		return sprintf(buf, "%s\n", "START");
	else
		return sprintf(buf, "%s\n", "STOP");

}

static ssize_t MtpInitStatusSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{
	mtpinitstatus = mtpinitstatus + 2;

	return size;
}

static DEVICE_ATTR(MtpInitStatusSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, MtpInitStatusSel_switch_show, MtpInitStatusSel_switch_store);


static int askinitstatus=0;
static ssize_t AskInitStatusSel_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	if(askinitstatus == 2)
		return sprintf(buf, "%s\n", "START");
	else
		return sprintf(buf, "%s\n", "STOP");
}

static ssize_t AskInitStatusSel_switch_store(struct device *dev, struct device_attribute *attr,	const char *buf, size_t size)
{
	askinitstatus = askinitstatus + 1;

	return size;
}

static DEVICE_ATTR(AskInitStatusSel, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, AskInitStatusSel_switch_show, AskInitStatusSel_switch_store);


static int g_tethering;
static int oldusbstatus;
static ssize_t tethering_switch_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (g_tethering)
		return sprintf(buf, "1\n");
	else			
		return sprintf(buf, "0\n");
}

static ssize_t tethering_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int usbstatus;

	if (strncmp(buf, "1", 1) == 0)
	{
		printk("tethering On\n");

		g_tethering = 1;
		/*store usb menu value before enable VTP*/
		oldusbstatus = get_usbmenupath_value() & 0xfffC;
		usb_switch_select(USBSTATUS_ADB_REMOVE);
		usb_switch_select(USBSTATUS_VTP);
	}
	else if (strncmp(buf, "0", 1) == 0)
	{
		printk("tethering Off\n");

		g_tethering = 0;
		if(oldusbstatus & USBSTATUS_ADB)
			usb_switch_select(USBSTATUS_ADB);
		else
			{
			usb_switch_select(USBSTATUS_ADB_REMOVE);
			usb_switch_select(oldusbstatus);
			}
	}

	return size;
}

static DEVICE_ATTR(tethering, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, tethering_switch_show, tethering_switch_store);


#define MICROUSBIC_USB_CABLE 1
/**********************************************************************
*    Name         : usb_state_show()
*    Description : for sysfs control (/sys/class/sec/switch/usb_state)
*                        return usb state using fsa9480's device1 and device2 register
*                        this function is used only when NPS want to check the usb cable's state.
*    Parameter   :
*                       
*                       
*    Return        : USB cable state's string
*                        USB_STATE_CONFIGURED is returned if usb cable is connected
***********************************************************************/
static ssize_t usb_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int cable_state = get_real_usbic_state();
	//  int cable_state = get_usbic_state();

	sprintf(buf, "%s\n", (cable_state == MICROUSBIC_USB_CABLE)?"USB_STATE_CONFIGURED":"USB_STATE_NOTCONFIGURED");

	return sprintf(buf, "%s\n", buf);
} 


/**********************************************************************
*    Name         : usb_state_store()
*    Description : for sysfs control (/sys/class/sec/switch/usb_state)
*                        noting to do.
*    Parameter   :
*                       
*                       
*    Return        : None
*
***********************************************************************/
static ssize_t usb_state_store(
		struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	printk("[USBMENU]%s\n ", __func__);
	return 0;
}

/*sysfs for usb cable's state.*/
static DEVICE_ATTR(usb_state, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, usb_state_show, usb_state_store);


#ifdef USBMENU_SWITCH_DEV
static ssize_t print_acm_switch_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", ACM_DRIVER_NAME);
}

static ssize_t print_acm_switch_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, "%s\n", (acmusbstatus ? "online" : "offline"));
}

#endif /*end of USBMENU_SWITCH_DEV*/

void UsbMenuSel_Createsysfs(void)
{
    printk("[USB] %s\n",__func__);
	int ret;

     //usbswitch_dev = device_create(sec_class, NULL, MKDEV(0, 0), NULL, "switch");

#if 0
	 if (IS_ERR(usbswitch_dev))
	{
		printk("[USB] Failed to create device(usbmenu_switch)!\n");
	}
#endif
	
	if (device_create_file(sio_switch_dev, &dev_attr_UsbMenuSel) < 0)
		printk("[USB] Failed to create device file(%s)!\n", dev_attr_UsbMenuSel.attr.name);
	 
    if (device_create_file(sio_switch_dev, &dev_attr_AskOnMenuSel) < 0)
		printk("[USB] Failed to create device file(%s)!\n", dev_attr_AskOnMenuSel.attr.name);

    if (device_create_file(sio_switch_dev, &dev_attr_Mtp) < 0)
		printk("[USB] Failed to create device file(%s)!\n", dev_attr_Mtp.attr.name);

    if (device_create_file(sio_switch_dev, &dev_attr_AskOnStatus) < 0)
		printk("[USB] Failed to create device file(%s)!\n", dev_attr_AskOnStatus.attr.name);

	if (device_create_file(sio_switch_dev, &dev_attr_MtpInitStatusSel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_MtpInitStatusSel.attr.name);

	if (device_create_file(sio_switch_dev, &dev_attr_AskInitStatusSel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_AskInitStatusSel.attr.name);	

	if (device_create_file(sio_switch_dev, &dev_attr_tethering) < 0)		
		pr_err("Failed to create device file(%s)!\n", dev_attr_tethering.attr.name);

	if (device_create_file(sio_switch_dev, &dev_attr_usb_state) < 0)		
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);

#ifdef USBMENU_SWITCH_DEV
   /*register usb indicator*/
	indicator_dev.name = ACM_DRIVER_NAME;
	indicator_dev.print_name = print_acm_switch_name;
	indicator_dev.print_state = print_acm_switch_state;
	ret = switch_dev_register(&indicator_dev);
	if(ret<0)
		printk("[ERROR] %s: failed to register acm switch indicator!!!\n",__func__);
#endif

	/*get param value*/
	if (sec_get_param_value)
	{
	sec_get_param_value(__SWITCH_SEL, &usb_menu_path);
	printk("[USB] %s: USB menu value is %d\n",__func__,usb_menu_path);
	}

}
EXPORT_SYMBOL(UsbMenuSel_Createsysfs);


void cleanup_UsbMenuSel(void)
{
	device_remove_file(sio_switch_dev, &dev_attr_UsbMenuSel);
	device_remove_file(sio_switch_dev, &dev_attr_AskOnMenuSel);
	device_remove_file(sio_switch_dev, &dev_attr_Mtp);
	device_remove_file(sio_switch_dev, &dev_attr_AskOnStatus);
	device_remove_file(sio_switch_dev, &dev_attr_MtpInitStatusSel);
	device_remove_file(sio_switch_dev, &dev_attr_AskInitStatusSel);
	device_remove_file(sio_switch_dev, &dev_attr_tethering);
	device_remove_file(sio_switch_dev, &dev_attr_usb_state);

#ifdef USBMENU_SWITCH_DEV
	switch_dev_unregister(&indicator_dev);
#endif

}
EXPORT_SYMBOL(cleanup_UsbMenuSel);


// 0: USB cable is detached
// 1: attached
void Send_USB_status_to_UsbMenuSel(int status)
{
	int tempUsbMenu = get_usbmenupath_value();

	printk("[Send_USB_status_to_UsbMenuSel] status=%d \n", status);
	if(status)
	{
		printk("[USB] %s : USB cable is attached\n",__func__);

#ifdef USBMENU_SWITCH_DEV
		/*inform indicator when KIES and ADB mode which include ACM device*/
		if(tempUsbMenu & (USBSTATUS_SAMSUNG_KIES|USBSTATUS_ADB|USBSTATUS_ASKON))
			UsbIndicator(1);
#endif
	}
	else
	{
		printk("[USB] %s : USB cable is detached\n",__func__);
		if(inaskonstatus)
		{
			inaskonstatus = 0;
			/*set default switch for ASKON mode*/
			usb_switch_select(USBSTATUS_ACMONLY);
		}
		mtp_mode_on = 0;

#ifdef USBMENU_SWITCH_DEV
		if(tempUsbMenu & (USBSTATUS_SAMSUNG_KIES|USBSTATUS_ADB|USBSTATUS_ASKON))
			UsbIndicator(0);
#endif
	}
}
EXPORT_SYMBOL(Send_USB_status_to_UsbMenuSel);

