#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/arm64-release"

mkdir -p "${BUILD_DIR}"

cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
echo "Build completed:"
echo "  ${BUILD_DIR}/c-kafka-trace-producer"
echo "  ${BUILD_DIR}/c-kafka-trace-consumer"
  
