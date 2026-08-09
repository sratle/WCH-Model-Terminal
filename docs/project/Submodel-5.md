# Submodel-5 模块（RGB 点阵）

## 概述

本模块为灯光效果扩展方案，通过 WS2812 可寻址 RGB LED 实现多种动态灯效，用于状态指示、氛围营造或音乐可视化。

- **主控芯片**：CH585F
- **与核心模块接口**：UART0，PA14-TX / PA15-RX，**230400/8-N-1**
- **模块 ID**：`0x40` ~ `0x42`（从首帧 DST 字段学习，槽位自适应）
- **模块类型编号**：`0x05`（Submodel）
- **子类型编号**：`0x05`（RGB Lights）

## 硬件组成

- **CH585F**：灯效算法运行、WS2812 SPI 驱动、协议帧解析
- **WS2812(B) 灯阵**：7×7 矩阵（49 颗灯珠），单线归零码协议，每颗 LED 24bit 颜色数据（GRB 顺序），级联连接
- **SPI0 MOSI 驱动**：PB14(MOSI) → WS2812 DIN，PB12(SCK) 提供时钟
- **供电注意**：WS2812 全白时单颗约 60mA，49 颗全白约 3A，需评估模块供电能力

## 软件架构

### 无 BLE/TMOS 超级循环架构

本模块不使用 BLE 协议栈和 TMOS 事件调度器，采用纯超级循环（superloop）架构：

- **固定 100fps 帧节拍**：每帧周期 10ms = 渲染 + WS2812 发送（约 1.5ms，期间关全局中断保证时序）
  + 约 8ms 分块延时；延时被切成 100 个小块，块间穿插 UART 协议解析，保证 230400 下不丢字节
- **速度档位制**：动画速度为档位 1~10（8 档为基准 = 10ms/步，每升一档步进 ×0.8，
  非法档位按 8 档处理），9/10 档由定点累加器实现单帧多步
- **UART 接收**：ISR 仅将字节写入环形缓冲（约 20 cycles），协议解析在主循环分块延时中完成
- **上电不亮灯**：收到第一帧 `CMD_SUB_SET_MODE` 之前不刷新 WS2812，避免上电默认色闪烁

### WS2812 SPI 驱动原理

使用 SPI0 Master 的 MOSI 引脚模拟 WS2812 时序：

- **SPI 时钟**：60MHz / 24 = 2.5MHz
- **编码方式**：每 1 bit WS2812 数据用 3 bit SPI 数据编码
  - WS2812 bit `1` → SPI `110`（0.8µs HIGH + 0.4µs LOW）
  - WS2812 bit `0` → SPI `100`（0.4µs HIGH + 0.8µs LOW）
- **每颗 LED**：24 bit GRB → 72 SPI bits → 9 SPI bytes
- **一帧数据**：49 LED × 9 bytes + 16 bytes reset = 457 bytes
- **传输时间**：~1.5ms / 帧（2.5MHz SPI）
- **Reset 信号**：帧末尾附加 16 字节全零（~51µs LOW > 50µs reset 要求）

### HSV 色彩空间

内部使用 HSV（Hue-Saturation-Value）作为色彩中间态：

- **H（色相）**：0-359°，颜色环
- **S（饱和度）**：0-255，0=灰，255=纯色
- **V（明度）**：0-255，0=黑，255=最亮

**色彩流程**：RGB888 输入 → 转 HSV → 强制 S=255（最大饱和度）→ 调整 V（亮度控制）→ 转回 RGB888 → 编码为 GRB → SPI 发送

### 四种灯效模式

| 模式 | 编号 | 名称 | 行为 |
|------|------|------|------|
| 自定义 | `0x00` | Custom | 从 Core 接收逐帧 RGB888 颜色数据，最多 20 帧循环播放 |
| 常亮 | `0x01` | Solid | 全部 49 颗 LED 以指定 RGB 颜色常亮 |
| 呼吸 | `0x02` | Breathing | 全部 LED 以指定颜色呼吸渐变，speed 控制周期 |
| 跑马灯 | `0x03` | Marquee | 单灯带拖尾依次移动，speed 控制移动速率 |

### 一次性动画事件（游戏联动）

