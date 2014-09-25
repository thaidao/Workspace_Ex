#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"
#include "stm32f10x_it.h"
#include "platform_config.h"

#include "serial.h"
#include "utils.h"

#include "sim18.h"

/**
 * ------------- macro -----------------------------------
 */

/*********** TYPEDEF ******************************************/

/*_______local functions _____________________________________*/
BOOL blGetBuffer(S8 *ch);

/**______ LOCAL VARIABLES ______________________________________________*/
S8 GPS_Buffer[GPS_BUFFER_MAX_SIZE];
U16 r_indx, w_indx;

/**_______GLOBAL VARIABLES_______________________________________*/


/********* FUNCTIONS ******************************/
/**
 * send command to sim18
 */
void vSIM18_Puts(const U8* str)
{
	while(*str !=0)
		vGPS_Putc(*str++);
}


/**
 * open sim18
 */
BOOL blSIM18_Open(void)
{
	vGPS_PowerSupplyOn();

	w_indx = 0;
	r_indx = 0;

	/* wait for Vgps stable */
	os_dly_wait(DELAY_2S/OS_TICK_RATE_MS);

	// turn on module gps.
	vGPS_GenPulse();

	os_dly_wait(500/OS_TICK_RATE_MS);

	return 1;
}

void vSIM18_Close(void)
{
	vGPS_GenPulse();
}

/********** FUNCTION **********************************/
/**
 * read a char from buffer
 */
BOOL blGetBuffer(S8 *ch)
{
	// k co du lieu trong buffer.
	if(r_indx == w_indx)
		return 0;

	*ch = GPS_Buffer[r_indx++];
	if(r_indx >= GPS_BUFFER_MAX_SIZE)
		r_indx = 0;

	return 1;
}

/*
 * write a char to buffer, call in interrupt routine.
 */
void vWriteBuffer(S8 ch)
{
	GPS_Buffer[w_indx++] = ch;
	if(w_indx >= GPS_BUFFER_MAX_SIZE)
		w_indx = 0;
}

#if GH12_HW_VERSION == 2

void vSIM18_GPIOConfig(void)
{

	GPIO_InitTypeDef config;
	//
	// SUPPLY
	config.GPIO_Mode = GPIO_Mode_Out_PP ;
	config.GPIO_Pin = SIM18_SUPPLY_PIN;
	config.GPIO_Speed = GPIO_Speed_50MHz;
	vGPS_PowerSupplyOn();

	GPIO_Init(SIM18_SUPPLY_PORT, &config);

	/* status pin */
	RCC_APB2PeriphClockCmd(SIM18_RCC_PORT, ENABLE);

	config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	config.GPIO_Pin = SIM18_STATUS_PIN;
	GPIO_Init(SIM18_STATUS_PORT, &config);
	/* on/off pin */
	config.GPIO_Mode = GPIO_Mode_Out_PP;
	config.GPIO_Pin = SIM18_ONOFF_PIN;
	config.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_ResetBits(SIM18_ONOFF_PORT, SIM18_ONOFF_PIN);

	GPIO_Init(SIM18_ONOFF_PORT, &config);

}


/****** uart *************************************/
void vGPIO_UARTConfig(void)
{
	GPIO_InitTypeDef config;

	/* Enable GPIOC clocks */
	RCC_APB2PeriphClockCmd(GPS_RCC_IO_PORT_TX | GPS_RCC_IO_PORT_RX | RCC_APB2Periph_AFIO, ENABLE);
	/* Enable UART clock, if using USART2 or USART3 ... we must use RCC_APB1PeriphClockCmd(RCC_APB1Periph_USARTx, ENABLE) */
	RCC_APB1PeriphClockCmd(GPS_RCC_Periph, ENABLE);

	config.GPIO_Mode = GPIO_Mode_AF_PP;
	config.GPIO_Pin = GPIO_Pin_12;
	config.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPS_IO_PORT_TX, &config);
	// Rx
	config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	config.GPIO_Pin = GPIO_Pin_2;//GPIO_Pin_3;

	GPIO_Init(GPS_IO_PORT_RX, &config);

}

void vSIM18_UART_baud(U32 baud)
{
	USART_InitTypeDef config;

	config.USART_BaudRate = baud;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	USART_Init(GPS_PORT, &config);

	USART_ITConfig(GPS_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(GPS_PORT, ENABLE);
}

void vSIM18_UART_Init(void)
{
	USART_InitTypeDef config;
	NVIC_InitTypeDef NVIC_InitStructure;

	vSIM18_GPIOConfig();
	vGPIO_UARTConfig();

	config.USART_BaudRate = 4800;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	USART_Init(GPS_PORT, &config);

	USART_ITConfig(GPS_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(GPS_PORT, ENABLE);

	/* Enable the USARTy Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = GPS_IRQ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/* Configure the NVIC Preemption Priority Bits */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
}
#endif

// irq
void GPS_IRQHandler(void)
{
	S8 c;
	if(USART_GetITStatus(GPS_PORT, USART_IT_RXNE) != RESET)
	{
		c = USART_ReceiveData(GPS_PORT);
		//USART_PrChar(USART1, c);
		// write buffer.
		vWriteBuffer(c);
	}
}
