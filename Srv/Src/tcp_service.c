/**
  ******************************************************************************
  * @file           : tcp_service.c
  * @brief          : TCP measurement and sensor-information services.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 24.07.2026 01:34:04 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "main.h"
#include "tcp_service.h"
#include "socket.h"
#include <string.h>

#define TCP_COMMAND_SOCKET      2U
#define TCP_COMMAND_PORT        5005U
#define TCP_HEALTH_SOCKET       3U
#define TCP_HEALTH_PORT         5006U
#define TCP_COMMAND_BUFFER_SIZE 64U
#define TCP_RESPONSE_SIZE       128U

static uint8_t commandBuffer[TCP_COMMAND_BUFFER_SIZE];
static uint16_t commandLength;
static uint8_t healthBuffer[TCP_COMMAND_BUFFER_SIZE];
static uint16_t healthLength;

static void tcpCommandService_Task(void*);
static void tcpCommandService_Receive(void);
static void tcpCommandService_ProcessInput(void);
static void tcpCommandService_ProcessCommand(const uint8_t*, uint16_t);
static ErrorStatus tcpCommandService_Send(const char*);
static void tcpHealthService_Run(void);
static void tcpHealthService_Receive(void);
static void tcpHealthService_ProcessInput(void);
static void tcpHealthService_ProcessCommand(const uint8_t*, uint16_t);
static ErrorStatus tcpHealthService_Send(const char*);
static ErrorStatus tcpCommandService_ParseSensorNumber(
  const uint8_t*,
  uint16_t,
  uint16_t,
  uint8_t*
);
static const char* tcpCommandService_ModelName(SensorModel_TypeDef);
static const char* tcpCommandService_HealthName(DeviceHealthState_TypeDef);
static const char* tcpCommandService_ErrorName(SensorError_TypeDef);
static void tcpCommandService_AppendText(char*, size_t, size_t*, const char*);
static void tcpCommandService_AppendTemperature(char*, size_t, size_t*, int16_t);
static void tcpCommandService_AppendPressure(char*, size_t, size_t*, uint32_t);
static void tcpCommandService_AppendHumidity(char*, size_t, size_t*, uint32_t);




// -------------------------------------------------------------
void TcpCommandService_Init(void) {
  static StaticTask_t taskControlBlock;
  static StackType_t taskStack[512];

  HealthService_Register(HEALTH_COMPONENT_TCP);
  (void) xTaskCreateStatic(
    tcpCommandService_Task,
    "TCP Commands",
    512,
    NULL,
    configMAX_PRIORITIES - 3U,
    taskStack,
    &taskControlBlock
  );
}




// -------------------------------------------------------------
static void tcpCommandService_Task(void* parameters) {
  (void) parameters;

  while (1) {
    TcpCommandService_Run();
    tcpHealthService_Run();
    HealthService_Report(HEALTH_COMPONENT_TCP);
    vTaskDelay(1U);
  }
}




// -------------------------------------------------------------
void TcpCommandService_Run(void) {
  switch (getSn_SR(TCP_COMMAND_SOCKET)) {
    case SOCK_ESTABLISHED:
      if ((getSn_IR(TCP_COMMAND_SOCKET) & Sn_IR_CON) != 0U) {
        setSn_IR(TCP_COMMAND_SOCKET, Sn_IR_CON);
        commandLength = 0U;
      }

      tcpCommandService_Receive();
      break;

    case SOCK_CLOSE_WAIT:
      if (getSn_RX_RSR(TCP_COMMAND_SOCKET) > 0U) {
        tcpCommandService_Receive();
      }
      if (getSn_SR(TCP_COMMAND_SOCKET) == SOCK_CLOSE_WAIT) {
        (void) disconnect(TCP_COMMAND_SOCKET);
        commandLength = 0U;
      }
      break;

    case SOCK_INIT:
      if (listen(TCP_COMMAND_SOCKET) != SOCK_OK) {
        (void) close(TCP_COMMAND_SOCKET);
      }
      break;

    case SOCK_CLOSED:
      commandLength = 0U;
      if (socket(TCP_COMMAND_SOCKET, Sn_MR_TCP, TCP_COMMAND_PORT, 0x00) != TCP_COMMAND_SOCKET) {
        (void) close(TCP_COMMAND_SOCKET);
      }
      break;

    default:
      break;
  }
}




// -------------------------------------------------------------
static void tcpHealthService_Run(void) {
  switch (getSn_SR(TCP_HEALTH_SOCKET)) {
    case SOCK_ESTABLISHED:
      if ((getSn_IR(TCP_HEALTH_SOCKET) & Sn_IR_CON) != 0U) {
        setSn_IR(TCP_HEALTH_SOCKET, Sn_IR_CON);
        healthLength = 0U;
      }
      tcpHealthService_Receive();
      break;

    case SOCK_CLOSE_WAIT:
      if (getSn_RX_RSR(TCP_HEALTH_SOCKET) > 0U) {
        tcpHealthService_Receive();
      }
      if (getSn_SR(TCP_HEALTH_SOCKET) == SOCK_CLOSE_WAIT) {
        (void)disconnect(TCP_HEALTH_SOCKET);
        healthLength = 0U;
      }
      break;

    case SOCK_INIT:
      if (listen(TCP_HEALTH_SOCKET) != SOCK_OK) {
        (void)close(TCP_HEALTH_SOCKET);
      }
      break;

    case SOCK_CLOSED:
      healthLength = 0U;
      if (socket(
            TCP_HEALTH_SOCKET,
            Sn_MR_TCP,
            TCP_HEALTH_PORT,
            0x00
          ) != TCP_HEALTH_SOCKET) {
        (void)close(TCP_HEALTH_SOCKET);
      }
      break;

    default:
      break;
  }
}




// -------------------------------------------------------------
static void tcpCommandService_Receive(void) {
  uint16_t receivedSize = getSn_RX_RSR(TCP_COMMAND_SOCKET);
  if (receivedSize == 0U) return;

  uint16_t available = TCP_COMMAND_BUFFER_SIZE - commandLength;
  uint16_t readSize = (receivedSize < available) ? receivedSize : available;
  if (readSize == 0U) {
    (void) tcpCommandService_Send("ERR command_too_long\r\n");
    commandLength = 0U;
    return;
  }

  int32_t result = recv(TCP_COMMAND_SOCKET, &commandBuffer[commandLength], readSize);
  if (result <= 0) {
    (void) close(TCP_COMMAND_SOCKET);
    commandLength = 0U;
    return;
  }

  commandLength += (uint16_t)result;
  tcpCommandService_ProcessInput();
}




// -------------------------------------------------------------
static void tcpHealthService_Receive(void) {
  uint16_t receivedSize = getSn_RX_RSR(TCP_HEALTH_SOCKET);
  if (receivedSize == 0U) return;

  uint16_t available = TCP_COMMAND_BUFFER_SIZE - healthLength;
  uint16_t readSize = (receivedSize < available) ? receivedSize : available;
  if (readSize == 0U) {
    (void)tcpHealthService_Send("ERR command_too_long\r\n");
    healthLength = 0U;
    return;
  }

  int32_t result = recv(
    TCP_HEALTH_SOCKET,
    &healthBuffer[healthLength],
    readSize
  );
  if (result <= 0) {
    (void)close(TCP_HEALTH_SOCKET);
    healthLength = 0U;
    return;
  }

  healthLength += (uint16_t)result;
  tcpHealthService_ProcessInput();
}




// -------------------------------------------------------------
static void tcpCommandService_ProcessInput(void) {
  uint16_t commandStart = 0U;

  for (uint16_t i = 0U; i < commandLength; i++) {
    if ((commandBuffer[i] == '\r') || (commandBuffer[i] == '\n')) {
      if (i > commandStart) {
        tcpCommandService_ProcessCommand(&commandBuffer[commandStart], i - commandStart);
        commandLength = 0U;
        return;
      }
      commandStart = i + 1U;
    }
  }

  if (commandStart > 0U) {
    uint16_t remaining = commandLength - commandStart;
    memmove(commandBuffer, &commandBuffer[commandStart], remaining);
    commandLength = remaining;
  }

  if (commandLength == TCP_COMMAND_BUFFER_SIZE) {
    (void) tcpCommandService_Send("ERR command_too_long\r\n");
    commandLength = 0U;
  }
}




// -------------------------------------------------------------
static void tcpHealthService_ProcessInput(void) {
  uint16_t commandStart = 0U;

  for (uint16_t i = 0U; i < healthLength; i++) {
    if ((healthBuffer[i] == '\r') || (healthBuffer[i] == '\n')) {
      if (i > commandStart) {
        tcpHealthService_ProcessCommand(
          &healthBuffer[commandStart],
          i - commandStart
        );
        healthLength = 0U;
        return;
      }
      commandStart = i + 1U;
    }
  }

  if (commandStart > 0U) {
    uint16_t remaining = healthLength - commandStart;
    memmove(healthBuffer, &healthBuffer[commandStart], remaining);
    healthLength = remaining;
  }
  if (healthLength == TCP_COMMAND_BUFFER_SIZE) {
    (void)tcpHealthService_Send("ERR command_too_long\r\n");
    healthLength = 0U;
  }
}




// -------------------------------------------------------------
static void tcpCommandService_ProcessCommand(const uint8_t* command, uint16_t length) {
  char response[TCP_RESPONSE_SIZE];
  size_t used = 0U;

  /** @brief Measurement command selected from the received TCP prefix. */
  typedef enum {
    TCP_SENSOR_COMMAND_NONE = 0U,
    TCP_SENSOR_COMMAND_ALL,
    TCP_SENSOR_COMMAND_TEMPERATURE,
    TCP_SENSOR_COMMAND_PRESSURE,
    TCP_SENSOR_COMMAND_HUMIDITY
  } TcpSensorCommand_TypeDef;

  TcpSensorCommand_TypeDef commandType = TCP_SENSOR_COMMAND_NONE;
  uint16_t prefixLength = 0U;
  if ((length > 8U) && (memcmp(command, "get_all_", 8U) == 0)) {
    commandType = TCP_SENSOR_COMMAND_ALL;
    prefixLength = 8U;
  } else if ((length > 6U) && (memcmp(command, "get_t_", 6U) == 0)) {
    commandType = TCP_SENSOR_COMMAND_TEMPERATURE;
    prefixLength = 6U;
  } else if ((length > 6U) && (memcmp(command, "get_p_", 6U) == 0)) {
    commandType = TCP_SENSOR_COMMAND_PRESSURE;
    prefixLength = 6U;
  } else if ((length > 6U) && (memcmp(command, "get_h_", 6U) == 0)) {
    commandType = TCP_SENSOR_COMMAND_HUMIDITY;
    prefixLength = 6U;
  }

  if (commandType == TCP_SENSOR_COMMAND_NONE) {
    (void) tcpCommandService_Send("ERR unknown_command\r\n");
    return;
  }

  uint8_t sensorNumber;
  if (tcpCommandService_ParseSensorNumber(
        command,
        length,
        prefixLength,
        &sensorNumber
      ) != SUCCESS) {
    (void) tcpCommandService_Send("ERR invalid_sensor_number\r\n");
    return;
  }

  SensorSnapshot_TypeDef snapshot;
  if (TemperatureSensorService_GetSnapshot(
        sensorNumber,
        &snapshot
      ) != SUCCESS) {
    (void) tcpCommandService_Send("ERR sensor_not_found\r\n");
    return;
  }

  uint8_t requiredCapability = 0U;
  if (commandType == TCP_SENSOR_COMMAND_TEMPERATURE) {
    requiredCapability = SENSOR_CAPABILITY_TEMPERATURE;
  } else if (commandType == TCP_SENSOR_COMMAND_PRESSURE) {
    requiredCapability = SENSOR_CAPABILITY_PRESSURE;
  } else if (commandType == TCP_SENSOR_COMMAND_HUMIDITY) {
    requiredCapability = SENSOR_CAPABILITY_HUMIDITY;
  }
  if ((requiredCapability != 0U)
      && ((snapshot.capabilities & requiredCapability) == 0U)) {
    (void)tcpCommandService_Send("ERR measurement_not_supported\r\n");
    return;
  }

  if (snapshot.dataValid != pdTRUE) {
    (void)tcpCommandService_Send("ERR measurement_unavailable\r\n");
    return;
  }

  used = (size_t)snprintf(response, sizeof(response), "OK");
  if ((commandType == TCP_SENSOR_COMMAND_ALL)
      || (commandType == TCP_SENSOR_COMMAND_TEMPERATURE)) {
    if (commandType == TCP_SENSOR_COMMAND_ALL) {
      tcpCommandService_AppendText(response, sizeof(response), &used, " model:");
      tcpCommandService_AppendText(
        response,
        sizeof(response),
        &used,
        tcpCommandService_ModelName(snapshot.model)
      );
      tcpCommandService_AppendText(response, sizeof(response), &used, ",t:");
    } else {
      tcpCommandService_AppendText(response, sizeof(response), &used, " ");
    }
    tcpCommandService_AppendTemperature(
      response,
      sizeof(response),
      &used,
      snapshot.temperature
    );
  }
  if ((commandType == TCP_SENSOR_COMMAND_ALL)
      && ((snapshot.capabilities & SENSOR_CAPABILITY_PRESSURE) != 0U)) {
    tcpCommandService_AppendText(response, sizeof(response), &used, ",p:");
    tcpCommandService_AppendPressure(response, sizeof(response), &used, snapshot.pressure);
  } else if (commandType == TCP_SENSOR_COMMAND_PRESSURE) {
    tcpCommandService_AppendText(response, sizeof(response), &used, " ");
    tcpCommandService_AppendPressure(response, sizeof(response), &used, snapshot.pressure);
  }
  if ((commandType == TCP_SENSOR_COMMAND_ALL)
      && ((snapshot.capabilities & SENSOR_CAPABILITY_HUMIDITY) != 0U)) {
    tcpCommandService_AppendText(response, sizeof(response), &used, ",h:");
    tcpCommandService_AppendHumidity(response, sizeof(response), &used, snapshot.humidity);
  } else if (commandType == TCP_SENSOR_COMMAND_HUMIDITY) {
    tcpCommandService_AppendText(response, sizeof(response), &used, " ");
    tcpCommandService_AppendHumidity(response, sizeof(response), &used, snapshot.humidity);
  }
  if (used < (sizeof(response) - 2U)) {
    response[used++] = '\r';
    response[used++] = '\n';
    response[used] = '\0';
  }
  (void)tcpCommandService_Send(response);
}




