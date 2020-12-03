/*
 * drivers/media/video/mt9p012.c
 *
 * mt9p012 sensor driver
 *
 *
 * Copyright (C) 2008 Texas Instruments.
 *
 * Leverage OV9640.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *****************************************************
 *****************************************************
 * modules/camera/S5K5CCGX.c
 *
 * S5K5CCGX sensor driver source file
 *
 * Modified by paladin in Samsung Electronics
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <media/v4l2-int-device.h>

#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>

#include "isp/isp.h"
#include "omap34xxcam.h"
#include "s5k5ccgx_tune.h"
#include "s5k5ccgx.h"
#include "Camsensor_tunner_32bits_reg.h"

#if (CAM_S5K5CCGX_DBG_MSG)
#include "dprintk.h"
#else
#define dprintk(x, y...)
#endif

static int debug = 1;
module_param(debug, int, 0644);

#define dprintk(level, fmt, arg...) do {            \
	if (debug >= level)                     \
	printk(S5K5CCGX_MOD_NAME fmt , ## arg); } while (0)

static struct delayed_work camera_esd_check;

static u32 s5k5ccgx_curr_state = S5K5CCGX_STATE_INVALID;
static u32 s5k5ccgx_pre_state = S5K5CCGX_STATE_INVALID;

//static int s5k5ccgx_set_focus(s32 value);

static struct s5k5ccgx_sensor S5K5CCGX = {
	.timeperframe = {
		.numerator    = 1,
		.denominator  = 30,
	},
	.fps				= 30,
	//.bv				= 0,
	.state				= S5K5CCGX_STATE_PREVIEW,
	.mode				= S5K5CCGX_MODE_CAMERA,
	.preview_size		= S5K5CCGX_PREVIEW_SIZE_640_480,
	.capture_size		= S5K5CCGX_CAPTURE_SIZE_2048_1536,
	.detect				= SENSOR_NOT_DETECTED,
	//.focus_mode		= S5K5CCGX_AF_INIT_NORMAL,
	.effect				= S5K5CCGX_EFFECT_OFF,
	.iso				= S5K5CCGX_ISO_AUTO,
	.photometry			= S5K5CCGX_PHOTOMETRY_CENTER,
	.ev					= S5K5CCGX_EV_DEFAULT,
	//.wdr				= S5K5CCGX_WDR_OFF,
	.contrast			= S5K5CCGX_CONTRAST_DEFAULT,
	.saturation			= S5K5CCGX_SATURATION_DEFAULT,
	.sharpness			= S5K5CCGX_SHARPNESS_DEFAULT,
	.wb					= S5K5CCGX_WB_AUTO,
	//.isc 				= S5K5CCGX_ISC_STILL_OFF,
	.scene				= S5K5CCGX_SCENE_OFF,
	.aewb				= S5K5CCGX_AWB_AE_UNLOCK,
	//.antishake		= S5K5CCGX_ANTI_SHAKE_OFF,
	//.flash_capture	= S5K5CCGX_FLASH_CAPTURE_OFF,
	//.flash_movie		= S5K5CCGX_FLASH_MOVIE_OFF,
	.jpeg_quality		= S5K5CCGX_JPEG_SUPERFINE, 
	//.zoom				= S5K5CCGX_ZOOM_1P00X,
	.thumb_offset		= S5K5CCGX_THUMBNAIL_OFFSET,
	.yuv_offset			= S5K5CCGX_YUV_OFFSET,
	.jpeg_capture_w		= JPEG_CAPTURE_WIDTH,
	.jpeg_capture_h		= JPEG_CAPTURE_HEIGHT,
};


struct v4l2_queryctrl S5K5CCGX_ctrl_list[] = {
	{
		.id            = V4L2_CID_SELECT_MODE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "select mode",
		.minimum       = S5K5CCGX_MODE_CAMERA,
		.maximum       = S5K5CCGX_MODE_CAMCORDER,
		.step          = 1,
		.default_value = S5K5CCGX_MODE_CAMERA,
	},    
	{
		.id            = V4L2_CID_SELECT_STATE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "select state",
		.minimum       = S5K5CCGX_STATE_PREVIEW,
		.maximum       = S5K5CCGX_STATE_CAPTURE,
		.step          = 1,
		.default_value = S5K5CCGX_STATE_PREVIEW,
	},    
	/*
	{
		.id            = V4L2_CID_FOCUS_MODE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Focus Mode",
		.minimum       = S5K5CCGX_AF_NORMAL,
		.maximum       = S5K5CCGX_AF_OFF,
		.step          = 1,
		.default_value = S5K5CCGX_AF_NORMAL,
	},
	{
		.id            = V4L2_CID_AF,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Focus Status",
		.minimum       = S5K5CCGX_AF_START,
		.maximum       = S5K5CCGX_AF_STOP,
		.step          = 1,
		.default_value = S5K5CCGX_AF_STOP,
	},
	*/
	{
		.id            = V4L2_CID_ISO,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "ISO",
		.minimum       = S5K5CCGX_ISO_AUTO,
		.maximum       = S5K5CCGX_ISO_400,
		.step          = 1,
		.default_value = S5K5CCGX_ISO_AUTO,
	},
	{
		.id			 = V4L2_CID_BRIGHTNESS,
		.type 		 = V4L2_CTRL_TYPE_INTEGER,
		.name 		 = "Brightness",
		.minimum		 = S5K5CCGX_EV_MINUS_4,
		.maximum		 = S5K5CCGX_EV_PLUS_4,
		.step 		 = 1,
		.default_value = S5K5CCGX_EV_DEFAULT,
	},
	{
		.id            = V4L2_CID_WB,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "White Balance",
		.minimum       = S5K5CCGX_WB_AUTO,
		.maximum       = S5K5CCGX_WB_FLUORESCENT,
		.step          = 1,
		.default_value = S5K5CCGX_WB_AUTO,
	},
	{
		.id            = V4L2_CID_EFFECT,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Effect",
		.minimum       = S5K5CCGX_EFFECT_OFF,
		.maximum       = S5K5CCGX_EFFECT_MONO,
		.step          = 1,
		.default_value = S5K5CCGX_EFFECT_OFF,
	},
	{
		.id			 = V4L2_CID_CONTRAST,
		.type 		 = V4L2_CTRL_TYPE_INTEGER,
		.name 		 = "Contrast",
		.minimum		 = S5K5CCGX_CONTRAST_MINUS_2,
		.maximum	= S5K5CCGX_CONTRAST_PLUS_2,
		.step 		 = 1,
		.default_value = S5K5CCGX_CONTRAST_DEFAULT,
	},	
	{
		.id			 = V4L2_CID_SATURATION,
		.type 		 = V4L2_CTRL_TYPE_INTEGER,
		.name 		 = "Saturation",
		.minimum		 = S5K5CCGX_SATURATION_MINUS_2,
		.maximum	 = S5K5CCGX_SATURATION_PLUS_2,
		.step 		 = 1,
		.default_value = S5K5CCGX_SATURATION_DEFAULT,
	},
	{
		.id 		   = V4L2_CID_SHARPNESS,
		.type		   = V4L2_CTRL_TYPE_INTEGER,
		.name		   = "Sharpness",
		.minimum	   = S5K5CCGX_SHARPNESS_MINUS_2,
		.maximum	= S5K5CCGX_SHARPNESS_PLUS_2,
		.step		   = 1,
		.default_value = S5K5CCGX_SHARPNESS_DEFAULT,
	},
	{
		.id            = V4L2_CID_SCENE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Scene",
		.minimum       = S5K5CCGX_SCENE_OFF,
		.maximum       = S5K5CCGX_SCENE_NIGHT_OFF,
		.step          = 1,
		.default_value = S5K5CCGX_SCENE_OFF,
	},
	{
		.id            = V4L2_CID_PHOTOMETRY,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Photometry",
		.minimum       = S5K5CCGX_PHOTOMETRY_NORMAL,
		.maximum       = S5K5CCGX_PHOTOMETRY_SPOT,
		.step          = 1,
		.default_value = S5K5CCGX_PHOTOMETRY_NORMAL,
	},
	{
		.id            = V4L2_CID_AEWB,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Auto Exposure/Auto White Balance",
		.minimum       = S5K5CCGX_AWB_AE_LOCK,
		.maximum       = S5K5CCGX_AWB_AE_UNLOCK,
		.step          = 1,
		.default_value = S5K5CCGX_AWB_AE_UNLOCK,
	},
};
#define NUM_S5K5CCGX_CONTROL ARRAY_SIZE(S5K5CCGX_ctrl_list)

/* list of image formats supported by S5K5CCGX sensor */
const static struct v4l2_fmtdesc S5K5CCGX_formats[] = {
	{
		.description	= "YUV422 (UYVY)",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
	},
	{
		.description	= "YUV422 (YUYV)",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
	},
	{
		//.type			= V4L2_BUF_TYPE_VIDEO_CAPTURE
		.description 	= "JPEG(without header)+ JPEG",
		.pixelformat 	= V4L2_PIX_FMT_JPEG,
	},
};
#define NUM_S5K5CCGX_FORMATS ARRAY_SIZE(S5K5CCGX_formats)

extern struct s5k5ccgx_platform_data s5k5ccgx_platform_data0;

#define CAMSENSOR_REGSET_INITIALIZE( ArrReg, S5K5CCGXArrReg ) \
{\
	ArrReg.reg32 = S5K5CCGXArrReg; \
	ArrReg.num = ARRAY_SIZE(S5K5CCGXArrReg); \
	ArrReg.nDynamicLoading = 0; \
}

#define CAMSENSOR_WRITE_REGSETS(ArrReg) \
{ \
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX; \
	struct i2c_client *client = sensor->i2c_client; \
	int i = 0; \    
	int err = 0; \
			  for(i=0; i<ArrReg.num; i++) \
			  { \
				  err = s5k5ccgx_write_word(client, ArrReg.reg32[i].addr, ArrReg.reg32[i].value); \
					  if(unlikely(err < 0 )){ \
						  v4l_info(client, "%s: register set failed\n", __func__); \
					  } \
			  } \
}

