/**
  ******************************************************************************
  * @file           : rtc_service.c
  * @brief          : RTC reporting and periodic NTP synchronization.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "rtc_service.h"
#include "rtc.h"
#include "wizchip_port.h"
#include "DNS/dns.h"
#include "socket.h"
#include <string.h>

#define NTP_SERVER_HOST         "pool.ntp.org"
#define NTP_PORT                 123U
#define NTP_LOCAL_PORT           123U
#define NTP_SOCKET                 4U
#define NTP_PACKET_SIZE            48U
#define NTP_UNIX_EPOCH_OFFSET   2208988800UL
#define NTP_RESPONSE_TIMEOUT_MS 5000U
#define RTC_REPORT_PERIOD_MS   60000U
#define NTP_SYNC_PERIOD_TICKS  (3600000UL / RTC_REPORT_PERIOD_MS) /* one hour */

static TickType_t lastSyncSuccess;
static uint16_t consecutiveFailures;
static BaseType_t synchronized;
static uint32_t requestSequence;

static void rtcService_Task(void*);
static void rtcService_DnsTimer(TimerHandle_t);
static ErrorStatus rtcService_Sync(void);
static ErrorStatus rtcService_ResolveServer(uint8_t*);
static ErrorStatus rtcService_RequestTime(const uint8_t*, uint32_t*);
static void rtcService_RecordResult(ErrorStatus);
static void rtcService_PrintTime(void);
static void rtcService_CivilFromDays(int64_t, int32_t*, int32_t*, int32_t*);




// -------------------------------------------------------------
void RtcService_Init(void) {
  static StaticTask_t taskControlBlock;
  static StackType_t taskStack[256];
  static StaticTimer_t dnsTimerStorage;
  TimerHandle_t dnsTimer;

  if (RTC_Init() != SUCCESS) {
    FLAG_SET(peripheralReadiness, PERIPHERAL_RTC_ERROR_BIT);
  }

  dnsTimer = xTimerCreateStatic(
    "RTC DNS tick",
    pdMS_TO_TICKS(1000U),
    pdTRUE,
    NULL,
    rtcService_DnsTimer,
    &dnsTimerStorage
  );
  if ((dnsTimer == NULL) || (xTimerStart(dnsTimer, 0U) != pdPASS)) {
    FLAG_SET(peripheralReadiness, PERIPHERAL_RTC_ERROR_BIT);
  }

  TaskHandle_t task = xTaskCreateStatic(
    rtcService_Task,
    "RTC",
    256,
    NULL,
    configMAX_PRIORITIES - 3U,
    taskStack,
    &taskControlBlock
  );
  if (task != NULL) {
    HealthService_Register(HEALTH_COMPONENT_RTC);
  } else {
    HealthService_LatchFailure();
  }
}




// -------------------------------------------------------------
void RtcService_GetStatus(RtcStatus_TypeDef* status) {
  if (status == NULL) return;

  taskENTER_CRITICAL();
  status->synchronized = synchronized;
  status->consecutiveFailures = consecutiveFailures;
  status->lastSyncAgeMs = (synchronized == pdTRUE)
    ? (uint32_t)((xTaskGetTickCount() - lastSyncSuccess) * portTICK_PERIOD_MS)
    : 0U;
  taskEXIT_CRITICAL();

  status->unixTime = RTC_GetUnixTime();
}




// -------------------------------------------------------------
void RtcService_FormatIso8601(uint32_t unixTime, char* buffer, size_t capacity) {
  int32_t year;
  int32_t month;
  int32_t day;
  uint32_t secondsOfDay = unixTime % 86400UL;

  rtcService_CivilFromDays((int64_t)(unixTime / 86400UL), &year, &month, &day);

  (void)snprintf(
    buffer,
    capacity,
    "%04ld-%02ld-%02ldT%02lu:%02lu:%02luZ",
    (long)year,
    (long)month,
    (long)day,
    (unsigned long)(secondsOfDay / 3600U),
    (unsigned long)((secondsOfDay % 3600U) / 60U),
    (unsigned long)(secondsOfDay % 60U)
  );
}




