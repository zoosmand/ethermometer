/**
  ******************************************************************************
  * @file           : buzzer.h
  * @brief          : Passive buzzer PWM interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 27.07.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "misc.h"
#include "FreeRTOS.h"

#define BUZZER_SELF_TEST_FREQUENCY_HZ  2000U
#define BUZZER_SELF_TEST_DURATION_MS    200U
#define BUZZER_MIN_FREQUENCY_HZ           16U
#define BUZZER_MAX_FREQUENCY_HZ        20000U

/**
  * @brief Configure PA8 and TIM1 channel 1 for passive-buzzer PWM.
  * @retval (ErrorStatus) SUCCESS when the output is initialized and silent.
  */
ErrorStatus Buzzer_Init(void);

/**
  * @brief Start a square-wave tone on the buzzer.
  * @param frequencyHz (uint32_t) Tone frequency in hertz.
  * @retval (ErrorStatus) SUCCESS when the requested frequency is valid.
  */
ErrorStatus Buzzer_Start(uint32_t frequencyHz);

/**
  * @brief Stop the tone and leave the 2N2222 transistor switched off.
  */
void Buzzer_Stop(void);

/**
  * @brief Emit the short audible startup confirmation tone.
  * @retval (ErrorStatus) SUCCESS when the test tone was generated.
  *
  * This is an operational output test: it verifies the configured timer path,
  * but it cannot electrically confirm that the buzzer produced sound.
  */
ErrorStatus Buzzer_SelfTest(void);

/**
  * @brief Emit the self-test tone after waiting at most the supplied interval.
  * @param timeout (TickType_t) Maximum ticks to wait for buzzer ownership.
  * @retval (ErrorStatus) ERROR when the buzzer is busy or the tone cannot start.
  */
ErrorStatus Buzzer_SelfTestWithTimeout(TickType_t timeout);

/** @brief Acquire exclusive ownership for a complete buzzer pattern. */
BaseType_t Buzzer_Lock(TickType_t timeout);

/** @brief Release exclusive buzzer ownership. */
void Buzzer_Unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
