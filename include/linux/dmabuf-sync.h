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

#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>

#define DMABUF_SYNC_NAME_SIZE	64

/*
 * Status to a dmabuf_sync object.
 *
 * @DMABUF_SYNC_GOT: Indicate that one more dmabuf objects have been added
 *			to a sync's list.
 * @DMABUF_SYNC_LOCKED: Indicate that all dmabuf objects in a sync's list
 *			have been locked.
 */
enum dmabuf_sync_status {
	DMABUF_SYNC_GOT		= 1,
	DMABUF_SYNC_LOCKED,
};

/*
 * A structure for dmabuf_sync_reservation.
 *
 * @syncs: A list head to sync object and this is global to system.
 *	This contains sync objects of tasks that requested a lock
 *	to this dmabuf.
 * @sync_lock: This provides read or write lock to a dmabuf.
 *	Except in the below cases, a task will be blocked if the task
 *	tries to lock a dmabuf for CPU or DMA access when other task
 *	already locked the dmabuf.
 *
 *	Before		After
 *	--------------------------
 *	CPU read	CPU read
 *	CPU read	DMA read
 *	DMA read	CPU read
 *	DMA read	DMA read
 *
 * @lock: Protecting a dmabuf_sync_reservation object.
 * @poll_wait: A wait queue object to poll a dmabuf object.
 * @poll_event: Indicate whether a dmabuf object - being polled -
 *	was unlocked or not. If true, a blocked task will be out
 *	of select system call.
 * @poll: Indicate whether the polling to a dmabuf object was requested
 *	or not by userspace.
 * @shared_cnt: Shared count to a dmabuf object.
 * @accessed_type: Indicate how and who a dmabuf object was accessed by.
 *	One of the below types could be set.
 *	DMA_BUF_ACCESS_R -> CPU access for read.
 *	DMA_BUF_ACCRSS_W -> CPU access for write.
 *	DMA_BUF_ACCESS_R | DMA_BUF_ACCESS_DMA -> DMA access for read.
 *	DMA_BUF_ACCESS_W | DMA_BUF_ACCESS_DMA -> DMA access for write.
 * @locked: Indicate whether a dmabuf object has been locked or not.
 *
 */
struct dmabuf_sync_reservation {
	struct list_head	syncs;
	struct ww_mutex		sync_lock;
	struct mutex		lock;
	wait_queue_head_t	poll_wait;
	unsigned int		poll_event;
	unsigned int		polled;
	atomic_t		shared_cnt;
	unsigned int		accessed_type;
	unsigned int		locked;
};

/*
 * A structure for dmabuf_sync_object.
 *
 * @head: A list head to be added to dmabuf_sync's syncs.
 * @r_head: A list head to be added to dmabuf_sync_reservation's syncs.
 * @robj: A reservation_object object.
 * @dma_buf: A dma_buf object.
 * @task: An address value to current task.
 *	This is used to indicate who is a owner of a sync object.
 * @wq: A wait queue head.
 *	This is used to guarantee that a task can take a lock to a dmabuf
 *	if the task requested a lock to the dmabuf prior to other task.
 *	For more details, see dmabuf_sync_wait_prev_objs function.
 * @refcnt: A reference count to a sync object.
 * @access_type: Indicate how a current task tries to access
 *	a given buffer, and one of the below types could be set.
 *	DMA_BUF_ACCESS_R -> CPU access for read.
 *	DMA_BUF_ACCRSS_W -> CPU access for write.
 *	DMA_BUF_ACCESS_R | DMA_BUF_ACCESS_DMA -> DMA access for read.
 *	DMA_BUF_ACCESS_W | DMA_BUF_ACCESS_DMA -> DMA access for write.
 * @waiting: Indicate whether current task is waiting for the wake up event
 *	from other task or not.
 */
struct dmabuf_sync_object {
	struct list_head		head;
	struct list_head		r_head;
	struct dmabuf_sync_reservation	*robj;
	struct dma_buf			*dmabuf;
	unsigned long			task;
	wait_queue_head_t		wq;
	atomic_t			refcnt;
	unsigned int			access_type;
	unsigned int			waiting;
};

struct dmabuf_sync_priv_ops {
	void (*free)(void *priv);
};

