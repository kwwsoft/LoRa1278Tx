//*****************************************************************************
#include "stm32f10x.h"                  // Device header
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
//#include <string.h>
//#include <stdio.h>
#include "my_global.h"
#include "my_debug.h"
#include "my_tim2_delay.h"
#include "my_usart.h"
#include "my_rtc.h"
#include "my_i2c_bmp280.h"
#include "vdda.h"
#include "my_lora_sx1278.h"
//*****************************************************************************
uint8_t bmp280_ready;
uint8_t retcode;
uint8_t bmp280_init_count;
//*****************************************************************************
/*
	All functions starting with `myDebug...` only work when #define DEBUG_CODE` is set; 
	otherwise, the compiler removes them from the release build.
*/
//*****************************************************************************
int main(){
	int32_t temp_x100 = 0;
	uint32_t press_pa = 0;
	uint8_t rtc_init_status;
	RTC_DateTime_t now;
	LoRa_Packet_t myTXpacket;
	//remap PA15 and PB3 from JTAG to normal pins
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
	//
	myDebugInitLedPC13AndOFF();
	// Allow debugging (ST-Link) in SLEEP, STOP, and STANDBY modes
	// check the BOOT1 (PB2) jumper and configure DBGMCU
	myDebugUpdateMode();
	//init delay lib
	my_delay_TIM2_init();
	//init usart module
	myDebugInitUart();
	//init rtc for sleep and date/time
	rtc_init_status = my_RTC_Clock_Init();
	myDebugRTCFailed(rtc_init_status);
	//read date
	//my_RTC_GetDateTime(&now);
	//if date is wrong - set compilation date/time
	//if (now.year < 2026){
	my_RTC_SetCompileTime();
	myDebugShowRTCDateTime();
	//LoRa SX1278
	LoRa_WakeUp();
  retcode = LoRa_Init(433000000UL); // Specify your frequency (433E6 or 868E6)
  myDebugLoRaStatus(retcode);
  LoRa_Sleep();


	
	while(1){
		//initialization of variables and structures after waking up
		//BMP280
		bmp280_init_count = 5;
		bmp280_ready = 0;
		//zero out packet fields
		myTXpacket = (LoRa_Packet_t){0};
		//begin work
		myTXpacket.timestamp = RTC_GetCounter();
		//--- get temperature and pressure from the sensor BMP280 --- begin
		do{
				i2c_init();
				//Sensor not initialized – initial setup required
				retcode = BMP280_Init();
				if (retcode == ERROR_OK){
					bmp280_ready = 1;
				}
				else{
					bmp280_ready = 0;
					my_delay_ms(1000);
					i2c_bus_recover();
				}
				//show BMP280 status
				myDebugBMP280Failed(retcode);
		//take 5 times to initialize BMP280	
		}
		while((--bmp280_init_count > 0) && (bmp280_ready == 0));
		//sensor ready
		if (bmp280_ready){
			retcode = BMP280_ReadData(&temp_x100, &press_pa);
			if (retcode == ERROR_OK){
				myDebugSendBuffer("BMP280_ReadData OK\r\n");
				myTXpacket.temp_x100 = (uint16_t)temp_x100;
				myTXpacket.press_mmhg_x10 = (uint16_t)((press_pa * 100) / 1333);
			
			}else{
				bmp280_ready = 0;
				myDebugBMP280Failed(retcode);
				i2c_bus_recover();
			}
		}
		//--- get temperature and pressure from the sensor BMP280 --- end
		//
		//--- measure the voltage of the power battery --- begin
		my_ADC_Vref_Init();
		myTXpacket.vbat_mv = my_Get_Supply_Voltage_mV();
		//--- measure the voltage of the power battery --- end
		//
		myTXpacket.RTC_error = rtc_init_status;
		myTXpacket.bmp280_error = retcode;
		
		





		//calculate the packet checksum
		myTXpacket.crc16 = my_Calc_CRC16((uint8_t*)&myTXpacket, sizeof(LoRa_Packet_t) - sizeof(myTXpacket.crc16));
		myDebugShowLoRaPacket(&myTXpacket);
		myDebugShowRawPacket(&myTXpacket, sizeof(myTXpacket));




		// Transmit the packet via LoRa
		LoRa_WakeUp();
		retcode = LoRa_SendPacket((uint8_t*)&myTXpacket, sizeof(LoRa_Packet_t));
		myDebugLoRaStatus(retcode);
		LoRa_Sleep();
		
		
		
		
		

		// Deinitialize sensors and switch all ports to low-power mode
		my_PrepareForStop();
		// 4. Sleep until the exact start of the next hour (XX:00:00)
		//my_RTC_EnterStopUntilNextHour();
		my_RTC_EnterStopSeconds(6);
		//minimum baseline immediately upon waking
		// check the BOOT1 (PB2) jumper and configure DBGMCU
		myDebugUpdateMode();
		myDebugInitUart();
		myDebugSendBuffer("WakeUp after Sleep\r\n");
		//remap PA15 and PB3 from JTAG to normal pins
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
		AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
		//allow debugging during sleep and stop
		//peripheral initialization after wake-up
		my_delay_TIM2_init();
		//
		myDebugShowRTCDateTime();
	};
	

//	return 0;
}
