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



#include <linux/irq.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel_stat.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <plat/gpio.h>
#include <plat/hardware.h>
#include <plat/mux.h>

#include "MD_AL25.h"
#include "MD_MAX14577.h"
#include "common.h"

// extern void (*sec_set_param_value)(int idx, void *value);
// extern void (*sec_get_param_value)(int idx, void *value);

static struct i2c_client *max14577_i2c_client;
struct i2c_driver max14577_i2c_driver;
extern struct device *sio_switch_dev;

#if 0
typedef struct
{
	int  state;
	spinlock_t vib_lock;
}timer_state_t;
timer_state_t timer_usb;
#endif

void enable_charging( bool, bool);
void disable_charging( bool, bool );
int MD_check_full_charge_dur_sleep_(void);
int MD_check_full_charge(void);
int set_uevent_jig(void);

#if defined(CONFIG_MAX14577_MICROUSB_CURRENT_CHANGE)
void change_charging_current( int change_current, bool is_sleep );
#endif

extern void MD_cable_detection_work_handler(void);
extern void MD_full_charge_work_handler(void);
extern s32 MD_normal_i2c_read_byte(u8 devaddr, u8 regoffset, u8 *value);
extern s32 MD_normal_i2c_write_byte(u8 devaddr, u8 regoffset, u8 value);
extern void Send_USB_status_to_UsbMenuSel(int status);
extern int get_usbmenupath_value(void);

static DECLARE_WAIT_QUEUE_HEAD (usb_detect_waitq);
static DECLARE_WAIT_QUEUE_HEAD(usb_disconnect_waitq);

static struct workqueue_struct *max14577_workqueue;
static struct work_struct max14577_work;
struct wake_lock max14577_wake_lock;

static int microusb_usbpath = 0;
static int microusb_uartpath = 0;
static struct timer_list charger_timer;

static u8 eoc_int_count = 0;

//static void init_charger_timer(void);

/**-----------------------------------------------------------------------------
 *
 * Glue code debug message enabling/disabling
 *
 *------------------------------------------------------------------------------
 */
#if defined( MCFG_DEBUG )
	#define   FEAT_EN_TRC_GLUE                          1
	#define   FEAT_EN_TRC_GLUE_NOTIFY_ACC               1
	#define   FEAT_EN_TRC_GLUE_NOTIFY_INT               1
	#define   FEAT_EN_TRC_GLUE_TIMER                    1
	#define   FEAT_EN_TRC_GLUE_I2C                      1
	#define   FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE   1  
	#define   FEAT_EN_TRC_GLUE_SAMSUNG                  1
#else
	#define   FEAT_EN_TRC_GLUE_SAMSUNG                  1
	#define   FEAT_EN_TRC_GLUE_NOTIFY_ACC               1
	#define   FEAT_EN_TRC_GLUE_NOTIFY_INT               1
	#define    MAX14577_DBG_SM_ENABLE		      1
#endif


/**-----------------------------------------------------------------------------
 *
 * Used to manage the device object.
 *
 *------------------------------------------------------------------------------
 */
MD_AL25_INSTANCE_T   MD_AL25_obj;


/**-----------------------------------------------------------------------------
 *
 * Local copy of current I2C Register
 *
 *------------------------------------------------------------------------------
 */
MCS_U8_T   gMD_AL25_I2CReg[ MD_AL25_REG_MAX ];


/*==============================================================================
 *
 *                           L O C A L   M E T H O D S
 *
 *==============================================================================
 */



#define MAX_NOTIFICATION_HANDLER	10
#define USB_STATE_CONNECTED       1
#define USB_STATE_DISCONNECTED    0

typedef void (*notification_handler)(const int);

notification_handler connection_state_change_handler[MAX_NOTIFICATION_HANDLER];

int max14577_add_connection_state_monitor(notification_handler handler)
{
	int index = 0;
	if(handler == NULL)
	{
		printk(KERN_ERR "[FSA9480][%s] param is null\n", __func__);
		return -EINVAL;
	}

	for(; index < MAX_NOTIFICATION_HANDLER; index++)
	{
		if(connection_state_change_handler[index] == NULL)
		{
			connection_state_change_handler[index] = handler;
			return 0;
		}
	}

	// there is no space this time
	printk(KERN_INFO "[FSA9480][%s] No spcae\n", __func__);

	return -ENOMEM;
}

void max14577_remove_connection_state_monitor(notification_handler handler)
{
	int index = 0;
	if(handler == NULL)
	{
		printk(KERN_ERR "[FSA9480][%s] param is null\n", __func__);
		return;
	}
	
	for(; index < MAX_NOTIFICATION_HANDLER; index++)
	{
		if(connection_state_change_handler[index] == handler)
		{
			connection_state_change_handler[index] = NULL;
		}
	}
}
	
static void max14577_notify_connection_state_changed(int state)
{
	int index = 0;
	for(; index < MAX_NOTIFICATION_HANDLER; index++)
	{
		if(connection_state_change_handler[index] != NULL)
		{
			connection_state_change_handler[index](state);
		}
	}
}


/**-----------------------------------------------------------------------------
 *
 * Debug routine to dump I2C registers
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_Dbg_DumpRegs( void )
{
#if 0
#if FEAT_EN_TRC_DBG_REG
	MI_TRC_Msg( 0, "Reg Dump ->" );
	MI_TRC_NumericVarMsg(   0, "DEVICE:  %02X",
			gMD_AL25_I2CReg[ 0x0 ] );
	MI_TRC_3NumericVarsMsg( 0, "INT:     %02X %02X %02X",
			gMD_AL25_I2CReg[ 0x1 ],
			gMD_AL25_I2CReg[ 0x2 ],
			gMD_AL25_I2CReg[ 0x3 ] );
	MI_TRC_3NumericVarsMsg( 0, "INTSTAT: %02X %02X %02X",
			gMD_AL25_I2CReg[ 0x4 ],
			gMD_AL25_I2CReg[ 0x5 ],
			gMD_AL25_I2CReg[ 0x6 ] );
	MI_TRC_3NumericVarsMsg( 0, "INTMASK: %02X %02X %02X",
			gMD_AL25_I2CReg[ 0x7 ],
			gMD_AL25_I2CReg[ 0x8 ],
			gMD_AL25_I2CReg[ 0x9 ] );
	MI_TRC_2NumericVarsMsg( 0, "CHGDET:  %02X %02X",
			gMD_AL25_I2CReg[ 0xA ],
			gMD_AL25_I2CReg[ 0xB ] );
	MI_TRC_3NumericVarsMsg( 0, "CTRL1-3: %02X %02X %02X",
			gMD_AL25_I2CReg[ 0xC ],
			gMD_AL25_I2CReg[ 0xD ],
			gMD_AL25_I2CReg[ 0xE ] );
	MI_TRC_4NumericVarsMsg( 0, "CHG1-4:  %02X %02X %02X %02X",
			gMD_AL25_I2CReg[ 0x0F ],
			gMD_AL25_I2CReg[ 0x10 ],
			gMD_AL25_I2CReg[ 0x11 ],
			gMD_AL25_I2CReg[ 0x12 ] );
	MI_TRC_3NumericVarsMsg( 0, "CHG5-7:  %02X %02X %02X",
			gMD_AL25_I2CReg[ 0x13 ],
			gMD_AL25_I2CReg[ 0x14 ],
			gMD_AL25_I2CReg[ 0x15 ] );
#endif

#if FEAT_EN_SAMSUNG_API
	DEBUG_MAX14577("[max14577] Reg Dump -> \n" );
	DEBUG_MAX14577("[max14577] DEVICE:  %02X \n",
			gMD_AL25_I2CReg[ 0x0 ] );
	DEBUG_MAX14577("[max14577] INT:     %02X %02X %02X \n",
			gMD_AL25_I2CReg[ 0x1 ],
			gMD_AL25_I2CReg[ 0x2 ],
			gMD_AL25_I2CReg[ 0x3 ] );
	DEBUG_MAX14577("[max14577] INTSTAT: %02X %02X %02X \n",
			gMD_AL25_I2CReg[ 0x4 ],
			gMD_AL25_I2CReg[ 0x5 ],
			gMD_AL25_I2CReg[ 0x6 ] );
	DEBUG_MAX14577("[max14577] INTMASK: %02X %02X %02X \n",
			gMD_AL25_I2CReg[ 0x7 ],
			gMD_AL25_I2CReg[ 0x8 ],
			gMD_AL25_I2CReg[ 0x9 ] );
	DEBUG_MAX14577("[max14577] CHGDET:  %02X %02X \n",
			gMD_AL25_I2CReg[ 0xA ],
			gMD_AL25_I2CReg[ 0xB ] );
	DEBUG_MAX14577("[max14577] CTRL1-3: %02X %02X %02X \n",
			gMD_AL25_I2CReg[ 0xC ],
			gMD_AL25_I2CReg[ 0xD ],
			gMD_AL25_I2CReg[ 0xE ] );
	DEBUG_MAX14577("[max14577] CHG1-4:  %02X %02X %02X %02X \n",
			gMD_AL25_I2CReg[ 0x0F ],
			gMD_AL25_I2CReg[ 0x10 ],
			gMD_AL25_I2CReg[ 0x11 ],
			gMD_AL25_I2CReg[ 0x12 ] );
	DEBUG_MAX14577("[max14577] CHG5-7:  %02X %02X %02X \n",
			gMD_AL25_I2CReg[ 0x13 ],
			gMD_AL25_I2CReg[ 0x14 ],
			gMD_AL25_I2CReg[ 0x15 ] );

#endif
#else
	int i;
	DEBUG_MAX14577("[max14577] Dump Regs ");
	for (i=0 ; i<6 ; i++)	{
		DEBUG_MAX14577("[%2d:%02x]", i, gMD_AL25_I2CReg[i]);
	}
	DEBUG_MAX14577("\n           Dump Regs ");
	for (i=6 ; i<15 ; i++)	{
		DEBUG_MAX14577("[%2d:%02x]", i, gMD_AL25_I2CReg[i]);
	}
	DEBUG_MAX14577("\n");

#endif
}


/**-----------------------------------------------------------------------------
 *
 * DEBUG ONLY Function
 *   Used to compare resistors in device with local copy in global memory
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_Dbg_CompareRegs( void )
{
	//
	// todo
	// 
}


/**-----------------------------------------------------------------------------
 *
 * Handler for ADC and/or ChargeType interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_Adc_ChgTyp( void )
{
#ifdef MAX14577_DBG_ENABLE
	DEBUG_MAX14577("[max14577] MD_AL25_IntHandler_Adc_ChgTyp: ENTER \n" );
#endif

	//
	// Check AdcLow is HI
	// 
	if ( MD_AL25_obj.intStat.statAdcLow )
	{
		//
		// Notify App
		// 
		MG_AL25_App_NotifyAcc( MD_AL25_obj.intStat.statAdc, MD_AL25_obj.intStat.statChargerType );
	}
	else if ( MD_AL25_obj.intStat.statAdc == MD_AL25_ADC_GND_ADCLOW )
	{
		MD_AL25_obj.intStat.statAdc = MD_AL25_ADC_GND;

		//
		// Notify App
		// 
		MG_AL25_App_NotifyAcc( MD_AL25_obj.intStat.statAdc, MD_AL25_obj.intStat.statChargerType );
	}
	else
	{
		//
		// todo: Error
		//
#ifdef MAX14577_DBG_ENABLE
		DEBUG_MAX14577("[max14577] ERROR: MD_AL25_IntHandler_Adc_ChgTyp: AdcLow Lo but ADC not 0 \n");
#endif
	}
}


/**-----------------------------------------------------------------------------
 *
 * Handler for Dead Battery Charge (DBCHG) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_DbChg( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_DbChg( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for Data Contact Detect Timeout (DCD_T) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_DcdT( MCS_BOOL_T state )
{
	//
	// If DCD_T interrupt set, set DCD_EN to disable to clear DCD_T interrupt.
	//   Immediately re-enable DCD_EN
	// 
	if ( state == MCS_TRUE )
	{
		//
		// Clear DCD_EN bit
		// 
		gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] &= ~MD_AL25_M_CDETCTRL_DCDEN;

		MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
				MD_AL25_REG_CDETCTRL,  
				1,                     
				&gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] ); 

		//
		// Set DCD_EN bit
		// 
		gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] |= MD_AL25_REG_CDETCTRL_DCDEN_ENABLE;

		MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
				MD_AL25_REG_CDETCTRL,  
				1,                     
				&gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] ); 
	}

	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_DcdT( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for ADC Error (ADCERR) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_AdcErr( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_AdcError( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for Vb Voltage (VBVOLT) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_VbVolt( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_VbVolt( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for Over Voltage Protection (OVP) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_Ovp( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_Ovp( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for Charge Detection Running (CHGDETRUN) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_ChgDetRun( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_ChgDetRun( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for MbcChgErr (Battery Fast Charging Timer Expire) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_MbcChgErr( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_MbcChgErr( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for CgMbc (Charger Power OK) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_CgMbc( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
		
	MG_AL25_App_NotifyINT_CgMbc( state );
}


/**-----------------------------------------------------------------------------
 *
 * Handler for End of Charge (EOC) interrupt
 *
 * @param state   in : new state of interrupt
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_IntHandler_EOC( MCS_BOOL_T state )
{
	//
	// Notify App
	// 
	MG_AL25_App_NotifyINT_EOC( state );
}


/**-----------------------------------------------------------------------------
 *
 * Sets the CHG_TYP_M bit
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_CHG_TYP_M_Set( void )
{
	//
	// todo
	// 
}


/**-----------------------------------------------------------------------------
 *
 * Read AL25 Interrupt Status registers and returns parameter values
 *
 * NOTE: this will clear AL25 Interrupt
 *
 * @param    pInterrupt   out : structure of interrupt register params
 *
 * @return   TRUE  : Successful
 *           FALSE : Failure
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
MCS_BOOL_T MD_AL25_ReadStatus( MD_AL25_INT_T       *pInt,
                               MD_AL25_INTSTAT_T   *pIntStat )
{
	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_INT1,
			6,
			&gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ] );

#if FEAT_EN_TRC_I2C_TRAFFIC
	MI_TRC_3NumericVarsMsg( 0, 
			"MD_AL25_ReadStatus: Regs 0x%02x 0x%02x 0x%02x",
			gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ],
			gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ],
			gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ] );
#endif

#ifdef MAX14577_DBG_ENABLE
	DEBUG_MAX14577("[max14577] MD_AL25_ReadStatus: Regs 0x%02x 0x%02x  0x%02x \n",
			gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ],
			gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ],
			gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ] );
#endif


	//
	// Interrupt Reg 1
	// 
	pInt->intAdc = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ] & MD_AL25_M_INT1_ADC    ) >> MD_AL25_B_INT1_ADC    );
	pInt->intAdcLow = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ] & MD_AL25_M_INT1_ADCLOW ) >> MD_AL25_B_INT1_ADCLOW );
	pInt->intAdcError = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ] & MD_AL25_M_INT1_ADCERR ) >> MD_AL25_B_INT1_ADCERR );

	//
	// Interrupt Reg 2
	// 
	pInt->intChargerType = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ] & MD_AL25_M_INT2_CHGTYP    ) >> MD_AL25_B_INT2_CHGTYP    );
	pInt->intChargerDetectRun = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ] & MD_AL25_M_INT2_CHGDETRUN ) >> MD_AL25_B_INT2_CHGDETRUN );
	pInt->intDataContactDetectTimeout = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ] & MD_AL25_M_INT2_DCDTMR    ) >> MD_AL25_B_INT2_DCDTMR    );
	pInt->intDeadBatteryChargeMode = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ] & MD_AL25_M_INT2_DBCHG     ) >> MD_AL25_B_INT2_DBCHG     );
	pInt->intVbVoltage = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ] & MD_AL25_M_INT2_VBVOLT    ) >> MD_AL25_B_INT2_VBVOLT    );

	//
	// Interrupt Reg 3
	// 
	pInt->intMbcChgErr = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ] & MD_AL25_M_INT3_MBCCHGERR ) >> MD_AL25_B_INT3_MBCCHGERR );
	pInt->intVbOverVoltageProtection = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ] & MD_AL25_M_INT3_OVP       ) >> MD_AL25_B_INT3_OVP       );
	pInt->intCgMbc = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ] & MD_AL25_M_INT3_CGMBC     ) >> MD_AL25_B_INT3_CGMBC     );
	pInt->intEndOfCharge = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ] & MD_AL25_M_INT3_EOC       ) >> MD_AL25_B_INT3_EOC       );

	//
	// Interrupt Status Reg 1
	// 
	pIntStat->statAdc = ( MD_AL25_ADC_T )
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT1 ] & MD_AL25_M_INTSTAT1_ADC    ) >> MD_AL25_B_INTSTAT1_ADC    );
	pIntStat->statAdcLow = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT1 ] & MD_AL25_M_INTSTAT1_ADCLOW ) >> MD_AL25_B_INTSTAT1_ADCLOW );
	pIntStat->statAdcError = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT1 ] & MD_AL25_M_INTSTAT1_ADCERR ) >> MD_AL25_B_INTSTAT1_ADCERR );

	//
	// Interrupt Status Reg 2
	// 
	pIntStat->statChargerType = ( MD_AL25_CHGTYP_T )
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT2 ] & MD_AL25_M_INTSTAT2_CHGTYP    ) >> MD_AL25_B_INTSTAT2_CHGTYP    );
	pIntStat->statChargerDetectRun = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT2 ] & MD_AL25_M_INTSTAT2_CHGDETRUN ) >> MD_AL25_B_INTSTAT2_CHGDETRUN );
	pIntStat->statDataContactDetectTimeout = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT2 ] & MD_AL25_M_INTSTAT2_DCDTMR    ) >> MD_AL25_B_INTSTAT2_DCDTMR    );
	pIntStat->statDeadBatteryChargeMode = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT2 ] & MD_AL25_M_INTSTAT2_DBCHG     ) >> MD_AL25_B_INTSTAT2_DBCHG     );
	pIntStat->statVbVoltage = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT2 ] & MD_AL25_M_INTSTAT2_VBVOLT    ) >> MD_AL25_B_INTSTAT2_VBVOLT    );

	//
	// Interrupt Status Reg 3
	// 
	pIntStat->statMbcChgErr = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] & MD_AL25_M_INTSTAT3_MBCCHGERR ) >> MD_AL25_B_INTSTAT3_MBCCHGERR );
	pIntStat->statVbOverVoltageProtection = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] & MD_AL25_M_INTSTAT3_OVP       ) >> MD_AL25_B_INTSTAT3_OVP       );
	pIntStat->statCgMbc = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] & MD_AL25_M_INTSTAT3_CGMBC     ) >> MD_AL25_B_INTSTAT3_CGMBC     );
	pIntStat->statEndOfCharge = 
			(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] & MD_AL25_M_INTSTAT3_EOC       ) >> MD_AL25_B_INTSTAT3_EOC       );


	return( MCS_TRUE );
}


/*==============================================================================
 *
 *                         E X T E R N A L   M E T H O D S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_ModuleInit( MD_AL25_USERCFG_T *userCfgData )
{
	MCS_U32_T   index = 0;

	//
	// Setup initial parameters of the AL25 object
	// 
	MD_AL25_obj.forceChgTypM   =   MCS_FALSE;

	//
	// Setup user config data to default of user choices
	// 
	if ( userCfgData == NULL )
	{
		MD_AL25_obj.userCfg.rcps          = MD_AL25_RCPS_DISABLE;
		MD_AL25_obj.userCfg.usbCplnt      = MD_AL25_USBCPLNT_DISABLE;
		MD_AL25_obj.userCfg.sfOutOrd      = MD_AL25_SFOUTORD_NORMAL;
		MD_AL25_obj.userCfg.sfOutAsrt     = MD_AL25_SFOUTASRT_NORMAL;
		MD_AL25_obj.userCfg.lowPwr        = MD_AL25_LOWPWR_DISABLE;

		MD_AL25_obj.userCfg.dchk          = MD_AL25_DCHK_50MS;
		MD_AL25_obj.userCfg.usbOtgCapable = MCS_TRUE;
	}
	else
	{
		MD_AL25_obj.userCfg = *userCfgData;
	}

	//
	// Set global I2C regs to zero
	// 
	for ( index = MD_AL25_REG_MIN ; index < MD_AL25_REG_MAX ; index++ )
	{
		gMD_AL25_I2CReg[ index ] = 0x00;
	}

	//set EOC count value to 0
	eoc_int_count = 0;
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */

