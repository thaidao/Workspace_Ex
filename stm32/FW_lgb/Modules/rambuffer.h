/*
 * rambuffer.h
 *
 *  Created on: May 17, 2012
 *      Author: Van Ha
 */

#ifndef RAMBUFFER_H_
#define RAMBUFFER_H_


int RamBufferRead(char *data, int *len);
int RamBufferWrite(char *data, int len);
int RamBufferInit(void);
void RamNextReadIndex(void);

#endif /* RAMBUFFER_H_ */
