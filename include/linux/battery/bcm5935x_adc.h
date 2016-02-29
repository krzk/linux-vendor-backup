/*
********************************************************************
* THIS INFORMATION IS PROPRIETARY TO
* BROADCOM CORP.
*-------------------------------------------------------------------
*
*           Copyright (c) 2015 Broadcom Corp.
*                      ALL RIGHTS RESERVED
*
********************************************************************

********************************************************************
*    File Name: bcm59350_adc.h
*
*    Abstract: Functions for BCM59350
*
*    $History:$
*
********************************************************************
*/

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef signed int int32;

// WPC ADC initial data

/* init read adc */
void bcm59350_read_adc_init(uint8 reg_0xb4, uint8 reg_0xb5, uint8 reg_0xb6, uint8 reg_0xb7, uint8 reg_0x141,
                                  uint8 reg_0x143, uint8 reg_0x144, uint8 reg_0x145, uint8 reg_0x146, uint8 reg_0x147);

/* init power calculation */
void bcm59350_power_packet_init(uint8 reg_0x11a, uint8 reg_0x11b, uint8 *max_power_samples, uint16 *wait_time_for_sample);

/* Read VRECT */
/* output : VRECT mv */
uint32 get_bcm59350_vrect (uint8 reg_0x8b, uint8 reg_0x8d, uint8 reg_0x0a, uint8 reg_0x11);

/* Read VBUCK */
/* output : VBUCK mv */
uint32 get_bcm59350_vbuck (uint8 reg_0x89, uint8 reg_0x8d, uint8 reg_0x0c, uint8 reg_0x12);

/* Read DIE Temperature */
/* output : DIE Temperature - Celsius */
uint32 get_bcm59350_tdie (uint8 reg_0x0f, uint8 reg_0x13);

/* Read IBUCK */
uint32 get_bcm59350_ibuck (uint32 vrect, uint32 tdie, uint8 reg_0x0d, uint8 reg_0x12);

/* Read IRECT */
uint32 get_bcm59350_irect (uint32 vrect, uint32 tdie, uint32 vbuck, uint32 ibuck);

/* Calculate Received power */
uint32 get_bcm59350_receivedPower (uint32 vrect, uint32 irect);

/* get power value to write BRCM_A4WP_REG_PMU_WPT_WPC_RP_VALUE (0x112) register */
uint32 get_bcm59350_PowerRect_value (uint32 recieved_power, uint8 power_sample_count);

