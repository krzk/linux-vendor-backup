/*
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 * Author: Jacek Anaszewski <j.anaszewski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#define GP2A_I2C_NAME "gp2ap002a00f"

/* Registers */
#define OP_REG			0x00 /* Basic operations */
#define ALS_REG			0x01 /* ALS related settings */
#define PS_REG			0x02 /* PS related settings */
#define LED_REG			0x03 /* LED reg */
#define TL_L_REG		0x04 /* ALS: Threshold low LSB */
#define TL_H_REG		0x05 /* ALS: Threshold low MSB */
#define TH_L_REG		0x06 /* ALS: Threshold high LSB */
#define TH_H_REG		0x07 /* ALS: Threshold high MSB */
#define PL_L_REG		0x08 /* PS: Threshold low LSB */
#define PL_H_REG		0x09 /* PS: Threshold low MSB */
#define PH_L_REG		0x0a /* PS: Threshold high LSB */
#define PH_H_REG		0x0b /* PS: Threshold high MSB */
#define D0_L_REG		0x0c /* ALS result: Clear/Illuminance LSB */
#define D0_H_REG		0x0d /* ALS result: Clear/Illuminance MSB */
#define D1_L_REG		0x0e /* ALS result: IR LSB */
#define D1_H_REG		0x0f /* ALS result: IR LSB */
#define D2_L_REG		0x10 /* PS result LSB */
#define D2_H_REG		0x11 /* PS result MSB */
#define REGS_NUM		0x12 /* Number of registers */

/* OP_REG bits */
#define OP3_MASK		0x80 /* Software shutdown */
#define OP3_SHUTDOWN		0x00
#define OP3_OPERATION		0x80
#define OP2_MASK		0x40 /* Auto shutdown/Continuous operation  */
#define OP2_AUTO_SHUTDOWN	0x00
#define OP2_CONT_OPERATION	0x40
#define OP_MASK			0x30 /* Operating mode selection  */
#define OP_ALS_AND_PS		0x00
#define OP_ALS			0x10
#define OP_PS			0x20
#define OP_DEBUG		0x30
#define PROX_MASK		0x08 /* PS: detection/non-detection  */
#define PROX_NON_DETECT		0x00
#define PROX_DETECT		0x08
#define FLAG_P			0x04 /* PS: interrupt result  */
#define FLAG_A			0x02 /* ALS: interrupt result  */
#define TYPE_MASK		0x01 /* Output data type selection */
#define TYPE_MANUAL_CALC	0x00
#define TYPE_AUTO_CALC		0x01

/* ALS_REG bits */
#define PRST_MASK		0xc0 /* Number of measurement cycles */
#define PRST_ONCE		0x00
#define PRST_4_CYCLES		0x40
#define PRST_8_CYCLES		0x80
#define PRST_16_CYCLES		0xc0
#define RES_A_MASK		0x38 /* ALS: Resolution (0.39ms - 800ms) */
#define RES_A_800ms		0x00
#define RES_A_400ms		0x08
#define RES_A_200ms		0x10
#define RES_A_100ms		0x18
#define RES_A_25ms		0x20
#define RES_A_6_25ms		0x28
#define RES_A_1_56ms		0x30
#define RES_A_0_39ms		0x38
#define RANGE_A_MASK		0x07 /* ALS: Max measurable range (x1 - x128) */
#define RANGE_A_x1		0x00
#define RANGE_A_x2		0x01
#define RANGE_A_x4		0x02
#define RANGE_A_x8		0x03
#define RANGE_A_x16		0x04
#define RANGE_A_x32		0x05
#define RANGE_A_x64		0x06
#define RANGE_A_x128		0x07

/* PS_REG bits */
#define ALC_MASK		0x80 /* Auto light cancel */
#define ALC_ON			0x80
#define ALC_OFF			0x00
#define INTTYPE_MASK		0x40 /* Interrupt type setting */
#define INTTYPE_LEVEL		0x00
#define INTTYPE_PULSE		0x40
#define RES_P_MASK		0x38 /* PS: Resolution (0.39ms - 800ms)  */
#define RES_P_800ms_x2		0x00
#define RES_P_400ms_x2		0x08
#define RES_P_200ms_x2		0x10
#define RES_P_100ms_x2		0x18
#define RES_P_25ms_x2		0x20
#define RES_P_6_25ms_x2		0x28
#define RES_P_1_56ms_x2		0x30
#define RES_P_0_39ms_x2		0x38
#define RANGE_P_MASK		0x07 /* PS: Max measurable range (x1 - x128) */
#define RANGE_P_x1		0x00
#define RANGE_P_x2		0x01
#define RANGE_P_x4		0x02
#define RANGE_P_x8		0x03
#define RANGE_P_x16		0x04
#define RANGE_P_x32		0x05
#define RANGE_P_x64		0x06
#define RANGE_P_x128		0x07

/* LED reg bits */
#define INTVAL_MASK		0xc0 /* Intermittent operating */
#define INTVAL_0		0x00
#define INTVAL_4		0x40
#define INTVAL_8		0x80
#define INTVAL_16		0xc0
#define IS_MASK			0x30 /* ILED drive peak current setting  */
#define IS_13_8mA		0x00
#define IS_27_5mA		0x10
#define IS_55mA			0x20
#define IS_110mA		0x30
#define PIN_MASK		0x0c /* INT terminal setting */
#define PIN_ALS_OR_PS		0x00
#define PIN_ALS			0x04
#define PIN_PS			0x08
#define PIN_PS_DETECT		0x0c
#define FREQ_MASK		0x02 /* LED modulation frequency */
#define FREQ_327_5kHz		0x00
#define FREQ_81_8kHz		0x02
#define RST			0x01 /* Software reset */

