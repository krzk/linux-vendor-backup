/* s6e36w3x01_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2018 Samsung Electronics
 *
 * SeungBeom, Park <sb1.parki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <video/mipi_display.h>
#include <linux/platform_device.h>
#include <linux/lcd.h>

#include "../dsim.h"
#include "s6e36w3x01_l_dimming.h"
#include "s6e36w3x01_l_param.h"
#include "s6e36w3x01_l_mipi_lcd.h"
#include "s6e36w3x01_l_lcd_ctrl.h"
#include "s6e36w3x01_sc_ctrl.h"
#include "decon_lcd.h"

#define BACKLIGHT_DEV_NAME	"s6e36w3x01-bl"
#define LCD_DEV_NAME		"s6e36w3x01"
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#define DISPLAY_DQA 1

static struct device *esd_dev = NULL;

static const char* power_state[FB_BLANK_POWERDOWN+1] = {
	"UNBLANK",
	"NORMAL",
	"V_SUSPEND",
	"H_SUSPEND",
	"POWERDOWN",
};

static const int hlpm_brightness[HLPM_NIT_HIGH+1] = {0, 5, 40};

#ifdef CONFIG_HAS_EARLYSUSPEND
static struct early_suspend    s6e36w3x01_early_suspend;
#endif
unsigned char s6e36w3x01_gamma_set[MAX_BR_INFO][GAMMA_CMD_CNT];

static signed char a3_da_rtbl10nit[10] = {0, 24, 23, 22, 19, 15, 11, 6, 2, 0};
static signed char a3_da_rtbl11nit[10] = {0, 21, 24, 23, 18, 15, 10, 6, 2, 0};
static signed char a3_da_rtbl12nit[10] = {0, 24, 24, 22, 17, 15, 11, 7, 3, 1};
static signed char a3_da_rtbl13nit[10] = {0, 24, 24, 20, 16, 13, 9, 6, 2, 0};
static signed char a3_da_rtbl14nit[10] = {0, 24, 24, 20, 16, 12, 8, 6, 1, 0};
static signed char a3_da_rtbl15nit[10] = {0, 22, 22, 19, 15, 11, 8, 5, 1, 0};
static signed char a3_da_rtbl16nit[10] = {0, 21, 21, 19, 15, 11, 7, 5, 2, 0};
static signed char a3_da_rtbl17nit[10] = {0, 19, 20, 16, 13, 10, 6, 4, 1, 0};
static signed char a3_da_rtbl19nit[10] = {0, 19, 18, 18, 12, 9, 6, 5, 2, 0};
static signed char a3_da_rtbl20nit[10] = {0, 18, 17, 14, 11, 8, 6, 3, 1, 0};
static signed char a3_da_rtbl21nit[10] = {0, 17, 16, 14, 11, 8, 5, 2, 1, 0};
static signed char a3_da_rtbl22nit[10] = {0, 18, 17, 13, 10, 8, 5, 4, 1, 0};
static signed char a3_da_rtbl24nit[10] = {0, 18, 17, 13, 10, 8, 4, 3, 0, 0};
static signed char a3_da_rtbl25nit[10] = {0, 18, 16, 13, 10, 8, 5, 4, 1, 0};
static signed char a3_da_rtbl27nit[10] = {0, 17, 15, 12, 9, 6, 4, 3, 1, 0};
static signed char a3_da_rtbl29nit[10] = {0, 15, 14, 12, 8, 5, 3, 2, 0, 0};
static signed char a3_da_rtbl30nit[10] = {0, 16, 14, 11, 7, 6, 4, 2, 0, 0};
static signed char a3_da_rtbl32nit[10] = {0, 14, 13, 10, 7, 6, 4, 3, 1, 0};
static signed char a3_da_rtbl34nit[10] = {0, 14, 13, 10, 6, 5, 3, 3, 1, 0};
static signed char a3_da_rtbl37nit[10] = {0, 14, 13, 10, 6, 5, 2, 3, 1, 0};
static signed char a3_da_rtbl39nit[10] = {0, 13, 12, 9, 6, 5, 3, 2, 0, 0};
static signed char a3_da_rtbl41nit[10] = {0, 13, 12, 9, 5, 4, 2, 2, 0, 0};
static signed char a3_da_rtbl44nit[10] = {0, 12, 11, 7, 5, 4, 3, 2, 1, 0};
static signed char a3_da_rtbl47nit[10] = {0, 10, 9, 7, 4, 3, 2, 2, 1, 0};
static signed char a3_da_rtbl50nit[10] = {0, 9, 9, 6, 4, 3, 1, 2, 0, 0};
static signed char a3_da_rtbl53nit[10] = {0, 10, 9, 6, 4, 3, 1, 2, 0, 0};
static signed char a3_da_rtbl56nit[10] = {0, 10, 9, 5, 4, 2, 1, 2, 0, 0};
static signed char a3_da_rtbl60nit[10] = {0, 9, 8, 5, 3, 2, 1, 2, 0, 0};
static signed char a3_da_rtbl64nit[10] = {0, 8, 7, 4, 3, 2, 0, 0, 0, 0};
static signed char a3_da_rtbl68nit[10] = {0, 7, 6, 4, 3, 2, 1, 1, 1, 0};
static signed char a3_da_rtbl72nit[10] = {0, 8, 7, 5, 3, 3, 1, 1, 1, 0};
static signed char a3_da_rtbl77nit[10] = {0, 8, 7, 5, 3, 2, 1, 1, 0, 0};
static signed char a3_da_rtbl82nit[10] = {0, 8, 7, 4, 2, 2, 2, 1, 1, 0};
static signed char a3_da_rtbl87nit[10] = {0, 8, 7, 4, 3, 2, 1, 2, 2, 1};
static signed char a3_da_rtbl93nit[10] = {0, 9, 8, 5, 3, 2, 1, 2, 1, 0};
static signed char a3_da_rtbl98nit[10] = {0, 7, 6, 4, 2, 2, 1, 1, 1, 0};
static signed char a3_da_rtbl105nit[10] = {0, 8, 6, 4, 2, 2, 1, 1, 2, 0};
static signed char a3_da_rtbl111nit[10] = {0, 8, 6, 4, 2, 3, 2, 1, 2, 0};
static signed char a3_da_rtbl119nit[10] = {0, 9, 7, 4, 2, 2, 1, 1, 1, 0};
static signed char a3_da_rtbl126nit[10] = {0, 8, 6, 4, 2, 2, 2, 1, 2, 0};
static signed char a3_da_rtbl134nit[10] = {0, 7, 5, 4, 2, 2, 2, 3, 3, 2};
static signed char a3_da_rtbl143nit[10] = {0, 7, 5, 4, 2, 1, 2, 2, 2, 0};
static signed char a3_da_rtbl152nit[10] = {0, 7, 4, 3, 1, 1, 2, 2, 1, 0};
static signed char a3_da_rtbl162nit[10] = {0, 5, 4, 3, 2, 2, 2, 1, 1, 0};
static signed char a3_da_rtbl172nit[10] = {0, 5, 4, 2, 2, 1, 2, 2, 1, 0};
static signed char a3_da_rtbl183nit[10] = {0, 5, 4, 2, 1, 1, 1, 2, 1, 0};
static signed char a3_da_rtbl195nit[10] = {0, 5, 4, 2, 1, 1, 1, 2, 1, 0};
static signed char a3_da_rtbl207nit[10] = {0, 5, 4, 1, 0, 0, 2, 2, 1, 0};
static signed char a3_da_rtbl220nit[10] = {0, 4, 3, 2, 0, 0, 0, 1, 1, 0};
static signed char a3_da_rtbl234nit[10] = {0, 5, 4, 2, 1, 1, 2, 2, 2, 0};
static signed char a3_da_rtbl249nit[10] = {0, 6, 5, 2, 1, 0, 1, 2, 1, 0};
static signed char a3_da_rtbl265nit[10] = {0, 3, 3, 1, 0, 1, 2, 2, 1, 0};
static signed char a3_da_rtbl282nit[10] = {0, 5, 4, 2, 0, 0, 3, 2, 1, 0};
static signed char a3_da_rtbl300nit[10] = {0, 4, 3, 2, 0, 2, 3, 2, 1, 0};
static signed char a3_da_rtbl316nit[10] = {0, 3, 3, 2, 1, 1, 2, 2, 2, 0};
static signed char a3_da_rtbl333nit[10] = {0, 3, 2, 1, 0, 0, 1, 1, 1, 0};
static signed char a3_da_rtbl350nit[10] = {0, 2, 2, 0, 0, 0, 2, 3, 1, 0};
static signed char a3_da_rtbl357nit[10] = {0, 2, 2, 1, 0, 1, 2, 3, 1, 0};
static signed char a3_da_rtbl365nit[10] = {0, 3, 3, 1, 1, 1, 2, 2, 1, 0};
static signed char a3_da_rtbl372nit[10] = {0, 4, 3, 1, 0, 0, 2, 1, 0, 0};
static signed char a3_da_rtbl380nit[10] = {0, 2, 2, 1, 0, 0, 1, 1, 0, 0};
static signed char a3_da_rtbl387nit[10] = {0, 4, 3, 1, 0, 1, 2, 2, 1, 0};
static signed char a3_da_rtbl395nit[10] = {0, 3, 3, 1, 0, 1, 2, 2, 2, 0};
static signed char a3_da_rtbl403nit[10] = {0, 3, 3, 0, 0, 1, 3, 2, 1, 0};
static signed char a3_da_rtbl412nit[10] = {0, 3, 3, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl420nit[10] = {0, 3, 3, 0, 0, 1, 2, 1, 1, 0};
static signed char a3_da_rtbl443nit[10] = {0, 2, 3, 1, 0, 0, 2, 2, 1, 0};
static signed char a3_da_rtbl465nit[10] = {0, 1, 2, 0, 0, 0, 2, 1, -1, 0};
static signed char a3_da_rtbl488nit[10] = {0, 4, 3, 1, 0, 0, 1, 1, 1, 0};
static signed char a3_da_rtbl510nit[10] = {0, 4, 3, 0, 1, 1, 2, 2, 1, 0};
static signed char a3_da_rtbl533nit[10] = {0, 2, 3, 0, 0, 1, 3, 4, 1, 0};
static signed char a3_da_rtbl555nit[10] = {0, 1, 3, 0, 0, 0, 0, 0, 0, 0};
static signed char a3_da_rtbl578nit[10] = {0, 0, 2, 0, 0, 0, 1, 1, -2, 0};
static signed char a3_da_rtbl600nit[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static signed char a3_da_ctbl10nit[30] = {0, 0, 0, -2, 3, -12, -22, 0, -30, -5, 2, -11, -10, 0, -14, -9, 6, -9, -9, 1, -9, -2, 1, -2, -2, 0, -3, 1, 1, 0};
static signed char a3_da_ctbl11nit[30] = {0, 0, 0, -7, 1, -14, -22, 0, -30, -8, 0, -14, -13, 2, -12, -11, 4, -11, -10, -1, -9, -2, 1, -2, -2, 0, -3, 1, 1, 0};
static signed char a3_da_ctbl12nit[30] = {0, 0, 0, -6, 0, -14, -18, 0, -19, -8, 0, -13, -10, 6, -12, -12, 3, -10, -9, -1, -9, -2, 0, -3, -1, 0, -3, 1, 1, 1};
static signed char a3_da_ctbl13nit[30] = {0, 0, 0, -6, 0, -14, -24, 0, -30, -7, 2, -11, -9, 3, -11, -11, 2, -10, -9, -1, -9, -2, 0, -2, -2, 0, -3, 1, 2, 2};
static signed char a3_da_ctbl14nit[30] = {0, 0, 0, -7, 1, -14, -18, 2, -24, -10, -1, -10, -14, 0, -13, -10, 2, -9, -6, 1, -7, -3, 0, -4, 0, 0, -1, 2, 2, 2};
static signed char a3_da_ctbl15nit[30] = {0, 0, 0, -7, 1, -14, -15, 2, -10, -11, 2, -15, -10, 0, -14, -10, 3, -9, -6, 0, -6, -3, 0, -2, -1, 0, -2, 0, 1, 0};
static signed char a3_da_ctbl16nit[30] = {0, 0, 0, -7, 1, -14, -15, 2, -10, -11, 2, -15, -13, 1, -14, -11, 1, -10, -8, 0, -7, -1, 0, -3, -1, 0, -2, 0, 0, 0};
static signed char a3_da_ctbl17nit[30] = {0, 0, 0, -4, 3, -11, -16, 0, -15, -13, 1, -16, -11, 1, -14, -10, 2, -9, -6, 1, -6, -1, 0, -2, 0, 0, -1, 0, 0, 0};
static signed char a3_da_ctbl19nit[30] = {0, 0, 0, -4, 3, -11, -14, 0, -14, -13, 0, -17, -13, 1, -14, -11, 2, -10, -5, 1, -4, -1, 0, -1, -2, 0, -4, 1, 0, 1};
static signed char a3_da_ctbl20nit[30] = {0, 0, 0, -14, 3, -6, -12, 3, -10, -14, 1, -18, -7, 2, -9, -10, 2, -8, -5, 0, -5, -1, 1, -2, 1, 0, 0, 2, 1, 2};
static signed char a3_da_ctbl21nit[30] = {0, 0, 0, -14, 4, -3, -12, 0, -24, -11, 2, -13, -8, 0, -9, -6, 4, -8, -5, 1, -6, 0, 1, 0, 0, 0, 1, 0, -1, 0};
static signed char a3_da_ctbl22nit[30] = {0, 0, 0, -5, 1, -1, -13, 0, -21, -10, 3, -14, -9, 3, -11, -7, 0, -8, -5, 1, -5, -1, 0, -1, 0, 0, -2, 0, 0, 1};
static signed char a3_da_ctbl24nit[30] = {0, 0, 0, -13, 2, -3, -14, 0, -22, -12, 2, -15, -8, 3, -9, -9, 0, -9, -4, 0, -5, -1, 0, -1, 0, 0, -2, 2, 1, 3};
static signed char a3_da_ctbl25nit[30] = {0, 0, 0, -6, 1, 1, -18, 0, -25, -11, 3, -15, -5, 4, -9, -7, 0, -8, -5, 0, -4, -2, 0, -1, -1, -1, -2, 1, 0, 2};
static signed char a3_da_ctbl27nit[30] = {0, 0, 0, -17, 0, -8, -12, 2, -19, -8, 4, -12, -8, 0, -9, -7, 0, -7, -4, 1, -4, 0, 0, -1, -1, 0, -2, 2, 1, 3};
static signed char a3_da_ctbl29nit[30] = {0, 0, 0, -8, -1, -2, -11, 5, -18, -13, 0, -16, -10, 0, -11, -5, 1, -6, -4, 0, -4, 0, 1, -1, 0, 0, -1, 1, 0, 2};
static signed char a3_da_ctbl30nit[30] = {0, 0, 0, -4, 0, -2, -13, 5, -19, -13, 0, -17, -10, 2, -10, -6, 0, -6, -5, 0, -5, 1, 0, -1, 0, 0, -1, 2, 1, 3};
static signed char a3_da_ctbl32nit[30] = {0, 0, 0, -2, 1, 0, -12, 3, -20, -8, 4, -9, -6, 1, -9, -3, 2, -4, -4, 0, -3, -2, 0, -1, -1, 0, -2, 1, 0, 2};
static signed char a3_da_ctbl34nit[30] = {0, 0, 0, -2, 0, -3, -12, 3, -20, -12, 1, -15, -9, 1, -9, -5, 0, -7, -2, 1, -2, -1, 0, -1, 0, -1, -2, 2, 1, 3};
static signed char a3_da_ctbl37nit[30] = {0, 0, 0, -9, 2, -3, -15, 1, -21, -13, 0, -15, -8, 0, -8, -5, 0, -6, -1, 1, -2, -1, 0, -1, 0, 0, -1, 1, 0, 2};
static signed char a3_da_ctbl39nit[30] = {0, 0, 0, -1, 1, -3, -14, 2, -23, -6, 4, -8, -8, -1, -9, -5, 0, -5, -2, 0, -3, -2, 0, -1, 1, 0, -1, 1, 0, 2};
static signed char a3_da_ctbl41nit[30] = {0, 0, 0, -5, 1, -3, -14, 1, -22, -14, 0, -14, -5, 1, -5, -4, 1, -4, -1, 1, -2, -1, 0, -1, 0, 0, -1, 1, 0, 2};
static signed char a3_da_ctbl44nit[30] = {0, 0, 0, 0, 2, -3, -16, 1, -23, -7, 4, -10, -5, 2, -5, -1, 0, -3, -1, 0, -2, 0, 0, 0, -1, -1, -3, 2, 1, 3};
static signed char a3_da_ctbl47nit[30] = {0, 0, 0, 1, 2, -2, -9, 5, -18, -6, 3, -9, -3, 3, -3, -2, 0, -3, -1, 0, -2, 0, 1, 0, -1, -1, -2, 2, 1, 3};
static signed char a3_da_ctbl50nit[30] = {0, 0, 0, 6, 4, 1, -10, 5, -18, -6, 3, -7, -3, 1, -3, -2, -1, -3, -1, 1, -2, 0, 1, 0, 0, -1, -1, 1, 1, 2};
static signed char a3_da_ctbl53nit[30] = {0, 0, 0, 0, 1, -4, -18, 0, -26, 0, 4, -2, -3, 2, -1, -2, 0, -2, -1, 0, -1, 0, 0, -1, 0, 0, -2, 2, 1, 3};
static signed char a3_da_ctbl56nit[30] = {0, 0, 0, -4, 1, -4, -17, 2, -23, -4, 1, -5, -5, -1, -6, 0, 2, -1, 0, 1, 0, 0, 0, 0, -1, -1, -1, 2, 1, 3};
static signed char a3_da_ctbl60nit[30] = {0, 0, 0, -1, 2, -6, -17, 1, -26, -3, 1, -2, -1, 1, -1, 1, 0, -1, -1, 0, -1, 0, 0, -1, 0, 0, -1, 2, 1, 3};
static signed char a3_da_ctbl64nit[30] = {0, 0, 0, -4, 3, -6, -11, 4, -20, 0, 4, -2, 0, 0, -1, 1, 0, -2, -2, 1, -1, 1, 1, 1, 0, 0, -1, 2, 1, 3};
static signed char a3_da_ctbl68nit[30] = {0, 0, 0, -2, 3, -7, -10, 4, -15, -3, 1, -4, 4, 3, 1, -1, 1, -3, 0, 1, 1, -1, 0, -1, 0, 0, -1, 0, -1, 1};
static signed char a3_da_ctbl72nit[30] = {0, 0, 0, -3, 1, -9, -11, 5, -16, -4, 0, -4, 0, 1, -1, 0, 0, -2, 0, 1, 0, -1, 0, -2, 0, 0, 0, 3, 1, 4};
static signed char a3_da_ctbl77nit[30] = {0, 0, 0, -4, 2, -9, -11, 3, -16, -5, 0, -4, 0, 1, -3, -1, 1, -1, 0, 1, -1, 0, 0, 0, 1, 1, 0, 1, -1, 1};
static signed char a3_da_ctbl82nit[30] = {0, 0, 0, -4, 2, -10, -14, 1, -19, -3, 0, -3, 1, 1, 0, 0, 2, 0, -3, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 3};
static signed char a3_da_ctbl87nit[30] = {0, 0, 0, -3, 1, -10, -9, 4, -14, -6, 0, -5, -1, 0, -3, -1, 1, -2, 0, 1, 0, 0, 0, -1, -1, 0, -1, 1, -1, 2};
static signed char a3_da_ctbl93nit[30] = {0, 0, 0, -2, 2, -8, -9, 3, -14, -7, 0, -5, -1, -1, -3, -2, 0, -3, 0, 1, 1, 1, 0, -1, -1, 0, -2, 3, 1, 5};
static signed char a3_da_ctbl98nit[30] = {0, 0, 0, -4, 1, -11, -9, 2, -16, 1, 4, 2, 2, 2, 1, -2, 1, -1, 0, 1, 1, 0, 0, 0, -1, 0, -3, 3, 1, 5};
static signed char a3_da_ctbl105nit[30] = {0, 0, 0, -5, 1, -12, -9, 2, -15, 0, 3, 1, -2, -1, -4, -2, 1, -4, 1, 1, 2, 1, 0, 0, -1, 0, -1, 2, -1, 3};
static signed char a3_da_ctbl111nit[30] = {0, 0, 0, -4, 1, -11, -12, -1, -17, -1, 2, 0, 1, 2, -1, -2, 0, 1, -1, 0, -1, -1, 0, 0, -1, 0, -2, 3, 1, 4};
static signed char a3_da_ctbl119nit[30] = {0, 0, 0, -5, 1, -11, -14, -1, -18, 0, 2, 0, 1, 1, -1, -1, 1, -2, -1, 0, 0, 1, 0, 1, -1, 0, -3, 2, -1, 4};
static signed char a3_da_ctbl126nit[30] = {0, 0, 0, -5, 0, -12, -8, 3, -12, -6, -1, -4, 1, 1, -2, 1, 0, 1, -2, 0, 0, 1, 0, -1, 0, 0, -1, 2, 1, 3};
static signed char a3_da_ctbl134nit[30] = {0, 0, 0, -6, 0, -13, -8, 1, -12, -6, -1, -5, 0, 0, -1, 0, 1, -1, -1, 0, 0, 0, 0, 0, 0, 0, -2, 1, 0, 4};
static signed char a3_da_ctbl143nit[30] = {0, 0, 0, -7, 0, -13, -9, 3, -14, -2, 2, 0, 0, 0, -1, -1, 1, -1, -1, 0, -1, 0, 0, -1, -1, 0, -1, 2, 0, 3};
static signed char a3_da_ctbl152nit[30] = {0, 0, 0, -8, 0, -14, -10, 3, -12, -2, 1, -2, 1, 1, 0, -1, 1, -1, 0, 0, -1, 0, 0, 1, 0, 0, -2, 2, 0, 3};
static signed char a3_da_ctbl162nit[30] = {0, 0, 0, -4, 4, -9, -11, 2, -15, 0, 2, 2, 1, 2, 0, -2, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, -2, 2, 0, 2};
static signed char a3_da_ctbl172nit[30] = {0, 0, 0, -2, 4, -8, -13, 1, -15, 3, 4, 4, 0, 0, -3, -1, 0, -1, 0, 1, 2, 1, 0, 1, 0, 0, -1, 1, 0, 2};
static signed char a3_da_ctbl183nit[30] = {0, 0, 0, -3, 4, -8, -14, 0, -15, -1, 1, 1, 2, 2, 0, -1, 0, 1, 1, 1, 1, -1, 0, 0, 0, 0, -1, 1, 0, 2};
static signed char a3_da_ctbl195nit[30] = {0, 0, 0, -5, 4, -9, -14, -1, -16, -1, 0, 1, 1, 2, -1, 0, 0, 1, 2, 1, 1, -1, 0, 0, 0, 0, -1, 1, 0, 2};
static signed char a3_da_ctbl207nit[30] = {0, 0, 0, -5, 3, -9, -14, -2, -16, 3, 2, 4, 2, 2, -2, 0, 0, 1, -1, 0, 1, 0, 0, 0, 0, 0, -1, 2, 0, 3};
static signed char a3_da_ctbl220nit[30] = {0, 0, 0, -9, 0, -11, -6, 3, -8, 4, 2, 3, 1, 2, -1, 0, 0, 0, 1, 1, 2, 0, 0, 1, -1, 0, -2, 2, 0, 3};
static signed char a3_da_ctbl234nit[30] = {0, 0, 0, -7, 3, -10, -8, 0, -10, -2, 1, 0, 0, 0, -1, 0, 0, -1, -1, 0, 0, 1, -1, 0, -1, 0, -1, 3, 0, 3};
static signed char a3_da_ctbl249nit[30] = {0, 0, 0, -8, 2, -12, -9, -1, -7, 2, 2, 2, 1, 0, 0, 0, 1, 1, 0, 0, -1, 0, 0, 1, -1, 0, -2, 3, 1, 2};
static signed char a3_da_ctbl265nit[30] = {0, 0, 0, -7, 0, -9, -2, 3, -3, 2, 2, 2, 3, 1, 2, -1, 0, -1, 0, 0, 0, -1, 0, -2, -1, 0, -1, 1, 0, 4};
static signed char a3_da_ctbl282nit[30] = {0, 0, 0, -9, 1, -11, -4, 1, -6, -3, -1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, -1, 0, -1, -1, 0, -2, 2, -1, 3};
static signed char a3_da_ctbl300nit[30] = {0, 0, 0, -9, 1, -12, -4, 1, -4, -1, -1, 0, 3, 2, 2, -1, 0, 1, 0, 0, 1, -1, 0, -2, -1, 0, -1, 2, 1, 3};
static signed char a3_da_ctbl316nit[30] = {0, 0, 0, -6, 1, -9, -1, 2, 0, -3, -1, 0, -1, 0, -2, 0, -1, 0, -1, 0, 0, 0, 0, 0, 0, 0, -2, 1, 1, 4};
static signed char a3_da_ctbl333nit[30] = {0, 0, 0, -6, 2, -10, -1, 1, 2, -3, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2};
static signed char a3_da_ctbl350nit[30] = {0, 0, 0, -6, -1, -7, 2, 4, 1, 3, 3, 4, 2, 1, 1, 0, -1, 0, 1, 1, 0, -1, 0, -1, -1, -1, -1, 2, 2, 4};
static signed char a3_da_ctbl357nit[30] = {0, 0, 0, -4, 2, -5, -3, 0, -1, -3, -1, -1, 3, 2, 2, 0, 0, 0, 1, 1, 1, -1, 0, -1, 0, 0, 0, 0, 1, 1};
static signed char a3_da_ctbl365nit[30] = {0, 0, 0, -4, 2, -5, -1, 1, -2, 1, 2, 2, 1, 0, 0, -1, 0, 1, -1, 0, -2, 1, 0, 1, -1, 0, -2, 1, 0, 2};
static signed char a3_da_ctbl372nit[30] = {0, 0, 0, -8, 1, -10, -2, 1, 0, -3, -1, -1, 2, 1, 0, 0, 1, 1, -1, 0, 0, 1, 0, 0, 0, 0, -1, 1, 0, 3};
static signed char a3_da_ctbl380nit[30] = {0, 0, 0, -8, 0, -10, -3, 0, -2, -2, 0, 0, 1, 0, 1, 0, 1, 2, -1, 0, 0, 1, 0, -1, 0, 0, -1, 1, 0, 3};
static signed char a3_da_ctbl387nit[30] = {0, 0, 0, -9, 0, -11, -2, 0, 0, -3, -1, -1, 2, 1, 0, 0, 1, 1, -1, 0, 0, 1, 0, 0, 0, 0, -1, 1, 1, 3};
static signed char a3_da_ctbl395nit[30] = {0, 0, 0, -4, 1, -5, -3, 0, -2, 2, 2, 3, -3, -2, -2, -1, 1, 0, 0, 0, 0, 1, 0, -1, 0, -1, 0, 1, 1, 3};
static signed char a3_da_ctbl403nit[30] = {0, 0, 0, -5, 1, -5, -2, 1, -2, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, -1, 0, -1, -1, 1, 1, 3};
static signed char a3_da_ctbl412nit[30] = {0, 0, 0, -6, 0, -7, -3, -1, -1, 2, 3, 3, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 3};
static signed char a3_da_ctbl420nit[30] = {0, 0, 0, -4, 1, -6, -3, -1, -1, 1, 2, 2, 3, 2, 2, -2, 0, 1, 1, 0, 1, 0, 0, -2, 1, 0, 1, -1, 1, 0};
static signed char a3_da_ctbl443nit[30] = {0, 0, 0, -7, 0, -7, 3, 3, 3, -3, -1, -1, 1, 1, 0, 1, 1, -1, -1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 1, 3};
static signed char a3_da_ctbl465nit[30] = {0, 0, 0, -3, 1, -3, 2, 2, 2, 1, 2, 2, -3, -2, -4, 0, 2, 2, 0, -1, 0, -1, 0, -1, 1, 0, 0, 1, 1, 2};
static signed char a3_da_ctbl488nit[30] = {0, 0, 0, -9, 1, -10, 1, 0, 1, -3, -1, -2, -3, -2, -4, 1, 1, 2, -2, 0, -1, 2, 0, -1, -1, -1, 0, 2, 1, 2};
static signed char a3_da_ctbl510nit[30] = {0, 0, 0, -10, 0, -11, 1, 0, 1, -4, -1, -2, -2, -1, -2, 0, 0, 1, 0, 0, 0, 0, 0, -1, 0, -1, 0, 2, 0, 3};
static signed char a3_da_ctbl533nit[30] = {0, 0, 0, -3, 0, -2, 0, -1, 0, 0, 1, 1, -1, -1, -2, -1, 0, -1, 0, 0, 0, -2, -2, -1, 0, -1, -1, 1, 0, 1};
static signed char a3_da_ctbl555nit[30] = {0, 0, 0, 2, 1, 1, 3, 2, 3, -2, 0, 0, -2, -2, -3, -1, 0, 0, -1, -1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1};
static signed char a3_da_ctbl578nit[30] = {0, 0, 0, 0, 0, 0, 4, 1, 4, -2, 1, 0, -1, 1, -1, 0, 0, 0, -1, 0, 0, 0, 0, 1, 2, 0, 1, -1, 0, -1};
static signed char a3_da_ctbl600nit[30] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static struct SmtDimInfo s6e36w3x01_dimming_info[MAX_BR_INFO] = {
	{ .br = 10, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl10nit, .cTbl = a3_da_ctbl10nit, .way = W1},
	{ .br = 11, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl11nit, .cTbl = a3_da_ctbl11nit, .way = W1},
	{ .br = 12, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl12nit, .cTbl = a3_da_ctbl12nit, .way = W1},
	{ .br = 13, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl13nit, .cTbl = a3_da_ctbl13nit, .way = W1},
	{ .br = 14, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl14nit, .cTbl = a3_da_ctbl14nit, .way = W1},
	{ .br = 15, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl15nit, .cTbl = a3_da_ctbl15nit, .way = W1},
	{ .br = 16, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl16nit, .cTbl = a3_da_ctbl16nit, .way = W1},
	{ .br = 17, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl17nit, .cTbl = a3_da_ctbl17nit, .way = W1},
	{ .br = 19, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl19nit, .cTbl = a3_da_ctbl19nit, .way = W1},
	{ .br = 20, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl20nit, .cTbl = a3_da_ctbl20nit, .way = W1},
	{ .br = 21, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl21nit, .cTbl = a3_da_ctbl21nit, .way = W1},
	{ .br = 22, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl22nit, .cTbl = a3_da_ctbl22nit, .way = W1},
	{ .br = 24, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl24nit, .cTbl = a3_da_ctbl24nit, .way = W1},
	{ .br = 25, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl25nit, .cTbl = a3_da_ctbl25nit, .way = W1},
	{ .br = 27, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl27nit, .cTbl = a3_da_ctbl27nit, .way = W1},
	{ .br = 29, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl29nit, .cTbl = a3_da_ctbl29nit, .way = W1},
	{ .br = 30, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl30nit, .cTbl = a3_da_ctbl30nit, .way = W1},
	{ .br = 32, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl32nit, .cTbl = a3_da_ctbl32nit, .way = W1},
	{ .br = 34, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl34nit, .cTbl = a3_da_ctbl34nit, .way = W1},
	{ .br = 37, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl37nit, .cTbl = a3_da_ctbl37nit, .way = W1},
	{ .br = 39, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl39nit, .cTbl = a3_da_ctbl39nit, .way = W1},
	{ .br = 41, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl41nit, .cTbl = a3_da_ctbl41nit, .way = W1},
	{ .br = 44, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl44nit, .cTbl = a3_da_ctbl44nit, .way = W1},
	{ .br = 47, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl47nit, .cTbl = a3_da_ctbl47nit, .way = W1},
	{ .br = 50, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl50nit, .cTbl = a3_da_ctbl50nit, .way = W1},
	{ .br = 53, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl53nit, .cTbl = a3_da_ctbl53nit, .way = W1},
	{ .br = 56, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl56nit, .cTbl = a3_da_ctbl56nit, .way = W1},
	{ .br = 60, .refBr = 106, .cGma = gma2p15, .rTbl = a3_da_rtbl60nit, .cTbl = a3_da_ctbl60nit, .way = W1},
	{ .br = 64, .refBr = 107, .cGma = gma2p15, .rTbl = a3_da_rtbl64nit, .cTbl = a3_da_ctbl64nit, .way = W1},
	{ .br = 68, .refBr = 114, .cGma = gma2p15, .rTbl = a3_da_rtbl68nit, .cTbl = a3_da_ctbl68nit, .way = W1},
	{ .br = 72, .refBr = 120, .cGma = gma2p15, .rTbl = a3_da_rtbl72nit, .cTbl = a3_da_ctbl72nit, .way = W1},
	{ .br = 77, .refBr = 129, .cGma = gma2p15, .rTbl = a3_da_rtbl77nit, .cTbl = a3_da_ctbl77nit, .way = W1},
	{ .br = 82, .refBr = 136, .cGma = gma2p15, .rTbl = a3_da_rtbl82nit, .cTbl = a3_da_ctbl82nit, .way = W1},
	{ .br = 87, .refBr = 142, .cGma = gma2p15, .rTbl = a3_da_rtbl87nit, .cTbl = a3_da_ctbl87nit, .way = W1},
	{ .br = 93, .refBr = 150, .cGma = gma2p15, .rTbl = a3_da_rtbl93nit, .cTbl = a3_da_ctbl93nit, .way = W1},
	{ .br = 98, .refBr = 158, .cGma = gma2p15, .rTbl = a3_da_rtbl98nit, .cTbl = a3_da_ctbl98nit, .way = W1},
	{ .br = 105, .refBr = 170, .cGma = gma2p15, .rTbl = a3_da_rtbl105nit, .cTbl = a3_da_ctbl105nit, .way = W1},
	{ .br = 111, .refBr = 178, .cGma = gma2p15, .rTbl = a3_da_rtbl111nit, .cTbl = a3_da_ctbl111nit, .way = W1},
	{ .br = 119, .refBr = 189, .cGma = gma2p15, .rTbl = a3_da_rtbl119nit, .cTbl = a3_da_ctbl119nit, .way = W1},
	{ .br = 126, .refBr = 197, .cGma = gma2p15, .rTbl = a3_da_rtbl126nit, .cTbl = a3_da_ctbl126nit, .way = W1},
	{ .br = 134, .refBr = 208, .cGma = gma2p15, .rTbl = a3_da_rtbl134nit, .cTbl = a3_da_ctbl134nit, .way = W1},
	{ .br = 143, .refBr = 222, .cGma = gma2p15, .rTbl = a3_da_rtbl143nit, .cTbl = a3_da_ctbl143nit, .way = W1},
	{ .br = 152, .refBr = 235, .cGma = gma2p15, .rTbl = a3_da_rtbl152nit, .cTbl = a3_da_ctbl152nit, .way = W1},
	{ .br = 162, .refBr = 248, .cGma = gma2p15, .rTbl = a3_da_rtbl162nit, .cTbl = a3_da_ctbl162nit, .way = W1},
	{ .br = 172, .refBr = 257, .cGma = gma2p15, .rTbl = a3_da_rtbl172nit, .cTbl = a3_da_ctbl172nit, .way = W1},
	{ .br = 183, .refBr = 257, .cGma = gma2p15, .rTbl = a3_da_rtbl183nit, .cTbl = a3_da_ctbl183nit, .way = W1},
	{ .br = 195, .refBr = 257, .cGma = gma2p15, .rTbl = a3_da_rtbl195nit, .cTbl = a3_da_ctbl195nit, .way = W1},
	{ .br = 207, .refBr = 257, .cGma = gma2p15, .rTbl = a3_da_rtbl207nit, .cTbl = a3_da_ctbl207nit, .way = W1},
	{ .br = 220, .refBr = 257, .cGma = gma2p15, .rTbl = a3_da_rtbl220nit, .cTbl = a3_da_ctbl220nit, .way = W1},
	{ .br = 234, .refBr = 270, .cGma = gma2p15, .rTbl = a3_da_rtbl234nit, .cTbl = a3_da_ctbl234nit, .way = W1},
	{ .br = 249, .refBr = 287, .cGma = gma2p15, .rTbl = a3_da_rtbl249nit, .cTbl = a3_da_ctbl249nit, .way = W1},
	{ .br = 265, .refBr = 303, .cGma = gma2p15, .rTbl = a3_da_rtbl265nit, .cTbl = a3_da_ctbl265nit, .way = W1},
	{ .br = 282, .refBr = 323, .cGma = gma2p15, .rTbl = a3_da_rtbl282nit, .cTbl = a3_da_ctbl282nit, .way = W1},
	{ .br = 300, .refBr = 338, .cGma = gma2p15, .rTbl = a3_da_rtbl300nit, .cTbl = a3_da_ctbl300nit, .way = W1},
	{ .br = 316, .refBr = 358, .cGma = gma2p15, .rTbl = a3_da_rtbl316nit, .cTbl = a3_da_ctbl316nit, .way = W1},
	{ .br = 333, .refBr = 377, .cGma = gma2p15, .rTbl = a3_da_rtbl333nit, .cTbl = a3_da_ctbl333nit, .way = W1},
	{ .br = 350, .refBr = 393, .cGma = gma2p15, .rTbl = a3_da_rtbl350nit, .cTbl = a3_da_ctbl350nit, .way = W1},
	{ .br = 357, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl357nit, .cTbl = a3_da_ctbl357nit, .way = W1},
	{ .br = 365, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl365nit, .cTbl = a3_da_ctbl365nit, .way = W1},
	{ .br = 372, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl372nit, .cTbl = a3_da_ctbl372nit, .way = W1},
	{ .br = 380, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl380nit, .cTbl = a3_da_ctbl380nit, .way = W1},
	{ .br = 387, .refBr = 401, .cGma = gma2p15, .rTbl = a3_da_rtbl387nit, .cTbl = a3_da_ctbl387nit, .way = W1},
	{ .br = 395, .refBr = 402, .cGma = gma2p15, .rTbl = a3_da_rtbl395nit, .cTbl = a3_da_ctbl395nit, .way = W1},
	{ .br = 403, .refBr = 414, .cGma = gma2p15, .rTbl = a3_da_rtbl403nit, .cTbl = a3_da_ctbl403nit, .way = W1},
	{ .br = 412, .refBr = 420, .cGma = gma2p15, .rTbl = a3_da_rtbl412nit, .cTbl = a3_da_ctbl412nit, .way = W1},
	{ .br = 420, .refBr = 428, .cGma = gma2p15, .rTbl = a3_da_rtbl420nit, .cTbl = a3_da_ctbl420nit, .way = W1},
	{ .br = 443, .refBr = 453, .cGma = gma2p15, .rTbl = a3_da_rtbl443nit, .cTbl = a3_da_ctbl443nit, .way = W1},
	{ .br = 465, .refBr = 472, .cGma = gma2p15, .rTbl = a3_da_rtbl465nit, .cTbl = a3_da_ctbl465nit, .way = W1},
	{ .br = 488, .refBr = 495, .cGma = gma2p15, .rTbl = a3_da_rtbl488nit, .cTbl = a3_da_ctbl488nit, .way = W1},
	{ .br = 510, .refBr = 516, .cGma = gma2p15, .rTbl = a3_da_rtbl510nit, .cTbl = a3_da_ctbl510nit, .way = W1},
	{ .br = 533, .refBr = 539, .cGma = gma2p15, .rTbl = a3_da_rtbl533nit, .cTbl = a3_da_ctbl533nit, .way = W1},
	{ .br = 555, .refBr = 564, .cGma = gma2p15, .rTbl = a3_da_rtbl555nit, .cTbl = a3_da_ctbl555nit, .way = W1},
	{ .br = 578, .refBr = 587, .cGma = gma2p15, .rTbl = a3_da_rtbl578nit, .cTbl = a3_da_ctbl578nit, .way = W1},
	{ .br = 600, .refBr = 600, .cGma = gma2p20, .rTbl = a3_da_rtbl600nit, .cTbl = a3_da_ctbl600nit, .way = W1},
};

static const unsigned char MAX_GAMMA_SET[GAMMA_CMD_CNT] = {
	0xCA,
	0x07, 0x00, 0x00,
	0x00, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x00, 0x00,
	0x00,
};

unsigned char mtp_default[S6E36W3_MTP_SIZE] = {
	0, 46, 46, 83, 89, 90, 91, 86, 85, 88,
	64, 62, 67, 72, 70, 76, 90, 90, 94, 84,
	89, 92, 59, 72, 69, 106, 116, 107, 42, 42,
	 58, 0, 2, 50,
};

static int panel_id;
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

	pr_info("%s: panel_id = 0x%x", __func__, panel_id);

	return 0;
}
__setup("lcdtype=", panel_id_cmdline);

#define	DEBUG_READ_DELAY	40 /* 40 ms */
extern void s6e36w3x01_print_debug_reg(int id);
extern void s6e36w3x01_testkey_enable(u32 id);
extern void s6e36w3x01_testkey_disable(u32 id);
static void s6e36w3x01_debug_dwork(struct work_struct *work)
{
	struct s6e36w3x01 *lcd = container_of(work,
				struct s6e36w3x01, debug_dwork.work);
	struct dsim_device *dsim = lcd->dsim;

	cancel_delayed_work(&lcd->debug_dwork);

	if (POWER_IS_OFF(lcd->power)) {
		dev_err(lcd->dev, "%s:panel off.\n", __func__);
		return;
	}

	/* Test Key Enable */
	s6e36w3x01_testkey_enable(dsim->id);

	s6e36w3x01_print_debug_reg(dsim->id);

	s6e36w3x01_testkey_disable(dsim->id);
}

