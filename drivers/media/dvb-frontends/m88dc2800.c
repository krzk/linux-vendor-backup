/*
    M88DC2800/M88TC2800  - DVB-C demodulator and tuner from Montage

    Copyright (C) 2012 Max nibble<nibble.max@gmail.com>
    Copyright (C) 2011 Montage Technology / www.montage-tech.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/div64.h>
#include "dvb_frontend.h"
#include "m88dc2800.h"

struct m88dc2800_state {
	struct i2c_adapter *i2c;
	const struct m88dc2800_config *config;
	struct dvb_frontend frontend;
	u32 freq;
	u32 ber;
	u32 sym;
	u16 qam;
	u8 inverted;
	u32 xtal;
	/* tuner state */
	u8 tuner_init_OK;	/* Tuner initialize status */
	u8 tuner_dev_addr;	/* Tuner device address */
	u32 tuner_freq;		/* RF frequency to be set, unit: KHz */
	u16 tuner_qam;		/* Reserved */
	u16 tuner_mode;
	u8 tuner_bandwidth;	/* Bandwidth of the channel, unit: MHz, 6/7/8 */
	u8 tuner_loopthrough;	/* Tuner loop through switch, 0/1 */
	u32 tuner_crystal;	/* Tuner crystal frequency, unit: KHz */
	u32 tuner_dac;		/* Tuner DAC frequency, unit: KHz */
	u16 tuner_mtt;		/* Tuner chip version, D1: 0x0d, E0: 0x0e, E1: 0x8e */
	u16 tuner_custom_cfg;
	u32 tuner_version;	/* Tuner driver version number */
	u32 tuner_time;
};

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Activates frontend debugging (default:0)");

#define dprintk(args...) \
	do { \
		if (debug) \
			printk(KERN_INFO "m88dc2800: " args); \
	} while (0)


static int m88dc2800_i2c_write(struct m88dc2800_state *state, u8 addr,
			       u8 * p_data, u8 len)
{
	struct i2c_msg msg = { .flags = 0 };

	msg.addr = addr;
	msg.buf = p_data;
	msg.len = len;

	return i2c_transfer(state->i2c, &msg, 1);
}

static int m88dc2800_i2c_read(struct m88dc2800_state *state, u8 addr,
			      u8 * p_data, u8 len)
{
	struct i2c_msg msg = { .flags = I2C_M_RD };

	msg.addr = addr;
	msg.buf = p_data;
	msg.len = len;

	return i2c_transfer(state->i2c, &msg, 1);
}

/*demod register operations.*/
static int WriteReg(struct m88dc2800_state *state, u8 reg, u8 data)
{
	u8 buf[] = { reg, data };
	u8 addr = state->config->demod_address;
	int err;

	dprintk("%s: write reg 0x%02x, value 0x%02x\n", __func__, reg, data);

	err = m88dc2800_i2c_write(state, addr, buf, 2);

	if (err != 1) {
		printk(KERN_ERR
		       "%s: writereg error(err == %i, reg == 0x%02x,"
		       " value == 0x%02x)\n", __func__, err, reg, data);
		return -EIO;
	}
	return 0;
}

static int ReadReg(struct m88dc2800_state *state, u8 reg)
{
	int ret;
	u8 b0[] = { reg };
	u8 b1[] = { 0 };
	u8 addr = state->config->demod_address;

	ret = m88dc2800_i2c_write(state, addr, b0, 1);

	if (ret != 1) {
		printk(KERN_ERR "%s: reg=0x%x (error=%d)\n",
		       __func__, reg, ret);
		return -EIO;
	}

	ret = m88dc2800_i2c_read(state, addr, b1, 1);

	dprintk("%s: read reg 0x%02x, value 0x%02x\n", __func__, reg, b1[0]);
	return b1[0];
}

static int _mt_fe_tn_set_reg(struct m88dc2800_state *state, u8 reg,
			     u8 data)
{
	int ret;
	u8 buf[2];
	u8 addr = state->tuner_dev_addr;

	buf[1] = ReadReg(state, 0x86);
	buf[1] |= 0x80;
	ret = WriteReg(state, 0x86, buf[1]);

	buf[0] = reg;
	buf[1] = data;

	ret = m88dc2800_i2c_write(state, addr, buf, 2);
	if (ret != 1)
		return -EIO;
	return 0;
}

static int _mt_fe_tn_get_reg(struct m88dc2800_state *state, u8 reg,
			     u8 * p_data)
{
	int ret;
	u8 buf[2];
	u8 addr = state->tuner_dev_addr;

	buf[1] = ReadReg(state, 0x86);
	buf[1] |= 0x80;
	ret = WriteReg(state, 0x86, buf[1]);

	buf[0] = reg;
	ret = m88dc2800_i2c_write(state, addr, buf, 1);

	msleep(1);

	buf[1] = ReadReg(state, 0x86);
	buf[1] |= 0x80;
	ret = WriteReg(state, 0x86, buf[1]);

	return m88dc2800_i2c_read(state, addr, p_data, 1);
}

/* Tuner operation functions.*/
static int _mt_fe_tn_set_RF_front_tc2800(struct m88dc2800_state *state)
{
	u32 freq_KHz = state->tuner_freq;
	u8 a, b, c;
	if (state->tuner_mtt == 0xD1) {	/* D1 */
		if (freq_KHz <= 123000) {
			if (freq_KHz <= 56000) {
				a = 0x00; b = 0x00; c = 0x00;
			} else if (freq_KHz <= 64000) {
				a = 0x10; b = 0x01; c = 0x08;
			} else if (freq_KHz <= 72000) {
				a = 0x20; b = 0x02; c = 0x10;
			} else if (freq_KHz <= 80000) {
				a = 0x30; b = 0x03; c = 0x18;
			} else if (freq_KHz <= 88000) {
				a = 0x40; b = 0x04; c = 0x20;
			} else if (freq_KHz <= 96000) {
				a = 0x50; b = 0x05; c = 0x28;
			} else if (freq_KHz <= 104000) {
				a = 0x60; b = 0x06; c = 0x30;
			} else {
				a = 0x70; b = 0x07; c = 0x38;
			}
			_mt_fe_tn_set_reg(state, 0x58, 0x9b);
			_mt_fe_tn_set_reg(state, 0x59, a);
			_mt_fe_tn_set_reg(state, 0x5d, b);
			_mt_fe_tn_set_reg(state, 0x5e, c);
			_mt_fe_tn_set_reg(state, 0x5a, 0x75);
			_mt_fe_tn_set_reg(state, 0x73, 0x0c);
		} else {	/* if (freq_KHz > 112000) */
			_mt_fe_tn_set_reg(state, 0x58, 0x7b);
			if (freq_KHz <= 304000) {
				if (freq_KHz <= 136000) {
					_mt_fe_tn_set_reg(state, 0x5e, 0x40);
				} else if (freq_KHz <= 160000) {
					_mt_fe_tn_set_reg(state, 0x5e, 0x48);
				} else if (freq_KHz <= 184000) {
					_mt_fe_tn_set_reg(state, 0x5e, 0x50);
				} else if (freq_KHz <= 208000) {
					_mt_fe_tn_set_reg(state, 0x5e, 0x58);
				} else if (freq_KHz <= 232000) {
					_mt_fe_tn_set_reg(state, 0x5e, 0x60);
				} else if (freq_KHz <= 256000) {
					_mt_fe_tn_set_reg(state, 0x5e, 0x68);
				} else if (freq_KHz <= 280000) {
					_mt_fe_tn_set_reg(state, 0x5e, 0x70);
				} else {	/* if (freq_KHz <= 304000) */
					_mt_fe_tn_set_reg(state, 0x5e, 0x78);
				}
				if (freq_KHz <= 171000) {
					_mt_fe_tn_set_reg(state, 0x73, 0x08);
				} else if (freq_KHz <= 211000) {
					_mt_fe_tn_set_reg(state, 0x73, 0x0a);
				} else {
					_mt_fe_tn_set_reg(state, 0x73, 0x0e);
				}
			} else {	/* if (freq_KHz > 304000) */
				_mt_fe_tn_set_reg(state, 0x5e, 0x88);
				if (freq_KHz <= 400000) {
					_mt_fe_tn_set_reg(state, 0x73, 0x0c);
				} else if (freq_KHz <= 450000) {
					_mt_fe_tn_set_reg(state, 0x73, 0x09);
				} else if (freq_KHz <= 550000) {
					_mt_fe_tn_set_reg(state, 0x73, 0x0e);
				} else if (freq_KHz <= 650000) {
					_mt_fe_tn_set_reg(state, 0x73, 0x0d);
				} else {	/*if (freq_KHz > 650000) */
					_mt_fe_tn_set_reg(state, 0x73, 0x0e);
				}
			}
		}
		if (freq_KHz > 800000)
			_mt_fe_tn_set_reg(state, 0x87, 0x24);
		else if (freq_KHz > 700000)
			_mt_fe_tn_set_reg(state, 0x87, 0x34);
		else if (freq_KHz > 500000)
			_mt_fe_tn_set_reg(state, 0x87, 0x44);
		else if (freq_KHz > 300000)
			_mt_fe_tn_set_reg(state, 0x87, 0x43);
		else if (freq_KHz > 220000)
			_mt_fe_tn_set_reg(state, 0x87, 0x54);
		else if (freq_KHz > 110000)
			_mt_fe_tn_set_reg(state, 0x87, 0x14);
		else
			_mt_fe_tn_set_reg(state, 0x87, 0x54);
		if (freq_KHz > 600000)
			_mt_fe_tn_set_reg(state, 0x6a, 0x53);
		else if (freq_KHz > 500000)
			_mt_fe_tn_set_reg(state, 0x6a, 0x57);
		else
			_mt_fe_tn_set_reg(state, 0x6a, 0x59);
		if (freq_KHz < 200000) {
			_mt_fe_tn_set_reg(state, 0x20, 0x5d);
		} else if (freq_KHz < 500000) {
			_mt_fe_tn_set_reg(state, 0x20, 0x7d);
		} else {
			_mt_fe_tn_set_reg(state, 0x20, 0xfd);
		}		/* end of 0xD1 */
	} else if (state->tuner_mtt == 0xE1) {	/* E1 */
		if (freq_KHz <= 112000) {	/* 123MHz */
			if (freq_KHz <= 56000) {
				_mt_fe_tn_set_reg(state, 0x5c, 0x01);
			} else if (freq_KHz <= 64000) {
				_mt_fe_tn_set_reg(state, 0x5c, 0x09);
			} else if (freq_KHz <= 72000) {
				_mt_fe_tn_set_reg(state, 0x5c, 0x11);
			} else if (freq_KHz <= 80000) {
				_mt_fe_tn_set_reg(state, 0x5c, 0x19);
			} else if (freq_KHz <= 88000) {
				_mt_fe_tn_set_reg(state, 0x5c, 0x21);
			} else if (freq_KHz <= 96000) {
				_mt_fe_tn_set_reg(state, 0x5c, 0x29);
			} else if (freq_KHz <= 104000) {
				_mt_fe_tn_set_reg(state, 0x5c, 0x31);
			} else {	/* if (freq_KHz <= 112000) */
				_mt_fe_tn_set_reg(state, 0x5c, 0x39);
			}
			_mt_fe_tn_set_reg(state, 0x5b, 0x30);
		} else {	/* if (freq_KHz > 112000) */
			if (freq_KHz <= 304000) {
				if (freq_KHz <= 136000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x41);
				} else if (freq_KHz <= 160000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x49);
				} else if (freq_KHz <= 184000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x51);
				} else if (freq_KHz <= 208000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x59);
				} else if (freq_KHz <= 232000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x61);
				} else if (freq_KHz <= 256000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x69);
				} else if (freq_KHz <= 280000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x71);
				} else {	/* if (freq_KHz <= 304000) */
					_mt_fe_tn_set_reg(state, 0x5c, 0x79);
				}
				if (freq_KHz <= 150000) {
					_mt_fe_tn_set_reg(state, 0x5b, 0x28);
				} else if (freq_KHz <= 256000) {
					_mt_fe_tn_set_reg(state, 0x5b, 0x29);
				} else {
					_mt_fe_tn_set_reg(state, 0x5b, 0x2a);
				}
			} else {	/* if (freq_KHz > 304000) */
				if (freq_KHz <= 400000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x89);
				} else if (freq_KHz <= 450000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x91);
				} else if (freq_KHz <= 650000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0x98);
				} else if (freq_KHz <= 850000) {
					_mt_fe_tn_set_reg(state, 0x5c, 0xa0);
				} else {
					_mt_fe_tn_set_reg(state, 0x5c, 0xa8);
				}
				_mt_fe_tn_set_reg(state, 0x5b, 0x08);
			}
		}
	}			/* end of 0xE1 */
	return 0;
}

