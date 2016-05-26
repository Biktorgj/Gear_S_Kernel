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
#include "mdss_dsi_panel_oled.h"
#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
#include <linux/smart_dimming_s6e36w0x01.h>
#endif

#define DT_CMD_HDR 6

/* This comes from kernel/init, used
* for brightness settings while in
* twrp */
extern int boot_mode_recovery;



#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
struct delayed_work panel_check;
static struct class *esd_class;
static struct device *esd_dev;

enum esd_status {
	ESD_NONE,
	ESD_DETECTED,
	};
#endif

enum {
	TEMP_RANGE_0 = 0,		/* 0 < temperature*/
	TEMP_RANGE_1,		/*-10 < temperature < =0*/
	TEMP_RANGE_2,		/*-20<temperature <= -10*/
	TEMP_RANGE_3,		/*temperature <= -20*/
};

#define MDNIE_TUNE	0

DEFINE_LED_TRIGGER(bl_led_trigger);
static struct mdss_samsung_driver_data msd;
static void panel_enter_hbm_mode(void);

/* Global ALPM states */
#define DISPLAY_ALPM_MODES

#ifdef DISPLAY_ALPM_MODES

u8 panel_previous_alpm_mode;
u8 panel_actual_alpm_mode;
static void panel_alpm_on(unsigned int enable);
u8 panel_alpm_handler(u8 flag);
static void alpm_store(	struct mdss_panel_info *pinfo, u8 mode);
#endif

/* Dummy function */
void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	return;
}

/* function wrapper to send dsi commands */
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

void mdss_dsi_panel_io_enable(int enable)
{
	if (!gpio_is_valid(msd.lcd_io_en))
		return;

	if (enable) {
		printk ("oled: %s: Enable GPIO", __func__);
		gpio_request(msd.lcd_io_en, "lcd_io_enable");
		gpio_set_value(msd.lcd_io_en, enable);
	} else {
		printk ("oled: %s: Disable GPIO", __func__);
		gpio_set_value(msd.lcd_io_en, enable);
		gpio_free(msd.lcd_io_en);
	}
}

/* Panel Reset. If display is in ALPM mode it shall not reset */
void mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int rc = 0;


	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	pinfo = &(ctrl_pdata->panel_data.panel_info);
	printk ("oled: Panel Reset called\n");
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

