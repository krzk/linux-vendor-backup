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


#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ctype.h>
#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <kprobe/swap_kprobes.h>
#include <buffer/swap_buffer_module.h>
#include <swap-asm/swap_kprobes.h>
#include <swap-asm/swap_uprobes.h>
#include "swap_msg.h"


#define MSG_PREFIX      KERN_INFO "[SWAP_MSG] "


struct swap_msg {
	u32 msg_id;
	u32 seq_num;
	u64 time;
	u32 len;
	char payload[0];
} __packed;


static char *cpu_buf[NR_CPUS];
static atomic_t seq_num = ATOMIC_INIT(-1);
static atomic_t discarded = ATOMIC_INIT(0);


int swap_msg_init(void)
{
	size_t i;
	const size_t end = ((size_t) 0) - 1;

	for (i = 0; i < NR_CPUS; ++i) {
		cpu_buf[i] = kmalloc(SWAP_MSG_BUF_SIZE, GFP_KERNEL);
		if (cpu_buf[i] == NULL)
			goto no_mem;
	}

	return 0;

no_mem:
	--i;
	for (; i != end; --i)
		kfree(cpu_buf[i]);

	return -ENOMEM;
}

void swap_msg_exit(void)
{
	int i;

	for (i = 0; i < NR_CPUS; ++i)
		kfree(cpu_buf[i]);
}

void swap_msg_seq_num_reset(void)
{
	atomic_set(&seq_num, -1);
}
EXPORT_SYMBOL_GPL(swap_msg_seq_num_reset);

void swap_msg_discard_reset(void)
{
	atomic_set(&discarded, 0);
}
EXPORT_SYMBOL_GPL(swap_msg_discard_reset);

int swap_msg_discard_get(void)
{
	return atomic_read(&discarded);
}
EXPORT_SYMBOL_GPL(swap_msg_discard_get);


u64 swap_msg_timespec2time(struct timespec *ts)
{
	return ((u64)ts->tv_nsec) << 32 | ts->tv_sec;
}





struct swap_msg *swap_msg_get(enum swap_msg_id id)
{
	struct swap_msg *m;

	m = (struct swap_msg *)cpu_buf[get_cpu()];

	m->msg_id = (u32)id;
	m->seq_num = atomic_inc_return(&seq_num);
	m->time = swap_msg_current_time();

	return m;
}
EXPORT_SYMBOL_GPL(swap_msg_get);

static int __swap_msg_flush(struct swap_msg *m, size_t size, bool wakeup)
{
	if (unlikely(size >= SWAP_MSG_PAYLOAD_SIZE))
		return -ENOMEM;

	m->len = size;

	if (swap_buffer_write(m, SWAP_MSG_PRIV_DATA + size, wakeup) !=
	    (SWAP_MSG_PRIV_DATA + size)) {
		atomic_inc(&discarded);
		return -EINVAL;
	}

	return 0;
}

int swap_msg_flush(struct swap_msg *m, size_t size)
{
	return __swap_msg_flush(m, size, true);
}
EXPORT_SYMBOL_GPL(swap_msg_flush);

int swap_msg_flush_wakeupoff(struct swap_msg *m, size_t size)
{
	return __swap_msg_flush(m, size, false);
}
EXPORT_SYMBOL_GPL(swap_msg_flush_wakeupoff);

void swap_msg_put(struct swap_msg *m)
{
	put_cpu();
}
EXPORT_SYMBOL_GPL(swap_msg_put);






