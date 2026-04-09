# scripts 目录脚本说明

本文档对应项目：

```text
E:\wulianwnag\c-kafka-trace-producer
```

本文档只说明一件事：

- `scripts` 目录下面每个脚本分别是干嘛的
- 什么时候该用哪个脚本
- 你当前这套 Buildroot ARM64 实际主要走的是哪条链路

## 1. 先说结论

当前 `scripts` 目录里的脚本，大体分成 3 类：

1. 编译项目本体
2. 编译或准备 `librdkafka`
3. 打包发布产物

如果只看你现在最常用、最贴近实际交付的链路，核心是这几个：

- `build-arm64-cross-windows-direct.ps1`
- `package-release-cross-windows.ps1`
- `prepare-arm64-toolchain-shim-windows.ps1`
- `validate-arm64-toolchain-windows.ps1`

你当前已经生成出来的 ARM64 压缩包：

```text
E:\wulianwnag\c-kafka-trace-producer\c-kafka-trace-producer-arm64.tar.gz
```

更像是下面这条链路产出的：

```text
build-arm64-cross-windows-direct.ps1
    -> build\arm64-cross-release
    -> package-release-cross-windows.ps1
    -> release\arm64
    -> c-kafka-trace-producer-arm64.tar.gz
```

## 2. 脚本总览

### 2.1 `build-arm64-cross-windows-direct.ps1`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\build-arm64-cross-windows-direct.ps1
```

作用：

- 在 Windows 上直接调用你的 ARM64 交叉编译器 `gcc.exe`
- 不走 `CMakeLists.txt`
- 直接把项目源码编译成 3 个 ARM64 可执行文件

输出：

```text
build\arm64-cross-release\c-kafka-trace-producer
build\arm64-cross-release\c-kafka-trace-consumer
build\arm64-cross-release\c-kafka-rdkafka-probe
```

特点：

- 这是“直编路线”
- 依赖外部 ARM64 `librdkafka.a`
- 更贴近你之前实际跑通的那套方式

适合场景：

- 你已经有自己编好的 ARM64 `librdkafka`
- 你想绕开 CMake，直接确认能不能编出来
- 你现在这套 Buildroot ARM64 交付，优先建议走它

### 2.2 `build-arm64-cross-windows.ps1`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\build-arm64-cross-windows.ps1
```

作用：

- 在 Windows 上走 CMake 交叉编译流程
- 会读取项目根目录的 `CMakeLists.txt`
- 用 toolchain file + `RDKAFKA_ROOT` 配置生成 ARM64 构建

特点：

- 这是“CMake 路线”
- 需要本机有 `cmake`
- 默认生成器是 `Ninja`，所以一般还需要 `ninja`

输出目录默认也是：

```text
build\arm64-cross-release
```

适合场景：

- 你想走标准 CMake 构建方式
- 后面如果项目扩展，想让构建规则更统一

补充：

- 虽然项目支持这条路
- 但你当前手上的现成 ARM64 包，更像不是用这条路生成的

### 2.3 `build-arm64-local-rdkafka.sh`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\build-arm64-local-rdkafka.sh
```

作用：

- 在 Linux/ARM64 本机环境下
- 使用项目自己的 `CMakeLists.txt`
- 并且显式指定本项目内的本地 `librdkafka` 安装目录

它要求本地先有：

```text
third_party/rdkafka/install
```

也就是先跑过：

- `build-rdkafka-local.sh`

适合场景：

- 你是在 Linux 环境下本地编译
- 而且不想依赖系统自带 `librdkafka`
- 想用项目自己下载并编译出来的那一份

### 2.4 `build-arm64-native.sh`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\build-arm64-native.sh
```

作用：

- 在 Linux/ARM64 本机环境下
- 直接用 CMake 编译当前项目
- 默认依赖系统能找到 `librdkafka`

适合场景：

- 目标环境本身就是 ARM64 Linux
- 并且系统已经安装好 `librdkafka`

### 2.5 `build-rdkafka-local.sh`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\build-rdkafka-local.sh
```

作用：

- 在 Linux 环境下下载 `librdkafka` 源码
- 本地编译并安装到项目自己的 `third_party` 目录下

默认位置：

```text
third_party/archives
third_party/src
third_party/rdkafka/install
```

适合场景：

- 你在 Linux 上做本地开发
- 想让本项目自己带一份 `librdkafka`
- 后续再配合 `build-arm64-local-rdkafka.sh` 编译项目

### 2.6 `install-build-deps-ubuntu.sh`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\install-build-deps-ubuntu.sh
```

作用：

- 在 Ubuntu 上安装本项目常见的编译依赖

大致包括：

- `build-essential`
- `cmake`
- `pkg-config`
- `curl`
- `git`
- `librdkafka-dev`
- 以及一批压缩、TLS、压缩算法相关依赖

适合场景：

- 你在 Ubuntu 上第一次准备本地构建环境

