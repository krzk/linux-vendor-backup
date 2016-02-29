/*
** =========================================================================
** File:
**     tspdrv.c
**
** Description:
**     TouchSense Kernel Module main entry-point.
**
** Portions Copyright (c) 2008-2013 Immersion Corporation. All Rights Reserved.
**
** This file contains Original Code and/or Modifications of Original Code
** as defined in and that are subject to the GNU Public License v2 -
** (the 'License'). You may not use this file except in compliance with the
** License. You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
** TouchSenseSales@immersion.com.
**
** The Original Code and all software distributed under the License are
** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
** the License for the specific language governing rights and limitations
** under the License.
** =========================================================================
*/

#ifdef CONFIG_MACH_WC1
#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/io.h>
#include <mach/map.h>
#include "tspdrv.h"
#ifdef CONFIG_MOTOR_DRV_ISA1200
#include "ImmVibeSPI_isa1200.c"
#elif defined(CONFIG_MOTOR_DRV_ISA1400)
#include "ImmVibeSPI_isa1400.c"
#else
#include "ImmVibeSPI.c"
#endif
#if defined(VIBE_DEBUG) && defined(VIBE_RECORD)
#include <tspdrvRecorder.c>
#endif

/* Device name and version information */

/* DO NOT CHANGE - this is auto-generated */
#define VERSION_STR " v3.4.55.8\n"

/* account extra space for future extra digits in version number */
#define VERSION_STR_LEN 16

/* initialized in init_module */
static char g_szDeviceName[(VIBE_MAX_DEVICE_NAME_LENGTH
							+ VERSION_STR_LEN)
							* NUM_ACTUATORS];
/* initialized in init_module */
static size_t g_cchDeviceName;

static struct wake_lock vib_wake_lock;

/* Flag indicating whether the driver is in use */
static char g_bIsPlaying;

/* Buffer to store data sent to SPI */
#define SPI_BUFFER_SIZE (NUM_ACTUATORS * \
	(VIBE_OUTPUT_SAMPLE_SIZE + SPI_HEADER_SIZE))
static int g_bStopRequested;
static actuator_samples_buffer g_SamplesBuffer[NUM_ACTUATORS] = { {0} };
static char g_cWriteBuffer[SPI_BUFFER_SIZE];

/* #define VIBE_TUNING */
/* #define VIBE_ENABLE_SYSTEM_TIMER */
/* #define IMPLEMENT_AS_CHAR_DRIVER */

/* For QA purposes */
#ifdef QA_TEST
#define FORCE_LOG_BUFFER_SIZE   128
#define TIME_INCREMENT          5
static int g_nTime;
static int g_nForceLogIndex;
static VibeInt8 g_nForceLog[FORCE_LOG_BUFFER_SIZE];
#endif

#if ((LINUX_VERSION_CODE & 0xFFFF00) < KERNEL_VERSION(2, 6, 0))
#error Unsupported Kernel version
#endif

#ifdef IMPLEMENT_AS_CHAR_DRIVER
static int g_nMajor;
#endif

/* Needs to be included after the global variables because it uses them */
#ifdef CONFIG_HIGH_RES_TIMERS
#include "VibeOSKernelLinuxHRTime.c"
#else
#include "VibeOSKernelLinuxTime.c"
#endif

/* File IO */
static int open(struct inode *inode, struct file *file);
static int release(struct inode *inode, struct file *file);
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t write(struct file *file, const char *buf, size_t count,
	loff_t *ppos);
static long unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);
static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = read,
	.write = write,
	.unlocked_ioctl = unlocked_ioctl,
	.open = open,
	.release = release,
	.llseek =	default_llseek
};

#ifndef IMPLEMENT_AS_CHAR_DRIVER
static struct miscdevice miscdev = {
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     MODULE_NAME,
	.fops =     &fops
};
#endif

static int suspend(struct platform_device *pdev, pm_message_t state);
static int resume(struct platform_device *pdev);
static struct platform_driver platdrv = {
	.suspend =  suspend,
	.resume =   resume,
	.driver = {
		.name = MODULE_NAME,
	},
};

static void platform_release(struct device *dev);
static struct platform_device platdev = {
	.name =     MODULE_NAME,
	/* means that there is only one device */
	.id =       -1,
	.dev = {
		.platform_data = NULL,
		/* a warning is thrown during rmmod if this is absent */
		.release = platform_release,
	},
};

/* Module info */
MODULE_AUTHOR("Immersion Corporation");
MODULE_DESCRIPTION("TouchSense Kernel Module");
MODULE_LICENSE("GPL v2");

#ifdef VIBE_ENABLE_SYSTEM_TIMER
int vibetonz_clk_on(struct device *dev)
{
	struct clk *vibetonz_clk = NULL;
    /*printk(KERN_ERR"[VIBRATOR]vibetonz_clk_on is called\n");*/
	vibetonz_clk = clk_get(dev, "timers");
	if (IS_ERR(vibetonz_clk)) {
		printk(KERN_ERR "tspdrv: failed to get clock for vibetonz\n");
		goto err_clk0;
	}
	clk_enable(vibetonz_clk);
	clk_put(vibetonz_clk);

	return 0;

err_clk0:
	clk_put(vibetonz_clk);
	return -EINVAL;
}

