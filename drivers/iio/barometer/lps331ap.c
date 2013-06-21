/*
* STMicroelectronics LPS331AP Pressure / Temperature Sensor module driver
*
* Copyright (C) 2013 Samsung Electronics Co., Ltd.
* Author: Jacek Anaszewski <j.anaszewski@samsung.com>
*
* Copyright (C) 2010 STMicroelectronics- MSH - Motion Mems BU - Application Team
* Authors:
*	Matteo Dameno <matteo.dameno@st.com>
*	Carmine Iascone <carmine.iascone@st.com>
*
* Both authors are willing to be considered the contact and update points for
* the driver.
*
* The device can be controlled by IIO sysfs interface from the location
* /sys/bus/iio/devices/iio:deviceN, where N is the variable IIO device
* number. The LPS331AP device can be identified by reading the 'name'
* attribute.
* Output data from the device can be read in two ways - on demand and by
* initiating IIO events for the device. The former mode allows for power
* saving as it instructs the device to do a 'one shot' measurement and return
* to the power down mode, whereas the latter enables cyclic measurements
* with the period managed through the 'odr' attribute, where possible settings
* are: 40ms, 80ms, 143ms and 1000ms. The value has to be given in miliseconds.
* Every integer value is accepted - the driver will round it to the nearest
* bottom odr setting.
*
* Initialization of 'one shot' measurement:
*	- read in_pressure_raw or in_temp_raw sysfs attribute
* Managing cyclic measurements:
*	1) enable:
*		- write 1 to the events/in_pressure_mag_either_en and/or
*		  events/in_temp_mag_either_en attribute
*	2) read:
*		- perform periodic read of events/in_pressure_mag_either_value
*		  and/or events/in_temp_mag_either_value atrributes
*	3) alter output data rate:
*		- write value of 40, 80, 143 or 1000 to the 'odr' attribute
*	1) disable:
*		- write 0 to the events/in_pressure_mag_either_en and/or
*		  events/in_temp_mag_either_en attribute
*
* Read pressures and temperatures output can be converted in units of
* measurement by dividing them respectively by in_pressure_scale and
* in_temp_scale. Temperature values must then be added by the constant float
* in_temp_offset expressed as Celsius degrees.
*
* Obtained values are then expessed as
* mbar (=0.1 kPa) and Celsius degrees.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*/

#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/bitops.h>

#define LPS331AP_VENDOR		"STM"
#define LPS331AP_CHIP_ID		"LPS331"

#define PR_ABS_MAX	8388607	/* 24 bit 2'compl */
#define PR_ABS_MIN	-8388608

#define WHOAMI_LPS331AP_PRS	0xBB	/* Expected content for WAI */

/* Control registers */
#define REF_P_XL	0x08	/*      pressure reference      */
#define REF_P_L		0x09	/*      pressure reference      */
#define REF_P_H		0x0a	/*      pressure reference      */
#define REF_T_L		0x0b	/*      temperature reference   */
#define REF_T_H		0x0c	/*      temperature reference   */

#define WHO_AM_I	0x0f	/*      WhoAmI register         */
#define TP_RESOL	0x10	/*      Pres Temp resolution set */
#define DGAIN_L		0x18	/*      Dig Gain (3 regs)       */

#define CTRL_REG1	0x20	/*      power / ODR control reg */
#define CTRL_REG2	0x21	/*      boot reg                */
#define CTRL_REG3	0x22	/*      interrupt control reg   */
#define INT_CFG_REG	0x23	/*      2nterrupt config reg    */
#define INT_SRC_REG	0x24	/*      interrupt source reg    */
#define THS_P_L		0x25	/*      pressure threshold      */
#define THS_P_H		0x26	/*      pressure threshold      */
#define STATUS_REG	0X27	/*      status reg              */

#define PRESS_OUT_XL	0x28	/*      press output (3 regs)   */
#define TEMP_OUT_L	0x2b	/*      temper output (2 regs)  */
#define DELTAREG_1	0x3c	/*      deltaPressure reg1       */
#define DELTAREG_2	0x3d	/*      deltaPressure reg2       */
#define DELTAREG_3	0x3e	/*      deltaPressure reg3       */

/* Register aliases */
#define P_REF_INDATA_REG	REF_P_XL
#define T_REF_INDATA_REG	REF_T_L
#define P_THS_INDATA_REG	THS_P_L
#define P_OUTDATA_REG		PRESS_OUT_XL
#define T_OUTDATA_REG		TEMP_OUT_L
#define OUTDATA_REG		PRESS_OUT_XL

/* Register masks */
#define LPS331AP_PRS_ENABLE_MASK	0x80	/*  ctrl_reg1 */
#define LPS331AP_PRS_ODR_MASK		0x70	/*  ctrl_reg1 */
#define LPS331AP_PRS_DIFF_MASK		0x08	/*  ctrl_reg1 */
#define LPS331AP_PRS_BDU_MASK		0x04	/*  ctrl_reg1 */
#define LPS331AP_PRS_DELTA_EN_MASK	0x02	/*  ctrl_reg1 */
#define LPS331AP_PRS_BOOT_MASK		0x80	/*  ctrl_reg2 */
#define LPS331AP_PRS_SWRESET_MASK	0x04	/*  ctrl_reg2 */
#define LPS331AP_PRS_AUTOZ_MASK		0x02	/*  ctrl_reg2 */
#define LPS331AP_PRS_ONE_SHOT		0x01	/*  ctrl_reg2 */
#define LPS331AP_PRS_INT1_MASK		0x07	/*  ctrl_reg3 */
#define LPS331AP_PRS_INT2_MASK		0x38	/*  ctrl_reg3 */
#define LPS331AP_PRS_PP_OD_MASK		0x40	/*  ctrl_reg3 */

