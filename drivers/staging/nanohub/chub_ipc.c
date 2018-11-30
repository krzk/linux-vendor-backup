/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "chub_ipc.h"

#if defined(CHUB_IPC)
#if defined(SEOS)
#include <seos.h>
#include <errno.h>
#elif defined(EMBOS)
#include <Device.h>
#endif
#include <csp_common.h>
#include <csp_printf.h>
#include <mailboxDrv.h>
#include <string.h>
#elif defined(AP_IPC)
#include <linux/delay.h>
#include <linux/io.h>
#include "chub.h"
#endif

/* ap-chub ipc */
struct ipc_area ipc_addr[IPC_REG_MAX];

struct ipc_owner_ctrl {
	enum ipc_direction src;
	void *base;
} ipc_own[IPC_OWN_MAX];

struct ipc_map_area *ipc_map;
struct ipc_shared_area *ipc_shared_map;

#ifdef PACKET_LOW_DEBUG
#define GET_IPC_REG_STRING(a) (((a) == IPC_REG_IPC_C2A) ? "wt" : "rd")

static char *get_cs_name(enum channel_status cs)
{
	switch (cs) {
	case CS_IDLE:
		return "I";
	case CS_AP_WRITE:
		return "AW";
	case CS_CHUB_RECV:
		return "CR";
	case CS_CHUB_WRITE:
		return "CW";
	case CS_AP_RECV:
		return "AR";
	case CS_MAX:
		break;
	};
	return NULL;
}

void content_disassemble(struct ipc_content *content, enum ipc_region act)
{
	CSP_PRINTF_INFO("[content-%s-%d: status:%s: buf: 0x%x, size: %d]\n",
			GET_IPC_REG_STRING(act), content->num,
			get_cs_name(content->status),
			(unsigned int)content->buf, content->size);
}
#endif

/* ipc address control functions */
void ipc_set_base(void *addr)
{
	ipc_addr[IPC_REG_BL].base = addr;
}

inline void *ipc_get_base(enum ipc_region area)
{
	return ipc_addr[area].base;
}

inline u32 ipc_get_offset(enum ipc_region area)
{
	return ipc_addr[area].offset;
}

inline void *ipc_get_addr(enum ipc_region area, int buf_num)
{
#ifdef CHUB_IPC
	return (void *)((unsigned int)ipc_addr[area].base +
			ipc_addr[area].offset * buf_num);
#else
	return ipc_addr[area].base + ipc_addr[area].offset * buf_num;
#endif
}

u32 ipc_get_chub_mem_size(void)
{
	return ipc_addr[IPC_REG_DUMP].offset;
}

void ipc_set_chub_clk(u32 clk)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->chubclk = clk;
}

u32 ipc_get_chub_clk(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->chubclk;
}

void ipc_set_chub_bootmode(u32 bootmode)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	map->bootmode = bootmode;

#if defined(DEBUG) && defined(CHUB_IPC)
	/* update sleep count */
	ipc_set_val(IPC_EVT_C2A, ipc_get_val(IPC_EVT_C2A) + 1);
#endif
}

u32 ipc_get_chub_bootmode(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return map->bootmode;
}

#if defined(LOCAL_POWERGATE)
u32 *ipc_get_chub_psp(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return &(map->psp);
}

u32 *ipc_get_chub_msp(void)
{
	struct chub_bootargs *map = ipc_get_base(IPC_REG_BL_MAP);

	return &(map->msp);
}
#endif

