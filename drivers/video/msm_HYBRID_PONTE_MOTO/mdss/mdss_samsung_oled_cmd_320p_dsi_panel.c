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

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/err.h>
#ifdef CONFIG_LCD_CLASS_DEVICE
	#include <linux/lcd.h>
#endif
#include "mdss_fb.h"
#include "mdss_dsi.h"
#include "mdss_samsung_dsi_panel_msm8x26.h"

#define DT_CMD_HDR 6
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
struct work_struct  err_fg_work;
struct delayed_work err_fg_enable;
static int esd_count;
static int err_fg_working;
static struct class *esd_class;
static struct device *esd_dev;
enum esd_status {
	ESD_NONE,
	ESD_DETECTED,
};
#define ESD_DEBUG 1
#endif

DEFINE_LED_TRIGGER(bl_led_trigger);
static struct mdss_samsung_driver_data msd;
static void panel_enter_hbm_mode(void);
static void panel_alpm_on(unsigned int enable);
void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	if (!gpio_is_valid(ctrl->pwm_pmic_gpio)) {
		pr_err("%s: pwm_pmic_gpio=%d Invalid\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ret = gpio_request(ctrl->pwm_pmic_gpio, "disp_pwm");
	if (ret) {
		pr_err("%s: pwm_pmic_gpio=%d request failed\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: lpg_chan=%d pwm request failed", __func__,
				ctrl->pwm_lpg_chan);
		gpio_free(ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
	}
}

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

u32 mdss_dsi_dcs_read(struct mdss_dsi_ctrl_pdata *ctrl,
			char cmd0, char cmd1)
{
	struct dcs_cmd_req cmdreq;

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rbuf = ctrl->rx_buf.data;
	cmdreq.rlen = 1;
	cmdreq.cb = NULL; /* call back */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, until call back called
	 */
	pr_info("%s: manufacture_id1=%x\n", __func__, *(ctrl->rx_buf.data));
	return 0;
}

void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int cnt, int flag)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;


	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

}
static void mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

unsigned char mdss_dsi_panel_pwm_scaling(int level)
{
	unsigned char scaled_level;

	if (level >= MAX_BRIGHTNESS_LEVEL)
		scaled_level  = BL_MAX_BRIGHTNESS_LEVEL;
	else if (level >= MID_BRIGHTNESS_LEVEL) {
		scaled_level  = (level - MID_BRIGHTNESS_LEVEL) *
		(BL_MAX_BRIGHTNESS_LEVEL - BL_MID_BRIGHTNESS_LEVEL) /
		(MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) +
		BL_MID_BRIGHTNESS_LEVEL;
	} else if (level >= DIM_BRIGHTNESS_LEVEL) {
		scaled_level  = (level - DIM_BRIGHTNESS_LEVEL) *
		(BL_MID_BRIGHTNESS_LEVEL - BL_DIM_BRIGHTNESS_LEVEL) /
		(MID_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) +
		BL_DIM_BRIGHTNESS_LEVEL;
	} else if (level >= LOW_BRIGHTNESS_LEVEL) {
		scaled_level  = (level - LOW_BRIGHTNESS_LEVEL) *
		(BL_DIM_BRIGHTNESS_LEVEL - BL_LOW_BRIGHTNESS_LEVEL) /
		(DIM_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) +
		BL_LOW_BRIGHTNESS_LEVEL;
	}  else{
		if (0/*poweroff_charging == 1*/)
			scaled_level  = level * BL_LOW_BRIGHTNESS_LEVEL /
					LOW_BRIGHTNESS_LEVEL;
		else
			scaled_level  = BL_MIN_BRIGHTNESS;
	}

	pr_info("level = [%d]: scaled_level = [%d]   autobrightness level:%d\n",
				level, scaled_level, msd.dstat.auto_brightness);

	return scaled_level;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static char lcd_cabc[2] = {0x55, 0x0};	/* CABC COMMAND : default disabled */
static struct dsi_cmd_desc cabc_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(lcd_cabc)},
	lcd_cabc
};

static void mdss_dsi_panel_cabc_dcs(struct mdss_dsi_ctrl_pdata *ctrl,
						int siop_status)
{

	struct dcs_cmd_req cmdreq;

	pr_debug("%s: cabc=%d\n", __func__, siop_status);
	lcd_cabc[1] = (unsigned char)siop_status;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &cabc_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}
#endif

void mdss_dsi_panel_io_enable(int enable)
{
	if (gpio_is_valid(msd.lcd_io_en))
		gpio_set_value(msd.lcd_io_en, enable);
}

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -ENOMEM;
	}

	if (msd.alpm_mode)
		return 0;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
				__func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	pr_info("%s:enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		int rc = 0;
		msleep(20);
		if (gpio_is_valid(msd.mipi_sel_gpio))
			gpio_set_value(msd.mipi_sel_gpio, 1);

		usleep_range(1000, 1000);
		if (gpio_is_valid(ctrl_pdata->rst_gpio)) {
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			msleep(20);
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			usleep_range(1000, 1000);
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			msleep(20);
		}
		if (gpio_is_valid(msd.en_on_gpio))
			gpio_set_value(msd.en_on_gpio, 1);

		if (gpio_is_valid(msd.disp_sel_en)) {
			rc = gpio_tlmm_config(GPIO_CFG(msd.disp_sel_en, 0,
				GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
				GPIO_CFG_ENABLE);
			if (rc)
				pr_err("request disp_sel_en failed, rc=%d\n",
									rc);
		}

		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}

		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
					__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		if (gpio_is_valid(ctrl_pdata->rst_gpio))
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
	}
	return 0;
}

static char caset[] = {0x2a, 0x00, 0x00, 0x03, 0x00};	/* DTYPE_DCS_LWRITE */
static char paset[] = {0x2b, 0x00, 0x00, 0x05, 0x00};	/* DTYPE_DCS_LWRITE */

static struct dsi_cmd_desc partial_update_enable_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(caset)}, caset},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(paset)}, paset},
};

static int mdss_dsi_panel_partial_update(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct dcs_cmd_req cmdreq;
	int rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	caset[1] = (((pdata->panel_info.roi_x) & 0xFF00) >> 8);
	caset[2] = (((pdata->panel_info.roi_x) & 0xFF));
	caset[3] = (((pdata->panel_info.roi_x - 1 + pdata->panel_info.roi_w)
								& 0xFF00) >> 8);
	caset[4] = (((pdata->panel_info.roi_x - 1 + pdata->panel_info.roi_w)
								& 0xFF));
	partial_update_enable_cmd[0].payload = caset;

	paset[1] = (((pdata->panel_info.roi_y) & 0xFF00) >> 8);
	paset[2] = (((pdata->panel_info.roi_y) & 0xFF));
	paset[3] = (((pdata->panel_info.roi_y - 1 + pdata->panel_info.roi_h)
								& 0xFF00) >> 8);
	paset[4] = (((pdata->panel_info.roi_y - 1 + pdata->panel_info.roi_h)
								& 0xFF));
	partial_update_enable_cmd[1].payload = paset;

	pr_debug("%s: enabling partial update\n", __func__);
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = partial_update_enable_cmd;
	cmdreq.cmds_cnt = 2;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_runtime_active(ctrl, true, __func__);

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	mdss_dsi_runtime_active(ctrl, false, __func__);

	return rc;
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pr_err("%s:@@@@@@@@ bklt_ctrl:%d :bl_level=%d\n", __func__,
			ctrl_pdata->bklt_ctrl, bl_level);
	/*
	 * In tizen brightness is controlled using backlight node
	 * of backlight class by platform only. so no need to update
	 * gamma values for brightness setting from splash thread in
	 * kernel
	 */
}