#define LPS331AP_PRS_PM_NORMAL		0x80	/* Power Normal Mode */
#define LPS331AP_PRS_PM_OFF		0x00	/* Power Down */

#define LPS331AP_PRS_DIFF_ON		0x08	/* En Difference circuitry */
#define LPS331AP_PRS_DIFF_OFF		0x00	/* Dis Difference circuitry */

#define LPS331AP_PRS_AUTOZ_ON		0x02	/* En AutoZero Function */
#define LPS331AP_PRS_AUTOZ_OFF		0x00	/* Dis Difference Function */

#define LPS331AP_PRS_BDU_ON		0x04	/* En BDU Block Data Upd */
#define LPS331AP_PRS_DELTA_EN_ON	0x02	/* En Delta Press registers */
#define LPS331AP_PRS_DIFF_EN		0x08	/* En diff pressure computing */
#define LPS331AP_PRS_RES_AVGTEMP_064	0x60
#define LPS331AP_PRS_RES_AVGTEMP_128	0x70
#define LPS331AP_PRS_RES_AVGPRES_512	0x0a
#define LPS331AP_PRS_RES_AVGPRES_384	0x09

#define LPS331AP_PRS_RES_MAX		(LPS331AP_PRS_RES_AVGTEMP_128  | \
						LPS331AP_PRS_RES_AVGPRES_512)
						/* Max Resol. for 1/7/12,5Hz */

#define LPS331AP_PRS_RES_25HZ		(LPS331AP_PRS_RES_AVGTEMP_128  | \
						LPS331AP_PRS_RES_AVGPRES_384)
						/* Max Resol. for 25Hz */
#define LPS331AP_PRS_DRDY_INT1		0x04	/* En INT1 'data ready' */
#define LPS331AP_PRS_DRDY_INT2		0x20	/* En INT2 'data ready' */
#define LPS331AP_PRS_P_HIGH_INT1	0x01	/* En INT1 P_high */
#define LPS331AP_PRS_P_HIGH_INT2	0x08	/* En INT2 P_high */
#define LPS331AP_PRS_P_LOW_INT1		0x02	/* En INT1 P_low */
#define LPS331AP_PRS_P_LOW_INT2		0x10	/* En INT2 P_low */
#define LPS331AP_PRS_PH_E		0x01	/* En prs high evt interrupt */
#define LPS331AP_PRS_PL_E		0x02	/* En prs low evt interrupt */

#define LPS331AP_PRS_INT_FLAG_PH	0x01	/* Diff press high int flag */
#define LPS331AP_PRS_INT_FLAG_PL	0x02	/* Diff press low int flag */
#define LPS331AP_PRS_INT_FLAG_IA	0x04	/* Any int flag */

#define LPS331AP_PRESS_SCALE		4096
#define LPS331AP_TEMP_SCALE		480
#define LPS331AP_TEMP_OFFSET_INT	42
#define LPS331AP_TEMP_OFFSET_FRACT	500000

#define I2C_AUTO_INCREMENT		0x80

/* Register cache indices */
#define LPS331AP_RES_REF_P_XL		0
#define LPS331AP_RES_REF_P_L		1
#define LPS331AP_RES_REF_P_H		2
#define LPS331AP_RES_REF_T_L		3
#define LPS331AP_RES_REF_T_H		4
#define LPS331AP_RES_TP_RESOL		5
#define LPS331AP_RES_CTRL_REG1		6
#define LPS331AP_RES_CTRL_REG2		7
#define LPS331AP_RES_CTRL_REG3		8
#define LPS331AP_RES_INT_CFG_REG	9
#define LPS331AP_RES_THS_P_L		10
#define LPS331AP_RES_THS_P_H		11
#define REG_ENTRIES			12

/* Poll delays */
#define LPS331AP_ODR_DELAY_DEFAULT	200
#define LPS331AP_ODR_DELAY_MINIMUM	40
#define LPS331AP_DATA_READY_TIMEOUT	(HZ * 2)
#define LPS331AP_DATA_READY_POLL_TIME	10

#define LPS331AP_PRS_DEV_NAME		"lps331ap"

/* Barometer and Termometer output data rate (ODR) */
#define LPS331AP_PRS_ODR_ONESH	0x00	/* one shot both                */
#define LPS331AP_PRS_ODR_1_1	0x10	/*  1  Hz baro,  1  Hz term ODR */
#define LPS331AP_PRS_ODR_7_7	0x50	/*  7  Hz baro,  7  Hz term ODR */
#define LPS331AP_PRS_ODR_12_12	0x60	/* 12.5Hz baro, 12.5Hz term ODR */
#define LPS331AP_PRS_ODR_25_25	0x70	/* 25  Hz baro, 25  Hz term ODR */

enum {
	LPS331AP_LPS331AP_VENDOR,
	LPS331AP_NAME,
	LPS331AP_ODR,
	LPS331AP_PRESSURE_REF_LEVEL,
};

