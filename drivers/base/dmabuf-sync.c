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

#define MAX_SYNC_TIMEOUT	5 /* Second. */

int dmabuf_sync_enabled = 1;

MODULE_PARM_DESC(enabled, "Check if dmabuf sync is supported or not");
module_param_named(enabled, dmabuf_sync_enabled, int, 0444);

DEFINE_WW_CLASS(dmabuf_sync_ww_class);
EXPORT_SYMBOL(dmabuf_sync_ww_class);

static void dmabuf_sync_timeout_worker(struct work_struct *work)
{
	struct dmabuf_sync *sync = container_of(work, struct dmabuf_sync, work);
	struct dmabuf_sync_object *sobj;

	mutex_lock(&sync->lock);

	list_for_each_entry(sobj, &sync->syncs, head) {
		if (WARN_ON(!sobj->robj))
			continue;

		mutex_lock(&sobj->robj->lock);

		printk(KERN_WARNING "%s: timeout = 0x%x [type = %d, " \
					"refcnt = %d, locked = %d]\n",
					sync->name, (u32)sobj->dmabuf,
					sobj->access_type,
					atomic_read(&sobj->robj->shared_cnt),
					sobj->robj->locked);

		/* unlock only valid sync object. */
		if (!sobj->robj->locked) {
			mutex_unlock(&sobj->robj->lock);
			continue;
		}

		if (sobj->robj->shared &&
		    atomic_add_unless(&sobj->robj->shared_cnt, -1, 1)) {
			mutex_unlock(&sobj->robj->lock);
			continue;
		}

		mutex_unlock(&sobj->robj->lock);

		ww_mutex_unlock(&sobj->robj->sync_lock);

		mutex_lock(&sobj->robj->lock);

		if (sobj->access_type & DMA_BUF_ACCESS_R)
			printk(KERN_WARNING "%s: r-unlocked = 0x%x\n",
					sync->name, (u32)sobj->dmabuf);
		else
			printk(KERN_WARNING "%s: w-unlocked = 0x%x\n",
					sync->name, (u32)sobj->dmabuf);

		mutex_unlock(&sobj->robj->lock);
	}

	sync->status = 0;
	mutex_unlock(&sync->lock);

	dmabuf_sync_put_all(sync);
	dmabuf_sync_fini(sync);
}

static void dmabuf_sync_lock_timeout(unsigned long arg)
{
	struct dmabuf_sync *sync = (struct dmabuf_sync *)arg;

	schedule_work(&sync->work);
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
		if (WARN_ON(!sobj->robj))
			continue;

		mutex_lock(&sobj->robj->lock);

		/* Don't lock in case of read and read. */
		if (sobj->robj->accessed_type & DMA_BUF_ACCESS_R &&
		    sobj->access_type & DMA_BUF_ACCESS_R) {
			atomic_inc(&sobj->robj->shared_cnt);
			sobj->robj->shared = true;
			mutex_unlock(&sobj->robj->lock);
			continue;
		}

		if (sobj == res_sobj) {
			res_sobj = NULL;
			mutex_unlock(&sobj->robj->lock);
			continue;
		}

		mutex_unlock(&sobj->robj->lock);

		ret = ww_mutex_lock(&sobj->robj->sync_lock, ctx);
		if (ret < 0) {
			contended_sobj = sobj;

			if (ret == -EDEADLK)
				printk(KERN_WARNING"%s: deadlock = 0x%x\n",
					sync->name, (u32)sobj->dmabuf);
			goto err;
		}

		mutex_lock(&sobj->robj->lock);
		sobj->robj->locked = true;

		mutex_unlock(&sobj->robj->lock);
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
		mutex_lock(&sobj->robj->lock);

		/* Don't need to unlock in case of read and read. */
		if (atomic_add_unless(&sobj->robj->shared_cnt, -1, 1)) {
			mutex_unlock(&sobj->robj->lock);
			continue;
		}

		ww_mutex_unlock(&sobj->robj->sync_lock);
		sobj->robj->locked = false;

		mutex_unlock(&sobj->robj->lock);
	}

	if (res_sobj) {
		mutex_lock(&res_sobj->robj->lock);

		if (!atomic_add_unless(&res_sobj->robj->shared_cnt, -1, 1)) {
			ww_mutex_unlock(&res_sobj->robj->sync_lock);
			res_sobj->robj->locked = false;
		}

		mutex_unlock(&res_sobj->robj->lock);
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
		mutex_lock(&sobj->robj->lock);

		if (sobj->robj->shared) {
			if (atomic_add_unless(&sobj->robj->shared_cnt, -1,
						1)) {
				mutex_unlock(&sobj->robj->lock);
				continue;
			}

			mutex_unlock(&sobj->robj->lock);

			ww_mutex_unlock(&sobj->robj->sync_lock);

			mutex_lock(&sobj->robj->lock);
			sobj->robj->shared = false;
			sobj->robj->locked = false;
		} else {
			mutex_unlock(&sobj->robj->lock);

			ww_mutex_unlock(&sobj->robj->sync_lock);

			mutex_lock(&sobj->robj->lock);
			sobj->robj->locked = false;
		}

		mutex_unlock(&sobj->robj->lock);
	}

	mutex_unlock(&sync->lock);

	if (ctx)
		ww_acquire_fini(ctx);

	del_timer(&sync->timer);
}

/**
 * is_dmabuf_sync_supported - Check if dmabuf sync is supported or not.
 */
bool is_dmabuf_sync_supported(void)
{
	return dmabuf_sync_enabled == 1;
}
EXPORT_SYMBOL(is_dmabuf_sync_supported);

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
struct dmabuf_sync *dmabuf_sync_init(void *priv, const char *name)
{
	struct dmabuf_sync *sync;

