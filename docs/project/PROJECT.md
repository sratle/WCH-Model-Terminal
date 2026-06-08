# 基于青稞RISC-V的嵌入式模块化移动终端

## 作品简介

本作品采用青稞RISC-V架构的CH32H417芯片作为核心芯片，旨在设计一台模块化的个人移动终端，
满足用户对移动设备的多样化需求。该移动终端主要分成核心底板、供电模块、显示模块、配件模块、
键盘模块五个部分，支持各个部分的独立更换和定制，提供灵活的硬件扩展能力和定制化体验。
模块可在核心底板上自由组合，终端自动识别其组成，最终实现文本编辑器、电子书、播放器、
电子琴、游戏机等多样功能，替代多种单一功能终端。

## 分模块硬件和功能描述

### 核心模块main-model

核心模块是一整块PCB，上面有CH378、CH9350L、CS43131、CH32H417QEU6、CH585F这些主要的芯片。
供电来自于Power模块，然后在这块PCB上将供电再传输给核心板上连接的其他模块。
核心板上可以连接的模块包括Power模块一个、Keyboard模块一个、Display模块一个，
Submodel模块三个
核心模块还支持串口连接CLI控制设备。

- CH378是一个文件管理芯片，通过SPI1和CH32H417通信，CH378下接一个USB-A和一个TF卡座，
  可以读取和操作U盘、TF卡中的文件，文件系统为FAT32，里面的文件主要为txt、wav、md、bmp文件，
  会有文件夹，满足基础的音乐播放、图片显示、文件编辑需求。会存储裸文件作为配置持久化的措施，
  也就是会从CH378下挂的TF卡中读取配置。
- CH9350L是一个HID转UART芯片，接了一个USB-A接口，可以连接外部的标准HID输入设备，
  是Host口，负责接收外部来的HID信息并转换为串口通过UART1传给CH32H417。
- CS43131是一个音频DAC芯片，接了外部22.5782MHz晶振，作为I2S1 Master和CH32H417通信，
  CS43131输出BCK、LRCK，CH32H417作为I2S Slave在时钟同步下通过DMA向CS43131发送音频数据（SDIN），
  CS43131需要通过I2C2让CH32H417配置它，配置为44.1KHz 16bit 双声道，输出HPOA和HPOB，
  可以通过一个模拟开关芯片使用CTRLA/CTRLB信号切换是否输出至音响
  （这部分通过HPOAB后面的功放芯片实现，功放芯片有SHUTDOWN和Core连接，
  因此HPOAB是需要一直输出的）。
- CH585F是一个独立主控wireless-model，作为BLE芯片使用，
  通过SPI4和CH32H417连接，其UART1作为DEBUG。
  BLE工作在Central + Peripheral双角色模式：Central角色支持扫描并连接BLE键盘、BLE鼠标（HID over GATT），
  HID事件通过SPI上报给Core，由Core转发给Display模块；Peripheral角色支持移动端APP作为Central连接CH585F，
  输入命令控制设备，相当于蓝牙SSH CLI，同时支持小文件传输。
  有配对记忆功能，可以配对BLE设备后记住配对过哪些。
- CH32H417作为核心为core-model，内部集成V5F（400MHz）和V3F（100MHz）两个RISC-V核心。
  V5F为唯一执行核心，负责所有模块的初始化、中断处理与业务逻辑；
  V3F仅负责上电初始化时钟、唤醒V5F，然后进入STOP低功耗模式。
  核心CH32H417连接了一个以太网口HR911105A，并且连接了上述四块芯片，负责调度四块芯片和其他模块，
  其UART2作为DEBUG，引出了UART3-8作为连接其他模块的接口，每个UART连接一种模块，
  分配为UART3-Keyboard，UART4-Display，UART5-Power，UART6-Submodel1，
  UART7-Submodel2，UART8-Submodel3。所有UART均由V5F统一管理，不存在核间通信。

### 键盘模块

