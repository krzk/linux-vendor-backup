/*
 * linux/arch/arm/mach-omap2/board-3430ldp.c
 *
 * Copyright (C) 2008 Texas Instruments Inc.
 *
 * Modified from mach-omap2/board-3430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *****************************************************
 *****************************************************
 * modules/camera/s5k5ccgx_platform.c
 *
 * S5K5CCGX sensor driver file related to platform
 *
 * Modified by paladin in Samsung Electronics
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <plat/mux.h>
#include <media/v4l2-int-device.h>
#include "omap34xxcam.h"
#include <../drivers/media/video/isp/ispreg.h>
#include "s5k5ccgx.h"

#include <plat/control.h>
static u16 control_pbias_offset;


#undef DEBUG
#define DEBUG 1

#if (DEBUG == 1)
#define dprintk(x, y...)
#else
#include "dprintk.h"
#endif

static struct v4l2_ifparm ifparm_s5k5ccgx = {
	.if_type = V4L2_IF_TYPE_BT656, // fix me
	.u = {
		.bt656 = {
			.frame_start_on_rising_vs = 0,
			.latch_clk_inv = 0,
			.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT, 
			.clock_min = S5K5CCGX_XCLK,
			.clock_max = S5K5CCGX_XCLK,
			.clock_curr = S5K5CCGX_XCLK,
		},
	},
};

#define S5K5CCGX_BIGGEST_FRAME_BYTE_SIZE  PAGE_ALIGN(1280 * 720 * 2 * 6) //fix for usage of 6 buffers for 720p capture and avoiding camera launch issues.

static struct omap34xxcam_sensor_config s5k5ccgx_hwc = {
	.sensor_isp = 1,
	//.xclk = OMAP34XXCAM_XCLK_A,
	.capture_mem =  S5K5CCGX_BIGGEST_FRAME_BYTE_SIZE, 
};

struct isp_interface_config s5k5ccgx_if_config = {
	.ccdc_par_ser = ISP_PARLL,
	.dataline_shift = 0x2,
	.hsvs_syncdetect = ISPCTRL_SYNC_DETECT_VSRISE,
	.wait_hs_vs = 0x03,
	.strobe = 0x0,
	.prestrobe = 0x0,
	.shutter = 0x0,
	.u.par.par_bridge = 0x3,
	.u.par.par_clk_pol = 0x0,
};

static int s5k5ccgx_enable_gpio(void)
{
	dprintk(CAM_INF, S5K5CCGX_MOD_NAME "s5k5ccgx_enable_gpio is called...\n");

	if (gpio_request(OMAP3430_GPIO_CAMERA_EN1,"CAM EN1") != 0) 
	{
		printk(S5K5CCGX_MOD_NAME "Could not request GPIO %d\n", OMAP3430_GPIO_CAMERA_EN1);
		return -EIO;
	} 
	if (gpio_request(OMAP3430_GPIO_CAMERA_EN2,"CAM EN2") != 0) 
	{
		printk(S5K5CCGX_MOD_NAME "Could not request GPIO %d\n", OMAP3430_GPIO_CAMERA_EN2);
		return -EIO;
	}   
	if (gpio_request(OMAP3430_GPIO_CAMERA_EN3,"CAM EN3") != 0) 
	{
		printk(S5K5CCGX_MOD_NAME "Could not request GPIO %d\n", OMAP3430_GPIO_CAMERA_EN3);
		return -EIO;
	}   	
	if (gpio_request(OMAP3430_GPIO_3MEGA_STBY,"CAM STBY") != 0) 
	{
		printk(S5K5CCGX_MOD_NAME "Could not request GPIO %d\n", OMAP3430_GPIO_3MEGA_STBY);
		return -EIO;
	}    
	if (gpio_request(OMAP3430_GPIO_3MEGA_RST,"CAM RST") != 0) 
	{
		printk(S5K5CCGX_MOD_NAME "Could not request GPIO %d\n", OMAP3430_GPIO_3MEGA_RST);
		return -EIO;
	}      
	if (gpio_request(OMAP3430_GPIO_VGA_STBY,"VGA STBY") != 0) 
	{
		printk(S5K5CCGX_MOD_NAME "Could not request GPIO %d\n", OMAP3430_GPIO_VGA_STBY);
		return -EIO;
	}    
	if (gpio_request(OMAP3430_GPIO_VGA_RST,"VGA RST") != 0) 
	{
		printk(S5K5CCGX_MOD_NAME "Could not request GPIO %d\n", OMAP3430_GPIO_VGA_RST);
		return -EIO;
	}      

	/* Reset GPIO */
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN1, 0);
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN2, 0);
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN3, 0);	
	gpio_direction_output(OMAP3430_GPIO_3MEGA_STBY, 0);
	gpio_direction_output(OMAP3430_GPIO_3MEGA_RST, 0);
	gpio_direction_output(OMAP3430_GPIO_VGA_STBY, 0);
	gpio_direction_output(OMAP3430_GPIO_VGA_RST, 0);

	// maroons have no cam pmic    
	/* Enable sensor module power */ 
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN1, 1);
	udelay(700);
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN2, 1);
	udelay(500);
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN3, 1);	
	udelay(60);	// 20100610, timimg control

	/* VGA STBY High*/
	//gpio_direction_output(OMAP3430_GPIO_VGA_STBY, 1);

	/* Clock Enable */
	isp_set_xclk(0, S5K5CCGX_XCLK, 0);
	//mdelay(10);   
	mdelay(20);   

	/* CIF Reset High */      
	//gpio_direction_output(OMAP3430_GPIO_VGA_RST, 1);
	//udelay(10);
	//udelay(100);
	
	/* VGA STBY Low => CIF Hi-Z */
	//gpio_direction_output(OMAP3430_GPIO_VGA_STBY, 0);

	/* 3Mega Activate STBY */
	gpio_direction_output(OMAP3430_GPIO_3MEGA_STBY, 1);
	mdelay(1);

	/* Activate Reset */      
	gpio_direction_output(OMAP3430_GPIO_3MEGA_RST, 1);
	mdelay(10);

	return 0; 
}

