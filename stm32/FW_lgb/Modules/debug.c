#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "string.h"
#include "platform_config.h"
#include "debug.h"
#include "utils.h"
#include "loadConfig.h"
#include "sim900.h"
#include "gsm.h"
#include "voice.h"
#include "rtc.h"
#include "drvkey.h"
#include "watchdog.h"
#include "camera.h"
#include "lgb.h"

/* Static function --------------------------- */
__task void dbg_ReceiveHandle(void);
void dbg_decodeCmd(uint8_t data);
void dbg_uploadLogHandler(uint8_t* dbg_buf);
void dbg_configHandler(uint8_t* dbg_buf);
void dbg_systemConfig(uint8_t* dbg_buf);
void dbg_adminConfig(uint8_t* dbg_buf);
//
extern int16_t sys_log_fwrite(uint8_t type, int8_t *logs, int16_t lsize);

/* Static variables -------------------------- */
#define DBG_BUF_LEN			250

/* Private macros ---------------------------- */
#define	DBG_UPLOAD_LOG_EVENT	0x0001
#define DBG_CONFIG_EVENT		0x0002
#define DBG_SERVER_CONFIG_EVENT 0X0003
#define	DBG_SYSTEM_CONFIG_EVENT	0x0004
#define	DBG_ADMIN_CONFIG_EVENT	0x0008

//#define 	SMS_FORWARD
#ifdef		SMS_FORWARD
void forward_sms(uint8_t *buf);
#endif

OS_TID tidDbg;
uint8_t dbg_buf[DBG_BUF_LEN];
uint8_t dbg_isr_buf[DBG_BUF_LEN];
static enum{
	DBG_NORMAL_MODE,
	DBG_UPLOAD_MODE,
} dbg_mode = DBG_UPLOAD_MODE;

// global variable for configuration system mode.
// default PRINTER_MODE for print data.
// DEBUG_MODE mode for debug, test.
system_mode_e glSystemMode = PRINTER_MODE;

static uint64_t tidDbg_buf[600/8];

/* Debug init */
void debug_init(void){
	// Init UART for debug (stm32_init.c)

	// Create process task
	tidDbg = os_tsk_create_user(dbg_ReceiveHandle, 0x02, tidDbg_buf, sizeof(tidDbg_buf));
}

/* Debug IRQ Handler */
void DBG_IRQHandler(void){
uint16_t data;


	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
		data = USART_ReceiveData(USART1);
		//USART_SendData(USART2, data);

		dbg_decodeCmd(data);
	};
}

/* Debug decode handler */
void dbg_decodeCmd(uint8_t data){
static uint8_t idx = 0;

	// Handle data based on value of data
	switch(data){
	case '$':				// Start char
		idx=0;
		memset(dbg_isr_buf, 0, sizeof(dbg_isr_buf));

		break;

	case '#':				// Terminal char
		dbg_isr_buf[idx] = 0;

		memcpy(dbg_buf, dbg_isr_buf, sizeof(dbg_buf));

		// PC,ANALYSIS message
		if(strncmp(dbg_isr_buf, "PC,", 3 ) == 0){
			isr_evt_set(DBG_UPLOAD_LOG_EVENT, tidDbg);
			break;
		};

		// Logging: $SCFG
		if(strncmp(dbg_isr_buf, "SCFG,", 5) == 0){
			isr_evt_set(DBG_CONFIG_EVENT, tidDbg);
			break;
		};

		/*  $SYSTEM_MODE,xx# */
		if(strncmp(dbg_isr_buf, "SYSTEM_MODE,", 12)==0)
		{
			isr_evt_set(DBG_SYSTEM_CONFIG_EVENT, tidDbg);
			break;
		}
		
		// $ADMIN,...#
		if(strncmp(dbg_isr_buf, "ADMIN,", 5)==0)
		{
			isr_evt_set(DBG_ADMIN_CONFIG_EVENT, tidDbg);
			break;
		}

		break;
	case ',':				// colon char

		//break;
	default:				// Data
		// Put data into the buffer
		dbg_isr_buf[idx++] = data;

		// Reset index
		if(idx >= DBG_BUF_LEN) idx=0;

		break;
	}
}

/* RTX Task ------------------------------------------ */

/* Debug handle task */
__task void dbg_ReceiveHandle(void){
OS_RESULT result;
uint16_t res_flags;


	for(;;){
		result = os_evt_wait_or(DBG_UPLOAD_LOG_EVENT | DBG_CONFIG_EVENT | DBG_SYSTEM_CONFIG_EVENT | DBG_ADMIN_CONFIG_EVENT, 0xffff);
		if(result == OS_R_EVT){
			res_flags = os_evt_get();

			switch(res_flags){
			case DBG_UPLOAD_LOG_EVENT:
				dbg_uploadLogHandler(dbg_buf);
				break;
			case DBG_CONFIG_EVENT:
				// log.
				sys_log_fwrite(1, dbg_buf, strlen(dbg_buf));
				// config.
				dbg_configHandler(dbg_buf);
				break;
			case DBG_SERVER_CONFIG_EVENT:
				dbg_configHandler(dbg_buf);
				break;

			case DBG_SYSTEM_CONFIG_EVENT:
				dbg_systemConfig(dbg_buf);
				break;
			
			case DBG_ADMIN_CONFIG_EVENT:
				dbg_adminConfig(dbg_buf + 6);
				break;
			
			}
		}
	}
}