void *ipc_get_chub_map(void)
{
	char *sram_base = ipc_get_base(IPC_REG_BL);
	struct chub_bootargs *map = (struct chub_bootargs *)(sram_base + MAP_INFO_OFFSET);

	if (strncmp(OS_UPDT_MAGIC, map->magic, sizeof(OS_UPDT_MAGIC))) {
		CSP_PRINTF_ERROR("%s: %p has wrong magic key: %s -> %s\n",
				 __func__, map, OS_UPDT_MAGIC, map->magic);
		return 0;
	}

	if (map->ipc_version != IPC_VERSION) {
		CSP_PRINTF_ERROR
		    ("%s: ipc_version doesn't match: AP %d, Chub: %d\n",
		     __func__, IPC_VERSION, map->ipc_version);
		return 0;
	}

	ipc_addr[IPC_REG_BL_MAP].base = map;

	ipc_addr[IPC_REG_OS].base = sram_base + map->code_start;
	ipc_addr[IPC_REG_SHARED].base = sram_base + map->shared_start;
	ipc_shared_map = ipc_addr[IPC_REG_SHARED].base;
	ipc_addr[IPC_REG_SHARED_A2C].base = &ipc_shared_map->a2c;
	ipc_addr[IPC_REG_SHARED_A2C].offset = sizeof(ipc_shared_map->a2c);
	ipc_addr[IPC_REG_SHARED_C2A].base = &ipc_shared_map->c2a;
	ipc_addr[IPC_REG_SHARED_C2A].offset = sizeof(ipc_shared_map->c2a);

	ipc_addr[IPC_REG_IPC].base = sram_base + map->ipc_start;
	ipc_addr[IPC_REG_RAM].base = sram_base + map->ram_start;
	ipc_addr[IPC_REG_DUMP].base = sram_base + map->dump_start;

	ipc_addr[IPC_REG_BL].offset = map->bl_end - map->bl_start;
	ipc_addr[IPC_REG_OS].offset = map->code_end - map->code_start;
	ipc_addr[IPC_REG_SHARED].offset = map->shared_end - map->shared_start;
	ipc_addr[IPC_REG_IPC].offset = map->ipc_end - map->ipc_start;
	ipc_addr[IPC_REG_RAM].offset = map->ram_end - map->ram_start;
	ipc_addr[IPC_REG_DUMP].offset = map->dump_end - map->dump_start;

	ipc_map = ipc_addr[IPC_REG_IPC].base;
	ipc_map->logbuf.size =
	    ipc_addr[IPC_REG_IPC].offset - sizeof(struct ipc_map_area);

	ipc_addr[IPC_REG_IPC_EVT_A2C].base = &ipc_map->evt[IPC_EVT_A2C].data;
	ipc_addr[IPC_REG_IPC_EVT_A2C].offset = 0;
	ipc_addr[IPC_REG_IPC_EVT_A2C_CTRL].base =
	    &ipc_map->evt[IPC_EVT_A2C].ctrl;
	ipc_addr[IPC_REG_IPC_EVT_A2C_CTRL].offset = 0;
	ipc_addr[IPC_REG_IPC_EVT_C2A].base = &ipc_map->evt[IPC_EVT_C2A].data;
	ipc_addr[IPC_REG_IPC_EVT_C2A].offset = 0;
	ipc_addr[IPC_REG_IPC_EVT_C2A_CTRL].base =
	    &ipc_map->evt[IPC_EVT_C2A].ctrl;
	ipc_addr[IPC_REG_IPC_EVT_C2A_CTRL].offset = 0;
	ipc_addr[IPC_REG_IPC_C2A].base = &ipc_map->data[IPC_DATA_C2A];
	ipc_addr[IPC_REG_IPC_C2A].offset = sizeof(struct ipc_content);
	ipc_addr[IPC_REG_IPC_A2C].base = &ipc_map->data[IPC_DATA_A2C];
	ipc_addr[IPC_REG_IPC_A2C].offset = sizeof(struct ipc_content);
	ipc_addr[IPC_REG_LOG].base = &ipc_map->logbuf.buf;
	ipc_addr[IPC_REG_LOG].offset =
	    ipc_addr[IPC_REG_IPC].offset - sizeof(struct ipc_map_area);

#ifdef CHUB_IPC
	ipc_map->logbuf.token = 0;
	memset(ipc_addr[IPC_REG_LOG].base, 0, ipc_addr[IPC_REG_LOG].offset);
#endif
	CSP_PRINTF_INFO
	    ("contexthub map information(v%u)\n\t powermode(%s) \n\t bl(%p %d)\n\t os(%p %d)\n\t ipc(%p %d)\n\t ram(%p %d)\n\t shared(%p %d)\n\t dump(%p %d)\n",
	     map->ipc_version,
	     map->bootmode ? "enable" : "disable",
	     ipc_addr[IPC_REG_BL].base, ipc_addr[IPC_REG_BL].offset,
	     ipc_addr[IPC_REG_OS].base, ipc_addr[IPC_REG_OS].offset,
	     ipc_addr[IPC_REG_IPC].base, ipc_addr[IPC_REG_IPC].offset,
	     ipc_addr[IPC_REG_RAM].base, ipc_addr[IPC_REG_RAM].offset,
	     ipc_addr[IPC_REG_SHARED].base, ipc_addr[IPC_REG_SHARED].offset,
	     ipc_addr[IPC_REG_DUMP].base, ipc_addr[IPC_REG_DUMP].offset);

	CSP_PRINTF_INFO
		("contexthub shared mem information: share base(%p %d)\n\t a2c(%p %d)\n\t c2a(%p %d)\n",
		ipc_addr[IPC_REG_SHARED].base, ipc_addr[IPC_REG_SHARED].offset,
		ipc_addr[IPC_REG_SHARED_A2C].base, ipc_addr[IPC_REG_SHARED_A2C].offset,
		ipc_addr[IPC_REG_SHARED_C2A].base, ipc_addr[IPC_REG_SHARED_C2A].offset);

	if ((ipc_addr[IPC_REG_SHARED].base != ipc_get_base(IPC_REG_SHARED)) ||
		(ipc_addr[IPC_REG_IPC].base != ipc_get_base(IPC_REG_IPC)))
		CSP_PRINTF_ERROR("%s fails\n", __func__);

	return ipc_map;
}

