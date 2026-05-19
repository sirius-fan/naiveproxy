# NaïveProxy 编译流程文档

当前基于 Chromium 版本：`148.0.7778.96`

---

## 目录

1. [项目结构概览](#项目结构概览)
2. [环境依赖](#环境依赖)
3. [编译工具链准备](#编译工具链准备)
4. [各平台编译步骤](#各平台编译步骤)
   - [Linux](#linux)
   - [Windows](#windows)
   - [macOS](#macos)
   - [Android](#android)
   - [OpenWrt](#openwrt)
5. [编译模式](#编译模式)
6. [EXTRA\_FLAGS 参数说明](#extra_flags-参数说明)
7. [编译产物](#编译产物)
8. [打包与测试](#打包与测试)

---

## 项目结构概览

```
naiveproxy/
├── CHROMIUM_VERSION        # 当前依赖的 Chromium 版本号
├── src/                    # 主要源码目录（工作目录）
│   ├── build.sh            # 核心编译脚本
│   ├── get-clang.sh        # 下载编译工具链（Clang/GN/PGO）
│   ├── get-sysroot.sh      # 平台 sysroot 配置
│   ├── get-android-sys.sh  # Android sysroot 准备
│   ├── get-openwrt.sh      # OpenWrt sysroot 准备
│   └── config.json         # 运行时配置示例
├── apk/                    # Android APK 打包（Gradle）
├── tests/
│   └── basic.sh            # 基础功能测试脚本
└── .github/workflows/
    └── build.yml           # CI 编译流程定义
```

所有编译操作均在 `src/` 目录下执行。

---

## 环境依赖

### Linux（Ubuntu 22.04 推荐）

```sh
sudo apt update
sudo apt install ninja-build pkg-config ccache bubblewrap
```

> 交叉编译非 x64 架构时，还需要安装 `qemu-user`：
> ```sh
> sudo apt remove -y qemu-user-binfmt
> sudo dpkg -i qemu-user*.deb   # 从 snapshot.debian.org 下载的特定版本
> sudo apt remove libc6-i386    # 避免干扰 x86 交叉编译
> ```

### macOS

```sh
brew install ninja ccache
pip install setuptools
```

### Windows

- 需要安装 [Git Bash](https://git-scm.com/) 以运行 `.sh` 脚本
- 安装 [sccache](https://github.com/mozilla/sccache)（自动下载到 `~/.cargo/bin/`）
- 安装 [ninja](https://ninja-build.org/)（下载到 `~/bin/ninja.exe`）

### Android（额外）

- JDK 21（用于 Gradle 打包 APK）
- 安装方式同 Linux

---

## 编译工具链准备

所有平台在编译前必须先运行 `get-clang.sh` 下载以下工具：

| 工具 | 说明 |
|------|------|
| **Clang** | Chromium 专用 LLVM 工具链，从 Google Storage 下载 |
| **GN** | 构建文件生成器，从 Chrome Infra Packages 下载 |
| **PGO Profile** | 性能剖析优化数据，Release 构建必需 |
| **Android NDK** | 仅 Android 构建需要 |
| **Linux Sysroot** | 交叉编译时下载对应架构的 Debian sysroot |

```sh
cd src

# 本机架构（自动检测）
./get-clang.sh

# 指定目标平台/架构
EXTRA_FLAGS='target_cpu="arm64"' ./get-clang.sh
EXTRA_FLAGS='target_os="android" target_cpu="arm64"' ./get-clang.sh
```

脚本会自动检测宿主机系统（`host_os` / `host_cpu`）并下载对应版本的工具链。

---

## 各平台编译步骤

### Linux

支持架构：`x64`、`x86`、`arm64`、`arm`、`mipsel`、`mips64el`、`riscv64`、`loong64`

```sh
cd src

# 1. 下载工具链
EXTRA_FLAGS='target_cpu="x64"' ./get-clang.sh

# 2. 编译
EXTRA_FLAGS='target_cpu="x64"' ./build.sh

# 3. 测试
../tests/basic.sh out/Release/naive
```

**Sysroot 映射（Debian 发行版）：**

| 架构 | Sysroot 版本 |
|------|-------------|
| x64 / x86 / arm64 / arm / mipsel / mips64el | bullseye |
| riscv64 | trixie |
| loong64 | sid |

本机架构且不指定 `EXTRA_FLAGS` 时，不下载 sysroot，直接使用宿主系统。

---

### Windows

支持架构：`x64`、`x86`、`arm64`

```sh
cd src

# 1. 下载工具链
EXTRA_FLAGS='target_cpu="x64"' ./get-clang.sh

# 2. 编译（使用 sccache 替代 ccache）
EXTRA_FLAGS='target_cpu="x64"' ./build.sh

# 3. 测试（arm64 无法测试）
../tests/basic.sh out/Release/naive.exe
```

Windows 上使用 `sccache` 作为编译缓存（自动检测 `~/.cargo/bin/sccache.exe`）。

---

### macOS

支持架构：`x64`（Intel）、`arm64`（Apple Silicon）

```sh
cd src

# 1. 下载工具链
EXTRA_FLAGS='target_cpu="arm64"' ./get-clang.sh

# 2. 编译
EXTRA_FLAGS='target_cpu="arm64"' ./build.sh

# 3. 测试
../tests/basic.sh out/Release/naive
```

---

### Android

支持架构：`x64`、`x86`、`arm64`、`arm`

```sh
cd src

# 1. 下载工具链（包含 Android NDK）
EXTRA_FLAGS='target_os="android" target_cpu="arm64"' ./get-clang.sh

# 2. 编译（需要 ccache 和 bubblewrap）
EXTRA_FLAGS='target_os="android" target_cpu="arm64"' ./build.sh

# 3. 准备 Android 系统镜像（用于测试）
EXTRA_FLAGS='target_os="android" target_cpu="arm64"' ./get-android-sys.sh

# 4. 测试
../tests/basic.sh out/Release/naive

# 5. 打包 APK（在 apk/ 目录下）
cd ../apk
APK_ABI=arm64-v8a APK_VERSION_NAME=v1.0.0 ./gradlew :app:assembleRelease
```

APK 输出位于 `apk/app/build/outputs/apk/release/`。

---

### OpenWrt

```sh
cd src

# 通过 OPENWRT_FLAGS 指定目标设备参数
EXTRA_FLAGS='target_os="openwrt"' OPENWRT_FLAGS='release="...", arch="..."' ./get-clang.sh
EXTRA_FLAGS='target_os="openwrt"' OPENWRT_FLAGS='release="...", arch="..."' ./build.sh
```

可用设备列表见 `../tools/list-openwrt.sh`。

---

## 编译模式

`build.sh` 支持两种模式，通过第一个参数控制：

### Release 模式（默认）

```sh
./build.sh
```

输出目录：`out/Release/naive`

启用的优化选项：
- `is_official_build=true` — 官方构建优化
- `is_chrome_branded=true` — Chrome 品牌标记
- `chrome_pgo_phase=2` — 完整 PGO 优化
- `symbol_level=0` — 不生成调试符号
- `exclude_unwind_tables=true` — 排除栈展开表

### Debug 模式

```sh
./build.sh debug
```

输出目录：`out/Debug/naive`

启用的选项：
- `is_debug=true`
- `is_component_build=true` — 组件化构建（更快增量编译）
- `chrome_pgo_phase=0` — 禁用 PGO

---

## EXTRA\_FLAGS 参数说明

`EXTRA_FLAGS` 是传递给 GN 构建系统的额外参数，同时也由 `get-sysroot.sh` 解析以确定目标平台。

常用组合：

```sh
# 指定目标 CPU
EXTRA_FLAGS='target_cpu="arm64"'

# 指定目标 OS + CPU
EXTRA_FLAGS='target_os="android" target_cpu="arm64"'
EXTRA_FLAGS='target_os="openwrt" target_cpu="mipsel"'

# 多参数组合
EXTRA_FLAGS='target_cpu="x64" use_custom_libcxx=false'
```

支持的 `target_os` 值：`linux`、`win`、`mac`、`android`、`openwrt`

支持的 `target_cpu` 值：`x64`、`x86`、`arm64`、`arm`、`mipsel`、`mips64el`、`riscv64`、`loong64`

---

## 编译缓存

编译脚本自动检测并启用缓存工具：

| 平台 | 工具 | 缓存目录 |
|------|------|---------|
| Linux / macOS | ccache | `~/.cache/ccache` |
| Windows | sccache | `~/AppData/Local/Mozilla/sccache` |

ccache 相关环境变量：
```sh
export CCACHE_SLOPPINESS=time_macros
export CCACHE_BASEDIR="$PWD"
export CCACHE_CPP2=yes
export CCACHE_MAXSIZE=200M
```

宿主机工具（非目标平台的编译器）使用独立缓存目录 `src/.host_tool_cache`。

---

## 编译产物

| 路径 | 说明 |
|------|------|
| `src/out/Release/naive` | Linux / macOS 可执行文件 |
| `src/out/Release/naive.exe` | Windows 可执行文件 |
| `apk/app/build/outputs/apk/release/*.apk` | Android APK |

Release 包内容（`naiveproxy-<version>-<platform>-<arch>/`）：

```
naive          # 可执行文件
config.json    # 配置示例
LICENSE        # 许可证
USAGE.txt      # 使用说明
```

Linux/macOS 打包为 `.tar.xz`，Windows 打包为 `.zip`。

---

## 打包与测试

```sh
cd src

# 基础功能测试
../tests/basic.sh out/Release/naive

# 打包（Linux/macOS）
BUNDLE=naiveproxy-v1.0.0-linux-x64
mkdir "$BUNDLE"
cp out/Release/naive config.json ../LICENSE ../USAGE.txt "$BUNDLE"
tar cJf "$BUNDLE.tar.xz" "$BUNDLE"

# 计算 SHA256
openssl sha256 out/Release/naive
```

---

## 完整编译流程示意

```
克隆仓库
    │
    ▼
安装系统依赖（ninja/ccache/bubblewrap 等）
    │
    ▼
cd src
EXTRA_FLAGS='...' ./get-clang.sh
（下载 Clang + GN + PGO Profile + Sysroot/NDK）
    │
    ▼
EXTRA_FLAGS='...' ./build.sh [debug]
（GN 生成构建文件 → Ninja 编译 → out/Release/naive）
    │
    ▼
../tests/basic.sh out/Release/naive
（基础功能测试）
    │
    ▼
打包发布
```
