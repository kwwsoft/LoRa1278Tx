#ifndef MY_LORA_SX1278_H
#define MY_LORA_SX1278_H

#include "stm32f10x.h"

//*****************************************************************************
// SX1278 pin configuration (SPI1)
//*****************************************************************************
#define LORA_SPI                    SPI1
#define LORA_SPI_CLK                RCC_APB2Periph_SPI1

// SPI pins (PA5 = SCK, PA6 = MISO, PA7 = MOSI)
#define LORA_SPI_PORT               GPIOA
#define LORA_SPI_PORT_CLK           RCC_APB2Periph_GPIOA
#define LORA_PIN_SCK                GPIO_Pin_5
#define LORA_PIN_MISO               GPIO_Pin_6
#define LORA_PIN_MOSI               GPIO_Pin_7

// Control pins (can be adjusted to match your PCB layout)
#define LORA_CTRL_PORT              GPIOA
#define LORA_CTRL_PORT_CLK          RCC_APB2Periph_GPIOA
#define LORA_PIN_NSS                GPIO_Pin_2   // Chip Select (CS)
#define LORA_PIN_RST                GPIO_Pin_4   // Reset

// Use hardware DIO0 pin via EXTI latch (catches even short pulses).
// If DIO0 is not physically connected — comment out the line below,
// and the code will automatically poll the IRQ register over SPI.
#define LORA_USE_DIO0_PIN

#ifdef LORA_USE_DIO0_PIN
	#define LORA_DIO0_PORT          GPIOA
	#define LORA_DIO0_PORT_CLK      RCC_APB2Periph_GPIOA
	#define LORA_DIO0_PORT_SOURCE   GPIO_PortSourceGPIOA
	#define LORA_DIO0_PIN_SOURCE    GPIO_PinSource3
	#define LORA_PIN_DIO0           GPIO_Pin_3
	#define LORA_DIO0_EXTI_LINE     EXTI_Line3
#endif

//*****************************************************************************
// LoRa error codes
//*****************************************************************************
#define ERROR_LORA_OK               0
#define ERROR_LORA_WRONG_VERSION    1   // Wrong chip ID (expected 0x12)
#define ERROR_LORA_TX_TIMEOUT       2   // Timeout waiting for transmission completion

//*****************************************************************************
// Function prototypes
//*****************************************************************************
void LoRa_WakeUp(void);
uint8_t LoRa_Init(uint32_t freq_hz);
uint8_t LoRa_SendPacket(const uint8_t *data, uint8_t len);
void LoRa_Sleep(void);

#endif
