/* drivers/input/touchscreen/cytma340.c
 *
 * Cypress TSP driver.
 *
 * Copyright (C) 2009 Samsung Electronics Co. Ltd.
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/i2c/twl.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/slab_def.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#ifdef TOUCH_PROC
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#endif
#include <mach/hardware.h>
#include <plat/omap-pm.h>
#include <plat/gpio.h>
#include <plat/mux.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/mach-types.h>
#include "cytma340.h"

struct i2c_driver cytma340_i2c_driver;

static struct workqueue_struct *cytma340_wq = NULL;
static struct workqueue_struct *check_ic_wq;

static struct cytma340_data cytma340;

#if ENABLE_NOISE_TEST_MODE
                                       
//botton_right    botton_left            center              top_right          top_left 		
unsigned char test_node[TEST_POINT_NUM] = {12,       		   20, 		   104,    		        188,     			   196};   

unsigned int return_refer_0, return_refer_1, return_refer_2, return_refer_3, return_refer_4;
unsigned int return_delta_0, return_delta_1, return_delta_2, return_delta_3, return_delta_4;
uint16_t diagnostic_addr;
#endif

#ifdef _SUPPORT_MULTITOUCH_
static report_finger_info_t fingerInfo[MAX_USING_FINGER_NUM]={0,};
#endif

#ifdef __SUPPORT_TOUCH_KEY__
#define MAX_KEYS	2
static const int touchkey_keycodes[] = {
			KEY_MENU,
			KEY_BACK,
};

static int touchkey_status[MAX_KEYS];

#define TK_STATUS_PRESS		1
#define TK_STATUS_RELEASE		0
#endif

static int prev_wdog_val = -1;

#define I2C_M_WR 0 /* for i2c */
#define I2C_M_RD 1

/*------------------------------ for tunning ATmel - start ----------------------------*/
struct class *touch_class;
EXPORT_SYMBOL(touch_class);

struct device *switch_test;
EXPORT_SYMBOL(switch_test);

/*------------------------------ for tunning ATmel - end ----------------------------*/

#define __QT_CONFIG__

/*------------------------------ Sub functions -----------------------------------*/
/*!
  \brief Initializes touch driver.

  This function initializes the touch driver: tries to connect to given 
  address, sets the message handler pointer, reads the info block and object
  table, sets the message processor address etc.

  @param I2C_address is the address where to connect.
  @param (*handler) is a pointer to message handler function.
  @return DRIVER_SETUP_OK if init successful, DRIVER_SETUP_FAILED 
  otherwise.
  */


unsigned int touch_state_val=0;
EXPORT_SYMBOL(touch_state_val);

#ifdef USE_TS_TA_DETECT_CHANGE_REG 
int set_tsp_for_ta_detect(int state)
{
	return 1;
}
EXPORT_SYMBOL(set_tsp_for_ta_detect);
#endif

void TSP_forced_release(void)
{
}
EXPORT_SYMBOL(TSP_forced_release);

void TSP_forced_release_forOKkey(void)
{
}

EXPORT_SYMBOL(TSP_forced_release_forOKkey);

int tsp_reset( void )
{
	int ret=1, key = 0;
	printk("[TSP] %s+\n", __func__ );

	// for TSK
	for(key = 0; key < MAX_KEYS ; key++)
		touchkey_status[key] = TK_STATUS_RELEASE;

	if (cytma340.use_irq)
	{
		disable_irq(cytma340.irq);
	}

#if 0
	gpio_set_value( TSP_SCL , 0 ); 
	gpio_set_value( TSP_SDA , 0 ); 
	gpio_set_value( TSP_INT , 0 ); 

	msleep( 5 );

	gpio_set_value( TSP_SCL , 1 ); 
	gpio_set_value( TSP_SDA , 1 ); 
	gpio_set_value( TSP_INT , 1 ); 
#endif

	gpio_set_value(OMAP_GPIO_TOUCH_EN, 0);
	gpio_direction_output(OMAP_GPIO_TOUCH_INT, 0);

	msleep( 5 );

	gpio_set_value(OMAP_GPIO_TOUCH_EN, 1);
	gpio_direction_input(OMAP_GPIO_TOUCH_INT);
	gpio_set_value(OMAP_GPIO_TOUCH_INT, 1);

	msleep(200);

tsp_reset_out:
	if (cytma340.use_irq)
	{
		enable_irq(cytma340.irq);
	}
	printk("[TSP] %s-\n", __func__ );

	return ret;
}


