/*
 * Led.c
 *
 *  Created on: May 10, 2012
 *      Author: Van Ha
 */


#include <stdio.h>
#include <stdlib.h>

#include "rtl.h"
#include "stm32f10x.h"

#include "string.h"

#include "gprs.h"
#include "platform_config.h"
#include "debug.h"
#include "utils.h"
#include "loadConfig.h"
#include "serial.h"
#include "debug.h"
#include "gpsmodule.h"
#include "gsm.h"


#include "led.h"

#define	LED_GROUP1_PORT			GPIOC
#define	LED_GROUP2_PORT			GPIOD

#define	LED_GROUP1_CLK		RCC_APB2Periph_GPIOC
#define	LED_GROUP2_CLK		RCC_APB2Periph_GPIOD

#define	LED_GSM_PIN				GPIO_Pin_7
#define	LED_GPS_PIN				GPIO_Pin_8
#define	LED_SDCARD_PIN			GPIO_Pin_9

#define	LED_OTHER_PIN			GPIO_Pin_13


#define TIMER_INTERVAL		10
#define NUMBER_LED		4

#define set_led				GPIO_SetBits
#define reset_led			GPIO_ResetBits

typedef enum
{
	LED_GSM_ID = 0,
	LED_GPS_ID,
	LED_SDCARD_ID,
	LED_OTHER_ID,
}led_id_e;

typedef struct
{
	uint8_t tOff;		// in 10ms.
	uint8_t tcycle;	// in 10ms
	uint8_t tcount;	// in 10ms
}led_driver_t;


led_driver_t _led[NUMBER_LED];


extern uint8_t get_sdcard_status(void);

/*
 * \brief		void led_init_port(void)
 *					config io pin used for led controller.
 * \param		none.
 * \return 	none.
 */
void led_init_port(void)
{
	// dont configurate rcc because its configurated in system_init.
	// if you don't use system_init, you must configurate by 
	//		RCC_APB2PeriphClockCmd(LED_GROUP1_CLK , ENABLE);
	//		RCC_APB2PeriphClockCmd(LED_GROUP2_CLK , ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;

	// out put
	GPIO_InitStructure.GPIO_Pin = LED_GSM_PIN | LED_GPS_PIN | LED_SDCARD_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_GROUP1_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = LED_OTHER_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LED_GROUP2_PORT, &GPIO_InitStructure);
}

/*
 * \brief		void led_turn_on(uint8_t id)
 *					turn on led with id.
 * \param		id - led id.
 * \return 	none.
 */
void led_turn_on(uint8_t id)
{
	switch(id){
		case LED_GSM_ID: // led 0.
			reset_led(LED_GROUP1_PORT, LED_GSM_PIN);
			break;
		case LED_GPS_ID:
			reset_led(LED_GROUP1_PORT, LED_GPS_PIN);
			break;
		case LED_SDCARD_ID:
			reset_led(LED_GROUP1_PORT, LED_SDCARD_PIN);
			break;
		default:
			reset_led(LED_GROUP2_PORT, LED_OTHER_PIN);
			break;
	}
}

/*
 * \brief		void led_turn_off(uint8_t id)
 *					turn on led with id.
 * \param		id.
 * \return 	none.
 */
void led_turn_off(uint8_t id)
{
	switch(id){
		case LED_GSM_ID: // led 0.
			set_led(LED_GROUP1_PORT, LED_GSM_PIN);
			break;
		case LED_GPS_ID:
			set_led(LED_GROUP1_PORT, LED_GPS_PIN);
			break;
		case LED_SDCARD_ID:
			set_led(LED_GROUP1_PORT, LED_SDCARD_PIN);
			break;
		default:
			set_led(LED_GROUP2_PORT, LED_OTHER_PIN);
			break;
	}
}

/*
 * \brief		void led_manager(void)
 *					management led by id. turn on, turn off, blink, flash...
 * \param		none.
 * \return 	none.
 */
void led_manager(void)
{
	int i;
	for(i=0;i<NUMBER_LED;i++){
		// blink led.
		if(_led[i].tcount == 0){	// turn on
			led_turn_on(i);
		}// turn off.
		else if(_led[i].tcount == _led[i].tOff){
			led_turn_off(i);
		}
		// timer count
		_led[i].tcount ++;
		if(_led[i].tcount >= _led[i].tcycle)
			_led[i].tcount = 0;
	}
}


