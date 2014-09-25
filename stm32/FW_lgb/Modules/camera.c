/*
 * camera.c
 *
 *  Created on: Mar 21, 2012
 *      Author: Van Ha
 */

#include <stdio.h>
#include <stdlib.h>

#include "rtl.h"
#include "stm32f10x.h"

#include "string.h"

#include "gprs.h"
#include "platform_config.h"
#include "debug.h"
#include "utils.h"
#include "loadConfig.h"
#include "serial.h"
#include "debug.h"
#include "gpsmodule.h"
#include "rtc.h"

#include "camera.h"

#define IMG_PACKET_SIZE				768
#define IMG_PKT_BUFFER_SIZE			1024
#define DATA_OFFSET					19

#define 	MAX_LOCATION_BUF		6000
#define 	FILE_INDEX_MAX			20

#define SNAPSHOT_FLAG				0x0001
#define IMG_HEADER_FINISH_FLAG 		0x0002
#define IMG_PKG_FINISH_FLAG			0x0004
#define SEND_IMG_PACKET_OK			0x0008

#define HEADER_IMAGE_FLAG_TMO		500
#define PACKET_IMAGE_FLAG_TMO		500

#define RETRY_PACKET_NUMBER  		6
#define RETRY_SNAPSHOT_NUMBER  		3

#define EMPTY				-1
#define SDCARD_ERR			-2
#define	OVER_FILE			-3
#define OK					0
#define RET_OK				0
#define NO_FILE				-1
#define SIZE_PACKET_ERR		-2
#define DATA_CHECKSUM_FAIL	-4


/* uart for camera */
#define CAM_PORT				UART4
#define CAM_RCC_Periph			RCC_APB1Periph_UART4

#define CAM_RCC_IO_PORT_TX		RCC_APB2Periph_GPIOC
#define CAM_RCC_IO_PORT_RX		RCC_APB2Periph_GPIOC

#define CAM_IO_PORT_TX			GPIOC
#define CAM_IO_PORT_RX			GPIOC

#define CAM_IRQ					UART4_IRQn

#define RS485_CTL_PIN			GPIO_Pin_0
#define RS485_CTL_PORT			GPIOD

#define CAM_IRQHandler			UART4_IRQHandler

typedef union _Cam_locate_t
{
	struct _field
	{
		U32 fileIdx:12;
		U32 location:20;
	}field;

	U32 all;
}Cam_locate_t;

typedef enum _CameraState_e
{
	WAIT_SNAPSHOT	= 0,
	SNAPSHOTTING,
	LOAD_ALL_IMAGE,
}CameraState_e;

CameraState_e CamState = WAIT_SNAPSHOT;
static CAM_State_e ISR_State = CAM_START;

BOOL blAllImageDownloaded(void);
void vCAM_GPIOConfig(void);

CAM_Ack_t CamAck;
CAM_Event_t CamEvent;
CAM_PKGData_t ImagePkg;

uint16_t CAMchecksum = 0;

/* response variables */
S8 *pCAM_Resp, CAM_RspBuffer[32];

/* event flag */
CAMERA_t *pCamera;
S8 CameraBuffer[IMG_PACKET_SIZE + 2];
S8 snapshoting;
U32 snapshot_time;
S8 tempBuff[IMG_PKT_BUFFER_SIZE];
U16 img_idx_in_day;
U32 wdt_camera_cnt = 0;


/* task variable */
OS_MUT camMutex;
OS_TID CameraTskID = NULL;
static U64 tskCAMStack[700/8];

S8 fn[25] = "Images\\ImgBuf0.dat";
FILE *f_Img;
#define fn_write_file_indx 				fn[13]

Cam_locate_t ImageWriteIdx, ImageReadIdx;

S8 fn_read[25] = "Images\\ImgBuf0.dat";
FILE *f_rImg;
#define fn_read_file_indx 			fn_read[13]

/* api */
void vCameraInit(void);
void CAM_SnapShot(void);

/* private functions */
BOOL blWaitAck(void);


/* implement ===========================================================*/
#if 1
void CAM_Putc(S8 c)
{
	USART_PrChar(CAM_PORT, c);
	while(USART_GetFlagStatus(CAM_PORT,USART_FLAG_TXE)==RESET);
}
#else
#define CAM_Putc(c)		USART_PrChar(USART1, c)
#endif

U32 ulString2U32(S8* str)
{
	U32 ret;
	ret = str[3];
	ret = (ret << 8) + (str[2]&0xFF);
	ret = (ret << 8) + (str[1]&0xFF);
	ret = (ret << 8) + (str[0]&0xFF);

	return ret;
}

U16 ulString2U16(S8* str)
{
	U16 ret;
	ret = str[1];
	ret = (ret << 8) + (str[0]&0xFF);

	return ret;
}

void CAM_Write(S8 *str, U16 size)
{
	while(size)
	{
		CAM_Putc(*str++);
		size--;
	}
}

void bus_dly(uint32_t timeout)
{
	while(timeout--);
}


BOOL blWaitAck(void)
{
	U16 tmo = 5;
	while((tmo>0)&& (!CamAck.Ack))
	{
		tmo --;
		os_dly_wait(100);
	}

	return CamAck.Ack;
}

