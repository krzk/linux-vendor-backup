/*
 * SiI8620 Linux Driver
 *
 * Copyright (C) 2013-2014 Silicon Image, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 * This program is distributed AS-IS WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; INCLUDING without the implied warranty
 * of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.
 * See the GNU General Public License for more details at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef _SII8620_H_
#define _SII8620_H_

/* Registers in TX_PAGE_0 (0x00-0xFF)                                         */

/* 0x00 Vendor ID Low byte Register                        (Default: 0x01)    */
#define REG_VND_IDL                                 (0x0000)
#define MSK_VND_IDL_VHDL_IDL                        (0xFF)

/* 0x01 Vendor ID High byte Register                       (Default: 0x00)    */
#define REG_VND_IDH                                 (0x0001)
#define MSK_VND_IDH_VHDL_IDH                        (0xFF)

/* 0x02 Device ID Low byte Register                        (Default: 0x60)    */
#define REG_DEV_IDL                                 (0x0002)
#define MSK_DEV_IDL_DEV_IDL                         (0xFF)

/* 0x03 Device ID High byte Register                       (Default: 0x86)    */
#define REG_DEV_IDH                                 (0x0003)
#define MSK_DEV_IDH_DEV_IDH                         (0xFF)

/* 0x04 Device Revision Register                           (Default: 0x10)    */
#define REG_DEV_REV                                 (0x0004)
#define MSK_DEV_REV_DEV_REV_ID                      (0xFF)

/* 0x06 OTP DBYTE510 Register                              (Default: 0x00)    */
#define REG_OTP_DBYTE510                            (0x0006)
#define MSK_OTP_DBYTE510_OTP_DBYTE510               (0xFF)

/* 0x08 System Control #1 Register                         (Default: 0x00)    */
#define REG_SYS_CTRL1                               (0x0008)
#define BIT_SYS_CTRL1_OTPVMUTEOVR_SET               (0x80)
#define BIT_SYS_CTRL1_VSYNCPIN                      (0x40)
#define BIT_SYS_CTRL1_OTPADROPOVR_SET               (0x20)
#define BIT_SYS_CTRL1_BLOCK_DDC_BY_HPD              (0x10)
#define BIT_SYS_CTRL1_OTP2XVOVR_EN                  (0x08)
#define BIT_SYS_CTRL1_OTP2XAOVR_EN                  (0x04)
#define BIT_SYS_CTRL1_TX_CONTROL_HDMI               (0x02)
#define BIT_SYS_CTRL1_OTPAMUTEOVR_SET               (0x01)

/* 0x0B System Control DPD                                 (Default: 0x90)    */
#define REG_DPD                                     (0x000B)
#define BIT_DPD_PWRON_PLL                           (0x80)
#define BIT_DPD_PDNTX12                             (0x40)
#define BIT_DPD_PDNRX12                             (0x20)
#define BIT_DPD_OSC_EN                              (0x10)
#define BIT_DPD_PWRON_HSIC                          (0x08)
#define BIT_DPD_PDIDCK_N                            (0x04)
#define BIT_DPD_PD_MHL_CLK_N                        (0x02)

/* 0x0D Dual link Control Register                         (Default: 0x00)    */
#define REG_DCTL                                    (0x000D)
#define BIT_DCTL_TDM_LCLK_PHASE                     (0x80)
#define BIT_DCTL_HSIC_CLK_PHASE                     (0x40)
#define BIT_DCTL_CTS_TCK_PHASE                      (0x20)
#define BIT_DCTL_EXT_DDC_SEL                        (0x10)
#define BIT_DCTL_TRANSCODE                          (0x08)
#define BIT_DCTL_HSIC_RX_STROBE_PHASE               (0x04)
#define BIT_DCTL_HSIC_TX_BIST_START_SEL             (0x02)
#define BIT_DCTL_TCLKNX_PHASE                       (0x01)

/* 0x0E PWD Software Reset Register                        (Default: 0x20)    */
#define REG_PWD_SRST                                (0x000E)
#define BIT_PWD_SRST_COC_DOC_RST                    (0x80)
#define BIT_PWD_SRST_CBUS_RST_SW                    (0x40)
#define BIT_PWD_SRST_CBUS_RST_SW_EN                 (0x20)
#define BIT_PWD_SRST_MHLFIFO_RST                    (0x10)
#define BIT_PWD_SRST_CBUS_RST                       (0x08)
#define BIT_PWD_SRST_SW_RST_AUTO                    (0x04)
#define BIT_PWD_SRST_HDCP2X_SW_RST                  (0x02)
#define BIT_PWD_SRST_SW_RST                         (0x01)

/* 0x1D AKSV_1 Register                                    (Default: 0x00)    */
#define REG_AKSV_1                                  (0x001D)
#define MSK_AKSV_1_AKSV0                            (0xFF)

/* 0x3A Video H Resolution #1 Register                     (Default: 0x00)    */
#define REG_H_RESL                                  (0x003A)
#define MSK_H_RESL_HRESOUT_7_0                      (0xFF)

/* 0x4A Video Mode Register                                (Default: 0x00)    */
#define REG_VID_MODE                                (0x004A)
#define BIT_VID_MODE_M1080P                         (0x40)
#define VAL_VID_MODE_M1080P_DISABLE                 (0x00)
#define VAL_VID_MODE_M1080P_ENABLE                  (0x40)

/* 0x51 Video Input Mode Register                          (Default: 0xC0)    */
#define REG_VID_OVRRD                               (0x0051)
#define BIT_VID_OVRRD_PP_AUTO_DISABLE               (0x80)
#define BIT_VID_OVRRD_M1080P_OVRRD                  (0x40)
#define BIT_VID_OVRRD_MINIVSYNC_ON                  (0x20)
#define BIT_VID_OVRRD_3DCONV_EN_FRAME_PACK          (0x10)
#define BIT_VID_OVRRD_ENABLE_AUTO_PATH_EN           (0x08)
#define BIT_VID_OVRRD_ENRGB2YCBCR_OVRRD             (0x04)
#define BIT_VID_OVRRD_ENDOWNSAMPLE_OVRRD            (0x01)

/* I2C Address reassignment   (Default: 0x00)    */
#define REG_PAGE_MHLSPEC_ADDR                       (0x0057)
#define REG_PAGE7_ADDR                              (0x0058)
#define REG_PAGE8_ADDR                              (0x005C)

/* 0x5F Fast Interrupt Status Register                     (Default: 0x00)    */
#define REG_FAST_INTR_STAT                          (0x005F)
#define LEN_FAST_INTR_STAT			7
#define BIT_FAST_INTR_STAT_TIMR			8
#define BIT_FAST_INTR_STAT_INT2			9
#define BIT_FAST_INTR_STAT_DDC			10
#define BIT_FAST_INTR_STAT_SCDT			11
#define BIT_FAST_INTR_STAT_INFR			13
#define BIT_FAST_INTR_STAT_EDID			14
#define BIT_FAST_INTR_STAT_HDCP			15
#define BIT_FAST_INTR_STAT_MSC			16
#define BIT_FAST_INTR_STAT_MERR			17
#define BIT_FAST_INTR_STAT_G2WB			18
#define BIT_FAST_INTR_STAT_G2WB_ERR		19
#define BIT_FAST_INTR_STAT_DISC			28
#define BIT_FAST_INTR_STAT_BLOCK		30
#define BIT_FAST_INTR_STAT_LTRN			31
#define BIT_FAST_INTR_STAT_HDCP2		32
#define BIT_FAST_INTR_STAT_TDM			42
#define BIT_FAST_INTR_STAT_COC			51

/* 0x6E GPIO Control Register 1                            (Default: 0x15)    */
#define REG_GPIO_CTRL1                              (0x006E)
#define BIT_CTRL1_GPIO_I_8                     (0x20)
#define BIT_CTRL1_GPIO_OEN_8                   (0x10)
#define BIT_CTRL1_GPIO_I_7                     (0x08)
#define BIT_CTRL1_GPIO_OEN_7                   (0x04)
#define BIT_CTRL1_GPIO_I_6                     (0x02)
#define BIT_CTRL1_GPIO_OEN_6                   (0x01)

/* 0x6F Interrupt Control Register                         (Default: 0x06)    */
#define REG_INT_CTRL                                (0x006F)
#define BIT_INT_CTRL_SOFTWARE_WP                    (0x80)
#define BIT_INT_CTRL_INTR_OD                        (0x04)
#define BIT_INT_CTRL_INTR_POLARITY                  (0x02)

/* 0x70 Interrupt State Register                           (Default: 0x00)    */
#define REG_INTR_STATE                              (0x0070)
#define BIT_INTR_STATE_INTR_STATE                   (0x01)

/* 0x71 Interrupt Source #1 Register                       (Default: 0x00)    */
#define REG_INTR1                                   (0x0071)

/* 0x72 Interrupt Source #2 Register                       (Default: 0x00)    */
#define REG_INTR2                                   (0x0072)

/* 0x73 Interrupt Source #3 Register                       (Default: 0x01)    */
#define REG_INTR3                                   (0x0073)
#define BIT_DDC_CMD_DONE		BIT(3)

/* 0x74 Interrupt Source #5 Register                       (Default: 0x00)    */
#define REG_INTR5                                   (0x0074)

/* 0x75 Interrupt #1 Mask Register                         (Default: 0x00)    */
#define REG_INTR1_MASK                              (0x0075)

/* 0x76 Interrupt #2 Mask Register                         (Default: 0x00)    */
#define REG_INTR2_MASK                              (0x0076)

/* 0x77 Interrupt #3 Mask Register                         (Default: 0x00)    */
#define REG_INTR3_MASK                              (0x0077)

/* 0x78 Interrupt #5 Mask Register                         (Default: 0x00)    */
#define REG_INTR5_MASK                              (0x0078)
#define BIT_INTR_SCDT_CHANGE		BIT(0)

/* 0x79 Hot Plug Connection Control Register               (Default: 0x45)    */
#define REG_HPD_CTRL                                (0x0079)
#define BIT_HPD_CTRL_HPD_DS_SIGNAL                  (0x80)
#define BIT_HPD_CTRL_HPD_OUT_OD_EN                  (0x40)

#define BIT_HPD_CTRL_HPD_OUT_OVR_VAL                (0x20)
#define VAL_HPD_CTRL_HPD_LOW                        (0x00)
#define VAL_HPD_CTRL_HPD_HIGH                       (0x20)

#define BIT_HPD_CTRL_HPD_OUT_OVR_EN                 (0x10)
#define VAL_HPD_CTRL_HPD_OUT_OVR_EN_OFF             (0x00)
#define VAL_HPD_CTRL_HPD_OUT_OVR_EN_ON              (0x10)

#define BIT_HPD_CTRL_GPIO_I_1                       (0x08)
#define BIT_HPD_CTRL_GPIO_OEN_1                     (0x04)
#define BIT_HPD_CTRL_GPIO_I_0                       (0x02)
#define BIT_HPD_CTRL_GPIO_OEN_0                     (0x01)

/* 0x7A GPIO Control Register                              (Default: 0x55)    */
#define REG_GPIO_CTRL                               (0x007A)
#define BIT_CTRL_GPIO_I_5                      (0x80)
#define BIT_CTRL_GPIO_OEN_5                    (0x40)
#define BIT_CTRL_GPIO_I_4                      (0x20)
#define BIT_CTRL_GPIO_OEN_4                    (0x10)
#define BIT_CTRL_GPIO_I_3                      (0x08)
#define BIT_CTRL_GPIO_OEN_3                    (0x04)
#define BIT_CTRL_GPIO_I_2                      (0x02)
#define BIT_CTRL_GPIO_OEN_2                    (0x01)

/* 0x7B Interrupt Source 7 Register                        (Default: 0x00)    */
#define REG_INTR7                                   (0x007B)

/* 0x7C Interrupt Source 8 Register                        (Default: 0x00)    */
#define REG_INTR8                                   (0x007C)

/* 0x7D Interrupt #7 Mask Register                         (Default: 0x00)    */
#define REG_INTR7_MASK                              (0x007D)

/* 0x7E Interrupt #8 Mask Register                         (Default: 0x00)    */
#define REG_INTR8_MASK                              (0x007E)
#define BIT_CEA_NEW_VSI			BIT(2)
#define BIT_CEA_NEW_AVI			BIT(1)

/* 0x80 IEEE                                               (Default: 0x10)    */
#define REG_TMDS_CCTRL                              (0x0080)
#define BIT_TMDS_CCTRL_TMDS_OE                      (0x10)

/* 0x85 TMDS Control #4 Register                           (Default: 0x02)    */
#define REG_TMDS_CTRL4                              (0x0085)
#define BIT_TMDS_CTRL4_SCDT_CKDT_SEL                (0x02)
#define BIT_TMDS_CTRL4_TX_EN_BY_SCDT                (0x01)

/* 0xBB BIST CNTL Register                                 (Default: 0x00)    */
#define REG_BIST_CTRL                               (0x00BB)
#define BIT_RXBIST_VGB_EN                 (0x80)
#define BIT_TXBIST_VGB_EN                 (0x40)
#define BIT_BIST_START_SEL                (0x20)
#define BIT_BIST_START_BIT                (0x10)
#define BIT_BIST_ALWAYS_ON                (0x08)
#define BIT_BIST_TRANS                    (0x04)
#define BIT_BIST_RESET                    (0x02)
#define BIT_BIST_EN                       (0x01)

