/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include <linux/dmabuf-sync.h>

#define MAX_SYNC_TIMEOUT	5	/* Second. */
#define MAX_WAIT_TIMEOUT	2000	/* Millisecond. */

#define NEED_BEGIN_CPU_ACCESS(old, new_type)	\
			((old->accessed_type & DMA_BUF_ACCESS_DMA_W) == \
			  DMA_BUF_ACCESS_DMA_W && new_type == DMA_BUF_ACCESS_R)

#define NEED_END_CPU_ACCESS(old, new_type)	\
			(((old->accessed_type == DMA_BUF_ACCESS_W) || \
			 (old->accessed_type == DMA_BUF_ACCESS_RW)) && \
			 new_type & DMA_BUF_ACCESS_DMA)

#define WAKE_UP_SYNC_OBJ(obj) {						\
		if (obj->waiting) {					\
			obj->waiting = false;				\
			wake_up(&obj->wq);				\
		}							\
	}

#define DEL_OBJ_FROM_RSV(obj, rsv) {					\
		struct dmabuf_sync_object *e, *n;			\
									\
		list_for_each_entry_safe(e, n, &rsv->syncs, r_head) {	\
			if (e == obj && !e->task) {			\
				list_del_init(&e->r_head);		\
				break;					\
			}						\
		}							\
	}

int dmabuf_sync_enabled = 1;

MODULE_PARM_DESC(enabled, "Check if dmabuf sync is supported or not");
module_param_named(enabled, dmabuf_sync_enabled, int, 0444);

DEFINE_WW_CLASS(dmabuf_sync_ww_class);
EXPORT_SYMBOL_GPL(dmabuf_sync_ww_class);

static void dmabuf_sync_timeout_worker(struct work_struct *work)
{
	struct dmabuf_sync *sync = container_of(work, struct dmabuf_sync, work);
	struct dmabuf_sync_object *sobj;

	mutex_lock(&sync->lock);

	list_for_each_entry(sobj, &sync->syncs, head) {
		struct dmabuf_sync_reservation *rsvp = sobj->robj;

		mutex_lock(&rsvp->lock);

		pr_warn("%s: timeout = 0x%p [type = %d:%d, "
					"refcnt = %d, locked = %d]\n",
					sync->name, sobj->dmabuf,
					rsvp->accessed_type,
					sobj->access_type,
					atomic_read(&rsvp->shared_cnt),
					rsvp->locked);

		if (rsvp->polled) {
			rsvp->poll_event = true;
			rsvp->polled = false;
			wake_up_interruptible(&rsvp->poll_wait);
		}

		/*
		 * Wake up a task blocked by dmabuf_sync_wait_prev_objs().
		 *
		 * If sobj->waiting is true, the task is waiting for the wake
		 * up event so wake up the task if a given time period is
		 * elapsed and current task is timed out.
		 */
		WAKE_UP_SYNC_OBJ(sobj);

		/* Delete a sync object from reservation object of dmabuf. */
		DEL_OBJ_FROM_RSV(sobj, rsvp);

		if (atomic_add_unless(&rsvp->shared_cnt, -1, 1)) {
			mutex_unlock(&rsvp->lock);
			continue;
		}

		/* unlock only valid sync object. */
		if (!rsvp->locked) {
			mutex_unlock(&rsvp->lock);
			continue;
		}

		mutex_unlock(&rsvp->lock);
		ww_mutex_unlock(&rsvp->sync_lock);

		mutex_lock(&rsvp->lock);
		rsvp->locked = false;

		if (sobj->access_type & DMA_BUF_ACCESS_R)
			pr_warn("%s: r-unlocked = 0x%p\n",
					sync->name, sobj->dmabuf);
		else
			pr_warn("%s: w-unlocked = 0x%p\n",
					sync->name, sobj->dmabuf);

		mutex_unlock(&rsvp->lock);
	}

	sync->status = 0;
	mutex_unlock(&sync->lock);

	dmabuf_sync_put_all(sync);
	dmabuf_sync_fini(sync);
}