static int _mt_fe_tn_cali_PLL_tc2800(struct m88dc2800_state *state,
				     u32 freq_KHz,
				     u32 cali_freq_thres_div2,
				     u32 cali_freq_thres_div3r,
				     u32 cali_freq_thres_div3)
{
	s32 N, F, MUL;
	u8 buf, tmp, tmp2;
	s32 M;
	const s32 crystal_KHz = state->tuner_crystal;
	 if (state->tuner_mtt == 0xD1) {
		M = state->tuner_crystal / 4000;
		if (freq_KHz > cali_freq_thres_div2) {
			MUL = 4;
			tmp = 2;
		} else if (freq_KHz > 300000) {
			MUL = 8;
			tmp = 3;
		} else if (freq_KHz > (cali_freq_thres_div2 / 2)) {
			MUL = 8;
			tmp = 4;
		} else if (freq_KHz > (cali_freq_thres_div2 / 4)) {
			MUL = 16;
			tmp = 5;
		} else if (freq_KHz > (cali_freq_thres_div2 / 8)) {
			MUL = 32;
			tmp = 6;
		} else if (freq_KHz > (cali_freq_thres_div2 / 16)) {
			MUL = 64;
			tmp = 7;
		} else {	/* invalid */
			MUL = 0;
			tmp = 0;
			return 1;
		}
	} else if (state->tuner_mtt == 0xE1) {
		M = state->tuner_crystal / 1000;
		_mt_fe_tn_set_reg(state, 0x30, 0xff);
		_mt_fe_tn_set_reg(state, 0x32, 0xe0);
		_mt_fe_tn_set_reg(state, 0x33, 0x86);
		_mt_fe_tn_set_reg(state, 0x37, 0x70);
		_mt_fe_tn_set_reg(state, 0x38, 0x20);
		_mt_fe_tn_set_reg(state, 0x39, 0x18);
		_mt_fe_tn_set_reg(state, 0x89, 0x83);
		if (freq_KHz > cali_freq_thres_div2) {
			M = M / 4;
			MUL = 4;
			tmp = 2;
			tmp2 = M + 16;	/* 48 */
		} else if (freq_KHz > cali_freq_thres_div3r) {
			M = M / 3;
			MUL = 6;
			tmp = 2;
			tmp2 = M + 32;	/* 32 */
		} else if (freq_KHz > cali_freq_thres_div3) {
			M = M / 3;
			MUL = 6;
			tmp = 2;
			tmp2 = M;	/* 16 */
		} else if (freq_KHz > 304000) {
			M = M / 4;
			MUL = 8;
			tmp = 3;
			tmp2 = M + 16;	/* 48 */
		} else if (freq_KHz > (cali_freq_thres_div2 / 2)) {
			M = M / 4;
			MUL = 8;
			tmp = 4;
			tmp2 = M + 16;	/* 48 */
		} else if (freq_KHz > (cali_freq_thres_div3r / 2)) {
			M = M / 3;
			MUL = 12;
			tmp = 4;
			tmp2 = M + 32;	/* 32 */
		} else if (freq_KHz > (cali_freq_thres_div3 / 2)) {
			M = M / 3;
			MUL = 12;
			tmp = 4;
			tmp2 = M;	/* 16 */
		} else if (freq_KHz > (cali_freq_thres_div2 / 4)) {
			M = M / 4;
			MUL = 16;
			tmp = 5;
			tmp2 = M + 16;	/* 48 */
		} else if (freq_KHz > (cali_freq_thres_div3r / 4)) {
			M = M / 3;
			MUL = 24;
			tmp = 5;
			tmp2 = M + 32;	/* 32 */
		} else if (freq_KHz > (cali_freq_thres_div3 / 4)) {
			M = M / 3;
			MUL = 24;
			tmp = 5;
			tmp2 = M;	/* 16 */
		} else if (freq_KHz > (cali_freq_thres_div2 / 8)) {
			M = M / 4;
			MUL = 32;
			tmp = 6;
			tmp2 = M + 16;	/* 48 */
		} else if (freq_KHz > (cali_freq_thres_div3r / 8)) {
			M = M / 3;
			MUL = 48;
			tmp = 6;
			tmp2 = M + 32;	/* 32 */
		} else if (freq_KHz > (cali_freq_thres_div3 / 8)) {
			M = M / 3;
			MUL = 48;
			tmp = 6;
			tmp2 = M;	/* 16 */
		} else if (freq_KHz > (cali_freq_thres_div2 / 16)) {
			M = M / 4;
			MUL = 64;
			tmp = 7;
			tmp2 = M + 16;	/* 48 */
		} else if (freq_KHz > (cali_freq_thres_div3r / 16)) {
			M = M / 3;
			MUL = 96;
			tmp = 7;
			tmp2 = M + 32;	/* 32 */
		} else if (freq_KHz > (cali_freq_thres_div3 / 16)) {
			M = M / 3;
			MUL = 96;
			tmp = 7;
			tmp2 = M;	/* 16 */
		} else {	/* invalid */
			M = M / 4;
			MUL = 0;
			tmp = 0;
			tmp2 = 48;
			return 1;
		}
		if (freq_KHz == 291000) {
			M = state->tuner_crystal / 1000 / 3;
			MUL = 12;
			tmp = 4;
			tmp2 = M + 32;	/* 32 */
		}
		/*
		   if (freq_KHz == 578000) {
		   M = state->tuner_crystal / 1000 / 4;
		   MUL = 4;
		   tmp = 2;
		   tmp2 = M + 16;	// 48
		   }
		 */
		if (freq_KHz == 690000) {
			M = state->tuner_crystal / 1000 / 3;
			MUL = 4;
			tmp = 2;
			tmp2 = M + 16;	/* 48 */
		}
		_mt_fe_tn_get_reg(state, 0x33, &buf);
		buf &= 0xc0;
		buf += tmp2;
		_mt_fe_tn_set_reg(state, 0x33, buf);
	} else {
		return 1;
	}
	_mt_fe_tn_get_reg(state, 0x39, &buf);
	buf &= 0xf8;
	buf += tmp;
	_mt_fe_tn_set_reg(state, 0x39, buf);
	N = (freq_KHz * MUL * M / crystal_KHz) / 2 * 2 - 256;
	buf = (N >> 8) & 0xcf;
	if (state->tuner_mtt == 0xE1) {
		buf |= 0x30;
	}
	_mt_fe_tn_set_reg(state, 0x34, buf);
	buf = N & 0xff;
	_mt_fe_tn_set_reg(state, 0x35, buf);
	F = ((freq_KHz * MUL * M / (crystal_KHz / 1000) / 2) -
	     (freq_KHz * MUL * M / crystal_KHz / 2 * 1000)) * 64 / 1000;
	buf = F & 0xff;
	_mt_fe_tn_set_reg(state, 0x36, buf);
	if (F == 0) {
		if (state->tuner_mtt == 0xD1) {
			_mt_fe_tn_set_reg(state, 0x3d, 0xca);
		} else if (state->tuner_mtt == 0xE1) {
			_mt_fe_tn_set_reg(state, 0x3d, 0xfe);
		} else {
			return 1;
		}
		_mt_fe_tn_set_reg(state, 0x3e, 0x9c);
		_mt_fe_tn_set_reg(state, 0x3f, 0x34);
	}
	if (F > 0) {
		if (state->tuner_mtt == 0xD1) {
			if ((F == 32) || (F == 16) || (F == 48)) {
				_mt_fe_tn_set_reg(state, 0x3e, 0xa4);
				_mt_fe_tn_set_reg(state, 0x3d, 0x4a);
				_mt_fe_tn_set_reg(state, 0x3f, 0x36);
			} else {
				_mt_fe_tn_set_reg(state, 0x3e, 0xa4);
				_mt_fe_tn_set_reg(state, 0x3d, 0x4a);
				_mt_fe_tn_set_reg(state, 0x3f, 0x36);
			}
		} else if (state->tuner_mtt == 0xE1) {
			_mt_fe_tn_set_reg(state, 0x3e, 0xa4);
			_mt_fe_tn_set_reg(state, 0x3d, 0x7e);
			_mt_fe_tn_set_reg(state, 0x3f, 0x36);
			_mt_fe_tn_set_reg(state, 0x89, 0x84);
			_mt_fe_tn_get_reg(state, 0x39, &buf);
			buf = buf & 0x1f;
			_mt_fe_tn_set_reg(state, 0x39, buf);
			_mt_fe_tn_get_reg(state, 0x32, &buf);
			buf = buf | 0x02;
			_mt_fe_tn_set_reg(state, 0x32, buf);
		} else {
			return 1;
		}
	}
	_mt_fe_tn_set_reg(state, 0x41, 0x00);
	if (state->tuner_mtt == 0xD1) {
		msleep(5);
	} else if (state->tuner_mtt == 0xE1) {
		msleep(2);
	} else {
		return 1;
	}
	_mt_fe_tn_set_reg(state, 0x41, 0x02);
	_mt_fe_tn_set_reg(state, 0x30, 0x7f);
	_mt_fe_tn_set_reg(state, 0x30, 0xff);
	_mt_fe_tn_set_reg(state, 0x31, 0x80);
	_mt_fe_tn_set_reg(state, 0x31, 0x00);

	return 0;
}

