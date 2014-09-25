/*
 * filter.c
 *
 *  Created on: Apr 12, 2012
 *      Author: Van Ha
 */

#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "Platform_Config.h"
#include "adc.h"

#if 0
const double lp_Param[2][3] =
{
		{0.0009, 0.0019, 0.0009},
		{1.0000, -1.9112, 0.9150},
};
#endif
const double lp_Param[2][3] =
{
	{2.219181891305322e-07, 4.438363782610644e-07, 2.219181891305322e-07},
	{1.0000, -1.998667135315678, 0.998668022988435}
};

double adc3_arr[2], adc3_stable_arr[2];
double adc2_arr[2], adc2_stable_arr[2];
double adc1_arr[2], adc1_stable_arr[2];

double lowpass_filter(double *y, double *x, double xn, int n)
{
	double yn, xi, yi;
	int i;

	xi = 0;
	yi = 0;

	for(i=0;i<n;i++)
	{
		xi += lp_Param[0][i+1]*x[i];
		yi += lp_Param[1][i+1]*y[i];
	}

	yn = (lp_Param[0][0]*xn + xi) - yi;

	return yn;
}

unsigned short ADC1_Filter(void)
{
	unsigned short tmp;
	double yn, xn;
	//char str_adc[20];
	//short n;

	tmp = uReadADC(ADC_Channel_10);

	xn = tmp;

	yn = lowpass_filter(adc1_stable_arr, adc1_arr, xn, 2);

	adc1_stable_arr[1] = adc1_stable_arr[0];
	adc1_stable_arr[0] = yn;

	adc1_arr[1] = adc1_arr[0];
	adc1_arr[0] = xn;

	return (unsigned short)yn;
}

unsigned short ADC2_Filter(void)
{
	unsigned short tmp;
	double yn, xn;

	tmp = uReadADC(ADC_Channel_11);
	xn = tmp;

	yn = lowpass_filter(adc2_stable_arr, adc2_arr, xn, 2);

	adc2_stable_arr[1] = adc2_stable_arr[0];
	adc2_stable_arr[0] = yn;

	adc2_arr[1] = adc2_arr[0];
	adc2_arr[0] = xn;

	return (unsigned short)yn;
}

unsigned short ADC3_Filter(void)
{
	unsigned short tmp;
	double yn, xn;

	tmp = uReadADC(ADC_Channel_12);
	xn = tmp;

	yn = lowpass_filter(adc3_stable_arr, adc3_arr, xn, 2);

	adc3_stable_arr[1] = adc3_stable_arr[0];
	adc3_stable_arr[0] = yn;

	adc3_arr[1] = adc3_arr[0];
	adc3_arr[0] = xn;

	return (unsigned short)yn;
}

__task void vADCTask(void);

void vADC_Filter(void)
{
	short tmp;
	int i;

	tmp = uReadADC(ADC_Channel_12);
	for(i=0;i<2;i++)
	{
		adc3_arr[i] = tmp;
		adc3_stable_arr[i] = tmp;
	}

	tmp = uReadADC(ADC_Channel_11);
	for(i=0;i<2;i++)
	{
		adc2_arr[i] = tmp;
		adc2_stable_arr[i] = tmp;
	}

	tmp = uReadADC(ADC_Channel_10);
	for(i=0;i<2;i++)
	{
		adc1_arr[i] = tmp;
		adc1_stable_arr[i] = tmp;
	}

	os_tsk_create(vADCTask, 1);
}

/* task adc */
__task void vADCTask(void)
{
	for(;;)
	{
		/* if bat may thi moi loc nhieu adc */
		if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_8)==1)
		{
			ADC1_Filter();
		}
		ADC2_Filter();
		ADC3_Filter();
		os_dly_wait(30/OS_TICK_RATE_MS);
	}
}

unsigned short uiReadADC1Value(void)
{
	return (unsigned short )(adc1_stable_arr[0] + 0.5);
}

unsigned short uiReadADC2Value(void)
{
	return (unsigned short )(adc2_stable_arr[0] + 0.5);
}
unsigned short uiReadADC3Value(void)
{
	return (unsigned short )(adc3_stable_arr[0] + 0.5);
}



/* gps filter */

typedef struct _poit_t
{
	float x;
	float y;
	unsigned short time;
}xPoint_t;


#define MAX_POINT 2

xPoint_t point[MAX_POINT];
float Sn_1 = 0;
int pointValidate(float xn, float yn, unsigned short tn, float v)
{
	float sn, a;

	if((xn == 0)||(yn==0)) return 0;

	/* init point 1 = n-2*/
	if((point[1].x == 0)||(point[1].y == 0))
	{
		point[1].x = xn;
		point[1].y = yn;
		point[1].time = tn;
		return 1;
	}
	/* init point 0 = n-1*/
	if((point[0].x == 0)||(point[0].y == 0))
	{
		point[0].x = xn;
		point[0].y = yn;
		point[0].time = tn;

		return 1;
	}
	if(tn <= point[0].time)
		return 0;

#if 0
	/* verify speed */
	if((sn > 1)&&(v<1))
		return 0;

	if((v > 1)&&(sn < 1))
		return 0;
#endif
	/* acce.. */
	Sn_1 = diff_dist_m(point[1].x, point[1].y, point[0].x, point[0].y)/(float)(point[0].time - point[1].time);
	sn = diff_dist_m(point[0].x, point[0].y, xn, yn)/(float)(tn - point[0].time);

	a = (sn > Sn_1)?(sn - Sn_1):(Sn_1 - sn);
	a = a/(float)(tn - point[1].time);
	if((a < 2.5) /*&& (a2 < 2.5)*/)
	{
		return 1;
	}
	return 0;
}


int GPS_Filter(float xn, float yn, unsigned short tn, float v)
{
	float x, y;
	if(pointValidate(xn, yn, tn, v)==0)
	{
		/* trung binh 2 diem */
		x = (point[0].x + xn)/2;
		y = (point[0].y + yn)/2;
	}
	else
	{
		/* diem ok */
		x = xn;
		y = yn;
	}

	/* update array */
	point[1].x = point[0].x;
	point[1].y = point[0].y;
	point[1].time = point[0].time;
	/* update new value */
	point[0].x = x;
	point[0].y = y;
	point[0].time = tn;

	return 1;
}

void readGPSFilter(float *x, float *y)
{
	*x = point[0].x;
	*y = point[0].y;
}

void resetFilter(void)
{
	int i;
	for(i=0;i<2;i++)
	{
		point[i].x = 0;
		point[i].y = 0;
		point[i].time = 0;
	}
}
