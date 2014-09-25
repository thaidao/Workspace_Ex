/*
 * transport.h
 *
 *  Created on: Jan 10, 2012
 *      Author: Van Ha
 */

#ifndef TRANSPORT_H_
#define TRANSPORT_H_


#define vCOMM_Putc(c)		USART_PrChar(USART3, c)


/** comm put in queue ***/
void vCOMM_PutsQueue(U8* str);
void vCOMM_WriteQueue(U8* str, U16 len);

void vInitCOMM(void);
void vCOMM_Callback(U8 c);
__task void vCOMM_Init(void);

#endif /* TRANSPORT_H_ */
