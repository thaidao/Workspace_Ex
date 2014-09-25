/* Include header file */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "Watchdog.h"


/* Local functions */
__task void vWatchdog_Task(void);

extern uint8_t WDT_Check_GSM_Service(void);
extern uint8_t WDT_Check_ClientRecv_Service(void);
extern uint8_t WDT_Check_Log2Buf_Service(void);
extern uint8_t WDT_Check_GpsRx_Service(void);
extern uint8_t WDT_Check_GsmTask_Service(void);
extern uint8_t WDT_Check_Tsk1Sec_Service(void);
extern uint8_t WDT_Check_TskSave_Service(void);
extern uint8_t WDT_Check_CamTask_Service(void);

/* Private variables ------------------------ */
__IO uint32_t LsiFreq = 40000;			// LSI Frequency

/*
 * @brief	Init watchdog module
 *
 */
void vIWDG_Init(void){

		U16 tmo = 0xFFFF;
/* Init IWDG Module */
	  /* IWDG timeout equal to 1000 ms (the timeout may varies due to LSI frequency
	     dispersion) */

	  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

	  /* IWDG counter clock: LSI/32 */
		while(IWDG_GetFlagStatus(IWDG_FLAG_PVU) != RESET)
		{
			tmo --;
			if(tmo == 0)
				break;
		}
	  IWDG_SetPrescaler(IWDG_Prescaler_32);

	  /* Set counter reload value to obtain 1000ms IWDG TimeOut.
	     Counter Reload Value = 1000ms /IWDG counter clock period
	                          = 1000ms / (LSI/32)
	                          = LsiFreq/32
	   */
	  tmo = 0xFFFF;
	  while(IWDG_GetFlagStatus(IWDG_FLAG_RVU) != RESET)
	  {
		  tmo --;
		  if(tmo == 0)
			break;
	  }
	  IWDG_SetReload(LsiFreq/32);

	  /* Reload IWDG counter */
	  IWDG_ReloadCounter();

	  /* Enable IWDG (the LSI oscillator will be enabled by hardware) */
	  IWDG_Enable();

/* Create WDG Task */
	  os_tsk_create(vWatchdog_Task, RTX_IDWDG_PRIORITY);
}

/*
 * @brief	Init watchdog module with no task
 *
 */
void vIWDG_InitNoTask(void){

/* Init IWDG Module */
	  /* IWDG timeout equal to 1000 ms (the timeout may varies due to LSI frequency
	     dispersion) */
	  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

	  /* IWDG counter clock: LSI/32 */
	  IWDG_SetPrescaler(IWDG_Prescaler_256);

	  /* Set counter reload value to obtain 1000ms IWDG TimeOut.
	     Counter Reload Value = 1000ms /IWDG counter clock period
	                          = 1000ms / (LSI/32)
	                          = LsiFreq/32
	   */
	  IWDG_SetReload(25*LsiFreq/256); //25s
	
	  /* Reload IWDG counter */
	  IWDG_ReloadCounter();

	  /* Enable IWDG (the LSI oscillator will be enabled by hardware) */
	  IWDG_Enable();
}


/* Watchdog task which run at every 1sec to kick dog */
__task void vWatchdog_Task(void){
	// Set inter val = 1sec
	os_itv_set(800/OS_TICK_RATE_MS);			// 30msec

	for(;;){

		/* check GSM service */
		if( (WDT_Check_GSM_Service() == 0)
			&&(WDT_Check_ClientRecv_Service() == 0)
			&&(WDT_Check_Log2Buf_Service() == 0)
			&&(WDT_Check_GpsRx_Service() == 0)
			&&(WDT_Check_GsmTask_Service() == 0)
			&&(WDT_Check_Tsk1Sec_Service() == 0)
			&&(WDT_Check_TskSave_Service() == 0)
			&&(WDT_Check_CamTask_Service() == 0))
		{
			IWDG_ReloadCounter();
		}
	
		os_itv_wait();		
	}
}
