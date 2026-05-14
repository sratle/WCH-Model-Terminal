# Wireless 模块（CH585F）

## 概述

本模块为独立 BLE MCU，负责核心底板与上位机（手机 / PC）之间的蓝牙连接，同时承担 BLE HID 外设接入、移动端 APP 远程 CLI 与文件传输等功能。

- **主控芯片**：CH585F（独立 RISC-V MCU，内置 **BLE 5.4 协议栈**，**不支持 Bluetooth Classic**）
- **与 Core 接口**：SPI4（V5F Master / CH585F Slave），应用层载荷遵循统一协议帧格式
- **模块 ID**：`0x01`
- **模块类型编号**：`0x02`
- **Debug 接口**：UART1（仅供 CH585F 自身调试输出，波特率 115200）

> **重要**：
> 1. CH585F 的固件工程为**独立仓库**，不在 `core` 仓库内。本文档同时覆盖 **Core 侧（V5F）** 与 **CH585F 侧** 的开发计划与协作边界。
> 2. CH585F **仅支持 BLE**，不支持 Bluetooth Classic，因此**不支持 A2DP（蓝牙耳机音频）、SPP（传统串口透传）**。蓝牙键鼠走 **HID over GATT (HOGP)**，APP 数据通道走 **BLE 自定义 Profile / L2CAP**。

---

## 硬件组成

- **CH585F**：独立 RISC-V MCU，运行 BLE 5.4 协议栈
- **天线**：板载陶瓷天线或外接 IPEX 天线（根据具体硬件版本）
- **SPI4**：与 CH32H417（V5F）全双工通信，用于收发协议帧
  - MOSI: PE14 (AF5)
  - MISO: PE13 (AF5)
  - SCK:  PE12 (AF5)
  - NSS:  PE11 (AF5，软件控制)
- **INT 中断线（建议）**：CH585F 通过 GPIO 中断通知 V5F 有上行数据待读取（如 PE10 或其他空闲引脚）
- **UART1**：Debug 串口，波特率 115200，输出 BLE 协议栈及模块运行日志

---

## 双 MCU 职责边界

| 职责 | Core (CH32H417 V5F) | CH585F (独立 MCU) |
|------|---------------------|-------------------|
| 物理层 | SPI4 Master 驱动、GPIO 中断接收 | SPI4 Slave 驱动 |
| 协议栈 | 统一协议帧的打包 / 解析 / 转发 | 统一协议帧的解析 / 打包 |
| 蓝牙协议栈 | 不运行蓝牙协议栈 | 运行 **BLE 5.4** 协议栈（Central + Peripheral 双角色） |
| 设备扫描/连接 | 下发扫描/连接/断开指令 | 执行 BLE 扫描、发起/接受连接、维护链路 |
| 配对管理 | 查询/删除/清空配对列表指令 | 存储配对密钥（LTK）、记忆已配对设备 |
| HID 外设 | 接收 HID 报告，转发给 Display | 作为 **HID over GATT Host** 接收 BLE 键鼠输入 |
| APP CLI | 接收 CLI 数据，调用 CLI 处理，回传结果 | 提供 BLE 自定义数据通道 Profile，与 APP 透传 |
| 文件传输 | 通过 CLI 数据通道转发文件操作指令 | 经 BLE 数据通道与 APP 交换文件数据 |

---

## 软件职责与开发阶段

开发按以下 **4 个 Phase** 推进：

### Phase 1: 基础 SPI 通信（P0）

目标：建立稳定可靠的 V5F ↔ CH585F 双向通信通道。

**Core 侧任务**：
- 完善 `CH585F_Init()`：配置 SPI4 Master、GPIO、时钟
- 实现 `CH585F_Recv_Data()`：支持 SPI 全双工接收（或中断线触发接收）
- 在 `hardware.c` 中解注释 `CH585F_Init(&ch585f_g)`，纳入 V5F 初始化流程
- 实现基于 SPI 的协议状态机：复用现有 `Protocol` 模块逻辑
- 实现协议帧打包发送封装：`CH585F_SendFrame()`、`CH585F_PollEvent()`

