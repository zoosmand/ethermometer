/**
  ******************************************************************************
  * @file           : api_service.c
  * @brief          : HTTP/JSON REST API service implementation.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "main.h"
#include "api_service.h"
#include "threshold_service.h"
#include "rtc_service.h"
#include "socket.h"
#include <string.h>

#define API_SOCKET               2U
#define API_PORT                 80U
#define API_REQUEST_BUFFER_SIZE 512U
#define API_BUZZER_WAIT_MS      1000U

/** @brief HTTP method recognized by the router. */
typedef enum {
  API_METHOD_UNKNOWN = 0U,
  API_METHOD_GET,
  API_METHOD_POST,
  API_METHOD_PUT,
  API_METHOD_HEAD
} ApiMethod_TypeDef;

static uint8_t requestBuffer[API_REQUEST_BUFFER_SIZE];
static uint16_t requestLength;
static BaseType_t headersParsed;
static uint16_t headerEnd;
static uint32_t contentLength;
static ApiMethod_TypeDef requestMethod;
static uint16_t requestPathStart;
static uint16_t requestPathLength;

static void apiService_Task(void*);
static void apiService_Run(void);
static void apiService_Receive(void);
static void apiService_ResetRequest(void);
static ErrorStatus apiService_LocateHeaderEnd(uint16_t*);
static ErrorStatus apiService_ParseRequestLine(void);
static BaseType_t apiService_HeaderNameIs(const uint8_t*, uint16_t, const char*);
static ErrorStatus apiService_ParseContentLength(void);
static void apiService_Dispatch(void);
static BaseType_t apiService_PathIs(const uint8_t*, uint16_t, const char*);
static ErrorStatus apiService_ParseIndex(const uint8_t*, uint16_t, uint8_t*);
static ErrorStatus apiService_PathIndex(const uint8_t*, uint16_t, const char*, uint8_t*);
static ErrorStatus apiService_ParseJsonNumber(const uint8_t*, uint16_t, const char*, uint32_t, int32_t*);
static ErrorStatus apiService_ParseJsonBool(const uint8_t*, uint16_t, const char*, BaseType_t*);
static const char* apiService_ModelName(SensorModel_TypeDef);
static const char* apiService_HealthStateName(DeviceHealthState_TypeDef);
static const char* apiService_ErrorName(SensorError_TypeDef);
static const char* apiService_BoolText(BaseType_t);
static void apiService_FormatModes(char*, size_t, uint8_t);
static int apiService_FormatCentiDegrees(char*, size_t, int32_t);
static int apiService_FormatMilliPercent(char*, size_t, uint32_t);
static void apiService_FormatThreshold(char*, size_t, uint8_t, const Threshold_TypeDef*);
static ErrorStatus apiService_SendText(const char*);
static ErrorStatus apiService_SendStatusHeader(uint16_t, const char*);
static ErrorStatus apiService_Finish(void);
static void apiService_Respond(uint16_t, const char*, const char*);
static void apiService_HandleHealth(void);
static void apiService_HandleRtc(void);
static void apiService_HandleSensorsList(void);
static void apiService_HandleSensorDetail(uint8_t);
static void apiService_HandleTemperatureList(void);
static void apiService_HandleBuzzerTest(void);
static void apiService_HandleThresholdsList(void);
static void apiService_HandleThresholdPut(uint8_t, const uint8_t*, uint16_t);




// -------------------------------------------------------------
void ApiService_Init(void) {
  static StaticTask_t taskControlBlock;
  static StackType_t taskStack[512];

  TaskHandle_t task = xTaskCreateStatic(
    apiService_Task,
    "API",
    512,
    NULL,
    configMAX_PRIORITIES - 3U,
    taskStack,
    &taskControlBlock
  );
  if (task != NULL) {
    HealthService_Register(HEALTH_COMPONENT_API);
  } else {
    HealthService_LatchFailure();
  }
}




// -------------------------------------------------------------
static void apiService_Task(void* parameters) {
  (void)parameters;

  while (1) {
    apiService_Run();
    HealthService_Report(HEALTH_COMPONENT_API);
    vTaskDelay(1U);
  }
}




