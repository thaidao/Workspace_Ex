/**
 * @file	DriverDB.h
 * @brief	Driver database API
 */
#ifndef _DRIVERDB_MODULE_
#define _DRIVERDB_MODULE_

/* Macros */
#define DRIVER_ID_LENGTH 	8

/* Driver Database type */
typedef struct{
	uint8_t ID[DRIVER_ID_LENGTH];
	uint8_t name[32];
	uint8_t lic_number[15];			// A123456789
	uint8_t issueDate[11];			// dd/mm/yyyy
	uint8_t expiryDate[11];			// dd/mm/yyyy
} driverdb_t;		 

/* Database return code */
typedef enum{
	DRV_DB_OK,
	DRV_DB_FILE_ERROR,
	DRV_DB_NOT_FOUND,
} eDriverDBStatus;


/* API */
eDriverDBStatus eDB_GetDriverInfo(uint8_t* ID, driverdb_t* pInfo);
eDriverDBStatus eDB_WriteNewInfo(driverdb_t* pInfo);
eDriverDBStatus eDB_Create(void);

// Testing
void vDB_CreateDriverDB(void);
eDriverDBStatus vDB_Test(void);

#endif