enum {
	LPS331AP_INTERRUPT_SRC_NONE,
	LPS331AP_INTERRUPT_SRC_INT1,
	LPS331AP_INTERRUPT_SRC_INT2
};

enum prs_state {
	FL_HW_ENABLED,
	FL_HW_INITIALIZED,
	FL_PRESS_EV_ENABLED,
	FL_TEMP_EV_ENABLED
};

static const struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lps331ap_odr_table[] = {
	{ 40, LPS331AP_PRS_ODR_25_25 },
	{ 80, LPS331AP_PRS_ODR_12_12 },
	{ 143, LPS331AP_PRS_ODR_7_7 },
	{ 1000, LPS331AP_PRS_ODR_1_1 }
};

struct outputdata {
	unsigned int press;
	int temperature;
};

struct lps331ap_data {
	struct i2c_client *client;
	struct mutex lock;
	struct outputdata press_temp;

	unsigned int output_data_rate;
	unsigned long flags;
	u8 reg_cache[REG_ENTRIES];

	int lps_irq;
	int lps_irq_src;
};

static int lps331ap_i2c_read(struct lps331ap_data *prs,
						u8 *buf, int len)
{
	int err;
	struct i2c_msg msgs[] = {
		{
			.addr = prs->client->addr,
			.flags = prs->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = prs->client->addr,
			.flags = (prs->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	err = i2c_transfer(prs->client->adapter, msgs, 2);
	if (err != 2) {
		dev_err(&prs->client->dev, "read transfer error = %d\n", err);
		err = -EIO;
	}
	return err;
}

static int lps331ap_i2c_write(struct lps331ap_data *prs,
						u8 *buf, int len)
{
	int err;
	struct i2c_msg msgs[] = {
		{
			.addr = prs->client->addr,
			.flags = prs->client->flags & I2C_M_TEN,
			.len = len + 1,
			.buf = buf,
		},
	};

	err = i2c_transfer(prs->client->adapter, msgs, 1);
	if (err != 1) {
		dev_err(&prs->client->dev, "write transfer error\n");
		err = -EIO;
	}
	return err;
}

static int lps331ap_hw_init(struct lps331ap_data *prs)
{
	int err;
	u8 buf[6];

	dev_dbg(&prs->client->dev, "hw init start\n");

	buf[0] = WHO_AM_I;
	err = lps331ap_i2c_read(prs, buf, 1);
	if (err < 0) {
		dev_err(&prs->client->dev,
		"error reading WHO_AM_I: is device available/working?\n");
		goto err_hw_init;
	}

	if (buf[0] != WHOAMI_LPS331AP_PRS) {
		err = -ENODEV;
		dev_err(&prs->client->dev,
			"device unknown. Expected: 0x%02x, Replies: 0x%02x\n",
			WHOAMI_LPS331AP_PRS, buf[0]);
		goto err_hw_init;
	}

	buf[0] = (I2C_AUTO_INCREMENT | P_REF_INDATA_REG);
	buf[1] = prs->reg_cache[LPS331AP_RES_REF_P_XL];
	buf[2] = prs->reg_cache[LPS331AP_RES_REF_P_L];
	buf[3] = prs->reg_cache[LPS331AP_RES_REF_P_H];
	buf[4] = prs->reg_cache[LPS331AP_RES_REF_T_L];
	buf[5] = prs->reg_cache[LPS331AP_RES_REF_T_H];
	err = lps331ap_i2c_write(prs, buf, 5);
	if (err < 0)
		goto err_hw_init;

	buf[0] = TP_RESOL;
	buf[1] = prs->reg_cache[LPS331AP_RES_TP_RESOL];
	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		goto err_hw_init;

	buf[0] = (I2C_AUTO_INCREMENT | P_THS_INDATA_REG);
	buf[1] = prs->reg_cache[LPS331AP_RES_THS_P_L];
	buf[2] = prs->reg_cache[LPS331AP_RES_THS_P_H];
	err = lps331ap_i2c_write(prs, buf, 2);
	if (err < 0)
		goto err_hw_init;

	/* clear INT_ACK flag */
	buf[0] = INT_SRC_REG;
	err = lps331ap_i2c_read(prs, buf, 1);
	if (err < 0)
		return err;

	/* CTRL_REG3 register has to be initialized before powering on */
	buf[0] = CTRL_REG3;
	buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG3];
	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		goto err_hw_init;

	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG1);
	buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG1];
	buf[2] = prs->reg_cache[LPS331AP_RES_CTRL_REG2];
	err = lps331ap_i2c_write(prs, buf, 2);
	if (err < 0)
		goto err_hw_init;

	set_bit(FL_HW_INITIALIZED, &prs->flags);
	dev_dbg(&prs->client->dev, "hw init done\n");

	return 0;

err_hw_init:
	clear_bit(FL_HW_INITIALIZED, &prs->flags);
	dev_err(&prs->client->dev, "hw init error 0x%02x,0x%02x: %d\n",
		buf[0], buf[1], err);
	return err;
}

static int lps331ap_device_power_off(struct lps331ap_data *prs)
{
	int err;
	u8 buf[2];

	prs->reg_cache[LPS331AP_RES_CTRL_REG1]
	    &= ~LPS331AP_PRS_PM_NORMAL;

	buf[0] = CTRL_REG1;
	buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG1];

	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		return err;

	return 0;
}

