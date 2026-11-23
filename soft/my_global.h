#ifndef MY_GLOBAL_H
#define MY_GLOBAL_H

//define global debug mode
//#define DEBUG_CODE


#include "stm32f10x.h"

//********************************************************************************************
//a sign that the watch is already running
#define RTC_BKP_MAGIC_VALUE 0xA5A5
//*****************************************************************************
//ERROR values
#define ERROR_OK															0
//*****************************************************************************
//ERROR values RTC
#define ERROR_RTC_LSE_Failed_LSEConfig				1
#define ERROR_RTC_LSE_Failed_ENABLE						2
#define ERROR_RTC_LSE_Failed_SetPrescaler			3
#define ERROR_RTC_LSI_Failed_ENABLE						4
#define ERROR_RTC_LSI_Failed_RTCCLKConfig			5
#define ERROR_RTC_LSI_Failed_SetPrescaler			6
#define ERROR_RTC_LSI_Failed_SetPrescaler1		7
#define ERROR_RTC_Reset_Magic									8
//*****************************************************************************
//ERROR values BMP280
#define ERROR_BMP280_ID_READ									1
#define ERROR_BMP280_ID_DATA									2
#define ERROR_BMP280_RESET										3
#define ERROR_BMP280_READ_CALIBR							4
#define ERROR_BMP280_IIR_OFF									5
#define ERROR_BMP280_SLEEP										6
#define ERROR_BMP280_OSRS_CONFIG							7
#define ERROR_BMP280_END_MEASUREMENT					8
#define ERROR_BMP280_MEASUREMENT_TIMEOUT			9
#define ERROR_BMP280_READ_DATA								10
//*****************************************************************************
// LoRa error codes
//*****************************************************************************
#define ERROR_LORA_OK               0
#define ERROR_LORA_WRONG_VERSION    1   // Wrong chip ID (expected 0x12)
#define ERROR_LORA_TX_TIMEOUT       2   // Timeout waiting for transmission completion
//*****************************************************************************
// Simple structure for convenient date and time handling
typedef struct {
	uint16_t year;   // e.g., 2026
	uint8_t  month;  // 1..12
	uint8_t  day;    // 1..31
	uint8_t  hour;   // 0..23
	uint8_t  min;    // 0..59
	uint8_t  sec;    // 0..59
	uint8_t  wday;   // Day of the week: 0 = Sun, 1 = Mon .. 6 = Sat
} RTC_DateTime_t;
//*****************************************************************************
//structure for broadcast
typedef struct __attribute__((packed)) {
	uint32_t timestamp;      // 4 bytes: Unix time from RTC_GetCounter()
	uint16_t press_mmhg_x10; // 2 bytes: Pressure in mmHg * 10 (e.g., 7542 = 754.2 mmHg)
	int16_t  temp_x100;      // 2 bytes: Temperature * 100 (e.g., 2345 = 23.45 °C)
	uint16_t vbat_mv;        // 2 bytes: AA battery voltage in mV (e.g., 3150 = 3.15 V)
	uint8_t  RTC_error;      // 1 byte: RTC error code, or 0 if OK
	uint8_t  bmp280_error;   // 1 byte: BMP280 sensor error code, or 0 if OK
	uint16_t crc16;          // 2 bytes: CRC-16/MODBUS checksum (over the first 12 bytes)
} LoRa_Packet_t;
//********************************************************************************************
//The LED turns on if any function returns an error during operation. 
//It turns off when entering sleep mode, and also during initialization.
void my_led_init_and_OFF();
//disable port clocking for the LED
void my_led_deinit();
//********************************************************************************************
void my_PrepareForStop(void);
uint16_t my_Calc_CRC16(const uint8_t *data, uint16_t length);
//********************************************************************************************


#endif
