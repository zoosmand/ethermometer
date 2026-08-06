/**
  ******************************************************************************
  * @file           : gpio.c
  * @brief          : GPIO initialization implementation.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 22.09.2025 02:40:44 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 

  /* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* Global variables ---------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/


ErrorStatus LED_Init(GPIO_TypeDef *port, uint16_t pin) {
    uint32_t shift;
    #define GPIO_MODE_OUTPUT_PP_2MHZ (GPIO_IOS_2 | GPIO_GPO_PP)

    if (pin < 8) {
        shift = pin * 4;
        MODIFY_REG(port->CRL, (0xf << shift), (GPIO_MODE_OUTPUT_PP_2MHZ << shift));
    } else {
        shift = (pin - 8) * 4;
        MODIFY_REG(port->CRH, (0xf << shift), (GPIO_MODE_OUTPUT_PP_2MHZ << shift));
    }
    PIN_H(port, pin);

    return (SUCCESS);
}


ErrorStatus OneWire_Init(GPIO_TypeDef *port, uint16_t pin) {
    uint32_t shift;
    #define GPIO_MODE_OUTPUT_OD_10MHZ (GPIO_IOS_10 | GPIO_GPO_OD)

    if (pin < 8) {
        shift = pin * 4;
        MODIFY_REG(port->CRL, (0xf << shift), (GPIO_MODE_OUTPUT_OD_10MHZ << shift));
    } else {
        shift = (pin - 8) * 4;
        MODIFY_REG(port->CRH, (0xf << shift), (GPIO_MODE_OUTPUT_OD_10MHZ << shift));
    }
    PIN_H(port, pin);

    return (SUCCESS);
}



ErrorStatus EthernetSPI_Init(GPIO_TypeDef *port, uint16_t pin) {
    uint32_t shift;
    #define SPI_ETH_PP_50MHZ (GPIO_IOS_50 | GPIO_GPO_PP)

    if (pin < 8) {
        shift = pin * 4;
        MODIFY_REG(port->CRL, (0xf << shift), (SPI_ETH_PP_50MHZ << shift));
    } else {
        shift = (pin - 8) * 4;
        MODIFY_REG(port->CRH, (0xf << shift), (SPI_ETH_PP_50MHZ << shift));
    }
    PIN_H(port, pin);

    return (SUCCESS);
}

