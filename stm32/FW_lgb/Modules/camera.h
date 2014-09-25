/*
 * camera.h
 *
 *  Created on: Mar 21, 2012
 *      Author: Van Ha
 */

#ifndef CAMERA_H_
#define CAMERA_H_

#define WDT_CAMERA_MAX_TIMING 	2250


/* typedef */
typedef struct _CAM_Ack_t
{
	U8 Ack:1;
	U8 Nack:1;
	U8 Request:1;
}CAM_Ack_t;

typedef struct _CAM_Event_t
{
	U16 ImageHeaderEvent:1;
	U16 ImageDataEvent:1;
	S8 Data[8];
}CAM_Event_t;

typedef struct
{
	U8 Cam_id;
	S8* pData;
	U16 ID;
	U16 Size;
	U16 Checksum;
}CAM_PKGData_t;

typedef struct
{
	U32 ImgInByte;
	U16 ImgPkg;
}Image_Header_t;

typedef enum
{
	CAM_START = 0,
	CAM_CHECK_COMMAND,
	CAM_GET_COMMAND,
	CAM_ACK_COMMAND,
	CAM_ACK_PACKAGE,
	CAM_GET_IMAGE_HEADER,
	CAM_GET_IMAGE_PKG_SIZE,
	CAM_GET_IMAGE_DATA,
	CAM_CHECKSUM_VALUE
}CAM_State_e;

typedef enum
{
	S_160x128 = '1',
	S_320x240,
	S_640x480,
	S_1280x1024
}CAM_Size_e;

typedef enum
{
	B_9600 = '0',
	B_19200,
	B_38400,
	B_57600,
	B_115200,
	B_2400,
	B_14400
}CAM_BAUD_e;

typedef struct
{
	/* Properties */
	U8 ID;
	CAM_Size_e ImageSize;
	CAM_BAUD_e Baud;
	/* Events */
	CAM_Ack_t AckInfo;
	CAM_Event_t Event;
	/* Methods */
	CAM_PKGData_t pkgData;
	Image_Header_t imgHeader;
}CAMERA_ALL_t;

typedef struct
{
	/* Properties */
	U8 ID;
	CAM_Size_e ImageSize;
	CAM_BAUD_e Baud;

	U16 pkgID;
	U16 pkgTmo;
	Image_Header_t imgHeader;
}CAMERA_t;

/* api */
uint8_t WDT_Check_CamTask_Service(void);

void vCameraInit(void);
void CAM_SnapShot(void);
BOOL blChangeCAM_ID(U8 id, U8 new_id);
/* buffer */
void vSDBufferReset(void);
void vNextImagePacket(void);
S8 blReadImagePacket(S8* buf);
void vReSetImageCount(void);

/* call in uart4 interrupt */
void vCAM_Recv_Handler(S8 ch);

/* hardware - uart4 */
void vCAM_UART_Init(void);
void vCAM_UART_baud(U32 baud);

#endif /* CAMERA_H_ */
