/*
 * drivers/media/video/sr030pc40.h
 *
 * Register definitions for the SR030PC40 camera from Samsung Electronics
 *
 * Author: Daniel Yun (TecAce Solutions)
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef SR030PC40_H
#define SR030PC40_H
#define CAM_SR030PC40_DBG_MSG            1
#define CAM_SR030PC40_I2C_DBG_MSG        1
#define CAM_SR030PC40_TUNE               0
#define SR030PC40_DRIVER_NAME            "sr030pc40"
#define SR030PC40_MOD_NAME               "SR030PC40: "

#define SR030PC40_YUV_OFFSET          0x280A00

#define SR030PC40_I2C_ADDR            0x60>>1

#define SR030PC40_I2C_RETRY           1
#define SR030PC40_XCLK                24000000      //have to be fixed

#define SENSOR_DETECTED           1
#define SENSOR_NOT_DETECTED       0

#define OMAP3630_GPIO_CAMERA_EN1	156	// VDD_REG 1.2V
#define OMAP3630_GPIO_CAMERA_EN3	186	// VDDAF 2.8V

#define OMAP3630_GPIO_CAMERA_EN2	152		// VDDIO 1.8V
#define OMAP3630_GPIO_CAMERA_EN4	177		// VDD_REG1.8V
#define OMAP3630_GPIO_CAMERA_EN5	157		// VDDA 2.8V

#define OMAP3630_GPIO_5MEGA_RST		98
#define OMAP3630_GPIO_5MEGA_STBY	153

#define OMAP3630_GPIO_VGA_RST		64 
#define OMAP3630_GPIO_VGA_STBY		101


typedef struct sr030pc40_reg {
	unsigned short subaddr;
	unsigned short value;
}sr030pc40_short_t;



/**
 * struct sr030pc40_platform_data - platform data values and access functions
 * @power_set: Power state access function, zero is off, non-zero is on.
 * @ifparm: Interface parameters access function
 * @priv_data_set: device private data (pointer) access function
 */
struct sr030pc40_platform_data {
	int (*power_set)(enum v4l2_power power);
	int (*ifparm)(struct v4l2_ifparm *p);
	int (*priv_data_set)(void *);
};

/**   
 * struct sr030pc40_sensor - main structure for storage of sensor information
 * @pdata: access functions and data for platform level information
 * @v4l2_int_device: V4L2 device structure structure
 * @i2c_client: iic client device structure
 * @pix: V4L2 pixel format information structure
 * @timeperframe: time per frame expressed as V4L fraction
 * @scaler:
 * @ver: sr030pc40 chip version
 * @fps: frames per second value   
 */
struct sr030pc40_sensor {
	const struct sr030pc40_platform_data *pdata;
	struct v4l2_int_device *v4l2_int_device;
	struct i2c_client *i2c_client;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	int check_dataline;  
	u32 state;
	u8 mode;
	u8 fps;
	u8 preview_size;
	u8 capture_size;
	u8 detect;
	u8 effect;
	u8 iso;
	u8 photometry;
	u8 ev;
	u8 contrast;
	u8 wb;
	u8 scene;
	u8 zoom;
	u8 pretty;
	u8 flip;
	u32 yuv_offset;
};

// register address define
#define PAGEMODE		0x03

// page 0
#define PWRCTL			0x01
#define DEVID 			0x04


/* delay define */
#define WAIT_CAM_AEAWB        100

/* State */
#define SR030PC40_STATE_PREVIEW	  0x0000	/*  preview state */
#define SR030PC40_STATE_CAPTURE	  0x0001	/*  capture state */
#define SR030PC40_STATE_INVALID	  0x0002	/*  invalid state */

/* Mode */
#define SR030PC40_MODE_CAMERA		1
#define SR030PC40_MODE_CAMCORDER	2
#define SR030PC40_MODE_VT			3

/* Preview Size */
#define SR030PC40_PREVIEW_SIZE_176_144		0
#define SR030PC40_PREVIEW_SIZE_320_240		1
#define SR030PC40_PREVIEW_SIZE_640_480		2

