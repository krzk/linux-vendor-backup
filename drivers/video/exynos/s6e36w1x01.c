/* linux/drivers/video/exynos/s6e36w1x01.c
 *
 * MIPI-DSI based s6e36w1x01 AMOLED lcd panel driver.
 *
 * Joong-Mock, Shin <jmock.shin@samsung.com>
 * Taeheon, Kim <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sysfs.h>
#include <linux/pm_runtime.h>
#include <linux/lcd-property.h>
#include <video/mipi_display.h>
#include <video/exynos_mipi_dsim.h>
#include <plat/gpio-cfg.h>
#if defined(CONFIG_SYSTEM_LOAD_ANALYZER)
#include <linux/load_analyzer.h>
#endif

#include "s6e36w1x01_dimming.h"
#include "exynos_smies.h"
#include "mdnie_lite.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DEFAULT_BRIGHTNESS	80
#define DIMMING_COUNT	57
#define MDNIE_TUNE	0
#define POWER_IS_ON(pwr)	((pwr) == FB_BLANK_UNBLANK)
#define POWER_IS_OFF(pwr)	((pwr) == FB_BLANK_POWERDOWN)
#define POWER_IS_NRM(pwr)	((pwr) == FB_BLANK_NORMAL)

#define lcd_to_master(a)	(a->dsim_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

#define LDI_CASET		0x2A
#define LDI_PASET		0x2B
#define LDI_MTP1		0xB1
#define LDI_MTP2		0xBC
#define LDI_MTP3		0xC7
#define LDI_MTP4		0xC8
#define LDI_CHIP_ID		0xD6
#define LDI_ELVSS		0xB6
#define LDI_GAMMA		0xCA

#define LDI_MTP1_LEN		6
#define LDI_MTP2_LEN		12
#define LDI_MTP3_LEN		4
#define LDI_MTP4_LEN		32
#define LDI_ELVSS_LEN		1
#define LDI_CHIP_LEN		5

#define GAMMA_CMD_CNT		36
#define AOR_ELVSS_CMD_CNT	3
#define HBM_ELVSS_CMD_CNT	26
#define MIN_ACL			0
#define MAX_ACL			3

#define SELF_IMAGE_CNT	7
#define REFRESH_60HZ		60

enum {
	AO_NODE_OFF = 0,
	AO_NODE_ALPM = 1,
	AO_NODE_SELF = 2,
};

enum {
	TEMP_RANGE_0 = 0,		/* 0 < temperature*/
	TEMP_RANGE_1,		/*-10 < temperature < =0*/
	TEMP_RANGE_2,		/*-20<temperature <= -10*/
	TEMP_RANGE_3,		/*temperature <= -20*/
};

