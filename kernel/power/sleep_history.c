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

#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/circ_buf.h>
#include <linux/rtc.h>
#include <linux/irq.h>
#include <linux/power_supply.h>
#include <linux/power/sleep_history.h>
#include <linux/suspend.h>
#ifdef CONFIG_MSM_SMD
#include <mach/msm_smd.h>
#endif
#ifdef CONFIG_MSM_RPM_STATS_LOG
#include <../arch/arm/mach-msm/rpm_stats.h>
#endif

#undef USE_SUSPEND_SYSTEM_TIME
#undef USE_SUSPEND_PERSISTCLOCK_TIME
#define USE_SUSPEND_RTC_TIME

#define WS_ARRAY_MAX 10
#define SLEEP_HISTORY_RINGBUFFER_SIZE 2048
#define sleep_history_ring_incr(n, s)	(((n) + 1) & ((s) - 1))
#define sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr) \
{\
	if (ts) {	\
		ts_ptr = &((ring_buf_ptr + *head_ptr)->ts);	\
		memcpy(ts_ptr, ts, sizeof(struct timespec));	\
		rtc_time_to_tm(ts->tv_sec, &tm);	\
		pr_cont("t:%d/%d/%d/%d/%d/%d ",	\
			tm.tm_year, tm.tm_mon,	\
			tm.tm_mday, tm.tm_hour,	\
			tm.tm_min, tm.tm_sec);	\
	}	\
}
#define sleep_history_set_type(type, ring_buf_ptr, head_ptr) (ring_buf_ptr + *head_ptr)->type = type

#ifdef CONFIG_PM_AUTOSLEEP
struct autosleep_ws {
	struct wakeup_source *ws;
	ktime_t prevent_time;
};
#endif

union wakeup_history {
	unsigned int irq;
#ifdef CONFIG_PM_AUTOSLEEP
	struct autosleep_ws ws;
#endif
};

struct battery_history {
	int status;
	int capacity;
};

struct sleep_history {
	int type;
	struct timespec ts;
	union wakeup_history ws;
	struct battery_history battery;
	int suspend_count;
	enum suspend_stat_step failed_step;
#ifdef CONFIG_MSM_RPM_STATS_LOG
	union msm_rpm_sleep_time rpm;
#endif
};

struct sleep_history_data {
	struct circ_buf sleep_history;
};

static struct sleep_history_data sleep_history_data;

static char *type_text[] = {
	"on", "autosleep", "autosleep", "suspend", "suspend",
	"suspend", "suspend", "suspend", "suspend", "irq"
};

static char *suspend_text[] = {
	"failed", "freeze", "prepare", "suspend",
	 "suspend_noirq", "resume_noirq", "resume"
};

static struct suspend_stats suspend_stats_bkup;

#ifdef CONFIG_DEBUG_FS
static int print_sleep_history_time(
	struct seq_file *s, struct timespec *entry_ts, struct timespec *exit_ts)
{
	struct timespec delta;
	struct rtc_time entry_tm, exit_tm;

	if (!s)
		return -EINVAL;

	if (entry_ts)
		rtc_time_to_tm(entry_ts->tv_sec, &entry_tm);
	else
		rtc_time_to_tm(0, &entry_tm);
	if (exit_ts)
		rtc_time_to_tm(exit_ts->tv_sec, &exit_tm);
	else
		rtc_time_to_tm(0, &exit_tm);
	if (entry_ts && exit_ts)
		delta = timespec_sub(*exit_ts, *entry_ts);

	seq_printf(s,
		"%04d-%02d-%02d/%02d:%02d:%02d "
		"%04d-%02d-%02d/%02d:%02d:%02d "
		"%7d ",
		entry_tm.tm_year + 1900, entry_tm.tm_mon + 1,
		entry_tm.tm_mday, entry_tm.tm_hour,
		entry_tm.tm_min, entry_tm.tm_sec,
		exit_tm.tm_year + 1900, exit_tm.tm_mon + 1,
		exit_tm.tm_mday, exit_tm.tm_hour,
		exit_tm.tm_min, exit_tm.tm_sec,
		(int)delta.tv_sec);

