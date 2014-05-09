/*
 * Copyright (C) 2014 Samsung Electronics
 *
 * Author: Tomasz Stanislawski <t.stanislaws@samsung.com>
 *
 * Based on sii9234 driver created by:
 *    Adam Hampson <ahampson@sta.samsung.com>
 *    Erik Gilling <konkers@android.com>
 *    Shankar Bandal <shankar.b@samsung.com>
 *    Dharam Kumar <dharam.kr@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/extcon.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/machine.h>

/* MHL Tx registers and bits */
#define MHL_TX_SRST                     0x05
#define MHL_TX_SYSSTAT_REG              0x09
#define         RSEN_STATUS             (1<<2)
#define MHL_TX_INTR1_REG                0x71
#define         HPD_CHANGE_INT          (1<<6)
#define         RSEN_CHANGE_INT         (1<<5)
#define MHL_TX_INTR4_REG                0x74
#define         RGND_READY_INT          (1<<6)
#define         VBUS_LOW_INT            (1<<5)
#define         CBUS_LKOUT_INT          (1<<4)
#define         MHL_DISC_FAIL_INT       (1<<3)
#define         MHL_EST_INT             (1<<2)
#define MHL_TX_INTR1_ENABLE_REG         0x75
#define         HPD_CHANGE_INT_MASK     (1<<6)
#define         RSEN_CHANGE_INT_MASK    (1<<5)
#define MHL_TX_INTR4_ENABLE_REG         0x78
#define         RGND_READY_MASK         (1<<6)
#define         CBUS_LKOUT_MASK         (1<<4)
#define         MHL_DISC_FAIL_MASK      (1<<3)
#define         MHL_EST_MASK            (1<<2)
#define MHL_TX_INT_CTRL_REG             0x79
#define MHL_TX_TMDS_CCTRL               0x80
#define MHL_TX_DISC_CTRL1_REG           0x90
#define MHL_TX_DISC_CTRL2_REG           0x91
#define         SKIP_GND                (1<<6)
#define         ATT_THRESH_SHIFT        0x04
#define         ATT_THRESH_MASK         (0x03 << ATT_THRESH_SHIFT)
#define         USB_D_OEN               (1<<3)
#define         DEGLITCH_TIME_MASK      0x07
#define         DEGLITCH_TIME_2MS       0
#define         DEGLITCH_TIME_4MS       1
#define         DEGLITCH_TIME_8MS       2
#define         DEGLITCH_TIME_16MS      3
#define         DEGLITCH_TIME_40MS      4
#define         DEGLITCH_TIME_50MS      5
#define         DEGLITCH_TIME_60MS      6
#define         DEGLITCH_TIME_128MS     7
#define MHL_TX_DISC_CTRL3_REG           0x92
#define MHL_TX_DISC_CTRL4_REG		0x93
#define MHL_TX_DISC_CTRL5_REG           0x94
#define MHL_TX_DISC_CTRL6_REG           0x95
#define         USB_D_OVR               (1<<7)
#define         USB_ID_OVR              (1<<6)
#define         DVRFLT_SEL              (1<<5)
#define         BLOCK_RGND_INT          (1<<4)
#define         SKIP_DEG                (1<<3)
#define         CI2CA_POL               (1<<2)
#define         CI2CA_WKUP              (1<<1)
#define         SINGLE_ATT              (1<<0)
#define MHL_TX_DISC_CTRL7_REG           0x96
#define         USB_D_ODN               (1<<5)
#define         VBUS_CHECK              (1<<2)
#define         RGND_INTP_MASK          0x03
#define         RGND_INTP_OPEN          0
#define         RGND_INTP_2K            1
#define         RGND_INTP_1K            2
#define         RGND_INTP_SHORT         3
#define MHL_TX_DISC_CTRL8_REG		0x97
#define MHL_TX_STAT2_REG                0x99
#define MHL_TX_MHLTX_CTL1_REG           0xA0
#define MHL_TX_MHLTX_CTL2_REG           0xA1
#define MHL_TX_MHLTX_CTL4_REG           0xA3
#define MHL_TX_MHLTX_CTL6_REG           0xA5
#define MHL_TX_MHLTX_CTL7_REG           0xA6

/* HDMI registers */
#define HDMI_RX_TMDS0_CCTRL1_REG        0x10
#define HDMI_RX_TMDS_CLK_EN_REG         0x11
#define HDMI_RX_TMDS_CH_EN_REG          0x12
#define HDMI_RX_PLL_CALREFSEL_REG       0x17
#define HDMI_RX_PLL_VCOCAL_REG          0x1A
#define HDMI_RX_EQ_DATA0_REG            0x22
#define HDMI_RX_EQ_DATA1_REG            0x23
#define HDMI_RX_EQ_DATA2_REG            0x24
#define HDMI_RX_EQ_DATA3_REG            0x25
#define HDMI_RX_EQ_DATA4_REG            0x26
#define HDMI_RX_TMDS_ZONE_CTRL_REG      0x4C
#define HDMI_RX_TMDS_MODE_CTRL_REG      0x4D