// -------------------------------------------------------------
static void apiService_Run(void) {
  if (W5500_IsReady() != pdTRUE) return;
  switch (getSn_SR(API_SOCKET)) {
    case SOCK_ESTABLISHED:
      if ((getSn_IR(API_SOCKET) & Sn_IR_CON) != 0U) {
        setSn_IR(API_SOCKET, Sn_IR_CON);
        apiService_ResetRequest();
      }
      apiService_Receive();
      break;

    case SOCK_CLOSE_WAIT:
      if (getSn_RX_RSR(API_SOCKET) > 0U) {
        apiService_Receive();
      }
      if (getSn_SR(API_SOCKET) == SOCK_CLOSE_WAIT) {
        (void)disconnect(API_SOCKET);
        apiService_ResetRequest();
      }
      break;

    case SOCK_INIT:
      if (listen(API_SOCKET) != SOCK_OK) {
        (void)close(API_SOCKET);
      }
      break;

    case SOCK_CLOSED:
      apiService_ResetRequest();
      if (socket(API_SOCKET, Sn_MR_TCP, API_PORT, 0x00) != API_SOCKET) {
        (void)close(API_SOCKET);
      }
      break;

    default:
      break;
  }
}




// -------------------------------------------------------------
static void apiService_ResetRequest(void) {
  requestLength = 0U;
  headersParsed = pdFALSE;
  headerEnd = 0U;
  contentLength = 0U;
  requestMethod = API_METHOD_UNKNOWN;
  requestPathStart = 0U;
  requestPathLength = 0U;
}




// -------------------------------------------------------------
static void apiService_Receive(void) {
  uint16_t receivedSize = getSn_RX_RSR(API_SOCKET);
  if (receivedSize == 0U) return;

  uint16_t available = API_REQUEST_BUFFER_SIZE - requestLength;
  uint16_t readSize = (receivedSize < available) ? receivedSize : available;
  if (readSize == 0U) {
    apiService_Respond(400U, "Bad Request", "{\"error\":\"request_too_large\"}");
    apiService_ResetRequest();
    return;
  }

  int32_t result = recv(API_SOCKET, &requestBuffer[requestLength], readSize);
  if (result <= 0) {
    (void)close(API_SOCKET);
    apiService_ResetRequest();
    return;
  }
  requestLength += (uint16_t)result;

  if (headersParsed != pdTRUE) {
    uint16_t end;
    if (apiService_LocateHeaderEnd(&end) == SUCCESS) {
      headerEnd = end;
      headersParsed = pdTRUE;
      if (apiService_ParseRequestLine() != SUCCESS) {
        apiService_Respond(400U, "Bad Request", "{\"error\":\"malformed_request\"}");
        apiService_ResetRequest();
        return;
      }
      if (apiService_ParseContentLength() != SUCCESS) {
        apiService_Respond(400U, "Bad Request", "{\"error\":\"invalid_content_length\"}");
        apiService_ResetRequest();
        return;
      }

      uint16_t bodyCapacity = API_REQUEST_BUFFER_SIZE - headerEnd;
      if (contentLength > bodyCapacity) {
        apiService_Respond(400U, "Bad Request", "{\"error\":\"request_too_large\"}");
        apiService_ResetRequest();
        return;
      }
    }
  }

  if (headersParsed == pdTRUE) {
    uint16_t bodyReceived = requestLength - headerEnd;
    if (bodyReceived >= (uint16_t)contentLength) {
      apiService_Dispatch();
      apiService_ResetRequest();
    }
  }
}




// -------------------------------------------------------------
static ErrorStatus apiService_LocateHeaderEnd(uint16_t* endOffset) {
  for (uint16_t i = 3U; i < requestLength; i++) {
    if ((requestBuffer[i - 3U] == '\r') && (requestBuffer[i - 2U] == '\n')
        && (requestBuffer[i - 1U] == '\r') && (requestBuffer[i] == '\n')) {
      *endOffset = i + 1U;
      return (SUCCESS);
    }
  }
  return (ERROR);
}




// -------------------------------------------------------------
static ErrorStatus apiService_ParseRequestLine(void) {
  uint16_t lineEnd = 0U;
  BaseType_t found = pdFALSE;
  for (uint16_t i = 0U; (i + 1U) < requestLength; i++) {
    if ((requestBuffer[i] == '\r') && (requestBuffer[i + 1U] == '\n')) {
      lineEnd = i;
      found = pdTRUE;
      break;
    }
  }
  if (found != pdTRUE) return (ERROR);

  uint16_t methodEnd = 0U;
  for (uint16_t i = 0U; i < lineEnd; i++) {
    if (requestBuffer[i] == ' ') { methodEnd = i; break; }
  }
  if (methodEnd == 0U) return (ERROR);

  if ((methodEnd == 3U) && (memcmp(requestBuffer, "GET", 3U) == 0)) {
    requestMethod = API_METHOD_GET;
  } else if ((methodEnd == 4U) && (memcmp(requestBuffer, "POST", 4U) == 0)) {
    requestMethod = API_METHOD_POST;
  } else if ((methodEnd == 3U) && (memcmp(requestBuffer, "PUT", 3U) == 0)) {
    requestMethod = API_METHOD_PUT;
  } else if ((methodEnd == 4U) && (memcmp(requestBuffer, "HEAD", 4U) == 0)) {
    requestMethod = API_METHOD_HEAD;
  } else {
    requestMethod = API_METHOD_UNKNOWN;
  }

  uint16_t pathStart = methodEnd + 1U;
  uint16_t pathEnd = pathStart;
  for (uint16_t i = pathStart; i < lineEnd; i++) {
    if (requestBuffer[i] == ' ') break;
    pathEnd = i + 1U;
  }
  if (pathEnd <= pathStart) return (ERROR);

  requestPathStart = pathStart;
  requestPathLength = pathEnd - pathStart;
  return (SUCCESS);
}




