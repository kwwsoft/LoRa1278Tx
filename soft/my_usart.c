#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"            // Keil::Device:StdPeriph Drivers:USART
#include "stm32f10x_rcc.h"
#include "my_usart.h"

//*****************************************************************************
void my_usart_init(void){
	GPIO_InitTypeDef port;
	USART_InitTypeDef usart;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

	// TX (PA9)
	port.GPIO_Pin = GPIO_Pin_9;
	port.GPIO_Mode = GPIO_Mode_AF_PP;
	port.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &port);

	// RX (PA10)
	port.GPIO_Pin = GPIO_Pin_10;
	port.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &port);

	usart.USART_BaudRate = 115200;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	usart.USART_Parity = USART_Parity_No;
	usart.USART_StopBits = USART_StopBits_1;
	usart.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &usart);
	USART_Cmd(USART1, ENABLE);
}
//*****************************************************************************
void my_usart_send_buffer(char *pb){
	while(*pb){
		while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
		USART_SendData(USART1, *pb++);
	}
	while(USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}
//*****************************************************************************
