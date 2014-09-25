/*
 * RamBuffer.c
 *
 *  Created on: May 17, 2012
 *      Author: Van Ha
 */
#include <stdio.h>
#include <stdlib.h>

#include "rtl.h"
#include "stm32f10x.h"

#include "string.h"

#include "platform_config.h"


/* ram buffer is FIFO buffer.
 * N items, each item fix length = 256 bytes. => buffer_len = N*256.
 * byte first in item is length  of data in item. data_len = item_byte[0].
 * //byte second in item is status of data in itme. status = "EMPTY", "READY"...
 * w_idx, r_idx.
 * buffer is pointer which alloc memory when init.
 */

#define NUMBER_OF_BUFFER		10
#define ITEM_LEN_IN_BYTES		256
#define BUFFER_LEN_IN_BYTES		NUMBER_OF_BUFFER*ITEM_LEN_IN_BYTES


typedef enum
{
	EMPTY = 0,
	READY
}eRamBuffer_t;

char *RamBuffer;
int w_idx, r_idx;

OS_MUT ramMutex;

int RamBufferInit(void)
{
	/* alloc ram box */
	RamBuffer = (char*) calloc (BUFFER_LEN_IN_BYTES, sizeof(char));
	/* init index */
	w_idx = r_idx = 0;

	os_mut_init(ramMutex);

	return (RamBuffer != 0);
}

int RamBufferWrite(char *data, int len)
{
	int i;
	char *p;
	/* buffer not ready */
	if(RamBuffer == 0)
		return 0;

	/* lock variable */
	os_mut_wait(ramMutex, 0xFFFF);

	/* assign data length */
	p = (w_idx++ * ITEM_LEN_IN_BYTES) + RamBuffer;
	/* if pointer over buffer, reset it */
	if(w_idx >= NUMBER_OF_BUFFER) w_idx = 0;

	/* data len */
	*p++ = (len & 0xFF);
	/* copy data */
	for(i=0;i<len;i++)
	{
		*p++ = *data++;
	}

	/* release */
	os_mut_release(ramMutex);

	return 1;
}

int RamBufferRead(char *data, int *len)
{
	int i;
	char *p;

	/* buffer not ready */
	if(RamBuffer == 0)
		return 0;

	/* lock variable */
	os_mut_wait(ramMutex, 0xFFFF);
	/* calculate pointer */
	p = (r_idx * ITEM_LEN_IN_BYTES) + RamBuffer;

	/* read data len */
	*len = ((*p++)&0xFF);
	/* copy data */
	for(i=0;i<len;i++)
	{
		*data++ = *p++;
	}
	/* release */
	os_mut_release(ramMutex);

	return (len != 0);
}

void RamNextReadIndex(void)
{
	char *p;
	p = (r_idx * ITEM_LEN_IN_BYTES) + RamBuffer;
	r_idx++;
	/* delete data */
	p[0] = 0;
	/* if pointer over buffer, reset it */
	if(r_idx >= NUMBER_OF_BUFFER) r_idx = 0;
}