/*
 * \brief		void led_set_params(uint8_t id, uint8_t toff, uint8_t tcycle)
 *					set cycle, time off for blink led.
 * \param		id - led index.
 * \param		toff - off mask.
 * \param		tcycle - time repeat state.
 * \return 	none.
 */
void led_set_params(uint8_t id, uint8_t toff, uint8_t tcycle)
{
	_led[id].tcycle = tcycle;
	_led[id].tOff = toff;
	//_led[id].tcount = 0;
}

/*
 * \brief		void led_hold(uint8_t id, uint8_t sta)
 *					set cycle, time off for blink led.
 * \param		id - led index.
 * \param		sta - 0 (turn off), 1 (turn on).
 * \return 	none.
 */
void led_hold(uint8_t id, uint8_t sta)
{
	if(sta == 0){	// always off.
		_led[id].tcount = _led[id].tOff;
	}
	else{	// always on.
		_led[id].tcount = 0;
	}
}

/*
 * \brief		void led_drv_gsm(uint8_t sta)
 *					driver led gsm. blink, turn off, turn on or flash.
 * \param		sta - gsm status.
 * \return 	none.
 */
void led_drv_gsm(uint8_t sta)
{
	if(sta == 0) led_hold(0, 1);			// gsm error.
	else if(sta == 1) led_set_params(0, 50, 100); // gsm search provider
	else if(sta == 2) led_set_params(0, 10, 100); // gsm search GPRS and server.
	else if(sta == 3) led_set_params(0, 5, 200);	// gsm connect server success.
}

/*
 * \brief		void led_drv_gps(uint8_t sta)
 *					driver led gsm. blink, turn off, turn on or flash.
 * \param		sta - gsm status.
 * \return 	none.
 */
void led_drv_gps(uint8_t sta)
{
	//if(sta == 0) led_set_params(1, 0, 0);			// gps error.
	if(sta == 0) led_set_params(1, 50, 100);		// gps search
	else if(sta == 1) led_set_params(1, 10, 200);// gps fix.
}

/*
 * \brief		void led_drv_sdcard(uint8_t sta)
 *					driver led sdcard. blink, turn off, turn on or flash.
 * \param		sta - gsm status.
 * \return 	none.
 */
void led_drv_sdcard(uint8_t sta)
{
	if(sta == 0) led_set_params(2, 1, 2);		// sdcard error.
	else if(sta == 1) led_hold(2, 0);				// sdcard ok.
}

/*
 * \brief		void led_drv_sdcard(uint8_t sta)
 *					driver led sdcard. blink, turn off, turn on or flash.
 * \param		sta - gsm status.
 * \return 	none.
 */
void led_drv_other(void)
{
	// warning by led.
	if(Config.alarmSpeed.alarmSpeedValue <= GPS_get_speed())
		led_set_params(3, 10, 20);
	else
		led_hold(3, 0);				// turn off led warning.
}


/*
 * \brief		__task void led_drivers(void)
 *					controller manager. blink, turn off, turn on or flash.
 * \param		sta - gsm status.
 * \return 	none.
 */
__task void led_drivers(void)
{
	for(;;){
		// get gsm status.
		led_drv_gsm(get_gsm_status());
		// get gps status.
		led_drv_gps((uint8_t)GPS_GetStatus());
		// get sdcard status.
		led_drv_sdcard(get_sdcard_status());
		// warnning speed.
		led_drv_other();
		// manager led.
		led_manager();
		// os next task
		os_dly_wait(TIMER_INTERVAL);	// wait 100ms.
	}
}

/*
 * \brief		void led_driver_init(void)
 *					controller manager. blink, turn off, turn on or flash.
 * \param		none.
 * \return 	none.
 */
void led_driver_init(void)
{	
	int i;
	led_init_port();
	
	for(i=0;i<NUMBER_LED;i++){
		_led[i].tcount = 0;
		_led[i].tcycle = 2;
		_led[i].tOff = 1;
	}
	
	led_hold(0, 0);
	led_hold(1, 0);
	led_hold(2, 0);
	led_hold(3, 0);
	// create task for control led
	os_tsk_create(led_drivers, 1);
}