#ifdef DISPLAY_ALPM_MODES

	if (pinfo->alpm_event) {
		printk("oled: Panel Reset: Alpm Event is true\n");
		if (enable && pinfo->alpm_event(CHECK_PREVIOUS_STATUS))
			return;
		else if (!enable && pinfo->alpm_event(CHECK_CURRENT_STATUS)) {
			pinfo->alpm_event(STORE_CURRENT_STATUS);
			return;
		}
	printk("oled: [ALPM_DEBUG] %s: Panel reset, enable : %d\n", __func__, enable);
#endif

	}
	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_info("%s:%d, disp_en line not configured\n",
				__func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		printk("%s:%d, reset line not configured\n",
				__func__, __LINE__);
		return;
	}
	printk ("oled: Panel Reset: Enable is set to %d\n", enable);


	if (enable) {
		printk("oled: panel reset: enabling display\n");
		msleep(20);
		if (gpio_is_valid(msd.mipi_sel_gpio)){
			printk("oled: panel reset - enable  mipi_sel_gpio\n");
			gpio_set_value(msd.mipi_sel_gpio, 1);
		}
		usleep_range(1000, 1000);
		if (gpio_is_valid(ctrl_pdata->rst_gpio)) {
			printk("oled: panel reset - enable rst_gpio \n");
			gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			msleep(20);
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			usleep_range(1000, 1000);
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			msleep(20);
		}
		if (gpio_is_valid(msd.en_on_gpio)){
			printk("oled: panel reset - enable en_on_gpio\n");
			gpio_set_value(msd.en_on_gpio, 1);
		}
		if (gpio_is_valid(msd.disp_sel_en)) {
			printk("oled: panel reset - enable disp_sel_gpio\n");
			rc = gpio_tlmm_config(GPIO_CFG(msd.disp_sel_en, 0,
				GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),
				GPIO_CFG_ENABLE);
			if (rc)
				pr_err("request disp_sel_en failed, rc=%d\n",
									rc);
		}

		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			printk("oled: panel reset - enable - mode_gpio \n");
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}

		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			printk("%s: Panel Not properly turned OFF\n",
					__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			printk("%s: Reset panel done\n", __func__);
		}
	} else {
		printk("oled: Panel reset: panel disable\n");
		if (gpio_is_valid(ctrl_pdata->rst_gpio)) {
			printk("oled: panel reset - disable - rst_gpio \n");
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			gpio_free(ctrl_pdata->rst_gpio);
		}
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
	}
	printk ("oled: Panel reset finished (Enable=%d\n )", enable);
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

	printk("oled: Partial Update: %s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

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

	printk("oled: %s: enabling partial update\n", __func__);
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = partial_update_enable_cmd;
	cmdreq.cmds_cnt = 2;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	pr_info("oled: %s: caset[%x][%x][%x][%x]paset[%x][%x][%x][%x]\n", __func__,
			caset[1], caset[2], caset[3], caset[4],
			paset[1], paset[2], paset[3], paset[4]);
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	return rc;
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

#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
static void panel_get_gamma_table(struct mdss_samsung_driver_data *msdd,
						const unsigned char *data)
{
	static const unsigned char gamma_300cd[] = {
		 0xCA, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		 0x80, 0x00, 0x00, 0x00
	};
	int i, j;

	panel_read_gamma(msdd->dimming, data);
	panel_generate_volt_tbl(msdd->dimming);

	for (i = 0; i < MAX_GAMMA_CNT - 1; i++) {
		msd.gamma_tbl[i][0] = LDI_MTP_SET;
		panel_get_gamma(msdd->dimming, i,
					&msdd->gamma_tbl[i][1]);
		printk("[G_tbl][%d]", candela_tbl[i]);
		for (j = 1; j < GAMMA_CNT + 1; j++)
			printk("%d, " , msd.gamma_tbl[i][j]);
		printk("\n");
	}

	memcpy(msd.gamma_tbl[MAX_GAMMA_CNT - 1],
				gamma_300cd, sizeof(gamma_300cd));
}
#endif

static int mdss_dsi_panel_read_mtp(struct mdss_panel_data *pdata)
{
	int length;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	unsigned char mtp_data[MAX_MTP_CNT];
	unsigned char buf[21];
	int i;

	struct dsi_cmd_desc ldi_id_cmd, ldi_mtp_cmd;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	ldi_id_cmd = msd.ldi_id_cmd.cmds[0];
	ldi_mtp_cmd = msd.ldi_mtp_cmd.cmds[0];

	/* ID */
	mdss_dsi_cmds_send(ctrl, &msd.test_key_on_cmd.cmds[1], 1, 0);

	length = mdss_dsi_cmd_receive(ctrl, &ldi_id_cmd, LDI_ID_LEN);
	memcpy(msd.ldi_id, ctrl->rx_buf.data, LDI_ID_LEN);
	pr_info("%s: panel id is 0x%x. 0x%x. 0x%x.\n", __func__,
				msd.ldi_id[0], msd.ldi_id[1], msd.ldi_id[2]);

	/* HBM */
	mdss_dsi_cmds_send(ctrl, &msd.pos_hbm_cmds.cmds[0], 1, 0);
	length = mdss_dsi_cmd_receive(ctrl, &msd.read_hbm_mtp_cmds.cmds[0],
				LDI_HBM_MTP_LEN1);
	memcpy(buf, ctrl->rx_buf.data, LDI_HBM_MTP_LEN1);

	mdss_dsi_cmds_send(ctrl, &msd.pos_hbm_cmds.cmds[1], 1, 0);
	length = mdss_dsi_cmd_receive(ctrl, &msd.read_hbm_mtp_cmds.cmds[1],
				LDI_HBM_MTP_LEN2);
	memcpy(&buf[LDI_HBM_MTP_LEN1], ctrl->rx_buf.data, LDI_HBM_MTP_LEN2);

	mdss_dsi_cmds_send(ctrl, &msd.pos_hbm_cmds.cmds[2], 1, 0);
	length = mdss_dsi_cmd_receive(ctrl, &msd.read_hbm_mtp_cmds.cmds[2],
				LDI_HBM_MTP_LEN3);
	memcpy(&buf[LDI_HBM_MTP_LEN1 + LDI_HBM_MTP_LEN2],
			ctrl->rx_buf.data, LDI_HBM_MTP_LEN3 - 1);

	/* HBM ELVSS  */
	msd.elvss_hbm = ctrl->rx_buf.data[3];
	msd.hbm_elvss_cmd.cmds[0].payload[25] = msd.elvss_hbm;
	pr_info("%s: HBM ELVSS: 0x%x\n", __func__, msd.elvss_hbm);

	/* HBM GAMMA  */
	memcpy(&msd.hbm_cmd.cmds[0].payload[1], buf,
		sizeof(buf)/sizeof(unsigned char));

	/* Default ELVSS read */
	mdss_dsi_cmds_send(ctrl, &msd.pos_hbm_cmds.cmds[3], 1, 0);
	length = mdss_dsi_cmd_receive(ctrl, &msd.read_hbm_mtp_cmds.cmds[3],
				LDI_HBM_MTP_LEN4);
	msd.default_hbm = ctrl->rx_buf.data[0];
	pr_info("%s: DEFAULT ELVSS: 0x%x\n", __func__, msd.default_hbm);

	/* GAMMA */
	length = mdss_dsi_cmd_receive(ctrl, &ldi_mtp_cmd, MAX_MTP_CNT);
	if (!length) {
		pr_err("%s : LDI_MTP command failed\n", __func__);
		return -EIO;
	}
	memcpy(mtp_data, ctrl->rx_buf.data, MAX_MTP_CNT);
	mdss_dsi_cmds_send(ctrl, &msd.test_key_off_cmd.cmds[1], 1, 0);

	for (i = 0; i < MAX_MTP_CNT / 3; i++)
		pr_info("%s:[MTP Offset] %d %d %d\n", __func__, mtp_data[3 * i],
				mtp_data[3 * i + 1], mtp_data[3 * i + 2]);
#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
	panel_get_gamma_table(&msd, mtp_data);
#endif

	return 0;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo = NULL;


	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	printk ("oled: Display panel power up started!\n");
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	msd.mpd = pdata;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	pinfo = &(ctrl->panel_data.panel_info);

#ifdef DISPLAY_ALPM_MODES
	/* ALPM rework */
	// If alpm was off and has been enabled...


	if (pinfo->alpm_event){
		printk("oled: Panel ON, Alpm_Event is true\n");
		if (!pinfo->alpm_event(CHECK_PREVIOUS_STATUS)){
			printk ("oled: Panel wasn't in ALPM mode!\n");
			mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);
			}
		else{
			printk("oled: check_previous_status was 1\n");
			}
	}
	else{
		printk ("oled: No alpm event, turn it on anyway\n");
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);
	}
#else /* If ALPM is not used, turn on the lcd */
	mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);
#endif
	msd.mfd->resume_state = MIPI_RESUME_STATE;



/* ALPM Mode Change */
	if (pinfo->alpm_event) {
		printk("oled: Panel On, ALPM Enable hook, Alpm_Event is true\n");
		if (!pinfo->alpm_event(CHECK_PREVIOUS_STATUS) && pinfo->alpm_event(CHECK_CURRENT_STATUS)) {
			/* Turn On ALPM Mode */
			panel_alpm_on(1);
			pinfo->alpm_event(STORE_CURRENT_STATUS);
			pr_info("oled: [ALPM_DEBUG] %s: Send ALPM mode on cmds\n", __func__);
		} else if (!pinfo->alpm_event(CHECK_CURRENT_STATUS) && pinfo->alpm_event(CHECK_PREVIOUS_STATUS)) {
			/* Turn Off ALPM Mode */
			panel_alpm_on(0);
			pinfo->alpm_event(CLEAR_MODE_STATUS);
			pr_info("oled: [ALPM_DEBUG] %s: Send ALPM off cmds\n", __func__);
		}
	}

	/*if (ctrl->on_cmds.cmd_cnt){
		printk ("oled: send powerup commands\n");
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);
		}*/

	if (msd.boot_power_on) {
		printk("oled: Boot power up!\n");
		mdss_dsi_panel_read_mtp(pdata);
		msd.boot_power_on = 0;
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
		gpio_free(msd.esd_det);
#endif
	}

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	msd.esd_status = ESD_NONE;
	gpio_request(msd.esd_det, "esd_det");
	enable_irq(msd.err_irq);
	enable_irq(msd.esd_irq);
	schedule_delayed_work(&panel_check, msecs_to_jiffies(500));
