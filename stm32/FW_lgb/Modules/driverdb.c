#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "driverdb.h"

/* Private variables --------------------- */
const uint8_t DriverDB_Name[] = "Drivers.db";			// List of driver
FILE* f_db;

/* Functions ----------------------------------- */

/**
 * @brief	Check driver with given ID in the database
 * @param	ID		ID of given driver
 * @param	info	Pointer to Driver info
 */
eDriverDBStatus eDB_GetDriverInfo(uint8_t* ID, driverdb_t* pInfo){
uint32_t res;

	// Open driver DB
	f_db = fopen(DriverDB_Name, "rb");
	if(f_db == NULL)
		return DRV_DB_FILE_ERROR;

	// Read block by block
	while(!feof(f_db)){
		res = fread(pInfo, sizeof(uint8_t), sizeof(driverdb_t), f_db);

		// Compare ID
		if (memcmp(ID, pInfo->ID, sizeof(pInfo->ID)) == 0){
			fclose(f_db);
			return DRV_DB_OK;
		};
	};

	// Close driver DB
	fclose(f_db);
	return DRV_DB_NOT_FOUND;
}


eDriverDBStatus eDB_WriteNewInfo(driverdb_t* pInfo){
uint32_t res;
uint8_t r_data[12];

	// Open driver DB
	f_db = fopen(DriverDB_Name, "ab+");
	if(f_db == NULL)
		return DRV_DB_FILE_ERROR;

	// Write info
	res = fwrite(pInfo, sizeof(uint8_t), sizeof(driverdb_t), f_db);
	if(res != sizeof(driverdb_t)){
		// Close DB
		fclose(f_db);
		return DRV_DB_FILE_ERROR;
	}

	// Move to the last write location
	fseek(f_db, -sizeof(driverdb_t), SEEK_END);

	// Read data
	res = fread(r_data, sizeof(uint8_t), 8, f_db);

	// Close DB
	fclose(f_db);

	if(strncmp(r_data, pInfo->ID, 8) != 0){
		return DRV_DB_FILE_ERROR;
	}

	return DRV_DB_OK;

}

eDriverDBStatus eDB_Create(void){

	// Open driver DB
	f_db = fopen(DriverDB_Name, "wb");
	if(f_db == NULL)
		return DRV_DB_FILE_ERROR;

	// Close DB
	fclose(f_db);

	return DRV_DB_OK;

}

/**
 * @brief	Create Database for testing only
 */
void vDB_CreateDriverDB(void){
driverdb_t info_temp;

	// First ID
	memcpy(info_temp.ID, "00000001", sizeof(info_temp.ID));
	memcpy(info_temp.lic_number, "A123456789", sizeof(info_temp.lic_number));
	memcpy(info_temp.name,"Nguyen Van A", sizeof(info_temp.name));
	memcpy(info_temp.issueDate,"20/02/2000", sizeof(info_temp.issueDate));
	memcpy(info_temp.expiryDate, "20/02/2001", sizeof(info_temp.expiryDate));


	eDB_WriteNewInfo(&info_temp);

	// Second ID
	memcpy(info_temp.ID, "00000002", sizeof(info_temp.ID));
	memcpy(info_temp.lic_number, "B123456789", sizeof(info_temp.lic_number));
	memcpy(info_temp.name,"Nguyen Van B", sizeof(info_temp.name));
	memcpy(info_temp.issueDate,"12/02/2010", sizeof(info_temp.issueDate));
	memcpy(info_temp.expiryDate, "12/02/2015", sizeof(info_temp.expiryDate));

	eDB_WriteNewInfo(&info_temp);

	// Third ID
	memcpy(info_temp.ID, "00000003", sizeof(info_temp.ID));
	memcpy(info_temp.lic_number, "C123456789", sizeof(info_temp.lic_number));
	memcpy(info_temp.name,"Nguyen Van C", sizeof(info_temp.name));
	memcpy(info_temp.issueDate,"12/03/2010", sizeof(info_temp.issueDate));
	memcpy(info_temp.expiryDate, "12/03/2015", sizeof(info_temp.expiryDate));

	eDB_WriteNewInfo(&info_temp);

	// Forth ID
	memcpy(info_temp.ID, "00000004", sizeof(info_temp.ID));
	memcpy(info_temp.lic_number, "D123456789", sizeof(info_temp.lic_number));
	memcpy(info_temp.name,"Nguyen Van D", sizeof(info_temp.name));
	memcpy(info_temp.issueDate,"12/04/2010", sizeof(info_temp.issueDate));
	memcpy(info_temp.expiryDate, "12/04/2015", sizeof(info_temp.expiryDate));

	eDB_WriteNewInfo(&info_temp);
}

/**
 *
 */
eDriverDBStatus vDB_Test(void){
driverdb_t info;
eDriverDBStatus res;

	res = eDB_GetDriverInfo("00000001", &info);


	return DRV_DB_OK;
}
