#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>
#include <plat/devs.h>
#include <plat/gpio-cfg.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/input.h>
#include <plat/udc-hs.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#ifdef CONFIG_MFD_MAX77804K
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77804k-private.h>
#else
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#endif
#endif

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/gpio.h>

#include <linux/power_supply.h>

#include <linux/notifier.h>
#include "board-universal5260.h"

#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/host_notify.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>

#ifdef CONFIG_JACK_MON
#include <linux/jack.h>
#endif

#include <mach/usb3-drd.h>

#ifdef CONFIG_VIDEO_MHL_SII8246
#include <linux/sii8246.h>
#endif

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI
#include <linux/i2c/synaptics_rmi.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXTS
#include <linux/i2c/mxts.h>
#endif
#ifdef CONFIG_TOUCHSCREEN_MELFAS_M2
#include <linux/platform_data/mms152_ts.h>
#endif

#include <linux/pm_qos.h>

struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

struct pm_qos_request arm_cpu_max;
struct pm_qos_request kfc_cpu_max;

static void cpu_max_throttling(int cable_type, int previous_cable_type)
{
	if (cable_type == CABLE_TYPE_TA_MUIC) {
		if (pm_qos_request_active(&arm_cpu_max))
			pm_qos_update_request(&arm_cpu_max, 1600000);
		else
			pm_qos_add_request(&arm_cpu_max, PM_QOS_CPU_FREQ_MAX, 1600000);

		if (pm_qos_request_active(&kfc_cpu_max))
			pm_qos_update_request(&kfc_cpu_max, 1200000);
		else
			pm_qos_add_request(&kfc_cpu_max, PM_QOS_KFC_FREQ_MAX, 1200000);
	}
	else if (previous_cable_type == CABLE_TYPE_TA_MUIC) {
		if (pm_qos_request_active(&arm_cpu_max))
			pm_qos_update_request(&arm_cpu_max, PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);
		else
			pm_qos_add_request(&arm_cpu_max, PM_QOS_CPU_FREQ_MAX, PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE);

		if (pm_qos_request_active(&kfc_cpu_max))
			pm_qos_update_request(&kfc_cpu_max, PM_QOS_KFC_FREQ_MAX_DEFAULT_VALUE);
		else
			pm_qos_add_request(&kfc_cpu_max, PM_QOS_KFC_FREQ_MAX, PM_QOS_KFC_FREQ_MAX_DEFAULT_VALUE);

	}
}

#ifdef SYNAPTICS_RMI_INFORM_CHARGER
struct synaptics_rmi_callbacks *charger_callbacks;
void synaptics_tsp_charger_infom(int cable_type)
{
	if (charger_callbacks && charger_callbacks->inform_charger)
		charger_callbacks->inform_charger(charger_callbacks, cable_type);
}
void synaptics_tsp_register_callback(struct synaptics_rmi_callbacks *cb)
{
	charger_callbacks = cb;
	pr_info("%s: [synaptics] charger callback!\n", __func__);
}
#endif

/* charger cable state */
bool is_cable_attached;
bool is_jig_attached;

#ifdef CONFIG_MFD_MAX77803
extern int g_usbvbus;
#endif
static ssize_t switch_show_vbus(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int i;
	struct regulator *regulator;

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	if (regulator_is_enabled(regulator))
		i = sprintf(buf, "VBUS is enabled\n");
	else
		i = sprintf(buf, "VBUS is disabled\n");
	regulator_put(regulator);

	return i;
}

static ssize_t switch_store_vbus(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int disable, ret, usb_mode;
	struct regulator *regulator;
	/* struct s3c_udc *udc = platform_get_drvdata(&s3c_device_usbgadget); */

	if (!strncmp(buf, "0", 1))
		disable = 0;
	else if (!strncmp(buf, "1", 1))
		disable = 1;
	else {
		pr_warn("%s: Wrong command\n", __func__);
		return count;
	}

	pr_info("%s: disable=%d\n", __func__, disable);
	usb_mode =
	    disable ? USB_CABLE_DETACHED_WITHOUT_NOTI : USB_CABLE_ATTACHED;
	/* ret = udc->change_usb_mode(usb_mode); */
	ret = -1;
	if (ret < 0)
		pr_err("%s: fail to change mode!!!\n", __func__);

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return count;
	}

	if (disable) {
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	} else {
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
	}
	regulator_put(regulator);

	return count;
}