//printk("{0x%x, 0x%x}\n", ArrReg.reg32[i].addr, ArrReg.reg32[i].value); \

#define	I2C_RW_WRITE 	0
#define	I2C_RW_READ 	1

static int s5k5ccgx_write_word(struct i2c_client *client, 
		unsigned short subaddr, unsigned short val)
{
	unsigned char buf[4];
	struct i2c_msg msg = { client->addr, I2C_RW_WRITE, 4, buf };

	if ( subaddr == 0xffff)
	{
		mdelay(val);
		return 0;
	}

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);	
	buf[2] = (val >> 8);
	buf[3] = (val & 0xFF);

	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

static int s5k5ccgx_read_word(struct i2c_client *client, unsigned short subaddr, unsigned short *data)
{
	int ret;
	unsigned char buf[2];
	struct i2c_msg msg = { client->addr, I2C_RW_WRITE, 2, buf };

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);

	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	/* if (ret == -EIO) */
	if (unlikely(ret == -EIO))
		goto error;

	msg.flags = I2C_RW_READ;

	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	/* if (ret == -EIO) */
	if (unlikely(ret == -EIO))
		goto error;

	*data = ((buf[0] << 8) | buf[1]);

error:
	return ret;
}

static void LoadCamsensorRegSettings()
{
	dprintk(5, "LoadCamsensorRegSettings)+ \n");
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_INIT, 								S5K5CCGX_TUNING_INIT );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_EFFECT_OFF, 						S5K5CCGX_TUNING_EFFECT_OFF );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_EFFECT_MONO, 					S5K5CCGX_TUNING_EFFECT_MONO );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_EFFECT_SEPIA, 						S5K5CCGX_TUNING_EFFECT_SEPIA );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_EFFECT_NEGATIVE, 					S5K5CCGX_TUNING_EFFECT_NEGATIVE );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_EFFECT_AQUA, 					S5K5CCGX_TUNING_EFFECT_AQUA);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_EFFECT_SKETCH, 					S5K5CCGX_TUNING_EFFECT_SKETCH);

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_WHITE_BALANCE_AUTO, 				S5K5CCGX_TUNING_WHITE_BALANCE_AUTO );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_WHITE_BALANCE_DAYLIGHT, 			S5K5CCGX_TUNING_WHITE_BALANCE_DAYLIGHT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_WHITE_BALANCE_CLOUDY, 			S5K5CCGX_TUNING_WHITE_BALANCE_CLOUDY );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_WHITE_BALANCE_FLUORESCENT, 		S5K5CCGX_TUNING_WHITE_BALANCE_FLUORESCENT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_WHITE_BALANCE_INCANDESCENT, 		S5K5CCGX_TUNING_WHITE_BALANCE_INCANDESCENT );	
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_N_4, 					S5K5CCGX_TUNING_BRIGHTNESS_N_4 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_N_3, 					S5K5CCGX_TUNING_BRIGHTNESS_N_3 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_N_2, 					S5K5CCGX_TUNING_BRIGHTNESS_N_2 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_N_1, 					S5K5CCGX_TUNING_BRIGHTNESS_N_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_0, 					S5K5CCGX_TUNING_BRIGHTNESS_0 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_P_1, 					S5K5CCGX_TUNING_BRIGHTNESS_P_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_P_2, 					S5K5CCGX_TUNING_BRIGHTNESS_P_2 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_P_3, 					S5K5CCGX_TUNING_BRIGHTNESS_P_3 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_BRIGHTNESS_P_4, 					S5K5CCGX_TUNING_BRIGHTNESS_P_4 );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CONTRAST_N_2, 					S5K5CCGX_TUNING_CONTRAST_M_2 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CONTRAST_N_1, 					S5K5CCGX_TUNING_CONTRAST_M_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CONTRAST_0, 					          S5K5CCGX_TUNING_CONTRAST_0 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CONTRAST_P_1, 					S5K5CCGX_TUNING_CONTRAST_P_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CONTRAST_P_2, 					S5K5CCGX_TUNING_CONTRAST_P_2 );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SATURATION_N_2, 					S5K5CCGX_TUNING_SATURATION_M_2 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SATURATION_N_1, 					S5K5CCGX_TUNING_SATURATION_M_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SATURATION_0, 					S5K5CCGX_TUNING_SATURATION_0 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SATURATION_P_1, 					S5K5CCGX_TUNING_SATURATION_P_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SATURATION_P_2, 					S5K5CCGX_TUNING_SATURATION_P_2 );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SHARPNESS_N_2, 					S5K5CCGX_TUNING_SHARPNESS_M_2 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SHARPNESS_N_1, 					S5K5CCGX_TUNING_SHARPNESS_M_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SHARPNESS_0, 					          S5K5CCGX_TUNING_SHARPNESS_0 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SHARPNESS_P_1, 					S5K5CCGX_TUNING_SHARPNESS_P_1 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SHARPNESS_P_2, 					S5K5CCGX_TUNING_SHARPNESS_P_2 );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_OFF, 						S5K5CCGX_TUNING_SCENE_OFF );
	//	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_OFF_NONE, 					S5K5CCGX_TUNING_SCENE_OFF_NONE);
	//	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_OFF_SPORTS, 				S5K5CCGX_TUNING_SCENE_OFF_SPORTS);
	//	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_OFF_NIGHT, 					S5K5CCGX_TUNING_SCENE_OFF_NIGHT);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_PORTRAIT, 					S5K5CCGX_TUNING_SCENE_PORTRAIT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_LANDSCAPE, 				S5K5CCGX_TUNING_SCENE_LANDSCAPE );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_SPORTS, 					S5K5CCGX_TUNING_SCENE_SPORTS );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_PARTY, 						S5K5CCGX_TUNING_SCENE_PARTY );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_BEACH, 					S5K5CCGX_TUNING_SCENE_BEACH );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_SUNSET, 					S5K5CCGX_TUNING_SCENE_SUNSET);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_DAWN, 						S5K5CCGX_TUNING_SCENE_DAWN );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_FALL, 						S5K5CCGX_TUNING_SCENE_FALL);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_NIGHT, 						S5K5CCGX_TUNING_SCENE_NIGHT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_BACKLIGHT, 					S5K5CCGX_TUNING_SCENE_BACKLIGHT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_FIRE, 						S5K5CCGX_TUNING_SCENE_FIRE );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_TEXT, 						S5K5CCGX_TUNING_SCENE_TEXT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_SCENE_CANDLE, 					S5K5CCGX_TUNING_SCENE_CANDLE );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_METERING_NORMAL, 				S5K5CCGX_TUNING_METERING_NORMAL );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_METERING_SPOT, 					S5K5CCGX_TUNING_METERING_SPOT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_METERING_CENTER, 					S5K5CCGX_TUNING_METERING_CENTER );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_ISO_AUTO, 						S5K5CCGX_TUNING_ISO_AUTO );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_ISO_50, 							S5K5CCGX_TUNING_ISO_50 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_ISO_100, 							S5K5CCGX_TUNING_ISO_100 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_ISO_200, 							S5K5CCGX_TUNING_ISO_200 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_ISO_400, 							S5K5CCGX_TUNING_ISO_400 );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_PREVIEW, 							S5K5CCGX_TUNING_PREVIEW );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_HIGH_SNAPSHOT, 					S5K5CCGX_TUNING_HIGH_SNAPSHOT);	
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_NORMAL_SNAPSHOT, 					S5K5CCGX_TUNING_NORMAL_SNAPSHOT);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_LOWLIGHT_SNAPSHOT, 				S5K5CCGX_TUNING_LOWLIGHT_SNAPSHOT );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_NIGHT_SNAPSHOT, 					S5K5CCGX_TUNING_NIGHT_SNAPSHOT);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_5_FPS, 							S5K5CCGX_TUNING_5_FPS );		//new
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_7_FPS, 							S5K5CCGX_TUNING_7_FPS );		//new
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_10_FPS, 							S5K5CCGX_TUNING_10_FPS );		//new
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_15_FPS, 							S5K5CCGX_TUNING_15_FPS );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_30_FPS, 							S5K5CCGX_TUNING_30_FPS );		//new
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AUTO15_FPS, 						S5K5CCGX_TUNING_AUTO15_FPS );		//new
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AUTO30_FPS, 						S5K5CCGX_TUNING_AUTO30_FPS );		//new
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AE_LOCK, 							S5K5CCGX_TUNING_AE_LOCK );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AE_UNLOCK, 						S5K5CCGX_TUNING_AE_UNLOCK );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AF_DO, 							S5K5CCGX_TUNING_AF_DO );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AF_NORMAL_ON, 					S5K5CCGX_TUNING_AF_NORMAL_ON );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AF_MACRO_ON, 						S5K5CCGX_TUNING_AF_MACRO_ON );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_AF_OFF, 							S5K5CCGX_TUNING_AF_OFF );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_DTP_ON, 							S5K5CCGX_TUNING_DTP_ON );		// new
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_DTP_OFF, 							S5K5CCGX_TUNING_DTP_OFF );		// new

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_PREVIEW_SIZE_320, 				S5K5CCGX_TUNING_PREVIEW_SIZE_320 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_PREVIEW_SIZE_640, 				S5K5CCGX_TUNING_PREVIEW_SIZE_640 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_PREVIEW_SIZE_720, 				S5K5CCGX_TUNING_PREVIEW_SIZE_720 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_PREVIEW_SIZE_800, 				S5K5CCGX_TUNING_PREVIEW_SIZE_800 );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CAPTURE_SIZE_640, 				S5K5CCGX_TUNING_CAPTURE_SIZE_640 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CAPTURE_SIZE_1600, 				S5K5CCGX_TUNING_CAPTURE_SIZE_1600 );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CAPTURE_SIZE_2048, 				S5K5CCGX_TUNING_CAPTURE_SIZE_2048 );

	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CAPTURE_SIZE_800W, 				S5K5CCGX_TUNING_CAPTURE_SIZE_800W);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CAPTURE_SIZE_1600W, 				S5K5CCGX_TUNING_CAPTURE_SIZE_1600W );
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_CAPTURE_SIZE_2048W, 				S5K5CCGX_TUNING_CAPTURE_SIZE_2048W );
	
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_JPEG_QUALITY_SUPERFINE, 			SK5KCCGX_TUNING_JPEG_QUALITY_SUPERFINE);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_JPEG_QUALITY_FINE, 				SK5KCCGX_TUNING_JPEG_QUALITY_FINE);
	CAMSENSOR_REGSET_INITIALIZE( CAM_REG32SET_JPEG_QUALITY_NORMAL, 				SK5KCCGX_TUNING_JPEG_QUALITY_NORMAL);

