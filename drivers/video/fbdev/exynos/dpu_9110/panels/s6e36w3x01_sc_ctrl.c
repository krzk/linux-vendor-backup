/* drivers/video/fbdev/exynos/dpu_9110/panels/s6e36w3x01_sc_ctrl.c
 *
 * Samsung Self Clock CONTROL functions
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * Sangmin,Lee <lsmin.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>
#include "../dsim.h"
#include "../decon.h"
#ifdef CONFIG_EXYNOS_DECON_LCD_S6E36W3X01_L
#include "s6e36w3x01_l_dimming.h"
#include "s6e36w3x01_l_param.h"
#include "s6e36w3x01_l_mipi_lcd.h"
#include "s6e36w3x01_l_lcd_ctrl.h"
#else
#include "s6e36w3x01_s_dimming.h"
#include "s6e36w3x01_s_param.h"
#include "s6e36w3x01_s_mipi_lcd.h"
#include "s6e36w3x01_s_lcd_ctrl.h"
#endif
#include "s6e36w3x01_sc_ctrl.h"
#include "s6e36w3x01_sc_sample_afpc.h"

#define 	SC_MIN_POSITION	1
#define 	SC_MAX_POSITION	360
#define 	SC_DEFAULT_POSITION	180
#define 	SC_DEFAULT_DATA	0xff
#define 	SC_MSEC_DIVIDER	100
#define 	SC_MAX_HOUR		12
#define 	SC_MAX_MSEC		10
#define 	SC_MIN_UPDATE_RATE	1
#define 	SC_MAX_UPDATE_RATE	10
#define 	SC_PANEL_VSYNC_RATE	30
#define 	SC_CMD_SIZE		1
#define	SC_START_CMD		0x4c
#define	SC_IMAGE_CMD		0x5c
#define	SC_CFG_REG		0xE9
#define	SC_TIME_REG		0x82
#define	SC_DISABLE_DELAY	40 //It need a one frame delay time in HLPM
#define	SC_TIME_REG_SIZE	19
#define	SC_COMP_SPEED		30
#define	SC_1FRAME_MSEC	34 /* 34 ms */
#define	SC_COMP_NEED_TIME	30 /* 3 seconds */
#define	SC_COMP_STEP_VALUE	3

#define 	SC_HANDS_DATA_SIZE	86400
#define 	SC_HANDS_RAM_HIGHT	45
#define 	SC_HANDS_SECTION	3
#define 	SC_HANDS_HIGHT	(SC_HANDS_RAM_HIGHT/SC_HANDS_SECTION)
#define 	SC_HANDS_WIDTH	(SC_HANDS_DATA_SIZE/SC_HANDS_RAM_HIGHT)
#define 	SC_HANDS_RAM_WIDTH	(SC_HANDS_WIDTH+SC_CMD_SIZE)

#define 	SC_FONT_DATA_SIZE	92160
#define 	SC_FONT_DATA_HIGHT	48
#define 	SC_FONT_DATA_WIDTH	(SC_FONT_DATA_SIZE/SC_FONT_DATA_HIGHT)
#define 	SC_FONT_DATA_RAM_WIDTH	(SC_FONT_DATA_WIDTH+SC_CMD_SIZE)

#define 	AFPC_DATA_SIZE	194400
#define 	AFPC_RAM_HIGHT	100
#define 	AFPC_DATA_WIDTH	(AFPC_DATA_SIZE/AFPC_RAM_HIGHT)
#define 	AFPC_RAM_WIDTH	(AFPC_DATA_WIDTH+SC_CMD_SIZE)

#define 	ICON_DATA_SIZE	9600
#define 	ICON_RAM_HIGHT	5
#define 	ICON_DATA_WIDTH	(ICON_DATA_SIZE/ICON_RAM_HIGHT)
#define 	ICON_RAM_WIDTH	(ICON_DATA_WIDTH+SC_CMD_SIZE)

#define 	BLINK_MAX_SYNC_INDEX	2
#define 	BLINK_DEFAULT_STEP	0
#define 	BLINK_DEFAULT_RATE	3

static char sc_select_cfg[SC_SELECT_CFG_SIZE] = {
	0x75, 0x00, 0x00,
};

static char sc_common_cfg[SC_COMMON_CFG_SIZE] = {
	0x76, 0x00, 0x0a, 0x08, 0x00, 0x00, 0x1e, 0x03,
	0x17, 0x06, 0x01, 0x68, 0x01, 0x68
};

