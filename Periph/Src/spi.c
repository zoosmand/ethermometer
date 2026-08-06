/**
  ******************************************************************************
  * @file           : spi.c
  * @brief          : SPI peripheral implementation.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 24.07.2026 12:54:49 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 

  /* Includes ------------------------------------------------------------------*/
#include "spi.h"

/* Global variables ---------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/


ErrorStatus SPI_Init(SPI_TypeDef* spi) {

  /* Enable GPIO SCK, MISO, MOSI alternative on high speed */

  if (spi == SPI1) {

    MODIFY_REG(SPI1_PORT->CRL,
      ((0xf << (SPI1_SCK_PIN * 4U)) | (0xf << (SPI1_MISO_PIN * 4U)) | (0xf << (SPI1_MOSI_PIN * 4U))), (
        ((GPIO_AF_PP | GPIO_IOS_50) << (SPI1_SCK_PIN * 4U))
      | (GPIO_IN_FL << (SPI1_MISO_PIN * 4U))
      | ((GPIO_AF_PP | GPIO_IOS_50) << (SPI1_MOSI_PIN * 4U))
    ));
    /* Enbale SPI master mode */
    SET_BIT(spi->CR1, SPI_CR1_MSTR);
  }

  switch ((uint32_t)spi) {
  case (uint32_t)SPI1:
    // NVIC_SetPriority(SPI1_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 15, 0));
    // NVIC_EnableIRQ(SPI1_IRQn);
    break;

  default:
    return (ERROR);
    break;
  }

  return (SUCCESS);
}



// ----------------------------------------------------------------------------

ErrorStatus SPI_AdjustConfiguration(SPI_TypeDef* spi) {

  if (spi != SPI1) return (ERROR);

  MODIFY_REG(spi->CR1, (SPI_CR1_BR_Msk | SPI_CR1_DFF_Msk), 0);
  PREG_SET(spi->CR2, SPI_CR2_SSOE_Pos);

  return (SUCCESS);
}



// ----------------------------------------------------------------------------

ErrorStatus SPI_Enable(SPI_TypeDef* spi) {

  uint32_t tmout = SPI_BUS_TIMEOUT;

  while(PREG_CHECK(spi->SR, SPI_SR_BSY_Pos)) {
    if (!(--tmout)) {
      SPI_Disable(spi);
      return (ERROR);
    }
  }

  PREG_SET(spi->CR1, SPI_CR1_SPE_Pos);
  return (SUCCESS);
}




// ----------------------------------------------------------------------------

ErrorStatus SPI_Disable(SPI_TypeDef* spi) {
  uint32_t tmout = SPI_BUS_TIMEOUT;
  
  while(PREG_CHECK(spi->SR, SPI_SR_BSY_Pos)) {
    if (!(--tmout)) {
      return (ERROR);
    }
  }
  
  PREG_CLR(spi->CR1, SPI_CR1_SPE_Pos);
  return (SUCCESS);
}




// ----------------------------------------------------------------------------

ErrorStatus SPI_Read8(SPI_TypeDef* spi, uint8_t *buffer, uint16_t length) {
  uint32_t tmout = 0;
  
  while (length--) {
    *(__IO uint8_t*)&spi->DR = 0;
    
    tmout = SPI_BUS_TIMEOUT;
    while(!(PREG_CHECK(spi->SR, SPI_SR_TXE_Pos))) {
      if (!(--tmout)) return (ERROR);
    }
    
    tmout = SPI_BUS_TIMEOUT;
    while(!(PREG_CHECK(spi->SR, SPI_SR_RXNE_Pos))) {
      if (!(--tmout)) return (ERROR);
    }
    *buffer++ = (uint8_t)spi->DR;
  }
  
  return (SUCCESS);
}



// ----------------------------------------------------------------------------

ErrorStatus SPI_Write8(SPI_TypeDef* spi, uint8_t *buffer, uint16_t length) {
  uint32_t tmout = 0;
  
  while (length--) {
    *(__IO uint8_t*)&spi->DR = *buffer++;
    
    tmout = SPI_BUS_TIMEOUT;
    while(!(PREG_CHECK(spi->SR, SPI_SR_TXE_Pos))) {
      if (!(--tmout)) return (ERROR);
    }
    
    tmout = SPI_BUS_TIMEOUT;
    while(!(PREG_CHECK(spi->SR, SPI_SR_RXNE_Pos))) {
      if (!(--tmout)) return (ERROR);
    }
    (spi->DR);
  }
  
  return (SUCCESS);
}
