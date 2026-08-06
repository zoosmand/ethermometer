#!/usr/bin/env bash

set -u

if [ "$#" -ne 1 ]; then
  echo "Usage: $0 <device-ip>" >&2
  exit 2
fi

DEVICE_IP="$1"
BASE_URL="http://${DEVICE_IP}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LOG_FILE="${SCRIPT_DIR}/api_test.log"
PASS_COUNT=0
FAIL_COUNT=0

: > "${LOG_FILE}"

log() {
  printf '%s\n' "$*" | tee -a "${LOG_FILE}"
}

# Performs one HTTP request and prints "<status_code>\n<body>".
request() {
  local method="$1"
  local path="$2"
  local data="${3:-}"

  if [ -n "${data}" ]; then
    curl -s -m 5 -o - -w '\n%{http_code}' -X "${method}" "${BASE_URL}${path}" -d "${data}" 2>>"${LOG_FILE}"
  else
    curl -s -m 5 -o - -w '\n%{http_code}' -X "${method}" "${BASE_URL}${path}" 2>>"${LOG_FILE}"
  fi
}

# expect_contains METHOD PATH EXPECTED_STATUS SUBSTRING [BODY]
expect_contains() {
  local method="$1"
  local path="$2"
  local expected_status="$3"
  local substring="$4"
  local data="${5:-}"
  local response status body

  response="$(request "${method}" "${path}" "${data}")"
  status="$(printf '%s' "${response}" | tail -n1)"
  body="$(printf '%s' "${response}" | sed '$d')"

  if [[ "${status}" == "${expected_status}" && "${body}" == *"${substring}"* ]]; then
    PASS_COUNT=$((PASS_COUNT + 1))
    log "PASS ${method} ${path} status=${status} body=${body}"
  else
    FAIL_COUNT=$((FAIL_COUNT + 1))
    log "FAIL ${method} ${path} expected_status=${expected_status} expected_substring=${substring} status=${status:-<empty>} body=${body:-<empty>}"
  fi
  printf '%s' "${body}"
}

log "API test"
log "Device: ${DEVICE_IP}"
log "Started: $(date '+%Y-%m-%d %H:%M:%S %z')"
log ""

expect_contains GET /health 200 '"status"' >/dev/null

HEAD_RESPONSE="$(request HEAD /health)"
HEAD_STATUS="$(printf '%s' "${HEAD_RESPONSE}" | tail -n1)"
HEAD_BODY="$(printf '%s' "${HEAD_RESPONSE}" | sed '$d')"
if [[ "${HEAD_STATUS}" == "200" && -z "${HEAD_BODY}" ]]; then
  PASS_COUNT=$((PASS_COUNT + 1))
  log "PASS HEAD /health status=${HEAD_STATUS} body=<empty>"
else
  FAIL_COUNT=$((FAIL_COUNT + 1))
  log "FAIL HEAD /health expected_status=200 expected_body=<empty> status=${HEAD_STATUS:-<empty>} body=${HEAD_BODY:-<empty>}"
fi

expect_contains GET /api/v1/rtc 200 '"synchronized"' >/dev/null
expect_contains GET /api/v1/unknown-route 404 '"error":"not_found"' >/dev/null

SENSORS_BODY="$(expect_contains GET /api/v1/sensors 200 '"count"')"
SENSOR_COUNT="$(printf '%s' "${SENSORS_BODY}" | sed -n 's/.*"count":\([0-9][0-9]*\).*/\1/p')"

if [ -z "${SENSOR_COUNT}" ] || [ "${SENSOR_COUNT}" -eq 0 ] 2>/dev/null; then
  FAIL_COUNT=$((FAIL_COUNT + 1))
  log "FAIL no registered sensors reported"
else
  expect_contains GET /api/v1/temperature 200 "\"count\":${SENSOR_COUNT}" >/dev/null

  SENSOR_NUMBER=1
  while [ "${SENSOR_NUMBER}" -le "${SENSOR_COUNT}" ]; do
    expect_contains GET "/api/v1/sensors/${SENSOR_NUMBER}" 200 "\"index\":${SENSOR_NUMBER}" >/dev/null
    SENSOR_NUMBER=$((SENSOR_NUMBER + 1))
  done
fi

expect_contains GET /api/v1/sensors/0 404 '"error":"sensor_not_found"' >/dev/null
expect_contains GET /api/v1/sensors/250 404 '"error":"sensor_not_found"' >/dev/null

expect_contains GET /api/v1/thresholds 200 '"count":3' >/dev/null
expect_contains PUT /api/v1/thresholds/1 200 '"enabled":true' \
  '{"sensor_index":1,"temperature":42.5,"tone_hz":2500,"enabled":true}' >/dev/null
expect_contains GET /api/v1/thresholds 200 '"temperature":42.50' >/dev/null
expect_contains PUT /api/v1/thresholds/1 400 '"error"' '{"sensor_index":1}' >/dev/null
expect_contains PUT /api/v1/thresholds/4 400 '"error":"invalid_threshold"' \
  '{"sensor_index":1,"temperature":30.0,"tone_hz":2000}' >/dev/null

expect_contains POST /api/v1/buzzer/test 200 '"result":"ok"' >/dev/null

log ""
log "Finished: $(date '+%Y-%m-%d %H:%M:%S %z')"
log "Result: pass=${PASS_COUNT} fail=${FAIL_COUNT}"
log "Log: ${LOG_FILE}"

if [ "${FAIL_COUNT}" -ne 0 ]; then
  exit 1
fi
