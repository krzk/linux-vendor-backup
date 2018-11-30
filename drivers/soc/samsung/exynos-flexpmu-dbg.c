/*
 * Exynos regulator support.
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#ifdef CONFIG_SLEEP_STAT_EXYNOS
#include <linux/sleep_stat_exynos.h>
#include <linux/suspend.h>
#ifdef CONFIG_ENERGY_MONITOR
#include <linux/power/energy_monitor.h>
#endif
#endif

#define EXYNOS_FLEXPMU_DBG_PREFIX	"EXYNOS-FLEXPMU-DBG: "

#define DATA_LINE	(16)
#define DATA_IDX	(8)

#define FLEXPMU_DBG_FUNC_READ(__name)		\
	exynos_flexpmu_dbg_ ## __name ## _read

#define BUF_MAX_LINE	65
#define BUF_LINE_SIZE	30
#define BUF_SIZE	(BUF_MAX_LINE * BUF_LINE_SIZE)

#define DEC_PRINT	1
#define HEX_PRINT	2

/* enum for debugfs files */
enum flexpmu_debugfs_id {
	FID_CPU_STATUS,
	FID_SEQ_STATUS,
	FID_CUR_SEQ,
	FID_SW_FLAG,
	FID_SEQ_COUNT,
	FID_MIF_ALWAYS_ON,
	FID_LPM_COUNT,
#ifdef CONFIG_SLEEP_STAT_EXYNOS
	FID_SLEEP_STAT,
#endif
	FID_MAX
};

char *flexpmu_debugfs_name[FID_MAX] = {
	"cpu_status",
	"seq_status",
	"cur_sequence",
	"sw_flag",
	"seq_count",
	"mif_always_on",
	"lpm_count",
#ifdef CONFIG_SLEEP_STAT_EXYNOS
	"sleep_stat",
#endif
};

/* enum for data lines */
enum data_id {
	DID_CPU_STATUS,		/* 0 */
	DID_SEQ_STATUS,
	DID_CUR_SEQ0,
	DID_CUR_SEQ1,
	DID_PWR_MODE0,		/* 4 */
	DID_PWR_MODE1,
	DID_PWR_MODE2,
	DID_PWR_MODE3,
	DID_PWR_MODE4,
	DID_PWR_MODE5,
	DID_SW_FLAG,
	DID_IRQ_STATUS,		/* 11 */
	DID_IRQ_DATA,
	DID_IPC_AP_STATUS,
	DID_IPC_AP_RXDATA,
	DID_IPC_AP_TXDATA,
	DID_SOC_COUNT,
	DID_MIF_COUNT,
	DID_IPC_VTS0,
	DID_IPC_VTS1,
	DID_LOCAL_PWR,
	DID_MIF_ALWAYS_ON,		/* 21 */
	DID_AP_COUNT_SLEEP,
	DID_MIF_COUNT_SLEEP,
	DID_AP_COUNT_SICD,
	DID_MIF_COUNT_SICD,
	DID_CUR_PMD,
	DID_MAX
};

struct flexpmu_dbg_print_arg {
	char prefix[BUF_LINE_SIZE];
	int print_type;
};

struct dbgfs_info {
	int fid;
	struct dentry *den;
	struct file_operations fops;
};

struct dbgfs_info *flexpmu_dbg_info;
void __iomem *flexpmu_dbg_base;
static struct dentry *flexpmu_dbg_root;


u32 acpm_get_mifdn_count(void)
{
	return __raw_readl(flexpmu_dbg_base + (DATA_LINE * DID_MIF_COUNT_SLEEP) + DATA_IDX + 4);
}
EXPORT_SYMBOL_GPL(acpm_get_mifdn_count);

#define MIF_REQ_MASK		(0x00FF0000)
#define MIF_REQ_SHIFT		(16)
u32 acpm_get_mif_request(void)
{
	u32 reg = __raw_readl(flexpmu_dbg_base + (DATA_LINE * DID_SW_FLAG) + DATA_IDX + 4);

	return ((reg & MIF_REQ_MASK) >> MIF_REQ_SHIFT);

}
EXPORT_SYMBOL_GPL(acpm_get_mif_request);

