/* linux/drivers/video/exynos/s6e36w0x01.c
 *
 * MIPI-DSI based S6E36W0X01 AMOLED lcd 1.84 inch panel driver.
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

#include "s6e36w0x01_dimming.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		100
#define DEFAULT_BRIGHTNESS	80

#define POWER_IS_ON(pwr)	((pwr) == FB_BLANK_UNBLANK)
#define POWER_IS_OFF(pwr)	((pwr) == FB_BLANK_POWERDOWN)
#define POWER_IS_NRM(pwr)	((pwr) == FB_BLANK_NORMAL)

#define lcd_to_master(a)	(a->dsim_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

#define LDI_ID_REG			0x04
#define LDI_MTP_SET0		0xC9
#define LDI_MTP_SET1		0xB1
#define LDI_MTP_SET2		0xBC
#define LDI_MTP_SET3		0xC7
#define LDI_MTP_SET4		0xB6
#define LDI_CASET_REG		0x2A
#define LDI_PASET_REG		0x2B
#define LDI_ID_LEN			3
#define LDI_MTP_LEN1		6
#define LDI_MTP_LEN2		12
#define LDI_MTP_LEN3		4
#define LDI_MTP_LEN4		1
#define MAX_MTP_CNT		33
#define GAMMA_CMD_CNT		28
#define AOR_ELVSS_CMD_CNT	3
#define ACL_CMD_CNT		66
#define MIN_ACL				0
#define MAX_ACL				3

#define CANDELA_2			2
#define CANDELA_60			60
#define CANDELA_120			120
#define CANDELA_200			200
#define CANDELA_300			300

#define REFRESH_30HZ		30
#define REFRESH_60HZ		60

#define PANEL_DBG_MSG(on, dev, fmt, ...)				\
{									\
	if (on)								\
		dev_err(dev, "%s: "fmt, __func__, ##__VA_ARGS__);	\
}

enum {
	DSIM_NONE_STATE,
	DSIM_RESUME_COMPLETE,
	DSIM_FRAME_DONE,
};

struct s6e36w0x01 {
	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct mipi_dsim_lcd_device	*dsim_dev;
	struct lcd_platform_data	*pd;
	struct lcd_property	*property;
	struct smart_dimming		*dimming;
	struct regulator		*vdd3;
	struct regulator		*vci;
	struct work_struct		det_work;
	struct mutex			lock;
	unsigned int			reset_gpio;
	unsigned int			te_gpio;
	unsigned int			det_gpio;
	unsigned int			esd_irq;
	unsigned int			power;
	unsigned int			gamma;
	unsigned int			acl;
	unsigned int			cur_acl;
	unsigned int			refresh;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			elvss_hbm;
	unsigned char			default_hbm;
	unsigned char			hbm_gamma[MAX_MTP_CNT];
	unsigned char			*gamma_tbl[MAX_GAMMA_CNT];
	bool				hbm_on;
	bool				alpm_on;
	bool				lp_mode;
	bool				boot_power_on;
	bool				compensation_on;
};

#ifdef CONFIG_LCD_ESD
static struct class *esd_class;
static struct device *esd_dev;
#endif
static unsigned int dbg_mode;

static const unsigned char TEST_KEY_ON_1[] = {
	0xF0,
	0x5A, 0x5A,
};

static const unsigned char OLED_CMD_TEST_KEY1_OFF[] = {
	0xF0,
	0xA5, 0xA5,
};

static const unsigned char TEST_KEY_ON_2[] = {
	0xF1,
	0x5A, 0x5A
};

static const unsigned char TEST_KEY_OFF_2[] = {
	0xF1,
	0xA5, 0xA5
};

static const unsigned char TEST_KEY3_ON[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char TEST_KEY3_OFF[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char TEMP_OFFSET_GPARAM[] = {
	0xB0,
	0x0B
};

static const unsigned char TEMP_OFFSET[] = {
	0xB6,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x08, 0x08,
	0x08
};

static const unsigned char PARAM_POS_DEFAULT[] = {
	0xb0,
	0x00, 0x00,
};

static const unsigned char PARAM_POS_HBM_ELVSS[] = {
	0xb0,
	0x07, 0x00
};

static const unsigned char PARAM_POS_DEF_ELVSS[] = {
	0xb0,
	0x18, 0x00,
};

static const unsigned char PARAM_POS_HBM_SET1[] = {
	0xb0,
	0x05, 0x00
};

static const unsigned char PARAM_POS_HBM_SET2[] = {
	0xb0,
	0x04, 0x00
};

/* Gamma for 200 nit */
static const unsigned char GAMMA_120[] = {
	0xCA,
	0x00, 0xE7, 0x00, 0xE6,
	0x00, 0xE0, 0x82, 0x82,
	0x82, 0x81, 0x82, 0x81,
	0x83, 0x83, 0x83, 0x81,
	0x82, 0x82, 0x82, 0x83,
	0x83, 0x81, 0x81, 0x80,
	0x83, 0x7F, 0x81, 0x85,
	0x86, 0x86, 0x00, 0x00,
	0x00
};

