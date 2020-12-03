/*******************************************************************************
*
* Copyright (C) 2010 Maxim Integrated Products, Inc. All Rights Reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,  
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL MAXIM
* INTEGRATED PRODUCTS INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
* IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*******************************************************************************/


/**-----------------------------------------------------------------------------
 *
 * @file MD_AL25.h
 * 
 * $Header: /CvsRepositories/CApps3/device/ext/MD_AL25.h,v 1.1.2.1 2010/04/22 05:25:31 wdawkins Exp $
 *
 * @brief 
 * Provides public interface to Maxim Device AL25 (MD_AL25)
 * 
 * @author 
 * Scooter Dawkins
 * 
 * @date 
 * April 19, 2010
 * 
 * @namespace 
 * MD_AL25
 * 
 * @reference todo
 *
 * @standard_constraints 
 * Before any MD_AL25 module function can be called, 
 * the MD_AL25 module must be initialized.
 *
 * @code
 * MD_AL25_ModuleInit();  // Module initialization
 * @endcode
 *
 * The following methods are exported for MD_AL25 clients:
 *
 * @code
 * @todo function exports
 * @endcode
 *
 *------------------------------------------------------------------------------
 */


#ifndef MD_AL25_HDR
#define MD_AL25_HDR


#ifdef __cplusplus
extern "C"{
#endif


/*==============================================================================
 *
 *                           I N C L U D E   F I L E S
 *
 *==============================================================================
 */
#define FEAT_EN_SAMSUNG_API 	1
#if FEAT_EN_SAMSUNG_API
	#define	MCS_U8_T	unsigned char
	#define	MCS_U16_T	unsigned short
	#define	MCS_U32_T	unsigned int
	#define MCS_BOOL_T	unsigned char
	#define MD_TMR_ID_T	unsigned int
	#define MCS_TRUE	1
	#define MCS_FALSE	0
#endif


/*==============================================================================
 *
 *                      E X T E R N A L   C O N S T A N T S
 *
 *==============================================================================
 */

#define MAX14577_DBG_ENABLE 

#ifdef MAX14577_DBG_ENABLE
#define DEBUG_MAX14577(fmt, args...)  printk(fmt, ##args)
#else
#define DEBUG_MAX14577(fmt, args...) do {} while(0)
#endif

#define MAX14577_ESS_LOG

#ifdef MAX14577_ESS_LOG
#define MAX14577_ELOG(fmt, args...)  printk(fmt, ##args)
#else
#define MAX14577_ELOG(fmt, args...) do {} while(0)
#endif





/*==============================================================================
 *
 *                         E X T E R N A L   M A C R O S
 *
 *==============================================================================
 */


/*==============================================================================
 *
 *                          E X T E R N A L   T Y P E S
 *
 *==============================================================================
 */
/*==============================================================================
 *
 *                       from MD_AL25.c
 *
 *===============================================================================
 *//*==============================================================================
 *
 *                          L O C A L   C O N S T A N T S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * Feature enabled statuses.
 *
 * @note
 * "1" enables a feature; 
 * "0" disables a feature.
 *
 *------------------------------------------------------------------------------
 */
#if FEAT_EN_SAMSUNG_API

  #define   FEAT_EN_x                            0   // Example
  #define   FEAT_EN_TRC_MAIN_STATE_MACHINE       0   // Trace Main State Machine Msgs
  #define   FEAT_EN_TRC_ACCESSORY                0   // Trace accessory detect/setup
  #define   FEAT_EN_TRC_CHARGE                   0   // Trace charging functions
  #define   FEAT_EN_TRC_I2C_TRAFFIC              0   // Trace I2C read/write data
  #define   FEAT_EN_TRC_DBG_REG                  0   // Trace Register Dumps


# else
  #define   FEAT_EN_x                            0   // Example
  #define   FEAT_EN_TRC_MAIN_STATE_MACHINE       1   // Trace Main State Machine Msgs
  #define   FEAT_EN_TRC_ACCESSORY                1   // Trace accessory detect/setup
  #define   FEAT_EN_TRC_CHARGE                   1   // Trace charging functions
  #define   FEAT_EN_TRC_I2C_TRAFFIC              1   // Trace I2C read/write data
  #define   FEAT_EN_TRC_DBG_REG                  1   // Trace Register Dumps
#endif


/**-----------------------------------------------------------------------------
 *
 * IIC Address (todo - maybe should be userCfg param)
 *
 *------------------------------------------------------------------------------
 */
#define   MD_AL25_IIC_ADDRESS   0x4A


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x00 - Device ID
 * BitField, Mask and Bit Setting definitions
 * READ ONLY
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_DEVICEID_VENDOR_ID   0
#define MD_AL25_B_DEVICEID_CHIP_REV    3

#define MD_AL25_M_DEVICEID_VENDOR_ID   0x07
#define MD_AL25_M_DEVICEID_CHIP_REV    0xF8


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x01 - Interrupt 1
 * BitField, Mask and Bit Setting definitions
 * READ ONLY (clears on Read)
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INT1_ADC      0
#define MD_AL25_B_INT1_ADCLOW   1
#define MD_AL25_B_INT1_ADCERR   2
                                      
#define MD_AL25_M_INT1_ADC      0x01
#define MD_AL25_M_INT1_ADCLOW   0x02
#define MD_AL25_M_INT1_ADCERR   0x04


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x02 - Interrupt 2
 * BitField, Mask and Bit Setting definitions
 * READ ONLY (clears on Read)
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INT2_CHGTYP      0
#define MD_AL25_B_INT2_CHGDETRUN   1
#define MD_AL25_B_INT2_DCDTMR      2
#define MD_AL25_B_INT2_DBCHG       3
#define MD_AL25_B_INT2_VBVOLT      4

#define MD_AL25_M_INT2_CHGTYP      0x01
#define MD_AL25_M_INT2_CHGDETRUN   0x02
#define MD_AL25_M_INT2_DCDTMR      0x04
#define MD_AL25_M_INT2_DBCHG       0x08
#define MD_AL25_M_INT2_VBVOLT      0x10


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x03 - Interrupt 3
 * BitField, Mask and Bit Setting definitions
 * READ ONLY (clears on Read)
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INT3_MBCCHGERR   3
#define MD_AL25_B_INT3_OVP         2
#define MD_AL25_B_INT3_CGMBC       1
#define MD_AL25_B_INT3_EOC         0

#define MD_AL25_M_INT3_MBCCHGERR   0x08
#define MD_AL25_M_INT3_OVP         0x04
#define MD_AL25_M_INT3_CGMBC       0x02
#define MD_AL25_M_INT3_EOC         0x01


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x04 - Interrupt Status 1
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INTSTAT1_ADC      0
#define MD_AL25_B_INTSTAT1_ADCLOW   5
#define MD_AL25_B_INTSTAT1_ADCERR   6

#define MD_AL25_M_INTSTAT1_ADC      0x1F
#define MD_AL25_M_INTSTAT1_ADCLOW   0x20
#define MD_AL25_M_INTSTAT1_ADCERR   0x40


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x05 - Interrupt Status 2
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INTSTAT2_CHGTYP      0
#define MD_AL25_B_INTSTAT2_CHGDETRUN   3
#define MD_AL25_B_INTSTAT2_DCDTMR      4
#define MD_AL25_B_INTSTAT2_DBCHG       5
#define MD_AL25_B_INTSTAT2_VBVOLT      6

#define MD_AL25_M_INTSTAT2_CHGTYP      0x07
#define MD_AL25_M_INTSTAT2_CHGDETRUN   0x08
#define MD_AL25_M_INTSTAT2_DCDTMR      0x10
#define MD_AL25_M_INTSTAT2_DBCHG       0x20
#define MD_AL25_M_INTSTAT2_VBVOLT      0x40


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x06 - Interrupt Status 3
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INTSTAT3_MBCCHGERR   3
#define MD_AL25_B_INTSTAT3_OVP         2
#define MD_AL25_B_INTSTAT3_CGMBC       1
#define MD_AL25_B_INTSTAT3_EOC         0

#define MD_AL25_M_INTSTAT3_MBCCHGERR   0x08
#define MD_AL25_M_INTSTAT3_OVP         0x04
#define MD_AL25_M_INTSTAT3_CGMBC       0x02
#define MD_AL25_M_INTSTAT3_EOC         0x01


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x07 - Interrupt Mask 1
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INTMASK1_ADC      0
#define MD_AL25_B_INTMASK1_ADCLOW   1
#define MD_AL25_B_INTMASK1_ADCERR   2

#define MD_AL25_M_INTMASK1_ADC      0x01
#define MD_AL25_M_INTMASK1_ADCLOW   0x02
#define MD_AL25_M_INTMASK1_ADCERR   0x04

#define MD_AL25_REG_INTMASK1_ADC_DISABLE      0
#define MD_AL25_REG_INTMASK1_ADC_ENABLE       ( 1 << MD_AL25_B_INTMASK1_ADC    )
#define MD_AL25_REG_INTMASK1_ADCLOW_DISABLE   0
#define MD_AL25_REG_INTMASK1_ADCLOW_ENABLE    ( 1 << MD_AL25_B_INTMASK1_ADCLOW )
#define MD_AL25_REG_INTMASK1_ADCERR_DISABLE   0
#define MD_AL25_REG_INTMASK1_ADCERR_ENABLE    ( 1 << MD_AL25_B_INTMASK1_ADCERR )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x08 - Interrupt Mask 2
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INTMASK2_CHGTYP      0
#define MD_AL25_B_INTMASK2_CHGDETRUN   1
#define MD_AL25_B_INTMASK2_DCDTMR      2
#define MD_AL25_B_INTMASK2_DBCHG       3
#define MD_AL25_B_INTMASK2_VBVOLT      4

#define MD_AL25_M_INTMASK2_CHGTYP      0x01
#define MD_AL25_M_INTMASK2_CHGDETRUN   0x02
#define MD_AL25_M_INTMASK2_DCDTMR      0x04
#define MD_AL25_M_INTMASK2_DBCHG       0x08
#define MD_AL25_M_INTMASK2_VBVOLT      0x10

#define MD_AL25_REG_INTMASK2_CHGTYP_DISABLE      0
#define MD_AL25_REG_INTMASK2_CHGTYP_ENABLE       ( 1 << MD_AL25_B_INTMASK2_CHGTYP    )
#define MD_AL25_REG_INTMASK2_CHGDETRUN_DISABLE   0
#define MD_AL25_REG_INTMASK2_CHGDETRUN_ENABLE    ( 1 << MD_AL25_B_INTMASK2_CHGDETRUN )
#define MD_AL25_REG_INTMASK2_DCDTMR_DISABLE      0
#define MD_AL25_REG_INTMASK2_DCDTMR_ENABLE       ( 1 << MD_AL25_B_INTMASK2_DCDTMR    )
#define MD_AL25_REG_INTMASK2_DBCHG_DISABLE       0
#define MD_AL25_REG_INTMASK2_DBCHG_ENABLE        ( 1 << MD_AL25_B_INTMASK2_DBCHG     )
#define MD_AL25_REG_INTMASK2_VBVOLT_DISABLE      0
#define MD_AL25_REG_INTMASK2_VBVOLT_ENABLE       ( 1 << MD_AL25_B_INTMASK2_VBVOLT    )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x09 - Interrupt Mask 3
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_INTMASK3_MBCCHGERR   3
#define MD_AL25_B_INTMASK3_OVP         2
#define MD_AL25_B_INTMASK3_CGMBC       1
#define MD_AL25_B_INTMASK3_EOC         0

#define MD_AL25_M_INTMASK3_MBCCHGERR   0x08
#define MD_AL25_M_INTMASK3_OVP         0x04
#define MD_AL25_M_INTMASK3_CGMBC       0x02
#define MD_AL25_M_INTMASK3_EOC         0x01

#define MD_AL25_REG_INTMASK3_MBCCHGERR_DISABLE   0
#define MD_AL25_REG_INTMASK3_MBCCHGERR_ENABLE    ( 1 << MD_AL25_B_INTMASK3_MBCCHGERR )
#define MD_AL25_REG_INTMASK3_OVP_DISABLE         0
#define MD_AL25_REG_INTMASK3_OVP_ENABLE          ( 1 << MD_AL25_B_INTMASK3_OVP )
#define MD_AL25_REG_INTMASK3_CGMBC_DISABLE       0
#define MD_AL25_REG_INTMASK3_CGMBC_ENABLE        ( 1 << MD_AL25_B_INTMASK3_CGMBC )
#define MD_AL25_REG_INTMASK3_EOC_DISABLE         0
#define MD_AL25_REG_INTMASK3_EOC_ENABLE          ( 1 << MD_AL25_B_INTMASK3_EOC )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x0A - Charger Detection Control
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CDETCTRL_CHGDETEN   0
#define MD_AL25_B_CDETCTRL_CHGTYPM    1
#define MD_AL25_B_CDETCTRL_DCDEN      2
#define MD_AL25_B_CDETCTRL_DCD2SCT    3
#define MD_AL25_B_CDETCTRL_DCHK       4
#define MD_AL25_B_CDETCTRL_DBEXIT     5
#define MD_AL25_B_CDETCTRL_DBIDLE     6
#define MD_AL25_B_CDETCTRL_CDPDET     7

#define MD_AL25_M_CDETCTRL_CHGDETEN   0x01
#define MD_AL25_M_CDETCTRL_CHGTYPM    0x02
#define MD_AL25_M_CDETCTRL_DCDEN      0x04
#define MD_AL25_M_CDETCTRL_DCD2SCT    0x08
#define MD_AL25_M_CDETCTRL_DCHK       0x10
#define MD_AL25_M_CDETCTRL_DBEXIT     0x20
#define MD_AL25_M_CDETCTRL_DBIDLE     0x40
#define MD_AL25_M_CDETCTRL_CDPDET     0x80

#define MD_AL25_REG_CDETCTRL_CHGDETEN_DISABLE   0
#define MD_AL25_REG_CDETCTRL_CHGDETEN_ENABLE    ( 1 << MD_AL25_B_CDETCTRL_CHGDETEN )
#define MD_AL25_REG_CDETCTRL_CHGTYPM_DISABLE    0
#define MD_AL25_REG_CDETCTRL_CHGTYPM_ENABLE     ( 1 << MD_AL25_B_CDETCTRL_CHGTYPM  )
#define MD_AL25_REG_CDETCTRL_DCDEN_DISABLE      0                                  
#define MD_AL25_REG_CDETCTRL_DCDEN_ENABLE       ( 1 << MD_AL25_B_CDETCTRL_DCDEN    )
#define MD_AL25_REG_CDETCTRL_DCD2SCT_NORMAL     0                                  
#define MD_AL25_REG_CDETCTRL_DCD2SCT_EXIT       ( 1 << MD_AL25_B_CDETCTRL_DCD2SCT  )
#define MD_AL25_REG_CDETCTRL_DCHK_50MS          0                                  
#define MD_AL25_REG_CDETCTRL_DCHK_620MS         ( 1 << MD_AL25_B_CDETCTRL_DCHK     )
#define MD_AL25_REG_CDETCTRL_DBEXIT_DISABLE     0                                  
#define MD_AL25_REG_CDETCTRL_DBEXIT_ENABLE      ( 1 << MD_AL25_B_CDETCTRL_DBEXIT   )
#define MD_AL25_REG_CDETCTRL_DBIDLE_DISABLE     0                                  
#define MD_AL25_REG_CDETCTRL_DBIDLE_ENABLE      ( 1 << MD_AL25_B_CDETCTRL_DBIDLE   )
#define MD_AL25_REG_CDETCTRL_CDPDET_VDP_SRC     0                                  
#define MD_AL25_REG_CDETCTRL_CDPDET_PULLUP      ( 1 << MD_AL25_B_CDETCTRL_CDPDET   )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x0B - RFU
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x0C - Control 1
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CTRL1_COMN1SW   0
#define MD_AL25_B_CTRL1_COMP2SW   3
#define MD_AL25_B_CTRL1_MICEN     6
#define MD_AL25_B_CTRL1_IDBEN     7

#define MD_AL25_M_CTRL1_COMN1SW   0x07
#define MD_AL25_M_CTRL1_COMP2SW   0x38
#define MD_AL25_M_CTRL1_MICEN     0x40
#define MD_AL25_M_CTRL1_IDBEN     0x80

#define MD_AL25_REG_CTRL1_COMN1SW_HIZ   0
#define MD_AL25_REG_CTRL1_COMN1SW_DN1   ( 1 << MD_AL25_B_CTRL1_COMN1SW )
#define MD_AL25_REG_CTRL1_COMN1SW_SL1   ( 2 << MD_AL25_B_CTRL1_COMN1SW )
#define MD_AL25_REG_CTRL1_COMN1SW_UT1   ( 3 << MD_AL25_B_CTRL1_COMN1SW )
#define MD_AL25_REG_CTRL1_COMP2SW_HIZ   0
#define MD_AL25_REG_CTRL1_COMP2SW_DP2   ( 1 << MD_AL25_B_CTRL1_COMP2SW )
#define MD_AL25_REG_CTRL1_COMP2SW_SR2   ( 2 << MD_AL25_B_CTRL1_COMP2SW )
#define MD_AL25_REG_CTRL1_COMP2SW_UT2   ( 3 << MD_AL25_B_CTRL1_COMP2SW )
#define MD_AL25_REG_CTRL1_MICEN_OPEN    0
#define MD_AL25_REG_CTRL1_MICEN_CONN    ( 1 << MD_AL25_B_CTRL1_MICEN   )
#define MD_AL25_REG_CTRL1_IDBEN_OPEN    0
#define MD_AL25_REG_CTRL1_IDBEN_CONN    ( 1 << MD_AL25_B_CTRL1_IDBEN   )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x0D - Control 2
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CTRL2_LOWPWR      0
#define MD_AL25_B_CTRL2_ADCEN       1
#define MD_AL25_B_CTRL2_CPEN        2
#define MD_AL25_B_CTRL2_SFOUTASRT   3
#define MD_AL25_B_CTRL2_SFOUTORD    4
#define MD_AL25_B_CTRL2_ACCDET      5
#define MD_AL25_B_CTRL2_USBCPLNT    6
#define MD_AL25_B_CTRL2_RCPS        7

#define MD_AL25_M_CTRL2_LOWPWR      0x01
#define MD_AL25_M_CTRL2_ADCEN       0x02
#define MD_AL25_M_CTRL2_CPEN        0x04
#define MD_AL25_M_CTRL2_SFOUTASRT   0x08
#define MD_AL25_M_CTRL2_SFOUTORD    0x10
#define MD_AL25_M_CTRL2_ACCDET      0x20
#define MD_AL25_M_CTRL2_USBCPLNT    0x40
#define MD_AL25_M_CTRL2_RCPS        0x80

#define MD_AL25_REG_CTRL2_LOWPWR_DISABLE      0
#define MD_AL25_REG_CTRL2_LOWPWR_ENABLE       ( 1 << MD_AL25_B_CTRL2_LOWPWR    )
#define MD_AL25_REG_CTRL2_ADCEN_DISABLE       0
#define MD_AL25_REG_CTRL2_ADCEN_ENABLE        ( 1 << MD_AL25_B_CTRL2_ADCEN     )
#define MD_AL25_REG_CTRL2_CPEN_DISABLE        0
#define MD_AL25_REG_CTRL2_CPEN_ENABLE         ( 1 << MD_AL25_B_CTRL2_CPEN      )
#define MD_AL25_REG_CTRL2_SFOUTASRT_NORMAL    0
#define MD_AL25_REG_CTRL2_SFOUTASRT_IMMED     ( 1 << MD_AL25_B_CTRL2_SFOUTASRT )
#define MD_AL25_REG_CTRL2_SFOUTORD_FORCEOFF   0
#define MD_AL25_REG_CTRL2_SFOUTORD_NORMAL     ( 1 << MD_AL25_B_CTRL2_SFOUTORD  )
#define MD_AL25_REG_CTRL2_ACCDET_DISABLE      0
#define MD_AL25_REG_CTRL2_ACCDET_ENABLE       ( 1 << MD_AL25_B_CTRL2_ACCDET    )
#define MD_AL25_REG_CTRL2_USBCPLNT_DISABLE    0
#define MD_AL25_REG_CTRL2_USBCPLNT_ENABLE     ( 1 << MD_AL25_B_CTRL2_USBCPLNT  )
#define MD_AL25_REG_CTRL2_RCPS_DISABLE        0
#define MD_AL25_REG_CTRL2_RCPS_ENABLE         ( 1 << MD_AL25_B_CTRL2_RCPS      )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x0E - Control 3
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CTRL3_JIGSET     0
#define MD_AL25_B_CTRL3_BOOTSET    2
#define MD_AL25_B_CTRL3_ADCDBSET   4
#define MD_AL25_B_CTRL3_WBTH       6

#define MD_AL25_M_CTRL3_JIGSET     0x03
#define MD_AL25_M_CTRL3_BOOTSET    0x0C
#define MD_AL25_M_CTRL3_ADCDBSET   0x30
#define MD_AL25_M_CTRL3_WBTH       0xC0

#define MD_AL25_REG_CTRL3_JIGSET_AUTO        ( 0 << MD_AL25_B_CTRL3_JIGSET   )
#define MD_AL25_REG_CTRL3_JIGSET_HI          ( 1 << MD_AL25_B_CTRL3_JIGSET   )
#define MD_AL25_REG_CTRL3_JIGSET_LO          ( 2 << MD_AL25_B_CTRL3_JIGSET   )
#define MD_AL25_REG_CTRL3_BOOTSET_AUTO       ( 0 << MD_AL25_B_CTRL3_BOOTSET  )
#define MD_AL25_REG_CTRL3_BOOTSET_LO         ( 1 << MD_AL25_B_CTRL3_BOOTSET  )
#define MD_AL25_REG_CTRL3_BOOTSET_HI         ( 2 << MD_AL25_B_CTRL3_BOOTSET  )
#define MD_AL25_REG_CTRL3_BOOTSET_HIZ        ( 3 << MD_AL25_B_CTRL3_BOOTSET  )
#define MD_AL25_REG_CTRL3_ADCDBSET_0P5MS     ( 0 << MD_AL25_B_CTRL3_ADCDBSET )
#define MD_AL25_REG_CTRL3_ADCDBSET_10MS      ( 1 << MD_AL25_B_CTRL3_ADCDBSET )
#define MD_AL25_REG_CTRL3_ADCDBSET_25MS      ( 2 << MD_AL25_B_CTRL3_ADCDBSET )
#define MD_AL25_REG_CTRL3_ADCDBSET_38P62MS   ( 3 << MD_AL25_B_CTRL3_ADCDBSET )
#define MD_AL25_REG_CTRL3_WBTH_3P7V          ( 0 << MD_AL25_B_CTRL3_WBTH     )
#define MD_AL25_REG_CTRL3_WBTH_3P5V          ( 1 << MD_AL25_B_CTRL3_WBTH     )
#define MD_AL25_REG_CTRL3_WBTH_3P3V          ( 2 << MD_AL25_B_CTRL3_WBTH     )
#define MD_AL25_REG_CTRL3_WBTH_3P1V          ( 3 << MD_AL25_B_CTRL3_WBTH     )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x0F - Charge Control 1
 * BitField, Mask and Bit Setting definitions
 *
 * NOTE: TTRW Read Only
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CHGCTRL1_TTRW   0
#define MD_AL25_B_CHGCTRL1_TCHW   4

#define MD_AL25_M_CHGCTRL1_TTRW   0x07
#define MD_AL25_M_CHGCTRL1_TCHW   0x70

#define MD_AL25_REG_CHGCTRL1_TCHW_5HR       ( 2 << MD_AL25_B_CHGCTRL1_TCHW )
#define MD_AL25_REG_CHGCTRL1_TCHW_6HR       ( 3 << MD_AL25_B_CHGCTRL1_TCHW )
#define MD_AL25_REG_CHGCTRL1_TCHW_7HR       ( 4 << MD_AL25_B_CHGCTRL1_TCHW )
#define MD_AL25_REG_CHGCTRL1_TCHW_DISABLE   ( 7 << MD_AL25_B_CHGCTRL1_TCHW )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x10 - Charge Control 2
 * BitField, Mask and Bit Setting definitions
 *
 * NOTE: TTRU Read Only
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CHGCTRL2_TTRU       0
#define MD_AL25_B_CHGCTRL2_MBHOSTEN   6
#define MD_AL25_B_CHGCTRL2_VCHGR_RC   7

#define MD_AL25_M_CHGCTRL2_TTRU       0x07
#define MD_AL25_M_CHGCTRL2_MBHOSTEN   0x40
#define MD_AL25_M_CHGCTRL2_VCHGR_RC   0x80

#define MD_AL25_REG_CHGCTRL2_MBHOSTEN_DISABLE   0
#define MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE    ( 1 << MD_AL25_B_CHGCTRL2_MBHOSTEN )
#define MD_AL25_REG_CHGCTRL2_VCHGR_RC_DISABLE   0
#define MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE    ( 1 << MD_AL25_B_CHGCTRL2_VCHGR_RC )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x11 - Charge Control 3
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CHGCTRL3_MBCCVWRC   0

#define MD_AL25_M_CHGCTRL3_MBCCVWRC   0x0F

#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V   ( 0x0 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P00V   ( 0x1 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P02V   ( 0x2 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P04V   ( 0x3 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P06V   ( 0x4 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P08V   ( 0x5 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P10V   ( 0x6 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P12V   ( 0x7 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P14V   ( 0x8 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P16V   ( 0x9 << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P18V   ( 0xA << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P22V   ( 0xB << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P24V   ( 0xC << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P26V   ( 0xD << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P28V   ( 0xE << MD_AL25_B_CHGCTRL3_MBCCVWRC )
#define MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P35V   ( 0xF << MD_AL25_B_CHGCTRL3_MBCCVWRC )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x12 - Charge Control 4
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CHGCTRL4_MBCICHWRCH   0
#define MD_AL25_B_CHGCTRL4_MBCICHWRCL   4

#define MD_AL25_M_CHGCTRL4_MBCICHWRCH   0x0F
#define MD_AL25_M_CHGCTRL4_MBCICHWRCL   0x10

#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_200MA   ( 0x0 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_250MA   ( 0x1 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_300MA   ( 0x2 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_350MA   ( 0x3 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA   ( 0x4 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_450MA   ( 0x5 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_500MA   ( 0x6 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_550MA   ( 0x7 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_600MA   ( 0x8 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_650MA   ( 0x9 << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_700MA   ( 0xA << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_750MA   ( 0xB << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_800MA   ( 0xC << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_850MA   ( 0xD << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_900MA   ( 0xE << MD_AL25_B_CHGCTRL4_MBCICHWRCH )
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCH_950MA   ( 0xF << MD_AL25_B_CHGCTRL4_MBCICHWRCH )

#define MD_AL25_REG_CHGCTRL4_MBCICHWRCL_90MA   0
#define MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI     ( 1 << MD_AL25_B_CHGCTRL4_MBCICHWRCL )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x13 - Charge Control 5
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CHGCTRL5_EOCS   0

#define MD_AL25_M_CHGCTRL5_EOCS   0x0F

#define MD_AL25_REG_CHGCTRL5_EOCS_50MA    ( 0x0 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_60MA    ( 0x1 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_70MA    ( 0x2 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_80MA    ( 0x3 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_90MA    ( 0x4 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_100MA   ( 0x5 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_110MA   ( 0x6 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_120MA   ( 0x7 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_130MA   ( 0x8 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_140MA   ( 0x9 << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_150MA   ( 0xA << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_160MA   ( 0xB << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_170MA   ( 0xC << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_180MA   ( 0xD << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_190MA   ( 0xE << MD_AL25_B_CHGCTRL5_EOCS )
#define MD_AL25_REG_CHGCTRL5_EOCS_200MA   ( 0xF << MD_AL25_B_CHGCTRL5_EOCS )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x14 - Charge Control 6
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CHGCTRL6_AUTOSTOP   5

#define MD_AL25_M_CHGCTRL6_AUTOSTOP   0x20

#define MD_AL25_REG_CHGCTRL6_AUTOSTOP_DISABLE   0
#define MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE    ( 1 << MD_AL25_B_CHGCTRL6_AUTOSTOP )


/**-----------------------------------------------------------------------------
 *
 * AL25 Register 0x15 - Charge Control 7
 * BitField, Mask and Bit Setting definitions
 *
 *------------------------------------------------------------------------------
 */
#define MD_AL25_B_CHGCTRL7_OTPCGHCVS   0

#define MD_AL25_M_CHGCTRL7_OTPCGHCVS   0x03

#define MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V   ( 0x0 << MD_AL25_B_CHGCTRL7_OTPCGHCVS )
#define MD_AL25_REG_CHGCTRL7_OTPCGHCVS_6P0V   ( 0x1 << MD_AL25_B_CHGCTRL7_OTPCGHCVS )
#define MD_AL25_REG_CHGCTRL7_OTPCGHCVS_6P5V   ( 0x2 << MD_AL25_B_CHGCTRL7_OTPCGHCVS )
#define MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P0V   ( 0x3 << MD_AL25_B_CHGCTRL7_OTPCGHCVS )


/**-----------------------------------------------------------------------------
 *
 * ADC Debounce time in milliseconds
 *
 *------------------------------------------------------------------------------
 */
#define   MD_AL25_ADC_DEBOUNCE_TIME_MAX   75   // todo ms


/*==============================================================================
 *
 *                            L O C A L   M A C R O S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * Device ID, Chip Revision Values
 *
 *------------------------------------------------------------------------------
 */
#define   MD_AL25_CHIP_REV_PASS_1   0x00   // todo

 
/*==============================================================================
 *
 *                             L O C A L   T Y P E S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * I2C Register List
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_REG_DEVICEID = 0x00,   /**< Device ID Reg              */
  MD_AL25_REG_INT1     = 0x01,   /**< Interrupt 1 Reg            */
  MD_AL25_REG_INT2     = 0x02,   /**< Interrupt 2 Reg            */
  MD_AL25_REG_INT3     = 0x03,   /**< Interrupt 3 Reg            */
  MD_AL25_REG_INTSTAT1 = 0x04,   /**< Interrupt Status 1 Reg     */
  MD_AL25_REG_INTSTAT2 = 0x05,   /**< Interrupt Status 2 Reg     */
  MD_AL25_REG_INTSTAT3 = 0x06,   /**< Interrupt Status 3 Reg     */
  MD_AL25_REG_INTMASK1 = 0x07,   /**< Interrupt Mask 1 Reg       */
  MD_AL25_REG_INTMASK2 = 0x08,   /**< Interrupt Mask 2 Reg       */
  MD_AL25_REG_INTMASK3 = 0x09,   /**< Interrupt Mask 3 Reg       */
  MD_AL25_REG_CDETCTRL = 0x0A,   /**< Charger Detect Control Reg */
  MD_AL25_REG_RFU      = 0x0B,   /**< Reserved Future Use        */
  MD_AL25_REG_CTRL1    = 0x0C,   /**< Control 1 Reg              */
  MD_AL25_REG_CTRL2    = 0x0D,   /**< Control 2 Reg              */
  MD_AL25_REG_CTRL3    = 0x0E,   /**< Control 3 Reg              */
  MD_AL25_REG_CHGCTRL1 = 0x0F,   /**< Charge Control 1 Reg       */
  MD_AL25_REG_CHGCTRL2 = 0x10,   /**< Charge Control 2 Reg       */
  MD_AL25_REG_CHGCTRL3 = 0x11,   /**< Charge Control 3 Reg       */
  MD_AL25_REG_CHGCTRL4 = 0x12,   /**< Charge Control 4 Reg       */
  MD_AL25_REG_CHGCTRL5 = 0x13,   /**< Charge Control 5 Reg       */
  MD_AL25_REG_CHGCTRL6 = 0x14,   /**< Charge Control 6 Reg       */
  MD_AL25_REG_CHGCTRL7 = 0x15,   /**< Charge Control 7 Reg       */
  MD_AL25_REG_MAX      = 0x16,   /**< Total # Regs               */
  MD_AL25_REG_MIN      = MD_AL25_REG_DEVICEID

} MD_AL25_REG_T;


