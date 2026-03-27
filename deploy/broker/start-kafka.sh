#!/usr/bin/env bash
set -euo pipefail

KAFKA_HOME="/opt/kafka"

echo "=== Start Kafka ==="

sudo -u kafka "${KAFKA_HOME}/bin/zookeeper-server-start.sh" -daemon "${KAFKA_HOME}/config/zookeeper.properties"
sleep 3

if ! pgrep -f zookeeper >/dev/null; then
  echo "Zookeeper failed to start."
  exit 1
fi

sudo -u kafka "${KAFKA_HOME}/bin/kafka-server-start.sh" -daemon "${KAFKA_HOME}/config/server.properties"
sleep 5

if ! pgrep -f kafka.Kafka >/dev/null; then
  echo "Kafka broker failed to start."
  echo "Check log: tail -f ${KAFKA_HOME}/logs/server.log"
  exit 1
fi

sleep 3

"${KAFKA_HOME}/bin/kafka-topics.sh" --create   --bootstrap-server localhost:9092   --topic trace-data   --partitions 3   --replication-factor 1   --if-not-exists   --config retention.ms=604800000   --config max.message.bytes=5242880 2>/dev/null || true

echo ""
echo "Topics:"
"${KAFKA_HOME}/bin/kafka-topics.sh" --list --bootstrap-server localhost:9092
  
