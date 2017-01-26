/* include/linux/pid_stat.h
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PID_STAT_H
#define __PID_STAT_H

/* Contains definitions for tcp resource tracking per pid. */

#ifdef CONFIG_PID_STAT
int pid_stat_tcp_snd(pid_t pid, int size);
int pid_stat_tcp_rcv(pid_t pid, int size);
#else
#define pid_stat_tcp_snd(pid, size) do {} while (0);
#define pid_stat_tcp_rcv(pid, size) do {} while (0);
#endif

#endif /* __SAP_PID_STAT_H */