/**-----------------------------------------------------------------------------
 *
 * Interrupt Struct
 *
 *------------------------------------------------------------------------------
 */
typedef struct {
  MCS_BOOL_T   intAdc;
  MCS_BOOL_T   intAdcLow;
  MCS_BOOL_T   intAdcError;
  MCS_BOOL_T   intChargerType;
  MCS_BOOL_T   intChargerDetectRun;
  MCS_BOOL_T   intDataContactDetectTimeout;
  MCS_BOOL_T   intDeadBatteryChargeMode;
  MCS_BOOL_T   intVbVoltage;
  MCS_BOOL_T   intEndOfCharge;
  MCS_BOOL_T   intCgMbc;
  MCS_BOOL_T   intVbOverVoltageProtection;
  MCS_BOOL_T   intMbcChgErr;

} MD_AL25_INT_T;




/*==============================================================================
 *
 *                          L O C A L   P R O T O T Y P E S
 *
 *==============================================================================
 */


/*==============================================================================
 *
 *                           L O C A L   V A R I A B L E S
 *
 *==============================================================================
 */

/**-----------------------------------------------------------------------------
 *
 * AL25 ADC setting values with 200kOhm pullup
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_ADC_GND_ADCLOW   = 0x00,                  
  MD_AL25_ADC_2K           = 0x01,                  
  MD_AL25_ADC_2P6K         = 0x02,                  
  MD_AL25_ADC_3P2K         = 0x03,                  
  MD_AL25_ADC_4K           = 0x04,                  
  MD_AL25_ADC_4P8K         = 0x05,                  
  MD_AL25_ADC_6K           = 0x06,                  
  MD_AL25_ADC_8K           = 0x07,                  
  MD_AL25_ADC_10K          = 0x08,                  
  MD_AL25_ADC_12K          = 0x09,                  
  MD_AL25_ADC_14K          = 0x0A,
  MD_AL25_ADC_17K          = 0x0B,                  
  MD_AL25_ADC_20K          = 0x0C,                  
  MD_AL25_ADC_24K          = 0x0D,                  
  MD_AL25_ADC_29K          = 0x0E,                  
  MD_AL25_ADC_34K          = 0x0F,
  MD_AL25_ADC_40K          = 0x10,                  
  MD_AL25_ADC_50K          = 0x11,                  
  MD_AL25_ADC_65K          = 0x12,                  
  MD_AL25_ADC_80K          = 0x13,                  
  MD_AL25_ADC_102K         = 0x14,                  
  MD_AL25_ADC_121K         = 0x15,                  
  MD_AL25_ADC_150K         = 0x16,                  
  MD_AL25_ADC_200K         = 0x17,                  
  MD_AL25_ADC_255K         = 0x18,                  
  MD_AL25_ADC_301K         = 0x19,                  
  MD_AL25_ADC_365K         = 0x1A,
  MD_AL25_ADC_442K         = 0x1B,                  
  MD_AL25_ADC_523K         = 0x1C,                  
  MD_AL25_ADC_619K         = 0x1D,                  
  MD_AL25_ADC_1000K        = 0x1E,                  
  MD_AL25_ADC_OPEN         = 0x1F,
  MD_AL25_ADC_TABLE_MAX    = 0x20,

  MD_AL25_ADC_INIT,
  MD_AL25_ADC_GND,

  MD_AL25_ADC_TABLE_MIN   = MD_AL25_ADC_GND

} MD_AL25_ADC_T;


/**-----------------------------------------------------------------------------
 *
 * AL25 CHG_TYP setting values
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_CHGTYP_NO_VOLTAGE        = 0x00,   /**< No Valid voltage at VB (Vvb < Vvbdet)                          */
  MD_AL25_CHGTYP_USB               = 0x01,   /**< Unknown (D+/D- does not present a valid USB charger signature) */
  MD_AL25_CHGTYP_DOWNSTREAM_PORT   = 0x02,   /**< Charging Downstream Port                                       */
  MD_AL25_CHGTYP_DEDICATED_CHGR    = 0x03,   /**< Dedicated Charger (D+/D- shorted)                              */
  MD_AL25_CHGTYP_500MA             = 0x04,   /**< Special 500mA charger, max current 500mA                       */
  MD_AL25_CHGTYP_1A                = 0x05,   /**< Special 1A charger, max current 1A                             */
  MD_AL25_CHGTYP_RFU               = 0x06,   /**< Reserved for Future Use                                        */
  MD_AL25_CHGTYP_DB_100MA          = 0x07,   /**< Dead Battery Charging, max current 100mA                       */
  MD_AL25_CHGTYP_MAX,

  MD_AL25_CHGTYP_INIT,
  MD_AL25_CHGTYP_MIN = MD_AL25_CHGTYP_NO_VOLTAGE

} MD_AL25_CHGTYP_T;


