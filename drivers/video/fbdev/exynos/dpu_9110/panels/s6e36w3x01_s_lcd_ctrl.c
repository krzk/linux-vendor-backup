/* s6e36w3x01_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * SeungBeom, Park <sb1.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s6e36w3x01_s_dimming.h"
#include "s6e36w3x01_s_param.h"
#include "s6e36w3x01_s_lcd_ctrl.h"
#include "s6e36w3x01_s_mipi_lcd.h"

#include "../dsim.h"
#include <video/mipi_display.h>

static const char *mdnie_sc_str[] = {
	"SCENARIO_UI",
	"SCENARIO_GALLERY",
	"SCENARIO_VIDEO",
	"SCENARIO_VTCALL",
	"SCENARIO_CAMERA",
	"SCENARIO_BROWSER",
	"SCENARIO_NEGATIVE",
	"SCENARIO_EMAIL",
	"SCENARIO_EBOOK",
	"SCENARIO_GRAY",
	"SCENARIO_CURTAIN",
	"SCENARIO_GRAY_NEGATIVE",
	"SCENARIO_MAX",
};

static unsigned char GAMMA_SET[] = {
	0xCA,
	0x07, 0x00, 0x00, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
};

extern unsigned char s6e36w3x01_gamma_set[MAX_BR_INFO][GAMMA_CMD_CNT];
static unsigned char ELVSS[57] = {
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
	0x18, 0x18, 0x18, 0x18, 0x17, 0x17, 0x17, 0x17,
	0x16, 0x16, 0x16, 0x15, 0x15, 0x15, 0x15, 0x15,
	0x15, 0x15, 0x14, 0x14, 0x14, 0x14, 0x14, 0x13,
	0x13
};

static unsigned char AID[114] = {
  0x01, 0x5A, 0x01, 0x56, 0x01, 0x52, 0x01, 0x4E,
  0x01, 0x4B, 0x01, 0x49, 0x01, 0x45, 0x01, 0x42,
  0x01, 0x3B, 0x01, 0x39, 0x01, 0x34, 0x01, 0x31,
  0x01, 0x2B, 0x01, 0x28, 0x01, 0x22, 0x01, 0x1B,
  0x01, 0x17, 0x01, 0x11, 0x01, 0x0A, 0x01, 0x02,
  0x00, 0xFA, 0x00, 0xF3, 0x00, 0xEA, 0x00, 0xE1,
  0x00, 0xD6, 0x00, 0xCC, 0x00, 0xC3, 0x00, 0xAF,
  0x00, 0xA5, 0x00, 0x9A, 0x00, 0x8F, 0x00, 0x7F,
  0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F,
  0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F,
  0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F, 0x00, 0x7F,
  0x00, 0x76, 0x00, 0x66, 0x00, 0x5D, 0x00, 0x43,
  0x00, 0x2C, 0x00, 0x18, 0x00, 0x10, 0x00, 0x10,
  0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10,
  0x00, 0x10,
};

static unsigned char ELVSS_SETTING[] = {
	0xB5,
	0x19, 0xDC, 0x13,
};

static unsigned char AID_SETTING[3] = {
	0xB1,
	0x00, 0x10,
};

static const unsigned char HLPM_ON_ETC[] = {
	0xFD,
	0x88, 0x05, 0x3E, 0x80,
	0x0A, 0x00, 0xEC, 0x25,
	0x00, 0x00, 0x0A, 0xAA,
	0x44, 0xD0
};

static const unsigned char HLPM_OFF_ETC[] = {
	0xFD,
	0x88, 0x05, 0x3E, 0x80,
	0x0A, 0x00, 0x6C, 0x25,
	0x00, 0x00, 0x0A, 0xAA,
	0x44, 0x10
};

void s6e36w3x01_testkey_enable(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	mutex_lock(&lcd->mipi_lock);

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");
}

void s6e36w3x01_testkey_disable(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("%s: failed to send TEST_KEY_OFF_0.\n", __func__);

	mutex_unlock(&lcd->mipi_lock);
}

void s6e36w3x01_testkey_enable_lv3(u32 id)
{
	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_FC,
		ARRAY_SIZE(TEST_KEY_ON_FC)) < 0)
		dsim_err("failed to send TEST_KEY_ON_FC.\n");
}

void s6e36w3x01_testkey_disable_lv3(u32 id)
{
	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_FC,
		ARRAY_SIZE(TEST_KEY_OFF_FC)) < 0)
		dsim_err("%s: failed to send TEST_KEY_OFF_FC.\n", __func__);
}

void s6e36w3x01_mdnie_outdoor_set(int id, enum mdnie_outdoor on)
{
	s6e36w3x01_testkey_enable(id);

	if (on) {
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) OUTDOOR_TUNE,
			ARRAY_SIZE(OUTDOOR_TUNE)) < 0)
			dsim_err("failed to send OUTDOOR_TUNE.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_OUTD_ON,
			ARRAY_SIZE(MDNIE_OUTD_ON)) < 0)
			dsim_err("failed to send MDNIE_OUTD_ON.\n");
	} else
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_CTL_OFF,
			ARRAY_SIZE(MDNIE_CTL_OFF)) < 0)
			dsim_err("failed to send MDNIE_CTL_OFF.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) < 0)
		dsim_err("failed to send PANEL_UPDATE.\n");

	s6e36w3x01_testkey_disable(id);

	return;
}


void s6e36w3x01_mdnie_set(u32 id, enum mdnie_scenario scenario)
{
	if (scenario >= SCENARIO_MAX) {
		dsim_err("%s: Invalid scenario (%d)\n", __func__, scenario);
		return;
	}

	s6e36w3x01_testkey_enable(id);

	switch (scenario) {
	case SCENARIO_GRAY:
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) GRAY_TUNE,
			ARRAY_SIZE(GRAY_TUNE)) < 0)
			dsim_err("failed to send GRAY_TUNE.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_GRAY_ON,
			ARRAY_SIZE(MDNIE_GRAY_ON)) < 0)
			dsim_err("failed to send MDNIE_GRAY_ON.\n");
		break;
	case SCENARIO_NEGATIVE:
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) NEGATIVE_TUNE,
			ARRAY_SIZE(NEGATIVE_TUNE)) < 0)
			dsim_err("failed to send NEGATIVE_TUNE.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_NEGATIVE_ON,
			ARRAY_SIZE(MDNIE_NEGATIVE_ON)) < 0)
			dsim_err("failed to send MDNIE_NEGATIVE_ON.\n");
		break;
	case SCENARIO_GRAY_NEGATIVE:
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) GRAY_NEGATIVE_TUNE,
			ARRAY_SIZE(GRAY_NEGATIVE_TUNE)) < 0)
			dsim_err("failed to send GRAY_NEGATIVE_TUNE.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_GRAY_NEGATIVE_ON,
			ARRAY_SIZE(MDNIE_GRAY_NEGATIVE_ON)) < 0)
			dsim_err("failed to send MDNIE_GRAY_NEGATIVE_ON.\n");
		break;
	default:
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MDNIE_CTL_OFF,
			ARRAY_SIZE(MDNIE_CTL_OFF)) < 0)
			dsim_err("failed to send MDNIE_CTL_OFF.\n");
		break;
	}

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) < 0)
		dsim_err("failed to send PANEL_UPDATE.\n");

	s6e36w3x01_testkey_disable(id);

	pr_info("%s:[%s]\n", __func__, mdnie_sc_str[scenario]);

	return;
}


int s6e36w3x01_read_mtp_reg(int id, u32 addr, char* buffer, u32 size)
{
	int ret = 0;

	s6e36w3x01_testkey_enable(id);

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		addr, size, buffer) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n", __func__, addr);
		ret = -EIO;
	}

	s6e36w3x01_testkey_disable(id);

	return ret;
}

void s6e36w3x01_init_ctrl(int id, struct decon_lcd * lcd)
{
	/* Test Key Enable */
	s6e36w3x01_testkey_enable(id);

	/* sleep out */
	if(dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
		SLPOUT[0], 0) < 0)
		dsim_err("failed to send SLEEP_OUT.\n");

	/* 20ms delay */
	msleep(20);

	/* Common Setting */
	/* TE(Vsync) ON/OFF */
	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long)TEON,
		ARRAY_SIZE(TEON)) < 0)
		dsim_err("failed to send TE_ON.\n");

	/* Brightness Setting */
	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) GAMMA_SET,
		ARRAY_SIZE(GAMMA_SET)) < 0)
		dsim_err("failed to send GAMMA_SET.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) AID_SETTING,
		ARRAY_SIZE(AID_SETTING)) < 0)
		dsim_err("failed to send AID_SETTING.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ELVSS_SET,
		ARRAY_SIZE(ELVSS_SET)) < 0)
		dsim_err("failed to send ELVSS_SET.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		GAMMA_UPDATE[0], GAMMA_UPDATE[1]) < 0)
		dsim_err("failed to send GAMMA_UPDATE.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) FFC_ON,
		ARRAY_SIZE(FFC_ON)) < 0)
		dsim_err("failed to send FFC_ON.\n");


	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) CIRCLE_MASK,
		ARRAY_SIZE(CIRCLE_MASK)) < 0)
		dsim_err("failed to send CIRCLE_MASK.\n");

	/* Test key disable */
	s6e36w3x01_testkey_disable(id);
}

