#ifndef TIM2DELAY_LIB
#define TIM2DELAY_LIB

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"

void my_delay_TIM2_init(void);
void my_delay_us(uint16_t n_usec);
void my_delay_ms(uint32_t n_msec);

#endif