void vChangeCAMBaud(U8 id, CAM_BAUD_e CAM_baud)
{
	GPIO_SetBits(RS485_CTL_PORT, RS485_CTL_PIN);
	bus_dly(1500);
	/* clear flag ack */
	memset(&CamAck, 0, sizeof(CamAck));
	/* start frame */
	CAM_Putc('U');
	/* change baud command */
	CAM_Putc('I');
	CAM_Putc(id);
	/* size of image */
	CAM_Putc(CAM_baud);
	/* end request */
	CAM_Putc('#');

	bus_dly(1500);
	GPIO_ResetBits(RS485_CTL_PORT, RS485_CTL_PIN);

	blWaitAck();

	/* changed baud */
}

void vChangeCAMRatio(U8 id, U8 ComRatio)
{
	GPIO_SetBits(RS485_CTL_PORT, RS485_CTL_PIN);
	bus_dly(1500);
	/* clear flag ack */
	memset(&CamAck, 0, sizeof(CamAck));
	/* start frame */
	CAM_Putc('U');
	/* change baud command */
	CAM_Putc('Q');
	/* camera id */
	CAM_Putc(id);
	/* size of image */
	CAM_Putc(ComRatio);
	/* end request */
	CAM_Putc('#');

	bus_dly(1500);
	GPIO_ResetBits(RS485_CTL_PORT, RS485_CTL_PIN);

	blWaitAck();
}

BOOL blChangeCAM_ID(U8 id, U8 new_id)
{
	GPIO_SetBits(RS485_CTL_PORT, RS485_CTL_PIN);
	bus_dly(1500);
	/* clear flag ack */
	memset(&CamAck, 0, sizeof(CamAck));
	/* start frame */
	CAM_Putc('U');
	/* change baud command */
	CAM_Putc('D');
	/* camera id */
	CAM_Putc(id);
	/* size of image */
	CAM_Putc(new_id);
	/* end request */
	CAM_Putc('#');

	bus_dly(1500);
	GPIO_ResetBits(RS485_CTL_PORT, RS485_CTL_PIN);

	return blWaitAck();
}

/*
 *
 *
 *  55 48 00 32 C8 00 23                              UH.2È.#
 */
void vRequestImage(U8 CAM_ID, CAM_Size_e CAM_Size, U16 pk_size)
{
	GPIO_SetBits(RS485_CTL_PORT, RS485_CTL_PIN);
	bus_dly(1500);
	/* clear flag ack */
	memset(&CamAck, 0, sizeof(CamAck));
	/* start frame */
	CAM_Putc('U');
	/* request command */
	CAM_Putc('H');
	/* id camera */
	CAM_Putc(CAM_ID);
	/* size of image */
	CAM_Putc(CAM_Size);
	/* size of read package */
	CAM_Write((S8*)&pk_size, 2);
	/* end request */
	CAM_Putc('#');

	bus_dly(1500);
	GPIO_ResetBits(RS485_CTL_PORT, RS485_CTL_PIN);

}

/*
 * Local funtions for load image data
 */
void vLoadImage(U8 cam_id, S16 pkg_id)
{
	GPIO_SetBits(RS485_CTL_PORT, RS485_CTL_PIN);
	bus_dly(1500);
	/* clear flag ack */
	memset(&CamAck, 0, sizeof(CamAck));
	/* start frame */
	CAM_Putc('U');
	/* request command */
	CAM_Putc('E');
	/* id camera */
	CAM_Putc(cam_id);
	/* id package */
	CAM_Write((S8*)&pkg_id, 2);
	/* end request */
	CAM_Putc('#');
	bus_dly(1500);
	GPIO_ResetBits(RS485_CTL_PORT, RS485_CTL_PIN);

}

/***********************************************************************/

/* init a hole for save camera data */
void vSDBufferInit(void)
{
	U32 temp;
	/* Read read location */
	temp = BKP_ReadBackupRegister(IMG_LIST_READ_IDX_H);
	temp = (temp<<16) + (BKP_ReadBackupRegister(IMG_LIST_READ_IDX));
	ImageReadIdx.all = temp;
	fn_read_file_indx = ImageReadIdx.field.fileIdx + '0';

	// Read write location */
	temp = BKP_ReadBackupRegister(IMG_LIST_WRITE_IDX_H);
	temp = (temp<<16) + (BKP_ReadBackupRegister(IMG_LIST_WRITE_IDX));
	ImageWriteIdx.all = temp;
	fn_write_file_indx = ImageWriteIdx.field.fileIdx + '0';

	// camera index in day
	img_idx_in_day = BKP_ReadBackupRegister(IMG_INDEX_IN_DAY);
}

/*
 * reset idex image in buffer.
 */
void vSDBufferReset(void)
{
	ImageWriteIdx.all = 0;
	fn_write_file_indx = ImageWriteIdx.field.fileIdx + '0';
	BKP_WriteBackupRegister(IMG_LIST_WRITE_IDX, (ImageWriteIdx.all)&0xFFFF);
	BKP_WriteBackupRegister(IMG_LIST_WRITE_IDX_H, (ImageWriteIdx.all>>16));

	// Read write location */
	ImageReadIdx.all = 0;
	fn_read_file_indx = ImageReadIdx.field.fileIdx + '0';
	BKP_WriteBackupRegister(IMG_LIST_READ_IDX, (ImageReadIdx.all&0xFFFF));
	BKP_WriteBackupRegister(IMG_LIST_READ_IDX_H, (ImageReadIdx.all>>16));
}

