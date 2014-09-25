/**
 * @file	serial.c
 * @brief   USART Communication: get data from USART and then write to buffer
 */
#include "rtl.h"
#include "stm32f10x.h"
#include "serial.h"
#include <stdio.h>

/* -------------------------------------------------------- */

/**
 * @fn		void USART_PrChar(USART_TypeDef* USARTx, uint16_t data)
 * @brief	send one character though USARTx
 * @param	USARTx	USART Instance
 * @param	data	character to be sent
 */
void USART_PrChar(USART_TypeDef* USARTx, uint8_t data){
	while(USART_GetFlagStatus(USARTx,USART_FLAG_TXE)==RESET);
	USART_SendData(USARTx, data);
}

/**
 * @fn		void USART_PrStr(USART_TypeDef* USARTx, uint16_t *str, uint16_t len)
 * @brief	send string
 */
void USART_PrStr(USART_TypeDef* USARTx, uint8_t *str, uint16_t len){

	while(len--)
		USART_PrChar(USARTx, *str++);
}

/**
 * @fn		void USART_PrCStr(USART_TypeDef* USARTx, const uint16_t* str)
 * @brief	send constant string to USART
 * @return	none
 */
void USART_PrCStr(USART_TypeDef* USARTx, const uint8_t* str){
	while(*str)
		USART_PrChar(USARTx, *str++);
}

/* -------------------------------------------------------- */

/* implementation of fputc (also used by printf function to output data) */
#if 0
int fputc (int ch, FILE *f){
	USART_PrChar(USART2, ch);
	return 0;
}
#endif