// -------------------------------------------------------------
static void tcpHealthService_ProcessCommand(
  const uint8_t* command,
  uint16_t length
) {
  char response[TCP_RESPONSE_SIZE];

  if ((length == 11U) && (memcmp(command, "get_sensors", 11U) == 0)) {
    SensorCounts_TypeDef counts;
    TemperatureSensorService_GetCounts(&counts);
    (void)snprintf(
      response,
      sizeof(response),
      "OK t:%u,h:%u,p:%u,all:%u\r\n",
      counts.temperature,
      counts.humidity,
      counts.pressure,
      counts.all
    );
    (void)tcpHealthService_Send(response);
    return;
  }

  /** @brief Information command selected from the received TCP prefix. */
  typedef enum {
    TCP_INFO_COMMAND_NONE = 0U,
    TCP_INFO_COMMAND_MODEL,
    TCP_INFO_COMMAND_SERIAL,
    TCP_INFO_COMMAND_HEALTH
  } TcpInfoCommand_TypeDef;

  TcpInfoCommand_TypeDef commandType = TCP_INFO_COMMAND_NONE;
  uint16_t prefixLength = 0U;
  if ((length > 10U) && (memcmp(command, "get_model_", 10U) == 0)) {
    commandType = TCP_INFO_COMMAND_MODEL;
    prefixLength = 10U;
  } else if ((length > 7U) && (memcmp(command, "get_sn_", 7U) == 0)) {
    commandType = TCP_INFO_COMMAND_SERIAL;
    prefixLength = 7U;
  } else if ((length > 7U) && (memcmp(command, "health_", 7U) == 0)) {
    commandType = TCP_INFO_COMMAND_HEALTH;
    prefixLength = 7U;
  }

  if (commandType == TCP_INFO_COMMAND_NONE) {
    (void)tcpHealthService_Send("ERR unknown_command\r\n");
    return;
  }

  uint8_t sensorNumber;
  if (tcpCommandService_ParseSensorNumber(
        command,
        length,
        prefixLength,
        &sensorNumber
      ) != SUCCESS) {
    (void)tcpHealthService_Send("ERR invalid_sensor_number\r\n");
    return;
  }

  SensorSnapshot_TypeDef snapshot;
  if (TemperatureSensorService_GetSnapshot(
        sensorNumber,
        &snapshot
      ) != SUCCESS) {
    (void)tcpHealthService_Send("ERR sensor_not_found\r\n");
    return;
  }

  if (commandType == TCP_INFO_COMMAND_MODEL) {
    (void)snprintf(
      response,
      sizeof(response),
      "OK %s\r\n",
      tcpCommandService_ModelName(snapshot.model)
    );
    (void)tcpHealthService_Send(response);
    return;
  }

  if (commandType == TCP_INFO_COMMAND_SERIAL) {
    if (snapshot.model == SENSOR_MODEL_DS18B20) {
      (void)snprintf(
        response,
        sizeof(response),
        "OK %02X%02X%02X%02X%02X%02X%02X%02X\r\n",
        snapshot.identity[0],
        snapshot.identity[1],
        snapshot.identity[2],
        snapshot.identity[3],
        snapshot.identity[4],
        snapshot.identity[5],
        snapshot.identity[6],
        snapshot.identity[7]
      );
    } else {
      (void)snprintf(
        response,
        sizeof(response),
        "OK %08lX\r\n",
        (unsigned long)snapshot.serialNumber
      );
    }
    (void)tcpHealthService_Send(response);
    return;
  }

  if (snapshot.health.lastSuccess == 0U) {
    (void)snprintf(
      response,
      sizeof(response),
      "OK model:%s,state:%s,age_ms:unavailable,failures:%u,error:%s\r\n",
      tcpCommandService_ModelName(snapshot.model),
      tcpCommandService_HealthName(snapshot.health.state),
      snapshot.health.consecutiveFailures,
      tcpCommandService_ErrorName((SensorError_TypeDef)snapshot.health.lastError)
    );
  } else {
    TickType_t age = xTaskGetTickCount() - snapshot.health.lastSuccess;
    (void)snprintf(
      response,
      sizeof(response),
      "OK model:%s,state:%s,age_ms:%lu,failures:%u,error:%s\r\n",
      tcpCommandService_ModelName(snapshot.model),
      tcpCommandService_HealthName(snapshot.health.state),
      (unsigned long)(age * portTICK_PERIOD_MS),
      snapshot.health.consecutiveFailures,
      tcpCommandService_ErrorName((SensorError_TypeDef)snapshot.health.lastError)
    );
  }
  (void)tcpHealthService_Send(response);
}




