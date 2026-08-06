/**
  ******************************************************************************
  * @file           : whxxxx.c
  * @brief          : WHxxxx character display implementation, 4-bit parallel mode.
  * @project        : STM32F1 Health Check Device
  * @platform       : STMicroelectronics STM32F103C8
  * @created        : 24.07.2026 04:51:32 PM
  ******************************************************************************
  * @attention
  * @copyright  : 2017-2026, Dmitry Slobodchikov
  ******************************************************************************
  */

#include "whxxxx.h"

#if defined(USE_WH_DISPLAY)

#if (WH_DSPL_MODEL == 1602)
#define WHXXXX_COLUMNS           16U
#define WHXXXX_ROWS              2U
#define WHXXXX_ONE_LINE_LENGTH   16U
#elif (WH_DSPL_MODEL == 2004)
#define WHXXXX_COLUMNS           20U
#define WHXXXX_ROWS              4U
#define WHXXXX_ONE_LINE_LENGTH   40U
#else
#error "Unsupported WHxxxx display model"
#endif

#if (WH_DSPL_LINE_MODE == 1)
#define WHXXXX_FUNCTION_SET 0x20U
#elif (WH_DSPL_LINE_MODE == 2)
#define WHXXXX_FUNCTION_SET 0x28U
#else
#error "WH_DSPL_LINE_MODE must be 1 or 2"
#endif

static uint8_t displayRow;
static uint8_t displayColumn;
static FunctionalState displayReady;
static FunctionalState displayNewLinePending;
#if (WH_DSPL_LINE_MODE == 2)
static uint8_t displayLines[WHXXXX_ROWS][WHXXXX_COLUMNS];
static FunctionalState displayRepaintPending;
#endif
static SemaphoreHandle_t displayMutex;
static StaticSemaphore_t displayMutexBuffer;

static void whxxxx_ConfigureOutputPin(GPIO_TypeDef*, uint16_t);
static void whxxxx_SetRS(FunctionalState);
static void whxxxx_SetDataNibble(uint8_t);
static void whxxxx_PulseEnable(void);
static void whxxxx_WriteNibble4(uint8_t);
static ErrorStatus whxxxx_WriteByte(uint8_t, FunctionalState);
static ErrorStatus whxxxx_Command(uint8_t);
#if (WH_DSPL_LINE_MODE == 2)
static ErrorStatus whxxxx_SetCursor(uint8_t, uint8_t);
static ErrorStatus whxxxx_StartPrintfLine(void);
#endif
static ErrorStatus whxxxx_Lock(void);
static void whxxxx_Unlock(void);




// -------------------------------------------------------------
ErrorStatus WHxxxx_Init(void) {
  displayReady = DISABLE;
  displayRow = 0U;
  displayColumn = 0U;
  displayNewLinePending = DISABLE;
#if (WH_DSPL_LINE_MODE == 2)
  for (uint8_t row = 0U; row < WHXXXX_ROWS; row++) {
    for (uint8_t column = 0U; column < WHXXXX_COLUMNS; column++) {
      displayLines[row][column] = ' ';
    }
  }
  displayNewLinePending = ENABLE;
  displayRepaintPending = DISABLE;
#endif
  displayMutex = xSemaphoreCreateMutexStatic(&displayMutexBuffer);
  if (displayMutex == NULL) return (ERROR);

  whxxxx_ConfigureOutputPin(WHXXXX_RS_PORT, WHXXXX_RS_PIN);
  whxxxx_ConfigureOutputPin(WHXXXX_EN_PORT, WHXXXX_EN_PIN);
  whxxxx_ConfigureOutputPin(WHXXXX_D4_PORT, WHXXXX_D4_PIN);
  whxxxx_ConfigureOutputPin(WHXXXX_D5_PORT, WHXXXX_D5_PIN);
  whxxxx_ConfigureOutputPin(WHXXXX_D6_PORT, WHXXXX_D6_PIN);
  whxxxx_ConfigureOutputPin(WHXXXX_D7_PORT, WHXXXX_D7_PIN);

  /* HD44780 4-bit "initialize by instruction" sequence (datasheet Fig. 24). */
  Delay_Milliseconds(40U);
  whxxxx_SetRS(DISABLE);
  whxxxx_WriteNibble4(0x3U);
  Delay_Microseconds(4100U);
  whxxxx_WriteNibble4(0x3U);
  Delay_Microseconds(100U);
  whxxxx_WriteNibble4(0x3U);
  Delay_Microseconds(40U);
  whxxxx_WriteNibble4(0x2U); /* Switch the controller to 4-bit mode. */
  Delay_Microseconds(40U);

  if (whxxxx_Command(WHXXXX_FUNCTION_SET) != SUCCESS) return (ERROR);
  if (whxxxx_Command(0x0CU) != SUCCESS) return (ERROR); /* Display on, cursor off */
  if (whxxxx_Command(0x01U) != SUCCESS) return (ERROR); /* Clear display */
  Delay_Microseconds(1640U);
  if (whxxxx_Command(0x06U) != SUCCESS) return (ERROR); /* Increment cursor */

  displayReady = ENABLE;
  return (SUCCESS);
}




