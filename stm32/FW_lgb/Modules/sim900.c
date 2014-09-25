/*
 * sim900.c
 *
 *  Created on: Jan 10, 2012
 *      Author: Van Ha
 *
 *      version 1.0
 *      	connect server, send data 10s. data read in ram.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"
#include "stm32f10x_it.h"
#include "platform_config.h"

#include "serial.h"
#include "utils.h"

#include "sim900.h"
#include "debug.h"


/*****  local function _____________________________________________*****/
void vSIM90_UART_Init(void);

/*****  local variables ____________________________________________****/
S8 sim900_Buf[2048];
U16 sim_r_indx, sim_w_indx;


/* Global variables ----------------------------------------------- */
gsm_mode_t gsm_mode = GSM_NORMAL_MODE;
//gsm_mode_t gsm_mode = GSM_DEBUG_MODE;


/* Functions  ----------------------------------------------- */
void sim900_err(S8 *str)
{
#ifdef SIM900_DEBUG
	if((gsm_mode == GSM_NORMAL_MODE) && (glSystemMode != DEBUG_MODE))
		return;

	USART_PrCStr(DBG_Port,(const uint8_t *)str);
#endif
}


void SIM900_WriteChar(S8 ch)
{
	sim900_Buf[sim_w_indx++] = ch;
	/* over buffer. */
	//if(sim_w_indx == sim_r_indx)
	//	__breakpoint(0);

	if(sim_w_indx >= sizeof(sim900_Buf))
		sim_w_indx = 0;
}

U16 SIM900_ReadChar(S8 *ch)
{
	if(sim_w_indx == sim_r_indx)
		return 0;

	*ch =  sim900_Buf[sim_r_indx++];
	if(sim_r_indx >= sizeof(sim900_Buf))
			sim_r_indx = 0;

	return 1;
}

U16 SIM900_ReadBuf(S8* buf, U16 len)
{
	U16 i;
	S8 ch;

	for(i=0;i<len;i++)
	{
		if(SIM900_ReadChar(&ch) == 0)
			break;

		buf[i] = ch;
	}

	return i;
}

U16 SIM900_GetItemSize(void)
{
	if(sim_r_indx > sim_w_indx)
	{
		return (sizeof(sim900_Buf) - (sim_r_indx - sim_w_indx));
	}
	else
		return (sim_w_indx - sim_r_indx);
}

/**
 * \brief 	send command to sim900
 */
void SIM900_SendCommand(const S8 *str)
{
	os_dly_wait(100/OS_TICK_RATE_MS);
	while((*str != 0)&&(*str != '\r'))
		SIM900_Putc(*str++);

	SIM900_Putc('\r');
}

void SIM900_ClearBuffer(uint32_t timeout)
{
	char ch;
	/*
	 * doc het du lieu trong buffer neu kich thuoc ban tin bi sai lon hon kich thuoc buffer.
	 */
//	while(timeout)
//	{
//		timeout--;
//
//		if(SIM900_ReadChar(&ch)==0)
//			break;
//	}

	sim_w_indx = 0;
	sim_r_indx = 0;

	return;
}


/**
 * \brief 	put const string to uart2
 */
void SIM900_WriteCStr(const S8 *str)
{
	while((*str != 0))
		SIM900_Putc(*str++);
}

/**
 * \brief	write fix length string to uart2
 */
void SIM900_WriteStr(S8* str, U16 len)
{
	while(len--)
		SIM900_Putc(*str++);
}

/**
 * \fn		void vTurnOnPowerSupply(void)
 * \brief	Turn on VCC for GSM module.
 * \param	none.
 * \return 	none.
 **/
void SIM900_TurnOnPowerSupply(void)
{
	SIM900_PowerPinOn();
	os_dly_wait(10/OS_TICK_RATE_MS);
}

/**
 * \fn		void SIM900_TurnOffPowerSupply(void)
 * \brief	Turn off VCC for GSM module.
 * \param	none.
 * \return 	none.
 **/
void SIM900_TurnOffPowerSupply(void)
{
	SIM900_PowerPinOff();
	SIM900_PWRKeyPinLow();
	SIM900_nResetOn();
	os_dly_wait(1000/OS_TICK_RATE_MS);
}

/**
 * \fn		BIT bPWKEY_Pusle(void)
 * \brief	Generation a pulse about 2s on GSM_PWKEY pin of GSM module.
 * \param	none.
 * \return 	status of GSM_STATUS Pin.
 **/
BIT SIM900_PWKEY_Pusle(void)
{
	/**
	 * powerkey: ____ 1,5s  ______
	 * 				 |_____|
	 */
	SIM900_PWRKeyPinLow();
	os_dly_wait(WAIT_2S/OS_TICK_RATE_MS);
	SIM900_PWRKeyPinHigh();
	os_dly_wait(WAIT_3S/OS_TICK_RATE_MS);

	return SIM900_Status();
}


// sync baud
void SIM900_AutoBaud(void)
{
	SIM900_WriteCStr("AT\r");
	os_dly_wait(DLY_RESP/OS_TICK_RATE_MS);

	SIM900_WriteCStr("ATE0\r");
	os_dly_wait(DLY_RESP/OS_TICK_RATE_MS);

	sim_w_indx = 0;
	sim_r_indx = 0;

	os_dly_wait(1500/OS_TICK_RATE_MS);
}

/**
 *  hard reset moudle sim900
 */
