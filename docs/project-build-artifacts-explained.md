# 当前项目的编译产物说明

这份文档专门回答下面这些问题：

- 你之前对 `librdkafka` 的理解对不对
- 当前完整项目是怎么从源码编译出来的
- 编译后到底生成了哪些文件
- 每个文件分别是干什么的
- 为什么这次看到的是几个可执行文件，而不是一堆 `.o`、`.a`

## 1. 先说结论：你对 `librdkafka` 的理解是对的

你前面的理解是对的，可以这样概括：

1. 先用你自己的 ARM64 交叉编译器
   `gcc-arm-10.3-aarch64-none-linux-gnu`
   把 `librdkafka` 编成 ARM64 版本
2. 编译出来的结果，本质上就是“给 ARM64 程序用的一套 Kafka 开发库”
3. 这套开发库至少包括：
   - 头文件：告诉编译器有哪些函数、结构体、宏可以调用
   - 库文件：真正实现这些函数
4. 然后你的 `producer.c` 或当前完整项目，再去“包含这些头文件 + 链接这些库文件”，最终生成 ARM64 可执行文件

你可以把它理解成：

- `librdkafka` 是“ARM64 版 Kafka 通信能力包”
- 你的 `producer.c` 或当前项目，是“基于这个能力包写的业务程序”

## 2. 外部 `librdkafka` 在这次项目里扮演什么角色

你当前外部编好的 `librdkafka` 路径是：

```text
E:\wulianwnag\testkafka\output
```

里面最关键的是两类东西：

### 2.1 头文件

例如：

```text
E:\wulianwnag\testkafka\output\include\librdkafka\rdkafka.h
```

作用：

- 给编译器看接口声明
- 例如 `rd_kafka_new()`、`rd_kafka_producev()`、`rd_kafka_conf_set()` 这些函数，都是从这里“声明出来”的

### 2.2 库文件

例如：

```text
E:\wulianwnag\testkafka\output\lib\librdkafka.a
```

作用：

- 里面是真正的函数实现
- 链接时把 Kafka 协议能力接进你的最终程序里

当前项目的直调构建脚本，实际使用的是这个静态库：

```text
E:\wulianwnag\testkafka\output\lib\librdkafka.a
```

所以可以把这次完整项目理解成：

- 先有一个已经编好的 ARM64 `librdkafka`
- 再用它去构建你自己的 ARM64 业务程序

## 3. 当前项目自己的源码分成哪几层

当前项目不是只有一个 `producer.c`，而是把功能拆成了几层。

### 3.1 配置文件解析层

文件：

- [properties.h](E:/wulianwnag/c-kafka-trace-producer/include/properties.h)
- [properties.c](E:/wulianwnag/c-kafka-trace-producer/src/properties.c)

作用：

- 读取 `application.properties`
- 把 `key=value` 解析成内存里的键值表

你可以把它理解成：

- “把配置文件读进来”

### 3.2 业务配置层

文件：

- [trace_config.h](E:/wulianwnag/c-kafka-trace-producer/include/trace_config.h)
- [trace_config.c](E:/wulianwnag/c-kafka-trace-producer/src/trace_config.c)

作用：

- 定义程序需要哪些配置项
- 例如 broker、topic、batch size、max frames、acks、compression
- 从 `properties` 解析结果里取值
- 如果配置缺失，则使用默认值

你可以把它理解成：

- “把文本配置，变成程序真正要用的结构化配置”

### 3.3 Kafka Producer 封装层

文件：

- [kafka_trace_producer.h](E:/wulianwnag/c-kafka-trace-producer/include/kafka_trace_producer.h)
- [kafka_trace_producer.c](E:/wulianwnag/c-kafka-trace-producer/src/kafka_trace_producer.c)

作用：

- 直接调用 `librdkafka`
- 创建 Kafka producer
- 设置 Kafka 参数
- 发送消息
- 等待投递确认
- 统计成功数和失败数

你可以把它理解成：

- “把底层 `librdkafka` 再包一层，变成当前项目自己的发送模块”

### 3.4 数据模拟与 JSON 生成层

文件：

- [trace_simulator.h](E:/wulianwnag/c-kafka-trace-producer/include/trace_simulator.h)
- [trace_simulator.c](E:/wulianwnag/c-kafka-trace-producer/src/trace_simulator.c)

作用：

- 生成模拟轨迹数据
- 拼出一批一批 JSON payload
- 控制每批多少帧、总共发多少帧、发送间隔多长
- 打印进度日志和最终统计

你可以把它理解成：

- “真正准备要发送给 Kafka 的业务数据”

### 3.5 主程序入口

文件：