void ipc_dump(void)
{
	CSP_PRINTF_INFO("%s: a2x event\n", __func__);
	ipc_print_evt(IPC_EVT_A2C);
	CSP_PRINTF_INFO("%s: c2a event\n", __func__);
	ipc_print_evt(IPC_EVT_C2A);
	CSP_PRINTF_INFO("%s: active channel\n", __func__);
	ipc_print_channel();
}

/* ipc channel functions */
#define GET_IPC_REG_NAME(c) (((c) == CS_WRITE) ? "W" : (((c) == CS_RECV) ? "R" : "I"))
#define GET_CH_NAME(c) (((c) == CS_AP) ? "A" : "C")
#define GET_CH_OWNER(o) (((o) == IPC_DATA_C2A) ? "C2A" : "A2C")

inline void ipc_update_channel_status(struct ipc_content *content,
				      enum channel_status next)
{
#ifdef PACKET_LOW_DEBUG
	unsigned int org = __raw_readl(&content->status);

	CSP_PRINTF_INFO("CH(%s)%d: %s->%s\n", GET_CH_NAME(org >> CS_OWN_OFFSET),
			content->num, GET_IPC_REG_NAME((org & CS_IPC_REG_CMP)),
			GET_IPC_REG_NAME((next & CS_IPC_REG_CMP)));
#endif

	__raw_writel(next, &content->status);
}

void *ipc_scan_channel(enum ipc_region area, enum channel_status target)
{
	int i;
	struct ipc_content *content = ipc_get_base(area);

	for (i = 0; i < IPC_BUF_NUM; i++, content++)
		if (__raw_readl(&content->status) == target)
			return content;

	return NULL;
}

void *ipc_get_channel(enum ipc_region area, enum channel_status target,
		      enum channel_status next)
{
	int i;
	struct ipc_content *content = ipc_get_base(area);

	for (i = 0; i < IPC_BUF_NUM; i++, content++) {
		if (__raw_readl(&content->status) == target) {
			ipc_update_channel_status(content, next);
			return content;
		}
	}

	return NULL;
}