void s6e36w3x01_enable(int id)
{
	/* display on */
	if(dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
		DISPLAY_ON[0],	0) < 0)
		dsim_err("failed to send DISPLAY_ON.\n");
}

/* follow Panel Power off sequence */
void s6e36w3x01_disable(int id)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
		DISPLAY_OFF[0],0) < 0)
		dsim_err("fail to write DISPLAY_OFF .\n");

	/* 10ms delay */
	msleep(10);

	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
		SLEEP_IN[0], 0) < 0)
		dsim_err("fail to write SLEEP_IN .\n");

	/* 150ms delay */
	msleep(150);
}

int s6e36w3x01_gamma_ctrl(struct s6e36w3x01 *lcd, u32 backlightlevel)
{
	struct dsim_device *dsim = lcd->dsim;
	int i, count;

	if (!lcd) {
		pr_err("%s: LCD is NULL\n", __func__);
		return 0;
	}

	for (i=0; i < GAMMA_CMD_CNT; i++) {
		GAMMA_SET[i] = s6e36w3x01_gamma_set[backlightlevel][i];
	}

	for (count=0; count < 2; count++) {
		AID_SETTING[count+1] = AID[(backlightlevel*2)+count];
	}

	ELVSS_SETTING[3] = ELVSS[backlightlevel];

	if (backlightlevel < 21)
		ELVSS_SETTING[2] = 0xCC;
	else
		ELVSS_SETTING[2] = 0xDC;

	s6e36w3x01_testkey_enable(dsim->id);
	s6e36w3x01_testkey_enable_lv3(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) HLPM_OFF_ETC,
		ARRAY_SIZE(HLPM_OFF_ETC)) < 0)
		dsim_err("failed to send HLPM_OFF_ETC.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) GAMMA_SET,
		ARRAY_SIZE(GAMMA_SET)) < 0)
		dsim_err("failed to send GAMMA_SET.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) AID_SETTING,
		ARRAY_SIZE(AID_SETTING)) < 0)
		dsim_err("failed to send AID_SETTING.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ELVSS_SETTING,
		ARRAY_SIZE(ELVSS_SETTING)) < 0)
		dsim_err("failed to send ELVSS_SETTING.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) PANEL_UPDATE,
		ARRAY_SIZE(PANEL_UPDATE)) < 0)
		dsim_err("failed to send PANEL_UPDATE.\n");

	s6e36w3x01_testkey_disable_lv3(dsim->id);
	s6e36w3x01_testkey_disable(dsim->id);

	return 0;
}