除模式外还支持两个**一次性动画事件**（播放一次后自动回到触发前的模式，
颜色与基准亮度沿用模式 1 常亮配置）：

| 子命令 | 名称 | 行为 |
|--------|------|------|
| `SET_MODE SUB=0x04` | 中心波纹 Ripple | 亮度波纹从 7×7 矩阵中心扩散到边缘一次（尾迹 4 步线性衰减） |
| `SET_MODE SUB=0x05` | 边缘波浪 Wave | 亮度波浪从一侧传播到另一侧一次（四方向可选，尾迹 3 步） |

**联动链路**：Display-1 本地游戏（俄罗斯方块/2048/贪吃蛇方向操作 → wave，
飞机大战击中/扫雷挖格 → ripple）经 Display 协议 `CMD_DISP_EXT_RGB_EFFECT`（0x1E）
请求 Core 代发，Core 查找 RGB 槽位后转发为上述子命令（发送即忘）。

#### 自定义帧动画

- Core 通过 `CMD_SUB_SET_MODE SUB=0x02` 逐帧传输 RGB888 颜色数据（每帧 147 bytes，每 3 bytes 对应一颗 LED 的 R/G/B 颜色）
- 亮度由模式设置命令（`SUB=0x01`）中的 `brightness` 参数全局控制
- 颜色由帧数据中的 RGB888 决定，渲染时 HSV 饱和度强制为最大值（255），仅通过 V 分量缩放亮度
- 通过 `CMD_SUB_SET_MODE SUB=0x03` 启动播放，指定帧数和帧间隔
- 帧间隔范围：50ms ~ 1000ms

## 通信协议

### UART 参数

| 参数 | 值 |
|------|-----|
| 引脚 | PA14(TX), PA15(RX) |
| 波特率 | 230400 |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |

### 支持的命令

| CMD | 子命令 | 方向 | 说明 |
|-----|--------|------|------|
| `0x01` GET_TYPE | — | Core→RGB | 查询模块类型，回复 ACK + 类型信息 |
| `0x02` GET_STATUS | — | Core→RGB | 查询当前状态（ACK `[模式][R][G][B][亮度][速度]`，6 字节） |
| `0x41` SET_MODE | `0x01` | Core→RGB | 设置灯效模式 + 颜色 + 亮度 + 速度 |
| `0x41` SET_MODE | `0x02` | Core→RGB | 传输自定义帧数据 |
| `0x41` SET_MODE | `0x03` | Core→RGB | 启动自定义帧动画播放 |
| `0x41` SET_MODE | `0x04` | Core→RGB | 中心波纹一次性动画 `[速度:1]` |
| `0x41` SET_MODE | `0x05` | Core→RGB | 边缘波浪一次性动画 `[方向:1][速度:1]` |
| `0x44` SET_CONFIG | — | Core→RGB | 快捷设置常亮色：`[R][G][B]`（亮度 0xFF） |

> **速度字段**：档位制 1~10（8 档 = 10ms/步基准），详见 `Protocol_Submodels.md` 速度档位定义。
>
> **已知偏差（待修固件）**：按协议规范（`Protocol_Submodels.md` §4.5），状态查询应响应
> `CMD_SUB_GET_STATUS(0x42) SUB=0x00`（与其他 Submodel 一致，Core 侧
> `Submodels_RGB_QueryStatus()` 亦按此发送）；但当前固件实际响应的是通用
> `CMD_GET_STATUS(0x02)`，对 0x42 会回复 NACK。协议以 0x42 SUB=0x00 为准，
> 固件侧待修正。

#### SET_MODE 0x01 设置灯效

```
DATA: [SUB:1][mode:1][R:1][G:1][B:1][brightness:1][speed:1]
- mode: 0x00~0x03（自定义/常亮/呼吸/跑马灯）
- R/G/B: 基础颜色 RGB888
- brightness: 全局亮度 0-255
- speed: 速度档位 1~10（非法值按 8 档处理）
```

#### SET_MODE 0x02 传输自定义帧

```
DATA: [SUB:1][frame_idx:1][R0:1][G0:1][B0:1]...[R48:1][G48:1][B48:1]
- frame_idx: 帧序号 0~19
- R_N/G_N/B_N: 第 N 颗 LED 的 RGB888 颜色值（各 1 字节）
- 每帧 DATA 共 149 字节（1+1+147，其中 147=49×3）
```

