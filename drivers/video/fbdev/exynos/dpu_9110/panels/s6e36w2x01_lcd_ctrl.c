/* s6e36w2x01_lcd_ctrl.c
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

#include "s6e36w2x01_dimming.h"
#include "s6e36w2x01_param.h"
#include "s6e36w2x01_lcd_ctrl.h"
#include "s6e36w2x01_mipi_lcd.h"

#include "../dsim.h"
#include <video/mipi_display.h>

static const char* sc_cmd_str[] = {
	"SC_ON_SEL",
	"SB_ON_SEL",
	"SC_ANA_CLOCK_EN",
	"SB_BLK_EN",
	"SC_HBP",
	"SC_VBP",
	"SC_BG_COLOR",
	"SC_AA_WIDTH",
	"SC_UPDATE_RATE",
	"SC_TIME_UPDATE",
	"SC_COMP_EN",
	"SC_INC_STEP",
	"SC_LINE_COLOR",
	"SC_RADIUS",
	"SC_CLS_CIR_ON",
	"SC_OPN_CIR_ON",
	"SC_RGB_INV",
	"SC_LINE_WIDTH",
	"SC_SET_HH",
	"SC_SET_MM",
	"SC_SET_SS",
	"SC_SET_MSS",
	"SC_CENTER_X",
	"SC_CENTER_Y",
	"SC_HH_CENTER_X",
	"SC_HH_CENTER_Y",
	"SC_HH_RGB_MASK",
	"SC_MM_CENTER_X",
	"SC_MM_CENTER_Y",
	"SC_MM_RGB_MASK",
	"SC_SS_CENTER_X",
	"SC_SS_CENTER_Y",
	"SC_SS_RGB_MASK",
	"SB_DE",
	"SB_RATE",
	"SB_RADIUS",
	"SB_LINE_COLOR_R",
	"SB_LINE_COLOR_G",
	"SB_LINE_COLOR_B",
	"SB_LINE_WIDTH",
	"SB_AA_ON",
	"SB_AA_WIDTH",
	"SB_CIRCLE_1_X",
	"SB_CIRCLE_1_Y",
	"SB_CIRCLE_2_X",
	"SB_CIRCLE_2_Y",
};

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

static unsigned char SC_CFG[] = {
	0xE9,
	0x00, 0x00, 0x00, 0x08,
	0x05, 0x00, 0x00, 0x00,
	0x03, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0a,
	0x0a, 0x1E, 0x00, 0x00,
	0xB3, 0x00, 0xB3, 0x3A,
	0x14, 0x00, 0x3A, 0x14,
	0x00, 0x3A, 0x14, 0x00,
	0x02, 0x06, 0xFF, 0xFF,
	0xFF, 0x0A, 0x00, 0xB3,
	0x00, 0xA1, 0x00, 0xB3,
	0x00, 0xC8,
};

static const unsigned char SC_DISABLE_CFG[] = {
	0xE9,
	0x03, 0x00, 0x00,
};

static unsigned char GAMMA_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00,
	0x01, 0x00, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80,
	0x80, 0x00, 0x00,
};


static unsigned char ELVSS_SETTING[] = {
	0xB6,
	0x19, 0xDC, 0x13,
};

void s6e36w2x01_testkey_enable(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&panel->dev);

	mutex_lock(&lcd->mipi_lock);

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_0,
		ARRAY_SIZE(TEST_KEY_ON_0)) < 0)
		dsim_err("failed to send TEST_KEY_ON_0.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_ON_1,
		ARRAY_SIZE(TEST_KEY_ON_1)) < 0)
		dsim_err("failed to send TEST_KEY_ON_1.\n");
}

void s6e36w2x01_testkey_disable(u32 id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *panel = dsim->ld;
	struct s6e36w2x01 *lcd = dev_get_drvdata(&panel->dev);

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_1,
		ARRAY_SIZE(TEST_KEY_OFF_1)) < 0)
		dsim_err("%s: failed to send TEST_KEY_OFF_1.\n", __func__);

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) TEST_KEY_OFF_0,
		ARRAY_SIZE(TEST_KEY_OFF_0)) < 0)
		dsim_err("%s: failed to send TEST_KEY_OFF_0.\n", __func__);

	mutex_unlock(&lcd->mipi_lock);
}

void s6e36w2x01_mdnie_outdoor_set(int id, enum mdnie_outdoor on)
{
	s6e36w2x01_testkey_enable(id);

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

	s6e36w2x01_testkey_disable(id);

	return;
}


void s6e36w2x01_mdnie_set(u32 id, enum mdnie_scenario scenario)
{
	if (scenario >= SCENARIO_MAX) {
		dsim_err("%s: Invalid scenario (%d)\n", __func__, scenario);
		return;
	}

	s6e36w2x01_testkey_enable(id);

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

	s6e36w2x01_testkey_disable(id);

	pr_info("%s:[%s]\n", __func__, mdnie_sc_str[scenario]);

	return;
}


int s6e36w2x01_read_mtp_reg(int id, u32 addr, char* buffer, u32 size)
{
	int ret = 0;

	s6e36w2x01_testkey_enable(id);

	if(dsim_rd_data(id, MIPI_DSI_DCS_READ,
		addr, size, buffer) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n", __func__, addr);
		ret = -EIO;
	}

	s6e36w2x01_testkey_disable(id);

	return ret;
}

void s6e36w2x01_init(int id, struct decon_lcd * lcd)
{
	/* Test Key Enable */
	s6e36w2x01_testkey_enable(id);

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
		(unsigned long) SC_FFC,
		ARRAY_SIZE(SC_FFC)) < 0)
		dsim_err("failed to send SC_FFC.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) SC_INIT_CFG,
		ARRAY_SIZE(SC_INIT_CFG)) < 0)
		dsim_err("failed to send SC_INIT_CFG.\n");

	/* Test key disable */
	s6e36w2x01_testkey_disable(id);
}

