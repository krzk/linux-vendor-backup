/*
 * Copyright (C) 2010 Samsung Electronics.
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

#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <linux/kallsyms.h>
#include <linux/platform_data/modem.h>

#include "modem_prj.h"
#include "modem_link_device_dpram.h"
#include "modem_utils.h"

/*
** Function prototypes for basic DPRAM operations
*/
static inline void clear_intr(struct dpram_link_device *dpld);
static inline u16 recv_intr(struct dpram_link_device *dpld);
static inline void send_intr(struct dpram_link_device *dpld, u16 mask);

static inline u16 get_magic(struct dpram_link_device *dpld);
static inline void set_magic(struct dpram_link_device *dpld, u16 val);
static inline u16 get_access(struct dpram_link_device *dpld);
static inline void set_access(struct dpram_link_device *dpld, u16 val);

static inline u32 get_tx_head(struct dpram_link_device *dpld, int id);
static inline u32 get_tx_tail(struct dpram_link_device *dpld, int id);
static inline void set_tx_head(struct dpram_link_device *dpld, int id, u32 in);
static inline void set_tx_tail(struct dpram_link_device *dpld, int id, u32 out);
static inline u8 *get_tx_buff(struct dpram_link_device *dpld, int id);
static inline u32 get_tx_buff_size(struct dpram_link_device *dpld, int id);

static inline u32 get_rx_head(struct dpram_link_device *dpld, int id);
static inline u32 get_rx_tail(struct dpram_link_device *dpld, int id);
static inline void set_rx_head(struct dpram_link_device *dpld, int id, u32 in);
static inline void set_rx_tail(struct dpram_link_device *dpld, int id, u32 out);
static inline u8 *get_rx_buff(struct dpram_link_device *dpld, int id);
static inline u32 get_rx_buff_size(struct dpram_link_device *dpld, int id);

static inline u16 get_mask_req_ack(struct dpram_link_device *dpld, int id);
static inline u16 get_mask_res_ack(struct dpram_link_device *dpld, int id);
static inline u16 get_mask_send(struct dpram_link_device *dpld, int id);

static inline void reset_tx_circ(struct dpram_link_device *dpld, int dev);
static inline void reset_rx_circ(struct dpram_link_device *dpld, int dev);

static int trigger_force_cp_crash(struct dpram_link_device *dpld);

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
static inline void log_dpram_status(struct dpram_link_device *dpld, char *str)
{
	struct utc_time utc;

	get_utc_time(&utc);

	pr_info("%s%s: %s: [%02d:%02d:%02d.%03d] "
		"ACC{%X %d} FMT{TI:%u TO:%u RI:%u RO:%u} "
		"RAW{TI:%u TO:%u RI:%u RO:%u} INTR{0x%X}\n",
		LOG_TAG, dpld->ld.mc->name, str,
		utc.hour, utc.min, utc.sec, utc.msec,
		get_magic(dpld), get_access(dpld),
		get_tx_head(dpld, IPC_FMT), get_tx_tail(dpld, IPC_FMT),
		get_rx_head(dpld, IPC_FMT), get_rx_tail(dpld, IPC_FMT),
		get_tx_head(dpld, IPC_RAW), get_tx_tail(dpld, IPC_RAW),
		get_rx_head(dpld, IPC_RAW), get_rx_tail(dpld, IPC_RAW),
		recv_intr(dpld));
}

static void pr_trace(struct dpram_link_device *dpld, int dev,
			struct timespec *ts, u8 *buff, u32 rcvd)
{
	struct link_device *ld = &dpld->ld;
	struct utc_time utc;

	ts2utc(ts, &utc);

	pr_info("%s[%d-%02d-%02d %02d:%02d:%02d.%03d] %s trace (%s)\n",
		LOG_TAG, utc.year, utc.mon, utc.day, utc.hour, utc.min, utc.sec,
		utc.msec, get_dev_name(dev), ld->name);

	mif_print_dump(buff, rcvd, 4);

	return;
}

static void save_dpram_dump_work(struct work_struct *work)
{
	struct dpram_link_device *dpld;
	struct link_device *ld;
	struct trace_queue *trq;
	struct trace_data *trd;
	struct file *fp;
	struct timespec *ts;
	u8 *dump;
	int rcvd;
	char *path;
	struct utc_time utc;

	dpld = container_of(work, struct dpram_link_device, dump_dwork.work);
	ld = &dpld->ld;
	trq = &dpld->dump_list;
	path = dpld->dump_path;

	while (1) {
		trd = trq_get_data_slot(trq);
		if (!trd)
			break;

		ts = &trd->ts;
		dump = trd->data;
		rcvd = trd->size;

		ts2utc(ts, &utc);
		snprintf(path, MIF_MAX_PATH_LEN,
			"%s/%s_dump_%d%02d%02d-%02d%02d%02d",
			MIF_LOG_DIR, ld->name, utc.year, utc.mon, utc.day,
			utc.hour, utc.min, utc.sec);

		fp = mif_open_file(path);
		if (fp) {
			mif_save_file(fp, dump, rcvd);
			mif_close_file(fp);
		} else {
			mif_err("%s: ERR! %s open fail\n", ld->name, path);
			mif_print_dump(dump, rcvd, 16);
		}

		kfree(dump);
	}
}

static void save_dpram_dump(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;
	struct trace_data *trd;
	u8 *buff;
	struct timespec ts;

	buff = kzalloc(dpld->size, GFP_ATOMIC);
	if (!buff) {
		mif_err("%s: ERR! kzalloc fail\n", ld->name);
		return;
	}

	getnstimeofday(&ts);

	memcpy(buff, dpld->base, dpld->size);

	trd = trq_get_free_slot(&dpld->dump_list);
	if (!trd) {
		mif_err("%s: ERR! trq_get_free_slot fail\n", ld->name);
		mif_print_dump(buff, dpld->size, 16);
		kfree(buff);
		return;
	}

	memcpy(&trd->ts, &ts, sizeof(struct timespec));
	trd->data = buff;
	trd->size = dpld->size;

	queue_delayed_work(system_nrt_wq, &dpld->dump_dwork, 0);
}

static void save_ipc_trace_work(struct work_struct *work)
{
	struct dpram_link_device *dpld;
	struct link_device *ld;
	struct trace_queue *trq;
	struct trace_data *trd;
	struct file *fp;
	struct timespec *ts;
	int dev;
	u8 *dump;
	int rcvd;
	u8 *buff;
	char *path;
	struct utc_time utc;

	dpld = container_of(work, struct dpram_link_device, trace_dwork.work);
	ld = &dpld->ld;
	trq = &dpld->trace_list;
	path = dpld->trace_path;

	buff = kzalloc(dpld->size << 3, GFP_KERNEL);
	if (!buff) {
		while (1) {
			trd = trq_get_data_slot(trq);
			if (!trd)
				break;

			ts = &trd->ts;
			dev = trd->dev;
			dump = trd->data;
			rcvd = trd->size;
			pr_trace(dpld, dev, ts, dump, rcvd);

			kfree(dump);
		}
		return;
	}

	while (1) {
		trd = trq_get_data_slot(trq);
		if (!trd)
			break;

		ts = &trd->ts;
		dev = trd->dev;
		dump = trd->data;
		rcvd = trd->size;

		ts2utc(ts, &utc);
		snprintf(path, MIF_MAX_PATH_LEN,
			"%s/%s_%s_%d%02d%02d-%02d%02d%02d",
			MIF_LOG_DIR, ld->name, get_dev_name(dev),
			utc.year, utc.mon, utc.day, utc.hour, utc.min, utc.sec);

		fp = mif_open_file(path);
		if (fp) {
			int len;

			snprintf(buff, MIF_MAX_PATH_LEN,
				"[%d-%02d-%02d %02d:%02d:%02d.%03d]\n",
				utc.year, utc.mon, utc.day, utc.hour, utc.min,
				utc.sec, utc.msec);
			len = strlen(buff);
			mif_dump2format4(dump, rcvd, (buff + len), NULL);
			strcat(buff, "\n");
			len = strlen(buff);

			mif_save_file(fp, buff, len);

			memset(buff, 0, len);
			mif_close_file(fp);
		} else {
			mif_err("%s: %s open fail\n", ld->name, path);
			pr_trace(dpld, dev, ts, dump, rcvd);
		}

		kfree(dump);
	}

	kfree(buff);
}

static void print_ipc_trace(struct dpram_link_device *dpld, int dev,
			u8 __iomem *src, u32 qsize, u32 in, u32 out, u32 rcvd)
{
	u8 *buff = dpld->buff;
	struct timespec ts;

	getnstimeofday(&ts);

	memset(buff, 0, dpld->size);
	circ_read(buff, src, qsize, out, rcvd);

	pr_trace(dpld, dev, &ts, buff, rcvd);
}

