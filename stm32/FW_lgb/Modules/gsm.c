/*
 * gsm.c
 *
 *  Created on: Feb 10, 2012
 *      Author: Van Ha
 *
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"
#include "platform_config.h"

#include "serial.h"
#include "utils.h"
#include "sim900.h"
#include "debug.h"

#include "gsm.h"
#include "gprs.h"
#include "http.h"

#define 	TCPIP_CLOSE				0
#define 	TCPIP_OPEN				1
#define 	TCPIP_SENDING			2

#define REQUEST_SEND_SMS 0x0001

/******* VARIABLEs *************************/
const S8* rspOk		= 	"\r\nOK\r\n";
const S8* rspError  = "\r\nERROR\r\n";
const S8* rspPrompt = "\r\n> ";

OS_MUT gsmMutex;
OS_TID gsmTaskID;
OS_TID SMS_tskID;

static U64 SMS_Stack[500/8];

/***** local ********************************/
BOOL blSetGSMProfiles(void);
__task void vSMS_Task(void);

/*** ham phuc vu unsolicited code. ***/
static void vCGREG_Unsol(S8* p);
static void vConnect_Unsol(S8 *p);
static void vData_Unsol(S8* p);
static void vSendOk_Unsol(S8 *p);
static void vSMS_Unsol(S8 *p);
static void vClose_Unsol(S8 *p);
static void vCall_Acept(S8 *p);
static void vSim_Fail(S8 *p);
static void vSim_NotInsert(S8 *p);
static void vSendSMS_finish(S8 *p);
static void vCusd_Unsol(S8 *p);

/******** typedef *****************************/
typedef void (*Unsol_Callback_t) (S8 *p);
typedef struct _unsol_code_t{
	S8 *name;
	S8 name_size;
}unsol_code_t;

typedef struct{
	uint8_t PhoneNum[20];
	uint8_t Content[180];
} SMS_Msg_t;

#define PMAIl_MSG_SIZE		1024		// Size of each pMail message

_declare_box(GSM_RxBuf, PMAIl_MSG_SIZE, 4);		/* 1024 byte */
U8 *pGSM_RxBuf, *pGSM_RxMailbox;
os_mbx_declare (GSM_RxMailbox, 4);

S8 rspBuf[256];
SMS_Msg_t SMSBuf;

// stack for task gsm
U64 gsmStack[400/8];

uint32_t wdt_gsm_tsk_cnt = 0;

GSM_Flag_t xGSMflag;

const unsol_code_t UNSOL_CODE[] =
{
		{"+CGREG:", 7},
		{"CONNECT OK", 10},
		{"+CMT:", 5},
		{"+IPD", 4},
		{"SEND OK", 7},
		{"CLOSE", 5},
		{"+PDP:", 5},
		{"SHUT", 4},
		{"RING", 4},
		{"+CPIN:", 6},
		{"+CMGS:", 6},
		{"+CUSD:", 6}
};

#define UNSOL_COUNT			sizeof(UNSOL_CODE)/sizeof(UNSOL_CODE[0])

Unsol_Callback_t Unsol_Callback[] =
{
	vCGREG_Unsol,
	vConnect_Unsol,
	vSMS_Unsol,
	vData_Unsol,
	vSendOk_Unsol,
	vClose_Unsol,
	vClose_Unsol,
	vClose_Unsol,
	vCall_Acept,
	vSim_Fail,
	vSendSMS_finish,
	vCusd_Unsol,
};

#define UNSOL_FUN_COUNT			sizeof(Unsol_Callback)/sizeof(Unsol_Callback[0])


#ifdef GSM_DEBUG
void gsm_err(S8* d)
{
	if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
		USART_PrCStr(USART1,d);
}
#else
void gsm_err(S8* d)
{
}

#endif

/* Mutex API */
void vLockGSMData(void)
{
	os_mut_wait(gsmMutex, 0xFFFF);
}

void vUnlockGSMData(void)
{
	os_mut_release(gsmMutex);
}

void GSM_MutexInit(void){
	os_mut_init(gsmMutex);
}