static int do_pack_custom_args(char *buf, int len, const char *fmt, va_list args)
{
	enum { max_str_len = 512 };
	const char *p;
	char *buf_orig = buf;

	for (p = fmt; *p != '\0'; p++) {
		char ch = *p;

		if (len < 1)
			return -ENOMEM;

		*buf = tolower(ch);
		buf += 1;
		len -= 1;

		switch (ch) {
		case 'b': /* 1 byte(bool) */
			if (len < 1)
				return -ENOMEM;
			*buf = !!(char)va_arg(args, int);
			buf += 1;
			len -= 1;
			break;
		case 'c': /* 1 byte(char) */
			if (len < 1)
				return -ENOMEM;
			*buf = (char)va_arg(args, int);
			buf += 1;
			len -= 1;
			break;
		case 'f': /* 4 byte(float) */
		case 'd': /* 4 byte(int) */
			if (len < 4)
				return -ENOMEM;
			*(u32 *)buf = va_arg(args, u32);
			buf += 4;
			len -= 4;
			break;
		case 'x': /* 8 byte(long) */
		case 'w': /* 8 byte(double) */
			if (len < 8)
				return -ENOMEM;
			*(u64 *)buf = va_arg(args, u64);
			buf += 8;
			len -= 8;
			break;
		case 'p': /* 8 byte(pointer) */
			if (len < 8)
				return -ENOMEM;
			*(u64 *)buf = va_arg(args, unsigned long);
			buf += 8;
			len -= 8;
			break;
		case 's': /* userspace string with '\0' terminating byte */
		{
			const char __user *str;
			int len_s, n;

			str = va_arg(args, const char __user *);
			/* strnlen_user includes '\0' in its return value */
			len_s = strnlen_user(str, max_str_len);
			if (len < len_s)
				return -ENOMEM;
			/* strncpy_from_user returns the length of the copied
			 * string (without '\0') */
			n = strncpy_from_user(buf, str, len_s - 1);
			if (n < 0)
				return n;
			buf[n] = '\0';

			buf += n + 1;
			len -= n + 1;
			break;
		}
		case 'S': /* kernelspace string with '\0' terminating byte */
		{
			const char *str;
			int len_s;

			str = va_arg(args, const char *);
			len_s = strnlen(str, max_str_len);
			if (len < len_s + 1) /* + '\0' */
				return -ENOMEM;
			strncpy(buf, str, len_s);
			buf[len_s] = '\0';

			buf += len_s + 1;
			len -= len_s + 1;
			break;
		}
		case 'a': /* userspace byte array (len + ptr) */
		{
			const void __user *ptr;
			u32 len_p, n;

			len_p = va_arg(args, u32);
			if (len < sizeof(len_p) + len_p)
				return -ENOMEM;
			*(u32 *)buf = len_p;
			buf += sizeof(len_p);
			len -= sizeof(len_p);

			ptr = va_arg(args, const void __user *);
			n = copy_from_user(buf, ptr, len_p);
			if (n < len_p)
				return -EFAULT;
			buf += len_p;
			len -= len_p;
			break;
		}
		case 'A': /* kernelspace byte array (len + ptr) */
		{
			const void *ptr;
			u32 len_p;

			/* array size */
			len_p = va_arg(args, u32);
			if (len < sizeof(len_p) + len_p)
				return -ENOMEM;
			*(u32 *)buf = len_p;
			buf += sizeof(len_p);
			len -= sizeof(len_p);

			/* byte array */
			ptr = va_arg(args, const void *);
			memcpy(buf, ptr, len_p);
			buf += len_p;
			len -= len_p;
			break;
		}
		default:
			return -EINVAL;
		}
	}

	return buf - buf_orig;
}

int swap_msg_pack_custom_args(char *buf, int len, const char *fmt, ...)
{
	int ret;
	va_list vargs;

	va_start(vargs, fmt);
	ret = do_pack_custom_args(buf, len, fmt, vargs);
	va_end(vargs);

	return ret;
}
EXPORT_SYMBOL_GPL(swap_msg_pack_custom_args);

static unsigned long get_arg(struct pt_regs *regs, unsigned long n)
{
	return user_mode(regs) ?
			swap_get_uarg(regs, n) :	/* US argument */
			swap_get_sarg(regs, n);		/* sys_call argument */
}

int swap_msg_pack_args(char *buf, int len,
		       const char *fmt, struct pt_regs *regs)
{
	char *buf_old = buf;
	u32 *tmp_u32;
	u64 *tmp_u64;
	int i,		/* the index of the argument */
	    fmt_i,	/* format index */
	    fmt_len;	/* the number of parameters, in format */

	fmt_len = strlen(fmt);

	for (i = 0, fmt_i = 0; fmt_i < fmt_len; ++i, ++fmt_i) {
		if (len < 2)
			return -ENOMEM;

		*buf = fmt[fmt_i];
		buf += 1;
		len -= 1;

		switch (fmt[fmt_i]) {
		case 'b': /* 1 byte(bool) */
			*buf = (char)!!get_arg(regs, i);
			buf += 1;
			len -= 1;
			break;
		case 'c': /* 1 byte(char) */
			*buf = (char)get_arg(regs, i);
			buf += 1;
			len -= 1;
			break;
		case 'f': /* 4 byte(float) */
		case 'd': /* 4 byte(int) */
			if (len < 4)
				return -ENOMEM;
			tmp_u32 = (u32 *)buf;
			*tmp_u32 = (u32)get_arg(regs, i);
			buf += 4;
			len -= 4;
			break;
		case 'x': /* 8 byte(long) */
		case 'p': /* 8 byte(pointer) */
			if (len < 8)
				return -ENOMEM;
			tmp_u64 = (u64 *)buf;
			*tmp_u64 = (u64)get_arg(regs, i);
			buf += 8;
			len -= 8;
			break;
		case 'w': /* 8 byte(double) */
			if (len < 8)
				return -ENOMEM;
			tmp_u64 = (u64 *)buf;
			*tmp_u64 = get_arg(regs, i);
			++i;
			*tmp_u64 |= (u64)get_arg(regs, i) << 32;
			buf += 8;
			len -= 8;
			break;
		case 's': /* string end with '\0' */
		{
			enum { max_str_len = 512 };
			const char __user *user_s;
			int len_s, ret;

			user_s = (const char __user *)get_arg(regs, i);
			len_s = strnlen_user(user_s, max_str_len);
			if (len < len_s)
				return -ENOMEM;

			ret = strncpy_from_user(buf, user_s, len_s);
			if (ret < 0)
				return -EFAULT;

			buf[ret] = '\0';

			buf += ret + 1;
			len -= ret + 1;
		}
			break;
		default:
			return -EINVAL;
		}
	}

	return buf - buf_old;
}
EXPORT_SYMBOL_GPL(swap_msg_pack_args);

