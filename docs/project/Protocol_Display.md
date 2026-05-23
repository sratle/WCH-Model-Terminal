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

- **HEAD**：固定 `0xAA`。仅在接收状态机处于"等待帧头"时识别为新帧起始，数据域中的 `0xAA` 不会触发重解析。
- **LEN**：`CMD + DATA` 的总字节数，即 `N = LEN - 1`，范围 `1 ~ 255`。`LEN = 1` 表示 DATA 为空。
- **DATA**：数据域，长度由 LEN 推导，最大 255 字节。
- **TAIL**：固定序列 `A5 5A FC FD`，任一字节不匹配则丢弃整帧。

**流式帧结构**（用于大数据量传输，DATA 长度不受 255 字节限制）：

```
+--------+--------+--------+--------+--------+-------------+--------+--------+--------+--------+
|  HEAD  |  SRC   |  DST   |  0xFF  |  CMD   |  DATA[0..N] | TAIL0  | TAIL1  | TAIL2  | TAIL3  |
| 1 byte | 1 byte | 1 byte | 1 byte | 1 byte |  N bytes    | 1 byte | 1 byte | 1 byte | 1 byte |
+--------+--------+--------+--------+--------+-------------+--------+--------+--------+--------+
```

- **LEN = 0xFF**：特殊值，表示本帧为**流式帧**，DATA 长度不由 LEN 字段决定，而是通过扫描帧尾序列 `A5 5A FC FD` 来确定。
- **DATA 最大长度**：受实现限制，建议单帧不超过 20KB。
- **帧尾冲突处理**：若 DATA 中偶然出现与帧尾相同的字节序列，接收方状态机需支持**回退解析**（详见 1.2 节）。

### 1.2 流式帧尾扫描与回退机制

当 LEN = 0xFF 时，接收状态机不预先计算 DATA 长度，而是逐字节扫描帧尾序列。为避免 DATA 中偶然出现的帧尾字节被误识别，状态机采用以下回退策略：

```
WAIT_STREAM_DATA:
  - 收到 TAIL0 (0xA5)：进入 TENTATIVE_TAIL1，暂存该字节位置
  - 收到其他字节：存入 DATA 缓冲区

TENTATIVE_TAIL1:
  - 收到 TAIL1 (0x5A)：进入 TENTATIVE_TAIL2
  - 收到其他字节：将之前暂存的 TAIL0 和当前字节均存入 DATA，返回 WAIT_STREAM_DATA

TENTATIVE_TAIL2:
  - 收到 TAIL2 (0xFC)：进入 TENTATIVE_TAIL3
  - 收到其他字节：将之前暂存的 TAIL0、TAIL1 和当前字节均存入 DATA，返回 WAIT_STREAM_DATA

TENTATIVE_TAIL3:
  - 收到 TAIL3 (0xFD)：帧完整，提交处理
  - 收到其他字节：将之前暂存的 TAIL0、TAIL1、TAIL2 和当前字节均存入 DATA，返回 WAIT_STREAM_DATA
```

> 此机制保证 DATA 中即使出现完整的 `A5 5A FC FD` 序列（概率极低，约 2.3e-10），也只会提前结束当前帧。应用层可通过帧完整性校验（如下一帧 HEAD 是否正确）检测并恢复。

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
| `0x15` | `CMD_DISP_INPUT_EVENT` | Core -> Display | 统一输入事件（键盘/鼠标/触摸/Core按键/BLE HID） | 见下方输入事件格式 |
| `0x16` | `CMD_DISP_SET_ROTATION` | Core -> Display | 设置屏幕旋转角度 | `[角度: 0/90/180/270]` |
| `0x17` | `CMD_DISP_GET_ROTATION` | Core -> Display | 获取当前屏幕旋转角度 | 无，Display 回复 ACK + 角度 |
| `0x18` | `CMD_DISP_SCREEN_CONTROL` | Core -> Display | 屏幕控制（息屏/亮屏/自动息屏/超时） | 见下方屏幕控制格式 |
| `0x19` | `CMD_DISP_GET_SCREEN_STATE` | Core -> Display | 获取屏幕当前状态 | 无，Display 回复 ACK + 状态 |
| `0x1A` | `CMD_DISP_SHOW_NOTICE` | Core -> Display | 显示通知弹窗 | `[优先级:1][标题:16字节][内容:变长]` |
| `0x1B` | `CMD_DISP_MUSIC_CONTROL` | Display -> Core | 音乐播放控制 | `[控制类型:1]` |
| `0x1C` | `CMD_DISP_MUSIC_STATUS` | Core -> Display | 音乐播放状态上报 | `[播放状态:1][当前时间:4][总时长:4][音量:1][曲目名:变长]` |
| `0x1D` | `CMD_DISP_VOLUME_CONTROL` | Display -> Core | 音量设置/获取 | `[操作: 0=设置, 1=查询][音量值(设置时)]` |
| `0x1E` | `CMD_DISP_FACTORY_RESET` | Core -> Display | 恢复出厂设置 | `[确认码: 0xA5]` |
| `0x1F` | — | — | 预留 | — |