void s6e36w3x01_send_esd_event(void)
{
	char *event_string = "LCD_ESD=ON";
	char *envp[] = {event_string, NULL};

	if (esd_dev != NULL)
		kobject_uevent_env(&esd_dev->kobj, KOBJ_CHANGE, envp);
	else
		pr_err("%s: esd_dev is NULL\n", __func__);
}
EXPORT_SYMBOL(s6e36w3x01_send_esd_event);

#define ESD_RETRY_TIMEOUT		(10*1000) /* 10 seconds */
static void s6e36w3x01_esd_dwork(struct work_struct *work)
{
	struct s6e36w3x01 *lcd = container_of(work,
				struct s6e36w3x01, esd_dwork.work);

	if (POWER_IS_OFF(lcd->power)) {
		dev_err(lcd->dev, "%s:panel off.\n", __func__);
		return;
	}

	cancel_delayed_work(&lcd->esd_dwork);
	schedule_delayed_work(&lcd->esd_dwork,
			msecs_to_jiffies(ESD_RETRY_TIMEOUT));

	s6e36w3x01_send_esd_event();

	lcd->esd_cnt++;
	dev_info(lcd->dev, "%s:Send uevent. ESD DETECTED[%d]\n",
					__func__, lcd->esd_cnt);
}

static ssize_t s6e36w3x01_mcd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->mcd_on ? "on" : "off");
}