/**-----------------------------------------------------------------------------
 *
 * AL25 DCHK (time for charger type detection) settings
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_DCHK_50MS  = 0x00,
  MD_AL25_DCHK_620MS = 0x10
} MD_AL25_DCHK_T;


/**-----------------------------------------------------------------------------
 *
 * AL25 RCPS (click/pop resistors for SL1, SR2) settings
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_RCPS_DISABLE = 0x00,
  MD_AL25_RCPS_ENABLE  = 0x80
} MD_AL25_RCPS_T;


/**-----------------------------------------------------------------------------
 *
 * AL25 USBCplnt (USB 2.0 compliant) settings
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_USBCPLNT_DISABLE = 0x00,
  MD_AL25_USBCPLNT_ENABLE  = 0x40
} MD_AL25_USBCPLNT_T;


/**-----------------------------------------------------------------------------
 *
 * AL25 SFOutOrd (SFOUT Override Control) settings
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_SFOUTORD_FORCE_OFF = 0x00,
  MD_AL25_SFOUTORD_NORMAL    = 0x10
} MD_AL25_SFOUTORD_T;


/**-----------------------------------------------------------------------------
 *
 * AL25 SFOutAsrt (SFOut Assert Time) settings
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_SFOUTASRT_NORMAL    = 0x00,
  MD_AL25_SFOUTASRT_IMMEDIATE = 0x08
} MD_AL25_SFOUTASRT_T;


/**-----------------------------------------------------------------------------
 *
 * AL25 LowPwr (Low Power Pulse Mode) settings
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_AL25_LOWPWR_DISABLE   = 0x00,
  MD_AL25_LOWPWR_ENABLE    = 0x01
} MD_AL25_LOWPWR_T;


/**-----------------------------------------------------------------------------
 *
 * User defined configuration parameters
 *
 *------------------------------------------------------------------------------
 */
