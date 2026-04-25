# Display 模块通信协议

## 1. 模块概述

- **模块类型编号**：`0x01`
- **模块 ID**：`0x10`（预留范围 `0x10~0x12`）

### 1.1 帧格式速查

Display 模块通过 **UART4** 与 Core（V5F）通信，物理层参数：

| 参数 | 值 |
|------|-----|
| 波特率 | 921600 |
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
| `0x10` | Display（本模块） |

**帧长度计算**：
```
总帧长 = 1(HEAD) + 1(SRC) + 1(DST) + 1(LEN) + 1(CMD) + N(DATA) + 4(TAIL)
       = 9 + N
       = 8 + LEN
```

> 完整协议规范（含接收状态机、通用操作码 `0x00~0x0F`、NACK 错误码定义）见 `PROJECT.md`。

---

## 2. 操作码定义

Display 模块操作码分为两类：
- **基础操作码**（`0x11~0x1F`）：直接对应常用功能，低延迟、高频率。
- **扩展操作码**：当基础操作码为 `0x10` 时，`DATA[0]` 作为扩展操作码，支持大数据量或低频复杂功能。

### 2.1 基础操作码（0x11 ~ 0x1F）

| 操作码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x11` | `CMD_DISP_SET_BRIGHTNESS` | Core -> Display | 设置背光亮度 | `[亮度值: 0-100]` |
| `0x12` | `CMD_DISP_GET_BRIGHTNESS` | Core -> Display | 获取当前背光亮度 | 无，Display 回复 ACK + 亮度值 |
| `0x13` | `CMD_DISP_SHOW_PAGE` | Core -> Display | 切换显示页面 | `[页面编号]` |
| `0x14` | `CMD_DISP_UPDATE_STATUS` | Core -> Display | 更新状态栏/模块状态信息 | 见下方状态栏数据格式 |
| `0x15` | `CMD_DISP_INPUT_EVENT` | Core -> Display | 统一输入事件（键盘/鼠标/触摸/虚拟触摸） | 见下方输入事件格式 |
| `0x16` | `CMD_DISP_SET_ROTATION` | Core -> Display | 设置屏幕旋转角度 | `[角度: 0/90/180/270]` |
| `0x17` | `CMD_DISP_GET_ROTATION` | Core -> Display | 获取当前屏幕旋转角度 | 无，Display 回复 ACK + 角度 |
| `0x18` | `CMD_DISP_SCREEN_CONTROL` | Core -> Display | 屏幕控制（息屏/亮屏/自动息屏/超时） | 见下方屏幕控制格式 |
| `0x19` | `CMD_DISP_GET_SCREEN_STATE` | Core -> Display | 获取屏幕当前状态 | 无，Display 回复 ACK + 状态 |
| `0x1A` | `CMD_DISP_SHOW_NOTICE` | Core -> Display | 显示通知弹窗 | `[优先级:1][标题:16字节][内容:变长]` |
| `0x1B` | `CMD_DISP_MUSIC_CONTROL` | Display -> Core | 音乐播放控制 | `[控制类型]` |
| `0x1C` | `CMD_DISP_MUSIC_STATUS` | Core -> Display | 音乐播放状态上报 | `[播放状态][进度高][进度低][总时长高][总时长低][音量]` |
| `0x1D` | `CMD_DISP_VOLUME_CONTROL` | 双向 | 音量设置/获取 | `[操作: 0=设置, 1=查询][音量值(设置时)]` |
| `0x1E` | `CMD_DISP_ETHERNET_STATUS` | Core -> Display | 以太网状态上报/响应 | `[连接状态:1][IP:4][掩码:4][网关:4][MAC:6]` |
| `0x1F` | `CMD_DISP_FACTORY_RESET` | Core -> Display | 恢复出厂设置 | `[确认码: 0xA5]` |

### 2.2 扩展操作码（CMD = 0x10，DATA[0] 为子命令）

| 扩展码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x01` | `CMD_DISP_EXT_APP_LAUNCH` | Core -> Display | 启动指定应用 | `[应用ID]` |
| `0x02` | `CMD_DISP_EXT_APP_CLOSE` | Core -> Display | 关闭当前应用 | 无 |
| `0x03` | `CMD_DISP_EXT_APP_DATA` | 双向 | 应用数据传输 | `[应用ID:1][数据类型:1][数据:变长]` |
| `0x04` | `CMD_DISP_EXT_MODULE_STATUS` | Core -> Display | 模块状态主动上报（插拔/在线/离线） | 见下方模块状态格式 |
| `0x05` | `CMD_DISP_EXT_GET_MODULE_STATUS` | Display -> Core | 请求获取当前模块组成信息 | 无，Core 回复 `0x04` |
| `0x06` | `CMD_DISP_EXT_REQUEST_FILE_LIST` | Display -> Core | 请求当前目录文件列表 | `[路径:变长]` |
| `0x07` | `CMD_DISP_EXT_FILE_LIST` | Core -> Display | 文件列表数据响应 | `[文件数量:1][文件信息:固定21字节×N]` |
| `0x08` | `CMD_DISP_EXT_FILE_READ` | Display -> Core | 请求读取文件（触发 Bulk Mode） | `[路径:变长]` |
| `0x09` | `CMD_DISP_EXT_FILE_SAVE` | Display -> Core | 请求保存文件（触发 Bulk Mode） | `[路径:变长]` |
| `0x0A` | `CMD_DISP_EXT_FILE_OPERATION` | 双向 | 文件操作（创建/删除/重命名） | `[操作类型:1][路径:变长]` |
| `0x0B` | `CMD_DISP_EXT_PLAY_MUSIC` | Display -> Core | 请求播放指定 wav 文件 | `[路径:变长]` |
| `0x0C` | `CMD_DISP_EXT_BT_SCAN` | Display -> Core | 请求扫描蓝牙设备 | 无 |
| `0x0D` | `CMD_DISP_EXT_BT_DEVICE_LIST` | Core -> Display | 蓝牙设备列表（扫描结果） | `[设备数量:1][设备信息:固定23字节×N]` |
| `0x0E` | `CMD_DISP_EXT_BT_CONNECT` | Display -> Core | 请求连接/断开蓝牙设备 | `[操作: 0=断开, 1=连接][MAC地址(6字节)]` |
| `0x0F` | `CMD_DISP_EXT_BT_STATUS` | Core -> Display | 蓝牙连接状态变化上报 | `[状态][设备MAC...]` |
| `0x10` | `CMD_DISP_EXT_FP_STATUS` | Core -> Display | 指纹识别信息上报 | `[状态][指纹ID][匹配度]` |
| `0x11` | `CMD_DISP_EXT_NFC_TAG` | Core -> Display | NFC 识别信息上报 | `[标签类型:1][UID:7字节][数据:变长]` |
| `0x12` | `CMD_DISP_EXT_HEALTH_DATA` | Core -> Display | 健康监测信息上报 | `[数据类型][数值高][数值低][时间戳...]` |
| `0x13` | `CMD_DISP_EXT_POWER_STATUS` | Core -> Display | 电源状态信息上报 | `[电压高][电压低][电流高][电流低][电量][充电状态]` |
| `0x14` | `CMD_DISP_EXT_IR_RANGE_REQ` | Display -> Core | 请求红外测距 | 无 |
| `0x15` | `CMD_DISP_EXT_IR_RANGE_DATA` | Core -> Display | 红外测距结果返回 | `[距离高][距离低][单位][精度]` |
| `0x16` | `CMD_DISP_EXT_SAVE_CONFIG` | Display -> Core | 请求保存配置到 TF 卡 | `[路径:变长]` |
| `0x17` | `CMD_DISP_EXT_LOAD_CONFIG` | Display -> Core | 请求加载配置文件 | `[路径:变长]` |
| `0x18` | `CMD_DISP_EXT_CONFIG_RESULT` | Core -> Display | 配置保存/加载结果 | `[结果:1][错误码:1][配置数据:变长]` |
| `0x19` | `CMD_DISP_EXT_SET_RGB_MODE` | Display -> Core | 设置 RGB 灯效模式 | `[灯效ID][颜色R][颜色G][颜色B][亮度][速度]` |
| `0x1A` | `CMD_DISP_EXT_BULK_TRANSFER` | 双向 | 批量传输控制/确认 | 见下方批量传输协议 |
| `0x1B` | `CMD_DISP_EXT_SUBDISP_CONTENT` | Core <-> Display | 副屏显示内容设置 | `[内容类型:1][内容数据:变长]` |
| `0x1C` | `CMD_DISP_EXT_SUBDISP_CONFIG` | Core <-> Display | 副屏配置 | `[分辨率][方向][开关]` |
| `0x1D` | `CMD_DISP_EXT_SCREEN_WAKEUP` | Display -> Core | 屏幕唤醒事件 | `[唤醒源]` |
| `0x1E` | `CMD_DISP_EXT_SCREEN_SLEEP` | Display -> Core | 屏幕休眠事件 | `[休眠源]` |
| `0x1F` | `CMD_DISP_EXT_ERROR_REPORT` | Display -> Core | Display 错误/异常上报 | `[错误码:1][错误信息:变长]` |
| `0x20` | `CMD_DISP_EXT_REPORT_EVENT` | Display -> Core | Display 用户事件上报 | `[事件类型:1][事件数据:变长]` |
| `0x21` | `CMD_DISP_EXT_REQUEST_DATA` | Display -> Core | Display 通用数据请求 | `[请求类型:1][参数:变长]` |
| `0x22~0x3F` | — | — | 预留 | — |

