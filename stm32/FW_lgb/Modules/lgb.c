/*
 * lgb.c
 *
 *  Created on: Nov 21, 2012
 *      Author: ThaiDN
 */
								 
#include <stdio.h>
#include <stdlib.h>

#include "rtl.h"
#include "stm32f10x.h"

#include "string.h"

#include "gprs.h"
#include "platform_config.h"
#include "debug.h"
#include "utils.h"
#include "loadConfig.h"
#include "serial.h"
#include "debug.h"
#include "gpsmodule.h"
#include "rtc.h"

#include "lgb.h"
#include "gpsModule.h"


//uart for lgb
#define LGB_PORT			USART3
#define LGB_RCC_Periph		RCC_APB1Periph_USART3
#define LGB_IRQHandler		USART3_IRQHandler

#define LGB_RCC_IO_PORT_TX	RCC_APB2Periph_GPIOD
#define LGB_RCC_IO_PORT_RX	RCC_APB2Periph_GPIOD

#define LGB_IO_PORT_TX		GPIOD
#define LGB_IO_PORT_RX		GPIOD

#define LGB_IRQ				USART3_IRQn

//common definition
#define RTX_LGB_PRIORITY  2

#define LGB_BUFFER_MAX_SIZE 192
#define LGB_LGT_MSG_LEN 	43

#define LGB_MSG_MAX_LEN		67
#define LGB_MSG_MIN_LEN		55

#define LGB_FRAME_ID_LEN	3 //gps, lgt

//tick timer
#define TM_TMO_TICK 10
//#define TM_DISCONNECT_TIMEOUT_MAX  300*100*TM_TMO_TICK
//#define TM_DISCONNECT_TIMEOUT_MAX  30000   //5 min to set error flag
#define TM_DISCONNECT_TIMEOUT_MAX  6000   	 //1 min to set error flag
#define TM_DISCONNECT_TIMEOUT_MAX2 90000	 //15 min to set error flag

typedef enum{
	LGB_WAIT_G = 0,
	LGB_WAIT_P,
	LGB_WAIT_S,
	LGB_GET_CAR_STATUS,
	LGB_VALID_GPS_CHECKSUM,
	LGB_GET_LGT_MSG,
	LGB_END_MSG
}LGB_State_e;

LGB_State_e LgbState = LGB_WAIT_G;

typedef enum{
	TM_GET_DATA = 0,
	TM_WAIT_EVENT,
	TM_SAVE_DATA
}TM_State_e; 

typedef enum{
	CAR_ACC_OFF = 0,
	CAR_OUT_OF_SERVICE,
	CAR_IN_SERVICE
}TM_CarState_e;

typedef enum{
	TM_NONE_EVT,
	TM_START_TRIP_EVT,
	TM_STOP_TRIP_EVT
}TM_CarEvent_e;

TM_CarEvent_e CarStatusEvt;

typedef struct
{
	LGB_GPS_Frame_TypeDef GpsMsg;
	LGB_LGT_Frame_TypeDef LgtMsg;
}LgbMsg_t;

//lgb buffer
U8 LgbBuffer[LGB_BUFFER_MAX_SIZE];  //3 longest msg
U8 lgb_r_indx = 0;
U8 lgb_w_indx = 0;

//lgb message
U8 	LgbByteIdx = 0;
U8  LgbMsgLen = 55;					//default length		
U8 	LgbMessage[LGB_MSG_MAX_LEN];	//Contain gps and lgt msg
U8* pLgbData = LgbMessage;
U8  *pGpsContent,*pLgtContent;

BOOL TmGetNowFlag = __FALSE;

//lgb task
OS_TID LgbTskID = 0;
static U64 tskLGBStack[650/8];
OS_MUT TmMutex;


__task void vLgb_Task(void);

uint32_t wdt_lgb_cnt = 0;

extern gpsInfo_t gpsInfo;
BOOL TmDebugEnableFlag = __FALSE;

/***********************************************************************/
//#define LGB_DEBUG_EN 	0
#define LGB_SERVER_TEST 0
//#define LGB_USE_FAKE_DATA														 