#ifdef USE_SD_CARD_TUNE
	Load32BitTuningFile();    
#endif
}


static int s5k5ccgx_detect(struct i2c_client *client)
{
	int ret;
	dprintk(5, "s5k5ccgx_detect is called...\n");

	/* Start Camera Program */

	u16 ID = 0xFFFF;
	if(s5k5ccgx_write_word(client, 0xFCFC, 0xD000)) printk("Error : I2C (line:%d)\n", __LINE__);
	if(s5k5ccgx_write_word(client, 0x0028, 0xD000)) printk("Error : I2C (line:%d)\n", __LINE__);
	//Clear host interrupt so main will wait!
	if(s5k5ccgx_write_word(client, 0x002A, 0x1030)) printk("Error : I2C (line:%d)\n", __LINE__);
	if(s5k5ccgx_write_word(client, 0x0F12, 0x0000)) printk("Error : I2C (line:%d)\n", __LINE__);

	//arm go!!
	if(s5k5ccgx_write_word(client, 0x002A, 0x0014))	printk("Error : I2C (line:%d)\n", __LINE__);

	/* Read Firmware Version */
	if(s5k5ccgx_write_word(client, 0x002C, 0x0000)) printk("Error : I2C (line:%d)\n", __LINE__);
	if(s5k5ccgx_write_word(client, 0x002E, 0x0040)) printk("Error : I2C (line:%d)\n", __LINE__);

	ret = s5k5ccgx_read_word(client, 0x0F12, &ID);
	printk(S5K5CCGX_MOD_NAME "ret of s5k5ccgx_read_word == 0x%x\n", ret);

	if(ID == 0x05CC)
	{
		printk(S5K5CCGX_MOD_NAME"=================================\n");
		printk(S5K5CCGX_MOD_NAME"   [3MEGA CAM] vendor_id ID : 0x%04X\n", ID);
		printk(S5K5CCGX_MOD_NAME"   Sensor is Detected!!!\n");		
		printk(S5K5CCGX_MOD_NAME"=================================\n");            
	}
	else
	{
		printk(S5K5CCGX_MOD_NAME"-------------------------------------------------\n");
		printk(S5K5CCGX_MOD_NAME"   [3MEGA CAM] sensor detect failure !!\n");
		printk(S5K5CCGX_MOD_NAME"   ID : 0x%04X[ID should be 0x05CC]\n", ID);
		printk(S5K5CCGX_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}	
	return 0;	
}

camera_light_status_type S5K5CCGX_check_illuminance_status()
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	struct i2c_client *client = sensor->i2c_client;

	static bool isFirst = true;
	int iRetrialCount = 5;
	int ret ;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;	
	u32 lightStatus = 0;
	u16 low_threshold_low_word = 0;
	u16 low_threshold_high_word= 0;	
	static u32 low_threshold = 0;
	u16 high_threshold_low_word = 0;
	u16 high_threshold_high_word= 0;	
	static u32 high_threshold = 0;

	dprintk(5, "S5K5CCGX_check_illuminance_status() \r\n");
	camera_light_status_type LightStatus = CAMERA_SENSOR_LIGHT_STATUS_INVALID;

	// get threshold value
	if( isFirst == true )
	{
		s5k5ccgx_write_word(client, 0xFCFC, 0xD000);
		s5k5ccgx_write_word(client, 0x002C, 0x7000);
		s5k5ccgx_write_word(client, 0x002E, 0x032E); 
		ret = s5k5ccgx_read_word (client, 0x0F12, &high_threshold_low_word);		
		s5k5ccgx_write_word(client, 0x002E, 0x0330); 
		ret = s5k5ccgx_read_word (client, 0x0F12, &high_threshold_high_word);		
		high_threshold = high_threshold_low_word | (high_threshold_high_word << 16);

		s5k5ccgx_write_word(client, 0xFCFC, 0xD000);
		s5k5ccgx_write_word(client, 0x002C, 0x7000);
		s5k5ccgx_write_word(client, 0x002E, 0x0332); 
		ret = s5k5ccgx_read_word (client, 0x0F12, &low_threshold_low_word);		
		s5k5ccgx_write_word(client, 0x002E, 0x0334); 
		ret = s5k5ccgx_read_word (client, 0x0F12, &low_threshold_high_word);		
		low_threshold = low_threshold_low_word | (low_threshold_high_word << 16);

		isFirst = false;
		dprintk(5, "high_threshold = 0x%08x, low_threshold = 0x%08x \r\n",high_threshold,low_threshold);		
	}

	do
	{		
		s5k5ccgx_write_word(client, 0xFCFC, 0xD000);
		s5k5ccgx_write_word(client, 0x002C, 0x7000);
		s5k5ccgx_write_word(client, 0x002E, 0x2448); 
		ret = s5k5ccgx_read_word (client, 0x0F12, &lightStatus_low_word);		
		s5k5ccgx_write_word(client, 0x002E, 0x244A); 
		ret = s5k5ccgx_read_word (client, 0x0F12, &lightStatus_high_word);	

		lightStatus = lightStatus_low_word  | (lightStatus_high_word << 16);

		if ( ret == - EIO )
		{
			dprintk(5, "S5K5CCGX_check_illuminance_status - Failed to read a lowlight status \r\n");
			goto RETRY;
		}
		else
		{
			if ( lightStatus > high_threshold )
			{
				dprintk(5, "S5K5CCGX_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_HIGH \r\n");
				LightStatus = CAMERA_SENSOR_LIGHT_STATUS_HIGH;
			}
			else if (lightStatus < low_threshold )
			{
				dprintk(5, "S5K5CCGX_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_LOW \r\n");
				LightStatus = CAMERA_SENSOR_LIGHT_STATUS_LOW; 
			}
			else
			{
				dprintk(5, "S5K5CCGX_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_NORMAL \r\n");
				LightStatus =  CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
			}
			return LightStatus;
		}

RETRY:
		mdelay(10);
	}while ( ( ret == - EIO ) && ( --iRetrialCount ) );
	return LightStatus;
}

static int s5k5ccgx_set_effect(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_effect is called...[%d]\n",value);

	sensor->effect = value;
	switch(sensor->effect)
	{
		case S5K5CCGX_EFFECT_OFF:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_EFFECT_OFF );
			break;
		case S5K5CCGX_EFFECT_MONO:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_EFFECT_MONO );
			break;    
		case S5K5CCGX_EFFECT_SEPIA:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_EFFECT_SEPIA );
			break;
		case S5K5CCGX_EFFECT_NEGATIVE:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_EFFECT_NEGATIVE );
			break;
		case S5K5CCGX_EFFECT_AQUA:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_EFFECT_AQUA );
			break;
		case S5K5CCGX_EFFECT_SKETCH:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_EFFECT_SKETCH );
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "Effect value is not supported!!!\n");
			return -EINVAL;
	}

	return 0;
}

static int s5k5ccgx_set_iso(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_iso is called...[%d]\n",value);

	sensor->iso = value;
	switch(sensor->iso)
	{
		case S5K5CCGX_ISO_AUTO:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_ISO_AUTO );
			break;
		case S5K5CCGX_ISO_50:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_ISO_50 );
			break;      
		case S5K5CCGX_ISO_100:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_ISO_100 );
			break;      
		case S5K5CCGX_ISO_200:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_ISO_200 );
			break;      
		case S5K5CCGX_ISO_400:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_ISO_400 );
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "ISO value is not supported!!!\n");
			return -EINVAL;
	}
	return 0;
}

static int s5k5ccgx_set_photometry(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_photometry is called...[%d]\n",value);

	sensor->photometry = value;
	switch(sensor->photometry)
	{
		case S5K5CCGX_PHOTOMETRY_NORMAL:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_METERING_NORMAL );
			break;
		case S5K5CCGX_PHOTOMETRY_CENTER:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_METERING_CENTER );
			break;
		case S5K5CCGX_PHOTOMETRY_SPOT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_METERING_SPOT );
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "Photometry value is not supported!!!\n");
			return -EINVAL;
	}

	return 0;
}

static int s5k5ccgx_set_ev(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_ev is called...[%d]\n",value);

	sensor->ev = value;
	switch(sensor->ev)
	{
		case S5K5CCGX_EV_MINUS_4:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_N_4 );
			break;
		case S5K5CCGX_EV_MINUS_3:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_N_3 );
			break;
		case S5K5CCGX_EV_MINUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_N_2 );
			break;
		case S5K5CCGX_EV_MINUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_N_1 );
			break;
		case S5K5CCGX_EV_DEFAULT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_0 );
			break;
		case S5K5CCGX_EV_PLUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_P_1 );
			break;
		case S5K5CCGX_EV_PLUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_P_2 );
			break;
		case S5K5CCGX_EV_PLUS_3:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_P_3 );
			break;
		case S5K5CCGX_EV_PLUS_4:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_BRIGHTNESS_P_4 );
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "EV value is not supported!!!\n");
			return -EINVAL;
	}
	return 0;
}


