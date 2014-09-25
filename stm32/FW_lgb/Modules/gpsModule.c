/*
 * gpsModule.c
 *
 *  Created on: Feb 5, 2012
 *      Author: Van Ha
 *
 *
 *  notes:
 *  thoi gian nhat het 1 message rmc khoang 146ms.
 *  thoi gian toi khi dong bo khoang 20ms.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"
#include "platform_config.h"

#include "serial.h"
#include "utils.h"
#include "rtc.h"

#include "sim18.h"

#include "gpsModule.h"

/**
 * ------------- macro -----------------------------------
 */


#define FLAG_1S				0x0001

//#define GPS_DEBUG


/*********** TYPEDEF ******************************************/
typedef S8 (*vGPSMsgFunc_t)(S8 *);

/*_______local functions _____________________________________*/
void vGPS_Parser(S8 ch);

S8 chGGAParser(S8 *prvParam);
S8 chRMCParser(S8 *prvParam);
S8 chGSAParser(S8 *prvParam);

static S16 iGPS_SyncTime(uGPSDateTime_t tm);

/**______ LOCAL VARIABLES ______________________________________________*/
S8 GPS_Message[228];
static U64 gpsStack[600/8];

U8 requestSyncTime = 0;

uint32_t wdt_gps_rx_cnt = 0;

const S8* sGPS_MSG_Table[] =
{
	"$GPRMC",
	"$GPGGA",
	//"$GPGSA"
};

vGPSMsgFunc_t vGPS_funcs[] =
{
	chRMCParser,
	chGGAParser,
	//chGSAParser
};

typedef enum{
	GPS_START_MESSAGE = 0,
	GPS_GET_MESSAGE,
	GPS_END_MESSAGE
}GPSState_e;

GPSState_e gpsState;


#define MSG_COUNT				sizeof(vGPS_funcs)/sizeof(vGPSMsgFunc_t)

/**_______GLOBAL VARIABLES_______________________________________*/
OS_MUT xGPS_Mutex;

gpsInfo_t gpsInfo;

OS_TID xGPS_RxID;

extern void vTaskGPS_Tick_signal(void);

void vAuto_SyncTime(void);

/********** TASK ***********************************/
__task void vGPS_RxTask(void)
{
	S8 c;

	// don't open sim18 at here. If its not running, save data task will open module.
//	if(!blSIM18Status())
//	{
//		blSIM18_Open();
//	}

	for(;;)
	{
		wdt_gps_rx_cnt = 0;

		// read nmea message.
		if(blGetBuffer(&c)==1)
		{

			vGPS_Parser(c);
		}
		else
		{
			/* pass other task */
			os_dly_wait(10/OS_TICK_RATE_MS);	// delay 10ms
		}
	}
}

/********* FUNCTIONS ******************************/
void vGPS_Init(void)
{
	memset(&gpsInfo, 0, sizeof(gpsInfo_t));

	/* init mutex */
	os_mut_init(&xGPS_Mutex);

	// request sync time.
	requestSyncTime = 1;

	// state for gps
	gpsState = GPS_START_MESSAGE;

	/* tao task */
	xGPS_RxID = os_tsk_create_user(vGPS_RxTask, RTX_GPS_TSK_PRIORITY, &gpsStack, sizeof(gpsStack));
}

/**
 * \brief 	get time date
 */
void vUpdateTime(uGPSDateTime_t *tm)
{
	U8 hh, mm, ss;
	/* get date in rtc ram */
	vRTCGetDate(&hh, &mm, &ss);
	tm->day = hh;
	tm->month = mm;
	tm->year = ss;

	// get time in rtc ram
	vRTCGetTime(&hh, &mm, &ss);
	tm->hour = hh;
	tm->min = mm;
	tm->sec = ss;
}

/*
 * \brief 	Only update time if its changed.
 */
