/*
 * DriverProcess.c
 *
 *  Created on: Mar 11, 2012
 *      Author: Van Ha
 */

#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
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

#include "PeriodicService.h"

/* define chuan */
#define PARAM_THRESHOLD_STD
#ifdef PARAM_THRESHOLD_STD
#define TIME_LOGOUT				900		// second
#define TIME_STOP_THRESHOLD		900		// second
#define TIME_NOSTOP_THRESHOLD	14400	// second
#define TIME_TOTAL_THRESHOLD	36000	// second

#else

/* define test */
#define TIME_LOGOUT				300			// second
#define TIME_STOP_THRESHOLD		300		// second
#define TIME_NOSTOP_THRESHOLD	900		// second
#define TIME_TOTAL_THRESHOLD	1800	// second
#endif

typedef struct _Driver_Data_t
{
	U8 id[8];

	U32 totalTime;
	U16 contTime;
	U16 relaxTime;

	U16 overSpeed;

	uGPSDateTime_t tmLogin;
	uGPSDateTime_t tmLogout;

}Driver_Data_t;


typedef enum
{
	DRV_FILE_OK = 0,
	DRV_FILE_OPEN_FAIL,
	DRV_FILE_SAVE_FAIL,
	DRV_FILE_LOAD_FAIL
}drv_result_e;



FILE *f_dr;
S8 drv_fn[] = "DRV\\00000000.drv";

Driver_Data_t drvKey1;
extern LogInfo_t LogInfo;

U16 over_speed_tmo = TIME_OUT_SPEED;

drv_result_e drvSaveData(S8* drv_id);
drv_result_e drvLoadData(S8* drv_id);

static int stop_time = TIME_STOP_THRESHOLD+1;
static int nonstop_tmo = 0, tt_tmo = 0;

uint8_t drvPlugIn = 'N';

#ifdef DRIVER_PROCESS_DEBUG
const S8 *err_string[] =
{
		"DRV_FILE_OK\r\n",
		"DRV_FILE_OPEN_FAIL\r\n",
		"DRV_FILE_SAVE_FAIL\r\n",
		"DRV_FILE_LOAD_FAIL\r\n"
};
void vDriverDebug(drv_result_e err)
{
	dbg_print((uint8_t*)err_string[err]);
}
#else
void vDriverDebug(drv_result_e err)
{
}
#endif

void vInitDriver(void)
{
	drvKey1.totalTime = LogInfo.total_time;
	drvKey1.contTime = LogInfo.cont_time;
	drvKey1.overSpeed = LogInfo.over_speed;
	memcpy(drvKey1.id, LogInfo.id_drv, 8);
	drvKey1.relaxTime = 0;
}

/*
 * \brief 	update information about current driver.
 */