/* CBUS registers */
#define CBUS_INT_STATUS_1_REG           0x08
#define CBUS_INTR1_ENABLE_REG		0x09
#define CBUS_MSC_REQ_ABORT_REASON_REG   0x0D
#define         SET_HPD_DOWNSTREAM      (1<<6)
#define CBUS_INT_STATUS_2_REG           0x1E
#define CBUS_INTR2_ENABLE_REG		0x1F
#define CBUS_LINK_CONTROL_2_REG         0x31
#define CBUS_DEVCAP_DEV_STATE		0x80
#define CBUS_DEVCAP_MHL_VERSION		0x81
#define CBUS_DEVCAP_DEV_CAT             0x82
#define CBUS_DEVCAP_ADOPTER_ID_H	0x83
#define CBUS_DEVCAP_ADOPTER_ID_L	0x84
#define CBUS_DEVCAP_VID_LINK_MODE	0x85
#define CBUS_DEVCAP_AUD_LINK_MODE	0x86
#define CBUS_DEVCAP_VIDEO_TYPE		0x87
#define CBUS_DEVCAP_LOG_DEV_MAP		0x88
#define CBUS_DEVCAP_BANDWIDTH		0x89
#define CBUS_DEVCAP_DEV_FEATURE_FLAG    0x8A
#define CBUS_DEVCAP_DEVICE_ID_H         0x8B
#define CBUS_DEVCAP_DEVICE_ID_L         0x8C
#define CBUS_DEVCAP_SCRATCHPAD_SIZE	0x8D
#define CBUS_DEVCAP_INT_STAT_SIZE	0x8E
#define CBUS_DEVCAP_RESERVED		0x8F
#define CBUS_MHL_STATUS_REG_0           0xB0
#define CBUS_MHL_STATUS_REG_1           0xB1

/* TPI registers */
#define TPI_DPD_REG                     0x3D

/* timeouts */
#define T_SRC_VBUS_CBUS_TO_STABLE	200
#define T_SRC_CBUS_FLOAT		100
#define T_SRC_CBUS_DEGLITCH		2
#define T_SRC_RXSENSE_DEGLITCH		110

enum sii9234_state {
	ST_OFF,
	ST_D3,
	ST_RGND_INIT,
	ST_RGND_1K,
	ST_RSEN_HIGH,
	ST_MHL_ESTABLISHED,
	ST_FAILURE_DISCOVERY,
	ST_FAILURE,
};

struct sii9234 {
	struct i2c_client *client[4];
	int i2c_error;
	int gpio_n_reset;
	int gpio_int;
	struct regulator *vcc_supply;
	int irq;

	enum sii9234_state		state;
	struct mutex			lock;

	/* Extcon */
	struct extcon_specific_cable_nb extcon_dev;
	struct notifier_block extcon_nb;
	struct work_struct extcon_wq;
	bool extcon_attached;
};

enum sii9234_client_id {
	I2C_MHL,
	I2C_TPI,
	I2C_HDMI,
	I2C_CBUS,
};

#define sii9234_dev(sii9234) (&(sii9234)->client[I2C_MHL]->dev)

static char *sii9234_client_name[] = {
	[I2C_MHL] = "MHL",
	[I2C_TPI] = "TPI",
	[I2C_HDMI] = "HDMI",
	[I2C_CBUS] = "CBUS",
};

static int sii9234_writeb(struct sii9234 *sii9234, int id, int offset,
			    int value)
{
	int ret;
	struct i2c_client *client = sii9234->client[id];
	struct device *dev = sii9234_dev(sii9234);

	if (sii9234->i2c_error)
		return sii9234->i2c_error;

	ret = i2c_smbus_write_byte_data(client, offset, value);
	if (ret < 0)
		dev_err(dev, "writeb: %4s[0x%02x] <- 0x%02x\n",
			sii9234_client_name[id], offset, value);
	sii9234->i2c_error = ret;
	return ret;
}

static int sii9234_writebm(struct sii9234 *sii9234, int id, int offset,
			    int value, int mask)
{
	int ret;
	struct i2c_client *client = sii9234->client[id];
	struct device *dev = sii9234_dev(sii9234);

	if (sii9234->i2c_error)
		return sii9234->i2c_error;

	ret = i2c_smbus_write_byte(client, offset);
	if (ret < 0) {
		dev_err(dev, "writebm: %4s[0x%02x] <- 0x%02x\n",
			sii9234_client_name[id], offset, value);
		sii9234->i2c_error = ret;
		return ret;
	}

	ret = i2c_smbus_read_byte(client);
	if (ret < 0) {
		dev_err(dev, "writebm: %4s[0x%02x] <- 0x%02x\n",
			sii9234_client_name[id], offset, value);
		sii9234->i2c_error = ret;
		return ret;
	}

	value = (value & mask) | (ret & ~mask);

	ret = i2c_smbus_write_byte_data(client, offset, value);
	if (ret < 0) {
		dev_err(dev, "writebm: %4s[0x%02x] <- 0x%02x\n",
			sii9234_client_name[id], offset, value);
		sii9234->i2c_error = ret;
	}
	return ret;
}

static int sii9234_readb(struct sii9234 *sii9234, int id, int offset)
{
	int ret;
	struct i2c_client *client = sii9234->client[id];
	struct device *dev = sii9234_dev(sii9234);

	if (sii9234->i2c_error)
		return sii9234->i2c_error;

	ret = i2c_smbus_write_byte(client, offset);
	if (ret < 0) {
		dev_err(dev, "readb: %4s[0x%02x]\n",
			sii9234_client_name[id], offset);
		sii9234->i2c_error = ret;
		return ret;
	}

	ret = i2c_smbus_read_byte(client);
	if (ret < 0) {
		dev_err(dev, "readb: %4s[0x%02x]\n",
			sii9234_client_name[id], offset);
		sii9234->i2c_error = ret;
	}

	return ret;
}

