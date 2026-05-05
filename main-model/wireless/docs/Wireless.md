# Wireless 模块（CH585F）

## 概述

本模块为独立蓝牙 MCU，负责核心底板与上位机（手机 / PC）之间的蓝牙连接，同时承担蓝牙 HID 外设接入、蓝牙耳机音频转发、配对记忆以及移动端 APP 远程 CLI 等功能。

- **主控芯片**：CH585F（独立 RISC-V MCU，内置 BLE / Classic 协议栈）
- **与 Core 接口**：SPI4（V5F Master / CH585F Slave），应用层载荷遵循统一协议帧格式
- **模块 ID**：`0x01`
- **模块类型编号**：`0x02`
- **Debug 接口**：UART1（仅供 CH585F 自身调试输出，波特率 115200）

> **重要**：CH585F 的固件工程为**独立仓库**，不在 `core` 仓库内。本文档同时覆盖 **Core 侧（V5F）** 与 **CH585F 侧** 的开发计划与协作边界。

---

## 硬件组成

- **CH585F**：独立 RISC-V MCU，运行蓝牙协议栈（BLE + Classic）
- **天线**：板载陶瓷天线或外接 IPEX 天线（根据具体硬件版本）
- **SPI4**：与 CH32H417（V5F）全双工通信，用于收发协议帧
  - MOSI: PE14 (AF5)
  - MISO: PE13 (AF5)
  - SCK:  PE12 (AF5)
  - NSS:  PE11 (AF5，软件控制)
- **INT 中断线（建议）**：CH585F 通过 GPIO 中断通知 V5F 有上行数据待读取（如 PE10 或其他空闲引脚）
- **UART1**：Debug 串口，波特率 115200，输出蓝牙协议栈及模块运行日志

---

## 双 MCU 职责边界

| 职责 | Core (CH32H417 V5F) | CH585F (独立 MCU) |
|------|---------------------|-------------------|
| 物理层 | SPI4 Master 驱动、GPIO 中断接收 | SPI4 Slave 驱动 |
| 协议栈 | 统一协议帧的打包 / 解析 / 转发 | 统一协议帧的解析 / 打包 |
| 蓝牙协议栈 | 不运行蓝牙协议栈 | 运行 BLE + Classic 主机/从机协议栈 |
| 设备扫描/连接 | 下发扫描/连接/断开指令 | 执行扫描、发起/接受连接、维护链路 |
| 配对管理 | 查询/删除/清空配对列表指令 | 存储配对密钥、记忆已配对设备 |
| HID 外设 | 接收 HID 报告，转发给 Display | 作为 HID Host 接收蓝牙键鼠输入 |
| 音频 | 从 CH378 读取音频文件，编码后通过 SPI 发送 | 接收音频流，通过 A2DP 转发给蓝牙耳机 |
| APP CLI | 接收 CLI 数据，调用 CLI 处理，回传结果 | 提供蓝牙 SPP / BLE UART 服务，与 APP 透传 |
| OTA | 读取 TF 卡固件包，通过 SPI 分批传输 | 接收固件包、校验、写入本地 Flash、自升级 |

---

## 软件职责与开发阶段

开发按以下 5 个 Phase 推进，**蓝牙 OTA 排在最后（P4）**，其余优先级为：基础通信（P0）→ 蓝牙主机核心（P1）→ HID 外设（P2）→ 蓝牙耳机音频（P3）。

### Phase 1: 基础 SPI 通信（P0）

目标：建立稳定可靠的 V5F ↔ CH585F 双向通信通道。

**Core 侧任务**：
- 完善 `CH585F_Init()`：配置 SPI4 Master、GPIO、时钟
- 实现 `CH585F_Recv_Data()`：支持 SPI 全双工接收（或中断线触发接收）
- 在 `hardware.c` 中解注释 `CH585F_Init(&ch585f_g)`，纳入 V5F 初始化流程
- 实现基于 SPI 的协议状态机：`Protocol_SPI_ParseByte()` 或等效机制，复用现有 `Protocol` 模块逻辑
- 实现协议帧打包发送封装：`CH585F_SendFrame()`、`CH585F_PollEvent()`

