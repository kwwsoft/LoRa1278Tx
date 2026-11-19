#include "stm32f10x_rtc.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_bkp.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_adc.h"
#include "misc.h"                       // Keil::Device:StdPeriph Drivers:Framework
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include "my_global.h"
#include "my_debug.h"
#include "my_usart.h"
#include "my_rtc.h"
//#include "my_lora_sx1278.h"
//*****************************************************************************
#define led_on 	(GPIOC->BRR = GPIO_Pin_13)  //Bit_RESET mapping
#define led_off (GPIOC->BSRR = GPIO_Pin_13) //bit_set mapping
//*****************************************************************************
#ifdef DEBUG_CODE
//*****************************************************************************
void	myDebugInitUart(){
	my_usart_init();
	my_usart_send_buffer("Start application\r\n");
}
//*****************************************************************************
void myDebugSendBuffer(char *pb){
	my_usart_send_buffer(pb);
}
//*****************************************************************************
void myDebugRTCFailed(uint8_t retcode){
			switch(retcode){
				case ERROR_OK:
					my_usart_send_buffer("Start ERROR_RTC_LSE_OK\r\n");
					break;
				case ERROR_RTC_LSE_Failed_LSEConfig:
					my_usart_send_buffer("Start ERROR_RTC_LSE_Failed_LSEConfig\r\n");
					led_on;
					break;
				case ERROR_RTC_LSE_Failed_ENABLE:
					my_usart_send_buffer("Start ERROR_RTC_LSE_Failed_ENABLE\r\n");
					led_on;
					break;
				case ERROR_RTC_LSE_Failed_SetPrescaler:
					my_usart_send_buffer("Start ERROR_RTC_LSE_Failed_SetPrescaler\r\n");
					led_on;
					break;
				case ERROR_RTC_LSI_Failed_ENABLE:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_ENABLE\r\n");
					led_on;
					break;
				case ERROR_RTC_LSI_Failed_RTCCLKConfig:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_RTCCLKConfig\r\n");
					led_on;
					break;
				case ERROR_RTC_LSI_Failed_SetPrescaler:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_SetPrescaler\r\n");
					led_on;
					break;
				case ERROR_RTC_LSI_Failed_SetPrescaler1:
					my_usart_send_buffer("Start ERROR_RTC_LSI_Failed_SetPrescaler1\r\n");
					led_on;
					break;
				case ERROR_RTC_Reset_Magic:
					my_usart_send_buffer("Start ERROR_RTC_Reset_Magic\r\n");
					led_on;
					break;
				default:
					break;
		}
}
//*****************************************************************************
void myDebugBMP280Failed(uint8_t retcode){
			switch(retcode){
				case ERROR_OK:
					my_usart_send_buffer("Start BMP280 OK\r\n");
					break;
				case ERROR_BMP280_ID_READ:
					my_usart_send_buffer("Start ERROR_BMP280_ID_READ\r\n");
					led_on;
					break;
				case ERROR_BMP280_ID_DATA:
					my_usart_send_buffer("Start ERROR_BMP280_ID_DATA\r\n");
					led_on;
					break;
				case ERROR_BMP280_RESET:
					my_usart_send_buffer("Start ERROR_BMP280_RESET\r\n");
					led_on;
					break;
				case ERROR_BMP280_READ_CALIBR:
					my_usart_send_buffer("Start ERROR_BMP280_READ_CALIBR\r\n");
					led_on;
					break;
				case ERROR_BMP280_IIR_OFF:
					my_usart_send_buffer("Start ERROR_BMP280_IIR_OFF\r\n");
					led_on;
					break;
				case ERROR_BMP280_SLEEP:
					my_usart_send_buffer("Start ERROR_BMP280_SLEEP\r\n");
					led_on;
					break;
				case ERROR_BMP280_OSRS_CONFIG:
					my_usart_send_buffer("Start ERROR_BMP280_OSRS_CONFIG\r\n");
					led_on;
					break;
				case ERROR_BMP280_END_MEASUREMENT:
					my_usart_send_buffer("Start ERROR_BMP280_END_MEASUREMENT\r\n");
					led_on;
					break;
				case ERROR_BMP280_MEASUREMENT_TIMEOUT:
					my_usart_send_buffer("Start ERROR_BMP280_MEASUREMENT_TIMEOUT\r\n");
					led_on;
					break;
				case ERROR_BMP280_READ_DATA:
					my_usart_send_buffer("Start ERROR_BMP280_READ_DATA\r\n");
					led_on;
					break;
				default:
					break;
		}
}
//*****************************************************************************
void myDebugShowRTCDateTime(){
RTC_DateTime_t now;
		char buffer[80] = {'\0'};
		my_RTC_GetDateTime(&now);
		sprintf(buffer, "%02d.%02d.%04d  %02d:%02d:%02d\r\n",
		now.day, now.month, now.year, now.hour, now.min, now.sec);
		my_usart_send_buffer(buffer);
}
//*****************************************************************************
void myDebugInitLedPC13AndOFF(){
	GPIO_InitTypeDef port;
	//pc13
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	port.GPIO_Pin = GPIO_Pin_13;
	port.GPIO_Speed = GPIO_Speed_50MHz;
	port.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_Init(GPIOC, &port);
	led_off;
}
//*****************************************************************************
void myDebugShowLoRaPacket(const LoRa_Packet_t *pkt){
    if (pkt == NULL) {
        my_usart_send_buffer("[DBG] NULL packet!\r\n");
        return;
    }

    // Ѕуфер п≥д весь текст пакета (≥з запасом, реальний текст ~190 байт)
    char buf[240];

    // Ѕезпечне зчитуванн€ з packed-структури у вир≥вн€н≥ зм≥нн≥
    uint32_t ts    = pkt->timestamp;
    uint16_t press = pkt->press_mmhg_x10;
    int16_t  temp  = pkt->temp_x100;
    uint16_t vbat  = pkt->vbat_mv;
    uint16_t res   = pkt->reserved;
    uint16_t crc   = pkt->crc16;

    char temp_sign = (temp < 0) ? '-' : '+';
    uint16_t temp_abs = (temp < 0) ? (uint16_t)(-temp) : (uint16_t)temp;

    // ‘ормуЇмо весь вив≥д за один раз
    snprintf(buf, sizeof(buf),
             "=== LoRa Packet (%u B) ===\r\n"
             "  Time  : %u\r\n"
             "  Press : %u.%u mmHg (%u)\r\n"
             "  Temp  : %c%u.%02u C (%d)\r\n"
             "  Vbat  : %u.%03u V (%u mV)\r\n"
             "  Res   : 0x%04X\r\n"
             "  CRC16 : 0x%04X\r\n"
             "==========================\r\n",
             (unsigned int)sizeof(LoRa_Packet_t),
             (unsigned int)ts,
             press / 10, press % 10, press,
             temp_sign, temp_abs / 100, temp_abs % 100, temp,
             vbat / 1000, vbat % 1000, vbat,
             res,
             crc);

    my_usart_send_buffer(buf);
}
//*****************************************************************************
//*****************************************************************************
void myDebugShowRawPacket(const void *data, uint16_t len){
    if (data == NULL || len == 0) {
        my_usart_send_buffer("[DBG] RAW: empty\r\n");
        return;
    }

    const uint8_t *bytes = (const uint8_t *)data;
    char buf[128];
    int offset = snprintf(buf, sizeof(buf), "RAW [%u]: ", (unsigned int)len);
    uint8_t is_lora_pkt = (len == sizeof(LoRa_Packet_t)) ? 1 : 0;

    for (uint16_t i = 0; i < len; i++) {
        // «алишаЇмо запас м≥сц€ п≥д "| ", "...\r\n\0" (м≥н≥мум 8 байт)
        if (offset >= (int)(sizeof(buf) - 8)) {
            offset += snprintf(buf + offset, sizeof(buf) - offset, "...");
            break;
        }

        // якщо це наша 14-байтна структура LoRa_Packet_t Ч в≥дд≥л€Їмо пол€ рискою
        // ћеж≥ пол≥в: [0..3] time | [4..5] press | [6..7] temp | [8..9] vbat | [10..11] res | [12..13] crc
        if (is_lora_pkt && (i == 4 || i == 6 || i == 8 || i == 10 || i == 12)) {
            offset += snprintf(buf + offset, sizeof(buf) - offset, "| ");
        }

        offset += snprintf(buf + offset, sizeof(buf) - offset, "%02X ", bytes[i]);
    }

    snprintf(buf + offset, sizeof(buf) - offset, "\r\n");
    my_usart_send_buffer(buf);
}
//*****************************************************************************
void myDebugLoRaStatus(uint8_t retcode) {
    switch (retcode) {
        case ERROR_LORA_OK:
            my_usart_send_buffer("LoRa TX OK\r\n");
            break;
        case ERROR_LORA_WRONG_VERSION:
            my_usart_send_buffer("ERROR_LORA_WRONG_VERSION\r\n");
            led_on;
            break;
        case ERROR_LORA_TX_TIMEOUT:
            my_usart_send_buffer("ERROR_LORA_TX_TIMEOUT\r\n");
            led_on;
            break;
        default:
            break;
    }
}
//*****************************************************************************






#endif
//*****************************************************************************

