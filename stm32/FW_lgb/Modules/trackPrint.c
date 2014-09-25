/*
 * trackPrint.c
 *
 *  Created on: Feb 13, 2012
 *      Author: Van Ha
 */
#include <stdlib.h>
#include <RTL.h>                      /* RTL kernel functions & defines      */
#include <stdio.h>                    /* standard I/O .h-file                */
#include <ctype.h>                    /* character functions                 */
#include <string.h>                   /* string and memory functions         */

/* Include needed modules */
#include "serial.h"					 // Serial Communication Module
#include "logger.h"					 // Logger Module
#include "printer.h"				 // Printer module
#include "Platform_Config.h"		 // Platform configuration

#include "loadconfig.h"
#include "driverdb.h"
#include "trackPrint.h"

#include "utils.h"
#include "sim18.h"

/*** local variables **************************/


const S8 VIN[18] = "1ZVHT82H485113456";

extern OS_MUT logLock;

/*
 * error
 */
#ifdef _PRINT_DEBUG
void print_err(S8* s)
{
	dbg_print(s);
}
void print_wr(S8* s, U16 si)
{
	DBG_PrStr(s,si);
}
#else
void print_err(S8* s)
{}
void print_wr(S8* s, U16 si)
{
}
#endif


/*
 * changed baudrate for printer port
 */
