# ARM64 / Buildroot aarch64 构建与部署指南

本文档适用于这类目标设备：

- `uname -m` 为 `aarch64`
- 系统是 Buildroot，例如 `Buildroot 2018.02-rc3`
- 目标机主要负责“运行成品”，不适合承担完整编译工作

这份文档同时覆盖两条构建路径：

1. 在 ARM64 Linux 构建机上原生编译
2. 在 Windows x86_64 主机上使用 `aarch64` 交叉工具链交叉编译

如果你的目标机就是你当前这台 Buildroot 设备，最重要的结论先放前面：

- 目标机不要执行 `git clone`、`apt install`
- 目标机只负责接收 `tar.gz`、解压、改配置、运行
- 如果在 `x86_64` 主机上直接运行仓库里的 `build-arm64-native.sh`，产物仍然会是 `x86_64`，不能在目标机上运行
- 交叉编译时必须同时具备：
  - `aarch64` 交叉编译工具链
  - 对应的 `sysroot`
  - ARM64 版本的 `librdkafka`

## 1. 目录约定

本文档默认：

- 源码目录：`E:\wulianwnag\c-kafka-trace-producer`
- 目标机部署目录：`/opt/c-kafka-trace-producer-release`

如果你的目录不同，把命令里的路径替换掉即可。

## 2. 先理解 4 个目录

无论你走哪条构建路径，都建议把下面 4 个概念分清：

- 源码目录：工程本身，例如 `E:\wulianwnag\c-kafka-trace-producer`
- 构建目录：例如 `build/arm64-release` 或 `build/arm64-cross-release`
- 运行包目录：例如 `release/arm64`
- 压缩包：例如 `c-kafka-trace-producer-arm64.tar.gz`

真正应该部署到目标机的是：

- `release/arm64`
- 或它压出来的 `c-kafka-trace-producer-arm64.tar.gz`

不是整个源码目录。

## 3. 目标机职责

目标机只负责：

- 接收 `c-kafka-trace-producer-arm64.tar.gz`
- 解压到 `/opt/c-kafka-trace-producer-release`
- 配置 `config/application.properties`
- 启动 `bin/c-kafka-trace-producer`

## 4. 路径 A：ARM64 Linux 原生编译

这条路径最简单，前提是你有一台真正的 ARM64 Linux 构建机。

### 4.1 构建机准备

```bash
cd /opt
git clone https://github.com/zrwsmd/c-kafka-trace-producer.git
cd /opt/c-kafka-trace-producer
chmod +x scripts/*.sh
chmod +x deploy/device/*.sh
chmod +x deploy/broker/*.sh
```

如果是 Ubuntu / Debian ARM64：

```bash
cd /opt/c-kafka-trace-producer
./scripts/install-build-deps-ubuntu.sh
```

### 4.2 编译

如果构建机已经有 ARM64 版本的 `librdkafka`：

```bash
cd /opt/c-kafka-trace-producer
./scripts/build-arm64-native.sh
```

如果你不想依赖系统 `librdkafka`：

```bash
cd /opt/c-kafka-trace-producer
./scripts/build-rdkafka-local.sh
./scripts/build-arm64-local-rdkafka.sh
```

### 4.3 打包

系统 `librdkafka` 方案：

```bash
cd /opt/c-kafka-trace-producer
./scripts/package-release.sh "$PWD/build/arm64-release" "$PWD/release/arm64"
tar -czf /opt/c-kafka-trace-producer-arm64.tar.gz -C release arm64
```

本地 `librdkafka` 方案：

```bash
cd /opt/c-kafka-trace-producer
RDKAFKA_ROOT="$PWD/third_party/rdkafka/install" \
./scripts/package-release.sh "$PWD/build/arm64-release" "$PWD/release/arm64"
tar -czf /opt/c-kafka-trace-producer-arm64.tar.gz -C release arm64
```

### 4.4 验证产物架构

```bash
file /opt/c-kafka-trace-producer/build/arm64-release/c-kafka-trace-producer
file /opt/c-kafka-trace-producer/release/arm64/bin/c-kafka-trace-producer
```

正常应看到：

```text
ELF 64-bit LSB pie executable, ARM aarch64
```

如果看到的是 `x86-64`，说明你没有在 ARM64 环境里编译成功。

## 5. 路径 B：Windows x86_64 + AArch64 交叉编译

这条路径适合你现在这种情况：

- 主机是 Windows x86_64
- 你手上有一套 `aarch64-none-linux-gnu` 工具链
- 你希望在 Windows 上直接生成 ARM64 Linux 产物

仓库现在已经新增了这几个文件：

- `cmake/toolchains/aarch64-none-linux-gnu.cmake`
- `scripts/validate-arm64-toolchain-windows.ps1`
- `scripts/build-arm64-cross-windows.ps1`
- `scripts/package-release-cross-windows.ps1`

### 5.1 这条路径的额外前提