static int lps331ap_device_power_on(struct lps331ap_data *prs)
{
	u8 buf[2];
	int err;

	prs->reg_cache[LPS331AP_RES_CTRL_REG1] |= LPS331AP_PRS_PM_NORMAL;

	if (!test_bit(FL_HW_INITIALIZED, &prs->flags)) {
		err = lps331ap_hw_init(prs);
		lps331ap_device_power_off(prs);
	} else {
		buf[0] = CTRL_REG1;
		buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG1];
		err = lps331ap_i2c_write(prs, buf, 1);
	}

	return err;
}

static int lps331ap_update_odr(struct lps331ap_data *prs, int delay_ms)
{
	int err = -1;
	int i;

	u8 buf[2];
	u8 init_val, updated_val;
	u8 curr_val, new_val;
	u8 mask = LPS331AP_PRS_ODR_MASK;
	u8 resol = LPS331AP_PRS_RES_MAX;

	/*
	 * Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (longest period) backward (shortest
	 * period), to support the poll_interval requested by the system.
	 * It must be the longest period shorter then the set poll period.
	 */
	for (i = ARRAY_SIZE(lps331ap_odr_table) - 1; i >= 0; i--) {
		if ((lps331ap_odr_table[i].cutoff_ms <= delay_ms)
		    || (i == 0))
			break;
	}

	prs->output_data_rate = lps331ap_odr_table[i].cutoff_ms;

	new_val = lps331ap_odr_table[i].mask;
	if (new_val == LPS331AP_PRS_ODR_25_25)
		resol = LPS331AP_PRS_RES_25HZ;

	if (!test_bit(FL_HW_ENABLED, &prs->flags))
		return 0;

	buf[0] = CTRL_REG1;
	err = lps331ap_i2c_read(prs, buf, 1);
	if (err < 0)
		goto error;
	/* work on all but ENABLE bits */
	init_val = buf[0];
	prs->reg_cache[LPS331AP_RES_CTRL_REG1] = init_val;

	/* disable */
	curr_val = ((LPS331AP_PRS_ENABLE_MASK & LPS331AP_PRS_PM_OFF)
		    | ((~LPS331AP_PRS_ENABLE_MASK) & init_val));
	buf[0] = CTRL_REG1;
	buf[1] = curr_val;
	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		goto error;

	buf[0] = CTRL_REG1;
	updated_val = ((mask & new_val) | ((~mask) & curr_val));

	buf[0] = CTRL_REG1;
	buf[1] = updated_val;
	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		goto error;

	/* enable */
	curr_val = ((LPS331AP_PRS_ENABLE_MASK & LPS331AP_PRS_PM_NORMAL)
		    | ((~LPS331AP_PRS_ENABLE_MASK) & updated_val));
	buf[0] = CTRL_REG1;
	buf[1] = curr_val;
	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		goto error;

	buf[0] = TP_RESOL;
	buf[1] = resol;
	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		goto error;

	prs->reg_cache[LPS331AP_RES_CTRL_REG1] = curr_val;
	prs->reg_cache[LPS331AP_RES_TP_RESOL] = resol;

	return err;

error:
	dev_err(&prs->client->dev, "update odr failed 0x%02x,0x%02x: %d\n",
		buf[0], buf[1], err);

	return err;
}

static int lps331ap_set_press_reference(struct lps331ap_data *prs,
						 s32 new_reference)
{
	u8 buf[4], *ref;
	__le32 new_ref;
	int err;

	new_ref = cpu_to_le32(new_reference);
	ref = (u8 *)&new_ref;

	buf[0] = (I2C_AUTO_INCREMENT | P_REF_INDATA_REG);
	buf[1] = ref[0];
	buf[2] = ref[1];
	buf[3] = ref[2];

	err = lps331ap_i2c_write(prs, buf, 3);
	if (err < 0)
		return err;

	prs->reg_cache[LPS331AP_RES_REF_P_XL] = ref[0];
	prs->reg_cache[LPS331AP_RES_REF_P_L] = ref[1];
	prs->reg_cache[LPS331AP_RES_REF_P_H] = ref[2];

	return 0;
}

static int lps331ap_get_press_reference(struct lps331ap_data *prs,
							s32 *reference)
{
	u8 buf[4];
	int err;

	memset(buf, 0, sizeof(buf));
	buf[0] = (I2C_AUTO_INCREMENT | P_REF_INDATA_REG);
	err = lps331ap_i2c_read(prs, buf, 3);

	if (err < 0)
		return err;

	*reference = le32_to_cpup((__le32 *) buf);

	return 0;
}

static int lps331ap_enable(struct lps331ap_data *prs)
{
	int err;

	if (!test_bit(FL_HW_ENABLED, &prs->flags)) {
		err = lps331ap_device_power_on(prs);
		if (err < 0) {
			clear_bit(FL_HW_ENABLED, &prs->flags);
			return err;
		}
		set_bit(FL_HW_ENABLED, &prs->flags);
	}

	return 0;
}

static int lps331ap_disable(struct lps331ap_data *prs)
{
	int err;

	if (!test_bit(FL_HW_ENABLED, &prs->flags))
		return 0;

	err = lps331ap_device_power_off(prs);
	clear_bit(FL_HW_ENABLED, &prs->flags);

	return err;
}

