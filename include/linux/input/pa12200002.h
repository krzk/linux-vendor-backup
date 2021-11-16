#ifndef  __TXC_H__
#define  __TXC_H__

#define TXC_DEV_NAME    "PA122"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/eralysuspend.h>
#endif

/*ioctl cmds*/
#define TXC_IOCTL_BASE 'g'
#define TXC_IOCTL_SET_ALS_ENABLE	_IOW(TXC_IOCTL_BASE, 0, int)
#define TXC_IOCTL_GET_ALS_ENABLE	_IOR(TXC_IOCTL_BASE, 1, int)
#define TXC_IOCTL_SET_PS_ENABLE		_IOW(TXC_IOCTL_BASE, 2, int)
#define TXC_IOCTL_GET_PS_ENABLE		_IOR(TXC_IOCTL_BASE, 3, int)

//PA12201001 als/ps sensor register map
/* REG 0x00*/
#define REG_CFG0 		0X00  	// ALS_GAIN(D5-4),PS_ON(D1) ALS_ON(D0)
#define PA12_ALS_GAIN500		(0 << 4) 	// 500lux
#define PA12_ALS_GAIN4000      (1 << 4)  //4000lux
#define PA12_ALS_GAIN7000      (2 << 4)  //7000lux
#define PA12_ALS_GAIN31000      (3 << 4)  //31000lux
#define PA12_PS_EN          (1 << 1) 
#define PA12_ALS_EN         (1 << 0)

/* REG 0x01 */
#define REG_CFG1 		0X01  	// LED_CURR(D5-4),PS_PRST(D3-2),ALS_PRST(D1-0)
#define PA12_LED_CURR150		(0 << 4) 	// 0:150mA 1:100mA 2:50mA 3:25mA
#define PA12_LED_CURR100		(1 << 4) 	// 0:150mA 1:100mA 2:50mA 3:25mA
#define PA12_LED_CURR50                 (2 << 4)
#define PA12_PS_PRST1		(0 << 2)	// 0:1point 1:2points 2:4points 3:8points (for INT)
#define PA12_PS_PRST2		(1 << 2)
#define PA12_PS_PRST4		(2 << 2)
#define PA12_PS_PRST8		(3 << 2)
#define PA12_ALS_PRST1		0	// 0:1point 1:2points 2:4points 3:8points (for INT)
#define PA12_ALS_PRST2		1
#define PA12_ALS_PRST4		2
#define PA12_ALS_PRST8		3

/* REG 0x02 */
   // PS_MODE(D7-6),CLEAR(D4),INT_SET(D3-2),PS_INT(D1),ALS_INT(D0)
#define REG_CFG2 		0X02  	
#define PA12_PS_MODE_OFFSET		(0 << 6)	// 0:OFFSET 3:NORMAL
#define PA12_PS_MODE_NORMAL		(3 << 6)	
#define PA12_RESET_CLEAR         (0 << 4) /* clear and reset */
#define PA12_RESET_ONLY          (1 << 4) /* Only reset */
#define PA12_INT_ALS		(0 << 2)	// 0:interrupt ALS only 1:PS only 3:BOTH
#define PA12_INT_PS         (1 << 2)
#define PA12_INT_ALS_PS_BOTH    (3 << 2)
#define PA12_PS_INTF_INACTIVE   (0 << 1)  /* PS interrupt flag 00: Inactive 01 :Active*/
#define PA12_PS_INTF_ACTIVE   (1 << 1)  /* PS interrupt flag 00: Inactive 01 :Active*/
#define PA12_ALS_INTF_ACTIVE   1 /* ALS interrupt flag 00: Inactive 01 :Active*/
#define PA12_ALS_INTF_INACTIVE   (0)  /* PS interrupt flag 00: Inactive 01 :Active*/

/* REG 0x03 */
#define REG_CFG3		0X03  	// INT_TYPE(D6),PS_PERIOD(D5-3),ALS_PERIOD(D2-0)
#define PA12_PS_INT_WINDOW		(0 << 6) 	// 0:Window type 1:Hysteresis type for Auto Clear flag
#define PA12_PS_INT_HYSTERESIS  (1 << 6)
#define PA12_PS_PERIOD6		(0 << 3)	// 6.25 ms 
#define PA12_PS_PERIOD12    (1 << 3)	// 12.5 ms 
#define PA12_PS_PERIOD25	(2 << 3)	// 25 ms 
#define PA12_PS_PERIOD50	(3 << 3)	// 50 ms 
#define PA12_PS_PERIOD100	(4 << 3)	// 100 ms 
#define PA12_ALS_PERIOD0		0	// 0 ms
#define PA12_ALS_PERIOD100	    1	// 100 ms
#define PA12_ALS_PERIOD300		2	// 300 ms
#define PA12_ALS_PERIOD700		3	// 700 ms