**CH585F 侧任务**：
- 配置 SPI Slave 模式（CPOL Low、CPHA 1Edge、8bit、MSB first）
- 实现 SPI 中断接收，逐字节推入协议解析状态机
- 实现上行数据缓冲区：CH585F 收到蓝牙事件/数据时，打包为标准协议帧，等待 V5F 发起 SPI 读取
- 若硬件支持，配置 INT 中断线：有上行数据时拉低/拉高，通知 V5F

**验收标准**：
- V5F 能向 CH585F 发送 `CMD_BT_GET_STATUS`，CH585F 正确回复 ACK + 状态数据
- CH585F 能主动上报模拟事件（如 `CMD_BT_CONN_EVT`），V5F 正确解析

### Phase 2: BLE Central 核心（P1）

目标：CH585F 作为 BLE Central，完成设备扫描、连接、配对、记忆功能；Core 侧能通过指令控制这些行为。

**Core 侧任务**：
- 封装蓝牙控制 API：`BT_ScanStart()`、`BT_Connect()`、`BT_Disconnect()`、`BT_GetPairedList()`、`BT_RemovePaired()`
- 在 `hardware.c` 中维护蓝牙连接状态全局变量，供其他模块（Display、CLI）查询
- 对接 Display 模块：将扫描结果/配对列表/连接状态转换为 `CMD_DISP_EXT_BT_*` 系列命令发给 Display
- 对接 CLI：增加 `btscan`、`btconnect`、`btdisconnect`、`btlist`、`btrm` 等命令

**CH585F 侧任务**：
- 初始化 BLE 协议栈，配置为 Central + Peripheral 双角色
- 实现设备扫描：根据 Core 下发的过滤条件（HID / APP）扫描周围 BLE 设备
- 实现连接管理：支持按 MAC 地址连接指定设备，支持主动断开
- 实现配对与绑定：配对成功后，将 LTK 等密钥写入 Flash；开机自动回连已配对设备
- 实现配对列表持久化：Flash 中维护已配对设备表（MAC + 名称 + 密钥）
- 实现连接状态机：空闲 → 扫描中 → 连接中 → 已连接 → 断开

**验收标准**：
- Display 发起扫描请求 → Core 转发给 CH585F → CH585F 扫描 10 秒 → 返回设备列表 → Core 转发给 Display 显示
- 配对后的 BLE 键鼠下次开机自动连接

### Phase 3: HID 外设支持（P2）

目标：支持连接 BLE 键盘、BLE 鼠标，并将 HID 输入事件上报给 Core，由 Core 转发给 Display。

**Core 侧任务**：
- 接收 `CMD_BT_HID_REPORT`，解析键盘/鼠标报告
- 将 HID 报告转换为 Display 模块的 `CMD_DISP_INPUT_EVENT`（统一输入事件格式）
- 协调 USB HID（CH9350）与 BLE HID 的输入合并：两者均可产生输入事件，Display 无需区分来源
- 可选：在 `hardware.c` 中增加输入事件路由逻辑

**CH585F 侧任务**：
- 开启 **HID over GATT (HOGP)** Host 功能
- 接收 BLE 键鼠的 HID 报告，解析为 Boot Report 格式
- 通过 `CMD_BT_HID_REPORT` 上报给 Core
- 支持多 HID 设备并发（如同时连接键盘+鼠标）

**验收标准**：
- BLE 键盘按键 → CH585F 解析 → SPI 上报 Core → Core 转发 Display → Display 响应输入
- BLE 鼠标移动/点击同理

### Phase 4: APP CLI 与文件传输（P3）

目标：移动端 APP 可通过 BLE 连接 CH585F，远程执行 CLI 命令控制设备，并支持小文件传输。

