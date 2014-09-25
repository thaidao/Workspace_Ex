/*
 * sim18.h
 *
 *  Created on: Jan 17, 2012
 *      Author: Van Ha
 *
 *      version:
 *      0.2
 *      	add init hardware functions.
 */

#ifndef SIM18_H_
#define SIM18_H_

#include "utils.h"

/**
 * --------MACRO DEFINE ---------------------------------
 */
#if GH12_HW_VERSION == 1
#define bGPSStatus()				GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_0)
#endif

#define DELAY_500MS				500
#define DELAY_3S				3000
#define DELAY_2S				1500

#define GPS_BUFFER_MAX_SIZE		512

#define DLY_400MS			400
#define DLY_100MS			100
#define DLY_200MS			200
#define DLY_300MS			300
#define DLY_500MS			500
#define DLY_600MS			600
#define DLY_700MS			700
#define DLY_800MS			800
#define DLY_900MS			900
#define DLY_1200MS			1200
#define DLY_3000MS			3000


#define vGPS_GenPulse()					\
	{ 									\
	GPIO_SetBits(GPIOE, GPIO_Pin_1);	\
	os_dly_wait(DLY_400MS/OS_TICK_RATE_MS);	\
	GPIO_ResetBits(GPIOE, GPIO_Pin_1);	\
	}

#if GH12_HW_VERSION == 1

#define vGPS_nRESET_Off()			GPIO_SetBits(GPIOE, GPIO_Pin_3)
#define vGPS_nRESET_On()			GPIO_ResetBits(GPIOE, GPIO_Pin_3)
#define GPS_PORT			USART3

#elif GH12_HW_VERSION == 2

#define vGPS_PowerSupplyOn()			GPIO_SetBits(GPIOE, GPIO_Pin_4)
#define vGPS_PowerSupplyOff()			GPIO_ResetBits(GPIOE, GPIO_Pin_4)
#define SIM18_RCC_PORT		RCC_APB2Periph_GPIOE

#define GPS_PORT			UART5
#define GPS_RCC_Periph		RCC_APB1Periph_UART5

#define GPS_RCC_IO_PORT_TX		RCC_APB2Periph_GPIOC
#define GPS_RCC_IO_PORT_RX		RCC_APB2Periph_GPIOD

#define GPS_IO_PORT_TX		GPIOC
#define GPS_IO_PORT_RX		GPIOD

#define GPS_IRQ				UART5_IRQn

#define SIM18_SUPPLY_PIN			GPIO_Pin_4
#define SIM18_SUPPLY_PORT			GPIOE

#endif

#define GPS_IRQHandler		UART5_IRQHandler
#define vGPS_Putc(c)		USART_PrChar(GPS_PORT, c)

#if GH12_HW_VERSION == 1

#define vGLL_MSG_OFF()		vSIM18_Puts("$PSRF103,01,00,00,01*25\r\n")
#define vGSA_MSG_OFF()		vSIM18_Puts("$PSRF103,02,00,00,01*26\r\n");
#define vGSV_MSG_OFF()		vSIM18_Puts("$PSRF103,03,00,00,01*27\r\n");
#define vVTR_MSG_OFF()		vSIM18_Puts("$PSRF103,05,00,00,01*21\r\n");
#define vRMC_MSG_1S()		vSIM18_Puts("$PSRF103,04,00,01,01*21\r\n");
#define vGGA_MSG_1S()		vSIM18_Puts("$PSRF103,00,00,01,01*25\r\n");

#endif

/* new feature */
#define SIM18_ONOFF_PIN		GPIO_Pin_1
#define	SIM18_ONOFF_PORT	GPIOE

#define SIM18_STATUS_PIN	GPIO_Pin_0
#define	SIM18_STATUS_PORT	GPIOE
#define blSIM18Status()		GPIO_ReadInputDataBit(SIM18_STATUS_PORT, SIM18_STATUS_PIN)

/*** ---------Typedef _____________________________****/

/**
 * mo module sim18
 */
BOOL blSIM18_Open(void);
void vSIM18_Close(void);
void vSIM18_GPIOConfig(void);

// data
BOOL blGetBuffer(S8 *ch);
void vWriteBuffer(S8 ch);

#define vGPS_Callback		vWriteBuffer
#define blSIM18_Status		bGPSStatus

#if GH12_HW_VERSION == 2
void vSIM18_UART_Init(void);
void vSIM18_UART_baud(U32 baud);
void vSIM18_Puts(const U8* str);
#endif

#endif /* SIM18_H_ */