void s6e36w3x01_hlpm_ctrl(struct s6e36w3x01 *lcd, bool enable)
{
	struct dsim_device *dsim = lcd->dsim;
	struct backlight_device *bd = lcd->bd;
	int brightness = bd->props.brightness;

	if (enable) {
		s6e36w3x01_testkey_enable(dsim->id);
		s6e36w3x01_testkey_enable_lv3(dsim->id);

		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HLPM_ON_ETC,
			ARRAY_SIZE(HLPM_ON_ETC)) < 0)
			dsim_err("failed to send HLPM_ON_ETC.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			HLPM_20NIT_ON[0], HLPM_20NIT_ON[1]) < 0)
			dsim_err("failed to send HLPM_20NIT_ON.\n");

		lcd->hlpm_nit = HLPM_NIT_LOW;
		s6e36w3x01_testkey_disable_lv3(dsim->id);
		s6e36w3x01_testkey_disable(dsim->id);

		pr_info("%s:on:hnit[%d]br[%d]\n",
			__func__, lcd->hlpm_nit, brightness);
	} else {
		s6e36w3x01_testkey_enable(dsim->id);
		s6e36w3x01_testkey_enable_lv3(dsim->id);
		lcd->hlpm_nit = HLPM_NIT_OFF;

		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HLPM_OFF_ETC,
			ARRAY_SIZE(HLPM_OFF_ETC)) < 0)
			dsim_err("failed to send HLPM_OFF_ETC.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			NORMAL_ON[0], NORMAL_ON[1]) < 0)
			dsim_err("failed to send NORMAL_ON.\n");

		s6e36w3x01_testkey_disable_lv3(dsim->id);
		s6e36w3x01_testkey_disable(dsim->id);

		s6e36w3x01_gamma_ctrl(lcd, lcd->br_map[brightness]);
		pr_info("%s:off:br[%d]\n", __func__, brightness);
	}

	return;
}