extern int s6e36w3x01_hbm_on(int id);
extern void s6e36w3x01_mcd_test_on(struct s6e36w3x01 *lcd);
static ssize_t s6e36w3x01_mcd_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	int old_hbm;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->hbm_on) {
		old_hbm = lcd->hbm_on;
		lcd->hbm_on = 0;
		s6e36w3x01_hbm_on(dsim->id);
		lcd->hbm_on = old_hbm;
	}

	if (!strncmp(buf, "on", 2))
		lcd->mcd_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->mcd_on = false;
	else
		dev_err(dev, "%s:invalid command.\n", __func__);

	s6e36w3x01_mcd_test_on(lcd);

	return size;
}

static ssize_t s6e36w3x01_hlpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	pr_info("%s:val[%d]\n", __func__, hlpm_brightness[lcd->hlpm_nit]);

	return sprintf(buf, "%d\n", hlpm_brightness[lcd->hlpm_nit]);
}

extern void s6e36w3x01_hlpm_ctrl(struct s6e36w3x01 *lcd, bool enable);
static ssize_t s6e36w3x01_hlpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (!strncmp(buf, "on", 2))
		lcd->hlpm_on = true;
	else if (!strncmp(buf, "off", 3))
		lcd->hlpm_on = false;
	else {
		dev_err(dev, "invalid command.\n");
		return size;
	}

	s6e36w3x01_hlpm_ctrl(lcd, lcd->hlpm_on);

	pr_info("%s: hlpm_on[%d]\n", __func__, lcd->hlpm_on);

	return size;
}

