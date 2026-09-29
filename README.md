# Ethernet Thermometer

## STM32F103C8

### Features

* Straightforward peripheral initialization
* HTTP/JSON REST API on port 80
* Temperature-triggered buzzer alarm thresholds

### Development documentation

* [Naming conventions](docs/NAMING_CONVENTIONS.md)
* [Postman collection](postman_collection.json) for the HTTP API

### Ethernet

The W5500 makes up to five reset-and-DHCP initialization attempts. After each
reset it waits up to 5 s for the PHY link before sending DHCP traffic. If DHCP
is unavailable, the fallback configuration is `192.168.1.50/24`, with gateway,
DNS, and NTP at `192.168.1.1`; with a DHCP lease, NTP uses `pool.ntp.org`. The
acquired IP address is printed and shown on the display. Link state is checked
once per second and the gateway is pinged once per minute; link restoration or
two missed gateway responses restart the full W5500/DHCP initialization
sequence. Network setup runs in its own task, so boot is not delayed by it.

Temperature measurement (and with it, threshold alarms) starts only after the
first network configuration pass finishes: about 3–5 s normally, up to ~30 s
with no cable and ~60 s with a link but no DHCP server. The acquired IP stays
on the display for at least 10 s before temperatures replace it.

To boot without networking, tie **PB12** to GND (a jumper or switch to GND,
with a 10 kΩ pull-up from PB12 to 3.3 V). PB12 is sampled once at reset: low
skips all W5500 configuration and measurement starts immediately; open (high)
configures the network as above.

### HTTP API

The device serves a small JSON REST API on TCP port 80. Every response has
`Content-Type: application/json` and the connection is closed after each
response (`Connection: close`), so a new request needs a new connection.
Request bodies for `POST`/`PUT` routes are optional plain JSON with no
required header beyond `Content-Length` (sent automatically by `curl`).

| Method | Path | Description |
| --- | --- | --- |
| `GET`, `HEAD` | `/health` | General device status |
| `GET` | `/api/v1/rtc` | Current time and NTP synchronization state |
| `GET` | `/api/v1/sensors` | Summary of every connected sensor |
| `GET` | `/api/v1/sensors/<sensor_index>` | Full detail for one sensor |
| `GET` | `/api/v1/temperature` | Temperature reading from every sensor |
| `GET` | `/api/v1/thresholds` | Configured buzzer alarm thresholds |
| `PUT` | `/api/v1/thresholds/<threshold_index>` | Configure one alarm threshold |
| `POST` | `/api/v1/buzzer/test` | Sound the buzzer's self-test tone |

Sensor and threshold indexes are one-based. Physical sensor indexes remain
assigned to the same registered sensor: DS18B20 devices are identified by
their unique 64-bit ROM addresses, so disconnecting one does not renumber
the others. Threshold indexes address a fixed bank of three configurable
slots (1 through 3). `HEAD /health` returns the same status line and headers
as `GET /health` without a body, per usual HTTP semantics.

Errors are returned as `{"error":"<reason>"}` with a matching HTTP status
code: `400` for a malformed request or out-of-range value and `404` for an
unknown route or sensor index. The buzzer test returns `503` with
`{"error":"buzzer_busy"}` when an alarm
pattern owns the buzzer for longer than one second.

#### `GET /health`

Overall device status, derived from boot-time peripheral initialization and
the internal watchdog service. `status` is `"ok"`, `"degraded"` (a
non-critical peripheral failed to initialize), or `"failed"` (the internal
self-check watchdog has latched a failure).

```console
$ curl http://192.168.1.10/health
{"status":"ok","uptime_ms":723041,"watchdog_latched":false,"peripherals":{"heartbeat_led":true,"usart1":true,"onewire":true,"eth_spi":true,"display":true,"buzzer":true,"rtc":true}}
```

#### `GET /api/v1/rtc`

The device's backup-domain RTC is reset to zero on every boot (SystemInit
forces a backup-domain reset every time, so nothing survives a reset without
a battery), then synchronized against `pool.ntp.org` over UDP/123 as soon as
the network comes up, and re-synchronized once an hour after that.
`synchronized` is `false` until the first sync succeeds; `unix_time`/`iso8601`
still advance from zero in the meantime, they just aren't wall-clock-accurate
yet. The same reading is also printed once a minute via `printf()` (visible
on the ITM/USART debug output), independent of the API.

```console
$ curl http://192.168.1.10/api/v1/rtc
{"unix_time":1767686400,"iso8601":"2026-01-06T12:00:00Z","synchronized":true,"last_sync_age_ms":142317,"sync_failures":0}
```

