#ifndef MY_RTC_H
#define MY_RTC_H

#include "stm32f10x.h"
#include <time.h>
#include "my_global.h"

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
