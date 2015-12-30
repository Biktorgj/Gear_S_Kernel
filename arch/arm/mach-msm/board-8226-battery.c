/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
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
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/irqs.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>
#include <mach/msm_bus_board.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c.h>
#include <linux/qpnp/pin.h>
#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)
#include <linux/qpnp/qpnp-adc.h>
#include <linux/sec_thermistor.h>
#endif

#if defined(CONFIG_BATTERY_SAMSUNG_P1X)
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/battery/sec_charging_common.h>
#define SEC_BATTERY_PMIC_NAME ""

#define GPIO_FUEL_SCL		11
#define GPIO_FUEL_SDA		10
#define GPIO_FUEL_nALRT		69

#define GPIO_FUELGAUGE_I2C_SDA		10
#define GPIO_FUELGAUGE_I2C_SCL		11
#define GPIO_FUEL_INT			69

#ifndef CONFIG_OF
#define MSM_FUELGAUGE_I2C_BUS_ID	19
#endif

static unsigned int sec_bat_recovery_mode;

/* Charging current table for 315mAh battery */
static sec_charging_current_t charging_current_table[] = {
	{150,	150,	60,	30*60},
	{0,	0,	0,	0},
	{150,	150,	60,	30*60},
	{150,	150,	60,	30*60},
	{175,	175,	60,	30*60},
	{150,	150,	60,	30*60},
	{150,	150,	60,	30*60},
	{150,	150,	60,	30*60},
	{150,	150,	60,	30*60},
	{0,	0,	0,	0},
	{150,	150,	60,	30*60},
	{150,	150,	60,	30*60},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
};

static bool sec_bat_adc_none_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_none_exit(void) {return true; }
static int sec_bat_adc_none_read(unsigned int channel) {return 0; }

static bool sec_bat_adc_ap_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ap_exit(void) {return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
#if defined(CONFIG_SENSORS_QPNP_ADC_VOLTAGE)
	int rc = -1, data =  -1;
	int results;

	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_TEMP:
		rc = sec_therm_get_adc(SEC_THERM_BATTERY, &results);

		if (rc) {
			pr_err("%s: Unable to read batt temperature rc=%d\n",
				__func__, rc);
			return 0;
		}
		data = results;
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		rc = sec_therm_get_adc(SEC_THERM_AP, &results);

		if (rc) {
			pr_err("%s: Unable to read ap temperature rc=%d\n",
				__func__, rc);
			return 0;
		}
		data = results;
		break;
	case SEC_BAT_ADC_CHANNEL_BAT_CHECK:
		break;
	default:
		break;
	}

	return data;
#else
	return 33000;
#endif
}

int sec_bat_vol_adc(struct sec_battery_info *battery)
{
#if 0
	int rc = -1, data =  -1;
	int results;

	rc = sec_therm_get_adc(SEC_THERM_VOL, &results);
	if (rc) {
		printk("%s: Unable to read batt voltage rc=%d\n",
			__func__, rc);
		return 2;
	}

	//battery->voltage_adc = results;
	data = results;

	return data;
#else
	int temp, value_vol_adc;

	temp = 4120;
	battery->voltage_adc = temp;

	value_vol_adc = battery->voltage_adc;

	return value_vol_adc;
#endif
}

static bool sec_bat_adc_ic_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ic_exit(void) {return true; }
static int sec_bat_adc_ic_read(unsigned int channel) {return 0; }

static bool sec_bat_gpio_init(void)
{
	return true;
}

static struct i2c_gpio_platform_data gpio_i2c_data_fgchg = {
	.sda_pin = GPIO_FUELGAUGE_I2C_SDA,
	.scl_pin = GPIO_FUELGAUGE_I2C_SCL,
};

