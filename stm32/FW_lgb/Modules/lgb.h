/*
 * lgb.h
 *
 *  Created on: Nov 21, 2012
 *      Author: ThaiDN
 */

#ifndef LGB_H_
#define LGB_H_

#define WDT_LGB_MAX_TIMING 	100

typedef struct
{
	U8 Type[3];
	U8 Speed;
	U8 OutDate[2];
	U8 OutTime[2];
	U8 CarStatus;
	U8 EmptyKmBeforeTrip[2];
	U8 InDate[2];
	U8 InTime[2];
	U8 KmCustomerOnCar[3];
	U8 StopTimeInTrip[2];
	U8 FareOfTrip[3];
	U8 CheckSum;
}LGB_GPS_Frame_TypeDef;

typedef struct
{
	U8 Type[3];					   	//Nhan dang khung
	U8 TotalDistance[4];	   		//Tong km da chay
	U8 TotalDistanceInservice[4];  	//Tong km co khach
	U8 HackGear[2];					//So lan luot van
	U8 TotalFare[4];				//Tong so tien
	U8 TotalTrip[3];   				//Tong so cuoc khach
	U8 HackPulse[2];				//Loi rut xung
	U8 OverSpeed[2]; 				//Loi vuot qua toc do
	U8 LostVcc[2];					//Tong so lan mat nguon
	U8 WorkShiftIndex[2];		   	//So thu tu ca lam viec
	U8 C1[3];
	U8 C2[3];
	U8 C3;
	U8 C4[3];
	U8 C5;
	U8 C6;
	U8 C7;
	U8 C9;
	U8 CheckSum;
}LGB_LGT_Frame_TypeDef;

typedef enum
{
	TM_RES_OK,
	NO_FILE = -1,
	FILE_EMPTY = -2,
	SIZE_PACKET_ERR = -3,
	SDCARD_ERR = -4,
	OVER_FILE = -5,
	CHECKSUM_FAIL = -6,
	TM_DISCONNECT = -7
}Tm_Error_e;

extern BOOL TmDebugEnableFlag;

void vLGB_UART_Init(void);
uint8_t WDT_Check_LGB_Service(void);
void TM_ImediatelyUpdate(void);

U32 TM_GetReadBufferIndex(void);
U32 TM_GetWriteBufferIndex(void);
void LGB_SDBufferReset(void);


#endif /* LGB_H_ */