static const unsigned char AID_120[] = {
	0xB2,
	0xE0, 0xB3,
};

static const unsigned char ELVSS_120[] = {
	0xB6,
	0x88, 0x20,
};

static const unsigned char UPDATE_GAMMA[] = {
	0xF7,
	0x03,
};

static const unsigned char ACL_OFF[] = {
	0x55,
	0x02,
};

static const unsigned char DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00,
};

static const unsigned char SLEEP_IN[] = {
	0x10,
	0x00, 0x00,
};

static const unsigned char TE_ON[] = {
	0x35,
	0x00, 0x00,
};

static const unsigned char DISPLAY_ON[] = {
	0x29,
	0x00, 0x00,
};

static const unsigned char IGNORE_EOT[] = {
	0xE7,
	0xEF, 0x67, 0x03, 0xAF,
	0x47,
};

static const unsigned char AOR_97_5[] = {
	0xB2,
	0xE1, 0xB5,
};

static const unsigned char AOR_45_5[] = {
	0xB2,
	0xE0, 0xCC,
};

static const unsigned char AOR_40_0[] = {
	0xB2,
	0xE0, 0xB3,
};

static const unsigned char AOR_19_9[] = {
	0xB2,
	0xE0, 0x59,
};

static const unsigned char AOR_1_3[] = {
	0xB2,
	0xE0, 0x06,
};

#ifdef ENABLE_ACL
/* ELVSS Value for ALC ON */
/* ELVSS for 2cd */
static const unsigned char ELVSS_L1[] = {
	0xB6,
	0x88, 0x20,
};

/* ELVSS for 60cd */
static const unsigned char ELVSS_L2[] = {
	0xB6,
	0x88, 0x20,
};

/* ELVSS for 120cd */
static const unsigned char ELVSS_L3[] = {
	0xB6,
	0x88, 0x20,
};

/* ELVSS for 200cd */
static const unsigned char ELVSS_L4[] = {
	0xB6,
	0x88, 0x1D,
};

/* ELVSS for 300cd */
static const unsigned char ELVSS_L5[] = {
	0xB6,
	0x88, 0x1B,
};
#else
/* ELVSS Value for ALC OFF */
/* ELVSS for 2cd */
static const unsigned char ELVSS_L1[] = {
	0xB6,
	0x88, 0x20,
};

/* ELVSS for 60cd */
static const unsigned char ELVSS_L2[] = {
	0xB6,
	0x88, 0x20,
};

/* ELVSS for 120cd */
static const unsigned char ELVSS_L3[] = {
	0xB6,
	0x88, 0x20,
};

/* ELVSS for 200cd */
static const unsigned char ELVSS_L4[] = {
	0xB6,
	0x88, 0x1D,
};

/* ELVSS for 300cd */
static const unsigned char ELVSS_L5[] = {
	0xB6,
	0x88, 0x1B,
};
#endif

static const int  candela_tbl[MAX_GAMMA_CNT] = {
	CANDELA_2,
	CANDELA_60,
	CANDELA_120,
	CANDELA_200,
	CANDELA_300,
};

static const unsigned char *AOR_CMD[MAX_GAMMA_CNT] = {
	(unsigned char *)AOR_97_5,
	(unsigned char *)AOR_45_5,
	(unsigned char *)AOR_40_0,
	(unsigned char *)AOR_19_9,
	(unsigned char *)AOR_1_3,
};

