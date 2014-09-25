/*
 * gpsModule.h
 *
 *  Created on: Feb 5, 2012
 *      Author: Van Ha
 */

#ifndef GPSMODULE_H_
#define GPSMODULE_H_

/**
 * temp:
	0B80AEAC | 00 |163727 | 692C10 | 00 | 1300
	date/time| ind|lat	  | long   | s  | altitude

 *
 **/

/* GPS information, */
typedef struct
{
	/* date, time structure. see logtask.h */
	uGPSDateTime_t dt;		/* 4 bytes */
	/* indication flag */
	U8 n_s:1;				/* 1 byte */
	U8 e_w:1;
	U8 status:1;
	U8 numberSat:5;

	U16 speed;				/* 2 byte - 10*kmph */

	/* */
	float latitude;		/* 4 bytes */
	float longitude;		/* 4 bytes */

	U16 trackAngle;		/* 2bytes - track angle = TrackAngle/10. (0.0 - 360.0) */

	S16 altitude;			/* 2 byte */
} gpsInfo_t;


#define WDT_GPS_RX_MAX_TIMING		5 //5*800/1000 = 4S

void vGPS_Init(void);


/*
 * \brief 	gpsInfo_t GPS_GetInfo(void)
 * 			Get all field in gps struct.
 * \param	none.
 * \return 	gps struct.
 */
gpsInfo_t GPS_GetInfo(void);

/*
 * \brief 	read buffer value to out buffer.
 */
BOOL blGPS_Read(S8* pOut);

/*
 * \brief	check status gps signal.
 */
int8_t GPS_GetStatus(void);

/** dong bo time */
void vUpdateTime(uGPSDateTime_t *tm);
void __GPS_GetTime(uGPSDateTime_t *tm);

U16 uGetSpeed(void);
void vGPS_Reset(void);
uint8_t WDT_Check_GpsRx_Service(void);

void GPS_get_location_indicate(uint8_t *n, uint8_t *e);
void GPS_get_xy(float *x, float *y);
uint8_t GPS_get_speed(void);

#endif /* GPSMODULE_H_ */
