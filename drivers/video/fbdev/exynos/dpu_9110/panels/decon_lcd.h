/* drivers/video/exynos_decon/lcd.h
 *
 * Copyright (c) 2011 Samsung Electronics
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_LCD__
#define __DECON_LCD__

#define AFPC_PANEL_ID_LEN	11

enum decon_psr_mode {
	DECON_VIDEO_MODE = 0,
	DECON_DP_PSR_MODE = 1,
	DECON_MIPI_COMMAND_MODE = 2,
};

/* Mic ratio: 0: 1/2 ratio, 1: 1/3 ratio */
enum decon_mic_comp_ratio {
	MIC_COMP_RATIO_1_2 = 0,
	MIC_COMP_RATIO_1_3 = 1,
	MIC_COMP_BYPASS
};

enum mic_ver {
	MIC_VER_1_1,
	MIC_VER_1_2,
	MIC_VER_2_0,
};

enum type_of_ddi {
	TYPE_OF_SM_DDI = 0,
	TYPE_OF_MAGNA_DDI,
	TYPE_OF_NORMAL_DDI,
};

#define MAX_RES_NUMBER		5
#define HDR_CAPA_NUM		4

struct lcd_res_info {
	unsigned int width;
	unsigned int height;
	unsigned int dsc_en;
	unsigned int dsc_width;
	unsigned int dsc_height;
};

/* multi-resolution */
struct lcd_mres_info {
	unsigned int mres_en;
	unsigned int mres_number;
	struct lcd_res_info res_info[MAX_RES_NUMBER];
};

struct lcd_hdr_info {
	unsigned int hdr_num;
	unsigned int hdr_type[HDR_CAPA_NUM];
	unsigned int hdr_max_luma;
	unsigned int hdr_max_avg_luma;
	unsigned int hdr_min_luma;
};

struct stdphy_pms {
	unsigned int p;
	unsigned int m;
	unsigned int s;
#if defined(CONFIG_SOC_EXYNOS9110)
	unsigned int k;
#endif
#if defined(CONFIG_EXYNOS_DSIM_DITHER)
	unsigned int mfr;
	unsigned int mrr;
	unsigned int sel_pf;
	unsigned int icp;
	unsigned int afc_enb;
	unsigned int extafc;
	unsigned int feed_en;
	unsigned int fsel;
	unsigned int fout_mask;
	unsigned int rsel;
#endif
};

struct decon_lcd {
	enum decon_psr_mode mode;
	unsigned int vfp;
	unsigned int vbp;
	unsigned int hfp;
	unsigned int hbp;

	unsigned int vsa;
	unsigned int hsa;

	unsigned int xres;
	unsigned int yres;

	unsigned int width;
	unsigned int height;

	unsigned int hs_clk;
	struct stdphy_pms dphy_pms;
	unsigned int esc_clk;

	unsigned int fps;
	unsigned int mic_enabled;
	enum decon_mic_comp_ratio mic_ratio;
	unsigned int dsc_enabled;
	unsigned int dsc_cnt;
	unsigned int dsc_slice_num;
	unsigned int dsc_slice_h;
	enum mic_ver mic_ver;
	enum type_of_ddi ddi_type;
	unsigned int data_lane;
	unsigned int cmd_underrun_lp_ref[MAX_RES_NUMBER];
	unsigned int vt_compensation;
	unsigned int mres_mode;
	struct lcd_mres_info dt_lcd_mres;
	struct lcd_hdr_info dt_lcd_hdr;
	unsigned int bpc;
	int power;
#ifdef CONFIG_TIZEN
	const char* name;
	const char* model_name;
#endif
};

#define SCLK_MAX_BUF 	3
#define SCLK_MAX_POS 	2
#define SCLK_MAX_TIME 	4


enum metadata_ops {
	METADATA_OP_AOD_SET_INFO,
	METADATA_OP_AOD_SET_STATE,
	METADATA_OP_MAX,
};

enum aod_request_type {
	AOD_SET_CONFIG,
	AOD_UPDATE_DATA,
};

enum aod_state {
	AOD_OFF,
	AOD_ENTER,
	AOD_UPDATE_REQ,
	AOD_UPDATE_DONE,
	AOD_EXIT,
};

enum aod_mode {
	AOD_DISABLE,
	AOD_ALPM,
	AOD_HLPM,
	AOD_SCLK_ANALOG,
	AOD_SCLK_DIGITAL,
};

enum aod_data_cmd {
	AOD_DATA_ENABLE,
	AOD_DATA_SET,
	AOD_DATA_POS,
};

struct sclk_analog_cfg {
	unsigned int	pos[SCLK_MAX_POS];
	unsigned int	timestamp[SCLK_MAX_TIME];
	unsigned int	rate;
	unsigned int	buf_id[SCLK_MAX_BUF];
	unsigned long	addr[SCLK_MAX_BUF];
};