#define SCAN_MODE_LIGHT_CLEAR	0
#define SCAN_MODE_LIGHT_IR	1
#define SCAN_MODE_PROXIMITY	2
#define CHAN_TIMESTAMP		3

#define GP2AP002A00F_DATA_READY_TIMEOUT ((1000*HZ)/1000)

#define GP2AP002A00F_DATA_REG(chan) (D0_L_REG + (chan) * 2)

#define GP2AP002A00F_SUBTRACT_MODE	0
#define GP2AP002A00F_ADD_MODE		1

#define GP2AP002A00F_MAX_CHANNELS	3

enum gp2ap002a00f_opmode {
	GP2AP002A00F_OPMODE_READ_RAW_CLEAR,
	GP2AP002A00F_OPMODE_READ_RAW_IR,
	GP2AP002A00F_OPMODE_READ_RAW_PROXIMITY,
	GP2AP002A00F_OPMODE_ALS,
	GP2AP002A00F_OPMODE_PS,
	GP2AP002A00F_OPMODE_ALS_AND_PS,
	GP2AP002A00F_OPMODE_PROX_DETECT,
	GP2AP002A00F_OPMODE_SHUTDOWN,
};

enum gp2ap002a00f_cmd {
	GP2AP002A00F_CMD_READ_RAW_CLEAR,
	GP2AP002A00F_CMD_READ_RAW_IR,
	GP2AP002A00F_CMD_READ_RAW_PROXIMITY,
	GP2AP002A00F_CMD_TRIGGER_CLEAR_EN,
	GP2AP002A00F_CMD_TRIGGER_CLEAR_DIS,
	GP2AP002A00F_CMD_TRIGGER_IR_EN,
	GP2AP002A00F_CMD_TRIGGER_IR_DIS,
	GP2AP002A00F_CMD_TRIGGER_PROX_EN,
	GP2AP002A00F_CMD_TRIGGER_PROX_DIS,
	GP2AP002A00F_CMD_ALS_HIGH_EV_EN,
	GP2AP002A00F_CMD_ALS_HIGH_EV_DIS,
	GP2AP002A00F_CMD_ALS_LOW_EV_EN,
	GP2AP002A00F_CMD_ALS_LOW_EV_DIS,
	GP2AP002A00F_CMD_PROX_EV_EN,
	GP2AP002A00F_CMD_PROX_EV_DIS,
};

enum gp2ap002a00f_flags {
	GP2AP002A00F_FLAG_ALS_CLEAR_TRIGGER,
	GP2AP002A00F_FLAG_ALS_IR_TRIGGER,
	GP2AP002A00F_FLAG_PS_TRIGGER,
	GP2AP002A00F_FLAG_PS_RISING_EVENT,
	GP2AP002A00F_FLAG_ALS_RISING_EVENT,
	GP2AP002A00F_FLAG_ALS_FALLING_EVENT,
	GP2AP002A00F_FLAG_DATA_READY,
};

struct gp2ap002a00f_data {
	const struct gp2ap002a00f_platform_data *pdata;
	struct i2c_client *client;
	struct mutex lock;
	char *buffer;
	u8 reg_cache[REGS_NUM];
	struct regulator *vled_reg;
	unsigned long flags;
	enum gp2ap002a00f_opmode cur_opmode;
	wait_queue_head_t data_ready_queue;
};

static unsigned long gp2ap002a00f_available_scan_masks[] = {
	0x01,
	0x02,
	0x04,
};

static int gp2ap002a00f_set_operation_mode(struct gp2ap002a00f_data *data,
					enum gp2ap002a00f_opmode op)
{
	u8 prev_ctrl_regs[4];
	int err;

	prev_ctrl_regs[OP_REG] = data->reg_cache[OP_REG];
	prev_ctrl_regs[ALS_REG] = data->reg_cache[ALS_REG];
	prev_ctrl_regs[PS_REG] = data->reg_cache[PS_REG];
	prev_ctrl_regs[LED_REG] = data->reg_cache[LED_REG];

	data->reg_cache[OP_REG] &= ~(OP_MASK | OP2_MASK | OP3_MASK
				     | TYPE_MASK);
	data->reg_cache[ALS_REG] &= ~PRST_MASK;
	data->reg_cache[LED_REG] &= ~PIN_MASK;

