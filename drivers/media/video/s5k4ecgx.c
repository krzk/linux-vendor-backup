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
 * modules/camera/s5k4ecgx.c
 *
 * s5k4ecgx sensor driver source file
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
#include "s5k4ecgx_tune.h"
#include "s5k4ecgx.h"
#include "Camsensor_tunner_32bits_reg.h"

#undef S5K4ECGX_USE_GPIO_I2C	

#if !defined(S5K4ECGX_USE_GPIO_I2C)
#define CAM_I2C_CLIENT	struct i2c_client
#else
#define CAM_I2C_CLIENT	OMAP_GPIO_I2C_CLIENT
#endif

#if defined(S5K4ECGX_USE_GPIO_I2C)
#include <plat/i2c-omap-gpio.h>
static struct i2c_client *dummy_i2c_client;
#endif

#if (CAM_S5K4ECGX_DBG_MSG)
#include "dprintk.h"
#else
#define dprintk(x, y...)
#endif

#define S5K4ECGX_USE_BURSTMODE	1

static int init_jiffies= 0;

static int use_i2c_bus_unlock_timer = 0;

static CAM_I2C_CLIENT *cam_i2c_client;
static int pwr_on_wait_msec = 5;
module_param(pwr_on_wait_msec, int, 0644);

static int pwr_off_wait_msec = 700;
module_param(pwr_off_wait_msec, int, 0644);

static int dtp_on= 0;
module_param(dtp_on, int, 0644);

static int enable_sdcard_tune = 0;
module_param(enable_sdcard_tune, int, 0644);

static int debug = 5;
module_param(debug, int, 0644);

static int init_delay = 750;
module_param(init_delay, int, 0644);

static int init_delay0 = 150;
module_param(init_delay0, int, 0644);

static int print_i2c_msg = 0;
module_param(print_i2c_msg, int, 0644);

#ifdef S5K4ECGX_USE_BURSTMODE	
static int burst_mode= 1;
module_param(burst_mode, int, 0644);
#endif

static int init_skip_frame= 1;
module_param(init_skip_frame, int, 0644);

static int capture_skip_frame= 1;
module_param(capture_skip_frame, int, 0644);

static char *caminfo = "SLSI_S5K4ECGX_NONE";
module_param(caminfo, charp, 0444);

int low_cap_on = 0;
int night_cap_on;

/* Here we store the status of touch AF; 0 --> not touch AF, 1 --> touch AF */
static int s5k4ecgx_touch_state;
static int s5k4ecgx_initialized = 0;

#define dprintk(level, fmt, arg...) do {		\
	if (debug >= level)				\
	printk(S5K4ECGX_MOD_NAME fmt , ## arg); } while (0)

#define S5K4ECGX_WRITE_LIST(A) do{\
	s5k4ecgx_write_reg32_table(A##_EVT1, ARRAY_SIZE(A##_EVT1)); } while (0)	\


static struct delayed_work camera_esd_check;

static u32 s5k4ecgx_curr_state = S5K4ECGX_STATE_INVALID;
static u32 s5k4ecgx_pre_state = S5K4ECGX_STATE_INVALID;

static struct s5k4ecgx_sensor next_s5k4ecgx = {
	.fps				= S5K4ECGX_INVALID_VALUE,
	.camcorder_size		= S5K4ECGX_INVALID_VALUE,
	.preview_size		= S5K4ECGX_INVALID_VALUE,
	.capture_size		= S5K4ECGX_INVALID_VALUE,
	.focus_mode			= S5K4ECGX_INVALID_VALUE,
	.effect				= S5K4ECGX_INVALID_VALUE,
	.iso				= S5K4ECGX_INVALID_VALUE,
	.photometry			= S5K4ECGX_INVALID_VALUE,
	.ev					= S5K4ECGX_INVALID_VALUE,
	.wdr				= S5K4ECGX_INVALID_VALUE,
	.contrast			= S5K4ECGX_INVALID_VALUE,
	.saturation			= S5K4ECGX_INVALID_VALUE,
	.sharpness			= S5K4ECGX_INVALID_VALUE,
	.wb					= S5K4ECGX_INVALID_VALUE,
	.scene				= S5K4ECGX_INVALID_VALUE,
	.jpeg_quality		= S5K4ECGX_INVALID_VALUE,
};

static struct s5k4ecgx_sensor s5k4ecgx = {
	.timeperframe = {
		.numerator	= 0,
		.denominator	= 0,
	},
	.fw_version			= 0,
	.check_dataline			= 0,
	.ae_lock			= S5K4ECGX_AE_UNLOCK,
	.awb_lock			= S5K4ECGX_AWB_UNLOCK,
	.fps				= 0,
	.state				= S5K4ECGX_STATE_PREVIEW,
	.mode				= S5K4ECGX_MODE_CAMERA,
	.camcorder_size			= S5K4ECGX_INVALID_VALUE,
	.preview_size			= S5K4ECGX_PREVIEW_SIZE_640_480,
	.capture_size			= S5K4ECGX_CAPTURE_SIZE_2560_1920,
	.detect				= SENSOR_NOT_DETECTED,
	.effect				= S5K4ECGX_EFFECT_OFF,
	.iso				= S5K4ECGX_ISO_AUTO,
	.photometry			= S5K4ECGX_PHOTOMETRY_CENTER,
	.ev				= S5K4ECGX_EV_DEFAULT,
	.contrast			= S5K4ECGX_CONTRAST_DEFAULT,
	.saturation			= S5K4ECGX_SATURATION_DEFAULT,
	.sharpness			= S5K4ECGX_SHARPNESS_DEFAULT,
	.wb				= S5K4ECGX_WB_AUTO,
	.wdr				= S5K4ECGX_WDR_OFF,		// AUTO CONTRAST
	.scene				= S5K4ECGX_SCENE_OFF,
	.aewb				= S5K4ECGX_AE_UNLOCK,
	.focus_mode			= S5K4ECGX_AF_SET_NORMAL,
	.jpeg_quality			= S5K4ECGX_JPEG_SUPERFINE,
	.zoom				= S5K4ECGX_ZOOM_STEP_0,
	.thumb_offset			= 0,
	.yuv_offset			= 0,
	.jpeg_main_size			= 0,
	.jpeg_main_offset		= 0,
	.jpeg_thumb_size		= 0,
	.jpeg_thumb_offset		= 0,
	.jpeg_postview_offset		= 0,
	.jpeg_capture_w			= JPEG_CAPTURE_WIDTH,
	.jpeg_capture_h			= JPEG_CAPTURE_HEIGHT,
};

struct v4l2_queryctrl s5k4ecgx_ctrl_list[] = {
	{
		.id		= V4L2_CID_SELECT_MODE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "select mode",
		.minimum	= S5K4ECGX_MODE_CAMERA,
		.maximum	= S5K4ECGX_MODE_CAMCORDER,
		.step		= 1,
		.default_value	= S5K4ECGX_MODE_CAMERA,
	},
	{
		.id		= V4L2_CID_SELECT_STATE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "select state",
		.minimum	= S5K4ECGX_STATE_PREVIEW,
		.maximum	= S5K4ECGX_STATE_CAPTURE,
		.step		= 1,
		.default_value	= S5K4ECGX_STATE_PREVIEW,
	},
	{
		.id		= V4L2_CID_ISO,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "ISO",
		.minimum	= S5K4ECGX_ISO_AUTO,
		.maximum	= S5K4ECGX_ISO_400,
		.step		= 1,
		.default_value	= S5K4ECGX_ISO_AUTO,
	},
	{
		.id		= V4L2_CID_BRIGHTNESS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Brightness",
		.minimum	= S5K4ECGX_EV_MINUS_4,
		.maximum	= S5K4ECGX_EV_PLUS_4,
		.step		= 1,
		.default_value	= S5K4ECGX_EV_DEFAULT,
	},
	{
		.id		= V4L2_CID_WB,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "White Balance",
		.minimum	= S5K4ECGX_WB_AUTO,
		.maximum	= S5K4ECGX_WB_FLUORESCENT,
		.step		= 1,
		.default_value	= S5K4ECGX_WB_AUTO,
	},
	{
		.id		= V4L2_CID_EFFECT,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Effect",
		.minimum	= S5K4ECGX_EFFECT_OFF,
		.maximum	= S5K4ECGX_EFFECT_MONO,
		.step		= 1,
		.default_value	= S5K4ECGX_EFFECT_OFF,
	},
	{
		.id		= V4L2_CID_CONTRAST,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Contrast",
		.minimum	= S5K4ECGX_CONTRAST_MINUS_2,
		.maximum	= S5K4ECGX_CONTRAST_PLUS_2,
		.step		= 1,
		.default_value	= S5K4ECGX_CONTRAST_DEFAULT,
	},	
	{
		.id		= V4L2_CID_SATURATION,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Saturation",
		.minimum	= S5K4ECGX_SATURATION_MINUS_2,
		.maximum	= S5K4ECGX_SATURATION_PLUS_2,
		.step		= 1,
		.default_value	= S5K4ECGX_SATURATION_DEFAULT,
	},
	{
		.id		= V4L2_CID_SHARPNESS,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Sharpness",
		.minimum	= S5K4ECGX_SHARPNESS_MINUS_2,
		.maximum	= S5K4ECGX_SHARPNESS_PLUS_2,
		.step		= 1,
		.default_value	= S5K4ECGX_SHARPNESS_DEFAULT,
	},
	{
		.id		= V4L2_CID_SCENE,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Scene",
		.minimum	= S5K4ECGX_SCENE_OFF,
		.maximum	= S5K4ECGX_SCENE_NIGHT_OFF,
		.step		= 1,
		.default_value	= S5K4ECGX_SCENE_OFF,
	},
	{
		.id		= V4L2_CID_PHOTOMETRY,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Photometry",
		.minimum	= S5K4ECGX_PHOTOMETRY_MATRIX,
		.maximum	= S5K4ECGX_PHOTOMETRY_SPOT,
		.step		= 1,
		.default_value	= S5K4ECGX_PHOTOMETRY_MATRIX,
	},
	{
		.id		= V4L2_CID_AEWB,
		.type		= V4L2_CTRL_TYPE_INTEGER,
		.name		= "Auto Exposure/Auto White Balance",
		.minimum	= S5K4ECGX_AE_LOCK,
		.maximum	= S5K4ECGX_AWB_UNLOCK,
		.step		= 1,
		.default_value	= S5K4ECGX_AE_UNLOCK,
	},
	{
		.id            = V4L2_CID_ZOOM,
		.type          = V4L2_CTRL_TYPE_INTEGER,
		.name          = "Pseudo Zoom",
		.minimum       = S5K4ECGX_ZOOM_STEP_0,
		.maximum       = S5K4ECGX_ZOOM_STEP_8,
		.step          = 1,
		.default_value = S5K4ECGX_ZOOM_STEP_0,
	},
};
#define NUM_S5K4ECGX_CONTROL ARRAY_SIZE(s5k4ecgx_ctrl_list)

/* list of image formats supported by s5k4ecgx sensor */
const static struct v4l2_fmtdesc s5k4ecgx_formats[] = {
	{
		.index		= 0,
		.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.description	= "JPEG + Postview",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
	},
};
#define NUM_S5K4ECGX_FORMATS ARRAY_SIZE(s5k4ecgx_formats)

extern struct s5k4ecgx_platform_data s5k4ecgx_platform_data0;

#define CAMSENSOR_REGSET_INITIALIZE(ArrReg, S5K4ECGXArrReg) \
{\
	ArrReg.reg32 = S5K4ECGXArrReg;			\
	ArrReg.num = ARRAY_SIZE(S5K4ECGXArrReg);	\
	ArrReg.nDynamicLoading = 0;			\
}

#define	I2C_RW_WRITE	0
#define	I2C_RW_READ	1

#if !defined(S5K4ECGX_USE_GPIO_I2C)
static int s5k4ecgx_write_word(struct i2c_client *client, u16 subaddr, u16 val)
{
	int retry = 0;
	int ret = 0;
	unsigned char buf[4];
	struct i2c_msg msg = { client->addr, I2C_RW_WRITE, 4, buf };

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);
	buf[2] = (val >> 8);
	buf[3] = (val & 0xFF);

	if (print_i2c_msg) {
		printk("[0x%02X%02X, 0x%02X%02X]\n", buf[0], buf[1], buf[2], buf[3]);
	}

	for (retry=0; retry < 5; retry++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1) {
			break;
		} else if (ret == -ETIMEDOUT) {
			dprintk(3, "[%s]Error : i2c_transfer(TIMEDOUT)\n", __func__);
			break;
		} else {
			dprintk(3, "[%s]Error : i2c_transfer(%d)\n", __func__, ret);
		}
		dprintk(3, "[%s]mdelay 10 msec\n", __func__); mdelay(10);
		dprintk(3, "[%s]retry(%d) i2c_transfer\n", __func__, retry);
	}

	if (ret == 1) ret = 0;
	else ret = -EIO;

	return ret;
}

static int s5k4ecgx_read_word(struct i2c_client *client, u16 subaddr, u16 *data)
{
	int retry = 0;
	int ret;
	u8 buf[2];
	struct i2c_msg msg = { client->addr, I2C_RW_WRITE, 2, buf };

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);

	for (retry=0; retry < 5; retry++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1) {
			break;
		} else if (ret == -ETIMEDOUT) {
			dprintk(3, "[%s]Error : i2c_transfer(TIMEDOUT)\n", __func__);
			break;
		} else {
			dprintk(3, "[%s]Error : i2c_transfer(%d)\n", __func__, ret);
		}
		dprintk(3, "[%s]mdelay 10 msec\n", __func__); mdelay(10);
		dprintk(3, "[%s]retry(%d) i2c_transfer\n", __func__, retry);
	}
	if (unlikely(ret != 1))
		goto error;

	msg.flags = I2C_RW_READ;

	for (retry=0; retry < 5; retry++) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1) {
			break;
		} else if (ret == -ETIMEDOUT) {
			dprintk(3, "[%s]Error : i2c_transfer(TIMEDOUT)\n", __func__);
			break;
		} else {
			dprintk(3, "[%s]Error : i2c_transfer(%d)\n", __func__, ret);
		}

		dprintk(3, "[%s]mdelay 5 msec\n", __func__); mdelay(5);
		dprintk(3, "[%s]retry(%d) i2c_transfer\n", __func__, retry);
	}
	if (unlikely(ret != 1))
		goto error;

	*data = ((buf[0] << 8) | buf[1]);

error:
	if (ret == 1) ret = 0;
	else ret = -EIO;

	return ret;
}
#else //(S5K4ECGX_USE_GPIO_I2C)
static int s5k4ecgx_write_word(CAM_I2C_CLIENT *client, u16 subaddr, u16 val)
{
	OMAP_GPIO_I2C_WR_DATA i2c_wr_param;
	u8 addr[2];
	u8 buf[2];
	int ret;

	if (subaddr == 0xffff) {
		mdelay(val);
		return 0;
	}

	addr[0] = (subaddr & 0xFF);
	addr[1] = (subaddr >> 8);
	buf[0] = (val >> 8);
	buf[1] = (val & 0xFF);

	i2c_wr_param.reg_len = 2;
	i2c_wr_param.reg_addr = addr;
	i2c_wr_param.wdata_len = 2;
	i2c_wr_param.wdata = buf;
	ret = omap_gpio_i2c_write(client, &i2c_wr_param);
	if (unlikely(ret == -EIO))
		return ret;

	return 0;
}

static int s5k4ecgx_read_word(CAM_I2C_CLIENT *client, u16 subaddr, u16 *data)
{
	OMAP_GPIO_I2C_RD_DATA i2c_rd_param;
	u8 buf[2];
	u8 addr[2];
	int ret;

	if (unlikely(!client)) {
		printk("%s: error client is null\n", __func__);
		return 0;
	}

	addr[0] = (subaddr & 0xFF);
	addr[1] = (subaddr >> 8);

	i2c_rd_param.reg_len = 2;
	i2c_rd_param.reg_addr = addr;
	i2c_rd_param.rdata_len = 2;
	i2c_rd_param.rdata = buf;

	ret = omap_gpio_i2c_read(client, &i2c_rd_param);
	if (unlikely(ret == -EIO))
		goto error;

	*data = ((buf[0] << 8) | buf[1]);
	if (print_i2c_msg) {
		dprintk(7, "read : 0x%04X\n", *data);
	}

error:
	return ret;
}
#endif

static int (*s5k4ecgx_write_regsets)(CAM_REG32_PACKAGE_T *pArrReg);

static int s5k4ecgx_write_reg32_table(const u32 reg32[], int nr_arr)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
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
		addr = (reg32[i] >> 16) & 0xFFFF;
		value =(reg32[i] 		& 0xFFFF);

		err = s5k4ecgx_write_word(client, addr, value);
		if (unlikely(err < 0 )) {
			dprintk(2, "%s: register set failed\n", __func__);
			return -ENODEV;
		}
	}

	return 0;
}




static int s5k4ecgx_normal_write_regsets(CAM_REG32_PACKAGE_T *pArrReg) 
{
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int i = 0;
	int err = 0;

	if (unlikely(!client)) {
		printk("%s: error client is null\n", __func__);
		return -ENODEV;
	}

	for(i=0; i<pArrReg->num; i++) {
		err = s5k4ecgx_write_word(client, pArrReg->reg32[i].addr, pArrReg->reg32[i].value);
		if (unlikely(err < 0 )) {
			dprintk(2, "%s: register set failed\n", __func__);
			return -ENODEV;
		}
	}

	return 0;
}


static int s5k4ecgx_wait_1_frame(void)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int msec;

	if ((sensor->scene == S5K4ECGX_SCENE_NIGHT) || 
			(sensor->scene == S5K4ECGX_SCENE_FIRE)) {
		msec = 250;
	} else {
		msec = 100;
	}
	dprintk(5, "wait %d msec\n", msec);
	mdelay(msec);

	return msec;
}
#ifdef S5K4ECGX_USE_BURSTMODE
#define BURST_MODE_BUFFER_MAX_SIZE 2700