int vibetonz_clk_off(struct device *dev)
{
	struct clk *vibetonz_clk = NULL;

	vibetonz_clk = clk_get(dev, "timers");
	if (IS_ERR(vibetonz_clk)) {
		DbgOut((KERN_ERR "tspdrv: failed to get clock for vibetonz\n"));
		goto err_clk0;
	}
	clk_disable(vibetonz_clk);
	clk_put(vibetonz_clk);

	return 0;

err_clk0:
	clk_put(vibetonz_clk);

	return -EINVAL;
}
#else
int vibetonz_clk_on(struct device *dev)
{
	return -EINVAL;
}

int vibetonz_clk_off(struct device *dev)
{
	return -EINVAL;
}
#endif	/* VIBE_ENABLE_SYSTEM_TIMER */

int init_module(void)
{
	int nRet, i;   /* initialized below */
	nRet = 0;

#ifdef IMPLEMENT_AS_CHAR_DRIVER
	printk(KERN_ERR
		"[VIBRATOR]IMPLEMENT_AS_CHAR_DRIVER\n");
	g_nMajor = register_chrdev(0, MODULE_NAME, &fops);
	if (g_nMajor < 0) {
		printk(KERN_ERR"[VIBRATOR]tspdrv: can't get major number.\n");
		return g_nMajor;
	}
#else
	nRet = misc_register(&miscdev);
	if (nRet) {
		printk(KERN_ERR "[VIBRATOR]tspdrv: misc_register failed.\n");
		return nRet;
	}
#endif

	nRet = platform_device_register(&platdev);
	if (nRet) {
		printk(KERN_ERR "tspdrv: platform_device_register failed.\n");
		goto err_platform_dev_reg;
	}

	nRet = platform_driver_register(&platdrv);
	if (nRet) {
		printk(KERN_ERR "tspdrv: platform_driver_register failed.\n");
		goto err_platform_drv_reg;
	}

	DbgRecorderInit(());

	vibetonz_clk_on(&platdev.dev);

	ImmVibeSPI_ForceOut_Initialize();
	VibeOSKernelLinuxInitTimer();

	/* Get and concatenate device name and initialize data buffer */
	g_cchDeviceName = 0;
	for (i = 0; i < NUM_ACTUATORS; i++) {
		char *szName = g_szDeviceName + g_cchDeviceName;
		ImmVibeSPI_Device_GetName(i, szName,
			VIBE_MAX_DEVICE_NAME_LENGTH);

		/* Append version information and get buffer length */
		strcat(szName, VERSION_STR);
		g_cchDeviceName += strlen(szName);

		g_SamplesBuffer[i].nIndexPlayingBuffer = -1; /* Not playing */
		g_SamplesBuffer[i].actuatorSamples[0].nBufferSize = 0;
		g_SamplesBuffer[i].actuatorSamples[1].nBufferSize = 0;
	}

	wake_lock_init(&vib_wake_lock, WAKE_LOCK_SUSPEND, "vib_present");
	return 0;

err_platform_drv_reg:
	platform_device_unregister(&platdev);
err_platform_dev_reg:
#ifdef IMPLEMENT_AS_CHAR_DRIVER
	unregister_chrdev(g_nMajor, MODULE_NAME);
#else
	misc_deregister(&miscdev);
#endif
	return nRet;
}

void cleanup_module(void)
{
	DbgOut((KERN_INFO "tspdrv: cleanup_module.\n"));

	DbgRecorderTerminate(());

	VibeOSKernelLinuxTerminateTimer();
	ImmVibeSPI_ForceOut_Terminate();

	platform_driver_unregister(&platdrv);
	platform_device_unregister(&platdev);

#ifdef IMPLEMENT_AS_CHAR_DRIVER
	unregister_chrdev(g_nMajor, MODULE_NAME);
#else
	misc_deregister(&miscdev);
#endif
	wake_lock_destroy(&vib_wake_lock);
}

static int open(struct inode *inode, struct file *file)
{
	DbgOut((KERN_INFO "tspdrv: open.\n"));

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	return 0;
}

static int release(struct inode *inode, struct file *file)
{
	DbgOut((KERN_INFO "tspdrv: release.\n"));

	/*
	** Reset force and stop timer when the driver is closed, to make sure
	** no dangling semaphore remains in the system, especially when the
	** driver is run outside of immvibed for testing purposes.
	*/
	VibeOSKernelLinuxStopTimer();

	/*
	** Clear the variable used to store the magic number to prevent
	** unauthorized caller to write data. TouchSense service is the only
	** valid caller.
	*/
	file->private_data = (void *)NULL;

	module_put(THIS_MODULE);

	return 0;
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	const size_t nBufSize = (g_cchDeviceName > (size_t)(*ppos)) ?
		min(count, g_cchDeviceName - (size_t)(*ppos)) : 0;

	/* End of buffer, exit */
	if (0 == nBufSize)
		return 0;

	if (0 != copy_to_user(buf, g_szDeviceName + (*ppos), nBufSize))	{
		/* Failed to copy all the data, exit */
		DbgOut((KERN_ERR "tspdrv: copy_to_user failed.\n"));
		return 0;
	}

	/* Update file position and return copied buffer size */
	*ppos += nBufSize;
	return nBufSize;
}

