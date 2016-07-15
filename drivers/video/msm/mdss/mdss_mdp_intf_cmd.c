/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/bootmem.h>

#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_dsi.h"

#define VSYNC_EXPIRE_TICK 2
#define RT_ACT_EXPIRE_TICK 4

#define START_THRESHOLD 4
#define CONTINUE_THRESHOLD 4

#define MAX_SESSIONS 2

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KOFF_TIMEOUT msecs_to_jiffies(84)
#define STOP_TIMEOUT msecs_to_jiffies(200)

static DEFINE_MUTEX(mdp_cmd_lock);

struct mdss_mdp_cmd_ctx {
	struct mdss_mdp_ctl *ctl;
	u32 pp_num;
	u8 ref_cnt;
	struct completion pp_comp;
	struct completion stop_comp;
	struct list_head vsync_handlers;
	int panel_on;
	int koff_cnt;
	int clk_enabled;
	int vsync_enabled;
	int rdptr_enabled;
	struct mutex clk_mtx;
	struct mutex vsync_mtx;
	spinlock_t clk_lock;
	struct work_struct clk_work;
	struct work_struct pp_done_work;
	atomic_t pp_done_cnt;

	/* te config */
	u8 tear_check;
	u16 height;	/* panel height */
	u16 vporch;	/* vertical porches */
	u16 start_threshold;
	u32 vclk_line;	/* vsync clock per line */
	struct mdss_panel_recovery recovery;
	bool ulps;
	bool need_wait;
	atomic_t rt_active;
	u32 dbg_cnt;
};

struct mdss_mdp_cmd_ctx mdss_mdp_cmd_ctx_list[MAX_SESSIONS];
int get_lcd_attached(void);

static inline u32 mdss_mdp_cmd_line_count(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_mixer *mixer;
	u32 cnt = 0xffff;	/* init it to an invalid value */
	u32 init;
	u32 height;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false, __func__);

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (!mixer) {
		mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
		if (!mixer) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false, __func__);
			goto exit;
		}
	}

	init = mdss_mdp_pingpong_read
		(mixer, MDSS_MDP_REG_PP_VSYNC_INIT_VAL) & 0xffff;

	height = mdss_mdp_pingpong_read
		(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT) & 0xffff;

	if (height < init) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false, __func__);
		goto exit;
	}

	cnt = mdss_mdp_pingpong_read
		(mixer, MDSS_MDP_REG_PP_INT_COUNT_VAL) & 0xffff;

	if (cnt < init)		/* wrap around happened at height */
		cnt += (height - init);
	else
		cnt -= init;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false, __func__);

	pr_debug("cnt=%d init=%d height=%d\n", cnt, init, height);
exit:
	return cnt;
}

/*
 * TE configuration:
 * dsi byte clock calculated base on 70 fps
 * around 14 ms to complete a kickoff cycle if te disabled
 * vclk_line base on 60 fps
 * write is faster than read
 * init == start == rdptr
 */
static int mdss_mdp_cmd_tearcheck_cfg(struct mdss_mdp_mixer *mixer,
			struct mdss_mdp_cmd_ctx *ctx, int enable)
{
	u32 cfg;

	cfg = BIT(19); /* VSYNC_COUNTER_EN */
	if (ctx->tear_check)
		cfg |= BIT(20);	/* VSYNC_IN_EN */
	cfg |= ctx->vclk_line;

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT,
				0xfff0); /* set to verh height */

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_VSYNC_INIT_VAL,
						ctx->height);

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_RD_PTR_IRQ,
						ctx->height + 1);

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_START_POS,
						ctx->height);

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_THRESH,
		   (CONTINUE_THRESHOLD << 16) | (ctx->start_threshold));

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_TEAR_CHECK_EN, enable);
	return 0;
}

