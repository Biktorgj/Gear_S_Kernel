/* board-8226-thermistor.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/gfp.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/sec_thermistor.h>
#include <linux/qpnp/qpnp-adc.h>

/* tunning data based on R750 rev0.4 */
static struct sec_therm_adc_table temp_table_ap[] = {
	/* adc, temperature */
	{25762,  900},
	{25970,  850},
	{26235,  800},
	{26513,  750},
	{26856,  700},
	{27256,  650},
	{27699,  600},
	{28231,  550},
	{28873,  500},
	{29581,  450},
	{30401,  400},
	{31292,  350},
	{32309,  300},
	{33266,  250},
	{34358,  200},
	{35424,  150},
	{36502,  100},
	{37579,   50},
	{37799,   40},
	{38003,   30},
	{38177,   20},
	{38352,   10},
	{38544,    0},
	{38748,  -10},
	{38928,  -20},
	{39118,  -30},
	{39305,  -40},
	{39467,  -50},
	{40221, -100},
	{40860, -150},
	{41375, -200},
	{41790, -250},
	{42097, -300},
};

/* tunning data based on R750 rev0.4 */
static struct sec_therm_adc_table temp_table_battery[] = {
	/* adc, temperature */
	{25813,  900},
	{26021,  850},
	{26286,  800},
	{26563,  750},
	{26907,  700},
	{27310,  650},
	{27759,  600},
	{28295,  550},
	{28943,  500},
	{29653,  450},
	{30478,  400},
	{31379,  350},
	{32390,  300},
	{33345,  250},
	{34434,  200},
	{35492,  150},
	{36565,  100},
	{37641,   50},
	{37858,   40},
	{38062,   30},
	{38233,   20},
	{38408,   10},
	{38599,    0},
	{38801,  -10},
	{38977,  -20},
	{39169,  -30},
	{39356,  -40},
	{39514,  -50},
	{40258, -100},
	{40896, -150},
	{41404, -200},
	{41820, -250},
	{42127, -300},
};

/* Need tunning */
static struct sec_therm_adc_table temp_table_pam0[] = {
	/* adc, temperature */
	{25762,  900},
	{25970,  850},
	{26235,  800},
	{26513,  750},
	{26856,  700},
	{27256,  650},
	{27699,  600},
	{28231,  550},
	{28873,  500},
	{29581,  450},
	{30401,  400},
	{31292,  350},
	{32309,  300},
	{33266,  250},
	{34358,  200},
	{35424,  150},
	{36502,  100},
	{37579,   50},
	{37799,   40},
	{38003,   30},
	{38177,   20},
	{38352,   10},
	{38544,    0},
	{38748,  -10},
	{38928,  -20},
	{39118,  -30},
	{39305,  -40},
	{39467,  -50},
	{40221, -100},
	{40860, -150},
	{41375, -200},
	{41790, -250},
	{42097, -300},
};

static struct sec_therm_adc_table temp_table_default[] = {
	/* adc, temperature */
	{25762,  900},
	{25970,  850},
	{26235,  800},
	{26513,  750},
	{26856,  700},
	{27256,  650},
	{27699,  600},
	{28231,  550},
	{28873,  500},
	{29581,  450},
	{30401,  400},
	{31292,  350},
	{32309,  300},
	{33266,  250},
	{34358,  200},
	{35424,  150},
	{36502,  100},
	{37579,   50},
	{37799,   40},
	{38003,   30},
	{38177,   20},
	{38352,   10},
	{38544,    0},
	{38748,  -10},
	{38928,  -20},
	{39118,  -30},
	{39305,  -40},
	{39467,  -50},
	{40221, -100},
	{40860, -150},
	{41375, -200},
	{41790, -250},
	{42097, -300},
};