void GPS_GetTime(uGPSDateTime_t *tm)
{
	static U8 old_sec = 1;
	U8 hh, mm, ss;
	U16 tmo;
	/* get date in rtc ram */
	vRTCGetDate(&hh, &mm, &ss);
	tm->day = hh;
	tm->month = mm;
	tm->year = ss;

	tmo = 1000;		// delay 100ms

	// wait ss different old_sec
	do
	{
		if(tmo == 0)	// time out occur
			break;

		tmo --;
		// get time in rtc ram
		vRTCGetTime(&hh, &mm, &ss);
		os_dly_wait(1);			// wait 1ms. next other task
	}while(ss == old_sec);
	
	if((hh == 0) && (mm == 0) && (ss == 0)){
		USART_PrCStr(USART1, "\r\nTime zero");
	}

	// assign to variable.
	tm->hour = hh;
	tm->min = mm;
	tm->sec = ss;

	// assert old second.
	old_sec = ss;
}

/**
 * \brief		sync time
 */
static S16 iGPS_SyncTime(uGPSDateTime_t tm)
{
	U8 hh, mm, ss;

	vRTCSetDate(tm.day, tm.month, tm.year);
	vRTCGetDate(&hh, &mm, &ss);

	if((hh != tm.day)||(mm != tm.month)|| (ss != tm.year))
		return 0;

	vRTCSetTime(tm.hour, tm.min, tm.sec);
	vRTCGetTime(&hh, &mm, &ss);

	if((hh != tm.hour)||(tm.min != mm)||(tm.sec != ss))
		return 0;

	return 1;
}

/**
 * \brief 	Call back interrupt routine.
 */
void vGPS_Parser(S8 ch)
{
	static S8* pGPSData = GPS_Message;
	U16 i;

	switch(gpsState)
	{
	case GPS_START_MESSAGE:
		if(ch == '$'){
			pGPSData = GPS_Message;
			*pGPSData++ = ch;

			// next state GET.
			gpsState = GPS_GET_MESSAGE;
		}

		break;
	case GPS_GET_MESSAGE:
		if(ch != '\r')
		{
			*pGPSData++ = ch;
			if(pGPSData >= (GPS_Message + sizeof(GPS_Message)))
			{
				// wrong data. return state
				gpsState = GPS_START_MESSAGE;
				break;
			}
			*pGPSData = 0;
		}
		else
		{
			// next state GET.
			gpsState = GPS_END_MESSAGE;
		}

		break;
	case GPS_END_MESSAGE:
		// parser data
		if(ch == '\n')
		{
			for(i=0;i<MSG_COUNT;i++)
			{
				if(strncmp((const char*)GPS_Message, (const char*)sGPS_MSG_Table[i], 6)==0)
				{
					if(vGPS_funcs[i] != NULL)
						vGPS_funcs[i](GPS_Message);
					break;
				}
			}
		}

		// next state GET.
		gpsState = GPS_START_MESSAGE;
		break;
	default:
		// next state GET.
		gpsState = GPS_START_MESSAGE;
		break;
	}
}

//char txtGPS[32];
/********** NMEA MESSAGE DECODE ***********************************************/
/**
 * \brief 	parser gps message.
 * 			RMC message.
 * 		ex: $GPRMC,044632.000,A,2101.1289,N,10547.6750,E,0.00,228.09,141111,,,A*6E
 *			  		  0		  1 	2 	  3		4	   5   6	7		8  9..
 **/
