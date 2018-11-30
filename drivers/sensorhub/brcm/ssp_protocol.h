/*
 *  Copyright (C) 2017, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#ifndef __SSP_PROTOCOL_H__
#define __SSP_PROTOCOL_H__

#include <linux/bitops.h>

#define MSG2SSP_AP_WHOAMI				0x01
#define MSG2SSP_AP_FIRMWARE_REV			0x02
#define MSG2SSP_AP_SENSOR_SCANNING		0x03

/* Factory Test cmd */
#define ACCELEROMETER_FACTORY		0x06
#define GYROSCOPE_FACTORY			0x07
#define PRESSURE_FACTORY			0x08
#define GYROSCOPE_TEMP_FACTORY		0x09
#define GYROSCOPE_DPS_FACTORY		0x0A
#define HRM_FACTORY					0x0B
#define MCU_FACTORY					0x0C
#define MCU_SLEEP_FACTORY			0x0D
#define HRM_G_IR_LED     			0x16
#define HRM_B_R_LED     			0x17
#define HRM_NOISE       			0x18

#define HRM_LIB_VERSION_INFO		0x11
#define HRM_IR_LEVEL_THRESHOLD		0x12
#define MSG2SSP_SENSOR_REG_DUMP		0x13
#define MSG2SSP_AP_BATT_DISCHG_ADC	0x14
#ifdef CONFIG_SSP_RTC
#define MSG2SSP_AP_RTC_DIFF			0x15
#endif

#define MSG2SSP_INST_BYPASS_SENSOR_ADD		0x32
#define MSG2SSP_INST_BYPASS_SENSOR_REMOVE	0x33
#define MSG2SSP_INST_CHANGE_DELAY			0x34
#define MSG2SSP_INST_LIB_DATA				0x36
#define MSG2SSP_INST_LIBRARY_ADD			0x37
#define MSG2SSP_INST_LIBRARY_REMOVE			0x38

#define MSG2SSP_AP_MCU_GET_TIME			0x0E
#define MSG2SSP_AP_MCU_SET_GYRO_CAL		0x2B
#define MSG2SSP_AP_MCU_SET_ACCEL_CAL	0x2C
#define MSG2SSP_AP_MCU_SET_HRM_OSC_REG	0x2D
#define MSG2SSP_AP_MCU_SET_BARO_CAL		0x2E
#define MSG2SSP_AP_MCU_SET_TIME			0x3B

#define MSG2SSP_AP_STATUS_WAKEUP			0x1E
#define MSG2SSP_AP_STATUS_SHUTDOWN			0x1F
#define MSG2SSP_AP_STATUS_SLEEP				0x20
#define MSG2SSP_AP_STATUS_RESUME			0x21
#define MSG2SSP_AP_STATUS_SUSPEND			0x22
#define MSG2SSP_AP_STATUS_POW_CONNECTED		0x24
#define MSG2SSP_AP_STATUS_POW_DISCONNECTED	0x25

#define MSG2SSP_AP_SENSOR_LPF			0x23
#define MSG2SSP_AP_SENSOR_FORMATION		0x28
#ifdef CONFIG_INPUT_FF_MEMLESS_NOTIFY
#define MSG2SSP_AP_MCU_SET_MOTOR_STATUS		0x29
#endif
#define MSG2SSP_AP_GET_BIG_DATA			0x05
#define MSG2SSP_AP_SET_BIG_DATA			0x39
#define MSG2SSP_AP_START_BIG_DATA		0x3A

/* MAX_INST_VALUE highly depends on SensorhubFW */
#define MSG2SSP_MAX_INST_VALUE			0x3F

/* WOM start instruction */
#define MSG2SSP_INST_WOM_IMG_INFO			0x26
#define MSG2SSP_INST_WOM_START				0x27

/* SSP_INSTRUCTION_CMD */
enum {
	REMOVE_SENSOR = 0,
	ADD_SENSOR,
	CHANGE_DELAY,
	GO_SLEEP,
	REMOVE_LIBRARY,
	ADD_LIBRARY,
	EXT_CMD = 10,
};

/* Used for TizenFW */

/* HUB_STATUS_NOTI for ssp_state */
enum {
	EVT2FW_HUB_STATE_NOT_READY = -1,
	EVT2FW_HUB_STATE_READY_DONE,
	EVT2FW_HUB_STATE_IN_RESET,

  EVT2FW_HUB_STATE_WOM_RESET,
	EVT2FW_HUB_STATE_WOM_READY_DONE,

	EVT2FW_HUB_STATE_MAX,
};

#define EVT2FW_HUB_STATE_MAX_BITS_SZ \
	(BITS_PER_BYTE * DIV_ROUND_UP(EVT2FW_HUB_STATE_MAX, BITS_PER_BYTE))

#define MSG2SSP_AP_STATUS_RESET			0xD5
#define MSG2SSP_HUB_STATUS_NOTI			0xD6
#define MSG2SSP_INST_LIB_NOTI			0xB4

#endif /* __SSP_PROTOCOL_H__ */
