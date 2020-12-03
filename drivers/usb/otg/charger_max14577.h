/*******************************************************************************
*
* Copyright (C) 2009 Maxim Integrated Products, Inc. All Rights Reserved.
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
 * @file MD_MAX8929.h
 * 
 * $Header: /CvsRepositories/CApps3/device/ext/MD_MAX8929.h,v 1.1.2.2 2009/06/08 04:02:54 wdawkins Exp $
 *
 * @brief 
 * Provides public interface to MD_MAX8929
 * 
 * @author 
 * Scooter Dawkins
 * 
 * @date 
 * April 6, 2009
 * 
 * @namespace 
 * MD_MAX8929
 * 
 * @reference todo
 *
 * @standard_constraints 
 * Before any MD_MAX8929 module function can be called, 
 * the MD_MAX8929 module must be initialized.
 *
 * @code
 * MD_MAX8929_ModuleInit();  // Module initialization
 * @endcode
 *
 * The following methods are exported for MD_MAX14521 clients:
 *
 * @code
 * @todo function exports
 * @endcode
 *
 * v0.9
 * 
 *------------------------------------------------------------------------------
 */


#ifndef MD_MAX8929_HDR
#define MD_MAX8929_HDR


#include "charger_MAX8929.h"

#ifdef __cplusplus
extern "C"{
#endif

/*==============================================================================
 *
 *                           I N C L U D E   F I L E S
 *
 *==============================================================================
 */

#define FEAT_EN_SAMSUNG_API  	1

#if FEAT_EN_SAMSUNG_API
  #define   MCS_U8_T    unsigned char
  #define   MCS_U16_T   unsigned short
  #define   MCS_U32_T   unsigned int
  #define   MCS_BOOL_T  unsigned char
  #define   MCS_TRUE    1
  #define   MCS_FALSE   0

#else
  #include "MC_Std.h"

#endif


/*==============================================================================
 *
 *                      E X T E R N A L   C O N S T A N T S
 *
 *==============================================================================
 */


/*==============================================================================
 *
 *                         E X T E R N A L   M A C R O S
 *
 *==============================================================================
 */
/* enalbing debug massage related with FSA9480*/
#define MAX8929_DBG_ENABLE	1

#ifdef  MAX8929_DBG_ENABLE
#define DEBUG_MAX8929(fmt,args...) printk(fmt, ##args)
#else
#define DEBUG_MAX8929(fmt,args...) do {} while(0)
#endif

/*==============================================================================
 *
 *                          E X T E R N A L   T Y P E S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * MAX8929 ADC setting values
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_MAX8929_ADC_GND   = 0x00,                  
  MD_MAX8929_ADC_75    = 0x00,                  
  MD_MAX8929_ADC_2K    = 0x01,                  
  MD_MAX8929_ADC_2_6K  = 0x02,                  
  MD_MAX8929_ADC_3_2K  = 0x03,                  
  MD_MAX8929_ADC_4_0K  = 0x04,                  
  MD_MAX8929_ADC_4_8K  = 0x05,                  
  MD_MAX8929_ADC_6K    = 0x06,                  
  MD_MAX8929_ADC_8K    = 0x07,                  
  MD_MAX8929_ADC_10K   = 0x08,                  
  MD_MAX8929_ADC_12K   = 0x09,                  
  MD_MAX8929_ADC_14K   = 0x0a,                  
  MD_MAX8929_ADC_17K   = 0x0b,                  
  MD_MAX8929_ADC_20_5K = 0x0c,                  
  MD_MAX8929_ADC_24K   = 0x0d,                  
  MD_MAX8929_ADC_28_7K = 0x0e,                  
  MD_MAX8929_ADC_34K   = 0x0f,                  
  MD_MAX8929_ADC_40K   = 0x10,                  
  MD_MAX8929_ADC_50K   = 0x11,                  
  MD_MAX8929_ADC_65K   = 0x12,                  
  MD_MAX8929_ADC_80K   = 0x13,                  
  MD_MAX8929_ADC_102K  = 0x14,                  
  MD_MAX8929_ADC_121K  = 0x15,                  
  MD_MAX8929_ADC_150K  = 0x16,                  
  MD_MAX8929_ADC_200K  = 0x17,                  
  MD_MAX8929_ADC_255K  = 0x18,                  
  MD_MAX8929_ADC_301K  = 0x19,                  
  MD_MAX8929_ADC_365K  = 0x1a,                  
  MD_MAX8929_ADC_442K  = 0x1b,                  
  MD_MAX8929_ADC_523K  = 0x1c,                  
  MD_MAX8929_ADC_619K  = 0x1d,                  
  MD_MAX8929_ADC_1000K = 0x1e,                  
  MD_MAX8929_ADC_OPEN  = 0x1f,                  
  MD_MAX8929_ADC_TABLE_MAX,
  MD_MAX8929_ADC_INIT,

  MD_MAX8929_ADC_TABLE_MIN = MD_MAX8929_ADC_GND

} MD_MAX8929_ADC_T;


/**-----------------------------------------------------------------------------
 *
 * MAX8929 CHG_TYP setting values
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_MAX8929_CHGTYP_NO_VOLTAGE        = 0x00,   /**< No Valid voltage at VB (Vvb < Vvbdet)                          */
  MD_MAX8929_CHGTYP_UNKNOWN           = 0x01,   /**< Unknown (D+/D- does not present a valid USB charger signature) */
  MD_MAX8929_CHGTYP_USB_HIGH_CURRENT  = 0x02,   /**< USB High Current Host/Hub                                      */
  MD_MAX8929_CHGTYP_DEDICATED_CHGR    = 0x03,   /**< Dedicated Charger (D+/D- shorted)                              */
  MD_MAX8929_CHGTYP_MAX,
  MD_MAX8929_CHGTYP_INIT,

  MD_MAX8929_CHGTYP_MIN = MD_MAX8929_CHGTYP_NO_VOLTAGE

} MD_MAX8929_CHGTYP_T;