u32 mdss_dsi_cmd_receive(struct mdss_dsi_ctrl_pdata *ctrl,
		struct dsi_cmd_desc *cmd, int rlen)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rbuf = ctrl->rx_buf.data;
	cmdreq.rlen = rlen;
	cmdreq.cb = NULL; /* call back */
	/*
	 * This mutex is to sync up with dynamic FPS changes
	 * so that DSI lockups shall not happen
	 */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	/*
	 * blocked here, untill call back called
	 */
	return ctrl->rx_buf.len;
}

#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
static void panel_get_gamma_table(struct mdss_samsung_driver_data *msdd,
						const unsigned char *data)
{
	int i;

	panel_read_gamma(msdd->dimming, data);
	panel_generate_volt_tbl(msdd->dimming);

	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		msd.gamma_tbl[i][0] = LDI_MTP_SET3;
		panel_get_gamma(msdd->dimming, candela_tbl[i],
					&msdd->gamma_tbl[i][1]);
	}
}
#endif

static int mdss_dsi_panel_read_mtp(struct mdss_panel_data *pdata)
{
	int length;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
	unsigned char mtp_data[MAX_MTP_CNT];
#endif
	struct dsi_cmd_desc test_key_on_cmd, test_key_off_cmd,
				ldi_id_cmd, ldi_mtp_cmd;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);
	test_key_on_cmd = msd.test_key_on_cmd.cmds[0];
	test_key_off_cmd = msd.test_key_off_cmd.cmds[0];
	ldi_id_cmd = msd.ldi_id_cmd.cmds[0];
	ldi_mtp_cmd = msd.ldi_mtp_cmd.cmds[0];

	/* ID */
	mdss_dsi_cmds_send(ctrl, &test_key_on_cmd, 1, 0);

	length = mdss_dsi_cmd_receive(ctrl, &ldi_id_cmd, LDI_ID_LEN);
	memcpy(msd.ldi_id, ctrl->rx_buf.data, LDI_ID_LEN);
	pr_debug("length[%d], panel id is %d.%d.%d\n", length,
				msd.ldi_id[3], msd.ldi_id[2], msd.ldi_id[5]);

	mdss_dsi_cmds_send(ctrl, &test_key_off_cmd, 1, 0);

	/* HBM */
	mdss_dsi_cmds_send(ctrl, &test_key_on_cmd, 1, 0);
	mdss_dsi_cmds_send(ctrl, &msd.pos_hbm_cmds.cmds[0], 1, 0);
	length = mdss_dsi_cmd_receive(ctrl, &ldi_mtp_cmd, LDI_MTP_SET3_LEN);
	msd.elvss_hbm = ctrl->rx_buf.data[0];
	pr_info("HBM ELVSS: 0x%x\n", msd.elvss_hbm);

	mdss_dsi_cmds_send(ctrl, &msd.pos_hbm_cmds.cmds[1], 1, 0);
	length = mdss_dsi_cmd_receive(ctrl, &ldi_id_cmd, LDI_MTP_SET3_LEN);
	msd.default_hbm = ctrl->rx_buf.data[0];
	pr_info("DEFAULT ELVSS: 0x%x\n", msd.default_hbm);

	mdss_dsi_cmds_send(ctrl, &msd.pos_hbm_cmds.cmds[2], 1, 0);
	mdss_dsi_cmds_send(ctrl, &test_key_off_cmd, 1, 0);

	/* GAMMA */
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
	mdss_dsi_cmds_send(ctrl, &test_key_on_cmd, 1, 0);
	length = mdss_dsi_cmd_receive(ctrl, &ldi_mtp_cmd, MAX_MTP_CNT);
	pr_debug("length[%d]\n", length);
	if (!length) {
		pr_err("%s : LDI_MTP command failed\n", __func__);
		return -EIO;
	}
	memcpy(mtp_data, ctrl->rx_buf.data, MAX_MTP_CNT);

	mdss_dsi_cmds_send(ctrl, &test_key_off_cmd, 1, 0);
	panel_get_gamma_table(&msd, mtp_data);
#endif
	return 0;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	int err_en_time = 0;
#endif

	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	msd.mpd = pdata;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (msd.alpm_mode) {
		if (msd.alpm_on) {
			msd.power = FB_BLANK_UNBLANK;
			pr_info("%s,panel on skip due to alpm mode\n", __func__);
			return 0;
		} else {
			pr_info("%s, panel on ALPM OFF\n", __func__);
			panel_alpm_on(0);
		}
	}

	if (ctrl->on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);

	msd.mfd->resume_state = MIPI_RESUME_STATE;
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (!msd.boot_power_on)
		gpio_request(msd.esd_det, "err_fg");
	if (msd.boot_power_on)
		err_en_time = 3000;
	else
		err_en_time = 120;
	if (gpio_is_valid(msd.esd_det)) {
		if (msd.esd_status == ESD_DETECTED)
			msd.esd_status = ESD_NONE;

		schedule_delayed_work(&err_fg_enable, msecs_to_jiffies(err_en_time));
	}
#endif
	msd.power = FB_BLANK_UNBLANK;

	if (msd.boot_power_on) {
		mdss_dsi_panel_read_mtp(pdata);
		msd.boot_power_on = 0;
	}

	pr_info("%s.\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (msd.alpm_on && msd.alpm_mode) {
		msd.power = FB_BLANK_POWERDOWN;
		pr_info("%s. skip panel off controlled by clock\n", __func__);
		return 0;
	}

	mutex_lock(&msd.mlock);

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (!err_fg_working) {
		disable_irq_nosync(msd.err_fg_irq);
		cancel_delayed_work_sync(&err_fg_enable);
		cancel_work_sync(&err_fg_work);
	} else {
		cancel_delayed_work_sync(&err_fg_enable);
		cancel_work_sync(&err_fg_work);
	}
	gpio_free(msd.esd_det);
#endif
	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (msd.alpm_on) {
		pr_info("%s, skip due to alpm mode\n", __func__);
		goto alpm_event;
	}

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	msd.power = FB_BLANK_POWERDOWN;
	pr_info("%s.\n", __func__);

	mutex_unlock(&msd.mlock);

	return 0;
alpm_event:
	msd.power = FB_BLANK_POWERDOWN;
	panel_alpm_on(msd.alpm_on);
	mutex_unlock(&msd.mlock);
	return 0;
}