#endif
	//msd.power = FB_BLANK_UNBLANK;

if (boot_mode_recovery){
		printk ("oled: We are in recovery mode, HBM mode please\n");
		// If we're in recovery, set the screen in HBM Mode
		panel_enter_hbm_mode();
		}
else
	printk ("oled: We ain't in recovery mode!\n");

	pr_info("%s Panel power up completed.\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo = NULL;
	
	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	printk ("oled: Panel Power off was called\n");

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pinfo = &(ctrl->panel_data.panel_info);
	printk ("oled: ENTERING TEST %i\n", msd.mfd->blank_mode);
	if (msd.mfd->blank_mode == FB_BLANK_VSYNC_SUSPEND || msd.mfd->blank_mode == FB_BLANK_NORMAL)
		{
		printk ("oled: Set Power: BLANK NORMAL -assuming ALPM ON!\n");
		if (!pinfo->alpm_event(CHECK_CURRENT_STATUS)){
			pinfo->alpm_event(ALPM_MODE_ON);
			panel_alpm_on(1); // ALPM ON
			}
	} 
	printk ("oled: FINISHED TEST\n");
	if (pinfo->alpm_event && pinfo->alpm_event(CHECK_CURRENT_STATUS)){
		pr_info("oled: [ALPM_DEBUG] %s: Not sending Panel Off cmds\n", __func__);
		//enable_irq_wake(msd.esd_irq);
		//enable_irq_wake(msd.err_irq);
	}else{
		printk("oled: Sending display off commands\n");
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);
	}

	pr_info("oled: Shutting down the display \n");
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;

	#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	if (msd.esd_status != ESD_DETECTED) {
		printk ("oled: ESD recovery required!\n");
		disable_irq(msd.esd_irq);
		disable_irq(msd.err_irq);
	}
	cancel_delayed_work_sync(&panel_check);
	gpio_free(msd.esd_det);
	#endif

	pr_info("oled: %s Panel power off finished.\n", __func__);
	return 0;
}

