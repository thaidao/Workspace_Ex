#ifndef _DEBUG_MODULE_
#define _DEBUG_MODULE_

#include "serial.h"

typedef enum
{
	PRINTER_MODE = 0,
	DEBUG_MODE,
}system_mode_e;

extern system_mode_e glSystemMode;

/* Configuration  */
#define	DBG_Port			USART1
#define DBG_IRQHandler		USART1_IRQHandler

/* API */
void debug_init(void);
void DBG_PrChar(uint8_t data);
void dbg_print(uint8_t* str);
void DBG_PrCStr(const uint8_t* str);
void DBG_PrStr(uint8_t* str, uint16_t len);

/* Process config frame */
void dbg_setConfigParams(uint8_t* frame);

#endif
