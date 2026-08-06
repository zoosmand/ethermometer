/**
  ******************************************************************************
  * @file           : usart.c
  * @brief          : USART peripheral implementation.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 22.09.2025 02:40:44 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 

  /* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* Global variables ---------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/


ErrorStatus USART_Init(USART_TypeDef* USARTx) {

  if (USARTx == USART1) {

    MODIFY_REG(GPIOA->CRH, (GPIO_PIN_9_Mask | GPIO_PIN_10_Mask), (((GPIO_IOS_50 | GPIO_AF_PP) << (GPIO_PIN_9 - 8) * 4) | (GPIO_IN_FL << (GPIO_PIN_10 - 8) * 4)));

    #define USART1_BAUDRATE         115200
    #define USART1_FRACTION         0x0001 /* If baudrate = 9600 -> Fractual = 0x0120 */
    #define USART1_BAUDRATE_CALC    ((configCPU_CLOCK_HZ / (USART1_BAUDRATE * 16)) << 4) | USART1_FRACTION

    USART1->BRR = USART1_BAUDRATE_CALC;
    MODIFY_REG(USART1->CR1, ~(USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE), (USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE));
    
    /* USART1_IRQn interrupt configuration */
    NVIC_SetPriority(USART1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 5, 0));
    NVIC_EnableIRQ(USART1_IRQn);

    return (SUCCESS);
  }

  return (ERROR);
}
