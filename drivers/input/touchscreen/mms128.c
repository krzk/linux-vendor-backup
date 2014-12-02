/*
 * mms_ts.c - Touchscreen driver for Melfas MMS-series touch controllers
 *
 * Copyright (C) 2011 Google Inc.
 * Author: Dima Zavin <dima@android.com>
 *         Simon Wilson <simonwilson@google.com>
 *
 * ISC reflashing code based on original code from Melfas.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#define ISC_DL_MODE		1
#define TSP_TA_CALLBACK		1

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_data/mms128.h>
#include <asm/unaligned.h>
#include <linux/fb.h>
#include <linux/regulator/driver.h>
#include <linux/suspend.h>
#include <linux/pm_qos.h>

#include "mms_ts_w_firmware_rev00.h"
#include "mms_ts_w_config_fw_rev00.h"
#include "mms_ts_w_firmware_rev01.h"
#include "mms_ts_w_config_fw_rev01.h"

#define EVENT_SZ_8_BYTES	8
#define MAX_FINGERS		10
#define MAX_WIDTH		30
#define MAX_PRESSURE		255
#define MAX_ANGLE		90
#define MIN_ANGLE		-90

/* Registers */
#define MMS_INPUT_EVENT_PKT_SZ	0x0F
#define MMS_INPUT_EVENT0	0x10
#define MMS_TSP_REVISION	0xF0
#define MMS_HW_REVISION		0xF1
#define MMS_COMPAT_GROUP	0xF2
#define MMS_FW_VERSION_REG	0xF3

#define MMS_TA_REG	0x32
#define MMS_TA_ON	0x02
#define MMS_TA_OFF	0x03

#define MMS_NOISE_REG	0x30
#define MMS_NOISE_ON	0x01
#define MMS_NOISE_OFF	0x02

enum {
	COMPARE_UPDATE = 0,
	FORCED_UPDATE,
};

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

enum {
	TSP_STATE_RELEASE = 0,
	TSP_STATE_PRESS,
	TSP_STATE_MOVE,
};

enum {
	TOUCH_BOOSTER_DELAY_OFF = 0,
	TOUCH_BOOSTER_ON,
	TOUCH_BOOSTER_QUICK_OFF,
};

#if ISC_DL_MODE	/* ISC_DL_MODE start */
/*
 * ISC_XFER_LEN	- ISC unit transfer length.
 * Give number of 2 power n, where  n is between 2 and 10
 * i.e. 4, 8, 16 ,,, 1024
 */
#define ISC_XFER_LEN		1024

#define MMS_FLASH_PAGE_SZ	1024
#define ISC_BLOCK_NUM		(MMS_FLASH_PAGE_SZ / ISC_XFER_LEN)

#define FLASH_VERBOSE_DEBUG	1
#define MAX_SECTION_NUM		3

#define MAX_FINGER_NUM		5
#define FINGER_EVENT_SZ		8
#define MAX_WIDTH		30
#define MAX_PRESSURE		255
#define MAX_LOG_LENGTH		128

/* Registers */
#define MMS_MODE_CONTROL	0x01
#define MMS_TX_NUM		0x0B
#define MMS_RX_NUM		0x0C
#define MMS_EVENT_PKT_SZ	0x0F
#define MMS_INPUT_EVENT		0x10
#define MMS_UNIVERSAL_CMD	0xA0
#define MMS_UNIVERSAL_RESULT	0xAF
#define MMS_CMD_ENTER_ISC	0x5F
#define MMS_FW_VERSION		0xE1
#define MMS_ERASE_DEFEND	0xB0

/* Runtime config */
#define MMS_RUN_CONF_POINTER	0xA1
#define MMS_GET_RUN_CONF	0xA2
#define MMS_SET_RUN_CONF	0xA3

#define MMS_GET_CONF_VER	0x01

/* Universal commands */
#define MMS_CMD_SET_LOG_MODE	0x20
#define MMS_CMD_CONTROL		0x22
#define MMS_SUBCMD_START	0x80

/* Event types */
#define MMS_LOG_EVENT		0xD
#define MMS_NOTIFY_EVENT	0xE
#define MMS_ERROR_EVENT		0xF
#define MMS_TOUCH_KEY_EVENT	0x40

/* Firmware file name */
#define TSP_FW_NAME		"mms128.fw"
#define TSP_FW_CONFIG_NAME	"mms128_config.fw"
#define MAX_FW_PATH	255
#define TSP_FW_DIRECTORY	"tsp_melfas/"

/* Firmware Start Control */
#define RUN_START		0
#define RUN_STOP		1

/* mfsp offset */
#define MMS_MFSP_OFFSET		16

enum {
	SEC_NONE = -1,
	SEC_BOOT = 0,
	SEC_CORE,
	SEC_CONF,
	SEC_LIMIT
};

enum {
	ISC_ADDR		= 0xD5,

	ISC_CMD_READ_STATUS	= 0xD9,
	ISC_CMD_READ		= 0x4000,
	ISC_CMD_EXIT		= 0x8200,
	ISC_CMD_PAGE_ERASE	= 0xC000,

	ISC_PAGE_ERASE_DONE	= 0x10000,
	ISC_PAGE_ERASE_ENTER	= 0x20000,
};

enum {
	MMS_RUN_TYPE_SINGLE	= 1,
	MMS_RUN_TYPE_ARRAY,
	MMS_RUN_TYPE_END,
	MMS_RUN_TYPE_INFO,
	MMS_RUN_TYPE_UNKNOWN,
};

struct mms_config_item {
        u16	type;
        u16	category;
        u16	offset;
        u16	datasize;
	u16	data_blocksize;
        u16	reserved;

        u32     value;
} __attribute__ ((packed));

struct mms_config_hdr {
	char	mark[4];

	char	tag[4];

	u32	core_version;
	u32	config_version;
	u32	data_offset;
	u32	data_count;

	u32	reserved0;
	u32	info_offset;
	u32	reserved2;
	u32	reserved3;
	u32	reserved4;
	u32	reserved5;

} __attribute__ ((packed));

struct mms_bin_hdr {
	char	tag[8];
	u16	core_version;
	u16	section_num;
	u16	contains_full_binary;
	u16	reserved0;

	u32	binary_offset;
	u32	binary_length;

	u32	extention_offset;
	u32	reserved1;

} __attribute__ ((packed));

struct mms_fw_img {
	u16	type;
	u16	version;

	u16	start_page;
	u16	end_page;

	u32	offset;
	u32	length;

} __attribute__ ((packed));

struct isc_packet {
	u8	cmd;
	u32	addr;
	u8	data[0];
} __attribute__ ((packed));