**APP CLI**：
- **CH585F 侧**：提供 BLE 自定义数据通道 Profile（可参考 Nordic UART Service 结构，使用自定义 UUID）；与 APP 建立数据通道；将 APP 发来的数据通过 `CMD_BT_EXT_CLI_DATA` 上报 Core；将 Core 下发的 `CMD_BT_EXT_CLI_DATA` 回传给 APP
- **Core 侧**：收到 `CMD_BT_EXT_CLI_DATA` 后，将数据喂入 `CLI_Process()`；CLI 输出再通过本命令下发给 CH585F

**文件传输**：
- APP 通过 BLE 数据通道发送文件操作请求（如 `cat`、`cp`、`put`、`get` 等自定义指令）
- Core 侧的 CLI 模块扩展命令，支持通过 BLE 通道收发文件数据
- 由于 BLE 吞吐量限制（2Mbps 物理层，实际应用层约 100~200KB/s），文件传输建议限制在 1MB 以内，或采用分块传输机制

**验收标准**：
- APP 连接 BLE 后，输入 `ls` 命令，能看到 Core 返回的 CLI 输出
- APP 能向 Core 发送一个小文件（如配置文件），Core 保存到 CH378/TF 卡
- APP 能请求下载 Core 上的某个文件

---

## Core 侧（V5F）开发任务清单

1. **SPI 驱动完善**
   - [ ] 在 `CH585F.c` 中实现 `CH585F_Recv_Data()`（轮询或中断方式）
   - [ ] 若硬件有 INT 线，配置 GPIO 外部中断，中断服务函数中发起 SPI 读取
   - [ ] 若无 INT 线，实现主循环轮询机制（定期发送空操作或状态查询）

2. **协议栈适配**
   - [ ] 复用 `Common/Common/Protocol/` 的帧打包/解析逻辑，适配 SPI 物理层
   - [ ] 实现 `CH585F_SendFrame()`、`CH585F_ParseByte()` 或等效状态机
   - [ ] 维护接收缓冲区（建议 512 字节）

3. **蓝牙指令封装层**
   - [ ] 在 `CH585F/` 目录下新增 `ch585f_bt.c/.h`，封装高层 API：`BT_ScanStart()`、`BT_Connect()` 等
   - [ ] 状态回调机制：注册事件回调函数，供 `hardware.c` 或 Display 模块监听蓝牙状态变化

4. **HID 事件转发**
   - [ ] 解析 `CMD_BT_HID_REPORT`，转换为 `display_input_event_t`
   - [ ] 调用 `Display_SendInputEvent()` 或等效函数转发给 Display

5. **CLI 集成**
   - [ ] 新增 `btscan`、`btconnect`、`btdisconnect`、`btlist`、`btrm` 等 CLI 命令
   - [ ] 将 APP CLI 数据通道接入 `CLI_Process()`
   - [ ] 扩展 CLI 支持通过 BLE 通道的文件收发命令

---

## CH585F 侧（独立仓库）开发任务清单

1. **基础架构**
   - [ ] 创建独立 MRS 工程：`wireless.wvproj`
   - [ ] 配置 CH585F 时钟、GPIO、UART1 Debug
   - [ ] 实现 SPI4 Slave 驱动 + 协议帧收发状态机
   - [ ] 实现 INT 中断线通知（若硬件支持）

2. **BLE 协议栈配置**
   - [ ] 配置 BLE 协议栈（Central + Peripheral 双角色）
   - [ ] 配置广播名称、配对码（Just Works 或 Passkey Entry）
   - [ ] 从 Flash 读取持久化配置（已配对列表、工作模式）

3. **设备管理**
   - [ ] 实现扫描、过滤、设备列表缓存
   - [ ] 实现连接/断开状态机
   - [ ] 实现配对绑定与密钥持久化（LTK 存入 Flash）
   - [ ] 实现开机自动回连

4. **HID over GATT Host**
   - [ ] 作为 GAP Central，发现并连接 BLE HID 设备的 HOGP Service
   - [ ] 订阅 HID Report Characteristic 的 Notify/Indicate
   - [ ] 接收 BLE 键鼠的 HID 报告，解析为 Boot Report 格式
   - [ ] 通过 SPI 上报 Core

