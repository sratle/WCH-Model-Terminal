# Display 模块通信协议

## 1. 模块概述

- **模块类型编号**：`0x01`
- **模块 ID**：`0x10`（预留范围 `0x10~0x12`）
- **物理接口**：UART4（921600/8-N-1）
- **主要交互核心**：V5F
- **说明**：UART4 由 V5F 独占访问。V3F 如有显示相关指令，通过跨核消息（共享内存 / HSEM）交由 V5F 统一发送。

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
| `0x1A` | `CMD_DISP_SHOW_NOTICE` | Core -> Display | 显示通知弹窗 | `[优先级][标题长度][标题...][内容长度][内容...]` |
| `0x1B` | `CMD_DISP_MUSIC_CONTROL` | Display -> Core | 音乐播放控制 | `[控制类型]` |
| `0x1C` | `CMD_DISP_MUSIC_STATUS` | Core -> Display | 音乐播放状态上报 | `[播放状态][进度高][进度低][总时长高][总时长低][音量]` |
| `0x1D` | `CMD_DISP_VOLUME_CONTROL` | 双向 | 音量设置/获取 | `[操作: 0=设置, 1=查询][音量值(设置时)]` |
| `0x1E` | `CMD_DISP_ETHERNET_STATUS` | Core -> Display | 以太网状态上报/响应 | `[连接状态][IP地址...][子网掩码...][网关...]` |
| `0x1F` | `CMD_DISP_FACTORY_RESET` | Core -> Display | 恢复出厂设置 | `[确认码: 0xA5]` |

### 2.2 扩展操作码（CMD = 0x10，DATA[0] 为子命令）