// -------------------------------------------------------------
static BaseType_t apiService_HeaderNameIs(
  const uint8_t* line,
  uint16_t lineLength,
  const char* name
) {
  size_t nameLength = strlen(name);
  if (lineLength < nameLength) return (pdFALSE);

  for (size_t i = 0U; i < nameLength; i++) {
    uint8_t a = line[i];
    char b = name[i];
    if ((a >= 'a') && (a <= 'z')) a = (uint8_t)(a - 'a' + 'A');
    if ((b >= 'a') && (b <= 'z')) b = (char)(b - 'a' + 'A');
    if (a != (uint8_t)b) return (pdFALSE);
  }
  return (pdTRUE);
}




// -------------------------------------------------------------
static ErrorStatus apiService_ParseContentLength(void) {
  static const char headerName[] = "Content-Length:";
  size_t headerNameLength = strlen(headerName);

  contentLength = 0U;
  BaseType_t found = pdFALSE;
  uint16_t lineStart = 0U;
  for (uint16_t i = 0U; (i + 1U) < headerEnd; i++) {
    if ((requestBuffer[i] == '\r') && (requestBuffer[i + 1U] == '\n')) {
      uint16_t lineLength = i - lineStart;
      if (apiService_HeaderNameIs(&requestBuffer[lineStart], lineLength, headerName) == pdTRUE) {
        if (found == pdTRUE) return (ERROR);
        found = pdTRUE;
        uint16_t cursor = lineStart + (uint16_t)headerNameLength;
        while ((cursor < i)
            && ((requestBuffer[cursor] == ' ') || (requestBuffer[cursor] == '\t'))) {
          cursor++;
        }
        if ((cursor >= i) || (requestBuffer[cursor] < '0') || (requestBuffer[cursor] > '9')) {
          return (ERROR);
        }
        uint32_t value = 0U;
        while ((cursor < i) && (requestBuffer[cursor] >= '0') && (requestBuffer[cursor] <= '9')) {
          uint32_t digit = (uint32_t)(requestBuffer[cursor] - '0');
          if (value > ((UINT32_MAX - digit) / 10U)) return (ERROR);
          value = (value * 10U) + digit;
          cursor++;
        }
        while ((cursor < i)
            && ((requestBuffer[cursor] == ' ') || (requestBuffer[cursor] == '\t'))) {
          cursor++;
        }
        if (cursor != i) return (ERROR);
        contentLength = value;
      }
      lineStart = i + 2U;
      i++;
    }
  }
  return (SUCCESS);
}




// -------------------------------------------------------------
static void apiService_Dispatch(void) {
  const uint8_t* path = &requestBuffer[requestPathStart];
  uint16_t pathLength = requestPathLength;
  const uint8_t* body = &requestBuffer[headerEnd];
  uint16_t bodyLength = (uint16_t)contentLength;
  uint8_t index;

  if (((requestMethod == API_METHOD_GET) || (requestMethod == API_METHOD_HEAD))
      && (apiService_PathIs(path, pathLength, "/health") == pdTRUE)) {
    apiService_HandleHealth();
  } else if ((requestMethod == API_METHOD_GET) && (apiService_PathIs(path, pathLength, "/api/v1/rtc") == pdTRUE)) {
    apiService_HandleRtc();
  } else if ((requestMethod == API_METHOD_GET) && (apiService_PathIs(path, pathLength, "/api/v1/sensors") == pdTRUE)) {
    apiService_HandleSensorsList();
  } else if ((requestMethod == API_METHOD_GET)
      && (apiService_PathIndex(path, pathLength, "/api/v1/sensors/", &index) == SUCCESS)) {
    apiService_HandleSensorDetail(index);
  } else if ((requestMethod == API_METHOD_GET) && (apiService_PathIs(path, pathLength, "/api/v1/temperature") == pdTRUE)) {
    apiService_HandleTemperatureList();
  } else if ((requestMethod == API_METHOD_POST) && (apiService_PathIs(path, pathLength, "/api/v1/buzzer/test") == pdTRUE)) {
    apiService_HandleBuzzerTest();
  } else if ((requestMethod == API_METHOD_GET) && (apiService_PathIs(path, pathLength, "/api/v1/thresholds") == pdTRUE)) {
    apiService_HandleThresholdsList();
  } else if ((requestMethod == API_METHOD_PUT)
      && (apiService_PathIndex(path, pathLength, "/api/v1/thresholds/", &index) == SUCCESS)) {
    apiService_HandleThresholdPut(index, body, bodyLength);
  } else {
    apiService_Respond(404U, "Not Found", "{\"error\":\"not_found\"}");
  }
}




