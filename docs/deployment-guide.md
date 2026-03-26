
# Deployment Guide

## Goal

Run the producer on an ARM64 industrial PC without Java.

## Recommended Flow

1. Generate this project on Windows:

```powershell
node generate-c-kafka-trace-producer.js
```

2. Copy the generated project to a Linux ARM64 build machine or an ARM64 build container.

3. Build the native executables:

```bash
./scripts/install-build-deps-ubuntu.sh
./scripts/build-arm64-native.sh
```

If the build machine should not depend on a system-installed librdkafka:

```bash
./scripts/build-rdkafka-local.sh
./scripts/build-arm64-local-rdkafka.sh
```

4. Package the runtime directory:

```bash
./scripts/package-release.sh
```

5. Copy release/arm64 to the device:

```bash
scp -r release/arm64 root@DEVICE_IP:/opt/c-kafka-trace-producer-release
```

6. Start manually on the device:

```bash
cd /opt/c-kafka-trace-producer-release
./deploy/device/start.sh
./deploy/device/status.sh
```

7. Or install as a systemd service:

```bash
cd /opt/c-kafka-trace-producer-release
sudo ./deploy/device/install-service.sh /opt/c-kafka-trace-producer
sudo systemctl enable c-kafka-trace-producer
sudo systemctl start c-kafka-trace-producer
```

## Why The Packaging Step Matters

The industrial PC should not need Java, Maven, or Node.js.
It should only receive:

- the native producer binary
- the config file
- the runtime shell scripts
- the required shared libraries bundled under lib/

## Runtime Logs

The deployment scripts write logs to:

```
logs/producer.log
```

Use:

```bash
./deploy/device/tail-log.sh
```

## Config Changes

Edit:

```
config/application.properties
```

before packaging or directly on the device.

## Performance Notes

- Lower trace.send.interval.ms to increase throughput.
- Keep kafka.compression.type=none first when validating the native build.
- After the path is stable, test lz4 only if the librdkafka build supports it.
  