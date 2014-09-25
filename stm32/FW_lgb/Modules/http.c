/*
 * http.c
 *
 *  Created on: Feb 12, 2012
 *      Author: Van Ha
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "rtl.h"
#include "stm32f10x.h"
#include "platform_config.h"

#include "utils.h"
#include "debug.h"
#include "sim900.h"
#include "gsm.h"
#include "gprs.h"
#include "fwupgrade.h"

#include "http.h"
#include "gprs.h"
#include "loadConfig.h"
#include "watchdog.h"

#define IsSpecialChar(chr)		if(((chr) == '#') || ((chr) == 27) || ((chr) == '$'))

#define SD_BUFFER_SIZE			1024				// 1024
#define WAIT_REQUEST_TIMEOUT	240					// 240

#define RETRY_SET_FWPort		10
#define	RETRY_REQUEST_PART		10
#define	RETRY_WAIT_PART			10

/* HTTP Event */
#define FW_REQUEST_FILE_EVT		0x0001
#define FW_REQUEST_CHUNK_EVT	0X0002

// Convert utility
#define HexChar2Dec(chr)		(((chr) > '9')? ((chr) - '7'):((chr) - '0'))
#define HexPairs2Dec(a,b)		HexChar2Dec(a)<<4 | HexChar2Dec(b)

const S8 *res_ok = "\r\nOK\r\n";

volatile FWState_t FWState = FW_INIT_GSM_MODULE;

/* Private variable */
FILE *f_dw;
S8* FWPort;
S8* FWCRC32;
S8* FWBuf;
S8* FWInfo;
U32 file_CRC32;
uint8_t fw_pkg_id= 0;
uint8_t fw_invalid_checksum_chunk_cnt = 0;

/* Private structure, types ------------------------------- */
/* Type */
typedef enum{
	RECORD_DATA=0x00,					// Data record
	RECORD_EOF,							// End of file record
	RECORD_EXTEND_SEGMENT_ADDR,			// Extended segment address record
	RECORD_EXTEND_LINEAR_ADDR=0x04,		// Extended linear address record
	RECORD_START_LINEAR_ADDR			// Start linear address record
} IntelRecord_t;


typedef struct {
	uint8_t 		ByteCount;
	uint8_t 		Address[2];
	IntelRecord_t 	RecordType;
	uint8_t 		Data[16];
	uint8_t 		CheckSum;
} DecodedIntelHexLine_t;
DecodedIntelHexLine_t DecodedLine;					// Decoded line


/* Firmware information */
uint16_t FWChunkIdx = 0;
uint16_t FWPkgCount = 0;

static U64 httpStack[800/8];

OS_TID FWTsk_ID;

extern OS_TID xClientTransID;
//extern S8 txData[MAX_SIZE_PACKET + 400];

__task void vDwlFirmware(void);

#define HTTP_DEBUG
#ifdef HTTP_DEBUG
void FWDebug(S8* s)
{
	if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
		USART_PrCStr(DBG_Port, s);
}
#else
void FWDebug(S8* s)
{}
#endif


/* Private functions --------------------------------------- */
int8_t FW_ConnectServer(uint8_t* IP, uint8_t* port);
int8_t FW_SendPackage(uint8_t id, uint8_t* data, uint16_t len);

/*
 * update is available
 */
BOOL FWUpgradeIsRunning(void)
{
	return (BOOL)(FWTsk_ID!=NULL);
}

void vStartDownload(void)
{
	if(FWTsk_ID != NULL)
		os_evt_set(RTX_DWL_START_FLAG, FWTsk_ID);
}

/*
 * call back in gprs function
 * DOTA,Port,Model,Checksum,CS		// CS = cs of  Port,Model,Checksum,
 */
