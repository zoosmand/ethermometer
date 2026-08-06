/**
  ******************************************************************************
  * @file           : temperature_service.h
  * @brief          : Periodic environmental sensor service interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 24.07.2026 10:05:16 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __TEMPERATURE_SERVICE_H
#define __TEMPERATURE_SERVICE_H

#include "main.h"
#include "device_health.h"

#define SENSOR_SERVICE_MAX_DS18B20 6U
#define SENSOR_SERVICE_MAX_DEVICES (SENSOR_SERVICE_MAX_DS18B20 + 2U)

/**
  * @brief Models supported by the periodic sensor service.
  */
typedef enum {
  SENSOR_MODEL_DS18B20 = 0U
} SensorModel_TypeDef;

/**
  * @brief Measurement capabilities exposed by a sensor.
  */
typedef enum {
  SENSOR_CAPABILITY_TEMPERATURE = (1U << 0U),
  SENSOR_CAPABILITY_PRESSURE    = (1U << 1U),
  SENSOR_CAPABILITY_HUMIDITY    = (1U << 2U)
} SensorCapability_TypeDef;

/**
  * @brief Errors recorded for the latest unsuccessful measurement.
  */
typedef enum {
  SENSOR_ERROR_NONE = 0U,
  SENSOR_ERROR_NOT_READY,
  SENSOR_ERROR_TIMEOUT,
  SENSOR_ERROR_CRC,
  SENSOR_ERROR_BUS,
  SENSOR_ERROR_MISSING,
  SENSOR_ERROR_CONVERSION
} SensorError_TypeDef;

/**
  * @brief Cached identity, measurements, and health for one physical sensor.
  * @param model (SensorModel_TypeDef) Detected sensor model.
  * @param capabilities (uint8_t) Bit mask of SensorCapability_TypeDef values.
  * @param identity (uint8_t[8]) DS18B20 ROM code; zero for Bosch sensors.
  * @param serialNumber (uint32_t) Bosch unique ID; zero for DS18B20 sensors.
  * @param dataValid (BaseType_t) Whether cached measurements may be returned.
  * @param temperature (int32_t) Temperature in hundredths of a degree Celsius.
  * @param pressure (uint32_t) Pressure in pascals.
  * @param humidity (uint32_t) Relative humidity in thousandths of percent.
  * @param health (DeviceHealth_TypeDef) Latest availability history.
  */
typedef struct {
  SensorModel_TypeDef model;
  uint8_t capabilities;
  uint8_t identity[8];
  uint32_t serialNumber;
  BaseType_t dataValid;
  int32_t temperature;
  uint32_t pressure;
  uint32_t humidity;
  DeviceHealth_TypeDef health;
} SensorSnapshot_TypeDef;

/**
  * @brief Number of registered sensors grouped by measurement capability.
  * @param temperature (uint8_t) Sensors that measure temperature.
  * @param pressure (uint8_t) Sensors that measure pressure.
  * @param humidity (uint8_t) Sensors that measure relative humidity.
  * @param all (uint8_t) All registered physical sensors.
  */
typedef struct {
  uint8_t temperature;
  uint8_t pressure;
  uint8_t humidity;
  uint8_t all;
} SensorCounts_TypeDef;

/**
  * @brief Detect supported sensors and create the periodic measurement task.
  */
void TemperatureSensorService_Init(void);

/**
  * @brief Return counts of registered sensors by capability.
  * @param counts (SensorCounts_TypeDef*) Destination for the counts.
  */
void TemperatureSensorService_GetCounts(SensorCounts_TypeDef*);

/**
  * @brief Copy the cached snapshot for a physical sensor number.
  * @param sensorNumber (uint8_t) One-based physical sensor number.
  * @param snapshot (SensorSnapshot_TypeDef*) Destination for the snapshot.
  * @retval (ErrorStatus) SUCCESS when the sensor number is registered.
  */
ErrorStatus TemperatureSensorService_GetSnapshot(
  uint8_t,
  SensorSnapshot_TypeDef*
);

#endif /* __TEMPERATURE_SERVICE_H */
