# ARM64 / Buildroot 构建与部署指南（当前 C 版本）

本文档面向下面这类环境：

- 目标设备是 `aarch64` Linux
- 目标系统是 Buildroot，适合运行成品，不适合在设备上完整编译
- 开发主机是 Windows x86_64
- 你已经有一套 `aarch64-none-linux-gnu` 交叉工具链
- 你已经有一份单独编好的 ARM64 `librdkafka`

当前项目已经从 C++ 迁成纯 C。对你当前这套工具链来说，这个变化很关键，因为现在项目本身不再依赖 ARM64 `libstdc++`。

## 1. 当前结论

- 当前仓库已经可以直接生成 ARM64 Linux 可执行文件。
- 已经验证通过的交叉构建产物是：
  - `build/arm64-cross-release/c-kafka-trace-producer`
  - `build/arm64-cross-release/c-kafka-trace-consumer`
  - `build/arm64-cross-release/c-kafka-rdkafka-probe`
- 已经验证过的打包产物是：
  - `release/arm64`
  - `c-kafka-trace-producer-arm64.tar.gz`
- 这套流程依赖外部 ARM64 `librdkafka`，不需要把 `librdkafka` 源码整个拷进当前项目。

## 2. 你当前环境里用到的关键路径

以下路径是当前已经跑通的一套示例：

- 项目根目录：`E:\wulianwnag\c-kafka-trace-producer`
- ARM64 交叉工具链：`E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu`
- 外部 ARM64 `librdkafka` 安装前缀：`E:\wulianwnag\testkafka\output`
- 本地 shim 库目录：`E:\wulianwnag\c-kafka-trace-producer\build\toolchain-shim`

其中：

- `RDKAFKA_ROOT` 就是外部 `librdkafka` 的安装前缀
- `build/toolchain-shim` 用来补齐 `-lpthread`、`-ldl`、`-lm`、`-lrt` 这类开发期链接名

## 3. 推荐方案：Windows 直接交叉编译

如果你的 Windows 环境里没有 `cmake` 或 `ninja`，推荐直接用仓库里的 GCC 直调脚本。

### 3.1 进入项目目录

```powershell
cd E:\wulianwnag\c-kafka-trace-producer
```

### 3.2 定义路径变量

```powershell
$ToolchainRoot = 'E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu'
$RdkafkaRoot = 'E:\wulianwnag\testkafka\output'
```

### 3.3 先做工具链探测

```powershell
.\scripts\validate-arm64-toolchain-windows.ps1 -ToolchainRoot $ToolchainRoot
```

如果你的工具链缺少 `libpthread.so`、`libm.so` 这类开发期别名，可以先执行：

```powershell
.\scripts\prepare-arm64-toolchain-shim-windows.ps1 -ToolchainRoot $ToolchainRoot
```

不过当前仓库里的直调构建脚本已经会自动准备 `build/toolchain-shim`，一般不需要你单独手工跑这一步。

### 3.4 直接构建

```powershell
.\scripts\build-arm64-cross-windows-direct.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot
```

默认输出目录：

```text
build\arm64-cross-release
```

### 3.5 打包运行目录

```powershell
.\scripts\package-release-cross-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot
```

默认会生成：

- `release\arm64`
- `c-kafka-trace-producer-arm64.tar.gz`

## 4. 备选方案：Windows + CMake 交叉编译

如果你的环境里已经装好了 `cmake` 和 `ninja`，也可以走 CMake 路径：

```powershell
cd E:\wulianwnag\c-kafka-trace-producer

$ToolchainRoot = 'E:\VSCode-win32-x64-1.85.1\data\extensions\undefined_publisher.devuni-ide-vscode-0.0.1\tool\iec-runtime-gen-run\gcc-arm-10.3-aarch64-none-linux-gnu'
$RdkafkaRoot = 'E:\wulianwnag\testkafka\output'

.\scripts\build-arm64-cross-windows.ps1 `
  -ToolchainRoot $ToolchainRoot `
  -RdkafkaRoot $RdkafkaRoot
```

如果脚本发现本地没有 `build/toolchain-shim`，它现在也会自动准备一份默认 shim。

## 5. 验证产物架构

你可以直接用工具链自带的 `readelf` 或 `objdump` 检查：

```powershell
& "$ToolchainRoot\bin\readelf.exe" -h .\build\arm64-cross-release\c-kafka-trace-producer
```

正常应该看到：

```text
Class:   ELF64
Machine: AArch64
```