static int mdss_dsi_panel_parse_dcs_string_cmds(struct device_node *of_node,
		struct dsi_panel_cmds *pcmds,
		char *cmd_key, char *link_key)
{
	unsigned int out_value;
	const char *out_text, *out_prop, *out_payload;
	int ret = 0, i, count, total = 0;

	printk("cmd_key[%s]link_state[%s]\n",
		cmd_key, link_key);

	count = of_property_count_strings(of_node, cmd_key);
	printk("count[%d]\n", count);

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

		printk("oled: %d:out_prop[%s]dtype[0x%x]dlen[%x]wait[%d]\n",
			i, out_prop, dchdr->dtype, dchdr->dlen, dchdr->wait);

		total += dchdr->dlen + sizeof(struct dsi_ctrl_hdr);
	}
	pcmds->blen = total;

	pcmds->link_state = DSI_LP_MODE; /* default */

	out_text = of_get_property(of_node, link_key, NULL);
	if (!strncmp(out_text, "dsi_hs_mode", 11))
		pcmds->link_state = DSI_HS_MODE;

	printk("len[%d]cmd_cnt[%d]link_state[%d]\n",
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
	printk("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
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

	msd.lcd_io_en = of_get_named_gpio(np,
			"qcom,lcd-io-en", 0);
	if (!gpio_is_valid(msd.lcd_io_en))
		pr_err("%s:%d lcd_io_en not specified\n",
				__func__, __LINE__);

	msd.oled_id_gpio = of_get_named_gpio(np,
			"qcom,oled-id-gpio", 0);
	if (!gpio_is_valid(msd.oled_id_gpio)) {
		pr_err("%s:%d qcom,oled-id-gpio not specified\n",
				__func__, __LINE__);
	} else {
		rc = gpio_request(msd.oled_id_gpio, "qcom,oled-id-gpio");
		if (rc) {
			pr_err("request oled_id_gpio failed, rc=%d\n",
					rc);
			gpio_free(msd.oled_id_gpio);
		} else {
			rc = gpio_tlmm_config(GPIO_CFG(msd.oled_id_gpio, 0,
					GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN,
					GPIO_CFG_8MA), GPIO_CFG_ENABLE);
			if (rc)
				pr_err("request oled_id_gpio failed,rc=%d\n",
					rc);
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
		printk("%s:%d FBC panel enabled.\n", __func__, __LINE__);
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
		printk("%s:%d Panel does not support FBC.\n",
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
	int rc, i, len, start = 0, end, offset = 0;
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
		printk("%s: problem determining dst format. Set Default\n",
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
		printk("%s: pdest not specified. Set Default\n",
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
			printk("%s: SUCCESS-> WLED TRIGGER register\n",
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

	/* brightness convert table*/
	data = of_get_property(np,
		"qcom,mdss_panel-brightness-convert-table", &tmp);
	if (!data) {
		pr_err("fail to get brightness-convert-table\n");
		return -EINVAL;
	}

	msd.br_map = devm_kzalloc(&msd.msm_pdev->dev,
		sizeof(char) * (ctrl_pdata->bklt_max + 1), GFP_KERNEL);
	if (!msd.br_map) {
		pr_err("failed to allocate br_map\n");
		return -ENOMEM;
	}

	for (i = 0; i < tmp; i++) {
		end = data[offset++];
		memset(&msd.br_map[start], i, end - start + 1);
		start = end + 1;
	}

	mdss_dsi_parse_fbc_params(np, pinfo);
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
			"qcom,mdss-dsi-on-command",
			"qcom,mdss-dsi-on-command-state");
	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
			"qcom,mdss-dsi-off-command",
			"qcom,mdss-dsi-off-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.update_gamma,
			"qcom,mdss-dsi-update-gamma-command",
			"qcom,mdss-dsi-update-gamma-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.acl_off_cmd,
			"qcom,mdss-dsi-acl-off-command",
			"qcom,mdss-dsi-acl-off-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.acl_on_cmd,
			"qcom,mdss-dsi-acl-on-command",
			"qcom,mdss-dsi-acl-on-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.hbm_white_cmd,
			"qcom,mdss-dsi-hbm-white-command",
			"qcom,mdss-dsi-hbm-white-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.ldi_id_cmd,
			"qcom,mdss-dsi-ldi-id-command",
			"qcom,mdss-dsi-ldi-id-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.ldi_mtp_cmd,
			"qcom,mdss-dsi-ldi-mtp-command",
			"qcom,mdss-dsi-ldi-mtp-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.hbm_cmd,
			"qcom,mdss-dsi-hbm-on-command",
			"qcom,mdss-dsi-hbm-on-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.hbm_elvss_cmd,
			"qcom,mdss-dsi-hbm-elvss-command",
			"qcom,mdss-dsi-hbm-elvss-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.alpm_on_cmds,
			"qcom,mdss-dsi-alpm-on-commands",
			"qcom,mdss-dsi-alpm-on-commands-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.alpm_on_temp_cmds,
			"qcom,mdss-dsi-alpm-on-temp-commands",
			"qcom,mdss-dsi-alpm-on-temp-commands-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.alpm_off_cmds,
			"qcom,mdss-dsi-alpm-off-commands",
			"qcom,mdss-dsi-alpm-off-commands-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.tset_on_cmds,
			"qcom,mdss-dsi-temp-offset-on-commands",
			"qcom,mdss-dsi-temp-offset-on-commands-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.tset_off_cmd,
			"qcom,mdss-dsi-temp-offset-off-command",
			"qcom,mdss-dsi-temp-offset-off-command-state");
	mdss_dsi_parse_dcs_cmds(np, &msd.mdnie_off_cmd,
			"qcom,mdss-dsi-mdnie-ctrl-off-command",
			"qcom,mdss-dsi-mdnie-ctrl-off-command-state");

	/* parse dcs command string */
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.test_key_on_cmd,
			"qcom,mdss-dsi-panel-test-key-on-commands",
			"qcom,mdss-dsi-panel-test-key-on-commands-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.test_key_off_cmd,
			"qcom,mdss-dsi-panel-test-key-off-commands",
			"qcom,mdss-dsi-panel-test-key-off-commands-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.acl_table,
			"qcom,mdss-dsi-acl-table-command",
			"qcom,mdss-dsi-acl-table-command-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.pos_hbm_cmds,
			"qcom,mdss-dsi-pos-hbm-commands",
			"qcom,mdss-dsi-pos-hbm-commands-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.read_hbm_mtp_cmds,
			"qcom,mdss-dsi-read-hbm-mtp-commands",
			"qcom,mdss-dsi-read-hbm-mtp-commands-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.aor_cmds,
			"qcom,mdss-dsi-panel-aor-commands",
			"qcom,mdss-dsi-panel-aor-commands-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.elvss_cmds,
			"qcom,mdss-dsi-panel-elvss-commands",
			"qcom,mdss-dsi-panel-elvss-commands-state");
	mdss_dsi_panel_parse_dcs_string_cmds(np, &msd.mdnie_ctl_cmds,
			"qcom,mdss-dsi-mdnie-ctrl-commands",
			"qcom,mdss-dsi-mdnie-ctrl-commands-state");
	on_cmds_state = of_get_property(np,
			"qcom,mdss-dsi-on-command-state", NULL);
	if (!strncmp(on_cmds_state, "dsi_lp_mode", 11)) {
		ctrl_pdata->dsi_on_state = DSI_LP_MODE;
	} else if (!strncmp(on_cmds_state, "dsi_hs_mode", 11)) {
		ctrl_pdata->dsi_on_state = DSI_HS_MODE;
	} else {
		printk("%s: ON cmds state not specified. Set Default\n",
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
		printk("%s: ON cmds state not specified. Set Default\n",
				__func__);
		ctrl_pdata->dsi_off_state = DSI_LP_MODE;
	}

	return 0;
error:
	return -EINVAL;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static int mdss_dsi_panel_get_power(struct lcd_device *ld)
{
	struct mdss_samsung_driver_data *msdd = lcd_get_data(ld);
	printk("%s[%d][%s]\n", __func__,
		msdd->power, current->comm);

	return msdd->power;
}

static int mdss_dsi_panel_set_power(struct lcd_device *ld, int power)
{
	struct mdss_samsung_driver_data *msdd = lcd_get_data(ld);
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
		
	/* We need this to be able to call things inside pinfo-> array */
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pinfo = &(ctrl_pdata->panel_data.panel_info);
	
	if (msdd->power == power) {
		printk ("oled: Set Power: Power mode is the same as it was before\n");
		//ret = -EINVAL;
		return -EINVAL;
		}

	switch (power) {
		case FB_BLANK_UNBLANK:
			printk("oled: Set Power: UNBLANK\n");
			if (pinfo->alpm_event(CHECK_CURRENT_STATUS))
				panel_alpm_on(0);
			msdd->power = power;
			break;
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			printk ("oled: Set Power: BLANK NORMAL -assuming ALPM ON!\n");
			if (!pinfo->alpm_event(CHECK_CURRENT_STATUS)){
				pinfo->alpm_event(ALPM_MODE_ON);
				panel_alpm_on(1); // ALPM ON
				}
			msdd->power = power;
			break;
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_POWERDOWN:
			printk("oled: Set Power: BLANK Powerdown\n");
			msdd->power = power;
			break;
		default:
			printk ("oled: Set Power: Default case hit\n");
			break;
		} // switch Power End
	return 0;
}
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static void panel_temp_offset_control(void)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	const unsigned char tset_tbl[4] = {
		0x00, 0x80, 0x8a, 0x94};

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[1], 1, 0);
	if (msd.temp_stage == TEMP_RANGE_0)
		mdss_dsi_cmds_send(ctrl_pdata, &msd.tset_off_cmd.cmds[0], 1, 0);
	else {
		msd.tset_on_cmds.cmds[1].payload[1] = tset_tbl[msd.temp_stage];
		mdss_dsi_panel_cmds_send(ctrl_pdata,
				&msd.tset_on_cmds);
	}
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[1], 1, 0);

	return;
}

static int panel_update_brightness(struct backlight_device *bd,
							int level)
{
	struct mdss_samsung_driver_data *msdd = bl_get_data(bd);
	struct mdss_panel_data *pdata = msdd->mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
	struct dsi_cmd_desc gamma_cmd;
	struct dsi_ctrl_hdr dchdr = {DTYPE_NULL_PKT, 1, 0, 0, 1, 0};
#endif
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[1], 1, 0);

	dchdr.dtype = DTYPE_DCS_LWRITE;
	dchdr.dlen = GAMMA_CMD_CNT;
	gamma_cmd.dchdr = dchdr;
	gamma_cmd.payload = (char *)msdd->gamma_tbl[level];
	mdss_dsi_cmds_send(ctrl_pdata, &gamma_cmd, 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.aor_cmds.cmds[level], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.elvss_cmds.cmds[level], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.update_gamma.cmds[0], 1, 0);
	usleep(10000);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[1], 1, 0);
#endif

	if (msd.temp_stage != TEMP_RANGE_0)
		panel_temp_offset_control();

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
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int ret = 0, level = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	if (brightness < pdata->panel_info.bl_min || brightness > bd->props.max_brightness) {
		pr_err("brightness should be %d to %d\n",
			pdata->panel_info.bl_min, bd->props.max_brightness);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	level = msd.br_map[brightness];

	pr_info("%s[%d]%s\n", "set_br", brightness, msd.hbm_on ? "[hbm_on]" : "");

	if (msd.power != FB_BLANK_UNBLANK) {
		pr_info("brightness is not support under power[%d]brightness[%d]\n",
				msd.power, brightness);
		ret = -EPERM;
		goto out;
	}

	/* ALPM rework */
	if (pinfo->alpm_event(CHECK_CURRENT_STATUS)){
		printk ("oled: ALPM enabled, skip brightness setting\n");
		return 0;


	}
	/*if (msd.alpm_mode) {
		pr_info("brightness control is not supported under alpm mode\n");
		ret = 0;
		goto out;
	}*/

	/*ret = mdss_dsi_runtime_active(ctrl_pdata, true, __func__);
	if (ret){
		pr_info("%s:failed to runtime_active:ret[%d]\n", __func__, ret);
		mdss_dsi_runtime_active(ctrl_pdata, false, __func__);
		goto out;
	}*/

	if (msd.hbm_on)
		panel_enter_hbm_mode();
	else
		panel_update_brightness(bd, level);

	//mdss_dsi_runtime_active(ctrl_pdata, false, __func__);

out:
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

static ssize_t mdss_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char temp[20];

	snprintf(temp, 20, "SDC_%02x%02x%02x\n",
			msd.ldi_id[0], msd.ldi_id[1], msd.ldi_id[2]);
	strlcat(buf, temp, 20);
	return strnlen(buf, 20);
}

static void panel_acl_update(unsigned int value)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);

	/* TODO send acl command*/
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

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[1], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.hbm_cmd.cmds[0], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.aor_cmds.cmds[MAX_GAMMA_CNT - 1], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.hbm_elvss_cmd.cmds[0], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.mdnie_ctl_cmds.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.mdnie_ctl_cmds.cmds[1], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.update_gamma.cmds[0], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[1], 1, 0);

	return;
}