void s6e36w2x01_enable(int id)
{
	/* display on */
	if(dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
		DISPLAY_ON[0],	0) < 0)
		dsim_err("failed to send DISPLAY_ON.\n");
}

/* follow Panel Power off sequence */
void s6e36w2x01_disable(int id)
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

void s6e36w2x01_write_sideram_data(int id, struct s6e36w2x01 *lcd)
{
	int i;

	s6e36w2x01_testkey_enable(id);

	for (i = 0; i < SC_RAM_HIGHT; i++) {
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) &lcd->sc_buf[i*SC_RAM_WIDTH], SC_RAM_WIDTH) < 0)
			dsim_err("failed to send selfclock_needle_data[%d]\n", i);
	}

	s6e36w2x01_testkey_disable(id);

	lcd->panel_sc_state = true;

	pr_info("%s\n", "write_sideram_data");
}


void s6e36w2x01_disable_sc_config(int id)
{
	s6e36w2x01_testkey_enable(id);

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) SC_DISABLE_CFG,
		ARRAY_SIZE(SC_DISABLE_CFG)) < 0)
		dsim_err("failed to send SC_DISABLE_CFG.\n");

	s6e36w2x01_testkey_disable(id);

	msleep(SC_DISABLE_DELAY);

	pr_info("%s\n", "disable_sc_config");
}

void s6e36w2x01_write_sc_config(int id)
{
	s6e36w2x01_testkey_enable(id);

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) SC_CFG,
		ARRAY_SIZE(SC_CFG)) < 0)
		dsim_err("failed to send SC_CFG.\n");

	if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) SC_FFC,
		ARRAY_SIZE(SC_FFC)) < 0)
		dsim_err("failed to send SC_FFC.\n");

	s6e36w2x01_testkey_disable(id);

	pr_info("%s\n", "write_sc_config");
}

