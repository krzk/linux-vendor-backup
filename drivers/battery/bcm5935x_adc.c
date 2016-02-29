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
*    File Name: bcm59350_adc.c
*
*    Abstract: Functions for BCM59350
*
*    $History:$
*
********************************************************************
*/

#include <linux/battery/bcm5935x_adc.h>

#define VBUCK_TRIM_LSB_BITS 2
#define IBUCK_TRIM_LSB_BITS 2
#define VRECT_TRIM_LSB_BITS 2
#define IRECT_TRIM_LSB_BITS 2

#define OFFSET_TRIM_IRECT_LSB_1_0       0xC0
#define OFFSET_TRIM_IRECT_LSB_1_0_SHIFT 6
#define OFFSET_TRIM_VRECT_LSB_1_0       0x30
#define OFFSET_TRIM_VRECT_LSB_1_0_SHIFT 4
#define OFFSET_TRIM_IBUCK_LSB_1_0       0x0C
#define OFFSET_TRIM_IBUCK_LSB_1_0_SHIFT 2
#define OFFSET_TRIM_VBUCK_LSB_1_0       0x03
#define OFFSET_TRIM_VBUCK_LSB_1_0_SHIFT 0

#define CONVERT_SHIFT_RIGHT	8
#define M_VRECT			6000
#define C_VRECT 		0
//#define M_VBUCK 		1650	//For 59350B0
#define M_VBUCK 		1650*2	//For 59350C0
#define C_VBUCK			0

#define WPC_GET_MAXPWR_FROM_REG_VALUE(value)        (value & 0x3F) // bit[5:0] - 6 buts
#define WPC_GET_PWRCLS_FROM_REG_VALUE(value)        ((value >> 6) & 0x3) // bit[7:6] - 2 bits

uint32 p_max; //Max power
#define A4WP_WPC_GET_WINDOW_FROM_REG_VALUE(value)   ((value >> 2) & 0x3F)   // bit[7:2] - 6 bits
#define A4WP_WPC_GET_WINDOW_OFFSET_FROM_REG_VALUE(value) (value & 0x03)     // bit[1:0] - 2 bits

#define A4WP_WPC_TIME_BETWEEN_CES_AND_RPP       160     // ms
#define A4WP_WPC_SAMPLE_TIME_INTERVAL           4       // ms
#define A4WP_WPC_MAX_POWER_SAMPLES              16

// m0 and m1 are loaded from OTP.
int32 g_m0 = 0;     // 0.000        // multiple of 1000
int32 g_m1 = 0;     // 0.000        // multiple of 10000
int32 g_c1 = 0;     // 0.000        // multiple of 1000
int32 g_c2 = 0;     // 0.000        // multiple of 10000
int32 g_s0 = 1000;  // 1.000        // multiple of 1000

// coefficients for Irect calculation
int32 g_c0 = 1000;     // 1.00E + 00  multiple of 1000
int32 g_v0 = 970;      // 0.97E + 00  multiple of 1000
int32 g_v1 = 0;        // 0.000       multiple of 1000
int32 g_p0 = 900;      // 0.90E + 00  multiple of 1000
int32 g_p1 = 0;        // 0.00,       multiple of 1000
int32 g_i_rect0 = 64;  // 6.40E-02    multiple of 1000
int32 g_p2 = 0;        // 0.00E+00    multiple of 10000
int32 g_p3 = 0;        // 0.00E+00    multiple of 100000
int32 g_mt = 0;        // 0.0E+00     multiple of 100000
int32 g_v2 = 0;        // 0.00E+00    multiple of 10000
int32 g_v3 = 0;        // 0.00E+00    multiple of 100000

// Look up table for IBUCK_m0 and IBUCK_m1 values based on the index value from OTP.
// index 0x0 -> -0.128, 0x1 -> -0.127 ... 0x80 -> 0 ... 0xFE -> 0.126, 0xFF -> 0.127
int32 g_i_buck_m1_base = -1280;   // Multiple of 10000

// index 0x0 -> -1.28, 0x1 -> -1.27 ... 0x80 -> 0 .... 0xFE -> 1.26, 0xFF -> 1.27
int32 g_i_buck_m0_base = -1280;  // Multiple of 1000

// index 0x0 -> -0.008, 0x1 -> -0.007 ... 8 -> 0 ... 0xE -> 0.006, 0xF -> 0.007
int32 g_i_buck_c1_base = -8;     // Multiple of 1000