void LGB_DebugPrCStr(S8* data)
{
	#ifdef LGB_DEBUG_EN
		USART_PrCStr(USART1, data);	
	#endif
}
void LGB_DebugPrStr(U8* data,U16 len)
{
	#ifdef LGB_DEBUG_EN	
		USART_PrStr(USART1,data,len);
	#endif	
}

/***********************************************************************/
/*
 * Lgb GPIO configuration
 */
void vLGB_GPIOConfig(void)
{
	GPIO_InitTypeDef config;

	/* Enable GPIOD clocks */
	RCC_APB2PeriphClockCmd(LGB_RCC_IO_PORT_TX | LGB_RCC_IO_PORT_RX | 

	RCC_APB2Periph_AFIO, ENABLE);
	/* Enable UART clock, if using USART2 or USART3 ... we must use RCC_APB1PeriphClockCmd

	(RCC_APB1Periph_USARTx, ENABLE) */
	RCC_APB1PeriphClockCmd(LGB_RCC_Periph, ENABLE);

	config.GPIO_Mode = GPIO_Mode_AF_PP;
	config.GPIO_Pin = GPIO_Pin_8;
	config.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(LGB_IO_PORT_TX, &config);
	// Rx
	config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	config.GPIO_Pin = GPIO_Pin_9;

	GPIO_Init(LGB_IO_PORT_RX, &config);
}

/*
 * Lgb configure baudrate
 */