void MD_AL25_ModuleOpen( void )
{
	//
	// Read Data Registers BUT leave interrupt registers alone!
	//   They will be read during ServiceStateMachine
	// 

	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_DEVICEID,
			1,
			&( gMD_AL25_I2CReg[ MD_AL25_REG_DEVICEID ] ));


	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_INTMASK1,
			( MD_AL25_REG_MAX - MD_AL25_REG_INTMASK1 ),
			&( gMD_AL25_I2CReg[ MD_AL25_REG_INTMASK1 ] ));

	MD_AL25_Dbg_DumpRegs();

	// 
	// Setup Default register values
	//
	gMD_AL25_I2CReg[ MD_AL25_REG_INTMASK1 ] =
	(
		MD_AL25_REG_INTMASK1_ADCERR_ENABLE   |
		MD_AL25_REG_INTMASK1_ADCLOW_ENABLE   |
		MD_AL25_REG_INTMASK1_ADC_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_INTMASK2 ] =
	(
		MD_AL25_REG_INTMASK2_VBVOLT_ENABLE       |
		MD_AL25_REG_INTMASK2_DBCHG_ENABLE        |
		MD_AL25_REG_INTMASK2_DCDTMR_ENABLE       |
		MD_AL25_REG_INTMASK2_CHGDETRUN_DISABLE   |
		MD_AL25_REG_INTMASK2_CHGTYP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_INTMASK3 ] =
	(
		MD_AL25_REG_INTMASK3_MBCCHGERR_ENABLE   |
		MD_AL25_REG_INTMASK3_OVP_ENABLE         |
		MD_AL25_REG_INTMASK3_CGMBC_ENABLE       |
		MD_AL25_REG_INTMASK3_EOC_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] =
	(
		MD_AL25_REG_CDETCTRL_CDPDET_VDP_SRC    |
		MD_AL25_REG_CDETCTRL_DBEXIT_DISABLE    |
		MD_AL25_obj.userCfg.dchk               |
		MD_AL25_REG_CDETCTRL_DCD2SCT_EXIT      |
		MD_AL25_REG_CDETCTRL_DCDEN_ENABLE      |
		MD_AL25_REG_CDETCTRL_CHGTYPM_DISABLE   |
		MD_AL25_REG_CDETCTRL_CHGDETEN_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =
	(
		MD_AL25_REG_CTRL1_IDBEN_OPEN    |
		MD_AL25_REG_CTRL1_MICEN_OPEN    |
		MD_AL25_REG_CTRL1_COMP2SW_HIZ   |
		MD_AL25_REG_CTRL1_COMN1SW_HIZ
	);

	printk("[RANJIT] lowpwer mode disabled \n");
	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =
	(
		MD_AL25_obj.userCfg.rcps             |
		MD_AL25_obj.userCfg.usbCplnt         |
		MD_AL25_REG_CTRL2_ACCDET_ENABLE|
		MD_AL25_REG_CTRL2_SFOUTORD_NORMAL    |
		MD_AL25_REG_CTRL2_SFOUTASRT_NORMAL   |
		MD_AL25_REG_CTRL2_CPEN_DISABLE       |
		MD_AL25_REG_CTRL2_ADCEN_ENABLE       |
		MD_AL25_obj.userCfg.lowPwr
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] =
	(
		MD_AL25_REG_CTRL3_WBTH_3P7V        |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		MD_AL25_REG_CTRL3_BOOTSET_AUTO     |
		MD_AL25_REG_CTRL3_JIGSET_AUTO
	);

	// 
	// todo
	//   Defaults for reg ChgCtrl 1-7
	// 
	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_INTMASK1,
			( MD_AL25_REG_MAX - MD_AL25_REG_INTMASK1 ),
			&(gMD_AL25_I2CReg[ MD_AL25_REG_INTMASK1 ]) );
	//MD_AL25_Dbg_DumpRegs();

	//
	// Setup all initial conidtions
	// 
	{
		MD_AL25_INT_T       newInt;

		MD_AL25_ReadStatus( &newInt, &MD_AL25_obj.intStat );

		MD_AL25_IntHandler_Adc_ChgTyp();
		MD_AL25_IntHandler_DbChg(     MD_AL25_obj.intStat.statDeadBatteryChargeMode    );
		MD_AL25_IntHandler_DcdT(      MD_AL25_obj.intStat.statDataContactDetectTimeout );
		MD_AL25_IntHandler_AdcErr(    MD_AL25_obj.intStat.statAdcError                 );
		MD_AL25_IntHandler_VbVolt(    MD_AL25_obj.intStat.statVbVoltage                );
		MD_AL25_IntHandler_Ovp(       MD_AL25_obj.intStat.statVbOverVoltageProtection  );
		MD_AL25_IntHandler_ChgDetRun( MD_AL25_obj.intStat.statChargerDetectRun         );
		MD_AL25_IntHandler_MbcChgErr( MD_AL25_obj.intStat.statMbcChgErr                );
		MD_AL25_IntHandler_CgMbc(     MD_AL25_obj.intStat.statCgMbc                    );
		MD_AL25_IntHandler_EOC(       MD_AL25_obj.intStat.statEndOfCharge              );
	}
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_ModuleClose( void )
{
	//
	// Todo
	//   Set AccDet to 1
	//   Set DBExit to 1
	// 
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_ServiceStateMachine( void )
{
	MD_AL25_INT_T       newInt;
	MD_AL25_INTSTAT_T   newIntStat;


#ifdef MAX14577_DBG_SM_ENABLE
	DEBUG_MAX14577("[max14577] ServiceStateMachine Enter \n");
#endif


	MD_AL25_ReadStatus( &newInt, &newIntStat );

	MD_AL25_Dbg_DumpRegs();

	//
	// Causes for running new accessory detection FW algorithm are
	//   - change in ADC
	//   - change in ChgTyp
	//   - force HW state machine with ChgTypM
	//   - adc Debounce Timer expiration
	// 
	if ( ( newInt.intAdc ) || ( newInt.intChargerType ) || ( MD_AL25_obj.forceChgTypM  == MCS_TRUE ) )
	{
		//
		// Save new interrupt status
		// 
		MD_AL25_obj.intStat.statAdc         = newIntStat.statAdc;
		MD_AL25_obj.intStat.statAdcLow      = newIntStat.statAdcLow;
		MD_AL25_obj.intStat.statChargerType = newIntStat.statChargerType;
		MD_AL25_obj.forceChgTypM            = MCS_FALSE;

#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577( "[max14577] AL25_SM: run AccId (ADC %d %d, ChgTyp %d), fChgTyp %d \n", 
		newIntStat.statAdc,
		newIntStat.statAdcLow,
		newIntStat.statChargerType,
		MD_AL25_obj.forceChgTypM );
#endif

		MD_AL25_IntHandler_Adc_ChgTyp();

	}
	
	//
	// DBCHG Interrupt
	// 
	if ( newInt.intDeadBatteryChargeMode )
	{  
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newDBCHG stat %d  \n", newIntStat.statDeadBatteryChargeMode );
#endif

		MD_AL25_obj.intStat.statDeadBatteryChargeMode = newIntStat.statDeadBatteryChargeMode;

		MD_AL25_IntHandler_DbChg( MD_AL25_obj.intStat.statDeadBatteryChargeMode );
	}

	//
	// DCD_T Interrupt
	// 
	if ( newInt.intDataContactDetectTimeout )
		{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newDCD_T stat %d \n", newIntStat.statDataContactDetectTimeout );
#endif

		MD_AL25_obj.intStat.statDataContactDetectTimeout = newIntStat.statDataContactDetectTimeout;

		MD_AL25_IntHandler_DcdT( MD_AL25_obj.intStat.statDataContactDetectTimeout );
	}

	//
	// ADC Error Interrupt
	// 
	if ( newInt.intAdcError )
	{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newAdcErr stat %d \n", newIntStat.statAdcError );
#endif

		MD_AL25_obj.intStat.statAdcError = newIntStat.statAdcError;

		MD_AL25_IntHandler_AdcErr( MD_AL25_obj.intStat.statAdcError );
	}

	//
	// Vb Voltage Interrupt
	// 
	if ( newInt.intVbVoltage )
	{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newVbVolt stat %d \n", newIntStat.statVbVoltage );
#endif

		MD_AL25_obj.intStat.statVbVoltage = newIntStat.statVbVoltage;

		MD_AL25_IntHandler_VbVolt( MD_AL25_obj.intStat.statVbVoltage );
	}

	//
	// Vb Over Voltage Protection Interrupt
	// 
	if ( newInt.intVbOverVoltageProtection )
	{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newOvp stat %d \n", newIntStat.statVbOverVoltageProtection );
#endif

		MD_AL25_obj.intStat.statVbOverVoltageProtection = newIntStat.statVbOverVoltageProtection;

		MD_AL25_IntHandler_Ovp( MD_AL25_obj.intStat.statVbOverVoltageProtection );
	}

	//
	// Charger Detection Running Interrupt
	// 
	if ( newInt.intChargerDetectRun )
	{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newChgDetRun stat %d \n", newIntStat.statChargerDetectRun );
#endif

		MD_AL25_obj.intStat.statChargerDetectRun = newIntStat.statChargerDetectRun;

		MD_AL25_IntHandler_ChgDetRun( MD_AL25_obj.intStat.statChargerDetectRun );
	}


	//
	// MbcChgErr Interrupt (Battery Fast Charge Timer Expire)
	// 
	if ( newInt.intMbcChgErr )
	{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newMbcChgErr stat %d \n", newIntStat.statMbcChgErr );
#endif

		MD_AL25_obj.intStat.statMbcChgErr = newIntStat.statMbcChgErr;

		MD_AL25_IntHandler_MbcChgErr( MD_AL25_obj.intStat.statMbcChgErr );
	}


	//
	// CgMbc Interrupt (Charger Voltage OK)
	// 
	if ( newInt.intCgMbc )
	{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newCgMbc stat %d \n", newIntStat.statCgMbc );
#endif

		MD_AL25_obj.intStat.statCgMbc = newIntStat.statCgMbc;

		MD_AL25_IntHandler_CgMbc( MD_AL25_obj.intStat.statCgMbc );
	}


	//
	// End Of Charge (EOC) Interrupt
	// 
	if ( newInt.intEndOfCharge )
	{
#ifdef MAX14577_DBG_SM_ENABLE
		DEBUG_MAX14577("[max14577] AL25_SM: newEOC stat %d \n", newIntStat.statEndOfCharge );
#endif

		MD_AL25_obj.intStat.statEndOfCharge = newIntStat.statEndOfCharge;

		MD_AL25_IntHandler_EOC( MD_AL25_obj.intStat.statEndOfCharge );
	}


	MD_AL25_Dbg_CompareRegs();

#ifdef MAX14577_DBG_SM_ENABLE
	DEBUG_MAX14577("[max14577] AL25_SM: Exit \n" );
#endif

	if (gpio_get_value(OMAP_GPIO_USBSW_NINT) == 0)	{

//		MG_AL25_HW_I2CRead(MD_AL25_IIC_ADDRESS, MD_AL25_REG_INT3, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ]);
//		mdelay(10);
		
		MAX14577_ELOG("@\n@[max14577] OMAP_GPIO_USBSW_NINT is LOW! [%d]\n@\n" ,gpio_get_value(OMAP_GPIO_USBSW_NINT));

/*		
		gpio_set_value(OMAP_GPIO_USBSW_NINT, 1);
		mdelay(10);
*/
		
	}
	else	{
		MAX14577_ELOG("[max14577] OMAP_GPIO_USBSW_NINT is HIGH! [%d]\n", gpio_get_value(OMAP_GPIO_USBSW_NINT));
	}
	
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_None( void )
{
#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_None: ENTER" );
#endif

	//
	// CTRL1
	//   IdbEn OPEN
	//   MicEn OPEN
	//   Switches OPEN
	// 
	// CTRL2
	//   RCPS userCfg
	//   UsbCplnt userCfg
	//   AccDet 0
	//   SfOutOrd userCfg
	//   SfOutAsrt userCfg
	//   CpEn disable
	//   AdcEn enable
	//   LowPwr usercfg
	// 
	// CTRL3
	//   default
	// 
	// CHGCTRL1
	//   TCHW 5hr
	// 
	// CHGCTRL2
	//   VCHGR_RC disable
	//   MBHOSTEN disable
	// 
	// CHGCTRL3
	//   MBCCVWRC 4.2V
	// 
	// CHGCTRL4
	//   MBCICHWRCL 1
	//   MBCICHWRCH 400mA
	// 
	// CHGCTRL5
	//   EOCS 60mA
	// 
	// CHGCTRL6
	//   AutoStop disable
	// 
	// CHGCTRL7
	//   OTPCGHCVS 7.5V
	// 

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =
	(
		MD_AL25_REG_CTRL1_IDBEN_OPEN    |
		MD_AL25_REG_CTRL1_MICEN_OPEN    |
		MD_AL25_REG_CTRL1_COMP2SW_HIZ   |
		MD_AL25_REG_CTRL1_COMN1SW_HIZ
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =
	(
		MD_AL25_obj.userCfg.rcps           |
		MD_AL25_obj.userCfg.usbCplnt       |
		MD_AL25_REG_CTRL2_ACCDET_ENABLE   |
		MD_AL25_obj.userCfg.sfOutOrd       |
		MD_AL25_obj.userCfg.sfOutAsrt      |
		MD_AL25_REG_CTRL2_CPEN_DISABLE     |
		MD_AL25_REG_CTRL2_ADCEN_ENABLE     |
		MD_AL25_obj.userCfg.lowPwr
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] =
	(
		MD_AL25_REG_CTRL3_WBTH_3P7V        |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		MD_AL25_REG_CTRL3_BOOTSET_AUTO     |
		MD_AL25_REG_CTRL3_JIGSET_AUTO
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] =
	(
		MD_AL25_REG_CHGCTRL1_TCHW_DISABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL3 ] =
	(
		MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
	(
		MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI      |
		MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL5 ] =
	(
		MD_AL25_REG_CHGCTRL5_EOCS_60MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL6 ] =
	(
		MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL7 ] =
	(
		MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V
	);

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL1,
			10,                     
			&( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] ));
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_DedChgr( void )
{
#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_DedChgr: ENTER" );
#endif

	//
	// CTRL1
	//   IdbEn OPEN
	//   MicEn OPEN
	//   Switches OPEN
	// 
	// CTRL2
	//   RCPS userCfg
	//   UsbCplnt userCfg
	//   AccDet 0
	//   SfOutOrd userCfg
	//   SfOutAsrt userCfg
	//   CpEn disable
	//   AdcEn enable
	//   LowPwr usercfg
	// 
	// CTRL3
	//   default
	// 
	// CHGCTRL1
	//   TCHW 5hr          (usrCfg? todo)
	// 
	// CHGCTRL2
	//   VCHGR_RC enable
	//   MBHOSTEN enable
	// 
	// CHGCTRL3
	//   MBCCVWRC 4.2V     (usrCfg? todo)
	// 
	// CHGCTRL4
	//   MBCICHWRCL 1      (usrCfg? todo)
	//   MBCICHWRCH 600mA  (usrCfg? todo)
	// 
	// CHGCTRL5
	//   EOCS 110mA         (usrCfg? todo)
	// 
	// CHGCTRL6
	//   AutoStop disable  (usrCfg? todo)
	// 
	// CHGCTRL7
	//   OTPCGHCVS 7.5V    (usrCfg? todo)
	// 

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =
	(
		MD_AL25_REG_CTRL1_IDBEN_OPEN    |
		MD_AL25_REG_CTRL1_MICEN_OPEN    |
		MD_AL25_REG_CTRL1_COMP2SW_HIZ   |
		MD_AL25_REG_CTRL1_COMN1SW_HIZ
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =
	(
		MD_AL25_obj.userCfg.rcps           |
		MD_AL25_obj.userCfg.usbCplnt       |
		MD_AL25_REG_CTRL2_ACCDET_ENABLE   |
		MD_AL25_obj.userCfg.sfOutOrd       |
		MD_AL25_obj.userCfg.sfOutAsrt      |
		MD_AL25_REG_CTRL2_CPEN_DISABLE     |
		MD_AL25_REG_CTRL2_ADCEN_ENABLE     |
		MD_AL25_obj.userCfg.lowPwr
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] =
	(
		MD_AL25_REG_CTRL3_WBTH_3P7V        |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		MD_AL25_REG_CTRL3_BOOTSET_AUTO     |
		MD_AL25_REG_CTRL3_JIGSET_AUTO
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] =
	(
		MD_AL25_REG_CHGCTRL1_TCHW_DISABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL3 ] =
	(
		MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
	(
		MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI      |
		MD_AL25_REG_CHGCTRL4_MBCICHWRCH_600MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL5 ] =
	(
		MD_AL25_REG_CHGCTRL5_EOCS_90MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL6 ] =
	(
		MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL7 ] =
	(
		MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V
	);

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL1,
			10,                     
			&( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] ));
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_Usb( void )
{
	u8 switch_sel;
#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_Usb: ENTER" );
#endif

	switch_sel = get_usbmenupath_value();

  //
  // CTRL1
  //   IdbEn OPEN
  //   MicEn OPEN
  //   Switches Usb
  // 
  // CTRL2
  //   RCPS userCfg
  //   UsbCplnt userCfg
  //   AccDet 0
  //   SfOutOrd userCfg
  //   SfOutAsrt userCfg
  //   CpEn enable
  //   AdcEn enable
  //   LowPwr userCfg
  // 
  // CTRL3
  //   default
  // 
  // CHGCTRL1
  //   TCHW 5hr          (usrCfg? todo)
  // 
  // CHGCTRL2
  //   if not USB Compliant
  //     VCHGR_RC enable
  //     MBHOSTEN enable
  //   else
  //     VCHGR_RC disable
  //     MBHOSTEN disable
  // 
  // CHGCTRL3
  //   MBCCVWRC 4.2V     (usrCfg? todo)
  // 
  // CHGCTRL4
  //   MBCICHWRCL 1      (usrCfg? todo)
  //   MBCICHWRCH 450mA  (usrCfg? todo)
  // 
  // CHGCTRL5
  //   EOCS 110mA         (usrCfg? todo)
  // 
  // CHGCTRL6
  //   AutoStop disable  (usrCfg? todo)
  // 
  // CHGCTRL7
  //   OTPCGHCVS 7.5V    (usrCfg? todo)
  // 

	if ((switch_sel & 0x01) == 0x01)	{
		mcirousb_usbpath_change(0);	// Switch USB to AP
	}
	else if ((switch_sel & 0x01) == 0x00)	{
		mcirousb_usbpath_change(1);	// Switch USB to CP
	}
	else	{
		printk("[MAX14577] Can't switch USB to AP neither CP\n");
	}



	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] =		(
		MD_AL25_REG_CHGCTRL1_TCHW_DISABLE
	);

	//
	// If USB Compliant is disabled, 
	//   user wants immediate charging from USB cables
	// 
	// Otherwise, charging is disabled until user enumerates
	//   and asks permission from host to charge
	// 
	if ( MD_AL25_obj.userCfg.usbCplnt == MD_AL25_USBCPLNT_DISABLE )
	{
		gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =		(
			MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
			MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
		);
	}
	else
	{
		gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] = (
			MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
			MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
		);
	}

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL3 ] =	(
		MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =		(
		MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI      |
		MD_AL25_REG_CHGCTRL4_MBCICHWRCH_450MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL5 ] =		(
		MD_AL25_REG_CHGCTRL5_EOCS_90MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL6 ] =		(
		MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL7 ] =		(
		MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V
	);

//	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,	MD_AL25_REG_CHGCTRL1, 7, &( gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] ));

}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_DwnStrmChgPort( void )
{
	// 
	// todo
	//   Same as USB?
	// 

#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_DwnStrmChgPort: ENTER" );
#endif

	MD_AL25_AccCfg_Usb();
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_Uart( void )
{
#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_Uart: ENTER" );
#endif

	//
	// CTRL1
	//   IdbEn OPEN
	//   MicEn OPEN
	//   Switches Uart
	// 
	// CTRL2
	//   RCPS userCfg
	//   UsbCplnt userCfg
	//   AccDet 0
	//   SfOutOrd userCfg
	//   SfOutAsrt userCfg
	//   CpEn enable
	//   AdcEn enable
	//   LowPwr userCfg
	// 
	// CTRL3
	//   default
	// 
	// CHGCTRL1
	//   TCHW 5hr
	// 
	// CHGCTRL2
	//   VCHGR_RC disable
	//   MBHOSTEN disable
	// 
	// CHGCTRL3
	//   MBCCVWRC 4.2V
	// 
	// CHGCTRL4
	//   MBCICHWRCL 1
	//   MBCICHWRCH 400mA
	// 
	// CHGCTRL5
	//   EOCS 60mA
	// 
	// CHGCTRL6
	//   AutoStop disable
	// 
	// CHGCTRL7
	//   OTPCGHCVS 7.5V
	// 

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =
	(
		MD_AL25_REG_CTRL1_IDBEN_OPEN    |
		MD_AL25_REG_CTRL1_MICEN_OPEN    |
		MD_AL25_REG_CTRL1_COMP2SW_UT2   |
		MD_AL25_REG_CTRL1_COMN1SW_UT1
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =
	(
		MD_AL25_obj.userCfg.rcps           |
		MD_AL25_obj.userCfg.usbCplnt       |
		MD_AL25_REG_CTRL2_ACCDET_ENABLE   |
		MD_AL25_obj.userCfg.sfOutOrd       |
		MD_AL25_obj.userCfg.sfOutAsrt      |
		MD_AL25_REG_CTRL2_CPEN_ENABLE      |
		MD_AL25_REG_CTRL2_ADCEN_ENABLE     |
		MD_AL25_obj.userCfg.lowPwr
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] =
	(
		MD_AL25_REG_CTRL3_WBTH_3P7V        |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		MD_AL25_REG_CTRL3_BOOTSET_AUTO     |
		MD_AL25_REG_CTRL3_JIGSET_AUTO
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] =
	(
		MD_AL25_REG_CHGCTRL1_TCHW_DISABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL3 ] =
	(
		MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
	(
		MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI      |
		MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL5 ] =
	(
		MD_AL25_REG_CHGCTRL5_EOCS_60MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL6 ] =
	(
		MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL7 ] =
	(
		MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V
	);

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL1,
			10,
			&( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] ));
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_Audio( void )
{
#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_Audio: ENTER" );
#endif

	//
	// CTRL1
	//   IdbEn OPEN
	//   MicEn Connected
	//   Switches Audio
	// 
	// CTRL2
	//   RCPS disable
	//   UsbCplnt userCfg
	//   AccDet 0
	//   SfOutOrd userCfg
	//   SfOutAsrt userCfg
	//   CpEn enable
	//   AdcEn enable
	//   LowPwr userCfg
	//
	// CTRL3
	//   default
	// 
	// CHGCTRL1
	//   TCHW 5hr
	// 
	// CHGCTRL2
	//   VCHGR_RC disable
	//   MBHOSTEN disable
	// 
	// CHGCTRL3
	//   MBCCVWRC 4.2V
	// 
	// CHGCTRL4
	//   MBCICHWRCL 1
	//   MBCICHWRCH 400mA
	// 
	// CHGCTRL5
	//   EOCS 60mA
	// 
	// CHGCTRL6
	//   AutoStop disable
	// 
	// CHGCTRL7
	//   OTPCGHCVS 7.5V
	// 

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =
	(
		MD_AL25_REG_CTRL1_IDBEN_OPEN    |
		MD_AL25_REG_CTRL1_MICEN_CONN    |
		MD_AL25_REG_CTRL1_COMP2SW_SR2   |
		MD_AL25_REG_CTRL1_COMN1SW_SL1
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =
	(
		MD_AL25_REG_CTRL2_RCPS_DISABLE     |
		MD_AL25_obj.userCfg.usbCplnt       |
		MD_AL25_REG_CTRL2_ACCDET_ENABLE   |

		MD_AL25_obj.userCfg.sfOutOrd       |
		MD_AL25_obj.userCfg.sfOutAsrt      |
		MD_AL25_REG_CTRL2_CPEN_ENABLE      |
		MD_AL25_REG_CTRL2_ADCEN_ENABLE     |
		MD_AL25_obj.userCfg.lowPwr
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] =
	(
		MD_AL25_REG_CTRL3_WBTH_3P7V        |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		MD_AL25_REG_CTRL3_BOOTSET_AUTO     |
		MD_AL25_REG_CTRL3_JIGSET_AUTO
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] =
	(
		MD_AL25_REG_CHGCTRL1_TCHW_DISABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL3 ] =
	(
		MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
	(
		MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI      |
		MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL5 ] =
	(
		MD_AL25_REG_CHGCTRL5_EOCS_60MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL6 ] =
	(
		MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL7 ] =
	(
		MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V
	);

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL1,
			10,
			&( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] ));
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_TTY( void )
{
#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_TTY: ENTER" );
#endif

	//
	// Same as Audio
	// 
	MD_AL25_AccCfg_Audio();
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_FactoryUsb( MCS_BOOL_T boot )
{
	MCS_U8_T bootSet = 0;


#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_FactoryUsb: ENTER" );
#endif

	//
	// CTRL1
	//   IdbEn OPEN
	//   MicEn OPEN
	//   Switches Usb
	// 
	// CTRL2
	//   RCPS userCfg
	//   UsbCplnt userCfg
	//   AccDet 0
	//   SfOutOrd userCfg
	//   SfOutAsrt userCfg
	//   CpEn enable
	//   AdcEn enable
	//   LowPwr userCfg
	//
	// CTRL3
	//   WBth default
	//   ADCDbSet default
	//   BootSet : 01 if boot FALSE, 10 if boot TRUE
	//   JigSet 01
	// 
	// CHGCTRL1
	//   TCHW 5hr
	// 
	// CHGCTRL2
	//   VCHGR_RC disable
	//   MBHOSTEN disable
	// 
	// CHGCTRL3
	//   MBCCVWRC 4.2V
	// 
	// CHGCTRL4
	//   MBCICHWRCL 1
	//   MBCICHWRCH 400mA
	// 
	// CHGCTRL5
	//   EOCS 60mA
	// 
	// CHGCTRL6
	//   AutoStop disable
	// 
	// CHGCTRL7
	//   OTPCGHCVS 7.5V
	// 

	if ( boot == MCS_FALSE )
	{
		bootSet = MD_AL25_REG_CTRL3_BOOTSET_LO;
	}
	else
	{
		bootSet = MD_AL25_REG_CTRL3_BOOTSET_HI;
	}

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =
	(
		MD_AL25_REG_CTRL1_IDBEN_OPEN    |
		MD_AL25_REG_CTRL1_MICEN_OPEN    |
		MD_AL25_REG_CTRL1_COMP2SW_DP2   |
		MD_AL25_REG_CTRL1_COMN1SW_DN1
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =
	(
		MD_AL25_obj.userCfg.rcps           |
		MD_AL25_obj.userCfg.usbCplnt       |
		MD_AL25_REG_CTRL2_ACCDET_ENABLE   |

		MD_AL25_obj.userCfg.sfOutOrd       |
		MD_AL25_obj.userCfg.sfOutAsrt      |
		MD_AL25_REG_CTRL2_CPEN_ENABLE      |
		MD_AL25_REG_CTRL2_ADCEN_ENABLE     |
		MD_AL25_obj.userCfg.lowPwr
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] =
	(
		MD_AL25_REG_CTRL3_WBTH_3P7V        |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		bootSet                            |
		MD_AL25_REG_CTRL3_JIGSET_HI
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] =
	(
		MD_AL25_REG_CHGCTRL1_TCHW_DISABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL3 ] =
	(
		MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
	(
		MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI      |
		MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL5 ] =
	(
		MD_AL25_REG_CHGCTRL5_EOCS_60MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL6 ] =
	(
		MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL7 ] =
	(
		MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V
	);

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL1,
			10,
			&( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] ));
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_FactoryUart( MCS_BOOL_T boot )
{
	MCS_U8_T bootSet = 0;


#if FEAT_EN_TRC_ACCESSORY
	MI_TRC_Msg( 0, "MD_AL25_AccCfg_FactoryUart: ENTER" );
#endif

	//
	// CTRL1
	//   IdbEn OPEN
	//   MicEn OPEN
	//   Switches Uart
	// 
	// CTRL2
	//   RCPS userCfg
	//   UsbCplnt userCfg
	//   AccDet 0
	//   SfOutOrd userCfg
	//   SfOutAsrt userCfg
	//   CpEn enable
	//   AdcEn enable
	//   LowPwr userCfg
	// 
	// CTRL3
	//   WBth default
	//   ADCDbSet default
	//   BootSet : 01 if boot FALSE, 10 if boot TRUE
	//   JigSet 01
	// 
	// CHGCTRL1
	//   TCHW 5hr
	// 
	// CHGCTRL2
	//   VCHGR_RC disable
	//   MBHOSTEN disable
	// 
	// CHGCTRL3
	//   MBCCVWRC 4.2V
	// 
	// CHGCTRL4
	//   MBCICHWRCL 1
	//   MBCICHWRCH 400mA
	// 
	// CHGCTRL5
	//   EOCS 60mA
	// 
	// CHGCTRL6
	//   AutoStop disable
	// 
	// CHGCTRL7
	//   OTPCGHCVS 7.5V
	// 

	if ( boot == MCS_FALSE )
	{
		bootSet = MD_AL25_REG_CTRL3_BOOTSET_LO;
	}
	else
	{
		bootSet = MD_AL25_REG_CTRL3_BOOTSET_HI;
	}

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =
	(
		MD_AL25_REG_CTRL1_IDBEN_OPEN    |
		MD_AL25_REG_CTRL1_MICEN_OPEN    |
		MD_AL25_REG_CTRL1_COMP2SW_UT2   |
		MD_AL25_REG_CTRL1_COMN1SW_UT1
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =
	(
		MD_AL25_obj.userCfg.rcps           |
		MD_AL25_obj.userCfg.usbCplnt       |
		MD_AL25_REG_CTRL2_ACCDET_ENABLE   |

		MD_AL25_obj.userCfg.sfOutOrd       |
		MD_AL25_obj.userCfg.sfOutAsrt      |
		MD_AL25_REG_CTRL2_CPEN_ENABLE      |
		MD_AL25_REG_CTRL2_ADCEN_ENABLE     |
		MD_AL25_obj.userCfg.lowPwr
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] =
	(
		MD_AL25_REG_CTRL3_WBTH_3P7V        |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		bootSet                            |
		MD_AL25_REG_CTRL3_JIGSET_HI
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL1 ] =
	(
		MD_AL25_REG_CHGCTRL1_TCHW_DISABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL3 ] =
	(
		MD_AL25_REG_CHGCTRL3_MBCCVWRC_4P20V
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
	(
		MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI      |
		MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL5 ] =
	(
	MD_AL25_REG_CHGCTRL5_EOCS_60MA
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL6 ] =
	(
		MD_AL25_REG_CHGCTRL6_AUTOSTOP_ENABLE
	);

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL7 ] =
	(
		MD_AL25_REG_CHGCTRL7_OTPCGHCVS_7P5V
	);

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL1,
			10,
			&( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] ));
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_AV( MCS_BOOL_T conn )
{
	// 
	// todo
	// 
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg_UsbOtg( void )
{
	// 
	// todo
	// 
}


/**-----------------------------------------------------------------------------
 *
 * see MD_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MD_AL25_AccCfg( void )
{
	//
	// todo: Raw config of an accessory
	// 
}




/*==============================================================================
 *
 *                            L O C A L   M A C R O S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * Determines if param is Audio Device Type 1 accessory
 *
 * @param __acc   in : accessory in question
 *
 * @return MCS_TRUE  : accessory is Audio Device Type 1
 *         MCS_FALSE : accessory is NOT Audio Device Type 1
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
#define MG_AL25_IsAccessoryAudioType1( __acc )                   \
(                                                              \
	(( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1     )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9  )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10 )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11 )   ||   \
	( __acc   ==   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12 ))       \
)


/*==============================================================================
 *
 *                             L O C A L   T Y P E S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * Internal Key Handling State Machine States
 *
 *------------------------------------------------------------------------------
 */
typedef enum {
	MG_AL25_KEYSTATE_NO_KEY_PRESS,       /** No Key is Pressed                         */
	MG_AL25_KEYSTATE_WAIT_TKP,           /** Key Pressed - waiting for tkp             */
	MG_AL25_KEYSTATE_WAIT_TLKP,          /** Key Pressed - waiting for tlkp            */
	MG_AL25_KEYSTATE_WAIT_TLKP_RELEASE   /** Key Pressed - after tklp, waiting release */
} MG_AL25_KEYSTATE_T;


/**-----------------------------------------------------------------------------
 *
 * Glue object parameters
 *
 *------------------------------------------------------------------------------
 */
typedef struct {
	MG_AL25_KEYSTATE_T    curKeyState;
	MG_AL25_KEYEVENT_T    curKeyPressed;
	MG_AL25_USERCFG_T     userCfg;
	MD_TMR_ID_T           keyTmrId;
	struct timer_list	chTimer;
	MG_AL25_ACCESSORY_T   currentAcc;
	MG_AL25_ACCESSORY_T   previousAcc;
} MG_AL25_INSTANCE_T;


/*==============================================================================
 *
 *                          L O C A L   P R O T O T Y P E S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * Forward reference required by
 *    - MG_AL25_KeyHandler_TmrExp
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_KeyHandler_SM( MG_AL25_KEYEVENT_T newKeyEvent );

MG_AL25_INSTANCE_T   MG_AL25_obj;


/**-----------------------------------------------------------------------------
 *
 * Lookup table takes ADC and CHG_TYP as input to determine accessories.
 *
 * NOTE - NOT Handled by table
 *   - CHG_TYP = 100, 101, 110, 111
 *   - ADC = GND
 *
 * All resistor values from 2.0k - 1000k and Open are handled by this table.
 *
 *------------------------------------------------------------------------------
 */
MG_AL25_ACCESSORY_T MG_AL25_AccessoryLookupTable[ MD_AL25_ADC_TABLE_MAX ][ 4 ] =
{
                // CHG_TYP
                // 000                                      001                                         010                                        011
  /* Gnd   */   MG_AL25_ACCESSORY_UNKNOWN,                  MG_AL25_ACCESSORY_UNKNOWN,                  MG_AL25_ACCESSORY_UNKNOWN,                 MG_AL25_ACCESSORY_UNKNOWN,
  /* 2.0k  */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 2.6k  */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 3.2k  */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 4.0k  */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 4.8k  */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 6.0k  */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 8.0k  */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 10k   */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 12k   */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 14.5k */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9,         MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 17.3k */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10,        MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 20.5k */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11,        MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 24k   */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12,        MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 28.7k */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 34k   */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 40k   */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 50k   */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 65k   */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 80k   */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 102k  */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 121k  */   MG_AL25_ACCESSORY_TTY_CONVERTER,            MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 150k  */   MG_AL25_ACCESSORY_UART_NO_CHGR,             MG_AL25_ACCESSORY_UART_MANUAL_CHGR,         MG_AL25_ACCESSORY_UART_AUTO_CHGR,          MG_AL25_ACCESSORY_UART_AUTO_CHGR,
  /* 200k  */   MG_AL25_ACCESSORY_NONE,                     MG_AL25_ACCESSORY_DEDCHGR_1P8A,             MG_AL25_ACCESSORY_DEDCHGR_1P8A,            MG_AL25_ACCESSORY_DEDCHGR_1P8A,
  /* 255k  */   MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF,     MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF,     MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF,    MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF, 
  /* 301k  */   MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON,      MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON,      MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON,     MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON,  
  /* 365k  */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 442k  */   MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* 523k  */   MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF,    MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF,    MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF,   MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF, 
  /* 619k  */   MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON,     MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON,     MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON,    MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON,  
  /* 1000k */   MG_AL25_ACCESSORY_AUDDEV_TYPE_1,            MG_AL25_ACCESSORY_ILLEGAL,                  MG_AL25_ACCESSORY_ILLEGAL,                 MG_AL25_ACCESSORY_ILLEGAL,
  /* open  */   MG_AL25_ACCESSORY_NONE,                     MG_AL25_ACCESSORY_USB,                      MG_AL25_ACCESSORY_USBCHGR,                 MG_AL25_ACCESSORY_DEDCHGR_1P8A
};


/*==============================================================================
 *
 *                           L O C A L   M E T H O D S
 *
 *==============================================================================
 */


/**-----------------------------------------------------------------------------
 *
 * DEBUG Routine
 *   - Logs info on newAdc and newChgTyp I2C values
 *
 * @param   newAdc      in : new ADC value
 * @param   newChgTyp   in : new CHGTYP value
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyAcc( MD_AL25_ADC_T newAdc, MD_AL25_CHGTYP_T newChgTyp )
{
	MG_AL25_ACCESSORY_T newAcc;

	MAX14577_ELOG("[max14577][%s]newAdc 0x%02X, newChgTyp 0x%X \n", 
			__func__, 
			newAdc, 
			newChgTyp );


	if ( ( newChgTyp > MD_AL25_CHGTYP_DEDICATED_CHGR )  && ( newAdc != MD_AL25_ADC_OPEN) )
	{
		MAX14577_ELOG("[max14577] +++ NotifyAcc: wrong \n");
		newAcc = MG_AL25_ACCESSORY_ILLEGAL;
		return ;
	}
	
	switch( newAdc )
	{
		case MD_AL25_ADC_GND:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: USB OTG \n");
		}
		break;

		case MD_AL25_ADC_GND_ADCLOW:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: Audio/Video with Load \n");
		}
		break;

		case MD_AL25_ADC_2K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 2K \n");
		}
		break;

		case MD_AL25_ADC_2P6K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 2.6K \n");
		}
		break;

		case MD_AL25_ADC_3P2K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 3.2K \n");
		}
		break;

		case MD_AL25_ADC_4K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 4K \n");
		}
		break;

		case MD_AL25_ADC_4P8K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 4.8K \n");
		}
		break;

		case MD_AL25_ADC_6K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 6K \n");
		}
		break;

		case MD_AL25_ADC_8K:
		{
			MAX14577_ELOG("+++ NotifyAcc: 8K \n");
		}
		break;

		case MD_AL25_ADC_10K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 10K \n");
		}
		break;

		case MD_AL25_ADC_12K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 12K \n");
		}
		break;

		case MD_AL25_ADC_14K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 14K \n");
		}
		break;

		case MD_AL25_ADC_17K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 17K \n");
		}
		break;

		case MD_AL25_ADC_20K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 20K \n");
		}
		break;

		case MD_AL25_ADC_24K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 24K \n");
		}
		break;

		case MD_AL25_ADC_29K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 29K \n");
		}
		break;

		case MD_AL25_ADC_34K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 34K \n");
		}
		break;

		case MD_AL25_ADC_40K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 40K \n");
		}
		break;

		case MD_AL25_ADC_50K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 50K \n");
		}
		break;

		case MD_AL25_ADC_65K:
		{
			MAX14577_ELOG("+++ NotifyAcc: 65K \n");
		}
		break;

		case MD_AL25_ADC_80K:
		{
			MAX14577_ELOG("+++ NotifyAcc: 80K \n");
		}
		break;

		case MD_AL25_ADC_102K:
		{
			MAX14577_ELOG("+++ NotifyAcc: 102K \n");
		}
		break;

		case MD_AL25_ADC_121K:
		{
			MAX14577_ELOG("+++ NotifyAcc: 121K \n");
		}
		break;

		case MD_AL25_ADC_150K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 150K \n");
		}
		break;

		case MD_AL25_ADC_200K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 200K \n");
		}
		break;

		case MD_AL25_ADC_255K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 255K \n" );
			newAcc = MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF;;
		}		
		break;

		case MD_AL25_ADC_301K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 301K \n" );
			newAcc = MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON;;
		}
		break;

		case MD_AL25_ADC_365K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 365K \n");
		}
		break;

		case MD_AL25_ADC_442K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 442K \n" );
		}
		break;

		case MD_AL25_ADC_523K:
		{
			MAX14577_ELOG("+++ NotifyAcc: JIG_UART_OFF (523K) \n");
			newAcc = MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF;
		}
		break;

		case MD_AL25_ADC_619K:
		{
			MAX14577_ELOG("+++ NotifyAcc: JIG_UART_ON (619K) \n");
			newAcc = MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON;
		}
		break;

		case MD_AL25_ADC_1000K:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: 1000K \n");
		}
		break;

		case MD_AL25_ADC_OPEN:
		{
			switch ( newChgTyp )
			{
				case MD_AL25_CHGTYP_NO_VOLTAGE:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: NONE \n" );
					newAcc = MG_AL25_ACCESSORY_NONE;
				}
				break;

				case MD_AL25_CHGTYP_USB:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: USB \n");
					newAcc = MG_AL25_ACCESSORY_USB;
				}
				break;

				case MD_AL25_CHGTYP_DOWNSTREAM_PORT:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: Downstream Charging Port \n");
					newAcc = MG_AL25_ACCESSORY_USBCHGR;
				}
				break;

				case MD_AL25_CHGTYP_DEDICATED_CHGR:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: Dedicated Charger \n");
					newAcc = MG_AL25_ACCESSORY_DEDCHGR_1P8A;
				}
				break;

				case MD_AL25_CHGTYP_500MA:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: 500mA Charger \n" );
					newAcc = MG_AL25_ACCESSORY_DEDCHGR_500MA;
				}
				break;

				case MD_AL25_CHGTYP_1A:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: 1A Charger \n" );
					newAcc = MG_AL25_ACCESSORY_DEDCHGR_1A;
				}
				break;

				case MD_AL25_CHGTYP_RFU:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: RFU \n");
					newAcc = MG_AL25_ACCESSORY_ILLEGAL;
				}
				break;

				case MD_AL25_CHGTYP_DB_100MA:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: Dead Battery 100mA Charger\n");
					newAcc = MG_AL25_ACCESSORY_DEDCHGR_100MA;
				}
				break;

				default:
				{
					MAX14577_ELOG("[max14577] +++ NotifyAcc: ERROR ChgTyp \n");
					newAcc = MG_AL25_ACCESSORY_ILLEGAL;
				}
				break;
			}
		}
		break;

		default:
		{
			MAX14577_ELOG("[max14577] +++ NotifyAcc: ERROR Adc \n");
			newAcc = MG_AL25_AccessoryLookupTable[ newAdc ][ newChgTyp ];
		}
	break;
	}

	MG_AL25_SetAccessory( newAcc );

	MG_AL25_obj.previousAcc = MG_AL25_obj.currentAcc;
	MG_AL25_obj.currentAcc  = newAcc;

	//
	// Code below was copied from MAX8929 project with Samsung
	//   Used to notify Samsung API of attach/detach events
	// 
	SAMSUNG_STUB_NotifyNewAccessory( newAcc );
}


/**-----------------------------------------------------------------------------
 *
 * Callback function for expiration of key press handling timer
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
static void MG_AL25_KeyHandler_TmrExp( void )
{
	DEBUG_MAX14577("[max14577] MG_AL25_KeyHandler_TmrExp: call Key State Machine \n" );
	MG_AL25_KeyHandler_SM( MG_AL25_KEYEVENT_TIMER_EXPIRE );
}


/**-----------------------------------------------------------------------------
 *
 * Key Handler State Machine
 *
 * State Machine for managing short key press vs long key press
 *
 * @param newKeyEvent   in : events used by state machine
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_KeyHandler_SM( MG_AL25_KEYEVENT_T newKeyEvent )
{
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
	MI_TRC_2NumericVarsMsg( 0, 
	"MG_AL25_KeyHandler_SM: ENTER (curState %d, newKeyEvent %d)", 
	MG_AL25_obj.curKeyState,
	newKeyEvent );
#endif

	switch( MG_AL25_obj.curKeyState )
	{
		case MG_AL25_KEYSTATE_NO_KEY_PRESS:
		{
			if ( newKeyEvent == MG_AL25_KEYEVENT_RELEASE )
			{
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_2NumericVarsMsg( 0, 
						"MG_AL25_KeyHandler_SM: why here? (state %d, keyEvt %d)",
						MG_AL25_obj.curKeyState,
						newKeyEvent );
#endif
			}
			else if ( newKeyEvent != MG_AL25_KEYEVENT_TIMER_EXPIRE )
			{
				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_WAIT_TKP;

				MG_AL25_App_NotiftyKey( newKeyEvent, MG_AL25_KEYPRESS_INIT );

				//
				// Start Timer for time = tkp
				// 
				MG_AL25_HW_TimerKeyStart( MG_AL25_obj.userCfg.keyTimerValue_tkp, 
						&MG_AL25_KeyHandler_TmrExp );

#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_NumericVarMsg( 0, 
						"MG_AL25_KeyHandler_SM: Start Tkp Timer (key %d)",
						newKeyEvent );
#endif
			}
			else
			{
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_2NumericVarsMsg( 0, 
						"MG_AL25_KeyHandler_SM: why here? (state %d, keyEvt %d)",
						MG_AL25_obj.curKeyState,
						newKeyEvent );
#endif
			}
		}
		break;

		case MG_AL25_KEYSTATE_WAIT_TKP:
		{
			if ( newKeyEvent == MG_AL25_KEYEVENT_TIMER_EXPIRE )
			{
				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_WAIT_TLKP;

				//
				// Start Timer for time = tlkp
				//
				MG_AL25_HW_TimerKeyStart( MG_AL25_obj.userCfg.keyTimerValue_tlkp, MG_AL25_KeyHandler_TmrExp );

#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_NumericVarMsg( 0, 
						"MG_AL25_KeyHandler_SM: Start Tlongkp Timer (key %d)",
						newKeyEvent );
#endif
			}
			else if ( newKeyEvent == MG_AL25_KEYEVENT_RELEASE )
			{
				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_NO_KEY_PRESS;

				//
				// Stop Timer
				// 
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_Msg( 0, "MG_AL25_KeyHandler_SM: Stop Timer" );
#endif

				MG_AL25_HW_TimerKeyStop();
			}
			else
			{
				//
				// todo: need to check for same key?
				// 

				//
				// Stop Timer
				// Start Timer for time = tkp
				//
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_Msg( 0, "MG_AL25_KeyHandler_SM: Stop Timer" );
#endif

				MG_AL25_HW_TimerKeyStop();

				MG_AL25_HW_TimerKeyStart( MG_AL25_obj.userCfg.keyTimerValue_tkp, &MG_AL25_KeyHandler_TmrExp );

#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_NumericVarMsg( 0, 
						"MG_AL25_KeyHandler_SM: Start Tkp Timer (key %d)",
						newKeyEvent );
#endif

				MG_AL25_App_NotiftyKey( newKeyEvent, MG_AL25_KEYPRESS_INIT );
			}
		}
		break;

		case MG_AL25_KEYSTATE_WAIT_TLKP:
		{
			if ( newKeyEvent == MG_AL25_KEYEVENT_TIMER_EXPIRE )
			{
				MG_AL25_App_NotiftyKey( MG_AL25_obj.curKeyPressed, MG_AL25_KEYPRESS_LONG );

				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_WAIT_TLKP_RELEASE;
			}
			else if ( newKeyEvent == MG_AL25_KEYEVENT_RELEASE )
			{
				//
				// Stop Timer
				// 
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_Msg( 0, "MG_AL25_KeyHandler_SM: Stop Timer" );
#endif

				MG_AL25_HW_TimerKeyStop();

				MG_AL25_App_NotiftyKey( MG_AL25_obj.curKeyPressed, MG_AL25_KEYPRESS_SHORT );

				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_NO_KEY_PRESS;
			}
			else
			{
				//
				// Stop Timer
				// 
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_Msg( 0, "MG_AL25_KeyHandler_SM: Stop Timer" );
#endif

				MG_AL25_HW_TimerKeyStop();

				MG_AL25_App_NotiftyKey( MG_AL25_obj.curKeyPressed, MG_AL25_KEYPRESS_SHORT );

				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_WAIT_TKP;

				//
				// Start Timer for time = tkp
				// 
				MG_AL25_HW_TimerKeyStart( MG_AL25_obj.userCfg.keyTimerValue_tkp, &MG_AL25_KeyHandler_TmrExp );

#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_NumericVarMsg( 0, 
						"MG_AL25_KeyHandler_SM: Start Tkp Timer (key %d)",
						newKeyEvent );
#endif

				MG_AL25_App_NotiftyKey( newKeyEvent, MG_AL25_KEYPRESS_INIT );
			}

		}
		break;

		case MG_AL25_KEYSTATE_WAIT_TLKP_RELEASE:
		{
			if ( newKeyEvent == MG_AL25_KEYEVENT_RELEASE )
			{
				MG_AL25_App_NotiftyKey( MG_AL25_obj.curKeyPressed, MG_AL25_KEYPRESS_RELEASE );

				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_NO_KEY_PRESS;
			}
			else if ( newKeyEvent == MG_AL25_KEYEVENT_TIMER_EXPIRE )
			{
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_2NumericVarsMsg( 0, 
					"MG_AL25_KeyHandler_SM: why here? (state %d, keyEvt %d)",
					MG_AL25_obj.curKeyState,
					newKeyEvent );
#endif
			}
			else
			{
				//
				// todo: need to check same key press?  or event = keyPress?
				// 
				MG_AL25_App_NotiftyKey( MG_AL25_obj.curKeyPressed, MG_AL25_KEYPRESS_RELEASE );

				MG_AL25_obj.curKeyState = MG_AL25_KEYSTATE_WAIT_TKP;

				//
				// Start Timer for time = tkp
				// 
				MG_AL25_HW_TimerKeyStart( MG_AL25_obj.userCfg.keyTimerValue_tkp, &MG_AL25_KeyHandler_TmrExp );

#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
				MI_TRC_NumericVarMsg( 0, 
						"MG_AL25_KeyHandler_SM: Start Tkp Timer (key %d)",
						newKeyEvent );
#endif

				MG_AL25_App_NotiftyKey( newKeyEvent, MG_AL25_KEYPRESS_INIT );
			}
		}
		break;

		default:
		break;
	}

	if ( newKeyEvent != MG_AL25_KEYEVENT_TIMER_EXPIRE )
	{
		MG_AL25_obj.curKeyPressed = newKeyEvent;
	}

#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
	MI_TRC_2NumericVarsMsg( 0, 
			"MG_AL25_KeyHandler_SM: EXIT (curState %d, curKeyPressed %d)", 
			MG_AL25_obj.curKeyState,
			MG_AL25_obj.curKeyPressed );
#endif
}


/**-----------------------------------------------------------------------------
 *
 * Takes action based on incoming accessory
 *   - handles key inputs for Samsung specific key handler
 *   - configures I2C regs based on new accessory
 *
 * @param newAcc   in : new Accessory to config
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_SetAccessory( MG_AL25_ACCESSORY_T newAcc )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_ACC
	DEBUG_MAX14577("[max14577] MG_AL25_SetAccessory: newAcc %d \n",newAcc);
#endif

	switch( newAcc )
	{
		case MG_AL25_ACCESSORY_NONE:
		case MG_AL25_ACCESSORY_ILLEGAL:
		{
			MD_AL25_AccCfg_None();
		}
		break;

		case MG_AL25_ACCESSORY_USB:
		{			
			MD_AL25_AccCfg_Usb();
		}
		break;

		case MG_AL25_ACCESSORY_USBCHGR:
		{
			MD_AL25_AccCfg_DwnStrmChgPort();
		}
		break;


		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12:
		{
			//
			// If previous accessory was any type of Audio Device Type 1, 
			//   do NOT re-write I2C Control data
			//
			if  ( MG_AL25_IsAccessoryAudioType1( MG_AL25_obj.currentAcc ) == MCS_FALSE )
			{
				//
				// Previous accessory NOT Audio Device Type 1 device, 
				//   need to configure I2C registers
				// 
				MD_AL25_AccCfg_Audio();

				//
				// Samsung requested that Key Press NOT be notified if 
				//   key pressed during insertion!
				// 
				//        if ( newAcc != MD_MAX8929_ACCESSORY_AUDDEV_TYPE_1 )
				//        {
				//          MD_MAX8929_NotifyKey_Internal( (MD_MAX8929_KEYEVENT_T)newAcc );
				//        }
			}
			//
			// Previous Accessory was already some form of Audio Device Type 1 
			//   so skip the IIC writes and just notify button press/release
			// 
			else
			{
				MG_AL25_KeyHandler_SM( (MG_AL25_KEYEVENT_T)newAcc );
			}

		}
		break;

		case MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF:
		{
			MD_AL25_AccCfg_FactoryUsb( MCS_FALSE );
		}
		break;

		case MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON:
		{
			MD_AL25_AccCfg_FactoryUsb( MCS_TRUE );
		}
		break;

		case MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF:
		{
			MD_AL25_AccCfg_FactoryUart( MCS_FALSE );
		}
		break;

		case MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON:
		{
			MD_AL25_AccCfg_FactoryUart( MCS_TRUE );
		}
		break;

		case MG_AL25_ACCESSORY_DEDCHGR_1P8A:
		case MG_AL25_ACCESSORY_DEDCHGR_500MA:
		case MG_AL25_ACCESSORY_DEDCHGR_1A:
		case MG_AL25_ACCESSORY_DEDCHGR_100MA:
		{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_ACC
			DEBUG_MAX14577("[max14577] MG_AL25_SetAccessory: setup Chgr x \n");
#endif

			MD_AL25_AccCfg_DedChgr();

			// todo: other chargers same as 1.8A ?
		}
		break;

		case MG_AL25_ACCESSORY_UART_NO_CHGR:
		case MG_AL25_ACCESSORY_UART_MANUAL_CHGR:
		case MG_AL25_ACCESSORY_UART_AUTO_CHGR:
		{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_ACC
			DEBUG_MAX14577("[max14577] MG_AL25_SetAccessory: setup UART Accessory\n");
#endif

			MD_AL25_AccCfg_Uart();
		}
		break;

		case MG_AL25_ACCESSORY_TTY_CONVERTER:
		{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_ACC
			DEBUG_MAX14577("[max14577] MG_AL25_SetAccessory: setup TTY Accessory \n");
#endif

			MD_AL25_AccCfg_TTY();
		}
		break;

		default:
		{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_ACC
			DEBUG_MAX14577("[max14577] MG_AL25_SetAccessory: invalid acc %d", newAcc );
#endif
		}
		break;
	}
}


/**-----------------------------------------------------------------------------
 *
 * Returns current accessory from Glue object
 *
 * @return   MG_AL25_ACCESSORY_T : current configured accessory
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
MG_AL25_ACCESSORY_T   MG_AL25_GetAccessory( void )
{
	return( MG_AL25_obj.currentAcc );
}


/**-----------------------------------------------------------------------------
 *
 * Returns previous accessory from Glue object
 *
 * @return   MG_AL25_ACCESSORY_T : current configured accessory
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
MG_AL25_ACCESSORY_T   MG_AL25_GetPrevAccessory( void )
{
	return( MG_AL25_obj.previousAcc );
}


/**-----------------------------------------------------------------------------
 *
 * SAMSUNG specific function from AJ86
 *   - STUB added for compile, link and testing 
 *   - Should be removed for customer integration
 *
 * @param   event   in : new Samsung MUS event
 *
 * @reference 
 * - AJ86 driver
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
#if FEAT_EN_USE_SAMSUNG_STUB
void SAMSUNG_STUB_ChargingMgr_Notifier( Charging_Notifier_t event )
{
#ifdef FEAT_EN_TRC_GLUE_SAMSUNG
	switch( event )
	{
		case  CHARGINGMGR_NOTIFIER_INVALID_EVENT:
		{
			DEBUG_MAX14577("[max14577]SAMSUNG: Invalid Event \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_EARJACK_ATTACHED:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Earjack Attached \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_EARJACK_DETACHED:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Earjack Detached \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_SHORTKEY_PRESSED:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Key Short Press \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_LONGKEY_PRESSED:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Key Long Press \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_LONGKEY_RELEASED:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Key Long Release \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_TA_ATTACHED:
		case  CHARGINGMGR_NOTIFIER_TA_DETACHED:
		case  CHARGINGMGR_NOTIFIER_USB_ATTACHED:
		case  CHARGINGMGR_NOTIFIER_USB_DETACHED:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: cable detection \n");
			eoc_int_count = 0;
			MD_cable_detection_work_handler();
		}
		break;

		case  CHARGINGMGR_NOTIFIER_CARKIT_ATTACHED:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: CarKit Attached \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_CARKIT_DETACHED:
		{
			DEBUG_MAX14577( "[max14577] SAMSUNG: CarKit Detached \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_COMPLETE_CHARGING:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Charge Complete \n");	  
			eoc_int_count = 0;
			MD_full_charge_work_handler();
		}
		break;

		case  CHARGINGMGR_NOTIFIER_STOP_BY_TEMP:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Stop By Temp \n");
		}
		break;

		case  CHARGINGMGR_NOTIFIER_GO_BY_TEMP:
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: Go By Temp \n");
		}
		break;
	}
#endif   // FEAT_EN_TRC_GLUE_SAMSUNG
}
#endif   // FEAT_EN_USE_SAMSUNG_STUB


/**-----------------------------------------------------------------------------
 *
 * SAMSUNG specific function from AJ86
 *   - Converts Maxim Accessory ID to Samsung Accessory ID
 *
 * @param   newAccessory   in : Maxim new accessory type
 *
 * @reference 
 * - AJ86 driver
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
void SAMSUNG_STUB_NotifyNewAccessory( MG_AL25_ACCESSORY_T newAccessory )
{
	MG_AL25_ACCESSORY_T prevAcc = MG_AL25_GetPrevAccessory();


#ifdef MAX14577_DBG_ENABLE
	DEBUG_MAX14577("[max14577] NotifyNewAccessory: nAcc %d, pAcc %d \n", newAccessory, prevAcc );
#endif

	switch( newAccessory )
	{
		case MG_AL25_ACCESSORY_NONE:
		{
			if ( prevAcc == MG_AL25_ACCESSORY_DEDCHGR_1P8A )   // todo - other ded chgrs?
			{
				SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_TA_DETACHED );				
			}
			else if (( prevAcc == MG_AL25_ACCESSORY_USB)   ||( prevAcc == MG_AL25_ACCESSORY_USBCHGR ))
			{
				SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_USB_DETACHED );

				Send_USB_status_to_UsbMenuSel(USB_STATE_DISCONNECTED);
				max14577_notify_connection_state_changed(USB_STATE_DISCONNECTED);
				wake_up_interruptible(&usb_disconnect_waitq);
				microusb_usbjig_detect();
				max14577_EnableDisable_AccDet(1);
			}
			else if (( prevAcc == MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON)   ||( prevAcc == MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF ))
			{
				DEBUG_MAX14577("[max14577] FACTORY_UART_BOOT detached !\n");				
				set_uevent_jig();
			}

			else if (( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1     )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9  )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10 )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11 )   ||
				( prevAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12 ))
			{
				SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_EARJACK_DETACHED );
			}
			else
			{
				//
				// Actually no need to notify anything
				// 
				//ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_INVALID_EVENT );
			}
		}
		break;

		case MG_AL25_ACCESSORY_USB:
		case MG_AL25_ACCESSORY_USBCHGR:
		{
			SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_USB_ATTACHED );

			Send_USB_status_to_UsbMenuSel(USB_STATE_CONNECTED);
			max14577_notify_connection_state_changed(USB_STATE_CONNECTED);
			wake_up_interruptible(&usb_detect_waitq);
			microusb_usbjig_detect();
		}
		break;

		case MG_AL25_ACCESSORY_DEDCHGR_1P8A:   // todo - other ded chgrs ?
		{
			SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_TA_ATTACHED );
		}
		break;

		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11:
		case MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12:
		{
			if (( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1     )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9  )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10 )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11 )   &&
				( prevAcc != MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12 ))
			{
				SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_EARJACK_ATTACHED );
			}
		}
		break;

		case MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF:
		case MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON:
		{
			set_uevent_jig();
		}
		break;
			

		default:
		{ 
			//
			// Actually no need to notify anything
			// 
			SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_INVALID_EVENT );
		}
		break;
	}
}


/**-----------------------------------------------------------------------------
 *
 * SAMSUNG specific function from AJ86
 *   - Checks/Confirms current accessory config
 *   - Converts Maxim Accessory ID to Samsung Accessory ID
 *
 * @param   device   in : Samsung accessory ID to confirm
 *
 * @reference 
 * - AJ86 driver
 *
 * @constraints 
 * Standard
 *
 *------------------------------------------------------------------------------
 */
#ifdef FEAT_EN_USE_SAMSUNG_STUB
MCS_BOOL_T SAMSUNG_STUB_DeviceCheck( ConnectedDeviceType device )
{
	MG_AL25_ACCESSORY_T curAcc = MG_AL25_GetAccessory();
	MCS_BOOL_T retVal          = MCS_FALSE;

	switch( device )
	{
		case PHK_DEVICE:
		{
			if (( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1     )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S0  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S1  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S2  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S3  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S4  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S5  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S6  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S7  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S8  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S9  )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S10 )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S11 )   ||
				( curAcc == MG_AL25_ACCESSORY_AUDDEV_TYPE_1_S12 ))
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		case USB_DEVICE:                
		{
			//
			// USB or USB Chgr?
			// 
			if (( curAcc == MG_AL25_ACCESSORY_USB     )   ||
				( curAcc == MG_AL25_ACCESSORY_USBCHGR ))
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		case CHG_DEVICE:
		{
			//
			// Travel Adapter / Dedicated Charger?
			// 
			if ( curAcc == MG_AL25_ACCESSORY_DEDCHGR_1P8A )   // todo - add other chargers
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		case UART_DEVICE:
		{
			//
			// UART cable?
			// 
			if (( curAcc == MG_AL25_ACCESSORY_UART_NO_CHGR     )   ||
				( curAcc == MG_AL25_ACCESSORY_UART_MANUAL_CHGR )   ||
				( curAcc == MG_AL25_ACCESSORY_UART_AUTO_CHGR   ))
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		case JIG_DEVICE:
		{
			if (( curAcc == MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON   )    ||
				( curAcc == MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF  )    ||
				( curAcc == MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON  )    ||
				( curAcc == MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF ))
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break; 

		case JIG_DEVICE_USB_BOOT_ON:
		{
			//
			// JIG, USB Boot On?
			// 
			if ( curAcc == MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON )
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		case JIG_DEVICE_USB_BOOT_OFF:
		{
			//
			// JIG, USB Boot Off?
			// 
			if ( curAcc == MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF )
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		case ITP_DEVICE:  
		case JIG_DEVICE_UART_BOOT_ON:
		{
			//
			// JIG, UART Boot On?
			// 
			if ( curAcc == MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON )
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		case JIG_DEVICE_UART_BOOT_OFF:
		{
			//
			// JIG, UART Boot Off?
			// 
			if ( curAcc == MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF )
			{
				retVal = MCS_TRUE;
			}
			else
			{
				retVal = MCS_FALSE;
			}
		}
		break;

		default:
		{
			retVal = MCS_FALSE;
		}
		break;
	}

#ifdef FEAT_EN_TRC_GLUE_SAMSUNG
	{
		if ( retVal == MCS_FALSE )
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: DeviceCheck(%d) FALSE \n", device);
		}
		else
		{
			DEBUG_MAX14577("[max14577] SAMSUNG: DeviceCheck(%d) TRUE \n", device);
		}
	}
#endif

		return( retVal );
}
#endif   // FEAT_EN_USE_SAMSUNG_STUB


/*==============================================================================
 *
 *                         E X T E R N A L   M E T H O D S
 *
 *==============================================================================
 */


////////////////////////////////////////////////////////////////////////////////
// 
// GlueAPIs
// 
// These routines pass data and information INTO the Maxim 14561 Driver.  They 
//   are meant to provide Glue code to ->
//  
//   1) Translate type definitions from one architecture to another
//   2) Manage some automatic settings for options not used
//   3) etc
// 
// NOTE: You can also call any public AL25 Driver Function directly!
// 
////////////////////////////////////////////////////////////////////////////////


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_ModuleInit( void )
{
	MG_AL25_obj.currentAcc    =   MG_AL25_ACCESSORY_UNKNOWN;
	MG_AL25_obj.previousAcc   =   MG_AL25_ACCESSORY_UNKNOWN;
	MG_AL25_obj.curKeyState   =   MG_AL25_KEYSTATE_NO_KEY_PRESS;
	MG_AL25_obj.keyTmrId      =   0;
	MG_AL25_obj.userCfg.keyTimerValue_tkp  = 300;
	MG_AL25_obj.userCfg.keyTimerValue_tlkp = 700;

	//
	// Allocate Timers
	// 

#ifdef FEAT_EN_SAMSUNG_API

	init_timer(&MG_AL25_obj.chTimer);
	MG_AL25_obj.chTimer.function =  (void *) &MG_AL25_KeyHandler_TmrExp;
	MG_AL25_obj.chTimer.expires = jiffies + HZ/4;
	MG_AL25_obj.chTimer.data = 0;

#else		
	MG_AL25_obj.keyTmrId = 
	MD_TMR_Allocate( MCS_FUNC_PTR( MD_TMR_CALLBACK_F, &MG_AL25_KeyHandler_TmrExp ));
#endif

	//
	// Initialize Driver
	// 
	{
		MD_AL25_USERCFG_T  driverCfg;

		driverCfg.rcps        = MD_AL25_RCPS_DISABLE;
		driverCfg.usbCplnt    = MD_AL25_USBCPLNT_DISABLE;
		driverCfg.sfOutOrd    = MD_AL25_SFOUTORD_NORMAL;
		driverCfg.sfOutAsrt   = MD_AL25_SFOUTASRT_NORMAL;
		driverCfg.lowPwr      = MD_AL25_LOWPWR_DISABLE;
		driverCfg.dchk        = MD_AL25_DCHK_50MS;

		MD_AL25_ModuleInit( &driverCfg );

		MD_AL25_ModuleOpen();

		//! Run the statemachine to determine initial accessory
		MD_AL25_ServiceStateMachine();
	}

#ifndef FEAT_EN_SAMSUNG_API
	//
	// Enable Driver ISR
	// 
	MG_AL25_HW_EnableISR();
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */

#define MAX14577_I2C_BUFSIZE		MD_AL25_REG_MAX	

void MG_AL25_HW_I2CRead( MCS_U8_T    devAddr,
                         MCS_U8_T    devReg,
                         MCS_U8_T    numBytesToRead,
                         MCS_U8_T   *pData )
{

#if 0
	MD_IICM_ReadDataFromReg( devAddr,
	devReg,
	pData,
	numBytesToRead );
#else

	static MCS_U8_T buf[MAX14577_I2C_BUFSIZE];
	MCS_U8_T regaddr, val;

	int ret;
	struct i2c_msg msg[2];
	int i;
	int i2cRErr_cnt = 0;		// Heekwon Ko [mUSBX]

	if(numBytesToRead > MAX14577_I2C_BUFSIZE -1 )
		return -EIO;

	DEBUG_MAX14577("[max14577][read]");
	for ( i = 0; i < numBytesToRead; i++) {
		regaddr = devReg + i;
		msg[0].addr = max14577_i2c_client->addr;
		msg[0].flags = 0;
		msg[0].len = 1;
		msg[0].buf	= &regaddr;

		msg[1].addr = max14577_i2c_client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = 1;
		msg[1].buf = &val;

		retry:
		ret = i2c_transfer(max14577_i2c_client->adapter, msg, 2);
		if ( ret!= 2)
		{
			DEBUG_MAX14577("[max14577] ****** I2C read error for RegNo= %d DevSAddr = 0x%02x\n",*(msg[0].buf), msg[0].addr);
			//return -EIO;

			i2cRErr_cnt++;

			while(i2cRErr_cnt > 3)
			{
				
				printk("i2c read Error occurs %d times. \n", i2cRErr_cnt);
				
				goto ignore;
				
			}
			goto retry;
		}
		ignore:		// Heekwon Ko [mUSBX]
		
		*(pData+i) = val;
		DEBUG_MAX14577("[%x:%x]", devReg+i, *(pData+i));
	}
	DEBUG_MAX14577("\n");

#endif

	return 0;

	//  #if FEAT_EN_TRC_GLUE_I2C
	//  MI_TRC_3NumericVarsMsg( 0, "MG_AL25_HW_I2CRead: addr %02x reg %02x #bytes %d",
	//                          devAddr,
	//                          devReg,
	//                          numBytesToRead );
//  #endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_I2CWrite( MCS_U8_T   devAddr,
                          MCS_U8_T   devReg,        
                          MCS_U16_T  numBytesToWrite,
                          MCS_U8_T * pData )        
{

#if 0
  MD_IICM_WriteDataToReg( devAddr,
                          devReg,
                          pData,
                          numBytesToWrite,
                          MCS_TRUE );
#else

	int i=0;
	DEBUG_MAX14577("[max14577][write]");
	for (i=0 ; i<numBytesToWrite ; i++)	{
		DEBUG_MAX14577("[%x:%x]", devReg+i, *(pData+i));
	}
	DEBUG_MAX14577("\n");
	

	static MCS_U8_T buf[MAX14577_I2C_BUFSIZE];
	int ret;
	int i2cWErr_cnt = 0;		// Heekwon Ko [mUSBX]

	if( numBytesToWrite > MAX14577_I2C_BUFSIZE-1)
		return -EIO;

	struct i2c_msg msg[1];

	buf[0] = devReg;
	memcpy(buf+1, pData, numBytesToWrite);
	msg[0].addr = max14577_i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = numBytesToWrite+1;
	msg[0].buf = buf;

	retry:
	ret = i2c_transfer(max14577_i2c_client->adapter, msg, 1);
	if( ret!= 1){
		DEBUG_MAX14577("[max14577] MAX14577 I2C write error \n");
		//return -EIO;

				i2cWErr_cnt++;
	
				while(i2cWErr_cnt > 3)
				{
					
					printk("i2c read Error occurs %d times. \n", i2cWErr_cnt);
					
					goto ignore;
					
				}
		goto retry;
	}

#endif

	ignore: 	// Heekwon Ko [mUSBX]
#ifdef FEAT_EN_TRC_GLUE_I2C
	MI_TRC_3NumericVarsMsg( 0, "MG_AL25_HW_I2CWrite: addr %02x reg %02x #bytes %d",
			devAddr,
			devReg,
			numBytesToWrite );
#endif

	return 0;
}

/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_EnableISR( void )
{
#if 0
	EXTI_InitTypeDef EXTI_InitStructure;


	/* Configure EXTI Line2 to generate an interrupt on falling edge */  
	EXTI_InitStructure.EXTI_Line    = EXTI_Line2;
	EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; //EXTI_Trigger_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init( &EXTI_InitStructure );
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_DisableISR( void )
{
#if 0
	EXTI_InitTypeDef EXTI_InitStructure;


	/* Configure EXTI Line2 to generate an interrupt on falling edge */  
	EXTI_InitStructure.EXTI_Line    = EXTI_Line2;
	EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_LineCmd = DISABLE;
	EXTI_Init( &EXTI_InitStructure );
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_ClearISR( void )
{
#if 0
	EXTI_ClearITPendingBit( EXTI_Line2 );
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_TimerKeyStart( MCS_U32_T timeoutValue, MG_AL25_TMR_EXP_F *pCallbackFunc )
{
#ifdef FEAT_EN_TRC_GLUE_TIMER
	MI_TRC_NumericVarMsg( 0, "MG_AL25_HW_TimerKeyStart: timeout %d", timeoutValue );
#endif

	//
	// Start Timer
	//
#ifdef FEAT_EN_SAMSUNG_API

#else
	MD_TMR_Schedule( MG_AL25_obj.keyTmrId, timeoutValue, MCS_FALSE );
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_HW_TimerKeyStop( void )
{
#ifdef FEAT_EN_TRC_GLUE_TIMER
	MI_TRC_Msg( 0, "MG_AL25_HW_TimerKeyStop: ENTER" );
#endif

	//
	// Stop Timer
	// 
#ifdef FEAT_EN_SAMSUNG_API

#else
	MD_TMR_Unschedule( MG_AL25_obj.keyTmrId );
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_DcdT( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_DcdT: state %d \n", state);
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_AdcError( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_AdcError: state %d \n", state);
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_DbChg( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_DbChg: state %d \n", state);
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_VbVolt( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_VbVolt: state %d \n", state);
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_Ovp( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577]--- App_NotifyINT_Ovp: state %d \n", state );
#endif

	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL2,
			1,
			&gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] );

	if(state) //ovp status
	{
		int count=0;

		while(count < 10)
		{
			msleep(1);
			count ++;
		}
		//after 10ms, read status register

		MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS,
				MD_AL25_REG_INTSTAT3,
				1,
				&gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] );

		//if ovp is still 1, turn off sfout
		if(gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] & MD_AL25_M_INTSTAT3_OVP)
		{
			gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] &= ~(MD_AL25_REG_CTRL2_SFOUTORD_NORMAL);
		}
	}
	else //ovp over
	{
		gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] |= (MD_AL25_REG_CTRL2_SFOUTORD_NORMAL);
	}

	DEBUG_MAX14577("[max14577]--- CTRL2 0x%x \n", gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] );

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_CTRL2,
			1,
			&gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] );  
}

/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_ChgDetRun( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_ChgDetRun: state %d \n", state );
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_MbcChgErr( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_MbcChgErr state %d \n", state);
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_CgMbc( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_CgMbc: state %d \n", state);
#endif
}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotifyINT_EOC( MCS_BOOL_T state )
{
#ifdef FEAT_EN_TRC_GLUE_NOTIFY_INT
	DEBUG_MAX14577("[max14577] --- App_NotifyINT_EOC: state %d \n", state );
#endif

	if(state)		
		SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_COMPLETE_CHARGING );

}


/**-----------------------------------------------------------------------------
 *
 * see MG_AL25.h
 *
 *------------------------------------------------------------------------------
 */
void MG_AL25_App_NotiftyKey( MG_AL25_KEYEVENT_T curKey, 
                             MG_AL25_KEYPRESS_T keyPress )
{
#ifdef FEAT_EN_TRC_GLUE_KEYPRESS_STATE_MACHINE
	DEBUG_MAX14577(" [max14577] GLUE(MG_AL25_App_NotiftyKey): curKey %d, keyPress %d \n",curKey, keyPress );
#endif

	if ( curKey == MG_AL25_KEYEVENT_S0_PRESS )
	{
		switch ( keyPress )
		{
			case  MG_AL25_KEYPRESS_RELEASE:
			{
				SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_LONGKEY_RELEASED );
			}
			break;

			case  MG_AL25_KEYPRESS_SHORT:
			{
				SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_SHORTKEY_PRESSED );
			}
			break;

			case  MG_AL25_KEYPRESS_LONG:
			{
				SAMSUNG_STUB_ChargingMgr_Notifier( CHARGINGMGR_NOTIFIER_LONGKEY_PRESSED );
			}
			break;
		}
	}

#if FEAT_EN_TRC_GLUE
	{
		char keyName[ 20 ];
		char keyPressName[ 20 ];

		switch ( curKey )
		{
			case  MG_AL25_KEYEVENT_RELEASE:
			{
				strcpy( keyName, "RELEASE" );
			}
			break;
			case  MG_AL25_KEYEVENT_S0_PRESS:
			{
				strcpy( keyName, "S0" );
			}
			break;
			case  MG_AL25_KEYEVENT_S1_PRESS:
			{
				strcpy( keyName, "S1" );
			}
			break;
			case  MG_AL25_KEYEVENT_S2_PRESS:
			{
				strcpy( keyName, "S2" );
			}
			break;
			case  MG_AL25_KEYEVENT_S3_PRESS:
			{
				strcpy( keyName, "S3" );
			}
			break;
			case  MG_AL25_KEYEVENT_S4_PRESS:
			{
				strcpy( keyName, "S4" );
			}
			break;
			case  MG_AL25_KEYEVENT_S5_PRESS:
			{
				strcpy( keyName, "S5" );
			}
			break;
			case  MG_AL25_KEYEVENT_S6_PRESS:
			{
				strcpy( keyName, "S6" );
			}
			break;
			case  MG_AL25_KEYEVENT_S7_PRESS:
			{
				strcpy( keyName, "S7" );
			}
			break;
			case  MG_AL25_KEYEVENT_S8_PRESS:
			{
				strcpy( keyName, "S8" );
			}
			break;
			case  MG_AL25_KEYEVENT_S9_PRESS:
			{
				strcpy( keyName, "S9" );
			}
			break;
			case  MG_AL25_KEYEVENT_S10_PRESS:
			{
				strcpy( keyName, "S10" );
			}
			break;
			case  MG_AL25_KEYEVENT_S11_PRESS:
			{
				strcpy( keyName, "S11" );
			}
			break;
			case  MG_AL25_KEYEVENT_S12_PRESS:
			{
				strcpy( keyName, "S12" );
			}
			break;
			default:
			{
				strcpy( keyName, "ERROR?" );
			}
			break;
		}

		switch ( keyPress )
		{
			case  MG_AL25_KEYPRESS_RELEASE:
			{
				strcpy( keyPressName, "RELEASE" );
			}
			break;
			case  MG_AL25_KEYPRESS_INIT:
			{
				strcpy( keyPressName, "INIT" );
			}
			break;
			case  MG_AL25_KEYPRESS_SHORT:
			{
				strcpy( keyPressName, "SHORT" );
			}
			break;
			case  MG_AL25_KEYPRESS_LONG:
			{
				strcpy( keyPressName, "LONG" );
			}
			break;
			default:
			{
				strcpy( keyPressName, "ERROR?" );
			}
			break;
		}

		DEBUG_MAX14577("[max14577] SAMSUNG: new Key %s(%d), keyPress %s(%d)\n", keyName, curKey, keyPressName, keyPress );
	}
#endif   // FEAT_EN_TRC_GLUE
}

static void max14577_read_int_register(struct work_struct *work)
{
	DEBUG_MAX14577("[max14577] max14577_read_int_register \n");
	MD_AL25_ServiceStateMachine();

	wake_up_interruptible(&usb_detect_waitq);
	enable_irq(max14577_i2c_client->irq);
	
	wake_lock_timeout(&max14577_wake_lock, 3*HZ);
	//wake_unlock(&max14577_wake_lock);
}

static irqreturn_t max14577_interrupt(int irq, void *ptr)
{
	MAX14577_ELOG("\n[max14577] INT!!!!!!! \n");
	
	if (!work_pending(&max14577_work)) {
		disable_irq_nosync(irq);
		//disable_irq(irq);

		wake_lock_timeout(&max14577_wake_lock, 3*HZ);
		//wake_lock(&max14577_wake_lock);
		
		//queue_work(max14577_workqueue, &max14577_work);
		schedule_work(&max14577_work);
	}
	
       return IRQ_HANDLED;
}

static void max14577_interrupt_init(int irq, void *dev_id)
{
#define MAX14577_IRQ_FLAGS (IRQF_DISABLED | IRQF_SHARED)
	//! todo IRQ type  RANJIT
	printk("[MAX14577] MAX INTERRUPT IS EDGE TRIGGERED \n");
	//set_irq_type ( irq,  IRQ_TYPE_EDGE_RISING /*IRQ_TYPE_LEVEL_HIGH*/ );
	set_irq_type ( irq, IRQ_TYPE_EDGE_FALLING /*IRQ_TYPE_LEVEL_HIGH*/ );
	if( request_irq(irq, max14577_interrupt, MAX14577_IRQ_FLAGS, "MAX14577 Detect", dev_id))
	{}
	enable_irq_wake(irq);
}


static int max14577_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	MAX14577_ELOG("[max14577] probe +++ @@@@@ \n");
	init_waitqueue_head(&usb_detect_waitq);
	INIT_WORK(&max14577_work, max14577_read_int_register);
	max14577_workqueue = create_singlethread_workqueue("max14577_wq");

	max14577_i2c_client = client;
	
	wake_lock_init(&max14577_wake_lock, WAKE_LOCK_SUSPEND, "MAX14577");
	
	MG_AL25_ModuleInit();
	max14577_interrupt_init(client->irq, (void*)id);

	// spin_lock_init(&(timer_usb.vib_lock));

	// todo regarding ftm_enable_usb_sw Ranjit

	return 0;
}


static int __devexit max14577_remove(struct i2c_client *client)
{
	eoc_int_count = 0;
	
	printk(" max14577_remove \n");
	return 0;
}

static int max14577_resume(struct i2c_client *client)
{
	printk(" max14577_resume \n");
	return 0;
}



static const struct i2c_device_id max14577_id[] = {
        { "max14577", 0 },
        { }
};

struct i2c_driver max14577_i2c_driver = {
	.driver = {
		.name = "max14577",
		.owner = THIS_MODULE,
	},
	.probe		= max14577_probe,
	.remove		= __devexit_p(max14577_remove),
	.resume		= max14577_resume,
	.id_table	= max14577_id,
};

int microusb_enable(void)
{
	int retval;
	printk("microusb_enable() ++++\n");
	retval = i2c_add_driver(&max14577_i2c_driver);
	if (retval != 0) {
		printk("[Micro-USB] can't add i2c driver");
	}

	return retval;
}
EXPORT_SYMBOL(microusb_enable);


void microusb_disable(void)
{
	i2c_del_driver(&max14577_i2c_driver);
}
EXPORT_SYMBOL(microusb_disable);

int get_usbic_state(void)
{
	int ret = 0;
	MAX14577_ELOG("[max14577][%s] CurrAcc = %d\n", __func__, MG_AL25_obj.currentAcc);
  	switch(MG_AL25_obj.currentAcc)
	{      
		case  MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON:
		case  MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF:
		case  MG_AL25_ACCESSORY_USB:
			ret = MICROUSBIC_USB_CABLE;
			break;

		case  MG_AL25_ACCESSORY_USBCHGR:
			ret = MICROUSBIC_USB_CHARGER;
			break;

		case MG_AL25_ACCESSORY_DEDCHGR_1P8A:
		case MG_AL25_ACCESSORY_DEDCHGR_500MA:
		case MG_AL25_ACCESSORY_DEDCHGR_1A:
		case MG_AL25_ACCESSORY_DEDCHGR_100MA:
			ret = MICROUSBIC_TA_CHARGER;
			break;
		//! anything else need to be added 

		default:
			break;
	}
	return ret;
}

int get_real_usbic_state(void)
{
	unsigned char status1, status2;
	MD_AL25_ADC_T newAdc;
	MD_AL25_CHGTYP_T newChgTyp;
	MG_AL25_ACCESSORY_T newAcc;		// Currently not used
	u8 ret = 0;

	MG_AL25_HW_I2CRead(MD_AL25_IIC_ADDRESS, MD_AL25_REG_CTRL2, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ]);
	printk("[%s] CTRL2 = 0x%x\n", __func__, gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ]);

	MG_AL25_HW_I2CRead(MD_AL25_IIC_ADDRESS, MD_AL25_REG_INTSTAT1, 1, &status1);
	MG_AL25_HW_I2CRead(MD_AL25_IIC_ADDRESS, MD_AL25_REG_INTSTAT2, 1, &status2);

	msleep(5);

	newAdc = (MD_AL25_ADC_T)(( status1 & MD_AL25_M_INTSTAT1_ADC) >> MD_AL25_B_INTSTAT1_ADC);
	newChgTyp = ( MD_AL25_CHGTYP_T )(( status2 & MD_AL25_M_INTSTAT2_CHGTYP) >> MD_AL25_B_INTSTAT2_CHGTYP);

	MAX14577_ELOG("[%s] newAdc=0x%02x, newChgTyp=0x%02x\n" ,__func__, newAdc, newChgTyp);

	if ( ( newChgTyp > MD_AL25_CHGTYP_DEDICATED_CHGR )  && ( newAdc != MD_AL25_ADC_OPEN) )
	{
		newAcc = MG_AL25_ACCESSORY_ILLEGAL;
		printk(KERN_INFO "[%s] MG_AL25_ACCESSORY_ILLEGAL \n", __func__);
		ret = MICROUSBIC_NO_DEVICE;
	}
	else if ( newAdc == MD_AL25_ADC_OPEN )
	{
		switch ( newChgTyp )
		{
			case  MD_AL25_CHGTYP_NO_VOLTAGE:
			{
				newAcc = MG_AL25_ACCESSORY_NONE;
				MAX14577_ELOG("[%s] Nothing Attached \n", __func__);
				ret = MICROUSBIC_NO_DEVICE;				
			}
			break;
			case  MD_AL25_CHGTYP_USB:
			{
				newAcc = MG_AL25_ACCESSORY_USB;
				MAX14577_ELOG("[%s] USB cable attached \n", __func__);
				ret = MICROUSBIC_USB_CABLE;
			}
			break;
			case  MD_AL25_CHGTYP_DOWNSTREAM_PORT:
			{
				newAcc = MG_AL25_ACCESSORY_USBCHGR;
				MAX14577_ELOG("[%s] Charging Downstream port \n", __func__);
				ret = MICROUSBIC_USB_CHARGER;
				
			}
			break;
			case  MD_AL25_CHGTYP_DEDICATED_CHGR:
			{
				newAcc = MG_AL25_ACCESSORY_DEDCHGR_1P8A;
				MAX14577_ELOG("[%s] Dedicated charger detected \n", __func__);
				ret = MICROUSBIC_TA_CHARGER;
			}
			break;
			case  MD_AL25_CHGTYP_500MA:
			{
				newAcc = MG_AL25_ACCESSORY_DEDCHGR_500MA;
				MAX14577_ELOG("[%s] Special 500MA charger detected \n", __func__);
			}
			break;
			case  MD_AL25_CHGTYP_1A:
			{
				newAcc = MG_AL25_ACCESSORY_DEDCHGR_1A;
				MAX14577_ELOG("[%s] Special 1A charger detected \n", __func__);
			}
			break;
			case  MD_AL25_CHGTYP_RFU:
			{
				newAcc = MG_AL25_ACCESSORY_ILLEGAL;
				MAX14577_ELOG("[%s] Illegal accessory detected \n", __func__);
			}
			break;
			case  MD_AL25_CHGTYP_DB_100MA:
			{
				newAcc = MG_AL25_ACCESSORY_DEDCHGR_100MA;
				MAX14577_ELOG("[%s]Dead Battery Charging detected \n", __func__);
			}
			break;
			default :
			{
				newAcc = MG_AL25_ACCESSORY_ILLEGAL;
				MAX14577_ELOG("[%s] Default Acc\n", __func__);
			}
		}
	}
	else if ( newAdc == MD_AL25_ADC_255K )	{
		newAcc = MG_AL25_ACCESSORY_FACTORY_USB_BOOT_OFF;
		MAX14577_ELOG("[%s] MICRO_JIG_USB_OFF detected \n", __func__);
		ret = MICROUSBIC_JIG_USB_OFF;
	}
	else if ( newAdc == MD_AL25_ADC_301K )	{
		newAcc = MG_AL25_ACCESSORY_FACTORY_USB_BOOT_ON;
		MAX14577_ELOG("[%s] MICRO_JIG_USB_ON detected \n", __func__);
		ret = MICROUSBIC_JIG_USB_ON;
	}	
	else
	{
//		MAX14577_ELOG("[%s] Should lookup Accessory Table \n", __func__);
		newAcc = MG_AL25_AccessoryLookupTable[ newAdc ][ newChgTyp ];
		switch (newAcc)	{
			case MG_AL25_ACCESSORY_FACTORY_UART_BOOT_OFF:
				MAX14577_ELOG("[%s] MICRO_JIG_UART_OFF detected \n", __func__);
				ret = MICROUSBIC_JIG_UART_OFF;
				break;
			case MG_AL25_ACCESSORY_FACTORY_UART_BOOT_ON:
				MAX14577_ELOG("[%s] MICRO_JIG_UART_ON detected \n", __func__);
				ret = MICROUSBIC_JIG_UART_ON;
				break;
		}
	}

	return ret;
}

EXPORT_SYMBOL(get_usbic_state);
EXPORT_SYMBOL(get_real_usbic_state);

void max14577_update_regs()
{
	DEBUG_MAX14577("[MAX14577] start func %s\n", __func__);
	MG_AL25_HW_I2CRead(MD_AL25_IIC_ADDRESS, MD_AL25_REG_CDETCTRL, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] );
	mdelay(10);

	gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] |= MD_AL25_REG_CDETCTRL_CHGTYPM_ENABLE;

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,	MD_AL25_REG_CDETCTRL, 1, &( gMD_AL25_I2CReg[ MD_AL25_REG_CDETCTRL ] ));
}

void max14577_update_corrupted_regs()
{
	int state1, state2;

	state1 = get_usbic_state();
	state2 = get_real_usbic_state();
	
	if (get_usbic_state() != get_real_usbic_state())	{
		MAX14577_ELOG("[MAX14577][%s] regs values are CORRUPTED !!!(%d,%d) \n", __func__, state1, state2);
		max14577_clear_intr();		
		max14577_update_regs();
	}
	else	{
		MAX14577_ELOG("[MAX14577][%s] no currupt (%d,%d) \n", __func__, state1, state2);
	}
}

void max14577_clear_intr()
{
	printk("[max14577][%s]\n", __func__);
	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS, MD_AL25_REG_INT1, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ] );
	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS, MD_AL25_REG_INT2, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ] );
	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS, MD_AL25_REG_INT3, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ] );

	DEBUG_MAX14577("[max14577][%s] 0x%x 0x%x 0x%x\n", __func__, gMD_AL25_I2CReg[ MD_AL25_REG_INT1 ]
												,gMD_AL25_I2CReg[ MD_AL25_REG_INT2 ]
												,gMD_AL25_I2CReg[ MD_AL25_REG_INT3 ]);
}


