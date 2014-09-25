/*
 * GPRS.c
 *
 *  Created on: Jan 28, 2012
 *      Author: Van Ha
 *  comm operator    s/n: N8WS3-EPBDN-KM7N5-M6BGN-9KP83
 *
 *  version
 *  0.2
 *  	- thuc hien buffer data vao the nho, sau do gui lan luc theo thu tu fifo.
 *  	-
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "rtl.h"
#include "stm32f10x.h"
#include "platform_config.h"

#include "utils.h"
#include "serial.h"
#include "logger.h"
#include "loadconfig.h"

#include "sim900.h"
#include "gsm.h"
#include "http.h"
#include "debug.h"

#include "camera.h"
#include "sdfifo.h"
#include "buzzer.h"
#include "voice.h"
#include "driverdb.h"
#include "Systemmonitor.h"
#include "gpsModule.h"
#include "rtc.h"
#include "io_manager.h"

#include "GPRS.h"
#include "lgb.h"

// timout in tick os.
#define TCPIP_TMO			20000
#define TCPIP_SEND_RETRY	3
#define RETRY_SEC_SERVER		2
#define RETRY_PRI_SERVER		2
#define IP1_TRY_CONNECT_MAX		3
#define IP2_TRY_CONNECT_MAX		6
#define TRY_CONNECT_MAX			10

#define ITEMS_WAIT_IN_BUFFER	30

/**
 * ------------ TYPE DEFINE ----------------------------------
 */
typedef void (*fnProcessMSG_t) (S8* pMsg);
typedef struct _MSG_TYPE_t
{
	U8 *name;
	U8 name_size;
	fnProcessMSG_t fn;
}MSG_TYPE_t;

typedef struct _GPRS_Report_t
{
	U16 ConnNumberFail;
	U16 SentAckRecv;
	U16 SentTotal;
	U16 SentErr;
}GPRS_Report_t;

typedef struct _pkt_info_t
{
	U8 id;
	MSG_Type_e type;
}pkt_info_t;

typedef struct _raw_pkt_t
{
	S8 txBuffer[MAX_SIZE_PACKET];
	U8 type;
	U16 txLen;
}raw_pkt_t;

typedef struct {
	S8 date[15];
	U8  day_number;
}LogHeader_t;



/******** local funtions *************************************/
static void vRecv_Handshake(S8* p);
static void vCheckVersion(S8 *p);
static void vResetCamBuffer(S8 *p);
static void vResetLogBuffer(S8 *p);
static void vSyncDriverInfo(S8 *p);
static void vSendLogRequest(S8 *p); 
static void vEchoSmsRequest(S8 *p);
static void SIMCostHandler(S8 *p);
static void SvrRequestTaxHandler(S8 *p);
static void SvrFowardTaxHandler(S8 *p);
static void SvrQueryConfig(S8* p);
static void SvrDeviceInfo(S8* p);
void sys_log_send_via_server(S8 *p);
void admin_config_via_server(S8*p);
void vSendLogData(S8 *p);

/* extern in camera.c */
void vSDBufferReset(void);
extern void device_getinfo(uint8_t *p);
void SystemMove2Mailbox(void);
extern void dbg_adminConfig(uint8_t* dbg_buf);

/* api */
BOOL blPushData(U8 type, S8 *data, U16 size);
BOOL GPRS_SendIOMessage(void);

/* ack for request set config */
#define blSendAckConfig(data, len)			blPushData(ACK_CFG_SET, data, len)

void GPRS_GetReport(uint8_t* strReport);
void GPRS_ResetReport(void);

// task
__task void vClient_Recv(void);
__task void vClient_Trans(void);


/* Local variables ---------------------------------------------- */
_declare_box(GPRSTxBuf, sizeof(raw_pkt_t), 5);
os_mbx_declare (GPRSTxMbx, 4);

//S8 txPacket[256];
pkt_info_t current_pkt;
//S8 txData[MAX_SIZE_PACKET + 200];

#ifdef USE_GPRS_MODULE
static U64 txTskStack[600/8];
static U64 rxTskStack[800/8];
static U64 ulogStack[700/8];
#else
static U64 txTskStack[100/8];
static U64 rxTskStack[150/8];
static U64 ulogStack[100/8];
#endif


U8 cnntNotResponse = 0;

GPRS_State_t cl_state = GPRS_NONE;
U8 sdCard_st = 0;
GPRS_Report_t GPRSReport;

uint16_t _system_count = 0;		// variable for reset device after 12h connect not ack.

// Watchdog variables and macros
uint32_t wdt_gsm_sending_cnt = 0;
uint32_t wdt_gsm_download_fw_timeout = 0;
uint32_t wdt_client_recv_cnt = 0;
uint32_t wdt_log2buf_cnt = 0;
 

#define WDT_RESET_GSM_COUNT()		{wdt_gsm_sending_cnt = 0;}


/* Global variables ---------------------------------------------- */
extern void vSMS_Handler(S8* pData);
extern void ServerDataHandler(S8* pData);
extern int16_t sys_log_fwrite(uint8_t type, int8_t *logs, int16_t lsize);

/**
 * \brief
 * \notes:
 * 		muon giai ma command nao tu server gui xuong thi add them vao bien nay theo format:
 * 		[Name_compare],[Name_size],[func_pointer]
 */
const MSG_TYPE_t xMsgType[] = {
		{"SMS", 3, &vSMS_Handler},
		{"FWVER", 5, &vCheckVersion},
		{"DOTA", 4, &FWUpgradeDemandHandler},
		{"SYN", 3, &vRecv_Handshake},	
		{"SCFG", 4, &SvrQueryConfig},
		{"CRST", 4, &vResetCamBuffer},
		{"BRST", 4, &vResetLogBuffer},
		{"DDRV", 4, &vSyncDriverInfo},
		{"RLOG", 4, &vSendLogRequest},
		{"ECHO_SMS",8, &vEchoSmsRequest},
		{"SRV", 3, &ServerDataHandler},
		{"CUSD", 4, &SIMCostHandler},
		{"GET_TAX", 7, &SvrRequestTaxHandler},
		{"FWR_TAX", 7, &SvrFowardTaxHandler},
		{"INFO", 4, &SvrDeviceInfo},
		{"LOGS", 4, &sys_log_send_via_server},
		{"ADMIN,", 6, &admin_config_via_server},
};

