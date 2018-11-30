/*
 *
 * Zinitix ztm620 touch driver
 *
 * Copyright (C) 2018 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#if defined(CONFIG_PM_RUNTIME)
#include <linux/pm_runtime.h>
#endif
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/async.h>
#include <linux/firmware.h>
#include <linux/wakelock.h>
//#include <linux/input/tsp_ta_callback.h>

#define	ZTM620_IC_CHIP_CODE	0xE628
#define	ZTW522_IC_CHIP_CODE	0xE532
#define	ZT7538_IC_CHIP_CODE	0xE538


#define	ZTM620_IC_FW_SIZE	(48 * 1024)

#define	SEC_FACTORY_TEST
#define	ENABLE_TOUCH_BOOSTER
//#define	USE_TSP_TA_CALLBACKS

#define	SEC_TSP_NAME			"sec_touchscreen"
#define	TS_DRVIER_VERSION		"1.0.18_1"
#define	ZTM620_TS_DEVICE		"ztm620_ts"

#ifdef CONFIG_TIZEN_SEC_KERNEL_ENG
#define	ZINITIX_MISC_DEBUG
#define	ZINITIX_FILE_DEBUG
#ifdef ZINITIX_MISC_DEBUG
#define	ZINITIX_DEBUG			0
#endif
#endif

#define	TOUCH_POINT_MODE		0
#define	CHECK_HWID			1
#define	TSP_INIT_TEST_RATIO		100
#define	MAX_SUPPORTED_FINGER_NUM	2	/* max 10 */
#define	SUPPORTED_PALM_TOUCH	1
#define	REAL_Z_MAX			3000

#ifdef SEC_FACTORY_TEST
typedef void (*run_func_t)(void *);
enum data_type {
	DATA_UNSIGNED_CHAR,
	DATA_SIGNED_CHAR,
	DATA_UNSIGNED_SHORT,
	DATA_SIGNED_SHORT,
	DATA_UNSIGNED_INT,
	DATA_SIGNED_INT,
};

#define	COVER_OPEN			0
#define	COVER_CLOSED			3
#endif

#define	ZINITIX_MAX_FW_PATH		0xff
#define	ZINITIX_FW_PATH		"tsp_zinitix/"
#define	ZINITIX_DEFAULT_UMS_FW	"/opt/usr/media/zinitix.fw"
#define	ZINITIX_DEFAULT_FFU_FW	"ffu_tsp.bin"

/* resolution offset */
#define	ABS_PT_OFFSET			(-1)
#define	TOUCH_FORCE_UPGRADE		0
#define	USE_CHECKSUM			1
#define	CHIP_OFF_DELAY			50	/*ms*/
#define	DELAY_FOR_SIGNAL_DELAY	30	/*us*/
#define	DELAY_FOR_TRANSCATION		50
#define	DELAY_FOR_POST_TRANSCATION	10
#define	RAWDATA_DELAY_FOR_HOST	100
#define	CHECKSUM_VAL_SIZE		2

#if USE_CHECKSUM
#define	ZTM620_CHECK_SUM		0x55aa
#endif

/*Test Mode (Monitoring Raw Data) */
#define	TSP_INIT_TEST_RATIO		100
#define	SEC_MUTUAL_AMP_V_SEL		0x0232

#define	CONNECT_TEST_DELAY		100
#define	I2C_TX_DELAY			1
#define	I2C_START_DELAY		500
#define	GPIO_GET_DELAY		1
#define	LPM_OFF_DELAY			1	/*ms*/
#define	POWER_ON_DELAY		30	/*ms*/
#define	RESET_DELAY			20	/*ms*/
#define	CHIP_ON_DELAY			50	/*ms*/
#define	FIRMWARE_ON_DELAY		200	/*ms*/
#define	SEC_DND_N_COUNT		11
#define	SEC_DND_U_COUNT		16
#define	SEC_DND_FREQUENCY		139

#define	SEC_HFDND_N_COUNT		11
#define	SEC_HFDND_U_COUNT		16
#define	SEC_HFDND_FREQUENCY		104

#define	SEC_M_U_COUNT		   3
#define	SEC_M_N_COUNT		   11
#define	SEC_M_FREQUENCY		 84
#define	SEC_M_RST0_TIME		 170