static ssize_t print_dataline_2(int did, struct flexpmu_dbg_print_arg *print_arg,
		ssize_t len, char *buf, int *data_count)
{
	int data[2];
	ssize_t line_len;
	int i;

	for (i = 0; i < 2; i++) {
		if (print_arg[*data_count].print_type == DEC_PRINT) {
			data[i] = __raw_readl(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i * 4);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : %d\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		} else if (print_arg[*data_count].print_type == HEX_PRINT) {
			data[i] = __raw_readl(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i * 4);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : 0x%x\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		}
		*data_count += 1;
	}

	return len;
}

static ssize_t print_dataline_8(int did, struct flexpmu_dbg_print_arg *print_arg,
		ssize_t len, char *buf, int *data_count)
{
	int data[8];
	ssize_t line_len;
	int i;

	for (i = 0; i < 8; i++) {
		if (print_arg[*data_count].print_type == DEC_PRINT) {
			data[i] = __raw_readb(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : %d\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		} else if (print_arg[*data_count].print_type == HEX_PRINT) {
			data[i] = __raw_readb(flexpmu_dbg_base +
					(DATA_LINE * did) + DATA_IDX + i);

			line_len = snprintf(buf + len, BUF_SIZE - len, "%s : 0x%x\n",
					print_arg[*data_count].prefix, data[i]);
			if (line_len > 0)
				len += line_len;
		}
		*data_count += 1;
	}

	return len;
}

struct flexpmu_dbg_print_arg cpu_print_arg[BUF_MAX_LINE] = {
	{"CL0_CPU0", DEC_PRINT},
	{"CL0_CPU1", DEC_PRINT},
	{"CL0_CPU2", DEC_PRINT},
	{"CL0_CPU3", DEC_PRINT},
	{"CL1_CPU0", DEC_PRINT},
	{"CL1_CPU1", DEC_PRINT},
	{"CL1_CPU2", DEC_PRINT},
	{"CL1_CPU3", DEC_PRINT},
};

static ssize_t exynos_flexpmu_dbg_cpu_status_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	ret = print_dataline_8(DID_CPU_STATUS, cpu_print_arg, ret, buf, &data_count);

	return ret;
}

struct flexpmu_dbg_print_arg seq_print_arg[BUF_MAX_LINE] = {
	{"SOC seq", DEC_PRINT},
	{"MIF seq", DEC_PRINT},
	{},
	{},
	{"nonCPU CL0", DEC_PRINT},
	{"nonCPU CL1", DEC_PRINT},
	{},
};

static ssize_t exynos_flexpmu_dbg_seq_status_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	ret = print_dataline_8(DID_SEQ_STATUS, seq_print_arg, ret, buf, &data_count);

	return ret;
}

struct flexpmu_dbg_print_arg cur_print_arg[BUF_MAX_LINE] = {
	{"UP Sequence", DEC_PRINT},
	{"DOWN Sequence", DEC_PRINT},
	{"Access Type", DEC_PRINT},
	{"Seq Index", DEC_PRINT},
};

static ssize_t exynos_flexpmu_dbg_cur_sequence_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	ret = print_dataline_2(DID_CUR_SEQ0, cur_print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_CUR_SEQ1, cur_print_arg, ret, buf, &data_count);

	return ret;
}

struct flexpmu_dbg_print_arg sw_flag_print_arg[BUF_MAX_LINE] = {
	{"Hotplug out flag", HEX_PRINT},
	{"Reset-Release flag", HEX_PRINT},
	{},
	{},
	{"CHUB ref_count", DEC_PRINT},
	{},
	{"MIF req_Master", HEX_PRINT},
	{"MIF req_count", DEC_PRINT},
};

static ssize_t exynos_flexpmu_dbg_sw_flag_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	ret = print_dataline_8(DID_SW_FLAG, sw_flag_print_arg, ret, buf, &data_count);

	return ret;
}