#if defined(__SUPPORT_TOUCH_KEY__)
static void process_key_event(uint8_t tsk_msg)
{
	int i;
	int keycode= 0;
	int st_old, st_new;

	//check each key status
	for(i = 0; i < MAX_KEYS; i++)
	{
		st_old = touchkey_status[i];
		st_new = (tsk_msg>>(i+6)) & 0x1;
		keycode = touchkey_keycodes[i];

		touchkey_status[i] = st_new;	// save status

		if(st_new > st_old)
		{
			// press event
			printk("[TSP] press keycode: %4d, keypress: %4d\n", keycode, 1);
			input_report_key(cytma340.input_dev, keycode, 1);
		}
		else if(st_old > st_new)
		{
			// release event
			printk("[TSP] release keycode: %4d, keypress: %4d\n", keycode, 0);
			input_report_key(cytma340.input_dev, keycode, 0);
		}
	}

}

#endif


/*------------------------------ I2C Driver block -----------------------------------*/

static int i2c_tsp_sensor_probe_client(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *new_client;
	int err = 0;
	printk("[TSP] %s() - start\n", __FUNCTION__);

	if ( !i2c_check_functionality(adapter,I2C_FUNC_SMBUS_BYTE_DATA) ) {
		printk("byte op is not permited.\n");
		goto ERROR0;
	}

	new_client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL );

	if ( !new_client )	{
		err = -ENOMEM;
		goto ERROR0;
	}

	new_client->addr = address; 
	printk("[TSP] %s :: addr=%x\n", __FUNCTION__, address);
	new_client->adapter = adapter;
	new_client->driver = &cytma340_i2c_driver;
	strlcpy(new_client->name, "touchscreen", I2C_NAME_SIZE);

	cytma340.client = new_client;

	printk("[TSP] %s() - end : success\n", __FUNCTION__);
	return 0;

	ERROR0:
		return err;
}

static int i2c_tsp_sensor_attach_adapter(struct i2c_adapter *adapter)
{
	int addr = 0;
	int id = 0;

	addr = TMA340_I2C_ADDR;

	id = adapter->nr;

	if (id == 3)
		return i2c_tsp_sensor_probe_client(adapter, addr, 0);
	return 0;
}

static int i2c_tsp_sensor_detach_client(struct i2c_client *client)
{
	int err;
	printk("[TSP] %s() - start\n", __FUNCTION__);
	i2c_set_clientdata(client,NULL);

	kfree(client); /* Frees client data too, if allocated at the same time */
	cytma340.client = NULL;
	return 0;
}

struct i2c_driver cytma340_i2c_driver = {
	.driver = {
		.name	= TOUCHSCREEN_NAME,
		.owner	= THIS_MODULE,
	},
	.attach_adapter	= &i2c_tsp_sensor_attach_adapter,
	.remove		= &i2c_tsp_sensor_detach_client,
};

int tsp_i2c_read(u8 reg, unsigned char *rbuf, int buf_size)
{
	int ret=-1;
	struct i2c_msg rmsg;
	uint8_t start_reg;

	rmsg.addr = cytma340.client->addr;
	rmsg.flags = 0;//I2C_M_WR;
	rmsg.len = 1;
	rmsg.buf = &start_reg;
	start_reg = reg;
	ret = i2c_transfer(cytma340.client->adapter, &rmsg, 1);

	if(ret>=0) {
		rmsg.flags = I2C_M_RD;
		rmsg.len = buf_size;
		rmsg.buf = rbuf;
		ret = i2c_transfer(cytma340.client->adapter, &rmsg, 1 );
	}

	if( ret < 0 )
	{
		printk("[TSP] Error code : %d\n", __LINE__ );
	}

	return ret;
}