static char sc_analog_cfg[SC_ANLALOG_CFG_SIZE] = {
	0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xB4, 0x00, 0xB4, 0x00, 0xB4, 0x00, 0xB4,
	0x00, 0xB4, 0x00, 0xB4, 0x00, 0x00, 0x00, 0x3C,
	0x1E, 0x3C, 0x1E, 0x3C, 0x1E, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static char sc_digital_cfg[SC_DIGITAL_CFG_SIZE] = {
	0x78, 0x03, 0x15, 0x10, 0x06, 0x00, 0x28, 0x00,
	0x4b, 0x00, 0x48, 0x00, 0x6e, 0x00, 0x00, 0x00,
	0x85, 0x00, 0x49, 0x00, 0x85, 0x00, 0x92, 0x00,
	0x85, 0x00, 0xdb, 0x00, 0x85, 0x01, 0x24, 0x00,
	0x85, 0x01, 0x24, 0x00, 0xd5, 0x00, 0x00, 0x00,
	0xb5, 0x00, 0x50, 0x00, 0xb5, 0x00, 0xa0, 0x00,
	0xbe, 0x00, 0xf0, 0x00, 0xbe, 0x00, 0x01, 0x01,
	0x04, 0x00, 0x46, 0x01, 0x04, 0x00, 0x96, 0x01,
	0x18, 0x00, 0xe6, 0x01, 0x18, 0x00, 0xb4, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static char sc_blink_cfg[SC_BLINK_CFG_SIZE] = {
	0x79, 0x00, 0x01, 0x1E, 0x00, 0x64, 0x00, 0x64,
	0x00, 0xFF, 0x00, 0x64, 0x00, 0x19, 0x00, 0x50,
	0x00, 0x00, 0x00, 0x00
};

static char sc_time_update_cfg[SC_TIME_UPDATE_CFG_SIZE] = {
	0x7a, 0x01
};

static char sc_icon_cfg[SC_ICON_CFG_SIZE] = {
	0x7e, 0x00, 0x01, 0x00, 0x64, 0x00, 0xb4, 0x00,
	0xC8, 0x00, 0x50
};

static char sc_move_cfg[SC_MOVE_CFG_SIZE] = {
	0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x19,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

static char sc_afpc_cfg[SC_AFPC_CFG_SIZE] = {
	0x81, 0x00, 0x01, 0xFC, 0xF6, 0xD6, 0x00, 0x00,
	0x00, 0x01
};

static const char* sc_sel_cfg_str[] = {
	"AC_SEL",
	"DC_SEL",
	"IC_SEL",
	"SS_SEL",
	"MA_SEL",
	"SCP_SEL",
	"AC_ENC_BP",
};

static const char* sc_comm_cfg_str[] = {
	"SC_TIME_STOP_EN",
	"SC_TIME_EN",
	"SC_DISP_ON",
	"SC_SET_HH",
	"SC_SET_MM",
	"SC_SET_SS",
	"SC_SET_MSS",
	"SC_UPDATE_RATE",
	"SC_COMP_STEP",
	"SC_COMP_EN",
	"SC_INC_STEP",
	"SELF_HBP",
	"SELF_VBP",
	"DISP_CONTENT_X",
	"DISP_CONTENT_Y",
};

static const char* sc_digi_cfg_str[] = {
	"SC_D_CLOCK_EN",
	"SC_24H_EN",
	"SC_D_MIN_LOCK_EN",
	"SC_D_EN_HH",
	"SC_D_EN_MM",
	"SC_D_EN_SS",
	"SC_D_EN_WD1",
	"SC_D_EN_WD2",
	"SC_D_ALPHA_INV",
	"SELF_FONT_MODE",
	"SC_D_WD_DISP_OP",
	"SC_D_EN_HH_FONT",
	"SC_D_EN_MM_FONT",
	"SC_D_EN_SS_FONT",
	"SC_D_EN_WD1_FONT",
	"SC_D_EN_WD2_FONT",
	"SC_D_FONT0_IMG_WIDTH",
	"SC_D_FONT0_IMG_HEIGHT",
	"SC_D_FONT1_IMG_WIDTH",
	"SC_D_FONT1_IMG_HEIGHT",
	"SC_D_ST_HH_X10",
	"SC_D_ST_HH_Y10",
	"SC_D_ST_HH_X01",
	"SC_D_ST_HH_Y01",
	"SC_D_ST_MM_X10",
	"SC_D_ST_MM_Y10",
	"SC_D_ST_MM_X01",
	"SC_D_ST_MM_Y01",
	"SC_D_ST_SS_X10",
	"SC_D_ST_SS_Y10",
	"SC_D_ST_SS_X01",
	"SC_D_ST_SS_Y01",
	"SC_D_ST_WD1_HH_X10",
	"SC_D_ST_WD1_HH_Y10",
	"SC_D_ST_WD1_HH_X01",
	"SC_D_ST_WD1_HH_Y01",
	"SC_D_ST_WD1_MM_X10",
	"SC_D_ST_WD1_MM_Y10",
	"SC_D_ST_WD1_MM_X01",
	"SC_D_ST_WD1_MM_Y01",
	"SC_D_ST_WD2_HH_X10",
	"SC_D_ST_WD2_HH_Y10",
	"SC_D_ST_WD2_HH_X01",
	"SC_D_ST_WD2_HH_Y01",
	"SC_D_ST_WD2_MM_X10",
	"SC_D_ST_WD2_MM_Y10",
	"SC_D_ST_WD2_MM_X01",
	"SC_D_ST_WD2_MM_Y01",
	"SC_D_WD_END_LINE",
	"SC_D_WD1_HH",
	"SC_D_WD1_MM",
	"SC_D_WD2_HH",
	"SC_D_WD2_MM",
	"SC_D_WD1_SIGN",
	"SC_D_WD2_SIGN",
};

static const char* sc_anal_cfg_str[] = {
	"SC_ON_SEL",
	"SC_ANA_CLOCK_EN",
	"SC_HH_MOTION",
	"SC_MM_MOTION",
	"SC_HH_FIX",
	"SC_MM_FIX",
	"SC_SS_FIX",
	"SC_SET_FIX_HH",
	"SC_SET_FIX_MM",
	"SC_SET_FIX_SS",
	"SC_SET_FIX_MSS",
	"SC_HH_HAND_CENTER_X",
	"SC_HH_HAND_CENTER_Y",
	"SC_MM_HAND_CENTER_X",
	"SC_MM_HAND_CENTER_Y",
	"SC_SS_HAND_CENTER_X",
	"SC_SS_HAND_CENTER_Y",
	"SC_HMS_LAYER",
	"SC_HMS_HH_MASK",
	"SC_HMS_MM_MASK",
	"SC_HMS_SS_MASK",
	"SC_HH_CENTER_X_CORR",
	"SC_HH_CENTER_Y_CORR",
	"SC_ROTATE",
	"SC_MM_CENTER_X_CORR",
	"SC_MM_CENTER_Y_CORR",
	"SC_SS_CENTER_X_CORR",
	"SC_SS_CENTER_Y_CORR",
	"SC_HH_CENTER_X",
	"SC_HH_CENTER_Y",
	"SC_MM_CENTER_X",
	"SC_MM_CENTER_Y",
	"SC_SS_CENTER_X",
	"SC_SS_CENTER_Y",
	"SC_HH_X_ST_MASK",
	"SC_HH_Y_ST_MASK",
	"SC_HH_X_ED_MASK",
	"SC_HH_Y_ED_MASK",
	"SC_MM_X_ST_MASK",
	"SC_MM_Y_ST_MASK",
	"SC_MM_X_ED_MASK",
	"SC_MM_Y_ED_MASK",
	"SC_SS_X_ST_MASK",
	"SC_SS_Y_ST_MASK",
	"SC_SS_X_ED_MASK",
	"SC_SS_Y_ED_MASK",
	"SC_HH_FLIP_EN",
	"SC_MM_FLIP_EN",
	"SC_SS_FLIP_EN",
	"SC_FLIP_ST",
	"SC_FLIP_ED",
};

static const char* aod_mode_str[] = {
	"AOD_DISABLE",
	"AOD_ALPM",
	"AOD_HLPM",
	"AOD_SCLK_ANALOG",
	"AOD_SCLK_DIGITAL",
};

static const char* aod_request_type_str[] = {
	"AOD_SET_CONFIG",
	"AOD_UPDATE_DATA",
};

static const char* aod_data_cmd_str[] = {
	"AOD_DATA_ENABLE",
	"AOD_DATA_SET",
	"AOD_DATA_POS",
};

static const char* aod_mask_str[] = {
	"SC_MASK_DISABLE",
	"SC_MASK_ENABLE",
};

static const char* sc_blink_cfg_str[] = {
	"SB_ON_SEL",
	"SB_BLK0_EN",
	"SB_BLK1_EN",
	"SB_ALPHA_INV",
	"SB_BLINK_RATE",
	"SB_BLINK0_FONT",
	"SB_BLINK1_FONT",
	"SB_BLINK_SYNC",
	"SB_ST_BLINK0_X",
	"SB_ST_BLINK0_Y",
	"SB_ST_BLINK1_X",
	"SB_ST_BLINK1_Y",
	"SB_FONT0_IMG_WIDTH",
	"SB_FONT0_IMG_HEIGHT",
	"SB_FONT1_IMG_WIDTH",
	"SB_FONT1_IMG_HEIGHT",
};

static const char* sc_afpc_cfg_str[] = {
	"SCP_ON_SEL",
	"SCP_COMPEN_EN",
	"SCP_MIN_RED",
	"SCP_MIN_GREEN",
	"SCP_MIN_BLUE",
	"SCP_BASE_RED",
	"SCP_BASE_GREEN",
	"SCP_BASE_BLUE",
	"SCP_BASE_OPT",
};

static const char* sc_icon_cfg_str[] = {
	"SI_ON_SEL",
	"SI_ICON_EN",
	"SI_ALPHA_INV",
	"SI_ST_X",
	"SI_ST_Y",
	"SI_IMG_WIDTH",
	"SI_IMG_HEIGHT",
};

static const char* sc_move_cfg_str[] = {
	"MV_ON_SEL",
	"SCHEDULER_EN_I",
	"SS_MOVE_ON",
	"SC_MOVE_ON",
	"SB_MOVE_ON",
	"SI_MOVE_ON",
	"SD_MOVE_ON",
	"SE_MOVE_ON",
	"DSP_X_ORG",
	"DSP_Y_ORG",
	"SC_MOVE_UPDATE_RATE",
	"SC_MAX_MOVING_EN",
	"SC_MAX_MOVING_POINT",
};

static int s6e36w3x01_set_select_cfg(enum sc_digital_cfg_t cmd)
{
	switch (cmd) {
	case AC_ENC_BP:
		sc_select_cfg[1] = 0x81;
		break;
	case SCP_SEL:
		sc_select_cfg[1] = 0x20;
		break;
	case MA_SEL:
		sc_select_cfg[1] = 0x10;
		break;
	case SS_SEL:
		sc_select_cfg[1] = 0x08;
		break;
	case IC_SEL:
		sc_select_cfg[1] = 0x04;
		break;
	case DC_SEL:
		sc_select_cfg[1] = 0x02;
		break;
	case AC_SEL:
		sc_select_cfg[1] = 0x01;
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		return -1;
	};

	pr_info("%s:[%s]\n", __func__, sc_sel_cfg_str[cmd]);

	return 0;
}

int s6e36w3x01_get_common_cfg(enum sc_common_cfg_t cmd)
{
	int value = 0;

	switch (cmd) {
	case SC_TIME_STOP_EN:
		value = ((sc_common_cfg[1] & 0x04) >>2);
		break;
	case SC_TIME_EN:
		value = ((sc_common_cfg[1] & 0x02) >>1);
		break;
	case SC_DISP_ON:
		value = (sc_common_cfg[1] & 0x01);
		break;
	case SC_SET_HH:
		value = (sc_common_cfg[2] & 0x1f);
		break;
	case SC_SET_MM:
		value = (sc_common_cfg[3] & 0x3f);
		break;
	case SC_SET_SS:
		value = (sc_common_cfg[4] & 0x3f);
		break;
	case SC_SET_MSS:
		value = (sc_common_cfg[5] & 0x0f);
		break;
	case SC_UPDATE_RATE:
		value = (sc_common_cfg[6] & 0x3f);
		break;
	case SC_COMP_STEP:
		value = ((sc_common_cfg[7] & 0x60) >>5);
		break;
	case SC_COMP_EN:
		value = ((sc_common_cfg[7] & 0x10) >>4);
		break;
	case SC_INC_STEP:
		value = (sc_common_cfg[7] & 0x03);
		break;
	case SELF_HBP:
		value = sc_common_cfg[8];
		break;
	case SELF_VBP:
		value = sc_common_cfg[9];
		break;
	case DISP_CONTENT_X:
		value = ((sc_common_cfg[10] & 0x01) <<8);
		value |= sc_common_cfg[11];
		break;
	case DISP_CONTENT_Y:
		value = ((sc_common_cfg[12] & 0x03) <<8);
		value |= sc_common_cfg[13];
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		return -1;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_comm_cfg_str[cmd], value);

	return value;
}

int s6e36w3x01_set_common_cfg(enum sc_common_cfg_t cmd, int value)
{
	switch (cmd) {
	case SC_TIME_STOP_EN:
		value &= 0x01;
		sc_common_cfg[1] = (sc_common_cfg[1] & ~0x04) | (value<<2);
		break;
	case SC_TIME_EN:
		value &= 0x01;
		sc_common_cfg[1] = (sc_common_cfg[1] & ~0x02) | (value<<1);
		break;
	case SC_DISP_ON:
		value &= 0x01;
		sc_common_cfg[1] = (sc_common_cfg[1] & ~0x01) | value;
		break;
	case SC_SET_HH:
		value &= 0x1f;
		sc_common_cfg[2] = (sc_common_cfg[2] & ~0x1f) | value;
		break;
	case SC_SET_MM:
		value &= 0x3f;
		sc_common_cfg[3] = (sc_common_cfg[3] & ~0x3f) | value;
		break;
	case SC_SET_SS:
		value &= 0x3f;
		sc_common_cfg[4] = (sc_common_cfg[4] & ~0x3f) | value;
		break;
	case SC_SET_MSS:
		value &= 0x0f;
		sc_common_cfg[5] = (sc_common_cfg[5] & ~0x0f) | value;
		break;
	case SC_UPDATE_RATE:
		value &= 0x3f;
		sc_common_cfg[6] = (sc_common_cfg[6] & ~0x3f) | value;
		break;
	case SC_COMP_STEP:
		value &= 0x03;
		sc_common_cfg[7] = (sc_common_cfg[7] & ~0x60) | (value<<5);
		break;
	case SC_COMP_EN:
		value &= 0x01;
		sc_common_cfg[7] = (sc_common_cfg[7] & ~0x10) | (value<<4);
		break;
	case SC_INC_STEP:
		value &= 0x03;
		sc_common_cfg[7] = (sc_common_cfg[7] & ~0x03) | value;
		break;
	case SELF_HBP:
		value &= 0xff;
		sc_common_cfg[8] = value;
		break;
	case SELF_VBP:
		value &= 0xff;
		sc_common_cfg[9] = value;
		break;
	case DISP_CONTENT_X:
		value &= 0x1ff;
		sc_common_cfg[10] = (sc_common_cfg[10] & ~0x01) | (value>>8);
		sc_common_cfg[11] = (value & 0xff);
		break;
	case DISP_CONTENT_Y:
		value &= 0x3ff;
		sc_common_cfg[12] = (sc_common_cfg[10] & ~0x03) | (value>>8);
		sc_common_cfg[13] = (value & 0xff);
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		return -1;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_comm_cfg_str[cmd], value);

	return 0;
}

int s6e36w3x01_set_analog_cfg(enum sc_analog_cfg_t cmd, int value)
{
	switch (cmd) {
	case SC_ON_SEL:
		value &= 0x01;
		sc_analog_cfg[1] = (sc_analog_cfg[1] & ~0x01) | value;
		break;
	case SC_ANA_CLOCK_EN:
		value &= 0x01;
		sc_analog_cfg[2] = (sc_analog_cfg[2] & ~0x01) | value;
		break;
	case SC_HH_MOTION:
		value &= 0x01;
		sc_analog_cfg[3] = (sc_analog_cfg[3] & ~0x20) | (value<<5);
		break;
	case SC_MM_MOTION:
		value &= 0x01;
		sc_analog_cfg[3] = (sc_analog_cfg[3] & ~0x10) | (value<<4);
		break;
	case SC_HH_FIX:
		value &= 0x01;
		sc_analog_cfg[3] = (sc_analog_cfg[3] & ~0x04) | (value<<2);
		break;
	case SC_MM_FIX:
		value &= 0x01;
		sc_analog_cfg[3] = (sc_analog_cfg[3] & ~0x02) | (value<<1);
		break;
	case SC_SS_FIX:
		value &= 0x01;
		sc_analog_cfg[3] = (sc_analog_cfg[3] & ~0x01) | value;
		break;
	case SC_SET_FIX_HH:
		value &= 0x1f;
		sc_analog_cfg[4] = (sc_analog_cfg[4] & ~0x1f) | value;
		break;
	case SC_SET_FIX_MM:
		value &= 0x3f;
		sc_analog_cfg[5] = (sc_analog_cfg[5] & ~0x3f) | value;
		break;
	case SC_SET_FIX_SS:
		value &= 0x3f;
		sc_analog_cfg[6] = (sc_analog_cfg[6] & ~0x3f) | value;
		break;
	case SC_SET_FIX_MSS:
		value &= 0x0f;
		sc_analog_cfg[7] = (sc_analog_cfg[7] & ~0x0f) | value;
		break;
	case SC_HH_HAND_CENTER_X:
		value &= 0x1ff;
		sc_analog_cfg[8] = (sc_analog_cfg[8] & ~0x01) | (value>>8);
		sc_analog_cfg[9] = (value & 0xff);
		break;
	case SC_HH_HAND_CENTER_Y:
		value &= 0x3ff;
		sc_analog_cfg[10] = (sc_analog_cfg[10] & ~0x03) | (value>>8);
		sc_analog_cfg[11] = (value & 0xff);
		break;
	case SC_MM_HAND_CENTER_X:
		value &= 0x1ff;
		sc_analog_cfg[12] = (sc_analog_cfg[12] & ~0x01) | (value>>8);
		sc_analog_cfg[13] = (value & 0xff);
		break;
	case SC_MM_HAND_CENTER_Y:
		value &= 0x3ff;
		sc_analog_cfg[14] = (sc_analog_cfg[14] & ~0x03) | (value>>8);
		sc_analog_cfg[15] = (value & 0xff);
		break;
	case SC_SS_HAND_CENTER_X:
		value &= 0x1ff;
		sc_analog_cfg[16] = (sc_analog_cfg[16] & ~0x01) | (value>>8);
		sc_analog_cfg[17] = (value & 0xff);
		break;
	case SC_SS_HAND_CENTER_Y:
		value &= 0x3ff;
		sc_analog_cfg[18] = (sc_analog_cfg[18] & ~0x03) | (value>>8);
		sc_analog_cfg[19] = (value & 0xff);
		break;
	case SC_HMS_LAYER:
		value &= 0x07;
		sc_analog_cfg[20] = (sc_analog_cfg[20] & ~0x70) | (value<<4);
		break;
	case SC_HMS_HH_MASK:
		value &= 0x01;
		sc_analog_cfg[20] = (sc_analog_cfg[20] & ~0x04) | (value<<2);
		break;
	case SC_HMS_MM_MASK:
		value &=0x01;
		sc_analog_cfg[20] = (sc_analog_cfg[20] & ~0x02) | (value<<1);
		break;
	case SC_HMS_SS_MASK:
		value &= 0x01;
		sc_analog_cfg[20] = (sc_analog_cfg[20] & ~0x01) | value;
		break;
	case SC_HH_CENTER_X_CORR:
		value &= 0x03;
		sc_analog_cfg[21] = (sc_analog_cfg[21] & ~0xc0) | (value<<6);
		break;
	case SC_HH_CENTER_Y_CORR:
		value &= 0x03;
		sc_analog_cfg[21] = (sc_analog_cfg[21] & ~0x30) | (value<<4);
		break;
	case SC_ROTATE:
		value &= 0x03;
		sc_analog_cfg[21] = (sc_analog_cfg[21] & ~0x03) | value;
		break;
	case SC_MM_CENTER_X_CORR:
		value &= 0x03;
		sc_analog_cfg[22] = (sc_analog_cfg[22] & ~0xc0) | (value<<6);
		break;
	case SC_MM_CENTER_Y_CORR:
		value &= 0x03;
		sc_analog_cfg[22] = (sc_analog_cfg[22] & ~0x30) | (value<<4);
		break;
	case SC_SS_CENTER_X_CORR:
		value &= 0x03;
		sc_analog_cfg[22] = (sc_analog_cfg[22] & ~0x0c) | (value<<2);
		break;
	case SC_SS_CENTER_Y_CORR:
		value &= 0x03;
		sc_analog_cfg[22] = (sc_analog_cfg[22] & ~0x03) | value;
		break;
	case SC_HH_CENTER_X:
		value &= 0xff;
		sc_analog_cfg[23] = value;
		break;
	case SC_HH_CENTER_Y:
		value &= 0xff;
		sc_analog_cfg[24] = value;
		break;
	case SC_MM_CENTER_X:
		value &= 0xff;
		sc_analog_cfg[25] = value;
		break;
	case SC_MM_CENTER_Y:
		value &= 0xff;
		sc_analog_cfg[26] = value;
		break;
	case SC_SS_CENTER_X:
		value &= 0xff;
		sc_analog_cfg[27] = value;
		break;
	case SC_SS_CENTER_Y:
		value &= 0xff;
		sc_analog_cfg[28] = value;
		break;
	case SC_HH_X_ST_MASK:
		value &= 0xff;
		sc_analog_cfg[29] = value;
		break;
	case SC_HH_Y_ST_MASK:
		value &= 0xff;
		sc_analog_cfg[30] = value;
		break;
	case SC_HH_X_ED_MASK:
		value &= 0xff;
		sc_analog_cfg[31] = value;
		break;
	case SC_HH_Y_ED_MASK:
		value &= 0xff;
		sc_analog_cfg[32] = value;
		break;
	case SC_MM_X_ST_MASK:
		value &= 0xff;
		sc_analog_cfg[33] = value;
		break;
	case SC_MM_Y_ST_MASK:
		value &= 0xff;
		sc_analog_cfg[34] = value;
		break;
	case SC_MM_X_ED_MASK:
		value &= 0xff;
		sc_analog_cfg[35] = value;
		break;
	case SC_MM_Y_ED_MASK:
		value &= 0xff;
		sc_analog_cfg[36] = value;
		break;
	case SC_SS_X_ST_MASK:
		value &= 0xff;
		sc_analog_cfg[37] = value;
		break;
	case SC_SS_Y_ST_MASK:
		value &= 0xff;
		sc_analog_cfg[38] = value;
		break;
	case SC_SS_X_ED_MASK:
		value &= 0xff;
		sc_analog_cfg[39] = value;
		break;
	case SC_SS_Y_ED_MASK:
		value &= 0xff;
		sc_analog_cfg[40] = value;
		break;
	case SC_HH_FLIP_EN:
		value &= 0x01;
		sc_analog_cfg[41] = (sc_analog_cfg[41] & ~0x04) | (value<<2);
		break;
	case SC_MM_FLIP_EN:
		value &= 0x01;
		sc_analog_cfg[41] = (sc_analog_cfg[41] & ~0x02) | (value<<1);
		break;
	case SC_SS_FLIP_EN:
		value &= 0x01;
		sc_analog_cfg[41] = (sc_analog_cfg[41] & ~0x01) | value;
		break;
	case SC_FLIP_ST:
		value &= 0x1ff;
		sc_analog_cfg[42] = (sc_analog_cfg[42] & ~0x01) | (value>>8);
		sc_analog_cfg[43] = (value & 0xff);
		break;
	case SC_FLIP_ED:
		value &= 0x1ff;
		sc_analog_cfg[44] = (sc_analog_cfg[44] & ~0x01) | (value>>8);
		sc_analog_cfg[45] = (value & 0xff);
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		return -1;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_anal_cfg_str[cmd], value);

	return 0;
}

int s6e36w3x01_set_digital_cfg(enum sc_digital_cfg_t cmd, int value)
{
	switch (cmd) {
	case SC_D_CLOCK_EN:
		value &= 0x01;
		sc_digital_cfg[1] = (sc_digital_cfg[1] & ~0x01) | value;
		break;
	case SC_24H_EN:
		value &= 0x03;
		sc_digital_cfg[1] = (sc_digital_cfg[1] & ~0x06) | (value<<1);
		break;
	case SC_D_MIN_LOCK_EN:
		value &= 0x01;
		sc_digital_cfg[1] = (sc_digital_cfg[1] & ~0x08) | (value<<3);
		break;
	case SC_D_EN_HH:
		value &= 0x03;
		value = !value;
		sc_digital_cfg[2] = (sc_digital_cfg[2] & ~0x30) | (value<<4);
		break;
	case SC_D_EN_MM:
		value &= 0x03;
		value = !value;
		sc_digital_cfg[2] = (sc_digital_cfg[2] & ~0x0c) | (value<<2);
		break;
	case SC_D_EN_SS:
		value &= 0x03;
		value = !value;
		sc_digital_cfg[2] = (sc_digital_cfg[2] & ~0x03) | value;
		break;
	case SC_D_EN_WD1:
		value &= 0x03;
		value = !value;
		sc_digital_cfg[3] = (sc_digital_cfg[3] & ~0x0c) | (value<<2);
		break;
	case SC_D_EN_WD2:
		value &= 0x03;
		value = !value;
		sc_digital_cfg[3] = (sc_digital_cfg[3] & ~0x03) | value;
		break;
	case SC_D_ALPHA_INV:
		value &= 0x01;
		sc_digital_cfg[3] = (sc_digital_cfg[3] & ~0x20) | (value<<5);
		break;
	case SELF_FONT_MODE:
		value &= 0x01;
		sc_digital_cfg[3] = (sc_digital_cfg[3] & ~0x10) | (value<<4);
		break;
	case SC_D_WD_DISP_OP:
		value &= 0x01;
		sc_digital_cfg[3] = (sc_digital_cfg[3] & ~0x40) | (value<<6);
		break;
	case SC_D_EN_HH_FONT:
		value &= 0x01;
		sc_digital_cfg[4] = (sc_digital_cfg[4] & ~0x04) | (value<<2);
		break;
	case SC_D_EN_MM_FONT:
		value &= 0x01;
		sc_digital_cfg[4] = (sc_digital_cfg[4] & ~0x02) | (value<<1);
		break;
	case SC_D_EN_SS_FONT:
		value &= 0x01;
		sc_digital_cfg[4] = (sc_digital_cfg[4] & ~0x01) | value;
		break;
	case SC_D_EN_WD1_FONT:
		value &= 0x01;
		sc_digital_cfg[4] = (sc_digital_cfg[4] & ~0x20) | (value<<5);
		break;
	case SC_D_EN_WD2_FONT:
		value &= 0x01;
		sc_digital_cfg[4] = (sc_digital_cfg[4] & ~0x10) | (value<<4);
		break;
	case SC_D_FONT0_IMG_WIDTH:
		value &= 0x1ff;
		sc_digital_cfg[5] = (sc_digital_cfg[5] & ~0x01) | (value>>8);
		sc_digital_cfg[6] = (value & 0xff);
		break;
	case SC_D_FONT0_IMG_HEIGHT:
		value &= 0x3ff;
		sc_digital_cfg[7] = (sc_digital_cfg[7] & ~0x03) | (value>>8);
		sc_digital_cfg[8] = (value & 0xff);
		break;
	case SC_D_FONT1_IMG_WIDTH:
		value &= 0x1ff;
		sc_digital_cfg[9] = (sc_digital_cfg[9] & ~0x01) | (value>>8);
		sc_digital_cfg[10] = (value & 0xff);
		break;
	case SC_D_FONT1_IMG_HEIGHT:
		value &= 0x3ff;
		sc_digital_cfg[11] = (sc_digital_cfg[11] & ~0x03) | (value>>8);
		sc_digital_cfg[12] = (value & 0xff);
		break;
	case SC_D_ST_HH_X10:
		value &= 0x1ff;
		sc_digital_cfg[13] = (sc_digital_cfg[13] & ~0x01) | (value>>8);
		sc_digital_cfg[14] = (value & 0xff);
		break;
	case SC_D_ST_HH_Y10:
		value &= 0x3ff;
		sc_digital_cfg[15] = (sc_digital_cfg[15] & ~0x03) | (value>>8);
		sc_digital_cfg[16] = (value & 0xff);
		break;
	case SC_D_ST_HH_X01:
		value &= 0x1ff;
		sc_digital_cfg[17] = (sc_digital_cfg[17] & ~0x01) | (value>>8);
		sc_digital_cfg[18] = (value & 0xff);
		break;
	case SC_D_ST_HH_Y01:
		value &= 0x3ff;
		sc_digital_cfg[19] = (sc_digital_cfg[19] & ~0x03) | (value>>8);
		sc_digital_cfg[20] = (value & 0xff);
		break;
	case SC_D_ST_MM_X10:
		value &= 0x1ff;
		sc_digital_cfg[21] = (sc_digital_cfg[21] & ~0x01) | (value>>8);
		sc_digital_cfg[22] = (value & 0xff);
		break;
	case SC_D_ST_MM_Y10:
		value &= 0x3ff;
		sc_digital_cfg[23] = (sc_digital_cfg[23] & ~0x03) | (value>>8);
		sc_digital_cfg[24] = (value & 0xff);
		break;
	case SC_D_ST_MM_X01:
		value &= 0x1ff;
		sc_digital_cfg[25] = (sc_digital_cfg[25] & ~0x01) | (value>>8);
		sc_digital_cfg[26] = (value & 0xff);
		break;
	case SC_D_ST_MM_Y01:
		value &= 0x3ff;
		sc_digital_cfg[27] = (sc_digital_cfg[27] & ~0x03) | (value>>8);
		sc_digital_cfg[28] = (value & 0xff);
		break;
	case SC_D_ST_SS_X10:
		value &= 0x1ff;
		sc_digital_cfg[29] = (sc_digital_cfg[29] & ~0x01) | (value>>8);
		sc_digital_cfg[30] = (value & 0xff);
		break;
	case SC_D_ST_SS_Y10:
		value &= 0x3ff;
		sc_digital_cfg[31] = (sc_digital_cfg[31] & ~0x03) | (value>>8);
		sc_digital_cfg[32] = (value & 0xff);
		break;
	case SC_D_ST_SS_X01:
		value &= 0x1ff;
		sc_digital_cfg[33] = (sc_digital_cfg[33] & ~0x01) | (value>>8);
		sc_digital_cfg[34] = (value & 0xff);
		break;
	case SC_D_ST_SS_Y01:
		value &= 0x3ff;
		sc_digital_cfg[35] = (sc_digital_cfg[35] & ~0x03) | (value>>8);
		sc_digital_cfg[36] = (value & 0xff);
		break;
	case SC_D_ST_WD1_HH_X10:
		value &= 0x1ff;
		sc_digital_cfg[37] = (sc_digital_cfg[37] & ~0x01) | (value>>8);
		sc_digital_cfg[38] = (value & 0xff);
		break;
	case SC_D_ST_WD1_HH_Y10:
		value &= 0x3ff;
		sc_digital_cfg[39] = (sc_digital_cfg[39] & ~0x03) | (value>>8);
		sc_digital_cfg[40] = (value & 0xff);
		break;
	case SC_D_ST_WD1_HH_X01:
		value &= 0x1ff;
		sc_digital_cfg[41] = (sc_digital_cfg[41] & ~0x01) | (value>>8);
		sc_digital_cfg[42] = (value & 0xff);
		break;
	case SC_D_ST_WD1_HH_Y01:
		value &= 0x3ff;
		sc_digital_cfg[43] = (sc_digital_cfg[43] & ~0x03) | (value>>8);
		sc_digital_cfg[44] = (value & 0xff);
		break;
	case SC_D_ST_WD1_MM_X10:
		value &= 0x1ff;
		sc_digital_cfg[45] = (sc_digital_cfg[45] & ~0x01) | (value>>8);
		sc_digital_cfg[46] = (value & 0xff);
		break;
	case SC_D_ST_WD1_MM_Y10:
		value &= 0x3ff;
		sc_digital_cfg[47] = (sc_digital_cfg[47] & ~0x03) | (value>>8);
		sc_digital_cfg[48] = (value & 0xff);
		break;
	case SC_D_ST_WD1_MM_X01:
		value &= 0x1ff;
		sc_digital_cfg[49] = (sc_digital_cfg[49] & ~0x01) | (value>>8);
		sc_digital_cfg[50] = (value & 0xff);
		break;
	case SC_D_ST_WD1_MM_Y01:
		value &= 0x3ff;
		sc_digital_cfg[51] = (sc_digital_cfg[51] & ~0x03) | (value>>8);
		sc_digital_cfg[52] = (value & 0xff);
		break;
	case SC_D_ST_WD2_HH_X10:
		value &= 0x1ff;
		sc_digital_cfg[53] = (sc_digital_cfg[53] & ~0x01) | (value>>8);
		sc_digital_cfg[54] = (value & 0xff);
		break;
	case SC_D_ST_WD2_HH_Y10:
		value &= 0x3ff;
		sc_digital_cfg[55] = (sc_digital_cfg[55] & ~0x03) | (value>>8);
		sc_digital_cfg[56] = (value & 0xff);
		break;
	case SC_D_ST_WD2_HH_X01:
		value &= 0x1ff;
		sc_digital_cfg[57] = (sc_digital_cfg[57] & ~0x01) | (value>>8);
		sc_digital_cfg[58] = (value & 0xff);
		break;
	case SC_D_ST_WD2_HH_Y01:
		value &= 0x3ff;
		sc_digital_cfg[59] = (sc_digital_cfg[59] & ~0x03) | (value>>8);
		sc_digital_cfg[60] = (value & 0xff);
		break;
	case SC_D_ST_WD2_MM_X10:
		value &= 0x1ff;
		sc_digital_cfg[61] = (sc_digital_cfg[61] & ~0x01) | (value>>8);
		sc_digital_cfg[62] = (value & 0xff);
		break;
	case SC_D_ST_WD2_MM_Y10:
		value &= 0x3ff;
		sc_digital_cfg[63] = (sc_digital_cfg[63] & ~0x03) | (value>>8);
		sc_digital_cfg[64] = (value & 0xff);
		break;
	case SC_D_ST_WD2_MM_X01:
		value &= 0x1ff;
		sc_digital_cfg[65] = (sc_digital_cfg[65] & ~0x01) | (value>>8);
		sc_digital_cfg[66] = (value & 0xff);
		break;
	case SC_D_ST_WD2_MM_Y01:
		value &= 0x3ff;
		sc_digital_cfg[67] = (sc_digital_cfg[67] & ~0x03) | (value>>8);
		sc_digital_cfg[68] = (value & 0xff);
		break;
	case SC_D_WD_END_LINE:
		value &= 0x3ff;
		sc_digital_cfg[69] = (sc_digital_cfg[69] & ~0x03) | (value>>8);
		sc_digital_cfg[70] = (value & 0xff);
		break;
	case SC_D_WD1_HH:
		if (value < 0)
			value *= -1;
		value &= 0x1f;
		sc_digital_cfg[71] = (sc_digital_cfg[71] & ~0x1f) | value;
		break;
	case SC_D_WD1_MM:
		if (value < 0)
			value *= -1;
		value &= 0x3f;
		sc_digital_cfg[72] = (sc_digital_cfg[72] & ~0x3f) | value;
		break;
	case SC_D_WD2_HH:
		if (value < 0)
			value *= -1;
		value &= 0x1f;
		sc_digital_cfg[73] = (sc_digital_cfg[73] & ~0x1f) | value;
		break;
	case SC_D_WD2_MM:
		if (value < 0)
			value *= -1;
		value &= 0x3f;
		sc_digital_cfg[74] = (sc_digital_cfg[74] & ~0x3f) | value;
		break;
	case SC_D_WD1_SIGN:
		value &= 0x01;
		sc_digital_cfg[75] = (sc_digital_cfg[75] & ~0x01) | value;
		break;
	case SC_D_WD2_SIGN:
		value &= 0x01;
		sc_digital_cfg[75] = (sc_digital_cfg[75] & ~0x02) | (value<<1);
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		return -1;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_digi_cfg_str[cmd], value);

	return 0;
}

static int s6e36w3x01_set_blink_cfg(enum sc_blink_cfg_t cmd, int value)
{
	switch (cmd) {
	case SB_ON_SEL:
		value &= 0x01;
		sc_blink_cfg[1] = (sc_blink_cfg[1] & ~0x01) | value;
		break;
	case SB_BLK0_EN:
		value &= 0x01;
		value = !value;
		sc_blink_cfg[2] = (sc_blink_cfg[2] & ~0x01) | value;
		break;
	case SB_BLK1_EN:
		value &= 0x01;
		value = !value;
		sc_blink_cfg[2] = (sc_blink_cfg[2] & ~0x02) | (value<<1);
		break;
	case SB_ALPHA_INV:
		value &= 0x01;
		sc_blink_cfg[2] = (sc_blink_cfg[2] & ~0x04) | (value<<2);
		break;
	case SB_BLINK_RATE:
		value &= 0xff;
		sc_blink_cfg[3] = value;
		break;
	case SB_BLINK0_FONT:
		value &= 0x01;
		sc_blink_cfg[2] = (sc_blink_cfg[2] & ~0x80) | (value<<7);
		break;
	case SB_BLINK1_FONT:
		value &= 0x01;
		sc_blink_cfg[2] = (sc_blink_cfg[2] & ~0x40) | (value<<6);
		break;
	case SB_BLINK_SYNC:
		value &= 0x03;
		sc_blink_cfg[2] = (sc_blink_cfg[2] & ~0x30) | (value<<4);
		break;
	case SB_ST_BLINK0_X:
		value &= 0x1ff;
		sc_blink_cfg[4] = (sc_blink_cfg[4] & ~0x01) | (value>>8);
		sc_blink_cfg[5] = (value & 0xff);
		break;
	case SB_ST_BLINK0_Y:
		value &= 0x3ff;
		sc_blink_cfg[6] = (sc_blink_cfg[6] & ~0x03) | (value>>8);
		sc_blink_cfg[7] = (value & 0xff);
		break;
	case SB_ST_BLINK1_X:
		value &= 0x1ff;
		sc_blink_cfg[8] = (sc_blink_cfg[8] & ~0x01) | (value>>8);
		sc_blink_cfg[9] = (value & 0xff);
		break;
	case SB_ST_BLINK1_Y:
		value &= 0x3ff;
		sc_blink_cfg[10] = (sc_blink_cfg[10] & ~0x03) | (value>>8);
		sc_blink_cfg[11] = (value & 0xff);
		break;
	case SB_FONT0_IMG_WIDTH:
		value &= 0x1ff;
		sc_blink_cfg[12] = (sc_blink_cfg[12] & ~0x01) | (value>>8);
		sc_blink_cfg[13] = (value & 0xff);
		break;
	case SB_FONT0_IMG_HEIGHT:
		value &= 0x3ff;
		sc_blink_cfg[14] = (sc_blink_cfg[14] & ~0x03) | (value>>8);
		sc_blink_cfg[15] = (value & 0xff);
		break;
	case SB_FONT1_IMG_WIDTH:
		value &= 0x1ff;
		sc_blink_cfg[16] = (sc_blink_cfg[16] & ~0x01) | (value>>8);
		sc_blink_cfg[17] = (value & 0xff);
		break;
	case SB_FONT1_IMG_HEIGHT:
		value &= 0x3ff;
		sc_blink_cfg[18] = (sc_blink_cfg[18] & ~0x03) | (value>>8);
		sc_blink_cfg[19] = (value & 0xff);
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		return -1;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_blink_cfg_str[cmd], value);

	return 0;
}

static int s6e36w3x01_set_afpc_cfg(enum sc_afpc_cfg_t cmd, int value)
{
	switch (cmd) {
	case SCP_ON_SEL:
		value &= 0x01;
		sc_afpc_cfg[1] = (sc_afpc_cfg[1] & ~0x01) | value;
		break;
	case SCP_COMPEN_EN:
		value &= 0x01;
		sc_afpc_cfg[2] = (sc_afpc_cfg[2] & ~0x01) | value;
		break;
	case SCP_MIN_RED:
		value &= 0xff;
		sc_afpc_cfg[3] = value;
		break;
	case SCP_MIN_GREEN:
		value &= 0xff;
		sc_afpc_cfg[4] = value;
		break;
	case SCP_MIN_BLUE:
		value &= 0xff;
		sc_afpc_cfg[5] = value;
		break;
	case SCP_BASE_RED:
		value &= 0xff;
		sc_afpc_cfg[6] = value;
		break;
	case SCP_BASE_GREEN:
		value &= 0xff;
		sc_afpc_cfg[7] = value;
		break;
	case SCP_BASE_BLUE:
		value &= 0xff;
		sc_afpc_cfg[8] = value;
		break;
	case SCP_BASE_OPT:
		value &= 0x03;
		sc_afpc_cfg[9] = (sc_afpc_cfg[9] & ~0x03) | value;
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		break;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_afpc_cfg_str[cmd], value);

	return 0;
}

static int s6e36w3x01_set_icon_cfg(enum sc_icon_cfg_t cmd, int value)
{
	switch (cmd) {
	case SI_ON_SEL:
		value &= 0x01;
		sc_icon_cfg[1] = (sc_icon_cfg[1] & ~0x01) | value;
		break;
	case SI_ICON_EN:
		value &= 0x01;
		value = !value;
		sc_icon_cfg[2] = (sc_icon_cfg[2] & ~0x01) | value;
		break;
	case SI_ALPHA_INV:
		value &= 0x01;
		sc_icon_cfg[2] = (sc_icon_cfg[2] & ~0x02) | (value<<1);
		break;
	case SI_ST_X:
		value &= 0x1ff;
		sc_icon_cfg[3] = (sc_icon_cfg[3] & ~0x01) | (value>>8);
		sc_icon_cfg[4] = (value & 0xff);
		break;
	case SI_ST_Y:
		value &= 0x3ff;
		sc_icon_cfg[5] = (sc_icon_cfg[5] & ~0x03) | (value>>8);
		sc_icon_cfg[6] = (value & 0xff);
		break;
	case SI_IMG_WIDTH:
		value &= 0x1ff;
		sc_icon_cfg[7] = (sc_icon_cfg[7] & ~0x01) | (value>>8);
		sc_icon_cfg[8] = (value & 0xff);
		break;
	case SI_IMG_HEIGHT:
		value &= 0x3ff;
		sc_icon_cfg[9] = (sc_icon_cfg[9] & ~0x03) | (value>>8);
		sc_icon_cfg[10] = (value & 0xff);
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		break;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_icon_cfg_str[cmd], value);

	return 0;
}

static int s6e36w3x01_set_move_cfg(enum sc_move_cfg_t cmd, int value)
{
	switch (cmd) {
	case MV_ON_SEL:
		value &= 0x01;
		sc_move_cfg[1] = (sc_move_cfg[1] & ~0x01) | value;
		break;
	case SCHEDULER_EN_I:
		value &= 0x01;
		value = !value;
		sc_move_cfg[2] = (sc_move_cfg[2] & ~0x01) | value;
		break;
	case SS_MOVE_ON:
		value &= 0x01;
		value = !value;
		sc_move_cfg[2] = (sc_move_cfg[2] & ~0x80) | (value<<7);
		break;
	case SC_MOVE_ON:
		value &= 0x01;
		value = !value;
		sc_move_cfg[2] = (sc_move_cfg[2] & ~0x40) | (value<<6);
		break;
	case SB_MOVE_ON:
		value &= 0x01;
		value = !value;
		sc_move_cfg[2] = (sc_move_cfg[2] & ~0x20) | (value<<5);
		break;
	case SI_MOVE_ON:
		value &= 0x01;
		value = !value;
		sc_move_cfg[2] = (sc_move_cfg[2] & ~0x10) | (value<<4);
		break;
	case SD_MOVE_ON:
		value &= 0x01;
		value = !value;
		sc_move_cfg[2] = (sc_move_cfg[2] & ~0x08) | (value<<3);
		break;
	case SE_MOVE_ON:
		value &= 0x01;
		value = !value;
		sc_move_cfg[2] = (sc_move_cfg[2] & ~0x04) | (value<<2);
		break;
	case DSP_X_ORG:
		value &= 0x1ff;
		sc_move_cfg[3] = (sc_move_cfg[3] & ~0x01) | (value>>8);
		sc_move_cfg[4] = (value & 0xff);
		break;
	case DSP_Y_ORG:
		value &= 0x3ff;
		sc_move_cfg[5] = (sc_move_cfg[5] & ~0x03) | (value>>8);
		sc_move_cfg[6] = (value & 0xff);
		break;
	case SC_MOVE_UPDATE_RATE:
		value &= 0x0f;
		sc_move_cfg[7] = (sc_move_cfg[7] & ~0x0f) | value;
		break;
	case SC_MAX_MOVING_EN:
		value &= 0x01;
		sc_move_cfg[7] = (sc_move_cfg[7] & ~0x10) | (value<<4);
		break;
	case SC_MAX_MOVING_POINT:
		value &= 0x3f;
		sc_move_cfg[8] = (sc_move_cfg[8] & ~0x3f) | value;
		break;
	default:
		pr_err("%s:unkown cmd[%d]\n", __func__, cmd);
		break;
	};

	pr_info("%s:[%s][%d]\n", __func__, sc_move_cfg_str[cmd], value);

	return 0;
}

static void s6e36w3x01_wait_vsync(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	struct decon_device *decon = get_decon_drvdata(dsim->id);
	ktime_t timestamp = decon->vsync.timestamp;
	int ret;

	pr_info("%s\n", __func__);

	ret = wait_event_interruptible_timeout(decon->vsync.wait,
				!ktime_equal(timestamp,
						decon->vsync.timestamp),
				msecs_to_jiffies(SC_1FRAME_MSEC));

	if (!ret)
		pr_err("%s:wait for vsync timeout\n", __func__);
}

int s6e36w3x01_sc_alloc(struct s6e36w3x01 *lcd)
{
	int ret = 0;

	lcd->sc_buf= devm_kzalloc(lcd->dev,
		sizeof(char) * SC_FONT_DATA_RAM_WIDTH * SC_FONT_DATA_HIGHT, GFP_KERNEL);
	if (!lcd->sc_buf) {
		pr_err("%s: failed to allocate sc_buf\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	lcd->afpc_buf= devm_kzalloc(lcd->dev,
		sizeof(char) * AFPC_RAM_WIDTH * AFPC_RAM_HIGHT, GFP_KERNEL);
	if (!lcd->afpc_buf) {
		pr_err("%s: failed to allocate afpc_buf\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	lcd->icon_buf= devm_kzalloc(lcd->dev,
		sizeof(char) * ICON_RAM_WIDTH * ICON_RAM_HIGHT, GFP_KERNEL);
	if (!lcd->icon_buf) {
		pr_err("%s: failed to allocate icon_buf\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

out:
	return ret;
}

extern void s6e36w3x01_testkey_enable(u32 id);
extern void s6e36w3x01_testkey_disable(u32 id);
extern void s6e36w3x01_testkey_enable_lv3(u32 id);
extern void s6e36w3x01_testkey_disable_lv3(u32 id);
void s6e36w3x01_write_blink_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);
	s6e36w3x01_testkey_enable_lv3(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_blink_cfg,
		ARRAY_SIZE(sc_blink_cfg)) < 0)
		dsim_err("failed to send sc_blink_cfg.\n");

	s6e36w3x01_testkey_disable_lv3(dsim->id);
	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_time_update_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);
	s6e36w3x01_testkey_enable_lv3(dsim->id);

	if(dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE_PARAM,
		sc_time_update_cfg[0], sc_time_update_cfg[1]) < 0)
		dsim_err("failed to send sc_time_update_cfg.\n");

	s6e36w3x01_testkey_disable_lv3(dsim->id);
	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_common_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);
	s6e36w3x01_testkey_enable_lv3(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_common_cfg,
		ARRAY_SIZE(sc_common_cfg)) < 0)
		dsim_err("failed to send sc_common_cfg.\n");

	s6e36w3x01_testkey_disable_lv3(dsim->id);
	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_icon_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_icon_cfg,
		ARRAY_SIZE(sc_icon_cfg)) < 0)
		dsim_err("failed to send sc_icon_cfg.\n");

	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_afpc_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_afpc_cfg,
		ARRAY_SIZE(sc_afpc_cfg)) < 0)
		dsim_err("failed to send sc_afpc_cfg.\n");

	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_analog_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_analog_cfg,
		ARRAY_SIZE(sc_analog_cfg)) < 0)
		dsim_err("failed to send sc_analog_cfg.\n");

	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_digital_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_digital_cfg,
		ARRAY_SIZE(sc_digital_cfg)) < 0)
		dsim_err("failed to send sc_digital_cfg.\n");

	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_move_cfg(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;

	s6e36w3x01_testkey_enable(dsim->id);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_move_cfg,
		ARRAY_SIZE(sc_move_cfg)) < 0)
		dsim_err("failed to send sc_move_cfg.\n");

	s6e36w3x01_testkey_disable(dsim->id);

	pr_info("%s\n", __func__);
}


