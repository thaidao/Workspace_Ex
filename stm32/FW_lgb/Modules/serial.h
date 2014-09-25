/* Serial Communication Interface */

#ifndef _SERIAL_MODULE_
#define _SERIAL_MODULE_

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"	  	// Standard Peripheral header files

/* ------------------------------------- */

/* Utility */
void USART_PrChar(USART_TypeDef* USARTx, uint8_t data);
void USART_PrStr(USART_TypeDef* USARTx, uint8_t *str, uint16_t len);
void USART_PrCStr(USART_TypeDef* USARTx, const uint8_t* str);

#endif