static ssize_t s6e36w3x01_alpm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int len = 0;

	pr_info("%s:val[%d]\n", __func__, lcd->ao_mode);

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

static ssize_t s6e36w3x01_alpm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

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

void s6e36w3x01_set_resolution(int id, int resolution);
static ssize_t s6e36w3x01_resolution_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	int resolution;

	sscanf(buf, "%d", &resolution);

	if (resolution < 0)
		resolution = 0;
	if (resolution > 2)
		resolution = 2;

	s6e36w3x01_set_resolution(dsim->id, resolution);
	lcd->resolution = resolution;

	pr_info("%s:val[%d]\n", __func__, lcd->resolution);

	return size;
}

static ssize_t s6e36w3x01_resolution_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n,", lcd->resolution);
}

static ssize_t s6e36w3x01_esd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n,", lcd->esd_cnt);
}

static ssize_t s6e36w3x01_esd_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_err("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	cancel_delayed_work(&lcd->esd_dwork);
	schedule_delayed_work(&lcd->esd_dwork, 0);

	return size;
}

#define	DDI_DEBUG_REG	0x9c
#define	DDI_DEBUG_REG_SIZE	512
int s6e36w3x01_read_mtp_reg(int id, u32 addr, char* buffer, u32 size);
static ssize_t s6e36w3x01_ddi_debug_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	char read_buf[DDI_DEBUG_REG_SIZE] = {0, };
	char show_buf[32] = {0, };
	int ret = 0;
	int i;

	ret = s6e36w3x01_read_mtp_reg(dsim->id,
		DDI_DEBUG_REG, &read_buf[0], DDI_DEBUG_REG_SIZE);
	if (ret) {
		pr_err("%s: read failed 0x9c register[%d]\n", __func__, ret);
		return -EIO;
	}


	for (i = 0; i < DDI_DEBUG_REG_SIZE; i++) {
		if (i == 0) {
			ret += sprintf(show_buf, "[%03d]", i);
			strcat(buf, show_buf);
		} else if (!(i %8)) {
			ret += sprintf(show_buf, "\n[%03d]", i);
			strcat(buf, show_buf);
		}
		ret += sprintf(show_buf, " 0x%02x,", read_buf[i]);
		strcat(buf, show_buf);
	}

	strcat(buf, "\n");
	ret += strlen("\n");

	return ret;
}

void s6e36w3x01_write_ddi_debug(struct s6e36w3x01 *lcd, int enable);
static ssize_t s6e36w3x01_ddi_debug_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int enable;

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	sscanf(buf, "%d", &enable);

	s6e36w3x01_write_ddi_debug(lcd, enable);

	pr_info("%s:[%d]\n", __func__, enable);

	return size;
}