static void save_ipc_trace(struct dpram_link_device *dpld, int dev,
			u8 __iomem *src, u32 qsize, u32 in, u32 out, u32 rcvd)
{
	struct link_device *ld = &dpld->ld;
	struct trace_data *trd;
	u8 *buff;
	struct timespec ts;

	buff = kzalloc(rcvd, GFP_ATOMIC);
	if (!buff) {
		mif_err("%s: %s: ERR! kzalloc fail\n",
			ld->name, get_dev_name(dev));
		print_ipc_trace(dpld, dev, src, qsize, in, out, rcvd);
		return;
	}

	getnstimeofday(&ts);

	circ_read(buff, src, qsize, out, rcvd);

	trd = trq_get_free_slot(&dpld->trace_list);
	if (!trd) {
		mif_err("%s: %s: ERR! trq_get_free_slot fail\n",
			ld->name, get_dev_name(dev));
		pr_trace(dpld, dev, &ts, buff, rcvd);
		kfree(buff);
		return;
	}

	memcpy(&trd->ts, &ts, sizeof(struct timespec));
	trd->dev = dev;
	trd->data = buff;
	trd->size = rcvd;

	queue_delayed_work(system_nrt_wq, &dpld->trace_dwork, 0);
}
#endif

static void set_dpram_map(struct dpram_link_device *dpld,
			struct mif_irq_map *map)
{
	map->magic = get_magic(dpld);
	map->access = get_access(dpld);

	map->fmt_tx_in = get_tx_head(dpld, IPC_FMT);
	map->fmt_tx_out = get_tx_tail(dpld, IPC_FMT);
	map->fmt_rx_in = get_rx_head(dpld, IPC_FMT);
	map->fmt_rx_out = get_rx_tail(dpld, IPC_FMT);
	map->raw_tx_in = get_tx_head(dpld, IPC_RAW);
	map->raw_tx_out = get_tx_tail(dpld, IPC_RAW);
	map->raw_rx_in = get_rx_head(dpld, IPC_RAW);
	map->raw_rx_out = get_rx_tail(dpld, IPC_RAW);

	map->cp2ap = recv_intr(dpld);
}

/*
** DPRAM operations
*/
static int register_isr(unsigned irq, irqreturn_t (*isr)(int, void*),
				unsigned long flag, const char *name,
				struct dpram_link_device *dpld)
{
	int ret;

	ret = request_irq(irq, isr, flag, name, dpld);
	if (ret) {
		mif_info("%s: ERR! request_irq fail (err %d)\n", name, ret);
		return ret;
	}

	ret = enable_irq_wake(irq);
	if (ret)
		mif_info("%s: ERR! enable_irq_wake fail (err %d)\n", name, ret);

	mif_info("%s (#%d) handler registered\n", name, irq);

	return 0;
}

static inline void clear_intr(struct dpram_link_device *dpld)
{
	if (likely(dpld->need_intr_clear))
		dpld->ext_op->clear_intr(dpld);
}

static inline u16 recv_intr(struct dpram_link_device *dpld)
{
	return ioread16(dpld->mbx2ap);
}

static inline void send_intr(struct dpram_link_device *dpld, u16 mask)
{
	iowrite16(mask, dpld->mbx2cp);
}

static inline u16 get_magic(struct dpram_link_device *dpld)
{
	return ioread16(dpld->magic);
}

static inline void set_magic(struct dpram_link_device *dpld, u16 val)
{
	iowrite16(val, dpld->magic);
}

static inline u16 get_access(struct dpram_link_device *dpld)
{
	return ioread16(dpld->access);
}

static inline void set_access(struct dpram_link_device *dpld, u16 val)
{
	iowrite16(val, dpld->access);
}

static inline u32 get_tx_head(struct dpram_link_device *dpld, int id)
{
	return ioread16(dpld->dev[id]->txq.head);
}

static inline u32 get_tx_tail(struct dpram_link_device *dpld, int id)
{
	return ioread16(dpld->dev[id]->txq.tail);
}

static inline void set_tx_head(struct dpram_link_device *dpld, int id, u32 in)
{
	int cnt = 0;
	u32 val = 0;

	iowrite16((u16)in, dpld->dev[id]->txq.head);

	while (1) {
		/* Check head value written */
		val = ioread16(dpld->dev[id]->txq.head);
		if (likely(val == in))
			return;

		cnt++;
		mif_err("ERR: %s txq.head(%d) != in(%d), count %d\n",
			get_dev_name(id), val, in, cnt);
		if (cnt >= MAX_RETRY_CNT)
			break;

		/* Write head value again */
		udelay(100);
		iowrite16((u16)in, dpld->dev[id]->txq.head);
	}

	trigger_force_cp_crash(dpld);
}

static inline void set_tx_tail(struct dpram_link_device *dpld, int id, u32 out)
{
	int cnt = 0;
	u32 val = 0;

	iowrite16((u16)out, dpld->dev[id]->txq.tail);

	while (1) {
		/* Check tail value written */
		val = ioread16(dpld->dev[id]->txq.tail);
		if (likely(val == out))
			return;

		cnt++;
		mif_err("ERR: %s txq.tail(%d) != out(%d), count %d\n",
			get_dev_name(id), val, out, cnt);
		if (cnt >= MAX_RETRY_CNT)
			break;

		/* Write tail value again */
		udelay(100);
		iowrite16((u16)out, dpld->dev[id]->txq.tail);
	}

	trigger_force_cp_crash(dpld);
}

static inline u8 *get_tx_buff(struct dpram_link_device *dpld, int id)
{
	return dpld->dev[id]->txq.buff;
}

static inline u32 get_tx_buff_size(struct dpram_link_device *dpld, int id)
{
	return dpld->dev[id]->txq.size;
}

static inline u32 get_rx_head(struct dpram_link_device *dpld, int id)
{
	return ioread16(dpld->dev[id]->rxq.head);
}

static inline u32 get_rx_tail(struct dpram_link_device *dpld, int id)
{
	return ioread16(dpld->dev[id]->rxq.tail);
}

static inline void set_rx_head(struct dpram_link_device *dpld, int id, u32 in)
{
	int cnt = 0;
	u32 val = 0;

	iowrite16((u16)in, dpld->dev[id]->rxq.head);

	while (1) {
		/* Check head value written */
		val = ioread16(dpld->dev[id]->rxq.head);
		if (val == in)
			return;

		cnt++;
		mif_err("ERR: %s rxq.head(%d) != in(%d), count %d\n",
			get_dev_name(id), val, in, cnt);
		if (cnt >= MAX_RETRY_CNT)
			break;

		/* Write head value again */
		udelay(100);
		iowrite16((u16)in, dpld->dev[id]->rxq.head);
	}

	trigger_force_cp_crash(dpld);
}

static inline void set_rx_tail(struct dpram_link_device *dpld, int id, u32 out)
{
	int cnt = 0;
	u32 val = 0;

	iowrite16((u16)out, dpld->dev[id]->rxq.tail);

	while (1) {
		/* Check tail value written */
		val = ioread16(dpld->dev[id]->rxq.tail);
		if (val == out)
			return;

		cnt++;
		mif_err("ERR: %s rxq.tail(%d) != out(%d), count %d\n",
			get_dev_name(id), val, out, cnt);
		if (cnt >= MAX_RETRY_CNT)
			break;

		/* Write tail value again */
		udelay(100);
		iowrite16((u16)out, dpld->dev[id]->rxq.tail);
	}

	trigger_force_cp_crash(dpld);
}

static inline u8 *get_rx_buff(struct dpram_link_device *dpld, int id)
{
	return dpld->dev[id]->rxq.buff;
}

static inline u32 get_rx_buff_size(struct dpram_link_device *dpld, int id)
{
	return dpld->dev[id]->rxq.size;
}

static inline u16 get_mask_req_ack(struct dpram_link_device *dpld, int id)
{
	return dpld->dev[id]->mask_req_ack;
}

static inline u16 get_mask_res_ack(struct dpram_link_device *dpld, int id)
{
	return dpld->dev[id]->mask_res_ack;
}

static inline u16 get_mask_send(struct dpram_link_device *dpld, int id)
{
	return dpld->dev[id]->mask_send;
}