static int _mt_fe_tn_set_PLL_freq_tc2800(struct m88dc2800_state *state)
{
	u8 buf, buf1;
	u32 freq_thres_div2_KHz, freq_thres_div3r_KHz,
	    freq_thres_div3_KHz;
	const u32 freq_KHz = state->tuner_freq;
	if (state->tuner_mtt == 0xD1) {
		_mt_fe_tn_set_reg(state, 0x32, 0xe1);
		_mt_fe_tn_set_reg(state, 0x33, 0xa6);
		_mt_fe_tn_set_reg(state, 0x37, 0x7f);
		_mt_fe_tn_set_reg(state, 0x38, 0x20);
		_mt_fe_tn_set_reg(state, 0x39, 0x18);
		_mt_fe_tn_set_reg(state, 0x40, 0x40);
		 freq_thres_div2_KHz = 520000;
		_mt_fe_tn_cali_PLL_tc2800(state, freq_KHz,
					   freq_thres_div2_KHz, 0, 0);
		 msleep(5);
		_mt_fe_tn_get_reg(state, 0x3a, &buf);
		buf1 = buf;
		buf = buf & 0x03;
		buf1 = buf1 & 0x01;
		if ((buf1 == 0) || (buf == 3)) {
			freq_thres_div2_KHz = 420000;
			_mt_fe_tn_cali_PLL_tc2800(state, freq_KHz,
						   freq_thres_div2_KHz, 0,
						   0);
			msleep(5);
			 _mt_fe_tn_get_reg(state, 0x3a, &buf);
			buf = buf & 0x07;
			if (buf == 5) {
				freq_thres_div2_KHz = 520000;
				_mt_fe_tn_cali_PLL_tc2800(state, freq_KHz,
							   freq_thres_div2_KHz,
							   0, 0);
				msleep(5);
			}
		}
		 _mt_fe_tn_get_reg(state, 0x38, &buf);
		_mt_fe_tn_set_reg(state, 0x38, buf);
		 _mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = buf | 0x10;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		 _mt_fe_tn_set_reg(state, 0x30, 0x7f);
		_mt_fe_tn_set_reg(state, 0x30, 0xff);
		 _mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = buf & 0xdf;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		_mt_fe_tn_set_reg(state, 0x40, 0x0);
		 _mt_fe_tn_set_reg(state, 0x30, 0x7f);
		_mt_fe_tn_set_reg(state, 0x30, 0xff);
		_mt_fe_tn_set_reg(state, 0x31, 0x80);
		_mt_fe_tn_set_reg(state, 0x31, 0x00);
		msleep(5);
		 _mt_fe_tn_get_reg(state, 0x39, &buf);
		buf = buf >> 5;
		if (buf < 5) {
			_mt_fe_tn_get_reg(state, 0x39, &buf);
			buf = buf | 0xa0;
			buf = buf & 0xbf;
			_mt_fe_tn_set_reg(state, 0x39, buf);
			 _mt_fe_tn_get_reg(state, 0x32, &buf);
			buf = buf | 0x02;
			_mt_fe_tn_set_reg(state, 0x32, buf);
		}
		 _mt_fe_tn_get_reg(state, 0x37, &buf);
		if (buf > 0x70) {
			buf = 0x7f;
			_mt_fe_tn_set_reg(state, 0x40, 0x40);
		}
		_mt_fe_tn_set_reg(state, 0x37, buf);
		  _mt_fe_tn_get_reg(state, 0x38, &buf);
		if (buf < 0x0f) {
			buf = (buf & 0x0f) << 2;
			buf = buf + 0x0f;
			_mt_fe_tn_set_reg(state, 0x37, buf);
		} else if (buf < 0x1f) {
			buf = buf + 0x0f;
			_mt_fe_tn_set_reg(state, 0x37, buf);
		}
		 _mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = (buf | 0x20) & 0xef;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		 _mt_fe_tn_set_reg(state, 0x41, 0x00);
		msleep(5);
		_mt_fe_tn_set_reg(state, 0x41, 0x02);
	} else if (state->tuner_mtt == 0xE1) {
		freq_thres_div2_KHz = 580000;
		freq_thres_div3r_KHz = 500000;
		freq_thres_div3_KHz = 440000;
		_mt_fe_tn_cali_PLL_tc2800(state, freq_KHz,
					   freq_thres_div2_KHz,
					   freq_thres_div3r_KHz,
					   freq_thres_div3_KHz);
		msleep(3);
		_mt_fe_tn_get_reg(state, 0x38, &buf);
		_mt_fe_tn_set_reg(state, 0x38, buf);
		_mt_fe_tn_set_reg(state, 0x30, 0x7f);
		_mt_fe_tn_set_reg(state, 0x30, 0xff);
		_mt_fe_tn_set_reg(state, 0x31, 0x80);
		_mt_fe_tn_set_reg(state, 0x31, 0x00);
		msleep(3);
		_mt_fe_tn_get_reg(state, 0x38, &buf);
		_mt_fe_tn_set_reg(state, 0x38, buf);
		_mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = buf | 0x10;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		 _mt_fe_tn_set_reg(state, 0x30, 0x7f);
		_mt_fe_tn_set_reg(state, 0x30, 0xff);
		_mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = buf & 0xdf;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		_mt_fe_tn_set_reg(state, 0x31, 0x80);
		_mt_fe_tn_set_reg(state, 0x31, 0x00);
		msleep(3);
		_mt_fe_tn_get_reg(state, 0x37, &buf);
		_mt_fe_tn_set_reg(state, 0x37, buf);
		/*
		   if ((freq_KHz == 802000) || (freq_KHz == 826000)) {
		   _mt_fe_tn_set_reg(state, 0x37, 0x5e);
		   }
		 */
		_mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = (buf & 0xef) | 0x30;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		 _mt_fe_tn_set_reg(state, 0x41, 0x00);
		msleep(2);
		_mt_fe_tn_set_reg(state, 0x41, 0x02);
	} else {
		return 1;
	}
	return 0;
}

static int _mt_fe_tn_set_BB_tc2800(struct m88dc2800_state *state)
{
	return 0;
}

 static int _mt_fe_tn_set_appendix_tc2800(struct m88dc2800_state *state)

{
	u8 buf;
	const u32 freq_KHz = state->tuner_freq;
	if (state->tuner_mtt == 0xD1) {
		if ((freq_KHz == 123000) || (freq_KHz == 147000) ||
		    (freq_KHz == 171000) || (freq_KHz == 195000)) {
			_mt_fe_tn_set_reg(state, 0x20, 0x1b);
		}
		if ((freq_KHz == 371000) || (freq_KHz == 419000) ||
		    (freq_KHz == 610000) || (freq_KHz == 730000) ||
		    (freq_KHz == 754000) || (freq_KHz == 826000)) {
			_mt_fe_tn_get_reg(state, 0x0d, &buf);
			_mt_fe_tn_set_reg(state, 0x0d, (u8) (buf + 1));
		}
		if ((freq_KHz == 522000) || (freq_KHz == 578000) ||
		    (freq_KHz == 634000) || (freq_KHz == 690000) ||
		    (freq_KHz == 834000)) {
			_mt_fe_tn_get_reg(state, 0x0d, &buf);
			_mt_fe_tn_set_reg(state, 0x0d, (u8) (buf - 1));
		}
	} else if (state->tuner_mtt == 0xE1) {
		_mt_fe_tn_set_reg(state, 0x20, 0xfc);
		if (freq_KHz == 123000 || freq_KHz == 147000 ||
		    freq_KHz == 171000 || freq_KHz == 195000 ||
		    freq_KHz == 219000 || freq_KHz == 267000 ||
		    freq_KHz == 291000 || freq_KHz == 339000 ||
		    freq_KHz == 387000 || freq_KHz == 435000 ||
		    freq_KHz == 482000 || freq_KHz == 530000 ||
		    freq_KHz == 722000 ||
		    (state->tuner_custom_cfg == 1 && freq_KHz == 315000)) {
			_mt_fe_tn_set_reg(state, 0x20, 0x5c);
		}
	}
	return 0;
}

 static int _mt_fe_tn_set_DAC_tc2800(struct m88dc2800_state *state)
{
	u8 buf, tempnumber;
	s32 N;
	s32 f1f2number, f1, f2, delta1, Totalnum1;
	s32 cntT, cntin, NCOI, z0, z1, z2, tmp;
	u32 fc, fadc, fsd, f2d;
	u32 FreqTrue108_Hz;
	s32 M = state->tuner_crystal / 4000;
	/* const u8 bandwidth = state->tuner_bandwidth; */
	const u16 DAC_fre = 108;
	const u32 crystal_KHz = state->tuner_crystal;
	const u32 DACFreq_KHz = state->tuner_dac;
	const u32 freq_KHz = state->tuner_freq;

	if (state->tuner_mtt == 0xE1) {
		_mt_fe_tn_get_reg(state, 0x33, &buf);
		M = buf & 0x0f;
		if (M == 0)
			M = 6;
	}
	_mt_fe_tn_get_reg(state, 0x34, &buf);
	N = buf & 0x07;
	_mt_fe_tn_get_reg(state, 0x35, &buf);
	N = (N << 8) + buf;
	buf = ((N + 256) * crystal_KHz / M / DAC_fre + 500) / 1000;
	if (state->tuner_mtt == 0xE1) {
		_mt_fe_tn_set_appendix_tc2800(state);
		if (freq_KHz == 187000 || freq_KHz == 195000 ||
		    freq_KHz == 131000 || freq_KHz == 211000 ||
		    freq_KHz == 219000 || freq_KHz == 227000 ||
		    freq_KHz == 267000 || freq_KHz == 299000 ||
		    freq_KHz == 347000 || freq_KHz == 363000 ||
		    freq_KHz == 395000 || freq_KHz == 403000 ||
		    freq_KHz == 435000 || freq_KHz == 482000 ||
		    freq_KHz == 474000 || freq_KHz == 490000 ||
		    freq_KHz == 610000 || freq_KHz == 642000 ||
		    freq_KHz == 666000 || freq_KHz == 722000 ||
		    freq_KHz == 754000 ||
		    ((freq_KHz == 379000 || freq_KHz == 467000 ||
		      freq_KHz == 762000) && state->tuner_custom_cfg != 1)) {
			buf = buf + 1;
		}
		if (freq_KHz == 123000 || freq_KHz == 139000 ||
		    freq_KHz == 147000 || freq_KHz == 171000 ||
		    freq_KHz == 179000 || freq_KHz == 203000 ||
		    freq_KHz == 235000 || freq_KHz == 251000 ||
		    freq_KHz == 259000 || freq_KHz == 283000 ||
		    freq_KHz == 331000 || freq_KHz == 363000 ||
		    freq_KHz == 371000 || freq_KHz == 387000 ||
		    freq_KHz == 411000 || freq_KHz == 427000 ||
		    freq_KHz == 443000 || freq_KHz == 451000 ||
		    freq_KHz == 459000 || freq_KHz == 506000 ||
		    freq_KHz == 514000 || freq_KHz == 538000 ||
		    freq_KHz == 546000 || freq_KHz == 554000 ||
		    freq_KHz == 562000 || freq_KHz == 570000 ||
		    freq_KHz == 578000 || freq_KHz == 602000 ||
		    freq_KHz == 626000 || freq_KHz == 658000 ||
		    freq_KHz == 690000 || freq_KHz == 714000 ||
		    freq_KHz == 746000 || freq_KHz == 522000 ||
		    freq_KHz == 826000 || freq_KHz == 155000 ||
		    freq_KHz == 530000 ||
		    ((freq_KHz == 275000 || freq_KHz == 355000) &&
		     state->tuner_custom_cfg != 1) ||
		    ((freq_KHz == 467000 || freq_KHz == 762000 ||
		      freq_KHz == 778000 || freq_KHz == 818000) &&
		     state->tuner_custom_cfg == 1)) {
			buf = buf - 1;
		}
	}
	 _mt_fe_tn_set_reg(state, 0x0e, buf);
	_mt_fe_tn_set_reg(state, 0x0d, buf);
	f1f2number =
	    (((DACFreq_KHz * M * buf) / crystal_KHz) << 16) / (N + 256) +
	    (((DACFreq_KHz * M * buf) % crystal_KHz) << 16) / ((N + 256) *
								crystal_KHz);
	_mt_fe_tn_set_reg(state, 0xf1, (f1f2number & 0xff00) >> 8);
	_mt_fe_tn_set_reg(state, 0xf2, f1f2number & 0x00ff);
	 FreqTrue108_Hz =
	    (N + 256) * crystal_KHz / (M * buf) * 1000 +
	    (((N + 256) * crystal_KHz) % (M * buf)) * 1000 / (M * buf);
	f1 = 4096;
	fc = FreqTrue108_Hz;
	fadc = fc / 4;
	fsd = 27000000;
	f2d = state->tuner_bandwidth * 1000 / 2 - 150;
	f2 = (fsd / 250) * f2d / ((fc + 500) / 1000);
	delta1 = ((f1 - f2) << 15) / f2;
	Totalnum1 = ((f1 - f2) << 15) - delta1 * f2;
	cntT = f2;
	cntin = Totalnum1;
	NCOI = delta1;
	 z0 = cntin;
	z1 = cntT;
	z2 = NCOI;
	tempnumber = (z0 & 0xff00) >> 8;
	_mt_fe_tn_set_reg(state, 0xc9, (u8) (tempnumber & 0x0f));
	tempnumber = (z0 & 0xff);
	_mt_fe_tn_set_reg(state, 0xca, tempnumber);
	 tempnumber = (z1 & 0xff00) >> 8;
	_mt_fe_tn_set_reg(state, 0xcb, tempnumber);
	tempnumber = (z1 & 0xff);
	_mt_fe_tn_set_reg(state, 0xcc, tempnumber);
	 tempnumber = (z2 & 0xff00) >> 8;
	_mt_fe_tn_set_reg(state, 0xcd, tempnumber);
	tempnumber = (z2 & 0xff);
	_mt_fe_tn_set_reg(state, 0xce, tempnumber);
	 tmp = f1;
	f1 = f2;
	f2 = tmp / 2;
	delta1 = ((f1 - f2) << 15) / f2;
	Totalnum1 = ((f1 - f2) << 15) - delta1 * f2;
	NCOI = (f1 << 15) / f2 - (1 << 15);
	cntT = f2;
	cntin = Totalnum1;
	z0 = cntin;
	z1 = cntT;
	z2 = NCOI;
	tempnumber = (z0 & 0xff00) >> 8;
	_mt_fe_tn_set_reg(state, 0xd9, (u8) (tempnumber & 0x0f));
	tempnumber = (z0 & 0xff);
	_mt_fe_tn_set_reg(state, 0xda, tempnumber);
	 tempnumber = (z1 & 0xff00) >> 8;
	_mt_fe_tn_set_reg(state, 0xdb, tempnumber);
	tempnumber = (z1 & 0xff);
	_mt_fe_tn_set_reg(state, 0xdc, tempnumber);
	 tempnumber = (z2 & 0xff00) >> 8;
	_mt_fe_tn_set_reg(state, 0xdd, tempnumber);
	tempnumber = (z2 & 0xff);
	_mt_fe_tn_set_reg(state, 0xde, tempnumber);

	return 0;
}

