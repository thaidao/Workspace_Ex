#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "rtl.h"
#include "stm32f10x.h"
#include "platform_config.h"

#include "utils.h"
#include "serial.h"
#include "logger.h"
#include "loadconfig.h"

#include "sim900.h"
#include "gsm.h"
#include "http.h"
#include "debug.h"

#include "buzzer.h"
#include "voice.h"
#include "driverdb.h"
#include "Systemmonitor.h"
#include "gpsModule.h"
#include "rtc.h"
#include "periodicService.h"
#include "loadconfig.h"

#include "io_manager.h"

#define	ENGINE_IO_BIT		0x01	// i0
#define	DOOR_IO_BIT			0x02	// i1
#define	AC_IO_BIT				0x04	// i2
#define BIT_USED				ENGINE_IO_BIT | DOOR_IO_BIT


#define IO_LOCATION_BANK_MAX			15
#define	IO_LOCATION_COUNT_MAX			4095

typedef union _io_location_t{
	struct{
		uint16_t bank:4;				// Idx: '0' -> '15'
		uint16_t count:12;
	} bit;

	uint16_t all;
}io_location_t;

uint8_t _previousIO = 0, _currentIO = 0;
io_location_t _IOAddrRead, _IOAddrWrite;

OS_MUT IOMesgMutex;

#define IO_MUT_INIT()			os_mut_init(&IOMesgMutex)
#define IO_MUT_LOCK()			os_mut_wait(&IOMesgMutex, 0xffff)
#define IO_MUT_RELEASE()		os_mut_release(&IOMesgMutex)


/*
 * \brief		uint8_t io_read_on_port(void)
 *					read state on GPIO port
 * \param		none.
 * \return	state value on port.
 */
uint8_t io_read_on_port(void)
{
	CarStatus_t io;
	uint8_t *ret;
	
	io.A_CStatus = GPIO_ReadInputDataBit(A_C_GPIO_PORT, A_C_GPIO_PIN);
	io.DoorStatus = GPIO_ReadInputDataBit(DOOR_GPIO_PORT, DOOR_GPIO_PIN);
	io.EngineStatus = GPIO_ReadInputDataBit(ENGINE_GPIO_PORT, ENGINE_GPIO_PIN);

	io.IO_IN3 = GPIO_ReadInputDataBit(IO_PORT, IO_IN3_PIN);
	io.IO_IN4 = GPIO_ReadInputDataBit(IO_PORT, IO_IN4_PIN);
	io.IO_IN5 = GPIO_ReadInputDataBit(IO_PORT, IO_IN5_PIN);
	
	ret = (uint8_t*)&io;
	
	return *ret;
}

/*
 * \brief		int8_t io_get_bit(uint8_t io, uint8_t bit)
 *					read state on PIN of GPIO port
 * \param		io as data on GPIO port.
 * \param		bit as pin of port.
 * \return	0 or 1 as state pin of port.
 */
int8_t io_get_bit(uint8_t io, uint8_t bit)
{
	int8_t flag;
	
	flag = (io & bit) ? 1 : 0;
	return flag;
}

/*
 * \brief		int8_t io_any_changed(uint8_t io)
 *					check state GPIO port is changed.
 * \param		io as data on GPIO port.
 * \return	1 - changed. other else.
 */
int8_t io_any_changed(uint8_t io)
{
	int8_t changed = 0;
	
	if(io_bit_changed(io, BIT_USED))		// only detect on two pin 0 & 1 (engine & door).
		changed = 1;

	return changed;
}

/*
 * \brief		int8_t io_any_changed(uint8_t io)
 *					check PIN's state is changed.
 * \param		io as data on GPIO port.
 * \param		bit as PIN.
 * \return	1 - changed. other else.
 */
int8_t io_bit_changed(uint8_t io, uint8_t bit)
{
	if((io & bit) != (_previousIO & bit))
		return 1;
	
	return 0;
}

/*
 * \brief		void io_manager(io_message_t *ioMessage)
 *					get data on PORT, check PIN changed, calculate, report data...
 * \param		ioMessage as message report.
 * \return	none.
 */
