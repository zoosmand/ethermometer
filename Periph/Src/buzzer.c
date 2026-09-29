/**
  ******************************************************************************
  * @file           : buzzer.c
  * @brief          : Passive buzzer PWM implementation using TIM1 channel 1.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 27.07.2026
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "buzzer.h"
#include "common.h"
#include "stm32f103xb.h"
#include "semphr.h"

#define BUZZER_PORT                 GPIOA
#define BUZZER_PIN                  GPIO_PIN_8
#define BUZZER_TIMER_CLOCK_HZ       72000000UL
#define BUZZER_TIMER_TICK_HZ         1000000UL

static StaticSemaphore_t buzzerMutexStorage;
static SemaphoreHandle_t buzzerMutex;


// -------------------------------------------------------------
ErrorStatus Buzzer_Init(void) {
  uint32_t shift = (BUZZER_PIN - 8U) * 4U;
  uint32_t gpioMode = GPIO_AF_PP | GPIO_IOS_2;

  /* Keep the low-side transistor off until PWM is explicitly started. */
  PIN_L(BUZZER_PORT, BUZZER_PIN);
  MODIFY_REG(
    BUZZER_PORT->CRH,
    0x0fUL << shift,
    gpioMode << shift
  );

  CLEAR_BIT(TIM1->CR1, TIM_CR1_CEN);
  TIM1->PSC = (BUZZER_TIMER_CLOCK_HZ / BUZZER_TIMER_TICK_HZ) - 1U;
  TIM1->ARR = 0xffffU;
  TIM1->CCR1 = 0U;

  MODIFY_REG(
    TIM1->CCMR1,
    TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE,
    TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE
  );
  CLEAR_BIT(TIM1->CCER, TIM_CCER_CC1P);
  SET_BIT(TIM1->CCER, TIM_CCER_CC1E);
  SET_BIT(TIM1->BDTR, TIM_BDTR_MOE);
  SET_BIT(TIM1->CR1, TIM_CR1_ARPE);
  SET_BIT(TIM1->EGR, TIM_EGR_UG);

  buzzerMutex = xSemaphoreCreateMutexStatic(&buzzerMutexStorage);
  return (buzzerMutex != NULL) ? SUCCESS : ERROR;
}

BaseType_t Buzzer_Lock(TickType_t timeout) {
  return (buzzerMutex != NULL) ? xSemaphoreTake(buzzerMutex, timeout) : pdFALSE;
}

void Buzzer_Unlock(void) {
  if (buzzerMutex != NULL) (void)xSemaphoreGive(buzzerMutex);
}




// -------------------------------------------------------------
ErrorStatus Buzzer_Start(uint32_t frequencyHz) {
  uint32_t period;

  if ((frequencyHz < BUZZER_MIN_FREQUENCY_HZ)
      || (frequencyHz > BUZZER_MAX_FREQUENCY_HZ)) {
    return (ERROR);
  }

  period = BUZZER_TIMER_TICK_HZ / frequencyHz;
  if ((period < 2U) || (period > 0x10000UL)) return (ERROR);

  CLEAR_BIT(TIM1->CR1, TIM_CR1_CEN);
  TIM1->ARR = period - 1U;
  TIM1->CCR1 = period / 2U;
  SET_BIT(TIM1->EGR, TIM_EGR_UG);
  SET_BIT(TIM1->CR1, TIM_CR1_CEN);

  return (SUCCESS);
}




// -------------------------------------------------------------
void Buzzer_Stop(void) {
  CLEAR_BIT(TIM1->CR1, TIM_CR1_CEN);
  TIM1->CCR1 = 0U;
  SET_BIT(TIM1->EGR, TIM_EGR_UG);
  PIN_L(BUZZER_PORT, BUZZER_PIN);
}




// -------------------------------------------------------------
ErrorStatus Buzzer_SelfTest(void) {
  return Buzzer_SelfTestWithTimeout(portMAX_DELAY);
}

ErrorStatus Buzzer_SelfTestWithTimeout(TickType_t timeout) {
  if (Buzzer_Lock(timeout) != pdTRUE) return (ERROR);
  if (Buzzer_Start(BUZZER_SELF_TEST_FREQUENCY_HZ) != SUCCESS) {
    Buzzer_Stop();
    Buzzer_Unlock();
    return (ERROR);
  }

  Delay_Milliseconds(BUZZER_SELF_TEST_DURATION_MS);
  Buzzer_Stop();
  Buzzer_Unlock();
  return (SUCCESS);
}