void vPrinterChangeBaudrate(U32 baud)
{
	USART_InitTypeDef config;

	config.USART_BaudRate = baud;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	USART_Init(PRINTER_PORT, &config);

	USART_ITConfig(PRINTER_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(PRINTER_PORT, ENABLE);
}


void vPrintHeader(void)
{
	vPos_PrCStr("----------------------");
	vPos_PrCRLF();

	vPos_PrCStr("CTY CPPTCN EPOSI");
	vPos_PrCRLF();
	vPos_PrCStr("SO 672, QUANG TRUNG");
	vPos_PrCRLF();

	vPos_PrCStr("----------------------");
	vPos_PrCRLF();
}

/*
 * print 10 line speed.
 */
void vPrintFormat(uint16_t speed, uint16_t pulse)
{
	uint8_t string[24];

//	sprintf((char*)text, "%02u:%02u:%02u  ", log->tm.hour, log->tm.min, log->tm.sec);
//	vPos_PrCStr((uint8_t*)text);
//
//	memset(text, 0, 25);

	// Print speed
	//if (pulseFlag)
	//	sprintf((char*)text, "%03u", log->pulse);
	//else
	/* print speed gps & pulse */
	sprintf((char*)string, "%03.01f km/h", (float)(speed)/10.0);

	vPos_PrCStr(string);
	vPos_PrCRLF();
}

void vPrintReport(S8* d, U16 size, FILE *f)
{
	LogInfo_t *log;
	uint16_t hh, mm;
	S8 *text;

	if(f == NULL)
		return;

	log = (LogInfo_t*)d;
	text = (S8*)calloc(40, sizeof(S8));
	if(text == NULL)
	{
		return;
	}

	memset(text, 0, 25);
	sprintf((char*)text, "So lan vuot toc do: %02u\x0", log->over_speed);
	vPos_PrCStr(text);
	vPos_PrCRLF();

	// in 5 lan gan nhat neu co.
	if(log->over_speed > 0){
		uint8_t *item[6];
		uint8_t n;
		rewind(f);
		mm = 1;
		while(fgets(text, 40, f) != NULL){
			// "S|%u|%02u:%02u:%02u|%02.05f,%03.05f|%03u\n"
			n = string_split(item, text, "|", 6);
			if(n == 5){
				hh = atoi(item[1]);
				if((*item[0] == 'S') && ((hh + 5) > log->over_speed)){
					vPos_PrCStr("Lan "); vPos_PrChar('0' + mm); vPos_PrChar(':');
					// time.
					vPos_PrCStr(item[2]);
					vPos_PrCRLF();
					// locations
					vPos_PrCStr("    ");
					vPos_PrCStr(item[3]);
					vPos_PrCRLF();
					// speed.
					vPos_PrCStr("    TDCP/TDTT:");
					memset(text, 0, 25);
					sprintf((char*)text, "%03u/\x0", Config.alarmSpeed.alarmSpeedValue);
					vPos_PrCStr(text);
					vPos_PrCStr(item[4]);
					vPos_PrCRLF();

					mm ++;
				}
			}
		}
	}

	vPos_PrCStr("----------------------");
	vPos_PrCRLF();

	memset(text, 0, 25);
	sprintf((char*)text, "So lan mo cua xe: %02u\x0", log->c_door);
	vPos_PrCStr(text);
	vPos_PrCRLF();

	// in 5 lan gan nhat neu co.
	if(log->c_door > 0){
		uint8_t *item[6];
		uint8_t n;
		rewind(f);
		mm = 1;
		while(fgets(text, 32, f) != NULL){
			// "D|%u|%02u:%02u:%02u|%02.05f,%03.05f\n"
			n = string_split(item, text, "|", 6);
			if(n == 4){
				hh = atoi(item[1]);
				if((*item[0] == 'D') && ((hh + 5) > log->c_door)){
					vPos_PrCStr("Lan "); vPos_PrChar('0' + mm); vPos_PrChar(':');
					// time.
					vPos_PrCStr(item[2]);
					vPos_PrCRLF();
					// locations
					vPos_PrCStr("   ");
					vPos_PrCStr(item[3]);
					vPos_PrCRLF();

					mm ++;
				}
			}
		}
	}

	vPos_PrCStr("-----------------------");
	vPos_PrCRLF();

	hh = log->cont_time%3600;
	mm = hh/60;
	hh = log->cont_time/3600;
	sprintf((char*)text, "LXLT: %02u gio %02u phut\x0", hh, mm);
	vPos_PrCStr(text);
	vPos_PrCRLF();

	hh = log->total_time%3600;
	mm = hh/60;
	hh = log->total_time/3600;
	memset(text, 0, 25);
	sprintf((char*)text, "LXTN: %02u gio %02u phut", hh, mm);
	vPos_PrCStr(text);

	free(text);
}

/**
 * k co file
 */
void vPrintErrFile(void)
{
	vPos_PrCStr("Khong co du lieu");
	vPos_PrCRLF();
}

/**
 * sai ngay hoac gio
 */
void vPrintErrDateTime(void)
{
	vPos_PrCStr("Dinh dang in khong dung");
	vPos_PrCRLF();
}

/*
 * in thong tin lai xe.
 */
void vPrintDriverInfo(uint8_t *id, U16 size)
{
driverdb_t *DriverInfo;
eDriverDBStatus res;

DriverInfo = (driverdb_t*)calloc(1, sizeof(driverdb_t));

	if(DriverInfo == NULL)
	{
		return;
	}


	/* Print car info */
	vPos_PrCStr("BKS: ");
	vPos_PrCStr(Config.VehicleID);
	vPos_PrCRLF();

//	vPos_PrCStr("VIN:");
//	vPos_PrCStr(Config.VehicleVIN);
//	vPos_PrCRLF();

	/* Print driver info */

	res = eDB_GetDriverInfo(id, DriverInfo);
	//res = eDB_GetDriverInfo("00000001", DriverInfo);
	if(res == DRV_DB_OK){
		vPos_PrCStr("LX: ");
		vPos_PrCStr(DriverInfo->name);
		vPos_PrCRLF();

		vPos_PrCStr("So GPLX:");
		vPos_PrCStr(DriverInfo->lic_number);
		vPos_PrCRLF();
	}
	else{
		// in thong tin lai xe mac dinh
		vPos_PrCStr("LX:");
		vPos_PrCStr(Config.DefaultDriver.name);
		vPos_PrCRLF();

		vPos_PrCStr("So GPLX:");
		vPos_PrCStr(Config.DefaultDriver.lic_number);
		vPos_PrCRLF();
	}
	vPos_PrCStr("So se-ri TBGSHT: ");
	vPos_PrCStr(Config.BoxID);
	vPos_PrCRLF();

	vPos_PrCStr("----------------------");
	vPos_PrCRLF();

	free(DriverInfo);
}

void PrintTime(uint8_t* time)
{
	// time = "dd mm yy hh mm ss"
	uint8_t *p = time;

	vPos_PrCStr("Thoi diem in: ");
	// time.
	vPos_PrStr(p+9, 2); vPos_PrChar(':');
	vPos_PrStr(p+12, 2); vPos_PrChar(':');
	vPos_PrStr(p+15, 2);

	vPos_PrCRLF();

	// date
	vPos_PrCStr("              ");
	vPos_PrStr(p, 2); vPos_PrChar('/');
	vPos_PrStr(p+3, 2); vPos_PrChar('/');
	vPos_PrStr(p+6, 2);
	vPos_PrCRLF();

	vPos_PrCStr("----------------------");
	vPos_PrCRLF();
}

void write_temp_over_speed(LogInfo_t *log, FILE* f)
{
	uint8_t string[40];
	uint8_t n;
	if(f == NULL)
		return;

	n = sprintf(string, "S|%u|%02u:%02u:%02u|%02.05f,%03.05f|%03u\n", log->over_speed, log->tm.hour,
			log->tm.min, log->tm.sec, log->lat, log->lng, log->CurSpeed/10);
	fwrite(string, n, 1, f);
}

void write_temp_open_door(LogInfo_t *log, FILE* f)
{
	uint8_t string[40];
	uint8_t n;

	if(f == NULL)
		return;

	n = sprintf(string, "D|%u|%02u:%02u:%02u|%02.05f,%03.05f\n", log->c_door, log->tm.hour,
			log->tm.min, log->tm.sec, log->lat, log->lng);
	fwrite(string, n, 1, f);
}


/*
 * \brief
 *
 * \param	S8* pData
 * \return 	none.
 * \note	format in:
		---------------------------
		BKS: 29A-11111
		LX: Nguyen Van A
		So GPLX:
		so se-ri TBGSHT:
		---------------------------
		Thoi diem in: xx:xx:xx (gio:phut:giay)
					  xx/xx/xx (ngay/thang/nam)
		---------------------------
		So lan vuot toc do: xx
		Lan 1: xx:xx:xx (gio, phut, giay)
				   xx,yy  (toa do xe)
				  TdCP/TdTT: xxx/xxx km/h; (toc do toi da cho phep/toc do thuc te cua xe)
				   …
		Lan 5: xx:xx:xx (gio, phut, giay)
				   xx,yy  (toa do xe)
				   TdCP/TdTT: xxx/xxx km/h; (toc do toi da cho phep/toc do thuc te cua xe)
		----------------------------
		So lan mo cua xe: xx
		Lan 1: xx:xx:xx (gio, phut, giay)
				   xx,yy  (toa do xe)
				  …
		Lan 5: xx:xx:xx (gio, phut, giay)
				   xx,yy  (toa do xe)
		----------------------------
		LXLT: xx gio xx phut
		LXTN: xx gio xx phut
 *
 */
void GH12_Print_1(uint8_t* pData)
{
	// pdata = IN1: dd mm yy hh mm ss
	uint8_t dd, mm, yy, hh, min, ss;
	uint8_t *p = pData + 5;
	uint8_t fn[14];
	uint8_t lock = 0, isData = 0;
	uint16_t overSpeed, doorNumber;
	FILE* fTemp;
	FILE* fi_log;
	LogInfo_t *gllog;

	/* tat module gps */
	vSIM18_Close();
	os_dly_wait(500/OS_TICK_RATE_MS);
	/* thay doi toc do baud */
	vSIM18_UART_baud(9600);
	os_dly_wait(500/OS_TICK_RATE_MS);

	// init printer
	vPosPrinterInit();

	sscanf(p, "%02u %02u %02u %02u %02u %02u", &dd, &mm, &yy, &hh, &min, &ss);
	// validate format input.
	if((mm == 0) || (mm > 12) || (dd > 31) || (dd == 0) || (hh > 23) || (min > 59) || (ss > 59))
	{
		vPrintHeader();
		// print format wrong.
		vPrintErrDateTime();
		goto exit;
	}

	// 10s ago.
	if(ss > 9) ss -= 10;
	else if(min > 0) {min -= 1; ss = (ss + 50);}
	else if(hh > 0) {hh -= 1; min = 59; ss = ss + 50;}
	else{ss = 0;}	// in tu dau ngay.

	// check today?
	vRTCGetDate(&fn[0], &fn[1], &fn[2]);
	if((dd == fn[0]) && (mm == fn[1]) && (yy == fn[2]))
	{
		lock = 1;
		// lock log file.
		os_mut_wait(logLock, 0xFFFF);
	}

	// get file name
	sprintf(fn, "log\\%02u%02u%02u.log\x0", dd, mm, yy);
	// open file nnttnn.log
	fi_log = fopen(fn, "rb+");
	if(fi_log == NULL)
	{
		vPrintHeader();
		// in ngay gio chon in.
		PrintTime(pData + 5);
		// print none data.
		vPrintErrFile();
		goto exit;
	}

	gllog = (LogInfo_t*)calloc(1, sizeof(LogInfo_t));
	if(gllog == NULL)
	{
		vPos_PrCStr("Loi trong qua trinh in");
		vPos_PrCRLF();

		fclose(fi_log);
		goto exit;
	}

	fTemp = fopen("Temp\\in.tmp", "wb+");
	// reset temp.
	doorNumber = 0;
	overSpeed = 0;
	// browser file.
	while(!feof(fi_log))
	{
		// xac dinh vi tri thuc cua ban tin
		fread(gllog, sizeof(LogInfo_t), 1, fi_log);

		// open door
		if(doorNumber != gllog->c_door){
			write_temp_open_door(gllog, fTemp);
			// old value
			doorNumber = gllog->c_door;
		}
		// over speed
		if(overSpeed != gllog->over_speed){
			write_temp_over_speed(gllog, fTemp);
			// old value
			overSpeed = gllog->over_speed;
		}
		// check time
		if(((uint8_t)gllog->tm.hour >= hh) && ((uint8_t)gllog->tm.min >= min) && ((uint8_t)gllog->tm.sec >= ss) &&
			 ((uint8_t)gllog->tm.day >= dd) && ((uint8_t)gllog->tm.month >= mm) && ((uint8_t)gllog->tm.year >= yy)){
			isData = 1;
			break;
		}
	}

	vPrintHeader();

	// in thong tin lai xe va xe.
	vPrintDriverInfo(gllog->id_drv, sizeof(LogInfo_t));

	// in ngay gio chon in.
	PrintTime(pData + 5);

	// in2:
	if(strncmp(pData, "In2:", 4) == 0)
	{
		if(isData == 0){
			vPos_PrCStr("Khong co du lieu!");
		}
		else{
			// print 10 message.
			vPos_PrCStr("01: "); vPrintFormat(gllog->CurSpeed, gllog->pulse);
			for(hh=1;hh<10;hh++)
			{
				// neu het file in quay lai dau file.
				if(feof(fi_log))
					break;

				sprintf(fn, "%02u: \x0", hh+1);
				vPos_PrCStr(fn);
				fread(gllog, sizeof(LogInfo_t), 1, fi_log);
				vPrintFormat(gllog->CurSpeed, gllog->pulse);
			}
		}
	}
	// in1:
	else if(strncmp(pData, "In1:", 4) == 0)
	{
		vPrintReport((S8*)gllog, 12, fTemp);
	}

	// finish close file an release memory.
	fclose(fi_log);
	free(gllog);
	fclose(fTemp);

exit:
	if(lock){
		// unlock log file.
		os_mut_release(logLock);
	}

	vPos_PrCRLF();

	// finish print.
	vPosPrinterFinish();
	// wait finish
	// reset lai module gps.
	os_dly_wait(500/OS_TICK_RATE_MS);
	vSIM18_UART_baud(4800);
	os_dly_wait(500/OS_TICK_RATE_MS);
	blSIM18_Open();
}