void s6e36w3x01_read_dsi_err(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	char read_buff[2];

	s6e36w3x01_testkey_enable(dsim->id);

	if (dsim_rd_data(dsim->id, MIPI_DSI_DCS_READ,
		0xe5, 2, &read_buff[0]) < 0) {
		dsim_err("%s: failed to read 0x%x reg\n", __func__, 0xe5);
	}
	s6e36w3x01_testkey_disable(dsim->id);

	dsim_info("%s: 0xe5[0x%02x][0x%02x]\n", __func__, read_buff[0], read_buff[1]);
}

void s6e36w3x01_write_sideram_analog(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	int i;

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_err("%s: aod already enable[%d]\n",
			__func__, lcd->aod_mode);
		return;
	}

	s6e36w3x01_testkey_enable(dsim->id);

	s6e36w3x01_set_select_cfg(AC_ENC_BP);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_select_cfg,
		ARRAY_SIZE(sc_select_cfg)) < 0)
		dsim_err("failed to send sc_select_cfg.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_OFF, 0) < 0)
		dsim_err("failed to send TEAR_OFF.\n");

	for (i = 0; i < SC_HANDS_RAM_HIGHT; i++) {
		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) &lcd->sc_buf[i*SC_HANDS_RAM_WIDTH], SC_HANDS_RAM_WIDTH) < 0)
			dsim_err("failed to send selfclock_needle_data[%d]\n", i);
	}

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_ON, 0) < 0)
		dsim_err("failed to send TEAR_ON.\n");

	s6e36w3x01_testkey_disable(dsim->id);
	s6e36w3x01_wait_vsync(lcd);
	lcd->panel_sc_state = true;
	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_sideram_digital(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	int i;

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_err("%s: aod already enable[%d]\n",
			__func__, lcd->aod_mode);
		return;
	}

	s6e36w3x01_testkey_enable(dsim->id);

	s6e36w3x01_set_select_cfg(DC_SEL);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_select_cfg,
		ARRAY_SIZE(sc_select_cfg)) < 0)
		dsim_err("failed to send sc_select_cfg.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_OFF, 0) < 0)
		dsim_err("failed to send TEAR_OFF.\n");

	for (i = 0; i < SC_FONT_DATA_HIGHT; i++) {
		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) &lcd->sc_buf[i*SC_FONT_DATA_RAM_WIDTH],
				SC_FONT_DATA_RAM_WIDTH) < 0)
			dsim_err("failed to send selfclock_needle_data[%d]\n", i);
	}

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_ON, 0) < 0)
		dsim_err("failed to send TEAR_ON.\n");

	s6e36w3x01_testkey_disable(dsim->id);
	s6e36w3x01_wait_vsync(lcd);
	lcd->panel_sc_state = true;
	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_sideram_icon(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	int time_en, disp_on;
	int i;

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_err("%s: aod already enable[%d]\n",
			__func__, lcd->aod_mode);
		return;
	}

	time_en = s6e36w3x01_get_common_cfg(SC_TIME_EN);
	disp_on = s6e36w3x01_get_common_cfg(SC_DISP_ON);

	s6e36w3x01_set_common_cfg(SC_TIME_EN, true);
	s6e36w3x01_set_common_cfg(SC_DISP_ON, false);
	s6e36w3x01_write_common_cfg(lcd);
	s6e36w3x01_wait_vsync(lcd);
	s6e36w3x01_testkey_enable(dsim->id);

	s6e36w3x01_set_select_cfg(IC_SEL);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_select_cfg,
		ARRAY_SIZE(sc_select_cfg)) < 0)
		dsim_err("failed to send sc_select_cfg.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_OFF, 0) < 0)
		dsim_err("failed to send TEAR_OFF.\n");

	for (i = 0; i < ICON_RAM_HIGHT; i++) {
		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) &lcd->icon_buf[i*ICON_RAM_WIDTH],
				ICON_RAM_WIDTH) < 0)
			dsim_err("failed to send afpc data[%d]\n", i);
	}

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_ON, 0) < 0)
		dsim_err("failed to send TEAR_ON.\n");

	s6e36w3x01_testkey_disable(dsim->id);
	lcd->panel_icon_state = true;

	s6e36w3x01_set_common_cfg(SC_TIME_EN, time_en);
	s6e36w3x01_set_common_cfg(SC_DISP_ON, disp_on);
	s6e36w3x01_write_common_cfg(lcd);
	s6e36w3x01_wait_vsync(lcd);

	pr_info("%s\n", __func__);
}