#define 	MSG_TYPE_COUNT		sizeof(xMsgType)/sizeof(xMsgType[0])

OS_TID xClientTransID;
OS_TID xClientRecvID;
OS_TID tskLog2Buffer;


/********* functions implementation ******************************/
//#define GPRS_DEBUG_
#ifdef GPRS_DEBUG_
char dbg_str[32];
void vGPRS_Error(S8* err)
{
	USART_PrCStr(DBG_Port, err);
}
#else
void vGPRS_Error(S8* err)
{}
#endif


/*
 * \brief	uint8_t gh_ramdom(uint32_t refnuber)
 * 			calculate a number ramdom in 0-7 and return it.
 * \param	refnumber is number for reference.
 * \return	a number in 0-7.
 */
uint8_t gh_ramdom(uint8_t* boxID)
{
	uint8_t hh,min,ss, ret;

	vRTCGetTime(&hh, &min, &ss);
	// random = (^BoxID[i]^ss^min^hh)&0x07;
	ret = boxID[0] ^ boxID[1] ^ boxID[2] ^ boxID[3] ^ boxID[4];
	ret = (ret ^ ss ^ min ^ hh) & 0x07;

	return ret;
}

/**
 *
 */
BOOL GPRS_Connect(void)
{
	U16 retry;

#define OVER_CONNECT_ERROR		5

	os_dly_wait(200/OS_TICK_RATE_MS);
	/* setup profile for tcpip
	 * recv_data: +IPD,data_len:data
	 * try 5 times. if not success then return fail.
	 */
	for(retry=5;retry;retry--)
	{
		if(blSetTCPIPProfiles())
			break;
	}
	if(retry == 0)
		return 0;

	/*
	 * Thu connect 5 lan den server 1, neu khong co ack tu server khi gui ban tin
	 * tien hanh connect den server 2, neu sau 5 lan cung khong co ack lai thi
	 * reset sim900.
	 */
	if(cnntNotResponse < IP1_TRY_CONNECT_MAX)	// thu ip1
	{
		// tang so lan thu connect len 1.
		cnntNotResponse ++;

		// retry 2 lan cho 1 lan connect
		for(retry = RETRY_PRI_SERVER;retry;retry--)
		{
			if(blTCPIP_Open((const S8*)Config.IP1, Config.Port1, 300))
			{
				return 1;

			}else{
				vTCPIP_Shutdown();
				// delay 500ms then reconnect.
				os_dly_wait(500/OS_TICK_RATE_MS);
			}
		}

		/* next ip2 */
		cnntNotResponse = IP1_TRY_CONNECT_MAX;
	}


	if(cnntNotResponse < IP2_TRY_CONNECT_MAX)
	{
		// tang so lan connect len 1.
		cnntNotResponse ++;

		// secondary server. thu 2 lan
		for(retry = RETRY_SEC_SERVER;retry;retry--)
		{
			if(blTCPIP_Open((const S8*)Config.IP2, Config.Port2, 300))
			{
				return 1;

			}else{
				vTCPIP_Shutdown();
				// delay 500ms then reconnect.
				os_dly_wait(500/OS_TICK_RATE_MS);
			}
		}
	}

	// fail.
	cnntNotResponse = 0;

	return 0;
}

void vGPRS_Init(void)
{
	vGPRS_Error("\r\nGPRS SERVICES...\r\n");
	// init memory.
	_init_box(GPRSTxBuf, sizeof(GPRSTxBuf), sizeof(raw_pkt_t));
	os_mbx_init(GPRSTxMbx, sizeof(GPRSTxMbx));

	// send system report to mailbox
	SystemMove2Mailbox();

	current_pkt.id = 0;
	current_pkt.type = RAW_DATA;

	xClientRecvID = os_tsk_create_user(vClient_Recv, RTX_GPRS_RXTSK_PRIORITY, &rxTskStack, sizeof(rxTskStack));
	xClientTransID = os_tsk_create_user(vClient_Trans, RTX_GPRS_TXTSK_PRIORITY, &txTskStack, sizeof(txTskStack));

}

/*
 * \brief 	loger_messages(uint8_t mesg, int16_t size)
 *					validate message which must logged.
 * \param		string message.
 * \param		size of message.
 * \return  none.
 */
void loger_messages(uint8_t *mesg, int16_t size)
{
	// abort write log with unexpect messages.
	if((strncmp((char*)mesg, "SYN", 3)!=0) &&	// synchronouse.
		 (strncmp((char*)mesg, "LOGS", 4)!=0) && // readlog
		 (strncmp((char*)mesg, "SRV", 3)!=0) && // download package
		 (strncmp((char*)mesg, "CUSD", 4)!=0)&& 	// unsolicited code.
		 (strncmp((char*)mesg, "GET_TAX", 7)!=0)&& 	// GET tax.
		 (strncmp((char*)mesg, "SMS", 3)!=0)) 	// SMS.
	{
		sys_log_fwrite(2, (int8_t*)mesg, size);
	}
}


/**
 * \brief 	Decode message received from server.
 * \param	pointer point data section.
 * \return 	none.
 */
void vDecodeGPRS_Message(S8 *pParam)
{
	S8 *uItem[7];
	U16 n, j, i;

	vGPRS_Error("\r\nRECV FROM SERVER\r\n");
	vGPRS_Error(pParam);

	n = string_split((U8**)uItem, (U8*)pParam, "#$", 7);

	for(j=0;j<n;j++)
	{
		// write log on sdcard.
		loger_messages(uItem[j], strlen(uItem[j]));
		// parser messages
		for(i=0;i<MSG_TYPE_COUNT;i++)
		{
			if(strncmp((char*)uItem[j], (char*)xMsgType[i].name, xMsgType[i].name_size)==0)
			{
				if(xMsgType[i].fn != NULL)
					xMsgType[i].fn(uItem[j] + xMsgType[i].name_size);

				// delay wait. pass to others task.
				wdt_client_recv_cnt = 0;
				os_dly_wait(1);

				break;
			}
		}
	}
}

/************ decode ack *****************************/
/*
 * read data in log file and send it to server.
 * format:
 * 		$RLOG,ddmmyy,day_number#
 */