static bool sec_fg_gpio_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_FUEL_INT,  0, GPIO_CFG_INPUT,
				GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(gpio_i2c_data_fgchg.scl_pin, 0,
				GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(gpio_i2c_data_fgchg.sda_pin,  0,
				GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_set_value(gpio_i2c_data_fgchg.scl_pin, 1);
	gpio_set_value(gpio_i2c_data_fgchg.sda_pin, 1);

	return true;
}

static bool sec_chg_gpio_init(void)
{
	return true;
}

#ifdef CONFIG_SAMSUNG_LPM_MODE
extern int boot_mode_lpm;
static bool sec_bat_is_lpm(void) { return (bool)boot_mode_lpm; }
#else /* LPM Mode not enabled */
static bool sec_bat_is_lpm(void) { return false; }
#endif

int extended_cable_type;
extern int current_cable_type;

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;

	psy_do_property("sec-charger", get,
		POWER_SUPPLY_PROP_ONLINE, value);
	if (value.intval != current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_ONLINE, value);
	}
}

static bool sec_bat_check_jig_status(void)
{
	return (current_cable_type == POWER_SUPPLY_TYPE_UARTOFF);
}
static bool sec_bat_switch_to_check(void) {return true; }
static bool sec_bat_switch_to_normal(void) {return true; }

static int sec_bat_check_cable_callback(void)
{
	return current_cable_type;
}

static int sec_bat_get_cable_from_extended_cable_type(
	int input_extended_cable_type)
{
	int cable_main, cable_sub, cable_power;
	int cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
	union power_supply_propval value;
	int charge_current_max = 0, charge_current = 0;

	cable_main = GET_MAIN_CABLE_TYPE(input_extended_cable_type);
	if (cable_main != POWER_SUPPLY_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_MAIN_MASK) |
			(cable_main << ONLINE_TYPE_MAIN_SHIFT);
	cable_sub = GET_SUB_CABLE_TYPE(input_extended_cable_type);
	if (cable_sub != ONLINE_SUB_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_SUB_MASK) |
			(cable_sub << ONLINE_TYPE_SUB_SHIFT);
	cable_power = GET_POWER_CABLE_TYPE(input_extended_cable_type);
	if (cable_power != ONLINE_POWER_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_PWR_MASK) |
			(cable_power << ONLINE_TYPE_PWR_SHIFT);

	switch (cable_main) {
	case POWER_SUPPLY_TYPE_CARDOCK:
		switch (cable_power) {
		case ONLINE_POWER_TYPE_BATTERY:
			cable_type = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case ONLINE_POWER_TYPE_TA:
			switch (cable_sub) {
			case ONLINE_SUB_TYPE_MHL:
				cable_type = POWER_SUPPLY_TYPE_USB;
				break;
			case ONLINE_SUB_TYPE_AUDIO:
			case ONLINE_SUB_TYPE_DESK:
			case ONLINE_SUB_TYPE_SMART_NOTG:
			case ONLINE_SUB_TYPE_KBD:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				break;
			case ONLINE_SUB_TYPE_SMART_OTG:
				cable_type = POWER_SUPPLY_TYPE_CARDOCK;
				break;
			}
			break;
		case ONLINE_POWER_TYPE_USB:
			cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		default:
			cable_type = current_cable_type;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_MISC:
		switch (cable_sub) {
		case ONLINE_SUB_TYPE_MHL:
			switch (cable_power) {
			case ONLINE_POWER_TYPE_BATTERY:
				cable_type = POWER_SUPPLY_TYPE_BATTERY;
				break;
			case ONLINE_POWER_TYPE_MHL_500:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 400;
				charge_current = 400;
				break;
			case ONLINE_POWER_TYPE_MHL_900:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 700;
				charge_current = 700;
				break;
			case ONLINE_POWER_TYPE_MHL_1500:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 1300;
				charge_current = 1300;
				break;
			case ONLINE_POWER_TYPE_USB:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 300;
				charge_current = 300;
				break;
			default:
				cable_type = cable_main;
			}
			break;
		default:
			cable_type = cable_main;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_OTG:
		cable_type = current_cable_type;
		break;
	default:
		cable_type = cable_main;
		break;
	}

	if (charge_current_max == 0) {
		charge_current_max =
			charging_current_table[cable_type].input_current_limit;
		charge_current =
			charging_current_table[cable_type].
			fast_charging_current;
	}
	value.intval = charge_current_max;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_MAX, value);
	value.intval = charge_current;
	psy_do_property(sec_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_CURRENT_AVG, value);
	return cable_type;
}

static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
	current_cable_type = cable_type;

	switch (cable_type) {
	case POWER_SUPPLY_TYPE_USB:
		pr_info("%s set vbus applied\n",
			__func__);
		break;

	case POWER_SUPPLY_TYPE_BATTERY:
		pr_info("%s set vbus cut\n",
			__func__);
		break;
	case POWER_SUPPLY_TYPE_MAINS:
		break;
	default:
		pr_err("%s cable type (%d)\n",
			__func__, cable_type);
		return false;
	}
	return true;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void)
{
	struct power_supply *psy;
	union power_supply_propval value;

	psy = get_power_supply_by_name(("sec-charger"));
	if (!psy) {
		pr_err("%s: Fail to get psy (%s)\n",
				__func__, "sec-charger");
		value.intval = 1;
	} else {
		if (sec_battery_pdata.bat_irq_gpio > 0) {
			value.intval = !gpio_get_value(sec_battery_pdata.bat_irq_gpio);
			if (value.intval == 0) {
				pr_info("%s:  Battery status(%d)\n",
					__func__, value.intval);
				return value.intval;
			}
		} else {
			int ret;
			ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &(value));
			if (ret < 0) {
				pr_err("%s: Fail to sec-charger get_property (%d=>%d)\n",
						__func__, POWER_SUPPLY_PROP_PRESENT, ret);
				value.intval = 1;
			}
		}
	}
	return value.intval;
}
static bool sec_bat_check_result_callback(void) {return true; }

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static bool sec_bat_ovp_uvlo_result_callback(int health) {return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) {return true; }
static bool sec_fg_fuelalert_process(bool is_fuel_alerted) {return true; }

