/**
  ******************************************************************************
  * @file           : api_service.h
  * @brief          : HTTP/JSON REST API service interface.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#ifndef __API_SERVICE_H
#define __API_SERVICE_H

/**
  * @brief Create the HTTP API server task, listening on TCP port 80.
  */
void ApiService_Init(void);

#endif /* __API_SERVICE_H */
