/**
 * @file 	PeriodicService.c
 * @brief	Service which run at interval time
 * @author	bacnh@eposi.vn
 * @date	2012.02.11
 * @verision:
 * 		0.2 	dem xung bang counter.
 * 		0.1		dem xung bang ngat.
 */

/* Include header file */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include "modules.h"
#include "Platform_Config.h"

#include "utils.h"

#include "gpsModule.h"
#include "gprs.h"
#include "adc.h"
#include "counter.h"
#include "rtc.h"
#include "voice.h"
#include "drvkey.h"
#include "loadconfig.h"
#include "tmp100.h"

#include "boardtest.h"
#include "http.h"
#include "driverprocess.h"
#include "filter.h"
#include "camera.h"
#include "debug.h"
#include "io_manager.h"

#include "PeriodicService.h"

/* Private macros --------------------------------------------- */
#define GPS_TIMEOUT				1800 // unit second

// define event over speed, over total time driver...
#define OVER_TOTAL_TIME_EVT		0x40			// event tong thoi gian lai xe trong ngay
#define OVER_SPEED_EVT			0x20			// event qua toc do.
#define OVER_CONT_TIME_EVT		0x10			// event lai xe lien tuc
#define	CHANGE_DRV_KEY			0x08			// event change driver key.
#define CAR_ENGINE_OFF_EVT		0x04			// event do xe
#define CAR_DOOR_EVT			0x02			// event dong cua.
#define CAR_ENGINE_STOP_EVT		0x01			// event dung xe

#define DIV_DELTA				1000000.00L

/* Private variables ------------------------------------------ */
CarPulse_t	TurbinPulse;
CarPulse_t	CarPulse;
ObjectState_t xObjState;
ObjectState_t OldState;

LogInfo_t LogInfo;
Data_frame_t raw_data;

#define CAM_TIMEOUT 			120
U16 cameraTimeout = 30; //CAM_TIMEOUT;
const U16 Cam_Snapshot_Time = 900;
uint16_t _gpsErrCount = 0;

static U64 tsk1s_Stack[600/8];
static U64 tskSaveData_Stack[600/8];

U16 gpsTimeOut = GPS_TIMEOUT;
uint32_t wdt_tsk_save_cnt = 0;
uint32_t wdt_tsk_1sec_cnt = 0;

// mutex
OS_MUT perMutex;
OS_MUT logLock;
OS_TID tsk1Sec_tid;
OS_TID tskSaveData_ID;

/* Private functions ------------------------------------------ */
static void vRAW_GPIOUpdate(void);
static void vLogUpdate(void);
static void vPulseUpdate(void);
void vUpdateStop_Off_vehicle(void);
void vPushMessage(void);

/*new */
void vUpdateGPSInfo(void);
void vUpdateOldValue(void);
void vCheckVehicleStatus(void);
void vConfigGPIO(void);
void vResetNewDayData(void);
void vRequestCamera(void);
static void vLOG_GPIOUpdate(void);
static void vRequestTax(io_message_t *ioMessage);

/* debug */
void vLogDebugOnPC(S8* data, U16 size);
void vDATADebugOnPC(S8* data, U16 size);

extern void GPRS_ResetReport(void);
extern int8_t GSM_Mailbox_Puts(int8_t *s);


/* RTX Tasks */
__task void vTask1Sec(void);
__task void vTaskSaveData(void);

/************ implementation ******************************/
#ifdef	PERIODIC_DEBUG
void vPeri_err(S8* s)
{
	dbg_print(s);
}
#else
void vPeri_err(S8* s)
{
}
#endif

/*
 * x, y la diem dang tin.
 * tat ca cac diem co delta(x,y, xi, yi) < 2.5m => i la diem dang tin.
 * cap nhat i vao arr diem dang tin.
 * dis(x, y, xn, yn) >= 60m -> tat ca cac diem la dang tin (xe chuyen dong).
 * nguoc lai, trung binh cac diem trong mang dang tin => diem dang tin tiep theo. cap nhat len x, y.
 */
