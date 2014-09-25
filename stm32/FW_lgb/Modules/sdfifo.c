/**
 * @file	SDFifo.c
 * @brief	Implementation of FIFO based on SDCard
 */

/* Include files */
#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "sdfifo.h"
#include "rtc.h"
#include "utils.h"
#include "modules.h"

/* Private macros -------------------- */
#define	SDFIFO_RECORD_PER_BANK		4095	// 4095
#define	SDFIFO_BANK_COUNT			15

#define SDFIFO_PUSH_READ_LOCATION()			BKP_WriteBackupRegister(BKP_SDFIFO_READ, sdfifo_r_addr.all);
#define SDFIFO_PUSH_WRITE_LOCATION()		BKP_WriteBackupRegister(BKP_SDFIFO_WRITE, sdfifo_w_addr.all);

/* Private types */
union sdfifo_location{
	struct{
		uint16_t bank:4;				// Idx: '0' -> '9', 'a' -> 'f'
		uint16_t count:12;
	} bit;

	uint16_t all;
} sdfifo_w_addr, sdfifo_r_addr;

/* Private variables ------------------ */
FILE* f_sdfifo;
extern void error(uint8_t* str);
OS_MUT sdfifo_mutex;

/* SDFIFO Buffer to read */
uint8_t sdfifo_buf[SDFIFO_MSG_LEN];

#ifdef SDFIFO_DEBUG_ENABLE
uint8_t buffer[20];
#endif

#define FS_MUT_INIT()			os_mut_init(&sdfifo_mutex)
#define FS_MUT_LOCK()			os_mut_wait(&sdfifo_mutex, 0xffff)
#define FS_MUT_RELEASE()		os_mut_release(&sdfifo_mutex)

char sdfifo_read_filename[] =  "sdfifo\\bufxx.log";	// buf
char sdfifo_write_filename[] = "sdfifo\\bufxx.log";
#define	SDFIFO_NUM_LOCATION		10

/* Private fucntion ------------------------- */
static void updateReadFileName(void){
uint16_t bank = sdfifo_r_addr.bit.bank;

	sdfifo_read_filename[SDFIFO_NUM_LOCATION] = bank/10 + '0';
	sdfifo_read_filename[SDFIFO_NUM_LOCATION+1] = bank%10 + '0';

	BKP_WriteBackupRegister(BKP_SDFIFO_READ, sdfifo_r_addr.all);
}

static void updateWriteFileName(void){
uint16_t bank = sdfifo_w_addr.bit.bank;

	sdfifo_write_filename[SDFIFO_NUM_LOCATION] = bank/10 + '0';
	sdfifo_write_filename[SDFIFO_NUM_LOCATION+1] = bank%10 + '0';

	BKP_WriteBackupRegister(BKP_SDFIFO_WRITE, sdfifo_w_addr.all);
}

/* Global functions --------------------------- */

/*
 * \brief	sdfifo_getItem()
 * 			this function for detect number in buffer is large.
 * 			purpose for increase performance server.
 * \return	0 - not clog, other else.
 */
uint32_t sdfifo_getItem(void)
{
	uint32_t in, out;

	if(sdfifo_w_addr.bit.bank == sdfifo_r_addr.bit.bank)
		return (sdfifo_w_addr.bit.count - sdfifo_r_addr.bit.count);
	// number writer.
	in = (sdfifo_w_addr.bit.bank * SDFIFO_RECORD_PER_BANK) + sdfifo_w_addr.bit.count;
	// number reader
	out = (sdfifo_r_addr.bit.bank * SDFIFO_RECORD_PER_BANK) + sdfifo_r_addr.bit.count;
	// over buffer.
	if(in < out)
		in += SDFIFO_RECORD_PER_BANK * (SDFIFO_BANK_COUNT + 1);

	return (in - out);
}

uint16_t sdfifo_getInItem(void)
{
	return sdfifo_w_addr.all;
}

uint16_t sdfifo_getOutItem(void)
{
	return sdfifo_r_addr.all;
}

/* reset all index */
void sdfifo_reset(void){
	sdfifo_r_addr.all = 0;
	sdfifo_w_addr.all = 0;

	updateReadFileName();
	updateWriteFileName();
}

/**
 * @brief	Init SDFifo by reading data from backup registers
 * @param	none
 * @return	none
 */