	sync = kzalloc(sizeof(*sync), GFP_KERNEL);
	if (!sync)
		return ERR_PTR(-ENOMEM);

	strncpy(sync->name, name, ARRAY_SIZE(sync->name) - 1);

	sync->priv = priv;
	INIT_LIST_HEAD(&sync->syncs);
	mutex_init(&sync->lock);
	INIT_WORK(&sync->work, dmabuf_sync_timeout_worker);

	return sync;
}
EXPORT_SYMBOL(dmabuf_sync_init);

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
	if (WARN_ON(!sync))
		return;

	kfree(sync);
}
EXPORT_SYMBOL(dmabuf_sync_fini);

/*
 * dmabuf_sync_get_obj - Add a given object to syncs list.
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

	if (!dmabuf->sync) {
		WARN_ON(1);
		return -EFAULT;
	}

	if (!IS_VALID_DMA_BUF_ACCESS_TYPE(type))
		return -EINVAL;

	if ((type & DMA_BUF_ACCESS_RW) == DMA_BUF_ACCESS_RW)
		type &= ~DMA_BUF_ACCESS_R;

	sobj = kzalloc(sizeof(*sobj), GFP_KERNEL);
	if (!sobj) {
		WARN_ON(1);
		return -ENOMEM;
	}

	sobj->dmabuf = dmabuf;
	sobj->robj = dmabuf->sync;

	mutex_lock(&sync->lock);
	list_add_tail(&sobj->head, &sync->syncs);
	mutex_unlock(&sync->lock);

	get_dma_buf(dmabuf);

	mutex_lock(&sobj->robj->lock);
	sobj->access_type = type;
	mutex_unlock(&sobj->robj->lock);

	return 0;
}

/*
 * dmabuf_sync_put_obj - Release a given sync object.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation is failed after
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
 * This function should be called if some operation is failed after
 * dmabuf_sync_get_obj call to release all sync objects.
 */
static void dmabuf_sync_put_objs(struct dmabuf_sync *sync)
{
	struct dmabuf_sync_object *sobj, *next;

	mutex_lock(&sync->lock);

	list_for_each_entry_safe(sobj, next, &sync->syncs, head) {
		dma_buf_put(sobj->dmabuf);

		list_del_init(&sobj->head);
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

	if (!sync) {
		WARN_ON(1);
		return -EFAULT;
	}

	if (list_empty(&sync->syncs))
		return -EINVAL;

	if (sync->status != DMABUF_SYNC_GOT)
		return -EINVAL;

	ret = dmabuf_sync_lock_objs(sync, &sync->ctx);
	if (ret < 0) {
		WARN_ON(1);
		return ret;
	}

	sync->status = DMABUF_SYNC_LOCKED;

	return ret;
}
EXPORT_SYMBOL(dmabuf_sync_lock);

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
	if (!sync) {
		WARN_ON(1);
		return -EFAULT;
	}

	/* If current dmabuf sync object wasn't reserved then just return. */
	if (sync->status != DMABUF_SYNC_LOCKED)
		return -EAGAIN;

	dmabuf_sync_unlock_objs(sync, &sync->ctx);

	return 0;
}
EXPORT_SYMBOL(dmabuf_sync_unlock);

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

	if (!dmabuf->sync) {
		WARN_ON(1);
		return -EFAULT;
	}

	if (!IS_VALID_DMA_BUF_ACCESS_TYPE(type)) {
		WARN_ON(1);
		return -EINVAL;
	}

	get_dma_buf(dmabuf);
	robj = dmabuf->sync;

	mutex_lock(&robj->lock);

	/* Don't lock in case of read and read. */
	if (robj->accessed_type & DMA_BUF_ACCESS_R && type & DMA_BUF_ACCESS_R) {
		atomic_inc(&robj->shared_cnt);
		robj->shared = true;
		mutex_unlock(&robj->lock);
		return 0;
	}

	/*
	 * In case of F_SETLK, just return -EAGAIN if this dmabuf has already
	 * been locked.
	 */
	if (!wait && robj->locked) {
		mutex_unlock(&robj->lock);
		dma_buf_put(dmabuf);
		return -EAGAIN;
	}

	mutex_unlock(&robj->lock);

	mutex_lock(&robj->sync_lock.base);

	mutex_lock(&robj->lock);
	robj->locked = true;
	mutex_unlock(&robj->lock);

	return 0;
}
EXPORT_SYMBOL(dmabuf_sync_single_lock);

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

	if (!dmabuf->sync) {
		WARN_ON(1);
		return;
	}

	robj = dmabuf->sync;

	mutex_lock(&robj->lock);

	if (robj->shared) {
		if (atomic_add_unless(&robj->shared_cnt, -1 , 1)) {
			mutex_unlock(&robj->lock);
			return;
		}

		robj->shared = false;
	}

	mutex_unlock(&robj->lock);

	mutex_unlock(&robj->sync_lock.base);

	mutex_lock(&robj->lock);
	robj->locked = false;
	mutex_unlock(&robj->lock);

	dma_buf_put(dmabuf);

	return;
}
EXPORT_SYMBOL(dmabuf_sync_single_unlock);

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

	if (!sync || !sync_buf) {
		WARN_ON(1);
		return -EFAULT;
	}

	ret = dmabuf_sync_get_obj(sync, sync_buf, type);
	if (ret < 0) {
		WARN_ON(1);
		return ret;
	}

	sync->status = DMABUF_SYNC_GOT;

	return 0;
}
EXPORT_SYMBOL(dmabuf_sync_get);

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
EXPORT_SYMBOL(dmabuf_sync_put);

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
EXPORT_SYMBOL(dmabuf_sync_put_all);
