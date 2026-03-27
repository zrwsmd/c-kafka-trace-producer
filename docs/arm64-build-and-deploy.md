# ARM64 编译与部署指南

这份文档用于说明如何在 ARM64 Linux 机器上编译、打包并部署原生版
Kafka Trace Producer 到 ARM64 设备或工控机。

请先注意几点：

- 这份文档适用于 ARM64 构建机和 ARM64 运行设备。
- 按本文步骤生成的二进制是 `aarch64` 版本。
- 这类产物不能在 AMD64 机器上直接运行。
- 如果 Kafka broker 端已经部署完成并验证通过，就不需要再次执行
  `deploy/broker/` 下面的脚本。
- 如果构建机和目标设备的系统差异很大，可能会出现 glibc 版本不兼容。
  最稳妥的方式是让构建机和目标设备尽量接近，或者构建机系统更老一些。

## 1. 目标

在一台 ARM64 Linux 构建机上完成下面这些事情：

1. 拉取最新代码
2. 编译原生 producer 和 consumer
3. 打包生成 `release/arm64`
4. 把打包目录拷贝到 ARM64 device
5. 在目标设备上启动 producer，验证 Kafka 发送链路是否正常

## 2. 本文档使用的目录

本文默认：

- ARM64 构建机源码目录：

```bash
/opt/c-kafka-trace-producer
```

- ARM64 目标设备部署目录：

```bash
/opt/c-kafka-trace-producer-release
```

如果你的实际目录不同，请把下面命令中的路径统一替换掉。

## 3. ARM64 构建机：拉取最新代码

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

## 4. ARM64 构建机：给脚本加执行权限

```bash
cd /opt/c-kafka-trace-producer
chmod +x scripts/*.sh
chmod +x deploy/device/*.sh
chmod +x deploy/broker/*.sh
```

## 5. ARM64 构建机：安装编译依赖

如果你当前是 Debian 或 Ubuntu 的 `root` 用户，可以直接执行：

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

## 6. ARM64 构建机：检查或修改运行配置

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

## 7. ARM64 构建机：配置构建目录

建议单独使用 ARM64 的构建目录：

```bash
cd /opt/c-kafka-trace-producer
cmake -S . -B build/arm64-release -DCMAKE_BUILD_TYPE=Release
```

## 8. ARM64 构建机：编译原生二进制

```bash
cd /opt/c-kafka-trace-producer
cmake --build build/arm64-release --parallel "$(nproc)"
```

## 9. ARM64 构建机：检查编译结果

```bash
cd /opt/c-kafka-trace-producer
ls -lh build/arm64-release
file build/arm64-release/c-kafka-trace-producer
file build/arm64-release/c-kafka-trace-consumer
```

正常情况下你应该看到类似：

```bash
ELF 64-bit LSB pie executable, ARM aarch64
```

如果这里看到的是 `x86-64`，说明你当前编译环境不是 ARM64，或者编译方式不对。

## 10. ARM64 构建机：打包运行目录

使用项目里的打包脚本，并明确指定 ARM64 的构建输出目录：

```bash
cd /opt/c-kafka-trace-producer
./scripts/package-release.sh "$PWD/build/arm64-release" "$PWD/release/arm64"
```

成功后一般会看到类似输出：

```bash
Runtime package created at:
  /opt/c-kafka-trace-producer/release/arm64
```

## 11. ARM64 构建机：检查打包结果

```bash
cd /opt/c-kafka-trace-producer
find release/arm64 -maxdepth 3 -type f | sort
```

正常应该能看到这些目录下的文件：

- `release/arm64/bin`
- `release/arm64/lib`
- `release/arm64/config`
- `release/arm64/deploy/device`

## 12. ARM64 构建机：压缩打包目录

建议把运行目录打成一个 tar 包，再发到设备上：

```bash
cd /opt/c-kafka-trace-producer
tar -czf c-kafka-trace-producer-arm64.tar.gz -C release arm64
ls -lh c-kafka-trace-producer-arm64.tar.gz
```

## 13. 从 ARM64 构建机拷贝到 ARM64 设备

假设目标设备 IP 是 `DEVICE_IP`，执行：

```bash
cd /opt/c-kafka-trace-producer
scp c-kafka-trace-producer-arm64.tar.gz root@DEVICE_IP:/opt/
```

## 14. ARM64 目标设备：解压部署包

在目标设备上执行：

```bash
mkdir -p /opt/c-kafka-trace-producer-release
tar -xzf /opt/c-kafka-trace-producer-arm64.tar.gz -C /opt/c-kafka-trace-producer-release --strip-components=1
```

然后给脚本和二进制加权限：

```bash
cd /opt/c-kafka-trace-producer-release
chmod +x deploy/device/*.sh
chmod +x bin/*
```