5. **APP 数据通道**
   - [ ] 作为 GAP Peripheral，广播自定义 Service UUID
   - [ ] 提供可读/可写的 Characteristic，与 APP 建立双向数据通道
   - [ ] 与 Core 双向透传 CLI 数据

6. **文件传输支持**
   - [ ] 在 BLE 数据通道上实现简单的分块确认机制（可选）
   - [ ] 支持 APP 通过数据通道发送/接收较大数据块

---

## 通信架构与数据流向

```
┌─────────────────────────────────────────────────────────────────┐
│                           Core (V5F)                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │  CH378   │  │ CS43131  │  │  Display │  │   CLI    │        │
│  │(文件系统)│  │(本地音频)│  │(屏幕UI)  │  │(命令行)  │        │
│  └────┬─────┘  └────┬─────┘  └────▲─────┘  └────▲─────┘        │
│       │             │             │             │               │
│       │ 文件数据    │ 音频输出    │ 输入事件转发 │ CLI数据转发    │
│       │             │             │             │               │
│  ┌────┴─────────────┴─────────────┴─────────────┴─────┐         │
│  │              CH585F 协议封装层 (V5F)                │         │
│  │  ┌─────────────────────────────────────────────┐   │         │
│  │  │  Protocol Pack / Parse (统一协议帧)          │   │         │
│  │  └─────────────────────────────────────────────┘   │         │
│  └────────────────────────┬────────────────────────────┘         │
│                           │ SPI4                                │
└───────────────────────────┼─────────────────────────────────────┘
                            │
┌───────────────────────────┼─────────────────────────────────────┐
│                      CH585F (独立 MCU)                          │
│  ┌────────────────────────┴────────────────────────────┐        │
│  │           SPI Slave + 协议帧处理                     │        │
│  └────────────────────────┬────────────────────────────┘        │
│                           │                                     │
│       ┌───────────────────┼───────────────────┐                 │
│       ▼                   ▼                   ▼                 │
│  ┌─────────┐        ┌─────────┐        ┌─────────┐             │
│  │ HID Host│        │ GATT    │        │ GAP     │             │
│  │(HOGP)   │        │ Server  │        │Peripheral│            │
│  │(BLE键鼠 │        │(APP数据 │        │(广播等待│             │
│  │ 输入)   │        │ 通道)   │        │ APP连接)│             │
│  └────┬────┘        └────┬────┘        └────┬────┘             │
│       │                  │                  │                  │
│       ▼                  ▼                  ▼                  │
│  BLE 键盘/鼠标      手机 APP (CLI)      手机 APP (文件传输)     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 项目目录结构

**Core 仓库（当前）**：

```
Common/Common/CH585F/
├── CH585F.c              # SPI4 驱动 + 基础收发
├── CH585F.h              # GPIO 定义、结构体、函数声明
├── ch585f_bt.c           # 【待创建】蓝牙指令封装层、状态机
└── ch585f_bt.h           # 【待创建】高层 API（扫描/连接/HID/CLI）
```

**CH585F 独立仓库（规划中）**：

```
wireless/
├── Common/
│   ├── Common/
│   │   ├── Bluetooth/    # BLE 协议栈配置、GAP/GATT/HOGP/自定义 Profile
│   │   ├── Spi_Slave/    # SPI4 Slave 驱动与协议帧解析
│   │   ├── Uart/         # UART1 Debug 输出
│   │   ├── hardware.c    # 全局调度与初始化入口
│   │   └── hardware.h
│   ├── Core/             # RISC-V 内核相关文件
│   ├── Debug/
│   ├── Peripheral/       # CH585F 外设库
│   └── Startup/
├── User/                 # main.c / 中断 / 系统配置
├── Ld/                   # 链接脚本
└── wireless.wvproj       # MounRiver Studio 工程文件
```

---

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`spi_slave.h`、`ch585f_bt.c`
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`、`SPI_Slave_IRQHandler()`、`BT_StartScan()`
- **变量**：统一小写加下划线，例如 `bt_conn_status`、`spi_rx_buf`
- **结构体类型名**：小写加下划线 + `_t`，例如 `bt_device_t`
- **文件夹**：下划线写法，仅首字母大写，例如 `Spi_Slave`、`Bluetooth`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范

