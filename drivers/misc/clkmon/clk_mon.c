/*
 * driver/misc/clkmon/clk_mon.c
 *
 * Copyright (C) 2013 Samsung Electronics co. ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/clk_mon.h>
#if defined(CONFIG_ARCH_MSM)
#include "clk_mon_msm.h"
#endif
#include "clk_mon_ioctl.h"
/* 8x26 is based on regulator framework and not Domain based framework
 * hence, we need to get the status of the Parent regulators to check if
 * it is ON/OFF
*/
#include <linux/regulator/consumer.h>

#define SIZE_REG				0x4
/* Bit 31 of each clock register indicates whether the Clock is ON/OFF */
#define CLK_STATUS_BIT_POS			31
#define CHECK_BIT_SET(var, pos)		((var) & (1<<(pos)))
#define BIT_ZERO				0x0
#define BIT_ONE					0x1

/* 8x26 is based on regulator concept and hence we get the list
 * of regulators in the system. They are source of supply for
 * the consumers that are under them.
*/

struct power_domain_mask power_domain_masks[] = {
	{"8226_l1"}, {"8226_l2"}, {"8226_l3"}, {"8226_l4"},
	{"8226_l5"}, {"8226_l6"}, {"8226_l7"}, {"8226_l8"},
	{"8226_l9"}, {"8226_l10"}, {"8226_l12"}, {"8226_l14"},
	{"8226_l15"}, {"8226_l16"}, {"8226_l17"}, {"8226_l18"},
	{"8226_l19"}, {"8226_l20"}, {"8226_l21"}, {"8226_l22"},
	{"8226_l23"}, {"8226_l24"}, {"8226_l25"}, {"8226_l26"},
	{"8226_l27"}, {"8226_l28"}, {"8226_lvs1"}, {"8226_s1"},
	{"8226_s1_corner"}, {"8226_s1_corner_ao"}, {"8226_s1_floor_corner"},
	{"8226_l3_ao"}, {"8226_l3_so"}, {"8226_l8_ao"}, {"8226_s2"},
	{"8226_s3"}, {"8226_s4"}, {"8226_s5"}, {"apc_corner"},
	{"gdsc_jpeg"}, {"gdsc_mdss"}, {"gdsc_oxili_cx"}, {"gdsc_usb_hsic"},
	{"gdsc_venus"}, {"gdsc_vfe"},
	/* These are from the Maxim IC */
	{"vhrm_led_3.3v"}, {"vdd_mot_2.7v"}
};

/*** GCC_BLSP Clocks ***/
#define CLK_MON_GCC_BLSP1_QUP1_SPI_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0644)
#define CLK_MON_GCC_BLSP1_QUP1_I2C_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0648)
#define CLK_MON_GCC_BLSP1_UART1_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0684)
#define CLK_MON_GCC_BLSP1_QUP2_SPI_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x06C4)
#define CLK_MON_GCC_BLSP1_QUP2_I2C_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x06C8)
#define CLK_MON_GCC_BLSP1_UART2_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0704)
#define CLK_MON_GCC_BLSP1_UART2_SIM_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0708)
#define CLK_MON_GCC_BLSP1_QUP3_SPI_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0744)
#define CLK_MON_GCC_BLSP1_QUP3_I2C_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0748)
#define CLK_MON_GCC_BLSP1_UART3_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0784)
#define CLK_MON_GCC_BLSP1_UART3_SIM_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0788)
#define CLK_MON_GCC_BLSP1_QUP4_SPI_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x07C4)
#define CLK_MON_GCC_BLSP1_QUP4_I2C_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x07C8)
#define CLK_MON_GCC_BLSP1_UART4_APPS_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0804)
#define CLK_MON_GCC_BLSP1_UART4_SIM_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0808)
#define CLK_MON_BLSP1_QUP5_SPI_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0844)
#define CLK_MON_BLSP1_UART5_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0884)
#define CLK_MON_BLSP1_QUP5_I2C_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0848)
#define CLK_MON_BLSP1_QUP6_SPI_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x08C4)
#define CLK_MON_BLSP1_QUP6_I2C_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x08C8)
#define CLK_MON_BLSP1_UART6_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0904)
#define CLK_MON_GCC_GP1_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x1900)
#define CLK_MON_GCC_GP2_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x1940)
#define CLK_MON_GCC_GP3_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x1980)
#define CLK_MON_GCC_BLSP1_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x05C4)
#define CLK_MON_GCC_BAM_DMA_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0D44)
#define CLK_MON_BOOT_ROM_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0E04)
#define CLK_MON_GCC_SPMI_CNOC_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0FC8)
#define CLK_MON_CE1_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x1044)
#define CLK_MON_CE1_AXI_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x1048)
#define CLK_MON_CE1_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x104C)
#define CLK_MONLPASS_Q6_AXI_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x1140)
#define CLK_MON_GCC_MSS_CFG_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0280)
#define CLK_MON_GCC_MSS_Q6_BIMC_AXI_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0284)
#define CLK_MON_GCC_QDSS_DAP_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0304)
/*** GCC_MMSS Clocks ***/
#define CLK_MON_GCC_MMSS_NOC_CFG_AHB_CBCR	\
					(CLK_MON_CGATE_MMSS_BASE + 0x024C)
#define CLK_MON_GCC_MMSS_NOC_AT_CBCR	\
					(CLK_MON_CGATE_MMSS_BASE + 0x0250)