DEVICE_ATTR(disable_vbus, 0664, switch_show_vbus,
	    switch_store_vbus);

static int __init sec_switch_init(void)
{
	int ret = 0;
	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev)) {
		pr_err("%s:%s= Failed to create device(switch)!\n",
				__FILE__, __func__);
		return -ENODEV;
	}

	ret = device_create_file(switch_dev, &dev_attr_disable_vbus);
	if (ret)
		pr_err("%s:%s= Failed to create device file(disable_vbus)!\n",
				__FILE__, __func__);

	return ret;
};

int current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
#ifdef CONFIG_MFD_MAX77803
int max77803_muic_charger_cb(enum cable_type_muic cable_type)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct power_supply *psy_p = power_supply_get_by_name("ps");
	union power_supply_propval value;
	static enum cable_type_muic previous_cable_type = CABLE_TYPE_NONE_MUIC;

	pr_info("[BATT] CB enabled(%d), prev_cable(%d)\n", cable_type, previous_cable_type);

	/* others setting */
	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
	case CABLE_TYPE_OTG_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_MUIC:
	case CABLE_TYPE_MHL_MUIC:
	case CABLE_TYPE_DESKDOCK_MUIC:
	case CABLE_TYPE_PS_CABLE_MUIC:
		is_cable_attached = false;
		break;
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB_OFF_MUIC:
	case CABLE_TYPE_JIG_USB_ON_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_MHL_VB_MUIC:
		is_cable_attached = true;
		break;
	case CABLE_TYPE_TA_MUIC:
	case CABLE_TYPE_CARDOCK_MUIC:
	case CABLE_TYPE_DESKDOCK_TA_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
	case CABLE_TYPE_AUDIODOCK_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_VB_MUIC:
	case CABLE_TYPE_CDP_MUIC:
		is_cable_attached = true;
		break;
	default:
		pr_err("%s: invalid type:%d\n", __func__, cable_type);
		return -EINVAL;
	}

#ifdef SYNAPTICS_RMI_INFORM_CHARGER
	synaptics_tsp_charger_infom(cable_type);
#endif

	/*  charger setting */
	if (previous_cable_type == cable_type) {
		pr_info("%s: SKIP cable setting\n", __func__);
		goto skip_cable_setting;
	}

	switch (cable_type) {
	case CABLE_TYPE_NONE_MUIC:
	case CABLE_TYPE_JIG_UART_OFF_MUIC:
	case CABLE_TYPE_DESKDOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case CABLE_TYPE_MHL_VB_MUIC:
		if (lpcharge)
			current_cable_type = POWER_SUPPLY_TYPE_USB;
		else
			goto skip;
		break;
	case CABLE_TYPE_MHL_MUIC:
		if (lpcharge) {
			current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		} else {
			goto skip;
		}
		break;
	case CABLE_TYPE_OTG_MUIC:
		goto skip;
	case CABLE_TYPE_USB_MUIC:
	case CABLE_TYPE_JIG_USB_OFF_MUIC:
	case CABLE_TYPE_JIG_USB_ON_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case CABLE_TYPE_JIG_UART_OFF_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case CABLE_TYPE_TA_MUIC:
	case CABLE_TYPE_CARDOCK_MUIC:
	case CABLE_TYPE_DESKDOCK_TA_MUIC:
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case CABLE_TYPE_AUDIODOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MISC;
		break;
	case CABLE_TYPE_CDP_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case CABLE_TYPE_PS_CABLE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_POWER_SHARING;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, cable_type);
		goto skip;
	}

	if (!psy || !psy->set_property || !psy_p || !psy_p->set_property) {
		pr_err("%s: fail to get battery/ps psy\n", __func__);
	} else {
		if (current_cable_type == POWER_SUPPLY_TYPE_POWER_SHARING) {
			value.intval = current_cable_type;
			psy_p->set_property(psy_p, POWER_SUPPLY_PROP_ONLINE, &value);
		} else {
			if (previous_cable_type == CABLE_TYPE_PS_CABLE_MUIC) {
				value.intval = current_cable_type;
				psy_p->set_property(psy_p, POWER_SUPPLY_PROP_ONLINE, &value);
			}
			value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT;
			psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		}
	}