/* 0xBD BIST DURATION0 Register                            (Default: 0x00)    */
#define REG_BIST_TEST_SEL                           (0x00BD)
#define MSK_BIST_TEST_SEL_BIST_PATT_SEL             (0x0F)

/* 0xBE BIST VIDEO_MODE Register                           (Default: 0x00)    */
#define REG_BIST_VIDEO_MODE                         (0x00BE)
#define MSK_BIST_VIDEO_MODE_BIST_VIDEO_MODE_3_0     (0x0F)

/* 0xBF BIST DURATION0 Register                            (Default: 0x00)    */
#define REG_BIST_DURATION_0                         (0x00BF)
#define MSK_BIST_DURATION_0_BIST_DURATION_7_0       (0xFF)

/* 0xC0 BIST DURATION1 Register                            (Default: 0x00)    */
#define REG_BIST_DURATION_1                         (0x00C0)
#define MSK_BIST_DURATION_1_BIST_DURATION_15_8      (0xFF)

/* 0xC1 BIST DURATION2 Register                            (Default: 0x00)    */
#define REG_BIST_DURATION_2                         (0x00C1)
#define MSK_BIST_DURATION_2_BIST_DURATION_22_16     (0x7F)

/* 0xC2 BIST 8BIT_PATTERN Register                         (Default: 0x00)    */
#define REG_BIST_8BIT_PATTERN                       (0x00C2)
#define MSK_BIST_8BIT_PATTERN_BIST_10BIT_PATT28LSB  (0xFF)

/* 0xC7 LM DDC Register                                    (Default: 0x80)    */
#define REG_LM_DDC                                  (0x00C7)
#define BIT_LM_DDC_SW_TPI_EN                        (0x80)
#define VAL_LM_DDC_SW_TPI_EN_ENABLED                (0x00)
#define VAL_LM_DDC_SW_TPI_EN_DISABLED               (0x80)

#define BIT_LM_DDC_VIDEO_MUTE_EN                    (0x20)
#define BIT_LM_DDC_DDC_TPI_SW                       (0x04)
#define BIT_LM_DDC_DDC_GRANT                        (0x02)
#define BIT_LM_DDC_DDC_GPU_REQUEST                  (0x01)

/* 0xEC DDC I2C Manual Register                            (Default: 0x03)    */
#define REG_DDC_MANUAL                              (0x00EC)
#define BIT_DDC_MANUAL_MAN_DDC                      (0x80)
#define BIT_DDC_MANUAL_VP_SEL                       (0x40)
#define BIT_DDC_MANUAL_DSDA                         (0x20)
#define BIT_DDC_MANUAL_DSCL                         (0x10)
#define BIT_DDC_MANUAL_GCP_HW_CTL_EN                (0x08)
#define BIT_DDC_MANUAL_DDCM_ABORT_WP                (0x04)
#define BIT_DDC_MANUAL_IO_DSDA                      (0x02)
#define BIT_DDC_MANUAL_IO_DSCL                      (0x01)

/* 0xED DDC I2C Target Slave Address Register              (Default: 0x00)    */
#define REG_DDC_ADDR                                (0x00ED)
#define MSK_DDC_ADDR_DDC_ADDR                       (0xFE)

/* 0xEE DDC I2C Target Segment Address Register            (Default: 0x00)    */
#define REG_DDC_SEGM                                (0x00EE)
#define MSK_DDC_SEGM_DDC_SEGMENT                    (0xFF)

/* 0xEF DDC I2C Target Offset Adress Register              (Default: 0x00)    */
#define REG_DDC_OFFSET                              (0x00EF)
#define MSK_DDC_OFFSET_DDC_OFFSET                   (0xFF)

/* 0xF0 DDC I2C Data In count #1 Register                  (Default: 0x00)    */
#define REG_DDC_DIN_CNT1                            (0x00F0)
#define MSK_DDC_DIN_CNT1_DDC_DIN_CNT_7_0            (0xFF)

/* 0xF1 DDC I2C Data In count #2 Register                  (Default: 0x00)    */
#define REG_DDC_DIN_CNT2                            (0x00F1)
#define MSK_DDC_DIN_CNT2_DDC_DIN_CNT_9_8            (0x03)

/* 0xF2 DDC I2C Status Register                            (Default: 0x04)    */
#define REG_DDC_STATUS                              (0x00F2)
#define BIT_DDC_STATUS_DDC_BUS_LOW                  (0x40)
#define BIT_DDC_STATUS_DDC_NO_ACK                   (0x20)
#define BIT_DDC_STATUS_DDC_I2C_IN_PROG              (0x10)
#define BIT_DDC_STATUS_DDC_FIFO_FULL                (0x08)
#define BIT_DDC_STATUS_DDC_FIFO_EMPTY               (0x04)
#define BIT_DDC_STATUS_DDC_FIFO_READ_IN_SUE         (0x02)
#define BIT_DDC_STATUS_DDC_FIFO_WRITE_IN_USE        (0x01)

/* 0xF3 DDC I2C Command Register                           (Default: 0x70)    */
#define REG_DDC_CMD                                 (0x00F3)
#define BIT_DDC_CMD_HDCP_DDC_EN                     (0x40)
#define BIT_DDC_CMD_SDA_DEL_EN                      (0x20)
#define BIT_DDC_CMD_DDC_FLT_EN                      (0x10)

#define MSK_DDC_CMD_DDC_CMD                         (0x0F)
#define VAL_DDC_CMD_ENH_DDC_READ_NO_ACK             (0x04)
#define VAL_DDC_CMD_DDC_CMD_CLEAR_FIFO              (0x09)
#define VAL_DDC_CMD_DDC_CMD_ABORT                   (0x0F)

/* 0xF4 DDC I2C FIFO Data In/Out Register                  (Default: 0x00)    */
#define REG_DDC_DATA                                (0x00F4)
#define MSK_DDC_DATA_DDC_DATA_OUT                   (0xFF)

/* 0xF5 DDC I2C Data Out Counter Register                  (Default: 0x00)    */
#define REG_DDC_DOUT_CNT                            (0x00F5)
#define BIT_DDC_DOUT_CNT_DDC_DELAY_CNT_8            (0x80)
#define MSK_DDC_DOUT_CNT_DDC_DATA_OUT_CNT           (0x1F)

/* 0xF6 DDC I2C Delay Count Register                       (Default: 0x14)    */
#define REG_DDC_DELAY_CNT                           (0x00F6)
#define MSK_DDC_DELAY_CNT_DDC_DELAY_CNT_7_0         (0xFF)

/* 0xF7 Test Control Register                              (Default: 0x80)    */
#define REG_TEST_TXCTRL                             (0x00F7)
#define BIT_TEST_TXCTRL_RCLK_REF_SEL                (0x80)
#define BIT_TEST_TXCTRL_PCLK_REF_SEL                (0x40)
#define MSK_TEST_TXCTRL_BYPASS_PLL_CLK              (0x3C)
#define BIT_TEST_TXCTRL_HDMI_MODE                   (0x02)
#define BIT_TEST_TXCTRL_TST_PLLCK                   (0x01)

/* 0xF8 CBUS Address Register                              (Default: 0x00)    */
#define REG_PAGE_CBUS_ADDR                          (0x00F8)
#define MSK_PAGE_CBUS_ADDR_PAGE_CBUS_ADDR_7_0       (0xFF)

/* I2C Device Address re-assignment */
#define REG_PAGE1_ADDR                             (0x00FC)
#define REG_PAGE2_ADDR                             (0x00FD)
#define REG_PAGE3_ADDR                             (0x00FE)
#define REG_HW_TPI_ADDR                            (0x00FF)

/* Registers in TX_PAGE_1 (0x00-0xFF)                                         */

/* 0x00 USBT CTRL0 Register                                (Default: 0x00)    */
#define REG_UTSRST                                  (0x0100)
#define BIT_UTSRST_FC_SRST                          (0x20)
#define BIT_UTSRST_KEEPER_SRST                      (0x10)
#define BIT_UTSRST_HTX_SRST                         (0x08)
#define BIT_UTSRST_TRX_SRST                         (0x04)
#define BIT_UTSRST_TTX_SRST                         (0x02)
#define BIT_UTSRST_HRX_SRST                         (0x01)

/* 0x04 HSIC RX Control3 Register                          (Default: 0x07)    */
#define REG_HRXCTRL3                                (0x0104)
#define MSK_HRXCTRL3_HRX_AFFCTRL                    (0xF0)
#define BIT_HRXCTRL3_HRX_OUT_EN                     (0x04)
#define BIT_HRXCTRL3_STATUS_EN                      (0x02)
#define BIT_HRXCTRL3_HRX_STAY_RESET                 (0x01)

/* HSIC RX INT Registers */
#define REG_HRXINTL                                 (0x0111)
#define REG_HRXINTH                                 (0x0112)

/* 0x16 TDM TX NUMBITS Register                            (Default: 0x0C)    */
#define REG_TTXNUMB                                 (0x0116)
#define MSK_TTXNUMB_TTX_AFFCTRL_3_0                 (0xF0)
#define BIT_TTXNUMB_TTX_COM1_AT_SYNC_WAIT           (0x08)
#define MSK_TTXNUMB_TTX_NUMBPS_2_0                  (0x07)

/* 0x17 TDM TX NUMSPISYM Register                          (Default: 0x04)    */
#define REG_TTXSPINUMS                              (0x0117)
#define MSK_TTXSPINUMS_TTX_NUMSPISYM                (0xFF)

/* 0x18 TDM TX NUMHSICSYM Register                         (Default: 0x14)    */
#define REG_TTXHSICNUMS                             (0x0118)
#define MSK_TTXHSICNUMS_TTX_NUMHSICSYM              (0xFF)

/* 0x19 TDM TX NUMTOTSYM Register                          (Default: 0x18)    */
#define REG_TTXTOTNUMS                              (0x0119)
#define MSK_TTXTOTNUMS_TTX_NUMTOTSYM                (0xFF)

/* 0x36 TDM TX INT Low Register                            (Default: 0x00)    */
#define REG_TTXINTL                                 (0x0136)
#define BIT_TTXINTL_TTX_INTR7                       (0x80)
#define BIT_TTXINTL_TTX_INTR6                       (0x40)
#define BIT_TTXINTL_TTX_INTR5                       (0x20)
#define BIT_TTXINTL_TTX_INTR4                       (0x10)
#define BIT_TTXINTL_TTX_INTR3                       (0x08)
#define BIT_TTXINTL_TTX_INTR2                       (0x04)
#define BIT_TTXINTL_TTX_INTR1                       (0x02)
#define BIT_TTXINTL_TTX_INTR0                       (0x01)

/* 0x37 TDM TX INT High Register                           (Default: 0x00)    */
#define REG_TTXINTH                                 (0x0137)
#define BIT_TTXINTH_TTX_INTR15                      (0x80)
#define BIT_TTXINTH_TTX_INTR14                      (0x40)
#define BIT_TTXINTH_TTX_INTR13                      (0x20)
#define BIT_TTXINTH_TTX_INTR12                      (0x10)
#define BIT_TTXINTH_TTX_INTR11                      (0x08)
#define BIT_TTXINTH_TTX_INTR10                      (0x04)
#define BIT_TTXINTH_TTX_INTR9                       (0x02)
#define BIT_TTXINTH_TTX_INTR8                       (0x01)

/* 0x3B TDM RX Control Register                            (Default: 0x1C)    */
#define REG_TRXCTRL                                 (0x013B)
#define BIT_TRXCTRL_TRX_CLR_WVALLOW                 (0x10)
#define BIT_TRXCTRL_TRX_FROM_SE_COC                 (0x08)
#define MSK_TRXCTRL_TRX_NUMBPS_2_0                  (0x07)

/* 0x3C TDM RX NUMSPISYM Register                          (Default: 0x04)    */
#define REG_TRXSPINUMS                       (0x013C)

#define MSK_TRXSPINUMS_TRX_NUMSPISYM                (0xFF << 0)

/* 0x3D TDM RX NUMHSICSYM Register                         (Default: 0x14)    */
#define REG_TRXHSICNUMS                      (0x013D)

#define MSK_TRXHSICNUMS_TRX_NUMHSICSYM              (0xFF << 0)

/* 0x3E TDM RX NUMTOTSYM Register                          (Default: 0x18)    */
#define REG_TRXTOTNUMS                       (0x013E)

#define MSK_TRXTOTNUMS_TRX_NUMTOTSYM                (0xFF << 0)


/* 0x5C TDM RX Status 2nd Register                         (Default: 0x00)    */
#define REG_TRXSTA2                                 (0x015C)
#define MSK_TRXSTA2_TRX_STATUS_15_8                 (0xFF)

/* 0x63 TDM RX INT Low Register                            (Default: 0x00)    */
#define REG_TRXINTL                                 (0x0163)

/* 0x64 TDM RX INT High Register                           (Default: 0x00)    */
#define REG_TRXINTH                                 (0x0164)

/* 0x66 TDM RX INTMASK High Register                       (Default: 0x00)    */
#define REG_TRXINTMH                                (0x0166)

