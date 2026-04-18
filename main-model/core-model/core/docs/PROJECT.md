# 项目结构

## 概述

这是一个基于 **CH32H417** 的双核项目：
- **V5F**：主执行核心，负责主要业务逻辑与复杂外设交互。
- **V3F**：辅助核心，负责唤醒 V5F 以及串口数据转发。

驱动初始化相关任务统一在 `hardware.h / hardware.c` 中调度，核心外设包括 **CS43131**、**CH378** 等；**CH585F** 为独立 MCU，自主完成初始化。

---

## 双核职责

### V3F 核心
- 初始化八个串口。
- 扫描当前模块组成情况。
- 更新 `hardware.c` 中的全局配件状态。
- 设置**系统初始化标志位 0**。
- 后续任务：处理屏幕模块与配件、供电、键盘之间的交互。

### V5F 核心
- 作为主力执行核心，运行板上系统的主要初始化逻辑。
- 初始化 **CH378** 和 **CS43131**。
- 从 **CH378** 中读取用户自定义持久化配置，更新全局配置状态。
- 设置**系统初始化标志位 1**。
- 后续任务：处理屏幕模块与 **CH378**、**CS43131**、**CH585F** 的交互（文件读取显示、音频播放与状态同步、蓝牙信号传输 / OTA）。
  > 注：CH585F 为独立 MCU，V5F 仅负责与其通信，无需执行硬件初始化。

### 同步机制
- 两核各自独立完成初始化，但系统初始化状态标志位会进行同步。
- 标志位定义在 `hardware.c` 中作为全局变量，两核共享内存访问；为避免编译器优化导致可见性问题，需将其声明为 `volatile`。
- 两核均等待系统初始化标志位全部设置完毕后，才继续执行后续任务。
- 屏幕模块自主处理大部分任务，少数需与系统交互的任务发回 **V5F** 处理。
- 由于两核位于同一 MCU，外设可被双核同时访问；屏幕在同一时间仅与一类外设交互（例如播放音乐时由 V5F 主导），因此无需复杂的总线仲裁机制，基本不会造成阻塞。

---

## 项目目录结构

```
core/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块
│   │   ├── CH378/                  # CH378 文件管理芯片驱动
│   │   ├── CH585F/                 # CH575F 蓝牙无线芯片交互
│   │   ├── CH9350/                 # CH9350 HID转串口驱动外接HID设备
│   │   ├── CS43131/                # CS43131 音频 DAC 驱动
│   │   ├── Display/                # 屏幕模块驱动
│   │   ├── I2c_soft/               # 软件 I2C 基础驱动
│   │   ├── Key/                    # 按键基础驱动
│   │   ├── Keyboard/               # 键盘模块驱动
│   │   ├── Power/                  # 供电管理模块
│   │   ├── Submodels/              # 子模块/配件管理
│   │   ├── hardware.c              # 全局调度与双核初始化入口
│   │   └── hardware.h              # 全局调度头文件
│   ├── Core/                       # RISC-V 内核相关文件
│   ├── Debug/                      # 调试支持文件
│   ├── Peripheral/                 # 芯片外设库
│   └── Startup/                    # 启动文件
│
├── V3F/                            # V3F 核心工程
│   ├── Ld/                         # V3F 链接脚本
│   ├── User/                       # V3F 用户代码（main.c / 中断 / 系统配置）
│   ├── .cproject
│   ├── .project
│   └── core_V3F.wvproj
│
├── V5F/                            # V5F 核心工程
│   ├── Ld/                         # V5F 链接脚本
│   ├── User/                       # V5F 用户代码（main.c / 中断 / 系统配置）
│   ├── .cproject
│   ├── .project
│   └── core_V5F.wvproj
│
├── docs/                           # 项目文档与数据手册
│   ├── CH32H417DS0.pdf
│   ├── CH32H417RM.pdf
│   ├── CH378DS1.PDF
│   ├── CH585DS1.PDF
│   ├── CH9329DS1.PDF
│   ├── CS43131_DS1155F2.pdf
│   └── PROJECT.md
│
└── core.wvsln                      # MounRiver Studio / WCH 解决方案文件
```