	switch (op) {
	case GP2AP002A00F_OPMODE_READ_RAW_CLEAR:
	case GP2AP002A00F_OPMODE_READ_RAW_IR:
		data->reg_cache[OP_REG] |= (OP_ALS | OP2_AUTO_SHUTDOWN
					   | OP3_OPERATION | TYPE_MANUAL_CALC);
		data->reg_cache[ALS_REG] |= PRST_ONCE;
		data->reg_cache[LED_REG] |= PIN_ALS;
		break;
	case GP2AP002A00F_OPMODE_READ_RAW_PROXIMITY:
		data->reg_cache[OP_REG] |= (OP_PS | OP2_AUTO_SHUTDOWN
					   | OP3_OPERATION | TYPE_MANUAL_CALC);
		data->reg_cache[ALS_REG] |= PRST_ONCE;
		data->reg_cache[LED_REG] |= PIN_PS;
		break;
	case GP2AP002A00F_OPMODE_PROX_DETECT:
		data->reg_cache[OP_REG] |= (OP_PS | OP2_CONT_OPERATION
					   | OP3_OPERATION | TYPE_MANUAL_CALC);
		data->reg_cache[ALS_REG] |= PRST_4_CYCLES;
		data->reg_cache[LED_REG] |= PIN_PS_DETECT;
		break;
	case GP2AP002A00F_OPMODE_ALS:
		data->reg_cache[OP_REG] |= (OP_ALS | OP2_CONT_OPERATION
					   | OP3_OPERATION | TYPE_MANUAL_CALC);
		data->reg_cache[ALS_REG] |= PRST_ONCE;
		data->reg_cache[LED_REG] |= PIN_ALS;
		break;
	case GP2AP002A00F_OPMODE_PS:
		data->reg_cache[OP_REG] |= (OP_PS | OP2_CONT_OPERATION
					   | OP3_OPERATION | TYPE_MANUAL_CALC);
		data->reg_cache[ALS_REG] |= PRST_4_CYCLES;
		data->reg_cache[LED_REG] |= PIN_PS;
		break;
	case GP2AP002A00F_OPMODE_ALS_AND_PS:
		data->reg_cache[OP_REG] |= (OP_ALS_AND_PS | OP2_CONT_OPERATION
					   | OP3_OPERATION | TYPE_MANUAL_CALC);
		data->reg_cache[ALS_REG] |= PRST_4_CYCLES;
		data->reg_cache[LED_REG] |= PIN_ALS_OR_PS;
		break;
	case GP2AP002A00F_OPMODE_SHUTDOWN:
		/*
		 * Bring back last OP state to avoid sending shutdown
		 * command twice due to spurious op mode transition.
		 */
		data->reg_cache[OP_REG] = prev_ctrl_regs[OP_REG]
					& OP_MASK;
		break;
	}

	/*
	 * Shutdown the device if the operation being executed entails
	 * mode transition.
	 */
	if ((data->reg_cache[OP_REG] & OP_MASK) !=
	    (prev_ctrl_regs[OP_REG] & OP_MASK)) {
		/* set shutdown mode */
		err = i2c_smbus_write_byte_data(data->client, OP_REG, 0);
		if (err < 0)
			goto error_ret;
	}

	err = i2c_smbus_write_i2c_block_data(data->client, ALS_REG, 3,
						&data->reg_cache[ALS_REG]);
	if (err < 0)
		goto error_ret;

	/* Set OP_REG and apply operation mode (power on / off) */
	err = i2c_smbus_write_byte_data(data->client, OP_REG,
						data->reg_cache[OP_REG]);
	if (err < 0)
		goto error_ret;

	data->cur_opmode = op;

	return 0;

error_ret:
	data->reg_cache[OP_REG] = prev_ctrl_regs[OP_REG];
	data->reg_cache[ALS_REG] = prev_ctrl_regs[ALS_REG];
	data->reg_cache[PS_REG] = prev_ctrl_regs[PS_REG];
	data->reg_cache[LED_REG] = prev_ctrl_regs[LED_REG];

	return err;
}

static bool gp2ap002a00f_als_enabled(struct gp2ap002a00f_data *data)
{
	return test_bit(GP2AP002A00F_FLAG_ALS_CLEAR_TRIGGER,
							&data->flags) ||
	       test_bit(GP2AP002A00F_FLAG_ALS_IR_TRIGGER,
							&data->flags) ||
	       test_bit(GP2AP002A00F_FLAG_ALS_RISING_EVENT,
							&data->flags) ||
	       test_bit(GP2AP002A00F_FLAG_ALS_FALLING_EVENT,
							&data->flags);
}

static int gp2ap002a00f_alter_opmode(struct gp2ap002a00f_data *data,
			enum gp2ap002a00f_opmode diff_mode, int add_sub)
{
	enum gp2ap002a00f_opmode new_mode;
	int err;

	if (diff_mode != GP2AP002A00F_OPMODE_ALS &&
	    diff_mode != GP2AP002A00F_OPMODE_PS)
		return -EINVAL;

	if (add_sub == GP2AP002A00F_ADD_MODE) {
		if (data->cur_opmode == GP2AP002A00F_OPMODE_SHUTDOWN)
			new_mode =  diff_mode;
		else
			new_mode = GP2AP002A00F_OPMODE_ALS_AND_PS;
	} else {
		if (data->cur_opmode == GP2AP002A00F_OPMODE_ALS_AND_PS)
			new_mode =  diff_mode;
		else
			new_mode = GP2AP002A00F_OPMODE_SHUTDOWN;
	}

	err = gp2ap002a00f_set_operation_mode(data, new_mode);

	return err;
}

static int gp2ap002a00f_exec_cmd(struct gp2ap002a00f_data *data,
					enum gp2ap002a00f_cmd cmd)
{
	const u8 thresh_off_buf[2] = {0x00, 0x00};
	int err = 0;