static int s5k5ccgx_set_contrast(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_contrast is called...[%d]\n",value);

	sensor->contrast = value;
	switch(sensor->ev)
	{
		case S5K5CCGX_CONTRAST_MINUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_CONTRAST_N_2 );
			break;
		case S5K5CCGX_CONTRAST_MINUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_CONTRAST_N_1 );
			break;
		case S5K5CCGX_CONTRAST_DEFAULT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_CONTRAST_0);
			break;
		case S5K5CCGX_CONTRAST_PLUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_CONTRAST_P_1 );
			break;
		case S5K5CCGX_CONTRAST_PLUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_CONTRAST_P_2 );
			break;				
		default:
			printk(S5K5CCGX_MOD_NAME "CONTRAST value is not supported!!!\n");
			return -EINVAL;
	}
	return 0;
}

static int s5k5ccgx_set_saturation(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_saturation is called...[%d]\n",value);

	sensor->saturation = value;
	switch(sensor->ev)
	{
		case S5K5CCGX_SATURATION_MINUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SATURATION_N_2 );
			break;
		case S5K5CCGX_SATURATION_MINUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SATURATION_N_1 );
			break;
		case S5K5CCGX_SATURATION_DEFAULT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SATURATION_0);
			break;
		case S5K5CCGX_SATURATION_PLUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SATURATION_P_1 );
			break;
		case S5K5CCGX_SATURATION_PLUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SATURATION_P_2 );
			break;				
		default:
			printk(S5K5CCGX_MOD_NAME "SATURATION value is not supported!!!\n");
			return -EINVAL;
	}
	return 0;
}

static int s5k5ccgx_set_sharpness(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_sharpness is called...[%d]\n",value);

	sensor->sharpness = value;
	switch(sensor->ev)
	{
		case S5K5CCGX_SHARPNESS_MINUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SHARPNESS_N_2 );
			break;
		case S5K5CCGX_SHARPNESS_MINUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SHARPNESS_N_1 );
			break;
		case S5K5CCGX_SHARPNESS_DEFAULT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SHARPNESS_0);
			break;
		case S5K5CCGX_SHARPNESS_PLUS_1:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SHARPNESS_P_1 );
			break;
		case S5K5CCGX_SHARPNESS_PLUS_2:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SHARPNESS_P_2 );
			break;				
		default:
			printk(S5K5CCGX_MOD_NAME "SHARPNESS value is not supported!!!\n");
			return -EINVAL;
	}
	return 0;
}

static int s5k5ccgx_set_wb(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_wb is called...[%d]\n",value);

	sensor->wb = value;
	switch(sensor->wb)
	{
		case S5K5CCGX_WB_AUTO:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_WHITE_BALANCE_AUTO );					
			break;
		case S5K5CCGX_WB_DAYLIGHT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_WHITE_BALANCE_DAYLIGHT );					
			break;
		case S5K5CCGX_WB_CLOUDY:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_WHITE_BALANCE_CLOUDY );					
			break;
		case S5K5CCGX_WB_FLUORESCENT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_WHITE_BALANCE_FLUORESCENT );					
			break;
		case S5K5CCGX_WB_INCANDESCENT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_WHITE_BALANCE_INCANDESCENT );					
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "WB value is not supported!!!\n");
			return -EINVAL;
	}
	return 0;
}


static int s5k5ccgx_get_scene(struct v4l2_control *vc)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_get_scene is called...\n"); 
	vc->value = sensor->scene;
	return 0;
}

static int s5k5ccgx_set_scene(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_set_scene is called...[%d]\n",value);

	sensor->scene = value;  
	switch(sensor->scene)
	{
		case S5K5CCGX_SCENE_OFF:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_OFF );		
			break;
		case S5K5CCGX_SCENE_PORTRAIT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_PORTRAIT );		
			break;		
		case S5K5CCGX_SCENE_LANDSCAPE:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_LANDSCAPE );		
			break;	
		case S5K5CCGX_SCENE_SPORTS:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_SPORTS );		
			break;			
		case S5K5CCGX_SCENE_PARTY:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_PARTY );		
			break;
		case S5K5CCGX_SCENE_BEACH:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_BEACH );		
			break;
		case S5K5CCGX_SCENE_SUNSET:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_SUNSET );		
			break;
		case S5K5CCGX_SCENE_DAWN:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_DAWN );		
			break;
		case S5K5CCGX_SCENE_FALL:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_FALL );		
			break;
		case S5K5CCGX_SCENE_NIGHT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_NIGHT);		
			break;
		case S5K5CCGX_SCENE_BACKLIGHT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_BACKLIGHT );		
			break;
		case S5K5CCGX_SCENE_FIRE:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_FIRE );		
			break;
		case S5K5CCGX_SCENE_TEXT:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_TEXT );		
			break;
		case S5K5CCGX_SCENE_CANDLE:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_SCENE_CANDLE );		
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "Scene value is not supported!!!\n");
			return -EINVAL;
	}

	return 0;
}

static int s5k5ccgx_set_mode(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	sensor->mode = value;
	dprintk(5, "s5k5ccgx_set_mode is called... mode = %d\n", sensor->mode); 
	return 0;
}

static int s5k5ccgx_set_state(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	sensor->state = value;
	dprintk(5, "s5k5ccgx_set_state is called... state = %d\n", sensor->state); 
	return 0;
}

static int s5k5ccgx_set_aewb(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_set_aewb is called...[%d]\n",value);

	sensor->aewb = value;
	switch(sensor->aewb)
	{
		case S5K5CCGX_AWB_AE_LOCK:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AE_LOCK );		
			break;
		case S5K5CCGX_AWB_AE_UNLOCK:
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AE_UNLOCK );		
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "AE/AWB value is not supported!!!\n");
			return -EINVAL;
	}

	return 0;
}


static int s5k5ccgx_set_fps(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_set_fps is called...[%d]\n",value);

	sensor->fps = value;

	if(sensor->mode == S5K5CCGX_MODE_VT)
	{
		switch(sensor->fps)
		{
			case 5:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_5_FPS);
				break;
			case 7:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_7_FPS);
				break;
			case 10:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_10_FPS);
				break;
			case 15:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_15_FPS);
				break;
			case 30:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_30_FPS);		
				break;
			default:
				printk(S5K5CCGX_MOD_NAME "%d FPS value is not supported!!!\n", sensor->fps );
				return -EINVAL;
		}
	}
	else	//(sensor->mode == S5K5CCGX_MODE_CAMCORDER || sensor->mode == S5K5CCGX_MODE_CAMERA)
	{
		switch(sensor->fps)
		{
			case 15:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AUTO15_FPS);
				break;
			case 30:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AUTO30_FPS);		
				break;
			default:
				printk(S5K5CCGX_MOD_NAME "%d FPS value is not supported!!!\n", sensor->fps );
				return -EINVAL;
		}
	}

	return 0;
}

static int s5k5ccgx_set_jpeg_quality(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

  dprintk(5, "s5k5ccgx_set_jpeg_quality is called... value : %d\n", value);
  
  if(sensor->state != S5K5CCGX_STATE_CAPTURE) 
  {
	  switch(value) 
	  {
		  case S5K5CCGX_JPEG_SUPERFINE:
			  sensor->jpeg_quality = S5K5CCGX_JPEG_SUPERFINE;
			  break;
		  case S5K5CCGX_JPEG_FINE:
			  sensor->jpeg_quality = S5K5CCGX_JPEG_FINE;
			  break;
		  case S5K5CCGX_JPEG_NORMAL:
			  sensor->jpeg_quality = S5K5CCGX_JPEG_NORMAL;
			  break;
		  default:
			  printk(S5K5CCGX_MOD_NAME "JPEG quality value is not supported!\n");
			  goto jpeg_quality_fail;
	  }
  }
  else
  {
	  switch(value) 
	  {
		  case S5K5CCGX_JPEG_SUPERFINE:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_JPEG_QUALITY_SUPERFINE );

			  break;
		  case S5K5CCGX_JPEG_FINE:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_JPEG_QUALITY_FINE );
			  break;
		  case S5K5CCGX_JPEG_NORMAL:
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_JPEG_QUALITY_NORMAL );
			  break;
		  default:
			  printk(S5K5CCGX_MOD_NAME "JPEG quality value is not supported!\n");
			  goto jpeg_quality_fail;
	  }

  }
  
  return 0;

jpeg_quality_fail:
  printk(S5K5CCGX_MOD_NAME "s5k5ccgx_set_jpeg_quality is failed!!!\n");
  return -EINVAL;    
}

static int s5k5ccgx_get_jpeg_size(struct v4l2_control *vc) 
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	struct i2c_client *client = sensor->i2c_client;
	u8 readdata[8];

	dprintk(CAM_INF, S5K5CCGX_MOD_NAME "s5k5ccgx_get_jpeg_size is called...\n"); 

	//if(s5k5ccgx_write_read_reg(client, sizeof(MakeImagesInLump_Status), MakeImagesInLump_Status, 8, readdata))
		//goto get_jpeg_size_fail;

	//vc->value = (readdata[3]<<16) + (readdata[2]<<8) + readdata[1];
	//dprintk(CAM_DBG, S5K5CCGX_MOD_NAME "s5k5ccgx_get_jpeg_size::Main JPEG Size reading... 0x%x      Q value...0x%x\n", vc->value, readdata[0]);

	return 0;

get_jpeg_size_fail:
	printk(S5K5CCGX_MOD_NAME "s5k5ccgx_get_jpeg_size is failed!!!\n");
	return -EINVAL;          
}