static int sii9234_clear_error(struct sii9234 *sii9234)
{
	int ret = sii9234->i2c_error;

	sii9234->i2c_error = 0;
	return ret;
}

#define mhl_tx_writeb(sii9234, offset, value) \
	sii9234_writeb(sii9234, I2C_MHL, offset, value)
#define mhl_tx_writebm(sii9234, offset, value, mask) \
	sii9234_writebm(sii9234, I2C_MHL, offset, value, mask)
#define mhl_tx_readb(sii9234, offset) \
	sii9234_readb(sii9234, I2C_MHL, offset)
#define cbus_writeb(sii9234, offset, value) \
	sii9234_writeb(sii9234, I2C_CBUS, offset, value)
#define cbus_writebm(sii9234, offset, value, mask) \
	sii9234_writebm(sii9234, I2C_CBUS, offset, value, mask)
#define cbus_readb(sii9234, offset) \
	sii9234_readb(sii9234, I2C_CBUS, offset)
#define hdmi_writeb(sii9234, offset, value) \
	sii9234_writeb(sii9234, I2C_HDMI, offset, value)
#define hdmi_writebm(sii9234, offset, value, mask) \
	sii9234_writebm(sii9234, I2C_HDMI, offset, value, mask)
#define hdmi_readb(sii9234, offset) \
	sii9234_readb(sii9234, I2C_HDMI, offset)
#define tpi_writeb(sii9234, offset, value) \
	sii9234_writeb(sii9234, I2C_TPI, offset, value)
#define tpi_writebm(sii9234, offset, value, mask) \
	sii9234_writebm(sii9234, I2C_TPI, offset, value, mask)
#define tpi_readb(sii9234, offset) \
	sii9234_readb(sii9234, I2C_TPI, offset)

static u8 sii9234_tmds_control(struct sii9234 *sii9234, bool enable)
{
	mhl_tx_writebm(sii9234, MHL_TX_TMDS_CCTRL, enable ? ~0 : 0, 0x10);
	mhl_tx_writebm(sii9234, MHL_TX_INT_CTRL_REG, enable ? ~0 : 0, 0x30);
	return sii9234_clear_error(sii9234);
}

static int sii9234_cbus_reset(struct sii9234 *sii9234)
{
	int i;

	/* Reset CBUS */
	mhl_tx_writebm(sii9234, MHL_TX_SRST, ~0, 1 << 3);
	/* CBUS deglitch - 2ms */
	msleep(T_SRC_CBUS_DEGLITCH);
	mhl_tx_writebm(sii9234, MHL_TX_SRST, 0, 1 << 3);

	for (i = 0; i < 4; i++) {
		/* Enable WRITE_STAT interrupt for writes to all
		   4 MSC Status registers. */
		cbus_writeb(sii9234, 0xE0 + i, 0xF2);
		/*Enable SET_INT interrupt for writes to all
		   4 MSC Interrupt registers. */
		cbus_writeb(sii9234, 0xF0 + i, 0xF2);
	}

	return sii9234_clear_error(sii9234);
}

/* require to chek mhl imformation of samsung in cbus_init_register*/
static int sii9234_cbus_init(struct sii9234 *sii9234)
{
	cbus_writeb(sii9234, 0x07, 0xF2);
	cbus_writeb(sii9234, 0x40, 0x03);
	cbus_writeb(sii9234, 0x42, 0x06);
	cbus_writeb(sii9234, 0x36, 0x0C);
	cbus_writeb(sii9234, 0x3D, 0xFD);
	cbus_writeb(sii9234, 0x1C, 0x01);
	cbus_writeb(sii9234, 0x1D, 0x0F);
	cbus_writeb(sii9234, 0x44, 0x02);
	/* Setup our devcap */
	/* To meet cts 6.3.10.1 spec */
	cbus_writeb(sii9234, CBUS_DEVCAP_DEV_STATE, 0x00);
	/* mhl version 1.1 */
	cbus_writeb(sii9234, CBUS_DEVCAP_MHL_VERSION, 0x11);
	cbus_writeb(sii9234, CBUS_DEVCAP_DEV_CAT, 0x02);
	cbus_writeb(sii9234, CBUS_DEVCAP_ADOPTER_ID_H, 0x01);
	cbus_writeb(sii9234, CBUS_DEVCAP_ADOPTER_ID_L, 0x41);
	/* YCbCr444, RGB444 */
	cbus_writeb(sii9234, CBUS_DEVCAP_VID_LINK_MODE, 0x03);
	cbus_writeb(sii9234, CBUS_DEVCAP_VIDEO_TYPE, 0);
	cbus_writeb(sii9234, CBUS_DEVCAP_LOG_DEV_MAP, 0x80);
	cbus_writeb(sii9234, CBUS_DEVCAP_BANDWIDTH, 0x0F);
	cbus_writeb(sii9234, CBUS_DEVCAP_DEV_FEATURE_FLAG, 0x07);
	cbus_writeb(sii9234, CBUS_DEVCAP_DEVICE_ID_H, 0x0);
	cbus_writeb(sii9234, CBUS_DEVCAP_DEVICE_ID_L, 0x0);
	cbus_writeb(sii9234, CBUS_DEVCAP_SCRATCHPAD_SIZE, 0x10);
	cbus_writeb(sii9234, CBUS_DEVCAP_INT_STAT_SIZE, 0x33);
	cbus_writeb(sii9234, CBUS_DEVCAP_RESERVED, 0);
	cbus_writebm(sii9234, 0x31, 0x0C, 0x0C);
	cbus_writeb(sii9234, 0x30, 0x01);
	cbus_writebm(sii9234, 0x3C, 0x30, 0x38);
	cbus_writebm(sii9234, 0x22, 0x0D, 0x0F);
	cbus_writebm(sii9234, 0x2E, 0x15, 0x15);
	cbus_writeb(sii9234, CBUS_INTR1_ENABLE_REG, 0);
	cbus_writeb(sii9234, CBUS_INTR2_ENABLE_REG, 0);

	return sii9234_clear_error(sii9234);
}