/* 0x69 HSIC TX CRTL Register                              (Default: 0x00)    */
#define REG_HTXCTRL                                 (0x0169)
#define BIT_HTXCTRL_HTX_ALLSBE_SOP                  (0x10)
#define BIT_HTXCTRL_HTX_RGDINV_USB                  (0x08)
#define BIT_HTXCTRL_HTX_RSPTDM_BUSY                 (0x04)
#define BIT_HTXCTRL_HTX_DRVCONN1                    (0x02)
#define BIT_HTXCTRL_HTX_DRVRST1                     (0x01)

/* 0x7D HSIC TX INT Low Register                           (Default: 0x00)    */
#define REG_HTXINTL                                 (0x017D)

/* 0x7E HSIC TX INT High Register                          (Default: 0x00)    */
#define REG_HTXINTH                                 (0x017E)

/* 0x81 HSIC Keeper Register                               (Default: 0x00)    */
#define REG_KEEPER                                  (0x0181)
#define MSK_KEEPER_KEEPER_MODE_1_0                  (0x03)

/* 0x83 HSIC Flow Control General Register                 (Default: 0x02)    */
#define REG_FCGC                                    (0x0183)
#define BIT_FCGC_HSIC_FC_HOSTMODE                   (0x02)
#define BIT_FCGC_HSIC_FC_ENABLE                     (0x01)

/*----------------------------------------------------------------------------*/
/* 0x91 HSIC Flow Control CTR13 Register                   (Default: 0xFC)    */
#define REG_FCCTR13                                 (0x0191)

#define MSK_FCCTR13_HFC_CONF13                      (0xFF)

/*----------------------------------------------------------------------------*/
/* 0x92 HSIC Flow Control CTR14 Register                   (Default: 0xFF)    */
#define REG_FCCTR14                                 (0x0192)

#define MSK_FCCTR14_HFC_CONF14                      (0xFF)

/*----------------------------------------------------------------------------*/
/* 0x93 HSIC Flow Control CTR15 Register                   (Default: 0xFF)    */
#define REG_FCCTR15                                 (0x0193)

#define MSK_FCCTR15_HFC_CONF15                      (0xFF)

/* 0xB6 HSIC Flow Control CTR50 Register                   (Default: 0x03)    */
#define REG_FCCTR50                                 (0x01B6)

#define MSK_FCCTR50_HFC_CONF50                      (0xFF)

/* 0xEC HSIC Flow Control INTR0 Register                   (Default: 0x00)    */
#define REG_FCINTR0                                 (0x01EC)
#define REG_FCINTR1                                 (0x01ED)
#define REG_FCINTR2                                 (0x01EE)
#define REG_FCINTR3                                 (0x01EF)
#define REG_FCINTR4                                 (0x01F0)
#define REG_FCINTR5                                 (0x01F1)
#define REG_FCINTR6                                 (0x01F2)
#define REG_FCINTR7                                 (0x01F3)

/* 0xFC TDM Low Latency Register                           (Default: 0x20)    */
#define REG_TDMLLCTL                                (0x01FC)
#define MSK_TDMLLCTL_TRX_LL_SEL_MANUAL              (0xC0)
#define MSK_TDMLLCTL_TRX_LL_SEL_MODE                (0x30)
#define MSK_TDMLLCTL_TTX_LL_SEL_MANUAL              (0x0C)
#define BIT_TDMLLCTL_TTX_LL_TIE_LOW                 (0x02)
#define BIT_TDMLLCTL_TTX_LL_SEL_MODE                (0x01)

/* Registers in TX_PAGE_2 (0x00-0xFF)                                         */

/* 0x10 TMDS 0 Clock Control Register 1                    (Default: 0x10)    */
#define REG_TMDS0_CCTRL1                            (0x0210)
#define MSK_TMDS0_CCTRL1_TEST_SEL                   (0xC0)
#define MSK_TMDS0_CCTRL1_CLK1X_CTL                  (0x30)

/* 0x11 TMDS Clock Enable Register                         (Default: 0x00)    */
#define REG_TMDS_CLK_EN                             (0x0211)
#define BIT_TMDS_CLK_EN_CLK_EN                      (0x01)

/* 0x12 TMDS Channel Enable Register                       (Default: 0x00)    */
#define REG_TMDS_CH_EN                              (0x0212)
#define BIT_TMDS_CH_EN_CH0_EN                       (0x10)
#define BIT_TMDS_CH_EN_CH12_EN                      (0x01)

/* 0x15 BGR_BIAS Register                                  (Default: 0x07)    */
#define REG_BGR_BIAS                                (0x0215)
#define BIT_BGR_BIAS_BGR_EN                         (0x80)
#define MSK_BGR_BIAS_BIAS_BGR_D                     (0x0F)

/* 0x31 TMDS 0 Digital I2C BW Register                     (Default: 0x0A)    */
#define REG_ALICE0_BW_I2C                           (0x0231)
#define MSK_ALICE0_BW_I2C_TMDS0_BW_I2C_7_0          (0xFF)

/* 0x4C TMDS 0 Digital Zone Control Register               (Default: 0xE0)    */
#define REG_ALICE0_ZONE_CTRL                        (0x024C)
#define BIT_ALICE0_ZONE_CTRL_ICRST_N                (0x80)
#define BIT_ALICE0_ZONE_CTRL_USE_INT_DIV20          (0x40)
#define MSK_ALICE0_ZONE_CTRL_SZONE_I2C              (0x30)
#define MSK_ALICE0_ZONE_CTRL_ZONE_CTRL              (0x0F)

/* 0x4D TMDS 0 Digital PLL Mode Control Register           (Default: 0x00)    */
#define REG_ALICE0_MODE_CTRL                        (0x024D)
#define MSK_ALICE0_MODE_CTRL_PLL_MODE_I2C           (0x0C)
#define MSK_ALICE0_MODE_CTRL_DIV20_CTRL             (0x03)

/* 0x85 MHL Tx Control 6th Register                        (Default: 0xA0)    */
#define REG_MHLTX_CTL6                              (0x0285)
#define MSK_MHLTX_CTL6_EMI_SEL                      (0xE0)
#define MSK_MHLTX_CTL6_TX_CLK_SHAPE_9_8             (0x03)

/* 0x90 Packet Filter0 Register                            (Default: 0x00)    */
#define REG_PKT_FILTER_0                            (0x0290)
#define BIT_PKT_FILTER_0_DROP_CEA_GAMUT_PKT         (0x80)
#define BIT_PKT_FILTER_0_DROP_CEA_CP_PKT            (0x40)
#define BIT_PKT_FILTER_0_DROP_MPEG_PKT              (0x20)
#define BIT_PKT_FILTER_0_DROP_SPIF_PKT              (0x10)
#define BIT_PKT_FILTER_0_DROP_AIF_PKT               (0x08)
#define BIT_PKT_FILTER_0_DROP_AVI_PKT               (0x04)
#define BIT_PKT_FILTER_0_DROP_CTS_PKT               (0x02)
#define BIT_PKT_FILTER_0_DROP_GCP_PKT               (0x01)

/* 0x91 Packet Filter1 Register                            (Default: 0x00)    */
#define REG_PKT_FILTER_1                            (0x0291)
#define BIT_PKT_FILTER_1_VSI_OVERRIDE_DIS           (0x80)
#define BIT_PKT_FILTER_1_AVI_OVERRIDE_DIS           (0x40)
#define BIT_PKT_FILTER_1_DROP_AUDIO_PKT             (0x08)
#define BIT_PKT_FILTER_1_DROP_GEN2_PKT              (0x04)
#define BIT_PKT_FILTER_1_DROP_GEN_PKT               (0x02)
#define BIT_PKT_FILTER_1_DROP_VSIF_PKT              (0x01)

/* 0xA0 TMDS Clock Status Register                         (Default: 0x10)    */
#define REG_TMDS_CSTAT_P3                           (0x02A0)
#define BIT_TMDS_CSTAT_P3_RX_HDMI_CP_CLR_MUTE       (0x80)
#define BIT_TMDS_CSTAT_P3_RX_HDMI_CP_SET_MUTE       (0x40)
#define BIT_TMDS_CSTAT_P3_RX_HDMI_CP_NEW_CP         (0x20)
#define BIT_TMDS_CSTAT_P3_CLR_AVI                   (0x08)
#define BIT_TMDS_CSTAT_P3_SCDT_CLR_AVI_DIS          (0x04)
#define BIT_TMDS_CSTAT_P3_SCDT                      (0x02)

#define BIT_TMDS_CSTAT_P3_CKDT                      (0x01)
#define VAL_TMDS_CSTAT_P3_CKDT_STOPPED              (0x00)
#define VAL_TMDS_CSTAT_P3_CKDT_DETECTED             (0x01)

/* 0xA1 RX_HDMI Control Register0                          (Default: 0x10)    */
#define REG_RX_HDMI_CTRL0                           (0x02A1)
#define BIT_RX_HDMI_CTRL0_BYP_DVIFILT_SYNC          (0x20)
#define BIT_RX_HDMI_CTRL0_HDMI_MODE_EN_ITSELF_CLR   (0x10)
#define BIT_RX_HDMI_CTRL0_HDMI_MODE_SW_VALUE        (0x08)
#define BIT_RX_HDMI_CTRL0_HDMI_MODE_OVERWRITE       (0x04)
#define BIT_RX_HDMI_CTRL0_RX_HDMI_HDMI_MODE_EN      (0x02)
#define BIT_RX_HDMI_CTRL0_RX_HDMI_HDMI_MODE         (0x01)

/* 0xA3 RX_HDMI Control Register2                          (Default: 0x38)    */
#define REG_RX_HDMI_CTRL2                           (0x02A3)
#define MSK_RX_HDMI_CTRL2_IDLE_CNT                  (0xF0)
#define VAL_RX_HDMI_CTRL2_IDLE_CNT(n)		((u8)((n) << 4))
#define BIT_RX_HDMI_CTRL2_USE_AV_MUTE               (0x08)
#define VAL_RX_HDMI_CTRL2_USE_AV_MUTE_DISABLE       (0x00)
#define VAL_RX_HDMI_CTRL2_USE_AV_MUTE_ENABLE        (0x08)
#define BIT_RX_HDMI_CTRL2_VSI_MON_SEL               (0x01)
#define VAL_RX_HDMI_CTRL2_VSI_MON_SEL_AVI           (0x00)
#define VAL_RX_HDMI_CTRL2_VSI_MON_SEL_VSI           (0x01)

/* 0xA4 RX_HDMI Control Register3                          (Default: 0x0F)    */
#define REG_RX_HDMI_CTRL3                           (0x02A4)
#define MSK_RX_HDMI_CTRL3_PP_MODE_CLK_EN            (0x0F)

/* 0xAC rx_hdmi Clear Buffer Register                      (Default: 0x00)    */
#define REG_RX_HDMI_CLR_BUFFER                      (0x02AC)
#define MSK_RX_HDMI_CLR_BUFFER_AIF4VSI_CMP          (0xC0)
#define BIT_RX_HDMI_CLR_BUFFER_USE_AIF4VSI          (0x20)
#define BIT_RX_HDMI_CLR_BUFFER_VSI_CLR_W_AVI        (0x10)
#define BIT_RX_HDMI_CLR_BUFFER_VSI_IEEE_ID_CHK_EN   (0x08)
#define BIT_RX_HDMI_CLR_BUFFER_SWAP_VSI_IEEE_ID     (0x04)
#define BIT_RX_HDMI_CLR_BUFFER_AIF_CLR_EN           (0x02)

#define BIT_RX_HDMI_CLR_BUFFER_VSI_CLR_EN           (0x01)
#define VAL_RX_HDMI_CLR_BUFFER_VSI_CLR_EN_STALE     (0x00)
#define VAL_RX_HDMI_CLR_BUFFER_VSI_CLR_EN_CLEAR     (0x01)

/* 0xB8 RX_HDMI VSI Header1 Register                       (Default: 0x00)    */
#define REG_RX_HDMI_MON_PKT_HEADER1                 (0x02B8)
#define MSK_RX_HDMI_MON_PKT_HEADER1_RX_HDMI_MON_PKT_HEADER_7_0 (0xFF)

/* 0xD7 RX_HDMI VSI MHL Monitor Register                   (Default: 0x3C)    */
#define REG_RX_HDMI_VSIF_MHL_MON                    (0x02D7)

#define MSK_RX_HDMI_VSIF_MHL_MON_RX_HDMI_MHL_3D_FORMAT (0x3C)
#define MSK_RX_HDMI_VSIF_MHL_MON_RX_HDMI_MHL_VID_FORMAT (0x03)

/* 0xE0 Interrupt Source 9 Register                        (Default: 0x00)    */
#define REG_INTR9                                   (0x02E0)
#define BIT_INTR9_EDID_ERROR		BIT(6)
#define BIT_INTR9_EDID_DONE		BIT(5)
#define BIT_INTR9_DEVCAP_DONE		BIT(4)

/* 0xE1 Interrupt 9 Mask Register                          (Default: 0x00)    */
#define REG_INTR9_MASK                              (0x02E1)

/* 0xE2 TPI CBUS Start Register                            (Default: 0x00)    */
#define REG_TPI_CBUS_START                          (0x02E2)
#define BIT_TPI_CBUS_START_RCP_REQ_START            (0x80)
#define BIT_TPI_CBUS_START_RCPK_REPLY_START         (0x40)
#define BIT_TPI_CBUS_START_RCPE_REPLY_START         (0x20)
#define BIT_TPI_CBUS_START_PUT_LINK_MODE_START      (0x10)
#define BIT_TPI_CBUS_START_PUT_DCAPCHG_START        (0x08)
#define BIT_TPI_CBUS_START_PUT_DCAPRDY_START        (0x04)
#define BIT_TPI_CBUS_START_GET_EDID_START_0         (0x02)
#define BIT_TPI_CBUS_START_GET_DEVCAP_START         (0x01)