#### SET_MODE 0x03 播放自定义动画

```
DATA: [SUB:1][frame_count:1][frame_interval:2(uint16大端)]
- frame_count: 总帧数 1~20
- frame_interval: 帧间隔 50~1000 ms
```

## 项目目录结构

```
submodel-5/
├── APP/                            # 应用代码
│   ├── include/                    # 头文件
│   │   ├── protocol.h              # 协议定义（帧结构、操作码、状态机）
│   │   ├── ws2812.h                # WS2812 SPI 驱动接口
│   │   ├── color.h                 # HSV/RGB888 色彩转换
│   │   ├── effect.h                # 灯效引擎（4种模式 + 自定义帧）
│   │   └── rgb_app.h               # 应用层（UART、SysTick、命令处理）
│   ├── protocol.c                  # 协议帧解析与打包
│   ├── ws2812.c                    # WS2812 驱动（SPI0 MOSI 编码 + 发送）
│   ├── color.c                     # HSV↔RGB888 整数运算转换
│   ├── effect.c                    # 灯效模式渲染（Solid/Breathing/Marquee/Custom）
│   └── rgb_app.c                   # UART0 通信 + 命令分发 + SysTick
│   └── rgb_main.c                  # 主入口（超级循环）
├── HAL/
│   └── include/
│       └── CONFIG.h                # 系统配置（芯片 ID、时钟、调试）
├── StdPeriphDriver/                # CH585 标准外设库（GPIO/UART/SPI/SysTick 等）
├── RVMSIS/                         # RISC-V CMSIS 内核头文件
├── Startup/                        # 启动汇编文件
├── Ld/                             # 链接脚本
└── rgb.wvproj                      # MounRiver Studio 工程文件
```

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `rgb_app.c`、`ws2812.h`
- **函数**：每个单词首字母大写加下划线，例如 `Effect_SetMode()`、`WS2812_Refresh()`
- **变量**：统一小写加下划线，例如 `led_count`、`brightness`
- **宏定义**：全大写加下划线，例如 `WS2812_LED_COUNT`、`PROTO_HEAD`
- **类型**：小写加下划线加 `_t` 后缀，例如 `proto_frame_t`、`rgb888_t`

## 引脚分配

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA14 | UART0 TX | 连接 Core UART6 RX |
| PA15 | UART0 RX | 连接 Core UART6 TX |
| PB12 | SPI0 SCK | WS2812 时钟（通过 SPI 编码产生） |
| PB14 | SPI0 MOSI | WS2812 DIN 数据线 |
| PB15 | SPI0 NSS | 固定拉低（始终选中） |

## 开发要点

1. **WS2812 时序**：SPI 2.5MHz 时钟下，3 SPI bit 精确编码 1 WS2812 bit（1.2µs），满足 WS2812 的 0/1 码时序要求。使用 nibble 查表法加速编码（16 项 LUT，每项 12 bit SPI 数据）。
2. **SPI FIFO 连续传输**：利用 SPI0 的 8 字节 FIFO 缓冲，在主循环中持续填充以保持 MOSI 连续输出，避免 FIFO 欠载导致 WS2812 误码。
3. **功耗与供电**：49 颗 WS2812 全白时约 3A，模块接口供电能力需提前评估；必要时灯阵独立供电，仅共地。
4. **亮度控制**：通过 HSV 色彩空间的 V 分量实现平滑亮度调节，饱和度强制为最大值以保证颜色鲜艳度，避免直接缩放 RGB 值导致的色彩偏移。
5. **动画平滑度**：渲染帧周期固定 10ms（100fps）；Breathing 模式使用二次函数近似正弦曲线，
   Marquee 模式带 5 级亮度拖尾，速度档位（1~10）映射为 6.4~47.7ms 的步进时间。
6. **无 BLE 干扰**：不使用 BLE 协议栈和 TMOS，避免了射频中断对 SPI/UART 时序的影响，保证 WS2812 传输稳定性。
   注意 CH585 的 RF 天线开关（`RB_RF_ANT_SW_EN`）与 UART0 重映射引脚 PA14/PA15 冲突，初始化时必须关闭。
