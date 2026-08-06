/**
  ******************************************************************************
  * @file           : common.h
  * @brief          : Common application definitions and low-level helper declarations.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 20.09.2025 08:34:08 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMMON_H
#define __COMMON_H

#ifdef __cplusplus
  extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported types ------------------------------------------------------------*/
    
/* Private typedef -----------------------------------------------------------*/

/* Private defines -----------------------------------------------------------*/
/* NVIC */
#define NVIC_PRIORITYGROUP_0         0x00000007U /*!< 0 bits for pre-emption priority 4 bits for subpriority */
#define NVIC_PRIORITYGROUP_1         0x00000006U /*!< 1 bits for pre-emption priority 3 bits for subpriority */
#define NVIC_PRIORITYGROUP_2         0x00000005U /*!< 2 bits for pre-emption priority 2 bits for subpriority */
#define NVIC_PRIORITYGROUP_3         0x00000004U /*!< 3 bits for pre-emption priority 1 bits for subpriority */
#define NVIC_PRIORITYGROUP_4         0x00000003U /*!< 4 bits for pre-emption priority 0 bits for subpriority */

/* GPIO*/
#define GPIO_PIN_0                  GPIO_BSRR_BS0_Pos /*!< Select pin 0 */
#define GPIO_PIN_0_Pos              GPIO_PIN_0
#define GPIO_PIN_0_Mask             0x0000000f
#define GPIO_PIN_1                  GPIO_BSRR_BS1_Pos /*!< Select pin 1 */
#define GPIO_PIN_1_Pos              GPIO_PIN_1
#define GPIO_PIN_1_Mask             0x000000f0
#define GPIO_PIN_2                  GPIO_BSRR_BS2_Pos /*!< Select pin 2 */
#define GPIO_PIN_2_Pos              GPIO_PIN_2
#define GPIO_PIN_2_Mask             0x00000f00
#define GPIO_PIN_3                  GPIO_BSRR_BS3_Pos /*!< Select pin 3 */
#define GPIO_PIN_3_Pos              GPIO_PIN_3
#define GPIO_PIN_3_Mask             0x0000f000
#define GPIO_PIN_4                  GPIO_BSRR_BS4_Pos /*!< Select pin 4 */
#define GPIO_PIN_4_Pos              GPIO_PIN_4
#define GPIO_PIN_4_Mask             0x000f0000
#define GPIO_PIN_5                  GPIO_BSRR_BS5_Pos /*!< Select pin 5 */
#define GPIO_PIN_5_Pos              GPIO_PIN_5
#define GPIO_PIN_5_Mask             0x00f00000
#define GPIO_PIN_6                  GPIO_BSRR_BS6_Pos /*!< Select pin 6 */
#define GPIO_PIN_6_Pos              GPIO_PIN_6
#define GPIO_PIN_6_Mask             0x0f000000
#define GPIO_PIN_7                  GPIO_BSRR_BS7_Pos /*!< Select pin 7 */
#define GPIO_PIN_7_Pos              GPIO_PIN_7
#define GPIO_PIN_7_Mask             0xf0000000
#define GPIO_PIN_8                  GPIO_BSRR_BS8_Pos /*!< Select pin 8 */
#define GPIO_PIN_8_Pos              GPIO_PIN_8
#define GPIO_PIN_8_Mask             0x0000000f
#define GPIO_PIN_9                  GPIO_BSRR_BS9_Pos /*!< Select pin 9 */
#define GPIO_PIN_9_Pos              GPIO_PIN_9
#define GPIO_PIN_9_Mask             0x000000f0
#define GPIO_PIN_10                 GPIO_BSRR_BS10_Pos /*!< Select pin 10 */
#define GPIO_PIN_10_Pos             GPIO_PIN_10
#define GPIO_PIN_10_Mask            0x00000f00
#define GPIO_PIN_11                 GPIO_BSRR_BS11_Pos /*!< Select pin 11 */
#define GPIO_PIN_11_Pos             GPIO_PIN_11
#define GPIO_PIN_11_Mask            0x0000f000
#define GPIO_PIN_12                 GPIO_BSRR_BS12_Pos /*!< Select pin 12 */
#define GPIO_PIN_12_Pos             GPIO_PIN_12
#define GPIO_PIN_12_Mask            0x000f0000
#define GPIO_PIN_13                 GPIO_BSRR_BS13_Pos /*!< Select pin 13 */
#define GPIO_PIN_13_Pos             GPIO_PIN_13
#define GPIO_PIN_13_Mask            0x00f00000
#define GPIO_PIN_14                 GPIO_BSRR_BS14_Pos /*!< Select pin 14 */
#define GPIO_PIN_14_Pos             GPIO_PIN_14
#define GPIO_PIN_14_Mask            0x0f000000
#define GPIO_PIN_15                 GPIO_BSRR_BS15_Pos /*!< Select pin 15 */
#define GPIO_PIN_15_Pos             GPIO_PIN_15
#define GPIO_PIN_15_Mask            0xf0000000
#define GPIO_PIN_ALL                (uint16_t)0xffff /*!< Select all pins */