// -------------------------------------------------------------
ErrorStatus WHxxxx_Clear(void) {
  if (displayReady != ENABLE) return (ERROR);
  if (whxxxx_Lock() != SUCCESS) return (ERROR);
  ErrorStatus status = whxxxx_Command(0x01U);
  if (status == SUCCESS) {
    Delay_Microseconds(1640U);
    displayRow = 0U;
    displayColumn = 0U;
    displayNewLinePending = DISABLE;
#if (WH_DSPL_LINE_MODE == 2)
    for (uint8_t row = 0U; row < WHXXXX_ROWS; row++) {
      for (uint8_t column = 0U; column < WHXXXX_COLUMNS; column++) {
        displayLines[row][column] = ' ';
      }
    }
    displayNewLinePending = ENABLE;
    displayRepaintPending = DISABLE;
#endif
  }
  whxxxx_Unlock();
  return (status);
}




// -------------------------------------------------------------
ErrorStatus WHxxxx_Print(const uint8_t* buffer, uint16_t length) {
  if ((displayReady != ENABLE) || (buffer == NULL)) return (ERROR);
  if (whxxxx_Lock() != SUCCESS) return (ERROR);

  ErrorStatus status = SUCCESS;
  for (uint16_t i = 0U; (i < length) && (status == SUCCESS); i++) {
    if (whxxxx_WriteByte(buffer[i], ENABLE) != SUCCESS) {
      status = ERROR;
    }
  }

  whxxxx_Unlock();
  return (status);
}




// -------------------------------------------------------------
int WHxxxx_PutChar(char character) {
  if ((displayReady != ENABLE) || (displayMutex == NULL)) {
    return (ERROR);
  }
  if (whxxxx_Lock() != SUCCESS) return (ERROR);

  ErrorStatus status = SUCCESS;
#if (WH_DSPL_LINE_MODE == 1)
  if (character == '\n') {
    displayNewLinePending = ENABLE;
  } else if (character != '\r') {
    if (displayNewLinePending == ENABLE) {
      status = whxxxx_Command(0x01U);
      Delay_Microseconds(1640U);
      displayRow = 0U;
      displayColumn = 0U;
      displayNewLinePending = DISABLE;
    }
    if ((status == SUCCESS)
        && (displayColumn < WHXXXX_ONE_LINE_LENGTH)) {
      status = whxxxx_WriteByte((uint8_t)character, ENABLE);
      if (status == SUCCESS) {
        displayColumn++;
      }
    }
  }
#else
  if (character == '\n') {
    displayNewLinePending = ENABLE;
  } else if (character != '\r') {
    if (displayNewLinePending == ENABLE) {
      status = whxxxx_StartPrintfLine();
    }
    if ((status == SUCCESS)
        && (displayColumn < WHXXXX_COLUMNS)) {
      status = whxxxx_WriteByte((uint8_t)character, ENABLE);
      if (status == SUCCESS) {
        displayLines[WHXXXX_ROWS - 1U][displayColumn] = (uint8_t)character;
        displayColumn++;
      }
    }
  }
#endif

  whxxxx_Unlock();
  return (status == SUCCESS) ? (uint8_t)character : ERROR;
}




// -------------------------------------------------------------
static void whxxxx_ConfigureOutputPin(GPIO_TypeDef* port, uint16_t pin) {
  uint32_t shift;
  uint32_t mode = GPIO_IOS_2 | GPIO_GPO_PP;

  if (pin < 8U) {
    shift = pin * 4U;
    MODIFY_REG(port->CRL, (0xfUL << shift), (mode << shift));
  } else {
    shift = (pin - 8U) * 4U;
    MODIFY_REG(port->CRH, (0xfUL << shift), (mode << shift));
  }
  PIN_L(port, pin);
}




// -------------------------------------------------------------
static void whxxxx_SetRS(FunctionalState rs) {
  if (rs == ENABLE) {
    PIN_H(WHXXXX_RS_PORT, WHXXXX_RS_PIN);
  } else {
    PIN_L(WHXXXX_RS_PORT, WHXXXX_RS_PIN);
  }
}




