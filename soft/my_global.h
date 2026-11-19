#ifndef MY_GLOBAL_H
#define MY_GLOBAL_H

//define global debug mode
#define DEBUG_CODE


#include "stm32f10x.h"

//*****************************************************************************
#define led_on 	(GPIOC->BRR = GPIO_Pin_13)  //Bit_RESET mapping
#define led_off (GPIOC->BSRR = GPIO_Pin_13) //bit_set mapping
//********************************************************************************************
//a sign that the watch is already running
#define RTC_BKP_MAGIC_VALUE 0xA5A5
//*****************************************************************************
//ERROR values
#define ERROR_OK															0
//*****************************************************************************
//ERROR values RTC
#define ERROR_RTC_LSE_Failed_LSEConfig				1
#define ERROR_RTC_LSE_Failed_ENABLE						2
#define ERROR_RTC_LSE_Failed_SetPrescaler			3
#define ERROR_RTC_LSI_Failed_ENABLE						4
#define ERROR_RTC_LSI_Failed_RTCCLKConfig			5
#define ERROR_RTC_LSI_Failed_SetPrescaler			6
#define ERROR_RTC_LSI_Failed_SetPrescaler1		7
#define ERROR_RTC_Reset_Magic									8
//*****************************************************************************
//ERROR values BMP280
#define ERROR_BMP280_ID_READ									1
#define ERROR_BMP280_ID_DATA									2
#define ERROR_BMP280_RESET										3
#define ERROR_BMP280_READ_CALIBR							4
#define ERROR_BMP280_IIR_OFF									5
#define ERROR_BMP280_SLEEP										6
//*****************************************************************************
// Проста структура для зручної роботи з датою та часом
typedef struct {
	uint16_t year;   // Наприклад, 2026
	uint8_t  month;  // 1..12
	uint8_t  day;    // 1..31
	uint8_t  hour;   // 0..23
	uint8_t  min;    // 0..59
	uint8_t  sec;    // 0..59
	uint8_t  wday;   // День тижня: 0 = Нд, 1 = Пн .. 6 = Сб
} RTC_DateTime_t;
//********************************************************************************************
//The LED turns on if any function returns an error during operation. 
//It turns off when entering sleep mode, and also during initialization.
void my_led_init_and_OFF();
//disable port clocking for the LED
void my_led_deinit();
//********************************************************************************************
void my_PrepareForStop(void);
//********************************************************************************************


#endif
