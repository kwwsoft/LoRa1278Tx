#include "my_rtc.h"
#include "stm32f10x_rtc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>
#include "my_global.h"

// Перевірка на високосний рік
static uint8_t is_leap_year(uint16_t year){
	return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

//*****************************************************************************
uint8_t my_RTC_Clock_Init(void){
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	uint32_t timeout = 0x00400000;
	uint8_t lse_failed = ERROR_OK;

	// 1. Вмикаємо тактування PWR та BKP і дозволяємо доступ до Backup-домену
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);

	// 2. Перевіряємо, чи RTC вже був налаштований раніше (щоб не збити час при Reset)
	if (BKP_ReadBackupRegister(BKP_DR1) != RTC_BKP_MAGIC_VALUE){
		BKP_DeInit();

		// Запускаємо зовнішній кварц 32.768 кГц (LSE)
		RCC_LSEConfig(RCC_LSE_ON);
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET){
			if (--timeout == 0){
				lse_failed = ERROR_RTC_LSE_Failed;
				break;
			}
		}

		if (!lse_failed){
			RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
			RCC_RTCCLKCmd(ENABLE);
			RTC_WaitForSynchro();
			RTC_WaitForLastTask();
			// Дільник 32768 -> рівно 1 тік на секунду
			RTC_SetPrescaler(32768 - 1);
			RTC_WaitForLastTask();
		} else {
			// Резервний варіант на LSI (40 кГц), якщо кварц 32.768 кГц пошкоджений
			RCC_LSICmd(ENABLE);
			while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);
			RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
			RCC_RTCCLKCmd(ENABLE);
			RTC_WaitForSynchro();
			RTC_WaitForLastTask();
			RTC_SetPrescaler(40000 - 1);
			RTC_WaitForLastTask();
		}

		// Записуємо мітку, що годинник ініціалізовано
		BKP_WriteBackupRegister(BKP_DR1, RTC_BKP_MAGIC_VALUE);
	} else {
		// Годинник вже йде — просто чекаємо синхронізації регістрів
		RTC_WaitForSynchro();
		RTC_WaitForLastTask();
	}

	// 3. Дозволяємо переривання будильника RTC (потрібно для виходу зі сну)
	RTC_ITConfig(RTC_IT_ALR, ENABLE);
	RTC_WaitForLastTask();

	// 4. Налаштовуємо EXTI Line 17 для пробудження зі STOP Mode
	EXTI_ClearITPendingBit(EXTI_Line17);
	EXTI_InitStructure.EXTI_Line = EXTI_Line17;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = RTCAlarm_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	return lse_failed;
}

//*****************************************************************************
void RTCAlarm_IRQHandler(void){
	if (RTC_GetITStatus(RTC_IT_ALR) != RESET){
		EXTI_ClearITPendingBit(EXTI_Line17);
		RTC_ClearITPendingBit(RTC_IT_ALR);
		RTC_WaitForLastTask();
	}
}

//*****************************************************************************
void my_RTC_SetDateTime(const RTC_DateTime_t *dt){
	const uint16_t days_before_month[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
	uint32_t days = 0;

	if (dt->year < 1970 || dt->month < 1 || dt->month > 12 || dt->day < 1 || dt->day > 31){
		return;
	}

	for (uint16_t y = 1970; y < dt->year; y++){
		days += is_leap_year(y) ? 366 : 365;
	}

	days += days_before_month[dt->month - 1];
	if (dt->month > 2 && is_leap_year(dt->year)){
		days += 1;
	}

	days += (dt->day - 1);

	uint32_t timestamp = days * 86400UL + dt->hour * 3600UL + dt->min * 60UL + dt->sec;

	RTC_WaitForLastTask();
	RTC_SetCounter(timestamp);
	RTC_WaitForLastTask();
}

//*****************************************************************************
void my_RTC_GetDateTime(RTC_DateTime_t *dt){
	const uint8_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	uint32_t timestamp = RTC_GetCounter();

	uint32_t days = timestamp / 86400UL;
	uint32_t rem  = timestamp % 86400UL;

	dt->hour = rem / 3600UL;
	rem %= 3600UL;
	dt->min  = rem / 60UL;
	dt->sec  = rem % 60UL;

	// 1 січня 1970 року був четвергом (4)
	dt->wday = (days + 4) % 7;

	uint16_t year = 1970;
	while (1){
		uint16_t days_in_year = is_leap_year(year) ? 366 : 365;
		if (days < days_in_year) break;
		days -= days_in_year;
		year++;
	}
	dt->year = year;

	uint8_t month = 0;
	while (1){
		uint8_t dim = days_in_month[month];
		if (month == 1 && is_leap_year(year)) dim = 29;
		if (days < dim) break;
		days -= dim;
		month++;
	}
	dt->month = month + 1;
	dt->day   = days + 1;
}

//*****************************************************************************
void my_RTC_EnterStopSeconds(uint32_t seconds){
	if (seconds == 0) return;

	// 1. Гарантуємо доступ до регістрів RTC
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);

	// 2. Очищаємо старі прапорці будильника, щоб уникнути миттєвого пробудження
	RTC_WaitForLastTask();
	RTC_ClearITPendingBit(RTC_IT_ALR);
	EXTI_ClearITPendingBit(EXTI_Line17);
	RTC_WaitForLastTask();

	// 3. Встановлюємо час пробудження (поточний час + 3600 секунд)
	uint32_t current_cnt = RTC_GetCounter();
	RTC_SetAlarm(current_cnt + seconds);
	RTC_WaitForLastTask();

	// 4. Зупиняємо SysTick, щоб він не розбудив МК через 1 мс
	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

	// 5. Входимо в режим глибокого сну STOP Mode
	PWR_ClearFlag(PWR_FLAG_WU);
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

	// --- Тут МК спить рівно 1 годину ---

	// 6. Після пробудження відновлюємо тактування 72 МГц та SysTick
	SystemInit();
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

//*****************************************************************************
void my_RTC_EnterStopUntilNextHour(void){
	// 1. Отримуємо поточний лічильник і вираховуємо залишок до рівної години
	RTC_WaitForLastTask();
	uint32_t current_cnt = RTC_GetCounter();
	uint32_t seconds_to_hour = 3600UL - (current_cnt % 3600UL);

	// 2. Засинаємо рівно на цю кількість секунд
	my_RTC_EnterStopSeconds(seconds_to_hour);
}
//*****************************************************************************
void my_RTC_SetCompileTime(void){
	RTC_DateTime_t dt = {0};
	const char *date_str = __DATE__; // Формат: "Nov 17 2026"
	const char *time_str = __TIME__; // Формат: "16:09:00"
	const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	// 1. Визначаємо номер місяця (1..12) за першими 3 літерами
	dt.month = 1;
	for (uint8_t i = 0; i < 12; i++){
		if (strncmp(date_str, months[i], 3) == 0){
			dt.month = i + 1;
			break;
		}
	}

	// 2. Витягуємо день (з 4-го символу) та рік (з 7-го символу)
	dt.day  = (uint8_t)atoi(date_str + 4);
	dt.year = (uint16_t)atoi(date_str + 7);

	// 3. Витягуємо години, хвилини та секунди
	dt.hour = (uint8_t)atoi(time_str);
	dt.min  = (uint8_t)atoi(time_str + 3);
	dt.sec  = (uint8_t)atoi(time_str + 6);

	// 4. Записуємо в RTC
	my_RTC_SetDateTime(&dt);
}
//*****************************************************************************