/* ------------------------- ????????????? -----------------*/

static irqreturn_t cytma340_irq_handler(int irq, void *dev_id)
{
	//printk("[TSP] %s\n",__FUNCTION__);

	disable_irq_nosync(cytma340.irq);
	queue_work(cytma340_wq, &cytma340.ts_event_work);

	return IRQ_HANDLED;
}

int  cytma340_enable_irq_handler(void)
{
	printk("[TSP] %s\n",__FUNCTION__);

	if (cytma340.irq != -1)
	{
		cytma340.irq = OMAP_GPIO_IRQ(OMAP_GPIO_TOUCH_INT);

		if (request_irq(cytma340.irq, cytma340_irq_handler, IRQF_TRIGGER_FALLING, TOUCHSCREEN_NAME, &cytma340))	
		{
			printk("[TS][ERR] Could not allocate touchscreen IRQ!\n");
			cytma340.irq = -1;
			input_free_device(cytma340.input_dev);
			return -EINVAL;
		}
		else
		{
			cytma340.use_irq = 1;
			printk("[TS] registor touchscreen IRQ!\n");
		}
	}
	return 0;
}

static void cytma340_ts_work_func(struct work_struct *work)
{
	int ret=0;
	uint8_t buf[12];// 02h ~ 0Dh
	uint8_t i2c_addr = 0x02;
	int i = 0;
	int finger = 0;

	ret = tsp_i2c_read( i2c_addr, buf, sizeof(buf));

	if (ret <= 0) {
		printk("[TSP] i2c failed : ret=%d, ln=%d\n",ret, __LINE__);
		goto work_func_out;
	}

	finger = buf[0] & 0x07;	
	
	fingerInfo[0].x = (buf[1] << 8) |buf[2];
	fingerInfo[0].y = (buf[3] << 8) |buf[4];
	fingerInfo[0].z = buf[5];
	fingerInfo[0].id = buf[6] >>4;

	fingerInfo[1].x = (buf[7] << 8) |buf[8];
	fingerInfo[1].y = (buf[9] << 8) |buf[10];
	fingerInfo[1].z = buf[11];
	fingerInfo[1].id = buf[6] & 0xf;

	/* check key event*/
	if(fingerInfo[0].status != 1 && fingerInfo[1].status != 1)
		process_key_event(buf[0]);

	/* check touch event */
	for ( i= 0; i<MAX_USING_FINGER_NUM; i++ )
	{
		if(fingerInfo[i].id >=1) // press interrupt
		{
			if(fingerInfo[i].status != -2) // force release
				fingerInfo[i].status = 1;
			else
				fingerInfo[i].status = -2;
		}
		else if(fingerInfo[i].id ==0) // release interrupt (only first finger)
		{
			if(fingerInfo[i].status == 1) // prev status is press
				fingerInfo[i].status = 0;
			else if(fingerInfo[i].status == 0 || fingerInfo[i].status == -2) // release already or force release
				fingerInfo[i].status = -1;				
		}

		if(fingerInfo[i].status < 0) continue;
		
		input_report_abs(cytma340.input_dev, ABS_MT_POSITION_X, fingerInfo[i].x);
		input_report_abs(cytma340.input_dev, ABS_MT_POSITION_Y, fingerInfo[i].y);
		input_report_abs(cytma340.input_dev, ABS_MT_TOUCH_MAJOR, fingerInfo[i].status);
		input_report_abs(cytma340.input_dev, ABS_MT_WIDTH_MAJOR, fingerInfo[i].z);
		input_mt_sync(cytma340.input_dev);

	}

	input_sync(cytma340.input_dev);

work_func_out:
	if (cytma340.use_irq)
	{
		enable_irq(cytma340.irq);
	}
}