// Called by switch sio driver
void mcirousb_usbpath_change(int usb_path)
{
	MD_AL25_INT_T       newInt;
	MD_AL25_INTSTAT_T   newIntStat;

	DEBUG_MAX14577("[max14577] mcirousb_usbpath_change \n");

	microusb_usbpath = usb_path;
	if (get_real_usbic_state() != MICROUSBIC_USB_CABLE)	{
		printk("[MAX14577] Invalid value of regs, skip usbpath_change\n");
		printk("[MAX14577] newAdc=0x%02x, newChgTyp=0x%02x\n", MD_AL25_obj.intStat.statAdc, MD_AL25_obj.intStat.statChargerType);
		return ;
	}

	if(usb_path) {	//!  CP USB
		// switch to audio
		MAX14577_ELOG("[MAX14577][%s] USB Path to CP\n", __func__);
		gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] = 0x12;

		gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =	(
			MD_AL25_obj.userCfg.rcps		   |
			MD_AL25_obj.userCfg.usbCplnt	   |
			MD_AL25_REG_CTRL2_ACCDET_DISABLE	|
			MD_AL25_obj.userCfg.sfOutOrd	   |
			MD_AL25_obj.userCfg.sfOutAsrt	   |
			MD_AL25_REG_CTRL2_CPEN_ENABLE	   |
			MD_AL25_REG_CTRL2_ADCEN_ENABLE	   |
			MD_AL25_obj.userCfg.lowPwr
		);
	} 
	else {
		MAX14577_ELOG("[MAX14577][%s] USB Path to AP\n", __func__);
		gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] =	  (
			MD_AL25_REG_CTRL1_IDBEN_OPEN	|
			MD_AL25_REG_CTRL1_MICEN_OPEN	|
			MD_AL25_REG_CTRL1_COMP2SW_DP2	|
			MD_AL25_REG_CTRL1_COMN1SW_DN1
		);

		gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] =	(
			MD_AL25_obj.userCfg.rcps		   |
			MD_AL25_obj.userCfg.usbCplnt	   |
			MD_AL25_REG_CTRL2_ACCDET_ENABLE	|
			MD_AL25_obj.userCfg.sfOutOrd	   |
			MD_AL25_obj.userCfg.sfOutAsrt	   |
			MD_AL25_REG_CTRL2_CPEN_ENABLE	   |
			MD_AL25_REG_CTRL2_ADCEN_ENABLE	   |
			MD_AL25_obj.userCfg.lowPwr
		);
	}

	gMD_AL25_I2CReg[ MD_AL25_REG_CTRL3 ] = (
		MD_AL25_REG_CTRL3_WBTH_3P7V 	   |
		MD_AL25_REG_CTRL3_ADCDBSET_0P5MS   |
		MD_AL25_REG_CTRL3_BOOTSET_AUTO	   |
		MD_AL25_REG_CTRL3_JIGSET_AUTO
	);

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,	MD_AL25_REG_CTRL1, 3, &( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL1 ] ));
	
}

