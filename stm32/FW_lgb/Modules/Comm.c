/*
 * transport.c
 *
 *  Created on: Jan 9, 2012
 *      Author: Van Ha
 */					 

#include <string.h>

#include "stm32f10x.h"	  	// Standard Peripheral header files
#include "rtl.h"			// Realtime RTX Library header file
#include "STM32_Init.h"
#include "stdio.h"

#include "utils.h"
#include "serial.h"

#include "comm.h"


/**** typedef & enum ***/
typedef void (*fnProcessMSG_t) (U8* pMsg);

/***** local functions ************_________________________________****/
__task void vCOMM_RxTask(void);
__task void vCOMM_TxTask(void);
void vDecodeRxMessage(U8* data);

void vResponse(U8* pdata);


/**** local variables *-----------------------------------------------*/
/* Reserve a memory for 32 blocks of 20-bytes. */
_declare_box(RSMemory, 128, 3);
os_mbx_declare (xRSBox, 3);
U8 *pRS_Recv, *pRS_Box;

typedef struct _cmd_t
{
	U8* name;
	U8 name_size;
	fnProcessMSG_t fn;
}cmd_t;

const cmd_t CMD_STRING[] = {
		{"RES", 3, vResponse}
};

//#define RS_DEBUG
#ifdef RS_DEBUG
void vRS_Debug(U8 *str);
#endif


/**
 * \brief 	put a fix length string to comm port.
 */
void vCOMM_Write(U8* str, U16 len)
{
	while(len--)
		vCOMM_Putc(*str++);
}

/**
 * \brief 	put a string to comm port.
 */
void vCOMM_Puts(const U8* str)
{
	while(*str !=0)
		vCOMM_Putc(*str++);
}

/**
 * \brief	init comm port.
 */
void vInitCOMM(void)
{
	_init_box(RSMemory, sizeof(RSMemory), 128);
	os_mbx_init(xRSBox, sizeof(xRSBox));

	pRS_Box = _calloc_box(RSMemory);
	pRS_Recv = pRS_Box;
}

/**
 * \brief
 */
void vCOMM_Callback(U8 c)
{
	if(c == '#') // end of frame.
	{
		*pRS_Recv = 0;
		isr_mbx_send(xRSBox, pRS_Box);
		pRS_Box = _alloc_box(RSMemory);
		pRS_Recv = pRS_Box;
	}else
	{
		*pRS_Recv++ = c;
	}
}

/**
 * \brief 	task init
 */
__task void vCOMM_Init(void)
{
	vInitCOMM();

	//os_tsk_create(vCOMM_RxTask, 13);
	//os_tsk_create(vCOMM_TxTask, 13);

	/* Self delete task */
	os_tsk_delete_self();
}

/**
 * \brief 	process message received from server.
 */
__task void vCOMM_RxTask(void)
{
	U8* ptr;
	//vCOMM_Puts("\r\nTASK RX\r\n");

	for(;;)
	{
		/* wait value = 0xffff, mean wait infinite */
		os_mbx_wait(xRSBox, (void**)&ptr, 0xffff);
		{
#ifdef RS_DEBUG
			vRS_Debug(ptr);
#endif
			vDecodeRxMessage(ptr);
			_free_box(RSMemory, ptr);
		}
	}
}

/**
 * \brief 	decode message.
 */
void vDecodeRxMessage(U8* data)
{
	U8 *uItem[7];
	U16 n, i, j;


	n = string_split(uItem, data, "#$", 7);
	/* command */
	for(i=0;i<n;i++)
	{
		for(j=0;j<1;j++)
		{
			if(strncmp(uItem[0], CMD_STRING[j].name, CMD_STRING[j].name_size)==0)
			{
				CMD_STRING[j].fn(data);
				break;
			}
		}
	}
}

/***************** ham khi co command nhan dc. ******************************/


/**
 * \brief	response
 * 			$[CMD],[MODE],[DATA],[CHK]#
 *
 * $PC,ANALYSIS,START#
 * $PC,ANALYSIS,STOP#
 * $PC,ANALYSIS,LOGFILE,240112#
 */
void vPC_Mode(U8* pdata)
{
	U8 *uItem[7];
	U16 n, i;

	//vCOMM_PutsQueue(pdata);

	n = string_split(uItem, pdata, ",", 7);

	if((strcmp(uItem[1], ANALYSIS_STRING)==0))
	{
		if(strcmp(uItem[2], START_STRING)==0)
		{
			// change uart mode.

			// pause all other task.
		}
		else if(strcmp(uItem[2], STOP_STRING)==0)
		{
			// change back uart mode.

			// resume all task
		}
		else if(strcmp(uItem[2], LOGFILE_STRING)==0)
		{
			// read log in sd card and send all to pc.
		}
	}

}

#ifdef RS_DEBUG
void vRS_Debug(U8 *str)
{
	U8 *rsItem[7];
	U16 n, i;
	n = string_split(rsItem, str, ",#$", 7);

	for(i=0;i<n;i++)
	{
		vCOMM_Puts("\r\nITEM [");
		vCOMM_Putc(i+'0');	vCOMM_Putc(']');
		vCOMM_Puts(rsItem[i]);
	}
}

#endif

