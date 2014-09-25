/*
 * SMS_Handler.c
 *
 *  Created on: Mar 13, 2012
 *      Author: Van Ha
 */
#include <stdlib.h>
#include <RTL.h>                      /* RTL kernel functions & defines      */
#include <stdio.h>                    /* standard I/O .h-file                */
#include <ctype.h>                    /* character functions                 */
#include <string.h>                   /* string and memory functions         */

/* Include needed modules */
#include "serial.h"					 // Serial Communication Module
#include "logger.h"					 // Logger Module
#include "printer.h"				 // Printer module
#include "Platform_Config.h"		 // Platform configuration

#include "loadconfig.h"
#include "driverdb.h"
#include "trackPrint.h"

#include "utils.h"
#include "sim18.h"
#include "http.h"
#include "gprs.h"
#include "gpsmodule.h"
#include "voice.h"
#include "driverdb.h"
#include "rtc.h"
#include "trackprint.h"
#include "driverprocess.h"
#include "sdfifo.h"
#include "adminSecurity.h"

/*
 * \brief	void device_getinfo()
 * 			return status of device as GPRS signal, GPS signal, SD card info.
 * \param	p is pointer point to memory.
 * \return 	none.
 */
void device_getinfo(uint8_t *p);

typedef struct _SMS_Content_t
{
	S8* text;
	U8 content_len;
}SMS_Content_t;

typedef struct _SMS_Header_t
{
	S8* text;
	U8 header_len;
}SMS_Header_t;

typedef struct _SMS_Message_t
{
	SMS_Content_t content;
	SMS_Header_t header;
}SMS_Message_t;

extern U8 cl_state, sdCard_st;
extern gpsInfo_t gpsInfo;

/* debug camera */
extern U32 uGetCameraReadIdx(void);
extern U32 uGetCameraWriteIdx(void);
extern U8 uGetImageCount(void);
extern void vSendLogData(S8 *p);
extern void GPRS_GetReport(uint8_t* strReport);
extern int16_t sys_log_fwrite(uint8_t type, int8_t *logs, int16_t lsize);

/**
 * print message.
 * usage:
 * 		extern function to gprs.c file.
 * 		add funtion name to xMsgType[] variable.
 *
 */