#define 	MAX_MTP_READ_SIZE	0xff
static unsigned char mtp_read_data[MAX_MTP_READ_SIZE] = {0, };
static int read_data_length = 0;
static int read_data_addr;
static int read_data_offset;
extern int s6e36w3x01_read_mtp_reg(int id, u32 addr, char* buffer, u32 size);
static ssize_t s6e36w3x01_read_mtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	char buffer[LDI_MTP4_LEN*8] = {0, };
	int i, ret = 0;

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_err("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	ret = s6e36w3x01_read_mtp_reg(dsim->id,
		read_data_addr+read_data_offset, &mtp_read_data[0], read_data_length);
	if (ret) {
		pr_err("%s: read failed[%d]\n", __func__, ret);
		return -EIO;
	}

	for ( i = 0; i < read_data_length; i++) {
		if ((i !=0) && !(i%8)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}

		ret += sprintf(buffer, "0x%02x", mtp_read_data[i]);
		strcat(buf, buffer);

		if ( i < (read_data_length-1)) {
			strcat(buf, ", ");
			ret += strlen(", ");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");

	pr_info("%s: length=%d\n", __func__, read_data_length);
	pr_info("%s\n", buf);

	return ret;
}

static ssize_t s6e36w3x01_read_mtp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	u32 buff[3] = {0, }; //addr, size, offset
	int i;

	sscanf(buf, "0x%x, 0x%x, 0x%x", &buff[0], &buff[1], &buff[2]);
	pr_info("%s: 0x%x, 0x%x, 0x%x\n", __func__, buff[0], buff[1], buff[2]);

	for (i = 0; i < 3; i++) {
		if (buff[i] > MAX_MTP_READ_SIZE)
			return -EINVAL;
	}

	read_data_addr = buff[0];
	read_data_length = buff[1];
	read_data_offset = buff[2];

	pr_info("%s:addr[0x%x] length[%d] offset[0x%x]\n", __func__, buff[0], buff[1], buff[2]);

	return size;
}

static int dim_table_num = 0;
#define MAX_DIMMING_TABLE	4
#define DIMMING_TABLE_PRINT_SIZE	17
static ssize_t s6e36w3x01_dimming_table_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	unsigned long value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0) {
		dev_err(lcd->dev,
			"%s: failed to read parameter value\n",
			__func__);
		return -EIO;
	}

	if (value > MAX_DIMMING_TABLE) {
		dev_err(lcd->dev, "%s: Invalid value. [%d]\n",
			__func__, (int)value);
		return -EIO;
	}

	dim_table_num = value;

	dev_info(lcd->dev, "%s: dimming_table[%d]\n",
			__func__, dim_table_num);

	return size;
}

static ssize_t s6e36w3x01_dimming_table_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct SmtDimInfo *diminfo = NULL;

	char buffer[32];
	int ret = 0;
	int i, j;
	int max_cnt;

	diminfo = (void *)s6e36w3x01_dimming_info;

	if (dim_table_num == 0) {
		i = 0;
		max_cnt = DIMMING_TABLE_PRINT_SIZE;
	} else if (dim_table_num == 1) {
		i = DIMMING_TABLE_PRINT_SIZE;
		max_cnt = DIMMING_TABLE_PRINT_SIZE * 2;
	} else if (dim_table_num == 2)  {
		i = DIMMING_TABLE_PRINT_SIZE * 2;
		max_cnt = DIMMING_TABLE_PRINT_SIZE * 3;
	} else if (dim_table_num ==3) {
		i = DIMMING_TABLE_PRINT_SIZE * 3;
		max_cnt = DIMMING_TABLE_PRINT_SIZE * 4;
	} else if (dim_table_num ==4) {
		i = DIMMING_TABLE_PRINT_SIZE * 4;
		max_cnt = MAX_GAMMA_CNT;
	}

	for (; i < max_cnt; i++) {
		ret += sprintf(buffer, "[Gtbl][%3d] : ", diminfo[i].br);
		strcat(buf, buffer);

		for(j =0; j < GAMMA_CMD_CNT ; j++) {
				ret += sprintf(buffer, "0x%02x, ", s6e36w3x01_gamma_set[i][j]);
				strcat(buf, buffer);
			}
		strcat(buf, "\n");
		ret += strlen("\n");
	}

	return ret;
}

static ssize_t s6e36w3x01_cr_map_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	char buffer[32];
	int ret = 0;
	int i;

	for (i = 0; i <= MAX_BRIGHTNESS; i++) {
		ret += sprintf(buffer, " %3d,", candela_tbl[lcd->br_map[i]]);
		strcat(buf, buffer);
		if ((i == 0) || !(i %10)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");
	pr_info("%s:%s\n", __func__, buf);

	return ret;
}

static ssize_t s6e36w3x01_br_map_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	char buffer[32];
	int ret = 0;
	int i;

	for (i = 0; i <= MAX_BRIGHTNESS; i++) {
		ret += sprintf(buffer, " %3d,", lcd->br_map[i]);
		strcat(buf, buffer);
		if ((i == 0) || !(i %10)) {
			strcat(buf, "\n");
			ret += strlen("\n");
		}
	}

	strcat(buf, "\n");
	ret += strlen("\n");
	pr_info("%s:%s\n", __func__, buf);

	return ret;
}


static ssize_t s6e36w3x01_cell_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;

	char temp[LDI_MTP4_MAX_PARA];
	char result_buff[CELL_ID_LEN+1];

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	memset(&temp[0], 0x00, LDI_MTP4_MAX_PARA);
	s6e36w3x01_read_mtp_reg(dsim->id, LDI_MTP4, &temp[0], LDI_MTP4_MAX_PARA);
	sprintf(result_buff, "%02x%02x%02x%02x%02x%02x%02x",
				temp[4], temp[5], temp[6], temp[7],
				temp[8], temp[9], temp[10]);
	strcat(buf, result_buff);

	memset(&temp[0], 0x00, WHITE_COLOR_LEN);
	s6e36w3x01_read_mtp_reg(dsim->id, LDI_WHITE_COLOR, &temp[0], WHITE_COLOR_LEN);
	sprintf(result_buff, "%02x%02x%02x%02x\n",
				temp[0], temp[1], temp[2], temp[3]);
	strcat(buf, result_buff);

	pr_info("%s:[%s]", __func__, buf);

	return strlen(buf);
}

static ssize_t s6e36w3x01_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", lcd->hbm_on ? "on" : "off");
}

static ssize_t s6e36w3x01_hbm_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;

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
		dev_warn(lcd->dev, "hbm control before lcd enable.\n");
		return -EPERM;
	}

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->mcd_on) {
		pr_info("%s:mcd enabled\n", __func__);
		return -EPERM;
	}

	s6e36w3x01_hbm_on(dsim->id);

	dev_info(lcd->dev, "HBM %s.\n", lcd->hbm_on ? "ON" : "OFF");

	return size;
}

static ssize_t s6e36w3x01_elvss_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", lcd->temp_stage);
}

extern int s6e36w3x01_temp_offset_comp(int id, unsigned int stage);
static ssize_t s6e36w3x01_elvss_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
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

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	lcd->temp_stage = value;

	s6e36w3x01_temp_offset_comp(dsim->id, lcd->temp_stage);

	dev_info(lcd->dev, "ELVSS temp stage[%d].\n", lcd->temp_stage);

	return size;
}

static ssize_t s6e36w3x01_octa_chip_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	char temp[20];

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	memset(&panel->chip[0], 0x00, LDI_CHIP_LEN);

	s6e36w3x01_read_mtp_reg(dsim->id, LDI_CHIP_ID, &panel->chip[0], LDI_CHIP_LEN);

	sprintf(temp, "%02x%02x%02x%02x%02x\n",
				panel->chip[0], panel->chip[1], panel->chip[2],
				panel->chip[3], panel->chip[4]);
	strcat(buf, temp);

	pr_info("%s: %s\n", __func__, temp);

	return strlen(buf);
}

static ssize_t s6e36w3x01_lcd_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	char temp[LDI_ID_LEN];
	int ret;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	memset(&temp[0], 0, LDI_ID_LEN);
	ret = s6e36w3x01_read_mtp_reg(dsim->id, LDI_ID_REG, &temp[0], LDI_ID_LEN);
	if (ret) {
		pr_err("%s:failed read LDI_ID_REG[%d]\n", __func__, ret);
		return -EIO;
	}

	sprintf(buf, "SDC_%02x%02x%02x\n", temp[0], temp[1], temp[2]);
	pr_info("%s:[%s]", __func__, buf);

	return strlen(buf);
}

#ifdef DISPLAY_DQA
static ssize_t s6e36w3x01_display_model_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct decon_lcd *lcd_info = &dsim->lcd_info;
	char temp[16];

	sprintf(temp, "SDC_%s\n", lcd_info->model_name);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t s6e36w3x01_display_chipid_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	char temp[20];

	sprintf(temp, "%02x%02x%02x%02x%02x\n",
		panel->chip[0], panel->chip[1], panel->chip[2],
		panel->chip[3], panel->chip[4]);
	strcat(buf, temp);

	pr_info("%s: %s\n", __func__, temp);

	return strlen(buf);
}

static ssize_t s6e36w3x01_product_date_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	char temp[30];
	int year, month, day, hour, minute, second, msecond;

	year = (panel->date[0] >> 4) + 1;
	month = panel->date[0] & 0x0f;
	day = panel->date[1] & 0x0f;
	hour = panel->date[2] & 0x0f;
	minute = panel->date[3];
	second = panel->date[4];
	msecond = ((panel->date[5] & 0x0f) << 8) + panel->date[6];

	sprintf(temp, "201%d-%02d-%02d %02d:%02d:%02d:%04d\n", year, month, day, hour, minute, second, msecond);
	strcat(buf, temp);

	return strlen(buf);
}

static ssize_t s6e36w3x01_white_x_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->coordinate[0]);

	return ret;
}

static ssize_t s6e36w3x01_white_y_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->coordinate[1]);

	return ret;
}

static ssize_t s6e36w3x01_lcdm_id1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->id[0]);

	return ret;
}

static ssize_t s6e36w3x01_lcdm_id2_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->id[1]);

	return ret;
}

static ssize_t s6e36w3x01_lcdm_id3_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct panel_private *panel = &dsim->priv;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", panel->id[2]);

	return ret;
}

static ssize_t s6e36w3x01_pndsie_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dsim->comm_err_cnt);
	dsim->comm_err_cnt = 0;

	return ret;
}

static ssize_t s6e36w3x01_qct_no_te_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", dsim->decon_timeout_cnt);
	dsim->decon_timeout_cnt = 0;

	return ret;
}

static ssize_t s6e36w3x01_lbhd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", lcd->hbm_total_time);
	lcd->hbm_total_time = 0;

	return ret;
}

static ssize_t s6e36w3x01_lod_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", lcd->disp_total_time);
	lcd->disp_total_time = 0;

	return ret;
}

static ssize_t s6e36w3x01_daod_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", lcd->aod_total_time);
	lcd->aod_total_time = 0;

	return ret;
}

static ssize_t s6e36w3x01_dahl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", lcd->aod_high_time);
	lcd->aod_high_time = 0;

	return ret;
}

static ssize_t s6e36w3x01_dall_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", lcd->aod_low_time);
	lcd->disp_total_time = 0;

	return ret;
}

static ssize_t s6e36w3x01_locnt_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", lcd->disp_cnt);
	lcd->disp_cnt = 0;

	return ret;
}
#endif

extern ssize_t s6e36w3x01_sc_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size);
extern ssize_t s6e36w3x01_sc_show(struct device *dev,
				struct device_attribute *attr, char *buf);
