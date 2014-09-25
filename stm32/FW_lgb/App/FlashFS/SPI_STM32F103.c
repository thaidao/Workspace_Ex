/*----------------------------------------------------------------------------
 *      RL-ARM - FlashFS
 *----------------------------------------------------------------------------
 *      Name:    SPI_STM32F103.c
 *      Purpose: Serial Peripheral Interface Driver for ST STM32F103
 *      Rev.:    V4.22
 *----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2011 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/

#include <File_Config.h>
#include "stm32f10x.h"	  						  /* Standard Peripheral header files */


/*----------------------------------------------------------------------------
  SPI Driver instance definition
   spi0_drv: First SPI driver
   spi1_drv: Second SPI driver
 *---------------------------------------------------------------------------*/
/* SPI Port configuration */
#define DEFAULT_SPI_MODULE
#ifndef DEFAULT_SPI_MODULE
#define SPIx					SPI2
#else
#define SPIx			SPI1
#endif


#define __DRV_ID  spi0_drv
#define __FPCLK   72000000	

/* SPI Driver Interface functions */
static BOOL Init (void);
static BOOL UnInit (void);
static U8   Send (U8 outb);
static BOOL SendBuf (U8 *buf, U32 sz);
static BOOL RecBuf (U8 *buf, U32 sz);
static BOOL BusSpeed (U32 kbaud);
static BOOL SetSS (U32 ss);
static U32  CheckMedia (void);        /* Optional function for SD card check */

/* SPI Device Driver Control Block */
SPI_DRV __DRV_ID = {
  Init,
  UnInit,
  Send,
  SendBuf,
  RecBuf,
  BusSpeed,
  SetSS,
  CheckMedia                          /* Can be NULL if not existing         */
};

/* SPI_SR - bit definitions. */
#define RXNE    0x01
#define TXE     0x02
#define BSY     0x80
#define FPCLK   (__FPCLK/1000)

/*--------------------------- Init ------------------------------------------*/

static BOOL Init (void) {
SPI_InitTypeDef  SPI_InitStructure;
GPIO_InitTypeDef GPIO_InitStructure;

/* Enable peripheral clock for SPI Module */
#ifdef DEFAULT_SPI_MODULE
	/* Enable clock for GPIOA and SPI1 clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);

	/* Configure SPI1 pins: SCK, MISO and MOSI ---------------------------------*/
	/* Confugure SCK and MOSI pins as Alternate Function Push Pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/* Confugure MISO pin as Input Floating  */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	/* Configure NSS as output: GPIOA4 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOA, GPIO_Pin_4);

	/* Power SD Module: PB0 */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_SetBits(GPIOB, GPIO_Pin_0);
	GPIO_SetBits(GPIOA, GPIO_Pin_4);

#else
	/* Enable clock for GPIOB and SPI2 clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

	/* Confugure SCK and MOSI pins as Alternate Function Push Pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/* Confugure MISO pin as Input Floating  */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Configure NSS as outptut: GPIOB12 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_SetBits(GPIOB, GPIO_Pin_12);

	/* HY-Firebull KIT : Power SD Module */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOC, GPIO_Pin_8);
#endif
/* Initialize and enable the SSP Interface module. */

	/* SPI Config: SPI_Clk = fPBCLK1/256 = 280kHz at 72Mhz PCLK1 clk */
	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
	SPI_InitStructure.SPI_CRCPolynomial = 7;
	SPI_Init(SPIx, &SPI_InitStructure);

	// Enable SPIx
	SPI_Cmd(SPIx, ENABLE);

  return (__TRUE);
}


/*--------------------------- UnInit ----------------------------------------*/

static BOOL UnInit (void) {
  /* Return SSP interface to default state. */ 

  GPIOA->BSRR =  0x00100000;

  GPIOA->CRL &= ~0xFFFF0000;
  GPIOA->CRL |=  0x44440000;

	/*
  SPIx->CR1  = 0x0000;
  SPIx->CR2  = 0x0000;
	*/
	
	/* Deinit SPI Module */
	SPI_I2S_DeInit(SPIx);

  /* turn off sdcard power */
  GPIO_ResetBits(GPIOA, GPIO_Pin_4);
  GPIO_ResetBits(GPIOB, GPIO_Pin_0);

  return (__TRUE);
}


/*--------------------------- Send ------------------------------------------*/

static U8 Send (U8 outb) {
  /* Write and Read a byte on SPI interface. */
  SPIx->DR = outb;
  /* Wait if RNE cleared, Rx FIFO is empty. */
  while (!(SPIx->SR & RXNE));
  return (SPIx->DR);
}


/*--------------------------- SendBuf ---------------------------------------*/

static BOOL SendBuf (U8 *buf, U32 sz) {
  /* Send buffer to SPI interface. */
  U32 i;

  for (i = 0; i < sz; i++) {
	  SPIx->DR = buf[i];
	  /* Wait if TXE cleared, Tx FIFO is full. */
	  while (!(SPIx->SR & TXE));
	  SPIx->DR;
  }
  /* Wait until Tx finished, drain Rx FIFO. */
  while (SPIx->SR & (BSY | RXNE)) {
	  SPIx->DR;
  }
  return (__TRUE);
}


/*--------------------------- RecBuf ----------------------------------------*/

static BOOL RecBuf (U8 *buf, U32 sz) {
  /* Receive SPI data to buffer. */
  U32 i;

  for (i = 0; i < sz; i++) {
	  SPIx->DR = 0xFF;
	  /* Wait if RNE cleared, Rx FIFO is empty. */
	  while (!(SPIx->SR & RXNE));
	  buf[i] = SPIx->DR;
  }
  return (__TRUE);
}


/*--------------------------- BusSpeed --------------------------------------*/

static BOOL BusSpeed (U32 kbaud) {
  /* Set an SPI clock to required baud rate. */
  U8 br;

  if      (kbaud >= FPCLK / 2)   br = 0;                       /* FPCLK/2    */
  else if (kbaud >= FPCLK / 4)   br = 1;                       /* FPCLK/4    */
  else if (kbaud >= FPCLK / 8)   br = 2;                       /* FPCLK/8    */
  else if (kbaud >= FPCLK / 16)  br = 3;                       /* FPCLK/16   */
  else if (kbaud >= FPCLK / 32)  br = 4;                       /* FPCLK/32   */
  else if (kbaud >= FPCLK / 64)  br = 5;                       /* FPCLK/64   */
  else if (kbaud >= FPCLK / 128) br = 6;                       /* FPCLK/128  */
  else                           br = 7;                       /* FPCLK/256  */

  SPIx->CR1 = (SPIx->CR1 & ~(7 << 3)) | (br << 3);
  return (__TRUE);
}       
/*--------------------------- SetSS -----------------------------------------*/

static BOOL SetSS (U32 ss) {
  /* Enable/Disable SPI Chip Select (drive it high or low). */

#ifdef DEFAULT_SPI_MODULE
	GPIO_WriteBit(GPIOA, GPIO_Pin_4, ss);
#else
	GPIO_WriteBit(GPIOB, GPIO_Pin_12, ss);
#endif

  return (__TRUE);
}


/*--------------------------- CheckMedia ------------------------------------*/
// Return: M_INSERTED, M_PROTECTED
static U32 CheckMedia (void) {
  /* Read CardDetect and WriteProtect SD card socket pins. */

  return (M_INSERTED);
}

/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