void vSMS_Handler(S8* pData)
{
	S8 ch, *p;
	S8 str[12];
	SMS_Message_t smsMesg;
	S8 *s[3], n;
	/* ban tin SMS co dang:
	 *pData = ,_"+841692128985","","12/02/03,22:56:23+28"<crlf>@Vt nnttnn ggppgg!<crlf>
	 *		   [	thong tin header					][	  ][	noi dung 	][end]
	 *
	 */
	// write log
	if(strstr(pData, "LOGS")==NULL){
		sys_log_fwrite(3, (int8_t*)pData, strlen(pData));
	}
	/* loc phan header.  smsMesg.header.text = "phonnumber","",...*/
	n = string_split((U8**)s, (U8*)pData, "\r\n", 2);
	if(n != 2)
		return;
	/* lay phan noi dung */
	smsMesg.content.text = s[1];
	//smsMesg.content.content_len = strlen(smsMesg.content.text);

	n = string_split((U8**)s, (U8*)pData, "\"", 2);
	if(n < 2)
		return;
	smsMesg.header.text = s[1];
	
	// VALIDATE PHONE NUMBER.
	if(user_validate(smsMesg.header.text) == __FALSE)
		return;

	/*********** PUBLIC *********************/
	/* print report */
	if(strncmp(smsMesg.content.text, "In", 2)==0)
	{
		GH12_Print_1(smsMesg.content.text);
		return;
	}

	/* change driver ID */
	if(strncmp(smsMesg.content.text, "DRV_LOGON,", 10)==0)
	{
		driverdb_t* pDriver;
		if(driverIDValidate(smsMesg.content.text + 10)!=0)
		{
			PlayVoiceFile(ALARM_DRV_LOGON_FAIL);
			return;
		}

		pDriver = (driverdb_t*)calloc(sizeof(driverdb_t), 1);

		if((pDriver==NULL) || eDB_GetDriverInfo(smsMesg.content.text + 10, pDriver) != 0){
			PlayVoiceFile(ALARM_DRV_LOGON_FAIL);
			return;
		}

		updateDriverKey(smsMesg.content.text + 10);
		
		PlayVoiceFile(ALARM_DRV_LOGON_OK);

		return;
	}
	
	/* Handle FWVER? */
	if(strncmp(smsMesg.content.text, "FWVER?", 5)==0)
	{
		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;

		strcpy(p, FW_VERSION_STR);
		strcat(p, ",");
		strcat(p, HW_VERSION_STR);
		/* send sms */
		blSMS_Puts(smsMesg.header.text, p);

		return;
	}

	/* return all config to sms */
	if(strncmp(smsMesg.content.text, "SCFG,GET,ALL", 12) == 0)
	{
		/* get phone number */

		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;

		if(config_device_gets(p, 160) == -1)
		{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, "Read device config error");
		}
		else{
			USART_PrCStr(USART1, p);

			/* send sms */
			blSMS_Puts(smsMesg.header.text, p);
		}

		free(p);
		return;
	}
	
	/* send gsm state */
	if(strncmp(smsMesg.content.text, "INFO", 4)==0)
	{
		/* new memory box */
		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;

		device_getinfo(p);

		/* send sms */
		blSMS_Puts(smsMesg.header.text, p);

		free(p);
		return;
	}
	// read log
	if(strncmp(smsMesg.content.text, "LOGS", 4)==0){
		int8_t* buf = (int8_t*)malloc(160);
		int16_t len = sys_log_fread(buf, 120);
		if(len > 0){
			blSMS_Puts(smsMesg.header.text, buf);
		}
		else{
			blSMS_Puts(smsMesg.header.text, "can't read logs");
		}
		// release memory
		free(buf);
		return;
	}
	/******** END PUBLIC ******************************/
	
	// validate content.
	if(content_validate(Config.admin.pass, smsMesg.content.text)== __FALSE) // should reply via sms?
		return;
	// abort password. content = pass + space + real content.
	smsMesg.content.text += (strlen(Config.admin.pass) + 1);
	
	/* upgrade firmware: DOTA,PATHFILE,CHECKSUM */
	if(strncmp(smsMesg.content.text, "DOTA", 4)==0)
	{
		/* PATHFILE,CHECKSUM */
		FWUpgradeDemandHandler(smsMesg.content.text + 4);
		return;
	}
	if(strncmp(smsMesg.content.text, "SCFG,GET,DEVICE", 15) == 0)
	{
		/* get phone number */

		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;

		if(config_device_gets(p, 160) == -1)
		{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, "Read device config error");
		}
		else{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, p);
		}

		free(p);
		return;
	}

	if(strncmp(smsMesg.content.text, "SCFG,GET,DRIVER", 16) == 0)
	{
		/* get phone number */

		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;

		if(config_driver_gets(p, 160) == -1)
		{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, "Read driver config error");
		}
		else{

			/* send sms */
			blSMS_Puts(smsMesg.header.text, p);
		}

		free(p);
		return;
	}

	if(strncmp(smsMesg.content.text, "SCFG,GET,VEHICLE", 16) == 0)
	{
		/* get phone number */

		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;

		if(config_vehicle_gets(p, 160) == -1)
		{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, "Read vehicle config error");
		}
		else{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, p);
		}

		free(p);
		return;
	}

	if(strncmp(smsMesg.content.text, "SCFG,GET,EXT", 12) == 0)
	{
		/* get phone number */

		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;

		if(config_ext_gets(p, 160) == -1)
		{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, "Read extension config error");
		}
		else{
			/* send sms */
			blSMS_Puts(smsMesg.header.text, p);
		}

		free(p);
		return;
	}

	// $SCFG,GET,TAX1#
	if(strncmp(smsMesg.content.text, "SCFG,GET,TAX1", 13) == 0){
		GSM_blSendCommand("at+cusd=1,\"*101#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_SMS;
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, smsMesg.header.text);
		HostChannel.state = CH_STATE_BUSY;
		return;
	}

	// $SCFG,GET,TAX2#
	if(strncmp(smsMesg.content.text, "SCFG,GET,TAX2", 13) == 0){
		GSM_blSendCommand("at+cusd=1,\"*102#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_SMS;
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, smsMesg.header.text);
		HostChannel.state = CH_STATE_BUSY;
		return;
	}

	// $SCFG,FWR,TAX1#
	if(strncmp(smsMesg.content.text, "SCFG,FWR,TAX1", 13) == 0){
		GSM_blSendCommand("at+cusd=1,\"*101#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_SMS;
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, smsMesg.content.text + 14);
		HostChannel.state = CH_STATE_BUSY;
		return;
	}

	// $SCFG,FWR,TAX2#
	if(strncmp(smsMesg.content.text, "SCFG,FWR,TAX2", 13) == 0){
		GSM_blSendCommand("at+cusd=1,\"*102#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_SMS;
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, smsMesg.content.text + 14);
		HostChannel.state = CH_STATE_BUSY;

		return;
	}

	/* config parameter: */
	if(strncmp(smsMesg.content.text, "SCFG,SET", 8)==0)
	{
		dbg_setConfigParams(smsMesg.content.text + 4);

		config_apply(&Config);
		os_dly_wait(200);
		/* Reset system */

		vIWDG_Init();
		tsk_lock();
		return;
	}

	/* administrator reset. Hide code */
	if(strncmp(smsMesg.content.text, "SOS_RESET", 9)==0)
	{
		/* reset device */
		vIWDG_Init();
		tsk_lock();
		return;
	}

	/* reset camera buffer idx */
	if(strncmp(smsMesg.content.text, "CRST", 4)==0)
	{
		vSDBufferReset();

		/* send sms */
		blSMS_Puts(smsMesg.header.text, "Camera index reset OK");

		return;
	}

	/* Reset sdfifo buffer */
	if(strncmp(smsMesg.content.text, "BRST", 4)==0)
	{
		sdfifo_reset();

		/* send sms */
		blSMS_Puts(smsMesg.header.text, "Logs index reset OK");

		return;
	}
	/* send camera index for debug */
	if(strncmp(smsMesg.content.text, "CAMERA_INFO", 11)==0)
	{
		/* new memory box */
		p = (S8*)calloc(160, sizeof(S8));
		if(p == NULL)
			return;
		/* modify data */
		sprintf(p, "W:%u,R:%u,C:%u", uGetCameraWriteIdx(), uGetCameraReadIdx(), uGetImageCount());
		/* send sms */
		blSMS_Puts(smsMesg.header.text, p);

		free(p);
		return;
	}

	/* Handle RLOG: RLOG,181012,3 */
	if(strncmp(smsMesg.content.text, "RLOG", 4)==0)
	{
		vSendLogData(smsMesg.content.text + 4);

		/* send sms */
		blSMS_Puts(smsMesg.header.text, "START READ LOG");

		return;
	}

	// IMAGE_BACKUP,ddmmyy\ii_nnnn.jpeg
	if(strncmp(smsMesg.content.text, "IMAGE_BACKUP", 12)==0)
	{
		str[0] = bBackupImageToBuffer(smsMesg.content.text + 13, smsMesg.content.text + 20);

		/* send sms */
		if(str[0] == 0)
			blSMS_Puts(smsMesg.header.text, "BACKUP IMAGE SUCCESS");

		else
			blSMS_Puts(smsMesg.header.text, "BACKUP IMAGE ERROR");

		return;
	}
	
	
	/******** administrator code ******************************/
	// change user.
	if(strncmp(smsMesg.content.text, "ADMIN_SU", 8)==0)
	{
		int8_t* item[3];
		int8_t n;
		n = string_split((U8**)item, (U8*)smsMesg.content.text, ",", 2);
		if(n != 2){
			blSMS_Puts(smsMesg.header.text, "supper user fail");
			return;
		}
		// change user.
		if(user_change(smsMesg.header.text, item[1])== __TRUE)
			blSMS_Puts(smsMesg.header.text, "supper user changed");
		else
			blSMS_Puts(smsMesg.header.text, "supper user fail");
		return;
	}
	
	// change pass
	if(strncmp(smsMesg.content.text, "ADMIN_PW", 8)==0)
	{
		int8_t* item[3];
		int8_t n;
		n = string_split((U8**)item, (U8*)smsMesg.content.text, ",", 2);
		if(n != 2){
			blSMS_Puts(smsMesg.header.text, "password fail");
			return;
		}
			
		// change user.
		if(pass_change(smsMesg.header.text, item[1])== __TRUE)
			blSMS_Puts(smsMesg.header.text, "password changed");
		else
			blSMS_Puts(smsMesg.header.text, "password fail");
		
		return;
	}
	
	// get user
	if(strncmp(smsMesg.content.text, "GET_ADMIN", 9)==0)
	{
		blSMS_Puts(smsMesg.header.text, Config.admin.user);		
		return;
	}
	
	// get pass
	if(strncmp(smsMesg.content.text, "GET_PASS", 8)==0)
	{
		if(strcmp(smsMesg.header.text, Config.admin.user)==0)
			blSMS_Puts(smsMesg.header.text, Config.admin.pass);
		
		return;
	}
	
	/*TM_IMEDIATELY_UPDATE*/
	if(strncmp(smsMesg.content.text, "TM_IMEDIATELY_UPDATE", 20)==0)
	{
		TM_ImediatelyUpdate();
		return;	
	}

	/* send camera index for debug */
	if(strncmp(smsMesg.content.text, "TM_INFO",7)==0)
	{
		p = smsMesg.header.text + 1;
		for(ch=0;ch<smsMesg.header.header_len;ch++)
		{
			if(*p == '"')
			{
				/* end of phone number */
				*p = 0;
				/* new memory box */
				p = (S8*)calloc(64, sizeof(S8));
				/* modify data */
				sprintf(p, "TMenable:%d,TMtype:%d,TMtimeout:%d,Ridx:%d,Widx:%d",\
				Config.TM.tm_used,Config.TM.tm_type,Config.TM.tm_timeout,TM_GetReadBufferIndex(),TM_GetWriteBufferIndex());
				/* send sms */
				blSMS_Puts(smsMesg.header.text+1, p);
				break;
			}
			p++;
		}
		os_dly_wait(200);
		free(p);
		return;
	}
}

