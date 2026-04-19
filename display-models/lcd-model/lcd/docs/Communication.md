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

| 字段  | 长度  | 说明  |
| --- | --- | --- |
| **HEAD** | 1 byte | 帧头，固定值 `0xAA`。仅在接收状态机处于 `WAIT_HEAD` 状态时识别为新帧起始，数据域中出现 `0xAA` 不触发帧重解析。 |
| **SRC** | 1 byte | 源模块ID，标识发送方。 |
| **DST** | 1 byte | 目标模块ID，标识接收方。 |
| **LEN** | 1 byte | 有效载荷长度，即 `CMD + DATA` 的总字节数。`DATA` 长度 = `LEN - 1`，最大 255 字节，故最大帧长度为 260 字节。 |
| **CMD** | 1 byte | 操作码。高 4 位标识模块类型，低 4 位标识操作类型，或由各模块独立定义。 |
| **DATA** | N bytes | 数据域，长度由 `LEN` 推导，允许为 0 字节（此时 `LEN = 1`）。 |

**最大帧传输时间**（115200 波特率）：约 22.6 ms。

### 3. 模块 ID 定义

模块 ID 与硬件编号一一对应，全系统统一：

| ID  | 模块  | 物理接口 | 主要交互核心 | 说明  |
| --- | --- | --- | --- | --- |
| `0x00` | Core（核心） | —   | V3F / V5F | 两核共享同一 ID，通过 SRC 区分来源 |
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

| 操作码 | 宏名  | 方向  | 说明  |
| --- | --- | --- | --- |
| `0x00` | `CMD_NOP` | 双向  | 心跳/空操作，可用于保活检测。 |
| `0x01` | `CMD_GET_TYPE` | Core -> Module | 查询模块类型。Module 回复自身类型编号。 |
| `0x02` | `CMD_GET_STATUS` | Core -> Module | 查询模块当前状态。 |
| `0x03` | `CMD_SET_CONFIG` | Core -> Module | 向模块下发配置参数。 |
| `0x04` | `CMD_ACK` | 双向  | 肯定确认。DATA 可选携带响应数据。 |
| `0x05` | `CMD_NACK` | 双向  | 否定确认。DATA[0] 建议携带错误码。 |
| `0x06` | `CMD_EVT_NOTIFY` | Module -> Core | 事件主动上报，如电量变化、按键按下、蓝牙连接等。 |
| `0x07` | `CMD_DATA_STREAM` | 双向  | 纯数据流传输，用于大数据量场景（如音频、文件）。 |

### 5. 模块专用操作码

各模块在独立头文件中定义专属操作码，操作码空间由各模块自行管理，建议按模块划分范围以避免冲突。0x10、0x20、0x30、0x40、0x50留作拓展操作码使用，允许模块在data中设置拓展操作码位来设置更多操作种类。

#### 5.1 Display（屏幕模块）

Display 模块操作码分为两类：
- **基础操作码**（`0x11~0x1F`）：直接对应常用功能
- **扩展操作码**：当基础操作码为 `0x10` 时，`DATA[0]` 作为扩展操作码，支持更多功能

##### 5.1.1 基础操作码（0x11 ~ 0x1F）

