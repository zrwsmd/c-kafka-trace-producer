#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LOG_DIR="${APP_ROOT}/logs"
PID_FILE="${LOG_DIR}/producer.pid"
LOG_FILE="${LOG_DIR}/producer.log"

mkdir -p "${LOG_DIR}"

if [[ -f "${PID_FILE}" ]]; then
  existing_pid="$(cat "${PID_FILE}")"
  if kill -0 "${existing_pid}" >/dev/null 2>&1; then
    echo "Producer is already running with PID ${existing_pid}"
    exit 0
  fi
  rm -f "${PID_FILE}"
fi

nohup "${SCRIPT_DIR}/run-foreground.sh" >> "${LOG_FILE}" 2>&1 &
new_pid="$!"
echo "${new_pid}" > "${PID_FILE}"

echo "Producer started with PID ${new_pid}"
echo "Log file: ${LOG_FILE}"
  
