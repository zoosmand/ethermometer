#!/usr/bin/env bash

set -u

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <device-ip>" >&2
  exit 2
fi

DEVICE_IP="$1"
MEASUREMENT_PORT=5005
INFO_PORT=5006
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_FILE="${SCRIPT_DIR}/sensor_tcp_test.log"
PASS_COUNT=0
FAIL_COUNT=0

: > "${LOG_FILE}"

log() {
  printf '%s\n' "$*" | tee -a "${LOG_FILE}"
}

request() {
  local port="$1"
  local command="$2"
  printf '%s\r\n' "${command}" \
    | nc -w 3 "${DEVICE_IP}" "${port}" 2>>"${LOG_FILE}" \
    | tr -d '\r'
}

expect_prefix() {
  local port="$1"
  local command="$2"
  local expected="$3"
  local response

  response="$(request "${port}" "${command}")"
  if [[ "${response}" == "${expected}"* ]]; then
    PASS_COUNT=$((PASS_COUNT + 1))
    log "PASS port=${port} command=${command} response=${response}"
  else
    FAIL_COUNT=$((FAIL_COUNT + 1))
    log "FAIL port=${port} command=${command} expected=${expected} response=${response:-<empty>}"
  fi
}

log "Sensor TCP test"
log "Device: ${DEVICE_IP}"
log "Started: $(date '+%Y-%m-%d %H:%M:%S %z')"
log ""

expect_prefix "${MEASUREMENT_PORT}" "get_sensors" "ERR unknown_command"
expect_prefix "${MEASUREMENT_PORT}" "get_tmpr" "ERR unknown_command"
expect_prefix "${INFO_PORT}" "get_t_1" "ERR unknown_command"

SENSOR_RESPONSE="$(request "${INFO_PORT}" "get_sensors")"
if [[ "${SENSOR_RESPONSE}" == "OK "* ]]; then
  PASS_COUNT=$((PASS_COUNT + 1))
  log "PASS port=${INFO_PORT} command=get_sensors response=${SENSOR_RESPONSE}"
else
  FAIL_COUNT=$((FAIL_COUNT + 1))
  log "FAIL port=${INFO_PORT} command=get_sensors expected=OK response=${SENSOR_RESPONSE:-<empty>}"
fi

SENSOR_COUNT="$(printf '%s\n' "${SENSOR_RESPONSE}" \
  | sed -n 's/.*all:\([0-9][0-9]*\).*/\1/p')"

if [ -z "${SENSOR_COUNT}" ] || [ "${SENSOR_COUNT}" -eq 0 ] 2>/dev/null; then
  FAIL_COUNT=$((FAIL_COUNT + 1))
  log "FAIL no registered sensors reported"
else
  SENSOR_NUMBER=1
  while [ "${SENSOR_NUMBER}" -le "${SENSOR_COUNT}" ]; do
    MODEL_RESPONSE="$(request "${INFO_PORT}" "get_model_${SENSOR_NUMBER}")"
    MODEL="${MODEL_RESPONSE#OK }"

    if [[ "${MODEL_RESPONSE}" == "OK "* ]]; then
      PASS_COUNT=$((PASS_COUNT + 1))
      log "PASS sensor=${SENSOR_NUMBER} model=${MODEL}"
    else
      FAIL_COUNT=$((FAIL_COUNT + 1))
      log "FAIL sensor=${SENSOR_NUMBER} model_response=${MODEL_RESPONSE:-<empty>}"
      SENSOR_NUMBER=$((SENSOR_NUMBER + 1))
      continue
    fi

    expect_prefix "${INFO_PORT}" "get_sn_${SENSOR_NUMBER}" "OK "
    expect_prefix "${INFO_PORT}" "health_${SENSOR_NUMBER}" "OK model:${MODEL},state:"
    expect_prefix "${MEASUREMENT_PORT}" "get_all_${SENSOR_NUMBER}" "OK model:${MODEL},"
    expect_prefix "${MEASUREMENT_PORT}" "get_t_${SENSOR_NUMBER}" "OK "

    case "${MODEL}" in
      DS18B20)
        expect_prefix "${MEASUREMENT_PORT}" "get_p_${SENSOR_NUMBER}" \
          "ERR measurement_not_supported"
        expect_prefix "${MEASUREMENT_PORT}" "get_h_${SENSOR_NUMBER}" \
          "ERR measurement_not_supported"
        ;;
      BMP280)
        expect_prefix "${MEASUREMENT_PORT}" "get_p_${SENSOR_NUMBER}" "OK "
        expect_prefix "${MEASUREMENT_PORT}" "get_h_${SENSOR_NUMBER}" \
          "ERR measurement_not_supported"
        ;;
      BME280|BME680)
        expect_prefix "${MEASUREMENT_PORT}" "get_p_${SENSOR_NUMBER}" "OK "
        expect_prefix "${MEASUREMENT_PORT}" "get_h_${SENSOR_NUMBER}" "OK "
        ;;
      *)
        FAIL_COUNT=$((FAIL_COUNT + 1))
        log "FAIL sensor=${SENSOR_NUMBER} unsupported_model=${MODEL}"
        ;;
    esac

    SENSOR_NUMBER=$((SENSOR_NUMBER + 1))
  done
fi

expect_prefix "${MEASUREMENT_PORT}" "get_t_0" "ERR invalid_sensor_number"
expect_prefix "${INFO_PORT}" "health_0" "ERR invalid_sensor_number"

log ""
log "Finished: $(date '+%Y-%m-%d %H:%M:%S %z')"
log "Result: pass=${PASS_COUNT} fail=${FAIL_COUNT}"
log "Log: ${LOG_FILE}"

if [ "${FAIL_COUNT}" -ne 0 ]; then
  exit 1
fi