/* 0xE3 EDID Control Register                              (Default: 0x10)    */
#define REG_EDID_CTRL                               (0x02E3)
#define BIT_EDID_CTRL_EDID_PRIME_VALID              (0x80)
#define VAL_EDID_CTRL_EDID_PRIME_VALID_DISABLE      (0x00)
#define VAL_EDID_CTRL_EDID_PRIME_VALID_ENABLE       (0x80)

#define BIT_EDID_CTRL_XDEVCAP_EN                    (0x40)
#define BIT_EDID_CTRL_DEVCAP_SEL                    (0x20)
#define VAL_EDID_CTRL_DEVCAP_SELECT_EDID            (0x00)
#define VAL_EDID_CTRL_DEVCAP_SELECT_DEVCAP          (0x20)

#define BIT_EDID_CTRL_EDID_FIFO_ADDR_AUTO           (0x10)
#define VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_DISABLE   (0x00)
#define VAL_EDID_CTRL_EDID_FIFO_ADDR_AUTO_ENABLE    (0x10)

#define BIT_EDID_CTRL_EDID_FIFO_ACCESS_ALWAYS_EN    (0x08)
#define BIT_EDID_CTRL_EDID_FIFO_BLOCK_SEL           (0x04)
#define BIT_EDID_CTRL_INVALID_BKSV                  (0x02)

#define BIT_EDID_CTRL_EDID_MODE_EN                  (0x01)
#define VAL_EDID_CTRL_EDID_MODE_EN_DISABLE          (0x00)
#define VAL_EDID_CTRL_EDID_MODE_EN_ENABLE           (0x01)

/* 0xE9 EDID FIFO Addr Register                            (Default: 0x00)    */
#define REG_EDID_FIFO_ADDR                          (0x02E9)
#define MSK_EDID_FIFO_ADDR_EDID_FIFO_ADDR           (0xFF)

/* 0xEA EDID FIFO Write Data Register                      (Default: 0x00)    */
#define REG_EDID_FIFO_WR_DATA                       (0x02EA)
#define MSK_EDID_FIFO_WR_DATA_EDID_FIFO_WR_DATA     (0xFF)

/* 0xEB EDID/DEVCAP FIFO Internal Addr Register            (Default: 0x00)    */
#define REG_EDID_FIFO_ADDR_MON                      (0x02EB)
#define MSK_EDID_FIFO_ADDR_MON_EDID_FIFO_ADDR_MON   (0xFF)

/* 0xEC EDID FIFO Read Data Register                       (Default: 0x00)    */
#define REG_EDID_FIFO_RD_DATA                       (0x02EC)
#define MSK_EDID_FIFO_RD_DATA_EDID_FIFO_RD_DATA     (0xFF)

/* 0xED EDID DDC Segment Pointer Register                  (Default: 0x00)    */
#define REG_EDID_START_EXT                          (0x02ED)

/* 0xF2 TX IP BIST CNTL and Status Register                (Default: 0x00)    */
#define REG_TX_IP_BIST_CNTLSTA                      (0x02F2)
#define BIT_TX_IP_BIST_CNTLSTA_TXBIST_QUARTER_CLK_SEL (0x40)
#define BIT_TX_IP_BIST_CNTLSTA_TXBIST_DONE          (0x20)
#define BIT_TX_IP_BIST_CNTLSTA_TXBIST_ON            (0x10)
#define BIT_TX_IP_BIST_CNTLSTA_TXBIST_RUN           (0x08)
#define BIT_TX_IP_BIST_CNTLSTA_TXCLK_HALF_SEL       (0x04)
#define BIT_TX_IP_BIST_CNTLSTA_TXBIST_EN            (0x02)
#define BIT_TX_IP_BIST_CNTLSTA_TXBIST_SEL           (0x01)

/* 0xF3 TX IP BIST INST LOW Register                       (Default: 0x00)    */
#define REG_TX_IP_BIST_INST_LOW                     (0x02F3)
#define REG_TX_IP_BIST_INST_HIGH                    (0x02F4)

/* 0xF5 TX IP BIST PATTERN LOW Register                    (Default: 0x00)    */
#define REG_TX_IP_BIST_PAT_LOW                      (0x02F5)
#define REG_TX_IP_BIST_PAT_HIGH                     (0x02F6)

/* 0xF7 TX IP BIST CONFIGURE LOW Register                  (Default: 0x00)    */
#define REG_TX_IP_BIST_CONF_LOW                     (0x02F7)
#define REG_TX_IP_BIST_CONF_HIGH                    (0x02F8)

/* Registers in TX_PAGE_3 (0x00-0xFF)                                         */

/* 0x00 E-MSC General Control Register                     (Default: 0x80)    */
#define REG_GENCTL                                  (0x0300)
#define BIT_GENCTL_SPEC_TRANS_DIS                   (0x80)
#define BIT_GENCTL_DIS_XMIT_ERR_STATE               (0x40)
#define BIT_GENCTL_SPI_MISO_EDGE                    (0x20)
#define BIT_GENCTL_SPI_MOSI_EDGE                    (0x10)
#define BIT_GENCTL_CLR_EMSC_RFIFO                   (0x08)
#define BIT_GENCTL_CLR_EMSC_XFIFO                   (0x04)
#define BIT_GENCTL_START_TRAIN_SEQ                  (0x02)
#define BIT_GENCTL_EMSC_EN                          (0x01)

/* 0x05 E-MSC Comma ErrorCNT Register                      (Default: 0x03)    */
#define REG_COMMECNT                                (0x0305)
#define BIT_COMMECNT_I2C_TO_EMSC_EN                 (0x80)
#define MSK_COMMECNT_COMMA_CHAR_ERR_CNT             (0x0F)

/* 0x1A E-MSC RFIFO ByteCnt Register            (Default: 0x00)    */
#define REG_EMSCRFIFOBCNTL                          (0x031A)
#define REG_EMSCRFIFOBCNTH                          (0x031B)

/* 0x1E SPI Burst Cnt Status Register                      (Default: 0x00)    */
#define REG_SPIBURSTCNT                             (0x031E)
#define MSK_SPIBURSTCNT_SPI_BURST_CNT               (0xFF)

/* 0x22 SPI Burst Status and SWRST Register                (Default: 0x00)    */
#define REG_SPIBURSTSTAT                            (0x0322)
#define BIT_SPIBURSTSTAT_SPI_HDCPRST                (0x80)
#define BIT_SPIBURSTSTAT_SPI_CBUSRST                (0x40)
#define BIT_SPIBURSTSTAT_SPI_SRST                   (0x20)
#define BIT_SPIBURSTSTAT_EMSC_NORMAL_MODE           (0x01)

/* 0x23 E-MSC 1st Interrupt Register                       (Default: 0x00)    */
#define REG_EMSCINTR                                (0x0323)
#define BIT_EMSCINTR_EMSC_XFIFO_EMPTY               (0x80)
#define BIT_EMSCINTR_EMSC_XMIT_ACK_TOUT             (0x40)
#define BIT_EMSCINTR_EMSC_RFIFO_READ_ERR            (0x20)
#define BIT_EMSCINTR_EMSC_XFIFO_WRITE_ERR           (0x10)
#define BIT_EMSCINTR_EMSC_COMMA_CHAR_ERR            (0x08)
#define BIT_EMSCINTR_EMSC_XMIT_DONE                 (0x04)
#define BIT_EMSCINTR_EMSC_XMIT_GNT_TOUT             (0x02)
#define BIT_EMSCINTR_SPI_DVLD                       (0x01)

/* 0x24 E-MSC Interrupt Mask Register                      (Default: 0x00)    */
#define REG_EMSCINTRMASK                            (0x0324)

/* 0x2A I2C E-MSC XMIT FIFO Write Port Register            (Default: 0x00)    */
#define REG_EMSC_XMIT_WRITE_PORT                    (0x032A)

/* 0x2B I2C E-MSC RCV FIFO Write Port Register             (Default: 0x00)    */
#define REG_EMSC_RCV_READ_PORT                      (0x032B)

/* 0x2C E-MSC 2nd Interrupt Register                       (Default: 0x00)    */
#define REG_EMSCINTR1                               (0x032C)
#define BIT_EMSCINTR1_EMSC_TRAINING_COMMA_ERR       (0x01)

/* 0x2D E-MSC Interrupt Mask Register                      (Default: 0x00)    */
#define REG_EMSCINTRMASK1                           (0x032D)
#define BIT_EMSCINTRMASK1_EMSC_INTRMASK1_0          (0x01)

/* 0x30 MHL Top Ctl Register                               (Default: 0x00)    */
#define REG_MHL_TOP_CTL                             (0x0330)
#define BIT_MHL_TOP_CTL_MHL3_DOC_SEL                (0x80)
#define BIT_MHL_TOP_CTL_MHL_PP_SEL                  (0x40)
#define MSK_MHL_TOP_CTL_IF_TIMING_CTL               (0x03)

/* 0x31 MHL DataPath 1st Ctl Register                      (Default: 0xBC)    */
#define REG_MHL_DP_CTL0                             (0x0331)
#define BIT_MHL_DP_CTL0_DP_OE                       (0x80)
#define BIT_MHL_DP_CTL0_TX_OE_OVR                   (0x40)
#define MSK_MHL_DP_CTL0_TX_OE                       (0x3F)

/* 0x32 MHL DataPath 2nd Ctl Register                      (Default: 0xBB)    */
#define REG_MHL_DP_CTL1                             (0x0332)
#define MSK_MHL_DP_CTL1_CK_SWING_CTL                (0xF0)
#define MSK_MHL_DP_CTL1_DT_SWING_CTL                (0x0F)

/* 0x33 MHL DataPath 3rd Ctl Register                      (Default: 0x2F)    */
#define REG_MHL_DP_CTL2                             (0x0333)
#define BIT_MHL_DP_CTL2_CLK_BYPASS_EN               (0x80)
#define MSK_MHL_DP_CTL2_DAMP_TERM_SEL               (0x30)
#define MSK_MHL_DP_CTL2_CK_TERM_SEL                 (0x0C)
#define MSK_MHL_DP_CTL2_DT_TERM_SEL                 (0x03)

/* 0x34 MHL DataPath 4th Ctl Register                      (Default: 0x48)    */
#define REG_MHL_DP_CTL3                             (0x0334)
#define MSK_MHL_DP_CTL3_DT_DRV_VNBC_CTL             (0xF0)
#define MSK_MHL_DP_CTL3_DT_DRV_VNB_CTL              (0x0F)

/* 0x35 MHL DataPath 5th Ctl Register                      (Default: 0x48)    */
#define REG_MHL_DP_CTL4                             (0x0335)
#define MSK_MHL_DP_CTL4_CK_DRV_VNBC_CTL             (0xF0)
#define MSK_MHL_DP_CTL4_CK_DRV_VNB_CTL              (0x0F)

/* 0x36 MHL DataPath 6th Ctl Register                      (Default: 0x3F)    */
#define REG_MHL_DP_CTL5                             (0x0336)
#define BIT_MHL_DP_CTL5_RSEN_EN_OVR                 (0x80)
#define BIT_MHL_DP_CTL5_RSEN_EN                     (0x40)
#define MSK_MHL_DP_CTL5_DAMP_TERM_VGS_CTL           (0x30)
#define MSK_MHL_DP_CTL5_CK_TERM_VGS_CTL             (0x0C)
#define MSK_MHL_DP_CTL5_DT_TERM_VGS_CTL             (0x03)

/* 0x37 MHL PLL 1st Ctl Register                           (Default: 0x05)    */
#define REG_MHL_PLL_CTL0                            (0x0337)
#define BIT_MHL_PLL_CTL0_AUD_CLK_EN                 (0x80)

#define MSK_MHL_PLL_CTL0_AUD_CLK_RATIO              (0x70)
#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_10         (0x70)
#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_6          (0x60)
#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_4          (0x50)
/*TODO:#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_2          (0x40)*/
#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_5          (0x30)
#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_3          (0x20)
#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_2_PRIME    (0x10)
#define VAL_MHL_PLL_CTL0_AUD_CLK_RATIO_5_1          (0x00)

#define MSK_MHL_PLL_CTL0_HDMI_CLK_RATIO             (0x0C)
#define VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_4X          (0x0C)
#define VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_2X          (0x08)
#define VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_1X          (0x04)
#define VAL_MHL_PLL_CTL0_HDMI_CLK_RATIO_HALF_X      (0x00)

#define BIT_MHL_PLL_CTL0_CRYSTAL_CLK_SEL            (0x02)
#define BIT_MHL_PLL_CTL0_ZONE_MASK_OE               (0x01)

/* 0x39 MHL PLL 3rd Ctl Register                           (Default: 0x80)    */
#define REG_MHL_PLL_CTL2                            (0x0339)
#define BIT_MHL_PLL_CTL2_CLKDETECT_EN               (0x80)
#define BIT_MHL_PLL_CTL2_MEAS_FVCO                  (0x08)
#define BIT_MHL_PLL_CTL2_PLL_FAST_LOCK              (0x04)
#define MSK_MHL_PLL_CTL2_PLL_LF_SEL                 (0x03)