void s6e36w3x01_write_sideram_afpc(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	int i;

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_err("%s: aod already enable[%d]\n",
			__func__, lcd->aod_mode);
		return;
	}

	s6e36w3x01_testkey_enable(dsim->id);

	s6e36w3x01_set_select_cfg(SCP_SEL);

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
		(unsigned long) sc_select_cfg,
		ARRAY_SIZE(sc_select_cfg)) < 0)
		dsim_err("failed to send sc_select_cfg.\n");

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_OFF, 0) < 0)
		dsim_err("failed to send TEAR_OFF.\n");

	for (i = 0; i < AFPC_RAM_HIGHT; i++) {
		if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_LONG_WRITE,
			(unsigned long) &lcd->afpc_buf[i*AFPC_RAM_WIDTH],
				AFPC_RAM_WIDTH) < 0)
			dsim_err("failed to send afpc data[%d]\n", i);
	}

	if (dsim_wr_data(dsim->id, MIPI_DSI_DCS_SHORT_WRITE,
		MIPI_DCS_SET_TEAR_ON, 0) < 0)
		dsim_err("failed to send TEAR_ON.\n");

	s6e36w3x01_testkey_disable(dsim->id);
	pr_info("%s\n", __func__);
}

void s6e36w3x01_restore_sideram_data(struct s6e36w3x01 *lcd, enum aod_mode watch_type)
{
	if (!lcd->panel_icon_state)
		s6e36w3x01_write_sideram_icon(lcd);

	switch(watch_type) {
	case AOD_SCLK_ANALOG:
		if (!lcd->panel_sc_state)
			s6e36w3x01_write_sideram_analog(lcd);
		break;
	case AOD_SCLK_DIGITAL:
		if (!lcd->panel_sc_state)
			s6e36w3x01_write_sideram_digital(lcd);
		break;
	default:
		break;
	}
}