uint8_t blValidateGPSData(float x, float y, S16 *arrX, S16 *arrY, S16 arr_size, S16 threshold)
{
	float x_end, y_end;
//	float xi, yi
	S16 dx, dy;
	S16 distance, i;
//	char gpsdbg[12];

	if(threshold == 0)
		return 1;			// khong loc.

	if(threshold > 250)		// set defalut
		threshold = 120;

	/* x, y must valid */
	if((x == 0) || (y == 0))
		return 0;

	/* init local variables */
	dx = 0;
	dy = 0;

	/* search point stable */
	for(i=0;i<arr_size;i++)
	{
		dx += arrX[i];
		dy += arrY[i];
	}
	x_end = (double)dx/DIV_DELTA + x;
	y_end = (double)dy/DIV_DELTA + y;

	distance = diff_dist_m(x_end, y_end, x, y);

//	sprintf(gpsdbg, "\r\nA:%u", distance);
//	USART_PrCStr(USART1, gpsdbg);

	if(distance >= threshold)
		return 1;

	return 0;
}


/*
 * \brief	int8_t ValidateGPSSpeed(uint8_t ovSpeed)
 * 			check overange speed more than ovSpeed?.
 * \param	ovSpeed as threshold.
 * \return 	1 - more than.
 */
int8_t ValidateGPSSpeed(uint8_t s, uint8_t *arrS, uint8_t ovSpeed)
{
	int16_t i;
	uint16_t aveg = s&0xFF;
	uint16_t temp;

	//char gpsdbg[12];

	for(i=0;i<29;i++)
	{
		temp = arrS[i] & 0xFF;
		aveg += temp;
	}
	aveg = aveg / 30;

//	sprintf(gpsdbg, "\r\nS:%u", aveg);
//	USART_PrCStr(USART1, gpsdbg);

	return (aveg >= ovSpeed)? 1: 0;
}


/*
 * @brief	Init Periodic Service
 * @param	none
 */
void vPeriodicServiceInit(void){
	/* init gpio */
	vConfigGPIO();
	/* adc module */
	vADC_Init();
	/* timer for pulse */
	vTimPulseInit();
	/* pool adc filter */
	vADC_Filter();

	/* if camera is used - init camera */
	if(Config.CAM.cam_used)
		vCameraInit();
		/* if allow support taxi meter */
	if(Config.TM.tm_used)
		vTaxiMeterInit();

	// set variables
	vLoadFromBackupRam(BKP_TOTAL_PULSE_H, (U16*)&CarPulse.TotalPulse, sizeof(CarPulse.TotalPulse)/sizeof(U16));
	vLoadFromBackupRam(BKP_TOTAL_TURBINPULSE_H, (U16*)&TurbinPulse.TotalPulse, sizeof(TurbinPulse.TotalPulse)/sizeof(U16));
	vLoadFromBackupRam(BKP_TOTAL_RUN_H, (U16*)&LogInfo.total_time, sizeof(LogInfo.total_time)/sizeof(U16));
	vLoadFromBackupRam(BKP_CONTI_RUN_H, (U16*)&LogInfo.cont_time, sizeof(LogInfo.cont_time)/sizeof(U16));
	vLoadFromBackupRam(BKP_TOTAL_CAR_OFF_H, (U16*)&LogInfo.c_off, sizeof(LogInfo.c_off)/sizeof(U16));
	vLoadFromBackupRam(BKP_TOTAL_CAR_STOP_H, (U16*)&LogInfo.c_stop, sizeof(LogInfo.c_stop)/sizeof(U16));
	vLoadFromBackupRam(BKP_DOOR_OPEN_H, (U16*)&LogInfo.c_door, sizeof(LogInfo.c_door)/sizeof(U16));
	vLoadFromBackupRam(BKP_OVER_SPEED_H, (U16*)&LogInfo.over_speed, sizeof(LogInfo.over_speed)/sizeof(U16));

	vLoadFromBackupRam(BKP_CURRENT_DRV_ID, (U16*)&LogInfo.id_drv,  sizeof(LogInfo.id_drv)/sizeof(U16));

	vInitDriver();
	memcpy(raw_data.drv_key, LogInfo.id_drv, 8);
	raw_data.cont_time = LogInfo.cont_time;
	raw_data.door_numb = LogInfo.c_door;
	raw_data.off_numb = LogInfo.c_off;
	raw_data.over_speed = LogInfo.over_speed;
	raw_data.total_pulse1 = CarPulse.TotalPulse;
	raw_data.total_pulse2 = TurbinPulse.TotalPulse;
	raw_data.stop_numb = LogInfo.c_stop;
	raw_data.total_time = LogInfo.total_time;

	memset((uint8_t*)&raw_data.delta_lat, 0, sizeof(raw_data.delta_lat));
	memset((uint8_t*)&raw_data.delta_lng, 0, sizeof(raw_data.delta_lng));
	memset((U8*)&raw_data.gps_speed_arr, 0, 29);

	os_mut_init(perMutex);
	os_mut_init(logLock);

	/* create 2 tasks */
	tskSaveData_ID = os_tsk_create_user(vTaskSaveData, RTX_SAVEDATA_TSK_PRIORITY, &tskSaveData_Stack, sizeof(tskSaveData_Stack));
	tsk1Sec_tid = os_tsk_create_user(vTask1Sec, RTX_1SEC_TSK_PRIORITY, &tsk1s_Stack, sizeof(tsk1s_Stack));

}

