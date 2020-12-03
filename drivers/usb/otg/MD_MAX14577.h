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
 * @file MG_AL25_Samsung.h
 * 
 * $Header: /CvsRepositories/CApps3/device/ext/MG_AL25_Samsung.h,v 1.1.2.2 2010/04/27 06:56:16 wdawkins Exp $
 *
 * @brief 
 * Provides a Samsung specific public interface to Maxim Glue (MG) module 
 *   implementation for AL25
 * 
 * @author 
 * Scooter Dawkins
 * 
 * @date 
 * April 19, 2010
 * 
 * @namespace 
 * MG_AL25
 * 
 * @reference todo
 *
 *------------------------------------------------------------------------------
 */


#ifndef MG_AL25_SAMSUNG_HDR
#define MG_AL25_SAMSUNG_HDR


#ifdef __cplusplus
extern "C"{
#endif


/*==============================================================================
 *
 *                           I N C L U D E   F I L E S
 *
 *==============================================================================
 */

/*==============================================================================
 *
 *                       C O N S T A N T S
 *
 *==============================================================================
 */

#define MICROUSBIC_JIG_USB_OFF				8
#define MICROUSBIC_JIG_USB_ON					7
#define MICROUSBIC_5W_CHARGER				6
#define MICROUSBIC_JIG_UART_OFF				5
#define MICROUSBIC_JIG_UART_ON				4
#define	MICROUSBIC_TA_CHARGER				3
#define MICROUSBIC_USB_CHARGER				2
#define MICROUSBIC_USB_CABLE				1
#define MICROUSBIC_NO_DEVICE				0


/**-----------------------------------------------------------------------------
 *
 * Glue code customer stub code enabling/disabling
 *
 *------------------------------------------------------------------------------
 */


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


/**-----------------------------------------------------------------------------
 *
 * Supported Accessories
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MG_AL25_ACCESSORY_MIN  = 0,                          /**< 0 */

  MG_AL25_ACCESSORY_NONE = MG_AL25_ACCESSORY_MIN,      /**< 0 No Accessory */
  MG_AL25_ACCESSORY_ILLEGAL,                           /**< 1 Illegal      */

  MG_AL25_ACCESSORY_AUDDEV_TYPE_1,                     /**<  2 Audio Type 1, no  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0,                  /**<  3 Audio Type 1, S0  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1,                  /**<  4 Audio Type 1, S1  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2,                  /**<  5 Audio Type 1, S2  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3,                  /**<  6 Audio Type 1, S3  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4,                  /**<  7 Audio Type 1, S4  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5,                  /**<  8 Audio Type 1, S5  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6,                  /**<  9 Audio Type 1, S6  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7,                  /**< 10 Audio Type 1, S7  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8,                  /**< 11 Audio Type 1, S8  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9,                  /**< 12 Audio Type 1, S9  button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10,                 /**< 13 Audio Type 1, S10 button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11,                 /**< 14 Audio Type 1, S11 button */
  MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12,                 /**< 15 Audio Type 1, S12 button */

  MG_AL25_ACCESSORY_TTY_CONVERTER,                     /**< 16 TTY Converter */

  MG_AL25_ACCESSORY_UART_NO_CHGR,                      /**< 17 UART, ChgTyp 000 */
  MG_AL25_ACCESSORY_UART_MANUAL_CHGR,                  /**< 18 UART, ChgTyp 001 */
  MG_AL25_ACCESSORY_UART_AUTO_CHGR,                    /**< 19 UART, ChgTyp 01x */

  MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF,              /**< 20 Factory Usb,  Boot Off */
  MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON,               /**< 21 Factory Usb,  Boot On  */
  MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF,             /**< 22 Facotry Uart, Boot Off */
  MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON,              /**< 23 Facotry Uart, Boot On  */

  MG_AL25_ACCESSORY_USB,                               /**< 24 Normal USB               */
  MG_AL25_ACCESSORY_USBCHGR,                           /**< 25 Downstream Charging Port */
  MG_AL25_ACCESSORY_DEDCHGR_1P8A,                      /**< 26 Dedicated Charger, 1.8A  */
  MG_AL25_ACCESSORY_DEDCHGR_500MA,                     /**< 27 Dedicated Charger, 500mA */
  MG_AL25_ACCESSORY_DEDCHGR_1A,                        /**< 28 Dedicated Charger, 1A    */
  MG_AL25_ACCESSORY_DEDCHGR_100MA,                     /**< 29 Dedicated Charger, 100mA */

  MG_AL25_ACCESSORY_MAX,                               /**< 30 */

  MG_AL25_ACCESSORY_UNKNOWN                            /**< 31 Unknown or Initial */

//  MG_AL25_ACCESSORY_USBOTG,                            /**< todo */
//  MG_AL25_ACCESSORY_AV_LOAD_NO_CHGR,                   /**< todo */
//  MG_AL25_ACCESSORY_AV_LOAD_MANUAL_CHGR,               /**< todo */
//  MG_AL25_ACCESSORY_AV_LOAD_AUTO_CHGR,                 /**< todo */
//  MG_AL25_ACCESSORY_AV_NO_LOAD_NO_CHGR,                /**< todo */
//  MG_AL25_ACCESSORY_AV_NO_LOAD_MANUAL_CHGR,            /**< todo */
//  MG_AL25_ACCESSORY_AV_NO_LOAD_AUTO_CHGR,              /**< todo */
//  MG_AL25_ACCESSORY_CEA_936A_TYPE_2_NO_PWR,            /**< todo */
//  MG_AL25_ACCESSORY_CEA_936A_TYPE_2_MANUAL,            /**< todo */
//  MG_AL25_ACCESSORY_CEA_936A_TYPE_2_AUTO,              /**< todo */
//  MG_AL25_ACCESSORY_RESERVED_1,                        /**< todo */
//  MG_AL25_ACCESSORY_RESERVED_2,                        /**< todo */
//  MG_AL25_ACCESSORY_RESERVED_3,                        /**< todo */
//  MG_AL25_ACCESSORY_RESERVED_4,                        /**< todo */
//  MG_AL25_ACCESSORY_RESERVED_5,                        /**< todo */
//  MG_AL25_ACCESSORY_AUDDEV_TYPE_2,                     /**< todo */
//  MG_AL25_ACCESSORY_PHONE_PWD,                         /**< todo */
//  MG_AL25_ACCESSORY_CEA_936A_TYPE_1_NO_PWR,            /**< todo */
//  MG_AL25_ACCESSORY_CEA_936A_TYPE_1_MANUAL,            /**< todo */
//  MG_AL25_ACCESSORY_CEA_936A_TYPE_1_AUTO,              /**< todo */

} MG_AL25_ACCESSORY_T;


