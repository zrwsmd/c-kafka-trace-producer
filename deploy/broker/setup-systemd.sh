#!/usr/bin/env bash
set -euo pipefail

KAFKA_HOME="/opt/kafka"

sudo tee /etc/systemd/system/zookeeper.service >/dev/null <<EOF
[Unit]
Description=Apache Zookeeper
After=network.target

[Service]
Type=simple
User=kafka
ExecStart=${KAFKA_HOME}/bin/zookeeper-server-start.sh ${KAFKA_HOME}/config/zookeeper.properties
ExecStop=${KAFKA_HOME}/bin/zookeeper-server-stop.sh
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

sudo tee /etc/systemd/system/kafka.service >/dev/null <<EOF
[Unit]
Description=Apache Kafka
After=zookeeper.service
Requires=zookeeper.service

[Service]
Type=simple
User=kafka
ExecStart=${KAFKA_HOME}/bin/kafka-server-start.sh ${KAFKA_HOME}/config/server.properties
ExecStop=${KAFKA_HOME}/bin/kafka-server-stop.sh
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable zookeeper kafka

echo "systemd units installed."
echo "Use:"
echo "  sudo systemctl start kafka"
echo "  sudo systemctl status kafka"
  