/*** Multi-Media Sub-System ***/
#define CLK_MON_MMSS_MMSS_SPDM_JPEG0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0204)
#define CLK_MON_MMSS_MMSS_SPDM_JPEG1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0208)
#define CLK_MON_MMSS_MMSS_SPDM_MDP_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x020C)
#define CLK_MON_MMSS_MMSS_SPDM_AXI_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0210)
#define CLK_MON_MMSS_MMSS_SPDM_VCODEC0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0214)
#define CLK_MON_MMSS_MMSS_SPDM_VFE0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0218)
#define CLK_MON_MMSS_MMSS_SPDM_VFE1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x021C)
#define CLK_MON_MMSS_MMSS_SPDM_JPEG2_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0224)
#define CLK_MON_MMSS_MMSS_SPDM_PCLK1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0228)
#define CLK_MON_MMSS_MMSS_SPDM_GFX3D_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x022C)
#define CLK_MON_MMSS_MMSS_SPDM_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0230)
#define CLK_MON_MMSS_MMSS_SPDM_PCLK0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0234)
#define CLK_MON_MMSS_MMSS_SPDM_OCMEMNOC_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0238)
#define CLK_MON_MMSS_MMSS_SPDM_CSI0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x023C)
#define CLK_MON_MMSS_MMSS_SPDM_RM_AXI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0304)
#define CLK_MON_MMSS_MMSS_SPDM_RM_OCMEMNOC_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x0308)
/*** Clocks from the Venus Sub-System ***/
#define CLK_MON_VENUS0_VCODEC0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x1028)
#define CLK_MON_VENUS0_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x1030)
#define CLK_MON_VENUS0_AXI_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x1034)
#define CLK_MON_VENUS0_OCMEMNOC_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x1038)
/*** Other clocks from the Multi-Media Sub-System ***/
#define CLK_MON_MDSS_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2308)
#define CLK_MON_MDSS_HDMI_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x230C)
#define CLK_MON_MDSS_AXI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2310)
#define CLK_MON_MMSS_MDSS_PCLK0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2314)
#define CLK_MON_MMSS_MDSS_PCLK1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2318)
#define CLK_MON_MMSS_MDSS_MDP_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x231C)
#define CLK_MON_MMSS_MDSS_MDP_LUT_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2320)
#define CLK_MON_MMSS_MDSS_EXTPCLK_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2324)
#define CLK_MON_MMSS_MDSS_VSYNC_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2328)
#define CLK_MON_MMSS_MDSS_EDPPIXEL_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x232C)
#define CLK_MON_MMSS_MDSS_EDPLINK_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2330)
#define CLK_MON_MMSS_MDSS_EDPAUX_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2334)
#define CLK_MON_MMSS_MDSS_HDMI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2338)
#define CLK_MON_MMSS_MDSS_BYTE0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x233C)
#define CLK_MON_MMSS_MDSS_BYTE1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2340)
#define CLK_MON_MMSS_MDSS_ESC0_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2344)
#define CLK_MON_MMSS_MDSS_ESC1_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x2348)
/*** Clocks from the Camera Module ***/
#define CLK_MON_MMSS_CAMSS_PHY0_CSI0PHYTIMER_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3024)
#define CLK_MON_MMSS_CAMSS_PHY1_CSI1PHYTIMER_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3054)
#define CLK_MON_MMSS_CAMSS_PHY2_CSI2PHYTIMER_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3084)
#define CLK_MON_MMSS_CAMSS_CSI0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x30B4)
#define CLK_MON_MMSS_CAMSS_CSI0_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x30BC)
#define CLK_MON_MMSS_CAMSS_CSI0PHY_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x30C4)
#define CLK_MON_MMSS_CAMSS_CSI0RDI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x30D4)
#define CLK_MON_MMSS_CAMSS_CSI0PIX_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x30E4)
#define CLK_MON_MMSS_CAMSS_CSI1_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3124)
#define CLK_MON_MMSS_CAMSS_CSI1_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3128)
#define CLK_MON_MMSS_CAMSS_CSI1PHY_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3134)
#define CLK_MON_MMSS_CAMSS_CSI1RDI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3144)
#define CLK_MON_MMSS_CAMSS_CSI1PIX_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3154)
#define CLK_MON_MMSS_CAMSS_CSI2_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3184)
#define CLK_MON_MMSS_CAMSS_CSI2_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3188)
#define CLK_MON_MMSS_CAMSS_CSI2PHY_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3194)
#define CLK_MON_MMSS_CAMSS_CSI2RDI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x31A4)
#define CLK_MON_MMSS_CAMSS_CSI2PIX_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x31B4)
#define CLK_MON_MMSS_CAMSS_CSI3_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x31E4)
#define CLK_MON_MMSS_CAMSS_CSI3_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x31E8)
#define CLK_MON_MMSS_CAMSS_CSI3PHY_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x31F4)
#define CLK_MON_MMSS_CAMSS_CSI3RDI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3204)
#define CLK_MON_MMSS_CAMSS_CSI3PIX_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3214)
#define CLK_MON_MMSS_CAMSS_ISPIF_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3224)
#define CLK_MON_MMSS_CAMSS_CCI_CCI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3344)
#define CLK_MON_MMSS_CAMSS_CCI_CCI_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3348)
#define CLK_MON_MMSS_CAMSS_MCLK0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3384)
#define CLK_MON_MMSS_CAMSS_MCLK1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x33B4)
#define CLK_MON_MMSS_CAMSS_MCLK2_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x33E4)
#define CLK_MON_MMSS_CAMSS_MCLK3_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3414)
#define CLK_MON_MMSS_CAMSS_GP0_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3444)
#define CLK_MON_MMSS_CAMSS_GP1_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3474)
#define CLK_MON_MMSS_CAMSS_TOP_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3484)
#define CLK_MON_MMSS_CAMSS_MICRO_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3494)
/*** Camera Module - JPEG CLocks ***/
#define CLK_MON_MMSS_CAMSS_JPEG_JPEG0_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x35A8)
#define CLK_MON_MMSS_CAMSS_JPEG_JPEG1_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x35AC)
#define CLK_MON_MMSS_CAMSS_JPEG_JPEG2_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x35B0)
#define CLK_MON_MMSS_CAMSS_JPEG_JPEG_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x35B4)
#define CLK_MON_MMSS_CAMSS_JPEG_JPEG_AXI_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x35B8)
#define CLK_MON_MMSS_CAMSS_JPEG_JPEG_OCMEMNOC_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x35BC)
/**** Camera Module - VFE CLocks ***/
#define CLK_MON_CAMSS_VFE_VFE0_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x36A8)
#define CLK_MON_MMSS_CAMSS_VFE_VFE1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x36AC)
#define CLK_MON_MMSS_CAMSS_VFE_CPP_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x36B0)
#define CLK_MON_MMSS_CAMSS_VFE_CPP_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x36B4)
#define CLK_MON_MMSS_CAMSS_VFE_VFE_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x36B8)
#define CLK_MON_MMSS_CAMSS_VFE_VFE_AXI_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x36BC)
#define CLK_MON_MMSS_CAMSS_VFE_VFE_OCMEMNOC_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x36C0)
#define CLK_MON_MMSS_CAMSS_CSI_VFE0_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3704)
#define CLK_MON_MMSS_CAMSS_CSI_VFE1_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x3714)
#define CLK_MON_MMSS_OXILI_GFX3D_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x4028)
#define CLK_MON_MMSS_OXILI_OCMEMGX_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x402C)
#define CLK_MON_MMSS_OXILICX_AXI_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x4038)
#define CLK_MON_MMSS_OXILI_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x403C)
#define CLK_MON_MMSS_OCMEMCX_OCMEMNOC_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x4058)
#define CLK_MON_MMSS_OCMEMCX_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x405C)
#define CLK_MON_MMSS_MMSS_MMSSNOC_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x5024)
#define CLK_MON_MMSS_MMSS_MMSSNOC_BTO_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x5028)
#define CLK_MON_MMSS_MMSS_MISC_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x502C)
#define CLK_MON_MMSS_MMSS_S0_AXI_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x5064)
#define CLK_MON_MMSS_MMSS_MMSSNOC_AXI_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x506C)
#define CLK_MON_MMSS_OCMEMNOC_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x50B4)
#define CLK_MON_MMSS_MMSS_SLEEPCLK_CBCR		\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x5100)
#define CLK_MON_MMSS_MMSS_CXO_CBCR	\
					(CLK_MON_CGATE_GCC_MSS_BASE + 0x5104)
