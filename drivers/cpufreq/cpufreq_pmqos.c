#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/cpuidle.h>

/* arch/arm/mach-msm/msm-bus-pm-dev + */
int msm_set_pmqos_bimc_min_freq(void);
int msm_set_pmqos_bimc_max_freq(void);
/* arch/arm/mach-msm/msm-bus-pm-dev - */

#define TRM_TOUCH_BOOST_CONTROL	1

#include <linux/trm.h>

#define MIN_CPU_FREQ	300000
#define MAX_CPU_FREQ	1000000

#define define_one_every_rw(_name)		\
static struct global_attr _name =		\
__ATTR(_name, 0600, show_##_name, store_##_name)


struct pm_qos_lock_tag {
	int min_value;
	int max_value;
};
int min_cpu_freq, max_cpu_freq;

static struct pm_qos_lock_tag cpufreq_pm_qos_lock[NUMBER_OF_LOCK];
static struct pm_qos_request cpufreq_min_qos_array[NUMBER_OF_LOCK];
static struct pm_qos_request cpufreq_max_qos_array[NUMBER_OF_LOCK];

static struct pm_qos_request cpu_gov_up_level_array[NUMBER_OF_LOCK];
static struct pm_qos_request cpu_freq_up_threshold_array[NUMBER_OF_LOCK];


static struct pm_qos_request cpu_online_min_qos_array[NUMBER_OF_LOCK];
static struct pm_qos_request cpu_online_max_qos_array[NUMBER_OF_LOCK];

static struct pm_qos_request bimc_freq_min_qos_array[NUMBER_OF_LOCK];
static struct pm_qos_request bimc_freq_max_qos_array[NUMBER_OF_LOCK];


#if defined(TRM_TOUCH_BOOST_CONTROL)
static struct pm_qos_request touch_press_qos_array[NUMBER_OF_LOCK];
static struct pm_qos_request touch_move_qos_array[NUMBER_OF_LOCK];
static struct pm_qos_request touch_release_qos_array[NUMBER_OF_LOCK];
#endif

static int atoi(const char *str)
{
	int result = 0;
	int count = 0;
	if (str == NULL)
		return -1;
	while (str[count] && str[count] >= '0' && str[count] <= '9') {
		result = result * 10 + str[count] - '0';
		++count;
	}
	return result;
}

static ssize_t show_cpufreq_max(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	max_cpu_freq = pm_qos_request(PM_QOS_CPU_FREQ_MAX);
	ret +=  snprintf(buf + ret, PAGE_SIZE - ret, "%d\n", max_cpu_freq);

	return ret;
}

static ssize_t store_cpufreq_max(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int lock_id = KERNEL_RESERVED00, lock_value = 0;
	char *p2 = NULL;

	p2 = strstr(buf, "ID");

	if (p2 == NULL)
		p2 = strstr(buf, "id");

	if (p2 != NULL)
		lock_id = atoi(p2+2);
	else
		lock_id = KERNEL_RESERVED00;

	if (lock_id >= NUMBER_OF_LOCK) {
		pr_err("%s lock_id=%d is wrong", __FUNCTION__ ,lock_id);
		return -EINVAL;
	}

	if (strstr(buf, "-1")!=NULL)
		lock_value = -1;
	else
		lock_value = atoi(buf);

	cpufreq_pm_qos_lock[lock_id].max_value = lock_value; /* for debugging */
	printk(KERN_DEBUG "%s %s/%d id=%d max_cpu_freq=%d\n", __FUNCTION__
		, get_current()->comm ,get_current()->pid ,lock_id ,lock_value);

	if (lock_value == -1) {
	 	if (pm_qos_request_active(&cpufreq_max_qos_array[lock_id]))
			pm_qos_remove_request(&cpufreq_max_qos_array[lock_id]);

	} else {
		if (!pm_qos_request_active(&cpufreq_max_qos_array[lock_id])) {
			pm_qos_add_request(&cpufreq_max_qos_array[lock_id]
				, PM_QOS_CPU_FREQ_MAX, lock_value);
		} else
			pm_qos_update_request(&cpufreq_max_qos_array[lock_id], lock_value);
	}

	return count;
}

static ssize_t show_cpufreq_min(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	min_cpu_freq = pm_qos_request(PM_QOS_CPU_FREQ_MIN);
	ret +=  snprintf(buf + ret, PAGE_SIZE - ret, "%d\n", min_cpu_freq);

	return ret;
}

static ssize_t store_cpufreq_min(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int lock_id = KERNEL_RESERVED00, lock_value = 0;
	char *p2 = NULL;

	p2 = strstr(buf, "ID");

	if (p2 == NULL)
		p2 = strstr(buf, "id");

	if (p2 != NULL)
		lock_id = atoi(p2+2);
	else
		lock_id = KERNEL_RESERVED00;

	if (lock_id >= NUMBER_OF_LOCK) {
		pr_err("%s lock_id=%d is wrong", __FUNCTION__ ,lock_id);
		return -EINVAL;
	}

	if (strstr(buf, "-1")!=NULL)
		lock_value = -1;
	else
		lock_value = atoi(buf);

	cpufreq_pm_qos_lock[lock_id].min_value = lock_value; /* for debugging */
	printk(KERN_DEBUG "%s %s/%d id=%d min_cpu_freq=%d\n", __FUNCTION__
		, get_current()->comm ,get_current()->pid ,lock_id ,lock_value);

	if (lock_value == -1) {
	 	if (pm_qos_request_active(&cpufreq_min_qos_array[lock_id]))
			pm_qos_remove_request(&cpufreq_min_qos_array[lock_id]);

	} else {
		if (!pm_qos_request_active(&cpufreq_min_qos_array[lock_id])) {
			pm_qos_add_request(&cpufreq_min_qos_array[lock_id]
				, PM_QOS_CPU_FREQ_MIN, lock_value);
		} else
			pm_qos_update_request(&cpufreq_min_qos_array[lock_id], lock_value);
	}

	return count;
}

#if defined(TRM_TOUCH_BOOST_CONTROL)
static int set_pmqos_data(struct pm_qos_request *any_qos_array, int pmqos_type, const char *buf)
{
	int lock_id = KERNEL_RESERVED00, lock_value = 0;
	char *p2 = NULL;

	p2 = strstr(buf, "ID");

	if (p2 == NULL)
		p2 = strstr(buf, "id");

	if (p2 != NULL)
		lock_id = atoi(p2+2);
	else
		lock_id = KERNEL_RESERVED00;

	if (lock_id >= NUMBER_OF_LOCK) {
		pr_err("%s lock_id=%d is wrong", __FUNCTION__ ,lock_id);
		return -EINVAL;
	}

	if (strstr(buf, "-1")!=NULL)
		lock_value = -1;
	else
		lock_value = atoi(buf);

	printk(KERN_DEBUG "%s %s/%d id=%d value=%d\n", __FUNCTION__
		, get_current()->comm ,get_current()->pid ,lock_id ,lock_value);

	if (lock_value == -1) {
		if (pm_qos_request_active(&any_qos_array[lock_id]))
			pm_qos_remove_request(&any_qos_array[lock_id]);

	} else {
		if (!pm_qos_request_active(&any_qos_array[lock_id])) {
			pm_qos_add_request(&any_qos_array[lock_id]
				, pmqos_type, lock_value);
		} else
			pm_qos_update_request(&any_qos_array[lock_id], lock_value);
	}

	return 0;
}
#endif

void cpufreq_release_all_cpu_lock(int lock_type)
{
	int i =0;

	for(i=0; i<NUMBER_OF_LOCK; i++) {
		if (lock_type == TRM_CPU_MIN_FREQ_TYPE) {
			if (cpufreq_pm_qos_lock[i].min_value > 0) {
			 	if (pm_qos_request_active(&cpufreq_min_qos_array[i])) {
					cpufreq_pm_qos_lock[i].min_value = -1;
					pm_qos_remove_request(&cpufreq_min_qos_array[i]);
					pr_info("%s min id[%d] released forcibly\n", __FUNCTION__, i);
			 	}
			}
		} else if (lock_type == TRM_CPU_MAX_FREQ_TYPE) {
			if (cpufreq_pm_qos_lock[i].max_value > 0) {
			 	if (pm_qos_request_active(&cpufreq_max_qos_array[i])) {
					cpufreq_pm_qos_lock[i].max_value = -1;
					pm_qos_remove_request(&cpufreq_max_qos_array[i]);
					pr_info("%s max id[%d] released forcibly\n", __FUNCTION__, i);
			 	}
			}
		}
	}
}

static ssize_t show_cpufreq_lock_state(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int i, ret = 0;

	for(i=0; i<NUMBER_OF_LOCK; i++) {
		ret +=  snprintf(buf + ret, PAGE_SIZE - ret, "[%d] MIN:%d MAX:%d\n"
			,i ,cpufreq_pm_qos_lock[i].min_value, cpufreq_pm_qos_lock[i].max_value);
	}

	return ret;
}

static ssize_t store_cpufreq_lock_state(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	return count;
}

static ssize_t show_cpu_online_max(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", pm_qos_request(PM_QOS_CPU_ONLINE_MAX));
	return ret;
}

static ssize_t __ref store_cpu_online_max(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int ret = 0;
	ret = set_pmqos_data(cpu_online_max_qos_array, PM_QOS_CPU_ONLINE_MAX, buf);
	if (ret)
		return ret;

	if (num_online_cpus() > pm_qos_request(PM_QOS_CPU_ONLINE_MAX))
		cpu_down(1);

	return count;
}

static ssize_t show_cpu_online_min(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", pm_qos_request(PM_QOS_CPU_ONLINE_MIN));
	return ret;
}

static ssize_t __ref store_cpu_online_min(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int ret = 0;
	ret = set_pmqos_data(cpu_online_min_qos_array, PM_QOS_CPU_ONLINE_MIN, buf);
	if (ret)
		return ret;

	if (num_online_cpus() < pm_qos_request(PM_QOS_CPU_ONLINE_MIN))
		cpu_up(1);

	return count;
}

static ssize_t show_bimc_freq_max(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", pm_qos_request(PM_QOS_BIMC_FREQ_MAX));
	return ret;
}

static ssize_t store_bimc_freq_max(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int ret = 0;
	ret = set_pmqos_data(bimc_freq_max_qos_array, PM_QOS_BIMC_FREQ_MAX, buf);
	if (ret)
		return ret;

	msm_set_pmqos_bimc_max_freq();

	return count;
}

static ssize_t show_bimc_freq_min(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", pm_qos_request(PM_QOS_BIMC_FREQ_MIN));
	return ret;
}

static ssize_t store_bimc_freq_min(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int ret = 0;
	ret = set_pmqos_data(bimc_freq_min_qos_array, PM_QOS_BIMC_FREQ_MIN, buf);
	if (ret)
		return ret;

	msm_set_pmqos_bimc_min_freq();

	return count;
}

#if defined(TRM_TOUCH_BOOST_CONTROL)
static bool  touch_boost_en_value = 1;
static ssize_t show_touch_boost_en(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", touch_boost_en_value);
	return ret;
}

static ssize_t store_touch_boost_en(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int input;
	input = atoi(buf);

	if ((input == 0 ) || (input == 1 ))
		touch_boost_en_value = input;

	return count;
}

bool cpufreq_get_touch_boost_en(void)
{
	return touch_boost_en_value;
}

static unsigned int touch_boost_press_value = PM_QOS_TOUCH_PRESS_DEFAULT_VALUE;
static ssize_t show_touch_boost_press(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", cpufreq_get_touch_boost_press());
	return ret;
}

static ssize_t store_touch_boost_press(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int ret = 0;
	ret = set_pmqos_data(touch_press_qos_array, PM_QOS_TOUCH_PRESS, buf);
	if (ret)
		return ret;
	else
		return count;
}

unsigned int cpufreq_get_touch_boost_press(void)
{
	touch_boost_press_value = pm_qos_request(PM_QOS_TOUCH_PRESS);

	return touch_boost_press_value;
}

static unsigned int touch_boost_move_value = PM_QOS_TOUCH_MOVE_DEFAULT_VALUE;
static ssize_t show_touch_boost_move(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", cpufreq_get_touch_boost_move());
	return ret;
}

static ssize_t store_touch_boost_move(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int ret = 0;
	ret = set_pmqos_data(touch_move_qos_array, PM_QOS_TOUCH_MOVE, buf);
	if (ret)
		return ret;
	else
		return count;
}

unsigned int cpufreq_get_touch_boost_move(void)
{
	touch_boost_move_value = pm_qos_request(PM_QOS_TOUCH_MOVE);

	return touch_boost_move_value;
}

static unsigned int touch_boost_release_value = PM_QOS_TOUCH_RELEASE_DEFAULT_VALUE;
static ssize_t show_touch_boost_release(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", cpufreq_get_touch_boost_release());
	return ret;
}

static ssize_t store_touch_boost_release(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{
	int ret = 0;
	ret = set_pmqos_data(touch_release_qos_array, PM_QOS_TOUCH_RELEASE, buf);
	if (ret)
		return ret;
	else
		return count;
}

unsigned int cpufreq_get_touch_boost_release(void)
{
	touch_boost_release_value = pm_qos_request(PM_QOS_TOUCH_RELEASE);

	return touch_boost_release_value;
}
#endif

static unsigned int cpu_gov_up_level_value = PM_QOS_CPU_GOV_UP_LEVEL_DEFAULT_VALUE;
unsigned int cpu_gov_get_up_level(void)
{
	cpu_gov_up_level_value = pm_qos_request(PM_QOS_CPU_GOV_UP_LEVEL);

	return cpu_gov_up_level_value;
}

static ssize_t show_cpu_gov_up_level(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", cpu_gov_get_up_level());
	return ret;
}

static ssize_t store_cpu_gov_up_level(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{

	set_pmqos_data(cpu_gov_up_level_array, PM_QOS_CPU_GOV_UP_LEVEL, buf);

	return count;
}


static unsigned int cpu_freq_up_threshold_value = PM_QOS_CPU_FREQ_UP_THRESHOLD_DEFAULT_VALUE;
unsigned int cpu_freq_get_threshold(void)
{
	cpu_freq_up_threshold_value = pm_qos_request(PM_QOS_CPU_FREQ_UP_THRESHOLD);

	return cpu_freq_up_threshold_value;
}

static ssize_t show_cpu_freq_up_threshold(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	unsigned int ret = 0;
	ret =  sprintf(buf, "%d\n", cpu_freq_get_threshold());
	return ret;
}

static ssize_t store_cpu_freq_up_threshold(struct kobject *a, struct attribute *b,
				  const char *buf, size_t count)
{

	set_pmqos_data(cpu_freq_up_threshold_array, PM_QOS_CPU_FREQ_UP_THRESHOLD, buf);

	return count;
}

define_one_every_rw(cpufreq_max);
define_one_every_rw(cpufreq_min);
define_one_every_rw(cpu_online_max);
define_one_every_rw(cpu_online_min);
define_one_every_rw(cpufreq_lock_state);

define_one_every_rw(bimc_freq_max);
define_one_every_rw(bimc_freq_min);

define_one_every_rw(cpu_gov_up_level);
define_one_every_rw(cpu_freq_up_threshold);

#if defined(TRM_TOUCH_BOOST_CONTROL)
define_one_every_rw(touch_boost_en);
define_one_every_rw(touch_boost_press);
define_one_every_rw(touch_boost_move);
define_one_every_rw(touch_boost_release);
#endif
static struct attribute *pmqos_attributes[] = {
	&cpufreq_min.attr,
	&cpufreq_max.attr,
	&cpu_online_min.attr,
	&cpu_online_max.attr,
	&bimc_freq_min.attr,
	&bimc_freq_max.attr,
	&cpu_gov_up_level.attr,
	&cpu_freq_up_threshold.attr,
	&cpufreq_lock_state.attr,
#if defined(TRM_TOUCH_BOOST_CONTROL)
	&touch_boost_en.attr,
	&touch_boost_press.attr,
	&touch_boost_move.attr,
	&touch_boost_release.attr,
#endif
	NULL
};

static struct attribute_group pmqos_attr_group = {
	.attrs = pmqos_attributes,
	.name = "pmqos",
};

static struct workqueue_struct *bimc_wq;
struct delayed_work bimc_fastboot_work;
static struct pm_qos_request bimc_min_qos;
#define BIMC_BOOT_VALUE	400000
#define BIMC_BOOTING_TIME	17

void bimc_boot_completed(struct work_struct *work)
{
	pr_info("%s\n", __FUNCTION__);
	if (pm_qos_request_active(&bimc_min_qos))
		pm_qos_remove_request(&bimc_min_qos);


	msm_set_pmqos_bimc_min_freq();
}


static int __init cpufreq_pmqos_init(void)
{
	int rc;

	rc = sysfs_create_group(cpufreq_global_kobject,
				&pmqos_attr_group);
	if (rc) {
		pr_err("%s create sysfs error rc=%d\n", __FUNCTION__, rc);
		return rc;
	}

	bimc_wq = alloc_workqueue("bimc_wq", WQ_HIGHPRI, 0);
	if (!bimc_wq) {
		printk(KERN_ERR "Failed to create bimc_wq workqueue\n");
		return -EFAULT;
	}
	INIT_DELAYED_WORK(&bimc_fastboot_work, bimc_boot_completed);

	/* mif max freq for fast booting */
	if (!pm_qos_request_active(&bimc_min_qos)) {
		pm_qos_add_request(&bimc_min_qos
			, PM_QOS_BIMC_FREQ_MIN, BIMC_BOOT_VALUE);
	} else
		pm_qos_update_request(&bimc_min_qos, BIMC_BOOT_VALUE);

	msm_set_pmqos_bimc_min_freq();

	queue_delayed_work(bimc_wq, &bimc_fastboot_work
					, (BIMC_BOOTING_TIME)*HZ);

	return rc;
}

MODULE_AUTHOR("Yong-U Baek <yu.baek@samsung.com>");
MODULE_DESCRIPTION("cpufreq_pmqos");
MODULE_LICENSE("GPL");

late_initcall(cpufreq_pmqos_init);
