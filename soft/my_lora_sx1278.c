#include "my_lora_sx1278.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_exti.h"
#include <stdio.h>
#include "my_tim2_delay.h"
#include "my_debug.h"

//*****************************************************************************
// Регістри SX1278 (LoRa Mode)
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

// Режими роботи (REG_OP_MODE)
#define MODE_LONG_RANGE_MODE        0x80
#define MODE_SLEEP                  0x00
#define MODE_STDBY                  0x01
#define MODE_TX                     0x03

// Прапорці переривань (REG_IRQ_FLAGS)
#define IRQ_TX_DONE_MASK            0x08

// Таймаут передачі (у мілісекундах)
#define LORA_TX_TIMEOUT_MS          3000

//*****************************************************************************
// Допоміжні макроси керування NSS
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

static uint8_t LoRa_ReadReg(uint8_t addr) {
	uint8_t val;
	LORA_NSS_LOW();
	LoRa_SPI_Transfer(addr & 0x7F); // Біт 7 = 0 для читання
	val = LoRa_SPI_Transfer(0x00);
	LORA_NSS_HIGH();
	return val;
}

static void LoRa_WriteReg(uint8_t addr, uint8_t val) {
	LORA_NSS_LOW();
	LoRa_SPI_Transfer(addr | 0x80); // Біт 7 = 1 для запису
	LoRa_SPI_Transfer(val);
	LORA_NSS_HIGH();
}

//*****************************************************************************
// 1. Пробудження периферії МК (GPIO + SPI1) та вивід SX1278 зі сну
//*****************************************************************************
void LoRa_WakeUp(void) {
	GPIO_InitTypeDef gpio;
	SPI_InitTypeDef spi;

	// Вмикаємо тактування портів, AFIO та SPI1
	RCC_APB2PeriphClockCmd(LORA_SPI_PORT_CLK | LORA_CTRL_PORT_CLK | LORA_SPI_CLK | RCC_APB2Periph_AFIO, ENABLE);

	// SCK (PA5) та MOSI (PA7) -> Alternate Function Push-Pull
	gpio.GPIO_Pin = LORA_PIN_SCK | LORA_PIN_MOSI;
	gpio.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LORA_SPI_PORT, &gpio);

	// MISO (PA6) -> Input Floating
	gpio.GPIO_Pin = LORA_PIN_MISO;
	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(LORA_SPI_PORT, &gpio);

	// NSS (PA4) та RST (PA3) -> Output Push-Pull
	gpio.GPIO_Pin = LORA_PIN_NSS | LORA_PIN_RST;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LORA_CTRL_PORT, &gpio);

	// Переводимо NSS та RST у високий рівень (неактивний стан)
	LORA_NSS_HIGH();
	GPIO_SetBits(LORA_CTRL_PORT, LORA_PIN_RST);

#ifdef LORA_USE_DIO0_PIN
	RCC_APB2PeriphClockCmd(LORA_DIO0_PORT_CLK, ENABLE);
	gpio.GPIO_Pin = LORA_PIN_DIO0;
	gpio.GPIO_Mode = GPIO_Mode_IPD; // Input Pull-Down
	GPIO_Init(LORA_DIO0_PORT, &gpio);

	// Налаштовуємо EXTI як апаратну заскочку по передньому фронту (без виклику IRQ в NVIC)
	GPIO_EXTILineConfig(LORA_DIO0_PORT_SOURCE, LORA_DIO0_PIN_SOURCE);
	EXTI->IMR  &= ~LORA_DIO0_EXTI_LINE; // Не генерувати запит переривання в процесор
	EXTI->EMR  &= ~LORA_DIO0_EXTI_LINE; // Не генерувати подію
	EXTI->RTSR |= LORA_DIO0_EXTI_LINE;  // Ловити передній фронт (0 -> 1)
	EXTI->FTSR &= ~LORA_DIO0_EXTI_LINE; // Не ловити задній фронт
	EXTI->PR    = LORA_DIO0_EXTI_LINE;  // Очищаємо прапорець заскочки
#endif

	// Налаштування SPI1 (72 МГц / 16 = 4.5 МГц — стабільно для SX1278)
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

	// Переводимо трансивер з режиму Sleep у Standby (якщо він уже ініціалізований)
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
	my_delay_ms(2);
}

