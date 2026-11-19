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
	void myDebugShowRTCDateTime();
	void myDebugInitLedPC13AndOFF();
#else	
	#define myDebugInitUart()     									((void)0)
	#define myDebugSendBuffer(pb);									((void)0)
	#define myDebugRTCFailed(retcode);							((void)0)
	#define myDebugShowRTCDateTime();								((void)0)
	#define myDebugInitLedPC13AndOFF();							((void)0)
#endif
//*****************************************************************************

#endif