/* RTX 10 sec Service Tasks ---------------------------------------------------- */
uint8_t GPS_Flag = 1;	// 1: ngon, 0:

__task void vTaskSaveData(void)
{
	U16 result;
	U8 evt_tmo = 10;

	/* Sync with GPS Tick */
	result = os_evt_wait_or(RTX_GPS_TICK_FLAG, 4000/OS_TICK_RATE_MS);
	if(result == OS_R_TMO){
		/* init sim18 again */
		vGPS_Reset();
	}

	/* synchronous time packet */
	vUpdateTime(&raw_data.tm);
	vUpdateTime(&LogInfo.tm);
	/* set first data at second = [0, 30] */
	if(raw_data.tm.sec < 30)
		raw_data.tm.sec = 0;
	else
		raw_data.tm.sec = 30;

	for(;;)
	{
		wdt_tsk_save_cnt = 0;
		if(os_evt_wait_and(RTX_GPS_TICK_FLAG, 15000/OS_TICK_RATE_MS) == OS_R_EVT)
		{
			GPS_Flag = 1 ;
			// reset evt timeout
			evt_tmo = 10;
			/* lock variable */
			os_mut_wait(perMutex, 0xFFFF);

			/* update GPS infomation */
			vUpdateGPSInfo();

			// kiem tra trang thai cua xe.
			vCheckVehicleStatus();

			/* Save log data */
			vLogUpdate();

			os_mut_release(perMutex);

			/* to pc on debuger */
			vDATADebugOnPC((S8*)&raw_data, sizeof(raw_data));

		}
		else
		{
// 			if(evt_tmo == 0)
// 			{
// 				evt_tmo = 10;
// 				/* send error code = 0 sim18 error */
// 				//blSendError(SIM18_ERR_CODE);

// 				//USART_PrCStr(USART1, "\r\nSEND GPS ERR MESSAGE");
// 			}
// 			else
// 				evt_tmo--;
			_gpsErrCount ++;
			/* init sim18 again */
			vGPS_Reset();
			// debug
			blSendError(SIM18_ERR_CODE, _gpsErrCount);
			USART_PrCStr(USART1, "\r\nSEND GPS ERR MESSAGE");
			/* Flag */
			GPS_Flag = 0;
		}
	}
}

/* RTX 1 sec Service Tasks ---------------------------------------------------- */
/*
 * 
 * @brief	Service which run every one sec
 * @param	None
 *
 * task base on GPS event tick.
 */
__task void vTask1Sec(void)
{
	io_message_t IOMessage;
	
	os_itv_set(INTERVAL_1SEC/OS_TICK_RATE_MS);
	io_manager_init(&IOMessage);
	/* Super loop */
	for(;;)
	{
		wdt_tsk_1sec_cnt = 0;
		// io manager
		if(Config.option.usedIOMessage)
			io_manager(&IOMessage);
		// get tax for server.
		vRequestTax(&IOMessage);
			
		/* lock variable */
		os_mut_wait(perMutex, 0xFFFF);

		vRequestCamera();

		// Read Pulse speed (Service)
		vPulseUpdate();

		/* update trang thai dung do xe */
		vUpdateStop_Off_vehicle();

		// thoi gian lai xe va qua toc do.
		vUpdateDriverData();

		// check change driver key.
		vCheckChangeKey();
		// io for log
		vLOG_GPIOUpdate();

		vUpdateOldValue();

		os_mut_release(perMutex);

		/* GPS fail case */
		if(GPS_Flag == 0){
			// date time
			vUpdateTime(&LogInfo.tm);

			vLogUpdate();

		};

		os_itv_wait();
	}
}


