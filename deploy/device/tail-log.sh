#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LOG_FILE="${APP_ROOT}/logs/producer.log"

mkdir -p "${APP_ROOT}/logs"
touch "${LOG_FILE}"
tail -f "${LOG_FILE}"
  