/**-----------------------------------------------------------------------------
 *
 * Events for key handling state machine and key notifications to system
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MG_AL25_KEYEVENT_RELEASE   = MG_AL25_ACCESSORY_AUDDEV_TYPE_1,       /**< No Key Pressed */
  MG_AL25_KEYEVENT_S0_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S1_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S2_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S3_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S4_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S5_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S6_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S7_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S8_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S9_PRESS  = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9,    /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S10_PRESS = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10,   /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S11_PRESS = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11,   /**< Key Sx Pressed */
  MG_AL25_KEYEVENT_S12_PRESS = MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12,   /**< Key Sx Pressed */
                                                                      
  MG_AL25_KEYEVENT_TIMER_EXPIRE                                       /**< Key Press Timer Expired */

} MG_AL25_KEYEVENT_T;


/**-----------------------------------------------------------------------------
 *
 * Key Press States
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
  MG_AL25_KEYPRESS_RELEASE,    /**< Key Released                */
  MG_AL25_KEYPRESS_INIT,       /**< Key Pressed, t=0            */
  MG_AL25_KEYPRESS_SHORT,      /**< Key Pressed, t>tkp & t<tlkp */
  MG_AL25_KEYPRESS_LONG        /**< Key Pressed, t>tlkp         */

} MG_AL25_KEYPRESS_T;


/**-----------------------------------------------------------------------------
 *
 * User defined configuration parameters.  
 *   These are typically set once at boot.
 *
 *------------------------------------------------------------------------------
 */
typedef struct {
  MCS_U32_T   keyTimerValue_tkp;           /**< Key Handler value for Short Key Press                  */
  MCS_U32_T   keyTimerValue_tlkp;          /**< Key Handler value for Long Key Press (added to Short ) */

} MG_AL25_USERCFG_T;


