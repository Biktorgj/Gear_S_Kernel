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

 #ifndef MDSS_DSI_PANEL_OLED_H
 #define MDSS_DSI_PANEL_OLED_H

 #include "mdss_panel.h"
#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
 #include <linux/smart_dimming_s6e36w0x01.h>
#endif
#define GAMMA_CMD_CNT		34
#define LDI_MTP_SET			0xCA
#define LDI_HBM_ELVSS		0xB6
#define MIN_ACL				0
#define MAX_ACL				3
#define MAX_MTP_CNT		33
#define LDI_ID_LEN			3
#define LDI_HBM_MTP_LEN1		6
#define LDI_HBM_MTP_LEN2		12
#define LDI_HBM_MTP_LEN3		4
#define LDI_HBM_MTP_LEN4		1

struct mdss_samsung_driver_data {
	struct dsi_buf sdc_tx_buf;
	struct msm_fb_data_type *mfd;
	struct dsi_buf sdc_rx_buf;
	struct mdss_panel_common_pdata *mdss_sdc_disp_pdata;
	struct mdss_panel_data *mpd;
	struct mutex lock;
	unsigned char ldi_id[LDI_ID_LEN];
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;

	int disp_sel_en;
	int mipi_sel_gpio;
	int en_on_gpio;
	int oled_det_gpio;
	int lcd_io_en;
	int oled_id_gpio;
	int esd_det;
	int err_fg;
	int err_irq;
	int esd_irq;
	int esd_status;
	int temp_stage;
	unsigned char *br_map;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct platform_device *msm_pdev;
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd;
#endif
#endif
	int power;
	int compensation_on;
	struct dsi_panel_cmds update_gamma;
	struct dsi_panel_cmds acl_off_cmd;
	struct dsi_panel_cmds acl_on_cmd;
	struct dsi_panel_cmds acl_table;
	struct dsi_panel_cmds pos_hbm_cmds;
	struct dsi_panel_cmds aor_cmds;
	struct dsi_panel_cmds elvss_cmds;
	struct dsi_panel_cmds hbm_white_cmd;
	struct dsi_panel_cmds partial_area_set_cmd;
	struct dsi_panel_cmds alpm_on_cmds;
	struct dsi_panel_cmds alpm_on_temp_cmds;
	struct dsi_panel_cmds alpm_off_cmds;
	struct dsi_panel_cmds tset_on_cmds;
	struct dsi_panel_cmds tset_off_cmd;
	struct dsi_panel_cmds test_key_on_cmd;
	struct dsi_panel_cmds test_key_off_cmd;
	struct dsi_panel_cmds ldi_id_cmd;
	struct dsi_panel_cmds ldi_mtp_cmd;
	struct dsi_panel_cmds read_hbm_mtp_cmds;
	struct dsi_panel_cmds hbm_cmd;
	struct dsi_panel_cmds hbm_elvss_cmd;
	struct dsi_panel_cmds mdnie_ctl_cmds;
	struct dsi_panel_cmds mdnie_off_cmd;
#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
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
	bool mdnie_on;
};

void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_cmd_desc *cmds, int cnt, int flag);
extern unsigned int system_rev;
static int lcd_attached = 1;
static int lcd_id;

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

int get_lcd_id(void)
{
	return lcd_id;
}
EXPORT_SYMBOL(get_lcd_id);

static int __init mdss_dsi_panel_id_cmdline(char *mode)
{
	char *pt;

	lcd_id = 0;
	if (mode == NULL)
		return 1;

	for (pt = mode; *pt != 0; pt++) {
		lcd_id <<= 4;
		switch (*pt) {
		case '0' ... '9':
			lcd_id += *pt - '0';
		break;
		case 'a' ... 'f':
			lcd_id += 10 + *pt - 'a';
		break;
		case 'A' ... 'F':
			lcd_id += 10 + *pt - 'A';
		break;
		}
	}

	pr_info("%s: Panel_ID = 0x%x", __func__, lcd_id);

	return 0;
}
__setup("lcd_id=0x", mdss_dsi_panel_id_cmdline);
extern int max77836_comparator_set(bool enable);
#endif