> **已移除操作码**：原 `0x1E CMD_DISP_ETHERNET_STATUS`（以太网状态上报）已移除。Core 硬件虽有以太网口，但优先级低且当前未启用，不在 Display 协议中定义。原 `0x1F FACTORY_RESET` 移至 `0x1E`，`0x1F` 预留。

### 2.2 扩展操作码（CMD = 0x10，DATA[0] 为子命令）

| 扩展码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x01` | `CMD_DISP_EXT_APP_LAUNCH` | Core -> Display | 启动指定应用 | `[应用ID]` |
| `0x02` | `CMD_DISP_EXT_APP_CLOSE` | Core -> Display | 关闭当前应用 | 无 |
| `0x03` | `CMD_DISP_EXT_APP_DATA` | 双向 | 应用数据传输 | `[应用ID:1][数据类型:1][数据:变长]` |
| `0x04` | `CMD_DISP_EXT_MODULE_STATUS` | Core -> Display | 模块状态主动上报（插拔/在线/离线） | 见下方模块状态格式 |
| `0x05` | `CMD_DISP_EXT_GET_MODULE_STATUS` | Display -> Core | 请求获取当前模块组成信息 | 无，Core 回复 `0x04` |
| `0x06` | `CMD_DISP_EXT_REQUEST_FILE_LIST` | Display -> Core | 请求当前目录文件列表 | `[路径:变长]` |
| `0x07` | `CMD_DISP_EXT_FILE_LIST` | Core -> Display | 文件列表数据响应 | 见下方文件列表格式 |
| `0x08` | `CMD_DISP_EXT_FILE_READ` | Display -> Core | 请求读取文件（触发 Bulk Mode） | `[路径:变长]` |
| `0x09` | `CMD_DISP_EXT_FILE_SAVE` | Display -> Core | 请求保存文件（触发 Bulk Mode） | `[路径:变长]` |
| `0x0A` | `CMD_DISP_EXT_FILE_OPERATION` | Display -> Core | 文件操作（创建/删除/重命名） | `[操作类型:1][路径:变长]` |
| `0x0B` | `CMD_DISP_EXT_PLAY_MUSIC` | Display -> Core | 请求播放指定 wav 文件 | `[路径:变长]` |
| `0x0C` | `CMD_DISP_EXT_BT_EVENT` | Core -> Display | 蓝牙事件转发（连接/断开/扫描结果） | 见下方蓝牙事件格式 |
| `0x0D` | `CMD_DISP_EXT_BT_CONTROL` | Display -> Core | 蓝牙控制请求（扫描/连接/断开） | `[控制类型:1][参数:变长]` |
| `0x0E` | `CMD_DISP_EXT_SUBMODEL_EVENT` | Core -> Display | Submodel 事件转发（指纹/NFC/健康/红外/触摸/RGB） | `[子类型:1][事件数据:变长]` |
| `0x0F` | `CMD_DISP_EXT_POWER_EVENT` | Core -> Display | 电源事件转发（充电/告警） | `[事件类型:1][事件数据:变长]` |
| `0x10` | `CMD_DISP_EXT_SAVE_CONFIG` | Display -> Core | 请求保存配置到 TF 卡 | `[路径:变长]` |
| `0x11` | `CMD_DISP_EXT_LOAD_CONFIG` | Display -> Core | 请求加载配置文件 | `[路径:变长]` |
| `0x12` | `CMD_DISP_EXT_CONFIG_RESULT` | Core -> Display | 配置保存/加载结果 | `[结果:1][错误码:1][配置数据:变长]` |
| `0x13` | `CMD_DISP_EXT_SET_RGB_MODE` | Display -> Core | 设置 RGB 灯效模式 | `[灯效ID][颜色R][颜色G][颜色B][亮度][速度]` |
| `0x14` | `CMD_DISP_EXT_BULK_TRANSFER` | 双向 | 批量传输控制/确认 | 见下方批量传输协议 |
| `0x15` | `CMD_DISP_EXT_SUBDISP_CONTENT` | Core -> Display | 副屏显示内容设置 | `[内容类型:1][内容数据:变长]` |
| `0x16` | `CMD_DISP_EXT_SUBDISP_CONFIG` | Core -> Display | 副屏配置 | `[分辨率][方向][开关]` |
| `0x17` | `CMD_DISP_EXT_ERROR_REPORT` | Display -> Core | Display 错误/异常上报 | `[错误码:1][错误信息:变长]` |
| `0x18` | `CMD_DISP_EXT_HID_STATUS` | Core -> Display | 外接 HID 设备连接/断开状态同步 | `[ext_code:1][事件:1][设备类型:1]` |
| `0x19` | `CMD_DISP_EXT_CD` | Display -> Core | 切换 CH378 工作目录（仿 cd） | `[路径:变长]`，Core 回复 ACK + CWD 或 NACK |
| `0x1A` | `CMD_DISP_EXT_CLI` | 双向 | CLI 命令直通（Display 发命令，Core 执行并返回文本） | Display→Core: `[命令字符串]`；Core→Display: ACK + `[输出文本]` |
| `0x1B~0x3F` | — | — | 预留 | — |

> **已移除/合并的扩展码**：
> - 原 `0x0C~0x0F`（BT_SCAN/BT_DEVICE_LIST/BT_CONNECT/BT_STATUS）：蓝牙操作由 Core 通过 CH585F（SPI）统一管理，Display 不直接与 CH585F 通信。合并为 `0x0C BT_EVENT`（Core 转发事件）和 `0x0D BT_CONTROL`（Display 请求 Core 代为执行）。
> - 原 `0x10~0x12`（FP_STATUS/NFC_TAG/HEALTH_DATA）：这些是 Submodel 事件，由 Core 转发给 Display，统一合并为 `0x0E SUBMODEL_EVENT`，通过子类型字段区分。
> - 原 `0x13 POWER_STATUS`：合并为 `0x0F POWER_EVENT`，与 Power 协议对齐。
> - 原 `0x14~0x15`（IR_RANGE_REQ/IR_RANGE_DATA）：红外测距是 Submodel-6 的功能，通过 `0x0E SUBMODEL_EVENT` 转发。
> - 原 `0x1D SCREEN_WAKEUP` / `0x1E SCREEN_SLEEP`：与基础操作码 `0x18 SCREEN_CONTROL` 功能重叠，移除。屏幕唤醒/休眠由 Core 通过 `0x18` 统一下发。
> - 原 `0x20 REPORT_EVENT` / `0x21 REQUEST_DATA`：过于笼统，具体事件已有专门的操作码承载，移除。

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
| `0x15` | INPUT_EVENT | 事件 | Display 不回复 |
| `0x16` | SET_ROTATION | 设置 | 发送即忘，Display 不回复 |
| `0x17` | GET_ROTATION | 查询 | Display 回复 ACK + `[角度:1]` |
| `0x18` | SCREEN_CONTROL | 设置 | 发送即忘，Display 不回复 |
| `0x19` | GET_SCREEN_STATE | 查询 | Display 回复 ACK + `[状态:4]` |
| `0x1A` | SHOW_NOTICE | 设置 | 发送即忘，Display 不回复 |
| `0x1B` | MUSIC_CONTROL | 动作 | Core 回复 ACK 空 或 NACK |
| `0x1C` | MUSIC_STATUS | 事件 | Display 不回复 |
| `0x1D` | VOLUME_CONTROL | 动作/查询 | Core 回复 ACK（查询时带音量）或 NACK |
| `0x1E` | FACTORY_RESET | 设置 | 发送即忘，Display 不回复 |
| `0x1F` | — | — | 预留 |

### 3.3 扩展操作码响应要求

| 码 | 命令 | 语义 | 接收方行为 |
|---|---|---|---|
| `0x01` | APP_LAUNCH | 设置 | 发送即忘，Display 不回复 |
| `0x02` | APP_CLOSE | 设置 | 发送即忘，Display 不回复 |
| `0x03` | APP_DATA | 数据通道 | **应用层自定义**，协议层不强制 ACK |
| `0x04` | MODULE_STATUS | 事件 | Display 不回复 |
| `0x05` | GET_MODULE_STATUS | 查询 | Core 回复 ACK + `[模块列表:变长]` |
| `0x06` | REQUEST_FILE_LIST | 查询 | Core 回复 `0x07` FILE_LIST |
| `0x07` | FILE_LIST | 查询响应 | 发送即忘（作为 0x06 的响应） |
| `0x08` | FILE_READ | 动作 | Core 回复 ACK + `[Bulk参数:6]` 或 NACK |
| `0x09` | FILE_SAVE | 动作 | Core 回复 ACK + `[Bulk参数:6]` 或 NACK |
| `0x0A` | FILE_OPERATION | 动作 | Core 回复 ACK 空 或 NACK |
| `0x0B` | PLAY_MUSIC | 动作 | Core 回复 ACK 空 或 NACK |
| `0x0C` | BT_EVENT | 事件 | Display 不回复 |
| `0x0D` | BT_CONTROL | 动作 | Core 回复 ACK 空 或 NACK |
| `0x0E` | SUBMODEL_EVENT | 事件 | Display 不回复 |
| `0x0F` | POWER_EVENT | 事件 | Display 不回复 |
| `0x10` | SAVE_CONFIG | 动作 | Core 回复 ACK 空 或 NACK |
| `0x11` | LOAD_CONFIG | 查询 | Core 回复 ACK + `[配置数据:变长]` 或 NACK |
| `0x12` | CONFIG_RESULT | 事件 | Display 不回复 |
| `0x13` | SET_RGB_MODE | 动作 | Core 回复 ACK 空 或 NACK |
| `0x14` | BULK_TRANSFER | 控制 | 接收方回复 ACK 空（握手/确认帧） |
| `0x15` | SUBDISP_CONTENT | 设置 | 发送即忘 |
| `0x16` | SUBDISP_CONFIG | 设置 | 发送即忘 |
| `0x17` | ERROR_REPORT | 事件 | Core 不回复 |
| `0x18` | HID_STATUS | 事件 | Display 不回复 |
| `0x19` | CD | 动作 | Core 回复 ACK + `[CWD字符串]` 或 NACK |
| `0x1A` | CLI | 动作 | Core 回复 ACK + `[命令输出文本]` 或 NACK |

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

#### 4.4.1 文件读取（Core -> Display）

```
① Display -> Core: [标准帧] CMD=0x10 EXT=0x08 FILE_READ
   DATA: [文件路径字符串...]

