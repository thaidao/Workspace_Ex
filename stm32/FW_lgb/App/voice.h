/**
  ******************************************************************************
  * @file SpeexVocoder_STM32F103-STK/inc/vocoder.h 
  * @author  MCD Application Team
  * @version  V2.0.0
  * @date  04/27/2009
  * @brief  This file contains all the functions prototypes for the 
  *         vocoder firmware library.
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2009 STMicroelectronics</center></h2>
  */ 


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __VOICE_H
#define __VOICE_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x.h"
#include "rtl.h"			// Realtime RTX Library header file

/* Voice macros ------------------------------------------------------------- */
#define	MICROSD_VOIDE_FOLDER	"M:\\SXR\\"
//#define VOICE_FILE_OVERSPEED	"WARN0001.SXR"
//#define VOICE_FILE_OVERTIME		"WARN0002.SXR"

typedef enum {
	ALARM_OVERSPEED,
	ALARM_OVER_CONTINUOUS_TIME,
	ALARM_OVER_DRIVING_TIME,
	ALARM_STARTUP,
	ALARM_LOST_GSM_CONNECT,
	ALARM_GSM_CONNECTED,
	ALARM_SDCARD_ERROR,
	ALARM_OVERSPEED_USER,
	ALARM_DRV_LOGON_OK,
	ALARM_DRV_LOGON_FAIL
} alarm_type_t;

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define TIM2ARRValue            4500 /* TIM2 @16KHz */
#define TIM5ARRValue            9000 /* sampling rate = 8KHz => TIM5 period =72MHz/8KHz = 9000 */
#define TIM_INT_Update          ((uint16_t)(~TIM_IT_Update))
#define CR1_CEN_Set             ((uint16_t)0x0001)
/*60seconds contain 3000 frames of 20ms, and every 20ms will be encoded into 20bytes so 1min=60000bytes */
#define ENCODED_FRAME_SIZE      20

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

/** call me in the main function, before initing RTX */
void Voice_HardwareInit(void);
void PlayVoiceFile(alarm_type_t);
void Speex_Init(void);
__task void taskVoice(void);

#endif /*__VOICE_H */

/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