| 操作码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x11` | `CMD_DISP_SET_BRIGHTNESS` | Core -> Display | 设置背光亮度 | `[亮度值: 0-100]` |
| `0x12` | `CMD_DISP_GET_BRIGHTNESS` | Core -> Display | 获取当前背光亮度 | 无，Display 回复 ACK + 亮度值 |
| `0x13` | `CMD_DISP_SHOW_PAGE` | Core -> Display | 切换显示页面 | `[页面编号]` |
| `0x14` | `CMD_DISP_UPDATE_STATUS` | Core -> Display | 更新状态栏信息 | 见下方状态栏数据格式 |
| `0x15` | `CMD_DISP_TOUCH_EVT` | Display -> Core | 触摸屏事件上报 | `[事件类型][X高][X低][Y高][Y低]` |
| `0x16` | `CMD_DISP_SET_ROTATION` | Core -> Display | 设置屏幕旋转角度 | `[角度: 0/90/180/270]` |
| `0x17` | `CMD_DISP_GET_ROTATION` | Core -> Display | 获取当前屏幕旋转角度 | 无，Display 回复 ACK + 角度 |
| `0x18` | `CMD_DISP_SET_AUTO_SCREEN_OFF` | Core -> Display | 设置自动息屏开关 | `[开关: 0=关, 1=开]` |
| `0x19` | `CMD_DISP_SET_SCREEN_TIMEOUT` | Core -> Display | 设置息屏超时时间 | `[超时秒数高][超时秒数低]` |
| `0x1A` | `CMD_DISP_VIRTUAL_TOUCH` | Core -> Display | 虚拟触摸事件（核心模块模拟触摸） | `[事件类型][X高][X低][Y高][Y低]` |
| `0x1B` | `CMD_DISP_KEY_EVENT` | Core -> Display | 键盘/鼠标事件注入 | `[设备类型][键码/事件][修饰键]` |
| `0x1C` | `CMD_DISP_GET_SYSTEM_INFO` | Core -> Display | 获取系统信息（内存、FPS、CPU） | 无，Display 回复 ACK + 信息 |
| `0x1D` | `CMD_DISP_FACTORY_RESET` | Core -> Display | 恢复出厂设置 | `[确认码: 0xA5]` |
| `0x1E` | `CMD_DISP_SET_DEBUG_INFO` | Core -> Display | 设置调试信息显示开关 | `[开关: 0=关, 1=开]` |
| `0x1F` | `CMD_DISP_RESERVED` | — | 预留 | — |

##### 5.1.2 扩展操作码（CMD = 0x10，DATA[0] 为子命令）

| 扩展码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x01` | `CMD_DISP_EXT_SET_QRCODE` | Core -> Display | 设置二维码显示内容 | `[内容类型][内容字符串...]` |
| `0x02` | `CMD_DISP_EXT_GET_QRCODE` | Core -> Display | 获取当前二维码内容 | 无 |
| `0x03` | `CMD_DISP_EXT_APP_LAUNCH` | Core -> Display | 启动指定应用 | `[应用ID]` |
| `0x04` | `CMD_DISP_EXT_APP_CLOSE` | Core -> Display | 关闭当前应用 | 无 |
| `0x05` | `CMD_DISP_EXT_APP_DATA` | 双向 | 应用数据传输 | `[应用ID][数据...]` |
| `0x06` | `CMD_DISP_EXT_MODULE_STATUS` | Core -> Display | 更新模块状态显示 | `[模块ID][状态][附加数据...]` |
| `0x07` | `CMD_DISP_EXT_GET_MODULE_STATUS` | Core -> Display | 获取模块状态 | `[模块ID]` |
| `0x08` | `CMD_DISP_EXT_GAME_CONTROL` | Core -> Display | 游戏控制指令 | `[游戏ID][控制类型][参数...]` |
| `0x09` | `CMD_DISP_EXT_SET_VOLUME` | Core -> Display | 设置音量（通过Display转发给音频模块） | `[音量值: 0-100]` |
| `0x0A` | `CMD_DISP_EXT_GET_VOLUME` | Core -> Display | 获取当前音量 | 无 |
| `0x0B` | `CMD_DISP_EXT_SET_DEVICE_NAME` | Core -> Display | 设置设备名称 | `[名称字符串...]` |
| `0x0C` | `CMD_DISP_EXT_GET_DEVICE_NAME` | Core -> Display | 获取设备名称 | 无 |
| `0x0D` | `CMD_DISP_EXT_NOTIFICATION` | Core -> Display | 显示通知弹窗 | `[优先级][标题长度][标题...][内容长度][内容...]` |
| `0x0E` | `CMD_DISP_EXT_CLEAR_NOTIFICATION` | Core -> Display | 清除通知 | `[通知ID]` |
| `0x0F` | `CMD_DISP_EXT_SET_THEME` | Core -> Display | 设置主题/配色 | `[主题ID]` |
| `0x10` | `CMD_DISP_EXT_MUSIC_CONTROL` | Core <-> Display | 音乐播放控制 | `[控制类型][参数...]` |
| `0x11` | `CMD_DISP_EXT_MUSIC_STATUS` | Core -> Display | 音乐播放状态上报 | `[播放状态][进度高][进度低][总时长高][总时长低]` |
| `0x12` | `CMD_DISP_EXT_FILE_LIST` | Core -> Display | 文件列表数据 | `[路径长度][路径...][文件数量][文件信息...]` |
| `0x13` | `CMD_DISP_EXT_FILE_OPERATION` | Core <-> Display | 文件操作（打开/读取/写入/删除） | `[操作类型][路径长度][路径...][数据...]` |
| `0x14` | `CMD_DISP_EXT_IMAGE_DATA` | Core -> Display | 图片数据流 | `[图片ID][格式][宽度高][宽度低][高度高][高度低][数据...]` |
| `0x15` | `CMD_DISP_EXT_BT_DEVICE` | Core -> Display | 蓝牙设备列表/状态 | `[设备数量][设备信息...]` |
| `0x16` | `CMD_DISP_EXT_BT_PAIRING` | Core <-> Display | 蓝牙配对流程 | `[配对状态][设备地址...][PIN码...]` |
| `0x17` | `CMD_DISP_EXT_NFC_TAG` | Core -> Display | NFC 标签数据 | `[标签类型][UID长度][UID...][数据长度][数据...]` |
| `0x18` | `CMD_DISP_EXT_FP_STATUS` | Core -> Display | 指纹识别状态 | `[状态][指纹ID][匹配度]` |
| `0x19` | `CMD_DISP_EXT_HEALTH_DATA` | Core -> Display | 健康监测数据 | `[数据类型][数值高][数值低][时间戳...]` |
| `0x1A` | `CMD_DISP_EXT_POWER_STATUS` | Core -> Display | 电源状态详细数据 | `[电压高][电压低][电流高][电流低][电量][充电状态]` |
| `0x1B` | `CMD_DISP_EXT_USB_STATUS` | Core -> Display | USB 连接状态/传输进度 | `[连接状态][传输进度][设备类型]` |
| `0x1C` | `CMD_DISP_EXT_LIGHTS_CONTROL` | Core <-> Display | 灯效控制 | `[灯效ID][颜色R][颜色G][颜色B][亮度][速度]` |
| `0x1D` | `CMD_DISP_EXT_IR_RANGE_DATA` | Core -> Display | 红外测距数据 | `[距离高][距离低][单位][精度]` |
| `0x1E` | `CMD_DISP_EXT_EBOOK_DATA` | Core -> Display | 电子书内容 | `[章节ID][页码高][页码低][文本长度][文本...]` |
| `0x1F` | `CMD_DISP_EXT_EMUSIC_CONTROL` | Core <-> Display | 电子音乐/合成器控制 | `[音符][音色][力度][时长]` |
| `0x20` | `CMD_DISP_EXT_SAVE_CONFIG` | Core <-> Display | 保存配置到 Flash | `[配置类型][配置数据...]` |
| `0x21` | `CMD_DISP_EXT_LOAD_CONFIG` | Core -> Display | 加载配置（开机下发） | `[配置类型][配置数据...]` |
| `0x22` | `CMD_DISP_EXT_DATA_STREAM` | 双向 | 大数据流传输（分帧） | `[传输ID][帧序号][总帧数][数据...]` |
| `0x23` | `CMD_DISP_EXT_REQUEST_DATA` | Display -> Core | Display 请求数据 | `[请求类型][参数...]` |
| `0x24` | `CMD_DISP_EXT_REPORT_EVENT` | Display -> Core | Display 上报用户事件 | `[事件类型][事件数据...]` |
| `0x25` | `CMD_DISP_EXT_ERROR_REPORT` | Display -> Core | Display 错误/异常上报 | `[错误码][错误信息...]` |
| `0x26` | `CMD_DISP_EXT_OTA_STATUS` | Core -> Display | OTA 升级状态通知 | `[状态][进度][错误码]` |
| `0x27` | `CMD_DISP_EXT_WIFI_SCAN` | Core <-> Display | WiFi 扫描/列表 | `[状态][网络数量][网络信息...]` |
| `0x28` | `CMD_DISP_EXT_WIFI_CONNECT` | Core <-> Display | WiFi 连接/断开 | `[SSID长度][SSID...][密码长度][密码...]` |
| `0x29` | `CMD_DISP_EXT_WIFI_STATUS` | Core -> Display | WiFi 状态详情 | `[连接状态][信号强度][IP地址...]` |
| `0x2A` | `CMD_DISP_EXT_SUBDISP_CONTENT` | Core <-> Display | 副屏显示内容设置 | `[内容类型][内容数据...]` |
| `0x2B` | `CMD_DISP_EXT_SUBDISP_CONFIG` | Core <-> Display | 副屏配置 | `[分辨率][方向][开关]` |
| `0x2C` | `CMD_DISP_EXT_SCREEN_WAKEUP` | Display -> Core | 屏幕唤醒事件 | `[唤醒源: 0=触摸, 1=按键, 2=UART]` |
| `0x2D` | `CMD_DISP_EXT_SCREEN_SLEEP` | Display -> Core | 屏幕休眠事件 | `[休眠源]` |
| `0x2E~0x3F` | — | — | 预留 | — |

