/**
  ******************************************************************************
  * @file           : health_service.h
  * @brief          : Internal health supervision and watchdog service interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 25.07.2026 07:44:16 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __HEALTH_SERVICE_H
#define __HEALTH_SERVICE_H

#include "main.h"
#include "device_health.h"

/**
  * @brief Bit mask identifying services supervised by the health service.
  */
typedef enum {
  HEALTH_COMPONENT_HEART_BEAT  = (1UL << 0U),
  HEALTH_COMPONENT_TEMPERATURE = (1UL << 1U),
  HEALTH_COMPONENT_TCP         = (1UL << 2U)
} HealthComponent_TypeDef;

/**
  * @brief Create the internal health supervision task.
  */
void HealthService_Init(void);

/**
  * @brief Add a component to the set required by each self-check round.
  * @param component (HealthComponent_TypeDef) Component bit to register.
  */
void HealthService_Register(HealthComponent_TypeDef);

/**
  * @brief Report forward progress by a registered component.
  * @param component (HealthComponent_TypeDef) Component bit to report.
  */
void HealthService_Report(HealthComponent_TypeDef);

/**
  * @brief Latch a system failure and stop reloading the watchdog.
  *
  * The IWDG timeout is nominally 2.4 seconds. The recovery target remains
  * within four seconds after allowing for the uncalibrated LSI tolerance.
  */
void HealthService_LatchFailure(void);

#endif /* __HEALTH_SERVICE_H */