/* Get free space in the TXQ as well as in & out pointers */
static int get_txq_space(struct dpram_link_device *dpld, int dev, u32 qsize,
			u32 *in, u32 *out)
{
	struct link_device *ld = &dpld->ld;
	int cnt = 0;
	u32 head;
	u32 tail;
	int space;

	while (1) {
		head = get_tx_head(dpld, dev);
		tail = get_tx_tail(dpld, dev);

		space = circ_get_space(qsize, head, tail);
		mif_debug("%s: %s_TXQ qsize[%u] in[%u] out[%u] space[%u]\n",
			ld->name, get_dev_name(dev), qsize, head, tail, space);

		if (circ_valid(qsize, head, tail)) {
			*in = head;
			*out = tail;
			return space;
		}

		cnt++;
		mif_err("%s: ERR! <%pf> "
			"%s_TXQ invalid (size:%d in:%d out:%d), count %d\n",
			ld->name, __builtin_return_address(0),
			get_dev_name(dev), qsize, head, tail, cnt);
		if (cnt >= MAX_RETRY_CNT)
			break;

		udelay(100);
	}

	*in = 0;
	*out = 0;
	return -EINVAL;
}

/* Get data size in the RXQ as well as in & out pointers */
static int get_rxq_rcvd(struct dpram_link_device *dpld, int dev, u32 qsize,
			u32 *in, u32 *out)
{
	struct link_device *ld = &dpld->ld;
	int cnt = 0;
	u32 head;
	u32 tail;
	u32 rcvd;

	while (1) {
		head = get_rx_head(dpld, dev);
		tail = get_rx_tail(dpld, dev);
		if (head == tail) {
			*in = head;
			*out = tail;
			return 0;
		}

		rcvd = circ_get_usage(qsize, head, tail);
		mif_debug("%s: %s_RXQ qsize[%u] in[%u] out[%u] rcvd[%u]\n",
			ld->name, get_dev_name(dev), qsize, head, tail, rcvd);

		if (circ_valid(qsize, head, tail)) {
			*in = head;
			*out = tail;
			return rcvd;
		}

		cnt++;
		mif_err("%s: ERR! <%pf> "
			"%s_RXQ invalid (size:%d in:%d out:%d), count %d\n",
			ld->name, __builtin_return_address(0),
			get_dev_name(dev), qsize, head, tail, cnt);
		if (cnt >= MAX_RETRY_CNT)
			break;

		udelay(100);
	}

	*in = 0;
	*out = 0;
	return -EINVAL;
}

static inline void reset_tx_circ(struct dpram_link_device *dpld, int dev)
{
	set_tx_head(dpld, dev, 0);
	set_tx_tail(dpld, dev, 0);
	if (dev == IPC_FMT)
		trigger_force_cp_crash(dpld);
}

static inline void reset_rx_circ(struct dpram_link_device *dpld, int dev)
{
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	trigger_force_cp_crash(dpld);
#else
	set_rx_tail(dpld, dev, get_rx_head(dpld, dev));
	if (dev == IPC_FMT)
		trigger_force_cp_crash(dpld);
#endif
}

/*
** CAUTION : dpram_allow_sleep() MUST be invoked after dpram_wake_up() success
*/
static inline bool dpram_can_sleep(struct dpram_link_device *dpld)
{
	return dpld->need_wake_up;
}

static int dpram_wake_up(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;

	if (unlikely(!dpram_can_sleep(dpld)))
		return 0;

	if (dpld->ext_op->wakeup(dpld) < 0) {
		mif_err("%s: ERR! <%pf> DPRAM wakeup fail once\n",
			ld->name, __builtin_return_address(0));

		dpld->ext_op->sleep(dpld);

		udelay(10);

		if (dpld->ext_op->wakeup(dpld) < 0) {
			mif_err("%s: ERR! <%pf> DPRAM wakeup fail twice\n",
				ld->name, __builtin_return_address(0));
			return -EACCES;
		}
	}

	atomic_inc(&dpld->accessing);
	return 0;
}

static void dpram_allow_sleep(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;

	if (unlikely(!dpram_can_sleep(dpld)))
		return;

	if (atomic_dec_return(&dpld->accessing) <= 0) {
		dpld->ext_op->sleep(dpld);
		atomic_set(&dpld->accessing, 0);
		mif_debug("%s: DPRAM sleep possible\n", ld->name);
	}
}

static int check_access(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;
	int i;
	u16 magic = get_magic(dpld);
	u16 access = get_access(dpld);

	if (likely(magic == DPRAM_MAGIC_CODE && access == 1))
		return 0;

	for (i = 1; i <= 100; i++) {
		mif_info("%s: ERR! magic:%X access:%X -> retry:%d\n",
			ld->name, magic, access, i);
		udelay(100);

		magic = get_magic(dpld);
		access = get_access(dpld);
		if (likely(magic == DPRAM_MAGIC_CODE && access == 1))
			return 0;
	}

	mif_info("%s: !CRISIS! magic:%X access:%X\n", ld->name, magic, access);
	return -EACCES;
}

static bool ipc_active(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;

	/* Check DPRAM mode */
	if (ld->mode != LINK_MODE_IPC) {
		mif_info("%s: <%pf> ld->mode != LINK_MODE_IPC\n",
			ld->name, __builtin_return_address(0));
		return false;
	}

	if (check_access(dpld) < 0) {
		mif_info("%s: ERR! <%pf> check_access fail\n",
			ld->name, __builtin_return_address(0));
		return false;
	}

	return true;
}

static void dpram_ipc_write(struct dpram_link_device *dpld, int dev, u32 qsize,
			u32 in, u32 out, struct sk_buff *skb)
{
	struct link_device *ld = &dpld->ld;
	u8 __iomem *buff = get_tx_buff(dpld, dev);
	u8 *src = skb->data;
	u32 len = skb->len;
	u32 inp;
	struct mif_irq_map map;

	circ_write(buff, src, qsize, in, len);

	/* update new in pointer */
	inp = in + len;
	if (inp >= qsize)
		inp -= qsize;
	set_tx_head(dpld, dev, inp);

	if (dev == IPC_FMT) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		char tag[MIF_MAX_STR_LEN];
		snprintf(tag, MIF_MAX_STR_LEN, "%s: MIF2CP", ld->mc->name);
		pr_ipc(tag, src, (len > 20 ? 20 : len));
		log_dpram_status(dpld, "MIF2CP");
#endif
		set_dpram_map(dpld, &map);
		mif_irq_log(ld->mc->msd, map, "ipc_write", sizeof("ipc_write"));
		mif_ipc_log(MIF_IPC_AP2CP, ld->mc->msd, skb->data, skb->len);
	}

#if 1
	if (ld->aligned && memcmp16_to_io((buff + in), src, 4)) {
		mif_err("%s: memcmp16_to_io fail\n", ld->name);
		trigger_force_cp_crash(dpld);
	}
#endif
}

static int dpram_ipc_tx(struct dpram_link_device *dpld, int dev)
{
	struct link_device *ld = &dpld->ld;
	struct sk_buff_head *txq = ld->skb_txq[dev];
	struct sk_buff *skb;
	unsigned long int flags;
	u32 qsize = get_tx_buff_size(dpld, dev);
	u32 in;
	u32 out;
	int space;
	int copied = 0;

	spin_lock_irqsave(&dpld->tx_lock[dev], flags);

	while (1) {
		space = get_txq_space(dpld, dev, qsize, &in, &out);
		if (unlikely(space < 0)) {
			spin_unlock_irqrestore(&dpld->tx_lock[dev], flags);
			reset_tx_circ(dpld, dev);
			return space;
		}

		skb = skb_dequeue(txq);
		if (unlikely(!skb))
			break;

		if (unlikely(space < skb->len)) {
			atomic_set(&dpld->res_required[dev], 1);
			skb_queue_head(txq, skb);
			spin_unlock_irqrestore(&dpld->tx_lock[dev], flags);
			mif_info("%s: %s "
				"qsize[%u] in[%u] out[%u] free[%u] < len[%u]\n",
				ld->name, get_dev_name(dev),
				qsize, in, out, space, skb->len);
			return -ENOSPC;
		}

		/* TX if there is enough room in the queue */
		dpram_ipc_write(dpld, dev, qsize, in, out, skb);
		copied += skb->len;
		dev_kfree_skb_any(skb);
	}

	spin_unlock_irqrestore(&dpld->tx_lock[dev], flags);

	return copied;
}

static int dpram_wait_for_res_ack(struct dpram_link_device *dpld, int dev)
{
	struct link_device *ld = &dpld->ld;
	struct completion *cmpl = &dpld->res_ack_cmpl[dev];
	unsigned long timeout = RES_ACK_WAIT_TIMEOUT;
	int ret;
	u16 mask;

	mask = get_mask_req_ack(dpld, dev);
	mif_info("%s: Send REQ_ACK 0x%04X\n", ld->name, mask);
	send_intr(dpld, INT_NON_CMD(mask));

	ret = wait_for_completion_interruptible_timeout(cmpl, timeout);
	/* ret == 0 on timeout, ret < 0 if interrupted */
	if (ret == 0) {
		mif_err("%s: %s: TIMEOUT\n", ld->name, get_dev_name(dev));
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[dev], 0);
	} else if (ret < 0) {
		mif_err("%s: %s: interrupted (ret %d)\n",
			ld->name, get_dev_name(dev), ret);
	}

	return ret;
}