/*
 * write fix 1024 data to sdcard.
 */
S8 vSDBufferWrite(S8* buf)
{
	U16 write_cnt;
	if(ImageWriteIdx.field.location == 0)
		f_Img = fopen(fn,"wb");
	else
		f_Img = fopen(fn,"ab");

	if(f_Img == NULL)
		return NO_FILE;

	write_cnt = fwrite(buf, sizeof(S8), IMG_PKT_BUFFER_SIZE, f_Img);
	if(write_cnt != IMG_PKT_BUFFER_SIZE){
		// Close open logfile
		fclose(f_Img);
		return SIZE_PACKET_ERR;
	};

	// Update write location

	if(++ImageWriteIdx.field.location > MAX_LOCATION_BUF){
		ImageWriteIdx.field.location = 0;
		if(++ImageWriteIdx.field.fileIdx > FILE_INDEX_MAX)
			ImageWriteIdx.field.fileIdx = 0;

		// Reset read location if overwrite same bank
		if(ImageReadIdx.field.fileIdx == ImageWriteIdx.field.fileIdx){
			ImageReadIdx.field.location = 0;

			if(++ImageReadIdx.field.fileIdx > FILE_INDEX_MAX)
				ImageReadIdx.field.fileIdx = 0;


			BKP_WriteBackupRegister(IMG_LIST_READ_IDX, (ImageReadIdx.all&0xFFFF));
			BKP_WriteBackupRegister(IMG_LIST_READ_IDX_H, (ImageReadIdx.all>>16));

			// Update read filename
			fn_read_file_indx = ImageReadIdx.field.fileIdx + '0';
		};

		// Update write filename
		fn_write_file_indx = ImageWriteIdx.field.fileIdx + '0';

	};
	BKP_WriteBackupRegister(IMG_LIST_WRITE_IDX, (ImageWriteIdx.all)&0xFFFF);
	BKP_WriteBackupRegister(IMG_LIST_WRITE_IDX_H, (ImageWriteIdx.all>>16));

	// Close open logfile
	fclose(f_Img);
	return RET_OK;
}

/*
 * tao mot file anh tren the nho de luu anh hien tai
 */
S8 bWriteImage(U8 cam_id, U16 pkt_id, U16 pkt_num, S8* buf, U16 size, U16 checksum)
{
	uGPSDateTime_t dt;
	char temp_fn[22];
	U16 write_cnt;

	memcpy(&dt, &snapshot_time, 4);
	sprintf(temp_fn, "%02u%02u%02u\\%02u_%04u.jpeg", dt.day, dt.month, dt.year, cam_id, img_idx_in_day);

	if(pkt_id == 1)	// create new file if new image received.
		f_Img = fopen(temp_fn,"wb");
	else
		f_Img = fopen(temp_fn,"ab");

	if(f_Img == NULL)
		return NO_FILE;

	write_cnt = fwrite(buf, sizeof(S8), size, f_Img);
	if(write_cnt != size){
		// Close open logfile
		fclose(f_Img);
		return SIZE_PACKET_ERR;
	};

	// Close open logfile
	fclose(f_Img);

	return RET_OK;
}

/*
 * xoa file anh tam tren the nho.
 */
S8 bRemoveTempFile(U8 cam_id)
{
	uGPSDateTime_t dt;
	char image_fn[22];

	memcpy(&dt, &snapshot_time, 4);
	sprintf(image_fn, "%02u%02u%02u\\%02u_%04u.jpeg", dt.day, dt.month, dt.year, cam_id, img_idx_in_day);

	if(fdelete(image_fn) != 0)
		return NO_FILE;

	return RET_OK;
}

/*
 * lui 3 ngay so voi ngay hien tai
 */