static void panel_exit_hbm_mode(void)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	unsigned char buf[2];
	struct dsi_ctrl_hdr dchdr = {DTYPE_DCS_LWRITE, 1, 0, 0, 1, 0};
	struct dsi_cmd_desc default_hbm_cmd;
	struct backlight_device *bd = msd.bd;
	int level;
	unsigned int brightness = bd->props.brightness;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	buf[0] = LDI_HBM_ELVSS;
	buf[1] = msd.default_hbm;
	dchdr.dlen = sizeof(buf);
	default_hbm_cmd.dchdr = dchdr;
	default_hbm_cmd.payload = buf;

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[1], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.pos_hbm_cmds.cmds[3], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &default_hbm_cmd, 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.update_gamma.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[1], 1, 0);

	level = msd.br_map[brightness];
	panel_update_brightness(bd, level);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[1], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.mdnie_off_cmd.cmds[0], 1, 0);
	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[1], 1, 0);

	return;
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
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

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

	if (!pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	/*if (mdss_dsi_runtime_active(ctrl_pdata, true, __func__)) {
		pr_err("hbm control:invalid state.\n");
		mdss_dsi_runtime_active(ctrl_pdata, false, __func__);
		return -EPERM;
	}*/

	if (msd.hbm_on) {
		pr_info("HBM ON.\n");
		panel_enter_hbm_mode();
	} else {
		pr_info("HBM OFF.\n");
		panel_exit_hbm_mode();
	}

	//mdss_dsi_runtime_active(ctrl_pdata, false, __func__);

	return size;
}

