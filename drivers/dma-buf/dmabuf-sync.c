/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Chanho Park <chanho61.park@samsung.com>
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
#include <linux/delay.h>

#include <linux/seqno-fence.h>
#include <linux/reservation.h>
#include <linux/dmabuf-sync.h>

#define DEFAULT_SYNC_TIMEOUT	5000	/* in millisecond. */
#define DEFAULT_WORKER_TIMEOUT	500	/* in millisecond. */
#define BEGIN_CPU_ACCESS(old, new_type)	\
			((old->accessed_type & DMA_BUF_ACCESS_DMA_W) == \
			  DMA_BUF_ACCESS_DMA_W && new_type == DMA_BUF_ACCESS_R)

#define END_CPU_ACCESS(old, new_type)	\
			(((old->accessed_type == DMA_BUF_ACCESS_W) || \
			 (old->accessed_type == DMA_BUF_ACCESS_RW)) && \
			 new_type & DMA_BUF_ACCESS_DMA)

static int dmabuf_sync_enabled = 0;
static unsigned long seqno;
static LIST_HEAD(orders);
static DEFINE_SPINLOCK(orders_lock);
static struct timer_list fence_free_worker;

MODULE_PARM_DESC(enabled, "Check if dmabuf sync is supported or not");
module_param_named(enabled, dmabuf_sync_enabled, int, 0444);

static void sobj_release(struct kref *kref)
{
	struct dmabuf_sync_object *sobj =
		container_of(kref, struct dmabuf_sync_object, refcount);

	fence_put(&sobj->sfence->base);
	kfree(sobj);
}

/*
 * - sobj_get - increases refcount of the dmabuf_sync_object
 * @sobj:	[in]	sync object to increase refcount of
 */
static inline void sobj_get(struct dmabuf_sync_object *sobj)
{
	if (sobj)
		kref_get(&sobj->refcount);
}

/*
 * sobj_put - decreases refcount of the dmabuf_sync_object
 * @sobj:	[in]	sync object to reduce refcount of
 */
static inline void sobj_put(struct dmabuf_sync_object *sobj)
{
	if (sobj)
		kref_put(&sobj->refcount, sobj_release);
}


static const char *dmabuf_sync_get_driver_name(struct fence *fence)
{
	return NULL;
}

static const char *dmabuf_sync_get_timeline_name(struct fence *fence)
{
	return NULL;
}

static bool dmabuf_sync_enable_sw_signaling(struct fence *fence)
{
	return true;
}

static void sync_free_worker_handler(unsigned long data)
{
	struct dmabuf_sync *sync = (struct dmabuf_sync *)data;

	kfree(sync);
}

static void setup_sync_free_worker(struct dmabuf_sync *sync)
{
	init_timer(&sync->sync_free_worker);
	sync->sync_free_worker.expires = jiffies +
				msecs_to_jiffies(DEFAULT_WORKER_TIMEOUT);
	sync->sync_free_worker.data = (unsigned long)sync;
	sync->sync_free_worker.function = sync_free_worker_handler;
	add_timer(&sync->sync_free_worker);
}

static void sfence_object_release(struct fence *fence)
{
	struct seqno_fence *sf = to_seqno_fence(fence);
	struct dmabuf_sync *sync = to_dmabuf_sync(sf);

	/* TODO. find out better way. */
	setup_sync_free_worker(sync);
}

static const struct fence_ops fence_default_ops = {
	.get_driver_name = dmabuf_sync_get_driver_name,
	.get_timeline_name = dmabuf_sync_get_timeline_name,
	.enable_signaling = dmabuf_sync_enable_sw_signaling,
	.wait = fence_default_wait,
	.release = sfence_object_release,
};

/**
 * dmabuf_sync_is_supported - Check if dmabuf sync is supported or not.
 */
bool dmabuf_sync_is_supported(void)
{
	return dmabuf_sync_enabled == 1;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_is_supported);

/*
 * Perform cache operation according to access type.
 *
 * This function is called by dmabuf_sync_wait or dmabuf_sync_wait_all function
 * to change the ownership of a buffer - for cache coherence between CPU and
 * DMA -, CPU -> DMA or DMA -> CPU after signaled.
 */