	return 0;
}

static int print_sleep_history_battery(
	struct seq_file *s, struct battery_history *entry, struct battery_history *exit)
{
	int capacity_delta = -1;

	if (!s || !entry || !exit)
		return -EINVAL;

	if (entry->capacity  != -1 && exit->capacity  != -1)
		capacity_delta = exit->capacity - entry->capacity;
	seq_printf(s, "%3d %3d %3d %d %d   ",
		entry->capacity, exit->capacity, capacity_delta,
		entry->status, exit->status);

	return 0;
}

static int copy_sleep_history(struct sleep_history *buf, int count)
{
	int c, err;
	int head, tail;
	struct sleep_history *ring_buf;

	if (!buf) {
		err = -EINVAL;
		goto err_invalid;
	}
	memset(buf, 0, count * sizeof(struct sleep_history));

	ring_buf = (struct sleep_history *)(sleep_history_data.sleep_history.buf);
	head = sleep_history_data.sleep_history.head;
	tail = sleep_history_data.sleep_history.tail;

	c = CIRC_CNT(head, tail, SLEEP_HISTORY_RINGBUFFER_SIZE);
	if (c == 0) {
		err = -EINVAL;
		goto err_read;
	} else  if (c == SLEEP_HISTORY_RINGBUFFER_SIZE - 1) {
		c = CIRC_CNT_TO_END(head, tail, SLEEP_HISTORY_RINGBUFFER_SIZE);
		memcpy(buf, ring_buf + tail, c * sizeof(struct sleep_history));
		memcpy(buf + c, ring_buf,
			(SLEEP_HISTORY_RINGBUFFER_SIZE-c) * sizeof(struct sleep_history));
		c = SLEEP_HISTORY_RINGBUFFER_SIZE - 1;
	} else
		memcpy(buf, ring_buf + tail, c * sizeof(struct sleep_history));

	return c;

err_invalid:
err_read:
	return err;
}

