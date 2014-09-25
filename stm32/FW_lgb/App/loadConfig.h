#ifndef _LOAD_CONFIG_
#define _LOAD_CONFIG_

/* Config structure  --------------------------- */
#include "driverdb.h"

/* Option: 1: enalbe, o: disable */
typedef struct{
	uint8_t	playVoice:1;			// Play voice or not
	uint8_t usedDelaySend:1;		// Wait for send data.
	uint8_t usedIOMessage:1;
	uint8_t reserved:4;
	uint8_t	resetBuffer:1;

} GH12_Option_t;

typedef struct{
	uint8_t cam_used:1;
	uint8_t cam_number:5;
	uint16_t CamSnapShotCycle;
}Camera_Option_t;

typedef struct{
	uint8_t tm_used:1;	//allow support tm or not
	uint8_t tm_type:7; 	//tm type
	uint8_t tm_timeout;	//timeout to send data
}TaxiMeter_Option_t;


typedef struct{
	uint8_t alarmSpeedEnable:1;
	uint8_t alarmSpeedValue:7;
}AlarmSpeed_t;

typedef struct{
	uint16_t use_engine:1;				// 0 not use engine for filter.
	uint16_t method_filter:1;			// 1 use gpsradius, 0 use overspeed.
	uint16_t overspeed_threshold: 4; 	// km/h = {0 -> 15}.
	uint16_t timeCycle:7;				// 1/2*timeCycle => T min.
	uint16_t reverse:4; //
}GPS_Filter_t;

typedef enum{
	CH_NULL = 0,
	CH_COM,
	CH_SMS,
	CH_GPRS,
}channel_type_e;

typedef enum{
	CH_STATE_BUSY = 0,
	CH_STATE_IDLE,
}channel_state_e;

typedef struct{
	uint8_t type:4;
	uint8_t state:4;
	int8_t *phost;
	int8_t *pData;
}host_channel_t;

typedef struct{
	int8_t user[16];
	int8_t pass[6];
}admin_t;

typedef struct{
	uint8_t BoxID[8];
	uint8_t	IP1[20];
	uint8_t IP2[20];
	uint8_t Port1[6];
	uint8_t Port2[6];
	uint8_t apn_name[15];
	uint8_t apn_user[15];
	uint8_t apn_pass[15];
	uint8_t SIM_Info[16];
	// Car info
	uint8_t VehicleVIN[24];
	uint8_t VehicleID[10];
	// Default Driver
	driverdb_t DefaultDriver;
	GH12_Option_t option;				// Options of
	/* camera option */
	Camera_Option_t CAM;

	/* speed alarm */
	AlarmSpeed_t alarmSpeed;

	/* tham so loc gps */
	uint8_t GPS_radius;
	GPS_Filter_t GPSFilter;
	/* taximeter option */
	TaxiMeter_Option_t TM;
	
	/* admin permision.*/
	admin_t admin;

} GH12_Config_t;

/* Exported variables */
extern GH12_Config_t Config;
extern const GH12_Config_t Config_Default;
extern host_channel_t HostChannel;

/* Macros */
#define Config_IRQHandler		USART1_IRQHandler

/* API */
void config_createDefault(void);
int8_t config_load(GH12_Config_t* pConfig);
int8_t config_apply(GH12_Config_t* pConfig);


/*
 *\brief	int8_t confige_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_device_gets(uint8_t *out, uint16_t size);

/*
 *\brief	int8_t confige_ext_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_ext_gets(uint8_t *out, uint16_t size);

/*
 *\brief	int8_t confige_driver_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_driver_gets(uint8_t *out, uint16_t size);

/*
 *\brief	int8_t confige_vehicle_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_vehicle_gets(uint8_t *out, uint16_t size);

#endif
