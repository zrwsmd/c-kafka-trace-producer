#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THIRD_PARTY_ROOT="${PROJECT_ROOT}/third_party"
ARCHIVE_DIR="${THIRD_PARTY_ROOT}/archives"
SOURCE_DIR="${THIRD_PARTY_ROOT}/src"
INSTALL_DIR="${THIRD_PARTY_ROOT}/rdkafka/install"

RDKAFKA_VERSION="${RDKAFKA_VERSION:-2.5.0}"
ARCHIVE_PATH="${ARCHIVE_DIR}/librdkafka-${RDKAFKA_VERSION}.tar.gz"
DOWNLOAD_URL="https://github.com/confluentinc/librdkafka/archive/refs/tags/v${RDKAFKA_VERSION}.tar.gz"

mkdir -p "${ARCHIVE_DIR}" "${SOURCE_DIR}" "${INSTALL_DIR}"

if [[ ! -f "${ARCHIVE_PATH}" ]]; then
  echo "Downloading librdkafka v${RDKAFKA_VERSION}..."
  curl -L "${DOWNLOAD_URL}" -o "${ARCHIVE_PATH}"
fi

rm -rf "${SOURCE_DIR}/librdkafka-${RDKAFKA_VERSION}"
tar -xzf "${ARCHIVE_PATH}" -C "${SOURCE_DIR}"

pushd "${SOURCE_DIR}/librdkafka-${RDKAFKA_VERSION}" >/dev/null
./configure --prefix="${INSTALL_DIR}"
make -j"$(nproc)"
make install
popd >/dev/null

echo ""
echo "Local librdkafka build completed."
echo "RDKAFKA_ROOT=${INSTALL_DIR}"
  
