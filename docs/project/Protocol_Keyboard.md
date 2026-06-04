# Keyboard 模块通信协议

## 1. 模块概述

- **模块类型编号**：`0x04`
- **模块 ID**：`0x20`（预留范围 `0x20~0x22`）

Keyboard 模块有三种物理形态，均通过 UART3 与 Core（V3F）通信：

| 子类型 | 名称 | 主要特征 |
|--------|------|---------|
| `0x01` | Keyboard-1（主键盘） | 40 配列，CH9329 串口转 HID，标准 USB HID 键盘输出 |
| `0x02` | Keyboard-2（游戏键盘） | 方向键 + 摇杆 + 4 自定义按键，CH9329 串口转 HID |
| `0x03` | Keyboard-3（音乐键盘） | 16 琴键 + 2 旋钮 + 4 控制按键，无 CH9329，直连 Core |

### 1.1 帧格式速查

Keyboard 模块通过 **UART3** 与 Core（V3F）通信，物理层参数：

| 参数 | 值 |
|------|-----|
| 波特率 | 230400 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 流控 | 无 |

**帧结构**（总长度最大 264 字节）：

```
+--------+--------+--------+--------+--------+-------------+--------+--------+--------+--------+
|  HEAD  |  SRC   |  DST   |  LEN   |  CMD   |  DATA[0..N] | TAIL0  | TAIL1  | TAIL2  | TAIL3  |
| 1 byte | 1 byte | 1 byte | 1 byte | 1 byte |   N bytes   | 1 byte | 1 byte | 1 byte | 1 byte |
+--------+--------+--------+--------+--------+-------------+--------+--------+--------+--------+
```

- **HEAD**：固定 `0xAA`。仅在接收状态机处于“等待帧头”时识别为新帧起始，数据域中的 `0xAA` 不会触发重解析。
- **LEN**：`CMD + DATA` 的总字节数，即 `N = LEN - 1`，范围 `1 ~ 255`。`LEN = 1` 表示 DATA 为空。
- **DATA**：数据域，长度由 LEN 推导，最大 255 字节。
- **TAIL**：固定序列 `A5 5A FC FD`，任一字节不匹配则丢弃整帧。

**常用模块 ID**：

| ID | 模块 |
|----|------|
| `0x00` | Core（主控） |
| `0x20` | Keyboard（本模块） |

**帧长度计算**：
```
总帧长 = 1(HEAD) + 1(SRC) + 1(DST) + 1(LEN) + 1(CMD) + N(DATA) + 4(TAIL)
       = 9 + N
       = 8 + LEN
```

> 完整协议规范（含接收状态机、通用操作码 `0x00~0x0F`、NACK 错误码定义）见 `PROJECT.md`。

---

## 2. 操作码定义

Keyboard 模块专用操作码范围为 `0x21 ~ 0x2F`，按模块 ID 高 4 位 `0x2` 统一分配。

### 2.1 基础操作码（0x21 ~ 0x2F）