## 15. ARM64 目标设备：第一次运行先以前台方式验证

这里非常重要：

- 第一次验证时一定先以前台方式运行
- 不要一上来就直接用 `deploy/device/start.sh`
- 先直接跑二进制本体，日志最清楚，方便定位问题

执行：

```bash
cd /opt/c-kafka-trace-producer-release
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```

## 16. ARM64 目标设备：正常启动时应该看到什么

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

## 17. ARM64 目标设备：验证消费

另开一个终端，运行原生 consumer：

```bash
cd /opt/c-kafka-trace-producer-release
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-consumer 47.129.128.147:9092 trace-data
```

注意：

- 这个 consumer 不会每条消息都打印
- 它默认每 1000 个 batch 才打印一次进度

## 18. ARM64 目标设备：前台验证通过后，再切后台运行

当前台运行确认没有问题后，再使用部署脚本：

```bash
cd /opt/c-kafka-trace-producer-release
./deploy/device/start.sh
./deploy/device/status.sh
./deploy/device/tail-log.sh
```

停止后台进程：

```bash
./deploy/device/stop.sh
```

## 19. ARM64 目标设备：如果要安装成 systemd 服务

如果你希望做成系统服务并开机自启，可以执行：

```bash
cd /opt/c-kafka-trace-producer-release
sudo ./deploy/device/install-service.sh /opt/c-kafka-trace-producer
sudo systemctl start c-kafka-trace-producer
sudo systemctl status c-kafka-trace-producer
```

之后常用命令：

```bash
sudo systemctl restart c-kafka-trace-producer
sudo systemctl stop c-kafka-trace-producer
sudo systemctl status c-kafka-trace-producer
```

## 20. 如果你修改了 application.properties

如果你修改的是源码目录下的配置文件：

```bash
/opt/c-kafka-trace-producer/config/application.properties
```

那么结论是：

- 不需要重新编译
- 需要重新打包
- 需要重新把新的打包目录发到设备上，或者手工覆盖设备上的配置文件
- 需要重启设备上的程序

### 20.1 不需要重新执行的步骤

只改配置文件时，不需要重新执行下面这些命令：

```bash
cmake -S . -B build/arm64-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/arm64-release --parallel "$(nproc)"
```

### 20.2 需要执行的步骤

在源码目录修改完配置后，执行：

```bash
cd /opt/c-kafka-trace-producer
./scripts/package-release.sh "$PWD/build/arm64-release" "$PWD/release/arm64"
tar -czf c-kafka-trace-producer-arm64.tar.gz -C release arm64
scp c-kafka-trace-producer-arm64.tar.gz root@DEVICE_IP:/opt/
```

然后在设备上重新解压覆盖，或者只覆盖配置文件。

### 20.3 如果你直接修改的是设备上的配置文件

如果你直接修改的是：

```bash
/opt/c-kafka-trace-producer-release/config/application.properties
```

那么：

- 不需要重新编译
- 不需要重新打包
- 直接重启程序即可

前台运行：

```bash
cd /opt/c-kafka-trace-producer-release
unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```

后台运行：

```bash
cd /opt/c-kafka-trace-producer-release
./deploy/device/stop.sh
./deploy/device/start.sh
```

如果是 systemd：

```bash
sudo systemctl restart c-kafka-trace-producer
```

## 21. 常见说明

- 如果 Kafka broker 已经部署并验证通过，就不用再执行 `deploy/broker/`
  下面的脚本。
- 按本文打出来的 ARM64 包只适用于 `aarch64` / `arm64` 环境。
- 第一次运行一定优先直接跑前台二进制。
- 确认稳定以后，再切换到 `deploy/device/start.sh` 后台运行。
- 如果设备报 `GLIBC_2.xx not found`，说明构建机系统比设备新太多了，
  需要换成更接近设备环境的 ARM64 Linux 来重新编译。

## 22. 一次性可复制执行的快捷版本

下面这段适合在 ARM64 构建机上执行：

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

cmake -S . -B build/arm64-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/arm64-release --parallel "$(nproc)"

./scripts/package-release.sh "$PWD/build/arm64-release" "$PWD/release/arm64"

tar -czf c-kafka-trace-producer-arm64.tar.gz -C release arm64
scp c-kafka-trace-producer-arm64.tar.gz root@DEVICE_IP:/opt/
```

下面这段适合在 ARM64 目标设备上执行：

```bash
mkdir -p /opt/c-kafka-trace-producer-release
tar -xzf /opt/c-kafka-trace-producer-arm64.tar.gz -C /opt/c-kafka-trace-producer-release --strip-components=1

cd /opt/c-kafka-trace-producer-release
chmod +x deploy/device/*.sh
chmod +x bin/*

unset LD_LIBRARY_PATH
./bin/c-kafka-trace-producer ./config/application.properties
```