unsigned char s5k4ecgx_buf_for_burstmode[BURST_MODE_BUFFER_MAX_SIZE];
static int s5k4ecgx_burst_write_regsets(CAM_REG32_PACKAGE_T *pArrReg) 
{
	int i = 0;
	int idx = 0;
	int err = -EINVAL;
	int retry = 0;
	int j = 0;
	unsigned short subaddr=0,next_subaddr=0;
	unsigned short value=0;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	struct i2c_msg msg = { client->addr, I2C_RW_WRITE, 0, s5k4ecgx_buf_for_burstmode };

I2C_RETRY:
	idx = 0;
	for (i = 0; i < pArrReg->num; i++) 
	{
		if (idx > (BURST_MODE_BUFFER_MAX_SIZE-10))
		{
			dprintk(2, "s5k4ecgx_buf_for_burstmode overflow will occur!!!\n");
			return err;
		}

		subaddr = pArrReg->reg32[i].addr; //address
		if (subaddr == 0x0F12) next_subaddr = pArrReg->reg32[i+1].addr; //address
		value = pArrReg->reg32[i].value; //value

		switch(subaddr)
		{
		case 0x0F12 :
			// make and fill buffer for burst mode write
			if (idx == 0) {
				s5k4ecgx_buf_for_burstmode[idx++] = 0x0F;
				s5k4ecgx_buf_for_burstmode[idx++] = 0x12;
			}
			s5k4ecgx_buf_for_burstmode[idx++] = value>> 8;
			s5k4ecgx_buf_for_burstmode[idx++] = value & 0xFF;
			//write in burstmode	
			if (next_subaddr != 0x0F12)
			{
				msg.len = idx;
				if (unlikely(print_i2c_msg)) {
					printk("DATA{{");
					for(j=0; j<msg.len;) {
						if (!(j % 12)) printk("\n");
						if ((msg.len - j) == 1) {
							printk("[0x%02X]", s5k4ecgx_buf_for_burstmode[j++]);
						} else {
							printk("[0x%02X%02X]", s5k4ecgx_buf_for_burstmode[j], s5k4ecgx_buf_for_burstmode[j+1]);
							j += 2;
						}
					}
					printk("}}\n");
				}
				err = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
				idx=0;
			}
			break;
		case 0xFFFF :
			break;
		default:
			// Set Address
			idx=0;
			err = s5k4ecgx_write_word(client, subaddr, value);
			break;
		}
	}
	if (unlikely(err < 0)) {
		dprintk(3, "%s: register set failed, try again(%d)\n", __func__, retry);
		if ((retry++) < 10) goto I2C_RETRY;
		return err;
	}
	dprintk(7, "s5k4ecgx_sensor_burst_write End!\n");

	return 0;
}
#endif

void s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(int bit, int set)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int REG_TC_DBG_AutoAlgEnBits = 0;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	/* Read 04E6 */
	s5k4ecgx_write_word(client, 0x002C, 0x7000);
	s5k4ecgx_write_word(client, 0x002E, 0x04E6);
	s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&REG_TC_DBG_AutoAlgEnBits);

	if (bit == 3 && set == true) {
		if (REG_TC_DBG_AutoAlgEnBits & 0x8 == 1) return;
		s5k4ecgx_wait_1_frame();
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits | 0x8;
		s5k4ecgx_write_word(client, 0x0028, 0x7000);
		s5k4ecgx_write_word(client, 0x002A, 0x04E6);
		s5k4ecgx_write_word(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	} else if (bit == 3 && set == false) {
		if (REG_TC_DBG_AutoAlgEnBits & 0x8 == 0)return;
		s5k4ecgx_wait_1_frame();
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits & 0xFFF7;
		s5k4ecgx_write_word(client, 0x0028, 0x7000);
		s5k4ecgx_write_word(client, 0x002A, 0x04E6);
		s5k4ecgx_write_word(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	} else if (bit == 5 && set == true) {
		if (REG_TC_DBG_AutoAlgEnBits & 0x20 == 1)return;
		s5k4ecgx_wait_1_frame();
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits | 0x20;
		s5k4ecgx_write_word(client, 0x0028, 0x7000);
		s5k4ecgx_write_word(client, 0x002A, 0x04E6);
		s5k4ecgx_write_word(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	} else if (bit == 5 && set == false) {
		if (REG_TC_DBG_AutoAlgEnBits & 0x20 == 0)return;
		s5k4ecgx_wait_1_frame();
		REG_TC_DBG_AutoAlgEnBits = REG_TC_DBG_AutoAlgEnBits & 0xFFDF;
		s5k4ecgx_write_word(client, 0x0028, 0x7000);
		s5k4ecgx_write_word(client, 0x002A, 0x04E6);
		s5k4ecgx_write_word(client, 0x0F12, REG_TC_DBG_AutoAlgEnBits);
	}

	return;
}

static void LoadCamsensorRegSettings()
{
	dprintk(6, "LoadCamsensorRegSettings)+\n");
	//CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_INIT_ARM,			S5K4ECGX_TUNING_INIT_ARM);
	//CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_INIT,				S5K4ECGX_TUNING_INIT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_EFFECT_OFF,			S5K4ECGX_TUNING_EFFECT_OFF);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_EFFECT_MONO,			S5K4ECGX_TUNING_EFFECT_MONO);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_EFFECT_SEPIA,			S5K4ECGX_TUNING_EFFECT_SEPIA);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_EFFECT_NEGATIVE,		S5K4ECGX_TUNING_EFFECT_NEGATIVE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_EFFECT_AQUA,			S5K4ECGX_TUNING_EFFECT_AQUA);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_EFFECT_SKETCH,			S5K4ECGX_TUNING_EFFECT_SKETCH);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_WHITE_BALANCE_AUTO,		S5K4ECGX_TUNING_WHITE_BALANCE_AUTO);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_WHITE_BALANCE_DAYLIGHT,	S5K4ECGX_TUNING_WHITE_BALANCE_DAYLIGHT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_WHITE_BALANCE_CLOUDY,		S5K4ECGX_TUNING_WHITE_BALANCE_CLOUDY);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_WHITE_BALANCE_FLUORESCENT,	S5K4ECGX_TUNING_WHITE_BALANCE_FLUORESCENT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_WHITE_BALANCE_INCANDESCENT,	S5K4ECGX_TUNING_WHITE_BALANCE_INCANDESCENT);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_WDR_ON,			S5K4ECGX_TUNING_WDR_ON);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_WDR_OFF,			S5K4ECGX_TUNING_WDR_OFF);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_M_4,		S5K4ECGX_TUNING_BRIGHTNESS_M_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_M_3,		S5K4ECGX_TUNING_BRIGHTNESS_M_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_M_2,		S5K4ECGX_TUNING_BRIGHTNESS_M_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_M_1,		S5K4ECGX_TUNING_BRIGHTNESS_M_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_0,			S5K4ECGX_TUNING_BRIGHTNESS_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_P_1,		S5K4ECGX_TUNING_BRIGHTNESS_P_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_P_2,		S5K4ECGX_TUNING_BRIGHTNESS_P_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_P_3,		S5K4ECGX_TUNING_BRIGHTNESS_P_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_BRIGHTNESS_P_4,		S5K4ECGX_TUNING_BRIGHTNESS_P_4);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CONTRAST_M_2,			S5K4ECGX_TUNING_CONTRAST_M_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CONTRAST_M_1,			S5K4ECGX_TUNING_CONTRAST_M_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CONTRAST_0,			S5K4ECGX_TUNING_CONTRAST_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CONTRAST_P_1,			S5K4ECGX_TUNING_CONTRAST_P_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CONTRAST_P_2,			S5K4ECGX_TUNING_CONTRAST_P_2);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SATURATION_M_2,		S5K4ECGX_TUNING_SATURATION_M_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SATURATION_M_1,		S5K4ECGX_TUNING_SATURATION_M_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SATURATION_0,			S5K4ECGX_TUNING_SATURATION_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SATURATION_P_1,		S5K4ECGX_TUNING_SATURATION_P_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SATURATION_P_2,		S5K4ECGX_TUNING_SATURATION_P_2);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SHARPNESS_M_2,			S5K4ECGX_TUNING_SHARPNESS_M_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SHARPNESS_M_1,			S5K4ECGX_TUNING_SHARPNESS_M_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SHARPNESS_0,			S5K4ECGX_TUNING_SHARPNESS_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SHARPNESS_P_1,			S5K4ECGX_TUNING_SHARPNESS_P_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SHARPNESS_P_2,			S5K4ECGX_TUNING_SHARPNESS_P_2);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_OFF,			S5K4ECGX_TUNING_SCENE_OFF);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_PORTRAIT,		S5K4ECGX_TUNING_SCENE_PORTRAIT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_LANDSCAPE,		S5K4ECGX_TUNING_SCENE_LANDSCAPE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_SPORTS,			S5K4ECGX_TUNING_SCENE_SPORTS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_PARTY,			S5K4ECGX_TUNING_SCENE_PARTY);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_BEACH,			S5K4ECGX_TUNING_SCENE_BEACH);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_SUNSET,			S5K4ECGX_TUNING_SCENE_SUNSET);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_DAWN,			S5K4ECGX_TUNING_SCENE_DAWN);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_FALL,			S5K4ECGX_TUNING_SCENE_FALL);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_NIGHT,			S5K4ECGX_TUNING_SCENE_NIGHT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_BACKLIGHT,		S5K4ECGX_TUNING_SCENE_BACKLIGHT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_FIRE,			S5K4ECGX_TUNING_SCENE_FIRE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_TEXT,			S5K4ECGX_TUNING_SCENE_TEXT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_SCENE_CANDLE,			S5K4ECGX_TUNING_SCENE_CANDLE);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_METERING_MATRIX,		S5K4ECGX_TUNING_METERING_MATRIX);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_METERING_SPOT,			S5K4ECGX_TUNING_METERING_SPOT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_METERING_CENTER,		S5K4ECGX_TUNING_METERING_CENTER);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_ISO_AUTO,			S5K4ECGX_TUNING_ISO_AUTO);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_ISO_50,			S5K4ECGX_TUNING_ISO_50);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_ISO_100,			S5K4ECGX_TUNING_ISO_100);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_ISO_200,			S5K4ECGX_TUNING_ISO_200);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_ISO_400,			S5K4ECGX_TUNING_ISO_400);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_RETURN_TO_PREVIEW,			S5K4ECGX_TUNING_RETURN_TO_PREVIEW);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_HIGH_SNAPSHOT,			S5K4ECGX_TUNING_HIGH_SNAPSHOT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_NORMAL_SNAPSHOT,		S5K4ECGX_TUNING_NORMAL_SNAPSHOT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_LOWLIGHT_SNAPSHOT,		S5K4ECGX_TUNING_LOWLIGHT_SNAPSHOT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_NIGHT_SNAPSHOT,		S5K4ECGX_TUNING_NIGHT_SNAPSHOT);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_START,			S5K4ECGX_TUNING_CAPTURE_START);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_5_FPS,				S5K4ECGX_TUNING_5_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_7_FPS,				S5K4ECGX_TUNING_7_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_10_FPS,			S5K4ECGX_TUNING_10_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_15_FPS,			S5K4ECGX_TUNING_15_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_30_FPS,			S5K4ECGX_TUNING_30_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AUTO15_FPS,			S5K4ECGX_TUNING_AUTO15_FPS);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AUTO30_FPS,			S5K4ECGX_TUNING_AUTO30_FPS);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AE_LOCK,			S5K4ECGX_TUNING_AE_LOCK);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AE_UNLOCK,			S5K4ECGX_TUNING_AE_UNLOCK);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AWB_LOCK,			S5K4ECGX_TUNING_AWB_LOCK);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AWB_UNLOCK,			S5K4ECGX_TUNING_AWB_UNLOCK);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_DTP_ON,			S5K4ECGX_TUNING_DTP_ON);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_DTP_OFF,			S5K4ECGX_TUNING_DTP_OFF);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAMCORDER_SIZE_176_144,	S5K4ECGX_TUNING_CAMCORDER_SIZE_176_144);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAMCORDER_SIZE_320_240,	S5K4ECGX_TUNING_CAMCORDER_SIZE_320_240);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAMCORDER_SIZE_352_288,	S5K4ECGX_TUNING_CAMCORDER_SIZE_352_288);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAMCORDER_SIZE_640_480,	S5K4ECGX_TUNING_CAMCORDER_SIZE_640_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAMCORDER_SIZE_720_480,	S5K4ECGX_TUNING_CAMCORDER_SIZE_720_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAMCORDER_SIZE_800_480,	S5K4ECGX_TUNING_CAMCORDER_SIZE_800_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAMCORDER_SIZE_1280_720,	S5K4ECGX_TUNING_CAMCORDER_SIZE_1280_720);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_PREVIEW_SIZE_144_176,		S5K4ECGX_TUNING_PREVIEW_SIZE_144_176);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_PREVIEW_SIZE_176_144,		S5K4ECGX_TUNING_PREVIEW_SIZE_176_144);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_PREVIEW_SIZE_352_288,		S5K4ECGX_TUNING_PREVIEW_SIZE_352_288);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_PREVIEW_SIZE_320_240,		S5K4ECGX_TUNING_PREVIEW_SIZE_320_240);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_PREVIEW_SIZE_640_480,		S5K4ECGX_TUNING_PREVIEW_SIZE_640_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_PREVIEW_SIZE_720_480,		S5K4ECGX_TUNING_PREVIEW_SIZE_720_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_PREVIEW_SIZE_800_480,		S5K4ECGX_TUNING_PREVIEW_SIZE_800_480);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_640_480,		S5K4ECGX_TUNING_CAPTURE_SIZE_640_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_800_480,		S5K4ECGX_TUNING_CAPTURE_SIZE_800_480);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_1600_960,		S5K4ECGX_TUNING_CAPTURE_SIZE_1600_960);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_1600_1200,	S5K4ECGX_TUNING_CAPTURE_SIZE_1600_1200);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_2048_1232,	S5K4ECGX_TUNING_CAPTURE_SIZE_2048_1232);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_2048_1536,	S5K4ECGX_TUNING_CAPTURE_SIZE_2048_1536);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_2560_1536,	S5K4ECGX_TUNING_CAPTURE_SIZE_2560_1536);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_CAPTURE_SIZE_2560_1920,	S5K4ECGX_TUNING_CAPTURE_SIZE_2560_1920);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_JPEG_QUALITY_SUPERFINE,	S5K4ECGX_TUNING_JPEG_QUALITY_SUPERFINE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_JPEG_QUALITY_FINE,		S5K4ECGX_TUNING_JPEG_QUALITY_FINE);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_JPEG_QUALITY_NORMAL,		S5K4ECGX_TUNING_JPEG_QUALITY_NORMAL);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_FLASH_INIT,			S5K4ECGX_TUNING_FLASH_INIT);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_LOW_CAP_ON,			S5K4ECGX_TUNING_LOW_CAP_ON);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_LOW_CAP_OFF,			S5K4ECGX_TUNING_LOW_CAP_OFF);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_NIGHT_CAP_ON,			S5K4ECGX_TUNING_NIGHT_CAP_ON);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_NIGHT_CAP_OFF,			S5K4ECGX_TUNING_NIGHT_CAP_OFF);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_NORMAL_MODE_1,		S5K4ECGX_TUNING_AF_NORMAL_MODE_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_NORMAL_MODE_2,		S5K4ECGX_TUNING_AF_NORMAL_MODE_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_NORMAL_MODE_3,		S5K4ECGX_TUNING_AF_NORMAL_MODE_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_NORMAL_MODE_4,		S5K4ECGX_TUNING_AF_NORMAL_MODE_4);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_MACRO_MODE_1,		S5K4ECGX_TUNING_AF_MACRO_MODE_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_MACRO_MODE_2,		S5K4ECGX_TUNING_AF_MACRO_MODE_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_MACRO_MODE_3,		S5K4ECGX_TUNING_AF_MACRO_MODE_3);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_START,			S5K4ECGX_TUNING_AF_START);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_OFF_1,			S5K4ECGX_TUNING_AF_OFF_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_OFF_2,			S5K4ECGX_TUNING_AF_OFF_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_AF_OFF_3,			S5K4ECGX_TUNING_AF_OFF_3);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_0,			S5K4ECGX_X1_25_ZOOM_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_1,			S5K4ECGX_X1_25_ZOOM_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_2,			S5K4ECGX_X1_25_ZOOM_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_3,			S5K4ECGX_X1_25_ZOOM_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_4,			S5K4ECGX_X1_25_ZOOM_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_5,			S5K4ECGX_X1_25_ZOOM_5);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_6,			S5K4ECGX_X1_25_ZOOM_6);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_7,			S5K4ECGX_X1_25_ZOOM_7);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_25_ZOOM_8,			S5K4ECGX_X1_25_ZOOM_8);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_0,			S5K4ECGX_X1_6_ZOOM_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_1,			S5K4ECGX_X1_6_ZOOM_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_2,			S5K4ECGX_X1_6_ZOOM_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_3,			S5K4ECGX_X1_6_ZOOM_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_4,			S5K4ECGX_X1_6_ZOOM_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_5,			S5K4ECGX_X1_6_ZOOM_5);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_6,			S5K4ECGX_X1_6_ZOOM_6);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_7,			S5K4ECGX_X1_6_ZOOM_7);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_6_ZOOM_8,			S5K4ECGX_X1_6_ZOOM_8);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_0,			S5K4ECGX_X1_77_ZOOM_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_1,			S5K4ECGX_X1_77_ZOOM_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_2,			S5K4ECGX_X1_77_ZOOM_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_3,			S5K4ECGX_X1_77_ZOOM_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_4,			S5K4ECGX_X1_77_ZOOM_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_5,			S5K4ECGX_X1_77_ZOOM_5);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_6,			S5K4ECGX_X1_77_ZOOM_6);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_7,			S5K4ECGX_X1_77_ZOOM_7);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X1_77_ZOOM_8,			S5K4ECGX_X1_77_ZOOM_8);

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_0,			S5K4ECGX_X2_ZOOM_0);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_1,			S5K4ECGX_X2_ZOOM_1);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_2,			S5K4ECGX_X2_ZOOM_2);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_3,			S5K4ECGX_X2_ZOOM_3);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_4,			S5K4ECGX_X2_ZOOM_4);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_5,			S5K4ECGX_X2_ZOOM_5);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_6,			S5K4ECGX_X2_ZOOM_6);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_7,			S5K4ECGX_X2_ZOOM_7);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_X2_ZOOM_8,			S5K4ECGX_X2_ZOOM_8);
	if (enable_sdcard_tune)
		Load32BitTuningFile();

	dprintk(6, "LoadCamsensorRegSettings)-\n");
}