static struct device_attribute s6e36w3x01_dev_attrs[] = {
	__ATTR(resolution, S_IRUGO | S_IWUSR, s6e36w3x01_resolution_show,
					s6e36w3x01_resolution_store),
	__ATTR(esd, S_IRUGO | S_IWUSR, s6e36w3x01_esd_show,
					s6e36w3x01_esd_store),
	__ATTR(sc, S_IRUGO | S_IWUSR, s6e36w3x01_sc_show, s6e36w3x01_sc_store),
	__ATTR(ddi_debug, S_IRUGO | S_IWUSR, s6e36w3x01_ddi_debug_show,
					s6e36w3x01_ddi_debug_store),
	__ATTR(alpm, S_IRUGO | S_IWUSR, s6e36w3x01_alpm_show,
					s6e36w3x01_alpm_store),
	__ATTR(hlpm, S_IRUGO | S_IWUSR, s6e36w3x01_hlpm_show,
					s6e36w3x01_hlpm_store),
	__ATTR(cell_id, S_IRUGO, s6e36w3x01_cell_id_show, NULL),
	__ATTR(hbm, S_IRUGO | S_IWUSR, s6e36w3x01_hbm_show,
					s6e36w3x01_hbm_store),
	__ATTR(elvss, S_IRUGO | S_IWUSR, s6e36w3x01_elvss_show,
					s6e36w3x01_elvss_store),
	__ATTR(chip_id, S_IRUGO, s6e36w3x01_octa_chip_id_show, NULL),
	__ATTR(cr_map, S_IRUGO, s6e36w3x01_cr_map_show, NULL),
	__ATTR(br_map, S_IRUGO, s6e36w3x01_br_map_show, NULL),
	__ATTR(dim_table, S_IRUGO | S_IWUSR, s6e36w3x01_dimming_table_show,
					s6e36w3x01_dimming_table_store),
	__ATTR(read_mtp, S_IRUGO | S_IWUSR, s6e36w3x01_read_mtp_show,
					s6e36w3x01_read_mtp_store),
	__ATTR(lcd_type, S_IRUGO , s6e36w3x01_lcd_type_show, NULL),
	__ATTR(mcd_test, S_IRUGO | S_IWUSR, s6e36w3x01_mcd_show, s6e36w3x01_mcd_store),
#ifdef DISPLAY_DQA
	__ATTR(disp_model, S_IRUGO , s6e36w3x01_display_model_show, NULL),
	__ATTR(disp_chipid, S_IRUGO, s6e36w3x01_display_chipid_show, NULL),
	__ATTR(prod_date, S_IRUGO , s6e36w3x01_product_date_show, NULL),
	__ATTR(white_x, S_IRUGO , s6e36w3x01_white_x_show, NULL),
	__ATTR(white_y, S_IRUGO , s6e36w3x01_white_y_show, NULL),
	__ATTR(lcdm_id1, S_IRUGO , s6e36w3x01_lcdm_id1_show, NULL),
	__ATTR(lcdm_id2, S_IRUGO , s6e36w3x01_lcdm_id2_show, NULL),
	__ATTR(lcdm_id3, S_IRUGO , s6e36w3x01_lcdm_id3_show, NULL),
	__ATTR(pndsie, S_IRUGO , s6e36w3x01_pndsie_show, NULL),
	__ATTR(qct_no_te, S_IRUGO, s6e36w3x01_qct_no_te_show, NULL),
	__ATTR(daod, S_IRUGO, s6e36w3x01_daod_show, NULL),
	__ATTR(dahl, S_IRUGO, s6e36w3x01_dahl_show, NULL),
	__ATTR(dall, S_IRUGO, s6e36w3x01_dall_show, NULL),
	__ATTR(lbhd, S_IRUGO , s6e36w3x01_lbhd_show, NULL),
	__ATTR(lod, S_IRUGO , s6e36w3x01_lod_show, NULL),
	__ATTR(locnt, S_IRUGO , s6e36w3x01_locnt_show, NULL),
#endif
};

extern void s6e36w3x01_mdnie_set(u32 id, enum mdnie_scenario scenario);
extern void s6e36w3x01_mdnie_outdoor_set(u32 id, enum mdnie_outdoor on);
static ssize_t s6e36w3x01_scenario_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->scenario);
}

static ssize_t s6e36w3x01_scenario_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
	struct mdnie_lite_device *mdnie = lcd->mdnie;
	int value;

	sscanf(buf, "%d", &value);

	dev_info(lcd->dev, "%s:cur[%d] new[%d]\n",
		__func__, mdnie->scenario, value);

	if (mdnie->scenario == value)
		return size;

	mdnie->scenario = value;

	if (lcd->alpm_on) {
		pr_info("%s:alpm is enabled\n", __func__);
		return -EPERM;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		return -EPERM;
	}

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	s6e36w3x01_mdnie_set(dsim->id, mdnie->scenario);

	schedule_delayed_work(&lcd->debug_dwork,
			msecs_to_jiffies(DEBUG_READ_DELAY));

	return size;
}

static ssize_t s6e36w3x01_outdoor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	return snprintf(buf, 4, "%d\n", mdnie->outdoor);
}

static ssize_t s6e36w3x01_outdoor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(dev);
	struct dsim_device *dsim = lcd->dsim;
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

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_info("%s:aod enabled\n", __func__);
		return -EPERM;
	}

	sscanf(buf, "%d", &value);

	if (value >= OUTDOOR_MAX) {
		dev_warn(lcd->dev, "invalid outdoor mode set\n");
		return -EINVAL;
	}
	mdnie->outdoor = value;

	s6e36w3x01_mdnie_outdoor_set(dsim->id, value);

	mdnie->outdoor = value;

	return size;
}

static struct device_attribute mdnie_attrs[] = {
	__ATTR(scenario, 0664, s6e36w3x01_scenario_show, s6e36w3x01_scenario_store),
	__ATTR(outdoor, 0664, s6e36w3x01_outdoor_show, s6e36w3x01_outdoor_store),
};

void s6e36w3x01_mdnie_lite_init(struct s6e36w3x01 *lcd)
{
	static struct class *mdnie_class;
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

static int s6e36w3x01_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int set_gamma_to_hbm(struct SmtDimInfo *brInfo, struct dim_data *dimData, u8 * hbm)
{
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;
	int i = 0;
	memset(result, 0, OLED_CMD_GAMMA_CNT);

	result[index++] = OLED_CMD_GAMMA;

	while (index < OLED_CMD_GAMMA_CNT) {
		if (((i >= 50) && (i < 65)) || ((i >= 68) && (i < 88)))
			result[index++] = hbm[i];
		i++;
	}
	for (i = 0; i < OLED_CMD_GAMMA_CNT; i++)
		dsim_info("%d : %d\n", i + 1, result[i]);
	return ret;
}

static int set_gamma_to_center(struct SmtDimInfo *brInfo)
{
	int i, j;
	int ret = 0;
	unsigned int index = 0;
	unsigned char *result = brInfo->gamma;

	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= 0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = (unsigned char)((center_gamma[i][j] >> 8) & 0x01);
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
			else {
				result[index++] = (unsigned char)center_gamma[i][j] & 0xff;
			}
		}
	}
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}

extern int generate_volt_table(struct dim_data *data);
extern int cal_gamma_from_index(struct dim_data *data, struct SmtDimInfo *brInfo);

static int s6e36w3x01_init_dimming(struct dsim_device *dsim, u8 * mtp, u8 * hbm)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *diminfo = NULL;
	static struct dim_data *dimming = NULL;
	char string_buf[1024];
	int i, j, k;
	int pos = 0;
	int ret = 0;
	short temp;
	int method = 0;
	int string_offset;
#ifdef READ_MTP_ERR_VER2
	unsigned int errcnt = 0;
#endif
	bool read_err = true;

	if (dimming == NULL) {
		dimming = (struct dim_data *)kmalloc(sizeof(struct dim_data), GFP_KERNEL);
		if (!dimming) {
			dsim_err("failed to allocate memory for dim data\n");
			ret = -ENOMEM;
			goto error;
		}
	}

	diminfo = (void *)s6e36w3x01_dimming_info;

	panel->dim_data = (void *)dimming;
	panel->dim_info = (void *)diminfo;

#ifdef READ_MTP_ERR_VER2
	for (i = 0; i <  S6E36W3_MTP_SIZE; i++)
	{
		if(mtp[i])
			read_err = false;
		if(mtp[i] == 0xff)
			errcnt++;
	}
	if(errcnt > 5)
		read_err = true;
#else
	for (i = 0; i <  S6E36W3_MTP_SIZE; i++)
	{
		if(mtp[i])
			read_err = false;
	}
#endif
	if(read_err) {
		dsim_info("%s: Wrong MTP offset! Use default one. \n", __func__);
		for (i = 0; i <  S6E36W3_MTP_SIZE; i++)
			mtp[i] = mtp_default[i];
	}

	for (j = 0; j < CI_MAX; j++) {
		pos += 1;
		temp = ((mtp[0] & (0x01<<(2-j))) ? -1 : 1) * mtp[pos];
		dimming->t_gamma[V255][j] = (int)center_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
	}
	pos += 1;

	for (i = V203; i >= 0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((mtp[pos] & 0x80) ? -1 : 1) * (mtp[pos] & 0x7f);
			dimming->t_gamma[i][j] = (int)center_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}

	/* for v0 */
	temp = ((mtp[pos] & 0xF0) >> 4) | ((mtp[pos] & 0x0F) << 4) | ((mtp[pos + 1] & 0xF0) << 4);
	for (i = 0; i < CI_MAX; i++)
		dimming->v0_mtp[i] = (temp >> (i * 4)) & 0x0f;

	pos++;
	/* for vt */
	temp = (mtp[pos] & 0x0F) | (mtp[pos + 1] & 0xF0) | ((mtp[pos + 1] & 0x0F) << 8);  // B, G, R

	for (i = 0; i < CI_MAX; i++)
		dimming->vt_mtp[i] = (temp >> (i * 4)) & 0x0f;
#ifdef SMART_DIMMING_DEBUG
	dsim_info("Center Gamma Info : \n");
	for (i = 0; i < VMAX; i++) {
		dsim_info("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			  dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE],
			  dimming->t_gamma[i][CI_RED], dimming->t_gamma[i][CI_GREEN], dimming->t_gamma[i][CI_BLUE]);
	}
#endif
	dsim_dbg("VT MTP : \n");
	dsim_dbg("Gamma : %3d %3d %3d : %3x %3x %3x\n",
		  dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE],
		  dimming->vt_mtp[CI_RED], dimming->vt_mtp[CI_GREEN], dimming->vt_mtp[CI_BLUE]);

	dsim_dbg("V0 MTP : \n");
	dsim_dbg("Gamma : %3d %3d %3d : %3x %3x %3x\n",
		  dimming->v0_mtp[CI_RED], dimming->v0_mtp[CI_GREEN], dimming->v0_mtp[CI_BLUE],
		  dimming->v0_mtp[CI_RED], dimming->v0_mtp[CI_GREEN], dimming->v0_mtp[CI_BLUE]);

	dsim_dbg("MTP Info : \n");
	for (i = 0; i < VMAX; i++) {
		dsim_dbg("Gamma : %3d %3d %3d : %3x %3x %3x\n",
			  dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE],
			  dimming->mtp[i][CI_RED], dimming->mtp[i][CI_GREEN], dimming->mtp[i][CI_BLUE]);
	}

	ret = generate_volt_table(dimming);
	if (ret) {
		dsim_info("[ERR:%s] failed to generate volt table\n", __func__);
		goto error;
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;

		if (method == DIMMING_METHOD_FILL_CENTER) {
			ret = set_gamma_to_center(&diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to get center gamma\n", __func__);
				goto error;
			}
		}
		else if (method == DIMMING_METHOD_FILL_HBM) {
			ret = set_gamma_to_hbm(&diminfo[i], dimming, hbm);
			if (ret) {
				dsim_err("%s : failed to get hbm gamma\n", __func__);
				goto error;
			}
		}
	}

	for (i = 0; i < MAX_BR_INFO; i++) {
		method = diminfo[i].way;
		if (method == DIMMING_METHOD_AID) {
			ret = cal_gamma_from_index(dimming, &diminfo[i]);
			if (ret) {
				dsim_err("%s : failed to calculate gamma : index : %d\n", __func__, i);
				goto error;
			}
		}
	}

	for (i = 0; i < (MAX_BR_INFO); i++) {
		memset(string_buf, 0, sizeof(string_buf));
		string_offset = sprintf(string_buf, "gamma[%3d] : ", diminfo[i].br);

		for (j = 0; j < GAMMA_CMD_CNT; j++){
			string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[i].gamma[j]);
			s6e36w3x01_gamma_set[i][j] = diminfo[i].gamma[j];
		}
		//dsim_info("[LCD] %s\n", string_buf);
	}

	for (k = 0; k < GAMMA_CMD_CNT; k++){
		diminfo[73].gamma[k] = MAX_GAMMA_SET[k];
		string_offset += sprintf(string_buf + string_offset, "%02x ", diminfo[73].gamma[k]);
		s6e36w3x01_gamma_set[73][k] = diminfo[73].gamma[k];
	}

