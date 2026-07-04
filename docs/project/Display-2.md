# Display-2 模块（墨水屏）

## 概述

本模块为低功耗电子墨水屏显示方案，适用于长续航阅读场景，与 Display-1 共用同一套 MiniUI 框架与通信协议。

- **主控芯片**：CH32V307RCT6
- **屏幕规格**：ZJY648480-0583AAAMFGN 5.83寸黑白墨水屏，648×480像素，138DPI
- **驱动IC**：JD79686AB（片上TCON、SRAM、LUT、DC-DC）
- **驱动方式**：4线SPI接口（BS1=L）
- **与核心模块接口**：UART1（PA9-TX，PA10-RX），**921600/8-N-1**
- **DEBUG接口**：UART2（PA2-TX，PA3-RX），**921600/8-N-1**
- **模块 ID**：`0x10`（与 Display-1 共用显示模块 ID，通过子类型区分）
- **模块类型编号**：`0x01`
- **子类型编号**：`0x02`（E-ink）

## 硬件引脚分配

### SPI1（墨水屏数据接口）

| 功能 | 引脚 | 说明 |
|------|------|------|
| CS# | PA4 | 片选（GPIO 软件控制，低有效） |
| SCL | PA5 | SPI 时钟（AF_PP） |
| MISO | PA6 | SPI 输入（浮空，读操作使用） |
| SDA | PA7 | SPI 输出（MOSI，AF_PP） |

### 控制引脚（GPIO）

| 功能 | 引脚 | 方向 | 说明 |
|------|------|------|------|
| RES# | PB3 | 输出 | 硬件复位，低有效 |
| D/C# | PB4 | 输出 | 数据/命令选择，低=命令，高=数据 |
| BUSY_N | PB5 | 输入（上拉） | 忙指示，低=忙碌，高=就绪 |

### UART1（Core 通信）

| 功能 | 引脚 | 说明 |
|------|------|------|
| TX | PA9 | 发送（连接 Core UART4 RX） |
| RX | PA10 | 接收（连接 Core UART4 TX） |

波特率 921600，8-N-1，协议遵循 `Protocol_Display.md`。

### UART2（DEBUG）

| 功能 | 引脚 | 说明 |
|------|------|------|
| TX | PA2 | 调试串口发送 |
| RX | PA3 | 调试串口接收 |

波特率 115200，用于 printf 调试输出。

## 墨水屏规格（ZJY648480-0583AAAMFGN）

| 参数 | 值 |
|------|-----|
| 尺寸 | 5.83 英寸 |
| 分辨率 | 648(H) × 480(V) 像素 |
| 有效显示区域 | 118.78 × 88.22 mm |
| 像素间距 | 0.1833 × 0.1833 mm |
| 显示颜色 | 黑白双色（1bpp） |
| 对比度 | ≥8:1 |
| 白色反射率 | 30% |
| 全刷时间 | ~4秒（25°C） |
| 工作温度 | 0°C ~ +50°C |
| 驱动IC | JD79686AB |
| SPI接口 | 4线SPI（CPOL=0, CPHA=0, MSB first） |
| SPI最大时钟 | 10MHz（写），4.3MHz（读） |

## 软件架构

### 驱动层次

```
┌─────────────────────────────────────────┐
│  main.c（应用层 / Demo）                 │
├─────────────────────────────────────────┤
│  epaper.c / epaper.h（E-paper 驱动层）  │
│  - 初始化、LUT 配置                      │
│  - 全屏刷新（DTM1 + DTM2 + PON + DRF）  │
│  - 局部刷新（PDTM1 + PDTM2 + PDRF）    │
│  - 深度睡眠 / 唤醒                       │
├─────────────────────────────────────────┤
│  epaper_hw.c / epaper_hw.h（硬件抽象层） │
│  - SPI1 外设初始化与收发                 │
│  - GPIO 控制（CS/DC/RES/BUSY）          │
│  - BUSY 等待与超时处理                   │
└─────────────────────────────────────────┘
```

