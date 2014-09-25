/**
  ******************************************************************************
  * @file    GPIO/IOToggle/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and peripherals
  *          interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "rtl.h"


/** @addtogroup STM32F10x_StdPeriph_Examples
  * @{
  */

/** @addtogroup GPIO_IOToggle
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  if(CoreDebug->DHCSR & 1){				// check C_DEBUGEN == 1 -> Debugger connected
	  __breakpoint(0);
  }
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**							 _free_box 
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}	

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}	  

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/

/**
 * @fn		void EXTI9_5_IRQHandler(void)
 * @brief	EXTI Interrupt Request Handle
 */
void EXTI9_5_IRQHandler(void){ 
	if(EXTI_GetITStatus(EXTI_Line8) != RESET){


		// clear EXTI line 8 pending bit
		EXTI_ClearITPendingBit(EXTI_Line8);
	}

}

void EXTI0_IRQHandler(void){
static u8 status = 0;

	if(EXTI_GetITStatus(EXTI_Line0) != RESET){
		if(status == 1)
			GPIOD->BSRR = GPIO_Pin_10;
		else
			GPIOD->BRR = GPIO_Pin_10;

		status = 1 - status;
		// clear int flag	
		EXTI_ClearITPendingBit(EXTI_Line0);
	};
}

/* UART1 Interrupt handle */
void USART1_IRQHandler(void){
uint16_t UART_Data;

	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {			
		UART_Data = USART_ReceiveData(USART1);
		USART_SendData(USART1, UART_Data);	

	};
}

/* USART2 Handler */
void USART2_IRQHandler(void){
uint16_t data;

	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET){
		data = USART_ReceiveData(USART2);
		USART_SendData(USART1, (uint8_t)data);
	
		//USART_SendData(USART2, (uint8_t)data);
		//USART_SendData(USART3, (uint8_t)data);
	};
}


/* USART3 Handler */
void USART3_IRQHandler(void){
uint16_t data;

	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET){
		data = USART_ReceiveData(USART3);

	};
}


/**
  * @brief  This function handles PPP interrupt request.
  * @param  None
  * @retval None
  */	  
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */

/**
  * @}
  */

/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
