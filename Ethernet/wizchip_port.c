/**
  ******************************************************************************
  * @file           : wizchip_port.c
  * @brief          : W5500 board integration and supervised network lifecycle.
  ******************************************************************************
  */

#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "DHCP/dhcp.h"
#include "DNS/dns.h"
#include <stdbool.h>
#include <string.h>

#define W5500_DHCP_SOCKET             7U
#define W5500_DNS_SOCKET              6U
#define W5500_PING_SOCKET             5U
#define W5500_INIT_ATTEMPTS           5U
#define W5500_LINK_WAIT_MS         5000U
#define W5500_LINK_POLL_MS          100U
#define W5500_DHCP_RUN_ATTEMPTS      20U
#define W5500_DHCP_STEP_MS          500U
#define W5500_MONITOR_PERIOD_MS    1000U
#define W5500_GATEWAY_PING_PERIOD  60U
#define W5500_PING_ATTEMPTS           2U
#define W5500_PING_TIMEOUT_MS      1000U
#define W5500_RETRY_BACKOFF_MAX_S    60U
#define W5500_DHCP_BUFFER_SIZE      548U
#define W5500_ICMP_PACKET_SIZE       16U
#define W5500_ICMP_ECHO_REQUEST       8U
#define W5500_ICMP_ECHO_REPLY         0U
#define W5500_NTP_HOST             "pool.ntp.org"
#define W5500_IP_DISPLAY_MS       10000U
#define W5500_SKIP_SETTLE_MS         20U

#define W5500_ERROR_MUTEX            -1
#define W5500_ERROR_SPI              -2
#define W5500_ERROR_TASK             -3
#define W5500_ERROR_CHIP             -4

#define W5500_SELECT()  PIN_L(ETH_CS_PORT, ETH_CS_PIN)
#define W5500_RELEASE() PIN_H(ETH_CS_PORT, ETH_CS_PIN)
#define W5500_RESET_L() PIN_L(ETH_RST_PORT, ETH_RST_PIN)
#define W5500_RESET_H() PIN_H(ETH_RST_PORT, ETH_RST_PIN)

/* Socket n protocol register (Sn_PROTO), used by IPRAW sockets. */
#define W5500_SN_PROTO(sn) \
  (_W5500_IO_BASE_ + (0x0014UL << 8) + (WIZCHIP_SREG_BLOCK(sn) << 3))

static const wiz_NetInfo w5500DefaultNetwork = {
  .mac = {0xaaU, 0xbbU, 0xccU, 0xddU, 0xeeU, 0xffU},
  .ip = {192U, 168U, 1U, 50U},
  .sn = {255U, 255U, 255U, 0U},
  .gw = {192U, 168U, 1U, 1U},
  .dns = {192U, 168U, 1U, 1U},
  .dhcp = NETINFO_STATIC
};

static const uint8_t w5500SocketMemory[2][8] = {
  {2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U},
  {2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U}
};

static wiz_NetInfo w5500Network;
static StaticSemaphore_t w5500BusMutexStorage;
static SemaphoreHandle_t w5500BusMutex;
static volatile bool w5500AddressAssigned;
static volatile BaseType_t w5500NetworkReady;
static uint8_t w5500DhcpBuffer[W5500_DHCP_BUFFER_SIZE];
static uint8_t w5500DnsBuffer[MAX_DNS_BUF_SIZE];
static uint16_t w5500PingSequence;
static StaticSemaphore_t w5500StartupStorage;
static SemaphoreHandle_t w5500Startup;
static volatile BaseType_t w5500StartupPending;
static volatile BaseType_t w5500IpShown;
static volatile TickType_t w5500IpShownTick;