/**-----------------------------------------------------------------------------
 *
 * MAX8929 Accessories
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_MAX8929_ACCESSORY_MIN  = 0,                          /**< todo */

  MD_MAX8929_ACCESSORY_NONE = MD_MAX8929_ACCESSORY_MIN,   /**< todo */
  MD_MAX8929_ACCESSORY_ILLEGAL,                           /**< todo */

  MD_MAX8929_ACCESSORY_USBOTG,                            /**< todo */
  MD_MAX8929_ACCESSORY_AV_LOAD_NO_CHGR,                   /**< todo */
  MD_MAX8929_ACCESSORY_AV_LOAD_MANUAL_CHGR,               /**< todo */
  MD_MAX8929_ACCESSORY_AV_LOAD_AUTO_CHGR,                 /**< 5 todo */

  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1,                     /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S0,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S1,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S2,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S3,                  /**< 10 todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S4,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S5,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S6,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S7,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S8,                  /**< 15 todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S9,                  /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S10,                 /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S11,                 /**< todo */
  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S12,                 /**< todo */

  MD_MAX8929_ACCESSORY_RESERVED_1,                        /**< 20 todo */
  MD_MAX8929_ACCESSORY_RESERVED_2,                        /**< todo */
  MD_MAX8929_ACCESSORY_RESERVED_3,                        /**< todo */
  MD_MAX8929_ACCESSORY_RESERVED_4,                        /**< todo */
  MD_MAX8929_ACCESSORY_RESERVED_5,                        /**< todo */

  MD_MAX8929_ACCESSORY_AUDDEV_TYPE_2,                     /**< 25 todo */
  MD_MAX8929_ACCESSORY_PHONE_PWD,                         /**< todo */
  MD_MAX8929_ACCESSORY_TTY_CONVERTER,                     /**< todo */

  MD_MAX8929_ACCESSORY_UART_NO_CHGR,                      /**< todo */
  MD_MAX8929_ACCESSORY_UART_MANUAL_CHGR,                  /**< todo */
  MD_MAX8929_ACCESSORY_UART_AUTO_CHGR,                    /**< 30 todo */

  MD_MAX8929_ACCESSORY_CEA_936A_TYPE_1_NO_PWR,            /**< todo */
  MD_MAX8929_ACCESSORY_CEA_936A_TYPE_1_MANUAL,            /**< todo */
  MD_MAX8929_ACCESSORY_CEA_936A_TYPE_1_AUTO,              /**< todo */

  MD_MAX8929_ACCESSORY_FACTORY_USB_BOOT_OFF,              /**< todo */
  MD_MAX8929_ACCESSORY_FACTORY_USB_BOOT_ON,               /**< 35 todo */

  MD_MAX8929_ACCESSORY_AV_NO_LOAD_NO_CHGR,                /**< todo */
  MD_MAX8929_ACCESSORY_AV_NO_LOAD_MANUAL_CHGR,            /**< todo */
  MD_MAX8929_ACCESSORY_AV_NO_LOAD_AUTO_CHGR,              /**< todo */

  MD_MAX8929_ACCESSORY_CEA_936A_TYPE_2_NO_PWR,            /**< todo */
  MD_MAX8929_ACCESSORY_CEA_936A_TYPE_2_MANUAL,            /**< 40 todo */
  MD_MAX8929_ACCESSORY_CEA_936A_TYPE_2_AUTO,              /**< todo */

  MD_MAX8929_ACCESSORY_FACTORY_UART_BOOT_OFF,             /**< todo */
  MD_MAX8929_ACCESSORY_FACTORY_UART_BOOT_ON,              /**< todo */

  MD_MAX8929_ACCESSORY_USB,                               /**< todo */
  MD_MAX8929_ACCESSORY_USB_CHGR,                          /**< 45 todo */
  MD_MAX8929_ACCESSORY_DEDICATED_CHGR,                    /**< todo */

  MD_MAX8929_ACCESSORY_MAX,                               /**< 47 todo */

  MD_MAX8929_ACCESSORY_UNKNOWN,                           /**< 48 todo */
  MD_MAX8929_ACCESSORY_INPROGRESS,                        /**< 49 todo */
  MD_MAX8929_ACCESSORY_CHGTYP_INPROGRESS                  /**< 50 todo */

} MD_MAX8929_ACCESSORY_T;


