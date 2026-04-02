# Buildroot ARM64 环境下交叉编译 librdkafka 与 producer.c 指南

本文档记录一套已经验证可执行的流程，用于在 Windows 主机上使用指定的 ARM64 交叉编译器：

`E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu`

交叉编译 `librdkafka`，并进一步编译一个 `producer.c` 示例程序，最终把可执行文件拷贝到 Buildroot ARM64 设备上运行。

本文档假设：

- Kafka broker 地址为 `47.129.128.147:9092`
- 工作目录使用 `E:\wulianwnag\testkafka`
- 当前机器已安装 Git for Windows
- 交叉编译器目录已经存在，不需要额外下载安装

## 1. 目录约定

本文档统一使用下面几个目录：

- `E:\wulianwnag\testkafka`
- `E:\wulianwnag\testkafka\librdkafka`
- `E:\wulianwnag\testkafka\output`
- `E:\wulianwnag\testkafka\toolchain-shim`
- `E:\wulianwnag\testkafka\producer.c`
- `E:\wulianwnag\testkafka\producer`

其中：

- `librdkafka` 是源码目录
- `output` 是安装目录
- `toolchain-shim` 是为当前工具链补充开发期链接库的目录
- `producer` 是最终生成的 ARM64 Linux 可执行文件

## 2. 准备源码目录

先在 PowerShell 中创建工作目录并下载源码：

```powershell
New-Item -ItemType Directory -Force E:\wulianwnag\testkafka | Out-Null
Set-Location E:\wulianwnag\testkafka
git clone https://github.com/confluentinc/librdkafka.git
Set-Location .\librdkafka
git submodule update --init --recursive
```

完成后，源码目录应为：

```text
E:\wulianwnag\testkafka\librdkafka
```

## 3. 打开 Git Bash

后续 `configure` 和 `make` 建议在 Git Bash 中执行，不要在 PowerShell 里直接跑 `./configure`。

可以在 PowerShell 中直接启动 Git Bash：

```powershell
& 'D:\Git\bin\bash.exe'
```

启动后切换到源码目录：

```bash
cd /e/wulianwnag/testkafka/librdkafka
```

注意：

- Windows 下的 `E:\wulianwnag\testkafka` 在 Git Bash 中写作 `/e/wulianwnag/testkafka`

## 4. 设置交叉编译环境变量

在 Git Bash 中执行：

```bash
export TOOLROOT=/e/VSCode-win32-x64-1.85.1/data/extensions/undefined_publisher.devuni-ide-vscode-0.0.1/tool/iec-runtime-gen-run/gcc-arm-10.3-aarch64-none-linux-gnu
export SYSROOT=$TOOLROOT/aarch64-none-linux-gnu/libc
export PREFIX=/e/wulianwnag/testkafka/output
export DEVLIB=/e/wulianwnag/testkafka/toolchain-shim

export CC=$TOOLROOT/bin/gcc.exe
export CXX=$TOOLROOT/bin/g++.exe
export AR=$TOOLROOT/bin/ar.exe
export RANLIB=$TOOLROOT/bin/ranlib.exe
export STRIP=$TOOLROOT/bin/strip.exe
```

可以先做一次简单确认：

```bash
$CC -dumpmachine
$CC --print-sysroot
```

正常应看到：

- target 类似 `aarch64-none-linux-gnu`
- sysroot 指向 `.../aarch64-none-linux-gnu/libc`

## 5. 补充本地 shim 库目录

当前工具链的 sysroot 里包含运行时库，但缺少部分开发期链接文件。为了让 `configure` 和后续链接顺利通过，需要单独准备一个本地 shim 目录。

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

## 6. 配置 librdkafka

为了先完成最小可用的 C 版 producer 验证，这里关闭 SSL、SASL、zstd、zlib 等额外依赖。

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

可以检查一下：

```bash
ls -l config.h
```

## 7. 编译并安装 C 版 librdkafka

当前目标只需要 `producer.c`，因此直接编译并安装 `src` 下的 C 库即可：

```bash
cd /e/wulianwnag/testkafka/librdkafka
make -C src clean
make -C src -j4
make -C src install
```

安装完成后，目标目录应包含：

```bash
ls -l /e/wulianwnag/testkafka/output/lib
ls -l /e/wulianwnag/testkafka/output/include/librdkafka
```

正常会看到类似文件：

- `librdkafka.a`
- `librdkafka.so`
- `librdkafka.so.1`
- `rdkafka.h`
- `rdkafka_mock.h`

## 8. 编写 producer.c

在 `E:\wulianwnag\testkafka\producer.c` 写入以下内容：

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

## 9. 交叉编译 producer.c

继续在 Git Bash 中执行：

