#ifndef MY_RTC_H
#define MY_RTC_H

#include "stm32f10x.h"
#include <time.h>

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

// Ініціалізація RTC на зовнішньому кварці 32.768 кГц (LSE)
// Повертає 0 при успіху, 1 при помилці запуску кварцу (перехід на LSI)
uint8_t my_RTC_Clock_Init(void);

// Встановлення дати та часу
void my_RTC_SetDateTime(const RTC_DateTime_t *dt);

// Отримання поточної дати та часу
void my_RTC_GetDateTime(RTC_DateTime_t *dt);

// Сон у режимі STOP Mode на вказану кількість секунд без втрати ходу годинника
void my_RTC_EnterStopSeconds(uint32_t seconds);

// Встановлення дати та часу з моменту компіляції прошивки
void my_RTC_SetCompileTime(void);


#endif
