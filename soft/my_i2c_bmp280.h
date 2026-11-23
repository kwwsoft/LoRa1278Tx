#ifndef MY_I2C_BMP280
#define MY_I2C_BMP280

#include "stm32f10x.h"
#include "my_tim2_delay.h"
//*****************************************************************************

// Sensor address: 0xEC (0x76 << 1 if SDO = GND) or 0xEE (0x77 << 1 if SDO = VCC)
#define BMP280_HW_ADDRESS 0xEC
//*****************************************************************************
// Standard atmospheric pressure at sea level (Pa)
#define BMP280_SEA_LEVEL_PRESSURE_PA 101325.0f
//*****************************************************************************
// Basic I2C functions
void i2c_init(void);
uint8_t i2c_read_byte_bmp280(uint8_t addr, uint8_t *data);
uint8_t i2c_read_array_bmp280(uint8_t addr, uint8_t *data, uint8_t count);
uint8_t i2c_write_byte_bmp280(uint8_t addr, uint8_t const data);
uint8_t i2c_write_array_bmp280(uint8_t addr, uint8_t const *data, uint8_t count);
// Hardware bus reset on hang
void i2c_bus_recover(void);
//*****************************************************************************
// BMP280 sensor functions
uint8_t BMP280_Init(void);
uint8_t BMP280_ReadData(int32_t *temperature, uint32_t *pressure_pa);
//float BMP280_CalculateAltitude(uint32_t pressure_pa, float sea_level_pa);
//*****************************************************************************

#endif
