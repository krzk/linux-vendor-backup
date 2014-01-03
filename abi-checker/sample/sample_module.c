#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/fs.h>

#define DEVICE_NAME "sample_module"
#define TEST_MSG_BUFF_LEN	1024

extern int snd_timer_pause;

static unsigned int majorNumber = 0;

static char message[TEST_MSG_BUFF_LEN + 1];

static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
	printk(KERN_ALERT "Empty write operation. %d\n", snd_timer_pause);
	return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t length, loff_t * offset)
{
	int bytes_read = 0;

	/*
	 * Copy message
	 */
	for(bytes_read = 0; bytes_read < (length - 1) && bytes_read < TEST_MSG_BUFF_LEN; bytes_read ++)
		put_user(message[bytes_read], buffer + bytes_read );
	put_user('\0', buffer + bytes_read );

	return bytes_read + 1;
}

static int device_open(struct inode *inode, struct file *file)
{
	static int counter = 0;

	printk( "Open device, count = %d\n", counter++ );

	try_module_get( THIS_MODULE );
	return 0;
}

static int device_release(struct inode *inode, struct file *file)
{
	printk("Release device\n");

	module_put( THIS_MODULE );
	return 0;
}

struct file_operations fileOperations =
{
	read:    device_read,
	write:   device_write,
	open:    device_open,
	release: device_release
};

static int __init test_module_init( void )
{
	int i;
	int j;

	printk(KERN_ALERT "Test kernel module 2 - enter\n");

	/*
	 * Init test message
	 */
	for(i = 0; i < TEST_MSG_BUFF_LEN;)
	{
		for(j = 0; j < 10 && i < TEST_MSG_BUFF_LEN; j ++, i ++ )
		{
			message[i] = '0' + j;
			message[i + 1] = '\0';
		}
	}

	/*
	 * Register device
	 */
	majorNumber = register_chrdev(0, DEVICE_NAME, &fileOperations);
	if (majorNumber < 0)
	{
		  printk(KERN_ALERT "Blad rejestrowania urzadzenia => [%d]\n", majorNumber );
		  return majorNumber;
	}

	return 0;
}

static void __exit test_module_exit(void)
{
	printk(KERN_ALERT "Test kernel module 2 - exit\n");

	unregister_chrdev(majorNumber, DEVICE_NAME);
}

module_init(test_module_init);
module_exit(test_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jacek Pielaszkiewicz");	/* Who wrote this module? */
MODULE_DESCRIPTION("Sample test kernel module.");	/* What does this module do */
MODULE_SUPPORTED_DEVICE("sample_test_module_2");