static ssize_t write(struct file *file, const char *buf, size_t count,
	loff_t *ppos)
{
	int i = 0;

	*ppos = 0;  /* file position not used, always set to 0 */

	/*
	** Prevent unauthorized caller to write data.
	** TouchSense service is the only valid caller.
	*/
	if (file->private_data != (void *)TSPDRV_MAGIC_NUMBER) {
		DbgOut((KERN_ERR "tspdrv: unauthorized write.\n"));
		return 0;
	}

	/* Check buffer size */
	if ((count < SPI_HEADER_SIZE) || (count > SPI_BUFFER_SIZE)) {
		DbgOut((KERN_ERR "tspdrv: invalid write buffer size.\n"));
		return 0;
	}

	if (count == SPI_HEADER_SIZE) {
		g_bOutputDataBufferEmpty = 1;
	} else {
		g_bOutputDataBufferEmpty = 0;
	}

	/* Copy immediately the input buffer */
	if (0 != copy_from_user(g_cWriteBuffer, buf, count)) {
		/* Failed to copy all the data, exit */
		DbgOut((KERN_ERR "tspdrv: copy_from_user failed.\n"));
		return 0;
	}

	while (i < count) {
		int nIndexFreeBuffer;   /* initialized below */

		samples_buffer *pInputBuffer =	(samples_buffer *)
			(&g_cWriteBuffer[i]);

		if ((i + SPI_HEADER_SIZE) > count) {
			/*
			** Index is about to go beyond the buffer size.
			** (Should never happen).
			*/
			DbgOut((KERN_EMERG "tspdrv: invalid buffer index.\n"));
			return 0;
		}

		/* Check bit depth */
		if (8 != pInputBuffer->nBitDepth)
			DbgOut((KERN_WARNING
				"tspdrv: invalid bit depth."
				"Use default value (8)\n"));

		/* The above code not valid if SPI header size is not 3 */
#if (SPI_HEADER_SIZE != 3)
#error "SPI_HEADER_SIZE expected to be 3"
#endif

		/* Check buffer size */
		if ((i + SPI_HEADER_SIZE + pInputBuffer->nBufferSize) > count) {
			/*
			** Index is about to go beyond the buffer size.
			** (Should never happen).
			*/
			DbgOut((KERN_EMERG "tspdrv: invalid data size.\n"));
			return 0;
		}

		/* Check actuator index */
		if (NUM_ACTUATORS <= pInputBuffer->nActuatorIndex) {
			DbgOut((KERN_ERR "tspdrv: invalid actuator index.\n"));
			i += (SPI_HEADER_SIZE + pInputBuffer->nBufferSize);
			continue;
		}

		if (0 == g_SamplesBuffer[pInputBuffer->nActuatorIndex].
			actuatorSamples[0].nBufferSize) {
			nIndexFreeBuffer = 0;
		} else if (0 == g_SamplesBuffer[pInputBuffer->nActuatorIndex].
			actuatorSamples[1].nBufferSize) {
			nIndexFreeBuffer = 1;
		} else {
			/* No room to store new samples  */
			DbgOut((KERN_ERR
				"tspdrv: no room to store new samples.\n"));
			return 0;
		}

		/* Store the data in the free buffer of the given actuator */
		memcpy(&(g_SamplesBuffer[pInputBuffer->nActuatorIndex].
			actuatorSamples[nIndexFreeBuffer]), &g_cWriteBuffer[i],
			(SPI_HEADER_SIZE + pInputBuffer->nBufferSize));

		/* if the no buffer is playing, prepare to play
		  * g_SamplesBuffer[pInputBuffer->nActuatorIndex].
		  * actuatorSamples[nIndexFreeBuffer] */
		if (-1 == g_SamplesBuffer[pInputBuffer->
			nActuatorIndex].nIndexPlayingBuffer) {
			g_SamplesBuffer[pInputBuffer->nActuatorIndex].
				nIndexPlayingBuffer = nIndexFreeBuffer;
			g_SamplesBuffer[pInputBuffer->nActuatorIndex].
				nIndexOutputValue = 0;
		}

		/* Increment buffer index */
		i += (SPI_HEADER_SIZE + pInputBuffer->nBufferSize);
	}

#ifdef QA_TEST
	g_nForceLog[g_nForceLogIndex++] = g_cSPIBuffer[0];
	if (g_nForceLogIndex >= FORCE_LOG_BUFFER_SIZE) {
		for (i = 0; i < FORCE_LOG_BUFFER_SIZE; i++) {
			printk(KERN_DEBUG "<6>%d\t%d\n",
				g_nTime, g_nForceLog[i]);
			g_nTime += TIME_INCREMENT;
		}
		g_nForceLogIndex = 0;
	}
#endif

	/* Start the timer after receiving new output force */
	g_bIsPlaying = true;
	VibeOSKernelLinuxStartTimer();

	return count;
}

static long unlocked_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
#ifdef QA_TEST
	int i;