static void s6e36w3x01_update_analog_sc_buf(struct s6e36w3x01 *lcd, char *source_addr[SC_HANDS_SECTION])
{
	int i, j, k;

	for (i = 0, k = 0; k < SC_HANDS_SECTION; k++) {
		for(j = 0; j < SC_HANDS_HIGHT; j++) {
			if (i == 0)
				lcd->sc_buf[i] = SC_START_CMD;
			else
				lcd->sc_buf[i] = SC_IMAGE_CMD;

			i += SC_CMD_SIZE;

			memcpy(&lcd->sc_buf[i],
				(char*)(source_addr[k]+(SC_HANDS_WIDTH*j)),
				SC_HANDS_WIDTH);

			i += SC_HANDS_WIDTH;
		}
	}
}

static void s6e36w3x01_update_digital_sc_buf(struct s6e36w3x01 *lcd,
						char *source_addr)
{
	int i, j;

	for (i = 0, j = 0; j < SC_FONT_DATA_HIGHT; j++) {
		if (i == 0)
			lcd->sc_buf[i] = SC_START_CMD;
		else
			lcd->sc_buf[i] = SC_IMAGE_CMD;

		i += SC_CMD_SIZE;

		memcpy((char*)&lcd->sc_buf[i],
			(char*)&source_addr[SC_FONT_DATA_WIDTH*j],
			SC_FONT_DATA_WIDTH);

		i += SC_FONT_DATA_WIDTH;
	}
}

static void s6e36w3x01_update_afpc_buf(struct s6e36w3x01 *lcd,
						char *source_addr)
{
	int i, j;

	for (i = 0, j = 0; j < AFPC_RAM_HIGHT; j++) {
		if (i == 0)
			lcd->afpc_buf[i] = SC_START_CMD;
		else
			lcd->afpc_buf[i] = SC_IMAGE_CMD;

		i += SC_CMD_SIZE;

		memcpy((char*)&lcd->afpc_buf[i],
			(char*)&source_addr[AFPC_DATA_WIDTH*j],
			AFPC_DATA_WIDTH);

		i += AFPC_DATA_WIDTH;
	}
}

static void s6e36w3x01_update_icon_buf(struct s6e36w3x01 *lcd,
						char *source_addr)
{
	int i, j;

	for (i = 0, j = 0; j < ICON_RAM_HIGHT; j++) {
		if (i == 0)
			lcd->icon_buf[i] = SC_START_CMD;
		else
			lcd->icon_buf[i] = SC_IMAGE_CMD;

		i += SC_CMD_SIZE;

		memcpy((char*)&lcd->icon_buf[i],
			(char*)&source_addr[ICON_DATA_WIDTH*j],
			ICON_DATA_WIDTH);

		i += ICON_DATA_WIDTH;
	}
}