static int s5k4ecgx_detect(CAM_I2C_CLIENT *client)
{
	int ret;
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	dprintk(6, "s5k4ecgx_detect is called...\n");

	if (unlikely(!client)) {
		printk("%s: error client is null\n", __func__);
		return 0;
	}
	/* Start Camera Program */
	u16 ID = 0xFFFF;

	/* Read Firmware Version */
	if (s5k4ecgx_write_word(client, 0x002C, 0x7000)) {
		printk("Error : I2C (line:%d)\n", __LINE__);
		return -EINVAL;
	}
	if (s5k4ecgx_write_word(client, 0x002E, 0x01A6)){
		printk("Error : I2C (line:%d)\n", __LINE__);
		return -EINVAL;
	}

	ret = s5k4ecgx_read_word(client, 0x0F12, &ID);
	sensor->fw_version = ID;
//	printk(S5K4ECGX_MOD_NAME "ret of s5k4ecgx_read_word == 0x%x\n", ret);

	//if (s5k4ecgx_write_word(client, 0x0028, 0x7000)) printk("Error : I2C (line:%d)\n", __LINE__);

	if (ID == 0x0011) {
		//printk(S5K4ECGX_MOD_NAME"=================================\n");
		printk(S5K4ECGX_MOD_NAME"   [5MEGA CAM] vendor_id ID : 0x%04X\n", ID);
		//printk(S5K4ECGX_MOD_NAME"   Sensor is Detected!!!\n");
		//printk(S5K4ECGX_MOD_NAME"=================================\n");
	} else {
		printk(S5K4ECGX_MOD_NAME"-------------------------------------------------\n");
		printk(S5K4ECGX_MOD_NAME"   [5MEGA CAM] sensor detect failure !!\n");
		printk(S5K4ECGX_MOD_NAME"   ID : 0x%04X[ID should be 0x0011]\n", ID);
		printk(S5K4ECGX_MOD_NAME"-------------------------------------------------\n");
		return -EINVAL;
	}	

	return 0;
}


static int s5k4ecgx_check_dataline()
{
	dprintk(5, "s5k4ecgx_check_dataline is called...\n");

	s5k4ecgx_write_regsets(&CAM_REG32SET_DTP_ON);
	return 0;
}

static int s5k4ecgx_check_dataline_stop()
{
	dprintk(5, "s5k4ecgx_check_dataline_stop is called...\n");

	s5k4ecgx_write_regsets(&CAM_REG32SET_DTP_OFF);
	return 0;
}

static int s5k4ecgx_get_lux(int* lux)
{
	unsigned short msb = 0;
	unsigned short lsb = 0;
	int cur_lux = 0;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	s5k4ecgx_write_word(client, 0x002C, 0x7000);
	s5k4ecgx_write_word(client, 0x002E, 0x2C18);//for EVT 1.1
	s5k4ecgx_read_word(client, 0x0F12, (unsigned short *)&lsb);
	s5k4ecgx_read_word(client, 0x0F12, (unsigned short *)&msb);

	cur_lux = ((msb<<16) | lsb);
	*lux = cur_lux;
	dprintk(6, "get_lux : %d lux\n", cur_lux);
	return cur_lux; //this value is under 0x0032 in low light condition 
}


camera_light_status_type s5k4ecgx_check_illuminance_status()
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	u32 lightStatus = 0;
	u16 lightStatus_low_word = 0;
	u16 lightStatus_high_word= 0;
	int err;

	dprintk(5, "s5k4ecgx_check_illuminance_status() \r\n");
	if (unlikely(!client)) {
		printk("%s: error client is null\n", __func__);
		return 0;
	}
	camera_light_status_type LightStatus = CAMERA_SENSOR_LIGHT_STATUS_INVALID;

	err = s5k4ecgx_write_word(client, 0xFCFC, 0xD000);
	err = s5k4ecgx_write_word(client, 0x002C, 0x7000);
	err = s5k4ecgx_write_word(client, 0x002E, 0x2A3C);
	err = s5k4ecgx_read_word(client, 0x0F12, &lightStatus_low_word);
	err = s5k4ecgx_write_word(client, 0x002E, 0x2A3E);
	err = s5k4ecgx_read_word(client, 0x0F12, &lightStatus_high_word);

	lightStatus = lightStatus_low_word  | (lightStatus_high_word << 16);

	printk("s5k4ecgx_check_illuminance_status() : lux_value == 0x%x\n", lightStatus);

	if (err < 0) {
		dprintk(5, "s5k4ecgx_check_illuminance_status - Failed to read a lowlight status \r\n");
		return -EIO;
	}

	if ( lightStatus > 0xFFFE) {		// Highlight Snapshot
		dprintk(6, "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_HIGH \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_HIGH;
	} else if ( lightStatus > 0x0020) {	// Normal Snapshot
		dprintk(6, "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_NORMAL \r\n");
		LightStatus =  CAMERA_SENSOR_LIGHT_STATUS_NORMAL;
	} else {				// Lowlight Snapshot
		dprintk(6, "s5k4ecgx_check_illuminance_status - CAMERA_SENSOR_LIGHT_STATUS_LOW \r\n");
		LightStatus = CAMERA_SENSOR_LIGHT_STATUS_LOW;
	}
	return LightStatus;
}

static int s5k4ecgx_set_effect(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->effect;

	switch (value) {
	case S5K4ECGX_EFFECT_OFF:
		s5k4ecgx_write_regsets(&CAM_REG32SET_EFFECT_OFF);
		dprintk(5, "set_effect : Normal\n");
		break;
	case S5K4ECGX_EFFECT_MONO:
		s5k4ecgx_write_regsets(&CAM_REG32SET_EFFECT_MONO);
		dprintk(5, "set_effect : Blank & White\n");
		break;
	case S5K4ECGX_EFFECT_SEPIA:
		s5k4ecgx_write_regsets(&CAM_REG32SET_EFFECT_SEPIA);
		dprintk(5, "set_effect : Sepia\n");
		break;
	case S5K4ECGX_EFFECT_NEGATIVE:
		s5k4ecgx_write_regsets(&CAM_REG32SET_EFFECT_NEGATIVE);
		dprintk(5, "set_effect : Negative\n");
		break;
	case S5K4ECGX_EFFECT_AQUA:
		s5k4ecgx_write_regsets(&CAM_REG32SET_EFFECT_AQUA);
		dprintk(5, "set_effect : Aqua\n");
		break;
	case S5K4ECGX_EFFECT_SKETCH:
		s5k4ecgx_write_regsets(&CAM_REG32SET_EFFECT_SKETCH);
		dprintk(5, "set_effect : Sketch\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_effect : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}

	sensor->effect = value;
//	dprintk(6, "%s success [effect:%d]\n",__func__, sensor->effect);

	return 0;
}

static int s5k4ecgx_set_iso(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->iso;

	//dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_ISO_AUTO:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(5,1);
		s5k4ecgx_write_regsets(&CAM_REG32SET_ISO_AUTO);
		dprintk(5, "set_iso : Auto\n");
		break;
	case S5K4ECGX_ISO_50:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(5,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_ISO_50);
		dprintk(5, "set_iso : 50\n");
		break;
	case S5K4ECGX_ISO_100:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(5,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_ISO_100);
		dprintk(5, "set_iso : 100\n");
		break;
	case S5K4ECGX_ISO_200:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(5,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_ISO_200);
		dprintk(5, "set_iso : 200\n");
		break;
	case S5K4ECGX_ISO_400:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(5,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_ISO_400);
		dprintk(5, "set_iso : 400\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_iso : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}

	sensor->iso = value;
	//dprintk(6, "%s success [iso:%d]\n",__func__, sensor->iso);

	return 0;
}

static int s5k4ecgx_set_photometry(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->photometry;

	//dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_PHOTOMETRY_MATRIX:
		s5k4ecgx_write_regsets(&CAM_REG32SET_METERING_MATRIX);
		dprintk(5, "set_photometry : matrix\n");
		break;
	case S5K4ECGX_PHOTOMETRY_CENTER:
		s5k4ecgx_write_regsets(&CAM_REG32SET_METERING_CENTER);
		dprintk(5, "set_photometry : center\n");
		break;
	case S5K4ECGX_PHOTOMETRY_SPOT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_METERING_SPOT);
		dprintk(5, "set_photometry : spot\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_photometry : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->photometry = value;
	//dprintk(6, "%s success [photometry:%d]\n",__func__, sensor->photometry);

	return 0;
}

static int s5k4ecgx_set_ev(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->ev;

	//dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_EV_MINUS_4:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_M_4);
		dprintk(5, "set_ev : minus 4\n");
		break;
	case S5K4ECGX_EV_MINUS_3:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_M_3);
		dprintk(5, "set_ev : minus 3\n");
		break;
	case S5K4ECGX_EV_MINUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_M_2);
		dprintk(5, "set_ev : minus 2\n");
		break;
	case S5K4ECGX_EV_MINUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_M_1);
		dprintk(5, "set_ev : minus 1\n");
		break;
	case S5K4ECGX_EV_DEFAULT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_0);
		dprintk(5, "set_ev : default\n");
		break;
	case S5K4ECGX_EV_PLUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_P_1);
		dprintk(5, "set_ev : plus 1\n");
		break;
	case S5K4ECGX_EV_PLUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_P_2);
		dprintk(5, "set_ev : plus 2\n");
		break;
	case S5K4ECGX_EV_PLUS_3:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_P_3);
		dprintk(5, "set_ev : plus 3\n");
		break;
	case S5K4ECGX_EV_PLUS_4:
		s5k4ecgx_write_regsets(&CAM_REG32SET_BRIGHTNESS_P_4);
		dprintk(5, "set_ev : plus 4\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_ev : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->ev = value;
	//dprintk(6, "%s success [ev:%d]\n",__func__, sensor->ev);

	return 0;
}

static int s5k4ecgx_get_zoom(struct v4l2_control *vc)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	u8 readdata[2] = {0x0,};

	dprintk(6, S5K4ECGX_MOD_NAME "s5k4ecgx_get_zoom is called...\n"); 

	/*
	if(s5k4ecgx_write_read_reg(client, 1, Lense_CheckDZoomStatus_List, 2, readdata))
		goto get_zoom_fail;
	vc->value = (u8)(2560/(readdata[0] +1));
	dprintk(CAM_DBG, S5K4ECGX_MOD_NAME "Zoom value... %d \n", vc->value);
	*/

	return 0;

get_zoom_fail:
	printk(S5K4ECGX_MOD_NAME "s5k4ecgx_get_zoom is failed!!!\n");
	return -EINVAL;   
}

static int s5k4ecgx_set_contrast(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->contrast;

	//dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_CONTRAST_MINUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CONTRAST_M_2);
		dprintk(5, "set_contrast : minus 2\n");
		break;
	case S5K4ECGX_CONTRAST_MINUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CONTRAST_M_1);
		dprintk(5, "set_contrast : minus 1\n");
		break;
	case S5K4ECGX_CONTRAST_DEFAULT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CONTRAST_0);
		dprintk(5, "set_contrast : default\n");
		break;
	case S5K4ECGX_CONTRAST_PLUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CONTRAST_P_1);
		dprintk(5, "set_contrast : plus 1\n");
		break;
	case S5K4ECGX_CONTRAST_PLUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CONTRAST_P_2);
		dprintk(5, "set_contrast : plus 2\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_contrast : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->contrast = value;
	//dprintk(6, "%s success [contrast:%d]\n",__func__, sensor->contrast);
	return 0;
}

static int s5k4ecgx_set_saturation(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->saturation;

	//dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_SATURATION_MINUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SATURATION_M_2);
		dprintk(5, "set_saturation : minus 2\n");
		break;
	case S5K4ECGX_SATURATION_MINUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SATURATION_M_1);
		dprintk(5, "set_saturation : minus 1\n");
		break;
	case S5K4ECGX_SATURATION_DEFAULT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SATURATION_0);
		dprintk(5, "set_saturation : default\n");
		break;
	case S5K4ECGX_SATURATION_PLUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SATURATION_P_1);
		dprintk(5, "set_saturation : plus 1\n");
		break;
	case S5K4ECGX_SATURATION_PLUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SATURATION_P_2);
		dprintk(5, "set_saturation : plus 2\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_saturation : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->saturation = value;
	dprintk(6, "%s success [saturation:%d]\n",__func__, sensor->saturation);
	return 0;
}

static int s5k4ecgx_set_sharpness(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->sharpness;

	//dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_SHARPNESS_MINUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SHARPNESS_M_2);
		dprintk(5, "set_sharpness : minus 2\n");
		break;
	case S5K4ECGX_SHARPNESS_MINUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SHARPNESS_M_1);
		dprintk(5, "set_sharpness : minus 1\n");
		break;
	case S5K4ECGX_SHARPNESS_DEFAULT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SHARPNESS_0);
		dprintk(5, "set_sharpness : default\n");
		break;
	case S5K4ECGX_SHARPNESS_PLUS_1:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SHARPNESS_P_1);
		dprintk(5, "set_sharpness : plus 1\n");
		break;
	case S5K4ECGX_SHARPNESS_PLUS_2:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SHARPNESS_P_2);
		dprintk(5, "set_sharpness : plus 2\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_sharpness : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->sharpness = value;
//	dprintk(6, "%s success [sharpness:%d]\n",__func__, sensor->sharpness);
	return 0;
}

