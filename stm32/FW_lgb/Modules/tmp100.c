/* Include header files */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "string.h"
#include "tmp100.h"
#include "Platform_Config.h"

/* Private macros ------------------------------------ */
#define i2c_start()			I2C_GenerateSTART(TMP100_I2C, ENABLE)
#define i2c_stop()			I2C_GenerateSTOP(TMP100_I2C, ENABLE)

/* I2C Routine ------------------------------------------------------- */

/* Check flag */
int8_t I2C_WaitFlag(I2C_TypeDef* I2Cx, uint32_t I2C_FLAG, uint32_t timeout){
	while(I2C_GetFlagStatus(I2Cx, I2C_FLAG) != SET){
		if(timeout-- == 0)
			return -1;
	};

	return 0;
}

/* Check event */
int8_t I2C_WaitEvent(I2C_TypeDef* I2Cx, uint32_t I2C_EVT, uint32_t timeout){

	while (!I2C_CheckEvent(I2Cx, I2C_EVT))
	{
	   if((timeout--) == 0)
		   return -1;
	}

	return 0;
}

#define i2c_wait_evt(tmo, evt)			I2C_WaitEvent(TMP100_I2C, evt, tmo)
#define i2c_wait_flag(tmo, flag)		I2C_WaitFlag(TMP100_I2C, flag, tmo)

/**
 * \brief 	write a byte to i2c
 **/
static S8 i2c_write(S8 c)
{
	I2C_SendData(TMP100_I2C, c);

	return i2c_wait_evt(1000, I2C_EVENT_MASTER_BYTE_TRANSMITTED);
}

/* TMP100 Routine ---------------------------------------------------- */

