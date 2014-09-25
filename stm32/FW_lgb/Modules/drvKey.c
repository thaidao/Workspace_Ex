/*
 * drvKey.c
 *
 *  Created on: Feb 22, 2012
 *      Author: Van Ha
 *
 *      do - 2
nau - 3
xanh la cay - 5
tim - 7
ghi - 8
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"
#include "stm32f10x_it.h"
#include "platform_config.h"

#include "debug.h"
#include "utils.h"

#include "drvKey.h"

/*
 * PB6 - SCL
 * PB7 - SDA
 */

/* Private macros ------------------------------------------------- */

#define		DRV_KEY_RCC_Peri		RCC_APB1Periph_I2C1
#define 	DRV_KEY_RCC_PORT		RCC_APB2Periph_GPIOB
#define 	DRV_KEY_PORT			GPIOB

#define 	DRV_KEY_SCL_PIN			GPIO_Pin_6
#define 	DRV_KEY_SDA_PIN			GPIO_Pin_7

#define 	DRV_KEY_I2C				I2C1
#define 	DRV_KEY_CLOCK			400000

#define 	EPR_24WC_ADDRESS		0xa8						// I2C Address of Driver Key

#define 	DRVER_INFO_LOCATE		16
#define 	DLY_WAIT_READY			20							// 20ms

#define i2c_start()			I2C_GenerateSTART(DRV_KEY_I2C, ENABLE)
#define i2c_stop()			I2C_GenerateSTOP(DRV_KEY_I2C, ENABLE)

/* Private functions -------------------------------------------- */
BOOL blEpr24WC_Read(U8 add, S8* out, U16 size);
BOOL blEpr24WC_Write(U8 add, S8* d, U16 size);


/*** local variables *********************************/
const S8 *_key = "GH12-KEY";

/* Config USB Driver GPIO */
void vPinoutConfig(void)
{
	GPIO_InitTypeDef config;

	/*!< I2C Periph clock enable */
	RCC_APB1PeriphClockCmd(DRV_KEY_RCC_Peri, ENABLE);
	RCC_APB2PeriphClockCmd(DRV_KEY_RCC_PORT, ENABLE);

	config.GPIO_Mode = GPIO_Mode_AF_OD;
	config.GPIO_Pin = DRV_KEY_SCL_PIN | DRV_KEY_SDA_PIN;
	config.GPIO_Speed = GPIO_Speed_10MHz;

	GPIO_Init(DRV_KEY_PORT, &config);
}

/* I2C Functions -------------------------------------------------- */

/* Init I2C Bus */
void vDrvInitI2CBus(void)
{
I2C_InitTypeDef info;

	I2C_DeInit(DRV_KEY_I2C);

	info.I2C_Mode = I2C_Mode_I2C;
	info.I2C_DutyCycle = I2C_DutyCycle_2;
	info.I2C_Ack = I2C_Ack_Enable;
	info.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	info.I2C_ClockSpeed = DRV_KEY_CLOCK;
	info.I2C_OwnAddress1 = 0xAA;

	// enable i2c
	I2C_Cmd(DRV_KEY_I2C, ENABLE);
	I2C_Init(DRV_KEY_I2C, &info);
}

/**
 * read string
 */
/**
 * \brief	wait flag i2c with timeout
 **/
static S8 i2c_wait_flg(U16 timeout, U32 flg)
{
	while(I2C_GetFlagStatus(DRV_KEY_I2C, flg))
		if((timeout--)== 0) return -1;

	return 0;
}

/**
 * \brief 	wait event from i2c module with timeout
 **/
static S8 i2c_wait_evt(U16 timeout, U32 evt)
{
	while(!I2C_CheckEvent(DRV_KEY_I2C, evt))
		if((timeout--)==0) return -1;

	return 0;
}

/**
 * \brief 	write a byte to i2c
 **/
static S8 i2c_write(S8 c)
{
	I2C_SendData(DRV_KEY_I2C, c);

	return i2c_wait_evt(1000, I2C_EVENT_MASTER_BYTE_TRANSMITTED);
}


/* Driver key functions ----------------------------------- */


/* Init USB Driver Module */
void vDriverInit(void)
{
	vPinoutConfig();
	vDrvInitI2CBus();
}


