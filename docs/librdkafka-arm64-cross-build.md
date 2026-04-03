# Buildroot ARM64 环境下交叉编译 librdkafka 与最小 C Producer 指南

本文档记录一套已经验证通过的流程，用于在 Windows 主机上使用指定的 ARM64 交叉工具链编译 `librdkafka`，并进一步编译一个最小 `producer.c` 做目标机验证。

已验证过的工具链路径：

```text
E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu
```

本文档默认使用这些目录：

- 工作目录：`E:\wulianwnag\testkafka`
- `librdkafka` 源码目录：`E:\wulianwnag\testkafka\librdkafka`
- 安装前缀：`E:\wulianwnag\testkafka\output`
- 本地 shim 库目录：`E:\wulianwnag\testkafka\toolchain-shim`
- 最小 demo 源文件：`E:\wulianwnag\testkafka\producer.c`
- 最终 ARM64 可执行文件：`E:\wulianwnag\testkafka\producer`

Kafka broker 示例地址：

```text
47.129.128.147:9092
```

## 1. 从 git clone 开始准备源码

先在 PowerShell 里执行：

```powershell
New-Item -ItemType Directory -Force E:\wulianwnag\testkafka | Out-Null
Set-Location E:\wulianwnag\testkafka
git clone https://github.com/confluentinc/librdkafka.git
Set-Location .\librdkafka
git submodule update --init --recursive
```

完成后源码目录应为：

```text
E:\wulianwnag\testkafka\librdkafka
```

## 2. 打开 Git Bash

后续 `configure` 和 `make` 建议在 Git Bash 里执行。

如果本机已经安装 Git for Windows，可以在 PowerShell 里启动：

```powershell
& 'D:\Git\bin\bash.exe'
```

进入源码目录：

```bash
cd /e/wulianwnag/testkafka/librdkafka
```

注意 Windows 路径 `E:\wulianwnag\testkafka` 在 Git Bash 下写成 `/e/wulianwnag/testkafka`。

## 3. 设置交叉编译环境变量

在 Git Bash 中执行：

```bash
export TOOLROOT=/e/VSCode-win32-x64-1.85.1/data/extensions/undefined_publisher.devuni-ide-vscode-0.0.1/tool/iec-runtime-gen-run/gcc-arm-10.3-aarch64-none-linux-gnu
export SYSROOT=$TOOLROOT/aarch64-none-linux-gnu/libc
export PREFIX=/e/wulianwnag/testkafka/output
export DEVLIB=/e/wulianwnag/testkafka/toolchain-shim

export CC=$TOOLROOT/bin/gcc.exe
export AR=$TOOLROOT/bin/ar.exe
export RANLIB=$TOOLROOT/bin/ranlib.exe
export STRIP=$TOOLROOT/bin/strip.exe
```

可以先确认一下：

```bash
$CC -dumpmachine
$CC --print-sysroot
```

正常应看到：

- target 类似 `aarch64-none-linux-gnu`
- sysroot 指向 `.../aarch64-none-linux-gnu/libc`

## 4. 准备本地 shim 库目录

### 4.1 为什么需要 shim 目录

你当前这套工具链的 sysroot 里有运行时库文件，但缺少某些开发期链接名。

例如：

- 链接器看到 `-lpthread` 时，会优先找 `libpthread.so` 或 `libpthread.a`
- 但 sysroot 里实际存在的往往是 `libpthread.so.0`

这就会导致：

- `configure` 探测线程库失败
- 后续 `gcc` 链接时报 `cannot find -lpthread`

`libdl`、`libm`、`librt` 也是同样的情况。

所以这里单独放一个 `toolchain-shim` 目录，用来给链接器补齐这些常见名字。它不是额外下载的库，也不是替换原始库，只是把 sysroot 里已经存在的目标库复制出来，命名成更适合开发期链接的名字。

### 4.2 这些 shim 文件从哪来

当前验证通过的一组来源如下：

- `libpthread.so` 来自 `$SYSROOT/lib64/libpthread.so.0`
- `libdl.so` 来自 `$SYSROOT/lib64/libdl.so.2`
- `libm.so` 来自 `$SYSROOT/lib64/libm.so.6`
- `librt.so` 来自 `$SYSROOT/usr/lib64/librt.so`

### 4.3 生成 shim 目录

在 Git Bash 中执行：

```bash
mkdir -p $DEVLIB

cp -f $SYSROOT/lib64/libpthread.so.0 $DEVLIB/libpthread.so
cp -f $SYSROOT/lib64/libdl.so.2 $DEVLIB/libdl.so
cp -f $SYSROOT/lib64/libm.so.6 $DEVLIB/libm.so
cp -f $SYSROOT/usr/lib64/librt.so $DEVLIB/librt.so
```

然后补充编译和链接参数：

```bash
export CPPFLAGS="--sysroot=$SYSROOT"
export CFLAGS="--sysroot=$SYSROOT -O2 -fPIC"
export LDFLAGS="--sysroot=$SYSROOT -L$DEVLIB -L$SYSROOT/usr/lib64 -Wl,-rpath-link,$SYSROOT/lib64 -Wl,-rpath-link,$SYSROOT/usr/lib64"
export LIBRARY_PATH="$DEVLIB:$SYSROOT/usr/lib64:$SYSROOT/lib64"
```

## 5. 配置 librdkafka

为了先完成最小可用的 C 版本验证，这里关闭了 `ssl`、`sasl`、`zstd`、`lz4-ext`、`zlib` 等额外依赖。

在 Git Bash 中执行：

