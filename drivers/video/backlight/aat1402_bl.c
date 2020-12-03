
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/earlysuspend.h>

#include <plat/mux.h>
#include <mach/gpio.h>

#define LCD_DEFAULT_BRIGHTNESS		6
#define LCD_NUMBER_OF_BRIGHTNESS		32	// include dimming level. 
#define LCD_DIMMING_BRIGHTNESS_LEVEL	3
#define LCD_MIN_BRIGHTNESS_LEVEL		17
#define LCD_MAX_BRIGHTNESS_LEVEL		31

#define GPIO_LEVEL_LOW   0
#define GPIO_LEVEL_HIGH  1

static struct platform_device *bl_pdev;
static struct early_suspend	st_early_suspend;

static int curLcdlevel = LCD_DEFAULT_BRIGHTNESS;
static int current_intensity = 0;

static DEFINE_SPINLOCK(aat1402_bl_lock);

static void aat1402_lcd_brightness_setting(int lcdlevel)
{
	int data;
	printk(KERN_INFO " *** aat1402_lcd_brightness_setting : %d\n", lcdlevel);
	while (curLcdlevel != lcdlevel)
	{
		if(curLcdlevel > lcdlevel)
			curLcdlevel --;
		else 
			curLcdlevel++;
		
		for(data=0 ; data<(LCD_NUMBER_OF_BRIGHTNESS-curLcdlevel) ; data++)
		{
			gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_LOW);
			udelay(1);
			gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_HIGH);
			udelay(1);
		}
		mdelay(1);	// T(lat) >= 500us.
//		printk("[LCD-BACKLIGHT] %s :: curLcdlevel=%d, lcdlevel=%d\n",__func__, curLcdlevel, lcdlevel);
	}	
}
static void aat1402_set_brightness(struct platform_device *pdev, int value)
{
	int lcdlevel = 0;

	printk(KERN_DEBUG" *** aat1402_set_brightness : %d\n", value);
    
	if( value < 0 || value > 255 )
		return;
    
	if (value == 255) lcdlevel = LCD_MAX_BRIGHTNESS_LEVEL; 		// Max 300cd
	else 
	if (value < 30) lcdlevel = LCD_DIMMING_BRIGHTNESS_LEVEL;	        // diming, Low BATT 50cd
	else 
		lcdlevel = (((value - 30) * (LCD_MAX_BRIGHTNESS_LEVEL -LCD_MIN_BRIGHTNESS_LEVEL)) /225) + LCD_MIN_BRIGHTNESS_LEVEL;

	aat1402_lcd_brightness_setting(lcdlevel);
	
}

static void aat1402_bl_send_intensity(struct backlight_device *bd)
{
	//unsigned long flags;
	int intensity = bd->props.brightness;
	struct platform_device *pdev = NULL;

	pdev = dev_get_drvdata(&bd->dev);
	if (pdev == NULL) 
		{
		printk(KERN_ERR "[LCD] %s:failed to get platform device.\n", __func__);
		return;
		}
/*
	if (bd->props.power != FB_BLANK_UNBLANK ||
		bd->props.fb_blank != FB_BLANK_UNBLANK) || 
		cmc623_pwm_suspended)
		{
		printk("[cmc]i:%d(c:%d)\n", intensity, current_intensity);
		if(!current_intensity)
			return;
		msleep(1);
		intensity = 0;
	}
*/
	//spin_lock_irqsave(&aat1402_bl_lock, flags);
	spin_lock(&aat1402_bl_lock);

	aat1402_set_brightness(pdev, intensity);

	//spin_unlock_irqrestore(&aat1402_bl_lock, flags);
	spin_unlock(&aat1402_bl_lock);

	current_intensity = intensity;
}

static int aat1402_bl_get_intensity(struct backlight_device *bd)
{
	return current_intensity;
}

static int aat1402_bl_set_intensity(struct backlight_device *bd)
{

	aat1402_bl_send_intensity(bd);

	return 0;
}

static struct backlight_ops aat1402_bl_ops = {
	.get_brightness = aat1402_bl_get_intensity,
	.update_status  = aat1402_bl_set_intensity,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void aat1402_early_suspend(struct early_suspend *h)
{	
	struct backlight_device *bd = platform_get_drvdata(bl_pdev);
	
	printk("[LCD] %s +\n", __func__);  
	gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_LOW);
	
	return 0;
}

static void aat1402_late_resume(struct early_suspend *h)
{
	struct backlight_device *bd = platform_get_drvdata(bl_pdev);
	
	printk("[LCD] %s +\n", __func__);
	aat1402_bl_send_intensity(bd);
	return 0;
}
#else
static int aat1402_suspend(struct platform_device *dev, pm_message_t state)
{  
	struct backlight_device *bd = to_backlight_device(dev);
	
	printk("[LCD] %s +\n", __func__);  
	gpio_set_value(OMAP_GPIO_LCD_EN_SET, GPIO_LEVEL_LOW);
	
	return 0;
}

static int aat1402_resume(struct platform_device *dev)
{
	struct backlight_device *bd = to_backlight_device(dev);
	
	printk("[LCD] %s +\n", __func__);
	aat1402_bl_send_intensity(bd);
	return 0;
}
#endif

static int aat1402_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;

	printk("[LCD] %s +\n", __func__);

	bd = backlight_device_register("omap_bl", &pdev->dev, pdev, &aat1402_bl_ops);

	if (IS_ERR(bd))
		return PTR_ERR(bd);

	platform_set_drvdata(pdev, bd);

	bd->props.max_brightness = 255;
	bd->props.brightness = (LCD_DEFAULT_BRIGHTNESS*225/(LCD_MAX_BRIGHTNESS_LEVEL-LCD_MIN_BRIGHTNESS_LEVEL))+30;

	bl_pdev = pdev;

#ifdef CONFIG_HAS_EARLYSUSPEND
	st_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	st_early_suspend.suspend = aat1402_early_suspend;
	st_early_suspend.resume = aat1402_late_resume;
	register_early_suspend(&st_early_suspend);
#endif	/* CONFIG_HAS_EARLYSUSPEND */

	printk("[LCD] %s -\n", __func__);
	return 0;

}

static int aat1402_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&st_early_suspend);
#endif

	bd->props.brightness = 0;
	bd->props.power = 0;
	aat1402_bl_send_intensity(bd);

	backlight_device_unregister(bd);

	return 0;
}

static struct platform_driver aat1402_driver = {
	.probe		= aat1402_probe,
	.remove		= aat1402_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND	
	.suspend        = aat1402_suspend,
	.resume         = aat1402_resume,
#endif	
	.driver		= {
		.name		= "aat1402-leds",
		.owner		= THIS_MODULE,
	},
};

static int __init aat1402_led_init(void)
{
	return platform_driver_register(&aat1402_driver);
}

static void __exit aat1402_led_exit(void)
{
	platform_driver_unregister(&aat1402_driver);
}

module_init(aat1402_led_init);
module_exit(aat1402_led_exit);

MODULE_DESCRIPTION("AAT1402 LED driver");
MODULE_LICENSE("GPL");