typedef struct {
  MD_AL25_DCHK_T        dchk;            /**< Time for Charger Type Detection */
  MCS_BOOL_T            usbOtgCapable;   /**< USB OTG capable                 */

  MD_AL25_RCPS_T        rcps;            /**< Click/Pop resistor setting      */
  MD_AL25_USBCPLNT_T    usbCplnt;        /**< USB 2.0 Charge Compliant        */
  MD_AL25_SFOUTORD_T    sfOutOrd;        /**< SFOut Override Control          */
  MD_AL25_SFOUTASRT_T   sfOutAsrt;       /**< SFOUT Assert Time               */
  MD_AL25_LOWPWR_T      lowPwr;          /**< Low Power Pulse Mode            */

} MD_AL25_USERCFG_T;


/**-----------------------------------------------------------------------------
 *
 * Interrupt Status Struct
 *
 *------------------------------------------------------------------------------
 */
typedef struct {
  MD_AL25_ADC_T      statAdc;
  MCS_BOOL_T         statAdcLow;
  MCS_BOOL_T         statAdcError;
  MD_AL25_CHGTYP_T   statChargerType;
  MCS_BOOL_T         statChargerDetectRun;
  MCS_BOOL_T         statDataContactDetectTimeout;
  MCS_BOOL_T         statDeadBatteryChargeMode;
  MCS_BOOL_T         statVbVoltage;
  MCS_BOOL_T         statEndOfCharge;
  MCS_BOOL_T         statCgMbc;
  MCS_BOOL_T         statVbOverVoltageProtection;
  MCS_BOOL_T         statMbcChgErr;

} MD_AL25_INTSTAT_T;