/* TMP100 LowLevel Init */
void TMP100_LowLevel_Init(void)
{
GPIO_InitTypeDef  GPIO_InitStructure;

	/* Enable Clock */
	RCC_APB1PeriphClockCmd(TMP100_I2C_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(TMP100_I2C_SCL_GPIO_CLK, ENABLE);

	/*!< Configure TMP100_I2C pins: SDA */
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
	GPIO_InitStructure.GPIO_Pin = TMP100_I2C_SDA_PIN | TMP100_I2C_SCL_PIN;
	GPIO_Init(TMP100_I2C_SDA_GPIO_PORT, &GPIO_InitStructure);
}

/* TMP100 Init */
void TMP100_Init(void){
I2C_InitTypeDef   I2C_InitStructure;

	/* Init low level */
	TMP100_LowLevel_Init();

	I2C_DeInit(TMP100_I2C);

	/*!< TMP100_I2C Init */
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0xAA;
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	//I2C_InitStructure.I2C_Ack = I2C_Ack_Disable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = TMP100_I2C_SPEED;

	/*!< Enable SMBus Alert interrupt */
	//I2C_ITConfig(TMP100_I2C, I2C_IT_ERR, ENABLE);

	/*!< TMP100_I2C Init */
	I2C_Cmd(TMP100_I2C, ENABLE);
	I2C_Init(TMP100_I2C, &I2C_InitStructure);

}


int8_t TMP100_Write(uint8_t addr, int16_t d, int size)
{
	int8_t *s = (int8_t*)&d;
	// wait busy
	i2c_wait_flag(10000, I2C_FLAG_BUSY);
	// start condition
	i2c_start();
	// wait event
	i2c_wait_evt(1000, I2C_EVENT_MASTER_MODE_SELECT);
	// write device address.
	// send slave address.
	I2C_Send7bitAddress(TMP100_I2C, TMP100_ADDR, I2C_Direction_Transmitter);
	// wait event
	if(i2c_wait_evt(10000, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)!=0)
		return -1;

	// send address in memory.
	i2c_write(addr);

	// write data
	for(;size;size--)
	{
		i2c_write(*s++);
	}

	/* Send T_I2C_INTERFACE STOP Condition */
	i2c_stop();
	return 0;
}


int8_t TMP100_Read(uint8_t addr, uint8_t* out, int size)
{

	int8_t ret = 0;
	/* debug */

	// setup ack.
	I2C_AcknowledgeConfig(TMP100_I2C, ENABLE);

	// wait busy
	i2c_wait_flag(10000, I2C_FLAG_BUSY);
	// start condition
	i2c_start();
	// wait event
	i2c_wait_evt(1000, I2C_EVENT_MASTER_MODE_SELECT);
	// write device address.
	// send slave address.
	I2C_Send7bitAddress(TMP100_I2C, TMP100_ADDR, I2C_Direction_Transmitter);
	// wait event
	if(i2c_wait_evt(1000, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)!=0){
		ret = -1;
		goto t_exit;
	}

	// send address in memory.
	i2c_write(addr);

	// restart
	i2c_start();
	// wait event
	i2c_wait_evt(1000, I2C_EVENT_MASTER_MODE_SELECT);
	I2C_Send7bitAddress(TMP100_I2C, TMP100_ADDR, I2C_Direction_Receiver);
	// wait event
	if(i2c_wait_evt(1000, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)!=0){
		ret = -1;
		goto t_exit;
	}

	// reading.
	for(;size>1;size--)
	{
		// next byte
		i2c_wait_evt(1000, I2C_EVENT_MASTER_BYTE_RECEIVED);
		*out++ = I2C_ReceiveData(TMP100_I2C);
	}
	// setup noack.
	I2C_AcknowledgeConfig(TMP100_I2C, DISABLE);
	// read end byte
	i2c_wait_evt(1000, I2C_EVENT_MASTER_BYTE_RECEIVED);
	*out = I2C_ReceiveData(TMP100_I2C);

t_exit:
	/* Send T_I2C_INTERFACE STOP Condition */
	i2c_stop();

	return ret;
}

#if 0
/* Write data */
int8_t TMP100_Read(uint8_t addr, uint8_t* tempVal){
int8_t res;
uint8_t data[2];
uint32_t TMP100_Timeout;

/* Write address to TMP100 */
	/* Enable TMP100 I2C */
	I2C_AcknowledgeConfig(TMP100_I2C, DISABLE);

	/* Check busy flag */
	TMP100_Timeout = TMP100_LONG_TIMEOUT;
	while (I2C_GetFlagStatus(TMP100_I2C,I2C_FLAG_BUSY))
	{
	   if((TMP100_Timeout--) == 0)
		   return -1;
	}

	/* Start I2C */
	I2C_GenerateSTART(TMP100_I2C, ENABLE);

	res = I2C_WaitEvent(TMP100_I2C, I2C_EVENT_MASTER_MODE_SELECT, TMP100_LONG_TIMEOUT);
	if(res == -1)
		return -1;

	/* Send device address for write */
	I2C_Send7bitAddress(TMP100_I2C, TMP100_ADDR, I2C_Direction_Transmitter);

	res = I2C_WaitEvent(TMP100_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, TMP100_FLAG_TIMEOUT);
		if(res == -1)
			return -1;

	/* Write reg to read from */
	I2C_SendData(TMP100_I2C, TMP100_TEMP_REG);
	res = I2C_WaitEvent(TMP100_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED, TMP100_FLAG_TIMEOUT);
	if(res == -1) return -1;

/* Read data */

	/* Restart I2C */
	I2C_GenerateSTART(TMP100_I2C, ENABLE);
	res = I2C_WaitEvent(TMP100_I2C, I2C_EVENT_MASTER_MODE_SELECT, TMP100_FLAG_TIMEOUT);
	if(res == -1) return -1;

	/* Send device address for read */
	I2C_Send7bitAddress(TMP100_I2C, TMP100_ADDR, I2C_Direction_Receiver);
	res = I2C_WaitEvent(TMP100_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, TMP100_FLAG_TIMEOUT);
	if(res == -1) return -1;

	/* Read only one byte */
	res = I2C_WaitEvent(TMP100_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED, TMP100_FLAG_TIMEOUT);
	if(res == -1) return -1;

	data[0] =  I2C_ReceiveData(TMP100_I2C);

	//I2C_AcknowledgeConfig(TMP100_I2C, DISABLE);
	res = I2C_WaitEvent(TMP100_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED, TMP100_FLAG_TIMEOUT);
	if(res == -1) return -1;

	data[1] =  I2C_ReceiveData(TMP100_I2C);

	/* Generate STOP */
	I2C_GenerateSTOP(TMP100_I2C, ENABLE);

/* assign value */
	//*tempVal = data[0];

	return 0;
}
#endif