void io_manager(io_message_t *ioMessage)
{	
	// reset value if new day.
	if(RTC_GetNewdayFlag(IO_REPORT_FLAG)){
		io_report_clear(ioMessage);
		// clear flag.
		RTC_ReSetNewdayFlag(IO_REPORT_FLAG);
	}
	// old io.
	_previousIO = _currentIO;
	
	// read current io.
	_currentIO = io_read_on_port();
	
	ioMessage->io_state = _currentIO;
	
	if(io_bit_changed(_currentIO, ENGINE_IO_BIT) && io_get_bit(_currentIO, ENGINE_IO_BIT)){
		ioMessage->i0_count++;
		// store in backup ram.
		BKP_WriteBackupRegister(BKP_I0_COUNTER, ioMessage->i0_count);
	}
	if(io_bit_changed(_currentIO, DOOR_IO_BIT) && io_get_bit(_currentIO, DOOR_IO_BIT)){
		ioMessage->i1_count++;
		// store in backup ram.
		BKP_WriteBackupRegister(BKP_I1_COUNTER, ioMessage->i1_count);
	}
	if(io_bit_changed(_currentIO, AC_IO_BIT) && io_get_bit(_currentIO, AC_IO_BIT)){
		ioMessage->i2_count++;
		// store in backup ram.
		BKP_WriteBackupRegister(BKP_I2_COUNTER, ioMessage->i2_count);
	}	
	
	// any bit in io is changed.
	if(io_any_changed(_currentIO)){
		// copy current time
		vUpdateTime((uGPSDateTime_t*)&ioMessage->time);
		// copy location.
		GPS_get_xy(&ioMessage->x, &ioMessage->y);
		// push message to buffer on sdcard.
		io_save_to_sdcard(ioMessage, sizeof(io_message_t));
	}
}

/*
 * \brief		void io_manager_init(io_message_t *ioMessage)
 *					init data
 * \param		ioMessage as message report.
 * \return	none.
 */
void io_manager_init(io_message_t *ioMessage)
{
	/* Read read location */
	_IOAddrRead.all = BKP_ReadBackupRegister(BKP_IO_REPORT_READ);

	/* Read write location */
	_IOAddrWrite.all = BKP_ReadBackupRegister(BKP_IO_REPORT_WRITE);
	
	// load from backup ram.
	memset((char*)ioMessage, 0, sizeof(io_message_t));
	// read in backup ram.
	ioMessage->i0_count = BKP_ReadBackupRegister(BKP_I0_COUNTER);
	ioMessage->i1_count = BKP_ReadBackupRegister(BKP_I1_COUNTER);
	ioMessage->i2_count = BKP_ReadBackupRegister(BKP_I2_COUNTER);
	
	// init mutex to lock file buffer.
	IO_MUT_INIT();
}

void io_report_clear(io_message_t *ioMessage)
{
	// read in backup ram.
	ioMessage->i0_count = 0;
	// store in backup ram.
	BKP_WriteBackupRegister(BKP_I0_COUNTER, 0);
	ioMessage->i1_count = 0;
	// store in backup ram.
	BKP_WriteBackupRegister(BKP_I1_COUNTER, 0);
	ioMessage->i2_count = 0;
	// store in backup ram.
	BKP_WriteBackupRegister(BKP_I2_COUNTER, 0);
}

/*
 * \brief		int8_t io_save_to_sdcard(io_message_t *msg, int16_t size)
 *					save data to sdcard.
 * \param		msg as message report.
 * \param		size as sizeof message.
 * \return	none.
 */