② Core -> Display: [标准帧] ACK + 传输参数
   DATA[0..3]:  总大小（32位大端，单位字节）
   DATA[4..5]:  块大小（16位大端，默认 20480）

③ [进入 Bulk Mode - 数据阶段]
   Core -> Display: [Raw] Block 0（20KB 原始数据）
   Core -> Display: [Raw] Block 1（20KB 原始数据）
   ...
   Core -> Display: [Raw] Block N（最后一块，可能不足 20KB）

④ Display -> Core: [标准帧] CMD=0x10 EXT=0x14 BULK_TRANSFER
   DATA[0]:   完成标记（0x01 = 接收完毕）
   DATA[1..4]: 实际接收总大小（32位大端）

⑤ Core -> Display: [标准帧] ACK（结束）
```

#### 4.4.2 文件保存（Display -> Core）

```
① Display -> Core: [标准帧] CMD=0x10 EXT=0x09 FILE_SAVE
   DATA: [文件路径字符串...]

② Core -> Display: [标准帧] ACK + 传输参数
   DATA[0..3]:  允许传输的最大大小（32位大端）
   DATA[4..5]:  块大小（16位大端，默认 20480）

③ [进入 Bulk Mode - 数据阶段]
   Display -> Core: [Raw] Block 0（20KB 原始数据）
   Display -> Core: [Raw] Block 1（20KB 原始数据）
   ...
   Display -> Core: [Raw] Block N（最后一块）

