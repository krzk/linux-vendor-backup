/* Bluetooth HCI driver model support. */

#include <linux/module.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h>

#if defined(CONFIG_BT_DQA_SUPPORT) && defined(CONFIG_TIZEN_WIP)
extern int bt_dev_and_host_wake_verbose;
#endif

static struct class *bt_class;

static void bt_link_release(struct device *dev)
{
	struct hci_conn *conn = to_hci_conn(dev);
	kfree(conn);
}

static struct device_type bt_link = {
	.name    = "link",
	.release = bt_link_release,
};

/*
 * The rfcomm tty device will possibly retain even when conn
 * is down, and sysfs doesn't support move zombie device,
 * so we should move the device before conn device is destroyed.
 */
static int __match_tty(struct device *dev, void *data)
{
	return !strncmp(dev_name(dev), "rfcomm", 6);
}

void hci_conn_init_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	BT_DBG("conn %p", conn);

	conn->dev.type = &bt_link;
	conn->dev.class = bt_class;
	conn->dev.parent = &hdev->dev;

	device_initialize(&conn->dev);
}

void hci_conn_add_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

#if defined(CONFIG_BT_DQA_SUPPORT) && defined(CONFIG_TIZEN_WIP)
	if(conn->type != INVALID_LINK)
	{
		if(conn->type == LE_LINK)
			hdev->le_connect_cnt++;
		else
			hdev->bredr_connect_cnt++;
	}
#endif

	BT_DBG("conn %p", conn);

	dev_set_name(&conn->dev, "%s:%d", hdev->name, conn->handle);

	if (device_add(&conn->dev) < 0) {
		BT_ERR("Failed to register connection device");
		return;
	}

	hci_dev_hold(hdev);
}

void hci_conn_del_sysfs(struct hci_conn *conn)
{
	struct hci_dev *hdev = conn->hdev;

	if (!device_is_registered(&conn->dev))
		return;

#if defined(CONFIG_BT_DQA_SUPPORT) && defined(CONFIG_TIZEN_WIP)
	if(conn->type != INVALID_LINK)
	{
		if(conn->type == LE_LINK)
			hdev->le_disconnect_cnt++;
		else
			hdev->bredr_disconnect_cnt++;
	}
#endif

	while (1) {
		struct device *dev;

		dev = device_find_child(&conn->dev, NULL, __match_tty);
		if (!dev)
			break;
		device_move(dev, NULL, DPM_ORDER_DEV_LAST);
		put_device(dev);
	}

	device_del(&conn->dev);

	hci_dev_put(hdev);
}

#if defined(CONFIG_BT_DQA_SUPPORT) && defined(CONFIG_TIZEN_WIP)
/* controller */
static ssize_t show_controller(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hci_dev *hdev = to_hci_dev(dev);

#if 0
	//le_connect_cnt, le_disconnect_cnt, bredr_connect_cnt, bredr_disconnect_cnt,
	//hci_cmd_tx_timeout_cnt, hci_cmd_tx_timeout_opcode, hci_ev_hardware_error_cnt, hci_ev_hardware_error_code,
	//manufacturer, lmp_ver, lmp_subver
	return sprintf(buf, "%u %u %u %u %u 0x%04x %u 0x%02x %u 0x%x 0x%x\n%s\n", hdev->le_connect_cnt, hdev->le_disconnect_cnt, hdev->bredr_connect_cnt, hdev->bredr_disconnect_cnt,
									hdev->hci_cmd_tx_timeout_cnt, hdev->hci_cmd_tx_timeout_opcode, hdev->hci_ev_hardware_error_cnt, hdev->hci_ev_hardware_error_code,
									hdev->manufacturer, hdev->lmp_ver, hdev->lmp_subver,
									"<LE conn, LE disconn, EDR conn, EDR disconn, HCI command Tx timeout, opcode, HCI event hardware error, error code, Manufacturer, LMP Version, LMP Subversion>");
#else
	return sprintf(buf, "\"LO_MFN\":\"%u\",\"LO_LMP\":\"%u\",\"LO_SUB\":\"%u\"\n", hdev->manufacturer, hdev->lmp_ver, hdev->lmp_subver);
#endif
}

static DEVICE_ATTR(controller, S_IRUGO, show_controller, NULL);

/* verbose */
static ssize_t show_verbose(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bt_dev_and_host_wake_verbose);
}

static ssize_t store_verbose(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &bt_dev_and_host_wake_verbose);

	return count;
}

static DEVICE_ATTR(verbose, 0664, show_verbose, store_verbose);

static struct attribute *bt_host_attrs[] = {
	&dev_attr_controller.attr,
	&dev_attr_verbose.attr,
	NULL
};

ATTRIBUTE_GROUPS(bt_host);
#endif

static void bt_host_release(struct device *dev)
{
	struct hci_dev *hdev = to_hci_dev(dev);
	kfree(hdev);
	module_put(THIS_MODULE);
}

static struct device_type bt_host = {
	.name    = "host",
#if defined(CONFIG_BT_DQA_SUPPORT) && defined(CONFIG_TIZEN_WIP)
	.groups  = bt_host_groups,
#endif
	.release = bt_host_release,
};

void hci_init_sysfs(struct hci_dev *hdev)
{
	struct device *dev = &hdev->dev;

	dev->type = &bt_host;
	dev->class = bt_class;

	__module_get(THIS_MODULE);
	device_initialize(dev);
}

int __init bt_sysfs_init(void)
{
	bt_class = class_create(THIS_MODULE, "bluetooth");

	return PTR_ERR_OR_ZERO(bt_class);
}

void bt_sysfs_cleanup(void)
{
	class_destroy(bt_class);
}