/***** task **************************************************/
__task void vGSM_Task(void)
{
	U16 i;
	S8 *p, ch;
	U16 tmo;
	int16_t id;

	/* Init buffer and recieve mailbox */
	_init_box(GSM_RxBuf, sizeof(GSM_RxBuf), PMAIl_MSG_SIZE);
	os_mbx_init(GSM_RxMailbox, sizeof(GSM_RxMailbox));

	//p = rspBuf;
	id = 0;
	for(;;)
	{
		wdt_gsm_tsk_cnt = 0;
		// check if data is available in the SimBuffer
		os_mut_wait(gsmMutex, 0xFFFF);

		if(SIM900_ReadChar(&ch) == 0)
		{
			os_mut_release(gsmMutex);
			os_dly_wait(200/OS_TICK_RATE_MS);		// delay 20ms
			continue;
		}

// 		*p++ = ch;
// 		if(p >= (sizeof(rspBuf) + rspBuf)-1) p = rspBuf;		// if error.
// 		*p = 0;
		rspBuf[id++] = ch;
		if(id >= sizeof(rspBuf)-1) id = 0;
		// read unsolicited code until finish.
		tmo = 5;
		while(__TRUE)
		{
			if(tmo == 0)
			{
				break;
			}
			// wait data ready
			if(SIM900_ReadChar(&ch) == 0)
			{
				tmo--;
				os_dly_wait(100/OS_TICK_RATE_MS);		// delay 100ms
			}
			else
			{
// 				*p++ = ch;
// 				if(p >= (sizeof(rspBuf) + rspBuf)-1) p = rspBuf;		// if error.

// 				*p = 0;
				rspBuf[id++] = ch;
				if(id >= sizeof(rspBuf)-1) id = 0;

				if((ch == ':') || ((ch == '\n')/*&&(*(p-2)=='\r')*/))
				{
					rspBuf[id] = 0;
					/* xu ly bo qua \r\n lan dau. */
					//if(p > (rspBuf+3))
					if(id > 2)
					{
						// kiem tra unsolicited code.
						for(i=0;i<UNSOL_COUNT;i++)
						{
							if(strncmp((const char*)rspBuf, (const char*)UNSOL_CODE[i].name, UNSOL_CODE[i].name_size)==0)
							{
								Unsol_Callback[i](rspBuf);   // 0123245+IPD,12:nxjxixik

								// pass other task.
								os_dly_wait(1);
								break;
							}
						}

						//p = rspBuf;
						id = 0;
						break;
					}
					else
					{
						/* if "crlf" then abort */
						//p = rspBuf;
						id = 0;
					}
				}
			}
		}

		os_mut_release(gsmMutex);
	}
}


/******** Static functions ******************************************/
/*
 * notes: cac ham phuc vu co unsolicited code.
 */
static void vSim_NotInsert(S8 *p)
{
	xGSMflag.SIM_flag = 0;	// sim not insert.
}
// co the dung con tro ham cho vao cac ham static nay de lay data len cho cac layer lop tren.
static void vCusd_Unsol(S8 *p)
{
	// p = [space]0,"...",...
	S8 *pSms = NULL, *pMail;
	S8 ch;
	U16 tmo;
	uint32_t timeout;

	/* Alloc memory from mailbox */
	pSms = _calloc_box(GSM_RxBuf);
	pMail = pSms;

	memcpy(pSms, "$CUSD,", 6);
	pSms += 6;
	tmo=30;
	while(__TRUE)
	{
		if(tmo == 0)
			break;

		if(SIM900_ReadChar(&ch)==0){
			tmo--;
			os_dly_wait(100/OS_TICK_RATE_MS);		// delay 100ms
		}
		else{
			if(pMail != NULL)
			{
				*pSms++ = ch;
				// neu sai du lieu, khong nhan dc crlf. bo qua du lieu
				if(pSms >= pMail + PMAIl_MSG_SIZE - 1)
				{
					timeout = PMAIl_MSG_SIZE;
					SIM900_ClearBuffer(timeout);

					return;

				}
				*pSms = 0;
			}

			// neu gap crlf thi kiem tra xem da lay du 2 lan hay chua.
			if((ch == '\n')&&(*(pSms-2)=='\r')){
				break;
			}
		}
	}

	if(pMail == NULL)
		return;

	if((tmo > 0) && (os_mbx_check (GSM_RxMailbox) > 0))
	{
		*(pSms - 2) = '#';	*(pSms -1) = 0;

		// put to mailbox
		os_mbx_send(GSM_RxMailbox, pMail, 0xFFFF);
	}
	else
	{
		// free buffer.
		_free_box(GSM_RxBuf, pMail);
	}
}

