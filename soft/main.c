#define DEBUG_CODE
//*****************************************************************************
#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include <string.h>
#include <stdio.h>
#include "my_global.h"
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
	my_led_init_and_OFF();
	my_led_deinit();
	// Дозволити відлагодження (ST-Link) у режимах SLEEP, STOP та STANDBY
	#ifdef DEBUG_CODE
		DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY;
	#endif
	//init usart module
	#ifdef DEBUG_CODE
		my_usart_init();
	#endif
	#ifdef DEBUG_CODE
		my_usart_send_buffer("Start application\r\n");
	#endif
	//init delay lib
	my_delay_TIM2_init();
	//init rtc for sleep and date/time
	if (my_RTC_Clock_Init() != ERROR_OK){
		my_led_init_and_OFF();
		led_on;
		#ifdef DEBUG_CODE
			my_usart_send_buffer("Start ERROR_RTC_LSE_Failed\r\n");
		#endif
	}
	else{
		#ifdef DEBUG_CODE
			my_usart_send_buffer("Start ERROR_RTC_LSE_OK\r\n");
		#endif
	}
	//read date
	my_RTC_GetDateTime(&now);
	//if date is wrong - set compilation date/time
	//if (now.year < 2026){
	my_RTC_SetCompileTime();
	//}
	#ifdef DEBUG_CODE
		char buffer[80] = {'\0'};
		my_RTC_GetDateTime(&now);
		sprintf(buffer, "%02d.%02d.%04d  %02d:%02d:%02d\r\n",
		now.day, now.month, now.year, now.hour, now.min, now.sec);
		my_usart_send_buffer(buffer);
	#endif

	


	
	

	
	while(1){
	
	};
	

//	return 0;
}
