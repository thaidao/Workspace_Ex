/**
 * @brief	SDFifo to cache buffer data
 */

#ifndef _SDFIFO_H
#define _SDFIFO_H

/* Global macros */
#define	SDFIFO_OK			0x00
#define SDFIFO_FAIL			0X01

/* Global macros */
#define SDFIFO_MSG_LEN		sizeof(Data_frame_t)

// Test macros
//#define	SDFIFO_DEBUG_ENABLE
//#define SDFIFO_TEST_ENABLE

/* API */
void sdfifo_init(void);
int8_t sdfifo_write(uint8_t* data);
int8_t sdfifo_read(uint8_t* buf);
int8_t sdfifo_readNext(void);
void sdfifo_reset(void);
uint16_t sdfifo_getInItem(void);
uint16_t sdfifo_getOutItem(void);
/*
 * \brief	sdfifo_getItem()
 * 			this function for detect number in buffer is large.
 * 			purpose for increase performance server.
 * \return	0 - not clog, other else.
 */
uint32_t sdfifo_getItem(void);

#endif //_SDFIFO_H