// -------------------------------------------------------------
static BaseType_t apiService_PathIs(
  const uint8_t* path,
  uint16_t pathLength,
  const char* literal
) {
  size_t literalLength = strlen(literal);
  if (pathLength != literalLength) return (pdFALSE);
  return (memcmp(path, literal, literalLength) == 0) ? pdTRUE : pdFALSE;
}




// -------------------------------------------------------------
static ErrorStatus apiService_ParseIndex(
  const uint8_t* text,
  uint16_t length,
  uint8_t* value
) {
  if ((length == 0U) || (length > 3U) || (value == NULL)) return (ERROR);

  uint16_t parsed = 0U;
  for (uint16_t i = 0U; i < length; i++) {
    if ((text[i] < '0') || (text[i] > '9')) return (ERROR);
    parsed = (uint16_t)(parsed * 10U + (uint16_t)(text[i] - '0'));
    if (parsed > 255U) return (ERROR);
  }
  if (parsed == 0U) return (ERROR);

  *value = (uint8_t)parsed;
  return (SUCCESS);
}




// -------------------------------------------------------------
static ErrorStatus apiService_PathIndex(
  const uint8_t* path,
  uint16_t pathLength,
  const char* prefix,
  uint8_t* index
) {
  size_t prefixLength = strlen(prefix);
  if ((pathLength <= prefixLength) || (memcmp(path, prefix, prefixLength) != 0)) {
    return (ERROR);
  }
  return apiService_ParseIndex(&path[prefixLength], pathLength - (uint16_t)prefixLength, index);
}




// -------------------------------------------------------------
static ErrorStatus apiService_ParseJsonNumber(
  const uint8_t* body,
  uint16_t length,
  const char* key,
  uint32_t scale,
  int32_t* value
) {
  char needle[24];
  (void)snprintf(needle, sizeof(needle), "\"%s\"", key);
  size_t needleLength = strlen(needle);
  uint8_t maxFractionDigits = (scale == 100U) ? 2U : (scale == 10U) ? 1U : 0U;

  for (uint16_t i = 0U; (i + needleLength) <= length; i++) {
    if (memcmp(&body[i], needle, needleLength) != 0) continue;

    uint16_t cursor = i + (uint16_t)needleLength;
    while ((cursor < length) && ((body[cursor] == ' ') || (body[cursor] == ':'))) cursor++;

    BaseType_t negative = pdFALSE;
    if ((cursor < length) && (body[cursor] == '-')) { negative = pdTRUE; cursor++; }
    if ((cursor >= length) || (body[cursor] < '0') || (body[cursor] > '9')) return (ERROR);

    uint32_t whole = 0U;
    while ((cursor < length) && (body[cursor] >= '0') && (body[cursor] <= '9')) {
      uint32_t digit = (uint32_t)(body[cursor] - '0');
      if (whole > ((UINT32_MAX - digit) / 10U)) return (ERROR);
      whole = (whole * 10U) + digit;
      cursor++;
    }

    int32_t fraction = 0;
    uint8_t fractionDigits = 0U;
    if ((cursor < length) && (body[cursor] == '.')) {
      cursor++;
      while ((cursor < length) && (body[cursor] >= '0') && (body[cursor] <= '9')) {
        if (fractionDigits < maxFractionDigits) {
          fraction = (fraction * 10) + (body[cursor] - '0');
          fractionDigits++;
        }
        cursor++;
      }
      while (fractionDigits < maxFractionDigits) { fraction *= 10; fractionDigits++; }
    }

    uint32_t fractionValue = (uint32_t)fraction;
    uint32_t limit = (negative == pdTRUE) ? ((uint32_t)INT32_MAX + 1U) : (uint32_t)INT32_MAX;
    if ((whole > (limit / scale))
        || ((whole == (limit / scale)) && (fractionValue > (limit % scale)))) {
      return (ERROR);
    }
    uint32_t magnitude = (whole * scale) + fractionValue;
    if ((negative == pdTRUE) && (magnitude == ((uint32_t)INT32_MAX + 1U))) {
      *value = INT32_MIN;
    } else {
      int32_t signedMagnitude = (int32_t)magnitude;
      *value = (negative == pdTRUE) ? -signedMagnitude : signedMagnitude;
    }
    return (SUCCESS);
  }
  return (ERROR);
}