##### 5.1.3 数据格式详解

**触摸屏/虚拟触摸事件格式**（CMD 0x15 / 0x1A）：
```
DATA[0]: 事件类型
  - 0x00: 按下 (PRESS)
  - 0x01: 移动 (MOVE)
  - 0x02: 释放 (RELEASE)
  - 0x03: 长按 (LONG_PRESS)
  - 0x04: 双击 (DOUBLE_CLICK)
DATA[1]: X 坐标高字节
DATA[2]: X 坐标低字节
DATA[3]: Y 坐标高字节
DATA[4]: Y 坐标低字节
```

**键盘/鼠标事件格式**（CMD 0x1B）：
```
DATA[0]: 设备类型
  - 0x00: 键盘
  - 0x01: 鼠标
  - 0x02: 触摸板
DATA[1]: 事件类型
  - 键盘: 0x00=按下, 0x01=释放, 0x02=长按
  - 鼠标: 0x00=移动, 0x01=左键按下, 0x02=左键释放, 0x03=右键按下, 0x04=右键释放, 0x05=滚轮
DATA[2]: 键码/鼠标X偏移（有符号）
DATA[3]: 修饰键（Ctrl/Shift/Alt）/ 鼠标Y偏移（有符号）
DATA[4]: 鼠标滚轮偏移（有符号，仅滚轮事件）
```