static const unsigned char *ELVSS_CMD[MAX_GAMMA_CNT] = {
	(unsigned char *)ELVSS_L1,
	(unsigned char *)ELVSS_L2,
	(unsigned char *)ELVSS_L3,
	(unsigned char *)ELVSS_L4,
	(unsigned char *)ELVSS_L5,
};

static struct regulator_bulk_data supplies[] = {
	{ .supply = "vcc_lcd_3.0", },
	{ .supply = "vcc_lcd_1.8", },
};

static void panel_regulator_enable(struct s6e36w0x01 *lcd)
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

static void panel_regulator_disable(struct s6e36w0x01 *lcd)
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

static int panel_enter_hbm_mode(struct s6e36w0x01 *lcd)
{
	/* TODO : */
	return 0;
}

static int panel_exit_hbm_mode(struct s6e36w0x01 *lcd)
{
	/* TODO : */
	return 0;
}

static int panel_get_power(struct lcd_device *ld)
{
	struct s6e36w0x01 *lcd = lcd_get_data(ld);

	return lcd->power;
}

static int panel_set_power(struct lcd_device *ld, int power)
{
	struct s6e36w0x01 *lcd = lcd_get_data(ld);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	int ret = 0;

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
	    power != FB_BLANK_NORMAL) {
		dev_err(lcd->dev, "%s: power value should be 0, 1 or 4.\n",
			__func__);
		return -EINVAL;
	}

	if ((power == FB_BLANK_UNBLANK) && ops->set_blank_mode) {
		/* LCD power on */
		if ((POWER_IS_ON(power) && POWER_IS_OFF(lcd->power))
			|| (POWER_IS_ON(power) && POWER_IS_NRM(lcd->power))) {
			ret = ops->set_blank_mode(lcd_to_master(lcd), power);
			if (!ret && lcd->power != power)
				lcd->power = power;
		}
	} else if ((power == FB_BLANK_POWERDOWN) && ops->set_early_blank_mode) {
		/* LCD power off */
		if ((POWER_IS_OFF(power) && POWER_IS_ON(lcd->power)) ||
		(POWER_IS_ON(lcd->power) && POWER_IS_NRM(power))) {
			ret = ops->set_early_blank_mode(lcd_to_master(lcd),
							power);
			if (!ret && lcd->power != power)
				lcd->power = power;
		}
	}

	dev_info(lcd->dev, "%s: lcd power: mode[%d]\n", __func__, power);

	return ret;
}


static struct lcd_ops s6e36w0x01_lcd_ops = {
	.get_power = panel_get_power,
	.set_power = panel_set_power,
};

static int panel_update_gamma(struct  s6e36w0x01 *lcd, unsigned int brightness)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			lcd->gamma_tbl[brightness], GAMMA_CMD_CNT);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			AOR_CMD[brightness], AOR_ELVSS_CMD_CNT);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			ELVSS_CMD[brightness], AOR_ELVSS_CMD_CNT);

	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			UPDATE_GAMMA, ARRAY_SIZE(UPDATE_GAMMA));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	return 0;
}