void FWUpgradeDemandHandler(S8* info)
{
	S8* mItem[6];
	U16 n;
	S8 ch[4];
	U8 data_checksum = 0, FWResCS;

	FWDebug("\r\nFIMRWARE File hander\r\n");
	// if downloading...
	if(FWUpgradeIsRunning())
		return;

	/* Split string: Port,Model,Checksum => mItem[0], mItem[1], mItem[2] */
	n = string_split((U8**)mItem, (U8*)info, ",", 5);
	if(n != 4){
		FWDebug("FW: Wrong demand\r\n");
		return;
	};

	/* Get checksum at third parameter */
	FWResCS = twochar2hex(*(mItem[3]), *(mItem[3] + 1));
	if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
		USART_PrChar(USART1,FWResCS);

	// validate checksum data
	data_checksum = checksum(mItem[0]) ^ checksum(mItem[1]) ^ checksum(mItem[2]);

	if(data_checksum != FWResCS)
	{
		if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
			USART_PrChar(USART1,data_checksum);
		return;
	}

	/* Request buffer for parameter */
	FWPort = (S8*)calloc(6, sizeof(S8));
	FWInfo = (S8*)calloc(20, sizeof(S8));
	FWCRC32 = (S8*)calloc(5, sizeof(S8));
	FWBuf = (S8*)calloc(SD_BUFFER_SIZE+3, sizeof(S8));

	if((FWBuf == NULL) || (FWCRC32 == NULL) || (FWPort == NULL) || (FWInfo == NULL))
		return;

	FWDebug("\r\nCreate Task download firmware\r\n");
	/* Get checksum data */
	strcpy((char*)FWPort, (const char*) mItem[0]);
	strcpy((char*)FWInfo, (const char*) mItem[1]);	
	strcpy((char*)FWCRC32, (const char*)mItem[2]);
	
	ch[3] = twochar2hex(*mItem[2], *(mItem[2]+1));
	ch[2] = twochar2hex(*(mItem[2]+2), *(mItem[2]+3));
	ch[1] = twochar2hex(*(mItem[2]+4), *(mItem[2]+5));
	ch[0] = twochar2hex(*(mItem[2]+6), *(mItem[2]+7));
	memcpy((S8*)&file_CRC32, ch, sizeof(U32));
	
	/* Start download */
	FWTsk_ID = os_tsk_create_user(vDwlFirmware, RTX_HTTP_TSK_PRIORITY, &httpStack, sizeof(httpStack));
}
/**
 * @brief	HTTP request Chunk data
 */

/* RTX Task ------------------------------------------ */
#define FW_CHUNKSIZE		20	// 20 line