键盘模块有三种，主控均为CH32V103C8T6，UART1作为和核心模块的接口，波特率230400。

- 主键盘Keyboard-1，大致为40配列，主控通过UART2和CH9329进行连接，
  CH9329是一款串口转HID芯片，转换为HID之后输出到USB-A口，是Device口，可以单独作为标准USB HID设备给其他设备使用。
- 游戏键盘Keyboard-2,大致为上下左右方向键、一个摇杆、四个自定义按键，
  也有UART2连接CH9329并输出到USB-A。
- 音乐键盘Keyboard-3,主要包括16个按键作为琴键、两个旋钮、四个控制按键，没有CH9329。

### 供电模块

供电模块只有一种，主控为CH32V103C8T6, UART1作为和核心模块的接口，波特率230400。

- 供电模块Power连接了一个3500mAh的电池，并且可以读取电量，可以单独拿出来作为充电宝使用，
  支持PD快充也支持无线充电还支持输出慢充，可以读取当前的供电/充电功率。通过接口给核心模块和其他所有模块供5V电。

### 显示模块

显示模块有两种，一种为RGB888的800\*480LCD屏幕，一种为黑白墨水屏。
均采用UART1作为与核心模块的接口，波特率921600。屏幕上运行MiniUI（自建UI架构）。

- LCD显示模块Display-1, 主控为CH32H417QEU6, 通过FMC 8080并口连接SSD1963, 
  再连接LCD屏幕。SSD1963是一款LCD驱动芯片，负责将CH32H417输出的RGB565转换为RGB888存储到GRAM, 
  数据一帧输出到RGB888的LCD屏幕，达到刷新的效果。采用DE模式，SSD1963有PWM可以操控背光亮度。
- Eink显示模块Display-2, 主控为CH32V307RCT6，通过SPI驱动墨水屏，墨水屏支持局部刷新。
  目前技术方案未确定，可能是800\*480的4.26寸触摸黑白墨水屏，
  也可能是648\*480的不支持触摸黑白墨水屏。

### 扩展模块

扩展模块sub-model一共有七种，均采用CH32V103C8T6作为主控，波特率230400

- submodel-1, 指纹识别finger，主要功能是存储指纹数据并上报识别ID成功/失败，也会上报指纹数据。
- submodel-2, 健康监测health, 主要功能是检测血氧、心跳、温湿度数据并上报。
- submodel-3, NFC读卡nfc, 主要功能是存储NFC数据并上报识别ID成功/失败。
- submodel-4, 触摸圆环/旋钮touch, 主要功能是可以通过触摸圆环发出操控指令，也可以通过旋钮控制。
- submodel-5, RGB点阵rgb, 主要功能是控制WS2812亮灯，接受指令以不同模式炫彩。
- submodel-6, 红外测距infrared, 主要功能是通过红外测距。
- submodel-7, 副屏显示subdisplay, 主要功能是接受一些数据，
  在2.13寸122*250全反屏上显示简单的LOGO和当前状态。

## 分程序项目描述（TODO）

## Core

### CLI 命令行接口模块

Core 固件内置一个交互式 CLI（Command Line Interface），作为设备的调试与文件管理特性，通过 **USART2（PA2/PA3, 115200 8-N-1）** 与用户交互。

#### 架构设计

CLI 是一个**独立模块**，源码位于 `Common/Common/CLI/`，与 CH378 驱动完全解耦：

- `CLI.c` / `CLI.h`：命令解析器、各命令实现、主入口 `CLI_Process()`
- `debug.c`：USART2 底层驱动、RX 中断行缓冲、轮询入口 `Debug_CLI_Process()`
- CH378 驱动仅提供文件系统 API，被 CLI 调用，自身不包含任何 CLI 代码

#### 通信流程

1. **输入**：用户通过串口助手发送命令（如 `ls`），末尾回车触发 `
   `
2. **中断接收**：`Debug_UART_IRQ_Handler()` 逐字节接收、回显、存入 `cli_rx_buf[]`
3. **命令就绪**：收到 `
   ` 后设置 `cli_cmd_ready = 1`