static void dmabuf_sync_cache_ops(struct dmabuf_sync *sync)
{
	struct dmabuf_sync_object *sobj;

	mutex_lock(&sync->lock);

	list_for_each_entry(sobj, &sync->syncs, head) {
		struct dma_buf *dmabuf;

		dmabuf = sobj->dmabuf;
		if (WARN_ON(!dmabuf || !sobj->robj))
			continue;

		mutex_lock(&sobj->robj->lock);

		/* first time access. */
		if (!sobj->robj->accessed_type)
			goto out;

		if (NEED_END_CPU_ACCESS(sobj->robj, sobj->access_type))
			/* cache clean */
			dma_buf_end_cpu_access(dmabuf, 0, dmabuf->size,
							DMA_TO_DEVICE);
		else if (NEED_BEGIN_CPU_ACCESS(sobj->robj, sobj->access_type))
			/* cache invalidate */
			dma_buf_begin_cpu_access(dmabuf, 0, dmabuf->size,
							DMA_FROM_DEVICE);

out:
		/* Update access type to new one. */
		sobj->robj->accessed_type = sobj->access_type;
		mutex_unlock(&sobj->robj->lock);
	}

	mutex_unlock(&sync->lock);
}

static void dmabuf_sync_single_cache_ops(struct dma_buf *dmabuf,
						unsigned int access_type)
{
	struct dmabuf_sync_reservation *robj;

	robj = dmabuf->sync;

	/* first time access. */
	if (!robj->accessed_type)
		goto out;

	if (NEED_END_CPU_ACCESS(robj, access_type))
		/* cache clean */
		dma_buf_end_cpu_access(dmabuf, 0, dmabuf->size,
						DMA_TO_DEVICE);
	else if (NEED_BEGIN_CPU_ACCESS(robj, access_type))
		/* cache invalidate */
		dma_buf_begin_cpu_access(dmabuf, 0, dmabuf->size,
						DMA_FROM_DEVICE);

out:
		/* Update access type to new one. */
		robj->accessed_type = access_type;
}

static void dmabuf_sync_lock_timeout(unsigned long arg)
{
	struct dmabuf_sync *sync = (struct dmabuf_sync *)arg;

	schedule_work(&sync->work);
}

static void dmabuf_sync_wait_prev_objs(struct dmabuf_sync_object *sobj,
					struct dmabuf_sync_reservation *rsvp,
					struct ww_acquire_ctx *ctx)
{
	mutex_lock(&rsvp->lock);

	/*
	 * This function handles the write-and-then-read ordering issue.
	 *
	 * The ordering issue:
	 * There is a case that a task don't take a lock to a dmabuf so
	 * this task would be stalled even though this task requested a lock
	 * to the dmabuf between other task unlocked and tries to lock
	 * the dmabuf again.
	 *
	 * How to handle the ordering issue:
	 * 1. Check if there is a sync object added prior to current task's one.
	 * 2. If exists, it unlocks the dmabuf so that other task can take
	 *	a lock to the dmabuf first.
	 * 3. Wait for the wake up event from other task: current task will be
	 *	waked up when other task unlocks the dmabuf.
	 * 4. Take a lock to the dmabuf again.
	 */
	if (!list_empty(&rsvp->syncs)) {
		struct dmabuf_sync_object *r_sobj, *next;

		list_for_each_entry_safe(r_sobj, next, &rsvp->syncs,
					r_head) {
			long timeout;

			/*
			 * Find a sync object added to rsvp->syncs by other task
			 * before current task tries to lock the dmabuf again.
			 * If sobj == r_sobj, it means that there is no any task
			 * that added its own sync object to rsvp->syncs so out
			 * of this loop.
			 */
			if (sobj == r_sobj)
				break;

			/*
			 * Unlock the dmabuf if there is a sync object added
			 * to rsvp->syncs so that other task can take a lock
			 * first.
			 */
			if (rsvp->locked) {
				ww_mutex_unlock(&rsvp->sync_lock);
				rsvp->locked = false;
			}

			r_sobj->waiting = true;

			atomic_inc(&r_sobj->refcnt);
			mutex_unlock(&rsvp->lock);

			/* Wait for the wake up event from other task. */
			timeout = wait_event_timeout(r_sobj->wq,
					!r_sobj->waiting,
					msecs_to_jiffies(MAX_WAIT_TIMEOUT));
			if (!timeout) {
				r_sobj->waiting = false;
				pr_warn("wait event timeout: sobj = 0x%p\n",
						r_sobj);

				/*
				 * A sync object from fcntl system call has no
				 * timeout handler so delete ane free r_sobj
				 * once timeout here without checking refcnt.
				 */
				if (r_sobj->task) {
					pr_warn("delete: user sobj = 0x%p\n",
							r_sobj);
					list_del_init(&r_sobj->r_head);
					kfree(r_sobj);
				}
			}

			if (!atomic_add_unless(&r_sobj->refcnt, -1, 1))
				kfree(r_sobj);

			/*
			 * Other task unlocked the dmabuf so take a lock again.
			 */
			ww_mutex_lock(&rsvp->sync_lock, ctx);

			mutex_lock(&rsvp->lock);
			rsvp->locked = true;
		}
	}