BOOL blNextDay(char *startDay)
{
	char dd, mm, yy;
	char *p = startDay;

	dd = *p++ - '0';
	dd = (dd*10) + (*p++ - '0');

	mm = *p++ - '0';
	mm = (mm*10) + (*p++ - '0');

	yy = *p++ - '0';
	yy = (yy*10) + (*p++ - '0');

	/* neu qua thang */
	if(isdateover(dd, mm, yy) == 1)
	{
		dd = 1;
		mm ++;
		if((mm > 12)){
			mm = 1;
			yy ++;
		}
	}else{
		dd ++;	/* next day */
	}

	p = startDay;
	sprintf(startDay, "%02u%02u%02u", dd, mm, yy);

	return 1;
}

void vWriteLogToBuffer(LogInfo_t xlog, Data_frame_t *rw)
{
	S8 *p;

	int id;
	static float x, y;
	static char running = 0;

#define DIV_DELTA (float)1000000.00

	p = (char*)&xlog;


	if(((xlog.tm.sec >= 30)&&(rw->tm.sec == 0)) || ((xlog.tm.sec < 30)&&(rw->tm.sec == 30)))
	{
		rw->io = xlog.io;
		// send to buffer. if raw_data size is changed, must changed size in msg_buffer.c */
		if((rw->checksum == 0x55) /*&& (running > 14)*/){
			//rw->io.EngineStatus = 1;

			sdfifo_write((uint8_t*)rw);
		}

		// reset run state
		running = 0;


		memcpy(rw->BoxId, Config.BoxID, 8);
		memcpy(rw->drv_key, xlog.id_drv, sizeof(xlog.id_drv));

		rw->adc1 = 0;	rw->adc2 = 0;	rw->adc3 = 0;
		rw->alt = 0;
		if((xlog.c_door - rw->door_numb)/2 == 0) rw->io.DoorStatus = 0;
		else									 rw->io.DoorStatus = 1;
		rw->door_numb = xlog.c_door;

		rw->off_numb = xlog.c_off;
		rw->stop_numb = xlog.c_stop;

		rw->gps_speed = xlog.CurSpeed/10;
		rw->heating = 0;
		rw->lat = xlog.lat;
		rw->lng = xlog.lng;
		rw->over_speed = xlog.over_speed;
		rw->pulse1 = xlog.pulse;
		rw->pulse2 = 0;
		rw->tm = xlog.tm;
		if(rw->tm.sec >= 30) rw->tm.sec = 30;
		else				rw->tm.sec = 0;

		rw->total_pulse1 = 0;
		rw->total_pulse2 = 0;
		rw->total_time = xlog.total_time/60;
		rw->cont_time = xlog.cont_time/60;
		rw->trackAngle = 0;
		rw->checksum = 0x55;
	}
	else
	{
		id = xlog.tm.sec%30;
		if(id > 0) id -= 1;

		if(diff_dist_m(x, y, xlog.lat, xlog.lng) < 50)
		{
			// sai so 29 vi tri tiep theo.
			rw->delta_lat[id] = (S16)(((double)(xlog.lat - x)*DIV_DELTA));
			rw->delta_lng[id] = (S16)(((double)(xlog.lng - y)*DIV_DELTA));
			rw->gps_speed_arr[id] = (U8)(xlog.CurSpeed/10);
			if(rw->gps_speed_arr[id]  > 120) rw->gps_speed_arr[id] = rw->gps_speed_arr[id]/10;

			if(xlog.CurSpeed > 20) running ++;
		}
		else	/* if gps error, fake */
		{
			rw->delta_lat[id] = (id > 0) ? rw->delta_lat[id-1] : 0;
			rw->delta_lng[id] = (id > 0) ? rw->delta_lng[id-1] : 0;
			rw->gps_speed_arr[id] = (id > 0) ? rw->gps_speed_arr[id-1] : rw->gps_speed;
		}
	}

	x = xlog.lat;
	y = xlog.lng;

}

__task void vLog2Buffer(void *avrg)
{
	FILE *flog;
	LogInfo_t *tlog;
	Data_frame_t *rw;

	LogHeader_t rLogHeader;
	char *p = (char*)avrg;
	char startDay[8];

	p++; // pass ','
	tlog = (LogInfo_t*)calloc(1, sizeof(LogInfo_t));
	rw = (Data_frame_t*)calloc(1, sizeof(Data_frame_t));

	if((tlog == NULL) || (rw == NULL))
	{

		//Clear wdt cnt again
		wdt_log2buf_cnt = 0;

		tskLog2Buffer = NULL;
		os_tsk_delete_self();
	}

	/* Get start date */
	memcpy(startDay, p, 6);

	/* Get number of day */
	p += 6;
	if(*p != ','){	
		tskLog2Buffer = NULL;
		free(avrg);
		os_tsk_delete_self();
	}

	p ++;
	rLogHeader.day_number = atoi(p);

	rw->checksum = 0;

	while(rLogHeader.day_number > 0){
		/* Get log file name */
		strcpy(rLogHeader.date, "log\\000000.log");
		memcpy(rLogHeader.date + 4, startDay, 6);

		/* Open given log file */
		flog = fopen(rLogHeader.date, "rb");
		if(flog != NULL)
		{
			while(!feof(flog))
			{
				fread(tlog, sizeof(LogInfo_t), 1, flog);
				vWriteLogToBuffer(*tlog, rw);

				wdt_log2buf_cnt = 0;
				os_dly_wait(3);
			}
			fclose(flog);
		}

		blNextDay(startDay);
		rLogHeader.day_number--;
	}

	free(tlog);
	free(rw);
	free(avrg);
	
	//Clear wdt cnt again
	wdt_log2buf_cnt = 0;
	
	tskLog2Buffer = NULL;
	os_tsk_delete_self();
}

void admin_config_via_server(S8*p)
{
	int8_t ack[12];
	dbg_adminConfig(p);
	// ack to server.
	strncpy(ack, Config.BoxID, 5);
	/* put ack to mailbox */
	blPushData(ACK_CFG_SET, ack, 5);
}

/**/
void sys_log_send_via_server(S8 *p)
{
	int8_t *buf;
	int16_t lsize;
	
	buf = (int8_t*)malloc(768);
	if(buf == NULL)
		return;
	// boxID.
	memcpy(buf, Config.BoxID, 5);
	// logs
	lsize = sys_log_fread(buf + 5, 760);
	if(lsize > 0){
		blPushData(LOGS_MESSAGE_TYPE, buf, lsize + 5);
	}
	free(buf);	
}

