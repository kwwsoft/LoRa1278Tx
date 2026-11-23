#include "my_lora_sx1278.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_exti.h"
#include <stdio.h>
#include "my_tim2_delay.h"
#include "my_debug.h"

//*****************************************************************************
// SX1278 Registers (LoRa Mode)
//*****************************************************************************
#define REG_FIFO                    0x00
#define REG_OP_MODE                 0x01
#define REG_FRF_MSB                 0x06
#define REG_FRF_MID                 0x07
#define REG_FRF_LSB                 0x08
#define REG_PA_CONFIG               0x09
#define REG_OCP                     0x0B
#define REG_LNA                     0x0C
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_ADDR       0x0E
#define REG_IRQ_FLAGS               0x12
#define REG_MODEM_CONFIG_1          0x1D
#define REG_MODEM_CONFIG_2          0x1E
#define REG_PREAMBLE_MSB            0x20
#define REG_PREAMBLE_LSB            0x21
#define REG_PAYLOAD_LENGTH          0x22
#define REG_MODEM_CONFIG_3          0x26
#define REG_SYNC_WORD               0x39
#define REG_DIO_MAPPING_1           0x40
#define REG_VERSION                 0x42
#define REG_PA_DAC                  0x4D

// Operating modes (REG_OP_MODE)
#define MODE_LONG_RANGE_MODE        0x80
#define MODE_SLEEP                  0x00
#define MODE_STDBY                  0x01
#define MODE_TX                     0x03

// Interrupt flags (REG_IRQ_FLAGS)
#define IRQ_TX_DONE_MASK            0x08

// Transmission timeout (in milliseconds)
#define LORA_TX_TIMEOUT_MS          3000

//*****************************************************************************
// Helper macros for NSS control
//*****************************************************************************
#define LORA_NSS_LOW()              GPIO_ResetBits(LORA_CTRL_PORT, LORA_PIN_NSS)
#define LORA_NSS_HIGH()             GPIO_SetBits(LORA_CTRL_PORT, LORA_PIN_NSS)

//*****************************************************************************
static uint8_t LoRa_SPI_Transfer(uint8_t data) {
	while (SPI_I2S_GetFlagStatus(LORA_SPI, SPI_I2S_FLAG_TXE) == RESET);
	SPI_I2S_SendData(LORA_SPI, data);
	while (SPI_I2S_GetFlagStatus(LORA_SPI, SPI_I2S_FLAG_RXNE) == RESET);
	return (uint8_t)SPI_I2S_ReceiveData(LORA_SPI);
}

//*****************************************************************************
static uint8_t LoRa_ReadReg(uint8_t addr) {
	uint8_t val;
	LORA_NSS_LOW();
	LoRa_SPI_Transfer(addr & 0x7F); // Bit 7 = 0 for read
	val = LoRa_SPI_Transfer(0x00);
	LORA_NSS_HIGH();
	return val;
}
//*****************************************************************************
static void LoRa_WriteReg(uint8_t addr, uint8_t val) {
	LORA_NSS_LOW();
	LoRa_SPI_Transfer(addr | 0x80); // Bit 7 = 1 for write
	LoRa_SPI_Transfer(val);
	LORA_NSS_HIGH();
}
//*****************************************************************************
// 1. Wake up MCU peripherals (GPIO + SPI1) and exit SX1278 from sleep
//*****************************************************************************
void LoRa_WakeUp(void) {
	GPIO_InitTypeDef gpio;
	SPI_InitTypeDef spi;

	// Enable clocks for ports, AFIO, and SPI1
	RCC_APB2PeriphClockCmd(LORA_SPI_PORT_CLK | LORA_CTRL_PORT_CLK | LORA_SPI_CLK | RCC_APB2Periph_AFIO, ENABLE);

	// SCK (PA5) and MOSI (PA7) -> Alternate Function Push-Pull
	gpio.GPIO_Pin = LORA_PIN_SCK | LORA_PIN_MOSI;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LORA_SPI_PORT, &gpio);

	// MISO (PA6) -> Input Floating
	gpio.GPIO_Pin = LORA_PIN_MISO;
	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(LORA_SPI_PORT, &gpio);

	// NSS (PA4) and RST (PA3) -> Output Push-Pull
	gpio.GPIO_Pin = LORA_PIN_NSS | LORA_PIN_RST;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LORA_CTRL_PORT, &gpio);

	// Set NSS and RST high (inactive state)
	LORA_NSS_HIGH();
	GPIO_SetBits(LORA_CTRL_PORT, LORA_PIN_RST);