static int panel_get_brightness_index(unsigned int brightness)
{

	switch (brightness) {
	case 0 ... 20:
		brightness = 0;
		break;
	case 21 ... 40:
		brightness = 1;
		break;
	case 41 ... 60:
		brightness = 2;
		break;
	case 61 ... 80:
		brightness = 3;
		break;
	default:
		brightness = 4;
		break;
	}

	return brightness;
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	unsigned int brightness = bd->props.brightness;
	struct s6e36w0x01 *lcd = bl_get_data(bd);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (brightness < MIN_BRIGHTNESS ||
		brightness > bd->props.max_brightness) {
		dev_err(lcd->dev, "lcd brightness should be %d to %d.\n",
			MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		return -EINVAL;
	}

	if (lcd->power > FB_BLANK_UNBLANK) {
		dev_warn(lcd->dev, "enable lcd panel.\n");
		return -EPERM;
	}

	if (lcd->alpm_on)
		return 0;

	dev_info(lcd->dev, "brightness[%d]dpms[%d]hbm[%d]\n",
		brightness, atomic_read(&master->dpms_on), lcd->hbm_on);

	brightness = panel_get_brightness_index(brightness);

	if (atomic_read(&master->dpms_on)) {
		if (lcd->hbm_on) {
#ifdef CONFIG_EXYNOS_SMIES_RUNTIME_ACTIVE
			ops->set_smies_active(master, lcd->hbm_on);
#else
			ops->set_smies_mode(master, OUTDOOR_MODE);
#endif
			ops->cmd_set_begin(master);
			ret = panel_enter_hbm_mode(lcd);
			ops->cmd_set_end(master);
		} else
			ret = panel_update_gamma(lcd, brightness);
	} else {
		if (ops->set_runtime_active(master)) {
			dev_warn(lcd->dev,
				"failed to set_runtime_active:power[%d]\n",
				lcd->power);

			if (lcd->power > FB_BLANK_UNBLANK)
				return -EPERM;
		}
		ops->cmd_set_begin(master);
		ret = panel_update_gamma(lcd, brightness);
		ops->cmd_set_end(master);
	}

	if (ret) {
		dev_err(lcd->dev, "failed gamma setting.\n");
		return -EIO;
	}

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops s6e36w0x01_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};

static void panel_power_on(struct mipi_dsim_lcd_device *dsim_dev, int power)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(1, lcd->dev, "S(power = %d)\n", power);

	/* lcd power on */
	if (power) {
		if (lcd->lp_mode)
			return;
		panel_regulator_enable(lcd);

		/* Do not reset at booting time if enabled. */
		if (lcd->boot_power_on) {
			lcd->boot_power_on = false;
			return;
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

	PANEL_DBG_MSG(1, lcd->dev, "E(power = %d)\n", power);
}


static void panel_get_gamma_tbl(struct s6e36w0x01 *lcd,
						const unsigned char *data)
{
	int i;

	panel_read_gamma(lcd->dimming, data);

	panel_generate_volt_tbl(lcd->dimming);

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i][0] = LDI_MTP_SET3;
		panel_get_gamma(lcd->dimming, i,
							&lcd->gamma_tbl[i][1]);
	}

	return;
}

static int panel_check_mtp(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned char mtp_data[MAX_MTP_CNT] = {0,};
	unsigned char hbm_data[12] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
					0x80, 0x80, 0x80, 0x00, 0x00, 0x00};

	ops->cmd_set_begin(master);
	/* ID */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));
	ops->cmd_read(master, MIPI_DSI_DCS_READ,
			LDI_ID_REG, LDI_ID_LEN, lcd->id);

	dev_info(lcd->dev, "Ver = 0x%x, 0x%x, 0x%x\n", lcd->id[0], lcd->id[1],
						lcd->id[2]);

	/* HBM */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_HBM_SET1, ARRAY_SIZE(PARAM_POS_HBM_SET1));
	ops->cmd_read(master, MIPI_DSI_DCS_READ,
			LDI_MTP_SET1, LDI_MTP_LEN1, lcd->hbm_gamma);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEFAULT, ARRAY_SIZE(PARAM_POS_DEFAULT));
	ops->cmd_read(master, MIPI_DSI_DCS_READ, LDI_MTP_SET2,
			LDI_MTP_LEN2, &lcd->hbm_gamma[LDI_MTP_LEN1]);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_HBM_SET2, ARRAY_SIZE(PARAM_POS_HBM_SET2));
	ops->cmd_read(master, MIPI_DSI_DCS_READ,
			LDI_MTP_SET3, LDI_MTP_LEN3,
			&lcd->hbm_gamma[LDI_MTP_LEN1 + LDI_MTP_LEN2]);

	lcd->elvss_hbm =
		lcd->hbm_gamma[LDI_MTP_LEN1 + LDI_MTP_LEN2 + LDI_MTP_LEN3 - 1];
	dev_info(lcd->dev, "HBM ELVSS: 0x%x\n", lcd->elvss_hbm);

	memcpy(&lcd->hbm_gamma[LDI_MTP_LEN1 + LDI_MTP_LEN2 + LDI_MTP_LEN3 - 1],
			hbm_data, sizeof(hbm_data));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEF_ELVSS, ARRAY_SIZE(PARAM_POS_DEF_ELVSS));
	ops->cmd_read(master, MIPI_DSI_DCS_READ,
			LDI_MTP_SET4, LDI_MTP_LEN4, &lcd->default_hbm);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			PARAM_POS_DEFAULT, ARRAY_SIZE(PARAM_POS_DEFAULT));
	dev_info(lcd->dev, "DEFAULT ELVSS: 0x%x\n", lcd->default_hbm);

	/* GAMMA */
	ops->cmd_read(master, MIPI_DSI_DCS_READ, LDI_MTP_SET0,
			MAX_MTP_CNT, mtp_data);

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_OFF_2, ARRAY_SIZE(TEST_KEY_OFF_2));

	ops->cmd_set_end(master);

	panel_get_gamma_tbl(lcd, mtp_data);

	return 0;
}