#endif

	DbgOut((KERN_INFO "tspdrv: ioctl cmd[0x%x].\n", cmd));

	switch (cmd) {
	case TSPDRV_STOP_KERNEL_TIMER:
		/*
		  * As we send one sample ahead of time,
		  * we need to finish playing the last sample
		  * before stopping the timer. So we just set a flag here.
		  */
		if (true == g_bIsPlaying)
			g_bStopRequested = true;

#ifdef VIBEOSKERNELPROCESSDATA
		/* Last data processing to disable amp and stop timer */
		VibeOSKernelProcessData(NULL);
#endif

#ifdef QA_TEST
		if (g_nForceLogIndex) {
			for (i = 0; i < g_nForceLogIndex; i++) {
				printk(KERN_DEBUG "<6>%d\t%d\n",
					g_nTime, g_nForceLog[i]);
				g_nTime += TIME_INCREMENT;
			}
		}
		g_nTime = 0;
		g_nForceLogIndex = 0;
#endif
		break;

	case TSPDRV_MAGIC_NUMBER:
	case TSPDRV_SET_MAGIC_NUMBER:
		file->private_data = (void *)TSPDRV_MAGIC_NUMBER;
		break;

	case TSPDRV_ENABLE_AMP:
		wake_lock(&vib_wake_lock);
		ImmVibeSPI_ForceOut_AmpEnable(arg);
		DbgRecorderReset((arg));
		DbgRecord((arg, ";------- TSPDRV_ENABLE_AMP ---------\n"));
		break;

	case TSPDRV_DISABLE_AMP:
		/* Small fix for now to handle proper combination of
		  * TSPDRV_STOP_KERNEL_TIMER and TSPDRV_DISABLE_AMP together
		  * If a stop was requested, ignore the request as the amp
		  * will be disabled by the timer proc when it's ready
		  */
#if 0
		if (!g_bStopRequested) {
			ImmVibeSPI_ForceOut_AmpDisable(arg);
#endif
		g_bStopRequested = true;
		/* Last data processing to disable amp and stop timer */
		VibeOSKernelProcessData(NULL);
		g_bIsPlaying = false;
		wake_unlock(&vib_wake_lock);

		break;

	case TSPDRV_GET_NUM_ACTUATORS:
		return NUM_ACTUATORS;
	}

	return 0;
}

static int suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret;

	if (g_bIsPlaying) {
		ret = -EBUSY;
	} else {
		ret = 0;
	}

	DbgOut((KERN_DEBUG "tspdrv: %s (%d).\n", __func__, ret));
	return ret;
}

static int resume(struct platform_device *pdev)
{
	DbgOut((KERN_DEBUG "tspdrv: %s.\n", __func__));
	return 0;
}

static void platform_release(struct device *dev)
{
	DbgOut((KERN_INFO "tspdrv: platform_release.\n"));
}

late_initcall(init_module);
module_exit(cleanup_module);
#else
#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include "tspdrv.h"
#if defined (CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif


static int g_nTimerPeriodMs = 5; /* 5ms timer by default. This variable could be used by the SPI.*/

#ifdef VIBE_RUNTIME_RECORD
/* Flag indicating whether runtime recorder on or off */
static atomic_t g_bRuntimeRecord;
#endif


#define CONFIG_TSPDRV_HISTORY
/*If you need to see detail write data, you can enable below config*/
//#define CONFIG_TSPDRV_HISTORY_DETAIL

#ifdef CONFIG_TSPDRV_HISTORY
#define TSPDRV_HISORY_MAX 20
#define TSPDRV_WRITE_MAX 6000

#define TSPDRV_BUF_SIZE PAGE_SIZE*16


int tspdrv_idx = 0;
int tspdrv_write_idx = 0;
int tspdrv_byte3_idx = 0;
int tspdrv_zero_idx = 0;
int tspdrv_detail_level;

#ifdef CONFIG_DEBUG_FS
	struct dentry			*tspdrv_debugfs_dir;
#endif


struct tspdrv_hisory_t {
	struct timespec ts_enable;
	struct timespec ts_disable;
	int write_count;
	int used;
	int byte3_idx;
	int zero_idx;
#ifdef CONFIG_TSPDRV_HISTORY_DETAIL
	char write_value[TSPDRV_WRITE_MAX];
#endif
};
struct tspdrv_hisory_t tspdrv_history[TSPDRV_HISORY_MAX];
#endif


#include "ImmVibeSPI.c"
#if (defined(VIBE_DEBUG) && defined(VIBE_RECORD)) || defined(VIBE_RUNTIME_RECORD)
#include <tspdrvRecorder.c>
#endif

/* Device name and version information */
#define VERSION_STR " v3.7.11.0\n"

#define VERSION_STR_LEN 16                          /* account extra space for future extra digits in version number */
static char g_szDeviceName[  (VIBE_MAX_DEVICE_NAME_LENGTH
                            + VERSION_STR_LEN)
                            * NUM_ACTUATORS];       /* initialized in init_module */
static size_t g_cchDeviceName;                      /* initialized in init_module */

/* Flag indicating whether the driver is in use */
static char g_bIsPlaying = false;