| 扩展码 | 宏名 | 方向 | 说明 | DATA 格式 |
| --- | --- | --- | --- | --- |
| `0x01` | `CMD_DISP_EXT_APP_LAUNCH` | Core -> Display | 启动指定应用 | `[应用ID]` |
| `0x02` | `CMD_DISP_EXT_APP_CLOSE` | Core -> Display | 关闭当前应用 | 无 |
| `0x03` | `CMD_DISP_EXT_APP_DATA` | 双向 | 应用数据传输 | `[应用ID][数据类型][数据长度][数据...]` |
| `0x04` | `CMD_DISP_EXT_MODULE_STATUS` | Core -> Display | 模块状态主动上报（插拔/在线/离线） | 见下方模块状态格式 |
| `0x05` | `CMD_DISP_EXT_GET_MODULE_STATUS` | Display -> Core | 请求获取当前模块组成信息 | 无，Core 回复 `0x04` |
| `0x06` | `CMD_DISP_EXT_REQUEST_FILE_LIST` | Display -> Core | 请求当前目录文件列表 | `[路径长度][路径字符串...]` |
| `0x07` | `CMD_DISP_EXT_FILE_LIST` | Core -> Display | 文件列表数据响应 | `[路径长度][路径...][文件数量][文件信息...]` |
| `0x08` | `CMD_DISP_EXT_FILE_READ` | Display -> Core | 请求读取文件（触发 Bulk Mode） | `[文件路径...]` |
| `0x09` | `CMD_DISP_EXT_FILE_SAVE` | Display -> Core | 请求保存文件（触发 Bulk Mode） | `[文件路径...]` |
| `0x0A` | `CMD_DISP_EXT_FILE_OPERATION` | 双向 | 文件操作（创建/删除/重命名） | `[操作类型][路径长度][路径...]` |
| `0x0B` | `CMD_DISP_EXT_PLAY_MUSIC` | Display -> Core | 请求播放指定 wav 文件 | `[文件路径...]` |
| `0x0C` | `CMD_DISP_EXT_BT_SCAN` | Display -> Core | 请求扫描蓝牙设备 | 无 |
| `0x0D` | `CMD_DISP_EXT_BT_DEVICE_LIST` | Core -> Display | 蓝牙设备列表（扫描结果） | `[设备数量][设备信息...]` |
| `0x0E` | `CMD_DISP_EXT_BT_CONNECT` | Display -> Core | 请求连接/断开蓝牙设备 | `[操作: 0=断开, 1=连接][MAC地址(6字节)]` |
| `0x0F` | `CMD_DISP_EXT_BT_STATUS` | Core -> Display | 蓝牙连接状态变化上报 | `[状态][设备MAC...]` |
| `0x10` | `CMD_DISP_EXT_FP_STATUS` | Core -> Display | 指纹识别信息上报 | `[状态][指纹ID][匹配度]` |
| `0x11` | `CMD_DISP_EXT_NFC_TAG` | Core -> Display | NFC 识别信息上报 | `[标签类型][UID长度][UID...][数据长度][数据...]` |
| `0x12` | `CMD_DISP_EXT_HEALTH_DATA` | Core -> Display | 健康监测信息上报 | `[数据类型][数值高][数值低][时间戳...]` |
| `0x13` | `CMD_DISP_EXT_POWER_STATUS` | Core -> Display | 电源状态信息上报 | `[电压高][电压低][电流高][电流低][电量][充电状态]` |
| `0x14` | `CMD_DISP_EXT_IR_RANGE_REQ` | Display -> Core | 请求红外测距 | 无 |
| `0x15` | `CMD_DISP_EXT_IR_RANGE_DATA` | Core -> Display | 红外测距结果返回 | `[距离高][距离低][单位][精度]` |
| `0x16` | `CMD_DISP_EXT_SAVE_CONFIG` | Display -> Core | 请求保存配置到 TF 卡 | `[文件路径...][配置数据...]` |
| `0x17` | `CMD_DISP_EXT_LOAD_CONFIG` | Display -> Core | 请求加载配置文件 | `[文件路径...]` |
| `0x18` | `CMD_DISP_EXT_CONFIG_RESULT` | Core -> Display | 配置保存/加载结果 | `[结果: 0=成功, 1=失败][配置数据...]` |
| `0x19` | `CMD_DISP_EXT_SET_RGB_MODE` | Display -> Core | 设置 RGB 灯效模式 | `[灯效ID][颜色R][颜色G][颜色B][亮度][速度]` |
| `0x1A` | `CMD_DISP_EXT_BULK_TRANSFER` | 双向 | 批量传输控制/确认 | 见下方批量传输协议 |
| `0x1B` | `CMD_DISP_EXT_SUBDISP_CONTENT` | Core <-> Display | 副屏显示内容设置 | `[内容类型][内容数据...]` |
| `0x1C` | `CMD_DISP_EXT_SUBDISP_CONFIG` | Core <-> Display | 副屏配置 | `[分辨率][方向][开关]` |
| `0x1D` | `CMD_DISP_EXT_SCREEN_WAKEUP` | Display -> Core | 屏幕唤醒事件 | `[唤醒源]` |
| `0x1E` | `CMD_DISP_EXT_SCREEN_SLEEP` | Display -> Core | 屏幕休眠事件 | `[休眠源]` |
| `0x1F` | `CMD_DISP_EXT_ERROR_REPORT` | Display -> Core | Display 错误/异常上报 | `[错误码][错误信息...]` |
| `0x20` | `CMD_DISP_EXT_REPORT_EVENT` | Display -> Core | Display 用户事件上报 | `[事件类型][事件数据...]` |
| `0x21` | `CMD_DISP_EXT_REQUEST_DATA` | Display -> Core | Display 通用数据请求 | `[请求类型][参数...]` |
| `0x22~0x3F` | — | — | 预留 | — |

### 2.3 已删除的操作码

以下操作码在旧版协议中存在，现已移除：

| 原操作码 | 原功能 | 删除原因 |
| --- | --- | --- |
| 扩展 `0x01/0x02` | 二维码 | 功能删除 |
| 扩展 `0x08` | 游戏控制 | 游戏走 `APP_DATA(0x03)` 或本地处理 |
| 扩展 `0x14` | 图片数据流 | 图片走 `FILE_READ(0x08)` + Bulk Mode |
| 扩展 `0x27/0x28/0x29` | WiFi 扫描/连接/状态 | 替换为以太网 `0x1E` |
| 基础 `0x1C` | 系统信息获取 | 功能删除 |
| 扩展 `0x1F` | OTA 状态 | 暂不支持 OTA |

---

## 3. 批量传输协议（Bulk Transfer Mode）

### 3.1 设计背景

统一协议单帧 DATA 域上限为 255 字节（`LEN` 字段 1 字节）。若用传统分帧方式传输 400KB 文件，需要约 1700 帧，协议头尾开销大、CPU 中断频繁。

