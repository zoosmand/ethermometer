/**
  ******************************************************************************
  * @file           : health_service.c
  * @brief          : Internal health supervision and watchdog service.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 25.07.2026 07:44:16 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "health_service.h"

#define HEALTH_CHECK_PERIOD_MS       60000U
#define HEALTH_WATCHDOG_FEED_MS       1000U

static volatile uint32_t registeredComponents;
static volatile uint32_t reportedComponents;
static volatile BaseType_t emergencyLatched;

static void healthService_Task(void*);
static void healthService_WatchdogInit(void);
static void healthService_WatchdogReload(void);




// -------------------------------------------------------------
void HealthService_Init(void) {
  static StaticTask_t taskControlBlock;
  static StackType_t taskStack[configMINIMAL_STACK_SIZE * 2U];

  (void)xTaskCreateStatic(
    healthService_Task,
    "Health",
    configMINIMAL_STACK_SIZE * 2U,
    NULL,
    configMAX_PRIORITIES - 1U,
    taskStack,
    &taskControlBlock
  );
}




// -------------------------------------------------------------
void HealthService_Register(HealthComponent_TypeDef component) {
  taskENTER_CRITICAL();
  registeredComponents |= (uint32_t)component;
  taskEXIT_CRITICAL();
}




// -------------------------------------------------------------
void HealthService_Report(HealthComponent_TypeDef component) {
  taskENTER_CRITICAL();
  reportedComponents |= (uint32_t)component;
  taskEXIT_CRITICAL();
}




// -------------------------------------------------------------
void HealthService_LatchFailure(void) {
  emergencyLatched = pdTRUE;
}




// -------------------------------------------------------------
static void healthService_Task(void* parameters) {
  (void)parameters;
  TickType_t lastCheck = xTaskGetTickCount();
  TickType_t lastFeed = lastCheck;

  healthService_WatchdogInit();

  while (1) {
    TickType_t now = xTaskGetTickCount();

    if (!emergencyLatched
        && ((now - lastCheck) >= pdMS_TO_TICKS(HEALTH_CHECK_PERIOD_MS))) {
      uint32_t expected;
      uint32_t observed;

      taskENTER_CRITICAL();
      expected = registeredComponents;
      observed = reportedComponents;
      reportedComponents = 0U;
      taskEXIT_CRITICAL();

      if ((observed & expected) != expected) {
        printf(
          "Self-check: FAILED, missing:0x%08lx\n",
          (unsigned long)(expected & ~observed)
        );
        HealthService_LatchFailure();
      } else {
        printf("Self-check: OK\n");
      }
      lastCheck = now;
    }

    if (!emergencyLatched
        && ((now - lastFeed) >= pdMS_TO_TICKS(HEALTH_WATCHDOG_FEED_MS))) {
      healthService_WatchdogReload();
      lastFeed = now;
    }

    vTaskDelay(pdMS_TO_TICKS(100U));
  }
}




// -------------------------------------------------------------
static void healthService_WatchdogInit(void) {
  /*
   * Start IWDG after application initialization has completed and the
   * scheduler is running. The configured timeout is 2.4 seconds at the
   * nominal 40 kHz LSI frequency; LSI tolerance determines the actual delay.
   */
  IWDG->KR = IWDG_KEY_ENABLE;
  IWDG->KR = IWDG_KEY_ACCESS;
  IWDG->PR = IWDG_PRESCALER_DIV64;
  IWDG->RLR = IWDG_RELOAD_COUNTER;
  while (IWDG->SR != 0U);
  healthService_WatchdogReload();
}




// -------------------------------------------------------------
static void healthService_WatchdogReload(void) {
  IWDG->KR = IWDG_KEY_RELOAD;
}