// index 0x0 -> -0.0008, 0x1 -> -0.0007 ... 8 -> 0 ... 0xE -> 0.0006, 0xF -> 0.0007
int32 g_i_buck_c2_base = -8;     // Multiple of 10000

// index 0x0 -> 0.92, 0x1 -> 0.93 ... 0xE -> 1.06, 0xF -> 1.07
int32 g_i_buck_s0_base = 920;  // Multiple of 1000

int32 g_i_rect0_base = 20;     // 2.00E-02  (in mA) multiples of 1000

int32 g_c0_base = 850;          //8.50E-01 in multiples of 1000

int32 g_v0_base = 850;          //8.50E-01 in multiples of 1000

int32 g_v1_base = -15;          //-1.50E-02 in multiples of 1000

int32 g_p0_base = 850;          //8.50E-01 in multiples of 1000

int32 g_p1_base = -15;          //-1.50E-02 in multiples of 1000

int32 g_p2_base = -16;          //-1.60E-03 in multiples of 10000

int32 g_p3_base = -16;          //-1.60E-04 in multiples of 100000

int32 g_mt_base = -75;          //-7.5E-04  in multiples of 100000

int32 g_v2_base = -16;          //-1.60E-03 in multiples of 10000

int32 g_v3_base = -16;          //-1.60E-04 in multiples of 100000

/* init read adc */
void bcm59350_read_adc_init(uint8 reg_0xb4, uint8 reg_0xb5, uint8 reg_0xb6, uint8 reg_0xb7, uint8 reg_0x141,
                                  uint8 reg_0x143, uint8 reg_0x144, uint8 reg_0x145, uint8 reg_0x146, uint8 reg_0x147) {
    uint8 index;
	uint32 value;

    // Load the Iout co-efficients from PMU OTP
    // get the m0
    value = reg_0xb6;
    g_m0 = g_i_buck_m0_base + (value * 10);

    // get the m1
    value = reg_0xb7;
    g_m1 = g_i_buck_m1_base + (value * 10);

    // get the c1
    value = reg_0xb5;
    index = value & 0x0F;
    g_c1 = g_i_buck_c1_base + index;

    // get the C2
    index = (value >> 4) & 0x0F;
    g_c2 = g_i_buck_c2_base + index;

    // get the s0
    value = reg_0xb4;
    index = value & 0x0F;
    g_s0 = g_i_buck_s0_base + (index * 10);

    // Irect0 value
    value = reg_0x141;
    index = value & 0x0F;
    g_i_rect0 = g_i_rect0_base + (index * 4);

    // C0
    value = reg_0x143;
    index = value & 0x0F;
    g_c0 = g_c0_base + (index * 10);

    // MT
    index = (value >> 4) & 0x0F;
    g_mt = g_mt_base + (index * 5);

    // V0
    value = reg_0x144;
    index = value & 0x0F;
    g_v0 = g_v0_base + (index * 10);

    // V1
    index = (value >> 4) & 0x0F;
    g_v1 = g_v1_base + (index * 1);

    //V2
    value = reg_0x145;
	index = value & 0x0F;
    g_v2 = g_v2_base + (index * 2);

    //V3
    index = (value >> 4) & 0x0F;
    g_v3 = g_v3_base + (index * 2);

    //P0
    value = reg_0x146;
	index = value & 0x0F;
    g_p0 = g_p0_base + (index * 10);

    // P1
    index = (value >> 4) & 0x0F;
    g_p1 = g_p1_base + (index * 1);

    // P2
    value = reg_0x147;
	index = value & 0x0F;
    g_p2 = g_p2_base + (index * 2);

    // P3
    index = (value >> 4) & 0x0F;
    g_p3 = g_p3_base + (index * 2);

}

