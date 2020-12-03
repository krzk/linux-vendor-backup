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
 * modules/camera/SR030PC40.c
 *
 * SR030PC40 sensor driver source file
 *
 * Modified by paladin in Samsung Electronics
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>

#include <mach/gpio.h>
#include <mach/hardware.h>

#include <media/v4l2-int-device.h>

#include "isp/isp.h"
#include "omap34xxcam.h"
#include "sr030pc40.h"
#include "sr030pc40_tune.h"
#include "Camsensor_tunner_16bits_reg.h"

//#define SR030PC40_USE_GPIO_I2C
#if defined(SR030PC40_USE_GPIO_I2C)
#undef	SR030PC40_USE_GPIO_I2C
#endif

#if !defined(SR030PC40_USE_GPIO_I2C)
#define CAM_I2C_CLIENT	struct i2c_client
#else 
#define CAM_I2C_CLIENT	OMAP_GPIO_I2C_CLIENT
#endif

#if defined(SR030PC40_USE_GPIO_I2C)
#include <plat/i2c-omap-gpio.h>
static struct i2c_client *dummy_i2c_client;
#endif

#if (CAM_SR030PC40_DBG_MSG)
//#include "dprintk.h"
#define dprintk(level, fmt, arg...) do {		\
	if (debug >= level)				\
	printk(SR030PC40_MOD_NAME fmt , ## arg); } while (0)
#else
#define dprintk(x, y...)
#endif

#define CAMSENSOR_REGSET_INITIALIZE(ArrReg, SR030PC40ArrReg) \
{\
	ArrReg.reg16 = SR030PC40ArrReg;			\
	ArrReg.num = ARRAY_SIZE(SR030PC40ArrReg);	\
	ArrReg.nDynamicLoading = 0;			\
}

static CAM_I2C_CLIENT *cam_i2c_client;

static int dtp_on= 0;
module_param(dtp_on, int, 0644);

static int enable_sdcard_tune = 0;
module_param(enable_sdcard_tune, int, 0644);

static int capture_skipframe = 1;
module_param(capture_skipframe, int, 0644);

static int debug = 6;
module_param(debug, int, 0644);


static u32 sr030pc40_curr_state = SR030PC40_STATE_INVALID;
static u32 sr030pc40_pre_state = SR030PC40_STATE_INVALID;
static int sr030pc40_need_to_initial = 0;

static struct sr030pc40_sensor sr030pc40 = {
	.timeperframe = {
		.numerator    = 0,
		.denominator  = 0,
	},
	.mode			= SR030PC40_MODE_CAMERA,
	.state			= SR030PC40_STATE_PREVIEW,
	.fps            = 0,
	.preview_size   = SR030PC40_PREVIEW_SIZE_640_480,
	.capture_size   = SR030PC40_IMAGE_SIZE_640_480,
	.detect         = SENSOR_NOT_DETECTED,
	.zoom           = SR030PC40_ZOOM_1P00X,
	.effect         = SR030PC40_EFFECT_OFF,
	//.iso		= SR030PC40_ISO_AUTO,
	//.photometry	= SR030PC40_PHOTOMETRY_CENTER,
	.ev             = SR030PC40_EV_DEFAULT,
	.contrast       = SR030PC40_CONTRAST_DEFAULT,
	.wb             = SR030PC40_WB_AUTO,
	.yuv_offset     = SR030PC40_YUV_OFFSET,
	.pretty		= SR030PC40_BLUR_LEVEL_0,
};

static struct sr030pc40_sensor next_sr030pc40 = {
	.mode		= SR030PC40_INVALID_VALUE,
	.state		= SR030PC40_INVALID_VALUE,
	.fps            = SR030PC40_INVALID_VALUE,
	.preview_size   = SR030PC40_INVALID_VALUE,
	.capture_size   = SR030PC40_INVALID_VALUE,
	.detect         = SR030PC40_INVALID_VALUE,
	.effect         = SR030PC40_INVALID_VALUE,
	.ev             = SR030PC40_INVALID_VALUE,
	.contrast       = SR030PC40_INVALID_VALUE,
	.wb             = SR030PC40_INVALID_VALUE,
	.yuv_offset     = SR030PC40_INVALID_VALUE,
	.pretty		= SR030PC40_INVALID_VALUE,
};

struct v4l2_queryctrl sr030pc40_ctrl_list[] = {
	{
		.id            = V4L2_CID_SELECT_MODE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "select mode",
		.minimum       = SR030PC40_MODE_CAMERA,
		.maximum       = SR030PC40_MODE_VT,
		.step          = 1,
		.default_value = SR030PC40_MODE_CAMERA,
	},    
	{
		.id            = V4L2_CID_SELECT_STATE,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "select state",
		.minimum       = SR030PC40_STATE_PREVIEW,
		.maximum       = SR030PC40_STATE_CAPTURE,
		.step          = 1,
		.default_value = SR030PC40_STATE_PREVIEW,
	},    
	{
		.id            = V4L2_CID_ZOOM,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Digital Zoom",
		.minimum       = SR030PC40_ZOOM_1P00X,
		.maximum       = SR030PC40_ZOOM_4P00X,
		.step          = 1,
		.default_value = SR030PC40_ZOOM_1P00X,
	},
	{
		.id            = V4L2_CID_BRIGHTNESS,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Brightness",
		.minimum       = SR030PC40_EV_MINUS_2P0,
		.maximum       = SR030PC40_EV_PLUS_2P0,
		.step          = 1,
		.default_value = SR030PC40_EV_DEFAULT,
	},
	{
		.id            = V4L2_CID_CONTRAST,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Contrast",
		.minimum       = SR030PC40_CONTRAST_MINUS_3,
		.maximum       = SR030PC40_CONTRAST_PLUS_3,
		.step          = 1,
		.default_value = SR030PC40_CONTRAST_DEFAULT,
	},
	{
		.id            = V4L2_CID_WB,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "White Balance",
		.minimum       = SR030PC40_WB_AUTO,
		.maximum       = SR030PC40_WB_FLUORESCENT,
		.step          = 1,
		.default_value = SR030PC40_WB_AUTO,
	},
	{
		.id            = V4L2_CID_EFFECT,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Effect",
		.minimum       = SR030PC40_EFFECT_OFF,
		.maximum       = SR030PC40_EFFECT_GREEN,
		.step          = 1,
		.default_value = SR030PC40_EFFECT_OFF,
	},
	{
		.id            = V4L2_CID_PRETTY,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Pretty",
		.minimum       = SR030PC40_BLUR_LEVEL_0,
		.maximum       = SR030PC40_BLUR_LEVEL_3,
		.step          = 1,
		.default_value = SR030PC40_BLUR_LEVEL_0,
	},
};

#define NUM_SR030PC40_CONTROL ARRAY_SIZE(sr030pc40_ctrl_list)

/* list of image formats supported by SR030PC40 sensor */
const static struct v4l2_fmtdesc sr030pc40_formats[] = {
	{
		.description = "YUV422 (UYVY)",
		.pixelformat = V4L2_PIX_FMT_UYVY,
	},
	{
		.description = "YUV422 (YUYV)",
		.pixelformat = V4L2_PIX_FMT_YUYV,
	},
};

extern struct sr030pc40_platform_data sr030pc40_platform_data0;
static int sr030pc40_set_preview(void);


#define	I2C_RW_WRITE 	0
#define	I2C_RW_READ 	1

#if !defined(SR030PC40_USE_GPIO_I2C)
static int sr030pc40_write_byte(struct i2c_client *client, u8 subaddr, u8 val)
{
	unsigned char buf[2];
	struct i2c_msg msg = { client->addr, I2C_RW_WRITE, 2, buf };
	int ret = 0;

	if (!client->adapter)
	{
		printk(SR030PC40_MOD_NAME "can't search i2c client adapter\n");
		return -ENODEV;
	}

	if (subaddr == 0xff)
	{
		mdelay(val);
		return 0;
	}

	buf[0] = subaddr;
	buf[1] = val;	

	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if(ret != 0) {
		printk(SR030PC40_MOD_NAME "[%s]Error : -EIO\n", __func__);
	}

	return ret;
}
#else	//(SR030PC40_USE_GPIO_I2C)
static int sr030pc40_write_byte(CAM_I2C_CLIENT *client, u8 subaddr, u8 val)
{
	OMAP_GPIO_I2C_WR_DATA i2c_wr_param;
	u8 addr[1];
	u8 buf[1];

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }

	if (subaddr == 0xff)
	{
		mdelay(val);
		return 0;
	}

	addr[0] = subaddr;
	buf[0] = val;	

	i2c_wr_param.reg_len = 1;
	i2c_wr_param.reg_addr = addr;
	i2c_wr_param.wdata_len = 1;
	i2c_wr_param.wdata = buf;
	omap_gpio_i2c_write(client, &i2c_wr_param);

	return 0;
}
#endif	//(SR030PC40_USE_GPIO_I2C)