	switch (cmd) {
	case GP2AP002A00F_CMD_READ_RAW_CLEAR:
		if (data->cur_opmode != GP2AP002A00F_OPMODE_SHUTDOWN)
			return -EBUSY;
		err = gp2ap002a00f_set_operation_mode(data,
					GP2AP002A00F_CMD_READ_RAW_CLEAR);
		break;
	case GP2AP002A00F_CMD_READ_RAW_IR:
		if (data->cur_opmode != GP2AP002A00F_OPMODE_SHUTDOWN)
			return -EBUSY;
		err = gp2ap002a00f_set_operation_mode(data,
					GP2AP002A00F_CMD_READ_RAW_IR);
		break;
	case GP2AP002A00F_CMD_READ_RAW_PROXIMITY:
		if (data->cur_opmode != GP2AP002A00F_OPMODE_SHUTDOWN)
			return -EBUSY;
		err = gp2ap002a00f_set_operation_mode(data,
					GP2AP002A00F_CMD_READ_RAW_PROXIMITY);
		break;
	case GP2AP002A00F_CMD_TRIGGER_CLEAR_EN:
		if (data->cur_opmode == GP2AP002A00F_OPMODE_PROX_DETECT)
			return -EBUSY;
		if (!gp2ap002a00f_als_enabled(data))
			err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_ADD_MODE);
		set_bit(GP2AP002A00F_FLAG_ALS_CLEAR_TRIGGER, &data->flags);
		break;
	case GP2AP002A00F_CMD_TRIGGER_CLEAR_DIS:
		clear_bit(GP2AP002A00F_FLAG_ALS_CLEAR_TRIGGER, &data->flags);
		if (gp2ap002a00f_als_enabled(data))
			break;
		err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_SUBTRACT_MODE);
		break;
	case GP2AP002A00F_CMD_TRIGGER_IR_EN:
		if (data->cur_opmode == GP2AP002A00F_OPMODE_PROX_DETECT)
			return -EBUSY;
		if (!gp2ap002a00f_als_enabled(data))
			err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_ADD_MODE);
		set_bit(GP2AP002A00F_FLAG_ALS_IR_TRIGGER, &data->flags);
		break;
	case GP2AP002A00F_CMD_TRIGGER_IR_DIS:
		clear_bit(GP2AP002A00F_FLAG_ALS_IR_TRIGGER, &data->flags);
		if (gp2ap002a00f_als_enabled(data))
			break;
		err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_SUBTRACT_MODE);
		break;
	case GP2AP002A00F_CMD_TRIGGER_PROX_EN:
		set_bit(GP2AP002A00F_FLAG_PS_TRIGGER, &data->flags);
		/*
		 * Don't change opmode if in PROX_DETECT, as
		 * it is compatible with PS mode.
		 */
		if (data->cur_opmode == GP2AP002A00F_OPMODE_PROX_DETECT)
			break;
		err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_PS,
						GP2AP002A00F_ADD_MODE);
		break;
	case GP2AP002A00F_CMD_TRIGGER_PROX_DIS:
		clear_bit(GP2AP002A00F_FLAG_PS_TRIGGER, &data->flags);
		if (test_bit(GP2AP002A00F_FLAG_PS_RISING_EVENT, &data->flags))
			break;
		err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_PS,
						GP2AP002A00F_SUBTRACT_MODE);
		break;
	case GP2AP002A00F_CMD_ALS_HIGH_EV_EN:
		if (data->cur_opmode == GP2AP002A00F_OPMODE_PROX_DETECT)
			return -EBUSY;
		if (!gp2ap002a00f_als_enabled(data)) {
			err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_ADD_MODE);
			if (err < 0)
				return err;
		}
		set_bit(GP2AP002A00F_FLAG_ALS_RISING_EVENT, &data->flags);
		err = i2c_smbus_write_i2c_block_data(data->client, TH_L_REG, 2,
						&data->reg_cache[TH_L_REG]);
		break;
	case GP2AP002A00F_CMD_ALS_HIGH_EV_DIS:
		clear_bit(GP2AP002A00F_FLAG_ALS_RISING_EVENT, &data->flags);
		if (!gp2ap002a00f_als_enabled(data)) {
			err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_SUBTRACT_MODE);
			if (err < 0)
				return err;
		}
		err = i2c_smbus_write_i2c_block_data(data->client, TH_L_REG, 2,
						thresh_off_buf);
		break;
	case GP2AP002A00F_CMD_ALS_LOW_EV_EN:
		if (data->cur_opmode == GP2AP002A00F_OPMODE_PROX_DETECT)
			return -EBUSY;
		if (!gp2ap002a00f_als_enabled(data)) {
			err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_ADD_MODE);
			if (err < 0)
				return err;
		}
		set_bit(GP2AP002A00F_FLAG_ALS_FALLING_EVENT, &data->flags);
		err = i2c_smbus_write_i2c_block_data(data->client, TL_L_REG, 2,
						&data->reg_cache[TL_L_REG]);
		break;
	case GP2AP002A00F_CMD_ALS_LOW_EV_DIS:
		clear_bit(GP2AP002A00F_FLAG_ALS_FALLING_EVENT, &data->flags);
		if (!gp2ap002a00f_als_enabled(data)) {
			err = gp2ap002a00f_alter_opmode(data,
						GP2AP002A00F_OPMODE_ALS,
						GP2AP002A00F_SUBTRACT_MODE);
			if (err < 0)
				return err;
		}
		err = i2c_smbus_write_i2c_block_data(data->client, TL_L_REG, 2,
						thresh_off_buf);
		break;
	case GP2AP002A00F_CMD_PROX_EV_EN:
		if (gp2ap002a00f_als_enabled(data))
			return -EBUSY;
		set_bit(GP2AP002A00F_FLAG_PS_RISING_EVENT, &data->flags);
		err = gp2ap002a00f_set_operation_mode(data,
					GP2AP002A00F_OPMODE_PROX_DETECT);
		if (err < 0)
			return err;
		err = i2c_smbus_write_i2c_block_data(data->client, PH_L_REG, 2,
						&data->reg_cache[PH_L_REG]);
		break;
	case GP2AP002A00F_CMD_PROX_EV_DIS:
		clear_bit(GP2AP002A00F_FLAG_PS_RISING_EVENT, &data->flags);
		if (test_bit(GP2AP002A00F_FLAG_PS_TRIGGER, &data->flags))
			err = gp2ap002a00f_set_operation_mode(data,
					GP2AP002A00F_OPMODE_PS);
		else
			err = gp2ap002a00f_set_operation_mode(data,
					GP2AP002A00F_OPMODE_SHUTDOWN);
		if (err < 0)
			return err;
		err = i2c_smbus_write_i2c_block_data(data->client, PH_L_REG, 2,
						thresh_off_buf);
		break;
	}

	return err;
}