static int dpram_process_res_ack(struct dpram_link_device *dpld, int dev)
{
	struct link_device *ld = &dpld->ld;
	int ret;
	u16 mask;

	ret = dpram_ipc_tx(dpld, dev);
	if (ret < 0) {
		if (ret == -ENOSPC) {
			/* dpld->res_required[dev] is set in dpram_ipc_tx() */
			queue_delayed_work(ld->tx_wq, ld->tx_dwork[dev], 0);
		} else {
			mif_err("%s: ERR! ipc_tx fail (%d)\n", ld->name, ret);
		}
	} else {
		if (ret > 0) {
			mask = get_mask_send(dpld, dev);
			send_intr(dpld, INT_NON_CMD(mask));
			mif_debug("%s: send intr 0x%04X\n", ld->name, mask);
		}
		atomic_set(&dpld->res_required[dev], 0);
	}

	return ret;
}

static void dpram_fmt_tx_work(struct work_struct *work)
{
	struct link_device *ld;
	struct dpram_link_device *dpld;
	int ret;

	ld = container_of(work, struct link_device, fmt_tx_dwork.work);
	dpld = to_dpram_link_device(ld);

	ret = dpram_wait_for_res_ack(dpld, IPC_FMT);
	/* ret == 0 on timeout, ret < 0 if interrupted */
	if (ret <= 0)
		return;

	ret = dpram_process_res_ack(dpld, IPC_FMT);
}

static void dpram_raw_tx_work(struct work_struct *work)
{
	struct link_device *ld;
	struct dpram_link_device *dpld;
	int ret;

	ld = container_of(work, struct link_device, raw_tx_dwork.work);
	dpld = to_dpram_link_device(ld);

	ret = dpram_wait_for_res_ack(dpld, IPC_RAW);
	/* ret == 0 on timeout, ret < 0 if interrupted */
	if (ret <= 0)
		return;

	ret = dpram_process_res_ack(dpld, IPC_RAW);
	if (ret > 0)
		mif_netif_wake(ld);
}

static void dpram_rfs_tx_work(struct work_struct *work)
{
	struct link_device *ld;
	struct dpram_link_device *dpld;
	int ret;

	ld = container_of(work, struct link_device, rfs_tx_dwork.work);
	dpld = to_dpram_link_device(ld);

	ret = dpram_wait_for_res_ack(dpld, IPC_RFS);
	/* ret == 0 on timeout, ret < 0 if interrupted */
	if (ret <= 0)
		return;

	ret = dpram_process_res_ack(dpld, IPC_RFS);
}

static void dpram_ipc_rx_task(unsigned long data)
{
	struct dpram_link_device *dpld = (struct dpram_link_device *)data;
	struct link_device *ld = &dpld->ld;
	struct io_device *iod;
	struct mif_rxb *rxb;
	unsigned qlen;
	int i;

	for (i = 0; i < ld->max_ipc_dev; i++) {
		iod = dpld->iod[i];
		qlen = rxbq_size(&dpld->rxbq[i]);
		while (qlen > 0) {
			rxb = rxbq_get_data_rxb(&dpld->rxbq[i]);
			iod->recv(iod, ld, rxb->data, rxb->len);
			rxb_clear(rxb);
			qlen--;
		}
	}
}

/*
  ret < 0  : error
  ret == 0 : no data
  ret > 0  : valid data
*/
static int dpram_recv_ipc_with_rxb(struct dpram_link_device *dpld, int dev)
{
	struct link_device *ld = &dpld->ld;
	struct mif_rxb *rxb;
	u8 __iomem *src = get_rx_buff(dpld, dev);
	u32 qsize = get_rx_buff_size(dpld, dev);
	u32 in;
	u32 out;
	u32 rcvd;
	struct mif_irq_map map;

	rcvd = get_rxq_rcvd(dpld, dev, qsize, &in, &out);
	if (rcvd <= 0)
		return rcvd;

	if (dev == IPC_FMT) {
		set_dpram_map(dpld, &map);
		mif_irq_log(ld->mc->msd, map, "ipc_recv", sizeof("ipc_recv"));
	}

	/* Allocate an rxb */
	rxb = rxbq_get_free_rxb(&dpld->rxbq[dev]);
	if (!rxb) {
		mif_info("%s: ERR! %s rxbq_get_free_rxb fail\n",
			ld->name, get_dev_name(dev));
		return -ENOMEM;
	}

	/* Read data from each DPRAM buffer */
	circ_read(rxb_put(rxb, rcvd), src, qsize, out, rcvd);

	/* Update tail (out) pointer */
	set_rx_tail(dpld, dev, in);

	return rcvd;
}

/*
  ret < 0  : error
  ret == 0 : no data
  ret > 0  : valid data
*/
static int dpram_recv_ipc_with_skb(struct dpram_link_device *dpld, int dev)
{
	struct link_device *ld = &dpld->ld;
	struct io_device *iod = dpld->iod[dev];
	struct sk_buff *skb;
	u8 __iomem *src = get_rx_buff(dpld, dev);
	u32 qsize = get_rx_buff_size(dpld, dev);
	u32 in;
	u32 out;
	u32 rcvd;
	int rest;
	u8 *frm;
	u8 *dst;
	unsigned int len;
	unsigned int pad;
	unsigned int tot;
	struct mif_irq_map map;

	rcvd = get_rxq_rcvd(dpld, dev, qsize, &in, &out);
	if (rcvd <= 0)
		return rcvd;

	if (dev == IPC_FMT) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		log_dpram_status(dpld, "CP2MIF");
#endif
		set_dpram_map(dpld, &map);
		mif_irq_log(ld->mc->msd, map, "ipc_recv", sizeof("ipc_recv"));
	}

	rest = rcvd;
	while (rest > 0) {
		/* Calculate the start of an SIPC5 frame */
		frm = src + out;

		/* Check the SIPC5 frame */
		len = sipc5_check_frame_in_dev(ld, dev, frm, rest);
		if (len <= 0) {
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
			u32 tail = get_rx_tail(dpld, dev);
			log_dpram_status(dpld, "CP2MIF");
			save_ipc_trace(dpld, dev, src, qsize, in, tail, rcvd);
			save_dpram_dump(dpld);
#endif
			return -EBADMSG;
		}

		/* Calculate total length with padding in DPRAM */
		pad = sipc5_calc_padding_size(len);
		tot = len + pad;

		/* Allocate an skb */
		skb = dev_alloc_skb(tot);
		if (!skb) {
			mif_err("%s: ERR! %s dev_alloc_skb fail\n",
				ld->name, get_dev_name(dev));
			return -ENOMEM;
		}

		/* Read data from each DPRAM buffer */
		dst = skb_put(skb, tot);
		circ_read(dst, src, qsize, out, tot);
		skb_trim(skb, len);
#if 1
		if (ld->aligned && memcmp16_to_io((src + out), dst, 4)) {
			mif_err("%s: memcmp16_to_io fail\n", ld->name);
			trigger_force_cp_crash(dpld);
		}
#endif
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
		if (unlikely(dev == IPC_FMT)) {
			char str[MIF_MAX_STR_LEN];
			snprintf(str, MIF_MAX_STR_LEN, "%s: CP2MIF",
				ld->mc->name);
			pr_ipc(str, skb->data, (skb->len > 20 ? 20 : skb->len));
		}
#endif

		/* Pass the IPC frame to IOD */
		iod->recv_skb(iod, ld, skb);

		/* Calculate new 'out' pointer */
		rest -= tot;
		out += tot;
		if (out >= qsize)
			out -= qsize;
	}

	/* Update tail (out) pointer */
	set_rx_tail(dpld, dev, in);

	return rcvd;
}

