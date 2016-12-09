/*
 * Copyright (C) 2013-2015 Kay Sievers
 * Copyright (C) 2013-2015 Greg Kroah-Hartman <gregkh@linuxfoundation.org>
 * Copyright (C) 2013-2015 Daniel Mack <daniel@zonque.org>
 * Copyright (C) 2013-2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright (C) 2013-2015 Linux Foundation
 * Copyright (C) 2014-2015 Djalal Harouni
 *
 * kdbus is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 */

#ifndef __KDBUS_METADATA_H
#define __KDBUS_METADATA_H

#include <linux/kernel.h>
#include <linux/kref.h>
#include <linux/path.h>
#include <linux/sched.h>

struct kdbus_conn;
struct kdbus_pool_slice;

struct kdbus_meta_conn;

/**
 * struct kdbus_meta_proc - Process metadata
 * @kref:		Reference counting
 * @lock:		Object lock
 * @collected:		Bitmask of collected items
 * @valid:		Bitmask of collected and valid items
 * @cred:		Credentials
 * @pid:		PID of process
 * @tgid:		TGID of process
 * @ppid:		PPID of process
 * @tid_comm:		TID comm line
 * @pid_comm:		PID comm line
 * @exe_path:		Executable path
 * @root_path:		Root-FS path
 * @cmdline:		Command-line
 * @cgroup:		Full cgroup path
 * @seclabel:		Seclabel
 * @audit_loginuid:	Audit login-UID
 * @audit_sessionid:	Audit session-ID
 */
struct kdbus_meta_proc {
	struct kref kref;
	struct mutex lock;
	u64 collected;
	u64 valid;

	/* KDBUS_ITEM_CREDS */
	/* KDBUS_ITEM_AUXGROUPS */
	/* KDBUS_ITEM_CAPS */
	const struct cred *cred;

	/* KDBUS_ITEM_PIDS */
	struct pid *pid;
	struct pid *tgid;
	struct pid *ppid;

	/* KDBUS_ITEM_TID_COMM */
	char tid_comm[TASK_COMM_LEN];
	/* KDBUS_ITEM_PID_COMM */
	char pid_comm[TASK_COMM_LEN];

	/* KDBUS_ITEM_EXE */
	struct path exe_path;
	struct path root_path;

	/* KDBUS_ITEM_CMDLINE */
	char *cmdline;

	/* KDBUS_ITEM_CGROUP */
	char *cgroup;

	/* KDBUS_ITEM_SECLABEL */
	char *seclabel;

	/* KDBUS_ITEM_AUDIT */
	kuid_t audit_loginuid;
	unsigned int audit_sessionid;
};


/**
 * struct kdbus_meta_fake - Fake metadata
 * @valid:		Bitmask of collected and valid items
 * @uid:		UID of process
 * @euid:		EUID of process
 * @suid:		SUID of process
 * @fsuid:		FSUID of process
 * @gid:		GID of process
 * @egid:		EGID of process
 * @sgid:		SGID of process
 * @fsgid:		FSGID of process
 * @pid:		PID of process
 * @tgid:		TGID of process
 * @ppid:		PPID of process
 * @seclabel:		Seclabel
 */
struct kdbus_meta_fake {
	u64 valid;

	/* KDBUS_ITEM_CREDS */
	kuid_t uid, euid, suid, fsuid;
	kgid_t gid, egid, sgid, fsgid;

	/* KDBUS_ITEM_PIDS */
	struct pid *pid, *tgid, *ppid;

	/* KDBUS_ITEM_SECLABEL */
	char *seclabel;
};

struct kdbus_meta_proc *kdbus_meta_proc_new(void);
struct kdbus_meta_proc *kdbus_meta_proc_ref(struct kdbus_meta_proc *mp);
struct kdbus_meta_proc *kdbus_meta_proc_unref(struct kdbus_meta_proc *mp);
void kdbus_meta_proc_free(struct kref *mpkref);
int kdbus_meta_proc_collect(struct kdbus_meta_proc *mp, u64 what);

struct kdbus_meta_fake *kdbus_meta_fake_new(void);
struct kdbus_meta_fake *kdbus_meta_fake_free(struct kdbus_meta_fake *mf);
int kdbus_meta_fake_collect(struct kdbus_meta_fake *mf,
			    const struct kdbus_creds *creds,
			    const struct kdbus_pids *pids,
			    const char *seclabel);

struct kdbus_meta_conn *kdbus_meta_conn_new(void);
struct kdbus_meta_conn *kdbus_meta_conn_ref(struct kdbus_meta_conn *mc);
struct kdbus_meta_conn *kdbus_meta_conn_unref(struct kdbus_meta_conn *mc);
int kdbus_meta_conn_collect(struct kdbus_meta_conn *mc,
			    struct kdbus_conn *conn,
			    u64 msg_seqnum, u64 what);

int kdbus_meta_emit(struct kdbus_meta_proc *mp,
		    struct kdbus_meta_fake *mf,
		    struct kdbus_meta_conn *mc,
		    struct kdbus_conn *conn,
		    u64 mask,
		    struct kdbus_item **out_items,
		    size_t *out_size);
u64 kdbus_meta_info_mask(const struct kdbus_conn *conn, u64 mask);
u64 kdbus_meta_msg_mask(const struct kdbus_conn *snd,
			const struct kdbus_conn *rcv);

#endif