static void w5500_Task(void*);
static BaseType_t w5500_IsSkipRequested(void);
static void w5500_Select(void);
static void w5500_Release(void);
static uint8_t w5500_ReadByte(void);
static void w5500_WriteByte(uint8_t);
static void w5500_Reset(void);
static void w5500_Delay(uint32_t);
static ErrorStatus w5500_InitializeNetwork(void);
static ErrorStatus w5500_InitializeChip(void);
static BaseType_t w5500_IsLinkUp(void);
static BaseType_t w5500_WaitForLink(void);
static ErrorStatus w5500_RunDhcp(void);
static void w5500_ApplyDhcpNetwork(void);
static void w5500_ApplyStaticNetwork(void);
static void w5500_PrintNetwork(void);
static void w5500_DisplayIp(void);
static ErrorStatus w5500_PingGateway(void);
static uint16_t w5500_Checksum(const uint8_t*, uint16_t);
static void w5500_AddressAssigned(void);
static void w5500_AddressConflict(void);


// -------------------------------------------------------------
int W5500_Init(void) {
  static StaticTask_t taskControlBlock;
  static StackType_t taskStack[configMINIMAL_STACK_SIZE * 3U];

  w5500Network = w5500DefaultNetwork;
  w5500NetworkReady = pdFALSE;
  w5500StartupPending = pdFALSE;

  if (w5500_IsSkipRequested() == pdTRUE) {
    printf("W5500 network configuration skipped (PB12 low)\n");
    return (0);
  }

  w5500BusMutex = xSemaphoreCreateMutexStatic(&w5500BusMutexStorage);
  if (w5500BusMutex == NULL) return (W5500_ERROR_MUTEX);
  if (SPI_Enable(SPI1) != SUCCESS) return (W5500_ERROR_SPI);

  reg_wizchip_cs_cbfunc(w5500_Select, w5500_Release);
  reg_wizchip_spi_cbfunc(w5500_ReadByte, w5500_WriteByte);

  /* Only confirm the chip answers here; link negotiation and DHCP can take
   * tens of seconds and run in the Network task so they never block boot. */
  w5500_Reset();
  if (w5500_InitializeChip() != SUCCESS) return (W5500_ERROR_CHIP);

  w5500Startup = xSemaphoreCreateBinaryStatic(&w5500StartupStorage);
  w5500StartupPending = (w5500Startup != NULL) ? pdTRUE : pdFALSE;

  /* Same priority as the API and RTC services: the WIZnet socket calls spin
   * while waiting for SEND_OK/ARP, and that must never delay temperature
   * measurement or threshold alarms, which run one level higher. */
  if (xTaskCreateStatic(
        w5500_Task,
        "Network",
        configMINIMAL_STACK_SIZE * 3U,
        NULL,
        configMAX_PRIORITIES - 3U,
        taskStack,
        &taskControlBlock
      ) == NULL) {
    w5500StartupPending = pdFALSE;
    return (W5500_ERROR_TASK);
  }
  return (0);
}


// -------------------------------------------------------------
BaseType_t W5500_IsReady(void) {
  return w5500NetworkReady;
}


// -------------------------------------------------------------
BaseType_t W5500_WaitStartup(TickType_t timeout) {
  /* Nothing to wait for when the network was skipped or never started. */
  if ((w5500StartupPending != pdTRUE) || (w5500Startup == NULL)) return (pdTRUE);
  if (xSemaphoreTake(w5500Startup, timeout) != pdTRUE) return (pdFALSE);
  (void)xSemaphoreGive(w5500Startup); /* stay signalled for any other waiter */
  return (pdTRUE);
}


// -------------------------------------------------------------
BaseType_t W5500_IsDisplayHeld(void) {
  if (w5500IpShown != pdTRUE) return (pdFALSE);
  return ((xTaskGetTickCount() - w5500IpShownTick) < pdMS_TO_TICKS(W5500_IP_DISPLAY_MS))
    ? pdTRUE
    : pdFALSE;
}


// -------------------------------------------------------------
void W5500_GetDnsServer(uint8_t* address) {
  if (address == NULL) return;
  taskENTER_CRITICAL();
  memcpy(address, w5500Network.dns, 4U);
  taskEXIT_CRITICAL();
}