**状态栏更新格式**（CMD 0x14）：
```
DATA[0]: 更新掩码（位域，指示哪些字段有效）
  - bit0: 电量
  - bit1: 蓝牙状态
  - bit2: WiFi状态
  - bit3: 时间
  - bit4: 日期
  - bit5: 核心模块连接状态
  - bit6: 音量
  - bit7: 预留
DATA[1]: 电量百分比（0-100，若bit0=1）
DATA[2]: 蓝牙状态（0=断开, 1=已连接, 2=配对中，若bit1=1）
DATA[3]: WiFi状态（0=断开, 1=已连接, 2=连接中，若bit2=1）
DATA[4-7]: 时间戳（Unix时间，32位大端，若bit3=1）
DATA[8-11]: 日期（YYYYMMDD格式，32位大端，若bit4=1）
DATA[12]: 核心模块状态（0=离线, 1=在线, 2=休眠，若bit5=1）
DATA[13]: 音量（0-100，若bit6=1）
```

**模块状态格式**（扩展码 0x06）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x06
DATA[2]: 模块ID（0x01=CH585F, 0x20=Keyboard, 0x30=Power, 0x40-0x42=Submodels）
DATA[3]: 状态（0=离线, 1=在线, 2=错误, 3=休眠）
DATA[4]: 附加数据长度
DATA[5..N]: 附加数据（模块特定）
```

**应用数据传输格式**（扩展码 0x05）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x05
DATA[2]: 应用ID（见下方应用ID定义）
DATA[3]: 数据类型（0x00=命令, 0x01=查询, 0x02=响应, 0x03=状态上报）
DATA[4]: 数据长度
DATA[5..N]: 应用特定数据
```