#define PANEL_DBG_MSG(on, dev, fmt, ...)				\
do {									\
	if (on)								\
		dev_err(dev, "%s: "fmt, __func__, ##__VA_ARGS__);	\
} while (0)

struct s6e36w1x01 {
	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct mdnie_lite_device	*mdnie;
	struct lcd_platform_data	*pd;
	struct lcd_property		*property;
	struct smart_dimming		*dimming;
	struct regulator		*vdd3;
	struct regulator		*vci;
	struct work_struct		det_work;
	struct mutex			lock;
	unsigned char			*br_map;
	unsigned int			reset_gpio;
	unsigned int			te_gpio;
	unsigned int			det_gpio;
	unsigned int			err_gpio;
	unsigned int			esd_irq;
	unsigned int			err_irq;
	unsigned int			power;
	unsigned int			acl;
	unsigned int			refresh;
	unsigned char			default_hbm;
	unsigned char			hbm_gamma[GAMMA_CMD_CNT];
	unsigned char			hbm_elvss[HBM_ELVSS_CMD_CNT];
	unsigned char			*gamma_tbl[MAX_GAMMA_CNT];
	unsigned char			chip[LDI_CHIP_LEN];
	unsigned int			dbg_cnt;
	unsigned int			ao_mode;
	unsigned int			temp_stage;
	bool				hbm_on;
	bool				alpm_on;
	bool				hlpm_on;
	bool				lp_mode;
	bool				boot_power_on;
	bool				br_ctl;
	bool				scm_on;
	bool				self_mode;
	bool				mcd_on;
	bool				irq_on;
};

static struct class *mdnie_class;
#ifdef CONFIG_LCD_ESD
static struct class *esd_class;
static struct device *esd_dev;
#endif
static unsigned int dbg_mode;
extern unsigned int system_rev;
static const unsigned char TEST_KEY_ON_0[] = {
	0xF0,
	0x5A, 0x5A,
};

static const unsigned char TEST_KEY_OFF_0[] = {
	0xF0,
	0xA5, 0xA5,
};

static const unsigned char TEST_KEY_ON_1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char TEST_KEY_OFF_1[] = {
	0xF1,
	0xA5, 0xA5,
};

static const unsigned char HIDDEN_KEY_ON[] = {
	0xFC,
	0x5A, 0x5A,
};

static const unsigned char HIDDEN_KEY_OFF[] = {
	0xFC,
	0xA5, 0xA5,
};

static const unsigned char SLPIN[] = {
	0x10,
};

static const unsigned char SLPOUT[] = {
	0x11,
};

static const unsigned char PTLON[] = {
	0x12,
};

static const unsigned char NORON[] = {
	0x13,
};

static const unsigned char DISPOFF[] = {
	0x28,
};

static const unsigned char DISPON[] = {
	0x29,
};

static const unsigned char PTLAR[] = {
	0x30,
	0x00, 0x00, 0x01, 0xDF,
};

static const unsigned char TEOFF[] = {
	0x34,
};

static const unsigned char TEON[] = {
	0x35,
	0x00,
};

static const unsigned char IDMOFF[] = {
	0x38,
};

static const unsigned char IDMON[] = {
	0x39,
};

static const unsigned char WRCABC_OFF[] = {
	0x55,
	0x00,
};

static const unsigned char TEMP_OFFSET_GPARA[] = {
	0xB0,
	0x07,
};

static const unsigned char MPS_TEMP_ON[] = {
	0xB6,
	0x8C,
};

static const unsigned char MPS_TEMP_OFF[] = {
	0xB6,
	0x88,
};

static const unsigned char MPS_TSET_1[] = {
	0xB8,
	0x80,
};

static const unsigned char MPS_TSET_2[] = {
	0xB8,
	0x8A,
};

static const unsigned char MPS_TSET_3[] = {
	0xB8,
	0x94,
};

static const unsigned char DEFAULT_GPARA[] = {
	0xB0,
	0x00,
};

static const unsigned char ELVSS_DFLT_GPARA[] = {
	0xB0,
	0x18,
};

static const unsigned char MTP1_GPARA[] = {
	0xB0,
	0x05,
};

static const unsigned char MTP3_GPARA[] = {
	0xB0,
	0x04,
};

static const unsigned char LTPS1_GPARA[] = {
	0xB0,
	0x0F,
};

static const unsigned char LTPS2_GPARA[] = {
	0xB0,
	0x54,
};

static const unsigned char TEMP_OFFSET[] = {
	0xB6,
	0x00, 0x00, 0x00, 0x05,
	0x05, 0x0C, 0x0C, 0x0C,
	0x0C,
};

static const unsigned char HBM_ELVSS[] = {
	0xB6,
	0x88, 0x11
};

static const unsigned char HBM_VINT[] = {
	0xF4,
	0x77, 0x0A,
};

static const unsigned char ACL_8P[] = {
	0xB5,
	0x51, 0x99, 0x0A, 0x0A, 0x0A,
};

static const unsigned char HBM_ACL_ON[] = {
	0xB4,
	0x0D,
};

static const unsigned char HBM_ACL_OFF[] = {
	0xB4,
	0x09,
};

static const unsigned char HBM_ON[] = {
	0x53,
	0xC0,
};

static const unsigned char HBM_OFF[] = {
	0x53,
	0x00,
};

static const unsigned char ACL_ON[] = {
	0x55,
	0x02,
};

static const unsigned char ACL_OFF[] = {
	0x55,
	0x00,
};

static const unsigned char SET_ALPM_FRQ[] = {
	0xBB,
	0x00, 0x00, 0x00, 0x00,
	0x01, 0xE0, 0x47, 0x49,
	0x55, 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x0A,
};

static const unsigned char GAMMA_360[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static const unsigned char LTPS_TIMING1[] = {
	0xCB,
	0x87,
};

static const unsigned char LTPS_TIMING2[] = {
	0xCB,
	0x00, 0x87, 0x69, 0x1A,
	0x69, 0x1A, 0x00, 0x00,
	0x08, 0x03, 0x03, 0x00,
	0x02, 0x02, 0x0F, 0x0F,
	0x0F, 0x0F, 0x0F, 0x0F,
};

static const unsigned char IGNORE_EOT[] = {
	0xE7,
	0xEF, 0x67, 0x03, 0xAF,
	0x47,
};

static const unsigned char MDNIE_LITE_CTL1[] = {
	0xEB,
	0x01, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

static const unsigned char MDNIE_LITE_CTL2[] = {
	0xEC,
	0x01, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x88, 0x01,
	0x88, 0x01, 0x88, 0x05,
	0x90, 0x05, 0x90, 0x05,
	0x90, 0x05, 0x90, 0x0C,
	0x98, 0x0C, 0x98, 0x0C,
	0x98, 0x0C, 0x98, 0x18,
	0xA0, 0x18, 0xA0, 0x18,
	0xA0, 0x18, 0xA0, 0x18,
	0xA0, 0x48, 0xB5, 0x40,
	0xB2, 0x31, 0xAE, 0x29,
	0x1D, 0x54, 0x16, 0x87,
	0x0F, 0x00, 0xFF, 0x00,
	0xFF, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0x00,
	0xFF, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0x00,
	0xFF, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00,
};

static const unsigned char GRAY_TUNE[] = {
	0xE7,
	0xb3, 0x4c, 0xb3, 0x4c,
	0xb3, 0x4c, 0x69, 0x96,
	0x69, 0x96, 0x69, 0x96,
	0xe2, 0x1d, 0xe2, 0x1d,
	0xe2, 0x1d, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0xFF,
	0x01, 0x00,
};

static const unsigned char NEGATIVE_TUNE[] = {
	0xE7,
	0xff, 0x00, 0x00, 0xff,
	0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0x00, 0xff,
	0x00, 0xff, 0x00, 0xff,
	0xff, 0x00, 0x00, 0xff,
	0x00, 0xff, 0x00, 0xff,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0x20,
	0x00, 0x20, 0x00, 0xFF,
	0x01, 0x00,
};

static const unsigned char OUTDOOR_TUNE[] = {
	0xE7,
	0x00, 0xff, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00,
	0x00, 0xff, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00,
	0x00, 0xff, 0xff, 0x00,
	0xff, 0x00, 0xff, 0x00,
	0x00, 0x00, 0x01, 0x88,
	0x01, 0x88, 0x01, 0x88,
	0x05, 0x90, 0x05, 0x90,
	0x05, 0x90, 0x05, 0x90,
	0x0c, 0x98, 0x0c, 0x98,
	0x0c, 0x98, 0x0c, 0x98,
	0x18, 0xa0, 0x18, 0xa0,
	0x18, 0xa0, 0x18, 0xa0,
	0x18, 0xa0, 0x48, 0xb5,
	0x40, 0xb2, 0x31, 0xae,
	0x29, 0x1d, 0x54, 0x16,
	0x87, 0x0f, 0x00, 0xFF,
	0x01, 0x00,
};

static const unsigned char MDNIE_GRAY_ON[] = {
	0xE6,
	0x01, 0x00, 0x30, 0x00,
};

static const unsigned char MDNIE_OUTD_ON[] = {
	0xE6,
	0x01, 0x00, 0x33, 0x05,
};

static const unsigned char MDNIE_CTL_OFF[] = {
	0xE6,
	0x00,
};

static const unsigned char SET_DC_VOL[] = {
	0xF5,
	0xC2, 0x03, 0x0B, 0x1B,
	0x7D, 0x57, 0x22, 0x0A,
};

static const unsigned char AOR_360[] = {
	0xB2,
	0x18, 0x00,
};

static const unsigned char ELVSS_360[] = {
	0xB6,
	0x88, 0x1B,
};

static const unsigned char VINT_360[] = {
	0xF4,
	0x77, 0x0A,
};

static const unsigned char PANEL_UPDATE[] = {
	0xF7,
	0x03,
};

static const unsigned char ETC_GPARA[] = {
	0xB0,
	0x06,
};

static const unsigned char ETC_SET[] = {
	0xFE,
	0x05,
};

static const unsigned char AUTO_CLK_ON[] = {
	0xB9,
	0xBE, 0x07, 0x7D, 0x00,
	0x3B, 0x41, 0x00, 0x00,
	0x0A, 0x04, 0x08, 0x00,
};

static const unsigned char AUTO_CLK_OFF[] = {
	0xB9,
	0xA0, 0x07, 0x7D, 0x00,
	0x3B, 0x41, 0x00, 0x00,
	0x0A, 0x04, 0x08, 0x00,
};

static const unsigned char ALPM_ETC[] = {
	0xBB,
	0x90,
};

static const unsigned char ALPM_ETC_EXIT[] = {
	0xBB,
	0x91,
};

static const unsigned char NORMAL_ON[] = {
	0x53,
	0x00,
};

static const unsigned char HLPM_ON[] = {
	0x53,
	0x01,
};

static const unsigned char ALPM_ON[] = {
	0x53,
	0x02,
};

static const unsigned char ALPM_TEMP_ETC0[] = {
	0xB0,
	0x06,
};

static const unsigned char ALPM_TEMP_ETC1[] = {
	0xF6,
	0x00,
};

static const unsigned char ALPM_TEMP_ETC2[] = {
	0xB0,
	0x2C,
};

static const unsigned char ALPM_TEMP_ETC3[] = {
	0xCB,
	0x5F, 0x4D,
};

static const unsigned char ALPM_TEMP_ETC4[] = {
	0xB0,
	0x60,
};

static const unsigned char ALPM_TEMP_ETC5[] = {
	0xCB,
	0x59, 0x2D, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x00,
	0x31, 0x01, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05,
};

static const unsigned char ALPM_TEMP_ETC6[] = {
	0xBB,
	0x91, 0x00, 0x00, 0x80,
	0x50, 0x3D, 0x3B, 0x4D,
	0x3D, 0x3B, 0x4D, 0x00,
	0x18, 0x18,
};

static const unsigned char HLPM_GAMMA_ETC[] = {
	0xCA,
	0x00, 0x68, 0x00, 0x68,
	0x00, 0x52, 0x80, 0x81,
	0x73, 0x77, 0x78, 0x79,
	0x51, 0x51, 0x49, 0x2D,
	0x2B, 0x10, 0x21, 0x2B,
	0x00, 0x21, 0x08, 0x44,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x33, 0x04,
};

static const unsigned char HLPM_ETC[] = {
	0xBB,
	0x10,
};

static const unsigned char MCD_TEST_ON[] = {
	0xCB,
	0x28, 0x41, 0x01, 0x01,
	0x80, 0x01, 0x45, 0x23,
	0x60, 0x66, 0x06, 0x00,
	0xB1, 0x06, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x06, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x78, 0x00,
	0x00, 0x28, 0x00, 0x00,
	0x6F, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0xAB, 0x8A, 0x24,
	0x03, 0x00, 0x70, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xF5, 0x10, 0x11, 0x0E,
	0x94, 0x14, 0xD4, 0x17,
	0x18, 0x19, 0xFA, 0xFB,
	0xFC, 0xD4, 0x00, 0x00,
	0xD5, 0xD4, 0x54, 0xD4,
	0xC5, 0xC6, 0xE2, 0xF7,
	0x18, 0x19, 0x1A, 0x1B,
	0x1C, 0x14, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x01,
	0x06, 0x01, 0x00, 0x00,
	0x29, 0x2D, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x00,
	0x04, 0x01, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05,
};

static const unsigned char MCD_PWR_ON[] = {
	0xF4,
	0x37,
};

static const unsigned char MCD_TEST_UPDATE[] = {
	0xF7,
	0x02,
};

static const unsigned char MCD_PWR_OFF[] = {
	0xF4,
	0x77,
};

static const unsigned char MCD_TEST_OFF[] = {
	0xCB,
	0x28, 0x41, 0x01, 0x01,
	0x80, 0x01, 0x45, 0x33,
	0x01, 0x23, 0x45, 0x00,
	0xB1, 0x06, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x06, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x53, 0x00,
	0x00, 0x59, 0x00, 0x00,
	0x6F, 0x3C, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x5F, 0x4D, 0x00, 0x00,
	0x03, 0x03, 0x0A, 0x0A,
	0x0A, 0x0A, 0x0A, 0x0A,
	0xF5, 0x10, 0x11, 0x0E,
	0x94, 0x14, 0xD4, 0x17,
	0x18, 0x19, 0xFA, 0xFB,
	0xFC, 0xD4, 0x00, 0x00,
	0xD5, 0xD4, 0x54, 0xD4,
	0xC5, 0xC6, 0xE2, 0xF7,
	0x18, 0x19, 0x1A, 0x1B,
	0x1C, 0x14, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x01,
	0x06, 0x01, 0x00, 0x00,
	0x59, 0x2D, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x00,
	0x31, 0x01, 0x05, 0x05,
	0x05, 0x05, 0x05, 0x05,
};

static const unsigned int br_convert[DIMMING_COUNT] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
	11, 12, 13, 14, 16, 18, 20, 21, 22, 24,
	26, 28, 30, 32, 34, 36, 38, 40, 42, 44,
	46, 48, 50, 52, 54, 56, 58, 59, 60, 62,
	64, 67, 70, 72, 74, 77, 80, 82, 84, 86,
	88, 90, 92, 94, 96, 98, 100
};

static const unsigned char panel_aor_rate_10[] = {0xB2, 0x5C, 0x10};
static const unsigned char panel_aor_rate_11[] = {0xB2, 0x57, 0x10};
static const unsigned char panel_aor_rate_12[] = {0xB2, 0x55, 0x10};
static const unsigned char panel_aor_rate_13[] = {0xB2, 0x53, 0x10};
static const unsigned char panel_aor_rate_14[] = {0xB2, 0x4E, 0x10};
static const unsigned char panel_aor_rate_15[] = {0xB2, 0x4A, 0x10};
static const unsigned char panel_aor_rate_16[] = {0xB2, 0x46, 0x10};
static const unsigned char panel_aor_rate_17[] = {0xB2, 0x44, 0x10};
static const unsigned char panel_aor_rate_19[] = {0xB2, 0x3D, 0x10};
static const unsigned char panel_aor_rate_20[] = {0xB2, 0x3A, 0x10};
static const unsigned char panel_aor_rate_21[] = {0xB2, 0x37, 0x10};
static const unsigned char panel_aor_rate_22[] = {0xB2, 0x34, 0x10};
static const unsigned char panel_aor_rate_24[] = {0xB2, 0x2E, 0x10};
static const unsigned char panel_aor_rate_25[] = {0xB2, 0x2A, 0x10};
static const unsigned char panel_aor_rate_27[] = {0xB2, 0x23, 0x10};
static const unsigned char panel_aor_rate_29[] = {0xB2, 0x1D, 0x10};
static const unsigned char panel_aor_rate_30[] = {0xB2, 0x18, 0x10};
static const unsigned char panel_aor_rate_32[] = {0xB2, 0x13, 0x10};
static const unsigned char panel_aor_rate_34[] = {0xB2, 0x0C, 0x10};
static const unsigned char panel_aor_rate_37[] = {0xB2, 0x02, 0x10};
static const unsigned char panel_aor_rate_39[] = {0xB2, 0xFD, 0x00};
static const unsigned char panel_aor_rate_41[] = {0xB2, 0xF7, 0x00};
static const unsigned char panel_aor_rate_44[] = {0xB2, 0xED, 0x00};
static const unsigned char panel_aor_rate_47[] = {0xB2, 0xE2, 0x00};
static const unsigned char panel_aor_rate_50[] = {0xB2, 0xD8, 0x00};
static const unsigned char panel_aor_rate_53[] = {0xB2, 0xCF, 0x00};
static const unsigned char panel_aor_rate_56[] = {0xB2, 0xC5, 0x00};
static const unsigned char panel_aor_rate_60[] = {0xB2, 0xB4, 0x00};
static const unsigned char panel_aor_rate_64[] = {0xB2, 0xA8, 0x00};
static const unsigned char panel_aor_rate_68[] = {0xB2, 0x9D, 0x00};
static const unsigned char panel_aor_rate_72[] = {0xB2, 0x8F, 0x00};
static const unsigned char panel_aor_rate_77[] = {0xB2, 0x7F, 0x00};
static const unsigned char panel_aor_rate_82[] = {0xB2, 0x8C, 0x00};
static const unsigned char panel_aor_rate_87[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_93[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_98[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_105[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_111[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_119[] = {0xB2, 0x8C, 0x00};
static const unsigned char panel_aor_rate_126[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_134[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_143[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_152[] = {0xB2, 0x8C, 0x00};
static const unsigned char panel_aor_rate_162[] = {0xB2, 0x8B, 0x00};
static const unsigned char panel_aor_rate_172[] = {0xB2, 0x7B, 0x00};
static const unsigned char panel_aor_rate_183[] = {0xB2, 0x69, 0x00};
static const unsigned char panel_aor_rate_195[] = {0xB2, 0x6B, 0x00};
static const unsigned char panel_aor_rate_207[] = {0xB2, 0x45, 0x00};
static const unsigned char panel_aor_rate_220[] = {0xB2, 0x2E, 0x00};
static const unsigned char panel_aor_rate_234[] = {0xB2, 0x1A, 0x00};
static const unsigned char panel_aor_rate_249[] = {0xB2, 0x18, 0x00};
static const unsigned char panel_aor_rate_265[] = {0xB2, 0x18, 0x00};
static const unsigned char panel_aor_rate_282[] = {0xB2, 0x18, 0x00};
static const unsigned char panel_aor_rate_300[] = {0xB2, 0x18, 0x00};
static const unsigned char panel_aor_rate_316[] = {0xB2, 0x18, 0x00};
static const unsigned char panel_aor_rate_333[] = {0xB2, 0x18, 0x00};
static const unsigned char panel_aor_rate_360[] = {0xB2, 0x18, 0x00};
static const unsigned char panel_elvss_10_77[] = {0xB6, 0x88, 0x16};
static const unsigned char panel_elvss_82[] = {0xB6, 0x88, 0x16};
static const unsigned char panel_elvss_87[] = {0xB6, 0x88, 0x16};
static const unsigned char panel_elvss_93[] = {0xB6, 0x88, 0x16};
static const unsigned char panel_elvss_98[] = {0xB6, 0x88, 0x16};
static const unsigned char panel_elvss_105[] = {0xB6, 0x88, 0x15};
static const unsigned char panel_elvss_111[] = {0xB6, 0x88, 0x15};
static const unsigned char panel_elvss_119[] = {0xB6, 0x88, 0x15};
static const unsigned char panel_elvss_126[] = {0xB6, 0x88, 0x15};
static const unsigned char panel_elvss_134[] = {0xB6, 0x88, 0x14};
static const unsigned char panel_elvss_143[] = {0xB6, 0x88, 0x14};
static const unsigned char panel_elvss_152[] = {0xB6, 0x88, 0x14};
static const unsigned char panel_elvss_162[] = {0xB6, 0x88, 0x13};
static const unsigned char panel_elvss_172[] = {0xB6, 0x88, 0x13};
static const unsigned char panel_elvss_183[] = {0xB6, 0x88, 0x13};
static const unsigned char panel_elvss_195[] = {0xB6, 0x88, 0x13};
static const unsigned char panel_elvss_207[] = {0xB6, 0x88, 0x13};
static const unsigned char panel_elvss_220[] = {0xB6, 0x88, 0x13};
static const unsigned char panel_elvss_234[] = {0xB6, 0x88, 0x13};
static const unsigned char panel_elvss_249[] = {0xB6, 0x88, 0x12};
static const unsigned char panel_elvss_265[] = {0xB6, 0x88, 0x12};
static const unsigned char panel_elvss_282[] = {0xB6, 0x88, 0x12};
static const unsigned char panel_elvss_300[] = {0xB6, 0x88, 0x12};
static const unsigned char panel_elvss_316[] = {0xB6, 0x88, 0x12};
static const unsigned char panel_elvss_333[] = {0xB6, 0x88, 0x11};
static const unsigned char panel_elvss_360[] = {0xB6, 0x88, 0x11};
static const unsigned char *aor_tbl[DIMMING_COUNT] = {
	panel_aor_rate_10, panel_aor_rate_11, panel_aor_rate_12,
	panel_aor_rate_13, panel_aor_rate_14, panel_aor_rate_15,
	panel_aor_rate_16, panel_aor_rate_17, panel_aor_rate_19,
	panel_aor_rate_20, panel_aor_rate_21, panel_aor_rate_22,
	panel_aor_rate_24, panel_aor_rate_25, panel_aor_rate_27,
	panel_aor_rate_29, panel_aor_rate_30, panel_aor_rate_32,
	panel_aor_rate_34, panel_aor_rate_37, panel_aor_rate_39,
	panel_aor_rate_41, panel_aor_rate_44, panel_aor_rate_47,
	panel_aor_rate_50, panel_aor_rate_53, panel_aor_rate_56,
	panel_aor_rate_60, panel_aor_rate_64, panel_aor_rate_68,
	panel_aor_rate_72, panel_aor_rate_77, panel_aor_rate_82,
	panel_aor_rate_87, panel_aor_rate_93, panel_aor_rate_98,
	panel_aor_rate_105, panel_aor_rate_111, panel_aor_rate_119,
	panel_aor_rate_126, panel_aor_rate_134, panel_aor_rate_143,
	panel_aor_rate_152, panel_aor_rate_162, panel_aor_rate_172,
	panel_aor_rate_183, panel_aor_rate_195, panel_aor_rate_207,
	panel_aor_rate_220, panel_aor_rate_234, panel_aor_rate_249,
	panel_aor_rate_265, panel_aor_rate_282, panel_aor_rate_300,
	panel_aor_rate_316, panel_aor_rate_333, panel_aor_rate_360
};
static const unsigned char *elvss_tbl[DIMMING_COUNT] = {
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_10_77,
	panel_elvss_10_77, panel_elvss_10_77, panel_elvss_82,
	panel_elvss_87, panel_elvss_93, panel_elvss_98,
	panel_elvss_105, panel_elvss_111, panel_elvss_119,
	panel_elvss_126, panel_elvss_134, panel_elvss_143,
	panel_elvss_152, panel_elvss_162, panel_elvss_172,
	panel_elvss_183, panel_elvss_195, panel_elvss_207,
	panel_elvss_220, panel_elvss_234, panel_elvss_249,
	panel_elvss_265, panel_elvss_282, panel_elvss_300,
	panel_elvss_316, panel_elvss_333, panel_elvss_360
};

static struct regulator_bulk_data supplies[] = {
	{ .supply = "vcc_lcd_3.0", },
	{ .supply = "vcc_lcd_1.8", },
};

static int panel_id;
extern int max77836_comparator_set(bool enable);
int get_panel_id(void)
{
	return panel_id;
}
EXPORT_SYMBOL(get_panel_id);
static int __init panel_id_cmdline(char *mode)
{
	char *pt;

	panel_id = 0;
	if (mode == NULL)
		return 1;

	for (pt = mode; *pt != 0; pt++) {
		panel_id <<= 4;
		switch (*pt) {
		case '0' ... '9':
			panel_id += *pt - '0';
		break;
		case 'a' ... 'f':
			panel_id += 10 + *pt - 'a';
		break;
		case 'A' ... 'F':
			panel_id += 10 + *pt - 'A';
		break;
		}
	}

	pr_info("%s: Panel_ID = 0x%x", __func__, panel_id);

	return 0;
}
__setup("lcdtype=", panel_id_cmdline);

static int panel_update_gamma(struct s6e36w1x01 *lcd, unsigned int brightness)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			lcd->gamma_tbl[brightness], GAMMA_CMD_CNT);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			aor_tbl[brightness], AOR_ELVSS_CMD_CNT);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			elvss_tbl[brightness], AOR_ELVSS_CMD_CNT);
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PANEL_UPDATE, ARRAY_SIZE(PANEL_UPDATE));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	return 0;
}

static void panel_regulator_enable(struct s6e36w1x01 *lcd)
{
	int ret = 0;
	struct lcd_platform_data *pd = NULL;

	pd = lcd->pd;
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	mutex_lock(&lcd->lock);

	/* enable vdd3 (1.8v). */
	ret = regulator_enable(supplies[1].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to enable vdd3 regulator.\n");
		goto out;
	}

	/* enable vci (3.3v). */
	ret = regulator_enable(supplies[0].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to enable vci regulator.\n");
		ret = regulator_disable(supplies[1].consumer);
		goto out;
	}

	usleep_range(pd->power_on_delay * 1000, pd->power_on_delay * 1000);
out:
	mutex_unlock(&lcd->lock);
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");
}

static void panel_regulator_disable(struct s6e36w1x01 *lcd)
{
	int ret = 0;

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	mutex_lock(&lcd->lock);

	/* disable vci (3.3v). */
	ret = regulator_disable(supplies[0].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to disable vci regulator.\n");
		goto out;
	}

	/* disable vdd3 (1.8v). */
	ret = regulator_disable(supplies[1].consumer);
	if (ret) {
		dev_err(lcd->dev, "failed to disable vdd3 regulator.\n");
		ret = regulator_enable(supplies[0].consumer);
		goto out;
	}

out:
	mutex_unlock(&lcd->lock);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");
}

static int panel_hbm_on(struct s6e36w1x01 *lcd, bool enable)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (enable) {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				HBM_ELVSS, ARRAY_SIZE(HBM_ELVSS));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				HBM_VINT, ARRAY_SIZE(HBM_VINT));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				ACL_8P, ARRAY_SIZE(ACL_8P));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				HBM_ACL_ON, ARRAY_SIZE(HBM_ACL_ON));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				HBM_ON, ARRAY_SIZE(HBM_ON));
	} else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				HBM_OFF, ARRAY_SIZE(HBM_OFF));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	return 0;
}

