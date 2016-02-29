/*
 * Fence mechanism for dma-buf and to allow for asynchronous dma access
 *
 * Copyright (C) 2012 Canonical Ltd
 * Copyright (C) 2012 Texas Instruments
 *
 * Authors:
 * Rob Clark <rob.clark@linaro.org>
 * Maarten Lankhorst <maarten.lankhorst@canonical.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/slab.h>
#include <linux/export.h>
#include <linux/fence.h>
#include <linux/seqno-fence.h>

atomic_t fence_context_counter = ATOMIC_INIT(0);
EXPORT_SYMBOL(fence_context_counter);

int __fence_signal(struct fence *fence)
{
	struct fence_cb *cur, *tmp;
	int ret = 0;

	if (WARN_ON(!fence))
		return -EINVAL;

	if (test_and_set_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		ret = -EINVAL;

		/*
		 * we might have raced with the unlocked fence_signal,
		 * still run through all callbacks
		 */
	}

	list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
		list_del_init(&cur->node);
		cur->func(fence, cur, cur->priv);
	}
	return ret;
}
EXPORT_SYMBOL(__fence_signal);

/**
 * fence_signal - signal completion of a fence
 * @fence: the fence to signal
 *
 * Signal completion for software callbacks on a fence, this will unblock
 * fence_wait() calls and run all the callbacks added with
 * fence_add_callback(). Can be called multiple times, but since a fence
 * can only go from unsignaled to signaled state, it will only be effective
 * the first time.
 */
int fence_signal(struct fence *fence)
{
	unsigned long flags;

	if (!fence || test_and_set_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return -EINVAL;

	if (test_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags)) {
		struct fence_cb *cur, *tmp;

		spin_lock_irqsave(fence->lock, flags);
		list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
			list_del_init(&cur->node);
			cur->func(fence, cur, cur->priv);
		}
		spin_unlock_irqrestore(fence->lock, flags);
	}
	return 0;
}
EXPORT_SYMBOL(fence_signal);

void release_fence(struct kref *kref)
{
	struct fence *fence =
			container_of(kref, struct fence, refcount);

	BUG_ON(!list_empty(&fence->cb_list));

	if (fence->ops->release)
		fence->ops->release(fence);
	else
		kfree(fence);
}
EXPORT_SYMBOL(release_fence);

/**
 * fence_enable_sw_signaling - enable signaling on fence
 * @fence:	[in]	the fence to enable
 *
 * this will request for sw signaling to be enabled, to make the fence
 * complete as soon as possible
 */
void fence_enable_sw_signaling(struct fence *fence)
{
	unsigned long flags;

	if (!test_and_set_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags) &&
	    !test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		spin_lock_irqsave(fence->lock, flags);

		if (!fence->ops->enable_signaling(fence))
			__fence_signal(fence);

		spin_unlock_irqrestore(fence->lock, flags);
	}
}
EXPORT_SYMBOL(fence_enable_sw_signaling);

/**
 * fence_add_callback - add a callback to be called when the fence
 * is signaled
 * @fence:	[in]	the fence to wait on
 * @cb:		[in]	the callback to register
 * @func:	[in]	the function to call
 * @priv:	[in]	the argument to pass to function
 *
 * cb will be initialized by fence_add_callback, no initialization
 * by the caller is required. Any number of callbacks can be registered
 * to a fence, but a callback can only be registered to one fence at a time.
 *
 * Note that the callback can be called from an atomic context.  If
 * fence is already signaled, this function will return -ENOENT (and
 * *not* call the callback)
 *
 * Add a software callback to the fence. Same restrictions apply to
 * refcount as it does to fence_wait, however the caller doesn't need to
 * keep a refcount to fence afterwards: when software access is enabled,
 * the creator of the fence is required to keep the fence alive until
 * after it signals with fence_signal. The callback itself can be called
 * from irq context.
 *
 */
int fence_add_callback(struct fence *fence, struct fence_cb *cb,
		       fence_func_t func, void *priv)
{
	unsigned long flags;
	int ret = 0;
	bool was_set;

	if (WARN_ON(!fence || !func))
		return -EINVAL;

	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return -ENOENT;

	spin_lock_irqsave(fence->lock, flags);

	was_set = test_and_set_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags);

	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		ret = -ENOENT;
	else if (!was_set && !fence->ops->enable_signaling(fence)) {
		__fence_signal(fence);
		ret = -ENOENT;
	}

	if (!ret) {
		cb->func = func;
		cb->priv = priv;
		list_add_tail(&cb->node, &fence->cb_list);
	}
	spin_unlock_irqrestore(fence->lock, flags);

	return ret;
}
EXPORT_SYMBOL(fence_add_callback);

