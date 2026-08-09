# Wireless 模块（CH585F）

## 概述

本模块为独立 BLE MCU，负责核心底板与手机 APP（WCH Terminal App）之间的蓝牙连接，
当前实现 **BLE Peripheral + APP CLI 透传桥**：手机 APP 通过 BLE 连接 CH585F，
以自定义 GATT 数据通道收发 CLI 命令/响应，CH585F 经 SPI 与 Core 双向透传协议帧，
Core 将数据喂入 `CLI_Process()` 执行并把输出回传——相当于一条**蓝牙 SSH 终端**。

- **主控芯片**：CH585F（独立 RISC-V MCU，内置 **BLE 5.4 协议栈**，**不支持 Bluetooth Classic**）
- **与 Core 接口**：SPI（Core 侧 V5F = SPI4 Master / CH585F 侧 = SPI0 Slave），
  应用层载荷遵循统一协议帧格式（支持标准帧与 LEN=0xFF 流式帧）
- **模块 ID**：`0x01`
- **模块类型编号**：`0x02`，子类型 `0x01`（标准 BLE 模块）
- **广播名称**：`WCH-Terminal`
- **Debug 接口**：UART1（仅供 CH585F 自身调试输出，115200）

> **重要**：CH585F 仅支持 BLE，不支持 Bluetooth Classic，因此不支持 A2DP、SPP。
> APP 数据通道走 BLE 自定义 GATT Profile（Service `0xFFE0`，见 `protocol_app.md`）。

---

## 当前功能状态

| 功能 | 状态 | 说明 |
|------|------|------|
| SPI 双向通信（Slave + 通知） | ✅ 已实现 | CH585F 有上行数据时通过 NSS 脉冲通知 Core；Core 侧全双工轮询（NSS 触发 / 有下行待发 / 10ms 保活三者任一） |
| 统一协议帧（标准 + 流式） | ✅ 已实现 | 支持 LEN=0xFF 流式帧（尾扫描 + 回退），单帧最大约 20KB；收发缓冲各 20KB |
| BLE Peripheral（APP 连接） | ✅ 已实现 | 广播 `WCH-Terminal`，GATT Service `0xFFE0`（RX `0xFFF1` Write / TX `0xFFF2` Notify） |
| APP CLI 透传（双向） | ✅ 已实现 | 扩展码 `0x50/0x07`；APP→Core 侧重组（2KB），Core→APP 侧 Notify 分包 + 4KB 发送队列节流 |
| APP 心跳 | ✅ 已实现 | `MSG_TYPE_HEARTBEAT`（0xFF）由 CH585F 本地应答，不转发 Core |
| 状态查询 / 复位 | ✅ 已实现 | `CMD_BT_GET_STATUS`（0x51）回复 ACK + 9 字节状态；`CMD_BT_RESET`（0x59）回复空 ACK |
| Core 侧 CLI 执行闭环 | ✅ 已实现 | `ch585f_bt.c`：CLI 输出捕获（`CLI_Capture_*`）→ 分帧（SOF/EOF）回传；命令长度上限 255 |
| BT 流量统计与 Display 联动 | ✅ 已实现 | Core 记录最近 10 次上/下行字节数，变化时经 `CMD_DISP_EXT_BT_EVENT`（0x0C，STATUS/TRAFFIC）推送 Display |
| BLE Central（扫描/连接/配对 BLE 键鼠） | ❌ 未实现 | 协议已预留（0x50/0x01~0x05、0x54、0x56），固件两侧均未实现 |
| HID over GATT Host | ❌ 未实现 | 依赖 Central 角色 |
| 文件传输 | ❌ 未实现 | `protocol_app.md` 第 9 节预留 |

---

## 硬件组成

- **CH585F**：独立 RISC-V MCU，运行 BLE 5.4 协议栈（Peripheral 角色）
- **SPI（接 Core）**：
  - Core（CH32H417 V5F）侧：SPI4 Master，MOSI=PE14 / MISO=PE13 / SCK=PE12 / NSS=PE11
  - CH585F 侧：SPI0 Slave（Mode 0，8bit，MSB first），20KB RX/TX 缓冲
- **上行通知**：CH585F 有数据待读时在 NSS 线上产生通知脉冲（`SPI_Slave_NotifyMaster()`），
  Core 侧 EXTI 置位 `nss_notify` 后发起 SPI 读取；辅以 10ms 周期保活轮询防丢通知
- **UART1**：Debug 串口，115200，输出 BLE/SPI 运行日志

---

## 双 MCU 职责边界

| 职责 | Core (CH32H417 V5F) | CH585F (独立 MCU) |
|------|---------------------|-------------------|
| 物理层 | SPI4 Master，轮询/中断驱动收发 | SPI0 Slave，ISR 收发 + 20KB 环形缓冲 |
| 协议栈 | 统一协议帧打包/解析（`Protocol`），CLI 输出捕获与分帧 | 统一协议帧解析/打包（含流式帧增量回调） |
| 蓝牙协议栈 | 不运行 | BLE 5.4 Peripheral：广播、连接管理、MTU 协商、GATT Server |
| APP 帧处理 | 不感知 | APP 协议帧（5 字节头 TYPE/FLAGS/SEQ/LEN/RSV）重组与分包 |
| CLI 执行 | `CLI_Process()` 执行命令，捕获 printf 输出回传 | 纯透传，不解析 CLI 内容 |
| 状态联动 | 流量统计、APP 连接状态，推送 Display | 上报连接状态（Core 侧亦以收到 CLI 数据自动判定 APP 在线） |