static int panel_temp_offset_comp(struct s6e36w1x01 *lcd, unsigned int stage)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));

	switch (stage) {
	case TEMP_RANGE_0:
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MPS_TEMP_OFF, ARRAY_SIZE(MPS_TEMP_OFF));
		break;
	case TEMP_RANGE_1:
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEMP_OFFSET_GPARA, ARRAY_SIZE(TEMP_OFFSET_GPARA));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MPS_TSET_1, ARRAY_SIZE(MPS_TSET_1));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MPS_TEMP_ON, ARRAY_SIZE(MPS_TEMP_ON));
		break;
	case TEMP_RANGE_2:
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEMP_OFFSET_GPARA, ARRAY_SIZE(TEMP_OFFSET_GPARA));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MPS_TSET_2, ARRAY_SIZE(MPS_TSET_2));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MPS_TEMP_ON, ARRAY_SIZE(MPS_TEMP_ON));
		break;
	case TEMP_RANGE_3:
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEMP_OFFSET_GPARA, ARRAY_SIZE(TEMP_OFFSET_GPARA));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MPS_TSET_3, ARRAY_SIZE(MPS_TSET_3));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MPS_TEMP_ON, ARRAY_SIZE(MPS_TEMP_ON));
		break;
	}

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	return 0;
}

