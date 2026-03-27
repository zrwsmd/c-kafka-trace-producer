#!/usr/bin/env bash
set -euo pipefail

echo "Installing native build dependencies..."
sudo apt update
sudo apt install -y   build-essential   cmake   pkg-config   curl   ca-certificates   git   python3   tar   unzip   zlib1g-dev   libssl-dev   libsasl2-dev   libzstd-dev   liblz4-dev   librdkafka-dev

echo "Build dependencies installed."
  