**CH585F 侧任务**：
- 配置 SPI Slave 模式（CPOL Low、CPHA 1Edge、8bit、MSB first）
- 实现 SPI 中断接收，逐字节推入协议解析状态机
- 实现上行数据缓冲区：CH585F 收到蓝牙事件/数据时，打包为标准协议帧，等待 V5F 发起 SPI 读取
- 若硬件支持，配置 INT 中断线：有上行数据时拉低/拉高，通知 V5F

**验收标准**：
- V5F 能向 CH585F 发送 `CMD_BT_GET_STATUS`，CH585F 正确回复 ACK + 状态数据
- CH585F 能主动上报模拟事件（如 `CMD_BT_CONN_EVT`），V5F 正确解析

### Phase 2: 蓝牙主机核心（P1）

目标：CH585F 作为蓝牙主机，完成设备扫描、连接、配对、记忆功能；Core 侧能通过指令控制这些行为。

**Core 侧任务**：
- 封装蓝牙控制 API：`BT_ScanStart()`、`BT_Connect()`、`BT_Disconnect()`、`BT_GetPairedList()`、`BT_RemovePaired()`
- 在 `hardware.c` 中维护蓝牙连接状态全局变量，供其他模块（Display、CLI）查询
- 对接 Display 模块：将扫描结果/配对列表/连接状态转换为 `CMD_DISP_EXT_BT_*` 系列命令发给 Display
- 对接 CLI：增加 `btscan`、`btconnect`、`btdisconnect`、`btlist`、`btrm` 等命令

**CH585F 侧任务**：
- 初始化蓝牙协议栈为主机模式（支持 Classic + BLE）
- 实现设备扫描：根据 Core 下发的过滤条件（HID / 音频 / 手机）扫描周围设备
- 实现连接管理：支持按 MAC 地址连接指定设备，支持主动断开
- 实现配对与绑定：配对成功后，将密钥写入 Flash；开机自动回连已配对设备
- 实现配对列表持久化：Flash 中维护已配对设备表（MAC + 名称 + 密钥）
- 实现连接状态机：空闲 → 扫描中 → 连接中 → 已连接 → 断开

**验收标准**：
- Display 发起扫描请求 → Core 转发给 CH585F → CH585F 扫描 10 秒 → 返回设备列表 → Core 转发给 Display 显示
- 配对后的蓝牙键盘/耳机下次开机自动连接

### Phase 3: HID 外设支持（P2）

目标：支持连接蓝牙键盘、蓝牙鼠标，并将 HID 输入事件上报给 Core，由 Core 转发给 Display。

**Core 侧任务**：
- 接收 `CMD_BT_HID_REPORT`，解析键盘/鼠标报告
- 将 HID 报告转换为 Display 模块的 `CMD_DISP_INPUT_EVENT`（统一输入事件格式）
- 协调 USB HID（CH9350）与蓝牙 HID 的输入合并：两者均可产生输入事件，Display 无需区分来源
- 可选：在 `hardware.c` 中增加输入事件路由逻辑

**CH585F 侧任务**：
- 开启 HID Host 功能（Classic 的 HID 或 BLE 的 HID over GATT）
- 接收蓝牙键鼠的 HID 报告，解析为 Boot Report 格式
- 通过 `CMD_BT_HID_REPORT` 上报给 Core
- 支持多 HID 设备并发（如同时连接键盘+鼠标）

**验收标准**：
- 蓝牙键盘按键 → CH585F 解析 → SPI 上报 Core → Core 转发 Display → Display 响应输入
- 蓝牙鼠标移动/点击同理

### Phase 4: 蓝牙耳机音频（P3）

目标：Core 从 CH378/TF 卡读取音频文件（WAV），通过 SPI 将音频流传输给 CH585F，CH585F 通过 A2DP 转发给蓝牙耳机播放。

**Core 侧任务**：
- 音频播放流程集成：在 CS43131 音频播放逻辑中，增加"蓝牙音频输出"分支
- 当用户选择"蓝牙耳机输出"时，Core 读取 WAV 文件（PCM 数据），通过 `CMD_BT_EXT_AUDIO_STREAM` 分批发送给 CH585F
- 维护音频播放状态机，接收 `CMD_BT_AUDIO_STATUS` 同步耳机端进度
- 支持播放控制指令：`CMD_BT_EXT_AUDIO_CTRL`（播放/暂停/停止/音量）
- 音频数据缓冲管理：确保 SPI 传输不导致耳机断流（建议维持 2~4KB 发送缓冲）

