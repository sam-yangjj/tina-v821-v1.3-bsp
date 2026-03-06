#ifndef __STK_AXES_SIM_H__
#define __STK_AXES_SIM_H__

//#include <stdint.h>

typedef enum { BIT_12 = 0, BIT_16 = 1 } bit_count;

//mapping register
typedef enum {
	SIM_RANGE_2G = 0x03,
	SIM_RANGE_4G = 0x05,
	SIM_RANGE_8G = 0x08,
	SIM_RANGE_16G = 0x0C
} sim_range;

int stk_axes_sim(short *acc_data);
void stk_axes_sim_reset(void);
int stk_axes_sim_init(unsigned char check_count, bit_count bit,
		      sim_range range);
#endif /* __STK_AXES_SIM_H__ */