static int s5k4ecgx_set_wb(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->wb;

//	dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_WB_AUTO:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(3,1);
		s5k4ecgx_write_regsets(&CAM_REG32SET_WHITE_BALANCE_AUTO);
		dprintk(5, "set_wb : auto\n");
		break;
	case S5K4ECGX_WB_DAYLIGHT:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(3,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_WHITE_BALANCE_DAYLIGHT);
		dprintk(5, "set_wb : daylight\n");
		break;
	case S5K4ECGX_WB_CLOUDY:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(3,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_WHITE_BALANCE_CLOUDY);
		dprintk(5, "set_wb : cloudy\n");
		break;
	case S5K4ECGX_WB_FLUORESCENT:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(3,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_WHITE_BALANCE_FLUORESCENT);
		dprintk(5, "set_wb : fluorescent\n");
		break;
	case S5K4ECGX_WB_INCANDESCENT:
		s5k4ecgx_set_REG_TC_DBG_AutoAlgEnBits(3,0);
		s5k4ecgx_write_regsets(&CAM_REG32SET_WHITE_BALANCE_INCANDESCENT);
		dprintk(5, "set_wb : incandescent\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_wb : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->wb = value;
//	dprintk(6, "%s success [wb:%d]\n",__func__, sensor->wb);
	return 0;
}

static int s5k4ecgx_set_wdr(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->wdr;

	static int prev_wdr = S5K4ECGX_INVALID_VALUE;
	static int contrast = S5K4ECGX_INVALID_VALUE;
	static int saturation = S5K4ECGX_INVALID_VALUE;
	static int sharpness = S5K4ECGX_INVALID_VALUE;

//	dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_WDR_ON:	//S5K4ECGX_AUTO_CONTRAST_ON :
		if (prev_wdr != S5K4ECGX_WDR_ON) {
			// save values
			contrast = sensor->contrast;
			saturation = sensor->saturation;
			sharpness = sensor->sharpness;
		}
		s5k4ecgx_set_contrast(S5K4ECGX_CONTRAST_DEFAULT);
		s5k4ecgx_set_saturation(S5K4ECGX_SATURATION_DEFAULT);
		s5k4ecgx_set_sharpness(S5K4ECGX_SHARPNESS_DEFAULT);
		s5k4ecgx_write_regsets(&CAM_REG32SET_WDR_ON);
		dprintk(5, "set_auto_contrast : ON\n");
		break;
	case S5K4ECGX_WDR_OFF:	//S5K4ECGX_AUTO_CONTRAST_OFF
		s5k4ecgx_write_regsets(&CAM_REG32SET_WDR_OFF);
		s5k4ecgx_set_contrast(contrast);
		s5k4ecgx_set_saturation(saturation);
		s5k4ecgx_set_sharpness(sharpness);
//		if (sensor->contrast != S5K4ECGX_CONTRAST_DEFAULT) s5k4ecgx_set_contrast(sensor->contrast);
//		if (sensor->saturation != S5K4ECGX_SATURATION_DEFAULT) s5k4ecgx_set_saturation(sensor->saturation);
//		if (sensor->sharpness != S5K4ECGX_SHARPNESS_DEFAULT) s5k4ecgx_set_sharpness(sensor->sharpness);
		dprintk(5, "set_auto_contrast : OFF\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_auto_contrast : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}

	sensor->wdr = value;
	prev_wdr = value;
//	dprintk(6, "%s success [WDR:%d]\n",__func__, sensor->wdr);
	return 0;
}


static int s5k4ecgx_set_mode(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	sensor->mode = value;
	dprintk(5, "s5k4ecgx_set_mode is called... mode = %d\n", sensor->mode);
	return 0;
}

static int s5k4ecgx_set_state(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	sensor->state = value;
	dprintk(5, "s5k4ecgx_set_state is called... state = %d\n", sensor->state);
	return 0;
}

static int s5k4ecgx_set_aewb(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	switch (value) {
	case S5K4ECGX_AE_LOCK:
		s5k4ecgx_write_regsets(&CAM_REG32SET_AE_LOCK);
		sensor->ae_lock = value;
		dprintk(5, "set_aewb : ae_lock\n");
		break;
	case S5K4ECGX_AE_UNLOCK:
		s5k4ecgx_write_regsets(&CAM_REG32SET_AE_UNLOCK);
		sensor->ae_lock = value;
		dprintk(5, "set_aewb : ae_unlock\n");
		break;
	case S5K4ECGX_AWB_LOCK:
		s5k4ecgx_write_regsets(&CAM_REG32SET_AWB_LOCK);
		sensor->awb_lock = value;
		dprintk(5, "set_aewb : awb_lock\n");
		break;
	case S5K4ECGX_AWB_UNLOCK:
		s5k4ecgx_write_regsets(&CAM_REG32SET_AWB_UNLOCK);
		sensor->awb_lock = value;
		dprintk(5, "set_aewb : awb_unlock\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_aewb : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	return 0;
}

static int s5k4ecgx_set_fps(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->fps;

//	dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case 0:
		s5k4ecgx_write_regsets(&CAM_REG32SET_AUTO30_FPS);
		dprintk(5, "set_fps : auto\n");
		break;
	case 7:
		s5k4ecgx_write_regsets(&CAM_REG32SET_7_FPS);
		dprintk(5, "set_fps : 7\n");
		break;
	case 15:
		s5k4ecgx_write_regsets(&CAM_REG32SET_15_FPS);
		dprintk(5, "set_fps : 15\n");
		break;
	case 30:
		s5k4ecgx_write_regsets(&CAM_REG32SET_30_FPS);
		dprintk(5, "set_fps : 30\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->fps = value;
//	dprintk(6, "%s success [fps:%d]\n",__func__, sensor->fps);

	return 0;
}

static int s5k4ecgx_set_jpeg_quality(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->jpeg_quality;

//	dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch (value) {
	case S5K4ECGX_JPEG_SUPERFINE:
		s5k4ecgx_write_regsets(&CAM_REG32SET_JPEG_QUALITY_SUPERFINE);
		dprintk(5, "set_jpeg_quality : superfine\n");
		break;
	case S5K4ECGX_JPEG_FINE:
		s5k4ecgx_write_regsets(&CAM_REG32SET_JPEG_QUALITY_FINE);
		dprintk(5, "set_jpeg_quality : fine\n");
		break;
	case S5K4ECGX_JPEG_NORMAL:
		s5k4ecgx_write_regsets(&CAM_REG32SET_JPEG_QUALITY_NORMAL);
		dprintk(5, "set_jpeg_quality : normal\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		goto jpeg_quality_fail;
	}
	sensor->jpeg_quality = value;
//	dprintk(6, "%s success [jpeg_quality:%d]\n",__func__, sensor->jpeg_quality);
	return 0;

jpeg_quality_fail:
	printk(S5K4ECGX_MOD_NAME "s5k4ecgx_set_jpeg_quality is failed!!!\n");
	return -EINVAL;
}



static int s5k4ecgx_get_jpeg_size(struct v4l2_control *vc)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	u8 readdata[8];

	dprintk(CAM_INF, S5K4ECGX_MOD_NAME "s5k4ecgx_get_jpeg_size is called...\n");

	//if (s5k4ecgx_write_read_reg(client, sizeof(MakeImagesInLump_Status), MakeImagesInLump_Status, 8, readdata))
	//goto get_jpeg_size_fail;

	//vc->value = (readdata[3]<<16) + (readdata[2]<<8) + readdata[1];
	//dprintk(CAM_DBG, S5K4ECGX_MOD_NAME "s5k4ecgx_get_jpeg_size::Main JPEG Size reading... 0x%x      Q value...0x%x\n", vc->value, readdata[0]);

	return 0;

get_jpeg_size_fail:
	printk(S5K4ECGX_MOD_NAME "s5k4ecgx_get_jpeg_size is failed!!!\n");
	return -EINVAL;
}

static int s5k4ecgx_get_scene(struct v4l2_control *vc)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	dprintk(5, "s5k4ecgx_get_scene is called...\n");
	vc->value = sensor->scene;
	return 0;
}

static int s5k4ecgx_set_scene(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->scene;
	int scene_num = 0;

	static int prev_scene_num = 0;
	static int iso = 0;
	static int photometry = 0;
	static int ev = 0;
	static int wb = 0;
	static int effect= 0;
	static int sharpness = 0;
	static int saturation = 0;
	static int contrast = 0;
	static int wdr = 0;

	if (value != S5K4ECGX_SCENE_OFF) {
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_OFF);
		if (!prev_scene_num) {
			// save value
			iso = sensor->iso;
			photometry = sensor->photometry;
			ev = sensor->ev;
			wb = sensor->wb;
			effect = sensor->effect;

			wdr = sensor->wdr;
			if (sensor->wdr != S5K4ECGX_WDR_OFF) {
				// restore adjust(contrast, saturation, sharpness)
				s5k4ecgx_set_wdr(S5K4ECGX_WDR_OFF);
			}
			contrast = sensor->contrast;
			saturation = sensor->saturation;
			sharpness = sensor->sharpness;

			dprintk(5, "**save setting values**\n");
			dprintk(5, "[iso:%d][photometry:%d][ev:%d][wb:%d]\n", iso, photometry, ev, wb);
			dprintk(5, "[wdr:%d][cont:%d][satu:%d][sharp:%d]\n", wdr, contrast, saturation, sharpness);
		}
	}

	switch (value) {
	case S5K4ECGX_SCENE_OFF:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_OFF);
		scene_num = 0;
//		if (sensor->effect != S5K4ECGX_EFFECT_OFF) s5k4ecgx_set_effect(sensor->effect);
//		if (sensor->wb != S5K4ECGX_WB_AUTO) s5k4ecgx_set_wb(sensor->wb);
//		if (sensor->iso != S5K4ECGX_ISO_AUTO) s5k4ecgx_set_iso(sensor->iso); 
//		if (sensor->photometry != S5K4ECGX_PHOTOMETRY_CENTER) s5k4ecgx_set_photometry(sensor->photometry);
//		if (sensor->wdr != S5K4ECGX_WDR_OFF) {
//			s5k4ecgx_set_wdr(sensor->wdr);
//		} else {
//			if (sensor->contrast != S5K4ECGX_CONTRAST_DEFAULT) s5k4ecgx_set_contrast(sensor->contrast);
//			if (sensor->saturation != S5K4ECGX_SATURATION_DEFAULT) s5k4ecgx_set_saturation(sensor->saturation);
//			if (sensor->sharpness != S5K4ECGX_SHARPNESS_DEFAULT) s5k4ecgx_set_sharpness(sensor->sharpness);
//		}
//		if (sensor->ev != S5K4ECGX_EV_DEFAULT) s5k4ecgx_set_ev(sensor->ev);
		dprintk(5, "set_scene : None\n");
		break;
	case S5K4ECGX_SCENE_PORTRAIT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_PORTRAIT);
		scene_num = 1;
		dprintk(5, "set_scene : portrait\n");
		break;
	case S5K4ECGX_SCENE_LANDSCAPE:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_LANDSCAPE);
		scene_num = 2;
		dprintk(5, "set_scene : landscape\n");
		break;
	case S5K4ECGX_SCENE_SPORTS:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_SPORTS);
		scene_num = 3;
		dprintk(5, "set_scene : sports\n");
		break;
	case S5K4ECGX_SCENE_PARTY:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_PARTY);
		scene_num = 4;
		dprintk(5, "set_scene : party\n");
		break;
	case S5K4ECGX_SCENE_BEACH:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_BEACH);
		scene_num = 5;
		dprintk(5, "set_scene : beach\n");
		break;
	case S5K4ECGX_SCENE_SUNSET:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_SUNSET);
		scene_num = 6;
		dprintk(5, "set_scene : sunset\n");
		break;
	case S5K4ECGX_SCENE_DAWN:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_DAWN);
		scene_num = 7;
		dprintk(5, "set_scene : dawn\n");
		break;
	case S5K4ECGX_SCENE_FALL:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_FALL);
		scene_num = 8;
		dprintk(5, "set_scene : fall\n");
		break;
	case S5K4ECGX_SCENE_NIGHT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_NIGHT);
		scene_num = 9;
		dprintk(5, "set_scene : night\n");
		break;
	case S5K4ECGX_SCENE_BACKLIGHT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_BACKLIGHT);
		scene_num = 10;
		dprintk(5, "set_scene : backlight\n");
		break;
	case S5K4ECGX_SCENE_FIRE:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_FIRE);
		scene_num = 11;
		dprintk(5, "set_scene : fire\n");
		break;
	case S5K4ECGX_SCENE_TEXT:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_TEXT);
		scene_num = 12;
		dprintk(5, "set_scene : text\n");
		break;
	case S5K4ECGX_SCENE_CANDLE:
		s5k4ecgx_write_regsets(&CAM_REG32SET_SCENE_CANDLE);
		scene_num = 13;
		dprintk(5, "set_scene : candle\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_scene : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
		return -EINVAL;
	}
	sensor->scene = value;

	if (scene_num) {
		struct s5k4ecgx_scene_setting *scene_list;
		scene_list = &(s5k4ecgx_scene_list[scene_num]);

//		if (sensor->iso != scene_list->iso)
//			s5k4ecgx_set_iso(scene_list->iso);
//		if (sensor->photometry != scene_list->photometry)
//			s5k4ecgx_set_photometry(scene_list->photometry);
//		if (sensor->ev != scene_list->ev)
//			s5k4ecgx_set_ev(scene_list->ev + S5K4ECGX_EV_DEFAULT);
//		if (sensor->wb != scene_list->wb)
//			s5k4ecgx_set_wb(scene_list->wb);
//		if (sensor->sharpness != scene_list->sharpness)
//			s5k4ecgx_set_sharpness(scene_list->sharpness + S5K4ECGX_SHARPNESS_DEFAULT);
//		if (sensor->saturation != scene_list->saturation)
//			s5k4ecgx_set_saturation(scene_list->saturation + S5K4ECGX_SATURATION_DEFAULT);

		s5k4ecgx_set_iso(scene_list->iso);
		s5k4ecgx_set_photometry(scene_list->photometry);
		s5k4ecgx_set_ev(scene_list->ev + S5K4ECGX_EV_DEFAULT);
		s5k4ecgx_set_wb(scene_list->wb);
//		s5k4ecgx_set_sharpness(scene_list->sharpness + S5K4ECGX_SHARPNESS_DEFAULT);
//		s5k4ecgx_set_saturation(scene_list->saturation + S5K4ECGX_SATURATION_DEFAULT);

//		s5k4ecgx_set_effect(S5K4ECGX_INVALID_VALUE);
//		s5k4ecgx_set_contrast(S5K4ECGX_INVALID_VALUE);
	} else {
		if (prev_scene_num) {
			// restore
//			if (sensor->iso != iso) s5k4ecgx_set_iso(iso);
//			if (sensor->photometry != photometry) s5k4ecgx_set_photometry(photometry);
//			if (sensor->ev != ev) s5k4ecgx_set_ev(ev);
//			if (sensor->wb != wb) s5k4ecgx_set_wb(wb);
//			if (sensor->effect != effect) s5k4ecgx_set_effect(sensor->effect);
//			if (sensor->contrast != contrast) s5k4ecgx_set_contrast(contrast);
//			if (sensor->saturation != saturation) s5k4ecgx_set_saturation(saturation);
//			if (sensor->sharpness != sharpness) s5k4ecgx_set_sharpness(sharpness);

			s5k4ecgx_set_iso(iso);
			s5k4ecgx_set_photometry(photometry);
			s5k4ecgx_set_ev(ev);
			s5k4ecgx_set_wb(wb);
			s5k4ecgx_set_effect(sensor->effect);
			if (wdr != S5K4ECGX_WDR_OFF) {
				sensor->contrast = contrast;
				sensor->saturation = saturation;
				sensor->sharpness = sharpness;
				s5k4ecgx_set_wdr(S5K4ECGX_WDR_ON);
			} else {
				s5k4ecgx_set_contrast(contrast);
				s5k4ecgx_set_saturation(saturation);
				s5k4ecgx_set_sharpness(sharpness);
			}
			dprintk(5, "**restore setting values**\n");
			dprintk(5, "[iso:%d][photometry:%d][ev:%d][wb:%d]\n", 
					sensor->iso, sensor->photometry, sensor->ev, sensor->wb);
			dprintk(5, "[wdr:%d][contr:%d][satua:%d][sharp:%d]\n",
					sensor->wdr, sensor->contrast, sensor->saturation, sensor->sharpness);
			prev_scene_num = 0;
			iso = 0;
			photometry = 0;
			ev = 0;
			wb = 0;
			effect = 0;
			sharpness = 0;
			saturation = 0;
			contrast = 0;
			wdr = 0;
		}
	}
	prev_scene_num = scene_num;

	return 0;
}

void s5k4ecgx_check_REG_TC_GP_EnablePreviewChanged(void)
{
	int cnt = 0; 
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int REG_TC_GP_EnablePreviewChanged = 0; 
	dprintk(6, "[S5K4ECGX]s5k4ecgx_check_REG_TC_GP_EnablePreviewChanged\n");
	while(cnt < 150) 
	{    

		s5k4ecgx_write_word(client, 0x002C, 0x7000);
		s5k4ecgx_write_word(client, 0x002E, 0x0244);
		s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&REG_TC_GP_EnablePreviewChanged);
		if (!REG_TC_GP_EnablePreviewChanged)break;
		mdelay(10);
		cnt++;
	}            
	if (cnt) dprintk(6, "[S5K4ECGX] wait time for preview frame : %dms\n",cnt*10);
	if (REG_TC_GP_EnablePreviewChanged) printk("[S5K4ECGX] start preview failed.\n");
}

void s5k4ecgx_check_REG_TC_GP_EnableCaptureChanged(void)
{
	int cnt = 0; 
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int REG_TC_GP_EnableCaptureChanged = 0; 
	dprintk(6, "[S5K4ECGX]s5k4ecgx_check_REG_TC_GP_EnableCaptureChanged\n");
	while(cnt < 150) 
	{    

		s5k4ecgx_write_word(client, 0x002C, 0x7000);
		s5k4ecgx_write_word(client, 0x002E, 0x0530);
		s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&REG_TC_GP_EnableCaptureChanged);
		if (REG_TC_GP_EnableCaptureChanged) {
			dprintk(6, "Capture Done\n");
			break;
		}
		mdelay(10);
		cnt++;
	}            
	if (cnt) dprintk(6, "[S5K4ECGX] wait time for capture frame : %dms\n",cnt*10);
	if (!REG_TC_GP_EnableCaptureChanged) printk("[S5K4ECGX] start capture failed.\n");
}

static int s5k4ecgx_start_preview(void)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	dprintk(5, "s5k4ecgx_start_preview() is called...\n");

	//s5k4ecgx_write_regsets(&CAM_REG32SET_INIT);

	/* Preview Start */
	//mdelay(100);

	//s5k4ecgx_write_regsets(&CAM_REG32SET_PREVIEW);

	//INIT_DELAYED_WORK(&camera_esd_check, s5k4ecgx_esd_check);
	//schedule_delayed_work(&camera_esd_check, msecs_to_jiffies(500));

	return 0;
}

static int s5k4ecgx_stop_preview(void)
{
	struct s5k4ecgx_sensor *n_sensor = &next_s5k4ecgx;
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	dprintk(5, "s5k4ecgx_stop_preview is called...\n");

	//cancel_rearming_delayed_work(&camera_esd_check);

	s5k4ecgx_pre_state = s5k4ecgx_curr_state;
	s5k4ecgx_curr_state = S5K4ECGX_STATE_INVALID;

	n_sensor->scene		= 0xFF;
	n_sensor->effect		= 0xFF;
	n_sensor->ev			= 0xFF;
	n_sensor->photometry		= 0xFF;
	n_sensor->iso			= 0xFF;
	n_sensor->contrast		= 0xFF;
	n_sensor->saturation		= 0xFF;
	n_sensor->sharpness		= 0xFF;
	n_sensor->wb			= 0xFF;
	n_sensor->wdr			= 0xFF;
	n_sensor->focus_mode		= 0xFF;
	n_sensor->capture_size	= 0xFF;
	n_sensor->jpeg_quality	= 0xFF;
	n_sensor->preview_size	= 0xFF;
	n_sensor->camcorder_size	= 0xFF;
	return 0;
}


static int s5k4ecgx_set_preview_size(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->preview_size;

//	dprintk(5, "[%s] [old:[%d][new:%d]\n",__func__, old_value, value);
	if (sensor->mode != S5K4ECGX_MODE_CAMCORDER)
	{
		switch (value) {
		case S5K4ECGX_PREVIEW_SIZE_144_176:
			s5k4ecgx_write_regsets(&CAM_REG32SET_PREVIEW_SIZE_144_176);
			dprintk(5, "set_preview_size : 144x176\n");
			break;
		case S5K4ECGX_PREVIEW_SIZE_176_144:
			s5k4ecgx_write_regsets(&CAM_REG32SET_PREVIEW_SIZE_176_144);
			dprintk(5, "set_preview_size : 176x144\n");
			break;
		case S5K4ECGX_PREVIEW_SIZE_320_240:
			s5k4ecgx_write_regsets(&CAM_REG32SET_PREVIEW_SIZE_320_240);
			dprintk(5, "set_preview_size : 320x240\n");
			break;
		case S5K4ECGX_PREVIEW_SIZE_640_480:
			s5k4ecgx_write_regsets(&CAM_REG32SET_PREVIEW_SIZE_640_480);
			dprintk(5, "set_preview_size : 640x480\n");
			break;
		case S5K4ECGX_PREVIEW_SIZE_720_480:
			s5k4ecgx_write_regsets(&CAM_REG32SET_PREVIEW_SIZE_720_480);
			dprintk(5, "set_preview_size : 720x480\n");
			break;
		case S5K4ECGX_PREVIEW_SIZE_800_480:
			s5k4ecgx_write_regsets(&CAM_REG32SET_PREVIEW_SIZE_800_480);
			dprintk(5, "set_preview_size : 800x480\n");
			break;
		default:
			// When running in image capture mode, the call comes here.
			// Set the default video resolution - S5K4ECGX_PREVIEW_VGA
			printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
			return 0;
		}
		sensor->preview_size = value;
		sensor->camcorder_size = S5K4ECGX_INVALID_VALUE;
	} else {
		switch (value) {
		case S5K4ECGX_CAMCORDER_SIZE_176_144:
			s5k4ecgx_write_regsets(&CAM_REG32SET_CAMCORDER_SIZE_176_144);
			dprintk(5, "set_camcorder_size : 176x144\n");
			break;
		case S5K4ECGX_CAMCORDER_SIZE_320_240:
			s5k4ecgx_write_regsets(&CAM_REG32SET_CAMCORDER_SIZE_320_240);
			dprintk(5, "set_camcorder_size : 320x240\n");
			break;
		case S5K4ECGX_CAMCORDER_SIZE_352_288:
			s5k4ecgx_write_regsets(&CAM_REG32SET_CAMCORDER_SIZE_352_288);
			dprintk(5, "set_camcorder_size : 352x288\n");
			break;
		case S5K4ECGX_CAMCORDER_SIZE_640_480:
			s5k4ecgx_write_regsets(&CAM_REG32SET_CAMCORDER_SIZE_640_480);
			dprintk(5, "set_camcorder_size : 640x480\n");
			break;
		case S5K4ECGX_CAMCORDER_SIZE_720_480:
			s5k4ecgx_write_regsets(&CAM_REG32SET_CAMCORDER_SIZE_720_480);
			dprintk(5, "set_camcorder_size : 720x480\n");
			break;
		case S5K4ECGX_CAMCORDER_SIZE_800_480:
			s5k4ecgx_write_regsets(&CAM_REG32SET_CAMCORDER_SIZE_800_480);
			dprintk(5, "set_camcorder_size : 800x480\n");
			break;
		case S5K4ECGX_CAMCORDER_SIZE_1280_720:
			s5k4ecgx_write_regsets(&CAM_REG32SET_CAMCORDER_SIZE_1280_720);
			s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_0);
			dprintk(5, "set_camcorder_size : 1280x720\n");
			break;
		default:
			// When running in image capture mode, the call comes here.
			// Set the default video resolution - S5K4ECGX_PREVIEW_VGA
			printk(S5K4ECGX_MOD_NAME "[%s]Invalid Value(%d)\n", __func__, value);
			return 0;
		}
		sensor->camcorder_size = value;
		sensor->preview_size = S5K4ECGX_INVALID_VALUE;
	}