static int _mt_fe_tn_preset_tc2800(struct m88dc2800_state *state)
{
	if (state->tuner_mtt == 0xD1) {
		_mt_fe_tn_set_reg(state, 0x19, 0x4a);
		_mt_fe_tn_set_reg(state, 0x1b, 0x4b);
		 _mt_fe_tn_set_reg(state, 0x04, 0x04);
		_mt_fe_tn_set_reg(state, 0x17, 0x0d);
		_mt_fe_tn_set_reg(state, 0x62, 0x6c);
		_mt_fe_tn_set_reg(state, 0x63, 0xf4);
		_mt_fe_tn_set_reg(state, 0x1f, 0x0e);
		_mt_fe_tn_set_reg(state, 0x6b, 0xf4);
		_mt_fe_tn_set_reg(state, 0x14, 0x01);
		_mt_fe_tn_set_reg(state, 0x5a, 0x75);
		_mt_fe_tn_set_reg(state, 0x66, 0x74);
		_mt_fe_tn_set_reg(state, 0x72, 0xe0);
		_mt_fe_tn_set_reg(state, 0x70, 0x07);
		_mt_fe_tn_set_reg(state, 0x15, 0x7b);
		_mt_fe_tn_set_reg(state, 0x55, 0x71);
		 _mt_fe_tn_set_reg(state, 0x75, 0x55);
		_mt_fe_tn_set_reg(state, 0x76, 0xac);
		_mt_fe_tn_set_reg(state, 0x77, 0x6c);
		_mt_fe_tn_set_reg(state, 0x78, 0x8b);
		_mt_fe_tn_set_reg(state, 0x79, 0x42);
		_mt_fe_tn_set_reg(state, 0x7a, 0xd2);
		 _mt_fe_tn_set_reg(state, 0x81, 0x01);
		_mt_fe_tn_set_reg(state, 0x82, 0x00);
		_mt_fe_tn_set_reg(state, 0x82, 0x02);
		_mt_fe_tn_set_reg(state, 0x82, 0x04);
		_mt_fe_tn_set_reg(state, 0x82, 0x06);
		_mt_fe_tn_set_reg(state, 0x82, 0x08);
		_mt_fe_tn_set_reg(state, 0x82, 0x09);
		_mt_fe_tn_set_reg(state, 0x82, 0x29);
		_mt_fe_tn_set_reg(state, 0x82, 0x49);
		_mt_fe_tn_set_reg(state, 0x82, 0x58);
		_mt_fe_tn_set_reg(state, 0x82, 0x59);
		_mt_fe_tn_set_reg(state, 0x82, 0x98);
		_mt_fe_tn_set_reg(state, 0x82, 0x99);
		_mt_fe_tn_set_reg(state, 0x10, 0x05);
		_mt_fe_tn_set_reg(state, 0x10, 0x0d);
		_mt_fe_tn_set_reg(state, 0x11, 0x95);
		_mt_fe_tn_set_reg(state, 0x11, 0x9d);
		if (state->tuner_loopthrough != 0) {
			_mt_fe_tn_set_reg(state, 0x67, 0x25);
		} else {
			_mt_fe_tn_set_reg(state, 0x67, 0x05);
		}
	} else if (state->tuner_mtt == 0xE1) {
		_mt_fe_tn_set_reg(state, 0x1b, 0x47);
		if (state->tuner_mode == 0) {	/* DVB-C */
			_mt_fe_tn_set_reg(state, 0x66, 0x74);
			_mt_fe_tn_set_reg(state, 0x62, 0x2c);
			_mt_fe_tn_set_reg(state, 0x63, 0x54);
			_mt_fe_tn_set_reg(state, 0x68, 0x0b);
			_mt_fe_tn_set_reg(state, 0x14, 0x00);
		} else {			/* CTTB */
			_mt_fe_tn_set_reg(state, 0x66, 0x74);
			_mt_fe_tn_set_reg(state, 0x62, 0x0c);
			_mt_fe_tn_set_reg(state, 0x63, 0x54);
			_mt_fe_tn_set_reg(state, 0x68, 0x0b);
			_mt_fe_tn_set_reg(state, 0x14, 0x05);
		}
		_mt_fe_tn_set_reg(state, 0x6f, 0x00);
		_mt_fe_tn_set_reg(state, 0x84, 0x04);
		_mt_fe_tn_set_reg(state, 0x5e, 0xbe);
		_mt_fe_tn_set_reg(state, 0x87, 0x07);
		_mt_fe_tn_set_reg(state, 0x8a, 0x1f);
		_mt_fe_tn_set_reg(state, 0x8b, 0x1f);
		_mt_fe_tn_set_reg(state, 0x88, 0x30);
		_mt_fe_tn_set_reg(state, 0x58, 0x34);
		_mt_fe_tn_set_reg(state, 0x61, 0x8c);
		_mt_fe_tn_set_reg(state, 0x6a, 0x42);
	}
	return 0;
}