static void panel_self_clock_on(struct s6e36w1x01 *lcd, bool enable)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (enable)
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				AUTO_CLK_ON, ARRAY_SIZE(AUTO_CLK_ON));
	else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				AUTO_CLK_OFF, ARRAY_SIZE(AUTO_CLK_OFF));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));

	ops->cmd_set_end(master);

	lcd->self_mode = enable;
	pr_info("%s: SCM:%s\n", __func__, enable ? "ON" : "OFF");
}

static bool panel_check_power(int cur, int next)
{
	bool ret = false;

	switch (cur) {
	case FB_BLANK_UNBLANK:
		if (next == FB_BLANK_VSYNC_SUSPEND ||
			next == FB_BLANK_POWERDOWN)
			ret = true;
		break;
	case FB_BLANK_NORMAL:
		if (next == FB_BLANK_UNBLANK ||
			next == FB_BLANK_VSYNC_SUSPEND ||
			next == FB_BLANK_POWERDOWN)
			ret = true;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		if (next == FB_BLANK_UNBLANK ||
			next == FB_BLANK_NORMAL)
			ret = true;
		break;
	case FB_BLANK_POWERDOWN:
		if (next == FB_BLANK_UNBLANK ||
			next == FB_BLANK_NORMAL)
			ret = true;
		break;
	default:
		break;
	}

	pr_info("%s[%d->%d]%s\n", __func__, cur, next, ret ? "" : "[invalid]");

	return ret;
}

static int panel_get_power(struct lcd_device *ld)
{
	struct s6e36w1x01 *lcd = lcd_get_data(ld);

	pr_info("%s[%d]\n", __func__, lcd->power);

	return lcd->power;
}

static int panel_set_power(struct lcd_device *ld, int power)
{
	struct s6e36w1x01 *lcd = lcd_get_data(ld);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret = -EINVAL;

	if (!panel_check_power(lcd->power, power)) {
		dev_err(lcd->dev, "%s: invalid power[%d]\n", __func__, power);
		return ret;
	}

	switch (power) {
	case FB_BLANK_UNBLANK:
		lcd->alpm_on = false;
		lcd->dbg_cnt = 0;
	case FB_BLANK_NORMAL:
		if (ops->set_blank_mode)
			ret = ops->set_blank_mode(lcd_to_master(lcd), power);
		if (lcd->scm_on) {
			if (lcd->self_mode)
				lcd->dbg_cnt--;
			else {
				panel_self_clock_on(lcd, lcd->scm_on);
				lcd->dbg_cnt = SELF_IMAGE_CNT;
			}
		}
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		lcd->alpm_on = true;
		 if (ops->set_early_blank_mode)
			ret = ops->set_early_blank_mode(lcd_to_master(lcd),
						power);
		break;
	case FB_BLANK_POWERDOWN:
		lcd->alpm_on = false;
		 if (ops->set_early_blank_mode)
			ret = ops->set_early_blank_mode(lcd_to_master(lcd),
						power);
		break;
	default:
		break;
	}

	if (!ret && lcd->power != power)
		lcd->power = power;

	pr_info("%s[%d]image_cnt[%d]ret[%d]\n", __func__,
		lcd->power, lcd->dbg_cnt, ret);

	return ret;
}

