/* include/linux/wakelock.h
 *
 * Copyright (C) 2014 SAMSUNG, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_PM_SLAVE_WAKELOCKS_H
#define _LINUX_PM_SLAVE_WAKELOCKS_H

#include <linux/ctype.h>
#include <linux/device.h>
 #include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#ifdef CONFIG_PM_SLAVE_WAKELOCKS
struct slave_wakelock {
	char			*name;
	struct list_head	node;
	/* not used */
	spinlock_t		lock;
	struct timer_list	timer;
	unsigned long		timer_expires;
	/* not used */
	ktime_t total_time;
	ktime_t max_time;
	ktime_t last_time;
	ktime_t start_prevent_time;
	ktime_t prevent_sleep_time;
	ktime_t prevent_time;
	unsigned long		active_count;
	bool			active:1;
};

struct wakelock {
	char			*name;
	struct rb_node		node;
	struct wakeup_source	ws;
#ifdef CONFIG_PM_WAKELOCKS_GC
	struct list_head	lru;
#endif
#ifdef CONFIG_PM_SLAVE_WAKELOCKS
	struct list_head	slave_wl_list;
#if CONFIG_PM_SLAVE_WAKELOCKS_LIMIT > 0
	unsigned int	number_of_slave_wakelocks;
#endif
#endif
};

extern bool pm_get_last_slave_wakelocks(struct wakeup_source *ws,
			struct slave_wakelock **last_slave_wl_array, int slave_wl_count);
extern bool pm_get_last_slave_wakelock(struct wakeup_source *ws,
			struct slave_wakelock **last_slave_wl);
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */
#endif /* _LINUX_PM_SLAVE_WAKELOCKS_H */
