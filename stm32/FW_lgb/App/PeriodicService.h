#ifndef _PERIODIC_SERVICE_
#define _PERIODIC_SERVICE_

/* Global macros --------------------------------- */

/* IO Configuration */
#define A_C_GPIO_PORT			GPIOE
#define DOOR_GPIO_PORT			GPIOE
#define ENGINE_GPIO_PORT		GPIOE

#define	IO_PORT					GPIOE

#define	ENGINE_GPIO_PIN			GPIO_Pin_8
#define	DOOR_GPIO_PIN			GPIO_Pin_9
#define	A_C_GPIO_PIN			GPIO_Pin_10
#define	IO_IN3_PIN				GPIO_Pin_11
#define	IO_IN4_PIN				GPIO_Pin_12
#define	IO_IN5_PIN				GPIO_Pin_13
#define	IO_OUT1_PIN				GPIO_Pin_14
#define	IO_OUT2_PIN				GPIO_Pin_15

#define	INTERVAL_1SEC			1000
#define INTERVAL_10SEC			10000


/* GPS Macros */
#ifndef		PERIODIC_DEBUG

/* Normal Operation Parameters */
#define 	MAX_SPEED_GPS		800		// gia tri van toc gps dc luu trong memory = vtoc thuc * 10.
#define 	MAX_SPEED_PULSE		79		//

#define 	MAX_TIME_RUNNING 	36000 // = 8*3600 = 10h.
#define 	CONT_DRV_TIM_OVER 	14400 // = 4*3600(s).

//#define 	CONTPLAY_TMO		60 // 60s.
//#define 	TTLPLAY_TMO			60 // 60s

#define 	TIME_OUT_SPEED		30	// 30 s

#define 	TIME_VEHICLE_OFF_THRESHODL		900		// second
#define 	TIME_VEHICLE_STOP_THRESHODL		180		// second

#define 	UPDATE_FW_TIMEOUT	12		// x*10(s)

#define WDT_TSK_1SEC_MAX_TIMING			100	 //60x800 = 48 s
#define WDT_TSK_SAVE_MAX_TIMING			375 //WDT_DEFAULT_MAX_TIMING	

#else

/* Debug Operation Parameters */
#define 	MAX_SPEED_GPS		299		// gia tri van toc gps dc luu trong memory = vtoc thuc * 10.
#define 	MAX_SPEED_PULSE		79		//

//#define 	MAX_TIME_RUNNING 	1800	//36000 // = 10*3600 = 10h.
//#define 	CONT_DRV_TIM_OVER 	320

//#define 	CONTPLAY_TMO		60 // 60s.
//#define 	TTLPLAY_TMO			60 // 60s

#define 	TIME_OUT_SPEED		30
#define 	TIME_VEHICLE_STOP_THRESHODL		90		// second

#define 	UPDATE_FW_TIMEOUT	0xFFFF

#endif

/* Service API ------------------------------------------- */
void vPeriodicServiceInit(void);
OBJSTATE_E xGetObjState(void);

void vResetNewDayData(void);
uint8_t WDT_Check_Tsk1Sec_Service(void);
uint8_t WDT_Check_TskSave_Service(void);

#endif