int s6e36w2x01_get_sc_time(int id, int *h, int* m, int* s, int *ms)
{
	unsigned char sc_buf[SC_TIME_REG_SIZE];
	int ret;

	ret = s6e36w2x01_read_mtp_reg(id, SC_TIME_REG, sc_buf, SC_TIME_REG_SIZE);
	if (ret) {
		pr_err("%s: Failed to get SC_TIME_REG[%d]\n", __func__, ret);
		*h = 0;
		*m = 0;
		*s = 0;
		*ms = 0;
		return -EIO;
	}

	*h = (sc_buf[0] & 0xf0) >> 4;
	*m = sc_buf[1] & 0x3f;
	*s = sc_buf[2] & 0x3f;
	*ms = sc_buf[0] & 0x0f;

	pr_info("%s:%02d:%02d:%02d.%d00\n", "get_sc_time", *h, *m, *s, *ms);

	return ret;
}

int s6e36w2x01_get_sc_register(int id, int cmd)
{
	unsigned char sc_buf[ARRAY_SIZE(SC_CFG)-1];
	int ret;

	ret = s6e36w2x01_read_mtp_reg(id, SC_CFG_REG, sc_buf, ARRAY_SIZE(SC_CFG)-1);
	if (ret) {
		pr_err("%s: Failed to get SC_CFG_REG[%d]\n", __func__, ret);
		return -EIO;
	}

	switch(cmd) {
	case SC_ON_SEL:
			ret = sc_buf[0] & 0x1;
			break;
	case SB_ON_SEL:
			ret = (sc_buf[0] & 0x2) >> 1;
			break;
	case SC_ANA_CLOCK_EN:
			ret = sc_buf[1] & 0x1;
			break;
	case SB_BLK_EN:
			ret = sc_buf[2] & 0x1;
			break;
	case SC_HBP:
			ret = sc_buf[3] & 0xff;
			break;
	case SC_VBP:
			ret = sc_buf[4] & 0xff;
			break;
	case SC_BG_COLOR:
			ret = (sc_buf[5] <<16) | (sc_buf[6] << 8) | (sc_buf[7] & 0xff);
			break;
	case SC_AA_WIDTH:
			ret = sc_buf[8] >> 6;
			break;
	case SC_UPDATE_RATE:
			ret = sc_buf[8] & 0x3f;
			break;
	case SC_TIME_UPDATE:
			ret = (sc_buf[9] & 0x80) >> 7;
			break;
	case SC_COMP_EN:
			ret = (sc_buf[9] & 0x40) >> 6;
			break;
	case SC_INC_STEP:
			ret = sc_buf[9] & 0x0f;
			break;
	case SC_LINE_COLOR:
			ret = (sc_buf[10] << 16) | (sc_buf[11] << 8) |(sc_buf[12] & 0xff);
			break;
	case SC_RADIUS:
			ret = sc_buf[13];
			break;
	case SC_CLS_CIR_ON:
			ret = (sc_buf[14] & 0x80) >> 7;
			break;
	case SC_OPN_CIR_ON:
			ret = (sc_buf[14] & 0x40) >> 6;
			break;
	case SC_RGB_INV:
			ret = (sc_buf[14] & 0x20) >> 5;
			break;
	case SC_LINE_WIDTH:
			ret = sc_buf[14] & 0x02;
			break;
	case SC_SET_HH:
			ret = sc_buf[15] & 0x0f;
			break;
	case SC_SET_MM:
			ret = sc_buf[16] & 0x3f;
			break;
	case SC_SET_SS:
			ret = sc_buf[17] & 0x3f;
			break;
	case SC_SET_MSS:
			ret = sc_buf[18] & 0x0f;
			break;
	case SC_CENTER_X:
			ret = (sc_buf[19] << 8) | (sc_buf[20] & 0xff);
			break;
	case SC_CENTER_Y:
			ret = ((sc_buf[21] & 0x03) << 8) |(sc_buf[22] & 0xff);
			break;
	case SC_HH_CENTER_X:
			ret = sc_buf[23] & 0xff;
			break;
	case SC_HH_CENTER_Y:
			ret = sc_buf[24] & 0xff;
			break;
	case SC_HH_RGB_MASK:
			ret = sc_buf[25] & 0x07;
			break;
	case SC_MM_CENTER_X:
			ret = sc_buf[26] & 0xff;
			break;
	case SC_MM_CENTER_Y:
			ret = sc_buf[27] & 0xff;
			break;
	case SC_MM_RGB_MASK:
			ret = sc_buf[28] & 0x07;
			break;
	case SC_SS_CENTER_X:
			ret = sc_buf[29] & 0xff;
			break;
	case SC_SS_CENTER_Y:
			ret = sc_buf[30] & 0xff;
			break;
	case SC_SS_RGB_MASK:
			ret = sc_buf[31] & 0x07;
			break;
	case SB_DE:
			ret = (sc_buf[32] & 0x10) >> 4;
			break;
	case SB_RATE:
			ret = sc_buf[32] & 0x0f;
			break;
	case SB_RADIUS:
			ret = sc_buf[33] & 0xff;
			break;
	case SB_LINE_COLOR_R:
			ret = sc_buf[34];
			break;
	case SB_LINE_COLOR_G:
			ret = sc_buf[35];
			break;
	case SB_LINE_COLOR_B:
			ret = sc_buf[36];
			break;
	case SB_LINE_WIDTH:
			ret = (sc_buf[37] & 0xc0) >> 6;
			break;
	case SB_AA_ON:
			ret = (sc_buf[37] & 0x08) >> 3;
			break;
	case SB_AA_WIDTH:
			ret = (sc_buf[37] & 0x06) >> 1;
			break;
	case SB_CIRCLE_1_X:
			ret = ((sc_buf[38] & 0x01) << 8) | (sc_buf[39] & 0xff);
			break;
	case SB_CIRCLE_1_Y:
			ret = ((sc_buf[40] & 0x03) << 8) | (sc_buf[41] & 0xff);
			break;
	case SB_CIRCLE_2_X:
			ret = ((sc_buf[42] & 0x01) << 8) | (sc_buf[43] & 0xff);
			break;
	case SB_CIRCLE_2_Y:
			ret = ((sc_buf[44] & 0x03) << 8) | (sc_buf[45] & 0xff);
			break;
	default:
		pr_err("%s:unkown command[%d]\n", __func__, cmd);
		return -EINVAL;
	};

	pr_info("%s:[%s][%d]\n", "get_sc_register",
			sc_cmd_str[cmd], ret);

	return ret;
}

