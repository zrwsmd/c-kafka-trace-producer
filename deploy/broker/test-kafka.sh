
#!/usr/bin/env bash
set -euo pipefail

KAFKA_HOME="/opt/kafka"

echo "=== Kafka health check ==="

if ! pgrep -f kafka.Kafka >/dev/null; then
  echo "Kafka broker is not running."
  exit 1
fi

echo "test-message-$(date +%s)" | "${KAFKA_HOME}/bin/kafka-console-producer.sh"   --bootstrap-server localhost:9092   --topic trace-data >/dev/null 2>&1

timeout 5 "${KAFKA_HOME}/bin/kafka-console-consumer.sh"   --bootstrap-server localhost:9092   --topic trace-data   --from-beginning   --max-messages 1 >/dev/null 2>&1

echo "Kafka producer/consumer test passed."
  