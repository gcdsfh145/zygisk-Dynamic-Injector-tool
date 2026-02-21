# Zygisk Injector

**[中文版本](README_CN.md)**

**Zygisk Injector** is an Android dynamic SO injection tool based on Magisk Zygisk technology. It allows users to inject custom shared libraries (.so) into target game or application processes during application startup through a Kotlin-based management interface.

## Core Features

- **Zygisk-level Injection**: Uses Zygisk API to complete injection before application process initialization
- **Hidden Loading**: Uses `dlopen` via Zygisk internal mechanisms
- **Kotlin Management UI**: Provides a modern interface for managing target app list and SO file paths
- **Dynamic Configuration**: Enables APK communication with Zygisk module via configuration files
- **High Compatibility**: Supports Android 9.0+ and Magisk Zygisk environment

## Project Architecture

The project consists of three core components:

1. **Manager APK (Kotlin)** - `injector-app/`
   - Handles user interaction, selecting target package names
   - Manages `.so` files to be injected
   - Writes configuration to `/data/adb/zygisk/injector_targets.txt`

2. **Zygisk Module (C++)** - `module/`
   - Resides in Zygote process
   - Reads configuration file for SO injection
   - Uses ptrace + dlopen mechanism

3. **Injected SO** - User-defined
   - Executes actual Hook or functionality modification logic
   - Communicates with management APK via JNI

## Communication Mechanism

Uses **configuration file polling** method:

1. **APK Side**: When user clicks "Inject SO", writes configuration to `/data/adb/zygisk/injector_targets.txt`
   ```
   PackageName:SOPath:InitFunctionName
   ```

2. **Module Side**: Zygisk module reads configuration file during app startup, executes injection after matching package name

## Quick Start

### Requirements

- Rooted Android device
- Magisk v24.0+ installed with **Zygisk** enabled
- Android NDK (for compiling C++ parts)

### Installation Steps

1. **Build Module**
   ```bash
   ./gradlew zipDebug
   ```
   Generated module zip is in `module/release/` directory

2. **Install Module**
   - Install the generated zip file in Magisk
   - Reboot device

3. **Install APK**
   - Install `injector-app/build/outputs/apk/release/injector-app-release.apk`
   - Open the app

4. **Configure Injection**
   - Enter target package name (e.g., `com.example.game`)
   - Enter SO path (e.g., `/data/data/com.example.game/lib/libtool.so`)
   - Click "Inject SO"
   - Restart target app

## Configuration File Format

Injection target configuration file: `/data/adb/zygisk/injector_targets.txt`

Format:
```
PackageName:SOPath:InitFunctionName
```

Example:
```
com.example.game:/data/data/com.example.game/lib/libtool.so:onLoad
```

## Roadmap

- [ ] Support multiple SO simultaneous injection
- [ ] Integrate Native Hook template
- [ ] Add real-time injection status logging
- [ ] Optimize anti-cheat detection compatibility

## Project Structure

```
zygisk-Dynamic-Injector-tool/
├── injector-app/           # Kotlin Manager APK
│   ├── src/main/
│   │   ├── java/com/injector/app/
│   │   │   └── MainActivity.kt
│   │   ├── res/layout/
│   │   │   └── activity_main.xml
│   │   └── AndroidManifest.xml
│   └── build.gradle.kts
│
├── module/                 # Zygisk Module
│   ├── src/main/
│   │   ├── cpp/
│   │   │   ├── zygisk_injector.cpp
│   │   │   ├── injector_core.cpp
│   │   │   └── libhack.cpp
│   │   ├── res/
│   │   └── AndroidManifest.xml
│   ├── template/           # Magisk Module Template
│   │   ├── module.prop
│   │   ├── customize.sh
│   │   └── zn_modules.txt
│   └── build.gradle.kts
│
└── build.gradle.kts        # Root Build File
```

## Build Commands

```bash
# Build Zygisk Module
./gradlew zipDebug        # Debug version
./gradlew zipRelease      # Release version

# Build Manager APK
./gradlew :injector-app:assembleRelease
```

## Disclaimer

This tool is for technical research and learning purposes only. Please do not use it for any activities that violate laws, regulations, or target application service agreements. The developer is not responsible for any account bans, data loss, or legal liabilities caused by using this tool.