int s6e36w2x01_get_sc_config(int cmd)
{
	int ret;

	switch(cmd) {
	case SC_ON_SEL:
			ret = SC_CFG[1] & 0x1;
			break;
	case SB_ON_SEL:
			ret = SC_CFG[1] & 0x2;
			break;
	case SC_ANA_CLOCK_EN:
			ret = SC_CFG[2] & 0x1;
			break;
	case SB_BLK_EN:
			ret = SC_CFG[3] & 0x1;
			break;
	case SC_HBP:
			ret = SC_CFG[4] & 0xff;
			break;
	case SC_VBP:
			ret = SC_CFG[5] & 0xff;
			break;
	case SC_BG_COLOR:
			ret = (SC_CFG[6] <<16) | (SC_CFG[7] << 8) | (SC_CFG[8] & 0xff);
			break;
	case SC_AA_WIDTH:
			ret = SC_CFG[9] >> 6;
			break;
	case SC_UPDATE_RATE:
			ret = SC_CFG[9] & 0x3f;
			break;
	case SC_TIME_UPDATE:
			ret = (SC_CFG[10] & 0x80) >> 7;
			break;
	case SC_COMP_EN:
			ret = (SC_CFG[10] & 0x40) >> 6;
			break;
	case SC_INC_STEP:
			ret = SC_CFG[10] & 0x0f;
			break;
	case SC_LINE_COLOR:
			ret = (SC_CFG[11] << 16) | (SC_CFG[12] << 8) |(SC_CFG[13] & 0xff);
			break;
	case SC_RADIUS:
			ret = SC_CFG[14];
			break;
	case SC_CLS_CIR_ON:
			ret = (SC_CFG[15] & 0x80) >> 7;
			break;
	case SC_OPN_CIR_ON:
			ret = (SC_CFG[15] & 0x40) >> 6;
			break;
	case SC_RGB_INV:
			ret = (SC_CFG[15] & 0x20) >> 5;
			break;
	case SC_LINE_WIDTH:
			ret = SC_CFG[15] & 0x02;
			break;
	case SC_SET_HH:
			ret = SC_CFG[16] & 0x0f;
			break;
	case SC_SET_MM:
			ret = SC_CFG[17] & 0x3f;
			break;
	case SC_SET_SS:
			ret = SC_CFG[18] & 0x3f;
			break;
	case SC_SET_MSS:
			ret = SC_CFG[19] & 0x0f;
			break;
	case SC_CENTER_X:
			ret = (SC_CFG[20] << 8) | (SC_CFG[21] & 0xff);
			break;
	case SC_CENTER_Y:
			ret = ((SC_CFG[22] & 0x03) << 8) |(SC_CFG[23] & 0xff);
			break;
	case SC_HH_CENTER_X:
			ret = SC_CFG[24] & 0xff;
			break;
	case SC_HH_CENTER_Y:
			ret = SC_CFG[25] & 0xff;
			break;
	case SC_HH_RGB_MASK:
			ret = SC_CFG[26] & 0x07;
			break;
	case SC_MM_CENTER_X:
			ret = SC_CFG[27] & 0xff;
			break;
	case SC_MM_CENTER_Y:
			ret = SC_CFG[28] & 0xff;
			break;
	case SC_MM_RGB_MASK:
			ret = SC_CFG[29] & 0x07;
			break;
	case SC_SS_CENTER_X:
			ret = SC_CFG[30] & 0xff;
			break;
	case SC_SS_CENTER_Y:
			ret = SC_CFG[31] & 0xff;
			break;
	case SC_SS_RGB_MASK:
			ret = SC_CFG[32] & 0x07;
			break;
	case SB_DE:
			ret = (SC_CFG[33] & 0x10) >> 4;
			break;
	case SB_RATE:
			ret = SC_CFG[33] & 0x0f;
			break;
	case SB_RADIUS:
			ret = SC_CFG[34] & 0xff;
			break;
	case SB_LINE_COLOR_R:
			ret = SC_CFG[35];
			break;
	case SB_LINE_COLOR_G:
			ret = SC_CFG[36];
			break;
	case SB_LINE_COLOR_B:
			ret = SC_CFG[37];
			break;
	case SB_LINE_WIDTH:
			ret = (SC_CFG[38] & 0xc0) >> 6;
			break;
	case SB_AA_ON:
			ret = (SC_CFG[38] & 0x08) >> 3;
			break;
	case SB_AA_WIDTH:
			ret = (SC_CFG[38] & 0x06) >> 1;
			break;
	case SB_CIRCLE_1_X:
			ret = ((SC_CFG[39] & 0x01) << 8) | (SC_CFG[40] & 0xff);
			break;
	case SB_CIRCLE_1_Y:
			ret = ((SC_CFG[41] & 0x03) << 8) | (SC_CFG[42] & 0xff);
			break;
	case SB_CIRCLE_2_X:
			ret = ((SC_CFG[43] & 0x01) << 8) | (SC_CFG[44] & 0xff);
			break;
	case SB_CIRCLE_2_Y:
			ret = ((SC_CFG[45] & 0x03) << 8) | (SC_CFG[46] & 0xff);
			break;
	default:
		pr_err("%s:unkown command[%d]\n", __func__, cmd);
		return -EINVAL;
	};

	pr_info("%s:[%s][%d]\n", "get_sc_config",
			sc_cmd_str[cmd], ret);

	return ret;
}

