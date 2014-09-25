#ifndef _RS485_MODULE_
#define _RS485_MODULE_


/* Functions ------------------------- */
void vRS485_Init(void);
void RS485_IRQHandler(void);

void RS485_PrChar(uint8_t data);
void RS485_PrStr(uint8_t* str, uint16_t len);
void RS485_PrCStr(uint8_t* str);

#endif
