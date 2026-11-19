#ifndef VDDA_LIB
#define VDDA_LIB

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"

//*****************************************************************************
void my_ADC_Vref_Init(void);
uint16_t my_Get_Supply_Voltage_mV(void);
//*****************************************************************************

#endif