//	dprintk(6, "%s success [preview_size:%d]\n",__func__, sensor->preview_size);

	return 0;
}

static int s5k4ecgx_start_capture(void)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	struct v4l2_pix_format* pix = &sensor->pix;

	dprintk(5, "s5k4ecgx_start_capture is called...\n");
	if (unlikely(!client)) {
		printk("%s: error client is null\n", __func__);
		return 0;
	}
	if (pix->pixelformat != V4L2_PIX_FMT_JPEG) {
		printk(S5K4ECGX_MOD_NAME "[start capture] pixelformat is not JPEG\n");
		return - EINVAL;
	}

	s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_START);
	dprintk(5, "capture started\n");

	return 0;
}

static int s5k4ecgx_stop_capture(void)
{
	dprintk(5, "s5k4ecgx_stop_capture is called...\n");

	s5k4ecgx_pre_state = s5k4ecgx_curr_state;
	s5k4ecgx_curr_state = S5K4ECGX_STATE_INVALID;

	return 0;
}

static int s5k4ecgx_set_capture_size(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	s32 old_value = (s32)sensor->capture_size;

//	dprintk(5, "%s is called... [old:%d][new:%d]\n",__func__, old_value, value);

	switch(value) {
	case S5K4ECGX_CAPTURE_SIZE_640_480:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_640_480);
		dprintk(5, "set_capture_size : 640x480\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_800_480:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_800_480);
		dprintk(5, "set_capture_size : 800x480\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_1280_960:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_1280_960);
		dprintk(5, "set_capture_size : 1280x960\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_1600_960:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_1600_960);
		dprintk(5, "set_capture_size : 1600x960\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_1600_1200:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_1600_1200);
		dprintk(5, "set_capture_size : 1600x1200\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_2048_1232:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_2048_1232);
		dprintk(5, "set_capture_size : 2048x1232\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_2048_1536:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_2048_1536);
		dprintk(5, "set_capture_size : 2048x1536\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_2560_1536:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_2560_1536);
		s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_0);
		dprintk(5, "set_capture_size : 2560x1536\n");
		break;
	case S5K4ECGX_CAPTURE_SIZE_2560_1920:
		s5k4ecgx_write_regsets(&CAM_REG32SET_CAPTURE_SIZE_2560_1920);
		s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_0);
		dprintk(5, "set_capture_size : 2560x1920\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid value(%d) is ordered!!!\n", __func__, value);
		/* The framesize index was not set properly.
		 * Check s_fmt call - it must be for video mode. */
		return -EINVAL;
	}
	sensor->capture_size = value;
//	dprintk(6, "%s success [capture_size:%d]\n",__func__, sensor->capture_size);
	return 0;
}

static int s5k4ecgx_set_focus_mode(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;
//	dprintk(CAM_INF, S5K4ECGX_MOD_NAME "AF set value = %d\n", value);

	switch(value) 
	{
	case S5K4ECGX_AF_SET_NORMAL :
		s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_1); s5k4ecgx_wait_1_frame();
		s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_2); s5k4ecgx_wait_1_frame();
		if (sensor->scene != S5K4ECGX_SCENE_NIGHT) s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_3); 
		dprintk(5, "set_focus_mode : normal\n");
		break;
	case S5K4ECGX_AF_SET_MACRO :
		s5k4ecgx_write_regsets(&CAM_REG32SET_AF_MACRO_MODE_1); s5k4ecgx_wait_1_frame();
		s5k4ecgx_write_regsets(&CAM_REG32SET_AF_MACRO_MODE_2); s5k4ecgx_wait_1_frame();
		if (sensor->scene != S5K4ECGX_SCENE_NIGHT) s5k4ecgx_write_regsets(&CAM_REG32SET_AF_MACRO_MODE_3); 
		dprintk(5, "set_focus_mode : macro\n");
		break;
	case S5K4ECGX_INVALID_VALUE:
		dprintk(5, "set_focus_mode : invalid\n");
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid value(%d) is ordered!!!\n", __func__, value);
		goto focus_fail;   
	}
	sensor->focus_mode = value;
	return 0;

focus_fail:
	printk(S5K4ECGX_MOD_NAME "s5k4ecgx_set_focus is failed!!!\n");
	return -EINVAL;     
}

static int s5k4ecgx_set_focus(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	int cnt = 0;

	u8 readdata = 0x00;
	u8 status = 0x00;

	dprintk(CAM_INF, S5K4ECGX_MOD_NAME "s5k4ecgx_set_focus_status is called...[%d]\n",value);


	if (s5k4ecgx_curr_state != S5K4ECGX_STATE_PREVIEW) {
		if (value == S5K4ECGX_AF_START || value == S5K4ECGX_AF_STOP) {
			printk(S5K4ECGX_MOD_NAME "[fail] set_focus : sensor is not preview state!!");
			goto focus_status_fail;
		}
	}

	switch(value) 
	{
	case S5K4ECGX_AF_START :
		s5k4ecgx_write_regsets(&CAM_REG32SET_AF_START);
		dprintk(5, "set_focus : af start\n");
		break;
	case S5K4ECGX_AF_STOP :
		s5k4ecgx_set_focus_mode(sensor->focus_mode);
		dprintk(5, "set_focus : af stop\n");
		break;
	case S5K4ECGX_AF_STOP_STEP_1:
		if (sensor->focus_mode == S5K4ECGX_AF_SET_NORMAL) {
			s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_1); 
		} else if (sensor->focus_mode == S5K4ECGX_AF_SET_MACRO) {
			s5k4ecgx_write_regsets(&CAM_REG32SET_AF_MACRO_MODE_1); 
		}
		dprintk(6, "set_focus : stop(1)\n");
		break;
	case S5K4ECGX_AF_STOP_STEP_2:
		if (sensor->focus_mode == S5K4ECGX_AF_SET_NORMAL) {
			s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_2); 
		} else if (sensor->focus_mode == S5K4ECGX_AF_SET_MACRO) {
			s5k4ecgx_write_regsets(&CAM_REG32SET_AF_MACRO_MODE_2); 
		}
		dprintk(6, "set_focus : stop(2)\n");
		break;
	case S5K4ECGX_AF_STOP_STEP_3:
		if (sensor->focus_mode == S5K4ECGX_AF_SET_NORMAL) {
			s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_3); 
		} else if (sensor->focus_mode == S5K4ECGX_AF_SET_MACRO) {
			s5k4ecgx_write_regsets(&CAM_REG32SET_AF_MACRO_MODE_3); 
		}
		dprintk(6, "set_focus : stop(3)\n");
		break;
		/*
		   case S5K4ECGX_AF_POWEROFF :
		   dprintk(6, "AF_POWEROFF \n");
		   s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_1); mdelay(msec);
		   s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_2); mdelay(msec);
		   if (sensor->scene != S5K4ECGX_SCENE_NIGHT) s5k4ecgx_write_regsets(&CAM_REG32SET_AF_NORMAL_MODE_4); 
		   break;
		   */
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid value(%d) is ordered!!!\n", __func__, value);
		goto focus_status_fail;
	}

	return 0;

focus_status_fail:
	printk(S5K4ECGX_MOD_NAME "s5k4ecgx_set_focus_status is failed!!!\n");  
	return -EINVAL;    
}


static int s5k4ecgx_set_zoom(s32 value)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	int capture_width;
	int preview_width;
	int zoom_max = 0;

	if (sensor->mode == S5K4ECGX_MODE_CAMCORDER) {
		preview_width = s5k4ecgx_camcorder_sizes[sensor->camcorder_size].width;
		if (preview_width <= 640) zoom_max = 200;
		else if (preview_width <= 720) zoom_max = 177;
		else if (preview_width <= 800) zoom_max = 160;
		else {
			dprintk(5, "set_zoom : not support zoom for preview_width : %d\n", preview_width);
		}
	} else if (sensor->mode == S5K4ECGX_MODE_VT) {
		preview_width = s5k4ecgx_preview_sizes[sensor->preview_size].width;
		if (preview_width <= 640) zoom_max = 200;
		else if (preview_width <= 720) zoom_max = 177;
		else if (preview_width <= 800) zoom_max = 160;
		else {
			dprintk(5, "set_zoom : not support zoom for preview_width : %d\n", preview_width);
		}
	} else {
		capture_width = s5k4ecgx_image_sizes[sensor->capture_size].width;
		if (capture_width < 800) zoom_max = 200;
		else if (capture_width <= 1600) zoom_max = 160; 
		else if (capture_width <= 2048) zoom_max = 125;
		else {
			dprintk(5, "set_zoom : not support zoom for capture_width : %d\n", capture_width);
		}
	}

	switch (value)
	{
	case S5K4ECGX_ZOOM_STEP_0:
		s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_0);
		break;
	case S5K4ECGX_ZOOM_STEP_1:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_1);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_1);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_1);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_1);
		break;
	case S5K4ECGX_ZOOM_STEP_2:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_2);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_2);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_2);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_2);
		break;
	case S5K4ECGX_ZOOM_STEP_3:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_3);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_3);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_3);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_3);
		break;
	case S5K4ECGX_ZOOM_STEP_4:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_4);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_4);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_4);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_4);
		break;
	case S5K4ECGX_ZOOM_STEP_5:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_5);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_5);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_5);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_5);
		break;
	case S5K4ECGX_ZOOM_STEP_6:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_6);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_6);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_6);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_6);
		break;
	case S5K4ECGX_ZOOM_STEP_7:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_7);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_7);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_7);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_7);
		break;
	case S5K4ECGX_ZOOM_STEP_8:
		if (zoom_max == 125) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_25_ZOOM_8);
		else if (zoom_max == 160) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_6_ZOOM_8);
		else if (zoom_max == 177) s5k4ecgx_write_regsets(&CAM_REG32SET_X1_77_ZOOM_8);
		else if (zoom_max == 200) s5k4ecgx_write_regsets(&CAM_REG32SET_X2_ZOOM_8);
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid value(%d) is ordered!!!\n", __func__, value);
		goto zoom_failed;
	}
	dprintk(5, "set_zoom : step %d(max:%d)\n", value, zoom_max);
	sensor->zoom = value;

	return 0;	

zoom_failed:
	return 0;
}

#define INNER_WINDOW_WIDTH		143
#define INNER_WINDOW_HEIGHT		143
#define OUTER_WINDOW_WIDTH		320
#define OUTER_WINDOW_HEIGHT		266

static int s5k4ecgx_set_focus_touch_position(s32 value)
{
	int err, i=0;
	int ret;
	u16 read_value = 0;
	u16 touch_x, touch_y;
	u16 outter_x, outter_y;
	u16 inner_x, inner_y;
	u32 width, height;
	u16 outter_window_width, outter_window_height;
	u16 inner_window_width, inner_window_height;

	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	dprintk(5, "value : %d\n", value);

	/* get x,y touch position */
	touch_x = (u16)sensor->position.x;
	touch_y = (u16)sensor->position.y;

	/* get preview width, height */
	width = s5k4ecgx_preview_sizes[sensor->preview_size].width;
	height = s5k4ecgx_preview_sizes[sensor->preview_size].height;

	touch_x = width - touch_x;
	touch_y = height - touch_y;

	CAM_REG32_T S5K4ECGX_TOUCH_AF[] =
	{
		{0xFCFC, 0xD000},
		{0x0028, 0x7000},
		{0x002A, 0x0294},	//AF window setting
		{0x0F12, 0x0100},	//REG_TC_AF_FstWinStartX 
		{0x0F12, 0x00E3},	//REG_TC_AF_FstWinStartY
		{0x002A, 0x029C},	//AF window setting
		{0x0F12, 0x01C6},	//REG_TC_AF_ScndWinStartX
		{0x0F12, 0x0166},	//REG_TC_AF_ScndWinStartY
		{0x002A, 0x02A4},	//AF window setting
		{0x0F12, 0x0001},	//REG_TC_AF_WinSizesUpdated
	};

	err = s5k4ecgx_write_word(client, 0xFCFC, 0xD000);
	err = s5k4ecgx_write_word(client, 0x002C, 0x7000);
	err = s5k4ecgx_write_word(client, 0x002E, 0x0298);
	err = s5k4ecgx_read_word(client, 0x0F12, (u16 *)&read_value);
	dprintk(5, "outter_width : %x(%d)\n", read_value, read_value);
	outter_window_width = (u32)(read_value * width / 1024);
	read_value = 0;

	err = s5k4ecgx_write_word(client, 0x002E, 0x029A);
	err = s5k4ecgx_read_word(client, 0x0F12, &read_value);
	dprintk(5, "outter_height : %x(%d)\n", read_value, read_value);
	outter_window_height = (u32)(read_value * height / 1024);
	read_value = 0;

	err = s5k4ecgx_write_word(client, 0x002E, 0x02A0);
	err = s5k4ecgx_read_word(client, 0x0F12, &read_value);
	dprintk(5, "inner_width : %x(%d)\n", read_value, read_value);
	inner_window_width = (u32)(read_value * width / 1024);
	read_value = 0;

	err = s5k4ecgx_write_word(client, 0x002E, 0x02A2);
	err = s5k4ecgx_read_word(client, 0x0F12, &read_value);
	dprintk(5, "inner_height : %x(%d)\n", read_value, read_value);
	inner_window_height = (u32)(read_value * height / 1024);
	read_value = 0;

	if (touch_x <= inner_window_width/2) {	
		// inner window, outter window should be positive.
		outter_x = 0;
		inner_x = 0;
	} else if (touch_x <= outter_window_width/2) {
		// outter window should be positive.
		inner_x = touch_x - inner_window_width/2;
		outter_x = 0;
	} else if (touch_x >= ((width - 1) - inner_window_width/2)) {
		// inner window, outter window should be less than LCD Display Size
		inner_x = (width - 1) - inner_window_width;
		outter_x = (width - 1) - outter_window_width;
	} else if (touch_x >= ((width -1) - outter_window_width/2)) {
		// outter window should be less than LCD Display Size
		inner_x = touch_x - inner_window_width/2;
		outter_x = (width -1) - outter_window_width;
	} else {
		// touch_x is not corner, so set using touch point.
		inner_x = touch_x - inner_window_width/2;
		outter_x = touch_x - outter_window_width/2;
	}

	if (touch_y <= inner_window_height/2) {	
		// inner window, outter window should be positive.
		outter_y = 0;
		inner_y = 0;
	} else if (touch_y <= outter_window_height/2) {
		// outter window should be positive.
		inner_y = touch_y - inner_window_height/2;
		outter_y = 0;
	} else if (touch_y >= ((height - 1) - inner_window_height/2)) {
		// inner window, outter window should be less than LCD Display Size
		inner_y = (height - 1) - inner_window_height;
		outter_y = (height - 1) - outter_window_height;
	} else if (touch_y >= ((height - 1) - outter_window_height/2)) {
		// outter window should be less than LCD Display Size
		inner_y = touch_y - inner_window_height/2;
		outter_y = (height - 1) - outter_window_height;
	} else {
		// touch_x is not corner, so set using touch point.
		inner_y = touch_y - inner_window_height/2;
		outter_y = touch_y - outter_window_height/2;
	}

	if (!outter_x) outter_x = 1;
	if (!outter_y) outter_y = 1;
	if (!inner_x) inner_x= 1;
	if (!inner_y) inner_y= 1;

//		outter_x = 0;
//		if (touch_x <= inner_window_width/2) inner_x = 0;
//		else inner_x = touch_x - inner_window_width/2;
//	} else if (width - outter_window_width <= touch_x) {
//		outter_x = width - outter_window_width;
//		if (touch_x <= width - outter_window_width + inner_window_width/2) inner_x = width - outter_window_width;
//		else if (width - inner_window_width <= touch_x) inner_x = width - inner_window_width ;
//		else inner_x = touch_x - inner_window_width/2;
//	} else {
//		outter_x = touch_x - outter_window_width/2 ;
//		inner_x = touch_x - inner_window_width/2 ;
//	}
//	// get start y position 
//	if (touch_y <= outter_window_height/2) {
//		outter_y = 0;
//		if (touch_y <= inner_window_height/2) inner_y = 0;
//		else inner_y = touch_y - inner_window_height/2;
//	} else if (height - outter_window_height <= touch_y) {
//		outter_y = height - outter_window_height;
//		if (touch_y <= height - outter_window_height + inner_window_height/2) inner_y = height - outter_window_height;
//		else if (height - inner_window_height <= touch_y) inner_y = height - inner_window_height ;
//		else inner_y = touch_y - inner_window_height/2;
//	} else {
//		outter_y = touch_y - outter_window_height/2 ;
//		inner_y = touch_y - inner_window_height/2 ;
//	}

	
	dprintk(5, "touch position(%d, %d), preview size(%d, %d)\n",
			touch_x, touch_y, width, height);
	dprintk(5, "point first(%d, %d), second(%d, %d)\n",
			outter_x, outter_y, inner_x, inner_y);
	
	//if(value == TOUCH_AF_START)
	{
		S5K4ECGX_TOUCH_AF[3].value = outter_x * 1024 / width;
		S5K4ECGX_TOUCH_AF[4].value = outter_y * 1024 / height;
		//S5K4ECGX_TOUCH_AF[5].value = outter_window_width * 1024 / width;
		//S5K4ECGX_TOUCH_AF[6].value = outter_window_height * 1024 / height;

		S5K4ECGX_TOUCH_AF[6].value = inner_x * 1024 / width;
		S5K4ECGX_TOUCH_AF[7].value = inner_y * 1024 / height;
		//S5K4ECGX_TOUCH_AF[9].value = inner_window_width * 1024 / width;
		//S5K4ECGX_TOUCH_AF[10].value = inner_window_height* 1024 / height;
	}

	dprintk(5, "fisrt reg(0x%x(%d), 0x%x(%d)) second reg(0x%x(%d), 0x%x(%d)\n",
			S5K4ECGX_TOUCH_AF[3].value, S5K4ECGX_TOUCH_AF[3].value,
			S5K4ECGX_TOUCH_AF[4].value, S5K4ECGX_TOUCH_AF[4].value,
			S5K4ECGX_TOUCH_AF[6].value, S5K4ECGX_TOUCH_AF[6].value,
			S5K4ECGX_TOUCH_AF[7].value, S5K4ECGX_TOUCH_AF[7].value);
	//dprintk(5, "second reg(0x%x, 0x%x, 0x%x, 0x%x)\n",
			//S5K4ECGX_TOUCH_AF[7].value,
			//S5K4ECGX_TOUCH_AF[8].value,
			//S5K4ECGX_TOUCH_AF[9].value,
			//S5K4ECGX_TOUCH_AF[10].value);

	for (i=0 ; i <ARRAY_SIZE(S5K4ECGX_TOUCH_AF); i++) {
		err = s5k4ecgx_write_word(client, S5K4ECGX_TOUCH_AF[i].addr, S5K4ECGX_TOUCH_AF[i].value);
	}
	s5k4ecgx_wait_1_frame();
	
	if(err < 0){
		dev_err(&client->dev, "%s: failed: i2c_write for touch_auto_focus\n", __func__);
		return -EIO;
	}

	s5k4ecgx_touch_state = true;
	return 0;
}


