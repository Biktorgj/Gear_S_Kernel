
#ifndef _LINUX_SYSTEM_LOAD_ANALYZER_H
#define _LINUX_SYSTEM_LOAD_ANALYZER_H

#define CPU_NUM	NR_CPUS
#define CONFIG_CHECK_WORK_HISTORY	1
#define CONFIG_SLP_CHECK_BUS_LOAD 	1
#define CONFIG_SLP_MINI_TRACER

enum {
	NR_RUNNING_TASK,
	GPU_FREQ,
	GPU_UTILIZATION,
};

struct saved_load_factor_tag {
	unsigned int nr_running_task;
	unsigned int gpu_freq;
	unsigned int gpu_utilization;
};

extern struct saved_load_factor_tag	saved_load_factor;

extern bool cpu_task_history_onoff;

void store_external_load_factor(int type, unsigned int data);

void store_cpu_load(unsigned int cpufreq[], unsigned int cpu_load[]);

void cpu_load_touch_event(unsigned int event);

void __slp_store_task_history(unsigned int cpu, struct task_struct *task);

static inline void slp_store_task_history(unsigned int cpu
					, struct task_struct *task)
{
	__slp_store_task_history(cpu, task);
}

u64  get_load_analyzer_time(void);

void __slp_store_work_history(struct work_struct *work, work_func_t func
						, u64 start_time, u64 end_time);

void store_killed_task(struct task_struct *tsk);


void cpu_last_load_freq(unsigned int range, int max_list_num);

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
unsigned int get_bimc_clk(void);
unsigned int get_cnoc_clk(void);
unsigned int get_pnoc_clk(void);
unsigned int get_snoc_clk(void);
#endif

#if defined(CONFIG_SLP_MINI_TRACER)

#define TIME_ON		(1 << 0)
#define FLUSH_CACHE	(1 << 1)

extern int kernel_mini_tracer_i2c_log_on;

void kernel_mini_tracer(char *input_string, int option);
void kernel_mini_tracer_smp(char *input_string);
#define mini_trace_log {  \
	char str[64];   \
	sprintf(str, "%s %d\n", __FUNCTION__, __LINE__); \
	kernel_mini_tracer(str, TIME_ON | FLUSH_CACHE); \
}
#else
#define mini_trace_log
#endif


void notify_load_analyzer(void);


extern int value_for_debug;

#endif