### 2.7 `package-release-cross-windows.ps1`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\package-release-cross-windows.ps1
```

作用：

- 在 Windows 上把已经编好的 ARM64 二进制整理成发布目录
- 自动收集运行时依赖库
- 最后打成 `.tar.gz`

它做的事情主要是：

1. 从 `build\arm64-cross-release` 取出 3 个二进制
2. 复制到 `release\arm64\bin`
3. 复制 `config\application.properties`
4. 复制 `deploy\device` 下的启动/停止脚本
5. 收集运行时依赖库到 `release\arm64\lib`
6. 生成：

```text
E:\wulianwnag\c-kafka-trace-producer\c-kafka-trace-producer-arm64.tar.gz
```

这是“发布打包脚本”，不是编译脚本。

### 2.8 `package-release.sh`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\package-release.sh
```

作用：

- Linux 下的发布打包脚本
- 把本地已经编好的可执行文件和运行依赖整理到 `release/arm64`

和 Windows 版的区别：

- 它不打 `.tar.gz`
- 主要是生成运行目录
- 使用 `ldd` 拷贝运行时依赖

适合场景：

- 你在 Linux 环境完成本地编译后
- 想生成一份可部署的运行目录

### 2.9 `prepare-arm64-toolchain-shim-windows.ps1`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\prepare-arm64-toolchain-shim-windows.ps1
```

作用：

- 从你的 ARM64 工具链 `sysroot` 中
- 复制几份基础系统库到本地 `build\toolchain-shim`

复制的库主要有：

- `libpthread.so`
- `libdl.so`
- `libm.so`
- `librt.so`

作用本质上是：

- 给 Windows 下交叉编译和链接提供“辅助库目录”
- 避免链接阶段找不到这些基础 ARM64 运行库

它不是业务产物脚本，而是“工具链辅助脚本”。

### 2.10 `validate-arm64-toolchain-windows.ps1`

路径：

```text
E:\wulianwnag\c-kafka-trace-producer\scripts\validate-arm64-toolchain-windows.ps1
```

作用：

- 在 Windows 上对 ARM64 交叉编译器做一次快速探测
- 生成一个最小 C 程序
- 尝试编译
- 再用 `objdump` 验证产物架构是否真的是 `aarch64`

它主要解决的是：

- 你的工具链能不能正常用
- `sysroot` 是否有效
- `-lpthread -lm -ldl -lrt` 这些基础依赖是否能被找到

这是“编译前体检脚本”。

## 3. 这些脚本之间的关系

### 3.1 Windows 直编发布链路

这是你当前最贴近实际的一条：

1. `prepare-arm64-toolchain-shim-windows.ps1`
2. `validate-arm64-toolchain-windows.ps1`
3. `build-arm64-cross-windows-direct.ps1`
4. `package-release-cross-windows.ps1`

说明：

- 第 1、2 步一般也可能被第 3 步间接调用
- 真正编译项目的是第 3 步
- 真正打发布包的是第 4 步

### 3.2 Windows CMake 发布链路

如果你想走标准 CMake，则是：

1. `prepare-arm64-toolchain-shim-windows.ps1`
2. `validate-arm64-toolchain-windows.ps1`
3. `build-arm64-cross-windows.ps1`
4. `package-release-cross-windows.ps1`

区别只在第 3 步：

- 一个是直编
- 一个是走 `CMakeLists.txt`

### 3.3 Linux 本地构建链路

如果你在 Ubuntu 或其他 Linux 上本地开发，大概是：

1. `install-build-deps-ubuntu.sh`
2. `build-rdkafka-local.sh`
3. `build-arm64-local-rdkafka.sh`
4. `package-release.sh`

或者如果系统已经有 `librdkafka`：

1. `install-build-deps-ubuntu.sh`
2. `build-arm64-native.sh`
3. `package-release.sh`

## 4. 你现在实际最该记住哪几个

如果只从你当前 Windows + Buildroot ARM64 这套实际场景出发，最重要的是这 4 个：

### 4.1 `build-arm64-cross-windows-direct.ps1`

负责：

- 真正把源码编成 ARM64 可执行文件

### 4.2 `package-release-cross-windows.ps1`

负责：

- 把编译产物整理成可交付目录并打包

### 4.3 `prepare-arm64-toolchain-shim-windows.ps1`

负责：

- 给交叉链接准备基础 ARM64 系统库

### 4.4 `validate-arm64-toolchain-windows.ps1`

负责：

- 先验证你的编译器和 sysroot 没问题

## 5. 一句话总结

这个项目的 `scripts` 目录，本质上就是一套“构建 + 打包 + 工具链辅助”的脚本集合。

对你当前最重要的理解是：

- `build-arm64-cross-windows-direct.ps1` 是直编脚本
- `build-arm64-cross-windows.ps1` 是 CMake 编译脚本
- `package-release-cross-windows.ps1` 是 Windows 打包脚本
- `prepare-arm64-toolchain-shim-windows.ps1` 和 `validate-arm64-toolchain-windows.ps1` 是给 ARM64 交叉编译保驾护航的辅助脚本

而你当前已经交付过的 ARM64 压缩包，更像是：

```text
直编脚本 + Windows 打包脚本
```

这一套出来的。
