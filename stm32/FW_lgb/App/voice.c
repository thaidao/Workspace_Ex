/**
  ******************************************************************************
  * @file SpeexVocoder_STM32F103_STK/src/vocoder.c
  * @author   MCD Application Team
  * @version  V2.0.0
  * @date     04/27/2009
  * @brief    This file provides all the vocoder firmware functions.
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


/* Includes ------------------------------------------------------------------*/
#include "voice.h"

#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#include <speex/speex.h>
#include "arch.h"

#include "debug.h"

#include "stdio.h"
#include <string.h>
#include <rtl.h>
#include "buzzer.h"
#include "loadConfig.h"

#include "Platform_Config.h"

/** @addtogroup SpeexVocoder_STM32F103_STK
  * @{
  */


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
__IO int16_t IN_Buffer[2][FRAME_SIZE];
__IO int16_t OUT_Buffer[2][FRAME_SIZE];
__IO int16_t *inBuffer = IN_Buffer[0];
__IO int16_t *outBuffer = OUT_Buffer[0];
__IO uint8_t Start_Decoding = 0;
//__IO uint8_t Playing = 0;
SpeexBits bits; 	/* Holds bits so they can be read and written by the Speex routines */
void *dec_state;	/* Holds the states of the encoder & the decoder */
/* SPEEX PARAMETERS, MUST REMAINED UNCHANGED */
int speex_decode_enh = 1;
//int quality = 4, complexity=1, vbr=0;
char out_bytes[ENCODED_FRAME_SIZE];
char input_bytes[ENCODED_FRAME_SIZE];

//S8 tskVoiceStack[4000];
static uint64_t tskVoiceStack[4000/8];

FILE *fileSXR;
int iReaded = 0;

/* Mailbox */
#define VOICE_MBX_SIZE		1
#define VOICE_MSG_SIZE		20

#define RTX_ALARM_EVENT		0x0001

os_mbx_declare(VoiceMsgMbx, VOICE_MBX_SIZE);
_declare_box(VoicePool, VOICE_MSG_SIZE, VOICE_MBX_SIZE);

/* Private function prototypes -----------------------------------------------*/
void Voice_Start(void);
void Voice_Stop(void);
void Voice_Init(void);

OS_TID tidAlarm;
buzzer_obj_t buzzerObj;

/* Constant */
const uint8_t* VoiceFileList[] = {
		"WARN0001.SXR",
		"WARN0002.SXR",
		"WARN0002.SXR"

		"WARN0002.SXR",	   		// ALARM_STARTUP
		"WARN0002.SXR",			// ALARM_LOST_GSM_CONNECT
		"WARN0002.SXR",			// ALARM_GSM_CONNECTED
		"WARN0002.SXR",			// ALARM_SDCARD_ERROR

		"WARN0002.SXR",			// ALARM_OVERSPEED_USER
};

const buzzer_obj_t BuzzerList[] = {
		{100, 3, 1000, 1 },		// Alarm OverSpeed
		{200, 2, 1000, 1 },		// Alarm Overtime
		{100, 5, 1000, 1 },
		{100, 2, 500, 1},		// start up

		/* new alarm */
		{20, 1, 100, 2},			// lost gsm connection
		{20, 1, 2000, 1}, 		// connection success.
		{20, 1, 50, 4}, 		// sdcard error.
		{400, 1, 500, 3},		// ALARM_OVERSPEED_USER
		{100, 1, 300, 4},		// alarm change driver ID - success.
		{100, 1, 200, 5}		// alarm change driver ID - fail.
};

/* Private functions ---------------------------------------------------------*/
void PlayVoiceFile(alarm_type_t alarmType){
uint8_t* voiceFileName;

	/* Play buzzer */
	if(Config.option.playVoice == DISABLE){
		memcpy(&buzzerObj, &BuzzerList[alarmType], sizeof(buzzer_obj_t));
		os_evt_set(RTX_ALARM_EVENT, tidAlarm);
		return;
	}

	/* Play voice */
	if(Config.option.playVoice == ENABLE){
		voiceFileName = _alloc_box(VoicePool);
		if(voiceFileName != NULL){
			memcpy(voiceFileName, VoiceFileList[alarmType], strlen(VoiceFileList[alarmType])+1);
			os_mbx_send(VoiceMsgMbx, voiceFileName, 0xffff);
		};
	};
}