/**-----------------------------------------------------------------------------
 *
 * Current object parameters
 *
 *------------------------------------------------------------------------------
 */
typedef struct {
  MD_AL25_INTSTAT_T        intStat;
  MCS_BOOL_T               forceChgTypM;
  MCS_U8_T                 revision;
  MD_AL25_USERCFG_T        userCfg;

} MD_AL25_INSTANCE_T;


/*==============================================================================
 *
 *                        E X T E R N A L   M E T H O D S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * Initializes module
 *
 * @param pUserCfgData   in : pointer to user configuration data structure
 *                            NULL will use default values
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_ModuleInit( MD_AL25_USERCFG_T *userCfgData );


/**-----------------------------------------------------------------------------
 *
 * Opens module
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_ModuleOpen( void );


/**-----------------------------------------------------------------------------
 *
 * AL25 Interrupt Handler
 *   This routine should NOT be run in the context of the ISR!
 *
 * - Reads Status Regs
 * - Notifies changes
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_ServiceStateMachine( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for No Accessory
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_None( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for Dedicated Charger
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_DedChgr( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for Audio accessory
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_Audio( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for TTY accessory
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_TTY( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for Downstream Charging Port
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_DwnStrmChgPort( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for standard USB
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_Usb( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for UART
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_Uart( void );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for Factory USB accessory
 *
 * @param conn   boot : TRUE  - boot ON
 *                      FALSE - boot OFF
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_FactoryUsb( MCS_BOOL_T boot );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for Factory UART accessory
 *
 * @param conn   boot : TRUE  - boot ON
 *                      FALSE - boot OFF
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_FactoryUart( MCS_BOOL_T boot );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for Audio/Video Accessory connect and disconnect
 *
 * @param conn   in : request to connect(1) or disconnect(0)
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_AV( MCS_BOOL_T conn );


/**-----------------------------------------------------------------------------
 *
 * Requests configuration for USB OTG
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_UsbOtg( void );


#ifdef __cplusplus
}
#endif


#endif  /* MD_AL25_HDR */