- [main.c](E:/wulianwnag/c-kafka-trace-producer/src/main.c)

作用：

- 启动程序
- 加载配置
- 初始化 producer
- 调用 simulator 开始循环发送

你可以把它理解成：

- “把前面几层串起来的总入口”

### 3.6 附带的两个辅助程序

文件：

- [simple_consumer.c](E:/wulianwnag/c-kafka-trace-producer/src/simple_consumer.c)
- [rdkafka_probe.c](E:/wulianwnag/c-kafka-trace-producer/src/rdkafka_probe.c)

作用：

- `simple_consumer.c`
  - 用来消费消息
  - 主要用于链路验证
  - 证明 broker 里确实有消息可消费
- `rdkafka_probe.c`
  - 用来探测 `librdkafka` 是否能正常加载、初始化
  - 不负责真正发送业务消息
  - 主要用于目标机兼容性验证

## 4. 当前项目是怎么编译出来的

你这次实际跑的是这个脚本：

- [build-arm64-cross-windows-direct.ps1](E:/wulianwnag/c-kafka-trace-producer/scripts/build-arm64-cross-windows-direct.ps1)

这个脚本的思路其实和你之前编 `producer.c` 非常像，只是它不再只编一个文件，而是编完整项目。

### 4.1 第一步：准备工具链和 sysroot

脚本会先拿到这些东西：

- 你的 ARM64 交叉编译器
- 对应 `sysroot`
- 本地 shim 库目录

作用：

- 告诉编译器目标平台是 ARM64 Linux
- 告诉编译器去哪里找标准头文件和系统库
- 补齐 `-lpthread`、`-ldl`、`-lm`、`-lrt` 这些开发期库名

### 4.2 第二步：接入外部 `librdkafka`

脚本会用：

- `-I E:\wulianwnag\testkafka\output\include`
- `E:\wulianwnag\testkafka\output\lib\librdkafka.a`

作用：

- `-I` 让编译器能找到 `rdkafka.h`
- `librdkafka.a` 让链接器把 Kafka 能力接进最终程序

### 4.3 第三步：编出 producer 主程序

脚本实际把这些源码一起编进一个最终程序：

- `src/main.c`
- `src/properties.c`
- `src/trace_config.c`
- `src/kafka_trace_producer.c`
- `src/trace_simulator.c`

最后链接成：

```text
build\arm64-cross-release\c-kafka-trace-producer
```

这一步可以理解成：

- “把配置解析 + Kafka 发送 + 数据模拟 + 主入口，一次性编成一个 ARM64 可执行文件”

### 4.4 第四步：编出 consumer

脚本把：

- `src/simple_consumer.c`

加上 `librdkafka.a` 链接成：

```text
build\arm64-cross-release\c-kafka-trace-consumer
```

### 4.5 第五步：编出 probe

脚本把：

- `src/rdkafka_probe.c`

加上 `librdkafka.a` 链接成：

```text
build\arm64-cross-release\c-kafka-rdkafka-probe
```

## 5. 为什么你现在没看到 `.o` 文件

这是因为你这次用的是“直调 GCC”脚本，不是先生成一堆中间目标文件、再单独链接的复杂构建流程。

它的做法更接近这种思路：

```text
gcc 源码1.c 源码2.c 源码3.c ... librdkafka.a -o 最终程序
```

所以当前你最直接看到的是最终成品：

- `c-kafka-trace-producer`
- `c-kafka-trace-consumer`
- `c-kafka-rdkafka-probe`

而不是一堆 `.o`

这不代表没有“编译阶段”，只是这些中间步骤没有单独落盘给你看。

## 6. 当前项目最终编出了哪些文件

### 6.1 构建目录下的核心产物

目录：

```text
E:\wulianwnag\c-kafka-trace-producer\build\arm64-cross-release
```

里面最重要的是这三个文件：

#### `c-kafka-trace-producer`

路径：

- [c-kafka-trace-producer](E:/wulianwnag/c-kafka-trace-producer/build/arm64-cross-release/c-kafka-trace-producer)

作用：

- 当前项目的主程序
- 负责读配置、造数据、发 Kafka、打印进度

这就是你最终部署到目标机后真正跑的主程序。

#### `c-kafka-trace-consumer`

路径：

- [c-kafka-trace-consumer](E:/wulianwnag/c-kafka-trace-producer/build/arm64-cross-release/c-kafka-trace-consumer)

作用：

- 简单消费者
- 用来验证 broker 里能不能收到和消费消息

这不是主业务程序，但很适合做链路排查。

#### `c-kafka-rdkafka-probe`

路径：

- [c-kafka-rdkafka-probe](E:/wulianwnag/c-kafka-trace-producer/build/arm64-cross-release/c-kafka-rdkafka-probe)