static void non_command_handler(struct dpram_link_device *dpld, u16 intr)
{
	struct link_device *ld = &dpld->ld;
	int i = 0;
	int ret = 0;
	u16 mask = 0;

	if (!ipc_active(dpld))
		return;

	/* Read data from DPRAM */
	for (i = 0; i < ld->max_ipc_dev; i++) {
		if (dpld->use_skb)
			ret = dpram_recv_ipc_with_skb(dpld, i);
		else
			ret = dpram_recv_ipc_with_rxb(dpld, i);
		if (ret < 0)
			reset_rx_circ(dpld, i);

		/* Check and process REQ_ACK (at this time, in == out) */
		if (intr & get_mask_req_ack(dpld, i)) {
			mif_debug("%s: send %s_RES_ACK\n",
				ld->name, get_dev_name(i));
			mask |= get_mask_res_ack(dpld, i);
		}
	}

	if (!dpld->use_skb) {
		/* Schedule soft IRQ for RX */
		tasklet_hi_schedule(&dpld->rx_tsk);
	}

	if (mask) {
		send_intr(dpld, INT_NON_CMD(mask));
		mif_debug("%s: send intr 0x%04X\n", ld->name, mask);
	}

	if (intr && INT_MASK_RES_ACK_SET) {
		if (intr && INT_MASK_RES_ACK_R)
			complete_all(&dpld->res_ack_cmpl[IPC_RAW]);
		else if (intr && INT_MASK_RES_ACK_F)
			complete_all(&dpld->res_ack_cmpl[IPC_FMT]);
		else
			complete_all(&dpld->res_ack_cmpl[IPC_RFS]);
	}
}

static void handle_cp_crash(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;
	struct io_device *iod;
	int i;

	mif_netif_stop(ld);

	for (i = 0; i < ld->max_ipc_dev; i++) {
		mif_info("%s: purging %s_skb_txq\n", ld->name, get_dev_name(i));
		skb_queue_purge(ld->skb_txq[i]);
	}

	iod = link_get_iod_with_format(ld, IPC_FMT);
	iod->modem_state_changed(iod, STATE_CRASH_EXIT);

	iod = link_get_iod_with_format(ld, IPC_BOOT);
	iod->modem_state_changed(iod, STATE_CRASH_EXIT);
}

static void handle_no_crash_ack(unsigned long arg)
{
	struct dpram_link_device *dpld = (struct dpram_link_device *)arg;
	struct link_device *ld = &dpld->ld;

	mif_err("%s: ERR! No CRASH_EXIT ACK from CP\n", ld->mc->name);

	if (!wake_lock_active(&dpld->wlock))
		wake_lock(&dpld->wlock);

	handle_cp_crash(dpld);
}

static int trigger_force_cp_crash(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;

	if (ld->mode == LINK_MODE_ULOAD) {
		mif_err("%s: CP crash is already in progress\n", ld->mc->name);
		return 0;
	}

	disable_irq_nosync(dpld->irq);

	ld->mode = LINK_MODE_ULOAD;
	mif_err("%s: called by %pf\n", ld->name, __builtin_return_address(0));

	dpram_wake_up(dpld);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	save_dpram_dump(dpld);
#endif

	enable_irq(dpld->irq);

	send_intr(dpld, INT_CMD(INT_CMD_CRASH_EXIT));

	mif_add_timer(&dpld->crash_ack_timer, FORCE_CRASH_ACK_TIMEOUT,
			handle_no_crash_ack, (unsigned long)dpld);

	return 0;
}

static int dpram_init_ipc(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;
	int i;

	if (ld->mode == LINK_MODE_IPC &&
	    get_magic(dpld) == DPRAM_MAGIC_CODE &&
	    get_access(dpld) == 1)
		mif_info("%s: IPC already initialized\n", ld->name);

	/* Clear pointers in every circular queue */
	for (i = 0; i < ld->max_ipc_dev; i++) {
		set_tx_head(dpld, i, 0);
		set_tx_tail(dpld, i, 0);
		set_rx_head(dpld, i, 0);
		set_rx_tail(dpld, i, 0);
	}

	/* Initialize variables for efficient TX/RX processing */
	for (i = 0; i < ld->max_ipc_dev; i++)
		dpld->iod[i] = link_get_iod_with_format(ld, i);
	dpld->iod[IPC_RAW] = link_get_iod_with_format(ld, IPC_MULTI_RAW);

	if (dpld->iod[IPC_RAW]->recv_skb)
		dpld->use_skb = true;

	for (i = 0; i < ld->max_ipc_dev; i++) {
		spin_lock_init(&dpld->tx_lock[i]);
		atomic_set(&dpld->res_required[i], 0);
	}

	/* Enable IPC */
	atomic_set(&dpld->accessing, 0);

	set_magic(dpld, DPRAM_MAGIC_CODE);
	set_access(dpld, 1);
	if (get_magic(dpld) != DPRAM_MAGIC_CODE || get_access(dpld) != 1)
		return -EACCES;

	ld->mode = LINK_MODE_IPC;

	if (wake_lock_active(&dpld->wlock))
		wake_unlock(&dpld->wlock);

	return 0;
}

static void cmd_req_active_handler(struct dpram_link_device *dpld)
{
	send_intr(dpld, INT_CMD(INT_CMD_RES_ACTIVE));
}

static void cmd_crash_reset_handler(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;
	struct io_device *iod = NULL;

	ld->mode = LINK_MODE_ULOAD;

	if (!wake_lock_active(&dpld->wlock))
		wake_lock(&dpld->wlock);

	mif_err("%s: Recv 0xC7 (CRASH_RESET)\n", ld->name);

	iod = link_get_iod_with_format(ld, IPC_FMT);
	iod->modem_state_changed(iod, STATE_CRASH_RESET);

	iod = link_get_iod_with_format(ld, IPC_BOOT);
	iod->modem_state_changed(iod, STATE_CRASH_RESET);
}

static void cmd_crash_exit_handler(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;

	ld->mode = LINK_MODE_ULOAD;

	if (!wake_lock_active(&dpld->wlock))
		wake_lock(&dpld->wlock);

	del_timer(&dpld->crash_ack_timer);

	dpram_wake_up(dpld);
#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	save_dpram_dump(dpld);
#endif

	if (dpld->ext_op && dpld->ext_op->crash_log)
		dpld->ext_op->crash_log(dpld);

	mif_err("%s: Recv 0xC9 (CRASH_EXIT)\n", ld->name);

	handle_cp_crash(dpld);
}

static void cmd_phone_start_handler(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;
	struct io_device *iod = NULL;

	mif_info("%s: Recv 0xC8 (CP_START)\n", ld->name);

	dpram_init_ipc(dpld);

	iod = link_get_iod_with_format(ld, IPC_FMT);
	if (!iod) {
		mif_info("%s: ERR! no iod\n", ld->name);
		return;
	}

	if (dpld->ext_op && dpld->ext_op->cp_start_handler)
		dpld->ext_op->cp_start_handler(dpld);

	if (ld->mc->phone_state != STATE_ONLINE) {
		mif_info("%s: phone_state: %d -> ONLINE\n",
			ld->name, ld->mc->phone_state);
		iod->modem_state_changed(iod, STATE_ONLINE);
	}

	mif_info("%s: Send 0xC2 (INIT_END)\n", ld->name);
	send_intr(dpld, INT_CMD(INT_CMD_INIT_END));
}

static void command_handler(struct dpram_link_device *dpld, u16 cmd)
{
	struct link_device *ld = &dpld->ld;

	switch (INT_CMD_MASK(cmd)) {
	case INT_CMD_REQ_ACTIVE:
		cmd_req_active_handler(dpld);
		break;

	case INT_CMD_CRASH_RESET:
		dpld->init_status = DPRAM_INIT_STATE_NONE;
		cmd_crash_reset_handler(dpld);
		break;

	case INT_CMD_CRASH_EXIT:
		dpld->init_status = DPRAM_INIT_STATE_NONE;
		cmd_crash_exit_handler(dpld);
		break;

	case INT_CMD_PHONE_START:
		dpld->init_status = DPRAM_INIT_STATE_READY;
		cmd_phone_start_handler(dpld);
		complete_all(&dpld->dpram_init_cmd);
		break;

	case INT_CMD_NV_REBUILDING:
		mif_info("%s: NV_REBUILDING\n", ld->name);
		break;

	case INT_CMD_PIF_INIT_DONE:
		complete_all(&dpld->modem_pif_init_done);
		break;

	case INT_CMD_SILENT_NV_REBUILDING:
		mif_info("%s: SILENT_NV_REBUILDING\n", ld->name);
		break;

	case INT_CMD_NORMAL_PWR_OFF:
		/*ToDo:*/
		/*kernel_sec_set_cp_ack()*/;
		break;

	case INT_CMD_REQ_TIME_SYNC:
	case INT_CMD_CP_DEEP_SLEEP:
	case INT_CMD_EMER_DOWN:
		break;

	default:
		mif_info("%s: unknown command 0x%04X\n", ld->name, cmd);
	}
}