static int panel_set_refresh_rate(struct mipi_dsim_lcd_device *dsim_dev,
						int refresh)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S(enable = %d)\n", refresh);

	/* TODO: support various refresh rate */

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E(enable = %d)\n", refresh);

	return 0;
}

static int panel_set_win_update_region(struct mipi_dsim_lcd_device *dsim_dev,
						int offset_x, int offset_y,
						int width, int height)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned char buf[5];

	offset_x += 20;

	ops->cmd_set_begin(master);

	buf[0] = LDI_CASET_REG;
	buf[1] = (offset_x & 0xff00) >> 8;
	buf[2] = offset_x & 0x00ff;
	buf[3] = ((offset_x + width - 1) & 0xff00) >> 8;
	buf[4] = (offset_x + width - 1) & 0x00ff;

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf, ARRAY_SIZE(buf));

	buf[0] = LDI_PASET_REG;
	buf[1] = (offset_y & 0xff00) >> 8;
	buf[2] = offset_y & 0x00ff;
	buf[3] = ((offset_y + height - 1) & 0xff00) >> 8;
	buf[4] = (offset_y + height - 1) & 0x00ff;

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE, buf, ARRAY_SIZE(buf));

	ops->cmd_set_end(master);

	return 0;
}

static void panel_display_on(struct mipi_dsim_lcd_device *dsim_dev,
				unsigned int enable)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (lcd->lp_mode && enable) {
		lcd->lp_mode = false;
		return;
	}

	ops->cmd_set_begin(master);
	if (enable)
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				DISPLAY_ON, ARRAY_SIZE(DISPLAY_ON));
	else
		ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
				DISPLAY_OFF, ARRAY_SIZE(DISPLAY_OFF));

	ops->cmd_set_end(master);

	dev_info(lcd->dev, "%s[%d]\n", __func__, enable);
}

static void panel_sleep_in(struct s6e36w0x01 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	ops->cmd_set_begin(master);
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			SLEEP_IN, ARRAY_SIZE(SLEEP_IN));
	ops->cmd_set_end(master);
}

static void panel_alpm_on(struct s6e36w0x01 *lcd, unsigned int enable)
{
	/* TODO : */
}

static void panel_pm_check(struct mipi_dsim_lcd_device *dsim_dev,
						bool *pm_skip)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	if (lcd->lp_mode) {
		if (lcd->alpm_on) {
			*pm_skip = true;
			lcd->power = FB_BLANK_UNBLANK;
		} else
			*pm_skip = false;
	} else
		*pm_skip = false;

	return;
}

static void panel_te_active(struct mipi_dsim_lcd_device *dsim_dev,
						bool enable)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	if (enable)
		enable_irq(gpio_to_irq(lcd->te_gpio));
	else
		disable_irq(gpio_to_irq(lcd->te_gpio));

	return;
}

