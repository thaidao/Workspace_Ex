/* @file		RTC.c
 * @brief		RTC API for RTC
 * @author		ngohaibac@gmail.com
 * @date		15/11/2011
 * @changelog	
 * 				V2. change to use with STM32F103
 * 			    V1. release simple API (15/11/2011)
 * @todo		add mutex, optimize setting input/output pins
 */
#include "rtl.h"
#include "stm32f10x.h"
#include "modules.h"
#include "loadconfig.h"
#include "rtc.h"


/* Private types --------------------------------- */
union date_t{
	struct{
		uint16_t day:5;		// 5 bits [0..31]
		uint16_t month:4;	// 4 bits [0..12]
		uint16_t year:7;	// 6 bits [01..99]
	} bit;

	uint16_t all;
} rtc_date;

union newday_u{
	struct
	{
		uint16_t intterupt:1;
		uint16_t rst_flag:1;
		uint16_t bus_flag:1;
		uint16_t io_report_flag:1;
	}newdayflags_t;

	uint16_t newday;
}newdayFlags;

uint8_t DayOfMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/* Private functions ----------------------------- */
static void vUpdateNewDay(void);
static void vUpdateDate(uint8_t val);

extern void vResetNewDayData(void);
/**
 * @brief	This function handlers RTC global interrupt request
 * @param	None
 * @retval 	None
 */
void RTC_IRQHandler(void){
uint8_t hour, min, sec;

	// RTC Second IRQ
	if(RTC_GetITStatus(RTC_IT_SEC) != RESET){
		/* Clear the RTC Second interrupt */
		RTC_ClearITPendingBit(RTC_IT_SEC);

		// Test
		vRTCGetDate(&hour, &min, &sec);
		DBG_PrChar(hour);
		DBG_PrChar(min);
		DBG_PrChar(sec);
	}

	/* Alarm date at 11:59:59, then update Date */
	if(RTC_GetITStatus(RTC_IT_ALR) != RESET){
		/* Clear the RTC Alarm flag */
		RTC_ClearITPendingBit(RTC_IT_ALR);

		// Update date for new day
		vUpdateNewDay();
	}
}

/**
 * @brief	Update data (day, month, year)
 * @param	None
 */
static void vUpdateNewDay(void){
	// Reset RTC counter
	RTC_WaitForLastTask();
	RTC_SetCounter(0);
	RTC_WaitForLastTask();

	vUpdateDate(1);
}

/**
 * @brief	Update date information when add day with val
 */
static void vUpdateDate(uint8_t val){
uint8_t day;

	day = rtc_date.bit.day + val;
	//rtc_date.day += val;
	while(day > DayOfMonth[rtc_date.bit.month]){
		day = day - DayOfMonth[rtc_date.bit.month];
		rtc_date.bit.month++;
		if(rtc_date.bit.month > 12){
			rtc_date.bit.month = 1;
			rtc_date.bit.year++;
			DayOfMonth[2] = (rtc_date.bit.year % 4 == 0) ? 29:28;
		}
	}
	// Update day value
	rtc_date.bit.day = day;
	// Update backup register
	BKP_WriteBackupRegister(BKP_RTC_DATE, rtc_date.all);

	/* neu qua ngay moi */
	if(val>0)
	{
		// event new day.
		vResetNewDayData();

		// Update logfile name
		vLog_createNewFile();

		/* reset camera index */
		if(Config.CAM.cam_used)
			vReSetImageCount();
		
		newdayFlags.newday = 0xFFFF;
	}

}

/* ---------------------------------------------------- */

/**
 * @brief	Init RTC Module, ..
 * @param	None
 */