static void ext_command_handler(struct dpram_link_device *dpld, u16 cmd)
{
	struct link_device *ld = &dpld->ld;
	u16 resp;

	switch (EXT_CMD_MASK(cmd)) {
	case EXT_CMD_SET_SPEED_LOW:
		if (dpld->dpctl->setup_speed) {
			dpld->dpctl->setup_speed(DPRAM_SPEED_LOW);
			resp = INT_EXT_CMD(EXT_CMD_SET_SPEED_LOW);
			send_intr(dpld, resp);
		}
		break;

	case EXT_CMD_SET_SPEED_MID:
		if (dpld->dpctl->setup_speed) {
			dpld->dpctl->setup_speed(DPRAM_SPEED_MID);
			resp = INT_EXT_CMD(EXT_CMD_SET_SPEED_MID);
			send_intr(dpld, resp);
		}
		break;

	case EXT_CMD_SET_SPEED_HIGH:
		if (dpld->dpctl->setup_speed) {
			dpld->dpctl->setup_speed(DPRAM_SPEED_HIGH);
			resp = INT_EXT_CMD(EXT_CMD_SET_SPEED_HIGH);
			send_intr(dpld, resp);
		}
		break;

	default:
		mif_info("%s: unknown command 0x%04X\n", ld->name, cmd);
		break;
	}
}

static void udl_command_handler(struct dpram_link_device *dpld, u16 cmd)
{
	struct link_device *ld = &dpld->ld;

	if (cmd & UDL_RESULT_FAIL) {
		mif_info("%s: ERR! Command failed: %04x\n", ld->name, cmd);
		return;
	}

	switch (UDL_CMD_MASK(cmd)) {
	case UDL_CMD_RECV_READY:
		mif_debug("%s: Send CP-->AP RECEIVE_READY\n", ld->name);
		send_intr(dpld, CMD_IMG_START_REQ);
		break;
	default:
		complete_all(&dpld->udl_cmd_complete);
	}
}

static inline void dpram_ipc_rx(struct dpram_link_device *dpld, u16 intr)
{
	if (unlikely(INT_CMD_VALID(intr)))
		command_handler(dpld, intr);
	else
		non_command_handler(dpld, intr);
}

static inline void dpram_intr_handler(struct dpram_link_device *dpld, u16 intr)
{
	char *name = dpld->ld.name;

	if (unlikely(intr == INT_POWERSAFE_FAIL)) {
		mif_info("%s: intr == INT_POWERSAFE_FAIL\n", name);
		return;
	}

	if (unlikely(EXT_UDL_CMD(intr))) {
		if (likely(EXT_INT_VALID(intr))) {
			if (UDL_CMD_VALID(intr))
				udl_command_handler(dpld, intr);
			else if (EXT_CMD_VALID(intr))
				ext_command_handler(dpld, intr);
			else
				mif_info("%s: ERR! invalid intr 0x%04X\n",
					name, intr);
		} else {
			mif_info("%s: ERR! invalid intr 0x%04X\n", name, intr);
		}
		return;
	}

	if (likely(INT_VALID(intr)))
		dpram_ipc_rx(dpld, intr);
	else
		mif_info("%s: ERR! invalid intr 0x%04X\n", name, intr);
}

static irqreturn_t ap_idpram_irq_handler(int irq, void *data)
{
	struct dpram_link_device *dpld = (struct dpram_link_device *)data;
	struct link_device *ld = (struct link_device *)&dpld->ld;
	u16 int2ap = recv_intr(dpld);

	if (unlikely(ld->mode == LINK_MODE_OFFLINE))
		return IRQ_HANDLED;

	dpram_intr_handler(dpld, int2ap);

	return IRQ_HANDLED;
}

static irqreturn_t cp_idpram_irq_handler(int irq, void *data)
{
	struct dpram_link_device *dpld = (struct dpram_link_device *)data;
	struct link_device *ld = (struct link_device *)&dpld->ld;
	u16 int2ap;

	if (unlikely(ld->mode == LINK_MODE_OFFLINE))
		return IRQ_HANDLED;

	if (dpram_wake_up(dpld) < 0) {
		trigger_force_cp_crash(dpld);
		return IRQ_HANDLED;
	}

	int2ap = recv_intr(dpld);

	dpram_intr_handler(dpld, int2ap);

	clear_intr(dpld);

	dpram_allow_sleep(dpld);

	return IRQ_HANDLED;
}

static irqreturn_t ext_dpram_irq_handler(int irq, void *data)
{
	struct dpram_link_device *dpld = (struct dpram_link_device *)data;
	struct link_device *ld = (struct link_device *)&dpld->ld;
	u16 int2ap = recv_intr(dpld);

	if (unlikely(ld->mode == LINK_MODE_OFFLINE))
		return IRQ_HANDLED;

	dpram_intr_handler(dpld, int2ap);

	return IRQ_HANDLED;
}

static void dpram_send_ipc(struct link_device *ld, int dev,
			struct io_device *iod, struct sk_buff *skb)
{
	struct dpram_link_device *dpld = to_dpram_link_device(ld);
	struct sk_buff_head *txq = ld->skb_txq[dev];
	int ret;
	u16 mask;

	if (unlikely(txq->qlen >= MAX_SKB_TXQ_DEPTH)) {
		mif_err("%s: %s txq->qlen %d >= %d\n", ld->name,
			get_dev_name(dev), txq->qlen, MAX_SKB_TXQ_DEPTH);
		if (iod->io_typ == IODEV_NET || iod->format == IPC_MULTI_RAW) {
			dev_kfree_skb_any(skb);
			return;
		}
	}

	skb_queue_tail(txq, skb);

	if (dpram_wake_up(dpld) < 0) {
		trigger_force_cp_crash(dpld);
		return;
	}

	if (!ipc_active(dpld)) {
		mif_info("%s: IPC is NOT active\n", ld->name);
		goto exit;
	}

	if (atomic_read(&dpld->res_required[dev]) > 0) {
		mif_info("%s: %s_TXQ is full\n", ld->name, get_dev_name(dev));
		goto exit;
	}

	ret = dpram_ipc_tx(dpld, dev);
	if (ret > 0) {
		mask = get_mask_send(dpld, dev);
		send_intr(dpld, INT_NON_CMD(mask));
	} else if (ret == -ENOSPC) {
		/*
		** dpld->res_required[dev] is set in dpram_ipc_tx()
		*/
		if (dev == IPC_RAW)
			mif_netif_stop(ld);
		queue_delayed_work(ld->tx_wq, ld->tx_dwork[dev], 0);
	} else {
		mif_info("%s: dpram_ipc_tx fail (err %d)\n", ld->name, ret);
	}

exit:
	dpram_allow_sleep(dpld);
}

static int dpram_send_cp_binary(struct link_device *ld, struct sk_buff *skb)
{
	struct dpram_link_device *dpld = to_dpram_link_device(ld);

	if (dpld->ext_op && dpld->ext_op->download_binary)
		return dpld->ext_op->download_binary(dpld, skb);
	else
		return -ENODEV;
}

static int dpram_send(struct link_device *ld, struct io_device *iod,
		struct sk_buff *skb)
{
	enum dev_format dev = iod->format;
	int len = skb->len;

	switch (dev) {
	case IPC_FMT:
	case IPC_RAW:
	case IPC_RFS:
		if (likely(ld->mode == LINK_MODE_IPC)) {
			dpram_send_ipc(ld, dev, iod, skb);
		} else {
			mif_info("%s: ld->mode != LINK_MODE_IPC\n", ld->name);
			dev_kfree_skb_any(skb);
		}
		return len;

	case IPC_BOOT:
		return dpram_send_cp_binary(ld, skb);

	default:
		mif_info("%s: ERR! no TXQ for %s\n", ld->name, iod->name);
		dev_kfree_skb_any(skb);
		return -ENODEV;
	}
}

static int dpram_force_dump(struct link_device *ld, struct io_device *iod)
{
	struct dpram_link_device *dpld = to_dpram_link_device(ld);
	trigger_force_cp_crash(dpld);
	return 0;
}

static int dpram_dump_start(struct link_device *ld, struct io_device *iod)
{
	struct dpram_link_device *dpld = to_dpram_link_device(ld);

	if (dpld->ext_op && dpld->ext_op->dump_start)
		return dpld->ext_op->dump_start(dpld);
	else
		return -ENODEV;
}

static int dpram_dump_update(struct link_device *ld, struct io_device *iod,
		unsigned long arg)
{
	struct dpram_link_device *dpld = to_dpram_link_device(ld);

	if (dpld->ext_op && dpld->ext_op->dump_update)
		return dpld->ext_op->dump_update(dpld, (void *)arg);
	else
		return -ENODEV;
}

static int dpram_ioctl(struct link_device *ld, struct io_device *iod,
		unsigned int cmd, unsigned long arg)
{
	struct dpram_link_device *dpld = to_dpram_link_device(ld);
	int err = 0;

	mif_info("%s: cmd 0x%08X\n", ld->name, cmd);

	switch (cmd) {
	case IOCTL_DPRAM_INIT_STATUS:
		mif_debug("%s: get dpram init status\n", ld->name);
		return dpld->init_status;

	default:
		if (dpld->ext_ioctl) {
			err = dpld->ext_ioctl(dpld, iod, cmd, arg);
		} else {
			mif_err("%s: ERR! invalid cmd 0x%08X\n", ld->name, cmd);
			err = -EINVAL;
		}

		break;
	}

	return err;
}