	mutex_unlock(&rsvp->lock);
}

static int dmabuf_sync_lock_objs(struct dmabuf_sync *sync,
					struct ww_acquire_ctx *ctx)
{
	struct dmabuf_sync_object *contended_sobj = NULL;
	struct dmabuf_sync_object *res_sobj = NULL;
	struct dmabuf_sync_object *sobj = NULL;
	int ret;

	if (ctx)
		ww_acquire_init(ctx, &dmabuf_sync_ww_class);

retry:
	list_for_each_entry(sobj, &sync->syncs, head) {
		struct dmabuf_sync_reservation *rsvp = sobj->robj;

		if (WARN_ON(!rsvp))
			continue;

		mutex_lock(&rsvp->lock);

		/*
		 * Add a sync object to reservation object of dmabuf
		 * to handle the write-and-then-read ordering issue.
		 *
		 * For more details, see dmabuf_sync_wait_prev_objs function.
		 */
		list_add_tail(&sobj->r_head, &rsvp->syncs);

		/* Don't lock in case of read and read. */
		if (rsvp->accessed_type & DMA_BUF_ACCESS_R &&
		    sobj->access_type & DMA_BUF_ACCESS_R) {
			atomic_inc(&rsvp->shared_cnt);
			mutex_unlock(&rsvp->lock);
			continue;
		}

		if (sobj == res_sobj) {
			res_sobj = NULL;
			mutex_unlock(&rsvp->lock);
			continue;
		}

		mutex_unlock(&rsvp->lock);

		ret = ww_mutex_lock(&rsvp->sync_lock, ctx);
		if (ret < 0) {
			contended_sobj = sobj;

			if (ret == -EDEADLK)
				pr_warn("%s: deadlock = 0x%p\n",
					sync->name, sobj->dmabuf);
			goto err;
		}

		mutex_lock(&rsvp->lock);
		rsvp->locked = true;
		mutex_unlock(&rsvp->lock);

		/*
		 * Check if there is a sync object added to reservation object
		 * of dmabuf before current task takes a lock to the dmabuf.
		 * And ithen wait for the for the wake up event from other task
		 * if exists.
		 */
		dmabuf_sync_wait_prev_objs(sobj, rsvp, ctx);
	}

	if (ctx)
		ww_acquire_done(ctx);

	init_timer(&sync->timer);

	sync->timer.data = (unsigned long)sync;
	sync->timer.function = dmabuf_sync_lock_timeout;
	sync->timer.expires = jiffies + (HZ * MAX_SYNC_TIMEOUT);

	add_timer(&sync->timer);

