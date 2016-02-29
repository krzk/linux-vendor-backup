/*
** =========================================================================
** File:
**     VibeOSKernelLinuxHRTime.c
**
** Description: 
**     High Resolution Time helper functions for Linux.
**
** Portions Copyright (c) 2010-2012 Immersion Corporation. All Rights Reserved. 
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

/* 
** Kernel high-resolution software timer is used as an example but another type 
** of timer (such as HW timer or standard software timer) might be used to achieve 
** the 5ms required rate.
*/
#ifdef CONFIG_MACH_WC1
#ifndef CONFIG_HIGH_RES_TIMERS
#warning "The Kernel does not have high resolution timers enabled. Either provide a non hr-timer implementation of VibeOSKernelLinuxTime.c or re-compile your kernel with CONFIG_HIGH_RES_TIMERS=y"
#endif

#include <linux/hrtimer.h>
#include <linux/mutex.h>

#define WATCHDOG_TIMEOUT    10  /* 10 timer cycles = 50ms */

/* Global variables */
static bool g_bTimerStarted;
static struct hrtimer g_tspTimer;
static ktime_t g_ktFiveMs;
static int g_nWatchdogCounter;

DEFINE_SEMAPHORE(g_hMutex);

/* Forward declarations */
static void VibeOSKernelLinuxStartTimer(void);
static void VibeOSKernelLinuxStopTimer(void);
static int VibeOSKernelProcessData(void *data);
#define VIBEOSKERNELPROCESSDATA

static inline int VibeSemIsLocked(struct semaphore *lock)
{
#if ((LINUX_VERSION_CODE & 0xFFFFFF) < KERNEL_VERSION(2, 6, 27))
	return atomic_read(&lock->count) != 1;
#else
	return (lock->count) != 1;
#endif
}

static enum hrtimer_restart tsp_timer_interrupt(struct hrtimer *timer)
{
	/* Scheduling next timeout value right away */
	hrtimer_forward_now(timer, g_ktFiveMs);

	if (g_bTimerStarted) {
		if (VibeSemIsLocked(&g_hMutex))
			up(&g_hMutex);
	}

	return HRTIMER_RESTART;
}

static int VibeOSKernelProcessData(void *data)
{
	int i;
	int nActuatorNotPlaying = 0;

	for (i = 0; i < NUM_ACTUATORS; i++) {
		actuator_samples_buffer *pCurrentActuatorSample = &(g_SamplesBuffer[i]);

		if (-1 == pCurrentActuatorSample->nIndexPlayingBuffer) {
			nActuatorNotPlaying++;
			if ((NUM_ACTUATORS == nActuatorNotPlaying) && ((++g_nWatchdogCounter) > WATCHDOG_TIMEOUT)) {
				VibeInt8 cZero[1] = {0};

				/* Nothing to play for all actuators, turn off the timer when we reach the watchdog tick count limit */
				ImmVibeSPI_ForceOut_SetSamples(i, 8, 1, cZero);
				ImmVibeSPI_ForceOut_AmpDisable(i);
				VibeOSKernelLinuxStopTimer();

				/* Reset watchdog counter */
				g_nWatchdogCounter = 0;
			}
		} else {
			/* Play the current buffer */
			if (VIBE_E_FAIL == ImmVibeSPI_ForceOut_SetSamples(
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nActuatorIndex,
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBitDepth,
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize,
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].dataBuffer)) {
				/* VIBE_E_FAIL means NAK has been handled. Schedule timer to restart 5 ms from now */
				hrtimer_forward_now(&g_tspTimer, g_ktFiveMs);
			}

			pCurrentActuatorSample->nIndexOutputValue += pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize;

			if (pCurrentActuatorSample->nIndexOutputValue >= pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize) {
				/* Reach the end of the current buffer */
				pCurrentActuatorSample->actuatorSamples[(int)pCurrentActuatorSample->nIndexPlayingBuffer].nBufferSize = 0;

				/* Switch buffer */
				(pCurrentActuatorSample->nIndexPlayingBuffer) ^= 1;
				pCurrentActuatorSample->nIndexOutputValue = 0;

				/* Finished playing, disable amp for actuator (i) */
				if (g_bStopRequested) {
					pCurrentActuatorSample->nIndexPlayingBuffer = -1;

					ImmVibeSPI_ForceOut_AmpDisable(i);
				}
			}
		}
	}

    /* If finished playing, stop timer */
		if (g_bStopRequested) {
			VibeOSKernelLinuxStopTimer();

			/* stop all actuator */
			for (i = 0; i < NUM_ACTUATORS; i++) {
				ImmVibeSPI_ForceOut_AmpDisable(i);
			}

		/* Reset watchdog counter */
		g_nWatchdogCounter = 0;

		if (VibeSemIsLocked(&g_hMutex))
			up(&g_hMutex);
		return 1;   /* tell the caller this is the last iteration */
	}

	return 0;
}

