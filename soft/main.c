//*****************************************************************************
#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
//#include <string.h>
//#include <stdio.h>
#include "my_global.h"
#include "my_debug.h"
#include "my_tim2_delay.h"
#include "my_usart.h"
#include "my_rtc.h"
#include "my_i2c_bmp280.h"
//*****************************************************************************
uint8_t bmp280_ready = 0;
uint8_t retcode;
//*****************************************************************************
/*
	All functions starting with `myDebug...` only work when #define DEBUG_CODE` is set; 
	otherwise, the compiler removes them from the release build.
*/
//*****************************************************************************
int main(){
	RTC_DateTime_t now;
	//remap PA15 and PB3 from JTAG to normal pins
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
	//
	my_led_init_and_OFF();
	my_led_deinit();
	// Дозволити відлагодження (ST-Link) у режимах SLEEP, STOP та STANDBY
//	#ifdef DEBUG_CODE
		DBGMCU->CR |= DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY;
//	#endif
	//init usart module
	myDebugInitUart();
	//init rtc for sleep and date/time
	retcode = my_RTC_Clock_Init();
	myDebugRTCFailed(retcode);
	//init delay lib
	my_delay_TIM2_init();
	//read date
	my_RTC_GetDateTime(&now);
	//if date is wrong - set compilation date/time
	//if (now.year < 2026){
	my_RTC_SetCompileTime();
	myDebugShowRTCDateTime();

	
	//i2c_init();

	
	

	
	while(1){
		//Sensor not initialized – initial setup required
		if (!bmp280_ready){
			//retcode = BMP280_Init();
		}
	
		
		
		
		
		
		
		
		// 2. Виконуємо корисну роботу
		// ...

		// 3. Деініціалізуємо датчики та переводимо всі порти в режим низького споживання
		// Sensors_DeInit();
		//my_PrepareForStop();

		// 4. Засинаємо точно до початку наступної години (XX:00:00)
		//my_RTC_EnterStopUntilNextHour();
	
		my_delay_ms(2000);
		
				
				// 1. Ініціалізуємо потрібну для роботи периферію (GPIO, LED, датчики, UART/I2C)
		//my_led_init_and_OFF();
		// Sensors_Init();

	
	};
	

//	return 0;
}
