/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Samsung Electronics, 2016
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/slab.h>
#include <linux/atomic.h>
#include <kprobe/swap_kprobes_deps.h>
#include "wsp_res.h"

static atomic_t wsp_res_count;

static int wsp_tdata_get_id(void)
{
	return atomic_inc_return(&wsp_res_count);
}

static struct wsp_tdata *g_tdata;

/* FIXME: get_tdata() need receive for each processes */
static struct wsp_tdata *get_tdata(void)
{
	return g_tdata;
}





/* ============================================================================
 * =                                  wsp_tdata                               =
 * ============================================================================
 */
struct wsp_tdata {
	struct list_head res_list;
	spinlock_t lock;

	enum tdata_stat stat;
};

static struct wsp_res *wsp_tdata_new_res(struct wsp_tdata *d, void *ptr,
					 enum wsp_res_t type)
{
	struct wsp_res *res;

	res = kmalloc(sizeof(*res), GFP_ATOMIC);
	if (res) {
		INIT_LIST_HEAD(&res->list);
		res->ptr = ptr;
		res->id = wsp_tdata_get_id();
		res->type = type;
		res->stat = WRS_NEW;

		/* add to list */
		spin_lock(&d->lock);
		list_add(&res->list, &d->res_list);
		spin_unlock(&d->lock);
	}

	return res;
}

static void wsp_tdata_del_res(struct wsp_res *res)
{
	list_del(&res->list);
	kfree(res);
}

static struct wsp_tdata *wsp_tdata_create(void)
{
	struct wsp_tdata *d;

	d = kmalloc(sizeof(*d), GFP_ATOMIC);
	if (d) {
		INIT_LIST_HEAD(&d->res_list);
		spin_lock_init(&d->lock);
		d->stat = TDS_NEW;
	}

	return d;
}

static void wsp_tdata_destroy(struct wsp_tdata *d)
{
	struct wsp_res *res, *n;

	spin_lock(&d->lock);
	list_for_each_entry_safe(res, n, &d->res_list, list)
		wsp_tdata_del_res(res);
	spin_unlock(&d->lock);

	kfree(d);
}

static struct wsp_res *wsp_tdata_last_res(struct wsp_tdata *d)
{
	struct wsp_res *res;

	spin_lock(&d->lock);
	res = list_first_entry_or_null(&d->res_list, struct wsp_res, list);
	spin_unlock(&d->lock);

	return res;
}

struct wsp_res *wsp_tdata_find_res(struct wsp_tdata *data, void *ptr,
				   enum wsp_res_t type)
{
	struct wsp_res *res;

	list_for_each_entry(res, &data->res_list, list) {
		if (res->type != type)
			continue;

		if (res->ptr == ptr) {
			if (res->stat == WRS_ERR) {
				pr_err("ERR: something went wrong\n");
				return NULL;
			}

			return res;
		}
	}

	return NULL;
}





/* ============================================================================
 * =                          wsp_current_[get/set]_stat()                    =
 * ============================================================================
 */
enum tdata_stat wsp_current_get_stat(void)
{
	struct wsp_tdata *d;

	d = get_tdata();
	if (d)
		return d->stat;
	else
		pr_err("ERR: no current tdata\n");

	return TDS_ERR;
}

void wsp_current_set_stat(enum tdata_stat stat)
{
	struct wsp_tdata *d;

	d = get_tdata();
	if (d)
		d->stat = stat;
	else
		pr_err("ERR: no current tdata\n");
}





/* ============================================================================
 * =                                   wsp_res                                =
 * ============================================================================
 */
struct wsp_res *wsp_res_new(void *ptr, enum wsp_res_t type)
{
	struct wsp_tdata *data;

	data = get_tdata();
	if (data == NULL) {
		pr_err("ERR: no current tdata\n");
		return NULL;
	}


	return wsp_tdata_new_res(data, ptr, type);
}

void wsp_res_del(struct wsp_res *res)
{
	wsp_tdata_del_res(res);
}

struct wsp_res *wsp_res_find(void *ptr, enum wsp_res_t type)
{
	struct wsp_tdata *data;

	data = get_tdata();
	if (data == NULL) {
		pr_err("ERR: no current tdata\n");
		return NULL;
	}

	return wsp_tdata_find_res(data, ptr, type);
}

struct wsp_res *wsp_res_last(void)
{
	struct wsp_tdata *d;

	d = get_tdata();
	if (d == NULL) {
		pr_err("ERR: no current tdata\n");
		return NULL;
	}

	return wsp_tdata_last_res(d);
}

int wsp_res_stat_set_next(struct wsp_res *res, enum wsp_res_stat stat)
{
	switch (res->stat) {
	case WRS_NEW:
		if (stat == WRS_WILL_REQ) {
			res->stat = stat;
			return 0;
		}
		break;

	case WRS_WILL_REQ:
		if (stat == WRS_SOUP_REQ) {
			res->stat = stat;
			return 0;
		}
		break;

	case WRS_SOUP_REQ:
	case WRS_ADD_DATA:
		if (stat == WRS_ADD_DATA || stat == WRS_FINISH) {
			res->stat = stat;
			return 0;
		}
		break;

	default:
		break;
	}

	pr_err("ERR: set_next_stat from %d to %d [id=%d]\n",
	       res->stat, stat, res->id);

	res->stat = WRS_ERR;

	return -1;
}





/* ============================================================================
 * =                                init/exit()                               =
 * ============================================================================
 */
int wsp_res_init(void)
{
	g_tdata = wsp_tdata_create();
	if (g_tdata == NULL)
		return -ENOMEM;

	atomic_set(&wsp_res_count, 0);

	return 0;
}

void wsp_res_exit(void)
{
	wsp_tdata_destroy(g_tdata);
	g_tdata = NULL;
}