static void panel_display_init(struct s6e36w0x01 *lcd)
{
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	usleep_range(10000, 15000);

	ops->cmd_set_begin(master);
	/* Test key enable */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_1, ARRAY_SIZE(TEST_KEY_ON_1));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY_ON_2, ARRAY_SIZE(TEST_KEY_ON_2));

	/* sleep out */
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			SLEEP_OUT, ARRAY_SIZE(SLEEP_OUT));

	usleep_range(20000, 20000);
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			TEMP_OFFSET_GPARAM,
			ARRAY_SIZE(TEMP_OFFSET_GPARAM));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEMP_OFFSET, ARRAY_SIZE(TEMP_OFFSET));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			GAMMA_120, ARRAY_SIZE(GAMMA_120));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			AID_120, ARRAY_SIZE(AID_120));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			ELVSS_120, ARRAY_SIZE(ELVSS_120));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			UPDATE_GAMMA, ARRAY_SIZE(UPDATE_GAMMA));
	ops->cmd_write(master, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			ACL_OFF, ARRAY_SIZE(ACL_OFF));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TE_ON, ARRAY_SIZE(TE_ON));

	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY3_ON, ARRAY_SIZE(TEST_KEY3_ON));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			IGNORE_EOT, ARRAY_SIZE(IGNORE_EOT));
	ops->cmd_write(master, MIPI_DSI_DCS_LONG_WRITE,
			TEST_KEY3_OFF, ARRAY_SIZE(TEST_KEY3_OFF));
	ops->cmd_set_end(master);

	msleep(200);

}

static void panel_set_sequence(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(1, lcd->dev, "S\n");

	if (lcd->lp_mode) {
		panel_alpm_on(lcd, lcd->alpm_on);
		lcd->power = FB_BLANK_UNBLANK;
		return;
	}

	panel_display_init(lcd);

	lcd->power = FB_BLANK_UNBLANK;
	panel_set_brightness(lcd->bd);

	PANEL_DBG_MSG(1, lcd->dev, "E(refresh = %d)\n", lcd->refresh);
}

static void panel_frame_freq_set(struct s6e36w0x01 *lcd)
{
	/* TODO : */
}

irqreturn_t panel_te_interrupt(int irq, void *dev_id)
{
	struct s6e36w0x01 *lcd = dev_id;
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	struct mipi_dsim_master_ops *ops = master->master_ops;

	if (ops->te_handler)
		ops->te_handler(master);

	return IRQ_HANDLED;
}

static ssize_t panel_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);
	char temp[15];

	sprintf(temp, "SDC_%02x%02x%02x\n",
			lcd->id[0], lcd->id[1], lcd->id[2]);

	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t panel_alpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->alpm_on ? "on" : "off");
}

static ssize_t panel_alpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);

	if (!strncmp(buf, "on", 2))
		lcd->alpm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->alpm_on = false;
	else
		dev_warn(dev, "invalid command.\n");

	return size;
}

static void panel_acl_update(struct s6e36w0x01 *lcd, unsigned int value)
{
	/* TODO : */
}

static ssize_t panel_acl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->acl);
}

static ssize_t panel_acl_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);
	unsigned long value;
	int rc;

	if (lcd->power > FB_BLANK_UNBLANK) {
		dev_warn(lcd->dev, "acl control before lcd enable.\n");
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (value < MIN_ACL || value > MAX_ACL) {
		dev_warn(dev, "invalid acl value[%ld]\n", value);
		return size;
	}

	if (ops->set_runtime_active(master))
		dev_warn(lcd->dev, "failed to set_runtime_active\n");

	ops->cmd_set_begin(master);
	panel_acl_update(lcd, value);
	ops->cmd_set_end(master);

	lcd->acl = value;

	dev_info(lcd->dev, "acl control[%d]\n", lcd->acl);

	return size;
}

static ssize_t panel_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

static ssize_t panel_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (!strncmp(buf, "on", 2))
		lcd->hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		lcd->hbm_on = 0;
	else {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (lcd->power > FB_BLANK_UNBLANK) {
		dev_warn(lcd->dev, "hbm control before lcd enable.\n");
		return -EPERM;
	}

	if (ops->set_runtime_active(master))
		dev_warn(lcd->dev, "failed to set_runtime_active\n");

	ops->cmd_set_begin(master);
	if (lcd->hbm_on) {
		dev_info(lcd->dev, "HBM ON.\n");
		panel_enter_hbm_mode(lcd);
	} else {
		dev_info(lcd->dev, "HBM OFF.\n");
		panel_exit_hbm_mode(lcd);
	}
	ops->cmd_set_end(master);

	return size;
}

static ssize_t panel_refresh_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", lcd->refresh);
}