④ Core -> Display: [标准帧] CMD=0x10 EXT=0x14 BULK_TRANSFER
   DATA[0]:   完成标记（0x01 = 接收完毕）
   DATA[1..4]: 实际接收总大小（32位大端）
   DATA[5]:   保存结果（0=成功, 1=失败）
```

### 4.5 注意事项

- Bulk Mode 期间 UART 总线被独占，不应插入其他标准协议帧
- 发送方在发送 Raw 数据前必须确保上一帧标准协议已完全发送（TC 标志置位）
- 接收方在进入 Bulk Mode 前需配置好 DMA 和 IDLE 中断
- 若文件大小 ≤ 块大小，仅发送一块即可
- CH378 文件系统为 FAT32，支持长文件名，文件路径使用 UTF-8 编码

---

## 5. 数据格式详解

### 5.1 统一输入事件格式（CMD 0x15）

Core 将所有输入源的事件统一打包后发送给 Display。输入源包括：

| 来源 | 硬件 | 说明 |
|------|------|------|
| CH9350 键盘 | USB-A 口外接 HID 键盘 | 通过 UART1 接收，V3F 解析后转发 |
| CH9350 鼠标 | USB-A 口外接 HID 鼠标 | 通过 UART1 接收，V3F 解析后转发 |
| BLE HID 键盘 | CH585F 连接的 BLE 键盘 | CH585F 通过 SPI 上报 `CMD_BT_HID_REPORT`，V5F 转发 |
| BLE HID 鼠标 | CH585F 连接的 BLE 鼠标 | CH585F 通过 SPI 上报 `CMD_BT_HID_REPORT`，V5F 转发 |
| Core 按键 | 板载 `+`/`-`/`Enter` 三键 | V3F GPIO 扫描，V3F/V5F 转发 |
| 触摸屏 | Display 模块触摸控制器 | Display 自行处理，不经过此通道 |
| Keyboard 模块 | Keyboard-1/2/3 | 通过 UART3 接收，V3F 转发 |

> **说明**：BLE HID 事件由 CH585F 通过 `CMD_BT_HID_REPORT`（0x56）上报给 Core，Core 统一转换为 `CMD_DISP_INPUT_EVENT`（0x15）格式后转发给 Display。Display 无需感知蓝牙层。

**帧结构**：
```
DATA[0]: 设备类型（1字节）
  - 0x00: 键盘（来自 CH9350 或 BLE HID 键盘或 Keyboard 模块）
  - 0x01: 鼠标（来自 CH9350 或 BLE HID 鼠标）
  - 0x02: 触摸（来自触摸屏控制器，Display 自行处理，通常不经过此通道）
  - 0x03: Core 按键（板载 `+`/`-`/`Enter` 三键）

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
> 此格式统一适用于 CH9350 键盘、BLE HID 键盘和 Keyboard-1/2 模块的 HID 报告。

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

> 此格式统一适用于 CH9350 鼠标和 BLE HID 鼠标。CH9350 工作在状态 2（相对坐标模式）。

---

**Core 按键报告（设备类型 = 0x03，报告长度 = 2 字节）**：

Core 板载三个物理按键 `+`/`-`/`Enter` 的状态上报。

```
DATA[1]: 按键事件（1字节）
  - 0x00: 释放
  - 0x01: 按下
  - 0x02: 长按
DATA[2]: 按键标识（1字节）
  - 0x01: `+` 键（短按=音量增加/向上滚动/数值增加）
  - 0x02: `-` 键（短按=音量减小/向下滚动/数值减小）
  - 0x03: `Enter` 键（短按=确认/进入，长按=系统唤醒）
```