/* GPIO Modes bit definitions */
#define GPIO_GPO_PP                 0b0000 // General purpose output push-pull
#define GPIO_GPO_OD                 0b0100 // General purpose output open drain
#define GPIO_IOS_2                  0b0010 // I/O speed is 2 MHz
#define GPIO_IOS_10                 0b0001 // I/O speed is 10 MHz
#define GPIO_IOS_50                 0b0011 // I/O speed is 50 MHz
#define GPIO_AF_PP                  0b1000 // Alterhative function push-pull
#define GPIO_AF_OD                  0b1100 // Alterhative function open drain
#define GPIO_IN_AN                  0b0000 // Input analog
#define GPIO_IN_FL                  0b0100 // Input floating point
#define GPIO_IN_PD                  0b1000 // correspondent ODR register pin has to be 0
#define GPIO_IN_PU                  0b1000 // correspondent ODR register pin has to be 1

/* GPIO Alternative function defines */
#define GPIO_AF_0                   0b0000 /*!< Select alternate function 0 */
#define GPIO_AF_1                   0b0001 /*!< Select alternate function 1 */
#define GPIO_AF_2                   0b0010 /*!< Select alternate function 2 */
#define GPIO_AF_3                   0b0011 /*!< Select alternate function 3 */
#define GPIO_AF_4                   0b0100 /*!< Select alternate function 4 */
#define GPIO_AF_5                   0b0101 /*!< Select alternate function 5 */
#define GPIO_AF_6                   0b0110 /*!< Select alternate function 6 */
#define GPIO_AF_7                   0b0111 /*!< Select alternate function 7 */
#define GPIO_AF_8                   0b1000 /*!< Select alternate function 8 */
#define GPIO_AF_9                   0b1001 /*!< Select alternate function 9 */
#define GPIO_AF_10                  0b1010 /*!< Select alternate function 10 */
#define GPIO_AF_11                  0b1011 /*!< Select alternate function 11 */
#define GPIO_AF_12                  0b1100 /*!< Select alternate function 12 */
#define GPIO_AF_13                  0b1101 /*!< Select alternate function 13 */
#define GPIO_AF_14                  0b1110 /*!< Select alternate function 14 */
#define GPIO_AF_15                  0b1111 /*!< Select alternate function 15 */
/* GPIO Register Offcets */
#define GPIO_IDR_Offset             (uint16_t)0x0008
#define GPIO_ODR_Offset             (uint16_t)0x000c
#define GPIO_BSRR_Offset            (uint16_t)0x0010
#define GPIO_BRR_Offset             (uint16_t)0x0014

/* EXTI */
#define EXTI_TRIGGER_NONE           0b00 /*!< No Trigger Mode */
#define EXTI_TRIGGER_RISING         0b01 /*!< Trigger Rising Mode */
#define EXTI_TRIGGER_FALLING        0b10 /*!< Trigger Falling Mode */
#define EXTI_TRIGGER_BOTH           0b11 /*!< Trigger Rising & Falling Mode */

/* IWDG */
#define IWDG_KEY_RELOAD             0x0000aaaa
#define IWDG_KEY_ENABLE             0x0000cccc
#define IWDG_KEY_ACCESS             0x00005555
#define IWDG_PRESCALER_DIV64        0x00000004
#define IWDG_RELOAD_COUNTER         0x000005db /* 1499 */



