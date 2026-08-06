/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Application entry point and system initialization.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 20.09.2025 08:34:08 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Global variables ----------------------------------------------------------*/
__IO uint32_t peripheralReadiness = 0;

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/




////////////////////////////////////////////////////////////////////////////////

/**
  * @brief  Main program entry point.
  * @param  None
  * @retval None
  */
int main(void) {

  /* Initialization of necessary peripherals */
  if (LED_Init(HEARTBEAT_PORT, HEARTBEAT_PIN) != SUCCESS) FLAG_SET(peripheralReadiness, PERIPHERAL_HEARTBEAT_LED_ERROR_BIT);
  if ((Buzzer_Init() != SUCCESS) || (Buzzer_SelfTest() != SUCCESS)) {
    FLAG_SET(peripheralReadiness, PERIPHERAL_BUZZER_ERROR_BIT);
  }
  if (OneWire_Init(ONEWIRE_PORT, ONEWIRE_PIN) != SUCCESS) FLAG_SET(peripheralReadiness, PERIPHERAL_ONEWIRE_ERROR_BIT);
  if (USART_Init(USART1) != SUCCESS) FLAG_SET(peripheralReadiness, PERIPHERAL_USART1_ERROR_BIT);
  if ((SPI_Init(SPI1) != SUCCESS)
      || (EthernetSPI_Init(ETH_CS_PORT, ETH_CS_PIN) != SUCCESS)
      || (EthernetSPI_Init(ETH_RST_PORT, ETH_RST_PIN) != SUCCESS)
      || (SPI_AdjustConfiguration(SPI1) != SUCCESS)) {
    FLAG_SET(peripheralReadiness, PERIPHERAL_SPI1_ERROR_BIT);
  }
  #if defined(USE_WH_DISPLAY)
    if (WHxxxx_Init() != SUCCESS) {
      FLAG_SET(peripheralReadiness, PERIPHERAL_WH_DISPLAY_ERROR_BIT);
    }
  #endif

  if (!FLAG_CHECK(peripheralReadiness, PERIPHERAL_SPI1_ERROR_BIT) && (W5500_Init() != 0)) {
    FLAG_SET(peripheralReadiness, PERIPHERAL_SPI1_ERROR_BIT);
  }

  printf("Peripherals readiness list: 0x%08lx\n", peripheralReadiness);
  
  /* Run the Heartbeat Service */
  HeartBeatService_Init();

  /* Run the Temperature Measurement Service */
  OneWireBusConfiguration_Init();
  ThresholdService_Init();
  TemperatureSensorService_Init();

  /* HTTP API service */
  if (!FLAG_CHECK(peripheralReadiness, PERIPHERAL_SPI1_ERROR_BIT)) {
    ApiService_Init();
  }

  /* Run the internal health and watchdog service last. */
  HealthService_Init();

  /* Start the scheduler. */
  vTaskStartScheduler();

  while (1);
}



#if (configCHECK_FOR_STACK_OVERFLOW > 0)

    void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
        /* Check pcTaskName for the name of the offending task,
         * or pxCurrentTCB if pcTaskName has itself been corrupted. */
        (void) xTask;
        (void) pcTaskName;
        HealthService_LatchFailure();
        taskDISABLE_INTERRUPTS();
        while (1);
    }

#endif /* #if (configCHECK_FOR_STACK_OVERFLOW > 0) */



















/**
  * @brief  Setup the microcontroller system
  *         Initialize the Embedded Flash Interface, the PLL and update the 
  *         SystemCoreClock variable.
  * @note   This function should be used only after reset.
  * @param  None
  * @retval None
  */