static void cytma340_check_ic_work_func(struct work_struct *work)
{
	int ret=0;
	uint8_t i2c_addr = 0x1F;
	uint8_t wdog_val[1];

	ret = tsp_i2c_read( i2c_addr, wdog_val, sizeof(wdog_val));
	if (ret <= 0) {
		tsp_reset();
		printk("[TSP] i2c failed : ret=%d, ln=%d\n",ret, __LINE__);
	}
	else if(wdog_val[0] == (uint8_t)prev_wdog_val || wdog_val[0] == 0x0 ||wdog_val[0] == 0xff)
	{
		tsp_reset();
		prev_wdog_val = -1;
	}
	else
	{
		printk("[TSP] %s counter = %x, prev = %x\n", __func__, wdog_val[0], (uint8_t)prev_wdog_val);
		prev_wdog_val = wdog_val[0];
	}
}

static enum hrtimer_restart cytma340_watchdog_timer_func(struct hrtimer *timer)
{
	queue_work(check_ic_wq, &cytma340.work_timer);
	hrtimer_start(&cytma340.timer, ktime_set(2, 0), HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static int __init cytma340_probe(struct platform_device *pdev)
{
	int ret;	
	int i;
	int key;
	uint8_t i2c_addr = 0x1B;
	uint8_t buf[3];

	printk("[TSP] %s \n", __FUNCTION__);

	cytma340.input_dev = input_allocate_device();
	if (cytma340.input_dev == NULL) {
		ret = -ENOMEM;
		printk(KERN_DEBUG "cytma340_probe: Failed to allocate input device\n");
	}

	cytma340.input_dev->name = TOUCHSCREEN_NAME;
	cytma340.input_dev->id.bustype = BUS_I2C;
	cytma340.input_dev->id.vendor = 0;
	cytma340.input_dev->id.product = 0;
	cytma340.input_dev->id.version = 0;

	set_bit(EV_SYN, cytma340.input_dev->evbit);
	set_bit(EV_KEY, cytma340.input_dev->evbit);
	set_bit(EV_ABS, cytma340.input_dev->evbit);
	set_bit(BTN_TOUCH, cytma340.input_dev->keybit);

	input_set_abs_params(cytma340.input_dev, ABS_MT_POSITION_X, 0, 320, 0, 0);
	input_set_abs_params(cytma340.input_dev, ABS_MT_POSITION_Y, 0, 480, 0, 0);
	input_set_abs_params(cytma340.input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(cytma340.input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);

#if defined(__SUPPORT_TOUCH_KEY__)
	for(key = 0; key < MAX_KEYS; key++)
		input_set_capability(cytma340.input_dev, EV_KEY, touchkey_keycodes[key]);

	for(key = 0; key < MAX_KEYS; key++)
		touchkey_status[key] = TK_STATUS_RELEASE;
#endif

	ret = input_register_device(cytma340.input_dev);
	if (ret) {
		printk(KERN_DEBUG "cytma340_probe: Unable to register %s input device\n", cytma340.input_dev->name);
		goto err_input_register_device_failed;
	}

	cytma340_wq = create_singlethread_workqueue("cytma340_wq");
	if (!cytma340_wq)
		return -ENOMEM;

	check_ic_wq = create_singlethread_workqueue("check_ic_wq");	
	if (!check_ic_wq)
		return -ENOMEM;

	INIT_WORK(&cytma340.ts_event_work, cytma340_ts_work_func );
	//INIT_WORK(&cytma340.work_timer, cytma340_check_ic_work_func );

	//hrtimer_init(&cytma340.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	//cytma340.timer.function = cytma340_watchdog_timer_func;

	ret = i2c_add_driver(&cytma340_i2c_driver);
	if(ret) printk("[TSP] i2c_add_driver failed...(%d)\n", ret);

	printk("[TSP] ret : %d, cytma340.client name : %s\n",ret,cytma340.client->name);

	cytma340_enable_irq_handler();
		
#ifdef _SUPPORT_MULTITOUCH_
	for (i=0; i<MAX_USING_FINGER_NUM ; i++)
		fingerInfo[i].z = -1;
#endif

	//dprintk("%s ,  %d\n",__FUNCTION__, __LINE__ );
#ifdef CONFIG_HAS_EARLYSUSPEND
	cytma340.early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	cytma340.early_suspend.suspend = cytma340_early_suspend;
	cytma340.early_suspend.resume = cytma340_late_resume;
	register_early_suspend(&cytma340.early_suspend);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

	/* Check point - i2c check - start */
	ret = tsp_i2c_read( i2c_addr, buf, sizeof(buf));

	if (ret <= 0) {
		printk(KERN_ERR "i2c_transfer failed\n");
		ret = tsp_i2c_read( i2c_addr, buf, sizeof(buf));

		if (ret <= 0) 
		{
			printk("[TSP] %s, ln:%d, Failed to register TSP!!!\n\tcheck the i2c line!!!, ret=%d\n", __func__,__LINE__, ret);
			goto err_check_functionality_failed;
		}
	}
	/* Check point - i2c check - end */
	/* Check point - Firmware */
	printk("[TSP] %s, ver CY=%x\n", __func__ , buf[0] );
	printk("[TSP] %s, ver HW=%x\n", __func__ , buf[1] );
	printk("[TSP] %s, ver SW=%x\n", __func__ , buf[2] );

	//hrtimer_start(&cytma340.timer, ktime_set(5, 0), HRTIMER_MODE_REL);

	return 0;

err_input_register_device_failed:
	input_free_device(cytma340.input_dev);
	return ret;
err_check_functionality_failed:
	return ret;
}

static int cytma340_remove(struct platform_device *pdev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cytma340.early_suspend);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

	input_unregister_device(cytma340.input_dev);

#if 0
	if (cytma340.irq != -1)
	{
		if(g_enable_touchscreen_handler == 1)
		{
			free_irq(cytma340.irq, &cytma340);
			g_enable_touchscreen_handler = 0;
		}
	}
#endif
	gpio_set_value(OMAP_GPIO_TOUCH_EN, 0);
	return 0;
}

static int cytma340_shutdown(struct platform_device *pdev)
{
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&cytma340.early_suspend);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

	input_unregister_device(cytma340.input_dev);

	if (cytma340_wq)
		destroy_workqueue(cytma340_wq);

	if (check_ic_wq)
		destroy_workqueue(check_ic_wq);

	if (cytma340.use_irq)
	{
		free_irq(cytma340.irq, &cytma340);
		cytma340.use_irq = 0;
	}

	gpio_set_value(OMAP_GPIO_TOUCH_EN, 0);
	return 0;
}

static int cytma340_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	printk("[TSP] %s+\n", __func__ );

