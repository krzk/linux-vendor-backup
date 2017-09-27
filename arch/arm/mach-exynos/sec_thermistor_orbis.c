/* sec_thermistor.c
 *
 * Copyright (C) 2014 Samsung Electronics
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/hwmon-sysfs.h>
#include <linux/sec_thermistor.h>

#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#ifdef CONFIG_SLP_KERNEL_ENG
static int debug_enabled[SEC_THERM_ADC_MAX];
static int dummy_temp[SEC_THERM_ADC_MAX];
#endif

#define ADC_SAMPLING_CNT	7

static struct device *temperature_dev;
static struct sec_therm_info *g_info;

extern struct class *sec_class;
extern int __sec_therm_adc_dev_read(struct sec_therm_adc_info *adc, int *val);
extern int sec_therm_temp_table_init(struct sec_therm_info *info);

static inline struct sec_therm_adc_info *sec_therm_match_adc(
		struct sec_therm_info *info, int therm_id)
{
	int i;

	if (therm_id < 0 || therm_id >= SEC_THERM_ADC_MAX)
		return NULL;

	for (i = 0; i < info->adc_list_size; i++) {
		if (therm_id == info->adc_list[i].therm_id)
			return &info->adc_list[i];
	}

	return NULL;
}

static int sec_therm_adc_read(struct sec_therm_adc_info *adc_info,
		int sampling_cnt, int *adc)
{
	int adc_data, adc_total = 0;
	int i, ret = -1;

	for (i = 0; i < sampling_cnt; i++) {
		ret = __sec_therm_adc_dev_read(adc_info, &adc_data);
		if (ret)
			break;

		if (adc_data < 0) {
			pr_err("%s: Invalid adc value (%d) \n", __func__, adc_data);
			return -EINVAL;
		}

		adc_total += adc_data;
	}
	if (ret) {
		pr_err("%s: Failed to read adc dev (%d) \n", __func__, ret);
		return ret;
	}

	*adc = adc_total / sampling_cnt;
	return 0;
}

static int sec_therm_adc_to_temper(struct sec_therm_adc_info *adc_info,
		int adc, int *temp)
{
	struct sec_therm_adc_table *temp_table;
	int low = 0, high = 0, mid = 0, temp_data = 0;

	if (unlikely(!adc_info->temp_table || adc_info->temp_table_size <= 0))
		return -EINVAL;

	temp_table = adc_info->temp_table;
	high = adc_info->temp_table_size - 1;

	if (adc >= temp_table[high].adc) {
		*temp = temp_table[high].temperature;
		return 0;
	} else if (adc <= temp_table[low].adc) {
		*temp = temp_table[low].temperature;
		return 0;
	}

	while (low <= high) {
		mid = (low + high) / 2;

		if (adc < temp_table[mid].adc) {
			high = mid - 1;
		} else if (adc > temp_table[mid].adc) {
			low = mid + 1;
		} else {
			*temp = temp_table[mid].temperature;
			return 0;
		}
	}
	temp_data = temp_table[low].temperature;

	/* Interpolation */
	temp_data += ((temp_table[high].temperature - temp_table[low].temperature)
			* (temp_table[low].adc - adc))
			/ (temp_table[low].adc - temp_table[high].adc);

	*temp = temp_data;
	return 0;
}

int sec_therm_get_adc_value(int therm_id, int *adc_val)
{
	struct sec_therm_adc_info *adc_info;
	int val, ret;

	if (unlikely(!g_info)) {
		pr_err("%s: context is null\n", __func__);
		return -ENODEV;
	}

	if (!adc_val) {
		pr_err("%s: Invalid args\n", __func__);
		return -EINVAL;
	}

	adc_info = sec_therm_match_adc(g_info, therm_id);
	if (!adc_info) {
		pr_err("%s: Not found adc: %d\n", __func__, therm_id);
		return -EINVAL;
	}

	ret = sec_therm_adc_read(adc_info, 1, &val);
	if (ret)
		return ret;

	*adc_val = val;
	pr_debug("%s: id:%d adc:%d\n", __func__, therm_id, *adc_val);

	return 0;
}
EXPORT_SYMBOL(sec_therm_get_adc_value);

