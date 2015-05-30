/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include "../ssp.h"

#define	VENDOR		"LITEON"
#define	CHIP_ID		"AL3320"

#define SNS_DD_ALSPRX_ALPHA_1                 512
#define SNS_DD_ALSPRX_ALPHA_2                 128
#define SNS_DD_ALSPRX_ALPHA_3                 320
#define SNS_DD_ALSPRX_ALPHA_4                 10
#define SNS_DD_ALSPRX_ALPHA_5                 1530
#define SNS_DD_ALSPRX_ALPHA_6                 380
#define SNS_DD_ALSPRX_ALPHA_7                 96
#define SNS_DD_ALSPRX_ALPHA_8                 30
#define SNS_DD_ALS_FACTOR                     1
#define SNS_DD_VISIBLE_TRANS_RATIO            5
#define SNS_DD_LUX_SCALE_FACTOR			7200
/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
#define MAX_LUX		135000

uint32_t light_get_lux(uint16_t raw_data)
{
	uint64_t lux_64;
#if 0
	uint32_t als_conversion = (SNS_DD_ALSPRX_ALPHA_2 *
		(uint32_t) SNS_DD_ALS_FACTOR) /
		(SNS_DD_VISIBLE_TRANS_RATIO * 10);

	lux_64 = (uint64_t)raw_data * als_conversion;
#else
	if (raw_data > 3)
		lux_64 = (uint64_t)((raw_data * SNS_DD_LUX_SCALE_FACTOR) / 10000);
	else
		lux_64 = 0;
#endif
	return (uint32_t)((lux_64 > 0xFFFFFFFF) ? 0xFFFFFFFF : lux_64);
}

static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->lux);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	u16 raw_lux = ((data->buf[LIGHT_SENSOR].raw_high << 8) |
		(data->buf[LIGHT_SENSOR].raw_low));

	return snprintf(buf, PAGE_SIZE, "%u\n", raw_lux);
}

static DEVICE_ATTR(vendor, S_IRUGO, light_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, light_name_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

void initialize_light_factorytest(struct ssp_data *data)
{
	sensors_register(data->light_device, data, light_attrs, "light_sensor");
}

void remove_light_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->light_device, light_attrs);
}
