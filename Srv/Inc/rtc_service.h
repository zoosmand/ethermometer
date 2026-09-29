/**
  ******************************************************************************
  * @file           : rtc_service.h
  * @brief          : RTC reporting and periodic NTP synchronization interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __RTC_SERVICE_H
#define __RTC_SERVICE_H

#include "main.h"

/**
  * @brief Latest RTC reading and NTP synchronization state.
  * @param unixTime (uint32_t) Current RTC reading, seconds since epoch.
  * @param synchronized (BaseType_t) Whether an NTP sync has ever succeeded.
  * @param lastSyncAgeMs (uint32_t) Milliseconds since the last successful
  *        sync; only meaningful when synchronized is pdTRUE.
  * @param consecutiveFailures (uint16_t) Sync attempts failed since the last
  *        success.
  */
typedef struct {
  uint32_t unixTime;
  BaseType_t synchronized;
  uint32_t lastSyncAgeMs;
  uint16_t consecutiveFailures;
} RtcStatus_TypeDef;

/**
  * @brief Configure the RTC and create the periodic report/sync task.
  *
  * The task waits for Ethernet address configuration, then attempts an NTP
  * sync against pool.ntp.org. It retries until successful, synchronizes once
  * per hour thereafter, and prints the current RTC reading once per minute.
  * NTP synchronization is disabled when PB12 skips network initialization.
  */
void RtcService_Init(void);

/**
  * @brief Copy the latest RTC reading and synchronization state.
  * @param status (RtcStatus_TypeDef*) Destination for the copied state.
  */
void RtcService_GetStatus(RtcStatus_TypeDef* status);

/**
  * @brief Format a Unix timestamp as an ISO 8601 UTC string.
  * @param unixTime (uint32_t) Seconds since 1970-01-01T00:00:00Z.
  * @param buffer (char*) Destination buffer.
  * @param capacity (size_t) Size of buffer in bytes; at least 21 is enough
  *        for "YYYY-MM-DDTHH:MM:SSZ" plus the terminator.
  */
void RtcService_FormatIso8601(uint32_t unixTime, char* buffer, size_t capacity);

#endif /* __RTC_SERVICE_H */
