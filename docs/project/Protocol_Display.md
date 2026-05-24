# Display 模块通信协议 V3.0

> **V3.0 CLI 直通重构**：文件操作、音乐控制等命令统一通过 CLI 直通（`DISP_EXT_CLI = 0x1A`）实现，废弃原自定义协议操作码。详见 §4。

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
| `0x1B` | ~~CMD_DISP_MUSIC_CONTROL~~ | — | **已废弃**（V3.0 CLI 直通替代） | — |
| `0x1C` | `CMD_DISP_MUSIC_STATUS` | Core -> Display | 音乐播放状态上报 | `[播放状态:1][当前时间:4][总时长:4][音量:1][曲目名:变长]` |
| `0x1D` | ~~CMD_DISP_VOLUME_CONTROL~~ | — | **已废弃**（V3.0 CLI 直通替代） | — |
| `0x1E` | `CMD_DISP_FACTORY_RESET` | Core -> Display | 恢复出厂设置 | `[确认码: 0xA5]` |
| `0x1F` | — | — | 预留 | — |

> **已移除/废弃的操作码**：`0x1B`（MUSIC_CONTROL）、`0x1D`（VOLUME_CONTROL）已废弃，使用 CLI 直通替代（`pause`/`resume`/`stop`/`vol`）。原 `0x1E CMD_DISP_ETHERNET_STATUS` 已移除（以太网当前未启用）。原 `0x1F FACTORY_RESET` 移至 `0x1E`。

### 2.2 扩展操作码（CMD = 0x10，DATA[0] 为子命令）