| 操作码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x21` | `CMD_KBD_SET_BACKLIGHT` | Core -> Keyboard | 设置键盘背光 | `[模式:1][亮度:1]` |
| `0x22` | `CMD_KBD_GET_BACKLIGHT` | Core -> Keyboard | 获取背光状态 | 无，Keyboard 回复 ACK + `[模式:1][亮度:1]` |
| `0x23` | `CMD_KBD_SET_CONFIG` | Core -> Keyboard | 设置配置参数（键映射等） | `[配置项:1][配置值:变长]` |
| `0x24` | `CMD_KBD_GET_STATUS` | Core -> Keyboard | 获取键盘状态 | `[状态类型:1]`，Keyboard 回复 ACK + 状态数据 |
| `0x25` | `CMD_KBD_HID_REPORT` | Keyboard -> Core | HID 输入报告（Keyboard-1/2） | `[报告类型:1][HID报告数据:变长]` |
| `0x26` | `CMD_KBD_MUSIC_KEYS` | Keyboard -> Core | 音乐键盘琴键状态 | `[[键ID:1][力度:1]...]` |
| `0x27` | `CMD_KBD_MUSIC_KNOBS` | Keyboard -> Core | 音乐键盘旋钮状态 | `[旋钮1值:2][旋钮2值:2]` |
| `0x28` | `CMD_KBD_MUSIC_SLIDER` | Keyboard -> Core | 音乐键盘推杆状态 | `[推杆值:2]` |
| `0x29` | `CMD_KBD_GAME_INPUT` | Keyboard -> Core | 游戏键盘输入事件 | `[摇杆X:int8][摇杆Y:int8][按键:1]` |
| `0x2A~0x2F` | — | — | 预留 | — |

---

## 3. ACK/NACK 响应规则

### 3.1 设计原则

ACK/NACK 由**命令语义**决定：

| 语义类别 | 说明 | 响应要求 |
|----------|------|---------|
| **查询 (Query)** | 请求获取某信息 | **必须 ACK + 数据** |
| **动作 (Action)** | 请求执行某操作 | **必须 ACK（空）或 NACK** |
| **设置 (Set)** | 修改某参数/状态 | **发送即忘，不响应** |
| **事件 (Event)** | 单向输入/状态上报 | **不需要响应** |

> **发送即忘**：Core 向 Keyboard 发送设置类命令后不需要等待 ACK。Keyboard 被动执行，不支持时静默丢弃。

### 3.2 操作码响应要求

| 码 | 命令 | 语义 | 接收方行为 |
|---|---|---|---|
| `0x21` | SET_BACKLIGHT | 设置 | 发送即忘，Keyboard 不回复 |
| `0x22` | GET_BACKLIGHT | 查询 | Keyboard 回复 ACK + `[模式:1][亮度:1]` |
| `0x23` | SET_CONFIG | 设置 | 发送即忘，Keyboard 不回复 |
| `0x24` | GET_STATUS | 查询 | Keyboard 回复 ACK + 状态数据 或 NACK |
| `0x25` | HID_REPORT | 事件 | Core 不回复 |
| `0x26` | MUSIC_KEYS | 事件/状态 | Core 不回复（事件上报时）；ACK + 数据（查询响应时） |
| `0x27` | MUSIC_KNOBS | 事件/状态 | Core 不回复（事件上报时）；ACK + 数据（查询响应时） |
| `0x28` | MUSIC_SLIDER | 事件/状态 | Core 不回复（事件上报时）；ACK + 数据（查询响应时） |
| `0x29` | GAME_INPUT | 事件 | Core 不回复 |

### 3.3 ACK / NACK 帧格式

**ACK 帧**：
```
[AA][DST][SRC][LEN][CMD=0x04][DATA...][A5][5A][FC][FD]
```
- SRC/DST 与原请求**交换**
- DATA 可选携带响应数据，`LEN = 1 + data_len`
- 空 ACK：`LEN = 01`，无 DATA

**NACK 帧**：
```
[AA][DST][SRC][02][CMD=0x05][错误码:1][A5][5A][FC][FD]
```
- DATA[0] = 错误码（1 字节），详见 `PROJECT.md` 通用协议错误码定义
- `LEN = 02`

### 3.4 Core 侧超时处理

Core 发送**查询类**命令后，应在 **500ms** 内收到 ACK/NACK。超时后由应用层处理。**协议层不实现自动重传**。

---

## 4. 数据格式详解

### 4.1 背光控制格式（CMD 0x21 / 0x22）

**设置背光（Core → Keyboard，0x21）**：
```
DATA[0]: 背光模式（1字节）
  - 0x00: 关闭
  - 0x01: 常亮
  - 0x02: 呼吸灯
  - 0x03: 跑马灯
  - 0x04: 随按键点亮
  - 0x80~0xFF: 用户自定义
DATA[1]: 亮度（1字节，0~100，0xFF=不变）
```

**获取背光响应（Keyboard → Core，ACK）**：
```
DATA[0]: 当前背光模式（1字节）
DATA[1]: 当前亮度（1字节，0~100）
```

### 4.2 配置设置/查询格式（CMD 0x23 / 0x24）

**设置配置（Core → Keyboard，0x23）**：
```
DATA[0]: 配置项（1字节）
  - 0x00: 键位映射层（0=默认层, 1=Fn层, 2=自定义层1...）
  - 0x01: 单个键映射（DATA[1]=物理键ID, DATA[2]=映射键码）
  - 0x02: 去抖时间（DATA[1]=毫秒，1字节）
  - 0x03: 重复按键速率（DATA[1]=字符/秒，1字节）
  - 0x80~0xFF: 用户自定义配置项
DATA[1..N]: 配置值（变长，由配置项决定长度）
```

**获取状态（Core → Keyboard，0x24）**：
```
DATA[0]: 状态类型（1字节）
  - 0x00: 查询音乐键盘琴键状态 → 回复 ACK，DATA 格式同 4.4 节
  - 0x01: 查询音乐键盘旋钮状态 → 回复 ACK，DATA 格式同 4.5 节
  - 0x02: 查询音乐键盘推杆状态 → 回复 ACK，DATA 格式同 4.6 节
  - 0x03: 查询游戏键盘输入状态 → 回复 ACK，DATA 格式同 4.7 节
  - 0x04: 查询当前按键矩阵状态 → 回复 ACK + `[按键位图:变长]`
```

### 4.3 HID 输入报告格式（CMD 0x25）

Keyboard-1（主键盘）和 Keyboard-2（游戏键盘）通过 CH9329 获取 HID 报告，通过本命令上报给 Core。

```
DATA[0]: 报告类型（1字节）
  - 0x00: 键盘报告（仿 HID Keyboard Boot Report）
  - 0x01: 鼠标报告（仿 HID Mouse Boot Report）

DATA[1..N]: HID 报告数据（N = LEN - 2）
```

**键盘报告（类型 = 0x00，N = 8）**：
```
DATA[1]: 修饰键（bitmask）
  - bit0: Left Ctrl    - bit1: Left Shift
  - bit2: Left Alt     - bit3: Left GUI
  - bit4: Right Ctrl   - bit5: Right Shift
  - bit6: Right Alt    - bit7: Right GUI