/*
 * \brief	static void SvrDeviceInfo(S8* p)
 * 			send device info to server.
 * \param	p
 * \return 	none.
 */
static void SvrDeviceInfo(S8* p)
{
	uint8_t *buf;
	
	buf = calloc(256, sizeof(char));
	if(buf == NULL) return;
	
	strncpy(buf, Config.BoxID, 5);

	device_getinfo(buf + 5);
	/* put ack to mailbox */
	blPushData(DEVICE_STATUS_INFO, buf, strlen(buf));
	
	free(buf);
}

/*
 * \brief
 *
 */
static void SvrQueryConfig(S8* p)
{
	// P = ,GET,ALL. or ,SET...
	uint8_t *pOut;
	uint8_t *txData;
	
	txData = calloc(1024, sizeof(char));
	if(txData == NULL) return;
	
	if(strcmp(p, ",GET,ALL") == 0)
	{
		uint16_t len;
		
		// read all config
		pOut = txData;
		config_device_gets(pOut, 1024);
		strcat(pOut, ",");
		// vehicle info
		len = strlen(txData);
		pOut = txData + len;
		config_vehicle_gets(pOut, 160);
		strcat(pOut, ",");

		// driver info
		len = strlen(txData);
		pOut = txData + len;
		config_driver_gets(pOut, 160);
		strcat(pOut, ",");

		// ext info
		len = strlen(txData);
		pOut = txData + len;
		config_ext_gets(pOut, 160);

		// extend for server. driver key plug in status.
		strcat(pOut, ",");
		pOut[strlen(pOut)] = getDriverPlugIn();
		
		// admin user.
		strcat(pOut, ",");
		strcat(pOut, Config.admin.user);
		
		// admin password.
		strcat(pOut, ",");
		strcat(pOut, Config.admin.pass);
		
		len = strlen(txData);

		/* put ack to mailbox */
		blPushData(READ_ALL_CONFIG, txData, len);
	
	}
	else if(strncmp(p, ",SET", 4)==0)
	{
		// validate data. verify in task debug.c

		// set config.
		dbg_setConfigParams(p);

		// ack to server.
		strncpy(txData, Config.BoxID, 5);
		/* put ack to mailbox */
		blPushData(ACK_CFG_SET, txData, 5);
	}
	
	free(txData);
}

/* Parse $RLOG,181012,3# */
void vSendLogData(S8 *p)
{
S8 *uItem[3];
S8 *argv;
U16 n;

	// p = ,ddmmyy,day_number. 111012,2
/* validate length of MSG */
	n = strlen(p);
	if(n >= 32)
		return;
	
/* Parse to argument */
	argv = calloc(1, 32* sizeof(S8));
	if(argv == NULL){
		return;
	}

	memcpy(argv, p, n);	

/* Validate data: 2 elements */
	n = string_split((U8**)uItem, (U8*)p, ",", 2);
	if((n != 2) || (strlen(uItem[0]) != 6) || (*uItem[1] < '0') || (*uItem[1] > '9') || (tskLog2Buffer != NULL)){
		free(argv);
		return;
	}

	/* Create task */
	tskLog2Buffer = os_tsk_create_user_ex (vLog2Buffer, 1, &ulogStack, sizeof(ulogStack), argv);
}

static void vSendLogRequest(S8 *p)
{
	uint8_t *txPacket;
	
	txPacket = malloc(256);
	if(txPacket == NULL) return;
	
	vSendLogData(p);

	memcpy(txPacket, Config.BoxID, 5);
	txPacket[5] = 0;
	txPacket[6] = 0;

	/* put ack to mailbox */
	blPushData(ACK_CFG_SET, txPacket, 7);
	
	free(txPacket);
}

/*
 * \brief	static void SvrRequestTax1Handler(S8 *p)
 * 			server request device get tax
 * 			$GET_TAX,TAX1#
 * \param	pointer p point to ",TAX1"...
 * \return 	none.
 */
static void SvrRequestTaxHandler(S8 *p)
{
	if(strcmp(p, ",TAX1") == 0)
	{
		GSM_blSendCommand("at+cusd=1,\"*101#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_GPRS;
		HostChannel.pData = (S8*)calloc(1, sizeof(S8));
		*HostChannel.pData = '1';
		HostChannel.state = CH_STATE_BUSY;
		return;
	}

	if(strcmp(p, ",TAX2") == 0)
	{
		GSM_blSendCommand("at+cusd=1,\"*102#\"\r", "\r\nOK\r\n", 50);
		HostChannel.type = CH_GPRS;
		HostChannel.pData = (S8*)calloc(1, sizeof(S8));
		*HostChannel.pData = '2';
		HostChannel.state = CH_STATE_BUSY;
		return;
	}
}

/*
 * \brief	static void SvrRequestTax1Handler(S8 *p)
 * 			server request device get tax and send it via messages to a phone number.
 * 			$FWR_TAX,TAX1#
 * \param	TAX1
 */
static void SvrFowardTaxHandler(S8 *p)
{
	uint8_t *s[3], n;

	n = string_split((U8**)s, (U8*)p, ",", 3);
	if(n<2)
		return;

	if(strcmp(s[0], "TAX1") == 0)
	{
		GSM_blSendCommand("at+cusd=1,\"*101#\"\r", "\r\nOK\r\n", 50);
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, s[1]);
		HostChannel.type = CH_SMS;
		HostChannel.state = CH_STATE_BUSY;
		return;
	}

	if(strcmp(s[0], "TAX2") == 0)
	{
		GSM_blSendCommand("at+cusd=1,\"*102#\"\r", "\r\nOK\r\n", 50);
		HostChannel.phost = (S8*)calloc(20, sizeof(S8));
		strcpy(HostChannel.phost, s[1]);
		HostChannel.type = CH_SMS;
		HostChannel.state = CH_STATE_BUSY;
		return;
	}
}

/*
 *
 */