/******** FUNCTIONS ***********************************************/
/* temperature update */
void vUpdateTemper(void)
{
	char s[2];
	int8_t ret;

	ret = TMP100_ReadTemp(s);

	if(ret==0){
		raw_data.heating = s[0];

	}else{
		raw_data.heating = -128;
	}

}

void vTaskGPS_Tick_signal(void)
{
	if(tskSaveData_ID != NULL)
		os_evt_set(RTX_GPS_TICK_FLAG, tskSaveData_ID);
}

/* request camera */
void vRequestCamera(void)
{
	/* camera if used */
	if(Config.CAM.cam_used)
	{
		if((cameraTimeout)==0)
		{
			cameraTimeout = Config.CAM.CamSnapShotCycle;

			CAM_SnapShot();
		}
		else
		{
			cameraTimeout--;
		}
	}

}


/*
 * \brief	void vRequestTax(void)
 * 			Generate a request tax to mailbox.
 * \param	none.
 * \return	none.
 */
static void vRequestTax(io_message_t *ioMessage)
{
	uint8_t *s;

	if(HostChannel.state == CH_STATE_BUSY)
		return;
	// if new day, request tax
	if(RTC_GetNewdayFlag(RTC_TAX1_FLAG))
	{
		// IO reset.
		io_report_clear(ioMessage);
		// reset GPRS report
		GPRS_ResetReport();

		s = (S8*)calloc(20, sizeof(S8));
		if(s == NULL)
			return;

		strcpy(s, "$GET_TAX,TAX1#");

		RTC_ReSetNewdayFlag(RTC_TAX1_FLAG);
		GSM_Mailbox_Puts(s);
		
		free(s);
		return;
	}

	if(RTC_GetNewdayFlag(RTC_TAX2_FLAG))
	{
		s = (S8*)calloc(20, sizeof(S8));
		if(s == NULL)
			return;

		strcpy(s, "$GET_TAX,TAX2#");

		RTC_ReSetNewdayFlag(RTC_TAX2_FLAG);
		GSM_Mailbox_Puts(s);
		
		free(s);
	}
}

