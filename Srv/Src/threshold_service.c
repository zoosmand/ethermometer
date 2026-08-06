/**
  ******************************************************************************
  * @file           : threshold_service.c
  * @brief          : Temperature alarm threshold registry implementation.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "threshold_service.h"
#include "temperature_service.h"
#include "buzzer.h"
#include <string.h>

#define THRESHOLD_BEEP_DURATION_MS 120U
#define THRESHOLD_BEEP_GAP_MS      120U

/* Beep count and repeat interval are fixed per slot, not user-configurable:
 * slot 1 (mildest) beeps once when crossed, slot 2 beeps more insistently,
 * and slot 3 (most severe) repeats for as long as it stays exceeded. */
static const uint8_t thresholdBeepCounts[THRESHOLD_SERVICE_MAX_THRESHOLDS] = {3U, 5U, 10U};
static const uint32_t thresholdRepeatMs[THRESHOLD_SERVICE_MAX_THRESHOLDS] = {0U, 0U, 10000U};

/**
  * @brief Per-slot alarm bookkeeping, not exposed through the public API.
  * @param latched (BaseType_t) Whether this slot's beep pattern has already
  *        fired for the current exceedance; cleared once the temperature
  *        drops back below the trip point.
  * @param lastAlarmTick (TickType_t) Tick count of the last time this slot
  *        actually sounded; used to gate slot 3's repeat interval.
  */
typedef struct {
  BaseType_t latched;
  TickType_t lastAlarmTick;
} ThresholdAlarmState_TypeDef;

static Threshold_TypeDef thresholds[THRESHOLD_SERVICE_MAX_THRESHOLDS];
static ThresholdAlarmState_TypeDef alarmState[THRESHOLD_SERVICE_MAX_THRESHOLDS];
static SemaphoreHandle_t thresholdsMutex;
static StaticSemaphore_t thresholdsMutexBuffer;

static void thresholdService_PlayBeeps(uint32_t, uint8_t);




// -------------------------------------------------------------
void ThresholdService_Init(void) {
  memset(thresholds, 0, sizeof(thresholds));
  memset(alarmState, 0, sizeof(alarmState));

  thresholds[0].enabled = pdTRUE;
  thresholds[0].sensorIndex = 1U;
  thresholds[0].temperature = 4000;  /* 40.00 C */
  thresholds[0].toneHz = 1400U;

  thresholds[1].enabled = pdTRUE;
  thresholds[1].sensorIndex = 1U;
  thresholds[1].temperature = 7000;  /* 70.00 C */
  thresholds[1].toneHz = 2100U;

  thresholds[2].enabled = pdTRUE;
  thresholds[2].sensorIndex = 1U;
  thresholds[2].temperature = 10000; /* 100.00 C */
  thresholds[2].toneHz = 2600U;

  thresholdsMutex = xSemaphoreCreateMutexStatic(&thresholdsMutexBuffer);
}




// -------------------------------------------------------------
ErrorStatus ThresholdService_Get(uint8_t index, Threshold_TypeDef* threshold) {
  if ((index == 0U)
      || (index > THRESHOLD_SERVICE_MAX_THRESHOLDS)
      || (threshold == NULL)) {
    return (ERROR);
  }

  (void)xSemaphoreTake(thresholdsMutex, portMAX_DELAY);
  *threshold = thresholds[index - 1U];
  (void)xSemaphoreGive(thresholdsMutex);
  return (SUCCESS);
}




// -------------------------------------------------------------
ErrorStatus ThresholdService_Set(
  uint8_t index,
  uint8_t sensorIndex,
  int32_t temperature,
  uint32_t toneHz,
  BaseType_t enabled,
  Threshold_TypeDef* result
) {
  if ((index == 0U) || (index > THRESHOLD_SERVICE_MAX_THRESHOLDS)) return (ERROR);
  if ((toneHz < BUZZER_MIN_FREQUENCY_HZ) || (toneHz > BUZZER_MAX_FREQUENCY_HZ)) {
    return (ERROR);
  }

  (void)xSemaphoreTake(thresholdsMutex, portMAX_DELAY);
  Threshold_TypeDef* slot = &thresholds[index - 1U];
  slot->sensorIndex = sensorIndex;
  slot->temperature = temperature;
  slot->toneHz = toneHz;
  slot->enabled = enabled;
  if (enabled != pdTRUE) slot->triggered = pdFALSE;
  /* Reconfiguring re-arms the alarm so the new settings apply cleanly. */
  alarmState[index - 1U].latched = pdFALSE;
  if (result != NULL) *result = *slot;
  (void)xSemaphoreGive(thresholdsMutex);
  return (SUCCESS);
}




// -------------------------------------------------------------
void ThresholdService_Evaluate(void) {
  TickType_t now = xTaskGetTickCount();
  int8_t fireIndex = -1;
  uint32_t fireToneHz = 0U;
  uint8_t fireBeepCount = 0U;

  (void)xSemaphoreTake(thresholdsMutex, portMAX_DELAY);
  for (uint8_t i = 0U; i < THRESHOLD_SERVICE_MAX_THRESHOLDS; i++) {
    Threshold_TypeDef* slot = &thresholds[i];
    BaseType_t exceeded = pdFALSE;

    if (slot->enabled == pdTRUE) {
      SensorSnapshot_TypeDef snapshot;
      if ((TemperatureSensorService_GetSnapshot(slot->sensorIndex, &snapshot) == SUCCESS)
          && (snapshot.dataValid == pdTRUE)
          && ((snapshot.capabilities & SENSOR_CAPABILITY_TEMPERATURE) != 0U)
          && (snapshot.temperature >= slot->temperature)) {
        exceeded = pdTRUE;
      }
    }
    slot->triggered = exceeded;

    if (exceeded != pdTRUE) {
      alarmState[i].latched = pdFALSE;
      continue;
    }

    BaseType_t dueToFire;
    if (alarmState[i].latched != pdTRUE) {
      dueToFire = pdTRUE; /* rising edge: always beep once */
    } else if (thresholdRepeatMs[i] != 0U) {
      dueToFire = ((now - alarmState[i].lastAlarmTick) >= pdMS_TO_TICKS(thresholdRepeatMs[i]))
        ? pdTRUE
        : pdFALSE;
    } else {
      dueToFire = pdFALSE; /* one-shot slot already fired for this exceedance */
    }

    if (dueToFire == pdTRUE) {
      alarmState[i].latched = pdTRUE;
      alarmState[i].lastAlarmTick = now;
      /* Later slots are more severe; let the last one due this cycle win. */
      fireIndex = (int8_t)i;
      fireToneHz = slot->toneHz;
      fireBeepCount = thresholdBeepCounts[i];
    }
  }
  (void)xSemaphoreGive(thresholdsMutex);

  if (fireIndex >= 0) {
    thresholdService_PlayBeeps(fireToneHz, fireBeepCount);
  }
}




// -------------------------------------------------------------
static void thresholdService_PlayBeeps(uint32_t toneHz, uint8_t count) {
  for (uint8_t i = 0U; i < count; i++) {
    if (i > 0U) vTaskDelay(pdMS_TO_TICKS(THRESHOLD_BEEP_GAP_MS));
    (void)Buzzer_Start(toneHz);
    vTaskDelay(pdMS_TO_TICKS(THRESHOLD_BEEP_DURATION_MS));
    Buzzer_Stop();
  }
}