/* sim fail */
static void vSim_Fail(S8 *p)
{
	uint32_t tmo = 30;
	int8_t ch, buf[20], id = 0;
	gsm_err("\r\nSIM CARD FAIL\r\n");
	
	while(__TRUE)
	{
		if(tmo == 0)
			break;

		if(SIM900_ReadChar(&ch)==0){
			tmo--;
			os_dly_wait(1/OS_TICK_RATE_MS);		// delay 100ms
		}
		else{
			buf[id++] = ch;
			// neu gap crlf thi kiem tra xem da lay du 2 lan hay chua.
			if((ch == '\n')&&(buf[id-2]=='\r')){
				if(strncmp(buf, " NOT READY", 10)==0){
					xGSMflag.SIM_flag = 1;
				}
				else if(strncmp(buf, " NOT INSERTED", 10)==0){
					xGSMflag.SIM_flag = 0;
				}
				else if(strncmp(buf, " READY", 6)==0){
					xGSMflag.SIM_flag = 2;
				}
				else{
					xGSMflag.SIM_flag = 3;
				}
				break;
			}
		}
	}
}

// Ham nay dung cho cu vien thong do luong va kiem dinh.
static void vCall_Acept(S8 *p)
{
#ifdef _TELCOM_TEST
	static U8 ringCount = 0;
	// auto answer voice call.
	if(++ringCount >= 2)
	{
		gsm_err("\r\nNHAN CALL\r\n");
		GSM_blSendCommand("ATA\r", rspOk,  50);
		ringCount = 0;
	}
#endif
}
// end

static void vClose_Unsol(S8 *p)
{
	xGSMflag.TCPIPStaus = TCPIP_CLOSE;
}

/**
 *
 */

static void vCGREG_Unsol(S8* p)
{
	S8 ch;
	S8 tmo = 100;
	// \r\n+CGREG: x\r\n
	while(SIM900_GetItemSize() < 4)
	{
		os_dly_wait(10/OS_TICK_RATE_MS);	// delay 10ms
		if(tmo-- == 0)
			break;
	}

	if(tmo)
	{
		SIM900_ReadChar(&ch);	// ' '
		SIM900_ReadChar(&ch); // 'x'
		xGSMflag.RecvCGReg = ch - '0';
		SIM900_ReadChar(&ch);	// '\r'
		SIM900_ReadChar(&ch); // '\n'
	}
}

static void vConnect_Unsol(S8 *p)
{
	// \r\nCONNECT OK\r\n
	//xGSMflag.SocketOpened = 1;
	xGSMflag.TCPIPStaus = TCPIP_OPEN;
}

static void vSendOk_Unsol(S8 *p)
{
	// \r\nSEND OK\r\n
	//xGSMflag.SocketSendOk = 1;
	xGSMflag.TCPIPStaus = TCPIP_OPEN;
}

static void vData_Unsol(S8* p)
{
	U16 pckLen;
	S8 *pData;
	static S8 *pMail = NULL;
	S8 ch;
	uint32_t timeout;
	uint8_t* uItem[3];
	uint8_t ItemCount;

/* Parse IPD message: ,x: */
	ItemCount = string_split((U8**)uItem, (U8*)p, ",:", 2);

	if(ItemCount != 2){
		SIM900_ClearBuffer(0);
		return;
	}
	
	pckLen = atoi((const char*)uItem[1]);

	if((pckLen >= PMAIl_MSG_SIZE-1))
	{
		/*
		 * doc het du lieu trong buffer neu kich thuoc ban tin bi sai lon hon kich thuoc buffer.
		 */
		timeout = PMAIl_MSG_SIZE;
		SIM900_ClearBuffer(timeout);

		return;
	}

/* create  buffer */
	pMail = _calloc_box(GSM_RxBuf);
	pData = pMail;

	// Read all data and put into buffer
	while(pckLen--)
	{
		/* Get data from SIM900 with timeout: 1000ms for each char */
		timeout = 10;
		while( (SIM900_ReadChar(&ch)==0) && (timeout--))
			os_dly_wait(1);

		// Process when timeout
		if(timeout == 0){
			if(pMail != NULL){
				_free_box(GSM_RxBuf, pMail);
			}
			return;
		}

		if((pMail != NULL) && (pData <= (pMail + PMAIl_MSG_SIZE)-3)){
			*pData++ = ch;
			*pData = 0;
		}
	}

	if(pMail == NULL){
		return;
	}

	// put to mailbox
	if (os_mbx_check (GSM_RxMailbox) > 0)
	{
		os_mbx_send(GSM_RxMailbox, pMail, 0xFFFF);
	}
	else
	{
		_free_box(GSM_RxBuf, pMail);
	}
}