/*
static int s5k4ecgx_set_focus_touch(s32 value)
{
  struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
  struct i2c_client *client = sensor->i2c_client;

  u16 touch_x = (u16)sensor->position.x;
  u16 touch_y = (u16)sensor->position.y;

  dprintk(CAM_INF, S5K4ECGX_MOD_NAME "s5k4ecgx_set_focus_touch is called... x : %d, y : %d\n", touch_x, touch_y); 

  u8 x_offset = 0x34;
  u8 y_offset = 0x24;  

  u8 Touch_AF_list[12] = {0x4D, 0x01, 0x03, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00};

  Touch_AF_list[4]  = ((touch_x - x_offset) & 0x00FF);
  Touch_AF_list[5]  = (((touch_x - x_offset) & 0xFF00) >> 8);
  Touch_AF_list[6]  = ((touch_y - y_offset) & 0x00FF);
  Touch_AF_list[7]  = (((touch_y - y_offset) & 0xFF00) >> 8);
  Touch_AF_list[8]  = ((touch_x + x_offset) & 0x00FF);
  Touch_AF_list[9]  = (((touch_x + x_offset) & 0xFF00) >> 8);
  Touch_AF_list[10] = ((touch_y + y_offset) & 0x00FF);
  Touch_AF_list[11] = (((touch_y + y_offset) & 0xFF00) >> 8);  
  
  if(s5k4ecgx_write_reg(client, sizeof(Touch_AF_list), Touch_AF_list))
    goto focus_touch_fail;
  
  s5k4ecgx_touch_state = true;

  return 0;

focus_touch_fail:
  printk(S5K4ECGX_MOD_NAME "s5k4ecgx_set_focus_touch is failed!!!\n");
  return -EINVAL;     
}
*/

static void s5k4ecgx_set_skip(void)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	int skip_frame = 0;//snapshot

//	dprintk(5, "s5k4ecgx_set_skip is called...\n");
	if (S5K4ECGX_STATE_CAPTURE == s5k4ecgx_pre_state) {
		skip_frame = capture_skip_frame;
		if (sensor->scene == S5K4ECGX_SCENE_NIGHT || 
				sensor->scene == S5K4ECGX_SCENE_FIRE) {
			skip_frame = capture_skip_frame;
		}
		dprintk(5, "set_skip : %d frame\n", skip_frame);
		isp_set_hs_vs(0, skip_frame);
		return;
	} 

	if (sensor->state == S5K4ECGX_STATE_PREVIEW) {
		if (sensor->fps > 30) {
			skip_frame = sensor->fps / 5;
		} else if (sensor->fps > 15) {
			skip_frame = sensor->fps / 4;
		} else if (sensor->fps > 7) {
			skip_frame = sensor->fps / 3;
		} else if (sensor->fps > 0) {
			skip_frame = sensor->fps / 2;
		} else {
			skip_frame = 5;
		}
		dprintk(5, "set_skip : %d frame\n", skip_frame);
		isp_set_hs_vs(0, skip_frame);
		return;
	}
}

static int ioctl_streamoff(struct v4l2_int_device *s)
{
	struct s5k4ecgx_sensor *sensor = s->priv;
	int err = 0;

	dprintk(5, "ioctl_streamoff is called...\n");
	if (sensor->state != S5K4ECGX_STATE_CAPTURE) {
		err = s5k4ecgx_stop_preview();
	} else {
		err = s5k4ecgx_stop_capture();
	}

	return err;
}

static int ioctl_streamon(struct v4l2_int_device *s)
{
	struct s5k4ecgx_sensor *sensor = s->priv;
	int err = 0;

	use_i2c_bus_unlock_timer = 1;
	dprintk(5, "ioctl_streamon is called...(%x)\n", sensor->state);

	if (sensor->state != S5K4ECGX_STATE_CAPTURE) {
		if (sensor->check_dataline || dtp_on) {
			dprintk(6, "check dataline....................\n");
			if (s5k4ecgx_check_dataline())
				goto streamon_fail;
		} else {
			dprintk(6, "start preview....................\n");
			if (s5k4ecgx_start_preview())
				goto streamon_fail;
		}
	} else {
		dprintk(6, "start capture....................\n");
		if (s5k4ecgx_start_capture())
			goto streamon_fail;
	}
	return 0;

streamon_fail:
	dprintk(1, "ioctl_streamon is failed\n");
	return -EINVAL;
}

/**
 * ioctl_queryctrl - V4L2 sensor interface handler for VIDIOC_QUERYCTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @qc: standard V4L2 VIDIOC_QUERYCTRL ioctl structure
 *
 * If the requested control is supported, returns the control information
 * from the s5k4ecgx_ctrl_list[] array.
 * Otherwise, returns -EINVAL if the control is not supported.
 */
static int ioctl_queryctrl(struct v4l2_int_device *s,
		struct v4l2_queryctrl *qc)
{
	int i;

	dprintk(5, "ioctl_queryctrl is called...\n");
	for (i = 0; i < NUM_S5K4ECGX_CONTROL; i++) {
		if (qc->id == s5k4ecgx_ctrl_list[i].id)
			break;
	}
	if (i == NUM_S5K4ECGX_CONTROL) {
		printk(S5K4ECGX_MOD_NAME "Control ID is not supported!!\n");
		qc->flags = V4L2_CTRL_FLAG_DISABLED;

		return -EINVAL;
	}
	*qc = s5k4ecgx_ctrl_list[i];
	return 0;
}

static int s5k4ecgx_get_focus(struct v4l2_control *vc, s32 cmd)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;
	CAM_I2C_CLIENT *client = cam_i2c_client;

	u16 status= 0x00;

	switch(cmd) {
	case S5K4ECGX_AF_CHECK_STATUS :
		dprintk(7, "s5k4ecgx_get_auto_focus is called...\n"); 
		s5k4ecgx_write_word(client, 0x002C, 0x7000);
		s5k4ecgx_write_word(client, 0x002E, 0x2EEE);
		s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&status);
		dprintk(7, "AutoFocus_STATUS : 0x%04X\n", status);
		switch(status & 0xFFFF) {    
		case 1:
			dprintk(7, "[1st]AF - PROGRESS \n");
			vc->value = S5K4ECGX_AF_STATUS_PROGRESS;
			break;
		case 2:
			dprintk(6, "[1st]AF - SUCCESS \n");
			vc->value = S5K4ECGX_AF_STATUS_SUCCESS;
			break;
		default:
			dprintk(7, "[1st]AF - FAIL\n");
			vc->value = S5K4ECGX_AF_STATUS_FAIL;
			break;
		}
		break;
	case S5K4ECGX_AF_CHECK_2nd_STATUS :
		s5k4ecgx_write_word(client, 0x002C, 0x7000);
		s5k4ecgx_write_word(client, 0x002E, 0x2207);
		s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&status);
		dprintk(7, "AutoFocus_2nd_STATUS : 0x%04X\n", status);
		switch((status & 0xFF00) > 8) {
		case 1:
			dprintk(7, "[2nd]AF - PROGRESS \n");
			vc->value = S5K4ECGX_AF_STATUS_PROGRESS;
			break;
		case 0:
			dprintk(6, "[2nd]AF - SUCCESS \n");
			vc->value = S5K4ECGX_AF_STATUS_SUCCESS;
			break;
		default:
			dprintk(7, "[2nd]AF - PROGRESS \n");
			vc->value = S5K4ECGX_AF_STATUS_PROGRESS;
			break;
		}
		break;
	case S5K4ECGX_AF_CHECK_AE_STATUS :
		s5k4ecgx_write_word(client, 0x002C, 0x7000);
		s5k4ecgx_write_word(client, 0x002E, 0x2C74);
		s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&status);
		dprintk(7, "AE_STATUS : 0x%04X\n", status);
		switch(status & 0xFF) {
		case 1:
			dprintk(6, "AE - STABLE \n");
			vc->value = S5K4ECGX_AE_STATUS_STABLE;
			break;
		default:
			dprintk(6, "AE - UNSTABLE \n");
			vc->value = S5K4ECGX_AE_STATUS_UNSTABLE;
			break;
		}
		break;

	default:
		printk(S5K4ECGX_MOD_NAME"AF CHECK CONTROL NOT MATCHED\n");
		break;
	}
//	dprintk(7, "result = %d (1.progress, 2.success, 3.fail)\n", vc->value);
	return 0;

get_focus_fail:
	printk(S5K4ECGX_MOD_NAME "s5k4ecgx_get_focus is failed!!!\n");
	return -EINVAL;      
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
	struct s5k4ecgx_sensor *sensor = s->priv;

	int retval = 0;

	dprintk(7, "ioctl_g_ctrl is called...(%d)\n", vc->id);

	switch (vc->id) {
	case V4L2_CID_SELECT_MODE:
		vc->value = sensor->mode;
		break;
	case V4L2_CID_SELECT_STATE:
		vc->value = sensor->state;
		break;
	case V4L2_CID_FOCUS_MODE:
		vc->value = sensor->focus_mode;
		break;
	case V4L2_CID_AF:
		retval = s5k4ecgx_get_focus(vc,0);
		break;
	case V4L2_CID_AF_2ND:
		retval = s5k4ecgx_get_focus(vc,6);
		break;
	case V4L2_CID_AE_STATUS:
		retval = s5k4ecgx_get_focus(vc,9);
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
	case V4L2_CID_WDR:
		vc->value = sensor->wdr;
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
	case V4L2_CID_CAMERA_AE_AWB_LOCKUNLOCK:
		vc->value = sensor->aewb;
		break;
	case V4L2_CID_SCENE:
		retval = s5k4ecgx_get_scene(vc);
		break;
	case V4L2_CID_JPEG_SIZE:
		retval = s5k4ecgx_get_jpeg_size(vc);
		break;
	case V4L2_CID_FW_YUV_OFFSET:
		vc->value = sensor->yuv_offset;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
		vc->value = sensor->jpeg_main_size;
		dprintk(6, "V4L2_CID_CAM_JPEG_MAIN_SIZE : %d\n", vc->value);
		break;
	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
		vc->value = sensor->jpeg_main_offset;
		dprintk(6, "V4L2_CID_CAM_JPEG_MAIN_OFFSET: %d\n", vc->value);
		break;
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
		vc->value = sensor->jpeg_thumb_size;
		dprintk(6, "V4L2_CID_CAM_JPEG_THUMB_SIZE: %d\n", vc->value);
		break;
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		vc->value = sensor->jpeg_thumb_offset;
		dprintk(6, "V4L2_CID_CAM_JPEG_THUMB_OFFSET: %d\n", vc->value);
		break;
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		vc->value = sensor->jpeg_postview_offset;
		dprintk(6, "V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET: %d\n", vc->value);
		break;
	case V4L2_CID_JPEG_QUALITY:
		vc->value = sensor->jpeg_quality;
		dprintk(6, "V4L2_CID_CAM_JPEG_QUALITY: %d\n", vc->value);
		break;
	case V4L2_CID_JPEG_CAPTURE_WIDTH:
		vc->value = sensor->jpeg_capture_w;
		break;
	case V4L2_CID_JPEG_CAPTURE_HEIGHT:
		vc->value = sensor->jpeg_capture_h;
		break;
	case V4L2_CID_FW_VERSION:
		vc->value = sensor->fw_version;
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid value(0x%08X) is ordered!!!\n", __func__, vc->id);
		break;
	}

	return retval;
}

static int s5k4ecgx_get_frame_index(const struct s5k4ecgx_frame_size *pix_sizes, int nr_sizes, int width, int height) 
{
	int index = 0;

	for(index = 0; index < nr_sizes; index++) {
		if (pix_sizes[index].width == width && pix_sizes[index].height == height) {
			dprintk(7, "success to find frame size[%dx%d][index:%d]\n", width, height, index);
			break;
		}
	}
	if (index == nr_sizes) {
		printk(S5K4ECGX_MOD_NAME "[%dx%d] frame size is not supported!\n", width, height);
		return -EINVAL;
	}
	return index;
}