void dbg_adminConfig(uint8_t* dbg_buf)
{
	/*// $ADMIN,PASS=123456# */
	if(strncmp(dbg_buf, "PASS=", 5) == 0){
		if(strlen(dbg_buf + 5) <= 6){
			strcpy(Config.admin.pass, (dbg_buf + 5));
		}
		USART_PrCStr(DBG_Port, "$DCFG,OK#");
	}
	/*// $SCFG,SET,USER=0945615452# */
	else if(strncmp(dbg_buf, "USER=", 5) == 0){
		if(strlen(dbg_buf + 5) <= 16){
			strcpy(Config.admin.user, (dbg_buf + 5));
		}
		USART_PrCStr(DBG_Port, "$DCFG,OK#");
	}
}

/* config system mode */
void dbg_systemConfig(uint8_t* dbg_buf)
{
	unsigned short mode = atoi(dbg_buf+12);

	/**
	 * $SYSTEM_MODE,0#			// PRINT_MODE
	 * $SYSTEM_MODE,1#			// DEBUG_MODE
	 *
	 */
	//using 2 mode: 0 & 1 (PRINT_MODE & DEBUG_MODE)
	if(mode > 1)
		mode = 0;		// default PRINT_MODE

	glSystemMode = (system_mode_e)mode;
}

/* Set param from server */
void dbg_setConfigParams(uint8_t* frame){
	/* Copy data to dbg_buf */
	//memcpy(dbg_buf, frame, len);
	sprintf(dbg_buf, "SCFG%s",frame);

	/* Raise event */
	os_evt_set(DBG_SERVER_CONFIG_EVENT, tidDbg);
}


/* Upload file handler -------------------------------------------------------------------------------- */
/**
 * Protocol:
 * 	// For reading data
 *		Request: 	PC,READ,REQUEST,*filename*,*block_size*
 *		Response:	PC,READ,REQUEST,*number of block*, -1 if error
 *
 *		// Request block file
 *		Req:	PC,READ,GET,*filename*,*block_size*,*block idx*
 *		Res:	PC,READ,GET,*block idx*,*block data(in hex string)*
 *
 *	// Writing data
 *		Request: PC,REQUEST,WRITE,*filename*
 *		Response:
 *
 *		// Write data
 *		Req: PC,WRITE,PUT,*block idx*,*data in  hex string*
 *		Res: PC,WRITE,PUT,*block idx*, *status*
 */
#define DBG_MAX_READ_BLOCKSIZE 256
#define	LOG_BLOCK_SIZE			sizeof(LogInfo_t)

FILE* f_dbg;
uint8_t dbg_rw_buffer[DBG_MAX_READ_BLOCKSIZE];
extern OS_MUT logLock;

void dbg_uploadLogHandler(uint8_t* pcBuf){
uint8_t item_size;
char *pItem[6];
uint8_t idx;
static char* dbg_filename;
static uint32_t blockID;
static uint32_t blockSize;
uint8_t res;
uint8_t HexPair[2];

	/* For old version ---------------------- */

	/* Start sync data */
	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,START") == 0){
		dbg_mode = DBG_UPLOAD_MODE;
		return;
	}

	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,STOP") == 0){
		dbg_mode = DBG_NORMAL_MODE;
		return;
	}

	/* $PC,ANALYSIS,GSM_MODE=NORMAL# */
	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,GSM_MODE=NORMAL") == 0){
		gsm_mode = GSM_NORMAL_MODE;
		return;
	}

	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,GSM_MODE=DEBUG") == 0){
		gsm_mode = GSM_DEBUG_MODE;
		return;
	}


	// Frame: [PC],[READ,WRITE],[FILENAME],[Block size]

	// Split data from dbg_buf
	item_size = string_split((uint8_t**) pItem, pcBuf, ",", 6);
	if(item_size < 4)
		return;

	/* Check 2nd item: Type of request */
	// Type READ request
	if(strcmp(pItem[1], "READ") == 0){
		// Validate
		if(item_size != 6)
			return;

		// request type
		if(strcmp(pItem[2], "REQUEST") == 0){
			return;
		}

		/* *PC,READ,GET,*filename*,*block_size*,*block idx** */
		// pItem[3] ~ Filename
		// pItem[4] ~ Block size
		// pItem[5] ~ Block ID

		if(strcmp(pItem[2], "GET") == 0){
			// Get parameters
			dbg_filename = (char*) pItem[3];
			blockSize = atoi(pItem[4]);			
			blockID = atoi(pItem[5]);
			
			// Validate blocksize
			if(blockSize > DBG_MAX_READ_BLOCKSIZE)
				return;		

			// Mutex log
			os_mut_wait(logLock, 0xffff);

			// Read data at the desired data
			f_dbg = fopen(dbg_filename, "rb");
			if(f_dbg == NULL){
				// @todo: No file was found
				sprintf(dbg_rw_buffer,"$PC,READ,GET,-1#");
				USART_PrCStr(DBG_Port, dbg_rw_buffer);
				os_mut_release(logLock);
				return;
			};
			
			// Move to desired point
			if(fseek(f_dbg, blockSize * blockID, SEEK_CUR) == EOF){
				fclose(f_dbg);
				os_mut_release(logLock);
				return;
			}

			// Read data
			res = fread(dbg_rw_buffer, sizeof(uint8_t), blockSize, f_dbg);
			if(res == 0){
				fclose(f_dbg);
				os_mut_release(logLock);

				sprintf(dbg_rw_buffer,"$PC,READ,GET,-2#");
				USART_PrCStr(DBG_Port, dbg_rw_buffer);
				return;
			}

			// Close data
			fclose(f_dbg);
			os_mut_release(logLock);

			/* Ouput data to DBG: PC,READ,GET,*block idx*,*block data(in hex string)* */
			USART_PrCStr(DBG_Port, "$PC,READ,GET,");
			USART_PrCStr(DBG_Port,pItem[5]);			// block_id
			USART_PrChar(DBG_Port,',');

			// Hex data
			for(idx=0;idx<res;idx++){
				hex2ascii(*(dbg_rw_buffer+idx), HexPair);
				USART_PrStr(DBG_Port, HexPair, 2);
			}

			USART_PrChar(DBG_Port, '#');


			os_dly_wait(10);

			return;
		}

	}

	// Type WRITE request
	if(strcmp(pItem[1], "READ") == 0){

	}

}

