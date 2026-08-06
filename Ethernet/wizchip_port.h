/**
  ******************************************************************************
  * @file           : wizchip_port.h
  * @brief          : W5500 board integration and network configuration interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 27.10.2025
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __WIZCHIP_PORT_H
#define __WIZCHIP_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
  * @brief Initialize the W5500, acquire network configuration, and prepare DNS.
  * @retval (int) Zero on success; a negative value identifies initialization
  *         failure and a positive value indicates that the PHY link is down.
  */
int W5500_Init(void);

/**
  * @brief Copy the currently configured IPv4 DNS server address.
  * @param address (uint8_t*) Destination array containing at least four bytes.
  */
void W5500_GetDnsServer(uint8_t* address);

#ifdef __cplusplus
}
#endif

#endif /* __WIZCHIP_PORT_H */
