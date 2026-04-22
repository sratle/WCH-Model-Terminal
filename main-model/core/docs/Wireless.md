# Wireless 模块（CH585F）

## 概述

本模块为独立蓝牙 MCU，负责核心底板与上位机（手机 / PC）之间的蓝牙连接，同时承担蓝牙 OTA 升级功能。

- **主控芯片**：CH585F
- **与核心模块接口**：SPI4（应用层载荷遵循统一协议帧格式）
- **模块 ID**：`0x01`
- **模块类型编号**：`0x02`
- **Debug 接口**：UART1（仅供 CH585F 自身调试输出）

## 硬件组成

- **CH585F**：独立 RISC-V MCU，内置蓝牙协议栈（BLE / Classic）
- **天线**：板载陶瓷天线或外接 IPEX 天线（根据具体硬件版本）
- **SPI4**：与 CH32H417（V5F）全双工通信，用于收发协议帧
- **UART1**：Debug 串口，波特率 115200，输出蓝牙协议栈及模块运行日志

## 软件职责

### 初始化阶段

- 初始化 CH585F 自身时钟、GPIO、蓝牙协议栈
- 初始化 SPI4 从机模式，准备接收来自 V5F 的协议帧
- 设置默认广播名称、配对码等蓝牙参数
- 从 Flash 读取持久化配置（如已配对设备列表）

### 运行阶段

- **蓝牙服务**：维持广播 / 连接状态，与上位机 APP 交互
- **数据透传**：将 SPI4 收到的 Core 数据经蓝牙发出；将蓝牙接收到的数据打包后通过 SPI4 上报给 Core
- **OTA 支持**：接收上位机发送的固件包，校验后写入本地 Flash 或转发给 Core 执行升级

### 中断与事件

- SPI 中断：逐字节接收 V5F 下发的协议帧，推入解析队列
- 蓝牙连接 / 断开事件：主动以 `CMD_BT_CONN_EVT` 上报 Core
- 蓝牙数据到达事件：以 `CMD_BT_RECV_DATA` 上报 Core

## 项目目录结构

```
wireless/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块
│   │   ├── Bluetooth/              # 蓝牙协议栈配置与 GAP/GATT 服务
│   │   ├── Spi_Slave/              # SPI4 从机驱动与协议帧解析
│   │   ├── Uart/                   # UART1 Debug 输出
│   │   ├── hardware.c              # 全局调度与初始化入口
│   │   └── hardware.h              # 全局调度头文件
│   ├── Core/                       # RISC-V 内核相关文件
│   ├── Debug/                      # 调试支持文件
│   ├── Peripheral/                 # 芯片外设库
│   └── Startup/                    # 启动文件
│
├── User/                           # 用户代码（main.c / 中断 / 系统配置）
├── Ld/                             # 链接脚本
├── .cproject
├── .project
└── wireless.wvproj                 # MounRiver Studio 工程文件
```

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`spi_slave.h`
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`
- **变量**：统一小写加下划线，例如 `bt_conn_status`
- **文件夹**：下划线写法，仅首字母大写，例如 `Spi_Slave`、`Bluetooth`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范

## 开发要点

1. CH585F 为独立 MCU，V5F 仅通过 SPI4 与其通信，**不负责 CH585F 的硬件初始化**。
2. SPI4 物理层需确保 CS、CLK、MOSI、MISO 走线尽量短，避免高速通信时信号完整性问题。
3. 蓝牙 OTA 需预留足够的 Flash 空间存放临时固件包，并设计断点续传或重传机制（应用层实现）。
4. 建议维护一个轻量级命令队列，避免 SPI 中断过于频繁导致蓝牙协议栈时序抖动。