#ifdef LORA_USE_DIO0_PIN
	RCC_APB2PeriphClockCmd(LORA_DIO0_PORT_CLK, ENABLE);
	gpio.GPIO_Pin = LORA_PIN_DIO0;
	gpio.GPIO_Mode = GPIO_Mode_IPD; // Input Pull-Down
	GPIO_Init(LORA_DIO0_PORT, &gpio);

	// Configure EXTI as a hardware rising-edge latch (without triggering NVIC IRQ)
	GPIO_EXTILineConfig(LORA_DIO0_PORT_SOURCE, LORA_DIO0_PIN_SOURCE);
	EXTI->IMR  &= ~LORA_DIO0_EXTI_LINE; // Do not generate an interrupt request to the CPU
	EXTI->EMR  &= ~LORA_DIO0_EXTI_LINE; // Do not generate an event
	EXTI->RTSR |= LORA_DIO0_EXTI_LINE;  // Catch rising edge (0 -> 1)
	EXTI->FTSR &= ~LORA_DIO0_EXTI_LINE; // Do not catch falling edge
	EXTI->PR    = LORA_DIO0_EXTI_LINE;  // Clear the latch flag
#endif

	// Configure SPI1 (72 MHz / 16 = 4.5 MHz — stable for SX1278)
	SPI_I2S_DeInit(LORA_SPI);
	spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	spi.SPI_Mode = SPI_Mode_Master;
	spi.SPI_DataSize = SPI_DataSize_8b;
	spi.SPI_CPOL = SPI_CPOL_Low;
	spi.SPI_CPHA = SPI_CPHA_1Edge;
	spi.SPI_NSS = SPI_NSS_Soft;
	spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
	spi.SPI_FirstBit = SPI_FirstBit_MSB;
	spi.SPI_CRCPolynomial = 7;
	SPI_Init(LORA_SPI, &spi);
	SPI_Cmd(LORA_SPI, ENABLE);

	// Switch transceiver from Sleep to Standby mode (if already initialized)
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
	my_delay_ms(2);
}

