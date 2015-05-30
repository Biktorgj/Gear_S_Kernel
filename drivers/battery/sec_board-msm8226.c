/*
 *  sec_board-msm8974.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charging_common.h>

extern int current_cable_type;
/* FG Max 17048 is not up yet, hence we set fixed SOC of 60%
 * This code would be removed once the Fuel Gauge is also
 * integrated
 */
#if 0
#if defined(CONFIG_FUELGAUGE_MAX17048)
static struct battery_data_t samsung_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{

		.RCOMP0 = 0x73,
		.RCOMP_charging = 0x8D,
		.temp_cohot = -1000,
		.temp_cocold = -4350,
		.is_using_model_data = true,
		.type_str = "SDI",
	}
};
#endif
#endif

static void sec_bat_adc_ap_init(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
}
/* FG Max 17048 is not up yet, hence we set fixed SOC of 60%
 * This code would be removed once the Fuel Gauge is also
 * integrated
 */
#if 0
static int sec_bat_adc_ap_read(struct sec_battery_info *battery, int channel)
{
	int data = -1;

	switch (channel)
	{
	case SEC_BAT_ADC_CHANNEL_TEMP :
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = 33000;
		break;
	default :
		break;
	}

	return data;
}
#endif

#if 0
static void sec_bat_adc_ap_exit(void)
{
}
#endif

static void sec_bat_adc_none_init(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
}

#if 0
static int sec_bat_adc_none_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}
#endif

#if 0
static void sec_bat_adc_none_exit(void)
{
}

#endif
static void sec_bat_adc_ic_init(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
}


#if 0
static int sec_bat_adc_ic_read(struct sec_battery_info *battery, int channel)
{
	return 0;
}
#endif

#if 0
static void sec_bat_adc_ic_exit(void)
{
}
#endif


#if 0
static int adc_read_type(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		adc = sec_bat_adc_none_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		adc = sec_bat_adc_ap_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		adc = sec_bat_adc_ic_read(battery, channel);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
	pr_debug("[%s] ADC = %d\n", __func__, adc);
	return adc;
}
#endif

static void adc_init_type(struct platform_device *pdev,
			  struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_init(pdev, battery);
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}

#if 0
static void adc_exit_type(struct sec_battery_info *battery)
{
	switch (battery->pdata->temp_adc_type)
	{
	case SEC_BATTERY_ADC_TYPE_NONE :
		sec_bat_adc_none_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_AP :
		sec_bat_adc_ap_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_IC :
		sec_bat_adc_ic_exit();
		break;
	case SEC_BATTERY_ADC_TYPE_NUM :
		break;
	default :
		break;
	}
}
#endif

#if 0
int adc_read(struct sec_battery_info *battery, int channel)
{
	int adc = 0;

	adc = adc_read_type(battery, channel);

	pr_debug("[%s]adc = %d\n", __func__, adc);

	return adc;
}

void adc_exit(struct sec_battery_info *battery, int channel)
{
	adc_exit_type(battery);
}
#endif

bool sec_bat_check_jig_status(void)
{
	return false;
}
/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
bool sec_bat_check_callback(struct sec_battery_info *battery)
{
	return true;
}

bool sec_bat_check_cable_result_callback(
		int cable_type)
{
	return true;
}

int sec_bat_check_cable_callback(struct sec_battery_info *battery)
{
#if 0
	union power_supply_propval value;
	msleep(750);

	if (battery->pdata->ta_irq_gpio == 0) {
		pr_err("%s: ta_int_gpio is 0 or not assigned yet(cable_type(%d))\n",
			__func__, current_cable_type);
	} else {
		if (current_cable_type == POWER_SUPPLY_TYPE_BATTERY &&
			!gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS IN\n", __func__);
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			return POWER_SUPPLY_TYPE_UARTOFF;
		}

		if ((current_cable_type == POWER_SUPPLY_TYPE_UARTOFF ||
			current_cable_type == POWER_SUPPLY_TYPE_CARDOCK) &&
			gpio_get_value_cansleep(battery->pdata->ta_irq_gpio)) {
			pr_info("%s : VBUS OUT\n", __func__);
			psy_do_property("battery", set, POWER_SUPPLY_PROP_ONLINE, value);
			return POWER_SUPPLY_TYPE_BATTERY;
		}
	}
#endif
	return current_cable_type;
}

void board_battery_init(struct platform_device *pdev, struct sec_battery_info *battery)
{
	adc_init_type(pdev, battery);
}

void board_fuelgauge_init(struct sec_fuelgauge_info *fuelgauge)
{
	pr_info("%s\n", __func__);
}

/* FG Max 17048 is not up yet, hence we set fixed SOC of 60%
 * This code would be removed once the Fuel Gauge is also
 * integrated
 */
#if 0
#if defined(CONFIG_FUELGAUGE_MAX77823)
void board_fuelgauge_init(struct max77823_fuelgauge_data *fuelgauge)
{
	sec_fuelgauge = 0;
	if (!fuelgauge->pdata->battery_data) {
		pr_info("%s : assign battery data\n", __func__);
			fuelgauge->pdata->battery_data = (void *)samsung_battery_data;
	}
}
#else
void board_fuelgauge_init(struct sec_fuelgauge_info *fuelgauge)
{
	sec_fuelgauge = fuelgauge;

	if (!fuelgauge->pdata->battery_data) {
		pr_info("%s : assign battery data\n", __func__);
			fuelgauge->pdata->battery_data = (void *)samsung_battery_data;
	}

	fuelgauge->pdata->capacity_max = CAPACITY_MAX;
	fuelgauge->pdata->capacity_max_margin = CAPACITY_MAX_MARGIN;
	fuelgauge->pdata->capacity_min = CAPACITY_MIN;

#if defined(CONFIG_FUELGAUGE_MAX17048)
	pr_info("%s: RCOMP0: 0x%x, RCOMP_charging: 0x%x, "
		"temp_cohot: %d, temp_cocold: %d, "
		"is_using_model_data: %d, type_str: %s, "
		"capacity_max: %d, capacity_max_margin: %d, "
		"capacity_min: %d, \n", __func__ ,
		get_battery_data(fuelgauge).RCOMP0,
		get_battery_data(fuelgauge).RCOMP_charging,
		get_battery_data(fuelgauge).temp_cohot,
		get_battery_data(fuelgauge).temp_cocold,
		get_battery_data(fuelgauge).is_using_model_data,
		get_battery_data(fuelgauge).type_str,
		fuelgauge->pdata->capacity_max,
		fuelgauge->pdata->capacity_max_margin,
		fuelgauge->pdata->capacity_min
		);
#endif
}
#endif
#endif

void cable_initial_check(struct sec_battery_info *battery)
{
	pr_info("%s\n", __func__);
}