struct flexpmu_dbg_print_arg seq_cnt_print_arg[BUF_MAX_LINE] = {
	{"Early Wakeup", DEC_PRINT},
	{"SOC sequence", DEC_PRINT},
	{},
	{"MIF sequence", DEC_PRINT},
};

static ssize_t exynos_flexpmu_dbg_seq_count_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	ret = print_dataline_2(DID_SOC_COUNT, seq_cnt_print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_MIF_COUNT, seq_cnt_print_arg, ret, buf, &data_count);

	return ret;
}

struct flexpmu_dbg_print_arg mif_print_arg[BUF_MAX_LINE] = {
	{},
	{"MIF always on", DEC_PRINT},
};

static ssize_t exynos_flexpmu_dbg_mif_always_on_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	ret = print_dataline_2(DID_MIF_ALWAYS_ON, mif_print_arg, ret, buf, &data_count);

	return ret;
}

struct flexpmu_dbg_print_arg lpm_cnt_print_arg[BUF_MAX_LINE] = {
	{"[SLEEP] Early wakeup", DEC_PRINT},
	{"[SLEEP] SOC seq down", DEC_PRINT},
	{},
	{"[SLEEP] MIF seq down", DEC_PRINT},
	{"[SICD] Early wakeup", DEC_PRINT},
	{"[SICD] SOC seq down", DEC_PRINT},
	{},
	{"[SICD] MIF seq down", DEC_PRINT},
};

static ssize_t exynos_flexpmu_dbg_lpm_count_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data_count = 0;

	ret = print_dataline_2(DID_AP_COUNT_SLEEP, lpm_cnt_print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_MIF_COUNT_SLEEP, lpm_cnt_print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_AP_COUNT_SICD, lpm_cnt_print_arg, ret, buf, &data_count);
	ret = print_dataline_2(DID_MIF_COUNT_SICD, lpm_cnt_print_arg, ret, buf, &data_count);

	return ret;
}

#ifdef CONFIG_SLEEP_STAT_EXYNOS
#ifdef CONFIG_ENERGY_MONITOR
struct sleep_stat_exynos emon_sleep_stat;

int sleep_stat_exynos_get_stat(int type, struct sleep_stat_exynos *sleep_stat)
{
	struct sleep_stat_exynos *stat = &emon_sleep_stat;
	int data1, data2, data3, data4, data5, data6;

	sleep_stat->fail = suspend_stats.fail -
								stat->fail;
	sleep_stat->failed_freeze = suspend_stats.failed_freeze -
								stat->failed_freeze;
	sleep_stat->failed_prepare = suspend_stats.failed_prepare -
								stat->failed_prepare;
	sleep_stat->failed_suspend = suspend_stats.failed_suspend -
								stat->failed_suspend;
	sleep_stat->failed_suspend_late = suspend_stats.failed_suspend_late -
								stat->failed_suspend_late;
	sleep_stat->failed_suspend_noirq = suspend_stats.failed_suspend_noirq -
								stat->failed_suspend_noirq;
	sleep_stat->suspend_success = suspend_stats.success -
								stat->suspend_success;

	data1 = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SLEEP) + DATA_IDX);
	sleep_stat->acpm_sleep_early_wakeup = data1 - stat->acpm_sleep_early_wakeup;

	data2 = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SLEEP) + DATA_IDX + 4);
	sleep_stat->acpm_sleep_soc_down  = data2 - stat->acpm_sleep_soc_down;

	data3 = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_MIF_COUNT_SLEEP) + DATA_IDX + 4);
	sleep_stat->acpm_sleep_mif_down = data3 - stat->acpm_sleep_mif_down;

	data4 = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SICD) + DATA_IDX);
	sleep_stat->acpm_sicd_early_wakeup = data4 - stat->acpm_sicd_early_wakeup;

	data5 = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SICD) + DATA_IDX + 4);
	sleep_stat->acpm_sicd_soc_down  = data5 - stat->acpm_sicd_soc_down;

	data6 = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_MIF_COUNT_SICD) + DATA_IDX + 4);
	sleep_stat->acpm_sicd_mif_down = data6 - stat->acpm_sicd_mif_down;

	if (type != ENERGY_MON_TYPE_DUMP) {
		stat->fail = suspend_stats.fail;
		stat->failed_freeze = suspend_stats.failed_freeze;
		stat->failed_prepare = suspend_stats.failed_prepare;
		stat->failed_suspend = suspend_stats.failed_suspend;
		stat->failed_suspend_late = suspend_stats.failed_suspend_late;
		stat->failed_suspend_noirq = suspend_stats.failed_suspend_noirq;
		stat->suspend_success = suspend_stats.success;

		stat->acpm_sleep_early_wakeup = data1;
		stat->acpm_sleep_soc_down = data2;
		stat->acpm_sleep_mif_down = data3;
		stat->acpm_sicd_early_wakeup = data4;
		stat->acpm_sicd_soc_down = data5;
		stat->acpm_sicd_mif_down = data6;
	}

	return 0;
}
#endif

