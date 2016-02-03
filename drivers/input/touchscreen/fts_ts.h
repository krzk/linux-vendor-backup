#ifndef _LINUX_FTS_TS_H_
#define _LINUX_FTS_TS_H_

#include <linux/device.h>
#include <linux/i2c/fts.h>

#define FIRMWARE_IC			"fts_ic"

#define FTS_MAX_FW_PATH			64

#define FTS_TS_DRV_NAME			"fts_touch"
#define FTS_TS_DRV_VERSION		"0132"

#define STM_DEVICE_NAME			"STM"

#define FTS_ID0				0x39
#define FTS_ID1				0x80
#define FTS_ID2				0x6C

#define FTS_DIGITAL_REV_1		0x01
#define FTS_DIGITAL_REV_2		0x02
#define FTS_FIFO_MAX			32
#define FTS_EVENT_SIZE			8

#define PRESSURE_MIN			0
#define PRESSURE_MAX			127
#define P70_PATCH_ADDR_START		0x00420000
#define FINGER_MAX			10
#define AREA_MIN			PRESSURE_MIN
#define AREA_MAX			PRESSURE_MAX

#define EVENTID_NO_EVENT			0x00
#define EVENTID_ENTER_POINTER			0x03
#define EVENTID_LEAVE_POINTER			0x04
#define EVENTID_MOTION_POINTER			0x05
#define EVENTID_HOVER_ENTER_POINTER		0x07
#define EVENTID_HOVER_LEAVE_POINTER		0x08
#define EVENTID_HOVER_MOTION_POINTER		0x09
#define EVENTID_PROXIMITY_IN			0x0B
#define EVENTID_PROXIMITY_OUT			0x0C
#define EVENTID_MSKEY				0x0E
#define EVENTID_ERROR				0x0F
#define EVENTID_CONTROLLER_READY		0x10
#define EVENTID_SLEEPOUT_CONTROLLER_READY	0x11
#define EVENTID_RESULT_READ_REGISTER		0x12
#define EVENTID_STATUS_EVENT			0x16
#define EVENTID_INTERNAL_RELEASE_INFO		0x19
#define EVENTID_EXTERNAL_RELEASE_INFO		0x1A

#define EVENTID_FROM_STRING			0x80
#define EVENTID_GESTURE				0x20

#define EVENTID_SIDE_SCROLL			0x40
#define EVENTID_SIDE_TOUCH_DEBUG		0xDB
/* side touch event-id for debug, remove after f/w fixed */
#define EVENTID_SIDE_TOUCH			0x0B

#define STATUS_EVENT_MUTUAL_AUTOTUNE_DONE	0x01
#define STATUS_EVENT_SELF_AUTOTUNE_DONE		0x42

#define INT_ENABLE				0x41
#define INT_DISABLE				0x00

#define READ_STATUS				0x84
#define READ_ONE_EVENT				0x85
#define READ_ALL_EVENT				0x86

#define SLEEPIN					0x90
#define SLEEPOUT				0x91
#define SENSEOFF				0x92
#define SENSEON					0x93
#define FTS_CMD_HOVER_OFF			0x94
#define FTS_CMD_HOVER_ON			0x95

#define FTS_CMD_MSKEY_AUTOTUNE			0x96

#define FTS_CMD_KEY_SENSE_OFF			0x9A
#define FTS_CMD_KEY_SENSE_ON			0x9B
#define FTS_CMD_SET_FAST_GLOVE_MODE		0x9D

#define FTS_CMD_MSHOVER_OFF			0x9E
#define FTS_CMD_MSHOVER_ON			0x9F
#define FTS_CMD_SET_NOR_GLOVE_MODE		0x9F

#define FLUSHBUFFER				0xA1
#define FORCECALIBRATION			0xA2
#define CX_TUNNING				0xA3
#define SELF_AUTO_TUNE				0xA4

#define FTS_CMD_CHARGER_PLUGGED			0xA8
#define FTS_CMD_CHARGER_UNPLUGGED		0xAB

#define FTS_CMD_RELEASEINFO			0xAA
#define FTS_CMD_STYLUS_OFF			0xAB
#define FTS_CMD_STYLUS_ON			0xAC
#define FTS_CMD_LOWPOWER_MODE			0xAD

#define FTS_CMS_ENABLE_FEATURE			0xC1
#define FTS_CMS_DISABLE_FEATURE			0xC2

#define FTS_CMD_WRITE_PRAM			0xF0
#define FTS_CMD_BURN_PROG_FLASH			0xF2
#define FTS_CMD_ERASE_PROG_FLASH		0xF3
#define FTS_CMD_READ_FLASH_STAT			0xF4
#define FTS_CMD_UNLOCK_FLASH			0xF7
#define FTS_CMD_SAVE_FWCONFIG			0xFB
#define FTS_CMD_SAVE_CX_TUNING			0xFC