#endif	/* ISC_DL_MODE end */

struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *tsp_cb, bool mode);
};

struct mms_ts_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	char	phys[32];
	int	max_x;
	int	max_y;
	int	invert_x;
	int	invert_y;
	int	irq;
	void	(*input_event)(void *data);
	bool	enabled;
	bool	enabled_user;
	u8	fw_ic_ver[3];
	int	finger_byte;
	const u8	*config_fw_version;
	unsigned char	finger_state[MAX_FINGERS];
	u16	mcount[MAX_FINGERS];

	struct melfas_tsi_platform_data *pdata;
	struct mutex lock;

	void (*register_cb)(void *);
	struct tsp_callbacks callbacks;
	bool	ta_status;
	bool	noise_mode;

	char	*fw_name;
	u8	*fw_data;
	u8	*config_fw;

	bool	resume_done;

	struct regulator *regulator_pwr;
	struct regulator *regulator_vdd;

	struct delayed_work work_config_set;

	struct notifier_block pm_notifier;
	bool suspended;

	struct pm_qos_request		busfreq_pm_qos_req;
	struct pm_qos_request		cpufreq_pm_qos_req;
};

static int mms_ts_power(struct mms_ts_info *info, int on)
{
	int ret = 0;

	if (info->enabled == on) {
		pr_err("melfas-ts : %s same state!", __func__);
		return 0;
	}

	if (on) {
		ret = regulator_enable(info->regulator_pwr);
		if (ret) {
			pr_err("melfas-ts : %s failed to enable regulator_pwr\n",
					__func__);
			return ret;
		}
		usleep_range(2500, 3000);
		ret = regulator_enable(info->regulator_vdd);
		if (ret) {
			pr_err("melfas-ts : %s failed to enable regulator_vdd\n",
					__func__);
			return ret;
		}
	} else {
		if (regulator_is_enabled(info->regulator_pwr))
			regulator_disable(info->regulator_pwr);
		else
			regulator_force_disable(info->regulator_pwr);

		if (regulator_is_enabled(info->regulator_vdd))
			regulator_disable(info->regulator_vdd);
		else
			regulator_force_disable(info->regulator_vdd);
	}

	if (regulator_is_enabled(info->regulator_pwr) == !!on &&
		regulator_is_enabled(info->regulator_vdd) == !!on) {
		info->enabled = on;
	} else {
		pr_err("melfas-ts : regulator_is_enabled value error!");
		ret = -1;
	}

	return ret;
}

static irqreturn_t mms_ts_interrupt(int irq, void *dev_id);

#if ISC_DL_MODE // watch
static int mms_read_config(struct i2c_client *client, u8 *buf, u8 *get_buf,int len);
static int mms_config_flash(struct mms_ts_info *info, const u8 *buf, const u8 len, char *text);
static int mms_config_start(struct mms_ts_info *info);
static int mms_config_finish(struct mms_ts_info *info);
static void mms_config_set(void *context);
static int mms_config_get(struct mms_ts_info *info, u8 mode);
static void mms_reboot(struct mms_ts_info *info)
{
	struct i2c_adapter *adapter = to_i2c_adapter(info->client->dev.parent);
	i2c_smbus_write_byte_data(info->client, MMS_ERASE_DEFEND, 1);

	i2c_lock_adapter(adapter);

	mms_ts_power(info, 0);
	msleep(1);
	mms_ts_power(info, 1);
	msleep(25);

	i2c_unlock_adapter(adapter);
}

static int mms_verify_config(struct mms_ts_info *info,const u8 *buf,const u8 *tmp,int len)
{
	int count = 0;
	int ret = 0 ;
	if (memcmp(buf, tmp , len))
	{
		dev_info(&info->client->dev, "Run-time config verify failed\n");
		mms_reboot(info);
		mms_config_set(info);
                ret = 1;
		count++;
        }
	if (count > 20){
		mms_reboot(info);
		dev_info(&info->client->dev, "Run-time config failed\n");
		ret = 1;
	}
	return ret;
}

static int mms_read_config(struct i2c_client *client, u8 *buf, u8 *get_buf,int len)
{
        u8 cmd = MMS_GET_RUN_CONF;
	struct i2c_msg msg[3] = {
                {
                        .addr = client->addr,
                        .flags = 0,
                        .buf = buf,
                        .len = 4,
                }, {
			.addr =client ->addr,
			.flags = 0,
			.buf = &cmd,
			.len = 1,
		}, {
                        .addr = client->addr,
                        .flags = I2C_M_RD,
                        .buf = get_buf,
                        .len = len,
                },
        };
	return (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg));
}

static int mms_config_flash(struct mms_ts_info *info, const u8 *buf,const u8 len, char *text)
{
	struct i2c_client *client;
	struct i2c_msg msg;
	int ret;
	client = info->client;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.buf = (u8 *)buf;
	msg.len = len;

        if(i2c_transfer(client->adapter, &msg,1) != 1){
                dev_err(&client->dev, "failed to transfer %s data\n",text);
		mms_reboot(info);
                mms_config_set(info);
                return 0;
        }else{
		ret = 1;
	}
	return ret;
}