static void dmabuf_sync_cache_ops(struct dmabuf_sync_object *sobj)
{
	struct dma_buf *dmabuf = sobj->dmabuf;

	/* It doesn't need cache operation if access first time. */
	if (!dmabuf->resv->accessed_type)
		goto out;

	if (END_CPU_ACCESS(dmabuf->resv, sobj->access_type))
		/* cache clean */
		dma_buf_end_cpu_access(dmabuf, 0, dmabuf->size,
						DMA_TO_DEVICE);
	else if (BEGIN_CPU_ACCESS(dmabuf->resv, sobj->access_type))
		/* cache invalidate */
		dma_buf_begin_cpu_access(dmabuf, 0, dmabuf->size,
						DMA_FROM_DEVICE);

out:
	/* Update access type to new one. */
	dmabuf->resv->accessed_type = sobj->access_type;
}

/**
 * dmabuf_sync_check - check whether a given buffer is required sync operation.
 *
 * @dmabuf: An object to dma_buf structure.
 *
 * This function should be called by dmabuf_sync_wait or dmabuf_sync_wait_all
 * function before it makes current thread to be blocked. It returnes true if
 * buffer sync is needed else false.
 */
struct fence *dmabuf_sync_check(struct dma_buf *dmabuf)
{
	struct reservation_object_list *rol;
	struct reservation_object *ro;
	struct fence *fence = NULL;
	unsigned int i;

	rcu_read_lock();
	ro = rcu_dereference(dmabuf->resv);
	if (!ro)
		goto unlock_rcu;

	/* First, check there is a fence for write access. */
	fence = rcu_dereference(ro->fence_excl);
	if (fence)
		goto unlock_rcu;

	/* Check if there is any fences requested for read access. */
	if (ro->fence) {
		rol = rcu_dereference(ro->fence);

		for (i = 0; i < rol->shared_count; i++) {
			fence = rcu_dereference(rol->shared[i]);
			if (fence)
				break;
		}
	}

unlock_rcu:
	rcu_read_unlock();
	return fence;
}

/**
 * dmabuf_sync_init - Allocate and initialize a dmabuf sync.
 *
 * @name: A dmabuf_sync object name.
 * @ops: operation callback specitic to device.
 * @priv: A device private data.
 *
 * This function should be called by DMA driver after device context
 * is created.
 * The created dmabuf_sync object should be set to the context of driver.
 * Each DMA driver and task should have one dmabuf_sync object.
 *
 * The caller can get a new dmabuf_sync object for buffer synchronization
 * through this function, which is used when driver drivers use dmabuf sync
 * APIs.
 * It returns dmabuf_sync object if true, else error pointer.
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
	spin_lock_init(&sync->lock);
	spin_lock_init(&sync->flock);

	sync->sfence.ops = &fence_default_ops;
	fence_init(&sync->sfence.base, &seqno_fence_ops, &sync->flock,
			(unsigned)priv, ++seqno);

	return sync;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_init);

/*
 * dmabuf_sync_get_obj - Add a given object to syncs list.
 *
 * @sync: An object to dmabuf_sync structure.
 * @dmabuf: An object to dma_buf structure.
 * @ctx: not used yet.
 * @type: A access type to a dma buf.
 *	The DMA_BUF_ACCESS_R means that this dmabuf could be accessed by
 *	others for read access. On the other hand, the DMA_BUF_ACCESS_W
 *	means that this dmabuf couldn't be accessed by others but would be
 *	accessed by caller's dma exclusively. And the DMA_BUF_ACCESS_DMA can be
 *	combined.
 *
 * This function creates and initializes a new dmabuf_sync_object and adds
 * the object to a given syncs list. A dmabuf_sync_object has an dmabuf object.
 */