/* init power calculation */
void bcm59350_power_packet_init(uint8 reg_0x11a, uint8 reg_0x11b, uint8 *max_power_samples, uint16 *wait_time_for_sample)
{
    uint8 val;
    uint32 pow_of_10[4] = {1, 10, 100, 1000};

    // read Config-Packet register (to obtain Max-Power and Power-Class
    // constants)
    val = reg_0x11a;

    // Pmax = (Maximum Power / 2) * 10^power class W
    p_max =((WPC_GET_MAXPWR_FROM_REG_VALUE(val) * 1000) >> 1) *
            pow_of_10[WPC_GET_PWRCLS_FROM_REG_VALUE(val)]; // in mW

    // Calculate the maximum number of samples from window size
    *max_power_samples =
            A4WP_WPC_GET_WINDOW_FROM_REG_VALUE(reg_0x11b);

    // Limit the max samples if greater than max supported.
    if (*max_power_samples > A4WP_WPC_MAX_POWER_SAMPLES)
    {
        *max_power_samples = A4WP_WPC_MAX_POWER_SAMPLES;
    }

    // Calculate the wait time after the WPC control error sent packet
    // received till the start of the first power sample reading.
    *wait_time_for_sample =
           A4WP_WPC_TIME_BETWEEN_CES_AND_RPP -
           (A4WP_WPC_GET_WINDOW_OFFSET_FROM_REG_VALUE(reg_0x11b) * 4) -
           ((*max_power_samples - 1) * A4WP_WPC_SAMPLE_TIME_INTERVAL);
}

/* Read Vrect trim */
uint32 get_bcm59350_vrect_trim (uint8 reg_0x8b, uint8 reg_0x8d) {
   uint32 trim, trimLsb, vRectTrim;
   
   // Vrect trim
   trimLsb = reg_0x8d;
   trim = reg_0x8b;
   trim &= 0xFF;
   trim <<= VRECT_TRIM_LSB_BITS;
   vRectTrim = trim | ((trimLsb & OFFSET_TRIM_VRECT_LSB_1_0)>>OFFSET_TRIM_VRECT_LSB_1_0_SHIFT);
   return vRectTrim;
}

/* Read Vbuck trim */
uint32 get_bcm59350_vbuck_trim (uint8 reg_0x89, uint8 reg_0x8d) {
   uint32 trim, trimLsb, vBuckTrim;
   
   // Vbuck trim
   trimLsb = reg_0x8d;
   trim = reg_0x89;
   trim &= 0xFF;
   trim <<= VBUCK_TRIM_LSB_BITS;
   vBuckTrim = trim | ((trimLsb & OFFSET_TRIM_VBUCK_LSB_1_0)>>OFFSET_TRIM_VBUCK_LSB_1_0_SHIFT);
   return vBuckTrim;
}

/* Read VRECT */
/* output : VRECT mv */
uint32 get_bcm59350_vrect (uint8 reg_0x8b, uint8 reg_0x8d, uint8 reg_0x0a, uint8 reg_0x11) {
   uint32 r;
   r = reg_0x0a;
   r <<=4;
   r |= reg_0x11 & 0xf;
   //Use 10bit
   r >>=2;
   //vRectTrim
   r -= get_bcm59350_vrect_trim(reg_0x8b, reg_0x8d);

   r = (r*M_VRECT + C_VRECT)>>CONVERT_SHIFT_RIGHT;
   return r;
}

/* Read VBUCK */
/* output : VBUCK mv */
uint32 get_bcm59350_vbuck (uint8 reg_0x89, uint8 reg_0x8d, uint8 reg_0x0c, uint8 reg_0x12) {
   uint32 r;
   r = reg_0x0c;
   r <<=4;
   r |= reg_0x12 & 0xf;
   //Use 10bit
   r >>=2;
   //vBuckTrim
   r -= get_bcm59350_vbuck_trim(reg_0x89, reg_0x8d);

   r = (r*M_VBUCK + C_VBUCK)>>CONVERT_SHIFT_RIGHT;
   return r;
}

/* Read DIE Temperature */
/* output : DIE Temperature - Celsius */
uint32 get_bcm59350_tdie (uint8 reg_0x0f, uint8 reg_0x13) {
   uint32 r;
   r = reg_0x0f;
   r <<=4;
   r |= (reg_0x13 >> 4) & 0xf;
   //Use 10bit
   r >>=2;
   r = (r>>1) - 275; //Celsius = 0.4989*(10-bit ADC result) - 274.96 
   return r;
}

