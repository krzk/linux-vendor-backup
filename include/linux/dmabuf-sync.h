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

enum dmabuf_sync_status {
	DMABUF_SYNC_GOT		= 1,
	DMABUF_SYNC_LOCKED,
};

struct dmabuf_sync_reservation {
	struct ww_mutex		sync_lock;
	struct mutex		lock;
	atomic_t		shared_cnt;
	unsigned int		accessed_type;
	unsigned int		locked;
};

/*
 * A structure for dmabuf_sync_object.
 *
 * @head: A list head to be added to syncs list.
 * @robj: A reservation_object object.
 * @dma_buf: A dma_buf object.
 * @access_type: Indicate how a current task tries to access
 *	a given buffer.
 */
struct dmabuf_sync_object {
	struct list_head		head;
	struct dmabuf_sync_reservation	*robj;
	struct dma_buf			*dmabuf;
	unsigned int			access_type;
};

/*
 * A structure for dmabuf_sync.
 *
 * @syncs: A list head to sync object and this is global to system.
 * @list: A list entry used as committed list node
 * @lock: A mutex lock to current sync object.
 * @ctx: A current context for ww mutex.
 * @work: A work struct to release resources at timeout.
 * @priv: A private data.
 * @name: A string to dmabuf sync owner.
 * @timer: A timer list to avoid lockup and release resources.
 * @status: Indicate current status (DMABUF_SYNC_GOT or DMABUF_SYNC_LOCKED).
 */
struct dmabuf_sync {
	struct list_head	syncs;
	struct list_head	list;
	struct mutex		lock;
	struct ww_acquire_ctx	ctx;
	struct work_struct	work;
	void			*priv;
	char			name[64];
	struct timer_list	timer;
	unsigned int		status;
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

extern bool is_dmabuf_sync_supported(void);

extern struct dmabuf_sync *dmabuf_sync_init(void *priv, const char *name);

extern void dmabuf_sync_fini(struct dmabuf_sync *sync);

extern int dmabuf_sync_lock(struct dmabuf_sync *sync);

extern int dmabuf_sync_unlock(struct dmabuf_sync *sync);

int dmabuf_sync_single_lock(struct dma_buf *dmabuf, unsigned int type,
				bool wait);

void dmabuf_sync_single_unlock(struct dma_buf *dmabuf);

extern int dmabuf_sync_get(struct dmabuf_sync *sync, void *sync_buf,
				unsigned int type);

extern void dmabuf_sync_put(struct dmabuf_sync *sync, struct dma_buf *dmabuf);

extern void dmabuf_sync_put_all(struct dmabuf_sync *sync);

#else

static inline void dmabuf_sync_reservation_init(struct dma_buf *dmabuf) { }

static inline void dmabuf_sync_reservation_fini(struct dma_buf *dmabuf) { }

static inline bool is_dmabuf_sync_supported(void) { return false; }

static inline struct dmabuf_sync *dmabuf_sync_init(void *priv,
					const char *names)
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