// -------------------------------------------------------------
static ErrorStatus tcpCommandService_ParseSensorNumber(
  const uint8_t* command,
  uint16_t length,
  uint16_t prefixLength,
  uint8_t* sensorNumber
) {
  if ((sensorNumber == NULL) || (prefixLength >= length)) return (ERROR);
  uint16_t value = 0U;
  for (uint16_t i = prefixLength; i < length; i++) {
    if ((command[i] < '0') || (command[i] > '9')) return (ERROR);
    value = (uint16_t)(value * 10U + (command[i] - '0'));
    if (value > 255U) return (ERROR);
  }
  if (value == 0U) return (ERROR);
  *sensorNumber = (uint8_t)value;
  return (SUCCESS);
}




// -------------------------------------------------------------
static const char* tcpCommandService_ModelName(SensorModel_TypeDef model) {
  switch (model) {
    case SENSOR_MODEL_DS18B20: return ("DS18B20");
    default:                   return ("unknown");
  }
}




// -------------------------------------------------------------
static const char* tcpCommandService_HealthName(
  DeviceHealthState_TypeDef health
) {
  switch (health) {
    case DEVICE_HEALTH_INITIALIZING: return ("initializing");
    case DEVICE_HEALTH_AVAILABLE:    return ("healthy");
    case DEVICE_HEALTH_DEGRADED:     return ("degraded");
    case DEVICE_HEALTH_UNAVAILABLE:  return ("failed");
    case DEVICE_HEALTH_STALE:        return ("stale");
    case DEVICE_HEALTH_MISSING:      return ("missing");
    default:                         return ("unknown");
  }
}