static int mt_fe_tn_wakeup_tc2800(struct m88dc2800_state *state)
{
	_mt_fe_tn_set_reg(state, 0x16, 0xb1);
	_mt_fe_tn_set_reg(state, 0x09, 0x7d);
	return 0;
}

  static int mt_fe_tn_sleep_tc2800(struct m88dc2800_state *state)
{
	_mt_fe_tn_set_reg(state, 0x16, 0xb0);
	_mt_fe_tn_set_reg(state, 0x09, 0x6d);
	return 0;
}

 static int mt_fe_tn_init_tc2800(struct m88dc2800_state *state)
{
	if (state->tuner_init_OK != 1) {
		state->tuner_dev_addr = 0x61;	/* TUNER_I2C_ADDR_TC2800 */
		state->tuner_freq = 650000;
		state->tuner_qam = 0;
		state->tuner_mode = 0;	// 0: DVB-C, 1: CTTB
		state->tuner_bandwidth = 8;
		state->tuner_loopthrough = 0;
		state->tuner_crystal = 24000;
		state->tuner_dac = 7200;
		state->tuner_mtt = 0x00;
		state->tuner_custom_cfg = 0;
		state->tuner_version = 30022;	/* Driver version number */
		state->tuner_time = 12092611;
		state->tuner_init_OK = 1;
	}
	_mt_fe_tn_set_reg(state, 0x2b, 0x46);
	_mt_fe_tn_set_reg(state, 0x2c, 0x75);
	if (state->tuner_mtt == 0x00) {
		u8 tmp = 0;
		_mt_fe_tn_get_reg(state, 0x01, &tmp);
		printk(KERN_INFO "m88dc2800: tuner id = 0x%02x ", tmp);
		switch (tmp) {
		case 0x0d:
			state->tuner_mtt = 0xD1;
			break;
		case 0x8e:
		default:
			state->tuner_mtt = 0xE1;
			break;
		}
	}
	return 0;
}

 static int mt_fe_tn_set_freq_tc2800(struct m88dc2800_state *state,
				       u32 freq_KHz)
{
	u8 buf;
	u8 buf1;

	mt_fe_tn_init_tc2800(state);
	state->tuner_freq = freq_KHz;
	_mt_fe_tn_set_reg(state, 0x21, freq_KHz > 500000 ? 0xb9 : 0x99);
	mt_fe_tn_wakeup_tc2800(state);
	 _mt_fe_tn_set_reg(state, 0x05, 0x7f);
	_mt_fe_tn_set_reg(state, 0x06, 0xf8);
	 _mt_fe_tn_set_RF_front_tc2800(state);
	_mt_fe_tn_set_PLL_freq_tc2800(state);
	_mt_fe_tn_set_DAC_tc2800(state);
	_mt_fe_tn_set_BB_tc2800(state);
	_mt_fe_tn_preset_tc2800(state);
	 _mt_fe_tn_set_reg(state, 0x05, 0x00);
	_mt_fe_tn_set_reg(state, 0x06, 0x00);
	 if (state->tuner_mtt == 0xD1) {
		_mt_fe_tn_set_reg(state, 0x00, 0x01);
		_mt_fe_tn_set_reg(state, 0x00, 0x00);
		 msleep(5);
		_mt_fe_tn_set_reg(state, 0x41, 0x00);
		msleep(5);
		_mt_fe_tn_set_reg(state, 0x41, 0x02);

		_mt_fe_tn_get_reg(state, 0x69, &buf1);
		buf1 = buf1 & 0x0f;
		_mt_fe_tn_get_reg(state, 0x61, &buf);
		buf = buf & 0x0f;
		if (buf == 0x0c)
			_mt_fe_tn_set_reg(state, 0x6a, 0x59);
		if (buf1 > 0x02) {
			if (freq_KHz > 600000)
				_mt_fe_tn_set_reg(state, 0x66, 0x44);
			else if (freq_KHz > 500000)
				_mt_fe_tn_set_reg(state, 0x66, 0x64);
			else
				_mt_fe_tn_set_reg(state, 0x66, 0x74);
		}		
		if (buf1 < 0x03) {
			if (freq_KHz > 800000)
				_mt_fe_tn_set_reg(state, 0x87, 0x64);
			else if (freq_KHz > 600000)
				_mt_fe_tn_set_reg(state, 0x87, 0x54);
			else if (freq_KHz > 500000)
				_mt_fe_tn_set_reg(state, 0x87, 0x54);
			else if (freq_KHz > 300000)
				_mt_fe_tn_set_reg(state, 0x87, 0x43);
			else if (freq_KHz > 220000)
				_mt_fe_tn_set_reg(state, 0x87, 0x54);
			else if (freq_KHz > 110000)
				_mt_fe_tn_set_reg(state, 0x87, 0x14);
			else
				_mt_fe_tn_set_reg(state, 0x87, 0x54);
			msleep(5);
		} else if (buf < 0x0c) {
			if (freq_KHz > 800000)
				_mt_fe_tn_set_reg(state, 0x87, 0x14);
			else if (freq_KHz > 600000)
				_mt_fe_tn_set_reg(state, 0x87, 0x14);
			else if (freq_KHz > 500000)
				_mt_fe_tn_set_reg(state, 0x87, 0x34);
			else if (freq_KHz > 300000)
				_mt_fe_tn_set_reg(state, 0x87, 0x43);
			else if (freq_KHz > 220000)
				_mt_fe_tn_set_reg(state, 0x87, 0x54);
			else if (freq_KHz > 110000)
				_mt_fe_tn_set_reg(state, 0x87, 0x14);
			else
				_mt_fe_tn_set_reg(state, 0x87, 0x54);
			msleep(5);
		}
	} else if ((state->tuner_mtt == 0xE1)) {
		_mt_fe_tn_set_reg(state, 0x00, 0x01);
		_mt_fe_tn_set_reg(state, 0x00, 0x00);
		 msleep(20);
		 _mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = (buf & 0xef) | 0x28;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		 msleep(50);
		_mt_fe_tn_get_reg(state, 0x38, &buf);
		_mt_fe_tn_set_reg(state, 0x38, buf);
		_mt_fe_tn_get_reg(state, 0x32, &buf);
		buf = (buf & 0xf7) | 0x10;
		_mt_fe_tn_set_reg(state, 0x32, buf);
		 msleep(10);
		 _mt_fe_tn_get_reg(state, 0x69, &buf);
		buf = buf & 0x03;
		_mt_fe_tn_set_reg(state, 0x2a, buf);
		if (buf > 0) {
			msleep(20);
			_mt_fe_tn_get_reg(state, 0x84, &buf);
			buf = buf & 0x1f;
			_mt_fe_tn_set_reg(state, 0x68, 0x0a);
			_mt_fe_tn_get_reg(state, 0x88, &buf1);
			buf1 = buf1 & 0x1f;
			if (buf <= buf1)
				_mt_fe_tn_set_reg(state, 0x66, 0x44);
			else
				_mt_fe_tn_set_reg(state, 0x66, 0x74);
		} else {
			if (freq_KHz <= 600000)
				_mt_fe_tn_set_reg(state, 0x68, 0x0c);
			else
				_mt_fe_tn_set_reg(state, 0x68, 0x0e);
			_mt_fe_tn_set_reg(state, 0x30, 0xfb);
			_mt_fe_tn_set_reg(state, 0x30, 0xff);
			_mt_fe_tn_set_reg(state, 0x31, 0x04);
			_mt_fe_tn_set_reg(state, 0x31, 0x00);
		}
		if (state->tuner_loopthrough != 0) {
			_mt_fe_tn_get_reg(state, 0x28, &buf);
			if (buf == 0) {
				_mt_fe_tn_set_reg(state, 0x28, 0xff);
				_mt_fe_tn_get_reg(state, 0x61, &buf);
				buf = buf & 0x0f;
				if (buf > 9)
					_mt_fe_tn_set_reg(state, 0x67, 0x74);
				else if (buf > 6)
					_mt_fe_tn_set_reg(state, 0x67, 0x64);
				else if (buf > 3)
					_mt_fe_tn_set_reg(state, 0x67, 0x54);
				else
					_mt_fe_tn_set_reg(state, 0x67, 0x44);
			}
		} else {
			_mt_fe_tn_set_reg(state, 0x67, 0x34);
		}
	} else {
		return 1;
	}
	return 0;
}


/*
static int mt_fe_tn_set_BB_filter_band_tc2800(struct m88dc2800_state *state,
					      u8 bandwidth)
{
	u8 buf, tmp;

	_mt_fe_tn_get_reg(state, 0x53, &tmp);

	if (bandwidth == 6)
		buf = 0x01 << 1;
	else if (bandwidth == 7)
		buf = 0x02 << 1;
	else if (bandwidth == 8)
		buf = 0x04 << 1;
	else
		buf = 0x04 << 1;

	tmp &= 0xf1;
	tmp |= buf;
	_mt_fe_tn_set_reg(state, 0x53, tmp);
	state->tuner_bandwidth = bandwidth;
	return 0;
}
*/

static s32 mt_fe_tn_get_signal_strength_tc2800(struct m88dc2800_state
					       *state)
{
	s32 level = -107;
	s32 tmp1, tmp2, tmp3, tmp4, tmp5, tmp6;
	s32 val1, val2, val;
	s32 result2, result3, result4, result5, result6;
	s32 append;
	u8 tmp;
	s32 freq_KHz = (s32) state->tuner_freq;
	if (state->tuner_mtt == 0xD1) {
		_mt_fe_tn_get_reg(state, 0x61, &tmp);
		tmp1 = tmp & 0x0f;
		 _mt_fe_tn_get_reg(state, 0x69, &tmp);
		tmp2 = tmp & 0x0f;
		 _mt_fe_tn_get_reg(state, 0x73, &tmp);
		tmp3 = tmp & 0x07;
		 _mt_fe_tn_get_reg(state, 0x7c, &tmp);
		tmp4 = (tmp >> 4) & 0x0f;
		 _mt_fe_tn_get_reg(state, 0x7b, &tmp);
		tmp5 = tmp & 0x0f;
		 _mt_fe_tn_get_reg(state, 0x7f, &tmp);
		tmp6 = (tmp >> 5) & 0x01;
		if (tmp1 > 6) {
			val1 = 0;
			if (freq_KHz <= 200000) {
				val2 = (tmp1 - 6) * 267;
			} else if (freq_KHz <= 600000) {
				val2 = (tmp1 - 6) * 280;
			} else {
				val2 = (tmp1 - 6) * 290;
			}
			val = val1 + val2;
		} else {
			if (tmp1 == 0) {
				val1 = -550;
			} else {
				val1 = 0;
			}
			if ((tmp1 < 4) && (freq_KHz >= 506000)) {
				val1 = -850;
			}
			val2 = 0;
			val = val1 + val2;
		}
		if (freq_KHz <= 95000) {
			result2 = tmp2 * 289;
		} else if (freq_KHz <= 155000) {
			result2 = tmp2 * 278;
		} else if (freq_KHz <= 245000) {
			result2 = tmp2 * 267;
		} else if (freq_KHz <= 305000) {
			result2 = tmp2 * 256;
		} else if (freq_KHz <= 335000) {
			result2 = tmp2 * 244;
		} else if (freq_KHz <= 425000) {
			result2 = tmp2 * 233;
		} else if (freq_KHz <= 575000) {
			result2 = tmp2 * 222;
		} else if (freq_KHz <= 665000) {
			result2 = tmp2 * 211;
		} else {
			result2 = tmp2 * 200;
		}
		result3 = (6 - tmp3) * 100;
		result4 = 300 * tmp4;
		result5 = 50 * tmp5;
		result6 = 300 * tmp6;
		if (freq_KHz < 105000) {
			append = -450;
		} else if (freq_KHz <= 227000) {
			append = -4 * (freq_KHz / 1000 - 100) + 150;
		} else if (freq_KHz <= 305000) {
			append = -4 * (freq_KHz / 1000 - 100);
		} else if (freq_KHz <= 419000) {
			append = 500 - 40 * (freq_KHz / 1000 - 300) / 17 + 130;
		} else if (freq_KHz <= 640000) {
			append = 500 - 40 * (freq_KHz / 1000 - 300) / 17;
		} else {
			append = -500;
		}
		level = append - (val + result2 + result3 + result4 +
				  result5 + result6);
		level /= 100;
	} else if (state->tuner_mtt == 0xE1) {
		_mt_fe_tn_get_reg(state, 0x61, &tmp);
		tmp1 = tmp & 0x0f;
		 _mt_fe_tn_get_reg(state, 0x84, &tmp);
		tmp2 = tmp & 0x1f;
		 _mt_fe_tn_get_reg(state, 0x69, &tmp);
		tmp3 = tmp & 0x03;
		 _mt_fe_tn_get_reg(state, 0x73, &tmp);
		tmp4 = tmp & 0x0f;
		 _mt_fe_tn_get_reg(state, 0x7c, &tmp);
		tmp5 = (tmp >> 4) & 0x0f;
		 _mt_fe_tn_get_reg(state, 0x7b, &tmp);
		tmp6 = tmp & 0x0f;
		if (freq_KHz < 151000) {
			result2 = (1150 - freq_KHz / 100) * 163 / 33 + 4230;
			result3 = (1150 - freq_KHz / 100) * 115 / 33 + 1850;
			result4 = -3676 * (freq_KHz / 1000) / 100 + 6115;
		} else if (freq_KHz < 257000) {
			result2 = (1540 - freq_KHz / 100) * 11 / 4 + 3870;
			result3 = (1540 - freq_KHz / 100) * 205 / 96 + 2100;
			result4 = -21 * freq_KHz / 1000 + 5084;
		} else if (freq_KHz < 305000) {
			result2 = (2620 - freq_KHz / 100) * 5 / 3 + 2770;
			result3 = (2620 - freq_KHz / 100) * 10 / 7 + 1700;
			result4 = 650;
		} else if (freq_KHz < 449000) {
			result2 = (307 - freq_KHz / 1000) * 82 / 27 + 11270;
			result3 = (3100 - freq_KHz / 100) * 5 / 3 + 10000;
			result4 = 134 * freq_KHz / 10000 + 11875;
		} else {
			result2 = (307 - freq_KHz / 1000) * 82 / 27 + 11270;
			result3 = 8400;
			result4 = 5300;
		}
		if (tmp1 > 6) {
			val1 = result2;
			val2 = 2900;
			val = 500;
		} else if (tmp1 > 0) {
			val1 = result3;
			val2 = 2700;
			val = 500;
		} else {
			val1 = result4;
			val2 = 2700;
			val = 400;
		}
		level = val1 - (val2 * tmp1 + 500 * tmp2 + 3000 * tmp3 -
			    500 * tmp4 + 3000 * tmp5 + val * tmp6) - 1000;
		level /= 1000;
	}
	return level;
}


