#include "my_rtc.h"
#include "stm32f10x_rtc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>

//*****************************************************************************
#define RTC_BKP_MAGIC_VALUE 0xA5A5
//*****************************************************************************
uint8_t my_RTC_Clock_Init(void){
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	uint32_t timeout = 0x00400000;
	uint8_t lse_failed = 0;

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
				lse_failed = 1;
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
	struct tm t = {0};
	t.tm_year = dt->year - 1900;
	t.tm_mon  = dt->month - 1;
	t.tm_mday = dt->day;
	t.tm_hour = dt->hour;
	t.tm_min  = dt->min;
	t.tm_sec  = dt->sec;

	time_t timestamp = mktime(&t);

	RTC_WaitForLastTask();
	RTC_SetCounter((uint32_t)timestamp);
	RTC_WaitForLastTask();
}
//*****************************************************************************
void my_RTC_GetDateTime(RTC_DateTime_t *dt){
	time_t timestamp = (time_t)RTC_GetCounter();
	struct tm *t = gmtime(&timestamp);

	dt->year  = t->tm_year + 1900;
	dt->month = t->tm_mon + 1;
	dt->day   = t->tm_mday;
	dt->hour  = t->tm_hour;
	dt->min   = t->tm_min;
	dt->sec   = t->tm_sec;
	dt->wday  = t->tm_wday;
}
//*****************************************************************************
void my_RTC_EnterStopSeconds(uint32_t seconds){
	if (seconds == 0) return;

	RTC_WaitForLastTask();
	uint32_t current_cnt = RTC_GetCounter();
	RTC_SetAlarm(current_cnt + seconds);
	RTC_WaitForLastTask();

	PWR_ClearFlag(PWR_FLAG_WU);
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

	// Відновлюємо тактування 72 МГц після виходу зі сну
	SystemInit();
}
//*****************************************************************************
void my_RTC_SetCompileTime(void){
	RTC_DateTime_t dt;
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