static int mdss_dsi_panel_parse_dcs_string_cmds(struct device_node *of_node,
		struct dsi_panel_cmds *pcmds,
		char *cmd_key, char *link_key)
{
	unsigned int out_value;
	const char *out_text, *out_prop, *out_payload;
	int ret = 0, i, count, total = 0;

	pr_debug("cmd_key[%s]link_state[%s]\n",
		cmd_key, link_key);

	count = of_property_count_strings(of_node, cmd_key);
	pr_debug("count[%d]\n", count);

	if (count < 0) {
		pr_err("failed to get cmd_key[%s]\n", cmd_key);
		return count;
	}

	pcmds->cmds = kzalloc(sizeof(struct dsi_cmd_desc) * count,
						GFP_KERNEL);

	if (!pcmds->cmds) {
		pr_err("failed to allocate cmds\n");
		return -ENOMEM;
	}

	pcmds->cmd_cnt = count;

	for (i = 0; i < count; i++) {
		struct dsi_ctrl_hdr *dchdr;
		ret = of_property_read_string_index(of_node,
			cmd_key, i, &out_prop);

		if (ret < 0) {
			pr_err("failed to get %d props string\n", i);
			return ret;
		}

		out_payload = of_get_property(of_node,
					out_prop, &out_value);
		if (!out_payload) {
			pr_err("failed to get %d payload string\n", i);
			return -EFAULT;
		}

		dchdr = (struct dsi_ctrl_hdr *)out_payload;
		dchdr->dlen = ntohs(dchdr->dlen);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = (char *)out_payload +
					sizeof(struct dsi_ctrl_hdr);

		pr_debug("%d:out_prop[%s]dtype[0x%x]dlen[%x]wait[%d]\n",
			i, out_prop, dchdr->dtype, dchdr->dlen, dchdr->wait);

		total += dchdr->dlen + sizeof(struct dsi_ctrl_hdr);
	}
	pcmds->blen = total;

	pcmds->link_state = DSI_LP_MODE; /* default */

	out_text = of_get_property(of_node, link_key, NULL);
	if (!strncmp(out_text, "dsi_hs_mode", 11))
		pcmds->link_state = DSI_HS_MODE;

	pr_debug("len[%d]cmd_cnt[%d]link_state[%d]\n",
		pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return ret;
}

static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, blen);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len > sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			return -ENOMEM;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		kfree(buf);
		return -ENOMEM;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds) {
		kfree(buf);
		return -ENOMEM;
	}

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}


	data = of_get_property(np, link_key, NULL);
	if (!strncmp(data, "dsi_hs_mode", 11))
		pcmds->link_state = DSI_HS_MODE;
	else
		pcmds->link_state = DSI_LP_MODE;
	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;
}
static int mdss_panel_dt_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
				char *dst_format)
{
	int rc = 0;
	switch (bpp) {
	case 3:
		*dst_format = DSI_CMD_DST_FORMAT_RGB111;
		break;
	case 8:
		*dst_format = DSI_CMD_DST_FORMAT_RGB332;
		break;
	case 12:
		*dst_format = DSI_CMD_DST_FORMAT_RGB444;
		break;
	case 16:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB565;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		}
		break;
	case 18:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB666;
			break;
		default:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		}
		break;
	case 24:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB888;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int mdss_panel_parse_dt_gpio(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	msd.mipi_sel_gpio = of_get_named_gpio(np,
			"qcom,mipi_sel-gpio", 0);
	if (!gpio_is_valid(msd.mipi_sel_gpio)) {
		pr_err("%s:%d mipi_sel_gpio not specified\n",
				__func__, __LINE__);
	} else {
		rc = gpio_request(msd.mipi_sel_gpio, "mipi_sel");
		if (rc) {
			pr_err("request mipi_sel_gpio   failed, rc=%d\n",
					rc);
			gpio_free(msd.mipi_sel_gpio);
		} else {
			rc = gpio_tlmm_config(GPIO_CFG(msd.mipi_sel_gpio, 0,
					GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
					GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			if (rc)
				pr_err("request mipi_sel failed, rc=%d\n", rc);
		}
	}
	msd.en_on_gpio = of_get_named_gpio(np,
			"qcom,en_on-gpio", 0);
	if (!gpio_is_valid(msd.en_on_gpio)) {
		pr_err("%s:%d en_on_gpio not specified\n",
				__func__, __LINE__);
	} else {
		rc = gpio_request(msd.en_on_gpio, "backlight_enable");
		if (rc) {
			pr_err("request en_on_gpio   failed, rc=%d\n",
					rc);
			gpio_free(msd.en_on_gpio);
		} else {
			rc = gpio_tlmm_config(GPIO_CFG(msd.en_on_gpio, 0,
					GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
					GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			if (rc)
				pr_err("request en_on_gpio failed,rc=%d\n", rc);
		}
	}

	msd.disp_sel_en = of_get_named_gpio(np,
			"qcom,disp_sel-gpio", 0);
	if (!gpio_is_valid(msd.disp_sel_en)) {
		pr_err("%s:%d, disp_sel_en gpio not specified\n",
				__func__, __LINE__);
	} else {
		rc = gpio_request(msd.disp_sel_en, "bl_ldi_en");
		if (rc) {
			pr_err("request disp_sel_en gpio failed, rc=%d\n",
					rc);
			gpio_free(msd.disp_sel_en);

		}
	}
	if (system_rev < 0x5)
		msd.lcd_io_en = -EINVAL;
	else
		msd.lcd_io_en = of_get_named_gpio(np,
				"qcom,lcd-io-en", 0);
	if (!gpio_is_valid(msd.lcd_io_en)) {
		pr_err("%s:%d en_on_gpio not specified\n",
				__func__, __LINE__);
	} else {
		rc = gpio_request(msd.lcd_io_en, "lcd_io_enable");
		if (rc) {
			pr_err("request lcd_io_en   failed, rc=%d\n",
					rc);
			gpio_free(msd.lcd_io_en);
		} else {
			rc = gpio_tlmm_config(GPIO_CFG(msd.lcd_io_en, 0,
					GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN,
					GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			if (rc)
				pr_err("request lcd_io_en failed,rc=%d\n", rc);
		}
	}
	return 0;
}

static int mdss_dsi_parse_fbc_params(struct device_node *np,
				struct mdss_panel_info *panel_info)
{
	int rc, fbc_enabled = 0;
	u32 tmp;

	fbc_enabled = of_property_read_bool(np,	"qcom,mdss-dsi-fbc-enable");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		panel_info->fbc.enabled = 1;
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bpp", &tmp);
		panel_info->fbc.target_bpp =	(!rc ? tmp : panel_info->bpp);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-packing",
				&tmp);
		panel_info->fbc.comp_mode = (!rc ? tmp : 0);
		panel_info->fbc.qerr_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-quant-error");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bias", &tmp);
		panel_info->fbc.cd_bias = (!rc ? tmp : 0);
		panel_info->fbc.pat_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-pat-mode");
		panel_info->fbc.vlc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-vlc-mode");
		panel_info->fbc.bflc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-bflc-mode");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-h-line-budget",
				&tmp);
		panel_info->fbc.line_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-budget-ctrl",
				&tmp);
		panel_info->fbc.block_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-block-budget",
				&tmp);
		panel_info->fbc.block_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossless-threshold", &tmp);
		panel_info->fbc.lossless_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-threshold", &tmp);
		panel_info->fbc.lossy_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-rgb-threshold",
				&tmp);
		panel_info->fbc.lossy_rgb_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-mode-idx", &tmp);
		panel_info->fbc.lossy_mode_idx = (!rc ? tmp : 0);
	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		panel_info->fbc.enabled = 0;
		panel_info->fbc.target_bpp =
			panel_info->bpp;
	}
	return 0;
}