int8_t io_save_to_sdcard(io_message_t *msg, int16_t size)
{
	int8_t filename[] = "IO\\IOX.LOG";
	FILE *fp;
	// change file name.
	filename[5] = _IOAddrWrite.bit.bank + 'A';
	// LOCK FILE.
	IO_MUT_LOCK();
	// open buffer.
	if(_IOAddrWrite.bit.count == 0)
		fp = fopen(filename,"wb");
	else
		fp = fopen(filename,"ab");
	if(fp == NULL){
		IO_MUT_RELEASE();
		return -1;
	}
	// calculate checksum message.
	msg->crc8 = cCheckSumFixSize((int8_t*)msg, size-1);
	// write to sd card.
	fwrite((int8_t*)msg, size, sizeof(char), fp);
	
	fclose(fp);
	// validate write process.
	fp = fopen(filename,"rb");
	if(fp == NULL) {
		IO_MUT_RELEASE();
		return -1;
	}
	else{	// open file success.
		int8_t rbuf[sizeof(io_message_t) + 2];	// buffer for read data.
		uint32_t res;	// result code.
		
		if(fseek(fp, _IOAddrWrite.bit.count * size, SEEK_SET) == EOF){
			fclose(fp);
			IO_MUT_RELEASE();
			return -3;
		}
		// Read data and check with write location at same bank
		res = fread(rbuf, sizeof(uint8_t), size, fp);
		fclose(fp);
		if(memcmp((int8_t*)msg, rbuf, size) != 0){
			dbg_print("I: Verify fail\r\n");
			io_wipe_in_sdcard();
			IO_MUT_RELEASE();
			return -5;
		}
	}
	// Update write location
	if(++_IOAddrWrite.bit.count >= IO_LOCATION_COUNT_MAX){
		_IOAddrWrite.bit.count = 0;
		if(++_IOAddrWrite.bit.bank >= IO_LOCATION_BANK_MAX)
			_IOAddrWrite.bit.bank = 0;

		BKP_WriteBackupRegister(BKP_IO_REPORT_WRITE, _IOAddrWrite.all);

		// CHANGEBANK: Reset read location if overwrite same bank
		if(_IOAddrRead.bit.bank == _IOAddrWrite.bit.bank){
			_IOAddrRead.bit.count = 0;

			/* Increase Read bank */
			if(++_IOAddrRead.bit.bank >= IO_LOCATION_BANK_MAX){
				_IOAddrRead.bit.bank = 0;
			}
			BKP_WriteBackupRegister(BKP_IO_REPORT_READ, _IOAddrRead.all);
		};
	};

	/* Push write location */
	BKP_WriteBackupRegister(BKP_IO_REPORT_WRITE, _IOAddrWrite.all);
	// release resource.
	IO_MUT_RELEASE();
	
	return 0;
}

/*
 * \brief		int8_t io_read_from_sdcard(uint8_t* buf, int16_t *size)
 *					read data to sdcard.
 * \param		msg as message report.
 * \param		size as sizeof message.
 * \return	none.
 */
int8_t io_read_from_sdcard(uint8_t* buf, int16_t *size)
{
static uint32_t res;
	FILE *fp;
	int8_t filename[] = "IO\\IOX.LOG";
	int8_t ret = 0;
	
	// If read/write at same location, then generate error
	if(_IOAddrRead.all == _IOAddrWrite.all) return -1;

	// mutex if needed. LOCK FILE.
	IO_MUT_LOCK();
	// change file name.
	filename[5] = _IOAddrRead.bit.bank + 'A';
	// Open buffer at read mode
	fp = fopen(filename, "rb");

	if(fp == NULL){
		IO_MUT_RELEASE();
		return -2;
	};

	// Move pointer to read location
	if(fseek(fp, _IOAddrRead.bit.count * sizeof(io_message_t), SEEK_SET) == EOF){
		ret = -3;
		goto io_read_exit;
	}

// Read data and check with write location at same bank
	res = fread(buf, sizeof(uint8_t), sizeof(io_message_t), fp);
	if(size != NULL) *size = res;

	if(res != sizeof(io_message_t)){		
		/* Reset index incase of fail seeking */	
		ret = -4;
	};
io_read_exit:
	fclose(fp);
	// release mutex.
	IO_MUT_RELEASE();
	return ret;
}

/*
 * \brief		int8_t io_release_in_sdcard(void)
 *					release data in sdcard.
 * \param		none.
 * \return	none.
 */
int8_t io_release_in_sdcard(void)
{	
// if read/write in the same bank, move to near write location
	if(_IOAddrRead.bit.bank == _IOAddrWrite.bit.bank){
		if(_IOAddrRead.bit.count < _IOAddrWrite.bit.count)
			_IOAddrRead.bit.count++;
		else
			return -1;
	}

	// if read/write at diffirent bank
	else if(++_IOAddrRead.bit.count >= IO_LOCATION_COUNT_MAX){
		_IOAddrRead.bit.count = 0;
		if(++_IOAddrRead.bit.bank >= IO_LOCATION_BANK_MAX){
			_IOAddrRead.bit.bank = 0;
		}

		// Update filename
		BKP_WriteBackupRegister(BKP_IO_REPORT_READ, _IOAddrRead.all);
	}

	// Push read addr
	BKP_WriteBackupRegister(BKP_IO_REPORT_READ, _IOAddrRead.all);

	return 0;
}

void io_wipe_in_sdcard(void)
{
	// index = 0.
	_IOAddrWrite.all = 0;
	// save to backup ram.
	BKP_WriteBackupRegister(BKP_IO_REPORT_WRITE, _IOAddrWrite.all);
	
	_IOAddrRead.all = 0;
	// save to backup ram.
	BKP_WriteBackupRegister(BKP_IO_REPORT_READ, _IOAddrRead.all);
}


