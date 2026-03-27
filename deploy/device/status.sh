#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PID_FILE="${APP_ROOT}/logs/producer.pid"

if command -v systemctl >/dev/null 2>&1 && systemctl list-unit-files | grep -q '^c-kafka-trace-producer.service'; then
  echo "systemd service status:"
  systemctl status c-kafka-trace-producer --no-pager || true
  echo ""
fi

if [[ ! -f "${PID_FILE}" ]]; then
  echo "Producer is not running."
  exit 0
fi

pid="$(cat "${PID_FILE}")"
if kill -0 "${pid}" >/dev/null 2>&1; then
  echo "Producer is running with PID ${pid}"
else
  echo "Producer PID file exists but process is not running."
fi
  
