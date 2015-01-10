/*
 * Driver for DVBSky USB2.0 receiver
 *
 * Copyright (C) 2013 Max nibble <nibble.max@gmail.com>
 *
 * CIMax code is copied and modified from:
 * CIMax2(R) SP2 driver in conjunction with NetUp Dual DVB-S2 CI card
 * Copyright (C) 2009 NetUP Inc.
 * Copyright (C) 2009 Igor M. Liplianin <liplianin@netup.ru>
 * Copyright (C) 2009 Abylay Ospan <aospan@netup.ru>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "dvb_ca_en50221.h"
#include "dvb_usb.h"
#include "m88ds3103.h"

static int dvbsky_debug;
module_param(dvbsky_debug, int, 0644);
MODULE_PARM_DESC(dvbsky_debug, "Activates dvbsky usb debugging (default:0)");

#define DVBSKY_CI_CTL		0x04
#define DVBSKY_CI_RD		1

#define dprintk(args...) \
	do { \
		if (dvbsky_debug) \
			printk(KERN_INFO "dvbsky_usb: " args); \
	} while (0)

DVB_DEFINE_MOD_OPT_ADAPTER_NR(adapter_nr);

struct dvbsky_state {
	struct mutex stream_mutex;
	u8 has_ci;
	u8 ci_attached;
	struct dvb_ca_en50221 ci;
	unsigned long next_status_checked_time;
	u8 ci_i2c_addr;
	u8 current_ci_flag;
	int ci_status;
};

static int dvbsky_stream_ctrl(struct dvb_usb_device *d, u8 onoff)
{
	struct dvbsky_state *state = d_to_priv(d);
	int ret;
	u8 obuf_pre[3] = { 0x37, 0, 0 };
	u8 obuf_post[3] = { 0x36, 3, 0 };
	dprintk("%s() -off \n", __func__);
	mutex_lock(&state->stream_mutex);
	ret = dvb_usbv2_generic_write(d, obuf_pre, 3);
	if (!ret && onoff) {
		msleep(10);
		ret = dvb_usbv2_generic_write(d, obuf_post, 3);
		dprintk("%s() -on \n", __func__);
	}
	mutex_unlock(&state->stream_mutex);
	return ret;
}

/* CI opertaions */
static int dvbsky_ci_read_i2c(struct i2c_adapter *i2c_adap, u8 addr, u8 reg,
						u8 *buf, int len)
{
	int ret;
	struct i2c_msg msg[] = {
		{
			.addr	= addr,
			.flags	= 0,
			.buf	= &reg,
			.len	= 1
		}, {
			.addr	= addr,
			.flags	= I2C_M_RD,
			.buf	= buf,
			.len	= len
		}
	};
	
	ret = i2c_transfer(i2c_adap, msg, 2);
	
	if (ret != 2) {
		dprintk("%s: error, Reg = 0x%02x, Status = %d\n", __func__, reg, ret);
		return -1;
	}
	return 0;
}

static int dvbsky_ci_write_i2c(struct i2c_adapter *i2c_adap, u8 addr, u8 reg,
						u8 *buf, int len)
{
	int ret;
	u8 buffer[len + 1];

	struct i2c_msg msg = {
		.addr	= addr,
		.flags	= 0,
		.buf	= &buffer[0],
		.len	= len + 1
	};

	buffer[0] = reg;
	memcpy(&buffer[1], buf, len);

	ret = i2c_transfer(i2c_adap, &msg, 1);

	if (ret != 1) {
		dprintk("%s: error, Reg=[0x%02x], Status=%d\n", __func__, reg, ret);
		return -1;
	}
	return 0;
}

static int dvbsky_ci_op_cam(struct dvb_ca_en50221 *ci, int slot,
				u8 flag, u8 read, int addr, u8 data)
{
	struct dvb_usb_device *d = ci->data;
	struct dvbsky_state *state = d_to_priv(d);	
	u8 store;
	int ret;
	u8 command[4], respond[2], command_size, respond_size;

	/*dprintk("%s()\n", __func__);*/
	if (0 != slot)
		return -EINVAL;

	if (state->current_ci_flag != flag) {
		ret = dvbsky_ci_read_i2c(&d->i2c_adap, state->ci_i2c_addr,
				0, &store, 1);
		if (ret != 0)
			return ret;

		store &= ~0x0c;
		store |= flag;

		ret = dvbsky_ci_write_i2c(&d->i2c_adap, state->ci_i2c_addr,
				0, &store, 1);
		if (ret != 0)
			return ret;
	}
	state->current_ci_flag = flag;

	command[1] = (u8)((addr >> 8) & 0xff); /*high part of address*/
	command[2] = (u8)(addr & 0xff); /*low part of address*/
	if (read) {
		command[0] = 0x71;
		command_size = 3;
		respond_size = 2;
	} else {
		command[0] = 0x70;
		command[3] = data;
		command_size = 4;
		respond_size = 1;
	}
	ret = dvb_usbv2_generic_rw(d, command, command_size, respond, respond_size);

	return (read) ? respond[1] : 0;
}

