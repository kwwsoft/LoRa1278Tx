#include "stm32f10x_rtc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>
#include "my_global.h"

//*****************************************************************************
void my_led_init_and_OFF(){
	GPIO_InitTypeDef port;
	//pc13
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	port.GPIO_Pin = GPIO_Pin_13;
	port.GPIO_Speed = GPIO_Speed_50MHz;
	port.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(GPIOC, &port);
	led_off;
}
//********************************************************************************************
void my_led_deinit(){
	//pc13
	led_off;
	GPIO_DeInit(GPIOC);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, DISABLE);
}
//********************************************************************************************
