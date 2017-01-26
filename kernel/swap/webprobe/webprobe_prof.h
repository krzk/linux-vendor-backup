#ifndef _WEBPROBE_PROF_H
#define _WEBPROBE_PROF_H

/**
 * @file webprobe/webprobe_prof.h
 * @author Anastasia Lyupa <a.lyupa@samsung.com>
 *
 * @section LICENSE
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
 * @section COPYRIGHT
 * Copyright (C) Samsung Electronics, 2015
 *
 * @section DESCRIPTION
 * Profiling for webprobe
 */


enum web_prof_addr_t {
	INSPSERVER_START = 1,
	WILL_EXECUTE = 2,
	DID_EXECUTE = 3
};

enum web_prof_state_t {
	PROF_OFF,
	PROF_ON
};

int web_prof_init(void);
void web_prof_exit(void);
int web_prof_enable(void);
int web_prof_disable(void);
enum web_prof_state_t web_prof_enabled(void);
int web_func_inst_remove(enum web_prof_addr_t type);
u64 *web_prof_addr_ptr(enum web_prof_addr_t type);
unsigned long web_prof_addr(enum web_prof_addr_t type);
int web_prof_data_set(char *app_path, char *app_id);
struct dentry *web_prof_lib_dentry(void);

#endif /* _WEBPROBE_PROF_H */