S8 chRMCParser(S8 *prvParam)
{
	float temp;
	U8 n;
	S8* pItem[16];
	S8* pcChk[3];

	os_mut_wait(&xGPS_Mutex, 0xFFFF);

	/* split gps message into items */
	n = string_split((U8**)pcChk, (U8*)prvParam, "$*", 3);

	// wrong message.
	if(n != 2)
	{
		//sprintf(txtGPS, "\r\nGPS wrong message: %u", n);
		//USART_PrCStr(USART1, txtGPS);
		goto gps_exit;
	}
	/* checksum message */
	if(sChkSum(pcChk[0], pcChk[1]) != 1)
	{
		//USART_PrCStr(USART1, "\r\nGPS wrong checksum");
		goto gps_exit;
	}

	// find char ',', split field.
	n = findchar((U8**)pItem, (U8*)pcChk[0], ',', 13);
	// field number not enough.
	if(n <= 9)
	{
		//sprintf(txtGPS, "\r\nGPS RMC wrong fields: %u", n);
		//USART_PrCStr(USART1, txtGPS);
		goto gps_exit;
	}

	/* gps active */
	if((*(pItem[1]+1) == 'A'))
	{
		/* update time to rtc */
		if(utc2ict(&gpsInfo.dt, pItem[0]+1, pItem[8]+1, 0))
		{

			vAuto_SyncTime();
			/* update time when reset */
			if(requestSyncTime == 1)
			{
				/* synchronous date and time */
				tsk_lock();
				if(iGPS_SyncTime(gpsInfo.dt)==1)
					requestSyncTime = 0;
				tsk_unlock();

			}
		}
		else
		{
			// lay thoi gian he thong.
			GPS_GetTime(&gpsInfo.dt);
		}

		/* update new gps infomation */
		gpsInfo.status = 1;

		/* indicator */
		gpsInfo.n_s = (*(pItem[3]+1) == 'N');

		gpsInfo.e_w = (*(pItem[5]+1) == 'E');

		/* track angle */
		gpsInfo.trackAngle = (str2float(pItem[7]+1)*10.0);
		// latitude.
		temp = fLat2float(pItem[2]+1);
		gpsInfo.latitude = temp;
		// copy latitude
		temp = fLong2float(pItem[4]+1);		
		gpsInfo.longitude = temp;

		/* speed in dm/s */
		temp = (str2float(pItem[6]+1));
		gpsInfo.speed = (knot2km10ph(temp)) + 0.5;
	}
	else{
		/* clear gps infomation */
		gpsInfo.status = 0;
	}

gps_exit:
	// release mutex.
	os_mut_release(&xGPS_Mutex);

	return 1;
}

/**
 * \brief 	parser gps message.
 * 			GGA message.
 * 		ex: $GPGGA,020944.000,2101.1175,N,10547.6599,E,1,04,2.5,15.1,M,-20.8,M,,0000*42
 *			  		  0		  1 		2 	  3		 4 5  6	 7	 8   9..
 **/
S8 chGGAParser(S8 *prvParam)
{
	S16 t;
	U8 n;
	S8* pItem[16];
	S8* pcChk[3];

	os_mut_wait(&xGPS_Mutex, 0xFFFF);
	/* split gps message into items */
	n = string_split((U8**)pcChk, (U8*)prvParam, "$*", 3);

	// wrong message.
	if(n != 2)
	{
		goto gga_exit;
	}

	/* checksum message */
	if(sChkSum(pcChk[0], pcChk[1]) != 1)
	{
		goto gga_exit;
	}

	n = findchar((U8**)pItem, (U8*)pcChk[0], ',', 14);
	if(n <= 9)
	{
		goto gga_exit;
	}

	if(*(pItem[5]+1) != '0')
	{
		/* copy number of satellites */
		t = (U8)atoi((const char*)pItem[6]+1);
		/* copy number of satellites */
		gpsInfo.numberSat = t;

		/* altitude */

		t = atoi((const char*)pItem[8]+1);

		gpsInfo.altitude = t;
	}

gga_exit:
	if(gpsInfo.status == 0)
	{
		memset((uint8_t*)&gpsInfo, 0, sizeof(gpsInfo));
		GPS_GetTime(&gpsInfo.dt);
	}	
	os_mut_release(&xGPS_Mutex);
	
	// set event for task save data.
	vTaskGPS_Tick_signal();

	return 1;
}

/*
 * ban tin GSA lay dop cua GPS. tam thoi chua dung.
 */
