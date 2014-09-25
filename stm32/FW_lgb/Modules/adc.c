/*
 * adc.c
 *
 *  Created on: Feb 2, 2012
 *      Author: Van Ha
 *
 *      version
 *      0.1
 *      	- su dung adc bang cach polling, k dung ngat.
 *      	- k tinh trung binh gia tri.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "rtl.h"
#include "stm32f10x.h"
#include "stm32f10x_adc.h"

#define 	ADC_PORT		ADC1
#define 	ADC_GPIO_PORT	GPIOC
#define		ADC1_PIN		GPIO_Pin_0
#define		ADC2_PIN		GPIO_Pin_1
#define		ADC3_PIN		GPIO_Pin_2


/*
 * Vref = 3.3V
 * 12bit = 4096
 * n =
 */

void vADC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;
	/*
	 *	configure ADC pins
	 */

	/* Configure PC.0 (ADC Channel2) as analog input -------------------------*/
  	GPIO_InitStructure.GPIO_Pin = ADC1_PIN;
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
  	GPIO_Init(ADC_GPIO_PORT, &GPIO_InitStructure);

  	GPIO_InitStructure.GPIO_Pin = ADC2_PIN;
  	GPIO_Init(ADC_GPIO_PORT, &GPIO_InitStructure);

  	GPIO_InitStructure.GPIO_Pin = ADC3_PIN;
  	GPIO_Init(ADC_GPIO_PORT, &GPIO_InitStructure);

	/* Configure clocks for ADC and GPIO PORT */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOC, ENABLE);

	/* ADCx configuration ------------------------------------------------------*/
  	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
  	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
  	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
  	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
  	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
  	ADC_InitStructure.ADC_NbrOfChannel = 1;
  	ADC_Init(ADC_PORT, &ADC_InitStructure);

  	/* Enable ADC_PORT */
  	ADC_Cmd(ADC_PORT, ENABLE);

	/* Enable ADC_PORT reset calibaration register */
	ADC_ResetCalibration(ADC_PORT);

	/* Check the end of ADC_PORT reset calibration register */
	while(ADC_GetResetCalibrationStatus(ADC_PORT));

	/* Start ADC_PORT calibaration */
	ADC_StartCalibration(ADC_PORT);
	/* Check the end of ADC_PORT calibration */
	while(ADC_GetCalibrationStatus(ADC_PORT));
}

U16 uReadADC(U8 channel)
{
	ADC_RegularChannelConfig(ADC_PORT, channel, 1, ADC_SampleTime_28Cycles5);
	ADC_SoftwareStartConvCmd(ADC_PORT, ENABLE);
	// Wait until conversion completion
	while(ADC_GetFlagStatus(ADC_PORT, ADC_FLAG_EOC) == RESET);
	// Get the conversion value
	return ADC_GetConversionValue(ADC_PORT);

}

