# Wireless — APP 蓝牙 CLI 通信协议规范

> 本文档规范手机 APP 通过 BLE 与 CH585F（Wireless 模块）之间的应用层通信格式。  
> **通信链条**：`APP` ↔ `CH585F` ↔ `Core` ↔ `CLI 模块`，链条较长，协议设计以**简单、明确、可调试**为首要原则。

---

## 1. 概述

### 1.1 角色定义

| 角色 | 设备 | GAP 角色 | 职责 |
|------|------|----------|------|
| APP Host | 手机 / PC | Central | 发起 BLE 连接，发送 CLI 命令，接收响应 |
| Wireless | CH585F | Peripheral | 提供 BLE 数据通道，透传 APP ↔ Core 之间的 CLI 数据 |
| Core | CH32H417 (V5F) | — | 执行 CLI 命令，返回输出结果 |

### 1.2 通信模式

APP 仅支持 **CLI 模式**（Phase 3 核心功能）：
- APP 发送的每一帧载荷均为 UTF-8 编码的文本数据（CLI 命令字符串）
- APP 接收的每一帧载荷均为 UTF-8 编码的 CLI 执行结果或事件通知
- CH585F **不对 CLI 内容进行任何解析**，纯透传
- Core 侧的 `CLI_Process()` 负责命令解析与执行

> **文件传输**作为 CLI 的扩展命令在协议层预留（见第 8 节），P3 阶段实现。

### 1.3 设计约束

| 约束项 | 说明 | 协议层应对 |
|--------|------|-----------|
| BLE MTU 限制 | 默认 ATT_MTU=23，协商后最大约 247 | 应用层自带分包/重组机制，不依赖底层分片 |
| Notify 无确认 | GATT Notify 无应用层 ACK | 关键控制命令使用 Write（带响应），数据流依赖帧序号检测丢包 |
| 长输出 | `ls -R`、`cat 大文件` 可能产生数 KB 输出 | 支持多帧连续传输，APP 负责按 EOF 拼接 |
| 并发 | 可能同时存在 HID 设备连接 | CLI 数据通道与 HID 通道完全独立，互不影响 |

---

## 2. GATT Profile 定义

CH585F 作为 Peripheral 广播并暴露以下 Service。APP 作为 Central 连接后发现该 Service，通过 Write 下发数据，通过 Notify 接收数据。

### 2.1 Service & Characteristic

```
┌─────────────────────────────────────────────────────────────┐
│  Service: WCH Wireless CLI Service                          │
│  UUID Type: 16-bit                                          │
│  UUID: 0xFFF0                                               │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Characteristic: CLI_RX                              │   │
│  │ UUID: 0xFFF1                                        │   │
│  │ Properties: Write, Write Without Response           │   │
│  │ Permissions: Writeable                              │   │
│  │ Description: APP → CH585F 下行数据通道              │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Characteristic: CLI_TX                              │   │
│  │ UUID: 0xFFF2                                        │   │
│  │ Properties: Notify                                  │   │
│  │ Permissions: None (通过 CCCD 使能 Notify)            │   │
│  │ Descriptors: Client Characteristic Configuration    │   │
│  │ Description: CH585F → APP 上行数据通道              │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 UUID 速查

| 名称 | UUID | 方向 | 属性 |
|------|------|------|------|
| CLI Service | `0xFFF0` | — | Primary Service |
| CLI_RX | `0xFFF1` | APP → CH585F | Write / Write Without Response |
| CLI_TX | `0xFFF2` | CH585F → APP | Notify |

> **兼容性说明**：若希望兼容通用 BLE 串口调试工具（如 nRF Connect、LightBlue），可将 Service UUID 替换为 Nordic UART Service (`0xFFE0`)，RX=`0xFFE1`，TX=`0xFFE2`。本协议的应用层帧格式与 UUID 选择无关。

### 2.3 连接参数建议

| 参数 | 建议值 | 说明 |
|------|--------|------|
| Connection Interval | 15~30 ms | 平衡功耗与响应速度 |
| Slave Latency | 0 | CLI 交互需要低延迟 |
| Supervision Timeout | 3000 ms | 合理范围 |
| ATT_MTU | 协商至 185 或 247 | 减少分包数量 |

---

## 3. 应用层帧格式

BLE 底层每次 Write/Notify 传输的 Payload 即为一**应用层帧**。当单条消息超过单帧容量时，采用**显式分包**机制。

### 3.1 帧结构

```
+--------+--------+--------+--------+--------+-------------+
|  TYPE  | FLAGS  |  SEQ   |  LEN   |  RSV   |  PAYLOAD[]  |
| 1 byte | 1 byte | 1 byte | 1 byte | 1 byte |  N bytes    |
+--------+--------+--------+--------+--------+-------------+