/*** USB Clocks ***/
#define CLK_MON_GCC_USB_HSIC_AHB_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0408)
#define CLK_MON_GCC_USB_HSIC_SYSTEM_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x040C)
#define CLK_MON_GCC_USB_HSIC_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0410)
#define CLK_MON_GCC_USB_HSIC_IO_CAL_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0414)
#define CLK_MON_GCC_USB2B_PHY_SLEEP_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x04B4)
#define CLK_MON_GCC_OCMEM_NOC_CFG_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0248)
#define CLK_MON_PDM_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0CC4)
#define CLK_MON_PDM_XO4_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0CC8)
#define CLK_MON_PDM2_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0CCC)
#define CLK_MON_PRNG_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0D04)
#define CLK_MON_SDCC1_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x04C4)
#define CLK_MON_SDCC1_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x04C8)
#define CLK_MON_SDCC2_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0504)
#define CLK_MON_SDCC2_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0508)
#define CLK_MON_SDCC3_APPS_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0544)
#define CLK_MON_SDCC3_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0548)
#define CLK_MON_GCC_USB2A_PHY_SLEEP_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x04AC)
#define CLK_MON_GCC_USB_HS_AHB_CBCR		\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0488)
#define CLK_MON_GCC_USB_HS_SYSTEM_CBCR	\
					(CLK_MON_CGATE_GCC_BLSP_BASE + 0x0484)

