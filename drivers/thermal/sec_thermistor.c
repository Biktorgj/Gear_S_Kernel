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
#include <linux/sched.h>
#include <linux/hwmon-sysfs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/sec_thermistor.h>

#define ADC_SAMPLING_CNT	7

#ifdef CONFIG_SLP_KERNEL_ENG
#define SEC_THERM_DEBUG
#endif

#ifdef SEC_THERM_DEBUG
int debug_mode[SEC_THERM_NR_ADC];
int debug_temp[SEC_THERM_NR_ADC];
#endif

static int sec_therm_sampling_adc(struct sec_therm_info *info,
			int therm_id, int sampling_cnt, int *adc)
{
	int adc_data = 0;
	int adc_total = 0;
	int ret = 0;
	int i;

	if (unlikely(!info || !adc)) {
		pr_err("%s: Invalid args: NULL\n", __func__);
		return -EINVAL;
	}

	if (therm_id < 0 || therm_id >= SEC_THERM_NR_ADC) {
		pr_err("%s: Invalid therm_id:%d\n", __func__, therm_id);
		return -EINVAL;
	}

	mutex_lock(&info->therm_mutex);
	for (i = 0; i < sampling_cnt; i++) {
		ret = sec_therm_adc_read(info, therm_id, &adc_data);
		if (ret) {
			pr_err("%s: Failed to read adc(%d) \n", __func__, ret);
			mutex_unlock(&info->therm_mutex);
			return ret;
		}

		adc_total += adc_data;
	}
	mutex_unlock(&info->therm_mutex);

	adc_data = adc_total / sampling_cnt;
	if (adc_data < 0) {
		pr_err("%s: Invalid ADC value(%d) \n", __func__, adc_data);
		return -EINVAL;
	}

	*adc = adc_data;
	return 0;
}

static int sec_therm_adc_to_temper(struct sec_therm_info *info,
		int therm_id, int adc, int *temp)
{
	struct sec_therm_adc_info *adc_info = NULL;
	struct sec_therm_adc_table *temp_table = NULL;
	int temp_table_size = 0;
	int low = 0, high = 0, mid = 0, temp_data = 0;
	int i;

	if (therm_id < 0 || therm_id >= SEC_THERM_NR_ADC) {
		pr_err("%s: Invalid therm_id:%d\n", __func__, therm_id);
		return -EINVAL;
	}

	for (i = 0; i < info->adc_list_size; i++) {
		if (therm_id == info->adc_list[i].therm_id) {
			adc_info = &info->adc_list[i];
			break;
		}
	}

	temp_table = adc_info->temp_table;
	temp_table_size = adc_info->temp_table_size;

	if (unlikely(!temp_table || !temp_table_size || !temp)) {
		pr_err("%s: Invalid temp_table info\n", __func__);
		return -EINVAL;
	}

	high = temp_table_size - 1;

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

static struct sec_therm_info *g_info = NULL;
int sec_therm_get_adc(int therm_id, int *adc)
{
	int val = 0;
	int ret = 0;

	if (unlikely(!g_info)) {
		pr_err("%s: context is null\n", __func__);
		return -ENODEV;
	}

	if (unlikely(!adc)) {
		pr_err("%s: Invalid args\n", __func__);
		return -EINVAL;
	}

	ret = sec_therm_sampling_adc(g_info, therm_id, 1, &val);
	if (ret)
		return ret;

	*adc = val;
	pr_debug("%s: id:%d adc:%d\n", __func__, therm_id, *adc);
	return 0;
}

int sec_therm_get_temp(int therm_id, int *temp)
{
	int adc = 0, t = 0;
	int ret = 0;

	if (unlikely(!g_info)) {
		pr_err("%s: context is null\n", __func__);
		return -ENODEV;
	}

	if (unlikely(!temp)) {
		pr_err("%s: Invalid args\n", __func__);
		return -EINVAL;
	}

	ret = sec_therm_sampling_adc(g_info, therm_id, ADC_SAMPLING_CNT, &adc);
	if (ret)
		return ret;

	ret = sec_therm_adc_to_temper(g_info, therm_id, adc, &t);
	if (ret)
		return ret;

	*temp = t;
	pr_debug("%s: id:%d t:%d (adc:%d)\n", __func__, therm_id, t, adc);
	return 0;
}

static ssize_t sec_therm_show(struct device *dev,
				   struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int adc, temp;
	int ret = 0;

	if (!g_info || !attr)
		return -ENODEV;

	ret = sec_therm_sampling_adc(g_info, attr->index, ADC_SAMPLING_CNT, &adc);
	if (ret)
		return -EINVAL;

	ret = sec_therm_adc_to_temper(g_info, attr->index, adc, &temp);
	if (ret)
		return -EINVAL;

	if (strcmp(current->comm, "trm") != 0) {
		pr_info("%s: id:%d, temp:%d, adc:%d by %s(%d)\n",
			__func__, attr->index, temp/10, adc, current->comm, current->pid);
	}

#ifdef SEC_THERM_DEBUG
	if (debug_mode[attr->index])
		temp = debug_temp[attr->index];
#endif

	return sprintf(buf, "temp:%d adc:%d\n", temp/10, adc);
}

#ifdef SEC_THERM_DEBUG
static ssize_t sec_therm_debug_store(struct device *dev,
				   struct device_attribute *devattr, const char *buf, size_t cnt)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int temp;

	if (!g_info || !attr)
		return -ENODEV;

	sscanf(buf, "%d", &temp);

	pr_info("%s: Set therm_id %d temp to %d\n", __func__, attr->index, temp);
	debug_temp[attr->index] = temp;
	debug_mode[attr->index] = 1;
	return cnt;
}
#endif