4. **轮询执行**：主循环 `while(1)` 中 `Debug_CLI_Process()` 检测到就绪标志，复制缓冲区并调用 `CLI_Process(buf, len)`
5. **输出**：`CLI_Process()` 解析命令词，调用对应 `CLI_Cmd_*()` 函数，通过 `printf()` 返回结果

#### 支持的命令

| 命令                      | 说明                       | 示例                     |
| ----------------------- | ------------------------ | ---------------------- |
| `ls`                    | 列出当前目录（支持显示长文件名）         | `ls`                   |
| `cd <dir>`              | 进入目录，`..` 返回上级，`/` 返回根目录 | `cd DOC`               |
| `pwd`                   | 打印当前路径                   | `pwd`                  |
| `mkdir <dir>`           | 创建目录（支持长文件名）             | `mkdir "New Folder"`   |
| `touch <file>`          | 创建空文件（支持长文件名）            | `touch config.json`    |
| `cat <file>`            | 读取并打印文件内容（支持长文件名）        | `cat readme.md`        |
| `echo <text>`           | 打印文本                     | `echo hello world`     |
| `echo <text> > <file>`  | 将文本覆盖写入文件                | `echo data > log.txt`  |
| `echo <text> >> <file>` | 将文本追加写入文件                | `echo data >> log.txt` |
| `rm <file>`             | 删除文件（支持长文件名）             | `rm old.txt`           |
| `rm -rf <dir>`          | 递归清空目录内所有文件              | `rm -rf temp`          |
| `cp <src> <dst>`        | 复制文件（支持长文件名）             | `cp a.txt b.txt`       |
| `mv <old> <new>`        | 重命名文件（仅短文件名）             | `mv OLD.TXT NEW.TXT`   |
| `hexdump <file>`        | 十六进制显示文件（支持长文件名）         | `hexdump test.bin`     |
| `head <file> [n]`       | 显示文件前 n 字节（默认 256）       | `head log.txt 64`      |
| `tail <file> [n]`       | 显示文件后 n 字节（默认 256）       | `tail log.txt 64`      |
| `tree [dir]`            | 树形列出目录结构                 | `tree`                 |
| `du <dir>`              | 显示目录总大小                  | `du DOC`               |
| `find <pattern>`        | 递归搜索文件名匹配 pattern        | `find .TXT`            |
| `df`                    | 显示磁盘总容量                  | `df`                   |
| `free`                  | 显示磁盘剩余空间                 | `free`                 |
| `device [usb            | sd]`                     | 显示或切换设备模式              |
| `stat <file>`           | 显示文件详细信息（支持长文件名）         | `stat file.txt`        |
| `chmod <file> <attr>`   | 修改文件属性（支持长文件名）           | `chmod file.txt 01`    |
| `ver`                   | 显示 CH378 固件版本            | `ver`                  |
| `clear`                 | 清屏                       | `clear`                |
| `help`                  | 显示帮助信息                   | `help`                 |

#### 长文件名支持 (LFN)

CLI 已实现完整的长文件名支持：

- **创建**：`touch`、`mkdir` 支持长文件名（如 `config.json`、`New Folder`）
- **访问**：`cat`、`rm`、`stat`、`chmod`、`hexdump`、`head`、`tail`、`cp` 均支持长文件名参数
- **显示**：`ls` 会自动尝试获取每个条目的长文件名并显示；如果无长文件名则回退到 8.3 短名
- **实现原理**：
  - `CLI_LFN_Operation()` 将用户输入的**完整路径**（如 `\DOC\test`）直接传给 CH378 LFN 命令；固件自行从路径中解析父目录与目标文件名，CLI 不再手动拆分 `dir_path` + `filename`
  - 文件名部分经 `CLI_AsciiToUnicode()` 转为 UTF-16LE（含 `0x00 0x00` 终止符），通过 `CMD10_SET_LONG_FILE_NAME` 写入 CH378
  - `ls` 使用 `CMD10_GET_LONG_FILE_NAME` 获取 LFN：返回的 `lfn_len` **精确可靠**，但数据**末尾不含 `0x00 0x00` 终止符**，必须严格按返回长度解析；若命令返回 `0x44` 表示该条目无 LFN，自动回退到 8.3 短名
  - `CH378ReadReqBlock()` 已增加 512 字节上限保护，防止固件偶发返回超长长度导致缓冲区溢出