static struct lcd_ops s6e36w1x01_lcd_ops = {
	.get_power = panel_get_power,
	.set_power = panel_set_power,
};

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	unsigned int level = 0;
	struct s6e36w1x01 *lcd = bl_get_data(bd);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(lcd->dev, "lcd brightness should be %d to %d.\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		ret = -EINVAL;
		goto out;
	}

	pr_info("%s[%d]dpms[%d]hbm[%d]power[%d]alpm_on[%d]type[%d]\n",
		"set_br", brightness, atomic_read(&master->dpms_on),
		lcd->hbm_on, lcd->power, lcd->alpm_on, bd->props.type);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		goto out;
	}

	if (bd->props.type != BACKLIGHT_RAW &&
		lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		ret = -EPERM;
		goto out;
	}

	if (!lcd->br_ctl)
		goto out;

	level = lcd->br_map[brightness];

	if (atomic_read(&master->dpms_on)) {
		if (lcd->hbm_on)
			ret = panel_hbm_on(lcd, true);
		else
			ret = panel_update_gamma(lcd, level);
	} else {
		if (ops->set_runtime_active(master)) {
			dev_warn(lcd->dev,
				"failed to set_runtime_active:power[%d]\n",
				lcd->power);
			ret = -EPERM;
			goto out;
		}
		ret = panel_update_gamma(lcd, level);
	}

	if (ret) {
		dev_err(lcd->dev, "failed gamma setting.\n");
		ret = -EIO;
		goto out;
	}

	if (lcd->temp_stage != TEMP_RANGE_0)
		panel_temp_offset_comp(lcd, lcd->temp_stage);

#if defined(CONFIG_SYSTEM_LOAD_ANALYZER)
	store_external_load_factor(LCD_BRIGHTNESS, brightness);
#endif

out:
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");
	return ret;
}

static int panel_update_brightness(struct backlight_device *bd)
{
	int ret = 0;
	struct s6e36w1x01 *lcd = bl_get_data(bd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	bd->props.type = BACKLIGHT_PLATFORM;
	ret = panel_set_brightness(bd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops s6e36w1x01_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_update_brightness,
};

static void panel_power_on(struct mipi_dsim_lcd_device *dsim_dev, int power)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(1, lcd->dev, "S(power = %d)\n", power);

	/* lcd power on */
	if (power) {
		if (lcd->lp_mode)
			goto out;
		panel_regulator_enable(lcd);

		/* Do not reset at booting time if enabled. */
		if (lcd->boot_power_on) {
			lcd->boot_power_on = false;
			goto out;
		}
		/* lcd reset */
		if (lcd->pd->reset)
			lcd->pd->reset(lcd->ld);
	} else {

	/* lcd reset low */
		if (lcd->property->reset_low)
			lcd->property->reset_low();

		panel_regulator_disable(lcd);
	}

out:
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(power = %d)\n", power);
	return;
}

static void panel_get_gamma_tbl(struct s6e36w1x01 *lcd,
						const unsigned char *data)
{
	int i;

	panel_read_gamma(lcd->dimming, data);
	panel_generate_volt_tbl(lcd->dimming);

	for (i = 0; i < MAX_GAMMA_CNT - 1; i++) {
		lcd->gamma_tbl[i][0] = LDI_GAMMA;
		panel_get_gamma(lcd->dimming, i, &lcd->gamma_tbl[i][1]);
	}

	memcpy(lcd->gamma_tbl[MAX_GAMMA_CNT - 1], GAMMA_360, sizeof(GAMMA_360));

	return;
}

static void panel_support_brightness(struct s6e36w1x01 *lcd)
{
	if (panel_id == 0x410000)
		lcd->br_ctl = true;
	else if (panel_id & 0x2000)
		lcd->br_ctl = true;
	else
		lcd->br_ctl = false;
}

static int panel_check_mtp(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned char mtp_data[LDI_MTP4_LEN] = {0, };

	ops->cmd_set_begin(master);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_1, ARRAY_SIZE(TEST_KEY_ON_1));

	dev_info(lcd->dev, "Panel ID = 0x%x\n", panel_id);

	panel_support_brightness(lcd);

	/* GAMMA */
	ops->cmd_read(master, MIPI_DSI_DCS_READ, LDI_MTP4, LDI_MTP4_LEN,
			mtp_data);
	 /* Octa Manufacture Code */
	ops->cmd_read(master, MIPI_DSI_DCS_READ, LDI_CHIP_ID, LDI_CHIP_LEN,
			&lcd->chip[0]);
	dev_info(lcd->dev, "Chip ID = 0x%x\n", lcd->chip[0]);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_1, ARRAY_SIZE(TEST_KEY_OFF_1));
	ops->cmd_set_end(master);

	panel_get_gamma_tbl(lcd, mtp_data);

	return 0;
}

static int panel_set_refresh_rate(struct mipi_dsim_lcd_device *dsim_dev,
						int refresh)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S(enable = %d)\n", refresh);

	/* TODO: support various refresh rate */

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(enable = %d)\n", refresh);

	return 0;
}

static int panel_set_win_update_region(struct mipi_dsim_lcd_device *dsim_dev,
						int offset_x, int offset_y,
						int width, int height)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned char buf[5];

	/* TODO: need to check condition. */
	/*
		if (offset_y >= vm->vactive || height > vm->vactive)
			return -EINVAL;
	*/
	ops->cmd_set_begin(master);

	buf[0] = LDI_CASET;
	buf[1] = (offset_x & 0xff00) >> 8;
	buf[2] = offset_x & 0x00ff;
	buf[3] = ((offset_x + width - 1) & 0xff00) >> 8;
	buf[4] = (offset_x + width - 1) & 0x00ff;

	ops->atomic_cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf,
				ARRAY_SIZE(buf));

	buf[0] = LDI_PASET;
	buf[1] = (offset_y & 0xff00) >> 8;
	buf[2] = offset_y & 0x00ff;
	buf[3] = ((offset_y + height - 1) & 0xff00) >> 8;
	buf[4] = (offset_y + height - 1) & 0x00ff;

	ops->atomic_cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf,
				ARRAY_SIZE(buf));

	ops->cmd_set_end(master);

	return 0;
}

static void panel_display_on(struct mipi_dsim_lcd_device *dsim_dev,
				unsigned int enable)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->lp_mode && enable) {
		lcd->lp_mode = false;
		return;
	}

	ops->cmd_set_begin(master);
	if (enable)
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE,
				DISPON, ARRAY_SIZE(DISPON));
	else
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE,
				DISPOFF, ARRAY_SIZE(DISPOFF));

	ops->cmd_set_end(master);

	dev_info(lcd->dev, "%s[%d]\n", __func__, enable);
}

static void panel_sleep_in(struct s6e36w1x01 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE,
			SLPIN, ARRAY_SIZE(SLPIN));
	ops->cmd_set_end(master);
}

static void panel_hlpm_on(struct s6e36w1x01 *lcd, unsigned int enable)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (enable) {
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ALPM_TEMP_ETC0, ARRAY_SIZE(ALPM_TEMP_ETC0));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ALPM_TEMP_ETC1, ARRAY_SIZE(ALPM_TEMP_ETC1));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ALPM_TEMP_ETC2, ARRAY_SIZE(ALPM_TEMP_ETC2));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			ALPM_TEMP_ETC3, ARRAY_SIZE(ALPM_TEMP_ETC3));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ALPM_TEMP_ETC4, ARRAY_SIZE(ALPM_TEMP_ETC4));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			ALPM_TEMP_ETC5, ARRAY_SIZE(ALPM_TEMP_ETC5));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			HLPM_GAMMA_ETC, ARRAY_SIZE(HLPM_GAMMA_ETC));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PANEL_UPDATE, ARRAY_SIZE(PANEL_UPDATE));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			HLPM_ETC, ARRAY_SIZE(HLPM_ETC));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ALPM_ON, ARRAY_SIZE(ALPM_ON));
	} else
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				NORMAL_ON, ARRAY_SIZE(NORMAL_ON));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	pr_info("%s: ALPM:%s\n", __func__, lcd->alpm_on ? "ON" : "OFF");
}


static void panel_alpm_on(struct s6e36w1x01 *lcd, unsigned int enable)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	int id3 = panel_id & 0xFF;

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (enable) {
		if (id3 < 0x12) {
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_TEMP_ETC0, ARRAY_SIZE(ALPM_TEMP_ETC0));
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_TEMP_ETC1, ARRAY_SIZE(ALPM_TEMP_ETC1));
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_TEMP_ETC2, ARRAY_SIZE(ALPM_TEMP_ETC2));
			ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				ALPM_TEMP_ETC3, ARRAY_SIZE(ALPM_TEMP_ETC3));
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_TEMP_ETC4, ARRAY_SIZE(ALPM_TEMP_ETC4));
			ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				ALPM_TEMP_ETC5, ARRAY_SIZE(ALPM_TEMP_ETC5));
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				PANEL_UPDATE, ARRAY_SIZE(PANEL_UPDATE));
			ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				ALPM_TEMP_ETC6, ARRAY_SIZE(ALPM_TEMP_ETC6));
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_ON, ARRAY_SIZE(ALPM_ON));
		} else {
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_ETC, ARRAY_SIZE(ALPM_ETC));
			ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_ON, ARRAY_SIZE(ALPM_ON));
		}
	} else {
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				ALPM_ETC_EXIT, ARRAY_SIZE(ALPM_ETC_EXIT));
		ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
				NORMAL_ON, ARRAY_SIZE(NORMAL_ON));
	}
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	pr_info("%s: ALPM:%s\n", __func__, lcd->alpm_on ? "ON" : "OFF");
}

