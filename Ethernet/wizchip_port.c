/**
  ******************************************************************************
  * @file           : wizchip_port.c
  * @brief          : W5500 board integration and network configuration.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 27.10.2025
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "DHCP/dhcp.h"
#include "DNS/dns.h"
#include <stdbool.h>
#include <string.h>

#define W5500_USE_DHCP             1U
#define W5500_DHCP_SOCKET          7U
#define W5500_DNS_SOCKET           6U
#define W5500_DHCP_RETRIES        20U
#define W5500_LINK_RETRIES        10U
#define W5500_DHCP_BUFFER_SIZE    548U

#define W5500_ERROR_MUTEX          -1
#define W5500_ERROR_CHIP_INIT      -2
#define W5500_ERROR_VERSION        -3
#define W5500_ERROR_LINK            1

#define W5500_SELECT()  PIN_L(ETH_CS_PORT, ETH_CS_PIN)
#define W5500_RELEASE() PIN_H(ETH_CS_PORT, ETH_CS_PIN)
#define W5500_RESET_L() PIN_L(ETH_RST_PORT, ETH_RST_PIN)
#define W5500_RESET_H() PIN_H(ETH_RST_PORT, ETH_RST_PIN)

static wiz_NetInfo w5500Network = {
  .mac = {0xaaU, 0xbbU, 0xccU, 0xddU, 0xeeU, 0xffU},
  .ip = {192U, 168U, 1U, 10U},
  .sn = {255U, 255U, 255U, 0U},
  .gw = {192U, 168U, 1U, 1U},
  .dns = {8U, 8U, 8U, 8U},
#if W5500_USE_DHCP
  .dhcp = NETINFO_DHCP
#else
  .dhcp = NETINFO_STATIC
#endif
};

static StaticSemaphore_t w5500BusMutexStorage;
static SemaphoreHandle_t w5500BusMutex;

#if W5500_USE_DHCP
static volatile bool w5500AddressAssigned;
static uint8_t w5500DhcpBuffer[W5500_DHCP_BUFFER_SIZE];
#endif

static uint8_t w5500DnsBuffer[MAX_DNS_BUF_SIZE];

static void w5500_Select(void);
static void w5500_Release(void);
static uint8_t w5500_ReadByte(void);
static void w5500_WriteByte(uint8_t byte);
static void w5500_Reset(void);
static ErrorStatus w5500_CheckIdentity(void);
static ErrorStatus w5500_WaitForLink(void);
static void w5500_ConfigureNetwork(void);
static void w5500_PrintNetwork(void);

#if W5500_USE_DHCP
static void w5500_AddressAssigned(void);
static void w5500_AddressConflict(void);
#endif


// -------------------------------------------------------------
int W5500_Init(void) {
  static const uint8_t socketMemory[2][8] = {
    {2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U},
    {2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U}
  };

  w5500BusMutex = xSemaphoreCreateMutexStatic(&w5500BusMutexStorage);
  if (w5500BusMutex == NULL) return (W5500_ERROR_MUTEX);
  if (SPI_Enable(SPI1) != SUCCESS) return (W5500_ERROR_CHIP_INIT);

  w5500_Reset();
  reg_wizchip_cs_cbfunc(w5500_Select, w5500_Release);
  reg_wizchip_spi_cbfunc(w5500_ReadByte, w5500_WriteByte);

  if (ctlwizchip(CW_INIT_WIZCHIP, (void*)socketMemory) == -1) {
    printf("W5500: initialization failed\n");
    return (W5500_ERROR_CHIP_INIT);
  }
  if (w5500_CheckIdentity() != SUCCESS) return (W5500_ERROR_VERSION);
  if (w5500_WaitForLink() != SUCCESS) return (W5500_ERROR_LINK);

  w5500_ConfigureNetwork();
  DNS_init(W5500_DNS_SOCKET, w5500DnsBuffer);
  w5500_PrintNetwork();
  return (0);
}




// -------------------------------------------------------------
void W5500_GetDnsServer(uint8_t* address) {
  if (address == NULL) return;
  memcpy(address, w5500Network.dns, 4U);
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
  W5500_RESET_L();
  Delay_Milliseconds(50U);
  W5500_RESET_H();
  Delay_Milliseconds(200U);
}




// -------------------------------------------------------------
static ErrorStatus w5500_CheckIdentity(void) {
  uint8_t version = getVERSIONR();

  if (version != 0x04U) {
    printf("W5500: unexpected version 0x%02x\n", version);
    return (ERROR);
  }

  printf("W5500: initialized\n");
  return (SUCCESS);
}




// -------------------------------------------------------------
static ErrorStatus w5500_WaitForLink(void) {
  uint8_t link = PHY_LINK_OFF;

  for (uint8_t attempt = 0U;
       (attempt < W5500_LINK_RETRIES) && (link != PHY_LINK_ON);
       attempt++) {
    (void)ctlwizchip(CW_GET_PHYLINK, &link);
    if (link != PHY_LINK_ON) Delay_Milliseconds(500U);
  }

  printf("W5500 link: %s\n", (link == PHY_LINK_ON) ? "UP" : "DOWN");
  return ((link == PHY_LINK_ON) ? SUCCESS : ERROR);
}




// -------------------------------------------------------------
static void w5500_ConfigureNetwork(void) {
#if W5500_USE_DHCP
  uint8_t retries = W5500_DHCP_RETRIES;

  w5500AddressAssigned = false;
  setSHAR(w5500Network.mac);
  DHCP_init(W5500_DHCP_SOCKET, w5500DhcpBuffer);
  reg_dhcp_cbfunc(
    w5500_AddressAssigned,
    w5500_AddressAssigned,
    w5500_AddressConflict
  );

  while (!w5500AddressAssigned && (retries-- > 0U)) {
    (void)DHCP_run();
    Delay_Milliseconds(500U);
  }

  if (w5500AddressAssigned) {
    getIPfromDHCP(w5500Network.ip);
    getGWfromDHCP(w5500Network.gw);
    getSNfromDHCP(w5500Network.sn);
    getDNSfromDHCP(w5500Network.dns);
    printf("W5500 network: DHCP\n");
  } else {
    w5500Network.dhcp = NETINFO_STATIC;
    printf("W5500 network: static fallback\n");
  }
#else
  printf("W5500 network: static\n");
#endif

  ctlnetwork(CN_SET_NETINFO, &w5500Network);
}




// -------------------------------------------------------------
static void w5500_PrintNetwork(void) {
  wiz_NetInfo network;

  ctlnetwork(CN_GET_NETINFO, &network);
  printf(
    "IP: %u.%u.%u.%u\n",
    network.ip[0], network.ip[1], network.ip[2], network.ip[3]
  );
  printf(
    "SUBNET: %u.%u.%u.%u\n",
    network.sn[0], network.sn[1], network.sn[2], network.sn[3]
  );
  printf(
    "GATEWAY: %u.%u.%u.%u\n",
    network.gw[0], network.gw[1], network.gw[2], network.gw[3]
  );
  printf(
    "DNS: %u.%u.%u.%u\n",
    network.dns[0], network.dns[1], network.dns[2], network.dns[3]
  );
}


#if W5500_USE_DHCP

// -------------------------------------------------------------
static void w5500_AddressAssigned(void) {
  w5500AddressAssigned = true;
}




// -------------------------------------------------------------
static void w5500_AddressConflict(void) {
  w5500AddressAssigned = false;
}

#endif /* W5500_USE_DHCP */