/**-----------------------------------------------------------------------------
 *
 * User defined configuration parameters.  These are set once at bootup,
 *   currently in MAX8929_ModuleInit
 *
 *------------------------------------------------------------------------------
 */
typedef struct {
  MCS_BOOL_T   serviceChgEnd;               /**< Ignore CHG_END interrupt?                 */
  MCS_BOOL_T   dchk;                        /**< DCHK_x value                              */
  MCS_U32_T    manualChgrDetCount;          /**< number of times to loop, looking for chgr */

  MCS_U32_T    keyTimerValue_tkp;           /**< Key Handler value for Short Key Press                  */
  MCS_U32_T    keyTimerValue_tlkp;          /**< Key Handler value for Long Key Press (added to Short ) */

  MCS_U8_T     defaultChgStp;               /**< default CHG_STP value                     */
  MCS_U8_T     defaultChgSeti;              /**< default CHG_SETI values                   */
  MCS_U8_T     avProfileChgStp;             /**< CHG_STP  to use with AV cables            */
  MCS_U8_T     avProfileChgSeti;            /**< CHG_SETI to use with AV cables            */
  MCS_U8_T     dedChgrProfileChgStp;        /**< CHG_STP  to use with dedicated chgr       */
  MCS_U8_T     dedChgrProfileChgSeti;       /**< CHG_SETI to use with dedicated chgr       */
  MCS_U8_T     usbProfileChgStp;            /**< CHG_STP  to use with USB                  */
  MCS_U8_T     usbProfileChgSeti;           /**< CHG_SETI to use with USB                  */
  MCS_U8_T     usbChgrProfileChgStp;        /**< CHG_STP  to use with USB chgr             */
  MCS_U8_T     usbChgrProfileChgSeti;       /**< CHG_SETI to use with USB chgr             */
  MCS_U8_T     chgrType1ProfileChgStp;      /**< CHG_STP  to use with Chgr Type 1          */
  MCS_U8_T     chgrType1ProfileChgSeti;     /**< CHG_SETI to use with Chgr Type 1          */
  MCS_U8_T     chgrType2ProfileChgStp;      /**< CHG_STP  to use with Chgr Type 2          */
  MCS_U8_T     chgrType2ProfileChgSeti;     /**< CHG_SETI to use with Chgr Type 2          */
  MCS_U8_T     factoryProfileChgEn;         /**< CHG_EN   setting for factory accessories  */
  MCS_U8_T     factoryProfileChgStp;        /**< CHG_STP  to use with factory              */
  MCS_U8_T     factoryProfileChgSeti;       /**< CHG_SETI to use with factory              */
  MCS_U8_T     uartProfileChgStp;           /**< CHG_STP  to use with UART                 */
  MCS_U8_T     uartProfileChgSeti;          /**< CHG_SETI to use with UART                 */

  MCS_U8_T     avProfileMaintChgStp;             /**< CHG_STP  to use Maint with AV cables      */
  MCS_U8_T     avProfileMaintChgSeti;            /**< CHG_SETI to use Maint with AV cables      */
  MCS_U8_T     dedChgrProfileMaintChgStp;        /**< CHG_STP  to use Maint with dedicated chgr */
  MCS_U8_T     dedChgrProfileMaintChgSeti;       /**< CHG_SETI to use Maint with dedicated chgr */
  MCS_U8_T     usbProfileMaintChgStp;            /**< CHG_STP  to use Maint with USB            */
  MCS_U8_T     usbProfileMaintChgSeti;           /**< CHG_SETI to use Maint with USB            */
  MCS_U8_T     usbChgrProfileMaintChgStp;        /**< CHG_STP  to use Maint with USB chgr       */
  MCS_U8_T     usbChgrProfileMaintChgSeti;       /**< CHG_SETI to use Maint with USB chgr       */
  MCS_U8_T     chgrType1ProfileMaintChgStp;      /**< CHG_STP  to use Maint with Chgr Type 1    */
  MCS_U8_T     chgrType1ProfileMaintChgSeti;     /**< CHG_SETI to use Maint with Chgr Type 1    */
  MCS_U8_T     chgrType2ProfileMaintChgStp;      /**< CHG_STP  to use Maint with Chgr Type 2    */
  MCS_U8_T     chgrType2ProfileMaintChgSeti;     /**< CHG_SETI to use Maint with Chgr Type 2    */
  MCS_U8_T     factoryProfileMaintChgStp;        /**< CHG_STP  to use Maint with factory        */
  MCS_U8_T     factoryProfileMaintChgSeti;       /**< CHG_SETI to use Maint with factory        */
  MCS_U8_T     uartProfileMaintChgStp;           /**< CHG_STP  to use Maint with UART           */
  MCS_U8_T     uartProfileMaintChgSeti;          /**< CHG_SETI to use Maint with UART           */

} MD_MAX8929_USERCFG_T;