// -------------------------------------------------------------
static void whxxxx_SetDataNibble(uint8_t nibble) {
  if ((nibble & 0x1U) != 0U) PIN_H(WHXXXX_D4_PORT, WHXXXX_D4_PIN); else PIN_L(WHXXXX_D4_PORT, WHXXXX_D4_PIN);
  if ((nibble & 0x2U) != 0U) PIN_H(WHXXXX_D5_PORT, WHXXXX_D5_PIN); else PIN_L(WHXXXX_D5_PORT, WHXXXX_D5_PIN);
  if ((nibble & 0x4U) != 0U) PIN_H(WHXXXX_D6_PORT, WHXXXX_D6_PIN); else PIN_L(WHXXXX_D6_PORT, WHXXXX_D6_PIN);
  if ((nibble & 0x8U) != 0U) PIN_H(WHXXXX_D7_PORT, WHXXXX_D7_PIN); else PIN_L(WHXXXX_D7_PORT, WHXXXX_D7_PIN);
}




// -------------------------------------------------------------
static void whxxxx_PulseEnable(void) {
  /* HD44780 minimum enable pulse width is 450 ns; 1 us clears that margin. */
  PIN_H(WHXXXX_EN_PORT, WHXXXX_EN_PIN);
  Delay_Microseconds(1U);
  PIN_L(WHXXXX_EN_PORT, WHXXXX_EN_PIN);
  Delay_Microseconds(1U);
}




// -------------------------------------------------------------
static void whxxxx_WriteNibble4(uint8_t nibble) {
  whxxxx_SetDataNibble(nibble);
  whxxxx_PulseEnable();
}




// -------------------------------------------------------------
static ErrorStatus whxxxx_WriteByte(uint8_t value, FunctionalState rs) {
  whxxxx_SetRS(rs);
  whxxxx_WriteNibble4((uint8_t)((value >> 4U) & 0x0FU));
  whxxxx_WriteNibble4((uint8_t)(value & 0x0FU));
  Delay_Microseconds(40U);
  return (SUCCESS);
}




// -------------------------------------------------------------
static ErrorStatus whxxxx_Command(uint8_t command) {
  return whxxxx_WriteByte(command, DISABLE);
}




// -------------------------------------------------------------
#if (WH_DSPL_LINE_MODE == 2)
static ErrorStatus whxxxx_SetCursor(uint8_t row, uint8_t column) {
#if (WH_DSPL_MODEL == 1602)
  static const uint8_t rowOffsets[WHXXXX_ROWS] = {0x00U, 0x40U};
#else
  static const uint8_t rowOffsets[WHXXXX_ROWS] = {0x00U, 0x40U, 0x14U, 0x54U};
#endif
  return whxxxx_Command(0x80U | (rowOffsets[row] + column));
}




// -------------------------------------------------------------
static ErrorStatus whxxxx_StartPrintfLine(void) {
  if (displayRepaintPending == DISABLE) {
    for (uint8_t row = 0U; row < (WHXXXX_ROWS - 1U); row++) {
      for (uint8_t column = 0U; column < WHXXXX_COLUMNS; column++) {
        displayLines[row][column] = displayLines[row + 1U][column];
      }
    }
    for (uint8_t column = 0U; column < WHXXXX_COLUMNS; column++) {
      displayLines[WHXXXX_ROWS - 1U][column] = ' ';
    }
    displayRepaintPending = ENABLE;
  }

  ErrorStatus status = SUCCESS;
  for (uint8_t row = 0U;
       (row < WHXXXX_ROWS) && (status == SUCCESS);
       row++) {
    status = whxxxx_SetCursor(row, 0U);
    for (uint8_t column = 0U;
         (column < WHXXXX_COLUMNS) && (status == SUCCESS);
         column++) {
      status = whxxxx_WriteByte(
        displayLines[row][column],
        ENABLE
      );
    }
  }

  if (status == SUCCESS) {
    status = whxxxx_SetCursor(WHXXXX_ROWS - 1U, 0U);
  }
  if (status == SUCCESS) {
    displayRow = WHXXXX_ROWS - 1U;
    displayColumn = 0U;
    displayNewLinePending = DISABLE;
    displayRepaintPending = DISABLE;
  }
  return (status);
}
#endif




// -------------------------------------------------------------
static ErrorStatus whxxxx_Lock(void) {
  return (xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE) ? SUCCESS : ERROR;
}




// -------------------------------------------------------------
static void whxxxx_Unlock(void) {
  (void)xSemaphoreGive(displayMutex);
}

#endif /* USE_WH_DISPLAY */