#### 注意事项

- **CH378 目录删除限制**：CH378 固件的 `CMD0H_FILE_ERASE` 命令官方注释明确为"对目录则等待"，即不支持删除目录。`rm -rf` 会递归清空目录树内所有文件，但空目录会保留。
- **mv 限制**：`mv` 命令目前仅支持短文件名重命名，且不能跨目录移动。长文件名重命名建议用 `cp` + `rm` 替代。
- **终端换行**：MCU 中断仅在收到 `
  ` 时回显 `
  `，忽略单独的 `
  `，以避免终端发送 `
  ` 时产生重复换行。

## Wireless

## Display-1

## Display-2

## Keyboard-1

## Keyboard-2

## Keyboard-3

## Power-1

## Submodels-1

## Submodels-2

## Submodels-3

## Submodels-4

## Submodels-5

## Submodels-6

## Submodels-7

## 通信协议规范（V1.0）

### 1. 协议概述

本项目采用一套统一的紧凑二进制通信协议，覆盖 **Core** 与 **Display**、**Keyboard**、
**Power**、**Submodels**、**CH585F** 之间的所有数据交互，协议只提供分类到类型的协议，子类型不再细分协议类型。

- **物理层**：UART（921600/8-N-1），
  CH585F 使用 SPI 但数据载荷遵循本协议格式
- **数据链路层**：固定帧头的紧凑二进制帧，无校验和
- **通信模式**：异步中断驱动，收发双方均不阻塞等待
- **隔离协议**：CH9350 保持其独立的 `0x57 0xAB` 专用协议，不参与本统一协议

### 2. 帧结构

```
+--------+--------+--------+--------+--------+-------------+--------+--------+--------+--------+
|  HEAD  |  SRC   |  DST   |  LEN   |  CMD   |  DATA[0..N] | TAIL0  | TAIL1  | TAIL2  | TAIL3  |
| 1 byte | 1 byte | 1 byte | 1 byte | 1 byte |   N bytes   | 1 byte | 1 byte | 1 byte | 1 byte |
+--------+--------+--------+--------+--------+-------------+--------+--------+--------+--------+
```

| 字段       | 长度      | 说明                                                                                                                                                                               |
| -------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **HEAD** | 1 byte  | 帧头，固定值 `0xAA`。仅在接收状态机处于 `WAIT_HEAD` 状态时识别为新帧起始，数据域中出现 `0xAA` 不触发帧重解析。                                                                                                            |
| **SRC**  | 1 byte  | 源模块ID，标识发送方。                                                                                                                                                                     |
| **DST**  | 1 byte  | 目标模块ID，标识接收方。                                                                                                                                                                    |
| **LEN**  | 1 byte  | 有效载荷长度，即 `CMD + DATA` 的总字节数。`DATA` 长度 = `LEN - 1`，最大 255 字节，故最大帧长度为 260 字节。                                                                                                      |
| **CMD**  | 1 byte  | 操作码。模块专用操作码建议按模块 ID 高 4 位划分：Display=`0x1x`、Keyboard=`0x2x`、Power=`0x3x`、Submodel=`0x4x`；Wireless（模块 ID `0x01`）作为特例独立分配 `0x5x`。`0x00~0x0F` 为系统通用操作码，由各模块协议文档在 DATA 中定义子命令以扩展更多功能。 |
| **DATA** | N bytes | 数据域，长度由 `LEN` 推导，允许为 0 字节（此时 `LEN = 1`）。                                                                                                                                         |
| **TAIL** | 4 bytes | 帧尾，固定序列 `A5 5A FC FD`，用于帧结束边界确认。                                                                                                                                                 |