#define REG_ALS_TL_LSB	0X04  	// ALS Threshold Low LSB
#define REG_ALS_TL_MSB	0X05  	// ALS Threshold Low MSB
#define REG_ALS_TH_LSB	0X06  	// ALS Threshold high LSB
#define REG_ALS_TH_MSB	0X07  	// ALS Threshold high MSB
#define REG_PS_TL		0X08  	// PS Threshold Low
#define REG_PS_TH		0X0A  	// PS Threshold High
#define REG_ALS_DATA_LSB	0X0B  	// ALS DATA
#define REG_ALS_DATA_MSB	0X0C  	// ALS DATA
#define REG_PS_DATA			0X0E  	// PS DATA
#define REG_PS_OFFSET		0X10 
#define REG_PS_SET			0X11  	// TBD

//Parameters
#define PA12_ALS_TH_HIGH	35000
#define PA12_ALS_TH_LOW		0
#define PA12_PS_NEAR_TH_HIGH		255
#define PA12_PS_NEAR_TH_LOW		25
#define PA12_PS_FAR_TH_HIGH		40
#define PA12_PS_FAR_TH_LOW		0

#define PA12_PS_OFFSET_DEFAULT  2 	// for X-talk canceling
#define PA12_PS_OFFSET_MAX	50
#define PA12_PS_OFFSET_EXTRA 	1
#define PA12_FAST_CAL		1	
#define PA12_FAST_CAL_ONCE	0

/* Sun light On*/
#define PA12_SUN_LIGHT_ON 16000
#define PA12_ALS_TH_LSB_SUN_LIGHT_ON (PA12_SUN_LIGHT_ON & 0xFF)
#define PA12_ALS_TH_MSB_SUN_LIGHT_ON (PA12_SUN_LIGHT_ON >> 8)
#define PA12_ALS_TL_LSB_SUN_LIGHT_ON 0
#define PA12_ALS_TL_MSB_SUN_LIGHT_ON 0
/* Sun light Off*/
#define PA12_SUN_LIGHT_OFF 5000
#define PA12_ALS_TH_LSB_SUN_LIGHT_OFF 0xFF
#define PA12_ALS_TH_MSB_SUN_LIGHT_OFF 0xFF
#define PA12_ALS_TL_LSB_SUN_LIGHT_OFF (PA12_SUN_LIGHT_OFF & 0xFF)
#define PA12_ALS_TL_MSB_SUN_LIGHT_OFF (PA12_SUN_LIGHT_OFF >> 8)


/* Calibration file path */
#define PS_CAL_FILE_PATH	"/data/xtalk_cal"       

#define PS_TAG                  "[ps sensor]: "
#define APS_FUN(f)               printk(KERN_INFO PS_TAG"%s\n", __FUNCTION__)
#define APS_ERR(fmt, args...)   printk(KERN_ERR  PS_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

#define ps_log(fmt, args...)    printk(PS_TAG fmt, ##args)

/*report event type*/
#define ABS_ALS ABS_MISC
#define ABS_PS	ABS_DISTANCE

/*sensor mask*/
#define ID_ALS	(1 << 0)
#define ID_PS	(1 << 1)

#define PS_NEAR 0
#define PS_FAR  1
#define PS_UNKONW -1

struct txc_data {
    struct i2c_client *client;
    struct input_dev *input_dev;

	const char *power_name; //read dts sensor 2.8 power name
	struct regulator *pa_pwr; //sensor 2.8 regulator
	
    int irq;  //irq number
    int irq_gpio;  //read irq gpio from dts,HW irq
    int ps_data;
    u16 ps_near_threshold;
    u16 ps_far_threshold;
    struct delayed_work ps_dwork;
    struct mutex enable_lock;
    //struct delayed_work ioctl_enable_work;
    bool ps_enable;
	bool nontype_enable;
    u8 psdata;
    int pstype;
//    bool mcu_enable;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
#endif
    int ps_calibvalue;
    int calibvalue_base;
    bool pa122_sys_run_cal; 
    
    //int ps_syscall;
    //int gesture;
    //struct notifier_block als_enable_notifier;
    int ps_resumed;
    int resume_report;
    int mobile_wakeup;
    //struct notifier_block notifier;
    int ps_shutdown;
    struct mutex i2c_lock;
    int fast_calibvalue;
    int fast_calib_flag;
    int factory_calibvalue;
    struct wake_lock pa_wakelock;
};

#endif