static void dpram_dump_memory(struct link_device *ld, char *buff)
{
	struct dpram_link_device *dpld = to_dpram_link_device(ld);
	dpram_wake_up(dpld);
	memcpy(buff, dpld->base, dpld->size);
	dpram_allow_sleep(dpld);
}

static void dpram_remap_std_16k_region(struct dpram_link_device *dpld)
{
	struct dpram_ipc_16k_map *dpram_map;
	struct dpram_ipc_device *dev;

	dpram_map = (struct dpram_ipc_16k_map *)dpld->base;

	/* magic code and access enable fields */
	dpld->ipc_map.magic = (u16 __iomem *)&dpram_map->magic;
	dpld->ipc_map.access = (u16 __iomem *)&dpram_map->access;

	/* FMT */
	dev = &dpld->ipc_map.dev[IPC_FMT];

	strcpy(dev->name, "FMT");
	dev->id = IPC_FMT;

	dev->txq.head = (u16 __iomem *)&dpram_map->fmt_tx_head;
	dev->txq.tail = (u16 __iomem *)&dpram_map->fmt_tx_tail;
	dev->txq.buff = (u8 __iomem *)&dpram_map->fmt_tx_buff[0];
	dev->txq.size = DP_16K_FMT_TX_BUFF_SZ;

	dev->rxq.head = (u16 __iomem *)&dpram_map->fmt_rx_head;
	dev->rxq.tail = (u16 __iomem *)&dpram_map->fmt_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&dpram_map->fmt_rx_buff[0];
	dev->rxq.size = DP_16K_FMT_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_F;
	dev->mask_res_ack = INT_MASK_RES_ACK_F;
	dev->mask_send    = INT_MASK_SEND_F;

	/* RAW */
	dev = &dpld->ipc_map.dev[IPC_RAW];

	strcpy(dev->name, "RAW");
	dev->id = IPC_RAW;

	dev->txq.head = (u16 __iomem *)&dpram_map->raw_tx_head;
	dev->txq.tail = (u16 __iomem *)&dpram_map->raw_tx_tail;
	dev->txq.buff = (u8 __iomem *)&dpram_map->raw_tx_buff[0];
	dev->txq.size = DP_16K_RAW_TX_BUFF_SZ;

	dev->rxq.head = (u16 __iomem *)&dpram_map->raw_rx_head;
	dev->rxq.tail = (u16 __iomem *)&dpram_map->raw_rx_tail;
	dev->rxq.buff = (u8 __iomem *)&dpram_map->raw_rx_buff[0];
	dev->rxq.size = DP_16K_RAW_RX_BUFF_SZ;

	dev->mask_req_ack = INT_MASK_REQ_ACK_R;
	dev->mask_res_ack = INT_MASK_RES_ACK_R;
	dev->mask_send    = INT_MASK_SEND_R;

	/* interrupt ports */
	dpld->ipc_map.mbx_cp2ap = (u16 __iomem *)&dpram_map->mbx_cp2ap;
	dpld->ipc_map.mbx_ap2cp = (u16 __iomem *)&dpram_map->mbx_ap2cp;
}

static int dpram_table_init(struct dpram_link_device *dpld)
{
	struct link_device *ld = &dpld->ld;
	u8 __iomem *dp_base;
	int i;

	if (!dpld->base) {
		mif_info("%s: ERR! dpld->base == NULL\n", ld->name);
		return -EINVAL;
	}
	dp_base = dpld->base;

	/* Map for booting */
	if (dpld->ext_op && dpld->ext_op->init_boot_map) {
		dpld->ext_op->init_boot_map(dpld);
	} else {
		dpld->bt_map.magic = (u32 *)(dp_base);
		dpld->bt_map.buff = (u8 *)(dp_base + DP_BOOT_BUFF_OFFSET);
		dpld->bt_map.size = dpld->size - 8;
	}

	/* Map for download (FOTA, UDL, etc.) */
	if (dpld->ext_op && dpld->ext_op->init_dl_map) {
		dpld->ext_op->init_dl_map(dpld);
	} else {
		dpld->dl_map.magic = (u32 *)(dp_base);
		dpld->dl_map.buff = (u8 *)(dp_base + DP_DLOAD_BUFF_OFFSET);
	}

	/* Map for upload mode */
	if (dpld->ext_op && dpld->ext_op->init_ul_map) {
		dpld->ext_op->init_ul_map(dpld);
	} else {
		dpld->ul_map.magic = (u32 *)(dp_base);
		dpld->ul_map.buff = (u8 *)(dp_base + DP_ULOAD_BUFF_OFFSET);
	}

	/* Map for IPC */
	if (dpld->ext_op && dpld->ext_op->init_ipc_map) {
		dpld->ext_op->init_ipc_map(dpld);
	} else if (dpld->dpctl->ipc_map) {
		memcpy(&dpld->ipc_map, dpld->dpctl->ipc_map,
			sizeof(struct dpram_ipc_map));
	} else {
		if (dpld->size == DPRAM_SIZE_16KB)
			dpram_remap_std_16k_region(dpld);
		else
			return -EINVAL;
	}

	dpld->magic = dpld->ipc_map.magic;
	dpld->access = dpld->ipc_map.access;
	for (i = 0; i < ld->max_ipc_dev; i++)
		dpld->dev[i] = &dpld->ipc_map.dev[i];
	dpld->mbx2ap = dpld->ipc_map.mbx_cp2ap;
	dpld->mbx2cp = dpld->ipc_map.mbx_ap2cp;

	return 0;
}

static void dpram_setup_common_op(struct dpram_link_device *dpld)
{
	dpld->recv_intr = recv_intr;
	dpld->send_intr = send_intr;
	dpld->get_magic = get_magic;
	dpld->set_magic = set_magic;
	dpld->get_access = get_access;
	dpld->set_access = set_access;
	dpld->get_tx_head = get_tx_head;
	dpld->get_tx_tail = get_tx_tail;
	dpld->set_tx_head = set_tx_head;
	dpld->set_tx_tail = set_tx_tail;
	dpld->get_tx_buff = get_tx_buff;
	dpld->get_tx_buff_size = get_tx_buff_size;
	dpld->get_rx_head = get_rx_head;
	dpld->get_rx_tail = get_rx_tail;
	dpld->set_rx_head = set_rx_head;
	dpld->set_rx_tail = set_rx_tail;
	dpld->get_rx_buff = get_rx_buff;
	dpld->get_rx_buff_size = get_rx_buff_size;
	dpld->get_mask_req_ack = get_mask_req_ack;
	dpld->get_mask_res_ack = get_mask_res_ack;
	dpld->get_mask_send = get_mask_send;
	dpld->ipc_rx_handler = dpram_intr_handler;
}

static int dpram_link_init(struct link_device *ld, struct io_device *iod)
{
	return 0;
}

static void dpram_link_terminate(struct link_device *ld, struct io_device *iod)
{
	if (iod->format == IPC_FMT && ld->mode == LINK_MODE_IPC) {
		if (!atomic_read(&iod->opened)) {
			ld->mode = LINK_MODE_OFFLINE;
			mif_err("%s: %s: link mode is changed: IPC->OFFLINE\n",
				iod->name, ld->name);
		}
	}

	return;
}

struct link_device *dpram_create_link_device(struct platform_device *pdev)
{
	struct dpram_link_device *dpld = NULL;
	struct link_device *ld = NULL;
	struct modem_data *modem = NULL;
	struct modemlink_dpram_control *dpctl = NULL;
	struct resource *res = NULL;
	resource_size_t res_size;
	unsigned long task_data;
	int ret = 0;
	int i = 0;
	int bsize;
	int qsize;

	/*
	** Alloc an instance of the DPRAM link device structure
	*/
	dpld = kzalloc(sizeof(struct dpram_link_device), GFP_KERNEL);
	if (!dpld) {
		mif_err("ERR! kzalloc dpld fail\n");
		goto err;
	}
	ld = &dpld->ld;

	/*
	** Get the modem (platform) data
	*/
	modem = (struct modem_data *)pdev->dev.platform_data;
	if (!modem) {
		mif_err("ERR! modem == NULL\n");
		goto err;
	}
	mif_info("modem = %s\n", modem->name);
	mif_info("link device = %s\n", modem->link_name);

	/*
	** Retrieve modem data and DPRAM control data from the modem data
	*/
	ld->mdm_data = modem;
	ld->name = modem->link_name;
	ld->ipc_version = modem->ipc_version;