static void force_usb_id_switch_open(struct sii9234 *sii9234)
{
	/*Disable CBUS discovery */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL1_REG, 0, 0x01);
	/*Force USB ID switch to open */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL6_REG, ~0, USB_ID_OVR);
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL3_REG, ~0, 0x86);
	/*Force upstream HPD to 0 when not in MHL mode. */
	mhl_tx_writebm(sii9234, MHL_TX_INT_CTRL_REG, 0, 0x30);
}

static void release_usb_id_switch_open(struct sii9234 *sii9234)
{
	msleep(T_SRC_CBUS_FLOAT);
	/* clear USB ID switch to open */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL6_REG, 0, USB_ID_OVR);
	/* Enable CBUS discovery */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL1_REG, ~0, 0x01);
}

static int sii9234_power_init(struct sii9234 *sii9234)
{
	/* Force the SiI9234 into the D0 state. */
	tpi_writeb(sii9234, TPI_DPD_REG, 0x3F);
	/* Enable TxPLL Clock */
	hdmi_writeb(sii9234, HDMI_RX_TMDS_CLK_EN_REG, 0x01);
	/* Enable Tx Clock Path & Equalizer */
	hdmi_writeb(sii9234, HDMI_RX_TMDS_CH_EN_REG, 0x15);
	/* Power Up TMDS */
	mhl_tx_writeb(sii9234, 0x08, 0x35);
	return sii9234_clear_error(sii9234);
}

static int sii9234_hdmi_init(struct sii9234 *sii9234)
{
	/* Analog PLL Control bits 5:4 = 2b00 as per char. team. */
	hdmi_writeb(sii9234, HDMI_RX_TMDS0_CCTRL1_REG, 0xC1);
	/* PLL Calrefsel */
	hdmi_writeb(sii9234, HDMI_RX_PLL_CALREFSEL_REG, 0x03);
	/* VCO Cal */
	hdmi_writeb(sii9234, HDMI_RX_PLL_VCOCAL_REG, 0x20);
	/* Auto EQ */
	hdmi_writeb(sii9234, HDMI_RX_EQ_DATA0_REG, 0x8A);
	/* Auto EQ */
	hdmi_writeb(sii9234, HDMI_RX_EQ_DATA1_REG, 0x6A);
	/* Auto EQ */
	hdmi_writeb(sii9234, HDMI_RX_EQ_DATA2_REG, 0xAA);
	/* Auto EQ */
	hdmi_writeb(sii9234, HDMI_RX_EQ_DATA3_REG, 0xCA);
	/* Auto EQ */
	hdmi_writeb(sii9234, HDMI_RX_EQ_DATA4_REG, 0xEA);
	/* Manual zone */
	hdmi_writeb(sii9234, HDMI_RX_TMDS_ZONE_CTRL_REG, 0xA0);
	/* PLL Mode Value */
	hdmi_writeb(sii9234, HDMI_RX_TMDS_MODE_CTRL_REG, 0x00);
	mhl_tx_writeb(sii9234, MHL_TX_TMDS_CCTRL, 0x34);
	hdmi_writeb(sii9234, 0x45, 0x44);
	/* Rx PLL BW ~ 4MHz */
	hdmi_writeb(sii9234, 0x31, 0x0A);
	/* Analog PLL Control * bits 5:4 = 2b00 as per char. team. */
	hdmi_writeb(sii9234, HDMI_RX_TMDS0_CCTRL1_REG, 0xC1);

	return sii9234_clear_error(sii9234);
}

static int sii9234_mhl_tx_ctl_int(struct sii9234 *sii9234)
{
	mhl_tx_writeb(sii9234, MHL_TX_MHLTX_CTL1_REG, 0xD0);
	mhl_tx_writeb(sii9234, MHL_TX_MHLTX_CTL2_REG, 0xFC);
	mhl_tx_writeb(sii9234, MHL_TX_MHLTX_CTL4_REG, 0xEB);
	mhl_tx_writeb(sii9234, MHL_TX_MHLTX_CTL7_REG, 0x0C);
	return sii9234_clear_error(sii9234);
}