static int mdss_mdp_cmd_tearcheck_setup(struct mdss_mdp_ctl *ctl, int enable)
{
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_mixer *mixer;

	pinfo = &ctl->panel_data->panel_info;

	if (pinfo->mipi.vsync_enable && enable) {
		u32 mdp_vsync_clk_speed_hz, total_lines;

		mdss_mdp_vsync_clk_enable(1);

		mdp_vsync_clk_speed_hz =
		mdss_mdp_get_clk_rate(MDSS_CLK_MDP_VSYNC);
		pr_debug("%s: vsync_clk_rate=%d\n", __func__,
					mdp_vsync_clk_speed_hz);

		if (mdp_vsync_clk_speed_hz == 0) {
			pr_err("can't get clk speed\n");
			return -EINVAL;
		}

		ctx->tear_check = pinfo->mipi.hw_vsync_mode;
		ctx->height = pinfo->yres;
		ctx->vporch = pinfo->lcdc.v_back_porch +
				    pinfo->lcdc.v_front_porch +
				    pinfo->lcdc.v_pulse_width;

		ctx->start_threshold = START_THRESHOLD;

		total_lines = ctx->height + ctx->vporch;
		total_lines *= pinfo->mipi.frame_rate;
		ctx->vclk_line = mdp_vsync_clk_speed_hz / total_lines;

		pr_debug("%s: fr=%d tline=%d vcnt=%d thold=%d vrate=%d\n",
			__func__, pinfo->mipi.frame_rate, total_lines,
				ctx->vclk_line, ctx->start_threshold,
				mdp_vsync_clk_speed_hz);
	} else {
		enable = 0;
	}

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (mixer)
		mdss_mdp_cmd_tearcheck_cfg(mixer, ctx, enable);

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
	if (mixer)
		mdss_mdp_cmd_tearcheck_cfg(mixer, ctx, enable);

	return 0;
}

static inline int mdss_mdp_cmd_ulps_on(struct mdss_mdp_cmd_ctx *ctx, bool on)
{
	struct mdss_mdp_ctl *ctl = ctx->ctl;
	struct msm_fb_data_type *mfd = ctx->ctl->mfd;
	int ret = 0;

	if (on) {
		if (ctl->mfd->use_rotator) {
			pr_info("use_rotator:bypass to enter ulps\n");
			return 0;
		}

		if (atomic_read(&ctx->rt_active)) {
			pr_err("runtime active:bypass to enter ulps\n");
			return 0;
		}

		mdss_mdp_check_suspended(&ctl->mfd->pdev->dev, false, __func__);

		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_ULPS_CTRL, (void *)1);
		if (!ret) {
			ctx->ulps = true;
			ctl->play_cnt = 0;

			if (ctl->roi.x || ctl->roi.y ||
				ctl->roi.w != ctl->panel_data->panel_info.xres ||
				ctl->roi.h != ctl->panel_data->panel_info.yres) {
				pr_info("mdss:%s:reset rect[%d %d %d %d]\n",
				"clk_off", ctl->roi.x, ctl->roi.y,
				ctl->roi.w, ctl->roi.h);
				ctl->roi = (struct mdss_mdp_img_rect) {0, 0, 0, 0};
			}

			mdss_mdp_footswitch_ctrl_ulps(0, &ctl->mfd->pdev->dev);
		} else
			pr_err("failed to enter dsi ulps:ret[%d]rd[%d]clk[%d]vs[%d]rt[%d]\n",
				ret, ctx->rdptr_enabled, ctx->clk_enabled,
				ctx->vsync_enabled, atomic_read(&ctx->rt_active));
	} else {
		struct mdss_panel_info *panel_info = mfd->panel_info;
		bool alpm_mode = panel_info->alpm_event &&
			panel_info->alpm_event(ALPM_MODE_STATE);

		mdss_mdp_check_suspended(&ctl->mfd->pdev->dev, true, __func__);

		mdss_mdp_footswitch_ctrl_ulps(1, &mfd->pdev->dev);

		if (mdss_mdp_pcc_enabled(ctx->ctl) && !alpm_mode)
			mdss_mdp_pcc_update(ctx->ctl, false);

		if (mdss_mdp_cmd_tearcheck_setup(ctx->ctl, 1))
			pr_warn("tearcheck setup failed\n");
		ret = mdss_mdp_ctl_intf_event(ctx->ctl,
			MDSS_EVENT_DSI_ULPS_CTRL, (void *)0);
		if (ret) {
			pr_err("failed to exit dsi ulps:ret[%d]rd[%d]clk[%d]vs[%d]rt[%d]\n",
				ret, ctx->rdptr_enabled, ctx->clk_enabled,
				ctx->vsync_enabled, atomic_read(&ctx->rt_active));
		} else
			ctx->ulps = false;
	}

	return ret;
}

