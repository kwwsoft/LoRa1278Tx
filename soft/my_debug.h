#ifndef MY_DEBUG_H
#define MY_DEBUG_H

//*****************************************************************************
#include "stm32f10x.h"
#include "my_global.h"
#include "my_usart.h"
//*****************************************************************************
#ifdef DEBUG_CODE
	void myDebugInitUart();
	void myDebugSendBuffer(char *pb);
	void myDebugRTCFailed(uint8_t retcode);
	void myDebugBMP280Failed(uint8_t retcode);
	void myDebugShowRTCDateTime();
	void myDebugInitLedPC13AndOFF();
	void myDebugShowLoRaPacket(const LoRa_Packet_t *pkt);
	void myDebugShowRawPacket(const void *data, uint16_t len);
	void myDebugLoRaStatus(uint8_t retcode);
#else	
	#define myDebugInitUart()     									((void)0)
	#define myDebugSendBuffer(pb)										((void)0)
	#define myDebugRTCFailed(retcode)								((void)0)
	#define myDebugBMP280Failed(retcode)						((void)0)
	#define myDebugShowRTCDateTime()								((void)0)
	#define myDebugInitLedPC13AndOFF()							((void)0)
	#define myDebugShowLoRaPacket(pkt)							((void)0)
	#define myDebugShowRawPacket(data, len)					((void)0)
	#define myDebugLoRaStatus(retcode)      				((void)0)
#endif
//*****************************************************************************

#endif