```bash
cd /e/wulianwnag/testkafka/librdkafka

./configure \
  --build=x86_64-w64-mingw32 \
  --host=aarch64-none-linux-gnu \
  --prefix=$PREFIX \
  --enable-static \
  --disable-shared \
  --disable-ssl \
  --disable-gssapi \
  --disable-sasl \
  --disable-zstd \
  --disable-lz4-ext \
  --disable-zlib
```

如果配置成功，根目录会生成 `config.h`。

## 6. 编译并安装 librdkafka

当前目标只是给最小 C demo 和当前项目提供 ARM64 `librdkafka`，所以直接编译 `src` 即可：

```bash
cd /e/wulianwnag/testkafka/librdkafka
make -C src clean
make -C src -j4
make -C src install
```

安装完成后，下面这些路径应存在：

```bash
ls -l /e/wulianwnag/testkafka/output/lib
ls -l /e/wulianwnag/testkafka/output/include/librdkafka
```

常见结果包括：

- `librdkafka.a`
- `librdkafka.so`
- `rdkafka.h`

## 7. 编写最小 producer.c

把下面内容保存为 `E:\wulianwnag\testkafka\producer.c`：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <inttypes.h>
#include <librdkafka/rdkafka.h>

static volatile sig_atomic_t run = 1;

static void stop_sig(int sig) {
    (void)sig;
    run = 0;
}

static void dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque) {
    (void)rk;
    (void)opaque;

    if (rkmessage->err) {
        fprintf(stderr, "delivery failed: %s\n", rd_kafka_err2str(rkmessage->err));
    } else {
        printf("delivered to topic=%s partition=%d offset=%" PRId64 "\n",
               rd_kafka_topic_name(rkmessage->rkt),
               rkmessage->partition,
               rkmessage->offset);
    }
}

int main(int argc, char **argv) {
    char errstr[512];
    const char *brokers = argc > 1 ? argv[1] : "47.129.128.147:9092";
    const char *topic   = argc > 2 ? argv[2] : "test-topic";
    const char *msg     = argc > 3 ? argv[3] : "hello from buildroot arm64";

    signal(SIGINT, stop_sig);
    signal(SIGTERM, stop_sig);

    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        fprintf(stderr, "set bootstrap.servers failed: %s\n", errstr);
        return 1;
    }

    if (rd_kafka_conf_set(conf, "client.id", "arm64-producer-demo", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        fprintf(stderr, "set client.id failed: %s\n", errstr);
        return 1;
    }

    rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk) {
        fprintf(stderr, "rd_kafka_new failed: %s\n", errstr);
        return 1;
    }

    if (rd_kafka_producev(
            rk,
            RD_KAFKA_V_TOPIC(topic),
            RD_KAFKA_V_VALUE((void *)msg, (size_t)strlen(msg)),
            RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
            RD_KAFKA_V_END) != RD_KAFKA_RESP_ERR_NO_ERROR) {
        fprintf(stderr, "produce failed: %s\n", rd_kafka_err2str(rd_kafka_last_error()));
        rd_kafka_destroy(rk);
        return 1;
    }

    while (run && rd_kafka_outq_len(rk) > 0) {
        rd_kafka_poll(rk, 100);
    }

    if (rd_kafka_flush(rk, 10000) != RD_KAFKA_RESP_ERR_NO_ERROR) {
        fprintf(stderr, "flush timeout\n");
        rd_kafka_destroy(rk);
        return 1;
    }

    rd_kafka_destroy(rk);
    return 0;
}
```

## 8. 交叉编译最小 producer.c

继续在 Git Bash 中执行：

```bash
$CC --sysroot=$SYSROOT -O2 \
  -I$PREFIX/include \
  -L$PREFIX/lib \
  -L$DEVLIB \
  -Wl,-rpath-link,$SYSROOT/lib64 \
  -Wl,-rpath-link,$SYSROOT/usr/lib64 \
  -o /e/wulianwnag/testkafka/producer \
  /e/wulianwnag/testkafka/producer.c \
  $PREFIX/lib/librdkafka.a \
  -lpthread -ldl -lm -lrt
```

如果遇到 `__atomic` 相关未定义符号，可以在最后补一个 `-latomic`。

编译完成后可以裁剪符号：

```bash
$STRIP /e/wulianwnag/testkafka/producer
```

## 9. 验证产物是否为 ARM64 Linux ELF

```bash
$TOOLROOT/bin/readelf.exe -h /e/wulianwnag/testkafka/producer | grep Machine
```

正常应看到：

```text
Machine:                           AArch64
```

这说明它是 ARM64 Linux 可执行文件，可以放到 Buildroot 设备上运行。

## 10. 上传到 Buildroot 目标机运行

把 `producer` 文件拷到目标机后执行：

```sh
chmod +x ./producer
./producer 47.129.128.147:9092 test-topic "hello from arm64"
```

如果发送成功，通常会看到：

```text
delivered to topic=test-topic partition=0 offset=123
```

如果出现：

- `Connect to 47.129.128.147:9092 failed`
- `No route to host`
- `1/1 brokers are down`
- `flush timeout`

那说明程序本身已经在目标机运行起来了，问题在网络链路，不在编译兼容性。

## 11. 和当前项目的关系

这份外部 ARM64 `librdkafka` 编好后，当前项目不需要把 `librdkafka` 源码拷进仓库，只需要在交叉编译时传入：

```text
RDKAFKA_ROOT=E:\wulianwnag\testkafka\output
```

当前项目已经迁成纯 C，后续可以直接这样构建：

```powershell
cd E:\wulianwnag\c-kafka-trace-producer

$ToolchainRoot = 'E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu'
$RdkafkaRoot = 'E:\wulianwnag\testkafka\output'

.\scripts\build-arm64-cross-windows-direct.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot
```

这就是你后续维护当前项目时最推荐保留的依赖方式。