/* Update GPS Information */
void vUpdateGPSInfo(void)
{
	static U8 isUpdate = 0;
	static U8 rd_idx = 0;
	static float x_start=0, y_start=0, x_old=0, y_old = 0;
	static U8 s_start = 0;
	gpsInfo_t gps;
	S16 i;
//	char gpsdbg[12];

	/* read gps info from gps module */
	//gps = GPS_GetInfo();
	blGPS_Read((uint8_t*)&gps);

	/* check for first data */
	if(((x_start == 0)||(y_start==0)) && (gps.status == 1))
	{
		raw_data.alt = gps.altitude;
		raw_data.trackAngle = gps.trackAngle;

		LogInfo.lat = gps.latitude;
		LogInfo.lng = gps.longitude;

		x_start = gps.latitude;
		y_start = gps.longitude;
		s_start = (U8)(gps.speed/10);

		x_old = gps.latitude; y_old = gps.longitude;
	}

	/* check gps ok? if fail then reset after 1200s = 20min */
	if(gps.status == 0){
		gpsTimeOut --;
		if(gpsTimeOut == 0){
			/* init sim18 again */
			vGPS_Reset();
			gpsTimeOut = GPS_TIMEOUT;
		}
	}
	else{
		gpsTimeOut = GPS_TIMEOUT;
	}
	
	/* message at [0, 30] */
	if(((raw_data.tm.sec == 0)&&(gps.dt.sec >= 30)) || ((raw_data.tm.sec == 30)&&(gps.dt.sec < 30)))
	{
		// caculate
		uint8_t overageSpeed = ValidateGPSSpeed(raw_data.gps_speed, raw_data.gps_speed_arr, (uint8_t)Config.GPSFilter.overspeed_threshold);
		uint8_t overdis = blValidateGPSData(x_start, y_start, raw_data.delta_lat, raw_data.delta_lng, 29, Config.GPS_radius);
		/* read status vehicle */
		uint8_t vehicle_sate = GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN) & Config.GPSFilter.use_engine;
		// validate state.
		vehicle_sate = vehicle_sate | (!Config.GPSFilter.method_filter & overageSpeed) | (Config.GPSFilter.method_filter & overdis);

//		sprintf(gpsdbg, "\r\nG:%u", vehicle_sate);
//		USART_PrCStr(USART1, gpsdbg);

		if(vehicle_sate || (isUpdate > 0))	// vehicle running.
		{
			//USART_PrCStr(USART1, "\r\nUpdating..");
			/* add begin point */
			raw_data.lat = x_start;
			raw_data.lng = y_start;
			raw_data.gps_speed = s_start;
			/* push packet to buffer. */

			vPushMessage();
			/* update state */
			if(vehicle_sate) 	isUpdate = 2;
			else				isUpdate --;

			raw_data.alt = gps.altitude;
			raw_data.trackAngle = gps.trackAngle;
			// clean data for next package.
			for(i=0;i<29;i++)
			{
				raw_data.delta_lat[i] = 0;
				raw_data.delta_lng[i] = 0;
				raw_data.gps_speed_arr[i] = 0;
			}
		}
		else{
			/* reset data */
			for(i=0;i<29;i++)
			{
				raw_data.delta_lat[i] = 0;
				raw_data.delta_lng[i] = 0;
				raw_data.gps_speed_arr[i] = 0;
			}
			raw_data.gps_speed = 0;

			if((x_start < 1.0) || (y_start < 1.0) || (raw_data.lat == 0) || (raw_data.lng==0))
			{
				raw_data.lat = x_start;
				raw_data.lng = y_start;
			}
			/* push packet to buffer. */
			vPushMessage();
		}

		/* update next state */
		x_start = gps.latitude;		// vi tri dau tien cua xe trong ban tin 30s
		y_start = gps.longitude;
		s_start = (U8)(gps.speed/10);
		if(s_start > 200) s_start = s_start/10;

		x_old = gps.latitude; y_old = gps.longitude;

		raw_data.tm = gps.dt;
		/* Set second at 0, 30 */
		if(gps.dt.sec >=30)	raw_data.tm.sec = 30;
		else				raw_data.tm.sec = 0;
		
		// GPS status.
		raw_data.e_w = gps.e_w;
		raw_data.n_s = gps.n_s;
		raw_data.status = gps.status;
		raw_data.numberSat = gps.numberSat;
	}
	else
	{
		// calculate index.
		rd_idx = gps.dt.sec%30;
		if(rd_idx > 0) rd_idx -= 1;		// index of array begin = 0. second begin = 1.

		// delta x, y. x(i) - x(i-1), y(i) - y(i-1).
		if((gps.status == 1) && (LogInfo.lat != 0) && (LogInfo.lng != 0) )
		{
			float d;			
			d = diff_dist_m(LogInfo.lat, LogInfo.lng, gps.latitude, gps.longitude);
			
			if((d < 42.5))
			{
				raw_data.gps_speed_arr[rd_idx] = (U8)(gps.speed/10);
				raw_data.delta_lat[rd_idx] = (S16)(((double)(gps.latitude - x_old)*DIV_DELTA));
				raw_data.delta_lng[rd_idx] = (S16)(((double)(gps.longitude - y_old)*DIV_DELTA));

				x_old = gps.latitude; y_old = gps.longitude;
			}
			else	// GPS bad.
			{
				raw_data.gps_speed_arr[rd_idx] = (U8)(LogInfo.CurSpeed/10);
				raw_data.delta_lat[rd_idx] = 0;
				raw_data.delta_lng[rd_idx] = 0;
			}

		}
		else	// GPS loss
		{
			raw_data.delta_lat[rd_idx] = 0;
			raw_data.delta_lng[rd_idx] = 0;
			raw_data.gps_speed_arr[rd_idx] = 0;

		}
	}

 	LogInfo.lat = gps.latitude;
 	LogInfo.lng = gps.longitude;
	// update log value.
 	LogInfo.CurSpeed = gps.speed;

	// update time for log.
	LogInfo.tm = gps.dt;
}