void ipc_print_channel(void)
{
	int i, j, org;

	for (j = 0; j < IPC_DATA_MAX; j++) {
		for (i = 0; i < IPC_BUF_NUM; i++) {
			org = ipc_map->data[j][i].status;
			if (org & CS_IPC_REG_CMP)
				CSP_PRINTF_INFO("CH-%s:%x\n",
						GET_CH_OWNER(j), org);
		}
	}
}

void ipc_init(void)
{
	int i, j;

	if (!ipc_map)
		CSP_PRINTF_ERROR("%s: ipc_map is NULL.\n", __func__);

	for (i = 0; i < IPC_BUF_NUM; i++) {
		ipc_map->data[IPC_DATA_C2A][i].num = i;
		ipc_map->data[IPC_DATA_C2A][i].status = CS_CHUB_OWN;
		ipc_map->data[IPC_DATA_A2C][i].num = i;
		ipc_map->data[IPC_DATA_A2C][i].status = CS_AP_OWN;
	}

	ipc_hw_clear_all_int_pend_reg(AP, 0xff);

	for (j = 0; j < IPC_EVT_MAX; j++) {
		ipc_map->evt[j].ctrl.dq = 0;
		ipc_map->evt[j].ctrl.eq = 0;
		ipc_map->evt[j].ctrl.full = 0;
		ipc_map->evt[j].ctrl.empty = 0;
		ipc_map->evt[j].ctrl.irq = 0;

		for (i = 0; i < IPC_EVT_NUM; i++) {
			ipc_map->evt[j].data[i].evt = IRQ_EVT_INVAL;
			ipc_map->evt[j].data[i].irq = IRQ_EVT_INVAL;
		}
	}
}

/* evt functions */
enum {
	IPC_EVT_DQ,		/* empty */
	IPC_EVT_EQ,		/* fill */
};

#define EVT_Q_INT(i) (((i) == IPC_EVT_NUM) ? 0 : (i))
#define IRQ_EVT_IDX_INT(i) (((i) == IRQ_EVT_END) ? IRQ_EVT_START : (i))
#define IRQ_C2A_WT_IDX_INT(i) (((i) == IRQ_C2A_END) ? IRQ_C2A_START : (i))

#define EVT_Q_DEC(i) (((i) == -1) ? IPC_EVT_NUM - 1 : (i - 1))

struct ipc_evt_buf *ipc_get_evt(enum ipc_evt_list evtq)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
	struct ipc_evt_buf *cur_evt = NULL;

	if (ipc_evt->ctrl.dq != __raw_readl(&ipc_evt->ctrl.eq)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.dq];
		cur_evt->status = IPC_EVT_DQ;
		ipc_evt->ctrl.dq = EVT_Q_INT(ipc_evt->ctrl.dq + 1);
		if (ipc_evt->ctrl.dq == __raw_readl(&ipc_evt->ctrl.eq))
			__raw_writel(1, &ipc_evt->ctrl.empty);
	} else if (__raw_readl(&ipc_evt->ctrl.full)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.dq];
		cur_evt->status = IPC_EVT_DQ;
		ipc_evt->ctrl.dq = EVT_Q_INT(ipc_evt->ctrl.dq + 1);
		__raw_writel(0, &ipc_evt->ctrl.full);
	}

	return cur_evt;
}

#define EVT_WAIT_TIME (10)
#define MAX_TRY_CNT (5)

