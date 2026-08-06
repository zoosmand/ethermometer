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

static Threshold_TypeDef thresholds[THRESHOLD_SERVICE_MAX_THRESHOLDS];
static SemaphoreHandle_t thresholdsMutex;
static StaticSemaphore_t thresholdsMutexBuffer;




// -------------------------------------------------------------
void ThresholdService_Init(void) {
  memset(thresholds, 0, sizeof(thresholds));
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
  if (result != NULL) *result = *slot;
  (void)xSemaphoreGive(thresholdsMutex);
  return (SUCCESS);
}




// -------------------------------------------------------------
void ThresholdService_Evaluate(void) {
  BaseType_t soundTone = pdFALSE;
  uint32_t toneHz = 0U;

  (void)xSemaphoreTake(thresholdsMutex, portMAX_DELAY);
  for (uint8_t i = 0U; i < THRESHOLD_SERVICE_MAX_THRESHOLDS; i++) {
    Threshold_TypeDef* slot = &thresholds[i];
    if (slot->enabled != pdTRUE) continue;

    SensorSnapshot_TypeDef snapshot;
    BaseType_t exceeded = pdFALSE;
    if ((TemperatureSensorService_GetSnapshot(slot->sensorIndex, &snapshot) == SUCCESS)
        && (snapshot.dataValid == pdTRUE)
        && ((snapshot.capabilities & SENSOR_CAPABILITY_TEMPERATURE) != 0U)
        && (snapshot.temperature >= slot->temperature)) {
      exceeded = pdTRUE;
    }
    slot->triggered = exceeded;

    if ((exceeded == pdTRUE) && (soundTone != pdTRUE)) {
      soundTone = pdTRUE;
      toneHz = slot->toneHz;
    }
  }
  (void)xSemaphoreGive(thresholdsMutex);

  if (soundTone == pdTRUE) {
    (void)Buzzer_Start(toneHz);
  } else {
    Buzzer_Stop();
  }
}
