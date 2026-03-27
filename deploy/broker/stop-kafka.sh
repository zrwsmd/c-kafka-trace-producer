#!/usr/bin/env bash
set -euo pipefail

KAFKA_HOME="/opt/kafka"

echo "Stopping Kafka broker..."
"${KAFKA_HOME}/bin/kafka-server-stop.sh" || true
sleep 3

echo "Stopping Zookeeper..."
"${KAFKA_HOME}/bin/zookeeper-server-stop.sh" || true

echo "Kafka stopped."
  
