/*
 * filter.h
 *
 *  Created on: Apr 12, 2012
 *      Author: Van Ha
 */

#ifndef FILTER_H_
#define FILTER_H_


void vADC_Filter(void);
unsigned short ADC3_Filter(void);
unsigned short ADC2_Filter(void);
unsigned short ADC1_Filter(void);

unsigned short uiReadADC3Value(void);
unsigned short uiReadADC2Value(void);
unsigned short uiReadADC1Value(void);

/* GPS */
int GPS_Filter(float *xn, float *yn, unsigned short tn, float v);
void resetFilter(void);
void readGPSFilter(float *x, float *y);

#endif /* FILTER_H_ */