static int lps331ap_get_presstemp_data(struct lps331ap_data *prs,
						struct outputdata *out)
{
	/*
	 * Data bytes from hardware:
	 * PRESS_OUT_XL, PRESS_OUT_L, PRESS_OUT_H,
	 * TEMP_OUT_L, TEMP_OUT_H.
	 */
	u8 prs_data[5] = { 0, };
	int err = 0;

	s32 pressure = 0;
	s16 temperature = 0;

	int reg_to_read = 5;

	prs_data[0] = (I2C_AUTO_INCREMENT | OUTDATA_REG);
	err = lps331ap_i2c_read(prs, prs_data, reg_to_read);
	if (err < 0)
		return err;

	dev_dbg(&prs->client->dev,
		"temp out tH = 0x%02x, tL = 0x%02x",
		prs_data[4], prs_data[3]);
	dev_dbg(&prs->client->dev,
		"press_out: pH = 0x%02x, pL = 0x%02x, pXL= 0x%02x\n",
		prs_data[2], prs_data[1], prs_data[0]);

	pressure = (s32) ((((s8) prs_data[2]) << 16) |
			  (prs_data[1] << 8) | (prs_data[0]));
	temperature = (s16) ((((s8) prs_data[4]) << 8) | (prs_data[3]));

	out->press = pressure;
	out->temperature = temperature;

	return 0;
}

static ssize_t lps331ap_get_press_ref(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct iio_dev *dev_info = dev_to_iio_dev(dev);
	struct lps331ap_data *prs = iio_priv(dev_info);
	s32 val;
	int err;

	mutex_lock(&prs->lock);
	err = lps331ap_get_press_reference(prs, &val);
	mutex_unlock(&prs->lock);
	if (err < 0)
		return err;

	return sprintf(buf, "%d\n", val);
}

static ssize_t lps331ap_set_press_ref(struct device *dev,
		       struct device_attribute *attr,
		       const char *buf, size_t size)
{
	struct iio_dev *dev_info = dev_to_iio_dev(dev);
	struct lps331ap_data *prs = iio_priv(dev_info);
	long val;
	int err;

	if (kstrtol(buf, 10, &val))
		return -EINVAL;

	if (val < PR_ABS_MIN || val > PR_ABS_MAX)
		return -EINVAL;

	mutex_lock(&prs->lock);
	err = lps331ap_set_press_reference(prs, val);
	mutex_unlock(&prs->lock);

	return err < 0 ? err : size;
}
static ssize_t lps331ap_get_odr(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val;
	struct iio_dev *dev_info = dev_to_iio_dev(dev);
	struct lps331ap_data *prs = iio_priv(dev_info);

	mutex_lock(&prs->lock);
	val = prs->output_data_rate;
	mutex_unlock(&prs->lock);

	return sprintf(buf, "%d\n", val);
}

static ssize_t lps331ap_set_odr(struct device *dev,
		 struct device_attribute *attr, const char *buf, size_t size)
{
	struct iio_dev *dev_info = dev_to_iio_dev(dev);
	struct lps331ap_data *prs = iio_priv(dev_info);
	unsigned long delay_ms = 0;
	unsigned int delay_min = LPS331AP_ODR_DELAY_MINIMUM;

	if (kstrtoul(buf, 10, &delay_ms))
		return -EINVAL;
	if (!delay_ms)
		return -EINVAL;

	dev_dbg(&prs->client->dev, "delay_ms passed = %ld\n", delay_ms);
	delay_ms = max_t(unsigned int, (unsigned int)delay_ms, delay_min);

	mutex_lock(&prs->lock);
	prs->output_data_rate = delay_ms;
	lps331ap_update_odr(prs, delay_ms);

	if (delay_ms == LPS331AP_ODR_DELAY_MINIMUM)
		dev_dbg(&prs->client->dev, "delay limited to 40ms\n");

	mutex_unlock(&prs->lock);

	return size;
}

static ssize_t lps331_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", LPS331AP_VENDOR);
}

static ssize_t lps331_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", LPS331AP_CHIP_ID);
}

static int lps331ap_wait_one_shot_ready_polled(struct iio_dev *indio_dev)
{
	struct lps331ap_data *prs = iio_priv(indio_dev);
	u32 timeout_ms = LPS331AP_DATA_READY_TIMEOUT;
	u8 buf;
	int err;

	/* Wait for the conversion to complete. */
	while (timeout_ms) {
		msleep(LPS331AP_DATA_READY_POLL_TIME);
		buf = CTRL_REG2;
		err = lps331ap_i2c_read(prs, &buf, 1);
		if (err < 0)
			return err;
		if (!(buf & LPS331AP_PRS_ONE_SHOT))
			break;
		timeout_ms -= LPS331AP_DATA_READY_POLL_TIME;
	}
	if (!timeout_ms) {
		dev_err(&prs->client->dev, "Conversion timeout occurred.\n");
		return -ETIME;
	}

	return err;
}

static u8 get_drdy_reg_mask(struct lps331ap_data *prs)
{
	if (prs->lps_irq_src == LPS331AP_INTERRUPT_SRC_INT1)
		return LPS331AP_PRS_DRDY_INT1;
	else
		return LPS331AP_PRS_DRDY_INT2;
}

