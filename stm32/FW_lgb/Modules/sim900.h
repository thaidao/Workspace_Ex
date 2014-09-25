/*
 * sim900.h
 *
 *  Created on: Jan 10, 2012
 *      Author: Van Ha
 */

#ifndef SIM900_H_
#define SIM900_H_

/**
 * ----------- MACRO DEFINE ---------------------------
 */
#if GH12_HW_VERSION == 2

#define SIM900_PowerPinOff()		GPIO_ResetBits(SIM900_SUPPLY_PORT, SIM900_SUPPLY_PIN)	//GSM_POWER_SUPPLY = 0
#define SIM900_PowerPinOn()			GPIO_SetBits(SIM900_SUPPLY_PORT, SIM900_SUPPLY_PIN)		//GSM_POWER_SUPPLY = 1

#define SIM900_RCC_Periph_PORT		RCC_APB2Periph_GPIOA
#define SIM900_RCC_Periph_UART		RCC_APB1Periph_USART2
#define SIM900_GPIO_PIN_TX			GPIO_Pin_2
#define SIM900_GPIO_PIN_RX			GPIO_Pin_3
#define SIM900_GPIO_PORT_TX			GPIOA
#define SIM900_GPIO_PORT_RX			GPIOA

#define SIM900_PWKEY_PIN			GPIO_Pin_5
#define SIM900_PWKEY_PORT			GPIOC

#define SIM900_SUPPLY_PIN			GPIO_Pin_7
#define SIM900_SUPPLY_PORT			GPIOE

#define SIM900_STATUS_PIN			GPIO_Pin_3
#define SIM900_STATUS_PORT			GPIOC

#define SIM900_RESET_PIN			GPIO_Pin_1
#define SIM900_RESET_PORT			GPIOA

#define SIM900_IRQ					USART2_IRQn
#define SIM900_IRQHandler			USART2_IRQHandler

#define SIM900_PWRKeyPinHigh()		GPIO_SetBits(SIM900_PWKEY_PORT, SIM900_PWKEY_PIN) 	//GSM_POWERKEY = 1;
#define SIM900_PWRKeyPinLow()		GPIO_ResetBits(SIM900_PWKEY_PORT, SIM900_PWKEY_PIN) 	//GSM_POWERKEY = 0;

#elif GH12_HW_VERSION == 1
	#define SIM900_PowerPinOff()	GPIO_ResetBits(GPIOB, GPIO_Pin_1)	//GSM_POWER_SUPPLY = 0
	#define SIM900_PowerPinOff()		GPIO_SetBits(GPIOB, GPIO_Pin_1)		//GSM_POWER_SUPPLY = 1

#define vGSMPowerKeyLow()		GPIO_SetBits(GPIOC, GPIO_Pin_5) 	//GSM_POWERKEY = 1;
#define vGSMPowerKeyHigh()		GPIO_ResetBits(GPIOC, GPIO_Pin_5) 	//GSM_POWERKEY = 0;

#endif



#define SIM900_nResetOn()			GPIO_ResetBits(SIM900_RESET_PORT, SIM900_RESET_PIN)
#define SIM900_nResetOff()			GPIO_SetBits(SIM900_RESET_PORT, SIM900_RESET_PIN)

#define SIM900_Status()				GPIO_ReadInputDataBit(SIM900_PWKEY_PORT, SIM900_STATUS_PIN) //GSM_STATUS

// define tick clock in platform_config.h
//OS_TICK_CLOCK/1000	// tick.

#define WAIT_2S					2000		// 2s
#define SEND_TIMEOUT			20000		// 20s
#define WAIT_3S					5000		// 5s

#define DLY_RESP				50			// 500ms
#define DLY_CGREG				50
#define DLY_CONNECT				50

#define GSM_PORT				USART2

#define SIM900_Putc(c)			USART_PrChar(GSM_PORT, c)



/**** Typedef & enum ______________________________________________******/

typedef BOOL (*blResCallback) (char *);

/* Global variables -------------- */
typedef enum{
	GSM_NORMAL_MODE,			/* Normal mode */
	GSM_DEBUG_MODE				/* Debug mode */
} gsm_mode_t;

extern gsm_mode_t gsm_mode;

/**
 * ------------ FUNCTIONS PROTOTYPE ---------------------------------------
 */
//init
BOOL blSim900_Open(void);
void vSim900_Close(void);

// uart funciton
void SIM900_WriteChar(S8 ch);

// buffer
U16 SIM900_ReadChar(S8 *ch);
U16 SIM900_GetItemSize(void);
void SIM900_ClearBuffer(uint32_t timeout);

// send to sim900
void SIM900_SendCommand(const S8 *str);
void SIM900_WriteCStr(const S8 *str);
void SIM900_WriteStr(S8* str, U16 len);

#define vGSM_Callback	SIM900_WriteChar
#define bSim900_Status	SIM900_Status

void vSIM90_UART_Init(void);
/**
 *  soft reset moudle sim900
 */
BOOL lbSim900_SoftReset(void);

#endif /* SIM900_H_ */