**CH585F 侧任务**：
- 开启 A2DP Source 功能
- 接收 `CMD_BT_EXT_AUDIO_STREAM` 的 PCM 数据，送入 A2DP 编码缓冲区（SBC 编码）
- 向蓝牙耳机发送编码后的音频数据
- 处理耳机端的播放控制（暂停/继续/音量），转换为 `CMD_BT_AUDIO_STATUS` 上报 Core
- 管理音频环形缓冲区，防止下溢/上溢

**验收标准**：
- Core 播放 WAV 文件，音频通过 SPI → CH585F → 蓝牙耳机正常播放
- 蓝牙耳机上的播放/暂停/音量调节能同步回 Core（通过 `CMD_BT_AUDIO_STATUS`）

### Phase 5: APP CLI 与蓝牙 OTA（P4）

目标：移动端 APP 可通过蓝牙连接 CH585F，远程执行 CLI 命令控制设备；支持通过蓝牙对 CH585F 进行 OTA 升级。

**APP CLI（P4-1）**：
- **CH585F 侧**：提供蓝牙 SPP（Classic）或 BLE UART 服务，与 APP 建立数据通道；将 APP 发来的数据通过 `CMD_BT_EXT_CLI_DATA` 上报 Core；将 Core 下发的 `CMD_BT_EXT_CLI_DATA` 回传给 APP
- **Core 侧**：收到 `CMD_BT_EXT_CLI_DATA` 后，将数据喂入 `CLI_Process()`；CLI 输出通过本命令下发给 CH585F

**蓝牙 OTA（P4-2，优先级最后）**：
- **Core 侧**：
  - 实现 `btota` CLI 命令：从 CH378/TF 卡读取 CH585F 固件包（.bin）
  - 发送 `CMD_BT_START_OTA` 启动 OTA
  - 通过 `CMD_BT_EXT_OTA_PACKET` 分批传输固件（头信息 → 数据包 → 结束标记）
  - 显示 OTA 进度（通过串口 Debug 或 Display）
- **CH585F 侧**：
  - 接收 OTA 启动命令，进入 OTA 模式（暂停正常业务）
  - 接收固件包，写入 Flash 临时区域
  - 收完后校验 CRC32，校验通过则更新启动向量，复位自升级
  - 升级结果通过事件上报 Core

**验收标准**：
- APP 连接蓝牙后，输入 `ls` 命令，能看到 Core 返回的 CLI 输出
- OTA 传输完整固件包后，CH585F 自动重启并运行新版本

---

## Core 侧（V5F）开发任务清单

1. **SPI 驱动完善**
   - [ ] 在 `CH585F.c` 中实现 `CH585F_Recv_Data()`（轮询或中断方式）
   - [ ] 若硬件有 INT 线，配置 GPIO 外部中断，中断服务函数中发起 SPI 读取
   - [ ] 若无线 INT 线，实现主循环轮询机制（定期发送空操作或状态查询）

2. **协议栈适配**
   - [ ] 复用 `Common/Common/Protocol/` 的帧打包/解析逻辑，适配 SPI 物理层
   - [ ] 实现 `CH585F_SendFrame()`、`CH585F_ParseByte()` 或等效状态机
   - [ ] 维护接收缓冲区（建议 512 字节）

3. **蓝牙指令封装层**
   - [ ] 在 `CH585F/` 目录下新增 `ch585f_bt.c/.h`，封装高层 API：`BT_ScanStart()`、`BT_Connect()`、`BT_SendAudioStream()` 等
   - [ ] 状态回调机制：注册事件回调函数，供 `hardware.c` 或 Display 模块监听蓝牙状态变化

4. **HID 事件转发**
   - [ ] 解析 `CMD_BT_HID_REPORT`，转换为 `display_input_event_t`
   - [ ] 调用 `Display_SendInputEvent()` 或等效函数转发给 Display

5. **音频通路对接**
   - [ ] 在 CS43131 音频播放逻辑中增加蓝牙输出分支
   - [ ] 实现音频数据分包发送（每包 256~512 字节 PCM）
   - [ ] 同步耳机播放状态到 UI