#if 0
/* Upload log handler */
void dbg_uploadLogHandler(uint8_t* dbg_buf){
uint32_t res;
uint8_t log_data[LOG_BLOCK_SIZE];
uint32_t idx;
uint8_t HexPair[2];

	/* Start sync data */
	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,START") == 0){
		dbg_mode = DBG_UPLOAD_MODE;
		return;
	}

	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,STOP") == 0){
		dbg_mode = DBG_NORMAL_MODE;
		return;
	}

	/* $PC,ANALYSIS,GSM_MODE=NORMAL# */
	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,GSM_MODE=NORMAL") == 0){
		gsm_mode = GSM_NORMAL_MODE;
		return;
	}

	if(strcmp((uint8_t*) dbg_buf,  "PC,ANALYSIS,GSM_MODE=DEBUG") == 0){
		gsm_mode = GSM_DEBUG_MODE;
		return;
	}

	/* Get list of file */
	if(strcmp((uint8_t*) dbg_buf, "PC,ANALYSIS,LISTFILES") == 0){
		__breakpoint(0);
		return;
	}

	// Read log: $PC,ANALYSIS,LOGFILE,240212#
	if(strncmp((uint8_t*) dbg_buf, "PC,ANALYSIS,LOGFILE,", 20) == 0){

		// Only work at upload mode
		if(dbg_mode != DBG_UPLOAD_MODE) return;

		// Get filename
		memcpy(dbg_logname+4, dbg_buf + 20, 6);

		/* @todo: read data block by block and send to PC */
		f_log = fopen(dbg_logname, "r");
		if(f_log == NULL){
			// @todo: No file was found
			USART_PrCStr(DBG_Port, "$PC,ANALYSIS,LOGFILE,NODATA#");
			return;
		};

		// Read data block by block: $PC,ANALYSIS,LOGFILE,[hex data]
		while(!feof(f_log)){
			res = fread(log_data, sizeof(uint8_t), LOG_BLOCK_SIZE, f_log);
			if(res == 0) break;

			/* Send data to UART */
			USART_PrCStr(DBG_Port, "$PC,ANALYSIS,LOGFILE,");

			// Hex data
			for(idx=0;idx<res;idx++){
				hex2ascii(*(log_data+idx), HexPair);
				USART_PrStr(DBG_Port, HexPair, 2);
			}

			// stop
			USART_PrChar(DBG_Port, '#');

			/* Delay for the next read one */

		}

		// Send end of file
		USART_PrCStr(DBG_Port, "$PC,ANALYSIS,LOGFILE,END#");

		fclose(f_log);	 		
	}
}
#endif

/* Config handler -------------------------------------------------- */

/* Private types --------------------------------------------------- */
typedef struct{
	uint8_t fieldName[20];
	uint8_t len;
} field_t;

const field_t ConfigFields[] = {
		{"DeviceID=", 			9},
		{"SIM=", 				4},
		{"APNName=", 			8},
		{"APNuser=", 			8},
		{"APNPass=", 			8},
		{"IP1=", 				4},
		{"IP2=", 				4},
		{"Port1=", 				6},
		{"Port2=", 				6},
		{"VehicleVIN=", 		11},
		{"VehicleID=", 			10},
		{"DriverName=", 		11},
		{"DriverLic=", 			10},
		{"DriverIssueDate=", 	16},
		{"DriverExpiryDate=", 	17},
/* Option byte */
		{"OptionPlayVoice=",	16},
		{"usedDelaySend=",		14},
		{"usedIOMessage=",		14},
		{"Camera=",				7},
		{"CycleCamera=",		12},
		{"AlarmSpeed=",			11},
		{"GPSRadius=", 			10},
		{"GPSFilter=", 			10},
		{"SOSAlarmAdd=", 		12},
		{"SOSAlarmRem=", 		12},
		{"SOSAlarmClear", 		13},
		{"DKEY=", 					5},
		{"CCAM_ID=", 				8},
};