/*
static int sr030pc40_write_reg(CAM_I2C_CLIENT *client, sr030pc40_reg *pReg)
{
	int err;
	u32 cnt = 0;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }

	while(cnt < 5)
	{
		err = sr030pc40_write_byte(client, (*pReg).addr, (*pReg).value);
		// if (err < 0) 
		// Arun C: stop writing and return err value on error 
		//  Also make error check unlikely

		if(err == 0)
		{
			return 0;
		}
		else
		{
			printk(SR030PC40_MOD_NAME "sr030pc40_i2c_write i2c write error....retry...ret=%d \n",err);
			mdelay(3);
			cnt++;
		}
	}

	return 0;	// FIXME 
}

static int sr030pc40_write_regs(CAM_I2C_CLIENT *client, u16 data[], int size)
{
	int i, err;
	if(!client) { printk("%s client is NULL\n", __func__); return 0; }

	for (i = 0; i < size; i++) {
		err = sr030pc40_write_byte(client, data[i] >> 8, data[i] & 0xff);
//		Arun C: stop writing and return err value on error 
//		Also make error check unlikely
		  
		if (unlikely(err < 0)) {
			printk("%s: register set failed\n", __func__);
			return err;
		}
	}
	return 0;	// FIXME 
}
*/

#if !defined(SR030PC40_USE_GPIO_I2C)
static int sr030pc40_read_byte(struct i2c_client *client, u8 subaddr, u8 *data)
{
	int ret =0;
	u8 buf[1];
	struct i2c_msg msg = { client->addr, I2C_RW_WRITE, 1, buf };

	if (!client->adapter)
	{
		printk(SR030PC40_MOD_NAME "can't search i2c client adapter\n");
		return -ENODEV;
	}

	buf[0] = subaddr;

	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	/* if (ret == -EIO) */
	if (unlikely(ret == -EIO))
		goto error;

	msg.flags = I2C_RW_READ;

	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	/* if (ret == -EIO) */
	if (unlikely(ret == -EIO))
		goto error;

	*data = buf[0];
	return ret;

error:
	printk(SR030PC40_MOD_NAME "[%s]Error : -EIO\n", __func__);
	return ret;
}
#else	//(SR030PC40_USE_GPIO_I2C)
static int sr030pc40_read_byte(CAM_I2C_CLIENT *client, u8 subaddr, u8 *data)
{
	OMAP_GPIO_I2C_RD_DATA i2c_rd_param;
	u8 buf[1];
	u8 addr[1];
	int ret;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	addr[0] = subaddr;

	i2c_rd_param.reg_len = 1;
	i2c_rd_param.reg_addr = addr;
	i2c_rd_param.rdata_len = 1;
	i2c_rd_param.rdata = buf;

	ret = omap_gpio_i2c_read(client, &i2c_rd_param);
	if (unlikely(ret == -EIO))
		goto error;

	*data = buf[0];

error:
	return ret;
}
#endif //(SR030PC40_USE_GPIO_I2C)

static int (*sr030pc40_write_regsets)(CAM_REG16_PACKAGE_T *pArrReg);

static int sr030pc40_write_reg16_table(const u16 reg16[], int nr_arr)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int i = 0;
	int err = 0;
	u16 addr = 0;
	u16 value = 0;

	if (unlikely(!client)) {
		printk("%s: error client is null\n", __func__);
		return -ENODEV;
	}

	for(i=0; i<nr_arr; i++) {
		addr = (reg16[i] >> 8)	& 0xFF;
		value =(reg16[i] 	& 0xFF);

		err = sr030pc40_write_byte(client, addr, value);
		if (unlikely(err < 0 )) {
			dprintk(2, "%s: register set failed\n", __func__);
			return -ENODEV;
		}
	}

	return 0;
}

static int sr030pc40_normal_write_regsets(CAM_REG16_PACKAGE_T *pArrReg) 
{
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int i = 0;
	int err = 0;

	if (unlikely(!client)) {
		printk("%s: error client is null\n", __func__);
		return -ENODEV;
	}

	for(i=0; i<pArrReg->num; i++) {
		err = sr030pc40_write_byte(client, pArrReg->reg16[i].addr, pArrReg->reg16[i].value);
		if (unlikely(err < 0 )) {
			dprintk(2, "%s: register set failed\n", __func__);
			return -ENODEV;
		}
	}

	return 0;
}

static void LoadCamsensorRegSettings()
{
	dprintk(5, "LoadCamsensorRegSettings)+ \n");
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_INIT, SR030PC40_TUNING_INIT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_VT_INIT, SR030PC40_TUNING_VT_INIT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_PREVIEW_SIZE_640_480, SR030PC40_TUNING_PREVIEW_SIZE_640_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_PREVIEW_SIZE_320_240,	SR030PC40_TUNING_PREVIEW_SIZE_320_240);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_PREVIEW_SIZE_176_144,	SR030PC40_TUNING_PREVIEW_SIZE_176_144);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_M_4,	SR030PC40_TUNING_BRIGHTNESS_M_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_M_3,	SR030PC40_TUNING_BRIGHTNESS_M_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_M_2,	SR030PC40_TUNING_BRIGHTNESS_M_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_M_1,	SR030PC40_TUNING_BRIGHTNESS_M_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_0,	SR030PC40_TUNING_BRIGHTNESS_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_P_1,	SR030PC40_TUNING_BRIGHTNESS_P_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_P_2,	SR030PC40_TUNING_BRIGHTNESS_P_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_P_3,	SR030PC40_TUNING_BRIGHTNESS_P_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BRIGHTNESS_P_4,	SR030PC40_TUNING_BRIGHTNESS_P_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_M_5,	SR030PC40_TUNING_CONTRAST_M_5);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_M_4,	SR030PC40_TUNING_CONTRAST_M_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_M_3,	SR030PC40_TUNING_CONTRAST_M_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_M_2,	SR030PC40_TUNING_CONTRAST_M_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_M_1,	SR030PC40_TUNING_CONTRAST_M_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_0,	SR030PC40_TUNING_CONTRAST_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_P_1,	SR030PC40_TUNING_CONTRAST_P_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_P_2,	SR030PC40_TUNING_CONTRAST_P_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_P_3,	SR030PC40_TUNING_CONTRAST_P_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_P_4,	SR030PC40_TUNING_CONTRAST_P_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_CONTRAST_P_5,	SR030PC40_TUNING_CONTRAST_P_5);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_WHITE_BALANCE_AUTO,	SR030PC40_TUNING_WHITE_BALANCE_AUTO);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_WHITE_BALANCE_DAYLIGHT,	SR030PC40_TUNING_WHITE_BALANCE_DAYLIGHT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_WHITE_BALANCE_CLOUDY,	SR030PC40_TUNING_WHITE_BALANCE_CLOUDY);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_WHITE_BALANCE_FLUORESCENT,	SR030PC40_TUNING_WHITE_BALANCE_FLUORESCENT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_WHITE_BALANCE_INCANDESCENT,	SR030PC40_TUNING_WHITE_BALANCE_INCANDESCENT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_EFFECT_OFF,	SR030PC40_TUNING_EFFECT_OFF);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_EFFECT_GRAY,	SR030PC40_TUNING_EFFECT_GRAY);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_EFFECT_SEPIA,	SR030PC40_TUNING_EFFECT_SEPIA);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_EFFECT_NEGATIVE,	SR030PC40_TUNING_EFFECT_NEGATIVE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_EFFECT_AQUA,	SR030PC40_TUNING_EFFECT_AQUA);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BLUR_0,	SR030PC40_TUNING_BLUR_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BLUR_P_1,	SR030PC40_TUNING_BLUR_P_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BLUR_P_2,	SR030PC40_TUNING_BLUR_P_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_BLUR_P_3,	SR030PC40_TUNING_BLUR_P_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_AUTO_FPS,	SR030PC40_TUNING_AUTO_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_7_FPS,	SR030PC40_TUNING_7_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_10_FPS,	SR030PC40_TUNING_10_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_15_FPS,	SR030PC40_TUNING_15_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_FLIP_NONE,	SR030PC40_TUNING_FLIP_NONE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_FLIP_MIRROR,	SR030PC40_TUNING_FLIP_MIRROR);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_VT_FLIP_NONE,	SR030PC40_TUNING_VT_FLIP_NONE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_VT_FLIP_MIRROR,	SR030PC40_TUNING_VT_FLIP_MIRROR);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_DTP_ON,	SR030PC40_TUNING_DTP_ON);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG16SET_DTP_OFF,	SR030PC40_TUNING_DTP_OFF);

	if (enable_sdcard_tune)
		Load16BitTuningFile();
}

