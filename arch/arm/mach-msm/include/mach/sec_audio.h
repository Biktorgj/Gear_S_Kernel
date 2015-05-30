/*
 * Copyright (C) 2014 Samsung Electronics, Inc.
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

#ifndef __MACH_SEC_AUDIO_H
#define __MACH_SEC_AUDIO_H __FILE__


#ifdef CONFIG_MACH_B3
static inline int check_es325_support(int rev)
{
	return 0;
}

static inline int check_es705_support(int rev)
{
	if (rev >= 0x08)
		return 1;
	else
		return 0;
}
#endif

#define	SEC_MICBIAS_SRC_CODEC	0x01
#define	SEC_MICBIAS_SRC_VS	0x02

#endif /* __MACH_SEC_AUDIO_H */