| 扩展码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x01` | `CMD_DISP_EXT_APP_LAUNCH` | Core -> Display | 启动指定应用 | `[应用ID]` |
| `0x02` | `CMD_DISP_EXT_APP_CLOSE` | Core -> Display | 关闭当前应用 | 无 |
| `0x03` | `CMD_DISP_EXT_APP_DATA` | 双向 | 应用数据传输 | `[应用ID:1][数据类型:1][数据:变长]` |
| `0x04` | `CMD_DISP_EXT_MODULE_STATUS` | Core -> Display | 模块状态主动上报（插拔/在线/离线） | 见下方模块状态格式 |
| `0x05` | `CMD_DISP_EXT_GET_MODULE_STATUS` | Display -> Core | 请求获取当前模块组成信息 | 无，Core 回复 `0x04` |
| `0x06`~`0x0B` | — | — | **已废弃**（V3.0 CLI 直通 `ls`/`cat`/`mkdir`/`rm`/`play` 替代） | — |
| `0x0C` | `CMD_DISP_EXT_BT_EVENT` | Core -> Display | 蓝牙事件转发（连接/断开/扫描结果） | 见 §6.11 |
| `0x0D` | `CMD_DISP_EXT_BT_CONTROL` | Display -> Core | 蓝牙控制请求（扫描/连接/断开） | `[控制类型:1][参数:变长]` |
| `0x0E` | `CMD_DISP_EXT_SUBMODEL_EVENT` | Core -> Display | Submodel 事件转发（指纹/NFC/健康/红外/触摸/RGB） | `[子类型:1][事件数据:变长]` |
| `0x0F` | `CMD_DISP_EXT_POWER_EVENT` | Core -> Display | 电源事件转发（充电/告警） | `[事件类型:1][事件数据:变长]` |
| `0x10` | `CMD_DISP_EXT_SAVE_CONFIG` | Display -> Core | 请求保存配置到 TF 卡 | `[路径:变长]` |
| `0x11` | `CMD_DISP_EXT_LOAD_CONFIG` | Display -> Core | 请求加载配置文件 | `[路径:变长]` |
| `0x12` | `CMD_DISP_EXT_CONFIG_RESULT` | Core -> Display | 配置保存/加载结果 | `[结果:1][错误码:1][配置数据:变长]` |
| `0x13` | `CMD_DISP_EXT_SET_RGB_MODE` | Display -> Core | 设置 RGB 灯效模式 | `[灯效ID][颜色R][颜色G][颜色B][亮度][速度]` |
| `0x14` | — | — | **已废弃**（V3.0 CLI 直通替代） | — |
| `0x15` | `CMD_DISP_EXT_SUBDISP_CONTENT` | Core -> Display | 副屏显示内容设置 | `[内容类型:1][内容数据:变长]` |
| `0x16` | `CMD_DISP_EXT_SUBDISP_CONFIG` | Core -> Display | 副屏配置 | `[分辨率][方向][开关]` |
| `0x17` | `CMD_DISP_EXT_ERROR_REPORT` | Display -> Core | Display 错误/异常上报 | `[错误码:1][错误信息:变长]` |
| `0x18` | `CMD_DISP_EXT_HID_STATUS` | Core -> Display | 外接 HID 设备连接/断开状态同步 | `[事件:1][设备类型:1]` |
| `0x19` | — | — | **已废弃**（V3.0 CLI 直通 `cd` 替代） | — |
| `0x1A` | `CMD_DISP_EXT_CLI` | 双向 | **CLI 命令直通**（Display→Core 发命令，Core 执行后通过同扩展码返回文本） | §4 |
| `0x1B` | `CMD_DISP_EXT_CWD_NOTIFY` | Core→Display | **CWD 变更通知**（Core 主动推送当前工作目录，用于三方路径同步） | §4.8 |
| `0x1C~0x3F` | — | — | 预留 | — |

> **V3.0 已废弃（Core 回复 NACK）**：`0x06~0x0B`（FILE_LIST/READ/SAVE/OP/PLAY_MUSIC）、`0x14`（BULK_TRANSFER）、`0x19`（CD）、基础码 `0x1B`（MUSIC_CONTROL）、`0x1D`（VOLUME_CONTROL）。全部由 CLI 直通替代。

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
| `0x1B` | ~~MUSIC_CONTROL~~ | 动作 | **已废弃**，Core 回复 NACK |
| `0x1C` | MUSIC_STATUS | 事件 | Display 不回复 |
| `0x1D` | ~~VOLUME_CONTROL~~ | 动作/查询 | **已废弃**，Core 回复 NACK |
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
| `0x06`~`0x0B` | — | — | **已废弃**，Core 回复 NACK |
| `0x0C` | BT_EVENT | 事件 | Display 不回复 |
| `0x0D` | BT_CONTROL | 动作 | Core 回复 ACK 空 或 NACK |
| `0x0E` | SUBMODEL_EVENT | 事件 | Display 不回复 |
| `0x0F` | POWER_EVENT | 事件 | Display 不回复 |
| `0x10` | SAVE_CONFIG | 动作 | Core 回复 ACK 空 或 NACK |
| `0x11` | LOAD_CONFIG | 查询 | Core 回复 ACK + `[配置数据:变长]` 或 NACK |
| `0x12` | CONFIG_RESULT | 事件 | Display 不回复 |
| `0x13` | SET_RGB_MODE | 动作 | Core 回复 ACK 空 或 NACK |
| `0x14` | — | — | **已废弃**，Core 回复 NACK |
| `0x15` | SUBDISP_CONTENT | 设置 | 发送即忘 |
| `0x16` | SUBDISP_CONFIG | 设置 | 发送即忘 |
| `0x17` | ERROR_REPORT | 事件 | Core 不回复 |
| `0x18` | HID_STATUS | 事件 | Display 不回复 |
| `0x19` | — | — | **已废弃**，Core 回复 NACK |
| `0x1A` | CLI | 动作 | Core 通过 DISP_EXT_CLI 返回文本，不发送独立 ACK |
| `0x1B` | CWD_NOTIFY | 事件 | Display 不回复（Core 主动推送） |

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

## 4. CLI 直通协议（CMD_DISP_EXT_CLI = 0x1A）

### 4.1 设计背景

Display 模块与 Core 之间的文件操作、音乐控制等命令，与 Core 侧 CLI 已实现的命令（`ls`/`cd`/`cat`/`mkdir`/`rm`/`play`/`pause`/`vol` 等）功能完全重叠。维护两套命令系统（自定义协议命令 + CLI 命令）成本高且容易不一致。

**CLI 直通机制**统一使用 `CMD_DISP_EXT_CLI`（0x1A）扩展命令，将 Display 侧的请求直接转换为 CLI 命令字符串发送给 Core，Core 通过 `CLI_Process()` 执行并返回文本输出。这与 Wireless 模块操控 Core 的方式完全一致。

### 4.2 命令映射

| 原扩展码 | 原命令 | CLI 直通命令 | 说明 |
|----------|--------|-------------|------|
| `0x06` | REQUEST_FILE_LIST | `cd <dirname>` → 收到响应后 → `ls` | 列出指定目录文件（两步顺序发送，dirname 为单级目录名） |
| `0x19` | CD | `cd <path>` | 切换工作目录 |
| `0x08` | FILE_READ | `cat <path>` | 读取文件内容 |
| `0x0A` | FILE_OPERATION (mkdir) | `mkdir <path>` | 创建目录 |
| `0x0A` | FILE_OPERATION (delete) | `rm <path>` 或 `rm -r <path>` | 删除文件/目录 |
| `0x0B` | PLAY_MUSIC | `play <path>` | 播放音乐文件 |
| `0x1B` | MUSIC_CONTROL (pause) | `pause` | 暂停/继续播放 |
| `0x1B` | MUSIC_CONTROL (stop) | `stop` | 停止播放 |
| `0x1D` | VOLUME_CONTROL | `vol <0-100>` | 设置音量 |
| `0x1D` | VOLUME_CONTROL (query) | `vol` | 查询当前音量 |

### 4.3 帧格式

#### Display → Core（请求）

```
[AA][10][00][LEN][10][1A][CLI命令字符串...][A5][5A][FC][FD]
```

- `CMD = 0x10`（扩展操作码基码）
- `DATA[0] = 0x1A`（CLI 直通子命令）
- `DATA[1..N]`：UTF-8 编码的 CLI 命令字符串（**不含** `\0` 终止符，长度由 LEN 推导）

**示例**：Display 请求进入 `DOC` 目录

```
[AA][10][00][07][10][1A] 'c' 'd' ' ' 'D' 'O' 'C' [A5][5A][FC][FD]
 HEAD SRC DST LEN CMD EXT       CLI 命令: "cd DOC"