static ssize_t exynos_flexpmu_dbg_sleep_stat_read(int fid, char *buf)
{
	ssize_t ret = 0;
	int data;

	ret += snprintf(buf + ret, BUF_SIZE - ret, "%s: %d\n%s: %d\n%s: %d\n"
			"%s: %d\n%s: %d\n%s: %d\n%s: %d\n",
			"fail",suspend_stats.fail,
			"failed_freeze", suspend_stats.failed_freeze,
			"failed_prepare", suspend_stats.failed_prepare,
			"failed_suspend", suspend_stats.failed_suspend,
			"failed_suspend_late", suspend_stats.failed_suspend_late,
			"failed_suspend_noirq", suspend_stats.failed_suspend_noirq,
			"suspend_success", suspend_stats.success);

	data = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SLEEP) + DATA_IDX);
	ret += snprintf(buf + ret, BUF_SIZE - ret, "acpm_sleep_early_wakeup: %d\n", data);

	data = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SLEEP) + DATA_IDX + 4);
	ret += snprintf(buf + ret, BUF_SIZE - ret, "acpm_sleep_soc_down: %d\n", data);

	data = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_MIF_COUNT_SLEEP) + DATA_IDX + 4);
	ret += snprintf(buf + ret, BUF_SIZE - ret, "acpm_sleep_mif_down: %d\n", data);

	data = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SICD) + DATA_IDX);
	ret += snprintf(buf + ret, BUF_SIZE - ret, "acpm_sicd_early_wakeup: %d\n", data);

	data = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_AP_COUNT_SICD) + DATA_IDX + 4);
	ret += snprintf(buf + ret, BUF_SIZE - ret, "acpm_sicd_soc_down: %d\n", data);

	data = __raw_readl(flexpmu_dbg_base +
			(DATA_LINE * DID_MIF_COUNT_SICD) + DATA_IDX + 4);
	ret += snprintf(buf + ret, BUF_SIZE - ret, "acpm_sicd_mif_down: %d\n", data);

	return ret;
}
#endif

static ssize_t (*flexpmu_debugfs_read_fptr[FID_MAX])(int, char *) = {
	FLEXPMU_DBG_FUNC_READ(cpu_status),
	FLEXPMU_DBG_FUNC_READ(seq_status),
	FLEXPMU_DBG_FUNC_READ(cur_sequence),
	FLEXPMU_DBG_FUNC_READ(sw_flag),
	FLEXPMU_DBG_FUNC_READ(seq_count),
	FLEXPMU_DBG_FUNC_READ(mif_always_on),
	FLEXPMU_DBG_FUNC_READ(lpm_count),
#ifdef CONFIG_SLEEP_STAT_EXYNOS
	FLEXPMU_DBG_FUNC_READ(sleep_stat),
#endif
};