#define	SEC_SX_AMP_V_SEL		0x0434
#define	SEC_SX_SUB_V_SEL		0x0055
#define	SEC_SY_AMP_V_SEL		0x0232
#define	SEC_SY_SUB_V_SEL		0x0022
#define	SEC_SHORT_N_COUNT		2
#define	SEC_SHORT_U_COUNT		1

//DND
#define	SEC_DND_CP_CTRL_L		0x1fb3
#define	SEC_DND_V_FORCE		0
#define	SEC_DND_AMP_V_SEL		0x0141

#define	MAX_LIMIT_CNT			10000
#define	MAX_RAW_DATA_SZ		36*22
#define	MAX_TRAW_DATA_SZ	\
	(MAX_RAW_DATA_SZ + 4*MAX_SUPPORTED_FINGER_NUM + 2)

#define	TOUCH_DELTA_MODE		3
#define	TOUCH_PROCESS_MODE		4
#define	TOUCH_NORMAL_MODE		5
#define	TOUCH_CND_MODE		6
#define	TOUCH_REFERENCE_MODE		8
#define	TOUCH_REF_MODE			10
#define	TOUCH_DND_MODE		11
#define	TOUCH_HFDND_MODE		12
#define	TOUCH_TRXSHORT_MODE		13
#define	TOUCH_SFR_MODE			17
#define	TOUCH_SSR_MODE			39

#define	TOUCH_SEC_MODE			48

#define	PALM_REPORT_WIDTH		200
#define	PALM_REJECT_WIDTH		255

#define	TOUCH_V_FLIP			0x01
#define	TOUCH_H_FLIP			0x02
#define	TOUCH_XY_SWAP			0x04

#define	TSP_NORMAL_EVENT_MSG		1
#define	I2C_RETRY_TIMES			2
#define	I2C_START_RETRY_TIMES		5
#define	I2C_BUFFER_SIZE			64
#define	INT_CLEAR_RETRY			5
#define	CONNECT_TEST_RETRY		2
/*  Other Things */
#define	INIT_RETRY_CNT			3
#define	I2C_SUCCESS			0
#define	I2C_FAIL				1

#define	INTERNAL_FLAG_01_DEFAULT	0x4021
#define	INTERNAL_FLAG_02_DEFAULT	0x0012

#define	TOUCH_IOCTL_BASE			0xbc
#define	TOUCH_IOCTL_GET_DEBUGMSG_STATE	_IOW(TOUCH_IOCTL_BASE, 0, int)
#define	TOUCH_IOCTL_SET_DEBUGMSG_STATE	_IOW(TOUCH_IOCTL_BASE, 1, int)
#define	TOUCH_IOCTL_GET_CHIP_REVISION		_IOW(TOUCH_IOCTL_BASE, 2, int)
#define	TOUCH_IOCTL_GET_FW_VERSION		_IOW(TOUCH_IOCTL_BASE, 3, int)
#define	TOUCH_IOCTL_GET_REG_DATA_VERSION	_IOW(TOUCH_IOCTL_BASE, 4, int)
#define	TOUCH_IOCTL_VARIFY_UPGRADE_SIZE	_IOW(TOUCH_IOCTL_BASE, 5, int)
#define	TOUCH_IOCTL_VARIFY_UPGRADE_DATA	_IOW(TOUCH_IOCTL_BASE, 6, int)
#define	TOUCH_IOCTL_START_UPGRADE		_IOW(TOUCH_IOCTL_BASE, 7, int)
#define	TOUCH_IOCTL_GET_X_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 8, int)
#define	TOUCH_IOCTL_GET_Y_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 9, int)
#define	TOUCH_IOCTL_GET_TOTAL_NODE_NUM	_IOW(TOUCH_IOCTL_BASE, 10, int)
#define	TOUCH_IOCTL_SET_RAW_DATA_MODE	_IOW(TOUCH_IOCTL_BASE, 11, int)
#define	TOUCH_IOCTL_GET_RAW_DATA		_IOW(TOUCH_IOCTL_BASE, 12, int)
#define	TOUCH_IOCTL_GET_X_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 13, int)
#define	TOUCH_IOCTL_GET_Y_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 14, int)
#define	TOUCH_IOCTL_HW_CALIBRAION		_IOW(TOUCH_IOCTL_BASE, 15, int)
#define	TOUCH_IOCTL_GET_REG			_IOW(TOUCH_IOCTL_BASE, 16, int)
#define	TOUCH_IOCTL_SET_REG			_IOW(TOUCH_IOCTL_BASE, 17, int)
#define	TOUCH_IOCTL_SEND_SAVE_STATUS		_IOW(TOUCH_IOCTL_BASE, 18, int)
#define	TOUCH_IOCTL_DONOT_TOUCH_EVENT	_IOW(TOUCH_IOCTL_BASE, 19, int)