// -------------------------------------------------------------
static void rtcService_Task(void* parameters) {
  (void)parameters;
  TickType_t lastWakeTime;
  uint32_t ticksSinceSync = 0U;

  if (!FLAG_CHECK(peripheralReadiness, PERIPHERAL_SPI1_ERROR_BIT)) {
    (void)rtcService_Sync();
  }
  rtcService_PrintTime();
  HealthService_Report(HEALTH_COMPONENT_RTC);

  lastWakeTime = xTaskGetTickCount();
  while (1) {
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(RTC_REPORT_PERIOD_MS));

    ticksSinceSync++;
    if ((ticksSinceSync >= NTP_SYNC_PERIOD_TICKS)
        && !FLAG_CHECK(peripheralReadiness, PERIPHERAL_SPI1_ERROR_BIT)) {
      ticksSinceSync = 0U;
      (void)rtcService_Sync();
    }
    rtcService_PrintTime();
    HealthService_Report(HEALTH_COMPONENT_RTC);
  }
}




// -------------------------------------------------------------
static void rtcService_DnsTimer(TimerHandle_t timer) {
  (void)timer;
  DNS_time_handler();
}




// -------------------------------------------------------------
static ErrorStatus rtcService_Sync(void) {
  uint8_t serverAddress[4];
  uint32_t unixTime;

  if (rtcService_ResolveServer(serverAddress) != SUCCESS) {
    rtcService_RecordResult(ERROR);
    return (ERROR);
  }
  if (rtcService_RequestTime(serverAddress, &unixTime) != SUCCESS) {
    rtcService_RecordResult(ERROR);
    return (ERROR);
  }
  if (RTC_SetUnixTime(unixTime) != SUCCESS) {
    rtcService_RecordResult(ERROR);
    return (ERROR);
  }

  rtcService_RecordResult(SUCCESS);
  printf("NTP: synchronized with %s, unix_time=%lu\n", NTP_SERVER_HOST, (unsigned long)unixTime);
  return (SUCCESS);
}




// -------------------------------------------------------------
static ErrorStatus rtcService_ResolveServer(uint8_t* address) {
  static const uint8_t host[] = NTP_SERVER_HOST;
  uint8_t dnsServer[4];

  W5500_GetDnsServer(dnsServer);
  return (DNS_run(dnsServer, (uint8_t*)host, address) == 1) ? SUCCESS : ERROR;
}