static void SIMCostHandler(S8 *p)
{
	uint8_t *s[3], n;

	n = string_split((U8**)s, (U8*)p, "\"", 3);
	if(n<2)
		strcpy(s[1], "Invalid MMI code");

	switch(HostChannel.type)
	{
	case CH_COM:
		USART_PrCStr(DBG_Port, s[1]);
		break;
	case CH_GPRS:
		s[0] = (S8*)calloc(160, sizeof(S8));
		if(s[0] == NULL)
			return;

		memcpy(s[0], Config.BoxID, 5);
		strncpy(s[0] + 5, HostChannel.pData, 1);
		strcat(s[0], s[1]);

		n = strlen(s[0]);
		/* put to mail box */
		blPushData(EXT_DEVICE_TAX, s[0], n);

		free(HostChannel.pData);
		free(s[0]);
		break;
	case CH_SMS:
		/* send sms */
		blSMS_Puts(HostChannel.phost, s[1]);
		free(HostChannel.phost);
		break;
	}
	HostChannel.type = CH_NULL;
	HostChannel.state = CH_STATE_IDLE;
}


/*
 * send a SMS to assigned phone number
 * format: $ECHO_SMS,phone,content#
 */
static void vEchoSmsRequest(S8 *p)
{
	S8 *pPhone,*pContent;
	U8 i = 0;

// decode message.
	p ++; //Bo qua dau phay sau ECHO_SMS
	pPhone = p;
	while((*p != ',') && (*p != 0) )
	{
		p++;
		if(i++>200)
			return;//Fail
	}

	*p-- = 0;
	pContent = p+2;
	i = 0;
	while((*p != '#') && (*p != 0) )
	{
		p++;
		if(i ++>200)
			return; //Fail
	}

	*p-- = 0;

	blSMS_Puts(pPhone,pContent);
}

/*
 * response check version
 */
static void vCheckVersion(S8 *p)
{
	U16 len;
	uint8_t *txPacket;
	
	txPacket = calloc(32, sizeof(char));
	if(txPacket == NULL) return;
	/* request version */
	if(*p == '?')
	{
		memset(txPacket, 0, 32);
		memcpy(txPacket, Config.BoxID, 5);
		strcat(txPacket, HW_VERSION_STR);
		strcat(txPacket, ",");
		strcat(txPacket, FW_VERSION_STR);

		len = strlen(txPacket);
		/* put to mail box */
		blPushData(QUERY_VERSION, txPacket, len);
	}
	
	free(txPacket);
}

/* reset all index for camera memory box */
static void vResetCamBuffer(S8 *p)
{
	vSDBufferReset();
}

static void vResetLogBuffer(S8 *p)
{
	sdfifo_reset();
}


int8_t driverIDValidate(uint8_t *driver_id)
{
	uint8_t i;
	for(i=0;i<8;i++)
	{
		if((*driver_id < '0') || ((*driver_id > '9') && (*driver_id < 'A')) || (*driver_id > 'Z'))
			return -1;
	}
	return 0;
}

/* driver info synchronous */
/*
 * $DDRV,*DRV_Count*,*DRV_Total*,ID=xxxxxxxx,NAME=Nguyen Van A,LIC=A123456789,ISSUED=12/12/2011,EXPD=12/12/2016,*check_sum*#
 * p = ,*DRV_Count*,*DRV_Total*,ID=xxxxxxxx,NAME=Nguyen Van A,LIC=A123456789,ISSUED=12/12/2011,EXPD=12/12/2016,*check_sum*
 */
static void vSyncDriverInfo(S8 *p)
{
	S8* drv_p[8];
	int n;
	char id, total;
	static char n_id = 0;
	driverdb_t pDrvInfo;

	S8* pBuf;

	/* create temp database if count = 1*/
	n = string_split(drv_p, p, ",", 8);
	if(n == 8)
	{
		id = *drv_p[0] - '0';
		total = *drv_p[1] - '0';
		//chk = drv_p[7];
		if((id <= n_id) || (n_id == total)) goto send_ack;

		if(id == 1)
		{
			/* create temp database */
			if(eDB_Create()!=DRV_DB_OK){
				return;
			}
		}

		if(('I'==drv_p[2][0]) && ('D'==drv_p[2][1]))
		{
			/* driver id */
			if(driverIDValidate(drv_p[2] + 3)!= 0){
				return;
			}

			strcpy(pDrvInfo.ID, drv_p[2]+3);
		}
		if(('N'==drv_p[3][0]) && ('A'==drv_p[3][1])&&('M'==drv_p[3][2]) && ('E'==drv_p[3][3]))
		{
			/* driver name */
			strcpy(pDrvInfo.name, drv_p[3]+5);
		}
		if(('L'==drv_p[4][0]) && ('I'==drv_p[4][1])&&('C'==drv_p[4][2]))
		{
			/* driver lic_number */
			strcpy(pDrvInfo.lic_number, drv_p[4]+4);
		}
		if(('I'==drv_p[5][0]) && ('S'==drv_p[5][1])&&('S'==drv_p[5][2]) && ('U'==drv_p[5][3]))
		{
			/* driver name */
			strcpy(pDrvInfo.issueDate, drv_p[5]+7);
		}
		if(('E'==drv_p[6][0]) && ('X'==drv_p[6][1])&&('P'==drv_p[6][2]) && ('D'==drv_p[6][3]))
		{
			/* driver name */
			strcpy(pDrvInfo.expiryDate, drv_p[6]+5);
		}

		if(eDB_WriteNewInfo(&pDrvInfo) != DRV_DB_OK)
		{
			return;
		}

		/* next id */
		n_id = id;

send_ack:
#define DRV_ACK_LEN 		7
		pBuf = (S8*)calloc(12, sizeof(char));
		if(pBuf == NULL)
			return;
		/* $DDRV,*DRV_Count*,*DRV_Total*,*check_sum*# */
		memcpy(pBuf, Config.BoxID, 5);
		pBuf[5] = id;
		pBuf[6] = total;

		/* put ack to mailbox */
		blPushData(ACK_DRV_SYN, pBuf, DRV_ACK_LEN);
		
		free(pBuf);
	}
}

/*
 * \brief	SystemMove2Mailbox(void)
 * 			Send system report to mailbox
 * 	\param	none.
 * 	\return	none.
 */