void s6e36w2x01_set_sc_config(int cmd, unsigned int param)
{
	switch(cmd) {
	case SC_ON_SEL:
			SC_CFG[1] = (SC_CFG[1] & 0x1) | ((param & 0x01) << 1);
			break;
	case SB_ON_SEL:
			SC_CFG[1] = (SC_CFG[1] & 0x2) | (param & 0x01);
			break;
	case SC_ANA_CLOCK_EN:
			SC_CFG[2] = param & 0x01;
			if (param & 0x01)
				SC_CFG[3] = 0;
			break;
	case SB_BLK_EN:
			SC_CFG[3] = param & 0x01;
			if (param & 0x01)
				SC_CFG[2] = 0;
			break;
	case SC_HBP:
			SC_CFG[4] = param & 0xff;
			break;
	case SC_VBP:
			SC_CFG[5] = param & 0xff;
			break;
	case SC_BG_COLOR:
			SC_CFG[6] = (param & 0xff0000) >>16;
			SC_CFG[7] = (param & 0x00ff00) >> 8;
			SC_CFG[8] = param & 0x0000ff;
			break;
	case SC_AA_WIDTH:
			SC_CFG[9] = (SC_CFG[9] & 0x3f) | ((param & 0x03)<<6);
			break;
	case SC_UPDATE_RATE:
			SC_CFG[9] = (SC_CFG[9] & 0xc0) | (param & 0x3f);
			break;
	case SC_TIME_UPDATE:
			SC_CFG[10] = (SC_CFG[10] & 0x7f) | ((param & 0x01) << 7) ;
			break;
	case SC_COMP_EN:
			SC_CFG[10] = (SC_CFG[10] & 0xbf) | ((param & 0x01) << 6) ;
			break;
	case SC_INC_STEP:
			SC_CFG[10] = (SC_CFG[10] & 0xf0) | (param & 0x0f);
			break;
	case SC_LINE_COLOR:
			SC_CFG[11] = (param & 0xff0000) >>16;
			SC_CFG[12] = (param & 0x00ff00) >> 8;
			SC_CFG[13] = param & 0x0000ff;
			break;
	case SC_RADIUS:
			SC_CFG[14] = param;
			break;
	case SC_CLS_CIR_ON:
			SC_CFG[15] = (SC_CFG[15] & 0x7f) | ((param & 0x01) << 7) ;
			break;
	case SC_OPN_CIR_ON:
			SC_CFG[15] = (SC_CFG[15] & 0xbf) | ((param & 0x01) << 6) ;
			break;
	case SC_RGB_INV:
			SC_CFG[15] = (SC_CFG[15] & 0xdf) | ((param & 0x01) << 5) ;
			break;
	case SC_LINE_WIDTH:
			SC_CFG[15] = (SC_CFG[15] & 0xfb) | (param & 0x03);
			break;
	case SC_SET_HH:
			SC_CFG[16] = param & 0x0f;
			break;
	case SC_SET_MM:
			SC_CFG[17] = param & 0x3f;
			break;
	case SC_SET_SS:
			SC_CFG[18] = param & 0x3f;
			break;
	case SC_SET_MSS:
			SC_CFG[19] = param & 0x0f;
			break;
	case SC_CENTER_X:
			SC_CFG[20] = (param & 0x100) >> 8;
			SC_CFG[21] = param & 0xff;
			break;
	case SC_CENTER_Y:
			SC_CFG[22] = (param & 0x300) >> 8;
			SC_CFG[23] = param & 0xff;
			break;
	case SC_HH_CENTER_X:
			SC_CFG[24] = param & 0xff;
			break;
	case SC_HH_CENTER_Y:
			SC_CFG[25] = param & 0xff;
			break;
	case SC_HH_RGB_MASK:
			SC_CFG[26] = (SC_CFG[26] & 0xf8) | (param & 0x07);
			break;
	case SC_MM_CENTER_X:
			SC_CFG[27] = param & 0xff;
			break;
	case SC_MM_CENTER_Y:
			SC_CFG[28] = param & 0xff;
			break;
	case SC_MM_RGB_MASK:
			SC_CFG[29] = (SC_CFG[29] & 0xf8) | (param & 0x07);
			break;
	case SC_SS_CENTER_X:
			SC_CFG[30] = param & 0xff;
			break;
	case SC_SS_CENTER_Y:
			SC_CFG[31] = param & 0xff;
			break;
	case SC_SS_RGB_MASK:
			SC_CFG[32] = (SC_CFG[32] & 0xf8) | (param & 0x07);
			break;
	case SB_DE:
			SC_CFG[33] = (SC_CFG[33] & 0x0f) | ((param & 0x01) << 4);
			break;
	case SB_RATE:
			SC_CFG[33] = (SC_CFG[33] & 0x10) | (param & 0x0f);
			break;
	case SB_RADIUS:
			SC_CFG[34] = param & 0xff;
			break;
	case SB_LINE_COLOR_R:
			SC_CFG[35] = param & 0xff;
			break;
	case SB_LINE_COLOR_G:
			SC_CFG[36] = param & 0xff;
			break;
	case SB_LINE_COLOR_B:
			SC_CFG[37] = param & 0xff;
			break;
	case SB_LINE_WIDTH:
			SC_CFG[38] = (SC_CFG[38] & 0x3f) | ((param & 0x03) << 6);
			break;
	case SB_AA_ON:
			SC_CFG[38] = (SC_CFG[38] & 0xc7) | ((param & 0x01) << 3);
			break;
	case SB_AA_WIDTH:
			SC_CFG[38] = (SC_CFG[38] & 0xc8) | ((param & 0x03) << 1);
			break;
	case SB_CIRCLE_1_X:
			SC_CFG[39] = (param & 0x100) >> 8;
			SC_CFG[40] = param & 0xff;
			break;
	case SB_CIRCLE_1_Y:
			SC_CFG[41] = (param & 0x300) >> 8;
			SC_CFG[42] = param & 0xff;
			break;
	case SB_CIRCLE_2_X:
			SC_CFG[43] = (param & 0x100) >> 8;
			SC_CFG[44] = param & 0xff;
			break;
	case SB_CIRCLE_2_Y:
			SC_CFG[45] = (param & 0x300) >> 8;
			SC_CFG[46] = param & 0xff;
			break;
	default:
		pr_err("%s:unkown command[%d]\n", __func__, cmd);
		return;
	};

	pr_info("%s:[%s][%d]\n", "set_sc_config",
			sc_cmd_str[cmd], param);
}