static int dmabuf_sync_get_obj(struct dmabuf_sync *sync, struct dma_buf *dmabuf,
					unsigned int ctx, unsigned int type)
{
	struct dmabuf_sync_object *sobj;
	unsigned long s_flags;

	if (!sync)
		return -EFAULT;

	if (!IS_VALID_DMA_BUF_ACCESS_TYPE(type))
		return -EINVAL;

	if ((type & DMA_BUF_ACCESS_RW) == DMA_BUF_ACCESS_RW)
		type &= ~DMA_BUF_ACCESS_R;

	sobj = kzalloc(sizeof(*sobj), GFP_KERNEL);
	if (!sobj)
		return -ENOMEM;

	kref_init(&sobj->refcount);
	sobj->access_type = type;
	sobj->sfence = &sync->sfence;
	fence_get(&sobj->sfence->base);
	sobj->dmabuf = dmabuf;
	get_dma_buf(dmabuf);

	spin_lock_irqsave(&sync->lock, s_flags);
	sync->obj_cnt++;
	list_add_tail(&sobj->l_head, &sync->syncs);
	sobj_get(sobj);
	spin_unlock_irqrestore(&sync->lock, s_flags);

	return 0;
}

/*
 * dmabuf_sync_put_objs - Release all sync objects of dmabuf_sync.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation failed after
 * dmabuf_sync_get_obj call to release all sync objects or
 * after device driver or task completes the use to all buffers.
 */
static void dmabuf_sync_put_objs(struct dmabuf_sync *sync)
{
	struct dmabuf_sync_object *sobj, *next;
	unsigned long s_flags;

	spin_lock_irqsave(&sync->lock, s_flags);
	list_for_each_entry_safe(sobj, next, &sync->syncs, l_head) {

		spin_unlock_irqrestore(&sync->lock, s_flags);

		sync->obj_cnt--;
		list_del_init(&sobj->l_head);
		fence_put(&sobj->sfence->base);
		sobj_put(sobj);

		spin_lock_irqsave(&sync->lock, s_flags);
	}
	spin_unlock_irqrestore(&sync->lock, s_flags);
}

/*
 * dmabuf_sync_put_obj - Release a given sync object.
 *
 * @sync: An object to dmabuf_sync structure.
 * @dmabuf: A dmabuf object to be released.
 *
 * This function should be called if some operation failed after
 * dmabuf_sync_get_obj call to release a given sync object or
 * after device driver or task completes the use of a buffer.
 */
static void dmabuf_sync_put_obj(struct dmabuf_sync *sync,
					struct dma_buf *dmabuf)
{
	struct dmabuf_sync_object *sobj, *next;
	unsigned long s_flags;

	spin_lock_irqsave(&sync->lock, s_flags);
	list_for_each_entry_safe(sobj, next, &sync->syncs, l_head) {
		spin_unlock_irqrestore(&sync->lock, s_flags);

		if (sobj->dmabuf != dmabuf)
			continue;

		sync->obj_cnt--;
		list_del_init(&sobj->l_head);
		fence_put(&sobj->sfence->base);
		sobj_put(sobj);

		spin_lock_irqsave(&sync->lock, s_flags);
		break;
	}
	spin_unlock_irqrestore(&sync->lock, s_flags);
}

/**
 * dmabuf_sync_get - Add a given dmabuf object to dmabuf_sync object.
 *
 * @sync: An object to dmabuf_sync structure.
 * @sync_buf: A dmabuf object to be synchronized with others.
 * @ctx: not used yet.
 * @type: A access type to a dma buf.
 *	The DMA_BUF_ACCESS_R means that this dmabuf could be accessed by
 *	others for read access. On the other hand, the DMA_BUF_ACCESS_W
 *	means that this dmabuf couldn't be accessed by others but would be
 *	accessed by caller's dma exclusively. And the DMA_BUF_ACCESS_DMA can
 *	be combined with other.
 *
 * This function should be called after dmabuf_sync_init function is called.
 * The caller can tie up multiple dmabufs into its own dmabuf_sync object
 * by calling this function several times.
 */
int dmabuf_sync_get(struct dmabuf_sync *sync, void *sync_buf,
			unsigned int ctx, unsigned int type)
{
	if (!sync || !sync_buf)
		return -EFAULT;

	return dmabuf_sync_get_obj(sync, sync_buf, ctx, type);
}
EXPORT_SYMBOL_GPL(dmabuf_sync_get);

/**
 * dmabuf_sync_put - Put dmabuf sync object to a given dmabuf.
 *
 * @sync: An object to dmabuf_sync structure.
 * @dmabuf: An dmabuf object.
 *
 * This function should be called if some operation failed after
 * dmabuf_sync_get function is called to release the dmabuf or
 * after device driver or task completes the use of the dmabuf.
 */