// Called by switch sio driver
void mcirousb_uartpath_change(int uart_path)
{
	//! todo
}

void max14577_EnableDisable_AccDet(u8 enable)
{
	MG_AL25_HW_I2CRead(MD_AL25_IIC_ADDRESS, MD_AL25_REG_CTRL2, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] );
	mdelay(10);
#if defined(MAX14577_DBG_ENABLE)
	DEBUG_MAX14577("[MAX14577] reg[CTRL2]=0x%02x", gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ]);
#endif

	if (enable)	{
		gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] |=MD_AL25_REG_CTRL2_ACCDET_ENABLE;
	}
	else	{
		gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] &= ~MD_AL25_REG_CTRL2_ACCDET_ENABLE;
	}

	MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,	MD_AL25_REG_CTRL2, 1, &( gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] ));
	
#if defined(MAX14577_DBG_ENABLE)
	MG_AL25_HW_I2CRead(MD_AL25_IIC_ADDRESS, MD_AL25_REG_CTRL2, 1, &gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ] );
	mdelay(10);
	DEBUG_MAX14577(" -> 0x%02x\n", gMD_AL25_I2CReg[ MD_AL25_REG_CTRL2 ]);
#endif
}

int set_uevent_jig(void)
{
	printk("[%s] Jig On Message!!\n", __func__);
	kobject_uevent(&sio_switch_dev->kobj, KOBJ_CHANGE);
//	kobject_uevent(&sdev_jig1->kobj, KOBJ_CHANGE);
}