#define FTS_CMD_FAST_SCAN			0x01
#define FTS_CMD_SLOW_SCAN			0x02
#define FTS_CMD_USLOW_SCAN			0x03

#define REPORT_RATE_90HZ			0
#define REPORT_RATE_60HZ			1
#define REPORT_RATE_30HZ			2

#define FTS_CMD_STRING_ACCESS			0xF000
#define FTS_CMD_NOTIFY				0xC0

#define FTS_RETRY_COUNT				10

/* QUICK SHOT : Quick Camera Launching */
#define FTS_STRING_EVENT_REAR_CAM		(1 << 0)
#define FTS_STRING_EVENT_FRONT_CAM		(1 << 1)

/* SCRUB : Display Watch, Event Status / Fast Access Event */
#define FTS_STRING_EVENT_WATCH_STATUS		(1 << 2)
#define FTS_STRING_EVENT_FAST_ACCESS		(1 << 3)
#define FTS_STRING_EVENT_DIRECT_INDICATOR	(1 << 3) | (1 << 2)

#define FTS_SIDEGESTURE_EVENT_SINGLE_STROKE	0xE0
#define FTS_SIDEGESTURE_EVENT_DOUBLE_STROKE	0xE1

#define FTS_SIDETOUCH_EVENT_LONG_PRESS		0xBB
#define FTS_SIDETOUCH_EVENT_REBOOT_BY_ESD	0xED

#define FTS_ENABLE				1
#define FTS_DISABLE				0

#define FTS_MODE_QUICK_SHOT			(1 << 0)
#define FTS_MODE_SCRUB				(1 << 1)
#define FTS_MODE_QUICK_APP_ACCESS		(1 << 2)
#define FTS_MODE_DIRECT_INDICATOR		(1 << 3)

#define TSP_BUF_SIZE				1024
#define CMD_STR_LEN				32
#define CMD_RESULT_STR_LEN			512
#define CMD_PARAM_NUM				8

#define FTS_LOWP_FLAG_QUICK_CAM			(1 << 0)
#define FTS_LOWP_FLAG_2ND_SCREEN		(1 << 1)
#define FTS_LOWP_FLAG_BLACK_UI			(1 << 2)
#define FTS_LOWP_FLAG_QUICK_APP_ACCESS		(1 << 3)
#define FTS_LOWP_FLAG_DIRECT_INDICATOR		(1 << 4)
#define FTS_LOWP_FLAG_TEMP_CMD			(1 << 5)

enum fts_error_return {
	FTS_NOT_ERROR = 0,
	FTS_ERROR_INVALID_CHIP_ID,
	FTS_ERROR_INVALID_CHIP_VERSION_ID,
	FTS_ERROR_INVALID_SW_VERSION,
	FTS_ERROR_EVENT_ID,
	FTS_ERROR_TIMEOUT,
	FTS_ERROR_FW_UPDATE_FAIL,
};

#define RAW_MAX	3750

/*
 * struct fts_finger - Represents fingers.
 * @ state: finger status (Event ID).
 * @ mcount: moving counter for debug.
 */
struct fts_finger {
	unsigned char state;
	unsigned short mcount;
	int lx;
	int ly;
};

enum tsp_power_mode {
	FTS_POWER_STATE_ACTIVE = 0,
	FTS_POWER_STATE_LOWPOWER,
	FTS_POWER_STATE_POWERDOWN,
	FTS_POWER_STATE_DEEPSLEEP,
};

enum fts_cover_id {
	FTS_FLIP_WALLET = 0,
	FTS_VIEW_COVER,
	FTS_COVER_NOTHING1,
	FTS_VIEW_WIRELESS,
	FTS_COVER_NOTHING2,
	FTS_CHARGER_COVER,
	FTS_VIEW_WALLET,
	FTS_LED_COVER,
	FTS_MONTBLANC_COVER = 100,
};

enum fts_customer_feature {
	FTS_FEATURE_ORIENTATION_GESTURE = 1,
	FTS_FEATURE_STYLUS,
	FTS_FEATURE_QUICK_SHORT_CAMERA_ACCESS,
	FTS_FEATURE_SIDE_GUSTURE,
	FTS_FEATURE_COVER_GLASS,
	FTS_FEATURE_COVER_WALLET,
	FTS_FEATURE_COVER_LED,
};

struct fts_ts_info {
	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *input_key;

	bool irq_enabled;
	struct fts_i2c_platform_data *board;
	bool enabled;

	struct fts_finger finger[FINGER_MAX];

	int touch_mode;
	int fw_ver_ic;
	int config_ver_ic;
	int fw_main_ver_ic;

	bool touch_stopped;
	bool reinit_done;

	unsigned char data[FTS_EVENT_SIZE * FTS_FIFO_MAX];
};
#endif		/* LINUX_FTS_TS_H_ */
