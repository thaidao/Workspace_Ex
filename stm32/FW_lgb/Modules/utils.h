/*
 * utils.h
 *
 *  Created on: Jan 10, 2012
 *      Author: Van Ha
 */

#ifndef UTILS_H_
#define UTILS_H_


#define abs(x, y)		(x > y)?(x-y):(y-x)

#define array_overage(arr, size) 		array_sum(arr, size)/size

/*___________TYPEDEF__________________________________*/
typedef union _float_t
{
	float f;
	struct ubyte
	{
		U8 ub1;
		U8 ub2;
		U8 ub3;
		U8 ub4;
	}FBYTE;
}mfloat_t;

typedef struct _uGPSDateTime_t
{
	U32 year:6;
	U32 month:4;
	U32 day:5;

	U32 sec:6;
	U32 min:6;
	U32 hour:5;
}uGPSDateTime_t;

typedef struct _Car_Status_t
{
	U8 EngineStatus:1;
	U8 DoorStatus:1;
	U8 A_CStatus:1;
	U8 IO_IN3:1;
	U8 IO_IN4:1;
	U8 IO_IN5:1;
	U8 IO_OUT1:1;
	U8 IO_OUT2:1;
}CarStatus_t;

typedef struct _Car_Pulse_t
{
	U16 CurPulse;			// 2 bytes
	U16 MaxPulse;
	U32 TotalPulse;
}CarPulse_t;


typedef struct _LogInfo_t
{
	uGPSDateTime_t tm;
	float lat;
	float lng;

	// thoi gian lai xe
	U32 total_time;	// tong thoi gian lai xe trong ngay.
	U16 CurSpeed;

	U16 cont_time;	// thoi gian lai xe lien tuc.
	U16 over_speed;	// so lan vuot qua toc do.

	// thong tin hanh trinh
	U16 c_door;		// so lan dong mo cua.
	U16 c_off;		// so lan do xe
	U16 c_stop;		// so lan dung xe.

	CarStatus_t io;
	U8 st;			// trang thai xe.
	U8 id_drv[8];	// id cua lai xe.

	U16 pulse;

}LogInfo_t;


typedef	struct _DATA_FRAME_t
{
	U8 BoxId[8];				// box id.
	U32 total_pulse1;			// tong xung 1
	U32 total_pulse2;			// tong xung 2
	uGPSDateTime_t tm;			// ngay, gio
	float lat;					// vi do
	float lng;					// kinh do

	S16 alt;					// do cao
	U16 total_time;				// tong thoi gian lia xe trong ngay
	U16 cont_time;				// tong thoi gian lai xe lien tuc
	U16 over_speed;				// so lan qua toc do
	U16 door_numb;				// so lan dong/mo cua
	U16 stop_numb;				// so lan dung xe
	U16 off_numb;				// so lan do xe

	U16 pulse1;					// xung van toc
	U16 pulse2;					// xung turbin

	U16 adc1;					// adc 1
	U16 adc2;					// adc 2
	U16 adc3;					// adc 3
	U16 trackAngle;				// goc, huong di chuyen

	CarStatus_t io;				// trang thai xe
	U8 gps_speed;				// toc do gps
	S8 heating;					// nhiet do

	S8 drv_key[8];				// the lai xe
	U8 n_s:1;					/* 1 byte */
	U8 e_w:1;
	U8 status:1;
	U8 numberSat:5;
	// info 1s
	S16 delta_lat[29];			// sai so vi do
	S16 delta_lng[29];			// sai so kinh do
	U8 gps_speed_arr[29];		// van toc gps

	U8 checksum;				// check sum
}Data_frame_t;


typedef struct _DriverInfo_t
{
	U16 OpenDoor;
	U16 OverSpeed;
	U16 DrvTime;
	U32 DrvTotal;
}DriverInfo_t;

typedef enum _OBJSTATE_E
{
	STOPPING=0,
	RUNNING,
	UNKNOWN
}OBJSTATE_E;

typedef struct _ObjectState_t
{
	OBJSTATE_E state;
}ObjectState_t;




//extra info


/**___________MACRO__________________________________*/
#define knot2kmph(knot)			knot*1.852

/**** 10km/h ***/
#define knot2km10ph(knot)		knot*18.52

/**
 *
 * \note	1knot = 0.514444mps.
 **/
#define knot2mps(knot)			(float)knot*0.514444


/**_________PROTOTYPE______________________________________*/
U8 string_split(U8 **sd, U8 *ss, U8 *sl, U8 size);
U8 twochar2hex(S8 ch1, S8 ch2);
void hex2ascii(S8 hex, S8* ascii);
S8 findchar(U8**d, U8* s, U8 ch, U8 ms);
U16 uCeil(float v);
float str2float(S8* str);
float fLat2float(S8* lng);
float fLong2float(S8* lng);

S16 sChkSum(S8* pcText, S8* pcchksum);
U8 string_split(U8 **sd, U8*ss, U8 *sl, U8 size);
S8 cCheckSumFixSize(S8* text, S16 size);
S8 checksum(S8* text);

float diffdistance(float lat1, float lng1, float lat2, float lng2);
float diff_dist_m(float x1, float y1, float x2, float y2);

BOOL utc2ict(uGPSDateTime_t *dt, S8* t, S8* d, U16 offset);
void int2ascii(u16 x, u8* ch);
void LowString(s8* str);
void vUpdateDateTimeToStr(uGPSDateTime_t* psTime);
float array_sum(float *arr, uint16_t size);

/**
 * \brief 	check number is date of month
 * \param	input: d, m, y - date, month, year in 2 digi.
 * \return	0 - not, 1 - others.
 **/
S8 isdateover(U8 d, U8 m, U8 y);

#endif /* UTILS_H_ */