void s6e36w3x01_update_sample_sc(struct s6e36w3x01 *lcd, enum sc_func_t func)
{
	//char *addr[SC_HANDS_SECTION];

	switch (func) {
	case SC_ANALOG:
		//addr[0] = (char*)&Analog_compressed_Sample[0];
		//addr[1] = (char*)&Analog_compressed_Sample[SC_HANDS_HIGHT*SC_HANDS_WIDTH];
		//addr[2] = (char*)&Analog_compressed_Sample[SC_HANDS_HIGHT*SC_HANDS_WIDTH*2];
		//s6e36w3x01_update_analog_sc_buf(lcd, (char**)&addr[0]);
		break;
	case SC_DIGITAL:
		//s6e36w3x01_update_digital_sc_buf(lcd, (char*)&Digital_Clock_Sample[0]);
		break;
	case SC_AFPC:
		s6e36w3x01_update_afpc_buf(lcd, (char*)&Afpc_Sample[0]);
		break;
	case SC_ICON:
		//s6e36w3x01_update_icon_buf(lcd, (char*)&Icon_Sample[0]);
		break;
	default:
		pr_err("%s: unknown func[%d]", __func__, func);
		break;
	}
}

static void s6e36w3x01_set_time(struct s6e36w3x01 *lcd)
{
	int diff_time, value, divider;
	int time = ktime_to_ms(ktime_get());
	diff_time = time - lcd->sc_time.diff;

	/* compensation by diff time */
	lcd->sc_time.sc_ms += (diff_time % 1000);
	lcd->sc_time.sc_s += (diff_time /1000);

	/* compensation by update rate */
	if (lcd->aod_mode == AOD_SCLK_ANALOG) {
		value = lcd->sc_time.sc_ms + ((1000/lcd->sc_rate)/2);
		divider = (SC_MAX_UPDATE_RATE / lcd->sc_rate) * SC_MSEC_DIVIDER;
		lcd->sc_time.sc_ms = (value /divider) * divider;
	}

	if (lcd->sc_time.sc_ms > 999) {
		lcd->sc_time.sc_s += (lcd->sc_time.sc_ms /1000);
		lcd->sc_time.sc_ms %= 1000;
	}

	if (lcd->sc_time.sc_s > 59) {
		lcd->sc_time.sc_m += (lcd->sc_time.sc_s/60);
		lcd->sc_time.sc_s %=60;
	}

	if (lcd->sc_time.sc_m > 59) {
		lcd->sc_time.sc_h += (lcd->sc_time.sc_m/60);
		lcd->sc_time.sc_m %=60;
	}

	if (lcd->sc_time.sc_h > 23)
		lcd->sc_time.sc_h = 0;

	pr_info("%s: %02d:%02d:%02d:%03d", __func__,
		lcd->sc_time.sc_h, lcd->sc_time.sc_m ,
		lcd->sc_time.sc_s, lcd->sc_time.sc_ms);

	s6e36w3x01_set_common_cfg(SC_SET_HH, lcd->sc_time.sc_h);
	s6e36w3x01_set_common_cfg(SC_SET_MM, lcd->sc_time.sc_m);
	s6e36w3x01_set_common_cfg(SC_SET_SS, lcd->sc_time.sc_s);
	s6e36w3x01_set_common_cfg(SC_SET_MSS, lcd->sc_time.sc_ms/SC_MSEC_DIVIDER);
}

extern int s6e36w3x01_read_mtp_reg(int id, u32 addr, char* buffer, u32 size);
int s6e36w3x01_get_panel_time(struct s6e36w3x01 *lcd, struct sc_time_st *panel_time)
{
	struct dsim_device *dsim = lcd->dsim;
	unsigned char read_buf[SC_TIME_REG_SIZE];
	int ret;

	ret = s6e36w3x01_read_mtp_reg(dsim->id, SC_TIME_REG, read_buf, SC_TIME_REG_SIZE);
	if (ret) {
		pr_err("%s: Failed to get SC_TIME_REG[%d]\n", __func__, ret);
		return -EIO;
	}

	panel_time->sc_h= read_buf[2] & 0x1f;
	panel_time->sc_m = read_buf[3] & 0x3f;
	panel_time->sc_s = read_buf[4] & 0x3f;
	panel_time->sc_ms = read_buf[5] & 0x0f;

	pr_info("%s:%02d:%02d:%02d.%d00\n", __func__,
		panel_time->sc_h, panel_time->sc_m,
		panel_time->sc_s, panel_time->sc_ms);

	return ret;
}

static void s6e36w3x01_set_time_comp(struct s6e36w3x01 *lcd)
{
	struct sc_time_st panel_time;
	unsigned int kernel_t;
	unsigned int panel_t;
	int ret, time_diff;

	kernel_t = (lcd->sc_time.sc_h*60*60*10) +
		       (lcd->sc_time.sc_m*60*10) +
		       (lcd->sc_time.sc_s *10) +
		       (lcd->sc_time.sc_ms/SC_MSEC_DIVIDER);

	if (lcd->sc_time.sc_h > 12)
		kernel_t -= (12*60*60*10);

	ret = s6e36w3x01_get_panel_time(lcd, &panel_time);
	if (ret) {
		pr_err("%s: Failed to get panel time[%d]\n", __func__, ret);
		s6e36w3x01_set_common_cfg(SC_COMP_EN, false);
		return;
	}

	panel_t = (panel_time.sc_h*60*60*10) +
		(panel_time.sc_m*60*10) +
		(panel_time.sc_s *10) +
		panel_time.sc_ms;

	time_diff = kernel_t - panel_t;

	if (time_diff < 0)
		time_diff *= -1;

	pr_info("%s: time_diff:%d.%d sec\n",
		__func__, time_diff/10, time_diff%10);

	s6e36w3x01_set_common_cfg(SC_COMP_STEP, SC_COMP_STEP_VALUE);

	if (time_diff < SC_COMP_NEED_TIME)
		s6e36w3x01_set_common_cfg(SC_COMP_EN, true);
	else
		s6e36w3x01_set_common_cfg(SC_COMP_EN, false);
}

int s6e36w3x01_set_aod_time(struct dsim_device *dsim, struct sclk_time_cfg_v2 *time_cfg)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	lcd->sc_time.diff = ktime_to_ms(ktime_get());
	lcd->sc_time.sc_h = time_cfg->hour;
	lcd->sc_time.sc_m = time_cfg->minute;
	lcd->sc_time.sc_s = time_cfg->second;
	lcd->sc_time.sc_ms = time_cfg->millisecond;

	pr_info("%s: %02d:%02d:%02d:%03d", __func__,
		lcd->sc_time.sc_h, lcd->sc_time.sc_m ,
		lcd->sc_time.sc_s, lcd->sc_time.sc_ms);

	/* time compensation routine in aod mode */
	if (lcd->aod_mode != AOD_DISABLE) {
		s6e36w3x01_set_time(lcd);
		s6e36w3x01_set_time_comp(lcd);
		s6e36w3x01_write_common_cfg(lcd);
		s6e36w3x01_write_time_update_cfg(lcd);
	}

	return 0;
}

#define 	SC_LAYER_MAX	2
#define 	SC_LAYER_MIN	0
static void s6e36w3x01_set_hands_layer(struct s6e36w3x01 *lcd, struct sclk_analog_cfg_v2 *analog_cfg)
{
	struct sclk_analog_hand *hour = &analog_cfg->hour;
	struct sclk_analog_hand *min = &analog_cfg->min;
	struct sclk_analog_hand *sec = &analog_cfg->sec;
	const char order_tbl[SC_LAYER_MAX+1][SC_LAYER_MAX+1] = {
		{0, 4, 1,},
		{5, 0, 0,},
		{2, 3, 0,}
	};

	if (hour->layer_order < SC_LAYER_MIN)
		hour->layer_order = SC_LAYER_MIN;
	if (hour->layer_order > SC_LAYER_MAX)
		hour->layer_order = SC_LAYER_MAX;
	if (min->layer_order < SC_LAYER_MIN)
		min->layer_order = SC_LAYER_MIN;
	if (min->layer_order > SC_LAYER_MAX)
		min->layer_order = SC_LAYER_MAX;

	s6e36w3x01_set_analog_cfg(SC_HMS_LAYER, order_tbl[min->layer_order][hour->layer_order]);

	pr_info("%s: hour:[%d], min:[%d], sec:[%d], layer:[%d]\n", __func__,
		hour->layer_order, min->layer_order, sec->layer_order,
		order_tbl[min->layer_order][hour->layer_order]);
}

static void s6e36w3x01_set_update_rate(struct s6e36w3x01 *lcd, unsigned int rate)
{
	int step, count;
	const char interval[4][2] = {{0, 3}, {1, 6}, {2, 15}, {3, 30}};

	if (rate >= SC_MAX_UPDATE_RATE) { /* 10hz*/
		lcd->sc_rate = SC_MAX_UPDATE_RATE;
		step = interval[0][0];
		count = interval[0][1];
	} else if (rate >= 5) {/* 5hz*/
		lcd->sc_rate = 5;
		step = interval[1][0];
		count = interval[1][1];
	} else if (rate >=2) {/* 2hz*/
		lcd->sc_rate = 2;
		step = interval[2][0];
		count = interval[2][1];
	} else {/* 1hz*/
		lcd->sc_rate = SC_MIN_UPDATE_RATE;
		step = interval[3][0];
		count = interval[3][1];
	}

	pr_info("%s:count[%d] step[%d] rate[%d]\n", __func__, count, step, rate);

	s6e36w3x01_set_common_cfg(SC_INC_STEP, step);
	s6e36w3x01_set_common_cfg(SC_UPDATE_RATE, count);
}

int s6e36w3x01_set_aod_analog (struct dsim_device *dsim, struct sclk_analog_cfg_v2 *analog_cfg)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);
	struct sclk_analog_hand *hour = &analog_cfg->hour;
	struct sclk_analog_hand *min = &analog_cfg->min;
	struct sclk_analog_hand *sec = &analog_cfg->sec;
	char* addr[SC_HANDS_SECTION];

	pr_info("%s:[%s]\n", __func__, aod_request_type_str[analog_cfg->req]);

	if (hour->layer_mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_analog_cfg(SC_HH_HAND_CENTER_X, hour->x);
		s6e36w3x01_set_analog_cfg(SC_HH_HAND_CENTER_Y, hour->y);
	}

	if (min->layer_mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_analog_cfg(SC_MM_HAND_CENTER_X, min->x);
		s6e36w3x01_set_analog_cfg(SC_MM_HAND_CENTER_Y, min->y);
	}

	if (sec->layer_mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_analog_cfg(SC_SS_HAND_CENTER_X, sec->x);
		s6e36w3x01_set_analog_cfg(SC_SS_HAND_CENTER_Y, sec->y);
	}

	if (analog_cfg->req == AOD_SET_CONFIG) {
		s6e36w3x01_set_analog_cfg(SC_HMS_HH_MASK, hour->layer_mask);
		s6e36w3x01_set_analog_cfg(SC_HMS_MM_MASK, min->layer_mask);
		s6e36w3x01_set_analog_cfg(SC_HMS_SS_MASK, sec->layer_mask);
		s6e36w3x01_set_analog_cfg(SC_FLIP_ST, analog_cfg->flip_start);
		s6e36w3x01_set_analog_cfg(SC_FLIP_ED, analog_cfg->flip_end);
		s6e36w3x01_set_update_rate(lcd, analog_cfg->rate);
		s6e36w3x01_set_hands_layer(lcd, analog_cfg);

		if (hour->layer_mask == SC_MASK_DISABLE) {
			s6e36w3x01_set_analog_cfg(SC_HH_X_ST_MASK, hour->mask_x);
			s6e36w3x01_set_analog_cfg(SC_HH_X_ED_MASK, hour->mask_x+hour->mask_w-1);
			s6e36w3x01_set_analog_cfg(SC_HH_Y_ST_MASK, hour->mask_y);
			s6e36w3x01_set_analog_cfg(SC_HH_Y_ED_MASK, hour->mask_y+hour->mask_h-1);
			s6e36w3x01_set_analog_cfg(SC_HH_MOTION, hour->motion);
			s6e36w3x01_set_analog_cfg(SC_HH_FLIP_EN, hour->flip_enable);
		}

		if (min->layer_mask == SC_MASK_DISABLE) {
			s6e36w3x01_set_analog_cfg(SC_MM_X_ST_MASK, min->mask_x);
			s6e36w3x01_set_analog_cfg(SC_MM_X_ED_MASK, min->mask_x+min->mask_w-1);
			s6e36w3x01_set_analog_cfg(SC_MM_Y_ST_MASK, min->mask_y);
			s6e36w3x01_set_analog_cfg(SC_MM_Y_ED_MASK, min->mask_y+min->mask_h-1);
			s6e36w3x01_set_analog_cfg(SC_MM_MOTION, min->motion);
			s6e36w3x01_set_analog_cfg(SC_MM_FLIP_EN, min->flip_enable);
		}

		if (sec->layer_mask == SC_MASK_DISABLE) {
			s6e36w3x01_set_analog_cfg(SC_SS_X_ST_MASK, sec->mask_x);
			s6e36w3x01_set_analog_cfg(SC_SS_X_ED_MASK, sec->mask_x+sec->mask_w-1);
			s6e36w3x01_set_analog_cfg(SC_SS_Y_ST_MASK, sec->mask_y);
			s6e36w3x01_set_analog_cfg(SC_SS_Y_ED_MASK, sec->mask_y+sec->mask_h-1);
			s6e36w3x01_set_analog_cfg(SC_SS_FLIP_EN, sec->flip_enable);
		}

		addr[0] = (char*)hour->addr;
		addr[1] = (char*)min->addr;
		addr[2] = (char*)sec->addr;
		s6e36w3x01_update_analog_sc_buf(lcd, (char**)&addr[0]);
		s6e36w3x01_write_sideram_analog(lcd);
	} else { /* AOD_UPDATE_DATA */
		if (lcd->aod_mode != AOD_DISABLE)
			s6e36w3x01_write_analog_cfg(lcd);
	}

	return 0;
}

