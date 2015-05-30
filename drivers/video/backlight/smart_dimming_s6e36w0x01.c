/* drivers/video/backlight/smart_dimming_s6e36w0x01.c
 *
 * MIPI-DSI based S6E36W0X01 AMOLED panel driver.
 *
 * Taeheon Kim, <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/smart_dimming_s6e36w0x01.h>
//#define SMART_DIMMING_DEBUG
#define RGB_COMPENSATION 30

static unsigned int ref_gamma[NUM_VREF][CI_MAX] = {
	{0x00, 0x00, 0x00},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x100, 0x100, 0x100},
};

static const unsigned short ref_cd_tbl[MAX_GAMMA_CNT] = {
	115, 115, 115, 115, 115, 115, 115, 115, 115, 115,
	115, 115, 115, 115, 115, 115, 115, 115, 115, 115,
	115, 115, 115, 115, 115, 115, 115, 115, 115, 115,
	112, 120, 128, 136, 145, 153, 164, 173, 186, 197,
	209, 223, 237,
	252, 252, 252, 252, 252, 252, 252,
	254, 265, 282, 300
};

static const int gradation_shift[MAX_GAMMA_CNT][NUM_VREF] = {
/*V0 V3 V11 V23 V51 V87 V151 V203 V255*/
	{0, 24, 21, 17, 13, 11, 8, 2, 3, 0},			/*	10cd		*/
	{0, 21, 20, 16, 12, 10, 6, 2, 0, 0},			/*	11cd		*/
	{0, 20, 19, 14, 11, 9, 5, 2, 2, 0},			/*	12cd		*/
	{0, 18, 22, 14, 11, 9, 6, 3, 2, 0},			/*	13cd		*/
	{0, 20, 17, 14, 10, 8, 4, 1, 0, 0},			/*	14cd		*/
	{0, 18, 15, 12, 9, 7, 4, 1, 0, 0},			/*	15cd		*/
	{0, 18, 14, 11, 8, 6, 4, 1, 0, 0},			/*	16cd		*/
	{0, 15, 14, 11, 8, 7, 5, 2, 1, 0},			/*	17cd		*/
	{0, 15, 13, 10, 7, 6, 4, 2, 2, 0},			/*	19cd		*/
	{0, 10, 12, 10, 7, 5, 3, 1, 0, 0},			/*	20cd		*/
	{0, 20, 12, 9, 7, 5, 3, 1, 0, 0},			/*	21cd		*/
	{0, 10, 12, 9, 6, 5, 3, 1, 0, 0},			/*	22cd		*/
	{0, 10, 11, 8, 6, 4, 3, 1, 1, 0},			/*	24cd		*/
	{0, 10, 11, 8, 5, 4, 3, 1, 1, 0},			/*	25cd		*/
	{0, 10, 10, 7, 5, 4, 2, 1, 1, 0},			/*	27cd		*/
	{0, 13, 9, 7, 5, 3, 3, 2, 1, 0},			/*	29cd		*/
	{0, 11, 8, 6, 4, 3, 2, 1, 1, 0},			/*	30cd		*/
	{0, 11, 8, 6, 4, 3, 2, 1, 0, 0},			/*	32cd		*/
	{0, 10, 8, 6, 4, 3, 2, 1, 1, 0},			/*	34cd		*/
	{0, 8, 7, 5, 3, 2, 2, 1, 0, 0},			/*	37cd		*/
	{0, 9, 6, 5, 3, 2, 2, 1, 1, 0},			/*	39cd		*/
	{0, 7, 7, 5, 3, 2, 2, 1, 1, 0},			/*	41cd		*/
	{0, 6, 6, 4, 3, 2, 2, 1, 1, 0},			/*	44cd		*/
	{0, 6, 5, 4, 2, 1, 1, 1, 1, 0},			/*	47cd		*/
	{0, 7, 5, 4, 2, 1, 1, 1, 1, 0},			/*	50cd		*/
	{0, 7, 4, 3, 2, 1, 1, 1, 1, 0},			/*	53cd		*/
	{0, 7, 5, 3, 2, 1, 1, 1, 1, 0},			/*	56cd		*/
	{0, 6, 5, 2, 1, 1, 1, 1, 1, 0},			/*	60cd		*/
	{0, 6, 4, 3, 1, 1, 1, 1, 1, 0},			/*	64cd		*/
	{0, 5, 3, 2, 1, 1, 1, 1, 1, 0},			/*	68cd		*/
	{0, 5, 3, 1, 1, 1, 1, 1, 1, 0},			/*	72cd		*/
	{0, 4, 2, 2, 1, 0, 2, 1, 1, 0},			/*	77cd		*/
	{0, 4, 3, 1, 1, 1, 1, 1, 1, 0},			/*	82cd		*/
	{0, 4, 3, 1, 1, 1, 1, 2, 0, 0},			/*	87cd		*/
	{0, 5, 4, 2, 0, 1, 1, 1, 0, 0},			/*	93cd		*/
	{0, 4, 2, 1, 1, 1, 2, 2, 0, 0},			/*	98cd		*/
	{0, 4, 2, 1, 1, 1, 2, 3, 1, 0},			/*	105cd	*/
	{0, 3, 2, 1, 0, 1, 2, 2, 1, 0},			/*	111cd	*/
	{0, 3, 2, 1, 1, 1, 2, 3, 1, 0},			/*	119cd	*/
	{0, 3, 1, 1, 1, 1, 1, 1, 0, 0},			/*	126cd	*/
	{0, 3, 1, 0, 0, 1, 1, 1, 0, 0},			/*	134cd	*/
	{0, 3, 1, 1, 0, 1, 0, 0, 1, 0},			/*	143cd	*/
	{0, 3, 0, 0, 0, 0, 0, 1, 0, 0},			/*	152cd	*/
	{0, 2, 1, 1, 0, 0, 0, 0, 0, 0},			/*	162cd	*/
	{0, 1, 0, 1, 0, -1, 0, 0, 0, 0},			/*	172cd	*/
	{0, 1, 0, 0, -1, 0, 0, 0, 0, 0},			/*	183cd	*/
	{0, 1, 0, 0, -1, 0, 0, 0, 0, 0},			/*	195cd	*/
	{0, 0, 0, 0, -1, 0, 0, 0, 0, 0},			/*	207cd	*/
	{0, 5, 0, 1, -1, 0, 0, 0, 0, 0},			/*	220cd	*/
	{0, 0, 0, 0, -1, 0, 0, 0, 0, 0},			/*	234cd	*/
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	249cd	*/
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	265cd	*/
	{0, 0, 0, 0, -1, 0, -1, 0, 0, 0},		/*	282cd	*/
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	300cd	*/
};

