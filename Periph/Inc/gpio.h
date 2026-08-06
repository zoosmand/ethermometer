/**
  ******************************************************************************
  * @file           : gpio.h
  * @brief          : GPIO initialization interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 22.09.2025 02:40:44 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 
  /* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __GPIO_INIT_H
#define __GPIO_INIT_H

#ifdef __cplusplus
  extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"


/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initializes the corresponding LED pin on the board. 
  *         The pin is configured as:
  *           - output
  *           - low speed (2 MHz)
  *           - push-pull
  * @param  port: pointer to the GPIO port instance
  * @param  pin:  pin number (0..15)
  * @retval (int) Status of operation (0 = success)
  */
ErrorStatus LED_Init(GPIO_TypeDef*, uint16_t);


/**
  * @brief  Initializes the corresponding OneWire bus pin on the board. 
  *         The pin is configured as:
  *           - output
  *           - low speed (10 MHz)
  *           - open-drain
  * @param  port: pointer to the GPIO port instance
  * @param  pin:  pin number (0..15)
  * @retval (int) Status of operation (0 = success)
  */
ErrorStatus OneWire_Init(GPIO_TypeDef*, uint16_t);


/**
  * @brief  Initializes the corresponding Ethernet pins on the board. 
  *         The pin is configured as:
  *           - output
  *           - low speed (10 MHz)
  *           - open-drain
  * @param  port: pointer to the GPIO port instance
  * @param  pin:  pin number (0..15)
  * @retval (int) Status of operation (0 = success)
  */
ErrorStatus EthernetSPI_Init(GPIO_TypeDef*, uint16_t);


#ifdef __cplusplus
}
#endif

#endif /* __GPIO_INIT_H */