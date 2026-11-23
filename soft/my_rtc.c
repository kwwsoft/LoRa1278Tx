#include "my_rtc.h"
#include "stm32f10x_rtc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>
#include "my_global.h"

// Check for leap year
static uint8_t is_leap_year(uint16_t year){
	return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}
//*****************************************************************************
uint32_t my_RTC_MeasureLSI_TIM2(void){
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	uint32_t start_cnt, next_cnt;
	uint32_t timeout = 0x01000000;

	// 1. Enable TIM2 with a 0.1 ms step (10,000 ticks per 1 second at 72 MHz)
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_TimeBaseStructure.TIM_Prescaler = 7200 - 1; // 72 MHz / 7200 = 10 kHz (0.1 ms step)
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	// 2. Catch the start of an RTC tick
	start_cnt = RTC_GetCounter();
	while (RTC_GetCounter() == start_cnt){
		if (--timeout == 0) goto err;
	}

	// 3. Start TIM2 from zero
	TIM_SetCounter(TIM2, 0);
	TIM_Cmd(TIM2, ENABLE);

	// 4. Wait for the next RTC tick (exactly 40,000 LSI pulses)
	next_cnt = RTC_GetCounter();
	timeout = 0x01000000;
	while (RTC_GetCounter() == next_cnt){
		if (--timeout == 0) goto err;
	}

	uint16_t ticks_01ms = TIM_GetCounter(TIM2);
	TIM_Cmd(TIM2, DISABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);

	// 5. Calculate the actual LSI frequency:
	// If exactly 10,000 ticks (1.0000 s) elapsed, frequency = 40,000 Hz
	if (ticks_01ms > 0){
		return (40000UL * 10000UL) / ticks_01ms;
	}

err:
	TIM_Cmd(TIM2, DISABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
	return 40000;
}
//*****************************************************************************
// Safe wait for RTC operation completion with timeout
static uint8_t RTC_WaitForLastTask_Timeout(void){
	uint32_t timeout = 0x00100000;
	while (RTC_GetFlagStatus(RTC_FLAG_RTOFF) == RESET){
		if (--timeout == 0) return 1; // Timeout!
	}
	return 0;
}
//*****************************************************************************
// Safe wait for RTC synchronization with timeout
static uint8_t RTC_WaitForSynchro_Timeout(void){
	uint32_t timeout = 0x00100000;
	RTC_ClearFlag(RTC_FLAG_RSF);
	while (RTC_GetFlagStatus(RTC_FLAG_RSF) == RESET){
		if (--timeout == 0) return 1; // Timeout!
	}
	return 0;
}
//*****************************************************************************
uint8_t my_RTC_Clock_Init(void){
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	//uint32_t timeout = 0x00400000;
	//2...3 sec
	uint32_t timeout = 0x01500000;
	uint8_t lse_failed = ERROR_OK;

	// 1. Enable PWR and BKP clocks and allow access to the Backup domain
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);

	// 2. Check BOTH the magic value in BKP_DR1 AND whether the RTCEN bit is actually enabled
	if ((BKP_ReadBackupRegister(BKP_DR1) != RTC_BKP_MAGIC_VALUE) || ((RCC->BDCR & RCC_BDCR_RTCEN) == 0)){
		BKP_DeInit();
		PWR_BackupAccessCmd(ENABLE); // Re-enable access after BKP_DeInit()

		// Start the external 32.768 kHz crystal (LSE)
		RCC_LSEConfig(RCC_LSE_ON);
		while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET){
			if (--timeout == 0){
				lse_failed = ERROR_RTC_LSE_Failed_LSEConfig;
				break;
			}
		}

		if (!lse_failed){
			RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
			RCC_RTCCLKCmd(ENABLE);
			if (RTC_WaitForSynchro_Timeout() || RTC_WaitForLastTask_Timeout()){
					return ERROR_RTC_LSE_Failed_ENABLE;
			}
			RTC_SetPrescaler(32768 - 1);
			if (RTC_WaitForLastTask_Timeout()){
				return ERROR_RTC_LSE_Failed_SetPrescaler;
			}
		} else {
			// Disable the failed LSE before starting LSI
			RCC_LSEConfig(RCC_LSE_OFF);

			// Fallback option using LSI (40 kHz)
			RCC_LSICmd(ENABLE);
			//timeout = 0x00400000;
			while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET){
				if (--timeout == 0) return ERROR_RTC_LSI_Failed_ENABLE;
			}
			RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
			RCC_RTCCLKCmd(ENABLE);
			if (RTC_WaitForSynchro_Timeout() || RTC_WaitForLastTask_Timeout()) return ERROR_RTC_LSI_Failed_RTCCLKConfig;
			RTC_SetPrescaler(40000 - 1);
			if (RTC_WaitForLastTask_Timeout()) return ERROR_RTC_LSI_Failed_SetPrescaler;
			// Measure the actual LSI frequency (takes 1-2 seconds) and set the exact prescaler
			uint32_t real_lsi = my_RTC_MeasureLSI_TIM2();
			RTC_SetPrescaler(real_lsi - 1);
			if (RTC_WaitForLastTask_Timeout()) return ERROR_RTC_LSI_Failed_SetPrescaler1;		
		}
		// Write the magic value ONLY after successful startup
		BKP_WriteBackupRegister(BKP_DR1, RTC_BKP_MAGIC_VALUE);
	} else {
		// Clock is already running — wait for synchronization with timeout
		if (RTC_WaitForSynchro_Timeout() || RTC_WaitForLastTask_Timeout()){
			// If synchronization hangs — clear the magic value to reinitialize RTC on the next reset
			BKP_WriteBackupRegister(BKP_DR1, 0x0000);
			return ERROR_RTC_Reset_Magic;
		}
	}

	// 3. Enable the RTC Alarm interrupt
	RTC_ITConfig(RTC_IT_ALR, ENABLE);
	RTC_WaitForLastTask_Timeout();

	// 4. Configure EXTI Line 17 for wakeup from STOP Mode
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

	// January 1, 1970 was a Thursday (4)
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

	// 1. Ensure access to RTC registers
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);

	// 2. Clear old alarm flags to prevent immediate wakeup
	RTC_WaitForLastTask();
	RTC_ClearITPendingBit(RTC_IT_ALR);
	EXTI_ClearITPendingBit(EXTI_Line17);
	RTC_WaitForLastTask();

	// 3. Set the wakeup time (current time + seconds)
	uint32_t current_cnt = RTC_GetCounter();
	RTC_SetAlarm(current_cnt + seconds);
	RTC_WaitForLastTask();

	// 4. Stop SysTick so it doesn't wake up the MCU after 1 ms
	SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

	// 5. Enter deep sleep STOP Mode
	PWR_ClearFlag(PWR_FLAG_WU);
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

	// --- MCU is sleeping here ---

	// 6. Restore 72 MHz clock and SysTick after wakeup
	SystemInit();
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
}