---

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`i2c_soft.h`。
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`。
- **变量**：统一小写加下划线，例如 `system_init_flag`。
- **文件夹**：下划线写法，仅首字母大写，例如 `I2c_soft`、`Submodels`。
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范。

---

## 架构规划

需要进行 **core model** 的整体架构设计，确保：
1. 双核初始化流程清晰、可维护。
2. 公共模块（`Common/Common/`）与具体核心解耦。
3. 同步原语（标志位 / 共享内存）定义明确。
4. 屏幕模块与两核的通信接口统一。

## 通信定义
编号
仅核心和CH585F之间使用SPI
别的模块和核心交互使用串口
核心：  00     core
CH585F 01     wireless
屏幕模块10-12  display
键盘模块20-22  keyboard
供电模块30-31  power
配件模块40-49  submodels

---

## 通信协议规范（V1.0）

### 1. 协议概述

本项目采用一套统一的紧凑二进制通信协议，覆盖 **Core** 与 **Display**、**Keyboard**、**Power**、**Submodels**、**CH585F** 之间的所有数据交互。

- **物理层**：UART（115200/8-N-1），CH585F 使用 SPI 但数据载荷遵循本协议格式
- **数据链路层**：固定帧头的紧凑二进制帧，无校验和
- **通信模式**：异步中断驱动，收发双方均不阻塞等待
- **隔离协议**：CH9350 保持其独立的 `0x57 0xAB` 专用协议，不参与本统一协议

### 2. 帧结构

```
+--------+--------+--------+--------+--------+-------------+
|  HEAD  |  SRC   |  DST   |  LEN   |  CMD   |  DATA[0..N] |
| 1 byte | 1 byte | 1 byte | 1 byte | 1 byte |   N bytes   |
+--------+--------+--------+--------+--------+-------------+
```

| 字段 | 长度 | 说明 |
|------|------|------|
| **HEAD** | 1 byte | 帧头，固定值 `0xAA`。仅在接收状态机处于 `WAIT_HEAD` 状态时识别为新帧起始，数据域中出现 `0xAA` 不触发帧重解析。 |
| **SRC** | 1 byte | 源模块ID，标识发送方。 |
| **DST** | 1 byte | 目标模块ID，标识接收方。 |
| **LEN** | 1 byte | 有效载荷长度，即 `CMD + DATA` 的总字节数。`DATA` 长度 = `LEN - 1`，最大 255 字节，故最大帧长度为 260 字节。 |
| **CMD** | 1 byte | 操作码。高 4 位标识模块类型，低 4 位标识操作类型，或由各模块独立定义。 |
| **DATA** | N bytes | 数据域，长度由 `LEN` 推导，允许为 0 字节（此时 `LEN = 1`）。 |

**最大帧传输时间**（115200 波特率）：约 22.6 ms。

### 3. 模块 ID 定义

模块 ID 与硬件编号一一对应，全系统统一：

| ID | 模块 | 物理接口 | 主要交互核心 | 说明 |
|----|------|----------|-------------|------|
| `0x00` | Core（核心） | — | V3F / V5F | 两核共享同一 ID，通过 SRC 区分来源 |
| `0x01` | CH585F（无线/蓝牙） | SPI4 | V5F | 独立 MCU，V5F 通过 SPI 通信 |
| `0x10` | Display（屏幕模块） | UART4 | V5F / V3F | V5F 主导，V3F 可直接访问（双核不并发操作） |
| `0x20` | Keyboard（键盘模块） | UART3 | V3F | V3F 直接管理 |
| `0x30` | Power（供电模块） | UART5 | V3F | V3F 直接管理 |
| `0x40` | Submodel-1（配件槽1） | UART6 | V3F | V3F 直接管理 |
| `0x41` | Submodel-2（配件槽2） | UART7 | V3F | V3F 直接管理 |
| `0x42` | Submodel-3（配件槽3） | UART8 | V3F | V3F 直接管理 |

> 预留范围：Display `0x10-0x12`、Keyboard `0x20-0x22`、Power `0x30-0x31`、Submodels `0x40-0x49`。
>
> **Display 双核访问说明**：V3F 和 V5F 均可直接向 Display 发送命令，但两核不会在同一时间操作 UART4，因此无需额外的并发仲裁机制。Display 模块通过帧中的 SRC 字段（0x00）无法区分来自 V3F 还是 V5F，如需区分可在 DATA 中增加核心标识字段。

### 4. 通用操作码（0x00 ~ 0x0F）

以下操作码为所有模块共享，用于基础握手、查询和状态同步：

| 操作码 | 宏名 | 方向 | 说明 |
|--------|------|------|------|
| `0x00` | `CMD_NOP` | 双向 | 心跳/空操作，可用于保活检测。 |
| `0x01` | `CMD_GET_TYPE` | Core -> Module | 查询模块类型。Module 回复自身类型编号。 |
| `0x02` | `CMD_GET_STATUS` | Core -> Module | 查询模块当前状态。 |
| `0x03` | `CMD_SET_CONFIG` | Core -> Module | 向模块下发配置参数。 |
| `0x04` | `CMD_ACK` | 双向 | 肯定确认。DATA 可选携带响应数据。 |
| `0x05` | `CMD_NACK` | 双向 | 否定确认。DATA[0] 建议携带错误码。 |
| `0x06` | `CMD_EVT_NOTIFY` | Module -> Core | 事件主动上报，如电量变化、按键按下、蓝牙连接等。 |
| `0x07` | `CMD_DATA_STREAM` | 双向 | 纯数据流传输，用于大数据量场景（如音频、文件）。 |

### 5. 模块专用操作码

各模块在独立头文件中定义专属操作码，操作码空间由各模块自行管理，建议按模块划分范围以避免冲突。

#### 5.1 Display（屏幕模块）

| 操作码 | 宏名 | 方向 | 说明 |
|--------|------|------|------|
| `0x11` | `CMD_DISP_SET_BRIGHTNESS` | Core -> Display | 设置背光亮度。DATA[0] = 亮度值（0-100）。 |
| `0x12` | `CMD_DISP_GET_BRIGHTNESS` | Core -> Display | 获取当前背光亮度。Display 回复 ACK + 亮度值。 |
| `0x13` | `CMD_DISP_SHOW_PAGE` | Core -> Display | 切换显示页面。DATA[0] = 页面编号。 |
| `0x14` | `CMD_DISP_UPDATE_STATUS` | Core -> Display | 更新状态栏信息（电量、蓝牙、时间等）。 |
| `0x15` | `CMD_DISP_TOUCH_EVT` | Display -> Core | 触摸屏事件上报。DATA 包含坐标与事件类型。 |

#### 5.2 Keyboard（键盘模块）

| 操作码 | 宏名 | 方向 | 说明 |
|--------|------|------|------|
| `0x21` | `CMD_KBD_GET_LAYOUT` | Core -> Keyboard | 查询当前键位布局。 |
| `0x22` | `CMD_KBD_SET_BACKLIGHT` | Core -> Keyboard | 设置键盘背光。DATA[0] = 模式/亮度。 |
| `0x23` | `CMD_KBD_KEY_DOWN` | Keyboard -> Core | 按键按下事件。DATA[0..5] = 键码数组。 |
| `0x24` | `CMD_KBD_KEY_UP` | Keyboard -> Core | 按键释放事件。 |
| `0x25` | `CMD_KBD_FN_COMBO` | Keyboard -> Core | Fn 组合键事件上报。 |

#### 5.3 Power（供电模块）

| 操作码 | 宏名 | 方向 | 说明 |
|--------|------|------|------|
| `0x31` | `CMD_PWR_GET_BATTERY` | Core -> Power | 查询电池电量百分比。 |
| `0x32` | `CMD_PWR_GET_VOLTAGE` | Core -> Power | 查询电池电压（mV）。 |
| `0x33` | `CMD_PWR_SET_CHARGE` | Core -> Power | 设置充电策略。DATA[0] = 开关/电流档位。 |
| `0x34` | `CMD_PWR_BAT_LOW_EVT` | Power -> Core | 低电量告警事件。DATA[0] = 当前电量。 |
| `0x35` | `CMD_PWR_CHARGE_EVT` | Power -> Core | 充电状态变化事件（插入/拔出/充满）。 |

#### 5.4 Submodels（配件模块）

| 操作码 | 宏名 | 方向 | 说明 |
|--------|------|------|------|
| `0x41` | `CMD_SUB_GET_TYPE` | Core -> Submodel | 查询配件类型（与通用 `CMD_GET_TYPE` 语义相同，可复用）。 |
| `0x42` | `CMD_SUB_GET_DATA` | Core -> Submodel | 读取配件传感器/状态数据。 |
| `0x43` | `CMD_SUB_SET_DATA` | Core -> Submodel | 向配件写入控制数据。 |
| `0x44` | `CMD_SUB_EVT_DATA` | Submodel -> Core | 配件数据主动上报。 |

#### 5.5 CH585F（无线模块，SPI 接口）

CH585F 使用 SPI 进行物理通信，但在应用层数据包中嵌入相同的协议帧格式：

| 操作码 | 宏名 | 方向 | 说明 |
|--------|------|------|------|
| `0x51` | `CMD_BT_GET_STATUS` | Core -> CH585F | 查询蓝牙连接状态。 |
| `0x52` | `CMD_BT_SEND_DATA` | Core -> CH585F | 通过蓝牙发送数据。 |
| `0x53` | `CMD_BT_START_OTA` | Core -> CH585F | 启动蓝牙 OTA 升级流程。 |
| `0x54` | `CMD_BT_CONN_EVT` | CH585F -> Core | 蓝牙连接/断开事件。 |
| `0x55` | `CMD_BT_RECV_DATA` | CH585F -> Core | 蓝牙接收到数据上报。 |

### 6. 异步通信规则

1. **不阻塞等待**：发送方发出帧后立即返回，不等待接收方回复。所有模块和核心均通过 UART 中断接收字节，在主循环或中断中解析处理。
2. **响应帧格式**：接收方回复时，交换 `SRC` 与 `DST` 字段，`CMD` 可保持原值或使用 `CMD_ACK`/`CMD_NACK` 包裹。例如：
   - 请求：`[AA][00][30][01][01]`（Core 查询 Power 类型）
   - 响应：`[AA][30][00][02][04][03]`（Power 回复 ACK，类型为 0x03）
3. **事件上报**：模块检测到状态变化时，主动以自身为 SRC、Core 为 DST 发送 `CMD_EVT_NOTIFY` 或模块专用事件码。
4. **无重传机制**：本协议不实现软件重传，依赖物理层可靠性。如需可靠性保障，由应用层在必要时使用 `CMD_ACK` 确认。

### 7. 接收状态机

各模块 UART 中断中逐字节喂入状态机：

```
WAIT_HEAD -> WAIT_SRC -> WAIT_DST -> WAIT_LEN -> WAIT_CMD -> WAIT_DATA -> FRAME_READY
```

- `WAIT_HEAD`：等待 `0xAA`，收到后进入 `WAIT_SRC`。
- `WAIT_SRC` / `WAIT_DST`：接收模块 ID，不校验合法性（由应用层过滤）。
- `WAIT_LEN`：接收 `LEN`，计算 `DATA` 字节数 = `LEN - 1`。
- `WAIT_CMD`：接收操作码。
- `WAIT_DATA`：接收剩余 `LEN - 1` 字节数据。
- `FRAME_READY`：帧完整，推入消息队列或直接回调处理函数，状态机重置为 `WAIT_HEAD`。

### 8. 消息缓冲区建议

| 模块 | 建议接收缓冲区大小 | 说明 |
|------|-------------------|------|
| Display | 512 bytes | 可能涉及图像/字体数据传输 |
| Keyboard | 64 bytes | 键码数据量小 |
| Power | 64 bytes | 状态数据量小 |
| Submodels | 256 bytes x3 | 每路 UART 独立缓冲 |
| CH585F | 512 bytes | 蓝牙数据包可能较大 |

### 9. 通信示例

#### 示例 1：Core 查询 Display 类型
```
Host:  [AA][00][10][01][01]
       HEAD=AA SRC=00(Core) DST=10(Display) LEN=01 CMD=01(GET_TYPE)

