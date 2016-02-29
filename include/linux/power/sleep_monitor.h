#ifndef _SLEEP_MONITOR_H
#define _SLEEP_MONITOR_H

/* Assign each device's index number */
enum SLEEP_MONITOR_DEVICE {
/* CAUTION!!! Need to sync with sleep_monitor_device  in sleep_monitor.c */
	SLEEP_MONITOR_BT = 0,
	SLEEP_MONITOR_WIFI = 1,
	SLEEP_MONITOR_MODEM = 2,
	SLEEP_MONITOR_GPS = 3,
	SLEEP_MONITOR_NFC = 4,
	SLEEP_MONITOR_SENSOR = 5,
	SLEEP_MONITOR_SENSOR1 = 6,
	SLEEP_MONITOR_AUDIO = 7,
	SLEEP_MONITOR_SAPA = 8,
	SLEEP_MONITOR_SAPA1 = 9,
	SLEEP_MONITOR_SAPB = 10,
	SLEEP_MONITOR_SAPB1 = 11,
	SLEEP_MONITOR_CONHR = 12,
	SLEEP_MONITOR_KEY = 13,
	SLEEP_MONITOR_TEMP = 14,
	SLEEP_MONITOR_CPU_UTIL = 15,
	SLEEP_MONITOR_LCD = 16,
	SLEEP_MONITOR_TSP = 17,
	SLEEP_MONITOR_ROTARY = 18,
	SLEEP_MONITOR_REGULATOR = 19,
	SLEEP_MONITOR_REGULATOR1 = 20,
	SLEEP_MONITOR_PMDOMAINS = 21,
	SLEEP_MONITOR_BATTERY = 22,
	SLEEP_MONITOR_DEV23 = 23,
	SLEEP_MONITOR_DEV24 = 24,
	SLEEP_MONITOR_DEV25 = 25,
	SLEEP_MONITOR_DEV26 = 26,
	SLEEP_MONITOR_DEV27 = 27,
	SLEEP_MONITOR_DEV28 = 28,
	SLEEP_MONITOR_TCP = 29,
	SLEEP_MONITOR_SYS_TIME = 30,
	SLEEP_MONITOR_RTC_TIME = 31,
	SLEEP_MONITOR_NUM_MAX,
};

/* Return device status from each device */
enum SLEEP_MONITOR_DEVICE_STATUS {
	DEVICE_ERR_1 = -1,
	DEVICE_POWER_OFF = 0,
	DEVICE_ON_LOW_POWER = 1,
	DEVICE_ON_ACTIVE1 = 2,
	DEVICE_ON_ACTIVE2 = 3,

	/* ADD HERE */

	DEVICE_UNKNOWN = 15,
};

/* Device return result whether via hw or not */
enum SLEEP_MONITOR_DEVICE_CALLER_TYPE {
	SLEEP_MONITOR_CALL_SUSPEND = 0,
	SLEEP_MONITOR_CALL_RESUME = 1,
	SLEEP_MONITOR_CALL_ETC =2,
};

/* Device return result whether via hw or not */
enum SLEEP_MONITOR_DEVICE_CHECK_LEVEL {
	SLEEP_MONITOR_CHECK_SOFT = 0,
	SLEEP_MONITOR_CHECK_HARD = 10 ,
};

/* Define boolean value */
enum SLEEP_MONITOR_BOOLEAN {
	SLEEP_MONITOR_BOOLEAN_FALSE = 0,
	SLEEP_MONITOR_BOOLEAN_TRUE = 1,
};


#define SLEEP_MONITOR_DEBUG_PREFIX "[slp_mon]"
#define SLEEP_MONITOR_BIT_INT_SIZE 32
#define SLEEP_MONITOR_DEVICE_BIT_WIDTH 4
#define SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE \
	SLEEP_MONITOR_BIT_INT_SIZE/SLEEP_MONITOR_DEVICE_BIT_WIDTH
#define SLEEP_MONITOR_NUM_DEVICE_RAW_VAL 8
#define SLEEP_MONITOR_ONE_LINE_RAW_SIZE 90
#define SLEEP_MONITOR_AFTER_BOOTING_TIME 15 	/* sec */

#define SLEEP_MONITOR_GROUP_SIZE \
	(((SLEEP_MONITOR_NUM_MAX) % ( SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE) == 0) \
	?((SLEEP_MONITOR_NUM_MAX) / ( SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE)) \
	:((SLEEP_MONITOR_NUM_MAX) / ( SLEEP_MONITOR_DEVICE_NUM_DEVICE_PER_4BYTE) + 1))

/* Sleep monitor's ops structure */
struct sleep_monitor_ops{
	int (*read_cb_func)(void *priv, unsigned int *raw_val, int check_level, int caller_type);
	int (*read64_cb_func)(void *priv, long long *raw_val, int check_level, int caller_type);
};

#ifdef CONFIG_SLEEP_MONITOR
extern int sleep_monitor_register_ops(void *dev, struct sleep_monitor_ops *ops,
													int device_type);
extern int sleep_monitor_unregister_ops(int device_type);
extern int sleep_monitor_get_pretty(int *pretty_group, int type);
extern int sleep_monitor_get_raw_value(int *raw_value);
#else
static inline int sleep_monitor_register_ops(void *dev,
						struct sleep_monitor_ops *ops, int device_type){}
static inline int sleep_monitor_unregister_ops(int device_type){}
static inline int sleep_monitor_get_pretty(int *pretty_group, int type){}
static inline int sleep_monitor_get_raw_value(int *raw_value){}
#endif

#endif
