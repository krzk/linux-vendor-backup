 /*
 * hdmi_lib.h
 *
 * HDMI driver definition for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _HDMI_H_
#define _HDMI_H_

#include <linux/string.h>

#define HDMI_WP				0x58006000
#define HDMI_CORE_SYS		0x58006400
#define HDMI_CORE_AV		0x58006900
#define HDMI_HDCP			0x58007000

#define HDMI_WP_AUDIO_DATA		0x8Cul

#define DBG(format, ...) \
		printk(KERN_DEBUG "hdmi: " format, ## __VA_ARGS__)
#define ERR(format, ...) \
	printk(KERN_ERR "hdmi error: " format, ## __VA_ARGS__)

#define BITS_32(in_NbBits) \
		((((u32)1 << in_NbBits) - 1) | ((u32)1 << in_NbBits))

#define BITFIELD(in_UpBit, in_LowBit) \
		(BITS_32(in_UpBit) & ~((BITS_32(in_LowBit)) >> 1))

struct hdmi_irq_vector {
	u8	pllRecal;
	u8	pllUnlock;
	u8	pllLock;
	u8	phyDisconnect;
	u8	phyConnect;
	u8	phyShort5v;
	u8	videoEndFrame;
	u8	videoVsync;
	u8	fifoSampleRequest;
	u8	fifoOverflow;
	u8	fifoUnderflow;
	u8	ocpTimeOut;
	u8	core;
};

typedef enum HDMI_PhyPwr_label {
	HDMI_PHYPWRCMD_OFF = 0,
	HDMI_PHYPWRCMD_LDOON = 1,
	HDMI_PHYPWRCMD_TXON = 2
} HDMI_PhyPwr_t, *pHDMI_PhyPwr_t;

typedef enum HDMI_PllPwr_label {
	HDMI_PLLPWRCMD_ALLOFF = 0,
	HDMI_PLLPWRCMD_PLLONLY = 1,
	HDMI_PLLPWRCMD_BOTHON_ALLCLKS = 2,
	HDMI_PLLPWRCMD_BOTHON_NOPHYCLK = 3
} HDMI_PllPwr_t, *pHDMI_PllPwr_t;

enum hdmi_core_inputbus_width {
	HDMI_INPUT_8BIT = 0,
	HDMI_INPUT_10BIT = 1,
	HDMI_INPUT_12BIT = 2
};

enum hdmi_core_dither_trunc {
	HDMI_OUTPUTTRUNCATION_8BIT = 0,
	HDMI_OUTPUTTRUNCATION_10BIT = 1,
	HDMI_OUTPUTTRUNCATION_12BIT = 2,
	HDMI_OUTPUTDITHER_8BIT = 3,
	HDMI_OUTPUTDITHER_10BIT = 4,
	HDMI_OUTPUTDITHER_12BIT = 5
};

enum hdmi_core_deepcolor_ed {
	HDMI_DEEPCOLORPACKECTDISABLE = 0,
	HDMI_DEEPCOLORPACKECTENABLE = 1
};

enum hdmi_core_packet_mode {
	HDMI_PACKETMODERESERVEDVALUE = 0,
	HDMI_PACKETMODE24BITPERPIXEL = 4,
	HDMI_PACKETMODE30BITPERPIXEL = 5,
	HDMI_PACKETMODE36BITPERPIXEL = 6,
	HDMI_PACKETMODE48BITPERPIXEL = 7
};

enum hdmi_core_hdmi_dvi {
	HDMI_DVI	= 0,
	HDMI_HDMI	= 1
};

enum hdmi_core_tclkselclkmult {
	FPLL05IDCK  = 0,
	FPLL10IDCK  = 1,
	FPLL20IDCK  = 2,
	FPLL40IDCK  = 3
};

struct hdmi_core_video_config_t {
	enum hdmi_core_inputbus_width	CoreInputBusWide;
	enum hdmi_core_dither_trunc 	CoreOutputDitherTruncation;
	enum hdmi_core_deepcolor_ed		CoreDeepColorPacketED;
	enum hdmi_core_packet_mode		CorePacketMode;
	enum hdmi_core_hdmi_dvi			CoreHdmiDvi;
	enum hdmi_core_tclkselclkmult	CoreTclkSelClkMult;
};

enum hdmi_core_fs {
	FS_32000 = 0,
	FS_44100 = 1
};

enum hdmi_core_layout {
	LAYOUT_2CH = 0,
	LAYOUT_8CH = 1
};

enum hdmi_core_cts_mode {
	CTS_MODE_HW = 0,
	CTS_MODE_SW = 1
};

enum hdmi_core_packet_ctrl {
	PACKETENABLE = 1,
	PACKETDISABLE = 0,
	PACKETREPEATON = 1,
	PACKETREPEATOFF = 0
};

/* INFOFRAME_AVI_ definations */
enum hdmi_core_infoframe {
	INFOFRAME_AVI_DB1Y_RGB = 0,
	INFOFRAME_AVI_DB1Y_YUV422 = 1,
	INFOFRAME_AVI_DB1Y_YUV444 = 2,
	INFOFRAME_AVI_DB1A_ACTIVE_FORMAT_OFF = 0,
	INFOFRAME_AVI_DB1A_ACTIVE_FORMAT_ON =  1,
	INFOFRAME_AVI_DB1B_NO = 0,
	INFOFRAME_AVI_DB1B_VERT = 1,
	INFOFRAME_AVI_DB1B_HORI = 2,
	INFOFRAME_AVI_DB1B_VERTHORI = 3,
	INFOFRAME_AVI_DB1S_0 = 0,
	INFOFRAME_AVI_DB1S_1 = 1,
	INFOFRAME_AVI_DB1S_2 = 2,
	INFOFRAME_AVI_DB2C_NO = 0,
	INFOFRAME_AVI_DB2C_ITU601 = 1,
	INFOFRAME_AVI_DB2C_ITU709 = 2,
	INFOFRAME_AVI_DB2C_EC_EXTENDED = 3,
	INFOFRAME_AVI_DB2M_NO = 0,
	INFOFRAME_AVI_DB2M_43 = 1,
	INFOFRAME_AVI_DB2M_169 = 2,
	INFOFRAME_AVI_DB2R_SAME = 8,
	INFOFRAME_AVI_DB2R_43 = 9,
	INFOFRAME_AVI_DB2R_169 = 10,
	INFOFRAME_AVI_DB2R_149 = 11,
	INFOFRAME_AVI_DB3ITC_NO = 0,
	INFOFRAME_AVI_DB3ITC_YES = 1,
	INFOFRAME_AVI_DB3EC_XVYUV601 = 0,
	INFOFRAME_AVI_DB3EC_XVYUV709 = 1,
	INFOFRAME_AVI_DB3Q_DEFAULT = 0,
	INFOFRAME_AVI_DB3Q_LR = 1,
	INFOFRAME_AVI_DB3Q_FR = 2,
	INFOFRAME_AVI_DB3SC_NO = 0,
	INFOFRAME_AVI_DB3SC_HORI = 1,
	INFOFRAME_AVI_DB3SC_VERT = 2,
	INFOFRAME_AVI_DB3SC_HORIVERT = 3,
	INFOFRAME_AVI_DB5PR_NO = 0,
	INFOFRAME_AVI_DB5PR_2 = 1,
	INFOFRAME_AVI_DB5PR_3 = 2,
	INFOFRAME_AVI_DB5PR_4 = 3,
	INFOFRAME_AVI_DB5PR_5 = 4,
	INFOFRAME_AVI_DB5PR_6 = 5,
	INFOFRAME_AVI_DB5PR_7 = 6,
	INFOFRAME_AVI_DB5PR_8 = 7,
	INFOFRAME_AVI_DB5PR_9 = 8,
	INFOFRAME_AVI_DB5PR_10 = 9
};

