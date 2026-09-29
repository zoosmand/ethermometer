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
#include "FreeRTOS.h"

/**
  * @brief Initialize the W5500, acquire network configuration, and prepare DNS.
  * @retval (int) Zero on success; a negative value identifies initialization
  *         failure and a positive value indicates that the PHY link is down.
  */
int W5500_Init(void);

/** @brief Report whether link and network configuration are currently usable. */
BaseType_t W5500_IsReady(void);

/**
  * @brief Wait for the first network configuration pass to finish.
  * @param timeout (TickType_t) Maximum ticks to wait.
  * @retval (BaseType_t) pdTRUE once configuration finished, or immediately when
  *         configuration was skipped (PB12 low) or the network never started.
  */
BaseType_t W5500_WaitStartup(TickType_t timeout);

/** @brief Report whether the display is reserved for the just-acquired IP. */
BaseType_t W5500_IsDisplayHeld(void);

/**
  * @brief Copy the currently configured IPv4 DNS server address.
  * @param address (uint8_t*) Destination array containing at least four bytes.
  */
void W5500_GetDnsServer(uint8_t* address);

/** @brief Copy the configured IPv4 NTP server address. */
void W5500_GetNtpServer(uint8_t* address);

#ifdef __cplusplus
}
#endif

#endif /* __WIZCHIP_PORT_H */