/**
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the S5K4ECGX sensor struct).
 * Otherwise, * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	struct s5k4ecgx_sensor *n_sensor = &next_s5k4ecgx;
	struct s5k4ecgx_sensor *sensor = s->priv;
	int retval = 0;
	int index = 0;
	int width = 0;
	int height = 0;
	const struct s5k4ecgx_frame_size *frame_sizes;
	int nr_sizes;
	int cur_lux = 0;

	dprintk(7, "ioctl_s_ctrl is called...(%d)\n", vc->id);

	switch (vc->id) {
	case V4L2_CID_SELECT_MODE:
		retval = s5k4ecgx_set_mode(vc->value);
		break;
	case V4L2_CID_SELECT_STATE:
		retval = s5k4ecgx_set_state(vc->value);
		break;
	case V4L2_CID_FOCUS_MODE:
		if (sensor->focus_mode == vc->value) {
			dprintk(6, "same as before : focus_mode(%d)\n", vc->value);
			break;
		}
		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
			dprintk(6, "post apply focus_mode\n");
			n_sensor->focus_mode = vc->value;
			break;
		}
		retval = s5k4ecgx_set_focus_mode(vc->value);
		break;
	case V4L2_CID_AF:
		retval = s5k4ecgx_set_focus(vc->value);
		break;
	case V4L2_CID_ANTISHAKE:
		dprintk(5, "s5k4ecgx is not support Anti-Shake\n");
		break;
	case V4L2_CID_ZOOM:
		if (sensor->zoom == vc->value) {
			dprintk(6, "same as before : zoom(%d)\n", vc->value);
			break;
		}
		retval = s5k4ecgx_set_zoom(vc->value);
		break;
	case V4L2_CID_ISO:
		if (sensor->iso == vc->value) {
			dprintk(6, "same as before : iso(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->iso = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_iso(vc->value);
		break;
	case V4L2_CID_BRIGHTNESS:
		if (sensor->ev == vc->value) {
			dprintk(6, "same as before : ev(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->ev = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_ev(vc->value);
		break;
	case V4L2_CID_WB:
		if (sensor->wb == vc->value) {
			dprintk(6, "same as before : wb(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->wb = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_wb(vc->value);
		break;
	case V4L2_CID_WDR:
		if (sensor->wdr == vc->value) {
			dprintk(6, "same as before : wdr(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->wdr = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_wdr(vc->value);
		break;
	case V4L2_CID_EFFECT:
		if (sensor->effect == vc->value) {
			dprintk(6, "same as before : effect(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->effect= vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_effect(vc->value);
		break;
	case V4L2_CID_CONTRAST:
		if (sensor->contrast == vc->value) {
			dprintk(6, "same as before : contrast(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->contrast = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_contrast(vc->value);
		break;
	case V4L2_CID_SATURATION:
		if (sensor->saturation == vc->value) {
			dprintk(6, "same as before : saturation(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->saturation = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_saturation(vc->value);
		break;
	case V4L2_CID_SHARPNESS:
		if (sensor->sharpness == vc->value) {
			dprintk(6, "same as before : sharpness(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->sharpness = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_sharpness(vc->value);
		break;
	case V4L2_CID_SCENE:
		if (sensor->scene == vc->value) {
			dprintk(6, "same as before : scene(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->scene = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_scene(vc->value);
		break;
	case V4L2_CID_PHOTOMETRY:
		if (sensor->photometry == vc->value) {
			dprintk(6, "same as before : photometry(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->photometry = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_photometry(vc->value);
		break;
	case V4L2_CID_AEWB:
	case V4L2_CID_CAMERA_AE_AWB_LOCKUNLOCK:
		if (sensor->ae_lock == vc->value) {
			dprintk(5, "same as before : ae_lock(%d)\n", vc->value);
			break;
		} else if (sensor->awb_lock == vc->value) {
			dprintk(5, "same as before : awb_lock(%d)\n", vc->value);
			break;
		}

		retval = s5k4ecgx_set_aewb(vc->value);
		break;
	case V4L2_CID_JPEG_QUALITY:
		if (sensor->jpeg_quality == vc->value) {
			dprintk(6, "same as before : jpeg_quality(%d)\n", vc->value);
			break;
		}
//		if (s5k4ecgx_curr_state == S5K4ECGX_STATE_INVALID) {
//			n_sensor->jpeg_quality = vc->value;
//			break;
//		}
		retval = s5k4ecgx_set_jpeg_quality(vc->value);
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
		sensor->check_dataline = vc->value;
		retval = 0;
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		retval = s5k4ecgx_check_dataline_stop();
		break;
	case V4L2_CID_CAMERA_PREVIEW_SIZE:
		if (sensor->mode == S5K4ECGX_MODE_CAMCORDER) {
			frame_sizes = s5k4ecgx_camcorder_sizes;
			nr_sizes = ARRAY_SIZE(s5k4ecgx_camcorder_sizes);
		} else {
			frame_sizes = s5k4ecgx_preview_sizes;
			nr_sizes = ARRAY_SIZE(s5k4ecgx_preview_sizes);
		}
		width = vc->value >> 16;
		height = vc->value & 0xFFFF;
		printk("s_ctrl : try to set preview [%dx%d]\n", width, height);

		index = s5k4ecgx_get_frame_index(frame_sizes, nr_sizes, width, height);
		if (index < 0) {
			printk(S5K4ECGX_MOD_NAME "Preview Size[%dx%d] is not supported!\n", width, height);
			return -EINVAL;
		}
		if (sensor->mode == S5K4ECGX_MODE_CAMCORDER) {
			if (sensor->camcorder_size == index) {
				dprintk(6, "same as before : camcorder_size[%dx%d]\n", width, height);
				break;
			}
			if (s5k4ecgx_curr_state != S5K4ECGX_STATE_PREVIEW) {
				// not preview status
				dprintk(6, "post apply camcorder_size\n");
				n_sensor->camcorder_size = index;
				break;
			}
		} else {
			if (sensor->preview_size == index) {
				dprintk(6, "same as before : preview_size[%dx%d]\n", width, height);
				break;
			}
		if (s5k4ecgx_curr_state != S5K4ECGX_STATE_PREVIEW) {
			// not preview status
			dprintk(6, "post apply preview_size\n");
			n_sensor->preview_size = index;
			break;
		}
		}
		s5k4ecgx_wait_1_frame();
		s5k4ecgx_set_preview_size(index);
		break;
	case V4L2_CID_CAMERA_CAPTURE_SIZE:
		width = vc->value >> 16;
		height = vc->value & 0xFFFF;
		printk("s_ctrl : try to set capture [%dx%d]\n", width, height);
		index = s5k4ecgx_get_frame_index(s5k4ecgx_image_sizes, 
				ARRAY_SIZE(s5k4ecgx_image_sizes), 
				width, 
				height);
		if (index < 0) {
			printk(S5K4ECGX_MOD_NAME "Capture Size[%dx%d] is not supported!\n", width, height);
			break;
		}
		if (sensor->capture_size == index) {
			dprintk(6, "[%s]ignore same value[capture_size:%d]\n", __func__, vc->value);
			break;
		}
		if (s5k4ecgx_curr_state != S5K4ECGX_STATE_PREVIEW) {
			// not preview status
			dprintk(6, "post apply capture_size\n");
			n_sensor->capture_size = index;
			break;
		}
		s5k4ecgx_wait_1_frame();
		s5k4ecgx_set_capture_size(index);
		if (sensor->zoom != S5K4ECGX_ZOOM_STEP_0) s5k4ecgx_set_zoom(sensor->zoom);
		break;
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
		printk("s_ctrl : position[x:%d]\n", vc->value);
		sensor->position.x = vc->value;
		retval = 0;
		break;
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		printk("s_ctrl : position[y:%d]\n", vc->value);
		sensor->position.y = vc->value;
		retval = 0;
		break;
	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		s5k4ecgx_set_focus_touch_position(vc->value);
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Invalid value(0x%08X) is ordered!!!\n", __func__,  vc->id);
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

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		switch (fmt->pixelformat) {
		case V4L2_PIX_FMT_JPEG:
			index = 0;
			break;
		default:
			printk(S5K4ECGX_MOD_NAME "[%s]Capture Format Should be JPEG\n", __func__);
			return -EINVAL;
		}
		break;
	default:
		printk(S5K4ECGX_MOD_NAME "[%s]Format should be CAPTURE\n", __func__);
		return -EINVAL;
	}
	fmt->flags = s5k4ecgx_formats[index].flags;
	fmt->pixelformat = s5k4ecgx_formats[index].pixelformat;
	strlcpy(fmt->description, s5k4ecgx_formats[index].description, sizeof(fmt->description));
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
	struct s5k4ecgx_sensor *n_sensor = &next_s5k4ecgx;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct s5k4ecgx_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix2 = &sensor->pix;
	int err = 0;
	int index = 0;
	int current_lux = 0;
	int fps = 0;
	int stable_msec = 0;
	int elapsed_msec = 0;

	dprintk(5, "ioctl_try_fmt_cap [mode:%d][state:%d][%dx%d]\n", 
			sensor->mode,
			sensor->state,
			pix->width,
			pix->height);

	if (sensor->state == S5K4ECGX_STATE_CAPTURE) {
		index = s5k4ecgx_get_frame_index(s5k4ecgx_image_sizes,
				ARRAY_SIZE(s5k4ecgx_image_sizes),
				pix->width, 
				pix->height);

		if (index < 0) {
			printk(S5K4ECGX_MOD_NAME "Capture Size[%dx%d] is not supported!\n", pix->width, pix->height);
			return -EINVAL;
		}

		dprintk(6, "capture size[%d][%dx%d]\n", index, 
				s5k4ecgx_image_sizes[index].width, 
				s5k4ecgx_image_sizes[index].height);

		if (pix->pixelformat == V4L2_PIX_FMT_UYVY ||
				pix->pixelformat == V4L2_PIX_FMT_YUYV) {
			pix->field = V4L2_FIELD_NONE;
			pix->bytesperline = pix->width * 2;
			pix->sizeimage = pix->bytesperline * pix->height;
			dprintk(7, "V4L2_PIX_FMT_UYVY\n");
		} else {
			pix->field = V4L2_FIELD_NONE;
			pix->bytesperline = JPEG_CAPTURE_WIDTH * 2;
			pix->sizeimage = pix->bytesperline * JPEG_CAPTURE_HEIGHT;
			dprintk(7, "V4L2_PIX_FMT_JPEG\n");
		}

		s5k4ecgx_get_lux(&current_lux);
		if (current_lux <= 0x0032) {
			if (sensor->scene == S5K4ECGX_SCENE_NIGHT || 
					sensor->scene == S5K4ECGX_SCENE_FIRE) {
				//night_cap_on = 1;
				//s5k4ecgx_write_regsets(&CAM_REG32SET_NIGHT_CAP_ON);
				//mdelay(250);
			} else {
				low_cap_on = 1;
				s5k4ecgx_write_regsets(&CAM_REG32SET_LOW_CAP_ON);
				dprintk(5, "low light capture on\n");
				mdelay(120);
			}
		}
		s5k4ecgx_pre_state = s5k4ecgx_curr_state;
		s5k4ecgx_curr_state = S5K4ECGX_STATE_CAPTURE;
	}  else {
		const struct s5k4ecgx_frame_size *frame_sizes;
		int nr_sizes;

		if (sensor->mode == S5K4ECGX_MODE_CAMCORDER) {
			frame_sizes = s5k4ecgx_camcorder_sizes;
			nr_sizes = ARRAY_SIZE(s5k4ecgx_camcorder_sizes);
		} else {
			frame_sizes = s5k4ecgx_preview_sizes;
			nr_sizes = ARRAY_SIZE(s5k4ecgx_preview_sizes);
		}

		index = s5k4ecgx_get_frame_index(frame_sizes, nr_sizes, pix->width, pix->height);
		if (index < 0) {
			printk(S5K4ECGX_MOD_NAME "Preview Size[%dx%d] is not supported!\n", pix->width, pix->height);
			return -EINVAL;
		}

		// Success to find proper preview
		pix->field = V4L2_FIELD_NONE;
		pix->bytesperline = pix->width * 2;
		pix->sizeimage = pix->bytesperline * pix->height;
		dprintk(7, "V4L2_PIX_FMT_UYVY\n");

		if (s5k4ecgx_initialized) {
			dprintk(5, "[1]sleep %dmsec\n", init_delay0);
			msleep(init_delay0);
		}

//		dprintk(6, "preview size[%d][%dx%d]\n", index, 
//				frame_sizes[index].width, 
//				frame_sizes[index].height);

//		if (n_sensor->scene != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_scene(n_sensor->scene);
//
//		if (sensor->scene == S5K4ECGX_SCENE_OFF)
//		{
//			if (n_sensor->effect != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_effect(n_sensor->effect);
//			if (n_sensor->wb != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_wb(n_sensor->wb);
//			if (n_sensor->iso != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_iso(n_sensor->iso); 
//			if (n_sensor->photometry != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_photometry(n_sensor->photometry);
//			if (n_sensor->ev != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_ev(n_sensor->ev);
//			if (n_sensor->wdr != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_wdr(n_sensor->wdr);
//
//			if (n_sensor->wdr == S5K4ECGX_WDR_OFF) {
//				if (n_sensor->contrast != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_contrast(n_sensor->contrast);
//				if (n_sensor->saturation != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_saturation(n_sensor->saturation);
//				if (n_sensor->sharpness != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_sharpness(n_sensor->sharpness);
//			}
//			if (n_sensor->focus_mode != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_focus_mode(n_sensor->focus_mode);
//		}
//		if (n_sensor->capture_size != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_capture_size(n_sensor->capture_size);
//		if (n_sensor->jpeg_quality != S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_jpeg_quality(n_sensor->jpeg_quality);
		if (n_sensor->capture_size != S5K4ECGX_INVALID_VALUE) {
			s5k4ecgx_set_capture_size(n_sensor->capture_size);
			if (sensor->zoom != S5K4ECGX_ZOOM_STEP_0) s5k4ecgx_set_zoom(sensor->zoom);
			if (n_sensor->preview_size == S5K4ECGX_INVALID_VALUE) s5k4ecgx_set_preview_size(sensor->preview_size);
			n_sensor->capture_size = S5K4ECGX_INVALID_VALUE;
		}
		if (n_sensor->preview_size != S5K4ECGX_INVALID_VALUE) {
			dprintk(5, "apply n_sensor preview_size\n");
			s5k4ecgx_set_preview_size(n_sensor->preview_size);
			n_sensor->preview_size = S5K4ECGX_INVALID_VALUE;
		}
		if (n_sensor->camcorder_size != S5K4ECGX_INVALID_VALUE) {
			dprintk(5, "apply n_sensor camcorder_size\n");
			s5k4ecgx_set_preview_size(n_sensor->camcorder_size);
			n_sensor->camcorder_size = S5K4ECGX_INVALID_VALUE;
		}

		if (n_sensor->focus_mode != S5K4ECGX_INVALID_VALUE) {
			dprintk(5, "apply n_sensor focus_mode\n");
			s5k4ecgx_set_focus_mode(n_sensor->focus_mode);
			n_sensor->focus_mode = S5K4ECGX_INVALID_VALUE;
		}


		if (S5K4ECGX_STATE_CAPTURE == s5k4ecgx_pre_state) {
			dprintk(6, "return to preview\n");
			s5k4ecgx_write_regsets(&CAM_REG32SET_RETURN_TO_PREVIEW);
			//if (night_cap_on) s5k4ecgx_write_regsets(&CAM_REG32SET_NIGHT_CAP_OFF);
			if (low_cap_on) {
				s5k4ecgx_write_regsets(&CAM_REG32SET_LOW_CAP_OFF);
				low_cap_on = 0;
				dprintk(6, "low light capture off\n");
			}
		} else {
			if (sensor->mode == S5K4ECGX_MODE_CAMCORDER) {
				if (sensor->camcorder_size != index) {
					s5k4ecgx_wait_1_frame();
					s5k4ecgx_set_preview_size(index);
				}
				// init camcorder wb is not auto. So excute set_wb()
			} else {
				if (sensor->preview_size != index) {
					s5k4ecgx_wait_1_frame();
					s5k4ecgx_set_preview_size(index);
				}
			}

			if (sensor->mode == S5K4ECGX_MODE_CAMCORDER) {
				s5k4ecgx_set_fps(30);
				s5k4ecgx_set_wb(sensor->wb);		
				s5k4ecgx_set_ev(sensor->ev);		
			} else {
				if (sensor->timeperframe.numerator == 0) fps = 0;
				else {
					fps = sensor->timeperframe.denominator / sensor->timeperframe.numerator;
					if (fps == 30) fps = 0;
				}

				if (sensor->scene != S5K4ECGX_SCENE_NIGHT &&
						sensor->scene != S5K4ECGX_SCENE_FIRE) {
					if (sensor->fps == fps) {
						dprintk(6, "same as before : %d fps\n", fps);
					} else {
						s5k4ecgx_set_fps(fps);
					}
				}
			}
		}

		if (s5k4ecgx_initialized) {
			elapsed_msec = jiffies_to_msecs(jiffies - init_jiffies);
			stable_msec = init_delay - elapsed_msec;
			if (stable_msec >= 0) {
				dprintk(5, "[2]sleep : %dmsec\n", stable_msec);
				msleep(stable_msec);
			}
			isp_set_hs_vs(0, init_skip_frame);
			s5k4ecgx_initialized = 0;
		} else {
			s5k4ecgx_set_skip();
		}

		s5k4ecgx_pre_state = s5k4ecgx_curr_state;
		s5k4ecgx_curr_state = S5K4ECGX_STATE_PREVIEW;
	}

	switch (pix->pixelformat) {
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
	struct s5k4ecgx_sensor *sensor = s->priv;
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
	struct s5k4ecgx_sensor *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;

	dprintk(5, "ioctl_g_parm is called...\n");
	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		printk(S5K4ECGX_MOD_NAME "ioctl_g_parm type not supported.\n");
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
	struct s5k4ecgx_sensor *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;

//	dprintk(5, "ioctl_s_parm is called...\n");

	/* Set mode (camera/camcorder/vt) & state (preview/capture) */
	sensor->mode = a->parm.capture.capturemode;
	sensor->state = a->parm.capture.currentstate;
//	dprintk(6, "mode = %d, state = %d\n", sensor->mode, sensor->state);

	/* Set time per frame (FPS) */
	if (timeperframe->numerator == 0) {
		if (timeperframe->denominator != 0) {
			printk("error : numerator is zero\n");
		}
		dprintk(6, "s_parm : try to set auto frame rate\n");
	} else {
		dprintk(6, "s_parm : try to set %d fps\n", timeperframe->denominator / timeperframe->numerator);
	}
	sensor->timeperframe = *timeperframe;
	dprintk(7, "numerator : %d, denominator: %d\n",
			timeperframe->numerator,
			timeperframe->denominator);
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
	struct s5k4ecgx_sensor *sensor = s->priv;
	int rval;

	dprintk(5, "ioctl_g_ifparm is called...\n");
	rval = sensor->pdata->ifparm(p);
	if (rval)
		return rval;

	p->u.bt656.clock_curr = S5K4ECGX_XCLK;
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
	struct s5k4ecgx_sensor *sensor = s->priv;

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
	struct s5k4ecgx_sensor* sensor = s->priv;
	const struct s5k4ecgx_frame_size *frame_sizes;
	int index = 0;

	dprintk(6, "ioctl_enum_framesizes called...\n");

	if (sensor->state == S5K4ECGX_STATE_CAPTURE) {
		frame_sizes = s5k4ecgx_image_sizes;
		index = sensor->capture_size;
//		dprintk(6, "Capture Size Enumeration = %d\n", sensor->capture_size);
	} else {
		if (sensor->mode == S5K4ECGX_MODE_CAMCORDER) {
			frame_sizes = s5k4ecgx_camcorder_sizes;
			index = sensor->camcorder_size;
//			dprintk(6, "Camcorder Size Enumeration = %d\n", sensor->preview_size);
		} else {
			frame_sizes = s5k4ecgx_preview_sizes;
			index = sensor->preview_size;
//			dprintk(6, "Preview Size Enumeration = %d\n", sensor->preview_size);
		}
	}

	frms->index = index;
	frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frms->discrete.width = frame_sizes[index].width;
	frms->discrete.height = frame_sizes[index].height;

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

struct timer_list s5k4ecgx_i2c_bus_unlock_timer;

