# Naming conventions

This document defines the preferred naming style for project-owned code in
`Core`, `Periph`, `Srv`, and the project-specific Ethernet integration.
Imported FreeRTOS, CMSIS, ST, and WIZnet code keeps its upstream style.

The conventions describe the target style. Existing names are changed only in
dedicated refactoring work, because renaming an API can affect several modules.

## General rules

- Use English names that describe purpose rather than implementation detail.
- Spell out words unless an abbreviation is established in the hardware or
  protocol documentation.
- Keep hardware names in their canonical form: `DS18B20`, `HTTP`, `IWDG`,
  `JSON`, `SPI`, `TCP`, `W5500`, and `WHxxxx`.
- Include units in names when the type alone does not make them clear, for
  example `periodMs`, `temperatureCentiDegrees`, or `humidityMilliPercent`.
- Avoid new identifiers beginning with an underscore. C reserves several such
  forms for the implementation.
- Use one term consistently for one concept. Prefer `Init`, `Read`, `Write`,
  `Measure`, `Get`, `Set`, `Lock`, `Unlock`, `Register`, and `Report`.

## Files and modules

- Use lowercase `snake_case` file names: `health_service.c`.
- Give a public header the same base name as its implementation file.
- Peripheral drivers belong in `Periph`; FreeRTOS-based application services
  belong in `Srv`; startup and application-wide facilities belong in `Core`.

## Functions

- Public functions use `PascalCase` with a module prefix:
  `HealthService_Init`, `I2C_Master_Send`, `DS18B20_Measure`.
- Private functions use `camelCase` with a module prefix:
  `healthService_WatchdogReload`.
- Use an underscore between the module name and the operation for new public
  APIs.
- Use verbs for operations and nouns only for accessors that return an object.
- An `Init` function initializes hardware or creates a service and does not
  perform periodic work.
- A `Get` function does not transfer ownership of returned storage unless its
  documentation explicitly says otherwise.
- Boolean predicates should begin with `Is`, `Has`, or `Can`.

## Types and enumerators

- Public typedef names use `PascalCase` and end in `_TypeDef`:
  `SensorSnapshot_TypeDef`.
- Structure names describe one object; collection names describe the contained
  set.
- Enum constants and bit flags use uppercase `SNAKE_CASE` with a module prefix:
  `SENSOR_HEALTH_FAILED`.
- Structure members and function parameters use `camelCase`.
- New code should not introduce `_t` typedef names because POSIX reserves many
  names with that suffix. Existing Bosch calibration types can be migrated in
  a separate compatibility-aware refactor.

## Variables and constants

- Local variables and parameters use `camelCase`.
- File-local variables use `camelCase`; their `static` storage already conveys
  privacy.
- Compile-time constants and macros use uppercase `SNAKE_CASE`.
- Macros that behave like functions use uppercase `SNAKE_CASE` and parenthesize
  every parameter and the complete expression.
- FreeRTOS handles should identify the owned object, for example
  `healthTaskHandle` or `i2cMutex`.

## Documentation

Document every project-owned function where it is declared. Document a private
function immediately above its definition or prototype. Do not duplicate a
complete public API description in both the header and source file.

Function documentation uses this form:

```c
/**
  * @brief Convert and read every discovered DS18B20 sensor.
  * @param measurements (DS18B20_Measurement_TypeDef*) Output array.
  * @param capacity (uint8_t) Number of elements available in the array.
  * @param count (uint8_t*) Number of entries written.
  * @retval (ErrorStatus) SUCCESS when at least one device was reported.
  */
```

Use `@param` only for real parameters; omit it for a `void` parameter list.
Use `@retval` only for functions that return a value. State units, ownership,
valid ranges, nullability, blocking behavior, and task/interrupt restrictions
when they matter.

Structure documentation lists the purpose and meaning of every member:

```c
/**
  * @brief Cached measurement and health information for one sensor.
  * @param model (SensorModel_TypeDef) Detected sensor model.
  * @param temperature (int32_t) Temperature in hundredths of a degree Celsius.
  */
```

## Compatibility notes

Issue #23 aligns project-owned identifiers with this convention. Imported
libraries retain their upstream APIs even where their style differs. Bosch
calibration members such as `dig_t1` and `par_p1` also retain the datasheet
spelling so the compensation formulas remain directly traceable.