#### `GET /api/v1/sensors`

`modes` lists the measurement types the sensor supports (`"temperature"`,
`"pressure"`, `"humidity"`). `serial` is the vendor-assigned identifier
(a DS18B20's 64-bit ROM code, 16 uppercase hexadecimal digits).

```console
$ curl http://192.168.1.10/api/v1/sensors
{"count":2,"sensors":[{"index":1,"model":"DS18B20","modes":["temperature"],"serial":"28FF641D2A1603B7","status":"healthy"},{"index":2,"model":"DS18B20","modes":["temperature"],"serial":"28AA123B4C5D6E7F","status":"stale"}]}
```

The possible `status` values are:

| Status | Meaning |
| --- | --- |
| `initializing` | Registered, but no successful measurement is available yet |
| `healthy` | The latest periodic measurement succeeded |
| `degraded` | One or two consecutive measurements failed |
| `failed` | Three or more consecutive measurements failed |
| `stale` | The last successful measurement is older than three service periods |
| `missing` | A previously registered DS18B20 is absent from the latest discovery |

#### `GET /api/v1/sensors/<sensor_index>`

Full detail for one physical sensor, including cached health metadata.
`temperature`/`pressure`/`humidity` are `null` when the sensor does not
support that measurement type or no valid recent measurement is available;
`age_ms` is `null` until the first successful measurement.

```console
$ curl http://192.168.1.10/api/v1/sensors/1
{"index":1,"model":"DS18B20","modes":["temperature"],"serial":"28FF641D2A1603B7","status":"healthy","age_ms":2150,"failures":0,"error":"none","temperature":28.06,"pressure":null,"humidity":null}
```

Diagnostic `error` values include `none`, `not_ready`, `timeout`, `crc`,
`bus`, `missing`, and `conversion`. Temperature is expressed in degrees
Celsius, pressure in pascals, and relative humidity as a percentage.

#### `GET /api/v1/temperature`

A lighter-weight readout of every sensor's temperature, without the rest of
the sensor detail.

```console
$ curl http://192.168.1.10/api/v1/temperature
{"count":2,"temperatures":[{"index":1,"available":true,"temperature":28.06},{"index":2,"available":false,"temperature":null}]}
```

#### `GET /api/v1/thresholds` and `PUT /api/v1/thresholds/<threshold_index>`

Each of the three slots has a fixed beep pattern, sounded at its configured
`tone_hz` whenever `sensor_index`'s temperature reaches or exceeds
`temperature`: slot 1 beeps 3 times once when crossed, slot 2 beeps 5 times
once when crossed, and slot 3 beeps 10 times when crossed and again every
10 seconds for as long as it stays exceeded. A slot re-arms (can beep again)
only after its temperature drops back below the trip point and reaches it
again. `triggered` reports whether the threshold is currently exceeded,
independent of whether it just beeped. All three slots are always listed.
By default, all three are enabled, watching physical sensor 1: slot 1 at
40 C (1400 Hz), slot 2 at 70 C (2100 Hz), and slot 3 at 100 C (2600 Hz).

```console
$ curl http://192.168.1.10/api/v1/thresholds
{"count":3,"thresholds":[{"index":1,"enabled":true,"sensor_index":1,"temperature":40.00,"tone_hz":1400,"triggered":false},{"index":2,"enabled":true,"sensor_index":1,"temperature":70.00,"tone_hz":2100,"triggered":false},{"index":3,"enabled":true,"sensor_index":1,"temperature":100.00,"tone_hz":2600,"triggered":false}]}

$ curl -X PUT http://192.168.1.10/api/v1/thresholds/2 \
    -d '{"sensor_index":1,"temperature":75.5,"tone_hz":2100,"enabled":true}'
{"index":2,"enabled":true,"sensor_index":1,"temperature":75.50,"tone_hz":2100,"triggered":false}
```

`sensor_index` and `tone_hz` (16 to 20000 Hz) are required; `enabled`
defaults to `true` when omitted. Reconfiguring a slot re-arms it. Only one
tone can play at a time: if several thresholds are exceeded simultaneously,
the most severe (highest-indexed) one wins for that measurement cycle; the
others are still marked as acknowledged, so they do not queue up and beep
later on their own.

#### `POST /api/v1/buzzer/test`

Sounds the same short confirmation tone played at boot, independent of any
configured threshold. The request waits for an active alarm pattern for at
most one second before returning `503` rather than blocking the API task.

```console
$ curl -X POST http://192.168.1.10/api/v1/buzzer/test
{"result":"ok"}
```

---

&copy; 2017-2026, Askug Ltd., Dmitry Slobodchikov