**最大帧传输时间**（921600 波特率）：约 2.8 ms。

### 3. 模块 ID 定义

模块 ID 与硬件编号一一对应，全系统统一：

| ID     | 模块               | 物理接口  | 主要交互核心 | 说明                            |
| ------ | ---------------- | ----- | -------- | ----------------------------- |
| `0x00` | Core（核心）         | —     | V5F      | V5F 为唯一执行核心，V3F 仅负责启动 V5F    |
| `0x01` | Wireless（无线/蓝牙）  | SPI4  | V5F      | 独立 MCU，V5F 通过 SPI 通信          |
| `0x10` | Display（屏幕模块）    | UART4 | V5F      | V5F 直接管理                      |
| `0x20` | Keyboard（键盘模块）   | UART3 | V5F      | V5F 直接管理                      |
| `0x30` | Power（供电模块）      | UART5 | V5F      | V5F 直接管理                      |
| `0x40` | Submodel-1（配件槽1） | UART6 | V5F      | V5F 直接管理                      |
| `0x41` | Submodel-2（配件槽2） | UART7 | V5F      | V5F 直接管理                      |
| `0x42` | Submodel-3（配件槽3） | UART8 | V5F      | V5F 直接管理                      |

> 预留范围：Display `0x10-0x12`、Keyboard `0x20-0x22`、Power `0x30-0x31`、Submodels `0x40-0x49`。
>
> **架构说明**：所有模块的 UART 均由 V5F 统一管理，不存在核间通信。
> 系统全局状态 `hardware_g` 为 V5F 普通全局变量（非共享内存），所有模块可直接访问，无需跨核同步。

### 4. 通用操作码（0x00 ~ 0x0F）

以下操作码为所有模块共享，用于基础握手、查询和状态同步：

| 操作码    | 宏名                | 方向             | 说明                                     |
| ------ | ----------------- | -------------- | -------------------------------------- |
| `0x00` | `CMD_NOP`         | 双向             | 心跳/空操作，可用于保活检测。                        |
| `0x01` | `CMD_GET_TYPE`    | Core -> Module | 查询模块类型。Module 回复 `CMD_ACK`，DATA 格式见下文。 |
| `0x02` | `CMD_GET_STATUS`  | Core -> Module | 查询模块当前状态。                              |
| `0x03` | `CMD_SET_CONFIG`  | Core -> Module | 向模块下发配置参数。                             |
| `0x04` | `CMD_ACK`         | 双向             | 肯定确认。DATA 可选携带响应数据。                    |
| `0x05` | `CMD_NACK`        | 双向             | 否定确认。DATA[0] 建议携带错误码。                  |
| `0x06` | `CMD_EVT_NOTIFY`  | Module -> Core | 事件主动上报，如电量变化、按键按下、蓝牙连接等。               |
| `0x07` | `CMD_DATA_STREAM` | 双向             | 纯数据流传输，用于大数据量场景（如音频、文件）。               |

#### 4.1 NACK 错误码定义

`CMD_NACK` (0x05) 的 DATA[0] 固定携带错误码，各模块统一使用以下定义：

| 错误码         | 宏名                          | 含义     | 典型使用场景                        |
| ----------- | --------------------------- | ------ | ----------------------------- |
| `0x00`      | `PROTO_ERR_NONE`            | 无错误    | 保留，不用于 NACK                   |
| `0x01`      | `PROTO_ERR_UNSUPPORTED_CMD` | 不支持的命令 | 收到未定义/未实现的操作码                 |
| `0x02`      | `PROTO_ERR_INVALID_PARAM`   | 参数错误   | 参数超出范围、非法枚举值、格式错误             |
| `0x03`      | `PROTO_ERR_LEN_MISMATCH`    | 长度不匹配  | LEN 与实际 DATA 长度不符、缺少必要字段      |
| `0x04`      | `PROTO_ERR_BUSY`            | 模块忙    | 硬件正在执行其他操作（如 CH378 读写中、蓝牙扫描中） |
| `0x05`      | `PROTO_ERR_HW_FAULT`        | 硬件故障   | TF 卡未插入、存储介质损坏、外设初始化失败        |
| `0x06`      | `PROTO_ERR_TIMEOUT`         | 超时     | 操作未在预期时间内完成（如蓝牙连接超时）          |
| `0x80~0xFF` | —                           | 用户自定义  | 各模块/应用专用错误码，由各模块协议文档细化        |

