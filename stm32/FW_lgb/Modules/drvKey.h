/*
 * drvKey.h
 *
 *  Created on: Feb 22, 2012
 *      Author: Van Ha
 */

#ifndef DRVKEY_H_
#define DRVKEY_H_

/* Driver macros -------------------------------------- */
#define DRIVER_FIELD_LENGTH 8
#define DRIVER_LOAD_OK		1
#define DRIVER_LOAD_FAIL	0

/* Read/Write driver info */
#define ReadDriverInfo(buf)	blReadInfo((buf), DRIVER_FIELD_LENGTH)

//**** hardware *******************************/
void vDriverInit(void);
void vEprInit(void);
void vDrvInitI2CBus(void);

// api
BOOL blVerifyDrvKey(void);
BOOL blReadInfo(S8 *info, U16 size);
BOOL blWriteInfo(S8 *info, U16 size);

#endif /* DRVKEY_H_ */
