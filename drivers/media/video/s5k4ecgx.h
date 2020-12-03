/*
 * drivers/media/video/s5k4ecgx.h
 *
 * Register definitions for the S5K4ECGX camera from Samsung Electronics
 *
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */
#ifndef S5K4ECGX_H
#define S5K4ECGX_H

#define CAM_S5K4ECGX_DBG_MSG            1
//#define USE_SD_CARD_TUNE
#define S5K4ECGX_DRIVER_NAME            "s5k4ecgx"
#define S5K4ECGX_MOD_NAME               "S5K4ECGX: "

#define S5K4ECGX_THUMBNAIL_OFFSET    	0x271000		// 0x271000		0x1EA000
#define S5K4ECGX_YUV_OFFSET        		0x280A00		// 0x280A00		0x224980

#define S5K4ECGX_I2C_ADDR		(0x5A >> 1)
#define S5K4ECGX_I2C_RETRY		1
#define S5K4ECGX_XCLK			24000000      

#define SENSOR_DETECTED 		1
#define SENSOR_NOT_DETECTED       		0

// EN1 - EN5 - EN2 - EN3
#define OMAP3630_GPIO_CAMERA_EN1	156	// VDD_REG 1.2V
#define OMAP3630_GPIO_CAMERA_EN3	186	// VDDAF 2.8V

#define OMAP3630_GPIO_CAMERA_EN2	152	// VDDIO 1.8V
#define OMAP3630_GPIO_CAMERA_EN4	177	// VDD_REG1.8V
#define OMAP3630_GPIO_CAMERA_EN5	157	// VDDA 2.8V

#define OMAP3630_GPIO_5MEGA_RST		98
#define OMAP3630_GPIO_5MEGA_STBY	153

#define OMAP3630_GPIO_VGA_RST		64 
#define OMAP3630_GPIO_VGA_STBY		101

struct s5k4ecgx_position {
	int x;
	int y;
}; 

/**
 * struct s5k4ecgx_platform_data - platform data values and access functions
 * @power_set: Power state access function, zero is off, non-zero is on.
 * @ifparm: Interface parameters access function
 * @priv_data_set: device private data (pointer) access function
 */
struct s5k4ecgx_platform_data {
	int (*power_set)(enum v4l2_power power);
	int (*ifparm)(struct v4l2_ifparm *p);
	int (*priv_data_set)(void *);
};

/**   
 * struct s5k4ecgx_sensor - main structure for storage of sensor information
 * @pdata: access functions and data for platform level information
 * @v4l2_int_device: V4L2 device structure structure
 * @i2c_client: iic client device structure
 * @pix: V4L2 pixel format information structure
 * @timeperframe: time per frame expressed as V4L fraction
 * @scaler:
 * @ver: s5k4ecgx chip version
 * @fps: frames per second value   
 */
struct s5k4ecgx_sensor {
	const struct s5k4ecgx_platform_data *pdata;
	struct v4l2_int_device *v4l2_int_device;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct s5k4ecgx_position position;
	u32 fw_version;
	u32 check_dataline;  
	u32 state;
	u8 ae_lock;
	u8 awb_lock;
	u8 mode;
	u8 fps;
	//u16 bv;
	u8 camcorder_size;
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
	s32 zoom;
	u32 thumb_offset;
	u32 yuv_offset;
	u32 jpeg_main_size;
	u32 jpeg_main_offset;
	u32 jpeg_thumb_size;
	u32 jpeg_thumb_offset;
	u32 jpeg_postview_offset;
	u32 jpeg_capture_w;
	u32 jpeg_capture_h;
};

/* State */
#define S5K4ECGX_STATE_PREVIEW	  0x0000	/*  preview state */
#define S5K4ECGX_STATE_CAPTURE	  0x0001	/*  capture state */
#define S5K4ECGX_STATE_INVALID	  0x0002	/*  invalid state */

/* Mode */
#define S5K4ECGX_MODE_CAMERA     		1
#define S5K4ECGX_MODE_CAMCORDER  		2
#define S5K4ECGX_MODE_VT         3

/* Preview Size */
#define S5K4ECGX_PREVIEW_SIZE_144_176    	0
#define S5K4ECGX_PREVIEW_SIZE_176_144    	1
#define S5K4ECGX_PREVIEW_SIZE_320_240    	2	
#define S5K4ECGX_PREVIEW_SIZE_352_288		3
#define S5K4ECGX_PREVIEW_SIZE_640_480		4
#define S5K4ECGX_PREVIEW_SIZE_720_480		5
#define S5K4ECGX_PREVIEW_SIZE_800_480		6
#define S5K4ECGX_PREVIEW_SIZE_1280_720		7

struct s5k4ecgx_frame_size {
	unsigned long width;
	unsigned long height;
};

