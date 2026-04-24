# Display 模块通信协议

## 1. 模块概述

- **模块类型编号**：`0x01`
- **模块 ID**：`0x10`（预留范围 `0x10~0x12`）
- **物理接口**：UART4（921600/8-N-1）
- **主要交互核心**：V5F
- **说明**：UART4 由 V5F 独占访问。V3F 如有显示相关指令，通过跨核消息（共享内存 / HSEM）交由 V5F 统一发送。

## 2. 操作码定义

Display 模块操作码分为两类：
- **基础操作码**（`0x11~0x1F`）：直接对应常用功能。
- **扩展操作码**：当基础操作码为 `0x10` 时，`DATA[0]` 作为扩展操作码，支持更多功能。

### 2.1 基础操作码（0x11 ~ 0x1F）

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

### 2.2 扩展操作码（CMD = 0x10，DATA[0] 为子命令）

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

## 3. 数据格式详解

### 3.1 触摸屏/虚拟触摸事件格式（CMD 0x15 / 0x1A）

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

### 3.2 键盘/鼠标事件格式（CMD 0x1B）

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

### 3.3 状态栏更新格式（CMD 0x14）

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

### 3.4 模块状态格式（扩展码 0x06）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x06
DATA[2]: 模块ID（0x01=CH585F, 0x20=Keyboard, 0x30=Power, 0x40-0x42=Submodels）
DATA[3]: 状态（0=离线, 1=在线, 2=错误, 3=休眠）
DATA[4]: 附加数据长度
DATA[5..N]: 附加数据（模块特定）
```

### 3.5 应用数据传输格式（扩展码 0x05）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x05
DATA[2]: 应用ID（见下方应用ID定义）
DATA[3]: 数据类型（0x00=命令, 0x01=查询, 0x02=响应, 0x03=状态上报）
DATA[4]: 数据长度
DATA[5..N]: 应用特定数据
```

### 3.6 音乐播放控制格式（扩展码 0x10）

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

### 3.7 音乐状态上报格式（扩展码 0x11）

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

### 3.8 文件列表格式（扩展码 0x12）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x12
DATA[2]: 路径长度
DATA[3..N]: 路径字符串
后续帧: [文件数量][文件1类型][文件1名长度][文件1名...][文件2...]
```

### 3.9 蓝牙设备格式（扩展码 0x15）

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

### 3.10 健康监测数据格式（扩展码 0x19）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x19
DATA[2]: 数据类型（0x00=心率, 0x01=血氧, 0x02=血压, 0x03=体温）
DATA[3-4]: 数值（16位大端，心率=次/分，血氧=0.1%，血压=mmHg，体温=0.1°C）
DATA[5-8]: 时间戳（32位大端，Unix时间）
```

### 3.11 电源状态格式（扩展码 0x1A）

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

### 3.12 配置保存/加载格式（扩展码 0x20 / 0x21）

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

### 3.13 大数据流传输格式（扩展码 0x22）

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

> **说明**：大数据传输用于图片、文件、电子书章节等。发送方分帧发送，接收方按传输ID和帧序号重组。如果帧序号不连续，接收方可请求重传（通过 `CMD_DISP_EXT_REQUEST_DATA`）。

### 3.14 Display 请求数据格式（扩展码 0x23）

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

### 3.15 Display 事件上报格式（扩展码 0x24）

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

### 3.16 Display 错误上报格式（扩展码 0x25）

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

### 3.17 OTA 升级状态格式（扩展码 0x26）

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

### 3.18 WiFi 扫描/连接格式（扩展码 0x27 / 0x28 / 0x29）

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

### 3.19 副屏控制格式（扩展码 0x2A / 0x2B）

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

### 3.20 屏幕唤醒/休眠事件格式（扩展码 0x2C / 0x2D）

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

## 4. 应用 ID 定义

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

## 5. 页面编号定义

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

## 6. CMD_GET_TYPE 响应

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