/* Primary ALPM Handling functions */
#ifdef DISPLAY_ALPM_MODES
u8 panel_alpm_handler(u8 flag)
{
//	static u8 brightness = 60; /* Default brightness level is 60cd */
	u8 ret = 0;

	switch (flag) {
		case ALPM_MODE_ON:
			panel_actual_alpm_mode = ALPM_MODE_ON;
			printk ("oled: ALPM_HANDLE : ALPM MODE ON\n");
			break;
		case NORMAL_MODE_ON:
			panel_actual_alpm_mode = MODE_OFF;
			printk ("oled: ALPM_HANDLE : NORMAL MODE ON\n");
			break;
		case MODE_OFF:
			panel_actual_alpm_mode = MODE_OFF;
			printk ("oled: ALPM_HANDLE : MODE OFF\n");
			break;
		case CHECK_CURRENT_STATUS:
			ret = panel_actual_alpm_mode;
			printk ("oled: ALPM_HANDLE : CHECK CURRENT STATUS: %i\n", panel_actual_alpm_mode);
			break;
		case CHECK_PREVIOUS_STATUS:
			ret = panel_previous_alpm_mode;
			printk ("oled: ALPM_HANDLE : CHK PREVIOUS STATUS %i\n", panel_previous_alpm_mode);
			break;
		case STORE_CURRENT_STATUS:
			panel_previous_alpm_mode = panel_actual_alpm_mode;
			printk ("oled: ALPM_HANDLE : STORE CURRENT STATUS: %i\n", panel_actual_alpm_mode);
			break;
		case CLEAR_MODE_STATUS:
			panel_previous_alpm_mode = 0;
			panel_actual_alpm_mode = 0;
			printk ("oled: ALPM_HANDLE : CLEAR CURRENT STATUS\n");
			break;
		default:
			printk ("oled: Panel Handler - default case\n");
			break;
	}
	return ret;
}

static void alpm_store(	struct mdss_panel_info *pinfo, u8 mode)
{

	/* Register ALPM event function */
	if (unlikely(!pinfo->alpm_event))
		pinfo->alpm_event = panel_alpm_handler;

	pinfo->alpm_event(mode);
}

static void panel_alpm_on(unsigned int enable)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,	panel_data);

	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
	// Not sure what this is for... temporary alpm enable?
	// why should it be temporary? you either turn it on or off
	// damn it samsung...
	/*	if (msd.ldi_id[2] < 0x14) {
			mdss_dsi_panel_cmds_send(ctrl_pdata,&msd.alpm_on_temp_cmds);
			pr_info("%s: Temp ALPM ON.\n", __func__);
			panel_alpm_handler(ALPM_STATE_SYNC);
			goto out;
		}*/
		/* So, as we don't know shit, we at least scream if we got here */
		if (msd.ldi_id[2] < 0x14) {
			printk ("oled: Here I am, screaming in despair, trying to know \n");
			printk ("oled: Why the fuck I am being called. \n");
		} // end of msd.ldi_id

		// Turn it on and fuck it.
		mdss_dsi_panel_cmds_send(ctrl_pdata, &msd.alpm_on_cmds);
		pr_info("oled: %s: ALPM ON.\n", __func__);
		pinfo->alpm_event(STORE_CURRENT_STATUS);
	} else {
		// Turn it off and stfu.
		mdss_dsi_panel_cmds_send(ctrl_pdata, &msd.alpm_off_cmds);
		pr_info("oled: %s: ALPM OFF.\n", __func__);
		pinfo->alpm_event(CLEAR_MODE_STATUS);
	}

	return;
}

/* Return ALPM status via sysfs */
static ssize_t panel_alpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
//	int rc;
	int alpm_mode = 0;
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	alpm_mode = (int)pinfo->alpm_event(CHECK_CURRENT_STATUS);

	//rc = 0;//snprintf((char *)buf, sizeof(buf), "%d\n", panel_actual_alpm_mode);
	pr_info("oled: [ALPM_DEBUG] %s: alpm current status : %d \n",\
					 __func__,  alpm_mode);

	return alpm_mode;
}
/* Receive the command from SysFS and act accordingly*/

static ssize_t panel_alpm_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t size)
{
	int mode = 0;
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
			panel_data);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	sscanf(buf, "%d" , &mode);
	pr_info("oled: [ALPM_DEBUG] %s: mode : %d,\n",
			__func__, mode);

if (mode == ALPM_MODE_ON) {
	alpm_store(pinfo, mode); // store current mode
	/* If ALPM was OFF and it is now ON, turn on
	ALPM, otherwise, do nothing */
	if (!pinfo->alpm_event(CHECK_PREVIOUS_STATUS) //check!
		&& pinfo->alpm_event(CHECK_CURRENT_STATUS)) {
			/* Turn On ALPM Mode */
			pinfo->alpm_event(ALPM_MODE_ON);
			panel_alpm_on(1);
			pr_info("oled: [ALPM_DEBUG] %s: Send ALPM mode on cmds\n", __func__);
			}
		}
/* If ALPM was ON and now it's requested OFF, turn it off,
otherwise, do nothing */
else if (mode == MODE_OFF) {
	alpm_store(pinfo, mode); // Store new mode
	if (pinfo->alpm_event) { // check check check
		if (pinfo->alpm_event(CHECK_PREVIOUS_STATUS) == ALPM_MODE_ON) {
				panel_alpm_on(0);
				pinfo->alpm_event(NORMAL_MODE_ON);
				pr_info("[ALPM_DEBUG] %s: Send ALPM off cmds\n", __func__);
			}
		} // if pinfo->alpm_event
	} // else if mode OFF
	return size;
}
#endif
/* End of ALPM Event functions */

static ssize_t elvss_control_show(struct device *dev, struct
	device_attribute * attr, char *buf)
{

	return snprintf(buf, PAGE_SIZE, "%d\n",
			msd.temp_stage);
}

static ssize_t elvss_control_store(struct device *dev, struct
	device_attribute * attr, const char *buf, size_t size)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	unsigned int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	msd.temp_stage = value;

	if (msd.power > FB_BLANK_UNBLANK) {
		pr_info("elvss control before lcd enable.\n");
		return -EPERM;
	}

	if (!pdata) {
		pr_info("failed to get pdata.\n");
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
								panel_data);

	/*if (mdss_dsi_runtime_active(ctrl_pdata, true, __func__)) {
		pr_err("elvss control:invalid state.\n");
		mdss_dsi_runtime_active(ctrl_pdata, false, __func__);
		return -EPERM;
	}*/

	dev_info(dev, "elvss temperature stage[%d]\n",
						msd.temp_stage);
	panel_temp_offset_control();

	//mdss_dsi_runtime_active(ctrl_pdata, false, __func__);

	return size;
}