/* Private macro -------------------------------------------------------------*/
#define GET_PERIPH_BB_ADDR(addr, periphOffset, wordOffset)      (PERIPH_BB_BASE + ((addr + periphOffset) * 32U) + (wordOffset * 4U))
#define SET_PERIPH_BB_VAL(addr, periphOffset, wordOffset, key)  (*(uint32_t*)(GET_PERIPH_BB_ADDR(addr, periphOffset, wordOffset)) = key)
#define GET_PERIPH_BB_VAL(addr, periphOffset, wordOffset)       (*(uint32_t*)(GET_PERIPH_BB_ADDR(addr, periphOffset, wordOffset)))

#define GET_SRAM_BB_ADDR(addr, offset)                          (SRAM_BB_BASE + (addr * 32U) + (offset * 4U))
#define SET_SRAM_BB_VAL(addr, offset, key)                      (*(uint32_t*)(GET_SRAM_BB_ADDR(addr, offset)) = key)
#define GET_SRAM_BB_VAL(addr, offset)                           (*(uint32_t*)(GET_SRAM_BB_ADDR(addr, offset)))

/* Exported macro ------------------------------------------------------------*/
#define FLAG_SET(registry, flag)                                SET_SRAM_BB_VAL((uint32_t)&registry, flag, 1)
#define FLAG_CLR(registry, flag)                                SET_SRAM_BB_VAL((uint32_t)&registry, flag, 0)
#define FLAG_CHECK(registry, flag)                              (GET_SRAM_BB_VAL((uint32_t)&registry, flag))

#define PIN_H(port, pinSource)                                  SET_PERIPH_BB_VAL((uint32_t)port, GPIO_BSRR_Offset, pinSource, 1)
#define PIN_L(port, pinSource)                                  SET_PERIPH_BB_VAL((uint32_t)port, GPIO_BSRR_Offset, (pinSource + 16U), 1)
// #define PIN_LEVEL(port, pinSource)                              (GET_PERIPH_BB_VAL((uint32_t)port, GPIO_IDR_Offset, pinSource))
#define PIN_LEVEL(port, pinSource)                               (BitBand_GetValue(GET_PERIPH_BB_ADDR((uint32_t)port, GPIO_IDR_Offset, pinSource)))

#define PREG_SET(registry, key)                                 SET_PERIPH_BB_VAL((uint32_t)&registry, 0, key, 1)
#define PREG_CLR(registry, key)                                 SET_PERIPH_BB_VAL((uint32_t)&registry, 0, key, 0)
// #define PREG_CHECK(registry, key)                               (GET_PERIPH_BB_VAL((uint32_t)&registry, 0, key))
#define PREG_CHECK(registry, key)                               (BitBand_GetValue(GET_PERIPH_BB_ADDR((uint32_t)&registry, 0, key)))




/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief Handle an unrecoverable system error.
  *
  * The default weak implementation stops execution. Applications may provide
  * a strong implementation to report the failure or reset the system.
  */
void System_ErrorHandler(void);

/**
  * @brief Write a value through a Cortex-M3 bit-band alias address.
  * @param aliasAddress (uint32_t) Bit-band alias address to write.
  * @param value (uint32_t) Bit value to store; normally zero or one.
  */
void BitBand_SetValue(uint32_t aliasAddress, uint32_t value);

/**
  * @brief Read a value through a Cortex-M3 bit-band alias address.
  * @param aliasAddress (uint32_t) Bit-band alias address to read.
  * @retval (uint32_t) Current value of the aliased bit.
  */
uint32_t BitBand_GetValue(uint32_t aliasAddress);

/**
  * @brief Delay execution using the Cortex-M3 cycle counter.
  * @param microseconds (uint32_t) Delay duration in microseconds.
  *
  * This function performs a blocking busy wait and must not be called before
  * the core cycle counter and SystemCoreClock are initialized.
  */
void Delay_Microseconds(uint32_t microseconds);

/**
  * @brief Delay execution using a blocking microsecond delay.
  * @param milliseconds (uint32_t) Delay duration in milliseconds.
  *
  * This function is intended for peripheral initialization and short hardware
  * timing intervals. FreeRTOS tasks should prefer vTaskDelay for long waits.
  */
void Delay_Milliseconds(uint32_t milliseconds);



#ifdef __cplusplus
}
#endif

#endif /* __COMMON_H */
