#include <linux/err.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

static ssize_t read_check_wifi_chip(struct file *file,
		char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if(*ppos == 0)
		ret = sprintf(buf, "%s", "bcm4343w semco r720");

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static const struct file_operations check_wifi_chip_fops = {
	.read = read_check_wifi_chip,
};

static struct miscdevice check_wifi_chip = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "check_wifi_chip",
	.fops = &check_wifi_chip_fops,
};

static int __init check_wifi_chip_init(void)
{
	return misc_register(&check_wifi_chip);
}

static void __exit check_wifi_chip_exit(void)
{
	misc_deregister(&check_wifi_chip);
}

module_init(check_wifi_chip_init);
module_exit(check_wifi_chip_exit);