#if MDNIE_TUNE
#include <linux/firmware.h>
#define MDNIECTL1_SIZE	20
#define MDNIECTL2_SIZE	75
static void mdnie_get_tune_dat(const u8 *data)
{
	unsigned int val1, val2, val3, val4, val5 = 0 ;
	int ret;
	char *str = NULL;
	int i = 0;
	unsigned char tune[95];

	while ((str = strsep((char **)&data, "\n"))) {
		ret = sscanf(str, "%2x,%2x,%2x,%2x,%2x,\n",
			&val1, &val2, &val3, &val4, &val5);
		if (ret == 5) {
			tune[i] = val1;
			tune[i + 1] = val2;
			tune[i + 2] = val3;
			tune[i + 3] = val4;
			tune[i + 4] = val5;
			i = i + 5;
			if ((val1 == 0xff) && (val2 == 0) && (val3 == 0)
				&& (val4 == 0) && (val5 == 0))
				break;
		}
	}

	memcpy(&msd.mdnie_ctl_cmds.cmds[0].payload[1],
			tune, MDNIECTL1_SIZE);
	memcpy(&msd.mdnie_ctl_cmds.cmds[1].payload[1],
			&tune[MDNIECTL1_SIZE], MDNIECTL2_SIZE);

}

static ssize_t panel_mdnie_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, 5, "%s\n", msd.mdnie_on ? "on" : "off");
}

static ssize_t panel_mdnie_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	struct mdss_panel_data *pdata = msd.mpd;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	const struct firmware *fw;
	char fw_path[256];
	int ret;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);

	if (!strncmp(buf, "on", 2)) {
		msd.mdnie_on = true;
		pr_info("%s: mDnie tune enable\n", __func__);
	} else if (!strncmp(buf, "off", 3)) {
		msd.mdnie_on = false;
		pr_info("%s: mDnie tune disable\n", __func__);
		goto out;
	} else {
		dev_warn(dev, "invalid command.\n");
		return size;
	}

	ret = request_firmware((const struct firmware **)&fw,
					"mdnie_tune.dat", dev);
	if (ret) {
		dev_err(dev, "ret: %d, %s: fail to request %s\n",
					ret, __func__, fw_path);
		return ret;
	}
	mdnie_get_tune_dat(fw->data);

	release_firmware(fw);

out:
	/*if (mdss_dsi_runtime_active(ctrl_pdata, true, __func__)) {
		pr_err("invalid state.\n");
		mdss_dsi_runtime_active(ctrl_pdata, false, __func__);
		return -EPERM;
	}*/

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_on_cmd.cmds[1], 1, 0);

	if (msd.mdnie_on) {
		mdss_dsi_cmds_send(ctrl_pdata,
					&msd.mdnie_ctl_cmds.cmds[0], 1, 0);
		mdss_dsi_cmds_send(ctrl_pdata,
					&msd.mdnie_ctl_cmds.cmds[1], 1, 0);
	} else
		mdss_dsi_cmds_send(ctrl_pdata,
					&msd.mdnie_off_cmd.cmds[0], 1, 0);

	mdss_dsi_cmds_send(ctrl_pdata, &msd.test_key_off_cmd.cmds[1], 1, 0);

	//mdss_dsi_runtime_active(ctrl_pdata, false, __func__);

	return size;
}
#endif
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
static void mdss_dsi_panel_uevent_handler(void)
{
	char *event_string = "LCD_ESD=ON";
	char *envp[] = { event_string, NULL };
	struct mdss_panel_data *pdata = msd.mpd;
	int ret = 0;

	disable_irq_nosync(msd.esd_irq);
	disable_irq_nosync(msd.err_irq);
	pdata->panel_info.panel_dead = true;
	msd.esd_status = ESD_DETECTED;
	kobject_uevent_env(&esd_dev->kobj,
		KOBJ_CHANGE, envp);
	ret = max77836_comparator_set(true);
	if (ret < 0)
		pr_err("%s: comparator set failed.\n", __func__);
}