static void vSMS_Unsol(S8 *p)
{
	S8 *pSms = NULL, *pMail;
	S8 ch, end = 0;
	U16 tmo;
	uint32_t timeout;
	// <crlf>+CMT: "+841692128985","","12/02/03,22:56:23+28"<crlf>
	//			thu mot tin nhan dai 1234567890!@#$%?&*()_+<crlf>
	//		poiter -> ':'.

	/* Alloc memory from mailbox */
	pSms = _calloc_box(GSM_RxBuf);
	pMail = pSms;

	memcpy(pSms, "$SMS,", 5);
	pSms += 5;
	tmo=30;
	while(__TRUE)
	{
		if(tmo == 0)
			break;

		if(SIM900_ReadChar(&ch)==0){
			tmo--;
			os_dly_wait(100/OS_TICK_RATE_MS);		// delay 100ms
		}
		else{
			if(pMail != NULL)
			{
				*pSms++ = ch;
				// neu sai du lieu, khong nhan dc crlf. bo qua du lieu
				if(pSms >= pMail + PMAIl_MSG_SIZE - 1)
				{
					timeout = PMAIl_MSG_SIZE;
					SIM900_ClearBuffer(timeout);

					return;

				}
				*pSms = 0;
			}

			// neu gap crlf thi kiem tra xem da lay du 2 lan hay chua.
			if((ch == '\n')&&(*(pSms-2)=='\r')){
				if(end)
					break;
				else
					end = 1;
			}
		}
	}

	if(pMail == NULL)
		return;

	if((tmo > 0) && (os_mbx_check (GSM_RxMailbox) > 0))
	{
		*(pSms - 2) = '#';	*(pSms -1) = 0;

		// put to mailbox
		os_mbx_send(GSM_RxMailbox, pMail, 0xFFFF);
	}
	else
	{
		// free buffer.
		_free_box(GSM_RxBuf, pMail);
	}
}

static void vSendSMS_finish(S8 *p)
{
	//xGSMflag.SMS_flag = 1;
	xGSMflag.SMS_flag = SMS_SEND_OK;
}


/**********functions****************************************/
BOOL blGSM_Init(void)
{
	// open module sim900. implement in sim900.c
	if(blSim900_Open())
	{
		memset(&xGSMflag, 0, sizeof(xGSMflag));
		if(blSetGSMProfiles() != 1)
			return 0;

		if(gsmTaskID == NULL)
			gsmTaskID = os_tsk_create_user(vGSM_Task, RTX_GSM_TSK_PRIORITY, &gsmStack, sizeof(gsmStack));
			//gsmTaskID = os_tsk_create(vGSM_Task, RTX_GSM_TSK_PRIORITY);
		return 1;
	}
	return 0;
}

/**
 * @param	cmd		Cmd (command) to sent to SIm900
 * @param	res		return data
 */
BOOL GSM_blSendCommand(const S8* cmd, const S8* res, U16 timeout)
{
	BOOL ret;

	os_mut_wait(gsmMutex, 0xFFFF);

	ret = GSM_blSendCommandWithoutMutex(cmd, res, timeout);

	os_mut_release(gsmMutex);

	return ret;
}

/* Send cmd without mutex */
BOOL GSM_blSendCommandWithoutMutex(const S8* cmd, const S8* res, U16 timeout)
{
	S8 *p, ch;

	p = rspBuf;
	memset(p, 0, sizeof(rspBuf));
	xGSMflag.RecvError = 0;

	SIM900_SendCommand(cmd);

	while(__TRUE)
	{
		if(timeout == 0)
			return 0;

		if(SIM900_ReadChar(&ch)==0)
		{
			timeout --;
			os_dly_wait(10/OS_TICK_RATE_MS);		// delay 10ms
		}
		else
		{
			*p++ = ch;
			/* neu loi du lieu, gay qua buffer */
			if(p >= (rspBuf + sizeof(rspBuf) - 1))
			{
				SIM900_ClearBuffer(PMAIl_MSG_SIZE);
				return 0;
			}

			*p = 0;

			if(strstr((const char*)rspBuf, (const char*)res) != 0)
				break;
			else if(strstr((const char*)rspBuf, (const char*)rspError)!=0)
			{
				xGSMflag.RecvError = 1;
				break;
			}
		}

	}

	//os_dly_wait(200/OS_TICK_RATE_MS);
	return 1;
}


/**
 * @param	cmd		Cmd (command) to sent to SIm900
 * @param	res		return data
 */
