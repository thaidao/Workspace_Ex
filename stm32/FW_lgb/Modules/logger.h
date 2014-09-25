/**
 * @file	logger.h
 * @brief	Logger API, is used with log, buffer, firmware download
 *
 */
#ifndef _GH12_LOGGER_
#define _GH12_LOGGER_

#include "utils.h"

/* SDCard API Functions -------------------- */

/* Init sdCard module */
int8_t vSDCardInit(void);

/* Logger API ------------------------------------------ */

/* Write log API*/
int8_t vLogWrite(uint8_t* data, uint16_t len);				// Write Log message into SDCard

/* Buffer API ------------------------------------------ */
#define MSG_BUFFER_LEN					sizeof(Data_frame_t)

int8_t iMsgBufWrite(uint8_t* data);
int8_t iMsgBufRead(uint8_t* buf);
int8_t iMsgBufReadNext(void);

/**
 * How to use:
 * 	1. Adjust message len to MSG_BUFFER_LEN
 * 	2. iMsgBufWrite to write message to buffer
 * 	3. iMsgBufRead to read data from buffer, check return dif than -1, can be repeated to read same location
 * 	4. iMsgBufReadNext to move to next read location
 */

/* Utility */
void vLog_createNewFile(void);

// hanv add
void vCreateDriverFile(int8_t* drv, U16 si);

#endif