BOOL blEpr24WC_Write(U8 add, S8* d, U16 size)
{
	// wait busy
	i2c_wait_flg(10000, I2C_FLAG_BUSY);
	// start condition
	i2c_start();
	// wait event
	i2c_wait_evt(1000, I2C_EVENT_MASTER_MODE_SELECT);
	// write device address.
	// send slave address.
	I2C_Send7bitAddress(DRV_KEY_I2C, EPR_24WC_ADDRESS, I2C_Direction_Transmitter);
	// wait event
	if(i2c_wait_evt(1000, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)!=0)
		return 0;

	// send address in memory.
	i2c_write(add);

	// write data
	for(;size;size--)
	{
		i2c_write(*d++);
	}

	/* Send T_I2C_INTERFACE STOP Condition */
	i2c_stop();

	return 1;
}

BOOL blEpr24WC_Read(U8 add, S8* out, U16 size)
{
	// setup ack.
	I2C_AcknowledgeConfig(DRV_KEY_I2C, ENABLE);

	// wait busy
	i2c_wait_flg(10000, I2C_FLAG_BUSY);
	// start condition
	i2c_start();
	// wait event
	i2c_wait_evt(1000, I2C_EVENT_MASTER_MODE_SELECT);
	// write device address.
	// send slave address.
	I2C_Send7bitAddress(DRV_KEY_I2C, EPR_24WC_ADDRESS, I2C_Direction_Transmitter);
	// wait event
	if(i2c_wait_evt(1000, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)!=0)
		return 0;

	// send address in memory.
	i2c_write(add);

	// restart
	i2c_start();
	// wait event
	i2c_wait_evt(1000, I2C_EVENT_MASTER_MODE_SELECT);
	I2C_Send7bitAddress(DRV_KEY_I2C, EPR_24WC_ADDRESS, I2C_Direction_Receiver);
	// wait event
	if(i2c_wait_evt(1000, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)!=0)
		return 0;

	// reading.
	for(;size>1;size--)
	{
		// next byte
		i2c_wait_evt(1000, I2C_EVENT_MASTER_BYTE_RECEIVED);
		*out++ = I2C_ReceiveData(DRV_KEY_I2C);
	}
	// setup noack.
	I2C_AcknowledgeConfig(DRV_KEY_I2C, DISABLE);
	// read end byte
	i2c_wait_evt(1000, I2C_EVENT_MASTER_BYTE_RECEIVED);
	*out = I2C_ReceiveData(DRV_KEY_I2C);

	/* Send T_I2C_INTERFACE STOP Condition */
	i2c_stop();

	return 1;
}

// api for app
BOOL blWriteInfo(S8 *info, U16 size)
{
S8 *pointer;
U8 add = DRVER_INFO_LOCATE;
BOOL ret;
S8 checksum[2] = {0x55, 0};

	ret = blEpr24WC_Write(0, (S8*)_key, 8);
	if(ret == 0) return 0;

	// delay 10ms for write finish.
	os_dly_wait(DLY_WAIT_READY/OS_TICK_RATE_MS);

	// Calculate checksum
	checksum[1] = cCheckSumFixSize(info, size);

	pointer = info;
	while(size >= 16)
	{
		ret = blEpr24WC_Write(add, pointer, 16);
		if(ret == 0) return 0;

		add += 16;
		pointer += 16;
		size -= 16;
		os_dly_wait(DLY_WAIT_READY/OS_TICK_RATE_MS);
	}

	if(size > 0)
	{
		ret = blEpr24WC_Write(add, pointer, size);
	}

	os_dly_wait(DLY_WAIT_READY/OS_TICK_RATE_MS);
	// write checksum
	ret = blEpr24WC_Write(add + size, checksum, 2);

	return ret;
}

BOOL blReadInfo(S8 *info, U16 size)
{
	BOOL ret;
	S8 checksum[2] = {0, 0};
	S8 chk;

	ret = blEpr24WC_Read(DRVER_INFO_LOCATE, info, size);
	if(ret == 0) return 0;

	ret = blEpr24WC_Read(DRVER_INFO_LOCATE + size, checksum, 2);
	if(ret == 0) return 0;

	// validate checksum
	chk = cCheckSumFixSize(info, size);
	if((checksum[0] == 0x55) && (chk != checksum[1]))
		return 0;

	return 1;
}

/* Verify USB Driver key: GH12-  */
BOOL blVerifyDrvKey(void)
{
	S8 temp[12];
	blEpr24WC_Read(0, temp, 8);

	if(strncmp(temp, _key, 8)== 0)
		return 1;

	return 0;
}
