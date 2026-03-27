#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PID_FILE="${APP_ROOT}/logs/producer.pid"

if [[ ! -f "${PID_FILE}" ]]; then
  echo "PID file not found. Producer may not be running."
  exit 0
fi

pid="$(cat "${PID_FILE}")"
if kill -0 "${pid}" >/dev/null 2>&1; then
  kill "${pid}"
  echo "Stop signal sent to PID ${pid}"
else
  echo "Process ${pid} is not running."
fi

rm -f "${PID_FILE}"
  