struct device *temperature_device;

static int sec_therm_probe(struct platform_device *pdev)
{
	struct sec_therm_info *info = NULL;
	int ret = 0;
	int i;

	pr_info("%s: +++\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct sec_therm_info), GFP_KERNEL);
	if (!info) {
		pr_err("%s: Failed to allocate context mem\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;
	g_info = info;

	mutex_init(&info->therm_mutex);

	ret = sec_therm_adc_init(pdev);
	if (ret) {
		pr_err("%s: Failed to init adc dev (%d)\n", __func__, ret);
		goto err_kfree;
	}

	/* Create sysfs node */
	if (temperature_device) {
		struct sensor_device_attribute *attrs = devm_kzalloc(&pdev->dev,
				sizeof(struct sensor_device_attribute) * info->adc_list_size,
				GFP_KERNEL);
		if (!attrs) {
			pr_err("%s: Failed to allocate attrs mem\n", __func__);
			ret = -ENOMEM;
			goto err_release_adc;
		}

		for (i = 0; i < info->adc_list_size; i++) {
			attrs[i].index = info->adc_list[i].therm_id;
			attrs[i].dev_attr.attr.name = info->adc_list[i].name;
			attrs[i].dev_attr.show = sec_therm_show;
		#ifdef SEC_THERM_DEBUG
			attrs[i].dev_attr.store = sec_therm_debug_store;
			attrs[i].dev_attr.attr.mode = S_IRUGO | S_IWUGO;
		#else
			attrs[i].dev_attr.store = NULL;
			attrs[i].dev_attr.attr.mode = S_IRUGO;
		#endif
			sysfs_attr_init(&attrs[i].dev_attr.attr);

			ret = device_create_file(temperature_device, &attrs[i].dev_attr);
			if (ret)
				pr_err("%s: Failed to create dev_attr-%d (%d)\n", __func__, i, ret);
		}

		info->sec_therm_attrs = attrs;
	}

	return 0;

err_release_adc:
	sec_therm_adc_exit(pdev);
err_kfree:
	devm_kfree(&pdev->dev, info);
	return ret;
}

static int sec_therm_remove(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);
	int i;

	if (!info)
		return 0;

	if (info->sec_therm_attrs) {
		for (i = 0; i < info->adc_list_size; i++)
			device_remove_file(&pdev->dev, &info->sec_therm_attrs[i].dev_attr);
		devm_kfree(&pdev->dev, info->sec_therm_attrs);
	}

	sec_therm_adc_exit(pdev);
	devm_kfree(&pdev->dev, info);
	return 0;
}

static const struct of_device_id sec_therm_match_table[] = {
	{	.compatible = "sec-thermistor",
	},
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

extern struct class *sec_class;
static int __init temperature_device_init(void)
{
	if (sec_class) {
		temperature_device = device_create(sec_class,
					NULL, 0, NULL, "temperature");
		if (IS_ERR(temperature_device)) {
			pr_err("%s: Failed to create device(temperature)!\n", __func__);
			return -ENODEV;
		}
	}

	return 0;
}
fs_initcall(temperature_device_init);

MODULE_AUTHOR("Tizen");
MODULE_DESCRIPTION("sec thermistor driver");
MODULE_LICENSE("GPL");
