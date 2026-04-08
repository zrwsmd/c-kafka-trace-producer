# Kafka 启动顺序说明

本文档说明 `deploy/broker` 目录下 Kafka 相关脚本的推荐执行顺序。

适用目录：

```bash
cd /opt/c-kafka-trace-producer/deploy/broker
```

如果脚本还没有执行权限，可以先执行：

```bash
chmod +x *.sh
```

## 一、标准启动顺序

推荐按下面顺序执行：

### 1. 安装 Kafka

```bash
./install-kafka.sh
```

作用：

- 安装 Java 运行环境
- 下载并解压 Kafka 到 `/opt/kafka`
- 创建数据目录 `/data/kafka`
- 创建 `kafka` 用户并设置目录权限

### 2. 生成 Kafka 配置

```bash
./configure-kafka.sh
```

作用：

- 生成 `zookeeper.properties`
- 生成 `server.properties`
- 自动探测公网 IP，并写入 `advertised.listeners`

说明：

- 这一步必须在安装完成之后执行
- 如果公网 IP 自动探测不准确，需要手动修改 `/opt/kafka/config/server.properties`

### 3. 可选：降低内存占用

```bash
./fix-memory.sh
```

作用：

- 将 Kafka 堆内存调整为 `256M`
- 将 Zookeeper 堆内存调整为 `128M`

说明：

- 这一步不是必须
- 如果机器内存较小，建议在首次启动前执行

### 4. 启动 Kafka

```bash
./start-kafka.sh
```

作用：

- 先启动 Zookeeper
- 再启动 Kafka Broker
- 自动创建 `trace-data` topic

### 5. 验证 Kafka 是否正常

```bash
./test-kafka.sh
```

作用：

- 检查 Kafka 进程是否正常运行
- 发送一条测试消息
- 消费一条消息，验证收发链路是否正常

## 二、推荐执行顺序汇总

标准推荐顺序如下：

```bash
cd /opt/c-kafka-trace-producer/deploy/broker
chmod +x *.sh
./install-kafka.sh
./configure-kafka.sh
./fix-memory.sh        # 可选，内存小时建议执行
./start-kafka.sh
./test-kafka.sh
```

如果不需要调整内存，可以省略 `./fix-memory.sh`。

## 三、setup-systemd.sh 的位置

`setup-systemd.sh` 不是 Kafka 首次启动的必需步骤，它是可选的服务化配置脚本。

```bash
./setup-systemd.sh
```

作用：

- 生成 `zookeeper.service`
- 生成 `kafka.service`
- 执行 `systemctl daemon-reload`
- 执行 `systemctl enable zookeeper kafka`

推荐使用方式：

1. 先按标准顺序完成安装和配置
2. 如果需要开机自启，再执行 `./setup-systemd.sh`
3. 之后可以使用 `systemctl` 管理 Kafka

例如：

```bash
sudo systemctl start kafka
sudo systemctl status kafka
```

## 四、停止 Kafka

如需停止服务，可以执行：

```bash
./stop-kafka.sh
```

## 五、结论

`install-kafka.sh` 之后下一步应执行：

```bash
./configure-kafka.sh
```

完整推荐顺序是：

```bash
./install-kafka.sh
./configure-kafka.sh
./fix-memory.sh   # 可选
./start-kafka.sh
./test-kafka.sh
```