static void mms_config_set(void *context)
{
	struct mms_ts_info *info = context;
	struct i2c_client *client = info->client;
	struct mms_config_hdr	*conf_hdr;
	struct mms_config_item **conf_item;
	int offset;
	int offset_tmp = 0;
	int i;
	u8 config_flash_set[4];
	u8 config_flash_data[5];
	u8 *config_array;
	u8 flash_array[50];
	u8 cmp_data[30];

	if (!info->enabled){
		dev_info(&info->client->dev, "%s: power off\n", __func__);
		return;
	}

	if(info->config_fw== NULL) {
		dev_err(&client->dev, "failed to get config fw\n");
		return;
	}

	mms_config_start(info);

	conf_hdr = (struct mms_config_hdr *)info->config_fw;

	if ((conf_hdr->core_version & 0xff ) != info->fw_ic_ver[1]){
		mms_reboot(info);
		dev_err(&client->dev, "mfsp-version is not correct : 0x%x 0x%x :: 0x%x 0x%x\n",
			conf_hdr->core_version, conf_hdr->config_version& 0xff,info->fw_ic_ver[1],info->fw_ic_ver[2]);
		return;
	}

	if (conf_hdr->mark[3] != 0x02){
		mms_reboot(info);
		dev_err(&client->dev, "failed to mfsp-version : %x \n",conf_hdr->mark[3]);
		return;
	}

	offset = conf_hdr->data_offset;
	conf_item = kzalloc(sizeof(*conf_item) * (conf_hdr->data_count + 1),
			GFP_KERNEL);

	for (i=0 ;; i++ , offset += MMS_MFSP_OFFSET)
	{
		conf_item[i] = (struct mms_config_item *)(info->config_fw + offset);

		if (i == MMS_GET_CONF_VER)
			dev_info(&info->client->dev, "Runtime Conf_Ver[%02x]\n",
							conf_item[i]->value);

		if(conf_item[i]->type == MMS_RUN_TYPE_INFO)
		{
			offset_tmp = conf_item[i]->data_blocksize;
			offset += offset_tmp;
		}

		if(conf_item[i]->type == MMS_RUN_TYPE_SINGLE)
		{
			config_flash_set[0] = MMS_RUN_CONF_POINTER;
			config_flash_set[1] = conf_item[i]->category;
			config_flash_set[2] = conf_item[i]->offset;
			config_flash_set[3] = conf_item[i]->datasize;

			mms_config_flash(info, config_flash_set,4,"config-set");
		}
		if(conf_item[i]->type == MMS_RUN_TYPE_ARRAY)
		{
			config_flash_set[0] = MMS_RUN_CONF_POINTER;
			config_flash_set[1] = conf_item[i]->category;
			config_flash_set[2] = conf_item[i]->offset;
			config_flash_set[3] = conf_item[i]->datasize;

			mms_config_flash(info,config_flash_set,4,"array-set");

                        offset_tmp = conf_item[i]->data_blocksize;
                        config_array =(u8 *)(info->config_fw + (offset + MMS_MFSP_OFFSET));
			offset += offset_tmp;

			flash_array[0]=MMS_SET_RUN_CONF;
			memcpy(&flash_array[1], config_array, conf_item[i]->datasize);

			mms_config_flash(info, flash_array, conf_item[i]->datasize + 1,"array_data");
			mms_read_config(client, config_flash_set, cmp_data, conf_item[i]->datasize);
			if (mms_verify_config(info, &flash_array[1], cmp_data, conf_item[i]->datasize)!=0)
			{
				break;
			}
		}

		config_flash_data[0] = MMS_SET_RUN_CONF;
		if(conf_item[i]->datasize == 1)
		{
			config_flash_data[1] = (u8)conf_item[i]->value;
			mms_config_flash(info,config_flash_data,2,"config-data1");
			mms_read_config(client, config_flash_set, cmp_data,
				   conf_item[i]->datasize);

                        if (mms_verify_config(info, &config_flash_data[1], cmp_data, 1)!=0)
                        {
                                break;
                        }

		}
		else if(conf_item[i]->datasize == 2)
		{
			config_flash_data[1] = (u8)((conf_item[i]->value&0x00FF)>>0);
			config_flash_data[2] = (u8)((conf_item[i]->value&0xFF00)>>8);
			mms_config_flash(info,config_flash_data,3,"config-data2");
			mms_read_config(client, config_flash_set, cmp_data,
				   conf_item[i]->datasize);
                        if (mms_verify_config(info, &config_flash_data[1], cmp_data, 2)!=0)
                        {
                                break;
                        }
		}
		else if(conf_item[i]->datasize == 4)
		{
			config_flash_data[1] = (u8)((conf_item[i]->value&0x000000FF)>>0);
			config_flash_data[2] = (u8)((conf_item[i]->value&0x0000FF00)>>8);
			config_flash_data[3] = (u8)((conf_item[i]->value&0x00FF0000)>>16);
			config_flash_data[4] = (u8)((conf_item[i]->value&0xFF000000)>>24);
			mms_config_flash(info,config_flash_data,5,"config-data4");
			mms_read_config(client, config_flash_set, cmp_data,
				   conf_item[i]->datasize);

                        if (mms_verify_config(info, &config_flash_data[1], cmp_data, 4)!=0)
                        {
                                break;
                        }
		}
		if(conf_item[i]->type == MMS_RUN_TYPE_END)
		{
			mms_config_finish(info);
			break;
		}

	}

	kfree(conf_item);
	return;
}
static int mms_config_get(struct mms_ts_info *info, u8 mode)
{
	struct i2c_client *client = info->client;
	char fw_path[MAX_FW_PATH+1];
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;

	if (mode == REQ_FW) {
		if (!info->config_fw) {
			dev_err(&client->dev, "Failed to get config firmware\n");
			return -EINVAL;
		}
	} else if (mode == UMS) {

		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FW_PATH, "/sdcard/%s", TSP_FW_CONFIG_NAME);
		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			dev_err(&client->dev, "file %s open error:%d\n",
					fw_path, (s32)fp);
			return -1;
		} else {
			fsize = fp->f_path.dentry->d_inode->i_size;
			kfree(info->config_fw);
			info->config_fw = kzalloc((size_t)fsize, GFP_KERNEL);

			nread = vfs_read(fp, (char __user *)info->config_fw, fsize, &fp->f_pos);
			if (nread != fsize) {
				dev_err(&client->dev, "nread != fsize error\n");
			}

			filp_close(fp, current->files);
		}
		set_fs(old_fs);
	} else {
		dev_err(&client->dev, "%s error mode[%d]\n", __func__, mode);
		return -1;
	}

	dev_info(&client->dev, "succeed to get runtime-config firmware\n");

	return 0;
}
static int mms_config_start(struct mms_ts_info *info)
{
	struct i2c_client *client;
	u8 mms_conf_buffer[4] = {MMS_UNIVERSAL_CMD, MMS_CMD_CONTROL, MMS_SUBCMD_START, RUN_START};
	client = info->client;

	dev_info(&client->dev, "runtime-config firmware update start!\n");
	msleep(40);
	mms_config_flash(info, mms_conf_buffer, 4, "start-packit");
	return 0;
}
static int mms_config_finish(struct mms_ts_info *info)
{
	struct i2c_client *client;
	u8 mms_conf_buffer[4] = {MMS_UNIVERSAL_CMD, MMS_CMD_CONTROL, MMS_SUBCMD_START, RUN_STOP};
	client = info->client;

	mms_config_flash(info, mms_conf_buffer, 4,"finish-packit" );
	dev_info(&client->dev, "succeed to runtime-config firmware update\n");
	return 0;
}
static int mms_isc_read_status(struct mms_ts_info *info, u32 val)
{
	struct i2c_client *client = info->client;
	u8 cmd = ISC_CMD_READ_STATUS;
	u32 result = 0;
	int cnt = 100;
	int ret = 0;

	do {
		i2c_smbus_read_i2c_block_data(client, cmd, 4, (u8 *)&result);
		if (result == val)
			break;
		msleep(1);
	} while (--cnt);

	if (!cnt){
		dev_err(&client->dev,
			"status read fail. cnt : %d, val : 0x%x != 0x%x\n",
			cnt, result, val);
	}

	return ret;
}

