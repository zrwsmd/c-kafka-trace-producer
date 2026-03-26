
#!/usr/bin/env bash
set -euo pipefail

KAFKA_HOME="/opt/kafka"
KAFKA_DATA="/data/kafka"

PUBLIC_IP="$(curl -s http://checkip.amazonaws.com || curl -s http://ifconfig.me || echo "YOUR_PUBLIC_IP")"
echo "Detected public IP: ${PUBLIC_IP}"

cat > "${KAFKA_HOME}/config/zookeeper.properties" <<EOF
dataDir=${KAFKA_DATA}/zookeeper
clientPort=2181
maxClientCnxns=0
admin.enableServer=false
EOF

cat > "${KAFKA_HOME}/config/server.properties" <<EOF
broker.id=0
num.network.threads=3
num.io.threads=8
socket.send.buffer.bytes=102400
socket.receive.buffer.bytes=102400
socket.request.max.bytes=104857600

listeners=PLAINTEXT://0.0.0.0:9092
advertised.listeners=PLAINTEXT://${PUBLIC_IP}:9092

log.dirs=${KAFKA_DATA}/kafka-logs
num.partitions=3
num.recovery.threads.per.data.dir=1

log.retention.hours=168
log.retention.bytes=-1
log.segment.bytes=1073741824
log.retention.check.interval.ms=300000

message.max.bytes=5242880
replica.fetch.max.bytes=5242880

zookeeper.connect=localhost:2181
zookeeper.connection.timeout.ms=18000

default.replication.factor=1
min.insync.replicas=1
offsets.topic.replication.factor=1
transaction.state.log.replication.factor=1
transaction.state.log.min.isr=1
EOF

echo ""
echo "Kafka configured."
echo "advertised.listeners=PLAINTEXT://${PUBLIC_IP}:9092"
  