/* Flag indicating the debug level*/
static atomic_t g_nDebugLevel;


/* Buffer to store data sent to SPI */
#define MAX_SPI_BUFFER_SIZE (NUM_ACTUATORS * (VIBE_OUTPUT_SAMPLE_SIZE + SPI_HEADER_SIZE))

static char g_cWriteBuffer[MAX_SPI_BUFFER_SIZE];


#if ((LINUX_VERSION_CODE & 0xFFFF00) < KERNEL_VERSION(2,6,0))
#error Unsupported Kernel version
#endif

#ifndef HAVE_UNLOCKED_IOCTL
#define HAVE_UNLOCKED_IOCTL 0
#endif

#ifdef IMPLEMENT_AS_CHAR_DRIVER
static int g_nMajor = 0;
#endif



/* Needs to be included after the global variables because they use them */
#include "tspdrvOutputDataHandler.c"
#ifdef CONFIG_HIGH_RES_TIMERS
    #include "VibeOSKernelLinuxHRTime.c"
#else
    #include <VibeOSKernelLinuxTime.c>
#endif

asmlinkage void _DbgOut(int level, const char *fmt,...)
{
    static char printk_buf[MAX_DEBUG_BUFFER_LENGTH];
    static char prefix[6][4] =
        {" * ", " ! ", " ? ", " I ", " V", " O "};

    int nDbgLevel = atomic_read(&g_nDebugLevel);

    if (0 <= level && level <= nDbgLevel) {
        va_list args;
        int ret;
        size_t size = sizeof(printk_buf);

        va_start(args, fmt);

        ret = scnprintf(printk_buf, size, KERN_EMERG "%s:%s %s",
             MODULE_NAME, prefix[level], fmt);
        if (ret < size)
            vprintk(printk_buf, args);

        va_end(args);
    }
}

/* File IO */
static int open(struct inode *inode, struct file *file);
static int release(struct inode *inode, struct file *file);
static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos);
#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif
static struct file_operations fops =
{
    .owner =            THIS_MODULE,
    .read =             read,
    .write =            write,
#if HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl =   unlocked_ioctl,
#else
    .ioctl =            ioctl,
#endif
    .open =             open,
    .release =          release,
    .llseek =           default_llseek    /* using default implementation as declared in linux/fs.h */
};

#ifndef IMPLEMENT_AS_CHAR_DRIVER
static struct miscdevice miscdev =
{
	.minor =    MISC_DYNAMIC_MINOR,
	.name =     MODULE_NAME,
	.fops =     &fops
};
#endif

static int suspend(struct platform_device *pdev, pm_message_t state);
static int resume(struct platform_device *pdev);
static struct platform_driver platdrv =
{
    .suspend =  suspend,
    .resume =   resume,
    .driver =
    {
        .name = MODULE_NAME,
    },
};

static void platform_release(struct device *dev);
static struct platform_device platdev =
{
	.name =     MODULE_NAME,
	.id =       -1,                     /* means that there is only one device */
	.dev =
    {
		.platform_data = NULL,
		.release = platform_release,    /* a warning is thrown during rmmod if this is absent */
	},
};