void vConvertDate(char o_date, char o_month, char o_year, char *n_date, char *n_month, char *n_year)
{
    const char month_table[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if(o_date > 3)
    {
        *n_date = o_date - 3;
        *n_month = o_month;
        *n_year = o_year;
    }
    else
    {
        if(o_month == 1)
        {
            *n_year = o_year - 1;
            *n_month = 12;
        }
        else
        {
            *n_month = o_month - 1;
            *n_year = o_year;
        }

        if((((2000+*n_year)%4)==0) && (*n_month == 2))
            *n_date = 29 - (3 - o_date);
        else
            *n_date = month_table[*n_month] - (3 - o_date);
    }
}

/*
 * xoa folder anh cua 2 ngay truoc.
 */
S8 bDeleteImageOldDay(uGPSDateTime_t dt, U16 id)
{
	int j, i;
	char image_fn[22];
	char day, month, year;

	if(id == 0)
	{
		vConvertDate(dt.day, dt.month, dt.year, &day, &month, &year);

		for(j=0;j<Config.CAM.cam_number;j++)
		{
			for(i=0;i<500;i++)
			{
				sprintf(image_fn, "%02u%02u%02u\\%02u_%04u.jpeg", day, month, year, j, i);
				fdelete(image_fn);
			}
		}

		//sprintf(image_fn, "\\%02u%02u%02u\\\x0", day, month, year);
		image_fn[7] = 0;
		if(fdelete(image_fn) != 0)
		{
			return NO_FILE;
		}

	}

	return RET_OK;
}

/*
 * copy file anh tam hoan chinh vao buffer de truyen len server
 */
S8 bMoveImageToBuffer(U8 cam_id)
{
	uGPSDateTime_t dt;
	char image_fn[22];
	u32 file_size;
	u16 total_pkg, pkg_id, checksum = 0;

	memcpy(&dt, &snapshot_time, 4);
	sprintf(image_fn, "%02u%02u%02u\\%02u_%04u.jpeg", dt.day, dt.month, dt.year, cam_id, img_idx_in_day);

	os_mut_wait(camMutex, 0xFFFF);

	f_rImg = fopen(image_fn,"rb");

	if(f_rImg == NULL)
	{
		os_mut_release(camMutex);

		return NO_FILE;
	}

	/* get file length */
	fseek (f_rImg, 0, SEEK_END);
	file_size = ftell(f_rImg);
	/* get total package available */
	total_pkg = file_size/IMG_PACKET_SIZE;
	total_pkg += (file_size%IMG_PACKET_SIZE)? 1: 0;

	/* split file to n package and calculate chechsum */
	pkg_id = 1;
	rewind (f_rImg);
	while(!feof(f_rImg))
	{
		file_size = fread(tempBuff + DATA_OFFSET, sizeof(uint8_t), IMG_PACKET_SIZE, f_rImg);

		if((file_size <= IMG_PACKET_SIZE) && (file_size > 0))
		{
			tempBuff[0] = ((file_size + DATA_OFFSET)&0xFF);
			tempBuff[1] = (((file_size + DATA_OFFSET)>>8)&0xFF);
			/* box id */
			memcpy(tempBuff+2, Config.BoxID, 5);

			/* write cam id */
			tempBuff[7] = cam_id;
			/* append data/time */
			memcpy(tempBuff+8, &snapshot_time, 4);
			/* packet id & packet number */
			memcpy(tempBuff+12, &pkg_id, 2);
			memcpy(tempBuff+14, &total_pkg, 2);

			tempBuff[16] = 0;
			tempBuff[17] = 0;
			tempBuff[18] = 0;

			// calculate checksum
			checksum = cCheckSumFixSize(tempBuff + 2,  file_size + DATA_OFFSET - 2);
			/* add checksum */
			memcpy(tempBuff + DATA_OFFSET + file_size, &checksum, 2);
			/* save to buffer */
			if(vSDBufferWrite(tempBuff)!=RET_OK)
			{
				// Close open logfile
				fclose(f_rImg);
				os_mut_release(camMutex);

				/* delete old file*/
				//bDeleteImageOldDay(dt, img_idx_in_day);

				return NO_FILE;
			}

			pkg_id++;
		}
	}
	// Close open logfile
	fclose(f_rImg);
	os_mut_release(camMutex);


	return RET_OK;
}

/*
 * backup an image to buffer send.
 * filename: ddmmyy\ii_nnnn.jpeg
 */
S8 bBackupImageToBuffer(S8* folder, S8 *filename)
{
	uGPSDateTime_t dt;
	char image_fn[22];
	u32 file_size;
	u16 total_pkg, pkg_id, checksum = 0;
	U8 cam_id;

	os_mut_wait(camMutex, 0xFFFF);

	strncpy(image_fn, folder, 6);

	strcpy(image_fn + 7, filename);
	image_fn[6] = '\\';


	f_rImg = fopen(image_fn,"rb");

	if(f_rImg == NULL)
	{
		os_mut_release(camMutex);
		return NO_FILE;
	}

	cam_id = filename[1] - '0';
	vUpdateTime(&dt);

	/* get file length */
	fseek (f_rImg, 0, SEEK_END);
	file_size = ftell(f_rImg);
	/* get total package available */
	total_pkg = file_size/IMG_PACKET_SIZE;
	total_pkg += (file_size%IMG_PACKET_SIZE)? 1: 0;

	/* split file to n package and calculate chechsum */
	pkg_id = 1;
	rewind (f_rImg);
	while(!feof(f_rImg))
	{
		file_size = fread(tempBuff + DATA_OFFSET, sizeof(uint8_t), IMG_PACKET_SIZE, f_rImg);

		if((file_size <= IMG_PACKET_SIZE) && (file_size > 0))
		{
			tempBuff[0] = ((file_size + DATA_OFFSET)&0xFF);
			tempBuff[1] = (((file_size + DATA_OFFSET)>>8)&0xFF);
			/* box id */
			memcpy(tempBuff+2, Config.BoxID, 5);

			/* write cam id */
			tempBuff[7] = cam_id;
			/* append data/time */
			memcpy(tempBuff+8, &dt, 4);
			/* packet id & packet number */
			memcpy(tempBuff+12, &pkg_id, 2);
			memcpy(tempBuff+14, &total_pkg, 2);

			tempBuff[16] = 0;
			tempBuff[17] = 0;
			tempBuff[18] = 0;

			// calculate checksum
			checksum = cCheckSumFixSize(tempBuff + 2,  file_size + DATA_OFFSET - 2);
			/* add checksum */
			memcpy(tempBuff + DATA_OFFSET + file_size, &checksum, 2);
			/* save to buffer */
			if(vSDBufferWrite(tempBuff)!=RET_OK)
			{
				// Close open logfile
				fclose(f_rImg);
				os_mut_release(camMutex);

				return NO_FILE;
			}

			pkg_id++;
		}
	}
	// Close open logfile
	fclose(f_rImg);
	os_mut_release(camMutex);

	return RET_OK;
}

/*
 * doc 1 packet trong buffer anh.
 */
S8 blReadImagePacket(S8* buf)
{
	U16 write_cnt;
	U16 checksum, size, buf_checksum;

// If read/write at same location, then generate error
	if(ImageReadIdx.all == ImageWriteIdx.all) return EMPTY;

//	sprintf(tempBuff, "\r\nG:%X,%X\x0", ImageWriteIdx.all, ImageReadIdx.all);
//	USART_PrCStr(USART1, tempBuff);

	os_mut_wait(camMutex, 0xFFFF);

// Open buffer at read mode
	f_rImg = fopen(fn_read, "rb");

	if(f_rImg == NULL){
		os_mut_release(camMutex);

		return SDCARD_ERR;
	};

	if(fseek(f_rImg, ImageReadIdx.field.location*IMG_PKT_BUFFER_SIZE, SEEK_CUR) == EOF){
		// Close the file
		fclose(f_rImg);
		os_mut_release(camMutex);

		return OVER_FILE;
	}

	write_cnt = fread(buf, sizeof(uint8_t), IMG_PKT_BUFFER_SIZE, f_rImg);

	// Close the file
	fclose(f_rImg);

	os_mut_release(camMutex);

	size = buf[1] & 0xFF;
	size = (buf[0] & 0xFF) + (size << 8);
	// tinh checksum tu data den truoc truong checksum trong buffer.
	checksum = cCheckSumFixSize(buf + 2,  size - 2);

	//checksum in buffer.
	memcpy(&buf_checksum, buf + size, 2);

	if((write_cnt != IMG_PKT_BUFFER_SIZE))
	{

		return OVER_FILE;
	}

	/* wrong checksum. */
	if((checksum != buf_checksum))
	{
		return DATA_CHECKSUM_FAIL;
	}

	return OK;
}

void vNextImagePacket(void)
{
	// if read/write in the same bank, move to near write location
	if(ImageReadIdx.field.fileIdx == ImageWriteIdx.field.fileIdx){
		if(ImageReadIdx.field.location < ImageWriteIdx.field.location)
			ImageReadIdx.field.location++;
		else
			return;
	}

	// if read/write at diffirent bank
	else if(++ImageReadIdx.field.location > MAX_LOCATION_BUF){
		ImageReadIdx.field.location = 0;
		if(++ImageReadIdx.field.fileIdx > FILE_INDEX_MAX){
			ImageReadIdx.field.fileIdx = 0;
		}

		// Update filename
		fn_read_file_indx = ImageReadIdx.field.fileIdx + '0';
	}
	/* write backup */
	BKP_WriteBackupRegister(IMG_LIST_READ_IDX, (ImageReadIdx.all&0xFFFF));
	BKP_WriteBackupRegister(IMG_LIST_READ_IDX_H, (ImageReadIdx.all>>16));
}


/* data: 2 byte id_pkg, 2 byte pkg_size, n bytes image, 2 byte checksum */
void vCAM_Recv_Handler(S8 ch)
{
	static uint16_t msg_size = 0;
	switch(ISR_State)
	{
	case CAM_START:
		if(ch == 'U')
		{
			ISR_State = CAM_CHECK_COMMAND;
		}
		break;
	case CAM_CHECK_COMMAND:
		if(ch == 'F')			/* image data */
		{
			/* calculate checksum */
			CAMchecksum = (uint16_t)('U') + (uint16_t)('F');
			/* change mode */
			pCAM_Resp = CamEvent.Data;
			ISR_State = CAM_GET_IMAGE_PKG_SIZE;
		}
		else if((ch == 'E') || (ch == 'I')||(ch == 'H')||(ch == 'Q')||(ch == 'D'))		/* ack read data */
		{
			pCAM_Resp = CAM_RspBuffer;
			ISR_State = CAM_ACK_COMMAND;
		}

		else if(ch == 'R')		/* Response image infor */
		{
			msg_size = 8;
			pCAM_Resp = CamEvent.Data;
			ISR_State = CAM_GET_IMAGE_HEADER;
		}
		else
			ISR_State = CAM_START;
		break;

	case CAM_ACK_COMMAND:
		/* ack or nack */
		*pCAM_Resp ++ = ch;
		if(pCAM_Resp >= (CAM_RspBuffer + sizeof(CAM_RspBuffer)))
			pCAM_Resp = CAM_RspBuffer;
		if(ch == '#')
		{
			CamAck.Ack = 1;
			ISR_State = CAM_START;
		}
		break;
	case CAM_GET_IMAGE_HEADER:
		msg_size--;
		*pCAM_Resp ++ = ch;
		if(pCAM_Resp >= (CamEvent.Data + sizeof(CamEvent.Data)))
			pCAM_Resp = CamEvent.Data;

		if(msg_size == 0)
		{
			CamEvent.ImageHeaderEvent = 1;
			isr_evt_set(IMG_HEADER_FINISH_FLAG, CameraTskID);
			ISR_State = CAM_START;
		}
		break;
	case CAM_GET_IMAGE_PKG_SIZE:
		*pCAM_Resp ++ = ch;
		if(pCAM_Resp >= (CamEvent.Data + sizeof(CamEvent.Data)))
			pCAM_Resp = CamEvent.Data;
		/* calculate checksum */
		CAMchecksum += (uint16_t)(ch)&0xFF;

		if(pCAM_Resp >= CamEvent.Data + 5)
		{
			ImagePkg.Cam_id = CamEvent.Data[0];
			/* calculate package id, package size */
			ImagePkg.ID = ulString2U16(CamEvent.Data+1);
			ImagePkg.Size = ulString2U16(CamEvent.Data+3);
			msg_size = ImagePkg.Size;
			/* set pointer for get data */
			pCAM_Resp = ImagePkg.pData;
			if(ImagePkg.Size > IMG_PACKET_SIZE)
			{
				ISR_State = CAM_START;
			}
			else
				ISR_State = CAM_GET_IMAGE_DATA;
		}
		break;
	case CAM_GET_IMAGE_DATA:
		msg_size --;
		/* put data to buffer */
		*pCAM_Resp ++ = ch;
		if(pCAM_Resp >= (ImagePkg.pData + IMG_PKT_BUFFER_SIZE))
			pCAM_Resp = ImagePkg.pData;
		/* calculate checksum */
		CAMchecksum += (uint16_t)(ch)&0xFF;

		/* check end of data */
		if(msg_size == 0)
		{
			/* point to checksum */
			pCAM_Resp = (S8*)&ImagePkg.Checksum;

			ISR_State = CAM_CHECKSUM_VALUE;
		}
		break;
	case CAM_CHECKSUM_VALUE:
		*pCAM_Resp ++ = ch;

		if(pCAM_Resp >= (S8*)&ImagePkg.Checksum + 2)
		{
			/* set event*/
			CamEvent.ImageDataEvent = 1;
			/* set evt for task */
			isr_evt_set(IMG_PKG_FINISH_FLAG, CameraTskID);
			ISR_State = CAM_START;
		}
		break;
	}
}


/*
 * save all image to buffer.
 * nen xu ly thu lai 3 lan.
 */
void vSaveAllImageToBuffer(void)
{
	int i, status = 0;
	U8 retrySave = 3;

	for(i=0;i<Config.CAM.cam_number;i++)
	{
		/* Have an Image not finish */
		if((pCamera[i].pkgID > pCamera[i].imgHeader.ImgPkg))
		{
			retrySave = 3;
			/* thu lai 3 lan khi move file */
			while(retrySave)
			{
				if(bMoveImageToBuffer(pCamera[i].ID) == 0)
				{
					status = 1;

					break;			/* finish */
				}

				if(retrySave)
					retrySave --;
			}
		}
	}

	/* neu co file dc luu thi tang index file len */
	if(status)
	{
		/* index in day */
		img_idx_in_day ++;
		BKP_WriteBackupRegister(IMG_INDEX_IN_DAY, img_idx_in_day);

	}

}


int8_t _cam_current_id = -1;

__task void vCameraTask(void)
{
	OS_RESULT result;
	static S8 requestSnapTimeout  = 3;
	// set begin state.
	CamState = WAIT_SNAPSHOT;

	for(;;)
	{
		switch(CamState)
		{
		case WAIT_SNAPSHOT:
			if(os_evt_wait_and(SNAPSHOT_FLAG, 0xFFFF) == OS_R_EVT){
				uGPSDateTime_t tm;
				int i;
				/* set flag */
				snapshoting = 1;

				/* Init variables */
				for(i=0;i<Config.CAM.cam_number;i++)
				{
					pCamera[i].pkgID = 0;

					pCamera[i].imgHeader.ImgInByte = 0;
					pCamera[i].imgHeader.ImgPkg = 0;
				}

				/* image packet info reseted */
				ImagePkg.Checksum = 0;
				ImagePkg.ID = 0;
				ImagePkg.Size = 0;
				ImagePkg.pData = CameraBuffer+DATA_OFFSET;

				/* camera ack info reset */
				CamAck.Ack = 0; CamAck.Nack = 0;
				/* camera event set to zero */
				memset(CamEvent.Data, 0, sizeof(CamEvent.Data));
				CamEvent.ImageDataEvent = 0;
				CamEvent.ImageHeaderEvent = 0;

				for(i=0;i<Config.CAM.cam_number;i++)
				{
					vChangeCAMRatio(pCamera[i].ID, 250);
				}
				/* changed time snapshot */
				vUpdateTime(&tm);
				memcpy(&snapshot_time, &tm, 4);

				/* delete old folder if it exists */
				bDeleteImageOldDay(tm, img_idx_in_day);

				/* change state */
				CamState = SNAPSHOTTING;
				ISR_State = CAM_START;
				/* retry get header */
				requestSnapTimeout  = RETRY_SNAPSHOT_NUMBER;

				wdt_camera_cnt = 0;
				_cam_current_id = 0;

//				sprintf(CAM_RspBuffer, "\r\n====%u=====\x0", img_idx_in_day);
//				USART_PrCStr(DBG_Port, CAM_RspBuffer);
			}
			break;
		case SNAPSHOTTING:
			/* snapshot */
			if((_cam_current_id < Config.CAM.cam_number))
			{
				// if image not received.
				if((pCamera[_cam_current_id].pkgID == 0) && (requestSnapTimeout > 0))
				{
//					sprintf(CAM_RspBuffer, "\r\nR: %u\x0", _cam_current_id);
//					USART_PrCStr(DBG_Port, CAM_RspBuffer);
					// number retry is count down.
					requestSnapTimeout --;

					vRequestImage(pCamera[_cam_current_id].ID, pCamera[_cam_current_id].ImageSize, IMG_PACKET_SIZE);
					/* wait 100ms */
					result = os_evt_wait_and(IMG_HEADER_FINISH_FLAG, HEADER_IMAGE_FLAG_TMO/OS_TICK_RATE_MS);

					// force isr state.
					ISR_State = CAM_START;
					// check event.
					if((result != OS_R_TMO) && (CamEvent.Data[0] == _cam_current_id))
					{
						/* size of image, number of package */
						pCamera[_cam_current_id].imgHeader.ImgInByte = ulString2U32(CamEvent.Data +1);
						pCamera[_cam_current_id].imgHeader.ImgPkg = ulString2U16(CamEvent.Data + 5);
						/* set ready get image */
						pCamera[_cam_current_id].pkgID = 1;
						pCamera[_cam_current_id].pkgTmo = RETRY_PACKET_NUMBER;

						memset(CamEvent.Data, 255, 8);
						CamState = LOAD_ALL_IMAGE;
					}

					break;
				}
				// next camera
				_cam_current_id ++;
				requestSnapTimeout = RETRY_SNAPSHOT_NUMBER;
				break;
			}

//			USART_PrCStr(DBG_Port, "\r\nCamera finish!");
			// snapshot ok. change status for request again.
			snapshoting = 0;
			CamState = WAIT_SNAPSHOT;

			vSaveAllImageToBuffer();

			break;
		case LOAD_ALL_IMAGE:
			// get all picture packet.
			while((pCamera[_cam_current_id].pkgID > 0) && (pCamera[_cam_current_id].pkgID <= pCamera[_cam_current_id].imgHeader.ImgPkg))
			{
//				sprintf(CAM_RspBuffer, "\r\nC: %u-%u/%u\x0", _cam_current_id, pCamera[_cam_current_id].pkgID, pCamera[_cam_current_id].imgHeader.ImgPkg);
//				USART_PrCStr(DBG_Port, CAM_RspBuffer);
				/* Start load packet image data */
				vLoadImage(pCamera[_cam_current_id].ID, pCamera[_cam_current_id].pkgID);

				result = os_evt_wait_and(IMG_PKG_FINISH_FLAG, PACKET_IMAGE_FLAG_TMO/OS_TICK_RATE_MS);
				// force isr state.
				ISR_State = CAM_START;

				if((result != OS_R_TMO) && ((CAMchecksum) == ImagePkg.Checksum) &&
						(ImagePkg.ID == pCamera[_cam_current_id].pkgID) && (pCamera[_cam_current_id].ID == ImagePkg.Cam_id))
				{
					if(bWriteImage(pCamera[_cam_current_id].ID, pCamera[_cam_current_id].pkgID,
							pCamera[_cam_current_id].imgHeader.ImgPkg, ImagePkg.pData, ImagePkg.Size, ImagePkg.Checksum)==0)
					{
						/* increase index packet */
						pCamera[_cam_current_id].pkgID ++;
						/* reset timeout for packet */
						pCamera[_cam_current_id].pkgTmo = RETRY_PACKET_NUMBER;

						ImagePkg.Cam_id = 255;
						ImagePkg.Checksum = 0;
						/* next camera */
						continue;
					}

				}

				/* if time out. retry */
				pCamera[_cam_current_id].pkgTmo--;
				if(pCamera[_cam_current_id].pkgTmo == 0)
				{
//					sprintf(CAM_RspBuffer, "\r\nD: %u\x0", _cam_current_id);
//					USART_PrCStr(DBG_Port, CAM_RspBuffer);
					/* cancel current image */
					pCamera[_cam_current_id].pkgID = 0;
					/* reset timeout */
					pCamera[_cam_current_id].pkgTmo = RETRY_PACKET_NUMBER;

					/* remove temp file */
					bRemoveTempFile(pCamera[_cam_current_id].ID);
				}
			}
			// next camera.
			CamState = SNAPSHOTTING;

			break;
		}
	}
}

BOOL blAllImageDownloaded(void)
{
	int i;
	for(i=0;i<Config.CAM.cam_number;i++)
	{
		/* Have an Image not finish */
		if((pCamera[i].pkgID > 0) &&
								(pCamera[i].pkgID <= pCamera[i].imgHeader.ImgPkg))
		{
			return 0;
		}
	}

	/* finish all */
	return 1;
}

/*
 * Create camera
 */
int8_t vCreateCamera(U8 number)
{
	U8 i;

	/* alloc memory */
	pCamera = (CAMERA_t*)calloc(number, sizeof(CAMERA_t));
	if(pCamera == NULL)
		return -1;

	for(i=0;i<number;i++)
	{
		pCamera[i].ID = i;
		pCamera[i].Baud = B_115200;
		pCamera[i].ImageSize = S_640x480;
	}

	return 0;
}

/* get write camera index in sdcard */
U32 uGetCameraWriteIdx(void)
{
	return ImageWriteIdx.all;
}

/* get read camera index in sdcard */
U32 uGetCameraReadIdx(void)
{
	return ImageReadIdx.all;
}

/* return camera image counter for debug */
U8 uGetImageCount(void)
{
	return img_idx_in_day;
}

/* reset index file image when next day. this function called in rtc.c file*/
void vReSetImageCount(void)
{
	img_idx_in_day = 0;
	BKP_WriteBackupRegister(IMG_INDEX_IN_DAY, img_idx_in_day);
}

/*
 * Init task camera.
 */
void vCameraInit(void)
{
int8_t res;

	CamAck.Ack = 0;
	memset(&CamEvent, 0, sizeof(CamEvent));
	ImagePkg.pData = CameraBuffer+DATA_OFFSET;

	snapshoting = 0;

	/* validate alloc memory */
	res = vCreateCamera(Config.CAM.cam_number);
	if(res != 0)
		return;

	vSDBufferInit();

	os_mut_init(camMutex);

	CameraTskID = os_tsk_create_user(vCameraTask, RTX_CAM_PRIORITY, tskCAMStack, sizeof(tskCAMStack));
}

/*
 * Take a photo
 */
void CAM_SnapShot(void)
{
	/* Validate task is running */
	if(CameraTskID == NULL)
		return;

	//	return;
	if(snapshoting){
		snapshoting = 0;
		return;
	}

	/* clear all flags */
	os_evt_clr (SNAPSHOT_FLAG, CameraTskID);
	os_evt_clr (IMG_HEADER_FINISH_FLAG, CameraTskID);
	os_evt_clr (IMG_PKG_FINISH_FLAG, CameraTskID);

	/* reset state */
	CamState = WAIT_SNAPSHOT;

	/* Request snapshot */
	os_evt_set(SNAPSHOT_FLAG, CameraTskID);

}


void vCAM_GPIOConfig(void)
{
	GPIO_InitTypeDef config;

	/* Enable GPIOC clocks */
	RCC_APB2PeriphClockCmd(CAM_RCC_IO_PORT_TX | CAM_RCC_IO_PORT_RX | RCC_APB2Periph_AFIO, ENABLE);
	/* Enable UART clock, if using USART2 or USART3 ... we must use RCC_APB1PeriphClockCmd(RCC_APB1Periph_USARTx, ENABLE) */
	RCC_APB1PeriphClockCmd(CAM_RCC_Periph, ENABLE);

	config.GPIO_Mode = GPIO_Mode_AF_PP;
	config.GPIO_Pin = GPIO_Pin_10;
	config.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(CAM_IO_PORT_TX, &config);
	// Rx
	config.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	config.GPIO_Pin = GPIO_Pin_11;

	GPIO_Init(CAM_IO_PORT_RX, &config);

	// SUPPLY
	config.GPIO_Mode = GPIO_Mode_Out_PP ;
	config.GPIO_Pin = RS485_CTL_PIN;
	config.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(RS485_CTL_PORT, &config);

	GPIO_ResetBits(RS485_CTL_PORT, RS485_CTL_PIN);
}

void vCAM_UART_baud(U32 baud)
{
	USART_InitTypeDef config;

	config.USART_BaudRate = baud;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	USART_Init(CAM_PORT, &config);

	USART_ITConfig(CAM_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(CAM_PORT, ENABLE);
}


/*
 * \brief		init uart module.
 * \param		void
 * \return		void
 * \note		this function must call before rtx os system init.
 */
void vCAM_UART_Init(void)
{
	USART_InitTypeDef config;
	NVIC_InitTypeDef NVIC_InitStructure;

	vCAM_GPIOConfig();

	config.USART_BaudRate = 115200;
	config.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	config.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	config.USART_Parity = USART_Parity_No;
	config.USART_StopBits = USART_StopBits_1;
	config.USART_WordLength = USART_WordLength_8b;

	USART_Init(CAM_PORT, &config);

	USART_ITConfig(CAM_PORT, USART_IT_RXNE, ENABLE);

	USART_Cmd(CAM_PORT, ENABLE);

	/* Enable the USARTy Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = CAM_IRQ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	/* Configure the NVIC Preemption Priority Bits */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
}

// irq
void CAM_IRQHandler(void)
{
	S8 c;

	if(USART_GetITStatus(CAM_PORT, USART_IT_RXNE) != RESET)
	{
		c = USART_ReceiveData(CAM_PORT);
		/* callback function */
		vCAM_Recv_Handler(c);
	}
}


uint8_t WDT_Check_CamTask_Service(void)
{
	if (CameraTskID != NULL)
		wdt_camera_cnt++;
	else
		wdt_camera_cnt = 0;

	if(wdt_camera_cnt >= WDT_CAMERA_MAX_TIMING)
		return 1;

	return 0;
}