/* charging functions */
int MG_AL25_HW_I2CRead_dur_sleep( unsigned char reg_addr )
{   	
	unsigned char buf;
	int ret = MD_normal_i2c_read_byte(max14577_i2c_client->addr, reg_addr, &buf);

	if(ret < 0)
	{
		printk(KERN_ERR"[%s] Fail to Read max14577\n", __FUNCTION__);
		return -1;
	} 

	return buf;
}

int MG_AL25_HW_I2CWrite_dur_sleep( unsigned char reg_addr, unsigned char data )
{ 
	int ret = MD_normal_i2c_write_byte(max14577_i2c_client->addr, reg_addr, data);

	if(ret < 0)
	{
		printk(KERN_ERR"[%s] Fail to Write max14577\n", __FUNCTION__);
		return -1;
	} 

	return ret;

}

int MD_check_full_charge(void)
{
	unsigned char max14577_full;

	MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS,
			MD_AL25_REG_INTSTAT3,
			1,
			&gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] );

	if(( max14577_full & MD_AL25_M_INTSTAT3_EOC ) ==  MD_AL25_B_INTSTAT3_EOC)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL(MD_check_full_charge);

int MD_check_full_charge_dur_sleep_(void)
{
	unsigned char max14577_full;

	max14577_full = MG_AL25_HW_I2CRead_dur_sleep( MD_AL25_REG_INTSTAT3 );

	if(( gMD_AL25_I2CReg[ MD_AL25_REG_INTSTAT3 ] & MD_AL25_M_INTSTAT3_EOC ) ==	MD_AL25_B_INTSTAT3_EOC)
		return 0;
	else
		return 1;
}
EXPORT_SYMBOL(MD_check_full_charge_dur_sleep_);

