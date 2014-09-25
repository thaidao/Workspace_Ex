#ifndef _FW_UPGRADE_MODULE_
#define _FW_UPGRADE_MODULE_

/* Global macros ---------------------------- */
#define ApplicationAddress			0x08020000

/* Type ------------------------------------- */

typedef enum{
	FW_FILE_NOT_FOUND,					// FW File is not found
	FW_WRONG_CHECKSUM,					// Wrong checksum of eachline
	FW_NO_STARTCODE,					// No start code ':' found
	FW_FLASH_ERROR,						// Error at programming data to Flash memory
	FW_UPGRADE_OK,
	FW_UPGRADE_FAIL
} FWUpgrade_Error_t;

/* Firmware Upgrade API ------------------------- */

/* Perform FWUpgrade after download all firmware data */
FWUpgrade_Error_t FWUpgrade(uint32_t FWSize);

#endif