static int sr030pc40_detect(CAM_I2C_CLIENT *client)
{

	int ret0 = 0;
	int ret1 = 0;
	u8 version[1] = {0xff};

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	/*===========================================================================
	 * there's no way to decide which one to use before sensor revision check,
	 * so we use reset-default page number (0x00) without specifying explicitly
	 ===========================================================================*/

	ret0 = sr030pc40_write_byte(client, PAGEMODE, 0x00);
	ret1 = sr030pc40_read_byte(client, DEVID, version);

	if (!(ret0 || ret1))
	{
		printk(SR030PC40_MOD_NAME"=================================\n");
		printk(SR030PC40_MOD_NAME"   [VGA CAM] vendor_id ID : 0x%x\n", version[0]);
		printk(SR030PC40_MOD_NAME"=================================\n");            

		//if(version[0] == 0x8C)
		if(version[0] == 0x96)
		{
			printk(SR030PC40_MOD_NAME"===============================\n");
			printk(SR030PC40_MOD_NAME"   [VGA CAM] sr030pc40  OK!!\n");
			printk(SR030PC40_MOD_NAME"===============================\n");
		}
		else
		{
			printk(SR030PC40_MOD_NAME"==========================================\n");
			printk(SR030PC40_MOD_NAME"   [VGA CAM] camsemsor operation fail!!\n");
			printk(SR030PC40_MOD_NAME"===========================================\n");
			return -EINVAL;
		}
	}
	else
	{
		printk(SR030PC40_MOD_NAME"-------------------------------------------------\n");
		printk(SR030PC40_MOD_NAME"   [VGA CAM] sensor reset failure !!\n");
		printk(SR030PC40_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}
	return 0;

}

/*
static void register_dump(CAM_I2C_CLIENT *client)
{
	int i;
	u8 read_buf[1];
	u8 data[2];
	int init_index;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	init_index  = ARRAY_SIZE(sr030pc40_init);

	for (i = 0; i < init_index; i++) 
	{
		//data[0] = sr030pc40_init[i].addr;
		//data[1] = sr030pc40_init[i].value;
		data[0] = sr030pc40_init[i] >> 8;
		data[1] = sr030pc40_init[i] & 0xff;

		if(data[0] == 0x03)	// means page change
		{
			sr030pc40_write_byte(client, data[0], data[1]);			
		}
		else
		{
			sr030pc40_read_byte(client, data[0],(unsigned char*)&read_buf);
			printk("0x%02x = 0x%02x\n", data[0], read_buf[0]);					
		}
	}
}
*/

static int sr030pc40_set_init(void)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;

	if(sensor->mode == SR030PC40_MODE_VT) {
		dprintk(5, "set init table : vt\n");
		sr030pc40_write_regsets(&CAM_REG16SET_VT_INIT);

		sensor->mode		= SR030PC40_MODE_VT;
		sensor->state		= SR030PC40_STATE_PREVIEW;
		sensor->fps		= 15;
		sensor->preview_size	= SR030PC40_PREVIEW_SIZE_640_480;
		sensor->capture_size	= SR030PC40_IMAGE_SIZE_640_480;
		sensor->effect		= SR030PC40_EFFECT_OFF;
		sensor->ev		= SR030PC40_EV_DEFAULT;
		sensor->contrast	= SR030PC40_CONTRAST_DEFAULT;
		sensor->wb		= SR030PC40_WB_AUTO;
		sensor->yuv_offset	= SR030PC40_YUV_OFFSET;
		sensor->pretty		= SR030PC40_BLUR_LEVEL_0;
	} else {
		dprintk(5, "set init table : normal\n");
		sr030pc40_write_regsets(&CAM_REG16SET_INIT);

		sensor->mode		= SR030PC40_MODE_CAMERA;
		sensor->state		= SR030PC40_STATE_PREVIEW;
		sensor->fps		= 0;
		sensor->preview_size	= SR030PC40_PREVIEW_SIZE_640_480;
		sensor->capture_size	= SR030PC40_IMAGE_SIZE_640_480;
		sensor->effect		= SR030PC40_EFFECT_OFF;
		sensor->ev		= SR030PC40_EV_DEFAULT;
		sensor->contrast	= SR030PC40_CONTRAST_DEFAULT;
		sensor->wb		= SR030PC40_WB_AUTO;
		sensor->yuv_offset	= SR030PC40_YUV_OFFSET;
		sensor->pretty		= SR030PC40_BLUR_LEVEL_0;
	}

	return 0;
}




static int sr030pc40_check_dataline()
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	if (!client) { 
		printk("%s client is NULL\n", __func__);
		return 0;
	}

	dprintk(5, "sr030pc40_check_dataline is called...\n");
	sr030pc40_write_regsets(&CAM_REG16SET_DTP_ON);
	return 0;
}


static int sr030pc40_check_dataline_stop()
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	dprintk(5, "sr030pc40_check_dataline_stop is called...\n");
	sr030pc40_write_regsets(&CAM_REG16SET_DTP_OFF);

	sensor->check_dataline = 0;
	return 0;
}

static int sr030pc40_set_flip(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	switch(value)
	{
	case SR030PC40_FLIP_NONE:
		if(sensor->mode == SR030PC40_MODE_VT) {
			dprintk(5, "set_flip : (vt)none\n");
			sr030pc40_write_regsets(&CAM_REG16SET_VT_FLIP_NONE);
		} else {
			dprintk(5, "set_flip : none\n");
			sr030pc40_write_regsets(&CAM_REG16SET_FLIP_NONE);
		}
		break;      

	case SR030PC40_FLIP_MIRROR:
		if(sensor->mode == SR030PC40_MODE_VT) {
			dprintk(5, "set_flip : (vt)mirror\n");
			sr030pc40_write_regsets(&CAM_REG16SET_VT_FLIP_MIRROR);
		} else {
			dprintk(5, "set_flip : mirror\n");
			sr030pc40_write_regsets(&CAM_REG16SET_FLIP_MIRROR);
		}
		break;
	default:
		printk(SR030PC40_MOD_NAME "[Flip]Invalid value is ordered!!!\n");
		goto flip_fail;
	}

	sensor->flip = value;

	return 0;

flip_fail:
	printk(SR030PC40_MOD_NAME "sr030pc40_set_flip is failed!!!\n");
	return -EINVAL; 
}