// -------------------------------------------------------------
static const char* tcpCommandService_ErrorName(SensorError_TypeDef error) {
  switch (error) {
    case SENSOR_ERROR_NONE:       return ("none");
    case SENSOR_ERROR_NOT_READY:  return ("not_ready");
    case SENSOR_ERROR_TIMEOUT:    return ("timeout");
    case SENSOR_ERROR_CRC:        return ("crc");
    case SENSOR_ERROR_BUS:        return ("bus");
    case SENSOR_ERROR_MISSING:    return ("missing");
    case SENSOR_ERROR_CONVERSION: return ("conversion");
    default:                      return ("unknown");
  }
}




// -------------------------------------------------------------
static void tcpCommandService_AppendText(
  char* response,
  size_t capacity,
  size_t* used,
  const char* text
) {
  if ((response == NULL) || (used == NULL) || (text == NULL)
      || (*used >= capacity)) {
    return;
  }
  size_t available = capacity - *used - 1U;
  size_t length = strlen(text);
  size_t copyLength = (length < available) ? length : available;
  memcpy(&response[*used], text, copyLength);
  *used += copyLength;
  response[*used] = '\0';
}




// -------------------------------------------------------------
static void tcpCommandService_AppendPressure(
  char* response,
  size_t capacity,
  size_t* used,
  uint32_t pressure
) {
  int written = snprintf(
    &response[*used],
    capacity - *used,
    "%lu",
    (unsigned long)pressure
  );
  if (written > 0) {
    size_t remaining = capacity - *used;
    size_t appended = (size_t)written;
    *used += (appended < remaining) ? appended : (remaining - 1U);
  }
}