---

## 3. ACK/NACK 响应规则

### 3.1 设计原则

ACK/NACK 由**命令语义**决定，而非发送方向：

| 语义类别 | 说明 | 响应要求 |
|----------|------|---------|
| **查询 (Query)** | 请求获取某信息 | **必须 ACK + 数据** |
| **动作 (Action)** | 请求执行某操作 | **必须 ACK（空）或 NACK** |
| **设置 (Set)** | 修改某参数/状态 | **发送即忘，不响应** |
| **事件 (Event)** | 单向状态/输入上报 | **不需要响应** |

> **发送即忘**：Core 作为系统主控，向 Display 发送设置类命令（如亮度、页面切换）后不需要等待 ACK。Display 被动执行，不支持时静默丢弃即可。这避免了 Core 维护未确认队列，简化状态机。

### 3.2 基础操作码响应要求

| 码 | 命令 | 语义 | 接收方行为 |
|---|---|---|---|
| `0x11` | SET_BRIGHTNESS | 设置 | 发送即忘，Display 不回复 |
| `0x12` | GET_BRIGHTNESS | 查询 | Display 回复 ACK + `[亮度值:1]` |
| `0x13` | SHOW_PAGE | 设置 | 发送即忘，Display 不回复 |
| `0x14` | UPDATE_STATUS | 设置 | 发送即忘，Display 不回复 |
| `0x15` | INPUT_EVENT | 事件 | Core 不回复 |
| `0x16` | SET_ROTATION | 设置 | 发送即忘，Display 不回复 |
| `0x17` | GET_ROTATION | 查询 | Display 回复 ACK + `[角度:1]` |
| `0x18` | SCREEN_CONTROL | 设置 | 发送即忘，Display 不回复 |
| `0x19` | GET_SCREEN_STATE | 查询 | Display 回复 ACK + `[状态:4]` |
| `0x1A` | SHOW_NOTICE | 设置 | 发送即忘，Display 不回复 |
| `0x1B` | MUSIC_CONTROL | 动作 | Core 回复 ACK 空 或 NACK |
| `0x1C` | MUSIC_STATUS | 事件 | Display 不回复 |
| `0x1D` | VOLUME_CONTROL | 动作/查询 | 接收方回复 ACK（查询时带音量）或 NACK |
| `0x1E` | ETHERNET_STATUS | 事件 | Display 不回复 |
| `0x1F` | FACTORY_RESET | 设置 | 发送即忘，Display 不回复 |

