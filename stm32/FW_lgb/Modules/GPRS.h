/*
 * GPRS.h
 *
 *  Created on: Feb 1, 2012
 *      Author: Van Ha
 */

#ifndef GPRS_H_
#define GPRS_H_

// state gprs.
#define 	NONE		0
#define 	OPEN		1
#define		READY		2

typedef enum{
	GPRS_NONE = 0,
	GPRS_OPEN,
	GPRS_READY,
	GPRS_READ_BUFFER,
	GPRS_SEND_DATA,
	GPRS_WAIT_ACK
} GPRS_State_t;

#define MAX_SIZE_PACKET		1024

typedef enum _MSG_Type_e
{
	RAW_DATA = 0,
	CAM_DATA,
	SYN_DATA,
	ACK_CFG_SET,
	QUERY_VERSION,
	ERROR_CODE,
	ACK_DRV_SYN,
	FW_CHUNK_TYPE,
	TAXIMETER_DATA = 9,
	SYSTEM_REPORT_TYPE = 10,
	EXT_DEVICE_TAX = 11,

	BUS_REPORT_OVER_ACC = 15,
	BUS_REPORT_TRACK_ERROR = 16,

	SOS_ALARM = 20,
	READ_ALL_CONFIG,
	DEVICE_STATUS_INFO,
	IO_MESSAGE_TYPE,
	LOGS_MESSAGE_TYPE,
}MSG_Type_e;


typedef enum
{
	SIM18_ERR_CODE = 0,
	SDCARD_ERR_CODE,
	NODATA_TIMEOUT,
	TM_DISCONNECT_TIMEOUT
}Err_code_e;

/* Global macros */
#define WDT_GSM_MAX_TIMING			180		// 120 x 1s = 120s
#define WDT_DOWNLOAD_FW_TIMEOUT 	4500	// 0.8s * 4500 = 32 min

#define WDT_CLIENT_RECV_MAX_TIMING	375 // 375*800/1000/60 = 5 min		
#define WDT_LOG2BUF_MAX_TIMING 		375 // 375*800/1000/60 = 5 min

extern uint32_t wdt_gsm_download_fw_timeout;

/*** */
void vGPRS_Init(void);

/* put data to buffer. wait to send */
BOOL blPushData(U8 type, S8 *data, U16 size);
uint8_t WDT_Check_GSM_Service(void);

BOOL blSendError(Err_code_e error, uint16_t total);
uint8_t WDT_Check_GSM_Service(void);
uint8_t WDT_Check_ClientRecv_Service(void);
uint8_t WDT_Check_Log2Buf_Service(void);

#endif /* GPRS_H_ */
