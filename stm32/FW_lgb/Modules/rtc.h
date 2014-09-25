#ifndef _RTC_MODULE_
#define _RTC_MODULE_

/* BKP Tables ------------------------- */
#define	BKP_RTC_DATE 				BKP_DR1

/* Firmware */
#define BKP_FW_STATUS				BKP_DR4				/* Firmware Update Status */
#define BKP_FW_LEN_H				BKP_DR5				// First 16bit of data len
#define BKP_FW_LEN_L				BKP_DR6				// Second 16bit of data len

#define NEW_FW_AVAILABLE_FLAG		0x1111
#define NO_FW_AVAILABLE_FLAG		0x1010

/* Pulse */
#define	BKP_TOTAL_PULSE_H			BKP_DR7
#define	BKP_TOTAL_PULSE_L			BKP_DR8
#define	BKP_TOTAL_IO				BKP_DR9

#define BKP_SYSTEM_RESET_COUNT 		BKP_DR10			// system reset counter

/* */
#define	BKP_TOTAL_RUN_H				BKP_DR11
#define	BKP_CONTI_RUN_H				BKP_DR13
#define	BKP_DOOR_OPEN_H				BKP_DR14
#define	BKP_OVER_SPEED_H			BKP_DR15

#define	BKP_TOTAL_TURBINPULSE_H		BKP_DR17
#define	BKP_TOTAL_TURBINPULSE_L		BKP_DR18

#define	BKP_TOTAL_CAR_OFF_H			BKP_DR19	// 1 word
#define	BKP_TOTAL_CAR_STOP_H		BKP_DR21	// 1 word

#define TODAY_BACKUP				BKP_DR23		// 1 word

#define BKP_CURRENT_DRV_ID			BKP_DR24		// length of id is 8 byte = 4 word
								//  BKP_DR25
								//  BKP_DR26
								//  BKP_DR27
/* Image */
#define IMG_LIST_READ_IDX		BKP_DR28
#define IMG_LIST_READ_IDX_H		BKP_DR29
#define IMG_LIST_WRITE_IDX		BKP_DR30
#define IMG_LIST_WRITE_IDX_H	BKP_DR31

#define IMG_INDEX_IN_DAY				BKP_DR32

/* SDFifo */
#define BKP_SDFIFO_READ					BKP_DR33
#define BKP_SDFIFO_WRITE				BKP_DR34

/* IO report message */
#define BKP_IO_REPORT_READ			BKP_DR35
#define BKP_IO_REPORT_WRITE			BKP_DR36
#define BKP_I0_COUNTER					BKP_DR37
#define BKP_I1_COUNTER					BKP_DR38
#define BKP_I2_COUNTER					BKP_DR39

// flags for reset newday
#define RTC_INT_FLAG		0x0001
#define RTC_RST_FLAG		0x0002
#define RTC_BUS_FLAG		0x0004
#define RTC_TAX1_FLAG		0x0008
#define RTC_TAX2_FLAG		0x0010
#define IO_REPORT_FLAG	0x0020

/* IRQ Handlers */
void RTC_IRQHandler(void);

/* RTC API ---------------------------- */

// Init RTC Module
void vRTCInit(void);

// Get time, date
void vRTCGetTime(uint8_t *hour, uint8_t *min, uint8_t *sec);
void vRTCGetDate(uint8_t *day, uint8_t *month, uint8_t *year);

// Update time, date
void vRTCSetTime(uint8_t hour, uint8_t min, uint8_t sec);
void vRTCSetDate(uint8_t day, uint8_t month, uint8_t year);


/* Backup RAM API */
void vSaveToBackupRam(U16 add, U16* data, U16 len);
void vLoadFromBackupRam(U16 add, U16* data, U16 len);

void vFactoryReset(void);

void RTC_ReSetNewdayFlag(uint16_t flag);
uint16_t RTC_GetNewdayFlag(uint16_t flag);
void RTC_SetNewdayFlag(uint16_t flag);

#endif