// -------------------------------------------------------------
static ErrorStatus apiService_ParseJsonBool(
  const uint8_t* body,
  uint16_t length,
  const char* key,
  BaseType_t* value
) {
  char needle[24];
  (void)snprintf(needle, sizeof(needle), "\"%s\"", key);
  size_t needleLength = strlen(needle);

  for (uint16_t i = 0U; (i + needleLength) <= length; i++) {
    if (memcmp(&body[i], needle, needleLength) != 0) continue;

    uint16_t cursor = i + (uint16_t)needleLength;
    while ((cursor < length) && ((body[cursor] == ' ') || (body[cursor] == ':'))) cursor++;

    if (((size_t)(length - cursor) >= 4U) && (memcmp(&body[cursor], "true", 4U) == 0)) {
      *value = pdTRUE;
      return (SUCCESS);
    }
    if (((size_t)(length - cursor) >= 5U) && (memcmp(&body[cursor], "false", 5U) == 0)) {
      *value = pdFALSE;
      return (SUCCESS);
    }
    return (ERROR);
  }
  return (ERROR);
}




// -------------------------------------------------------------
static const char* apiService_ModelName(SensorModel_TypeDef model) {
  switch (model) {
    case SENSOR_MODEL_DS18B20: return ("DS18B20");
    default:                   return ("unknown");
  }
}




