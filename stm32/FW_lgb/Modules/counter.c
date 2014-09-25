/*
 * counter.c
 *
 *  Created on: Feb 9, 2012
 *      Author: Van Ha
 *
 *
 *  Pin:
 *  	PC6 - Timer3_channel_1 (full remap).
 *  	PD12 - timer4_channel_1 (remap).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "rtl.h"
#include "stm32f10x.h"

#define PULSE_1_PIN			GPIO_Pin_6
#define PULSE_1_PORT		GPIOC

#define PULSE_2_PIN			GPIO_Pin_12
#define PULSE_2_PORT		GPIOD

#define RCC_APB2Periph_PULSE_1		RCC_APB2Periph_GPIOC
#define RCC_APB2Periph_PULSE_2		RCC_APB2Periph_GPIOD

#define PULSE_1_CHANNEL			TIM_Channel_1
#define PULSE_2_CHANNEL			TIM_Channel_1

#define PULSE_1_TIMER			TIM4
#define PULSE_2_TIMER			TIM3

void vGPIOTimerConfig(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	/*
	 *	configure ADC pins
	 */

	/* Configure PC.6 (ADC Channel2) as analog input -------------------------*/
	GPIO_InitStructure.GPIO_Pin = PULSE_1_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(PULSE_1_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = PULSE_2_PIN;
	GPIO_Init(PULSE_2_PORT, &GPIO_InitStructure);

	/* Configure clocks for ADC and GPIO PORT */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_PULSE_1 | RCC_APB2Periph_PULSE_2, ENABLE);
	/* configure clocks for AFIO */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

	// Remap pin
	GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_TIM4, ENABLE);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3 |
	                         RCC_APB1Periph_TIM4, ENABLE);
}

void vTIMConfig(void)
{
	// thiet lap timer voi che do dung xung clock ngoai, bat tin hieu theo suon xuong.
	TIM_TIxExternalClockConfig(PULSE_1_TIMER, TIM_TIxExternalCLK1Source_TI1,
			TIM_ICPolarity_Falling, 15);
	TIM_TIxExternalClockConfig(PULSE_2_TIMER, TIM_TIxExternalCLK1Source_TI1,
				TIM_ICPolarity_Falling, 15);

	TIM_Cmd (PULSE_1_TIMER, ENABLE);
	TIM_Cmd (PULSE_2_TIMER, ENABLE);
}

/*
 * init pulse
 */
void vTimPulseInit(void)
{
	vGPIOTimerConfig();
	vTIMConfig();
}

/*
 * doc gia tri timer.
 */
U16 uiGetPulse1(void)
{
	U16 ret;
	ret = TIM_GetCounter(PULSE_1_TIMER);
	// reset value
	TIM_SetCounter(PULSE_1_TIMER, 0);
	return ret;
}

/*
 * doc gia tri timer.
 */
U16 uiGetPulse2(void)
{
	U16 ret;
	ret = TIM_GetCounter(PULSE_2_TIMER);
	// reset value
	TIM_SetCounter(PULSE_2_TIMER, 0);
	return ret;
}