void enable_charging( bool is_update, bool is_sleep )
// ----------------------------------------------------------------------------
// Description    : 
// Input Argument :  
// Return Value   : 
{
	unsigned char max14577_chgctrl;

	DEBUG_MAX14577("[max14577] +++  enable charging_start, is_update %d\n", is_update);

	if(!is_update)
		return;

	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE	|
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_ENABLE
	);

	if( is_sleep){
		MG_AL25_HW_I2CWrite_dur_sleep(MD_AL25_REG_CHGCTRL2, gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ]);
	}
	else{
		MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
				MD_AL25_REG_CHGCTRL2,
				1,
				&gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] );
	}
}

EXPORT_SYMBOL(enable_charging);

void disable_charging( bool is_update, bool is_sleep )
// ----------------------------------------------------------------------------
// Description    : 
// Input Argument :  
// Return Value   : 
{
	DEBUG_MAX14577("[max14577] +++  disable charging_start, is_update %d\n", is_update);

	if(!is_update)
		return;
	
	gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] =
	(
		MD_AL25_REG_CHGCTRL2_VCHGR_RC_ENABLE   |
		MD_AL25_REG_CHGCTRL2_MBHOSTEN_DISABLE
	);

	if( is_sleep)
	{
		MG_AL25_HW_I2CWrite_dur_sleep(MD_AL25_REG_CHGCTRL2, gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ]);
	}
	else{			
		MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
				MD_AL25_REG_CHGCTRL2,
				1,
				&gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL2 ] );
	}
}
EXPORT_SYMBOL(disable_charging);