/**-----------------------------------------------------------------------------
 *
 * Timer Callback Function
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
typedef void MG_AL25_TMR_EXP_F( void );


/**-----------------------------------------------------------------------------
 *
 * SAMSUNG specific type used during AJ86
 *
 *------------------------------------------------------------------------------
 */

#define FEAT_EN_USE_SAMSUNG_STUB	1
#if FEAT_EN_USE_SAMSUNG_STUB
typedef enum
{
	CHARGINGMGR_NOTIFIER_INVALID_EVENT = 0,
		
	CHARGINGMGR_NOTIFIER_EARJACK_ATTACHED,    // 1
	CHARGINGMGR_NOTIFIER_EARJACK_DETACHED,    // 2

	CHARGINGMGR_NOTIFIER_SHORTKEY_PRESSED,    // 3
	CHARGINGMGR_NOTIFIER_LONGKEY_PRESSED,     // 4
	CHARGINGMGR_NOTIFIER_LONGKEY_RELEASED,    // 5

	CHARGINGMGR_NOTIFIER_TA_ATTACHED,         // 6
	CHARGINGMGR_NOTIFIER_TA_DETACHED,         // 7

	CHARGINGMGR_NOTIFIER_USB_ATTACHED,        // 8
	CHARGINGMGR_NOTIFIER_USB_DETACHED,        // 9

	CHARGINGMGR_NOTIFIER_CARKIT_ATTACHED,     // A
	CHARGINGMGR_NOTIFIER_CARKIT_DETACHED,     // B
	
	CHARGINGMGR_NOTIFIER_COMPLETE_CHARGING,   // C

	CHARGINGMGR_NOTIFIER_STOP_BY_TEMP,        // D
	CHARGINGMGR_NOTIFIER_GO_BY_TEMP           // E
} Charging_Notifier_t;
#endif   // FEAT_EN_USE_SAMSUNG_STUB


/**-----------------------------------------------------------------------------
 *
 * SAMSUNG specific type used during AJ86
 *
 *------------------------------------------------------------------------------
 */
#if FEAT_EN_USE_SAMSUNG_STUB
typedef enum
{
  PHK_DEVICE,                // aud dev type 1
  USB_DEVICE,                // usb and usb charger
  CHG_DEVICE,                // travel adapter
  UART_DEVICE,               // regular uart
  ITP_DEVICE,
  JIG_DEVICE,
  JIG_DEVICE_USB_BOOT_ON,    // factory
  JIG_DEVICE_USB_BOOT_OFF,   // factory
  JIG_DEVICE_UART_BOOT_ON,   // factory
  JIG_DEVICE_UART_BOOT_OFF   // factory
} ConnectedDeviceType;
#endif   // FEAT_EN_USE_SAMSUNG_STUB


/*==============================================================================
 *
 *                         M E T H O D S
 *
 *==============================================================================
 */

int get_real_usbic_state(void);
int microusb_enable(void);
void microusb_disable(void);
void mcirousb_usbpath_change(int usb_path);
void mcirousb_uartpath_change(int uart_path);
void microusb_usbjig_detect(void);
void max14577_clear_intr();
void MG_AL25_SetAccessory( MG_AL25_ACCESSORY_T newAcc );
void SAMSUNG_STUB_NotifyNewAccessory( MG_AL25_ACCESSORY_T newAccessory );
void max14577_EnableDisable_AccDet(u8 enable);




/**-----------------------------------------------------------------------------
 *
 * Initializes Glue
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_ModuleInit( void );


/**-----------------------------------------------------------------------------
 *
 * Reads from an I2C HW Device
 *
 * @param   devAddr          in  : I2C device address
 * @param   devReg           in  : I2C device start register for read
 * @param   numBytesToRead   in  : Number of bytes to read
 * @param   pData            out : pointer to data storage for I2C read
 *
 * @return <description of the return value>
 * <delete this line: If there is no return value, don't use @return>
 * todo: return TRUE/FALSE?
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */

void MG_AL25_HW_I2CRead( MCS_U8_T    devAddr,
                         MCS_U8_T    devReg,
                         MCS_U8_T    numBytesToRead,
                         MCS_U8_T   *pData );