__task void vDwlFirmware(void)
{
OS_RESULT result;
uint16_t gprsRetry = 3;
uint8_t isFWFirstSession = 1;
uint8_t FWCmd[44];
S8* uItem[FW_CHUNKSIZE];
uint16_t ItemCount;
uint16_t idx;
uint8_t FW_Line_CS;
uint8_t* p_decoded;
uint8_t* p;
uint8_t len;
uint8_t idx_cs;
FWUpgrade_Error_t res;

	FWState = FW_INIT_GSM_MODULE;
	fw_pkg_id = 0;	

	FWDebug("\r\nWait event download firmware\r\n");

	/* Wait 1min of task sending data */
	if(os_evt_wait_and(RTX_DWL_START_FLAG, 65000) != OS_R_EVT)
	{  		
		// release memory
		free(FWPort);
		free(FWCRC32);
		free(FWBuf);

		// delete task
		os_evt_set(RTX_DWL_FINISH_FLAG, xClientTransID);

		FWTsk_ID = NULL;
		os_tsk_delete_self();
		return;	 
	}

	// debug
	FWDebug("\r\nSTART DOWNLOAD FIMRWARE\r\n");


/* Init Firmware procedure */
	if(vFWInit(128000) != 0)
	{
		// release memory
		free(FWPort);
		free(FWCRC32);
		free(FWBuf);

		/* Send event to transport task */
		os_evt_set(RTX_DWL_FINISH_FLAG, xClientTransID);

		// delete task
		FWTsk_ID = NULL;
		os_tsk_delete_self();
		return;
	}

	/* Reset firmware wrong checksum */
	fw_invalid_checksum_chunk_cnt = 0;
	FWState = FW_INIT_GSM_MODULE;

/* Super loop: all doing here */
	for(;;){
		/* Break rule */
		if( (FWState == FW_FAIL) || (FWState == FW_SUCCESS))
			break;

		/* state machine */
		switch(FWState){
		case FW_INIT_GSM_MODULE:
			/* Open SIM900 module and connect to network */
			if(blGSM_Init()){
				/* Connect to GPRS */
				while(gprsRetry--){
					/* set apn and wait gpsr ready */
					if(blGPRS_Open((S8*)Config.apn_name, (S8*)Config.apn_user, (S8*)Config.apn_pass, 200)!=0)
					{
						FWState = FW_CONNECT_SERVER;

						/* reset gprsRetry variable */
						gprsRetry = 3;
						FWDebug("\r\nGPRS STATE 'NETWORK OK'\r\n");						
						break;
					}
					/* status pin and sim ready? */
					if(!SIM900_Status() || (blSIMCARD_Status()!=2))
					{
						break;
					}
				}

				/* Increase retry time */
				gprsRetry *= 2;
				if(gprsRetry >= 48)
					gprsRetry = 3;
			}
			/* Else wait and retry again: 10 sec */
			else {
				os_dly_wait(10000/OS_TICK_RATE_MS);
			}

			break;
		case FW_CONNECT_SERVER:
			/* Check status pin and sim ready pin */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2)){
				FWState = FW_INIT_GSM_MODULE;
				gprsRetry = 3;
				break;
			}

			/* Connect to IP and provided port */
			if(FW_ConnectServer(Config.IP1, FWPort) == 0){
			//if(FW_ConnectServer("210.211.97.103", FWPort) == 0){
				FWDebug("E: Can't connect to server\r\n");
				FWState = FW_FAIL;
				gprsRetry = 3;
			}
			else if(isFWFirstSession == 1){
				FWDebug("FW Server connected! \r\n");
				FWState = FW_REQUEST_FILE;
				FWChunkIdx = 0;
				isFWFirstSession = 0;
			}
			else
				FWState = FW_REQUEST_CHUNK;

			gprsRetry = 0;

			break;
		case FW_REQUEST_FILE:
			/* Check status pin and sim ready pin */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2)){
				FWState = FW_INIT_GSM_MODULE;
				gprsRetry = 3;
				break;
			}

			/* Check status gprs */
			if((uiTCPIP_Status()==0))
			{
				// shutdown current connect.
				vTCPIP_Shutdown();
				// reconnect.
				FWState = FW_CONNECT_SERVER;
				break;
			}

			/* Request [FWInfo],[So line]? , and move to the next state */
			sprintf(FWCmd,"%s,%.2X?",FWInfo,FW_CHUNKSIZE);
			if(FW_SendPackage(fw_pkg_id, FWCmd, strlen(FWCmd)) == 0){
				os_dly_wait(500/OS_TICK_RATE_MS);
				break;
			}

			FWDebug("State: Moved to request file\r\n");
			FWState = FW_WAIT_REQUEST_FILE;

			break;

		case FW_WAIT_REQUEST_FILE:
			/* Check status pin and sim ready pin */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2)){
				FWState = FW_INIT_GSM_MODULE;
				gprsRetry = 3;
				break;
			}

			/* Check status gprs */
			if(uiTCPIP_Status()==0)
			{
				// shutdown current connect.
				vTCPIP_Shutdown();
				// reconnect.
				FWState = FW_CONNECT_SERVER;
				break;
			}

			/* Wait for event */
			if(os_evt_wait_and(FW_REQUEST_FILE_EVT, 4000/OS_TICK_RATE_MS) == OS_R_TMO){
				gprsRetry++;
				if(gprsRetry == 10){
					FWState = FW_FAIL;
					//__breakpoint(0);
					break;
				}

				/* Go to request file again */
				FWState = FW_REQUEST_FILE;
			}
			else {
				/* Validate data: File not found */
				if (FWPkgCount == 0xFFFF){
					FWState = FW_FAIL;
					break;
				}

				fw_pkg_id++;
				FWDebug("File requested \r\n");
				FWState = FW_REQUEST_CHUNK;
				gprsRetry = 0;
				break;
			}
			break;

		case FW_REQUEST_CHUNK:
			/* Check status pin and sim ready pin */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2)){
				FWState = FW_INIT_GSM_MODULE;
				gprsRetry = 3;
				break;
			}

			/* Check status gprs */
			if(uiTCPIP_Status()==0)
			{
				// shutdown current connect.
				vTCPIP_Shutdown();
				// reconnect.
				FWState = FW_CONNECT_SERVER;
				break;
			}

			/* Send request with index, compare with length of index: FWChunkIdx */
			sprintf(FWCmd,"%s,%.2X,%.4X?",FWInfo,FW_CHUNKSIZE, FWChunkIdx);
			if(FW_SendPackage(fw_pkg_id, FWCmd, strlen(FWCmd)) == 0){
				os_dly_wait(500/OS_TICK_RATE_MS);
				break;
			} else
			{
				FWDebug("Chunk requested\r\n");
				FWState = FW_WAIT_CHUNK_DATA;
			}
			break;
		case FW_WAIT_CHUNK_DATA:
			/* Check status pin and sim ready pin */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2)){
				FWState = FW_INIT_GSM_MODULE;
				gprsRetry = 3;
				break;
			}

			/* Check status gprs */
			if(uiTCPIP_Status()==0)
			{
				// shutdown current connect.
				vTCPIP_Shutdown();
				// reconnect.
				FWState = FW_CONNECT_SERVER;
				break;
			}

			/* Wait for event */
			if(os_evt_wait_and(FW_REQUEST_CHUNK_EVT, 4000/OS_TICK_RATE_MS) == OS_R_TMO){
				gprsRetry++;
				if(gprsRetry == 10){
					FWState = FW_FAIL;
					//__breakpoint(0);
					break;
				}
				else
					FWState = FW_REQUEST_CHUNK;
			}
			else{

				/* Move to process data */
				fw_pkg_id++;
				FWState = FW_PROCESS_CHUNK_DATA;
			}

			break;

		case FW_PROCESS_CHUNK_DATA:
			/* Reset watchdog check*/
			wdt_gsm_download_fw_timeout = 0;

			/* Process data in FWBuf with ':' */

			/* Validate fail cases */
			if (memcmp(FWBuf, "FFFF", 4) == 0){
				FWState = FW_FAIL;
				//__breakpoint(0);
				break;
			}

			/* Validate each line of data */
			ItemCount = string_split((U8**)uItem, (U8*)FWBuf, ":", FW_CHUNKSIZE);
			if(ItemCount > FW_CHUNKSIZE){
				FWDebug("E: FW Wrong FW_CHUNKSIZE\r\n");

				/* Go to request chunk again */
				FWState = FW_REQUEST_CHUNK;
				gprsRetry = 0;
				break;
			}

			/* Validate each line */
			for(idx = 0; idx < ItemCount; idx++){			
				if(strlen(uItem[idx]) > 43){
					FWDebug("E: FW Wrong len\r\n");
					
					/* Go to request chunk again */
					FWState = FW_REQUEST_CHUNK;
					gprsRetry = 0;
					break;
				}
			
				/* Convert into hex */
				p = (uint8_t*) uItem[idx];
				p_decoded = (uint8_t*) &DecodedLine;

				/* Decode data */
				while(*p){
					*p_decoded++ = HexPairs2Dec(*p, *(p+1));
					p += 2;
				}

				/* Validate byte count */
				if(DecodedLine.ByteCount > 0x10){
					FWDebug("E: FW Wrong line length\r\n");

					FWState = FW_REQUEST_CHUNK;
					gprsRetry = 0;
					break;
				}

				if(DecodedLine.ByteCount < 0x10)
					DecodedLine.CheckSum = DecodedLine.Data[DecodedLine.ByteCount];

				/* Validate checksum */
				p = (uint8_t*) &DecodedLine;
				FW_Line_CS = 0;
				for(idx_cs=0; idx_cs< DecodedLine.ByteCount + 4; idx_cs++){
					FW_Line_CS += ((*p++) & 0xFF);					
				};				
				FW_Line_CS += (DecodedLine.CheckSum & 0xFF);				

				if(FW_Line_CS != 0x00){
					FWDebug("E: FW Wrong checksum\r\n");
					
					fw_invalid_checksum_chunk_cnt++;
					if (fw_invalid_checksum_chunk_cnt >= 10){
						FWState = FW_FAIL;
						break;
					}
					else
					{
					
						/* Go to request chunk again */
						FWState = FW_REQUEST_CHUNK;
						gprsRetry = 0;
						break;
					}
				};
			}

			/* Invalid checksum break */
			if ((FWState == FW_FAIL) || (FWState == FW_REQUEST_CHUNK))
				break;

			/* Reset invalid checksum count */
			fw_invalid_checksum_chunk_cnt = 0;

			/* Write each line data into buffer */
			for(idx = 0; idx < ItemCount; idx++){
				res = vFWPrChar(':');
				if(res == FW_WRITE_ERROR){
					FWState = FW_FAIL;
					break;
				}

				// Data = [2u-Len] [ 4u- address] [2xData size] [2u-Type][2u-CS]
				len = HexPairs2Dec(*(uItem[idx]), *(uItem[idx]+1));				
				res = vFWWrite(uItem[idx], len * 2 + 10);
				if(res == FW_WRITE_ERROR){
					FWState = FW_FAIL;
					break;
				}

				res = vFWPrChar(0x0D);
				if(res == FW_WRITE_ERROR){
					FWState = FW_FAIL;
					break;
				}

				res = vFWPrChar(0x0A);
				if(res == FW_WRITE_ERROR){
					FWState = FW_FAIL;
					break;
				}
			}

			/* Validate */
			if(FWState == FW_FAIL)
				break;

			/* Increase chunk idx: in case success  */
			FWDebug("+");
			FWChunkIdx++;
			if(FWChunkIdx >= FWPkgCount){
				FWState = FW_SUCCESS;
				break;
			}

			/* Request next chunk data */  
			FWState = FW_REQUEST_CHUNK ;
			gprsRetry = 0;
			break;

		case FW_FAIL:
			//__breakpoint(0);
			break;
		case FW_SUCCESS:

			break;
		}
	}

	FWDebug("Finish get data \r\n");

	// release memory
	free(FWPort);
	free(FWCRC32);
	free(FWBuf);

	/* Close FW file */
	vFWWriteFinish();

	/* Upgrade procedure */
	if(FWState != FW_FAIL)
	{
		FWDebug("\r\nNEW FIRMWARE DOWNLOAD OK\r\n");

		// @todo Add checksum data here
		if(FWVerify(file_CRC32)== FW_VERIFY_OK)
		{
			FWUpgrade();
		}
		else
			FWDebug("\r\nCRC32 FAIL\r\n");
	}

	/* Send finish event to GPRS trans task  */
	os_evt_set(RTX_DWL_FINISH_FLAG, xClientTransID);

	/* For temporary, reset STM32 into normal mode */
	vIWDG_Init();
	tsk_lock();

	FWTsk_ID = NULL;
	os_tsk_delete_self();
}