#ifdef CONFIG_TSPDRV_HISTORY
static int tspdrv_debug_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t tspdrv_debug_read(struct file *filp,
	char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	size_t len = 0;
	ssize_t ret;
	int i;

	if (*ppos != 0)
		return 0;

	if (count < sizeof(buf))
		return -ENOSPC;

	buf = kzalloc(TSPDRV_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	len += snprintf(buf + len, TSPDRV_BUF_SIZE - len, "tspdrv history\n");
	len += snprintf(buf + len, TSPDRV_BUF_SIZE - len, "Total number of request=%d\n", tspdrv_idx);
	len += snprintf(buf + len, TSPDRV_BUF_SIZE - len, "-----------------------------------------------------------------------\n");
	len += snprintf(buf + len, TSPDRV_BUF_SIZE - len, "-----------------------------------------------------------------------\n");

	for (i=0; i < TSPDRV_HISORY_MAX; i++) {

		if(tspdrv_history[i].used == 0)
			break;

		len += snprintf(buf + len, TSPDRV_BUF_SIZE - len,
			"tspdrv_idx=%d, write_count=%d(0x%x), # of zero=%d, # of len 3=%d\n", i,
				tspdrv_history[i].write_count, tspdrv_history[i].write_count, tspdrv_history[i].zero_idx, tspdrv_history[i].byte3_idx);

		len += snprintf(buf + len, TSPDRV_BUF_SIZE - len,
			"ts_enable=%ld.%09ld\nts_disable=%ld.%09ld\n",
				tspdrv_history[i].ts_enable.tv_sec, tspdrv_history[i].ts_enable.tv_nsec,
				tspdrv_history[i].ts_disable.tv_sec, tspdrv_history[i].ts_disable.tv_nsec);

		#ifdef CONFIG_TSPDRV_HISTORY_DETAIL
			for (j=0; j<tspdrv_history[i].write_count; j++) {
				len += snprintf(buf + len, TSPDRV_BUF_SIZE - len,
					"%02x ", tspdrv_history[i].write_value[j]);
			}
		#endif

		len += snprintf(buf + len, TSPDRV_BUF_SIZE - len,
			"\n\n");

	}
	len += snprintf(buf + len, TSPDRV_BUF_SIZE - len, "-----------------------------------------------------------------------\n");

	ret = simple_read_from_buffer(buffer, len, ppos, buf, TSPDRV_BUF_SIZE);
	kfree(buf);

	return ret;
}

static const struct file_operations tspdrv_debugfs_fops = {
	.owner = THIS_MODULE,
	.open =  tspdrv_debug_open,
	.read = tspdrv_debug_read,
};
#endif
/* Module info */
MODULE_AUTHOR("Immersion Corporation");
MODULE_DESCRIPTION("TouchSense Kernel Module");
MODULE_LICENSE("GPL v2");

static int __init tspdrv_init(void)
{
    int nRet, i;   /* initialized below */

    atomic_set(&g_nDebugLevel, DBL_ERROR);
#ifdef VIBE_RUNTIME_RECORD
    atomic_set(&g_bRuntimeRecord, 0);
    DbgOut((DBL_ERROR, "*** tspdrv: runtime recorder feature is ON for debugging which should be OFF in release version.\n"
                        "*** tspdrv: please turn off the feature by removing VIBE_RUNTIME_RECODE macro.\n"));
#endif
    DbgOut((DBL_INFO, "tspdrv: init_module.\n"));


#ifdef IMPLEMENT_AS_CHAR_DRIVER
    g_nMajor = register_chrdev(0, MODULE_NAME, &fops);
    if (g_nMajor < 0)
    {
        DbgOut((DBL_ERROR, "tspdrv: can't get major number.\n"));
        return g_nMajor;
    }
#else
    nRet = misc_register(&miscdev);
	if (nRet)
    {
        DbgOut((DBL_ERROR, "tspdrv: misc_register failed.\n"));
		return nRet;
	}
#endif

	nRet = platform_device_register(&platdev);
	if (nRet)
    {

        DbgOut((DBL_ERROR, "tspdrv: platform_device_register failed.\n"));
    }

	nRet = platform_driver_register(&platdrv);

	if (nRet)
    {
        DbgOut((DBL_ERROR, "tspdrv: platform_driver_register failed.\n"));
    }

    DbgRecorderInit(());
    ImmVibeSPI_ForceOut_Initialize();
    VibeOSKernelLinuxInitTimer();
    ResetOutputData();

    /* Get and concatenate device name and initialize data buffer */
    g_cchDeviceName = 0;
    for (i=0; i<NUM_ACTUATORS; i++)
    {
        char *szName = g_szDeviceName + g_cchDeviceName;
        ImmVibeSPI_Device_GetName(i, szName, VIBE_MAX_DEVICE_NAME_LENGTH);

        /* Append version information and get buffer length */
        strcat(szName, VERSION_STR);
        g_cchDeviceName += strlen(szName);

    }

#if defined (CONFIG_DEBUG_FS) && defined (CONFIG_TSPDRV_HISTORY)
		{
			tspdrv_debugfs_dir = kzalloc(sizeof(struct dentry), GFP_KERNEL);

			if (!tspdrv_debugfs_dir)
				return -ENOMEM;

			tspdrv_debugfs_dir =
				debugfs_create_dir("tspdrv_debug", NULL);
			if (tspdrv_debugfs_dir) {
				if (!debugfs_create_file("tspdrv_history", 0644,
					tspdrv_debugfs_dir,
					NULL, &tspdrv_debugfs_fops))
					pr_err("%s : debugfs_create_file, error\n", __func__);
			} else
				pr_err("%s : debugfs_create_dir, error\n", __func__);
		}
#endif

    return 0;
}

static void __exit tspdrv_exit(void)
{
    DbgOut((DBL_INFO, "tspdrv: cleanup_module.\n"));

    DbgRecorderTerminate(());

    VibeOSKernelLinuxTerminateTimer();
    ImmVibeSPI_ForceOut_Terminate();

	platform_driver_unregister(&platdrv);
	platform_device_unregister(&platdev);

#ifdef IMPLEMENT_AS_CHAR_DRIVER
    unregister_chrdev(g_nMajor, MODULE_NAME);
#else
    misc_deregister(&miscdev);
#endif
}

static int open(struct inode *inode, struct file *file)
{
    DbgOut((DBL_INFO, "tspdrv: open.\n"));

    if (!try_module_get(THIS_MODULE)) return -ENODEV;

    return 0;
}

static int release(struct inode *inode, struct file *file)
{
    DbgOut((DBL_INFO, "tspdrv: release.\n"));

    /*
    ** Reset force and stop timer when the driver is closed, to make sure
    ** no dangling semaphore remains in the system, especially when the
    ** driver is run outside of immvibed for testing purposes.
    */
    VibeOSKernelLinuxStopTimer();

    /*
    ** Clear the variable used to store the magic number to prevent
    ** unauthorized caller to write data. TouchSense service is the only
    ** valid caller.
    */
    file->private_data = (void*)NULL;

    module_put(THIS_MODULE);

    return 0;
}

static ssize_t read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    const size_t nBufSize = (g_cchDeviceName > (size_t)(*ppos)) ? min(count, g_cchDeviceName - (size_t)(*ppos)) : 0;

    /* End of buffer, exit */
    if (0 == nBufSize) return 0;

    if (0 != copy_to_user(buf, g_szDeviceName + (*ppos), nBufSize))
    {
        /* Failed to copy all the data, exit */
        DbgOut((DBL_ERROR, "tspdrv: copy_to_user failed.\n"));
        return 0;
    }

    /* Update file position and return copied buffer size */
    *ppos += nBufSize;
    return nBufSize;
}