// -------------------------------------------------------------
void W5500_GetNtpServer(uint8_t* address) {
  if (address == NULL) return;
  uint8_t dnsServer[4];
  uint8_t dhcp;

  taskENTER_CRITICAL();
  memcpy(address, w5500Network.gw, 4U);
  memcpy(dnsServer, w5500Network.dns, 4U);
  dhcp = w5500Network.dhcp;
  taskEXIT_CRITICAL();

  /* The static fallback's gateway is the configured NTP server. A DHCP
   * gateway usually is not, so resolve the public pool and keep the gateway
   * only as a last resort. */
  if (dhcp == NETINFO_DHCP) {
    static const uint8_t host[] = W5500_NTP_HOST;
    uint8_t resolved[4];
    if (DNS_run(dnsServer, (uint8_t*)host, resolved) == 1) {
      memcpy(address, resolved, 4U);
    }
  }
}


// -------------------------------------------------------------
static void w5500_Task(void* parameters) {
  (void)parameters;
  uint8_t secondsSincePing = 0U;
  uint8_t retryDelaySeconds = 1U;
  uint8_t retryCountdown = 0U;

  (void)w5500_InitializeNetwork();
  w5500StartupPending = pdFALSE;
  (void)xSemaphoreGive(w5500Startup);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(W5500_MONITOR_PERIOD_MS));

    if (w5500_IsLinkUp() != pdTRUE) {
      if (w5500NetworkReady == pdTRUE) {
        printf("W5500 link: DOWN\n");
        DHCP_stop();
      }
      w5500NetworkReady = pdFALSE;
      secondsSincePing = 0U;
      continue;
    }

    if (w5500NetworkReady != pdTRUE) {
      /* Back off when the link is up but the chip cannot be brought up. */
      if (retryCountdown > 0U) {
        retryCountdown--;
        continue;
      }
      printf("W5500 link: UP, restarting network configuration\n");
      if (w5500_InitializeNetwork() == SUCCESS) {
        retryDelaySeconds = 1U;
      } else {
        retryCountdown = retryDelaySeconds;
        if (retryDelaySeconds < W5500_RETRY_BACKOFF_MAX_S) {
          retryDelaySeconds = (uint8_t)(retryDelaySeconds * 2U);
        }
      }
      secondsSincePing = 0U;
      continue;
    }

    if (w5500Network.dhcp == NETINFO_DHCP) {
      DHCP_time_handler();
      uint8_t state = DHCP_run();
      if ((state == DHCP_IP_ASSIGN) || (state == DHCP_IP_CHANGED)) {
        w5500_ApplyDhcpNetwork();
      } else if (state == DHCP_FAILED) {
        printf("W5500 DHCP lease failed, reinitializing\n");
        w5500NetworkReady = pdFALSE;
        (void)w5500_InitializeNetwork();
        secondsSincePing = 0U;
        continue;
      }
    }

    secondsSincePing++;
    if (secondsSincePing >= W5500_GATEWAY_PING_PERIOD) {
      secondsSincePing = 0U;
      /* One retry absorbs a single dropped or rate-limited echo. */
      ErrorStatus ping = ERROR;
      for (uint8_t attempt = 0U; (attempt < W5500_PING_ATTEMPTS) && (ping != SUCCESS); attempt++) {
        ping = w5500_PingGateway();
      }
      if (ping != SUCCESS) {
        printf("W5500 gateway ping failed, reinitializing\n");
        w5500NetworkReady = pdFALSE;
        (void)w5500_InitializeNetwork();
      }
    }
  }
}