/* Connect to server */
/**
 * @return	1: success, 0: fail
 *
 */
int8_t FW_ConnectServer(uint8_t* IP, uint8_t* port){
uint8_t retry_count = 5;

	/* Wait 200ms */
	os_dly_wait(200/OS_TICK_RATE_MS);

	/* Set TCP Profiles */
	while(retry_count--){
		if(blSetTCPIPProfiles())
			break;
	}
	if(retry_count == 0) return 0;

	/* Connect to server */
	retry_count = 10;
	while(retry_count--){
		if(blTCPIP_Open(IP, port, 300))
		{
			return 1;

		}else{
			vTCPIP_Shutdown();
			// delay 500ms then reconnect.
			os_dly_wait(500/OS_TICK_RATE_MS);
		}
	}

	return 0;
}

/* Pack data and send to server
 * Format: [1u-'$'][1u-Checksum][1u-ID][2u-LenData][Vu-Data][1u-'#']
 * 			[1u-'$'][1u-ID][1u-MsgType][1u-Dummy][Vu-Data][1u-Checksum][1u-'#']
 */
int8_t FW_SendPackage(uint8_t id, uint8_t* data, uint16_t len){
uint8_t *p, *buf; // = txData;
uint8_t checksum;
uint16_t idx;
uint8_t len_l = len & 0xFF;
uint8_t len_h = len>>8;
	uint32_t res;

/* cal checksum */
	checksum = cCheckSumFixSize(data, len);
	if(len > 1000)
		len = 1000;

	p = malloc(len + len + 3);
	if(p == NULL) return 0;
	
	// assign buffer.
	buf = p;
	
/* Put data first */
	/* Start char $*/
	*p++ = '$';

	/* ID */
	IsSpecialChar(id)
		*p++ = 27;
	*p++ = id;

	/* Type 0x06 for download firmware */
	*p++ = FW_CHUNK_TYPE;

	/* Dummy byte */
	*p++ = 0;

	/* Put data */
	for(idx = 0; idx < len; idx++){
		IsSpecialChar(*(data+idx)){
			*p++ = 27;
		}
		*p++ = *(data+idx);
	}

	/* checksum */
	IsSpecialChar(checksum)
		*p++ = 27;
	*p++ = checksum;

	/* last char */
	*p++ = '#';
	
	len = (p - buf);

	res = blTCPIP_Write(buf, len, 1);
	
	//release memory
	free(buf);
	
	return res;
}