error:
	return ret;

}

static int s6e36w3x01_read_init_info(struct dsim_device *dsim, unsigned char *mtp, unsigned char *hbm)
{
	struct panel_private *panel = &dsim->priv;
	unsigned char buf[S6E36W3_MTP_DATE_SIZE] = { 0, };
	unsigned char bufForCoordi[S6E36W3_COORDINATE_LEN] = { 0, };
	unsigned char bufForProductDate[S6E36W3_PRODUCT_DATE_LEN] = {0, };
	int i = 0;
	int ret = 0;

	s6e36w3x01_testkey_enable(dsim->id);

	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,  S6E36W3_ID_REG,  S6E36W3_ID_LEN, panel->id);
	if (ret < 0) {
		dsim_err("%s : can't find connected panel. check panel connection\n", __func__);
		goto read_fail;
	}

	dsim_info("%s: READ ID:[0x%02x%02x%02x]\n",
		__func__, panel->id[0], panel->id[1], panel->id[2]);

	dsim->priv.current_model = (dsim->priv.id[1] >> 4) & 0x0F;
	dsim->priv.panel_rev = dsim->priv.id[2] & 0x0F;
	dsim->priv.panel_line = (dsim->priv.id[0] >> 6) & 0x03;
	dsim->priv.panel_material = dsim->priv.id[1] & 0x01;
	dsim_info("%s model is %d, panel rev:[%d], panel line:[%d], panel material:[%d]\n",
		__func__, dsim->priv.current_model, dsim->priv.panel_rev,
		dsim->priv.panel_line, dsim->priv.panel_material);

	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,  S6E36W3_MTP_ADDR,  S6E36W3_MTP_DATE_SIZE, buf);
	if (ret < 0) {
		dsim_err("failed to read mtp, check panel connection\n");
		goto read_fail;
	}
	memcpy(mtp, buf,  S6E36W3_MTP_SIZE);
	memcpy(panel->date, &buf[40], ARRAY_SIZE(panel->date));
	dsim_info("%s:READ MTP SIZE : %d\n", __func__,  S6E36W3_MTP_SIZE);
	dsim_info("%s:=========== MTP INFO =========== \n", __func__);
	for (i = 0; i <  S6E36W3_MTP_SIZE; i++)
		dsim_info("%s:MTP[%2d] : %2d : %2x\n", __func__, i, mtp[i], mtp[i]);

	// coordinate
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,  S6E36W3_COORDINATE_REG,  S6E36W3_COORDINATE_LEN, bufForCoordi);
	if (ret < 0) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];      /* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];      /* Y */
	dsim_info("%s:READ coordi:[0x%x 0x%x]\n", __func__, panel->coordinate[0], panel->coordinate[1]);

	// product date
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, S6E36W3_PRODUCT_DATE_REG, S6E36W3_PRODUCT_DATE_LEN, bufForProductDate);
	if (ret < 0) {
		dsim_err("fail to read product date on command.\n");
		goto read_fail;
	}
	memcpy(&dsim->priv.date, &bufForProductDate[S6E36W3_COORDINATE_LEN], S6E36W3_PRODUCT_DATE_LEN - S6E36W3_COORDINATE_LEN);
	dsim_info("%s:READ product date:[%d %d %d %d %d %d %d]\n", __func__,
		panel->date[0], panel->date[1], panel->date[2], panel->date[3], panel->date[4], panel->date[5], panel->date[6]);

	// code
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,  S6E36W3_CODE_REG,  S6E36W3_CODE_LEN, panel->code);
	if (ret < 0) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("%s:READ code : ", __func__);
	for (i = 0; i <  S6E36W3_CODE_LEN; i++)
		dsim_info("%x, ", dsim->priv.code[i]);
	dsim_info("\n");
	// tset
	panel->tset_set[0] =  S6E36W3_TSET_REG;
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,  S6E36W3_TSET_REG,  S6E36W3_TSET_LEN, &(panel->tset_set[1]));
	if (ret < 0) {
		dsim_err("fail to read code on command.\n");
		goto read_fail;
	}
	dsim_info("%s:READ tset : ", __func__);
	for (i = 0; i <=  S6E36W3_TSET_LEN; i++)
		dsim_info("%x, ", panel->tset_set[i]);
	dsim_info("\n");

	 // elvss
	panel->elvss_set[0] = S6E36W3_ELVSS_REG;
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ,  S6E36W3_ELVSS_REG,  S6E36W3_ELVSS_LEN, &(panel->elvss_set[1]));
	if (ret < 0) {
		dsim_err("fail to read elvss on command.\n");
		goto read_fail;
	}
	dsim_info("%s:READ elvss : ", __func__);
	for (i = 0; i <=  S6E36W3_ELVSS_LEN; i++)
		dsim_info("%x \n", panel->elvss_set[i]);

	// chip id
	ret = dsim_read_data(dsim, MIPI_DSI_DCS_READ, LDI_CHIP_ID, LDI_CHIP_LEN, &(panel->chip[0]));
	if (ret < 0) {
		dsim_err("fail to read chip id on command.\n");
		goto read_fail;
	}

	ret = 0;

read_fail:
	s6e36w3x01_testkey_disable(dsim->id);

	return ret;
}

void s6e36w3x01_mdnie_restore(struct s6e36w3x01 *lcd, bool aod_state)
{
        struct mdnie_lite_device *mdnie = lcd->mdnie;
        struct dsim_device *dsim = lcd->dsim;

        if ((mdnie->scenario == SCENARIO_UI) ||
        (mdnie->scenario == SCENARIO_GRAY))
                return;

        if (aod_state) {
                switch (mdnie->scenario) {
                case SCENARIO_GRAY:
                case SCENARIO_GRAY_NEGATIVE:
                        s6e36w3x01_mdnie_set(dsim->id, SCENARIO_GRAY);
                        break;
                case SCENARIO_UI:
                case SCENARIO_GALLERY:
                case SCENARIO_VIDEO:
                case SCENARIO_VTCALL:
                case SCENARIO_CAMERA:
                case SCENARIO_BROWSER:
                case SCENARIO_NEGATIVE:
                case SCENARIO_EMAIL:
                case SCENARIO_EBOOK:
                case SCENARIO_CURTAIN:
                default:
                        s6e36w3x01_mdnie_set(dsim->id, SCENARIO_UI);
                        break;
                }
                usleep_range(40000, 41000);
        } else {
                usleep_range(40000, 41000);
                s6e36w3x01_mdnie_set(dsim->id, mdnie->scenario);
        }
}

extern int s6e36w3x01_gamma_ctrl(struct s6e36w3x01 *lcd, u32 backlightlevel);
static int s6e36w3x01_update_brightness(struct s6e36w3x01 *lcd)
{
	struct dsim_device *dsim = lcd->dsim;
	struct backlight_device *bd = lcd->bd;
	int brightness = bd->props.brightness, ret = 0;

	pr_info("%s[%d]lv[%d]pwr[%d]aod[%d]hnit[%d]hbm[%d]\n",
		__func__, brightness, lcd->br_map[brightness],
		lcd->power,lcd->aod_mode, lcd->hlpm_nit,
		lcd->hbm_on);

	if (lcd->hbm_on)
		ret = s6e36w3x01_hbm_on(dsim->id);
	else
		ret = s6e36w3x01_gamma_ctrl(lcd, lcd->br_map[brightness]);

	return ret;
}