int ipc_add_evt(enum ipc_evt_list evtq, enum irq_evt_chub evt)
{
	enum ipc_owner owner = (evtq < IPC_EVT_AP_MAX) ? AP : IPC_OWN_MAX;
	struct ipc_evt_buf *cur_evt = NULL;
	struct ipc_evt *ipc_evt;

	/* generate irq directly without event queue */
	if (evt >= IRQ_EVT_CHUB_EVT_MAX) {
		ipc_hw_gen_interrupt(owner, evt);
		return 0;
	}

	ipc_evt = &ipc_map->evt[evtq];
	if (!ipc_evt) {
		CSP_PRINTF_ERROR("%s: invalid ipc_evt\n", __func__);
		return -1;
	}

	if (!__raw_readl(&ipc_evt->ctrl.full)) {
		cur_evt = &ipc_evt->data[ipc_evt->ctrl.eq];
		if (!cur_evt) {
			CSP_PRINTF_ERROR("%s: invalid cur_evt\n", __func__);
			return -1;
		}

		cur_evt->evt = evt;
		cur_evt->status = IPC_EVT_EQ;
		cur_evt->irq = ipc_evt->ctrl.irq;

		ipc_evt->ctrl.eq = EVT_Q_INT(ipc_evt->ctrl.eq + 1);
		ipc_evt->ctrl.irq = IRQ_EVT_IDX_INT(ipc_evt->ctrl.irq + 1);

		if (__raw_readl(&ipc_evt->ctrl.empty))
			__raw_writel(0, &ipc_evt->ctrl.full);

		if (ipc_evt->ctrl.eq == __raw_readl(&ipc_evt->ctrl.dq))
			__raw_writel(1, &ipc_evt->ctrl.full);
	} else {
#if defined(CHUB_IPC)
		int trycnt = 0;

		do {
			trycnt++;
			msleep(EVT_WAIT_TIME);
		} while (ipc_evt->ctrl.full && (trycnt < MAX_TRY_CNT));

		if (!__raw_readl(&ipc_evt->ctrl.full)) {
			CSP_PRINTF_INFO("%s: evt %d during %d ms is full\n",
					__func__, evt, EVT_WAIT_TIME * trycnt);
			return -1;
		} else {
			CSP_PRINTF_ERROR("%s: fail to add evt\n", __func__);
			ipc_dump();
			return -1;
		}
#else
		CSP_PRINTF_ERROR("%s: fail to add evt\n", __func__);
		return -1;
#endif
	}

	if (owner != IPC_OWN_MAX) {
#if defined(AP_IPC)
	    ipc_write_val(AP, sched_clock());
#endif

	    if (cur_evt)
			ipc_hw_gen_interrupt(owner, cur_evt->irq);
		else
			return -1;
	}

	return 0;
}

void ipc_set_val(enum ipc_evt_list evtq, int val)
{
	__raw_writel(val, &ipc_map->val[evtq]);
}

int ipc_get_val(enum ipc_evt_list evtq)
{
	return __raw_readl(&ipc_map->val[evtq]);
}

#define IPC_GET_EVT_NAME(a) (((a) == IPC_EVT_A2C) ? "A2C" : "C2A")

void ipc_print_evt(enum ipc_evt_list evtq)
{
	struct ipc_evt *ipc_evt = &ipc_map->evt[evtq];
	int i;

	CSP_PRINTF_INFO("evt-%s: eq:%d dq:%d full:%d irq:%d\n",
			IPC_GET_EVT_NAME(evtq), ipc_evt->ctrl.eq,
			ipc_evt->ctrl.dq, ipc_evt->ctrl.full,
			ipc_evt->ctrl.irq);

	for (i = 0; i < IPC_EVT_NUM; i++) {
		CSP_PRINTF_INFO("evt%d(evt:%d,irq:%d,f:%d)\n",
				i, ipc_evt->data[i].evt,
				ipc_evt->data[i].irq, ipc_evt->data[i].status);
	}

	(void)ipc_evt;
}

u32 ipc_logbuf_get_token(void)
{
	__raw_writel(ipc_map->logbuf.token + 1, &ipc_map->logbuf.token);

	return __raw_readl(&ipc_map->logbuf.token);
}

/* contexthub use */
void ipc_logbuf_put_with_char(char ch)
{
	char *logbuf;
	int eqNext;

	if (ipc_map) {
		eqNext = ipc_map->logbuf.eq + 1;

#ifdef IPC_DEBUG
		if (eqNext == ipc_map->logbuf.dq) {
			ipc_write_debug_event(AP, IPC_DEBUG_CHUB_FULL_LOG);
			ipc_add_evt(IPC_EVT_C2A, IRQ_EVT_CHUB_TO_AP_DEBUG);
		}
#endif

		logbuf = ipc_map->logbuf.buf;

		*(logbuf + ipc_map->logbuf.eq) = ch;

		if (eqNext == ipc_map->logbuf.size)
			ipc_map->logbuf.eq = 0;
		else
			ipc_map->logbuf.eq = eqNext;
	}
}