/*
static int s5k5ccgx_get_focus(struct v4l2_control *vc)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	struct i2c_client *client = sensor->i2c_client;	
	dprintk(5, "s5k5ccgx_get_auto_focus is called...\n"); 

	int iRetrialCount = 5;
	int ret ;
	int af_status;

	do
	{		
		s5k5ccgx_write_word(client, 0xFCFC, 0xD000);
		s5k5ccgx_write_word(client, 0x002C, 0x7000);
		s5k5ccgx_write_word(client, 0x002E, 0x0252); 
		ret = s5k5ccgx_read_word (client, 0x0F12, &af_status);		
		if ( ret == - EIO )
		{
			dprintk(5, "s5k5ccgx_get_focus() - Failed to read af_status \r\n");
			goto RETRY;
		}
		else
		{
			if ( af_status == 0 )	
			{
				vc->value = S5K5CCGX_AF_STATUS_FAIL;
			}
			else if ( af_status == 1)
			{
				vc->value = S5K5CCGX_AF_STATUS_PROGRESS;
			}
			else	// 0002~0004 : AF success
			{
				vc->value = S5K5CCGX_AF_STATUS_SUCCESS;
			}
		}
RETRY:
		mdelay(10);
	}while ( ( ret == - EIO ) && ( --iRetrialCount ) );
	dprintk(6, "AF result = %d (1.progress, 2.success, 3.fail)\n", vc->value);
	return ret;
}

static int s5k5ccgx_set_focus_status(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_set_focus_status is called...[%d]\n",value);

	if(s5k5ccgx_curr_state != S5K5CCGX_STATE_PREVIEW)
	{
		printk(S5K5CCGX_MOD_NAME "Sensor is not preview state!!");
		return -EINVAL;
	}

	switch(value) 
	{
		case S5K5CCGX_AF_START :
			dprintk(6, "AF start.\n");
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AF_DO );						
			break;
		case S5K5CCGX_AF_STOP :
			dprintk(6, "AF stop.\n");
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AF_OFF );		

			// Lenz set base position 
			if(sensor->focus_mode == S5K5CCGX_AF_MACRO) // Macro off
			{
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AF_MACRO_ON );		
			}
			else
			{
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AF_NORMAL_ON);		
			}

			// AE/AWB Unlock 
			if(sensor->aewb == S5K5CCGX_AWB_AE_UNLOCK)
			{
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AWB_AE_UNLOCK );		
			}
			else
			{
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AE_LOCK );		
			}	   

			break;
		default:
			printk(S5K5CCGX_MOD_NAME "[af]Invalid value is ordered!!!\n");
			return -EINVAL;
	}
	return 0;
}


static int s5k5ccgx_set_focus(s32 value)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "AF set value = %d\n", value);

	sensor->focus_mode = value;
	switch(sensor->focus_mode) 
	{
		case S5K5CCGX_AF_NORMAL :      
			printk( "CAM_REGSET_AF_Normal_On \n");		
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AF_NORMAL_ON );			
			break;

		case S5K5CCGX_AF_MACRO : 
			printk( "CAM_REGSET_AF_Macro_On \n");		
			CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_AF_MACRO_ON );						
			break;

		default:
			printk(S5K5CCGX_MOD_NAME "[af]Invalid value is ordered!!!\n");
			return -EINVAL;   
	}
	return 0;
}
*/

static void s5k5ccgx_esd_check(struct work_struct *work)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	struct i2c_client *client = sensor->i2c_client;	
	int ret;
	u16 esd_check ;
	dprintk(5, "s5k5ccgx_esd_check() is called...\n");

	// check preview mode
	if(s5k5ccgx_curr_state != S5K5CCGX_STATE_PREVIEW)
	{
		cancel_rearming_delayed_work(&camera_esd_check);
		return;
	}

	// check 0x032C for every 500 msec, check the value 0xAAAA
	// if not, reset preview
	s5k5ccgx_write_word(client, 0xFCFC, 0xD000);
	s5k5ccgx_write_word(client, 0x002C, 0x7000);
	s5k5ccgx_write_word(client, 0x002E, 0x032C); 
	ret = s5k5ccgx_read_word(client, 0x0F12, &esd_check);
	if ( esd_check != 0xAAAA )
	{
		dprintk(5, "restart preview ################..\n");		
		// restart preview
		CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_INIT );
		mdelay(100);		
		CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_PREVIEW );		
	}

	/* Schedule next poll */
	schedule_delayed_work(&camera_esd_check, msecs_to_jiffies(500));	
	return;
}


static int s5k5ccgx_start_preview(void)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	dprintk(5, "s5k5ccgx_start_preview() is called...\n");

	//CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_INIT );

	/* Preview Start */
	//mdelay(100);

	CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_PREVIEW );

	//INIT_DELAYED_WORK(&camera_esd_check, s5k5ccgx_esd_check);
	//schedule_delayed_work(&camera_esd_check, msecs_to_jiffies(500));

	return 0;
}

static int s5k5ccgx_stop_preview(void)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_stop_preview is called...\n");

	cancel_rearming_delayed_work(&camera_esd_check);

	s5k5ccgx_pre_state = s5k5ccgx_curr_state;
	s5k5ccgx_curr_state = S5K5CCGX_STATE_INVALID;  

	return 0;
}

static int s5k5ccgx_set_preview(void)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_set_preview is called...%d\n", sensor->preview_size);

	s5k5ccgx_pre_state = s5k5ccgx_curr_state;
	s5k5ccgx_curr_state = S5K5CCGX_STATE_PREVIEW;  

	// 1. Preview Size Setting
	switch(sensor->preview_size)
	{
		case S5K5CCGX_PREVIEW_SIZE_176_144: //176 X 144 (0)
			break;
		case S5K5CCGX_PREVIEW_SIZE_320_240: //320 X 240 (0)
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_PREVIEW_SIZE_320);
			break;
		case S5K5CCGX_PREVIEW_SIZE_640_480: //640 X 480	 (0)
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_PREVIEW_SIZE_640);
			break;
		case S5K5CCGX_PREVIEW_SIZE_720_480: //720 X 480 (X)
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_PREVIEW_SIZE_720);
			break;
		case S5K5CCGX_PREVIEW_SIZE_800_480: //800 X 480 (X)
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_PREVIEW_SIZE_800);
			break;
		case S5K5CCGX_PREVIEW_SIZE_1024_768: //1024 X 768 (0)
			break;
		case S5K5CCGX_PREVIEW_SIZE_1280_720: //1280 X 720 (X)
			break;

		default:
			/* When running in image capture mode, the call comes here.
			 * Set the default video resolution - S5K5CCGX_PREVIEW_VGA
			 */ 
			dprintk(5, "Setting preview resoution as VGA for image capture mode\n");
			break;
	}


	// 2. is DTP on?
	/*
	if(state->check_dataline)
	{
		dprintk(5, "DTP ON\n");
		CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_DTP_ON);
		return 0;
	}
	*/


	// 3. Camera Config 
	
	//if(sensor->fps != S5K5CCGX_FPS_DEFAULT)
		//s5k5ccgx_set_fps(sensor->fps);

	if(sensor->scene != S5K5CCGX_SCENE_OFF)
	{
		s5k5ccgx_set_scene(sensor->scene);
	}
	else // scene mode off
	{
		// Effect
		if(sensor->effect != S5K5CCGX_EFFECT_DEFAULT) 
			s5k5ccgx_set_effect(sensor->effect);

		// White Balance
		if(sensor->wb != S5K5CCGX_WB_DEFAULT)
			s5k5ccgx_set_wb(sensor->wb);

		// ISO
		if(sensor->iso != S5K5CCGX_ISO_DEFAULT)
			s5k5ccgx_set_iso(sensor->iso);

		// Metering 
		if(sensor->photometry != S5K5CCGX_PHOTOMETRY_DEFAULT)
			s5k5ccgx_set_photometry(sensor->photometry);

		// Adjust
		if(sensor->contrast != S5K5CCGX_CONTRAST_DEFAULT)
			s5k5ccgx_set_contrast(sensor->contrast);
		if(sensor->sharpness != S5K5CCGX_SHARPNESS_DEFAULT)
			s5k5ccgx_set_sharpness(sensor->sharpness);
		if(sensor->saturation != S5K5CCGX_SATURATION_DEFAULT)
			s5k5ccgx_set_saturation(sensor->saturation);

		// EV
		if(sensor->ev != S5K5CCGX_EV_DEFAULT)
			s5k5ccgx_set_ev(sensor->ev);
	}	

	// Capture Size Config


	return 0;
}

static int s5k5ccgx_start_capture(void)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;
	struct v4l2_pix_format* pix = &sensor->pix;
	camera_light_status_type illuminance;

	dprintk(5, "s5k5ccgx_start_capture is called...\n");  


	/* Data Output Setting */
	if(pix->pixelformat == V4L2_PIX_FMT_UYVY || pix->pixelformat == V4L2_PIX_FMT_YUYV || pix->pixelformat == V4L2_PIX_FMT_JPEG)
	{
		// Nightshot or Firework Mode
		if(sensor->scene == S5K5CCGX_SCENE_NIGHT || sensor->scene == S5K5CCGX_SCENE_FIRE)
		{
				CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_NIGHT_SNAPSHOT );		
		}
		else	// Normal Capture
		{
			illuminance = S5K5CCGX_check_illuminance_status();
			switch (illuminance)
			{
				case CAMERA_SENSOR_LIGHT_STATUS_NORMAL:				
					CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_NORMAL_SNAPSHOT );		
					break;
				case CAMERA_SENSOR_LIGHT_STATUS_LOW:
					CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_LOWLIGHT_SNAPSHOT);	
					break;
				case CAMERA_SENSOR_LIGHT_STATUS_HIGH:
					CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_HIGH_SNAPSHOT);	
					break;
				default:
					CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_NORMAL_SNAPSHOT );
					break;
			}				
		}
	}
	else
	{
		printk(S5K5CCGX_MOD_NAME "[start capture] Invalid pixelformat is ordered!!!\n");
		return - EINVAL;
	}

	return 0;
}

static int s5k5ccgx_stop_capture(void)
{
	dprintk(5, "s5k5ccgx_stop_capture is called...\n");

	s5k5ccgx_pre_state = s5k5ccgx_curr_state;
	s5k5ccgx_curr_state = S5K5CCGX_STATE_INVALID;  

	return 0;
}