static ssize_t panel_refresh_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(dev);
	struct mipi_dsim_master_ops *ops = lcd_to_master_ops(lcd);
	struct mipi_dsim_device *master = lcd_to_master(lcd);

	if (strncmp(buf, "30", 2) && strncmp(buf, "60", 2)) {
		dev_warn(dev, "invalid comman (use on or off)d.\n");
		return size;
	}

	if (ops->set_runtime_active(master))
		dev_warn(lcd->dev, "failed to set_runtime_active\n");

	ops->cmd_set_begin(master);
	if (!strncmp(buf, "30", 2)) {
		lcd->refresh = 30;
		panel_frame_freq_set(lcd);
	} else {
		lcd->refresh = 60;
		panel_frame_freq_set(lcd);
	}
	ops->cmd_set_end(master);

	return size;
}
DEVICE_ATTR(lcd_type, 0444, panel_lcd_type_show, NULL);
DEVICE_ATTR(alpm, 0644, panel_alpm_show, panel_alpm_store);
DEVICE_ATTR(acl, 0644, panel_acl_show, panel_acl_store);
DEVICE_ATTR(hbm, 0644, panel_hbm_show, panel_hbm_store);
DEVICE_ATTR(refresh, 0644, panel_refresh_show, panel_refresh_store);

#ifdef CONFIG_LCD_ESD
static void panel_esd_detect_work(struct work_struct *work)
{
	struct s6e36w0x01 *lcd = container_of(work,
						struct s6e36w0x01, det_work);
	char *event_string = "LCD_ESD=ON";
	char *envp[] = { event_string, NULL };

	if (!POWER_IS_OFF(lcd->power)) {
		kobject_uevent_env(&esd_dev->kobj,
			KOBJ_CHANGE, envp);
		dev_info(lcd->dev,
			"Send uevent. ESD DETECTED\n");
	}
}

irqreturn_t panel_det_interrupt(int irq, void *dev_id)
{
	struct s6e36w0x01 *lcd = dev_id;

	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0x00));

	if (!work_busy(&lcd->det_work)) {
		schedule_work(&lcd->det_work);
		dev_dbg(lcd->dev, "add esd schedule_work.\n");
	}

	return IRQ_HANDLED;
}
#endif

static int panel_probe(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w0x01 *lcd;
	struct mipi_dsim_platform_data *dsim_pd;
	struct fb_videomode *timing;
	int ret, i;

	lcd = devm_kzalloc(&dsim_dev->dev, sizeof(struct s6e36w0x01),
				GFP_KERNEL);
	if (!lcd) {
		dev_err(&dsim_dev->dev,
				"failed to allocate s6e36w0x01 structure.\n");
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

	mutex_init(&lcd->lock);

	ret = devm_regulator_bulk_get(lcd->dev, ARRAY_SIZE(supplies), supplies);
	if (ret) {
		dev_err(lcd->dev, "Failed to get regulator: %d\n", ret);
		return ret;
	}

	lcd->bd = backlight_device_register("s6e36w0x01-bl", lcd->dev, lcd,
			&s6e36w0x01_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		dev_err(lcd->dev, "failed to register backlight ops.\n");
		ret = PTR_ERR(lcd->bd);
		goto err_free_gamma_tbl;
	}

	lcd->ld = lcd_device_register("s6e36w0x01", lcd->dev, lcd,
					&s6e36w0x01_lcd_ops);
	if (IS_ERR(lcd->ld)) {
		dev_err(lcd->dev, "failed to register lcd ops.\n");
		ret = PTR_ERR(lcd->ld);
		goto err_unregister_bd;
	}

	if (lcd->pd)
		lcd->property = lcd->pd->pdata;

	lcd->te_gpio = lcd->property->te_gpio;
	ret = devm_request_irq(lcd->dev, gpio_to_irq(lcd->te_gpio),
				panel_te_interrupt,
				IRQF_DISABLED | IRQF_TRIGGER_RISING, "TE", lcd);
	if (ret < 0) {
		dev_err(lcd->dev, "failed to request te irq.\n");
		goto err_unregister_lcd;
	}

#ifdef CONFIG_LCD_ESD
	lcd->det_gpio = lcd->property->det_gpio;
	lcd->esd_irq = gpio_to_irq(lcd->det_gpio);

	ret = devm_request_irq(lcd->dev, lcd->esd_irq,
				panel_det_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "esd",
				lcd);
	if (ret < 0) {
		dev_err(lcd->dev, "failed to request det irq.\n");
		goto err_unregister_lcd;
	}

	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(lcd->det_gpio, S3C_GPIO_PULL_DOWN);
	INIT_WORK(&lcd->det_work, panel_esd_detect_work);

	esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(esd_class)) {
		dev_err(lcd->dev, "Failed to create class(lcd_event)!\n");
		ret = PTR_ERR(esd_class);
		goto err_unregister_lcd;
	}

	esd_dev = device_create(esd_class, NULL, 0, NULL, "esd");
#endif

	ret = device_create_file(&lcd->ld->dev, &dev_attr_lcd_type);
	if (ret < 0) {
		dev_err(&lcd->ld->dev, " failed to create sysfs.\n");
		goto err_unregister_lcd;
	}

	ret = device_create_file(lcd->dev, &dev_attr_alpm);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_lcd_type_file;
	}

	ret = device_create_file(lcd->dev, &dev_attr_acl);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_alpm_file;
	}

	ret = device_create_file(lcd->dev, &dev_attr_hbm);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_acl_file;
	}

	ret = device_create_file(lcd->dev, &dev_attr_refresh);
	if (ret < 0) {
		dev_err(lcd->dev, " failed to create sysfs.\n");
		goto err_remove_hbm_file;
	}

	lcd->power = FB_BLANK_UNBLANK;
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;
	dsim_pd = (struct mipi_dsim_platform_data *)lcd_to_master(lcd)->pd;
	timing = (struct fb_videomode *)dsim_pd->lcd_panel_info;