BOOL GSM_GetRespondCommand(const S8* cmd, const S8 *end, S8* res, uint16_t size, U16 timeout)
{
	BOOL ret;

	os_mut_wait(gsmMutex, 0xFFFF);

	ret = GSM_GetResCmdpWithoutMutex(cmd, end, res, size, timeout);

	os_mut_release(gsmMutex);

	return ret;
}
/* Send cmd without mutex */
BOOL GSM_GetResCmdpWithoutMutex(const S8* cmd, const S8 *end, S8* res, uint16_t size, U16 timeout)
{
	S8 *p, ch;

	p = rspBuf;
	memset(p, 0, sizeof(rspBuf));
	memset(res, 0, size);
	xGSMflag.RecvError = 0;

	SIM900_SendCommand(cmd);

	while(__TRUE)
	{
		if(timeout == 0)
			return 0;

		if(SIM900_ReadChar(&ch)==0)
		{
			timeout --;
			os_dly_wait(50/OS_TICK_RATE_MS);		// delay 10ms
		}
		else
		{
			*p++ = ch;
			/* neu loi du lieu, gay qua buffer */
			if(p >= (rspBuf + sizeof(rspBuf) - 1))
			{
				SIM900_ClearBuffer(PMAIl_MSG_SIZE);
				return 0;
			}

			*p = 0;

			if(strstr((const char*)rspBuf, (const char*)end) != 0)
			{
				// validate size of buffer
				if(strlen(rspBuf) > size)
					return 0;
				// copy respond to buffer
				strcpy(res, rspBuf);

				break;
			}
		}

	}

	return 1;
}

/*
 * gsm flag
 */
BOOL blSIMCARD_Status(void)
{
	/* return cpin */
	return xGSMflag.SIM_flag;
}

BOOL blGSM_Status(void)
{
	BOOL ret;
	memcpy(&ret, &xGSMflag, sizeof(xGSMflag));
	return ret;
}

uint8_t get_gsm_status(void)
{
	if(xGSMflag.SIM_flag < 2)
		return 0;
	if(xGSMflag.SIM_flag > 3)
		return 1;
	if(xGSMflag.RecvCGReg != 1)
		return 2;
	
	if(xGSMflag.TCPIPStaus == 0)
		return 2;
	
	return 3;
}


/**
 * setup profiles
 */
BOOL blSetGSMProfiles(void)
{
#define COMMAND_RETRY	3
	U8 cmd_retry = COMMAND_RETRY;

	// set baud rate
	while(cmd_retry)
	{
		if((GSM_blSendCommand("AT+IPR=115200\r", rspOk, DLY_RESP)!= 0) && (xGSMflag.RecvError != 1))
			break;

		cmd_retry --;
	}
	// if timeout return 0;
	if(cmd_retry == 0)
		return 0;

	// set cfun, full gsm function
	cmd_retry = COMMAND_RETRY;
	while(cmd_retry)
	{
		if((GSM_blSendCommand("AT+CFUN=1\r", rspOk, DLY_RESP)!=0) && (xGSMflag.RecvError != 1))
			break;

		cmd_retry --;
	}
	// if timeout return 0;
	if(cmd_retry == 0)
		return 0;

	/* Config SMS Message format as Text */
	cmd_retry = COMMAND_RETRY;
	while(cmd_retry)
	{
		if((GSM_blSendCommand("AT+CMGF=1\r", rspOk, DLY_RESP)!=0) && (xGSMflag.RecvError != 1))
			break;

		cmd_retry --;
	}
	// if timeout return 0;
	if(cmd_retry == 0)
		return 0;


	// forward sms direct to uart
	cmd_retry = COMMAND_RETRY;
	while(cmd_retry)
	{
		if((GSM_blSendCommand("AT+CNMI=3,2,0,1,0\r", rspOk, DLY_RESP)!=0) && (xGSMflag.RecvError != 1))
			break;

		cmd_retry --;
	}
	// if timeout return 0;
	if(cmd_retry == 0)
		return 0;

	// set sms text mode parameter.
	cmd_retry = COMMAND_RETRY;
	while(cmd_retry)
	{
		if((GSM_blSendCommand("AT+CSMP=49,167,0,241\r", rspOk, DLY_RESP)!=0) && (xGSMflag.RecvError != 1))
			break;

		cmd_retry --;
	}
	// if timeout return 0;
	if(cmd_retry == 0)
		return 0;


	// forward sms direct to uart
	cmd_retry = COMMAND_RETRY;
	while(cmd_retry)
	{
		if((GSM_blSendCommand("AT+CSCS=\"GSM\"\r", rspOk, DLY_RESP)!=0) && (xGSMflag.RecvError != 1))
			break;

		cmd_retry --;
	}
	// if timeout return 0;
	if(cmd_retry == 0)
		return 0;

	// revision.
	GSM_blSendCommand("AT+GMR\r", rspOk, DLY_RESP);
	/* delete all sms in mem */
	GSM_blSendCommand("AT+CMGD=0,4\r", rspOk, DLY_RESP);

	xGSMflag.SIM_flag = 2;
	return 1;
}