void SystemInit (void) {
  uint32_t timeout;

  #if (PREFETCH_ENABLE != 0)
    PREG_SET(FLASH->ACR, FLASH_ACR_PRFTBE_Pos);
  #else
    PREG_CLR(FLASH->ACR, FLASH_ACR_PRFTBE_Pos);
  #endif /* PREFETCH_ENABLE */

  __NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);

  /* SysCfg */
  PREG_SET(RCC->APB2ENR, RCC_APB2ENR_AFIOEN_Pos);
  while (!(PREG_CHECK(RCC->APB2ENR, RCC_APB2ENR_AFIOEN_Pos)));

  /* PWR */
  PREG_SET(RCC->APB1ENR, RCC_APB1ENR_PWREN_Pos);
  while (!(PREG_CHECK(RCC->APB1ENR, RCC_APB1ENR_PWREN_Pos)));

  /* Flash */
  MODIFY_REG(FLASH->ACR, FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_1);
  if (READ_BIT(FLASH->ACR, FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_1) {
    System_ErrorHandler();
  }

  /* JTAG-DP disabled and SW-DP enabled */
  CLEAR_BIT(AFIO->MAPR, AFIO_MAPR_SWJ_CFG);
  SET_BIT(AFIO->MAPR, AFIO_MAPR_SWJ_CFG_JTAGDISABLE);

  /* HSE enable and wait until it runs */
  PREG_SET(RCC->CR, RCC_CR_HSEON_Pos);
  timeout = 72000000U;
  while (!(PREG_CHECK(RCC->CR, RCC_CR_HSERDY_Pos)) && (--timeout != 0U));
  if (timeout == 0U) System_ErrorHandler();

  /* LSI enable and wait until it runs */
  PREG_SET(RCC->CSR, RCC_CSR_LSION_Pos);
  timeout = 72000000U;
  while (!(PREG_CHECK(RCC->CSR, RCC_CSR_LSIRDY_Pos)) && (--timeout != 0U));
  if (timeout == 0U) System_ErrorHandler();

  /* Enable backup registers access */
  PREG_SET(PWR->CR, PWR_CR_DBP_Pos);

  /* Force backup domain reset */
  PREG_SET(RCC->BDCR, RCC_BDCR_BDRST_Pos);
  PREG_CLR(RCC->BDCR, RCC_BDCR_BDRST_Pos);

  /* LSE enable and wait until it runs */
  PREG_SET(RCC->BDCR, RCC_BDCR_LSEON_Pos);
  timeout = 360000000U;
  while (!(PREG_CHECK(RCC->BDCR, RCC_BDCR_LSERDY_Pos)) && (--timeout != 0U));
  if (timeout == 0U) System_ErrorHandler();

  /* RTC Source is LSE */
  MODIFY_REG(RCC->BDCR, RCC_BDCR_RTCSEL, RCC_BDCR_RTCSEL_0);

  /* Enable RTC */
  PREG_SET(RCC->BDCR, RCC_BDCR_RTCEN_Pos);

  /* PLL confugure domain clock */
  RCC->CFGR |= RCC_CFGR_PLLMULL9; // mutiprexing pll on 9
  PREG_SET(RCC->CFGR, RCC_CFGR_PLLSRC_Pos); // PLL is the clock source

  /* PLL enable and wait until it runs */
  PREG_SET(RCC->CR, RCC_CR_PLLON_Pos);
  timeout = 72000000U;
  while (!(PREG_CHECK(RCC->CR, RCC_CR_PLLRDY_Pos)) && (--timeout != 0U));
  if (timeout == 0U) System_ErrorHandler();

  /* AHB clock isn't divided */
  /* APB1 clock divided by 2 */
  MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE1, RCC_CFGR_PPRE1_DIV2);
  /* APB2 clock isn't divided */

  /* set PLL as sysclock source and wait until it runs */
  MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_CFGR_SW_PLL);
  timeout = 72000000U;
  while ((READ_BIT(RCC->CFGR, RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) && (--timeout != 0U));
  if (timeout == 0U) System_ErrorHandler();



  SET_BIT(CoreDebug->DEMCR, CoreDebug_DEMCR_TRCENA_Msk);


  /*----------------------------------------------------------------------------*/
  /*                          Set peripheral clocks                             */
  /*----------------------------------------------------------------------------*/
  /* AHB peripherals */
  SET_BIT(RCC->AHBENR, (
      RCC_AHBENR_CRCEN
    | RCC_AHBENR_SRAMEN
  ));

  /* APB2 peripherals */
  SET_BIT(RCC->APB2ENR, (
      RCC_APB2ENR_IOPAEN
    | RCC_APB2ENR_IOPBEN
    | RCC_APB2ENR_IOPCEN
    | RCC_APB2ENR_USART1EN
    | RCC_APB2ENR_SPI1EN
    | RCC_APB2ENR_TIM1EN
  ));


  
  
  /* Stop ticking peripheral while debugging */
  #ifdef DEBUG
    SET_BIT(DBGMCU->CR , (
        DBGMCU_CR_DBG_IWDG_STOP
      | DBGMCU_CR_DBG_WWDG_STOP
    ));
  #endif /* DEBUG */

}