### 3.3 扩展操作码响应要求

| 码 | 命令 | 语义 | 接收方行为 |
|---|---|---|---|
| `0x01` | APP_LAUNCH | 设置 | 发送即忘，Display 不回复 |
| `0x02` | APP_CLOSE | 设置 | 发送即忘，Display 不回复 |
| `0x03` | APP_DATA | 数据通道 | **应用层自定义**，协议层不强制 ACK |
| `0x04` | MODULE_STATUS | 事件 | Display 不回复 |
| `0x05` | GET_MODULE_STATUS | 查询 | Core 回复 ACK + `[模块列表:变长]` |
| `0x06` | REQUEST_FILE_LIST | 查询 | Core 回复 ACK + `[文件列表:变长]` |
| `0x07` | FILE_LIST | 查询响应 | 发送即忘（作为 0x06 的 ACK payload） |
| `0x08` | FILE_READ | 动作 | Core 回复 ACK + `[Bulk参数:6]` 或 NACK |
| `0x09` | FILE_SAVE | 动作 | Core 回复 ACK + `[Bulk参数:6]` 或 NACK |
| `0x0A` | FILE_OPERATION | 动作 | 接收方回复 ACK 空 或 NACK |
| `0x0B` | PLAY_MUSIC | 动作 | Core 回复 ACK 空 或 NACK |
| `0x0C` | BT_SCAN | 动作 | Core 回复 ACK 空 或 NACK |
| `0x0D` | BT_DEVICE_LIST | 查询响应 | 发送即忘（作为 0x0C 的 ACK payload） |
| `0x0E` | BT_CONNECT | 动作 | Core 回复 ACK 空 或 NACK |
| `0x0F` | BT_STATUS | 事件 | Display 不回复 |
| `0x10` | FP_STATUS | 事件 | Display 不回复 |
| `0x11` | NFC_TAG | 事件 | Display 不回复 |
| `0x12` | HEALTH_DATA | 事件 | Display 不回复 |
| `0x13` | POWER_STATUS | 事件 | Display 不回复 |
| `0x14` | IR_RANGE_REQ | 查询 | Core 回复 ACK + `[距离数据:4]` 或 NACK |
| `0x15` | IR_RANGE_DATA | 查询响应 | 发送即忘 |
| `0x16` | SAVE_CONFIG | 动作 | Core 回复 ACK 空 或 NACK |
| `0x17` | LOAD_CONFIG | 查询 | Core 回复 ACK + `[配置数据:变长]` 或 NACK |
| `0x18` | CONFIG_RESULT | 事件 | Display 不回复 |
| `0x19` | SET_RGB_MODE | 动作 | Core 回复 ACK 空 或 NACK |
| `0x1A` | BULK_TRANSFER | 控制 | 接收方回复 ACK 空（握手/确认帧） |
| `0x1B` | SUBDISP_CONTENT | 设置 | 发送即忘 |
| `0x1C` | SUBDISP_CONFIG | 设置 | 发送即忘 |
| `0x1D` | SCREEN_WAKEUP | 事件 | Core 不回复 |
| `0x1E` | SCREEN_SLEEP | 事件 | Core 不回复 |
| `0x1F` | ERROR_REPORT | 事件 | Core 不回复 |
| `0x20` | REPORT_EVENT | 事件 | Core 不回复 |
| `0x21` | REQUEST_DATA | 查询 | Core 回复 ACK + `[请求数据:变长]` 或 NACK |

