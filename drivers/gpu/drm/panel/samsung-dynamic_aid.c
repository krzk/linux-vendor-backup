/*
 * Dynamic AMOLED Impulse Driving (DAID) helper functions.
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * Andrzej Hajda <a.hajda@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>

#include "samsung-dynamic_aid.h"

static const u8 daid_gcp[] = {
	0, 3, 11, 23, 35, 51, 87, 151, 203, 255
};

static const int daid_vt_coefficient[] = {
	0, 12, 24, 36, 48, 60, 72, 84, 96, 108, 138, 148, 158, 168, 178, 186
};

static const int daid_gamma_formula_mods[2][2] = {
	{64, 320}, /* V1 - V203 */
	{72, 860}, /* V255, VT */
};

static const int daid_gamma_default[] = {
	0x80, /* V1 - V203 */
	0x100, /* V255 */
};

/* Gamma Curve Tables */

static const int daid_gct_2p15[DAID_VOUT_COUNT] = {
	0,	7,	30,	71,	132,	213,	315,	439,
	586,	754,	946,	1161,	1400,	1663,	1950,	2262,
	2599,	2961,	3348,	3761,	4199,	4663,	5154,	5671,
	6214,	6784,	7381,	8005,	8656,	9335,	10040,	10774,
	11535,	12324,	13141,	13986,	14859,	15761,	16691,	17649,
	18637,	19653,	20698,	21772,	22875,	24007,	25169,	26360,
	27581,	28831,	30111,	31421,	32760,	34130,	35529,	36959,
	38419,	39909,	41429,	42980,	44562,	46174,	47817,	49490,
	51195,	52930,	54696,	56494,	58322,	60182,	62073,	63995,
	65948,	67933,	69950,	71998,	74078,	76189,	78333,	80508,
	82715,	84954,	87224,	89528,	91863,	94230,	96630,	99062,
	101526,	104022,	106552,	109113,	111708,	114334,	116994,	119686,
	122411,	125169,	127960,	130784,	133641,	136530,	139453,	142409,
	145399,	148421,	151477,	154566,	157688,	160844,	164034,	167257,
	170513,	173803,	177127,	180484,	183875,	187300,	190759,	194252,
	197778,	201339,	204933,	208562,	212224,	215921,	219652,	223417,
	227217,	231050,	234918,	238821,	242757,	246729,	250734,	254775,
	258849,	262959,	267103,	271282,	275495,	279743,	284026,	288344,
	292697,	297084,	301507,	305964,	310457,	314984,	319547,	324145,
	328778,	333446,	338149,	342888,	347661,	352471,	357315,	362195,
	367110,	372061,	377047,	382069,	387126,	392219,	397348,	402512,
	407712,	412948,	418219,	423526,	428869,	434248,	439663,	445113,
	450600,	456122,	461681,	467275,	472906,	478572,	484275,	490014,
	495789,	501600,	507448,	513332,	519252,	525208,	531201,	537230,
	543296,	549398,	555536,	561711,	567923,	574171,	580455,	586777,
	593134,	599529,	605960,	612428,	618933,	625474,	632052,	638668,
	645319,	652008,	658734,	665497,	672296,	679133,	686006,	692917,
	699865,	706850,	713872,	720931,	728027,	735160,	742331,	749539,
	756784,	764066,	771386,	778743,	786138,	793569,	801039,	808545,
	816089,	823671,	831290,	838947,	846641,	854373,	862143,	869950,
	877794,	885677,	893597,	901555,	909550,	917584,	925655,	933764,
	941911,	950095,	958318,	966578,	974877,	983213,	991588,	1000000
};

