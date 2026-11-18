#include "my_tim2_delay.h"
//*****************************************************************************
volatile uint8_t f_timer_2_end = 0;
//*****************************************************************************
void my_delay_TIM2_init(void){
	TIM_TimeBaseInitTypeDef TIMER_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_TimeBaseStructInit(&TIMER_InitStructure);

	TIMER_InitStructure.TIM_CounterMode = TIM_CounterMode_Down;
	TIMER_InitStructure.TIM_Prescaler = 72 - 1;
	TIMER_InitStructure.TIM_Period = 1;
	TIM_TimeBaseInit(TIM2, &TIMER_InitStructure);

	TIM2->CR1 |= TIM_CR1_URS; // Переривання тільки при переповненні, без реакції на UG

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}
//*****************************************************************************
void TIM2_IRQHandler(void){
	if (TIM2->SR & TIM_SR_UIF){
		TIM2->SR &= ~TIM_SR_UIF;
		f_timer_2_end = 1;
		TIM_Cmd(TIM2, DISABLE);
		TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
	}
}
//*****************************************************************************
void my_delay_us(uint16_t n_usec){
	if (n_usec <= 4) return;
	f_timer_2_end = 0;

	TIM2->PSC = 72 - 1;
	TIM2->ARR = n_usec - 4;
	TIM2->EGR = TIM_EGR_UG;
	TIM2->SR = 0;

	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM2, ENABLE);

	while(f_timer_2_end == 0);
}
//*****************************************************************************
void my_delay_ms(uint32_t n_msec){
	while (n_msec--){
		my_delay_us(1000);
	}
}
//*****************************************************************************

