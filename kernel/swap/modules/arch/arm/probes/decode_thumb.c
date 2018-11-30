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
#include <linux/errno.h>
#include <linux/ptrace.h>
#include "decode_thumb.h"
#include "tramps_thumb.h"
#include "compat_arm64.h"


#define GET_BIT(x, n)		((x >> n) & 0x1)
#define GET_FIELD(x, s, l)      ((x >> s) & ((1 << l) - 1))
#define SET_FIELD(x, s, l, v)	({		\
	typeof(x) mask = (((1 << l) - 1) << s);	\
	(x & ~mask) | ((v << s) & mask);	\
})


typedef union thumb_insn {
	u32 val;
	struct {
		u16 hw1;
		u16 hw2;
	} __packed;
} thumb_insn_t;

typedef int (*decode_handler_t)(thumb_insn_t insn, struct decode_info *info);


static void make_def(void *tramp, u32 insn, u32 vaddr, bool t2)
{
	u32 ret_addr;
	u16 *tr = tramp;

	/*
	 * thumb  - +2
	 * thumb2 - +4
	 */
	ret_addr = vaddr + (2 << t2);
	tr[4] = insn & 0x0000ffff;
	if (t2)
		tr[5] = insn >> 16;

	tr[13] = RET_BREAK_THUMB;
	tr[16] = (ret_addr & 0x0000ffff) | 0x1;
	tr[17] = ret_addr >> 16;
}

static void tt_make_common(void *tramp, u32 insn, u32 vaddr, bool t2)
{
	memcpy(tramp, gen_insn_execbuf_thumb, sizeof(gen_insn_execbuf_thumb));
	make_def(tramp, insn, vaddr, t2);
}

static void tt_make_pc_deps(void *tramp, u32 mod_insn, u32 vaddr, bool t2)
{
	u32 pc_val = vaddr + 4;
	u16 *tr = tramp;

	memcpy(tramp, pc_dep_insn_execbuf_thumb,
	       sizeof(pc_dep_insn_execbuf_thumb));
	make_def(tramp, mod_insn, vaddr, t2);

	/* save PC value */
	tr[14] = pc_val & 0x0000ffff;
	tr[15] = pc_val >> 16;
}


static bool bad_reg(int n)
{
	return n == 13 || n == 15;
}

static int thumb_not_implement(thumb_insn_t insn, struct decode_info *info)
{
	return -EFAULT;
}

static int thumb_unpredictable(thumb_insn_t insn, struct decode_info *info)
{
	return -EINVAL;
}

/* hw1[1110 100x x1xx ????] */
static int t32_ldrd_strd(thumb_insn_t insn, struct decode_info *info)
{
	int w = GET_BIT(insn.hw1, 5);
	int n = GET_FIELD(insn.hw1, 0, 4);
	int t = GET_FIELD(insn.hw2, 12, 4);
	int t2 = GET_FIELD(insn.hw2, 8, 4);

	if (bad_reg(t) || bad_reg(t2))
		return thumb_unpredictable(insn, info);

	/* check load flag */
	if (GET_BIT(insn.hw1, 4)) {
		/* LDRD */
		if ((w && (n == 15)) || t == t2)
			return thumb_unpredictable(insn, info);

		if (n == 15) {
			/* change PC -> SP */
			insn.hw1 = SET_FIELD(insn.hw1, 0, 4, 13);
			tt_make_pc_deps(info->tramp, insn.val,
					info->vaddr, true);

			return 0;
		}
	} else {
		/* STRD */
		if ((w && t == n) || (w && t2 == n) || (n == 15))
			return thumb_unpredictable(insn, info);
	}

	tt_make_common(info->tramp, insn.val, info->vaddr, true);

	return 0;
}