static const int daid_gct_2p20[DAID_VOUT_COUNT] = {
	0,	5,	23,	57,	107,	175,	262,	367,
	493,	638,	805,	992,	1202,	1433,	1687,	1963,
	2263,	2586,	2932,	3303,	3697,	4116,	4560,	5028,
	5522,	6041,	6585,	7155,	7751,	8373,	9021,	9696,
	10398,	11126,	11881,	12664,	13473,	14311,	15175,	16068,
	16988,	17936,	18913,	19918,	20951,	22013,	23104,	24223,
	25371,	26549,	27755,	28991,	30257,	31551,	32876,	34230,
	35614,	37029,	38473,	39947,	41452,	42987,	44553,	46149,
	47776,	49433,	51122,	52842,	54592,	56374,	58187,	60032,
	61907,	63815,	65754,	67725,	69727,	71761,	73828,	75926,
	78057,	80219,	82414,	84642,	86901,	89194,	91518,	93876,
	96266,	98689,	101145,	103634,	106156,	108711,	111299,	113921,
	116576,	119264,	121986,	124741,	127530,	130352,	133209,	136099,
	139022,	141980,	144972,	147998,	151058,	154152,	157281,	160444,
	163641,	166872,	170138,	173439,	176774,	180144,	183549,	186989,
	190463,	193972,	197516,	201096,	204710,	208360,	212044,	215764,
	219520,	223310,	227137,	230998,	234895,	238828,	242796,	246800,
	250840,	254916,	259027,	263175,	267358,	271577,	275833,	280124,
	284452,	288816,	293216,	297653,	302125,	306635,	311180,	315763,
	320382,	325037,	329729,	334458,	339223,	344026,	348865,	353741,
	358654,	363604,	368591,	373615,	378676,	383775,	388910,	394083,
	399293,	404541,	409826,	415148,	420508,	425905,	431340,	436813,
	442323,	447871,	453456,	459080,	464741,	470440,	476177,	481952,
	487765,	493616,	499505,	505432,	511398,	517401,	523443,	529523,
	535642,	541798,	547994,	554227,	560499,	566810,	573159,	579547,
	585973,	592438,	598942,	605484,	612066,	618686,	625345,	632043,
	638779,	645555,	652370,	659224,	666117,	673049,	680020,	687031,
	694081,	701170,	708298,	715465,	722672,	729919,	737205,	744530,
	751895,	759300,	766744,	774227,	781751,	789314,	796917,	804559,
	812241,	819964,	827726,	835528,	843370,	851252,	859174,	867136,
	875138,	883180,	891262,	899385,	907547,	915750,	923993,	932277,
	940601,	948965,	957370,	965815,	974300,	982826,	991393,	1000000
};

static int daid_bits_to_int(const u8* data, int bit_start, int bit_len)
{
	int o = bit_start / 8, ob = bit_start % 8;
	int val, sign;

	switch (bit_len) {
	case 4:
		val = (data[o] >> ob) & 0x7;
		sign = data[o] & BIT(ob + 3);
		break;
	case 8:
		return (char)data[o];
	case 9:
		val = data[o + 1];
		sign = data[o] & 1;
	}

	return sign ? -val : val;
}

static void daid_int_to_bits(u8* data, int bit_start, int bit_len, int val)
{
	int o = bit_start / 8, ob = bit_start % 8;

	switch (bit_len) {
	case 4:
		data[o] &= ~(0xf << ob);
		data[o] |= val << ob;
		break;
	case 8:
		data[o] = (u8)val;
		break;
	case 9:
		data[o] = (val & 0x100) ? 1 : 0;
		data[o + 1] = val;
	}
}

static void daid_params_to_array(daid_rgb *arr, const u8 *d)
{
	int i, j;

	for (j = 0; j < 3; ++j) {
		arr[0][j] = daid_bits_to_int(&d[DAID_PARAM_COUNT - 2],
			4 * j, 4);
		arr[DAID_GCP_COUNT - 1][j] = daid_bits_to_int(&d[2 * j], 0, 9);
	}

	for (i = 1; i < DAID_GCP_COUNT - 1; ++i)
		for (j = 0; j < 3; ++j)
			arr[i][j] = daid_bits_to_int(
				&d[DAID_PARAM_COUNT - 5 - 3 * i + j], 0, 8);
}

static void daid_array_to_params(u8 *d, daid_rgb *arr)
{
	int i, j;

	for (j = 0; j < 3; ++j) {
		daid_int_to_bits(&d[DAID_PARAM_COUNT - 2], 4 * j, 4, arr[0][j]);
		daid_int_to_bits(&d[2 * j], 0, 9, arr[DAID_GCP_COUNT - 1][j]);
		d[3 * DAID_GCP_COUNT + j] = 0;
	}

	for (i = 1; i < DAID_GCP_COUNT - 1; ++i)
		for (j = 0; j < 3; ++j)
			daid_int_to_bits(&d[DAID_PARAM_COUNT - 5 - 3 * i + j],
				0, 8, arr[i][j]);
}

struct daid_ctx {
	struct daid_cfg cfg;
	daid_rgb mtp[DAID_GCP_COUNT];
	daid_rgb vgcp[DAID_GCP_COUNT];
	daid_rgb vout[DAID_VOUT_COUNT];
};

