# Zygisk Injector

**[English Version](README.md)**

**Zygisk Injector** 是一款基于 Magisk Zygisk 技术的 Android 动态 SO 注入工具。它允许用户通过 Kotlin 编写的管理界面，在应用启动阶段将自定义的共享库（.so）注入到目标游戏或应用进程中。

## 核心特性

- **Zygisk 级注入**：利用 Zygisk API，在应用进程初始化前完成注入
- **隐藏式加载**：通过 Zygisk 内部机制调用 `dlopen`
- **Kotlin 管理端**：提供现代化的 UI 界面，管理目标应用列表和 SO 文件路径
- **动态配置**：通过配置文件实现 APK 与 Zygisk 模块的通信
- **高兼容性**：支持 Android 9.0+ 及 Magisk Zygisk 环境

## 项目架构

项目由三个核心部分组成：

1. **Manager APK (Kotlin)** - `injector-app/`
   - 负责用户交互，选择目标包名
   - 管理待注入的 `.so` 文件
   - 将配置写入 `/data/adb/zygisk/injector_targets.txt`

2. **Zygisk 模块 (C++)** - `module/`
   - 驻留在 Zygote 进程中
   - 读取配置文件进行 SO 注入
   - 使用 ptrace + dlopen 机制

3. **注入的 SO** - 用户自定义
   - 实际执行 Hook 或功能修改的逻辑库
   - 通过 JNI 与管理 APK 通信

## 通信机制

采用**配置文件轮询**的方式：

1. **APK 端**：用户点击"注入 SO"时，将配置写入 `/data/adb/zygisk/injector_targets.txt`
   ```
   包名:SO路径:初始化函数名
   ```

2. **模块端**：Zygisk 模块在应用启动时读取配置文件，匹配包名后执行注入

## 快速开始

### 环境要求

- 已 Root 的 Android 设备
- 已安装 Magisk v24.0+ 并开启 **Zygisk** 选项
- Android NDK (用于编译 C++ 部分)

### 安装步骤

1. **编译模块**
   ```bash
   ./gradlew zipDebug
   ```
   生成的模块 zip 包在 `module/release/` 目录

2. **安装模块**
   - 在 Magisk 中安装生成的 zip 文件
   - 重启设备

3. **安装 APK**
   - 安装 `injector-app/build/outputs/apk/release/injector-app-release.apk`
   - 打开应用

4. **配置注入**
   - 输入目标包名（如 `com.example.game`）
   - 输入 SO 路径（如 `/data/data/com.example.game/lib/libtool.so`）
   - 点击"注入 SO"
   - 重启目标应用

## 配置文件格式

注入目标配置文件：`/data/adb/zygisk/injector_targets.txt`

格式：
```
包名:SO路径:初始化函数名
```

示例：
```
com.example.game:/data/data/com.example.game/lib/libtool.so:onLoad
```

## 开发计划

- [ ] 支持多 SO 同时注入
- [ ] 集成 Native Hook 模板
- [ ] 增加注入状态的实时日志
- [ ] 优化反作弊检测兼容性

## 项目结构

```
zygisk-Dynamic-Injector-tool/
├── injector-app/           # Kotlin 管理 APK
│   ├── src/main/
│   │   ├── java/com/injector/app/
│   │   │   └── MainActivity.kt
│   │   ├── res/layout/
│   │   │   └── activity_main.xml
│   │   └── AndroidManifest.xml
│   └── build.gradle.kts
│
├── module/                 # Zygisk 模块
│   ├── src/main/
│   │   ├── cpp/
│   │   │   ├── zygisk_injector.cpp
│   │   │   ├── injector_core.cpp
│   │   │   └── libhack.cpp
│   │   ├── res/
│   │   └── AndroidManifest.xml
│   ├── template/           # Magisk 模块模板
│   │   ├── module.prop
│   │   ├── customize.sh
│   │   └── zn_modules.txt
│   └── build.gradle.kts
│
└── build.gradle.kts        # 根构建文件
```

## 编译命令

```bash
# 编译 Zygisk 模块
./gradlew zipDebug        # Debug 版本
./gradlew zipRelease      # Release 版本

# 编译管理 APK
./gradlew :injector-app:assembleRelease
```

## 免责声明

本工具仅用于技术研究与学习。请勿将其用于任何违反法律法规或目标应用服务协议的行为。开发者不对因使用本工具导致的任何账号封禁、数据丢失或法律责任负责。