/* Register Map*/
#define	ZTM620_SWRESET_CMD				0x0000
#define	ZTM620_WAKEUP_CMD				0x0001
#define	ZTM620_DEEP_SLEEP_CMD			0x0004
#define	ZTM620_SLEEP_CMD				0x0005
#define	ZTM620_CLEAR_INT_STATUS_CMD			0x0003
#define	ZTM620_CALIBRATE_CMD				0x0006
#define	ZTM620_SAVE_STATUS_CMD			0x0007
#define	ZTM620_SAVE_CALIBRATION_CMD			0x0008
//#define	ZTM620_I2C_START_CMD				0x000A
//#define	ZTM620_I2C_END_CMD	  			0x000B
#define	ZTM620_RECALL_FACTORY_CMD			0x000f
#define	ZTM620_THRESHOLD				0x0020
#define	ZTM620_DEBUG_REG				0x0115 /* 0~7 */
#define	ZTM620_TOUCH_MODE				0x0010
#define	ZTM620_CHIP_REVISION				0x0011
#define	ZTM620_FIRMWARE_VERSION			0x0012
#define	ZTM620_MINOR_FW_VERSION			0x0121
#define	ZTM620_VENDOR_ID				0x001C
#define	ZTM620_HW_ID					0x0014
#define	ZTM620_DATA_VERSION_REG			0x0013
#define	ZTM620_SUPPORTED_FINGER_NUM			0x0015
#define	ZTM620_EEPROM_INFO				0x0018
#define	ZTM620_INITIAL_TOUCH_MODE			0x0019
#define	ZTM620_TOTAL_NUMBER_OF_X			0x0060
#define	ZTM620_TOTAL_NUMBER_OF_Y			0x0061
#define	ZTM620_DELAY_RAW_FOR_HOST			0x007f
#define	ZTM620_BUTTON_SUPPORTED_NUM			0x00B0
#define	ZTM620_BUTTON_SENSITIVITY			0x00B2
#define	ZTM620_DUMMY_BUTTON_SENSITIVITY		0X00C8
#define	ZTM620_X_RESOLUTION				0x00C0
#define	ZTM620_Y_RESOLUTION				0x00C1
#define	ZTM620_POINT_STATUS_REG			0x0080
#define	ZTM620_ICON_STATUS_REG			0x00AA
#define	ZTM620_I2C_MODE_REG				0x00E2

#define	ZTM620_GESTURE_STATE				0x01FB
#define	ZTM620_MUTUAL_AMP_V_SEL			0x02F9
#define	ZTM620_DND_SHIFT_VALUE			0x012B
#define	ZTM620_AFE_FREQUENCY				0x0100
#define	ZTM620_DND_N_COUNT				0x0122
#define	ZTM620_DND_U_COUNT				0x0135
#define	ZTM620_CONNECTED_REG				0x0062
#define	ZTM620_CONNECTED_REG_MAX			0x0078
#define	ZTM620_CONNECTED_REG_MIN			0x0079
#define	ZTM620_DND_V_FORCE				0x02F1
#define	ZTM620_DND_AMP_V_SEL				0x02F9
#define	ZTM620_DND_CP_CTRL_L				0x02bd
#define	ZTM620_RAWDATA_REG				0x0200
#define	ZTM620_EEPROM_INFO_REG			0x0018
#define	ZTM620_INT_ENABLE_FLAG			0x00f0
#define	ZTM620_PERIODICAL_INTERRUPT_INTERVAL	0x00f1
#define	ZTM620_BTN_WIDTH				0x0316
#define	ZTM620_CHECKSUM_RESULT			0x012c
#define	ZTM620_INIT_FLASH				0x01d0
#define	ZTM620_WRITE_FLASH				0x01d1
#define	ZTM620_READ_FLASH				0x01d2
#define	ZINITIX_INTERNAL_FLAG_01			0x011d
#define	ZINITIX_INTERNAL_FLAG_02			0x011e
#define	ZTM620_OPTIONAL_SETTING			0x0116
#define	ZT75XX_SX_AMP_V_SEL				0x02DF
#define	ZT75XX_SX_SUB_V_SEL				0x02E0
#define	ZT75XX_SY_AMP_V_SEL				0x02EC
#define	ZT75XX_SY_SUB_V_SEL				0x02ED

