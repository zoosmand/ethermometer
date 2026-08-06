/**
  ******************************************************************************
  * @file           : spi.h
  * @brief          : SPI peripheral interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 24.07.2026 12:54:49 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 
  /* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPI_INIT_H
#define __SPI_INIT_H

#ifdef __cplusplus
  extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"


#define SPI1_SCK_PIN          GPIO_PIN_5
#define SPI1_MISO_PIN         GPIO_PIN_6
#define SPI1_MOSI_PIN         GPIO_PIN_7
#define SPI1_PORT             GPIOA

#define ETH_CS_PORT           GPIOA
#define ETH_CS_PIN            GPIO_PIN_4
#define ETH_RST_PORT          GPIOA
#define ETH_RST_PIN           GPIO_PIN_3
#define ETH_INT_PORT          GPIOA
#define ETH_INT_PIN           GPIO_PIN_1

#define SPI_BUS_TIMEOUT         10000 /* cycles timeout on SPI bus operations */



/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initializes the corresponding SPI peripheral. 
  *         The pin is configured as:
  *           - output, push-pull
  *           - low speed (2 MHz)
  *           - no pull-up and no pull-down
  * @param  port: pointer to the SPI port instance
  * @retval (int) Status of operation (0 = success)
  */
ErrorStatus SPI_Init(SPI_TypeDef*);

ErrorStatus SPI_AdjustConfiguration(SPI_TypeDef*);

/**
 * @brief  Enables the given SPI peripherals.
 * @param  spi: pointer to the given SPI peripherals
 * @retval status of operation
 */
ErrorStatus SPI_Enable(SPI_TypeDef*);

/**
 * @brief  Disables the given SPI peripherals.
 * @param  spi: pointer to the given SPI peripherals
 * @retval status of operation
 */
ErrorStatus SPI_Disable(SPI_TypeDef*);

/**
  * @brief  Reads data from SPI bus with 8-bit data buffer size.
  * @param  spi: pointer to the dedicatred SPI bus structure
  * @param  buffer: pointer to the buffer to store the read data
  * @param  length: count of bytes to read from the bus
  * @retval status of aperation
  */
ErrorStatus SPI_Read8(SPI_TypeDef*, uint8_t*, uint16_t);

/**
  * @brief  Writes data to the SPI bus with 8-bit data buffer size.
  * @param  spi: pointer to the dedicatred SPI bus structure
  * @param  buffer: pointer to the buffer of transmitting data
  * @param  length: count of bytes to write to the bus
  * @retval status of aperation
  */
ErrorStatus SPI_Write8(SPI_TypeDef*, uint8_t*, uint16_t);



#ifdef __cplusplus
}
#endif

#endif /* __SPI_INIT_H */
