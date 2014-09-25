/* Include header file */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "Watchdog.h"

/* Private variables ------------------------ */
__IO uint32_t LsiFreq = 40000;			// LSI Frequency

/*
 * @brief	Init watchdog module
 *
 */
void vIWDG_Init(void){

/* Init IWDG Module */
	  /* IWDG timeout equal to 2000 ms (the timeout may varies due to LSI frequency
	     dispersion) */
	  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

	  /* IWDG counter clock: LSI/32 */
	  IWDG_SetPrescaler(IWDG_Prescaler_32);
	  IWDG_SetReload(LsiFreq/64);

	  /* Reload IWDG counter */
	  IWDG_ReloadCounter();

	  /* Enable IWDG (the LSI oscillator will be enabled by hardware) */
	  IWDG_Enable();
}