总帧长 = 5 + N 字节
最大 PAYLOAD 长度 N_max = 单包 BLE 载荷上限 - 5
```

> 例如：ATT_MTU=247 时，L2CAP 载荷=244，ATT 头=3，**最大 PAYLOAD ≈ 236 字节**；  
> ATT_MTU=23 时，**最大 PAYLOAD ≈ 18 字节**。

### 3.2 字段详解

#### TYPE — 消息类型（1 byte）

| 值 | 宏名 | 说明 |
|----|------|------|
| `0x01` | `MSG_TYPE_CLI_DATA` | CLI 数据（命令或响应），**最常用** |
| `0x02` | `MSG_TYPE_CTRL` | 控制命令/响应（见 3.3 节） |
| `0x03` | `MSG_TYPE_FILE_META` | 文件传输元数据（P3 预留） |
| `0x04` | `MSG_TYPE_FILE_CHUNK` | 文件传输分块数据（P3 预留） |
| `0x05` | `MSG_TYPE_ACK` | 应用层确认帧（用于文件传输或可靠模式） |
| `0xFF` | `MSG_TYPE_HEARTBEAT` | 心跳/保活 |
| `0x00`, `0x06~0xFE` | — | 预留 |

#### FLAGS — 标志位（1 byte）

```
bit0: SOF (Start of Frame)  — 1 = 本条消息的首包
bit1: EOF (End of Frame)    — 1 = 本条消息的尾包
bit2: DIR                   — 0 = 下行 (APP→Core), 1 = 上行 (Core→APP)
bit3: ACK_REQ               — 1 = 发送方请求接收方回复 ACK（仅 TYPE=0x03/0x04 时有效）
bit4: ERR                   — 1 = 本帧携带错误信息（TYPE=0x02 或异常响应时使用）
bit5~7: 预留，必须填 0
```

**FLAGS 常见组合**：

| FLAGS 值 | SOF | EOF | DIR | 含义 |
|----------|-----|-----|-----|------|
| `0x01` | 1 | 0 | 0 | 下行消息首包（命令开始） |
| `0x02` | 0 | 1 | 0 | 下行消息尾包（命令结束） |
| `0x03` | 1 | 1 | 0 | 单包完整下行消息 |
| `0x05` | 1 | 0 | 1 | 上行消息首包（响应开始） |
| `0x06` | 0 | 1 | 1 | 上行消息尾包（响应结束） |
| `0x07` | 1 | 1 | 1 | 单包完整上行消息 |

#### SEQ — 帧序号（1 byte）

- 范围 `0x00 ~ 0xFF`，循环递增
- **同一条消息内的各包 SEQ 必须连续**（首包 SEQ=n，第二包 SEQ=n+1，以此类推）
- 新消息的首包 SEQ 在上一条消息的尾包 SEQ 基础上 +1（模 256）
- 接收方通过 SEQ 连续性检测丢包

#### LEN — PAYLOAD 长度（1 byte）

- 表示本帧 `PAYLOAD[]` 的实际字节数
- 范围 `0 ~ N_max`，不允许超过当前 MTU 下的最大 Payload

#### RSV — 预留（1 byte）

- 当前版本固定为 `0x00`
- 未来可用于扩展（如消息 ID 高字节、通道号等）

#### PAYLOAD — 数据载荷（N bytes）

- 内容和编码由 `TYPE` 决定
- 对于 `MSG_TYPE_CLI_DATA`，PAYLOAD 为 **UTF-8 编码的文本片段**
- **不含 `\0` 终止符**，长度严格由 `LEN` 字段指定

---

## 4. 消息类型详解

### 4.1 MSG_TYPE_CLI_DATA (0x01)

CLI 数据通道，承载实际的命令字符串和响应字符串。

#### 下行：APP → Core（DIR=0）

- PAYLOAD 为 CLI 命令文本片段
- APP 应将完整命令字符串按 MTU 限制切分，逐包发送
- **命令结束标志**：最后一包 EOF=1
- APP 可在单包内发送完整命令（SOF=1, EOF=1），只要长度不超过 MTU 限制

**示例**：APP 发送命令 `"ls -la /DOC\n"`（10 字节）

```
假设 MTU 足够大，单包即可：
[01][07][00][0A][00] 'l' 's' ' ' '-' 'l' 'a' ' ' '/' 'D' 'O' 'C' '\n'
 TYPE FLAGS SEQ LEN RSV        PAYLOAD (10 bytes)