S8 chGSAParser(S8 *prvParam)
{
	U8 n;
	float hdop, vdop, pdop;
	S8* pItem[16];
	S8* pcChk[3];

	USART_PrCStr(USART1, prvParam);

	os_mut_wait(&xGPS_Mutex, 0xFFFF);

	/* split gps message into items */
	n = string_split((U8**)pcChk, (U8*)prvParam, "$*", 3);

	// wrong message.
	if(n != 2)
	{
		goto set_event;
	}

	/* checksum message */
	if(sChkSum(pcChk[0], pcChk[1]) != 1)
	{
		goto set_event;
	}

	n = string_split((U8**)pItem, (U8*)pcChk[0], ",", 16);
	if(n < 5)
	{
		USART_PrCStr(USART1, "\r\nGPS GSA wrong number of fields");
		goto set_event;
	}

	USART_PrCStr(USART1, "\r\nGPS GSA OK");
	USART_PrCStr(USART1, pItem[n-3]);
	USART_PrCStr(USART1, ",");
	USART_PrCStr(USART1, pItem[n-2]);
	USART_PrCStr(USART1, ",");
	USART_PrCStr(USART1, pItem[n-1]);

	hdop = atof(pItem[n-2]);
	pdop = atof(pItem[n-3]);
	vdop = atof(pItem[n-1]);

	if((hdop >= 6) || (pdop >= 6) || (vdop >= 6))
	{
		USART_PrCStr(USART1, "\r\nGPS's DOP is bad");
		goto set_event;
	}

	USART_PrCStr(USART1, "\r\nGPS SIGNAL OK");
	//TODO: xu ly bao trang thai tin hieu tot cho GPS

set_event:
	// set event for task save data.
	//vTaskGPS_Tick_signal();

	os_mut_release(&xGPS_Mutex);

	return 1;
}


/**** API FOR other LAYER *******************************************************/
void GPS_get_location_indicate(uint8_t *n, uint8_t *e)
{
	os_mut_wait(&xGPS_Mutex, 0xFFFF);
	*n = gpsInfo.n_s;
	*e = gpsInfo.e_w;
	os_mut_release(&xGPS_Mutex);
}

void GPS_get_xy(float *x, float *y)
{
	os_mut_wait(&xGPS_Mutex, 0xFFFF);

	*x = gpsInfo.latitude;
	*y = gpsInfo.longitude;

	os_mut_release(&xGPS_Mutex);
}

uint8_t GPS_get_speed(void)
{
	uint8_t v;
	os_mut_wait(&xGPS_Mutex, 0xFFFF);

	v = gpsInfo.speed/10;

	os_mut_release(&xGPS_Mutex);
	
	return v;
}

BOOL blGPS_Read(S8* pOut)
{
	int i;
	char *p;

	os_mut_wait(&xGPS_Mutex, 0xFFFF);
	p = (char*)&gpsInfo;
	for(i=0;i<sizeof(gpsInfo_t);i++)
		*pOut++ = *p++;

	os_mut_release(&xGPS_Mutex);

	return 1;
}

/*
 * \brief 	gpsInfo_t GPS_GetInfo(void)
 * 			Get all field in gps struct.
 * \param	none.
 * \return 	gps struct.
 */
gpsInfo_t GPS_GetInfo(void)
{
	return gpsInfo;
}

/*
 * \brief
 * 			return GPS status
 */
int8_t GPS_GetStatus(void)
{
	return gpsInfo.status;
}

void vAuto_SyncTime(void)
{
	U32 t1, t2;
	uGPSDateTime_t tm;

	t1 = gpsInfo.dt.hour*3600 + gpsInfo.dt.min*60 + gpsInfo.dt.sec;

	vUpdateTime(&tm);
	t2 = tm.hour*3600 + tm.min*60 + tm.sec;

	// neu lech nhau 3s.
	if((t1 > (t2 + 2))||(t2 > t1 + 2))
	{
		requestSyncTime = 1;
	}
}

void vGPS_Reset(void)
{
	// reset state cua GPS machine
	gpsState = GPS_START_MESSAGE;
	/* power off */
	vGPS_PowerSupplyOff();
	os_dly_wait(500/OS_TICK_RATE_MS);

	/* reopen sim18 module */
	if(blSIM18_Open()!=__TRUE)
	{
		/* power off */
		vGPS_PowerSupplyOff();			// turn off power for reset again
	}
}


uint8_t WDT_Check_GpsRx_Service(void)
{
	wdt_gps_rx_cnt++;
	
	if(wdt_gps_rx_cnt >= WDT_GPS_RX_MAX_TIMING)
		return 1;
	
	return 0;
}
