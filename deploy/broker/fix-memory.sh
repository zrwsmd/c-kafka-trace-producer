
#!/usr/bin/env bash
set -euo pipefail

KAFKA_HOME="/opt/kafka"

echo "=== Reduce Kafka memory footprint ==="

sudo cp "${KAFKA_HOME}/bin/kafka-server-start.sh" "${KAFKA_HOME}/bin/kafka-server-start.sh.bak"
sudo cp "${KAFKA_HOME}/bin/zookeeper-server-start.sh" "${KAFKA_HOME}/bin/zookeeper-server-start.sh.bak"

sudo sed -i 's/export KAFKA_HEAP_OPTS=.*/export KAFKA_HEAP_OPTS="-Xmx256M -Xms256M"/' "${KAFKA_HOME}/bin/kafka-server-start.sh" || true
sudo sed -i 's/export KAFKA_HEAP_OPTS=.*/export KAFKA_HEAP_OPTS="-Xmx128M -Xms128M"/' "${KAFKA_HOME}/bin/zookeeper-server-start.sh" || true

if ! grep -q '256M' "${KAFKA_HOME}/bin/kafka-server-start.sh"; then
  sudo sed -i '2i export KAFKA_HEAP_OPTS="-Xmx256M -Xms256M"' "${KAFKA_HOME}/bin/kafka-server-start.sh"
fi

if ! grep -q '128M' "${KAFKA_HOME}/bin/zookeeper-server-start.sh"; then
  sudo sed -i '2i export KAFKA_HEAP_OPTS="-Xmx128M -Xms128M"' "${KAFKA_HOME}/bin/zookeeper-server-start.sh"
fi

echo "Memory footprint reduced."
  