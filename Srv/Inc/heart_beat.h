/**
  ******************************************************************************
  * @file           : heart_beat.h
  * @brief          : Heartbeat LED service interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 22.09.2025 02:40:44 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 
  /* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __HEART_BEAT_H
#define __HEART_BEAT_H

#ifdef __cplusplus
  extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"


#define HEARTBEAT_PORT   GPIOC
#define HEARTBEAT_PIN    GPIO_PIN_13


/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief Create the task that displays system activity on the heartbeat LED.
  */
void HeartBeatService_Init(void);


#ifdef __cplusplus
}
#endif

#endif /* __HEART_BEAT_H */