static inline void mdss_mdp_cmd_clk_on(struct mdss_mdp_cmd_ctx *ctx)
{
	unsigned long flags;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int ret;

	mutex_lock(&ctx->clk_mtx);

	if (!ctx->clk_enabled && get_lcd_attached()) {
		ctx->clk_enabled = 1;

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false, __func__);

		if (mdss_dsi_ulps_feature_enabled(ctx->ctl->panel_data) &&
			ctx->ulps) {
			ret = mdss_mdp_cmd_ulps_on(ctx, false);
			if (ret) {
				pr_err("try again to exit ulps\n");
				ret = mdss_mdp_cmd_ulps_on(ctx, false);
			}
		}

		mdss_mdp_ctl_intf_event
			(ctx->ctl, MDSS_EVENT_PANEL_CLK_CTRL, (void *)1);

		mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_RESUME);
	}
	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (!ctx->rdptr_enabled)
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num);
	ctx->rdptr_enabled = atomic_read(&ctx->rt_active) ?
		RT_ACT_EXPIRE_TICK : VSYNC_EXPIRE_TICK;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (ctx->dbg_cnt)
		pr_info("mdss:%s:rd[%d]clk[%d]vs[%d]ulps[%d]panel_on[%d]d[%d]\n",
			"clk_on", ctx->rdptr_enabled, ctx->clk_enabled,
			ctx->vsync_enabled, ctx->ulps, ctx->panel_on, ctx->dbg_cnt);

	mutex_unlock(&ctx->clk_mtx);
}

static inline int mdss_mdp_cmd_ulps_recovery(struct mdss_mdp_cmd_ctx *ctx)
{
	unsigned long flags;

	pr_info("%s:start:rd[%d]clk[%d]vs[%d]rt[%d]ulps[%d]\n",
		__func__, ctx->rdptr_enabled, ctx->clk_enabled,
		ctx->vsync_enabled, atomic_read(&ctx->rt_active), ctx->ulps);

	ctx->ulps = true;
	mdss_mdp_cmd_clk_on(ctx);

	spin_lock_irqsave(&ctx->clk_lock, flags);
	ctx->rdptr_enabled = RT_ACT_EXPIRE_TICK << 2;
	ctx->dbg_cnt = ctx->rdptr_enabled;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	pr_info("%s:done:rd[%d]clk[%d]vs[%d]rt[%d]ulps[%d]\n",
		__func__, ctx->rdptr_enabled, ctx->clk_enabled,
		ctx->vsync_enabled, atomic_read(&ctx->rt_active), ctx->ulps);

	return 0;
}

static inline void mdss_mdp_cmd_clk_off(struct mdss_mdp_cmd_ctx *ctx)
{
	unsigned long flags;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_ctl *ctl = ctx->ctl;
	int set_clk_off = 0, ret = 0;

	mutex_lock(&ctx->clk_mtx);
	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (!ctx->rdptr_enabled)
		set_clk_off = 1;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if ((ctx->clk_enabled && set_clk_off) || (get_lcd_attached() == 0)) {
		ctx->clk_enabled = 0;
		mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_SUSPEND);
		mdss_mdp_ctl_intf_event
			(ctl, MDSS_EVENT_PANEL_CLK_CTRL, (void *)0);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false, __func__);
		if (mdss_dsi_ulps_feature_enabled(ctl->panel_data) &&
			ctx->panel_on)
			ret = mdss_mdp_cmd_ulps_on(ctx, true);
	}

	if (ctx->dbg_cnt)
		pr_info("mdss:%s:rd[%d]clk[%d]vs[%d]ulps[%d]panel_on[%d]d[%d]\n",
			"clk_off", ctx->rdptr_enabled, ctx->clk_enabled,
			ctx->vsync_enabled, ctx->ulps, ctx->panel_on, ctx->dbg_cnt);

	mutex_unlock(&ctx->clk_mtx);
	if (ret)
		mdss_mdp_cmd_ulps_recovery(ctx);
}

