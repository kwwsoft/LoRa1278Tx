#ifndef MY_GLOBAL_H
#define MY_GLOBAL_H

#include "stm32f10x.h"

//*****************************************************************************
#define led_on 	(GPIOC->BRR = GPIO_Pin_13)  //Bit_RESET mapping
#define led_off (GPIOC->BSRR = GPIO_Pin_13) //bit_set mapping
//********************************************************************************************
//a sign that the watch is already running
#define RTC_BKP_MAGIC_VALUE 0xA5A5
//*****************************************************************************
//ERROR values
#define ERROR_OK									0
#define ERROR_RTC_LSE_Failed			1
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



#endif