/**
 * \fn		BOOL blGPRS_Connect(U8* apn, U8 *user, U8* pass)
 * \brief	check GPRS is valid and connect.
 * \param	apn - string as apn of network.
 * \param	user - string as name user login
 * \param	pass ...
 * \return 	Bool value, 0 - connect fail, otherwise success.
 **/
BOOL blGPRS_Open(S8* apn, S8 *user, S8* pass, U32 tmo)
{
	S8 command_buffer[64];
	/* setup apn of provider: apn_name, apn_user, apn_pass
	 * viettel: v-internet,null,null
	 * mobifone: m-wap, mms, mms
	 * vinaphone: m3-world, mms, mms
	 * */
	sprintf((char*)command_buffer, "AT+CIPCSGP=1,\"%s\",\"%s\",\"%s\"\r", apn, user, pass);
	if(GSM_blSendCommand(command_buffer, rspOk, DLY_RESP)==0)
		return 0;

	if(xGSMflag.RecvError == 1)
		return 0;

	/* register network */
	if(GSM_blSendCommand("AT+CGREG=1\r", rspOk, DLY_CGREG)==0)
		return 0;
	if(xGSMflag.RecvError == 1)
		return 0;

	/* wait unsolicited code cgreg: 1*/
	for(;tmo > 0;tmo--)
	{
		if(xGSMflag.RecvCGReg == 1)
			break;

		os_dly_wait(100/OS_TICK_RATE_MS);
	}
	/* if network ok then return success. */
	if(tmo != 0)
		return 1;

	/* check network status again */
	return blGPRS_Status();
}

BOOL blGPRS_Status(void)
{
	if(GSM_blSendCommand("AT+CGATT?\r", "\r\n+CGATT: 1\r\n\r\nOK\r\n", DLY_CGREG) == 0)
	{
		return 0;
	}
	if(xGSMflag.RecvError == 1)
		return 0;

	return 1;
}


/********** TCP/IP *****************************************************/
#define _USING_TCPIP_SIGNLE
#ifdef _USING_TCPIP_SIGNLE
/**
 * \fn		BOOL blSetTCPIPProfiles(void)
 * \brief	configuration some functions as dns, header... mode
 * \param	none
 * \return 	Bool value, 0 - setup fail, otherwise setup success
 **/
BOOL blSetTCPIPProfiles(void)
{
	if(GSM_blSendCommand("AT+CIPHEAD=1\r", rspOk, DLY_RESP)==0)
	{
		return 0;
	}
	if((blGSM_Status() & RECV_ERROR_MASK))
	{
		return 0;
	}

	return 1;
}

/**
 * \fn		BOOL blTCPIP_Connect(U8* ip, U16 port)
 * \brief	check socket is valid and connect.
 * \param	ip - string as apn of network.
 * \param	port - u16 as port connect.
 * \return 	Bool value, 0 - connect fail, otherwise success.
 **/
BOOL blTCPIP_Open(const S8* ip, const uint8_t* port, U16 tmo)
{
	S8 txBuf[64];

	if(xGSMflag.TCPIPStaus != 0)
		xGSMflag.TCPIPStaus = 0;

	sprintf((char*)txBuf, "AT+CIPSTART=\"UDP\",\"%s\",\"%s\"\r", (const char*)ip, port);
	if(GSM_blSendCommand(txBuf, rspOk, DLY_CONNECT)==0)
		return 0;

	if(xGSMflag.RecvError == 1)
		return 0;

	for(;tmo > 0;tmo--){
		if(uiTCPIP_Status()==1)
			break;
		os_dly_wait(100/OS_TICK_RATE_MS);
	}

	return tmo;
}

/**
 *
 */
void vTCPIP_Close(void)
{
	GSM_blSendCommand("AT+CIPCLOSE\r", rspOk, DLY_RESP);
}

/**
 *
 */
void vTCPIP_Shutdown(void)
{
	U16 tmo;
	if(GSM_blSendCommand("AT+CIPSHUT\r", rspOk, DLY_RESP)==0)
		return;

	tmo = 30;
	while(xGSMflag.TCPIPStaus != 0 && (tmo--))
	{
		os_dly_wait(100/OS_TICK_RATE_MS);
	}
}