void SystemMove2Mailbox(void)
{
#define SYSTEM_PKG_LEN		sizeof(SystemData_t) + 10
	S8* pBuf;
	uGPSDateTime_t time;
	SystemData_t *sys;
	ResultType_e res;

	pBuf = (S8*)calloc(SYSTEM_PKG_LEN + 2, sizeof(char));
	if(pBuf == NULL)
		return;

	sys = (SystemData_t*)(pBuf + 10);

	// mesg: boxid time data_status reset_source reset_count lsiCnt ...
	//		  5		4	 	1			4			2		   1				bytes

	// copy boxid
	memcpy(pBuf, Config.BoxID, 5);
	// get time
	vUpdateTime(&time);
	// copy to buffer
	memcpy(pBuf + 5, (S8*)&time, 4);

	res = SystemGetInfoInFile(sys);
	// check system file status.
	if(res == DATA_CHECKSUM_FAIL)
	{
		res = SystemGetInfoInFile(sys);
	}

	if(res == RESULT_OK)
		pBuf[9] = 0x01;
	else
		pBuf[9] = 0x00;

	/* put data to mailbox */
	blPushData(SYSTEM_REPORT_TYPE, pBuf, SYSTEM_PKG_LEN);
	free(pBuf);
}

/*********** send ****************************/
/*
 * send a packet to server. This function send data over sim900.
 */
BOOL blSendPacketOverGPRS(U8 type, S8 *data, U16 size)
{
	S8 *p, *buf;
	U16 i;
	U8 checksum = 0;
	uint8_t result;

	if((data == NULL) || (size >= MAX_SIZE_PACKET))
		return 0;

	/* checksum */
	checksum = cCheckSumFixSize(data, size) ^ current_pkt.id ^ type ^ 0;

//[1u-'$'][1u-ID][1u-MsgType][1u-Dummy][Vu-Data][1u-Checksum][1u-'#']
	//p = txData;
	p = malloc(size + size + 3);		// size = size*2.
	if(p == NULL) return 0;
	
	buf = p;
	/* start */
	*p++ = '$';
	/* id of message */
	if((current_pkt.id == '#') || (current_pkt.id == 27) || (current_pkt.id == '$')){
		*p++ = 27;
	}
	
	*p++ = current_pkt.id;
	
	/* type of message */
	if((type == '#') || (type== 27) || (type == '$')){
		*p++ = 27;
	}
	*p++ = type;
	current_pkt.type = (MSG_Type_e)type;	
	/* dummy */
	*p++ = 0;

	/* copy data*/
	for(i=0;i<size;i++)
	{
		/* insert escape */
		if((*data == '#') || (*data == 27) || (*data == '$'))
			*p++ = 27;

		*p++ = *data++;
	}

	/* send checksum */
	if((checksum == '#') || (checksum== 27) || (checksum == '$'))
		*p++ = 27;
	*p++ = checksum;

	*p++ = '#';
	size = (p - buf);
	/* send data */
	result = blTCPIP_Write(buf, size, 1);	
	// release memory.
	free(buf);
	
	return result;
}


/*
 * This function send data to mailbox.
 */
BOOL blPushData(U8 type, S8 *data, U16 size)
{
	raw_pkt_t *pkt;

	// check size packet.
	if(size > MAX_SIZE_PACKET-1)
		return 0;

	/* alloc memory */
	pkt = (raw_pkt_t*)_alloc_box(GPRSTxBuf);
	if((pkt == NULL))
		return 0;

	/* modify data for handshake */
	pkt->type = type;
	/* copy to buffer */
	memcpy(pkt->txBuffer, data, size);
	pkt->txLen = size;

	/* Put data into mailbox */
	if (os_mbx_check (GPRSTxMbx) > 0)
	{
		if(os_mbx_send(GPRSTxMbx, pkt, 100)!= OS_R_TMO)
			return 1;
	}
	else
	{
		_free_box(GPRSTxBuf, pkt);
	}

	return 0;
}

/** APPLICATIONS ******************************************************/
/* send LOG */
BOOL blSend_RawData(void)
{
	int8_t res = 1;
	uint8_t send_timeout;
	uint32_t items;
	uint8_t *txData;

	txData = malloc(MAX_SIZE_PACKET);
	if(txData == NULL) return 0;
	
	res=sdfifo_read((uint8_t*)txData);

	/* data valid */
	if((res == 0))
	{
		// get a random number for wait send to mailbox. down performance server purpose.
		// if items in buffer is large, dont wait.
		items = sdfifo_getItem();
		if(items < ITEMS_WAIT_IN_BUFFER){
			send_timeout = (Config.option.usedDelaySend) ? gh_ramdom(Config.BoxID) : 0;
//
//			sprintf(txData, "\r\nWait %u..\x0", send_timeout);
//			USART_PrCStr(USART1, txData);

			if(send_timeout > 0){
				os_dly_wait(send_timeout * 1000);		// delay in second.
			}
		}

		/* modify data for handshake. shift boxid sang phai 3bytes */
		memcpy(txData+3, Config.BoxID, 5);

		/* chuyen tinh checksum vao phan truyen du lieu. bo di byte cuoi la byte checksum */
		res = (int8_t)blPushData(RAW_DATA, txData+3, sizeof(Data_frame_t)-4);
	}
	/* error sdcard */
	else if((res == -4)||(res == -3))
	{
		/* send error code = 1 sdcard error */
		sdCard_st = 1;
		/* abort message fail */
		sdfifo_readNext();
	}
	else if(res == -2){
		/* send error code = 1 sdcard error */
		sdCard_st = 1;
	}

	free(txData);
	return res;
}

/* camera send */
BOOL blSendCameraPacket(void)
{
	int8_t res = 1;
	U16 size;
	uint8_t *txData;

	txData = malloc(MAX_SIZE_PACKET);
	if(txData == NULL) return 0;

	res=blReadImagePacket((uint8_t*)txData);
	/* data valid */
	if((res == 0))
	{
		size = (txData[1]&0xFF);
		size = (size<<8) + (txData[0]&0xFF);

		/* end modify */
		if(size >= (MAX_SIZE_PACKET))
		{
			/* assige to default */
			size = 782;
		}

//		sprintf(txData, "\r\nI:%u\x0", size);
//		USART_PrCStr(USART1, txData);
		res = (int8_t)blPushData(CAM_DATA, txData+2, size);
	}
	else if(res != -1){
		sdCard_st = 1;
		/* abort message fail */
		vNextImagePacket();
	}
	
	free(txData);
	return res;
}

BOOL GPRS_SendIOMessage(void)
{
	int8_t res = 1;
	U16 size;
	uint8_t *txData;

	if(!Config.option.usedIOMessage)
		return 1;
	
	txData = malloc(sizeof(io_message_t)+2);
	if(txData == NULL) return 0;
	
	memcpy(txData, Config.BoxID, 5);

	res=io_read_from_sdcard((txData + 5), &size);
	/* data valid */
	if((res == 0))
	{
		res = (int8_t)blPushData(IO_MESSAGE_TYPE, txData, size + 5);
	}
	else if(res != -1){
		/* abort message fail */
		io_release_in_sdcard();
	}
	
	free(txData);
	
	return res;
}

