# AMD64 Build And Deploy Guide

This document is the step-by-step guide for building, packaging, and running
the native Kafka trace producer on an AMD64 Linux machine.

It is based on the current verified workflow on:

- Architecture: `x86_64`
- OS: Debian 12
- glibc: 2.36

Important:

- This guide is for AMD64 validation and deployment only.
- The generated binaries from this guide are `x86_64` binaries.
- They cannot be copied directly to an ARM64 industrial PC.
- The broker-side Kafka scripts do not need to be executed again if your Kafka
  server is already running normally.

## 1. Goal

Use an AMD64 Linux server to:

1. pull the latest native project code
2. build the native producer and consumer
3. package the runtime folder under `release/amd64`
4. run the producer locally to verify the full Kafka send flow

## 2. Paths Used In This Guide

This document assumes the project lives at:

```bash
/opt/c-kafka-trace-producer
```

If your path is different, replace it consistently in the commands below.

## 3. Pull The Latest Code

If the repo has not been cloned yet:

```bash
cd /opt
git clone https://github.com/zrwsmd/c-kafka-trace-producer.git
```

Then always update to the latest version before building:

```bash
cd /opt/c-kafka-trace-producer
git pull
```

## 4. Add Execute Permission To Scripts

```bash
cd /opt/c-kafka-trace-producer
chmod +x scripts/*.sh
chmod +x deploy/device/*.sh
chmod +x deploy/broker/*.sh
```

## 5. Install Build Dependencies

On Debian 12 as `root`:

```bash
apt update
apt install -y \
  build-essential \
  cmake \
  pkg-config \
  curl \
  ca-certificates \
  git \
  python3 \
  tar \
  unzip \
  file \
  zlib1g-dev \
  libssl-dev \
  libsasl2-dev \
  libzstd-dev \
  liblz4-dev \
  librdkafka-dev
```

If you are not `root`, add `sudo` in front of `apt`.

## 6. Check Or Update Runtime Config

The runtime config file is:

```bash
/opt/c-kafka-trace-producer/config/application.properties
```

Print the current config:

```bash
cd /opt/c-kafka-trace-producer
cat config/application.properties
```

If you want to update the broker address:

```bash
sed -i 's#^kafka.bootstrap.servers=.*#kafka.bootstrap.servers=47.129.128.147:9092#' config/application.properties
cat config/application.properties
```

At minimum, confirm these values:

```properties
kafka.bootstrap.servers=47.129.128.147:9092
kafka.topic=trace-data
trace.batch.size=1000
trace.max.frames=1000000
trace.send.interval.ms=100
```

## 7. Configure The Build Directory

Use a dedicated AMD64 build directory:

```bash
cd /opt/c-kafka-trace-producer
cmake -S . -B build/amd64-release -DCMAKE_BUILD_TYPE=Release
```

## 8. Build The Native Binaries

```bash
cd /opt/c-kafka-trace-producer
cmake --build build/amd64-release --parallel "$(nproc)"
```

## 9. Verify The Build Output

```bash
cd /opt/c-kafka-trace-producer
ls -lh build/amd64-release
file build/amd64-release/c-kafka-trace-producer
file build/amd64-release/c-kafka-trace-consumer
```

Expected result:

```bash
ELF 64-bit LSB pie executable, x86-64
```

## 10. Package The Runtime Folder

Use the generated packaging script and point it at the AMD64 build directory:

```bash
cd /opt/c-kafka-trace-producer
./scripts/package-release.sh "$PWD/build/amd64-release" "$PWD/release/amd64"
```

When it succeeds, you should see a message like:

```bash
Runtime package created at:
  /opt/c-kafka-trace-producer/release/amd64
```

## 11. Verify The Runtime Package

```bash
cd /opt/c-kafka-trace-producer
find release/amd64 -maxdepth 3 -type f | sort
```

You should see files under:

- `release/amd64/bin`
- `release/amd64/lib`
- `release/amd64/config`
- `release/amd64/deploy/device`

## 12. First Run: Use Foreground Verification

Important:

- The first verification run should be done in the foreground.
- Do not start with `deploy/device/start.sh`.
- Use the executable directly first so the logs are easier to read.

Run:

```bash
cd /opt/c-kafka-trace-producer/release/amd64
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```

## 13. What Normal Startup Looks Like

Expected startup log shape:

```text
=== Native Kafka Trace Producer ===
=== Trace Config ===
[KafkaProducer] creating Kafka config...
[KafkaProducer] calling rd_kafka_new...
[KafkaProducer] rd_kafka_new returned
[KafkaProducer] initialized: ...
>>> sending 1000000 frames to Kafka ...
[TraceSimulator] starting: ...
[TraceSimulator] send loop started: ...
[2026-xx-xx xx:xx:xx] [TraceSimulator] progress: 1.0% ...
```

If you see `progress: 1.0%`, `2.0%`, `3.0%` and so on, the producer is already
sending data to Kafka successfully.

## 14. Validate Consumption

Open another terminal and run the native consumer:

```bash
cd /opt/c-kafka-trace-producer/release/amd64
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-consumer 47.129.128.147:9092 trace-data
```

Note:

- The consumer does not print every single message.
- It prints progress every 1000 batches.

## 15. Background Run After Foreground Validation

Once the foreground run is confirmed healthy, you can use the deployment
scripts:

```bash
cd /opt/c-kafka-trace-producer/release/amd64
./deploy/device/start.sh
./deploy/device/status.sh
./deploy/device/tail-log.sh
```

Stop the background process with:

```bash
./deploy/device/stop.sh
```

## 16. Create A Tar Package

If you want to archive the AMD64 runtime package:

```bash
cd /opt/c-kafka-trace-producer
tar -czf c-kafka-trace-producer-amd64.tar.gz -C release amd64
ls -lh c-kafka-trace-producer-amd64.tar.gz
```

## 17. Copy To Another AMD64 Linux Machine

Create the archive:

```bash
cd /opt/c-kafka-trace-producer
tar -czf c-kafka-trace-producer-amd64.tar.gz -C release amd64
```

Copy it:

```bash
scp c-kafka-trace-producer-amd64.tar.gz root@TARGET_IP:/opt/
```

Extract and run on the target AMD64 machine:

```bash
mkdir -p /opt/c-kafka-trace-producer-release
tar -xzf /opt/c-kafka-trace-producer-amd64.tar.gz -C /opt/c-kafka-trace-producer-release --strip-components=1

cd /opt/c-kafka-trace-producer-release
chmod +x deploy/device/*.sh
chmod +x bin/*

unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```

## 18. Common Notes

- The broker-side scripts under `deploy/broker/` are not needed again if Kafka
  has already been deployed and verified.
- The AMD64 package generated from this guide is for `x86_64` only.
- The first run should always use the foreground executable directly.
- After verification, you can switch to `deploy/device/start.sh`.

## 19. One-Paste Quick Version

```bash
cd /opt
if [ ! -d /opt/c-kafka-trace-producer ]; then
  git clone https://github.com/zrwsmd/c-kafka-trace-producer.git
fi

cd /opt/c-kafka-trace-producer
git pull

chmod +x scripts/*.sh
chmod +x deploy/device/*.sh
chmod +x deploy/broker/*.sh

apt update
apt install -y build-essential cmake pkg-config curl ca-certificates git python3 tar unzip file zlib1g-dev libssl-dev libsasl2-dev libzstd-dev liblz4-dev librdkafka-dev

cmake -S . -B build/amd64-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/amd64-release --parallel "$(nproc)"

./scripts/package-release.sh "$PWD/build/amd64-release" "$PWD/release/amd64"

cd release/amd64
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```
