/**
  ******************************************************************************
  * @file    main.c
  * @author  Eposi Embedded Team
  * @version V1.0.0
  * @date    19-Dec-2011
  * @brief   Main program body
  ******************************************************************************
 */ 

/* Includes ------------------------------------------------------------------*/
#include "main.h"
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
#include "PeriodicService.h"
#include "loadConfig.h"
#include "voice.h"

#include "sim900.h"
#include "sim18.h"
#include "drvkey.h"
#include "buzzer.h"
#include "debug.h"
#include "core_cm3.h"
#include "SystemMonitor.h"
#include "camera.h"
#include "led.h"

#ifdef 		_TELCOM_TEST
#include "boardtest.h"
#endif

/* Private macro -------------------------------------------------------------*/ 
__task void MainTask(void);

/* Private functions ---------------------------------------------------------*/

/* Private variables ----------------------------------------------------------*/
static U64 tskMainStack[500/8];

extern U8 sdCard_st;




#ifdef  TESTING_VOICE
__task void taskVoice1(void);
__task void taskVoice2(void);
#endif
//S8 *printtext = " \"+841692128985\",\"\",\"12/02/03,22:56:23+28\"\r\n@Vt 270212 000001!\r\n";

#if _TEST_MUTEX
__task void task1 (void);
__task void task2 (void) ;
#endif

void vDelay(U32 t)
{
	for(;t>0;t--);
}

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
int main(void) {  
int8_t res;
 	
	/* Change vector table to 0x800 3000 */
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, ApplicationAddressOffset);

	/* Init System: Clock, GPIO, ... */
	//vIWDG_InitNoTask();

	stm32_Init();  

	vSIM90_UART_Init();
	vSIM18_UART_Init();
	vDriverInit();
	vCAM_UART_Init();		
	vLGB_UART_Init();

	vDelay(720000);

	/* Load config from SDCard */
	res = config_load(&Config);
	if(res == -1){
		// @todo Move from default config to Config
		memcpy(&Config, &Config_Default, sizeof(GH12_Config_t));
		config_apply((GH12_Config_t*) &Config_Default); 		
	}

	/* Voice GPIO */
	Voice_HardwareInit();

	//Kickdog after 25s 
	IWDG_ReloadCounter();

	os_sys_init_user (MainTask, RTX_MAINTASK_PRIORITY, &tskMainStack, sizeof(tskMainStack));			/* Start RTX Scheduler */

	while(1);  
}  

/**
 * @fn		__task void MainTask(void)
 * @brief	Main task which init all other tasks, then kill itself
 */

__task void MainTask(void) {
int8_t res;  	

	/* Init Watchdog tasks */																			 
	//vIWDG_Init();

	/* Init RTC and backup RAM */
	vRTCInit();

	/* init sdcard */
	res = vSDCardInit();
	if(res == -1){
		// @todo Change system state to no buffer
		sdCard_st = 1;
	};

	// check status reset
	SystemReport();
	
	// Debug init
	debug_init();

	/* Test buffer */
#ifdef SDFIFO_TEST_ENABLE
	sdfifo_Test();
	os_tsk_delete_self();
#endif
	
	led_driver_init();

	/* Voice task */
	Speex_Init();

#ifdef 		USE_PERIODIC_TASK
	/* Create tasks */
	vPeriodicServiceInit();
#endif

#ifdef		USE_GPS_MODULE
	vGPS_Init();
#endif

#ifdef		USE_GPRS_MODULE
	vGPRS_Init();
#endif

#ifdef 		_TELCOM_TEST
	vTelCom_Init();
#endif

#if _TEST_MUTEX
	os_tsk_create(task1, 3);
	os_tsk_create(task2, 3);
#endif

	/* Self delete task */
	os_tsk_delete_self();
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line) { 
  /** User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	uint8_t text[32];
	sprintf(text, "file:%d, line:%u", file, line);
	//vCOMM_PutsQueue(text);

  	while (1);				  	/* Infinite loop */
}

#endif


#if _TEST_MUTEX

OS_MUT mutex1;
void f2 (void);
OS_RESULT res;
char string[32];

void f1 (void) {
  res = os_mut_wait (&mutex1, 0xffff);

  /* Critical region 1 */
  USART_PrCStr(USART1, "\r\nCall functions 1");
  sprintf(string, "\r\nMutex f1: %u", res);
  USART_PrCStr(USART1, string);

  /* f2() will not block the task. */
  //f2 ();
  os_mut_release (&mutex1);
}

void f2 (void) {
  res = os_mut_wait (&mutex1, 0xffff);

  /* Critical region 2 */
  USART_PrCStr(USART1, "\r\nCall functions 2");
  sprintf(string, "\r\nMutex f2: %u", res);
  USART_PrCStr(USART1, string);

  os_mut_release (&mutex1);
}

__task void task1 (void) {

  os_mut_init (&mutex1);
  for(;;)
  {
	  USART_PrCStr(USART1, "\r\nTASK 1");
	  f1 ();
	  //os_dly_wait(1000);
  }

}

__task void task2 (void) {

	  for(;;)
	  {
		  if(mutex1 != NULL)
		  {
			  USART_PrCStr(USART1, "\r\nTASK 2");
			  f2 ();
		  }
		  //os_dly_wait(1000);
	  }

}

#endif