#define	ZTM620_M_U_COUNT				0x02F2
#define	ZTM620_M_N_COUNT				0x02F3
#define	ZTM620_M_RST0_TIME	  			0x02F5
#define	ZTM620_REAL_WIDTH				0x03A6
#define	ZTM620_FW_CHECKSUM_REG  			0x03DF


#define	ZTM620_DEBUG_00				0x0115

/* Interrupt & status register flag bit
-------------------------------------------------
*/
#define	BIT_PT_CNT_CHANGE	0
#define	BIT_DOWN		1
#define	BIT_MOVE		2
#define	BIT_UP			3
#define	BIT_PALM		4
#define	BIT_PALM_REJECT	5
#define	BIT_GESTURE_WAKE	6
#define	RESERVED_1		7
#define	BIT_WEIGHT_CHANGE	8
#define	BIT_PT_NO_CHANGE	9
#define	BIT_REJECT		10
#define	BIT_PT_EXIST		11
#define	RESERVED_2		12
#define	BIT_MUST_ZERO		13
#define	BIT_DEBUG		14
#define	BIT_ICON_EVENT		15


/* DEBUG_00 */
#define	BIT_DEVICE_STATUS_LPM	0
#define	BIT_POWER_SAVING_MODE	14

#define DEF_DEVICE_STATUS_NPM				0// read only
#define DEF_DEVICE_STATUS_WALLET_COVER_MODE	1// read only
#define DEF_DEVICE_STATUS_NOISE_MODE			2// read only
#define DEF_DEVICE_STATUS_WATER_MODE		3// read only
#define DEF_DEVICE_STATUS_LPM				4// read only
#define DEF_DEVICE_STATUS_GLOVE_MODE			5// read only
#define DEF_DEVICE_STATUS_HIGH_SEN_MODE		6// read only
#define DEF_DEVICE_STATUS_HIGH_SEN_NORMAL_MODE	7// read only

#define DEF_DEVICE_STATUS_MAX_SKIP			8// read only
#define DEF_DEVICE_STATUS_NOISE_SKIP			9// read only
#define DEF_DEVICE_STATUS_PALM_DETECT		10// read only
#define DEF_DEVICE_STATUS_SVIEW_MODE		11// read only
#define DEF_DEVICE_STATUS_ESD_COVER			12

/* button */
#define	BIT_O_ICON0_DOWN	0
#define	BIT_O_ICON1_DOWN	1
#define	BIT_O_ICON2_DOWN	2
#define	BIT_O_ICON3_DOWN	3
#define	BIT_O_ICON4_DOWN	4
#define	BIT_O_ICON5_DOWN	5
#define	BIT_O_ICON6_DOWN	6
#define	BIT_O_ICON7_DOWN	7

#define	BIT_O_ICON0_UP		8
#define	BIT_O_ICON1_UP		9
#define	BIT_O_ICON2_UP		10
#define	BIT_O_ICON3_UP		11
#define	BIT_O_ICON4_UP		12
#define	BIT_O_ICON5_UP		13
#define	BIT_O_ICON6_UP		14
#define	BIT_O_ICON7_UP		15

#define	SUB_BIT_EXIST		0
#define	SUB_BIT_DOWN		1
#define	SUB_BIT_MOVE		2
#define	SUB_BIT_UP		3
#define	SUB_BIT_UPDATE		4
#define	SUB_BIT_WAIT		5

#define	zinitix_bit_set(val, n)	((val) &= ~(1<<(n)), (val) |= (1<<(n)))
#define	zinitix_bit_clr(val, n)	((val) &= ~(1<<(n)))
#define	zinitix_bit_test(val, n)	((val) & (1<<(n)))
#define	zinitix_swap_v(a, b, t)	((t) = (a), (a) = (b), (b) = (t))
#define	zinitix_swap_16(s)	(((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)))

