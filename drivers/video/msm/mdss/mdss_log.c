/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include "mdss_log.h"

const char *mdss_log_get_cmd_str(u32 cmd)
{
	const char *cmd_str;

	switch (cmd) {
	/* Framebuffer */
	case FBIOGET_VSCREENINFO:
		cmd_str = "FBIOGET_VSCREENINFO";
		break;
	case FBIOPUT_VSCREENINFO:
		cmd_str = "FBIOPUT_VSCREENINFO";
		break;
	case FBIOGET_FSCREENINFO:
		cmd_str = "FBIOGET_FSCREENINFO";
		break;
	case FBIOGETCMAP:
		cmd_str = "FBIOGETCMAP";
		break;
	case FBIOPUTCMAP:
		cmd_str = "FBIOPUTCMAP";
		break;
	case FBIOPAN_DISPLAY:
		cmd_str = "FBIOPAN_DISPLAY";
		break;
	case FBIO_CURSOR:
		cmd_str = "FBIO_CURSOR";
		break;
	case FBIOGET_CON2FBMAP:
		cmd_str = "FBIOGET_CON2FBMAP";
		break;
	case FBIOPUT_CON2FBMAP:
		cmd_str = "FBIOPUT_CON2FBMAP";
		break;
	case FBIOBLANK:
		cmd_str = "FBIOBLANK";
		break;
	case FBIOGET_VBLANK:
		cmd_str = "FBIOGET_VBLANK";
		break;
	case FBIO_ALLOC:
		cmd_str = "FBIO_ALLOC";
		break;
	case FBIO_FREE:
		cmd_str = "FBIO_FREE";
		break;
	case FBIOGET_GLYPH:
		cmd_str = "FBIOGET_GLYPH";
		break;
	case FBIOGET_HWCINFO:
		cmd_str = "FBIOGET_HWCINFO";
		break;
	case FBIOPUT_MODEINFO:
		cmd_str = "FBIOPUT_MODEINFO";
		break;
	case FBIOGET_DISPINFO:
		cmd_str = "FBIOGET_DISPINFO";
		break;
	case FBIO_WAITFORVSYNC:
		cmd_str = "FBIO_WAITFORVSYNC";
		break;
	/* Normal */
	case MSMFB_GRP_DISP:
		cmd_str = "MSMFB_GRP_DISP";
		break;
	case MSMFB_BLIT:
		cmd_str = "MSMFB_BLIT";
		break;
	case MSMFB_SUSPEND_SW_REFRESHER:
		cmd_str = "MSMFB_SUSPEND_SW_REFRESHER";
		break;
	case MSMFB_RESUME_SW_REFRESHER:
		cmd_str = "MSMFB_RESUME_SW_REFRESHER";
		break;
	case MSMFB_CURSOR:
		cmd_str = "MSMFB_CURSOR";
		break;
	case MSMFB_SET_LUT:
		cmd_str = "MSMFB_SET_LUT";
		break;
	case MSMFB_HISTOGRAM:
		cmd_str = "MSMFB_HISTOGRAM";
		break;
	case MSMFB_HISTOGRAM_START:
		cmd_str = "MSMFB_HISTOGRAM_START";
		break;
	case MSMFB_HISTOGRAM_STOP:
		cmd_str = "MSMFB_HISTOGRAM_STOP";
		break;
	case MSMFB_MDP_PP:
		cmd_str = "MSMFB_MDP_PP";
		break;
	case MSMFB_GET_CCS_MATRIX:
		cmd_str = "MSMFB_GET_CCS_MATRIX";
		break;
	case MSMFB_SET_CCS_MATRIX:
		cmd_str = "MSMFB_SET_CCS_MATRIX";
		break;
	case MSMFB_GET_PAGE_PROTECTION:
		cmd_str = "MSMFB_GET_PAGE_PROTECTION";
		break;
	case MSMFB_SET_PAGE_PROTECTION:
		cmd_str = "MSMFB_SET_PAGE_PROTECTION";
		break;
	case MSMFB_NOTIFY_UPDATE:
		cmd_str = "MSMFB_NOTIFY_UPDATE";
		break;
	case MSMFB_MIXER_INFO:
		cmd_str = "MSMFB_MIXER_INFO";
		break;
	case MSMFB_DISPLAY_COMMIT:
		cmd_str = "MSMFB_DISPLAY_COMMIT";
		break;
	case MSMFB_OVERLAY_VSYNC_CTRL:
		cmd_str = "MSMFB_OVERLAY_VSYNC_CTRL";
		break;
	case MSMFB_VSYNC_CTRL:
		cmd_str = "MSMFB_VSYNC_CTRL";
		break;
	case MSMFB_BUFFER_SYNC:
		cmd_str = "MSMFB_BUFFER_SYNC";
		break;
	case MSMFB_METADATA_SET:
		cmd_str = "MSMFB_METADATA_SET";
		break;
	case MSMFB_METADATA_GET:
		cmd_str = "MSMFB_METADATA_GET";
		break;
	case MSMFB_ASYNC_BLIT:
		cmd_str = "MSMFB_ASYNC_BLIT";
		break;
	/* Overlay */
	case MSMFB_OVERLAY_SET:
		cmd_str = "MSMFB_OVERLAY_SET";
		break;
	case MSMFB_OVERLAY_UNSET:
		cmd_str = "MSMFB_OVERLAY_UNSET";
		break;
	case MSMFB_OVERLAY_PLAY:
	/* case MSMFB_OVERLAY_QUEUE: */
		cmd_str = "MSMFB_OVERLAY_PLAY";
		break;
	case MSMFB_OVERLAY_GET:
		cmd_str = "MSMFB_OVERLAY_GET";
		break;
	case MSMFB_OVERLAY_PLAY_ENABLE:
		cmd_str = "MSMFB_OVERLAY_PLAY_ENABLE";
		break;
	case MSMFB_OVERLAY_BLT:
		cmd_str = "MSMFB_OVERLAY_BLT";
		break;
	case MSMFB_OVERLAY_BLT_OFFSET:
		cmd_str = "MSMFB_OVERLAY_BLT_OFFSET";
		break;
	case MSMFB_OVERLAY_3D:
		cmd_str = "MSMFB_OVERLAY_3D";
		break;
	case MSMFB_OVERLAY_PLAY_WAIT:
		cmd_str = "MSMFB_OVERLAY_PLAY_WAIT";
		break;
	case MSMFB_OVERLAY_COMMIT:
		cmd_str = "MSMFB_OVERLAY_COMMIT";
		break;
	/* Writeback */
	case MSMFB_WRITEBACK_INIT:
		cmd_str = "MSMFB_WRITEBACK_INIT";
		break;
	case MSMFB_WRITEBACK_START:
		cmd_str = "MSMFB_WRITEBACK_START";
		break;
	case MSMFB_WRITEBACK_STOP:
		cmd_str = "MSMFB_WRITEBACK_STOP";
		break;
	case MSMFB_WRITEBACK_QUEUE_BUFFER:
		cmd_str = "MSMFB_WRITEBACK_QUEUE_BUFFER";
		break;
	case MSMFB_WRITEBACK_DEQUEUE_BUFFER:
		cmd_str = "MSMFB_WRITEBACK_DEQUEUE_BUFFER";
		break;
	case MSMFB_WRITEBACK_TERMINATE:
		cmd_str = "MSMFB_WRITEBACK_TERMINATE";
		break;
	case MSMFB_WRITEBACK_SET_MIRRORING_HINT:
		cmd_str = "MSMFB_WRITEBACK_SET_MIRRORING_HINT";
		break;
	default:
		cmd_str = "**Invalid command**";
		break;
	}

	return cmd_str;
}