int s6e36w2x01_gamma_ctrl(int id, u32 backlightlevel)
{
	return 0;
}

int s6e36w2x01_gamma_update(int id)
{
	return 0;
}

void s6e36w2x01_hlpm_ctrl(struct s6e36w2x01 *lcd, bool enable)
{
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *priv = &dsim->priv;
	struct backlight_device *bd = lcd->bd;
	int brightness = bd->props.brightness;

	if (enable) {
		s6e36w2x01_testkey_enable(dsim->id);
		if (priv->id[2] == 0x24) {
			pr_info("Force HLPM_PRE_ON_24 for ID3 == 24.\n");
			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_PRE_ON_24,
				ARRAY_SIZE(HLPM_PRE_ON_24)) < 0)
				dsim_err("failed to send HLPM_PRE_ON_24.\n");
		} else if (priv->id[2] >= 0x14) {
			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_PRE_ON,
				ARRAY_SIZE(HLPM_PRE_ON)) < 0)
				dsim_err("failed to send HLPM_PRE_ON.\n");
		} else {
			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_PRE_ON_0,
				ARRAY_SIZE(HLPM_PRE_ON_0)) < 0)
				dsim_err("failed to send HLPM_PRE_ON_0.\n");

			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_PRE_ON_1,
				ARRAY_SIZE(HLPM_PRE_ON_1)) < 0)
				dsim_err("failed to send HLPM_PRE_ON_1.\n");

			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_PRE_ON_2,
				ARRAY_SIZE(HLPM_PRE_ON_2)) < 0)
				dsim_err("failed to send HLPM_PRE_ON_2.\n");

			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_PRE_ON_3,
				ARRAY_SIZE(HLPM_PRE_ON_3)) < 0)
				dsim_err("failed to send HLPM_PRE_ON_3.\n");
		}

		if (brightness < BRIGHTNESS_LEVEL_40NIT) {
			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_5NIT_ON,
				ARRAY_SIZE(HLPM_5NIT_ON)) < 0)
				dsim_err("failed to send HLPM_5NIT_ON.\n");
			lcd->hlpm_nit = HLPM_NIT_LOW;
		} else {
			if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HLPM_40NIT_ON,
				ARRAY_SIZE(HLPM_40NIT_ON)) < 0)
				dsim_err("failed to send HLPM_40NIT_ON.\n");
			lcd->hlpm_nit = HLPM_NIT_HIGH;
		}
		s6e36w2x01_testkey_disable(dsim->id);

		pr_info("%s:on:hnit[%d]br[%d]\n", "hlpm_ctrl", lcd->hlpm_nit, brightness);
	} else {
		s6e36w2x01_testkey_enable(dsim->id);

		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) NORMAL_ON,
			ARRAY_SIZE(NORMAL_ON)) < 0)
			dsim_err("failed to send NORMAL_ON.\n");
		lcd->hlpm_nit = HLPM_NIT_OFF;

		s6e36w2x01_testkey_disable(dsim->id);

		s6e36w2x01_gamma_ctrl(dsim->id, lcd->br_map[brightness]);
		pr_info("%s:off:br[%d]\n", "hlpm_ctrl", brightness);
	}

	return;
}