static int lps331ap_read_presstemp(struct iio_dev *indio_dev, int index,
					int *val)
{
	struct lps331ap_data *prs = iio_priv(indio_dev);
	u8 drdy_reg_mask;
	int enabled;
	char buf[2], curr_odr;
	int err;

	mutex_lock(&prs->lock);

	drdy_reg_mask = get_drdy_reg_mask(prs);
	/*
	 * Set 'one shot' mode if either device is in power down mode
	 * or it is powered up but 'data ready' interrupt isn't enabled.
	 * If this condition evaluates to false then it means that the recent
	 * output is being cached periodically by 'data ready' interrupt
	 * handler, so we can return the cached value.
	 */
	enabled = test_bit(FL_HW_ENABLED, &prs->flags);
	if (!enabled ||
	    !(prs->reg_cache[LPS331AP_RES_CTRL_REG3] & drdy_reg_mask)) {
		/* ensure device is in power down mode */
		lps331ap_disable(prs);

		/* set ODR configuration to 'one shot' */
		curr_odr = prs->reg_cache[LPS331AP_RES_CTRL_REG1]
		    & LPS331AP_PRS_ODR_MASK;
		prs->reg_cache[LPS331AP_RES_CTRL_REG1]
		    &= ~LPS331AP_PRS_ODR_MASK;
		buf[0] = CTRL_REG1;
		buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG1];
		err = lps331ap_i2c_write(prs, buf, 1);
		if (err < 0)
			goto exit;

		/* power on the device */
		err = lps331ap_enable(prs);
		if (err < 0)
			goto exit;

		/* set ONE_SHOT mode */
		buf[0] = CTRL_REG2;
		buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG2]
		    | LPS331AP_PRS_ONE_SHOT;
		err = lps331ap_i2c_write(prs, buf, 1);

		if (err < 0)
			goto exit;

		/* wait for the end of coversion */
		err = lps331ap_wait_one_shot_ready_polled(indio_dev);
		if (err < 0)
			goto exit;

		/* read output data */
		err = lps331ap_get_presstemp_data(prs, &prs->press_temp);
		if (err < 0)
			goto exit;

		/* bring back previous ODR settings */
		prs->reg_cache[LPS331AP_RES_CTRL_REG1]
		    |= curr_odr;
		buf[0] = CTRL_REG1;
		buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG1];
		err = lps331ap_i2c_write(prs, buf, 1);

		if (!enabled)
			lps331ap_disable(prs);
	}

	*val = index == 0 ? prs->press_temp.press : prs->press_temp.temperature;

	mutex_unlock(&prs->lock);

	return IIO_VAL_INT;

exit:
	mutex_unlock(&prs->lock);
	return -EINVAL;
}

static irqreturn_t lps331ap_irq_handler(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct lps331ap_data *prs = iio_priv(indio_dev);
	int err;

	if (test_bit(FL_PRESS_EV_ENABLED, &prs->flags))
		iio_push_event(indio_dev,
			       IIO_UNMOD_EVENT_CODE(IIO_PRESSURE, 0,
						    IIO_EV_TYPE_MAG,
						    IIO_EV_DIR_EITHER),
			       iio_get_time_ns());
	if (test_bit(FL_TEMP_EV_ENABLED, &prs->flags))
		iio_push_event(indio_dev,
			       IIO_UNMOD_EVENT_CODE(IIO_TEMP, 1,
						    IIO_EV_TYPE_MAG,
						    IIO_EV_DIR_EITHER),
			       iio_get_time_ns());

	err = lps331ap_get_presstemp_data(prs, &prs->press_temp);
	if (err < 0)
		dev_err_ratelimited(&prs->client->dev,
				    "get_presstemp_data failed\n");

	return IRQ_HANDLED;
}

static int lps331ap_setup_irq(struct iio_dev *indio_dev)
{
	struct lps331ap_data *prs = iio_priv(indio_dev);
	struct i2c_client *client = prs->client;
	int err;

	err = request_threaded_irq(prs->lps_irq, NULL,
				   lps331ap_irq_handler,
				   IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				   dev_name(&client->dev), indio_dev);
	if (err < 0) {
		dev_err(&client->dev,
			"irq %d request failed: %d\n",
			prs->lps_irq, err);
		return err;
	}

	return 0;
}

static int lps331ap_read_raw(struct iio_dev *indio_dev,
		  struct iio_chan_spec const *chan,
		  int *val, int *val2, long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return lps331ap_read_presstemp(indio_dev, chan->address, val);
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_PRESSURE:
			*val = LPS331AP_PRESS_SCALE;
			return IIO_VAL_INT;
		case IIO_TEMP:
			*val = LPS331AP_TEMP_SCALE;
			return IIO_VAL_INT;
		default:
			return -EINVAL;
		}
	case IIO_CHAN_INFO_OFFSET:
		/* Only the temperature channel has an offset. */
		*val = LPS331AP_TEMP_OFFSET_INT;
		*val2 = LPS331AP_TEMP_OFFSET_FRACT;
		return IIO_VAL_INT_PLUS_MICRO;
	}

	return -EINVAL;
}

static int lps331ap_read_event_val(struct iio_dev *indio_dev, u64 event_code,
					int *val)
{
	struct lps331ap_data *prs = iio_priv(indio_dev);
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	int event_type = IIO_EVENT_CODE_EXTRACT_TYPE(event_code);
	u8 drdy_reg_mask;

	drdy_reg_mask = get_drdy_reg_mask(prs);

	if (event_type == IIO_EV_TYPE_MAG) {
		/*
		 * If events are enabled the output data is being
		 * cached in the interrupt handler and thus we
		 * can return it here.
		 */
		if (prs->reg_cache[LPS331AP_RES_CTRL_REG3]
		    & drdy_reg_mask)
			switch (chan_type) {
			case IIO_PRESSURE:
				*val = prs->press_temp.press;
				break;
			case IIO_TEMP:
				*val = prs->press_temp.temperature;
				break;
		} else
			return -EINVAL;
	}

	return 0;
}

