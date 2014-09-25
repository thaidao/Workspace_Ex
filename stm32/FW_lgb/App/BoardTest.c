/*
 * telCom_test.c
 *
 *  Created on: Feb 24, 2012
 *      Author: Van Ha
 */

/* Include header file */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "PeriodicService.h"
#include "Platform_Config.h"

#include "utils.h"

#include "gpsModule.h"
#include "gsm.h"
#include "adc.h"
#include "counter.h"
#include "rtc.h"
#include "voice.h"
#include "drvkey.h"

// define
#define SMS_HEADER_SIZE		4

// private variables
const S8* SMS_HEADER = "$SMS";


/********** TASK TEST *************************************************/

void vBoard_Debug(S8 *d)
{
	dbg_print(d);
}

/** task for test sim900 call, sms, no gprs. ***/
#ifdef 	_TELCOM_TEST

void vReplySMS(S8 *p);
__task void vTelCom_Test(void);

void vTelCom_Init(void)
{
	os_tsk_create(vTelCom_Test, 2);
}

__task void vTelCom_Test(void)
{
	S8 *ptr;
	static U8 tel_state = 0;

	for(;;)
	{
		switch(tel_state)
		{
		case 0:
			vBoard_Debug("\r\nOPEN SIM900\r\n");
			// open sim900.
			if(blGSM_Init()==1)
			{
				/* check gprs signal */
				blSetTCPIPProfiles();

				os_dly_wait(2000);

				tel_state = 1;
				//blSMS_Puts("01692321648", "kiem tra phan sms");

				vBoard_Debug("\r\nSIM900 Ready\r\n");
			}
			break;
		case 1:
			if(blTCPIP_Gets(&ptr, 0x0100)==1)
			{
				vBoard_Debug("\r\nBAN TIN\r\n");
				// sms: $SMS,[sp]"+841692128985","","12/02/03,22:56:23+28"<crlf>
				//			thu mot tin nhan dai 1234567890!@#$%?&*()_+<crlf>
				if(strncmp((const char*)ptr, (const char*)SMS_HEADER, SMS_HEADER_SIZE)==0)
				{
					vReplySMS(ptr+SMS_HEADER_SIZE);
				}

				// release memory
				vTCPIP_FreeRxBuf(ptr);
			}
			break;
		}
	}
}


/***** functions *******************************************/
void vReplySMS(S8 *p)
{
	S8 *phone;
	S8 *content;
	S8 *pSms, *pMail;
	S8 ch;
	U16 tmo;
	// p = [space]"+841692128985","","12/02/03,22:56:23+28"<crlf>
	//			thu mot tin nhan dai 1234567890!@#$%?&*()_+<crlf>
	//		poiter -> ':'.

	pSms = p;
	pMail = pSms;

	// pMail = _"sodt","",....<crlf>noidung<crlf>
	phone = (S8*)calloc(12, sizeof(S8));
	content = (S8*)calloc(160, sizeof(S8));
	pSms = pMail;

	while((*pMail != 0) && (*pMail != '"')) pMail++;
	pMail ++;
	tmo = 0;
	memset(phone, 0, 12);
	// copy phone number.
	while((*pMail != 0) && (*pMail != '"'))
	{
		phone[tmo++] = *pMail++;
	}

	do{
		ch = *pMail++;
		if(ch == 0) break;

		if((ch=='\r')&&(*pMail=='\n'))
			break;
	}while(1);
	pMail++;
	// noi dung tin nhan + "REPLY:"
	memset(content, 0, 160);
	memcpy(content, "REPLY:", 6);
	tmo = 6;
	do{
		ch = *pMail++;
		if(ch == 0) break;

		if((ch=='\r')&&(*pMail=='\n'))
			break;

		content[tmo++] = ch;
	}while(1);

	blSMS_Puts(phone, content);

}
#endif


__task void vTestModeTask(void)
{
	for(;;)
	{

	}
}


#ifdef FIRM_TEST
void vFirmTest(void)
{
	U16 i;
	S8 ch[2], *p;

	DBG_PrCStr("$TES,00,");

	// read ID driver key.

	p = (S8*)&Gh12_Mesg;
	for(i=0;i<sizeof(GPRS_Message_base_t);i++)
	{
		hex2ascii(*p++, ch);
		DBG_PrStr(ch, 2);
	}

	DBG_PrChar('#');
}
#endif


#ifdef USE_CERTIFICATE
void vCertificate_Mode(S8* data, U16 size)
{
	U16 i;
	S8 ch[2], *p;

	DBG_PrCStr("$DBG,00,");

	// read ID driver key.

	p = data;
	for(i=0;i<size;i++)
	{
		hex2ascii(*p++, ch);
		DBG_PrStr(ch, 2);
	}

	DBG_PrChar('#');
}
#else
void vCertificate_Mode(S8* data, U16 size)
{

}
#endif