#if defined(CONFIG_MAX14577_MICROUSB_CURRENT_CHANGE)
void change_charging_current( int change_current, bool is_sleep )
// ----------------------------------------------------------------------------
// Description    : 
// Input Argument :  
// Return Value   : 
{
	bool is_changed = false;
	//printk(KERN_DEBUG "[CHG] change_charging_current ++ current value : %d, is_sleep : %d\n", change_current, is_sleep);
	switch(change_current)
	{
		case CHARGING_CURRENT_600 :
		{		
			if( gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] != (MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI|MD_AL25_REG_CHGCTRL4_MBCICHWRCH_600MA))
			{
				is_changed = true;
				
				gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
				(
					MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI		|
					MD_AL25_REG_CHGCTRL4_MBCICHWRCH_600MA
				);
			}
			break;
		}
		case CHARGING_CURRENT_450 :
		{
			if( gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] != (MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI|MD_AL25_REG_CHGCTRL4_MBCICHWRCH_450MA))
			{
				is_changed = true;
				
				gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
				(
					MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI		|
					MD_AL25_REG_CHGCTRL4_MBCICHWRCH_450MA
				);
			}
			break;
		}
		case CHARGING_CURRENT_400 :
		{
			if( gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] != (MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI|MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA))
			{
				is_changed = true;
				
				gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] =
				(
					MD_AL25_REG_CHGCTRL4_MBCICHWRCL_HI		|
					MD_AL25_REG_CHGCTRL4_MBCICHWRCH_400MA
				);
			}
			break;
		}
		default :
			break;
	}

	if(is_changed)
	{
		if( is_sleep)
		{
			MG_AL25_HW_I2CWrite_dur_sleep(MD_AL25_REG_CHGCTRL4, gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ]);

			gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] = MG_AL25_HW_I2CRead_dur_sleep( MD_AL25_REG_CHGCTRL4 );

			//printk(KERN_INFO "[CHG] reg_chgctrl4 : 0x%x\n", gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ]);
		}
		else{
			MG_AL25_HW_I2CWrite( MD_AL25_IIC_ADDRESS,
					MD_AL25_REG_CHGCTRL4,
					1,                     
					&( gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] ));

			MG_AL25_HW_I2CRead( MD_AL25_IIC_ADDRESS,
					MD_AL25_REG_CHGCTRL4,
					1,
					&gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ] );

			//printk(KERN_INFO "[CHG] REG_CHGCTRL4 : 0x%x\n", gMD_AL25_I2CReg[ MD_AL25_REG_CHGCTRL4 ]);
		}
	}

	//printk(KERN_DEBUG "[CHG] change_charging_current --\n");
}

EXPORT_SYMBOL(change_charging_current);
#endif

EXPORT_SYMBOL(mcirousb_usbpath_change);
EXPORT_SYMBOL(mcirousb_uartpath_change);
EXPORT_SYMBOL(max14577_clear_intr);

MODULE_AUTHOR("SAMSUNG ELECTRONICS CO., LTD");
MODULE_DESCRIPTION("MAX8929 MicroUSB driver");
MODULE_LICENSE("GPL");