void sdfifo_init(void){
uint16_t temp;
uint16_t old_buffer_status;

//uint8_t buffer[12];
//
//sprintf(buffer, "\r\nB:%X,%X\x0", sdfifo_w_addr.all, sdfifo_r_addr.all);
//USART_PrCStr(USART1, buffer);

/* Init SDFIFO */
	/* Read read location */
	temp = BKP_ReadBackupRegister(BKP_SDFIFO_READ);
	//if(temp != 0x00)			// don't understand this line.
		sdfifo_r_addr.all = temp;

	/* Read write location */
	temp = BKP_ReadBackupRegister(BKP_SDFIFO_WRITE);
	//if(temp != 0x00)			// don't understand this line.
		sdfifo_w_addr.all = temp;

	/* Update filename */
	updateReadFileName();
	updateWriteFileName();

	FS_MUT_INIT();
	FS_MUT_RELEASE();
}

/**
 * @brief 	Write data to buffer
 */
int8_t sdfifo_write(uint8_t* data){
uint32_t write_cnt;
uint32_t res;
uint8_t error_cnt = 0;

/* Debug */
#ifdef SDFIFO_DEBUG_ENABLE
		// USART_PrCStr
		sprintf(buffer, "\r\nW:%d,%d\r\n", sdfifo_w_addr.bit.bank, sdfifo_w_addr.bit.count);
		USART_PrCStr(USART1, buffer);
#endif

	// Mutex if at the same bank
	FS_MUT_LOCK();

/* Write data to buffer */
	// Open buffer to write
	if(sdfifo_w_addr.bit.count == 0)
		f_sdfifo = fopen(sdfifo_write_filename,"wb");
	else
		f_sdfifo = fopen(sdfifo_write_filename,"ab");
	
	if(f_sdfifo == NULL) {
		error("E: Open buffer");
		FS_MUT_RELEASE();
		return -1;
	};

	// Write data to buffer
	write_cnt = fwrite(data, sizeof(uint8_t), SDFIFO_MSG_LEN , f_sdfifo);
	if(write_cnt != SDFIFO_MSG_LEN)
	{
		dbg_print("E: Write to buffer\r\n");
		fclose(f_sdfifo);
		FS_MUT_RELEASE();
		return -2;
	}

	// Close file
	fclose(f_sdfifo);

	f_sdfifo = fopen(sdfifo_write_filename,"rb");

	if(f_sdfifo == NULL) {
		error("E: Open buffer");
		FS_MUT_RELEASE();
		return -1;
	};

	if(fseek(f_sdfifo, sdfifo_w_addr.bit.count * SDFIFO_MSG_LEN, SEEK_SET) == EOF){
		fclose(f_sdfifo);
		FS_MUT_RELEASE();
		return -3;
	}

	memset(sdfifo_buf, 0, SDFIFO_MSG_LEN);
	// Read data and check with write location at same bank
	res = fread(sdfifo_buf, sizeof(uint8_t), SDFIFO_MSG_LEN, f_sdfifo);

	// Close the file
	fclose(f_sdfifo);
	FS_MUT_RELEASE();

	if(memcmp(data, sdfifo_buf, SDFIFO_MSG_LEN) != 0){
		dbg_print("E: Verify fail\r\n");

		/* Reset index incase of fail seeking */
		sdfifo_reset();
		return -5;
	}

/* Update file location */
	// Update write location
	if(++sdfifo_w_addr.bit.count >= SDFIFO_RECORD_PER_BANK){
		sdfifo_w_addr.bit.count = 0;
		if(++sdfifo_w_addr.bit.bank >= SDFIFO_BANK_COUNT)
			sdfifo_w_addr.bit.bank = 0;

		// Update filename
		updateWriteFileName();

		// CHANGEBANK: Reset read location if overwrite same bank
		if(sdfifo_r_addr.bit.bank == sdfifo_w_addr.bit.bank){
			sdfifo_r_addr.bit.count = 0;

			/* Increase Read bank */
			if(++sdfifo_r_addr.bit.bank >= SDFIFO_BANK_COUNT){
				sdfifo_r_addr.bit.bank = 0;
			}

			updateReadFileName();
		};
	};

	/* Push write location */
	SDFIFO_PUSH_WRITE_LOCATION();

	return 0;
}


/**
 * @brief	Read data from buffer at sdfifo_r_addr
 * @param	buf		Storage buffer for data
 * @param	len		size of data to read
 * @return	0		success
 * 			-1		error read location, which hasn't been written
 *
 */