/* error code */
BOOL blSendError(Err_code_e error, uint16_t total)
{
	unsigned char err[10];
	memcpy(err, Config.BoxID, 5);
	err[5] = (unsigned char)error;
	memcpy(err+6, (int8_t*)&total, 2);
	
	return blPushData(ERROR_CODE, err, 8);
}

/************ TASK *************************************************/

/**
 * task decode message from server.
 */
__task void vClient_Recv(void)
{
	S8* ptr;
	for(;;)
	{
		// wait forever data.
		if(blTCPIP_Gets(&ptr, 0x0100) == 1)
		{
			// decode message.
			vDecodeGPRS_Message(ptr);

			// release memory
			vTCPIP_FreeRxBuf(ptr);
		}

		// reset watchdog.
		wdt_client_recv_cnt = 0;
	}
}


/**
 * task send a packet to server.
 */
__task void vClient_Trans(void)
{
uint8_t gprs_pkg_retry_count = 0;
uint8_t gprsRetry = 3, i;
uint8_t gprsInitRetry = 3;
raw_pkt_t *pkt = NULL;

	/* Init GSM Mutex */
	GSM_MutexInit();

	for(;;)
	{
		/* @Todo: reset counter monitoring everytime in sending state: At least 30 sec */
		wdt_gsm_sending_cnt = 0;
		/* Wait while SMS in SENDING state */
		if(xGSMflag.SMS_flag == SMS_SENDING){	//Danh cho nhung con SIM 900 co ma Z092E
			os_dly_wait(10);
			continue;
		};
		
		/* Wait in downloading state */
		if(FWUpgradeIsRunning())
		{
			cl_state = GPRS_OPEN;

			/* Send download signal to DownloadFW Task */
			vStartDownload();

			/* Wait until download firmware finish */
			os_evt_wait_and(RTX_DWL_FINISH_FLAG, 0xFFFF);
			// reset data pointer. fix bug handshake device & server after upgrade fail.
			_free_box (GPRSTxBuf, pkt);
			pkt = NULL;

			continue;
		}

		/* Send data */
		switch(cl_state)
		{
		case GPRS_NONE:
			PlayVoiceFile(ALARM_LOST_GSM_CONNECT);

			// open sim900. this function in file gsm.c.
			if(blGSM_Init())
			{
				// connect to gprs.
				for(i=0; i < gprsInitRetry; i++)
				{
					/* @Todo: reset counter monitoring everytime in sending state: At least 30 sec */
					wdt_gsm_sending_cnt = 0;
					/* set apn and wait gpsr ready */
					if(blGPRS_Open((S8*)Config.apn_name, (S8*)Config.apn_user, (S8*)Config.apn_pass, 200)!=0)
					{
						cl_state = GPRS_OPEN;
						/* reset gprsRetry variable */
						gprsInitRetry = 3;
						PlayVoiceFile(ALARM_GSM_CONNECTED);

						break;
					}
					/* status pin and sim ready? */
					if(!SIM900_Status() || (blSIMCARD_Status()!=2))
					{
						int8_t log[24];
						sprintf(log, "ERR90:%u,%u\x0", SIM900_Status(), blSIMCARD_Status());
						sys_log_fwrite(4, log, strlen(log));
						/* sim com init fail */
						os_dly_wait(10000/OS_TICK_RATE_MS);		// 10000 ms.
						break;
					}
				}
				// so lan thu lai tang len de keo dai thoi gian tre
				gprsInitRetry *= 2;
				/* 1.5h gprs aren't ready. reset variable */
				if(gprsInitRetry >= 48)	gprsInitRetry = 3;
			}
			else
			{
				int8_t log[24];
				sprintf(log, "ERR90:%u,%u\x0", SIM900_Status(), blSIMCARD_Status());
				sys_log_fwrite(4, log, strlen(log));
				/* sim com init fail */
				os_dly_wait(10000/OS_TICK_RATE_MS);		// 10000 ms.
			}

			break;
		// open connection
		case GPRS_OPEN:
			/* status pin and sim ready? */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2))
			{
				int8_t log[24];
				sprintf(log, "ERR90:%u,%u\x0", SIM900_Status(), blSIMCARD_Status());
				sys_log_fwrite(4, log, strlen(log));
				cl_state = GPRS_NONE;
				break;
			}

			/* connect */
			if(GPRS_Connect() == 0)
			{
				GPRSReport.ConnNumberFail ++;
				cl_state = GPRS_NONE;
				os_dly_wait(6000/OS_TICK_RATE_MS);		// 10000 ms.
			}
			else // open connect ok.
			{
				/* Reset retry count */
				gprs_pkg_retry_count = 0;
				gprsRetry = 3;

				if(pkt == NULL)
					cl_state = GPRS_READ_BUFFER;						/* gprs ready for trans/recv data */
				else
					cl_state = GPRS_SEND_DATA;
			}
			break;

		case GPRS_READ_BUFFER:

			/* Get data from mailbox and send data to server: 10ms */
			if(os_mbx_wait(GPRSTxMbx, (void**)&pkt, 10)!=OS_R_TMO)
			{
				/* move to send data state */
				gprs_pkg_retry_count = 0;
				gprsRetry = 3;
				cl_state = GPRS_SEND_DATA;
				break;
			}
			else
			{
				/* Timeout then put next data to mailbox */
				if(blSend_RawData()!= 1)
				{
					if(Config.CAM.cam_used)
					{
						/* send image packet to server */
						if(blSendCameraPacket()!=1){
							GPRS_SendIOMessage();
						}
					}else{
						GPRS_SendIOMessage();
					}
				}
			}
			break;

		// connect success
		case GPRS_SEND_DATA:
			GPRSReport.SentTotal ++;

			/* status pin and sim ready? */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2))
			{
				int8_t log[24];
				sprintf(log, "ERR90:%u,%u\x0", SIM900_Status(), blSIMCARD_Status());
				sys_log_fwrite(4, log, strlen(log));
				
				GPRSReport.SentErr ++;
				cl_state = GPRS_NONE;
				break;
			}
			/* check status gprs */
			if((uiTCPIP_Status()==0) || (gprsRetry == 0) || (gprsRetry > 3))
			{
				GPRSReport.SentErr ++;
				// shutdown current connect.
				vTCPIP_Shutdown();
				// reconnect.
				cl_state = GPRS_OPEN;
				break;
			}
			/* Send data */
			if(blSendPacketOverGPRS(pkt->type, pkt->txBuffer, pkt->txLen)==0)
			{
				GPRSReport.SentErr ++;
				gprsRetry--;
				os_dly_wait(500/OS_TICK_RATE_MS);
				break;
			}

			/* Move to wait ACK state */
			cl_state = GPRS_WAIT_ACK;

			break;
		case GPRS_WAIT_ACK:
			/* status pin and sim ready? */
			if(!SIM900_Status() || (blSIMCARD_Status()!=2))
			{
				int8_t log[24];
				sprintf(log, "ERR90:%u,%u\x0", SIM900_Status(), blSIMCARD_Status());
				sys_log_fwrite(4, log, strlen(log));
				
				cl_state = GPRS_NONE;
				break;
			}

			/* check status gprs */
			if(uiTCPIP_Status()==0)
			{
				// shutdown current connect.
				vTCPIP_Shutdown();
				// reconnect.
				cl_state = GPRS_OPEN;
				break;
			}

			/* Wait ACK("SYN") Event: 4 sec */
			if(os_evt_wait_and(RTX_GPRS_ACK_FLAG, 10000/OS_TICK_RATE_MS) == OS_R_TMO)
			{

				gprs_pkg_retry_count++;
				if(gprs_pkg_retry_count > 20){
					// shutdown current connect.
					vTCPIP_Shutdown();
					cl_state = GPRS_OPEN;
					// free data after n times
					_free_box (GPRSTxBuf, pkt);
					pkt = NULL;		// assign to null for new section.
					break;
				}

				/* Change to Send state again */
				cl_state = GPRS_SEND_DATA;
				gprsRetry = 3;

			}
			else {

				GPRSReport.SentAckRecv ++;

				/* free memory box, and read next data */
				current_pkt.id ++;
				_free_box (GPRSTxBuf, pkt);
				pkt = NULL;			// assigned to null for new data.

				cl_state = GPRS_READ_BUFFER;
				// reset system error counter.
				_system_count = 0;
			};

			break;


		default:
			cl_state = GPRS_NONE;
			break;
		}

	}
}