---

## 通信数据流

```
手机 APP (WCH Terminal)
   │  BLE Write(0xFFF1) / Notify(0xFFF2)     ← APP 协议帧：TYPE/FLAGS/SEQ/LEN/RSV + PAYLOAD
   ▼
CH585F (Peripheral)
   │  重组完整 CLI 消息（SOF→EOF），打包为统一协议帧
   │  [AA][01][00][LEN][50][07][FLAGS][CLI数据][A5 5A FC FD]（>253B 用流式帧）
   ▼  SPI0 Slave → SPI4 Master
Core (V5F)
   │  ch585f_bt.c 组装（SOF/EOF）→ CLI_Process() 执行
   │  printf 输出经 CLI_Capture 捕获 → 分帧（≤252B/帧）回传
   ▼
CH585F → BLE Notify 分包 → APP 重组显示
```

**链路细节**：

- **上行可靠送达**：CH585F 打包完整帧入 20KB TX 缓冲并拉 NSS 通知；Core 每次 SPI 事务
  全双工交换固定长度（`CH585F_BT_POLL_SIZE`），无数据时补 0x00 dummy
- **CLI 输出捕获**：Core 执行 APP 命令期间 `CLI_Capture_Start()` 劫持 `_write`，
  输出落入捕获缓冲区，`CLI_Capture_Flush()` 取出后经 0x50/0x07 分帧回传
- **流量统计**：每次上/下行记录字节数到最近 10 次环形数组，
  经 `BT_EVT_TRAFFIC` 推送 Display 的 BT 页面
- **异常自愈**：Core 侧接收状态机卡住（半帧）连续 5 次轮询未完成即强制复位；
  CH585F 侧 SPI ISR 维护丢字节计数器供诊断

---

## 已实现的协议命令（详见 `Protocol_Wireless.md`）

| 命令 | 方向 | 说明 |
|------|------|------|
| `CMD_GET_TYPE`(0x01) | Core→CH585F | 身份查询，ACK 6 字节（类型 0x02/子类型 0x01/HW 0x01/FW 1.0/保留） |
| `CMD_BT_GET_STATUS`(0x51) | Core→CH585F | 状态查询，ACK 9 字节（状态位图/连接数/模式/MAC） |
| `CMD_BT_RESET`(0x59) | Core→CH585F | 复位应答（空 ACK） |
| `CMD_BT_EXT_CLI_DATA`(0x50/0x07) | 双向 | CLI 数据透传，SOF/EOF 分帧，支持流式帧 |

其余操作码（扫描/连接/配对/HID 等）已在协议层预留，待 Central 角色开发时启用。

---

## 项目目录结构

**CH585F 侧**（`main-model/wireless/`，MounRiver 工程）：

```
wireless/
├── APP/
│   ├── peripheral_main.c       # 主入口（TMOS 任务注册）
│   ├── peripheral.c            # BLE Peripheral + GATT 回调 + APP帧↔协议帧桥接
│   ├── spi_slave.c             # SPI0 Slave 驱动（20KB 环形缓冲、NSS 通知）
│   ├── protocol.c              # 统一协议帧解析/打包（含流式帧尾扫描回退）
│   ├── data_queue.c            # 数据队列
│   └── include/                # 对应头文件
├── Profile/                    # GATT Profile（0xFFE0 Service、设备信息服务）
├── HAL/  LIB/  RVMSIS/  StdPeriphDriver/  Startup/  Ld/
└── wireless.wvproj
```

**配套手机 APP**：`main-model/wch_terminal_app/`（Flutter 工程），
实现 CLI 终端、文件浏览、CWD 同步等，应用层协议见 `protocol_app.md`。

**Core 侧**（`main-model/core/Common/Common/CH585F/`）：

```
CH585F/
├── CH585F.c/h                  # SPI4 Master 驱动、EXTI(NSS) 中断
└── ch585f_bt.c/h               # 轮询收发、协议分发、CLI 捕获、流量统计、Display 推送
```

---

## 开发要点与风险

1. **SPI Slave 上行依赖通知 + 轮询双保险**：NSS 通知脉冲可能被屏蔽或丢失，
   Core 侧每 10ms 保底一次全双工轮询；下行有数据待发时也立即发起事务。
2. **流式帧边界**：LEN=0xFF 帧以帧尾扫描定边界，DATA 中恰好出现 `A5 5A FC FD`
   会提前截断（概率约 2.3e-10）；解析器实现带回退，截断后由下一帧 HEAD 重新同步。
3. **BLE 吞吐**：Notify 发送经 4KB 环形队列节流，APP 端按 SEQ 检测丢包；
   CLI 长输出（如 `tree`）由 Core 分帧（≤252 字节）+ APP 重组，无需缓存整段。
4. **CTRL 通道未启用**：APP 协议的 `MSG_TYPE_CTRL`（0x02）当前固件忽略，
   仅处理 CLI（0x01）与心跳（0xFF）；文件传输类型（0x03/0x04）为预留。
5. **GET_TYPE 身份长度特例**：Wireless 的 GET_TYPE ACK 数据域为 **6 字节**（LEN=7），
   不参与心跳槽位身份指纹约束（Wireless 不在 6 个 UART 心跳槽位内）。
6. **CH585F 固件独立性**：CH585F 为独立 MCU，固件维护在 `main-model/wireless/`，
   不随 Core 固件烧录；版本经 GET_TYPE 的 FW 字段上报。

---

> 最后更新：2026-08-08（按实际工程现状重写，替代原 Phase 开发计划）
