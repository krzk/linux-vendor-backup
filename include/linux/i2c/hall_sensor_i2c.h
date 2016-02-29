/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Joong-Mock Shin <jmock.shin@samsung.com>
 *	Taeheon Kim <th908.kim@samsung.com>
 *	Sangmin Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

 #ifndef _LINUX_HALL_SENSOR_I2C_H
#define _LINUX_HALL_SENSOR_I2C_H

#define I2C_HALL_NAME "tizen_detent"
#define BUFF_SIZE_MAX	16

/* ********************************************************* */
/* property of driver */
/* ********************************************************* */
#define HALL_DRIVER_NAME				I2C_HALL_NAME
#define HALL_IRQ_A_NAME				I2C_HALL_A_NAME
#define HALL_IRQ_B_NAME				I2C_HALL_B_NAME
#define HALL_IRQ_C_NAME				I2C_HALL_C_NAME
#define HALL_PATH					"/dev/m1120"
#define HALL_A_SLAVE_ADDR				0x0c
#define HALL_B_SLAVE_ADDR				0x0c
#define HALL_C_SLAVE_ADDR				0x0c
/* ********************************************************* */

/* ********************************************************* */
/* register map */
/* ********************************************************* */
#define HALL_REG_NUM					(15)
#define HALL_REG_PERSINT				(0x00)
#define HALL_VAL_PERSINT_COUNT(n)			(n<<4)
#define HALL_VAL_PERSINT_INTCLR			(0x01)
/*
	[7:4]	PERS		: interrupt persistence count
	[0]	INTCLR	= 1	: interrupt clear
*/
/* --------------------------------------------------------- */
#define HALL_REG_INTSRS				(0x01)
#define HALL_VAL_INTSRS_INT_ON			(0x80)
#define HALL_DETECTION_MODE_INTERRUPT		HALL_VAL_INTSRS_INT_ON
#define HALL_VAL_INTSRS_INT_OFF			(0x00)
#define HALL_DETECTION_MODE_POLLING			HALL_VAL_INTSRS_INT_OFF
#define HALL_VAL_INTSRS_INTTYPE_BESIDE		(0x00)
#define HALL_VAL_INTSRS_INTTYPE_WITHIN		(0x10)
#define HALL_VAL_INTSRS_SRS_10BIT_0_068mT		(0x00)
#define HALL_VAL_INTSRS_SRS_10BIT_0_034mT		(0x01)
#define HALL_VAL_INTSRS_SRS_10BIT_0_017mT		(0x02)
#define HALL_VAL_INTSRS_SRS_10BIT_0_009mT		(0x03)
#define HALL_VAL_INTSRS_SRS_10BIT_0_004mT		(0x04)
#define HALL_VAL_INTSRS_SRS_8BIT_0_272mT		(0x00)
#define HALL_VAL_INTSRS_SRS_8BIT_0_136mT		(0x01)
#define HALL_VAL_INTSRS_SRS_8BIT_0_068mT		(0x02)
#define HALL_VAL_INTSRS_SRS_8BIT_0_036mT		(0x03)
#define HALL_VAL_INTSRS_SRS_8BIT_0_016mT		(0x04)
/*
	[7]		INTON	= 0 : disable interrupt
	[7]		INTON	= 1 : enable interrupt
	[4]		INT_TYP = 0 : generate inteerupt when raw data is within range of threshold
	[4]		INT_TYP = 1 : generate interrupt when raw data is beside range of threshold
	[2:0]	SRS			: select sensitivity type when HALL_VAL_OPF_BIT_10
				000		: 0.068 (mT/LSB)
				001		: 0.034 (mT/LSB)
				010		: 0.017 (mT/LSB)
				011		: 0.009 (mT/LSB)
				100		: 0.004 (mT/LSB)
				101		: 0.017 (mT/LSB)
				110		: 0.017 (mT/LSB)
				111		: 0.017 (mT/LSB)
	[2:0]	SRS			: select sensitivity type when HALL_VAL_OPF_BIT_8
				000		: 0.272 (mT/LSB)
				001		: 0.136 (mT/LSB)
				010		: 0.068 (mT/LSB)
				011		: 0.036 (mT/LSB)
				100		: 0.016 (mT/LSB)
				101		: 0.068 (mT/LSB)
				110		: 0.068 (mT/LSB)
				111		: 0.068 (mT/LSB)
*/
/* --------------------------------------------------------- */
#define HALL_REG_LTHL					(0x02)
/*
	[7:0]	LTHL		: low byte of low threshold value
*/
/* --------------------------------------------------------- */
#define HALL_REG_LTHH					(0x03)
/*
	[7:6]	LTHH		: high 2bits of low threshold value with sign
*/
/* --------------------------------------------------------- */
#define HALL_REG_HTHL					(0x04)
/*
	[7:0]	HTHL		: low byte of high threshold value
*/
/* --------------------------------------------------------- */
#define HALL_REG_HTHH					(0x05)
/*
	[7:6]	HTHH		: high 2bits of high threshold value with sign
*/
/* --------------------------------------------------------- */
#define HALL_REG_I2CDIS					(0x06)
#define HALL_VAL_I2CDISABLE				(0x37)
/*
	[7:0]	I2CDIS		: disable i2c
*/
/* --------------------------------------------------------- */
#define HALL_REG_SRST					(0x07)
#define HALL_VAL_SRST_RESET				(0x01)
/*
	[0]		SRST	= 1	: soft reset
*/
/* --------------------------------------------------------- */
#define HALL_REG_OPF					(0x08)
#define HALL_VAL_OPF_FREQ_20HZ			(0x00)
#define HALL_VAL_OPF_FREQ_10HZ			(0x10)
#define HALL_VAL_OPF_FREQ_6_7HZ			(0x20)
#define HALL_VAL_OPF_FREQ_5HZ				(0x30)
#define HALL_VAL_OPF_FREQ_80HZ			(0x40)
#define HALL_VAL_OPF_FREQ_40HZ			(0x50)
#define HALL_VAL_OPF_FREQ_26_7HZ			(0x60)
#define HALL_VAL_OPF_EFRD_ON				(0x08)
#define HALL_VAL_OPF_BIT_8				(0x02)
#define HALL_VAL_OPF_BIT_10				(0x00)
#define HALL_VAL_OPF_HSSON_ON				(0x01)
/*
	[6:4]	OPF		: operation frequency
			000	: 20	(Hz)
			001	: 10	(Hz)
			010	: 6.7	(Hz)
			011	: 5	(Hz)
			100	: 80	(Hz)
			101	: 40	(Hz)
			110	: 26.7	(Hz)
			111	: 20	(Hz)
	[3]	EFRD	= 0	: keep data without accessing eFuse
	[3]	EFRD	= 1	: update data after accessing eFuse
	[1]	BIT	= 0	: 10 bit resolution
	[1]	BIT	= 1	: 8 bit resolution
	[0]	HSSON	= 0	: Off power down mode
	[0]	HSSON	= 1	: On power down mode
*/
/* --------------------------------------------------------- */
#define HALL_REG_DID						(0x09)
#define HALL_VAL_DID						(0x9C)
/*
	[7:0]	DID			: Device ID
*/
/* --------------------------------------------------------- */
#define HALL_REG_INFO						(0x0A)
/*
	[7:0]	INFO		: Information about IC
*/
/* --------------------------------------------------------- */
#define HALL_REG_ASA						(0x0B)
/*
	[7:0]	ASA			: Hall Sensor sensitivity adjustment
*/
/* --------------------------------------------------------- */
#define HALL_REG_ST1						(0x10)
#define HALL_VAL_ST1_DRDY					(0x01)
/*
	[4]	INTM			: status of interrupt mode
	[1]	BITM			: status of resolution
	[0]	DRDY			: status of data ready
*/
/* --------------------------------------------------------- */
#define HALL_REG_HSL						(0x11)
/*
	[7:0]	HSL			: low byte of hall sensor measurement data
*/
/* --------------------------------------------------------- */
#define HALL_REG_HSH						(0x12)
/*
	[7:6]	HSL			: high 2bits of hall sensor measurement data with sign
*/
/* ********************************************************* */
/* ioctl command */
/* ********************************************************* */
#define HALL_IOCTL_BASE				(0x80)
#define HALL_IOCTL_SET_ENABLE			_IOW(HALL_IOCTL_BASE, 0x00, int)
#define HALL_IOCTL_GET_ENABLE			_IOR(HALL_IOCTL_BASE, 0x01, int)
#define HALL_IOCTL_SET_DELAY			_IOW(HALL_IOCTL_BASE, 0x02, int)
#define HALL_IOCTL_GET_DELAY			_IOR(HALL_IOCTL_BASE, 0x03, int)
#define HALL_IOCTL_SET_CALIBRATION		_IOW(HALL_IOCTL_BASE, 0x04, int*)
#define HALL_IOCTL_GET_CALIBRATED_DATA	_IOR(HALL_IOCTL_BASE, 0x05, int*)
#define HALL_IOCTL_SET_INTERRUPT		_IOW(HALL_IOCTL_BASE, 0x06, unsigned int)
#define HALL_IOCTL_GET_INTERRUPT		_IOR(HALL_IOCTL_BASE, 0x07, unsigned int*)
#define HALL_IOCTL_SET_THRESHOLD_HIGH	_IOW(HALL_IOCTL_BASE, 0x08, unsigned int)
#define HALL_IOCTL_GET_THRESHOLD_HIGH	_IOR(HALL_IOCTL_BASE, 0x09, unsigned int*)
#define HALL_IOCTL_SET_THRESHOLD_LOW		_IOW(HALL_IOCTL_BASE, 0x0A, unsigned int)
#define HALL_IOCTL_GET_THRESHOLD_LOW		_IOR(HALL_IOCTL_BASE, 0x0B, unsigned int*)
#define HALL_IOCTL_SET_REG			_IOW(HALL_IOCTL_BASE, 0x0C, int)
#define HALL_IOCTL_GET_REG			_IOR(HALL_IOCTL_BASE, 0x0D, int)
/* ********************************************************* */


