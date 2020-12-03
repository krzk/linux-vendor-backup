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
 * modules/camera/sr030pc40_platform.c
 *
 * SR030PC40 sensor driver file related to platform
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
#include "sr030pc40.h"

#if (CAM_SR030PC40_DBG_MSG)
#include "dprintk.h"
#else
#define dprintk(x, y...)
#endif

static struct v4l2_ifparm ifparm_sr030pc40 = {
	.if_type = V4L2_IF_TYPE_BT656, // fix me
	.u = {
		.bt656 = {
			.frame_start_on_rising_vs = 0,
			.latch_clk_inv = 0,
			.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT, 
			.clock_min = SR030PC40_XCLK,
			.clock_max = SR030PC40_XCLK,
			.clock_curr = SR030PC40_XCLK,
		},
	},
};

#define SR030PC40_BIGGEST_FRAME_BYTE_SIZE  PAGE_ALIGN(640 * 480 * 2 * 6) //fix for usage of 6 buffers for 720p capture and avoiding camera launch issues.


static struct omap34xxcam_sensor_config sr030pc40_hwc = {
	.sensor_isp = 1,
	//.xclk = OMAP34XXCAM_XCLK_A,
	.capture_mem =  SR030PC40_BIGGEST_FRAME_BYTE_SIZE,
};

struct isp_interface_config sr030pc40_if_config = {
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

static int sr030pc40_enable_gpio(void)
{
	dprintk(CAM_INF, SR030PC40_MOD_NAME "sr030pc40_enable_gpio is called...\n");

	/* Request and configure gpio pins */
	if (gpio_request(OMAP3630_GPIO_CAMERA_EN1,"CAM VDDA 2.8V") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d\n", OMAP3630_GPIO_CAMERA_EN1);
		return -EIO;
	} 
	if (gpio_request(OMAP3630_GPIO_CAMERA_EN3,"CAM VDDAF 2.8V") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d\n", OMAP3630_GPIO_CAMERA_EN3);
		return -EIO;
	}   	
	if (gpio_request(OMAP3630_GPIO_CAMERA_EN2,"VDDIO 1.8V") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d", OMAP3630_GPIO_CAMERA_EN2);
		return -EIO;
	} 
	if (gpio_request(OMAP3630_GPIO_CAMERA_EN4,"VDD_REG 1.8V") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d", OMAP3630_GPIO_CAMERA_EN4);
		return -EIO;       
	}
	if (gpio_request(OMAP3630_GPIO_CAMERA_EN5,"VDDA 2.8V") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d", OMAP3630_GPIO_CAMERA_EN5);
		return -EIO;
	} 
	if (gpio_request(OMAP3630_GPIO_5MEGA_STBY,"5M STBY") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d\n", OMAP3630_GPIO_5MEGA_STBY);
		return -EIO;
	}    
	if (gpio_request(OMAP3630_GPIO_5MEGA_RST,"5M RST") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d\n", OMAP3630_GPIO_5MEGA_RST);
		return -EIO;
	}      
	if (gpio_request(OMAP3630_GPIO_VGA_STBY,"VGA STDBY") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d", OMAP3630_GPIO_VGA_STBY);
		return -EIO;       
	}
	if (gpio_request(OMAP3630_GPIO_VGA_RST,"CAM VGA RST") != 0) {
		printk(SR030PC40_MOD_NAME "Could not request GPIO %d", OMAP3630_GPIO_VGA_RST);
		return -EIO;       
	}  

	/* Reset the GPIO pins */
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN1, 0);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN3, 0);

	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN2, 0);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN4, 0);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN5, 0);
	gpio_direction_output(OMAP3630_GPIO_5MEGA_STBY, 0);
	gpio_direction_output(OMAP3630_GPIO_5MEGA_RST, 0);
	gpio_direction_output(OMAP3630_GPIO_VGA_STBY, 0);        
	gpio_direction_output(OMAP3630_GPIO_VGA_RST, 0);    

	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN1, 1); udelay(100);	// 5M(core)
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN5, 1); udelay(100);	// VDDA(Analog VDD)
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN4, 1); 			// VGA(core)
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN2, 1); udelay(100);	// VDDIO(Sensor I/O)

	gpio_direction_output(OMAP3630_GPIO_VGA_STBY, 1); udelay(100); 
	gpio_direction_output(OMAP3630_GPIO_VGA_STBY, 0); udelay(30); 

	/* Clock Enable */
	isp_set_xclk(0, SR030PC40_XCLK, 0); udelay(20);   

	gpio_direction_output(OMAP3630_GPIO_5MEGA_STBY, 1); mdelay(5);
	gpio_direction_output(OMAP3630_GPIO_5MEGA_RST, 1); mdelay(5);
	gpio_direction_output(OMAP3630_GPIO_5MEGA_STBY, 0); udelay(30);

	/* VGA Reset Low -> Hi -> Low */      
	gpio_direction_output(OMAP3630_GPIO_VGA_RST, 1); udelay(100);
	return 0; 
}