int sec_therm_get_temp(int therm_id, int *temp)
{
	struct sec_therm_adc_info *adc_info;
	int adc_val, t;
	int ret;

	if (unlikely(!g_info)) {
		pr_err("%s: context is null\n", __func__);
		return -ENODEV;
	}

	if (!temp) {
		pr_err("%s: Invalid args\n", __func__);
		return -EINVAL;
	}

	adc_info = sec_therm_match_adc(g_info, therm_id);
	if (!adc_info) {
		pr_err("%s: Not found adc: %d\n", __func__, therm_id);
		return -EINVAL;
	}

	ret = sec_therm_adc_read(adc_info, ADC_SAMPLING_CNT, &adc_val);
	if (ret)
		return ret;

	ret = sec_therm_adc_to_temper(adc_info, adc_val, &t);
	if (ret)
		return ret;

	*temp = t;
	pr_debug("%s: id:%d t:%d (adc:%d)\n", __func__, therm_id, t, adc_val);

	return 0;
}
EXPORT_SYMBOL(sec_therm_get_temp);

int tmu_temp_table[2][100] = {{0,},};

static ssize_t sec_therm_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct sec_therm_info *info = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct sec_therm_adc_info *adc_info;
	int adc, temp;
	int ret;
	static int index = 0;

	adc_info = sec_therm_match_adc(info, attr->index);
	if (!adc_info) {
		pr_err("%s: Not found adc: %d\n", __func__, attr->index);
		return -EINVAL;
	}

	ret = sec_therm_adc_read(adc_info, ADC_SAMPLING_CNT, &adc);
	if (ret)
		return ret;

	ret = sec_therm_adc_to_temper(adc_info, adc, &temp);
	if (ret)
		return ret;

	tmu_temp_table[0][index] = adc;
	tmu_temp_table[1][index] = temp/10;

	index++;

	if (index >= 100)
			index = 0;

#ifdef CONFIG_SLP_KERNEL_ENG
	if (debug_enabled[attr->index])
		temp = dummy_temp[attr->index];
#endif

	return sprintf(buf, "temp:%d adc:%d\n", temp/10, adc);
}

#ifdef CONFIG_SLP_KERNEL_ENG
static ssize_t sec_therm_debug_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t cnt)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int temp;

	if (sscanf(buf, "%d", &temp) < 0)
		return -EINVAL;

	debug_enabled[attr->index] = 1;
	dummy_temp[attr->index] = temp;

	pr_info("%s: Set therm_id %d temp to %d\n", __func__, attr->index, temp);
	return cnt;
}
#endif

static int sec_therm_parse_dt(struct device_node *adc_node,
		struct sec_therm_adc_info *adc)
{
	int ret;

	ret = of_property_read_u32(adc_node, "sec,therm-id", &adc->therm_id);
	if (ret) {
		pr_err("%s: Failed to get thermistor id: %d\n", __func__, ret);
		return ret;
	}

	ret = of_property_read_string(adc_node, "sec,therm-name", &adc->name);
	if (ret) {
		if (adc_node->name) {
			adc->name = adc_node->name;
		} else {
			pr_err("%s: Failed to get adc name: %d\n", __func__, ret);
			return -EINVAL;
		}
	}

	ret = of_property_read_string(adc_node, "sec,therm-adc-name", &adc->adc_name);
	if (ret) {
		pr_err("%s: Failed to get adc name: %d\n", __func__, ret);
		return -EINVAL;
	}