/* RTX Task ------------------------------------------------------------------- */

/* Buzzer task */
__task void taskBuzzer(void){
OS_RESULT result;

	/* Init hardware */
	vBuzzer_Init();

	vBuzzerPlay(BuzzerList[3]);

	for(;;){
		/* Wait event */
		result = os_evt_wait_or(RTX_ALARM_EVENT, 0xffff);
		if(result == OS_R_EVT){
			vBuzzerPlay(buzzerObj);
		};
	}
}

/* Voice task */
__task void taskVoice(void) {
OS_RESULT result;
uint8_t* voiceFileName;
char SXRfilename[30];

	/* Init hardware */
	//Voice_HardwareInit();

	/* Init mailbox */
	os_mbx_init(VoiceMsgMbx, sizeof(VoiceMsgMbx));
	_init_box(VoicePool, sizeof(VoicePool), VOICE_MSG_SIZE);

   	for(;;){
		result = os_mbx_wait(VoiceMsgMbx, &voiceFileName, 0x0000);
		if (result == OS_R_OK) {
			iReaded = 0;

			sprintf(SXRfilename, "M:\\SXR\\%s",voiceFileName);

			fileSXR = fopen (SXRfilename, "r");            /* Open a file from default drive. */
			if (fileSXR != NULL)  {
			    /* we prepare two buffers of decoded data: */
			    /* the first one, */
				iReaded = fread (&input_bytes[0], 1, ENCODED_FRAME_SIZE, fileSXR);
			    speex_bits_read_from(&bits, input_bytes, ENCODED_FRAME_SIZE);
			    speex_decode_int(dec_state, &bits, (spx_int16_t*)OUT_Buffer[0]);

			    /* and the second one. */
				iReaded = fread (&input_bytes[0], 1, ENCODED_FRAME_SIZE, fileSXR);
			    speex_bits_read_from(&bits, input_bytes, ENCODED_FRAME_SIZE);
			    speex_decode_int(dec_state, &bits, (spx_int16_t*)OUT_Buffer[1]);

			    Voice_Start();

//				Playing = 1;
			    //os_itv_set(10);	  // 10ms or 10 ticks
			    /* Now we wait until the playing of the buffers to re-decode ...*/
			    while(iReaded > 0 ) {
					//GPIO_SetBits(GPIOE, GPIO_Pin_14);
					if (Start_Decoding > 0) {
						memset (&input_bytes[0], 0, ENCODED_FRAME_SIZE);
						iReaded = fread (&input_bytes[0], 1, ENCODED_FRAME_SIZE, fileSXR);

						if (iReaded > 0) {
							/* Copy the encoded data into the bit-stream struct */
							speex_bits_read_from(&bits, input_bytes, ENCODED_FRAME_SIZE);
							/* Decode the data */
							speex_decode_int(dec_state, &bits, (spx_int16_t*)OUT_Buffer[Start_Decoding-1]);
						}
						Start_Decoding = 0;
					}
					//GPIO_ResetBits(GPIOE, GPIO_Pin_14);
			    }

				// Comment this line to Prevent the noise after playing
			    //Voice_Stop();


				//Playing = 0;
			    inBuffer = IN_Buffer[0];
			    outBuffer = OUT_Buffer[0];

			    /* Clear all OUT_Buffer */
			    memset(OUT_Buffer[0], 0, sizeof(OUT_Buffer[0]));
			    memset(OUT_Buffer[0], 0, sizeof(OUT_Buffer[1]));

				fclose (fileSXR);
			}

			// Free Pool
			_free_box(VoicePool, voiceFileName);
		}
	}
}

/* IRQ Handler -------------------------------------------------------------------- */

/**
  * @brief  This function handles TIM2 interrupt request.
  * @param  None
  * @retval : None
  */