static int mdss_mdp_cmd_ulps_trigger(struct mdss_mdp_ctl *ctl, bool on)
{
	struct mdss_mdp_cmd_ctx *ctx;
	int ret = 0;

	mutex_lock(&mdp_cmd_lock);

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_info("mdss:%s:[%s]invalid state\n", "ulps_trg",
			on ? "on" : "off");
		ret = -ENODEV;
		goto out;
	}


	if (on) {
		if (ctx->ulps)
			pr_info("mdss:%s:[on]exit ulps\n", "ulps_trg");
		atomic_set(&ctx->rt_active, 1);
		mdss_mdp_cmd_clk_on(ctx);
	} else {
		pr_info("mdss:%s:[off]\n", "ulps_trg");
		atomic_set(&ctx->rt_active, 0);
	}

out:
	mutex_unlock(&mdp_cmd_lock);
	return ret;
}

static void mdss_mdp_cmd_readptr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;
	struct mdss_mdp_vsync_handler *tmp;
	ktime_t vsync_time;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	mdss_mdp_ctl_perf_taken(ctl);

	vsync_time = ktime_get();
	ctl->vsync_cnt++;

	spin_lock(&ctx->clk_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		if (tmp->enabled && !tmp->cmd_post_flush)
			tmp->vsync_handler(ctl, vsync_time);
	}

	if (!ctx->vsync_enabled && ctx->rdptr_enabled &&
		!atomic_read(&ctx->rt_active)) {
		if (ctx->need_wait)
			ctx->rdptr_enabled--;
		if (ctx->rdptr_enabled)
			ctx->rdptr_enabled--;
	}

	if (ctx->rdptr_enabled < 1)
		ctx->dbg_cnt = 1;

	if (ctx->dbg_cnt) {
		pr_info("mdss:%s:rd[%d]clk[%d]vs[%d]d[%d]\n",
			"pp_read", ctx->rdptr_enabled, ctx->clk_enabled,
			ctx->vsync_enabled, --ctx->dbg_cnt);
	}

	if (ctx->rdptr_enabled == 0) {
		mdss_mdp_irq_disable_nosync
			(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num);
		complete(&ctx->stop_comp);
		if (ctx->panel_on)
			schedule_work(&ctx->clk_work);
	}

	spin_unlock(&ctx->clk_lock);
}

static void mdss_mdp_cmd_underflow_recovery(void *data)
{
	struct mdss_mdp_cmd_ctx *ctx = data;
	unsigned long flags;

	if (!data) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	if (!ctx->ctl)
		return;
	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (ctx->koff_cnt) {
		mdss_mdp_ctl_reset(ctx->ctl);
		pr_info("%s: intf_num=%d\n", __func__,
					ctx->ctl->intf_num);
		ctx->koff_cnt--;
		mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_PING_PONG_COMP,
						ctx->pp_num);
		complete_all(&ctx->pp_comp);
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);
}

static void mdss_mdp_cmd_pingpong_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;
	struct mdss_mdp_vsync_handler *tmp;
	ktime_t vsync_time;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	mdss_mdp_ctl_perf_done(ctl);

	spin_lock(&ctx->clk_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		if (tmp->enabled && tmp->cmd_post_flush)
			tmp->vsync_handler(ctl, vsync_time);
	}
	mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);

	if (ctx->dbg_cnt)
		pr_info("mdss:%s:rd[%d]clk[%d]vs[%d]d[%d]\n",
			"pp_done", ctx->rdptr_enabled, ctx->clk_enabled,
			ctx->vsync_enabled, ctx->dbg_cnt);

	complete_all(&ctx->pp_comp);

	if (ctx->koff_cnt) {
		atomic_inc(&ctx->pp_done_cnt);
		schedule_work(&ctx->pp_done_work);
		ctx->koff_cnt--;
		if (ctx->koff_cnt) {
			pr_err("%s: too many kickoffs=%d!\n", __func__,
			       ctx->koff_cnt);
			ctx->koff_cnt = 0;
		}
	} else
		pr_err("%s: should not have pingpong interrupt!\n", __func__);

	pr_debug("%s: ctl_num=%d intf_num=%d ctx=%d kcnt=%d\n", __func__,
		ctl->num, ctl->intf_num, ctx->pp_num, ctx->koff_cnt);

	spin_unlock(&ctx->clk_lock);
}

