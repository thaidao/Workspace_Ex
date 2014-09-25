/**
 * @file 	rs485.c
 * @brief	RS485 Module
 */

/* Include header files */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "rs485.h"
#include "Platform_Config.h"

/**
 * @brief	Initialize RS485 module
 * @param
 */
void vRS485_Init(void){
GPIO_InitTypeDef GPIO_InitStructure;
NVIC_InitTypeDef NVIC_InitStructure;
USART_InitTypeDef USART_InitStructure;

/* RCC Configuration */
	// GPIO Clock
	RCC_APB2PeriphClockCmd(RS485_GPIO_CLK | RCC_APB2Periph_AFIO , ENABLE);
	// UART clock
	RCC_APB1PeriphClockCmd(RS485_CLK, ENABLE);

/* GPIO Configuration */
	/* Configure RS485 Rx as input floating */
	GPIO_InitStructure.GPIO_Pin = RS485_RxPin;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(RS485_GPIO, &GPIO_InitStructure);

	/* Configure RS485 Tx as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = RS485_TxPin;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(RS485_GPIO, &GPIO_InitStructure);

	/* Configure RS485_Dir as output push pull */
	GPIO_InitStructure.GPIO_Pin = RS485_DirPin;
	GPIO_Init(RS485_GPIO, &GPIO_InitStructure);

/* NVIC Configuration */
	/* Configure the NVIC Preemption Priority Bits */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

	/* Enable the USARTy Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = RS485_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

/* UART Configuration */
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(RS485_Port, &USART_InitStructure);

	/* Init interrupts */
	USART_ITConfig(RS485_Port, USART_IT_RXNE, ENABLE);

	/* Enable RS485 */
	USART_Cmd(RS485_Port, ENABLE);
}


/* Utility functions ------------------------------ */
/* Send one char to RS485 port */
void RS485_PrChar(uint8_t data){
	GPIO_SetBits(RS485_GPIO, RS485_DirPin);
	USART_PrChar(RS485_Port, data);
	GPIO_ResetBits(RS485_GPIO, RS485_DirPin);
}

/* Send one string with given len to RS485 port */
void RS485_PrStr(uint8_t* str, uint16_t len){
	GPIO_SetBits(RS485_GPIO, RS485_DirPin);
	USART_PrStr(RS485_Port, str, len);
	GPIO_ResetBits(RS485_GPIO, RS485_DirPin);
}

/* Send one constant string to RS485 port */
void RS485_PrCStr(uint8_t* str){
	GPIO_SetBits(RS485_GPIO, RS485_DirPin);
	USART_PrCStr(RS485_Port, str);
	GPIO_ResetBits(RS485_GPIO, RS485_DirPin);
}

/* IRQ Handler ------------------------------------ */
void RS485_IRQHandler(void){
uint16_t UART_Data;

	/* Receive interrupt */
	if(USART_GetITStatus(RS485_Port, USART_IT_RXNE) != RESET){
		UART_Data = USART_ReceiveData(RS485_Port);
	};

}