Guest: [AA][10][00][02][04][01]
       HEAD=AA SRC=10(Display) DST=00(Core) LEN=02 CMD=04(ACK) DATA[0]=01(类型编号)
```

#### 示例 2：Power 主动上报低电量
```
Guest -> Host: [AA][30][00][02][06][15]
               HEAD=AA SRC=30(Power) DST=00(Core) LEN=02 CMD=06(EVT_NOTIFY) DATA[0]=21(电量21%)
```

#### 示例 3：Core 设置 Display 背光为 80%
```
Host:  [AA][00][10][02][11][50]
       HEAD=AA SRC=00(Core) DST=10(Display) LEN=02 CMD=11(SET_BRIGHTNESS) DATA[0]=0x50(80)

Guest: [AA][10][00][01][04]
       HEAD=AA SRC=10(Display) DST=00(Core) LEN=01 CMD=04(ACK)
```

### 10. 双核通信边界

- **V3F 侧通信**：Keyboard、Power、Submodels 的 UART 中断与协议解析运行在 V3F 上。V3F 可直接访问 UART4 与 Display 通信，无需经过 V5F 转发。
- **V5F 侧通信**：Display、CH585F、CH378、CS43131 的交互运行在 V5F 上。
- **跨核数据**：V3F 与 V5F 之间不通过 UART 传递数据，如需同步状态使用 `hardware.c` 中的 `volatile` 共享标志位或 HSEM 机制。
- **Display 并发**：V3F 和 V5F 均可直接操作 UART4，但两核不会在同一时间发送数据，Display 模块无需处理并发冲突。

### 11. 版本与扩展

- 本协议版本号为 **V1.0**，帧结构中暂不包含版本字段。
- 后续如需扩展，可在 `DATA` 首字节增加子版本号，或保留 `CMD` 的 `0x08~0x0F` 作为系统扩展码。
- 新增模块时，从预留 ID 范围中分配，并在本章节补充操作码定义。