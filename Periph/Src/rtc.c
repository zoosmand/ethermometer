/**
  ******************************************************************************
  * @file           : rtc.c
  * @brief          : Backup-domain RTC counter implementation.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 06.08.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "rtc.h"

/* LSE is 32.768 kHz; PRL+1 = 32768 divides it to an exact one-second tick. */
#define RTC_PRESCALER_RELOAD 0x7fffUL
#define RTC_OPERATION_TIMEOUT 1000000UL

static ErrorStatus rtc_WaitForLastTask(void);
static ErrorStatus rtc_WaitForSynchro(void);




// -------------------------------------------------------------
ErrorStatus RTC_Init(void) {
  if (rtc_WaitForSynchro() != SUCCESS) return (ERROR);
  if (rtc_WaitForLastTask() != SUCCESS) return (ERROR);

  SET_BIT(RTC->CRL, RTC_CRL_CNF);
  RTC->PRLH = (uint16_t)(RTC_PRESCALER_RELOAD >> 16U);
  RTC->PRLL = (uint16_t)(RTC_PRESCALER_RELOAD & 0xffffUL);
  CLEAR_BIT(RTC->CRL, RTC_CRL_CNF);

  return rtc_WaitForLastTask();
}




// -------------------------------------------------------------
ErrorStatus RTC_SetUnixTime(uint32_t seconds) {
  if (rtc_WaitForLastTask() != SUCCESS) return (ERROR);

  SET_BIT(RTC->CRL, RTC_CRL_CNF);
  RTC->CNTH = (uint16_t)(seconds >> 16U);
  RTC->CNTL = (uint16_t)(seconds & 0xffffU);
  CLEAR_BIT(RTC->CRL, RTC_CRL_CNF);

  return rtc_WaitForLastTask();
}




// -------------------------------------------------------------
uint32_t RTC_GetUnixTime(void) {
  uint32_t high1;
  uint32_t high2;
  uint32_t low;

  /* CNTH may tick between the two register reads; retry until it settles. */
  do {
    high1 = RTC->CNTH;
    low = RTC->CNTL;
    high2 = RTC->CNTH;
  } while (high1 != high2);

  return ((high1 << 16U) | low);
}




// -------------------------------------------------------------
static ErrorStatus rtc_WaitForLastTask(void) {
  uint32_t timeout = RTC_OPERATION_TIMEOUT;
  while (!PREG_CHECK(RTC->CRL, RTC_CRL_RTOFF_Pos)) {
    if (--timeout == 0U) return (ERROR);
  }
  return (SUCCESS);
}




// -------------------------------------------------------------
static ErrorStatus rtc_WaitForSynchro(void) {
  uint32_t timeout = RTC_OPERATION_TIMEOUT;
  CLEAR_BIT(RTC->CRL, RTC_CRL_RSF);
  while (!PREG_CHECK(RTC->CRL, RTC_CRL_RSF_Pos)) {
    if (--timeout == 0U) return (ERROR);
  }
  return (SUCCESS);
}