static int mdss_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32	tmp;
	int rc, i, len;
	const char *data;
	static const char *pdest;
	static const char *on_cmds_state, *off_cmds_state;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-width", &tmp);
	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	pinfo->xres = (!rc ? tmp : 640);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-height", &tmp);
	if (rc) {
		pr_err("%s:%d, panel height not specified\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->yres = (!rc ? tmp : 480);

	rc = of_property_read_u32(np,
			"qcom,mdss-pan-physical-width-dimension", &tmp);
	pinfo->physical_width = (!rc ? tmp : 0);
	rc = of_property_read_u32(np,
			"qcom,mdss-pan-physical-height-dimension", &tmp);
	pinfo->physical_height = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-left-border", &tmp);
	pinfo->lcdc.xres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-right-border", &tmp);
	if (!rc)
		pinfo->lcdc.xres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-top-border", &tmp);
	pinfo->lcdc.yres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-bottom-border", &tmp);
	if (!rc)
		pinfo->lcdc.yres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, bpp not specified\n", __func__, __LINE__);
		return -EINVAL;
	}
	pinfo->bpp = (!rc ? tmp : 24);
	pinfo->mipi.mode = DSI_VIDEO_MODE;
	data = of_get_property(np, "qcom,mdss-dsi-panel-type", NULL);
	if (data && !strncmp(data, "dsi_cmd_mode", 12))
		pinfo->mipi.mode = DSI_CMD_MODE;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-pixel-packing", &tmp);
	tmp = (!rc ? tmp : 0);
	rc = mdss_panel_dt_get_dst_fmt(pinfo->bpp,
			pinfo->mipi.mode, tmp,
			&(pinfo->mipi.dst_format));
	if (rc) {
		pr_debug("%s: problem determining dst format. Set Default\n",
				__func__);
		pinfo->mipi.dst_format =
			DSI_VIDEO_DST_FORMAT_RGB888;
	}

	pdest = of_get_property(np,
			"qcom,mdss-dsi-panel-destination", NULL);
	if (strlen(pdest) != 9) {
		pr_err("%s: Unknown pdest specified\n", __func__);
		return -EINVAL;
	}
	if (!strncmp(pdest, "display_1", 9))
		pinfo->pdest = DISPLAY_1;
	else if (!strncmp(pdest, "display_2", 9))
		pinfo->pdest = DISPLAY_2;
	else {
		pr_debug("%s: pdest not specified. Set Default\n",
				__func__);
		pinfo->pdest = DISPLAY_1;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-front-porch", &tmp);
	pinfo->lcdc.h_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-back-porch", &tmp);
	pinfo->lcdc.h_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-pulse-width", &tmp);
	pinfo->lcdc.h_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-sync-skew", &tmp);
	pinfo->lcdc.hsync_skew = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-back-porch", &tmp);
	pinfo->lcdc.v_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-front-porch", &tmp);
	pinfo->lcdc.v_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-pulse-width", &tmp);
	pinfo->lcdc.v_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-underflow-color", &tmp);
	pinfo->lcdc.underflow_clr = (!rc ? tmp : 0xff);

	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-border-color", &tmp);
	pinfo->lcdc.border_clr = (!rc ? tmp : 0);
	pinfo->bklt_ctrl = UNKNOWN_CTRL;
	data = of_get_property(np, "qcom,mdss-dsi-bl-pmic-control-type", NULL);
	if (data) {
		if (!strncmp(data, "bl_ctrl_wled", 12)) {
			led_trigger_register_simple("bkl-trigger",
					&bl_led_trigger);
			pr_debug("%s: SUCCESS-> WLED TRIGGER register\n",
					__func__);
			ctrl_pdata->bklt_ctrl = BL_WLED;
		} else if (!strncmp(data, "bl_ctrl_pwm", 11)) {
			ctrl_pdata->bklt_ctrl = BL_PWM;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-pwm-frequency", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, panel pwm_period\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_period = tmp;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-bank-select", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, dsi lpg channel\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_lpg_chan = tmp;
			tmp = of_get_named_gpio(np,
					"qcom,mdss-dsi-pwm-gpio", 0);
			ctrl_pdata->pwm_pmic_gpio = tmp;
		} else if (!strncmp(data, "bl_ctrl_dcs", 11)) {
			ctrl_pdata->bklt_ctrl = BL_DCS_CMD;
		}
	}
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-min-level", &tmp);
	pinfo->bl_min = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-max-level", &tmp);
	pinfo->bl_max = (!rc ? tmp : 100);
	ctrl_pdata->bklt_max = pinfo->bl_max;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-interleave-mode", &tmp);
	pinfo->mipi.interleave_mode = (!rc ? tmp : 0);

	pinfo->mipi.vsync_enable = of_property_read_bool(np,
			"qcom,mdss-dsi-te-check-enable");
	pinfo->mipi.hw_vsync_mode = of_property_read_bool(np,
			"qcom,mdss-dsi-te-using-te-pin");
	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-h-sync-pulse", &tmp);
	pinfo->mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	pinfo->mipi.hfp_power_stop = of_property_read_bool(np,
			"qcom,mdss-dsi-hfp-power-mode");
	pinfo->mipi.hsa_power_stop = of_property_read_bool(np,
			"qcom,mdss-dsi-hsa-power-mode");
	pinfo->mipi.hbp_power_stop = of_property_read_bool(np,
			"qcom,mdss-dsi-hbp-power-mode");
	pinfo->mipi.bllp_power_stop = of_property_read_bool(np,
			"qcom,mdss-dsi-bllp-power-mode");
	pinfo->mipi.eof_bllp_power_stop = of_property_read_bool(
			np, "qcom,mdss-dsi-bllp-eof-power-mode");
	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-traffic-mode", &tmp);
	pinfo->mipi.traffic_mode =
		(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);

	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-te-dcs-command", &tmp);
	pinfo->mipi.insert_dcs_cmd =
		(!rc ? tmp : 1);

	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-wr-mem-continue", &tmp);
	pinfo->mipi.wr_mem_continue =
		(!rc ? tmp : 0x3c);

	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-wr-mem-start", &tmp);
	pinfo->mipi.wr_mem_start =
		(!rc ? tmp : 0x2c);

	rc = of_property_read_u32(np,
			"qcom,mdss-dsi-te-pin-select", &tmp);
	pinfo->mipi.te_sel =
		(!rc ? tmp : 1);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-color-order", &tmp);
	pinfo->mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);

	rc = of_property_read_u32(np, "qcom,mdss-force-clk-lane-hs", &tmp);
	pinfo->mipi.force_clk_lane_hs = (!rc ? tmp : 0);

	pinfo->mipi.data_lane0 = of_property_read_bool(np,
			"qcom,mdss-dsi-lane-0-state");
	pinfo->mipi.data_lane1 = of_property_read_bool(np,
			"qcom,mdss-dsi-lane-1-state");
	pinfo->mipi.data_lane2 = of_property_read_bool(np,
			"qcom,mdss-dsi-lane-2-state");
	pinfo->mipi.data_lane3 = of_property_read_bool(np,
			"qcom,mdss-dsi-lane-3-state");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-lane-map", &tmp);
	pinfo->mipi.dlane_swap = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-pre", &tmp);
	pinfo->mipi.t_clk_pre = (!rc ? tmp : 0x24);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-post", &tmp);
	pinfo->mipi.t_clk_post = (!rc ? tmp : 0x03);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);
	pinfo->mipi.rx_eot_ignore = of_property_read_bool(np,
			"qcom,mdss-dsi-rx-eot-ignore");
	pinfo->mipi.tx_eot_append = of_property_read_bool(np,
			"qcom,mdss-dsi-tx-eot-append");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-mdp-trigger", &tmp);
	pinfo->mipi.mdp_trigger =
		(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
				__func__, __LINE__);
		pinfo->mipi.mdp_trigger =
			DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-dma-trigger", &tmp);
	pinfo->mipi.dma_trigger =
		(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
				__func__, __LINE__);
		pinfo->mipi.dma_trigger =
			DSI_CMD_TRIGGER_SW;
	}
	data = of_get_property(np, "qcom,mdss-dsi-panel-mode-gpio-state", &tmp);
	if (data) {
		if (!strcmp(data, "high"))
			pinfo->mode_gpio_state = MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			pinfo->mode_gpio_state = MODE_GPIO_LOW;
	} else {
		pinfo->mode_gpio_state = MODE_GPIO_NOT_VALID;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-frame-rate", &tmp);
	pinfo->mipi.frame_rate = (!rc ? tmp : 60);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-clock-rate", &tmp);
	pinfo->clk_rate = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,mdss-dsi-panel-timings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
				__func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		pinfo->mipi.dsi_phy_db.timing[i] = data[i];

	pinfo->mipi.lp11_init = of_property_read_bool(np,
			"qcom,mdss-dsi-lp11-init");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-init-delay-us", &tmp);
	pinfo->mipi.init_delay = (!rc ? tmp : 0);

	mdss_dsi_parse_fbc_params(np, pinfo);
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
			"qcom,mdss-dsi-on-command",
			"qcom,mdss-dsi-on-command-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
			"qcom,mdss-dsi-off-command",
			"qcom,mdss-dsi-off-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.bklt_cmds,
			"qcom,mdss-dsi-bklt-command",
			"qcom,mdss-dsi-bklt-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.bklt_ctrl_cmd,
			"qcom,mdss-dsi-bklt-ctrl-command",
			"qcom,mdss-dsi-bklt-ctrl-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.acl_off_cmd,
			"qcom,mdss-dsi-acl-off-command",
			"qcom,mdss-dsi-acl-off-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.acl_on_cmd,
			"qcom,mdss-dsi-acl-on-command",
			"qcom,mdss-dsi-acl-on-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.acl_sel_lock_cmd,
			"qcom,mdss-dsi-acl-sel-lock-command",
			"qcom,mdss-dsi-acl-sel-lock-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.acl_global_param_cmd,
			"qcom,mdss-dsi-acl-global-param-command",
			"qcom,mdss-dsi-acl-global-param-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.acl_lut_enable_cmd,
			"qcom,mdss-dsi-acl-lut-enable-command",
			"qcom,mdss-dsi-acl-lut-enable-command-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.acl_table,
			"qcom,mdss-dsi-acl-table-command",
			"qcom,mdss-dsi-acl-table-command-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.pos_hbm_cmds,
			"qcom,mdss-dsi-pos-hbm-commands",
			"qcom,mdss-dsi-pos-hbm-commands-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.hbm_white_cmd,
			"qcom,mdss-dsi-hbm-white-command",
			"qcom,mdss-dsi-hbm-white-command-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.mode_cmds,
			"qcom,mdss-dsi-mode-commands",
			"qcom,mdss-dsi-mode-commands-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.partial_area_set_cmd,
			"qcom,mdss-dsi-partial-area-set-cmd",
			"qcom,mdss-dsi-partial-area-set-cmd-state");
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
	mdss_dsi_parse_dcs_cmds(np, &msd.test_key_on_cmd,
			"qcom,mdss-dsi-test-key-on-command",
			"qcom,mdss-dsi-test-key-on-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.test_key_off_cmd,
			"qcom,mdss-dsi-test-key-off-command",
			"qcom,mdss-dsi-test-key-off-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.ldi_id_cmd,
			"qcom,mdss-dsi-ldi-id-command",
			"qcom,mdss-dsi-ldi-id-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.ldi_mtp_cmd,
			"qcom,mdss-dsi-ldi-mtp-command",
			"qcom,mdss-dsi-ldi-mtp-command-state");
