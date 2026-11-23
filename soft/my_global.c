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
	Function executed before entering STOP Mode.
	After wakeup, sensors and interfaces (UART, SPI, I2C, GPIO) 
	must be reinitialized.
*/	
	GPIO_InitTypeDef GPIO_InitStructure;

	// 1. Disable ADC if used (otherwise it consumes ~150 uA during sleep)
	ADC_TempSensorVrefintCmd(DISABLE);
	ADC_Cmd(ADC1, DISABLE);
	ADC_Cmd(ADC2, DISABLE);

	// 2. Enable clocks for all ports to reconfigure the pins
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | 
	                       RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | 
	                       RCC_APB2Periph_AFIO, ENABLE);

	// 3. Switch all unused pins to Analog Input (AIN) mode
	// WARNING: If any pins have external pull-ups (I2C, buttons), configure them separately below!
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;

	// Port A: leave PA13 (SWDIO) and PA14 (SWCLK) untouched to keep flashing capability without the Reset button
	// Keep PA13, PA14 for SWD, and do not switch PA2 (LoRa NSS) to AIN
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & ~(GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_2);
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// Pull up NSS (PA2) to 3.3V so the SPI input on SX1278 doesn't float
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// Ports B, C, D: switch completely to AIN (including PC13 LED)
	// WARNING: PC14 and PC15 are the 32.768 kHz crystal (LSE); although hardware-controlled in LSE mode,
	// it is safer not to touch them when writing the GPIOC configuration:
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & ~(GPIO_Pin_14 | GPIO_Pin_15);
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// 4. Example for pins with external pull-ups (e.g., I2C on PB6/PB7):
	// Since I2C has pull-up resistors to 3.3V, switch them to Floating Input to prevent current leakage
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	

	// 5. Disable clocks for all ports and APB2/APB1 peripherals (except PWR and BKP)
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