static int sleep_history_debug_show(struct seq_file *s, void *data)
{
	int i, j, index, wakeup_count = 0, err = -ENODEV;
	struct sleep_history *sleep_history = 0;
	struct timespec *entry_ts = 0, *exit_ts = 0;
	struct battery_history batt_entry, batt_exit;
	struct irq_desc *desc = 0;
	union wakeup_history *wakeup[WS_ARRAY_MAX];
#ifdef CONFIG_MSM_RPM_STATS_LOG
	union msm_rpm_sleep_time rpm ;
#endif

	sleep_history = (struct sleep_history *)vmalloc(sizeof(struct sleep_history)
			* SLEEP_HISTORY_RINGBUFFER_SIZE);
	if (!sleep_history) {
		err =  -ENOMEM;
		goto err_invalid;
	}

	err = copy_sleep_history(sleep_history, SLEEP_HISTORY_RINGBUFFER_SIZE);
	if (err < 0) {
		pr_err("%s: unable to read  sleep history\n", __func__);
		goto err_read;
	}

	seq_printf(s, "    type      count     entry time          ");
	seq_printf(s, "exit time           ");
	seq_printf(s, "diff      ");
#ifdef CONFIG_MSM_RPM_STATS_LOG
	seq_printf(s,	"                  ");
#endif
	seq_printf(s, "battery           wakeup source\n");
	seq_printf(s, "--- --------- --------- ------------------- ");
	seq_printf(s, "------------------- ");
#ifdef CONFIG_MSM_RPM_STATS_LOG
	seq_printf(s,	"----------------");
#endif
	seq_printf(s, "------- ");
	seq_printf(s, "----------------  ----------------------\n");

	for (i = 0, index = 1; i < SLEEP_HISTORY_RINGBUFFER_SIZE-1; i ++) {
#ifdef CONFIG_PM_AUTOSLEEP
		if ((sleep_history + i)->type == SLEEP_HISTORY_AUTOSLEEP_ENTRY) {
			/* autosleep state */
			seq_printf(s, "%3d %9s           ",
				index++, type_text[(sleep_history + i)->type]);

			entry_ts = &(sleep_history + i)->ts;
			batt_entry.status = (sleep_history + i)->battery.status;
			batt_entry.capacity = (sleep_history + i)->battery.capacity;
			if (!(++i < SLEEP_HISTORY_RINGBUFFER_SIZE-1))
				break;

			exit_ts = &(sleep_history + i)->ts;
			print_sleep_history_time(s, entry_ts, exit_ts);
#ifdef CONFIG_MSM_RPM_STATS_LOG
			seq_printf(s,	"                ");
#endif

			batt_exit.status = (sleep_history + i)->battery.status;
			batt_exit.capacity = (sleep_history + i)->battery.capacity;
			print_sleep_history_battery(s, &batt_entry, &batt_exit);

			wakeup_count = 0;
			memset(wakeup, 0, sizeof(wakeup));
			wakeup[wakeup_count] = &((sleep_history + i)->ws);
			if (wakeup[wakeup_count]->ws.ws)
				seq_printf(s, "%s:%lld ", wakeup[wakeup_count]->ws.ws->name,
							ktime_to_ms(wakeup[wakeup_count]->ws.prevent_time));
			wakeup_count++;

			do {
				if (!(++i < SLEEP_HISTORY_RINGBUFFER_SIZE-1))
					break;
				if ((sleep_history + i)->type == SLEEP_HISTORY_AUTOSLEEP_EXIT) {
					wakeup[wakeup_count] = &((sleep_history + i)->ws);
					if (wakeup[wakeup_count]->ws.ws)
						seq_printf(s, "%s:%lld ", wakeup[wakeup_count]->ws.ws->name,
							ktime_to_ms(wakeup[wakeup_count]->ws.prevent_time));
					wakeup_count++;
				} else {
					i--;
					break;
				}
			} while (wakeup_count < WS_ARRAY_MAX);
			seq_printf(s, "\n");

			/* suspend on */
			entry_ts = exit_ts;
			batt_entry.status = batt_exit.status;
			batt_entry.capacity = batt_exit.capacity;
			if (!(++i < SLEEP_HISTORY_RINGBUFFER_SIZE-1))
				break;
			if ((sleep_history + i)->type == SLEEP_HISTORY_AUTOSLEEP_ENTRY) {
				if ((sleep_history + i)->failed_step > 0)
					seq_printf(s, "%3d %9s %9s ", index++, suspend_text[0],
							suspend_text[(sleep_history + i)->failed_step]);
				 else
					seq_printf(s, "%3d %9s           ", index++, type_text[0]);

				exit_ts = &(sleep_history +  i)->ts;
				print_sleep_history_time(s, entry_ts, exit_ts);

				batt_exit.status = (sleep_history + i)->battery.status;
				batt_exit.capacity = (sleep_history + i)->battery.capacity;
				print_sleep_history_battery(s, &batt_entry, &batt_exit);

				seq_printf(s, "\n");
			}
			i--;
		}else
#endif
		if ((sleep_history + i)->type == SLEEP_HISTORY_SUSPEND_SYSTEM_ENTRY ||
			(sleep_history + i)->type == SLEEP_HISTORY_SUSPEND_PERSISTCLOCK_ENTRY ||
			(sleep_history + i)->type == SLEEP_HISTORY_SUSPEND_RTC_ENTRY) {
			/* suspend mem */
			entry_ts = &(sleep_history + i)->ts;
#ifdef CONFIG_MSM_RPM_STATS_LOG
			memset(&rpm, 0, sizeof(union msm_rpm_sleep_time));
			if ((sleep_history + i)->rpm.v2.version == 2) {
				rpm.v2.xosd = (sleep_history + i)->rpm.v2.xosd;
				rpm.v2.vmin= (sleep_history + i)->rpm.v2.vmin;
			} else if ((sleep_history + i)->rpm.v1.version == 1)
				rpm.v1.sleep= (sleep_history + i)->rpm.v1.sleep;
#endif
			batt_entry.status = (sleep_history + i)->battery.status;
			batt_entry.capacity = (sleep_history + i)->battery.capacity;

			wakeup_count = 0;
			memset(wakeup, 0, sizeof(wakeup));
			do {
				if (!(++i < SLEEP_HISTORY_RINGBUFFER_SIZE-1))
					goto end_ring_buf;
				if ((sleep_history + i)->type == SLEEP_HISTORY_WAKEUP_IRQ)
					wakeup[wakeup_count++] = &((sleep_history + i)->ws);
				else
					break;
			} while (wakeup_count < WS_ARRAY_MAX);

			if ((sleep_history + i)->failed_step > 0)
				seq_printf(s, "%3d %9s %9s ", index++, suspend_text[0],
							suspend_text[(sleep_history + i)->failed_step]);
			 else
				seq_printf(s, "%3d %9s %9d ", index++,
							type_text[(sleep_history + i)->type],
							(sleep_history + i)->suspend_count);

			exit_ts = &(sleep_history + i)->ts;
			print_sleep_history_time(s, entry_ts, exit_ts);
#ifdef CONFIG_MSM_RPM_STATS_LOG
			if ((sleep_history + i)->rpm.v2.version == 2) {
				seq_printf(s,	"%7d %7d ",
					(sleep_history + i)->rpm.v2.xosd - rpm.v2.xosd,
					(sleep_history + i)->rpm.v2.vmin - rpm.v2.vmin);
			} else if ((sleep_history + i)->rpm.v1.version == 1)
				seq_printf(s,	"%7d         ",
					(sleep_history + i)->rpm.v1.sleep - rpm.v1.sleep);
			else
				seq_printf(s,	"				 ");
#endif
			batt_exit.status = (sleep_history + i)->battery.status;
			batt_exit.capacity = (sleep_history + i)->battery.capacity;
			print_sleep_history_battery(s, &batt_entry, &batt_exit);

			for (j  = 0; j < wakeup_count; j++) {
				if (wakeup[j]) {
					if (wakeup[j]->irq != NR_IRQS) {
						desc = irq_to_desc(wakeup[j]->irq);
						if (desc && desc->action && desc->action->name) {
#ifdef CONFIG_MSM_SMD
							const char *msm_subsys;

							msm_subsys = smd_irq_to_subsystem(wakeup[j]->irq);
							if (msm_subsys)
								seq_printf(s, "%d,%s:%s/ ",
									wakeup[j]->irq, desc->action->name,
									msm_subsys);
							else
#endif
							seq_printf(s, "%d,%s/ ",
								wakeup[j]->irq, desc->action->name);
						}
					} else
						seq_printf(s, "%d, NR_IRQS", wakeup[j]->irq);
				}
			}
end_ring_buf:
			seq_printf(s, "\n");
		}
	}

	vfree(sleep_history);

	return 0;

err_read:
err_invalid:
	vfree(sleep_history);

	return err;
}