**批量传输协议** 在不修改统一协议帧格式的前提下，通过 **标准协议帧握手 + UART DMA Raw 数据传输** 实现大文件高效传输。

### 3.2 核心机制

- **控制阶段**：使用标准协议帧（`0xAA ... A5 5A FC FD`）进行握手和确认
- **数据阶段**：发送方直接通过 UART DMA 发送 **原始文件数据**（无协议头尾），接收方用 **DMA + IDLE 中断** 接收
- **块大小**：由 ACK 帧协商，**默认 20KB**，最大不超过 `CH378_MAX_FILE_SIZE`（400KB）
- **无 CRC 校验**：不实现错码修正，误码由应用层自行兜底（如花屏则重传）

### 3.3 UART IDLE 中断

IDLE 中断在接收线空闲超过 **1 个字节时间**（921600 波特率下约 10.8μs）时触发。配合 DMA 使用，可解决最后一块不足 20KB 的收尾问题：
- **整 block（20KB）**：DMA 收满触发 `TCIF` 完成中断
- **尾 block（<20KB）**：发送方停发后 UART 线空闲，触发 `IDLE` 中断，CPU 读取 DMA 剩余计数器得到实际接收长度

### 3.4 传输流程

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

### 3.5 注意事项

- Bulk Mode 期间 UART 总线被独占，不应插入其他标准协议帧
- 发送方在发送 Raw 数据前必须确保上一帧标准协议已完全发送（TC 标志置位）
- 接收方在进入 Bulk Mode 前需配置好 DMA 和 IDLE 中断
- 若文件大小 ≤ 块大小，仅发送一块即可

---

## 4. 数据格式详解

### 4.1 统一输入事件格式（CMD 0x15）

```
DATA[0]: 设备类型
  - 0x00: 键盘（来自 Keyboard 模块）
  - 0x01: 鼠标（来自 CH9350）
  - 0x02: 触摸板（来自 CH9350）
  - 0x03: 虚拟触摸（Core 模拟）
DATA[1]: 事件类型
  - 键盘: 0x00=按下, 0x01=释放, 0x02=长按
  - 鼠标: 0x00=移动, 0x01=左键按下, 0x02=左键释放, 0x03=右键按下, 0x04=右键释放, 0x05=滚轮
  - 触摸/虚拟触摸: 0x00=按下, 0x01=移动, 0x02=释放, 0x03=长按, 0x04=双击
DATA[2]: 键码 / X 坐标高字节 / 鼠标 X 偏移（有符号）
DATA[3]: 修饰键 / X 坐标低字节 / 鼠标 Y 偏移（有符号）
DATA[4]: Y 坐标高字节 / 鼠标滚轮偏移（有符号，仅滚轮事件）
DATA[5]: Y 坐标低字节
```

### 4.2 屏幕控制格式（CMD 0x18）

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

### 4.3 屏幕状态响应格式（CMD 0x19 ACK）

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

### 4.4 状态栏更新格式（CMD 0x14）

```
DATA[0]: 更新掩码（位域，指示哪些字段有效）
  - bit0: 电量
  - bit1: 蓝牙状态
  - bit2: 以太网状态
  - bit3: 时间
  - bit4: 日期
  - bit5: 核心模块连接状态
  - bit6: 音量
  - bit7: 预留
DATA[1]: 电量百分比（0-100，若 bit0=1）
DATA[2]: 蓝牙状态（0=断开, 1=已连接, 2=配对中，若 bit1=1）
DATA[3]: 以太网状态（0=断开, 1=已连接, 2=连接中，若 bit2=1）
DATA[4-7]: 时间戳（Unix 时间，32位大端，若 bit3=1）
DATA[8-11]: 日期（YYYYMMDD 格式，32位大端，若 bit4=1）
DATA[12]: 核心模块状态（0=离线, 1=在线, 2=休眠，若 bit5=1）
DATA[13]: 音量（0-100，若 bit6=1）
```

