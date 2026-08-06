/**
  ******************************************************************************
  * @file           : device_health.h
  * @brief          : Shared physical-device availability state.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 27.07.2026 03:11:07 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __DEVICE_HEALTH_H
#define __DEVICE_HEALTH_H

#include <stdint.h>
#include "FreeRTOS.h"

/**
  * @brief Availability states shared by physical devices.
  */
typedef enum {
  DEVICE_HEALTH_INITIALIZING = 0U,
  DEVICE_HEALTH_AVAILABLE,
  DEVICE_HEALTH_DEGRADED,
  DEVICE_HEALTH_UNAVAILABLE,
  DEVICE_HEALTH_STALE,
  DEVICE_HEALTH_MISSING
} DeviceHealthState_TypeDef;

/**
  * @brief Latest availability history maintained by a device-owning service.
  * @param lastAttempt (TickType_t) Tick of the latest availability attempt.
  * @param lastSuccess (TickType_t) Tick of the latest successful operation.
  * @param consecutiveFailures (uint16_t) Failures since the last success.
  * @param lastError (uint8_t) Device-specific error code from the last attempt.
  * @param state (DeviceHealthState_TypeDef) Derived device availability state.
  */
typedef struct {
  TickType_t lastAttempt;
  TickType_t lastSuccess;
  uint16_t consecutiveFailures;
  DeviceHealthState_TypeDef state;
  uint8_t lastError;
} DeviceHealth_TypeDef;

#endif /* __DEVICE_HEALTH_H */