static void panel_pm_check(struct mipi_dsim_lcd_device *dsim_dev,
						bool *pm_skip)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	if (lcd->lp_mode && lcd->alpm_on)
		*pm_skip = true;
	else
		*pm_skip = false;

	return;
}

static void panel_te_active(struct mipi_dsim_lcd_device *dsim_dev,
						bool enable)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S(enable = %d)\n", enable);

	if (lcd->irq_on == enable) {
		pr_err("[panel]unbalance for te irq\n");
		goto out;
	}

	if (enable)
		enable_irq(gpio_to_irq(lcd->te_gpio));
	else
		disable_irq(gpio_to_irq(lcd->te_gpio));

	lcd->irq_on = enable;

out:
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(enable = %d)\n", enable);
	return;
}

void panel_mdnie_set(struct s6e36w1x01 *lcd, enum mdnie_scenario scenario)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));

	switch (scenario) {
	case SCENARIO_GRAY:
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				GRAY_TUNE, ARRAY_SIZE(GRAY_TUNE));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MDNIE_GRAY_ON, ARRAY_SIZE(MDNIE_GRAY_ON));
		break;
	default:
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MDNIE_CTL_OFF, ARRAY_SIZE(MDNIE_CTL_OFF));
		break;
	}

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PANEL_UPDATE, ARRAY_SIZE(PANEL_UPDATE));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	dev_info(lcd->dev, "[mDNIe]set[%d]\n", scenario);

	return;
}

static void panel_mdnie_outdoor_set(struct s6e36w1x01 *lcd,
			enum mdnie_outdoor on)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (on) {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				OUTDOOR_TUNE, ARRAY_SIZE(OUTDOOR_TUNE));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MDNIE_OUTD_ON, ARRAY_SIZE(MDNIE_OUTD_ON));
	} else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MDNIE_CTL_OFF, ARRAY_SIZE(MDNIE_CTL_OFF));

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PANEL_UPDATE, ARRAY_SIZE(PANEL_UPDATE));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	dev_info(lcd->dev, "[mDNIe]outdoor[%s]\n", on ? "ON" : "OFF");

	return;
}

static void panel_display_init(struct s6e36w1x01 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);

	/* Test key enable */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_1, ARRAY_SIZE(TEST_KEY_ON_1));
	/* sleep out */
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE,
			SLPOUT, ARRAY_SIZE(SLPOUT));

	usleep_range(120000, 120000);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			GAMMA_360, ARRAY_SIZE(GAMMA_360));
	if (lcd->br_ctl) {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				AOR_360, ARRAY_SIZE(AOR_360));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				ELVSS_360, ARRAY_SIZE(ELVSS_360));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				VINT_360, ARRAY_SIZE(VINT_360));
	}
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PANEL_UPDATE, ARRAY_SIZE(PANEL_UPDATE));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			TEON, ARRAY_SIZE(TEON));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			HIDDEN_KEY_ON, ARRAY_SIZE(HIDDEN_KEY_ON));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ETC_GPARA, ARRAY_SIZE(ETC_GPARA));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ETC_SET, ARRAY_SIZE(ETC_SET));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			HIDDEN_KEY_OFF, ARRAY_SIZE(HIDDEN_KEY_OFF));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_1, ARRAY_SIZE(TEST_KEY_OFF_1));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));

	ops->cmd_set_end(master);

}

static void panel_set_sequence(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (lcd->lp_mode) {
		if (lcd->self_mode)
			panel_self_clock_on(lcd, lcd->scm_on);

		panel_alpm_on(lcd, lcd->alpm_on);
		goto out;
	}

	panel_display_init(lcd);

	lcd->bd->props.type = BACKLIGHT_RAW;
	panel_set_brightness(lcd->bd);

out:
	if (mdnie->scenario == SCENARIO_GRAY)
		panel_mdnie_set(lcd, mdnie->scenario);

	if (mdnie->outdoor == OUTDOOR_ON)
		panel_mdnie_outdoor_set(lcd, mdnie->outdoor);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(refresh = %d)\n", lcd->refresh);
}

static void panel_frame_freq_set(struct s6e36w1x01 *lcd)
{
	/* TODO: implement frame freq set */
}

static void panel_mcd_test_on(struct s6e36w1x01 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (lcd->mcd_on) {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MCD_PWR_ON, ARRAY_SIZE(MCD_PWR_ON));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MCD_TEST_ON, ARRAY_SIZE(MCD_TEST_ON));
	} else {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MCD_PWR_OFF, ARRAY_SIZE(MCD_PWR_OFF));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MCD_TEST_OFF, ARRAY_SIZE(MCD_TEST_OFF));
	}
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			MCD_TEST_UPDATE, ARRAY_SIZE(MCD_TEST_UPDATE));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	msleep(100);
}

irqreturn_t panel_te_isr(int irq, void *dev_id)
{
	struct s6e36w1x01 *lcd = dev_id;
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	struct mipi_dsim_master_ops *ops = master->master_ops;

	if (ops->te_handler)
		ops->te_handler(master);

	return IRQ_HANDLED;
}

static ssize_t panel_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "SDC_%06x\n", panel_id);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t panel_mcd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->mcd_on ? "on" : "off");
}

static ssize_t panel_mcd_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->mcd_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->mcd_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);
		return -EPERM;
	}

	panel_mcd_test_on(lcd);

	return size;
}

static ssize_t panel_hlpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->hlpm_on ? "on" : "off");
}

static ssize_t panel_hlpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hlpm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->hlpm_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);
		return -EPERM;
	}

	panel_hlpm_on(lcd, lcd->hlpm_on);

	pr_info("%s: val[%d]\n", __func__, lcd->hlpm_on);

	return size;
}

static ssize_t panel_alpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	int len = 0;

	pr_debug("%s:val[%d]\n", __func__, lcd->ao_mode);

	switch (lcd->ao_mode) {
	case AO_NODE_OFF:
		len = sprintf(buf, "%s\n", "off");
		break;
	case AO_NODE_ALPM:
		len = sprintf(buf, "%s\n", "on");
		break;
	case AO_NODE_SELF:
		len = sprintf(buf, "%s\n", "self");
		break;
	default:
		dev_warn(dev, "invalid status.\n");
		break;
	}

	return len;
}

static ssize_t panel_alpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2))
		lcd->ao_mode = AO_NODE_ALPM;
	else if (!strncmp(buf, "off", 3))
		lcd->ao_mode = AO_NODE_OFF;
	else if (!strncmp(buf, "self", 4))
		lcd->ao_mode = AO_NODE_SELF;
	else
		dev_warn(dev, "invalid command.\n");

	pr_info("%s:val[%d]\n", __func__, lcd->ao_mode);

	return size;
}

static ssize_t panel_scm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->scm_on ? "on" : "off");
}

static ssize_t panel_scm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2))
		lcd->scm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->scm_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	pr_info("%s: val[%d]\n", __func__, lcd->scm_on);

	return size;
}

static void panel_acl_update(struct s6e36w1x01 *lcd, unsigned int value)
{
	/* TODO: implement acl update function */
}

static ssize_t panel_acl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->acl);
}

static ssize_t panel_acl_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned long value;
	int rc;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);
		return -EPERM;
	}

	panel_acl_update(lcd, value);

	lcd->acl = value;

	dev_info(lcd->dev, "acl control[%d]\n", lcd->acl);

	return size;
}

static ssize_t panel_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

static ssize_t panel_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		lcd->hbm_on = 0;
	else {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		/*
		 *  let the fimd know the smies status
		 *  before DPMS ON
		 */
		ops->set_smies_active(master, lcd->hbm_on);
		dev_warn(lcd->dev, "hbm control before lcd enable.\n");
		return -EPERM;
	}

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);
		return -EPERM;
	}

	panel_hbm_on(lcd, lcd->hbm_on);

	dev_info(lcd->dev, "HBM %s.\n", lcd->hbm_on ? "ON" : "OFF");

	return size;
}

static ssize_t panel_elvss_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", lcd->temp_stage);
}

static ssize_t panel_elvss_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned long value;
	int rc;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	lcd->temp_stage = value;

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);
		return -EPERM;
	}

	panel_temp_offset_comp(lcd, lcd->temp_stage);

	dev_info(lcd->dev, "ELVSS temp stage[%d].\n", lcd->temp_stage);

	return size;
}

static ssize_t panel_octa_chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	char temp[20];

	sprintf(temp, "%02x%02x%02x%02x%02x\n",
				lcd->chip[0], lcd->chip[1], lcd->chip[2],
				lcd->chip[3], lcd->chip[4]);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t panel_refresh_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->refresh);
}

