/* board-orbis-thermistor.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/sec_thermistor.h>
#include <plat/adc.h>

static struct sec_therm_adc_table temp_table_ap[] = {
	/* adc, temperature */
	{194, 800},
	{219, 750},
	{254, 700},
	{300, 650},
	{347, 600},
	{404, 550},
	{469, 500},
	{547, 450},
	{634, 400},
	{732, 350},
	{834, 300},
	{946, 250},
	{1058, 200},
	{1170, 150},
	{1281, 100},
	{1381, 50},
	{1403, 40},
	{1427, 30},
	{1448, 20},
	{1467, 10},
	{1485, 0},
	{1529, -10},
	{1534, -20},
	{1549, -30},
	{1566, -40},
	{1586, -50},
	{1660, -100},
	{1730, -150},
	{1791, -200},
};

static struct sec_therm_adc_table temp_table_batt[] = {
	/* adc, temperature */
	{  191,  800 },
	{  231,  750 },
	{  271,  700 },
	{  316,  650 },
	{  365,  600 },
	{  391,  580 },
	{  427,  550 },
	{  457,  530 },
	{  498,  500 },
	{  528,  480 },
	{  564,  460 },
	{  583,  450 },
	{  669,  400 },
	{  765,  350 },
	{  870,  300 },
	{  980,  250 },
	{ 1091,  200 },
	{ 1210,  150 },
	{ 1320,  100 },
	{ 1424,   50 },
	{ 1538,    0 },
	{ 1574,  -20 },
	{ 1628,  -50 },
	{ 1668,  -70 },
	{ 1717, -100 },
	{ 1755, -150 },
	{ 1795, -200 },
};

static struct sec_therm_adc_table temp_table_default[] = {
	/* adc, temperature */
	{181, 800},
	{189, 790},
	{197, 780},
	{205, 770},
	{213, 760},
	{221, 750},
	{229, 740},
	{237, 730},
	{245, 720},
	{253, 710},
	{261, 700},
	{269, 690},
	{277, 680},
	{286, 670},
	{297, 660},
	{309, 650},
	{318, 640},
	{329, 630},
	{340, 620},
	{350, 610},
	{361, 600},
	{370, 590},
	{380, 580},
	{393, 570},
	{405, 560},
	{418, 550},
	{431, 540},
	{446, 530},
	{460, 520},
	{473, 510},
	{489, 500},
	{503, 490},
	{520, 480},
	{536, 470},
	{552, 460},
	{569, 450},
	{586, 440},
	{604, 430},
	{622, 420},
	{640, 410},
	{659, 400},
	{679, 390},
	{698, 380},
	{718, 370},
	{739, 360},
	{759, 350},
	{781, 340},
	{803, 330},
	{824, 320},
	{847, 310},
	{869, 300},
	{891, 290},
	{914, 280},
	{938, 270},
	{959, 260},
	{982, 250},
	{1005, 240},
	{1030, 230},
	{1052, 220},
	{1076, 210},
	{1099, 200},
	{1126, 190},
	{1145, 180},
	{1168, 170},
	{1193, 160},
	{1215, 150},
	{1239, 140},
	{1263, 130},
	{1286, 120},
	{1309, 110},
	{1333, 100},
	{1354, 90},
	{1377, 80},
	{1397, 70},
	{1418, 60},
	{1439, 50},
	{1459, 40},
	{1483, 30},
	{1497, 20},
	{1518, 10},
	{1537, 0},
	{1557, -10},
	{1577, -20},
	{1593, -30},
	{1611, -40},
	{1628, -50},
	{1645, -60},
	{1661, -70},
	{1678, -80},
	{1693, -90},
	{1704, -100},
	{1718, -110},
	{1732, -120},
	{1745, -130},
	{1757, -140},
	{1770, -150},
	{1781, -160},
	{1794, -170},
	{1805, -180},
	{1815, -190},
	{1827, -200},
};

struct sec_therm_adc_info orbis_adc_list[] = {
	{
		.therm_id = SEC_THERM_AP,
		.name = "ap_therm",
		.adc_ch = 1,
	},
	{
		.therm_id = SEC_THERM_BATTERY,
		.name = "batt_therm",
		.adc_ch = 0,
	},
};

static struct sec_therm_platform_data sec_therm_pdata = {
	.adc_list = orbis_adc_list,
	.adc_list_size = ARRAY_SIZE(orbis_adc_list),
};

struct platform_device sec_device_thermistor = {
	.name = "sec-thermistor",
	.id = -1,
	.dev.platform_data = &sec_therm_pdata,
};

int __sec_therm_adc_dev_read(struct sec_therm_adc_info *adc, int *val)
{
	int adc_val;

	adc_val = s3c_adc_read(adc->adc_client, adc->adc_ch);
	if (adc_val < 0) {
		pr_err("%s: invalid adc value: %d\n", __func__, adc_val);
		return -EINVAL;
	}

	pr_debug("%s: val: %d\n", __func__, adc_val);
	*val = adc_val;
	return 0;
}

int sec_therm_temp_table_init(struct sec_therm_info *info)
{
	int i;

	for (i = 0; i < info->adc_list_size; i++) {
		switch (info->adc_list[i].therm_id) {
			case SEC_THERM_AP:
				info->adc_list[i].temp_table = temp_table_ap;
				info->adc_list[i].temp_table_size = ARRAY_SIZE(temp_table_ap);
				break;
			case SEC_THERM_BATTERY:
				info->adc_list[i].temp_table = temp_table_batt;
				info->adc_list[i].temp_table_size = ARRAY_SIZE(temp_table_batt);
				break;
			case SEC_THERM_XO:
			case SEC_THERM_PAM0:
			case SEC_THERM_PAM1:
			case SEC_THERM_FLASH:
				info->adc_list[i].temp_table = temp_table_default;
				info->adc_list[i].temp_table_size = ARRAY_SIZE(temp_table_default);
				break;
			default:
				return -EINVAL;
		}
	}

	return 0;
}

int sec_therm_adc_init(struct sec_therm_info *info)
{
	struct s3c_adc_client *client;
	int i;

	client = s3c_adc_register(&sec_device_thermistor, NULL, NULL, 0);
	if (IS_ERR_OR_NULL(client)) {
		pr_err("%s: Failed to register s3c adc\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < info->adc_list_size; i++)
		info->adc_list[i].adc_client = (void *)client;

	return 0;
}

void sec_therm_adc_exit(struct sec_therm_info *info)
{
	struct s3c_adc_client *client;
	int i;

	for (i = 0; i < info->adc_list_size; i++) {
		client = (struct s3c_adc_client *)info->adc_list[i].adc_client;
		s3c_adc_release(client);
		info->adc_list[i].adc_client = NULL;
	}
}

void exynos3_thermistor_init(void)
{
#ifdef CONFIG_SEC_THERMISTOR
	platform_device_register(&sec_device_thermistor);
#endif
}