```

> **注意**：Core CLI 不支持 `&&` 命令拼接。目录切换与文件列表需分两步顺序发送：先 `cd <dirname>`（单级目录名），收到响应后再发 `ls`。Display 侧 `uart_module.c` 通过 `s_pending_ls_after_cd` 标志自动完成这一流程。

#### Core → Display（响应）

CLI 命令执行后，Core 通过 `Display_SendCLIResponse()` 将 `CLI_Process()` 的输出文本以 **DISP_EXT_CLI 帧**返回（**不发送独立 ACK**）：

```
[AA][10][00][LEN][10][1A][输出文本...][A5][5A][FC][FD]
```

- `CMD = 0x10`（扩展操作码基码）
- `DATA[0] = 0x1A`（CLI 直通子命令，与请求相同）
- `DATA[1..N]`：UTF-8 编码的 CLI 输出文本

**长输出处理**：当 CLI 输出超过单帧 DATA 域上限（250 字节）时，Core 分帧发送，每帧最大 240 字节 payload。Display 侧负责组装多帧响应。

**空输出**：某些命令（如 `mkdir`、`rm`）成功执行后无输出，Core 发送 `"ok\n"` 作为响应。

**错误输出**：CLI 命令执行失败时，输出包含错误信息（如 `"play: cannot open 'xxx' (status=XX)\n"`），Display 侧通过解析输出内容判断成功/失败。

**CH378 忙碌**：当 CH378 被音乐流式播放占用时，Core 直接返回 `"CH378 busy (music streaming)\n"`，不执行 CLI 命令。

### 4.4 Display 侧响应组装

Display 侧 `uart_module.c` 维护 CLI 响应组装缓冲区：

1. 收到 CLI 响应首包时，初始化组装缓冲区
2. 追加每帧的输出文本到缓冲区
3. 收到最后一帧后，根据发送的命令类型分发：
   - `ls` 命令 → 解析输出为 `file_entry_t` 结构，调用 `on_file_list` 回调
   - 其他命令 → 调用 `on_cli_response` 回调，传递原始文本

### 4.5 ls 输出解析

Core 侧 `ls` 命令输出格式（每行一个条目）：

```
--- \DOC ---
  [DIR]  MUSIC
  [DIR]  PICTURES
  [FILE] readme.txt  (1024 bytes)
  [FILE] song.wav  (65536 bytes)
