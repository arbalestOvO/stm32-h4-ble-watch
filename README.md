# STM32H743 BLE Smart Watch Project

## 📖 项目简介

这是一个基于 **STM32H743** 高性能MCU运行 **ThreadX** 实时操作系统和 **LVGL** 图形库的智能手表工程。项目集成了 **ESP32-C3** 作为蓝牙控制器，移植了 **NimBLE** 蓝牙协议栈，并实现了华为穿戴设备的私有协议栈及鉴权逻辑。

本项目重点解决了STM32H7系列在开启D-Cache和DMA2D加速下常见的图形渲染问题，以及双芯片蓝牙通信的底层驱动移植难点。

**演示地址**: http://47.115.146.185:3000/

## 🚀 核心特性与技术难点攻克

### 1. 图形显示与交互 (GUI)

- **LCD驱动移植**：成功在STM32H743上移植厂商LCD驱动。
- **LVGL & DMA2D优化**：移植LVGL图形库，并开启STM32硬件**DMA2D (Chrom-ART)** 加速。
- **触摸驱动 (GT911)**：调通GT911触摸芯片驱动。
- **🛠️ 关键问题修复**：
    - **内存崩溃 (OOM)**：解决SRAM空间不足问题，成功将LVGL的显存Buffer迁移至板载 **SDRAM**。
    - **重影与画面叠加 (Cache Coherence)**：
        - *问题*：CPU开启D-Cache的情况下，DMA2D直接搬运SDRAM数据时，数据可能还滞留在CPU Cache中，导致DMA搬运的是脏数据。
        - *解决*：在屏幕Flush之前，手动执行 **Clean Cache** 操作，确保数据一致性。
    - **背景噪点残留**：
        - *问题*：LVGL渲染透明图层时未写入背景像素，SDRAM中残留了上一帧的数据。
        - *解决*：利用LVGL Event回调，在绘制前预填充背景色，彻底清除残留噪点。

### 2. 操作系统 (RTOS)

- **ThreadX 适配**：将LVGL移植到Azure RTOS (ThreadX) 环境运行。
- **触摸延迟优化**：
    - *问题*：原厂触摸驱动在RTOS环境下存在I2C时序被打断的情况。
    - *解决*：修改驱动逻辑，加入中断锁机制，保证I2C通信原子性，显著降低触摸延迟。

### 3. 蓝牙与连接 (BLE Connectivity)

- **双芯片架构**：STM32 (Host) + ESP32-C3 (Controller)。
- **ESP-Hosted 移植**：将ESP32的Hosted BLE + SPI驱动移植到STM32 MCU工程中，解决了大量跨平台编译兼容性问题。
- **NimBLE 协议栈**：成功移植NimBLE开源蓝牙协议栈。
- **🛠️ HCI 通信修复**：
    - *问题*：蓝牙初始化时 `hci_reset` 回复乱码。
    - *解决*：排查发现Handshake握手信号未正确开启中断，修正后通信正常。
- **应用层协议**：实现了华为穿戴设备的私有协议栈及其鉴权逻辑。

### 4. 安全 (Security)

- **mbedtls 移植**：移植 mbedtls 加密模块。
- **硬件加速**：适配STM32硬件随机数生成器 (RNG)，提高加密效率与安全性。

## 🛠️ 硬件架构

- **主控芯片 (MCU)**: STM32H743 (ARM Cortex-M7, 480MHz)
- **蓝牙协处理器**: ESP32-C3 (需烧录 HCI Controller 固件)
- **显示屏**: 支持DMA2D接口的LCD屏幕
- **触摸芯片**: GT911

## 📂 目录结构

```
├── .cache/              # 缓存目录
├── AZURE_RTOS/          # ThreadX 操作系统核心文件
├── Core/                # STM32 HAL库核心代码及Main函数
├── Drivers/             # STM32 HAL驱动库
├── LinkProtocol/        # 通信协议相关 (ESP-Hosted/HCI)
├── Middlewares/         # 中间件
├── TF-PSA-Crypto/       # 加密库支持
├── commands/            # 命令行工具/调试指令
├── fonts/               # 字体文件
├── lv_conf.h            # LVGL 配置文件
├── mynewt-nimble/       # NimBLE 蓝牙协议栈
├── screens/             # UI 屏幕逻辑代码
├── ui/                  # 生成的UI资源文件
├── CMakeLists.txt       # CMake 构建脚本
├── stm32-h4-ble-watch.ioc # STM32CubeMX 配置文件
└── ...
```

## 🔨 构建与运行

本项目使用 **CMake** 进行构建，开发环境推荐使用 VSCode 或 CLion。

### 1. 前置准备

- 安装 `arm-none-eabi-gcc` 工具链。
- 安装 `CMake` 和 `Ninja` (可选)。
- 安装 STM32CubeMX (用于配置引脚和生成部分初始化代码)。

### 2. 硬件配置

1. 使用 STM32CubeMX 打开 `stm32-h4-ble-watch.ioc`。
2. 根据你实际使用的硬件板卡，修改引脚配置（GPIO, SPI, I2C, FMC等）。
3. 重新生成代码（如果修改了外设）。

### 3. 蓝牙固件烧录

**注意**：本工程将STM32作为Host，ESP32-C3作为Controller。

- 请务必先向 **ESP32-C3** 烧录对应的 **HCI Controller 从机固件**，否则蓝牙功能无法初始化。

### 4. 编译工程

在工程根目录下执行以下命令：

```
# 创建构建目录
mkdir build && cd build

# 生成 Makefile (Debug模式)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 编译
make -j8
```

或者如果你使用 Ninja:

```
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### 5. 烧录

使用 OpenOCD, JLink 或 ST-Link Utility 将生成的 `.elf` 或 `.bin` 文件烧录至 STM32H743。

## 🤝 贡献与反馈

如果你对本项目感兴趣或发现了Bug，欢迎提交 Issue 或 Pull Request。

Copyright (c) 2025-2026 arbalestOvO