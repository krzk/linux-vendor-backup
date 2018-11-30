#ifndef __ZTM620_MOTOR_H__
#define __ZTM620_MOTOR_H__
/*
** =============================================================================
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     ztm620_motor.h
**
** Description:
**     Header file for ztm620_motor.c
**
** =============================================================================
*/

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>


#define HAPTICS_DEVICE_NAME "ztm620_motor"

#define	MOTOR_REG_MODE_00		0x00
#define	MOTOR_REG_MODE_01		0x01
#define	MOTOR_REG_SOFT_EN		0x10
#define	MOTOR_REG_STRENGTH		0x11
#define	MOTOR_REG_ADC_SAMPLING_TIME	0x12
#define	MOTOR_REG_MODE_13		0x13
#define	MOTOR_REG_OVER_DRV		0x14
#define	MOTOR_REG_START_STRENGTH	0x19
#define	MOTOR_REG_SEARCH_DRV_RATIO1	0x1A
#define	MOTOR_REG_SEARCH_DRV_RATIO2	0x1B
#define	MOTOR_REG_SEARCH_DRV_RATIO3	0x1C
#define	MOTOR_REG_DRV_FREQ_H		0x1F
#define	MOTOR_REG_DRV_FREQ_L		0x20
#define	MOTOR_REG_RESO_FREQ_H		0x21
#define	MOTOR_REG_RESO_FREQ_L		0x22
#define	MOTOR_REG_FUNC_ENABLE		0x23
#define	MOTOR_REG_OUTPUT		0x24

#define	MOTOR_REG_MODE_00_MASK		0x17
#define	MODE_00_PWM			0x14
#define	MODE_00_I2C			0x15
#define	MODE_00_STOP			0x16

#define	MOTOR_REG_MODE_01_MASK		0x1B
#define MODE_01_SHIFT_DRV_MODE		0x04
#define MODE_01_SHIFT_NBREAK_EN		0x03
#define MODE_01_SHIFT_PGA_BEMP_GAIN	0x00
#define MODE_01_DEFAULT_PGA_BEMP_GAIN	0x02

#define	MOTOR_REG_SOFT_EN_MASK		0x01
#define SOFT_DISABLE			0x00
#define SOFT_ENABLE			0x01

#define	MOTOR_REG_STRENGTH_MASK		0x7F

#define	MOTOR_REG_MODE_13_MASK		0xF9
#define MODE_13_SHIFT_RESO_CAL_MD	0x07
#define MODE_13_SHIFT_RESO_CAL_EN	0x06
#define MODE_13_SHIFT_OUT_PWM_FREQ	0x05
#define MODE_13_SHIFT_SIN_OUT_EN	0x04
#define MODE_13_SHIFT_PWR_CAL_EN	0x03
#define MODE_13_SHIFT_ERM_NLRA		0x00

#define MAX_LEVEL 0xffff
#define DEFAULT_MOTOR_FREQ 205
#define DEFAULT_MOTOR_STRENGTH 0x56
#define DEFAULT_BRAKE_DELAY 0
#define DEFAULT_ADC_SAMPLING_TIME 0x86
#define DEFAULT_SOFT_EN_DELAY 0
#define MOTOR_VCC 3000000
#define MOTOR_CLK 25000000

enum VIBRATOR_CONTROL {
	VIBRATOR_DISABLE = 0,
	VIBRATOR_ENABLE = 1,
};

enum actuator_type {
	ACTUATOR_LRA = 0,
	ACTUATOR_ERM = 1
};

enum loop_type {
	OPEN_LOOP = 0,
	CLOSED_LOOP = 1
};

struct reg_data {
	u32 addr;
	u32 data;
};

struct ztm620_motor_platform_data {
	struct reg_data *init_regs;
	int count_init_regs;
	enum actuator_type motor_type;
	enum loop_type	meLoop;
	bool break_mode;
	int break_delay;
	int gpio_en;
	const char *regulator_name;
	int adc_sampling_time;
	int soft_en_delay;
	int freq_strong;
	int freq_weak;
	int strength_strong;
	int strength_weak;
};

struct ztm620_motor_data {
	struct ztm620_motor_platform_data msPlatData;
	unsigned char mnDeviceID;
	struct device *dev;
	struct regmap *mpRegmap;

	struct wake_lock wklock;
	struct mutex lock;
	struct work_struct vibrator_work;
	struct work_struct delay_en_off;
	struct work_struct trigger_init;
	struct timespec last_motor_off;

	/* using FF_input_device */
	__u16 level;
	bool running;
	struct regulator *regulator;
	int gpio_en;
};

int ztm620_motor_reset_handler(void);

#endif