	return 0;

err:
	list_for_each_entry_continue_reverse(sobj, &sync->syncs, head) {
		struct dmabuf_sync_reservation *rsvp = sobj->robj;

		mutex_lock(&rsvp->lock);

		/* Don't need to unlock in case of read and read. */
		if (atomic_add_unless(&rsvp->shared_cnt, -1, 1)) {
			mutex_unlock(&rsvp->lock);
			continue;
		}

		/*
		 * Delete a sync object from reservation object of dmabuf.
		 *
		 * The sync object was added to reservation object of dmabuf
		 * just before ww_mutex_lock() is called.
		 */
		DEL_OBJ_FROM_RSV(sobj, rsvp);
		mutex_unlock(&rsvp->lock);

		ww_mutex_unlock(&rsvp->sync_lock);

		mutex_lock(&rsvp->lock);
		rsvp->locked = false;
		mutex_unlock(&rsvp->lock);
	}

	if (res_sobj) {
		struct dmabuf_sync_reservation *rsvp = res_sobj->robj;

		mutex_lock(&rsvp->lock);

		if (!atomic_add_unless(&rsvp->shared_cnt, -1, 1)) {
			/*
			 * Delete a sync object from reservation object
			 * of dmabuf.
			 */
			DEL_OBJ_FROM_RSV(sobj, rsvp);
			mutex_unlock(&rsvp->lock);

			ww_mutex_unlock(&rsvp->sync_lock);

			mutex_lock(&rsvp->lock);
			rsvp->locked = false;
		}

		mutex_unlock(&rsvp->lock);
	}

	if (ret == -EDEADLK) {
		ww_mutex_lock_slow(&contended_sobj->robj->sync_lock, ctx);
		res_sobj = contended_sobj;

		goto retry;
	}

	if (ctx)
		ww_acquire_fini(ctx);

	return ret;
}

static void dmabuf_sync_unlock_objs(struct dmabuf_sync *sync,
					struct ww_acquire_ctx *ctx)
{
	struct dmabuf_sync_object *sobj;

	if (list_empty(&sync->syncs))
		return;

	mutex_lock(&sync->lock);

	list_for_each_entry(sobj, &sync->syncs, head) {
		struct dmabuf_sync_reservation *rsvp = sobj->robj;

		mutex_lock(&rsvp->lock);

		if (rsvp->polled) {
			rsvp->poll_event = true;
			rsvp->polled = false;
			wake_up_interruptible(&rsvp->poll_wait);
		}

		/*
		 * Wake up a task blocked by dmabuf_sync_wait_prev_objs().
		 *
		 * If sobj->waiting is true, the task is waiting for wake_up
		 * call. So wake up the task if a given time period was
		 * elapsed so current task was timed out.
		 */
		WAKE_UP_SYNC_OBJ(sobj);

		/* Delete a sync object from reservation object of dmabuf. */
		DEL_OBJ_FROM_RSV(sobj, rsvp);

		if (atomic_add_unless(&rsvp->shared_cnt, -1, 1)) {
			mutex_unlock(&rsvp->lock);
			continue;
		}

		mutex_unlock(&rsvp->lock);

		ww_mutex_unlock(&rsvp->sync_lock);

		mutex_lock(&rsvp->lock);
		rsvp->locked = false;
		mutex_unlock(&rsvp->lock);
	}

	mutex_unlock(&sync->lock);

	if (ctx)
		ww_acquire_fini(ctx);

	del_timer(&sync->timer);
}

/**
 * dmabuf_sync_is_supported - Check if dmabuf sync is supported or not.
 */
bool dmabuf_sync_is_supported(void)
{
	return dmabuf_sync_enabled == 1;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_is_supported);

/**
 * dmabuf_sync_init - Allocate and initialize a dmabuf sync.
 *
 * @priv: A device private data.
 * @name: A sync object name.
 *
 * This function should be called when a device context or an event
 * context such as a page flip event is created. And the created
 * dmabuf_sync object should be set to the context.
 * The caller can get a new sync object for buffer synchronization
 * through this function.
 */