static struct clk_gate_mask clk_gate_masks[] = {
	/* System Related Clocks, Start */
	{"gcc_b1_qup1_spi", CLK_MON_GCC_BLSP1_QUP1_SPI_APPS_CBCR},
	{"gcc_b1_qup2_i2c", CLK_MON_GCC_BLSP1_QUP1_I2C_APPS_CBCR},
	{"gcc_b1_uart1", CLK_MON_GCC_BLSP1_UART1_APPS_CBCR},
	{"gcc_b1_qup2_spi", CLK_MON_GCC_BLSP1_QUP2_SPI_APPS_CBCR},
	{"gcc_b1_qup2_i2c", CLK_MON_GCC_BLSP1_QUP2_I2C_APPS_CBCR},
	{"gcc_b1_uart2", CLK_MON_GCC_BLSP1_UART2_SIM_CBCR},
	{"gcc_b1_qup3_spi", CLK_MON_GCC_BLSP1_QUP3_SPI_APPS_CBCR},
	{"gcc_b1_qup3_i2c", CLK_MON_GCC_BLSP1_QUP3_I2C_APPS_CBCR},
	{"gcc_b1_uart3", CLK_MON_GCC_BLSP1_UART3_APPS_CBCR},
	{"gcc_b1_uart3_sim", CLK_MON_GCC_BLSP1_UART3_SIM_CBCR},
	{"gcc_b1_qup4_spi", CLK_MON_GCC_BLSP1_QUP4_SPI_APPS_CBCR},
	{"gcc_b1_qup4_i2c", CLK_MON_GCC_BLSP1_QUP4_I2C_APPS_CBCR},
	{"gcc_b1_uart4_apps", CLK_MON_GCC_BLSP1_UART4_APPS_CBCR},
	{"gcc_b1_uart4_sim", CLK_MON_GCC_BLSP1_UART4_SIM_CBCR},
	{"gcc_blsp1_qup5_spi", CLK_MON_BLSP1_QUP5_SPI_APPS_CBCR},
	{"gcc_blsp1_uart5", CLK_MON_BLSP1_UART5_APPS_CBCR},
	{"gcc_blsp1_qup5_i2c", CLK_MON_BLSP1_QUP5_I2C_APPS_CBCR},
	{"gcc_blsp1_qup6_i2c", CLK_MON_BLSP1_QUP6_I2C_APPS_CBCR},
	{"gcc_blsp1_qup6_spi", CLK_MON_BLSP1_QUP6_SPI_APPS_CBCR},
	{"gcc_blsp1_uart6", CLK_MON_BLSP1_UART6_APPS_CBCR},
	{"gcc_gp1", CLK_MON_GCC_GP1_CBCR},
	{"gcc_gp2", CLK_MON_GCC_GP2_CBCR},
	{"gcc_gp3", CLK_MON_GCC_GP3_CBCR},
	{"gcc_blsp1_ahb", CLK_MON_GCC_BLSP1_AHB_CBCR},
	{"gcc_bam_dma_ahb", CLK_MON_GCC_BAM_DMA_AHB_CBCR},
	{"gcc_boot_rom_ahb", CLK_MON_BOOT_ROM_AHB_CBCR},
	{"gcc_spmi_cnoc_ahb", CLK_MON_GCC_SPMI_CNOC_AHB_CBCR},
	{"gcc_ce1", CLK_MON_CE1_CBCR},
	{"gcc_spmi_cnoc_ahb", CLK_MON_CE1_AXI_CBCR},
	{"gcc_ce1_axi", CLK_MON_CE1_AHB_CBCR},
	{"gcc_lpass_q6_axi", CLK_MONLPASS_Q6_AXI_CBCR},
	{"gcc_mss_cfg_ahb", CLK_MON_GCC_MSS_CFG_AHB_CBCR},
	{"gcc_mss_q6_bimc_axi", CLK_MON_GCC_MSS_Q6_BIMC_AXI_CBCR},
	{"gcc_pdm_ahb", CLK_MON_PDM_AHB_CBCR},
	{"gcc_pdm_xo4", CLK_MON_PDM_XO4_CBCR},
	{"gcc_pdm2", CLK_MON_PDM2_CBCR},
	{"gcc_prng_ahb", CLK_MON_PRNG_AHB_CBCR},
	{"gcc_sdcc1_apps", CLK_MON_SDCC1_APPS_CBCR},
	{"gcc_sdcc1_ahb", CLK_MON_SDCC1_AHB_CBCR},
	{"gcc_sdcc2_apps", CLK_MON_SDCC2_APPS_CBCR},
	{"gcc_sdcc2_ahb", CLK_MON_SDCC2_AHB_CBCR},
	{"gcc_sdcc3_apps", CLK_MON_SDCC3_APPS_CBCR},
	{"gcc_sdcc3_ahb", CLK_MON_SDCC3_AHB_CBCR},
	{"gcc_usb_hsic_ahb", CLK_MON_GCC_USB_HSIC_AHB_CBCR},
	{"gcc_usb_hsic_system", CLK_MON_GCC_USB_HSIC_SYSTEM_CBCR},
	{"gcc_usb_hsic", CLK_MON_GCC_USB_HSIC_CBCR},
	{"gcc_usb_hsic_io_cal", CLK_MON_GCC_USB_HSIC_IO_CAL_CBCR},
	{"gcc_usb_hs_system", CLK_MON_GCC_USB_HS_SYSTEM_CBCR},
	{"gcc_usb_hs_ahb", CLK_MON_GCC_USB_HS_AHB_CBCR},
	{"gcc_usb2a_phy_sleep", CLK_MON_GCC_USB2A_PHY_SLEEP_CBCR},
	{"gcc_ocmem_noc_cfg_ahb", CLK_MON_GCC_OCMEM_NOC_CFG_AHB_CBCR},
	/* System Related Clocks, End */
	/* Camera Sub-System Clocks */
	{"camss_cci_cci_ahb", CLK_MON_MMSS_CAMSS_CCI_CCI_AHB_CBCR},
	{"camss_cci_cci", CLK_MON_MMSS_CAMSS_CCI_CCI_CBCR},
	{"camss_csi0_ahb", CLK_MON_MMSS_CAMSS_CSI0_AHB_CBCR},
	{"camss_csi0", CLK_MON_MMSS_CAMSS_CSI0_CBCR},
	{"camss_csi0phy", CLK_MON_MMSS_CAMSS_CSI0PHY_CBCR},
	{"camss_csi0pix", CLK_MON_MMSS_CAMSS_CSI0PIX_CBCR},
	{"camss_csi0rdi", CLK_MON_MMSS_CAMSS_CSI0RDI_CBCR},
	{"camss_csi1_ahb", CLK_MON_MMSS_CAMSS_CSI1_AHB_CBCR},
	{"camss_csi1", CLK_MON_MMSS_CAMSS_CSI1_CBCR},
	{"camss_csi1phy", CLK_MON_MMSS_CAMSS_CSI1PHY_CBCR},
	{"camss_csi1pix", CLK_MON_MMSS_CAMSS_CSI1PIX_CBCR},
	{"camss_csi1rdi", CLK_MON_MMSS_CAMSS_CSI1RDI_CBCR},
	{"camss_csi_vfe0", CLK_MON_MMSS_CAMSS_CSI_VFE0_CBCR},
	{"camss_gp0", CLK_MON_MMSS_CAMSS_GP0_CBCR},
	{"camss_gp1", CLK_MON_MMSS_CAMSS_GP1_CBCR},
	{"camss_ispif_ahb", CLK_MON_MMSS_CAMSS_ISPIF_AHB_CBCR},
	{"camss_jpeg_jpeg0", CLK_MON_MMSS_CAMSS_JPEG_JPEG0_CBCR},
	{"camss_jpeg_jpeg_ahb", CLK_MON_MMSS_CAMSS_JPEG_JPEG_AHB_CBCR},
	{"camss_jpeg_jpeg_axi", CLK_MON_MMSS_CAMSS_JPEG_JPEG_AXI_CBCR},
	{"camss_mclk0", CLK_MON_MMSS_CAMSS_MCLK0_CBCR},
	{"camss_mclk1", CLK_MON_MMSS_CAMSS_MCLK1_CBCR},
	{"camss_micro_ahb", CLK_MON_MMSS_CAMSS_MICRO_AHB_CBCR},
	{"camss_phy0_csi0phy", CLK_MON_MMSS_CAMSS_PHY0_CSI0PHYTIMER_CBCR},
	{"camss_phy1_csi1phy", CLK_MON_MMSS_CAMSS_PHY1_CSI1PHYTIMER_CBCR},
	{"camss_top_ahb", CLK_MON_MMSS_CAMSS_TOP_AHB_CBCR},
	{"camss_vfe_cpp_ahb", CLK_MON_MMSS_CAMSS_VFE_CPP_AHB_CBCR},
	{"camss_vfe_cpp", CLK_MON_MMSS_CAMSS_VFE_CPP_CBCR},
	{"camss_vfe_vfe0", CLK_MON_CAMSS_VFE_VFE0_CBCR},
	{"camss_vfe_vfe_ahb", CLK_MON_MMSS_CAMSS_VFE_VFE_AHB_CBCR},
	{"camss_vfe_vfe_axi", CLK_MON_MMSS_CAMSS_VFE_VFE_AXI_CBCR},
	/* Camera Sub-System Clocks,End */
	/* MDSS Clocks, Start */
	{"mdss_ahb", CLK_MON_MDSS_AHB_CBCR},
	{"mdss_axi", CLK_MON_MDSS_AXI_CBCR},
	{"mdss_byte0", CLK_MON_MMSS_MDSS_BYTE0_CBCR},
	{"mdss_esc0", CLK_MON_MMSS_MDSS_ESC0_CBCR},
	{"mdss_mdp", CLK_MON_MMSS_MDSS_MDP_CBCR},
	{"mdss_mdp_lut", CLK_MON_MMSS_MDSS_MDP_LUT_CBCR},
	{"mdss_pclk0", CLK_MON_MMSS_MDSS_PCLK0_CBCR},
	{"mdss_vsync", CLK_MON_MMSS_MDSS_VSYNC_CBCR},
	{"mmss_misc_ahb", CLK_MON_MMSS_MMSS_MISC_AHB_CBCR},
	{"mmss_mmssnoc_bto_ahb", CLK_MON_MMSS_MMSS_MMSSNOC_BTO_AHB_CBCR},
	{"mmss_mmssnoc_axi", CLK_MON_MMSS_MMSS_MMSSNOC_AXI_CBCR},
	{"mmss_s0_axi", CLK_MON_MMSS_MMSS_S0_AXI_CBCR},
	/* MDSS Clocks,End */
	/* 3D Domain Clocks */
	{"oxili_gfx3d", CLK_MON_MMSS_OXILI_GFX3D_CBCR},
	{"oxilicx_ahb", CLK_MON_MMSS_OXILI_AHB_CBCR},
	{"oxilicx_axi", CLK_MON_MMSS_OXILICX_AXI_CBCR},
	/* 3D Domain Clocks,End */
	/* Venus Sub-System Clocks */
	{"venus0_ahb", CLK_MON_VENUS0_AHB_CBCR},
	{"venus0_axi", CLK_MON_VENUS0_AXI_CBCR},
	{"venus0_vcodec0", CLK_MON_VENUS0_VCODEC0_CBCR},
	/* Venus Sub-System Clocks, End */
	/* Any Missing Clocks to be added here */
	{{0}, 0},
};