static int dvbsky_ci_read_attribute_mem(struct dvb_ca_en50221 *ci,
						int slot, int addr)
{
	return dvbsky_ci_op_cam(ci, slot, 0, DVBSKY_CI_RD, addr, 0);
}

static int dvbsky_ci_write_attribute_mem(struct dvb_ca_en50221 *ci,
						int slot, int addr, u8 data)
{
	return dvbsky_ci_op_cam(ci, slot, 0, 0, addr, data);
}

static int dvbsky_ci_read_cam_ctl(struct dvb_ca_en50221 *ci, int slot, u8 addr)
{
	return dvbsky_ci_op_cam(ci, slot, DVBSKY_CI_CTL, DVBSKY_CI_RD, addr, 0);
}

static int dvbsky_ci_write_cam_ctl(struct dvb_ca_en50221 *ci, int slot,
							u8 addr, u8 data)
{
	return dvbsky_ci_op_cam(ci, slot, DVBSKY_CI_CTL, 0, addr, data);
}

static int dvbsky_ci_slot_reset(struct dvb_ca_en50221 *ci, int slot)
{
	struct dvb_usb_device *d = ci->data;
	struct dvbsky_state *state = d_to_priv(d);
	u8 buf =  0x80;
	int ret;
	dprintk("%s() slot=%d\n", __func__, slot);

	if (0 != slot)
		return -EINVAL;

	udelay(500);
	ret = dvbsky_ci_write_i2c(&d->i2c_adap, state->ci_i2c_addr,
							0, &buf, 1);

	if (ret != 0)
		return ret;

	udelay(500);

	buf = 0x00;
	ret = dvbsky_ci_write_i2c(&d->i2c_adap, state->ci_i2c_addr,
							0, &buf, 1);
	msleep(1000);
	dprintk("%s() slot=%d complete\n", __func__, slot);
	return 0;

}

static int dvbsky_ci_slot_shutdown(struct dvb_ca_en50221 *ci, int slot)
{
	/* not implemented */
	dprintk("%s()\n", __func__);
	return 0;
}

static int dvbsky_ci_slot_ts_enable(struct dvb_ca_en50221 *ci, int slot)
{
	struct dvb_usb_device *d = ci->data;
	struct dvbsky_state *state = d_to_priv(d);
	u8 buf;
	int ret;

	dprintk("%s()\n", __func__);
	if (0 != slot)
		return -EINVAL;

	dvbsky_ci_read_i2c(&d->i2c_adap, state->ci_i2c_addr,
			0, &buf, 1);
	buf |= 0x60;

	ret = dvbsky_ci_write_i2c(&d->i2c_adap, state->ci_i2c_addr,
							0, &buf, 1);
	return ret;
}

static int dvbsky_ci_poll_slot_status(struct dvb_ca_en50221 *ci, int slot,
	int open)
{
	struct dvb_usb_device *d = ci->data;
	struct dvbsky_state *state = d_to_priv(d);
	int ret = 0;
	u8 buf = 0;
	/*dprintk("%s()\n", __func__);*/

	/* CAM module INSERT/REMOVE processing. slow operation because of i2c
	 * transfers */
	if (time_after(jiffies, state->next_status_checked_time)) {
		ret = dvbsky_ci_read_i2c(&d->i2c_adap, state->ci_i2c_addr,
				0, &buf, 1);

		/*dprintk("%s() status=%x\n", __func__, buf);*/
		
		state->next_status_checked_time = jiffies
			+ msecs_to_jiffies(1000);

		if (ret != 0)
			return 0;

		if (buf & 1) {
			state->ci_status = DVB_CA_EN50221_POLL_CAM_PRESENT |
				DVB_CA_EN50221_POLL_CAM_READY;
		}
		else
			state->ci_status = 0;
	}
	/*dprintk("%s() ret=%x\n", __func__, state->ci_status);*/
	return state->ci_status;
}

