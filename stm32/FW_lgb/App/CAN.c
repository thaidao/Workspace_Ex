/*----------------------------------------------------------------------------
 *      RL-ARM - CAN
 *----------------------------------------------------------------------------
 *      Name:    CAN_Ex1.c
 *      Purpose: RTX CAN Driver usage example
 *      Rev.:    V4.20
 *----------------------------------------------------------------------------
 *      This code is part of the RealView Run-Time Library.
 *      Copyright (c) 2004-2011 KEIL - An ARM Company. All rights reserved.
 *---------------------------------------------------------------------------*/

#include "stm32f10x.h"	  			// Standard Peripheral header files
#include "rtl.h"					// Realtime RTX Library header file
#include <RTX_CAN.h>                 /* CAN Generic functions & defines     */
#include "CAN.h"

__task void task_send_CAN (void);
__task void task_rece_CAN (void);

U32 Tx_val = 0, Rx_val = 0;           /* Global variables used for display   */

unsigned char hex_chars[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};	

/*----------------------------------------------------------------------------
 *  Function for converting 1 byte to string in hexadecimal notation
 *---------------------------------------------------------------------------*/

void Hex_Str (unsigned char hex, unsigned char *str) {
  *str++ = '0';
  *str++ = 'x';
  *str++ = hex_chars[hex >>  4];
  *str++ = hex_chars[hex & 0xF];
}

/*----------------------------------------------------------------------------
 *  Task 0: Initializes and starts other tasks
 *---------------------------------------------------------------------------*/

__task void CAN_Module (void)  {
  os_tsk_create (task_send_CAN, 3);   /* Start          transmit task        */
  os_tsk_create (task_rece_CAN, 4);   /* Start           receive task        */

  /* Self delete task */
  os_tsk_delete_self();
}

/*----------------------------------------------------------------------------
 *  Task 1: Sends message with input value in data[0] over CAN periodically
 *---------------------------------------------------------------------------*/

__task void task_send_CAN (void)  {
  /* Initialize message  = { ID, {data[0] .. data[7]}, LEN, CHANNEL, FORMAT, TYPE } */
  CAN_msg msg_send       = { 33, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                              1, 2, STANDARD_FORMAT, DATA_FRAME };

  CAN_init (1, 500000);               /* CAN controller 1 init, 500 kbit/s   */

  CAN_rx_object (1, 2,  33, DATA_TYPE | STANDARD_TYPE); /* Enable reception  */
                                      /* of message on controller 1, channel */
                                      /* is not used for STM32 (can be set to*/
                                      /* whatever value), data frame with    */
                                      /* standard id 33                      */

  /* The activation of test mode in line below is used for enabling
     self-testing mode when CAN controller receives the message it sends so
     program functionality can be tested without another CAN device          */
  /* COMMENT THE LINE BELOW TO ENABLE DEVICE TO PARTICIPATE IN CAN NETWORK   */
  //CAN_hw_testmode(1, CAN_BTR_SILM | CAN_BTR_LBKM); /* Loopback and           */
                                                   /* Silent Mode (self-test)*/

  CAN_start (1);                      /* Start controller 1                  */


  for (;;)  {
    msg_send.data[0] = 100;     	  /* Data[0] field = analog value from   */

    CAN_send (1, &msg_send, 0x0F00);  /* Send msg_send on controller 1       */
    Tx_val = msg_send.data[0];
    os_dly_wait (100);                /* Wait 1 second                       */
  }
}

/*----------------------------------------------------------------------------
 *  Task 2: Received CAN message
 *---------------------------------------------------------------------------*/

__task void task_rece_CAN (void)  {
  CAN_msg msg_rece;

  for (;;)  {
    /* When message arrives store received value to global variable Rx_val   */
    if (CAN_receive (1, &msg_rece, 0x00FF) == CAN_OK)  {
      Rx_val = msg_rece.data[0];
    }
  }
}


/*----------------------------------------------------------------------------
 * end of file
 *---------------------------------------------------------------------------*/
