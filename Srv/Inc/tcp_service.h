/**
  ******************************************************************************
  * @file           : tcp_service.h
  * @brief          : TCP command service interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 24.07.2026 01:34:04 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __TCP_SERVICE_H
#define __TCP_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief Creates the TCP command server task.
  */
void TcpCommandService_Init(void);

/**
  * @brief Advance the non-blocking measurement TCP server state machine.
  *
  * This function is called by the TCP service task and operates on port 5005.
  */
void TcpCommandService_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* __TCP_SERVICE_H */