static int sr030pc40_disable_gpio(void)
{
	dprintk(CAM_INF, SR030PC40_MOD_NAME "sr030pc40_disable_gpio is called...\n");
	
	gpio_direction_output(OMAP3630_GPIO_5MEGA_STBY, 0);
	gpio_direction_output(OMAP3630_GPIO_VGA_STBY, 0);
	gpio_direction_output(OMAP3630_GPIO_VGA_RST, 0); udelay(10);
	gpio_direction_output(OMAP3630_GPIO_5MEGA_RST, 0); udelay(60);
	isp_set_xclk(0, 0, 0); udelay(10); 

	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN2, 0); udelay(20);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN4, 0); udelay(20);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN5, 0); udelay(20);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN1, 0); udelay(20);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN3, 0); udelay(20);	

	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN3, 0); udelay(10);	
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN2, 0); udelay(10);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN5, 0); udelay(10);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN1, 0); udelay(10);
	gpio_direction_output(OMAP3630_GPIO_CAMERA_EN4, 0); udelay(10);

	gpio_free(OMAP3630_GPIO_5MEGA_RST);
	gpio_free(OMAP3630_GPIO_5MEGA_STBY);
	gpio_free(OMAP3630_GPIO_VGA_RST);
	gpio_free(OMAP3630_GPIO_VGA_STBY);
	gpio_free(OMAP3630_GPIO_CAMERA_EN5);
	gpio_free(OMAP3630_GPIO_CAMERA_EN4);
	gpio_free(OMAP3630_GPIO_CAMERA_EN2);
	gpio_free(OMAP3630_GPIO_CAMERA_EN3);     
	gpio_free(OMAP3630_GPIO_CAMERA_EN1);     
	return 0;     
}

static int sr030pc40_sensor_power_set(enum v4l2_power power)
{
	static enum v4l2_power c_previous_pwr = V4L2_POWER_OFF;

	printk("sr030pc40_sensor_power_set is called...[%x]\n", power);

	switch (power) 
	{
		case V4L2_POWER_OFF:
			{
				sr030pc40_disable_gpio();
			}
			break;

		case V4L2_POWER_ON:
			{
				isp_configure_interface(0, &sr030pc40_if_config);
				sr030pc40_enable_gpio();       
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


static int sr030pc40_ifparm(struct v4l2_ifparm *p)
{
	dprintk(CAM_INF, SR030PC40_MOD_NAME "sr030pc40_ifparm is called...\n");

	*p = ifparm_sr030pc40;

	return 0;
}


static int sr030pc40_sensor_set_prv_data(void *priv)
{
	struct omap34xxcam_hw_config *hwc = priv;

	dprintk(CAM_INF, SR030PC40_MOD_NAME "sr030pc40_sensor_set_prv_data is called...\n");

	//hwc->u.sensor.xclk = sr030pc40_hwc.xclk;
	hwc->u.sensor.sensor_isp = sr030pc40_hwc.sensor_isp;
	hwc->dev_index = 1;
	hwc->dev_minor = 5;
	hwc->dev_type = OMAP34XXCAM_SLAVE_SENSOR;

	return 0;
}


struct sr030pc40_platform_data sr030pc40_platform_data0 = {
	.power_set      = sr030pc40_sensor_power_set,
	.priv_data_set  = sr030pc40_sensor_set_prv_data,
	.ifparm         = sr030pc40_ifparm,
};
