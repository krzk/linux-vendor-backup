/*
 * drivers/media/video/s5k5ccgx.h
 *
 * Register definitions for the S5K5CCGX camera from Samsung Electronics
 *
 * Author: Gwanghui Lee 
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/*
#if CONFIG_MACH_ARIES
#include <mach/aries.h>
#elif CONFIG_MACH_MAROONS
#include <mach/maroons.h>
#elif CONFIG_MACH_JEWELFISH
#include <mach/jewelfish.h>
#elif CONFIG_MACH_OSCAR
#include <mach/oscar.h>
#endif
*/

#ifndef S5K5CCGX_H
#define S5K5CCGX_H
#define CAM_S5K5CCGX_DBG_MSG            1
//#define USE_SD_CARD_TUNE
#define S5K5CCGX_DRIVER_NAME            "s5k5ccgx"
#define S5K5CCGX_MOD_NAME               "S5K5CCGX: "

#define S5K5CCGX_THUMBNAIL_OFFSET    	0x271000		// 0x271000		0x1EA000
#define S5K5CCGX_YUV_OFFSET        		0x280A00		// 0x280A00		0x224980

#define S5K5CCGX_I2C_ADDR            			(0x5A >> 1)
#define S5K5CCGX_I2C_RETRY           			1
#define S5K5CCGX_XCLK                			24000000      

#define SENSOR_DETECTED           			1
#define SENSOR_NOT_DETECTED       		0

#define OMAP3430_GPIO_CAMERA_EN1          152
#define OMAP3430_GPIO_CAMERA_EN2          186
#define OMAP3430_GPIO_CAMERA_EN3          177

#define OMAP3430_GPIO_3MEGA_RST          	98
#define OMAP3430_GPIO_3MEGA_STBY         	153

#define OMAP3430_GPIO_VGA_RST             	64 
#define OMAP3430_GPIO_VGA_STBY            	101


/**
 * struct s5k5ccgx_platform_data - platform data values and access functions
 * @power_set: Power state access function, zero is off, non-zero is on.
 * @ifparm: Interface parameters access function
 * @priv_data_set: device private data (pointer) access function
 */
struct s5k5ccgx_platform_data {
	int (*power_set)(enum v4l2_power power);
	int (*ifparm)(struct v4l2_ifparm *p);
	int (*priv_data_set)(void *);
};

/**   
 * struct s5k5ccgx_sensor - main structure for storage of sensor information
 * @pdata: access functions and data for platform level information
 * @v4l2_int_device: V4L2 device structure structure
 * @i2c_client: iic client device structure
 * @pix: V4L2 pixel format information structure
 * @timeperframe: time per frame expressed as V4L fraction
 * @scaler:
 * @ver: s5k5ccgx chip version
 * @fps: frames per second value   
 */
struct s5k5ccgx_sensor {
	const struct s5k5ccgx_platform_data *pdata;
	struct v4l2_int_device *v4l2_int_device;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	int check_dataline;  
	u32 state;
	u8 mode;
	u8 fps;
	//u16 bv;
	u8 preview_size;
	u8 capture_size;
	u8 focus_mode;
	u8 detect;
	u8 effect;
	u8 iso;
	u8 photometry;
	u8 ev;
	u8 wdr;
	u8 contrast;
	u8 saturation;
	u8 sharpness;
	u8 wb;
	//u8 isc;
	u8 scene;
	u8 aewb;
	//u8 antishake;
	//u8 flash_capture;
	//u8 flash_movie;
	u8 jpeg_quality;
	//s32 zoom;
	u32 thumb_offset;
	u32 yuv_offset;
	u32 jpeg_capture_w;
	u32 jpeg_capture_h;

};

/* State */
#define S5K5CCGX_STATE_PREVIEW	  0x0000	/*  preview state */
#define S5K5CCGX_STATE_CAPTURE	  0x0001	/*  capture state */
#define S5K5CCGX_STATE_INVALID	  0x0002	/*  invalid state */

/* Mode */
#define S5K5CCGX_MODE_CAMERA     		1
#define S5K5CCGX_MODE_CAMCORDER  		2
#define S5K5CCGX_MODE_VT         3