static int mms_isc_transfer_cmd(struct mms_ts_info *info, int cmd)
{
	struct i2c_client *client = info->client;
	struct isc_packet pkt = { ISC_ADDR, cmd };
	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = sizeof(struct isc_packet),
		.buf = (u8 *)&pkt,
	};

	return (i2c_transfer(client->adapter, &msg, 1) != 1);
}

static int mms_isc_erase_page(struct mms_ts_info *info, int page)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_PAGE_ERASE | page) ||
		mms_isc_read_status(info, ISC_PAGE_ERASE_DONE | ISC_PAGE_ERASE_ENTER | page);
}

static int mms_isc_enter(struct mms_ts_info *info)
{
	return i2c_smbus_write_byte_data(info->client, MMS_CMD_ENTER_ISC, true);
}

static int mms_isc_exit(struct mms_ts_info *info)
{
	return mms_isc_transfer_cmd(info, ISC_CMD_EXIT);
}

static int mms_flash_section(struct mms_ts_info *info, struct mms_fw_img *img, const u8 *data)
{
	struct i2c_client *client = info->client;
	struct isc_packet *isc_packet;
	int ret;
	int page, i;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = ISC_XFER_LEN,
		},
	};
	int ptr = img->offset;

	isc_packet = kzalloc(sizeof(*isc_packet) + ISC_XFER_LEN, GFP_KERNEL);
	isc_packet->cmd = ISC_ADDR;

	msg[0].buf = (u8 *)isc_packet;
	msg[1].buf = kzalloc(ISC_XFER_LEN, GFP_KERNEL);

	for (page = img->start_page; page <= img->end_page; page++) {
		mms_isc_erase_page(info, page);

		for (i = 0; i < ISC_BLOCK_NUM; i++, ptr += ISC_XFER_LEN) {
			/* flash firmware */
			u32 tmp = page * 256 + i * (ISC_XFER_LEN / 4);
			put_unaligned_le32(tmp, &isc_packet->addr);
			msg[0].len = sizeof(struct isc_packet) + ISC_XFER_LEN;

			memcpy(isc_packet->data, data + ptr, ISC_XFER_LEN);
			if (i2c_transfer(client->adapter, msg, 1) != 1)
				goto i2c_err;

			/* verify firmware */
			tmp |= ISC_CMD_READ;
			put_unaligned_le32(tmp, &isc_packet->addr);
			msg[0].len = sizeof(struct isc_packet);

			if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg))
				goto i2c_err;

			if (memcmp(isc_packet->data, msg[1].buf, ISC_XFER_LEN)) {
#if FLASH_VERBOSE_DEBUG
				print_hex_dump(KERN_ERR, "mms fw wr : ",
						DUMP_PREFIX_OFFSET, 16, 1,
						isc_packet->data, ISC_XFER_LEN, false);

				print_hex_dump(KERN_ERR, "mms fw rd : ",
						DUMP_PREFIX_OFFSET, 16, 1,
						msg[1].buf, ISC_XFER_LEN, false);
#endif
				dev_err(&client->dev, "flash verify failed\n");
				ret = -1;
				goto out;
			}

		}
	}

	dev_info(&client->dev, "section [%d] update succeeded\n", img->type);

	ret = 0;
	goto out;

i2c_err:
	dev_err(&client->dev, "i2c failed @ %s\n", __func__);
	ret = -1;

out:
	kfree(isc_packet);
	kfree(msg[1].buf);

	return ret;
}

static int get_fw_version_ic(struct i2c_client *client, u8 *buf)
{
	u8 cmd = MMS_FW_VERSION;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &cmd,
			.len = 1,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
			.len = MAX_SECTION_NUM,
		},
	};

	return (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg));
}

static int mms_flash_fw(const u8 *fw_data, struct mms_ts_info *info, u8 update_mode)
{
	int ret = 0;
	int i;
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img **img;
	struct i2c_client *client = info->client;
	u8 ver[MAX_SECTION_NUM];
	u8 target[MAX_SECTION_NUM];
	int offset = sizeof(struct mms_bin_hdr);
	int retires = 3;
	bool update_flag = false;
	bool isc_enter_flag = false;

	if (fw_data == NULL) {
		dev_err(&client->dev, "mms_flash_fw fw_data is NULL!");
		return 1;
	}

	fw_hdr = (struct mms_bin_hdr *)fw_data;
	img = kzalloc(sizeof(*img) * fw_hdr->section_num, GFP_KERNEL);

	while (retires--) {
		if (!get_fw_version_ic(client, ver))
			break;
		else
			mms_reboot(info);
	}

	if (retires < 0) {
		dev_warn(&client->dev, "failed to obtain ver. info\n");
		memset(ver, 0xff, sizeof(ver));
	}

	for (i = 0 ; i < MAX_SECTION_NUM; i++)
		info->fw_ic_ver[i] = ver[i];
	dev_info(&client->dev, "MMS-128S Before FW update : [0x%02x][0x%02x][0x%02x]",
				ver[SEC_BOOT], ver[SEC_CORE], ver[SEC_CONF]);

	for (i = 0; i < fw_hdr->section_num; i++, offset += sizeof(struct mms_fw_img)) {
		img[i] = (struct mms_fw_img *)(fw_data + offset);
		target[i] = img[i]->version;

		if(update_mode == COMPARE_UPDATE)
			info->pdata->fw_bin_ver[i] = target[i];

		if (update_mode == FORCED_UPDATE) {
			update_flag = true;
			dev_info(&client->dev, "section [%d] forced update mode. ver : 0x%x, bin : 0x%x\n",
					img[i]->type, ver[img[i]->type], target[i]);
		} else if (ver[img[i]->type] == 0xff) {
			update_flag = true;
			dev_info(&client->dev, "tsp ic fw error : section [%d] is need to be updated\n",
					img[i]->type);
		/* defence code for empty tsp ic */
		} else if (ver[img[i]->type] > 0x30) {
			update_flag = true;
			dev_info(&client->dev, "empty tsp ic : section [%d] is need to be updated\n",
					img[i]->type);
		} else if (ver[img[i]->type] < target[i]) {
			update_flag = true;
			dev_info(&client->dev, "section [%d] is need to be updated. ver : 0x%x, bin : 0x%x\n",
					img[i]->type, ver[img[i]->type], target[i]);
		} else {
			dev_info(&client->dev, "section [%d] is up to date, ver : 0x%x\n", i, target[i]);
		}

		if (update_flag) {
			if (!isc_enter_flag) {
				mms_isc_enter(info);
				isc_enter_flag = true;
			}

			ret = mms_flash_section(info, img[i], fw_data + fw_hdr->binary_offset);
			update_flag = false;

			if (ret) {
				dev_err(&client->dev, "failed to mms_flash_section[%d]\n", i);
				mms_reboot(info);
				goto out;
			}
		}
		info->fw_ic_ver[i] = target[i];
	}

	if (isc_enter_flag) {
		mms_isc_exit(info);
		mms_reboot(info);

		if (get_fw_version_ic(client, ver)) {
			dev_err(&client->dev, "failed to obtain version after flash\n");
			ret = -1;
			goto out;
		} else {
			for (i = 0; i < fw_hdr->section_num; i++) {
				if (ver[img[i]->type] != target[i]) {
					dev_info(&client->dev,
						"version mismatch after flash. [%d] 0x%02x != 0x%02x\n",
						i, ver[img[i]->type], target[i]);

					ret = -1;
					goto out;
				}
			}
		}
	}

out:
	kfree(img);

	return ret;
}
#endif	/* ISC_DL_MODE */