```

> **关于换行符**：建议 APP 在每条命令末尾主动追加 `\n`（或 `\r\n`），以便 Core 侧的 CLI 解析器按行处理。若 APP 不追加，Core 侧需等待超时后才执行。

#### 上行：Core → APP（DIR=1）

- PAYLOAD 为 `CLI_Process()` 的输出文本片段
- CH585F 从 Core 收到 `CMD_BT_EXT_CLI_DATA` 后，按原样分包 Notify 给 APP
- **响应结束标志**：最后一包 EOF=1
- Core 侧的 `printf()` 输出可能包含多行文本，APP 按 EOF=1 判断整条响应接收完毕

**示例**：Core 响应 `"total 128\ndrwx...\n"`（较长，分两包）

```
包 1（首包）：
[01][05][10][50][00] 't' 'o' 't' 'a' 'l' ' ' '1' '2' '8' '\n' 'd' 'r' 'w' 'x' ...

包 2（尾包）：
[01][06][11][20][00] ... '\n'
```

#### 空响应处理

某些 CLI 命令（如 `touch file.txt`）成功执行后无输出。Core 侧应至少回复一个空响应帧：

```
[01][07][20][00][00]       # 单包完整上行，PAYLOAD 为空，LEN=0
```

这确保 APP 明确知道命令已执行完毕，避免无限等待。

### 4.2 MSG_TYPE_CTRL (0x02)

控制通道，用于链路管理和参数协商。PAYLOAD 为固定格式的控制数据结构。

#### 控制命令（下行 DIR=0）

| CTRL_CMD 值 | 宏名 | PAYLOAD 格式 | 说明 |
|-------------|------|-------------|------|
| `0x01` | `CTRL_PING` | 无（LEN=0） | 心跳请求，测试链路 |
| `0x02` | `CTRL_PONG` | 无（LEN=0） | 心跳响应 |
| `0x03` | `CTRL_MTU_REQ` | `[建议MTU:2]` | APP 请求协商 MTU（如 `0x00 0xF7` = 247） |
| `0x04` | `CTRL_MTU_RSP` | `[确认MTU:2]` | CH585F 确认实际使用的 MTU |
| `0x05` | `CTRL_FLUSH` | 无（LEN=0） | APP 请求清空 CH585F 的发送缓冲 |
| `0x06` | `CTRL_STATUS_REQ` | 无（LEN=0） | APP 查询 CH585F 当前状态 |
| `0x07` | `CTRL_STATUS_RSP` | `[状态字:1]` | 返回状态：bit0=BLE就绪, bit1=Core在线, bit2=缓冲区非空 |

控制帧格式：

```
PAYLOAD[0]: CTRL_CMD 子命令码
PAYLOAD[1..N]: 参数（视子命令而定）
```

**示例**：APP 发送心跳请求

```
[02][03][00][01][00][01]    # TYPE=CTRL, SOF+EOF+DIR=0, SEQ=0, LEN=1, CTRL_CMD=PING
```

#### 错误控制帧（FLAGS.ERR=1）

当 CH585F 检测到异常时，可通过控制通道上报：

| ERR_CODE | 含义 | 场景 |
|----------|------|------|
| `0x01` | SPI 链路断开 | CH585F 与 Core 通信超时 |
| `0x02` | 发送缓冲区溢出 | 上行数据太多，CH585F 丢弃部分数据 |
| `0x03` | 分包超时 | 收到 SOF 后 2 秒内未收到 EOF |
| `0x04` | SEQ 错误 | 检测到丢包 |

```
[02][1F][00][02][00][04][03]   # TYPE=CTRL, ERR=1, EOF=1, DIR=1, ERR_CODE=SEQ_ERROR
```

### 4.3 MSG_TYPE_HEARTBEAT (0xFF)

简化的心跳帧，**不含 PAYLOAD**（LEN=0），FLAGS 中 DIR 区分方向：

```
APP → CH585F: [FF][03][XX][00][00]   # 下行心跳
CH585F → APP: [FF][07][XX][00][00]   # 上行心跳（回应）
```

- 建议 APP 每 **5 秒** 发送一次心跳
- CH585F 收到后 1 秒内回应
- 若 APP 连续 **3 次** 未收到回应，可认为链路异常，主动断开重连

> 心跳与 `MSG_TYPE_CTRL` 中的 PING/PONG 功能等价，推荐使用 `MSG_TYPE_HEARTBEAT` 更轻量。

---

## 5. 分包与重组规则

### 5.1 发送方分包流程

```
1. 将完整消息（命令或响应）编码为 UTF-8 字节流
2. 计算单帧最大 PAYLOAD = min(当前MTU对应最大载荷, 236) - 5
3. 按 max_payload 切分字节流
4. 首包：SOF=1, EOF=0（若仅一包则 EOF=1）
5. 中间包：SOF=0, EOF=0
6. 尾包：SOF=0, EOF=1（若仅一包则 SOF=1）
7. SEQ 连续递增（模 256）
8. 逐帧通过 BLE Write 或 Notify 发出
```

### 5.2 接收方重组流程

```
1. 维护一个重组缓冲区（建议 4KB，足够容纳 typical CLI 输出）
2. 当收到 SOF=1 的帧：
   a. 清空重组缓冲区
   b. 将本帧 PAYLOAD 写入缓冲区头部
   c. 记录期望的下一个 SEQ = 本帧 SEQ + 1
   d. 启动分包超时定时器（2 秒）