static void sr030pc40_set_skip(void)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;

	int skip_frame = 0; //snapshot

	if(sensor->state == SR030PC40_STATE_PREVIEW) {      
		switch (sensor->fps) 
		{
		case 7:
			skip_frame = 3;
			break;
		case 10:
			skip_frame = 3;
			break;
		case 0:
		case 15:
		default:
			skip_frame = 4;
			break;
		}
	} else {
		skip_frame = capture_skipframe;
	}
	dprintk(5, "set_skip : %d frame\n", skip_frame);
	isp_set_hs_vs(0, skip_frame);
}

static int sr030pc40_set_fps(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client; 

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	switch(value)
	{
	case 0:
		dprintk(5, "set_fps : auto\n");
		sr030pc40_write_regsets(&CAM_REG16SET_AUTO_FPS);
		break;
	case 15:
		dprintk(5, "set_fps : 15\n");
		sr030pc40_write_regsets(&CAM_REG16SET_15_FPS);
		break;
	case 10:
		dprintk(5, "set_fps : 10\n");
		sr030pc40_write_regsets(&CAM_REG16SET_10_FPS);
		break;
	case 7:
		dprintk(5, "set_fps : 7\n");
		sr030pc40_write_regsets(&CAM_REG16SET_7_FPS);
		break;   
	default:
		printk(SR030PC40_MOD_NAME "[%d fps]Invalid Value!!\n", value);
		return -EINVAL; 
	}  
	sensor->fps = value;
	return 0;
}

static int sr030pc40_set_mode(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;

	dprintk(6, "sr030pc40_set_mode is called... mode = %d\n", value);  

	sensor->mode = value;

	return 0;
}

static int sr030pc40_set_state(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;

	dprintk(6, "sr030pc40_set_state is called... state = %d\n", value);  

	sensor->state = value;

	sr030pc40_pre_state = sr030pc40_curr_state;
	sr030pc40_curr_state = sensor->state;

	return 0;
}


static int sr030pc40_set_zoom(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;

	sensor->zoom = value;

	return 0;
}

static int sr030pc40_set_effect(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	dprintk(6, "sr030pc40_set_effect is called... effect = %d\n", value);

	switch(value)
	{
	case SR030PC40_EFFECT_OFF:
		dprintk(5, "set_effect : off\n");
		sr030pc40_write_regsets(&CAM_REG16SET_EFFECT_OFF);
		break;

	case SR030PC40_EFFECT_BW:
		goto NOT_SUPPORT_EFFECT;
		break;    

	case SR030PC40_EFFECT_GREY:
		dprintk(5, "set_effect : gray\n");
		sr030pc40_write_regsets(&CAM_REG16SET_EFFECT_GRAY);
		break;      

	case SR030PC40_EFFECT_SEPIA:
		dprintk(5, "set_effect : sepia\n");
		sr030pc40_write_regsets(&CAM_REG16SET_EFFECT_SEPIA);
		break;

	case SR030PC40_EFFECT_SHARPEN:
		//dprintk(5, "set_effect : sharpen\n");
		goto NOT_SUPPORT_EFFECT;
		break;      

	case SR030PC40_EFFECT_NEGATIVE:
		dprintk(5, "set_effect : negative\n");
		sr030pc40_write_regsets(&CAM_REG16SET_EFFECT_NEGATIVE);
		break;

	case SR030PC40_EFFECT_ANTIQUE:
		goto NOT_SUPPORT_EFFECT;
		break;

	case SR030PC40_EFFECT_AQUA:
		dprintk(5, "set_effect : aqua\n");
		sr030pc40_write_regsets(&CAM_REG16SET_EFFECT_AQUA);
		break;

	case SR030PC40_EFFECT_RED:
		goto NOT_SUPPORT_EFFECT;
		break; 

	case SR030PC40_EFFECT_PINK:
		goto NOT_SUPPORT_EFFECT;
		break; 

	case SR030PC40_EFFECT_YELLOW:
		goto NOT_SUPPORT_EFFECT;
		break;       

	case SR030PC40_EFFECT_GREEN:
		goto NOT_SUPPORT_EFFECT;
		break; 

	case SR030PC40_EFFECT_BLUE:
		goto NOT_SUPPORT_EFFECT;
		break; 

	default:
		printk(SR030PC40_MOD_NAME "[Effect]Invalid value is ordered!!!\n");
		return -EINVAL;
	}
	sensor->effect = value;

	return 0;

NOT_SUPPORT_EFFECT:
	dprintk(6, "doesn't support effect = %d\n", value);
	return 0;

}


