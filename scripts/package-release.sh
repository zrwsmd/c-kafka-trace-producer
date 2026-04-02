#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_ROOT}/build/arm64-release}"
OUTPUT_DIR="${2:-${PROJECT_ROOT}/release/arm64}"

PRODUCER_BIN="${BUILD_DIR}/c-kafka-trace-producer"
CONSUMER_BIN="${BUILD_DIR}/c-kafka-trace-consumer"
PROBE_BIN="${BUILD_DIR}/c-kafka-rdkafka-probe"

if [[ ! -x "${PRODUCER_BIN}" ]]; then
  echo "Producer binary not found: ${PRODUCER_BIN}"
  echo "Run a build script first."
  exit 1
fi

if [[ ! -x "${CONSUMER_BIN}" ]]; then
  echo "Consumer binary not found: ${CONSUMER_BIN}"
  echo "Run a build script first."
  exit 1
fi

if [[ ! -x "${PROBE_BIN}" ]]; then
  echo "Probe binary not found: ${PROBE_BIN}"
  echo "Run a build script first."
  exit 1
fi

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}/bin" "${OUTPUT_DIR}/config" "${OUTPUT_DIR}/lib" "${OUTPUT_DIR}/deploy"

cp "${PRODUCER_BIN}" "${OUTPUT_DIR}/bin/"
cp "${CONSUMER_BIN}" "${OUTPUT_DIR}/bin/"
cp "${PROBE_BIN}" "${OUTPUT_DIR}/bin/"
cp "${PROJECT_ROOT}/config/application.properties" "${OUTPUT_DIR}/config/"
cp -R "${PROJECT_ROOT}/deploy/device" "${OUTPUT_DIR}/deploy/"

chmod +x "${OUTPUT_DIR}/deploy/device/"*.sh

should_copy_library() {
  local base_name
  base_name="$(basename "$1")"
  case "${base_name}" in
    linux-vdso.so*|libc.so*|libm.so*|libpthread.so*|librt.so*|libdl.so*|ld-linux*.so*|libresolv.so*)
      return 1
      ;;
    *)
      return 0
      ;;
  esac
}

copy_runtime_libraries() {
  local binary_path="$1"
  while IFS= read -r library_path; do
    [[ -z "${library_path}" ]] && continue
    if should_copy_library "${library_path}"; then
      cp -L "${library_path}" "${OUTPUT_DIR}/lib/"
    fi
  done < <(ldd "${binary_path}" | awk '
    /=>/ && $3 ~ /^\// { print $3 }
    /^[[:space:]]*\// { print $1 }
  ' | sort -u)
}

copy_runtime_libraries "${PRODUCER_BIN}"
copy_runtime_libraries "${CONSUMER_BIN}"
copy_runtime_libraries "${PROBE_BIN}"

if [[ -n "${RDKAFKA_ROOT:-}" && -d "${RDKAFKA_ROOT}/lib" ]]; then
  find "${RDKAFKA_ROOT}/lib" -maxdepth 1 -type f \( -name "librdkafka.so*" -o -name "librdkafka++.so*" \)     -exec cp -L {} "${OUTPUT_DIR}/lib/" \;
fi

cat > "${OUTPUT_DIR}/README-runtime.txt" <<'EOF'
This directory is the runtime package for the ARM64 device.

Contents:
- bin/: native executables
- lib/: runtime shared libraries
- config/: application.properties
- deploy/device/: start/stop/service scripts

Manual start:
  ./deploy/device/start.sh

Validate librdkafka on the target:
  ./bin/c-kafka-rdkafka-probe --require ssl,sasl

Install as service:
  sudo ./deploy/device/install-service.sh /opt/c-kafka-trace-producer
EOF

echo ""
echo "Runtime package created at:"
echo "  ${OUTPUT_DIR}"
  
