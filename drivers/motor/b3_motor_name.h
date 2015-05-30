#ifndef _MOTOR_NAME_H
#define _MOTOR_NAME_H
#include <linux/dc_motor.h>

#define DC_MOTOR_CHECK_NOT_YET -3

extern unsigned int system_rev;
extern int is_dc;

enum HW_REV{
	HW_REV_EMUL = 0x0,
	HW_REV_00 = 0x1,
	HW_REV_01 = 0x2,
	HW_REV_02 = 0x3,
	HW_REV_03 = 0x4,
	HW_REV_04 = 0x5,
	HW_REV_05 = 0x9,
	HW_REV_07D = 0xA,
};

static char * get_motor_name(void)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(false == is_dc)
		return "DMJBRN3694AA";
	else
		return "BSS-3715";
}

static char * get_motor_type(void)
{
	if(DC_MOTOR_CHECK_NOT_YET == is_dc)
		is_dc  = is_dc_motor();
	if(false == is_dc)
		return "LRA";
	else
		return "ERM";
}

#endif