void vLGB_UART_baud(U32 baud)
{
	USART_InitTypeDef config;

	config.USART_BaudRate = baud;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	USART_Init(LGB_PORT, &config);

	USART_ITConfig(LGB_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(LGB_PORT, ENABLE);
}

/*
 * init uart module.
 */
void vLGB_UART_Init(void)
{
	USART_InitTypeDef config;
	NVIC_InitTypeDef NVIC_InitStructure;

	vLGB_GPIOConfig();

	config.USART_BaudRate = 9600;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	/* Configure the NVIC Preemption Priority Bits */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

	/* Enable the USARTy Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = LGB_IRQ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_EnableIRQ(LGB_IRQ);
	USART_Init(LGB_PORT, &config);

	USART_ITConfig(LGB_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(LGB_PORT, ENABLE);
}

/*
 * read a char from buffer
 */
BOOL LGB_GetBuffer(U8 *ch)
{
	/* Buffer is empty */
	if(lgb_r_indx == lgb_w_indx)
		return 0;

	*ch = LgbBuffer[lgb_r_indx++];
	if(lgb_r_indx >= LGB_BUFFER_MAX_SIZE)
		lgb_r_indx = 0;

	return 1;
}

/*
 * write a char to buffer, call in interrupt routine.
 */
void LGB_WriteBuffer(S8 ch)
{
	LgbBuffer[lgb_w_indx++] = ch;
	if(lgb_w_indx >= LGB_BUFFER_MAX_SIZE)
		lgb_w_indx = 0;
}

/*
 * reset lgb buffer
 */
 void LGB_ResetBuffer(void)
{
	lgb_w_indx = 0;
	lgb_r_indx = 0;
}

/*
 * Interrupt routine
 */
void LGB_IRQHandler(void)
{
	U8 c;

	if(USART_GetITStatus(LGB_PORT, USART_IT_RXNE) != RESET)
	{
		c = USART_ReceiveData(LGB_PORT);
		/*Put data to buffer*/
		LGB_WriteBuffer(c);
		
		/*Enable debug, Forward data to USART1*/
		if(TmDebugEnableFlag == __TRUE)
			USART_SendData(USART1, (uint8_t)c);
	}
}


/***********************************************************************/
/*
 * parse gps msg
 */
void LGB_GpsParser(U8 *pMsg,U8 len)
{
	pGpsContent = pMsg;			
}

/*
 * parse lgt msg
 */
void LGB_LgtParser(U8 *pMsg)
{
	pLgtContent = pMsg;
}

/*
 * decode lgb msg
 */
void LGB_Decode(U8 *ch)
{
	*ch ^= 0x29^(gpsInfo.dt.day/8)^gpsInfo.dt.month;
	//*ch ^= 0x29^0x03^0x0b;
}

/*
 * get lgt msg and pre-parse msg
 */
BOOL  LGB_Parser(U8 ch)
{
	U16 i;
	U8  byteTemp;
	static U8  CarStatusOld = 0, CarStatusNew = 0;

	switch(LgbState)
	{
		case LGB_WAIT_G:			
			if(ch == 'G')
			{
				LgbByteIdx ++;
				*pLgbData++ = ch;
				LgbState = LGB_WAIT_P;
			}
			else
			{
				pLgbData = LgbMessage;
			}
			break;

		case LGB_WAIT_P:

			*pLgbData++ = ch;
			if(ch == 'P')
			{
				LgbByteIdx ++;
				LgbState = LGB_WAIT_S;
			}
			else
			{
				LgbByteIdx = 0;
				LgbState = LGB_WAIT_G;
				pLgbData = LgbMessage;
			}
			break;

		case LGB_WAIT_S:

			*pLgbData++ = ch;
			if(ch == 'S')
			{
				LgbByteIdx ++;
				LgbState = LGB_GET_CAR_STATUS;
			}
			else
			{
				LgbByteIdx = 0;
				LgbState = LGB_WAIT_G;
				pLgbData = LgbMessage;
			}
			break;
			
		case LGB_GET_CAR_STATUS:
			
			/* gan du lieu */
			*pLgbData++ = ch;
			LgbByteIdx ++;

			/* Check car status*/
			if(LgbByteIdx == 9)
			{
				// Bit 3, Bit 2, Bit 1, Bit 0: 
				// 0x00: Dong ho dang tat cuoc
				// 0x01: Dong ho dang co khach
				// 0x02: Dong ho dang co khach nhung khong tinh thoi gian cho
				
				/* validate */
				CarStatusNew = ch & 0x0F;
				if(CarStatusNew > 0x02)
				{
					LgbState = LGB_WAIT_G;
					pLgbData = LgbMessage;
					break;
				}									
					
				/* Check car status */
				if(CarStatusNew == 0)
				{	
					//Dong ho dang tat cuoc, 12 gps + 43 lgt
					LgbMsgLen = LGB_MSG_MIN_LEN; 
					//LGB_DebugPrCStr("\n\rLen:55");
					if(CarStatusOld != 0)
					{
						//Chuyen tu co khach sang ko co khach
						CarStatusEvt = TM_STOP_TRIP_EVT;
						LGB_DebugPrCStr("\n\rTM_STOP_TRIP_EVT");	
					}
					else
					{
						CarStatusEvt = TM_NONE_EVT;	
					}
				}
				else
				{
					//Dong ho dang co khach, 24 gps + 43 lgt
					LgbMsgLen = LGB_MSG_MAX_LEN;
					//LGB_DebugPrCStr("\n\rLen:67");
					if(CarStatusOld == 0)
					{
						//Chuyen tu ko co khach sang co khach
						CarStatusEvt = TM_START_TRIP_EVT;
						
						LGB_DebugPrCStr("\n\rTM_START_TRIP_EVT");		
					}
					else
					{
						CarStatusEvt = TM_NONE_EVT;	
					}
				}
				/*update car status*/
				CarStatusOld = CarStatusNew;
				/*next state*/
				LgbState = LGB_VALID_GPS_CHECKSUM;				
			}			
			break;

		case LGB_VALID_GPS_CHECKSUM:
			/* Validate gps msg checksum */
			*pLgbData++ = ch;
			LgbByteIdx ++;
			
			if(LgbByteIdx == (LgbMsgLen - LGB_LGT_MSG_LEN))
			{
				//Validate checksum	GPS
				byteTemp = 0;
				for(i = 0; i<(LgbMsgLen - LGB_LGT_MSG_LEN - 1); i++)
				{
					byteTemp += LgbMessage[i];
				}
				
				byteTemp ^= LgbMessage[LgbMsgLen - LGB_LGT_MSG_LEN - 1];

				#if 1	//ignore checksum
					if(byteTemp == 0)
					{
						LgbState = LGB_GET_LGT_MSG;
						LGB_DebugPrCStr("\n\rGPS checksum OK");
					}
					else
					{
						LgbByteIdx = 0;
						LgbState = LGB_WAIT_G;
						pLgbData = LgbMessage;
						LGB_DebugPrCStr("\n\rGPS checksum fail");
					}
				#else
					LgbState = LGB_GET_LGT_MSG;
				#endif
			}

			break;
			
		case LGB_GET_LGT_MSG:
			/* Get remain data */
			if(LgbByteIdx < LgbMsgLen)
			{
				*pLgbData++ = ch;
				LgbByteIdx ++;
			}
			else
			{
				LgbState = LGB_END_MSG;
			}
			break;
			
		case LGB_END_MSG:			
//			#if LGB_DEBUG_EN
//				LGB_DebugPrCStr("\n\n\r");
//				USART_PrStr(USART1,LgbMessage,LgbMsgLen - LGB_LGT_MSG_LEN);	
//	
//				LGB_DebugPrCStr("\n\r");
//				USART_PrStr(USART1,&LgbMessage[LgbMsgLen - LGB_LGT_MSG_LEN],LGB_LGT_MSG_LEN);
//			#endif

			//Validate checksum lgt
			byteTemp = 0;
			for(i = LgbMsgLen - LGB_LGT_MSG_LEN; i<LgbMsgLen -1 ; i++)
			{
				byteTemp += LgbMessage[i];
			}
			byteTemp ^= LgbMessage[LgbMsgLen - 1];

			if(byteTemp == 0)
			{
			 	LGB_DebugPrCStr("\n\rLGT checksum OK");
			}
			else
			{
			 	LGB_DebugPrCStr("\n\rLGT checksum fail");
			}

			/* Parser lgb msg */
			LGB_GpsParser(&LgbMessage[0],LgbMsgLen - LGB_LGT_MSG_LEN);
			LGB_LgtParser(&LgbMessage[LgbMsgLen - LGB_LGT_MSG_LEN]);

			/* Get next lgb msg */
			LgbState = LGB_WAIT_G;
			LgbByteIdx = 0;
			pLgbData = LgbMessage;
			
			return __TRUE;
			
		default:
			LgbByteIdx = 0;
			LgbState = LGB_WAIT_G;
			break;
	}
	return __FALSE;
}

/***********************************************************************/
/* Tm Ram BackUp Register*/
#define TM_LIST_READ_IDX		BKP_DR38
#define TM_LIST_READ_IDX_H		BKP_DR39
#define TM_LIST_WRITE_IDX		BKP_DR40
#define TM_LIST_WRITE_IDX_H		BKP_DR41


/* Tm buffer */
#define TM_PKT_BUFFER_SIZE			80			//size of package on sd card
//#define TM_NUMBER_PKT_ON_FILE_MAX	2000//number of package in one buffer file
//#define TM_BUF_FILE_IDX_MAX			7

#define TM_NUMBER_PKT_ON_FILE_MAX	4//number of package in one buffer file
#define TM_BUF_FILE_IDX_MAX			3

typedef union _Tm_locate_t
{
	struct _field
	{
		U32 fileIdx:12;
		U32 location:12;
	}field;

	U32 all;
}Tm_locate_t;

Tm_locate_t TmWriteIdx, TmReadIdx;

S8 TmBufPath[25] = "Taximeter\\TmBuf0.dat";
FILE *pTmBufFile;
#define TM_BUF_WRITE_IDX 	TmBufPath[15]

S8 TmBufReadPath[25] = "Taximeter\\TmBuf0.dat";
FILE *pTmReadBufFile;
#define TM_BUF_READ_IDX 	TmBufReadPath[15]

/* 
*init a hole for save tm data 
*/
void LGB_SDBufferInit(void)
{
	U32 temp;
	/* Get current read idx location */
	temp = BKP_ReadBackupRegister(TM_LIST_READ_IDX_H);
	temp = (temp<<16) + (BKP_ReadBackupRegister(TM_LIST_READ_IDX));
	TmReadIdx.all = temp;
	TM_BUF_READ_IDX = TmReadIdx.field.fileIdx + '0';

	// Get current write idx location */
	temp = BKP_ReadBackupRegister(TM_LIST_WRITE_IDX_H);
	temp = (temp<<16) + (BKP_ReadBackupRegister(TM_LIST_WRITE_IDX));
	TmWriteIdx.all = temp;
	TM_BUF_WRITE_IDX = TmWriteIdx.field.fileIdx + '0';
}

/*
 * reset all index tm in buffer.
 */
void LGB_SDBufferReset(void)
{
	TmWriteIdx.all = 0;
	TM_BUF_WRITE_IDX = TmWriteIdx.field.fileIdx + '0';
	BKP_WriteBackupRegister(TM_LIST_WRITE_IDX, (TmWriteIdx.all)&0xFFFF);
	BKP_WriteBackupRegister(TM_LIST_WRITE_IDX_H, (TmWriteIdx.all>>16));

	// Read write location */
	TmReadIdx.all = 0;
	TM_BUF_READ_IDX = TmReadIdx.field.fileIdx + '0';
	BKP_WriteBackupRegister(TM_LIST_READ_IDX, (TmReadIdx.all&0xFFFF));
	BKP_WriteBackupRegister(TM_LIST_READ_IDX_H, (TmReadIdx.all>>16));
}

/*
 * write fix TM_PKT_BUFFER_SIZE data to sdcard.
 */
S8 LGB_SDBufferWrite(S8* buf)
{
	U16 write_cnt;
	if(TmWriteIdx.field.location == 0)
		pTmBufFile = fopen(TmBufPath,"w");
	else
		pTmBufFile = fopen(TmBufPath,"a");

	if(pTmBufFile == NULL)
		return NO_FILE;

   	/*for debug*/
	//USART_PrCStr(DBG_Port,"\n\rSD:");
   	//USART_PrStr(DBG_Port,buf,TM_PKT_BUFFER_SIZE);


	write_cnt = fwrite(buf, sizeof(S8), TM_PKT_BUFFER_SIZE, pTmBufFile);
	if(write_cnt != TM_PKT_BUFFER_SIZE){
		// Close open logfile
		fclose(pTmBufFile);
		return SIZE_PACKET_ERR;
	};

	// Update write location
	if(++TmWriteIdx.field.location >= TM_NUMBER_PKT_ON_FILE_MAX){
		TmWriteIdx.field.location = 0;
		if(++TmWriteIdx.field.fileIdx >= TM_BUF_FILE_IDX_MAX)
			TmWriteIdx.field.fileIdx = 0;

		// Reset read location if overwrite same bank
		if(TmReadIdx.field.fileIdx == TmWriteIdx.field.fileIdx){
			TmReadIdx.field.location = 0;

			if(++TmReadIdx.field.fileIdx >= TM_BUF_FILE_IDX_MAX)
				TmReadIdx.field.fileIdx = 0;
			
			//Update ram idx
			BKP_WriteBackupRegister(TM_LIST_READ_IDX, (TmReadIdx.all&0xFFFF));
			BKP_WriteBackupRegister(TM_LIST_READ_IDX_H, (TmReadIdx.all>>16));

			// Update read file name
			TM_BUF_READ_IDX = TmReadIdx.field.fileIdx + '0';
		};

		// Update write file name
		TM_BUF_WRITE_IDX = TmWriteIdx.field.fileIdx + '0';
	};
	BKP_WriteBackupRegister(TM_LIST_WRITE_IDX, (TmWriteIdx.all)&0xFFFF);
	BKP_WriteBackupRegister(TM_LIST_WRITE_IDX_H, (TmWriteIdx.all>>16));

	// Close open log file
	fclose(pTmBufFile);
	return TM_RES_OK;
}

/***********************************************************************/
/*
 * read packet
 */
S16 LGB_ReadPacket(U8 *pData)
{
	U16 read_cnt;
	U16 checksum, size, buf_checksum;

	/*get data from sd*/
	if(TmReadIdx.all == TmWriteIdx.all) 
		return NO_FILE;

	//wait mutex
	os_mut_wait(TmMutex, 0xFFFF);

	/*Open buffer*/
	pTmReadBufFile = fopen(TmBufReadPath, "r");
	if(pTmReadBufFile == NULL)
	{	
		os_mut_release(TmMutex);
		return FILE_EMPTY;
	}

	if(fseek(pTmReadBufFile, TmReadIdx.field.location*TM_PKT_BUFFER_SIZE, SEEK_CUR) == EOF){
		// Close the file
		fclose(pTmReadBufFile);
		os_mut_release(TmMutex);
		return SIZE_PACKET_ERR;
	}

	read_cnt = fread(pData, sizeof(uint8_t),TM_PKT_BUFFER_SIZE,pTmReadBufFile);
	fclose(pTmReadBufFile);
	os_mut_release(TmMutex);

	if((read_cnt != TM_PKT_BUFFER_SIZE))
	{
		return SIZE_PACKET_ERR;
	}

	/*validate checksum, loi the, sai checksum thi next package*/
	if((pData[10]&0x0F) == 0)
	{
		size = 57;	
	}
	else
	{
		size = 71;	
	}

	// tinh checksum tu data den truoc truong checksum trong buffer.
	checksum = cCheckSumFixSize(pData,size - 2);

	//checksum in buffer.
	memcpy(&buf_checksum, pData + size - 2, 2);

	if((checksum != buf_checksum))
	{
		return CHECKSUM_FAIL;
	}

	/*return result*/
	return size;
}

void LGB_NextPacket()
{
	/* if read/write in the same bank, move to near write location*/
	if(TmReadIdx.field.fileIdx == TmWriteIdx.field.fileIdx){
		if(TmReadIdx.field.location < TmWriteIdx.field.location)
			TmReadIdx.field.location++;
		else
			return;
	}

	/* if read/write at diffirent bank */
	else if(++TmReadIdx.field.location >= TM_NUMBER_PKT_ON_FILE_MAX){
		TmReadIdx.field.location = 0;
		if(++TmReadIdx.field.fileIdx >= TM_BUF_FILE_IDX_MAX){
			TmReadIdx.field.fileIdx = 0;
		}

		// Update filename
		TM_BUF_READ_IDX = TmReadIdx.field.fileIdx + '0';
	}
	
	/* write backup */
	BKP_WriteBackupRegister(TM_LIST_READ_IDX, (TmReadIdx.all&0xFFFF));
	BKP_WriteBackupRegister(TM_LIST_READ_IDX_H, (TmReadIdx.all>>16));	
}

/*
* Next package if receive ack	
*/
void TM_NextPacket()
{
	switch(Config.TM.tm_type)
	{
		case 0:
		case 1:
		//to do for other taxi meter
		default:
			LGB_NextPacket();
			break;	
	}
}

/*
*	Request save immediately tm msg to sd
*/
void TM_ImediatelyUpdate(void)
{
	TmGetNowFlag = __TRUE;
}

/*
*	Get read buffer index
*/
U32 TM_GetReadBufferIndex(void)
{
	return TmReadIdx.all;
}

/*
*	Get write buffer index
*/
U32 TM_GetWriteBufferIndex(void)
{
	return TmWriteIdx.all; 
}

/*
*	Save data to sd card
*/
BOOL LGB_UpdateLog(U8 *pData)
{
	Tm_Error_e res = TM_RES_OK;
	
	os_mut_wait(TmMutex, 0xFFFF);
	//res = LGB_SDBufferWrite(TmMsgBuff); 
	res = LGB_SDBufferWrite(pData); 
	
	if(res != TM_RES_OK)
	{
		LGB_DebugPrCStr("\n\rres=");
		LGB_DebugPrCStr(&res);
		os_mut_release(TmMutex);
		return 1;
	}
	LGB_DebugPrCStr("\n\rRes ok");

	os_mut_release(TmMutex);
	return 0;			
}

/***********************************************************************/
/*
 * init task process TM
 */
void vTaxiMeterInit(void)
{
	/*init sd buffer*/
	LGB_SDBufferInit();
	
	/*init task*/
	if(LgbTskID == NULL)
		LgbTskID = os_tsk_create_user(vLgb_Task, RTX_LGB_PRIORITY, tskLGBStack, sizeof(tskLGBStack));
}

/*
* TM task produce
*/
__task void vLgb_Task(void)
{
	U8 c,i;
	uGPSDateTime_t dt;
	static U8	tmPacketId = 0;
	static U8	tmOffset = 0;
	U16 tmPacketChecksum = 0;
	TM_State_e TmState = TM_GET_DATA;

  U32 tmTimeDisconnect = 0;
	U32 tmTimeDisconnectMax = TM_DISCONNECT_TIMEOUT_MAX;
	U8 	tmTimeDisconnectNum = 0;
	U32 tmTimeout= 0;
	U8 *pLgbSavePacket;
	U8 TmMsgBuff[TM_PKT_BUFFER_SIZE+2];
	float x,y;

	
	#ifdef LGB_SERVER_TEST
		/*approximate 30 sec for putting data to sd card*/
		tmTimeout = Config.TM.tm_timeout*20*1000/TM_TMO_TICK;		
	#else
		/*get timeout config- convert from minutest to x10ms*/
		tmTimeout = Config.TM.tm_timeout*60*1000/TM_TMO_TICK;
	#endif

	/*force car status*/
	CarStatusEvt = TM_NONE_EVT;
	
	/*loop forever*/
	for(;;)
	{
		switch(TmState)
		{
			case TM_GET_DATA:
				/*clear watchdog tick*/
				wdt_lgb_cnt = 0;

				/*get raw data from uart buffer*/
				if(LGB_GetBuffer(&c)==1)
				{
					//if(IsGpsValid)
					{
						/*clear disconnect flag*/
						if(tmTimeDisconnect != 0)
							tmTimeDisconnect = 0;

						if(tmTimeDisconnectMax == TM_DISCONNECT_TIMEOUT_MAX2)
						{
							tmTimeDisconnectMax = TM_DISCONNECT_TIMEOUT_MAX;
						}

						/*decode receive charater*/
						LGB_Decode(&c);
						
						/*get full msg and parser*/
						if(LGB_Parser(c) == __TRUE)
						{
							TmState = TM_WAIT_EVENT;
							break;	
						}
					}	
				}
				else
				{
					//if(++tmTimeDisconnect > TM_DISCONNECT_TIMEOUT_MAX)  
					if(++tmTimeDisconnect > tmTimeDisconnectMax)
					{
						LGB_DebugPrCStr("\n\rTM is disconnected");
						blSendError(TM_DISCONNECT_TIMEOUT,5);	
						tmTimeDisconnect = 0;

						/*disconnect in 30 mins, change timeout max*/
						if(tmTimeDisconnectNum ++ > 30)
						{
							tmTimeDisconnectMax = TM_DISCONNECT_TIMEOUT_MAX2;
						}	
					}

					os_dly_wait(TM_TMO_TICK);
					/*ko co du lieu tu dong ho, gui ma loi sau thoi gian xac dinh*/

					if(tmTimeout)
						tmTimeout --;
				}	
				break;

			case TM_WAIT_EVENT:
				/*request packing and update log immediately*/
				if(TmGetNowFlag == __TRUE) 
				{
					TmGetNowFlag = __FALSE;
					TmState = TM_SAVE_DATA;
					break;									
				}

				switch(CarStatusEvt)
				{
					case TM_NONE_EVT:
						/*update log depend on timeout*/
						if(tmTimeout == 0)
						{	
							//res = LGB_UpdateLog();
							LGB_DebugPrCStr("\n\rTm none evt");
							TmState = TM_SAVE_DATA;
							#ifdef LGB_SERVER_TEST
								tmTimeout = Config.TM.tm_timeout*20*1000/TM_TMO_TICK;
							#else
								tmTimeout = Config.TM.tm_timeout*60*1000/TM_TMO_TICK;
							#endif	
						}
						else
						{
							os_dly_wait(TM_TMO_TICK);
							tmTimeout --;
							TmState = TM_GET_DATA;								
						}
						break;

					case TM_START_TRIP_EVT:						
					case TM_STOP_TRIP_EVT:
						/*packing and update log when trip start/stop*/
						//LGB_UpdateLog();
						//LGB_DebugPrCStr("\n\rTaxi in/out service");
						TmState = TM_SAVE_DATA;
						break;

					default:
						TmState = TM_GET_DATA;
						break;  		
				}
				break;

			case TM_SAVE_DATA:	
			
				/*for test*/
//				TmState = TM_GET_DATA;
//					break;
				
				//LGB_DebugPrCStr("\n\rTM save data");
				
				/*point to temporally buffer for packing*/
				memset(TmMsgBuff,0x00,TM_PKT_BUFFER_SIZE);
				pLgbSavePacket = &TmMsgBuff[0];
				tmOffset = 0;

				/*add box id*/
				memcpy(pLgbSavePacket + tmOffset,Config.BoxID,5);
				tmOffset += 5;
				/*add date time*/
				vUpdateDateTimeToStr(&dt);
				memcpy(pLgbSavePacket + tmOffset,&dt,sizeof(uGPSDateTime_t));
				tmOffset += sizeof(uGPSDateTime_t);
				/*add packet id*/
				memcpy(pLgbSavePacket + tmOffset,&tmPacketId,1);
				tmOffset ++;
				
				/*add gps location info*/
				GPS_get_xy(&x,&y);
		
				memcpy(pLgbSavePacket + tmOffset,&x,4);
				tmOffset += 4;
				memcpy(pLgbSavePacket + tmOffset,&y,4);
				tmOffset += 4;
				
				/*add gps data*/
				/*remove 3 byte "gps" + 1 byte checksum of gps frame*/
				if(LgbMsgLen == LGB_MSG_MIN_LEN)//if(CarStatusEvt == TM_NONE_EVTì)
				{					
					memcpy(pLgbSavePacket + tmOffset,&LgbMessage[3],6);	
					tmOffset += 6;	
				}
				else
				{
					memcpy(pLgbSavePacket + tmOffset,&LgbMessage[3],20);
					tmOffset += 20;			
				}
					
				/*add lgt data/*
				/*remove 3 byte "lgt" + 1 byte checksum of lgt frame*/
				memcpy(pLgbSavePacket + tmOffset,&LgbMessage[LgbMsgLen - LGB_LGT_MSG_LEN - 1 + LGB_FRAME_ID_LEN],LGB_LGT_MSG_LEN-4);
				tmOffset += LGB_LGT_MSG_LEN-4;
				
				#ifdef LGB_USE_FAKE_DATA
					for(i = 0;i<= tmOffset; i++)
						//TmMsgBuff[i] = 'A';
						TmMsgBuff[i] = i;
				#endif

				/*add checksum*/
				tmPacketChecksum = (U16)cCheckSumFixSize(pLgbSavePacket,tmOffset) & 0x00FF;
				memcpy(pLgbSavePacket + tmOffset,&tmPacketChecksum,2);	
				
				/*update log*/		   	
				if(LGB_UpdateLog(TmMsgBuff) == 0)
				{ 		
					if(tmPacketId == 0xFF)
					{
						tmPacketId = 0;		
					}
					else
						tmPacketId++;

					LGB_DebugPrCStr("\n\rTM save ok");
				}
				else
				{
					LGB_DebugPrCStr("\n\rTM save fail");	
				}

				/*Next state*/
			   	TmState = TM_GET_DATA;
				break;
						
			default:
				TmState = TM_GET_DATA;
				break;			
		}
	}
}

/*
 * process watchdog
 */
uint8_t WDT_Check_LGB_Service(void)
{
	if (LgbTskID != NULL)
		wdt_lgb_cnt++;
	else
		wdt_lgb_cnt = 0;

	if(wdt_lgb_cnt >= WDT_LGB_MAX_TIMING)
		return 1;

	return 0;
}

