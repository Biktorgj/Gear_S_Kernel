/*
 * Copyright (C) 2014 Samsung Electronics, Inc.
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

#ifndef __SEC_THERMISTOR_H__
#define __SEC_THERMISTOR_H__  __FILE__

#include <linux/platform_device.h>

enum {
	SEC_THERM_AP = 0,
	SEC_THERM_BATTERY,
	SEC_THERM_PAM0,
	SEC_THERM_PAM1,
	SEC_THERM_XO,
	SEC_THERM_FLASH,
	SEC_THERM_NR_ADC,
};

struct sec_therm_adc_table {
	int adc;
	int temperature;
};

struct sec_therm_adc_info {
	int therm_id;
	const char *name;
	const char *adc_name;
	int adc_ch;
	void *adc_client;

	struct sec_therm_adc_table *temp_table;
	int temp_table_size;
};

struct sec_therm_info {
	struct device *dev;
	struct mutex therm_mutex;
	struct sec_therm_adc_info *adc_list;
	int adc_list_size;

	struct sensor_device_attribute *sec_therm_attrs;
};

#ifdef CONFIG_SEC_THERMISTOR
int sec_therm_get_adc(int therm_id, int *adc);
int sec_therm_get_temp(int therm_id, int *temp);
int sec_therm_adc_read(struct sec_therm_info *info, int therm_id, int *val);
int sec_therm_adc_init(struct platform_device *pdev);
void sec_therm_adc_exit(struct platform_device *pdev);
#else
static inline int sec_therm_get_adc(int therm_id, int *adc)
{
	return -ENODEV;
}
static inline int sec_therm_get_temp(int therm_id, int *temp)
{
	return -ENODEV;
}

static inline int sec_therm_adc_read(struct sec_therm_info *info, int therm_id, int *val)
{
	return -ENODEV;
}

static inline int sec_therm_adc_init(struct platform_device *pdev)
{
	return -ENODEV;
}

static inline void sec_therm_adc_exit(struct platform_device *pdev)
{
	return;
}
#endif /* CONFIG_SEC_THERMISTOR */

#endif /* __SEC_THERMISTOR_H__ */
