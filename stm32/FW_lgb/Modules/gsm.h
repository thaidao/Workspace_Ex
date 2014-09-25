/*
 * gsm.h
 *
 *  Created on: Feb 10, 2012
 *      Author: Van Ha
 */

#ifndef GSM_H_
#define GSM_H_


typedef struct _GSM_Flag_t
{
	U32 RecvCGReg:2;
	U32 RecvError:1;
	U32 SocketSendOk:1;
	U32 SocketAck:1;
	U32 TCPIPStaus:3;
	U32 HTTPStatus:3;
	U32 HTTPFILSize:8;
	U32 SMS_flag:3;
	U32 SIM_flag:2;
}GSM_Flag_t;

typedef enum _SMS_State_e
{
	SMS_IDLE = 0,
	SMS_SENDING,
	SMS_SEND_OK
}SMS_State_e;

extern GSM_Flag_t xGSMflag;


#define RECV_CGREG_MASK		0x00000001
#define RECV_ERROR_MASK		0x00000002

#define WDT_GSM_TSK_MAX_TIMING	1500 // 375*800/1000/60 = 5 min


/****  GSM FUNCTIONS ****************************************/
BOOL blGSM_Init(void);
BOOL blSetGSMProfiles(void);
BOOL GSM_blSendCommand(const S8* cmd, const S8* res, U16 timeout);
BOOL GSM_blSendCommandWithoutMutex(const S8* cmd, const S8* res, U16 timeout);
// get respond commands.
BOOL GSM_GetRespondCommand(const S8* cmd, const S8 *end, S8* res, uint16_t size, U16 timeout);
BOOL GSM_GetResCmdpWithoutMutex(const S8* cmd, const S8 *end, S8* res, uint16_t size, U16 timeout);

BOOL blSIMCARD_Status(void);
BOOL blGSM_Status(void);
BOOL blGPRS_Status(void);
#define blGPRS_Ready()		(blGSM_Status()&RECV_CGREG_MASK)
uint8_t get_gsm_status(void);

/***** TCPIP FUNCTIONS *************************************/
BOOL blGPRS_Open(S8* apn, S8 *user, S8* pass, U32 tmo);
U16 uiTCPIP_Status(void);
BOOL blSetTCPIPProfiles(void);
BOOL blTCPIP_Open(const S8* ip, const uint8_t* port, U16 tmo);
void vTCPIP_Close(void);
void vTCPIP_Shutdown(void);

BOOL blTCPIP_Write(const S8* data, U16 size, U16 tmo);
BOOL blTCPIP_Puts(const S8* data, U16 tmo);
BOOL blTCPIP_Gets(S8 **buf, U16 tmo);
void vTCPIP_FreeRxBuf(S8* ptr);

/***** SMS FUNTIONS ********************************************/
BOOL blSMS_Puts(S8* phone, S8* content);

void GSM_MutexInit(void);
void vLockGSMData(void);
void vUnlockGSMData(void);
uint8_t WDT_Check_GsmTask_Service(void);
int8_t GSM_Mailbox_Puts(int8_t *s);

#endif /* GSM_H_ */
