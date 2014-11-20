/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Chango Park <chanho61.park@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/dma-buf.h>
#include <linux/seqno-fence.h>

#define DMABUF_SYNC_NAME_SIZE	256
#define	DMA_BUF_ACCESS_R	0x1
#define DMA_BUF_ACCESS_W	0x2
#define DMA_BUF_ACCESS_DMA	0x4
#define DMA_BUF_ACCESS_RW	(DMA_BUF_ACCESS_R | DMA_BUF_ACCESS_W)
#define DMA_BUF_ACCESS_DMA_R	(DMA_BUF_ACCESS_R | DMA_BUF_ACCESS_DMA)
#define DMA_BUF_ACCESS_DMA_W	(DMA_BUF_ACCESS_W | DMA_BUF_ACCESS_DMA)
#define DMA_BUF_ACCESS_DMA_RW	(DMA_BUF_ACCESS_DMA_R | DMA_BUF_ACCESS_DMA_W)
#define IS_VALID_DMA_BUF_ACCESS_TYPE(t)	(t == DMA_BUF_ACCESS_R || \
					 t == DMA_BUF_ACCESS_W || \
					 t == DMA_BUF_ACCESS_DMA_R || \
					 t == DMA_BUF_ACCESS_DMA_W || \
					 t == DMA_BUF_ACCESS_RW || \
					 t == DMA_BUF_ACCESS_DMA_RW)

/*
 * A structure for dmabuf_sync_object.
 *
 * @head: A list head to be added to dmabuf_sync's syncs.
 * @access_type: Indicate how a current task tries to access
 *	a given buffer, and one of the below types could be set.
 *	DMA_BUF_ACCESS_R -> CPU access for read.
 *	DMA_BUF_ACCRSS_W -> CPU access for write.
 *	DMA_BUF_ACCESS_R | DMA_BUF_ACCESS_DMA -> DMA access for read.
 *	DMA_BUF_ACCESS_W | DMA_BUF_ACCESS_DMA -> DMA access for write.
 */
struct dmabuf_sync_object {
	struct kref			refcount;
	struct list_head		l_head;
	struct list_head		g_head;
	struct seqno_fence		*sfence;
	struct dma_buf			*dmabuf;
	unsigned int			access_type;
};

struct dmabuf_sync_priv_ops {
	void (*free)(void *priv);
};

/*
 * A structure for dmabuf_sync.
 *
 * @syncs: A list head to sync object and this is global to system.
 *	This contains sync objects of dmabuf_sync owner.
 * @priv: A private data.
 * @name: A string to dmabuf sync owner.
 */
struct dmabuf_sync {
	struct list_head		list;
	struct list_head		syncs;
	struct seqno_fence		sfence;
	unsigned int			obj_cnt;
	struct dmabuf_sync_priv_ops	*ops;
	char				name[DMABUF_SYNC_NAME_SIZE];
	struct dmabuf_sync_object	*single_sobj;
	struct timer_list		sync_free_worker;
	spinlock_t			lock;
	spinlock_t			flock;
	void				*priv;
};

bool dmabuf_sync_is_supported(void);

struct dmabuf_sync *dmabuf_sync_init(const char *name,
					struct dmabuf_sync_priv_ops *ops,
					void *priv);

void dmabuf_sync_fini(struct dmabuf_sync *sync);

int dmabuf_sync_get(struct dmabuf_sync *sync, void *sync_buf,
				unsigned int ctx, unsigned int type);

void dmabuf_sync_put(struct dmabuf_sync *sync, struct dma_buf *dmabuf);

void dmabuf_sync_put_all(struct dmabuf_sync *sync);

long dmabuf_sync_wait(struct dma_buf *dmabuf, unsigned int ctx,
			unsigned int access_type);

long dmabuf_sync_wait_all(struct dmabuf_sync *sync);

int dmabuf_sync_signal(struct dma_buf *dmabuf);

int dmabuf_sync_signal_all(struct dmabuf_sync *sync);

static inline struct dmabuf_sync *to_dmabuf_sync(struct seqno_fence *sf)
{
	if (!sf)
		return NULL;

	return container_of(sf, struct dmabuf_sync, sfence);
}
