/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

 #ifndef MDSS_SAMSUNG_PANEL_H
 #define MDSS_SAMSUNG_PANEL_H

 #include "mdss_panel.h"
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
 #include <linux/smart_dimming_s6e63j0x03.h>
#endif

#define MAX_BRIGHTNESS_LEVEL 255
#define MID_BRIGHTNESS_LEVEL 143
#define LOW_BRIGHTNESS_LEVEL 20
#define DIM_BRIGHTNESS_LEVEL 30
#define MAX_MTP_CNT		27
#define GAMMA_CMD_CNT		28
#define LDI_ID_REG		0xD3
#define LDI_MTP_SET3		0xD4
#define LDI_ID_LEN		6
#define LDI_MTP_SET3_LEN	1
#define MAX_GAMMA_CNT		11
#define CANDELA_10		10
#define CANDELA_30		30
#define CANDELA_60		60
#define CANDELA_90		90
#define CANDELA_120		120
#define CANDELA_150		150
#define CANDELA_200		200
#define CANDELA_240		240
#define CANDELA_300		300
#define ACL_CMD_CNT		66
#define MIN_ACL			0
#define MAX_ACL			3
#if defined(CONFIG_FB_MSM_MDSS_TC_DSI2LVDS_WXGA_PANEL)
#define BL_MIN_BRIGHTNESS			2
#define BL_MAX_BRIGHTNESS_LEVEL		126
#define BL_MID_BRIGHTNESS_LEVEL		65
#define BL_LOW_BRIGHTNESS_LEVEL		2
#define BL_DIM_BRIGHTNESS_LEVEL		4
#define BL_DEFAULT_BRIGHTNESS		BL_MID_BRIGHTNESS_LEVEL
#else
#define BL_MIN_BRIGHTNESS			2
#define BL_MAX_BRIGHTNESS_LEVEL		230
#define BL_MID_BRIGHTNESS_LEVEL		115
#define BL_LOW_BRIGHTNESS_LEVEL		2
#define BL_DIM_BRIGHTNESS_LEVEL		9
#define BL_DEFAULT_BRIGHTNESS		BL_MID_BRIGHTNESS_LEVEL
#endif
enum {
	MIPI_RESUME_STATE,
	MIPI_SUSPEND_STATE,
};

static const short candela_tbl[MAX_GAMMA_CNT] = {
	CANDELA_10,
	CANDELA_30,
	CANDELA_60,
	CANDELA_90,
	CANDELA_120,
	CANDELA_150,
	CANDELA_200,
	CANDELA_200,
	CANDELA_240,
	CANDELA_300,
	CANDELA_300,
};

struct display_status {
	unsigned char auto_brightness;
	int bright_level;
	int siop_status;
};

struct mdss_samsung_driver_data {
	struct dsi_buf sdc_tx_buf;
	struct msm_fb_data_type *mfd;
	struct dsi_buf sdc_rx_buf;
	struct mdss_panel_common_pdata *mdss_sdc_disp_pdata;
	struct mdss_panel_data *mpd;
	struct mutex lock;
	unsigned int manufacture_id;
	unsigned char ldi_id[LDI_ID_LEN];
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

#if defined(CONFIG_FB_MSM_MDSS_SDC_WXGA_PANEL)
	int bl_on_gpio;
	int bl_rst_gpio;
	int bl_ldi_en;
#elif defined(CONFIG_FB_MSM_MDSS_TC_DSI2LVDS_WXGA_PANEL)
	int lvds_supply_en;
	int bl_on_gpio;
	int bl_ap_pwm;
	int bl_rst_gpio;
	int bl_wled;
	int bl_ldi_en;
#elif defined(CONFIG_FB_MSM_MDSS_SAMSUNG_OLED_PANEL)
	int disp_sel_en;
	int mipi_sel_gpio;
	int en_on_gpio;
	int oled_det_gpio;
#endif
	int lcd_io_en;
	int esd_det;
	int err_fg_irq;
	int esd_status;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct platform_device *msm_pdev;
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd;
#endif
#endif

#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct display_status dstat;
	struct dsi_panel_cmds bklt_cmds;
	struct dsi_panel_cmds bklt_ctrl_cmd;
	int power;
	int compensation_on;
	struct dsi_panel_cmds acl_off_cmd;
	struct dsi_panel_cmds acl_on_cmd;
	struct dsi_panel_cmds acl_sel_lock_cmd;
	struct dsi_panel_cmds acl_global_param_cmd;
	struct dsi_panel_cmds acl_lut_enable_cmd;
	struct dsi_panel_cmds acl_table;
	struct dsi_panel_cmds pos_hbm_cmds;
	struct dsi_panel_cmds hbm_white_cmd;
	struct dsi_panel_cmds partial_area_set_cmd;
	struct dsi_panel_cmds mode_cmds;
	struct dsi_panel_cmds test_key_on_cmd;
	struct dsi_panel_cmds test_key_off_cmd;
	struct dsi_panel_cmds ldi_id_cmd;
	struct dsi_panel_cmds ldi_mtp_cmd;
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
	unsigned char *gamma_tbl[MAX_GAMMA_CNT];
	struct smart_dimming *dimming;
#endif
	int boot_power_on;
	unsigned int acl;
	unsigned char elvss_hbm;
	unsigned char default_hbm;
	bool hbm_on;
	bool alpm_on;
	bool alpm_mode;
	struct mutex mlock;
};

void mdnie_lite_tuning_init(struct mdss_samsung_driver_data *msd);
void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_cmd_desc *cmds, int cnt, int flag);
extern unsigned int system_rev;
static int lcd_attached = 1;

int get_lcd_attached(void)
{
	return lcd_attached;
}
EXPORT_SYMBOL(get_lcd_attached);

static int __init lcd_attached_status(char *mode)
{
	/*
	*	1 is lcd attached
	*	0 is lcd detached
	*/

	if (strncmp(mode, "1", 1) == 0)
		lcd_attached = 1;
	else
		lcd_attached = 0;

	pr_info("%s %s", __func__, lcd_attached == 1 ?
				"lcd_attached" : "lcd_detached");
	return 1;
}
__setup("lcd_attached=", lcd_attached_status);
#endif