/* Useage - echo "Register_Address" > check_reg
	Eg. Input - echo 0xFC400644 > check_reg
	>> Output - cat check_reg
	>> [0xfc400644] 0x80000000
*/
static int clk_mon_ioc_check_reg(struct clk_mon_ioc_buf __user *uarg)
{
	struct clk_mon_ioc_buf *karg = NULL;
	void __iomem *v_addr = NULL;
	int size = sizeof(struct clk_mon_ioc_buf);
	int ret = -EFAULT;
	int i;

	if (!access_ok(VERIFY_WRITE, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	if (copy_from_user(karg, uarg, size)) {
		ret = -EFAULT;
		goto out;
	}

	for (i = 0; i < karg->nr_addrs; i++) {
		v_addr = ioremap((unsigned int)karg->reg[i].addr, SIZE_REG);
		karg->reg[i].value = ioread32(v_addr);
		iounmap(v_addr);
	}

	if (copy_to_user(uarg, karg, size)) {
		ret = -EFAULT;
		goto out;
	}
	ret = 0;

out:
	kfree(karg);
	return ret;
}

static int clk_mon_ioc_check_power_domain(struct clk_mon_ioc_buf __user *uarg)
{
	struct clk_mon_ioc_buf *karg = NULL;
	unsigned int dom_en = 0;
	int size = sizeof(struct clk_mon_ioc_buf);
	int ret = -EFAULT;
	int i;
	unsigned int num_domains = 0;
	static struct regulator *regulator_pm;

	if (!access_ok(VERIFY_WRITE, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	num_domains = sizeof(power_domain_masks)/sizeof(power_domain_masks[0]);

	for (i = 0; i < num_domains; i++) {
		regulator_pm = regulator_get(NULL, power_domain_masks[i].name);
		if (IS_ERR(regulator_pm)) {
			pr_err("%s - Failed to get [%s] regulator\n",
				__func__, power_domain_masks[i].name);
		} else {
			dom_en = regulator_is_enabled(regulator_pm);
			if (dom_en < 0)	{
				pr_err("%s - Getting status of [%s] regulator failed\n",
					__func__, power_domain_masks[i].name);
			} else {
				strlcpy(karg->reg[i].name,
					power_domain_masks[i].name,
					sizeof(karg->reg[i].name));
				karg->reg[i].value = dom_en;
				/* Free the regulator from the consumer list
				else suspend would be prevented
				*/
				regulator_put(regulator_pm);
				regulator_pm = NULL;
				karg->nr_addrs++;
			}
		}
	}

	if (copy_to_user(uarg, karg, size)) {
		ret = -EFAULT;
		goto out;
	}

	ret = 0;

out:
	kfree(karg);
	return ret;
}

static int clk_mon_ioc_check_clock_gating(struct clk_mon_ioc_buf __user *uarg)
{
	struct clk_mon_ioc_buf *karg = NULL;
	unsigned int val = 0, value = 0;
	int size = sizeof(struct clk_mon_ioc_buf);
	int ret = -EFAULT;
	int i;
	void __iomem *v_addr = NULL;

	if (!access_ok(VERIFY_WRITE, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	for (i = 0; clk_gate_masks[i].addr != 0; i++) {
		v_addr = ioremap((unsigned int)clk_gate_masks[i].addr,
				SIZE_REG);
		value = ioread32(v_addr);
		/* Bit 31 indicates whether CLK is ON/OFF
		CLK_OFF : 0 - CLK ON, 1 - CLK OFF
		*/
		val = CHECK_BIT_SET((unsigned int) value, CLK_STATUS_BIT_POS);
		/* The output contains the register_name, address & value */
		strlcpy(karg->reg[i].name,
			clk_gate_masks[i].name,
			sizeof(karg->reg[i].name));
		karg->reg[i].addr = (void *) (&(clk_gate_masks[i].addr));
		karg->reg[i].value = val;
		karg->nr_addrs++;
	}

	if (copy_to_user(uarg, karg, size)) {
		ret = -EFAULT;
		goto out;
	}

	ret = 0;

out:
	kfree(karg);
	return ret;
}

/* Useage - echo "Register_Address" "Value_to_be_set" > set_reg
	Eg. Input - echo 0xFC400648 0x1 > set_reg
	>> Output - cat check_reg
	>> [0xfc400648] 0x00000001
*/
static int clk_mon_ioc_set_reg(struct clk_mon_reg_info __user *uarg)
{
	struct clk_mon_reg_info *karg = NULL;
	void __iomem *v_addr = NULL;
	int size = sizeof(struct clk_mon_reg_info);
	int ret = 0;

	if (!access_ok(VERIFY_READ, uarg, size))
		return -EFAULT;

	karg = kzalloc(size, GFP_KERNEL);

	if (!karg)
		return -ENOMEM;

	if (copy_from_user(karg, uarg, size)) {
		ret = -EFAULT;
		goto out;
	}

	v_addr = ioremap((unsigned int)karg->addr, SIZE_REG);
	iowrite32(karg->value, v_addr);
	iounmap(v_addr);

	ret = 0;

out:
	kfree(karg);
	return ret;
}

static long clk_mon_ioctl(struct file *filep, unsigned int cmd,
		unsigned long arg)
{
	struct clk_mon_ioc_buf __user *uarg = NULL;
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!arg)
		return -EINVAL;

	uarg = (struct clk_mon_ioc_buf __user *)arg;

	switch (cmd) {
	case CLK_MON_IOC_CHECK_REG:
		ret = clk_mon_ioc_check_reg(uarg);
		break;
	case CLK_MON_IOC_CHECK_POWER_DOMAIN:
		ret = clk_mon_ioc_check_power_domain(uarg);
		break;
	case CLK_MON_IOC_CHECK_CLOCK_DOMAIN:
		ret = clk_mon_ioc_check_clock_gating(uarg);
		break;
	case CLK_MON_IOC_SET_REG:
		ret = clk_mon_ioc_set_reg(
				(struct clk_mon_reg_info __user *)arg);
		break;
	default:
		pr_err("%s:Invalid ioctl\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static unsigned int g_reg_addr;
static unsigned int g_reg_value;

static ssize_t clk_mon_store_check_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg_addr = 0;
	char *cur = NULL;
	int ret = 0;

	if (!buf)
		return -EINVAL;

	cur = strnstr(buf, "0x", sizeof(buf));

	if (cur && cur + 2)
		ret = sscanf(cur + 2, "%x", &reg_addr);

	if (!ret)
		return -EINVAL;

	g_reg_addr = reg_addr;

	return size;
}

static ssize_t clk_mon_show_check_reg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	void __iomem *v_addr = NULL;
	unsigned int p_addr = 0;
	unsigned int value = 0;
	ssize_t size = 0;

	if (!g_reg_addr)
		return -EINVAL;

	p_addr = g_reg_addr;
	v_addr = ioremap(p_addr, SIZE_REG);

	value = ioread32(v_addr);
	iounmap(v_addr);

	size += snprintf(buf + size, CLK_MON_BUF_SIZE,
		"[0x%x] 0x%x\n", p_addr, value) + 1;

	return size + 1;
}

static ssize_t clk_mon_store_set_reg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg_addr = 0;
	unsigned int reg_value = 0;
	void __iomem *v_addr = NULL;
	char tmp_addr[9] = {0};
	char *cur = NULL;

	if (!buf)
		return -EINVAL;

	cur = strnstr(buf, "0x", strlen(buf));

	if (!cur || !(cur + 2))
		return -EINVAL;

	strlcpy(tmp_addr, cur + 2, 8);

	if (!sscanf(tmp_addr, "%x", &reg_addr))
		return -EFAULT;

	cur = strnstr(&cur[2], "0x", strlen(&cur[2]));

	if (!cur || !(cur + 2))
		return -EINVAL;

	if (!sscanf(cur + 2, "%x", &reg_value))
		return -EFAULT;

	g_reg_addr  = reg_addr;
	g_reg_value = reg_value;

	v_addr = ioremap(g_reg_addr, SIZE_REG);
	iowrite32(g_reg_value, v_addr);
	iounmap(v_addr);

	return size;
}

static const int NR_BIT = 8 * sizeof(unsigned int);
static const int IDX_SHIFT = 5;

int clk_mon_power_domain(unsigned int *pm_status)
{
	unsigned int dom_en = 0;
	int i, bit_shift, idx;
	unsigned int num_domains = 0;
	static struct regulator *regulator_pm;
	/* In total, 62+ regulators are present */
	int bit_max = NR_BIT * PWR_DOMAINS_NUM;

	num_domains = sizeof(power_domain_masks)/sizeof(power_domain_masks[0]);

	/* Parse through the list of regulators & based on request from the
	consumers of the regulator, it would be enabled/disabled i.e. ON/OFF
	*/

	if (!pm_status || bit_max < 0 || num_domains <= 0)
		return -EINVAL;

	memset(pm_status, 0, sizeof(unsigned int) * PWR_DOMAINS_NUM);

	for (i = 0; i < num_domains; i++) {
		if (i > bit_max) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
				__func__, i, bit_max);
			break;
		}
		regulator_pm = regulator_get(NULL, power_domain_masks[i].name);
		if (IS_ERR(regulator_pm)) {
			pr_err("%s - Failed to get [%s] regulator\n",
				__func__, power_domain_masks[i].name);
		} else {
			idx = (i >> IDX_SHIFT);
			bit_shift = (i % NR_BIT);
			/* Check the regulator status */
			dom_en = regulator_is_enabled(regulator_pm);
			if (dom_en < 0) {
				pr_err("%s-Getting status of [%s] regulator failed\n",
					__func__, power_domain_masks[i].name);
			} else {
				if (dom_en)
					pm_status[idx] |= (0x1 << bit_shift);
				else
					pm_status[idx] &= ~(0x1 << bit_shift);
				regulator_put(regulator_pm);
				regulator_pm = NULL;
			}
		}
	}
	return i;
}

int clk_mon_get_power_info(unsigned int *pm_status, char *buf)
{
	int i, bit_shift, idx, size = 0;
	unsigned int num_domains = 0, dom_en = 0;
	int bit_max = NR_BIT * PWR_DOMAINS_NUM;

	num_domains = sizeof(power_domain_masks)/sizeof(power_domain_masks[0]);

	if  ((!pm_status) || (!buf) || (num_domains <= 0))
		return -EINVAL;

	for (i = 0; i < num_domains; i++) {
		if (i > bit_max) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
				__func__, i, NR_BIT);
			break;
		}

		bit_shift = i % NR_BIT;
		idx = i >> IDX_SHIFT;
		dom_en = 0;
		/* If the bit is set indicates that the regulator is enabled as
		observed in the API clk_mon_power_domain.
		*/
		dom_en = CHECK_BIT_SET(pm_status[idx], bit_shift);

		size += snprintf(buf + size, CLK_MON_BUF_SIZE,
				"[%-15s] %-3s\n",
				power_domain_masks[i].name,
				(dom_en) ? "on" : "off");
	}
	return size + 1;
}

int clk_mon_clock_gate(unsigned int *clk_status)
{
	int bit_max = NR_BIT * CLK_GATES_NUM;
	unsigned int val = 0, value = 0;
	void __iomem *v_addr = NULL;
	unsigned long addr = 0;
	unsigned int clk_dis = 0;
	int i, bit_shift, idx;

	if (!clk_status || bit_max < 0)
		return -EINVAL;

	memset(clk_status, 0, sizeof(unsigned int) * CLK_GATES_NUM);

	for (i = 0; clk_gate_masks[i].addr != 0; i++) {
		if (i >= bit_max) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
				__func__, i, bit_max);
			break;
		}

		if (addr != clk_gate_masks[i].addr) {
			/* addr = clk_gate_masks[i].addr; */
			v_addr = ioremap((unsigned int)clk_gate_masks[i].addr,
					SIZE_REG);
			value = ioread32(v_addr);
		}
		/* Bit 31 indicates whether CLK is ON/OFF
		 * 0 - CLK ON,1 - CLK OFF
		*/
		val = CHECK_BIT_SET((unsigned int) value, CLK_STATUS_BIT_POS);
		clk_dis = val;

		idx = i >> IDX_SHIFT;
		bit_shift = i % NR_BIT;

		if (clk_dis)
			clk_status[idx] &= ~(BIT_ONE << bit_shift);
		else
			clk_status[idx] |= (BIT_ONE << bit_shift);

		/* Unmap the memory */
		if (v_addr != NULL)
			iounmap(v_addr);
	}
	return i;
}