### 4.5 模块状态格式（扩展码 0x04 / 0x05）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x04（主动上报）或 0x05（响应查询）
DATA[2]: 模块数量 N
DATA[3..6]:  模块1信息 [模块ID][类型][子类型][在线状态]
DATA[7..10]: 模块2信息 ...
...
在线状态: 0=离线, 1=在线, 2=错误, 3=休眠
```

### 4.6 文件列表格式（扩展码 0x07）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x07
DATA[2]: 路径长度
DATA[3..N]: 路径字符串
DATA[N+1]: 文件数量 M
DATA[N+2]: 文件1类型（0=文件, 1=文件夹）
DATA[N+3]: 文件1名称长度
DATA[N+4..]: 文件1名称
... 后续文件
```

### 4.7 音乐播放控制格式（基础码 0x1B / 扩展码 0x0B）

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
DATA[2]: 文件路径长度
DATA[3..N]: 文件路径字符串（如 "/music/song.wav"）
```

### 4.8 音乐状态上报格式（基础码 0x1C）

```
DATA[0]: 播放状态（0x00=停止, 0x01=播放中, 0x02=暂停）
DATA[1-4]: 当前进度（毫秒，32位大端）
DATA[5-8]: 总时长（毫秒，32位大端）
DATA[9]: 当前音量（0-100）
DATA[10]: 播放模式（0x00=顺序, 0x01=随机, 0x02=单曲循环, 0x03=列表循环）
DATA[11]: 当前歌曲名长度
DATA[12..N]: 当前歌曲名
```

### 4.9 蓝牙设备格式（扩展码 0x0D）

```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x0D
DATA[2]: 设备数量 N
DATA[3]:  设备1状态（0x00=已配对, 0x01=已连接, 0x02=发现中）
DATA[4]:  设备1名称长度
DATA[5..M]: 设备1名称
DATA[M+1..M+6]: 设备1 MAC 地址（6字节）
... 后续设备
```

### 4.10 配置保存/加载格式（扩展码 0x16 / 0x17 / 0x18）

**请求保存（Display → Core，0x16）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x16
DATA[2]: 文件路径长度
DATA[3..N]: 文件路径
DATA[N+1]: 配置数据长度（若 ≤200 字节可直接携带）
DATA[N+2..]: 配置数据
```

> 若配置数据 > 200 字节，DATA 中不携带配置内容，走 `BULK_TRANSFER(0x1A)` 传输。

**请求加载（Display → Core，0x17）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x17
DATA[2]: 文件路径长度
DATA[3..N]: 文件路径
```

**结果响应（Core → Display，0x18）**：
```
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x18
DATA[2]: 结果（0=成功, 1=失败）
DATA[3]: 错误码（仅失败时有效）
DATA[4]: 配置数据长度（加载成功时）
DATA[5..N]: 配置数据（加载成功时，若 ≤200 字节）
```

### 4.11 以太网状态格式（基础码 0x1E）

```
DATA[0]: 连接状态（0x00=断开, 0x01=连接中, 0x02=已连接）
DATA[1..4]: IP 地址（4字节）
DATA[5..8]: 子网掩码（4字节）
DATA[9..12]: 网关（4字节）
DATA[13..18]: MAC 地址（6字节）
```

### 4.12 副屏控制格式（扩展码 0x1B / 0x1C）

```
// 副屏内容设置（0x1B）
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x1B
DATA[2]: 内容类型（0x00=关闭, 0x01=镜像主屏, 0x02=自定义内容, 0x03=状态信息）
DATA[3..N]: 内容数据（根据类型）

// 副屏配置（0x1C）
DATA[0]: 基础操作码 = 0x10
DATA[1]: 扩展码 = 0x1C
DATA[2]: 分辨率（0x00=800x480, 0x01=640x480, 0x02=自定义）
DATA[3]: 方向（0x00=0°, 0x01=90°, 0x02=180°, 0x03=270°）
DATA[4]: 开关（0x00=关, 0x01=开）
```

### 4.13 屏幕唤醒/休眠事件格式（扩展码 0x1D / 0x1E）

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

## 5. 应用 ID 定义

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

## 6. 页面编号定义

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

## 7. CMD_GET_TYPE 响应

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

## 8. 版本记录

| 版本 | 日期 | 说明 |
|------|------|------|
| V1.0 | 2026-04-22 | 初始版本 |
| V2.0 | 2026-04-24 | 重构操作码：删除二维码/WiFi/OTA/系统信息/图片独立码；新增统一输入事件、批量传输协议、以太网状态、RGB控制 |