#if 0
	lcd->refresh = timing->refresh;
#else
	lcd->refresh = REFRESH_60HZ;
#endif

	dev_set_drvdata(&dsim_dev->dev, lcd);

	dev_info(lcd->dev, "probed s6e36w0x01 panel driver.\n");

	return 0;
err_remove_hbm_file:
	device_remove_file(lcd->dev, &dev_attr_hbm);
err_remove_acl_file:
	device_remove_file(lcd->dev, &dev_attr_acl);
err_remove_alpm_file:
	device_remove_file(lcd->dev, &dev_attr_alpm);
err_remove_lcd_type_file:
	device_remove_file(&lcd->ld->dev, &dev_attr_lcd_type);
err_unregister_lcd:
	lcd_device_unregister(lcd->ld);
err_unregister_bd:
	backlight_device_unregister(lcd->bd);
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
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

	if (lcd->alpm_on) {
		if (lcd->lp_mode)
			lcd->power = FB_BLANK_POWERDOWN;
		else {
			panel_alpm_on(lcd, lcd->alpm_on);
			lcd->power = FB_BLANK_POWERDOWN;
			lcd->lp_mode = true;
		}

		return 0;
	}
#ifdef CONFIG_LCD_ESD
	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0x00));
#endif

	panel_display_on(dsim_dev, 0);
	panel_sleep_in(lcd);

	usleep_range(lcd->pd->power_off_delay * 1000,
			lcd->pd->power_off_delay * 1000);

	panel_power_on(dsim_dev, 0);

	lcd->power = FB_BLANK_POWERDOWN;

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return 0;
}

static int panel_resume(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct s6e36w0x01 *lcd = dev_get_drvdata(&dsim_dev->dev);

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "S\n");

#ifdef CONFIG_LCD_ESD
	s3c_gpio_cfgpin(lcd->det_gpio, S3C_GPIO_SFN(0xf));
#endif

	PANEL_DBG_MSG(dbg_mode, lcd->dev, "E\n");

	return 0;
}
#else
#define panel_suspend	NULL
#define panel_resume	NULL
#endif

static struct mipi_dsim_lcd_driver panel_dsim_ddi_driver = {
	.name = "s6e36w0x01",
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
		/* TODO - unregister lcd device. */
		return ret;
	}

	return 0;
}

static void panel_exit(void)
{
}

module_init(panel_init);
module_exit(panel_exit);

MODULE_AUTHOR("Joong-Mock Shin <jmock.shin@samsung.com>");
MODULE_DESCRIPTION("MIPI-DSI based S6E36W0X01 AMOLED LCD Panel Driver");
MODULE_LICENSE("GPL");