static void work_mms_config_set(struct work_struct *work)
{
	struct mms_ts_info *info = container_of(work,
				struct mms_ts_info, work_config_set.work);
	mms_config_set(info);

#if TSP_TA_CALLBACK
	dev_info(&info->client->dev, "%s & TA %sconnect & noise mode %s!\n",
					__func__,
					info->ta_status ? "" : "dis",
					info->noise_mode ? "on" : "off");

	if (info->ta_status) {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_ON);
		if (info->noise_mode)
			i2c_smbus_write_byte_data(info->client, MMS_NOISE_REG, MMS_NOISE_ON);

	} else {
		if (info->noise_mode)
			info->noise_mode = false;
	}
#endif

	enable_irq(info->irq);
	info->resume_done = true;

}


static void mms_ts_pm_qos_request(struct mms_ts_info *info,
				  unsigned int cpu_freq, unsigned int bus_freq)
{
	if (!pm_qos_request_active(&info->cpufreq_pm_qos_req))
		pm_qos_add_request(&info->cpufreq_pm_qos_req,
				   PM_QOS_CPU_FREQUENCY, cpu_freq);
	else
		pm_qos_update_request(&info->cpufreq_pm_qos_req, cpu_freq);

	if (!pm_qos_request_active(&info->busfreq_pm_qos_req))
		pm_qos_add_request(&info->busfreq_pm_qos_req,
				   PM_QOS_BUS_FREQUENCY, bus_freq);
	else
		pm_qos_update_request(&info->busfreq_pm_qos_req, bus_freq);
}

static void release_all_fingers(struct mms_ts_info *info)
{
	int i;

	dev_notice(&info->client->dev, "%s\n", __func__);

	for (i = 0; i < MAX_FINGERS; i++) {
		input_mt_slot(info->input_dev, i);
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER,
					   false);

		if (info->finger_state[i] != TSP_STATE_RELEASE) {
			dev_notice(&info->client->dev,
						"finger %d up(force)\n", i);
		}
		info->finger_state[i] = TSP_STATE_RELEASE;
		info->mcount[i] = 0;
	}
	input_sync(info->input_dev);

	mms_ts_pm_qos_request(info, PM_QOS_CPU_FREQUENCY_MIN_VALUE,
			      PM_QOS_BUS_FREQUENCY_MIN_VALUE);
}

static void reset_mms_ts(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int retries_off = 3, retries_on = 3;

#if TSP_TA_CALLBACK
	dev_info(&client->dev, "%s & tsp %s & TA %sconnect & noise mode %s!\n",
					__func__,
					info->enabled ? "on" : "off",
					info->ta_status ? "" : "dis",
					info->noise_mode ? "on" : "off");
#else
	dev_info(&client->dev, "%s called & tap %s!\n", __func__,
					info->enabled ? "on" : "off");
#endif

	if (info->enabled == false)
		return;

	release_all_fingers(info);

	while(retries_off--) {
		if(!mms_ts_power(info, 0))
			break;
	}

	if (retries_off < 0) {
		dev_err(&info->client->dev, "%s : power off error!\n", __func__);
		return;
	}

	msleep(30);

	while(retries_on--) {
		if(!mms_ts_power(info, 1))
			break;
	}

	if (retries_on < 0) {
		dev_err(&info->client->dev, "%s : power on error!\n", __func__);
		return;
	}

	msleep(120);

	mms_config_set(info);

#if TSP_TA_CALLBACK
	if (info->ta_status) {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_ON);
		if (info->noise_mode)
			i2c_smbus_write_byte_data(info->client, MMS_NOISE_REG, MMS_NOISE_ON);
	} else
		info->noise_mode = false;
#endif
}

static void melfas_ta_cb(struct tsp_callbacks *cb, bool ta_status)
{
	struct mms_ts_info *info =
			container_of(cb, struct mms_ts_info, callbacks);
	struct i2c_client *client = info->client;

	info->ta_status = ta_status;

	dev_info(&client->dev, "%s & tsp %s & TA %sconnect & noise mode %s!\n",
					__func__,
					info->enabled ? "on" : "off",
					info->ta_status ? "" : "dis",
					info->noise_mode ? "on" : "off");

	if (info->enabled == false)
		return;

#if TSP_TA_CALLBACK
	if (info->ta_status) {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_ON);

		if (info->noise_mode) {
			i2c_smbus_write_byte_data(info->client, MMS_NOISE_REG, MMS_NOISE_ON);
			dev_err(&client->dev, "melfas_ta_cb & abnormal noise mode on!\n");
		}

	} else {
		i2c_smbus_write_byte_data(info->client, MMS_TA_REG, MMS_TA_OFF);

		if (info->noise_mode) {
			info->noise_mode = false;
			i2c_smbus_write_byte_data(info->client, MMS_NOISE_REG, MMS_NOISE_OFF);
			dev_info(&client->dev, "ta_cb & noise mode off!\n");
		}
	}
#endif
}