static int sii9234_reset(struct sii9234 *sii9234)
{
	int ret;

	sii9234_clear_error(sii9234);

	ret = sii9234_power_init(sii9234);
	if (ret < 0)
		return ret;
	ret = sii9234_cbus_reset(sii9234);
	if (ret < 0)
		return ret;
	ret = sii9234_hdmi_init(sii9234);
	if (ret < 0)
		return ret;
	ret = sii9234_mhl_tx_ctl_int(sii9234);
	if (ret < 0)
		return ret;

	/* Enable HDCP Compliance safety */
	mhl_tx_writeb(sii9234, 0x2B, 0x01);
	/* CBUS discovery cycle time for each drive and float = 150us */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL1_REG, 0x04, 0x06);
	/* Clear bit 6 (reg_skip_rgnd) */
	mhl_tx_writeb(sii9234, MHL_TX_DISC_CTRL2_REG, (1 << 7) /* Reserved */
		| 2 << ATT_THRESH_SHIFT | DEGLITCH_TIME_50MS);
	/* Changed from 66 to 65 for 94[1:0] = 01 = 5k reg_cbusmhl_pup_sel */
	/* 1.8V CBUS VTH & GND threshold */
	/*To meet CTS 3.3.7.2 spec */
	mhl_tx_writeb(sii9234, MHL_TX_DISC_CTRL5_REG, 0x77);
	/* set bit 2 and 3, which is Initiator Timeout */
	cbus_writebm(sii9234, CBUS_LINK_CONTROL_2_REG, ~0, 0x0C);
	mhl_tx_writeb(sii9234, MHL_TX_MHLTX_CTL6_REG, 0xA0);
	/* RGND & single discovery attempt (RGND blocking) */
	mhl_tx_writeb(sii9234, MHL_TX_DISC_CTRL6_REG, BLOCK_RGND_INT |
			       DVRFLT_SEL | SINGLE_ATT);
	/* Use VBUS path of discovery state machine */
	mhl_tx_writeb(sii9234, MHL_TX_DISC_CTRL8_REG, 0);
	/* 0x92[3] sets the CBUS / ID switch */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL6_REG, ~0, USB_ID_OVR);
	/* To allow RGND engine to operate correctly.
	 * When moving the chip from D2 to D0 (power up, init regs)
	 * the values should be
	 * 94[1:0] = 01  reg_cbusmhl_pup_sel[1:0] should be set for 5k
	 * 93[7:6] = 10  reg_cbusdisc_pup_sel[1:0] should be
	 * set for 10k (default)
	 * 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
	 */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL3_REG, ~0, 0x86);
	/* change from CC to 8C to match 5K */
	/*To meet CTS 3.3.72 spec */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL4_REG, ~0, 0x8C);
	/* Configure the interrupt as active high */
	mhl_tx_writebm(sii9234, MHL_TX_INT_CTRL_REG, 0, 0x06);

	msleep(25);

	/* release usb_id switch */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL6_REG, 0,  USB_ID_OVR);
	mhl_tx_writeb(sii9234, MHL_TX_DISC_CTRL1_REG, 0x27);

	ret = sii9234_clear_error(sii9234);
	if (ret < 0)
		return ret;
	ret = sii9234_cbus_init(sii9234);
	if (ret < 0)
		return ret;

	/* Enable Auto soft reset on SCDT = 0 */
	mhl_tx_writeb(sii9234, 0x05, 0x04);
	/* HDMI Transcode mode enable */
	mhl_tx_writeb(sii9234, 0x0D, 0x1C);
	mhl_tx_writeb(sii9234, MHL_TX_INTR4_ENABLE_REG,
			       RGND_READY_MASK | CBUS_LKOUT_MASK |
			       MHL_DISC_FAIL_MASK | MHL_EST_MASK);
	mhl_tx_writeb(sii9234, MHL_TX_INTR1_ENABLE_REG, 0x60);

	/* this point is very importand before megsure RGND impedance */
	force_usb_id_switch_open(sii9234);
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL4_REG, 0, 0xF0);
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL5_REG, 0, 0x03);
	release_usb_id_switch_open(sii9234);
	/*end of this */

	/* Force upstream HPD to 0 when not in MHL mode */
	mhl_tx_writebm(sii9234, MHL_TX_INT_CTRL_REG, 0, 1 << 5);
	mhl_tx_writebm(sii9234, MHL_TX_INT_CTRL_REG, ~0, 1 << 4);

	return sii9234_clear_error(sii9234);
}

static int sii9234_goto_d3(struct sii9234 *sii9234)
{
	int ret;
	struct device *dev = sii9234_dev(sii9234);

	dev_dbg(dev, "sii9234: detection started d3\n");

	ret = sii9234_reset(sii9234);
	if (ret < 0)
		goto exit;

	hdmi_writeb(sii9234, 0x01, 0x03);
	tpi_writebm(sii9234, TPI_DPD_REG, 0, 1);
	/* I2C above is expected to fail because power goes down */
	sii9234_clear_error(sii9234);

	sii9234->state = ST_D3;

	return 0;
 exit:
	dev_err(dev, "sii9234_goto_d3() failed\n");
	return -1;
}

#define sii9234_hw_on(sii9234) \
	regulator_enable((sii9234)->vcc_supply)

static void sii9234_hw_off(struct sii9234 *sii9234)
{
	gpio_direction_output(sii9234->gpio_n_reset, 0);
	usleep_range(10000, 20000);
	gpio_direction_output(sii9234->gpio_n_reset, 1);
	regulator_disable(sii9234->vcc_supply);
	gpio_direction_output(sii9234->gpio_n_reset, 0);
}

static void sii9234_hw_reset(struct sii9234 *sii9234)
{
	gpio_direction_output(sii9234->gpio_n_reset, 0);
	usleep_range(10000, 20000);
	gpio_direction_output(sii9234->gpio_n_reset, 1);
}

static void sii9234_cable_in(struct sii9234 *sii9234)
{
	int ret;

	mutex_lock(&sii9234->lock);
	if (sii9234->state != ST_OFF)
		goto unlock;
	ret = sii9234_hw_on(sii9234);
	if (ret < 0)
		goto unlock;

	sii9234_hw_reset(sii9234);
	sii9234_goto_d3(sii9234);
	enable_irq(sii9234->irq);

unlock:
	mutex_unlock(&sii9234->lock);
}