/*
 * ack from server
 * $SYN,id#
 */
static void vRecv_Handshake(S8 *p)
{
	U8 id;
	// p = ",id"
	if(strlen(p) != 3) 
		return;

	/* Process ack only at wait_ack state */
	if(cl_state != GPRS_WAIT_ACK)
	{
		//sprintf(txDebug, "\r\nState: %u", cl_state);
		//USART_PrCStr(DBG_Port, txDebug);
		return;
	}

	/* Validate ID */
	id = twochar2hex(*(p+1), *(p+2));
	if(id != current_pkt.id)
		return;

	switch(current_pkt.type)
	{
	case RAW_DATA:
		sdfifo_readNext();
		break;
	case SYN_DATA:
		break;
	case QUERY_VERSION:
		break;
	case CAM_DATA:
		vNextImagePacket();
		break;
	case IO_MESSAGE_TYPE:
		if(Config.option.usedIOMessage)
			io_release_in_sdcard();
		break;
	default:
		break;
	}

	/* reset connect indicate not response */
	if(cnntNotResponse < IP1_TRY_CONNECT_MAX) cnntNotResponse = 0;
	else if(cnntNotResponse < IP2_TRY_CONNECT_MAX) cnntNotResponse = IP1_TRY_CONNECT_MAX;

	/* remove duplicate index packet */
	os_evt_set(RTX_GPRS_ACK_FLAG, xClientTransID);
}

/* GPRS monitoring service */
/**
 * @brief	Check GSM sevice by increase counter each time it is called
 * @param	none
 * @return	1 if there is some problem
 * 			0 if nothing is happened
 */
uint8_t WDT_Check_GSM_Service(void){

	// check system error
	if((_system_count ++) >= 36000) // 36000 = 8h * 3600s / 0.8s.
	{
		return 1;
	}

	/* Check at Download FW State */
	if(FWUpgradeIsRunning()){
		wdt_gsm_download_fw_timeout++;
		if(wdt_gsm_download_fw_timeout >= WDT_DOWNLOAD_FW_TIMEOUT)
			return 1;

		return 0;
	}
	else wdt_gsm_download_fw_timeout = 0;

	/* Check at sending LOG data state */
	wdt_gsm_sending_cnt++;

	if(wdt_gsm_sending_cnt >= WDT_GSM_MAX_TIMING)
		return 1;

	return 0;
}

uint8_t WDT_Check_ClientRecv_Service(void)
{	
//	if(xClientRecvID != NULL)
	wdt_client_recv_cnt++;
	if(wdt_client_recv_cnt >= WDT_CLIENT_RECV_MAX_TIMING)
		return 1;
	
	return 0;
}

uint8_t WDT_Check_Log2Buf_Service(void)
{		  	
	if (tskLog2Buffer != NULL)
		wdt_log2buf_cnt++;
	else
		wdt_log2buf_cnt = 0;

	if(wdt_log2buf_cnt >= WDT_LOG2BUF_MAX_TIMING)
		return 1;
	
	return 0;
}

/*
 * \brief	GPRS_GetReport(uint8_t* strReport)
 * 			get GPRS info. number of connect fail, send fail...
 * \param	strReport: string store info.
 * \return 	none.
 */
void GPRS_GetReport(uint8_t* strReport)
{
	if(strReport == NULL)
		return;
	sprintf(strReport, "%u.%u.%u.%u",  GPRSReport.ConnNumberFail, GPRSReport.SentTotal, GPRSReport.SentAckRecv, GPRSReport.SentErr);
}

/*
 * \brief	GPRS_ResetReport(void)
 * 			set to zero GPRS info. number of connect fail, send fail...
 * \param	none
 * \return 	none.
 */
void GPRS_ResetReport(void)
{
	GPRSReport.ConnNumberFail = 0;
	GPRSReport.SentAckRecv = 0;
	GPRSReport.SentErr = 0;
	GPRSReport.SentTotal = 0;
}

