/*
 * gpio_state_mon.h -- Samsung GPIO state display system
 */

#ifndef __GPIO_STATE_MON_H
#define __GPIO_STATE_MON_H

#include <linux/types.h>

#define GPIO_MON_BUF_SIZE PAGE_SIZE*12
#define GPIO_MON_MAXLENGTH  5  /*max length of the string out of - PD/PU/NP ; FUNC/IN/OUT ...*/

extern unsigned int gpio_mon_gpio_count;

struct gpio_mon_gpio_state_map {
unsigned int direction;
unsigned int resistor;
unsigned int state;
};

void gpio_mon_save_gpio_state(unsigned char,unsigned int ,unsigned int, unsigned int, unsigned int);
void msm8x26_check_current_gpio_status( void );

#endif
