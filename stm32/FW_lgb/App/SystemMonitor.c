/*
 * SystemMonitor.c
 *
 * author: hanv
 * date: 07/01/2013
 *
 * version 1.0
 * 		- read reset source register.
 * 		- store reset count to backup ram
 */

#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"			// Realtime RTX Library header file
#include "STM32_Init.h"

#include "File_Config.h"
#include "modules.h"		// Modules header file

#include "utils.h"
#include "serial.h"
#include "gpsModule.h"
#include "gprs.h"
#include "Watchdog.h"
#include "stdio.h"
#include "loadConfig.h"
#include "core_cm3.h"
#include "rtc.h"

#include "Systemmonitor.h"

FILE *fsys;
uint32_t sysflag = 0;

/*
 * \brief	ResultType_e SystemGetInfoInFile(SystemData_t *sysInfo)
 * 			Read file system.log. Get information as reset source, reset count, ...
 * \param	a point type SystemData_t which store information
 * \return	result type. see file .h for more details.
 */
ResultType_e SystemGetInfoInFile(SystemData_t *sysInfo)
{
	uint32_t readCount;
	uint8_t checksum, cal_Chk;

	// if new day, reset report.
//	if(RTC_GetNewdayFlag(RTC_RST_FLAG))
//	{
//		SystemMakeFile();
//		RTC_ReSetNewdayFlag(RTC_RST_FLAG);
//	}

	memset(sysInfo, 0, sizeof(SystemData_t));

	fsys = fopen("system.log", "rb");
	if(fsys == NULL)
	{
		sysInfo->reset_flag = sysflag;
		return OPEN_FILE_ERROR;
	}

	readCount = fread((char*)sysInfo, sizeof(char), sizeof(SystemData_t), fsys);
	// read checksum
	fread((char*)&checksum, sizeof(char), sizeof(char), fsys);
	// calculate checksum
	cal_Chk = cCheckSumFixSize((S8*)sysInfo, sizeof(SystemData_t));

	fclose(fsys);

	sysInfo->reset_flag = sysflag;

	if(readCount != sizeof(SystemData_t))
		return DATA_SIZE_ERROR;

	if(cal_Chk != checksum)
		return DATA_CHECKSUM_FAIL;

	return RESULT_OK;
}

/*
 * \brief	ResultType_e SystemSaveInfoInFile(SystemData_t *sysInfo)
 * 			Save file system.log. Get information as reset source, reset count, ...
 * \param	a pointer type SystemData_t point to data
 * \return	result type. see file .h for more details.
 */
ResultType_e SystemSaveInfoInFile(SystemData_t *sysInfo)
{
	uint32_t writeCount;

	uint8_t cal_Chk = cCheckSumFixSize((S8*)sysInfo, sizeof(SystemData_t));

	fsys = fopen("system.log", "wb");
	if(fsys == NULL)
		return OPEN_FILE_ERROR;

	writeCount = fwrite((char*)sysInfo, sizeof(char), sizeof(SystemData_t), fsys);
	// write checksum
	fwrite((char*)&cal_Chk, sizeof(char), sizeof(char), fsys);

	fclose(fsys);

	if(writeCount != sizeof(SystemData_t))
		return DATA_SIZE_ERROR;

	return RESULT_OK;
}


ResultType_e SystemMakeFile(void)
{
	SystemData_t SystemStatus;

	memset((char*)&SystemStatus, 0, sizeof(SystemStatus));

	return SystemSaveInfoInFile(&SystemStatus);
}

/*
 * \brief 	SystemReport(void)
 * 			Read reset source and counter reset number.
 * 	\param	none.
 * 	\return none
 */
void SystemReport(void)
{
	SystemData_t SystemStatus;

	// test the reset flags in order because the pin reset is always set.
	sysflag = (RCC_GetFlagStatus(RCC_FLAG_HSIRDY) << FLAG_HSIRDY) | \
			(RCC_GetFlagStatus(RCC_FLAG_HSERDY) << FLAG_HSERDY) | \
			(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) << FLAG_PLLRDY) | \
			(RCC_GetFlagStatus(RCC_FLAG_LSERDY) << FLAG_LSERDY) | \
			(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) << FLAG_LSIRDY) | \
			(RCC_GetFlagStatus(RCC_FLAG_PINRST) << FLAG_PINRST) | \
			(RCC_GetFlagStatus(RCC_FLAG_PORRST) << FLAG_PORRST) | \
			(RCC_GetFlagStatus(RCC_FLAG_SFTRST) << FLAG_SFTRST) | \
			(RCC_GetFlagStatus(RCC_FLAG_IWDGRST) << FLAG_IWDGRST) | \
			(RCC_GetFlagStatus(RCC_FLAG_WWDGRST) << FLAG_WWDGRST) | \
			(RCC_GetFlagStatus(RCC_FLAG_LPWRRST) << FLAG_LPWRRST);
	// The flags must be cleared manually after use
	RCC_ClearFlag();

	// load system info in file system.log
	SystemGetInfoInFile(&SystemStatus);

	// increase total reset
	SystemStatus.reset_count ++;
	// increase reset resource count
	if(SystemStatus.reset_flag & (1<< FLAG_LSIRDY))
		SystemStatus.lsiCnt ++;
	if(SystemStatus.reset_flag & (1<< FLAG_PINRST))
		SystemStatus.pinCnt ++;
	if(SystemStatus.reset_flag & (1<< FLAG_PORRST))
		SystemStatus.porCnt ++;
	if(SystemStatus.reset_flag & (1<< FLAG_SFTRST))
		SystemStatus.sftCnt ++;
	if(SystemStatus.reset_flag & (1<< FLAG_IWDGRST))
		SystemStatus.iwdCnt ++;
	if(SystemStatus.reset_flag & (1<< FLAG_WWDGRST))
		SystemStatus.wwdCnt ++;
	if(SystemStatus.reset_flag & (1<< FLAG_LPWRRST))
		SystemStatus.lpwCnt ++;

	SystemSaveInfoInFile(&SystemStatus);
}

/*
 * \brief	SystemReportClear(void)
 * 			Clear reset count,
 * 	\param	none.
 * 	\return none.
 */
void SystemReportClear(void)
{
	BKP_WriteBackupRegister(BKP_SYSTEM_RESET_COUNT, 0);
}