static ssize_t exynos_flexpmu_dbg_read(struct file *file, char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct dbgfs_info *d2f = file->private_data;
	ssize_t ret;
	char buf[BUF_SIZE] = {0,};

	ret = flexpmu_debugfs_read_fptr[d2f->fid](d2f->fid, buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t exynos_flexpmu_dbg_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	struct dbgfs_info *f2d = file->private_data;
	ssize_t ret;
	char buf[32];

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	switch (f2d->fid) {
	case FID_MIF_ALWAYS_ON:
		if (buf[0] == '0') {
			__raw_writel(0, flexpmu_dbg_base +
					(DATA_LINE * DID_MIF_ALWAYS_ON) + 0xC);
		}
		if (buf[0] == '1') {
			__raw_writel(1, flexpmu_dbg_base +
					(DATA_LINE * DID_MIF_ALWAYS_ON) + 0xC);
		}
		break;
	default:
		break;
	}

	return ret;
}

static int exynos_flexpmu_dbg_probe(struct platform_device *pdev)
{
	int ret = 0;
	const __be32 *prop;
	unsigned int len = 0;
	unsigned int data_base = 0;
	unsigned int data_size = 0;
	int i;

	flexpmu_dbg_info = kzalloc(sizeof(struct dbgfs_info) * FID_MAX, GFP_KERNEL);
	if (!flexpmu_dbg_info) {
		pr_err("%s %s: can not allocate mem for flexpmu_dbg_info\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_flexpmu_info;
	}

	prop = of_get_property(pdev->dev.of_node, "data-base", &len);
	if (!prop) {
		pr_err("%s %s: can not read data-base in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -EINVAL;
		goto err_dbgfs_probe;
	}
	data_base = be32_to_cpup(prop);

	prop = of_get_property(pdev->dev.of_node, "data-size", &len);
	if (!prop) {
		pr_err("%s %s: can not read data-base in DT\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -EINVAL;
		goto err_dbgfs_probe;
	}
	data_size = be32_to_cpup(prop);

	if (data_base && data_size)
		flexpmu_dbg_base = ioremap(data_base, data_size);

	flexpmu_dbg_root = debugfs_create_dir("flexpmu-dbg", NULL);
	if (!flexpmu_dbg_root) {
		pr_err("%s %s: could not create debugfs root dir\n",
				EXYNOS_FLEXPMU_DBG_PREFIX, __func__);
		ret = -ENOMEM;
		goto err_dbgfs_probe;
	}

	for (i = 0; i < FID_MAX; i++) {
		flexpmu_dbg_info[i].fid = i;
		flexpmu_dbg_info[i].fops.open = simple_open;
		flexpmu_dbg_info[i].fops.read = exynos_flexpmu_dbg_read;
		flexpmu_dbg_info[i].fops.write = exynos_flexpmu_dbg_write;
		flexpmu_dbg_info[i].fops.llseek = default_llseek;
		flexpmu_dbg_info[i].den = debugfs_create_file(flexpmu_debugfs_name[i],
				0644, flexpmu_dbg_root, &flexpmu_dbg_info[i],
				&flexpmu_dbg_info[i].fops);
	}

	platform_set_drvdata(pdev, flexpmu_dbg_info);

	return 0;

err_dbgfs_probe:
	kfree(flexpmu_dbg_info);
err_flexpmu_info:
	return ret;
}

static int exynos_flexpmu_dbg_remove(struct platform_device *pdev)
{
	struct dbgfs_info *flexpmu_dbg_info = platform_get_drvdata(pdev);

	debugfs_remove_recursive(flexpmu_dbg_root);
	kfree(flexpmu_dbg_info);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id exynos_flexpmu_dbg_match[] = {
	{
		.compatible = "samsung,exynos-flexpmu-dbg",
	},
	{},
};

static struct platform_driver exynos_flexpmu_dbg_drv = {
	.probe		= exynos_flexpmu_dbg_probe,
	.remove		= exynos_flexpmu_dbg_remove,
	.driver		= {
		.name	= "exynos_flexpmu_dbg",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_flexpmu_dbg_match,
	},
};

static int __init exynos_flexpmu_dbg_init(void)
{
	return platform_driver_register(&exynos_flexpmu_dbg_drv);
}
late_initcall(exynos_flexpmu_dbg_init);
