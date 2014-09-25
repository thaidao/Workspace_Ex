/**
 * @file 	main_bootloader.c
 * @author	Eposi Embedded Team
 * @version V1.0.0
 * @brief	Main source for GH12 Bootloader
 */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"			// Realtime RTX Library header file
#include "STM32_Init.h"
#include "stdio.h"
#include "FWUpgrade.h"
#include "Watchdog.h"
#include "serial.h"

/* Private macros ---------------------------- */


/* System flags */
#define BKP_FW_STATUS				BKP_DR4				/* Firmware Update Status */
#define BKP_FW_LEN_H				BKP_DR5				// First 16bit of data len
#define BKP_FW_LEN_L				BKP_DR6				// Second 16bit of data len

#define NEW_FW_AVAILABLE_FLAG		0x1111
#define NO_FW_AVAILABLE_FLAG		0x1010

/* Private types ----------------------------- */
typedef  void (*pFunction)(void);


/* Main program ------------------------------ */
int main(void){
pFunction Jump_To_Application;
uint32_t JumpAddress;
uint32_t FW_Size;											/* Firmware size */
uint8_t count;
FWUpgrade_Error_t FWStatus;
uint16_t status;

/* Init the system */
	stm32_Init();

/* Check status and perform copy Flash data */
	/* Init BKP Module */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);

	/* Perform copy flash */
	status = BKP_ReadBackupRegister(BKP_FW_STATUS);
	if(status == NEW_FW_AVAILABLE_FLAG){

		dbg_print("Upgrading firmware ...\r\n");

		/* Initialize microSD Driver */
		count = 10;
		while(finit(NULL) != 0){
			if(!(count--)){
				dbg_print("Initialization failed.\r");
				//return;

				// @todo: Reboot system
				vIWDG_Init();
			};
		};
		
		/* Upgrade firmware */ 		
		FW_Size = ((uint32_t) BKP_ReadBackupRegister(BKP_FW_LEN_H) << 16) | BKP_ReadBackupRegister(BKP_FW_LEN_L);

		FWStatus = FWUpgrade(FW_Size);
		if((FWStatus != FW_UPGRADE_OK) | (FWStatus != FW_FILE_NOT_FOUND)){

			// @todo: reboot to do again
			vIWDG_Init();
		};

		/* Write back status */
		BKP_WriteBackupRegister(BKP_FW_STATUS, NO_FW_AVAILABLE_FLAG);
	}

/* Jump to application */
	dbg_print("Jumping to app address ...\r\n");

    /* Jump to user application */
    JumpAddress = *(__IO uint32_t*) (ApplicationAddress + 4);
    Jump_To_Application = (pFunction) JumpAddress;
    /* Initialize user application's Stack Pointer */
    __set_MSP(*(__IO uint32_t*) ApplicationAddress);
    Jump_To_Application();

	while(1);
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	//uint8_t text[32];
	//sprintf(text, "file:%d, line:%u", file, line);
	//vCOMM_PutsQueue(text);
  /* Infinite loop */
  while (1)
  {
  }
}

#endif