/**
 * @fcn	void ServerDataHandler(S8* pData)
 * @brief	Data received handler
 * 			Frame: [1u-'$']SRV[1u-Checksum][1u-ID][2u-LenData][Vu-Data][1u-'#']
 * 			Pdata: ~~ [1u-Checksum][1u-ID][2u-LenData][Vu-Data]
 *
 * 			[1u-'$']SRV,ID,Data,CS_Data[1u-'#']
 *
 * @param	pData	Pointer to data
 */
void ServerDataHandler(S8* pData){
uint16_t idx;
//uint8_t *p = FWBuf;
uint8_t FWResCS;
S8* uItem[4];
S8 ItemCount;
uint8_t* p;
uint8_t pkgID;

	if(FWBuf == NULL)
		return;

	/* Validate data */
	// {0} ID, {1} Data, {2} CS_Data
	ItemCount = string_split((U8**)uItem, (U8*)pData, ",", 4);

	/* validate data */
	if(ItemCount != 3){
		FWDebug("E Wrong data\r\n");
		return;
	};

	/* @todo validate ID */
	pkgID = twochar2hex(*(uItem[0]), *(uItem[0] + 1));
	if(pkgID != fw_pkg_id){
		FWDebug("E: Wrong pkg ID\r\n");
		return;
	};

	/* Get checksum at third parameter */
	FWResCS = twochar2hex(*(uItem[2]), *(uItem[2] + 1));
	if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
		USART_PrChar(USART1,FWResCS);

	/* context based processing */
	switch(FWState){
	case FW_WAIT_REQUEST_FILE:

		// validate data by checksum
		if(checksum(uItem[1]) != FWResCS)
			return;

		/* Get number of package */
		p = (uint8_t*) &FWPkgCount;
		*p++ = twochar2hex(*(uItem[1]+2), *(uItem[1] + 3));
		*p++ = twochar2hex(*(uItem[1]), *(uItem[1] + 1));

//		/* @todo validate data */
//		p = (uint8_t*) &FWPkgCount;
//		if(FWResCS != (p[0] ^ p[1]))
//		{
//			USART_PrChar(USART1, (p[0] ^ p[1]));
//			return;
//		}

		/* Send event to firmware task */
		os_evt_set(FW_REQUEST_FILE_EVT,FWTsk_ID);

		break;
	case FW_WAIT_CHUNK_DATA:
		// [1u-'$']SRV,ID,Data,CS_Data[1u-'#']

		/* copy to FWBuf and process at FW task */
		if( strlen(uItem[1]) > SD_BUFFER_SIZE+3)
			return;							 

		memset(FWBuf, 0, SD_BUFFER_SIZE+3);
		memcpy(FWBuf, uItem[1], strlen(uItem[1]));

		/* parse checksum */

		/* Send event to firmware task */
		os_evt_set(FW_REQUEST_CHUNK_EVT,FWTsk_ID);

		break;
	}

	return;
}