---

## 开发要点与风险

1. **SPI Slave 上行数据问题**
   - CH585F 作为 Slave 无法主动发起 SPI 传输。**强烈建议在硬件上预留 INT 中断线**（CH585F GPIO → V5F GPIO），否则 V5F 必须高频轮询，增加 CPU 负载。
   - 若当前 PCB 未引出 INT 线，Phase 1 应先采用**轮询方案**（V5F 每 10~20ms 发起一次 SPI 事务读取上行数据），后续硬件改版再改为中断方案。

2. **BLE 吞吐量限制**
   - CH585F 的 BLE 物理层为 **2Mbps**，但实际应用层吞吐量受协议开销、连接间隔、MTU 大小限制，通常在 **100~200KB/s** 左右。
   - APP CLI 数据通道完全够用，但**大文件传输（>1MB）会比较慢**，需要设计分块传输+确认机制，或限制单次传输文件大小。

3. **HID 输入合并**
   - 设备同时支持 USB HID（CH9350）和 BLE HID（CH585F）。两者的输入事件最终都转换为 Display 的统一输入事件格式。Display 无需区分来源，但 Core 侧应避免重复注入或冲突。

4. **CH585F 独立性与版本对齐**
   - CH585F 固件版本号应在 `CMD_GET_TYPE` 响应中上报，Core 侧可在 CLI 中增加 `btver` 命令查询。
   - CH585F 为独立 MCU，其固件升级**不在本 core 仓库管理**，需单独维护 wireless 仓库的版本发布流程。

5. **配对记忆与安全性**
   - 配对密钥（LTK）存储在 CH585F 本地 Flash 中，Core 侧不直接管理密钥。清除配对列表通过 `CMD_BT_EXT_PAIRING_MGMT` 指令下发。
   - 建议设置最大配对数（如 8 个设备），超出时按 LRU 淘汰。
   - BLE 配对建议使用 **Just Works**（仅模式 1，无 MITM）或 **Passkey Entry**（模式 2，有用户确认），具体取决于安全需求与交互便利性。

---

## 待办清单（TODO）

### Core 侧（本仓库）

- [ ] **P0** 解注释 `hardware.c` 中的 `CH585F_Init()`，纳入 V5F 初始化流程
- [ ] **P0** 实现 SPI 接收函数（轮询或中断方式），完成双向通信验证
- [ ] **P0** 创建 `ch585f_bt.c/.h`，封装协议帧打包/解析 + 状态机
- [ ] **P1** 实现蓝牙扫描/连接/配对管理的高层 API
- [ ] **P1** 对接 Display 模块：蓝牙设备列表、连接状态同步
- [ ] **P1** 新增 CLI 蓝牙相关命令（`btscan`、`btconnect`、`btlist` 等）
- [ ] **P2** 实现 HID 报告接收与 Display 统一输入事件转发
- [ ] **P3** 对接 CLI 数据通道，支持 APP 远程命令
- [ ] **P3** 扩展 CLI 支持通过 BLE 通道的文件收发

### CH585F 侧（独立仓库）

- [ ] **P0** 搭建独立 MRS 工程，配置 SPI Slave + UART1 Debug
- [ ] **P0** 实现统一协议帧的接收/解析/发送状态机
- [ ] **P0** 完成与 Core 的协议握手验证（`CMD_BT_GET_STATUS` 往返测试）
- [ ] **P1** 配置 BLE 协议栈，实现 Central 扫描/连接/配对
- [ ] **P1** 实现配对列表持久化（Flash 读写 LTK）
- [ ] **P1** 实现开机自动回连
- [ ] **P2** 实现 HID over GATT Host，接收 BLE 键鼠输入并上报
- [ ] **P3** 实现 GATT Server 自定义数据通道，支持 APP CLI 透传
- [ ] **P3** 实现 BLE 数据通道上的分块文件传输支持

---

> 最后更新：2026-05-04