void vRTCInit(void){
uint16_t temp;
uint32_t TimeVar;
uint8_t	day, month, year;

	/* Init RTC Module see in STM32_Init.c
	 * - Init RTC using LSE (32.768)
	 * - Set alarm at 23:59:59 and generate interrupt
	 */

/* Init BKP Module, and get data saved in BKP register */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
	BKP_ClearFlag();
	BKP_TamperPinCmd(DISABLE);
	BKP_ITConfig(DISABLE);

/* Get date value from BKP Register if it is saved */
	temp = BKP_ReadBackupRegister(BKP_RTC_DATE);
	if(temp != 0x00)
		rtc_date.all = temp;

/* Recalculate date value */
	TimeVar = RTC_GetCounter();

	// Set correct counter value
	RTC_WaitForLastTask();
	RTC_SetCounter(TimeVar%86400);
	RTC_WaitForLastTask();

	// Update date, and month 2
	DayOfMonth[2] = (rtc_date. bit.year % 4 == 0) ? 29:28;
	vUpdateDate(TimeVar/86400);
	DayOfMonth[2] = (rtc_date. bit.year % 4 == 0) ? 29:28;

	/* Validate date/time */
	vRTCGetDate(&day, &month, &year);
	// if day/month is out of range, then apply 01/01/2000
	if( (day==0) || (day>31) || (month == 0) || (month > 12) ){
		vRTCSetDate(1, 1, 0);
	}

}

/* Utility functions ------------------------------------- */

/* Set time value */
void vRTCSetTime(uint8_t hour, uint8_t min, uint8_t sec){
uint32_t CounterValue;
	CounterValue = (hour * 3600) + (min * 60) + sec;

	RTC_WaitForLastTask();
	RTC_SetCounter(CounterValue);
	RTC_WaitForLastTask();
}

/* Set date values */
void vRTCSetDate(uint8_t day, uint8_t month, uint8_t year){
uint8_t preDay, preMonth, preYear;

	/* Compare date */
	vRTCGetDate(&preDay, &preMonth, &preYear);
	if( (day != preDay) || (month != preMonth) || (year != preYear)){
		vResetNewDayData();
		vLog_createNewFile();
	}

	// Update current date values
	rtc_date.bit.day = day;
	rtc_date.bit.month = month;
	rtc_date.bit.year = year;

	// Update backup register
	BKP_WriteBackupRegister(BKP_RTC_DATE, rtc_date.all);

}

/* Get time from counter */
void vRTCGetTime(uint8_t *hour, uint8_t *min, uint8_t *sec){
uint32_t TimeVar = RTC_GetCounter();

	*hour = TimeVar/3600;
	TimeVar = TimeVar%3600;
	*min = TimeVar/60;
	*sec = TimeVar%60;
}

/* Get date from local variable */
void vRTCGetDate(uint8_t *day, uint8_t *month, uint8_t *year){

	*day = rtc_date.bit.day;
	*month = rtc_date.bit.month;
	*year = rtc_date.bit.year;
}


/* BACKUP RAM API ---------------------------------- */

/* Save data to Backup RAM */
void vSaveToBackupRam(U16 add, U16* data, U16 len)
{
	int i;

	for(i=0;i<len;i++)
	{
		BKP_WriteBackupRegister(add+(i<<2), *data++);
	}
}

/* Load data from given RAM location */
void vLoadFromBackupRam(U16 add, U16* data, U16 len)
{
	int i;
	for(i=0;i<len;i++)
	{
		*data++ = BKP_ReadBackupRegister(add+(i<<2));
	}
}

/*
 * reset all backup ram
 */
void vFactoryReset(void)
{
	int i;
	U16 add = BKP_DR1;

	for(i=0;i<10;i++)
	{
		BKP_WriteBackupRegister(add+(i<<2), 0x00);
	}
	add = BKP_DR11;

	for(i=0;i<32;i++)
	{
		BKP_WriteBackupRegister(add+(i<<2), 0x00);
	}
}


/*
 * \brief
 */
void RTC_ReSetNewdayFlag(uint16_t flag)
{
	newdayFlags.newday &= (~flag);
}

/*
 * \brief
 */
uint16_t RTC_GetNewdayFlag(uint16_t flag)
{
	uint16_t newday = newdayFlags.newday;

	return (newday & flag) ? 1:0;
}


void RTC_SetNewdayFlag(uint16_t flag)
{
	newdayFlags.newday |= (flag);
}