//*****************************************************************************
void my_RTC_EnterStopUntilNextHour(void){
	// 1. Get the current counter and calculate the remaining seconds until the top of the hour
	RTC_WaitForLastTask();
	uint32_t current_cnt = RTC_GetCounter();
	uint32_t seconds_to_hour = 3600UL - (current_cnt % 3600UL);

	// 2. Sleep for that exact number of seconds
	my_RTC_EnterStopSeconds(seconds_to_hour);
}
//*****************************************************************************
void my_RTC_SetCompileTime(void){
	RTC_DateTime_t dt = {0};
	const char *date_str = __DATE__; // Format: "Nov 17 2026"
	const char *time_str = __TIME__; // Format: "16:09:00"
	const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	// 1. Determine the month number (1..12) by the first 3 letters
	dt.month = 1;
	for (uint8_t i = 0; i < 12; i++){
		if (strncmp(date_str, months[i], 3) == 0){
			dt.month = i + 1;
			break;
		}
	}

	// 2. Extract the day (from the 4th character) and year (from the 7th character)
	dt.day  = (uint8_t)atoi(date_str + 4);
	dt.year = (uint16_t)atoi(date_str + 7);

	// 3. Extract hours, minutes, and seconds
	dt.hour = (uint8_t)atoi(time_str);
	dt.min  = (uint8_t)atoi(time_str + 3);
	dt.sec  = (uint8_t)atoi(time_str + 6);

	// 4. Write to RTC
	my_RTC_SetDateTime(&dt);
}
//*****************************************************************************