// -------------------------------------------------------------
static const char* apiService_HealthStateName(DeviceHealthState_TypeDef health) {
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
static const char* apiService_ErrorName(SensorError_TypeDef error) {
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
static const char* apiService_BoolText(BaseType_t condition) {
  return (condition != pdFALSE) ? "true" : "false";
}




// -------------------------------------------------------------
static void apiService_FormatModes(char* buffer, size_t capacity, uint8_t capabilities) {
  size_t used = 0U;
  BaseType_t first = pdTRUE;

  used += (size_t)snprintf(&buffer[used], capacity - used, "[");
  if ((capabilities & SENSOR_CAPABILITY_TEMPERATURE) != 0U) {
    used += (size_t)snprintf(&buffer[used], capacity - used, "%s\"temperature\"", first ? "" : ",");
    first = pdFALSE;
  }
  if ((capabilities & SENSOR_CAPABILITY_PRESSURE) != 0U) {
    used += (size_t)snprintf(&buffer[used], capacity - used, "%s\"pressure\"", first ? "" : ",");
    first = pdFALSE;
  }
  if ((capabilities & SENSOR_CAPABILITY_HUMIDITY) != 0U) {
    used += (size_t)snprintf(&buffer[used], capacity - used, "%s\"humidity\"", first ? "" : ",");
    first = pdFALSE;
  }
  (void)snprintf(&buffer[used], capacity - used, "]");
}




// -------------------------------------------------------------
static int apiService_FormatCentiDegrees(char* buffer, size_t capacity, int32_t centiDegrees) {
  uint32_t magnitude = (centiDegrees < 0)
    ? (uint32_t)(-centiDegrees)
    : (uint32_t)centiDegrees;
  return snprintf(
    buffer,
    capacity,
    "%s%lu.%02lu",
    (centiDegrees < 0) ? "-" : "",
    (unsigned long)(magnitude / 100U),
    (unsigned long)(magnitude % 100U)
  );
}




// -------------------------------------------------------------
static int apiService_FormatMilliPercent(char* buffer, size_t capacity, uint32_t milliPercent) {
  return snprintf(
    buffer,
    capacity,
    "%lu.%03lu",
    (unsigned long)(milliPercent / 1000U),
    (unsigned long)(milliPercent % 1000U)
  );
}




// -------------------------------------------------------------
static void apiService_FormatThreshold(
  char* buffer,
  size_t capacity,
  uint8_t index,
  const Threshold_TypeDef* threshold
) {
  char temperatureField[24];
  (void)apiService_FormatCentiDegrees(temperatureField, sizeof(temperatureField), threshold->temperature);
  (void)snprintf(
    buffer,
    capacity,
    "{\"index\":%u,\"enabled\":%s,\"sensor_index\":%u,\"temperature\":%s,\"tone_hz\":%lu,\"triggered\":%s}",
    index,
    apiService_BoolText(threshold->enabled),
    threshold->sensorIndex,
    temperatureField,
    (unsigned long)threshold->toneHz,
    apiService_BoolText(threshold->triggered)
  );
}




// -------------------------------------------------------------
static ErrorStatus apiService_SendText(const char* text) {
  uint16_t length = (uint16_t)strlen(text);
  uint16_t sent = 0U;

  while (sent < length) {
    int32_t result = send(API_SOCKET, (uint8_t*)&text[sent], length - sent);
    if (result <= 0) {
      (void)close(API_SOCKET);
      return (ERROR);
    }
    sent += (uint16_t)result;
  }
  return (SUCCESS);
}




// -------------------------------------------------------------
static ErrorStatus apiService_SendStatusHeader(uint16_t statusCode, const char* statusText) {
  char header[100];
  (void)snprintf(
    header,
    sizeof(header),
    "HTTP/1.1 %u %s\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n",
    statusCode,
    statusText
  );
  return apiService_SendText(header);
}




// -------------------------------------------------------------
static ErrorStatus apiService_Finish(void) {
  TickType_t start = xTaskGetTickCount();
  TickType_t timeout = pdMS_TO_TICKS(1000U);

  while ((getSn_IR(API_SOCKET) & Sn_IR_SENDOK) == 0U) {
    uint8_t status = getSn_SR(API_SOCKET);
    if ((status != SOCK_ESTABLISHED) && (status != SOCK_CLOSE_WAIT)) {
      (void)close(API_SOCKET);
      return (ERROR);
    }
    if ((xTaskGetTickCount() - start) >= timeout) {
      (void)close(API_SOCKET);
      return (ERROR);
    }
    vTaskDelay(1U);
  }

  setSn_IR(API_SOCKET, Sn_IR_SENDOK);
  (void)disconnect(API_SOCKET);
  return (SUCCESS);
}




// -------------------------------------------------------------
static void apiService_Respond(uint16_t statusCode, const char* statusText, const char* jsonBody) {
  if ((apiService_SendStatusHeader(statusCode, statusText) == SUCCESS)
      && (requestMethod != API_METHOD_HEAD)) {
    (void)apiService_SendText(jsonBody);
  }
  (void)apiService_Finish();
}




// -------------------------------------------------------------
static void apiService_HandleHealth(void) {
  char body[320];
  BaseType_t latched = HealthService_IsLatched();
  const char* status = (latched == pdTRUE)
    ? "failed"
    : ((peripheralReadiness != 0U) ? "degraded" : "ok");

  (void)snprintf(
    body,
    sizeof(body),
    "{\"status\":\"%s\",\"uptime_ms\":%lu,\"watchdog_latched\":%s,"
    "\"peripherals\":{"
      "\"heartbeat_led\":%s,\"usart1\":%s,\"onewire\":%s,\"eth_spi\":%s,"
      "\"display\":%s,\"buzzer\":%s,\"rtc\":%s"
    "}}",
    status,
    (unsigned long)(xTaskGetTickCount() * portTICK_PERIOD_MS),
    apiService_BoolText(latched),
    apiService_BoolText(!FLAG_CHECK(peripheralReadiness, PERIPHERAL_HEARTBEAT_LED_ERROR_BIT)),
    apiService_BoolText(!FLAG_CHECK(peripheralReadiness, PERIPHERAL_USART1_ERROR_BIT)),
    apiService_BoolText(!FLAG_CHECK(peripheralReadiness, PERIPHERAL_ONEWIRE_ERROR_BIT)),
    apiService_BoolText(!FLAG_CHECK(peripheralReadiness, PERIPHERAL_SPI1_ERROR_BIT)),
    apiService_BoolText(!FLAG_CHECK(peripheralReadiness, PERIPHERAL_WH_DISPLAY_ERROR_BIT)),
    apiService_BoolText(!FLAG_CHECK(peripheralReadiness, PERIPHERAL_BUZZER_ERROR_BIT)),
    apiService_BoolText(!FLAG_CHECK(peripheralReadiness, PERIPHERAL_RTC_ERROR_BIT))
  );
  apiService_Respond(200U, "OK", body);
}




// -------------------------------------------------------------
static void apiService_HandleRtc(void) {
  RtcStatus_TypeDef rtcStatus;
  RtcService_GetStatus(&rtcStatus);

  char iso8601[32];
  RtcService_FormatIso8601(rtcStatus.unixTime, iso8601, sizeof(iso8601));

  char lastSyncField[16] = "null";
  if (rtcStatus.synchronized == pdTRUE) {
    (void)snprintf(lastSyncField, sizeof(lastSyncField), "%lu", (unsigned long)rtcStatus.lastSyncAgeMs);
  }

  char body[160];
  (void)snprintf(
    body,
    sizeof(body),
    "{\"unix_time\":%lu,\"iso8601\":\"%s\",\"synchronized\":%s,"
    "\"last_sync_age_ms\":%s,\"sync_failures\":%u}",
    (unsigned long)rtcStatus.unixTime,
    iso8601,
    apiService_BoolText(rtcStatus.synchronized),
    lastSyncField,
    rtcStatus.consecutiveFailures
  );
  apiService_Respond(200U, "OK", body);
}




// -------------------------------------------------------------
static void apiService_HandleSensorsList(void) {
  SensorCounts_TypeDef counts;
  TemperatureSensorService_GetCounts(&counts);

  if (apiService_SendStatusHeader(200U, "OK") != SUCCESS) return;

  char fragment[160];
  (void)snprintf(fragment, sizeof(fragment), "{\"count\":%u,\"sensors\":[", counts.all);
  if (apiService_SendText(fragment) != SUCCESS) { (void)apiService_Finish(); return; }

  BaseType_t first = pdTRUE;
  for (uint8_t i = 1U; i <= counts.all; i++) {
    SensorSnapshot_TypeDef snapshot;
    if (TemperatureSensorService_GetSnapshot(i, &snapshot) != SUCCESS) continue;

    char modes[64];
    apiService_FormatModes(modes, sizeof(modes), snapshot.capabilities);
    (void)snprintf(
      fragment,
      sizeof(fragment),
      "%s{\"index\":%u,\"model\":\"%s\",\"modes\":%s,"
      "\"serial\":\"%02X%02X%02X%02X%02X%02X%02X%02X\",\"status\":\"%s\"}",
      first ? "" : ",",
      i,
      apiService_ModelName(snapshot.model),
      modes,
      snapshot.identity[0], snapshot.identity[1], snapshot.identity[2], snapshot.identity[3],
      snapshot.identity[4], snapshot.identity[5], snapshot.identity[6], snapshot.identity[7],
      apiService_HealthStateName(snapshot.health.state)
    );
    first = pdFALSE;
    if (apiService_SendText(fragment) != SUCCESS) break;
  }
  (void)apiService_SendText("]}");
  (void)apiService_Finish();
}




// -------------------------------------------------------------
static void apiService_HandleSensorDetail(uint8_t index) {
  SensorSnapshot_TypeDef snapshot;
  if (TemperatureSensorService_GetSnapshot(index, &snapshot) != SUCCESS) {
    apiService_Respond(404U, "Not Found", "{\"error\":\"sensor_not_found\"}");
    return;
  }

  char modes[64];
  apiService_FormatModes(modes, sizeof(modes), snapshot.capabilities);

  char ageField[24] = "null";
  if (snapshot.health.lastSuccess != 0U) {
    TickType_t age = xTaskGetTickCount() - snapshot.health.lastSuccess;
    (void)snprintf(ageField, sizeof(ageField), "%lu", (unsigned long)(age * portTICK_PERIOD_MS));
  }

  char temperatureField[24] = "null";
  char pressureField[16] = "null";
  char humidityField[24] = "null";
  if (snapshot.dataValid == pdTRUE) {
    if ((snapshot.capabilities & SENSOR_CAPABILITY_TEMPERATURE) != 0U) {
      (void)apiService_FormatCentiDegrees(temperatureField, sizeof(temperatureField), snapshot.temperature);
    }
    if ((snapshot.capabilities & SENSOR_CAPABILITY_PRESSURE) != 0U) {
      (void)snprintf(pressureField, sizeof(pressureField), "%lu", (unsigned long)snapshot.pressure);
    }
    if ((snapshot.capabilities & SENSOR_CAPABILITY_HUMIDITY) != 0U) {
      (void)apiService_FormatMilliPercent(humidityField, sizeof(humidityField), snapshot.humidity);
    }
  }

  char body[384];
  (void)snprintf(
    body,
    sizeof(body),
    "{\"index\":%u,\"model\":\"%s\",\"modes\":%s,"
    "\"serial\":\"%02X%02X%02X%02X%02X%02X%02X%02X\","
    "\"status\":\"%s\",\"age_ms\":%s,\"failures\":%u,\"error\":\"%s\","
    "\"temperature\":%s,\"pressure\":%s,\"humidity\":%s}",
    index,
    apiService_ModelName(snapshot.model),
    modes,
    snapshot.identity[0], snapshot.identity[1], snapshot.identity[2], snapshot.identity[3],
    snapshot.identity[4], snapshot.identity[5], snapshot.identity[6], snapshot.identity[7],
    apiService_HealthStateName(snapshot.health.state),
    ageField,
    snapshot.health.consecutiveFailures,
    apiService_ErrorName((SensorError_TypeDef)snapshot.health.lastError),
    temperatureField,
    pressureField,
    humidityField
  );
  apiService_Respond(200U, "OK", body);
}




// -------------------------------------------------------------
static void apiService_HandleTemperatureList(void) {
  SensorCounts_TypeDef counts;
  TemperatureSensorService_GetCounts(&counts);

  if (apiService_SendStatusHeader(200U, "OK") != SUCCESS) return;

  char fragment[160];
  (void)snprintf(fragment, sizeof(fragment), "{\"count\":%u,\"temperatures\":[", counts.all);
  if (apiService_SendText(fragment) != SUCCESS) { (void)apiService_Finish(); return; }

  BaseType_t first = pdTRUE;
  for (uint8_t i = 1U; i <= counts.all; i++) {
    SensorSnapshot_TypeDef snapshot;
    if (TemperatureSensorService_GetSnapshot(i, &snapshot) != SUCCESS) continue;

    char temperatureField[24] = "null";
    BaseType_t available = pdFALSE;
    if ((snapshot.dataValid == pdTRUE) && ((snapshot.capabilities & SENSOR_CAPABILITY_TEMPERATURE) != 0U)) {
      (void)apiService_FormatCentiDegrees(temperatureField, sizeof(temperatureField), snapshot.temperature);
      available = pdTRUE;
    }

    (void)snprintf(
      fragment,
      sizeof(fragment),
      "%s{\"index\":%u,\"available\":%s,\"temperature\":%s}",
      first ? "" : ",",
      i,
      apiService_BoolText(available),
      temperatureField
    );
    first = pdFALSE;
    if (apiService_SendText(fragment) != SUCCESS) break;
  }
  (void)apiService_SendText("]}");
  (void)apiService_Finish();
}




// -------------------------------------------------------------
static void apiService_HandleBuzzerTest(void) {
  if (Buzzer_SelfTestWithTimeout(pdMS_TO_TICKS(API_BUZZER_WAIT_MS)) == SUCCESS) {
    apiService_Respond(200U, "OK", "{\"result\":\"ok\"}");
  } else {
    apiService_Respond(503U, "Service Unavailable", "{\"error\":\"buzzer_busy\"}");
  }
}




// -------------------------------------------------------------
static void apiService_HandleThresholdsList(void) {
  if (apiService_SendStatusHeader(200U, "OK") != SUCCESS) return;

  char fragment[128];
  (void)snprintf(
    fragment,
    sizeof(fragment),
    "{\"count\":%u,\"thresholds\":[",
    THRESHOLD_SERVICE_MAX_THRESHOLDS
  );
  if (apiService_SendText(fragment) != SUCCESS) { (void)apiService_Finish(); return; }

  BaseType_t first = pdTRUE;
  for (uint8_t i = 1U; i <= THRESHOLD_SERVICE_MAX_THRESHOLDS; i++) {
    Threshold_TypeDef threshold;
    if (ThresholdService_Get(i, &threshold) != SUCCESS) continue;

    apiService_FormatThreshold(fragment, sizeof(fragment), i, &threshold);
    if (first != pdTRUE) {
      if (apiService_SendText(",") != SUCCESS) break;
    }
    first = pdFALSE;
    if (apiService_SendText(fragment) != SUCCESS) break;
  }
  (void)apiService_SendText("]}");
  (void)apiService_Finish();
}




// -------------------------------------------------------------
static void apiService_HandleThresholdPut(
  uint8_t index,
  const uint8_t* body,
  uint16_t bodyLength
) {
  int32_t sensorIndexValue = 0;
  int32_t temperatureValue = 0;
  int32_t toneHzValue = 0;
  BaseType_t enabledValue = pdTRUE;

  if ((apiService_ParseJsonNumber(body, bodyLength, "sensor_index", 1U, &sensorIndexValue) != SUCCESS)
      || (apiService_ParseJsonNumber(body, bodyLength, "temperature", 100U, &temperatureValue) != SUCCESS)
      || (apiService_ParseJsonNumber(body, bodyLength, "tone_hz", 1U, &toneHzValue) != SUCCESS)) {
    apiService_Respond(400U, "Bad Request", "{\"error\":\"missing_or_invalid_field\"}");
    return;
  }
  (void)apiService_ParseJsonBool(body, bodyLength, "enabled", &enabledValue);

  if ((sensorIndexValue <= 0) || (sensorIndexValue > 255)
      || (toneHzValue <= 0) || (toneHzValue > 65535)) {
    apiService_Respond(400U, "Bad Request", "{\"error\":\"value_out_of_range\"}");
    return;
  }

  Threshold_TypeDef stored;
  if (ThresholdService_Set(
        index,
        (uint8_t)sensorIndexValue,
        temperatureValue,
        (uint32_t)toneHzValue,
        enabledValue,
        &stored
      ) != SUCCESS) {
    apiService_Respond(400U, "Bad Request", "{\"error\":\"invalid_threshold\"}");
    return;
  }

  char body2[160];
  apiService_FormatThreshold(body2, sizeof(body2), index, &stored);
  apiService_Respond(200U, "OK", body2);
}
