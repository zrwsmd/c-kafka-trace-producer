
# c-kafka-trace-producer

This project is the native C replacement for the Java-based Kafka trace producer.
It targets Linux devices, including Buildroot ARM64 systems, that cannot install Java or Node.js on the target.

The workflow is:

1. Prepare or generate the project on your development machine.
2. Build a native Linux binary for the target architecture.
3. Package the executable together with config, scripts, and any required runtime libraries.
4. Copy the packaged folder to the device and run it directly.

## What Is Included

- Native C producer that mirrors the Java producer flow:
  - loads `application.properties`
  - generates trace batches
  - sends JSON payloads to Kafka with synchronous delivery confirmation
  - prints progress and summary
- Native C simple consumer example for link verification
- Minimal `librdkafka` probe executable for target-side validation
- Build scripts for native Linux ARM64 and Windows-to-ARM64 cross-compilation
- Optional local `librdkafka` build script
- Packaging scripts that bundle binaries and runtime files into `release/arm64`
- Device deployment scripts for foreground mode, background mode, and systemd
- Kafka broker deployment scripts carried over from the original project

## Important Notes

- The target device does not need Java.
- The target device does not need Node.js.
- The project has been migrated from C++ to pure C so it can build with ARM64 toolchains that do not provide a complete `libstdc++` SDK.
- This native version uses `librdkafka` for Kafka protocol support.
- When cross-compiling, point `RDKAFKA_ROOT` at a compiled ARM64 `librdkafka` install prefix.
- If your cross toolchain misses development symlinks for `-lpthread`, `-ldl`, `-lm`, or `-lrt`, prepare a local shim directory under `build/toolchain-shim` and pass it as an extra library directory.
- To reduce dependency friction on constrained devices, the default config uses `kafka.compression.type=none`.
- Only use `c-kafka-rdkafka-probe --require ...` for features that were actually enabled when your `librdkafka` was built.

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

### Option C: Cross-build on Windows x86_64 with direct GCC invocation

Use this when your host is Windows x86_64 and you have:

- an `aarch64-none-linux-gnu` cross toolchain with sysroot
- an ARM64 `librdkafka` install prefix
- no `cmake` or `ninja` in `PATH`, or you prefer a simpler direct build flow

Run:

```powershell
.\scripts\build-arm64-cross-windows-direct.ps1 `
  -ToolchainRoot C:\path\to\toolchain `
  -RdkafkaRoot C:\path\to\rdkafka-arm64

.\scripts\package-release-cross-windows.ps1 `
  -ToolchainRoot C:\path\to\toolchain `
  -RdkafkaRoot C:\path\to\rdkafka-arm64
```

The direct build script automatically prepares `build/toolchain-shim` when needed for missing ARM64 development-library aliases.

### Option D: Cross-build on Windows x86_64 with CMake

Use this when `cmake` and `ninja` are available in `PATH`:

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

Examples:

```bash
./build/arm64-release/c-kafka-rdkafka-probe
./build/arm64-release/c-kafka-rdkafka-probe --require gzip
```

If your ARM64 `librdkafka` was built without `ssl` or `sasl`, do not require those features in the probe command.

## Detailed Guide

See `docs/arm64-build-and-deploy.md`
See `docs/librdkafka-arm64-cross-build.md`
See `docs/buildroot-target-deploy-and-run.md`
See `docs/project-build-artifacts-explained.md`
See `docs/amd64-build-and-deploy.md`
  