int clk_mon_get_clock_info(unsigned int *clk_status, char *buf)
{
	unsigned long addr = 0;
	int bit_max = NR_BIT * CLK_GATES_NUM;
	int bit_shift, idx;
	int size = 0;
	int val, i;
	void __iomem *v_addr = NULL;
	unsigned int value = 0;

	if (!clk_status || !buf)
		return -EINVAL;

	for (i = 0; clk_gate_masks[i].addr != 0; i++) {
		if (i >= bit_max) {
			pr_err("%s: Error Exceed storage size %d(%d)\n",
				__func__, i, bit_max);
			break;
		}

		if (addr != clk_gate_masks[i].addr) {
			addr = clk_gate_masks[i].addr;
			v_addr = ioremap((unsigned int)clk_gate_masks[i].addr,
					SIZE_REG);
			value = ioread32(v_addr);
			size += snprintf(buf + size, CLK_MON_BUF_SIZE,
				"\n[0x%x]\n",
				((unsigned int) clk_gate_masks[i].addr));
			if (v_addr != NULL)
				iounmap(v_addr);
		}

		bit_shift = i % NR_BIT;
		idx = i >> IDX_SHIFT;
		/* If the bit is set indicates that the clock is enabled as
		observed in the API clk_mon_clock_gate.
		*/
		val = CHECK_BIT_SET(clk_status[idx], bit_shift);

		size += snprintf(buf + size, CLK_MON_BUF_SIZE,
				" %-20s\t: %s\n", clk_gate_masks[i].name,
				(val) ? "on" : "off");
	}
	return size;
}

