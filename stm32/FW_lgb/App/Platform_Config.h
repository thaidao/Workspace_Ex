#ifndef _PLATFORM_CONFIG_
#define _PLATFORM_CONFIG_


#define 	HW_VERSION_STR		"0.04"
#define		FW_VERSION_STR		"1.4.1_LGB"


//#define TESTING_VOICE			1	// remove this line in production firmware or you dont wana test the voice

#define 	OS_TICK_CLOCK		1000	// 1000us = 1ms.
#define		OS_TICK_RATE_MS		(OS_TICK_CLOCK/1000)

/* STM32F103VCT6 Memory Map */
#define ApplicationAddressOffset	0x00020000				// 0x802 0000
#define FLASH_PAGE_SIZE			 	((uint16_t)0x800)		// 2 KBytes

#define LOC_CONFIG_PARAM			0x8010000				// 2KB from 0x801000 - 0x801800

/* Main function parameters --------------------------- */
#define PULSE_DETECT_THRESHOLD	5			// Detect accurate speed source of the system

/* RTX Task Priority ------------------------------------ */
#define RTX_MAINTASK_PRIORITY	0x05

#define	RTX_IDWDG_PRIORITY		0x10
#define RTX_PERIODIC_PRIORITY	0x03
#define RTX_1SEC_TSK_PRIORITY	0x02
#define RTX_SAVEDATA_TSK_PRIORITY	0x03

#define RTX_GPS_TSK_PRIORITY	0x04
#define RTX_GSM_TSK_PRIORITY	0x01
#define RTX_HTTP_TSK_PRIORITY	0x01
#define RTX_GPRS_RXTSK_PRIORITY	0x01
#define RTX_GPRS_TXTSK_PRIORITY	0x01
#define RTX_CAM_PRIORITY		0x01

#define RTX_SMS_TSK_PRIORITY	0x01

#ifdef  TESTING_VOICE
	#define RTX_TSK_VOICE_TESTING_1_PRIORITY	0x03
	#define RTX_TSK_VOICE_TESTING_2_PRIORITY	0x03
#endif

#define	RTX_VOICE_PRIORITY		0x01

/* Event Flags */
#define RTX_VOICE_EVENT_FLAG	0x0099
#define RTX_GPRS_ACK_FLAG		0x0050
#define	RTX_DWL_FINISH_FLAG		0x0100
#define	RTX_DWL_START_FLAG		0x0200

/* phien ban hardware */
#define GH12_HW_VERSION			2

/* su dung module periodic thi define */
#define USE_PERIODIC_TASK
#ifdef	USE_PERIODIC_TASK

/* su dung che do debug */
//#define		PERIODIC_DEBUG

/* su dung debug Log on PC */
//#define 	LOG_TO_PC
/* su dung debug Data on PC */
#define 	DATA_TO_PC
#endif

/* su dung module gps thi define dong nay. */
#define USE_GPS_MODULE
#ifdef USE_GPS_MODULE

#define RTX_GPS_TICK_FLAG		0x0001
#define RTX_GPS_SYNC_FLAG		0x0001

/* debug mode. */
//#define GPS_DEBUG
#endif

//----------------------------------------------------
/* su dung gprs thi define dong nay */
#define USE_GPRS_MODULE
/* define cho gprs module */
#ifdef USE_GPRS_MODULE
/* che do debug. */
//#define GPRS_DEBUG_
/* debug for gsm */
#define GSM_DEBUG
/* debug for sim900 */
#define SIM900_DEBUG
/*define support LGB module*/
#define USE_LGB_MODULE

#endif
//-------------------------------------------------------

//#define _PRINT_DEBUG

/* define for certificate ************************************** */

/* neu su dung de cuc vien thong kiem tra thi define dong nay. khong thi bo di */
//#define 	_TELCOM_TEST

// neu su dung de cuc do luong kiem tra thi define dong nay.
//#define USE_CERTIFICATE

/* neu dung cho kiem tra phan cung cua board thi define dong nay. */
//#define 	FIRM_TEST

/* dung cho put online len may tinh */
//#define	USE_PC_MODE

#endif
