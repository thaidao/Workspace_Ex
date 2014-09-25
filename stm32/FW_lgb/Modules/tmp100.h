#ifndef _TMP100_MODULE_
#define _TMP100_MODULE_

/* TMP100 IO Configuration */
#define TMP100_I2C							I2C2
#define TMP100_I2C_CLK						RCC_APB1Periph_I2C2
#define TMP100_I2C_SCL_PIN					GPIO_Pin_10					/* PB.10 */
#define TMP100_I2C_SCL_GPIO_PORT            GPIOB                       /* GPIOB */
#define TMP100_I2C_SCL_GPIO_CLK             RCC_APB2Periph_GPIOB

#define TMP100_I2C_SDA_PIN                  GPIO_Pin_11                  /* PB.11 */
#define TMP100_I2C_SDA_GPIO_PORT            GPIOB                       /* GPIOB */
#define TMP100_I2C_SDA_GPIO_CLK             RCC_APB2Periph_GPIOB

#define TMP100_I2C_SPEED					100000						/* 100k */

#define TMP100_ADDR							0x90

/* Timeout */
#define TMP100_FLAG_TIMEOUT					0xffff
#define TMP100_LONG_TIMEOUT					(uint32_t)(10 * TMP100_FLAG_TIMEOUT)

/* TMP100 Registers */
#define	TMP100_TEMP_REG						0x00
#define	TMP100_CONFIG_REG					0x01
#define	TMP100_TLOW_REG						0x02
#define	TMP100_THIGH_REG					0x03


/* Configuration value */
#define CFG_VALUE_SD						0x01		/* shutdown mode */
#define CFG_VALUE_TM						0x02		/* limit mode */

#define CFG_VALUE_RES_9						0b00000000 /* 9 bits 0.5 - convert time 40ms */
#define CFG_VALUE_RES_10					0b00010000 /* 10 bits 0.25 - 80ms */
#define CFG_VALUE_RES_11					0b00100000 /* 11 bits 0.125 - 160ms */
#define CFG_VALUE_RES_12					0b00110000 /* 12 bits 0.0625C - 320ms */

#define CFG_VALUE_OS						0b01000000 /* single convertion when shutdown */

int8_t TMP100_Read(uint8_t addr, uint8_t* out, int size);
int8_t TMP100_Write(uint8_t addr, int16_t value, int size);

/* API */
void TMP100_Init(void);

#define TMP100_ReadTemp(tempr)				TMP100_Read(TMP100_TEMP_REG, tempr, 2)
#define TMP100_ReadConfig(cfg)				TMP100_Read(TMP100_CONFIG_REG, cfg, 1)
#define TMP100_ReadTempLow(temprLow)		TMP100_Read(TMP100_TLOW_REG, temprLow, 2)
#define TMP100_ReadTempHigh(temprHigh)		TMP100_Read(TMP100_THIGH_REG, temprHigh, 2)

/* write api */
#define TMP100_WriteConfig(cfg)				TMP100_Write(TMP100_CONFIG_REG, cfg, 1)
#define TMP100_WriteTempLow(templow)		TMP100_Write(TMP100_TLOW_REG, templow, 2)
#define TMP100_WriteTempHigh(temphigh))		TMP100_Write(TMP100_THIGH_REG, temphigh, 2)

#endif