### 像素格式

- **1bpp**（1 bit per pixel）
- 每字节表示 8 个水平像素，MSB 为最左侧像素
- bit=1 → 白色，bit=0 → 黑色
- 每行 81 字节（648 / 8），全帧 38,880 字节

### 全屏刷新流程

```
DTM1(0x10) 写入 OLD 数据
  → DTM2(0x13) 写入 NEW 数据
  → PON(0x04) 开启电源
  → 等待 BUSY_N 拉高
  → DRF(0x12) 触发刷新
  → 等待 BUSY_N 拉高（约4秒）
  → POF(0x02) 关闭电源
  → 等待 BUSY_N 拉高
```

### 局部刷新流程

使用 JD79686AB 专用局部刷新命令：
- **PDTM1(0x14)**：写入区域 OLD 数据（含位置头 X/Y/W/L）
- **PDTM2(0x15)**：写入区域 NEW 数据（含位置头 X/Y/W/L）
- **PDRF(0x16)**：触发局部刷新（含位置头 + DFV_EN 标志）

约束：
- X 和 W 必须为 8 的倍数
- 局部刷新速度远快于全屏刷新（约 0.3~1 秒）
- 驱动内部维护 4KB old-data 缓冲区（`g_partial_old[]`）

### 初始化序列

基于 JD79686AB User Guide §2.4.1.1 "Before OTP Model" 适配为 BW 模式：

1. Hardware Reset（RES# 拉低 10ms → 拉高 → 等待 BUSY_N）
2. Dummy Code（0x08 = 0x00）
3. TDY 命令序列（F8 + 参数对）
4. 用户命令（PSR/PWR/BTST/OSC/CDI/TRES/VDCS/PWS/TCON/SET_STG）
5. 写入 BW 模式 LUT 表（LUTC/LUTWW/LUTBW/LUTWB/LUTBB，各 42 字节）

LUT 波形数据参考 JD79686AB User Guide §2.5.1 BW Mode Waveform。

## 项目目录结构

```
display_2/
├── Common/
│   └── Common/
│       ├── Eink/                      # 墨水屏驱动
│       │   ├── epaper_hw.h            # 硬件抽象层头文件（SPI/GPIO 定义）
│       │   ├── epaper_hw.c            # SPI1 初始化、GPIO 控制、BUSY 等待
│       │   ├── epaper.h               # E-paper 驱动 API 头文件
│       │   └── epaper.c               # 初始化、全屏/局部刷新、LUT、休眠
│       ├── MiniUI/                    # MiniUI 框架（待移植）
│       ├── Uart/                      # UART1 协议栈（待实现）
│       ├── Font/                      # 字库资源（待添加）
│       ├── hardware.c                 # 全局调度与初始化入口（待实现）
│       └── hardware.h                 # 全局调度头文件（待实现）
├── Core/                              # RISC-V 内核文件
├── Debug/                             # 调试支持（printf over UART2）
├── Peripheral/                        # CH32V30x 外设库
├── Startup/                           # 启动文件
├── User/
│   ├── main.c                         # 主程序入口与 Demo
│   ├── ch32v30x_conf.h               # 外设库配置
│   ├── system_ch32v30x.c             # 系统时钟配置
│   └── system_ch32v30x.h
├── Ld/
│   └── Link.ld                        # 链接脚本（288K FLASH + 32K RAM）
└── Epaper.wvproj                      # MounRiver Studio 工程文件
```

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `epaper_hw.c`、`epaper.h`
- **函数**：每个单词首字母大写加下划线，例如 `Epaper_Init()`、`Epaper_PartialRefresh()`
- **变量**：统一小写加下划线，例如 `g_partial_old`、`frame_buffer`
- **宏**：全大写加下划线，例如 `EPD_WIDTH`、`EPD_FRAME_SIZE`
- **文件夹**：下划线写法，仅首字母大写，例如 `MiniUI`、`Eink`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动

## 构建配置