// -------------------------------------------------------------
static ErrorStatus w5500_InitializeNetwork(void) {
  BaseType_t linkSeen = pdFALSE;

  w5500NetworkReady = pdFALSE;
  DHCP_stop();
  for (uint8_t attempt = 1U; attempt <= W5500_INIT_ATTEMPTS; attempt++) {
    printf("W5500 initialization attempt %u/%u\n", attempt, W5500_INIT_ATTEMPTS);
    w5500_Reset();
    if (w5500_InitializeChip() != SUCCESS) continue;

    if (w5500_WaitForLink() != pdTRUE) {
      printf("W5500 link: DOWN; DHCP skipped\n");
      continue;
    }
    linkSeen = pdTRUE;
    printf("W5500 link: UP\n");

    if (w5500_RunDhcp() == SUCCESS) {
      w5500_ApplyDhcpNetwork();
      w5500NetworkReady = pdTRUE;
      return (SUCCESS);
    }
    printf("W5500 DHCP attempt failed\n");
  }

  /* Leave the chip usable with deterministic defaults after retries expire. */
  if (w5500_InitializeChip() == SUCCESS) {
    w5500_ApplyStaticNetwork();
    if ((linkSeen == pdTRUE) && (w5500_IsLinkUp() == pdTRUE)) {
      w5500NetworkReady = pdTRUE;
      return (SUCCESS);
    }
  }
  return (ERROR);
}


// -------------------------------------------------------------
static ErrorStatus w5500_InitializeChip(void) {
  if (ctlwizchip(CW_INIT_WIZCHIP, (void*)w5500SocketMemory) == -1) {
    printf("W5500: initialization failed\n");
    return (ERROR);
  }
  uint8_t version = getVERSIONR();
  if (version != 0x04U) {
    printf("W5500: unexpected version 0x%02x\n", version);
    return (ERROR);
  }
  setSHAR((uint8_t*)w5500DefaultNetwork.mac);
  return (SUCCESS);
}


// -------------------------------------------------------------
static BaseType_t w5500_IsLinkUp(void) {
  uint8_t link = PHY_LINK_OFF;
  (void)ctlwizchip(CW_GET_PHYLINK, &link);
  return (link == PHY_LINK_ON) ? pdTRUE : pdFALSE;
}


// -------------------------------------------------------------
static BaseType_t w5500_WaitForLink(void) {
  /* A reset restarts PHY auto-negotiation, which takes 1-3 s. */
  for (uint32_t waited = 0U; waited < W5500_LINK_WAIT_MS; waited += W5500_LINK_POLL_MS) {
    if (w5500_IsLinkUp() == pdTRUE) return (pdTRUE);
    w5500_Delay(W5500_LINK_POLL_MS);
  }
  return w5500_IsLinkUp();
}


// -------------------------------------------------------------
static ErrorStatus w5500_RunDhcp(void) {
  w5500AddressAssigned = false;
  DHCP_init(W5500_DHCP_SOCKET, w5500DhcpBuffer);
  reg_dhcp_cbfunc(
    w5500_AddressAssigned,
    w5500_AddressAssigned,
    w5500_AddressConflict
  );

  for (uint8_t step = 0U;
       (step < W5500_DHCP_RUN_ATTEMPTS) && !w5500AddressAssigned;
       step++) {
    if (w5500_IsLinkUp() != pdTRUE) {
      DHCP_stop();
      return (ERROR);
    }
    uint8_t state = DHCP_run();
    if ((state == DHCP_IP_ASSIGN) || (state == DHCP_IP_CHANGED)
        || (state == DHCP_IP_LEASED)) {
      w5500AddressAssigned = true;
      break;
    }
    w5500_Delay(W5500_DHCP_STEP_MS);
    if ((step & 1U) != 0U) DHCP_time_handler();
  }
  return w5500AddressAssigned ? SUCCESS : ERROR;
}


// -------------------------------------------------------------
static void w5500_ApplyDhcpNetwork(void) {
  taskENTER_CRITICAL();
  w5500Network = w5500DefaultNetwork;
  getIPfromDHCP(w5500Network.ip);
  getGWfromDHCP(w5500Network.gw);
  getSNfromDHCP(w5500Network.sn);
  getDNSfromDHCP(w5500Network.dns);
  w5500Network.dhcp = NETINFO_DHCP;
  taskEXIT_CRITICAL();
  ctlnetwork(CN_SET_NETINFO, &w5500Network);
  DNS_init(W5500_DNS_SOCKET, w5500DnsBuffer);
  printf("W5500 network: DHCP\n");
  w5500_PrintNetwork();
  w5500_DisplayIp();
}