/* Image Size */
#define SR030PC40_IMAGE_SIZE_176_144    0
#define SR030PC40_IMAGE_SIZE_320_240	1
#define SR030PC40_IMAGE_SIZE_640_480    2

/* Image Effect */
#define SR030PC40_EFFECT_OFF      1
#define SR030PC40_EFFECT_SHARPEN  2
#define SR030PC40_EFFECT_PURPLE   3
#define SR030PC40_EFFECT_NEGATIVE 4
#define SR030PC40_EFFECT_SEPIA    5
#define SR030PC40_EFFECT_AQUA     6
#define SR030PC40_EFFECT_GREEN    7
#define SR030PC40_EFFECT_BLUE     8
#define SR030PC40_EFFECT_PINK     9
#define SR030PC40_EFFECT_YELLOW   10
#define SR030PC40_EFFECT_GREY     11
#define SR030PC40_EFFECT_RED      12
#define SR030PC40_EFFECT_BW       13
#define SR030PC40_EFFECT_ANTIQUE  14

/* Flip */
#define SR030PC40_FLIP_NONE             1
#define SR030PC40_FLIP_MIRROR           2
#define SR030PC40_FLIP_WATER            3
#define SR030PC40_FLIP_WATER_MIRROR     4

/* Blur */
#define SR030PC40_BLUR_LEVEL_0	1
#define SR030PC40_BLUR_LEVEL_1	2
#define SR030PC40_BLUR_LEVEL_2	3
#define SR030PC40_BLUR_LEVEL_3	4

/* ISO */
#define SR030PC40_ISO_AUTO        1
#define SR030PC40_ISO_50          2
#define SR030PC40_ISO_100         3
#define SR030PC40_ISO_200         4
#define SR030PC40_ISO_400         5
#define SR030PC40_ISO_800         6
#define SR030PC40_ISO_1600        7

/* Photometry */
#define SR030PC40_PHOTOMETRY_MATRIX   1
#define SR030PC40_PHOTOMETRY_CENTER   2
#define SR030PC40_PHOTOMETRY_SPOT     3

/* EV */
#define SR030PC40_EV_MINUS_2P0    1
#define SR030PC40_EV_MINUS_1P5    2
#define SR030PC40_EV_MINUS_1P0    3
#define SR030PC40_EV_MINUS_0P5    4
#define SR030PC40_EV_DEFAULT      5
#define SR030PC40_EV_PLUS_0P5     6
#define SR030PC40_EV_PLUS_1P0     7
#define SR030PC40_EV_PLUS_1P5     8
#define SR030PC40_EV_PLUS_2P0     9

/* WDR */
#define SR030PC40_WDR_OFF         1
#define SR030PC40_WDR_ON          2
#define SR030PC40_WDR_AUTO        3

/* Contrast */
#define SR030PC40_CONTRAST_MINUS_3      2
#define SR030PC40_CONTRAST_MINUS_2      3
#define SR030PC40_CONTRAST_MINUS_1      4
#define SR030PC40_CONTRAST_DEFAULT      5
#define SR030PC40_CONTRAST_PLUS_1       6
#define SR030PC40_CONTRAST_PLUS_2       7
#define SR030PC40_CONTRAST_PLUS_3       8

/* Saturation */
#define SR030PC40_SATURATION_MINUS_3    2
#define SR030PC40_SATURATION_MINUS_2    3
#define SR030PC40_SATURATION_MINUS_1    4
#define SR030PC40_SATURATION_DEFAULT    5
#define SR030PC40_SATURATION_PLUS_1     6
#define SR030PC40_SATURATION_PLUS_2     7
#define SR030PC40_SATURATION_PLUS_3     8

/* Sharpness */
#define SR030PC40_SHARPNESS_MINUS_3     2
#define SR030PC40_SHARPNESS_MINUS_2     3
#define SR030PC40_SHARPNESS_MINUS_1     4
#define SR030PC40_SHARPNESS_DEFAULT     5
#define SR030PC40_SHARPNESS_PLUS_1      6
#define SR030PC40_SHARPNESS_PLUS_2      7
#define SR030PC40_SHARPNESS_PLUS_3      8

