/*
 * kernel/power/wakelock.c
 *
 * User space wakeup sources support.
 *
 * Copyright (C) 2012 Rafael J. Wysocki <rjw@sisk.pl>
 *
 * This code is based on the analogous interface allowing user space to
 * manipulate wakelocks on Android.
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#ifdef CONFIG_PM_SLAVE_WAKELOCKS
#include <linux/pm_slave_wakelocks.h>
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */

static DEFINE_MUTEX(wakelocks_lock);

#ifndef CONFIG_PM_SLAVE_WAKELOCKS
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
#endif /* !CONFIG_PM_SLAVE_WAKELOCKS */

static struct rb_root wakelocks_tree = RB_ROOT;

ssize_t pm_show_wakelocks(char *buf, bool show_active)
{
	struct rb_node *node;
	struct wakelock *wl;
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	mutex_lock(&wakelocks_lock);

	for (node = rb_first(&wakelocks_tree); node; node = rb_next(node)) {
		wl = rb_entry(node, struct wakelock, node);
		if (wl->ws.active == show_active)
			str += scnprintf(str, end - str, "%s ", wl->name);
	}
	if (str > buf)
		str--;

	str += scnprintf(str, end - str, "\n");

	mutex_unlock(&wakelocks_lock);
	return (str - buf);
}

#if CONFIG_PM_WAKELOCKS_LIMIT > 0
static unsigned int number_of_wakelocks;

static inline bool wakelocks_limit_exceeded(void)
{
	return number_of_wakelocks > CONFIG_PM_WAKELOCKS_LIMIT;
}

static inline void increment_wakelocks_number(void)
{
	number_of_wakelocks++;
}

static inline void decrement_wakelocks_number(void)
{
	number_of_wakelocks--;
}
#else /* CONFIG_PM_WAKELOCKS_LIMIT = 0 */
static inline bool wakelocks_limit_exceeded(void) { return false; }
static inline void increment_wakelocks_number(void) {}
static inline void decrement_wakelocks_number(void) {}
#endif /* CONFIG_PM_WAKELOCKS_LIMIT */

#ifdef CONFIG_PM_WAKELOCKS_GC
#define WL_GC_COUNT_MAX	100
#define WL_GC_TIME_SEC	300

static LIST_HEAD(wakelocks_lru_list);
static unsigned int wakelocks_gc_count;

static inline void wakelocks_lru_add(struct wakelock *wl)
{
	list_add(&wl->lru, &wakelocks_lru_list);
}

static inline void wakelocks_lru_most_recent(struct wakelock *wl)
{
	list_move(&wl->lru, &wakelocks_lru_list);
}

static void wakelocks_gc(void)
{
	struct wakelock *wl, *aux;
#ifdef CONFIG_PM_SLAVE_WAKELOCKS
	struct slave_wakelock *slave_wl = NULL;
#endif
	ktime_t now;

	if (++wakelocks_gc_count <= WL_GC_COUNT_MAX)
		return;

	now = ktime_get();
	list_for_each_entry_safe_reverse(wl, aux, &wakelocks_lru_list, lru) {
		u64 idle_time_ns;
		bool active;

		spin_lock_irq(&wl->ws.lock);
		idle_time_ns = ktime_to_ns(ktime_sub(now, wl->ws.last_time));
		active = wl->ws.active;
		spin_unlock_irq(&wl->ws.lock);

		if (idle_time_ns < ((u64)WL_GC_TIME_SEC * NSEC_PER_SEC))
			break;

		if (!active) {
#ifdef CONFIG_PM_SLAVE_WAKELOCKS
			while (!list_empty(&wl->slave_wl_list)) {
				slave_wl = list_entry(wl->slave_wl_list.next,
							struct slave_wakelock, node);
				pr_info("%s: %s\n", __func__, slave_wl->name);
				list_del(&slave_wl->node);
				kfree(slave_wl->name);
				kfree(slave_wl);
			}
#endif
			wakeup_source_remove(&wl->ws);
			rb_erase(&wl->node, &wakelocks_tree);
			list_del(&wl->lru);
			kfree(wl->name);
			kfree(wl);
			decrement_wakelocks_number();
		}
	}
	wakelocks_gc_count = 0;
}
#else /* !CONFIG_PM_WAKELOCKS_GC */
static inline void wakelocks_lru_add(struct wakelock *wl) {}
static inline void wakelocks_lru_most_recent(struct wakelock *wl) {}
static inline void wakelocks_gc(void) {}
#endif /* !CONFIG_PM_WAKELOCKS_GC */