	ret = of_property_read_u32(adc_node, "sec,therm-adc-ch", &adc->adc_ch);
	if (ret) {
		pr_err("%s: Failed to get adc channel: %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s: id:%d, name:%s, adc-name:%s, ch:%d\n", __func__, adc->therm_id,
			adc->name, adc->adc_name, adc->adc_ch);

	return 0;
}

#ifdef CONFIG_SLEEP_MONITOR
#define MAX_LOG_CNT	(4)

static int sec_therm_get_temp_cb(void *priv, unsigned int *raw, int chk_lv, int caller_type)
{
	struct sec_therm_info *info;
	unsigned int curr_temp;
	int temp, limit, ret, i, pretty = 0;

	if (unlikely(!priv)) {
		pr_err("%s: context is null\n", __func__);
		return -ENODEV;
	}

	info = (struct sec_therm_info *)priv;
	*raw = 0;

	for (i = 0; ((i < info->adc_list_size) && (i < MAX_LOG_CNT)); i++) {
		ret = sec_therm_get_temp(info->adc_list[i].therm_id, &temp);
		if (ret) {
			pr_err("%s: get temp fail for %s\n",
				__func__, info->adc_list[i].name);
			pretty |= (1 << i);
			continue;
		}

		curr_temp = (unsigned int)(temp/10);
		limit = info->adc_list[i].temp_limit;

		if (curr_temp > 0xFF) {
			pr_warn("%s: temp exceed MAX %s/%d\n",
				__func__, info->adc_list[i].name, (temp/10));
			pretty |= (1 << i);
			*raw = *raw | (0xFF << (i * 8));
			continue;
		}

		if ((limit != 0) && (curr_temp > limit))
			pretty |= (1 << i);

		*raw = *raw | (curr_temp << (i * 8));
	}

	return pretty;
}

static struct sleep_monitor_ops therm_slp_mon_ops = {
		.read_cb_func = sec_therm_get_temp_cb,
};

static void sec_therm_slp_mon_enable(struct sec_therm_info *info)
{
	int ret;

	ret = sleep_monitor_register_ops(info,
			&therm_slp_mon_ops, SLEEP_MONITOR_TEMP);
	if (ret)
		pr_err("%s: Failed to slp_mon register(%d)\n",
				__func__, ret);

	return;
}

static void sec_therm_slp_mon_disable(void)
{
	int ret;

	ret = sleep_monitor_unregister_ops(SLEEP_MONITOR_TEMP);
	if (ret)
		pr_err("%s: Failed to slp_mon unregister(%d)\n",
				__func__, ret);

	return;
}
#else
static inline void sec_therm_slp_mon_enable(struct sec_therm_info *info) { }
static inline void sec_therm_slp_mon_disable(void) { }
#endif

static __devinit int sec_therm_probe(struct platform_device *pdev)
{
	struct sec_therm_info *info;
	struct sec_therm_adc_info *adc_list;
	int adc_list_size = 0;
	int ret, i = 0;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: Failed to alloc info\n", __func__);
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		struct device_node *adc_node;

		for_each_child_of_node(pdev->dev.of_node, adc_node)
			adc_list_size++;

		if (adc_list_size <= 0) {
			pr_err("%s: No adc info\n", __func__);
			ret = -ENODEV;
			goto err;
		}

		adc_list = devm_kzalloc(&pdev->dev,
				sizeof(*adc_list) * adc_list_size, GFP_KERNEL);
		if (!adc_list) {
			pr_err("%s: Failed to alloc adc_list\n", __func__);
			ret = -ENOMEM;
			goto err;
		}

		for_each_child_of_node(pdev->dev.of_node, adc_node) {
			ret = sec_therm_parse_dt(adc_node, &adc_list[i]);
			if (ret) {
				pr_err("%s: Failed to parse dt: %d (%d)\n", __func__, i, ret);
				ret = -EINVAL;
				goto err;
			}
			i++;
		}
	} else {
		struct sec_therm_platform_data *pdata = dev_get_platdata(&pdev->dev);

		if (!pdata || !pdata->adc_list || pdata->adc_list_size <= 0) {
			pr_err("%s: Failed to init adc_list from pdata\n", __func__);
			ret = -EINVAL;
			goto err;
		}

		adc_list = pdata->adc_list;
		adc_list_size = pdata->adc_list_size;

		for (i = 0; i < adc_list_size; i++) {
			pr_err("%s: id:%d, name:%s, ch:%d\n", __func__, adc_list[i].therm_id,
					adc_list[i].name, adc_list[i].adc_ch);
		}
	}

