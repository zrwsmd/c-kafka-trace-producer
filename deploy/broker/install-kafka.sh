
#!/usr/bin/env bash
set -euo pipefail

KAFKA_VERSION="3.6.1"
SCALA_VERSION="2.13"
KAFKA_HOME="/opt/kafka"
KAFKA_DATA="/data/kafka"

echo "=== Install Kafka ${KAFKA_VERSION} ==="

echo ">>> Check Java..."
if command -v java >/dev/null 2>&1; then
  echo "  Java already installed: $(java -version 2>&1 | head -n 1)"
else
  echo "  Installing Java runtime..."
  sudo apt update
  sudo apt install -y default-jre-headless || sudo apt install -y openjdk-17-jre-headless
fi

java -version

if ! command -v wget >/dev/null 2>&1; then
  sudo apt install -y wget
fi

echo ">>> Download Kafka..."
cd /tmp
rm -f kafka.tgz

wget -q --timeout=30 "https://downloads.apache.org/kafka/${KAFKA_VERSION}/kafka_${SCALA_VERSION}-${KAFKA_VERSION}.tgz" -O kafka.tgz || {
  wget -q --timeout=30 "https://archive.apache.org/dist/kafka/${KAFKA_VERSION}/kafka_${SCALA_VERSION}-${KAFKA_VERSION}.tgz" -O kafka.tgz || {
    wget -q --timeout=30 "https://mirrors.tuna.tsinghua.edu.cn/apache/kafka/${KAFKA_VERSION}/kafka_${SCALA_VERSION}-${KAFKA_VERSION}.tgz" -O kafka.tgz
  }
}

if [[ ! -f kafka.tgz ]]; then
  echo "Kafka download failed."
  exit 1
fi

sudo rm -rf "${KAFKA_HOME}"
sudo tar -xzf kafka.tgz -C /opt/
sudo mv "/opt/kafka_${SCALA_VERSION}-${KAFKA_VERSION}" "${KAFKA_HOME}"
rm -f kafka.tgz

sudo mkdir -p "${KAFKA_DATA}/zookeeper" "${KAFKA_DATA}/kafka-logs"
sudo useradd -r -s /sbin/nologin kafka 2>/dev/null || true
sudo chown -R kafka:kafka "${KAFKA_HOME}" "${KAFKA_DATA}"

echo ""
echo "Kafka installed:"
echo "  home: ${KAFKA_HOME}"
echo "  data: ${KAFKA_DATA}"
  