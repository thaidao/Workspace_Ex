#ifndef _BUZZER_MODULE_
#define _BUZZER_MODULE_

/* Macros: GPIOE14 */
#define BUZZER_GPIO_PORT	GPIOE
#define BUZZER_GPIO_PIN		GPIO_Pin_14
#define BUZZER_GPIO_CLK		RCC_APB2Periph_GPIOE

/* Structure */
typedef struct{
	uint16_t 	period;			// Buzzer period
	uint8_t 	playCount;		//
	uint16_t 	delayTime;		// Delay time between 2 repeat play
	uint8_t		repeatCount;
} buzzer_obj_t;

/* API */
void vBuzzer_Init(void);
void vBuzzerPlay(buzzer_obj_t buzzerObj);

#endif