BOOL SIM900_HardReset(void)
{
	BOOL ret;
	U16 i;
	
	#ifdef SIM900_DEBUG
	if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
		USART_PrCStr(DBG_Port,"\n\rHard reset SIM900");
	#endif

	/* turn off power supply */
	SIM900_TurnOffPowerSupply();

	/* init module sim900 */
	SIM900_nResetOff();
	SIM900_PWRKeyPinHigh();
	// turn on power supply.
	SIM900_TurnOnPowerSupply();

	if(SIM900_Status() == 1)		// if sim900 running, then shutdown.
	{
		SIM900_PWKEY_Pusle();
	}

	// retry 5 times
	for(i=0;i<5;i++)
	{
		sim900_err("\r\nSIM900...\r\n");
		ret = SIM900_PWKEY_Pusle();

		if(ret)
		{
			sim900_err("\r\nSIM900 ON\r\n");
			// turn off echo.
			SIM900_AutoBaud();
			return 1;
		}
	}
	return 0;
}

/**
 *  soft reset moudle sim900
 */
BOOL lbSim900_SoftReset(void)
{
	/**
	 * NRESET: ____ 50 uS <___1.2s___>___
	 * 			   |_____|
	 * STATUS: ______                ____
	 * 				 |______________|
	 */

	/*Create NRESET timing*/
	uint16_t tmo;

	#ifdef SIM900_DEBUG
	if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
		USART_PrCStr(DBG_Port,"\n\rSoft reset SIM900");
	#endif

	SIM900_nResetOn(); //Pull down NRESET
	tmo = 50;
	while(SIM900_Status() == 1)
	{
		os_dly_wait(10/OS_TICK_RATE_MS);
		tmo --;
		if(tmo == 0)
			return 0; //Status no change after pull down NRESET
	}

	SIM900_nResetOff(); //Pull up NRESET
	tmo = 400;
	while(SIM900_Status() == 0)
	{
		os_dly_wait(10/OS_TICK_RATE_MS);
		tmo --;
		if(tmo == 0)
			return 0;
	}

	// turn off echo.
	SIM900_AutoBaud();

	return 1;
}

/**
 *  open moudle sim900
 */
BOOL blSim900_Open(void)
{
	BOOL ret;
	U16 try;
	static U16 DoSoftReset = 0;
	try = 3;

	//Although continously soft reset in 5 times, force hard reset
	if((SIM900_Status() == 1) && (DoSoftReset < 5))
	{
		while(try)
		{
			if(lbSim900_SoftReset())
			{	
				DoSoftReset ++;
				return 1;
			}
			else 
				try --;
		}
		
		if(try == 0)
		{
			ret = SIM900_HardReset();
			DoSoftReset = 0;
		}
			
	}
	else
	{
		ret = SIM900_HardReset();
		DoSoftReset = 0;
	}
	return ret;
}

/**
 * close
 */
void vSim900_Close(void)
{
	// if sim900 running, then shutdown.
	if(SIM900_Status() == 1)
	{
		SIM900_PWKEY_Pusle();
	}
}

/******* uart interface **************************************/
void vSIM900_UART_GPIOConfig(void)
{
	GPIO_InitTypeDef config;

	/* Enable GPIOA clocks */
	RCC_APB2PeriphClockCmd(SIM900_RCC_Periph_PORT | RCC_APB2Periph_AFIO, ENABLE);
	/* Enable UART clock, if using USART2 or USART3 ... we must use RCC_APB1PeriphClockCmd(RCC_APB1Periph_USARTx, ENABLE) */
	RCC_APB1PeriphClockCmd(SIM900_RCC_Periph_UART, ENABLE);

	config.GPIO_Mode = GPIO_Mode_AF_PP;
	config.GPIO_Pin = SIM900_GPIO_PIN_TX;
	config.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(SIM900_GPIO_PORT_TX, &config);
	// Rx
	config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	config.GPIO_Pin = SIM900_GPIO_PIN_RX;

	GPIO_Init(SIM900_GPIO_PORT_RX, &config);

	// pin driver
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
			            RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOE, ENABLE);

	// PWER KEY
	config.GPIO_Mode = GPIO_Mode_Out_PP ;
	config.GPIO_Pin = SIM900_PWKEY_PIN;

	GPIO_Init(SIM900_PWKEY_PORT, &config);

	// SUPPLY
	config.GPIO_Mode = GPIO_Mode_Out_PP ;
	config.GPIO_Pin = SIM900_SUPPLY_PIN;

	GPIO_Init(SIM900_SUPPLY_PORT, &config);

	// RESET
	config.GPIO_Mode = GPIO_Mode_Out_PP ;
	config.GPIO_Pin = SIM900_RESET_PIN;

	GPIO_Init(SIM900_RESET_PORT, &config);

	// status
	config.GPIO_Mode = GPIO_Mode_IPD  ;
	config.GPIO_Pin = SIM900_STATUS_PIN;

	GPIO_Init(SIM900_STATUS_PORT, &config);
}

void vSIM90_UART_Init(void)
{
	USART_InitTypeDef config;
	NVIC_InitTypeDef NVIC_InitStructure;

	vSIM900_UART_GPIOConfig();

	config.USART_BaudRate = 115200;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	/* Configure the NVIC Preemption Priority Bits */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);

	/* Enable the USARTy Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = SIM900_IRQ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	NVIC_EnableIRQ(SIM900_IRQ);

	USART_Init(GSM_PORT, &config);

	USART_ITConfig(GSM_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(GSM_PORT, ENABLE);

}

// irq
void SIM900_IRQHandler(void)
{
	S8 c;
	if(USART_GetITStatus(GSM_PORT, USART_IT_RXNE) != RESET)
	{
		c = USART_ReceiveData(GSM_PORT);

#ifdef SIM900_DEBUG
		if((gsm_mode == GSM_DEBUG_MODE) && (glSystemMode == DEBUG_MODE))
			USART_SendData(USART1, (uint8_t)c);
#endif

		vGSM_Callback(c);
	}
}