static irqreturn_t mms_ts_interrupt(int irq, void *dev_id)
{
	struct mms_ts_info *info = dev_id;
	struct i2c_client *client = info->client;
	u8 buf[MAX_FINGERS * EVENT_SZ_8_BYTES] = { 0, };
	int ret, i, sz;
	int id, state, posX, posY, strenth, width;
	int palm, major_axis, minor_axis;
	int finger_event_sz;
	u8 *read_data;
	u8 reg = MMS_INPUT_EVENT0;
	bool press_flag = false;
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .buf = &reg,
		 .len = 1,
		 }, {
		     .addr = client->addr,
		     .flags = I2C_M_RD,
		     .buf = buf,
		     },
	};
	finger_event_sz = EVENT_SZ_8_BYTES;

	if (WARN_ON(info->suspended))
		return IRQ_HANDLED;

	sz = i2c_smbus_read_byte_data(client, MMS_INPUT_EVENT_PKT_SZ);

	if (sz < 0) {
		dev_err(&client->dev, "%s bytes=%d\n", __func__, sz);
		for (i = 0; i < 50; i++) {
			sz = i2c_smbus_read_byte_data(client,
						      MMS_INPUT_EVENT_PKT_SZ);
			if (sz > 0)
				break;
		}

		if (i == 50) {
			dev_dbg(&client->dev, "i2c failed... reset!!\n");
			reset_mms_ts(info);
			return IRQ_HANDLED;
		}
		dev_err(&client->dev, "success read touch info data\n");
	}
	if (sz == 0)
		return IRQ_HANDLED;

	if (sz > MAX_FINGERS*finger_event_sz || sz%finger_event_sz) {
		dev_err(&client->dev, "abnormal data inputed & reset IC[%d]\n",
									sz);
		reset_mms_ts(info);
		return IRQ_HANDLED;
	}

	msg[1].len = sz;
	ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));

	if (ret != ARRAY_SIZE(msg)) {
		dev_err(&client->dev,
			"failed to read %d bytes of touch data (%d)\n",
			sz, ret);

		for (i = 0; i < 5; i++) {
			ret = i2c_transfer(client->adapter, msg,
							ARRAY_SIZE(msg));
			if (ret == ARRAY_SIZE(msg))
				break;
		}

		if (i == 5) {
			dev_err(&client->dev,
				"failed to read touch data & reset IC[%d]\n",
									ret);
			reset_mms_ts(info);
			return IRQ_HANDLED;
		}
		dev_err(&client->dev, "success read touch data\n");
	}

	if (buf[0] == MMS_NOTIFY_EVENT) {	/* MMS_NOTIFY_EVENT */
		dev_info(&client->dev, "TSP noise mode enter!(%d)\n", buf[1]);
		i2c_smbus_write_byte_data(info->client, MMS_NOISE_REG, MMS_NOISE_ON);
		if (!info->ta_status) {
			dev_err(&client->dev, "TSP noise mode enter but ta off!\n");
		}
		info->noise_mode = true;
		return IRQ_HANDLED;
	}

	if (buf[0] == MMS_ERROR_EVENT) { /* MMS_ERROR_EVENT*/
		dev_err(&client->dev, "Error detected, restarting TSP\n");
		reset_mms_ts(info);
		return IRQ_HANDLED;
	}

	for (i = 0; i < sz; i += finger_event_sz) {
		read_data = &buf[i];
		id = (read_data[0] & 0xf) - 1;
		state = read_data[0] & 0x80;
		posX = read_data[2] | ((read_data[1] & 0xf) << 8);
		posY = read_data[3] | (((read_data[1] >> 4) & 0xf) << 8);
		width = read_data[4];
		strenth = read_data[5];
		palm = (read_data[0] & 0x10) >> 4;
		major_axis = read_data[6];
		minor_axis = read_data[7];

		if (info->invert_x) {
			posX = info->max_x - posX;
			if (posX < 0)
				posX = 0;
		}
		if (info->invert_y) {
			posY = info->max_y - posY;
			if (posY < 0)
				posY = 0;
		}
		if (id >= MAX_FINGERS) {
			dev_notice(&client->dev, \
				"finger id error [%d]\n", id);
			reset_mms_ts(info);
			return IRQ_HANDLED;
		}

		if (state == TSP_STATE_RELEASE) {
			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, false);
			dev_dbg(&client->dev, "R [%2d],([%4d],[%3d])[%d][%d]",
				id, posX, posY,	palm, info->mcount[id]);
			info->finger_state[id] = TSP_STATE_RELEASE;
			info->mcount[id] = 0;
		} else {
			input_mt_slot(info->input_dev, id);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, true);
			input_report_abs(info->input_dev,
						ABS_MT_POSITION_X, posX);
			input_report_abs(info->input_dev,
						ABS_MT_POSITION_Y, posY);

			input_report_abs(info->input_dev,
						ABS_MT_WIDTH_MAJOR, width);
			input_report_abs(info->input_dev,
						ABS_MT_PRESSURE, strenth);
			input_report_abs(info->input_dev,
						ABS_MT_TOUCH_MAJOR, major_axis);
			input_report_abs(info->input_dev,
						ABS_MT_TOUCH_MINOR, minor_axis);

			input_report_abs(info->input_dev,
						ABS_MT_TOOL_Y, palm);

			info->mcount[id] += 1;

			if (info->finger_state[id] == TSP_STATE_RELEASE) {
				info->finger_state[id] = TSP_STATE_PRESS;
				dev_dbg(&client->dev,
		"P [%2d],([%4d],[%3d]),P:%d W:%d S:%3d Mj_a:%d Mi_a:%d",\
					id, posX, posY, palm, width, strenth,
					major_axis, minor_axis);
			} else
				info->finger_state[id] = TSP_STATE_MOVE;
		}
	}
	input_sync(info->input_dev);


	for (i = 0 ; i < MAX_FINGERS ; i++) {
		if (info->finger_state[i] == TSP_STATE_PRESS
			|| info->finger_state[i] == TSP_STATE_MOVE) {
			press_flag = TOUCH_BOOSTER_ON;
			break;
		}
	}

	if (press_flag == TOUCH_BOOSTER_ON) {
		/* cpu frequency : 400MHz, bus frequency : 100MHz */
		mms_ts_pm_qos_request(info, 400000, 100000);
	} else {
		mms_ts_pm_qos_request(info, PM_QOS_CPU_FREQUENCY_MIN_VALUE,
				      PM_QOS_BUS_FREQUENCY_MIN_VALUE);
	}

	return IRQ_HANDLED;
}

static int mms_ts_fw_load(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	int ret = 0, retries = 3;

	if (!info->pdata) {
		dev_err(&client->dev,
			"fw cannot be updated, missing platform data\n");
		return 1;
	}

	do {
		ret = mms_flash_fw(info->fw_data, info, COMPARE_UPDATE);
	} while (ret && --retries);

	if (!retries)
		dev_err(&info->client->dev, "failed to flash firmware after retires\n");

	return ret;
}

static int mms_ts_suspend(struct device *dev);
static int mms_ts_resume(struct device *dev);

static ssize_t show_enabled(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", info->enabled);
}