static int gp2ap002a00f_get_reg_cache_word(struct gp2ap002a00f_data *data,
					u8 reg_addr)
{
	return (data->reg_cache[reg_addr + 1] << 8) |
		data->reg_cache[reg_addr];
}

/* Returns 0 if the end of conversion interrupt occured or -ETIME otherwise */
static int wait_conversion_complete_interrupt(struct gp2ap002a00f_data *data)
{
	int ret;

	ret = wait_event_timeout(data->data_ready_queue,
				 test_bit(GP2AP002A00F_FLAG_DATA_READY,
					  &data->flags),
				 GP2AP002A00F_DATA_READY_TIMEOUT);
	clear_bit(GP2AP002A00F_FLAG_DATA_READY, &data->flags);

	return ret > 0 ? 0 : -ETIME;
}

static int gp2ap002a00f_read_output(struct gp2ap002a00f_data *data,
					u8 output_reg, int *val)
{
	int err = -EINVAL;

	err = wait_conversion_complete_interrupt(data);
	if (err < 0)
		dev_dbg(&data->client->dev, "data ready timeout\n");

	err = i2c_smbus_read_i2c_block_data(data->client, output_reg, 2,
						&data->reg_cache[output_reg]);
	if (err < 0)
		return err;

	*val = gp2ap002a00f_get_reg_cache_word(data, output_reg);

	return err;
}

static irqreturn_t gp2ap002a00f_event_handler(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct gp2ap002a00f_data *lgt = iio_priv(indio_dev);
	u8 op_reg_val, op_reg_flags;
	int output_val, ret;

	/* Read interrupt flags */
	ret = i2c_smbus_read_i2c_block_data(lgt->client, OP_REG, 1,
						&op_reg_val);
	if (ret < 0)
		goto done;

	op_reg_flags = op_reg_val & (FLAG_A | FLAG_P | PROX_DETECT);

	/* Clear interrupt flags */
	op_reg_val &= (~FLAG_A & ~FLAG_P & ~PROX_DETECT);
	ret = i2c_smbus_write_i2c_block_data(lgt->client, OP_REG, 1,
						&op_reg_val);
	if (ret < 0)
		goto done;

	if (lgt->cur_opmode == GP2AP002A00F_OPMODE_READ_RAW_CLEAR ||
	    lgt->cur_opmode == GP2AP002A00F_OPMODE_READ_RAW_IR ||
	    lgt->cur_opmode == GP2AP002A00F_OPMODE_READ_RAW_PROXIMITY) {
		set_bit(GP2AP002A00F_FLAG_DATA_READY, &lgt->flags);
		wake_up(&lgt->data_ready_queue);
		goto done;
	}

	if ((test_bit(GP2AP002A00F_FLAG_ALS_RISING_EVENT, &lgt->flags) ||
	    test_bit(GP2AP002A00F_FLAG_ALS_FALLING_EVENT, &lgt->flags)) &&
	    (op_reg_flags & FLAG_A)) {
		/*
		 * We need to read output value to distinguish
		 * between high and low threshold event.
		 */
		ret = i2c_smbus_read_i2c_block_data(lgt->client, D0_L_REG, 2,
						&lgt->reg_cache[D0_L_REG]);
		if (ret < 0)
			goto done;

		output_val = gp2ap002a00f_get_reg_cache_word(lgt, D0_L_REG);

		if (output_val > gp2ap002a00f_get_reg_cache_word(lgt,
								 TH_L_REG))
			iio_push_event(indio_dev,
				       IIO_MOD_EVENT_CODE(
						IIO_LIGHT,
						SCAN_MODE_LIGHT_CLEAR,
						IIO_MOD_LIGHT_CLEAR,
						IIO_EV_TYPE_THRESH,
						IIO_EV_DIR_RISING),
				       iio_get_time_ns());
		else if (output_val < gp2ap002a00f_get_reg_cache_word(lgt,
								    TL_L_REG))
			iio_push_event(indio_dev,
				       IIO_MOD_EVENT_CODE(
						IIO_LIGHT,
						SCAN_MODE_LIGHT_CLEAR,
						IIO_MOD_LIGHT_CLEAR,
						IIO_EV_TYPE_THRESH,
						IIO_EV_DIR_FALLING),
				       iio_get_time_ns());
	}

	if (test_bit(GP2AP002A00F_FLAG_PS_RISING_EVENT, &lgt->flags) &&
		(op_reg_flags & FLAG_P)) {
		iio_push_event(indio_dev,
			       IIO_UNMOD_EVENT_CODE(
					IIO_PROXIMITY,
					SCAN_MODE_PROXIMITY,
					IIO_EV_TYPE_THRESH,
					IIO_EV_DIR_RISING),
			       iio_get_time_ns());
	}

done:
	return IRQ_HANDLED;
}