static irqreturn_t mdss_dsi_panel_esd_handler(int irq, void *handle)
{
	if (msd.power == FB_BLANK_POWERDOWN) {
		printk("%s: Skip oled_det recovery\n", __func__);
		return IRQ_HANDLED;
	}
	if (msd.esd_status == ESD_DETECTED) {
		printk("%s: on progressing\n", __func__);
		return IRQ_HANDLED;
	}
	printk("%s: esd irq handler.\n", __func__);
	if (!gpio_get_value(msd.esd_det)) {
		mdss_dsi_panel_uevent_handler();
		pr_err("%s: ESD DETECTED(OLED_DET)\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t mdss_dsi_panel_err_handler(int irq, void *handle)
{
	if (msd.power == FB_BLANK_POWERDOWN) {
		printk("%s: Skip err_fg recovery\n", __func__);
		return IRQ_HANDLED;
	}
	if (msd.esd_status == ESD_DETECTED) {
		printk("%s: on progressing\n", __func__);
		return IRQ_HANDLED;
	}
	printk("%s: err_fg irq handler.\n", __func__);
	if (gpio_get_value(msd.err_fg)) {
		mdss_dsi_panel_uevent_handler();
		pr_err("%s: ESD DETECTED(ERR_FG)\n", __func__);
	}

	return IRQ_HANDLED;
}

static void mdss_dsi_panel_check_status(struct work_struct *work)
{
	if (msd.power == FB_BLANK_POWERDOWN) {
		printk("%s: Skip(powerdown)\n", __func__);
		return;
	}
	if (msd.esd_status == ESD_DETECTED) {
		printk("%s: Skip(on recovery)\n", __func__);
		return;
	}

	if (gpio_get_value(msd.err_fg)) {
		mdss_dsi_panel_uevent_handler();
		pr_err("%s: ESD DETECTED(ERR_FG)\n", __func__);
		if (!gpio_get_value(msd.esd_det))
			pr_err("%s: ESD DETECTED(ERR_FG & ESD)\n", __func__);
	} else if (!gpio_get_value(msd.esd_det)) {
		mdss_dsi_panel_uevent_handler();
		pr_err("%s: ESD DETECTED(ESD)\n", __func__);
	}

}
#endif

DEVICE_ATTR(lcd_type, S_IRUGO, mdss_disp_lcdtype_show, NULL);
static struct device_attribute lcd_dev_attrs[] = {
	__ATTR(acl, S_IRUGO | S_IWUSR, panel_acl_show, panel_acl_store),
	__ATTR(hbm, S_IRUGO | S_IWUSR, panel_hbm_show, panel_hbm_store),
	#ifdef DISPLAY_ALPM_MODES
	__ATTR(alpm, S_IRUGO | S_IWUSR, panel_alpm_show, panel_alpm_store),
	#endif
	__ATTR(elvss, S_IRUGO|S_IWUSR|S_IWGRP,
			elvss_control_show, elvss_control_store),
#if MDNIE_TUNE
	__ATTR(tune, S_IRUGO | S_IWUSR | S_IWGRP,
			panel_mdnie_show, panel_mdnie_store),
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
#if defined CONFIG_LCD_CLASS_DEVICE
	struct lcd_device *lcd_device;
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	int i;
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
	msd.msm_pdev = pdev;
#endif
	if (!node) {
		pr_err("%s: no panel node\n", __func__);
		return -ENODEV;
	}

	printk("%s:%d\n", __func__, __LINE__);
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name)
		pr_info("%s:%d, Panel name not specified\n",__func__, __LINE__);

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

	cont_splash_enabled = of_property_read_bool(node,
			"qcom,cont-splash-enabled");
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
	/* Only supported in newer kernels / tizen variant */

	/*ctrl_pdata->panel_data.panel_info.ulps_feature_enabled =
		of_property_read_bool(node, "qcom,ulps-enabled");
	pr_info("%s: ulps feature %s", __func__,
		(ctrl_pdata->panel_data.panel_info.ulps_feature_enabled ?
		"enabled" : "disabled"));*/

	msd.boot_power_on = of_property_read_bool(node,	"qcom,was-enable");


	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->panel_reset = mdss_dsi_panel_reset;
	//ctrl_pdata->alpm_event = panel_alpm_handler; We may use this later
	pinfo->alpm_event = panel_alpm_handler;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("s6e36w0x01", &pdev->dev, &msd,
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
	bd = backlight_device_register("s6e36w0x01-bl", &lcd_device->dev,
			&msd, &bd_ops, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		goto err_backlight_register;
	}

	bd->props.max_brightness = ctrl_pdata->bklt_max;
	bd->props.brightness = ctrl_pdata->bklt_max;
	msd.bd = bd;
#endif
#endif

#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
	msd.esd_status = ESD_NONE;

	if (!get_lcd_attached()) {
		pr_info("%s: lcd_not_attached\n", __func__);
		goto err_lcd_attached;
	}

	INIT_DELAYED_WORK(&panel_check, mdss_dsi_panel_check_status);

	msd.esd_det = of_get_named_gpio(node, "qcom,oled-det-gpio", 0);
	msd.esd_irq = gpio_to_irq(msd.esd_det);
	rc = gpio_request(msd.esd_det, "esd_det");
	if (rc) {
		pr_err("request gpio GPIO_ESD failed, ret=%d\n", rc);
		goto err_gpio_oled_fail;
	}
	gpio_tlmm_config(GPIO_CFG(msd.esd_det,  0, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	rc = gpio_direction_input(msd.esd_det);
	if (unlikely(rc < 0)) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, msd.esd_det, rc);
	}

	rc = request_threaded_irq(msd.esd_irq, NULL,
				mdss_dsi_panel_esd_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"esd_detect", NULL);
	if (rc)
		pr_err("%s : Failed to request_irq. :ret=%d", __func__, rc);

	disable_irq(msd.esd_irq);

	msd.err_fg = of_get_named_gpio(node, "qcom,err-fg-gpio", 0);
	msd.err_irq = gpio_to_irq(msd.err_fg);
	rc = gpio_request(msd.err_fg, "err_fg");
	if (rc) {
		pr_err("request gpio GPIO_ERR_FG failed, ret=%d\n", rc);
		goto err_gpio_err_fail;
	}
	gpio_tlmm_config(GPIO_CFG(msd.err_fg, 0, GPIO_CFG_INPUT,
					GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
					GPIO_CFG_ENABLE);
	rc = gpio_direction_input(msd.err_fg);
	if (unlikely(rc < 0)) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, msd.err_fg, rc);
	}

	rc = request_threaded_irq(msd.err_irq, NULL,
				mdss_dsi_panel_err_handler,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"err_fg", NULL);
	if (rc)
		pr_err("%s : Failed to request_irq. :ret=%d", __func__, rc);

	esd_class = class_create(THIS_MODULE, "lcd_event");
	if (IS_ERR(esd_class)) {
		dev_err(&msd.msm_pdev->dev, "Failed to create class(lcd_event)!\n");
		rc = PTR_ERR(esd_class);
		goto err_esd_class_fail;
	}
	esd_dev = device_create(esd_class, NULL, 0, NULL, "esd");

	disable_irq(msd.err_irq);
#endif
#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
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
	msd.temp_stage = TEMP_RANGE_0;
	if (ctrl_pdata->panel_data.panel_info.cont_splash_enabled)
		msd.power = FB_BLANK_UNBLANK;
	else
		msd.power = FB_BLANK_POWERDOWN;

	return 0;
#ifdef CONFIG_SMART_DIMMING_S6E36W0X01
err_gamma_alloc:
	devm_kfree(&pdev->dev, msd.dimming);
err_dimming_alloc:
#endif
#if defined(CONFIG_ESD_ERR_FG_RECOVERY)
err_esd_class_fail:
	gpio_free(msd.err_fg);
err_gpio_err_fail:
	gpio_free(msd.esd_det);
err_lcd_attached:
err_gpio_oled_fail:
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