struct hdmi_core_infoframe_avi {
	u8		db1y_rgb_yuv422_yuv444;
	u8		db1a_active_format_off_on;
	u8		db1b_no_vert_hori_verthori;
	u8		db1s_0_1_2;
	u8		db2c_no_itu601_itu709_extented;
	u8		db2m_no_43_169;
	u8		db2r_same_43_169_149;
	u8		db3itc_no_yes;
	u8		db3ec_xvyuv601_xvyuv709;
	u8		db3q_default_lr_fr;
	u8		db3sc_no_hori_vert_horivert;
	u8		db4vic_videocode;
	u8		db5pr_no_2_3_4_5_6_7_8_9_10;
	u16		db6_7_lineendoftop;
	u16		db8_9_linestartofbottom;
	u16		db10_11_pixelendofleft;
	u16		db12_13_pixelstartofright;
};

struct hdmi_core_packet_enable_repeat {
	u32		AudioPacketED;
	u32		AudioPacketRepeat;
	u32		AVIInfoFrameED;
	u32		AVIInfoFrameRepeat;
	u32		GeneralcontrolPacketED;
	u32		GeneralcontrolPacketRepeat;
	u32		GenericPacketED;
	u32		GenericPacketRepeat;
};

enum hdmi_stereo_channel {
	HDMI_STEREO_NOCHANNEL = 0,
	HDMI_STEREO_ONECHANNELS = 1,
	HDMI_STEREO_TWOCHANNELS = 2,
	HDMI_STEREO_THREECHANNELS = 3,
	HDMI_STEREO_FOURCHANNELS = 4
};