### 3.4 ACK / NACK 帧格式

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

### 3.5 Display 侧超时处理

Display 发送**动作/查询类**命令后，应在 **500ms** 内收到 ACK/NACK。超时后由应用层决定提示用户或允许用户手动重试。**协议层不实现自动重传**。

---

## 4. 批量传输协议（Bulk Transfer Mode）

### 4.1 设计背景

统一协议单帧 DATA 域上限为 255 字节（`LEN` 字段 1 字节）。若用传统分帧方式传输 400KB 文件，需要约 1700 帧，协议头尾开销大、CPU 中断频繁。

**批量传输协议** 在不修改统一协议帧格式的前提下，通过 **标准协议帧握手 + UART DMA Raw 数据传输** 实现大文件高效传输。

### 4.2 核心机制

- **控制阶段**：使用标准协议帧（`0xAA ... A5 5A FC FD`）进行握手和确认
- **数据阶段**：发送方直接通过 UART DMA 发送 **原始文件数据**（无协议头尾），接收方用 **DMA + IDLE 中断** 接收
- **块大小**：由 ACK 帧协商，**默认 20KB**，最大不超过 `CH378_MAX_FILE_SIZE`（400KB）
- **无 CRC 校验**：不实现错码修正，误码由应用层自行兜底（如花屏则重传）