void TIM2_IRQHandler(void){
static uint8_t spin = 0;

//	if (Playing) {
		TIM2->ARR = TIM2ARRValue;			/* Relaod output compare */
		TIM2->SR = TIM_INT_Update;			/* Clear TIM2 update interrupt */

		spin=spin^1;

		if(spin) {
			TIM5->CCR1 = ((*outBuffer>>6)) + 0x200 ;

			if(outBuffer == &OUT_Buffer[1][159]) {
				outBuffer = OUT_Buffer[0];
				Start_Decoding = 2;
			} else if(outBuffer == &OUT_Buffer[0][159]) {
				outBuffer++;
				Start_Decoding = 1;
			} else {
				outBuffer++;
			}
		}
//	}
}


/**
  * @brief  This function handles TIM5 interrupt request.
  * @param  None
  * @retval : None
  */
void TIM3_IRQHandler(void) {
//	TIM3->ARR = TIM5ARRValue;				/* Relaod output compare */
//	TIM3->SR = TIM_INT_Update;				/* Clear TIM2 update interrupt */
//
//	if(Playing) {
//		TIM1->CCR1 = ((*outBuffer>>6)) + 0x200 ;
//
//		if(outBuffer == &OUT_Buffer[0][159]) {
//			Start_Decoding = 1;
//			outBuffer++;
//		} else if(outBuffer == &OUT_Buffer[1][159]) {
//			outBuffer = OUT_Buffer[0];
//			Start_Decoding = 2;
//		} else {
//			outBuffer++;
//		}
//	}
}

/* Utilities ----------------------------------------------------------- */


void Voice_HardwareInit(void) {
	/*******************************************************/
	/* TIM2 and TIM3 clocks enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM3 , ENABLE);

	/* Enable GPIOA, GPIOC, AFIO and TIM5 clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA| RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

	/*******************************************************/
	/* Interrupt Configuration */
	/* 1 bit for pre-emption priority, 3 bits for subpriority */
	NVIC_SetPriorityGrouping(6);

	 /* Enable the TIM2 Interrupt */
	NVIC_SetPriority(TIM2_IRQn, 0x00); /* 0x00 = 0x01 << 3 | (0x00 & 0x7*/
	NVIC_EnableIRQ(TIM2_IRQn);
	/* Enable the TIM3 Interrupt */
	NVIC_SetPriority(TIM3_IRQn, 0x00); /* 0x00 = 0x01 << 3 | (0x00 & 0x7*/
	NVIC_EnableIRQ(TIM3_IRQn);

	//
	Voice_Init();
}

/**
  * @brief  Initializes the speex codec
  * @param  None
  * @retval : None
  */
void Speex_Init(void) {
	/*******************************************************/
	/* Speex encoding initializations */
	speex_bits_init(&bits);

	/* speex decoding intilalization */
	dec_state = speex_decoder_init(&speex_nb_mode);
	speex_decoder_ctl(dec_state, SPEEX_SET_ENH, &speex_decode_enh);

	/*******************************************************/

	//os_tsk_create(taskVoice, RTX_VOICE_PRIORITY);
	/* Create task with a bigger stack */
	if(Config.option.playVoice == ENABLE){
		tidAlarm = os_tsk_create_user (taskVoice, RTX_VOICE_PRIORITY, &tskVoiceStack, sizeof(tskVoiceStack));
	} else {
		tidAlarm = os_tsk_create_user (taskBuzzer, RTX_VOICE_PRIORITY, &tskVoiceStack, sizeof(tskVoiceStack));
	}

}

/* Task Buzzer Alarm */


/**
  * @brief  Initializes Voice recording/playing
  * @param  None
  * @retval : None.
  */