> **注意**：`0x00` 仅作为占位/初始化值使用，NACK 帧不应发送 `0x00`。`0x07~0x7F` 预留为未来标准扩展。

#### 4.2 CMD_GET_TYPE 响应数据格式

Module 收到 `CMD_GET_TYPE` 后，应以 `CMD_ACK` 回复，DATA 字段格式如下：

| 字节         | 字段         | 说明                     |
| ---------- | ---------- | ---------------------- |
| DATA[0]    | 模块类型编号     | 标识模块大类，详见下表。           |
| DATA[1]    | 模块子类型编号    | 标识该大类下的具体实现/变种。        |
| DATA[2]    | 硬件版本号      | 模块硬件版本（可选，无则填 `0x00`）。 |
| DATA[3]    | 固件版本号（主版本） | 固件主版本号（可选，无则填 `0x00`）。 |
| DATA[4]    | 固件版本号（次版本） | 固件次版本号（可选，无则填 `0x00`）。 |
| DATA[5..N] | 扩展信息       | 模块特定能力位或附加信息（可选）。      |

> **模块身份模型**：一个模块的**完整身份**由 **模块类型（DATA[0]）+ 模块子类型（DATA[1]）** 共同决定。模块类型决定物理大类与协议族，子类型决定该大类下的具体硬件实现。Core 侧在识别模块时，必须同时读取这两个字段才能确定模块能力。

**模块类型编号定义（大类）**

| 类型编号        | 模块种类            | 子类型定义位置                                  |
| ----------- | --------------- | ---------------------------------------- |
| `0x00`      | 保留              | —                                        |
| `0x01`      | Display Module  | `protocol.h`：`MODULE_SUBTYPE_DISPLAY_*`  |
| `0x02`      | Wireless Module | `protocol.h`：`MODULE_SUBTYPE_WIRELESS_*` |
| `0x03`      | Power Module    | `protocol.h`：`MODULE_SUBTYPE_POWER_*`    |
| `0x04`      | Keyboard Module | `protocol.h`：`MODULE_SUBTYPE_KEYBOARD_*` |
| `0x05`      | Submodel        | `protocol.h`：`MODULE_SUBTYPE_SUBMODEL_*` |
| `0x06~0xFF` | 预留              | 未来扩展。                                    |

**子类型编号全局规则**

所有大类的子类型遵循统一编号规则，以保证 Core 侧可以用一致的逻辑处理：

| 子类型编号       | 含义      | 说明                                                                            |
| ----------- | ------- | ----------------------------------------------------------------------------- |
| `0x00`      | 保留      | 系统保留，不得使用。                                                                    |
| `0x01`      | 标准/自身实现 | 该大类的默认或唯一实现。例如 Power、Wireless 仅有 `0x01` 一种子类型；Display 的 `0x01` 表示 LCD（当前主方案）。 |
| `0x02~0xFF` | 扩展实现    | 该大类下的其他具体硬件变种，由各模块协议文档细化。                                                     |

### 5. 模块专用操作码

各模块专属操作码及详细数据格式已拆分至独立文档，便于查阅和维护：

