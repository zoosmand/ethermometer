/**
  ******************************************************************************
  * @file           : whxxxx.h
  * @brief          : WHxxxx character display interface, 4-bit parallel mode.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 24.07.2026 04:51:32 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __WHXXXX_H
#define __WHXXXX_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/*
 * 4-bit parallel HD44780-compatible interface. RW is tied to ground on the
 * board (write-only operation, timed with fixed delays instead of busy-flag
 * polling), so only RS, E, and the D4..D7 data lines are driven by the MCU.
 */
#define WHXXXX_RS_PORT GPIOB
#define WHXXXX_RS_PIN  GPIO_PIN_0
#define WHXXXX_EN_PORT GPIOB
#define WHXXXX_EN_PIN  GPIO_PIN_1
#define WHXXXX_D4_PORT GPIOB
#define WHXXXX_D4_PIN  GPIO_PIN_5
#define WHXXXX_D5_PORT GPIOB
#define WHXXXX_D5_PIN  GPIO_PIN_6
#define WHXXXX_D6_PORT GPIOB
#define WHXXXX_D6_PIN  GPIO_PIN_7
#define WHXXXX_D7_PORT GPIOB
#define WHXXXX_D7_PIN  GPIO_PIN_8

ErrorStatus WHxxxx_Init(void);
ErrorStatus WHxxxx_Clear(void);
ErrorStatus WHxxxx_Print(const uint8_t*, uint16_t);
int WHxxxx_PutChar(char);

#ifdef __cplusplus
}
#endif

#endif /* __WHXXXX_H */