### MounRiver Studio Include 路径

在工程属性中添加以下 Include 路径：

- `Common/Common/Eink/`
- `Common/Common/`（待添加 MiniUI/UART/Font 等模块后）
- `Debug/`
- `User/`
- `Peripheral/inc/`
- `Core/`

### 链接脚本

当前配置：`FLASH-288K + RAM-32K`（`Ld/Link.ld`）。  
若后续需要更大 RAM（如缓存全帧），可改为 `FLASH-192K + RAM-128K` 或 `FLASH-224K + RAM-96K`。

## 内存约束

- **RAM**：32KB（SRAM `0x20000000`）
- **全帧数据**：648×480/8 = 38,880 字节（超出 RAM）
- **解决方案**：全屏刷新采用流式传输（逐行发送，不缓存全帧）
- **局部刷新缓冲区**：4KB（`g_partial_old[]`），支持最大 ~32K像素区域
- **链接脚本配置**：288K FLASH + 32K RAM

## 当前实现状态

| 功能 | 状态 | 说明 |
|------|------|------|
| SPI1 硬件初始化 | ✅ 已完成 | CPOL=0, CPHA=0, 9MHz |
| GPIO 控制（CS/DC/RES/BUSY） | ✅ 已完成 | 软件控制 CS |
| 硬件复位 | ✅ 已完成 | RES# 低→高 + WaitBusy |
| 初始化寄存器序列 | ✅ 已完成 | TDY + User commands, BW 模式 |
| BW LUT 波形表 | ✅ 已完成 | 参考 JD79686AB §2.5.1 |
| 全屏刷新 | ✅ 已完成 | DTM1 + DTM2 + PON + DRF + POF |
| 清屏（全白） | ✅ 已完成 | Epaper_ClearWhite() |
| 局部刷新 | ✅ 已完成 | PDTM1 + PDTM2 + PDRF |
| 深度睡眠/唤醒 | ✅ 已完成 | DSLP + HW Reset |
| 温度读取 | ✅ 已完成 | 内部温度传感器 |
| Demo 测试图案 | ✅ 已完成 | 边框、棋盘格、局部矩形、条纹 |
| UART1 Core 协议栈 | ❌ 待实现 | 遵循 Protocol_Display.md |
| MiniUI 框架移植 | ❌ 待实现 | 从 Display-1 移植，适配 1bpp |
| 字库渲染 | ❌ 待实现 | 1bpp 字体渲染引擎 |
| 触摸屏驱动 | ⏸ 待定 | 取决于硬件方案选型 |

## 后续开发计划

1. **UART1 协议栈**：实现 `Protocol_Display.md` 帧解析状态机，支持标准帧与流式帧
2. **CLI 直通**：实现 `CMD_DISP_EXT_CLI` 命令处理
3. **MiniUI 适配**：将 Display-1 的 MiniUI 框架移植到 1bpp 黑白显示
4. **帧缓冲管理**：实现双缓冲机制，支持差分更新
5. **低功耗策略**：刷新间隙 MCU 进入低功耗，UART 中断唤醒

## 开发要点

1. **LUT 调优**：当前使用 JD79686AB 参考波形，实际模组可能需要微调电压和帧数以消除残影
2. **温度补偿**：低于 10°C 时需要使用不同的 LUT 波形和更长的 POF 等待时间（5秒）
3. **局部刷新限制**：X/W 必须为 8 的倍数；频繁局部刷新可能导致残影，建议定期全屏刷新
4. **SPI 时钟**：写操作最高 10MHz（SPI 分频 4 = 9MHz），读操作需降至 ~4MHz
5. **与 Display-1 的协议兼容性**：两者共用 `0x10` 模块 ID 和同一套 Display 协议，Core 侧通过 `CMD_GET_TYPE` 的子类型（`0x01` LCD / `0x02` E-ink）区分
6. **内存管理**：32KB RAM 无法缓存全帧，采用流式传输；局部刷新使用 4KB 缓冲区