/* 0x40 MHL CBUS 1st Ctl Register                          (Default: 0x12)    */
#define REG_MHL_CBUS_CTL0                           (0x0340)
#define BIT_MHL_CBUS_CTL0_CBUS_RGND_TEST_MODE       (0x80)

#define MSK_MHL_CBUS_CTL0_CBUS_RGND_VTH_CTL         (0x30)
#define VAL_MHL_CBUS_CTL0_CBUS_RGND_VBIAS_734       (0x00)
#define VAL_MHL_CBUS_CTL0_CBUS_RGND_VBIAS_747       (0x10)
#define VAL_MHL_CBUS_CTL0_CBUS_RGND_VBIAS_740       (0x20)
#define VAL_MHL_CBUS_CTL0_CBUS_RGND_VBIAS_754       (0x30)

#define MSK_MHL_CBUS_CTL0_CBUS_RES_TEST_SEL         (0x0C)

#define MSK_MHL_CBUS_CTL0_CBUS_DRV_SEL              (0x03)
#define VAL_MHL_CBUS_CTL0_CBUS_DRV_SEL_WEAKEST      (0x00)
#define VAL_MHL_CBUS_CTL0_CBUS_DRV_SEL_WEAK         (0x01)
#define VAL_MHL_CBUS_CTL0_CBUS_DRV_SEL_STRONG       (0x02)
#define VAL_MHL_CBUS_CTL0_CBUS_DRV_SEL_STRONGEST    (0x03)

/* 0x41 MHL CBUS 2nd Ctl Register                          (Default: 0x03)    */
#define REG_MHL_CBUS_CTL1                           (0x0341)
#define MSK_MHL_CBUS_CTL1_CBUS_RGND_RES_CTL         (0x07)
#define VAL_MHL_CBUS_CTL1_0888_OHM                  (0x00)
#define VAL_MHL_CBUS_CTL1_1115_OHM                  (0x04)
#define VAL_MHL_CBUS_CTL1_1378_OHM                  (0x07)

/* 0x42 MHL CoC 1st Ctl Register                           (Default: 0xC3)    */
#define REG_MHL_COC_CTL0                            (0x0342)
#define BIT_MHL_COC_CTL0_COC_BIAS_EN                (0x80)
#define MSK_MHL_COC_CTL0_COC_BIAS_CTL               (0x70)
#define MSK_MHL_COC_CTL0_COC_TERM_CTL               (0x07)

/* 0x43 MHL CoC 2nd Ctl Register                           (Default: 0x87)    */
#define REG_MHL_COC_CTL1                            (0x0343)
#define BIT_MHL_COC_CTL1_COC_EN                     (0x80)
#define MSK_MHL_COC_CTL1_COC_DRV_CTL                (0x3F)

/* 0x45 MHL CoC 4th Ctl Register                           (Default: 0x00)    */
#define REG_MHL_COC_CTL3                            (0x0345)
#define BIT_MHL_COC_CTL3_COC_AECHO_EN               (0x01)

/* 0x46 MHL CoC 5th Ctl Register                           (Default: 0x28)    */
#define REG_MHL_COC_CTL4                            (0x0346)
#define MSK_MHL_COC_CTL4_COC_IF_CTL                 (0xF0)
#define MSK_MHL_COC_CTL4_COC_SLEW_CTL               (0x0F)

/* 0x47 MHL CoC 6th Ctl Register                           (Default: 0x0D)    */
#define REG_MHL_COC_CTL5                            (0x0347)
#define MSK_MHL_COC_CTL5_COC_RSV_7_0                (0xFF)

/* 0x49 MHL DoC 1st Ctl Register                           (Default: 0x18)    */
#define REG_MHL_DOC_CTL0                            (0x0349)
#define BIT_MHL_DOC_CTL0_DOC_RXDATA_EN              (0x80)
#define MSK_MHL_DOC_CTL0_DOC_DM_TERM                (0x38)
#define MSK_MHL_DOC_CTL0_DOC_OPMODE                 (0x06)
#define BIT_MHL_DOC_CTL0_DOC_RXBIAS_EN              (0x01)

/* 0x50 MHL DataPath 7th Ctl Register                      (Default: 0x2A)    */
#define REG_MHL_DP_CTL6                             (0x0350)
#define BIT_MHL_DP_CTL6_DP_TAP2_SGN                 (0x20)
#define BIT_MHL_DP_CTL6_DP_TAP2_EN                  (0x10)
#define BIT_MHL_DP_CTL6_DP_TAP1_SGN                 (0x08)
#define BIT_MHL_DP_CTL6_DP_TAP1_EN                  (0x04)
#define BIT_MHL_DP_CTL6_DT_PREDRV_FEEDCAP_EN        (0x02)
#define BIT_MHL_DP_CTL6_DP_PRE_POST_SEL             (0x01)

/* 0x51 MHL DataPath 8th Ctl Register                      (Default: 0x06)    */
#define REG_MHL_DP_CTL7                             (0x0351)
#define MSK_MHL_DP_CTL7_DT_DRV_VBIAS_CASCTL         (0xF0)
#define MSK_MHL_DP_CTL7_DT_DRV_IREF_CTL             (0x0F)

/* 0x61 Tx Zone Ctl1 Register                              (Default: 0x00)    */
#define REG_TX_ZONE_CTL1                            (0x0361)
#define MSK_TX_ZONE_CTL1_TX_ZONE_CTRL               (0xFF)
#define VAL_TX_ZONE_CTL1_TX_ZONE_CTRL_MODE          (0x08)

/* 0x64 MHL3 Tx Zone Ctl Register                          (Default: 0x00)    */
#define REG_MHL3_TX_ZONE_CTL                        (0x0364)
#define BIT_MHL3_TX_ZONE_CTL_MHL2_INTPLT_ZONE_MANU_EN (0x80)
#define MSK_MHL3_TX_ZONE_CTL_MHL3_TX_ZONE           (0x03)

#define MSK_TX_ZONE_CTL3_TX_ZONE                    (0x03)
#define VAL_TX_ZONE_CTL3_TX_ZONE_6GBPS              (0x00)
#define VAL_TX_ZONE_CTL3_TX_ZONE_3GBPS              (0x01)
#define VAL_TX_ZONE_CTL3_TX_ZONE_1_5GBPS            (0x02)

/* 0x91 HDCP Polling Control and Status Register           (Default: 0x70)    */
#define REG_HDCP2X_POLL_CS                          (0x0391)

#define BIT_HDCP2X_POLL_CS_HDCP2X_MSG_SZ_CLR_OPTION (0x40)
#define BIT_HDCP2X_POLL_CS_HDCP2X_RPT_READY_CLR_OPTION (0x20)
#define BIT_HDCP2X_POLL_CS_HDCP2X_REAUTH_REQ_CLR_OPTION (0x10)
#define MSK_HDCP2X_POLL_CS_                         (0x0C)
#define BIT_HDCP2X_POLL_CS_HDCP2X_DIS_POLL_GNT      (0x02)
#define BIT_HDCP2X_POLL_CS_HDCP2X_DIS_POLL_EN       (0x01)

/* 0x98 HDCP Interrupt 0 Register                          (Default: 0x00)    */
#define REG_HDCP2X_INTR0                            (0x0398)

/* 0x99 HDCP Interrupt 0 Mask Register                     (Default: 0x00)    */
#define REG_HDCP2X_INTR0_MASK                       (0x0399)

/* 0xA0 HDCP General Control 0 Register                    (Default: 0x02)    */
#define REG_HDCP2X_CTRL_0                           (0x03A0)
#define BIT_HDCP2X_CTRL_0_HDCP2X_ENCRYPT_EN         (0x80)
#define BIT_HDCP2X_CTRL_0_HDCP2X_POLINT_SEL         (0x40)
#define BIT_HDCP2X_CTRL_0_HDCP2X_POLINT_OVR         (0x20)
#define BIT_HDCP2X_CTRL_0_HDCP2X_PRECOMPUTE         (0x10)
#define BIT_HDCP2X_CTRL_0_HDCP2X_HDMIMODE           (0x08)
#define BIT_HDCP2X_CTRL_0_HDCP2X_REPEATER           (0x04)
#define BIT_HDCP2X_CTRL_0_HDCP2X_HDCPTX             (0x02)
#define BIT_HDCP2X_CTRL_0_HDCP2X_EN                 (0x01)

/* 0xA1 HDCP General Control 1 Register                    (Default: 0x08)    */
#define REG_HDCP2X_CTRL_1                           (0x03A1)
#define MSK_HDCP2X_CTRL_1_HDCP2X_REAUTH_MSK_3_0     (0xF0)
#define BIT_HDCP2X_CTRL_1_HDCP2X_HPD_SW             (0x08)
#define BIT_HDCP2X_CTRL_1_HDCP2X_HPD_OVR            (0x04)
#define BIT_HDCP2X_CTRL_1_HDCP2X_CTL3MSK            (0x02)
#define BIT_HDCP2X_CTRL_1_HDCP2X_REAUTH_SW          (0x01)

/* 0xA5 HDCP Misc Control Register                         (Default: 0x00)    */
#define REG_HDCP2X_MISC_CTRL                        (0x03A5)
#define BIT_HDCP2X_MISC_CTRL_HDCP2X_RPT_SMNG_XFER_START (0x10)
#define BIT_HDCP2X_MISC_CTRL_HDCP2X_RPT_SMNG_WR_START (0x08)
#define BIT_HDCP2X_MISC_CTRL_HDCP2X_RPT_SMNG_WR     (0x04)
#define BIT_HDCP2X_MISC_CTRL_HDCP2X_RPT_RCVID_RD_START (0x02)
#define BIT_HDCP2X_MISC_CTRL_HDCP2X_RPT_RCVID_RD    (0x01)

/* 0xA6 HDCP RPT SMNG K Register                           (Default: 0x00)    */
#define REG_HDCP2X_RPT_SMNG_K                       (0x03A6)
#define MSK_HDCP2X_RPT_SMNG_K_HDCP2X_RPT_SMNG_K_7_0 (0xFF)

/* 0xA7 HDCP RPT SMNG In Register                          (Default: 0x00)    */
#define REG_HDCP2X_RPT_SMNG_IN                      (0x03A7)
#define MSK_HDCP2X_RPT_SMNG_IN_HDCP2X_RPT_SMNG_IN   (0xFF)

/* 0xAA HDCP Auth Status Register                          (Default: 0x00)    */
#define REG_HDCP2X_AUTH_STAT                        (0x03AA)
#define MSK_HDCP2X_AUTH_STAT_HDCP2X_AUTH_STAT_7_0   (0xFF)

/* 0xAC HDCP RPT RCVID Out Register                        (Default: 0x00)    */
#define REG_HDCP2X_RPT_RCVID_OUT                    (0x03AC)
#define MSK_HDCP2X_RPT_RCVID_OUT_HDCP2X_RPT_RCVID_OUT_7_0 (0xFF)

/* 0xB4 HDCP TP1  Register                                 (Default: 0x62)    */
#define REG_HDCP2X_TP1                              (0x03B4)
#define MSK_HDCP2X_TP1_HDCP2X_TP1_7_0               (0xFF)

/* 0xC7 HDCP GP Out 0 Register                             (Default: 0x00)    */
#define REG_HDCP2X_GP_OUT0                          (0x03C7)
#define MSK_HDCP2X_GP_OUT0_HDCP2X_GP_OUT0_7_0       (0xFF)

/* 0xD1 HDCP Repeater RCVR ID 0 Register                   (Default: 0x00)    */
#define REG_HDCP2X_RPT_RCVR_ID0                     (0x03D1)
#define MSK_HDCP2X_RPT_RCVR_ID0_HDCP2X_RCVR_ID_7_0  (0xFF)

/* 0xD8 HDCP DDCM Status Register                          (Default: 0x00)    */
#define REG_HDCP2X_DDCM_STS                         (0x03D8)
#define MSK_HDCP2X_DDCM_STS_HDCP2X_DDCM_ERR_STS_3_0 (0xF0)
#define MSK_HDCP2X_DDCM_STS_HDCP2X_DDCM_CTL_CS_3_0  (0x0F)

/* 0xE0 HDMI2MHL3 Control Register                         (Default: 0x0A)    */
#define REG_M3_CTRL                                 (0x03E0)
#define BIT_M3_CTRL_H2M_SWRST                       (0x10)
#define BIT_M3_CTRL_SW_MHL3_SEL                     (0x08)
#define BIT_M3_CTRL_M3AV_EN                         (0x04)
#define BIT_M3_CTRL_ENC_TMDS                        (0x02)
#define BIT_M3_CTRL_MHL3_MASTER_EN                  (0x01)

#define VAL_M3_CTRL_MHL1_2_VALUE (BIT_M3_CTRL_SW_MHL3_SEL | \
	BIT_M3_CTRL_ENC_TMDS)
#define VAL_M3_CTRL_MHL3_VALUE (BIT_M3_CTRL_SW_MHL3_SEL \
				| BIT_M3_CTRL_M3AV_EN \
				| BIT_M3_CTRL_ENC_TMDS \
				| BIT_M3_CTRL_MHL3_MASTER_EN)