static void pingpong_done_work(struct work_struct *work)
{
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), pp_done_work);

	if (ctx->ctl) {
		while (atomic_add_unless(&ctx->pp_done_cnt, -1, 0))
			mdss_mdp_ctl_notify(ctx->ctl, MDP_NOTIFY_FRAME_DONE);

		mdss_mdp_ctl_perf_release_bw(ctx->ctl);
	}
}

static void clk_ctrl_work(struct work_struct *work)
{
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), clk_work);

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	mdss_mdp_cmd_clk_off(ctx);
}

static int mdss_mdp_cmd_add_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&ctx->vsync_mtx);

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->vsync_handlers);
		if (!handle->cmd_post_flush)
			ctx->vsync_enabled = 1;
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (!handle->cmd_post_flush)
		mdss_mdp_cmd_clk_on(ctx);

	mutex_unlock(&ctx->vsync_mtx);

	return 0;
}

static int mdss_mdp_cmd_remove_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{

	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;
	struct mdss_mdp_vsync_handler *tmp;
	int num_rdptr_vsync = 0;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&ctx->vsync_mtx);

	spin_lock_irqsave(&ctx->clk_lock, flags);

	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);
	} else {
		spin_unlock_irqrestore(&ctx->clk_lock, flags);
		mutex_unlock(&ctx->vsync_mtx);
		return 0;
	}

	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		if (!tmp->cmd_post_flush)
			num_rdptr_vsync++;
	}

	if (!num_rdptr_vsync) {
		ctx->vsync_enabled = 0;
		ctx->rdptr_enabled = atomic_read(&ctx->rt_active) ?
			RT_ACT_EXPIRE_TICK : VSYNC_EXPIRE_TICK;
	}

	if (!ctx->vsync_enabled)
		pr_info("mdss:%s:rd[%d]clk[%d]vs[%d]\n",
			"rm_vs", ctx->rdptr_enabled, ctx->clk_enabled, ctx->vsync_enabled);

	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	mutex_unlock(&ctx->vsync_mtx);

	return 0;
}

int mdss_mdp_cmd_reconfigure_splash_done(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_panel_data *pdata;
	int ret = 0;
#ifndef CONFIG_SLP_KERNEL_ENG
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(ctl->mfd);
#endif

	pdata = ctl->panel_data;

	pdata->panel_info.cont_splash_enabled = 0;

	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_CLK_CTRL, (void *)0);

#ifndef CONFIG_SLP_KERNEL_ENG
	memblock_free(mdp5_data->splash_mem_addr, mdp5_data->splash_mem_size);
	free_bootmem_late(mdp5_data->splash_mem_addr,
				mdp5_data->splash_mem_size);
#endif

	return ret;
}

static int mdss_mdp_cmd_wait4pingpong(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;
	int need_wait = 0;
	int rc = 0;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (ctx->koff_cnt > 0)
		need_wait = 1;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	pr_debug("%s: need_wait=%d  intf_num=%d ctx=%p\n",
			__func__, need_wait, ctl->intf_num, ctx);

	if (need_wait) {
		rc = wait_for_completion_timeout(
				&ctx->pp_comp, KOFF_TIMEOUT);

		if (rc <= 0) {
			WARN(1, "cmd kickoff timed out (%d) ctl=%d\n",
						rc, ctl->num);
			rc = -EPERM;
			mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_TIMEOUT);
		} else {
			rc = 0;
		}
	}

	return rc;
}