int8_t sdfifo_read(uint8_t* buf){
uint32_t res;

// If read/write at same location, then generate error
	if(sdfifo_r_addr.all == sdfifo_w_addr.all) return -1;

	// Debug
#ifdef SDFIFO_DEBUG_ENABLE
	sprintf(buffer, "\r\nR:%d,%d\r\n", sdfifo_r_addr.bit.bank, sdfifo_r_addr.bit.count);
	USART_PrCStr(USART1, buffer);
#endif

	FS_MUT_LOCK();

// Open buffer at read mode
	f_sdfifo = fopen(sdfifo_read_filename, "rb");

	if(f_sdfifo == NULL){
		error("E: Open buffer\r\n");
		FS_MUT_RELEASE();
		return -2;
	};

// Move pointer to read location
	if(fseek(f_sdfifo, sdfifo_r_addr.bit.count * SDFIFO_MSG_LEN, SEEK_SET) == EOF){
		// Close the file
		fclose(f_sdfifo);

		/* Reset index incase of fail seeking */
		sdfifo_reset();

		FS_MUT_RELEASE();
		return -3;
	}

// Read data and check with write location at same bank
	res = fread(buf, sizeof(uint8_t), SDFIFO_MSG_LEN, f_sdfifo);

	// Close the file
	fclose(f_sdfifo);
	FS_MUT_RELEASE();

	if(res != SDFIFO_MSG_LEN){
		dbg_print("E: R Wrong read len");
		
		/* Reset index incase of fail seeking */
		sdfifo_reset();
		
		return -4;
	};

	return 0;
}

/**
 * @brief	Move read buffer to the next location
 * @todo	check read/write location
 *
 */
int8_t sdfifo_readNext(void){

// if read/write in the same bank, move to near write location
	if(sdfifo_r_addr.bit.bank == sdfifo_w_addr.bit.bank){
		if(sdfifo_r_addr.bit.count < sdfifo_w_addr.bit.count)
			sdfifo_r_addr.bit.count++;
		else
			return -1;
	}

	// if read/write at diffirent bank
	else if(++sdfifo_r_addr.bit.count >= SDFIFO_RECORD_PER_BANK){
		sdfifo_r_addr.bit.count = 0;
		if(++sdfifo_r_addr.bit.bank >= SDFIFO_BANK_COUNT){
			sdfifo_r_addr.bit.bank = 0;
		}

		// Update filename
		updateReadFileName();
	}

	// Push read addr
	SDFIFO_PUSH_READ_LOCATION();

	return 0;
}


#ifdef SDFIFO_TEST_ENABLE
/* Test function ----------------------------------------  */
__task void sdfifo_tsk_read(void);
__task void sdfifo_tsk_write(void);
Data_frame_t data_r;
Data_frame_t data_w;
static U64 tsk_write_stack[500/8];
static U64 tsk_read_stack[500/8];

OS_TID sdfifo_w_tid;
OS_TID sdfifo_r_tid;

/* Test function */
void sdfifo_Test(void){
	sdfifo_init();

	/* init data */
	memset((uint8_t*)&data_w, 0x1A, sizeof(Data_frame_t));

	/* create task */
	sdfifo_r_tid = os_tsk_create_user(sdfifo_tsk_read, 0x01, tsk_read_stack, sizeof(tsk_read_stack));
	sdfifo_w_tid = os_tsk_create_user(sdfifo_tsk_write, 0x01, tsk_write_stack, sizeof(tsk_write_stack));
}

/* Sdfifo read task */
__task void sdfifo_tsk_read(void){

int8_t res;

	for(;;){
		res = sdfifo_read((uint8_t*)&data_r);
		if(res != 0){
			USART_PrCStr(USART1, "+");
		}

		/* Move to the next */
		sdfifo_readNext();
		os_dly_wait(10);
	}
}
/* Sdfifo write task: write data to sdfifo */
__task void sdfifo_tsk_write(void){
uint32_t cnt = 0;
uint16_t idx;
	for(;;){
		idx = 100;
		while(idx--){
			/* Write data */
			sdfifo_write((uint8_t*)&data_w);
			cnt++;
		};
		
		if(cnt%100 == 0)
			USART_PrCStr(USART1, "*");

		os_dly_wait(1);
	}
}

void sdfifo_kill_read(void){
	os_tsk_delete(sdfifo_r_tid);
}

void sdfifo_kill_write(void){
	os_tsk_delete(sdfifo_w_tid);
}

#endif