#endif
	on_cmds_state = of_get_property(np,
			"qcom,mdss-dsi-on-command-state", NULL);
	if (!strncmp(on_cmds_state, "dsi_lp_mode", 11)) {
		ctrl_pdata->dsi_on_state = DSI_LP_MODE;
	} else if (!strncmp(on_cmds_state, "dsi_hs_mode", 11)) {
		ctrl_pdata->dsi_on_state = DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
				__func__);
		ctrl_pdata->dsi_on_state = DSI_LP_MODE;
	}

	off_cmds_state = of_get_property(np,
			"qcom,mdss-dsi-off-command-state", NULL);
	if (!strncmp(off_cmds_state, "dsi_lp_mode", 11)) {
		ctrl_pdata->dsi_off_state = DSI_LP_MODE;
	} else if (!strncmp(off_cmds_state, "dsi_hs_mode", 11)) {
		ctrl_pdata->dsi_off_state = DSI_HS_MODE;
	} else {
		pr_debug("%s: ON cmds state not specified. Set Default\n",
				__func__);
		ctrl_pdata->dsi_off_state = DSI_LP_MODE;
	}

	return 0;
error:
	return -EINVAL;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static ssize_t mdss_siop_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf(buf, 2, "%d\n", msd.dstat.siop_status);
	pr_info("%s :[MDSS_SDC] CABC: %d\n", __func__, msd.dstat.siop_status);
	return rc;
}
static ssize_t mdss_siop_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	if (sysfs_streq(buf, "1") && !msd.dstat.siop_status)
		msd.dstat.siop_status = true;
	else if (sysfs_streq(buf, "0") && msd.dstat.siop_status)
		msd.dstat.siop_status = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	return size;

}

static int mdss_dsi_panel_get_power(struct lcd_device *ld)
{
	struct mdss_samsung_driver_data *msdd = lcd_get_data(ld);
	return msdd->power;
}

static int mdss_dsi_panel_set_power(struct lcd_device *ld, int power)
{
	struct mdss_samsung_driver_data *msdd = lcd_get_data(ld);
	pr_debug("power[%d]\n", power);
	if (power != FB_BLANK_UNBLANK &&
	    power != FB_BLANK_NORMAL &&
	    power != FB_BLANK_POWERDOWN) {
		dev_err(&ld->dev, "power value should be 0, 1 or 4\n");
		return -EINVAL;
	}

	if (msdd->power == power) {
		dev_err(&ld->dev, "power mode[%d]"	\
			"it is same as previous one\n", power);
		return -EINVAL;
	}

	switch (power) {
	case FB_BLANK_UNBLANK:
	case FB_BLANK_NORMAL:
		/* TODO : unblank control */
		break;
	default:
		break;
	}

	msdd->power = power;
	dev_info(&ld->dev, "%s:power[%d]\n", __func__, power);
	return 0;
}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static int panel_get_brightness_index(unsigned int brightness)
{

	switch (brightness) {
	case 0 ... 20:
		brightness = 0;
		break;
	case 21 ... 40:
		brightness = 2;
		break;
	case 41 ... 60:
		brightness = 4;
		break;
	case 61 ... 80:
		brightness = 6;
		break;
	default:
		brightness = 10;
		break;
	}

	return brightness;
}