/* m88dc2800 operation functions */
u8 M88DC2000GetLock(struct m88dc2800_state * state)
{
	u8 u8ret = 0;
	if (ReadReg(state, 0x80) < 0x06) {
		if ((ReadReg(state, 0xdf) & 0x80) == 0x80
		     &&(ReadReg(state, 0x91) & 0x23) == 0x03
		     &&(ReadReg(state, 0x43) & 0x08) == 0x08)
			u8ret = 1;
		else
			u8ret = 0;
	} else {
		if ((ReadReg(state, 0x85) & 0x08) == 0x08)
			u8ret = 1;
		else
			u8ret = 0;
	}
	dprintk("%s, lock=%d\n", __func__, u8ret);
	return u8ret;
}

static int M88DC2000SetTsType(struct m88dc2800_state *state, u8 type)
{
	u8 regC2H;

	if (type == 3) {
		WriteReg(state, 0x84, 0x6A);
		WriteReg(state, 0xC0, 0x43);
		WriteReg(state, 0xE2, 0x06);
		regC2H = ReadReg(state, 0xC2);
		regC2H &= 0xC0;
		regC2H |= 0x1B;
		WriteReg(state, 0xC2, regC2H);
		WriteReg(state, 0xC1, 0x60);	/* common interface */
	} else if (type == 1) {
		WriteReg(state, 0x84, 0x6A);
		WriteReg(state, 0xC0, 0x47);	/* serial format */
		WriteReg(state, 0xE2, 0x02);
		regC2H = ReadReg(state, 0xC2);
		regC2H &= 0xC7;
		WriteReg(state, 0xC2, regC2H);
		WriteReg(state, 0xC1, 0x00);
	} else {
		WriteReg(state, 0x84, 0x6C);
		WriteReg(state, 0xC0, 0x43);	/* parallel format */
		WriteReg(state, 0xE2, 0x06);
		regC2H = ReadReg(state, 0xC2);
		regC2H &= 0xC7;
		WriteReg(state, 0xC2, regC2H);
		WriteReg(state, 0xC1, 0x00);
	}
	return 0;
}

static int M88DC2000RegInitial_TC2800(struct m88dc2800_state *state)
{
	u8 RegE3H, RegE4H;

	WriteReg(state, 0x00, 0x48);
	WriteReg(state, 0x01, 0x09);
	WriteReg(state, 0xFB, 0x0A);
	WriteReg(state, 0xFC, 0x0B);
	WriteReg(state, 0x02, 0x0B);
	WriteReg(state, 0x03, 0x18);
	WriteReg(state, 0x05, 0x0D);
	WriteReg(state, 0x36, 0x80);
	WriteReg(state, 0x43, 0x40);
	WriteReg(state, 0x55, 0x7A);
	WriteReg(state, 0x56, 0xD9);
	WriteReg(state, 0x57, 0xDF);
	WriteReg(state, 0x58, 0x39);
	WriteReg(state, 0x5A, 0x00);
	WriteReg(state, 0x5C, 0x71);
	WriteReg(state, 0x5D, 0x23);
	WriteReg(state, 0x86, 0x40);
	WriteReg(state, 0xF9, 0x08);
	WriteReg(state, 0x61, 0x40);
	WriteReg(state, 0x62, 0x0A);
	WriteReg(state, 0x90, 0x06);
	WriteReg(state, 0xDE, 0x00);
	WriteReg(state, 0xA0, 0x03);
	WriteReg(state, 0xDF, 0x81);
	WriteReg(state, 0xFA, 0x40);
	WriteReg(state, 0x37, 0x10);
	WriteReg(state, 0xF0, 0x40);
	WriteReg(state, 0xF2, 0x9C);
	WriteReg(state, 0xF3, 0x40);
	RegE3H = ReadReg(state, 0xE3);
	RegE4H = ReadReg(state, 0xE4);
	if (((RegE3H & 0xC0) == 0x00) && ((RegE4H & 0xC0) == 0x00)) {
		WriteReg(state, 0x30, 0xFF);
		WriteReg(state, 0x31, 0x00);
		WriteReg(state, 0x32, 0x00);
		WriteReg(state, 0x33, 0x00);
		WriteReg(state, 0x35, 0x32);
		WriteReg(state, 0x40, 0x00);
		WriteReg(state, 0x41, 0x10);
		WriteReg(state, 0xF1, 0x02);
		WriteReg(state, 0xF4, 0x04);
		WriteReg(state, 0xF5, 0x00);
		WriteReg(state, 0x42, 0x14);
		WriteReg(state, 0xE1, 0x25);
	} else if (((RegE3H & 0xC0) == 0x80) && ((RegE4H & 0xC0) == 0x40)) {
		WriteReg(state, 0x30, 0xFF);
		WriteReg(state, 0x31, 0x00);
		WriteReg(state, 0x32, 0x00);
		WriteReg(state, 0x33, 0x00);
		WriteReg(state, 0x35, 0x32);
		WriteReg(state, 0x39, 0x00);
		WriteReg(state, 0x3A, 0x00);
		WriteReg(state, 0x40, 0x00);
		WriteReg(state, 0x41, 0x10);
		WriteReg(state, 0xF1, 0x00);
		WriteReg(state, 0xF4, 0x00);
		WriteReg(state, 0xF5, 0x40);
		WriteReg(state, 0x42, 0x14);
		WriteReg(state, 0xE1, 0x25);
	} else if ((RegE3H == 0x80 || RegE3H == 0x81)
		    && (RegE4H == 0x80 || RegE4H == 0x81)) {
		WriteReg(state, 0x30, 0xFF);
		WriteReg(state, 0x31, 0x00);
		WriteReg(state, 0x32, 0x00);
		WriteReg(state, 0x33, 0x00);
		WriteReg(state, 0x35, 0x32);
		WriteReg(state, 0x39, 0x00);
		WriteReg(state, 0x3A, 0x00);
		WriteReg(state, 0xF1, 0x00);
		WriteReg(state, 0xF4, 0x00);
		WriteReg(state, 0xF5, 0x40);
		WriteReg(state, 0x42, 0x24);
		WriteReg(state, 0xE1, 0x25);
		WriteReg(state, 0x92, 0x7F);
		WriteReg(state, 0x93, 0x91);
		WriteReg(state, 0x95, 0x00);
		WriteReg(state, 0x2B, 0x33);
		WriteReg(state, 0x2A, 0x2A);
		WriteReg(state, 0x2E, 0x80);
		WriteReg(state, 0x25, 0x25);
		WriteReg(state, 0x2D, 0xFF);
		WriteReg(state, 0x26, 0xFF);
		WriteReg(state, 0x27, 0x00);
		WriteReg(state, 0x24, 0x25);
		WriteReg(state, 0xA4, 0xFF);
		WriteReg(state, 0xA3, 0x0D);
	} else {
		WriteReg(state, 0x30, 0xFF);
		WriteReg(state, 0x31, 0x00);
		WriteReg(state, 0x32, 0x00);
		WriteReg(state, 0x33, 0x00);
		WriteReg(state, 0x35, 0x32);
		WriteReg(state, 0x39, 0x00);
		WriteReg(state, 0x3A, 0x00);
		WriteReg(state, 0xF1, 0x00);
		WriteReg(state, 0xF4, 0x00);
		WriteReg(state, 0xF5, 0x40);
		WriteReg(state, 0x42, 0x24);
		WriteReg(state, 0xE1, 0x27);
		WriteReg(state, 0x92, 0x7F);
		WriteReg(state, 0x93, 0x91);
		WriteReg(state, 0x95, 0x00);
		WriteReg(state, 0x2B, 0x33);
		WriteReg(state, 0x2A, 0x2A);
		WriteReg(state, 0x2E, 0x80);
		WriteReg(state, 0x25, 0x25);
		WriteReg(state, 0x2D, 0xFF);
		WriteReg(state, 0x26, 0xFF);
		WriteReg(state, 0x27, 0x00);
		WriteReg(state, 0x24, 0x25);
		WriteReg(state, 0xA4, 0xFF);
		WriteReg(state, 0xA3, 0x10);
	}
	WriteReg(state, 0xF6, 0x4E);
	WriteReg(state, 0xF7, 0x20);
	WriteReg(state, 0x89, 0x02);
	WriteReg(state, 0x14, 0x08);
	WriteReg(state, 0x6F, 0x0D);
	WriteReg(state, 0x10, 0xFF);
	WriteReg(state, 0x11, 0x00);
	WriteReg(state, 0x12, 0x30);
	WriteReg(state, 0x13, 0x23);
	WriteReg(state, 0x60, 0x00);
	WriteReg(state, 0x69, 0x00);
	WriteReg(state, 0x6A, 0x03);
	WriteReg(state, 0xE0, 0x75);
	WriteReg(state, 0x8D, 0x29);
	WriteReg(state, 0x4E, 0xD8);
	WriteReg(state, 0x88, 0x80);
	WriteReg(state, 0x52, 0x79);
	WriteReg(state, 0x53, 0x03);
	WriteReg(state, 0x59, 0x30);
	WriteReg(state, 0x5E, 0x02);
	WriteReg(state, 0x5F, 0x0F);
	WriteReg(state, 0x71, 0x03);
	WriteReg(state, 0x72, 0x12);
	WriteReg(state, 0x73, 0x12);

	return 0;
}