```

- 第一行为当前路径（`--- \PATH ---`）
- `[DIR]` 表示目录，`[FILE]` 表示文件
- 文件条目包含大小信息 `(NNN bytes)`
- 目录条目无大小信息

Display 侧将此输出解析为 `file_entry_t` 结构数组：

```c
typedef struct {
    uint8_t  attr;       /* 属性位：FILE_ATTR_IS_DIR 等 */
    uint32_t size;       /* 文件大小（目录为 0） */
    char     name[17];   /* 文件名，UTF-8 */
} file_entry_t;
```

### 4.6 交互流程示例

#### 列出目录文件

```
Display                              Core
  │                                    │
  │── UART ──[ CLI: "cd DOC" ]───────>│
  │   CMD=0x10 EXT=0x1A                │
  │                                    │── CLI_Process() ──> cd DOC
  │                                    │
  │<─ UART ──[ CLI resp: "\DOC\n" ]───│
  │   CMD=0x10 EXT=0x1A                │
  │                                    │
  │ (s_pending_ls_after_cd=true)       │
  │── UART ──[ CLI: "ls" ]───────────>│  ← 自动发送
  │   CMD=0x10 EXT=0x1A                │
  │                                    │── CLI_Process() ──> ls 输出
  │                                    │
  │<─ UART ──[ CLI resp + ls 输出 ]────│
  │   CMD=0x10 EXT=0x1A                │
  │                                    │
  │ (解析 ls 输出 → file_entry_t[])    │
  │ (调用 on_file_list 回调)           │
```

> **重要**：Core 的 `cd` 命令只接受单级目录名（如 `DOC`、`..`、`\`），不支持绝对路径（如 `\DOC\MUSIC`）。Display 侧需在本地维护路径状态，通过多次单级 `cd` 实现导航。这与 WCH Terminal APP 的实现方式一致：先 `cd <dirname>` 等待响应成功，再发 `ls` 获取文件列表。

#### 播放音乐

```
Display                              Core
  │                                    │
  │── UART ──[ CLI: "play song.wav" ]─>│
  │   CMD=0x10 EXT=0x1A                │
  │                                    │── CLI_Process() ──> 播放
  │                                    │
  │<─ UART ──[ CLI resp + "Playing..." ]│
  │   CMD=0x10 EXT=0x1A                │
  │                                    │
  │ (调用 on_cli_response 回调)        │
```

> **注意**：`play` 和 `cat` 命令在 Core 侧基于当前工作目录 + 文件名构建完整路径（`CH378_Path_Join`），因此只需传入文件名，无需传入绝对路径。

#### 读取文件内容

```
Display                              Core
  │                                    │
  │── UART ──[ CLI: "cat readme.txt" ]─>│
  │   CMD=0x10 EXT=0x1A                │
  │                                    │── CLI_Process() ──> cat 输出
  │                                    │
  │<─ UART ──[ CLI resp + 文件内容 ]───│
  │   CMD=0x10 EXT=0x1A                │
  │   (可能多帧)                        │
  │                                    │
  │ (组装多帧 → 完整文件内容)          │
  │ (调用 on_cli_response 回调)        │
```

### 4.7 与 Wireless CLI 直通的对比

| 特性 | Display ↔ Core CLI | Wireless (APP) ↔ Core CLI |
|------|-------------------|--------------------------|
| 物理通道 | UART4（921600 bps） | BLE（CH585F 透传） |
| 协议封装 | 统一协议帧 `CMD=0x10 EXT=0x1A` | BLE 应用层帧 `MSG_TYPE_CLI_DATA` |
| 分包机制 | 流式帧（LEN=0xFF） | BLE 应用层分包（SOF/EOF） |
| 响应组装 | Display 侧 `uart_module.c` | APP 侧重组缓冲区 |
| ls 解析 | Display 侧解析为 `file_entry_t` | APP 侧直接显示文本 |
| Core 处理入口 | `Display_HandleCLI` → `CLI_Process()` | `CMD_BT_EXT_CLI_DATA` → `CLI_Process()` |

> 两种方式在 Core 侧最终都通过同一个 `CLI_Process()` 执行命令，确保行为一致。

### 4.8 CWD 变更通知与三方路径同步

#### 问题背景

Core 的当前工作目录（CWD）可由三个客户端修改：
1. **Display-1**（通过 UART CLI 直通）
2. **WCH Terminal App**（通过 BLE CLI 直通）
3. **Core UART CLI**（直接串口终端）

当任一客户端执行 `cd` 或 `device` 命令时，Core 的 CWD 发生变化，其他客户端的本地路径显示可能与 Core 实际状态不同步。

#### 同步方案

采用**事件驱动**方式（非定时轮询），Core 在 CWD 变更时主动推送通知：

**Display-1**：通过专用扩展码 `CMD_DISP_EXT_CWD_NOTIFY`（0x1B）推送

```
[AA][00][10][LEN][10][1B][路径字符串...][A5][5A][FC][FD]
 HEAD SRC DST LEN CMD EXT   DATA[1..N] = CWD 路径（UTF-8）