static int sec_therm_temp_table_init(struct sec_therm_adc_info *adc_info)
{
	if (unlikely(!adc_info))
		return -EINVAL;

	switch (adc_info->therm_id) {
		case SEC_THERM_AP:
			adc_info->temp_table = temp_table_ap;
			adc_info->temp_table_size = ARRAY_SIZE(temp_table_ap);
			break;
		case SEC_THERM_BATTERY:
			adc_info->temp_table = temp_table_battery;
			adc_info->temp_table_size = ARRAY_SIZE(temp_table_battery);
			break;
		case SEC_THERM_PAM0:
			adc_info->temp_table = temp_table_pam0;
			adc_info->temp_table_size = ARRAY_SIZE(temp_table_pam0);
			break;
		case SEC_THERM_XO:
			adc_info->temp_table = temp_table_default;
			adc_info->temp_table_size = ARRAY_SIZE(temp_table_default);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int sec_therm_parse_dt(struct device_node *node,
			struct sec_therm_adc_info *adc_list)
{
	struct device_node *child = NULL;
	int i = 0, ret;

	for_each_child_of_node(node, child) {
		int therm_id, therm_adc_ch;
		const char *therm_name, *therm_adc_name;

		therm_name = child->name;
		if (!therm_name) {
			pr_err("%s: Failed to get thermistor name\n", __func__);
			return -EINVAL;
		}

		ret = of_property_read_u32(child, "sec,therm-id", &therm_id);
		if (ret) {
			pr_err("%s: Failed to get thermistor id\n", __func__);
			return ret;
		}

		therm_adc_name = of_get_property(child, "sec,therm-adc-name", NULL);
		if (!therm_adc_name) {
			pr_err("%s: Failed to get adc name\n", __func__);
			return -EINVAL;
		}

		ret = of_property_read_u32(child, "sec,therm-adc-ch", &therm_adc_ch);
		if (ret) {
			pr_err("%s: Failed to get thermistor adc channel\n", __func__);
			return ret;
		}

		pr_info("%s: name:%s, therm_id:%d, adc_name:%s, adc_ch:0x%x\n",
				__func__, therm_name, therm_id, therm_adc_name, therm_adc_ch);

		adc_list[i].name = therm_name;
		adc_list[i].therm_id = therm_id;
		adc_list[i].adc_name = therm_adc_name;
		adc_list[i].adc_ch = therm_adc_ch;
		i++;
	}

	return 0;
}

int sec_therm_adc_read(struct sec_therm_info *info, int therm_id, int *val)
{
	struct sec_therm_adc_info *adc_info = NULL;
	struct qpnp_vadc_result result;
	int i, ret = 0;

	if (unlikely(!info || !val))
		return -EINVAL;

	for (i = 0; i < info->adc_list_size; i++) {
		if (therm_id == info->adc_list[i].therm_id) {
			adc_info = &info->adc_list[i];
			break;
		}
	}

	if (!adc_info) {
		pr_err("%s: Failed to found therm_id %d\n", __func__, therm_id);
		return -EINVAL;
	}

	ret = qpnp_vadc_read(adc_info->adc_client, adc_info->adc_ch, &result);
	if (ret) {
		pr_err("%s: Failed to read adc channel 0x%02x (%d)\n",
				__func__, adc_info->adc_ch, ret);
		return -EINVAL;
	}

	*val = result.adc_code;
	return 0;
}

int sec_therm_adc_init(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);
	struct sec_therm_adc_info *adc_list = NULL;
	int adc_list_size = 0;
	int i, ret = 0;

	/* device tree support */
	if (pdev->dev.of_node) {
		struct device_node *node = pdev->dev.of_node;
		struct device_node *child;

		for_each_child_of_node(node, child)
			adc_list_size++;

		if (adc_list_size <= 0) {
			pr_err("%s: No adc channel info\n", __func__);
			return -ENODEV;
		}

		adc_list = devm_kzalloc(&pdev->dev,
				sizeof(struct sec_therm_adc_info) * adc_list_size, GFP_KERNEL);
		if (!adc_list) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}

		ret = sec_therm_parse_dt(node, adc_list);
		if (ret) {
			pr_err("%s: Failed to parse dt (%d)\n", __func__, ret);
			goto err;
		}

		for (i = 0; i < adc_list_size; i++) {
			ret = sec_therm_temp_table_init(&adc_list[i]);
			if (ret) {
				pr_err("%s: Failed to init %d adc_temp_table\n",
						__func__, adc_list[i].therm_id);
				goto err;
			}

			adc_list[i].adc_client = qpnp_get_vadc(info->dev, adc_list[i].adc_name);
			if (IS_ERR_OR_NULL(adc_list[i].adc_client)) {
				pr_err("%s: Failed to get %d vadc (%ld)\n", __func__,
						adc_list[i].therm_id, PTR_ERR(adc_list[i].adc_client));
				goto err;
			}
		}
	} else {
		pr_err("%s: Failed to find device tree info\n", __func__);
		return -ENODEV;
	}

	info->adc_list = adc_list;
	info->adc_list_size = adc_list_size;

	return 0;

err:
	devm_kfree(&pdev->dev, adc_list);
	return ret;
}

void sec_therm_adc_exit(struct platform_device *pdev)
{
	struct sec_therm_info *info = platform_get_drvdata(pdev);

	if (!info)
		return;

	if (pdev->dev.of_node && info->adc_list)
		devm_kfree(&pdev->dev, info->adc_list);
}