void vUpdateDriverData(void)
{
	// neu xe chay thi tang thoi gian lai xe cua lai xe hien tai.
	if((xGetObjState() != STOPPING) && GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN))
	{
		drvKey1.contTime ++;
		drvKey1.totalTime ++;

		/* neu lai xe chua nghi du thoi gian quy dinh */
		if((stop_time > 0) && (stop_time < TIME_STOP_THRESHOLD))
		{
			drvKey1.totalTime += stop_time;
			drvKey1.contTime += stop_time;
		}

		// backup thoi gian lai xe va lai xe lien tuc.
		vSaveToBackupRam(BKP_TOTAL_RUN_H, (U16*)&drvKey1.totalTime,
				sizeof(drvKey1.totalTime)/sizeof(U16));
		vSaveToBackupRam(BKP_CONTI_RUN_H, (U16*)&drvKey1.contTime,
				sizeof(drvKey1.contTime)/sizeof(U16));

		stop_time = 0;

		// canh bao qua lai xe lien tuc
		if(drvKey1.contTime >= TIME_NOSTOP_THRESHOLD)
		{
			/* play audio */
			// Give alarm
			if(nonstop_tmo == 0)
			{
				nonstop_tmo = 120;
				PlayVoiceFile(ALARM_OVER_CONTINUOUS_TIME);
			}
			else
			{
				nonstop_tmo--;
			}

		}

		// canh bao qua thoi gian lai xe trong ngay
		if(drvKey1.totalTime >= TIME_TOTAL_THRESHOLD)
		{
			/* play audio */
			// Give alarm
			if(tt_tmo==0)
			{
				tt_tmo = 120;
				PlayVoiceFile(ALARM_OVER_DRIVING_TIME);
			}
			else
			{
				tt_tmo--;
			}
		}

		// canh bao qua toc do. bo base theo xung.
		//if((LogInfo.CurSpeed >= MAX_SPEED_GPS)/*||(LogInfo.pulse > MAX_SPEED_PULSE)*/) // bo vi thay bang canh bao co the thay doi dc.
		if(Config.alarmSpeed.alarmSpeedValue <= (U8)(LogInfo.CurSpeed/10))
		{
			// Give alarm
			PlayVoiceFile(ALARM_OVERSPEED);

			// If Overspeed for 30s, then write to log
			if((--over_speed_tmo)==0)
			{
				over_speed_tmo = TIME_OUT_SPEED;
				drvKey1.overSpeed++;

				LogInfo.over_speed = drvKey1.overSpeed;
				vSaveToBackupRam(BKP_OVER_SPEED_H, (U16*)&drvKey1.overSpeed,
								sizeof(drvKey1.overSpeed)/sizeof(U16));
			}
		}else
		{
			over_speed_tmo = TIME_OUT_SPEED;
			
//			// alarm speed user. bo tinh nang nay.
//			if(Config.alarmSpeed.alarmSpeedEnable)
//			{
//				// over speed
//				if(Config.alarmSpeed.alarmSpeedValue <= (U8)(LogInfo.CurSpeed/10))
//				{
//					// Give alarm
//					PlayVoiceFile(ALARM_OVERSPEED_USER);
//				}
//			}
		}
	}
	else
	{
		// kiem tra xem xe co dung du tg k?
		if(stop_time==TIME_STOP_THRESHOLD)
		{
			// reset lai xe lien tuc.
			nonstop_tmo = 0;
			drvKey1.contTime = 0;
			stop_time++;
			vSaveToBackupRam(BKP_CONTI_RUN_H, (U16*)&drvKey1.contTime, sizeof(drvKey1.contTime)/sizeof(U16));
		}
		else if(stop_time < TIME_STOP_THRESHOLD)
		{
			stop_time++;
		}
		
	}

	// update vao bien log
	LogInfo.total_time = drvKey1.totalTime;
	LogInfo.cont_time = drvKey1.contTime;
}

void updateDriverKey(uint8_t *newDrv_id)
{
	drv_result_e res;
	U8 hh, mm, ss;
	U16 sec1, sec2;

	if(memcmp(newDrv_id, (S8*)drvKey1.id, DRIVER_FIELD_LENGTH)!=0){

		/* logout current driver */
		// get time logout
		vRTCGetTime(&hh, &mm, &ss);
		drvKey1.tmLogout.hour = hh;
		drvKey1.tmLogout.min = mm;
		drvKey1.tmLogout.sec = ss;
		drvKey1.relaxTime = stop_time;
		// save current driver.
		res = drvSaveData((S8*)drvKey1.id);
		vDriverDebug(res);

		// load new dirver.
		res = drvLoadData(newDrv_id);
		vDriverDebug(res);
		// error load file. create new drver.
		if(res == DRV_FILE_OPEN_FAIL)
		{
			// reset thong tin lien quan den lai xe.
			memset(&drvKey1, 0, sizeof(Driver_Data_t));
			// change id.
			memcpy(&drvKey1.id, newDrv_id, 8);
			// get time login
			vRTCGetTime(&hh, &mm, &ss);
			drvKey1.tmLogin.hour = hh;
			drvKey1.tmLogin.min = mm;
			drvKey1.tmLogin.sec = ss;
			// save to file
			drvSaveData((S8*)drvKey1.id);
		}
		// neu load file ok
		else if(res == DRV_FILE_OK)
		{
			// update time temp
			vRTCGetTime(&hh, &mm, &ss);
			// convert to sec.
			sec1 = hh*3600 + mm*60 + ss;
			sec2 = drvKey1.tmLogout.hour*3600 + drvKey1.tmLogout.min*60 + drvKey1.tmLogout.sec;
			// kiem tra xem da du thoi gian logout chua?
			sec2 = sec1 - sec2;
			drvKey1.relaxTime += sec2;

			// thoi gian nghi.
			if(drvKey1.relaxTime >= TIME_LOGOUT)
			{
				drvKey1.contTime = 0;
				drvKey1.totalTime = 0;
				drvKey1.relaxTime = 0;
				stop_time = TIME_STOP_THRESHOLD+1;
			}else
			{
				stop_time = drvKey1.relaxTime;
			}
		}
		vSaveToBackupRam(BKP_CONTI_RUN_H, (U16*)&drvKey1.contTime,
									sizeof(drvKey1.contTime)/sizeof(U16));
		vSaveToBackupRam(BKP_OVER_SPEED_H, (U16*)&drvKey1.overSpeed,
											sizeof(drvKey1.overSpeed)/sizeof(U16));

		vSaveToBackupRam(BKP_TOTAL_RUN_H, (U16*)&drvKey1.totalTime,
				sizeof(drvKey1.totalTime)/sizeof(U16));
		
		memcpy(LogInfo.id_drv, drvKey1.id, 8);

		vSaveToBackupRam(BKP_CURRENT_DRV_ID, (U16*)&drvKey1.id,
					sizeof(drvKey1.id)/sizeof(U16));
	};
}