3. 当收到 SOF=0 的帧：
   a. 检查 SEQ 是否等于期望值
   b. 若 SEQ 不连续：丢弃当前重组，发送 ERR=SEQ_ERROR
   c. 若连续：追加 PAYLOAD 到缓冲区，期望 SEQ++
4. 当收到 EOF=1 的帧：
   a. 追加 PAYLOAD
   b. 停止超时定时器
   c. 将完整消息递交给上层（CLI 解析或文件重组）
5. 若超时定时器触发（2 秒内未收到 EOF）：
   a. 丢弃重组缓冲区
   b. 发送 ERR=分包超时
```

### 5.3 单帧消息

若消息较短（如 `"pwd\n"`），可在单帧内完成：

```
FLAGS = SOF | EOF | DIR = 0x03（下行）或 0x07（上行）
SEQ   = 正常递增
LEN   = 实际长度
```

---

## 6. 超时与错误处理

### 6.1 超时定义

| 超时类型 | 时长 | 触发条件 | 处理动作 |
|----------|------|---------|---------|
| 分包超时 | 2000 ms | 收到 SOF 后未在时间内收到 EOF | 丢弃缓冲区，上报 ERR |
| 命令响应超时 | 5000 ms | APP 发送命令后未收到任何上行帧 | APP 可选择重发或提示用户 |
| 心跳超时 | 15000 ms | 连续 3 次心跳无回应 | APP 断开连接，提示链路异常 |
| SPI 转发超时 | 500 ms | CH585F 通过 SPI 发给 Core 后未收到回复 | CH585F 上报 CTRL 错误帧 |

### 6.2 错误恢复

| 错误场景 | APP 侧行为 | CH585F 侧行为 |
|----------|-----------|--------------|
| 单包丢包（SEQ 不连续）| 等待 ERR 帧，或直接重发整条命令 | 丢弃当前重组，Notify ERR 帧给 APP |
| 分包超时 | 重发整条命令 | 清空接收缓冲 |
| BLE 连接断开 | 自动重连（如用户授权） | 停止所有传输，恢复广播 |
| SPI 链路异常 | 无感知（由 CH585F 屏蔽） | 缓存上行数据或丢弃，上报 CTRL 状态帧 |

---

## 7. 数据流示例

### 7.1 完整 CLI 交互（短命令，单包）

```
APP                                 CH585F                              Core
  │                                    │                                    │
  │── Write ──[ CLI: "pwd\n" ]────────>│                                    │
  │   TYPE=0x01 FLAGS=0x03 SEQ=10      │                                    │
  │                                    │── SPI ──[ CMD_BT_EXT_CLI_DATA ]──>│
  │                                    │                                    │
  │                                    │<─ SPI ──[ CMD_BT_EXT_CLI_DATA ]───│
  │                                    │   Payload: "/DOC\n"                │
  │<─ Notify ──[ CLI: "/DOC\n" ]───────│                                    │
  │   TYPE=0x01 FLAGS=0x07 SEQ=20      │                                    │
