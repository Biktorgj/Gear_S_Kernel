#ifndef _CLK_MON_H_
#define _CLK_MON_H_

#define CLK_MON_MAX_CLK_GATE_NAME   	30
#define CLK_MON_MAX_REG     		10

#define CLK_GATES_NUM			5
#define PWR_DOMAINS_NUM			3
#define CLK_MON_BUF_SIZE		PAGE_SIZE

enum {
	PWR_REG = 0,
	CLK_REG,
};

struct power_domain_mask {
	char name[CLK_MON_MAX_CLK_GATE_NAME];
};

struct clk_gate_mask {
	char name[CLK_MON_MAX_CLK_GATE_NAME];
	unsigned long addr;
};

int clk_mon_power_domain(unsigned int *pm_status);
int clk_mon_clock_gate(unsigned int *clk_status);
int clk_mon_get_clock_info(unsigned int *clk_status, char *buf);
#endif
