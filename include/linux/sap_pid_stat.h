/* include/linux/sap_pid_stat.h
 *
 * Copyright (C) 2015 SAMSUNG, Inc.
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
 *******************************************************************************
 *                                  HISTORY                                    *
 *******************************************************************************
 * ver   who                                         what                      *
 * ---- -------------------------------------------- ------------------------- *
 * 1.0   Junho Jang <vincent.jang@samsung.com>       <2015>                    *
 *                                                   Initial Release           *
 * ---- -------------------------------------------- ------------------------- *
 * 1.1   Hunsup Jung <hunsup.jung@samsung.com>       <2017.08.01>              *
 *                                                   Remove unnecessary code   *
 * ---- -------------------------------------------- ------------------------- *
 * 1.2   Junho Jang <vincent.jang@samsung.com>       <2018>                    *
 *                                                   refactoring           *
 * ---- -------------------------------------------- ------------------------- *
 */

#ifndef __SAP_PID_STAT_H
#define __SAP_PID_STAT_H

#define ASP_ID_LEN 15
typedef struct {
	char id[ASP_ID_LEN + 1];
} aspid_t;

#define SAP_STAT_SAPID_MAX 32
struct sap_pid_wakeup_stat {
	int wakeup_cnt[SAP_STAT_SAPID_MAX];
	int activity_cnt[SAP_STAT_SAPID_MAX];
};

struct sap_stat_traffic {
	aspid_t aspid;
	unsigned int snd;
	unsigned int rcv;
	unsigned int snd_count;
	unsigned int rcv_count;
};

#ifdef CONFIG_SAP_PID_STAT
int sap_stat_get_wakeup(struct sap_pid_wakeup_stat *sap_wakeup);
void sap_stat_get_traffic(int type,
	struct sap_stat_traffic *sap_traffic, size_t n);
int sap_stat_is_noti_wakeup(void);
#else
static inline int sap_stat_get_wakeup(
	struct sap_pid_wakeup_stat *sap_wakeup) { return 0; }
static inline void sap_stat_get_traffic(int type,
	struct sap_stat_traffic *sap_traffic, size_t n) { return; }
static inline int sap_stat_is_noti_wakeup(void) { return 0; }
#endif

#endif /* __SAP_PID_STAT_H */