static int M88DC2000AutoTSClock_P(struct m88dc2800_state *state, u32 sym,
				  u16 qam)
{
	u32 dataRate;
	u8 clk_div, value;
	printk(KERN_INFO
	       "m88dc2800: M88DC2000AutoTSClock_P, symrate=%d qam=%d\n",
	       sym, qam);
	switch (qam) {
	case 16:
		dataRate = 4;
		break;
	case 32:
		dataRate = 5;
		break;
	case 128:
		dataRate = 7;
		break;
	case 256:
		dataRate = 8;
		break;
	case 64:
	default:
		dataRate = 6;
		break;
	}
	dataRate *= sym * 105;
	dataRate /= 800;
	if (dataRate <= 4115)
		clk_div = 0x05;
	else if (dataRate <= 4800)
		clk_div = 0x04;
	else if (dataRate <= 5760)
		clk_div = 0x03;
	else if (dataRate <= 7200)
		clk_div = 0x02;
	else if (dataRate <= 9600)
		clk_div = 0x01;
	else
		clk_div = 0x00;
	value = ReadReg(state, 0xC2);
	value &= 0xc0;
	value |= clk_div;
	WriteReg(state, 0xC2, value);
	return 0;
}

static int M88DC2000AutoTSClock_C(struct m88dc2800_state *state, u32 sym,
				  u16 qam)
{
	u32 dataRate;
	u8 clk_div, value;
	printk(KERN_INFO
	       "m88dc2800: M88DC2000AutoTSClock_C, symrate=%d qam=%d\n",
	       sym, qam);
	switch (qam) {
	case 16:
		dataRate = 4;
		break;
	case 32:
		dataRate = 5;
		break;
	case 128:
		dataRate = 7;
		break;
	case 256:
		dataRate = 8;
		break;
	case 64:
	default:
		dataRate = 6;
		break;
	}
	dataRate *= sym * 105;
	dataRate /= 800;
	if (dataRate <= 4115)
		clk_div = 0x3F;
	else if (dataRate <= 4800)
		clk_div = 0x36;
	else if (dataRate <= 5760)
		clk_div = 0x2D;
	else if (dataRate <= 7200)
		clk_div = 0x24;
	else if (dataRate <= 9600)
		clk_div = 0x1B;
	else
		clk_div = 0x12;
	value = ReadReg(state, 0xC2);
	value &= 0xc0;
	value |= clk_div;
	WriteReg(state, 0xC2, value);
	return 0;
}

static int M88DC2000SetTxMode(struct m88dc2800_state *state, u8 inverted,
			      u8 j83)
{
	u8 value = 0;
	if (inverted)
		value |= 0x08;	/*	spectrum inverted	*/
	if (j83)
		value |= 0x01;	/*	J83C			*/
	WriteReg(state, 0x83, value);
	return 0;
}

static int M88DC2000SoftReset(struct m88dc2800_state *state)
{
	WriteReg(state, 0x80, 0x01);
	WriteReg(state, 0x82, 0x00);
	msleep(1);
	WriteReg(state, 0x80, 0x00);
	return 0;
}

static int M88DC2000SetSym(struct m88dc2800_state *state, u32 sym, u32 xtal)
{
	u8 value;
	u8 reg6FH, reg12H;
	u64 fValue;
	u32 dwValue;

	printk(KERN_INFO "%s, sym=%d, xtal=%d\n", __func__, sym, xtal);
	fValue = 4294967296 * (sym + 10);
	do_div(fValue, xtal);

	/* fValue  = 4294967296 * (sym + 10) / xtal; */
	dwValue = (u32) fValue;
	printk(KERN_INFO "%s, fvalue1=%x\n", __func__, dwValue);
	WriteReg(state, 0x58, (u8) ((dwValue >> 24) & 0xff));
	WriteReg(state, 0x57, (u8) ((dwValue >> 16) & 0xff));
	WriteReg(state, 0x56, (u8) ((dwValue >> 8) & 0xff));
	WriteReg(state, 0x55, (u8) ((dwValue >> 0) & 0xff));

	/* fValue = 2048 * xtal / sym; */
	fValue = 2048 * xtal;
	do_div(fValue, sym);
	dwValue = (u32) fValue;
	printk(KERN_INFO "%s, fvalue2=%x\n", __func__, dwValue);
	WriteReg(state, 0x5D, (u8) ((dwValue >> 8) & 0xff));
	WriteReg(state, 0x5C, (u8) ((dwValue >> 0) & 0xff));
	value = ReadReg(state, 0x5A);
	if (((dwValue >> 16) & 0x0001) == 0)
		value &= 0x7F;
	else
		value |= 0x80;
	WriteReg(state, 0x5A, value);
	value = ReadReg(state, 0x89);
	if (sym <= 1800)
		value |= 0x01;
	else
		value &= 0xFE;
	WriteReg(state, 0x89, value);
	if (sym >= 6700) {
		reg6FH = 0x0D;
		reg12H = 0x30;
	} else if (sym >= 4000) {
		fValue = 22 * 4096 / sym;
		reg6FH = (u8) fValue;
		reg12H = 0x30;
	} else if (sym >= 2000) {
		fValue = 14 * 4096 / sym;
		reg6FH = (u8) fValue;
		reg12H = 0x20;
	} else {
		fValue = 7 * 4096 / sym;
		reg6FH = (u8) fValue;
		reg12H = 0x10;
	}
	WriteReg(state, 0x6F, reg6FH);
	WriteReg(state, 0x12, reg12H);
	if (((ReadReg(state, 0xE3) & 0x80) == 0x80)
	       && ((ReadReg(state, 0xE4) & 0x80) == 0x80)) {
		if (sym < 3000) {
			WriteReg(state, 0x6C, 0x16);
			WriteReg(state, 0x6D, 0x10);
			WriteReg(state, 0x6E, 0x18);
		} else {
			WriteReg(state, 0x6C, 0x14);
			WriteReg(state, 0x6D, 0x0E);
			WriteReg(state, 0x6E, 0x36);
		}
	} else {
		WriteReg(state, 0x6C, 0x16);
		WriteReg(state, 0x6D, 0x10);
		WriteReg(state, 0x6E, 0x18);
	}
	return 0;
}

static int M88DC2000SetQAM(struct m88dc2800_state *state, u16 qam)
{
	u8 reg00H, reg4AH, regC2H, reg44H, reg4CH, reg4DH, reg74H, value;
	u8 reg8BH, reg8EH;
	printk(KERN_INFO "%s, qam=%d\n", __func__, qam);
	regC2H = ReadReg(state, 0xC2);
	regC2H &= 0xF8;
	switch (qam) {
	case 16:		/* 16 QAM */
		reg00H = 0x08;
		reg4AH = 0x0F;
		regC2H |= 0x02;
		reg44H = 0xAA;
		reg4CH = 0x0C;
		reg4DH = 0xF7;
		reg74H = 0x0E;
		if (((ReadReg(state, 0xE3) & 0x80) == 0x80)
		     && ((ReadReg(state, 0xE4) & 0x80) == 0x80)) {
			reg8BH = 0x5A;
			reg8EH = 0xBD;
		} else {
			reg8BH = 0x5B;
			reg8EH = 0x9D;
		}
		WriteReg(state, 0x6E, 0x18);
		break;
	case 32:		/* 32 QAM */
		reg00H = 0x18;
		reg4AH = 0xFB;
		regC2H |= 0x02;
		reg44H = 0xAA;
		reg4CH = 0x0C;
		reg4DH = 0xF7;
		reg74H = 0x0E;
		if (((ReadReg(state, 0xE3) & 0x80) == 0x80)
		     && ((ReadReg(state, 0xE4) & 0x80) == 0x80)) {
			reg8BH = 0x5A;
			reg8EH = 0xBD;
		} else {
			reg8BH = 0x5B;
			reg8EH = 0x9D;
		}
		WriteReg(state, 0x6E, 0x18);
		break;
	case 64:		/* 64 QAM */
		reg00H = 0x48;
		reg4AH = 0xCD;
		regC2H |= 0x02;
		reg44H = 0xAA;
		reg4CH = 0x0C;
		reg4DH = 0xF7;
		reg74H = 0x0E;
		if (((ReadReg(state, 0xE3) & 0x80) == 0x80)
		     && ((ReadReg(state, 0xE4) & 0x80) == 0x80)) {
			reg8BH = 0x5A;
			reg8EH = 0xBD;
		} else {
			reg8BH = 0x5B;
			reg8EH = 0x9D;
		}
		break;
	case 128:		/* 128 QAM */
		reg00H = 0x28;
		reg4AH = 0xFF;
		regC2H |= 0x02;
		reg44H = 0xA9;
		reg4CH = 0x08;
		reg4DH = 0xF5;
		reg74H = 0x0E;
		reg8BH = 0x5B;
		reg8EH = 0x9D;
		break;
	case 256:		/* 256 QAM */
		reg00H = 0x38;
		reg4AH = 0xCD;
		if (((ReadReg(state, 0xE3) & 0x80) == 0x80)
		     && ((ReadReg(state, 0xE4) & 0x80) == 0x80)) {
			regC2H |= 0x02;
		} else {
			regC2H |= 0x01;
		}
		reg44H = 0xA9;
		reg4CH = 0x08;
		reg4DH = 0xF5;
		reg74H = 0x0E;
		reg8BH = 0x5B;
		reg8EH = 0x9D;
		break;
	default:		/* 64 QAM */
		reg00H = 0x48;
		reg4AH = 0xCD;
		regC2H |= 0x02;
		reg44H = 0xAA;
		reg4CH = 0x0C;
		reg4DH = 0xF7;
		reg74H = 0x0E;
		if (((ReadReg(state, 0xE3) & 0x80) == 0x80)
		     && ((ReadReg(state, 0xE4) & 0x80) == 0x80)) {
			reg8BH = 0x5A;
			reg8EH = 0xBD;
		} else {
			reg8BH = 0x5B;
			reg8EH = 0x9D;
		}
		break;
	}
	WriteReg(state, 0x00, reg00H);
	value = ReadReg(state, 0x88);
	value |= 0x08;
	WriteReg(state, 0x88, value);
	WriteReg(state, 0x4B, 0xFF);
	WriteReg(state, 0x4A, reg4AH);
	value &= 0xF7;
	WriteReg(state, 0x88, value);
	WriteReg(state, 0xC2, regC2H);
	WriteReg(state, 0x44, reg44H);
	WriteReg(state, 0x4C, reg4CH);
	WriteReg(state, 0x4D, reg4DH);
	WriteReg(state, 0x74, reg74H);
	WriteReg(state, 0x8B, reg8BH);
	WriteReg(state, 0x8E, reg8EH);
	return 0;
}