/* ********************************************************* */
/* event property */
/* ********************************************************* */
#define DEFAULT_EVENT_TYPE			EV_ABS
#define DEFAULT_EVENT_CODE			ABS_X
#define DEFAULT_EVENT_DATA_CAPABILITY_MIN	(-32768)
#define DEFAULT_EVENT_DATA_CAPABILITY_MAX	(32767)
/* ********************************************************* */
/* delay property */
/* ********************************************************* */
#define HALL_DELAY_MAX				(200)	// ms
#define HALL_DELAY_MIN				(20)	// ms
#define HALL_DELAY_FOR_READY			(10)	// ms
/* ********************************************************* */


/* ********************************************************* */
/* data type for driver */
/* ********************************************************* */

enum {
	OPERATION_MODE_POWERDOWN,
	OPERATION_MODE_MEASUREMENT,
	OPERATION_MODE_FUSEROMACCESS
};

enum direction_patten {
	CounterClockwise_Detent = -2,
	CounterClockwise_Leave = -1,
	Clockwise_Leave = 1,
	Clockwise_Detent = 2,
	Return_Detent = 3,
	Direction_MAX,
};

enum hall_id {
	HALL_A,
	HALL_B,
	HALL_C,
	HALL_ALL ,
};

struct map_t{
	unsigned char persint;
	unsigned char intsrs;
	unsigned char lthl;
	unsigned char lthh;
	unsigned char hthl;
	unsigned char hthh;
	unsigned char i2cdis;
	unsigned char srst;
	unsigned char opf;
	unsigned char did;
	unsigned char info;
	unsigned char asa;
	unsigned char st1;
	unsigned char hsl;
	unsigned char hsh;
};

union hall_reg {
	struct map_t map;
	unsigned char array[HALL_REG_NUM];
};

struct hall_mutex {
	struct mutex	enable;
	struct mutex	data;
};

struct hall_atomic {
	atomic_t enable[HALL_ALL];
	atomic_t delay;
	atomic_t debug;
};

struct hall_platform_data {
	int	power_vi2c;
	int	power_vdd;
	int	int_a_gpio;
	int	int_b_gpio;
	int	int_c_gpio;
};

struct hall_ddata {
	struct	device *sec_dev;
	struct	i2c_client *client;
	struct	hall_platform_data *pdata;
	struct	input_dev	*input_dev;
	struct	hall_mutex	mtx;
	struct	hall_atomic	atm;
	union	hall_reg	reg;
	bool	irq_enabled;
	int	calibrated_data;
	int	last_data;
	int	last_id;
	int	last_detent_id;
	short	thrhigh;
	short	thrlow;
	bool	irq_first;
	bool	probe_done;

	int	power_vi2c;
	int	power_vdd;
	int	igpio[HALL_ALL];
	int	irq[HALL_ALL];
};
#endif
