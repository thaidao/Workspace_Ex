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
#include "FWUpgrade.h"
#include "serial.h"
#include "Watchdog.h"
#include "string.h"

/* Private macros ----------------------------------------- */

#define FLASH_PAGE_SIZE			 	((uint16_t)0x800)		// 2 KBytes
#define FLASH_SIZE					((uint32_t)0x40000)		// 256 KBytes

//#define FW_SIZE					0x20000					// 64 pages
#define FW_MAX_SIZE					0x20000

// Convert utility
#define HexChar2Dec(chr)			(((chr) > '9')? ((chr) - '7'):((chr) - '0'))
#define HexPairs2Dec(a,b)			HexChar2Dec(a)<<4 | HexChar2Dec(b)

/* Type */
typedef enum{
	RECORD_DATA=0x00,					// Data record
	RECORD_EOF,							// End of file record
	RECORD_EXTEND_SEGMENT_ADDR,			// Extended segment address record
	RECORD_EXTEND_LINEAR_ADDR=0x04,		// Extended linear address record
	RECORD_START_LINEAR_ADDR			// Start linear address record
} IntelRecord_t;

/* Private structure, types ------------------------------- */
typedef struct {
	uint8_t 		ByteCount;
	uint8_t 		Address[2];
	IntelRecord_t 	RecordType;
	uint8_t 		Data[16];
	uint8_t 		CheckSum;
} DecodedIntelHexLine_t;

/* Private variables -------------------------------------- */
char FW_Filename[] = "fw\\firmware.hex";				// Firmware filename
FILE* f_fw;											// Firmware file description
char line[46];										// Firmware line
DecodedIntelHexLine_t DecodedLine;					// Decoded line

uint32_t ExtendedAddress;

/* FW Upgrade API ----------------------------------------- */

/**
 * @brief	Firmware upgrade function, decode IntelHex and copy to Flash memory
 * @param 	None
 * @return	Status of firmware upgrade
 * @todo	Get Firmware size to erase properly pages
 */
FWUpgrade_Error_t FWUpgrade(uint32_t FWSize){
uint8_t idx;
uint8_t *p;
uint8_t *p_decoded;
uint8_t CheckSum;
uint8_t NumberOfBank = 0x00;
volatile FLASH_Status FLASHStatus = FLASH_COMPLETE;
uint32_t address, *pdata;

/* Erase the flash Location */
	/* Unlock Bank1 */
	FLASH_UnlockBank1();

	/* Clear all pending flags */
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

/* Open FW File at read mode */
	f_fw = fopen(FW_Filename, "r");
	if(f_fw == NULL) return FW_FILE_NOT_FOUND;

/* Erase Flash pages */
	NumberOfBank = FWSize/FLASH_PAGE_SIZE;
	for(idx=0; idx<NumberOfBank; idx++){
		FLASHStatus = FLASH_ErasePage(ApplicationAddress + idx*FLASH_PAGE_SIZE);
	};

/* Read FW Line by line */
	while( fgets(line, sizeof(line), f_fw) != NULL ){

		dbg_print(".");
																								
		/* Check StartCode */
		p = (uint8_t*) line;
		if(*p++ != ':') return FW_NO_STARTCODE;
		
		/* Convert into Hex */
		p_decoded = (uint8_t*) &DecodedLine;
		while(*p){
			*p_decoded++ = HexPairs2Dec(*p, *(p+1));
			p += 2;
		}

		if(DecodedLine.ByteCount < 0x10) DecodedLine.CheckSum = DecodedLine.Data[DecodedLine.ByteCount];

		/* Checksum data */
		p_decoded = (uint8_t*) &DecodedLine;
		CheckSum = 0;
		for(idx=0; idx< DecodedLine.ByteCount + 4; idx++)
			CheckSum += *p_decoded++;
		CheckSum = 0x01 + (~CheckSum);

		if(CheckSum != DecodedLine.CheckSum) return FW_WRONG_CHECKSUM;

		/* Process message */
		switch(DecodedLine.RecordType){
		case RECORD_DATA:
			/* Program data to Flash */
			address = ExtendedAddress + ((uint32_t) DecodedLine.Address[0] << 8) + (uint32_t) DecodedLine.Address[1];
			pdata = (uint32_t*) &DecodedLine.Data[0];

			for(idx=0; idx<4; idx++)
				FLASHStatus = FLASH_ProgramWord(address + 4*idx, *(pdata + idx));

			/* Verify flash data */
			if (memcmp(pdata, (uint32_t*) address, 16) != 0){

				FLASH_LockBank1();
				fclose(f_fw);
				return FW_FLASH_ERROR;
			}

			break;
		case RECORD_EOF:
			/* End of file record */

			FLASH_LockBank1();
			fclose(f_fw);
			return FW_UPGRADE_OK;

		case RECORD_EXTEND_LINEAR_ADDR:
			ExtendedAddress = ((uint32_t)DecodedLine.Data[0] << 24)| ((uint32_t) DecodedLine.Data[1] << 16);

			break;

		case RECORD_START_LINEAR_ADDR:
			/* Start linear address records specify the start address of the application.
			 * These records contain the full linear 32 bit address.*/
			break;
		};
	};

/* Lock  bank 1 */
	FLASH_LockBank1();
	fclose(f_fw);

	return FW_UPGRADE_FAIL;
}