### 4.3 UART IDLE 中断

IDLE 中断在接收线空闲超过 **1 个字节时间**（921600 波特率下约 10.8μs）时触发。配合 DMA 使用，可解决最后一块不足 20KB 的收尾问题：
- **整 block（20KB）**：DMA 收满触发 `TCIF` 完成中断
- **尾 block（<20KB）**：发送方停发后 UART 线空闲，触发 `IDLE` 中断，CPU 读取 DMA 剩余计数器得到实际接收长度

### 4.4 传输流程

#### 3.4.1 文件读取（Core → Display）

```
① Display → Core: [标准帧] CMD=0x10 EXT=0x08 FILE_READ
   DATA: [文件路径字符串...]

② Core → Display: [标准帧] ACK + 传输参数
   DATA[0..3]:  总大小（32位大端，单位字节）
   DATA[4..5]:  块大小（16位大端，默认 20480）

③ [进入 Bulk Mode - 数据阶段]
   Core → Display: [Raw] Block 0（20KB 原始数据）
   Core → Display: [Raw] Block 1（20KB 原始数据）
   ...
   Core → Display: [Raw] Block N（最后一块，可能不足 20KB）

④ Display → Core: [标准帧] CMD=0x10 EXT=0x1A BULK_TRANSFER
   DATA[0]:   完成标记（0x01 = 接收完毕）
   DATA[1..4]: 实际接收总大小（32位大端）

⑤ Core → Display: [标准帧] ACK（结束）
```

#### 3.4.2 文件保存（Display → Core）

```
① Display → Core: [标准帧] CMD=0x10 EXT=0x09 FILE_SAVE
   DATA: [文件路径字符串...]

② Core → Display: [标准帧] ACK + 传输参数
   DATA[0..3]:  允许传输的最大大小（32位大端）
   DATA[4..5]:  块大小（16位大端，默认 20480）

③ [进入 Bulk Mode - 数据阶段]
   Display → Core: [Raw] Block 0（20KB 原始数据）
   Display → Core: [Raw] Block 1（20KB 原始数据）
   ...
   Display → Core: [Raw] Block N（最后一块）

④ Core → Display: [标准帧] CMD=0x10 EXT=0x1A BULK_TRANSFER
   DATA[0]:   完成标记（0x01 = 接收完毕）
   DATA[1..4]: 实际接收总大小（32位大端）
   DATA[5]:   保存结果（0=成功, 1=失败）
```

### 4.5 注意事项

- Bulk Mode 期间 UART 总线被独占，不应插入其他标准协议帧
- 发送方在发送 Raw 数据前必须确保上一帧标准协议已完全发送（TC 标志置位）
- 接收方在进入 Bulk Mode 前需配置好 DMA 和 IDLE 中断
- 若文件大小 ≤ 块大小，仅发送一块即可

---

## 5. 数据格式详解

### 5.1 统一输入事件格式（CMD 0x15）

统一输入事件仿照 USB HID 报告格式，按设备类型分为三种报告。坐标系采用**左上角为原点 (0,0)，X 轴向右增长，Y 轴向下增长**的屏幕坐标系。

**帧结构**：
```
DATA[0]: 设备类型（1字节）
  - 0x00: 键盘（来自 Keyboard 模块或 CH9350）
  - 0x01: 鼠标（来自 CH9350）
  - 0x02: 触摸（来自触摸屏控制器）
  - 0x03: 虚拟触摸（Core 模拟）

DATA[1..N]: 输入报告数据（N = LEN - 2，由帧 LEN 推导）
```

> **说明**：每种设备类型的报告长度固定，接收方通过 `DATA[0]` 设备类型识别报告格式，报告总长度由 `LEN - 2` 推导，无需额外长度字段。

---

**键盘报告（设备类型 = 0x00，报告长度 = 8 字节）**：

仿 USB HID Keyboard Boot Report。

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

---

**鼠标报告（设备类型 = 0x01，报告长度 = 4 字节）**：

仿 USB HID Mouse Boot Report。

```
DATA[1]: 按钮状态（bitmask）
  - bit0: 左键    - bit1: 右键    - bit2: 中键
DATA[2]: X 轴偏移（int8，-128~127，正值向右）
DATA[3]: Y 轴偏移（int8，-128~127，正值向下）
DATA[4]: 滚轮偏移（int8，正值为远离用户/向上滚动）
```