static void sii9234_cable_out(struct sii9234 *sii9234)
{
	mutex_lock(&sii9234->lock);

	if (sii9234->state == ST_OFF)
		goto unlock;

	disable_irq_nosync(sii9234->irq);
	tpi_writeb(sii9234, TPI_DPD_REG, 0);
	/*turn on&off hpd festure for only QCT HDMI */
	sii9234_hw_off(sii9234);

	sii9234->state = ST_OFF;

unlock:
	mutex_unlock(&sii9234->lock);
}

static enum sii9234_state sii9234_rgnd_ready_irq(struct sii9234 *sii9234)
{
	int value;
	struct device *dev = sii9234_dev(sii9234);

	if (sii9234->state == ST_D3) {
		int ret;

		dev_dbg(dev, "RGND_READY_INT\n");
		sii9234_hw_reset(sii9234);

		ret = sii9234_reset(sii9234);
		if (ret < 0) {
			dev_err(dev, "sii9234_reset() failed\n");
			return ST_FAILURE;
		}

		return ST_RGND_INIT;
	}

	/* got interrupt in inappropriate state */
	if (sii9234->state != ST_RGND_INIT)
		return ST_FAILURE;

	value = mhl_tx_readb(sii9234, MHL_TX_STAT2_REG);
	if (sii9234_clear_error(sii9234))
		return ST_FAILURE;

	if ((value & RGND_INTP_MASK) != RGND_INTP_1K) {
		dev_warn(dev, "RGND is not 1k\n");
		return ST_RGND_INIT;
	}
	dev_dbg(dev, "RGND 1K!!\n");
	/* After applying RGND patch, there is some issue
	   about discovry failure
	   This point is add to fix that problem */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL4_REG, ~0, 0x8C);
	mhl_tx_writeb(sii9234, MHL_TX_DISC_CTRL5_REG, 0x77);
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL6_REG, ~0, 0x05);
	if (sii9234_clear_error(sii9234))
		return ST_FAILURE;

	usleep_range(T_SRC_VBUS_CBUS_TO_STABLE * USEC_PER_MSEC,
		     T_SRC_VBUS_CBUS_TO_STABLE * USEC_PER_MSEC);

	return ST_RGND_1K;
}

static enum sii9234_state sii9234_mhl_established(struct sii9234 *sii9234)
{
	dev_dbg(sii9234_dev(sii9234), "mhl est interrupt\n");

	/* discovery override */
	mhl_tx_writeb(sii9234, MHL_TX_MHLTX_CTL1_REG, 0x10);
	/* increase DDC translation layer timer (byte mode) */
	cbus_writeb(sii9234, 0x07, 0x32);
	cbus_writebm(sii9234, 0x44, ~0, 1 << 1);
	/* Keep the discovery enabled. Need RGND interrupt */
	mhl_tx_writebm(sii9234, MHL_TX_DISC_CTRL1_REG, ~0, 1);
	mhl_tx_writeb(sii9234, MHL_TX_INTR1_ENABLE_REG,
	       RSEN_CHANGE_INT_MASK | HPD_CHANGE_INT_MASK);

	if (sii9234_clear_error(sii9234))
		return ST_FAILURE;

	return ST_MHL_ESTABLISHED;
}

static enum sii9234_state sii9234_hpd_change(struct sii9234 *sii9234)
{
	int value;

	value = cbus_readb(sii9234, CBUS_MSC_REQ_ABORT_REASON_REG);
	if (sii9234_clear_error(sii9234))
		return ST_FAILURE;

	if (value & SET_HPD_DOWNSTREAM) {
		dev_info(sii9234_dev(sii9234), "HPD High\n");
		/* Downstream HPD High, Enable TMDS */
		sii9234_tmds_control(sii9234, true);
		/*turn on&off hpd festure for only QCT HDMI */
	} else {
		dev_info(sii9234_dev(sii9234), "HPD Low\n");
		/* Downstream HPD Low, Disable TMDS */
		sii9234_tmds_control(sii9234, false);
	}

	return sii9234->state;
}

static enum sii9234_state sii9234_rsen_change(struct sii9234 *sii9234)
{
	int value;
	struct device *dev = sii9234_dev(sii9234);

	/* work_around code to handle worng interrupt */
	if (sii9234->state != ST_RGND_1K) {
		dev_err(dev, "RSEN_HIGH without RGND_1K\n");
		return ST_FAILURE;
	}
	value = mhl_tx_readb(sii9234, MHL_TX_SYSSTAT_REG);
	if (value < 0)
		return ST_FAILURE;

	if (value & RSEN_STATUS) {
		dev_info(dev, "MHL cable connected.. RSEN High\n");
		return ST_RSEN_HIGH;
	}
	dev_info(dev, "RSEN lost\n");
	/* Once RSEN loss is confirmed,we need to check
	 * based on cable status and chip power status,whether
	 * it is SINK Loss(HDMI cable not connected, TV Off)
	 * or MHL cable disconnection
	 * TODO: Define the below mhl_disconnection()
	 */
	msleep(T_SRC_RXSENSE_DEGLITCH);
	value = mhl_tx_readb(sii9234, MHL_TX_SYSSTAT_REG);
	if (value < 0)
		return ST_FAILURE;
	dev_dbg(dev, "sys_stat: %x\n", value);