/* Read IBUCK */
uint32 get_bcm59350_ibuck (uint32 vrect, uint32 tdie, uint8 reg_0x0d, uint8 reg_0x12) {

   uint32 r;
   uint32 i_buck_adc = 0;
   uint32 tdie_adc;
   uint32 ibuck, t1, t2, nr, dr;
   r = reg_0x0d;
   r <<=4;
   r |= (reg_0x12 >> 4) & 0xf;
   //Use 10bit
   r >>=2;

   // Calculate Ibuck ADC
   // Ibuck_adc = (adc_value/1024) * 3.3
   i_buck_adc = (r * 1000 * 1000) / 1024;
   i_buck_adc = i_buck_adc * 3300;

   i_buck_adc = i_buck_adc/(1000 * 1000);

   //Ibuck = (Ibuck,adc - m0 - m1 * (1 - c1 * (Tdie,adc - 27)) * Vrect,adc)/ (s0 * (1 + c2 * (Tdie,adc - 27))
   tdie_adc = tdie - 27;

   t1 = (1000 - (g_c1 * tdie_adc));
   t2 = (g_m1 * t1) / 10000 ;               // in multiple of 1000
   t2  = (t2 * vrect)/1000;
   nr = ((i_buck_adc ) - (g_m0) - (t2));
   nr = nr * 1000;
   t2 = 10000 + (g_c2 * tdie_adc);
   dr = (g_s0 * t2) / 10000;

   t2 = (nr / dr);            // in mA

   // report zero if calaculated value is negative.
   ibuck = (t2 > 0) ? t2 : 0;

   return ibuck;
}

/* Read IRECT */
uint32 get_bcm59350_irect (uint32 vrect, uint32 tdie, uint32 vbuck, uint32 ibuck) {
   uint32 r, tdie_adc;
   uint32 t1, t2, t3, p_buck, v_rect_comp, p_buck_comp, mt_comp, k;

   tdie_adc = tdie - 27;

   // Irect = Irect0 + VBUCK * IBUCK / [VRECT * K(VRECT, PBUCK, TEMP)]
   // K = C0*(V0 + V1*VRECT + V2*VRECT^2 + V3*VRECT^3)
   //         *(P0 + P1*PBUCK + P2*PBUCK^2 + P3*PBUCK^3)
   //         *(1 + MT*(T ?27))
   // Pick fixed values for above coefficients (already in OTP)
   // C0 = 1, V0 = 1, V1 = -0.003, V2 = 0, V3 = 0, P0 = 1, P1 = -0.01, P2 = 0, P3 = 0, MT = 0, Irect0 = 28 mA
   p_buck = (ibuck * vbuck) / 1000;   // Calculate PBUCK = Vout * Iout (in mW)
 
   t1 = (g_v1 * vrect);                 // in multiple of 10^6
 
   t2 = (vrect * vrect) / 1000;
   t3 = (t2 * vrect) / 1000;
   t2 = (t2 * g_v2) / 10;              // in multiple of 10^6
 
   t3 = (t3 * g_v3) / 100;             // in multiple of 10^6
 
   v_rect_comp = ((g_v0 * 1000) + t1 + t2 + t3);        // in multiple of 10^6
 
   t1 = (g_p1 * p_buck);                   // in multiple of 10^6
 
   t2 = (p_buck * p_buck) / 1000;
   t3 = (t2 * p_buck) / 1000;
   t2 = (p_buck * g_p2) / 10;              // in multiple of 10^6
 
   t3 = (t3 * g_p3) / 100;                 // in multiple of 10^6
 
   p_buck_comp = ((g_p0 * 1000) + t1 + t2 + t3);        // in multiple of 10^6
 
   mt_comp = 100000 + (g_mt * tdie_adc);         // in multiple of 10^5
 
   // calculate K
   k = ((v_rect_comp / 100) * (p_buck_comp / 100)) / 1000;              // in multiple of 10^5
   k = (g_c0 * k) / 10000;                     // in multiple of 10^4
   k = (k * (mt_comp / 10)) / 1000;           // in multiple of 10^5
 
   t1 = (vrect * k) / 100000;                 // in multiple of 10^3
 
   // Calculate Irect
   r = (p_buck * 1000) / t1;  // in mA 
   r = r + g_i_rect0;
   return r;
}

/* Calculate Received power */
uint32 get_bcm59350_receivedPower (uint32 vrect, uint32 irect)
{
	return (vrect * irect);
}

/* get power value to write BRCM_A4WP_REG_PMU_WPT_WPC_RP_VALUE (0x112) register */
uint32 get_bcm59350_PowerRect_value (uint32 recieved_power, uint8 power_sample_count)
{
    uint32 temp = recieved_power;
    uint8  p_recv_val;

    if (power_sample_count)
    {
        // Calculate the average power till now.
        temp = temp / power_sample_count;

        // Received Power Value = (Preceived / Pmax) * 128
        temp = (temp << 7) / p_max;
        temp += (temp & 0x200) ? 488 : 0; // Ensuring that rounding off is fair
        p_recv_val = (temp / 1000);

        // Return the value to write the received power value into received-power registers
        return p_recv_val;
    }

	return 0;
}

