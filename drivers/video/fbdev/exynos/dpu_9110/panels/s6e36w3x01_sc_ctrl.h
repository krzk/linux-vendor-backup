/* drivers/video/fbdev/exynos/dpu_9110/panels/s6e36w3x01_sc_ctrl.h
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
#ifndef __S6E36W3X01_SC_CTRL_H__
#define __S6E36W3X01_SC_CTRL_H__

#include "../dsim.h"

#define	SC_SELECT_CFG_SIZE	3
#define	SC_COMMON_CFG_SIZE	14
#define	SC_ANLALOG_CFG_SIZE	46
#define	SC_DIGITAL_CFG_SIZE	76
#define	SC_BLINK_CFG_SIZE	20
#define	SC_AFPC_CFG_SIZE	10
#define	SC_ICON_CFG_SIZE	11
#define	SC_MOVE_CFG_SIZE	68
#define	SC_TIME_UPDATE_CFG_SIZE	2

enum sc_mask_state_t {
	SC_MASK_DISABLE,
	SC_MASK_ENABLE,
};

enum sc_func_t {
	SC_ANALOG,
	SC_DIGITAL,
	SC_AFPC,
	SC_ICON,
	SC_MOVE
};

enum sc_select_cfg_t {
	AC_SEL,
	DC_SEL,
	IC_SEL,
	SS_SEL,
	MA_SEL,
	SCP_SEL,
	AC_ENC_BP,
};

enum sc_common_cfg_t {
	SC_TIME_STOP_EN,
	SC_TIME_EN,
	SC_DISP_ON,
	SC_SET_HH,
	SC_SET_MM,
	SC_SET_SS,
	SC_SET_MSS,
	SC_UPDATE_RATE,
	SC_COMP_STEP,
	SC_COMP_EN,
	SC_INC_STEP,
	SELF_HBP,
	SELF_VBP,
	DISP_CONTENT_X,
	DISP_CONTENT_Y,
};

enum sc_digital_cfg_t {
	SC_D_CLOCK_EN,
	SC_24H_EN,
	SC_D_MIN_LOCK_EN,
	SC_D_EN_HH,
	SC_D_EN_MM,
	SC_D_EN_SS,
	SC_D_EN_WD1,
	SC_D_EN_WD2,
	SC_D_ALPHA_INV,
	SELF_FONT_MODE,
	SC_D_WD_DISP_OP,
	SC_D_EN_HH_FONT,
	SC_D_EN_MM_FONT,
	SC_D_EN_SS_FONT,
	SC_D_EN_WD1_FONT,
	SC_D_EN_WD2_FONT,
	SC_D_FONT0_IMG_WIDTH,
	SC_D_FONT0_IMG_HEIGHT,
	SC_D_FONT1_IMG_WIDTH,
	SC_D_FONT1_IMG_HEIGHT,
	SC_D_ST_HH_X10,
	SC_D_ST_HH_Y10,
	SC_D_ST_HH_X01,
	SC_D_ST_HH_Y01,
	SC_D_ST_MM_X10,
	SC_D_ST_MM_Y10,
	SC_D_ST_MM_X01,
	SC_D_ST_MM_Y01,
	SC_D_ST_SS_X10,
	SC_D_ST_SS_Y10,
	SC_D_ST_SS_X01,
	SC_D_ST_SS_Y01,
	SC_D_ST_WD1_HH_X10,
	SC_D_ST_WD1_HH_Y10,
	SC_D_ST_WD1_HH_X01,
	SC_D_ST_WD1_HH_Y01,
	SC_D_ST_WD1_MM_X10,
	SC_D_ST_WD1_MM_Y10,
	SC_D_ST_WD1_MM_X01,
	SC_D_ST_WD1_MM_Y01,
	SC_D_ST_WD2_HH_X10,
	SC_D_ST_WD2_HH_Y10,
	SC_D_ST_WD2_HH_X01,
	SC_D_ST_WD2_HH_Y01,
	SC_D_ST_WD2_MM_X10,
	SC_D_ST_WD2_MM_Y10,
	SC_D_ST_WD2_MM_X01,
	SC_D_ST_WD2_MM_Y01,
	SC_D_WD_END_LINE,
	SC_D_WD1_HH,
	SC_D_WD1_MM,
	SC_D_WD2_HH,
	SC_D_WD2_MM,
	SC_D_WD1_SIGN,
	SC_D_WD2_SIGN,
};

enum sc_analog_cfg_t {
	SC_ON_SEL,
	SC_ANA_CLOCK_EN,
	SC_HH_MOTION,
	SC_MM_MOTION,
	SC_HH_FIX,
	SC_MM_FIX,
	SC_SS_FIX,
	SC_SET_FIX_HH,
	SC_SET_FIX_MM,
	SC_SET_FIX_SS,
	SC_SET_FIX_MSS,
	SC_HH_HAND_CENTER_X,
	SC_HH_HAND_CENTER_Y,
	SC_MM_HAND_CENTER_X,
	SC_MM_HAND_CENTER_Y,
	SC_SS_HAND_CENTER_X,
	SC_SS_HAND_CENTER_Y,
	SC_HMS_LAYER,
	SC_HMS_HH_MASK,
	SC_HMS_MM_MASK,
	SC_HMS_SS_MASK,
	SC_HH_CENTER_X_CORR,
	SC_HH_CENTER_Y_CORR,
	SC_ROTATE,
	SC_MM_CENTER_X_CORR,
	SC_MM_CENTER_Y_CORR,
	SC_SS_CENTER_X_CORR,
	SC_SS_CENTER_Y_CORR,
	SC_HH_CENTER_X,
	SC_HH_CENTER_Y,
	SC_MM_CENTER_X,
	SC_MM_CENTER_Y,
	SC_SS_CENTER_X,
	SC_SS_CENTER_Y,
	SC_HH_X_ST_MASK,
	SC_HH_Y_ST_MASK,
	SC_HH_X_ED_MASK,
	SC_HH_Y_ED_MASK,
	SC_MM_X_ST_MASK,
	SC_MM_Y_ST_MASK,
	SC_MM_X_ED_MASK,
	SC_MM_Y_ED_MASK,
	SC_SS_X_ST_MASK,
	SC_SS_Y_ST_MASK,
	SC_SS_X_ED_MASK,
	SC_SS_Y_ED_MASK,
	SC_HH_FLIP_EN,
	SC_MM_FLIP_EN,
	SC_SS_FLIP_EN,
	SC_FLIP_ST,
	SC_FLIP_ED,
};

enum sc_blink_cfg_t {
	SB_ON_SEL,
	SB_BLK0_EN,
	SB_BLK1_EN,
	SB_ALPHA_INV,
	SB_BLINK_RATE,
	SB_BLINK0_FONT,
	SB_BLINK1_FONT,
	SB_BLINK_SYNC,
	SB_ST_BLINK0_X,
	SB_ST_BLINK0_Y,
	SB_ST_BLINK1_X,
	SB_ST_BLINK1_Y,
	SB_FONT0_IMG_WIDTH,
	SB_FONT0_IMG_HEIGHT,
	SB_FONT1_IMG_WIDTH,
	SB_FONT1_IMG_HEIGHT,
};

enum sc_afpc_cfg_t {
	SCP_ON_SEL,
	SCP_COMPEN_EN,
	SCP_MIN_RED,
	SCP_MIN_GREEN,
	SCP_MIN_BLUE,
	SCP_BASE_RED,
	SCP_BASE_GREEN,
	SCP_BASE_BLUE,
	SCP_BASE_OPT,
};

enum sc_icon_cfg_t {
	SI_ON_SEL,
	SI_ICON_EN,
	SI_ALPHA_INV,
	SI_ST_X,
	SI_ST_Y,
	SI_IMG_WIDTH,
	SI_IMG_HEIGHT,
};

enum sc_blink_sync_t{
	BLINK_SYNC_RATE = 0,
	BLINK_SYNC_0_5SEC = 2,
	BLINK_SYNC_1SEC = 3,
};

enum sc_move_cfg_t  {
	MV_ON_SEL,
	SCHEDULER_EN_I,
	SS_MOVE_ON,
	SC_MOVE_ON,
	SB_MOVE_ON,
	SI_MOVE_ON,
	SD_MOVE_ON,
	SE_MOVE_ON,
	DSP_X_ORG,
	DSP_Y_ORG,
	SC_MOVE_UPDATE_RATE,
	SC_MAX_MOVING_EN,
	SC_MAX_MOVING_POINT,
};

enum {
	WCLK_P_OFFSET,
	WCLK_M_OFFSET,
};

struct sc_time_st {
	unsigned int		sc_h;
	unsigned int		sc_m;
	unsigned int		sc_s;
	unsigned int		sc_ms;
	unsigned int		diff;
};

int s6e36w3x01_aod_ctrl(struct dsim_device *dsim, bool enable);
int s6e36w3x01_set_aod_time(struct dsim_device *dsim, struct sclk_time_cfg_v2 *time_cfg);
int s6e36w3x01_set_aod_analog (struct dsim_device *dsim, struct sclk_analog_cfg_v2 *analog_cfg);
int s6e36w3x01_set_aod_digital (struct dsim_device *dsim, struct sclk_digital_cfg_v2 *digital_cfg);
int s6e36w3x01_set_aod_icon (struct dsim_device *dsim, struct sclk_icon_cfg_v2 *icon_cfg);
int s6e36w3x01_set_aod_move(struct dsim_device *dsim, struct sclk_move_cfg_v2 *move_cfg);
int s6e36w3x01_aod_enter(struct dsim_device *dsim, enum aod_mode watch_type);
int s6e36w3x01_aod_exit(struct dsim_device *dsim, enum aod_mode watch_type);
int s6e36w3x01_afpc_set_compensation(struct dsim_device *dsim, struct afpc_compensation_v2 *comp);
#endif
