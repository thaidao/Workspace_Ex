#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "loadConfig.h"
#include "Platform_Config.h"


/* Private macros ------------------------------------ */


/** Private variable  -------------------------------- */
GH12_Config_t Config;
host_channel_t HostChannel = {CH_NULL, CH_STATE_IDLE, NULL, NULL};

const GH12_Config_t Config_Default = {
	"CZXXX",
	"210.245.94.99",
	"183.91.4.73",
	"1235",
	"1235",
	"v-internet",
	"abc",
	"123",
	"NA",
	"NA",
	"NA",
	{"00000000","NA","NA","NA","NA"},
	{0, 0, 0, 0, 0},
	{0, 0, 900},
	{0, 80},
	120,
	{0, 0, 6, 5, 0},
	{1,0,1},
	{"+841639989005", "EPOSI"}
};

/**
 * @brief	Load config from microSd Card
 *
 */
int8_t config_load(GH12_Config_t* pConfig){

	/* Copy from Flash memory to config location */
	memcpy(pConfig, LOC_CONFIG_PARAM, sizeof(GH12_Config_t));

	if((pConfig->BoxID[0] < 'A') || (pConfig->BoxID[0] > 'Z'))
		return -1;
	/* @todo Validate config */

	if(Config.GPS_radius >= 254)		// default config.
		Config.GPS_radius = 120;

	// default alarm speed.
	if((pConfig->alarmSpeed.alarmSpeedEnable == 0) || (pConfig->alarmSpeed.alarmSpeedValue >= 126)){
		pConfig->alarmSpeed.alarmSpeedEnable = 1;
		pConfig->alarmSpeed.alarmSpeedValue = 80;

		config_apply(pConfig);
	}

	// default gps filter.
	if((pConfig->GPSFilter.timeCycle != 5))		// config.
	{
		pConfig->GPSFilter.overspeed_threshold = 6;
		pConfig->GPSFilter.use_engine = 1;
		pConfig->GPSFilter.method_filter = 0;
		pConfig->GPSFilter.timeCycle = 5;

		config_apply(pConfig);
	}
	if((pConfig->admin.user[0] <= 20) || (pConfig->admin.user[0] >= 127) || 
		(pConfig->admin.user[0] <= 20) || (pConfig->admin.user[0] >= 127)){	// set default.
		strcpy((char*)pConfig->admin.user, "+841639989005");
		strcpy((char*)pConfig->admin.pass, "EPOSI");
		// save config.
		config_apply(pConfig);
	}

	return 0;
}

/**
 * @brief	Write config to Flash
 * 			Location from 0x801 0000
 */
int8_t config_apply(GH12_Config_t* pConfig){
volatile FLASH_Status FLASHStatus = FLASH_COMPLETE;
uint16_t idx;

	/* Compare pConfig with saved memory */
	if(memcmp(pConfig, LOC_CONFIG_PARAM, sizeof(GH12_Config_t)) == 0){
		return 0;
	}

	/* Unlock Bank1 */
	FLASH_UnlockBank1();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	/* Erase page */
	FLASHStatus = FLASH_ErasePage(LOC_CONFIG_PARAM);
	if(FLASHStatus != FLASH_COMPLETE){
		return -1;
	}

	/* Write config */
	for(idx=0; idx< sizeof(GH12_Config_t)/4 + 1; idx++){
		FLASHStatus = FLASH_ProgramWord(LOC_CONFIG_PARAM + 4*idx, *((uint32_t*) pConfig + idx));
		if(FLASHStatus != FLASH_COMPLETE)
			return -1;
	}

	/* Lock Bank1 again */
	FLASH_LockBank1();
	return 0;
}

/*
 *\brief	int8_t confige_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_device_gets(uint8_t *out, uint16_t size)
{
	uint16_t len = &Config.VehicleVIN[0] - &Config.BoxID[0];
	if(size <= len)
		return -1;

	strncpy(out, Config.BoxID, 8);
	strcat(out, ",");

	strncat(out, Config.IP1, 20);
	strcat(out, ",");

	strncat(out, Config.IP2, 20);
	strcat(out, ",");

	strncat(out, Config.Port1, 6);
	strcat(out, ",");

	strncat(out, Config.Port2, 6);
	strcat(out, ",");

	strncat(out, Config.apn_name, 15);
	strcat(out, ",");

	strncat(out, Config.apn_user, 15);
	strcat(out, ",");

	strncat(out, Config.apn_pass, 15);
	strcat(out, ",");

	strncat(out, Config.SIM_Info, 16);

	return 0;
}



/*
 *\brief	int8_t confige_vehicle_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_vehicle_gets(uint8_t *out, uint16_t size)
{
	uint16_t len = &Config.DefaultDriver.ID[0] -  &Config.VehicleID[0];
	if(size <= len)
		return -1;

	strncpy(out, Config.VehicleVIN, 24);
	strcat(out, ",");

	strncat(out, Config.VehicleID, 10);

	return 0;

}


/*
 *\brief	int8_t confige_driver_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_driver_gets(uint8_t *out, uint16_t size)
{

	strncpy(out, Config.DefaultDriver.ID, 8);
	strcat(out, ",");

	strncat(out, Config.DefaultDriver.name, 32);
	strcat(out, ",");

	strncat(out, Config.DefaultDriver.lic_number, 15);
	strcat(out, ",");

	strncat(out, Config.DefaultDriver.issueDate, 11);
	strcat(out, ",");

	strncat(out, Config.DefaultDriver.expiryDate, 11);

	return 0;
}


/*
 *\brief	int8_t confige_ext_gets(uint8_t *out, uint16_t size)
 *			return all fields in Config format text string.
 *\param	out is block memory store data.
 *\param	size is size of out.
 *\return	0 - success, -1 fail.
*/
int8_t config_ext_gets(uint8_t *out, uint16_t size)
{
	uint8_t temp[12];

	sprintf(temp, "%u\x0", Config.option.playVoice);
	strcpy(out, temp);
	strcat(out, ",");

	sprintf(temp, "%u.%u.%u\x0", Config.CAM.cam_used, Config.CAM.cam_number, Config.CAM.CamSnapShotCycle);
	strcat(out, temp);
	strcat(out, ",");

	sprintf(temp, "%u.%u\x0", Config.alarmSpeed.alarmSpeedEnable, Config.alarmSpeed.alarmSpeedValue);
	strcat(out, temp);
	strcat(out, ",");

	sprintf(temp, "%u\x0", Config.GPS_radius);
	strcat(out, temp);
	strcat(out, ",");

	sprintf(temp, "%u.%u.%u.%u\x0", Config.GPSFilter.use_engine, Config.GPSFilter.method_filter, Config.GPSFilter.overspeed_threshold, Config.GPSFilter.timeCycle);
	strcat(out, temp);
	strcat(out, ",");

	// use delay send data to server.
	sprintf(temp, "%u\x0", Config.option.usedDelaySend);
	strcat(out, temp);
	strcat(out, ",");
	
	// io message to server.
	sprintf(temp, "%u\x0", Config.option.usedIOMessage);
	strcat(out, temp);

	return 0;
}
