/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Main application declarations and module integration.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 20.09.2025 08:34:08 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <semphr.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
// #include <unistd.h>

#include "misc.h"
#include "stm32f103xb.h"

/* Private includes ----------------------------------------------------------*/
#include "common.h"
#include "stm32f10x_it.h"
#include "project_config.h"

#include "gpio.h"
#include "buzzer.h"
#include "usart.h"
#include "onewire.h"
#include "spi.h"
#include "whxxxx.h"

#include "heart_beat.h"
#include "health_service.h"
#include "temperature_service.h"
#include "threshold_service.h"
#include "api_service.h"

#include "wizchip_port.h"


/* Exported constants --------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern __IO uint32_t peripheralReadiness;

/* Private defines -----------------------------------------------------------*/
/* Peripherals readiness flags */
#define PERIPHERAL_HEARTBEAT_LED_ERROR_BIT 0
#define PERIPHERAL_USART1_ERROR_BIT        1
#define PERIPHERAL_ONEWIRE_ERROR_BIT       2
#define PERIPHERAL_SPI1_ERROR_BIT          3
#define PERIPHERAL_WH_DISPLAY_ERROR_BIT    4
#define PERIPHERAL_BUZZER_ERROR_BIT        5

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
