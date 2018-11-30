/*
 * This code copied from linux kernel 3.11+
 * commit cb65537ee1134d3cc55c1fa83952bc8eb1212833
 */


#ifndef _SWAP_WAIT_H
#define _SWAP_WAIT_H


#include <linux/version.h>


#if LINUX_VERSION_CODE < 199424


#include <linux/atomic.h>


void wake_up_atomic_t(atomic_t *);
int out_of_line_wait_on_atomic_t(atomic_t *, int (*)(atomic_t *), unsigned);

static inline
int wait_on_atomic_t(atomic_t *val, int (*action)(atomic_t *), unsigned mode)
{
	if (atomic_read(val) == 0)
		return 0;
	return out_of_line_wait_on_atomic_t(val, action, mode);
}

#else /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0) */

#include <linux/wait.h>

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0) */

#endif /* _SWAP_WAIT_H */