**音乐播放控制格式**（扩展码 0x10）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x10
DATA[2]: 控制类型
  - 0x00: 播放
  - 0x01: 暂停
  - 0x02: 停止
  - 0x03: 下一首
  - 0x04: 上一首
  - 0x05: 设置进度（DATA[3-6] = 毫秒，32位大端）
  - 0x06: 设置音量（DATA[3] = 0-100）
  - 0x07: 设置播放模式（0x00=顺序, 0x01=随机, 0x02=单曲循环, 0x03=列表循环）
  - 0x08: 获取播放列表
  - 0x09: 选择歌曲（DATA[3] = 歌曲索引）
DATA[3..N]: 参数（根据控制类型）
```

**音乐状态上报格式**（扩展码 0x11）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x11
DATA[2]: 播放状态（0x00=停止, 0x01=播放中, 0x02=暂停）
DATA[3-6]: 当前进度（毫秒，32位大端）
DATA[7-10]: 总时长（毫秒，32位大端）
DATA[11]: 当前音量（0-100）
DATA[12]: 播放模式
DATA[13..N]: 当前歌曲信息（歌曲名长度 + 歌曲名 + 艺术家长度 + 艺术家）
```

**文件列表格式**（扩展码 0x12）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x12
DATA[2]: 路径长度
DATA[3..N]: 路径字符串
后续帧: [文件数量][文件1类型][文件1名长度][文件1名...][文件2...]
```

**蓝牙设备格式**（扩展码 0x15）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x15
DATA[2]: 设备数量
DATA[3]: 设备1状态（0x00=已配对, 0x01=已连接, 0x02=发现中）
DATA[4]: 设备1名称长度
DATA[5..N]: 设备1名称
DATA[N+1]: 设备1 MAC 地址（6字节）
... 后续设备
```

**健康监测数据格式**（扩展码 0x19）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x19
DATA[2]: 数据类型（0x00=心率, 0x01=血氧, 0x02=血压, 0x03=体温）
DATA[3-4]: 数值（16位大端，心率=次/分，血氧=0.1%，血压=mmHg，体温=0.1°C）
DATA[5-8]: 时间戳（32位大端，Unix时间）
```

**电源状态格式**（扩展码 0x1A）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x1A
DATA[2-3]: 电池电压（mV，16位大端）
DATA[4-5]: 电池电流（mA，16位有符号大端，负值=放电，正值=充电）
DATA[6]: 电池电量百分比（0-100）
DATA[7]: 充电状态（0x00=未充电, 0x01=充电中, 0x02=已充满, 0x03=充电错误）
DATA[8-9]: 电池温度（0.1°C，16位有符号大端）
DATA[10-11]: 预计剩余时间（分钟，16位大端，0xFFFF=未知）
```

**配置保存/加载格式**（扩展码 0x20 / 0x21）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x20（保存）或 0x21（加载）
DATA[2]: 配置类型
  - 0x00: 全部配置
  - 0x01: 背光亮度
  - 0x02: 屏幕旋转
  - 0x03: 自动息屏设置
  - 0x04: 主题/配色
  - 0x05: 音量
  - 0x06: 调试信息显示
  - 0x07: 设备名称
  - 0x08~0xFF: 预留
DATA[3]: 配置数据长度
DATA[4..N]: 配置数据（根据配置类型）
```

**大数据流传输格式**（扩展码 0x22）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x22
DATA[2]: 传输ID（唯一标识一次传输，0-255）
DATA[3]: 帧序号（0-based，最后一帧标记为 0xFF 表示结束）
DATA[4]: 总帧数（1-255，0xFF 表示未知/流式传输）
DATA[5-6]: 数据偏移（字节，16位大端，可选）
DATA[7]: 本帧数据长度
DATA[8..N]: 数据载荷
```
> **说明**：大数据传输用于图片、文件、电子书章节等。发送方分帧发送，接收方按传输ID和帧序号重组。如果帧序号不连续，接收方可请求重传（通过 CMD_DISP_EXT_REQUEST_DATA）。