/* 0xE1 HDMI2MHL3 Port0 Control Register                   (Default: 0x04)    */
#define REG_M3_P0CTRL                               (0x03E1)
#define BIT_M3_P0CTRL_MHL3_P0_HDCP_ENC_EN           (0x10)
#define BIT_M3_P0CTRL_MHL3_P0_UNLIMIT_EN            (0x08)
#define BIT_M3_P0CTRL_MHL3_P0_HDCP_EN               (0x04)

#define BIT_M3_P0CTRL_MHL3_P0_PIXEL_MODE            (0x02)
#define VAL_M3_P0CTRL_MHL3_P0_PIXEL_MODE_NORMAL     (0x00)
#define VAL_M3_P0CTRL_MHL3_P0_PIXEL_MODE_PACKED     (0x02)

#define BIT_M3_P0CTRL_MHL3_P0_PORT_EN               (0x01)

#define REG_M3_POSTM				   (0x03E2)
#define MSK_M3_POSTM_RRP_DECODE			   (0xF8)
#define MSK_M3_POSTM_MHL3_P0_STM_ID		   (0x07)

/* 0xE6 HDMI2MHL3 Scramble Control Register                (Default: 0x41)    */
#define REG_M3_SCTRL                                (0x03E6)
#define MSK_M3_SCTRL_MHL3_SR_LENGTH                 (0xF0)
#define BIT_M3_SCTRL_MHL3_SCRAMBLER_EN              (0x01)

/* 0xF2 HSIC Div Ctl Register                              (Default: 0x05)    */
#define REG_DIV_CTL_MAIN                            (0x03F2)
#define MSK_DIV_CTL_MAIN_PRE_DIV_CTL_MAIN           (0x1C)
#define MSK_DIV_CTL_MAIN_FB_DIV_CTL_MAIN            (0x03)

/* Registers in TX_PAGE_4 (0x00-0xFF)                                         */

/* 0x00 MHL Capability 1st Byte Register                   (Default: 0x00)    */
#define REG_MHL_DEVCAP_0                            (0x0400)
#define MSK_MHL_DEVCAP_0_MHL_DEVCAP_0               (0xFF)

/* 0x20 MHL Interrupt 1st Byte Register                    (Default: 0x00)    */
#define REG_MHL_INT_0                               (0x0420)

/* 0x30 Device Status 1st byte Register                    (Default: 0x00)    */
#define REG_MHL_STAT_0                              (0x0430)
#define MSK_MHL_STAT_0_MHL_STAT_0                   (0xFF)

/* 0x40 CBUS Scratch Pad 1st Byte Register                 (Default: 0x00)    */
#define REG_MHL_SCRPAD_0                            (0x0440)
#define MSK_MHL_SCRPAD_0_MHL_SCRPAD_0               (0xFF)

/* 0x80 MHL Extended Capability 1st Byte Register          (Default: 0x00)    */
#define REG_MHL_EXTDEVCAP_0                         (0x0480)
#define MSK_MHL_EXTDEVCAP_0_MHL_EXTDEVCAP_0         (0xFF)

/* 0x90 Device Extended Status 1st byte Register           (Default: 0x00)    */
#define REG_MHL_EXTSTAT_0                           (0x0490)
#define MSK_MHL_EXTSTAT_0_MHL_EXTSTAT_0             (0xFF)

/* 0x02 TPI DTD Byte2 Register                             (Default: 0x00)    */
#define REG_TPI_DTD_B2                              (0x0602)
#define MSK_TPI_DTD_B2_DTDB2                        (0xFF)