```

### 7.2 完整 CLI 交互（长响应，多包）

```
APP 发送："tree\n"（单包下行）

Core 返回长文本（约 300 字节），CH585F 分 2 包 Notify：

包 1: TYPE=0x01 FLAGS=0x05 SEQ=30 LEN=236  [前 236 字节]
包 2: TYPE=0x01 FLAGS=0x06 SEQ=31 LEN=64   [剩余 64 字节]

APP 收到包 1（SOF=1）→ 启动重组
APP 收到包 2（EOF=1）→ 拼接完整文本，交给 UI 显示
```

### 7.3 心跳维持

```
APP                                 CH585F
  │                                    │
  │── Write ──[ HEARTBEAT ]───────────>│  (每 5 秒)
  │   TYPE=0xFF FLAGS=0x03 SEQ=100     │
  │                                    │
  │<─ Notify ──[ HEARTBEAT ]───────────│  (1 秒内回应)
  │   TYPE=0xFF FLAGS=0x07 SEQ=101     │
```

### 7.4 错误场景：分包超时

```
APP                                 CH585F
  │                                    │
  │── Write ──[ CLI: "cat big.txt" ]──>│  (包 1/3)
  │   TYPE=0x01 FLAGS=0x01 SEQ=50      │
  │                                    │
  │── Write ──[ CLI: (包 2/3) ]───────>│
  │   TYPE=0x01 FLAGS=0x00 SEQ=51      │
  │                                    │
  │   [包 3 因 BLE 链路抖动丢失]        │
  │                                    │
  │                                    │  ← 2 秒超时触发
  │<─ Notify ──[ CTRL ERR=0x03 ]───────│  (分包超时)
  │   TYPE=0x02 FLAGS=0x1F SEQ=52      │
  │                                    │
  │── Write ──[ CLI: "cat big.txt" ]──>│  (APP 重发整条命令)