static int dvbsky_ci_init(struct dvb_usb_device *d)
{
	struct dvbsky_state *state = d_to_priv(d);
	int ret;
	u8 cimax_init[34] = {
		0x00, /* module A control*/
		0x00, /* auto select mask high A */
		0x00, /* auto select mask low A */
		0x00, /* auto select pattern high A */
		0x00, /* auto select pattern low A */
		0x44, /* memory access time A */
		0x00, /* invert input A */
		0x00, /* RFU */
		0x00, /* RFU */
		0x00, /* module B control*/
		0x00, /* auto select mask high B */
		0x00, /* auto select mask low B */
		0x00, /* auto select pattern high B */
		0x00, /* auto select pattern low B */
		0x44, /* memory access time B */
		0x00, /* invert input B */
		0x00, /* RFU */
		0x00, /* RFU */
		0x00, /* auto select mask high Ext */
		0x00, /* auto select mask low Ext */
		0x00, /* auto select pattern high Ext */
		0x00, /* auto select pattern low Ext */
		0x00, /* RFU */
		0x02, /* destination - module A */
		0x01, /* power on (use it like store place) */
		0x00, /* RFU */
		0x00, /* int status read only */
		0x00, /* Max: Disable the interrupt in USB solution.*/
		0x05, /* EXTINT=active-high, INT=push-pull */
		0x00, /* USCG1 */
		0x04, /* ack active low */
		0x00, /* LOCK = 0 */
		0x22, /* serial mode, rising in, rising out, MSB first*/
		0x00  /* synchronization */
	};	
	dprintk("%s()\n", __func__);
	state->current_ci_flag = 0xff;
	state->ci_status = 0;
	state->next_status_checked_time = jiffies + msecs_to_jiffies(1000);
	state->ci_i2c_addr = 0x40;

	state->ci.owner               = THIS_MODULE;
	state->ci.read_attribute_mem  = dvbsky_ci_read_attribute_mem;
	state->ci.write_attribute_mem = dvbsky_ci_write_attribute_mem;
	state->ci.read_cam_control    = dvbsky_ci_read_cam_ctl;
	state->ci.write_cam_control   = dvbsky_ci_write_cam_ctl;
	state->ci.slot_reset          = dvbsky_ci_slot_reset;
	state->ci.slot_shutdown       = dvbsky_ci_slot_shutdown;
	state->ci.slot_ts_enable      = dvbsky_ci_slot_ts_enable;
	state->ci.poll_slot_status    = dvbsky_ci_poll_slot_status;
	state->ci.data                = d;

	ret = dvbsky_ci_write_i2c(&d->i2c_adap, state->ci_i2c_addr,
						0, &cimax_init[0], 34);
	/* lock registers */
	ret |= dvbsky_ci_write_i2c(&d->i2c_adap, state->ci_i2c_addr,
						0x1f, &cimax_init[0x18], 1);
	/* power on slots */
	ret |= dvbsky_ci_write_i2c(&d->i2c_adap, state->ci_i2c_addr,
						0x18, &cimax_init[0x18], 1);
	if (0 != ret)
		return ret;
		
	ret = dvb_ca_en50221_init(&d->adapter[0].dvb_adap, &state->ci, 0, 1);
	if (ret)
		return ret;
	state->ci_attached = 1;
	dprintk("%s() complete.\n", __func__);
	return 0;
}

static void dvbsky_ci_release(struct dvb_usb_device *d)
{
	struct dvbsky_state *state = d_to_priv(d);

	/* detach CI */
	if (state->ci_attached)
		dvb_ca_en50221_release(&state->ci);

	return;
}

static int dvbsky_streaming_ctrl(struct dvb_frontend *fe, int onoff)
{
	struct dvb_usb_device *d = fe_to_d(fe);
	/*dprintk("%s() %d\n", __func__, onoff);*/
	return dvbsky_stream_ctrl(d, (onoff == 0) ? 0 : 1);
}

/* GPIO */
static int dvbsky_gpio_ctrl(struct dvb_usb_device *d, u8 gport, u8 value)
{
	u8 obuf[64], ibuf[64];
	obuf[0] = 0x0e;
	obuf[1] = gport;
	obuf[2] = value;
	return dvb_usbv2_generic_rw(d, obuf, 3, ibuf, 1);
}