---

**触摸/虚拟触摸报告（设备类型 = 0x02 / 0x03，报告长度 = 7 字节）**：

```
DATA[1]: 触摸点 ID（0~9，单点触摸填 0）
DATA[2]: 事件类型
  - 0x00: 按下    - 0x01: 移动    - 0x02: 释放
  - 0x03: 长按    - 0x04: 双击
DATA[3..4]: X 坐标（uint16 大端，0~65535，左上角为原点，向右增长）
DATA[5..6]: Y 坐标（uint16 大端，0~65535，左上角为原点，向下增长）
DATA[7]: 压力值（0~255，0xFF 表示不支持压力检测）
```

> **坐标映射**：Display 端根据实际屏幕分辨率将 16 位归一化坐标线性映射到物理像素。例如 800×480 LCD 屏幕，X=32768 对应物理 X=400。

### 5.2 屏幕控制格式（CMD 0x18）

```
DATA[0]: 控制类型
  - 0x00: 立即息屏
  - 0x01: 立即亮屏
  - 0x02: 设置自动息屏开关
  - 0x03: 设置息屏超时时间
DATA[1]: 参数
  - 控制类型 0x02: 0x00=关, 0x01=开
  - 控制类型 0x03: 超时秒数高字节
DATA[2]: 超时秒数低字节（仅控制类型 0x03）
```

### 5.3 屏幕状态响应格式（CMD 0x19 ACK）

```
DATA[0]: 状态位掩码
  - bit0: 是否息屏（0=亮屏, 1=息屏）
  - bit1: 自动息屏开关（0=关, 1=开）
  - bit2: 背光亮度有效
  - bit3: 旋转角度有效
DATA[1]: 背光亮度（0-100，若 bit2=1）
DATA[2]: 旋转角度（0/90/180/270，若 bit3=1）
DATA[3..4]: 息屏超时秒数（16位大端，若 bit1=1）
```

### 5.4 状态栏更新格式（CMD 0x14）

```
DATA[0]: 更新掩码（1字节，位域指示哪些字段有效）
  - bit0: 电量    - bit1: 蓝牙状态    - bit2: 以太网状态
  - bit3: 时间    - bit4: 日期        - bit5: 核心模块连接状态
  - bit6: 音量    - bit7: 预留
DATA[1]: 电量百分比（1字节，0-100，若 bit0=1）
DATA[2]: 蓝牙状态（1字节，0=断开, 1=已连接, 2=配对中，若 bit1=1）
DATA[3]: 以太网状态（1字节，0=断开, 1=已连接, 2=连接中，若 bit2=1）
DATA[4..7]: 时间戳（4字节，Unix 时间秒数，32位大端，若 bit3=1）
DATA[8..11]: 日期（4字节，YYYYMMDD 格式，32位大端，若 bit4=1）
DATA[12]: 核心模块状态（1字节，0=离线, 1=在线, 2=休眠，若 bit5=1）
DATA[13]: 音量（1字节，0-100，若 bit6=1）
```

### 5.5 模块状态格式（扩展码 0x04 / 0x05）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x04（主动上报）或 0x05（响应查询）
DATA[2]: 模块数量 N（1字节）
DATA[3..6]:  模块1信息 [模块ID:1][类型:1][子类型:1][在线状态:1]
DATA[7..10]: 模块2信息 ...
...
在线状态: 0=离线, 1=在线, 2=错误, 3=休眠
```

> 每个模块信息固定 4 字节，模块数量 `N = (LEN - 3) / 4`。

### 5.6 文件列表格式（扩展码 0x07）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x07
DATA[2]: 文件数量 M（1字节）
DATA[3..23]:  文件1信息 [类型:1][名称:16字节][大小:4字节（32位大端）]
DATA[24..44]: 文件2信息 ...
...
```

> 每个文件信息固定 **21 字节**：类型(1) + 名称(16，不足补零) + 文件大小(4)。
> 文件数量 `M = (LEN - 3) / 21`。
> 若文件列表总长度超过单帧容量（255 字节 DATA），应通过 `BULK_TRANSFER` 分批传输。

