/**
  ******************************************************************************
  * @file           : utils.s
  * @brief          : Cortex-M3 bit-band access helper routines.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 21.09.2025 08:26:19 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

.syntax unified

.global BitBand_SetValue
.global BitBand_GetValue



    .section  .text.BitBand_SetValue
    .type BitBand_SetValue, %function
BitBand_SetValue:
  str r1, [r0]
  bx lr
  .size  BitBand_SetValue, .-BitBand_SetValue


    .section  .text.BitBand_GetValue
    .type BitBand_GetValue, %function
BitBand_GetValue:
  ldr r0, [r0]
  bx lr
  .size  BitBand_GetValue, .-BitBand_GetValue