/* Preview Size */
#define S5K5CCGX_PREVIEW_SIZE_160_120    	0
#define S5K5CCGX_PREVIEW_SIZE_144_176    	1
#define S5K5CCGX_PREVIEW_SIZE_176_144    	2
#define S5K5CCGX_PREVIEW_SIZE_200_120    	3
#define S5K5CCGX_PREVIEW_SIZE_320_240    	4	
#define S5K5CCGX_PREVIEW_SIZE_352_288    	5
#define S5K5CCGX_PREVIEW_SIZE_400_240    	6
#define S5K5CCGX_PREVIEW_SIZE_640_480		6
#define S5K5CCGX_PREVIEW_SIZE_720_480		7
#define S5K5CCGX_PREVIEW_SIZE_800_480		8
#define S5K5CCGX_PREVIEW_SIZE_1024_768		9
#define S5K5CCGX_PREVIEW_SIZE_1280_720		10

struct s5k5ccgx_preview_size {
	unsigned long width;
	unsigned long height;
};

const static struct s5k5ccgx_preview_size s5k5ccgx_preview_sizes[] = {
	{160,120},  
	{144,176},	
	{176,144},	// QCIF
	{200,120}, 	// WQQVGA      
	{320,240},	// QVGA
	{352,288},	// CIF
	{400,240},	// WQVGA
	{640,480},	// VGA
	{720,480},	// D1
	{800,480},	// WVGA
};

/* Image Size */
#define S5K5CCGX_CAPTURE_SIZE_160_120    		0
#define S5K5CCGX_CAPTURE_SIZE_176_144    		1
#define S5K5CCGX_CAPTURE_SIZE_200_120    		2
#define S5K5CCGX_CAPTURE_SIZE_320_240    		3
#define S5K5CCGX_CAPTURE_SIZE_352_288    		4
#define S5K5CCGX_CAPTURE_SIZE_400_240    		5
#define S5K5CCGX_CAPTURE_SIZE_640_480    		6
#define S5K5CCGX_CAPTURE_SIZE_720_480    		7
#define S5K5CCGX_CAPTURE_SIZE_800_480    		8
#define S5K5CCGX_CAPTURE_SIZE_800_600    		9
#define S5K5CCGX_CAPTURE_SIZE_1024_768   		10
#define S5K5CCGX_CAPTURE_SIZE_1280_720   		11
#define S5K5CCGX_CAPTURE_SIZE_1280_768   		12
#define S5K5CCGX_CAPTURE_SIZE_1280_960   		13
#define S5K5CCGX_CAPTURE_SIZE_1600_960   		14
#define S5K5CCGX_CAPTURE_SIZE_1600_1200  		15
#define S5K5CCGX_CAPTURE_SIZE_1920_1080  		16
#define S5K5CCGX_CAPTURE_SIZE_2048_1232  		17
#define S5K5CCGX_CAPTURE_SIZE_2048_1536  		18

struct s5k5ccgx_capture_size {
	unsigned long width;
	unsigned long height;
};

/* Image sizes */
const static struct s5k5ccgx_capture_size s5k5ccgx_image_sizes[] = {
	{160,120},
	{176,144},
	{200,120},       
	{320,240},
	{352,288},
	{400,240},
	{640,480},
	{720,480}, 
	{800,480}, 
	{800,600},
	{1024,768},
	{1280,720},
	{1280,768},     
	{1280,960},
	{1600,960},
	{1600,1200},
	{1920,1080},
	{2048,1232},
	{2048,1536},  
};



/* Image Effect */
#define S5K5CCGX_EFFECT_OFF				1
#define S5K5CCGX_EFFECT_MONO			11	// 2
#define S5K5CCGX_EFFECT_SEPIA			5	// 3
#define S5K5CCGX_EFFECT_NEGATIVE		4
#define S5K5CCGX_EFFECT_AQUA			8	// 5
#define S5K5CCGX_EFFECT_SKETCH			6
#define S5K5CCGX_EFFECT_DEFAULT			(S5K5CCGX_EFFECT_OFF)

/* White Balance */
#define S5K5CCGX_WB_AUTO               	1
#define S5K5CCGX_WB_DAYLIGHT           	2
#define S5K5CCGX_WB_CLOUDY             	3
#define S5K5CCGX_WB_FLUORESCENT			5	// 4
#define S5K5CCGX_WB_INCANDESCENT		4	// 5
#define S5K5CCGX_WB_DEFAULT				(S5K5CCGX_WB_AUTO)

/* EV */
#define S5K5CCGX_EV_MINUS_4    		1
#define S5K5CCGX_EV_MINUS_3    		2
#define S5K5CCGX_EV_MINUS_2    		3
#define S5K5CCGX_EV_MINUS_1    		4
#define S5K5CCGX_EV_DEFAULT      	5
#define S5K5CCGX_EV_PLUS_1     		6
#define S5K5CCGX_EV_PLUS_2     		7
#define S5K5CCGX_EV_PLUS_3     		8
#define S5K5CCGX_EV_PLUS_4     		9