void dmabuf_sync_put(struct dmabuf_sync *sync, struct dma_buf *dmabuf)
{
	unsigned long flags;

	if (!sync || !dmabuf) {
		WARN_ON(1);
		return;
	}

	spin_lock_irqsave(&sync->lock, flags);
	if (list_empty(&sync->syncs)) {
		spin_unlock_irqrestore(&sync->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&sync->lock, flags);

	dmabuf_sync_put_obj(sync, dmabuf);
}
EXPORT_SYMBOL_GPL(dmabuf_sync_put);

/**
 * dmabuf_sync_put_all - Release all dmabuf_sync_objects to a given
 *			dmabuf_sync object.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation is failed after
 * dmabuf_sync_get function is called to release all dmabuf_sync_object
 * objects, or after DMA driver or task completes the use of all dmabufs.
 */
void dmabuf_sync_put_all(struct dmabuf_sync *sync)
{
	unsigned long flags;

	if (!sync) {
		WARN_ON(1);
		return;
	}

	spin_lock_irqsave(&sync->lock, flags);
	if (list_empty(&sync->syncs)) {
		spin_unlock_irqrestore(&sync->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&sync->lock, flags);

	dmabuf_sync_put_objs(sync);
}
EXPORT_SYMBOL_GPL(dmabuf_sync_put_all);

static void dmabuf_sync_debug_fence(struct dmabuf_sync_object *sobj)
{
	struct dma_buf *dmabuf = sobj->dmabuf;
	struct reservation_object *resv;
	struct fence *fence_excl, **shared;
	unsigned int shared_count, i;

	rcu_read_lock();

	resv = rcu_dereference(dmabuf->resv);
	reservation_object_get_fences_rcu(resv,
			&fence_excl,
			&shared_count,
			&shared);

	printk("%s: new fence = 0x%p(type = %d), ", __func__,
			&sobj->sfence->base,
			sobj->access_type);


	fence_excl = rcu_dereference(resv->fence_excl);
	if (fence_excl) {
		printk("excl_fence = 0x%p(%d), ", fence_excl,
			atomic_read(&fence_excl->refcount.refcount));
		fence_put(fence_excl);
	}

	for (i = 0; i < shared_count; i++) {
		if (shared[i]) {
			printk("shared[%d] = 0x%p(%d), ",
				i, shared[i],
				atomic_read(&shared[i]->refcount.refcount));
			fence_put(shared[i]);
		}
	}

	rcu_read_unlock();

	printk("shared_count = %d\n", shared_count);
}

static void fence_free_worker_handler(unsigned long data)
{
	struct dma_buf *dmabuf = (struct dma_buf *)data;
	struct reservation_object *resv = dmabuf->resv;

	if (resv && resv->fence_excl) {
		struct fence *fence_excl = resv->fence_excl;

		if (atomic_read(&fence_excl->refcount.refcount) > 0)
			fence_put(fence_excl);
	}
}

static void setup_fence_free_worker(struct dmabuf_sync_object *sobj)
{
	if (timer_pending(&fence_free_worker)) {
		del_timer(&fence_free_worker);
		dma_buf_put(sobj->dmabuf);
	}

	get_dma_buf(sobj->dmabuf);

	init_timer(&fence_free_worker);
	fence_free_worker.expires = jiffies +
				msecs_to_jiffies(DEFAULT_WORKER_TIMEOUT);
	fence_free_worker.data = (unsigned long)sobj->dmabuf;
	fence_free_worker.function = fence_free_worker_handler;
	add_timer(&fence_free_worker);
}

static void cancel_fence_free_worker(struct dmabuf_sync_object *sobj)
{
	if (timer_pending(&fence_free_worker)) {
		del_timer(&fence_free_worker);
		dma_buf_put(sobj->dmabuf);
	}
}

/*
 * dmabuf_sync_update - register a fence object to reservation_object.
 *
 * @sobj: An object o dmabuf_sync_object.
 *
 * This function is called by dmabuf_sync_wait and dmabuf_sync_wait_all
 * functions when there is no any thread accessing the same dmabuf or
 * after current thread is waked up, Which means that current thread has
 * ownership of the dmabuf.
 */
static void dmabuf_sync_update(struct dmabuf_sync_object *sobj)
{
	struct seqno_fence *sf = sobj->sfence;
	struct dma_buf *dmabuf = sobj->dmabuf;

	if (sobj->access_type & DMA_BUF_ACCESS_R) {
		struct reservation_object_list *fobj;

		fobj = reservation_object_get_list(dmabuf->resv);

		/*
		 * Reserve spaces for shared fences if this is the first call
		 * or there is no space for a new fence.
		 */
		if (!fobj || (fobj && fobj->shared_count == fobj->shared_max))
			reservation_object_reserve_shared(dmabuf->resv);

		reservation_object_add_shared_fence(dmabuf->resv, &sf->base);

		/*
		 * Add timer handler, which is used to drop a reference
		 * of fence object after sync operation is completed.
		 *
		 * dmabuf_sync_update request with a read access doesn't drop
		 * a reference of old excl_fence. For more details, see
		 * below functions of reservation module,
		 * - reservation_object_add_shared_inplace()
		 * - reservation_object_add_shared_replace()
		 *
		 * P.S. a reference of old excl_fence is dropped by only
		 * reservation_object_add_excl_fence(), which is called
		 * by when dmabuf_sync_upate is called with a write access.
		 * TODO. find out better way.
		 */
		setup_fence_free_worker(sobj);
	} else {
		reservation_object_add_excl_fence(dmabuf->resv, &sf->base);

		/*
		 * In this case, each reference of all old fences, shared and
		 * excl, must be dropped by above function,
		 * reservation_object_add_excl_fence(), so cancel a timer.
		 * TODO. find out better way.
		 */
		cancel_fence_free_worker(sobj);
	}
}

static void remove_obj_from_req_queue(struct dmabuf_sync_object *csobj)
{
	struct dmabuf_sync_object *sobj, *next;
	unsigned long o_flags;

	spin_lock_irqsave(&orders_lock, o_flags);
	if (list_empty(&orders)) {
		/* There should be no such case. */
		WARN_ON(1);
		goto out;
	}

	list_for_each_entry_safe(sobj, next, &orders, g_head) {
		if (sobj == csobj) {
			list_del_init(&sobj->g_head);
			sobj_put(sobj);
			break;
		}

	}
out:
	spin_unlock_irqrestore(&orders_lock, o_flags);
}

static void dmabuf_sync_reference_reservation(struct dma_buf *dmabuf,
						bool reference)
{
	struct reservation_object *obj = dmabuf->resv;
	struct reservation_object_list *fobj;
	struct fence *fence;

	rcu_read_lock();

	fobj = rcu_dereference(obj->fence);
	if (fobj) {
		int i;

		for (i = 0; i < fobj->shared_count; i++) {
			fence = rcu_dereference(fobj->shared[i]);
			if (!fence) {
				WARN_ON(1);
				continue;
			}

			if (reference)
				fence_get_rcu(fence);
			else
				fence_put(fence);
		}
	}

	fence = rcu_dereference(obj->fence_excl);
	if (!fence) {
		rcu_read_unlock();
		return;
	}

	if (reference)
		fence_get_rcu(fence);
	else
		fence_put(fence);

	rcu_read_unlock();
}

/*
 * is_higher_priority_than_current - check if a given sobj was requested
 *				the first.
 *
 * @csobj: An object to dmabuf_sync_object for checking if buffer sync was
 *	requested the first for buffer sync.
 *
 * This function can be called by dmabuf_sync_wait and dmabuf_sync_wait_all
 * to check if there is any object that buffer sync was requested earlier
 * than current thread.
 * If there is a object, the current thread doesn't have the ownership of
 * the dmabuf so will try to check the priority again to yield the ownership
 * to other threads that requested buffer sync earlier than current thread.
 */
static bool is_higher_priority_than_current(struct dma_buf *dmabuf,
					struct dmabuf_sync_object *csobj)
{
	struct dmabuf_sync_object *sobj;
	unsigned long o_flags;
	bool ret = false;

	spin_lock_irqsave(&orders_lock, o_flags);
	if (unlikely(list_empty(&orders))) {
		/* There should be no such case. */
		WARN_ON(1);
		goto out;
	}

	list_for_each_entry(sobj, &orders, g_head) {
		struct seqno_fence *old_sf, *cur_sf;

		old_sf = sobj->sfence;
		cur_sf = csobj->sfence;
		WARN_ON(!old_sf || !cur_sf);

		if (old_sf->base.context != cur_sf->base.context &&
		    sobj != csobj)
			ret = true;
		break;
	}

out:
	spin_unlock_irqrestore(&orders_lock, o_flags);
	return ret;
}

/**
 * dmabuf_sync_wait_all - Wait for the completion of DMA or CPU access to
 *				all dmabufs.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * The caller should call this function prior to CPU or DMA access to
 * dmabufs so that other CPU or DMA device cannot access the dmabufs.
 */
long dmabuf_sync_wait_all(struct dmabuf_sync *sync)
{
	struct dmabuf_sync_object *sobj;
	unsigned long timeout = 0;
	unsigned long s_flags;

	spin_lock_irqsave(&sync->lock, s_flags);
	list_for_each_entry(sobj, &sync->syncs, l_head) {
		struct dma_buf *dmabuf;
		struct seqno_fence *sf;
		unsigned long o_flags;
		bool all_wait;

		spin_unlock_irqrestore(&sync->lock, s_flags);

		/*
		 * orders is used to check if there is any sobj
		 * of other thread that requested earlier buffer sync than
		 * current thread.
		 */
		spin_lock_irqsave(&orders_lock, o_flags);
		list_add_tail(&sobj->g_head, &orders);
		sobj_get(sobj);
		spin_unlock_irqrestore(&orders_lock, o_flags);

		sf = sobj->sfence;
		dmabuf = sobj->dmabuf;
		dmabuf_sync_reference_reservation(dmabuf, true);

		/*
		 * It doesn't need to wait for other thread or threads
		 * if there is no any sync object which has higher priority
		 * than this one, sobj so go update a fence.
		 */
		if (!is_higher_priority_than_current(dmabuf, sobj))
			goto out_enable_signal;

		all_wait = sobj->access_type & DMA_BUF_ACCESS_W;

go_back_to_wait:
		/*
		 * Need to wait for all buffers for a read or a write
		 * if it should access a buffer for a write.
		 * Otherwise, just wait for only the completion of a buffer
		 * access for a write.
		 *
		 * P.S. the references of previous fence objects for a read
		 * or a write will be dropped in this function so these
		 * fence objects could be freed here.
		 */
		timeout = reservation_object_wait_timeout_rcu(dmabuf->resv,
				all_wait, true,
				msecs_to_jiffies(DEFAULT_SYNC_TIMEOUT));
		if (!timeout)
			pr_warning("[DMA] signal wait has been timed out.\n");

		/*
		 * Check if there is any sobj of other thread that requested
		 * earlier than current thread. If other sobj, then it should
		 * yield the ownership of a buffer to other thread for buffer
		 * access ordering so go wait for the thread.
		 */
		if (is_higher_priority_than_current(dmabuf, sobj))
			goto go_back_to_wait;

out_enable_signal:
		fence_enable_sw_signaling(&sf->base);
		dmabuf_sync_update(sobj);
		dmabuf_sync_reference_reservation(dmabuf, false);
		dmabuf_sync_cache_ops(sobj);

		spin_lock_irqsave(&sync->lock, s_flags);
	}
	spin_unlock_irqrestore(&sync->lock, s_flags);

	return timeout;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_wait_all);

/**
 * dmabuf_sync_wait - Wait for the completion of DMA or CPU access to a dmabuf.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * The caller should call this function prior to CPU or DMA access to
 * a dmabuf so that other CPU or DMA device cannot access the dmabuf.
 */
long dmabuf_sync_wait(struct dma_buf *dmabuf, unsigned int ctx,
			unsigned int access_type)
{
	struct dmabuf_sync_object *sobj;
	struct dmabuf_sync *sync;
	unsigned long timeout = 0;
	bool all_wait;
	unsigned long o_flags;

	if (!IS_VALID_DMA_BUF_ACCESS_TYPE(access_type))
		return -EINVAL;

	if ((access_type & DMA_BUF_ACCESS_RW) == DMA_BUF_ACCESS_RW)
		access_type &= ~DMA_BUF_ACCESS_R;

	sync = kzalloc(sizeof(*sync), GFP_KERNEL);
	if (!sync)
		return -ENOMEM;

	sobj = kzalloc(sizeof(*sobj), GFP_KERNEL);
	if (!sobj) {
		kfree(sync);
		return -ENOMEM;
	}

	spin_lock_init(&sync->flock);

	kref_init(&sobj->refcount);
	sobj->access_type = access_type;
	sobj->sfence = &sync->sfence;
	sobj->dmabuf = dmabuf;
	sync->single_sobj = sobj;
	seqno_fence_init(&sync->sfence, &sync->flock, dmabuf, ctx, 0,
				++seqno, 0, &fence_default_ops);

	spin_lock_irqsave(&orders_lock, o_flags);
	list_add_tail(&sobj->g_head, &orders);
	sobj_get(sobj);
	spin_unlock_irqrestore(&orders_lock, o_flags);

	dmabuf_sync_reference_reservation(dmabuf, true);

	/*
	 * It doesn't need to wait for other thread or threads
	 * if there is no any sync object which has higher priority
	 * than this one, sobj so go update a fence.
	 */
	if (!is_higher_priority_than_current(dmabuf, sobj))
		goto out_enable_signal;

	all_wait = access_type & DMA_BUF_ACCESS_W;

go_back_to_wait:
	/*
	 * Need to wait for all buffers for a read or a write
	 * if it should access a buffer for a write.
	 * Otherwise, just wait for only the completion of a buffer
	 * access for a write.
	 *
	 * P.S. the references of previous fence objects for a read
	 * or a write will be dropped in this function so these
	 * fence objects could be freed here.
	 */
	timeout = reservation_object_wait_timeout_rcu(dmabuf->resv,
			all_wait, true,
			msecs_to_jiffies(DEFAULT_SYNC_TIMEOUT));
	if (!timeout)
		pr_warning("[CPU] signal wait has been timed out.\n");

	/*
	 * Check if there is any sobj of other thread that requested
	 * earlier than current thread. If other sobj, then it should
	 * yield the ownership of a buffer to other thread for buffer
	 * access ordering so go wait for the thread.
	 */
	if (is_higher_priority_than_current(dmabuf, sobj))
		goto go_back_to_wait;

out_enable_signal:
	fence_enable_sw_signaling(&sync->sfence.base);
	dmabuf_sync_update(sobj);
	dmabuf_sync_reference_reservation(dmabuf, false);
	dmabuf_sync_cache_ops(sobj);

	return timeout;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_wait);

/**
 * dmabuf_sync_signal_all - Wake up all threads blocked when they tried to
 *				access dmabufs registered to a given
 *				dmabuf_sync object.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * The caller should call this function after CPU or DMA access to
 * the dmabufs is completed so that other CPU and DMA device can access
 * the dmabufs.
 */
int dmabuf_sync_signal_all(struct dmabuf_sync *sync)
{
	struct dmabuf_sync_object *sobj;
	unsigned long s_flags;
	int ret = -EAGAIN;

	rcu_read_lock();

	spin_lock_irqsave(&sync->lock, s_flags);
	list_for_each_entry(sobj, &sync->syncs, l_head) {
		struct fence *fence;
		unsigned long flags;

		spin_unlock_irqrestore(&sync->lock, s_flags);

		fence = &sobj->sfence->base;
		fence = rcu_dereference(fence);

		remove_obj_from_req_queue(sobj);

		spin_lock_irqsave(fence->lock, flags);

		/*
		 * Drop a reference if there is no any task waiting for signal.
		 * if any task then a reference of this fence will be dropped
		 * by that the task calls dmabuf_sync_update() after waked up.
		 */
		if (list_empty(&fence->cb_list)) {
			fence_put(fence);
			/* Cancel fence free worker if no any task. */
			cancel_fence_free_worker(sobj);
		}

		ret = fence_signal_locked(fence);
		if (ret) {
			pr_warning("signal request has been failed.\n");
			spin_unlock_irqrestore(fence->lock, flags);
			break;
		}

		spin_unlock_irqrestore(fence->lock, flags);

		sobj_put(sobj);

		/*
		 * Set sync_but, which is a signaled buffer recently.
		 *
		 * When seqno_release is called, dma_buf_put is called
		 * with seqno_fence->sync_buf.
		 */
		sobj->sfence->sync_buf = sobj->dmabuf;

		spin_lock_irqsave(&sync->lock, s_flags);
	}
	spin_unlock_irqrestore(&sync->lock, s_flags);

	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_signal_all);

static int dmabuf_sync_signal_fence(struct fence *fence)
{
	struct dmabuf_sync *sync;
	struct seqno_fence *sf;
	unsigned long flags;
	int ret;

	sf = to_seqno_fence(fence);
	sync = to_dmabuf_sync(sf);

	remove_obj_from_req_queue(sync->single_sobj);

	rcu_read_unlock();

	spin_lock_irqsave(fence->lock, flags);

	/*
	 * Drop a reference if there is no any task waiting for signal.
	 * if any task then a reference of this fence will be dropped
	 * by that the task calls dmabuf_sync_update() after waked up.
	 */
	if (list_empty(&fence->cb_list)) {
		fence_put(fence);
		/* Cancel fence free worker if no any task. */
		cancel_fence_free_worker(sync->single_sobj);
	}

	ret = fence_signal_locked(fence);
	if (ret)
		pr_warning("signal request has been failed.\n");

	spin_unlock_irqrestore(fence->lock, flags);

	sobj_put(sync->single_sobj);

	rcu_read_lock();

	return ret;
}

/**
 * dmabuf_sync_signal - Wake up all threads blocked when they tried to access
 *			a given dmabuf.
 *
 * @dmabuf: An object to dma_buf structure.
 *
 * The caller should call this function after CPU or DMA access to the dmabuf
 * is completed so that other CPU and DMA device can access the dmabuf.
 */
int dmabuf_sync_signal(struct dma_buf *dmabuf)
{
	struct reservation_object_list *rol;
	struct reservation_object *ro;
	struct fence *fence;
	int i;

	rcu_read_lock();

	ro = rcu_dereference(dmabuf->resv);

	/* Was it a fence for a write? */
	fence = rcu_dereference(ro->fence_excl);
	if (fence) {
		if (fence->context == (unsigned int)current) {
			/*
			 * If this fence is already signaled,
			 * this context has its own shared fence
			 * so go check the shared fence.
			 */
			if (fence_is_signaled(fence)) {
				fence = NULL;
				goto out_to_shared;
			}
			dmabuf_sync_signal_fence(fence);
			goto out;
		} else
			fence = NULL;
	}

out_to_shared:
	rol = rcu_dereference(ro->fence);
	if (!rol)
		goto out;

	/* Was it a fence for a read? */
	for (i = 0; i < rol->shared_count; i++) {
		fence = rcu_dereference(rol->shared[i]);
		if (!fence)
			continue;

		if (fence && fence->context != (unsigned int)current) {
			fence = NULL;
			continue;
		}

		/* There should be no such case. */
		if (fence_is_signaled(fence))
			WARN_ON(1);

		break;
	}

	if (fence)
		dmabuf_sync_signal_fence(fence);
	else
		WARN_ON(1);

out:
	rcu_read_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(dmabuf_sync_signal);

/**
 * dmabuf_sync_fini - Release a given dmabuf_sync object and relevant to it.
 *
 * @sync: An object to dmabuf_sync structure.
 *
 * This function should be called if some operation failed after
 * dmabuf_sync_init call to release relevant resources, and after
 * dmabuf_sync_signal or dmabuf_sync_signal_all function is called.
 */
void dmabuf_sync_fini(struct dmabuf_sync *sync)
{
	unsigned long s_flags;

	if (WARN_ON(!sync))
		return;

	spin_lock_irqsave(&sync->lock, s_flags);
	if (list_empty(&sync->syncs))
		goto free_sync;

	/*
	 * It considers a case that a caller never call dmabuf_sync_put_all().
	 */
	dmabuf_sync_put_all(sync);

free_sync:
	spin_unlock_irqrestore(&sync->lock, s_flags);

	if (sync->ops && sync->ops->free)
		sync->ops->free(sync->priv);
}
EXPORT_SYMBOL_GPL(dmabuf_sync_fini);
