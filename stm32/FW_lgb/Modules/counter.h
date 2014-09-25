/*
 * counter.h
 *
 *  Created on: Feb 9, 2012
 *      Author: Van Ha
 */

#ifndef COUNTER_H_
#define COUNTER_H_

void vGPIOTimerConfig(void);
void vTIMConfig(void);

void vTimPulseInit(void);
//data
U16 uiGetPulse1(void);
U16 uiGetPulse2(void);

#endif /* COUNTER_H_ */
