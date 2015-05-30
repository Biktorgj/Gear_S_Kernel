/* trm.h Tizen Resouce Manager header file */

/* resouce list */
enum {
	KERNEL_RESERVED00,
	KERNEL_RESERVED01,

	RESERVED1_LOCK,

	NUMBER_OF_LOCK = (RESERVED1_LOCK + 30),
};


enum {
	TRM_CPU_MIN_FREQ_TYPE,
	TRM_CPU_MAX_FREQ_TYPE,
};

bool cpufreq_get_touch_boost_en(void);
unsigned int cpufreq_get_touch_boost_press(void);
unsigned int cpufreq_get_touch_boost_move(void);
unsigned int cpufreq_get_touch_boost_release(void);

void cpufreq_release_all_cpu_lock(int lock_type);

unsigned int cpu_gov_get_up_level(void);
unsigned int cpu_freq_get_threshold(void);