const static struct s5k4ecgx_frame_size s5k4ecgx_preview_sizes[] = {
	{144,176}, 	//  
	{176,144}, 	// QCIF 
	{320,240},	// QVGA
	{352,288},	// XVGA
	{640,480},	// VGA
	{720,480},	// D1
	{800,480},	// WVGA
};

/* Preview Size */
#define S5K4ECGX_CAMCORDER_SIZE_144_176		0
#define S5K4ECGX_CAMCORDER_SIZE_176_144		1
#define S5K4ECGX_CAMCORDER_SIZE_320_240		2	
#define S5K4ECGX_CAMCORDER_SIZE_352_288		3	
#define S5K4ECGX_CAMCORDER_SIZE_640_480		4
#define S5K4ECGX_CAMCORDER_SIZE_720_480		5
#define S5K4ECGX_CAMCORDER_SIZE_800_480		6
#define S5K4ECGX_CAMCORDER_SIZE_1280_720	7

const static struct s5k4ecgx_frame_size s5k4ecgx_camcorder_sizes[] = {
	{144,176}, 	// 
	{176,144}, 	// QCIF 
	{320,240},	// QVGA
	{352,288},	
	{640,480},	// VGA
	{720,480},	// D1
	{800,480},	// D1
	{1280,720},	// D1
};


/* Image Size */
#define S5K4ECGX_CAPTURE_SIZE_160_120    		0
#define S5K4ECGX_CAPTURE_SIZE_176_144    		1
#define S5K4ECGX_CAPTURE_SIZE_320_240    		2
#define S5K4ECGX_CAPTURE_SIZE_352_288    		3
#define S5K4ECGX_CAPTURE_SIZE_480_320    		4
#define S5K4ECGX_CAPTURE_SIZE_640_480    		5
#define S5K4ECGX_CAPTURE_SIZE_720_480    		6
#define S5K4ECGX_CAPTURE_SIZE_800_480			7
#define S5K4ECGX_CAPTURE_SIZE_1024_768   		8
#define S5K4ECGX_CAPTURE_SIZE_1280_848   		9
#define S5K4ECGX_CAPTURE_SIZE_1280_960   		10
#define S5K4ECGX_CAPTURE_SIZE_1600_1072  		11
#define S5K4ECGX_CAPTURE_SIZE_1600_960			12
#define S5K4ECGX_CAPTURE_SIZE_1600_1200  		13
#define S5K4ECGX_CAPTURE_SIZE_2048_1232			14
#define S5K4ECGX_CAPTURE_SIZE_2048_1536			15
#define S5K4ECGX_CAPTURE_SIZE_2560_1536  		16
#define S5K4ECGX_CAPTURE_SIZE_2560_1920	  		17

/* Image sizes */
const static struct s5k4ecgx_frame_size s5k4ecgx_image_sizes[] = {
	{160,120},	
	{176,144},
	{320,240},
	{352,288},
	{480,320},
	{640,480},
	{720,480}, 
	{800,480},
	{1024,768},
	{1280,848},
	{1280,960},
	{1600,1072},
	{1600,960},
	{1600,1200},
	{2048,1232},
	{2048,1536},  
	{2560,1536},  
	{2560,1920},  
};

struct s5k4ecgx_scene_setting {
	int iso;
	int photometry;
	int ev;
	int wb;
	int sharpness;
	int saturation;
};



/* Image Effect */
#define S5K4ECGX_EFFECT_OFF				1
#define S5K4ECGX_EFFECT_MONO			11	// 2
#define S5K4ECGX_EFFECT_SEPIA			5	// 3
#define S5K4ECGX_EFFECT_NEGATIVE		4
#define S5K4ECGX_EFFECT_AQUA			8	// 5
#define S5K4ECGX_EFFECT_SKETCH			6
#define S5K4ECGX_EFFECT_DEFAULT			(S5K4ECGX_EFFECT_OFF)

/* White Balance */
#define S5K4ECGX_WB_AUTO               	1
#define S5K4ECGX_WB_DAYLIGHT           	2
#define S5K4ECGX_WB_CLOUDY             	3
#define S5K4ECGX_WB_FLUORESCENT			5	// 4
#define S5K4ECGX_WB_INCANDESCENT		4	// 5
#define S5K4ECGX_WB_DEFAULT				(S5K4ECGX_WB_AUTO)

/* EV */
#define S5K4ECGX_EV_MINUS_4    		1
#define S5K4ECGX_EV_MINUS_3    		2
#define S5K4ECGX_EV_MINUS_2    		3
#define S5K4ECGX_EV_MINUS_1    		4
#define S5K4ECGX_EV_DEFAULT      	5
#define S5K4ECGX_EV_PLUS_1     		6
#define S5K4ECGX_EV_PLUS_2     		7
#define S5K4ECGX_EV_PLUS_3     		8
#define S5K4ECGX_EV_PLUS_4     		9

