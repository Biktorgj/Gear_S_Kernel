#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/export.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <mach/irqs.h>
#include <linux/dc_motor.h>

#define ERROR_REGULATOR_NAME "error"
#define REGULATOR_NAME "mot_2.7v"
#define REGULATOR_NAME2 "mot_2.7v_2"

static int prev_val;

void dc_motor_power(bool on)
{
	struct regulator *regulator, *regulator2;
	static bool motor_enabled;

	if (motor_enabled == on)
		return ;

	if(!strcmp(ERROR_REGULATOR_NAME , REGULATOR_NAME)){
		pr_info("%s : regulator_name error\n", __func__);
		return;
	}
	/* if the nforce is zero, do not enable the vdd */
	if (on && !prev_val)
		return ;

	regulator = regulator_get(NULL, REGULATOR_NAME);
	regulator2 = regulator_get(NULL, REGULATOR_NAME2);
	motor_enabled = on;

#if defined(DC_MOTOR_LOG)
	pr_info("[VIB] %s\n", on ? "ON" : "OFF");
#endif

	if (on){
		regulator_enable(regulator);
		regulator_enable(regulator2);
	}
	else {
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		else
			regulator_force_disable(regulator);

		if (regulator_is_enabled(regulator2))
			regulator_disable(regulator2);
		else
			regulator_force_disable(regulator2);
		prev_val = 0;
	}
	regulator_put(regulator);
	regulator_put(regulator2);
}

void dc_motor_voltage(int val)
{
	struct regulator *regulator, *regulator2;

	if(!strcmp(ERROR_REGULATOR_NAME , REGULATOR_NAME)){
		pr_info("%s : regulator_name error\n", __func__);
		return;
	}

	if (!val) {
		dc_motor_power(val);
		return ;
	} else if (DC_MOTOR_VOL_MAX < val) {
		val = DC_MOTOR_VOL_MAX;
	} else if (DC_MOTOR_VOL_MIN > val)
		val = DC_MOTOR_VOL_MIN;

	if (prev_val == val)
		return ;
	else
		prev_val = val;

	regulator = regulator_get(NULL, REGULATOR_NAME);
	regulator2 = regulator_get(NULL, REGULATOR_NAME2);


	if (!IS_ERR(regulator)) {
	} else {
		pr_info("%s, fail!!!\n", __func__);
		regulator_put(regulator);
		return ;
	}

	if (!IS_ERR(regulator2)) {
	} else {
		pr_info("%s, fail!!!\n", __func__);
		regulator_put(regulator2);
		return ;
	}


#if defined(DC_MOTOR_LOG)
	pr_info("[VIB] %s : %d\n", __func__, val);
#endif

	val *= 100000;

	if (val) {
		if (regulator_set_voltage(regulator, val, val))
			pr_err("[VIB] failed to set voltage: %duV\n", val);
		if (regulator_set_voltage(regulator2, val, val))
			pr_err("[VIB] failed to set voltage: %duV\n", val);
	}
	regulator_put(regulator);
	regulator_put(regulator2);
	dc_motor_power(!!val);
}

static struct dc_motor_platform_data dc_motor_pdata = {
	.power = dc_motor_power,
	.set_voltage = dc_motor_voltage,
	.max_timeout = 10000,
};

static struct platform_device dc_motor_device = {
	.name = "dc_motor",
	.id = -1,
	.dev = {
		.platform_data = &dc_motor_pdata,
	},
};

void __init msm8226_vibrator_init(void)
{
	pr_info("[VIB] %s", __func__);

	platform_device_register(&dc_motor_device);
}