Windows 交叉编译时，除了源码外，还需要：

1. `aarch64` 交叉编译工具链
2. 对应 `sysroot`
3. ARM64 版本的 `librdkafka` 安装前缀
4. `cmake`
5. `ninja`

其中第 3 点非常关键。

当前工程在交叉编译时不会再走主机上的 `pkg-config` 自动找库，而是要求你显式提供：

```text
RDKAFKA_ROOT=<ARM64 librdkafka install prefix>
```

也就是说，`RDKAFKA_ROOT` 下面至少要有：

- `include/librdkafka/rdkafka.h`
- `lib/librdkafka.so`
  或
- `lib/librdkafka.a`

### 5.2 先自检工具链

先不要直接编整个项目，先用新增脚本检查工具链是不是完整 C++ SDK。

PowerShell 中执行：

```powershell
cd E:\wulianwnag\c-kafka-trace-producer

$ToolchainRoot = 'E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu'

.\scripts\validate-arm64-toolchain-windows.ps1 -ToolchainRoot $ToolchainRoot
```

如果工具链没问题，脚本会输出：

```text
Toolchain probe succeeded.
```

### 5.3 你当前这套工具链的实际状态

对你给出的这套工具链，当前实测结果是：

```text
cannot find -lstdc++
cannot find -lm
```

也就是：

- 它有 `aarch64` 编译器
- 也有 `sysroot`
- 但对这个 C++ 项目来说，它当前还不是一套完整可用的 C++ SDK

所以如果你直接用这套工具链编本项目，会在自检阶段失败。

这时有两种处理方式：

1. 换成更完整的 `aarch64` Linux SDK / Buildroot SDK
2. 补齐缺失的 C++ 头文件和运行库目录，再通过脚本参数传进去

### 5.4 如果工具链需要补额外目录

如果你拿到的是“编译器主体 + 额外库目录”的组合，可以这样传：

```powershell
$ExtraIncludeDirs = @(
  'D:\sdk\include'
)

$ExtraLibraryDirs = @(
  'D:\sdk\lib',
  'D:\sdk\lib64'
)

.\scripts\validate-arm64-toolchain-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -AdditionalIncludeDirs $ExtraIncludeDirs `
  -AdditionalLibraryDirs $ExtraLibraryDirs
```

### 5.5 正式交叉编译

先准备好 ARM64 版本的 `librdkafka` 安装前缀。例如：

```powershell
$RdkafkaRoot = 'D:\sdk\rdkafka-aarch64'
```

然后构建：

```powershell
cd E:\wulianwnag\c-kafka-trace-producer

$ToolchainRoot = 'E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu'
$RdkafkaRoot = 'D:\sdk\rdkafka-aarch64'

.\scripts\build-arm64-cross-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot
```

如果工具链还依赖补充目录：

```powershell
.\scripts\build-arm64-cross-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot `
  -AdditionalIncludeDirs $ExtraIncludeDirs `
  -AdditionalLibraryDirs $ExtraLibraryDirs
```

默认输出目录是：

```text
build\arm64-cross-release
```

### 5.6 Windows 上打包运行包

构建成功后执行：

```powershell
cd E:\wulianwnag\c-kafka-trace-producer

$ToolchainRoot = 'E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu'
$RdkafkaRoot = 'D:\sdk\rdkafka-aarch64'

.\scripts\package-release-cross-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot
```

如果需要补库目录：

```powershell
.\scripts\package-release-cross-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot `
  -AdditionalLibraryDirs $ExtraLibraryDirs
```

默认会生成：

- `release\arm64`
- `c-kafka-trace-producer-arm64.tar.gz`

### 5.7 Windows 上验证产物架构

可以用工具链自带的 `objdump.exe` 检查：

```powershell
& "$ToolchainRoot\bin\objdump.exe" -f .\build\arm64-cross-release\c-kafka-trace-producer
```

正常应出现：

```text
architecture: aarch64
```

## 6. 正式部署到目标机

无论你走路径 A 还是路径 B，拿到的最终压缩包都是：

```text
c-kafka-trace-producer-arm64.tar.gz
```

把它传到目标机 `/opt/` 下即可。

### 6.1 目标机解压

Buildroot 上很多时候是 BusyBox `tar`，不支持 GNU tar 的 `-z`。

所以推荐这样解压：

```sh
mkdir -p /opt/c-kafka-trace-producer-release
gzip -dc /opt/c-kafka-trace-producer-arm64.tar.gz | tar -xf - -C /opt/c-kafka-trace-producer-release --strip-components=1
```

然后检查：

```sh
ls -l /opt/c-kafka-trace-producer-release
ls -l /opt/c-kafka-trace-producer-release/bin
```

### 6.2 先验证 librdkafka 本身

在目标机上，建议先跑 probe，再跑完整 producer：

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-rdkafka-probe
```

如果你关心某些特性，例如 TLS / SASL，可直接要求它检查：

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-rdkafka-probe --require ssl,sasl
```