void Voice_Init(void) {
	/* Peripherals InitStructure define -----------------------------------------*/
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;

	/* TIM5 configuration -------------------------------------------------------*/
	TIM_DeInit(TIM5);
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	GPIO_StructInit(&GPIO_InitStructure);

	/* Configure PA.08 as alternate function (TIM1_OC1) */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* TIM5 used for PWM genration */
	TIM_TimeBaseStructure.TIM_Prescaler = 0x00; /* TIM1CLK = 72 MHz */
	TIM_TimeBaseStructure.TIM_Period = 0x3FF; /* 10 bits resolution */
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);

	/* TIM5's Channel1 in PWM1 mode */
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	//TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0x200;/* Duty cycle: 50%*/
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
	//TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCIdleState_Reset;

	TIM_OC1Init(TIM5, &TIM_OCInitStructure);

	TIM_OC1PreloadConfig(TIM5, TIM_OCPreload_Enable);
	TIM_ARRPreloadConfig(TIM5, ENABLE);

	/* TIM2 configuration -------------------------------------------------------*/
	TIM_DeInit(TIM2);
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	/* TIM2 used for timing, the timing period depends on the sample rate */
	TIM_TimeBaseStructure.TIM_Prescaler = 0x00;    /* TIM2CLK = 72 MHz */
	TIM_TimeBaseStructure.TIM_Period = TIM2ARRValue;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	/* Output Compare Inactive Mode configuration: Channel1 */
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Inactive;
	TIM_OCInitStructure.TIM_Pulse = 0x0;
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);

	/* TIM3 configuration -------------------------------------------------------*/
	TIM_DeInit(TIM3);
	TIM_OCStructInit(&TIM_OCInitStructure);
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	/* TIM5 used for timing, the timing period depends on the sample rate */
	TIM_TimeBaseStructure.TIM_Prescaler = 0x00;    /* TIM5CLK = 72 MHz */
	TIM_TimeBaseStructure.TIM_Period = TIM5ARRValue;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0x0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

	/* Output Compare Inactive Mode configuration: Channel1 */
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_Inactive;
	TIM_OCInitStructure.TIM_Pulse = 0x0;
	TIM_OC1Init(TIM3, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM5, TIM_OCPreload_Disable);
}

/**
  * @brief  Start playing
  * @param  None
  * @retval : None
  */
void Voice_Start(void) {
	TIM_Cmd(TIM5, ENABLE);						/* TIM5 counter enable */
	//TIM_CtrlPWMOutputs(TIM5, ENABLE);			/* TIM5 Main Output Enable */
	TIM2->ARR = TIM2ARRValue;					/* Relaod ARR register */
	TIM2->CR1 |= CR1_CEN_Set;					/* Enable the TIM Counter */
	TIM2->SR = (uint16_t)TIM_INT_Update;		/* Clear the IT pending Bit */
	TIM2->DIER |= TIM_IT_Update;				/* Enable TIM2 update interrupt */
}

/**
  * @brief  Stop vocoder
  * @param  None
  * @retval : None
  */
void Voice_Stop(void) {
	TIM_Cmd(TIM5, DISABLE);						/* TIM5 counter enable */
	//TIM_CtrlPWMOutputs(TIM5, DISABLE);		/* TIM5 Main Output Enable */
	TIM_Cmd(TIM2, DISABLE);						/* Stop TIM2 */
	TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);	/* Disable TIM2 update interrupt */
}

/**
  * @brief  Start voice playing
  * @param  None
  * @retval : None
  */
void Voice_Playing_Start(void) {
	TIM_Cmd(TIM5, ENABLE);						/* TIM5 counter enable */
	TIM_CtrlPWMOutputs(TIM5, ENABLE);			/* TIM5 Main Output Enable */
	TIM3->ARR = TIM5ARRValue;					/* Relaod ARR register */
	TIM3->CR1 |= CR1_CEN_Set;					/* Enable the TIM Counter */
	TIM3->SR = (uint16_t)TIM_INT_Update;		/* Clear the IT pending Bit */
	TIM3->DIER |= TIM_IT_Update;				/* Enable TIM5 update interrupt */
}

/**
  * @brief  Ovveride the _speex_putc function of the speex library
  * @param  None
  * @retval : None
  */
void _speex_putc(int ch, void *file) {
	while(1);
}

/**
  * @brief  Ovveride the _speex_fatal function of the speex library
  * @param  None
  * @retval : None
  */
void _speex_fatal(const char *str, const char *file, int line) {
	while(1);
}

/**
  * @}
  */


/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