int s6e36w3x01_hbm_on(int id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *lcd = dsim->ld;
	struct s6e36w3x01 *panel = dev_get_drvdata(&lcd->dev);
	ktime_t hbm_time;

	if (!panel) {
		pr_info("%s: LCD is NULL\n", __func__);
		return 0;
	}
	ELVSS_SETTING[3] = 0x13;
	ELVSS_SETTING[2] = 0xDC;

	s6e36w3x01_testkey_enable(id);

	if (panel->hbm_on) {
		printk("[LCD] %s : HBM ON\n", __func__);
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ELVSS_SETTING,
			ARRAY_SIZE(ELVSS_SETTING)) < 0)
			dsim_err("failed to send ELVSS_SETTING.\n");

		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_ACL_ON,
			ARRAY_SIZE(HBM_ACL_ON)) < 0)
			dsim_err("failed to send HBM_ACL_OFF.\n");

		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ACL_ON,
			ARRAY_SIZE(ACL_ON)) < 0)
			dsim_err("failed to send ACL_OFF.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_ON,
			ARRAY_SIZE(HBM_ON)) < 0)
			dsim_err("failed to send HBM_ON.\n");
		panel->hbm_stime = ktime_get_boottime();
	} else {
		printk("[LCD] %s : HBM OFF\n", __func__);
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_OFF,
			ARRAY_SIZE(HBM_OFF)) < 0)
			dsim_err("failed to send HBM_OFF.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_ACL_OFF,
			ARRAY_SIZE(HBM_ACL_OFF)) < 0)
			dsim_err("failed to send HBM_ACL_OFF.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ACL_OFF,
			ARRAY_SIZE(ACL_OFF)) < 0)
			dsim_err("failed to send HBM_ON.\n");
		hbm_time = ktime_get_boottime();
		panel->hbm_total_time += ((unsigned int)ktime_ms_delta(hbm_time, panel->hbm_stime) / 1000);
	}
	s6e36w3x01_testkey_disable(id);

	return 0;
}

int s6e36w3x01_temp_offset_comp(int id, unsigned int stage)
{
	s6e36w3x01_testkey_enable(id);

	switch (stage) {
	case TEMP_RANGE_0:
		ELVSS_SETTING[1] = 0x19;
		break;
	case TEMP_RANGE_1:
		ELVSS_SETTING[1] = 0x80;
		break;
	case TEMP_RANGE_2:
		ELVSS_SETTING[1] = 0x8A;
		break;
	case TEMP_RANGE_3:
		ELVSS_SETTING[1] = 0x94;
		break;
	}
	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) ELVSS_SETTING,
		ARRAY_SIZE(ELVSS_SETTING)) < 0)
		dsim_err("failed to send ELVSS_SETTING.\n");

	s6e36w3x01_testkey_disable(id);

	return 0;
}

void s6e36w3x01_mcd_test_on(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	if (!lcd) {
		pr_info("%s: LCD is NULL\n", __func__);
		return;
	}

	s6e36w3x01_testkey_enable(dsim->id);

	if (lcd->mcd_on) {
		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_TEST_ON,
			ARRAY_SIZE(MCD_TEST_ON)) < 0)
			dsim_err("failed to send MCD_TEST_ON.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_ON_ECT_01,
			ARRAY_SIZE(MCD_ON_ECT_01)) < 0)
			dsim_err("failed to send MCD_ON_ECT_01.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			MCD_ON_ECT_02[0], MCD_ON_ECT_02[1]) < 0)
			dsim_err("failed to send MCD_ON_ECT_02.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			MCD_ON_LTPS[0], MCD_ON_LTPS[1]) < 0)
			dsim_err("failed to send MCD_ON_LTPS.\n");
	} else {
		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_TEST_OFF,
			ARRAY_SIZE(MCD_TEST_OFF)) < 0)
			dsim_err("failed to send MCD_TEST_OFF.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) MCD_OFF_ECT_01,
			ARRAY_SIZE(MCD_OFF_ECT_01)) < 0)
			dsim_err("failed to send MCD_OFF_ECT_01.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			MCD_OFF_ECT_02[0], MCD_OFF_ECT_02[1]) < 0)
			dsim_err("failed to send MCD_OFF_ECT_02.\n");

		if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			MCD_OFF_LTPS[0], MCD_OFF_LTPS[1]) < 0)
			dsim_err("failed to send MCD_OFF_LTPS.\n");
	}

	s6e36w3x01_testkey_disable(dsim->id);

	msleep(100);
}