int swap_msg_pack_ret_val(char *buf, int len,
			  char ret_type, struct pt_regs *regs)
{
	const char *buf_old = buf;
	u32 *tmp_u32;
	u64 *tmp_u64;

	*buf = ret_type;
	++buf;

	switch (ret_type) {
	case 'b': /* 1 byte(bool) */
		if (len < 1)
			return -ENOMEM;
		*buf = (char)!!regs_return_value(regs);
		++buf;
		break;
	case 'c': /* 1 byte(char) */
		if (len < 1)
			return -ENOMEM;
		*buf = (char)regs_return_value(regs);
		++buf;
		break;
	case 'd': /* 4 byte(int) */
		if (len < 4)
			return -ENOMEM;
		tmp_u32 = (u32 *)buf;
		*tmp_u32 = regs_return_value(regs);
		buf += 4;
		break;
	case 'x': /* 8 byte(long) */
	case 'p': /* 8 byte(pointer) */
		if (len < 8)
			return -ENOMEM;
		tmp_u64 = (u64 *)buf;
		*tmp_u64 = (u64)regs_return_value(regs);
		buf += 8;
		break;
	case 's': /* string end with '\0' */
	{
		enum { max_str_len = 512 };
		const char __user *user_s;
		int len_s, ret;

		user_s = (const char __user *)regs_return_value(regs);
		len_s = strnlen_user(user_s, max_str_len);
		if (len < len_s)
			return -ENOMEM;

		ret = strncpy_from_user(buf, user_s, len_s);
		if (ret < 0)
			return -EFAULT;

		buf[ret] = '\0';
		buf += ret + 1;
	}
		break;
	case 'n':
	case 'v':
		break;
	case 'f': /* 4 byte(float) */
		if (len < 4)
			return -ENOMEM;
		tmp_u32 = (u32 *)buf;
		*tmp_u32 = swap_get_urp_float(regs);
		buf += 4;
		break;
	case 'w': /* 8 byte(double) */
		if (len < 8)
			return -ENOMEM;
		tmp_u64 = (u64 *)buf;
		*tmp_u64 = swap_get_urp_double(regs);
		buf += 8;
		break;
	default:
		return -EINVAL;
	}

	return buf - buf_old;
}
EXPORT_SYMBOL_GPL(swap_msg_pack_ret_val);





int swap_msg_raw(void *data, size_t size)
{
	struct swap_msg *m = (struct swap_msg *)data;

	if (sizeof(*m) > size) {
		printk(MSG_PREFIX "ERROR: message RAW small size=%zu\n", size);
		return -EINVAL;
	}

	if (m->len + sizeof(*m) != size) {
		printk(MSG_PREFIX "ERROR: message RAW wrong format\n");
		return -EINVAL;
	}

	m->seq_num = atomic_inc_return(&seq_num);

	/* TODO: What should be returned?! When message was discarded. */
	if (swap_buffer_write(m, size, true) != size)
		atomic_inc(&discarded);

	return size;
}
EXPORT_SYMBOL_GPL(swap_msg_raw);

void swap_msg_error(const char *fmt, ...)
{
	int ret;
	struct swap_msg *m;
	void *p;
	size_t size;
	va_list args;

	m = swap_msg_get(MSG_ERROR);
	p = swap_msg_payload(m);
	size = swap_msg_size(m);

	va_start(args, fmt);
	ret = vsnprintf(p, size, fmt, args);
	va_end(args);

	if (ret <= 0) {
		printk(MSG_PREFIX "ERROR: msg error packing, ret=%d\n", ret);
		goto put_msg;
	}

	swap_msg_flush(m, ret + 1);

put_msg:
	swap_msg_put(m);
}
EXPORT_SYMBOL_GPL(swap_msg_error);