static int dsi_panel_update_brightness(struct backlight_device *bd,
							int brightness)
{
	struct mdss_samsung_driver_data *msdd = bl_get_data(bd);
	struct mdss_panel_data *pdata = msdd->mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dsi_cmd_desc bklt_cmd, bklt_ctrl_cmd;
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
	struct dsi_cmd_desc gamma_cmd;
	struct dsi_ctrl_hdr dchdr = {DTYPE_NULL_PKT, 1, 0, 0, 1, 0};
	struct dsi_cmd_desc test_key_on_cmd, test_key_off_cmd;
	test_key_on_cmd = msd.test_key_on_cmd.cmds[0];
	test_key_off_cmd = msd.test_key_off_cmd.cmds[0];
#endif

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	bklt_ctrl_cmd = msd.bklt_ctrl_cmd.cmds[0];

	mdss_dsi_runtime_active(ctrl_pdata, true, __func__);

	switch (candela_tbl[brightness]) {
	case CANDELA_10:
		bklt_cmd = msd.bklt_cmds.cmds[0];
		mdss_dsi_cmds_send(ctrl_pdata, &bklt_cmd, 1, 0);
		mdss_dsi_cmds_send(ctrl_pdata, &bklt_ctrl_cmd, 1, 0);
		msdd->compensation_on = true;
		break;
	case CANDELA_30:
		bklt_cmd = msd.bklt_cmds.cmds[1];
		mdss_dsi_cmds_send(ctrl_pdata, &bklt_cmd, 1, 0);
		mdss_dsi_cmds_send(ctrl_pdata, &bklt_ctrl_cmd, 1, 0);
		msdd->compensation_on = true;
		break;
	default:
		if (msdd->compensation_on) {
			bklt_cmd = msd.bklt_cmds.cmds[2];
			mdss_dsi_cmds_send(ctrl_pdata, &bklt_cmd, 1, 0);
			mdss_dsi_cmds_send(ctrl_pdata, &bklt_ctrl_cmd, 1, 0);
			msdd->compensation_on = false;
		}
		break;
	}

#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
	mdss_dsi_cmds_send(ctrl_pdata, &test_key_on_cmd, 1, 0);

	dchdr.dtype = DTYPE_DCS_LWRITE;
	dchdr.dlen = GAMMA_CMD_CNT;
	gamma_cmd.dchdr = dchdr;
	gamma_cmd.payload = (char *)msdd->gamma_tbl[brightness];
	mdss_dsi_cmds_send(ctrl_pdata, &gamma_cmd, 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &test_key_off_cmd, 1, 0);
#endif

	mdss_dsi_runtime_active(ctrl_pdata, false, __func__);

	return 0;
}

static int mdss_dsi_panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int mdss_dsi_panel_set_brightness(struct backlight_device *bd)
{
	int brightness = bd->props.brightness;
	struct mdss_samsung_driver_data *msdd = bl_get_data(bd);
	struct mdss_panel_data *pdata = msdd->mpd;
	int ret = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (brightness < pdata->panel_info.bl_min ||
		brightness > bd->props.max_brightness) {
		pr_err("brightness should be %d to %d\n",
			pdata->panel_info.bl_min, bd->props.max_brightness);
		return -EINVAL;
	}

	mutex_lock(&msd.mlock);

	if (msd.power == FB_BLANK_POWERDOWN) {
		pr_info("brightness should be call after power on\n");
		goto err;
	}

	if (msd.alpm_on) {
		ret = 0;
		goto err;
	}

	pr_info("%s: brightness[%d]hbm[%d].\n",
			__func__, brightness, msd.hbm_on);

	brightness = panel_get_brightness_index(brightness);

	if (msd.hbm_on)
		panel_enter_hbm_mode();
	else
		dsi_panel_update_brightness(bd, brightness);

err:
	mutex_unlock(&msd.mlock);

	return ret;
}

static const struct backlight_ops bd_ops = {
	.get_brightness = mdss_dsi_panel_get_brightness,
	.update_status = mdss_dsi_panel_set_brightness,
};
#endif

static struct lcd_ops mdss_dsi_disp_props = {

	.get_power = mdss_dsi_panel_get_power,
	.set_power = mdss_dsi_panel_set_power,

};

static ssize_t mdss_dsi_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf(buf, sizeof(int), "%d\n",
					msd.dstat.auto_brightness);
	pr_info("%s : auto_brightness : %d\n", __func__,
				msd.dstat.auto_brightness);

	return rc;
}

static ssize_t mdss_dsi_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	static unsigned char prev_auto_brightness;
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	if (sysfs_streq(buf, "0"))
		msd.dstat.auto_brightness = 0;
	else if (sysfs_streq(buf, "1"))
		msd.dstat.auto_brightness = 1;
	else if (sysfs_streq(buf, "2"))
		msd.dstat.auto_brightness = 2;
	else if (sysfs_streq(buf, "3"))
		msd.dstat.auto_brightness = 3;
	else if (sysfs_streq(buf, "4"))
		msd.dstat.auto_brightness = 4;
	else if (sysfs_streq(buf, "5"))
		msd.dstat.auto_brightness = 5;
	else if (sysfs_streq(buf, "6"))
		msd.dstat.auto_brightness = 6;
	else
		pr_info("%s: Invalid argument!!", __func__);

	if (prev_auto_brightness == msd.dstat.auto_brightness)
		return size;

	usleep_range(1000, 1000);

	if ((msd.dstat.auto_brightness >= 5) ||
			(msd.dstat.auto_brightness == 0))
		msd.dstat.siop_status = false;
	else
		msd.dstat.siop_status = true;

	if (msd.mfd->panel_power_on == false) {
		pr_err("%s: panel power off no bl ctrl\n", __func__);
		return size;
	}

	mdss_dsi_panel_cabc_dcs(ctrl_pdata, msd.dstat.siop_status);
	prev_auto_brightness = msd.dstat.auto_brightness;
	pr_info("%s %d %d\n", __func__, msd.dstat.auto_brightness,
					msd.dstat.siop_status);
	return size;
}

static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_dsi_auto_brightness_show,
			mdss_dsi_auto_brightness_store);

static ssize_t mdss_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[20];

	snprintf(temp, 20, "SDC_%02x%02x%02x\n",
			msd.ldi_id[3], msd.ldi_id[2], msd.ldi_id[5]);
	strlcat(buf, temp, 20);
	return strnlen(buf, 20);
}

static void panel_acl_update(unsigned int value)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	if (value == MIN_ACL) {
		mdss_dsi_cmds_send(ctrl_pdata, &msd.acl_off_cmd.cmds[0], 1, 0);
		return;
	}
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.acl_sel_lock_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.acl_table.cmds[value - 1], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.acl_global_param_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.acl_lut_enable_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.acl_on_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[0], 1, 0);
}

static ssize_t panel_acl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(int), "%d\n", msd.acl);
}

static ssize_t panel_acl_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	unsigned long value;
	int rc;

	if (msd.power > FB_BLANK_UNBLANK) {
		pr_err("acl control before lcd enable.\n");
		return -EPERM;
	}

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (value < MIN_ACL || value > MAX_ACL) {
		pr_err("invalid acl value[%ld]\n", value);
		return size;
	}

	panel_acl_update(value);

	msd.acl = value;
	dev_info(dev, "acl control[%d]\n", msd.acl);

	return size;
}

