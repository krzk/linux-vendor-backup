/*
 * This code copied from linux kernel 3.11+
 * commit cb65537ee1134d3cc55c1fa83952bc8eb1212833
 */


#include "wait.h"


#if LINUX_VERSION_CODE < 199424

#include <linux/wait.h>
#include <linux/module.h>


#define WAIT_ATOMIC_T_BIT_NR -1
#define __WAIT_ATOMIC_T_KEY_INITIALIZER(p)				\
	{ .flags = p, .bit_nr = WAIT_ATOMIC_T_BIT_NR, }


/*
 * Manipulate the atomic_t address to produce a better bit waitqueue table hash
 * index (we're keying off bit -1, but that would produce a horrible hash
 * value).
 */
static inline wait_queue_head_t *atomic_t_waitqueue(atomic_t *p)
{
	if (BITS_PER_LONG == 64) {
		unsigned long q = (unsigned long)p;
		return bit_waitqueue((void *)(q & ~1), q & 1);
	}
	return bit_waitqueue(p, 0);
}

static int wake_atomic_t_function(wait_queue_t *wait, unsigned mode, int sync,
				  void *arg)
{
	struct wait_bit_key *key = arg;
	struct wait_bit_queue *wait_bit
		= container_of(wait, struct wait_bit_queue, wait);
	atomic_t *val = key->flags;

	if (wait_bit->key.flags != key->flags ||
	    wait_bit->key.bit_nr != key->bit_nr ||
	    atomic_read(val) != 0)
		return 0;
	return autoremove_wake_function(wait, mode, sync, key);
}

/*
 * To allow interruptible waiting and asynchronous (i.e. nonblocking) waiting,
 * the actions of __wait_on_atomic_t() are permitted return codes.  Nonzero
 * return codes halt waiting and return.
 */
static int __wait_on_atomic_t(wait_queue_head_t *wq, struct wait_bit_queue *q,
			      int (*action)(atomic_t *), unsigned mode)
{
	atomic_t *val;
	int ret = 0;

	do {
		prepare_to_wait(wq, &q->wait, mode);
		val = q->key.flags;
		if (atomic_read(val) == 0)
			ret = (*action)(val);
	} while (!ret && atomic_read(val) != 0);
	finish_wait(wq, &q->wait);
	return ret;
}

#define DEFINE_WAIT_ATOMIC_T(name, p)					\
	struct wait_bit_queue name = {					\
		.key = __WAIT_ATOMIC_T_KEY_INITIALIZER(p),		\
		.wait = {						\
			.private	= current,			\
			.func		= wake_atomic_t_function,	\
			.task_list	=				\
				LIST_HEAD_INIT((name).wait.task_list),	\
		},							\
	}

int out_of_line_wait_on_atomic_t(atomic_t *p, int (*action)(atomic_t *),
                                        unsigned mode)
{
	wait_queue_head_t *wq = atomic_t_waitqueue(p);
	DEFINE_WAIT_ATOMIC_T(wait, p);

	return __wait_on_atomic_t(wq, &wait, action, mode);
}
EXPORT_SYMBOL(out_of_line_wait_on_atomic_t);

/**
 * wake_up_atomic_t - Wake up a waiter on a atomic_t
 * @word: The word being waited on, a kernel virtual address
 * @bit: The bit of the word being waited on
 *
 * Wake up anyone waiting for the atomic_t to go to zero.
 *
 * Abuse the bit-waker function and its waitqueue hash table set (the atomic_t
 * check is done by the waiter's wake function, not the by the waker itself).
 */
void wake_up_atomic_t(atomic_t *p)
{
	__wake_up_bit(atomic_t_waitqueue(p), p, WAIT_ATOMIC_T_BIT_NR);
}
EXPORT_SYMBOL(wake_up_atomic_t);

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0) */