static ssize_t clk_mon_show_power_domain(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int val = 0;
	ssize_t size = 0;
	static struct regulator *regulator_pm;
	unsigned int num_domains = 0;
	int i;

	num_domains = sizeof(power_domain_masks)/sizeof(power_domain_masks[0]);

	memset(buf, 0, sizeof(buf));

	/* Parse through the list of regulators & based on request from the
	consumers of the regulator, it would be enabled/disabled i.e. ON/OFF
	*/
	for (i = 0; i < num_domains; i++) {
		regulator_pm = regulator_get(NULL, power_domain_masks[i].name);
		if (IS_ERR(regulator_pm)) {
			pr_err("Failed to get [%s] regulator\n",
				power_domain_masks[i].name);
		} else {
			val = regulator_is_enabled(regulator_pm);
			if (val < 0) {
				pr_err("Getting status of [%s] regulator failed\n",
					power_domain_masks[i].name);
			} else {
				regulator_put(regulator_pm);
				regulator_pm = NULL;
			}
		}
		size += snprintf(buf + size, CLK_MON_BUF_SIZE,
			" %-15s\t: %s\n",
			power_domain_masks[i].name, (val) ? "on" : "off");
	}
	return size + 1;
}

static ssize_t clk_mon_show_clock_gating(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	void __iomem *v_addr = NULL;
	unsigned int val  = 0;
	unsigned int value = 0;
	ssize_t size = 0;
	int i;

	for (i = 0; clk_gate_masks[i].addr != 0; i++) {
		v_addr = ioremap((unsigned int)clk_gate_masks[i].addr,
				SIZE_REG);
		value = ioread32(v_addr);
		val = CHECK_BIT_SET((unsigned int) value,
				CLK_STATUS_BIT_POS);

		size += snprintf(buf + size, CLK_MON_BUF_SIZE,
				" %-20s\t: %s\n",
				clk_gate_masks[i].name, (val) ? "off" : "on");
		iounmap(v_addr);
	}

	return size + 1;
}