#ifdef CONFIG_TOUCHSCREEN_MELFAS_M2
	tsp_charger_infom(is_cable_attached);
#endif
cpu_max_throttling(cable_type, previous_cable_type);
skip:
	previous_cable_type = cable_type;
skip_cable_setting:
#ifdef CONFIG_JACK_MON
	jack_event_handler("charger", is_cable_attached);
#endif

	return 0;
}

int max77803_get_jig_state(void)
{
	return is_jig_attached;
}
EXPORT_SYMBOL(max77803_get_jig_state);

void max77803_set_jig_state(int jig_state)
{
	is_jig_attached = jig_state;
}

static void max77803_check_id_state(int state)
{
	pr_info("%s: id state = %d\n", __func__, state);

	exynos_drd_switch_id_event(&exynos5_device_usb3_drd0, state);
}

static void max77803_set_vbus_state(int state)
{
	pr_info("%s: vbus state = %d\n", __func__, state);

	exynos_drd_switch_vbus_event(&exynos5_device_usb3_drd0, state);
}

/* usb cable call back function */
void max77803_muic_usb_cb(u8 usb_mode)
{
#ifdef CONFIG_USB_HOST_NOTIFY
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;
#endif
	printk(KERN_ERR "%s - %d\n", __func__, __LINE__);
	if (usb_mode == USB_CABLE_ATTACHED) {
#ifdef CONFIG_MFD_MAX77803
		g_usbvbus = USB_CABLE_ATTACHED;
#endif
		max77803_set_vbus_state(USB_CABLE_ATTACHED);
		pr_err("%s - USB_CABLE_ATTACHED\n", __func__);
	} else if (usb_mode == USB_CABLE_DETACHED) {
#ifdef CONFIG_MFD_MAX77803
		g_usbvbus = USB_CABLE_DETACHED;
#endif
		max77803_set_vbus_state(USB_CABLE_DETACHED);
		pr_err("%s - USB_CABLE_DETACHED\n", __func__);
	} else if (usb_mode == USB_OTGHOST_ATTACHED) {
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->booster(1);
		host_noti_pdata->ndev.mode = NOTIFY_HOST_MODE;
		if (host_noti_pdata->usbhostd_start)
			host_noti_pdata->usbhostd_start();
		/* defense code for otg mis-detecing issue */
		msleep(50);
#endif
		max77803_check_id_state(0);
		pr_err("%s - USB_OTGHOST_ATTACHED\n", __func__);
	} else if (usb_mode == USB_OTGHOST_DETACHED) {
		max77803_check_id_state(1);
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->ndev.mode = NOTIFY_NONE_MODE;
		if (host_noti_pdata->usbhostd_stop)
			host_noti_pdata->usbhostd_stop();
		host_noti_pdata->booster(0);
#endif
		pr_err("%s - USB_OTGHOST_DETACHED\n", __func__);
	} else if (usb_mode == USB_POWERED_HOST_ATTACHED) {
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->powered_booster(1);
		start_usbhostd_wakelock();
#endif
		max77803_check_id_state(0);
		pr_info("%s - USB_POWERED_HOST_ATTACHED\n", __func__);
	} else if (usb_mode == USB_POWERED_HOST_DETACHED) {
		max77803_check_id_state(1);
#ifdef CONFIG_USB_HOST_NOTIFY
		host_noti_pdata->powered_booster(0);
		stop_usbhostd_wakelock();
#endif
		pr_info("%s - USB_POWERED_HOST_DETACHED\n", __func__);
	}
}