	if (!modem->dpram_ctl) {
		mif_err("ERR! modem->dpram_ctl == NULL\n");
		goto err;
	}
	dpctl = modem->dpram_ctl;

	dpld->dpctl = dpctl;
	dpld->type = dpctl->dp_type;

	if (ld->ipc_version < SIPC_VER_50) {
		if (!dpctl->max_ipc_dev) {
			mif_err("%s: ERR! no max_ipc_dev\n", ld->name);
			goto err;
		}

		ld->aligned = dpctl->aligned;
		ld->max_ipc_dev = dpctl->max_ipc_dev;
	} else {
		ld->aligned = 1;
		ld->max_ipc_dev = MAX_SIPC5_DEV;
	}

	/*
	** Set attributes as a link device
	*/
	ld->init_comm = dpram_link_init;
	ld->terminate_comm = dpram_link_terminate;
	ld->send = dpram_send;
	ld->force_dump = dpram_force_dump;
	ld->dump_start = dpram_dump_start;
	ld->dump_update = dpram_dump_update;
	ld->ioctl = dpram_ioctl;

	INIT_LIST_HEAD(&ld->list);

	skb_queue_head_init(&ld->sk_fmt_tx_q);
	skb_queue_head_init(&ld->sk_raw_tx_q);
	skb_queue_head_init(&ld->sk_rfs_tx_q);
	ld->skb_txq[IPC_FMT] = &ld->sk_fmt_tx_q;
	ld->skb_txq[IPC_RAW] = &ld->sk_raw_tx_q;
	ld->skb_txq[IPC_RFS] = &ld->sk_rfs_tx_q;

	/*
	** Set up function pointers
	*/
	dpram_setup_common_op(dpld);
	dpld->dpram_dump = dpram_dump_memory;
	dpld->ext_op = dpram_get_ext_op(modem->modem_type);
	if (dpld->ext_op && dpld->ext_op->ioctl)
		dpld->ext_ioctl = dpld->ext_op->ioctl;
	if (dpld->ext_op && dpld->ext_op->wakeup && dpld->ext_op->sleep)
		dpld->need_wake_up = true;
	if (dpld->ext_op && dpld->ext_op->clear_intr)
		dpld->need_intr_clear = true;

	/*
	** Retrieve DPRAM resource
	*/
	if (!dpctl->dp_base) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						STR_DPRAM_BASE);
		if (!res) {
			mif_err("%s: ERR! no DPRAM resource\n", ld->name);
			goto err;
		}
		res_size = resource_size(res);

		dpctl->dp_base = ioremap_nocache(res->start, res_size);
		if (!dpctl->dp_base) {
			mif_err("%s: ERR! ioremap_nocache for BASE fail\n",
				ld->name);
			goto err;
		}
		dpctl->dp_size = res_size;
	}
	dpld->base = dpctl->dp_base;
	dpld->size = dpctl->dp_size;

	mif_info("%s: type %d, aligned %d, base 0x%08X, size %d\n",
		ld->name, dpld->type, ld->aligned, (int)dpld->base, dpld->size);

	/*
	** Retrieve DPRAM SFR resource if exists
	*/
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					STR_DPRAM_SFR_BASE);
	if (res) {
		res_size = resource_size(res);
		dpld->sfr_base = ioremap_nocache(res->start, res_size);
		if (!dpld->sfr_base) {
			mif_err("%s: ERR! ioremap_nocache for SFR fail\n",
				ld->name);
			goto err;
		}
	}

	/* Initialize DPRAM map (physical map -> logical map) */
	ret = dpram_table_init(dpld);
	if (ret < 0) {
		mif_err("%s: ERR! dpram_table_init fail (err %d)\n",
			ld->name, ret);
		goto err;
	}

	/* Disable IPC */
	set_magic(dpld, 0);
	set_access(dpld, 0);
	dpld->init_status = DPRAM_INIT_STATE_NONE;

	/* Initialize locks, completions, and bottom halves */
	snprintf(dpld->wlock_name, MIF_MAX_NAME_LEN, "%s_wlock", ld->name);
	wake_lock_init(&dpld->wlock, WAKE_LOCK_SUSPEND, dpld->wlock_name);

	init_completion(&dpld->dpram_init_cmd);
	init_completion(&dpld->modem_pif_init_done);
	init_completion(&dpld->udl_start_complete);
	init_completion(&dpld->udl_cmd_complete);
	init_completion(&dpld->crash_start_complete);
	init_completion(&dpld->crash_recv_done);
	for (i = 0; i < ld->max_ipc_dev; i++)
		init_completion(&dpld->res_ack_cmpl[i]);

	task_data = (unsigned long)dpld;
	tasklet_init(&dpld->rx_tsk, dpram_ipc_rx_task, task_data);

	ld->tx_wq = create_singlethread_workqueue("dpram_tx_wq");
	if (!ld->tx_wq) {
		mif_err("%s: ERR! fail to create tx_wq\n", ld->name);
		goto err;
	}
	INIT_DELAYED_WORK(&ld->fmt_tx_dwork, dpram_fmt_tx_work);
	INIT_DELAYED_WORK(&ld->raw_tx_dwork, dpram_raw_tx_work);
	INIT_DELAYED_WORK(&ld->rfs_tx_dwork, dpram_rfs_tx_work);
	ld->tx_dwork[IPC_FMT] = &ld->fmt_tx_dwork;
	ld->tx_dwork[IPC_RAW] = &ld->raw_tx_dwork;
	ld->tx_dwork[IPC_RFS] = &ld->rfs_tx_dwork;

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	INIT_DELAYED_WORK(&dpld->dump_dwork, save_dpram_dump_work);
	INIT_DELAYED_WORK(&dpld->trace_dwork, save_ipc_trace_work);
	spin_lock_init(&dpld->dump_list.lock);
	spin_lock_init(&dpld->trace_list.lock);
#endif

	/* Prepare RXB queue */
	qsize = DPRAM_MAX_RXBQ_SIZE;
	for (i = 0; i < ld->max_ipc_dev; i++) {
		bsize = rxbq_get_page_size(get_rx_buff_size(dpld, i));
		dpld->rxbq[i].size = qsize;
		dpld->rxbq[i].in = 0;
		dpld->rxbq[i].out = 0;
		dpld->rxbq[i].rxb = rxbq_create_pool(bsize, qsize);
		if (!dpld->rxbq[i].rxb) {
			mif_err("%s: ERR! %s rxbq_create_pool fail\n",
				ld->name, get_dev_name(i));
			goto err;
		}
		mif_info("%s: %s rxbq_pool created (bsize:%d, qsize:%d)\n",
			ld->name, get_dev_name(i), bsize, qsize);
	}

	/* Prepare a multi-purpose miscellaneous buffer */
	dpld->buff = kzalloc(dpld->size, GFP_KERNEL);
	if (!dpld->buff) {
		mif_err("%s: ERR! kzalloc dpld->buff fail\n", ld->name);
		goto err;
	}

	/*
	** Retrieve DPRAM IRQ GPIO#, IRQ#, and IRQ flags
	*/
	dpld->gpio_dpram_int = modem->gpio_dpram_int;

	if (dpctl->dpram_irq) {
		dpld->irq = dpctl->dpram_irq;
	} else {
		dpld->irq = platform_get_irq_byname(pdev, STR_DPRAM_IRQ);
		if (dpld->irq < 0) {
			mif_err("%s: ERR! no DPRAM IRQ resource\n", ld->name);
			goto err;
		}
	}

	if (dpctl->dpram_irq_flags)
		dpld->irq_flags = dpctl->dpram_irq_flags;
	else
		dpld->irq_flags = (IRQF_NO_SUSPEND | IRQF_TRIGGER_LOW);

	/*
	** Register DPRAM interrupt handler
	*/
	snprintf(dpld->irq_name, MIF_MAX_NAME_LEN, "%s_irq", ld->name);
	if (dpld->ext_op && dpld->ext_op->irq_handler)
		dpld->irq_handler = dpld->ext_op->irq_handler;
	else if (dpld->type == CP_IDPRAM)
		dpld->irq_handler = cp_idpram_irq_handler;
	else if (dpld->type == AP_IDPRAM)
		dpld->irq_handler = ap_idpram_irq_handler;
	else
		dpld->irq_handler = ext_dpram_irq_handler;

	ret = register_isr(dpld->irq, dpld->irq_handler, dpld->irq_flags,
				dpld->irq_name, dpld);
	if (ret)
		goto err;

	return ld;

err:
	if (dpld) {
		if (dpld->buff)
			kfree(dpld->buff);
		kfree(dpld);
	}

	return NULL;
}