struct sclk_analog_hand {
	unsigned int	x;
	unsigned int	y;
	unsigned int	width;
	unsigned int	height;
	unsigned int	mask_x;
	unsigned int	mask_y;
	unsigned int	mask_w;
	unsigned int	mask_h;
	unsigned int	flip_enable;
	unsigned int 	layer_order;
	unsigned int 	layer_mask;
	unsigned int	motion;
	unsigned int	timestamp;
	unsigned int	buf_id;
	unsigned long	addr;
	void	*tbm_bo;
};

struct sclk_analog_cfg_v2 {
	enum aod_request_type req;
	unsigned int rate;
	unsigned int flip_start;
	unsigned int flip_end;
	struct sclk_analog_hand hour;
	struct sclk_analog_hand min;
	struct sclk_analog_hand sec;
};


struct sclk_digital_cfg {
	unsigned int circle_r;
	unsigned int circle1[SCLK_MAX_POS];
	unsigned int circle2[SCLK_MAX_POS];
	unsigned int color[SCLK_MAX_BUF];
	unsigned int rate;
};

struct sclk_digital_hand {
	unsigned int x[2];
	unsigned int y[2];
	unsigned int zero_digit;
	unsigned int font_index;
	unsigned int font_type;
	unsigned int mask;
	unsigned int blink_enable;
	int time_diff;
};

struct sclk_digital_font {
	unsigned int digit_w;
	unsigned int gitit_h;
	unsigned int colon_w;
	unsigned int colon_h;
};

struct sclk_digital_cfg_v2 {
	struct sclk_digital_hand hour;
	struct sclk_digital_hand min;
	struct sclk_digital_hand sec;
	struct sclk_digital_hand blink1;
	struct sclk_digital_hand blink2;
	struct sclk_digital_hand world_clock1_hour;
	struct sclk_digital_hand world_clock1_minute;
	struct sclk_digital_hand world_clock2_hour;
	struct sclk_digital_hand world_clock2_minute;
	struct sclk_digital_font font1;
	struct sclk_digital_font font2;
	enum aod_request_type req;
	unsigned int world_clock1_mask;
	unsigned int world_clock2_mask;
	unsigned int world_clock_position;
	unsigned int world_clock_y;
	unsigned int font_type;
	unsigned int font_format;
	unsigned int watch_format;
	unsigned int blink_rate;
	unsigned int blink_sync;
	unsigned int font_buf_id;
	unsigned long font_addr;
	void *font_tbm_bo;
};

struct sclk_time_cfg_v2 {
	enum aod_request_type req;
	unsigned int hour;
	unsigned int minute;
	unsigned int second;
	unsigned int millisecond;
};

struct sclk_icon_cfg_v2 {
	unsigned int req;
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
	unsigned int mask;
	unsigned long addr;
	void *tbm_bo;
};

struct sclk_move_cfg_v2 {
	unsigned int req;
	unsigned int x;
	unsigned int y;
	unsigned int w;
	unsigned int h;
	unsigned int clock_mask;
	unsigned int blink_mask;
	unsigned int icon_mask;
	unsigned int mask;
};

struct afpc_panel_v2 {
	char id[AFPC_PANEL_ID_LEN];
};

struct afpc_compensation_v2 {
	int fd;
	unsigned long addr;
	void *tbm_bo;
};

struct aod_config {
	enum aod_request_type req;
	enum aod_mode mode;
	union {
		struct sclk_analog_cfg analog_cfg;
		struct sclk_digital_cfg digital_cfg;
	};
};

struct decon_metadata {
	enum metadata_ops ops;
	union {
		enum aod_state state;
		struct aod_config aod_cfg;
	};
};


struct decon_dsc {
/* 04 */	unsigned int comp_cfg;
/* 05 */	unsigned int bit_per_pixel;
/* 06-07 */	unsigned int pic_height;
/* 08-09 */	unsigned int pic_width;
/* 10-11 */	unsigned int slice_height;
/* 12-13 */	unsigned int slice_width;
/* 14-15 */	unsigned int chunk_size;
/* 16-17 */	unsigned int initial_xmit_delay;
/* 18-19 */	unsigned int initial_dec_delay;
/* 21 */	unsigned int initial_scale_value;
/* 22-23 */	unsigned int scale_increment_interval;
/* 24-25 */	unsigned int scale_decrement_interval;
/* 27 */	unsigned int first_line_bpg_offset;
/* 28-29 */	unsigned int nfl_bpg_offset;
/* 30-31 */	unsigned int slice_bpg_offset;
/* 32-33 */	unsigned int initial_offset;
/* 34-35 */	unsigned int final_offset;
/* 58-59 */	unsigned int rc_range_parameters;

		unsigned int overlap_w;
		unsigned int width_per_enc;
		unsigned char *dec_pps_t;
};

#endif