/**** pulse *********************************/
static void vPulseUpdate(void)
{
	CarPulse.CurPulse = uiGetPulse1();

	if(CarPulse.MaxPulse < CarPulse.CurPulse)
	{
		CarPulse.MaxPulse = CarPulse.CurPulse;
	}
	CarPulse.TotalPulse += CarPulse.CurPulse;
	// save total pulse to backup ram.
	vSaveToBackupRam(BKP_TOTAL_PULSE_H, (U16*)&CarPulse.TotalPulse,
			sizeof(CarPulse.TotalPulse)/sizeof(U16));

	// turbin pulse.
	TurbinPulse.CurPulse = uiGetPulse2();
	if(TurbinPulse.MaxPulse < TurbinPulse.CurPulse)
	{
		TurbinPulse.MaxPulse = TurbinPulse.CurPulse;
	}
	TurbinPulse.TotalPulse += TurbinPulse.CurPulse;
	// save total pulse to backup ram.
	vSaveToBackupRam(BKP_TOTAL_TURBINPULSE_H, (U16*)&TurbinPulse.TotalPulse,
			sizeof(TurbinPulse.TotalPulse)/sizeof(U16));

}

/**** GPIO *******************************/
/**
 * doc trang thai i/o
 */
static void vRAW_GPIOUpdate(void)
{
	//
	raw_data.io.A_CStatus = GPIO_ReadInputDataBit(A_C_GPIO_PORT, A_C_GPIO_PIN);
	raw_data.io.DoorStatus = GPIO_ReadInputDataBit(DOOR_GPIO_PORT, DOOR_GPIO_PIN);
	raw_data.io.EngineStatus = GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN);

	raw_data.io.IO_IN3 = GPIO_ReadInputDataBit(IO_PORT, IO_IN3_PIN);
	raw_data.io.IO_IN4 = GPIO_ReadInputDataBit(IO_PORT, IO_IN4_PIN);
	raw_data.io.IO_IN5 = GPIO_ReadInputDataBit(IO_PORT, IO_IN5_PIN);

}

/* update trang thai dung/do xe */
void vUpdateStop_Off_vehicle(void)
{
	static U16 off_tmo = 1;//TIME_VEHICLE_STOP_THRESHODL;
	static U16 stop_tmo = 1;
	/* neu xe dung */
	if(xGetObjState() == STOPPING)
	{
		// neu xe chuyen tu chay sang dung thi coi la dung xe.
		if(stop_tmo == 1)
		{
			LogInfo.c_stop++;
			LogInfo.st |= CAR_ENGINE_STOP_EVT;		// su kien dung xe
			// Prevent check again

			vSaveToBackupRam(BKP_TOTAL_CAR_STOP_H, (U16*)&LogInfo.c_stop,
							sizeof(LogInfo.c_stop)/sizeof(U16));
		}
		if(stop_tmo >0) stop_tmo--;

		// kiem tra thoi gian do xe.
		if((off_tmo == 1))
		{
			// sau 15p ma trang thai dong co la tat thi coi la do xe.
			if(GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN) == 0)
			{
				LogInfo.c_off ++;
				LogInfo.st |= CAR_ENGINE_OFF_EVT;			// su kien do xe
				// save backuup
				vSaveToBackupRam(BKP_TOTAL_CAR_OFF_H, (U16*)&LogInfo.c_off,
							sizeof(LogInfo.c_off)/sizeof(U16));

			}
		}
		if(off_tmo > 0) off_tmo--;

	}
	// neu xe chay. reset time out.
	else
	{
		off_tmo = TIME_VEHICLE_OFF_THRESHODL;
		stop_tmo = TIME_VEHICLE_STOP_THRESHODL;
	}

}

/*
 * IO update
 */
