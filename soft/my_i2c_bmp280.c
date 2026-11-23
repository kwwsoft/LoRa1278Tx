#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_i2c.h"
#include "my_global.h"
#include "my_i2c_bmp280.h"
//#include <math.h>

//*****************************************************************************
#define chip I2C1
#define I2C_TIMEOUT 10000
// Store oversampling configuration (Temp x2, Press x16)
#define BMP280_OSRS_CONFIG ((0x02 << 5) | (0x05 << 2))
//*****************************************************************************

volatile uint32_t i2c_error = 0;
//*****************************************************************************
// Structure to store factory calibration coefficients of BMP280
typedef struct {
	uint16_t dig_T1;
	int16_t  dig_T2;
	int16_t  dig_T3;
	uint16_t dig_P1;
	int16_t  dig_P2;
	int16_t  dig_P3;
	int16_t  dig_P4;
	int16_t  dig_P5;
	int16_t  dig_P6;
	int16_t  dig_P7;
	int16_t  dig_P8;
	int16_t  dig_P9;
} BMP280_CalibData;

//*****************************************************************************
static BMP280_CalibData calib;
static int32_t t_fine;
//*****************************************************************************
/*
I2C Bus Recovery Algorithm
• Switch pins to GPIO Open-Drain: Temporarily disable the I2C1 peripheral and 
	reconfigure PB6 (SCL) and PB7 (SDA) as standard Open-Drain outputs (GPIO_Mode_Out_OD).
• Generate 9 pulses on SCL: Send up to 9 clock pulses on the SCL line. 
	This forces the BMP280 to finish clocking out the current byte and release the SDA line (it will be pulled up to 3.3 V).
• Generate a STOP condition: Once SDA is released, manually create a low-to-high transition on SDA 
	while SCL is high. This reliably puts the sensor into idle mode waiting for a new command.
• Software reset of the I2C module: Call RCC_APB1PeriphResetCmd (or set the SWRST bit in CR1) 
	to clear the internal state machine of the STM32 controller itself, and reinitialize I2C1.
*/
void i2c_bus_recover(void){
	GPIO_InitTypeDef port;
	uint8_t i;

	// 1. Disable I2C1
	I2C_Cmd(chip, DISABLE);
	I2C_DeInit(chip);

	// 2. Configure PB6 (SCL) and PB7 (SDA) as GPIO Open-Drain
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	port.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	port.GPIO_Mode = GPIO_Mode_Out_OD;
	port.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &port);

	// Set both lines high (release them)
	GPIO_SetBits(GPIOB, GPIO_Pin_6 | GPIO_Pin_7);
	my_delay_us(10);

	// 3. Generate up to 9 clock pulses on SCL until SDA goes high
	for (i = 0; i < 9; i++){
		if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7) == Bit_SET){
			break; // SDA line is free, no need to clock further
		}
		GPIO_ResetBits(GPIOB, GPIO_Pin_6); // SCL = 0
		my_delay_us(10);
		GPIO_SetBits(GPIOB, GPIO_Pin_6);   // SCL = 1
		my_delay_us(10);
	}

	// 4. Manually generate a STOP condition (SDA: 0 -> 1 while SCL = 1)
	GPIO_ResetBits(GPIOB, GPIO_Pin_6);
	my_delay_us(10);
	GPIO_ResetBits(GPIOB, GPIO_Pin_7);
	my_delay_us(10);
	GPIO_SetBits(GPIOB, GPIO_Pin_6);
	my_delay_us(10);
	GPIO_SetBits(GPIOB, GPIO_Pin_7);
	my_delay_us(10);

	// 5. Hardware reset of I2C1 registers via RCC
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, ENABLE);
	my_delay_us(10);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_I2C1, DISABLE);
}
//*****************************************************************************
static uint8_t I2C_WaitEvent(uint32_t event){
	uint32_t timeout = I2C_TIMEOUT;
	while(!I2C_CheckEvent(chip, event)){
		if (i2c_error != 0 || --timeout == 0){
			I2C_GenerateSTOP(chip, ENABLE);
			return 0xFF;
		}
	}
	return 0x00;
}
//*****************************************************************************
void i2c_init(void){
	I2C_InitTypeDef i2c;
	GPIO_InitTypeDef port;

	//i2c_bus_recover();

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	

	port.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	port.GPIO_Mode = GPIO_Mode_AF_OD;
	port.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &port);

	I2C_DeInit(chip);
	i2c.I2C_Mode = I2C_Mode_I2C;
	i2c.I2C_DutyCycle = I2C_DutyCycle_2;
	i2c.I2C_OwnAddress1 = 0;
	i2c.I2C_Ack = I2C_Ack_Enable;
	i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	i2c.I2C_ClockSpeed = 100000;
	I2C_Init(chip, &i2c);

	I2C_ITConfig(chip, I2C_IT_ERR, ENABLE);
	I2C_Cmd(chip, ENABLE);
	NVIC_EnableIRQ(I2C1_ER_IRQn);
}
//*****************************************************************************
void I2C1_ER_IRQHandler(void){
	uint32_t sr1 = I2C1->SR1;
	i2c_error = sr1 & 0xDF00;
	I2C1->SR1 &= ~0xDF00;
	I2C_GenerateSTOP(chip, ENABLE);
}
//*****************************************************************************
uint8_t i2c_read_array_bmp280(uint8_t addr, uint8_t *data, uint8_t count){
	uint32_t timeout = I2C_TIMEOUT;
	if (count == 0) return 0xFF;

	i2c_error = 0;
	while(I2C_GetFlagStatus(chip, I2C_FLAG_BUSY)){
		if (--timeout == 0){
			i2c_bus_recover();
			i2c_init();
			//I2C_AcknowledgeConfig(chip, DISABLE);
			//I2C_GenerateSTOP(chip, ENABLE);
			return 0xFF;
		}
	}

	I2C_AcknowledgeConfig(chip, ENABLE);
	I2C_GenerateSTART(chip, ENABLE);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 0xFF;

	I2C_Send7bitAddress(chip, BMP280_HW_ADDRESS, I2C_Direction_Transmitter);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) return 0xFF;

	I2C_SendData(chip, addr);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 0xFF;

	I2C_GenerateSTART(chip, ENABLE);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 0xFF;

	I2C_Send7bitAddress(chip, BMP280_HW_ADDRESS, I2C_Direction_Receiver);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) return 0xFF;

	while(count > 0){
		if (count == 1){
			I2C_AcknowledgeConfig(chip, DISABLE);
		}
		if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_RECEIVED)) return 0xFF;
		*data++ = I2C_ReceiveData(chip);
		count--;
	}

	I2C_GenerateSTOP(chip, ENABLE);
	return 0x00;
}
//*****************************************************************************
uint8_t i2c_read_byte_bmp280(uint8_t addr, uint8_t *data){
	return i2c_read_array_bmp280(addr, data, 1);
}
//*****************************************************************************
uint8_t i2c_write_array_bmp280(uint8_t addr, uint8_t const *data, uint8_t count){
	uint32_t timeout = I2C_TIMEOUT;
	if (count == 0) return 0xFF;

	i2c_error = 0;
	while(I2C_GetFlagStatus(chip, I2C_FLAG_BUSY)){
		if (--timeout == 0){
			i2c_bus_recover();
			i2c_init();
			//I2C_GenerateSTOP(chip, ENABLE);
			return 0xFF;
		}
	}

	I2C_GenerateSTART(chip, ENABLE);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT)) return 0xFF;

	I2C_Send7bitAddress(chip, BMP280_HW_ADDRESS, I2C_Direction_Transmitter);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) return 0xFF;

	I2C_SendData(chip, addr);
	if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 0xFF;

	while(count--){
		I2C_SendData(chip, *data++);
		if (I2C_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 0xFF;
	}

	I2C_GenerateSTOP(chip, ENABLE);
	return 0x00;
}
//*****************************************************************************
uint8_t i2c_write_byte_bmp280(uint8_t addr, uint8_t const data){
	return i2c_write_array_bmp280(addr, &data, 1);
}
//********************************************************************************************
// BMP280 initialization and configuration
//********************************************************************************************
uint8_t BMP280_Init(void){
	uint8_t chip_id = 0;
	uint8_t raw_calib[24];

	// 1. Check chip ID
	if (i2c_read_byte_bmp280(0xD0, &chip_id) != 0) return ERROR_BMP280_ID_READ;
	if (chip_id != 0x58 && chip_id != 0x60) return ERROR_BMP280_ID_DATA;

	// 2. Software reset of the sensor
	if (i2c_write_byte_bmp280(0xE0, 0xB6) != 0) return ERROR_BMP280_RESET;
	my_delay_ms(10);

	// 3. Read 24 calibration bytes (with proper indices!)
	if (i2c_read_array_bmp280(0x88, raw_calib, 24) != 0) return ERROR_BMP280_READ_CALIBR;

	calib.dig_T1 = (uint16_t)((raw_calib[1] << 8) | raw_calib[0]);
	calib.dig_T2 = (int16_t)((raw_calib[3] << 8) | raw_calib[2]);
	calib.dig_T3 = (int16_t)((raw_calib[5] << 8) | raw_calib[4]);

	calib.dig_P1 = (uint16_t)((raw_calib[7] << 8) | raw_calib[6]);
	calib.dig_P2 = (int16_t)((raw_calib[9] << 8) | raw_calib[8]);
	calib.dig_P3 = (int16_t)((raw_calib[11] << 8) | raw_calib[10]);
	calib.dig_P4 = (int16_t)((raw_calib[13] << 8) | raw_calib[12]);
	calib.dig_P5 = (int16_t)((raw_calib[15] << 8) | raw_calib[14]);
	calib.dig_P6 = (int16_t)((raw_calib[17] << 8) | raw_calib[16]);
	calib.dig_P7 = (int16_t)((raw_calib[19] << 8) | raw_calib[18]);
	calib.dig_P8 = (int16_t)((raw_calib[21] << 8) | raw_calib[20]);
	calib.dig_P9 = (int16_t)((raw_calib[23] << 8) | raw_calib[22]);

	// 4. Disable IIR filter for Forced Mode
	if (i2c_write_byte_bmp280(0xF5, 0x00) != 0) return ERROR_BMP280_IIR_OFF;

	// 5. Configure oversampling and switch to Sleep Mode (0x00)
	if (i2c_write_byte_bmp280(0xF4, BMP280_OSRS_CONFIG | 0x00) != 0) return ERROR_BMP280_SLEEP;

	return ERROR_OK;
}
//*****************************************************************************
// Temperature compensation formula from Bosch datasheet (returns °C * 100)
static int32_t BMP280_Compensate_T(int32_t adc_T){
	int32_t var1, var2, T;
	var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) * ((int32_t)calib.dig_T2)) >> 11;
	var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) * ((int32_t)calib.dig_T3)) >> 14;
	t_fine = var1 + var2;
	T = (t_fine * 5 + 128) >> 8;
	return T;
}
//*****************************************************************************
// Pressure compensation formula from Bosch datasheet (returns pressure in Pascals)
static uint32_t BMP280_Compensate_P(int32_t adc_P){
	int64_t var1, var2, p;
	var1 = ((int64_t)t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)calib.dig_P6;
	var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
	var2 = var2 + (((int64_t)calib.dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8) + ((var1 * (int64_t)calib.dig_P2) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;
	if (var1 == 0){
		return 0; // Protection against division by zero
	}
	p = 1048576 - adc_P;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)calib.dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);
	return (uint32_t)(p >> 8);
}
//*****************************************************************************
// Read and calculate temperature and pressure
uint8_t BMP280_ReadData(int32_t *temperature, uint32_t *pressure_pa){
uint8_t raw_data[6];
uint8_t status = 0;
uint8_t timeout = 50;
int32_t adc_P, adc_T;
	// 1. Trigger a single measurement (Forced Mode = 0x01)
	if (i2c_write_byte_bmp280(0xF4, BMP280_OSRS_CONFIG | 0x01) != 0) return ERROR_BMP280_OSRS_CONFIG;
	// 2. Wait for measurement completion
	do {
	my_delay_ms(2);
	if (i2c_read_byte_bmp280(0xF3, &status) != 0) return ERROR_BMP280_END_MEASUREMENT;
	if (--timeout == 0) return ERROR_BMP280_MEASUREMENT_TIMEOUT;
	} while (status & (1 << 3));
	// 3. Read 6 data bytes (with proper indices!)
	if (i2c_read_array_bmp280(0xF7, raw_data, 6) != 0) return ERROR_BMP280_READ_DATA;
	adc_P = (int32_t)((((uint32_t)raw_data[0]) << 12) | (((uint32_t)raw_data[1]) << 4) | ((uint32_t)raw_data[2] >> 4));
	adc_T = (int32_t)((((uint32_t)raw_data[3]) << 12) | (((uint32_t)raw_data[4]) << 4) | ((uint32_t)raw_data[5] >> 4));
	*temperature = BMP280_Compensate_T(adc_T);
	*pressure_pa = BMP280_Compensate_P(adc_P);
	return ERROR_OK;
}//********************************************************************************************
// Calculate altitude above sea level (in meters)
// pressure_pa  - current measured pressure (Pa)
// sea_level_pa - sea level pressure (typically 101325.0f or local QNH pressure)
//********************************************************************************************
/*
float BMP280_CalculateAltitude(uint32_t pressure_pa, float sea_level_pa){
if (pressure_pa == 0 || sea_level_pa <= 0.0f){
return 0.0f;
}
return 44330.0f * (1.0f - powf((float)pressure_pa / sea_level_pa, 0.19029495f));
}
*/