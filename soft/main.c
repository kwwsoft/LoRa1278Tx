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
	//init rtc for sleep and date/time
	uint8_t retcode = my_RTC_Clock_Init();
	if (retcode == ERROR_OK){
		#ifdef DEBUG_CODE
			my_usart_send_buffer("Start ERROR_RTC_LSE_OK\r\n");
		#endif
	}
	else{
		my_led_init_and_OFF();
		led_on;
		#ifdef DEBUG_CODE
			switch(retcode){
				case ERROR_RTC_LSE_Failed_LSEConfig:
					my_usart_send_buffer("Start ERROR_RTC_LSE_Failed_LSEConfig\r\n");
					break;
				case ERROR_RTC_LSE_Failed_ENABLE:
					my_usart_send_buffer("Start ERROR_RTC_LSE_Failed_ENABLE\r\n");
					break;
				case ERROR_RTC_LSE_Failed_SetPrescaler:
					my_usart_send_buffer("Start ERROR_RTC_LSE_Failed_SetPrescaler\r\n");
					break;
				case ERROR_RTC_LSI_Failed_ENABLE:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_ENABLE\r\n");
					break;
				case ERROR_RTC_LSI_Failed_RTCCLKConfig:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_RTCCLKConfig\r\n");
					break;
				case ERROR_RTC_LSI_Failed_SetPrescaler:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_SetPrescaler\r\n");
					break;
				case ERROR_RTC_LSI_Failed_SetPrescaler1:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_SetPrescaler1\r\n");
					break;
				case ERROR_RTC_Reset_Magic:
					my_usart_send_buffer("Start ERROR_RTC_Reset_Magic\r\n");
					break;
				default:
					break;
		}
		#endif
	}
	//init delay lib
	my_delay_TIM2_init();
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
		

		// 2. Виконуємо корисну роботу
		// ...

		// 3. Деініціалізуємо датчики та переводимо всі порти в режим низького споживання
		// Sensors_DeInit();
		//my_PrepareForStop();

		// 4. Засинаємо точно до початку наступної години (XX:00:00)
		//my_RTC_EnterStopUntilNextHour();

		
				
				// 1. Ініціалізуємо потрібну для роботи периферію (GPIO, LED, датчики, UART/I2C)
		//my_led_init_and_OFF();
		// Sensors_Init();

	
	};
	

//	return 0;
}