// -------------------------------------------------------------
static void tcpCommandService_AppendHumidity(
  char* response,
  size_t capacity,
  size_t* used,
  uint32_t humidity
) {
  int written = snprintf(
    &response[*used],
    capacity - *used,
    "%lu.%03lu",
    (unsigned long)(humidity / 1000U),
    (unsigned long)(humidity % 1000U)
  );
  if (written > 0) {
    size_t remaining = capacity - *used;
    size_t appended = (size_t)written;
    *used += (appended < remaining) ? appended : (remaining - 1U);
  }
}




// -------------------------------------------------------------
static void tcpCommandService_AppendTemperature(
  char* response,
  size_t capacity,
  size_t* used,
  int16_t centiDegrees
) {
  uint32_t magnitude = (centiDegrees < 0)
    ? (uint32_t)(-(int32_t)centiDegrees)
    : (uint32_t)centiDegrees;

  int written = snprintf(
    &response[*used],
    capacity - *used,
    "%s%lu.%02lu",
    (centiDegrees < 0) ? "-" : "",
    (unsigned long)(magnitude / 100U),
    (unsigned long)(magnitude % 100U)
  );
  if (written > 0) {
    size_t remaining = capacity - *used;
    size_t appended = (size_t)written;
    *used += (appended < remaining) ? appended : (remaining - 1U);
  }
}