static int lps331ap_write_event_config(struct iio_dev *indio_dev,
			    u64 event_code, int state)
{
	struct lps331ap_data *prs = iio_priv(indio_dev);
	int event_type = IIO_EVENT_CODE_EXTRACT_TYPE(event_code);
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	u8 ctrl_reg3 = prs->reg_cache[LPS331AP_RES_CTRL_REG3];
	u8 drdy_reg_mask;
	char buf[2];
	enum prs_state event_state = (chan_type == IIO_PRESSURE) ?
	    FL_PRESS_EV_ENABLED : FL_TEMP_EV_ENABLED;
	int err = 0;

	if (prs->lps_irq_src == LPS331AP_INTERRUPT_SRC_NONE) {
		dev_err(&prs->client->dev,
			"current platform doesn't support LPS331 interrupts\n");
		return -EINVAL;
	}

	mutex_lock(&prs->lock);

	drdy_reg_mask = get_drdy_reg_mask(prs);

	if (state) {
		/* Don't do anything if the event is already enabled. */
		if (test_bit(event_state, &prs->flags))
			goto done;
		switch (event_type) {
		case IIO_EV_TYPE_MAG:
			ctrl_reg3 |= drdy_reg_mask;
			set_bit(event_state, &prs->flags);
			break;
		default:
			goto err_unsupported_event_type;
		}
	} else {
		switch (event_type) {
		case IIO_EV_TYPE_MAG:
			clear_bit(event_state, &prs->flags);
			/*
			 * Disable 'data ready' interrupt only if
			 * there is no enabled event.
			 */
			if (!test_bit(FL_PRESS_EV_ENABLED, &prs->flags) &&
			    !test_bit(FL_TEMP_EV_ENABLED, &prs->flags))
				ctrl_reg3 &= ~drdy_reg_mask;
			break;
		default:
			goto err_unsupported_event_type;
		}
	}

	/* Setup new interrupt configuration. */
	prs->reg_cache[LPS331AP_RES_CTRL_REG3] = ctrl_reg3;

	buf[0] = CTRL_REG3;
	buf[1] = prs->reg_cache[LPS331AP_RES_CTRL_REG3];
	err = lps331ap_i2c_write(prs, buf, 1);
	if (err < 0)
		goto err_write_event_config;

	/*
	 * The device should be powered on only
	 * if at least one event is enabled.
	 */
	if (ctrl_reg3 & drdy_reg_mask) {
		err = lps331ap_enable(prs);
		if (err < 0)
			goto err_write_event_config;
	} else {
		err = lps331ap_disable(prs);
		if (err < 0)
			goto err_write_event_config;
	}

	/* clear INT_ACK flag */
	buf[0] = INT_SRC_REG;
	err = lps331ap_i2c_read(prs, buf, 1);
	if (err < 0)
		goto err_write_event_config;
done:
	mutex_unlock(&prs->lock);

	return 0;

err_unsupported_event_type:
	err = -EINVAL;
	dev_err(&prs->client->dev, "unsupported event type\n");
err_write_event_config:
	clear_bit(event_state, &prs->flags);
	mutex_unlock(&prs->lock);
	dev_err(&prs->client->dev, "writing event config failed\n");

	return err;
}

static int lps331ap_read_event_config(struct iio_dev *indio_dev,
					u64 event_code)
{
	struct lps331ap_data *prs = iio_priv(indio_dev);
	int event_type = IIO_EVENT_CODE_EXTRACT_TYPE(event_code);
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	enum prs_state event_state = (chan_type == IIO_PRESSURE) ?
	    FL_PRESS_EV_ENABLED : FL_TEMP_EV_ENABLED;
	int val;

	if (prs->lps_irq_src == LPS331AP_INTERRUPT_SRC_NONE)
		return -EINVAL;

	switch (event_type) {
	case IIO_EV_TYPE_MAG:
		val = test_bit(event_state, &prs->flags);
		break;
	default:
		return -EINVAL;
	}

	return val;
}

static IIO_DEVICE_ATTR(odr, 0664,
		       lps331ap_get_odr, lps331ap_set_odr, LPS331AP_ODR);
static IIO_DEVICE_ATTR(pressure_reference_level, 0664,
		       lps331ap_get_press_ref,
		       lps331ap_set_press_ref, LPS331AP_PRESSURE_REF_LEVEL);
static IIO_DEVICE_ATTR(vendor, 0644, lps331_vendor_show, NULL,
		       LPS331AP_LPS331AP_VENDOR);
static IIO_DEVICE_ATTR(name, 0644, lps331_name_show, NULL, LPS331AP_NAME);

static struct attribute *lps331ap_attributes[] = {
	&iio_dev_attr_vendor.dev_attr.attr,
	&iio_dev_attr_name.dev_attr.attr,
	&iio_dev_attr_odr.dev_attr.attr,
	&iio_dev_attr_pressure_reference_level.dev_attr.attr,
	NULL
};