static int s6e36w3x01_set_brightness(struct backlight_device *bd)
{
	struct s6e36w3x01 *lcd = bl_get_data(bd);
	int brightness = bd->props.brightness, ret = 0;
	struct dsim_device *dsim;

	if (lcd == NULL) {
		pr_err("%s: LCD is NULL\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	dsim = lcd->dsim;
	if (dsim == NULL) {
		pr_err("%s: dsim is NULL\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	if (brightness < MIN_BRIGHTNESS || brightness > MAX_BRIGHTNESS) {
		pr_err("%s:Brightness should be in the range of %d ~ %d\n",
			__func__, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
		ret = -EINVAL;
		goto out;
	}

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_debug("%s:invalid power[%d]\n", __func__, lcd->power);
		ret = 0;
		goto out;
	}

	if (lcd->aod_mode != AOD_DISABLE) {
		pr_err("%s:aod enabled\n", __func__);
		ret = 0;
		goto out;
	}

	ret = s6e36w3x01_update_brightness(lcd);
	if (ret) {
		pr_err("%s:failed change_brightness\n", __func__);
		goto out;
	}

out:
	return ret;
}

static int s6e36w3x01_get_power(struct lcd_device *ld)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(&ld->dev);

	pr_debug("%s [%d]\n", __func__, lcd->power);

	return lcd->power;
}

static int s6e36w3x01_set_power(struct lcd_device *ld, int power)
{
	struct s6e36w3x01 *lcd = dev_get_drvdata(&ld->dev);

	if (power > FB_BLANK_POWERDOWN) {
		pr_err("%s: invalid power state.[%d]\n", __func__, power);
		return -EINVAL;
	}

	lcd->power = power;

	pr_info("%s[%s]\n", __func__, power_state[lcd->power]);

	return 0;
}

static irqreturn_t s6e36w3x01_esd_interrupt(int irq, void *dev_id)
{
	struct s6e36w3x01 *lcd = dev_id;
	struct dsim_device *dsim = lcd->dsim;
	struct dsim_resources *res = &dsim->res;
	int ret;

	if (lcd->power > FB_BLANK_NORMAL) {
		dev_dbg(lcd->dev, "%s:invalid power[%d]\n",
			__func__, lcd->power);
		return IRQ_HANDLED;
	}

	ret = gpio_get_value(res->err_fg);
	if (!ret) {
		dev_dbg(lcd->dev, "%s: Invalid interrupt\n", __func__);
		return IRQ_HANDLED;
	}

	cancel_delayed_work(&lcd->esd_dwork);
	schedule_delayed_work(&lcd->esd_dwork, 0);

	return IRQ_HANDLED;
}

static struct lcd_ops s6e36w3x01_lcd_ops = {
	.get_power = s6e36w3x01_get_power,
	.set_power = s6e36w3x01_set_power,
};


static const struct backlight_ops s6e36w3x01_backlight_ops = {
	.get_brightness = s6e36w3x01_get_brightness,
	.update_status = s6e36w3x01_set_brightness,
};

static int s6e36w3x01_dimming_alloc(struct s6e36w3x01 *lcd)
{
	int start = 0, end, i, offset = 0;
	int ret = 0;

	lcd->dimming = devm_kzalloc(lcd->dev,
				sizeof(*lcd->dimming), GFP_KERNEL);
	if (lcd->dimming == NULL) {
		pr_err("failed to allocate dimming.\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		lcd->gamma_tbl[i] = (unsigned char *)
			kzalloc(sizeof(unsigned char) * GAMMA_CMD_CNT,
			GFP_KERNEL);
		if (!lcd->gamma_tbl[i]) {
			pr_err("failed to allocate gamma_tbl\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	lcd->br_map = devm_kzalloc(lcd->dev,
		sizeof(unsigned char) * (MAX_BRIGHTNESS + 1), GFP_KERNEL);
	if (lcd->br_map == NULL) {
		pr_err("failed to allocate br_map\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < DIMMING_COUNT; i++) {
		end = br_convert[offset++];
		memset(&lcd->br_map[start], i, end - start + 1);
		start = end + 1;
	}

out:
	return ret;
}

#ifdef CONFIG_SLEEP_MONITOR
int s6e36w3x01_sleep_monitor_cb(void *priv, unsigned int *raw_val,
		int check_level, int caller_type)
{
	struct s6e36w3x01 *lcd = priv;
	int state = DEVICE_UNKNOWN;
	int mask = (1 << SLEEP_MONITOR_DEVICE_BIT_WIDTH) - 1;
	unsigned int cnt = 0;
	int ret;

	switch (lcd->power) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		state = DEVICE_ON_ACTIVE1;
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		state = DEVICE_ON_LOW_POWER;
		break;
	case FB_BLANK_POWERDOWN:
		state = DEVICE_POWER_OFF;
		break;
	}

	switch (caller_type) {
	case SLEEP_MONITOR_CALL_SUSPEND:
	case SLEEP_MONITOR_CALL_RESUME:
		cnt = lcd->act_cnt;
		if (state == DEVICE_ON_LOW_POWER)
			cnt++;
		break;
	case SLEEP_MONITOR_CALL_ETC:
		break;
	default:
		break;
	}

	*raw_val = cnt | state << 24;

	/* panel on count 1~15*/
	if (cnt > mask)
		ret = mask;
	else
		ret = cnt;

	pr_info("%s: caller[%d], dpms[%d] panel on[%d], raw[0x%x]\n",
			__func__, caller_type, lcd->power, ret, *raw_val);

	lcd->act_cnt = 0;

	return ret;
}

static struct sleep_monitor_ops s6e36w3x01_sleep_monitor_ops = {
	.read_cb_func = s6e36w3x01_sleep_monitor_cb,
};
#endif

int s6e36w3x01_afpc_get_panel(struct dsim_device *dsim, struct afpc_panel_v2 *afpc_panel)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);
	unsigned char	chip[LDI_CHIP_LEN];
	int ret = 0;

	if (lcd->power > FB_BLANK_NORMAL) {
		pr_info("%s:invalid power[%d]\n", __func__, lcd->power);
		ret = -1;
	}

	memset(&chip, 0x00, LDI_CHIP_LEN);
	s6e36w3x01_read_mtp_reg(dsim->id, LDI_CHIP_ID, &chip[0], LDI_CHIP_LEN);

	sprintf(afpc_panel->id, "%02x%02x%02x%02x%02x\n",
					chip[0], chip[1], chip[2], chip[3], chip[4]);
	pr_info("%s: %s\n", __func__, afpc_panel->id);

	return ret;
}

extern int s6e36w3x01_sc_alloc(struct s6e36w3x01 *lcd);
static int s6e36w3x01_probe(struct dsim_device *dsim)
{
	struct dsim_resources *res = &dsim->res;
	struct s6e36w3x01 *lcd;
	int ret, i;
	unsigned char mtp[ S6E36W3_MTP_SIZE] = { 0, };
	unsigned char hbm[ S6E36W3_HBMGAMMA_LEN] = { 0, };

	pr_info("%s\n", __func__);

	if (get_panel_id() == -1) {
		pr_err("%s: No lcd attached!\n", __func__);
		return -ENODEV;
	}

	lcd = devm_kzalloc(dsim->dev,
			sizeof(struct s6e36w3x01), GFP_KERNEL);
	if (lcd == NULL) {
		pr_err("%s: failed to allocate s6e36w3x01 structure.\n", __func__);
		return -ENOMEM;
	}

	lcd->dev = dsim->dev;
	lcd->dsim = dsim;
	mutex_init(&lcd->mipi_lock);

	ret = s6e36w3x01_dimming_alloc(lcd);
	if (ret) {
		pr_err("%s: failed to allocate s6e36w3x01 structure.\n", __func__);
		ret = -ENOMEM;
		goto err_dimming;
	}

	lcd->bd = backlight_device_register(BACKLIGHT_DEV_NAME,
		lcd->dev, lcd, &s6e36w3x01_backlight_ops, NULL);
	if (IS_ERR(lcd->bd)) {
		pr_err("%s: failed to register backlight device[%d]\n",
			__func__, (int)PTR_ERR(lcd->bd));
		ret = PTR_ERR(lcd->bd);
		goto err_bd;
	}
	lcd->bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd->bd->props.brightness = DEFAULT_BRIGHTNESS;

	dsim->ld = lcd_device_register(LCD_DEV_NAME,
			lcd->dev, lcd, &s6e36w3x01_lcd_ops);
	if (IS_ERR(dsim->ld)) {
		pr_err("%s: failed to register lcd ops[%d]\n",
			__func__, (int)PTR_ERR(dsim->ld));
		ret = PTR_ERR(lcd->bd);
		goto err_ld;
	}
	lcd->ld = dsim->ld;

	lcd->esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(lcd->esd_class)) {
		dev_err(lcd->dev, "%s: Failed to create esd_class[%d]\n",
			__func__, (int)PTR_ERR(lcd->esd_class));
		ret = PTR_ERR(lcd->esd_class);
		goto err_esd_class;
	}

	esd_dev = device_create(lcd->esd_class, NULL, 0, NULL, "esd");
	if (IS_ERR(esd_dev)) {
		dev_err(lcd->dev, "Failed to create esd_dev\n");
		goto err_esd_dev;
	}

	if (res->err_fg > 0) {
		ret = gpio_request_one(res->err_fg, GPIOF_IN, "err_fg");
		if (ret < 0) {
			dev_err(lcd->dev, "failed to get err_fg GPIO\n");
			goto err_esd_dev;
		}
		INIT_DELAYED_WORK(&lcd->esd_dwork, s6e36w3x01_esd_dwork);
		lcd->esd_irq = gpio_to_irq(res->err_fg);
		dev_dbg(lcd->dev, "esd_irq_num [%d]\n", lcd->esd_irq);
		ret = request_threaded_irq(lcd->esd_irq, NULL, s6e36w3x01_esd_interrupt,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT , "err_fg", lcd);
		if (ret < 0) {
			dev_err(lcd->dev, "failed to request det irq.\n");
			goto err_err_fg;
		}
	}

	for (i = 0; i < ARRAY_SIZE(s6e36w3x01_dev_attrs); i++) {
		ret = device_create_file(&lcd->ld->dev, &s6e36w3x01_dev_attrs[i]);
		if (ret < 0) {
			dev_err(&lcd->ld->dev, "failed to add ld dev sysfs entries\n");
			for (i--; i >= 0; i--)
				device_remove_file(&lcd->ld->dev, &s6e36w3x01_dev_attrs[i]);
			goto err_create_file;
		}
	}

	ret = s6e36w3x01_read_init_info(dsim, mtp, hbm);
	if (ret)
		pr_err("%s : failed to generate gamma tablen\n", __func__);

	ret = s6e36w3x01_init_dimming(dsim, mtp, hbm);

	s6e36w3x01_mdnie_lite_init(lcd);

	ret = s6e36w3x01_sc_alloc(lcd);
	if (ret) {
		pr_err("%s : failed to sc alloc[%d]\n", __func__, ret);
		goto err_create_file;
	}
#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_register_ops(lcd, &s6e36w3x01_sleep_monitor_ops,
		SLEEP_MONITOR_LCD);
	lcd->act_cnt = 1;
#endif
	INIT_DELAYED_WORK(&lcd->debug_dwork, s6e36w3x01_debug_dwork);
	schedule_delayed_work(&lcd->debug_dwork,
			msecs_to_jiffies(DEBUG_READ_DELAY));

	pr_info("%s done\n", __func__);

	return 0;

err_create_file:
err_err_fg:
	device_destroy(lcd->esd_class, esd_dev->devt);
err_esd_dev:
	class_destroy(lcd->esd_class);
err_esd_class:
	lcd_device_unregister(lcd->ld);
err_ld:
	backlight_device_unregister(lcd->bd);
err_bd:
	devm_kfree(dsim->dev, lcd->br_map);
	for (i = 0; i < MAX_GAMMA_CNT; i++)
		if (lcd->gamma_tbl[i])
			devm_kfree(dsim->dev, lcd->gamma_tbl[i]);
err_dimming:
	mutex_destroy(&lcd->mipi_lock);
	devm_kfree(dsim->dev, lcd);
	return ret;
}

extern void s6e36w3x01_init_ctrl(int id, struct decon_lcd * lcd);
int s6e36w3x01_init(struct dsim_device *dsim)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);
	struct mdnie_lite_device *mdnie = lcd->mdnie;

	s6e36w3x01_init_ctrl(dsim->id, &dsim->lcd_info);

	if (mdnie->outdoor == OUTDOOR_ON)
		s6e36w3x01_mdnie_outdoor_set(dsim->id, mdnie->outdoor);
	else
		s6e36w3x01_mdnie_set(dsim->id, mdnie->scenario);

	pr_info("%s\n", __func__);

	return 0;
}

extern void s6e36w3x01_enable(int id);
extern void s6e36w3x01_print_debug_reg(int id);
static int s6e36w3x01_displayon(struct dsim_device *dsim)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);
	struct dsim_resources *res = &dsim->res;

	/* restore brightness level */
	s6e36w3x01_update_brightness(lcd);

	s6e36w3x01_enable(dsim->id);

	schedule_delayed_work(&lcd->debug_dwork,
			msecs_to_jiffies(DEBUG_READ_DELAY));

#ifdef CONFIG_SLEEP_MONITOR
	lcd->act_cnt++;
#endif
	lcd->disp_stime = ktime_get();
	lcd->disp_cnt++;

	if (res->err_fg > 0)
		enable_irq(lcd->esd_irq);

	lcd->panel_sc_state = false;
	lcd->panel_icon_state = false;
	lcd->power = FB_BLANK_UNBLANK;

	pr_info("%s[%s]\n", __func__, power_state[lcd->power]);

	return 0;
}

extern void s6e36w3x01_disable(int id);
static int s6e36w3x01_suspend(struct dsim_device *dsim)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);
	struct dsim_resources *res = &dsim->res;
	ktime_t disp_time;

	if (res->err_fg > 0) {
		disable_irq(lcd->esd_irq);
		cancel_delayed_work_sync(&lcd->esd_dwork);
	}
	cancel_delayed_work_sync(&lcd->debug_dwork);

	lcd->power = FB_BLANK_POWERDOWN;
	s6e36w3x01_disable(dsim->id);

	disp_time = ktime_get();
	lcd->disp_total_time += ((unsigned int)ktime_ms_delta(disp_time, lcd->disp_stime) / 1000);

	pr_info("%s[%s]\n", __func__, power_state[lcd->power]);

	return 0;
}

static int s6e36w3x01_resume(struct dsim_device *dsim)
{
	return 0;
}

static int s6e36w3x01_dump(struct dsim_device *dsim)
{
	struct lcd_device *panel = dsim->ld;
	struct s6e36w3x01 *lcd = dev_get_drvdata(&panel->dev);

	pr_info("%s:power=[%s]\n", __func__, power_state[lcd->power]);
	s6e36w3x01_print_debug_reg(dsim->id);

	return 0;
}

struct dsim_lcd_driver s6e36w3x01_mipi_lcd_driver = {
	.probe		= s6e36w3x01_probe,
	.init		= s6e36w3x01_init,
	.displayon	= s6e36w3x01_displayon,
	.suspend		= s6e36w3x01_suspend,
	.resume		= s6e36w3x01_resume,
	.dump		= s6e36w3x01_dump,
	.aod_time_set = s6e36w3x01_set_aod_time,
	.aod_analog_set = s6e36w3x01_set_aod_analog,
	.aod_digital_set = s6e36w3x01_set_aod_digital,
	.aod_icon_set = s6e36w3x01_set_aod_icon,
	.aod_move_set = s6e36w3x01_set_aod_move,
	.aod_mode_enter = s6e36w3x01_aod_enter,
	.aod_mode_exit = s6e36w3x01_aod_exit,
	.afpc_compensation_set = s6e36w3x01_afpc_set_compensation,
	.afpc_panel_get = s6e36w3x01_afpc_get_panel,
};