static int s5k5ccgx_disable_gpio(void)
{
	dprintk(CAM_INF, S5K5CCGX_MOD_NAME "s5k5ccgx_disable_gpio is called...\n");

	gpio_direction_output(OMAP3430_GPIO_VGA_STBY, 0);
	gpio_direction_output(OMAP3430_GPIO_VGA_RST, 0);
	mdelay(1);	// 20100610, timimg control
	gpio_direction_output(OMAP3430_GPIO_3MEGA_STBY, 0);
	gpio_direction_output(OMAP3430_GPIO_3MEGA_RST, 0);

	isp_set_xclk(0, 0, 0);

	//mdelay(1);  
	mdelay(4);  	// 20100610, timimg control

	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN3, 0);
	udelay(200);	
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN2, 0);
	//udelay(10);
	udelay(20);
	gpio_direction_output(OMAP3430_GPIO_CAMERA_EN1, 0);

	gpio_free(OMAP3430_GPIO_VGA_RST);
	gpio_free(OMAP3430_GPIO_VGA_STBY);
	gpio_free(OMAP3430_GPIO_3MEGA_RST);
	gpio_free(OMAP3430_GPIO_3MEGA_STBY);
	gpio_free(OMAP3430_GPIO_CAMERA_EN1);
	gpio_free(OMAP3430_GPIO_CAMERA_EN2);
	gpio_free(OMAP3430_GPIO_CAMERA_EN3);
	
	return 0;     
}

static int s5k5ccgx_sensor_power_set(enum v4l2_power power)
{
	static enum v4l2_power c_previous_pwr = V4L2_POWER_OFF;

	printk("s5k5ccgx_sensor_power_set is called...[%x]\n", power);

	u32 reg;
	u32 reg_origin;

	/*
	control_pbias_offset = 0x0A5C;	// CONTROL_WKUP_CTRL
	reg = omap_ctrl_readl(control_pbias_offset);
	reg_origin = reg;
//	printk(KERN_ALERT "reg = 0x%x \n",reg_origin);	
	reg &=	~(1<<6);		// GPIO_IO_PWRDNZ
//	printk(KERN_ALERT  "reg = 0x%x \n",reg);	
	omap_ctrl_writel(reg, control_pbias_offset);

	control_pbias_offset = OMAP343X_CONTROL_PBIAS_LITE;	
	reg = omap_ctrl_readl(control_pbias_offset);
	reg_origin = reg;
//	printk(KERN_ALERT "reg = 0x%x \n",reg_origin);		
	reg &= ~OMAP343X_PBIASLITEPWRDNZ1;
	reg &= ~OMAP343X_PBIASLITEVMODE1;
//	printk(KERN_ALERT  "reg = 0x%x \n",reg);	
	omap_ctrl_writel(reg, control_pbias_offset);

	mdelay(10);

	control_pbias_offset = 0x0A5C;
	reg = omap_ctrl_readl(control_pbias_offset);
	reg_origin = reg;
//	printk(KERN_ALERT "reg = 0x%x \n",reg_origin);	
	reg |=	(1<<6);
//	printk(KERN_ALERT  "reg = 0x%x \n",reg);	
	omap_ctrl_writel(reg, control_pbias_offset);
	
	control_pbias_offset = OMAP343X_CONTROL_PBIAS_LITE;	
	reg = omap_ctrl_readl(control_pbias_offset);
	reg_origin = reg;
//	printk(KERN_ALERT "reg = 0x%x \n",reg_origin);		
	reg |=	OMAP343X_PBIASLITEPWRDNZ1;
//	printk(KERN_ALERT  "reg = 0x%x \n",reg);	
	omap_ctrl_writel(reg, control_pbias_offset);
	*/
	
	switch (power) 
	{
		case V4L2_POWER_OFF:
			{
				s5k5ccgx_disable_gpio();
			}
			break;

		case V4L2_POWER_ON:
			{
				isp_configure_interface(0, &s5k5ccgx_if_config);
				s5k5ccgx_enable_gpio();       
			}
			break;

		case V4L2_POWER_STANDBY:
			break;

		case V4L2_POWER_RESUME:
			break;
	}

	c_previous_pwr = power;

	return 0;
}


static int s5k5ccgx_ifparm(struct v4l2_ifparm *p)
{
	dprintk(CAM_INF, S5K5CCGX_MOD_NAME "s5k5ccgx_ifparm is called...\n");

	*p = ifparm_s5k5ccgx;

	return 0;
}


static int s5k5ccgx_sensor_set_prv_data(void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;

	dprintk(CAM_INF, S5K5CCGX_MOD_NAME "s5k5ccgx_sensor_set_prv_data is called...\n");

	//hwc->u.sensor.xclk = s5k5ccgx_hwc.xclk;
	hwc->u.sensor.sensor_isp = s5k5ccgx_hwc.sensor_isp;
	hwc->u.sensor.capture_mem = s5k5ccgx_hwc.capture_mem;
	hwc->dev_index = 0;
	hwc->dev_minor = 0;
	hwc->dev_type = OMAP34XXCAM_SLAVE_SENSOR;

	return 0;
}


struct s5k5ccgx_platform_data s5k5ccgx_platform_data0 = {
	.power_set      = s5k5ccgx_sensor_power_set,
	.priv_data_set  = s5k5ccgx_sensor_set_prv_data,
	.ifparm         = s5k5ccgx_ifparm,
};