/*
 * Battery Temperature Table, modified in-line with 8974
 * since the Battery Temp is read using the QPNP ADC. These
 * readings would need to be modified for 300mAh battery
 * that is supported in the device
 */
static sec_bat_adc_table_data_t temp_table[] = {
	{26213, 900},
	{26421, 850},
	{26686, 800},
	{26963, 750},
	{27307, 700},
	{27710, 650},
	{28159, 600},
	{28695, 550},
	{29343, 500},
	{30053, 450},
	{30878, 400},
	{31779, 350},
	{32790, 300},
	{33745, 250},
	{34834, 200},
	{35892, 150},
	{36965, 100},
	{38041,  50},
	{38258,  40},
	{38462,  30},
	{38633,  20},
	{38808,  10},
	{38999,   0},
	{39201, -10},
	{39377, -20},
	{39569, -30},
	{39756, -40},
	{39914, -50},
	{40658, -100},
	{41296, -150},
	{41804, -200},
	{42220, -250},
	{42527, -300},
};

/* ADC region should be exclusive */
static sec_bat_adc_region_t cable_adc_value_table[] = {
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
};

static int polling_time_table[] = {
	10,	/* BASIC */
	30,	/* CHARGING */
	30,	/* DISCHARGING */
	30,	/* NOT_CHARGING */
	60 * 60,	/* SLEEP */
};

/* For Max17048 */
static struct battery_data_t msm8x26_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 0x4A,
		.RCOMP_charging = 0x4A,
		.temp_cohot = -1325,
		.temp_cocold = -5825,
#if 1
		.is_using_model_data = true,	/* using 19 bits */
#else
		.is_using_model_data = false,	/* using 18 bits */
#endif
		.type_str = "SDI",
	}
};

#ifndef CONFIG_OF
/* temp, same with board-8974-sec.c */
#define IF_PMIC_IRQ_BASE	353 /* temp val */
#endif

sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,
	.cable_switch_check = sec_bat_switch_to_check,
	.cable_switch_normal = sec_bat_switch_to_normal,
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
		},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 6,
	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_AP,	/* FULL_CHECK */
	},

	/* Battery */
	.vendor = "SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)msm8x26_battery_data,
	.bat_gpio_ta_nconnected = 0,
	.bat_polarity_ta_nconnected = 0,
	.bat_irq = 0,
	.bat_irq_attr = 0,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_CHGINT,
	.cable_source_type =
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL,

	.event_check = true,
	.event_waiting_time = 180,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_NONE,
	.check_count = 0,
	/* Battery check by ADC */
	.check_adc_max = 1440,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = temp_table,
	.temp_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,