static ssize_t write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    *ppos = 0;  /* file position not used, always set to 0 */

    /*
    ** Prevent unauthorized caller to write data.
    ** TouchSense service is the only valid caller.
    */
    if (file->private_data != (void*)TSPDRV_MAGIC_NUMBER)
    {
        DbgOut((DBL_ERROR, "tspdrv: unauthorized write.\n"));
        return 0;
    }

    /*
    ** Ignore packets that have size smaller than SPI_HEADER_SIZE or bigger than MAX_SPI_BUFFER_SIZE.
    ** Please note that the daemon may send an empty buffer (count == SPI_HEADER_SIZE)
    ** during quiet time between effects while playing a Timeline effect in order to maintain
    ** correct timing: if "count" is equal to SPI_HEADER_SIZE, the call to VibeOSKernelLinuxStartTimer()
    ** will just wait for the next timer tick.
    */
    if ((count < SPI_HEADER_SIZE) || (count > MAX_SPI_BUFFER_SIZE))
    {
        DbgOut((DBL_ERROR, "tspdrv: invalid buffer size.\n"));
        return 0;
    }

    /* Copy immediately the input buffer */
    if (0 != copy_from_user(g_cWriteBuffer, buf, count))
    {
        /* Failed to copy all the data, exit */
        DbgOut((DBL_ERROR, "tspdrv: copy_from_user failed.\n"));
        return 0;
    }

#ifdef CONFIG_TSPDRV_HISTORY
	if (count == 4) {
#ifdef CONFIG_TSPDRV_HISTORY_DETAIL
		tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].write_value[tspdrv_write_idx % TSPDRV_WRITE_MAX] = g_cWriteBuffer[3];
#endif
		if(g_cWriteBuffer[3] == 0)
			tspdrv_zero_idx++;
	} else if (count == 3) {
#ifdef CONFIG_TSPDRV_HISTORY_DETAIL
		tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].write_value[tspdrv_write_idx % TSPDRV_WRITE_MAX] = 0xff;
#endif
		tspdrv_byte3_idx++;
	}
	tspdrv_write_idx++;
#endif

    /* Extract force output samples and save them in an internal buffer */
    if (!SaveOutputData(g_cWriteBuffer, count))
    {
        DbgOut((DBL_ERROR, "tspdrv: SaveOutputData failed.\n"));
        return 0;
    }

    /* Start the timer after receiving new output force */
    g_bIsPlaying = true;

    VibeOSKernelLinuxStartTimer();

    return count;
}

#if HAVE_UNLOCKED_IOCTL
static long unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
#else
static int ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
#endif
{
    switch (cmd)
    {
        case TSPDRV_SET_MAGIC_NUMBER:
            file->private_data = (void*)TSPDRV_MAGIC_NUMBER;
            break;

        case TSPDRV_ENABLE_AMP:
		pr_info("tspdrv : ioctl - TSPDRV_ENABLE_AMP!\n");
		ImmVibeSPI_ForceOut_AmpEnable(arg);
#ifdef CONFIG_TSPDRV_HISTORY
				{
					struct timespec ts;

					getnstimeofday(&ts);
					memcpy(&tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].ts_enable, &ts, sizeof(struct timespec));
				}
#endif /* CONFIG_TSPDRV_HISTORY */

#ifdef VIBE_RUNTIME_RECORD
            if (atomic_read(&g_bRuntimeRecord)) {
                DbgRecord((arg,";------- TSPDRV_ENABLE_AMP ---------\n"));
            }
#else
            DbgRecorderReset((arg));
            DbgRecord((arg,";------- TSPDRV_ENABLE_AMP ---------\n"));
#endif
            break;

        case TSPDRV_DISABLE_AMP:
	   pr_info("tspdrv : ioctl - TSPDRV_DISABLE_AMP!\n");
            ImmVibeSPI_ForceOut_AmpDisable(arg);
#ifdef CONFIG_TSPDRV_HISTORY
				{
					struct timespec ts;

					getnstimeofday(&ts);
					memcpy(&tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].ts_disable, &ts, sizeof(struct timespec));
					tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].write_count = tspdrv_write_idx;
					tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].used = 1;
					tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].byte3_idx = tspdrv_byte3_idx;
					tspdrv_history[tspdrv_idx % TSPDRV_HISORY_MAX].zero_idx = tspdrv_zero_idx;
					tspdrv_idx ++;
					tspdrv_write_idx = 0;
					tspdrv_byte3_idx = 0;
					tspdrv_zero_idx = 0;
				}
