/**
  ******************************************************************************
  * @file           : threshold_service.h
  * @brief          : Temperature alarm threshold registry interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __THRESHOLD_SERVICE_H
#define __THRESHOLD_SERVICE_H

#include "main.h"

#define THRESHOLD_SERVICE_MAX_THRESHOLDS 3U

/**
  * @brief One temperature trip point that sounds the buzzer when reached.
  * @param enabled (BaseType_t) Whether this slot is active.
  * @param sensorIndex (uint8_t) One-based physical sensor number to watch.
  * @param temperature (int32_t) Trip point in hundredths of a degree Celsius.
  * @param toneHz (uint32_t) Buzzer tone used for this slot's beep pattern.
  * @param triggered (BaseType_t) Whether the threshold is currently exceeded.
  */
typedef struct {
  BaseType_t enabled;
  uint8_t sensorIndex;
  int32_t temperature;
  uint32_t toneHz;
  BaseType_t triggered;
} Threshold_TypeDef;

/**
  * @brief Create the mutex and activate the default alarm thresholds.
  *
  * All three slots start enabled, watching physical sensor 1: slot 1 at
  * 40 C (1400 Hz), slot 2 at 70 C (2100 Hz), and slot 3 at 100 C (2600 Hz).
  */
void ThresholdService_Init(void);

/**
  * @brief Copy one threshold slot.
  * @param index (uint8_t) One-based slot number, 1..THRESHOLD_SERVICE_MAX_THRESHOLDS.
  * @param threshold (Threshold_TypeDef*) Destination for the copied slot.
  * @retval (ErrorStatus) SUCCESS when the index is in range.
  */
ErrorStatus ThresholdService_Get(uint8_t index, Threshold_TypeDef* threshold);

/**
  * @brief Configure one threshold slot.
  * @param index (uint8_t) One-based slot number, 1..THRESHOLD_SERVICE_MAX_THRESHOLDS.
  * @param sensorIndex (uint8_t) One-based physical sensor number to watch.
  * @param temperature (int32_t) Trip point in hundredths of a degree Celsius.
  * @param toneHz (uint32_t) Buzzer tone in hertz; must be within the buzzer's
  *        supported range.
  * @param enabled (BaseType_t) Whether the slot becomes active immediately.
  * @param result (Threshold_TypeDef*) Optional destination for the stored slot.
  * @retval (ErrorStatus) SUCCESS when the index and tone are valid.
  */
ErrorStatus ThresholdService_Set(
  uint8_t index,
  uint8_t sensorIndex,
  int32_t temperature,
  uint32_t toneHz,
  BaseType_t enabled,
  Threshold_TypeDef* result
);

/**
  * @brief Compare every enabled threshold against its sensor's latest
  *        snapshot and sound its beep pattern when newly exceeded.
  *
  * Intended to be called once per temperature measurement cycle. Each slot
  * has a fixed beep pattern sounded at its configured tone: slot 1 beeps 3
  * times once when crossed, slot 2 beeps 5 times once when crossed, and
  * slot 3 beeps 10 times when crossed and again every 10 seconds for as
  * long as it remains exceeded. A slot re-arms (may beep again) only after
  * its temperature drops back below the trip point and reaches it again.
  * Only one tone can play at a time; if more than one slot is due to beep
  * in the same cycle, the most severe (highest-indexed) one wins and the
  * others are still marked as acknowledged so they do not queue up.
  */
void ThresholdService_Evaluate(void);

#endif /* __THRESHOLD_SERVICE_H */