struct dmabuf_sync *dmabuf_sync_init(const char *name,
					struct dmabuf_sync_priv_ops *ops,
					void *priv)
{
	struct dmabuf_sync *sync;

	sync = kzalloc(sizeof(*sync), GFP_KERNEL);
	if (!sync)
		return ERR_PTR(-ENOMEM);

	strncpy(sync->name, name, DMABUF_SYNC_NAME_SIZE);

	sync->ops = ops;
	sync->priv = priv;
	INIT_LIST_HEAD(&sync->syncs);
	mutex_init(&sync->lock);
	INIT_WORK(&sync->work, dmabuf_sync_timeout_worker);

	return sync;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_init);

/**
 * dmabuf_sync_fini - Release a given dmabuf sync.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation is failed after
 * dmabuf_sync_init call to release relevant resources, and after
 * dmabuf_sync_unlock function is called.
 */
void dmabuf_sync_fini(struct dmabuf_sync *sync)
{
	struct dmabuf_sync_object *sobj;

	if (WARN_ON(!sync))
		return;

	if (list_empty(&sync->syncs))
		goto free_sync;

	list_for_each_entry(sobj, &sync->syncs, head) {
		struct dmabuf_sync_reservation *rsvp = sobj->robj;

		mutex_lock(&rsvp->lock);

		if (rsvp->locked) {
			mutex_unlock(&rsvp->lock);
			ww_mutex_unlock(&rsvp->sync_lock);

			mutex_lock(&rsvp->lock);
			rsvp->locked = false;
		}

		mutex_unlock(&rsvp->lock);
	}

	/*
	 * If !list_empty(&sync->syncs) then it means that dmabuf_sync_put()
	 * or dmabuf_sync_put_all() was never called. So unreference all
	 * dmabuf objects added to sync->syncs, and remove them from the syncs.
	 */
	dmabuf_sync_put_all(sync);

free_sync:
	if (sync->ops && sync->ops->free)
		sync->ops->free(sync->priv);

	kfree(sync);
}
EXPORT_SYMBOL_GPL(dmabuf_sync_fini);

/*
 * dmabuf_sync_get_obj - Add a given object to sync's list.
 *
 * @sync: An object to dmabuf_sync structure.
 * @dmabuf: An object to dma_buf structure.
 * @type: A access type to a dma buf.
 *	The DMA_BUF_ACCESS_R means that this dmabuf could be accessed by
 *	others for read access. On the other hand, the DMA_BUF_ACCESS_W
 *	means that this dmabuf couldn't be accessed by others but would be
 *	accessed by caller's dma exclusively. And the DMA_BUF_ACCESS_DMA can be
 *	combined.
 *
 * This function creates and initializes a new dmabuf sync object and it adds
 * the dmabuf sync object to syncs list to track and manage all dmabufs.
 */
static int dmabuf_sync_get_obj(struct dmabuf_sync *sync, struct dma_buf *dmabuf,
					unsigned int type)
{
	struct dmabuf_sync_object *sobj;

	if (!dmabuf->sync)
		return -EFAULT;

	if (!IS_VALID_DMA_BUF_ACCESS_TYPE(type))
		return -EINVAL;

	if ((type & DMA_BUF_ACCESS_RW) == DMA_BUF_ACCESS_RW)
		type &= ~DMA_BUF_ACCESS_R;

	sobj = kzalloc(sizeof(*sobj), GFP_KERNEL);
	if (!sobj)
		return -ENOMEM;

	get_dma_buf(dmabuf);

	sobj->dmabuf = dmabuf;
	sobj->robj = dmabuf->sync;
	sobj->access_type = type;
	atomic_set(&sobj->refcnt, 1);
	init_waitqueue_head(&sobj->wq);

	mutex_lock(&sync->lock);
	list_add_tail(&sobj->head, &sync->syncs);
	mutex_unlock(&sync->lock);

	return 0;
}