static int M88DC2000WriteTuner_TC2800(struct m88dc2800_state *state,
				      u32 freq_KHz)
{
	printk(KERN_INFO "%s, freq=%d KHz\n", __func__, freq_KHz);
	return mt_fe_tn_set_freq_tc2800(state, freq_KHz);
}

static int m88dc2800_init(struct dvb_frontend *fe)
{
	dprintk("%s()\n", __func__);
	return 0;
}

static int m88dc2800_set_parameters(struct dvb_frontend *fe)
{
	struct dtv_frontend_properties *c = &fe->dtv_property_cache;
	u8 is_annex_c, is_update;
	u16 temp_qam;
	s32 waiting_time;
	struct m88dc2800_state *state = fe->demodulator_priv;

	is_annex_c = c->delivery_system == SYS_DVBC_ANNEX_C ? 1 : 0;

	switch (c->modulation) {
	case QAM_16:
		temp_qam = 16;
		break;
	case QAM_32:
		temp_qam = 32;
		break;
	case QAM_128:
		temp_qam = 128;
		break;
	case QAM_256:
		temp_qam = 256;
		break;
	default:		/* QAM_64 */
		temp_qam = 64;
		break;
	}

	state->inverted = c->inversion == INVERSION_ON ? 1 : 0;

	printk(KERN_INFO
	     "m88dc2800: state, freq=%d qam=%d sym=%d inverted=%d xtal=%d\n",
	     state->freq, state->qam, state->sym, state->inverted,
	     state->xtal);
	printk(KERN_INFO
	     "m88dc2800: set frequency to %d qam=%d symrate=%d annex-c=%d\n",
	     c->frequency, temp_qam, c->symbol_rate, is_annex_c);

	is_update = 0;
	WriteReg(state, 0x80, 0x01);
	if (c->frequency != state->freq) {
		M88DC2000WriteTuner_TC2800(state, c->frequency / 1000);
		state->freq = c->frequency;
	}
	if (c->symbol_rate != state->sym) {
		M88DC2000SetSym(state, c->symbol_rate / 1000, state->xtal);
		state->sym = c->symbol_rate;
		is_update = 1;
	}
	if (temp_qam != state->qam) {
		M88DC2000SetQAM(state, temp_qam);
		state->qam = temp_qam;
		is_update = 1;
	}

	if (is_update != 0) {
		if (state->config->ts_mode == 3)
			M88DC2000AutoTSClock_C(state, state->sym / 1000,
					       temp_qam);
		else
			M88DC2000AutoTSClock_P(state, state->sym / 1000,
					       temp_qam);
	}

	M88DC2000SetTxMode(state, state->inverted, is_annex_c);
	M88DC2000SoftReset(state);
	if (((ReadReg(state, 0xE3) & 0x80) == 0x80)
	    && ((ReadReg(state, 0xE4) & 0x80) == 0x80))
		waiting_time = 800;
	else
		waiting_time = 500;
	while (waiting_time > 0) {
		msleep(50);
		waiting_time -= 50;
		if (M88DC2000GetLock(state))
			return 0;
	}

	state->inverted = (state->inverted != 0) ? 0 : 1;
	M88DC2000SetTxMode(state, state->inverted, is_annex_c);
	M88DC2000SoftReset(state);
	if (((ReadReg(state, 0xE3) & 0x80) == 0x80) &&
	    ((ReadReg(state, 0xE4) & 0x80) == 0x80))
		waiting_time = 800;
	else
		waiting_time = 500;
	while (waiting_time > 0) {
		msleep(50);
		waiting_time -= 50;
		if (M88DC2000GetLock(state))
			return 0;
	}
	return 0;
}

static int m88dc2800_read_status(struct dvb_frontend *fe,
				 fe_status_t * status)
{
	struct m88dc2800_state *state = fe->demodulator_priv;
	*status = 0;

	if (M88DC2000GetLock(state)) {
		*status = FE_HAS_SIGNAL | FE_HAS_CARRIER
		    |FE_HAS_SYNC | FE_HAS_VITERBI | FE_HAS_LOCK;
	}
	return 0;
}

static int m88dc2800_read_ber(struct dvb_frontend *fe, u32 * ber)
{
	struct m88dc2800_state *state = fe->demodulator_priv;
	u16 tmp;

	if (M88DC2000GetLock(state) == 0) {
		state->ber = 0;
	} else if ((ReadReg(state, 0xA0) & 0x80) != 0x80) {
		tmp = ReadReg(state, 0xA2) << 8;
		tmp += ReadReg(state, 0xA1);
		state->ber = tmp;
		WriteReg(state, 0xA0, 0x05);
		WriteReg(state, 0xA0, 0x85);
	}
	*ber = state->ber;
	return 0;
}

static int m88dc2800_read_signal_strength(struct dvb_frontend *fe,
					  u16 * strength)
{
	struct m88dc2800_state *state = fe->demodulator_priv;
	s16 tuner_strength;

	tuner_strength = mt_fe_tn_get_signal_strength_tc2800(state);
	*strength = tuner_strength < -107 ? 0 : tuner_strength + 107;

	return 0;
}

static int m88dc2800_read_snr(struct dvb_frontend *fe, u16 * snr)
{
	static const u32 mes_log[] = {
		0, 3010, 4771, 6021, 6990, 7781, 8451, 9031, 9542, 10000,
		10414, 10792, 11139, 11461, 11761, 12041, 12304, 12553, 12788,
		13010, 13222, 13424, 13617, 13802, 13979, 14150, 14314, 14472,
		14624, 14771, 14914, 15052, 15185, 15315, 15441, 15563, 15682,
		15798, 15911, 16021, 16128, 16232, 16335, 16435, 16532, 16628,
		16721, 16812, 16902, 16990, 17076, 17160, 17243, 17324, 17404,
		17482, 17559, 17634, 17709, 17782, 17853, 17924, 17993, 18062,
		18129, 18195, 18261, 18325, 18388, 18451, 18513, 18573, 18633,
		18692, 18751, 18808, 18865, 18921, 18976, 19031
	};
	struct m88dc2800_state *state = fe->demodulator_priv;
	u8 i;
	u32 _snr, mse;

	if ((ReadReg(state, 0x91) & 0x23) != 0x03) {
		*snr = 0;
		return 0;
	}
	mse = 0;
	for (i = 0; i < 30; i++) {
		mse += (ReadReg(state, 0x08) << 8) + ReadReg(state, 0x07);
	}
	mse /= 30;
	if (mse > 80)
		mse = 80;
	switch (state->qam) {
	case 16:
		_snr = 34080;
		break;		/*      16QAM                           */
	case 32:
		_snr = 37600;
		break;		/*      32QAM                           */
	case 64:
		_snr = 40310;
		break;		/*      64QAM                           */
	case 128:
		_snr = 43720;
		break;		/*      128QAM                          */
	case 256:
		_snr = 46390;
		break;		/*      256QAM                          */
	default:
		_snr = 40310;
		break;
	}
	_snr -= mes_log[mse - 1];	/*      C - 10*log10(MSE)       */
	_snr /= 1000;
	if (_snr > 0xff)
		_snr = 0xff;
	*snr = _snr;
	return 0;
}

static int m88dc2800_read_ucblocks(struct dvb_frontend *fe, u32 * ucblocks)
{
	struct m88dc2800_state *state = fe->demodulator_priv;
	u8 u8Value;

	u8Value = ReadReg(state, 0xdf);
	u8Value |= 0x02;	/* Hold */
	WriteReg(state, 0xdf, u8Value);

	*ucblocks = ReadReg(state, 0xd5);
	*ucblocks = (*ucblocks << 8) | ReadReg(state, 0xd4);

	u8Value &= 0xfe;	/* Clear */
	WriteReg(state, 0xdf, u8Value);
	u8Value &= 0xfc;	/* Update */
	u8Value |= 0x01;
	WriteReg(state, 0xdf, u8Value);

	return 0;
}

static int m88dc2800_sleep(struct dvb_frontend *fe)
{
	struct m88dc2800_state *state = fe->demodulator_priv;

	mt_fe_tn_sleep_tc2800(state);
	state->freq = 0;

	return 0;
}

static void m88dc2800_release(struct dvb_frontend *fe)
{
	struct m88dc2800_state *state = fe->demodulator_priv;
	kfree(state);
}

static struct dvb_frontend_ops m88dc2800_ops;

struct dvb_frontend *m88dc2800_attach(const struct m88dc2800_config
				      *config, struct i2c_adapter *i2c)
{
	struct m88dc2800_state *state = NULL;

	/* allocate memory for the internal state */
	state = kzalloc(sizeof(struct m88dc2800_state), GFP_KERNEL);
	if (state == NULL)
		goto error;

	/* setup the state */
	state->config = config;
	state->i2c = i2c;
	state->xtal = 28800;

	WriteReg(state, 0x80, 0x01);
	M88DC2000RegInitial_TC2800(state);
	M88DC2000SetTsType(state, state->config->ts_mode);
	mt_fe_tn_init_tc2800(state);

	/* create dvb_frontend */
	memcpy(&state->frontend.ops, &m88dc2800_ops,
	       sizeof(struct dvb_frontend_ops));
	state->frontend.demodulator_priv = state;
	return &state->frontend;

      error:
	kfree(state);
	return NULL;
}

EXPORT_SYMBOL(m88dc2800_attach);

static struct dvb_frontend_ops m88dc2800_ops = {
	.delsys = {SYS_DVBC_ANNEX_A, SYS_DVBC_ANNEX_C},
	.info = {
		 .name = "Montage M88DC2800 DVB-C",
		 .frequency_stepsize = 62500,
		 .frequency_min = 48000000,
		 .frequency_max = 870000000,
		 .symbol_rate_min = 870000,
		 .symbol_rate_max = 9000000,
		 .caps = FE_CAN_QAM_16 | FE_CAN_QAM_32 | FE_CAN_QAM_64 |
			 FE_CAN_QAM_128 | FE_CAN_QAM_256 | FE_CAN_FEC_AUTO
	},
	.release = m88dc2800_release,
	.init = m88dc2800_init,
	.sleep = m88dc2800_sleep,
	.set_frontend = m88dc2800_set_parameters,
	.read_status = m88dc2800_read_status,
	.read_ber = m88dc2800_read_ber,
	.read_signal_strength = m88dc2800_read_signal_strength,
	.read_snr = m88dc2800_read_snr,
	.read_ucblocks = m88dc2800_read_ucblocks,
};

MODULE_DESCRIPTION("Montage DVB-C demodulator driver");
MODULE_AUTHOR("Max Nibble <nibble.max@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.00");