static struct attribute_group lps331ap_attribute_group = {
	.attrs = lps331ap_attributes,
};

#define LPS331AP_PRESSURE_CHANNEL(index)			\
	{							\
		.channel = index,				\
		.address = index,				\
		.type = IIO_PRESSURE,				\
		.modified = 0,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |	\
			     BIT(IIO_CHAN_INFO_SCALE),		\
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_MAG,	\
					IIO_EV_DIR_EITHER),	\
	}

#define LPS331AP_TEMP_CHANNEL(index)				\
	{							\
		.channel = index,				\
		.address = index,				\
		.type = IIO_TEMP,				\
		.modified = 0,					\
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |	\
			     BIT(IIO_CHAN_INFO_SCALE) |		\
			     BIT(IIO_CHAN_INFO_OFFSET),		\
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_MAG,	\
					IIO_EV_DIR_EITHER),	\
	}

static const struct iio_chan_spec lps331ap_channels[] = {
	LPS331AP_PRESSURE_CHANNEL(0), LPS331AP_TEMP_CHANNEL(1),
};

static const struct iio_info lps331ap_info = {
	.attrs = &lps331ap_attribute_group,
	.read_raw = &lps331ap_read_raw,
	.read_event_value = &lps331ap_read_event_val,
	.read_event_config = &lps331ap_read_event_config,
	.write_event_config = &lps331ap_write_event_config,
	.driver_module = THIS_MODULE,
};

static int lps331ap_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	struct lps331ap_data *prs;
	struct iio_dev *indio_dev;
	int int1_src, int2_src;
	int err = -EINVAL;

	indio_dev = iio_device_alloc(sizeof(struct lps331ap_data));
	if (indio_dev == NULL)
		return -ENOMEM;

	prs = iio_priv(indio_dev);

	if (client->dev.of_node) {
		int1_src = irq_of_parse_and_map(client->dev.of_node, 0);
		int2_src = irq_of_parse_and_map(client->dev.of_node, 1);

		if (int1_src) {
			prs->lps_irq = int1_src;
			prs->lps_irq_src = LPS331AP_INTERRUPT_SRC_INT1;
		} else if (int2_src) {
			prs->lps_irq = int2_src;
			prs->lps_irq_src = LPS331AP_INTERRUPT_SRC_INT2;
		} else {
			prs->lps_irq_src = LPS331AP_INTERRUPT_SRC_NONE;
		}
	} else {
		prs->lps_irq = -EINVAL;
	}

	prs->output_data_rate = LPS331AP_ODR_DELAY_DEFAULT;
	prs->client = client;
	mutex_init(&prs->lock);
	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = lps331ap_channels;
	indio_dev->num_channels = ARRAY_SIZE(lps331ap_channels);
	indio_dev->info = &lps331ap_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	if (prs->lps_irq) {
		err = lps331ap_setup_irq(indio_dev);
		if (err < 0) {
			dev_err(&client->dev,
				"Error setting data ready interrupt\n");
			goto err_free_data;
		}
	}

	i2c_set_clientdata(client, indio_dev);

	memset(prs->reg_cache, 0, sizeof(prs->reg_cache));

	/* Do not update output registers until MSB and LSB reading. */
	prs->reg_cache[LPS331AP_RES_CTRL_REG1] = LPS331AP_PRS_BDU_ON;

	/* Perform hw init. */
	err = lps331ap_device_power_on(prs);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_free_data;
	}

	set_bit(FL_HW_ENABLED, &prs->flags);

	err = lps331ap_update_odr(prs, prs->output_data_rate);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto err_power_off;
	}

	lps331ap_device_power_off(prs);

	clear_bit(FL_HW_ENABLED, &prs->flags);

	err = iio_device_register(indio_dev);
	if (err < 0)
		goto err_free_data;

	return 0;

err_power_off:
	lps331ap_device_power_off(prs);
err_free_data:
	if (prs->lps_irq)
		free_irq(prs->lps_irq, indio_dev);
	iio_device_free(indio_dev);

	return err;
}

static int lps331ap_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct lps331ap_data *prs = iio_priv(indio_dev);

	if (prs->lps_irq)
		free_irq(prs->lps_irq, indio_dev);

	lps331ap_device_power_off(prs);

	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static const struct i2c_device_id lps331ap_id[] = {
	{ LPS331AP_PRS_DEV_NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, lps331ap_id);

#ifdef CONFIG_OF
static const struct of_device_id lps331ap_of_match[] = {
	{ .compatible = "st,lps331ap" },
	{ .compatible = "lps331ap" },
	{ }
};
#endif

static struct i2c_driver lps331ap_driver = {
	.driver = {
		.name = LPS331AP_PRS_DEV_NAME,
		.of_match_table = of_match_ptr(lps331ap_of_match),
	},
	.probe = lps331ap_probe,
	.remove = lps331ap_remove,
	.id_table = lps331ap_id,
};

module_i2c_driver(lps331ap_driver);

MODULE_DESCRIPTION("STMicrolectronics LPS331AP pressure sensor IIO driver");
MODULE_AUTHOR("Jacek Anaszewski <j.anaszewski@samsung.com>");
MODULE_AUTHOR("Matteo Dameno, STMicroelectronic <matteo.dameno@st.com>");
MODULE_AUTHOR("Carmine Iascone, STMicroelectronic <carmine.iascone@st.com>");
MODULE_LICENSE("GPL");
