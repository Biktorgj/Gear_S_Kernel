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

#define	VENDOR	"STM"
#define	CHIP_ID	"UVIS25"


/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/
/* sysfs for vendor & name */
static ssize_t uv_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t uv_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t uv_raw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->buf[UV_SENSOR].uv_raw);
}

static ssize_t uv_id_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct ssp_data *data = dev_get_drvdata(dev);
	struct ssp_msg *msg = kzalloc(sizeof(*msg), GFP_KERNEL);
	char buffer = 0;
	int iRet = 0;

	if (msg== NULL) {
		pr_err("[SSP] %s, failed to alloc memory for ssp_msg\n", __func__);
		goto exit;
	}
	msg->cmd = MSG2SSP_AP_GET_UV_DEVICE_ID;
	msg->length = 1;
	msg->options = AP2HUB_READ;
	msg->buffer = &buffer;
	msg->free_buffer = 0;

	iRet = ssp_spi_sync(data, msg, 1000);
	if (iRet != SUCCESS) {
		pr_err("[SSP]: %s Get UV Device ID Timeout!!\n", __func__);
		goto exit;
	}

	pr_info("[SSP]%s, UV Device ID = 0x%x\n", __func__, buffer);

	return snprintf(buf, PAGE_SIZE, "%x\n", buffer);
exit:
	return snprintf(buf, PAGE_SIZE, "%u\n", 0);
}


static DEVICE_ATTR(vendor, S_IRUGO, uv_vendor_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, uv_name_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, uv_raw_show, NULL);
static DEVICE_ATTR(id_check, S_IRUGO, uv_id_check_show, NULL);

static struct device_attribute *uv_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_raw_data,
	&dev_attr_id_check,
	NULL,
};

void initialize_uv_factorytest(struct ssp_data *data)
{
	sensors_register(data->uv_device, data, uv_attrs, "uv_sensor");
}

void remove_uv_factorytest(struct ssp_data *data)
{
	sensors_unregister(data->uv_device, uv_attrs);
}
