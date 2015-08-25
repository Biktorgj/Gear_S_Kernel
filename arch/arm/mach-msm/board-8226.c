/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/msm_tsens.h>
#include <linux/export.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif
#ifdef CONFIG_ANDROID_PERSISTENT_RAM
#include <linux/persistent_ram.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/msm_smem.h>
#include <linux/msm_thermal.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"
#include "spm.h"
#include "pm.h"
#include "modem_notifier.h"
#include <mach/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

extern int system_rev;

#ifdef CONFIG_SENSORS_SSP

#ifdef CONFIG_MACH_B3
#define GPIO_MCU_VDD_EN			74
#define GPIO_MCU_RST_REV03		16
#define GPIO_MCU_RST			28
#define GPIO_HAPTIC_I2C_SDA		6
#define GPIO_HAPTIC_I2C_SCL		7
#define GPIO_HAPTIC_EN			35
#define HAPTIC_I2C_ID				15
#else
#define GPIO_MCU_VDD_EN_REV00		62
#define GPIO_MCU_VDD_EN			74
#endif

#ifdef CONFIG_MACH_B3
static int __init sensor_hub_init(void)
{
	struct regulator *vdd_acc, *vdd_led, *vdd_hrm, *vdd_hub;
	int ret;

	pr_info("[SSP]%s, system_rev=%d\n", __func__, system_rev);

	vdd_acc = regulator_get(NULL, "8226_l28");
	if (IS_ERR(vdd_acc)) {
		pr_err("[SSP] %s, could not get 8226_l28(acc vdd), %ld\n",
			__func__, PTR_ERR(vdd_acc));
	} else {
		ret = regulator_set_voltage(vdd_acc, 2850000, 2850000);
		if (ret)
			pr_err("[SSP] %s, fail to set voltage for 8226_l28(acc vdd)\n", __func__);

		ret = regulator_enable(vdd_acc);
		if (ret)
			pr_err("[SSP] %s, can't turn on for 8226_l28(acc vdd)\n", __func__);
	}

	if (system_rev == 8) {
		vdd_led = regulator_get(NULL, "8226_l16");
		if (IS_ERR(vdd_led)) {
			pr_err("[SSP] %s, could not get 8226_l16(led vdd), %ld\n",
				__func__, PTR_ERR(vdd_led));
		} else {
			ret = regulator_set_voltage(vdd_led, 3300000, 3300000);
			if (ret)
				pr_err("[SSP] %s, fail to set voltage for 8226_l16(led vdd)\n",
					__func__);

			ret = regulator_enable(vdd_led);
			if (ret)
				pr_err("[SSP] %s, can't turn on for 8226_l16(led vdd)\n",
					__func__);
		}
	}

	vdd_hrm = regulator_get(NULL, "8226_l27");
	if (IS_ERR(vdd_hrm)) {
		pr_err("[SSP] %s, could not get 8226_l27(hrm vdd), %ld\n",
			__func__, PTR_ERR(vdd_hrm));
	} else {
		ret = regulator_enable(vdd_hrm);
		if (ret)
			pr_err("[SSP] %s, can't turn on for 8226_l27(hrm vdd)\n", __func__);
	}
	msleep(200);

	/* MCU RESET */
	if (system_rev > 7) {
		ret = gpio_tlmm_config(GPIO_CFG(GPIO_MCU_RST,
				0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		if (ret)
			pr_err("[SSP] %s, could not get gpio(mcu rst)\n",
				__func__);
		gpio_set_value(GPIO_MCU_RST, 1);
	} else {
		ret = gpio_tlmm_config(GPIO_CFG(GPIO_MCU_RST_REV03,
				0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		if (ret)
			pr_err("[SSP] %s, could not get gpio(mcu rst)\n",
				__func__);
		gpio_set_value(GPIO_MCU_RST_REV03, 1);
	}

	/* MCU power enable */
	if (system_rev > 8) {
		vdd_hub = regulator_get(NULL, "8226_lvs1");
		if (IS_ERR(vdd_hub)) {
			pr_err("[SSP] %s, could not get 8226_lvs1(mcu vdd), %ld\n",
				__func__, PTR_ERR(vdd_hub));
		} else {
			ret = regulator_enable(vdd_hub);
			if (ret)
				pr_err("[SSP] %s, can't turn on for 8226_lvs1(mcu vdd)\n",
					__func__);
		}
	} else {
		ret = gpio_tlmm_config(GPIO_CFG(GPIO_MCU_VDD_EN, 0,
				GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		if (ret)
			pr_err("[SSP] %s, could not get gpio(mcu vdd)\n",
				__func__);
		gpio_set_value(GPIO_MCU_VDD_EN, 1);
	}

	return 0;
}
#endif
#endif


#ifdef CONFIG_DRV2604L
#ifndef CONFIG_USING_HW_I2C_DRV2604L
static struct i2c_gpio_platform_data gpio_i2c_haptic = {
        .scl_pin = GPIO_HAPTIC_I2C_SCL,
        .sda_pin = GPIO_HAPTIC_I2C_SDA,
};

struct platform_device device_i2c_haptic = {
        .name = "i2c-gpio",
        .id = HAPTIC_I2C_ID,
        .dev.platform_data = &gpio_i2c_haptic,
};

static struct i2c_board_info i2c_devs_haptic[] __initdata = {
        {
                I2C_BOARD_INFO("DRV2604L", 0x5A),
        },
};
#endif
#endif


/* MSM8x26 internal GPS */
static struct platform_device msm8226_gps = {
	.name	= "msm8226_gps",
	.id	= -1,
};

static struct memtype_reserve msm8226_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

#ifdef CONFIG_ANDROID_PERSISTENT_RAM
/* CONFIG_SEC_DEBUG reserving memory for persistent RAM*/
#define RAMCONSOLE_PHYS_ADDR 0x1FB00000
static struct persistent_ram_descriptor per_ram_descs[] __initdata = {
       {
               .name = "ram_console",
               .size = SZ_1M,
       }
};

static struct persistent_ram per_ram __initdata = {
       .descs = per_ram_descs,
       .num_descs = ARRAY_SIZE(per_ram_descs),
       .start = RAMCONSOLE_PHYS_ADDR,
       .size = SZ_1M
};
#endif
static int msm8226_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct of_dev_auxdata msm8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, "msm_hsic_host", NULL),

	{}
};

static struct reserve_info msm8226_reserve_info __initdata = {
	.memtype_reserve_table = msm8226_reserve_table,
	.paddr_to_memtype = msm8226_paddr_to_memtype,
};

static void __init msm8226_early_memory(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8226_reserve_table);
}

static void __init msm8226_reserve(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8226_reserve_table);
	msm_reserve();
#ifdef CONFIG_ANDROID_PERSISTENT_RAM
	persistent_ram_early_init(&per_ram);
#endif
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm8226_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_regulator_smd_driver_init();
	qpnp_regulator_init();
	if (of_board_is_rumi())
		msm_clock_init(&msm8226_rumi_clock_init_data);
	else
		msm_clock_init(&msm8226_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
}
struct class *sec_class;
EXPORT_SYMBOL(sec_class);

static void samsung_sys_class_init(void)
{
	pr_info("samsung sys class init.\n");

	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		return;
	}

	pr_info("samsung sys class end.\n");
};

#if defined(CONFIG_BATTERY_SAMSUNG_P1X)
extern void __init samsung_init_battery(void);
#endif

#if defined(CONFIG_DC_MOTOR)
extern void msm8226_vibrator_init(void);
#endif

void __init msm8226_init(void)
{
	struct of_dev_auxdata *adata = msm8226_auxdata_lookup;

#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8226_init_gpiomux();
	board_dt_populate(adata);
	samsung_sys_class_init();
	msm8226_add_drivers();
#if defined(CONFIG_BATTERY_SAMSUNG_P1X)
	samsung_init_battery();
#endif
#if defined(CONFIG_DC_MOTOR)
	msm8226_vibrator_init();
#endif

#ifdef CONFIG_DRV2604L
#ifndef CONFIG_USING_HW_I2C_DRV2604L
i2c_register_board_info(HAPTIC_I2C_ID, i2c_devs_haptic,
                                ARRAY_SIZE(i2c_devs_haptic));
#endif
#endif

#ifdef CONFIG_SENSORS_SSP
#ifdef CONFIG_MACH_B3
	sensor_hub_init();
#endif
#endif

	/* CP GPS */
	platform_device_register(&msm8226_gps);
#ifdef CONFIG_DRV2604L
#ifndef CONFIG_USING_HW_I2C_DRV2604L
	platform_device_register(&device_i2c_haptic);
#endif
#endif
}

static const char *msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	"qcom,msm8926",
	"qcom,apq8026",
	NULL
};

DT_MACHINE_START(MSM8226_DT, "Qualcomm MSM 8226 (Flattened Device Tree)")
	.map_io = msm_map_msm8226_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8226_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8226_dt_match,
	.reserve = msm8226_reserve,
	.init_very_early = msm8226_early_memory,
	.restart = msm_restart,
	.smp = &arm_smp_ops,
MACHINE_END