	if (value & RSEN_STATUS) {
		dev_info(dev, "RSEN recovery\n");
		return ST_RSEN_HIGH;
	}
	dev_info(dev, "RSEN Really LOW\n");
	/*To meet CTS 3.3.22.2 spec */
	sii9234_tmds_control(sii9234, false);
	force_usb_id_switch_open(sii9234);
	release_usb_id_switch_open(sii9234);

	return ST_FAILURE;
}

static irqreturn_t sii9234_irq_thread(int irq, void *data)
{
	struct sii9234 *sii9234 = data;
	int intr1, intr4;
	int intr1_en, intr4_en;
	int cbus_intr1, cbus_intr2;
	struct device *dev = sii9234_dev(sii9234);

	dev_dbg(dev, "%s\n", __func__);

	msleep(30);

	mutex_lock(&sii9234->lock);

	intr1 = mhl_tx_readb(sii9234, MHL_TX_INTR1_REG);
	intr4 = mhl_tx_readb(sii9234, MHL_TX_INTR4_REG);
	intr1_en = mhl_tx_readb(sii9234, MHL_TX_INTR1_ENABLE_REG);
	intr4_en = mhl_tx_readb(sii9234, MHL_TX_INTR4_ENABLE_REG);
	cbus_intr1 = cbus_readb(sii9234, CBUS_INT_STATUS_1_REG);
	cbus_intr2 = cbus_readb(sii9234, CBUS_INT_STATUS_2_REG);

	if (sii9234_clear_error(sii9234))
		goto done;

	dev_dbg(dev, "irq %02x/%02x %02x/%02x %02x/%02x\n",
		 intr1, intr1_en, intr4, intr4_en, cbus_intr1, cbus_intr2);

	if (intr4 & RGND_READY_INT)
		sii9234->state = sii9234_rgnd_ready_irq(sii9234);
	if (intr1 & RSEN_CHANGE_INT)
		sii9234->state = sii9234_rsen_change(sii9234);
	if (intr4 & MHL_EST_INT)
		sii9234->state = sii9234_mhl_established(sii9234);
	if (intr1 & HPD_CHANGE_INT)
		sii9234->state = sii9234_hpd_change(sii9234);
	if (intr4 & CBUS_LKOUT_INT)
		sii9234->state = ST_FAILURE;
	if (intr4 & MHL_DISC_FAIL_INT)
		sii9234->state = ST_FAILURE_DISCOVERY;

 done:
	/* clean interrupt status and pending flags */
	mhl_tx_writeb(sii9234, MHL_TX_INTR1_REG, intr1);
	mhl_tx_writeb(sii9234, MHL_TX_INTR4_REG, intr4);
	cbus_writeb(sii9234, CBUS_MHL_STATUS_REG_0, 0xFF);
	cbus_writeb(sii9234, CBUS_MHL_STATUS_REG_1, 0xFF);
	cbus_writeb(sii9234, CBUS_INT_STATUS_1_REG, cbus_intr1);
	cbus_writeb(sii9234, CBUS_INT_STATUS_2_REG, cbus_intr2);

	sii9234_clear_error(sii9234);

	if (sii9234->state == ST_FAILURE) {
		dev_info(dev, "try to reset after failure\n");
		sii9234_hw_reset(sii9234);
		sii9234_goto_d3(sii9234);
	}

	if (sii9234->state == ST_FAILURE_DISCOVERY) {
		dev_err(dev, "discovery failed, no power for MHL?\n");
		tpi_writebm(sii9234, TPI_DPD_REG, 0, 1);
		sii9234->state = ST_D3;
	}

	mutex_unlock(&sii9234->lock);

	return IRQ_HANDLED;
}

static void sii9234_extcon_work(struct work_struct *work)
{
	struct sii9234 *sii9234 =
		container_of(work, struct sii9234, extcon_wq);

	dev_info(sii9234_dev(sii9234), "MHL is %s\n",
		sii9234->extcon_attached ? "attached" : "detached");

	if (sii9234->extcon_attached)
		sii9234_cable_in(sii9234);
	else
		sii9234_cable_out(sii9234);
}

static int sii9234_extcon_notifier(struct notifier_block *self,
			unsigned long event, void *ptr)
{
	struct sii9234 *sii9234 =
		container_of(self, struct sii9234, extcon_nb);

	sii9234->extcon_attached = event;
	schedule_work(&sii9234->extcon_wq);

	return NOTIFY_DONE;
}

static int sii9234_extcon_init(struct sii9234 *sii9234)
{
	struct device *dev = &sii9234->client[I2C_MHL]->dev;
	struct extcon_dev *edev;
	int ret;

	if (of_property_read_bool(dev->of_node, "extcon")) {
		edev = extcon_get_edev_by_phandle(dev, 0);
		if (IS_ERR(edev)) {
			dev_err(dev, "failed to acquire extcon device\n");
			return PTR_ERR(edev);
		}

		sii9234->extcon_attached = extcon_get_cable_state(edev, "MHL");
		dev_info(dev, "extcon(MHL) = %d\n", sii9234->extcon_attached);

		sii9234->extcon_nb.notifier_call = sii9234_extcon_notifier;
		ret = extcon_register_interest(&sii9234->extcon_dev, edev->name,
			"MHL", &sii9234->extcon_nb);

		if (ret) {
			dev_err(dev, "failed to register notifier for MHL\n");
			return ret;
		}
		INIT_WORK(&sii9234->extcon_wq, sii9234_extcon_work);
	} else {
		dev_info(dev, "no extcon found, switching to 'always on' mode\n");
		sii9234->extcon_attached = true;
	}
	return 0;
}