static DEVICE_ATTR(check_reg, S_IRUSR | S_IWUSR,
		clk_mon_show_check_reg, clk_mon_store_check_reg);
static DEVICE_ATTR(set_reg, S_IWUSR, NULL, clk_mon_store_set_reg);
static DEVICE_ATTR(power_domain, S_IRUSR, clk_mon_show_power_domain, NULL);
static DEVICE_ATTR(clock_gating, S_IRUSR, clk_mon_show_clock_gating, NULL);

static struct attribute *clk_mon_attributes[] = {
	&dev_attr_check_reg.attr,
	&dev_attr_set_reg.attr,
	&dev_attr_power_domain.attr,
	&dev_attr_clock_gating.attr,
	NULL,
};

static struct attribute_group clk_mon_attr_group = {
	.attrs = clk_mon_attributes,
	.name  = "check",
};

static const struct file_operations clk_mon_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = clk_mon_ioctl,
};

static struct miscdevice clk_mon_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "clk_mon",
	.fops  = &clk_mon_fops,
};

static int __init clk_mon_init(void)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = misc_register(&clk_mon_device);

	if (ret) {
		pr_err("%s: Unable to register clk_mon_device\n", __func__);
		goto err_misc_register;
	}

	ret = sysfs_create_group(&clk_mon_device.this_device->kobj,
			&clk_mon_attr_group);

	if (ret) {
		pr_err("%s: Unable to Create sysfs node\n", __func__);
		goto err_create_group;
	}

	return 0;

err_create_group:
	misc_deregister(&clk_mon_device);
err_misc_register:
	return ret;
}

static void __exit clk_mon_exit(void)
{
	misc_deregister(&clk_mon_device);
}

module_init(clk_mon_init);
module_exit(clk_mon_exit);

MODULE_AUTHOR("Himanshu Sheth <himanshu.s@samsung.com>");
MODULE_DESCRIPTION("Clock Gate Monitor");
MODULE_LICENSE("GPL");