static void s5k4ecgx_i2c_bus_unlock(unsigned long s_jiffies)
{
	CAM_I2C_CLIENT *client;
	client = cam_i2c_client;

	dprintk(5, "elapsed time : %d msec\n", jiffies_to_msecs(jiffies - s_jiffies));
	if (unlikely(!client)) {
		dprintk(1, "[%s]i2c client is null!!\n", __func__);
		return -ENODEV;
	}
	if (unlikely(!client->adapter)) {
		dprintk(1, "[%s]i2c adapter is null!!\n", __func__);
		return -ENODEV;
	}
	mutex_unlock(&client->adapter->bus_lock);
	dprintk(5, "Released i2c.2 bus_lock\n");
	del_timer_sync(&s5k4ecgx_i2c_bus_unlock_timer);
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
	struct s5k4ecgx_sensor *sensor = s->priv;
	CAM_I2C_CLIENT *client;
	int err = 0;
	int ret = 0;
	int retries = 0;
	int retry_reset = 0;

//	dprintk(5, "ioctl_s_power is called......ON=%x, detect= %x\n", on, sensor->detect);

	switch (on) {
	case V4L2_POWER_ON:
		dprintk(6, "ioctl_s_power[on]\n");
retry_power_on:
#if defined(S5K4ECGX_USE_GPIO_I2C)
		cam_i2c_client = omap_gpio_i2c_init(OMAP_GPIO_CAM_I2C_SDA,
				OMAP_GPIO_CAM_I2C_SCL,
				S5K4ECGX_I2C_ADDR,
				400);

		if (cam_i2c_client == NULL) {
			dprintk(1, "omap_gpio_i2c_init failed!\n");
			return -ENODEV;
		} else {
			dprintk(5, "gpio_i2c init success (sda:%d, scl:%d, addr:%d, %dHz)\n",
					OMAP_GPIO_CAM_I2C_SDA,
					OMAP_GPIO_CAM_I2C_SCL,
					S5K4ECGX_I2C_ADDR,
					400);
		}
#endif
		client = cam_i2c_client;
		if (unlikely(!client)) {
			dprintk(1, "i2c client is null!!\n");
			return -ENODEV;
		}
		if (unlikely(!client->adapter)) {
			dprintk(1, "i2c adapter is null!!\n");
			return -ENODEV;
		}

		ret = mutex_trylock(&client->adapter->bus_lock);
		while (!ret) {
			if (++retries < 50) {
				dprintk(6, "try to acquire i2c.2 bus_lock : %d times\n", retries);
			} else break;
			msleep(10);
			ret = mutex_trylock(&client->adapter->bus_lock);
		} 
		if (ret) {
			dprintk(5, "Acquired i2c.2 bus_lock\n");
			sensor->pdata->power_set(V4L2_POWER_ON);
			if (pwr_on_wait_msec) {dprintk(5, "msleep : %dmsec\n", pwr_on_wait_msec); msleep(pwr_on_wait_msec);}
			mutex_unlock(&client->adapter->bus_lock);
			dprintk(5, "Released i2c.2 bus_lock\n");
		} else {
			printk(S5K4ECGX_MOD_NAME "timeout - trylock : %d times\n", retries);
			sensor->pdata->power_set(V4L2_POWER_ON);
		}

		err = s5k4ecgx_detect(client);
		if (unlikely(err)) {
			printk(S5K4ECGX_MOD_NAME "Unable to detect " S5K4ECGX_DRIVER_NAME " sensor\n");
			sensor->pdata->power_set(V4L2_POWER_OFF);
//			msleep(700);
//			if (retry_reset++ < 3){
//				dprintk(1, "retry(%d) power on\n", retry_reset);
//				goto retry_power_on;
//			}
			return err;
		}

		/* Make the default detect */
		sensor->detect = SENSOR_DETECTED;

		/* Make the state init */
		s5k4ecgx_curr_state = S5K4ECGX_STATE_INVALID;
		break;
	case V4L2_POWER_RESUME:
		dprintk(6, "pwr resume-----!\n");
		break;
	case V4L2_POWER_STANDBY:
		dprintk(6, "pwr stanby-----!\n");
		break;

	case V4L2_POWER_OFF:
		dprintk(6, "ioctl_s_power[off]\n");
		client = cam_i2c_client;
		if (unlikely(!client)) {
			dprintk(1, "i2c client is null!!\n");
			sensor->pdata->power_set(V4L2_POWER_OFF);
			return -ENODEV;
		}
		if (unlikely(!client->adapter)) {
			dprintk(1, "i2c adapter is null!!\n");
			sensor->pdata->power_set(V4L2_POWER_OFF);
			return -ENODEV;
		}

		ret = mutex_trylock(&client->adapter->bus_lock);
		while (!ret) {
			if (++retries < 50) {
				dprintk(6, "try to acquire i2c.2 bus_lock : %d times\n", retries);
			} else break;
			msleep(10);
			ret = mutex_trylock(&client->adapter->bus_lock);
		} 
		if (ret) {
			dprintk(5, "Acquired i2c.2 bus_lock\n");
			sensor->pdata->power_set(V4L2_POWER_OFF);
			if (use_i2c_bus_unlock_timer && pwr_off_wait_msec) {
				dprintk(7, "set bus_unlock timer: %d msec\n", pwr_off_wait_msec); 
				init_timer(&s5k4ecgx_i2c_bus_unlock_timer);
				s5k4ecgx_i2c_bus_unlock_timer.expires = jiffies + msecs_to_jiffies(pwr_off_wait_msec);
				s5k4ecgx_i2c_bus_unlock_timer.data = (unsigned long)jiffies;
				s5k4ecgx_i2c_bus_unlock_timer.function = &s5k4ecgx_i2c_bus_unlock;
				add_timer(&s5k4ecgx_i2c_bus_unlock_timer);
			} else {
				if (pwr_off_wait_msec) {dprintk(5, "msleep : %dmsec\n", pwr_off_wait_msec); msleep(pwr_off_wait_msec);}
				mutex_unlock(&client->adapter->bus_lock);
				dprintk(5, "Released i2c.2 bus_lock\n");
			}
		} else {
			printk(S5K4ECGX_MOD_NAME "timeout - trylock : %d times\n", retries);
			sensor->pdata->power_set(V4L2_POWER_OFF);
		}

#if defined(S5K4ECGX_USE_GPIO_I2C)
		if (client)
			omap_gpio_i2c_deinit(client);
#endif

		/* Make the default detect */
		sensor->detect = SENSOR_NOT_DETECTED;

		/* Make the state init */
		s5k4ecgx_pre_state = S5K4ECGX_STATE_INVALID;
		if (enable_sdcard_tune)
			UnLoad32BitTuningFile();
		break;
	}

	return err;
}

static int ioctl_g_exif (struct v4l2_int_device *s, struct v4l2_exif *exif)
{
	struct s5k4ecgx_sensor *sensor = s->priv;
	CAM_I2C_CLIENT *client = cam_i2c_client;
	u16 lsb, msb,a_gain,d_gain;
	u32 iso_value;
	dprintk(5, "ioctl_g_exif is called\n");

	s5k4ecgx_write_word(client, 0xFCFC, 0xD000);
	s5k4ecgx_write_word(client, 0x002C, 0x7000);
	s5k4ecgx_write_word(client, 0x002E, 0x2BC0);

	s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&lsb);//8
	s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&msb);//A
	s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&a_gain);//C
	s5k4ecgx_read_word(client, 0x0F12, (unsigned short*)&d_gain);//E

	// exposure time
	exif->exposure_time_numerator = (msb << 16) | lsb;
	exif->exposure_time_denominator = 400;

	// ISO
	switch (sensor->iso) {
	case S5K4ECGX_ISO_50:
		exif->iso = 50;
		break;
	case S5K4ECGX_ISO_100:
		exif->iso = 100;
		break;
	case S5K4ECGX_ISO_200:
		exif->iso = 200;
		break;
	case S5K4ECGX_ISO_400:
		exif->iso = 400;
		break;
	case S5K4ECGX_ISO_AUTO:
	default:
		iso_value = (a_gain * d_gain) >> 9;		// ((a_agin * d_gain) / 0x100) / 2
		if (iso_value < 0x100) exif->iso = 50;
		else if (iso_value < 0x1FF) exif->iso = 100;
		else if (iso_value < 0x3FF) exif->iso = 200;
		else exif->iso = 400;
		break;
	}

	dprintk(6, "exposure time:%d/%d, iso:%d\n", 
			exif->exposure_time_numerator, 
			exif->exposure_time_denominator, 
			exif->iso);

	return 0;

g_exif_fail:
	printk("ioctl_g_exif is failed\n");
	return -EINVAL;

}

/**
 * ioctl_deinit - V4L2 sensor interface handler for VIDIOC_INT_DEINIT
 * @s: pointer to standard V4L2 device structure
 *
 * Deinitialize the sensor device
 */
static int ioctl_deinit(struct v4l2_int_device *s)
{
	struct s5k4ecgx_sensor *sensor = s->priv;

	dprintk(5, "ioctl_deinit is called...\n");

	sensor->state = S5K4ECGX_STATE_INVALID;//init problem

	return 0;
}

static int s5k4ecgx_sensor_value_init(struct s5k4ecgx_sensor *sensor)
{
	sensor->timeperframe.numerator	= 0;
	sensor->timeperframe.denominator= 0;
	sensor->fps			= 0;
	sensor->state			= S5K4ECGX_STATE_INVALID;
	sensor->mode			= S5K4ECGX_MODE_CAMERA;
	sensor->camcorder_size		= S5K4ECGX_INVALID_VALUE;
	sensor->preview_size		= S5K4ECGX_PREVIEW_SIZE_640_480;
	sensor->capture_size		= S5K4ECGX_CAPTURE_SIZE_2560_1920;
	sensor->detect			= SENSOR_NOT_DETECTED;
	sensor->effect			= S5K4ECGX_EFFECT_OFF;
	sensor->iso			= S5K4ECGX_ISO_AUTO;
	sensor->photometry		= S5K4ECGX_PHOTOMETRY_CENTER;
	sensor->ev			= S5K4ECGX_EV_DEFAULT;
	sensor->contrast		= S5K4ECGX_CONTRAST_DEFAULT;
	sensor->saturation		= S5K4ECGX_SATURATION_DEFAULT;
	sensor->sharpness		= S5K4ECGX_SHARPNESS_DEFAULT;
	sensor->wb			= S5K4ECGX_WB_AUTO;
	sensor->wdr			= S5K4ECGX_WDR_OFF;
	sensor->scene			= S5K4ECGX_SCENE_OFF;
	sensor->aewb			= S5K4ECGX_AE_UNLOCK;
	sensor->focus_mode		= S5K4ECGX_AF_SET_NORMAL,
	sensor->jpeg_quality		= S5K4ECGX_JPEG_SUPERFINE;
	sensor->thumb_offset		= 0;
	sensor->yuv_offset		= 0;
	sensor->jpeg_capture_w		= JPEG_CAPTURE_WIDTH;
	sensor->jpeg_capture_h		= JPEG_CAPTURE_HEIGHT;
	sensor->check_dataline		= 0;
}



/**
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 *
 * Initialize the sensor device (call S5K4ECGX_configure())
 */
static int ioctl_init(struct v4l2_int_device *s)
{
	struct s5k4ecgx_sensor *sensor = s->priv;

	dprintk(5, "ioctl_init is called...\n");

	s5k4ecgx_write_regsets = s5k4ecgx_normal_write_regsets;

#ifdef S5K4ECGX_USE_BURSTMODE
	if (burst_mode) s5k4ecgx_write_regsets = s5k4ecgx_burst_write_regsets;
#endif
	sensor->timeperframe.numerator	= 0;
	sensor->timeperframe.denominator= 0;
	//sensor->fw_version		= 0;
	sensor->check_dataline		= 0;
	sensor->ae_lock			= S5K4ECGX_AE_UNLOCK;
	sensor->awb_lock		= S5K4ECGX_AWB_UNLOCK;
	sensor->fps			= 0;
	sensor->state			= S5K4ECGX_STATE_INVALID;
	sensor->mode			= S5K4ECGX_MODE_CAMERA;
	sensor->camcorder_size		= S5K4ECGX_INVALID_VALUE;
	sensor->preview_size		= S5K4ECGX_PREVIEW_SIZE_640_480;
	sensor->capture_size		= S5K4ECGX_CAPTURE_SIZE_2560_1920;
	sensor->detect			= SENSOR_NOT_DETECTED;
	sensor->effect			= S5K4ECGX_EFFECT_OFF;
	sensor->iso			= S5K4ECGX_ISO_AUTO;
	sensor->photometry		= S5K4ECGX_PHOTOMETRY_CENTER;
	sensor->ev			= S5K4ECGX_EV_DEFAULT;
	sensor->contrast		= S5K4ECGX_CONTRAST_DEFAULT;
	sensor->saturation		= S5K4ECGX_SATURATION_DEFAULT;
	sensor->sharpness		= S5K4ECGX_SHARPNESS_DEFAULT;
	sensor->wb			= S5K4ECGX_WB_AUTO;
	sensor->wdr			= S5K4ECGX_WDR_OFF;
	sensor->scene			= S5K4ECGX_SCENE_OFF;
	sensor->aewb			= S5K4ECGX_AE_UNLOCK;
	sensor->focus_mode		= S5K4ECGX_AF_SET_NORMAL,
	sensor->jpeg_quality		= S5K4ECGX_JPEG_SUPERFINE;
	sensor->zoom				= S5K4ECGX_ZOOM_STEP_0;
	sensor->thumb_offset		= 0;
	sensor->yuv_offset		= 0;
	sensor->jpeg_capture_w		= JPEG_CAPTURE_WIDTH;
	sensor->jpeg_capture_h		= JPEG_CAPTURE_HEIGHT;

	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_INIT_ARM,			S5K4ECGX_TUNING_INIT_ARM);
	CAMSENSOR_REGSET_INITIALIZE(CAM_REG32SET_INIT,				S5K4ECGX_TUNING_INIT);
	s5k4ecgx_write_regsets(&CAM_REG32SET_INIT_ARM);
	msleep(10);
	s5k4ecgx_write_regsets(&CAM_REG32SET_INIT);

	init_jiffies = jiffies;
	LoadCamsensorRegSettings();

	s5k4ecgx_initialized = 1;
	// chaneg sensor state to RUNMODE.
	// In RUNMODE, if setting should affect directly.
	// In INVALID MODE, setting should affect after initilize sensor.
	return 0;
}

static struct v4l2_int_ioctl_desc s5k4ecgx_ioctl_desc[] = {
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

static struct v4l2_int_slave s5k4ecgx_slave = {
	.ioctls = s5k4ecgx_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(s5k4ecgx_ioctl_desc),
};

static struct v4l2_int_device s5k4ecgx_int_device = {
	.module = THIS_MODULE,
	.name = S5K4ECGX_DRIVER_NAME,
	.priv = &s5k4ecgx,
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &s5k4ecgx_slave,
	},
};


/**
 * s5k4ecgx_probe - sensor driver i2c probe handler
 * @client: i2c driver client device structure
 *
 * Register sensor as an i2c client device and V4L2
 * device.
 */
static int s5k4ecgx_probe(struct i2c_client *client, const struct i2c_device_id *device)
{
	struct s5k4ecgx_sensor *sensor = &s5k4ecgx;

	dprintk(5, "s5k4ecgx_probe is called...\n");
#if !defined(S5K4ECGX_USE_GPIO_I2C)
	if (i2c_get_clientdata(client)) {
		printk(S5K4ECGX_MOD_NAME "can't get i2c client data!!\n");
		return -EBUSY;
	}
#endif

	sensor->pdata = &s5k4ecgx_platform_data0;

	if (!sensor->pdata) {
		printk(S5K4ECGX_MOD_NAME "no platform data!!\n");
		return -ENODEV;
	}

	sensor->v4l2_int_device = &s5k4ecgx_int_device;
#if defined(S5K4ECGX_USE_GPIO_I2C)
	dummy_i2c_client = client;
#else
	cam_i2c_client = client;
#endif

	/* Make the default capture size */
	sensor->pix.width = 2560;
	sensor->pix.height = 1920;


	/* Make the default capture format V4L2_PIX_FMT_UYVY */
	sensor->pix.pixelformat = V4L2_PIX_FMT_JPEG;

#if !defined(S5K4ECGX_USE_GPIO_I2C)
	i2c_set_clientdata(client, sensor);
#endif

	if (v4l2_int_device_register(sensor->v4l2_int_device)) {
		printk(S5K4ECGX_MOD_NAME "fail to init device register \n");
#if !defined(S5K4ECGX_USE_GPIO_I2C)
		i2c_set_clientdata(client, NULL);
#endif
	}

	return 0;
}

/**
 * s5k4ecgx_remove - sensor driver i2c remove handler
 * @client: i2c driver client device structure
 *
 * Unregister sensor as an i2c client device and V4L2
 * device.  Complement of s5k4ecgx_probe().
 */
static int __exit s5k4ecgx_remove(struct i2c_client *client)
{
#if !defined(S5K4ECGX_USE_GPIO_I2C)
	struct s5k4ecgx_sensor *sensor = i2c_get_clientdata(client);
#else
	struct s5k4ecgx_sensor *sensor= &s5k4ecgx;
#endif

	dprintk(5, "s5k4ecgx_remove is called...\n");

#if !defined(S5K4ECGX_USE_GPIO_I2C)
	if (!client->adapter) {
		printk(S5K4ECGX_MOD_NAME "no i2c client adapter!!");
		return -ENODEV;/* our client isn't attached */
	}
#endif

	v4l2_int_device_unregister(sensor->v4l2_int_device);
#if !defined(S5K4ECGX_USE_GPIO_I2C)
	i2c_set_clientdata(client, NULL);
#endif
	cam_i2c_client = NULL;

	return 0;
}

static const struct i2c_device_id s5k4ecgx_id[] = {
	{ S5K4ECGX_DRIVER_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, s5k4ecgx_id);


static struct i2c_driver s5k4ecgx_i2c_driver = {
	.driver = {
		.name = S5K4ECGX_DRIVER_NAME,
	},
	.probe = s5k4ecgx_probe,
	.remove = __exit_p(s5k4ecgx_remove),
	.id_table = s5k4ecgx_id,
};

/**
 * s5k4ecgx_sensor_init - sensor driver module_init handler
 *
 * Registers driver as an i2c client driver.  Returns 0 on success,
 * error code otherwise.
 */
static int __init s5k4ecgx_sensor_init(void)
{
	int err;
	struct s5k4ecgx_sensor *sensor= &s5k4ecgx;

	dprintk(5, "s5k4ecgx_sensor_init is called...\n");

	err = i2c_add_driver(&s5k4ecgx_i2c_driver);
	if (err) {
		printk(S5K4ECGX_MOD_NAME "Failed to register" S5K4ECGX_DRIVER_NAME ".\n");
		return err;
	}
	return 0;
}

module_init(s5k4ecgx_sensor_init);

/**
 * s5k4ecgx_sensor_cleanup - sensor driver module_exit handler
 *
 * Unregisters/deletes driver as an i2c client driver.
 * Complement of s5k4ecgx_sensor_init.
 */
static void __exit s5k4ecgx_sensor_cleanup(void)
{
	i2c_del_driver(&s5k4ecgx_i2c_driver);
}
module_exit(s5k4ecgx_sensor_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("S5K4ECGX camera sensor driver");