> Core 按键由 V3F 通过 GPIO 扫描获取（`Key_Scan()`），V3F/V5F 将按键事件打包为输入事件发送给 Display。长按检测由 Core 固件实现，Display 只需响应按下/长按/释放事件。

---

**触摸报告（设备类型 = 0x02，报告长度 = 7 字节）**：

> 触摸事件通常由 Display 模块自行处理（触摸屏控制器直连 Display MCU），不经过 Core 转发。此格式保留用于 Display 内部或特殊场景。

```
DATA[1]: 触摸点 ID（0~9，单点触摸填 0）
DATA[2]: 事件类型
  - 0x00: 按下    - 0x01: 移动    - 0x02: 释放
  - 0x03: 长按    - 0x04: 双击
DATA[3..4]: X 坐标（uint16 大端，0~65535，左上角为原点，向右增长）
DATA[5..6]: Y 坐标（uint16 大端，0~65535，左上角为原点，向下增长）
DATA[7]: 压力值（0~255，0xFF 表示不支持压力检测）
```

### 5.2 外接 HID 设备状态同步（扩展码 0x18）

Core 通过 CH9350 芯片（USB-A 口）检测外接 USB 键盘/鼠标的连接与断开。由于 CH9350 工作在状态 2，不存在设备连接帧，Core 在收到第一条 HID 数据帧时判断设备类型并通知 Display；收到设备断开帧时通知 Display 设备已断开。

**同一时刻只能外接一个 HID 设备**（键盘或鼠标），切换设备时 Core 先发送旧设备断开，再发送新设备连接。

**帧格式**（CMD = `0x10`，DATA[0] = `0x18`）：

```
DATA[0]: 扩展操作码 = 0x18 (CMD_DISP_EXT_HID_STATUS)
DATA[1]: 事件类型
  - 0x00: 设备断开 (HID_EVT_DISCONNECTED)
  - 0x01: 设备连接 (HID_EVT_CONNECTED)
DATA[2]: 设备类型
  - 0x01: 外接键盘 (HID_DEV_KEYBOARD)
  - 0x02: 外接鼠标 (HID_DEV_MOUSE)
```

**Display 侧处理**：
- 收到设备连接事件：更新 `g_settings.ext_keyboard_connected` 或 `g_settings.ext_mouse_connected`，若为鼠标连接则初始化光标位置并显示光标
- 收到设备断开事件：清除对应状态，若为鼠标断开则隐藏光标
- Display 不回复此事件

**Core 侧发送时机**：
- 收到第一条键盘数据帧（`CH9350_DEV_KEYBOARD`）且 `connected_dev_type` 不是键盘时：发送键盘连接；若之前是鼠标，先发送鼠标断开
- 收到第一条鼠标数据帧（`CH9350_DEV_MOUSE_REL`）且 `connected_dev_type` 不是鼠标时：发送鼠标连接；若之前是键盘，先发送键盘断开
- 收到 `CH9350_OP_DEV_DISCONNECT` 时：根据 `connected_dev_type` 发送对应设备断开

### 5.3 音乐播放控制格式（CMD 0x1B）

Display 向 Core 发送音乐控制命令。Core 的音频系统由 CS43131 DAC 驱动，CH32H417 作为 I2S Slave 通过 DMA 向 CS43131 发送音频数据，支持 WAV 文件流式播放。

```
DATA[0]: 控制类型（1字节）
  - 0x00: 播放/暂停切换
  - 0x01: 播放
  - 0x02: 暂停
  - 0x03: 停止
  - 0x04: 上一首
  - 0x05: 下一首
  - 0x06: 快进（DATA[1] = 秒数，1字节）
  - 0x07: 快退（DATA[1] = 秒数，1字节）
  - 0x08: 设置播放模式（DATA[1] = 模式: 0=单曲, 1=顺序, 2=随机）
```

> Core 收到控制命令后操作 CS43131 音频子系统（`Audio_PlayStart`/`Audio_Pause`/`Audio_Resume`/`Audio_PlayStop`）。播放的音频文件来自 CH378 管理的 TF 卡或 U 盘（WAV 格式，44.1kHz 16bit 双声道）。

### 5.4 音乐播放状态格式（CMD 0x1C）

Core 主动向 Display 上报音乐播放状态。状态变化时上报（如播放、暂停、曲目切换、播放完成），Display 也可通过查询获取。

```
DATA[0]: 播放状态（1字节）
  - 0x00: 空闲（IDLE）
  - 0x01: 播放中（PLAYING）
  - 0x02: 暂停（PAUSED）
  - 0x03: 已停止（STOPPED）
DATA[1..4]: 当前播放时间（uint32 大端，单位毫秒）
DATA[5..8]: 总时长（uint32 大端，单位毫秒，0 表示未知）
DATA[9]: 音量（0~100）
DATA[10..N]: 当前曲目名称（UTF-8 字符串，变长，N 最大 63）
```