```bash
export TOOLROOT=/e/VSCode-win32-x64-1.85.1/data/extensions/undefined_publisher.devuni-ide-vscode-0.0.1/tool/iec-runtime-gen-run/gcc-arm-10.3-aarch64-none-linux-gnu
export SYSROOT=$TOOLROOT/aarch64-none-linux-gnu/libc
export DEVLIB=/e/wulianwnag/testkafka/toolchain-shim
export PREFIX=/e/wulianwnag/testkafka/output
export CC=$TOOLROOT/bin/gcc.exe

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

如果遇到 `__atomic` 相关未定义符号，可以在最后补一个 `-latomic`：

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
  -lpthread -ldl -lm -lrt -latomic
```

编译成功后可以裁剪符号：

```bash
$STRIP /e/wulianwnag/testkafka/producer
```

## 10. 验证产物是否为 ARM64 Linux 可执行文件

使用交叉工具链自带的 `readelf` 检查：

```bash
$TOOLROOT/bin/readelf.exe -h /e/wulianwnag/testkafka/producer | grep Machine
```

正常应看到：

```text
Machine:                           AArch64
```

这说明产物是 ARM64 Linux ELF，可放到 Buildroot 设备上执行。

## 11. 拷贝到 Buildroot 目标机运行

将 `producer` 文件拷贝到目标机后执行：

```sh
chmod +x ./producer
./producer 47.129.128.147:9092 test-topic "hello from arm64"
```

如果发送成功，通常会看到类似输出：

```text
delivered to topic=test-topic partition=0 offset=123
```

如果出现类似下面的信息：

```text
Connect to 47.129.128.147:9092 failed: No route to host
1/1 brokers are down
flush timeout
```

则说明程序本身已经运行起来了，但目标设备到 Kafka broker 的网络不通，需要继续检查：

- 目标机 IP 地址是否正确
- 默认路由是否存在
- 目标机是否能访问外网
- broker 侧防火墙或云安全组是否已放行 `9092`
- broker 是否监听外网可访问地址

## 12. 推荐的完整命令顺序

如果希望按顺序从头执行一遍，可以参考下面这组命令。

### 12.1 PowerShell

```powershell
New-Item -ItemType Directory -Force E:\wulianwnag\testkafka | Out-Null
Set-Location E:\wulianwnag\testkafka
git clone https://github.com/confluentinc/librdkafka.git
Set-Location .\librdkafka
git submodule update --init --recursive
& 'D:\Git\bin\bash.exe'
```

### 12.2 Git Bash

```bash
cd /e/wulianwnag/testkafka/librdkafka

export TOOLROOT=/e/VSCode-win32-x64-1.85.1/data/extensions/undefined_publisher.devuni-ide-vscode-0.0.1/tool/iec-runtime-gen-run/gcc-arm-10.3-aarch64-none-linux-gnu
export SYSROOT=$TOOLROOT/aarch64-none-linux-gnu/libc
export PREFIX=/e/wulianwnag/testkafka/output
export DEVLIB=/e/wulianwnag/testkafka/toolchain-shim
export CC=$TOOLROOT/bin/gcc.exe
export CXX=$TOOLROOT/bin/g++.exe
export AR=$TOOLROOT/bin/ar.exe
export RANLIB=$TOOLROOT/bin/ranlib.exe
export STRIP=$TOOLROOT/bin/strip.exe

mkdir -p $DEVLIB
cp -f $SYSROOT/lib64/libpthread.so.0 $DEVLIB/libpthread.so
cp -f $SYSROOT/lib64/libdl.so.2 $DEVLIB/libdl.so
cp -f $SYSROOT/lib64/libm.so.6 $DEVLIB/libm.so
cp -f $SYSROOT/usr/lib64/librt.so $DEVLIB/librt.so

export CPPFLAGS="--sysroot=$SYSROOT"
export CFLAGS="--sysroot=$SYSROOT -O2 -fPIC"
export LDFLAGS="--sysroot=$SYSROOT -L$DEVLIB -L$SYSROOT/usr/lib64 -Wl,-rpath-link,$SYSROOT/lib64 -Wl,-rpath-link,$SYSROOT/usr/lib64"
export LIBRARY_PATH="$DEVLIB:$SYSROOT/usr/lib64:$SYSROOT/lib64"

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

make -C src clean
make -C src -j4
make -C src install
```

### 12.3 编译 producer

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

## 13. 结果说明

完成以上步骤后，你会得到两部分结果：

- `E:\wulianwnag\testkafka\output` 下的 ARM64 版 `librdkafka`
- `E:\wulianwnag\testkafka\producer` 这个 ARM64 Linux 可执行文件

将 `producer` 拷到 Buildroot ARM64 目标机后即可实际验证是否能向你的 Kafka broker 发送消息。