正常会输出类似：

```text
=== librdkafka Probe ===
librdkafka version: ...
builtin.features: gzip,snappy,ssl,sasl,...
rd_kafka_new: ok
require[ssl]: ok
require[sasl]: ok
probe result: ok
```

如果这里已经失败，就不要继续跑完整 producer，先解决 `librdkafka` 兼容性问题。

### 6.3 首次启动：再前台验证 producer

```sh
cd /opt/c-kafka-trace-producer-release
chmod +x bin/*
chmod +x deploy/device/*.sh
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-trace-producer ./config/application.properties
```

正常启动后，日志会出现类似：

```text
=== Native Kafka Trace Producer ===
=== Trace Config ===
[KafkaProducer] creating Kafka config...
[KafkaProducer] initialized: ...
>>> sending 1000000 frames to Kafka ...
[TraceSimulator] send loop started: ...
```

如果你已经看到了持续增长的进度日志，就说明 producer 已经在正常发送数据。

### 6.4 验证消费

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-trace-consumer 47.129.128.147:9092 trace-data
```

### 6.5 后台运行

如果这台目标机已经确认有 `bash`：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/start.sh
bash ./deploy/device/status.sh
bash ./deploy/device/tail-log.sh
```

停止：

```sh
cd /opt/c-kafka-trace-producer-release
bash ./deploy/device/stop.sh
```

如果你不想用脚本，也可以手工后台：

```sh
cd /opt/c-kafka-trace-producer-release
mkdir -p logs
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
nohup ./bin/c-kafka-trace-producer ./config/application.properties > ./logs/producer.log 2>&1 &
echo $! > ./logs/producer.pid
```

## 7. 修改配置后的处理方式

### 7.1 只改源码目录里的配置

如果你修改的是源码目录里的：

```text
config/application.properties
```

那么：

- 不需要重新编译
- 需要重新打包
- 需要重新传包到目标机

Linux ARM64 原生打包：

```bash
./scripts/package-release.sh "$PWD/build/arm64-release" "$PWD/release/arm64"
tar -czf /opt/c-kafka-trace-producer-arm64.tar.gz -C release arm64
```

Windows 交叉打包：

```powershell
.\scripts\package-release-cross-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot
```

### 7.2 只改目标机上的配置

如果你修改的是目标机上的：

```text
/opt/c-kafka-trace-producer-release/config/application.properties
```

那么：

- 不需要重新编译
- 不需要重新打包
- 只需要重启程序

## 8. 常见问题

### 8.1 `Exec format error`

这说明目标机拿到的不是 ARM64 二进制。

最常见原因：

- 在 `x86_64` 主机上直接执行了 `build-arm64-native.sh`
- 构建目录名字虽然叫 `arm64`，但实际上还是本机原生编译

先确认：

```bash
file build/arm64-release/c-kafka-trace-producer
```

或者：

```powershell
& "$ToolchainRoot\bin\objdump.exe" -f .\build\arm64-cross-release\c-kafka-trace-producer
```

### 8.2 `cannot find -lstdc++`

说明你的交叉工具链不完整，缺失 C++ 运行时库。

处理方式：

- 换一套完整的 `aarch64` Linux SDK / Buildroot SDK
- 或补齐 `libstdc++` 所在目录，再通过 `-AdditionalLibraryDirs` 传给脚本

### 8.3 `cannot find -lm`

说明工具链的标准数学库没有被正确找到。

通常也是：

- 工具链不完整
- 或 `sysroot` / 补充库目录不完整

### 8.4 `Cross-compilation requires RDKAFKA_ROOT`

说明你在交叉编译时没有提供 ARM64 版本的 `librdkafka` 安装前缀。

需要补上：

```text
-RdkafkaRoot D:\path\to\rdkafka-aarch64
```

### 8.5 `invalid tar magic`

如果目标机直接：

```sh
tar -xzf xxx.tar.gz
```

报错，不是包坏了，通常是 BusyBox `tar` 对 `.tar.gz` 处理方式不同。

改用：

```sh
gzip -dc xxx.tar.gz | tar -xf -
```

### 8.6 `librdkafka.so` 找不到

先确认启动前设置了：

```sh
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

再检查运行包里是否真的带出了对应 `.so` 文件。

## 9. 推荐流程

### 9.1 如果你有 ARM64 Linux 构建机

优先走路径 A，最省心。

### 9.2 如果你只有 Windows x86_64 主机

走路径 B，但先执行：

```powershell
.\scripts\validate-arm64-toolchain-windows.ps1 -ToolchainRoot $ToolchainRoot
```

只有这个步骤成功后，再继续构建和打包。

### 9.3 如果你的工具链像当前这套一样卡在 `-lstdc++`

先不要继续编项目，先去拿一套更完整的 SDK，或者补齐缺失的 C++ 库目录后再继续。