/**
 * fence_remove_callback - remove a callback from the signaling list
 * @fence:	[in]	the fence to wait on
 * @cb:		[in]	the callback to remove
 *
 * Remove a previously queued callback from the fence. This function returns
 * true is the callback is succesfully removed, or false if the fence has
 * already been signaled.
 *
 * *WARNING*:
 * Cancelling a callback should only be done if you really know what you're
 * doing, since deadlocks and race conditions could occur all too easily. For
 * this reason, it should only ever be done on hardware lockup recovery,
 * with a reference held to the fence.
 */
bool
fence_remove_callback(struct fence *fence, struct fence_cb *cb)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(fence->lock, flags);

	ret = !list_empty(&cb->node);
	if (ret)
		list_del_init(&cb->node);

	spin_unlock_irqrestore(fence->lock, flags);

	return ret;
}
EXPORT_SYMBOL(fence_remove_callback);

static void
fence_default_wait_cb(struct fence *fence, struct fence_cb *cb, void *priv)
{
	try_to_wake_up(priv, TASK_NORMAL, 0);
}

/**
 * fence_default_wait - default sleep until the fence gets signaled
 * or until timeout elapses
 * @fence:	[in]	the fence to wait on
 * @intr:	[in]	if true, do an interruptible wait
 * @timeout:	[in]	timeout value in jiffies, or MAX_SCHEDULE_TIMEOUT
 *
 * Returns -ERESTARTSYS if interrupted, 0 if the wait timed out, or the
 * remaining timeout in jiffies on success.
 */
long
fence_default_wait(struct fence *fence, bool intr, signed long timeout)
{
	struct fence_cb cb;
	unsigned long flags;
	long ret = timeout;
	bool was_set;

	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return timeout;

	spin_lock_irqsave(fence->lock, flags);

	if (intr && signal_pending(current)) {
		ret = -ERESTARTSYS;
		goto out;
	}

	was_set = test_and_set_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags);

	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		goto out;

	if (!was_set && !fence->ops->enable_signaling(fence)) {
		__fence_signal(fence);
		goto out;
	}

	cb.func = fence_default_wait_cb;
	cb.priv = current;
	list_add(&cb.node, &fence->cb_list);

	while (!test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags) && ret > 0) {
		if (intr)
			__set_current_state(TASK_INTERRUPTIBLE);
		else
			__set_current_state(TASK_UNINTERRUPTIBLE);
		spin_unlock_irqrestore(fence->lock, flags);

		ret = schedule_timeout(ret);

		spin_lock_irqsave(fence->lock, flags);
		if (ret > 0 && intr && signal_pending(current))
			ret = -ERESTARTSYS;
	}

	if (!list_empty(&cb.node))
		list_del(&cb.node);
	__set_current_state(TASK_RUNNING);

out:
	spin_unlock_irqrestore(fence->lock, flags);
	return ret;
}
EXPORT_SYMBOL(fence_default_wait);

static bool seqno_enable_signaling(struct fence *fence)
{
	struct seqno_fence *seqno_fence = to_seqno_fence(fence);
	return seqno_fence->ops->enable_signaling(fence);
}

static bool seqno_signaled(struct fence *fence)
{
	struct seqno_fence *seqno_fence = to_seqno_fence(fence);
	return seqno_fence->ops->signaled && seqno_fence->ops->signaled(fence);
}

static void seqno_release(struct fence *fence)
{
	struct seqno_fence *f = to_seqno_fence(fence);

	dma_buf_put(f->sync_buf);
	if (f->ops->release)
		f->ops->release(fence);
	else
		kfree(f);
}

static long seqno_wait(struct fence *fence, bool intr, signed long timeout)
{
	struct seqno_fence *f = to_seqno_fence(fence);
	return f->ops->wait(fence, intr, timeout);
}

const struct fence_ops seqno_fence_ops = {
	.enable_signaling = seqno_enable_signaling,
	.signaled = seqno_signaled,
	.wait = seqno_wait,
	.release = seqno_release
};
EXPORT_SYMBOL(seqno_fence_ops);
