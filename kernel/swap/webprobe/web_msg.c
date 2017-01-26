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
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>
#include <writer/swap_msg.h>
#include <writer/event_filter.h>
#include <swap-asm/swap_uprobes.h>


#define WEB_PREFIX      KERN_INFO "[WEB_PROF] "

/* TODO: develop method for obtaining this data during build... */
/* location: webkit2-efl-123997_0.11.113/Source/WTF/wtf/text/StringImpl.h:70 */
struct MStringImpl {
	unsigned m_refCount;
	unsigned m_length;
	union {
		const unsigned char *m_data8;
		const unsigned short *m_data16;
	};
	union {
		void *m_buffer;
		struct MStringImpl *m_substringBuffer;
		unsigned short *m_copyData16;
	};
	unsigned m_hashAndFlags;
};

/* location: webkit2-efl-123997_0.11.113/Source/JavaScriptCore/profiler/
 * CallIdentifier.h:36
 */
struct MCallIdentifier {
	struct MStringImpl *m_name;
	struct MStringImpl *m_url;
	unsigned m_lineNumber;
};

enum {
	OFFSET_NAME = offsetof(struct MCallIdentifier, m_name),
	OFFSET_URL = offsetof(struct MCallIdentifier, m_url),
	OFFSET_LNUM = offsetof(struct MCallIdentifier, m_lineNumber)
};


static int pack_web_string(void *data, size_t size,
			   struct MStringImpl __user *str_imp)
{
	int ret;
	char __user *str;
	unsigned len;
	char __user **pstr;
	unsigned __user *plen;

	pstr = (void __user *)str_imp + offsetof(struct MStringImpl, m_data8);
	plen = (void __user *)str_imp + offsetof(struct MStringImpl, m_length);

	if (get_user(str, pstr) ||
	    get_user(len, plen)) {
		printk(WEB_PREFIX "%s: cannot read user memory\n", __func__);
		return -EPERM;
	}

	if (size < len + 1) {
		printk(WEB_PREFIX "function name is very long(len=%u)\n", len);
		return -ENOMEM;
	}

	ret = strncpy_from_user(data, str, len);
	if (ret < 0) {
		printk(WEB_PREFIX "%s: cannot read user memory\n", __func__);
		return ret;
	}

	((char *)data)[ret] = '\0';

	return ret + 1;
}


void web_msg_entry(struct pt_regs *regs)
{
	int ret;
	struct swap_msg *m;
	void *p;
	long t_name, t_url;
	unsigned lnum;
	size_t pack_size = 0, size;
	struct MCallIdentifier *call_id;
	struct task_struct *task = current;

	if (!check_event(task))
		return;

	call_id = (void *)swap_get_uarg(regs, 2);
	if (get_user(t_name, (long *)((long)call_id + OFFSET_NAME)) ||
	    get_user(t_url, (long *)((long)call_id + OFFSET_URL)) ||
	    get_user(lnum, (unsigned *)((long)call_id + OFFSET_LNUM))) {
		printk(WEB_PREFIX "%s: cannot read user memory\n", __func__);
		return;
	}

	m = swap_msg_get(MSG_WEB_FUNCTION_ENTRY);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	/* Pack message */
	/* PID */
	*(u32 *)p = task->tgid;
	p += sizeof(u32);
	/* TID */
	*(u32 *)p = task->pid;
	p += sizeof(u32);
	/* Line number (in source file) */
	*(u32 *)p = lnum;
	p += sizeof(u32);

	size -= 3 * sizeof(u32);
	pack_size += 3 * sizeof(u32);

	/* Function name */
	ret = pack_web_string(p, size, (struct MStringImpl *)t_name);
	if (ret < 0)
		goto put_msg;

	p += ret;
	size -= ret;
	pack_size += ret;

	/* URL (source file) */
	ret = pack_web_string(p, size, (struct MStringImpl *)t_url);
	if (ret < 0)
		goto put_msg;

	swap_msg_flush(m, pack_size + ret);

put_msg:
	swap_msg_put(m);
}

void web_msg_exit(struct pt_regs *regs)
{
	int ret;
	struct swap_msg *m;
	void *p;
	long t_name;
	size_t pack_size = 0, size;
	struct MCallIdentifier *call_id;
	struct task_struct *task = current;

	if (!check_event(task))
		return;

	call_id = (void *)swap_get_uarg(regs, 2);
	if (get_user(t_name, (long *)((long)call_id + OFFSET_NAME))) {
		printk(WEB_PREFIX "%s: cannot read user memory\n", __func__);
		return;
	}

	m = swap_msg_get(MSG_WEB_FUNCTION_EXIT);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	/* PID */
	*(u32 *)p = task->tgid;
	p += sizeof(u32);

	/* TID */
	*(u32 *)p = task->pid;
	p += sizeof(u32);

	size -= 2 * sizeof(u32);
	pack_size += 2 * sizeof(u32);

	/* Function name */
	ret = pack_web_string(p, size, (struct MStringImpl *)t_name);
	if (ret < 0)
		goto put_msg;

	swap_msg_flush(m, pack_size + ret);

put_msg:
	swap_msg_put(m);
}
