/*
 * drivers/misc/clkmon/clk_mon_msm.h
 *
 * Register address based on MSM 8x26 Chipset
 */

#ifndef _CLK_MON_MSM8x26_H_
#define _CLK_MON_MSM8x26_H_

#include <linux/clk_mon.h>

#define CLK_MON_CGATE_GCC_BLSP_BASE	0xFC400000
#define CLK_MON_CGATE_GCC_MSS_BASE	0xFD8C0000
#define CLK_MON_CGATE_MMSS_BASE		0xFC400000

static inline unsigned int vaddr_to_paddr(unsigned long vaddr, int mode)
{
	unsigned int paddr = (unsigned int)vaddr;

	if (mode == PWR_REG) {
		paddr &= 0x0000ffff;
		paddr |= 0x10020000;
	} else if (mode == CLK_REG) {
		unsigned int tmp_high, tmp_low;
		tmp_low = paddr & 0x0000ffff;
		tmp_high = paddr & 0xffff0000;
		tmp_high -= 0xE80D0000;
		paddr = tmp_high | tmp_low;
	}

	return paddr;
}

#endif