如果你看到的是 `x86-64`，说明这不是 ARM64 产物。

## 6. 部署到 Buildroot 目标机

推荐把 `c-kafka-trace-producer-arm64.tar.gz` 传到目标机，例如放到 `/opt/`。

### 6.1 解压

Buildroot 常见的是 BusyBox `tar`，建议这样解压：

```sh
mkdir -p /opt/c-kafka-trace-producer-release
gzip -dc /opt/c-kafka-trace-producer-arm64.tar.gz | tar -xf - -C /opt/c-kafka-trace-producer-release --strip-components=1
```

### 6.2 先做 `librdkafka` 探测

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-rdkafka-probe
```

如果你明确知道自己的 `librdkafka` 是带哪些能力编出来的，也可以按需加 `--require`，例如：

```sh
./bin/c-kafka-rdkafka-probe --require gzip
```

不要默认要求 `ssl,sasl`，除非你的 ARM64 `librdkafka` 确实开启了这些能力。

### 6.3 启动 producer

```sh
cd /opt/c-kafka-trace-producer-release
chmod +x bin/*
chmod +x deploy/device/*.sh
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-trace-producer ./config/application.properties
```

### 6.4 验证 consumer

```sh
cd /opt/c-kafka-trace-producer-release
export LD_LIBRARY_PATH="$PWD/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
./bin/c-kafka-trace-consumer 47.129.128.147:9092 trace-data
```

### 6.5 后台运行

```sh
cd /opt/c-kafka-trace-producer-release
./deploy/device/start.sh
./deploy/device/status.sh
./deploy/device/tail-log.sh
```

停止：

```sh
cd /opt/c-kafka-trace-producer-release
./deploy/device/stop.sh
```

## 7. 配置文件修改规则

源码目录里的配置文件是：

```text
config/application.properties
```

如果你修改的是仓库里的这个文件：

- 不需要重新编译
- 需要重新打包
- 需要把新的运行目录重新传到目标机

如果你修改的是目标机上的：

```text
/opt/c-kafka-trace-producer-release/config/application.properties
```

那就只需要重启程序，不需要重新编译，也不需要重新打包。

## 8. 常见问题

### 8.1 `cannot find -lpthread` / `cannot find -ldl` / `cannot find -lm` / `cannot find -lrt`

这说明你的交叉工具链里有运行时库，但缺少开发期链接名。

处理方式：

- 运行 `.\scripts\prepare-arm64-toolchain-shim-windows.ps1 -ToolchainRoot $ToolchainRoot`
- 或者手动把对应库目录通过 `-AdditionalLibraryDirs` 传给构建脚本

### 8.2 `Cross-compilation requires RDKAFKA_ROOT`

这说明你在交叉编译时没有给当前项目传 ARM64 `librdkafka` 前缀。

需要提供：

```text
-RdkafkaRoot E:\path\to\rdkafka-arm64
```

并确保下面两个路径真实存在：

- `include\librdkafka\rdkafka.h`
- `lib\librdkafka.a` 或 `lib\librdkafka.so`

### 8.3 `Exec format error`

这说明目标机拿到的不是 ARM64 Linux ELF。

先回到开发机检查：

```powershell
& "$ToolchainRoot\bin\readelf.exe" -h .\build\arm64-cross-release\c-kafka-trace-producer
```

### 8.4 程序能启动，但连不上 Kafka broker

如果可执行文件本身已经在目标机运行起来，只是日志里出现：

- `Connect to ... failed`
- `No route to host`
- `1/1 brokers are down`
- `flush timeout`

那通常已经不是编译兼容性问题，而是网络问题。继续排查：

- 目标机是否能路由到 broker
- broker 的 `9092` 是否对目标机开放
- 云安全组或防火墙是否放行
- broker 对外监听地址是否正确

## 9. 建议你后续固定下来的工作流

对你当前这套环境，建议以后都按这个顺序来：

1. 在 `E:\wulianwnag\testkafka\output` 维护你自己编好的 ARM64 `librdkafka`
2. 在当前项目里把它作为外部依赖，通过 `RDKAFKA_ROOT` 引入
3. 用 `build-arm64-cross-windows-direct.ps1` 生成 ARM64 可执行文件
4. 用 `package-release-cross-windows.ps1` 生成运行包
5. 把运行包传到 Buildroot 目标机验证

这条链路已经和你当前的 Buildroot ARM64 环境对齐，不需要再回退到 C++ 方案。