/**-----------------------------------------------------------------------------
 *
 * Events for key handling state machine and key notifications to system
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_MAX8929_KEYEVENT_RELEASE   = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1,            /**< No Key Pressed */
  MD_MAX8929_KEYEVENT_S0_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S0,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S1_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S1,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S2_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S2,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S3_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S3,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S4_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S4,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S5_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S5,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S6_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S6,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S7_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S7,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S8_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S8,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S9_PRESS  = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S9,         /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S10_PRESS = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S10,        /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S11_PRESS = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S11,        /**< Key Sx Pressed */
  MD_MAX8929_KEYEVENT_S12_PRESS = MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1_S12,        /**< Key Sx Pressed */

  MD_MAX8929_KEYEVENT_TIMER_EXPIRE                                               /**< Key Press Timer Expired */

} MD_MAX8929_KEYEVENT_T;


/**-----------------------------------------------------------------------------
 *
 * Key Press States
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MD_MAX8929_KEYPRESS_RELEASE,    /**< Key Released                */
  MD_MAX8929_KEYPRESS_INIT,       /**< Key Pressed, t=0            */
  MD_MAX8929_KEYPRESS_SHORT,      /**< Key Pressed, t>tkp & t<tlkp */
  MD_MAX8929_KEYPRESS_LONG        /**< Key Pressed, t>tlkp         */

} MD_MAX8929_KEYPRESS_T;

typedef enum
{
	CHARGINGMGR_NOTIFIER_INVALID_EVENT = 0,
		
	CHARGINGMGR_NOTIFIER_EARJACK_ATTACHED,    // 1
	CHARGINGMGR_NOTIFIER_EARJACK_DETACHED,

	CHARGINGMGR_NOTIFIER_SHORTKEY_PRESSED,    // 3
	CHARGINGMGR_NOTIFIER_LONGKEY_PRESSED,
	CHARGINGMGR_NOTIFIER_LONGKEY_RELEASED,    // 5

	CHARGINGMGR_NOTIFIER_TA_ATTACHED,         // 6
	CHARGINGMGR_NOTIFIER_TA_DETACHED,         // 7

	CHARGINGMGR_NOTIFIER_USB_ATTACHED,
	CHARGINGMGR_NOTIFIER_USB_DETACHED,        // 9

	CHARGINGMGR_NOTIFIER_CARKIT_ATTACHED,     // A
	CHARGINGMGR_NOTIFIER_CARKIT_DETACHED,
	
	CHARGINGMGR_NOTIFIER_COMPLETE_CHARGING,   // C

	CHARGINGMGR_NOTIFIER_STOP_BY_TEMP,
	CHARGINGMGR_NOTIFIER_GO_BY_TEMP           // E
} Charging_Notifier_t;


// todo
typedef enum {
  MAX8929_GLUE_CHARGETYPE_NORMAL      = 0,   /**< Use normal/default charging profile */
  MAX8929_GLUE_CHARGETYPE_MAINTENANCE = 1    /**< Use maintenance charging profile    */
} MAX8929_GLUE_CHARGETYPE_T;


#ifdef __cplusplus
}
#endif


// microusb device id
#define MICROUSBIC_5W_CHARGER		6
#define MICROUSBIC_JIG_UART_OFF		5
#define MICROUSBIC_JIG_UART_ON		4

#define MICROUSBIC_TA_CHARGER		3
#define MICROUSBIC_USB_CHARGER		2
#define MICROUSBIC_USB_CABLE		1
#define MICROUSBIC_NO_DEVICE		0

void microusb_usbjig_detect(void);

int microusb_enable(void);
void microusb_disable(void);
void mcirousb_usbpath_change(int usb_path);
void mcirousb_uartpath_change(int uart_path);
int get_real_usbic_state(void);

extern void MD_full_charge_work_handler(void);
extern void MD_cable_detection_work_handler(void);

extern s32 MD_normal_i2c_read_byte(u8 devaddr, u8 regoffset, u8 *value);
extern s32 MD_normal_i2c_write_byte(u8 devaddr, u8 regoffset, u8 value);


#endif  /* MD_MAX8929_HDR */