static void VibeOSKernelLinuxInitTimer(void)
{
	/* Get a 5,000,000ns = 5ms time value */
	g_ktFiveMs = ktime_set(0, 5000000);

	hrtimer_init(&g_tspTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	/* Initialize a 5ms-timer with tsp_timer_interrupt as timer callback (interrupt driven)*/
	g_tspTimer.function = tsp_timer_interrupt;
}

static void VibeOSKernelLinuxStartTimer(void)
{
	int i;
	int res;

	/* Reset watchdog counter */
	g_nWatchdogCounter = 0;

	if (!g_bTimerStarted) {
		if (!VibeSemIsLocked(&g_hMutex))
			res = down_interruptible(&g_hMutex); /* start locked */

		g_bTimerStarted = true;

		/* Start the timer */
		hrtimer_start(&g_tspTimer, g_ktFiveMs, HRTIMER_MODE_REL);

		/* Don't block the write() function after the first sample to allow the host sending the next samples with no delay */
		for (i = 0; i < NUM_ACTUATORS; i++) {
			if ((g_SamplesBuffer[i].actuatorSamples[0].nBufferSize)
				|| (g_SamplesBuffer[i].actuatorSamples[1].nBufferSize)) {
				g_SamplesBuffer[i].nIndexOutputValue = 0;
				return;
			}
		}
	}

	if (0 != VibeOSKernelProcessData(NULL))
		return;

	/*
	** Use interruptible version of down to be safe
	** (try to not being stuck here if the mutex is not freed for any reason)
	*/
	res = down_interruptible(&g_hMutex);  /* wait for the mutex to be freed by the timer */
	if (res != 0)
		DbgOut((KERN_INFO "VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
}

static void VibeOSKernelLinuxStopTimer(void)
{
	int i;

	if (g_bTimerStarted) {
		g_bTimerStarted = false;
		hrtimer_cancel(&g_tspTimer);
	}

	/* Reset samples buffers */
	for (i = 0; i < NUM_ACTUATORS; i++) {
		g_SamplesBuffer[i].nIndexPlayingBuffer = -1;
		g_SamplesBuffer[i].actuatorSamples[0].nBufferSize = 0;
		g_SamplesBuffer[i].actuatorSamples[1].nBufferSize = 0;
	}
	g_bStopRequested = false;
	g_bIsPlaying = false;
}

static void VibeOSKernelLinuxTerminateTimer(void)
{
	VibeOSKernelLinuxStopTimer();
	if (VibeSemIsLocked(&g_hMutex))
		up(&g_hMutex);
}
#else
#ifndef CONFIG_HIGH_RES_TIMERS
#warning "The Kernel does not have high resolution timers enabled. Either provide a non hr-timer implementation of VibeOSKernelLinuxTime.c or re-compile your kernel with CONFIG_HIGH_RES_TIMERS=y"
#endif

#include <linux/hrtimer.h>
#include <linux/semaphore.h>

#define WATCHDOG_TIMEOUT    10  /* 10 timer cycles */

/* Global variables */
static bool g_bTimerStarted = false;
static struct hrtimer g_tspTimer;
static ktime_t g_ktTimerPeriod; /* ktime_t equivalent of g_nTimerPeriodMs */
static int g_nWatchdogCounter = 0;

#ifndef NUM_EXTRA_BUFFERS
#define NUM_EXTRA_BUFFERS 0
#endif
struct semaphore g_hSemaphore;

/* Forward declarations */
static void VibeOSKernelLinuxStartTimer(void);
static void VibeOSKernelLinuxStopTimer(void);

static inline int VibeSemIsLocked(struct semaphore *lock)
{
#if ((LINUX_VERSION_CODE & 0xFFFFFF) < KERNEL_VERSION(2,6,27))
    return atomic_read(&lock->count) < 1;
#else
    return (lock->count) < 1;
#endif
}

static enum hrtimer_restart VibeOSKernelTimerProc(struct hrtimer *timer)
{
    /* Return right away if timer is not supposed to run */
    if (!g_bTimerStarted) return  HRTIMER_NORESTART;

    /* Scheduling next timeout value right away */
    if (++g_nWatchdogCounter < WATCHDOG_TIMEOUT)
        hrtimer_forward_now(timer, g_ktTimerPeriod);

    if (VibeSemIsLocked(&g_hSemaphore))
    {
        up(&g_hSemaphore);
    }

   if (g_nWatchdogCounter < WATCHDOG_TIMEOUT)
   {
       return HRTIMER_RESTART;
   }
   else
   {
       /* Do not call SPI functions in this function as their implementation coud use interrupt */
       VibeOSKernelLinuxStopTimer();
       return HRTIMER_NORESTART;
   }
}

static int VibeOSKernelProcessData(void* data)
{
    SendOutputData();

    /* Reset watchdog counter */
    g_nWatchdogCounter = 0;

    return 0;
}

static void VibeOSKernelLinuxInitTimer(void)
{
    /* Convert the initial timer period in ktime_t value */
    g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);

    hrtimer_init(&g_tspTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

    /* Initialize a 5ms-timer with VibeOSKernelTimerProc as timer callback (interrupt driven)*/
    g_tspTimer.function = VibeOSKernelTimerProc;
}

static void VibeOSKernelLinuxStartTimer(void)
{
    /* Reset watchdog counter */
    g_nWatchdogCounter = 0;

    if (!g_bTimerStarted)
    {
        /* (Re-)Initialize the semaphore used with the timer */
        sema_init(&g_hSemaphore, NUM_EXTRA_BUFFERS);

        g_bTimerStarted = true;

        /* Start the timer */
        g_ktTimerPeriod = ktime_set(0, g_nTimerPeriodMs * 1000000);
        hrtimer_start(&g_tspTimer, g_ktTimerPeriod, HRTIMER_MODE_REL);
    }
    else
    {
        int res;  
        /* 
        ** Use interruptible version of down to be safe 
        ** (try to not being stuck here if the semaphore is not freed for any reason)
        */
        res = down_interruptible(&g_hSemaphore);  /* wait for the semaphore to be freed by the timer */
        if (res != 0)
        {
            DbgOut((DBL_INFO, "VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
        }
    }
    VibeOSKernelProcessData(NULL);
    /* 
    ** Because of possible NACK handling, the  VibeOSKernelProcessData() call above could take more than
    ** 5 ms on some piezo devices that are buffering output samples; when this happens, the timer
    ** interrupt will release the g_hSemaphore while VibeOSKernelProcessData is executing and the player
    ** will immediately send the new packet to the SPI layer when VibeOSKernelProcessData exits, which
    ** could cause another NACK right away. To avoid that, we'll create a small delay if the semaphore
    ** was released when VibeOSKernelProcessData exits, by acquiring the mutex again and waiting for
    ** the timer to release it.
    */
#if defined(NUM_EXTRA_BUFFERS) && (NUM_EXTRA_BUFFERS)
    if (g_bTimerStarted && !VibeSemIsLocked(&g_hSemaphore))
    {
        int res;

        res = down_interruptible(&g_hSemaphore);

        if (res != 0)
        {
            DbgOut((DBL_INFO, "VibeOSKernelLinuxStartTimer: down_interruptible interrupted by a signal.\n"));
        }
    }
#endif
}

static void VibeOSKernelLinuxStopTimer(void)
{
    if (g_bTimerStarted)
    {
        g_bTimerStarted = false;
    }

    /* Reset samples buffers */
    ResetOutputData();

    g_bIsPlaying = false;
} 

static void VibeOSKernelLinuxTerminateTimer(void)
{
    VibeOSKernelLinuxStopTimer();
    hrtimer_cancel(&g_tspTimer);

    if (VibeSemIsLocked(&g_hSemaphore)) up(&g_hSemaphore);
}
#endif