//*****************************************************************************
// 2. Hardware reset and full initialization of radio channel parameters
// freq_hz: frequency in Hz (e.g., 433000000UL for 433 MHz)
//*****************************************************************************
uint8_t LoRa_Init(uint32_t freq_hz) {
	// Hardware reset of the module
	GPIO_ResetBits(LORA_CTRL_PORT, LORA_PIN_RST);
	my_delay_ms(5);
	GPIO_SetBits(LORA_CTRL_PORT, LORA_PIN_RST);
	my_delay_ms(10);

	// Verify communication with the chip (register 0x42 must return 0x12)
	if (LoRa_ReadReg(REG_VERSION) != 0x12) {
		return ERROR_LORA_WRONG_VERSION;
	}

	// Switch to Sleep mode to change mode to LoRa
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
	my_delay_ms(2);

	// Set carrier frequency: Frf = (Freq * 2^19) / 32000000
	uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000ULL;
	LoRa_WriteReg(REG_FRF_MSB, (uint8_t)(frf >> 16));
	LoRa_WriteReg(REG_FRF_MID, (uint8_t)(frf >> 8));
	LoRa_WriteReg(REG_FRF_LSB, (uint8_t)(frf >> 0));

	// FIFO base address for transmission
	LoRa_WriteReg(REG_FIFO_TX_BASE_ADDR, 0x00);

	// Output power configuration: PA_BOOST, +17 dBm (0x8F)
	// For +20 dBm: PA_CONFIG = 0x8F and PA_DAC = 0x87
	LoRa_WriteReg(REG_PA_CONFIG, 0x8F);
	LoRa_WriteReg(REG_OCP, 0x2B);       // 100 mA over-current protection
	LoRa_WriteReg(REG_PA_DAC, 0x84);    // Default mode (+17 dBm)

	// Modem configuration:
	// Bandwidth = 125 kHz (0x70), Coding Rate = 4/5 (0x02), Explicit Header (0x00)
	LoRa_WriteReg(REG_MODEM_CONFIG_1, 0x72);

	// Spreading Factor = SF9 (0x90), CRC ON (0x04) -> 0x94
	LoRa_WriteReg(REG_MODEM_CONFIG_2, 0x94);

	// AGC Auto ON (0x04)
	LoRa_WriteReg(REG_MODEM_CONFIG_3, 0x04);

	// Preamble: 8 symbols
	LoRa_WriteReg(REG_PREAMBLE_MSB, 0x00);
	LoRa_WriteReg(REG_PREAMBLE_LSB, 0x08);

	// Sync Word (0x12 — private network, 0x34 — LoRaWAN)
	LoRa_WriteReg(REG_SYNC_WORD, 0x12);

	// 01 in bits 7:6 -> TxDone on DIO0
	LoRa_WriteReg(REG_DIO_MAPPING_1, 0x40);

	// Switch to Standby mode
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
	my_delay_ms(2);

	return ERROR_LORA_OK;
}

//*****************************************************************************
// 3. Send a data packet
//*****************************************************************************
uint8_t LoRa_SendPacket(const uint8_t *data, uint8_t len) {
	uint16_t timeout = LORA_TX_TIMEOUT_MS;

	if (len == 0) return ERROR_LORA_OK;

	// Switch to Standby before writing to FIFO
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

	// Set FIFO pointer to the start of the TX buffer
	LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
	LoRa_WriteReg(REG_PAYLOAD_LENGTH, len);

	// Write packet bytes into FIFO
	for (uint8_t i = 0; i < len; i++) {
		LoRa_WriteReg(REG_FIFO, data[i]);
	}

	// Ensure DIO0 is mapped to TxDone (01 in bits 7:6) and clear IRQ flags in SX1278
	LoRa_WriteReg(REG_DIO_MAPPING_1, 0x40);
	LoRa_WriteReg(REG_IRQ_FLAGS, 0xFF);

#ifdef LORA_USE_DIO0_PIN
	// Clear EXTI hardware latch right before starting transmission
	EXTI->PR = LORA_DIO0_EXTI_LINE;
#endif

	// Start transmission (TX Mode)
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

	// Wait for transmission completion (TxDone)
	while (timeout--) {
#ifdef LORA_USE_DIO0_PIN
		// Catch either a high level (classic SX1278) or the EXTI latch (short pulse)
		if ((GPIO_ReadInputDataBit(LORA_DIO0_PORT, LORA_PIN_DIO0) == Bit_SET) ||
		    (EXTI->PR & LORA_DIO0_EXTI_LINE)) {
			EXTI->PR = LORA_DIO0_EXTI_LINE; // Clear latch by writing 1
			break;
		}
#else
		if (LoRa_ReadReg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) {
			break;
		}
#endif
		my_delay_ms(1);
	}	
	
	// Clear TxDone flag in the transceiver
	LoRa_WriteReg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

	// Return to Standby
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

	if (timeout == 0) {
		return ERROR_LORA_TX_TIMEOUT;
	}

	return ERROR_LORA_OK;
}

//*****************************************************************************
// 4. Put SX1278 into deep sleep (~0.2 uA) and disable SPI1
//*****************************************************************************
void LoRa_Sleep(void) {
	// Command to enter Sleep Mode via SPI
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);

	// Disable SPI1
	SPI_Cmd(LORA_SPI, DISABLE);
	RCC_APB2PeriphClockCmd(LORA_SPI_CLK, DISABLE);
}
//*****************************************************************************
