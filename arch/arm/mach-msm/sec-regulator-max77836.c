/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/regulator/max77836-regulator.h>


#if defined(CONFIG_MACH_B3)

static struct regulator_consumer_supply ldo1_consumer =
	REGULATOR_SUPPLY("mot_2.7v", NULL);
static struct regulator_consumer_supply ldo2_consumer =
	REGULATOR_SUPPLY("mot_2.7v_2", NULL);

static struct regulator_init_data max77836_ldo1_data = {
	.constraints	= {
		.name		= "max77836_ldo_1",
		.min_uV		= 1500000,
		.max_uV		= 2700000,
		.apply_uV	= 1,
		.always_on	= 0,
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo1_consumer,
};

static struct regulator_init_data max77836_ldo2_data = {
	.constraints	= {
		.name		= "max77836_ldo_2",
		.min_uV		= 1500000,
		.max_uV		= 2700000,
		.apply_uV	= 1,
		.boot_on	= 0,
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo2_consumer,
};

#elif defined(CONFIG_MACH_RINATO_3G_OPEN)

static struct regulator_consumer_supply ldo1_consumer =
	REGULATOR_SUPPLY("vdd_mot_2.7v", NULL);

static struct regulator_consumer_supply ldo2_consumer =
	REGULATOR_SUPPLY("vsensor_2.85v", NULL);

static struct regulator_init_data max77836_ldo1_data = {
	.constraints	= {
		.name		= "vdd_mot_2.7v range",
		.min_uV		= 1500000,
		.max_uV		= 2700000,
		.apply_uV	= 1,
		.always_on	= 0,
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo1_consumer,
};

static struct regulator_init_data max77836_ldo2_data = {
	.constraints	= {
		.name		= "vsensor_2.85v range",
		.min_uV		= 2850000,
		.max_uV		= 2850000,
		.apply_uV	= 1,
#ifdef CONFIG_MACH_RINATO_3G_OPEN
		.boot_on		= 1,
#endif
		.state_mem	= {
			.enabled	= 0,
		},
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo2_consumer,
};

#endif

struct max77836_reg_platform_data max77836_reglator_pdata[] = {
	{
		.reg_id = MAX77836_LDO1,
		.init_data = &max77836_ldo1_data,
		.overvoltage_clamp_enable = 0,
		.auto_low_power_mode_enable = 0,
		.compensation_ldo = 0,
		.active_discharge_enable = 0,
		.soft_start_slew_rate = 0,
		.enable_mode = REG_NORMAL_MODE,

	},
	{
		.reg_id = MAX77836_LDO2,
		.init_data = &max77836_ldo2_data,
		.overvoltage_clamp_enable = 0,
		.auto_low_power_mode_enable = 0,
		.compensation_ldo = 0,
		.active_discharge_enable = 0,
		.soft_start_slew_rate = 0,
		.enable_mode = REG_NORMAL_MODE,

	},
};