static ssize_t store_enabled(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	struct mms_ts_info *info = dev_get_drvdata(dev);
	int enabled;
	int ret;

	ret = sscanf(buf, "%d", &enabled);
	if (ret == 0)
		return -EINVAL;

	if (enabled == info->enabled)
		return 0;

	if (enabled) {
		info->enabled_user = false;
		mms_ts_resume(&info->client->dev);
	} else {
		mms_ts_suspend(&info->client->dev);
		info->enabled_user = true;
	}

	info->enabled = enabled;

	return size;
}
static DEVICE_ATTR(enabled, S_IRUGO | S_IWUSR, show_enabled, store_enabled);

static struct attribute *mms_ts_attributes[] = {
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group mms_ts_attr_group = {
	.attrs = mms_ts_attributes,
};

#ifdef CONFIG_OF
static struct melfas_tsi_platform_data *mms_ts_parse_dt(struct device *dev)
{
	struct melfas_tsi_platform_data *pdata;
	struct device_node *np = dev->of_node;

	if (!np)
		return NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(dev, "failed to allocate platform data\n");
		return NULL;
	}

	if (of_property_read_u32(np, "max_x", &pdata->max_x)) {
		dev_err(dev, "failed to get max_x property\n");
		return NULL;
	};

	if (of_property_read_u32(np, "max_y", &pdata->max_y)) {
		dev_err(dev, "failed to get max_y property\n");
		return NULL;
	};

	if (of_property_read_u32(np, "invert_x", &pdata->invert_x)) {
		dev_err(dev, "failed to get invert_x property\n");
		return NULL;
	};
	pdata->invert_x = !!pdata->invert_x;

	if (of_property_read_u32(np, "invert_y", &pdata->invert_y)) {
		dev_err(dev, "failed to get invert_y property\n");
		return NULL;
	};
	pdata->invert_y = !!pdata->invert_y;

	if (of_property_read_string(np, "config_fw_version",
				&pdata->config_fw_version))
		dev_warn(dev, "cannot get touchscreen firmware version\n");

	pdata->register_cb = NULL;

	return pdata;
}
#else
static inline struct melfas_tsi_platform_data *mms_ts_parse_dt(struct device *dev)
{
	return NULL;
}
#endif


static int mms128_pm_notifier_callback(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct mms_ts_info *info = container_of(this, struct mms_ts_info,
						 pm_notifier);

	switch (event) {
	case PM_SUSPEND_PREPARE:
		info->suspended = true;
		break;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		info->suspended = false;
		break;
	}

	return NOTIFY_DONE;
}

static int mms_request_firmware(struct mms_ts_info *info)
{
	struct i2c_client *client = info->client;
	const struct firmware *fw, *fw_config;
	char fw_path[MAX_FW_PATH + 1];
	int ret = 0;

	/* firmware */
	snprintf(fw_path, MAX_FW_PATH, "%s%s", TSP_FW_DIRECTORY, TSP_FW_NAME);
	ret = request_firmware(&fw, fw_path, &client->dev);
	if (ret) {
		dev_err(&client->dev,
			"Failed to request firmware (%s)\n", fw_path);
		return -EINVAL;
	}

	info->fw_data = kzalloc(fw->size, GFP_KERNEL);
	if (!info->fw_data) {
		dev_err(&client->dev,
			"Faild to allocate firmware memory (%s)\n", fw_path);
		goto err_fw;
	}
	memcpy(info->fw_data, fw->data, fw->size);

	/* configuration firmware */
	snprintf(fw_path, MAX_FW_PATH, "%s%s", TSP_FW_DIRECTORY, TSP_FW_CONFIG_NAME);
	ret = request_firmware(&fw_config, fw_path, &client->dev);
	if (ret) {
		dev_err(&client->dev,
			"Failed to request firmware (%s)\n", fw_path);
		goto err_fw_alloc;
	}

	info->config_fw = kzalloc(fw_config->size, GFP_KERNEL);
	if (!info->config_fw) {
		dev_err(&client->dev,
			"Faild to allocate firmware memory (%s)\n", fw_path);
		goto err_fw_config;
	}
	memcpy(info->config_fw, fw_config->data, fw_config->size);

	release_firmware(fw_config);
	release_firmware(fw);

	return 0;
err_fw_config:
	release_firmware(fw_config);
err_fw_alloc:
	kfree(info->fw_data);
err_fw:
	release_firmware(fw);

	return ret;
}

static int mms_ts_input_open(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);

	dev_info(&info->client->dev, "%s\n", __func__);

	if (info->enabled) {
		dev_info(&info->client->dev,
			"%s already power on\n", __func__);
		return 0;
	}

	mms_config_get(info, REQ_FW);

	info->resume_done = false;
	mms_ts_power(info, true);
	schedule_delayed_work(&info->work_config_set,
					msecs_to_jiffies(10));

	return 0;
}

static void mms_ts_stop_device(struct mms_ts_info *info)
{
	disable_irq(info->irq);
	release_all_fingers(info);
	mms_ts_power(info, false);

	return;
}

static void mms_ts_input_close(struct input_dev *dev)
{
	struct mms_ts_info *info = input_get_drvdata(dev);
	int retries = 50;

	dev_info(&info->client->dev, "%s\n", __func__);

	if (!info->enabled){
		dev_info(&info->client->dev,
			"%s already power off\n", __func__);
		return;
	}

	while (!info->resume_done) {
		if (!retries--)
			break;
		msleep(100);
		dev_info(&info->client->dev, "%s:"
			" waiting for resume done.[%d]\n", __func__, retries);
	}

	mms_ts_stop_device(info);

	dev_info(&info->client->dev, "%s: close done.\n", __func__);
	return;
}

