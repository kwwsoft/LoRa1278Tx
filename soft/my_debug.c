#include "stm32f10x_rtc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_adc.h"
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include "my_global.h"
#include "my_debug.h"
#include "my_usart.h"
#include "my_rtc.h"
//*****************************************************************************
#ifdef DEBUG_CODE
//*****************************************************************************
void	myDebugInitUart(){
	my_usart_init();
	my_usart_send_buffer("Start application\r\n");
}
//*****************************************************************************
void myDebugSendBuffer(char *pb){
	my_usart_send_buffer(pb);
}
//*****************************************************************************
void myDebugRTCFailed(uint8_t retcode){
		my_led_init_and_OFF();
		led_on;
			switch(retcode){
				case ERROR_OK:
					my_usart_send_buffer("Start ERROR_RTC_LSE_OK\r\n");
					break;
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
}
//*****************************************************************************
void myDebugShowRTCDateTime(){
RTC_DateTime_t now;
		char buffer[80] = {'\0'};
		my_RTC_GetDateTime(&now);
		sprintf(buffer, "%02d.%02d.%04d  %02d:%02d:%02d\r\n",
		now.day, now.month, now.year, now.hour, now.min, now.sec);
		my_usart_send_buffer(buffer);
}
//*****************************************************************************






#endif
//*****************************************************************************