#if defined(CONFIG_MUIC_MAX77803_SUPPORT_MHL_CABLE_DETECTION)
void max77803_muic_mhl_cb(int attached)
{
	if (attached == MAX77803_MUIC_ATTACHED) {
		pr_info("MHL Attached !!\n");
		acc_notify(MHL_ATTACHED);
	} else {
		pr_info("MHL Detached !!\n");
		acc_notify(MHL_DETACHED);
	}
}
#endif

#if defined(CONFIG_MUIC_MAX77803_SUPPORT_MHL_CABLE_DETECTION)
bool max77803_muic_is_mhl_attached(void)
{
	return true;
}
#endif

void max77803_muic_dock_cb(int type)
{

#ifdef CONFIG_JACK_MON
	jack_event_handler("cradle", type);
#endif
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif

}

void max77803_muic_init_cb(void)
{
//#if 0 //CHECK ME!!
#ifdef CONFIG_SWITCH
	int ret;

	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("Failed to register dock switch. %d\n", ret);
#endif
//#endif //CHECK ME!!
}

int max77803_muic_cfg_uart_gpio(void)
{
	return UART_PATH_AP;
}

void max77803_muic_jig_uart_cb(int path)
{
//#if 0 //CHECK ME!!
	switch (path) {
	case UART_PATH_AP:
		break;
	case UART_PATH_CP:
		break;
	default:
		pr_info("func %s: invalid value!!\n", __func__);
	}
//#endif //CHECK ME!!
}

#if defined(CONFIG_MUIC_DET_JACK)
void max77803_muic_earjack_cb(int attached)
{

}
void max77803_muic_earjackkey_cb(int pressed, unsigned int code)
{

}
#endif

#ifdef CONFIG_USB_HOST_NOTIFY
int max77803_muic_host_notify_cb(int enable)
{
	struct host_notifier_platform_data *host_noti_pdata =
	    host_notifier_device.dev.platform_data;

	struct host_notify_dev *ndev = &host_noti_pdata->ndev;

	if (!ndev) {
		pr_err("%s: ndev is null.\n", __func__);
		return -1;
	}

	ndev->booster = enable ? NOTIFY_POWER_ON : NOTIFY_POWER_OFF;
	return ndev->mode;
}
#endif

int max77803_muic_set_safeout(int path)
{
	struct regulator *regulator;

	if (path == CP_USB_MODE) {
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		/* AP_USB_MODE || AUDIO_MODE */
		regulator = regulator_get(NULL, "safeout1");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (!regulator_is_enabled(regulator))
			regulator_enable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "safeout2");
		if (IS_ERR(regulator))
			return -ENODEV;
		if (regulator_is_enabled(regulator))
			regulator_force_disable(regulator);
		regulator_put(regulator);
	}

	return 0;
}

struct max77803_muic_data max77803_muic = {
	.usb_cb = max77803_muic_usb_cb,
	.charger_cb = max77803_muic_charger_cb,
#if defined(CONFIG_MUIC_MAX77803_SUPPORT_MHL_CABLE_DETECTION)
	.mhl_cb = max77803_muic_mhl_cb,
	.is_mhl_attached = max77803_muic_is_mhl_attached,
#endif
	.set_safeout = max77803_muic_set_safeout,
	.init_cb = max77803_muic_init_cb,
	.dock_cb = max77803_muic_dock_cb,
	.cfg_uart_gpio = max77803_muic_cfg_uart_gpio,
	.jig_uart_cb = max77803_muic_jig_uart_cb,
#if defined(CONFIG_MUIC_DET_JACK)
	.earjack_cb = max77803_muic_earjack_cb,
	.earjackkey_cb = max77803_muic_earjackkey_cb,
#endif
#ifdef CONFIG_USB_HOST_NOTIFY
	.host_notify_cb = max77803_muic_host_notify_cb,
#else
	.host_notify_cb = NULL,
#endif
	/* .gpio_usb_sel = GPIO_USB_SEL, */
	.gpio_usb_sel = -1,
	.jig_state = max77803_set_jig_state,
	.check_id_state = max77803_check_id_state,
};
#endif

device_initcall(sec_switch_init);
