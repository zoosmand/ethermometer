# Ethernet Thermometer

## STM32F103C8

### Features

* Straightforward peripheral initialization
* TCP measurement service on port 5005
* TCP sensor information and health service on port 5006

### Development documentation

* [Naming conventions](docs/NAMING_CONVENTIONS.md)

### TCP measurement commands

Measurement commands listen on TCP port 5005. Commands may be terminated with
`CR`, `LF`, or `CRLF`.
The server sends one response and then closes the connection. Sensor commands
return cached results from the periodic measurement service; they do not start
a new conversion.

| Command | Response |
| --- | --- |
| `get_all_X` | All recent measurements from physical sensor `X` |
| `get_t_X` | Recent temperature from temperature sensor `X` |
| `get_p_X` | Recent pressure from pressure sensor `X` |
| `get_h_X` | Recent relative humidity from humidity sensor `X` |
| Unknown command | `ERR unknown_command\r\n` |

Sensor numbers are one-based. Every command uses the same physical sensor list.
For example, if physical sensor 1 is a DS18B20, `get_t_1` returns its
temperature while `get_p_1` and `get_h_1` return
`ERR measurement_not_supported`.

Example responses:

```text
get_all_1   -> OK model:DS18B20,t:28.06
get_all_3   -> OK model:BME280,t:26.98,p:100099,h:46.419
get_t_1     -> OK 28.06
get_p_1     -> OK 100099
get_h_1     -> OK 46.419
```

### TCP sensor information and health

Sensor information and health checks listen on a separate WIZnet socket on TCP
port 5006.

| Command | Response |
| --- | --- |
| `get_sensors` | Number of registered sensors by measurement type |
| `get_model_X` | Model of physical sensor `X` |
| `get_sn_X` | Serial number of physical sensor `X` |
| `health_X` | Cached availability and health of physical sensor `X` |

These commands use the same one-based physical sensor list as `get_all_X`.
Physical indexes remain assigned to the same registered sensor. DS18B20 devices
are identified by their unique 64-bit ROM addresses, so disconnecting one does
not renumber the others.

`get_sensors` returns counts in the following format:

```text
OK t:4,h:2,p:2,all:4
```

Here, `all` is the number of physical sensors. A sensor that provides multiple
measurement types is counted once in `all` and once in each applicable type.
Gas measurements are reserved for a future protocol extension.

Information examples:

```text
get_model_1 -> OK DS18B20
get_sn_1    -> OK 28FF641D2A1603B7
```

DS18B20 serial numbers are returned as 16 uppercase hexadecimal digits. Bosch
sensor serial numbers use the per-device 31-bit Unique ID and are returned as
eight uppercase hexadecimal digits.

`health_X` reads cached service metadata and does not communicate with the
sensor. A healthy response includes the model, time since the last successful
measurement in milliseconds, consecutive failure count, and last error:

```text
health_1 -> OK model:DS18B20,state:healthy,age_ms:2150,failures:0,error:none
```

Example using Netcat:

```console
$ printf 'health_1\r\n' | nc 192.168.1.10 5006
OK model:DS18B20,state:healthy,age_ms:2150,failures:0,error:none
```

The possible health states are:

| State | Meaning |
| --- | --- |
| `initializing` | Registered, but no successful measurement is available yet |
| `healthy` | The latest periodic measurement succeeded |
| `degraded` | One or two consecutive measurements failed |
| `failed` | Three or more consecutive measurements failed |
| `stale` | The last successful measurement is older than three service periods |
| `missing` | A previously registered DS18B20 is absent from the latest discovery |

Diagnostic errors currently include `none`, `not_ready`, `timeout`, `crc`,
`bus`, `missing`, and `conversion`. When a sensor has never produced a valid
measurement, the response contains `age_ms:unavailable`.

Temperature is expressed in degrees Celsius, pressure in pascals, and relative
humidity as a percentage.

Possible error responses include:

| Response | Meaning |
| --- | --- |
| `ERR invalid_sensor_number` | The index is missing, zero, malformed, or too large |
| `ERR sensor_not_found` | The requested physical sensor index does not exist |
| `ERR measurement_not_supported` | The selected sensor does not provide the requested measurement type |
| `ERR measurement_unavailable` | The sensor exists, but no valid recent measurement is available |
| `ERR command_too_long` | The command exceeds the receive buffer |
| `ERR unknown_command` | The command name is not supported |

Example using Netcat, with the device at `192.168.1.10`:

```console
$ printf 'get_all_3\r\n' | nc 192.168.1.10 5005
OK model:BME280,t:26.98,p:100099,h:46.419
$ printf 'get_sensors\r\n' | nc 192.168.1.10 5006
OK t:4,h:2,p:2,all:4
```

---

&copy; 2017-2026, Askug Ltd., Dmitry Slobodchikov