/*
 * dmabuf_sync_put_obj - Release a given sync object.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation failed after
 * dmabuf_sync_get_obj call to release a given sync object.
 */
static void dmabuf_sync_put_obj(struct dmabuf_sync *sync,
					struct dma_buf *dmabuf)
{
	struct dmabuf_sync_object *sobj;

	mutex_lock(&sync->lock);

	list_for_each_entry(sobj, &sync->syncs, head) {
		if (sobj->dmabuf != dmabuf)
			continue;

		dma_buf_put(sobj->dmabuf);

		list_del_init(&sobj->head);

		if (!atomic_add_unless(&sobj->refcnt, -1, 1))
			kfree(sobj);
		break;
	}

	if (list_empty(&sync->syncs))
		sync->status = 0;

	mutex_unlock(&sync->lock);
}

/*
 * dmabuf_sync_put_objs - Release all sync objects of dmabuf_sync.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation failed after
 * dmabuf_sync_get_obj call to release all sync objects.
 */
static void dmabuf_sync_put_objs(struct dmabuf_sync *sync)
{
	struct dmabuf_sync_object *sobj, *next;

	mutex_lock(&sync->lock);

	list_for_each_entry_safe(sobj, next, &sync->syncs, head) {
		dma_buf_put(sobj->dmabuf);

		list_del_init(&sobj->head);

		if (!atomic_add_unless(&sobj->refcnt, -1, 1))
			kfree(sobj);
	}

	mutex_unlock(&sync->lock);

	sync->status = 0;
}

/**
 * dmabuf_sync_lock - lock all dmabufs added to syncs list.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * The caller should call this function prior to CPU or DMA access to
 * the dmabufs so that others can not access the dmabufs.
 * Internally, this function avoids dead lock issue with ww-mutex.
 */
int dmabuf_sync_lock(struct dmabuf_sync *sync)
{
	int ret;

	if (!sync)
		return -EFAULT;

	if (list_empty(&sync->syncs))
		return -EINVAL;

	if (sync->status != DMABUF_SYNC_GOT)
		return -EINVAL;

	ret = dmabuf_sync_lock_objs(sync, &sync->ctx);
	if (ret < 0)
		return ret;

	sync->status = DMABUF_SYNC_LOCKED;

	dmabuf_sync_cache_ops(sync);

	return ret;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_lock);

/**
 * dmabuf_sync_unlock - unlock all objects added to syncs list.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * The caller should call this function after CPU or DMA access to
 * the dmabufs is completed so that others can access the dmabufs.
 */
int dmabuf_sync_unlock(struct dmabuf_sync *sync)
{
	if (!sync)
		return -EFAULT;

	/* If current dmabuf sync object wasn't reserved then just return. */
	if (sync->status != DMABUF_SYNC_LOCKED)
		return -EAGAIN;

	dmabuf_sync_unlock_objs(sync, &sync->ctx);

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_unlock);

/**
 * dmabuf_sync_single_lock - lock a dma buf.
 *
 * @dmabuf: A dma buf object that tries to lock.
 * @type: A access type to a dma buf.
 *	The DMA_BUF_ACCESS_R means that this dmabuf could be accessed by
 *	others for read access. On the other hand, the DMA_BUF_ACCESS_W
 *	means that this dmabuf couldn't be accessed by others but would be
 *	accessed by caller's dma exclusively. And the DMA_BUF_ACCESS_DMA can
 *	be combined with other.
 * @wait: Indicate whether caller is blocked or not.
 *	true means that caller will be blocked, and false means that this
 *	function will return -EAGAIN if this caller can't take the lock
 *	right now.
 *
 * The caller should call this function prior to CPU or DMA access to the dmabuf
 * so that others cannot access the dmabuf.
 */