/**-----------------------------------------------------------------------------
 *
 * Writes to an I2C HW Device
 *
 * @param   devAddr          in : I2C device address
 * @param   devReg           in : I2C device start register for write
 * @param   numBytesToRead   in : Number of bytes to write
 * @param   pData            in : pointer to data storage for I2C write
 *
 * @return <description of the return value>
 * <delete this line: If there is no return value, don't use @return>
 * todo: return TRUE/FALSE?
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */

void MG_AL25_HW_I2CWrite( MCS_U8_T   devAddr,
                          MCS_U8_T   devReg,        
                          MCS_U16_T  numBytesToWrite,
                          MCS_U8_T  *pData );


/**-----------------------------------------------------------------------------
 *
 * Enables baseband HW ISR associated with AL25, INT pin
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_EnableISR(  void );


/**-----------------------------------------------------------------------------
 *
 * Disables baseband HW ISR associated with AL25, INT pin
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_DisableISR( void );


/**-----------------------------------------------------------------------------
 *
 * Clears baseband HW ISR associated with AL25, INT pin
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_ClearISR(   void );


/**-----------------------------------------------------------------------------
 *
 * Starts a timer for key press handling
 *
 * @param timeoutValue    in : timeout value in milliseconds
 * @param pCallbackFunc   in : pointer to function to call upon timer completion
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_TimerKeyStart( MCS_U32_T             timeoutValue, 
                               MG_AL25_TMR_EXP_F    *pCallbackFunc );


/**-----------------------------------------------------------------------------
 *
 * Stops key press handling timer
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_TimerKeyStop( void );


/**-----------------------------------------------------------------------------
 *
 * Notifies App of new accessory detection based on ADC and ChgTyp
 *
 * @param   newAdc      in : new ADC value
 * @param   newChgTyp   in : new ChgTyp value
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyAcc( MD_AL25_ADC_T    newAdc, 
                            MD_AL25_CHGTYP_T newChgTyp );


/**-----------------------------------------------------------------------------
 *
 * Notifies App of new Audio accessory detection
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyAcc_Audio( void );


/**-----------------------------------------------------------------------------
 *
 * Notifies App of new USB OTG accessory detection
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyAcc_UsbOtg( void );


/**-----------------------------------------------------------------------------
 *
 * Notifies new ADC value during USB OTG mode
 *
 * @param adc   in : New ADC value
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyAcc_UsbOtg_Adc( MD_AL25_ADC_T adc );


/**-----------------------------------------------------------------------------
 *
 * Notifies App of new Audio/Video accessory detection
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyAcc_AV( void );


/**-----------------------------------------------------------------------------
 *
 * Notifies CHG interrupt status change
 *
 * @param state   in : CHG interrupt status
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_Chg( MCS_BOOL_T state );


/**-----------------------------------------------------------------------------
 *
 * Notifies DCD_T interrupt status change
 *
 * @param state   in : DCD_T interrupt status
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_DcdT( MCS_BOOL_T state );


/**-----------------------------------------------------------------------------
 *
 * Notifies ADCERR interrupt status change
 *
 * @param state   in : ADCERR interrupt status
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_AdcError( MCS_BOOL_T state );


// todo
void MG_AL25_App_NotifyINT_DbChg( MCS_BOOL_T state );


// todo
void MG_AL25_App_NotifyINT_VbVolt( MCS_BOOL_T state );


// todo
void MG_AL25_App_NotifyINT_Ovp( MCS_BOOL_T state );


// todo
void MG_AL25_App_NotifyINT_ChgDetRun( MCS_BOOL_T state );


// todo
void MG_AL25_App_NotifyINT_MbcChgErr( MCS_BOOL_T state );


// todo
void MG_AL25_App_NotifyINT_CgMbc( MCS_BOOL_T state );


// todo
void MG_AL25_App_NotifyINT_EOC( MCS_BOOL_T state );


/**-----------------------------------------------------------------------------
 *
 * Notifies Key Press Activity
 *
 * @param curKey     in : key name
 * @param keyPress   in : key activity
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotiftyKey( MG_AL25_KEYEVENT_T curKey, 
                             MG_AL25_KEYPRESS_T keyPress );


// todo
MG_AL25_ACCESSORY_T   MG_AL25_GetAccessory( void );


#ifdef __cplusplus
}
#endif


#endif  /* MG_AL25_SAMSUNG_HDR */