static void vLOG_GPIOUpdate(void)
{
	static CarStatus_t old_io;

	/* io */
	LogInfo.io.A_CStatus = GPIO_ReadInputDataBit(A_C_GPIO_PORT, A_C_GPIO_PIN);
	LogInfo.io.DoorStatus = GPIO_ReadInputDataBit(DOOR_GPIO_PORT, DOOR_GPIO_PIN);
	LogInfo.io.EngineStatus = GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN);

	LogInfo.io.IO_IN3 = GPIO_ReadInputDataBit(IO_PORT, IO_IN3_PIN);
	LogInfo.io.IO_IN4 = GPIO_ReadInputDataBit(IO_PORT, IO_IN4_PIN);
	LogInfo.io.IO_IN5 = GPIO_ReadInputDataBit(IO_PORT, IO_IN5_PIN);

	// thay doi trang thai dong mo cua.
	if((old_io.DoorStatus != LogInfo.io.DoorStatus) && (LogInfo.io.DoorStatus == 1))
	{
		// neu dong or mo cua?
		LogInfo.c_door ++;
		vSaveToBackupRam(BKP_DOOR_OPEN_H, (U16*)&LogInfo.c_door,
					sizeof(LogInfo.c_door)/sizeof(U16));
		LogInfo.st |= CAR_DOOR_EVT;					// su kien dong cua
	}
	/* xung cho log */
	LogInfo.pulse = CarPulse.CurPulse;


	/* old io */
	old_io = LogInfo.io;
}

/******* UPDATE LOG *************************************/
static void vLogUpdate(void)
{
	// lock if printing...
	if(os_mut_wait(logLock, 0) != OS_R_TMO)
	{
		// write to sdcard
		vLogWrite((uint8_t*)&LogInfo, sizeof(LogInfo_t));
		// unlock.
		os_mut_release(logLock);
	}
}

/* xu ly cac su kien khi chuyen ngay moi */
void vResetNewDayData(void)
{
	LogInfo.total_time = 0;
	LogInfo.cont_time = 0;
	LogInfo.over_speed = 0;
	LogInfo.c_door = 0;
	LogInfo.c_off = 0;
	LogInfo.c_stop = 0;
	LogInfo.st = 0;
	/* reset thong tin lai xe. total time, over speed. */
	vResetDataInDay();

	/* save data to backup ram */
	vSaveToBackupRam(BKP_DOOR_OPEN_H, (U16*)&LogInfo.c_door,
					sizeof(LogInfo.c_door)/sizeof(U16));
	vSaveToBackupRam(BKP_TOTAL_CAR_STOP_H, (U16*)&LogInfo.c_stop,
					sizeof(LogInfo.c_stop)/sizeof(U16));
	vSaveToBackupRam(BKP_TOTAL_CAR_OFF_H, (U16*)&LogInfo.c_off,
					sizeof(LogInfo.c_off)/sizeof(U16));

}

/* tinh checksum va luu ban tin vao the nho. */
void vPushMessage(void)
{
	vUpdateTemper();
	/* read io */
	vRAW_GPIOUpdate();
	/* update xung */
	raw_data.pulse1 = CarPulse.CurPulse;
	raw_data.pulse2 = TurbinPulse.CurPulse;
	raw_data.total_pulse1 = CarPulse.TotalPulse;
	raw_data.total_pulse2 = TurbinPulse.TotalPulse;

	/* read ADC in filter buffer */
	raw_data.adc1 = uiReadADC1Value();

	raw_data.adc2 = uiReadADC2Value();
	raw_data.adc3 = uiReadADC3Value();

	/* thong tin lai xe */
	raw_data.total_time = (U16)((double)LogInfo.total_time/(double)60.0 + 0.5);
	raw_data.cont_time = (U16)((double)LogInfo.cont_time/(double)60.0 + 0.5);
	raw_data.over_speed = LogInfo.over_speed;
	raw_data.stop_numb = LogInfo.c_stop;
	raw_data.off_numb = LogInfo.c_off;
	raw_data.door_numb = LogInfo.c_door;
	memcpy(raw_data.drv_key, LogInfo.id_drv, 8);

	/* set boxid for packet */
	memcpy(raw_data.BoxId, Config.BoxID, 5);

	// send to buffer. if raw_data size is changed, must changed size in msg_buffer.c */
	if(sdfifo_write((uint8_t*)&raw_data)!=0)
	{
		/* Send data directly into mailbox */
		memcpy(raw_data.BoxId+3, Config.BoxID, 5);
		blPushData(RAW_DATA, (uint8_t*)&raw_data+3, sizeof(Data_frame_t)-4);
	}
}

void vUpdateOldValue(void)
{
	// reset max pulse.
	CarPulse.MaxPulse = 0;
	TurbinPulse.MaxPulse = 0;
	// log event.
	LogInfo.st = 0x00;
}


/******** TRANG THAI XE *****************************************************/

/**
 * set trang thai xe
 */
