/**
  ******************************************************************************
  * @file           : usart.h
  * @brief          : USART peripheral interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 22.09.2025 02:40:44 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 
  /* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_INIT_H
#define __USART_INIT_H

#ifdef __cplusplus
  extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"


/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initializes the corresponding USART peripheral. 
  *         The pin is configured as:
  *           - output, push-pull
  *           - low speed (2 MHz)
  *           - no pull-up and no pull-down
  * @param  port: pointer to the USART port instance
  * @retval (int) Status of operation (0 = success)
  */
ErrorStatus USART_Init(USART_TypeDef*);


#ifdef __cplusplus
}
#endif

#endif /* __USART_INIT_H */