enum hdmi_cea_code {
	HDMI_CEA_CODE_00 = 0x0,
	HDMI_CEA_CODE_01 = 0x1,
	HDMI_CEA_CODE_02 = 0x2,
	HDMI_CEA_CODE_03 = 0x3,
	HDMI_CEA_CODE_04 = 0x4,
	HDMI_CEA_CODE_05 = 0x5,
	HDMI_CEA_CODE_06 = 0x6,
	HDMI_CEA_CODE_07 = 0x7,
	HDMI_CEA_CODE_08 = 0x8,
	HDMI_CEA_CODE_09 = 0x9,
	HDMI_CEA_CODE_0A = 0xA,
	HDMI_CEA_CODE_0B = 0xB,
	HDMI_CEA_CODE_0C = 0xC,
	HDMI_CEA_CODE_0D = 0xD,
	HDMI_CEA_CODE_0E = 0xE,
	HDMI_CEA_CODE_0F = 0xF,
	HDMI_CEA_CODE_10 = 0x10,
	HDMI_CEA_CODE_11 = 0x11,
	HDMI_CEA_CODE_12 = 0x12,
	HDMI_CEA_CODE_13 = 0x13,
	HDMI_CEA_CODE_14 = 0x14,
	HDMI_CEA_CODE_15 = 0x15,
	HDMI_CEA_CODE_16 = 0x16,
	HDMI_CEA_CODE_17 = 0x17,
	HDMI_CEA_CODE_18 = 0x18,
	HDMI_CEA_CODE_19 = 0x19,
	HDMI_CEA_CODE_1A = 0x1A,
	HDMI_CEA_CODE_1B = 0x1B,
	HDMI_CEA_CODE_1C = 0x1C,
	HDMI_CEA_CODE_1D = 0x1D,
	HDMI_CEA_CODE_1E = 0x1E,
	HDMI_CEA_CODE_1F = 0x1F,
	HDMI_CEA_CODE_20 = 0x20,
	HDMI_CEA_CODE_21 = 0x21,
	HDMI_CEA_CODE_22 = 0x22,
	HDMI_CEA_CODE_23 = 0x23,
	HDMI_CEA_CODE_24 = 0x24,
	HDMI_CEA_CODE_25 = 0x25,
	HDMI_CEA_CODE_26 = 0x26
};

enum hdmi_iec_format {
	HDMI_AUDIO_FORMAT_LPCM = 0,
	HDMI_AUDIO_FORMAT_IEC = 1
};

enum hdmi_audio_justify {
	HDMI_AUDIO_JUSTIFY_LEFT = 0,
	HDMI_AUDIO_JUSTIFY_RIGHT = 1
};

enum hdmi_sample_order {
	HDMI_SAMPLE_RIGHT_FIRST = 0,
	HDMI_SAMPLE_LEFT_FIRST = 1
};

enum hdmi_sample_perword {
	HDMI_ONEWORD_ONE_SAMPLE = 0,
	HDMI_ONEWORD_TWO_SAMPLES = 1
};

enum hdmi_sample_size {
	HDMI_SAMPLE_16BITS = 0,
	HDMI_SAMPLE_24BITS = 1
};

struct hdmi_audio_format {
	enum hdmi_stereo_channel	stereo_channel_enable;
	enum hdmi_cea_code		audio_channel_location;
	enum hdmi_iec_format		iec;
	enum hdmi_audio_justify		justify;
	enum hdmi_sample_order		left_before;
	enum hdmi_sample_perword	sample_number;
	enum hdmi_sample_size		sample_size;
};

enum hdmi_dma_irq {
	HDMI_THRESHOLD_DMA = 0,
	HDMI_THRESHOLD_IRQ = 1
};

enum hdmi_block_start_end {
	HDMI_BLOCK_STARTEND_ON = 0,
	HDMI_BLOCK_STARTEND_OFF = 1
};

