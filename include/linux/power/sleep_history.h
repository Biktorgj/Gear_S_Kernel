/*
 * debugfs file to keep track of suspend
 *
 * Copyright (C) 2012 SAMSUNG, Inc.
 * Junho Jang <vincent.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */


#ifndef _SLEEP_HISTORY_H
#define _SLEEP_HISTORY_H
#include <linux/time.h>
#include <linux/ktime.h>
#ifdef CONFIG_PM_SLAVE_WAKELOCKS
#include <linux/pm_slave_wakelocks.h>
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */

enum sleep_history_type {
	SLEEP_HISTORY_NONE,
#ifdef CONFIG_PM_AUTOSLEEP
	SLEEP_HISTORY_AUTOSLEEP_ENTRY,
	SLEEP_HISTORY_AUTOSLEEP_EXIT,
#endif
	SLEEP_HISTORY_SUSPEND_SYSTEM_ENTRY,
	SLEEP_HISTORY_SUSPEND_SYSTEM_EXIT,
	SLEEP_HISTORY_SUSPEND_PERSISTCLOCK_ENTRY,
	SLEEP_HISTORY_SUSPEND_PERSISTCLOCK_EXIT,
	SLEEP_HISTORY_SUSPEND_RTC_ENTRY,
	SLEEP_HISTORY_SUSPEND_RTC_EXIT,
	SLEEP_HISTORY_WAKEUP_IRQ,
	SLEEP_HISTORY_TYPEY_MAX
};

extern int sleep_history_marker(int type, struct timespec *ts, void *wakeup);
#endif /* _SLEEP_HISTORY_H */
