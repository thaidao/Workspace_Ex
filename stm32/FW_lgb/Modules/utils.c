/*
 * utils.c
 *
 *  Created on: Jan 10, 2012
 *      Author: Van Ha
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "rtl.h"
#include "stm32f10x.h"
#include "stm32f10x_adc.h"


#include "serial.h"

#include "utils.h"

#define ICT				7
#define BG_YEAR			2001

#define TIME_RECV_GPS		400	// do che tu luc nhan dc ban tin 1 -> ban tin 2
#define TIME_DLY_OTHER 		20


/*----------- local variables --------------------------------*/
static const U8 mthTable[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * \brief
 */
float array_sum(float *arr, uint16_t size)
{
	float sum = 0;
	while(size)
	{
		sum += arr[size-1];
	}
	return sum;
}

/**
 * \brief 	convert a hex to ascii
 **/
void hex2ascii(S8 hex, S8* ascii)
{
	S8 digi1;
	digi1 = (hex >> 4)&0x0F;
	*ascii++ = (digi1 > 9)? (digi1 + '7') : (digi1 + '0');

	digi1 = hex & 0x0f;
	*ascii = (digi1 > 9)? (digi1 + '7') : (digi1 + '0');
}

U8 ascii2hex(S8* ascii)
{
	U8 digi;
	U8 ret;
	digi = (*ascii > '9')? (*ascii - '7'):(*ascii - '0');
	ret = digi;
	ascii++;
	digi = (*ascii > '9')? (*ascii - '7'):(*ascii - '0');
	ret = (ret<<4) | digi;

	return ret;
}

U8 twochar2hex(S8 ch1, S8 ch2)
{
	U8 digi;
	U8 ret;
	digi = (ch1 > '9')? (ch1 - '7'):(ch1 - '0');
	ret = digi;

	digi = (ch2 > '9')? (ch2 - '7'):(ch2 - '0');
	ret = (ret<<4) | digi;

	return ret;
}

float diffdistance(float lat1, float lng1, float lat2, float lng2)
{
	float x, y;
	x = abs(lat1, lat2);
	y = abs(lng1, lng2);

	//return x*x + y*y;
	return x + y;
}

static double degToRad(double deg)
{
	return deg * (double)0.017453292519943295;
}

float diff_dist_m(float x1, float y1, float x2, float y2)
{
	double p1X = degToRad((double)x1);
	double p1Y = degToRad((double)y1);
	double p2X = degToRad((double)x2);
	double p2Y = degToRad((double)y2);
	double distanceInMeter;
	double temp;
	temp = sin(p1X) * sin(p2X) + cos(p1X) * cos(p2X) * cos(p2Y - p1Y);

	if(temp > 1) temp = 1;
	if(temp < -1) temp = -1;

	distanceInMeter = acos(temp)* (double)6378137;
	return distanceInMeter;
}

/**
 * \brief	calculator checksum
 **/
S8 checksum(S8* text)
{
	S8 ch=0;
	while(*text != 0)
		ch ^= *text++;

	return ch;
}

/**
 * \brief 	calculate checksum with fix length.
 **/
S8 cCheckSumFixSize(S8* text, S16 size)
{
	S8 ch=0;
	while(size)
	{
		ch ^= *text++;
		size --;
	}

	return ch;
}

/**
 * \brief 	verify a year is leap.
 * \brief 	input: y - two digi. ex: 10, 11,...
 * \return 	0 - not, 1-others
 **/
S8 isLeap(U8 y)
{
	y += 2000;
	if((y%4)==0)
	{
		if((y%100)!=0) return 1;
		if((y%400)==0) return 1;
	}
	return ((y%4)==0);
}

/**
 * \brief 	check number is date of month
 * \param	input: d, m, y - date, month, year in 2 digi.
 * \return	0 - not, 1 - others.
 **/
S8 isdateover(U8 d, U8 m, U8 y)
{
	if(isLeap(y) && (m == 2))
	{
        if((d >= 29))
		    return 1;
        else return 0;
    }
	if(d >= mthTable[m]) return 1;

	return 0;
}


/**
 * \brief 	decode nmea messages
 * \param	input: string ss - main string
 * \param 	input: string sl - sub string
 * \param 	output: sd - array pointer point to items.
 * \return 	number of items.
 **/
U8 string_split(U8 **sd, U8* ss, U8 *sl, U8 size)
{
	char *p;
	U8 n;
	n = 0;
	p = strtok((char*)ss, (const char*)sl);
	while(p != NULL)
	{
		// put p to sd
		*sd++ = (U8*)p;
		n++;
		// next item
		p = strtok(NULL, (const char*)sl);
		if(n>= size) return n;
	}
	return n;
}

/**
 * \brief	calculator checksum and check it.
 **/
S16 sChkSum(S8* pcText, S8* pcchksum)
{
	S8 chkStr[2];
	hex2ascii(checksum(pcText), chkStr);
	if((chkStr[0] == pcchksum[0]) && (chkStr[1] == pcchksum[1])) return 1;

	return 0;
}

float fLong2float(S8* lng)
{
	float ret;
	U16 dd, mm, dotmm;
	S8 *p;
	//sscanf(lng, "%3d%2d.%4d", &dd, &mm, &dotmm);
	p = lng;
	/* day */
	dd = *p++ - '0';
	dd = dd*10 + (*p++ - '0');
	dd = dd*10 + (*p++ - '0');

	/* month */
	mm = *p++ - '0';
	mm = mm*10 + (*p++ - '0');

	if(*p != '.') return 0;
	p++;
	/* year */
	dotmm = *p++ - '0';
	dotmm = dotmm*10 + (*p++ - '0');
	dotmm = dotmm*10 + (*p++ - '0');
	dotmm = dotmm*10 + (*p++ - '0');

	ret = (float)dotmm/600000.0;
	ret += (float)dd + (float)mm/60.0;

	return ret;
}

float fLat2float(S8* lng)
{
	float ret;
	U16 dd, mm, dotmm;
	S8 *p;
	//sscanf(lng, "%2d%2d.%4d", &dd, &mm, &dotmm);
	p = lng;
	/* day */
	dd = *p++ - '0';
	dd = dd*10 + (*p++ - '0');

	/* month */
	mm = *p++ - '0';
	mm = mm*10 + (*p++ - '0');

	if(*p != '.') return 0;
	p++;
	/* year */
	dotmm = *p++ - '0';
	dotmm = dotmm*10 + (*p++ - '0');
	dotmm = dotmm*10 + (*p++ - '0');
	dotmm = dotmm*10 + (*p++ - '0');

	ret = dd + (float)mm/60.0 + (float)dotmm/600000.0;

	return ret;
}

float str2float(S8* str)
{
	float ret;
	U16 intValue=0, remValue=0;

	for(;*str==' ';str++);
	intValue = atoi((const char*)str);
	while((*str <='9')&&(*str>='0')&&(*str != 0)) str++;
	if(*str++ == '.') remValue = atoi((const char*)str);

	ret = (float)remValue*0.01 + (float)intValue;
	return ret;
}

U16 uCeil(float v)
{
	float t;
	t = (v>=0)? (v+0.5):(v-0.5);
	return (U16)t;
}

S8 findchar(U8**d, U8* s, U8 ch, U8 ms)
{
	S8 ret = 0;
	while((*s != 0) && (ms))
	{
		if(*s == ch)
		{
			*d++ = s;
			ret ++;
			ms--;
		}
		s++;
	}
	return ret;
}

/**
 * \brief 	convert time in gps format to ICT time, date.
 * \param	input: t, d - gps format time, date.
 * \param	output: tm, dt - time, date in ICT.
 **/
BOOL utc2ict(uGPSDateTime_t *dt, S8* t, S8* d, U16 offset)
{
	// thoi gian thuc hien het ham nay vao khang 1ms

	U8 hh, mm, ss;
	S8 *p;

	//sscanf(t, "%2d%2d%2d", &hh, &mm, &ss);
	/* calculate time, convert to struct timer */
	/* format time: 103025.435 */
	p = t;
	/* hour */
	hh = *p++ - '0';
	hh = hh*10 + (*p++ - '0');

	/* min */
	mm = *p++ - '0';
	mm = mm*10 + (*p++ - '0');

	/* second */
	ss = *p++ - '0';
	ss = ss*10 + (*p++ - '0');

	if((ss > 59) || (mm > 59) || (hh > 24))
		return 0;

	/* copy time */
	if(*p == '.')
	{
		dt->hour = hh;
		dt->min = mm;
		dt->sec = ss;
	}
	else
		return 0;


	//sscanf(d, "%2d%2d%2d", &hh, &mm, &ss);
	/* calculate time, convert to struct timer */
	/* format time: 220512 */
	p = d;
	/* day */
	hh = *p++ - '0';
	hh = hh*10 + (*p++ - '0');

	/* month */
	mm = *p++ - '0';
	mm = mm*10 + (*p++ - '0');

	/* year */
	ss = *p++ - '0';
	ss = ss*10 + (*p++ - '0');

	if((mm > 12) || (mm == 0) || (hh == 0) || (hh > 31))
		return 0;

	/* copy date */
	dt->day = hh;
	dt->month = mm;
	dt->year = ss;

	dt->hour += ICT;
	/* if new day */
	if(dt->hour > 23)
	{
		dt->hour = dt->hour-24;
		/* if new month */
		if(isdateover(dt->day, dt->month, dt->year))
		{
			dt->day = 0;
			/* if new year */
			if(dt->month >=12)
			{
				dt->month = 0;
				dt->year ++;
			}
			dt->month++;
		}
		dt->day++;
	}
	return 1;
}

void LowString(s8* str)
{
  	while(*str != 0)
	{
		if((*str >= 'A') && (*str <= 'Z'))
		{
			*str = *str + 'a' - 'A';
		}
		str++;
	}
}

void int2ascii(u16 x, u8* ch)
{
	s16 t;
	t = x/100;
	*ch++ = t+'0';		// hang tram

	x -= t*100;
	t = x/10;
	x -= t*10;
	*ch++ = t+'0';		// hang chuc
	*ch = x + '0';		// hang don vi
}

void vUpdateDateTimeToStr(uGPSDateTime_t* psTime)
{
	uint8_t hour,min,sec;
	uint8_t year,month,day;
	vRTCGetTime(&hour,&min,&sec);
	vRTCGetDate(&day,&month,&year);	

	psTime->year = year;
	psTime->month = month;
	psTime->day = day;

	psTime->sec = sec;
	psTime->min = min;
	psTime->hour =  hour;
}