/*
 *
 */
BOOL blTCPIP_SendReady(U16 tmo)
{
	for(;tmo;tmo--)
	{
		if(xGSMflag.TCPIPStaus == 1)
		{
			return 1;
		}
		os_dly_wait(100/OS_TICK_RATE_MS);
	}
	return 0;
}

/**
 * \fn 		BOOL blSocket_Puts(const U8* data)
 * \brief	send a string in queue to socket.
 * \param	data - string in queue.
 * \return 	0 - fail, otherwise - success.
 **/
BOOL blTCPIP_Puts(const S8* data, U16 tmo)
{
	// check sim900 ready.
	if(blTCPIP_SendReady(tmo)==0)
		return 0;
	// send.
	os_mut_wait(gsmMutex, 0xFFFF);

	if(GSM_blSendCommandWithoutMutex("AT+CIPSEND\r", rspPrompt, DLY_RESP)==0)
	{
		os_mut_release(gsmMutex);
		return 0;
	}

	if(xGSMflag.RecvError == 1){
		os_mut_release(gsmMutex);
		return 0;
	}

	xGSMflag.TCPIPStaus = TCPIP_SENDING;
	os_dly_wait(10/OS_TICK_RATE_MS);

	SIM900_WriteCStr(data);

	SIM900_Putc(0x1A);		// ctrl + z.

	os_mut_release(gsmMutex);

	return 1;
}

/**
 * \fn 		BOOL blSocket_Puts(const U8* data)
 * \brief	send a string in queue to socket.
 * \param	data - string in queue.
 * \return 	0 - fail, otherwise - success.
 **/
BOOL blTCPIP_Write(const S8* data, U16 size, U16 tmo)
{
	S8 str[32];
	// check sim900 ready.
	if(blTCPIP_SendReady(tmo)==0)
		return 0;

	memset(str, 0, 32);
	// send.
	os_mut_wait(gsmMutex, 0xFFFF);

	sprintf(str, "AT+CIPSEND=%u\r", size );
	if(GSM_blSendCommandWithoutMutex(str, rspPrompt, DLY_RESP)==0)
	{
		os_mut_release(gsmMutex);
		return 0;
	}

	if(xGSMflag.RecvError == 1){
		os_mut_release(gsmMutex);
		return 0;
	}

	xGSMflag.TCPIPStaus = TCPIP_SENDING;
	os_dly_wait(200/OS_TICK_RATE_MS);

	SIM900_WriteStr((S8*)data, size);
	os_mut_release(gsmMutex);

	// doi nhan dc sendok. 500ms
	while(blTCPIP_SendReady(5)==0)
	{
		// chong loi uart
		GSM_blSendCommand("AT\r", "\r\nOK\r\n", 10);

		// if error return fail.
		if(xGSMflag.RecvError == 1){
			os_mut_release(gsmMutex);
			return 0;
		}

		if(size == 0) break;
		size = size/3;
	}

	return 1;
}

BOOL blTCPIP_Gets(S8 **buf, U16 tmo)
{
	S8 *ptr;
	if(os_mbx_wait(GSM_RxMailbox, (void**)&ptr, tmo) == OS_R_TMO){
		return 0;
	}
	*buf = ptr;
	return 1;
}

void vTCPIP_FreeRxBuf(S8* ptr)
{
	if(ptr != NULL)
		_free_box(GSM_RxBuf, ptr);
}

/**
 * \return:
 * 		0 - disconnect
 * 		1 - connect, ready for trans
 * 		2 - sending
 */
U16 uiTCPIP_Status(void)
{
	return xGSMflag.TCPIPStaus;
}

#else
/**
 * \fn		BOOL blSetTCPIPProfiles(void)
 * \brief	configuration some functions as dns, header... mode
 * \param	none
 * \return 	Bool value, 0 - setup fail, otherwise setup success
 **/
BOOL blSetTCPIPProfiles(void)
{
	GSM_blSendCommand("AT+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\"\r", rspOk, DLY_RESP);
	GSM_blSendCommand("AT+CDNSORIP=0\r", rspOk, DLY_RESP);
	if(GSM_blSendCommand("AT+CIPHEAD=1\r", rspOk, DLY_RESP)!=1)
	{
		return 0;
	}

	GSM_blSendCommand("AT+CIPMUX=1\r", rspOk, DLY_RESP);

	//if(bSendCommand("AT+CIPSRIP=1\r", blResOk, DLY_RESP)==0)
	//		return 0;

	return 1;
}
#endif