| 模块                | 文档                                             |
| ----------------- | ---------------------------------------------- |
| Display（屏幕模块）     | [Protocol_Display.md](Protocol_Display.md)     |
| Wireless（无线/蓝牙模块） | [Protocol_Wireless.md](Protocol_Wireless.md)   |
| Keyboard（键盘模块）    | [Protocol_Keyboard.md](Protocol_Keyboard.md)   |
| Power（供电模块）       | [Protocol_Power.md](Protocol_Power.md)         |
| Submodels（配件模块）   | [Protocol_Submodels.md](Protocol_Submodels.md) |

> 模块专用操作码与模块 ID 高 4 位的映射关系如下：
> 
> - Display（`0x10`）→ `0x10` ~ `0x1F`
> - Keyboard（`0x20`）→ `0x20` ~ `0x2F`
> - Power（`0x30`）→ `0x30` ~ `0x3F`
> - Submodel（`0x40`）→ `0x40` ~ `0x4F`
> - Wireless（`0x01`）→ `0x50` ~ `0x5F`（特例，不参与模块 ID 映射）
> 
> 若操作码空间不足，可通过扩展操作码（基础码+DATA 子命令）或由各模块在 DATA 中设置子命令以支持更多功能。

### 6. 异步通信规则

1. **不阻塞等待**：发送方发出帧后立即返回，不等待接收方回复。
   所有模块和核心均通过 UART 中断接收字节，在主循环或中断中解析处理。
2. **响应帧格式**：接收方回复时，交换 `SRC` 与 `DST` 字段，`CMD` 可保持原值或使用
   `CMD_ACK`/`CMD_NACK` 包裹。例如：
   - 请求：`[AA][00][30][01][01][A5][5A][FC][FD]`（Core 查询 Power 类型）
   - 响应：`[AA][30][00][02][04][03][A5][5A][FC][FD]`（Power 回复 ACK，类型为 0x03）
3. **事件上报**：模块检测到状态变化时，主动以自身为 SRC、Core 为 DST 发送
   `CMD_EVT_NOTIFY` 或模块专用事件码。
4. **无重传机制**：本协议不实现软件重传，依赖物理层可靠性。
   如需可靠性保障，由应用层在必要时使用 `CMD_ACK` 确认。

### 7. 接收状态机

各模块 UART 中断中逐字节喂入状态机：

```
WAIT_HEAD -> WAIT_SRC -> WAIT_DST -> WAIT_LEN -> WAIT_CMD -> WAIT_DATA -> WAIT_TAIL0 -> WAIT_TAIL1 -> WAIT_TAIL2 -> WAIT_TAIL3 -> FRAME_READY
```

- `WAIT_HEAD`：等待 `0xAA`，收到后进入 `WAIT_SRC`。
- `WAIT_SRC` / `WAIT_DST`：接收模块 ID，不校验合法性（由应用层过滤）。
- `WAIT_LEN`：接收 `LEN`，计算 `DATA` 字节数 = `LEN - 1`。
- `WAIT_CMD`：接收操作码。
- `WAIT_DATA`：接收剩余 `LEN - 1` 字节数据。
- `WAIT_TAIL0~3`：依次校验帧尾 `A5 5A FC FD`，任一字节不匹配则丢弃整帧。
- `FRAME_READY`：帧完整，推入消息队列或直接回调处理函数，状态机重置为 `WAIT_HEAD`。

### 8. 消息缓冲区建议

| 模块        | 建议接收缓冲区大小    | 说明            |
| --------- | ------------ | ------------- |
| Display   | 512 bytes    | 可能涉及图像/字体数据传输 |
| Keyboard  | 64 bytes     | 键码数据量小        |
| Power     | 64 bytes     | 状态数据量小        |
| Submodels | 256 bytes x3 | 每路 UART 独立缓冲  |
| CH585F    | 512 bytes    | 蓝牙数据包可能较大     |

### 9. 通信示例

#### 示例 1：Core 查询 Display 类型

```
Host:  [AA][00][10][01][01][A5][5A][FC][FD]
       HEAD=AA SRC=00(Core) DST=10(Display) LEN=01 CMD=01(GET_TYPE)

Guest: [AA][10][00][02][04][01][A5][5A][FC][FD]
       HEAD=AA SRC=10(Display) DST=00(Core) LEN=02 CMD=04(ACK) DATA[0]=01(类型编号)
```

