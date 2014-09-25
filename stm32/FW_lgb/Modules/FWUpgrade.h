#ifndef _FW_UPGRADE_MODULE_
#define _FW_UPGRADE_MODULE_

/* Global macros ---------------------------- */

typedef enum{
	FW_FILE_NOT_FOUND,					// FW File is not found
	FW_WRONG_CHECKSUM,					// Wrong checksum of eachline
	FW_NO_STARTCODE,					// No start code ':' found
	FW_FLASH_ERROR,						// Error at programming data to Flash memory
	FW_UPGRADE_OK,
	FW_VERIFY_OK,
	FW_VERIFY_FAIL,

	FW_WRITE_ERROR,
	FW_WRITE_OK,

} FWUpgrade_Error_t;

/* Firmware Upgrade API ------------------------- */

/* Init Firmware with prefered FW size */
int8_t vFWInit(uint32_t fw_size);

/* Write firmware data into SDCard */
FWUpgrade_Error_t vFWWrite(uint8_t* data, uint16_t len);
FWUpgrade_Error_t vFWPrChar(uint8_t chr);

/* Perform FWUpgrade after download all firmware data */
FWUpgrade_Error_t FWUpgrade(void);
FWUpgrade_Error_t FWVerify(uint32_t crcValue);
FWUpgrade_Error_t vFWWriteFinish(void);

#endif