/* White Balance */
#define SR030PC40_WB_AUTO               1
#define SR030PC40_WB_DAYLIGHT           2
#define SR030PC40_WB_CLOUDY             3
#define SR030PC40_WB_INCANDESCENT       4
#define SR030PC40_WB_FLUORESCENT        5
#define SR030PC40_WB_TUNGSTEN			6

/* Image Stabilization */
#define SR030PC40_ISC_STILL_OFF         1
#define SR030PC40_ISC_STILL_ON          2
#define SR030PC40_ISC_STILL_AUTO        3
#define SR030PC40_ISC_MOVIE_ON          4

/* Scene Mode */
#define SR030PC40_SCENE_OFF             1
#define SR030PC40_SCENE_ASD             2
#define SR030PC40_SCENE_SUNSET          3
#define SR030PC40_SCENE_DAWN            4
#define SR030PC40_SCENE_CANDLELIGHT     5
#define SR030PC40_SCENE_BEACH_SNOW      6
#define SR030PC40_SCENE_AGAINST_LIGHT   7
#define SR030PC40_SCENE_TEXT            8
#define SR030PC40_SCENE_NIGHTSHOT       9
#define SR030PC40_SCENE_LANDSCAPE       10
#define SR030PC40_SCENE_FIREWORKS       11
#define SR030PC40_SCENE_PORTRAIT        12
#define SR030PC40_SCENE_FALLCOLOR       13
#define SR030PC40_SCENE_INDOORS         14
#define SR030PC40_SCENE_SPORTS          15

/* Auto Exposure & Auto White Balance */
#define SR030PC40_AE_LOCK_AWB_LOCK      1
#define SR030PC40_AE_LOCK_AWB_UNLOCK    2
#define SR030PC40_AE_UNLOCK_AWB_LOCK    3
#define SR030PC40_AE_UNLOCK_AWB_UNLOCK  4

/* Anti-Shake */
#define SR030PC40_ANTI_SHAKE_OFF        1
#define SR030PC40_ANTI_SHAKE_ON         2

/* Flash Setting */
#define SR030PC40_FLASH_CAPTURE_OFF     1
#define SR030PC40_FLASH_CAPTURE_ON      2
#define SR030PC40_FLASH_CAPTURE_AUTO    3

#define SR030PC40_FLASH_MOVIE_OFF       1
#define SR030PC40_FLASH_MOVIE_ON        2

/* Focus Mode */
#define SR030PC40_AF_INIT_NORMAL        1
#define SR030PC40_AF_INIT_MACRO         2
#define SR030PC40_AF_INIT_FACE          3

/* Focust start/stop */
#define SR030PC40_AF_START              1
#define SR030PC40_AF_STOP               2

/* Auto Focus Status */
#define SR030PC40_AF_STATUS_PROGRESS    1
#define SR030PC40_AF_STATUS_SUCCESS     2
#define SR030PC40_AF_STATUS_FAIL        3

/* Digital Zoom */
#define SR030PC40_ZOOM_1P00X            1
#define SR030PC40_ZOOM_1P25X            2
#define SR030PC40_ZOOM_1P50X            3
#define SR030PC40_ZOOM_1P75X            4
#define SR030PC40_ZOOM_2P00X            5
#define SR030PC40_ZOOM_2P25X            6
#define SR030PC40_ZOOM_2P50X            7
#define SR030PC40_ZOOM_2P75X            8
#define SR030PC40_ZOOM_3P00X            9
#define SR030PC40_ZOOM_3P25X            10
#define SR030PC40_ZOOM_3P50X            11
#define SR030PC40_ZOOM_3P75X            12
#define SR030PC40_ZOOM_4P00X            13

#define SR030PC40_INVALID_VALUE 	(0xff)	

struct sr030pc40_preview_size {
	unsigned long width;
	unsigned long height;
};

const static struct sr030pc40_preview_size sr030pc40_preview_sizes[] = {
	{176,144},
	{320,240},
	{640,480},
};

struct sr030pc40_capture_size {
	unsigned long width;
	unsigned long height;
};

/* Image sizes */
const static struct sr030pc40_capture_size sr030pc40_image_sizes[] = {
	{176,144},
	{320,240},
	{640,480},
};

#endif /* ifndef SR030PC40_H */