/* I2C */
static int dvbsky_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msg[],
	int num)
{
	struct dvb_usb_device *d = i2c_get_adapdata(adap);
	int ret = 0;
	u8 ibuf[64], obuf[64]; 

	if (mutex_lock_interruptible(&d->i2c_mutex) < 0)
		return -EAGAIN;

	if (num > 2) {
		printk(KERN_ERR "dvbsky_usb: too many i2c messages[%d] than 2.", num);
		ret = -EOPNOTSUPP;
		goto i2c_error;
	}
		
	if(num == 1) {
		if (msg[0].len > 60) {
			printk(KERN_ERR "dvbsky_usb: too many i2c bytes[%d] than 60.", msg[0].len);
			ret = -EOPNOTSUPP;
			goto i2c_error;
		}
		if (msg[0].flags & I2C_M_RD) {
			/* single read */
			obuf[0] = 0x09;
			obuf[1] = 0;
			obuf[2] = msg[0].len;
			obuf[3] = msg[0].addr;
			ret = dvb_usbv2_generic_rw(d, obuf, 4, ibuf, msg[0].len + 1);
			/*dprintk("%s(): read status = %d\n", __func__, ibuf[0]);*/
			if (!ret)
				memcpy(msg[0].buf, &ibuf[1], msg[0].len);
		} else {
			/* write */
			obuf[0] = 0x08;
			obuf[1] = msg[0].addr;
			obuf[2] = msg[0].len;
			memcpy(&obuf[3], msg[0].buf, msg[0].len);
			ret = dvb_usbv2_generic_rw(d, obuf, msg[0].len + 3, ibuf, 1);
			/*dprintk("%s(): write status = %d\n", __func__, ibuf[0]);*/
		}
	} else {
		if ((msg[0].len > 60) || (msg[1].len > 60)) {
			printk(KERN_ERR "dvbsky_usb: too many i2c bytes[w-%d][r-%d] than 60.", msg[0].len, msg[1].len);
			ret = -EOPNOTSUPP;
			goto i2c_error;
		}
		/* write then read */
		obuf[0] = 0x09;
		obuf[1] = msg[0].len;
		obuf[2] = msg[1].len;
		obuf[3] = msg[0].addr;
		memcpy(&obuf[4], msg[0].buf, msg[0].len);
		ret = dvb_usbv2_generic_rw(d, obuf, msg[0].len + 4, ibuf, msg[1].len + 1);
		/*dprintk("%s(): write then read status = %d\n", __func__, ibuf[0]);*/
		if (!ret)
			memcpy(msg[1].buf, &ibuf[1], msg[1].len);
	}
i2c_error:
	mutex_unlock(&d->i2c_mutex);
	return (ret) ? ret : num;
}

static u32 dvbsky_i2c_func(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C;
}

static struct i2c_algorithm dvbsky_i2c_algo = {
	.master_xfer   = dvbsky_i2c_xfer,
	.functionality = dvbsky_i2c_func,
};

#if IS_ENABLED(CONFIG_RC_CORE)
static int dvbsky_rc_query(struct dvb_usb_device *d)
{
	u32 code = 0xffff;
	u8 obuf[2], ibuf[2], toggle;
	int ret;
	obuf[0] = 0x10;
	ret = dvb_usbv2_generic_rw(d, obuf, 1, ibuf, 2);
	if(ret == 0)
		code = (ibuf[0] << 8) | ibuf[1];

	if (code != 0xffff) {
		dprintk("rc code: %x", code);
		toggle = (code & 0x800) ? 1 : 0;
		code &= 0x3f;
		rc_keydown(d->rc_dev, code, toggle);
	}
	return 0;
}

static int dvbsky_get_rc_config(struct dvb_usb_device *d, struct dvb_usb_rc *rc)
{
	rc->allowed_protos = RC_BIT_RC5;
	rc->query          = dvbsky_rc_query;
	rc->interval       = 300;
	return 0;
}
#else
	#define dvbsky_get_rc_config NULL
#endif

static int dvbsky_sync_ctrl(struct dvb_frontend *fe)
{
	struct dvb_usb_device *d = fe_to_d(fe);
	return dvbsky_stream_ctrl(d, 1);
}

static int dvbsky_usb_set_voltage(struct dvb_frontend *fe, fe_sec_voltage_t voltage)
{
	struct dvb_usb_device *d = fe_to_d(fe);
	u8 value;

	if (voltage == SEC_VOLTAGE_OFF)
		value = 0;
	else
		value = 1;
	return dvbsky_gpio_ctrl(d, 0x80, value);
}

static int dvbsky_read_mac_addr(struct dvb_usb_adapter *adap, u8 mac[6])
{
	struct dvb_usb_device *d = adap_to_d(adap);
	u8 obuf[] = { 0x1e, 0x00 };
	u8 ibuf[6] = { 0 };
	struct i2c_msg msg[] = {
		{
			.addr = 0x51,
			.flags = 0,
			.buf = obuf,
			.len = 2,
		}, {
			.addr = 0x51,
			.flags = I2C_M_RD,
			.buf = ibuf,
			.len = 6,

		}
	};
	
	if (i2c_transfer(&d->i2c_adap, msg, 2) == 2)
		memcpy(mac, ibuf, 6);

	printk(KERN_INFO "dvbsky_usb MAC address=%pM\n", mac);
	
	return 0;
}

