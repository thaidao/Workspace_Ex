/**
 * @file	logger.c
 * @biref	logger API Implementation
 *
 */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "logger.h"
#include "voice.h"
#include "sim900.h"

#define LOG_APPEND_MODE

/* --------------------------------------------------- */
/*		Local variables and functions		 		   */
/* --------------------------------------------------- */
/* Error handler */
void error(uint8_t* str);

/* File description */
FILE* f_log;			// Log file description
char Log_Filename[] = "log\\xxxxxx.log";
uint8_t newFileFlag = 0;

int playSDError = 0;

/* Private functions ---------------------------------- */
uint8_t vUpdateLogFileName(void);


/* ------------------------------------------------------ */
extern U8 sdCard_st;

/**
 * @fn		void vLoggerInit(void)
 * @brief	Init Logger module
 * @param	none
 * @param	none
 * Initialize
 */
int8_t vSDCardInit(void){
uint8_t count;

/* release sd card */
	funinit(NULL);

/* Initialize microSD Driver */
	// Init M0 Drive 5 times
	count = 10;
	while(finit(NULL) != 0){
		if(!(count--)){
			//error("Initialization failed.\r");
			dbg_print("E: vSDCardInit\r\n");
			return -1;
		};
	};

	// Check for formatted drive
#if 0
	if(fcheck("M:") != 0){
		dbg_print("FlashFS is inconsistent, formatting ...\r\n");
		if(fformat(NULL) != 0) {
			error("E: format M0\r\n");
			return;
		}
	}
#endif

// Get current date/time and update logfile name
	vUpdateLogFileName();

#ifndef LOG_APPEND_MODE
	f_log = fopen(Log_Filename,"a");
	if(f_log == NULL){
		error("Error: Open log file for writing\r\n");
		return -1;
	};
#endif

/* Init message buffer */
	sdfifo_init();

	return 0;
}

/* Logger functions ---------------------------------------------------------*/

/**
 * @fn		void vLogWrite(uint8_t* data, uint16_t len)
 * @brief	Write data into SDCard
 * @param	data	Pointer to data which is allocted
 * @param	len		length of data in bytes
 * @return	0 	success write to log
 * 			-1	error occurs during write
 */
#define LOG_SIZE 	sizeof(LogInfo_t)
int8_t vLogWrite(uint8_t* data, uint16_t len){
uint32_t write_cnt;
uint8_t temp[LOG_SIZE];
	/* Check if needed to create new file */
//	if(newFileFlag == 1){
//		//USART_PrCStr(USART1, "\r\nNew Day");
//		if(vUpdateLogFileName() == 1)
//		{
//			newFileFlag = 0;
//
//			f_log = fopen(Log_Filename,"w");
//
//			//USART_PrCStr(USART1, "\r\nNew File");
//		}
//		else
//		{
//			// Open logfile to append data
//			f_log = fopen(Log_Filename,"a");
//		}
//	}
//	else{
//		// Open logfile to append data
//		f_log = fopen(Log_Filename,"a");
//	}

	// if next day. change file name.
	if(newFileFlag == 1){
		if(vUpdateLogFileName())
			newFileFlag = 0;
	}

	// open file for append.
	f_log = fopen(Log_Filename,"ab");

	if(f_log == NULL){
		dbg_print("E: fopen log\r\n");

		return -1;
	};

	/* Write data to sdCard */
	write_cnt = fwrite(data, sizeof(uint8_t), len, f_log);
	if(write_cnt != len){
		// Close open logfile
		fclose(f_log);
		dbg_print("Error: vLogWrite\r\n");
		// @todo: add to mailbox ??
		return -1;
	};

	// Close open logfile
	fclose(f_log);

	f_log = fopen(Log_Filename,"rb");
	if(f_log == NULL){
		return -1;
	};

	if(fseek(f_log, -LOG_SIZE, SEEK_END) == EOF){
		fclose(f_log);
		return -1;
	}

	// Read data and check with write location at same bank
	fread(temp, sizeof(uint8_t), LOG_SIZE, f_log);
	// Close the file
	fclose(f_log);

	if(memcmp(data, temp, LOG_SIZE) != 0){
		dbg_print("E: Log verify\r\n");
		return -1;
	}
	/* reset time out for alarm sdcard error */
	sdCard_st = 0;
	return 0;
}


/* Utility functions ------------------------------------ */
void vLog_createNewFile(void){
	/* Set flag */
	newFileFlag = 1;
}


/* Update log file name with given date */
uint8_t vUpdateLogFileName(void){
//char *buf = Log_Filename;		// xxxxxx.log
uint8_t date, month, year;
uint8_t oldLogName[20];

	/* Get current date */
	vRTCGetDate(&date, &month, &year);
	sprintf(oldLogName,"log\\%02u%02u%02u.log\x0", date, month, year);

	if(strcmp(oldLogName, Log_Filename)==0)
		return 0;

	/* Modify logfile name */
	//sprintf(Log_Filename,"log\\%02u%02u%02u.log", date, month, year);
	strcpy(Log_Filename, oldLogName);

	/* Delete the old log file */
	if(month > 2)
	{
		month -= 2;
	}
	else if(month == 2)
	{
		month = 12;
		year--;
	}
	else if(month == 1)
	{
		month = 11;
		year --;
	}

	sprintf(oldLogName, "log\\%02u%02u%02u.log", date, month, year);
	if( fdelete(oldLogName) == 0)
		dbg_print("Delete old file ok"); 

	return 1;
}

/* ------------------------------------------------------ */

/**
 * @fn		static void error(const uint8_t* str)
 * @brief	Error handler
 * @param 	str	constant string pointer is used for display
 * @todo	Give alarm
 */
void error(uint8_t* str){
uint8_t count;

	/* Debug */
	if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
		//dbg_print(str);
		USART_PrCStr(USART1, str);

	sdCard_st = 1;
	if(playSDError==0)
	{
		playSDError = 500;
		PlayVoiceFile(ALARM_SDCARD_ERROR);
	}
	else
	{
		playSDError --;
	}

	/* Init sdCard again */
	/* release sd card */
	funinit(NULL);

	/* Initialize microSD Driver */
	count = 10;
	while(finit(NULL) != 0){
		if(!(count--)){
			dbg_print("E: vSDCardInit\r\n");
			return;
		};
	};
}

uint8_t get_sdcard_status(void)
{
	if(sdCard_st)
		return 0;	// error.
	
	return 1;
}