/*
 * \brief	void device_getinfo()
 * 			return status of device as GPRS signal, GPS signal, SD card info.
 * \param	p is pointer point to memory.
 * \return 	none.
 */
void device_getinfo(uint8_t *p)
{
	uint8_t *s[3];
	uint8_t intstring[32];
	// check GSM signal
	if(GSM_GetRespondCommand("at+csq\r", "\r\nOK\r\n", p, 32, 50)==0)
	{
		strcpy(p, "SIGNAL UNKNOWN");
	}else
	{
		string_split(s, p, "\r\n", 3);
		strcpy(p, s[0]);
	}

	/* modify data */
	if(cl_state == GPRS_NONE)
		strcat(p, ", GPRS SEARCHING, ");
	else if(cl_state == GPRS_OPEN)
		strcat(p, ", SERVER CONNECTING, ");
	else if(( cl_state == GPRS_READ_BUFFER) || ( cl_state == GPRS_SEND_DATA) || ( cl_state == GPRS_WAIT_ACK))
		strcat(p, ", SERVER CONNECTED, ");

	if(sdCard_st==0)
		strcat(p, "SDCARD OK, ");
	else
		strcat(p, "SDCARD ERROR, ");

	if(gpsInfo.status == 0)
		strcat(p, "GPS SEARCHING, ");
	else
		strcat(p, "GPS OK, ");

	sprintf(intstring, "%X.%X, \x0", sdfifo_getInItem(), sdfifo_getOutItem());
	strcat(p, intstring);

	// date time
	vRTCGetDate(&intstring[0], &intstring[1], &intstring[2]);
	sprintf(intstring + 3, "%02u/%02u/%02u. \x0", intstring[2], intstring[1], intstring[0]);
	strcat(p, intstring + 3);

	vRTCGetTime(&intstring[0], &intstring[1], &intstring[2]);
	sprintf(intstring + 3, "%02u:%02u:%02u, \x0", intstring[0], intstring[1], intstring[2]);
	strcat(p, intstring + 3);

	// gprs report.
	memset(intstring, 0, 32);
	GPRS_GetReport(intstring);
	strcat(p, intstring);

	strcat(p, ", ");
	// driverkey plugin
	p[strlen(p)] = getDriverPlugIn();
}