static struct m88ds3103_config dvbsky_usb_ds3103_config = {
	.demod_address = 0x68,
	.ci_mode = 1,
	.pin_ctrl = 0x83,
	.ts_mode = 0,
	.start_ctrl = dvbsky_sync_ctrl,
	.set_voltage = dvbsky_usb_set_voltage,
};

static int dvbsky_s960_attach(struct dvb_usb_adapter *adap)
{
	struct dvbsky_state *state = adap_to_priv(adap);
	struct dvb_usb_device *d = adap_to_d(adap);
	int ret = 0;
	
	dprintk("%s()\n", __func__);

	dvbsky_gpio_ctrl(d, 0x04, 1);
		
	dvbsky_gpio_ctrl(d, 0x83, 0);
	msleep(50);
	dvbsky_gpio_ctrl(d, 0x83, 1);
	msleep(20);
	
	adap->fe[0] = dvb_attach(m88ds3103_attach,
				&dvbsky_usb_ds3103_config,
				&d->i2c_adap);
	if (!adap->fe[0]) {
		printk(KERN_ERR "dvbsky_s960_attach fail.");
		ret = -ENODEV;
	}
	
	state->has_ci = 0;

	return ret;
}

static int dvbsky_identify_state(struct dvb_usb_device *d, const char **name)
{
	return WARM;
}

static int dvbsky_init(struct dvb_usb_device *d)
{
	struct dvbsky_state *state = d_to_priv(d);
	int ret;

	/* use default interface */
	ret = usb_set_interface(d->udev, 0, 0);
	if (ret)
		return ret;

	mutex_init(&state->stream_mutex);
	
	/* attach CI */
	if (state->has_ci) {
		dvbsky_gpio_ctrl(d, 0xc0, 1);
		msleep(100);
		dvbsky_gpio_ctrl(d, 0xc0, 0);
		msleep(50);
		state->ci_attached = 0;
		ret = dvbsky_ci_init(d);
		if (ret)
			return ret;
	}
	return 0;
}

static void dvbsky_exit(struct dvb_usb_device *d)
{
	return dvbsky_ci_release(d);
}

/* DVB USB Driver stuff */
static struct dvb_usb_device_properties dvbsky_s960_props = {
	.driver_name = KBUILD_MODNAME,
	.owner = THIS_MODULE,
	.adapter_nr = adapter_nr,
	.size_of_priv = sizeof(struct dvbsky_state),

	.generic_bulk_ctrl_endpoint = 0x01,
	.generic_bulk_ctrl_endpoint_response = 0x81,

	.i2c_algo         = &dvbsky_i2c_algo,
	.frontend_attach  = dvbsky_s960_attach,
	.init             = dvbsky_init,
	.get_rc_config    = dvbsky_get_rc_config,
	.streaming_ctrl   = dvbsky_streaming_ctrl,
	.identify_state	  = dvbsky_identify_state,
	.exit             = dvbsky_exit,
	.read_mac_address = dvbsky_read_mac_addr,

	.num_adapters = 1,
	.adapter = {
		{
			.stream = DVB_USB_STREAM_BULK(0x82, 8, 4096),
		}
	}
};

static const struct usb_device_id dvbsky_id_table[] = {
	{ DVB_USB_DEVICE(0x0572, 0x6831,
		&dvbsky_s960_props, "DVBSky S960/S860", RC_MAP_DVBSKY) },
	{ }
};
MODULE_DEVICE_TABLE(usb, dvbsky_id_table);

static struct usb_driver dvbsky_usb_driver = {
	.name = KBUILD_MODNAME,
	.id_table = dvbsky_id_table,
	.probe = dvb_usbv2_probe,
	.disconnect = dvb_usbv2_disconnect,
	.suspend = dvb_usbv2_suspend,
	.resume = dvb_usbv2_resume,
	.reset_resume = dvb_usbv2_reset_resume,
	.no_dynamic_id = 1,
	.soft_unbind = 1,
};

module_usb_driver(dvbsky_usb_driver);

MODULE_AUTHOR("Max nibble <nibble.max@gmail.com>");
MODULE_DESCRIPTION("Driver for DVBSky USB2.0");
MODULE_LICENSE("GPL");