/*
 * A structure for dmabuf_sync.
 *
 * @syncs: A list head to sync object and this is global to system.
 *	This contains sync objects of dmabuf_sync owner.
 * @list: A list entry used as committed list node
 * @lock: Protecting a dmabuf_sync object.
 * @ctx: A current context for ww mutex.
 * @work: A work struct to release resources at timeout.
 * @priv: A private data.
 * @name: A string to dmabuf sync owner.
 * @timer: A timer list to avoid lockup and release resources.
 * @status: Indicate current status (DMABUF_SYNC_GOT or DMABUF_SYNC_LOCKED).
 */
struct dmabuf_sync {
	struct list_head		syncs;
	struct list_head		list;
	struct mutex			lock;
	struct ww_acquire_ctx		ctx;
	struct work_struct		work;
	void				*priv;
	struct dmabuf_sync_priv_ops	*ops;
	char				name[DMABUF_SYNC_NAME_SIZE];
	struct timer_list		timer;
	unsigned int			status;
};

#ifdef CONFIG_DMABUF_SYNC

extern struct ww_class dmabuf_sync_ww_class;

static inline void dmabuf_sync_reservation_init(struct dma_buf *dmabuf)
{
	struct dmabuf_sync_reservation *obj;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj)
		return;

	dmabuf->sync = obj;

	ww_mutex_init(&obj->sync_lock, &dmabuf_sync_ww_class);

	mutex_init(&obj->lock);
	atomic_set(&obj->shared_cnt, 1);
	INIT_LIST_HEAD(&obj->syncs);

	init_waitqueue_head(&obj->poll_wait);
}

static inline void dmabuf_sync_reservation_fini(struct dma_buf *dmabuf)
{
	struct dmabuf_sync_reservation *obj;

	if (!dmabuf->sync)
		return;

	obj = dmabuf->sync;

	ww_mutex_destroy(&obj->sync_lock);

	kfree(obj);
}

bool dmabuf_sync_is_supported(void);

struct dmabuf_sync *dmabuf_sync_init(const char *name,
					struct dmabuf_sync_priv_ops *ops,
					void *priv);

void dmabuf_sync_fini(struct dmabuf_sync *sync);

int dmabuf_sync_lock(struct dmabuf_sync *sync);

int dmabuf_sync_unlock(struct dmabuf_sync *sync);

int dmabuf_sync_single_lock(struct dma_buf *dmabuf, unsigned int type,
				bool wait);

void dmabuf_sync_single_unlock(struct dma_buf *dmabuf);

int dmabuf_sync_get(struct dmabuf_sync *sync, void *sync_buf,
				unsigned int type);

void dmabuf_sync_put(struct dmabuf_sync *sync, struct dma_buf *dmabuf);

void dmabuf_sync_put_all(struct dmabuf_sync *sync);

#else

static inline void dmabuf_sync_reservation_init(struct dma_buf *dmabuf) { }

static inline void dmabuf_sync_reservation_fini(struct dma_buf *dmabuf) { }

static inline bool dmabuf_sync_is_supported(void) { return false; }

static inline  struct dmabuf_sync *dmabuf_sync_init(const char *name,
					struct dmabuf_sync_priv_ops *ops,
					void *priv)
{
	return ERR_PTR(0);
}

static inline void dmabuf_sync_fini(struct dmabuf_sync *sync) { }

static inline int dmabuf_sync_lock(struct dmabuf_sync *sync)
{
	return 0;
}

static inline int dmabuf_sync_unlock(struct dmabuf_sync *sync)
{
	return 0;
}

static inline int dmabuf_sync_single_lock(struct dma_buf *dmabuf,
						unsigned int type,
						bool wait)
{
	return 0;
}

static inline void dmabuf_sync_single_unlock(struct dma_buf *dmabuf)
{
	return;
}

static inline int dmabuf_sync_get(struct dmabuf_sync *sync,
					void *sync_buf,
					unsigned int type)
{
	return 0;
}

static inline void dmabuf_sync_put(struct dmabuf_sync *sync,
					struct dma_buf *dmabuf) { }

static inline void dmabuf_sync_put_all(struct dmabuf_sync *sync) { }

#endif