static int s5k5ccgx_set_capture(void)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_set_capture is called... %d\n", sensor->capture_size);

	s5k5ccgx_pre_state = s5k5ccgx_curr_state;
	s5k5ccgx_curr_state = S5K5CCGX_STATE_CAPTURE;
	
	switch(sensor->capture_size)
	{
		case S5K5CCGX_CAPTURE_SIZE_640_480: /* 640x480 */ 
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_CAPTURE_SIZE_640);
			break;
		case S5K5CCGX_CAPTURE_SIZE_800_480: /* 800x480 */
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_CAPTURE_SIZE_800W);
			break;
		case S5K5CCGX_CAPTURE_SIZE_1600_960: /* 1600x960 */
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_CAPTURE_SIZE_1600W);
			break;
		case S5K5CCGX_CAPTURE_SIZE_1600_1200: /* 1600x1200 */
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_CAPTURE_SIZE_1600);
			break;
		case S5K5CCGX_CAPTURE_SIZE_2048_1232: /* 2048x1232 */
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_CAPTURE_SIZE_2048W);
			break;
		case S5K5CCGX_CAPTURE_SIZE_2048_1536: /* 2048x1536 */
			CAMSENSOR_WRITE_REGSETS(CAM_REG32SET_CAPTURE_SIZE_2048);
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "[capture]Invalid value is ordered!!!\n");
			/* The framesize index was not set properly. 
			 * Check s_fmt call - it must be for video mode. */
			return -EINVAL;
	}

	// JPEG Quality Config
	//s5k5ccgx_set_jpeg_quality(sensor->jpeg_quality);

	return 0;
}

static void s5k5ccgx_set_skip(void)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	int skip_frame = 0; //snapshot

	dprintk(5, "s5k5ccgx_set_skip is called...\n");

	if(sensor->state == S5K5CCGX_STATE_PREVIEW)
	{      
		if(sensor->fps > 30)
		{
			skip_frame = sensor->fps / 5;
		}
		else if(sensor->fps > 15)
		{
			skip_frame = sensor->fps / 4; 
		}
		else if(sensor->fps > 7)
		{
			skip_frame = sensor->fps / 3; 
		}
		else
		{
			skip_frame = sensor->fps / 2; 
		}
	}

	dprintk(5, "skip frame = %d frame\n", skip_frame);

	isp_set_hs_vs(0, skip_frame);
}

static int ioctl_streamoff(struct v4l2_int_device *s)
{
	struct s5k5ccgx_sensor *sensor = s->priv;  
	int err = 0;

	dprintk(5, "ioctl_streamoff is called...\n");
	if(sensor->state != S5K5CCGX_STATE_CAPTURE)
	{
		dprintk(6, "s5k5ccgx_stop_preview....................\n");
		err = s5k5ccgx_stop_preview();
	}
	else
	{
		dprintk(6, "s5k5ccgx_stop_capture....................\n");
		err = s5k5ccgx_stop_capture();
	}

	return err;
}

static int ioctl_streamon(struct v4l2_int_device *s)
{
	struct s5k5ccgx_sensor *sensor = s->priv;  
	int err = 0;

	dprintk(5, "ioctl_streamon is called...(%x)\n", sensor->state);  

	if(sensor->state != S5K5CCGX_STATE_CAPTURE)
	{
		dprintk(6, "start preview....................\n");
		err = s5k5ccgx_start_preview();
	}
	else
	{
		dprintk(6, "start capture....................\n");
		err = s5k5ccgx_start_capture();
	}

	return err;
}

/**
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the S5K5CCGX_ctrl_list[] array.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s,
		struct v4l2_queryctrl *qc)
{
	int i;

	dprintk(5, "ioctl_queryctrl is called...\n");

	for (i = 0; i < NUM_S5K5CCGX_CONTROL; i++) 
	{
		if (qc->id == S5K5CCGX_ctrl_list[i].id)
		{
			break;
		}
	}
	if (i == NUM_S5K5CCGX_CONTROL)
	{
		printk(S5K5CCGX_MOD_NAME "Control ID is not supported!!\n");
		qc->flags = V4L2_CTRL_FLAG_DISABLED;

		return -EINVAL;
	}

	*qc = S5K5CCGX_ctrl_list[i];

	return 0;
}





/**
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the ce13 sensor struct.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct s5k5ccgx_sensor *sensor = s->priv;

	int retval = 0;

	dprintk(5, "ioctl_g_ctrl is called...(%d)\n", vc->id);

	switch (vc->id) 
	{
		case V4L2_CID_SELECT_MODE:
			vc->value = sensor->mode;
			break;  
		case V4L2_CID_SELECT_STATE:
			vc->value = sensor->state;
			break;       
		case V4L2_CID_FOCUS_MODE:
			//vc->value = sensor->focus_mode;
			break;  
		case V4L2_CID_AF:
			//retval = s5k5ccgx_get_focus(vc);
			vc->value = 2;
			break;
		case V4L2_CID_ISO:
			vc->value = sensor->iso;
			break;
		case V4L2_CID_BRIGHTNESS:
			vc->value = sensor->ev;
			break;
		case V4L2_CID_WB:
			vc->value = sensor->wb;
			break;      
		case V4L2_CID_EFFECT:
			vc->value = sensor->effect;
			break;
		case V4L2_CID_CONTRAST:
			vc->value = sensor->contrast;
			break;
		case V4L2_CID_SATURATION:
			vc->value = sensor->saturation;
			break;
		case V4L2_CID_SHARPNESS:
			vc->value = sensor->sharpness;
			break;
		case V4L2_CID_PHOTOMETRY:
			vc->value = sensor->photometry;
			break;
		case V4L2_CID_AEWB:
			vc->value = sensor->aewb;
			break;
		case V4L2_CID_SCENE:
			retval = s5k5ccgx_get_scene(vc);
			break;      
		case V4L2_CID_JPEG_SIZE:
			retval = s5k5ccgx_get_jpeg_size(vc);
			break;
		case V4L2_CID_FW_YUV_OFFSET:
			vc->value = sensor->yuv_offset;
			break;      
		case V4L2_CID_JPEG_QUALITY:
			vc->value = sensor->jpeg_quality;
			dprintk(5, "V4L2_CID_CAM_JPEG_QUALITY: %d\n", vc->value);
			break;
		case V4L2_CID_JPEG_CAPTURE_WIDTH:
			vc->value = sensor->jpeg_capture_w;
			break; 
		case V4L2_CID_JPEG_CAPTURE_HEIGHT:
			vc->value = sensor->jpeg_capture_h;
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}

	return retval;
}




/**
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the S5K5CCGX sensor struct).
 * Otherwise, * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct s5k5ccgx_sensor *sensor = s->priv;
	int retval = 0;

	dprintk(5, "ioctl_s_ctrl is called...(%d)\n", vc->id);

	switch (vc->id) 
	{
		case V4L2_CID_SELECT_MODE:
			retval = s5k5ccgx_set_mode(vc->value);
			break;  
		case V4L2_CID_SELECT_STATE:
			retval = s5k5ccgx_set_state(vc->value);
			break;       
		case V4L2_CID_FOCUS_MODE:
			//retval = s5k5ccgx_set_focus(vc->value);
			break;
		case V4L2_CID_AF:
			//retval = s5k5ccgx_set_focus_status(vc->value);
			break;
		case V4L2_CID_ISO:
			retval = s5k5ccgx_set_iso(vc->value);
			break;
		case V4L2_CID_BRIGHTNESS:
			retval = s5k5ccgx_set_ev(vc->value);
			break;
		case V4L2_CID_WB:
			retval = s5k5ccgx_set_wb(vc->value);
			break;
		case V4L2_CID_EFFECT:
			retval = s5k5ccgx_set_effect(vc->value);
			break;
		case V4L2_CID_CONTRAST:
			retval = s5k5ccgx_set_contrast(vc->value);
			break;
		case V4L2_CID_SATURATION:
			retval = s5k5ccgx_set_saturation(vc->value);
			break;
		case V4L2_CID_SHARPNESS:
			retval = s5k5ccgx_set_sharpness(vc->value);
			break;			
		case V4L2_CID_SCENE:
			retval = s5k5ccgx_set_scene(vc->value);
			break;
		case V4L2_CID_PHOTOMETRY:
			retval = s5k5ccgx_set_photometry(vc->value);
			break;
		case V4L2_CID_AEWB:
			retval = s5k5ccgx_set_aewb(vc->value);
			break;
		case V4L2_CID_JPEG_QUALITY:
			retval = s5k5ccgx_set_jpeg_quality(vc->value);
			break;
		default:
			printk(S5K5CCGX_MOD_NAME "[id]Invalid value is ordered!!!\n");
			break;
	}

	return retval;
}


/**
 * ioctl_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s, struct v4l2_fmtdesc *fmt)
{
	int index = 0;

	dprintk(5, "ioctl_enum_fmt_cap is called...\n");

	switch (fmt->type) 
	{
		case V4L2_BUF_TYPE_VIDEO_CAPTURE:
			switch(fmt->pixelformat)
			{
				case V4L2_PIX_FMT_UYVY:
					index = 0;
					break;

				case V4L2_PIX_FMT_YUYV:
					index = 1;
					break;

				case V4L2_PIX_FMT_JPEG:
					index = 2;
					break;

				default:
					printk(S5K5CCGX_MOD_NAME "[format]Invalid value is ordered!!!\n");
					return -EINVAL;
			}
			break;

		default:
			printk(S5K5CCGX_MOD_NAME "[type]Invalid value is ordered!!!\n");
			return -EINVAL;
	}

	fmt->flags = S5K5CCGX_formats[index].flags;
	fmt->pixelformat = S5K5CCGX_formats[index].pixelformat;
	strlcpy(fmt->description, S5K5CCGX_formats[index].description, sizeof(fmt->description));

	dprintk(6, "ioctl_enum_fmt_cap flag : %d\n", fmt->flags);
	dprintk(6, "ioctl_enum_fmt_cap description : %s\n", fmt->description);

	return 0;
}

/**
 * ioctl_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */
static int ioctl_try_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct s5k5ccgx_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix2 = &sensor->pix;

	int err = 0;
	int index = 0;

	dprintk(5, "ioctl_try_fmt_cap is called...\n");
	dprintk(6, "ioctl_try_fmt_cap. mode : %d\n", sensor->mode);
	dprintk(6, "ioctl_try_fmt_cap. state : %d\n", sensor->state);
	dprintk(6, "pix->width : %d, pix->height : %d\n", pix->width,pix->height);

	/*
	if(pix->pixelformat == V4L2_PIX_FMT_JPEG && pix->colorspace != V4L2_COLORSPACE_JPEG)
	{
		dprintk(6, "Error : mismatch in pixelformat and colorspace(line:%d)\n", __LINE__);
		return -EINVAL;
	}
	*/

	s5k5ccgx_set_skip();  

	if(sensor->state == S5K5CCGX_STATE_CAPTURE)
	{ 
		for(index = 0; index < ARRAY_SIZE(s5k5ccgx_image_sizes); index++)
		{
			if(s5k5ccgx_image_sizes[index].width == pix->width && s5k5ccgx_image_sizes[index].height == pix->height)
			{
				sensor->capture_size = index;
				break;
			}
		}   

		if(index == ARRAY_SIZE(s5k5ccgx_image_sizes))
		{
			printk(S5K5CCGX_MOD_NAME "Capture Image Size is not supported!\n");
			return -EINVAL;
		}

		dprintk(6, "S5K5CCGX--capture size = %d\n", sensor->capture_size);  
		dprintk(6, "S5K5CCGX--capture width : %d\n", s5k5ccgx_image_sizes[index].width);
		dprintk(6, "S5K5CCGX--capture height : %d\n", s5k5ccgx_image_sizes[index].height);      

		if(pix->pixelformat == V4L2_PIX_FMT_UYVY || pix->pixelformat == V4L2_PIX_FMT_YUYV)
		{
			pix->field = V4L2_FIELD_NONE;
			pix->bytesperline = pix->width * 2;
			pix->sizeimage = pix->bytesperline * pix->height;
			dprintk(6, "V4L2_PIX_FMT_UYVY\n");
		}
		else
		{
			pix->field = V4L2_FIELD_NONE;
			pix->bytesperline = JPEG_CAPTURE_WIDTH * 2;
			pix->sizeimage = pix->bytesperline * JPEG_CAPTURE_HEIGHT;
			dprintk(6, "V4L2_PIX_FMT_JPEG\n");
		}

		dprintk(6, "set capture....................\n");
		err = s5k5ccgx_set_capture();    
	}  
	else
	{  
		for(index = 0; index < ARRAY_SIZE(s5k5ccgx_preview_sizes); index++)
		{
			if(s5k5ccgx_preview_sizes[index].width == pix->width && s5k5ccgx_preview_sizes[index].height == pix->height)
			{
				sensor->preview_size = index;
				break;
			}
		}   

		if(index == ARRAY_SIZE(s5k5ccgx_preview_sizes))
		{
			printk(S5K5CCGX_MOD_NAME "Preview Image Size is not supported!\n");
			return -EINVAL;
		}

		if(sensor->mode == S5K5CCGX_MODE_CAMCORDER)
		{
			if(pix->width == 1280 && pix->height == 720)
			{
				dprintk(6, "Preview Image Size is 720P!\n");
			}
		}

		dprintk(6, "S5K5CCGX--preview size = %d\n", sensor->preview_size); 
		dprintk(6, "S5K5CCGX--preview width : %d\n", s5k5ccgx_preview_sizes[index].width);
		dprintk(6, "S5K5CCGX--preview height : %d\n", s5k5ccgx_preview_sizes[index].height);      

		pix->field = V4L2_FIELD_NONE;
		pix->bytesperline = pix->width * 2;
		pix->sizeimage = pix->bytesperline * pix->height;  
		dprintk(6, "V4L2_PIX_FMT_VYUY\n");

		dprintk(6, "set preview....................\n");     
		err = s5k5ccgx_set_preview();
	}      

	switch (pix->pixelformat) 
	{
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_JPEG:
			pix->colorspace = V4L2_COLORSPACE_JPEG;
			break;
		case V4L2_PIX_FMT_RGB565:
		case V4L2_PIX_FMT_RGB565X:
		case V4L2_PIX_FMT_RGB555:
		case V4L2_PIX_FMT_SGRBG10:
		case V4L2_PIX_FMT_RGB555X:
		default:
			pix->colorspace = V4L2_COLORSPACE_SRGB;
			break;
	}

	*pix2 = *pix;

	return err;
}

/**
 * ioctl_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
static int ioctl_s_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	int err = 0;

	dprintk(5, "ioctl_s_fmt_cap called...\n");

	err = ioctl_try_fmt_cap(s, f);

	return err;
}

/**
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct s5k5ccgx_sensor *sensor = s->priv;

	dprintk(5, "ioctl_g_fmt_cap is called...\n");

	f->fmt.pix = sensor->pix;

	return 0;
}

/**
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct s5k5ccgx_sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	dprintk(5, "ioctl_g_parm is called...\n");

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
	{
		printk(S5K5CCGX_MOD_NAME "ioctl_g_parm type not supported.\n");
		return -EINVAL;
	}

	memset(a, 0, sizeof(*a));
	a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe = sensor->timeperframe;
	return 0;
}

/**
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct s5k5ccgx_sensor *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;

	dprintk(5, "ioctl_s_parm is called...\n");

	/* Set mode (camera/camcorder/vt) & state (preview/capture) */
	sensor->mode = a->parm.capture.capturemode;
	sensor->state = a->parm.capture.currentstate;
	dprintk(6, "mode = %d, state = %d\n", sensor->mode, sensor->state);   

	/* Set time per frame (FPS) */
	if((timeperframe->numerator == 0)&&(timeperframe->denominator == 0))
	{
		sensor->fps = 30;
	}
	else
	{
		sensor->fps = timeperframe->denominator / timeperframe->numerator;
	}

	sensor->timeperframe = *timeperframe;
	dprintk(6, "fps = %d\n", sensor->fps);  
	dprintk(6, "numerator : %d, denominator: %d\n", timeperframe->numerator, timeperframe->denominator);

	return 0;
}

/**
 * ioctl_g_ifparm - V4L2 sensor interface handler for vidioc_int_g_ifparm_num
 * @s: pointer to standard V4L2 device structure
 * @p: pointer to standard V4L2 vidioc_int_g_ifparm_num ioctl structure
 *
 * Gets slave interface parameters.
 * Calculates the required xclk value to support the requested
 * clock parameters in p.  This value is returned in the p
 * parameter.
 */
static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	struct s5k5ccgx_sensor *sensor = s->priv;
	int rval;

	dprintk(5, "ioctl_g_ifparm is called...\n");

	rval = sensor->pdata->ifparm(p);
	if (rval)
	{
		return rval;
	}

	p->u.bt656.clock_curr = S5K5CCGX_XCLK;

	return 0;
}

/**
 * ioctl_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int ioctl_g_priv(struct v4l2_int_device *s, void *p)
{
	struct s5k5ccgx_sensor *sensor = s->priv;

	dprintk(5, "ioctl_g_priv is called...\n");

	return sensor->pdata->priv_data_set(p);
}


/* added following functins for v4l2 compatibility with omap34xxcam */

/**
 * ioctl_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 **/
static int ioctl_enum_framesizes(struct v4l2_int_device *s, struct v4l2_frmsizeenum *frms)
{
	struct s5k5ccgx_sensor* sensor = s->priv;

	dprintk(5, "ioctl_enum_framesizes called...\n");   

	if (sensor->state == S5K5CCGX_STATE_CAPTURE)
	{    
		dprintk(6, "Size enumeration for image capture size = %d\n", sensor->capture_size);

		frms->index = sensor->capture_size;
		frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		frms->discrete.width = s5k5ccgx_image_sizes[sensor->capture_size].width;
		frms->discrete.height = s5k5ccgx_image_sizes[sensor->capture_size].height;        
	}
	else
	{
		dprintk(6, "Size enumeration for image preview size = %d\n", sensor->preview_size);

		frms->index = sensor->preview_size;
		frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		frms->discrete.width = s5k5ccgx_preview_sizes[sensor->preview_size].width;
		frms->discrete.height = s5k5ccgx_preview_sizes[sensor->preview_size].height;        
	}

	dprintk(6, "framesizes width : %d\n", frms->discrete.width); 
	dprintk(6, "framesizes height : %d\n", frms->discrete.height); 

	return 0;
}

static int ioctl_enum_frameintervals(struct v4l2_int_device *s, struct v4l2_frmivalenum *frmi)
{
	dprintk(5, "ioctl_enum_frameintervals \n"); 
	dprintk(6, "ioctl_enum_frameintervals numerator : %d\n", frmi->discrete.numerator); 
	dprintk(6, "ioctl_enum_frameintervals denominator : %d\n", frmi->discrete.denominator); 

	return 0;
}


/**
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int ioctl_s_power(struct v4l2_int_device *s, enum v4l2_power on)
{
	struct s5k5ccgx_sensor *sensor = s->priv;
	struct i2c_client *client = sensor->i2c_client;
	int err = 0;

	dprintk(5, "ioctl_s_power is called......ON=%x, detect= %x\n", on, sensor->detect);

	sensor->pdata->power_set(on);

	switch(on)
	{
		case V4L2_POWER_ON:
			{
				dprintk(6, "pwr on-----!\n");
				err = s5k5ccgx_detect(client);
				if (err) 
				{
					printk(S5K5CCGX_MOD_NAME "Unable to detect " S5K5CCGX_DRIVER_NAME " sensor\n");
					sensor->pdata->power_set(V4L2_POWER_OFF);
					return err;
				}

				/* Make the default detect */
				sensor->detect = SENSOR_DETECTED;     

				/* Make the state init */
				s5k5ccgx_curr_state = S5K5CCGX_STATE_INVALID;
			}
			break;

		case V4L2_POWER_RESUME:
			{
				dprintk(6, "pwr resume-----!\n");
			}  
			break;

		case V4L2_POWER_STANDBY:
			{
				dprintk(6, "pwr stanby-----!\n");
			}
			break;

		case V4L2_POWER_OFF:
			{
				dprintk(6, "pwr off-----!\n");

				/* Make the default detect */
				sensor->detect = SENSOR_NOT_DETECTED;  

				/* Make the state init */
				s5k5ccgx_pre_state = S5K5CCGX_STATE_INVALID;
#ifdef USE_SD_CARD_TUNE
				UnLoad32BitTuningFile();
#endif
			}
			break;
	}

	return err;
}