#define VAL_TPI_QUAN_RANGE_LIMITED		(1)
#define VAL_TPI_QUAN_RANGE_FULL			(2)
#define VAL_TPI_FORMAT_RGB                        (0x00)
#define VAL_TPI_FORMAT_YCBCR444                   (0x01)
#define VAL_TPI_FORMAT_YCBCR422                   (0x02)
#define VAL_TPI_FORMAT_INTERNAL_RGB               (0x03)
#define VAL_TPI_FORMAT(_fmt, _qr) \
		(VAL_TPI_FORMAT_##_fmt | (VAL_TPI_QUAN_RANGE_##_qr << 2))

/* 0x09 Input Format Register                              (Default: 0x00)    */
#define REG_TPI_INPUT                               (0x0609)
#define BIT_TPI_INPUT_EXTENDEDBITMODE               (0x80)
#define BIT_TPI_INPUT_ENDITHER                      (0x40)
#define MSK_TPI_INPUT_INPUT_QUAN_RANGE              (0x0C)
#define MSK_TPI_INPUT_INPUT_FORMAT                  (0x03)

/* 0x0A Output Format Register                             (Default: 0x00)    */
#define REG_TPI_OUTPUT                              (0x060A)
#define BIT_TPI_OUTPUT_CSCMODE709                   (0x10)
#define MSK_TPI_OUTPUT_OUTPUT_QUAN_RANGE            (0x0C)
#define MSK_TPI_OUTPUT_OUTPUT_FORMAT                (0x03)

/* 0x0C TPI AVI Check Sum Register                         (Default: 0x00)    */
#define REG_TPI_AVI_CHSUM                           (0x060C)
#define MSK_TPI_AVI_CHSUM_AVI_CHECKSUM              (0xFF)

/* 0x1A TPI System Control Register                        (Default: 0x00)    */
#define REG_TPI_SC                                  (0x061A)
#define BIT_TPI_SC_TPI_UPDATE_FLG                   (0x80)
#define BIT_TPI_SC_TPI_REAUTH_CTL                   (0x40)
#define BIT_TPI_SC_TPI_OUTPUT_MODE_1                (0x20)
#define BIT_TPI_SC_REG_TMDS_OE                      (0x10)
#define VAL_TPI_SC_REG_TMDS_OE_ACTIVE               (0x00)
#define VAL_TPI_SC_REG_TMDS_OE_POWER_DOWN           (0x10)
#define BIT_TPI_SC_TPI_AV_MUTE                      (0x08)
#define VAL_TPI_SC_TPI_AV_MUTE_NORMAL               (0x00)
#define VAL_TPI_SC_TPI_AV_MUTE_MUTED                (0x08)
#define BIT_TPI_SC_DDC_GPU_REQUEST                  (0x04)
#define BIT_TPI_SC_DDC_TPI_SW                       (0x02)
#define BIT_TPI_SC_TPI_OUTPUT_MODE_0                (0x01)
#define VAL_TPI_SC_TPI_OUTPUT_MODE_0_DVI            (0x00)
#define VAL_TPI_SC_TPI_OUTPUT_MODE_0_HDMI           (0x01)

/* 0x29 TPI COPP Query Data Register                       (Default: 0x00)    */
#define REG_TPI_COPP_DATA1                          (0x0629)
#define BIT_TPI_COPP_DATA1_COPP_GPROT               (0x80)
#define VAL_TPI_COPP_GPROT_NONE                            (0x00)
#define VAL_TPI_COPP_GPROT_SECURE                          (0x80)

#define BIT_TPI_COPP_DATA1_COPP_LPROT               (0x40)
#define VAL_TPI_COPP_LPROT_NONE                            (0x00)
#define VAL_TPI_COPP_LPROT_SECURE                          (0x40)

#define MSK_TPI_COPP_DATA1_COPP_LINK_STATUS         (0x30)
#define VAL_TPI_COPP_LINK_STATUS_NORMAL                    (0x00)
#define VAL_TPI_COPP_LINK_STATUS_LINK_LOST                 (0x10)
#define VAL_TPI_COPP_LINK_STATUS_RENEGOTIATION_REQ         (0x20)
#define VAL_TPI_COPP_LINK_STATUS_LINK_SUSPENDED            (0x30)

#define BIT_TPI_COPP_DATA1_COPP_HDCP_REP            (0x08)
#define BIT_TPI_COPP_DATA1_COPP_CONNTYPE_0          (0x04)
#define BIT_TPI_COPP_DATA1_COPP_PROTYPE             (0x02)
#define BIT_TPI_COPP_DATA1_COPP_CONNTYPE_1          (0x01)

/* 0x2A TPI COPP Control Data Register                     (Default: 0x00)    */
#define REG_TPI_COPP_DATA2                          (0x062A)
#define BIT_TPI_COPP_DATA2_INTR_ENCRYPTION          (0x20)
#define BIT_TPI_COPP_DATA2_KSV_FORWARD              (0x10)
#define BIT_TPI_COPP_DATA2_INTERM_RI_CHECK_EN       (0x08)
#define BIT_TPI_COPP_DATA2_DOUBLE_RI_CHECK          (0x04)
#define BIT_TPI_COPP_DATA2_DDC_SHORT_RI_RD          (0x02)

#define BIT_TPI_COPP_DATA2_COPP_PROTLEVEL           (0x01)
#define VAL_TPI_COPP_PROTLEVEL_MIN                  (0x00)
#define VAL_TPI_COPP_PROTLEVEL_MAX                  (0x01)

/* 0x3C TPI Interrupt Enable Register                      (Default: 0x00)    */
#define REG_TPI_INTR_EN                             (0x063C)

/* 0x3D TPI Interrupt Status Low Byte Register             (Default: 0x00)    */
#define REG_TPI_INTR_ST0                            (0x063D)
#define BIT_TPI_INTR_ST0_TPI_AUTH_CHNGE_STAT        (0x80)
#define BIT_TPI_INTR_ST0_TPI_V_RDY_STAT             (0x40)
#define BIT_TPI_INTR_ST0_TPI_COPP_CHNGE_STAT        (0x20)
#define BIT_TPI_INTR_ST0_KSV_FIFO_FIRST_STAT        (0x08)
#define BIT_TPI_INTR_ST0_READ_BKSV_BCAPS_DONE_STAT  (0x04)
#define BIT_TPI_INTR_ST0_READ_BKSV_BCAPS_ERR_STAT   (0x02)
#define BIT_TPI_INTR_ST0_READ_BKSV_ERR_STAT         (0x01)

/* 0x44 TPI DS BCAPS Status Register                       (Default: 0x00)    */
#define REG_TPI_DS_BCAPS                            (0x0644)
#define MSK_TPI_DS_BCAPS_DS_BCAPS                   (0xFF)

/* 0x45 TPI BStatus1 Register                              (Default: 0x00)    */
#define REG_TPI_BSTATUS1                            (0x0645)
#define BIT_TPI_BSTATUS1_DS_DEV_EXCEED              (0x80)
#define MSK_TPI_BSTATUS1_DS_DEV_CNT                 (0x7F)

/* 0x46 TPI BStatus2 Register                              (Default: 0x10)    */
#define REG_TPI_BSTATUS2                            (0x0646)
#define MSK_TPI_BSTATUS2_DS_BSTATUS                 (0xE0)
#define BIT_TPI_BSTATUS2_DS_HDMI_MODE               (0x10)
#define BIT_TPI_BSTATUS2_DS_CASC_EXCEED             (0x08)
#define MSK_TPI_BSTATUS2_DS_DEPTH                   (0x07)

/* 0xBB TPI HW Optimization Control #3 Register            (Default: 0x00)    */
#define REG_TPI_HW_OPT3                             (0x06BB)
#define BIT_TPI_HW_OPT3_DDC_DEBUG                   (0x80)
#define BIT_TPI_HW_OPT3_RI_CHECK_SKIP               (0x08)
#define BIT_TPI_HW_OPT3_TPI_DDC_BURST_MODE          (0x04)
#define MSK_TPI_HW_OPT3_TPI_DDC_REQ_LEVEL           (0x03)

/* 0xBF TPI Info Frame Select Register                     (Default: 0x00)    */
#define REG_TPI_INFO_FSEL                           (0x06BF)
#define BIT_TPI_INFO_FSEL_TPI_INFO_EN               (0x80)
#define BIT_TPI_INFO_FSEL_TPI_INFO_RPT              (0x40)
#define BIT_TPI_INFO_FSEL_TPI_INFO_READ_FLAG        (0x20)
#define MSK_TPI_INFO_FSEL_TPI_INFO_SEL              (0x07)

/* 0xC0 TPI Info Byte #0 Register                          (Default: 0x00)    */
#define REG_TPI_INFO_B0                             (0x06C0)
#define MSK_TPI_INFO_B0_MODE_BYTE0                  (0xFF)

/* Registers in TX_PAGE_7 (0x00-0xFF)                                         */

/* 0x00 CoC 1st Status Register                            (Default: 0x00)    */
#define REG_COC_STAT_0                              (0x0700)

/* 0x01 - 0x02 CoC Status Registers                        (Default: 0x00)    */
#define REG_COC_STAT_1                              (0x0701)
#define REG_COC_STAT_2                              (0x0702)
#define REG_COC_STAT_3                              (0x0703)
#define REG_COC_STAT_4                              (0x0704)
#define REG_COC_STAT_5                              (0x0705)

/* 0x10 CoC 1st Ctl Register                               (Default: 0x40)    */
#define REG_COC_CTL0                                (0x0710)

/* 0x11 CoC 2nd Ctl Register                               (Default: 0x0A)    */
#define REG_COC_CTL1                                (0x0711)
#define MSK_COC_CTL1_COC_CONTROL1_7_6               (0xC0)
#define MSK_COC_CTL1_COC_CONTROL1_5_0               (0x3F)

/* 0x12 CoC 3rd Ctl Register                               (Default: 0x14)    */
#define REG_COC_CTL2                                (0x0712)
#define MSK_COC_CTL2_COC_CONTROL2_7_6               (0xC0)
#define MSK_COC_CTL2_COC_CONTROL2_5_0               (0x3F)

/* 0x13 CoC 4th Ctl Register                               (Default: 0x40)    */
#define REG_COC_CTL3                                (0x0713)
#define BIT_COC_CTL3_COC_CONTROL3_7                 (0x80)
#define MSK_COC_CTL3_COC_CONTROL3_6_0               (0x7F)

/* 0x16 CoC 7th Ctl Register                               (Default: 0x00)    */
#define REG_COC_CTL6                                (0x0716)
#define BIT_COC_CTL6_COC_CONTROL6_7                 (0x80)
#define BIT_COC_CTL6_COC_CONTROL6_6                 (0x40)
#define MSK_COC_CTL6_COC_CONTROL6_5_0               (0x3F)

/* 0x17 CoC 8th Ctl Register                               (Default: 0x06)    */
#define REG_COC_CTL7                                (0x0717)
#define BIT_COC_CTL7_COC_CONTROL7_7                 (0x80)
#define BIT_COC_CTL7_COC_CONTROL7_6                 (0x40)
#define BIT_COC_CTL7_COC_CONTROL7_5                 (0x20)
#define MSK_COC_CTL7_COC_CONTROL7_4_3               (0x18)
#define MSK_COC_CTL7_COC_CONTROL7_2_0               (0x07)

/* 0x19 CoC 10th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTL9                                (0x0719)
#define MSK_COC_CTL9_COC_CONTROL9                   (0xFF)

/* 0x1A CoC 11th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTLA                                (0x071A)
#define MSK_COC_CTLA_COC_CONTROLA                   (0xFF)

/* 0x1B CoC 12th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTLB                                (0x071B)
#define MSK_COC_CTLB_COC_CONTROLB                   (0xFF)

/* 0x1C CoC 13th Ctl Register                              (Default: 0x0F)    */
#define REG_COC_CTLC                                (0x071C)
#define MSK_COC_CTLC_COC_CONTROLC                   (0xFF)

/* 0x1D CoC 14th Ctl Register                              (Default: 0x0A)    */
#define REG_COC_CTLD                                (0x071D)
#define BIT_COC_CTLD_COC_CONTROLD_7                 (0x80)
#define MSK_COC_CTLD_COC_CONTROLD_6_0               (0x7F)

/* 0x1E CoC 15th Ctl Register                              (Default: 0x0A)    */
#define REG_COC_CTLE                                (0x071E)
#define BIT_COC_CTLE_COC_CONTROLE_7                 (0x80)
#define MSK_COC_CTLE_COC_CONTROLE_6_0               (0x7F)

/* 0x1F CoC 16th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTLF                                (0x071F)
#define MSK_COC_CTLF_COC_CONTROLF_7_3               (0xF8)
#define MSK_COC_CTLF_COC_CONTROLF_2_0               (0x07)

/* 0x21 CoC 18th Ctl Register                              (Default: 0x32)    */
#define REG_COC_CTL11                               (0x0721)
#define MSK_COC_CTL11_COC_CONTROL11_7_4             (0xF0)
#define MSK_COC_CTL11_COC_CONTROL11_3_0             (0x0F)

/* 0x24 CoC 21st Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTL14                               (0x0724)
#define MSK_COC_CTL14_COC_CONTROL14_7_4             (0xF0)
#define MSK_COC_CTL14_COC_CONTROL14_3_0             (0x0F)

/* 0x25 CoC 22nd Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTL15                               (0x0725)
#define BIT_COC_CTL15_COC_CONTROL15_7               (0x80)
#define MSK_COC_CTL15_COC_CONTROL15_6_4             (0x70)
#define MSK_COC_CTL15_COC_CONTROL15_3_0             (0x0F)

/* 0x26 CoC Interrupt Register                             (Default: 0x00)    */
#define REG_COC_INTR                                (0x0726)

/* 0x27 CoC Interrupt Mask Register                        (Default: 0x00)    */
#define REG_COC_INTR_MASK                           (0x0727)
#define BIT_COC_PLL_LOCK_STATUS_CHANGE	BIT(0)
#define BIT_COC_CALIBRATION_DONE	BIT(1)

/* 0x28 CoC Misc Ctl Register                              (Default: 0x00)    */
#define REG_COC_MISC_CTL0                           (0x0728)
#define BIT_COC_MISC_CTL0_FSM_MON                   (0x80)

/* 0x2A CoC 24th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTL17                               (0x072A)
#define MSK_COC_CTL17_COC_CONTROL17_7_4             (0xF0)
#define MSK_COC_CTL17_COC_CONTROL17_3_0             (0x0F)

/* 0x2B CoC 25th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTL18                               (0x072B)
#define MSK_COC_CTL18_COC_CONTROL18_7_4             (0xF0)
#define MSK_COC_CTL18_COC_CONTROL18_3_0             (0x0F)

/* 0x2C CoC 26th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTL19                               (0x072C)
#define MSK_COC_CTL19_COC_CONTROL19_7_4             (0xF0)
#define MSK_COC_CTL19_COC_CONTROL19_3_0             (0x0F)

/* 0x2D CoC 27th Ctl Register                              (Default: 0x00)    */
#define REG_COC_CTL1A                               (0x072D)
#define MSK_COC_CTL1A_COC_CONTROL1A_7_2             (0xFC)
#define MSK_COC_CTL1A_COC_CONTROL1A_1_0             (0x03)

/* 0x40 DoC 9th Status Register                            (Default: 0x00)    */
#define REG_DOC_STAT_8                              (0x0740)
#define MSK_DOC_STAT_8_DOC_STATUS8_7_0              (0xFF)

/* 0x41 DoC 10th Status Register                           (Default: 0x00)    */
#define REG_DOC_STAT_9                              (0x0741)
#define MSK_DOC_STAT_9_DOC_STATUS9_7_0              (0xFF)

/* 0x4E DoC 5th CFG Register                               (Default: 0x00)    */
#define REG_DOC_CFG4                                (0x074E)
#define MSK_DOC_CFG4_DBG_STATE_DOC_FSM              (0x0F)

/* 0x51 DoC 1st Ctl Register                               (Default: 0x40)    */
#define REG_DOC_CTL0                                (0x0751)

/* 0x57 DoC 7th Ctl Register                               (Default: 0x00)    */
#define REG_DOC_CTL6                                (0x0757)
#define BIT_DOC_CTL6_DOC_CONTROL6_7                 (0x80)
#define BIT_DOC_CTL6_DOC_CONTROL6_6                 (0x40)
#define MSK_DOC_CTL6_DOC_CONTROL6_5_4               (0x30)
#define MSK_DOC_CTL6_DOC_CONTROL6_3_0               (0x0F)

/* 0x58 DoC 8th Ctl Register                               (Default: 0x00)    */
#define REG_DOC_CTL7                                (0x0758)
#define BIT_DOC_CTL7_DOC_CONTROL7_7                 (0x80)
#define BIT_DOC_CTL7_DOC_CONTROL7_6                 (0x40)
#define BIT_DOC_CTL7_DOC_CONTROL7_5                 (0x20)
#define MSK_DOC_CTL7_DOC_CONTROL7_4_3               (0x18)
#define MSK_DOC_CTL7_DOC_CONTROL7_2_0               (0x07)

/* 0x6C DoC 9th Ctl Register                               (Default: 0x00)    */
#define REG_DOC_CTL8                                (0x076C)
#define BIT_DOC_CTL8_DOC_CONTROL8_7                 (0x80)
#define MSK_DOC_CTL8_DOC_CONTROL8_6_4               (0x70)
#define MSK_DOC_CTL8_DOC_CONTROL8_3_2               (0x0C)
#define MSK_DOC_CTL8_DOC_CONTROL8_1_0               (0x03)

/* 0x6D DoC 10th Ctl Register                              (Default: 0x00)    */
#define REG_DOC_CTL9                                (0x076D)
#define MSK_DOC_CTL9_DOC_CONTROL9                   (0xFF)

/* 0x6E DoC 11th Ctl Register                              (Default: 0x00)    */
#define REG_DOC_CTLA                                (0x076E)
#define MSK_DOC_CTLA_DOC_CONTROLA                   (0xFF)

/* 0x72 DoC 15th Ctl Register                              (Default: 0x00)    */
#define REG_DOC_CTLE                                (0x0772)
#define BIT_DOC_CTLE_DOC_CONTROLE_7                 (0x80)
#define BIT_DOC_CTLE_DOC_CONTROLE_6                 (0x40)
#define MSK_DOC_CTLE_DOC_CONTROLE_5_4               (0x30)
#define MSK_DOC_CTLE_DOC_CONTROLE_3_0               (0x0F)

/* Registers in TX_PAGE_5 (0x00-0xFF)                                         */

/* 0x80 Interrupt Mask 1st Register                        (Default: 0x00)    */
#define REG_MHL_INT_0_MASK                          (0x0580)

/* 0x81 Interrupt Mask 2nd Register                        (Default: 0x00)    */
#define REG_MHL_INT_1_MASK                          (0x0581)

/* 0x82 Interrupt Mask 3rd Register                        (Default: 0x00)    */
#define REG_MHL_INT_2_MASK                          (0x0582)

/* 0x83 Interrupt Mask 4th Register                        (Default: 0x00)    */
#define REG_MHL_INT_3_MASK                          (0x0583)

/* 0x84 MDT Receive Time Out Register                      (Default: 0x00)    */
#define REG_MDT_RCV_TIMEOUT                         (0x0584)
#define MSK_MDT_RCV_TIMEOUT_MDT_RCV_TIMEOUT_MAX_MSB (0xFF)

/* 0x85 MDT Transmit Time Out Register                     (Default: 0x00)    */
#define REG_MDT_XMIT_TIMEOUT                        (0x0585)
#define MSK_MDT_XMIT_TIMEOUT_MDT_XMIT_TIMEOUT_MAX_MSB (0xFF)

/* 0x86 MDT Receive Control Register                       (Default: 0x00)    */
#define REG_MDT_RCV_CONTROL                         (0x0586)
#define BIT_MDT_RCV_CONTROL_MDT_RCV_EN              (0x80)
#define BIT_MDT_RCV_CONTROL_MDT_DELAY_RCV_EN        (0x40)
#define BIT_MDT_RCV_CONTROL_MDT_RFIFO_OVER_WR_EN    (0x10)
#define BIT_MDT_RCV_CONTROL_MDT_XFIFO_OVER_WR_EN    (0x08)
#define BIT_MDT_RCV_CONTROL_MDT_DISABLE             (0x04)
#define BIT_MDT_RCV_CONTROL_MDT_RFIFO_CLR_ALL       (0x02)
#define BIT_MDT_RCV_CONTROL_MDT_RFIFO_CLR_CUR       (0x01)

/* 0x87 MDT Receive Read Port                              (Default: 0x00)    */
#define REG_MDT_RCV_READ_PORT                       (0x0587)
#define MSK_MDT_RCV_READ_PORT_MDT_RFIFO_DATA        (0xFF)

/* 0x88 MDT Transmit Control Register                      (Default: 0x70)    */
#define REG_MDT_XMIT_CONTROL                        (0x0588)
#define BIT_MDT_XMIT_CONTROL_MDT_XMIT_EN            (0x80)
#define BIT_MDT_XMIT_CONTROL_MDT_XMIT_CMD_MERGE_EN  (0x40)
#define BIT_MDT_XMIT_CONTROL_MDT_XMIT_FIXED_BURST_LEN (0x20)
#define BIT_MDT_XMIT_CONTROL_MDT_XMIT_FIXED_AID     (0x10)
#define BIT_MDT_XMIT_CONTROL_MDT_XMIT_SINGLE_RUN_EN (0x08)
#define BIT_MDT_XMIT_CONTROL_MDT_CLR_ABORT_WAIT     (0x04)
#define BIT_MDT_XMIT_CONTROL_MDT_XFIFO_CLR_ALL      (0x02)
#define BIT_MDT_XMIT_CONTROL_MDT_XFIFO_CLR_CUR      (0x01)

/* 0x89 MDT Receive WRITE Port                             (Default: 0x00)    */
#define REG_MDT_XMIT_WRITE_PORT                     (0x0589)
#define MSK_MDT_XMIT_WRITE_PORT_MDT_XFIFO_WDATA     (0xFF)

/* 0x8A MDT RFIFO Status Register                          (Default: 0x00)    */
#define REG_MDT_RFIFO_STAT                          (0x058A)
#define MSK_MDT_RFIFO_STAT_MDT_RFIFO_CNT            (0xE0)
#define MSK_MDT_RFIFO_STAT_MDT_RFIFO_CUR_BYTE_CNT   (0x1F)

/* 0x8B MDT XFIFO Status Register                          (Default: 0x80)    */
#define REG_MDT_XFIFO_STAT                          (0x058B)
#define MSK_MDT_XFIFO_STAT_MDT_XFIFO_LEVEL_AVAIL    (0xE0)
#define BIT_MDT_XFIFO_STAT_MDT_XMIT_PRE_HS_EN       (0x10)
#define MSK_MDT_XFIFO_STAT_MDT_WRITE_BURST_LEN      (0x0F)

/* 0x8C MDT Interrupt 0 Register                           (Default: 0x0C)    */
#define REG_MDT_INT_0                               (0x058C)
#define BIT_MDT_RFIFO_DATA_RDY		BIT(0)
#define BIT_MDT_IDLE_AFTER_HAWB_DISABLE	BIT(2)
#define BIT_MDT_XFIFO_EMPTY		BIT(3)

/* 0x8D MDT Interrupt 0 Mask Register                      (Default: 0x00)    */
#define REG_MDT_INT_0_MASK                          (0x058D)

/* 0x8E MDT Interrupt 1 Register                           (Default: 0x00)    */
#define REG_MDT_INT_1                               (0x058E)
#define BIT_MDT_RCV_TIMEOUT		BIT(0)
#define BIT_MDT_RCV_SM_ABORT_PKT_RCVD	BIT(1)
#define BIT_MDT_RCV_SM_ERROR		BIT(2)
#define BIT_MDT_XMIT_TIMEOUT		BIT(5)
#define BIT_MDT_XMIT_SM_ABORT_PKT_RCVD	BIT(6)
#define BIT_MDT_XMIT_SM_ERROR		BIT(7)

/* 0x8F MDT Interrupt 1 Mask Register                      (Default: 0x00)    */
#define REG_MDT_INT_1_MASK                          (0x058F)

/* 0x90 CBUS Vendor ID Register                            (Default: 0x01)    */
#define REG_CBUS_VENDOR_ID                          (0x0590)
#define MSK_CBUS_VENDOR_ID_CBUS_VENDOR_ID           (0xFF)

/* 0x91 CBUS Connection Status Register                    (Default: 0x00)    */
#define REG_CBUS_STATUS                             (0x0591)
#define BIT_CBUS_STATUS_MHL_CABLE_PRESENT           (0x10)
#define BIT_CBUS_STATUS_MSC_HB_SUCCESS              (0x08)
#define BIT_CBUS_STATUS_CBUS_HPD                    (0x04)
#define BIT_CBUS_STATUS_MHL_MODE                    (0x02)
#define BIT_CBUS_STATUS_CBUS_CONNECTED              (0x01)

/* 0x92 CBUS Interrupt 1st Register                        (Default: 0x00)    */
#define REG_CBUS_INT_0                              (0x0592)
#define BIT_CBUS_MSC_MT_DONE_NACK	BIT(7)
#define BIT_CBUS_MSC_MR_SET_INT		BIT(6)
#define BIT_CBUS_MSC_MR_WRITE_BURST	BIT(5)
#define BIT_CBUS_MSC_MR_MSC_MSG		BIT(4)
#define BIT_CBUS_MSC_MR_WRITE_STAT	BIT(3)
#define BIT_CBUS_HPD_CHG		BIT(2)
#define BIT_CBUS_MSC_MT_DONE		BIT(1)
#define BIT_CBUS_CNX_CHG		BIT(0)

/* 0x93 CBUS Interrupt Mask 1st Register                   (Default: 0x00)    */
#define REG_CBUS_INT_0_MASK                         (0x0593)

/* 0x94 CBUS Interrupt 2nd Register                        (Default: 0x00)    */
#define REG_CBUS_INT_1                              (0x0594)
#define BIT_CBUS_CMD_ABORT		BIT(6)
#define BIT_CBUS_MSC_ABORT_RCVD		BIT(3)
#define BIT_CBUS_DDC_ABORT		BIT(2)
#define BIT_CBUS_CEC_ABORT		BIT(1)

/* 0x95 CBUS Interrupt Mask 2nd Register                   (Default: 0x00)    */
#define REG_CBUS_INT_1_MASK                         (0x0595)

/* 0x98 CBUS DDC Abort Interrupt Register                  (Default: 0x00)    */
#define REG_DDC_ABORT_INT                           (0x0598)

/* 0x99 CBUS DDC Abort Interrupt Mask Register             (Default: 0x00)    */
#define REG_DDC_ABORT_INT_MASK                      (0x0599)

/* 0x9A CBUS MSC Requester Abort Interrupt Register        (Default: 0x00)    */
#define REG_MSC_MT_ABORT_INT                        (0x059A)

/* 0x9B CBUS MSC Requester Abort Interrupt Mask Register   (Default: 0x00)    */
#define REG_MSC_MT_ABORT_INT_MASK                   (0x059B)

/* 0x9C CBUS MSC Responder Abort Interrupt Register        (Default: 0x00)    */
#define REG_MSC_MR_ABORT_INT                        (0x059C)

/* 0x9D CBUS MSC Responder Abort Interrupt Mask Register   (Default: 0x00)    */
#define REG_MSC_MR_ABORT_INT_MASK                   (0x059D)

/* 0x9E CBUS RX DISCOVERY interrupt Register               (Default: 0x00)    */
#define REG_CBUS_RX_DISC_INT0                       (0x059E)

/* 0x9F CBUS RX DISCOVERY Interrupt Mask Register          (Default: 0x00)    */
#define REG_CBUS_RX_DISC_INT0_MASK                  (0x059F)

/* 0xA7 CBUS_Link_Layer Control #8 Register                (Default: 0x00)    */
#define REG_CBUS_LINK_CONTROL_8                     (0x05A7)
#define MSK_CBUS_LINK_CONTROL_8_LNK_XMIT_BIT_TIME   (0xFF)

/* 0xB5 MDT State Machine Status Register                  (Default: 0x00)    */
#define REG_MDT_SM_STAT                             (0x05B5)
#define MSK_MDT_SM_STAT_MDT_RCV_STATE               (0xF0)
#define MSK_MDT_SM_STAT_MDT_XMIT_STATE              (0x0F)

/* 0xB8 CBUS MSC command trigger Register                  (Default: 0x00)    */
#define REG_MSC_COMMAND_START                       (0x05B8)
#define BIT_MSC_COMMAND_START_DEBUG                 (0x20)
#define BIT_MSC_COMMAND_START_WRITE_BURST           (0x10)
#define BIT_MSC_COMMAND_START_WRITE_STAT            (0x08)
#define BIT_MSC_COMMAND_START_READ_DEVCAP           (0x04)
#define BIT_MSC_COMMAND_START_MSC_MSG               (0x02)
#define BIT_MSC_COMMAND_START_PEER                  (0x01)

/* 0xB9 CBUS MSC Command/Offset Register                   (Default: 0x00)    */
#define REG_MSC_CMD_OR_OFFSET                       (0x05B9)

/* CBUS MSC Transmit Data Registers                (Default: 0x00)    */
#define REG_MSC_1ST_TRANSMIT_DATA                   (0x05BA)
#define REG_MSC_2ND_TRANSMIT_DATA                   (0x05BB)

/* CBUS MSC Requester Received Data Registers      (Default: 0x00)    */
#define REG_MSC_MT_RCVD_DATA0                       (0x05BC)
#define REG_MSC_MT_RCVD_DATA1                       (0x05BD)

/* CBUS MSC Responder MSC_MSG Received Data Regs   (Default: 0x00)    */
#define REG_MSC_MR_MSC_MSG_RCVD_1ST_DATA            (0x05BF)
#define REG_MSC_MR_MSC_MSG_RCVD_2ND_DATA            (0x05C0)

/* 0xC4 CBUS MSC Heartbeat Control Register                (Default: 0x27)    */
#define REG_MSC_HEARTBEAT_CONTROL                   (0x05C4)
#define BIT_MSC_HEARTBEAT_CONTROL_MSC_HB_EN         (0x80)
#define MSK_MSC_HEARTBEAT_CONTROL_MSC_HB_FAIL_LIMIT (0x70)
#define MSK_MSC_HEARTBEAT_CONTROL_MSC_HB_PERIOD_MSB (0x0F)

/* 0xC7 CBUS MSC Compatibility Control Register            (Default: 0x02)    */
#define REG_CBUS_MSC_COMPATIBILITY_CONTROL          (0x05C7)
#define BIT_CBUS_MSC_COMPATIBILITY_CONTROL_XDEVCAP_EN (0x80)
#define BIT_CBUS_MSC_COMPATIBILITY_CONTROL_DISABLE_MSC_ON_CBUS (0x40)
#define BIT_CBUS_MSC_COMPATIBILITY_CONTROL_DISABLE_DDC_ON_CBUS (0x20)
#define BIT_CBUS_MSC_COMPATIBILITY_CONTROL_DISABLE_GET_DDC_ERRORCODE (0x08)
#define BIT_CBUS_MSC_COMPATIBILITY_CONTROL_DISABLE_GET_VS1_ERRORCODE (0x04)

/* 0xDC CBUS3 Converter Control Register                   (Default: 0x24)    */
#define REG_CBUS3_CNVT                              (0x05DC)
#define MSK_CBUS3_CNVT_CBUS3_RETRYLMT               (0xF0)
#define MSK_CBUS3_CNVT_CBUS3_PEERTOUT_SEL           (0x0C)
#define BIT_CBUS3_CNVT_TEARCBUS_EN                  (0x02)
#define BIT_CBUS3_CNVT_CBUS3CNVT_EN                 (0x01)

/* 0xE0 Discovery Control1 Register                        (Default: 0x24)    */
#define REG_DISC_CTRL1                              (0x05E0)
#define BIT_DISC_CTRL1_CBUS_INTR_EN                 (0x80)
#define BIT_DISC_CTRL1_HB_ONLY                      (0x40)
#define MSK_DISC_CTRL1_DISC_ATT                     (0x30)
#define MSK_DISC_CTRL1_DISC_CYC                     (0x0C)
#define BIT_DISC_CTRL1_DISC_EN                      (0x01)

#define VAL_PUP_OFF		0
#define VAL_PUP_20K		1
#define VAL_PUP_5K		2

/* 0xE3 Discovery Control4 Register                        (Default: 0x80)    */
#define REG_DISC_CTRL4                              (0x05E3)
#define MSK_DISC_CTRL4_CBUSDISC_PUP_SEL             (0xC0)
#define MSK_DISC_CTRL4_CBUSIDLE_PUP_SEL             (0x30)
#define VAL_DISC_CTRL4(disc, idle)		(((disc) << 6) | (idle << 4))

/* 0xE4 Discovery Control5 Register                        (Default: 0x03)    */
#define REG_DISC_CTRL5                              (0x05E4)
#define BIT_DISC_CTRL5_DSM_OVRIDE                   (0x08)
#define MSK_DISC_CTRL5_CBUSMHL_PUP_SEL              (0x03)

/* 0xE7 Discovery Control8 Register                        (Default: 0x81)    */
#define REG_DISC_CTRL8                              (0x05E7)
#define BIT_DISC_CTRL8_NOMHLINT_CLR_BYPASS          (0x80)
#define BIT_DISC_CTRL8_DELAY_CBUS_INTR_EN           (0x01)

/* 0xE8 Discovery Control9 Register                        (Default: 0x54)    */
#define REG_DISC_CTRL9                              (0x05E8)
#define BIT_DISC_CTRL9_MHL3_RSEN_BYP                (0x80)
#define BIT_DISC_CTRL9_MHL3DISC_EN                  (0x40)
#define BIT_DISC_CTRL9_WAKE_DRVFLT                  (0x10)
#define BIT_DISC_CTRL9_NOMHL_EST                    (0x08)
#define BIT_DISC_CTRL9_DISC_PULSE_PROCEED           (0x04)
#define BIT_DISC_CTRL9_WAKE_PULSE_BYPASS            (0x02)
#define BIT_DISC_CTRL9_VBUS_OUTPUT_CAPABILITY_SRC   (0x01)

/* 0xEB Discovery Status1 Register                         (Default: 0x00)    */
#define REG_DISC_STAT1                              (0x05EB)
#define BIT_DISC_STAT1_PSM_OVRIDE                   (0x20)
#define MSK_DISC_STAT1_DISC_SM                      (0x0F)

/* 0xEC Discovery Status2 Register                         (Default: 0x00)    */
#define REG_DISC_STAT2                              (0x05EC)
#define BIT_DISC_STAT2_CBUS_OE_POL                  (0x40)
#define BIT_DISC_STAT2_CBUS_SATUS                   (0x20)
#define BIT_DISC_STAT2_RSEN                         (0x10)

#define MSK_DISC_STAT2_MHL_VRSN                     (0x0C)
#define VAL_DISC_STAT2_DEFAULT                      (0x00)
#define VAL_DISC_STAT2_MHL1_2                       (0x04)
#define VAL_DISC_STAT2_MHL3                         (0x08)
#define VAL_DISC_STAT2_RESERVED                     (0x0C)

#define MSK_DISC_STAT2_RGND                         (0x03)
#define VAL_RGND_OPEN                               (0x00)
#define VAL_RGND_2K                                 (0x01)
#define VAL_RGND_1K                                 (0x02)
#define VAL_RGND_SHORT                              (0x03)

/* 0xED Interrupt CBUS_reg1 INTR0 Register                 (Default: 0x00)    */
#define REG_CBUS_DISC_INTR0                         (0x05ED)
#define BIT_RGND_READY_INT			BIT(6)
#define BIT_CBUS_MHL12_DISCON_INT		BIT(5)
#define BIT_CBUS_MHL3_DISCON_INT		BIT(4)
#define BIT_NOT_MHL_EST_INT			BIT(3)
#define BIT_MHL_EST_INT				BIT(2)
#define BIT_MHL3_EST_INT			BIT(1)
#define VAL_CBUS_MHL_DISCON			(BIT_CBUS_MHL12_DISCON_INT | \
		BIT_CBUS_MHL3_DISCON_INT)

/* 0xEE Interrupt CBUS_reg1 INTR0 Mask Register            (Default: 0x00)    */
#define REG_CBUS_DISC_INTR0_MASK                    (0x05EE)

#endif
