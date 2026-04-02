
# c-kafka-trace-producer

This project is the native C++ replacement for the Java-based Kafka trace producer.
It is designed for an ARM64 industrial PC that cannot install Java or Node.js on the target.

The workflow is:

1. Generate this project on your development machine with Node.js.
2. Build a native ARM64 binary on a Linux ARM64 build machine or ARM64 container.
3. Package the binary together with required shared libraries.
4. Copy the packaged folder to the industrial PC and run it directly, without Java.

## What Is Included

- Native C++ producer that mirrors the Java producer flow:
  - loads application.properties
  - generates trace batches
  - sends JSON payloads to Kafka with synchronous delivery confirmation
  - prints progress and summary
- Native C++ simple consumer example for link verification
- Build scripts for ARM64 Linux
- Optional local librdkafka build script
- Packaging scripts that bundle the binary and runtime libraries into release/arm64
- Device deployment scripts for foreground mode, background mode, and systemd
- Kafka broker deployment scripts carried over from the original project

## Important Notes

- The target industrial PC does not need Java.
- The target industrial PC does not need Node.js.
- The target industrial PC only needs the packaged native files copied to it.
- This native version uses librdkafka for Kafka protocol support.
- To reduce native dependency friction on constrained devices, the generated config defaults to:
  - kafka.compression.type=none
- You can switch it back to lz4 later if your librdkafka build supports it.

## Project Layout

```
config/
  application.properties
deploy/
  broker/
  device/
docs/
include/
scripts/
src/
CMakeLists.txt
```

## Build Paths

### Option A: Build against system-installed librdkafka

Run these commands on a Linux ARM64 build machine:

```bash
./scripts/install-build-deps-ubuntu.sh
./scripts/build-arm64-native.sh
./scripts/package-release.sh
```

### Option B: Build librdkafka locally inside the project

Use this if you do not want to rely on a system-wide librdkafka installation on the build machine:

```bash
./scripts/install-build-deps-ubuntu.sh
./scripts/build-rdkafka-local.sh
./scripts/build-arm64-local-rdkafka.sh
./scripts/package-release.sh
```

### Option C: Cross-build on Windows x86_64 with an AArch64 SDK

Use this when your host is Windows x86_64 and you have:

- an `aarch64-none-linux-gnu` cross toolchain with sysroot
- an ARM64 `librdkafka` install prefix
- `cmake` plus `ninja` in `PATH`

Run:

```powershell
.\scripts\validate-arm64-toolchain-windows.ps1 -ToolchainRoot C:\path\to\toolchain
.\scripts\build-arm64-cross-windows.ps1 -ToolchainRoot C:\path\to\toolchain -RdkafkaRoot C:\path\to\rdkafka-arm64
.\scripts\package-release-cross-windows.ps1 -ToolchainRoot C:\path\to\toolchain -RdkafkaRoot C:\path\to\rdkafka-arm64
```

The detailed Buildroot deployment guide, including Windows cross-build notes, is in `docs/arm64-build-and-deploy.md`.

## Deploy To The Industrial PC

After packaging, copy the generated `release/arm64` folder to the industrial PC, then:

```bash
cd /path/to/release/arm64
./deploy/device/start.sh
```

Or install it as a service:

```bash
cd /path/to/release/arm64
sudo ./deploy/device/install-service.sh /opt/c-kafka-trace-producer
sudo systemctl status c-kafka-trace-producer
```

## Default Runtime Config

The generated config keeps the current producer behavior in spirit:

- kafka.topic=trace-data
- trace.batch.size=1000
- trace.max.frames=1000000
- trace.period.ms=1
- trace.send.interval.ms=100
- kafka.acks=all
- kafka.enable.idempotence=true

Adjust config/application.properties before deployment.

## Consumer Example

The build also generates a native consumer example:

```bash
./build/arm64-release/c-kafka-trace-consumer 47.129.128.147:9092 trace-data
```

## librdkafka Probe

The build also generates a minimal `librdkafka` probe executable. Use it on the target device to verify:

- the shared library loads successfully
- `rd_kafka_new()` succeeds
- required builtin features are present

Example:

```bash
./build/arm64-release/c-kafka-rdkafka-probe --require ssl,sasl
```

## Detailed Guide

See docs/deployment-guide.md
See docs/amd64-build-and-deploy.md
  