/*
 * thay doi lai xe.
 */
void vCheckChangeKey(void)
{
	BOOL drvKeyStatus;
	S8 newDrv_id[8] = {0};

	/* Check driverKey */
	drvKeyStatus = ReadDriverInfo(newDrv_id);
	if (drvKeyStatus == DRIVER_LOAD_OK)
	{
		updateDriverKey(newDrv_id);
		drvPlugIn = 'Y';
	}else
	{
		vDrvInitI2CBus();
		drvPlugIn = 'N';
	}
}

void vResetDataInDay(void)
{
	tt_tmo = 0;
	nonstop_tmo = 0;

	drvKey1.totalTime = 0;
	drvKey1.overSpeed = 0;
	drvKey1.contTime = 0;
	drvKey1.relaxTime = 0;
	vSaveToBackupRam(BKP_CONTI_RUN_H, (U16*)&drvKey1.contTime,
					sizeof(drvKey1.contTime)/sizeof(U16));
	vSaveToBackupRam(BKP_TOTAL_RUN_H, (U16*)&drvKey1.totalTime,
					sizeof(drvKey1.totalTime)/sizeof(U16));
	vSaveToBackupRam(BKP_OVER_SPEED_H, (U16*)&drvKey1.overSpeed,
					sizeof(drvKey1.overSpeed)/sizeof(U16));
}

/*
 * save thong tin lai xe vao file tren the nho.
 */
drv_result_e drvSaveData(S8* drv_id)
{
	U16 ret_len;
	// open file with name is drv_id.drv
	memcpy(drv_fn + 4, drv_id, 8);
	f_dr = fopen((const char*)drv_fn, "wb");
	if(f_dr == NULL)
		return DRV_FILE_OPEN_FAIL;

	ret_len = fwrite((S8*)&drvKey1, sizeof(uint8_t), sizeof(Driver_Data_t), f_dr);
	if(ret_len != sizeof(Driver_Data_t))
	{
		fclose(f_dr);
		return DRV_FILE_SAVE_FAIL;
	}

	// close file
	fclose(f_dr);

	return DRV_FILE_OK;
}

/*
 * load thong tin lai xe.
 */
drv_result_e drvLoadData(S8* drv_id)
{
	U16 ret_len;
	// open file with name is drv_id.drv
	memcpy(drv_fn + 4, drv_id, 8);
	f_dr = fopen((const char*)drv_fn, "rb");
	if(f_dr == NULL)
		return DRV_FILE_OPEN_FAIL;

	ret_len = fread((S8*)&drvKey1, sizeof(uint8_t), sizeof(Driver_Data_t), f_dr);
	if(ret_len != sizeof(Driver_Data_t))
	{
		fclose(f_dr);
		return DRV_FILE_LOAD_FAIL;
	}

	// close file
	fclose(f_dr);

	return DRV_FILE_OK;
}

/*
 * \brief 	uint8_t getDriverPlugIn(void)
 */
uint8_t getDriverPlugIn(void)
{
	return drvPlugIn;
}