#define	MDNIE_PARAM_SIZE	4
void s6e36w3x01_print_debug_reg(int id)
{
	const char te_state_reg = 0x0a;
	const char panel_state_reg = 0x0e;
	const char mdnie_ctrl_reg = 0xe6;
	const char mdnie_tune_reg = 0xe7;
	char read_buf[MDNIE_PARAM_SIZE] = {0, };
	char reg_1, reg_2;

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		te_state_reg, 1, &reg_1) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n",
			__func__, te_state_reg);
	}

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		panel_state_reg, 1, &reg_2) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n",
			__func__, panel_state_reg);
	}

	pr_info("%s: 0x%02x[0x%02x], 0x%02x[0x%02x]\n",
		__func__, te_state_reg, reg_1, panel_state_reg, reg_2);

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		mdnie_ctrl_reg, MDNIE_PARAM_SIZE, &read_buf[0]) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n",
			__func__, panel_state_reg);
	}

	pr_info("%s: 0x%02x[0x%02x, 0x%02x, 0x%02x, 0x%02x]\n",
		__func__, mdnie_ctrl_reg, read_buf[0], read_buf[1],
		read_buf[2], read_buf[3]);

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		mdnie_tune_reg, MDNIE_PARAM_SIZE, &read_buf[0]) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n",
			__func__, panel_state_reg);
	}

	pr_info("%s: 0x%02x[0x%02x, 0x%02x, 0x%02x, 0x%02x]\n",
		__func__, mdnie_tune_reg, read_buf[0], read_buf[1],
		read_buf[2], read_buf[3]);

	return;
}

void s6e36w3x01_set_resolution(int id, int resolution)
{
	/* Test Key Enable */
	s6e36w3x01_testkey_enable(id);

	switch(resolution) {
	case 0:
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) CASET_360,
			ARRAY_SIZE(CASET_360)) < 0)
			dsim_err("failed to send CASET_360.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) PASET_360,
			ARRAY_SIZE(PASET_360)) < 0)
			dsim_err("failed to send PASET_360.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) OMOK_OFF,
			ARRAY_SIZE(OMOK_OFF)) < 0)
			dsim_err("failed to send OMOK_OFF.\n");
		break;
	case 1:
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) CASET_240,
			ARRAY_SIZE(CASET_240)) < 0)
			dsim_err("failed to send CASET_240.\n");
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) PASET_240,
				ARRAY_SIZE(PASET_240)) < 0)
				dsim_err("failed to send PASET_240.\n");
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) OMOK_240,
				ARRAY_SIZE(OMOK_240)) < 0)
				dsim_err("failed to send OMOK_240.\n");
		break;
	case 2:
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) CASET_180,
			ARRAY_SIZE(CASET_180)) < 0)
			dsim_err("failed to send CASET_180.\n");
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) PASET_180,
				ARRAY_SIZE(PASET_180)) < 0)
				dsim_err("failed to send PASET_180.\n");
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) OMOK_180,
				ARRAY_SIZE(OMOK_180)) < 0)
				dsim_err("failed to send OMOK_180.\n");
		break;

	default:
		pr_err("%s: unknown resolution\n", __func__);
	};
	/* Test key disable */
	s6e36w3x01_testkey_disable(id);

	pr_info("%s:[%d]\n", __func__, resolution);
}

void s6e36w3x01_write_ddi_debug(struct s6e36w3x01 *lcd, int enable)
{
	struct dsim_device *dsim = lcd->dsim;
	const char b0_first_cmd[] = {0xb0};
	const char b0_second_cmd[] = {0xb0, 0x0c, 0xf2};
	const char enable_cmd[] = {0xf2, 0xc0};
	const char disable_cmd[] = {0xf2, 0xc0};

	s6e36w3x01_testkey_enable(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
			b0_first_cmd[0], 0) < 0)
		dsim_err("failed to send b0_first_cmd.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) b0_second_cmd,
		ARRAY_SIZE(b0_second_cmd)) < 0)
		dsim_err("failed to send b0_second_cmd.\n");

	if (enable) {
		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			enable_cmd[0], enable_cmd[1]) < 0)
			dsim_err("failed to send enable_cmd.\n");
	} else {
		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
			disable_cmd[0], disable_cmd[1]) < 0)
			dsim_err("failed to send disable_cmd.\n");
	}

	s6e36w3x01_testkey_disable(dsim->id);
}