static int mdss_mdp_cmd_set_partial_roi(struct mdss_mdp_ctl *ctl)
{
	int rc = 0;
	if (ctl->roi.w && ctl->roi.h && ctl->roi_changed &&
			ctl->panel_data->panel_info.partial_update_enabled) {
		ctl->panel_data->panel_info.roi_x = ctl->roi.x;
		ctl->panel_data->panel_info.roi_y = ctl->roi.y;
		ctl->panel_data->panel_info.roi_w = ctl->roi.w;
		ctl->panel_data->panel_info.roi_h = ctl->roi.h;

		rc = mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_ENABLE_PARTIAL_UPDATE, NULL);
	}
	return rc;
}

int mdss_mdp_cmd_kickoff(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;
	int rc;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (!get_lcd_attached()) {
		pr_err("%s : lcd is not attached..\n",__func__);
		return -ENODEV;
	}

	if (ctx->dbg_cnt)
		pr_info("mdss:%s:rd[%d]clk[%d]vs[%d]d[%d]\n",
			"cmd_kick", ctx->rdptr_enabled, ctx->clk_enabled,
			ctx->vsync_enabled, ctx->dbg_cnt);

	if (ctx->panel_on == 0) {
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_UNBLANK, NULL);
		WARN(rc, "intf %d unblank error (%d)\n", ctl->intf_num, rc);

		ctx->panel_on++;

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_ON, NULL);
		WARN(rc, "intf %d panel on error (%d)\n", ctl->intf_num, rc);
	}

	mdss_mdp_cmd_clk_on(ctx);

	mdss_mdp_cmd_set_partial_roi(ctl);

	/*
	 * tx dcs command if had any
	 */
	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_CMDLIST_KOFF,
						(void *)&ctx->recovery);

	INIT_COMPLETION(ctx->pp_comp);
	mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
	spin_lock_irqsave(&ctx->clk_lock, flags);
	ctx->koff_cnt++;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);
	mb();

	return 0;
}

int mdss_mdp_cmd_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;
	unsigned long flags;
	struct mdss_mdp_vsync_handler *tmp, *handle;
	int ret = 0;

	mutex_lock(&mdp_cmd_lock);

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		mutex_unlock(&mdp_cmd_lock);
		return -ENODEV;
	}

	pr_info("%s:rd[%d]clk[%d]vs[%d]ulps[%d]panel_on[%d]rt[%d]\n",
		__func__, ctx->rdptr_enabled, ctx->clk_enabled,
		ctx->vsync_enabled, ctx->ulps, ctx->panel_on,
		atomic_read(&ctx->rt_active));
	ctx->dbg_cnt = 4;

	if (!get_lcd_attached()) {
		pr_info("%s: lcd_not_attached\n", __func__);
		mutex_unlock(&mdp_cmd_lock);
		return 0;
	}

	list_for_each_entry_safe(handle, tmp, &ctx->vsync_handlers, list) {
		pr_info("%s:remove_vsync_handler\n", __func__);
		mdss_mdp_cmd_remove_vsync_handler(ctl, handle);
	}

	spin_lock_irqsave(&ctx->clk_lock, flags);
	ctx->panel_on = 0;

	if (ctx->rdptr_enabled && ctx->clk_enabled) {
		INIT_COMPLETION(ctx->stop_comp);
		ctx->need_wait = true;
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (ctx->need_wait) {
		ret = wait_for_completion_timeout(&ctx->stop_comp,
						STOP_TIMEOUT);
		if (ret <= 0) {
			WARN(1, "stop cmd time out\n");

			if (IS_ERR_OR_NULL(ctl->panel_data)) {
				pr_err("no panel data\n");
			} else {
				if (pinfo->panel_dead) {
					mdss_mdp_irq_disable
						(MDSS_MDP_IRQ_PING_PONG_RD_PTR,
								ctx->pp_num);
					ctx->rdptr_enabled = 0;
				}
			}
		}

		ctx->need_wait = false;
	}

	if (cancel_work_sync(&ctx->clk_work))
		pr_info("%s:deleted pending clk work\n", __func__);

	mdss_mdp_cmd_clk_off(ctx);

	flush_work(&ctx->pp_done_work);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num,
				   NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   NULL, NULL);

	if (mdss_dsi_ulps_feature_enabled(ctx->ctl->panel_data) &&
		ctx->ulps) {
		pr_info("%s:exit ulps\n", __func__);
		mdss_mdp_cmd_ulps_on(ctx, false);
	}

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK, NULL);
	WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF, NULL);
	WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);

	atomic_set(&ctx->rt_active, 0);
	mutex_destroy(&ctx->clk_mtx);
	mutex_destroy(&ctx->vsync_mtx);

	memset(ctx, 0, sizeof(*ctx));
	ctl->priv_data = NULL;

	ctl->stop_fnc = NULL;
	ctl->display_fnc = NULL;
	ctl->wait_pingpong = NULL;
	ctl->add_vsync_handler = NULL;
	ctl->remove_vsync_handler = NULL;
	ctl->ulps_trigger = NULL;

	pr_debug("%s:rd[%d]clk[%d]vs[%d]ulps[%d]panel_on[%d]done\n",
		__func__, ctx->rdptr_enabled, ctx->clk_enabled,
		ctx->vsync_enabled, ctx->ulps, ctx->panel_on);

	mutex_unlock(&mdp_cmd_lock);

	return 0;
}