int dmabuf_sync_single_lock(struct dma_buf *dmabuf, unsigned int type,
				bool wait)
{
	struct dmabuf_sync_reservation *robj;
	struct dmabuf_sync_object *sobj;

	if (!dmabuf->sync)
		return -EFAULT;

	if (!IS_VALID_DMA_BUF_ACCESS_TYPE(type))
		return -EINVAL;

	get_dma_buf(dmabuf);
	robj = dmabuf->sync;

	sobj = kzalloc(sizeof(*sobj), GFP_KERNEL);
	if (!sobj) {
		dma_buf_put(dmabuf);
		return -ENOMEM;
	}

	sobj->dmabuf = dmabuf;
	sobj->task = (unsigned long)current;
	atomic_set(&sobj->refcnt, 1);
	init_waitqueue_head(&sobj->wq);

	mutex_lock(&robj->lock);

	/*
	 * Add a sync object to reservation object of dmabuf to handle
	 * the write-and-then-read ordering issue.
	 */
	list_add_tail(&sobj->r_head, &robj->syncs);

	/* Don't lock in case of read and read. */
	if (robj->accessed_type & DMA_BUF_ACCESS_R && type & DMA_BUF_ACCESS_R) {
		atomic_inc(&robj->shared_cnt);
		mutex_unlock(&robj->lock);
		return 0;
	}

	/*
	 * In case of F_SETLK, just return -EAGAIN if this dmabuf has already
	 * been locked.
	 */
	if (!wait && robj->locked) {
		list_del_init(&sobj->r_head);
		mutex_unlock(&robj->lock);
		kfree(sobj);
		dma_buf_put(dmabuf);
		return -EAGAIN;
	}

	mutex_unlock(&robj->lock);

	/* Unlocked by dmabuf_sync_single_unlock or dmabuf_sync_unlock. */
	mutex_lock(&robj->sync_lock.base);

	mutex_lock(&robj->lock);
	robj->locked = true;
	mutex_unlock(&robj->lock);

	/*
	 * Check if there is a sync object added to reservation object of
	 * dmabuf before current task takes a lock to the dmabuf, and wait
	 * for the for the wake up event from other task if exists.
	 */
	dmabuf_sync_wait_prev_objs(sobj, robj, NULL);

	mutex_lock(&robj->lock);
	dmabuf_sync_single_cache_ops(dmabuf, type);
	mutex_unlock(&robj->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_single_lock);

/**
 * dmabuf_sync_single_unlock - unlock a dma buf.
 *
 * @dmabuf: A dma buf object that tries to unlock.
 *
 * The caller should call this function after CPU or DMA access to
 * the dmabuf is completed so that others can access the dmabuf.
 */
void dmabuf_sync_single_unlock(struct dma_buf *dmabuf)
{
	struct dmabuf_sync_reservation *robj;
	struct dmabuf_sync_object *sobj, *next;

	if (!dmabuf->sync) {
		WARN_ON(1);
		return;
	}

	robj = dmabuf->sync;

	mutex_lock(&robj->lock);

	if (robj->polled) {
		robj->poll_event = true;
		robj->polled = false;
		wake_up_interruptible(&robj->poll_wait);
	}

	/*
	 * Wake up a blocked task/tasks by dmabuf_sync_wait_prev_objs()
	 * with two steps.
	 *
	 * 1. Wake up a task waiting for the wake up event to a sync object
	 *	of same task, and remove the sync object from reservation
	 *	object of dmabuf, and then go to out: requested by same task.
	 * 2. Wait up a task waiting for the wake up event to a sync object
	 *	of other task, and remove the sync object if not existed
	 *	at step 1: requested by other task.
	 *
	 * The reason, we have to handle it with the above two steps,
	 * is that fcntl system call is called with a file descriptor so
	 * kernel side cannot be aware of which sync object of robj->syncs
	 * should be waked up and deleted at this function.
	 * So for this, we use the above two steps to find a sync object
	 * to be waked up.
	 */
	list_for_each_entry_safe(sobj, next, &robj->syncs, r_head) {
		if (sobj->task == (unsigned long)current) {
			/*
			 * Wake up a task blocked by
			 * dmabuf_sync_wait_prev_objs().
			 */
			WAKE_UP_SYNC_OBJ(sobj);

			list_del_init(&sobj->r_head);

			if (!atomic_add_unless(&sobj->refcnt, -1, 1))
				kfree(sobj);
			goto out;
		}
	}

	list_for_each_entry_safe(sobj, next, &robj->syncs, r_head) {
		if (sobj->task) {
			/*
			 * Wake up a task blocked by
			 * dmabuf_sync_wait_prev_objs().
			 */
			WAKE_UP_SYNC_OBJ(sobj);

			list_del_init(&sobj->r_head);

			if (!atomic_add_unless(&sobj->refcnt, -1, 1))
				kfree(sobj);
			break;
		}
	}

out:
	if (atomic_add_unless(&robj->shared_cnt, -1 , 1)) {
		mutex_unlock(&robj->lock);
		dma_buf_put(dmabuf);
		return;
	}

	mutex_unlock(&robj->lock);

	mutex_unlock(&robj->sync_lock.base);

	mutex_lock(&robj->lock);
	robj->locked = false;
	mutex_unlock(&robj->lock);

	dma_buf_put(dmabuf);

	return;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_single_unlock);

/**
 * dmabuf_sync_get - Get dmabuf sync object.
 *
 * @sync: An object to dmabuf_sync structure.
 * @sync_buf: A dmabuf object to be synchronized with others.
 * @type: A access type to a dma buf.
 *	The DMA_BUF_ACCESS_R means that this dmabuf could be accessed by
 *	others for read access. On the other hand, the DMA_BUF_ACCESS_W
 *	means that this dmabuf couldn't be accessed by others but would be
 *	accessed by caller's dma exclusively. And the DMA_BUF_ACCESS_DMA can
 *	be combined with other.
 *
 * This function should be called after dmabuf_sync_init function is called.
 * The caller can tie up multiple dmabufs into one sync object by calling this
 * function several times. Internally, this function allocates
 * a dmabuf_sync_object and adds a given dmabuf to it, and also takes
 * a reference to a dmabuf.
 */
int dmabuf_sync_get(struct dmabuf_sync *sync, void *sync_buf, unsigned int type)
{
	int ret;

	if (!sync || !sync_buf)
		return -EFAULT;

	ret = dmabuf_sync_get_obj(sync, sync_buf, type);
	if (ret < 0)
		return ret;

	sync->status = DMABUF_SYNC_GOT;

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_get);

/**
 * dmabuf_sync_put - Put dmabuf sync object to a given dmabuf.
 *
 * @sync: An object to dmabuf_sync structure.
 * @dmabuf: An dmabuf object.
 *
 * This function should be called if some operation is failed after
 * dmabuf_sync_get function is called to release the dmabuf, or
 * dmabuf_sync_unlock function is called. Internally, this function
 * removes a given dmabuf from a sync object and remove the sync object.
 * At this time, the dmabuf is putted.
 */
void dmabuf_sync_put(struct dmabuf_sync *sync, struct dma_buf *dmabuf)
{
	if (!sync || !dmabuf) {
		WARN_ON(1);
		return;
	}

	if (list_empty(&sync->syncs))
		return;

	dmabuf_sync_put_obj(sync, dmabuf);
}
EXPORT_SYMBOL_GPL(dmabuf_sync_put);

/**
 * dmabuf_sync_put_all - Put dmabuf sync object to dmabufs.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation is failed after
 * dmabuf_sync_get function is called to release all sync objects, or
 * dmabuf_sync_unlock function is called. Internally, this function
 * removes dmabufs from a sync object and remove the sync object.
 * At this time, all dmabufs are putted.
 */
void dmabuf_sync_put_all(struct dmabuf_sync *sync)
{
	if (!sync) {
		WARN_ON(1);
		return;
	}

	if (list_empty(&sync->syncs))
		return;

	dmabuf_sync_put_objs(sync);
}
EXPORT_SYMBOL_GPL(dmabuf_sync_put_all);