// -------------------------------------------------------------
static ErrorStatus rtcService_RequestTime(const uint8_t* serverAddress, uint32_t* unixTime) {
  uint8_t packet[NTP_PACKET_SIZE];
  ErrorStatus status = ERROR;

  memset(packet, 0, sizeof(packet));
  packet[0] = 0x23U; /* LI=0, VN=4, Mode=3 (client) */
  uint32_t requestTick = xTaskGetTickCount();
  uint32_t requestId = ++requestSequence;
  packet[40] = (uint8_t)(requestTick >> 24U);
  packet[41] = (uint8_t)(requestTick >> 16U);
  packet[42] = (uint8_t)(requestTick >> 8U);
  packet[43] = (uint8_t)requestTick;
  packet[44] = (uint8_t)(requestId >> 24U);
  packet[45] = (uint8_t)(requestId >> 16U);
  packet[46] = (uint8_t)(requestId >> 8U);
  packet[47] = (uint8_t)requestId;
  uint8_t requestTimestamp[8];
  memcpy(requestTimestamp, &packet[40], sizeof(requestTimestamp));

  (void)close(NTP_SOCKET);
  if (socket(NTP_SOCKET, Sn_MR_UDP, NTP_LOCAL_PORT, 0x00) != NTP_SOCKET) {
    return (ERROR);
  }

  if (sendto(NTP_SOCKET, packet, NTP_PACKET_SIZE, (uint8_t*)serverAddress, NTP_PORT)
      != (int32_t)NTP_PACKET_SIZE) {
    (void)close(NTP_SOCKET);
    return (ERROR);
  }

  TickType_t start = xTaskGetTickCount();
  while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(NTP_RESPONSE_TIMEOUT_MS)) {
    if (getSn_RX_RSR(NTP_SOCKET) >= NTP_PACKET_SIZE) {
      uint8_t remoteAddress[4] = {0U};
      uint16_t remotePort = 0U;
      int32_t received = recvfrom(NTP_SOCKET, packet, NTP_PACKET_SIZE, remoteAddress, &remotePort);
      BaseType_t expectedEndpoint = (remotePort == NTP_PORT)
        && (memcmp(remoteAddress, serverAddress, sizeof(remoteAddress)) == 0);
      uint8_t leap = packet[0] >> 6U;
      uint8_t mode = packet[0] & 0x07U;
      uint8_t stratum = packet[1];
      BaseType_t matchesRequest =
        (memcmp(&packet[24], requestTimestamp, sizeof(requestTimestamp)) == 0);
      if ((received == (int32_t)NTP_PACKET_SIZE)
          && (expectedEndpoint == pdTRUE)
          && (matchesRequest == pdTRUE)
          && (leap != 3U)
          && (mode == 4U)
          && (stratum > 0U)
          && (stratum < 16U)) {
        uint32_t ntpSeconds = ((uint32_t)packet[40] << 24)
          | ((uint32_t)packet[41] << 16)
          | ((uint32_t)packet[42] << 8)
          | (uint32_t)packet[43];
        if (ntpSeconds > NTP_UNIX_EPOCH_OFFSET) {
          *unixTime = ntpSeconds - NTP_UNIX_EPOCH_OFFSET;
          status = SUCCESS;
        }
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(50U));
  }

  (void)close(NTP_SOCKET);
  return (status);
}




// -------------------------------------------------------------
static void rtcService_RecordResult(ErrorStatus result) {
  taskENTER_CRITICAL();
  if (result == SUCCESS) {
    lastSyncSuccess = xTaskGetTickCount();
    consecutiveFailures = 0U;
    synchronized = pdTRUE;
  } else if (consecutiveFailures < UINT16_MAX) {
    consecutiveFailures++;
  }
  taskEXIT_CRITICAL();
}




// -------------------------------------------------------------
static void rtcService_PrintTime(void) {
  char buffer[32];
  RtcService_FormatIso8601(RTC_GetUnixTime(), buffer, sizeof(buffer));
  printf("RTC: %s synchronized=%s\n", buffer, (synchronized == pdTRUE) ? "true" : "false");
}




// -------------------------------------------------------------
/**
  * @brief Convert days since 1970-01-01 to a proleptic Gregorian date.
  *
  * Howard Hinnant's civil_from_days algorithm (public domain); see
  * http://howardhinnant.github.io/date_algorithms.html.
  */
static void rtcService_CivilFromDays(
  int64_t z,
  int32_t* year,
  int32_t* month,
  int32_t* day
) {
  z += 719468;
  int64_t era = ((z >= 0) ? z : (z - 146096)) / 146097;
  int64_t doe = z - (era * 146097);                                       /* [0, 146096] */
  int64_t yoe = (doe - (doe / 1460) + (doe / 36524) - (doe / 146096)) / 365; /* [0, 399] */
  int64_t y = yoe + (era * 400);
  int64_t doy = doe - ((365 * yoe) + (yoe / 4) - (yoe / 100));            /* [0, 365] */
  int64_t mp = ((5 * doy) + 2) / 153;                                     /* [0, 11] */
  int64_t d = doy - (((153 * mp) + 2) / 5) + 1;                           /* [1, 31] */
  int64_t m = (mp < 10) ? (mp + 3) : (mp - 9);                            /* [1, 12] */

  *year = (int32_t)(y + ((m <= 2) ? 1 : 0));
  *month = (int32_t)m;
  *day = (int32_t)d;
}