int s6e36w2x01_hbm_on(int id)
{
	struct dsim_device *dsim = get_dsim_drvdata(id);
	struct lcd_device *lcd = dsim->ld;
	struct s6e36w2x01 *panel = dev_get_drvdata(&lcd->dev);
	struct panel_private *priv = &dsim->priv;

	if (!panel) {
		pr_info("%s: LCD is NULL\n", __func__);
		return 0;
	}
	ELVSS_SETTING[3] = 0x13;
	ELVSS_SETTING[2] = 0xDC;

	s6e36w2x01_testkey_enable(id);

	if (panel->hbm_on) {
		printk("[LCD] %s : HBM ON\n", __func__);
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ELVSS_SETTING,
			ARRAY_SIZE(ELVSS_SETTING)) < 0)
			dsim_err("failed to send ELVSS_SETTING.\n");

		if(priv->id[2] >= 0x14){
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HBM_ACL_ON,
				ARRAY_SIZE(HBM_ACL_ON)) < 0)
				dsim_err("failed to send HBM_ACL_ON.\n");
		} else {
			if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
				(unsigned long) HBM_ACL_ON_OLD,
				ARRAY_SIZE(HBM_ACL_ON_OLD)) < 0)
				dsim_err("failed to send HBM_ACL_ON.\n");
		}
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) ACL_ON,
			ARRAY_SIZE(ACL_ON)) < 0)
			dsim_err("failed to send HBM_ON.\n");
		if(dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) HBM_ON,
			ARRAY_SIZE(HBM_ON)) < 0)
			dsim_err("failed to send HBM_ON.\n");
	} else{
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
	}
	s6e36w2x01_testkey_disable(id);

	return 0;
}

int s6e36w2x01_temp_offset_comp(int id, unsigned int stage)
{
	s6e36w2x01_testkey_enable(id);

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

	s6e36w2x01_testkey_disable(id);

	return 0;
}