static int rgb_offset[MAX_GAMMA_CNT][RGB_COMPENSATION] = {
	/* R0 R3 R11 R23 R51 R87 R151 R203 R255(RGB)*/
	{0, 0, 0, 0, 0, 0, -10, 2, -6, -16, 0, -5, -10, 8, -15, -6, 3, -7, -4, 1, -2, -2, 0, -2, -1, -1, 0, 0, 0, 0},			/*	10cd	*/
	{0, 0, 0, 0, 0, 0, -12, -2, -9, -15, 3, -10, -6, 8, -9, -10, 0, -6, -5, 1, -3, -2, 0, -2, 0, 0, -1, 0, 0, 0},			/*	11cd	*/
	{0, 0, 0, 0, 0, 0, -10, 5, -11, -25, 1, 0, -5, 5, -9, -8, 0, -6, -5, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0},			/*	12cd	*/
	{0, 0, 0, 0, 0, 0, -8, -2, -6, -18, 4, -10, -18, 4, -10, -5, 0, -6, -4, 1, -3, -1, 0, -1, 0, 0, -1, 0, 0, 0},			/*	13cd	*/
	{0, 0, 0, 0, 0, 0, -15, 2, -10, -2, 3, -10, -16, 3, -8, -12, 2, -6, -4, 1, -2, -1, 0, -1, 0, 0, -1, 0, 0, 0},			/*	14cd	*/
	{0, 0, 0, 0, 0, 0, -15, 0, -6, -8, 6, -9, -7, 3, -8, -11, 2, -6, -3, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0},				/*	15cd	*/
	{0, 0, 0, 0, 0, 0, -12, 1, -8, -8, 6, -10, -7, 4, -8, -10, 2, -5, -3, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0},			/*	16cd	*/
	{0, 0, 0, 0, 0, 0, -12, 3, -1, -12, 5, -8, -10, 4, -8, -10, 2, -5, -3, 0, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0},			/*	17cd	*/
	{0, 0, 0, 0, 0, 0, -16, 3, -5, -5, 3, -10, -8, 4, -8, -8, 2, -4, -3, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0},			/*	19cd	*/
	{0, 0, 0, 0, 0, 0, -8, 6, -5, -13, 5, -12, -8, 0, -4, -8, 2, -4, -3, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0},			/*	20cd	*/
	{0, 0, 0, 0, 0, 0, -10, 1, -3, -10, 5, -10, -9, 3, -6, -8, 1, -4, -3, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0},			/*	21cd	*/
	{0, 0, 0, 0, 0, 0, -15, 0, -10, -10, 3, -8, -8, 3, -6, -8, 1, -4, -3, 0, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0},			/*	22cd	*/
	{0, 0, 0, 0, 0, 0, -15, 0, -12, -10, 6, -10, -6, 2, -6, -7, 1, -4, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	24cd	*/
	{0, 0, 0, 0, 0, 0, -18, 3, -12, -10, 5, -9, -5, 2, -6, -7, 1, -4, -2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	25cd	*/
	{0, 0, 0, 0, 0, 0, -15, 4, -10, -10, 5, -10, -6, 2, -5, -6, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 2},			/*	27cd	*/
	{0, 0, 0, 0, 0, 0, -14, 7, -10, -10, 2, -10, -5, 2, -4, -6, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 2},			/*	29cd	*/
	{0, 0, 0, 0, 0, 0, -10, 5, 0, -10, 5, -8, -7, 2, -4, -5, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 2},			/*	30cd	*/
	{0, 0, 0, 0, 0, 0, -14, 5, -8, -5, 4, -10, -9, 2, -4, -5, 1, -3, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 3},			/*	32cd	*/
	{0, 0, 0, 0, 0, 0, -20, 3, -10, -7, 3, -7, -7, 2, -4, -4, 1, -2, -1, 0, -1, 0, 0, 0, 0, 0, 0, 1, 0, 2},			/*	34cd	*/
	{0, 0, 0, 0, 0, 0, -15, 5, -12, -3, 4, -7, -9, 1, -4, -4, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2},			/*	37cd	*/
	{0, 0, 0, 0, 0, 0, -11, 4, -10, -5, 5, -6, -9, 1, -4, -3, 1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2},			/*	39cd	*/
	{0, 0, 0, 0, 0, 0, -15, 5, -10, -5, 2, -8, -9, 1, -4, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 2},			/*	41cd	*/
	{0, 0, 0, 0, 0, 0, -16, 5, -12, -5, 3, -6, -7, 1, -3, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1},		/*	44cd	*/
	{0, 0, 0, 0, 0, 0, -10, 6, -10, -7, 3, -5, -7, 1, -3, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 1},		/*	47cd	*/
	{0, 0, 0, 0, 0, 0, -12, 5, -11, -5, 3, -3, -6, 1, -2, -3, 0, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},		/*	50cd	*/
	{0, 0, 0, 0, 0, 0, -13, 5, -10, -8, -1, -4, -5, 1, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*	53cd	*/
	{0, 0, 0, 0, 0, 0, -9, 0, -9, -10, 1, -4, -5, 1, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	56cd	*/
	{0, 0, 0, 0, 0, 0, -13, -1, -10, -8, 0, -4, -5, 0, -2, -2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*	60cd	*/
	{0, 0, 0, 0, 0, 0, -10, 3, -9, -9, -1, -3, -5, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},			/*	64cd	*/
	{0, 0, 0, 0, 0, 0, -10, 1, -9, -6, 0, -2, -4, 0, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	68cd	*/
	{0, 0, 0, 0, 0, 0, -11, 3, -8, -6, 1, -2, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0},			/*	72cd	*/
	{0, 0, 0, 0, 0, 0, -10, 2, -8, -6, 0, -2, -3, 0, -1, -2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	77cd	*/
	{0, 0, 0, 0, 0, 0, -12, 1, -8, -6, 0, -2, -3, 0, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	82cd	*/
	{0, 0, 0, 0, 0, 0, -9, 2, -6, -6, 0, -2, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	87cd	*/
	{0, 0, 0, 0, 0, 0, -8, 2, -6, -6, -1, -2, -3, 0, -1, 0, 0, 0, -3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*	93cd	*/
	{0, 0, 0, 0, 0, 0, -10, 3, -5, -6, 0, -2, -2, 0, -1, -2, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*	98cd	*/
	{0, 0, 0, 0, 0, 0, -10, 3, -5, -5, 0, -1, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	105cd	*/
	{0, 0, 0, 0, 0, 0, -8, 3, -6, -4, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	111cd	*/
	{0, 0, 0, 0, 0, 0, -12, 2, -5, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},		/*	119cd	*/
	{0, 0, 0, 0, 0, 0, -8, 5, -5, -4, 0, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	126cd	*/
	{0, 0, 0, 0, 0, 0, -6, 2, -4, -5, 1, -1, -1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	134cd	*/
	{0, 0, 0, 0, 0, 0, -8, 3, -4, -3, 1, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	143cd	*/
	{0, 0, 0, 0, 0, 0, -8, 3, -3, -4, 2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	152cd	*/
	{0, 0, 0, 0, 0, 0, -5, 1, -2, -2, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	162cd	*/
	{0, 0, 0, 0, 0, 0, -6, 2, -2, -1, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	172cd	*/
	{0, 0, 0, 0, 0, 0, -5, 2, -2, -2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	183cd	*/
	{0, 0, 0, 0, 0, 0, -3, 1, -2, -2, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	195cd	*/
	{0, 0, 0, 0, 0, 0, -3, 2, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	207cd	*/
	{0, 0, 0, 0, 0, 0, -2, 2, -1, -1, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	220cd	*/
	{0, 0, 0, 0, 0, 0, -3, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	234cd	*/
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0},			/*	249cd	*/
	{0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	265cd	*/
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},			/*	282cd	*/
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},			/*	300cd	*/
};

const unsigned int vref_index[NUM_VREF] = {
	TBL_INDEX_V0,
	TBL_INDEX_V3,
	TBL_INDEX_V11,
	TBL_INDEX_V23,
	TBL_INDEX_V35,
	TBL_INDEX_V51,
	TBL_INDEX_V87,
	TBL_INDEX_V151,
	TBL_INDEX_V203,
	TBL_INDEX_V255,
};

const int vreg_element_max[NUM_VREF] = {
	0x0f,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0x1ff,
};

const struct v_constant fix_const[NUM_VREF] = {
	{.nu = 0,	.de = 860},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 72,	.de = 860},
};

const short vt_trans_volt[16] = {
	6451, 6361, 6271, 6181, 6091, 6001, 5911, 5821,
	5731, 5641, 5416, 5341, 5266, 5191, 5116, 5041,
};

const short v255_trans_volt[512] = {
	5911, 5904, 5896, 5889, 5881, 5874, 5866, 5859,
	5851, 5844, 5836, 5829, 5821, 5814, 5806, 5799,
	5791, 5784, 5776, 5769, 5761, 5754, 5746, 5739,
	5731, 5724, 5716, 5709, 5701, 5694, 5686, 5679,
	5671, 5664, 5656, 5649, 5641, 5634, 5626, 5619,
	5611, 5604, 5596, 5589, 5581, 5574, 5566, 5559,
	5551, 5544, 5536, 5529, 5521, 5514, 5506, 5499,
	5491, 5484, 5476, 5469, 5461, 5454, 5446, 5439,
	5431, 5424, 5416, 5409, 5401, 5394, 5386, 5379,
	5371, 5363, 5356, 5348, 5341, 5333, 5326, 5318,
	5311, 5303, 5296, 5288, 5281, 5273, 5266, 5258,
	5251, 5243, 5236, 5228, 5221, 5213, 5206, 5198,
	5191, 5183, 5176, 5168, 5161, 5153, 5146, 5138,
	5131, 5123, 5116, 5108, 5101, 5093, 5086, 5078,
	5071, 5063, 5056, 5048, 5041, 5033, 5026, 5018,
	5011, 5003, 4996, 4988, 4981, 4973, 4966, 4958,
	4951, 4943, 4936, 4928, 4921, 4913, 4906, 4898,
	4891, 4883, 4876, 4868, 4861, 4853, 4846, 4838,
	4831, 4823, 4816, 4808, 4801, 4793, 4786, 4778,
	4771, 4763, 4756, 4748, 4741, 4733, 4726, 4718,
	4711, 4703, 4696, 4688, 4681, 4673, 4666, 4658,
	4651, 4643, 4636, 4628, 4621, 4613, 4606, 4598,
	4591, 4583, 4576, 4568, 4561, 4553, 4546, 4538,
	4531, 4523, 4516, 4508, 4501, 4493, 4486, 4478,
	4471, 4463, 4456, 4448, 4441, 4433, 4426, 4418,
	4411, 4403, 4396, 4388, 4381, 4373, 4366, 4358,
	4351, 4343, 4336, 4328, 4321, 4313, 4306, 4298,
	4291, 4283, 4276, 4268, 4261, 4253, 4246, 4238,
	4231, 4223, 4216, 4208, 4201, 4193, 4186, 4178,
	4171, 4163, 4156, 4148, 4141, 4133, 4126, 4118,
	4111, 4103, 4096, 4088, 4081, 4073, 4066, 4058,
	4051, 4043, 4036, 4028, 4021, 4013, 4006, 3998,
	3991, 3983, 3976, 3968, 3961, 3953, 3946, 3938,
	3931, 3923, 3916, 3908, 3901, 3893, 3886, 3878,
	3871, 3863, 3856, 3848, 3841, 3833, 3826, 3818,
	3811, 3803, 3796, 3788, 3781, 3773, 3766, 3758,
	3751, 3743, 3736, 3728, 3721, 3713, 3706, 3698,
	3691, 3683, 3676, 3668, 3661, 3653, 3646, 3638,
	3631, 3623, 3616, 3608, 3601, 3593, 3586, 3578,
	3571, 3563, 3556, 3548, 3541, 3533, 3526, 3518,
	3511, 3503, 3496, 3488, 3481, 3473, 3466, 3458,
	3451, 3443, 3436, 3428, 3421, 3413, 3406, 3398,
	3391, 3383, 3376, 3368, 3361, 3353, 3346, 3338,
	3331, 3323, 3316, 3308, 3301, 3293, 3286, 3278,
	3271, 3263, 3256, 3248, 3241, 3233, 3226, 3218,
	3211, 3203, 3196, 3188, 3181, 3173, 3166, 3158,
	3151, 3143, 3136, 3128, 3121, 3113, 3106, 3098,
	3091, 3083, 3076, 3068, 3061, 3053, 3046, 3038,
	3031, 3023, 3016, 3008, 3001, 2993, 2986, 2978,
	2971, 2963, 2956, 2948, 2941, 2933, 2926, 2918,
	2911, 2903, 2896, 2888, 2881, 2873, 2866, 2858,
	2851, 2843, 2836, 2828, 2821, 2813, 2806, 2798,
	2791, 2783, 2776, 2768, 2761, 2753, 2746, 2738,
	2731, 2723, 2716, 2708, 2701, 2693, 2685, 2678,
	2670, 2663, 2655, 2648, 2640, 2633, 2625, 2618,
	2610, 2603, 2595, 2588, 2580, 2573, 2565, 2558,
	2550, 2543, 2535, 2528, 2520, 2513, 2505, 2498,
	2490, 2483, 2475, 2468, 2460, 2453, 2445, 2438,
	2430, 2423, 2415, 2408, 2400, 2393, 2385, 2378,
	2370, 2363, 2355, 2348, 2340, 2333, 2325, 2318,
	2310, 2303, 2295, 2288, 2280, 2273, 2265, 2258,
	2250, 2243, 2235, 2228, 2220, 2213, 2205, 2198,
	2190, 2183, 2175, 2168, 2160, 2153, 2145, 2138,
	2130, 2123, 2115, 2108, 2100, 2093, 2085, 2078,
};

const short v3_v203_trans_volt[256] = {
	205, 208, 211, 214, 218, 221, 224, 227,
	230, 234, 237, 240, 243, 246, 250, 253,
	256, 259, 262, 266, 269, 272, 275, 278,
	282, 285, 288, 291, 294, 298, 301, 304,
	307, 310, 314, 317, 320, 323, 326, 330,
	333, 336, 339, 342, 346, 349, 352, 355,
	358, 362, 365, 368, 371, 374, 378, 381,
	384, 387, 390, 394, 397, 400, 403, 406,
	410, 413, 416, 419, 422, 426, 429, 432,
	435, 438, 442, 445, 448, 451, 454, 458,
	461, 464, 467, 470, 474, 477, 480, 483,
	486, 490, 493, 496, 499, 502, 506, 509,
	512, 515, 518, 522, 525, 528, 531, 534,
	538, 541, 544, 547, 550, 554, 557, 560,
	563, 566, 570, 573, 576, 579, 582, 586,
	589, 592, 595, 598, 602, 605, 608, 611,
	614, 618, 621, 624, 627, 630, 634, 637,
	640, 643, 646, 650, 653, 656, 659, 662,
	666, 669, 672, 675, 678, 682, 685, 688,
	691, 694, 698, 701, 704, 707, 710, 714,
	717, 720, 723, 726, 730, 733, 736, 739,
	742, 746, 749, 752, 755, 758, 762, 765,
	768, 771, 774, 778, 781, 784, 787, 790,
	794, 797, 800, 803, 806, 810, 813, 816,
	819, 822, 826, 829, 832, 835, 838, 842,
	845, 848, 851, 854, 858, 861, 864, 867,
	870, 874, 877, 880, 883, 886, 890, 893,
	896, 899, 902, 906, 909, 912, 915, 918,
	922, 925, 928, 931, 934, 938, 941, 944,
	947, 950, 954, 957, 960, 963, 966, 970,
	973, 976, 979, 982, 986, 989, 992, 995,
	998, 1002, 1005, 1008, 1011, 1014, 1018, 1021,
};

const short int_tbl_v0_v3[2] = {
	341, 683,
};

const short int_tbl_v3_v11[7] = {
	128, 256, 384, 512, 640, 768, 896,
};

const short int_tbl_v11_v23[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const short int_tbl_v23_v35[11] = {
	85, 171, 256, 341, 427, 512, 597, 683, 768, 853, 939,
};

const short int_tbl_v35_v51[15] = {
	64, 128, 192, 256, 320, 384, 448, 512,
	576, 640, 704, 768, 832, 896, 960,
};

const short int_tbl_v51_v87[35] = {
	28, 57, 85, 114, 142, 171, 199, 228,
	256, 284, 313, 341, 370, 398, 427, 455,
	484, 512, 540, 569, 597, 626, 654, 683,
	711, 740, 768, 796, 825, 853, 882, 910,
	939, 967, 996,
};

const short int_tbl_v87_v151[63] = {
	16, 32, 48, 64, 80, 96, 112, 128,
	144, 160, 176, 192, 208, 224, 240, 256,
	272, 288, 304, 320, 336, 352, 368, 384,
	400, 416, 432, 448, 464, 480, 496, 512,
	528, 544, 560, 576, 592, 608, 624, 640,
	656, 672, 688, 704, 720, 736, 752, 768,
	784, 800, 816, 832, 848, 864, 880, 896,
	912, 928, 944, 960, 976, 992, 1008,
};

const short int_tbl_v151_v203[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

const short int_tbl_v203_v255[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

const int gamma_tbl[256] = {
	0, 7, 31, 75, 138, 224, 331, 461,
	614, 791, 992, 1218, 1468, 1744, 2045, 2372,
	2725, 3105, 3511, 3943, 4403, 4890, 5404, 5946,
	6516, 7114, 7740, 8394, 9077, 9788, 10528, 11297,
	12095, 12922, 13779, 14665, 15581, 16526, 17501, 18507,
	19542, 20607, 21703, 22829, 23986, 25174, 26392, 27641,
	28921, 30232, 31574, 32947, 34352, 35788, 37255, 38754,
	40285, 41847, 43442, 45068, 46727, 48417, 50140, 51894,
	53682, 55501, 57353, 59238, 61155, 63105, 65088, 67103,
	69152, 71233, 73348, 75495, 77676, 79890, 82138, 84418,
	86733, 89080, 91461, 93876, 96325, 98807, 101324, 103874,
	106458, 109075, 111727, 114414, 117134, 119888, 122677, 125500,
	128358, 131250, 134176, 137137, 140132, 143163, 146227, 149327,
	152462, 155631, 158835, 162074, 165348, 168657, 172002, 175381,
	178796, 182246, 185731, 189251, 192807, 196398, 200025, 203688,
	207385, 211119, 214888, 218693, 222533, 226410, 230322, 234270,
	238254, 242274, 246330, 250422, 254550, 258714, 262914, 267151,
	271423, 275732, 280078, 284459, 288878, 293332, 297823, 302351,
	306915, 311516, 316153, 320827, 325538, 330285, 335069, 339890,
	344748, 349643, 354575, 359544, 364549, 369592, 374672, 379789,
	384943, 390134, 395363, 400629, 405932, 411272, 416650, 422065,
	427517, 433007, 438534, 444099, 449702, 455342, 461020, 466735,
	472488, 478279, 484107, 489973, 495878, 501819, 507799, 513817,
	519872, 525966, 532098, 538267, 544475, 550721, 557005, 563327,
	569687, 576085, 582522, 588997, 595510, 602062, 608651, 615280,
	621946, 628652, 635395, 642177, 648998, 655857, 662755, 669691,
	676667, 683680, 690733, 697824, 704954, 712122, 719330, 726576,
	733861, 741186, 748549, 755951, 763391, 770871, 778390, 785948,
	793545, 801182, 808857, 816571, 824325, 832118, 839950, 847821,
	855732, 863682, 871671, 879700, 887768, 895875, 904022, 912208,
	920434, 928699, 937004, 945349, 953733, 962156, 970619, 979122,
	987665, 996247, 1004869, 1013531, 1022233, 1030974, 1039755, 1048576
};

const int gamma_multi_tbl[256] = {
	0, 2, 9, 22, 41, 65, 97, 135,
	180, 232, 291, 357, 430, 511, 599, 695,
	798, 910, 1028, 1155, 1290, 1433, 1583, 1742,
	1909, 2084, 2268, 2459, 2659, 2868, 3084, 3310,
	3543, 3786, 4037, 4296, 4565, 4842, 5127, 5422,
	5725, 6037, 6358, 6688, 7027, 7375, 7732, 8098,
	8473, 8857, 9250, 9652, 10064, 10485, 10915, 11354,
	11802, 12260, 12727, 13204, 13689, 14185, 14689, 15203,
	15727, 16260, 16803, 17355, 17917, 18488, 19069, 19659,
	20259, 20869, 21489, 22118, 22757, 23405, 24064, 24732,
	25410, 26098, 26795, 27503, 28220, 28947, 29685, 30432,
	31189, 31956, 32733, 33520, 34317, 35124, 35941, 36768,
	37605, 38452, 39309, 40177, 41054, 41942, 42840, 43748,
	44666, 45595, 46534, 47483, 48442, 49411, 50391, 51381,
	52382, 53392, 54413, 55445, 56486, 57539, 58601, 59674,
	60757, 61851, 62955, 64070, 65195, 66331, 67477, 68634,
	69801, 70979, 72167, 73366, 74575, 75795, 77026, 78267,
	79519, 80781, 82054, 83338, 84632, 85937, 87253, 88579,
	89916, 91264, 92623, 93992, 95372, 96763, 98165, 99577,
	101001, 102435, 103879, 105335, 106802, 108279, 109767, 111266,
	112776, 114297, 115829, 117372, 118925, 120490, 122065, 123652,
	125249, 126858, 128477, 130107, 131749, 133401, 135064, 136739,
	138424, 140121, 141828, 143547, 145277, 147017, 148769, 150532,
	152306, 154092, 155888, 157695, 159514, 161344, 163185, 165037,
	166900, 168775, 170661, 172558, 174466, 176385, 178316, 180258,
	182211, 184175, 186151, 188138, 190136, 192146, 194167, 196199,
	198242, 200297, 202363, 204441, 206529, 208630, 210741, 212864,
	214998, 217144, 219301, 221470, 223650, 225841, 228044, 230258,
	232484, 234721, 236970, 239230, 241501, 243785, 246079, 248385,
	250703, 253032, 255372, 257725, 260088, 262463, 264850, 267249,
	269658, 272080, 274513, 276958, 279414, 281882, 284361, 286852,
	289355, 291869, 294395, 296933, 299482, 302043, 304616, 307200
};

const unsigned char lookup_tbl[301] = {
	0, 18, 25, 30, 34, 38, 41, 44,
	47, 50, 52, 55, 57, 59, 61, 63,
	65, 67, 69, 71, 72, 74, 76, 77,
	79, 80, 82, 83, 85, 86, 87, 89,
	90, 91, 93, 94, 95, 96, 98, 99,
	100, 101, 102, 103, 104, 105, 107, 108,
	109, 110, 111, 112, 113, 114, 115, 116,
	117, 118, 119, 120, 121, 122, 122, 123,
	124, 125, 126, 127, 128, 129, 130, 130,
	131, 132, 133, 134, 134, 135, 136, 137,
	138, 139, 139, 140, 141, 142, 142, 143,
	144, 145, 146, 146, 147, 148, 149, 149,
	150, 151, 152, 152, 153, 154, 154, 155,
	156, 156, 157, 158, 159, 159, 160, 161,
	161, 162, 163, 163, 164, 165, 165, 166,
	167, 167, 168, 168, 169, 170, 170, 171,
	172, 172, 173, 173, 174, 175, 175, 176,
	176, 177, 178, 178, 179, 179, 180, 180,
	181, 182, 182, 183, 184, 184, 185, 185,
	186, 186, 187, 188, 188, 189, 189, 190,
	190, 191, 191, 192, 193, 193, 194, 194,
	195, 195, 196, 196, 197, 197, 198, 198,
	199, 200, 200, 201, 201, 202, 202, 203,
	203, 204, 204, 205, 205, 206, 206, 207,
	207, 208, 208, 209, 209, 210, 210, 211,
	211, 212, 212, 213, 213, 214, 214, 215,
	215, 216, 216, 217, 217, 217, 218, 218,
	219, 219, 220, 220, 221, 221, 222, 222,
	223, 223, 224, 224, 224, 225, 225, 226,
	226, 227, 227, 228, 228, 229, 229, 229,
	230, 230, 231, 231, 232, 232, 233, 233,
	234, 234, 234, 235, 235, 236, 236, 237,
	237, 237, 238, 238, 239, 239, 239, 240,
	240, 241, 241, 242, 242, 242, 243, 243,
	244, 244, 244, 245, 245, 246, 246, 246,
	247, 247, 248, 248, 249, 249, 249, 250,
	250, 251, 251, 252, 252, 252, 253, 253,
	253, 254, 254, 255, 255
};

static int calc_vt_volt(int gamma)
{
	int max;

	max = sizeof(vt_trans_volt) >> 2;
	if (gamma > max) {
		pr_warn("%s: exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(struct smart_dimming *dimming, int color)
{
	return MULTIPLE_VREGOUT;
}

static int calc_v3_volt(struct smart_dimming *dimming, int color)
{
	int ret, v11, gamma;

	gamma = dimming->gamma[V3][color];

	if (gamma > vreg_element_max[V3]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V3];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (MULTIPLE_VREGOUT << 10) -
		((MULTIPLE_VREGOUT - v11) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v11_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v23, gamma;

	gamma = dimming->gamma[V11][color];

	if (gamma > vreg_element_max[V11]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V11];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (vt << 10) - ((vt - v23) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v23_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v35, gamma;

	gamma = dimming->gamma[V23][color];

	if (gamma > vreg_element_max[V23]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V23];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (vt << 10) - ((vt - v35) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v35_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v51, gamma;

	gamma = dimming->gamma[V35][color];

	if (gamma > vreg_element_max[V35]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V35];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (vt << 10) - ((vt - v51) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v51_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v87, gamma;

	gamma = dimming->gamma[V51][color];

	if (gamma > vreg_element_max[V51]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V51];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (vt << 10) - ((vt - v87) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v87_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v151, gamma;

	gamma = dimming->gamma[V87][color];

	if (gamma > vreg_element_max[V87]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V87];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (vt << 10) -
		((vt - v151) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v151_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v203, gamma;

	gamma = dimming->gamma[V151][color];

	if (gamma > vreg_element_max[V151]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V151];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (vt << 10) - ((vt - v203) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v203_volt(struct smart_dimming *dimming, int color)
{
	int vt, ret, v255, gamma;

	gamma = dimming->gamma[V203][color];

	if (gamma > vreg_element_max[V203]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V203];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	vt = dimming->volt_vt[color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (vt << 10) - ((vt - v255) * (int)v3_v203_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v255_volt(struct smart_dimming *dimming, int color)
{
	int ret, gamma;

	gamma = dimming->gamma[V255][color];

	if (gamma > vreg_element_max[V255]) {
		pr_warn("%s : gamma overflow : %d\n", __func__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		pr_warn("%s : gamma undeflow : %d\n", __func__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int calc_inter_v0_v3(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v0, v3, ratio;

	ratio = (int)int_tbl_v0_v3[gray];

	v0 = dimming->volt[TBL_INDEX_V0][color];
	v3 = dimming->volt[TBL_INDEX_V3][color];

	ret = (v0 << 10) - ((v0 - v3) * ratio);

	return ret >> 10;
}

static int calc_inter_v3_v11(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v3, v11, ratio;

	ratio = (int)int_tbl_v3_v11[gray];
	v3 = dimming->volt[TBL_INDEX_V3][color];
	v11 = dimming->volt[TBL_INDEX_V11][color];

	ret = (v3 << 10) - ((v3 - v11) * ratio);

	return ret >> 10;
}

static int calc_inter_v11_v23(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v11, v23, ratio;

	ratio = (int)int_tbl_v11_v23[gray];
	v11 = dimming->volt[TBL_INDEX_V11][color];
	v23 = dimming->volt[TBL_INDEX_V23][color];

	ret = (v11 << 10) - ((v11 - v23) * ratio);

	return ret >> 10;
}

static int calc_inter_v23_v35(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v23, v35, ratio;

	ratio = (int)int_tbl_v23_v35[gray];
	v23 = dimming->volt[TBL_INDEX_V23][color];
	v35 = dimming->volt[TBL_INDEX_V35][color];

	ret = (v23 << 10) - ((v23 - v35) * ratio);

	return ret >> 10;
}

static int calc_inter_v35_v51(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v35, v51, ratio;

	ratio = (int)int_tbl_v35_v51[gray];
	v35 = dimming->volt[TBL_INDEX_V35][color];
	v51 = dimming->volt[TBL_INDEX_V51][color];

	ret = (v35 << 10) - ((v35 - v51) * ratio);

	return ret >> 10;
}

static int calc_inter_v51_v87(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v51, v87, ratio;

	ratio = (int)int_tbl_v51_v87[gray];
	v51 = dimming->volt[TBL_INDEX_V51][color];
	v87 = dimming->volt[TBL_INDEX_V87][color];

	ret = (v51 << 10) - ((v51 - v87) * ratio);

	return ret >> 10;
}

static int calc_inter_v87_v151(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v87, v151, ratio;

	ratio = (int)int_tbl_v87_v151[gray];
	v87 = dimming->volt[TBL_INDEX_V87][color];
	v151 = dimming->volt[TBL_INDEX_V151][color];

	ret = (v87 << 10) - ((v87 - v151) * ratio);

	return ret >> 10;
}

static int calc_inter_v151_v203(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v151, v203, ratio;

	ratio = (int)int_tbl_v151_v203[gray];
	v151 = dimming->volt[TBL_INDEX_V151][color];
	v203 = dimming->volt[TBL_INDEX_V203][color];

	ret = (v151 << 10) - ((v151 - v203) * ratio);

	return ret >> 10;
}

static int calc_inter_v203_v255(struct smart_dimming *dimming,
		int gray, int color)
{
	int ret = 0;
	int v203, v255, ratio;

	ratio = (int)int_tbl_v203_v255[gray];
	v203 = dimming->volt[TBL_INDEX_V203][color];
	v255 = dimming->volt[TBL_INDEX_V255][color];

	ret = (v203 << 10) - ((v203 - v255) * ratio);

	return ret >> 10;
}

void panel_read_gamma(struct smart_dimming *dimming, const unsigned char *data)
{
	int i, j;
	int temp;
	u8 pos = 0;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((data[pos] & 0x01) ? -1 : 1) * data[pos+1];
		dimming->gamma[V255][j] = ref_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos += 2;
	}

	for (i = V203; i >= V0; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((data[pos] & 0x80) ? -1 : 1) *
					(data[pos] & 0x7f);
			dimming->gamma[i][j] = ref_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}
}

int panel_generate_volt_tbl(struct smart_dimming *dimming)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;
	int calc_seq[NUM_VREF] = {
		V0, V255, V203, V151, V87, V51, V35, V23, V11, V3};
	int (*calc_volt_point[NUM_VREF])(struct smart_dimming *, int) = {
		calc_v0_volt,
		calc_v3_volt,
		calc_v11_volt,
		calc_v23_volt,
		calc_v35_volt,
		calc_v51_volt,
		calc_v87_volt,
		calc_v151_volt,
		calc_v203_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct smart_dimming *, int, int)  = {
		NULL,
		calc_inter_v0_v3,
		calc_inter_v3_v11,
		calc_inter_v11_v23,
		calc_inter_v23_v35,
		calc_inter_v35_v51,
		calc_inter_v51_v87,
		calc_inter_v87_v151,
		calc_inter_v151_v203,
		calc_inter_v203_v255,
	};
#ifdef SMART_DIMMING_DEBUG
	long temp[CI_MAX];
#endif
	for (i = 0; i < CI_MAX; i++)
		dimming->volt_vt[i] =
			calc_vt_volt(dimming->gamma[VT][i]);

	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF; j++) {
		seq = calc_seq[j];
		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
				dimming->volt[index][i] =
					calc_volt_point[seq](dimming, i);
		}
	}

	index = 0;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_index[index]) {
			index++;
			continue;
		}
		gray = (i - vref_index[index - 1]) - 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_inter_volt[index] != NULL)
				dimming->volt[i][j] =
				calc_inter_volt[index](dimming, gray, j);
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("============= VT Voltage ===============\n");
	for (i = 0; i < CI_MAX; i++)
		temp[i] = dimming->volt_vt[i] >> 10;

	pr_info("R : %d : %ld G : %d : %ld B : %d : %ld.\n",
				dimming->gamma[VT][0], temp[0],
				dimming->gamma[VT][1], temp[1],
				dimming->gamma[VT][2], temp[2]);

	pr_info("=================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		for (j = 0; j < CI_MAX; j++)
			temp[j] = dimming->volt[i][j] >> 10;

		pr_info("V%d R : %d : %ld G : %d : %ld B : %d : %ld\n", i,
					dimming->volt[i][0], temp[0],
					dimming->volt[i][1], temp[1],
					dimming->volt[i][2], temp[2]);
	}
#endif
	return ret;
}


static int lookup_volt_index(struct smart_dimming *dimming, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray >> 20;
	index = (int)lookup_tbl[temp];
#ifdef SMART_DIMMING_DEBUG
	pr_info("============== look up index ================\n");
	pr_info("gray : %d : %d, index : %d\n", gray, temp, index);
#endif
	exit = 1;
	i = 0;
	while (exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_GAMMA)
			index_h = MAX_GAMMA;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];

		if (cnt_l + cnt_h)
			exit = 0;
		i++;
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("base index : %d, cnt : %d\n",
			lookup_tbl[index_l], cnt_l + cnt_h);
#endif
	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;
	temp = gamma_multi_tbl[index] << 10;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
	pr_info("temp : %d, gray : %d, p_delta : %d\n", temp, gray, p_delta);
#endif
	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_multi_tbl[index + i] << 10;
		if (gray > temp)
			delta = gray - temp;
		else
			delta = temp - gray;
#ifdef SMART_DIMMING_DEBUG
		pr_info("temp : %d, gray : %d, delta : %d\n",
				temp, gray, delta);
#endif
		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		}
	}
#ifdef SMART_DIMMING_DEBUG
	pr_info("ret : %d\n", ret);
#endif
	return ret;
}

static int calc_reg_v3(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V3][color] - MULTIPLE_VREGOUT;
	t2 = dimming->look_volt[V11][color] - MULTIPLE_VREGOUT;

	ret = ((t1 * fix_const[V3].de) / t2) - fix_const[V3].nu;

	return ret;
}

static int calc_reg_v11(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V11][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V23][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V11].de)) / t2) - fix_const[V11].nu;

	return ret;
}

static int calc_reg_v23(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V23][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V35][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V23].de)) / t2) - fix_const[V23].nu;

	return ret;
}

static int calc_reg_v35(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V35][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V51][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V35].de)) / t2) - fix_const[V35].nu;

	return ret;
}

static int calc_reg_v51(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V51][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V87][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V51].de)) / t2) - fix_const[V51].nu;

	return ret;
}

static int calc_reg_v87(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V87][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V151][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V87].de)) / t2) - fix_const[V87].nu;

	return ret;
}

static int calc_reg_v151(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V151][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V203][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V151].de)) / t2) - fix_const[V151].nu;

	return ret;
}

static int calc_reg_v203(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1, t2;

	t1 = dimming->look_volt[V203][color] - dimming->volt_vt[color];
	t2 = dimming->look_volt[V255][color] - dimming->volt_vt[color];

	ret = (((t1) * (fix_const[V203].de)) / t2) - fix_const[V203].nu;

	return ret;
}

static int calc_reg_v255(struct smart_dimming *dimming, int color)
{
	int ret;
	int t1;

	t1 = MULTIPLE_VREGOUT - dimming->look_volt[V255][color];

	ret = ((t1 * fix_const[V255].de) / MULTIPLE_VREGOUT) -
			fix_const[V255].nu;

	return ret;
}

int panel_get_gamma(struct smart_dimming *dimming,
				int index_br, unsigned char *result)
{
	int i, j;
	int ret = 0;
	int gray, index, shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int br;
	int *color_shift_table = NULL;
	int (*calc_reg[NUM_VREF])(struct smart_dimming *, int)  = {
		NULL,
		calc_reg_v3,
		calc_reg_v11,
		calc_reg_v23,
		calc_reg_v35,
		calc_reg_v51,
		calc_reg_v87,
		calc_reg_v151,
		calc_reg_v203,
		calc_reg_v255,
	};

	br = ref_cd_tbl[index_br];

	if (br > MAX_GAMMA)
		br = MAX_GAMMA;

	for (i = V3; i < NUM_VREF; i++) {
		/* get reference shift value */
		shift = gradation_shift[index_br][i];
		gray = gamma_tbl[vref_index[i]] * br;
		index = lookup_volt_index(dimming, gray);
		index = index + shift;
#ifdef SMART_DIMMING_DEBUG
		pr_info("index : %d\n", index);
#endif
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				dimming->look_volt[i][j] =
					dimming->volt[index][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("volt : %d : %d\n",
					dimming->look_volt[i][j],
					dimming->look_volt[i][j] >> 10);
#endif
			}
		}
	}

	for (i = V3; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;
				color_shift_table = rgb_offset[index_br];
				c_shift = color_shift_table[index];
				gamma_int[i][j] =
					(calc_reg[i](dimming, j) + c_shift) -
					dimming->mtp[i][j];
#ifdef SMART_DIMMING_DEBUG
				pr_info("gamma : %d, shift : %d\n",
						gamma_int[i][j], c_shift);
#endif
				if (gamma_int[i][j] >= vreg_element_max[i])
					gamma_int[i][j] = vreg_element_max[i];
				if (gamma_int[i][j] < 0)
					gamma_int[i][j] = 0;
			}
		}
	}

	for (j = 0; j < CI_MAX; j++)
		gamma_int[VT][j] = dimming->gamma[VT][j] - dimming->mtp[VT][j];

	index = 0;

	for (i = V255; i >= VT; i--) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] =
					gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff;
			} else {
				result[index++] =
					(unsigned char)gamma_int[i][j];
			}
		}
	}

	return ret;
}