// -------------------------------------------------------------
static void w5500_ApplyStaticNetwork(void) {
  taskENTER_CRITICAL();
  w5500Network = w5500DefaultNetwork;
  taskEXIT_CRITICAL();
  ctlnetwork(CN_SET_NETINFO, &w5500Network);
  DNS_init(W5500_DNS_SOCKET, w5500DnsBuffer);
  DHCP_stop();
  printf("W5500 network: static fallback\n");
  w5500_PrintNetwork();
  w5500_DisplayIp();
}


// -------------------------------------------------------------
static void w5500_PrintNetwork(void) {
  printf(
    "IP: %u.%u.%u.%u\nSUBNET: %u.%u.%u.%u\n"
    "GATEWAY: %u.%u.%u.%u\nDNS: %u.%u.%u.%u\n",
    w5500Network.ip[0], w5500Network.ip[1], w5500Network.ip[2], w5500Network.ip[3],
    w5500Network.sn[0], w5500Network.sn[1], w5500Network.sn[2], w5500Network.sn[3],
    w5500Network.gw[0], w5500Network.gw[1], w5500Network.gw[2], w5500Network.gw[3],
    w5500Network.dns[0], w5500Network.dns[1], w5500Network.dns[2], w5500Network.dns[3]
  );
}


// -------------------------------------------------------------
static void w5500_DisplayIp(void) {
#if defined(USE_WH_DISPLAY)
  if (FLAG_CHECK(peripheralReadiness, PERIPHERAL_WH_DISPLAY_ERROR_BIT)) return;
  char text[17];
  int length = snprintf(
    text, sizeof(text), "%u.%u.%u.%u",
    w5500Network.ip[0], w5500Network.ip[1],
    w5500Network.ip[2], w5500Network.ip[3]
  );
  if (length > 0) {
    (void)WHxxxx_Clear();
    (void)WHxxxx_Print((const uint8_t*)text, (uint16_t)length);
    /* Keep the address on screen; see W5500_IsDisplayHeld(). */
    w5500IpShownTick = xTaskGetTickCount();
    w5500IpShown = pdTRUE;
  }
#endif
}


// -------------------------------------------------------------
static BaseType_t w5500_IsSkipRequested(void) {
  uint32_t shift = (ETH_SKIP_PIN - 8U) * 4U;

  /* Input with pull-up: an open strap reads high, a strap to GND reads low. */
  PIN_H(ETH_SKIP_PORT, ETH_SKIP_PIN);
  MODIFY_REG(ETH_SKIP_PORT->CRH, (0xfU << shift), (GPIO_IN_PU << shift));
  Delay_Milliseconds(W5500_SKIP_SETTLE_MS);
  return (PIN_LEVEL(ETH_SKIP_PORT, ETH_SKIP_PIN) == 0U) ? pdTRUE : pdFALSE;
}