/* load/store multiple */
static int t32_1110_100x_x0(thumb_insn_t insn, struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* load/store dual, load/store exclusive, table branch */
static int t32_1110_100x_x1(thumb_insn_t insn, struct decode_info *info)
{
	/* check PW bits */
	if (insn.hw1 & 0x120)
		return t32_ldrd_strd(insn, info);

	return thumb_not_implement(insn, info);
}

/* data-processing (shifled register) */
static int t32_1110_101(thumb_insn_t insn, struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* coprocessor, advanced SIMD, and floating-point instructions */
static int t32_1110_11(thumb_insn_t insn, struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

static void t32_simulate_branch(u32 insn, struct arch_insn_arm *ainsn,
				struct pt_regs *regs)
{
	u32 pc = regs->ARM_pc;
	thumb_insn_t i = { .val = insn };

	s32 offset = GET_FIELD(i.hw2, 0, 11);		/* imm11 */
	offset += GET_FIELD(i.hw1, 0, 10) << 11;	/* imm10 */
	offset += GET_BIT(i.hw2, 13) << 21;		/* J1 */
	offset += GET_BIT(i.hw2, 11) << 22;		/* J2 */

	/* check S bit */
	if (GET_BIT(i.hw1, 10))
		offset -= 0x00800000;	/* Apply sign bit */
	else
		offset ^= 0x00600000;	/* Invert J1 and J2 */

	/* check link */
	if (GET_BIT(i.hw2, 14)) {
		/* BL or BLX */
		regs->ARM_lr = regs->ARM_pc | 1;
		if (!GET_BIT(i.hw2, 12)) {
			/* BLX so switch to ARM mode */
			regs->ARM_cpsr &= ~PSR_T_BIT;
			pc &= ~3;
		}
	}

	regs->ARM_pc = pc + (offset * 2);
}

static int t32_branch(thumb_insn_t insn, struct decode_info *info)
{
	info->handeler = t32_simulate_branch;

	return 0;
}

/* data-processing (modified immediate) */
static int t32_1111_0x0x_xxxx_xxxx_0(thumb_insn_t insn,
				      struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* data-processing (plain binary immediate) */
static int t32_1111_0x1x_xxxx_xxxx_0(thumb_insn_t insn,
				      struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* conditional branch, move to blanked or special register */
static int t32_1111_0011_100x_xxxx_10x0(thumb_insn_t insn,
					struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* change processor state, and hints */
static int t32_1111_0011_1010_xxxx_10x0(thumb_insn_t insn,
					struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* miscellaneous control instructions */
static int t32_1111_0011_1011_xxxx_10x0(thumb_insn_t insn,
					struct decode_info *info)
{
	tt_make_common(info->tramp, insn.val, info->vaddr, true);

	return 0;
}

/* branch and exchange jazelle */
static int t32_1111_0011_1100_xxxx_10x0(thumb_insn_t insn,
					struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* exeption return  */
static int t32_1111_0011_1101_xxxx_10x0(thumb_insn_t insn,
					struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

/* move from banked or special register */
static int t32_1111_0011_111x_xxxx_10x0(thumb_insn_t insn,
					struct decode_info *info)
{
	return thumb_not_implement(insn, info);
}

static decode_handler_t t32_1111_0011_1XXX_xxxx_10x0[8] = {
	/* 000 */	t32_1111_0011_100x_xxxx_10x0,
	/* 001 */	t32_1111_0011_100x_xxxx_10x0,
	/* 010 */	t32_1111_0011_1010_xxxx_10x0,
	/* 011 */	t32_1111_0011_1011_xxxx_10x0,
	/* 100 */	t32_1111_0011_1100_xxxx_10x0,
	/* 101 */	t32_1111_0011_1101_xxxx_10x0,
	/* 110 */	t32_1111_0011_111x_xxxx_10x0,
	/* 111 */	t32_1111_0011_111x_xxxx_10x0,
};


static int t32_1111_0xxx_xxxx_xxxx_10x0(thumb_insn_t insn,
					struct decode_info *info)
{
	unsigned char op = GET_FIELD(insn.hw1, 4, 7);

	if (GET_FIELD(op, 3, 3) == 0x7) {
		if (GET_BIT(op, 6) == 0) {
			unsigned char val = GET_FIELD(op, 0, 3);

			return t32_1111_0011_1XXX_xxxx_10x0[val](insn, info);
		} else {
			/* hypervisor call and secure monitor call */
			return thumb_not_implement(insn, info);
		}
	} else {
		/* conditional branch */
		return thumb_not_implement(insn, info);
	}
}

static int t32_1111_0xxx_xxxx_xxxx_10x1(thumb_insn_t insn,
					struct decode_info *info)
{
	return t32_branch(insn, info);
}

static int t32_1111_0xxx_xxxx_xxxx_11x0(thumb_insn_t insn,
					struct decode_info *info)
{
	return t32_branch(insn, info);
}

static int t32_1111_0xxx_xxxx_xxxx_11x1(thumb_insn_t insn,
					struct decode_info *info)
{
	return t32_branch(insn, info);
}

static decode_handler_t table_1111_0xxx_xxxx_xxxx_1XXX[8] = {
	/* 000 */	t32_1111_0xxx_xxxx_xxxx_10x0,
	/* 001 */	t32_1111_0xxx_xxxx_xxxx_10x1,
	/* 010 */	t32_1111_0xxx_xxxx_xxxx_10x0,
	/* 011 */	t32_1111_0xxx_xxxx_xxxx_10x1,
	/* 100 */	t32_1111_0xxx_xxxx_xxxx_11x0,
	/* 101 */	t32_1111_0xxx_xxxx_xxxx_11x1,
	/* 110 */	t32_1111_0xxx_xxxx_xxxx_11x0,
	/* 111 */	t32_1111_0xxx_xxxx_xxxx_11x1,
};

/* branches and miscellaneous control */
static int t32_1111_0xxx_xxxx_xxxx_1(thumb_insn_t insn,
				      struct decode_info *info)
{
	unsigned char op1 = GET_FIELD(insn.hw2, 12, 3);

	/* check op1 bits hw1[1111 0??? ???? ???? 1xxx] */
	return table_1111_0xxx_xxxx_xxxx_1XXX[op1](insn, info);
}

static int t32_1110_1(thumb_insn_t insn, struct decode_info *info)
{
	unsigned char op2 = GET_FIELD(insn.hw1, 4, 7);

	if (GET_BIT(op2, 6) == 0) {
		if (GET_BIT(op2, 5) == 0) {
			if (GET_BIT(op2, 2) == 0)
				return t32_1110_100x_x0(insn, info);
			else
				return t32_1110_100x_x1(insn, info);
		} else {
			return t32_1110_101(insn, info);
		}
	} else {
		return t32_1110_11(insn, info);
	}

	return thumb_not_implement(insn, info);
}

static int t32_1111_0(thumb_insn_t insn, struct decode_info *info)
{
	if (GET_BIT(insn.hw2, 15) == 0) {
		if (GET_BIT(insn.hw1, 9) == 0)
			return t32_1111_0x0x_xxxx_xxxx_0(insn, info);
		else
			return t32_1111_0x1x_xxxx_xxxx_0(insn, info);
	} else {
		/* [1111 0xxx xxxx xxxx 1xxx xxxx xxxx xxxx] */
		return t32_1111_0xxx_xxxx_xxxx_1(insn, info);
	}

	return thumb_not_implement(insn, info);
}

decode_handler_t table_111x_x[4] = {
	/* 00 | 16-bit */	thumb_not_implement,
	/* 01 | 32-bit */	t32_1110_1,
	/* 10 | 32-bit */	t32_1111_0,
	/* 11 | 32-bit */	thumb_not_implement,
};

static int b111(thumb_insn_t insn, struct decode_info *info)
{
	unsigned char op1 = GET_FIELD(insn.hw1, 11, 2);

	/* check op1 bits hw1[111x x??? ???? ????] */
	return table_111x_x[op1](insn, info);
}

decode_handler_t table_xxx[8] = {
	/* 000 */	thumb_not_implement,
	/* 001 */	thumb_not_implement,
	/* 010 */	thumb_not_implement,
	/* 011 */	thumb_not_implement,
	/* 100 */	thumb_not_implement,
	/* 101 */	thumb_not_implement,
	/* 110 */	thumb_not_implement,
	/* 111 */	b111,
};


int decode_thumb(u32 insn, struct decode_info *info)
{
	thumb_insn_t tinsn = { .val = insn };

	/* check first 3 bits hw1[xxx? ???? ???? ????] */
	return table_xxx[GET_FIELD(tinsn.hw1, 13, 3)](tinsn, info);
}