static int ioctl_g_exif(struct v4l2_int_device *s, struct v4l2_exif *exif)
{
	struct s5k5ccgx_sensor *sensor = s->priv;
	struct i2c_client *client = sensor->i2c_client;

	return 0;

}

/**
 * ioctl_deinit - V4L2 sensor interface handler for VIDIOC_INT_DEINIT
 * @s: pointer to standard V4L2 device structure
 *
 * Deinitialize the sensor device
 */
static int ioctl_deinit(struct v4l2_int_device *s)
{
	struct s5k5ccgx_sensor *sensor = s->priv;

	dprintk(5, "ioctl_deinit is called...\n");

	sensor->state = S5K5CCGX_STATE_INVALID; //init problem

	return 0;
}


/**
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 *
 * Initialize the sensor device (call S5K5CCGX_configure())
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	struct s5k5ccgx_sensor *sensor = s->priv;

	dprintk(5, "ioctl_init is called...\n");

	sensor->timeperframe.numerator    = 1;
	sensor->timeperframe.denominator  = 30;
	sensor->fps                       = 30;
	//sensor->bv                        = 0;
	sensor->state                     = S5K5CCGX_STATE_INVALID;
	sensor->mode                      = S5K5CCGX_MODE_CAMERA;
	sensor->preview_size              = S5K5CCGX_PREVIEW_SIZE_640_480;
	sensor->capture_size              = S5K5CCGX_CAPTURE_SIZE_2048_1536;
	sensor->detect                    = SENSOR_NOT_DETECTED;
	//sensor->focus_mode                = S5K5CCGX_AF_INIT_NORMAL;
	sensor->effect                    = S5K5CCGX_EFFECT_OFF;
	sensor->iso                       = S5K5CCGX_ISO_AUTO;
	sensor->photometry                = S5K5CCGX_PHOTOMETRY_CENTER;
	sensor->ev                        = S5K5CCGX_EV_DEFAULT;
	//sensor->wdr                       = S5K5CCGX_WDR_OFF;
	sensor->contrast                  = S5K5CCGX_CONTRAST_DEFAULT;
	sensor->saturation                = S5K5CCGX_SATURATION_DEFAULT;
	sensor->sharpness                 = S5K5CCGX_SHARPNESS_DEFAULT;
	sensor->wb                        = S5K5CCGX_WB_AUTO;
	//sensor->isc                       = S5K5CCGX_ISC_STILL_OFF;
	sensor->scene                     = S5K5CCGX_SCENE_OFF;
	sensor->aewb                      = S5K5CCGX_AWB_AE_UNLOCK;
	//sensor->antishake                 = S5K5CCGX_ANTI_SHAKE_OFF;
	//sensor->flash_capture             = S5K5CCGX_FLASH_CAPTURE_OFF;
	//sensor->flash_movie               = S5K5CCGX_FLASH_MOVIE_OFF;
	sensor->jpeg_quality              = S5K5CCGX_JPEG_SUPERFINE;
	//sensor->zoom                      = S5K5CCGX_ZOOM_1P00X;
	sensor->thumb_offset              = S5K5CCGX_THUMBNAIL_OFFSET;
	sensor->yuv_offset                = S5K5CCGX_YUV_OFFSET;
	sensor->jpeg_capture_w            = JPEG_CAPTURE_WIDTH;
	sensor->jpeg_capture_h            = JPEG_CAPTURE_HEIGHT;  

	LoadCamsensorRegSettings();

	CAMSENSOR_WRITE_REGSETS( CAM_REG32SET_INIT );
	mdelay(100);

	// chaneg sensor state to RUNMODE. 
	// In RUNMODE, if setting should affect directly. 
	// In INVALID MODE, setting should affect after initilize sensor. 

	return 0;
}

static struct v4l2_int_ioctl_desc S5K5CCGX_ioctl_desc[] = {
	{ .num = vidioc_int_enum_framesizes_num,
		.func = (v4l2_int_ioctl_func *)ioctl_enum_framesizes},
	{ .num = vidioc_int_enum_frameintervals_num,
		.func = (v4l2_int_ioctl_func *)ioctl_enum_frameintervals},
	{ .num = vidioc_int_s_power_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_power },
	{ .num = vidioc_int_g_priv_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_priv },
	{ .num = vidioc_int_g_ifparm_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_ifparm },
	{ .num = vidioc_int_init_num,
		.func = (v4l2_int_ioctl_func *)ioctl_init },
	{ .num = vidioc_int_deinit_num,
		.func = (v4l2_int_ioctl_func *)ioctl_deinit },
	{ .num = vidioc_int_enum_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_enum_fmt_cap },
	{ .num = vidioc_int_try_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_try_fmt_cap },
	{ .num = vidioc_int_g_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_fmt_cap },
	{ .num = vidioc_int_s_fmt_cap_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_fmt_cap },
	{ .num = vidioc_int_g_parm_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_parm },
	{ .num = vidioc_int_s_parm_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_parm },
	{ .num = vidioc_int_queryctrl_num,
		.func = (v4l2_int_ioctl_func *)ioctl_queryctrl },
	{ .num = vidioc_int_g_ctrl_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_ctrl },
	{ .num = vidioc_int_s_ctrl_num,
		.func = (v4l2_int_ioctl_func *)ioctl_s_ctrl },
	{ .num = vidioc_int_streamon_num,
		.func = (v4l2_int_ioctl_func *)ioctl_streamon },
	{ .num = vidioc_int_streamoff_num,
		.func = (v4l2_int_ioctl_func *)ioctl_streamoff },
	{ .num = vidioc_int_g_exif_num,
		.func = (v4l2_int_ioctl_func *)ioctl_g_exif },    
};

static struct v4l2_int_slave S5K5CCGX_slave = {
	.ioctls = S5K5CCGX_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(S5K5CCGX_ioctl_desc),
};

static struct v4l2_int_device S5K5CCGX_int_device = {
	.module = THIS_MODULE,
	.name = S5K5CCGX_DRIVER_NAME,
	.priv = &S5K5CCGX,
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &S5K5CCGX_slave,
	},
};


/**
 * s5k5ccgx_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * device.
 */
static int s5k5ccgx_probe(struct i2c_client *client, const struct i2c_device_id *device)
{
	struct s5k5ccgx_sensor *sensor = &S5K5CCGX;

	dprintk(5, "s5k5ccgx_probe is called...\n");

	if (i2c_get_clientdata(client))
	{
		printk(S5K5CCGX_MOD_NAME "can't get i2c client data!!\n");
		return -EBUSY;
	}

	sensor->pdata = &s5k5ccgx_platform_data0;

	if (!sensor->pdata) 
	{
		printk(S5K5CCGX_MOD_NAME "no platform data!!\n");
		return -ENODEV;
	}

	sensor->v4l2_int_device = &S5K5CCGX_int_device;
	sensor->i2c_client = client;

	/* Make the default capture size */
	sensor->pix.width = 2048;
	sensor->pix.height = 1536;


	/* Make the default capture format V4L2_PIX_FMT_UYVY */
	sensor->pix.pixelformat = V4L2_PIX_FMT_JPEG;

	i2c_set_clientdata(client, sensor);

	if (v4l2_int_device_register(sensor->v4l2_int_device))
	{
		printk(S5K5CCGX_MOD_NAME "fail to init device register \n");
		i2c_set_clientdata(client, NULL);
	}
	return 0;
}

/**
 * s5k5ccgx_remove - sensor driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister sensor as an i2c client device and V4L2
 * device.  Complement of s5k5ccgx_probe().
 */
static int __exit s5k5ccgx_remove(struct i2c_client *client)
{
	struct s5k5ccgx_sensor *sensor = i2c_get_clientdata(client);

	dprintk(5, "s5k5ccgx_remove is called...\n");

	if (!client->adapter)
	{
		printk(S5K5CCGX_MOD_NAME "no i2c client adapter!!");
		return -ENODEV; /* our client isn't attached */
	}

	v4l2_int_device_unregister(sensor->v4l2_int_device);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id S5K5CCGX_id[] = {
	{ S5K5CCGX_DRIVER_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, S5K5CCGX_id);


static struct i2c_driver s5k5ccgx_i2c_driver = {
	.driver = {
		.name = S5K5CCGX_DRIVER_NAME,
	},
	.probe = s5k5ccgx_probe,
	.remove = __exit_p(s5k5ccgx_remove),
	.id_table = S5K5CCGX_id,
};

/**
 * S5K5CCGX_sensor_init - sensor driver module_init handler
 *
 * Registers driver as an i2c client driver.  Returns 0 on success,
 * error code otherwise.
 */
static int __init S5K5CCGX_sensor_init(void)
{
	int err;

	dprintk(5, "S5K5CCGX_sensor_init is called...\n");

	err = i2c_add_driver(&s5k5ccgx_i2c_driver);
	if (err) 
	{
		printk(S5K5CCGX_MOD_NAME "Failed to register" S5K5CCGX_DRIVER_NAME ".\n");
		return err;
	}
	return 0;
}

module_init(S5K5CCGX_sensor_init);

/**
 * S5K5CCGXsensor_cleanup - sensor driver module_exit handler
 *
 * Unregisters/deletes driver as an i2c client driver.
 * Complement of S5K5CCGX_sensor_init.
 */
static void __exit S5K5CCGXsensor_cleanup(void)
{
	i2c_del_driver(&s5k5ccgx_i2c_driver);
}
module_exit(S5K5CCGXsensor_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("S5K5CCGX camera sensor driver");