static int sr030pc40_set_blur(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int err = 0;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	dprintk(6, "sr030pc40_set_blur is called... value = %d\n", value);
	switch(value)
	{
	case SR030PC40_BLUR_LEVEL_0:
		dprintk(5, "set_blur : level 0\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BLUR_0);
		break;

	case SR030PC40_BLUR_LEVEL_1:
		dprintk(5, "set_blur : level 1\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BLUR_P_1);
		break;

	case SR030PC40_BLUR_LEVEL_2:
		dprintk(5, "set_blur : level 2\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BLUR_P_2);
		break;

	case SR030PC40_BLUR_LEVEL_3:
		dprintk(5, "set_blur : level 3\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BLUR_P_3);
		break;

	default:
		printk(SR030PC40_MOD_NAME "[%s]Invalid value is ordered!!!\n", __func__);
		err = 0;
		break;
	}
	return err;
}

static int sr030pc40_set_contrast(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }

	switch(value)
	{
	case SR030PC40_CONTRAST_MINUS_3:
		dprintk(5, "set_contrast : minus 3\n");
		sr030pc40_write_regsets(&CAM_REG16SET_CONTRAST_M_3);
		break;
	case SR030PC40_CONTRAST_MINUS_2:
		dprintk(5, "set_contrast : minus 2\n");
		sr030pc40_write_regsets(&CAM_REG16SET_CONTRAST_M_2);
		break;
	case SR030PC40_CONTRAST_MINUS_1:
		dprintk(5, "set_contrast : minus 1\n");
		sr030pc40_write_regsets(&CAM_REG16SET_CONTRAST_M_1);
		break;
	case SR030PC40_CONTRAST_DEFAULT:
		dprintk(5, "set_contrast : default\n");
		sr030pc40_write_regsets(&CAM_REG16SET_CONTRAST_0);
		break;
	case SR030PC40_CONTRAST_PLUS_1:
		dprintk(5, "set_contrast : plus 1\n");
		sr030pc40_write_regsets(&CAM_REG16SET_CONTRAST_P_1);
		break;
	case SR030PC40_CONTRAST_PLUS_2:
		dprintk(5, "set_contrast : plus 2\n");
		sr030pc40_write_regsets(&CAM_REG16SET_CONTRAST_P_2);
		break;
	case SR030PC40_CONTRAST_PLUS_3:
		dprintk(5, "set_contrast : plus 3\n");
		sr030pc40_write_regsets(&CAM_REG16SET_CONTRAST_P_3);
		break;
	default:
		printk(SR030PC40_MOD_NAME "[CONTRAST]Invalid value is ordered!!!\n");
		return -EINVAL;
	}

	sensor->contrast = value;
	return 0;
}


static int sr030pc40_set_wb(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	dprintk(6, "sr030pc40_set_wb is called... value = %d\n", value);

	switch(value)
	{
	case SR030PC40_WB_AUTO:
		dprintk(5, "set_wb : auto\n");
		sr030pc40_write_regsets(&CAM_REG16SET_WHITE_BALANCE_AUTO);
		break;
	case SR030PC40_WB_DAYLIGHT:
		dprintk(5, "set_wb : daylight\n");
		sr030pc40_write_regsets(&CAM_REG16SET_WHITE_BALANCE_DAYLIGHT);
		break;
	case SR030PC40_WB_CLOUDY:
		dprintk(5, "set_wb : cloudy\n");
		sr030pc40_write_regsets(&CAM_REG16SET_WHITE_BALANCE_CLOUDY);
		break;
	case SR030PC40_WB_INCANDESCENT:
		dprintk(5, "set_wb : incandescent\n");
		sr030pc40_write_regsets(&CAM_REG16SET_WHITE_BALANCE_INCANDESCENT);
		break;
	case SR030PC40_WB_FLUORESCENT:
		dprintk(5, "set_wb : fluorescent\n");
		sr030pc40_write_regsets(&CAM_REG16SET_WHITE_BALANCE_FLUORESCENT);
		break;
	default:
		printk(SR030PC40_MOD_NAME "[WB]Invalid value is ordered!!!\n");
		return -EINVAL;
	}
	sensor->wb = value;
	return 0;
}

static int sr030pc40_set_ev(s32 value)
{

	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }

	switch(value)
	{
	case SR030PC40_EV_MINUS_2P0:
		dprintk(5, "set_ev : minus 2\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_M_4);
		break;
	case SR030PC40_EV_MINUS_1P5:
		dprintk(5, "set_ev : minus 1.5\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_M_3);
		break;
	case SR030PC40_EV_MINUS_1P0:
		dprintk(5, "set_ev : minus 1\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_M_2);
		break;
	case SR030PC40_EV_MINUS_0P5:
		dprintk(5, "set_ev : minus 0.5\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_M_1);
		break;

	case SR030PC40_EV_DEFAULT:
		dprintk(5, "set_ev : default\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_0);
		break;

	case SR030PC40_EV_PLUS_2P0:
		dprintk(5, "set_ev : plus 2\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_P_4);
		break;
	case SR030PC40_EV_PLUS_1P5:
		dprintk(5, "set_ev : plus 1.5\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_P_3);
		break;
	case SR030PC40_EV_PLUS_1P0:
		dprintk(5, "set_ev : plus 1\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_P_2);
		break;
	case SR030PC40_EV_PLUS_0P5:
		dprintk(5, "set_ev : plus 0.5\n");
		sr030pc40_write_regsets(&CAM_REG16SET_BRIGHTNESS_P_1);
		break;
	default: 
		printk(SR030PC40_MOD_NAME "EV value is not supported!!!\n");
		return -EINVAL;    
	}
	sensor->ev=value;
	return 0;
}

static int sr030pc40_start_preview(void)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int ret = 0;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	dprintk(5, "sr030pc40_start_preview is called...\n");

	//dprintk(6, "sensor mode = %d\n", sensor->mode);
	//dprintk(6, "preview size = %d\n", sensor->preview_size);


	/* Preview Start */
	//register_dump(client);

	return 0;
}

static int sr030pc40_set_preview_size(s32 value)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	s32 old_value = (s32)sensor->preview_size;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	/* Set Preview Size*/	
	switch(value)
	{
	case SR030PC40_PREVIEW_SIZE_176_144:
		dprintk(5, "set_preview_size : 176_144\n");
		sr030pc40_write_regsets(&CAM_REG16SET_PREVIEW_SIZE_176_144);
		break;
	case SR030PC40_PREVIEW_SIZE_320_240:
		dprintk(5, "set_preview_size : 320_240\n");
		sr030pc40_write_regsets(&CAM_REG16SET_PREVIEW_SIZE_320_240);
		break;
	case SR030PC40_PREVIEW_SIZE_640_480:
		dprintk(5, "set_preview_size : 640_480\n");
		sr030pc40_write_regsets(&CAM_REG16SET_PREVIEW_SIZE_640_480);
		break;
	}
	sensor->preview_size = value;
	return 0;
}


static int sr030pc40_set_preview(void)
{
	struct sr030pc40_sensor *n_sensor = &next_sr030pc40;
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int fps = 0;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }

	if (sr030pc40_need_to_initial) {
		sr030pc40_set_init();
		sr030pc40_need_to_initial = 0;
	}

	//if (sensor->preview_size != index) sr030pc40_set_preview_size(index);

	if(sensor->check_dataline || dtp_on) {
		sr030pc40_check_dataline();
		sr030pc40_need_to_initial = 1;
		return 0;
	}

	if (n_sensor->effect != SR030PC40_INVALID_VALUE) {
		if (n_sensor->effect != sensor->effect) sr030pc40_set_effect(n_sensor->effect);
	}
	if (n_sensor->wb != SR030PC40_INVALID_VALUE) {
			if (n_sensor->wb != sensor->wb) sr030pc40_set_wb(n_sensor->wb);
	}
	if (n_sensor->ev != SR030PC40_INVALID_VALUE) {
		if (n_sensor->ev != sensor->ev) sr030pc40_set_ev(n_sensor->ev);
	}
	if (n_sensor->contrast != SR030PC40_INVALID_VALUE) {
		if (n_sensor->contrast != sensor->contrast) sr030pc40_set_contrast(n_sensor->contrast);
	}
	if (n_sensor->pretty != SR030PC40_INVALID_VALUE) {
		if(n_sensor->pretty != sensor->pretty) sr030pc40_set_blur(n_sensor->pretty);
	}

	// Set FPS
	if (sensor->timeperframe.numerator == 0 || sensor->timeperframe.denominator == 0) {
	       	fps = 0;
	} else {
		fps = sensor->timeperframe.denominator / sensor->timeperframe.numerator;
	}
	if (sensor->mode != SR030PC40_MODE_VT) fps = 0;
	if (sensor->fps != fps) sr030pc40_set_fps(fps);
	sr030pc40_set_flip(SR030PC40_FLIP_MIRROR);

	return 0;
}

static int sr030pc40_start_capture(void)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	int err = 0;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	dprintk(5, "sr030pc40_start_capture is called...\n");

	/* Capture Start */
	return err;
}




static int ioctl_streamoff(struct v4l2_int_device *s)
{
	dprintk(6, "ioctl_streamoff is called...\n");

	return 0;
}

static int ioctl_streamon(struct v4l2_int_device *s)
{
	struct sr030pc40_sensor *sensor = s->priv;  

	int err = 0;

	dprintk(6, "ioctl_streamon is called...(%x)\n", sensor->state);  

	if(sensor->state != SR030PC40_STATE_CAPTURE)
	{
		dprintk(7, "start preview....................\n");
		err = sr030pc40_start_preview();
		sr030pc40_pre_state = sr030pc40_curr_state;
		sr030pc40_curr_state = SR030PC40_STATE_PREVIEW;     
	}
	else
	{
		dprintk(7, "start capture....................\n");
		err = sr030pc40_start_capture();		
		sr030pc40_pre_state = sr030pc40_curr_state;
		sr030pc40_curr_state = SR030PC40_STATE_CAPTURE;        
	}

	return err;
}

/**
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the sr030pc40_ctrl_list[] array.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s,
		struct v4l2_queryctrl *qc)
{
	int i;

	dprintk(6, "ioctl_queryctrl is called...\n");

	for (i = 0; i < NUM_SR030PC40_CONTROL; i++) 
	{
		if (qc->id == sr030pc40_ctrl_list[i].id)
		{
			break;
		}
	}
	if (i == NUM_SR030PC40_CONTROL)
	{
		printk(SR030PC40_MOD_NAME "Control ID is not supported!!\n");
		qc->flags = V4L2_CTRL_FLAG_DISABLED;

		return -EINVAL;
	}

	*qc = sr030pc40_ctrl_list[i];

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
	struct sr030pc40_sensor *sensor = s->priv;

	int retval = 0;

	dprintk(6, "ioctl_g_ctrl is called...(%d)\n", vc->id);

	switch (vc->id) 
	{
	case V4L2_CID_SELECT_MODE:
		vc->value = sensor->mode;
		break;  
	case V4L2_CID_SELECT_STATE:
		vc->value = sensor->state;
		break;       
	case V4L2_CID_AF:
		vc->value = 2;
		break;
	case V4L2_CID_ZOOM:
		retval = sensor->zoom;
		break;
	case V4L2_CID_ISO:
		//vc->value = sensor->iso;
		break;
	case V4L2_CID_BRIGHTNESS:
		vc->value = sensor->ev;
		break;
	case V4L2_CID_CONTRAST:
		vc->value = sensor->contrast;
		break; 
	case V4L2_CID_WB:
		vc->value = sensor->wb;
		break;      
	case V4L2_CID_SATURATION:
		//vc->value = sensor->saturation;
		break;
	case V4L2_CID_EFFECT:
		vc->value = sensor->effect;
		break;
	case V4L2_CID_FLIP:
		vc->value = sensor->flip;
		break;
	case V4L2_CID_PHOTOMETRY:
		//vc->value = sensor->photometry;
		break;
	case V4L2_CID_WDR:
		//vc->value = sensor->wdr;
		break;
	case V4L2_CID_SCENE:
		retval = sensor->scene;
		break;      
	default:
		printk(SR030PC40_MOD_NAME "G_CTRL : [0x%08x(%d)]Invalid value!!\n", vc->id, vc->id);
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
 * value in HW (and updates the SR030PC40 sensor struct).
 * Otherwise, * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct sr030pc40_sensor *n_sensor = &next_sr030pc40;
	struct sr030pc40_sensor *sensor = s->priv;
	int retval = 0;

	dprintk(6, "ioctl_s_ctrl is called...(%d)\n", vc->id);

	switch (vc->id) 
	{
	case V4L2_CID_SELECT_MODE:
		retval = sr030pc40_set_mode(vc->value);
		break;  
	case V4L2_CID_SELECT_STATE:
		retval = sr030pc40_set_state(vc->value);
		break;       
	case V4L2_CID_BRIGHTNESS:
//		if (sensor->ev == vc->value) {
//			dprintk(6, "same as before : ev(%d)\n", vc->value);
//			break;
//		}
		if(sensor->state != SR030PC40_STATE_PREVIEW) {
			dprintk(5, "update brightness : [%d->%d]\n", sensor->ev, vc->value);
			n_sensor->ev = vc->value;
			break;
		}
		retval = sr030pc40_set_ev(vc->value);
		break;
	case V4L2_CID_CONTRAST:
//		if (sensor->contrast == vc->value) {
//			dprintk(6, "same as before : contrast(%d)\n", vc->value);
//			break;
//		}
		if(sensor->state != SR030PC40_STATE_PREVIEW) {
			dprintk(5, "update contrast : [%d->%d]\n", sensor->contrast, vc->value);
			n_sensor->contrast = vc->value;
			break;
		}
		retval = sr030pc40_set_contrast(vc->value);
		break;
	case V4L2_CID_WB:
//		if (sensor->wb == vc->value) {
//			dprintk(6, "same as before : wb(%d)\n", vc->value);
//			break;
//		}
		if(sensor->state != SR030PC40_STATE_PREVIEW) {
			dprintk(5, "update white balance : [%d->%d]\n", sensor->wb, vc->value);
			n_sensor->wb = vc->value;
			break;
		}
		retval = sr030pc40_set_wb(vc->value);
		break;
	case V4L2_CID_EFFECT:
//		if (sensor->effect == vc->value) {
//			dprintk(6, "same as before : effect(%d)\n", vc->value);
//			break;
//		}
		if(sensor->state != SR030PC40_STATE_PREVIEW) {
			dprintk(5, "update effect : [%d->%d]\n", sensor->effect, vc->value);
			n_sensor->effect = vc->value;
			break;
		}
		retval = sr030pc40_set_effect(vc->value);
		break;
		//case V4L2_CID_CAMERA_FRAME_RATE:		//gwang : need to fix
		//sensor->fps = vc->value;
		//break;
	case V4L2_CID_FLIP:
		retval = sr030pc40_set_flip(vc->value);
		break;
	case V4L2_CID_PRETTY:			//gwang : need to add V4L2_CID_CAMERA_VGA_BLUR
//		if (sensor->pretty == vc->value) {
//			dprintk(6, "same as before : pretty(%d)\n", vc->value);
//			break;
//		}
		if(sensor->state != SR030PC40_STATE_PREVIEW) {
			dprintk(5, "update pretty : [%d->%d]\n", sensor->pretty, vc->value);
			n_sensor->pretty= vc->value;
			break;
		}
		retval = sr030pc40_set_blur(vc->value);
		break;
	case V4L2_CID_ZOOM:			// not support
		dprintk(5, "doesn't support Zoom\n");
		break;
	case V4L2_CID_ISO:			// not support
		dprintk(5, "doesn't support ISO\n");
		break;
	case V4L2_CID_SCENE:		// not support
		dprintk(5, "doesn't support Scene Mode\n");
		break;
	case V4L2_CID_PHOTOMETRY:	// not support
		dprintk(5, "doesn't support photometry\n");
		break;
	case V4L2_CID_WDR:			// not support
		dprintk(5, "doesn't support WDR\n");
		break;
	case V4L2_CID_AEWB:
	case V4L2_CID_CAMERA_AE_AWB_LOCKUNLOCK:
		dprintk(5, "doesn't support AEWB\n");
		break;
	case V4L2_CID_CAMERA_CAPTURE_SIZE:
		dprintk(5, "doesn't support capture_size\n");
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
		sensor->check_dataline = vc->value;
		retval = 0;
		break;	
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		retval = sr030pc40_check_dataline_stop();
		break;	  
	default:
		printk(SR030PC40_MOD_NAME "S_CTRL : [0x%08x(%d)]Invalid value!!\n", vc->id, vc->id);
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

	dprintk(6, "ioctl_enum_fmt_cap is called...\n");

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

		default:
			printk(SR030PC40_MOD_NAME "[format]Invalid value is ordered!!!\n");
			return -EINVAL;
		}
		break;

	default:
		printk(SR030PC40_MOD_NAME "[type]Invalid value is ordered!!!\n");
		return -EINVAL;
	}

	fmt->flags = sr030pc40_formats[index].flags;
	fmt->pixelformat = sr030pc40_formats[index].pixelformat;
	strlcpy(fmt->description, sr030pc40_formats[index].description, sizeof(fmt->description));

	dprintk(7, "ioctl_enum_fmt_cap flag : %d\n", fmt->flags);
	dprintk(7, "ioctl_enum_fmt_cap description : %s\n", fmt->description);

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
	struct sr030pc40_sensor *n_sensor = &next_sr030pc40;
	struct sr030pc40_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix2 = &sensor->pix;

	int err = 0;
	int index = 0;
	int fps = 0;

	CAM_I2C_CLIENT *client = cam_i2c_client;

	if(!client) { printk("%s client is NULL\n", __func__); return 0; }
	dprintk(5, "ioctl_try_fmt_cap [mode:%d][state:%d][%dx%d]\n", 
			sensor->mode,
			sensor->state,
			pix->width,
			pix->height);

	if (sensor->state == SR030PC40_STATE_CAPTURE) { 
		sr030pc40_set_skip();  
		for(index = 0; index < ARRAY_SIZE(sr030pc40_image_sizes); index++) {
			if(sr030pc40_image_sizes[index].width == pix->width && 
					sr030pc40_image_sizes[index].height == pix->height) {
				sensor->capture_size = index;
				break;
			}
		}   

		if (index == ARRAY_SIZE(sr030pc40_image_sizes)) {
			printk(SR030PC40_MOD_NAME "Capture Image Size is not supported!\n");
			return -EINVAL;
		}
		dprintk(6, "capture size[%d][%dx%d]\n", index, 
				sr030pc40_image_sizes[index].width, 
				sr030pc40_image_sizes[index].height);

		if (pix->pixelformat == V4L2_PIX_FMT_UYVY || 
				pix->pixelformat == V4L2_PIX_FMT_YUYV) {
			pix->field = V4L2_FIELD_NONE;
			pix->bytesperline = pix->width * 2;
			pix->sizeimage = pix->bytesperline * pix->height;
			dprintk(7, "V4L2_PIX_FMT_UYVY\n");
		} else {    
			dprintk(4, SR030PC40_MOD_NAME "Doesn't Support V4L2_PIX_FMT_JPEG\n");
			return -EINVAL;
		} 
		dprintk(7, "set capture....................\n");
		if (sensor->capture_size != sensor->preview_size) {
			sr030pc40_set_preview_size(sensor->capture_size);
		}
	}  else {  
		if (SR030PC40_STATE_CAPTURE == sr030pc40_pre_state) {
			dprintk(5, "return to preview\n");
			return 0;
		}
		sr030pc40_set_skip();

		for(index = 0; index < ARRAY_SIZE(sr030pc40_preview_sizes); index++) {
			if(sr030pc40_preview_sizes[index].width == pix->width && 
					sr030pc40_preview_sizes[index].height == pix->height) {
				sensor->preview_size = index;
				break;
			}
		}   

		if(index == ARRAY_SIZE(sr030pc40_preview_sizes)) {
			printk(SR030PC40_MOD_NAME "Preview Image Size is not supported!\n");
			return -EINVAL;
		}
		dprintk(6, "preview size[%d][%dx%d]\n", index, 
				sr030pc40_preview_sizes[index].width, 
				sr030pc40_preview_sizes[index].height);

		pix->field = V4L2_FIELD_NONE;
		pix->bytesperline = pix->width * 2;
		pix->sizeimage = pix->bytesperline * pix->height;  
		dprintk(7, "V4L2_PIX_FMT_VYUY\n");

		dprintk(7, "set preview....................\n");     
		//err = sr030pc40_set_preview();
		
		if (sr030pc40_need_to_initial) {
			sr030pc40_set_init();
			sr030pc40_need_to_initial = 0;
		}

		//if (n_sensor->preview_size != SR030PC40_INVALID_VALUE) {
			//if (n_sensor->preview_size != sensor->preview_size) sr030pc40_set_preview_size(n_sensor->preview_size);
		//}

		if (sensor->preview_size != index) sr030pc40_set_preview_size(index);

		if (sensor->check_dataline || dtp_on) {
			sr030pc40_check_dataline();
			return 0;
		}

		if (n_sensor->effect != SR030PC40_INVALID_VALUE) {
			if (n_sensor->effect != sensor->effect) sr030pc40_set_effect(n_sensor->effect);
		}
		if (n_sensor->wb != SR030PC40_INVALID_VALUE) {
			if (n_sensor->wb != sensor->wb) sr030pc40_set_wb(n_sensor->wb);
		}
		if (n_sensor->ev != SR030PC40_INVALID_VALUE) {
			if (n_sensor->ev != sensor->ev) sr030pc40_set_ev(n_sensor->ev);
		}
		if (n_sensor->contrast != SR030PC40_INVALID_VALUE) {
			if (n_sensor->contrast != sensor->contrast) sr030pc40_set_contrast(n_sensor->contrast);
		}
		if (n_sensor->pretty != SR030PC40_INVALID_VALUE) {
			if(n_sensor->pretty != sensor->pretty) sr030pc40_set_blur(n_sensor->pretty);
		}

		// Set FPS
		if (sensor->timeperframe.numerator == 0 || sensor->timeperframe.denominator == 0) {
			fps = 0;
		} else {
			fps = sensor->timeperframe.denominator / sensor->timeperframe.numerator;
		}
		if (sensor->mode != SR030PC40_MODE_VT) fps = 0;
		if (sensor->fps != fps) sr030pc40_set_fps(fps);
		sr030pc40_set_flip(SR030PC40_FLIP_MIRROR);
	}      

	switch (pix->pixelformat) 
	{
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_JPEG:
	case V4L2_PIX_FMT_MJPEG:
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

	dprintk(6, "ioctl_s_fmt_cap called...\n");

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
	struct sr030pc40_sensor *sensor = s->priv;

	dprintk(6, "ioctl_g_fmt_cap is called...\n");

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
	struct sr030pc40_sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	dprintk(6, "ioctl_g_parm is called...\n");

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
	{
		printk(SR030PC40_MOD_NAME "ioctl_g_parm type not supported.\n");
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
	struct sr030pc40_sensor *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;

	dprintk(6, "ioctl_s_parm is called...\n");

	/* Set mode (camera/camcorder/vt) & state (preview/capture) */
	sensor->mode = a->parm.capture.capturemode;
	sensor->state = a->parm.capture.currentstate;

	sr030pc40_pre_state = sr030pc40_curr_state;
	sr030pc40_curr_state = sensor->state;

	dprintk(7, "mode = %d, state = %d\n", sensor->mode, sensor->state);   

	/* Set time per frame (FPS) */
	/*
	if((timeperframe->numerator == 0) && (timeperframe->denominator == 0)) {
		sensor->fps = 15;
	} else {
		sensor->fps = timeperframe->denominator / timeperframe->numerator;
	}
	*/
	if (timeperframe->numerator == 0) {
		if (timeperframe->denominator != 0) {
			printk("error : numerator is zero\n");
		}
		dprintk(6, "s_parm : try to set auto frame rate\n");
	} else {
		dprintk(6, "s_parm : try to set %d fps\n", timeperframe->denominator / timeperframe->numerator);
	}
	sensor->timeperframe = *timeperframe;
	dprintk(5, "s_parm : [mode:%d][state:%d][fps:%d/%d]\n",
			sensor->mode,
			sensor->state,
			timeperframe->denominator,
			timeperframe->numerator);

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
	struct sr030pc40_sensor *sensor = s->priv;
	int rval;

	dprintk(6, "ioctl_g_ifparm is called...\n");

	rval = sensor->pdata->ifparm(p);
	if (rval)
	{
		return rval;
	}

	p->u.bt656.clock_curr = SR030PC40_XCLK;

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
	struct sr030pc40_sensor *sensor = s->priv;

	dprintk(6, "ioctl_g_priv is called...\n");

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
	struct sr030pc40_sensor* sensor = s->priv;

	dprintk(6, "ioctl_enum_framesizes called...\n");   

	if (sensor->state == SR030PC40_STATE_CAPTURE) {    
		//dprintk(7, "Size enumeration for image capture size = %d\n", sensor->capture_size);

		frms->index = sensor->capture_size;
		frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		frms->discrete.width = sr030pc40_image_sizes[sensor->capture_size].width;
		frms->discrete.height = sr030pc40_image_sizes[sensor->capture_size].height;        
	} else {
		//dprintk(7, "Size enumeration for image preview size = %d\n", sensor->preview_size);

		frms->index = sensor->preview_size;
		frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		frms->discrete.width = sr030pc40_preview_sizes[sensor->preview_size].width;
		frms->discrete.height = sr030pc40_preview_sizes[sensor->preview_size].height;        
	}

	dprintk(7, "framesizes width : %d\n", frms->discrete.width); 
	dprintk(7, "framesizes height : %d\n", frms->discrete.height); 

	return 0;
}

static int ioctl_enum_frameintervals(struct v4l2_int_device *s, struct v4l2_frmivalenum *frmi)
{
	dprintk(6, "ioctl_enum_frameintervals \n"); 
	dprintk(7, "ioctl_enum_frameintervals numerator : %d\n", frmi->discrete.numerator); 
	dprintk(7, "ioctl_enum_frameintervals denominator : %d\n", frmi->discrete.denominator); 

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
	struct sr030pc40_sensor *sensor = s->priv;
	int err = 0;

	dprintk(6, "ioctl_s_power is called......ON=%x, detect= %x\n", on, sensor->detect);

	sensor->pdata->power_set(on);

	switch(on)
	{
	case V4L2_POWER_ON:
		{
			dprintk(5, "pwr on-----!\n");

#if defined(SR030PC40_USE_GPIO_I2C)
			cam_i2c_client = omap_gpio_i2c_init(OMAP_GPIO_CAM_I2C_SDA,
					OMAP_GPIO_CAM_I2C_SCL,
					SR030PC40_I2C_ADDR,
					400);
			if(cam_i2c_client == NULL) {
				dprintk(4, "omap_gpio_i2c_init failed!\n");
				return 0;
			}
			else 
			{
				dprintk(6, "gpio_i2c init success (sda:%d, scl:%d, addr:%d, %dHz)\n", 
						OMAP_GPIO_CAM_I2C_SDA, 
						OMAP_GPIO_CAM_I2C_SCL,
						SR030PC40_I2C_ADDR, 
						400 );
			}

#endif 
			err = sr030pc40_detect(cam_i2c_client);
			if (err) 
			{
				printk(SR030PC40_MOD_NAME "Unable to detect " SR030PC40_DRIVER_NAME " sensor\n");
				sensor->pdata->power_set(V4L2_POWER_OFF);
				return err;
			}

			/* Make the default detect */
			sensor->detect = SENSOR_DETECTED;     
			//set to the defalut
			sensor->effect = SR030PC40_EFFECT_OFF;
			sensor->wb     = SR030PC40_WB_AUTO;
			sensor->ev     = SR030PC40_EV_DEFAULT;
			sensor->contrast = SR030PC40_CONTRAST_DEFAULT;
			//sensor->scene =     SR030PC40_SCENE_OFF;
			//sensor->photometry= SR030PC40_PHOTOMETRY_CENTER;
			//sensor->iso  =      SR030PC40_ISO_AUTO;
			sensor->zoom =      SR030PC40_ZOOM_1P00X;
			/* Make the state init */
			sr030pc40_curr_state = SR030PC40_STATE_INVALID;
		}
		break;

	case V4L2_POWER_RESUME:
		{
			dprintk(7, "pwr resume-----!\n");
		}  
		break;

	case V4L2_POWER_STANDBY:
		{
			dprintk(7, "pwr stanby-----!\n");
		}
		break;

	case V4L2_POWER_OFF:
		{
			dprintk(5, "pwr off-----!\n");
#if defined(SR030PC40_USE_GPIO_I2C)
			if(cam_i2c_client)
				omap_gpio_i2c_deinit(cam_i2c_client);
#endif

			/* Make the default detect */
			sensor->detect = SENSOR_NOT_DETECTED;  

			/* Make the state init */
			sr030pc40_pre_state = SR030PC40_STATE_INVALID;      
			if (enable_sdcard_tune)
				UnLoad16BitTuningFile();
		}
		break;
	}

	return err;
}



static int ioctl_g_exif(struct v4l2_int_device *s, struct v4l2_exif *exif)
{
	struct sr030pc40_sensor *sensor = s->priv;

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
	struct sr030pc40_sensor *sensor = s->priv;

	dprintk(6, "ioctl_deinit is called...\n");

	sensor->state = SR030PC40_STATE_INVALID; //init problem

	return 0;
}

#define SR030PC40_FIRMWARE_F2U_NAME

/**
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 *
 * Initialize the sensor device (call sr030pc40_configure())
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	struct sr030pc40_sensor *sensor = s->priv;
	int ret = 0;

	sr030pc40_write_regsets = sr030pc40_normal_write_regsets;
	sr030pc40_need_to_initial  = 1;
	dprintk(6, "ioctl_init is called...\n");

	sensor->state = SR030PC40_STATE_INVALID; //init problem

	LoadCamsensorRegSettings();

	return 0;
}

static struct v4l2_int_ioctl_desc sr030pc40_ioctl_desc[] = {
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

static struct v4l2_int_slave sr030pc40_slave = {
	.ioctls = sr030pc40_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(sr030pc40_ioctl_desc),
};

static struct v4l2_int_device sr030pc40_int_device = {
	.module = THIS_MODULE,
	.name = SR030PC40_DRIVER_NAME,
	.priv = &sr030pc40,
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &sr030pc40_slave,
	},
};


/**
 * sr030pc40_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * device.
 */
static int sr030pc40_probe(struct i2c_client *client, const struct i2c_device_id *device)
{
	struct sr030pc40_sensor *sensor = &sr030pc40;

	dprintk(6, "sr030pc40_probe is called...\n");

	if(!client)
	{
		printk("client is NULL\n");
		return 0;
	}

#if !defined(SR030PC40_USE_GPIO_I2C)
	if (i2c_get_clientdata(client))
	{
		printk(SR030PC40_MOD_NAME "can't get i2c client data!!\n");
		return -EBUSY;
	}
#endif

	sensor->pdata = &sr030pc40_platform_data0;

	if (!sensor->pdata) 
	{
		printk(SR030PC40_MOD_NAME "no platform data!!\n");
		return -ENODEV;
	}

	sensor->v4l2_int_device = &sr030pc40_int_device;
#if defined(SR030PC40_USE_GPIO_I2C)
	dummy_i2c_client = client;
#else
	cam_i2c_client = client;
#endif

	/* Make the default capture size VGA */
	sensor->pix.width = 640;		
	sensor->pix.height = 480;		


	/* Make the default capture format V4L2_PIX_FMT_UYVY */
	sensor->pix.pixelformat = V4L2_PIX_FMT_UYVY;

#if !defined(SR030PC40_USE_GPIO_I2C)
	i2c_set_clientdata(client, sensor);
#endif

	if (v4l2_int_device_register(sensor->v4l2_int_device))
	{
		printk(SR030PC40_MOD_NAME "fail to init device register \n");
#if !defined(SR030PC40_USE_GPIO_I2C)
		i2c_set_clientdata(client, NULL);
#endif
	}

	return 0;
}

/**
 * sr030pc40_remove - sensor driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister sensor as an i2c client device and V4L2
 * device.  Complement of sr030pc40_probe().
 */
static int __exit sr030pc40_remove(struct i2c_client *client)
{
#if !defined(SR030PC40_USE_GPIO_I2C)
	struct sr030pc40_sensor *sensor = i2c_get_clientdata(client);
#else 
	struct sr030pc40_sensor *sensor= &sr030pc40;
#endif

	dprintk(6, "sr030pc40_remove is called...\n");

	v4l2_int_device_unregister(sensor->v4l2_int_device);
#if !defined(SR030PC40_USE_GPIO_I2C)
	i2c_set_clientdata(client, NULL);
#endif

	return 0;
}

static const struct i2c_device_id sr030pc40_id[] = {
	{ SR030PC40_DRIVER_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sr030pc40_id);


static struct i2c_driver SR030PC40sensor_i2c_driver = {
	.driver = {
		.name = SR030PC40_DRIVER_NAME,
	},
	.probe = sr030pc40_probe,
	.remove = __exit_p(sr030pc40_remove),
	.id_table = sr030pc40_id,
};

/**
 * sr030pc40_sensor_init - sensor driver module_init handler
 *
 * Registers driver as an i2c client driver.  Returns 0 on success,
 * error code otherwise.
 */
static int __init sr030pc40_sensor_init(void)
{
	int err;

	dprintk(6, "sr030pc40_sensor_init is called...\n");

	err = i2c_add_driver(&SR030PC40sensor_i2c_driver);
	if (err) 
	{
		printk(SR030PC40_MOD_NAME "Failed to register" SR030PC40_DRIVER_NAME ".\n");
		return err;
	}

	return 0;
}

module_init(sr030pc40_sensor_init);

/**
 * SR030PC40sensor_cleanup - sensor driver module_exit handler
 *
 * Unregisters/deletes driver as an i2c client driver.
 * Complement of sr030pc40_sensor_init.
 */
static void __exit SR030PC40sensor_cleanup(void)
{
	i2c_del_driver(&SR030PC40sensor_i2c_driver);
}
module_exit(SR030PC40sensor_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SR030PC40 camera sensor driver");