### 5.7 音乐播放控制格式（基础码 0x1B / 扩展码 0x0B）

**基础码 0x1B（播放控制命令）**：
```
DATA[0]: 控制类型
  - 0x00: 播放
  - 0x01: 暂停
  - 0x02: 继续
  - 0x03: 停止
  - 0x04: 下一首
  - 0x05: 上一首
```

**扩展码 0x0B（请求播放指定文件）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x0B
DATA[2..N]: 文件路径字符串（变长，UTF-8 编码，如 "/music/song.wav"）
```

> 路径长度由 `LEN` 推导：`路径长度 = LEN - 1 - 2`。

### 5.8 音乐状态上报格式（基础码 0x1C）

```
DATA[0]: 播放状态（1字节，0x00=停止, 0x01=播放中, 0x02=暂停）
DATA[1..4]: 当前进度（4字节，毫秒，32位大端）
DATA[5..8]: 总时长（4字节，毫秒，32位大端）
DATA[9]: 当前音量（1字节，0-100）
DATA[10]: 播放模式（1字节，0x00=顺序, 0x01=随机, 0x02=单曲循环, 0x03=列表循环）
DATA[11..N]: 当前歌曲名（变长，UTF-8 编码）
```

> 歌曲名长度由 `LEN` 推导：`歌曲名长度 = LEN - 1 - 10`。

### 5.9 蓝牙设备格式（扩展码 0x0D）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x0D
DATA[2]: 设备数量 N（1字节）
DATA[3]:      设备1状态（1字节，0x00=已配对, 0x01=已连接, 0x02=发现中）
DATA[4..19]:  设备1名称（16字节，不足补零，UTF-8 编码）
DATA[20..25]: 设备1 MAC 地址（6字节）
DATA[26]:     设备2状态 ...
... 后续设备
```

> 每个设备信息固定 **23 字节**：状态(1) + 名称(16) + MAC(6)。
> 设备数量 `N = (LEN - 3) / 23`。

### 5.10 配置保存/加载格式（扩展码 0x16 / 0x17 / 0x18）

**请求保存（Display → Core，0x16）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x16
DATA[2..N]: 文件路径（变长，UTF-8 编码，如 "/config/app.cfg"）
```

> 配置数据本身通过以下方式传输：
> - 若配置数据 **≤ 200 字节**：直接附加在 `SAVE_CONFIG` 帧的路径之后（单帧内完成），总长度由 `LEN` 推导。
> - 若配置数据 **> 200 字节**：`SAVE_CONFIG` 帧仅携带路径，后续通过 `BULK_TRANSFER(0x1A)` 传输配置数据。

**请求加载（Display → Core，0x17）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x17
DATA[2..N]: 文件路径（变长）
```

**结果响应（Core → Display，0x18）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x18
DATA[2]: 结果（1字节，0=成功, 1=失败）
DATA[3]: 错误码（1字节，仅失败时有效，成功时填 0x00）
DATA[4..N]: 配置数据（变长，加载成功时携带，长度由 LEN 推导）
```

> 加载成功时配置数据长度由 `LEN` 推导：`配置数据长度 = LEN - 1 - 3`。
> 若配置数据 > 200 字节，Core 在结果响应中仅返回前 200 字节摘要，完整数据通过 `BULK_TRANSFER` 传输。

### 5.11 以太网状态格式（基础码 0x1E）

```
DATA[0]: 连接状态（0x00=断开, 0x01=连接中, 0x02=已连接）
DATA[1..4]: IP 地址（4字节）
DATA[5..8]: 子网掩码（4字节）
DATA[9..12]: 网关（4字节）
DATA[13..18]: MAC 地址（6字节）
```

### 5.12 副屏控制格式（扩展码 0x1B / 0x1C）

**副屏内容设置（0x1B）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x1B
DATA[2]: 内容类型（1字节，0x00=关闭, 0x01=镜像主屏, 0x02=自定义内容, 0x03=状态信息）
DATA[3..N]: 内容数据（变长，根据内容类型定义，长度由 LEN 推导）
```

