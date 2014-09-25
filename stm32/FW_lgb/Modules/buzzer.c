/* Include header files */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "string.h"
#include "buzzer.h"
#include "Platform_Config.h"

/* Private macros */
#define BUZZER_ON()			GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN)
#define BUZZER_OFF()			GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN)

/* Init Buzzer module */
void vBuzzer_Init(void){
GPIO_InitTypeDef GPIO_InitStructure;

	/* Enable buzzer GPIO clock */
	RCC_APB2PeriphClockCmd(BUZZER_GPIO_CLK , ENABLE);

	/* Config Buzzer as output */
	GPIO_InitStructure.GPIO_Pin = BUZZER_GPIO_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStructure);
}

/**
 * @brief	Play buzzer
 * @param	period		Period of buzzer sound
 * @param	waitime		Delay between each alarm
 * @param	playCount	how many times it plays
 */
void vBuzzerPlay(buzzer_obj_t buzzerObj){
uint8_t idx;

	while(buzzerObj.repeatCount--){
		for(idx = 0; idx < buzzerObj.playCount; idx++){
			//BUZZER_ON();
			GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
			os_dly_wait(buzzerObj.period/OS_TICK_RATE_MS);
			//BUZZER_OFF();
			GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
			os_dly_wait(buzzerObj.period/OS_TICK_RATE_MS);
		};

		os_dly_wait(buzzerObj.delayTime/OS_TICK_RATE_MS);
	};
}