//*****************************************************************************
// 2. Апаратне скидання та повна ініціалізація параметрів радіоканалу
// freq_hz: частота в Гц (наприклад, 433000000UL для 433 МГц)
//*****************************************************************************
uint8_t LoRa_Init(uint32_t freq_hz) {
	// Апаратний Reset модуля
	GPIO_ResetBits(LORA_CTRL_PORT, LORA_PIN_RST);
	my_delay_ms(5);
	GPIO_SetBits(LORA_CTRL_PORT, LORA_PIN_RST);
	my_delay_ms(10);

	// Перевірка зв'язку з чіпом (регістр 0x42 повинен повернути 0x12)
	if (LoRa_ReadReg(REG_VERSION) != 0x12) {
		return ERROR_LORA_WRONG_VERSION;
	}

	// Перехід у Sleep для зміни режиму на LoRa
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
	my_delay_ms(2);

	// Встановлення несучої частоти: Frf = (Freq * 2^19) / 32000000
	uint64_t frf = ((uint64_t)freq_hz << 19) / 32000000ULL;
	LoRa_WriteReg(REG_FRF_MSB, (uint8_t)(frf >> 16));
	LoRa_WriteReg(REG_FRF_MID, (uint8_t)(frf >> 8));
	LoRa_WriteReg(REG_FRF_LSB, (uint8_t)(frf >> 0));

	// Базова адреса FIFO для передачі
	LoRa_WriteReg(REG_FIFO_TX_BASE_ADDR, 0x00);

	// Налаштування вихідної потужності: PA_BOOST, +17 дБм (0x8F)
	// Для +20 дБм: PA_CONFIG = 0x8F та PA_DAC = 0x87
	LoRa_WriteReg(REG_PA_CONFIG, 0x8F);
	LoRa_WriteReg(REG_OCP, 0x2B);       // Захист по струму 100 мА
	LoRa_WriteReg(REG_PA_DAC, 0x84);    // Стандартний режим (+17 дБм)

	// Налаштування модему:
	// Bandwidth = 125 kHz (0x70), Coding Rate = 4/5 (0x02), Explicit Header (0x00)
	LoRa_WriteReg(REG_MODEM_CONFIG_1, 0x72);

	// Spreading Factor = SF9 (0x90), CRC ON (0x04) -> 0x94
	LoRa_WriteReg(REG_MODEM_CONFIG_2, 0x94);

	// AGC Auto ON (0x04)
	LoRa_WriteReg(REG_MODEM_CONFIG_3, 0x04);

	// Преамбула: 8 символів
	LoRa_WriteReg(REG_PREAMBLE_MSB, 0x00);
	LoRa_WriteReg(REG_PREAMBLE_LSB, 0x08);

	// Sync Word (0x12 — приватна мережа, 0x34 — LoRaWAN)
	LoRa_WriteReg(REG_SYNC_WORD, 0x12);

	// Мапінг DIO0 на TxDone (00 у бітах 7:6)
	LoRa_WriteReg(REG_DIO_MAPPING_1, 0x00);

	// Перехід у режим очікування Standby
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
	my_delay_ms(2);

	return ERROR_LORA_OK;
}

//*****************************************************************************
// 3. Відправка пакету даних
//*****************************************************************************
uint8_t LoRa_SendPacket(const uint8_t *data, uint8_t len) {
	uint16_t timeout = LORA_TX_TIMEOUT_MS;

	if (len == 0) return ERROR_LORA_OK;

	// Переводимо в Standby перед записом у FIFO
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

	// Встановлюємо вказівник FIFO на початок TX-буфера
	LoRa_WriteReg(REG_FIFO_ADDR_PTR, 0x00);
	LoRa_WriteReg(REG_PAYLOAD_LENGTH, len);

	// Записуємо байти пакета у FIFO
	for (uint8_t i = 0; i < len; i++) {
		LoRa_WriteReg(REG_FIFO, data[i]);
	}

	// Гарантуємо мапінг DIO0 на TxDone та очищаємо прапорці переривань у SX1278
	LoRa_WriteReg(REG_DIO_MAPPING_1, 0x00);
	LoRa_WriteReg(REG_IRQ_FLAGS, 0xFF);

#ifdef LORA_USE_DIO0_PIN
	// Скидаємо апаратну заскочку EXTI безпосередньо перед стартом передачі
	EXTI->PR = LORA_DIO0_EXTI_LINE;
#endif

	// Запускаємо передачу (TX Mode)
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

	
	
// --- ДІАГНОСТИКА ---
	char dbg[64];
	uint8_t pin_before = GPIO_ReadInputDataBit(LORA_DIO0_PORT, LORA_PIN_DIO0);
	uint8_t irq_before = LoRa_ReadReg(REG_IRQ_FLAGS);
	uint8_t map1       = LoRa_ReadReg(REG_DIO_MAPPING_1);
	// -------------------	
	
	
	
	// Чекаємо завершення передачі (TxDone)
	while (timeout--) {
#ifdef LORA_USE_DIO0_PIN
		// Перевіряємо апаратну заскочку EXTI (фіксує навіть короткий імпульс)
		if (EXTI->PR & LORA_DIO0_EXTI_LINE) {
			EXTI->PR = LORA_DIO0_EXTI_LINE; // Скидаємо заскочку записом 1
			break;
		}
#else
		if (LoRa_ReadReg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) {
			break;
		}
#endif
		my_delay_ms(1);
	}

	
	
// --- ДІАГНОСТИКА ПІСЛЯ ЦИКЛУ ---
	uint8_t pin_after = GPIO_ReadInputDataBit(LORA_DIO0_PORT, LORA_PIN_DIO0);
	uint8_t irq_after = LoRa_ReadReg(REG_IRQ_FLAGS);
	snprintf(dbg, sizeof(dbg), "MAP=0x%02X | PIN: %d->%d | IRQ: 0x%02X->0x%02X\r\n",
	         map1, pin_before, pin_after, irq_before, irq_after);
	myDebugSendBuffer(dbg);
	// -------------------------------	
	
	
	
	
	// Очищаємо прапорець TxDone у трансивері
	LoRa_WriteReg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

	// Повертаємо в Standby
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

	if (timeout == 0) {
		return ERROR_LORA_TX_TIMEOUT;
	}

	return ERROR_LORA_OK;
}

//*****************************************************************************
// 4. Переведення SX1278 у глибокий сон (~0.2 мкА) та вимкнення SPI1
//*****************************************************************************
void LoRa_Sleep(void) {
	// Команда переходу в Sleep Mode по SPI
	LoRa_WriteReg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);

	// Вимикаємо SPI1
	SPI_Cmd(LORA_SPI, DISABLE);
	RCC_APB2PeriphClockCmd(LORA_SPI_CLK, DISABLE);
}