6. **CLI 集成**
   - [ ] 新增 `btscan`、`btconnect`、`btdisconnect`、`btlist`、`btrm`、`btota` 等 CLI 命令
   - [ ] 将 APP CLI 数据通道接入 `CLI_Process()`

---

## CH585F 侧（独立仓库）开发任务清单

1. **基础架构**
   - [ ] 创建独立 MRS 工程：`wireless.wvproj`
   - [ ] 配置 CH585F 时钟、GPIO、UART1 Debug
   - [ ] 实现 SPI4 Slave 驱动 + 协议帧收发状态机
   - [ ] 实现 INT 中断线通知（若硬件支持）

2. **蓝牙协议栈配置**
   - [ ] 配置蓝牙为主机+从机双模（或根据 Phase 逐步开启）
   - [ ] 配置广播名称、配对码
   - [ ] 从 Flash 读取持久化配置（已配对列表、工作模式）

3. **设备管理**
   - [ ] 实现扫描、过滤、设备列表缓存
   - [ ] 实现连接/断开状态机
   - [ ] 实现配对绑定与密钥持久化
   - [ ] 实现开机自动回连

4. **HID Host**
   - [ ] 连接蓝牙键盘/鼠标，接收 HID 报告
   - [ ] 解析 Boot Report 格式，通过 SPI 上报 Core

5. **A2DP Source**
   - [ ] 连接蓝牙耳机，建立 A2DP 链路
   - [ ] 接收 SPI 音频流，SBC 编码后发送
   - [ ] 处理耳机播放控制指令，上报状态

6. **APP 服务**
   - [ ] 提供 SPP / BLE UART Profile
   - [ ] 与 Core 双向透传 CLI 数据

7. **OTA**
   - [ ] Flash 分区规划：保留当前固件区 + OTA 临时区
   - [ ] 接收固件包、校验、写入临时区
   - [ ] 升级完成后切换启动向量并复位

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
│       │ WAV文件     │ 音频输出分支 │ 输入事件转发 │ CLI数据转发    │
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
│  │ HID Host│        │A2DP Src │        │SPP/BLE  │             │
│  │(键鼠输入│        │(耳机音频│        │(APP CLI)│             │
│  │ 上报)   │        │ 转发)   │        │ 透传)   │             │
│  └────┬────┘        └────┬────┘        └────┬────┘             │
│       │                  │                  │                  │
│       ▼                  ▼                  ▼                  │
│  蓝牙键盘/鼠标      蓝牙耳机/音箱        手机 APP                │
└─────────────────────────────────────────────────────────────────┘
```

---

## 项目目录结构

**Core 仓库**：

```
Common/Common/CH585F/
├── CH585F.c              # SPI4 驱动 + 基础收发
├── CH585F.h              # GPIO 定义、结构体、函数声明
├── ch585f_bt.c           # 【待创建】蓝牙指令封装层、状态机
└── ch585f_bt.h           # 【待创建】高层 API（扫描/连接/音频/HID）
```

**CH585F 独立仓库**：

```
wireless/
├── Common/
│   ├── Common/
│   │   ├── Bluetooth/    # 蓝牙协议栈配置、GAP/GATT/SPP/A2DP/HID
│   │   ├── Spi_Slave/    # SPI4 Slave 驱动与协议帧解析
│   │   ├── Uart/         # UART1 Debug 输出
│   │   ├── Audio/        # 音频缓冲、SBC 编码
│   │   ├── OTA/          # OTA 接收与 Flash 写入
│   │   ├── hardware.c    # 全局调度与初始化入口
│   │   └── hardware.h
│   ├── Core/             # RISC-V 内核相关文件
│   ├── Debug/
│   ├── Peripheral/       # CH585F 外设库
│   └── Startup/
├── User/                 # main.c / 中断 / 系统配置
├── Ld/                   # 链接脚本（需规划 OTA 临时区）
└── wireless.wvproj       # MounRiver Studio 工程文件
```

---

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`spi_slave.h`、`ch585f_bt.c`
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`、`SPI_Slave_IRQHandler()`、`BT_StartScan()`
- **变量**：统一小写加下划线，例如 `bt_conn_status`、`spi_rx_buf`
- **结构体类型名**：小写加下划线 + `_t`，例如 `bt_device_t`、`audio_stream_ctx_t`
- **文件夹**：下划线写法，仅首字母大写，例如 `Spi_Slave`、`Bluetooth`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范