struct hdmi_audio_dma {
	u8				dma_transfer;
	u8				block_size;
	enum hdmi_dma_irq		dma_or_irq;
	u16				threshold_value;
	enum hdmi_block_start_end	block_start_end;
};

enum hdmi_packing_mode {
	HDMI_PACK_10b_RGB_YUV444 = 0,
	HDMI_PACK_24b_RGB_YUV444_YUV422 = 1,
	HDMI_PACK_20b_YUV422 = 2,
	HDMI_PACK_ALREADYPACKED = 7
};

struct hdmi_video_format {
	enum hdmi_packing_mode	packingMode;
	u32	linePerPanel;
	u32	pixelPerLine;
};

struct hdmi_video_interface {
	int	vSyncPolarity;
	int	hSyncPolarity;
	int	interlacing;
	int	timingMode;
};

struct hdmi_video_timing {
	u32	horizontalBackPorch;
	u32	horizontalFrontPorch;
	u32	horizontalSyncPulse;
	u32	verticalBackPorch;
	u32	verticalFrontPorch;
	u32	verticalSyncPulse;
};

typedef struct HDMI_Timing_label {
	u32	pixelPerLine;
	u32	linePerPanel;
	u32	horizontalBackPorch;
	u32	horizontalFrontPorch;
	u32	horizontalSyncPulse;
	u32	verticalBackPorch;
	u32	verticalFrontPorch;
	u32	verticalSyncPulse;
	u32	pplclk;
} HDMI_Timing_t, *pHDMI_Timing_t;

struct hdmi_config {
	u16 ppl;	/* pixel per line */
	u16 lpp;	/* line per panel */
	u32 pixel_clock;
	u16 hsw;	/* Horizontal synchronization pulse width */
	u16 hfp;	/* Horizontal front porch */
	u16 hbp;	/* Horizontal back porch */
	u16 vsw;	/* Vertical synchronization pulse width */
	u16 vfp;	/* Vertical front porch */
	u16 vbp;	/* Vertical back porch */
	u16 interlace;
	u16 h_pol;
	u16 v_pol;
	u16 hdmi_dvi;
	u16 video_format;
};

enum hdmi_core_if_fs {
	IF_FS_NO     = 0x0,
	IF_FS_32000  = 0x1,
	IF_FS_44100  = 0x2,
	IF_FS_48000  = 0x3,
	IF_FS_88200  = 0x4,
	IF_FS_96000  = 0x5,
	IF_FS_176400 = 0x6,
	IF_FS_192000 = 0x7
};

enum hdmi_core_if_sample_size{
	IF_NO_PER_SAMPLE = 0x0,
	IF_16BIT_PER_SAMPLE = 0x1,
	IF_20BIT_PER_SAMPLE = 0x2,
	IF_24BIT_PER_SAMPLE = 0x3
};

struct hdmi_core_audio_config {
	enum hdmi_core_fs		fs; /* 0=32KHz - 1=44.1KHz */
	u32				n;
	u32				cts;
	u32				aud_par_busclk;
	enum hdmi_core_layout		layout; /* 0: 2Ch - 1: 8Ch */
	enum hdmi_core_cts_mode		cts_mode; /* 0: HW  - 1: SW*/
	enum hdmi_core_if_fs		if_fs;
	u32				if_channel_number;
	enum hdmi_core_if_sample_size	if_sample_size;
	enum hdmi_cea_code		if_audio_channel_location;
 };

/* Function prototype */
int HDMI_W1_StopVideoFrame(u32);
int HDMI_W1_StartVideoFrame(u32);
int HDMI_W1_SetWaitPhyPwrState(u32 name, HDMI_PhyPwr_t param);
int HDMI_W1_SetWaitPllPwrState(u32 name, HDMI_PllPwr_t param);
int HDMI_W1_SetWaitSoftReset(void);
int hdmi_w1_wrapper_disable(u32);
int hdmi_w1_wrapper_enable(u32);
int hdmi_w1_stop_audio_transfer(u32);
int hdmi_w1_start_audio_transfer(u32);
int HDMI_CORE_DDC_READEDID(u32 Core, u8 data[256]);
int DSS_HDMI_CONFIG(HDMI_Timing_t timings, u32 video_format, u32 mode);
void HDMI_W1_HPD_handler(int *r);
int hdmi_lib_init(void);
void hdmi_lib_exit(void);

#endif