**副屏配置（0x1C）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x1C
DATA[2]: 分辨率（1字节，0x00=800x480, 0x01=640x480, 0x02=自定义）
DATA[3]: 方向（1字节，0x00=0°, 0x01=90°, 0x02=180°, 0x03=270°）
DATA[4]: 开关（1字节，0x00=关, 0x01=开）
```

### 5.13 屏幕唤醒/休眠事件格式（扩展码 0x1D / 0x1E）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x1D（唤醒）或 0x1E（休眠）
DATA[2]: 事件源
  - 0x00: 触摸唤醒
  - 0x01: 按键唤醒
  - 0x02: UART 命令唤醒
  - 0x03: 定时唤醒
  - 0x04: 自动息屏超时休眠
  - 0x05: 命令休眠
  - 0x06: 省电模式休眠
```

---

## 6. 应用 ID 定义

| 应用ID | 应用名称 | 说明 |
| --- | --- | --- |
| `0x00` | 保留 | 系统保留 |
| `0x01` | Music | 音乐播放 |
| `0x02` | File | 文件管理 |
| `0x03` | Editor | 文档编辑 |
| `0x04` | Images | 图片查看 |
| `0x05` | USB | USB连接 |
| `0x06` | Power | 电源管理 |
| `0x07` | BT | 蓝牙管理 |
| `0x08` | NFC | NFC功能 |
| `0x09` | Fingerprint | 指纹识别 |
| `0x0A` | Health | 健康监测 |
| `0x0B` | SubDisplay | 副屏管理 |
| `0x0C` | Lights | 灯效控制 |
| `0x0D` | IRRange | 红外测距 |
| `0x0E` | EBook | 电子书阅读 |
| `0x0F` | EMusic | 电子音乐/合成器 |
| `0x10` | Tetris | 俄罗斯方块（本地游戏） |
| `0x11` | 2048 | 2048游戏（本地游戏） |
| `0x12` | Snake | 贪吃蛇（本地游戏） |
| `0x13` | Breakout | 打砖块（本地游戏） |
| `0x14~0x7F` | 预留 | 未来扩展 |
| `0x80~0xFF` | 用户自定义 | 用户自定义应用 |

---

## 7. 页面编号定义

| 页面编号 | 页面名称 |
| --- | --- |
| `0x00` | 首页 (Main) |
| `0x01` | 应用界面 (Software) |
| `0x02` | 模块界面 (Models) |
| `0x03` | 设置界面 (Options) |
| `0x04~0x0F` | 预留 |
| `0x10~0x1F` | 游戏页面（对应游戏ID） |
| `0x20~0x7F` | 应用页面（对应应用ID） |
| `0x80~0xFF` | 用户自定义页面 |

---

## 8. CMD_GET_TYPE 响应

Display 模块响应 `CMD_GET_TYPE` 时，`CMD_ACK` 的 DATA 格式如下：

| 字节 | 字段 | 值/说明 |
|------|------|---------|
| DATA[0] | 模块类型编号 | `0x01` |
| DATA[1] | 模块子类型编号 | 见下表 |
| DATA[2] | 硬件版本号 | 由具体屏幕硬件决定 |
| DATA[3] | 固件主版本号 | Display 固件主版本 |
| DATA[4] | 固件次版本号 | Display 固件次版本 |
| DATA[5..N] | 扩展信息 | 屏幕分辨率、触摸支持等能力位（可选） |

**Display 子类型定义**

> 代码宏定义：`protocol.h` → `MODULE_SUBTYPE_DISPLAY_*`

| 子类型编号 | 宏名 | 实现名称 | 说明 |
|------------|------|----------|------|
| `0x00` | `MODULE_SUBTYPE_DISPLAY_RESERVED` | 保留 | 系统保留 |
| `0x01` | `MODULE_SUBTYPE_DISPLAY_LCD` | LCD | 液晶显示屏（当前主方案） |
| `0x02` | `MODULE_SUBTYPE_DISPLAY_EINK` | E-ink | 电子墨水屏 |
| `0x03~0xFF` | — | 预留 | 未来扩展 |

---

## 9. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| V1.0 | 2026-04-22 | 初始版本 |
| V2.0 | 2026-04-24 | 重构操作码：删除二维码/WiFi/OTA/系统信息/图片独立码；新增统一输入事件、批量传输协议、以太网状态、RGB控制 |
