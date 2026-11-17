#define DEBUG_CODE
//*****************************************************************************
#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include "my_tim2_delay.h"
#include "my_usart.h"
#include "my_rtc.h"
//*****************************************************************************


RTC_DateTime_t now;
//*****************************************************************************
int main(){
	//remap PA15 and PB3 from JTAG to normal pins
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
	//
	// Дозволити відлагодження (ST-Link) у режимах SLEEP, STOP та STANDBY
	#ifdef DEBUG_CODE
		DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY;
	#endif
	//init usart module
	my_usart_init();
	#ifdef DEBUG_CODE
		my_usart_send_buffer("\r\n--- Start application ---\r\n");
	#endif
	//init delay lib
	my_delay_TIM2_init();
	//init rtc for sleep and date/time
	my_RTC_Clock_Init();
	//read date
	my_RTC_GetDateTime(&now);
	//if date is wrong - set compilation date/time
	if (now.year < 2026){
		my_RTC_SetCompileTime();
	}
	


	
	

	
	while(1){
	
	};
	

//	return 0;
}