int mdss_mdp_cmd_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_mdp_mixer *mixer;
	int i, ret;

	pr_debug("%s:+\n", __func__);

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (!mixer) {
		pr_err("mixer not setup correctly\n");
		return -ENODEV;
	}

	for (i = 0; i < MAX_SESSIONS; i++) {
		ctx = &mdss_mdp_cmd_ctx_list[i];
		if (ctx->ref_cnt == 0) {
			ctx->ref_cnt++;
			break;
		}
	}
	if (i == MAX_SESSIONS) {
		pr_err("too many sessions\n");
		return -ENOMEM;
	}

	ctl->priv_data = ctx;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	ctx->ctl = ctl;
	ctx->pp_num = mixer->num;
	init_completion(&ctx->pp_comp);
	init_completion(&ctx->stop_comp);
	spin_lock_init(&ctx->clk_lock);
	mutex_init(&ctx->clk_mtx);
	mutex_init(&ctx->vsync_mtx);
	INIT_WORK(&ctx->clk_work, clk_ctrl_work);
	INIT_WORK(&ctx->pp_done_work, pingpong_done_work);
	atomic_set(&ctx->pp_done_cnt, 0);
	atomic_set(&ctx->rt_active, 0);
	INIT_LIST_HEAD(&ctx->vsync_handlers);

	ctx->recovery.fxn = mdss_mdp_cmd_underflow_recovery;
	ctx->recovery.data = ctx;

	pr_debug("%s: ctx=%p num=%d mixer=%d\n", __func__,
				ctx, ctx->pp_num, mixer->num);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num,
				   mdss_mdp_cmd_readptr_done, ctl);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   mdss_mdp_cmd_pingpong_done, ctl);

	ret = mdss_mdp_cmd_tearcheck_setup(ctl, 1);
	if (ret) {
		pr_err("tearcheck setup failed\n");
		return ret;
	}

	ctl->stop_fnc = mdss_mdp_cmd_stop;
	ctl->display_fnc = mdss_mdp_cmd_kickoff;
	ctl->wait_pingpong = mdss_mdp_cmd_wait4pingpong;
	ctl->add_vsync_handler = mdss_mdp_cmd_add_vsync_handler;
	ctl->remove_vsync_handler = mdss_mdp_cmd_remove_vsync_handler;
	ctl->read_line_cnt_fnc = mdss_mdp_cmd_line_count;
	ctl->ulps_trigger = mdss_mdp_cmd_ulps_trigger;

	pr_debug("%s:rd[%d]clk[%d]vs[%d]ulps[%d]panel_on[%d]done\n",
		__func__, ctx->rdptr_enabled, ctx->clk_enabled,
		ctx->vsync_enabled, ctx->ulps, ctx->panel_on);

	return 0;
}