> 播放时间由 `Audio_GetPlayTime_ms()` 获取，基于 DMA 传输的声道样本数计算（Stereo 44.1kHz = 88200 样本/秒）。曲目名称由 `Audio_GetCurrentTrackName()` 获取，最长 63 字节。

### 5.5 音量控制格式（CMD 0x1D）

```
DATA[0]: 操作类型（1字节）
  - 0x00: 设置音量（DATA[1] = 音量值 0~100）
  - 0x01: 查询当前音量
```

**查询响应（Core -> Display，ACK）**：
```
DATA[0]: 当前音量（0~100）
```

> 音量由 `Audio_SetVolume()`/`Audio_GetVolume()` 控制，直接写入 CS43131 的 PCM Volume 寄存器。Core 板载 `+`/`-` 按键的短按也会触发音量调节，Core 自行调整后通过 `CMD_DISP_MUSIC_STATUS` 上报最新音量。

### 5.6 屏幕控制格式（CMD 0x18）

```
DATA[0]: 控制类型（1字节）
  - 0x00: 息屏
  - 0x01: 亮屏
  - 0x02: 设置自动息屏超时
  - 0x03: 获取自动息屏超时

DATA[1]: 参数（1字节，仅控制类型 0x02 时有效）
  - 自动息屏超时（单位秒，0=关闭自动息屏）
```

**获取自动息屏超时响应（Display -> Core，ACK）**：
```
DATA[0]: 当前自动息屏超时（单位秒，0=关闭）
```

> 屏幕唤醒/休眠统一通过本命令控制。Core 检测到 `Enter` 键长按等唤醒事件时，通过 `0x01`（亮屏）通知 Display；Display 自身检测到触摸事件也可自行亮屏，但需通知 Core 状态变化。

### 5.7 状态栏数据格式（CMD 0x14）

Core 向 Display 发送状态栏更新信息，用于显示电量、蓝牙、时间等系统状态。

```
DATA[0]: 状态位图（1字节）
  - bit0: 电量有效
  - bit1: 蓝牙状态有效
  - bit2: 时间有效
  - bit3: 充电状态有效
  - bit4: 当前应用有效
  - bit5~7: 预留
DATA[1]: 电量百分比（0~100，若 bit0=1）
DATA[2]: 蓝牙状态（0=断开, 1=已连接, 若 bit1=1）
DATA[3..6]: 时间戳（Unix 时间秒数，32位大端，若 bit2=1）
DATA[7]: 充电状态（0=未充电, 1=充电中, 2=已充满, 若 bit3=1）
DATA[8]: 当前应用ID（若 bit4=1）
```

> 状态数据长度由 LEN 推导，位图指示哪些字段有效。Core 定期（如每 5 秒）或在状态变化时主动上报。

### 5.8 模块状态格式（扩展码 0x04）

Core 向 Display 上报模块插拔/在线/离线事件。

```
DATA[0]: 扩展码 = 0x04
DATA[1]: 事件类型（1字节）
  - 0x00: 模块已插入
  - 0x01: 模块已拔出
  - 0x02: 完整模块列表（响应 GET_MODULE_STATUS）
DATA[2]: 模块 ID（1字节，如 0x20=Keyboard, 0x30=Power, 0x40~0x42=Submodel）
DATA[3]: 模块类型编号（1字节）
DATA[4]: 模块子类型编号（1字节）
```

**完整模块列表格式（事件类型 = 0x02）**：
```
DATA[0]: 扩展码 = 0x04
DATA[1]: 事件类型 = 0x02
DATA[2]: 模块数量 N
DATA[3]:      模块1 ID
DATA[4]:      模块1 类型
DATA[5]:      模块1 子类型
DATA[6]:      模块2 ID
...
```

> 每个模块信息固定 3 字节：ID(1) + 类型(1) + 子类型(1)。

### 5.9 文件列表格式（扩展码 0x07）

Core 响应 Display 的 `REQUEST_FILE_LIST`（0x06）请求，返回指定目录下的文件列表。

```
DATA[0]: 扩展码 = 0x07
DATA[1]: 状态码（1字节）
  - 0x00: 成功
  - 0x01: 路径不存在
  - 0x02: 磁盘未就绪
DATA[2]: 文件/目录数量 N（1字节）
DATA[3]:      条目1 属性（1字节）
  - bit0: 是否为目录
  - bit1: 是否为只读
  - bit2: 是否为隐藏
  - bit3: 是否有长文件名
DATA[4..7]:  条目1 文件大小（uint32 大端，目录为 0）
DATA[8..23]: 条目1 文件名（16字节，UTF-8，不足补 0x00）
... 后续条目
```

> 每个条目固定 **21 字节**：属性(1) + 大小(4) + 文件名(16)。
> 文件名优先使用长文件名（CH378 LFN 支持），无长文件名时回退到 8.3 短名。
> CH378 文件系统为 FAT32，支持 TF 卡和 U 盘，文件路径使用 UTF-8 编码，最大路径长度 260 字节。

### 5.10 文件操作格式（扩展码 0x0A）

Display 请求 Core 对 CH378 文件系统执行操作。