static irqreturn_t gp2ap002a00f_trigger_handler(int irq, void *data)
{
	struct iio_poll_func *pf = data;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct gp2ap002a00f_data *lgt = iio_priv(indio_dev);
	s64 time_ns;
	size_t d_size = 0;
	int i, ret;

	for_each_set_bit(i, indio_dev->active_scan_mask,
		indio_dev->masklength) {
		ret = i2c_smbus_read_i2c_block_data(lgt->client,
				GP2AP002A00F_DATA_REG(i), 2,
				&lgt->buffer[d_size]);
		if (ret < 0)
			goto done;
		d_size += 2;
	}

	if (indio_dev->scan_timestamp)
		d_size += sizeof(s64);

	lgt->buffer = kmalloc(d_size, GFP_KERNEL);
	if (lgt->buffer == NULL)
		goto done;

	time_ns = iio_get_time_ns();

	if (indio_dev->scan_timestamp)
		memcpy(lgt->buffer + d_size - sizeof(s64), &time_ns,
							sizeof(time_ns));

	iio_push_to_buffers(indio_dev, lgt->buffer);

	kfree(lgt->buffer);
done:
	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static int gp2ap002a00f_setup(struct gp2ap002a00f_data *data)
{
	int err = 0;

	data->reg_cache[OP_REG] = OP3_SHUTDOWN;
	data->reg_cache[LED_REG] = INTVAL_0 | IS_110mA | FREQ_327_5kHz;
	data->reg_cache[ALS_REG] = RES_A_25ms | RANGE_A_x8;
	data->reg_cache[PS_REG] = ALC_ON | INTTYPE_LEVEL | RES_P_1_56ms_x2
				  | RANGE_P_x4;

	err = i2c_smbus_write_i2c_block_data(data->client, OP_REG, REGS_NUM,
						&data->reg_cache[OP_REG]);

	return err;
}

static u8 get_reg_by_event_code(u64 event_code)
{
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);

	if (chan_type == IIO_PROXIMITY) {
		/*
		 * Only IIO_EV_DIR_RISING direction for the IIO_PROXIMITY
		 * event is supported by the driver.
		 */
		return PH_L_REG;
	} else if (chan_type == IIO_LIGHT) {
		if (IIO_EVENT_CODE_EXTRACT_DIR(event_code)
					== IIO_EV_DIR_RISING)
			return TH_L_REG;
		else
			return TL_L_REG;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int gp2ap002a00f_write_event_val(struct iio_dev *indio_dev,
					u64 event_code, int val)
{
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);
	bool event_en = false;
	int thresh_reg_l, err = 0;

	mutex_lock(&data->lock);

	thresh_reg_l = get_reg_by_event_code(event_code);

	if (thresh_reg_l > PH_L_REG) {
		err = -EINVAL;
		goto error_ret;
	}

	data->reg_cache[thresh_reg_l] = val & 0xff;
	data->reg_cache[thresh_reg_l + 1] = val >> 8;

	switch (thresh_reg_l) {
	case TH_L_REG:
		if (test_bit(GP2AP002A00F_FLAG_ALS_RISING_EVENT,
							&data->flags))
			event_en = true;
		break;
	case TL_L_REG:
		if (test_bit(GP2AP002A00F_FLAG_ALS_FALLING_EVENT,
							&data->flags))
			event_en = true;
		break;
	case PH_L_REG:
		if (test_bit(GP2AP002A00F_FLAG_PS_RISING_EVENT,
							&data->flags))
			event_en = true;
		break;
	}

	if (event_en)
		err = i2c_smbus_write_i2c_block_data(data->client,
					thresh_reg_l, 2,
					&data->reg_cache[thresh_reg_l]);
error_ret:
	mutex_unlock(&data->lock);

	return err;
}

static int gp2ap002a00f_read_event_val(struct iio_dev *indio_dev,
					u64 event_code, int *val)
{
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);
	int thresh_reg_l;

	mutex_lock(&data->lock);

	thresh_reg_l = get_reg_by_event_code(event_code);

	*val = gp2ap002a00f_get_reg_cache_word(data, thresh_reg_l);

	mutex_unlock(&data->lock);

	return 0;
}

