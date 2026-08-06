/**
  ******************************************************************************
  * @file           : rtc.h
  * @brief          : Backup-domain RTC counter interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __RTC_H
#define __RTC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief Configure the RTC prescaler for an exact one-second tick.
  *
  * The backup domain (and with it, the RTC prescaler and counter) is reset on
  * every boot in SystemInit, so this must run after every reset. The counter
  * starts at zero; call RTC_SetUnixTime() to give it a real value.
  * @retval (ErrorStatus) SUCCESS once the prescaler write completes.
  */
ErrorStatus RTC_Init(void);

/**
  * @brief Set the RTC counter to a Unix timestamp.
  * @param seconds (uint32_t) Seconds since 1970-01-01T00:00:00Z.
  * @retval (ErrorStatus) SUCCESS once the counter write completes.
  */
ErrorStatus RTC_SetUnixTime(uint32_t seconds);

/**
  * @brief Read the current RTC counter.
  * @retval (uint32_t) Seconds since 1970-01-01T00:00:00Z if the counter has
  *         been set by RTC_SetUnixTime(), otherwise seconds since the last
  *         backup-domain reset.
  */
uint32_t RTC_GetUnixTime(void);

#ifdef __cplusplus
}
#endif

#endif /* __RTC_H */