static void daid_calc_vgcp(struct daid_ctx *ctx)
{
	int vref, vdiff;
	int i = 0, j;
	const int v0 = ctx->cfg.vreg_out * 100;
	daid_rgb *v = ctx->vgcp;
	const int i_max = DAID_GCP_COUNT - 1;

	/* VT is calculated differently */
	for (j = 0; j < 3; ++j) {
		vdiff = v0 * daid_vt_coefficient[ctx->mtp[0][j]];
		v[0][j] = v0 - vdiff / daid_gamma_formula_mods[1][1];
	}

	for (i = i_max; i > 0; --i) {
		const int *m = daid_gamma_formula_mods[i == i_max];
		const int gamma = daid_gamma_default[i == i_max];

		for (j = 0; j < 3; ++j) {
			if (i == 1 || i == i_max)
				vref = v0;
			else
				vref = v[0][j];
			vdiff = vref;
			if (i < i_max)
				vdiff -=  v[i + 1][j];
			vdiff *= gamma + ctx->mtp[i][j] + m[0];
			v[i][j] = vref - vdiff / m[1];
		}
	}
}

static void daid_calc_vout(struct daid_ctx *ctx)
{
	int i, j, k, ke, uninitialized_var(dk);
	daid_rgb dv, v;
	daid_rgb *ov = ctx->vout;

	for (j = 0; j < 3; ++j)
		ov[0][j] = v[j] = ctx->cfg.vreg_out * 100;

	for (k = 1, i = 0, ke = 0; k < DAID_VOUT_COUNT; ++k) {
		if (k > ke) {
			ke = daid_gcp[++i];
			dk = ke - daid_gcp[i - 1];
			for (j = 0; j < 3; ++j) {
				dv[j] = ctx->vgcp[i][j] - v[j];
				v[j] = ctx->vgcp[i][j];
			}
		}
		for (j = 0; j < 3; ++j)
			ov[k][j] = v[j] - dv[j] * (ke - k) / dk;
	}
}

static int daid_get_lut_index(int val)
{
	const int *begin = daid_gct_2p20, *end = begin + DAID_VOUT_COUNT;
	const int *b = begin, *e = end;

	while (e - b > 1) {
		const int *m = b + (e - b) / 2;
		if (val >= *m)
			b = m;
		else
			e = m;
	}

	if ((e < end) && (val - *b > *e - val))
		b = e;

	return b - begin;
}

static void daid_calc_vmgcp(struct daid_ctx *ctx, daid_rgb *mv, int ibr)
{
	const int nit = ctx->cfg.nits[ibr];
	const int *gct = nit < ctx->cfg.nit_gct ? daid_gct_2p15 : daid_gct_2p20;
	const int nit_max = ctx->cfg.nits[ctx->cfg.nits_count - 1];
	int i, j;

	for (j = 0; j < 3; ++j)
		mv[0][j] = ctx->vgcp[0][j];

	for (i = 1; i < DAID_GCP_COUNT; ++i) {
		int l = ctx->cfg.brightness_base[ibr] * gct[daid_gcp[i]]
			/ nit_max;
		int gv = daid_get_lut_index(l) + ctx->cfg.gradation[ibr][i];
		for (j = 0; j < 3; ++j)
			mv[i][j] = ctx->vout[gv][j];
	}

}

static void daid_calc_gamma(struct daid_ctx *ctx, daid_rgb *g, int ibr)
{
	int v0, vref, vdiff;
	int i = 0, j, gt;
	const int i_max = DAID_GCP_COUNT - 1;
	daid_rgb mv[DAID_GCP_COUNT];

	daid_calc_vmgcp(ctx, mv, ibr);

	v0 = ctx->cfg.vreg_out * 100;

	for (i = i_max; i > 0; --i) {
		const int *m = daid_gamma_formula_mods[i == i_max];

		for (j = 0; j < 3; ++j) {
			vref = (i == 1 || i == i_max) ? v0 : ctx->vgcp[0][j];
			vdiff = (i < i_max) ? (vref - mv[i + 1][j]) : vref;
			vref -= mv[i][j];
			gt = (vref + 1) * m[1] / vdiff - m[0];
			gt += ctx->cfg.color_offset[ibr][i][j] - ctx->mtp[i][j];
			if (gt > 255 && i != i_max)
				gt = 255;
			g[i][j] = gt;
		}
	}
	for (j = 0; j < 3; ++j)
		g[0][j] = 0;
}

int daid_calc_gammodes(u8 (*gamma)[DAID_PARAM_COUNT],
		struct daid_cfg *cfg, u8 *mtp)
{
	struct daid_ctx *ctx;
	daid_rgb g[DAID_GCP_COUNT];
	int i;

	ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->cfg = *cfg;
	daid_params_to_array(ctx->mtp, mtp);
	daid_calc_vgcp(ctx);
	daid_calc_vout(ctx);
	for (i = 0; i < cfg->nits_count; ++i) {
		daid_calc_gamma(ctx, g, i);
		daid_array_to_params(gamma[i], g);
		pr_debug("Gamma[%d]: %*ph\n", i, DAID_PARAM_COUNT, gamma[i]);
	}

	kfree(ctx);

	return 0;
}