void vSetObjState(OBJSTATE_E s)
{
	xObjState.state = s;
}

/*
 * doc trang thai xe.
 */
OBJSTATE_E xGetObjState(void)
{
	return (OBJSTATE_E)xObjState.state;
}

/*
 *  kiem tra trang thai cua xe.
 */
void vCheckVehicleStatus(void)
{
	static float pointStartX=0, pointStartY=0;
	static U8 count = 0;

	// Save old state
	OldState.state = xObjState.state;

	/* xe phai bat may, trong 20s neu xe nam ngoai vong 50m50 (hoac la quang duong di chuyen) thi coi la xe chay.
	 *
	 */
	switch(xGetObjState())
	{
		case STOPPING:
			/* if have speed and distance between points more than 50m
			 * object's speed fast enough, running.
			*/
			/* neu xe bat may */
			if(GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN) == 1)
			{
				/* start point */
				if(count == 0)
				{
					pointStartX = LogInfo.lat;
					pointStartY = LogInfo.lng;
				}
				count ++;
				/* end point */
				if(count >= 20)
				{
					if((pointStartX == 0)&& (pointStartY == 0))
					{
						pointStartX = LogInfo.lat;
						pointStartY = LogInfo.lng;
						break;
					}
					/* distance of two point in meters */
					if(diff_dist_m(pointStartX, pointStartY, LogInfo.lat, LogInfo.lng) > 40)
					{
						vSetObjState(RUNNING);
					}
					count = 0;
				}

			}

		break;
		case RUNNING:
			/* neu xe tat may */
			if(GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN) == 0)
			{
				vSetObjState(STOPPING);
				break;
			}

			/* neu khoang cach kha nho thi chua xac dinh trnag thai */
			if((diff_dist_m(pointStartX, pointStartY, LogInfo.lat, LogInfo.lng) <= 15))
			{
				vSetObjState(UNKNOWN);
			}

			/* next point */
			pointStartX = LogInfo.lat;
			pointStartY = LogInfo.lng;
		break;
		case UNKNOWN:
		default:
			/* neu xe tat may */
			if(GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN) == 0)
			{
				vSetObjState(STOPPING);
				break;
			}
			/* start point */
			if(count == 0)
			{
				if((pointStartX==0)&&(pointStartY==0))
				{
					vSetObjState(STOPPING);
					break;
				}
				pointStartX = LogInfo.lat;
				pointStartY = LogInfo.lng;
			}
			count ++;
			/* end point */
			if(count >= 20)
			{

				/* distance of two point in meters */
				if(diff_dist_m(pointStartX, pointStartY, LogInfo.lat, LogInfo.lng) < 20)
				{
					vSetObjState(STOPPING);
				}
				else
				{
					vSetObjState(RUNNING);
				}
				count = 0;
			}

		break;
	}
}


/* gpio init */
void vConfigGPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = DOOR_GPIO_PIN | A_C_GPIO_PIN | IO_IN3_PIN | IO_IN4_PIN | IO_IN5_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(IO_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = ENGINE_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_Init(IO_PORT, &GPIO_InitStructure);

	// out put
	GPIO_InitStructure.GPIO_Pin = IO_OUT1_PIN | IO_OUT2_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(IO_PORT, &GPIO_InitStructure);
}


/* for debug on PC */
#ifdef LOG_TO_PC
void vLogDebugOnPC(S8* data, U16 size)
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
void vLogDebugOnPC(S8* data, U16 size)
{
}
#endif

#ifdef DATA_TO_PC
void vDATADebugOnPC(S8* data, U16 size)
{
U16 i;
	S8 ch[2], *p;

	DBG_PrCStr("$DBG,");

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
void vDATADebugOnPC(S8* data, U16 size)
{
}
#endif

uint8_t WDT_Check_Tsk1Sec_Service(void)
{
	wdt_tsk_1sec_cnt++;
			
	if(wdt_tsk_1sec_cnt >= WDT_TSK_1SEC_MAX_TIMING)
		return 1;
	
	return 0;
}

uint8_t WDT_Check_TskSave_Service(void)
{
	wdt_tsk_save_cnt++;

	if(wdt_tsk_save_cnt >= WDT_TSK_SAVE_MAX_TIMING)
		return 1;
	
	return 0;
}