// -------------------------------------------------------------
static ErrorStatus tcpCommandService_Send(const char* response) {
  uint16_t length = (uint16_t)strlen(response);
  uint16_t sent = 0U;

  while (sent < length) {
    int32_t result = send(TCP_COMMAND_SOCKET, (uint8_t*)&response[sent], length - sent);
    if (result <= 0) {
      (void) close(TCP_COMMAND_SOCKET);
      return (ERROR);
    }
    sent += (uint16_t)result;
  }

  TickType_t start = xTaskGetTickCount();
  TickType_t timeout = pdMS_TO_TICKS(1000U);
  while ((getSn_IR(TCP_COMMAND_SOCKET) & Sn_IR_SENDOK) == 0U) {
    uint8_t status = getSn_SR(TCP_COMMAND_SOCKET);
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT)) {
      (void) close(TCP_COMMAND_SOCKET);
      return (ERROR);
    }
    if ((xTaskGetTickCount() - start) >= timeout) {
      (void) close(TCP_COMMAND_SOCKET);
      return (ERROR);
    }
    vTaskDelay(1U);
  }

  setSn_IR(TCP_COMMAND_SOCKET, Sn_IR_SENDOK);
  (void) disconnect(TCP_COMMAND_SOCKET);
  commandLength = 0U;
  return (SUCCESS);
}




// -------------------------------------------------------------
static ErrorStatus tcpHealthService_Send(const char* response) {
  uint16_t length = (uint16_t)strlen(response);
  uint16_t sent = 0U;

  while (sent < length) {
    int32_t result = send(
      TCP_HEALTH_SOCKET,
      (uint8_t*)&response[sent],
      length - sent
    );
    if (result <= 0) {
      (void)close(TCP_HEALTH_SOCKET);
      return (ERROR);
    }
    sent += (uint16_t)result;
  }

  TickType_t start = xTaskGetTickCount();
  TickType_t timeout = pdMS_TO_TICKS(1000U);
  while ((getSn_IR(TCP_HEALTH_SOCKET) & Sn_IR_SENDOK) == 0U) {
    uint8_t status = getSn_SR(TCP_HEALTH_SOCKET);
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT)) {
      (void)close(TCP_HEALTH_SOCKET);
      return (ERROR);
    }
    if ((xTaskGetTickCount() - start) >= timeout) {
      (void)close(TCP_HEALTH_SOCKET);
      return (ERROR);
    }
    vTaskDelay(1U);
  }

  setSn_IR(TCP_HEALTH_SOCKET, Sn_IR_SENDOK);
  (void)disconnect(TCP_HEALTH_SOCKET);
  healthLength = 0U;
  return (SUCCESS);
}