void ipc_set_owner(enum ipc_owner owner, void *base, enum ipc_direction dir)
{
	ipc_own[owner].base = base;
	ipc_own[owner].src = dir;
}

int ipc_hw_read_int_start_index(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return IRQ_EVT_CHUB_MAX;
	else
		return 0;
}

unsigned int ipc_hw_read_gen_int_status_reg(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base +
				   REG_MAILBOX_INTSR1) & (1 << irq);
	else
		return __raw_readl((char *)ipc_own[owner].base +
				   REG_MAILBOX_INTSR0) & (1 << (irq +
								IRQ_EVT_CHUB_MAX));
}

void ipc_hw_write_shared_reg(enum ipc_owner owner, unsigned int val, int num)
{
	__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_ISSR0 + num * 4);
}

unsigned int ipc_hw_read_shared_reg(enum ipc_owner owner, int num)
{
	return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_ISSR0 + num * 4);
}

unsigned int ipc_hw_read_int_status_reg(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR0);
	else
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTSR1);
}

unsigned int ipc_hw_read_int_gen_reg(enum ipc_owner owner)
{
	if (ipc_own[owner].src)
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTGR0);
	else
		return __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTGR1);
}

void ipc_hw_clear_int_pend_reg(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTCR0);
	else
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTCR1);
}

void ipc_hw_clear_all_int_pend_reg(enum ipc_owner owner, unsigned int val)
{
	if (ipc_own[owner].src)
		__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_INTCR0);
	else
		__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_INTCR1);
}

void ipc_hw_gen_interrupt(enum ipc_owner owner, int irq)
{
	if (ipc_own[owner].src)
		__raw_writel(1 << irq,
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR1);
	else
		__raw_writel(1 << (irq + IRQ_EVT_CHUB_MAX),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTGR0);
}

void ipc_hw_set_mcuctrl(enum ipc_owner owner, unsigned int val)
{
	__raw_writel(val, (char *)ipc_own[owner].base + REG_MAILBOX_MCUCTL);
}

void ipc_hw_mask_irq(enum ipc_owner owner, int irq)
{
	int mask;

	if (ipc_own[owner].src) {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(mask | (1 << (irq + IRQ_EVT_CHUB_MAX)),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
	} else {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
		__raw_writel(mask | (1 << irq),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	}
}

void ipc_hw_unmask_irq(enum ipc_owner owner, int irq)
{
	int mask;

	if (ipc_own[owner].src) {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
		__raw_writel(mask & ~(1 << (irq + IRQ_EVT_CHUB_MAX)),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR0);
	} else {
		mask = __raw_readl((char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
		__raw_writel(mask & ~(1 << irq),
			     (char *)ipc_own[owner].base + REG_MAILBOX_INTMR1);
	}
}

void ipc_write_debug_event(enum ipc_owner owner, enum ipc_debug_event action)
{
	ipc_hw_write_shared_reg(owner, action, SR_DEBUG_ACTION);
}

u32 ipc_read_debug_event(enum ipc_owner owner)
{
	return ipc_hw_read_shared_reg(owner, SR_DEBUG_ACTION);
}

void ipc_write_val(enum ipc_owner owner, u64 val)
{
	u32 low = val & 0xffffffff;
	u32 high = val >> 32;

	ipc_hw_write_shared_reg(owner, low, SR_DEBUG_VAL_LOW);
	ipc_hw_write_shared_reg(owner, high, SR_DEBUG_VAL_HIGH);
}

u64 ipc_read_val(enum ipc_owner owner)
{
	u32 low = ipc_hw_read_shared_reg(owner, SR_DEBUG_VAL_LOW);
	u64 high = ipc_hw_read_shared_reg(owner, SR_DEBUG_VAL_HIGH);
	u64 val = low | (high << 32);

	return val;
}