#endif /* CONFIG_TSPDRV_HISTORY */

#ifdef VIBE_RUNTIME_RECORD
            if (atomic_read(&g_bRuntimeRecord)) {
                DbgRecord((arg,";------- TSPDRV_DISABLE_AMP ---------\n"));
            }
#endif
            break;

        case TSPDRV_GET_NUM_ACTUATORS:
            return NUM_ACTUATORS;

        case TSPDRV_SET_DBG_LEVEL:
            {
                long nDbgLevel;
                if (0 != copy_from_user((void *)&nDbgLevel, (const void __user *)arg, sizeof(long))) {
                    /* Error copying the data */
                    DbgOut((DBL_ERROR, "copy_from_user failed to copy debug level data.\n"));
                    return -1;
                }

                if (DBL_TEMP <= nDbgLevel &&  nDbgLevel <= DBL_OVERKILL) {
                    atomic_set(&g_nDebugLevel, nDbgLevel);
                } else {
                    DbgOut((DBL_ERROR, "Invalid debug level requested, ignored."));
                }

                break;
            }

        case TSPDRV_GET_DBG_LEVEL:
            return atomic_read(&g_nDebugLevel);

#ifdef VIBE_RUNTIME_RECORD
        case TSPDRV_SET_RUNTIME_RECORD_FLAG:
            {
                long nRecordFlag;
                if (0 != copy_from_user((void *)&nRecordFlag, (const void __user *)arg, sizeof(long))) {
                    /* Error copying the data */
                    DbgOut((DBL_ERROR, "copy_from_user failed to copy runtime record flag.\n"));
                    return -1;
                }

                atomic_set(&g_bRuntimeRecord, nRecordFlag);
                if (nRecordFlag) {
                    int i;
                    for (i=0; i<NUM_ACTUATORS; i++) {
                        DbgRecorderReset((i));
                    }
                }
                break;
            }
        case TSPDRV_GET_RUNTIME_RECORD_FLAG:
            return atomic_read(&g_bRuntimeRecord);
        case TSPDRV_SET_RUNTIME_RECORD_BUF_SIZE:
            {
                long nRecorderBufSize;
                if (0 != copy_from_user((void *)&nRecorderBufSize, (const void __user *)arg, sizeof(long))) {
                    /* Error copying the data */
                    DbgOut((DBL_ERROR, "copy_from_user failed to copy recorder buffer size.\n"));
                    return -1;
                }

                if (0 == DbgSetRecordBufferSize(nRecorderBufSize)) {
                    DbgOut((DBL_ERROR, "DbgSetRecordBufferSize failed.\n"));
                    return -1;
                }
                break;
            }
        case TSPDRV_GET_RUNTIME_RECORD_BUF_SIZE:
            return DbgGetRecordBufferSize();
#endif

        case TSPDRV_SET_DEVICE_PARAMETER:
            {
                device_parameter deviceParam;

                if (0 != copy_from_user((void *)&deviceParam, (const void __user *)arg, sizeof(deviceParam)))
                {
                    /* Error copying the data */
                    DbgOut((DBL_ERROR, "tspdrv: copy_from_user failed to copy kernel parameter data.\n"));
                    return -1;
                }

                switch (deviceParam.nDeviceParamID)
                {
                    case VIBE_KP_CFG_UPDATE_RATE_MS:
                        /* Update the timer period */
                        g_nTimerPeriodMs = deviceParam.nDeviceParamValue;



#ifdef CONFIG_HIGH_RES_TIMERS
                        /* For devices using high resolution timer we need to update the ktime period value */
                        g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);
#endif
                        break;

                    case VIBE_KP_CFG_FREQUENCY_PARAM1:
                    case VIBE_KP_CFG_FREQUENCY_PARAM2:
                    case VIBE_KP_CFG_FREQUENCY_PARAM3:
                    case VIBE_KP_CFG_FREQUENCY_PARAM4:
                    case VIBE_KP_CFG_FREQUENCY_PARAM5:
                    case VIBE_KP_CFG_FREQUENCY_PARAM6:
                        if (0 > ImmVibeSPI_ForceOut_SetFrequency(deviceParam.nDeviceIndex, deviceParam.nDeviceParamID, deviceParam.nDeviceParamValue))
                        {
                            DbgOut((DBL_ERROR, "tspdrv: cannot set device frequency parameter.\n"));
                            return -1;
                        }
                        break;
                }
            }
        }
    return 0;
}

static int suspend(struct platform_device *pdev, pm_message_t state)
{
    if (g_bIsPlaying)
    {
        DbgOut((DBL_INFO, "tspdrv: can't suspend, still playing effects.\n"));
        return -EBUSY;
    }
    else
    {
        DbgOut((DBL_INFO, "tspdrv: suspend.\n"));
        return 0;
    }
}

static int resume(struct platform_device *pdev)
{
    DbgOut((DBL_ERROR, "tspdrv: resume.\n"));

	return 0;   /* can resume */
}

static void platform_release(struct device *dev)
{
    DbgOut((DBL_ERROR, "tspdrv: platform_release.\n"));
}

module_init(tspdrv_init);
module_exit(tspdrv_exit);
#endif