static struct wakelock *wakelock_lookup_add(const char *name, size_t len,
					    bool add_if_not_found)
{
	struct rb_node **node = &wakelocks_tree.rb_node;
	struct rb_node *parent = *node;
	struct wakelock *wl;

	while (*node) {
		int diff;

		parent = *node;
		wl = rb_entry(*node, struct wakelock, node);
		diff = strncmp(name, wl->name, len);
		if (diff == 0) {
			if (wl->name[len])
				diff = -1;
			else
				return wl;
		}
		if (diff < 0)
			node = &(*node)->rb_left;
		else
			node = &(*node)->rb_right;
	}
	if (!add_if_not_found)
		return ERR_PTR(-EINVAL);

	if (wakelocks_limit_exceeded())
		return ERR_PTR(-ENOSPC);

	/* Not found, we have to add a new one. */
	wl = kzalloc(sizeof(*wl), GFP_KERNEL);
	if (!wl)
		return ERR_PTR(-ENOMEM);

#ifdef CONFIG_PM_SLAVE_WAKELOCKS
	*wl = ((struct wakelock) { .slave_wl_list	= LIST_HEAD_INIT(wl->slave_wl_list) });
#endif

	wl->name = kstrndup(name, len, GFP_KERNEL);
	if (!wl->name) {
		kfree(wl);
		return ERR_PTR(-ENOMEM);
	}
	wl->ws.name = wl->name;
	wakeup_source_add(&wl->ws);
	rb_link_node(&wl->node, parent, node);
	rb_insert_color(&wl->node, &wakelocks_tree);
	wakelocks_lru_add(wl);
	increment_wakelocks_number();
	return wl;
}

int pm_wake_lock(const char *buf)
{
	const char *str = buf;
	struct wakelock *wl;
	u64 timeout_ns = 0;
	size_t len;
	int ret = 0;

	while (*str && !isspace(*str))
		str++;

	len = str - buf;
	if (!len)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a valid timeout string appended. */
		ret = kstrtou64(skip_spaces(str), 10, &timeout_ns);
		if (ret)
			return -EINVAL;
	}

	mutex_lock(&wakelocks_lock);

	wl = wakelock_lookup_add(buf, len, true);
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}
	if (timeout_ns) {
		u64 timeout_ms = timeout_ns + NSEC_PER_MSEC - 1;

		do_div(timeout_ms, NSEC_PER_MSEC);
		__pm_wakeup_event(&wl->ws, timeout_ms);
	} else {
		__pm_stay_awake(&wl->ws);
	}

	wakelocks_lru_most_recent(wl);

 out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

int pm_wake_unlock(const char *buf)
{
	struct wakelock *wl;
	size_t len;
	int ret = 0;

	len = strlen(buf);
	if (!len)
		return -EINVAL;

	if (buf[len-1] == '\n')
		len--;

	if (!len)
		return -EINVAL;

	mutex_lock(&wakelocks_lock);

	wl = wakelock_lookup_add(buf, len, false);
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}
	__pm_relax(&wl->ws);

	wakelocks_lru_most_recent(wl);
	wakelocks_gc();

 out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

#ifdef CONFIG_PM_SLAVE_WAKELOCKS
#if CONFIG_PM_SLAVE_WAKELOCKS_LIMIT > 0
static inline bool slave_wakelocks_limit_exceeded(struct wakelock *wl)
{
	return wl->number_of_slave_wakelocks > CONFIG_PM_SLAVE_WAKELOCKS_LIMIT;
}

