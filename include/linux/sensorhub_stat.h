/* include/linux/sensorhub_stat.h
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Junho Jang <vincent.jang@samsung.com>       <2015>                    *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.08.17>              *
 *                                                   Remove unnecessary code   *
 * ---- -------------------------------------------- ------------------------- *
 * ---- -------------------------------------------- ------------------------- *
 * 1.2   Junho Jang <vincent.jang@samsung.com>       <2018.03.20>              *
 *                                                   Minor refactoring   *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef __SENSORHUB_STAT_H
#define __SENSORHUB_STAT_H

#define SHUB_LIB_DATA_APDR	1
#define SHUB_LIB_DATA_CALL_POSE	2
#define SHUB_LIB_DATA_PEDOMETER	3
#define SHUB_LIB_DATA_MOTION	4
#define SHUB_LIB_DATA_APPROACH	5
#define SHUB_LIB_DATA_STEP_COUNT_ALERT	6
#define SHUB_LIB_DATA_AUTO_ROTATION	7
#define SHUB_LIB_DATA_MOVEMENT	8
#define SHUB_LIB_DATA_MOVEMENT_FOR_POSITIONING	9
#define SHUB_LIB_DATA_DIRECT_CALL	10
#define SHUB_LIB_DATA_STOP_ALERT	11
#define SHUB_LIB_DATA_ENVIRONMENT_SENSOR	12
#define SHUB_LIB_DATA_SHAKE_MOTION	13
#define SHUB_LIB_DATA_FLIP_COVER_ACTION	14
#define SHUB_LIB_DATA_GYRO_TEMPERATURE	15
#define SHUB_LIB_DATA_PUT_DOWN_MOTION	16
#define SHUB_LIB_DATA_BOUNCE_SHORT_MOTION	18
#define SHUB_LIB_DATA_WRIST_UP	19
#define SHUB_LIB_DATA_BOUNCE_LONG_MOTION	20
#define SHUB_LIB_DATA_FLAT_MOTION	21
#define SHUB_LIB_DATA_MOVEMENT_ALERT	22
#define SHUB_LIB_DATA_TEST_FLAT_MOTION	23
#define SHUB_LIB_DATA_SPECIFIC_POSE_ALERT	25
#define SHUB_LIB_DATA_ACTIVITY_TRACKER	26
#define SHUB_LIB_DATA_SLEEP_MONITOR	37
#define SHUB_LIB_DATA_CAPTURE_MOTION	39
#define SHUB_LIB_DATA_HRM_EX_COACH	40
#define SHUB_LIB_DATA_CALL_MOTION	41
#define SHUB_LIB_DATA_DOUBLE_TAP	42
#define SHUB_LIB_DATA_SIDE_PRESS	43
#define SHUB_LIB_DATA_SLMONITOR		44
#define SHUB_LIB_DATA_EXERCISE		45
#define SHUB_LIB_DATA_HEARTRATE		46
#define SHUB_LIB_DATA_LOCATION	    47
#define SHUB_LIB_DATA_AUTO_BRIGHTNESS    50
#define SHUB_LIB_DATA_WEAR_STATUS	52
#define SHUB_LIB_DATA_RESTING_HR	53
#define SHUB_LIB_DATA_WEARONOFF_MONITOR	54
#define SHUB_LIB_DATA_CYCLE_MONITOR 55
#define SHUB_LIB_DATA_NO_MOVEMENT_DETECTION 56
#define SHUB_LIB_DATA_ACTIVITY_LEVEL_MONITOR 57
#define SHUB_LIB_DATA_WRIST_DOWN 58
#define SHUB_LIB_DATA_ACTIVITY_SESSION_REC 59
#define SHUB_LIB_DATA_STRESS_TEST 60
#define SHUB_LIB_DATA_GIRO_CALIBRATION 61
#define SHUB_LIB_DATA_MCU_LOGGER 62
#define SHUB_LIB_MAX 64

#define DATAFRAME_GPS_TIME_START		3
#define DATAFRAME_GPS_TIME_END			6
#define DATAFRAME_GPS_LAST_USER_START	7
#define DATAFRAME_GPS_LAST_USER_END		14
#define DATAFRAME_GPS_EXT				15

struct sensorhub_stat_info {
	int wakeup[SHUB_LIB_MAX];
	int activity[SHUB_LIB_MAX];
	int event[SHUB_LIB_MAX];
};

struct sensorhub_gps_stat {
	int dataframe_size;
	int gps_time;
	unsigned long long last_gps_user;
	unsigned char last_gps_ext;
};

struct sensorhub_hr_stat {
	int success_cnt;
	int success_dur;
	int fail_cnt;
	int fail_dur;
};

/* Contains definitions for sensorhub resource tracking per sensorhub lib number. */
#ifdef CONFIG_SENSORHUB_STAT
int sensorhub_stat_rcv(const char *dataframe, int size);
int sensorhub_stat_get_stat(int type,
	struct sensorhub_stat_info *sh_stat);
int sensorhub_stat_get_gps_info(int type,
	struct sensorhub_gps_stat *sh_gps);
int sensorhub_stat_get_hr_info(int type,
	struct sensorhub_hr_stat *sh_hr);
int sensorhub_stat_is_wristup_wakeup(void);
#else
static inline int sensorhub_stat_rcv
				(const char *dataframe, int size) { return 0; }
static inline int sensorhub_stat_get_wakeup
				(int type, struct sensorhub_stat_info *sh_stat) { return 0; }
static inline int sensorhub_stat_get_gps_info
				(int type, struct sensorhub_gps_stat *sh_gps) { return 0; }
static inline int sensorhub_stat_get_hr_info
				(int type, struct sensorhub_hr_stat *sh_hr) { return 0; }
static inline int sensorhub_stat_is_wristup_wakeup(void) { return 0; }
#endif

#endif /* __SENSORHUB_STAT_H */
