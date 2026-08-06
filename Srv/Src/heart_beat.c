/**
  ******************************************************************************
  * @file           : heart_beat.c
  * @brief          : Heartbeat LED service implementation.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 22.09.2025 04:30:00 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */
 

/* Includes ------------------------------------------------------------------*/
#include "heart_beat.h"

/* Global variables ----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static void heartBeatTask(void* parameters);

/**
  * @brief  Heartbeat LED blinking
  * @param  port: pointer to the GPIO port instance
  * @param  pin:  pin number (0..15)
  * @param  callbackDelay:  pointer to delay function 
  * @param  delay:  delay value 
  * @retval none
  */
static void heartBeat_Blink(GPIO_TypeDef*, uint16_t, void (*)(TickType_t), TickType_t); 




/*******************************************************************************/

void HeartBeatService_Init(void) {

  static StaticTask_t heartBeatTaskTCB;
  static StackType_t heartBeatTaskStack[configMINIMAL_STACK_SIZE];

  HealthService_Register(HEALTH_COMPONENT_HEART_BEAT);
  (void) xTaskCreateStatic(
                            heartBeatTask,
                            "Heart Beat",
                            configMINIMAL_STACK_SIZE,
                            NULL,
                            configMAX_PRIORITIES - 1U,
                            &(heartBeatTaskStack[0]),
                            &(heartBeatTaskTCB)
                          );
}



static void heartBeatTask(void* parameters) {
  /* Unused parameters. */
  (void) parameters;

  while(1) {
      heartBeat_Blink(HEARTBEAT_PORT, HEARTBEAT_PIN, vTaskDelay, 1200);
      HealthService_Report(HEALTH_COMPONENT_HEART_BEAT);
  }
}




static void heartBeat_Blink(GPIO_TypeDef* port, uint16_t pin, void (*callbackDelay)(TickType_t), TickType_t delay) {
  TickType_t fraction = delay/10;

  PIN_L(port, pin);
  callbackDelay(fraction);
  
  PIN_H(port, pin);
  callbackDelay(fraction);
  
  PIN_L(port, pin);
  callbackDelay(fraction);
  
  PIN_H(port, pin);
  callbackDelay(delay - fraction);
}