```
DATA[0]: 扩展码 = 0x0A
DATA[1]: 操作类型（1字节）
  - 0x00: 创建目录
  - 0x01: 删除文件（支持长文件名）
  - 0x02: 删除目录（仅空目录，CH378 限制）
  - 0x03: 重命名文件（仅短文件名，CH378 限制）
DATA[2..N]: 目标路径（UTF-8 字符串，变长）
```

> **CH378 限制**：
> - 目录删除仅支持空目录（CH378 固件 `CMD0H_FILE_ERASE` 对非空目录会等待/失败）
> - 重命名仅支持短文件名（8.3 格式），长文件名重命名建议用复制+删除替代
> - 文件创建和删除支持长文件名

### 5.11 蓝牙事件格式（扩展码 0x0C）

Core 将 CH585F 上报的蓝牙事件转发给 Display。CH585F 通过 SPI 与 Core（V5F）通信，使用 `CMD_BT_CONN_EVT`（0x54）上报连接/断开事件，使用扩展码 `CMD_BT_EXT_DEVICE_LIST`（0x50/0x02）上报扫描结果。Core 将这些事件统一打包后转发给 Display。

```
DATA[0]: 扩展码 = 0x0C
DATA[1]: 事件类型（1字节）
  - 0x00: 设备已连接
  - 0x01: 设备已断开
  - 0x02: 连接失败/超时
  - 0x03: 扫描结果（设备列表）
  - 0x04: 扫描完成
  - 0x05: 配对结果
DATA[2]: 设备类型（1字节，事件类型 0x00~0x02/0x05 时有效）
  - 0x00: 未知
  - 0x01: BLE 键盘
  - 0x02: BLE 鼠标
  - 0x03: 其他 BLE HID 设备
  - 0x04: 手机/PC（APP）
DATA[3..8]: 设备 MAC 地址（6字节，事件类型 0x00~0x02/0x05 时有效）
DATA[9..24]: 设备名称（16字节，不足补 0x00，事件类型 0x00~0x02/0x05 时有效）
```

**扫描结果格式（事件类型 = 0x03）**：
```
DATA[0]: 扩展码 = 0x0C
DATA[1]: 事件类型 = 0x03
DATA[2]: 设备数量 N
DATA[3]:      设备1 状态（0=已配对, 1=已连接, 2=发现中）
DATA[4..19]:  设备1 名称（16字节）
DATA[20..25]: 设备1 MAC（6字节）
... 后续设备
```

> 每个设备信息固定 **23 字节**：状态(1) + 名称(16) + MAC(6)，与 Wireless 协议对齐。

### 5.12 蓝牙控制格式（扩展码 0x0D）

Display 请求 Core 代为执行蓝牙操作。Core 收到后通过 SPI 向 CH585F 发送对应命令。

```
DATA[0]: 扩展码 = 0x0D
DATA[1]: 控制类型（1字节）
  - 0x00: 开始扫描（无额外参数）
  - 0x01: 停止扫描
  - 0x02: 连接设备（DATA[2..7] = MAC 地址）
  - 0x03: 断开设备（DATA[2..7] = MAC 地址）
  - 0x04: 断开所有设备
  - 0x05: 设置可发现性（DATA[2] = 0=关闭, 1=开启）
DATA[2..N]: 参数（变长，由控制类型决定）
```

> Core 收到后通过 SPI 向 CH585F 发送 `CMD_BT_EXTENSION`（0x50）+ 对应扩展码（`CMD_BT_EXT_SCAN`/`CMD_BT_EXT_CONNECT` 等），CH585F 执行后通过 SPI 上报结果，Core 再通过 `0x0C BT_EVENT` 转发给 Display。

### 5.13 Submodel 事件格式（扩展码 0x0E）

Core 将 Submodel 事件统一转发给 Display。通过子类型字段区分不同 Submodel 的事件，数据格式与 `Protocol_Submodels.md` 中各子类型的事件格式对齐。

```
DATA[0]: 扩展码 = 0x0E
DATA[1]: Submodel 子类型（1字节）
  - 0x01: 指纹识别（Fingerprint）
  - 0x02: 健康监测（Health）
  - 0x03: NFC 读卡
  - 0x04: 触摸圆环/旋钮（Touch Ring）
  - 0x05: RGB 灯效
  - 0x06: 红外测距（Infrared）
  - 0x07: 副屏（Sub-Display）
DATA[2]: 事件操作码（1字节，对应 Submodel 协议中的 CMD）
DATA[3..N]: 事件数据（变长，格式与 Submodel 协议中对应操作码的 DATA 格式一致）
```

**指纹识别事件示例**：
```
DATA[0] = 0x0E
DATA[1] = 0x01  (Fingerprint)
DATA[2] = 0x40  (CMD_SUB_EVT_NOTIFY)
DATA[3] = 0x01  (子命令: 识别成功)
DATA[4] = 指纹ID
DATA[5..20]: 名字（16字节）
```

**健康监测事件示例**：
```
DATA[0] = 0x0E
DATA[1] = 0x02  (Health)
DATA[2] = 0x43  (CMD_SUB_DATA_REPORT)
DATA[3] = 0x01  (子命令: 健康数据上报)
DATA[4] = 心跳
DATA[5] = 血氧
DATA[6] = 体温
```