```

---

## 8. 与 Core 侧协议衔接

### 8.1 下行：APP → CH585F → Core

1. APP 通过 BLE Write 发送 `MSG_TYPE_CLI_DATA` 帧
2. CH585F 在 BLE 中断/任务中接收完整消息（重组后）
3. CH585F 将完整 CLI 字符串通过 SPI 发送给 Core，使用统一协议：
   ```
   [AA][01][00][LEN][50][01][07][CLI数据...][A5][5A][FC][FD]
   ```
   - `CMD = 0x50`（扩展操作码基码）
   - `DATA[0] = 0x01`（扩展码：基础操作码，保持与协议一致，实际在 BLE 侧已解包）
   - `DATA[1] = 0x07`（扩展码：CLI_DATA）
   - `DATA[2..N]`：完整的 UTF-8 CLI 字符串
4. Core 收到 `CMD_BT_EXT_CLI_DATA` 后，将数据喂入 `CLI_Process()`

> **优化**：若 CLI 字符串较长，Core 侧协议支持 `CMD_DATA_STREAM`（0x07），CH585F 可考虑在数据量超过阈值时使用流式传输。

### 8.2 上行：Core → CH585F → APP

1. Core 的 `CLI_Process()` 通过 `printf()` 输出响应
2. Core 将输出文本通过 SPI 发送给 CH585F，同样使用 `CMD_BT_EXT_CLI_DATA`（扩展码 0x07）
3. CH585F 收到后，将文本重新打包为 `MSG_TYPE_CLI_DATA`（DIR=1），按 MTU 分包
4. CH585F 通过 GATT Notify 逐包发送给 APP
5. APP 重组完整响应文本并显示

### 8.3 关键衔接点

| 衔接点 | Core 侧协议 | APP 侧协议 | CH585F 动作 |
|--------|------------|-----------|------------|
| 命令下发 | `CMD_BT_EXT_CLI_DATA` (0x50/0x07) | `MSG_TYPE_CLI_DATA` DIR=0 | 解包 → 按 SPI 协议转发 |
| 响应上报 | `CMD_BT_EXT_CLI_DATA` (0x50/0x07) | `MSG_TYPE_CLI_DATA` DIR=1 | SPI 接收 → 按 BLE 协议打包 Notify |
| 连接状态 | `CMD_BT_CONN_EVT` (0x54) | `MSG_TYPE_CTRL` STATUS_RSP | 转换事件类型后 Notify |
| 心跳/链路 | 无（纯 APP-CH585F 本地） | `MSG_TYPE_HEARTBEAT` | 本地处理，不转发 Core |

---

## 9. 文件传输扩展（P3 预留）

虽然当前阶段 APP 仅支持 CLI 模式，但协议已为文件传输预留扩展能力。文件传输通过 CLI 命令触发（如 `put file.txt`、`get file.txt`），实际数据走 `MSG_TYPE_FILE_*` 通道。

### 9.1 文件传输元数据（MSG_TYPE_FILE_META, 0x03）

```
PAYLOAD 格式：
[0]: 操作类型  0x01=上传请求(APP→Core), 0x02=下载请求, 0x03=元数据确认
[1..4]: 文件大小（uint32, little-endian）
[5..20]: 文件名（16 字节，不足补 0x00，UTF-8）
[21..24]: CRC32 校验（可选）
```

### 9.2 文件数据分块（MSG_TYPE_FILE_CHUNK, 0x04）

```
PAYLOAD 格式：
[0..1]: 块序号（uint16, little-endian）
[2..3]: 总块数（uint16, little-endian）
[4..N]: 原始文件数据
```

- 每块独立传输，ACK_REQ=1 时需要接收方回复 `MSG_TYPE_ACK`
- 块大小由 MTU 决定，建议不超过 200 字节/块
- 全部块接收完毕后，接收方校验并回复 `MSG_TYPE_FILE_META`（操作类型=0x03 确认）

### 9.3 与 CLI 的关系

文件传输由 CLI 命令**触发**，但数据不混在 `MSG_TYPE_CLI_DATA` 中：

```
APP: 输入 "put config.json"
Core: 解析命令，识别为文件上传请求
Core: 回复 "Ready to receive config.json, 2048 bytes"
APP: 发送 MSG_TYPE_FILE_META（上传请求）
CH585F: 转发给 Core
Core: 确认 META，进入文件接收模式
APP: 连续发送 MSG_TYPE_FILE_CHUNK（块 1/10, 2/10...）
CH585F: 逐块转发
Core: 接收完毕，回复 "Upload complete"
```

---

## 10. 实现建议

### 10.1 CH585F 侧建议

| 模块 | 建议 |
|------|------|
| BLE 接收缓冲 | 维护一个 2KB 的环形缓冲区，存放 APP Write 来的原始帧 |
| 重组缓冲区 | 4KB 静态数组，用于 `MSG_TYPE_CLI_DATA` 的重组 |
| 发送缓冲 | 2KB 环形缓冲区，存放待 Notify 给 APP 的上行数据 |
| TMOS 任务 | 注册独立任务处理 BLE 事件，避免在中断中执行重组/分包逻辑 |
| MTU 协商 | 在连接建立后主动发起 MTU 交换，目标 247 字节 |

### 10.2 APP 侧建议

| 功能 | 建议 |
|------|------|
| 命令编辑 | 提供命令行输入框，支持历史记录、自动补全 |
| 输出显示 | 滚动终端界面，支持 ANSI 颜色代码（可选） |
| 分包透明 | APP SDK 层实现自动分包/重组，UI 层无感知 |
| 重连机制 | 连接断开后自动扫描并重连（需用户授权） |
| 心跳策略 | 前台每 5 秒心跳，后台可降低频率或暂停 |

### 10.3 调试建议

- 开发阶段 APP 可集成日志模式：打印收发每一帧的 TYPE/FLAGS/SEQ/LEN/PAYLOAD_HEX
- CH585F UART1 Debug 输出每帧的收发详情，便于定位是 BLE 链路还是 SPI 链路问题
- 使用 nRF Connect 等工具直接测试 GATT Write/Notify，验证 CH585F 侧协议实现

---

## 11. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| V1.0 | 2026-05-06 | 初始版本：定义 GATT Profile、应用层帧格式、CLI 数据通道、控制通道、分包机制、文件传输预留 |

---

## 附录 A：快速参考卡

### A.1 最小可工作交互

```
APP 连接 CH585F，使能 CLI_TX Notify

1. APP 发送命令：
   Write(0xFFF1): [01][03][00][04][00] 'h' 'e' 'l' 'p' '\n'
   → TYPE=CLI, SOF+EOF+DIR=0, SEQ=0, LEN=4, "help\n"

2. CH585F 转发给 Core（SPI 协议帧，略）

3. Core 回复（假设输出 "Available commands: ...\n"）

4. CH585F Notify APP：
   Notify(0xFFF2): [01][07][01][18][00] 'A' 'v' 'a' 'i' ... '\n'
   → TYPE=CLI, SOF+EOF+DIR=1, SEQ=1, LEN=24
```

### A.2 MTU 与最大 Payload 对照表

| ATT_MTU | L2CAP 载荷 | ATT 头 | 应用层头(5B) | 最大 PAYLOAD |
|---------|-----------|--------|-------------|-------------|
| 23 (默认) | 20 | 3 | 5 | **12 字节** |
| 185 | 182 | 3 | 5 | **174 字节** |
| 247 | 244 | 3 | 5 | **236 字节** |

> 强烈建议协商 MTU 至 185 或更高，否则中文命令/长路径极易触发频繁分包。