static int gp2ap002a00f_write_event_config(struct iio_dev *indio_dev,
					u64 event_code, int state)
{
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);
	enum gp2ap002a00f_cmd cmd;
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	int err;

	mutex_lock(&data->lock);

	if (chan_type == IIO_PROXIMITY) {
		cmd = state ? GP2AP002A00F_CMD_PROX_EV_EN :
			      GP2AP002A00F_CMD_PROX_EV_DIS;
		err = gp2ap002a00f_exec_cmd(data, cmd);
	} else if (chan_type == IIO_LIGHT) {
		if (IIO_EVENT_CODE_EXTRACT_DIR(event_code)
					== IIO_EV_DIR_RISING) {
			cmd = state ? GP2AP002A00F_CMD_ALS_HIGH_EV_EN :
				      GP2AP002A00F_CMD_ALS_HIGH_EV_DIS;
			err = gp2ap002a00f_exec_cmd(data, cmd);
		} else {
			cmd = state ? GP2AP002A00F_CMD_ALS_LOW_EV_EN :
				      GP2AP002A00F_CMD_ALS_LOW_EV_DIS;
			err = gp2ap002a00f_exec_cmd(data, cmd);
		}
	} else {
		return -EINVAL;
	}

	mutex_unlock(&data->lock);

	return err;
}

static int gp2ap002a00f_read_event_config(struct iio_dev *indio_dev,
					u64 event_code)
{
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);
	int chan_type = IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code);
	int event_en;

	mutex_lock(&data->lock);

	if (chan_type == IIO_PROXIMITY) {
		event_en = test_bit(GP2AP002A00F_FLAG_PS_RISING_EVENT,
								&data->flags);
	} else if (chan_type == IIO_LIGHT) {
		if (IIO_EVENT_CODE_EXTRACT_DIR(event_code)
					== IIO_EV_DIR_RISING)
			event_en = test_bit(GP2AP002A00F_FLAG_ALS_RISING_EVENT,
								&data->flags);
		else
			event_en = test_bit(GP2AP002A00F_FLAG_ALS_FALLING_EVENT,
								&data->flags);
	} else {
		return -EINVAL;
	}

	mutex_unlock(&data->lock);

	return event_en;
}

static int gp2ap002a00f_read_channel(struct gp2ap002a00f_data *data,
				struct iio_chan_spec const *chan, int *val)
{
	enum gp2ap002a00f_cmd cmd;
	int err;

	switch (chan->scan_index) {
	case SCAN_MODE_LIGHT_CLEAR:
		cmd = GP2AP002A00F_CMD_READ_RAW_CLEAR;
		break;
	case SCAN_MODE_LIGHT_IR:
		cmd = GP2AP002A00F_CMD_READ_RAW_IR;
		break;
	case SCAN_MODE_PROXIMITY:
		cmd = GP2AP002A00F_CMD_READ_RAW_PROXIMITY;
		break;
	}

	err = gp2ap002a00f_exec_cmd(data, cmd);
	if (err < 0) {
		dev_err(&data->client->dev, "gp2ap002a00f_exec_cmd failed\n");
		goto error_ret;
	}

	err = gp2ap002a00f_read_output(data, chan->address, val);
	if (err < 0)
		dev_err(&data->client->dev, "gp2ap002a00f_read_output failed\n");

	data->cur_opmode = GP2AP002A00F_OPMODE_SHUTDOWN;

error_ret:
	return err;
}

static int gp2ap002a00f_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2,
			   long mask)
{
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);
	int err = -EINVAL;

	mutex_lock(&data->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (iio_buffer_enabled(indio_dev)) {
			err = -EBUSY;
			goto error_ret;
		}
		err = gp2ap002a00f_read_channel(data, chan, val);
		break;
	}

error_ret:
	mutex_unlock(&data->lock);

	return err < 0 ? err : IIO_VAL_INT;
}

static const struct iio_chan_spec gp2ap002a00f_channels[] = {
	{
		.type = IIO_LIGHT,
		.channel2 = IIO_MOD_LIGHT_CLEAR,
		.modified = 1,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.shift = 0,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.scan_index = SCAN_MODE_LIGHT_CLEAR,
		.address = D0_L_REG,
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_THRESH,
					 IIO_EV_DIR_RISING) |
			      IIO_EV_BIT(IIO_EV_TYPE_THRESH,
					 IIO_EV_DIR_FALLING),
	},
	{
		.type = IIO_LIGHT,
		.channel2 = IIO_MOD_LIGHT_IR,
		.modified = 1,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.shift = 0,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.scan_index = SCAN_MODE_LIGHT_IR,
		.address = D1_L_REG,
	},
	{
		.type = IIO_PROXIMITY,
		.modified = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.shift = 0,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.scan_index = SCAN_MODE_PROXIMITY,
		.address = D2_L_REG,
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_THRESH,
					 IIO_EV_DIR_RISING),
	},
	IIO_CHAN_SOFT_TIMESTAMP(CHAN_TIMESTAMP),
};

static const struct iio_info gp2ap002a00f_info = {
	.read_raw = &gp2ap002a00f_read_raw,
	.read_event_value = &gp2ap002a00f_read_event_val,
	.read_event_config = &gp2ap002a00f_read_event_config,
	.write_event_value = &gp2ap002a00f_write_event_val,
	.write_event_config = &gp2ap002a00f_write_event_config,
	.driver_module = THIS_MODULE,
};