static int sleep_history_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sleep_history_debug_show, NULL);
}

static const struct file_operations sleep_history_debug_fops = {
	.open		= sleep_history_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init sleep_history_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("sleep_history", 0700, NULL, NULL,
		&sleep_history_debug_fops);
	if (!d) {
		pr_err("Failed to create sleep_history debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(sleep_history_debug_init);
#endif

int sleep_history_marker(int type, struct timespec *ts, void *wakeup)
{
	int err;
	int *head_ptr, *tail_ptr;
	struct sleep_history *ring_buf_ptr;
	struct timespec *ts_ptr;
	union wakeup_history *wakeup_ptr;
	struct rtc_time tm;
	struct power_supply *psy_battery = 0;
	union power_supply_propval value;
#ifdef CONFIG_PM_SLAVE_WAKELOCKS
	struct slave_wakelock *last_slave_wl[4];
	int j;
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */

	if (type >= SLEEP_HISTORY_TYPEY_MAX  || (!ts && !wakeup))
		return -EINVAL;

	ring_buf_ptr = (struct sleep_history *)(sleep_history_data.sleep_history.buf);
	head_ptr = &sleep_history_data.sleep_history.head;
	tail_ptr =	&sleep_history_data.sleep_history.tail;
	memset((ring_buf_ptr + *head_ptr), 0, sizeof(struct sleep_history));

	sleep_history_set_type(type, ring_buf_ptr, head_ptr);
	psy_battery = power_supply_get_by_name("battery");
	if (psy_battery) {
		err = psy_battery->get_property(psy_battery,
				POWER_SUPPLY_PROP_STATUS, &value);
		if (err < 0)
			value.intval = -1;
		(ring_buf_ptr + *head_ptr)->battery.status = value.intval;

		err = psy_battery->get_property(psy_battery,
				POWER_SUPPLY_PROP_CAPACITY, &value);
		if (err < 0)
			value.intval = -1;
		(ring_buf_ptr + *head_ptr)->battery.capacity = value.intval;
	}

	switch (type) {
#ifdef CONFIG_PM_AUTOSLEEP
	case SLEEP_HISTORY_AUTOSLEEP_ENTRY:
		pr_info("marker: %d: ", type);
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);

		if (wakeup) {
			wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
			wakeup_ptr->ws.ws = (struct wakeup_source *)wakeup;
			wakeup_ptr->ws.prevent_time =
				((struct wakeup_source *)wakeup)->prevent_time;
			pr_cont("ws:%s/%lld ", wakeup_ptr->ws.ws->name,
				ktime_to_ms(wakeup_ptr->ws.prevent_time));
		}

		if (suspend_stats_bkup.fail != suspend_stats.fail) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			(ring_buf_ptr + *head_ptr)->failed_step =  suspend_stats.failed_steps[last_step];
			pr_cont("previous suspend failed(%d)!!! ", suspend_stats.failed_steps[last_step]);
		}
		break;

	case SLEEP_HISTORY_AUTOSLEEP_EXIT:
		memcpy(&suspend_stats_bkup, &suspend_stats, sizeof(struct suspend_stats));

		pr_info("marker: %d: ", type);
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);

		if (wakeup) {
			wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
			wakeup_ptr->ws.ws = (struct wakeup_source *)wakeup;
			wakeup_ptr->ws.prevent_time =
				((struct wakeup_source *)wakeup)->prevent_time;
			pr_cont("ws:%s/%lld ", wakeup_ptr->ws.ws->name,
				ktime_to_ms(wakeup_ptr->ws.prevent_time));

#ifdef CONFIG_PM_SLAVE_WAKELOCKS
			memset(last_slave_wl, 0, sizeof(last_slave_wl));
			if (pm_get_last_slave_wakelocks(wakeup, &last_slave_wl[0], 4)) {
				for (j = 0;  last_slave_wl[j] && j < 4; j++) {
					if (CIRC_SPACE(*head_ptr, *tail_ptr, SLEEP_HISTORY_RINGBUFFER_SIZE) == 0)
						*tail_ptr = sleep_history_ring_incr(*tail_ptr,
									SLEEP_HISTORY_RINGBUFFER_SIZE);
					*head_ptr = sleep_history_ring_incr(*head_ptr,
									SLEEP_HISTORY_RINGBUFFER_SIZE);

					ring_buf_ptr = (struct sleep_history *)(sleep_history_data.sleep_history.buf);
					head_ptr = &sleep_history_data.sleep_history.head;
					tail_ptr =	&sleep_history_data.sleep_history.tail;
					memset((ring_buf_ptr + *head_ptr), 0, sizeof(struct sleep_history));
					sleep_history_set_type(type, ring_buf_ptr, head_ptr);

					wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
					wakeup_ptr->ws.ws = (struct wakeup_source *)last_slave_wl[j];
					wakeup_ptr->ws.prevent_time =
						last_slave_wl[j]->prevent_time;

					pr_cont("swl=%s/%lld ", last_slave_wl[j]->name,
						ktime_to_ms(last_slave_wl[j]->prevent_time));
				}
			}
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */
		}
		break;
#endif

#ifdef USE_SUSPEND_SYSTEM_TIME
	case SLEEP_HISTORY_SUSPEND_SYSTEM_EXIT:
		if (suspend_stats_bkup.fail != suspend_stats.fail ||
			suspend_stats_bkup.failed_prepare != suspend_stats.failed_prepare ||
			suspend_stats_bkup.failed_suspend != suspend_stats.failed_suspend ||
			suspend_stats_bkup.failed_suspend_late != suspend_stats.failed_suspend_late ||
			suspend_stats_bkup.failed_suspend_noirq != suspend_stats.failed_suspend_noirq) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			(ring_buf_ptr + *head_ptr)->failed_step =
				suspend_stats.failed_steps[last_step];
			pr_info("marker: %d: suspend failed(%d)!!! ",
					type, suspend_stats.failed_steps[last_step]);
		} else {
			(ring_buf_ptr + *head_ptr)->suspend_count =
				suspend_stats.success + 1;
			pr_info("marker: %d: count:%d ",
					type, (ring_buf_ptr + *head_ptr)->suspend_count);
		}
		/* fall through */
	case SLEEP_HISTORY_SUSPEND_SYSTEM_ENTRY:
#endif
#ifdef USE_SUSPEND_PERSISTCLOCK_TIME
	case SLEEP_HISTORY_SUSPEND_PERSISTCLOCK_EXIT:
		if (suspend_stats_bkup.fail != suspend_stats.fail ||
			suspend_stats_bkup.failed_prepare != suspend_stats.failed_prepare ||
			suspend_stats_bkup.failed_suspend != suspend_stats.failed_suspend ||
			suspend_stats_bkup.failed_suspend_late != suspend_stats.failed_suspend_late ||
			suspend_stats_bkup.failed_suspend_noirq != suspend_stats.failed_suspend_noirq) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			(ring_buf_ptr + *head_ptr)->failed_step =
				suspend_stats.failed_steps[last_step];
			pr_info("marker: %d: suspend failed(%d)!!! ",
					type, suspend_stats.failed_steps[last_step]);
		} else {
			(ring_buf_ptr + *head_ptr)->suspend_count =
				suspend_stats.success + 1;
			pr_info("marker: %d: count:%d ",
					type, (ring_buf_ptr + *head_ptr)->suspend_count);
		}
		/* fall through */
	case SLEEP_HISTORY_SUSPEND_PERSISTCLOCK_ENTRY:
#endif
	case SLEEP_HISTORY_WAKEUP_IRQ:
		pr_info("marker: %d: ", type);
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);

		if (wakeup) {
			wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
			wakeup_ptr->irq = *((unsigned int *)wakeup);
			pr_cont("ws:%d ", wakeup_ptr->irq);
		}
		break;
#ifdef USE_SUSPEND_RTC_TIME
	case SLEEP_HISTORY_SUSPEND_RTC_ENTRY:
		pr_info("marker: %d: ", type);
		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);
#ifdef CONFIG_MSM_RPM_STATS_LOG
		mms_rpmstats_get_total_sleep_time(&((ring_buf_ptr + *head_ptr)->rpm));
		if ((ring_buf_ptr + *head_ptr)->rpm.v1.version == 1)
			pr_cont("rt:%d ", (ring_buf_ptr + *head_ptr)->rpm.v1.sleep);
		else if ((ring_buf_ptr + *head_ptr)->rpm.v2.version == 2)
			pr_cont("rt:%d/%d ", (ring_buf_ptr + *head_ptr)->rpm.v2.xosd,
					(ring_buf_ptr + *head_ptr)->rpm.v2.vmin);
#endif
		break;
	case SLEEP_HISTORY_SUSPEND_RTC_EXIT:
		if (suspend_stats_bkup.fail != suspend_stats.fail ||
			suspend_stats_bkup.failed_prepare != suspend_stats.failed_prepare ||
			suspend_stats_bkup.failed_suspend != suspend_stats.failed_suspend ||
			suspend_stats_bkup.failed_suspend_late != suspend_stats.failed_suspend_late ||
			suspend_stats_bkup.failed_suspend_noirq != suspend_stats.failed_suspend_noirq) {
			int last_step;
			last_step = suspend_stats.last_failed_step + REC_FAILED_NUM - 1;
			last_step %= REC_FAILED_NUM;
			(ring_buf_ptr + *head_ptr)->failed_step =
				suspend_stats.failed_steps[last_step];
			pr_info("marker: %d: suspend failed(%d)!!! ",
					type, suspend_stats.failed_steps[last_step]);
		} else {
			(ring_buf_ptr + *head_ptr)->suspend_count =
				suspend_stats.success + 1;
			pr_info("marker: %d: count:%d ",
					type, (ring_buf_ptr + *head_ptr)->suspend_count);
		}
 		sleep_history_set_ts(ts, ts_ptr, ring_buf_ptr, head_ptr);

		if (wakeup) {
			wakeup_ptr = &((ring_buf_ptr + *head_ptr)->ws);
			wakeup_ptr->irq = *((unsigned int *)wakeup);
			pr_cont("ws:%d ", wakeup_ptr->irq);
		}