static void s6e36w3x01_update_blink_cfg(struct s6e36w3x01 *lcd, struct sclk_digital_cfg_v2 *digital_cfg)
{
	struct sclk_digital_hand *blink1 = &digital_cfg->blink1;
	struct sclk_digital_hand *blink2 = &digital_cfg->blink2;
	struct sclk_digital_font *font1 = &digital_cfg->font1;
	struct sclk_digital_font *font2 = &digital_cfg->font2;
	const enum sc_blink_sync_t sync_trans_tbl[BLINK_MAX_SYNC_INDEX+1] = {
			BLINK_SYNC_RATE, BLINK_SYNC_0_5SEC, BLINK_SYNC_1SEC};

	s6e36w3x01_set_blink_cfg(SB_BLK0_EN, blink1->mask);
	s6e36w3x01_set_blink_cfg(SB_BLK1_EN, blink2->mask);

	if (blink1->mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_blink_cfg(SB_BLINK0_FONT, blink1->font_index);
		s6e36w3x01_set_blink_cfg(SB_ST_BLINK0_X, blink1->x[0]);
		s6e36w3x01_set_blink_cfg(SB_ST_BLINK0_Y, blink1->y[0]);
	}

	if (blink2->mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_blink_cfg(SB_BLINK1_FONT, blink2->font_index);
		s6e36w3x01_set_blink_cfg(SB_ST_BLINK1_X, blink2->x[0]);
		s6e36w3x01_set_blink_cfg(SB_ST_BLINK1_Y, blink2->y[0]);
	}

	if ((blink1->mask == SC_MASK_DISABLE) ||
	    (blink2->mask == SC_MASK_DISABLE)) {
		s6e36w3x01_set_common_cfg(SC_INC_STEP, BLINK_DEFAULT_STEP);
		s6e36w3x01_set_common_cfg(SC_UPDATE_RATE, BLINK_DEFAULT_RATE);
		s6e36w3x01_set_blink_cfg(SB_BLINK_RATE, digital_cfg->blink_rate);

		if ( digital_cfg->blink_sync > BLINK_MAX_SYNC_INDEX)
			digital_cfg->blink_sync = BLINK_MAX_SYNC_INDEX;
		s6e36w3x01_set_blink_cfg(SB_BLINK_SYNC, sync_trans_tbl[digital_cfg->blink_sync]);
	}

	if (digital_cfg->req == AOD_SET_CONFIG) {
		s6e36w3x01_set_blink_cfg(SB_FONT0_IMG_WIDTH, font1->colon_w);
		s6e36w3x01_set_blink_cfg(SB_FONT0_IMG_HEIGHT, font1->colon_h);
		s6e36w3x01_set_blink_cfg(SB_FONT1_IMG_WIDTH, font2->colon_w);
		s6e36w3x01_set_blink_cfg(SB_FONT1_IMG_HEIGHT, font2->colon_h);
	}
}