static inline void increment_slave_wakelocks_number(struct wakelock *wl)
{
	wl->number_of_slave_wakelocks++;
	pr_info("%s: %s: number_of_slave_wakelocks=%d\n",
			__func__, wl->name, wl->number_of_slave_wakelocks);
}

static inline void decrement_slave_wakelocks_number(struct wakelock *wl)
{
	wl->number_of_slave_wakelocks--;
	pr_info("%s: %s: number_of_slave_wakelocks=%d\n",
			__func__, wl->name, wl->number_of_slave_wakelocks);
}
#else /* CONFIG_PM_SLAVE_WAKELOCKS_LIMIT = 0 */
static inline bool slave_wakelocks_limit_exceeded(void) { return false; }
static inline void increment_slave_wakelocks_number(void) {}
static inline void decrement_slave_wakelocks_number(void) {}
#endif /* CONFIG_PM_SLAVE_WAKELOCKS_LIMIT */

#ifdef CONFIG_PM_SLAVE_WAKELOCKS_GC
#define SLAVE_WL_GC_COUNT_MAX	100
#define SLAVE_WL_GC_TIME_SEC	60*60*24

static unsigned int slave_wakelocks_gc_count;

static inline void slave_wakelocks_most_recent(struct wakelock *wl, struct slave_wakelock *slave_wl)
{
	list_move(&slave_wl->node, &wl->slave_wl_list);
}

static void slave_wakelocks_gc(struct wakelock *wl)
{
	struct slave_wakelock *slave_wl, *aux;
	ktime_t now;

	if (++slave_wakelocks_gc_count <= SLAVE_WL_GC_COUNT_MAX)
		return;

	now = ktime_get();
	list_for_each_entry_safe_reverse(slave_wl, aux, &wl->slave_wl_list, node) {
		u64 idle_time_ns;
		bool active;

		spin_lock_irq(&wl->ws.lock);
		idle_time_ns = ktime_to_ns(ktime_sub(now, slave_wl->last_time));
		active = slave_wl->active;
		spin_unlock_irq(&wl->ws.lock);

		if (idle_time_ns < ((u64)SLAVE_WL_GC_TIME_SEC * NSEC_PER_SEC))
			break;

		if (!active) {
			pr_info("%s: %s\n", __func__, slave_wl->name);
			list_del(&slave_wl->node);
			kfree(slave_wl->name);
			kfree(slave_wl);
			decrement_slave_wakelocks_number(wl);
		}
	}
	slave_wakelocks_gc_count = 0;
}
#else /* !CONFIG_PM_SLAVE_WAKELOCKS_GC */
static inline void slave_wakelocks_most_recent(struct wakelock *wl, struct slave_wakelock *slave_wl) {}
static inline void slave_wakelocks_gc(struct wakelock *wl) {}
#endif /* !CONFIG_PM_SLAVE_WAKELOCKS_GC */

ssize_t pm_show_slave_wakelocks(char *buf, bool show_active)
{
	struct rb_node *node;
	struct wakelock *wl;
	struct slave_wakelock *slave_wl = NULL;
	char *str = buf;
	char *end = buf + PAGE_SIZE;

	mutex_lock(&wakelocks_lock);

	for (node = rb_first(&wakelocks_tree); node; node = rb_next(node)) {
		wl = rb_entry(node, struct wakelock, node);
		str += scnprintf(str, end - str, "%s: ", wl->name);

		if (list_empty(&wl->slave_wl_list))
			continue;
		list_for_each_entry(slave_wl, &wl->slave_wl_list, node) {
			if (slave_wl->active == show_active)
				str += scnprintf(str, end - str, "%s ", slave_wl->name);
		}
	}
	if (str > buf)
		str--;

	str += scnprintf(str, end - str, "\n");

	mutex_unlock(&wakelocks_lock);
	return (str - buf);
}