**Display 请求数据格式**（扩展码 0x23）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x23
DATA[2]: 请求类型
  - 0x00: 请求文件列表
  - 0x01: 请求文件内容
  - 0x02: 请求图片数据
  - 0x03: 请求音乐列表
  - 0x04: 请求音乐状态
  - 0x05: 请求蓝牙设备列表
  - 0x06: 请求 WiFi 列表
  - 0x07: 请求电源状态
  - 0x08: 请求健康数据
  - 0x09: 请求模块状态
  - 0x0A: 请求系统信息
  - 0x0B~0xFF: 预留
DATA[3..N]: 请求参数（根据请求类型，如路径、ID 等）
```

**Display 事件上报格式**（扩展码 0x24）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x24
DATA[2]: 事件类型
  - 0x00: 用户点击按钮
  - 0x01: 用户切换页面
  - 0x02: 用户启动应用
  - 0x03: 用户关闭应用
  - 0x04: 用户修改设置
  - 0x05: 游戏分数上报
  - 0x06: 屏幕唤醒
  - 0x07: 屏幕休眠
  - 0x08~0xFF: 预留
DATA[3]: 事件数据长度
DATA[4..N]: 事件数据（如按钮ID、应用ID、设置项ID、分数值等）
```

**Display 错误上报格式**（扩展码 0x25）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x25
DATA[2]: 错误码
  - 0x00: SSD1963 通信失败
  - 0x01: 触摸屏通信失败
  - 0x02: UART 通信错误
  - 0x03: 内存不足
  - 0x04: 渲染超时
  - 0x05: 配置保存失败
  - 0x06: 配置加载失败
  - 0x07~0xFF: 预留
DATA[3]: 错误信息长度
DATA[4..N]: 错误信息字符串（可选）
```

**OTA 升级状态格式**（扩展码 0x26）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x26
DATA[2]: 状态
  - 0x00: 进入升级模式
  - 0x01: 下载中
  - 0x02: 校验中
  - 0x03: 烧录中
  - 0x04: 升级完成
  - 0x05: 升级失败
DATA[3]: 进度（0-100，仅下载/烧录状态有效）
DATA[4]: 错误码（仅失败状态有效，0=无错误）
DATA[5..N]: 错误信息（可选）
```

**WiFi 扫描/连接格式**（扩展码 0x27 / 0x28 / 0x29）：
```
// WiFi 扫描请求（Display -> Core）
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x27
DATA[2]: 0x00（开始扫描）

// WiFi 扫描结果（Core -> Display）
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x27
DATA[2]: 状态（0x00=扫描中, 0x01=完成, 0x02=失败）
DATA[3]: 网络数量
DATA[4]: 网络1 SSID 长度
DATA[5..N]: 网络1 SSID
DATA[N+1]: 网络1 信号强度（dBm，有符号）
DATA[N+2]: 网络1 加密类型（0=开放, 1=WPA, 2=WPA2, 3=WPA3）
... 后续网络

// WiFi 连接（Display -> Core）
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x28
DATA[2]: SSID 长度
DATA[3..N]: SSID
DATA[N+1]: 密码长度
DATA[N+2..M]: 密码（明文或预共享密钥）

// WiFi 状态（Core -> Display）
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x29
DATA[2]: 连接状态（0x00=断开, 0x01=连接中, 0x02=已连接）
DATA[3]: 信号强度（dBm，有符号）
DATA[4]: IP 地址（4字节）
DATA[5]: 子网掩码（4字节）
DATA[6]: 网关（4字节）
DATA[7]: MAC 地址（6字节）
```