/* REG_USB_STATUS : optional setting from AP */
#define	DEF_OPTIONAL_MODE_USB_DETECT_BIT		0
#define	DEF_OPTIONAL_MODE_SVIEW_DETECT_BIT		1
#define	DEF_OPTIONAL_MODE_SENSITIVE_BIT		2
#define	DEF_OPTIONAL_MODE_EDGE_SELECT		3
#define	DEF_OPTIONAL_MODE_DUO_TOUCH		4

#ifdef SEC_FACTORY_TEST
/* Touch Screen */
#define	TSP_CMD_STR_LEN		32
#define	TSP_CMD_RESULT_STR_LEN	4096
#define	TSP_CMD_PARAM_NUM	8
#define	TSP_CMD_Y_NUM		18
#define	TSP_CMD_X_NUM		30
#define	TSP_CMD_NODE_NUM	(TSP_CMD_Y_NUM * TSP_CMD_X_NUM)
#define	TSP_CONNECTED_INVALID	0
#define	TSP_CONNECTED_VALID	1
#define	REG_EDGE_XF_OFFSET	0xEC
#define	REG_EDGE_XL_OFFSET	0xED
#define	REG_EDGE_YF_OFFSET	0xEE
#define	REG_EDGE_YL_OFFSET	0xEF

#define	REG_SET_I2C_NORMAL	610
#define	REG_SET_I2C_LPM		1600

enum {
	WAITING = 0,
	RUNNING,
	OK,
	FAIL,
	NOT_APPLICABLE,
};
#endif

enum power_control {
	POWER_OFF,
	POWER_ON,
};

enum key_event {
	ICON_BUTTON_UNCHANGE,
	ICON_BUTTON_DOWN,
	ICON_BUTTON_UP,
};

enum work_state {
	NOTHING = 0,
	NORMAL,
	ESD_TIMER,
	EALRY_SUSPEND,
	SUSPEND,
	RESUME,
	LATE_RESUME,
	UPGRADE,
	REMOVE,
	SET_MODE,
	HW_CALIBRAION,
	RAW_DATA,
	PROBE,
};

enum {
	BUILT_IN = 0,
	UMS,
	FFU,
};

struct raw_ioctl {
	u32	sz;
	u32	*buf;
};

struct reg_ioctl {
	u32	addr;
	u32	*val;
};

struct coord {
	u16	x;
	u16	y;
	u8	width;
	u8	sub_status;
	u16	real_width;
};

struct point_info {
	u16	status;
#if TOUCH_POINT_MODE
	u16 	event_flag;
#else
	u8	finger_cnt;
	u8	time_stamp;
#endif
	struct coord coord[MAX_SUPPORTED_FINGER_NUM];
	u16 fw_status;
};

struct capa_info {
	u16	vendor_id;
	u16	ic_revision;
	u16	fw_version;
	u16	fw_minor_version;
	u16	reg_data_version;
	u16	threshold;
	u16	key_threshold;
	u16	dummy_threshold;
	u32	ic_fw_size;
	u32	MaxX;
	u32	MaxY;
	u32	MinX;
	u32	MinY;
	u8	gesture_support;
	u16	multi_fingers;
	u16	button_num;
	u16	ic_int_mask;
	u16	x_node_num;
	u16	y_node_num;
	u16	total_node_num;
	u16	hw_id;
	u16	afe_frequency;
	u16	shift_value;
	u16	v_force;
	u16	amp_v_sel;
	u16	N_cnt;
	u16	u_cnt;
	u16	cp_ctrl_l;
	u16	mutual_amp_v_sel;
	u16	i2s_checksum;
	u16	sx_amp_v_sel;
	u16	sx_sub_v_sel;
	u16	sy_amp_v_sel;
	u16	sy_sub_v_sel;
};

#ifdef USE_TSP_TA_CALLBACKS
extern struct tsp_callbacks *ist30xxc_charger_callbacks;
struct tsp_callbacks {
	void (*inform_charger)(struct tsp_callbacks *, int);
};
#endif

struct zxt_ts_platform_data {
	int		gpio_int;
	int		vdd_en;
	int		gpio_scl;
	int		gpio_sda;
	int		gpio_ldo_en;
	int		gpio_reset;
	int		gpio_lpm;
	u32		x_resolution;
	u32		y_resolution;
	const char	*fw_name;
	const char	*model_name;
	u32		page_size;
	u32		orientation;
	u32		tsp_supply_type;
	u32		core_num;
	bool		reg_boot_on;
	const char 	*avdd_ldo;
	const char 	*dvdd_ldo;
};