	if (cytma340.use_irq)
	{
		disable_irq(cytma340.irq);
	}

#if 0
	gpio_set_value( TSP_SCL , 0 ); 
	gpio_set_value( TSP_SDA , 0 ); 
	gpio_set_value( TSP_INT , 0 ); 
#endif
	gpio_direction_output(OMAP_GPIO_TOUCH_INT, 0);

	ret = cancel_work_sync(&cytma340.work_timer);
	ret = cancel_work_sync(&cytma340.ts_event_work);

	if (ret && cytma340.use_irq) /* if work was pending disable-count is now 2 */
	{
		enable_irq(cytma340.irq);
	}

	//hrtimer_cancel(&cytma340.timer);

	gpio_set_value(OMAP_GPIO_TOUCH_EN, 0);

	msleep(400);

	printk("[TSP] %s-\n", __func__ );
	return 0;
}

static int cytma340_resume(struct i2c_client *client)
{
	int ret, key;
	uint8_t i2c_addr = 0x1D;
	uint8_t buf[1];

	printk("[TSP] %s+\n", __func__ );

#if 0
	gpio_set_value( TSP_SCL , 1 ); 
	gpio_set_value( TSP_SDA , 1 ); 
	gpio_set_value( TSP_INT , 1 ); 
#endif

	gpio_direction_input(OMAP_GPIO_TOUCH_INT);
	gpio_set_value(OMAP_GPIO_TOUCH_INT, 1);

	gpio_set_value(OMAP_GPIO_TOUCH_EN, 1);

	msleep(130);

	// for TSK
	for(key = 0; key < MAX_KEYS; key++)
		touchkey_status[key] = TK_STATUS_RELEASE;

	while (cytma340.use_irq)
	{
		ret = tsp_i2c_read( i2c_addr, buf, sizeof(buf));
		if (ret <= 0) {
//			printk("[TSP] %d : i2c_transfer failed\n", __LINE__);
		}
		else if( buf[0] == 0 )
		{
			continue;
		}
		else
		{
			printk("[TSP] %s:%d, ver SW=%x\n", __func__,__LINE__, buf[0] );
			enable_irq(cytma340.irq);
			break;
		}
		msleep(20);
	}

	prev_wdog_val = -1;

	//hrtimer_start(&cytma340.timer, ktime_set(2, 0), HRTIMER_MODE_REL);

	printk("[TSP] %s-\n", __func__ );
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cytma340_early_suspend(struct early_suspend *h)
{
	cytma340_suspend(cytma340.client, PMSG_SUSPEND);
}

static void cytma340_late_resume(struct early_suspend *h)
{
	cytma340_resume(cytma340.client);
}
#endif

static void cytma340_device_release(struct device *dev)
{
	/* Nothing */
}

static struct platform_device cytma340_device = {
	.name		= TOUCHSCREEN_NAME,
	.id			= -1,
	.dev = {
		.release 	= cytma340_device_release,
	},
};

static struct platform_driver cytma340_driver = {
	.probe		= cytma340_probe,
	.remove		= cytma340_remove,
	.shutdown	= cytma340_shutdown,	
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= &cytma340_suspend,
	.resume 	= &cytma340_resume,
#endif	
	.driver = {
		.name	= TOUCHSCREEN_NAME,
	},
};

void init_hw_setting(void)
{
	gpio_set_value(OMAP_GPIO_TOUCH_EN, 1);
	gpio_direction_input(OMAP_GPIO_TOUCH_INT);

	printk(KERN_DEBUG "[TSP] cytma340 GPIO Status\n");
	printk(KERN_DEBUG "[TSP] OMAP_GPIO_TOUCH_EN  : %s\n", gpio_get_value(OMAP_GPIO_TOUCH_EN)? "High":"Low");
	printk(KERN_DEBUG "[TSP] OMAP_GPIO_TOUCH_INT : %s\n", gpio_get_value(OMAP_GPIO_TOUCH_INT)? "High":"Low");

	msleep(100);
}

/*****************************************************************************
*
*  FUNCTION
*  PURPOSE
*  INPUT
*  OUTPUT
*                                   
* ***************************************************************************/


int __init cytma340_init(void)
{
	int ret;
	int i=0;

	printk(KERN_INFO "[TSP] %s\n", __FUNCTION__);

	init_hw_setting();

	ret = platform_device_register(&cytma340_device);
	if (ret != 0)
		return -ENODEV;

	ret = platform_driver_register(&cytma340_driver);
	if (ret != 0) {
		platform_device_unregister(&cytma340_device);
		return -ENODEV;
	}

	return 0;
}

void __exit cytma340_exit(void)
{
	platform_driver_unregister(&cytma340_driver);
	platform_device_unregister(&cytma340_device);

	if (cytma340_wq)
		destroy_workqueue(cytma340_wq);

	if (check_ic_wq)
		destroy_workqueue(check_ic_wq);
}
module_init(cytma340_init);
module_exit(cytma340_exit);

MODULE_DESCRIPTION("Cypress Touchscreen Driver");
MODULE_LICENSE("GPL");

