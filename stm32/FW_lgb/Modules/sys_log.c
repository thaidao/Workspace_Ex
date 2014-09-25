/**
 * syslog.c
 * store anything GH12 received from server, sms, exception...
 *
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "rtl.h"
#include "stm32f10x.h"
#include "platform_config.h"
#include "rtc.h"

/**
 * max size of file logs.txt. 
 * when file size more than, old file is backup and new file is created.
 **/
#define MAX_FILE_LOG_SIZE			10024000L		// default 10MBytes.

/**
 * limit length of line.
 **/
#define	MAX_LINE_IN_LOG_FILE 	256

/*
 * \brief		calculate file size.
 * \param		fp as file pointer.
 * \return	size of file.
 */
int32_t fsize(FILE *fp)
{
	int32_t sz;
	if(fp == NULL) return 0;
	
	fseek(fp, 0L, SEEK_END);
	sz = ftell(fp);
	rewind(fp);
	
	return sz;
}

/**
 * \brief		write log on sdcard.
 * \param		type as {PC, SERVER, SMS, DEVICE...}.
 * \param		logs as string which written.
 * \param		lsize as length of logs.
 * \return	number of bytes which written.
 **/
int16_t sys_log_fwrite(uint8_t type, int8_t *logs, int16_t lsize)
{
	FILE* fp;
	int8_t dateTime[]="00/00/00.00:00:00.";	// yy/month/day.hour-min-sec.
	uint8_t a, b, c;
	
	// validate size of log
	if(lsize >= MAX_LINE_IN_LOG_FILE)
		return -1;
	
	fp = fopen("logs.txt", "ab");
	if(fp == NULL)
		return -1;
	
	if(fsize(fp) >= MAX_FILE_LOG_SIZE){
		fclose(fp);
		// delete backup file.
		fdelete("logs.bkp");
		// backup file.
		frename("logs.txt", "logs.bkp");
		// create new file
		fp = fopen("logs.txt", "wb");
		if(fp == NULL) return -1;
	}
	// date.
	vRTCGetDate(&a, &b, &c);	// day, month, year.
	// year.
	dateTime[0] = c/10 + '0';
	dateTime[1] = c%10 + '0';
	// month.
	dateTime[3] = b/10 + '0';
	dateTime[4] = b%10 + '0';
	// day.
	dateTime[6] = a/10 + '0';
	dateTime[7] = a%10 + '0';
	// time.
	vRTCGetTime(&a, &b, &c);	// hour, min, sec.
	// hour
	dateTime[9] = a/10 + '0';
	dateTime[10] = a%10 + '0';
	// min.
	dateTime[12] = b/10 + '0';
	dateTime[13] = b%10 + '0';
	// sec.
	dateTime[15] = c/10 + '0';
	dateTime[16] = c%10 + '0';
	// write date time.
	fwrite(dateTime, sizeof(int8_t), strlen((const char*)dateTime), fp);
	// source write.
	switch(type){
		case 1: // PC.
			fwrite("PC->", sizeof(int8_t), 4, fp);
			break;
		case 2: // SERVER.
			fwrite("IP->", sizeof(int8_t), 4, fp);
			break;
		case 3: // SMS.
			fwrite("SM->", sizeof(int8_t), 4, fp);
			break;
		case 4: // DEVICE.
			fwrite("DE->", sizeof(int8_t), 4, fp);
			break;
		default:// unknow.
			fwrite("UK->", sizeof(int8_t), 4, fp);
			break;
	}
	// write data
	fwrite(logs, sizeof(int8_t), lsize, fp);
	fwrite("\n", 1, 1, fp);
	// release file.
	fclose(fp);
	
	return lsize;
}

/*
 * \brief		read newest log in file.
 * \param		logs as output buffer.
 * \param		lsize as number bytes for read.
 * \return	number bytes read.
 */
int16_t sys_log_fread(int8_t *logs, int16_t lsize)
{
	FILE* fp;
	int16_t ret = 0;
	
	fp = fopen("logs.txt", "rb");
	if(fp == NULL)
		return -1;
	
	// read newest data.
	fseek(fp, -lsize, SEEK_END);
	
	ret = fread(logs, sizeof(int8_t), lsize, fp);
	// release file.
	fclose(fp);
	
	return ret;
}