struct finger_event {
	u32	x;
	u32	y;
	u32	move_cnt;
	u32	time;
};

struct ztm620_info {
	struct i2c_client			*client;
	struct input_dev			*input_dev;
	struct zxt_ts_platform_data		*pdata;
	struct capa_info			cap_info;
	struct point_info			touch_info;
	struct point_info			reported_touch_info;
	struct semaphore			work_lock;
	struct mutex			set_reg_lock;
	struct semaphore			raw_data_lock;
	struct pinctrl			*pinctrl;
	struct pinctrl_state		*gpio_wake;
	struct pinctrl_state		*gpio_off;
	struct wake_lock			wake_lock;
	struct finger_event		event[MAX_SUPPORTED_FINGER_NUM];
	struct regulator			*vddo;
	struct regulator			*avdd;
	wait_queue_head_t			wait_q;
	bool 				wait_until_wake;
	u16				touch_mode;
	s16				cur_data[MAX_TRAW_DATA_SZ];
	char				phys[32];
	u8				finger_cnt;
	u8				update;
	unsigned char			*fw_data;
	u16				icon_event_reg;
	u16				prev_icon_event;
	u16				chip_code;
	int				irq;
	u8				work_state;
	s16				ref_scale_factor;
	s16				ref_btn_option;
	bool				device_enabled;
	bool				init_done;
	u16				cover_state;
	u16				optional_mode;
	bool				debug_enable;
	bool				gesture_enable;
	int				cal_mode;
	u8				lpm_mode;
	u32				multi_touch_cnt;
	u32				wet_mode_cnt;
	u32				comm_err_cnt;
	u32				noise_mode_cnt;
	u32				all_touch_cnt;
	u32				aod_touch_cnt;
	u32				holding_time;
	bool				wet_mode;
	bool				noise_mode;
	u32				max_z_value;
#ifdef USE_TSP_TA_CALLBACKS
	struct tsp_callbacks		callbacks;
	int				ta_connected;
#endif
#if SUPPORTED_PALM_TOUCH
	u8				palm_flag;
#endif
#ifdef SEC_FACTORY_TEST
	struct tsp_factory_info		*factory_info;
	struct tsp_raw_data		*raw_data;
	bool				get_all_data;
	const u16				*dnd_max_spec;
	const u16				*dnd_min_spec;
	const u16				*dnd_v_gap_spec;
	const u16				*dnd_h_gap_spec;
	const u16				*hfdnd_max_spec;
	const u16				*hfdnd_min_spec;
	const u16				*hfdnd_v_gap_spec;
	const u16				*hfdnd_h_gap_spec;
	u16				checksum;
#endif
#ifdef CONFIG_SLEEP_MONITOR
	u32				release_cnt;
#endif
};

#ifdef SEC_FACTORY_TEST
struct tsp_factory_info {
	struct list_head cmd_list_head;
	char		cmd[TSP_CMD_STR_LEN];
	char		cmd_param[TSP_CMD_PARAM_NUM];
	char		cmd_result[TSP_CMD_RESULT_STR_LEN];
	char		cmd_buff[TSP_CMD_RESULT_STR_LEN];
	struct mutex	cmd_lock;
	bool		cmd_is_running;
	u8		cmd_state;
};

struct tsp_raw_data {
	u16	dnd_data[TSP_CMD_NODE_NUM];
	s16	delta_data[TSP_CMD_NODE_NUM];
	s16	hfdnd_data[TSP_CMD_NODE_NUM];
	s16	vgap_data[TSP_CMD_NODE_NUM];
	s16	hgap_data[TSP_CMD_NODE_NUM];
	s16	hfvgap_data[TSP_CMD_NODE_NUM];
	s16	hfhgap_data[TSP_CMD_NODE_NUM];
	s16	trxshort_data[TSP_CMD_NODE_NUM];
	s16	reference_data[TSP_CMD_NODE_NUM];
	s16 ssr_data[TSP_CMD_NODE_NUM];
	s16 sfr_data[TSP_CMD_NODE_NUM];
	s16 sfr_hgap_data[TSP_CMD_NODE_NUM];
};

struct tsp_cmd {
	struct list_head	list;
	const char	*cmd_name;
	void		(*cmd_func)(void *device_data);
};
#endif
