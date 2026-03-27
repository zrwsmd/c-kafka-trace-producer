# AMD64 编译与部署指南

这份文档用于说明如何在 AMD64 Linux 机器上编译、打包并运行原生版
Kafka Trace Producer。

本文档基于当前已经验证通过的环境整理：

- 架构：`x86_64`
- 操作系统：Debian 12
- glibc：2.36

请先注意几点：

- 这份文档只适用于 AMD64 环境的编译验证和部署。
- 按本文步骤生成的二进制是 `x86_64` 版本。
- 这个产物不能直接拷到 ARM64 工控机上运行。
- 如果你的 Kafka broker 端已经部署完成并验证通过，就不需要再次执行
  `deploy/broker/` 下面的脚本。

## 1. 目标

在一台 AMD64 Linux 服务器上完成下面这些事情：

1. 拉取最新代码
2. 编译原生 producer 和 consumer
3. 打包生成 `release/amd64`
4. 在本机启动 producer，验证 Kafka 发送链路是否正常

## 2. 本文档使用的目录

本文默认项目目录为：

```bash
/opt/c-kafka-trace-producer
```

如果你的实际目录不同，请把下面命令中的路径统一替换掉。

## 3. 拉取最新代码

如果仓库还没有 clone：

```bash
cd /opt
git clone https://github.com/zrwsmd/c-kafka-trace-producer.git
```

然后每次编译前都先更新到最新版本：

```bash
cd /opt/c-kafka-trace-producer
git pull
```

## 4. 给脚本加执行权限

```bash
cd /opt/c-kafka-trace-producer
chmod +x scripts/*.sh
chmod +x deploy/device/*.sh
chmod +x deploy/broker/*.sh
```

## 5. 安装编译依赖

如果你当前是 Debian 12 的 `root` 用户，直接执行：

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

如果你不是 `root`，就在 `apt` 前面加上 `sudo`。

## 6. 检查或修改运行配置

运行配置文件在：

```bash
/opt/c-kafka-trace-producer/config/application.properties
```

先打印当前配置：

```bash
cd /opt/c-kafka-trace-producer
cat config/application.properties
```

如果你要修改 broker 地址，可以执行：

```bash
sed -i 's#^kafka.bootstrap.servers=.*#kafka.bootstrap.servers=47.129.128.147:9092#' config/application.properties
cat config/application.properties
```

至少要确认下面这些值是否正确：

```properties
kafka.bootstrap.servers=47.129.128.147:9092
kafka.topic=trace-data
trace.batch.size=1000
trace.max.frames=1000000
trace.send.interval.ms=100
```

## 7. 配置构建目录

建议单独使用 AMD64 的构建目录：

```bash
cd /opt/c-kafka-trace-producer
cmake -S . -B build/amd64-release -DCMAKE_BUILD_TYPE=Release
```

## 8. 编译原生二进制

```bash
cd /opt/c-kafka-trace-producer
cmake --build build/amd64-release --parallel "$(nproc)"
```

## 9. 检查编译结果

```bash
cd /opt/c-kafka-trace-producer
ls -lh build/amd64-release
file build/amd64-release/c-kafka-trace-producer
file build/amd64-release/c-kafka-trace-consumer
```

正常情况下你应该看到类似：

```bash
ELF 64-bit LSB pie executable, x86-64
```

## 10. 打包运行目录

使用项目里的打包脚本，并明确指定 AMD64 的构建输出目录：

```bash
cd /opt/c-kafka-trace-producer
./scripts/package-release.sh "$PWD/build/amd64-release" "$PWD/release/amd64"
```

成功后一般会看到类似输出：

```bash
Runtime package created at:
  /opt/c-kafka-trace-producer/release/amd64
```

## 11. 检查打包结果

```bash
cd /opt/c-kafka-trace-producer
find release/amd64 -maxdepth 3 -type f | sort
```

正常应该能看到这些目录下的文件：

- `release/amd64/bin`
- `release/amd64/lib`
- `release/amd64/config`
- `release/amd64/deploy/device`

## 12. 第一次运行：先以前台方式验证

这里非常重要：

- 第一次验证时一定先以前台方式运行
- 不要一上来就直接用 `deploy/device/start.sh`
- 先直接跑二进制本体，日志最清楚，方便定位问题

执行：

```bash
cd /opt/c-kafka-trace-producer/release/amd64
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```

## 13. 正常启动时应该看到什么

正常情况下，启动日志大致会是这样：

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

如果你已经看到 `progress: 1.0%`、`2.0%`、`3.0%` 这种日志，就说明
producer 已经在正常向 Kafka 发送数据了。

## 14. 验证消费

另开一个终端，运行原生 consumer：

```bash
cd /opt/c-kafka-trace-producer/release/amd64
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-consumer 47.129.128.147:9092 trace-data
```

注意：

- 这个 consumer 不会每条消息都打印
- 它默认每 1000 个 batch 才打印一次进度

## 15. 前台验证通过后，再切后台运行

当前台运行确认没有问题后，再使用部署脚本：

```bash
cd /opt/c-kafka-trace-producer/release/amd64
./deploy/device/start.sh
./deploy/device/status.sh
./deploy/device/tail-log.sh
```

停止后台进程：

```bash
./deploy/device/stop.sh
```

## 16. 打成 tar 包

如果你想把 AMD64 运行目录打成压缩包：

```bash
cd /opt/c-kafka-trace-producer
tar -czf c-kafka-trace-producer-amd64.tar.gz -C release amd64
ls -lh c-kafka-trace-producer-amd64.tar.gz
```

## 17. 拷到另一台 AMD64 Linux 机器运行

先在当前机器打包：

```bash
cd /opt/c-kafka-trace-producer
tar -czf c-kafka-trace-producer-amd64.tar.gz -C release amd64
```

然后拷过去：

```bash
scp c-kafka-trace-producer-amd64.tar.gz root@TARGET_IP:/opt/
```

在目标 AMD64 机器上解压并运行：

```bash
mkdir -p /opt/c-kafka-trace-producer-release
tar -xzf /opt/c-kafka-trace-producer-amd64.tar.gz -C /opt/c-kafka-trace-producer-release --strip-components=1

cd /opt/c-kafka-trace-producer-release
chmod +x deploy/device/*.sh
chmod +x bin/*

unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```

## 18. 常见说明

- 如果 Kafka broker 已经部署并验证通过，就不用再执行 `deploy/broker/`
  下面的脚本。
- 按本文打出来的 AMD64 包只适用于 `x86_64` 环境。
- 第一次运行一定优先直接跑前台二进制。
- 确认稳定以后，再切换到 `deploy/device/start.sh` 后台运行。

## 19. 一次性可复制执行的快捷版本

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
