# Buildroot 目标机部署与运行指南

本文档只关注一件事：

- 已经在开发机上生成好了 `c-kafka-trace-producer-arm64.tar.gz`
- 现在要把它上传到 Buildroot ARM64 目标机
- 然后在目标机上完成解压、改配置、启动、查看状态、停止

本文档按当前已经验证通过的目录来写：

```text
/opt/c-kafka-trace-producer-release
```

压缩包文件名默认是：

```text
c-kafka-trace-producer-arm64.tar.gz
```

## 1. 上传到目标机

可以用 `rz`、`scp`、SFTP 或其他文件传输方式。

上传完成后，假设你把压缩包放到了：

```text
/opt/c-kafka-trace-producer-release/c-kafka-trace-producer-arm64.tar.gz
```

先确认一下：

```sh
cd /opt/c-kafka-trace-producer-release
ls -lh c-kafka-trace-producer-arm64.tar.gz
```

## 2. 解压运行包

在 Buildroot 上，推荐使用下面这种兼容 BusyBox 的方式：

```sh
cd /opt/c-kafka-trace-producer-release
gzip -dc ./c-kafka-trace-producer-arm64.tar.gz | tar -xf - --strip-components=1
```

解压后检查目录：

```sh
cd /opt/c-kafka-trace-producer-release
ls -l
ls -l bin
ls -l config
ls -l deploy/device
```

正常应该能看到这些目录或文件：

- `bin`
- `config`
- `deploy`
- `lib`
- `README-runtime.txt`

### 2.1 如果 `tar` 不支持 `--strip-components=1`

有些极简 BusyBox 环境不支持这个参数，那就改用下面这套：

```sh
cd /opt/c-kafka-trace-producer-release
mkdir -p ./_unpack_tmp
cd ./_unpack_tmp
gzip -dc ../c-kafka-trace-producer-arm64.tar.gz | tar -xf -
cp -R ./arm64/. ..
cd ..
rm -rf ./_unpack_tmp
```

## 3. 给程序和脚本加执行权限

```sh
cd /opt/c-kafka-trace-producer-release
chmod +x bin/*
chmod +x deploy/device/*.sh
```

## 4. 修改配置文件

配置文件路径：

```text
/opt/c-kafka-trace-producer-release/config/application.properties
```

### 4.1 如果系统里有 `vi`

很多 Buildroot 没有 `vim`，但会带 `vi`。可以这样改：

```sh
cd /opt/c-kafka-trace-producer-release
vi ./config/application.properties
```

### 4.2 如果你只想快速改 broker 或 topic

也可以直接用 `sed`：

```sh
cd /opt/c-kafka-trace-producer-release
sed -i 's#^kafka.bootstrap.servers=.*#kafka.bootstrap.servers=192.168.37.69:9092#' ./config/application.properties
sed -i 's#^kafka.topic=.*#kafka.topic=test-topic#' ./config/application.properties
```

### 4.3 改完后确认一下

```sh
cd /opt/c-kafka-trace-producer-release
cat ./config/application.properties
```

最关键的配置通常是：

```properties
kafka.bootstrap.servers=192.168.37.69:9092
kafka.topic=test-topic
kafka.acks=all
kafka.enable.idempotence=true
kafka.compression.type=none
trace.max.frames=100000
trace.batch.size=1000
trace.send.interval.ms=100
```

## 5. 先验证 `librdkafka` 是否正常

启动任何业务程序前，建议先跑 probe：

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-rdkafka-probe
```

正常输出会类似：

```text
=== librdkafka Probe ===
librdkafka version: ...
builtin.features: ...
rd_kafka_new: ok
probe result: ok
```

### 5.1 关于这条 warning

如果你看到类似：

```text
No 'bootstrap.servers' configured: client will not be able to connect to Kafka cluster
```

这是正常的，不算失败。

原因是：

- `c-kafka-rdkafka-probe` 只是探测 `librdkafka` 能否加载、初始化
- 它本来就不会真的拿配置去连接 Kafka
- 所以这条只是提示，不影响 `probe result: ok`

## 6. 前台启动 producer

第一次验证时，强烈建议先前台运行，这样日志最直接。

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-trace-producer ./config/application.properties
```