#if defined(CONFIG_MACH_B3_ATT) || defined(CONFIG_MACH_B3_TMO) || defined(CONFIG_MACH_B3_VMC)
	.temp_high_threshold_event = 540,
	.temp_high_recovery_event = 490,
	.temp_low_threshold_event = 0,
	.temp_low_recovery_event = 50,

	.temp_high_threshold_normal = 540,
	.temp_high_recovery_normal = 490,
	.temp_low_threshold_normal = 0,
	.temp_low_recovery_normal = 50,

	.temp_high_threshold_lpm = 540,
	.temp_high_recovery_lpm = 490,
	.temp_low_threshold_lpm = 0,
	.temp_low_recovery_lpm = 50,
#elif defined(CONFIG_MACH_B3_CDMA_SPR) || defined(CONFIG_MACH_B3_CDMA_VZW) || defined(CONFIG_MACH_B3_CDMA_USC)
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 460,
	.temp_low_threshold_event = -40,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 460,
	.temp_low_threshold_normal = -40,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 540,
	.temp_high_recovery_lpm = 490,
	.temp_low_threshold_lpm = 0,
	.temp_low_recovery_lpm = 50,
#else /* CONFIG_MACH_B3 */
	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 460,
	.temp_low_threshold_event = -40,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 600,
	.temp_high_recovery_normal = 460,
	.temp_low_threshold_normal = -40,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 600,
	.temp_high_recovery_lpm = 460,
	.temp_low_threshold_lpm = -40,
	.temp_low_recovery_lpm = 0,
#endif

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	.chg_gpio_full_check = 0,
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 97,
	.full_condition_vcell = 4300,

	.recharge_check_count = 2,
	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_condition_soc = 90,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = GPIO_FUEL_nALRT,
	.fg_irq_attr = IRQF_TRIGGER_RISING,
	.fuel_alert_soc = 2,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW |
		SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE,
		/*SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL,*/
	.capacity_max = 1000,
	.capacity_max_margin = 50,
	.capacity_min = 5,

	/* Charger */
	.charger_name = "sec-charger",
	.chg_gpio_en = 0,
	.chg_polarity_en = 0,
	.chg_gpio_status = 0,
	.chg_polarity_status = 0,
	.chg_irq = 0,
	.chg_irq_attr = 0,
	.chg_float_voltage = 4350,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

#ifndef CONFIG_OF
struct platform_device sec_device_fgchg = {
	.name = "i2c-gpio",
	.id = MSM_FUELGAUGE_I2C_BUS_ID,
	.dev.platform_data = &gpio_i2c_data_fgchg,
};


static struct i2c_board_info sec_brdinfo_fgchg[] __initdata = {
	{
		I2C_BOARD_INFO("sec-fuelgauge",
				SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};
#endif

static struct platform_device *samsung_battery_devices[] __initdata = {
#ifndef CONFIG_OF
	&sec_device_fgchg,
#endif
	&sec_device_battery,
};

static int __init sec_bat_current_boot_mode(char *mode)
{
	/*
	*	1 is recovery booting
	*	0 is normal booting
	*/

	if (strncmp(mode, "1", 1) == 0)
		sec_bat_recovery_mode = 1;
	else
		sec_bat_recovery_mode = 0;

	pr_info("%s : %s", __func__, sec_bat_recovery_mode == 1 ?
				"recovery" : "normal");

	return 1;
}
__setup("androidboot.boot_recovery=", sec_bat_current_boot_mode);

void __init samsung_init_battery(void)
{
	/* FUEL_SDA/SCL setting */
	platform_add_devices(
		samsung_battery_devices,
		ARRAY_SIZE(samsung_battery_devices));

#ifndef CONFIG_OF
	i2c_register_board_info(
		MSM_FUELGAUGE_I2C_BUS_ID,
		sec_brdinfo_fgchg,
		ARRAY_SIZE(sec_brdinfo_fgchg));
#endif
}

#endif