#ifdef CONFIG_MSM_RPM_STATS_LOG
		mms_rpmstats_get_total_sleep_time(&((ring_buf_ptr + *head_ptr)->rpm));
		if ((ring_buf_ptr + *head_ptr)->rpm.v1.version == 1)
			pr_cont("rt:%d ", (ring_buf_ptr + *head_ptr)->rpm.v1.sleep);
		else if ((ring_buf_ptr + *head_ptr)->rpm.v2.version == 2)
			pr_cont("rt:%d/%d ", (ring_buf_ptr + *head_ptr)->rpm.v2.xosd,
					(ring_buf_ptr + *head_ptr)->rpm.v2.vmin);
#endif
		break;
#endif
	default:
		return -EPERM;
	}

	if (psy_battery)

		pr_cont("b:%d/%d\n",(ring_buf_ptr + *head_ptr)->battery.status,
			(ring_buf_ptr + *head_ptr)->battery.capacity);
	else
		pr_cont("\n");

	if (CIRC_SPACE(*head_ptr, *tail_ptr, SLEEP_HISTORY_RINGBUFFER_SIZE) == 0)
		*tail_ptr = sleep_history_ring_incr(*tail_ptr,
					SLEEP_HISTORY_RINGBUFFER_SIZE);
	*head_ptr = sleep_history_ring_incr(*head_ptr,
					SLEEP_HISTORY_RINGBUFFER_SIZE);

	return 0;
}