// -------------------------------------------------------------
static ErrorStatus w5500_PingGateway(void) {
  uint8_t packet[W5500_ICMP_PACKET_SIZE] = {0U};
  uint8_t source[4] = {0U};
  uint16_t protocol = 0U;
  uint16_t identifier = 0x4554U; /* "ET" */
  uint16_t sequence = ++w5500PingSequence;

  packet[0] = W5500_ICMP_ECHO_REQUEST;
  packet[4] = (uint8_t)(identifier >> 8U);
  packet[5] = (uint8_t)identifier;
  packet[6] = (uint8_t)(sequence >> 8U);
  packet[7] = (uint8_t)sequence;
  memcpy(&packet[8], "ethermom", 8U);
  uint16_t checksum = w5500_Checksum(packet, sizeof(packet));
  packet[2] = (uint8_t)(checksum >> 8U);
  packet[3] = (uint8_t)checksum;

  (void)close(W5500_PING_SOCKET);
  /* An IPRAW socket carries whatever protocol Sn_PROTO holds; set ICMP here
   * rather than relying on socket() to do it. */
  WIZCHIP_WRITE(W5500_SN_PROTO(W5500_PING_SOCKET), IPPROTO_ICMP);
  if (socket(W5500_PING_SOCKET, Sn_MR_IPRAW, IPPROTO_ICMP, SF_IO_NONBLOCK)
      != W5500_PING_SOCKET) {
    return (ERROR);
  }
  if (sendto(
        W5500_PING_SOCKET, packet, sizeof(packet),
        w5500Network.gw, IPPROTO_ICMP
      ) != (int32_t)sizeof(packet)) {
    (void)close(W5500_PING_SOCKET);
    return (ERROR);
  }

  TickType_t start = xTaskGetTickCount();
  while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(W5500_PING_TIMEOUT_MS)) {
    if (getSn_RX_RSR(W5500_PING_SOCKET) > 0U) {
      int32_t received = recvfrom(
        W5500_PING_SOCKET, packet, sizeof(packet), source, &protocol
      );
      if ((received >= 8)
          && (memcmp(source, w5500Network.gw, sizeof(source)) == 0)
          && (packet[0] == W5500_ICMP_ECHO_REPLY)
          && (packet[1] == 0U)
          && (packet[4] == (uint8_t)(identifier >> 8U))
          && (packet[5] == (uint8_t)identifier)
          && (packet[6] == (uint8_t)(sequence >> 8U))
          && (packet[7] == (uint8_t)sequence)
          && (w5500_Checksum(packet, (uint16_t)received) == 0U)) {
        (void)close(W5500_PING_SOCKET);
        printf("W5500 gateway ping: OK\n");
        return (SUCCESS);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20U));
  }
  (void)close(W5500_PING_SOCKET);
  return (ERROR);
}


// -------------------------------------------------------------
static uint16_t w5500_Checksum(const uint8_t* data, uint16_t length) {
  uint32_t sum = 0U;
  while (length > 1U) {
    sum += ((uint16_t)data[0] << 8U) | data[1];
    data += 2;
    length -= 2U;
  }
  if (length > 0U) sum += (uint16_t)data[0] << 8U;
  while ((sum >> 16U) != 0U) sum = (sum & 0xffffU) + (sum >> 16U);
  return (uint16_t)~sum;
}


// -------------------------------------------------------------
static void w5500_Select(void) {
  (void)xSemaphoreTake(w5500BusMutex, portMAX_DELAY);
  W5500_SELECT();
}


// -------------------------------------------------------------
static void w5500_Release(void) {
  W5500_RELEASE();
  (void)xSemaphoreGive(w5500BusMutex);
}


// -------------------------------------------------------------
static uint8_t w5500_ReadByte(void) {
  uint8_t byte = 0U;
  (void)SPI_Read8(SPI1, &byte, 1U);
  return (byte);
}


// -------------------------------------------------------------
static void w5500_WriteByte(uint8_t byte) {
  (void)SPI_Write8(SPI1, &byte, 1U);
}


// -------------------------------------------------------------
static void w5500_Reset(void) {
  /* Hold the bus so no other task is mid-transfer while the chip resets. */
  (void)xSemaphoreTake(w5500BusMutex, portMAX_DELAY);
  W5500_RESET_L();
  w5500_Delay(50U);
  W5500_RESET_H();
  w5500_Delay(200U);
  (void)xSemaphoreGive(w5500BusMutex);
}


// -------------------------------------------------------------
static void w5500_Delay(uint32_t milliseconds) {
  /* Busy-wait only before the scheduler starts; tasks must yield the CPU. */
  if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
    Delay_Milliseconds(milliseconds);
  } else {
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
  }
}


// -------------------------------------------------------------
static void w5500_AddressAssigned(void) {
  w5500AddressAssigned = true;
}


// -------------------------------------------------------------
static void w5500_AddressConflict(void) {
  w5500AddressAssigned = false;
}