static void panel_enter_hbm_mode(void)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	unsigned char buf[3];
	struct dsi_ctrl_hdr dchdr = {DTYPE_DCS_LWRITE, 1, 0, 0, 1, 0};
	struct dsi_cmd_desc elvss_hbm_cmd;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);
	buf[0] = LDI_ID_REG;
	buf[1] = msd.elvss_hbm;
	buf[2] = 0;
	dchdr.dlen = sizeof(buf);
	elvss_hbm_cmd.dchdr = dchdr;
	elvss_hbm_cmd.payload = buf;

	mdss_dsi_runtime_active(ctrl_pdata, true, __func__);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.pos_hbm_cmds.cmds[1], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &elvss_hbm_cmd, 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[0], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.hbm_white_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.bklt_ctrl_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.pos_hbm_cmds.cmds[2], 1, 0);

	mdss_dsi_runtime_active(ctrl_pdata, false, __func__);
}

static void panel_exit_hbm_mode(void)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	unsigned char buf[3];
	struct dsi_ctrl_hdr dchdr = {DTYPE_DCS_LWRITE, 1, 0, 0, 1, 0};
	struct dsi_cmd_desc default_hbm_cmd;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	buf[0] = LDI_ID_REG;
	buf[1] = msd.default_hbm;
	buf[2] = 0;
	dchdr.dlen = sizeof(buf);
	default_hbm_cmd.dchdr = dchdr;
	default_hbm_cmd.payload = buf;

	mdss_dsi_runtime_active(ctrl_pdata, true, __func__);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.bklt_cmds.cmds[2], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.bklt_ctrl_cmd.cmds[0], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.pos_hbm_cmds.cmds[1], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &default_hbm_cmd, 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[0], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.pos_hbm_cmds.cmds[2], 1, 0);

	mdss_dsi_runtime_active(ctrl_pdata, false, __func__);
}

static ssize_t panel_hbm_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 5, "%s\n", msd.hbm_on ? "on" : "off");
}

static ssize_t panel_hbm_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t size)
{
	if (!strncmp(buf, "on", 2))
		msd.hbm_on = 1;
	else if (!strncmp(buf, "off", 3))
		msd.hbm_on = 0;
	else {
		pr_err("invalid comman (use on or off)d.\n");
		return size;
	}

	if (msd.power > FB_BLANK_UNBLANK) {
		pr_err("hbm control before lcd enable.\n");
		return -EPERM;
	}

	if (msd.hbm_on) {
		pr_info("HBM ON.\n");
		panel_enter_hbm_mode();
	} else {
		pr_info("HBM OFF.\n");
		panel_exit_hbm_mode();
	}

	return size;
}

static int panel_alpm_handler(int event)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	int ret = 0;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	pr_debug("%s: event=%d\n", __func__, event);

	switch (event) {
		case ALPM_STATE_SYNC:
			msd.alpm_mode = msd.alpm_on;
			break;
		case ALPM_MODE_STATE:
			ret = msd.alpm_mode;
			break;
		case ALPM_NODE_STATE:
			ret = msd.alpm_on;
			break;
		default:
			break;
	}

	return ret;
}

static void panel_alpm_on(unsigned int enable)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	if (enable) {
		mdss_dsi_cmds_send(ctrl_pdata,
				&msd.partial_area_set_cmd.cmds[0], 1, 0);
		mdss_dsi_cmds_send(ctrl_pdata, &msd.mode_cmds.cmds[0],
					1, 0); /* Partial Mode On */
		mdss_dsi_cmds_send(ctrl_pdata, &msd.mode_cmds.cmds[3],
					1, 0); /* Idle Mode On */
		pr_info("Entered into ALPM mode\n");
	} else {
		struct backlight_device *bd = msd.bd;
		unsigned int brightness = bd->props.brightness;
		mdss_dsi_cmds_send(ctrl_pdata, &msd.mode_cmds.cmds[2],
					1, 0); /* Idle Mode Off */
		mdss_dsi_cmds_send(ctrl_pdata, &msd.mode_cmds.cmds[1],
					1, 0); /* Normal Mode On */

		brightness = panel_get_brightness_index(brightness);
		dsi_panel_update_brightness(bd, brightness);
		pr_info("Exited from ALPM mode\n");
	}
	panel_alpm_handler(ALPM_STATE_SYNC);
}

static ssize_t panel_alpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 5, "%s\n", msd.alpm_on ? "on" : "off");
}

static ssize_t panel_alpm_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t size)
{
	if (!strncmp(buf, "on", 2)) {
		if (msd.alpm_on)
			return size;
		msd.alpm_on = true;
	} else if (!strncmp(buf, "off", 3)) {
		if (!msd.alpm_on)
			return size;
		msd.alpm_on = false;
	} else {
		pr_warn("invalid command.\n");
		return size;
	}

	if (msd.power == FB_BLANK_POWERDOWN)
		return size;

	return size;
}

unsigned int mdss_dsi_show_cabc(void)
{
	return msd.dstat.siop_status;
}

void mdss_dsi_store_cabc(unsigned int cabc)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	if (msd.mfd->panel_power_on == false) {
		pr_err("%s: panel power off no bl ctrl\n", __func__);
		return;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);
	if (msd.dstat.auto_brightness)
		return;

	msd.dstat.siop_status = cabc;
	mdss_dsi_panel_cabc_dcs(ctrl_pdata, msd.dstat.siop_status);
	pr_info("%s :[MDSS_SDC] CABC: %d\n", __func__, msd.dstat.siop_status);

}

#endif
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
static irqreturn_t mdss_dsi_panel_err_fg_irq(int irq, void *handle)
{
	pr_debug("%s : esd irq handler.\n", __func__);
	if (!work_busy(&err_fg_work)) {
		if (!gpio_get_value(msd.esd_det)) {
			schedule_work(&err_fg_work);
			err_fg_working = 1;
			disable_irq_nosync(msd.err_fg_irq);
			pr_info("%s : add esd schedule_work for esd\n",
					__func__);
		}
	}

	return IRQ_HANDLED;
}

static void mdss_dsi_panel_err_fg_work(struct work_struct *work)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	char *event_string = "LCD_ESD=ON";
	char *envp[] = { event_string, NULL };

	if (msd.mfd->panel_power_on == false) {
		pr_err("%s: Display off Skip ESD recovery\n", __func__);
		return;
	}
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	msd.esd_status  = ESD_DETECTED;
	kobject_uevent_env(&esd_dev->kobj,
			KOBJ_CHANGE, envp);
	pr_err("Send uevent. ESD DETECTED\n");

	return;
}

static void mdss_dsi_panel_err_fg_enable(struct work_struct *work)
{
	pr_debug("%s: enable esd irq.\n", __func__);
	enable_irq(msd.err_fg_irq);
	err_fg_working = 0;
	esd_count++;
}

#ifdef ESD_DEBUG
static ssize_t mipi_samsung_esd_check_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf((char *)buf, 20, "esd count:%d\n", esd_count);

	return rc;
}
static ssize_t mipi_samsung_esd_check_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(msd.msm_pdev);

	mdss_dsi_panel_err_fg_irq(0, mfd);
	return 1;
}
#endif
#endif
DEVICE_ATTR(lcd_type, S_IRUGO, mdss_disp_lcdtype_show, NULL);
static struct device_attribute lcd_dev_attrs[] = {
	__ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		mdss_siop_enable_show, mdss_siop_enable_store),
	__ATTR(acl, S_IRUGO | S_IWUSR, panel_acl_show, panel_acl_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, panel_hbm_show, panel_hbm_store),
	__ATTR(alpm, S_IRUGO | S_IWUSR, panel_alpm_show, panel_alpm_store),
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
#ifdef ESD_DEBUG
	__ATTR(esd_check, S_IRUGO , mipi_samsung_esd_check_show,\
			 mipi_samsung_esd_check_store),