/* Scene Mode */
#define S5K4ECGX_SCENE_OFF				1
#define S5K4ECGX_SCENE_PORTRAIT			12	// 2 
#define S5K4ECGX_SCENE_LANDSCAPE		10	// 3
#define S5K4ECGX_SCENE_SPORTS			15	// 4
#define S5K4ECGX_SCENE_PARTY			14	// 5
#define S5K4ECGX_SCENE_BEACH			6
#define S5K4ECGX_SCENE_SUNSET			3	// 7
#define S5K4ECGX_SCENE_DAWN				4	// 8
#define S5K4ECGX_SCENE_FALL				13	// 9
#define S5K4ECGX_SCENE_NIGHT			9	// 10
#define S5K4ECGX_SCENE_BACKLIGHT		7	// 11
#define S5K4ECGX_SCENE_FIRE				11	// 12
#define S5K4ECGX_SCENE_TEXT				8	// 13
#define S5K4ECGX_SCENE_CANDLE			5	// 14
#define S5K4ECGX_SCENE_SPORTS_OFF		16	// 15
#define S5K4ECGX_SCENE_NIGHT_OFF		17	// 16
#define S5K4ECGX_SCENE_DEFAULT			(S5K4ECGX_SCENE_OFF)		

/* Contrast */
#define S5K4ECGX_CONTRAST_MINUS_2      3
#define S5K4ECGX_CONTRAST_MINUS_1      4
#define S5K4ECGX_CONTRAST_DEFAULT      5
#define S5K4ECGX_CONTRAST_PLUS_1       6
#define S5K4ECGX_CONTRAST_PLUS_2       7

/* Saturation */
#define S5K4ECGX_SATURATION_MINUS_2    3
#define S5K4ECGX_SATURATION_MINUS_1    4
#define S5K4ECGX_SATURATION_DEFAULT    5
#define S5K4ECGX_SATURATION_PLUS_1     6
#define S5K4ECGX_SATURATION_PLUS_2     7

/* Sharpness */
#define S5K4ECGX_SHARPNESS_MINUS_2	3
#define S5K4ECGX_SHARPNESS_MINUS_1	4
#define S5K4ECGX_SHARPNESS_DEFAULT	5
#define S5K4ECGX_SHARPNESS_PLUS_1	6
#define S5K4ECGX_SHARPNESS_PLUS_2	7

/* Photometry */
#define S5K4ECGX_PHOTOMETRY_MATRIX	1
#define S5K4ECGX_PHOTOMETRY_CENTER   	2	// 3	
#define S5K4ECGX_PHOTOMETRY_SPOT     	3	// 2
#define S5K4ECGX_PHOTOMETRY_DEFAULT		(S5K4ECGX_PHOTOMETRY_CENTER)

/* ISO */
#define S5K4ECGX_ISO_AUTO        		1
#define S5K4ECGX_ISO_50          		2
#define S5K4ECGX_ISO_100         		3
#define S5K4ECGX_ISO_200         		4
#define S5K4ECGX_ISO_400         		5
#define S5K4ECGX_ISO_DEFAULT			(S5K4ECGX_ISO_AUTO)

/* AUTO CONTRAST */
#define S5K4ECGX_AUTO_CONTRAST_ON		0 
#define S5K4ECGX_AUTO_CONTRAST_OFF		1

/* fps */
#define S5K4ECGX_15_FPS	        		1
#define S5K4ECGX_FPS_DEFAULT		30

/* Auto Exposure & Auto White Balance */
/*
#define S5K4ECGX_AE_LOCK_AWB_LOCK      1
#define S5K4ECGX_AE_LOCK_AWB_UNLOCK    2
#define S5K4ECGX_AE_UNLOCK_AWB_LOCK    3
#define S5K4ECGX_AE_UNLOCK_AWB_UNLOCK  4
#define S5K4ECGX_AE_AWB_DEFAULT			(S5K4ECGX_AE_UNLOCK_AWB_UNLOCK)
*/

/* Auto Exposure Lock or Unlock */
#define S5K4ECGX_AE_LOCK		1
#define S5K4ECGX_AE_UNLOCK		2
#define S5K4ECGX_AWB_LOCK		3
#define S5K4ECGX_AWB_UNLOCK		4

/* Focus Mode */
#define S5K4ECGX_AF_SET_NORMAL		1
#define S5K4ECGX_AF_SET_MACRO		2
#define S5K4ECGX_AF_SET_OFF		3
#define S5K4ECGX_AF_SET_NORMAL_1	10
#define S5K4ECGX_AF_SET_NORMAL_2	11
#define S5K4ECGX_AF_SET_NORMAL_3	12
#define S5K4ECGX_AF_SET_MACRO_1		15
#define S5K4ECGX_AF_SET_MACRO_2		16
#define S5K4ECGX_AF_SET_MACRO_3		17

