/* @file		FWUpgrade.c
 * @brief		Decode and copy data from SDCard to Flash memory
 * @author		ngohaibac@gmail.com
 * @date		24/01/20122
 * @changelog
 * 				V0.1	Create module
 */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "FWUpgrade.h"


/* Private macros ----------------------------------------- */

#define FLASH_PAGE_SIZE			 	((uint16_t)0x800)		// 2 KBytes
#define FLASH_SIZE					((uint32_t)0x40000)		// 256 KBytes
#define FW_MAX_SIZE					0x20000

/* Private variables -------------------------------------- */
char FW_Filename[] = "fw\\firmware.hex";			// Firmware filename
FILE* f_fw;											// Firmware file description  

/* Private functions -------------------------------------- */
void error(const uint8_t* str);

/* Firmware download API ---------------------------------- */
/**
 * @file	Init FW Module: create new FW file. Call this function when
 * @param	fw_size		size of firmware
 * @return	-1	Can't create new file OR limited fw file size
 * 			0	Success
 */
int8_t vFWInit(uint32_t fw_size){

	/* Create new FW file */
	f_fw = fopen(FW_Filename, "w");
	if(f_fw == NULL)
		return -1;

	//fclose(f_fw);


	//fdelete(FW_Filename);

	/* Update firmware length */
	if(fw_size > FW_MAX_SIZE)
		return -1;	  

	/* Backup to BKP regs */
	BKP_WriteBackupRegister(BKP_FW_LEN_H, fw_size>>16);
	BKP_WriteBackupRegister(BKP_FW_LEN_L, fw_size);

	/* Enable CRC clock */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);

	return 0;
}

FWUpgrade_Error_t vFWPrChar(uint8_t chr){
int res;

	/* Write char into stream */
	res = fputc(chr, f_fw);
	if(res == EOF)
		return FW_WRITE_ERROR;

	return FW_WRITE_OK;
}

/**
 * @brief 	Write data to microSD
 * @param	data	Pointer to data
 * @param	len		length of data
 *
 */
FWUpgrade_Error_t vFWWrite(uint8_t* data, uint16_t len){
uint32_t write_cnt;

	/* Open fw file */
#if 0
	f_fw = fopen(FW_Filename, "a");
	if(f_fw == NULL){
		error("Error: open FW file \r\n");
		return FW_FILE_NOT_FOUND;
	};
#endif

	/* Write data */
	write_cnt = fwrite(data, sizeof(uint8_t), len, f_fw);
	if(write_cnt != len){
		//fclose(f_fw);
		error("Error: vFWrite \r\n");
		return FW_WRITE_ERROR;
	}


	/* Close open Firmware */
	//fclose(f_fw);

	return FW_WRITE_OK;
}

/* FW Write finish */
FWUpgrade_Error_t vFWWriteFinish(void){
uint8_t res;

	// Close file after write
	res = fclose(f_fw);
	if(res != 0)
		error("Error: close file\r\n");

	return res;
}

/* FW Upgrade API ----------------------------------------- */

/**
 * @brief	Firmware upgrade function, decode IntelHex and copy to Flash memory
 * @param 	None
 * @return	Status of firmware upgrade
 * @todo	Get Firmware size to erase properly pages
 */
FWUpgrade_Error_t FWUpgrade(void){

/* Write Firmware Update status */
	BKP_WriteBackupRegister(BKP_FW_STATUS, NEW_FW_AVAILABLE_FLAG);

// @todo Reboot system
	tsk_lock();
	vIWDG_Init();

	return 0;
}

/**
 *		Verify downloaded data
 */
FWUpgrade_Error_t FWVerify(uint32_t fwCRCValue){
char line[46];
uint32_t CRCValue = 0;

/* Verify FW Data */
	f_fw = fopen(FW_Filename, "r");
	if(f_fw == NULL) return FW_FILE_NOT_FOUND;

	// Calculate CRC of each line
	CRC_ResetDR();

	memset(line, 0, sizeof(line));
	while( fgets(line, sizeof(line), f_fw) != NULL ){
		CRCValue = CRC_CalcBlockCRC((uint32_t *)line, 1 + strlen(line)/4);
		memset(line, 0, sizeof(line));
	};

	// Close file
	fclose(f_fw);

	// Compare with given CRC
	if(CRCValue != fwCRCValue)
		return FW_VERIFY_FAIL;

	return FW_VERIFY_OK;
}