static struct slave_wakelock *slave_wakelock_lookup_add(struct wakelock *wl,
			pid_t pid, bool add_if_not_found)
{
	struct slave_wakelock *slave_wl = NULL;
	struct task_struct *tsk;

	if (!wl) {
		pr_err("%s: cant add slave_wl, wl is empty\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	rcu_read_lock();
	tsk = find_task_by_vpid(pid);
	if (!tsk) {
		rcu_read_unlock();
		return ERR_PTR(-ESRCH);
	}
	rcu_read_unlock();

	list_for_each_entry(slave_wl, &wl->slave_wl_list, node) {
		if (!strcmp(tsk->comm, slave_wl->name))
			return slave_wl;
	}
	if (!add_if_not_found) {
		pr_err("%s: cant find slave_wl\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (slave_wakelocks_limit_exceeded(wl))
		return ERR_PTR(-ENOSPC);

	/* Not found, we have to add a new one. */
	slave_wl = kzalloc(sizeof(*slave_wl), GFP_KERNEL);
	if (!slave_wl) {
		pr_err("%s: cant allocation register, no free memory\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	slave_wl->name = kstrndup(tsk->comm, TASK_COMM_LEN, GFP_KERNEL);
	if (!slave_wl->name) {
		kfree(slave_wl);
		return ERR_PTR(-ENOMEM);
	}

	list_add(&slave_wl->node, &wl->slave_wl_list);
	increment_slave_wakelocks_number(wl);
	return slave_wl;
}

int pm_slave_wake_lock(const char *buf)
{
	const char *str = buf;
	struct wakelock *wl;
	struct slave_wakelock *slave_wl;
	int pid = 0;
	size_t len;
	int ret = 0;

	while (*str && !isspace(*str))
		str++;

	len = str - buf;
	if (!len)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a pid string appended. */
		ret = kstrtos32(skip_spaces(str), 10, &pid);
		if (ret)
			return -EINVAL;
	}

	mutex_lock(&wakelocks_lock);

	wl = wakelock_lookup_add(buf, len, false);
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}

	pr_info("%s: wl=%s: pid=%d\n", __func__, wl->name, pid);
	slave_wl = slave_wakelock_lookup_add(wl, (pid_t)pid, true);
	if (IS_ERR(slave_wl)) {
		ret = PTR_ERR(slave_wl);
		goto out;
	}
	if (!slave_wl->active) {
		slave_wl->active = true;
		slave_wl->active_count++;
		slave_wl->last_time = ktime_get();
		if (wl->ws.autosleep_enabled)
			slave_wl->start_prevent_time = slave_wl->last_time;
	}

	slave_wakelocks_most_recent(wl, slave_wl);

 out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

int pm_slave_wake_unlock(const char *buf)
{
	const char *str = buf;
	struct wakelock *wl;
	struct slave_wakelock *slave_wl;
	int pid = 0;
	size_t len;
	int ret = 0;

	while (*str && !isspace(*str))
		str++;

	len = str - buf;
	if (!len)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a pid string appended. */
		ret = kstrtos32(skip_spaces(str), 10, &pid);
		if (ret)
			return -EINVAL;
	}

	mutex_lock(&wakelocks_lock);

	wl = wakelock_lookup_add(buf, len, false);
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}

	pr_info("%s: wl=%s: pid=%d\n", __func__, wl->name, pid);
	slave_wl = slave_wakelock_lookup_add(wl, (pid_t)pid, false);
	if (IS_ERR(slave_wl)) {
		ret = PTR_ERR(slave_wl);
		goto out;
	}

	if (slave_wl->active) {
		ktime_t duration;
		ktime_t now;

		slave_wl->active = false;
		now = ktime_get();
		duration = ktime_sub(now, slave_wl->last_time);
		slave_wl->total_time = ktime_add(slave_wl->total_time, duration);
		if (ktime_to_ns(duration) > ktime_to_ns(slave_wl->max_time))
			slave_wl->max_time = duration;

		slave_wl->last_time = now;

		if (wl->ws.autosleep_enabled) {
			ktime_t delta = ktime_sub(now, slave_wl->start_prevent_time);
			slave_wl->prevent_sleep_time = ktime_add(slave_wl->prevent_sleep_time, delta);
			slave_wl->prevent_time = delta;
		}
	}

	slave_wakelocks_most_recent(wl, slave_wl);
	slave_wakelocks_gc(wl);

 out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

int pm_slave_wake_del(const char *buf)
{
	const char *str = buf;
	struct wakelock *wl;
	struct slave_wakelock *slave_wl;
	int pid = 0;
	size_t len;
	int ret = 0;

	while (*str && !isspace(*str))
		str++;

	len = str - buf;
	if (!len)
		return -EINVAL;

	if (*str && *str != '\n') {
		/* Find out if there's a pid string appended. */
		ret = kstrtos32(skip_spaces(str), 10, &pid);
		if (ret)
			return -EINVAL;
	}

	mutex_lock(&wakelocks_lock);

	wl = wakelock_lookup_add(buf, len, false);
	if (IS_ERR(wl)) {
		ret = PTR_ERR(wl);
		goto out;
	}

	pr_info("%s: wl=%s: pid=%d\n", __func__, wl->name, pid);
	slave_wl = slave_wakelock_lookup_add(wl, (pid_t)pid, false);
	if (IS_ERR(slave_wl)) {
		ret = PTR_ERR(slave_wl);
		goto out;
	}

	list_del(&slave_wl->node);
	kfree(slave_wl->name);
	kfree(slave_wl);
	decrement_slave_wakelocks_number(wl);
 out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

bool pm_get_last_slave_wakelocks(struct wakeup_source *ws,
			struct slave_wakelock **last_slave_wl_array, int slave_wl_count)
{
	unsigned int ret = 0;
	int active_count = 0;
	struct wakelock *wl;
	struct slave_wakelock *slave_wl;
	const char *str = ws->name;
	size_t len;

	while (*str)
		str++;

	len = str - ws->name;
	if (!len)
		return ret;

	mutex_lock(&wakelocks_lock);
	wl = wakelock_lookup_add(ws->name, len, false);
	if (IS_ERR(wl))
		goto out;

	pr_debug("%s: wl=%s: start_prevent_time=%lld\n", __func__,
		wl->name, ktime_to_ms(ws->start_prevent_time));
	list_for_each_entry(slave_wl, &wl->slave_wl_list, node) {
		pr_debug("%s: slave_wl=%s: active=%d: start_prevent_time=%lld: "
				"prevent_time=%lld\n", __func__,
				slave_wl->name, slave_wl->active,
				ktime_to_ms(slave_wl->start_prevent_time),
				ktime_to_ms(slave_wl->prevent_time));
		if (slave_wl->active) {
			if (active_count < slave_wl_count)
				*(&last_slave_wl_array[active_count++]) = slave_wl;
		} else if (ktime_to_ns(slave_wl->start_prevent_time) >
				ktime_to_ns(ws->start_prevent_time)) {
			if (active_count < slave_wl_count)
				*(&last_slave_wl_array[active_count++]) = slave_wl;
			else if (ktime_to_ns(slave_wl->prevent_time) >
					ktime_to_ns(
					(*(&last_slave_wl_array[slave_wl_count-1]))->prevent_time))
				*(&last_slave_wl_array[slave_wl_count-1]) = slave_wl;
		}
	}

	if (active_count > 0)
		ret = 1;
out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

bool pm_get_last_slave_wakelock(struct wakeup_source *ws,
			struct slave_wakelock **last_slave_wl)
{
	unsigned int ret = 0;
	struct wakelock *wl;
	struct slave_wakelock *slave_wl;
	int active_count = 0;
	struct slave_wakelock *last_activity_slave_wl = NULL;
	struct slave_wakelock *only_activity_slave_wl = NULL;


	const char *str = ws->name;
	size_t len;

	while (*str)
		str++;

	len = str - ws->name;
	if (!len)
		return ret;

	mutex_lock(&wakelocks_lock);
	wl = wakelock_lookup_add(ws->name, len, false);
	if (IS_ERR(wl))
		goto out;

	list_for_each_entry(slave_wl, &wl->slave_wl_list, node) {
		if (slave_wl->active) {
			active_count++;
			if (active_count == 1)
				only_activity_slave_wl = slave_wl;
		} else if (active_count == 0 &&
			   (!last_activity_slave_wl ||
				ktime_to_ns(slave_wl->last_time) >
				ktime_to_ns(last_activity_slave_wl->last_time))) {
			last_activity_slave_wl = slave_wl;
		}
	}

	if (active_count == 0 && last_activity_slave_wl) {
		*last_slave_wl = last_activity_slave_wl;
		ret = 1;
	} else if (active_count == 1) {
		*last_slave_wl = only_activity_slave_wl;
		ret = 1;
	}
out:
	mutex_unlock(&wakelocks_lock);
	return ret;
}

static struct dentry *slave_wakelocks_stats_dentry;

/**
 * slave_wakelocks_stats_show - Print slave wakelocks statistics information.
 * @m: seq_file to print the statistics into.
 */
static int slave_wakelocks_stats_show(struct seq_file *m, void *unused)
{
	struct rb_node *node;
	struct wakelock *wl;
	struct slave_wakelock *slave_wl = NULL;
	ktime_t total_time;
	ktime_t max_time;
	unsigned long active_count;
	ktime_t active_time;
	ktime_t prevent_sleep_time;

	seq_puts(m, "m-name\t\ts-name\t\tactive_count\t"
		"active_since\ttotal_time\tmax_time\t"
		"last_change\tprevent_suspend_time\tlast_prevent_time\n");

	mutex_lock(&wakelocks_lock);

	for (node = rb_first(&wakelocks_tree); node; node = rb_next(node)) {
		wl = rb_entry(node, struct wakelock, node);

		if (list_empty(&wl->slave_wl_list))
			continue;

		list_for_each_entry(slave_wl, &wl->slave_wl_list, node) {
			total_time = slave_wl->total_time;
			max_time = slave_wl->max_time;
			prevent_sleep_time = slave_wl->prevent_sleep_time;
			active_count = slave_wl->active_count;
			if (slave_wl->active) {
				ktime_t now = ktime_get();

				active_time = ktime_sub(now, slave_wl->last_time);
				total_time = ktime_add(total_time, active_time);
				if (active_time.tv64 > max_time.tv64)
					max_time = active_time;

				if (wl->ws.autosleep_enabled)
					prevent_sleep_time = ktime_add(prevent_sleep_time,
						ktime_sub(now, slave_wl->start_prevent_time));
			} else {
				active_time = ktime_set(0, 0);
			}
			seq_printf(m, "%-12s\t%-12s\t%lu\t\t"
				"%lld\t\t%lld\t\t%lld\t\t%lld\t\t%lld\t\t\t%lld\n",
				wl->name, slave_wl->name, slave_wl->active_count,
				ktime_to_ms(active_time), ktime_to_ms(total_time),
				ktime_to_ms(max_time), ktime_to_ms(slave_wl->last_time),
				ktime_to_ms(prevent_sleep_time),
				ktime_to_ms(slave_wl->prevent_time));
		}
	}

	seq_printf(m, "\n");

	mutex_unlock(&wakelocks_lock);
	return 0;
}

static int slave_wakelocks_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, slave_wakelocks_stats_show, NULL);
}

static const struct file_operations slave_wakelocks_stats_fops = {
	.owner = THIS_MODULE,
	.open = slave_wakelocks_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init slave_wakelocks_debugfs_init(void)
{
	slave_wakelocks_stats_dentry = debugfs_create_file("slave_wakelocks",
			S_IRUGO, NULL, NULL, &slave_wakelocks_stats_fops);
	return 0;
}

postcore_initcall(slave_wakelocks_debugfs_init);
#endif /* CONFIG_PM_SLAVE_WAKELOCKS */
