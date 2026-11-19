#include "stm32f10x_rtc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_adc.h"
#include "misc.h"
#include <stdlib.h>
#include <string.h>
#include "my_global.h"

//********************************************************************************************
void my_led_deinit(){
	//pc13
	GPIO_DeInit(GPIOC);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, DISABLE);
}
//********************************************************************************************
void my_PrepareForStop(void){
/*
	функція перед входом у STOP Mode. 
	Після пробудження потрібно буде заново викликати 
	ініціалізацію датчиків та інтерфейсів (UART, SPI, I2C, GPIO).
*/	
	GPIO_InitTypeDef GPIO_InitStructure;

	// 1. Вимикаємо АЦП, якщо використовувався (інакше він жере ~150 мкА у сні)
	ADC_Cmd(ADC1, DISABLE);
	ADC_Cmd(ADC2, DISABLE);

	// 2. Вмикаємо тактування всіх портів, щоб змінити конфігурацію пінів
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | 
	                       RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | 
	                       RCC_APB2Periph_AFIO, ENABLE);

	// 3. Переводимо всі вільні піни в режим Analog Input (AIN)
	// УВАГА: Якщо якісь піни мають зовнішню підтяжку (I2C, кнопки), налаштуйте їх окремо нижче!
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;

	// Порт A: залишаємо PA13 (SWDIO) та PA14 (SWCLK), щоб не втратити можливість прошивки без кнопки Reset
	// Якщо хочете абсолютний мінімум струму в релізі — можна додати і їх
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & ~(GPIO_Pin_13 | GPIO_Pin_14);
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// Порти B, C, D: переводимо повністю в AIN (включаючи PC13 світлодіод)
	// УВАГА: PC14 та PC15 — це кварц 32.768 кГц (LSE), але в режимі LSE вони керуються апаратно,
	// проте безпечніше їх не чіпати при записі конфігурації GPIOC:
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & ~(GPIO_Pin_14 | GPIO_Pin_15);
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// 4. Приклад для пінів із зовнішньою підтяжкою (якщо є I2C на PB6/PB7):
	// Оскільки на I2C стоять резистори до 3.3В, переводимо їх у Floating Input, щоб не було витоку струму
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	

	// 5. Вимикаємо тактування всіх портів і периферії APB2/APB1 (крім PWR та BKP)
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | 
	                       RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | 
	                       RCC_APB2Periph_AFIO | RCC_APB2Periph_ADC1 | 
	                       RCC_APB2Periph_SPI1 | RCC_APB2Periph_USART1, DISABLE);
}
//********************************************************************************************
uint16_t my_Calc_CRC16(const uint8_t *data, uint16_t length){
	uint16_t crc = 0xFFFF;
	for (uint16_t i = 0; i < length; i++){
		crc ^= data[i];
		for (uint8_t j = 0; j < 8; j++){
			if (crc & 0x0001){
				crc = (crc >> 1) ^ 0xA001;
			} else {
				crc >>= 1;
			}
		}
	}
	return crc;
}
//********************************************************************************************

