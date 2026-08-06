/**
  ******************************************************************************
  * @file           : ds18b20.h
  * @brief          : DS18B20 temperature sensor interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 22.09.2025 02:40:44 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 
  /* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DS18B20_H
#define __DS18B20_H

#ifdef __cplusplus
  extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"



/**
  * @brief Result of communicating with one DS18B20 sensor.
  */
typedef enum {
  DS18B20_STATUS_OK = 0U,
  DS18B20_STATUS_BUS,
  DS18B20_STATUS_TIMEOUT,
  DS18B20_STATUS_CRC
} DS18B20_Status_TypeDef;

/**
  * @brief Identity, temperature, and communication status for one DS18B20.
  * @param rom (uint8_t[8]) Unique one-wire ROM code.
  * @param temperature (int16_t) Temperature in hundredths of a degree Celsius.
  * @param status (DS18B20_Status_TypeDef) Per-device measurement status.
  */
typedef struct {
  uint8_t rom[8];
  int16_t temperature;
  DS18B20_Status_TypeDef status;
} DS18B20_Measurement_TypeDef;

/**
  * @brief Converts and reads every discovered DS18B20 independently.
  * @param measurements (DS18B20_Measurement_TypeDef*) Per-device output array.
  * @param capacity (uint8_t) Number of elements available in measurements.
  * @param count (uint8_t*) Number of discovered devices written.
  * @retval (ErrorStatus) SUCCESS when at least one device was reported.
  */
ErrorStatus DS18B20_Measure(
  DS18B20_Measurement_TypeDef*,
  uint8_t,
  uint8_t*
);


/* Private defines -----------------------------------------------------------*/
#define DS18B20_COMMAND_ALARM_SEARCH     0xec
#define DS18B20_COMMAND_CONVERT_T        0x44
#define DS18B20_COMMAND_WRITE_SCRATCHPAD 0x4e
#define DS18B20_COMMAND_READ_SCRATCHPAD  0xbe
#define DS18B20_COMMAND_COPY_SCRATCHPAD  0x48
#define DS18B20_COMMAND_RECALL_EEPROM     0xb8
#define DS18B20_COMMAND_READ_POWER_SUPPLY 0xb4



#ifdef __cplusplus
}
#endif

#endif /* __DS18B20_H */