#endif
#endif
};

int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	bool cont_splash_enabled;
	bool partial_update_enabled;
	struct mdss_panel_info *pinfo;
#if (defined CONFIG_SMART_DIMMING_S6E63J0X03 || defined CONFIG_LCD_CLASS_DEVICE)
	int i;
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
#endif

	msd.power = FB_BLANK_POWERDOWN;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	np = of_parse_phandle(node,
			"qcom,mdss-dsi-panel-controller", 0);
	if (!np) {
		pr_err("%s: Dsi controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	pdev = of_find_device_by_node(np);

#endif
	if (!node) {
		pr_err("%s: no panel node\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s:%d\n", __func__, __LINE__);
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name)
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	pinfo = &(ctrl_pdata->panel_data.panel_info);
	rc = mdss_panel_parse_dt(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt parse failed\n", __func__, __LINE__);
		return rc;
	}
	ctrl_pdata->panel_mode = pinfo->mipi.mode;
	rc = mdss_panel_parse_dt_gpio(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt gpio parse failed\n",
					__func__, __LINE__);
		return rc;
	}

	/*if (cmd_cfg_cont_splash)*/
		cont_splash_enabled = of_property_read_bool(node,
				"qcom,cont-splash-enabled");
	/*else
		cont_splash_enabled = false;*/
	if (!cont_splash_enabled) {
		pr_info("%s:%d Continuous splash flag not found.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;
	} else {
		pr_info("%s:%d Continuous splash flag enabled.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 1;
	}
	partial_update_enabled = of_property_read_bool(node,
						"qcom,partial-update-enabled");
	if (partial_update_enabled) {
		pr_info("%s:%d Partial update enabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 1;
		ctrl_pdata->partial_update_fnc = mdss_dsi_panel_partial_update;
	} else {
		pr_info("%s:%d Partial update disabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 0;
		ctrl_pdata->partial_update_fnc = NULL;
	}

	ctrl_pdata->panel_data.panel_info.ulps_feature_enabled =
		of_property_read_bool(node, "qcom,ulps-enabled");
	pr_info("%s: ulps feature %s", __func__,
		(ctrl_pdata->panel_data.panel_info.ulps_feature_enabled ?
		"enabled" : "disabled"));

	msd.boot_power_on = of_property_read_bool(node,
				"qcom,was-enable");

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->panel_reset = mdss_dsi_panel_reset;
	pinfo->alpm_event = panel_alpm_handler;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("s6e63j0x03", &pdev->dev, &msd,
					&mdss_dsi_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		printk(KERN_ERR "lcd : failed to register device\n");
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(lcd_dev_attrs); i++) {
		rc = device_create_file(&pdev->dev,
				&lcd_dev_attrs[i]);
		if (rc < 0) {
			pr_err("failed to add ld dev sysfs entries\n");
			break;
		}
	}

	rc = device_create_file(&lcd_device->dev, &dev_attr_lcd_type);
	if (rc < 0) {
		pr_err("failed to add ld sysfs entries\n");
		return rc;
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("s6e63j0x03-bl", &lcd_device->dev,
			&msd, &bd_ops, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		goto err_backlight_register;
	}
	rc = sysfs_create_file(&bd->dev.kobj,
					&dev_attr_auto_brightness.attr);
	if (rc) {
		pr_info("sysfs create fail-%s\n",
				dev_attr_auto_brightness.attr.name);
	}

	bd->props.max_brightness = ctrl_pdata->bklt_max;
	bd->props.brightness = ctrl_pdata->bklt_max;
	msd.bd = bd;
#endif
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	msd.msm_pdev = pdev;
	msd.esd_status = ESD_NONE;
	if (!get_lcd_attached()) {
		pr_info("%s: lcd_not_attached\n", __func__);
		goto err_lcd_attached;
	}

	INIT_WORK(&err_fg_work, mdss_dsi_panel_err_fg_work);
	INIT_DELAYED_WORK(&err_fg_enable, mdss_dsi_panel_err_fg_enable);

	if (system_rev > 0x05)
		msd.esd_det = of_get_named_gpio(node, "qcom,oled-det-gpio", 0);
	else
		msd.esd_det = of_get_named_gpio(node, "qcom,esd-det-gpio", 0);
	msd.err_fg_irq = gpio_to_irq(msd.esd_det);
	rc = gpio_request(msd.esd_det, "err_fg");
	if (rc) {
		pr_err("request gpio GPIO_ESD failed, ret=%d\n", rc);
		goto err_gpio_fail;
	}
	gpio_tlmm_config(GPIO_CFG(msd.esd_det,  0, GPIO_CFG_INPUT,
					GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	rc = gpio_direction_input(msd.esd_det);
	if (unlikely(rc < 0)) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, msd.esd_det, rc);
	}

	rc = request_threaded_irq(msd.err_fg_irq, NULL,
				mdss_dsi_panel_err_fg_irq,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"esd_detect", NULL);
	if (rc)
		pr_err("%s : Failed to request_irq. :ret=%d", __func__, rc);

	esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(esd_class)) {
		dev_err(&msd.msm_pdev->dev, "Failed to create class(lcd_event)!\n");
		rc = PTR_ERR(esd_class);
		goto err_gpio_fail;
	}
	esd_dev = device_create(esd_class, NULL, 0, NULL, "esd");

	disable_irq(msd.err_fg_irq);
#endif
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
	msd.dimming = devm_kzalloc(&pdev->dev, sizeof(*(msd.dimming)),
								GFP_KERNEL);
	if (!msd.dimming) {
		dev_err(&pdev->dev, "failed to allocate dimming.\n");
		rc = -ENOMEM;
		goto err_dimming_alloc;
	}
	for (i = 0; i < MAX_GAMMA_CNT; i++) {
		msd.gamma_tbl[i] = (unsigned char *)
		devm_kzalloc(&pdev->dev, sizeof(unsigned char) * GAMMA_CMD_CNT,
						GFP_KERNEL);

		if (!msd.gamma_tbl[i]) {
			dev_err(&pdev->dev, "failed to allocate gamma_tbl\n");
			rc = -ENOMEM;
			goto err_gamma_alloc;
		}
	}
#endif

	if (ctrl_pdata->panel_data.panel_info.cont_splash_enabled)
		msd.power = FB_BLANK_UNBLANK;
	else
		msd.power = FB_BLANK_POWERDOWN;

	mutex_init(&msd.mlock);

	return 0;
#ifdef CONFIG_SMART_DIMMING_S6E63J0X03
err_gamma_alloc:
	devm_kfree(&pdev->dev, msd.dimming);
err_dimming_alloc:
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	gpio_free(msd.esd_det);
err_lcd_attached:
err_gpio_fail:
#endif

#if defined(CONFIG_LCD_CLASS_DEVICE)
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	backlight_device_unregister(bd);
err_backlight_register:
#endif
	lcd_device_unregister(lcd_device);
#endif
	return rc;
}