	info->adc_list = adc_list;
	info->adc_list_size = adc_list_size;
	info->dev = &pdev->dev;
	g_info = info;

	ret = sec_therm_adc_init(info);
	if (ret) {
		pr_err("%s: Failed to init adc dev (%d)\n", __func__, ret);
		goto err;
	}

	ret = sec_therm_temp_table_init(info);
	if (ret) {
		pr_err("%s: Failed to init temp table: %d (%d)\n", __func__, i, ret);
		goto err_adc;
	}

	mutex_init(&info->therm_mutex);
	platform_set_drvdata(pdev, info);

	/* Create sysfs node */
	if (temperature_dev) {
		struct sensor_device_attribute *attrs = devm_kzalloc(&pdev->dev,
				sizeof(*attrs) * info->adc_list_size, GFP_KERNEL);
		if (!attrs) {
			pr_err("%s: Failed to allocate attrs mem\n", __func__);
			ret = -ENOMEM;
			goto err_adc;
		}

		for (i = 0; i < info->adc_list_size; i++) {
			attrs[i].index = info->adc_list[i].therm_id;
			attrs[i].dev_attr.attr.name = info->adc_list[i].name;
			attrs[i].dev_attr.show = sec_therm_show;
		#ifdef CONFIG_SLP_KERNEL_ENG
			attrs[i].dev_attr.store = sec_therm_debug_store;
			attrs[i].dev_attr.attr.mode = S_IRUGO | S_IWUGO;
		#else
			attrs[i].dev_attr.store = NULL;
			attrs[i].dev_attr.attr.mode = S_IRUGO;
		#endif
			sysfs_attr_init(&attrs[i].dev_attr.attr);

			ret = device_create_file(temperature_dev, &attrs[i].dev_attr);
			if (ret)
				pr_err("%s: Failed to create attr: %d (%d)\n", __func__, i, ret);
		}
		info->sec_therm_attrs = attrs;
		dev_set_drvdata(temperature_dev, info);
	}

	sec_therm_slp_mon_enable(info);

	return 0;

err_adc:
	sec_therm_adc_exit(info);

err:
	if (pdev->dev.of_node && info->adc_list)
		devm_kfree(&pdev->dev, info->adc_list);

	devm_kfree(&pdev->dev, info);
	return ret;
}

static int sec_therm_remove(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);
	int i;

	sec_therm_slp_mon_disable();
	sec_therm_adc_exit(info);

	if (pdev->dev.of_node && info->adc_list)
		devm_kfree(&pdev->dev, info->adc_list);

	if (info->sec_therm_attrs) {
		for (i = 0; i < info->adc_list_size; i++)
			device_remove_file(&pdev->dev, &info->sec_therm_attrs[i].dev_attr);
		devm_kfree(&pdev->dev, info->sec_therm_attrs);
	}

	devm_kfree(&pdev->dev, info);
	return 0;
}

static const struct of_device_id sec_therm_match_table[] = {
	{ .compatible = "sec-thermistor", },
};

static struct platform_driver sec_thermistor_driver = {
	.driver = {
		   .name = "sec-thermistor",
		   .owner = THIS_MODULE,
		   .of_match_table = sec_therm_match_table,
	},
	.probe = sec_therm_probe,
	.remove = sec_therm_remove,
};

static int __init sec_therm_init(void)
{
	return platform_driver_register(&sec_thermistor_driver);
}
late_initcall(sec_therm_init);

static void __exit sec_therm_exit(void)
{
	platform_driver_unregister(&sec_thermistor_driver);
}
module_exit(sec_therm_exit);

static int __init sec_therm_temperature_dev_init(void)
{
	if (sec_class) {
		temperature_dev = device_create(sec_class, NULL, 0, NULL, "temperature");
		if (IS_ERR(temperature_dev)) {
			pr_err("%s: Failed to create temperature dev\n", __func__);
			return -ENODEV;
		}
	}
	return 0;
}
fs_initcall(sec_therm_temperature_dev_init);

MODULE_AUTHOR("tyung.kim@samsung.com");
MODULE_DESCRIPTION("sec thermistor driver");
MODULE_LICENSE("GPL");