#ifdef USE_SUSPEND_PERSISTCLOCK_TIME
static int sleep_history_syscore_suspend(void)
{
	struct timespec ts;

	/* snapshot the current time at suspend */
	read_persistent_clock(&ts);
	if (ts.tv_sec == 0 && ts.tv_nsec == 0)
		return 0;

	sleep_history_marker(SLEEP_HISTORY_SUSPEND_PERSISTCLOCK_ENTRY, &ts, NULL);

	return 0;
}

static void sleep_history_syscore_resume(void)
{
	struct timespec ts;

	/* snapshot the current time at suspend */
	read_persistent_clock(&ts);
	if (ts.tv_sec == 0 && ts.tv_nsec == 0)
		return;

	sleep_history_marker(SLEEP_HISTORY_SUSPEND_PERSISTCLOCK_EXIT, &ts, NULL);
}

static struct syscore_ops sleep_history_syscore_ops = {
	.suspend = sleep_history_syscore_suspend,
	.resume = sleep_history_syscore_resume,
};
#endif

static int sleep_history_syscore_init(void)
{
	/* Init circ buf for sleep history*/
	sleep_history_data.sleep_history.head = 0;
	sleep_history_data.sleep_history.tail = 0;
	sleep_history_data.sleep_history.buf = (char *)kmalloc(sizeof(struct sleep_history) *
						SLEEP_HISTORY_RINGBUFFER_SIZE, GFP_KERNEL);

#ifdef USE_SUSPEND_PERSISTCLOCK_TIME
	register_syscore_ops(&sleep_history_syscore_ops);
#endif

	return 0;
}

static void sleep_history_syscore_exit(void)
{
#ifdef USE_SUSPEND_PERSISTCLOCK_TIME
	unregister_syscore_ops(&sleep_history_syscore_ops);
#endif

	kfree(sleep_history_data.sleep_history.buf);
}
module_init(sleep_history_syscore_init);
module_exit(sleep_history_syscore_exit);