DATA[2]: 保留（0x00）
DATA[3]: 键码 0（USB HID Usage ID，0x00 表示空槽）
DATA[4]: 键码 1
DATA[5]: 键码 2
DATA[6]: 键码 3
DATA[7]: 键码 4
DATA[8]: 键码 5
```

> 最多同时上报 6 个非修饰键。释放按键时对应键码槽填 `0x00`。

**鼠标报告（类型 = 0x01，N = 4）**：
```
DATA[1]: 按钮状态（bitmask）
  - bit0: 左键    - bit1: 右键    - bit2: 中键
DATA[2]: X 轴偏移（int8，-128~127，正值向右）
DATA[3]: Y 轴偏移（int8，-128~127，正值向下）
DATA[4]: 滚轮偏移（int8，正值为远离用户/向上滚动）
```

### 4.4 音乐键盘琴键格式（CMD 0x26）

Keyboard-3（音乐键盘）上报当前按下的琴键列表。

```
DATA[0..N]: 琴键列表（每个琴键 2 字节，N = LEN - 1）

  琴键条目：
  [键ID: 1字节][力度: 1字节]
```

> 键 ID：0~15 对应 16 个琴键。
> 力度：0~127（MIDI 兼容，0 表示释放）。
> 当前按下的键数 = `(LEN - 1) / 2`。
> 无按键按下时 `LEN = 1`，DATA 为空。

**示例**：同时按下琴键 3（力度 100）和琴键 7（力度 80）
```
LEN = 05
DATA[0] = 0x03  /* 键 ID = 3 */
DATA[1] = 0x64  /* 力度 = 100 */
DATA[2] = 0x07  /* 键 ID = 7 */
DATA[3] = 0x50  /* 力度 = 80 */
```

### 4.5 音乐键盘旋钮格式（CMD 0x27）

```
DATA[0..1]: 旋钮 1 值（uint16 大端，0~65535）
DATA[2..3]: 旋钮 2 值（uint16 大端，0~65535）
```

> 旋钮值采用 16 位无符号大端，Display 端/Core 端按实际功能映射到具体参数（如音量、音色、速度等）。
> 旋钮无绝对零点，为相对增量编码器时，建议用 uint16 循环计数（0~65535 回绕）。

### 4.6 音乐键盘推杆格式（CMD 0x28）

```
DATA[0..1]: 推杆值（uint16 大端，0~65535）
```

> 推杆值 0 表示最低位置，65535 表示最高位置。中间位置约为 32768。

### 4.7 游戏键盘输入格式（CMD 0x29）

Keyboard-2（游戏键盘）上报摇杆和自定义按键状态。

```
DATA[0]: 摇杆 X 轴（int8，-128~127，正值向右）
DATA[1]: 摇杆 Y 轴（int8，-128~127，正值向下）
DATA[2]: 自定义按键状态（bitmask）
  - bit0: 按键 1    - bit1: 按键 2
  - bit2: 按键 3    - bit3: 按键 4
  - bit4~7: 预留
```

> 摇杆中心位置为 (0, 0)。按键按下对应 bit 置 1，释放置 0。

---

## 5. CMD_GET_TYPE 响应

Keyboard 模块响应 `CMD_GET_TYPE` 时，`CMD_ACK` 的 DATA 格式如下：

| 字节 | 字段 | 值/说明 |
|------|------|---------|
| DATA[0] | 模块类型编号 | `0x04` |
| DATA[1] | 模块子类型编号 | 见下表 |
| DATA[2] | 硬件版本号 | 键盘硬件版本 |
| DATA[3] | 固件主版本号 | 键盘固件主版本 |
| DATA[4] | 固件次版本号 | 键盘固件次版本 |
| DATA[5..N] | 扩展信息 | 键位布局支持、背光能力等（可选） |

**Keyboard 子类型定义**

> 代码宏定义：`protocol.h` → `MODULE_SUBTYPE_KEYBOARD_*`

| 子类型编号 | 宏名 | 实现名称 | 说明 |
|------------|------|----------|------|
| `0x00` | `MODULE_SUBTYPE_KEYBOARD_RESERVED` | 保留 | 系统保留 |
| `0x01` | `MODULE_SUBTYPE_KEYBOARD_MAIN` | Main Keyboard | 主键盘（标准 40 配列 + CH9329） |
| `0x02` | `MODULE_SUBTYPE_KEYBOARD_GAME` | Game Keyboard | 游戏键盘（摇杆 + 自定义按键 + CH9329） |
| `0x03` | `MODULE_SUBTYPE_KEYBOARD_MUSIC` | Music Keyboard | 音乐键盘（16 琴键 + 2 旋钮 + 4 控制键） |
| `0x04~0xFF` | — | 预留 | 未来扩展 |

---

## 6. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| V1.0 | 2026-04-24 | 初始版本：定义 HID 报告、音乐键盘、游戏键盘操作码及数据格式 |