static int mms_ts_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mms_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	int i;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	info = kzalloc(sizeof(struct mms_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	if (client->dev.platform_data)
		info->pdata = dev_get_platdata(&client->dev);
	else if (client->dev.of_node)
		info->pdata = mms_ts_parse_dt(&client->dev);
	else
		info->pdata = NULL;

	if (of_machine_is_compatible("samsung,tizen-w")) {
		ret = mms_request_firmware(info);
		if (ret < 0) {
			dev_err(&client->dev, "Failed to request firmware\n");
			goto err_alloc;
		}
	} else if (of_machine_is_compatible("samsung,rinato-rev00")) {
		info->fw_data = mms_ts_w_firmware_rev00;
		info->config_fw = mms_ts_w_config_fw_rev00;
	} else if (of_machine_is_compatible("samsung,rinato-rev01")) {
		info->fw_data = mms_ts_w_firmware_rev01;
		info->config_fw = mms_ts_w_config_fw_rev01;
	} else if (of_machine_is_compatible("samsung,rinato-rev05")) {
		info->fw_data = mms_ts_w_firmware_rev01;
		info->config_fw = mms_ts_w_config_fw_rev01;
	}

	if (!info->pdata) {
		dev_err(&client->dev, "failed to get platform data\n");
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->irq = -1;
	mutex_init(&info->lock);

	if (info->pdata) {
		info->max_x = info->pdata->max_x;
		info->max_y = info->pdata->max_y;
		info->invert_x = info->pdata->invert_x;
		info->invert_y = info->pdata->invert_y;
		info->config_fw_version = info->pdata->config_fw_version;
		info->input_event = info->pdata->input_event;
		info->register_cb = info->pdata->register_cb;
	} else {
		info->max_x = 720;
		info->max_y = 1280;
	}
	for (i = 0 ; i < MAX_FINGERS; i++) {
		info->finger_state[i] = TSP_STATE_RELEASE;
		info->mcount[i] = 0;
	}

	info->client = client;
	info->input_dev = input_dev;

	info->regulator_pwr = regulator_get(&client->dev, "tsp_avdd_3.3v");
	if (IS_ERR(info->regulator_pwr)) {
		pr_err("melfas-ts : %s regulator_pwr error!", __func__);
		ret = -EINVAL;
		goto err_regulator;
	}

	info->regulator_vdd = regulator_get(&client->dev, "tsp_vdd_1.8v");
	if (IS_ERR(info->regulator_vdd)) {
		pr_err("melfas-ts : %s regulator_vdd error!", __func__);
		ret = -EINVAL;
		goto err_regulator;
	}

	mms_ts_power(info, true);
	msleep(25);

	i2c_set_clientdata(client, info);

	ret = mms_ts_fw_load(info);

	if (ret) {
		dev_err(&client->dev, "failed to initialize (%d)\n", ret);
		goto err_fw_init;
	}

	info->resume_done= true;

	snprintf(info->phys, sizeof(info->phys),
		 "%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchscreen"; /*= "Melfas MMSxxx Touchscreen";*/
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = mms_ts_input_open;
	input_dev->close = mms_ts_input_close;

	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, MAX_FINGERS, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
			     0, (info->max_x)-1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
				0, (info->max_y)-1, 0, 0);
	input_set_abs_params(input_dev, ABS_X, 0, info->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, info->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,
				0, MAX_WIDTH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
				0, MAX_PRESSURE, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR,
				0, MAX_PRESSURE, 0, 0);
	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		dev_err(&client->dev, "failed to register input dev (%d)\n",
			ret);
		goto err_reg_input_dev;
	}

	INIT_DELAYED_WORK(&info->work_config_set, work_mms_config_set);

	info->callbacks.inform_charger = melfas_ta_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);

	ret = request_threaded_irq(client->irq, NULL, mms_ts_interrupt,
				   IRQF_TRIGGER_FALLING  | IRQF_ONESHOT,
				   MELFAS_TS_NAME, info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to register interrupt\n");
		goto err_reg_input_dev;
	}

	info->irq = client->irq;
	barrier();

	/* Register pm_notifier */
	info->pm_notifier.notifier_call = mms128_pm_notifier_callback;
	ret = register_pm_notifier(&info->pm_notifier);
	if (ret) {
		dev_err(&client->dev, "Failed to setup pm notifier\n");
		goto err_reg_input_dev;
	}

	ret = sysfs_create_group(&input_dev->dev.kobj, &mms_ts_attr_group);
	if (ret) {
		dev_err(&client->dev, "Failed to create sysfs group\n");
		goto err_regulator;
	}
	disable_irq(info->irq);
	mms_ts_power(info, false);

	dev_info(&client->dev,
			"Melfas MMS-series touch controller initialized\n");

	return 0;

err_regulator:
	regulator_put(info->regulator_pwr);
	regulator_put(info->regulator_vdd);
err_reg_input_dev:
	input_unregister_device(input_dev);
err_fw_init:
	mms_ts_power(info, false);
err_input_alloc:
err_alloc:
	kfree(info);
	return ret;

}

static int mms_ts_remove(struct i2c_client *client)
{
	struct mms_ts_info *info = i2c_get_clientdata(client);

	if (of_machine_is_compatible("samsung,tizen-w")) {
		if (info->fw_data)
			kfree(info->fw_data);
		if (info->config_fw)
			kfree(info->config_fw);
	}

	if (info->irq >= 0)
		free_irq(info->irq, info);
	input_unregister_device(info->input_dev);

	regulator_put(info->regulator_pwr);
	regulator_put(info->regulator_vdd);

	kfree(info);

	return 0;
}

static int mms_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);
	int retries = 50;

	if (info->enabled_user)
		return 0;

	dev_info(&info->client->dev, "%s %s\n",
			__func__, info->enabled ? "ON" : "OFF");

	cancel_delayed_work_sync(&info->work_config_set);

	while (!info->resume_done) {
		if (!retries--)
			break;
		msleep(10);
	}

	if (!info->enabled)
		return 0;

	disable_irq(info->irq);
	release_all_fingers(info);

	mms_ts_power(info, 0);

	return 0;
}

static int mms_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mms_ts_info *info = i2c_get_clientdata(client);

	if (info->enabled_user)
		return 0;

	dev_info(&info->client->dev, "%s %s\n",
			__func__, info->enabled ? "ON" : "OFF");

	if (info->enabled)
		return 0;

	mms_ts_power(info, 1);
	info->resume_done = false;
	schedule_delayed_work(&info->work_config_set,
				msecs_to_jiffies(80));

	return 0;
}

static SIMPLE_DEV_PM_OPS(mms_ts_pm_ops, mms_ts_suspend, mms_ts_resume);

#ifdef CONFIG_OF
static struct of_device_id mms128_dt_match[] = {
	{ .compatible = "melfas,mms128" },
	{},
};
#endif

static const struct i2c_device_id mms_ts_id[] = {
	{MELFAS_TS_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, mms_ts_id);

static struct i2c_driver mms_ts_driver = {
	.probe = mms_ts_probe,
	.remove = mms_ts_remove,
	.driver = {
		   .name = MELFAS_TS_NAME,
		   .pm = &mms_ts_pm_ops,
		   .of_match_table = of_match_ptr(mms128_dt_match),
		   },
	.id_table = mms_ts_id,
};

module_i2c_driver(mms_ts_driver);

/* Module information */
MODULE_DESCRIPTION("Touchscreen driver for Melfas MMS-series controllers");
MODULE_LICENSE("GPL");
