#ifndef _CYPRESS_TOUCHKEY_H
#define _CYPRESS_TOUCHKEY_H

#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>

/* LDO Regulator */
#define	TK_REGULATOR_NAME	"VTOUCH_1.8V_AP"

/* LDO Regulator */
#define	TK_LED_REGULATOR_NAME	"VTOUCH_LED_3.3V"

/* LED LDO Type*/
#define LED_LDO_WITH_REGULATOR

#define CYPRESS_IC_20055 20055
#define CYPRESS_IC_20075 20075
#define CYPRESS_IC_MBR3155 3155

/* Touchkey Register */
#define KEYCODE_REG			0x03
#define BASE_REG			0x00
#define TK_STATUS_FLAG			0x07
#define TK_DIFF_DATA			0x16
#define TK_RAW_DATA			0x1E
#define TK_IDAC				0x0D
#define TK_COMP_IDAC			0x11
#define TK_BASELINE_DATA		0x26
#define TK_THRESHOLD			0x09
#define TK_FW_VER			0x04

#define TK_BIT_PRESS_EV			0x08
#define TK_BIT_KEYCODE			0x07

/* New command*/
/* Owner for LED on/off cpntrol(0:host / 1: touch IC) */
#define TK_BIT_CMD_LEDCONTROL		0x40
/* Inspection mode */
#define TK_BIT_CMD_INSPECTION		0x20
/* 1mm stylus mode */
#define TK_BIT_CMD_1mmSTYLUS		0x10
/* Flip mode */
#define TK_BIT_CMD_FLIP			0x08
/* Glove mode */
#define TK_BIT_CMD_GLOVE		0x04
/* TA mode */
#define TK_BIT_CMD_TA_ON		0x02
/* Regular mode(Normal mode) */
#define TK_BIT_CMD_REGULAR		0x01

#define TK_BIT_WRITE_CONFIRM		0xAA
#define TK_BIT_EXIT_CONFIRM		0xBB

/* Owner for LED on/off cpntrol(0:host / 1: touch IC) */
#define TK_BIT_LEDCONTROL		0x40
/* 1mm stylus mode */
#define TK_BIT_1mmSTYLUS		0x20
/* Flip mode */
#define TK_BIT_FLIP			0x10
/* Glove mode */
#define TK_BIT_GLOVE			0x08
/* TA mode */
#define TK_BIT_TA_ON			0x04
/* Regular mode(Normal mode) */
#define TK_BIT_REGULAR			0x02
/* LED status */
#define TK_BIT_LED_STATUS		0x01

#define TK_CMD_LED_ON			0x10
#define TK_CMD_LED_OFF			0x20
#define TK_LED_VOLTAGE_MIN		2500000
#define TK_LED_VOLTAGE_MID		2900000
#define TK_LED_VOLTAGE_MAX		3300000


#define TK_UPDATE_DOWN			1
#define TK_UPDATE_FAIL			-1
#define TK_UPDATE_PASS			0

/* update condition */
#define TK_RUN_UPDATE			1
#define TK_EXIT_UPDATE			2
#define TK_RUN_CHK			3

/* KEY_TYPE*/
#define TK_USE_2KEY_TYPE

/* Boot-up Firmware Update */
#define FW_PATH		"cypress/cypress_t.fw"

#define TKEY_FW_PATH	"/sdcard/cypress/fw.bin"

enum {
	FW_NONE = 0,
	FW_BUILT_IN,
	FW_HEADER,
	FW_IN_SDCARD,
	FW_EX_SDCARD,
};

/* mode change struct */
#define MODE_NONE			0
#define MODE_NORMAL			1
#define MODE_GLOVE			2
#define MODE_FLIP			3

#define CMD_GLOVE_ON			1
#define CMD_GLOVE_OFF			2
#define CMD_FLIP_ON			3
#define CMD_FLIP_OFF			4
#define CMD_GET_LAST_MODE		0xfd
#define CMD_MODE_RESERVED		0xfe
#define CMD_MODE_RESET			0xff

struct cmd_mode_change {
	int mode;
	int cmd;
};
struct mode_change_data {
	bool busy;
	int cur_mode;
	struct cmd_mode_change mtc;	/* mode to change */
	struct cmd_mode_change mtr;	/* mode to reserved */
};

struct touchkey_platform_data {
	int gpio_sda;
	int gpio_scl;
	int gpio_int;
	u32 irq_gpio_flags;
	u32 sda_gpio_flags;
	u32 scl_gpio_flags;
	bool i2c_gpio;
	u32 stabilizing_time;
	u32 ic_type;
	bool boot_on_ldo;
	bool led_by_ldo;
	bool glove_mode_keep_status;
	char *fw_path;
	void (*init_platform_hw)(void);
	int (*suspend)(void);
	int (*resume)(void);
	int (*power_on)(void *, bool);
	int (*led_power_on)(void *, bool);
	int (*reset_platform_hw)(void);
	void (*register_cb)(void *);
	unsigned int keycodes[2];
};

/*Parameters for i2c driver*/
struct touchkey_i2c {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct completion init_done;
	struct led_classdev cdev;
	struct mutex lock;
	struct mutex i2c_lock;
	struct device	*dev;
	int irq;
	int md_ver_ic; /*module ver*/
	int fw_ver_ic;
	int device_ver;
	int firmware_id;
	int crc;

	struct touchkey_platform_data *pdata;
	char *name;
	int (*power)(int on);
	int update_status;
	bool enabled;
	int	src_fw_ver;
	int	src_md_ver;
	struct work_struct update_work;
	struct workqueue_struct *fw_wq;
	u8 fw_path;
	const struct firmware *firm_data;
	struct fw_image *fw_img;
	bool do_checksum;
	struct pinctrl *pinctrl_irq;
	struct pinctrl *pinctrl_i2c;
	struct pinctrl_state *pin_state[4];
	struct mode_change_data mc_data;
	int ic_mode;
	int tsk_enable_glove_mode;

	struct regulator *regulator_pwr;
	struct regulator *regulator_led;
};

#endif /* _CYPRESS_TOUCHKEY_H */
