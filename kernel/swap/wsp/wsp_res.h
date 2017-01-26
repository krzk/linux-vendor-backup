#ifndef _WSP_TDATA_H
#define _WSP_TDATA_H

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
 * Copyright (C) Samsung Electronics, 2015
 *
 * 2015         Vyacheslav Cherkashin <v.cherkashin@samsung.com>
 *
 */


#include <linux/types.h>


enum tdata_stat {
	TDS_ERR,
	TDS_NEW,
	TDS_FINISH_MAIN_LOAD,
	TDS_DRAW
};

enum wsp_res_t {
	WR_NONE,
	WR_MAIN,
	WR_ANY
};

enum wsp_res_stat {
	WRS_NEW,
	WRS_WILL_REQ,
	WRS_SOUP_REQ,
	WRS_ADD_DATA,
	WRS_FINISH,
	WRS_ERR
};

struct wsp_res {
	struct list_head list;
	void *ptr;
	int id;
	enum wsp_res_t type;
	enum wsp_res_stat stat;
};


enum tdata_stat wsp_current_get_stat(void);
void wsp_current_set_stat(enum tdata_stat stat);

struct wsp_res *wsp_res_new(void *ptr, enum wsp_res_t type);
void wsp_res_del(struct wsp_res *res);

struct wsp_res *wsp_res_last(void);
struct wsp_res *wsp_res_find(void *ptr, enum wsp_res_t type);

int wsp_res_stat_set_next(struct wsp_res *res, enum wsp_res_stat stat);

int wsp_res_init(void);
void wsp_res_exit(void);


#endif /* _WSP_TDATA_H */