**红外测距事件示例**：
```
DATA[0] = 0x0E
DATA[1] = 0x06  (Infrared)
DATA[2] = 0x45  (CMD_SUB_ACTION_RESULT)
DATA[3] = 0x01  (子命令: 测距结果)
DATA[4..5] = 距离（uint16 大端）
DATA[6] = 单位
DATA[7] = 精度
```

> Display 收到 Submodel 事件后，根据子类型和事件操作码解析数据，更新 UI 显示。

### 5.14 电源事件格式（扩展码 0x0F）

Core 将 Power 模块的事件转发给 Display，数据格式与 `Protocol_Power.md` 对齐。

```
DATA[0]: 扩展码 = 0x0F
DATA[1]: 事件类型（1字节）
  - 0x00: 状态变化（DATA[2..10] = 电源状态，格式同 Power 协议 0x36）
  - 0x01: 充电事件（DATA[2..4] = 充电事件，格式同 Power 协议 0x37）
  - 0x02: 告警事件（DATA[2..4] = 告警事件，格式同 Power 协议 0x38）
```

**状态变化数据（事件类型 = 0x00）**：
```
DATA[2]: 状态位图（1字节）
  - bit0: 是否充电中
  - bit1: 是否已充满
  - bit2: 是否充电宝输出中
DATA[3]: 电量百分比（0~100）
DATA[4..5]: 电池电压（uint16 大端，mV）
DATA[6..7]: 电池电流（int16 大端，mA）
DATA[8]: 电池温度（int8，°C）
DATA[9..10]: 功率（uint16 大端，mW）
```

**充电事件数据（事件类型 = 0x01）**：
```
DATA[2]: 充电事件类型（0x00=插入, 0x01=拔出, 0x02=开始充电, 0x03=充满, ...）
DATA[3..4]: 当前充电功率（uint16 大端，mW）
```

**告警事件数据（事件类型 = 0x02）**：
```
DATA[2]: 告警类型（0x00=低电量, 0x01=电量危急, 0x02=过温, 0x03=过流）
DATA[3..4]: 当前值（uint16 大端）
```

### 5.15 副屏内容格式（扩展码 0x15）

Core 向 Display 发送副屏（Submodel-7）显示内容，Display 负责转发给副屏模块。

```
DATA[0]: 扩展码 = 0x15
DATA[1]: 内容类型（1字节）
  - 0x00: 模块状态数据
  - 0x01: 自定义内容
  - 0x02: 清屏
DATA[2..N]: 内容数据（变长，格式同 Submodel 协议中副屏的对应格式）
```

> Display 作为副屏数据的中转站，收到后通过 UART 转发给 Submodel-7。

### 5.16 通知弹窗格式（CMD 0x1A）

```
DATA[0]: 优先级（1字节）
  - 0x00: 普通
  - 0x01: 重要
  - 0x02: 紧急
DATA[1..16]: 标题（16字节，UTF-8，不足补 0x00）
DATA[17..N]: 内容（UTF-8 字符串，变长）
```

---

## 6. CMD_GET_TYPE 响应

Display 模块响应 `CMD_GET_TYPE` 时，`CMD_ACK` 的 DATA 格式如下：

| 字节 | 字段 | 值/说明 |
|------|------|---------|
| DATA[0] | 模块类型编号 | `0x01` |
| DATA[1] | 模块子类型编号 | 见下表 |
| DATA[2] | 硬件版本号 | Display 硬件版本 |
| DATA[3] | 固件主版本号 | Display 固件主版本 |
| DATA[4] | 固件次版本号 | Display 固件次版本 |
| DATA[5..N] | 扩展信息 | 分辨率、触摸支持等能力位（可选） |

**Display 子类型定义**

> 代码宏定义：`protocol.h` → `MODULE_SUBTYPE_DISPLAY_*`

| 子类型编号 | 宏名 | 实现名称 | 说明 |
|------------|------|----------|------|
| `0x00` | `MODULE_SUBTYPE_DISPLAY_RESERVED` | 保留 | 系统保留 |
| `0x01` | `MODULE_SUBTYPE_DISPLAY_LCD` | LCD 显示 | RGB888 800×480，SSD1963 驱动，PWM 背光 |
| `0x02` | `MODULE_SUBTYPE_DISPLAY_EINK` | 墨水屏 | 黑白，支持局部刷新，可选触摸 |
| `0x03~0xFF` | — | 预留 | 未来扩展 |

---

## 7. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| V1.0 | 2026-04-24 | 初始版本 |
| V2.0 | 2026-05-22 | 全面修正：移除以太网操作码；新增 Core 按键输入事件；修正音乐控制/状态格式对齐 CS43131 实现；合并蓝牙操作为事件转发+控制请求模式；合并 Submodel 事件为统一转发格式；合并电源事件；移除与 SCREEN_CONTROL 重叠的 SCREEN_WAKEUP/SLEEP；移除过于笼统的 REPORT_EVENT/REQUEST_DATA；修正文件列表格式对齐 CH378 LFN 支持；扩展码重新编号 |