**副屏控制格式**（扩展码 0x2A / 0x2B）：
```
// 副屏内容设置
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x2A
DATA[2]: 内容类型（0x00=关闭, 0x01=镜像主屏, 0x02=自定义内容, 0x03=状态信息）
DATA[3..N]: 内容数据（根据类型）

// 副屏配置
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x2B
DATA[2]: 分辨率（0x00=800x480, 0x01=640x480, 0x02=自定义）
DATA[3]: 方向（0x00=0°, 0x01=90°, 0x02=180°, 0x03=270°）
DATA[4]: 开关（0x00=关, 0x01=开）
```

**屏幕唤醒/休眠事件格式**（扩展码 0x2C / 0x2D）：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x2C（唤醒）或 0x2D（休眠）
DATA[2]: 事件源
  - 0x00: 触摸唤醒
  - 0x01: 按键唤醒
  - 0x02: UART 命令唤醒
  - 0x03: 定时唤醒
  - 0x04: 自动息屏超时休眠
  - 0x05: 命令休眠
  - 0x06: 省电模式休眠
```

##### 5.1.4 应用ID定义

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
| `0x10` | Tetris | 俄罗斯方块（本地游戏，不通过UART交互） |
| `0x11` | 2048 | 2048游戏（本地游戏） |
| `0x12` | Snake | 贪吃蛇（本地游戏） |
| `0x13` | Breakout | 打砖块（本地游戏） |
| `0x14~0x7F` | 预留 | 未来扩展 |
| `0x80~0xFF` | 用户自定义 | 用户自定义应用 |

##### 5.1.5 页面编号定义

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

#### 5.2 Keyboard（键盘模块）

| 操作码 | 宏名  | 方向  | 说明  |
| --- | --- | --- | --- |
| `0x21` | `CMD_KBD_GET_LAYOUT` | Core -> Keyboard | 查询当前键位布局。 |
| `0x22` | `CMD_KBD_SET_BACKLIGHT` | Core -> Keyboard | 设置键盘背光。DATA[0] = 模式/亮度。 |
| `0x23` | `CMD_KBD_KEY_DOWN` | Keyboard -> Core | 按键按下事件。DATA[0..5] = 键码数组。 |
| `0x24` | `CMD_KBD_KEY_UP` | Keyboard -> Core | 按键释放事件。 |
| `0x25` | `CMD_KBD_FN_COMBO` | Keyboard -> Core | Fn 组合键事件上报。 |

#### 5.3 Power（供电模块）

| 操作码 | 宏名  | 方向  | 说明  |
| --- | --- | --- | --- |
| `0x31` | `CMD_PWR_GET_BATTERY` | Core -> Power | 查询电池电量百分比。 |
| `0x32` | `CMD_PWR_GET_VOLTAGE` | Core -> Power | 查询电池电压（mV）。 |
| `0x33` | `CMD_PWR_SET_CHARGE` | Core -> Power | 设置充电策略。DATA[0] = 开关/电流档位。 |
| `0x34` | `CMD_PWR_BAT_LOW_EVT` | Power -> Core | 低电量告警事件。DATA[0] = 当前电量。 |
| `0x35` | `CMD_PWR_CHARGE_EVT` | Power -> Core | 充电状态变化事件（插入/拔出/充满）。 |

#### 5.4 Submodels（配件模块）

| 操作码 | 宏名  | 方向  | 说明  |
| --- | --- | --- | --- |
| `0x41` | `CMD_SUB_GET_TYPE` | Core -> Submodel | 查询配件类型（与通用 `CMD_GET_TYPE` 语义相同，可复用）。 |
| `0x42` | `CMD_SUB_GET_DATA` | Core -> Submodel | 读取配件传感器/状态数据。 |
| `0x43` | `CMD_SUB_SET_DATA` | Core -> Submodel | 向配件写入控制数据。 |
| `0x44` | `CMD_SUB_EVT_DATA` | Submodel -> Core | 配件数据主动上报。 |

#### 5.5 CH585F（无线模块，SPI 接口）

CH585F 使用 SPI 进行物理通信，但在应用层数据包中嵌入相同的协议帧格式：

| 操作码 | 宏名  | 方向  | 说明  |
| --- | --- | --- | --- |
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

| 模块  | 建议接收缓冲区大小 | 说明  |
| --- | --- | --- |
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