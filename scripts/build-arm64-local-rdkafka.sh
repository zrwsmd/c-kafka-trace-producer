
#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/arm64-release"
RDKAFKA_ROOT="${PROJECT_ROOT}/third_party/rdkafka/install"

if [[ ! -d "${RDKAFKA_ROOT}" ]]; then
  echo "Local librdkafka install not found: ${RDKAFKA_ROOT}"
  echo "Run ./scripts/build-rdkafka-local.sh first."
  exit 1
fi

mkdir -p "${BUILD_DIR}"

cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}"   -DCMAKE_BUILD_TYPE=Release   -DRDKAFKA_ROOT="${RDKAFKA_ROOT}"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
echo "Build completed with local librdkafka:"
echo "  ${BUILD_DIR}/c-kafka-trace-producer"
echo "  ${BUILD_DIR}/c-kafka-trace-consumer"
  