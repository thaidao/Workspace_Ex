/* Serial Communication Interface */

#ifndef _SERIAL_MODULE_
#define _SERIAL_MODULE_

/* ------------------------------------- */

/* Debug port */
#define	DBG_Port			USART1
#define DBG_PrChar(chr)		USART_PrChar(DBG_Port, (chr))
#define DBG_PrStr(str, len)	USART_PrStr(DBG_Port, str, len)
#define DBG_PrCStr(str)		USART_PrCStr(DBG_Port, str)

#define dbg_print(str)		DBG_PrCStr((uint8_t*)str)

/* ------------------------------------- */

/* Utility */
void USART_PrChar(USART_TypeDef* USARTx, uint8_t data);
void USART_PrStr(USART_TypeDef* USARTx, uint8_t *str, uint16_t len);
void USART_PrCStr(USART_TypeDef* USARTx, const uint8_t* str);

#endif