正常启动后会看到类似：

```text
=== Native Kafka Trace Producer ===
=== Trace Config ===
[KafkaProducer] creating Kafka config...
[KafkaProducer] calling rd_kafka_new...
[KafkaProducer] rd_kafka_new returned
[KafkaProducer] initialized: ...
>>> sending ... frames to Kafka ...
[TraceSimulator] send loop started: ...
```

如果继续出现：

```text
[TraceSimulator] progress: 1.0%
[TraceSimulator] progress: 2.0%
```

这就说明 producer 已经在正常发数据了。

### 6.1 前台停止

前台运行时，停止最简单：

```text
Ctrl + C
```

程序会收到信号并退出。

## 7. 后台启动方式

当前项目里自带了后台脚本，但这些脚本是 `bash` 脚本，不是纯 `sh` 脚本。

所以分两种情况。

### 7.1 目标机有 `bash`

可以这样启动：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/start.sh
```

查看状态：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/status.sh
```

看日志：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/tail-log.sh
```

停止：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/stop.sh
```

日志和 PID 文件默认在：

- `/opt/c-kafka-trace-producer-release/logs/producer.log`
- `/opt/c-kafka-trace-producer-release/logs/producer.pid`

### 7.2 目标机没有 `bash`

如果 Buildroot 上没有 `bash`，那就不要直接跑 `deploy/device/*.sh`。

这时可以手工后台运行：

```sh
cd /opt/c-kafka-trace-producer-release
mkdir -p logs
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
nohup ./bin/c-kafka-trace-producer ./config/application.properties > ./logs/producer.log 2>&1 &
echo $! > ./logs/producer.pid
```

查看状态：

```sh
cd /opt/c-kafka-trace-producer-release
cat ./logs/producer.pid
kill -0 "$(cat ./logs/producer.pid)" && echo running
```

看日志：

```sh
cd /opt/c-kafka-trace-producer-release
tail -f ./logs/producer.log
```

停止：

```sh
cd /opt/c-kafka-trace-producer-release
kill "$(cat ./logs/producer.pid)"
rm -f ./logs/producer.pid
```

## 8. 配置修改后要不要重新编译

如果你改的是目标机上的：

```text
/opt/c-kafka-trace-producer-release/config/application.properties
```

那就：

- 不需要重新编译
- 不需要重新打包
- 只需要重启程序

前台方式重启：

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-trace-producer ./config/application.properties
```

后台脚本方式重启：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/stop.sh
bash ./deploy/device/start.sh
```

## 9. 常见问题

### 9.1 `-sh: vim: command not found`

这是正常现象，很多 Buildroot 没有 `vim`。

优先试：

```sh
vi ./config/application.properties
```

或者直接用：

```sh
sed -i 's#^kafka.topic=.*#kafka.topic=test-topic#' ./config/application.properties
```

### 9.2 `c-kafka-rdkafka-probe` 成功，但 producer 连不上 broker

这通常说明：

- 可执行文件没问题
- 动态库加载没问题
- ARM64 兼容没问题
- 问题在网络连通性或 broker 配置

继续检查：

- 目标机到 broker 是否能通
- broker 监听地址是否正确
- 防火墙或安全组是否放开 `9092`

### 9.3 解压后直接运行报找不到共享库

先确认当前 shell 里已经执行过：

```sh
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

## 10. 一套最短可执行流程

如果你只是想照着快速跑通一遍，可以直接按下面执行：

```sh
cd /opt/c-kafka-trace-producer-release
gzip -dc ./c-kafka-trace-producer-arm64.tar.gz | tar -xf - --strip-components=1
chmod +x bin/*
chmod +x deploy/device/*.sh
sed -i 's#^kafka.bootstrap.servers=.*#kafka.bootstrap.servers=192.168.37.69:9092#' ./config/application.properties
sed -i 's#^kafka.topic=.*#kafka.topic=test-topic#' ./config/application.properties
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-rdkafka-probe
./bin/c-kafka-trace-producer ./config/application.properties
```