//***** SMS FUNCTIONS ****************************//

//BOOL blSMS_Puts(S8* phone, S8* content) 
BOOL blSMS_Send(S8* phone, S8* content)
{
	U32 tmo = 1000;
	S8 str[32];
	char *p, ch;

	sprintf((char*)str, "AT+CMGS=\"%s\"\r", phone);

	os_mut_wait(gsmMutex, 0xFFFF);

	if(GSM_blSendCommandWithoutMutex(str, rspPrompt, DLY_RESP) == 0)
	{
		os_mut_release(gsmMutex);
		return 0;
	}

	if(xGSMflag.RecvError == 1){
		os_mut_release(gsmMutex);
		return 0;
	};

	os_dly_wait(100/OS_TICK_RATE_MS);
	SIM900_WriteCStr(content);

	SIM900_Putc(0x1A);		// ctrl + z.

	/* wait +cmgs: rm ok*/
	p = str;

	while(__TRUE)
	{
		if(tmo == 0)
			break;

		if(SIM900_ReadChar(&ch)==0)
		{
			tmo --;
			os_dly_wait(10/OS_TICK_RATE_MS);		// delay 10ms
		}
		else
		{
			*p++ = ch;
			if(p >= (str + sizeof(str) - 1))
			{
				SIM900_ClearBuffer(1024);
				os_mut_release(gsmMutex);
				return 0;
			}

			*p = 0;

			if(strstr((const char*)str, (const char*)"\r\nOK\r\n") != 0)
				break;
		}
	}

	os_mut_release(gsmMutex);

	if(tmo == 0)
	{
		//SIM900_Putc(27);		// ESC.
		return 0;
	}

	return 1;
}


BOOL blSMS_Puts(S8* phone, S8* content)
{
	if((strlen(content) > 160) || (strlen(phone) > 20) || (phone == NULL) || (content == NULL) || (SMS_tskID != NULL))
	{
		return 0;
	}

	/* Copy to SMS buffer */
	memset(&SMSBuf, 0, sizeof(SMSBuf));
	memcpy(&SMSBuf.PhoneNum, phone, strlen(phone));
	memcpy(&SMSBuf.Content, content, strlen(content));


	SMS_tskID = os_tsk_create_user(vSMS_Task, RTX_SMS_TSK_PRIORITY, &SMS_Stack, sizeof(SMS_Stack));
	//SMS_tskID = os_tsk_create(vSMS_Task, RTX_SMS_TSK_PRIORITY);

	return 1;
}
/**
 * \fn		__task void vSMS_Task(void)
 * \brief	process sending SMS
 * \param	none
 * \return 	
 **/
__task void vSMS_Task(void)
{

	S8 try;

	if(FWUpgradeIsRunning())
	{
		xGSMflag.SMS_flag = SMS_IDLE;
		SMS_tskID = NULL;
		os_tsk_delete_self();
	}

	//Set SMS_flag to stop vClient_Trans
	xGSMflag.SMS_flag = SMS_SENDING;

	/* Send SMS 3 times */
	try = 3;
	while(try--){
		if(blSMS_Send(SMSBuf.PhoneNum,SMSBuf.Content) == 0){
			gsm_err("Resend SMS\r\n");
			lbSim900_SoftReset();
			os_dly_wait(15000/OS_TICK_RATE_MS);
		}
		else {
			xGSMflag.SMS_flag = SMS_SEND_OK;
			break;
		}

		/* Resend 3 times fail */
		if(try == 0){
			/* Hard reset SIM900 */
			blSim900_Open();
			xGSMflag.SMS_flag = SMS_IDLE;
		}

	}

	SMS_tskID = NULL;
	os_tsk_delete_self();
}

uint8_t WDT_Check_GsmTask_Service(void)
{
	wdt_gsm_tsk_cnt++;
	
	if(wdt_gsm_tsk_cnt >= WDT_GSM_TSK_MAX_TIMING)
		return 1;
	
	return 0;
}

int8_t GSM_Mailbox_Puts(int8_t *s)
{
	int8_t* p = _calloc_box(GSM_RxBuf);
	if(p == NULL)
		return -1;
	
	strcpy(p, s);
	if (os_mbx_check (GSM_RxMailbox) > 0)
	{
		os_mbx_send(GSM_RxMailbox, p, 0xFFFF);

		return 0;
	}

	return -1;
}

