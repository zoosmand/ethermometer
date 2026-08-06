/**
  ******************************************************************************
  * @file           : onewire.h
  * @brief          : One-wire bus and device interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 23.09.2025 04:55:12 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 
  /* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ONEWIRE_H
#define __ONEWIRE_H

#ifdef __cplusplus
  extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"


/**
  * @brief Identity and scratchpad data cached for one one-wire device.
  * @param rom (uint8_t[8]) Unique one-wire ROM code.
  * @param scratchpad (uint8_t[9]) Device scratchpad including its CRC byte.
  */
typedef struct {
  uint8_t rom[8];
  uint8_t scratchpad[9];
} OneWireDevice_TypeDef;


/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief Reset the one-wire bus and detect a presence pulse.
  * @retval (ErrorStatus) SUCCESS when at least one device responds.
  */
ErrorStatus OneWire_Reset(void);

/**
  * @brief Write one byte to the one-wire bus, least-significant bit first.
  * @param value (uint8_t) Byte to transmit.
  */
void OneWire_WriteByte(uint8_t value);

/**
  * @brief Read one bit from the one-wire bus.
  * @retval (uint8_t) Sampled bus level, either zero or one.
  */
uint8_t OneWire_ReadBit(void);

/**
  * @brief Read one byte from the one-wire bus.
  * @param value (uint8_t*) Destination for the received byte.
  */
void OneWire_ReadByte(uint8_t* value);

/**
  * @brief Update a Dallas/Maxim CRC-8 with one byte.
  * @param crc (uint8_t) Previous CRC value.
  * @param value (uint8_t) Byte to include.
  * @retval (uint8_t) Updated CRC value.
  */
uint8_t OneWire_CRC8(uint8_t crc, uint8_t value);

/**
  * @brief Discover devices and update the internal one-wire device table.
  * @retval (ErrorStatus) SUCCESS when the bus search completes.
  */
ErrorStatus OneWire_Search(void);

/**
  * @brief Create the task that refreshes one-wire discovery once per minute.
  */
void OneWireBusConfiguration_Init(void);

/**
  * @brief Acquire exclusive access to the one-wire bus.
  * @param timeout (TickType_t) Maximum number of scheduler ticks to wait.
  * @retval (BaseType_t) pdTRUE when the bus lock was acquired.
  */
BaseType_t OneWire_Lock(TickType_t timeout);

/**
  * @brief Release exclusive access to the one-wire bus.
  */
void OneWire_Unlock(void);

/**
  * @brief Return the number of devices found by the latest bus search.
  * @retval (uint8_t) Number of cached one-wire devices.
  */
uint8_t OneWire_GetDeviceCount(void);

/**
  * @brief Drive the one-wire pin push-pull high for parasitic power.
  */
void OneWire_StrongPullupEnable(void);

/**
  * @brief Release parasitic power and restore open-drain one-wire operation.
  */
void OneWire_StrongPullupDisable(void);

/**
  * @brief Determine whether a device uses parasitic power.
  * @param rom (uint8_t*) Eight-byte one-wire ROM code.
  * @retval (uint8_t) Zero for parasitic power and one for external power.
  */
uint8_t OneWire_ReadPowerSupply(uint8_t* rom);

/**
  * @brief Select the device with the supplied ROM code.
  * @param rom (uint8_t*) Eight-byte one-wire ROM code.
  * @retval (ErrorStatus) SUCCESS when the device acknowledges the selection.
  */
ErrorStatus OneWire_MatchROM(uint8_t* rom);

/**
  * @brief Return the internal table populated by the latest bus search.
  * @retval (OneWireDevice_TypeDef*) Borrowed pointer to the device table.
  */
OneWireDevice_TypeDef* OneWire_GetDevices(void);


/* Exported defines -----------------------------------------------------------*/
#define ONEWIRE_COMMAND_SEARCH_ROM        0xf0
#define ONEWIRE_COMMAND_READ_ROM          0x33
#define ONEWIRE_COMMAND_MATCH_ROM         0x55
#define ONEWIRE_COMMAND_SKIP_ROM          0xcc
#define ONEWIRE_COMMAND_READ_POWER_SUPPLY 0xb4

#define ONEWIRE_PORT GPIOB
#define ONEWIRE_PIN  GPIO_PIN_9_Pos

#define ONEWIRE_DRIVE_LOW PIN_H(ONEWIRE_PORT, ONEWIRE_PIN)
#define ONEWIRE_RELEASE   PIN_L(ONEWIRE_PORT, ONEWIRE_PIN)
#define ONEWIRE_LEVEL     (PIN_LEVEL(ONEWIRE_PORT, ONEWIRE_PIN))



#ifdef __cplusplus
}
#endif

#endif /* __ONEWIRE_H */