```

- `CMD = 0x10`（扩展操作码基码）
- `DATA[0] = 0x1B`（CWD_NOTIFY 子命令）
- `DATA[1..N]`：当前工作目录路径字符串（如 `\DOC\MUSIC`），最大 63 字节
- Display 收到后更新本地路径显示，无需回复

**WCH Terminal App**：通过 CLI 输出中的 `[CWD]` 标记行推送

Core 在 `cd`/`device` 成功后，额外输出一行 `[CWD] <path>`。该标记行：
- 作为 CLI 输出的一部分，通过 BLE CLI_DATA 通道传输
- App 的 `CliEngine` 检测 `[CWD]` 前缀，提取路径并通过 `cwdStream` 发出通知
- `FileNotifier` 监听 `cwdStream`，自动同步本地路径并刷新文件列表
- 对于 App 自身发起的 `cd`，该标记行被 `CliEngine` 过滤后不影响命令响应解析

#### 触发时机

Core 在以下场景推送 CWD 通知：

| 场景 | 触发方式 | Display 通知 | App 通知 |
|------|---------|-------------|---------|
| Display 发送 `cd` | CLI 直通 | CWD_NOTIFY 帧 | `[CWD]` 标记行（通过 BLE 捕获输出） |
| App 发送 `cd` | BLE CLI | CWD_NOTIFY 帧 | `[CWD]` 标记行（被 CliEngine 过滤） |
| Core UART `cd` | 直接执行 | CWD_NOTIFY 帧 | `[CWD]` 标记行（unsolicited output） |
| `device` 命令切换存储 | CLI/BLE | CWD_NOTIFY 帧 | `[CWD]` 标记行 |
| Display/App 发送 `ls` | CLI 直通 | ls 输出 `--- \PATH ---` 头部解析 | cd 响应中路径已同步 |

#### ls 输出路径头部

`ls` 命令输出的首行 `--- \PATH ---` 也携带路径信息。Display 侧 `uart_module.c` 在解析 `ls` 输出时，同时提取该头部并通过 `on_cwd_notify` 回调通知应用层，作为 CWD 同步的补充机制。

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

### 5.3 音乐播放状态格式（CMD 0x1C）

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

### 5.4 屏幕控制格式（CMD 0x18）

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

### 5.5 状态栏数据格式（CMD 0x14）

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

### 5.6 模块状态格式（扩展码 0x04）

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

### 5.7 蓝牙事件格式（扩展码 0x0C）

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

### 5.8 蓝牙控制格式（扩展码 0x0D）

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

### 5.9 Submodel 事件格式（扩展码 0x0E）

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

### 5.10 电源事件格式（扩展码 0x0F）

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

### 5.11 副屏内容格式（扩展码 0x15）

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

### 5.12 通知弹窗格式（CMD 0x1A）

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
| V3.0 | 2026-05-23 | CLI 直通重构：废弃扩展码 0x06~0x0B、0x14、0x19 及基础码 0x1B/0x1D，统一使用 0x1A CLI 直通；Core 不再发送独立 ACK，CLI 响应通过 DISP_EXT_CLI 帧返回；删除批量传输/文件列表/文件操作/音乐控制/音量控制等废弃章节；删除以太网相关内容；章节重新编号 |
| V3.1 | 2026-05-23 | CWD 三方同步：新增扩展码 0x1B `CMD_DISP_EXT_CWD_NOTIFY`（Core→Display 推送路径变更）；Core `cd`/`device` 成功后输出 `[CWD] <path>` 标记行供 WCH Terminal App 同步；Display 侧 ls 输出头部路径解析；app_file 支持触摸滑动/鼠标滚轮/双击打开/设备切换 |