/* Focust start/stop */
#define S5K4ECGX_AF_START			1
#define S5K4ECGX_AF_STOP			2
#define S5K4ECGX_AF_STOP_STEP_1			10
#define S5K4ECGX_AF_STOP_STEP_2			11
#define S5K4ECGX_AF_STOP_STEP_3			12

/* Auto Focus Status */
#define S5K4ECGX_AF_STATUS_PROGRESS    	1
#define S5K4ECGX_AF_STATUS_SUCCESS     	2
#define S5K4ECGX_AF_STATUS_FAIL        		3
#define S5K4ECGX_AF_STATUS_CANCELED	4       
#define S5K4ECGX_AF_STATUS_TIMEOUT	5       
#define S5K4ECGX_AE_STATUS_STABLE	6
#define S5K4ECGX_AE_STATUS_UNSTABLE	7

#define S5K4ECGX_AF_CHECK_STATUS	0
#define S5K4ECGX_AF_OFF			1
#define S5K4ECGX_AF_DO			4
#define S5K4ECGX_AF_SET_MANUAL		5
#define S5K4ECGX_AF_CHECK_2nd_STATUS	6
#define S5K4ECGX_AF_SET_AE_FOR_FLASH	7
#define S5K4ECGX_AF_BACK_AE_FOR_FLASH	8
#define S5K4ECGX_AF_CHECK_AE_STATUS	9
#define S5K4ECGX_AF_POWEROFF		10

/* JPEG Quality */
#define S5K4ECGX_JPEG_SUPERFINE			1
#define S5K4ECGX_JPEG_FINE				2
#define S5K4ECGX_JPEG_NORMAL			3
#define S5K4ECGX_JPEG_DEFAULT			(S5K4ECGX_JPEG_SUPERFINE)

#define S5K4ECGX_WDR_OFF			1
#define S5K4ECGX_WDR_ON				2

#define S5K4ECGX_INVALID_VALUE 			(0xff)	

#define S5K4ECGX_ZOOM_DEFAULT          0
#define S5K4ECGX_ZOOM_1P00X            1
#define S5K4ECGX_ZOOM_1P25X            2
#define S5K4ECGX_ZOOM_1P50X            3
#define S5K4ECGX_ZOOM_1P75X            4
#define S5K4ECGX_ZOOM_2P00X            5
#define S5K4ECGX_ZOOM_2P25X            6
#define S5K4ECGX_ZOOM_2P50X            7
#define S5K4ECGX_ZOOM_2P75X            8
#define S5K4ECGX_ZOOM_3P00X            9
#define S5K4ECGX_ZOOM_3P25X            10
#define S5K4ECGX_ZOOM_3P50X            11
#define S5K4ECGX_ZOOM_3P75X            12
#define S5K4ECGX_ZOOM_4P00X            13

#define S5K4ECGX_ZOOM_STEP_0		0
#define S5K4ECGX_ZOOM_STEP_1		1
#define S5K4ECGX_ZOOM_STEP_2		2
#define S5K4ECGX_ZOOM_STEP_3		3
#define S5K4ECGX_ZOOM_STEP_4		4
#define S5K4ECGX_ZOOM_STEP_5		5
#define S5K4ECGX_ZOOM_STEP_6		6
#define S5K4ECGX_ZOOM_STEP_7		7
#define S5K4ECGX_ZOOM_STEP_8		8

const static struct s5k4ecgx_scene_setting s5k4ecgx_scene_list[] = {
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, 0, 0},		// None
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, -1, 0},		// Portrait - 1
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_MATRIX, 0, S5K4ECGX_WB_AUTO, 1, 1},		// Landscape - 2
	{S5K4ECGX_INVALID_VALUE, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, 0, 0},	// Sport 
	{S5K4ECGX_ISO_200, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, 0, 1},		// Party/Indoor
	{S5K4ECGX_ISO_50, S5K4ECGX_PHOTOMETRY_CENTER, 1, S5K4ECGX_WB_AUTO, 0, 1},		// Beach/Snow
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_DAYLIGHT, 0, 0},		// Sunset
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_FLUORESCENT, 0, 0},		// Dawn
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, 0, 2},		// Fall
	{S5K4ECGX_INVALID_VALUE, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, 0, 0},		// Night
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_SPOT, 0, S5K4ECGX_WB_AUTO, 0, 0},		// Against Light
	{S5K4ECGX_ISO_50, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, 0, 0},		// Fire 
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_AUTO, 2, 0},		// Text 
	{S5K4ECGX_ISO_AUTO, S5K4ECGX_PHOTOMETRY_CENTER, 0, S5K4ECGX_WB_DAYLIGHT, 0, 0},		// Candle
};

#endif /* ifndef S5K4ECGX_H */