static void s6e36w3x01_update_wdigital_cfg(struct s6e36w3x01 *lcd, struct sclk_digital_cfg_v2 *digital_cfg)
{
	struct sclk_digital_hand *wclk1_hour = &digital_cfg->world_clock1_hour;
	struct sclk_digital_hand *wclk1_min = &digital_cfg->world_clock1_minute;
	struct sclk_digital_hand *wclk2_hour = &digital_cfg->world_clock2_hour;
	struct sclk_digital_hand *wclk2_min = &digital_cfg->world_clock2_minute;

	s6e36w3x01_set_digital_cfg(SC_D_EN_WD1, digital_cfg->world_clock1_mask);
	s6e36w3x01_set_digital_cfg(SC_D_EN_WD2, digital_cfg->world_clock2_mask);

	if (digital_cfg->world_clock1_mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_HH_X10, wclk1_hour->x[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_HH_Y10, wclk1_hour->y[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_HH_X01, wclk1_hour->x[1]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_HH_Y01, wclk1_hour->y[1]);

		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_MM_X10, wclk1_min->x[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_MM_Y10, wclk1_min->y[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_MM_X01, wclk1_min->x[1]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD1_MM_Y01, wclk1_min->y[1]);

		s6e36w3x01_set_digital_cfg(SC_D_EN_WD1_FONT, wclk1_hour->font_index);
		s6e36w3x01_set_digital_cfg(SC_D_WD1_HH, wclk1_hour->time_diff);
		s6e36w3x01_set_digital_cfg(SC_D_WD1_MM, wclk1_min->time_diff);

		if (wclk1_hour->time_diff < 0 || wclk1_min->time_diff < 0)
			s6e36w3x01_set_digital_cfg(SC_D_WD1_SIGN, WCLK_M_OFFSET);
		else
			s6e36w3x01_set_digital_cfg(SC_D_WD1_SIGN, WCLK_P_OFFSET);
	}

	if (digital_cfg->world_clock2_mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_HH_X10, wclk2_hour->x[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_HH_Y10, wclk2_hour->y[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_HH_X01, wclk2_hour->x[1]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_HH_Y01, wclk2_hour->y[1]);

		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_MM_X10, wclk2_min->x[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_MM_Y10, wclk2_min->y[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_MM_X01, wclk2_min->x[1]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_WD2_MM_Y01, wclk2_min->y[1]);

		s6e36w3x01_set_digital_cfg(SC_D_EN_WD2_FONT, wclk2_hour->font_index);
		s6e36w3x01_set_digital_cfg(SC_D_WD2_HH, wclk2_hour->time_diff);
		s6e36w3x01_set_digital_cfg(SC_D_WD2_MM, wclk2_min->time_diff);

		if (wclk2_hour->time_diff < 0 || wclk2_min->time_diff < 0)
			s6e36w3x01_set_digital_cfg(SC_D_WD2_SIGN, WCLK_M_OFFSET);
		else
			s6e36w3x01_set_digital_cfg(SC_D_WD2_SIGN, WCLK_P_OFFSET);
	}

	if ((digital_cfg->world_clock1_mask == SC_MASK_DISABLE) ||
	    (digital_cfg->world_clock2_mask == SC_MASK_DISABLE)) {
		s6e36w3x01_set_digital_cfg(SC_D_WD_DISP_OP, digital_cfg->world_clock_position);
		s6e36w3x01_set_digital_cfg(SC_D_WD_END_LINE, digital_cfg->world_clock_y);
	}
}

static void s6e36w3x01_update_digital_cfg(struct s6e36w3x01 *lcd, struct sclk_digital_cfg_v2 *digital_cfg)
{
	struct sclk_digital_hand *hour = &digital_cfg->hour;
	struct sclk_digital_hand *min = &digital_cfg->min;
	struct sclk_digital_hand *sec = &digital_cfg->sec;
	struct sclk_digital_font *font1 = &digital_cfg->font1;
	struct sclk_digital_font *font2 = &digital_cfg->font2;

	s6e36w3x01_set_digital_cfg(SC_D_EN_HH, hour->mask);
	s6e36w3x01_set_digital_cfg(SC_D_EN_MM, min->mask);
	s6e36w3x01_set_digital_cfg(SC_D_EN_SS, sec->mask);

	if (hour->mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_digital_cfg(SC_D_EN_HH_FONT, hour->font_index);
		s6e36w3x01_set_digital_cfg(SC_D_ST_HH_X10, hour->x[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_HH_Y10, hour->y[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_HH_X01, hour->x[1]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_HH_Y01, hour->y[1]);
	}

	if (min->mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_digital_cfg(SC_D_EN_MM_FONT, min->font_index);
		s6e36w3x01_set_digital_cfg(SC_D_ST_MM_X10, min->x[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_MM_Y10, min->y[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_MM_X01, min->x[1]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_MM_Y01, min->y[1]);
	}

	if (sec->mask == SC_MASK_DISABLE) {
		s6e36w3x01_set_digital_cfg(SC_D_EN_SS_FONT, sec->font_index);
		s6e36w3x01_set_digital_cfg(SC_D_ST_SS_X10, sec->x[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_SS_Y10, sec->y[0]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_SS_X01, sec->x[1]);
		s6e36w3x01_set_digital_cfg(SC_D_ST_SS_Y01, sec->y[1]);
	}

	if (digital_cfg->req == AOD_SET_CONFIG) {
		s6e36w3x01_set_digital_cfg(SELF_FONT_MODE, digital_cfg->font_type);
		s6e36w3x01_set_digital_cfg(SC_24H_EN, digital_cfg->font_format);
		s6e36w3x01_set_digital_cfg(SC_D_FONT0_IMG_WIDTH, font1->digit_w);
		s6e36w3x01_set_digital_cfg(SC_D_FONT0_IMG_HEIGHT, font1->gitit_h);
		s6e36w3x01_set_digital_cfg(SC_D_FONT1_IMG_WIDTH, font2->digit_w);
		s6e36w3x01_set_digital_cfg(SC_D_FONT1_IMG_HEIGHT, font2->gitit_h);

	}
}

int s6e36w3x01_set_aod_digital (struct dsim_device *dsim, struct sclk_digital_cfg_v2 *digital_cfg)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s:[%s]\n", __func__, aod_request_type_str[digital_cfg->req]);

	s6e36w3x01_update_digital_cfg(lcd, digital_cfg);
	s6e36w3x01_update_wdigital_cfg(lcd, digital_cfg);
	s6e36w3x01_update_blink_cfg(lcd, digital_cfg);

	switch (digital_cfg->req) {
	case AOD_SET_CONFIG:
		s6e36w3x01_update_digital_sc_buf(lcd, (char*)digital_cfg->font_addr);
		s6e36w3x01_write_sideram_digital(lcd);
		break;
	case AOD_UPDATE_DATA:
		if (lcd->aod_mode != AOD_DISABLE) {
			s6e36w3x01_write_digital_cfg(lcd);
			s6e36w3x01_write_blink_cfg(lcd);
		}
		break;
	default:
		pr_err("%s: unkown request[%d]\n", __func__, digital_cfg->req);
		break;
	};

	return 0;
}

int s6e36w3x01_set_aod_icon (struct dsim_device *dsim, struct sclk_icon_cfg_v2 *icon_cfg)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s:[%s]\n", __func__, aod_data_cmd_str[icon_cfg->req]);

	switch (icon_cfg->req) {
	case AOD_DATA_ENABLE:
		s6e36w3x01_set_icon_cfg(SI_ICON_EN, icon_cfg->mask);
		if (lcd->aod_mode != AOD_DISABLE)
			s6e36w3x01_write_icon_cfg(lcd);
		break;
	case AOD_DATA_SET:
		s6e36w3x01_set_icon_cfg(SI_IMG_WIDTH, icon_cfg->w);
		s6e36w3x01_set_icon_cfg(SI_IMG_HEIGHT, icon_cfg->h);
		s6e36w3x01_update_icon_buf(lcd, (char*)icon_cfg->addr);
		s6e36w3x01_write_sideram_icon(lcd);
		break;
	case AOD_DATA_POS:
		s6e36w3x01_set_icon_cfg(SI_ST_X, icon_cfg->x);
		s6e36w3x01_set_icon_cfg(SI_ST_Y, icon_cfg->y);
		if (lcd->aod_mode != AOD_DISABLE)
			s6e36w3x01_write_icon_cfg(lcd);
		break;
	default:
		pr_err("%s: unkown request[%d]\n", __func__, icon_cfg->req);
		break;
	};

	return 0;
}

int s6e36w3x01_set_aod_move (struct dsim_device *dsim, struct sclk_move_cfg_v2 *move_cfg)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s:[%s]\n", __func__, aod_mask_str[move_cfg->req]);

	s6e36w3x01_set_move_cfg(SCHEDULER_EN_I, move_cfg->mask);
	s6e36w3x01_set_move_cfg(SC_MOVE_ON, move_cfg->clock_mask);
	s6e36w3x01_set_move_cfg(SB_MOVE_ON, move_cfg->blink_mask);
	s6e36w3x01_set_move_cfg(SI_MOVE_ON, move_cfg->icon_mask);
	s6e36w3x01_set_move_cfg(DSP_X_ORG, move_cfg->x);
	s6e36w3x01_set_move_cfg(DSP_Y_ORG, move_cfg->y);

	s6e36w3x01_write_move_cfg(lcd);

	return 0;
}

extern void s6e36w3x01_mdnie_restore(struct s6e36w3x01 *lcd, bool aod_state);
extern void s6e36w3x01_hlpm_ctrl(struct s6e36w3x01 *lcd, bool enable);
int s6e36w3x01_aod_enter(struct dsim_device *dsim, enum aod_mode watch_type)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_err("%s: aod already enable[%d]\n",
			__func__, lcd->aod_mode);
		return -EIO;
	}

	pr_info("%s:[%s]\n", __func__, aod_mode_str[watch_type]);

	s6e36w3x01_restore_sideram_data(lcd, watch_type);

	switch(watch_type) {
	case AOD_SCLK_ANALOG:
		s6e36w3x01_set_common_cfg(SC_TIME_EN, true);
		s6e36w3x01_set_common_cfg(SC_DISP_ON, true);
		s6e36w3x01_set_analog_cfg(SC_ANA_CLOCK_EN, true);
		s6e36w3x01_mdnie_restore(lcd, true);
		s6e36w3x01_set_time(lcd);
		s6e36w3x01_hlpm_ctrl(lcd, true);
		s6e36w3x01_write_analog_cfg(lcd);
		s6e36w3x01_write_common_cfg(lcd);
		break;
	case AOD_SCLK_DIGITAL:
		s6e36w3x01_set_common_cfg(SC_TIME_EN, true);
		s6e36w3x01_set_common_cfg(SC_DISP_ON, true);
		s6e36w3x01_set_digital_cfg(SC_D_CLOCK_EN, true);
		s6e36w3x01_mdnie_restore(lcd, true);
		s6e36w3x01_set_time(lcd);
		s6e36w3x01_hlpm_ctrl(lcd, true);
		s6e36w3x01_write_blink_cfg(lcd);
		s6e36w3x01_write_digital_cfg(lcd);
		s6e36w3x01_write_common_cfg(lcd);
		break;
	case AOD_HLPM:
	case AOD_ALPM:
		s6e36w3x01_mdnie_restore(lcd, true);
		s6e36w3x01_hlpm_ctrl(lcd, true);
		break;
	default:
		pr_err("%s: watch_type[%d]\n", __func__, watch_type);
		return -EIO;
	}

	lcd->aod_mode = watch_type;
	lcd->aod_stime = ktime_get_boottime();
	lcd->prev_hlpm_nit = lcd->hlpm_nit;

	return 0;
}

int s6e36w3x01_aod_exit(struct dsim_device *dsim, enum aod_mode watch_type)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);
	ktime_t aod_time;
	unsigned int diff_time;

	if (lcd->aod_mode == AOD_DISABLE) {
		pr_err("%s: aod already disable[%d]\n",
			__func__, lcd->aod_mode);
		return -EIO;
	}

	pr_info("%s:[%s]\n", __func__, aod_mode_str[watch_type]);

	switch(watch_type) {
	case AOD_SCLK_ANALOG:
		s6e36w3x01_set_common_cfg(SC_TIME_EN, false);
		s6e36w3x01_set_common_cfg(SC_DISP_ON, false);
		s6e36w3x01_set_analog_cfg(SC_ANA_CLOCK_EN, false);
		s6e36w3x01_set_icon_cfg(SI_ICON_EN, SC_MASK_ENABLE);
		s6e36w3x01_set_move_cfg(SCHEDULER_EN_I, SC_MASK_ENABLE);
		s6e36w3x01_write_move_cfg(lcd);
		s6e36w3x01_write_icon_cfg(lcd);
		s6e36w3x01_write_analog_cfg(lcd);
		s6e36w3x01_write_common_cfg(lcd);
		s6e36w3x01_hlpm_ctrl(lcd, false);
		s6e36w3x01_mdnie_restore(lcd, false);
		break;
	case AOD_SCLK_DIGITAL:
		s6e36w3x01_set_common_cfg(SC_TIME_EN, false);
		s6e36w3x01_set_common_cfg(SC_DISP_ON, false);
		s6e36w3x01_set_digital_cfg(SC_D_CLOCK_EN, false);
		s6e36w3x01_set_blink_cfg(SB_BLK0_EN, SC_MASK_ENABLE);
		s6e36w3x01_set_blink_cfg(SB_BLK1_EN, SC_MASK_ENABLE);
		s6e36w3x01_set_icon_cfg(SI_ICON_EN, SC_MASK_ENABLE);
		s6e36w3x01_set_move_cfg(SCHEDULER_EN_I, SC_MASK_ENABLE);
		s6e36w3x01_write_move_cfg(lcd);
		s6e36w3x01_write_icon_cfg(lcd);
		s6e36w3x01_write_blink_cfg(lcd);
		s6e36w3x01_write_digital_cfg(lcd);
		s6e36w3x01_write_common_cfg(lcd);
		s6e36w3x01_hlpm_ctrl(lcd, false);
		s6e36w3x01_mdnie_restore(lcd, false);
		break;
	case AOD_HLPM:
	case AOD_ALPM:
		s6e36w3x01_set_icon_cfg(SI_ICON_EN, SC_MASK_ENABLE);
		s6e36w3x01_set_move_cfg(SCHEDULER_EN_I, SC_MASK_ENABLE);
		s6e36w3x01_write_move_cfg(lcd);
		s6e36w3x01_write_icon_cfg(lcd);
		s6e36w3x01_hlpm_ctrl(lcd, false);
		s6e36w3x01_mdnie_restore(lcd, false);
		break;
	default:
		pr_err("%s: watch_type[%d]\n", __func__, watch_type);
		return -EIO;
	}

	lcd->aod_mode = AOD_DISABLE;
	aod_time = ktime_get_boottime();
	diff_time = ((unsigned int)ktime_ms_delta(aod_time, lcd->aod_stime) / 1000);
	lcd->aod_total_time += diff_time;
	if (lcd->prev_hlpm_nit == HLPM_NIT_HIGH)
		lcd->aod_high_time += diff_time;
	else if (lcd->prev_hlpm_nit == HLPM_NIT_LOW)
		lcd->aod_low_time += diff_time;

	return 0;
}


int s6e36w3x01_afpc_set_compensation(struct dsim_device *dsim, struct afpc_compensation_v2 *comp)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	s6e36w3x01_update_afpc_buf(lcd, (char*)comp->addr);

	return 0;
}

ssize_t s6e36w3x01_sc_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int func, enable;

	sscanf(buf, "%d %d", &func, &enable);
	pr_info("%s:func:[%d] enable:[%d]\n", __func__, func, enable);

	switch (func) {
	case SC_ANALOG:
		if (1 == enable) {
			s6e36w3x01_set_common_cfg(SC_TIME_EN, true);
			s6e36w3x01_set_common_cfg(SC_DISP_ON, true);
			s6e36w3x01_set_analog_cfg(SC_ANA_CLOCK_EN, true);
			s6e36w3x01_write_analog_cfg(lcd);
			s6e36w3x01_write_common_cfg(lcd);
		} else if (0 == enable){
			s6e36w3x01_set_common_cfg(SC_TIME_EN, false);
			s6e36w3x01_set_common_cfg(SC_DISP_ON, false);
			s6e36w3x01_set_analog_cfg(SC_ANA_CLOCK_EN, false);
			s6e36w3x01_write_analog_cfg(lcd);
			s6e36w3x01_write_common_cfg(lcd);
		} else {
			s6e36w3x01_update_sample_sc(lcd, SC_ANALOG);
			s6e36w3x01_write_sideram_analog(lcd);
		}
		break;
	case SC_DIGITAL:
		if (1 == enable) {
			s6e36w3x01_set_common_cfg(SC_TIME_EN, true);
			s6e36w3x01_set_common_cfg(SC_DISP_ON, true);
			s6e36w3x01_set_digital_cfg(SC_D_CLOCK_EN, true);
			s6e36w3x01_write_digital_cfg(lcd);
			s6e36w3x01_write_common_cfg(lcd);
		} else if (0 == enable){
			s6e36w3x01_set_common_cfg(SC_TIME_EN, false);
			s6e36w3x01_set_common_cfg(SC_DISP_ON, false);
			s6e36w3x01_set_digital_cfg(SC_D_CLOCK_EN, false);
			s6e36w3x01_write_digital_cfg(lcd);
			s6e36w3x01_write_common_cfg(lcd);
		} else {
			s6e36w3x01_update_sample_sc(lcd, SC_DIGITAL);
			s6e36w3x01_write_sideram_digital(lcd);
		}
		break;
	case SC_AFPC:
		if (1 == enable) {
			s6e36w3x01_set_common_cfg(SCP_ON_SEL, true);
			s6e36w3x01_set_afpc_cfg(SCP_COMPEN_EN, true);
			s6e36w3x01_write_afpc_cfg(lcd);
		} else if (0 == enable){
			s6e36w3x01_set_afpc_cfg(SCP_COMPEN_EN, false);
			s6e36w3x01_write_afpc_cfg(lcd);
		} else {
			s6e36w3x01_update_sample_sc(lcd, SC_AFPC);
			s6e36w3x01_write_sideram_afpc(lcd);
		}
		break;
	case SC_ICON:
		if (1 == enable) {
			s6e36w3x01_set_common_cfg(SC_DISP_ON, true);
			s6e36w3x01_set_icon_cfg(SI_ICON_EN, SC_MASK_DISABLE);
			s6e36w3x01_write_icon_cfg(lcd);
			s6e36w3x01_write_common_cfg(lcd);
		} else if (0 == enable){
			s6e36w3x01_set_common_cfg(SC_DISP_ON, false);
			s6e36w3x01_set_icon_cfg(SI_ICON_EN, SC_MASK_ENABLE);
			s6e36w3x01_write_icon_cfg(lcd);
			s6e36w3x01_write_common_cfg(lcd);
		} else {
			s6e36w3x01_update_sample_sc(lcd, SC_ICON);
			s6e36w3x01_write_sideram_icon(lcd);
		}
		break;
	case SC_MOVE:
		if (1 == enable) {
			s6e36w3x01_set_move_cfg(SCHEDULER_EN_I, false);
			s6e36w3x01_set_move_cfg(DSP_X_ORG, -5);
			s6e36w3x01_set_move_cfg(DSP_Y_ORG, -5);
			s6e36w3x01_write_move_cfg(lcd);
		} else if (0 == enable){
			s6e36w3x01_set_move_cfg(SCHEDULER_EN_I, true);
			s6e36w3x01_write_move_cfg(lcd);
		}
		break;
	default:
		dev_err(dev, "invalid command.\n");
	}

	return size;
}

ssize_t s6e36w3x01_sc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	char buffer[128] = {0, };
	int i, ret = 0;

	for ( i = 0; i < ARRAY_SIZE(sc_select_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_select_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_select_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	for ( i = 0; i < ARRAY_SIZE(sc_common_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_common_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_common_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	for ( i = 0; i < ARRAY_SIZE(sc_analog_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_analog_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_analog_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	for ( i = 0; i < ARRAY_SIZE(sc_digital_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_digital_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_digital_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	for ( i = 0; i < ARRAY_SIZE(sc_blink_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_blink_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_blink_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	for ( i = 0; i < ARRAY_SIZE(sc_icon_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_icon_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_icon_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	for ( i = 0; i < ARRAY_SIZE(sc_move_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_move_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_move_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	for ( i = 0; i < ARRAY_SIZE(sc_afpc_cfg); i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", sc_afpc_cfg[i]);
		strcat(buf, buffer);

		if ( i < (ARRAY_SIZE(sc_afpc_cfg)-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}
	strcat(buf, "\n\n");
	ret += strlen("\n\n");

	return ret;
}
