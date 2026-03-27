#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LOG_DIR="${APP_ROOT}/logs"

mkdir -p "${LOG_DIR}"
export LD_LIBRARY_PATH="${APP_ROOT}/lib:${LD_LIBRARY_PATH:-}"

exec "${APP_ROOT}/bin/c-kafka-trace-producer" "${APP_ROOT}/config/application.properties"
  
