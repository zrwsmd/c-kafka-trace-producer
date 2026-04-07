# Buildroot ARM64 Kafka Producer 部署与启动文档

本文档现在只保留项目 `c-kafka-trace-producer` 的部署与启动内容。

对应项目目录：

```text
E:\wulianwnag\c-kafka-trace-producer
```

建议目标机部署目录：

```sh
/opt/c-kafka-trace-producer-release
```

## 1. 本地要准备的文件

最终交付包：

```text
E:\wulianwnag\c-kafka-trace-producer\c-kafka-trace-producer-arm64.tar.gz
```

如果这个包已经在，就可以直接上传，不需要重新构建。

## 2. 如果需要重新构建和打包

在 Windows 上执行：

```powershell
cd E:\wulianwnag\c-kafka-trace-producer

.\scripts\package-release-cross-windows.ps1 `
  -ToolchainRoot 'E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu' `
  -RdkafkaRoot 'E:\wulianwnag\testkafka\output'
```

完成后会重新生成：

```text
E:\wulianwnag\c-kafka-trace-producer\c-kafka-trace-producer-arm64.tar.gz
```

## 3. 上传到目标机

把下面这个包上传到目标机：

```text
c-kafka-trace-producer-arm64.tar.gz
```

建议上传到：

```sh
/opt/c-kafka-trace-producer-release/
```

目标机上最终应看到：

```sh
/opt/c-kafka-trace-producer-release/c-kafka-trace-producer-arm64.tar.gz
```

## 4. 目标机创建目录

先执行：

```sh
mkdir -p /opt/c-kafka-trace-producer-release
cd /opt/c-kafka-trace-producer-release
```

## 5. 解压发布包

确认压缩包存在：

```sh
ls -lh c-kafka-trace-producer-arm64.tar.gz
```

解压：

```sh
gzip -dc ./c-kafka-trace-producer-arm64.tar.gz | tar -xf - --strip-components=1
```

解压完成后建议检查：

```sh
ls
ls -l bin
ls -l lib
ls -l config
ls -l deploy/device
```

## 6. 增加执行权限

```sh
cd /opt/c-kafka-trace-producer-release
chmod +x bin/*
chmod +x deploy/device/*.sh
```

## 7. 配置文件

配置文件路径：

```sh
/opt/c-kafka-trace-producer-release/config/application.properties
```

你需要确认这里至少配置正确：
- Kafka broker 地址
- topic
- 业务发送参数

## 8. 先跑 probe

建议先验证运行环境和 librdkafka 是否正常：

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH=/opt/c-kafka-trace-producer-release/lib:${LD_LIBRARY_PATH}
./bin/c-kafka-rdkafka-probe
```

如果 probe 能正常运行，再启动主程序。

## 9. 前台启动 producer

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH=/opt/c-kafka-trace-producer-release/lib:${LD_LIBRARY_PATH}
./bin/c-kafka-trace-producer ./config/application.properties
```

前台停止：

```sh
Ctrl + C
```

## 10. 后台启动 producer

如果目标机支持 `bash`，直接执行：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/start.sh
```

查看状态：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/status.sh
```

查看日志：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/tail-log.sh
```

停止：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/stop.sh
```

## 11. 如果目标机没有 bash

有些 Buildroot 环境只有 `sh`，没有 `bash`。这时不要直接跑 `deploy/device/*.sh`，改用手工后台启动：

```sh
cd /opt/c-kafka-trace-producer-release
mkdir -p logs
export LD_LIBRARY_PATH=/opt/c-kafka-trace-producer-release/lib:${LD_LIBRARY_PATH}
nohup ./bin/c-kafka-trace-producer ./config/application.properties > ./logs/producer.log 2>&1 &
echo $! > ./logs/producer.pid
```

查看进程：

```sh
cat ./logs/producer.pid
ps | grep c-kafka-trace-producer
```

查看日志：

```sh
tail -f ./logs/producer.log
```

停止：

```sh
kill "$(cat ./logs/producer.pid)"
rm -f ./logs/producer.pid
```

## 12. 断电后目录丢失怎么办

如果目标机断电后目录没了：

```sh
/opt/c-kafka-trace-producer-release
```

那就重新按这份文档执行：

1. 创建目录
2. 重新上传 `c-kafka-trace-producer-arm64.tar.gz`
3. 重新解压
4. 重新启动

## 13. 最短操作版

```sh
mkdir -p /opt/c-kafka-trace-producer-release
cd /opt/c-kafka-trace-producer-release
gzip -dc ./c-kafka-trace-producer-arm64.tar.gz | tar -xf - --strip-components=1
chmod +x bin/*
chmod +x deploy/device/*.sh
bash ./deploy/device/start.sh
```

停止：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/stop.sh
```