---

## 开发要点与风险

1. **SPI Slave 上行数据问题**
   - CH585F 作为 Slave 无法主动发起 SPI 传输。**强烈建议在硬件上预留 INT 中断线**（CH585F GPIO → V5F GPIO），否则 V5F 必须高频轮询，增加 CPU 负载和蓝牙协议栈时序风险。
   - 若当前 PCB 未引出 INT 线，Phase 1 应先采用**轮询方案**（V5F 每 10~20ms 发起一次 SPI 事务读取上行数据），后续硬件改版再改为中断方案。

2. **SPI 总线独占**
   - SPI4 当前仅连接 CH585F，无需与其他设备分时复用。但 CS 信号需由软件控制（`SPI_NSS_Soft`），每次收发前拉低 NSS，收发完成后拉高。

3. **音频流实时性**
   - 44.1kHz 16bit 双声道 PCM 数据率约 176KB/s。SPI 波特率需远高于此（建议 ≥ 1MHz），且 CH585F 侧需维护足够大的环形缓冲区（≥ 4KB），以吸收 SPI 传输抖动。
   - 若 CH585F 的 A2DP 编码需要固定帧大小（如 SBC 每帧 128 字节），Core 侧需按编码要求对齐音频数据包。

4. **HID 输入合并**
   - 设备同时支持 USB HID（CH9350）和蓝牙 HID（CH585F）。两者的输入事件最终都转换为 Display 的统一输入事件格式。Display 无需区分来源，但 Core 侧应避免重复注入或冲突。

5. **CH585F 独立性与版本对齐**
   - CH585F 固件版本号应在 `CMD_GET_TYPE` 响应中上报，Core 侧可在 CLI 中增加 `btver` 命令查询。
   - OTA 时应先校验版本号，避免重复升级或降级冲突。

6. **配对记忆与安全性**
   - 配对密钥存储在 CH585F 本地 Flash 中，Core 侧不直接管理密钥。清除配对列表通过 `CMD_BT_EXT_PAIRING_MGMT` 指令下发。
   - 建议设置最大配对数（如 8 个设备），超出时按 LRU 淘汰。

---

## 待办清单（TODO）

### Core 侧

- [ ] **P0** 解注释 `hardware.c` 中的 `CH585F_Init()`，纳入 V5F 初始化流程
- [ ] **P0** 实现 SPI 接收函数（轮询或中断方式），完成双向通信验证
- [ ] **P0** 创建 `ch585f_bt.c/.h`，封装协议帧打包/解析 + 状态机
- [ ] **P1** 实现蓝牙扫描/连接/配对管理的高层 API
- [ ] **P1** 对接 Display 模块：蓝牙设备列表、连接状态同步
- [ ] **P1** 新增 CLI 蓝牙相关命令（`btscan`、`btconnect`、`btlist` 等）
- [ ] **P2** 实现 HID 报告接收与 Display 统一输入事件转发
- [ ] **P3** 实现音频流分包发送，对接 CS43131 播放分支
- [ ] **P3** 接收并处理 `CMD_BT_AUDIO_STATUS`，同步耳机播放进度
- [ ] **P4** 对接 CLI 数据通道，支持 APP 远程命令
- [ ] **P4** 实现 OTA 固件读取与分包传输（`btota` CLI 命令）

### CH585F 侧

- [ ] **P0** 搭建独立 MRS 工程，配置 SPI Slave + UART1 Debug
- [ ] **P0** 实现统一协议帧的接收/解析/发送状态机
- [ ] **P0** 完成与 Core 的协议握手验证（`CMD_BT_GET_STATUS` 往返测试）
- [ ] **P1** 配置蓝牙协议栈，实现主机模式扫描/连接/配对
- [ ] **P1** 实现配对列表持久化（Flash 读写）
- [ ] **P1** 实现开机自动回连
- [ ] **P2** 实现 HID Host，接收蓝牙键鼠输入并上报
- [ ] **P3** 实现 A2DP Source，接收 SPI 音频流并转发给蓝牙耳机
- [ ] **P3** 实现耳机播放控制与状态上报
- [ ] **P4** 实现 SPP / BLE UART APP 服务
- [ ] **P4** 实现 OTA 固件接收、校验、Flash 写入、自升级

---

> 最后更新：2026-05-04
