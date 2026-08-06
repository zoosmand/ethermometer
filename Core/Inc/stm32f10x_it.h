/**
  ******************************************************************************
  * @file    stm32f1xx_it.h
  * @brief   This file contains the headers of the interrupt handlers.
  ******************************************************************************
  * @attention
  *
 ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F1xx_IT_H
#define __STM32F1xx_IT_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Private includes ----------------------------------------------------------*/
#include "main.h"

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void __attribute__((weak)) SVC_Handler(void);
void DebugMon_Handler(void);
void __attribute__((weak)) PendSV_Handler(void);
void __attribute__((weak)) SysTick_Handler(void);
void RCC_IRQHandler(void);
void USART1_IRQHandler(void);
void TIM7_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F1xx_IT_H */