static int gp2ap002a00f_buffer_preenable(struct iio_dev *indio_dev)
{
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);
	int i, err = 0;

	mutex_lock(&data->lock);

	/* Enable triggers according to the scan_mask */
	for_each_set_bit(i, indio_dev->active_scan_mask,
		indio_dev->masklength) {
		switch (i) {
		case SCAN_MODE_LIGHT_CLEAR:
			err = gp2ap002a00f_exec_cmd(data,
					GP2AP002A00F_CMD_TRIGGER_CLEAR_EN);
			if (err < 0)
				goto error_ret;
			break;
		case SCAN_MODE_LIGHT_IR:
			err = gp2ap002a00f_exec_cmd(data,
					GP2AP002A00F_CMD_TRIGGER_IR_EN);
			if (err < 0)
				goto error_ret;
			break;
		case SCAN_MODE_PROXIMITY:
			err = gp2ap002a00f_exec_cmd(data,
					GP2AP002A00F_CMD_TRIGGER_PROX_EN);
			if (err < 0)
				goto error_ret;
			break;
		}

	}

	err = iio_sw_buffer_preenable(indio_dev);

error_ret:
	mutex_unlock(&data->lock);

	return err;
}

static int gp2ap002a00f_buffer_predisable(struct iio_dev *indio_dev)
{
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);
	int err;

	clear_bit(GP2AP002A00F_FLAG_ALS_CLEAR_TRIGGER, &data->flags);
	clear_bit(GP2AP002A00F_FLAG_ALS_IR_TRIGGER, &data->flags);
	clear_bit(GP2AP002A00F_FLAG_PS_TRIGGER, &data->flags);

	err = gp2ap002a00f_exec_cmd(data, GP2AP002A00F_CMD_TRIGGER_CLEAR_DIS);
	if (err < 0)
		goto error_ret;

	err = gp2ap002a00f_exec_cmd(data, GP2AP002A00F_CMD_TRIGGER_IR_DIS);
	if (err < 0)
		goto error_ret;

	err = gp2ap002a00f_exec_cmd(data, GP2AP002A00F_CMD_TRIGGER_PROX_DIS);
	if (err < 0)
		goto error_ret;

	err = iio_triggered_buffer_predisable(indio_dev);
	if (err < 0)
		goto error_ret;

error_ret:
	return err;
}

static const struct iio_buffer_setup_ops gp2ap002a00f_buffer_setup_ops = {
	.preenable = &gp2ap002a00f_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &gp2ap002a00f_buffer_predisable,
};

static int gp2ap002a00f_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct gp2ap002a00f_data *data;
	struct iio_dev *indio_dev;
	int err;

	pr_info("gp2ap002a00f probe\n");

	/* Register with IIO */
	indio_dev = iio_device_alloc(sizeof(*data));
	if (indio_dev == NULL) {
		err = -ENOMEM;
		goto error_alloc;
	}

	data = iio_priv(indio_dev);

	data->vled_reg = devm_regulator_get(&client->dev, "vled");
	if (IS_ERR(data->vled_reg)) {
		err = PTR_ERR(data->vled_reg);
		goto error_regulator_get;
	}

	err = regulator_enable(data->vled_reg);
	if (err)
		goto error_free_data;

	i2c_set_clientdata(client, indio_dev);

	data->client = client;
	data->cur_opmode = GP2AP002A00F_OPMODE_SHUTDOWN;

	init_waitqueue_head(&data->data_ready_queue);

	err = gp2ap002a00f_setup(data);
	if (err < 0)
		goto error_free_data;

	mutex_init(&data->lock);
	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = gp2ap002a00f_channels;
	indio_dev->num_channels = ARRAY_SIZE(gp2ap002a00f_channels);
	indio_dev->info = &gp2ap002a00f_info;
	indio_dev->name = id->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->available_scan_masks = gp2ap002a00f_available_scan_masks;

	err = iio_triggered_buffer_setup(indio_dev, NULL,
		&gp2ap002a00f_trigger_handler, &gp2ap002a00f_buffer_setup_ops);

	err = devm_request_threaded_irq(&client->dev, client->irq,
				   &gp2ap002a00f_event_handler,
				   NULL,
				   IRQF_TRIGGER_FALLING,
				   "gp2ap002a00f_event",
				   indio_dev);
	if (err < 0)
		goto error_uninit_buffer;

	err = iio_device_register(indio_dev);
	if (err < 0)
		goto error_uninit_buffer;

	return 0;

error_uninit_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
error_free_data:
	regulator_disable(data->vled_reg);
error_regulator_get:
	iio_device_free(indio_dev);
error_alloc:
	return err;
}

static int gp2ap002a00f_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct gp2ap002a00f_data *data = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	regulator_disable(data->vled_reg);

	iio_device_free(indio_dev);

	return 0;
}

static const struct i2c_device_id gp2ap002a00f_id[] = {
	{ GP2A_I2C_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, gp2ap002a00f_id);

#ifdef CONFIG_OF
static const struct of_device_id gp2ap002a00f_of_match[] = {
	{ .compatible = "sharp,gp2ap002a00f" },
	{ .compatible = "gp2ap002a00f" },
	{ }
};
#endif

static struct i2c_driver gp2ap002a00f_driver = {
	.driver = {
		.name	= GP2A_I2C_NAME,
		.of_match_table = of_match_ptr(gp2ap002a00f_of_match),
		.owner	= THIS_MODULE,
	},
	.probe		= gp2ap002a00f_probe,
	.remove		= gp2ap002a00f_remove,
	.id_table	= gp2ap002a00f_id,
};

module_i2c_driver(gp2ap002a00f_driver);

MODULE_AUTHOR("Jacek Anaszewski <j.anaszewski@samsung.com>");
MODULE_DESCRIPTION("Sharp GP2AP002A00F I2C Proximity/Opto sensor driver");
MODULE_LICENSE("GPL v2");