static ssize_t panel_refresh_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (strncmp(buf, "30", 2) && strncmp(buf, "60", 2)) {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);
		return -EPERM;
	}

	if (!strncmp(buf, "30", 2)) {
		lcd->refresh = 30;
		panel_frame_freq_set(lcd);
	} else {
		lcd->refresh = 60;
		panel_frame_freq_set(lcd);
	}

	return size;
}

DEVICE_ATTR(lcd_type, 0444, panel_lcd_type_show, NULL);
static struct device_attribute ld_dev_attrs[] = {
	__ATTR(mcd_test, S_IRUGO | S_IWUSR, panel_mcd_show, panel_mcd_store),
	__ATTR(alpm, S_IRUGO | S_IWUSR, panel_alpm_show, panel_alpm_store),
	__ATTR(hlpm, S_IRUGO | S_IWUSR, panel_hlpm_show, panel_hlpm_store),
	__ATTR(acl, S_IRUGO | S_IWUSR, panel_acl_show, panel_acl_store),
	__ATTR(scm, S_IRUGO | S_IWUSR, panel_scm_show, panel_scm_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, panel_hbm_show, panel_hbm_store),
	__ATTR(elvss, S_IRUGO | S_IWUSR, panel_elvss_show, panel_elvss_store),
	__ATTR(chip_id, S_IRUGO, panel_octa_chip_id_show, NULL),
	__ATTR(refresh, S_IRUGO | S_IWUSR,
			panel_refresh_show, panel_refresh_store),
};

#ifdef CONFIG_LCD_ESD
static void panel_esd_detect_work(struct work_struct *work)
{
	struct s6e36w1x01 *lcd = container_of(work,
						struct s6e36w1x01, det_work);
	char *event_string = "LCD_ESD=ON";
	char *envp[] = {event_string, NULL};
	int ret = 0;

	if (!POWER_IS_OFF(lcd->power)) {
		kobject_uevent_env(&esd_dev->kobj,
			KOBJ_CHANGE, envp);
		dev_info(lcd->dev, "Send uevent. ESD DETECTED\n");
		ret = max77836_comparator_set(true);
		if (ret < 0)
			dev_info(lcd->dev, "%s: comparator set failed.\n",
					__func__);
	}
}

irqreturn_t panel_esd_interrupt(int irq, void *dev_id)
{
	struct s6e36w1x01 *lcd = dev_id;

	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0x00));
	s3c_gpio_cfgpin(lcd->err_gpio, S3C_GPIO_SFN(0x00));

	if (!work_busy(&lcd->det_work)) {
		schedule_work(&lcd->det_work);
		dev_info(lcd->dev, "add esd schedule_work by irq[%d]]\n", irq);
	}

	return IRQ_HANDLED;
}
#endif

static ssize_t panel_scenario_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->scenario);
}

static ssize_t panel_scenario_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	struct mdnie_lite_device *mdnie = lcd->mdnie;
	int value;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	sscanf(buf, "%d", &value);

	dev_info(lcd->dev, "[mDNIe]cur[%d]new[%d]\n",
			mdnie->scenario, value);

	if (mdnie->scenario == value)
		return size;

	mdnie->scenario = value;

	if (ops->set_runtime_active(master))
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);

	panel_mdnie_set(lcd, value);

	mdnie->scenario = value;

	return size;
}

static ssize_t panel_outdoor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->outdoor);
}

static ssize_t panel_outdoor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	struct mdnie_lite_device *mdnie = lcd->mdnie;
	int value;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	sscanf(buf, "%d", &value);

	if (value >= OUTDOOR_MAX) {
		dev_warn(lcd->dev, "invalid outdoor mode set\n");
		return -EINVAL;
	}
	mdnie->outdoor = value;

	if (ops->set_runtime_active(master))
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);

	panel_mdnie_outdoor_set(lcd, value);

	mdnie->outdoor = value;

	return size;
}

#if MDNIE_TUNE
#include <linux/firmware.h>
#define MDNIECTL1_SIZE	5
#define MDNIECTL2_SIZE	75
#define MAX_REG_NUM	10
static bool tune_en;
unsigned char TUNE_ON[MDNIECTL1_SIZE] = {0,};
unsigned char TUNE_E7[MDNIECTL2_SIZE] = {0,};
static void mdnie_get_tune_dat(const u8 *data)
{
	char *str = NULL;
	unsigned char tune[100];
	unsigned int val1, val2, val3 = 0;
	int i = 0, len = 0, reg_num = 0;
	int ret;

	while ((str = strsep((char **)&data, "\n"))) {
		ret = sscanf(str, "0x%2x,", &reg_num);
		if (ret == 1)
			break;
	}
	while ((str = strsep((char **)&data, "\n"))) {
		ret = sscanf(str, "0x%2x,0x%2x,", &val1, &val2);
		if (ret == 2) {
			tune[i] = val1;
			i++;
			for (len = 0; len < val2; len++) {
				str = strsep((char **)&data, "\n");
				ret = sscanf(str, "0x%2x,", &val3);
				if (ret == 1) {
					tune[i] = val3;
					i++;
				}
			}
			reg_num--;
			if (reg_num == 0)
				break;
		}
	}

	memcpy(TUNE_ON, tune, MDNIECTL1_SIZE);
	memcpy(TUNE_E7, &tune[MDNIECTL1_SIZE], MDNIECTL2_SIZE);

}

static ssize_t panel_mdnie_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 5, "%s\n", tune_en ? "on" : "off");
}

static ssize_t panel_mdnie_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	const struct firmware *fw;
	char fw_path[256];
	int ret;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2)) {
		tune_en = true;
		pr_info("%s: mDNIe tune enable\n", __func__);
	} else if (!strncmp(buf, "off", 3)) {
		tune_en = false;
		pr_info("%s: mDNIe tune disable\n", __func__);
		goto out;
	} else {
		dev_warn(dev, "invalid command.\n");
		return size;
	}

	ret = request_firmware((const struct firmware **)&fw,
					"mdnie_tune.dat", dev);
	if (ret) {
		dev_err(dev, "ret: %d, %s: fail to request %s\n",
					ret, __func__, fw_path);
		return ret;
	}
	mdnie_get_tune_dat(fw->data);

	release_firmware(fw);

out:
	if (ops->set_runtime_active(master)) {
		dev_warn(lcd->dev,
			"failed to set_runtime_active:power[%d]\n",
			lcd->power);
		return -EPERM;
	}

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_0, ARRAY_SIZE(TEST_KEY_ON_0));
	if (tune_en) {
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TUNE_E7, ARRAY_SIZE(TUNE_E7));
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TUNE_ON, ARRAY_SIZE(TUNE_ON));
	} else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				MDNIE_CTL_OFF, ARRAY_SIZE(MDNIE_CTL_OFF));

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			PANEL_UPDATE, ARRAY_SIZE(PANEL_UPDATE));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_0, ARRAY_SIZE(TEST_KEY_OFF_0));
	ops->cmd_set_end(master);

	return size;
}
#endif

static struct device_attribute mdnie_attrs[] = {
	__ATTR(scenario, 0664, panel_scenario_show, panel_scenario_store),
	__ATTR(outdoor, 0664, panel_outdoor_show, panel_outdoor_store),
#if MDNIE_TUNE
	__ATTR(tune, 0664, panel_mdnie_show, panel_mdnie_store),
#endif
};

void panel_mdnie_lite_init(struct s6e36w1x01 *lcd)
{
	struct mdnie_lite_device *mdnie;
	int i;

	mdnie = kzalloc(sizeof(struct mdnie_lite_device), GFP_KERNEL);
	if (!mdnie) {
		pr_err("failed to allocate mdnie object.\n");
		return;
	}

	mdnie_class = class_create(THIS_MODULE, "extension");
	if (IS_ERR(mdnie_class)) {
		pr_err("Failed to create class(mdnie)!\n");
		goto err_free_mdnie;
	}

	mdnie->dev = device_create(mdnie_class, NULL, 0, NULL, "mdnie");
	if (IS_ERR(&mdnie->dev)) {
		pr_err("Failed to create device(mdnie)!\n");
		goto err_free_mdnie;
	}

	for (i = 0; i < ARRAY_SIZE(mdnie_attrs); i++) {
		if (device_create_file(mdnie->dev, &mdnie_attrs[i]) < 0)
			pr_err("Failed to create device file(%s)!\n",
				mdnie_attrs[i].attr.name);
	}

	mdnie->scenario = SCENARIO_UI;
	lcd->mdnie = mdnie;

	dev_set_drvdata(mdnie->dev, lcd);

	return;

err_free_mdnie:
	kfree(mdnie);
}

