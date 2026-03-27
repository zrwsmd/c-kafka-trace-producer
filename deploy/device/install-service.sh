#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
TARGET_ROOT="${1:-/opt/c-kafka-trace-producer}"
SERVICE_NAME="c-kafka-trace-producer"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

echo "Installing runtime files to ${TARGET_ROOT}"
sudo mkdir -p "${TARGET_ROOT}"
sudo cp -R "${SOURCE_ROOT}/." "${TARGET_ROOT}/"
sudo chmod +x "${TARGET_ROOT}/deploy/device/"*.sh

sudo tee "${SERVICE_FILE}" >/dev/null <<EOF
[Unit]
Description=Native Kafka Trace Producer
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=${TARGET_ROOT}
ExecStart=${TARGET_ROOT}/deploy/device/run-foreground.sh
Restart=on-failure
RestartSec=5
Environment=LD_LIBRARY_PATH=${TARGET_ROOT}/lib

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "${SERVICE_NAME}"

echo ""
echo "Service installed."
echo "Start it with:"
echo "  sudo systemctl start ${SERVICE_NAME}"
echo ""
echo "Check it with:"
echo "  sudo systemctl status ${SERVICE_NAME}"
  