#### 示例 2：Power 主动上报低电量

```
Guest -> Host: [AA][30][00][02][06][15][A5][5A][FC][FD]
               HEAD=AA SRC=30(Power) DST=00(Core) LEN=02 CMD=06(EVT_NOTIFY) DATA[0]=21(电量21%)
```

#### 示例 3：Core 设置 Display 背光为 80%

```
Host:  [AA][00][10][02][11][50][A5][5A][FC][FD]
       HEAD=AA SRC=00(Core) DST=10(Display) LEN=02 CMD=11(SET_BRIGHTNESS) DATA[0]=0x50(80)

Guest: [AA][10][00][01][04][A5][5A][FC][FD]
       HEAD=AA SRC=10(Display) DST=00(Core) LEN=01 CMD=04(ACK)
```

### 10. 双核通信边界

- **V3F 侧通信**：Keyboard、Power、Submodels 的 UART 中断与协议解析运行在 V3F 上。
  V3F 不直接访问 UART4；如需更新 Display，通过共享内存 / HSEM 通知 V5F 代发。
- **V5F 侧通信**：Display、CH585F、CH378、CS43131 的交互运行在 V5F 上。
- **跨核数据**：V3F 与 V5F 之间不通过 UART 传递数据，如需同步状态使用
  `hardware.c` 中的 `volatile` 共享标志位或 HSEM 机制。
- **Display 访问**：UART4 由 V5F 独占，V3F 通过跨核消息让 V5F 代发，
  Display 模块无需处理并发冲突。

#### 10.1 共享内存机制

CH32H417 的 V3F 和 V5F 拥有各自独立的 RAM 区域（V3F: SRAM `0x20110000`，V5F: DTCM `0x200C0000`），
默认无法直接共享数据。为实现核间通信，在 SRAM 高地址区域 `0x20178000` 分配了 32KB 的 `RAM_SHARED` 区域，
两个核心的链接脚本均将 `.shared_data` 段映射到此地址，实现物理内存共享。

核心全局变量 `hardware_g` 使用 `__attribute__((section(".shared_data")))` 放入共享内存，
两个核心编译后该变量均位于 `0x20178000`，访问同一块物理内存。

#### 10.2 硬件信号量（HSEM）保护

对共享数据的并发访问使用 CH32H417 内置的硬件信号量进行互斥保护：

- **HSEM_ID0**：双核启动同步（V3F 唤醒 V5F 时使用）
- **HSEM_ID1**：`key_queue` 按键事件队列互斥保护

访问共享数据时，先 `HSEM_FastTake()` 获取信号量，操作完成后 `HSEM_ReleaseOneSem()` 释放。
获取失败表示另一核心正在访问，当前操作应放弃或重试。

#### 10.3 核间数据流

- **按键事件（V3F → V5F）**：V3F 扫描 GPIO 按键（PF8/PF9/PF10），检测到事件后
  通过 `Hardware_KeyQueue_Push()` 推入共享环形队列（HSEM_ID1 保护），
  V5F 通过 `Hardware_KeyQueue_Pop()` 取出事件并处理（音量调节、Enter 信号等）。
- **心跳状态**：`hardware_g.hb_slots` 由两个核心各自管理不同模块的心跳，
  V5F 负责 Display，V3F 负责 Keyboard/Power/Submodels，字段无并发冲突。
- **配置请求（V3F → V5F）**：`hardware_g.config_req` 用于 V3F 向 V5F 提交跨核配置请求。

### 11. 版本与扩展

- 本协议版本号为 **V1.0**，帧结构中暂不包含版本字段。
- 后续如需扩展，可在 `DATA` 首字节增加子版本号，或保留 `CMD` 的 `0x08~0x0F` 作为系统扩展码。
- 新增模块时，从预留 ID 范围中分配，并在独立协议文档中补充操作码定义。