static int panel_probe(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w1x01 *lcd;
	struct mipi_dsim_platform_data *dsim_pd;
	struct fb_videomode *timing;
	int start = 0, end, i, offset = 0;
	int ret;

	if (!get_panel_id()) {
		pr_err("No lcd attached!\n");
		return -ENODEV;
	}

	lcd = devm_kzalloc(&dsim_dev->dev, sizeof(struct s6e36w1x01),
				GFP_KERNEL);
	if (!lcd) {
		dev_err(&dsim_dev->dev,
				"failed to allocate s6e36w1x01 structure.\n");
		return -ENOMEM;
	}

	lcd->dsim_dev = dsim_dev;
	lcd->dev = &dsim_dev->dev;
	lcd->pd = (struct lcd_platform_data *)dsim_dev->platform_data;
	lcd->boot_power_on = lcd->pd->lcd_enabled;

	lcd->dimming = devm_kzalloc(&dsim_dev->dev, sizeof(*lcd->dimming),
								GFP_KERNEL);
	if (!lcd->dimming) {
		dev_err(&dsim_dev->dev, "failed to allocate dimming.\n");
		ret = -ENOMEM;
		goto err_free_lcd;
	}

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i] = (unsigned char *)
			kzalloc(sizeof(unsigned char) * GAMMA_CMD_CNT,
								GFP_KERNEL);
		if (!lcd->gamma_tbl[i]) {
			dev_err(&dsim_dev->dev,
					"failed to allocate gamma_tbl\n");
			ret = -ENOMEM;
			goto err_free_dimming;
		}
	}

	lcd->br_map = devm_kzalloc(&dsim_dev->dev,
		sizeof(unsigned char) * (MAX_BRIGHTNESS + 1), GFP_KERNEL);
	if (!lcd->br_map) {
		dev_err(&dsim_dev->dev, "failed to allocate br_map\n");
		ret = -ENOMEM;
		goto err_free_gamma_tbl;
	}

	for (i = 0; i < DIMMING_COUNT; i++) {
		end = br_convert[offset++];
		memset(&lcd->br_map[start], i, end - start + 1);
		start = end + 1;
	}

	mutex_init(&lcd->lock);

	ret = devm_regulator_bulk_get(lcd->dev, ARRAY_SIZE(supplies), supplies);
	if (ret) {
		dev_err(lcd->dev, "Failed to get regulator: %d\n", ret);
		return ret;
	}

	lcd->bd = backlight_device_register("s6e36w1x01-bl", lcd->dev, lcd,
			&s6e36w1x01_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dev_err(lcd->dev, "failed to register backlight ops.\n");
		ret = PTR_ERR(lcd->bd);
		goto err_free_br_map;
	}

	lcd->ld = lcd_device_register("s6e36w1x01", lcd->dev, lcd,
					&s6e36w1x01_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(lcd->ld);
		goto err_unregister_bd;
	}

	if (lcd->pd)
		lcd->property = lcd->pd->pdata;

	lcd->te_gpio = lcd->property->te_gpio;
	ret = devm_request_irq(lcd->dev, gpio_to_irq(lcd->te_gpio),
				panel_te_isr,
				IRQF_DISABLED | IRQF_TRIGGER_RISING, "TE", lcd);
	if (ret < 0) {
		dev_err(lcd->dev, "failed to request te irq.\n");
		goto err_unregister_lcd;
	}

	lcd->irq_on = true;

#ifdef CONFIG_LCD_ESD
	esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(esd_class)) {
		dev_err(lcd->dev, "Failed to create class(lcd_event)!\n");
		ret = PTR_ERR(esd_class);
		goto err_unregister_lcd;
	}

	esd_dev = device_create(esd_class, NULL, 0, NULL, "esd");

	INIT_WORK(&lcd->det_work, panel_esd_detect_work);

	if (system_rev < 0x6)
		lcd->det_gpio = EXYNOS3_GPX1(7);
	else
		lcd->det_gpio = lcd->property->det_gpio;
	lcd->esd_irq = gpio_to_irq(lcd->det_gpio);
	dev_info(lcd->dev, "esd_irq_num [%d]\n", lcd->esd_irq);
	ret = devm_request_irq(lcd->dev, lcd->esd_irq,
				panel_esd_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "esd",
				lcd);
	if (ret < 0) {
		dev_err(lcd->dev, "failed to request det irq.\n");
		goto err_unregister_lcd;
	}

	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(lcd->det_gpio, S3C_GPIO_PULL_DOWN);

	if (system_rev < 0x6)
		lcd->err_gpio = EXYNOS3_GPX1(0);
	else
		lcd->err_gpio = lcd->property->err_gpio;
	lcd->err_irq = gpio_to_irq(lcd->err_gpio);
	dev_info(lcd->dev, "err_irq_num [%d]\n", lcd->err_irq);
	ret = devm_request_irq(lcd->dev, lcd->err_irq,
				panel_esd_interrupt,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT, "err_fg",
				lcd);
	if (ret < 0) {
		dev_err(lcd->dev, "failed to request err irq.\n");
		goto err_unregister_lcd;
	}
	s3c_gpio_cfgpin(lcd->err_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(lcd->err_gpio, S3C_GPIO_PULL_DOWN);
#endif

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, " failed to create lcd_type sysfs.\n");
		goto err_unregister_lcd;
	}

	for (i = 0; i < ARRAY_SIZE(ld_dev_attrs); i++) {
		ret = device_create_file(lcd->dev,
				&ld_dev_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add ld dev sysfs entries\n");
			for (i--; i >= 0; i--)
				device_remove_file(lcd->dev,
					&ld_dev_attrs[i]);
			goto err_remove_lcd_type_file;
		}
	}

	lcd->power = FB_BLANK_UNBLANK;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd->temp_stage = TEMP_RANGE_0;
	dsim_pd = (struct mipi_dsim_platform_data *)lcd_to_master(lcd)->pd;
	timing = (struct fb_videomode *)dsim_pd->lcd_panel_info;
#if 0
	lcd->refresh = timing->refresh;
#else
	lcd->refresh = REFRESH_60HZ;
#endif
	dev_set_drvdata(&dsim_dev->dev, lcd);

	if (lcd->property->enable_mdnie)
		panel_mdnie_lite_init(lcd);

	dev_info(lcd->dev, "probed s6e36w1x01 panel driver.\n");

	return 0;
err_remove_lcd_type_file:
	device_remove_file(&lcd->ld->dev, &dev_attr_lcd_type);
err_unregister_lcd:
	lcd_device_unregister(lcd->ld);
err_unregister_bd:
	backlight_device_unregister(lcd->bd);
err_free_br_map:
	devm_kfree(&dsim_dev->dev, lcd->br_map);
err_free_gamma_tbl:
	for (i = 0; i < MAX_GAMMA_CNT; i++)
		if (lcd->gamma_tbl[i])
			devm_kfree(&dsim_dev->dev, lcd->gamma_tbl[i]);
err_free_dimming:
	devm_kfree(&dsim_dev->dev, lcd->dimming);
err_free_lcd:
	devm_kfree(&dsim_dev->dev, lcd);

	return ret;
}

#ifdef CONFIG_PM
static int panel_suspend(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (lcd->alpm_on) {
		if (!lcd->lp_mode) {
			panel_alpm_on(lcd, lcd->alpm_on);
			lcd->lp_mode = true;
		}
		goto out;
	}

#ifdef CONFIG_LCD_ESD
	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0x00));
	s3c_gpio_cfgpin(lcd->err_gpio, S3C_GPIO_SFN(0x00));
#endif

	panel_display_on(dsim_dev, 0);
	panel_sleep_in(lcd);

	usleep_range(lcd->pd->power_off_delay * 1000,
			lcd->pd->power_off_delay * 1000);

	panel_power_on(dsim_dev, 0);
	lcd->lp_mode = false;

out:
	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return 0;
}

static int panel_resume(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w1x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

#ifdef CONFIG_LCD_ESD
	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_cfgpin(lcd->err_gpio, S3C_GPIO_SFN(0xf));
#endif

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return 0;
}
#else
#define panel_suspend	NULL
#define panel_resume	NULL
#endif

static struct mipi_dsim_lcd_driver panel_dsim_ddi_driver = {
	.name = "s6e36w1x01",
	.id = -1,
	.if_type = DSIM_COMMAND,
	.power_on = panel_power_on,
	.set_sequence = panel_set_sequence,
	.display_on = panel_display_on,
	.probe = panel_probe,
	.suspend = panel_suspend,
	.resume = panel_resume,
	.check_mtp = panel_check_mtp,
	.set_refresh_rate = panel_set_refresh_rate,
	.set_partial_region = panel_set_win_update_region,
	.panel_pm_check = panel_pm_check,
	.te_active = panel_te_active,
};

static int panel_init(void)
{
	int ret;

	ret = exynos_mipi_dsi_register_lcd_driver(&panel_dsim_ddi_driver);
	if (ret < 0) {
		pr_err("failed to register mipi lcd driver.\n");
		/* TODO: unregister lcd device. */
		return ret;
	}

	return 0;
}

static void panel_exit(void)
{
}

module_init(panel_init);
module_exit(panel_exit);

MODULE_AUTHOR("Tae-Heon Kim <th908.kim@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based s6e36w1x01 AMOLED LCD Panel Driver");
MODULE_LICENSE("GPL");