enum ConfigFieldIndex{
	CFG_DEVICE_ID = 0,
	CFG_SIM,
	CFG_APN_NAME,
	CFG_APN_USER,
	CFG_APN_PASS,
	CFG_APN_IP1,
	CFG_APN_IP2,
	CFG_PORT1,
	CFG_PORT2,
	CFG_VIN,
	CFG_ID,
	CFG_DRV_NAME,
	CFG_DRV_LIC,
	CFG_DRV_ISS_DATE,
	CFG_DRV_EXP_DATE,
	CFG_OPTION_PLAYVOICE,
	CFG_OPTION_USE_DELAY_SEND,
	CFG_OPTION_USE_IO_MESSAGE,
	CFG_OPTION_CAMERA,
	CFG_CAMERA_CYCLE,
	CFG_ALARM_SPEED,
	CFG_GPS_RADIUS,
	CFG_GPS_FILTER,
	CFG_SOS_ALARM_ADD,
	CFG_SOS_ALARM_REM,
	CFG_SOS_ALARM_CLEAR,
	CFG_DRIVER_KEY,
	CFG_CHANGE_CAM_ID,
};


void vSendLogData(S8 *p);

/* Config handler */
void dbg_configHandler(uint8_t* dbg_buf){

		/* Set command */
		if(strncmp(dbg_buf, "SCFG,SET,", 9) == 0){

		/* read log for test function */
		if(strncmp(dbg_buf + 9, "RLOG",  4) == 0){
			vSendLogData(dbg_buf + 9 + 4);
			return;
		}

		/* write over camera id */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_CHANGE_CAM_ID].fieldName,  ConfigFields[CFG_CHANGE_CAM_ID].len) == 0){
			if(blChangeCAM_ID(*(dbg_buf + 9 + ConfigFields[CFG_CHANGE_CAM_ID].len)-'0', *(dbg_buf + 9 + ConfigFields[CFG_CHANGE_CAM_ID].len + 2)-'0'))
				USART_PrCStr(DBG_Port, "$EVT,OK#");
			else
				USART_PrCStr(DBG_Port, "$EVT,FAIL#");

			return;
		}

		if(strncmp(dbg_buf + 9, ConfigFields[CFG_DRIVER_KEY].fieldName, ConfigFields[CFG_DRIVER_KEY].len ) == 0){
			S8 dkey[8];
			
			blWriteInfo(dbg_buf+9+ConfigFields[CFG_DRIVER_KEY].len, 8);
			// wait for store finish.
			os_dly_wait(30);
			// read again for verify.
			memset(dkey, 0, 8);
			blReadInfo(dkey, 8);
			// check.
			if(strncmp(dbg_buf+9+ConfigFields[CFG_DRIVER_KEY].len, dkey, 8)==0)
				USART_PrCStr(DBG_Port, "$EVT,OK#");
			else
				USART_PrCStr(DBG_Port, "$EVT,FAIL#");

			return;
		}

		// $SCFG,SET,DeviceID=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_DEVICE_ID].fieldName, ConfigFields[CFG_DEVICE_ID].len ) == 0){
			memset(Config.BoxID, 0, sizeof(Config.BoxID));
			memcpy(Config.BoxID, dbg_buf + 9 + ConfigFields[CFG_DEVICE_ID].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_DEVICE_ID].len);
			goto response_set;
		}

		// $SCFG,SET,SIM=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_SIM].fieldName, ConfigFields[CFG_SIM].len ) == 0){
			memset(Config.SIM_Info, 0, sizeof(Config.SIM_Info));
			memcpy(Config.SIM_Info, dbg_buf + 9 + ConfigFields[CFG_SIM].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_SIM].len);
			goto response_set;
		}

		// $SCFG,SET,APNName=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_APN_NAME].fieldName, ConfigFields[CFG_APN_NAME].len ) == 0){
			memset(Config.apn_name, 0, sizeof(Config.apn_name));
			memcpy(Config.apn_name, dbg_buf + 9 + ConfigFields[CFG_APN_NAME].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_APN_NAME].len);
			goto response_set;
		}

		// $SCFG,SET,APNuser=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_APN_USER].fieldName, ConfigFields[CFG_APN_USER].len ) == 0){
			memset(Config.apn_user, 0, sizeof(Config.apn_user));
			memcpy(Config.apn_user, dbg_buf + 9 + ConfigFields[CFG_APN_USER].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_APN_USER].len);
			goto response_set;
		}

		// $SCFG,SET,APNPass=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_APN_PASS].fieldName, ConfigFields[CFG_APN_PASS].len ) == 0){
			memset(Config.apn_pass, 0, sizeof(Config.apn_pass));
			memcpy(Config.apn_pass, dbg_buf + 9 + ConfigFields[CFG_APN_PASS].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_APN_PASS].len);
			goto response_set;
		}

		// $SCFG,SET,IP1=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_APN_IP1].fieldName, ConfigFields[CFG_APN_IP1].len ) == 0){
			memset(Config.IP1, 0, sizeof(Config.IP1));
			memcpy(Config.IP1, dbg_buf + 9 + ConfigFields[CFG_APN_IP1].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_APN_IP1].len);
			goto response_set;
		}

		// $SCFG,SET,IP2=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_APN_IP2].fieldName, ConfigFields[CFG_APN_IP2].len ) == 0){
			memset(Config.IP2, 0, sizeof(Config.IP2));
			memcpy(Config.IP2, dbg_buf + 9 + ConfigFields[CFG_APN_IP2].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_APN_IP2].len);
			goto response_set;
		}

		// $SCFG,SET,Port1=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_PORT1].fieldName, ConfigFields[CFG_PORT1].len ) == 0){
			memset(Config.Port1, 0, sizeof(Config.Port1));
			memcpy(Config.Port1, dbg_buf + 9 + ConfigFields[CFG_PORT1].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_PORT1].len);
			goto response_set;
		}

		// $SCFG,SET,Port2=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_PORT2].fieldName, ConfigFields[CFG_PORT2].len ) == 0){
			memset(Config.Port2, 0, sizeof(Config.Port2));
			memcpy(Config.Port2, dbg_buf + 9 + ConfigFields[CFG_PORT2].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_PORT2].len);
			goto response_set;
		}

		// $SCFG,SET,VehicleVIN=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_VIN].fieldName, ConfigFields[CFG_VIN].len ) == 0){
			memset(Config.VehicleVIN, 0, sizeof(Config.VehicleVIN));
			memcpy(Config.VehicleVIN, dbg_buf + 9 + ConfigFields[CFG_VIN].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_VIN].len);
			goto response_set;
		}

		// $SCFG,SET,VehicleID=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_ID].fieldName, ConfigFields[CFG_ID].len ) == 0){
			memset(Config.VehicleID, 0, sizeof(Config.VehicleID));
			memcpy(Config.VehicleID, dbg_buf + 9 + ConfigFields[CFG_ID].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_ID].len);
			goto response_set;
		}

		// $SCFG,SET,DriverName=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_DRV_NAME].fieldName, ConfigFields[CFG_DRV_NAME].len ) == 0){
			memset(Config.DefaultDriver.name, 0, sizeof(Config.DefaultDriver.name));
			memcpy(Config.DefaultDriver.name, dbg_buf + 9 + ConfigFields[CFG_DRV_NAME].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_DRV_NAME].len);
			goto response_set;
		}

		// $SCFG,SET,DriverLic=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_DRV_LIC].fieldName, ConfigFields[CFG_DRV_LIC].len ) == 0){
			memset(Config.DefaultDriver.lic_number, 0, sizeof(Config.DefaultDriver.lic_number));
			memcpy(Config.DefaultDriver.lic_number, dbg_buf + 9 + ConfigFields[CFG_DRV_LIC].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_DRV_LIC].len);
			goto response_set;
		}

		// $SCFG,SET,DriverIssueDate=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_DRV_ISS_DATE].fieldName, ConfigFields[CFG_DRV_ISS_DATE].len ) == 0){
			memset(Config.DefaultDriver.issueDate, 0, sizeof(Config.DefaultDriver.issueDate));
			memcpy(Config.DefaultDriver.issueDate, dbg_buf + 9 + ConfigFields[CFG_DRV_ISS_DATE].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_DRV_ISS_DATE].len);
			goto response_set;
		}

		// $SCFG,SET,DriverExpiryDate=xxxx#
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_DRV_EXP_DATE].fieldName, ConfigFields[CFG_DRV_EXP_DATE].len ) == 0){
			memset(Config.DefaultDriver.expiryDate, 0, sizeof(Config.DefaultDriver.expiryDate));
			memcpy(Config.DefaultDriver.expiryDate, dbg_buf + 9 + ConfigFields[CFG_DRV_EXP_DATE].len, strlen(dbg_buf) - 9 - ConfigFields[CFG_DRV_EXP_DATE].len);
			goto response_set;
		}

		/* Option bits */

		/*// $SCFG,SET,OptionPlayVoice=x# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_OPTION_PLAYVOICE].fieldName, ConfigFields[CFG_OPTION_PLAYVOICE].len ) == 0){
			Config.option.playVoice = *(dbg_buf + 9 + ConfigFields[CFG_OPTION_PLAYVOICE].len) - '0';
			goto response_set;
		}

		/*// $SCFG,SET,usedDelaySend=x# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_OPTION_USE_DELAY_SEND].fieldName, ConfigFields[CFG_OPTION_USE_DELAY_SEND].len ) == 0){
			Config.option.usedDelaySend = *(dbg_buf + 9 + ConfigFields[CFG_OPTION_USE_DELAY_SEND].len) - '0';
			goto response_set;
		}
		
		
		/*// $SCFG,SET,usedIOMessage=x# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_OPTION_USE_IO_MESSAGE].fieldName, ConfigFields[CFG_OPTION_USE_IO_MESSAGE].len ) == 0){
			Config.option.usedIOMessage = (*(dbg_buf + 9 + ConfigFields[CFG_OPTION_USE_IO_MESSAGE].len) != '0');
			goto response_set;
		}

		/* Option camera */

		/*// $SCFG,SET,Camera=x,y# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_OPTION_CAMERA].fieldName, ConfigFields[CFG_OPTION_CAMERA].len ) == 0){
			Config.CAM.cam_used = *(dbg_buf + 9 + ConfigFields[CFG_OPTION_CAMERA].len) - '0';
			Config.CAM.cam_number = *((dbg_buf + 9 + ConfigFields[CFG_OPTION_CAMERA].len + 2)) - '0';

			goto response_set;
		}

		/*// $SCFG,SET,CycleCamera=xxx# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_CAMERA_CYCLE].fieldName, ConfigFields[CFG_CAMERA_CYCLE].len ) == 0){
			uint16_t a;
			a = atoi(dbg_buf + 9 + ConfigFields[CFG_CAMERA_CYCLE].len);
			if(a > 60)
				Config.CAM.CamSnapShotCycle = a;

			goto response_set;

		}

		/*// $SCFG,SET,AlarmSpeed=enable,value# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_ALARM_SPEED].fieldName, ConfigFields[CFG_ALARM_SPEED].len ) == 0){
			uint16_t a;
			Config.alarmSpeed.alarmSpeedEnable = (uint8_t)(*(dbg_buf + 9 + ConfigFields[CFG_ALARM_SPEED].len) != '0');
			a = atoi(dbg_buf + 9 + ConfigFields[CFG_ALARM_SPEED].len+2);
			if(a <= 125)
				Config.alarmSpeed.alarmSpeedValue = a;

			goto response_set;

		}

		/*// $SCFG,SET,GPSRadius=x# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_GPS_RADIUS].fieldName, ConfigFields[CFG_GPS_RADIUS].len ) == 0){
			Config.GPS_radius = atoi(dbg_buf + 9 + ConfigFields[CFG_GPS_RADIUS].len);
			goto response_set;
		}

		/*// $SCFG,SET,GPSFilter=1,16# */
		if(strncmp(dbg_buf + 9, ConfigFields[CFG_GPS_FILTER].fieldName, ConfigFields[CFG_GPS_FILTER].len ) == 0){
			uint16_t t;
			uint8_t *s[4];

			t = string_split(s, dbg_buf + 9 + ConfigFields[CFG_GPS_FILTER].len, ",", 4);
			if(t == 4)
			{
				Config.GPSFilter.use_engine = (*(s[0]) != '0');
				Config.GPSFilter.method_filter = (*(s[1]) != '0');

				t = atoi(s[2]);
				if(t < 16)
					Config.GPSFilter.overspeed_threshold = t;

				t = atoi(s[3]);
				if(t < 128)
					Config.GPSFilter.timeCycle = t;
			}

			goto response_set;
		}
		
		/* Reset log buffer index */
		if(strcmp((uint8_t*) dbg_buf + 9, "BRST") == 0){
			sdfifo_reset();

			goto response_set;
		}

		/* Reset camera buffer index */
		if(strcmp((uint8_t*) dbg_buf + 9, "CRST") == 0){
			vSDBufferReset();

			goto response_set;
		}
		
		/* Reset IO buffer index */
		if(strcmp((uint8_t*) dbg_buf + 9, "IO_WIPE") == 0){
			io_wipe_in_sdcard();

			goto response_set;
		}

		/* $SCFG,SET,TMenable=0# 0 or 1*/
		if(strncmp(dbg_buf + 9,"TMenable",8) == 0)
		{
			Config.TM.tm_used = (uint8_t)(*(dbg_buf + 9 + 8 + 1) != '0');
			goto response_set;	
		}

		/* $SCFG,SET,TMtype=0#   0-127*/
		if(strncmp(dbg_buf + 9,"TMtype",6) == 0)
		{
			uint16_t a;
			a = atoi(dbg_buf + 9 + 6 + 1);
			if(a >=0 && a <=127)
			{
				Config.TM.tm_type = a;	
				goto response_set;	
			}
		}

		/* $SCFG,SET,TMtimeout=1# 1-255*/
		if(strncmp(dbg_buf + 9,"TMtimeout",9) == 0)
		{
			uint16_t a;
			a = atoi(dbg_buf + 9 + 9 + 1);
			if(a >=1 && a <=255)
			{
				Config.TM.tm_timeout = a;
				goto response_set;		
			}
		}

		/* $SCFG,SET,TMdebug=1# 0-1*/
		if(strncmp(dbg_buf + 9,"TMdebug",7) == 0)
		{
			if(*(dbg_buf + 9 + 7 + 1) != '0')
				TmDebugEnableFlag = __TRUE;
			else
				TmDebugEnableFlag = __FALSE;
			
			goto response_set;	
		}
		
		if(strncmp(dbg_buf + 9,"TMBRST",6) == 0)
		{
			LGB_SDBufferReset();
			goto response_set;	
		}

		/* $SCFG,SET,FACTORY# */
		if(strncmp(dbg_buf + 9, "FACTORY", 7) == 0){
			USART_PrCStr(DBG_Port, "$DCFG,OK#");

			vFactoryReset();
			/* Reset system */
			vIWDG_Init();
			tsk_lock();
		}

		if(strncmp(dbg_buf + 9, "TIME=", 5) == 0){

			uint8_t hh, mm, ss;
			hh = atoi(dbg_buf + 14 );
			mm = atoi(dbg_buf + 17);
			ss = atoi(dbg_buf + 20);

			vRTCSetDate(ss, mm, hh);

			hh = atoi(dbg_buf + 23 );
			mm = atoi(dbg_buf + 26);
			ss = atoi(dbg_buf + 29);
			vRTCSetTime(hh, mm, ss);

			goto response_set;
		}


		/* $SCFG,SET,APPLY# */
		if(strncmp(dbg_buf + 9, "APPLY", 5) == 0){
			if( config_apply(&Config) == 0){
				USART_PrCStr(DBG_Port, "$DCFG,OK#");

				/* Reset system */

				vIWDG_Init();
				tsk_lock();
			};
		}

		return;
	}

	/* Get command */
	if(strncmp(dbg_buf, "SCFG,GET,", 9) == 0){

		// $SCFG,GET,ALL#
		if(strncmp(dbg_buf + 9, "ALL", 3) == 0){
			USART_PrCStr(DBG_Port, "$DCFG,GET,");

			/* Device information */

			// Device ID (BoxID)
			USART_PrCStr(DBG_Port, ConfigFields[CFG_DEVICE_ID].fieldName);
			USART_PrCStr(DBG_Port, Config.BoxID);

			USART_PrChar(DBG_Port, ',');

			// IP1
			USART_PrCStr(DBG_Port, ConfigFields[CFG_APN_IP1].fieldName);
			USART_PrCStr(DBG_Port, Config.IP1);

			USART_PrChar(DBG_Port, ',');

			// IP2
			USART_PrCStr(DBG_Port, ConfigFields[CFG_APN_IP2].fieldName);
			USART_PrCStr(DBG_Port, Config.IP2);

			USART_PrChar(DBG_Port, ',');

			// Port1
			USART_PrCStr(DBG_Port, ConfigFields[CFG_PORT1].fieldName);
			USART_PrCStr(DBG_Port, Config.Port1);

			USART_PrChar(DBG_Port, ',');

			// Port2
			USART_PrCStr(DBG_Port, ConfigFields[CFG_PORT2].fieldName);
			USART_PrCStr(DBG_Port, Config.Port2);

			USART_PrChar(DBG_Port, ',');

			/* APN Information */

			// Apnname
			USART_PrCStr(DBG_Port, ConfigFields[CFG_APN_NAME].fieldName);
			USART_PrCStr(DBG_Port, Config.apn_name);

			USART_PrChar(DBG_Port, ',');

			// Apnuser
			USART_PrCStr(DBG_Port, ConfigFields[CFG_APN_USER].fieldName);
			USART_PrCStr(DBG_Port, Config.apn_user);

			USART_PrChar(DBG_Port, ',');

			// apnpass
			USART_PrCStr(DBG_Port, ConfigFields[CFG_APN_PASS].fieldName);
			USART_PrCStr(DBG_Port, Config.apn_pass);

			USART_PrChar(DBG_Port, ',');

			// SIM
			USART_PrCStr(DBG_Port, ConfigFields[CFG_SIM].fieldName);
			USART_PrCStr(DBG_Port, Config.SIM_Info);

			USART_PrChar(DBG_Port, ',');


			/* Vehicle information */

			// Vehicle ID
			USART_PrCStr(DBG_Port, ConfigFields[CFG_ID].fieldName);
			USART_PrCStr(DBG_Port, Config.VehicleID);

			USART_PrChar(DBG_Port, ',');

			// Vehicle VIN
			USART_PrCStr(DBG_Port, ConfigFields[CFG_VIN].fieldName);
			USART_PrCStr(DBG_Port, Config.VehicleVIN);

			USART_PrChar(DBG_Port, ',');


			/* Driver info */

			// Driver Lic
			USART_PrCStr(DBG_Port, ConfigFields[CFG_DRV_LIC].fieldName);
			USART_PrCStr(DBG_Port, Config.DefaultDriver.lic_number);

			USART_PrChar(DBG_Port, ',');

			// Driver Name
			USART_PrCStr(DBG_Port, ConfigFields[CFG_DRV_NAME].fieldName);
			USART_PrCStr(DBG_Port, Config.DefaultDriver.name);

			USART_PrChar(DBG_Port, ',');

			//DriverIssueDate
			// Driver Expiry date
			USART_PrCStr(DBG_Port, ConfigFields[CFG_DRV_ISS_DATE].fieldName);

			USART_PrCStr(DBG_Port, Config.DefaultDriver.issueDate);
			USART_PrChar(DBG_Port, ',');


			// Driver Expiry date
			USART_PrCStr(DBG_Port, ConfigFields[CFG_DRV_EXP_DATE].fieldName);
			USART_PrCStr(DBG_Port, Config.DefaultDriver.expiryDate);

			USART_PrChar(DBG_Port, ',');

			/* Option bits */
			USART_PrCStr(DBG_Port, ConfigFields[CFG_OPTION_PLAYVOICE].fieldName);
			USART_PrChar(DBG_Port, Config.option.playVoice + '0');

			USART_PrChar(DBG_Port, ',');

			/* Option camera */
			USART_PrCStr(DBG_Port, ConfigFields[CFG_OPTION_CAMERA].fieldName);
			USART_PrChar(DBG_Port, Config.CAM.cam_used + '0');
			USART_PrChar(DBG_Port, ',');
			USART_PrChar(DBG_Port, Config.CAM.cam_number + '0');
			USART_PrChar(DBG_Port, ',');
			sprintf(dbg_buf, "%03u\x0", Config.CAM.CamSnapShotCycle);
			USART_PrCStr(DBG_Port, dbg_buf);

			USART_PrChar(DBG_Port, ',');

			/*Option taxi meter*/
			USART_PrCStr(DBG_Port, "TaxiMeter=");
			USART_PrChar(DBG_Port, Config.TM.tm_used + '0');
			USART_PrChar(DBG_Port, ',');

			sprintf(dbg_buf, "%03u\x0",Config.TM.tm_type);
			USART_PrCStr(DBG_Port, dbg_buf);
			USART_PrChar(DBG_Port, ',');

			sprintf(dbg_buf, "%03u\x0",Config.TM.tm_timeout);
			USART_PrCStr(DBG_Port, dbg_buf);
			USART_PrChar(DBG_Port, ',');

			/* Option alarm speed */
			USART_PrCStr(DBG_Port, ConfigFields[CFG_ALARM_SPEED].fieldName);
			USART_PrChar(DBG_Port, Config.alarmSpeed.alarmSpeedEnable + '0');
			USART_PrChar(DBG_Port, ',');
			sprintf(dbg_buf, "%u\x0", Config.alarmSpeed.alarmSpeedValue);
			USART_PrCStr(DBG_Port, dbg_buf);

			USART_PrChar(DBG_Port, ',');

			/* Device version */
			USART_PrCStr(DBG_Port, "FWVersion=");
			USART_PrCStr(DBG_Port, FW_VERSION_STR);

			USART_PrChar(DBG_Port, ',');

			USART_PrCStr(DBG_Port, "HWVersion=");
			USART_PrCStr(DBG_Port, HW_VERSION_STR);

			USART_PrChar(DBG_Port, ',');

			USART_PrCStr(DBG_Port, "GPSRadius=");
			sprintf(dbg_buf, "%u\x0", Config.GPS_radius);
			USART_PrCStr(DBG_Port, dbg_buf);

			USART_PrChar(DBG_Port, ',');

			/* Option GPS filter speed */
			USART_PrCStr(DBG_Port, ConfigFields[CFG_GPS_FILTER].fieldName);
			USART_PrChar(DBG_Port, Config.GPSFilter.use_engine + '0');
			USART_PrChar(DBG_Port, ',');
			USART_PrChar(DBG_Port, Config.GPSFilter.method_filter + '0');
			USART_PrChar(DBG_Port, ',');
			sprintf(dbg_buf, "%u\x0", Config.GPSFilter.overspeed_threshold);
			USART_PrCStr(DBG_Port, dbg_buf);
			USART_PrChar(DBG_Port, ',');
			sprintf(dbg_buf, "%u\x0", Config.GPSFilter.timeCycle);
			USART_PrCStr(DBG_Port, dbg_buf);

			USART_PrChar(DBG_Port, ',');

			// delay send data to server.
			USART_PrCStr(DBG_Port, ConfigFields[CFG_OPTION_USE_DELAY_SEND].fieldName);
			USART_PrChar(DBG_Port, Config.option.usedDelaySend + '0');
			
			USART_PrChar(DBG_Port, ',');
			
			// IO message data to server.
			USART_PrCStr(DBG_Port, ConfigFields[CFG_OPTION_USE_IO_MESSAGE].fieldName);
			USART_PrChar(DBG_Port, Config.option.usedIOMessage + '0');
			
			USART_PrChar(DBG_Port, ',');
			
			// Admin permissions.
			USART_PrCStr(DBG_Port, "AdminUser=");
			USART_PrCStr(DBG_Port, Config.admin.user);
			
			USART_PrChar(DBG_Port, ',');
			
			// Admin permissions.
			USART_PrCStr(DBG_Port, "AdminPass=");
			USART_PrCStr(DBG_Port, Config.admin.pass);

			USART_PrChar(DBG_Port, '#');

			return;
		};

		// $SCFG,GET,TAX1#
		if(strncmp(dbg_buf + 9, "TAX1", 4) == 0){
			GSM_blSendCommand("at+cusd=1,\"*101#\"\r", "\r\nOK\r\n", 50);
			HostChannel.type = CH_COM;
			HostChannel.state = CH_STATE_BUSY;
		}

		// $SCFG,GET,TAX2#
		if(strncmp(dbg_buf + 9, "TAX2", 4) == 0){
			GSM_blSendCommand("at+cusd=1,\"*102#\"\r", "\r\nOK\r\n", 50);
			HostChannel.type = CH_COM;
			HostChannel.state = CH_STATE_BUSY;
		}

		return;

	}

	// $SCFG,FWR,TAX1,phone_number#
	if(strncmp(dbg_buf, "SCFG,FWR,TAX1", 13) == 0){
		GSM_blSendCommand("at+cusd=1,\"*101#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_SMS;
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, dbg_buf + 14);
		HostChannel.state = CH_STATE_BUSY;

		return;
	}

	// $SCFG,FWR,TAX2,phone_number#
	if(strncmp(dbg_buf, "SCFG,FWR,TAX2", 13) == 0){
		GSM_blSendCommand("at+cusd=1,\"*102#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_SMS;
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, dbg_buf + 14);
		HostChannel.state = CH_STATE_BUSY;

		return;
	}

response_set:
	//if( config_apply(&Config) == 0)
	USART_PrCStr(DBG_Port, "$DCFG,OK#");

	return;
}

/* Config handler */


/* Debug send data API -------------------------------------------------------- */
void DBG_PrChar(uint8_t data){
	// check system mode
	if(glSystemMode == PRINTER_MODE)
		return;

	if(dbg_mode != DBG_NORMAL_MODE) return;

	USART_PrChar(DBG_Port, data);
}

void dbg_print(uint8_t* str){
	// check system mode
	if(glSystemMode == PRINTER_MODE)
		return;

	if(dbg_mode != DBG_NORMAL_MODE) return;

	DBG_PrCStr(str);
}

void DBG_PrCStr(const uint8_t* str){
	// check system mode
	if(glSystemMode == PRINTER_MODE)
		return;

	if(dbg_mode != DBG_NORMAL_MODE) return;

	USART_PrCStr(DBG_Port, str);
}

void DBG_PrStr(uint8_t* str, uint16_t len){
	// check system mode
	if(glSystemMode == PRINTER_MODE)
		return;

	if(dbg_mode != DBG_NORMAL_MODE) return;

	USART_PrStr(DBG_Port, str, len);
}