/* Scene Mode */
#define S5K5CCGX_SCENE_OFF				1
#define S5K5CCGX_SCENE_PORTRAIT			12	// 2 
#define S5K5CCGX_SCENE_LANDSCAPE		10	// 3
#define S5K5CCGX_SCENE_SPORTS			15	// 4
#define S5K5CCGX_SCENE_PARTY			14	// 5
#define S5K5CCGX_SCENE_BEACH			6
#define S5K5CCGX_SCENE_SUNSET			3	// 7
#define S5K5CCGX_SCENE_DAWN				4	// 8
#define S5K5CCGX_SCENE_FALL				13	// 9
#define S5K5CCGX_SCENE_NIGHT			9	// 10
#define S5K5CCGX_SCENE_BACKLIGHT		7	// 11
#define S5K5CCGX_SCENE_FIRE				11	// 12
#define S5K5CCGX_SCENE_TEXT				8	// 13
#define S5K5CCGX_SCENE_CANDLE			5	// 14
#define S5K5CCGX_SCENE_SPORTS_OFF		16	// 15
#define S5K5CCGX_SCENE_NIGHT_OFF		17	// 16
#define S5K5CCGX_SCENE_DEFAULT			(S5K5CCGX_SCENE_OFF)		

/* Contrast */
#define S5K5CCGX_CONTRAST_MINUS_2      1
#define S5K5CCGX_CONTRAST_MINUS_1      2
#define S5K5CCGX_CONTRAST_DEFAULT      3
#define S5K5CCGX_CONTRAST_PLUS_1       4
#define S5K5CCGX_CONTRAST_PLUS_2       5

/* Saturation */
#define S5K5CCGX_SATURATION_MINUS_2    1
#define S5K5CCGX_SATURATION_MINUS_1    2
#define S5K5CCGX_SATURATION_DEFAULT    3
#define S5K5CCGX_SATURATION_PLUS_1     4
#define S5K5CCGX_SATURATION_PLUS_2     5

/* Sharpness */
#define S5K5CCGX_SHARPNESS_MINUS_2     1
#define S5K5CCGX_SHARPNESS_MINUS_1     2
#define S5K5CCGX_SHARPNESS_DEFAULT     3
#define S5K5CCGX_SHARPNESS_PLUS_1      4
#define S5K5CCGX_SHARPNESS_PLUS_2      5

/* Photometry */
#define S5K5CCGX_PHOTOMETRY_NORMAL   	1
#define S5K5CCGX_PHOTOMETRY_SPOT     	3	// 2
#define S5K5CCGX_PHOTOMETRY_CENTER   	2	// 3	
#define S5K5CCGX_PHOTOMETRY_DEFAULT		(S5K5CCGX_PHOTOMETRY_CENTER)

/* ISO */
#define S5K5CCGX_ISO_AUTO        		1
#define S5K5CCGX_ISO_50          		2
#define S5K5CCGX_ISO_100         		3
#define S5K5CCGX_ISO_200         		4
#define S5K5CCGX_ISO_400         		5
#define S5K5CCGX_ISO_DEFAULT			(S5K5CCGX_ISO_AUTO)

/* fps */
#define S5K5CCGX_15_FPS	        		1
#define S5K5CCGX_FPS_DEFAULT		30

/* Auto Exposure & Auto White Balance */
#define S5K5CCGX_AWB_AE_LOCK      		1
#define S5K5CCGX_AWB_AE_UNLOCK  		2
#define S5K5CCGX_AWB_AE_DEFAULT			(S5K5CCGX_AWB_AE_UNLOCK)

/* Focus Mode */
#define S5K5CCGX_AF_NORMAL        	1
#define S5K5CCGX_AF_MACRO         	2
#define S5K5CCGX_AF_OFF         	3

/* Focust start/stop */
#define S5K5CCGX_AF_START			1
#define S5K5CCGX_AF_STOP			2

/* Auto Focus Status */
#define S5K5CCGX_AF_STATUS_PROGRESS    	1
#define S5K5CCGX_AF_STATUS_SUCCESS     	2
#define S5K5CCGX_AF_STATUS_FAIL        		3

/* JPEG Quality */
#define S5K5CCGX_JPEG_SUPERFINE			1
#define S5K5CCGX_JPEG_FINE				2
#define S5K5CCGX_JPEG_NORMAL			3
#define S5K5CCGX_JPEG_DEFAULT			(S5K5CCGX_JPEG_SUPERFINE)

#endif /* ifndef S5K5CCGX_H */