作用：

- 验证 `librdkafka` 在目标机上能不能正常工作
- 先于 producer 做兼容性检测

这就是你之前在目标机上跑通并看到 `probe result: ok` 的那个程序。

### 6.2 发布目录下的运行包

目录：

```text
E:\wulianwnag\c-kafka-trace-producer\release\arm64
```

这个目录不是“新的程序”，而是“适合拷到目标机上运行的一整套发布包”。

它里面包含：

#### `bin/`

路径：

- [release/arm64/bin](E:/wulianwnag/c-kafka-trace-producer/release/arm64/bin)

作用：

- 放最终可执行文件
- 里面是上面那三个程序的部署版副本

#### `config/application.properties`

路径：

- [application.properties](E:/wulianwnag/c-kafka-trace-producer/release/arm64/config/application.properties)

作用：

- 运行时配置文件
- 改 broker、topic、发送参数，通常改这个文件就行

#### `deploy/device/*.sh`

路径：

- [start.sh](E:/wulianwnag/c-kafka-trace-producer/release/arm64/deploy/device/start.sh)
- [stop.sh](E:/wulianwnag/c-kafka-trace-producer/release/arm64/deploy/device/stop.sh)
- [status.sh](E:/wulianwnag/c-kafka-trace-producer/release/arm64/deploy/device/status.sh)
- [tail-log.sh](E:/wulianwnag/c-kafka-trace-producer/release/arm64/deploy/device/tail-log.sh)
- [run-foreground.sh](E:/wulianwnag/c-kafka-trace-producer/release/arm64/deploy/device/run-foreground.sh)
- [install-service.sh](E:/wulianwnag/c-kafka-trace-producer/release/arm64/deploy/device/install-service.sh)

作用：

- 帮你在设备上前台运行、后台运行、看日志、停进程、装成 systemd 服务

#### `lib/`

路径：

- [release/arm64/lib](E:/wulianwnag/c-kafka-trace-producer/release/arm64/lib)

作用：

- 放运行期需要一起带到目标机的共享库

需要特别说明：

- 你这次构建时，`librdkafka` 用的是 `librdkafka.a` 静态库
- 也就是说，`librdkafka` 的代码已经被链接进最终程序里了
- 所以发布包里的 `lib/` 目录可能是空的，这并不代表失败

换句话说：

- `lib/` 为空，不代表程序缺少 `librdkafka`
- 很可能只是因为 `librdkafka` 已经静态进了二进制里
- 而 glibc 这类系统基础库，打包脚本本来也不会强行复制进去

#### `README-runtime.txt`

路径：

- [README-runtime.txt](E:/wulianwnag/c-kafka-trace-producer/release/arm64/README-runtime.txt)

作用：

- 简单说明这个运行包怎么用

### 6.3 最终压缩包

路径：

- [c-kafka-trace-producer-arm64.tar.gz](E:/wulianwnag/c-kafka-trace-producer/c-kafka-trace-producer-arm64.tar.gz)

作用：

- 这是最适合传到 Buildroot 目标机的交付文件
- 你上传、解压、运行，主要就是围绕这个包

## 7. 这些文件之间的关系，用一句人话怎么说

可以把整条链路理解成下面这样：

1. 你先用自己的交叉编译器，做出 ARM64 版 `librdkafka`
2. 当前项目再把自己的 C 源码，和这个 ARM64 `librdkafka` 链接起来
3. 于是得到三个 ARM64 可执行文件：
   - 主 producer
   - simple consumer
   - probe
4. 再把这些可执行文件 + 配置文件 + 运行脚本打成发布目录
5. 再把发布目录压成 `c-kafka-trace-producer-arm64.tar.gz`
6. 最后把这个压缩包传到 Buildroot 目标机运行

## 8. 你最值得记住的几个点

- `librdkafka` 不是你的业务程序，它是你的 Kafka 基础库
- 当前项目主程序是 `c-kafka-trace-producer`
- `c-kafka-trace-consumer` 是链路验证工具
- `c-kafka-rdkafka-probe` 是兼容性验证工具
- `application.properties` 是最常改的运行配置
- `c-kafka-trace-producer-arm64.tar.gz` 是最终交付给目标机的包
- 当前项目现在已经是纯 C，不再依赖 ARM64 `libstdc++`
- 当前直调构建方式没有单独保留 `.o` 文件，这属于正常现象

## 9. 如果要看“目标机怎么上传、解压、启动”

另外一份文档在这里：

- [buildroot-target-deploy-and-run.md](E:/wulianwnag/c-kafka-trace-producer/docs/buildroot-target-deploy-and-run.md)