static int sii9234_init_resources(struct sii9234 *sii9234,
	struct i2c_client *client)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "not DT device\n");
		return -ENODEV;
	}

	sii9234->gpio_n_reset = of_get_named_gpio(dev->of_node,
		"gpio-reset", 0);
	if (!gpio_is_valid(sii9234->gpio_n_reset)) {
		dev_err(dev, "failed to get gpio-reset from DT\n");
		return -ENODEV;
	}
	sii9234->gpio_int = of_get_named_gpio(dev->of_node, "gpio-int", 0);
	if (!gpio_is_valid(sii9234->gpio_int)) {
		dev_err(dev, "failed to get gpio-int from DT\n");
		return -ENODEV;
	}

	ret = devm_gpio_request(dev, sii9234->gpio_n_reset, "MHL_RST");
	if (ret) {
		dev_err(dev, "failed to acquire MHL_RST gpio\n");
		return ret;
	}

	ret = devm_gpio_request(dev, sii9234->gpio_int, "MHL_INT");
	if (ret) {
		dev_err(dev, "failed to request MHL_INT gpio\n");
		return ret;
	}

	sii9234->vcc_supply = devm_regulator_get(dev, "vcc");
	if (IS_ERR(sii9234->vcc_supply)) {
		dev_err(dev, "failed to acquire regulator vcc\n");
		return PTR_ERR(sii9234->vcc_supply);
	}

	sii9234->client[I2C_MHL] = client;

	sii9234->client[I2C_TPI] = i2c_new_dummy(adapter, 0x7A >> 1);
	if (!sii9234->client[I2C_TPI]) {
		dev_err(dev, "failed to create TPI client\n");
		return -ENODEV;
	}

	sii9234->client[I2C_HDMI] = i2c_new_dummy(adapter, 0x92 >> 1);
	if (!sii9234->client[I2C_HDMI]) {
		dev_err(dev, "failed to create HDMI RX client\n");
		goto fail_tpi;
	}

	sii9234->client[I2C_CBUS] = i2c_new_dummy(adapter, 0xC8 >> 1);
	if (!sii9234->client[I2C_CBUS]) {
		dev_err(dev, "failed to create CBUS client\n");
		goto fail_hdmi;
	}

	return 0;

fail_hdmi:
	i2c_unregister_device(sii9234->client[I2C_HDMI]);
fail_tpi:
	i2c_unregister_device(sii9234->client[I2C_TPI]);

	return -ENODEV;
}

static void sii9234_deinit_resources(struct sii9234 *sii9234)
{
	i2c_unregister_device(sii9234->client[I2C_CBUS]);
	i2c_unregister_device(sii9234->client[I2C_HDMI]);
	i2c_unregister_device(sii9234->client[I2C_TPI]);
}

static int sii9234_mhl_tx_i2c_probe(struct i2c_client *client,
					      const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sii9234 *sii9234;
	struct device *dev = &client->dev;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "I2C adapter lacks SMBUS feature\n");
		return -EIO;
	}

	sii9234 = devm_kzalloc(dev, sizeof(struct sii9234), GFP_KERNEL);
	if (!sii9234) {
		dev_err(dev, "not enough memory\n");
		return -ENOMEM;
	}

	mutex_init(&sii9234->lock);

	ret = sii9234_init_resources(sii9234, client);
	if (ret < 0) {
		dev_err(&client->dev, "failed to initialize sii9234 resources\n");
		return ret;
	}

	i2c_set_clientdata(client, sii9234);

	sii9234->irq = gpio_to_irq(sii9234->gpio_int);
	ret = devm_request_threaded_irq(dev, sii9234->irq, NULL,
		sii9234_irq_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
		"sii9234", sii9234);
	if (ret < 0) {
		dev_err(dev, "failed to request MHL interrupt\n");
		goto err_resource;
	}

	disable_irq(sii9234->irq);

	ret = sii9234_extcon_init(sii9234);
	if (ret < 0) {
		dev_err(dev, "failed to initialize EXTCON\n");
		goto err_resource;
	}

	/* Force MHL connect EVENT for platforms with no EXTCON */
	if (sii9234->extcon_attached)
		sii9234_cable_in(sii9234);

	return 0;

err_resource:
	sii9234_deinit_resources(sii9234);

	return ret;
}

static int sii9234_mhl_tx_i2c_remove(struct i2c_client *client)
{
	struct sii9234 *sii9234 = i2c_get_clientdata(client);

	extcon_unregister_interest(&sii9234->extcon_dev);
	if (sii9234->extcon_attached)
		sii9234_cable_out(sii9234);
	sii9234_deinit_resources(sii9234);

	return 0;
}

static const struct of_device_id sii9234_dt_match[] = {
	{ .compatible = "sii,sii9234" },
	{ },
};
MODULE_DEVICE_TABLE(of, sii9234_dt_match);

static const struct i2c_device_id sii9234_id[] = {
	{ "SII9234", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, sii9234_id);
static struct i2c_driver sii9234_driver = {
	.driver = {
		.name	= "sii9234",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(sii9234_dt_match),
	},
	.probe		= sii9234_mhl_tx_i2c_probe,
	.remove		= sii9234_mhl_tx_i2c_remove,
	.id_table = sii9234_id,
};

module_i2c_driver(sii9234_driver);
MODULE_LICENSE("GPL");
