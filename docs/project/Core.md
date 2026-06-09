# 项目结构

## 概述

这是一个基于 **CH32H417** 的双核 RISC-V 微控制器固件工程，属于一款模块化终端设备的 **Core Model（核心板固件）**。

- **MCU**: CH32H417QEU6（青稞 RISC-V 双核：V3F + V5F）
- **IDE**: MounRiver Studio (MRS) / WCH Studio（基于 Eclipse CDT）
- **工具链**: RISC-V Embedded GCC（`riscv-none-elf-gcc`）
- **语言**: C（GNU99 标准），无 RTOS（NoneOS）
- **调试器**: WCH-Link，使用 OpenOCD + GDB，支持双核同步调试

### 双核分工

| 核心 | 主频 | 职责 |
|------|------|------|
| **V5F** | 400 MHz | 唯一执行核心，负责所有模块初始化与业务逻辑：CS43131（音频 DAC）、CH378（文件管理）、CH585F（蓝牙/无线，SPI 通信）、Display（屏幕模块，UART4）、Keyboard（UART3）、Power（UART5）、Submodels（UART6~8）、CH9350（HID 转串口，UART1）、Key（核心按键）、Config、CLI |
| **V3F** | 100 MHz | 辅助核心，仅负责上电初始化时钟、唤醒 V5F，然后进入 STOP 低功耗模式。不执行任何业务逻辑 |

> **架构说明**：V3F 和 V5F 共享同一 MCU 的外设总线。当前架构下，V3F 仅作为启动核心存在，所有模块驱动和中断处理均由 V5F 统一管理，避免了跨核数据同步的复杂性。

---

## V3F 启动流程

1. V3F 上电，执行 `SystemInit()` 配置 PLL 和系统时钟。
2. V3F 通过 `NVIC_WakeUp_V5F()` 唤醒 V5F。
3. V3F 进入 `PWR_EnterSTOPMode` 低功耗等待。
4. V5F 唤醒后执行全部初始化逻辑和主循环。

---

## V5F 初始化与主循环

### 初始化顺序

`Hardware_Init()` 统一初始化所有模块：

1. `CS43131_init()` — 音频 DAC
2. `CH378_Init()` — 文件管理芯片
3. `Config_Init()` — 配置系统
4. `CLI_Init()` — 命令行接口
5. `CH585F_Init()` — 蓝牙无线芯片
6. `Display_Init()` — 屏幕模块
7. `Key_Init()` — 核心按键
8. `Power_Init()` — 供电模块
9. `CH9350_Init()` — HID 转串口
10. `Keyboard_Init()` — 键盘模块
11. `Submodels_Init()` — 子模块
12. 心跳槽位初始化

### 主循环

```c
while(1) {
    Display_Process(&display_g);
    Audio_Process();
    Audio_SyncToHardware();
    Debug_CLI_Process();
    CH585F_BT_Poll();
    Key_PollAndProcess();
    Keyboard_Process(&keyboard_g);
    Power_Process(&power_g);
    Submodels_Process(&submodels_g[0]);
    Submodels_Process(&submodels_g[1]);
    Submodels_Process(&submodels_g[2]);
    CH9350_Process(&ch9350_g);
    Hardware_Heartbeat();
    Config_Process();
    Delay_Ms(1);
}
```

---

## 全局变量

### hardware_g

`hardware_t hardware_g` 是系统全局状态变量，定义在 `hardware.c` 中，由 V5F 独占管理。

包含：
- `hardware_init_flag` — 模块初始化完成标志
- `hb_slots[]` — 心跳槽位（模块在线检测）
- `hb_tick` — 心跳计时
- `config_applied` — 配置已应用标志
- `rgb_config` — RGB 配置
- `rgb_frame` — RGB 自定义帧传输
- `subdisp_req` — SubDisplay 命令

### 模块全局变量

所有模块全局变量定义在 `hardware.c` 中：

```c
cs43131_t CS43131_g;          // 音频 DAC
ch378_t ch378_g;              // 文件管理芯片
ch585f_t ch585f_g;            // 蓝牙无线芯片
display_t display_g;           // 屏幕模块
power_t power_g;              // 供电模块
keyboard_t keyboard_g;         // 键盘模块
ch9350_t ch9350_g;            // HID 转串口
submodels_t submodels_g[3];   // 子模块
```

---

## 中断处理

所有 UART 中断均由 V5F 处理，定义在 `V5F/User/ch32h417_it.c` 中：

| 中断 | UART | 模块 |
|------|------|------|
| USART1_IRQHandler | UART1 | CH9350 HID |
| USART2_IRQHandler | UART2 | Debug/CLI |
| USART3_IRQHandler | UART3 | Keyboard |
| USART4_IRQHandler | UART4 | Display |
| USART5_IRQHandler | UART5 | Power |
| USART6_IRQHandler | UART6 | Submodel-1 |
| USART7_IRQHandler | UART7 | Submodel-2 |
| USART8_IRQHandler | UART8 | Submodel-3 |

V3F 的 `ch32h417_it.c` 仅保留 NMI 和 HardFault 处理。

---

## 内存布局

| 区域 | 起始地址 | 大小 | 归属 | 用途 |
|------|---------|------|------|------|
| FLASH | 0x00010000 | 512K | V5F 代码 | V5F 固件（加载到 ITCM 运行） |
| ITCM | 0x200A0000 | 128K | V5F 代码 | V5F 指令运行区 |
| DTCM+SRAM | 0x200C0000 | ~511K | V5F 数据 | V5F .data .bss 栈堆（含音频引擎缓冲区） |
| FLASH | 0x00000000 | 64K | V3F 代码 | V3F 固件（最小化） |
| RAM_CODE | 0x20140000 | 16K | V3F 代码 | V3F 指令运行区 |
| RAM | 0x20144000 | ~16K | V3F 数据 | V3F .data .bss（仅启动代码） |

> **资源分配策略**：V5F 是唯一的业务执行核心，需要大量 RAM 用于音频缓冲区（~55KB）、协议缓冲、文件 I/O 等，因此获得绝大部分 FLASH 和 RAM 资源。V3F 仅执行上电启动和唤醒 V5F 的代码，分配最小资源。

---

## 项目目录结构

```
core/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块
│   │   ├── CH378/                  # CH378 文件管理芯片驱动
│   │   ├── CH585F/                 # CH585F 蓝牙无线芯片交互
│   │   ├── CH9350/                 # CH9350 HID 转串口驱动
│   │   ├── CJSON/                  # cJSON JSON 解析库
│   │   ├── CLI/                    # CLI 命令行接口（含 play/pause/stop 等音频命令）
│   │   ├── Config/                 # 配置系统（JSON 持久化）
│   │   ├── CS43131/                # CS43131 音频 DAC + 多通道混音引擎 + 效果器
│   │   ├── Display/                # 屏幕模块驱动
│   │   ├── I2c_soft/               # 软件 I2C 基础驱动
│   │   ├── Key/                    # 按键基础驱动
│   │   ├── Keyboard/               # 键盘模块驱动
│   │   ├── Power/                  # 供电管理模块
│   │   ├── Protocol/               # 统一通信协议栈
│   │   ├── Submodels/              # 子模块/配件管理
│   │   ├── Test/                   # 测试代码
│   │   ├── all_devices.h           # 全设备头文件聚合
│   │   ├── hardware.c              # 全局调度与初始化入口
│   │   └── hardware.h              # 全局调度头文件
│   ├── Core/                       # RISC-V 内核相关文件
│   ├── Debug/                      # 调试支持文件
│   ├── Peripheral/                 # 芯片外设库
│   └── Startup/                    # 启动文件
│
├── V3F/                            # V3F 核心工程（仅启动 V5F）
│   ├── Ld/                         # V3F 链接脚本
│   ├── User/                       # V3F 用户代码
│   │   ├── main.c                  # V3F 主函数（唤醒 V5F 后 STOP）
│   │   ├── ch32h417_it.c/.h        # V3F 中断（仅 NMI/HardFault）
│   │   ├── system_ch32h417.c/.h    # 系统时钟配置
│   │   └── ch32h417_conf.h         # 外设库头文件包含配置
│   └── ...
│
├── V5F/                            # V5F 核心工程（主执行核心）
│   ├── Ld/                         # V5F 链接脚本
│   ├── User/                       # V5F 用户代码
│   │   ├── main.c                  # V5F 主函数（所有模块初始化与主循环）
│   │   ├── ch32h417_it.c/.h        # V5F 中断（全部 UART 中断）
│   │   ├── system_ch32h417.c/.h    # 系统时钟更新
│   │   └── ch32h417_conf.h         # 外设库头文件包含配置
│   └── ...
│
└── docs/                           # 项目文档与数据手册
```

---

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`i2c_soft.h`。
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`。
- **变量**：统一小写加下划线，例如 `system_init_flag`。
- **文件夹**：下划线写法，仅首字母大写，例如 `I2c_soft`、`Submodels`。
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范。

---

## 音频引擎（CS43131 + 多通道混音 + 效果器）

### 架构概览

音频引擎基于 CS43131 DAC（44.1kHz / 16bit / 立体声 I2S）构建，支持 **2 路虚拟通道**同时播放，内置 **3 段 EQ、压缩器、Echo** 效果器链。

```
TF卡/USB
   │
   │ CH378 SPI1（时分复用，每通道 4KB 突发读取）
   ▼
┌──────────────────┐
│  Channel 0 (16KB RB)  ─┐
│  Channel 1 (16KB RB)  ─┼─→ Mixer ─→ EQ → Compressor → Echo ─→ DMA Buffer ─→ I2S → CS43131 → Speaker
└──────────────────┘
```

- **CH378 时分复用**：`Audio_Process()` 在主循环中轮转为每个活跃通道执行 open → seek → read 4KB → close，读完即释放 CH378 文件句柄，其他模块（CLI、Config 等）可自由使用 CH378
- **16KB Ring Buffer**：每通道独立 16KB 环形缓冲，44.1kHz 立体声 16bit（176KB/s）下提供约 93ms 缓冲，足以覆盖 CH378 被其他模块短暂占用的情况
- **DMA 双缓冲**：DMA1_Channel1 以 Circular 模式传输 `dma_buf[2048]`，半传输/传输完成中断中各填充 1024 样本

### 内存消耗

| 组件 | 大小 |
|------|------|
| 2 × 通道 Ring Buffer | 32 KB |
| Echo 延迟线（100ms 立体声） | 17.6 KB |
| DMA 输出缓冲 | 4 KB |
| 效果器状态 + 系数 | ~1 KB |
| **总计** | **~55 KB**（占 511KB RAM 的 11%） |

### CLI 命令

#### `play <file> [channel]` — 播放 WAV 文件

| 用法 | 行为 |
|------|------|
| `play song.wav` | 停止**所有**通道，在 ch0 播放（默认单通道模式） |
| `play song.wav 0` | 仅停止 ch0，在 ch0 播放（不影响 ch1） |
| `play bgm.wav 1` | 仅停止 ch1，在 ch1 播放（不影响 ch0） |

#### `pause [channel]` / `resume [channel]` — 暂停/恢复

| 用法 | 行为 |
|------|------|
| `pause` | 暂停所有通道 |
| `pause 0` | 仅暂停 ch0 |
| `resume` | 恢复所有通道 |
| `resume 1` | 仅恢复 ch1 |

#### `stop [channel]` — 停止播放（不可恢复）

| 用法 | 行为 |
|------|------|
| `stop` | 停止所有通道 |
| `stop 1` | 仅停止 ch1 |

#### 典型双通道用法

```bash
play music.wav         # ch0: 主音乐（自动 stop all）
play bgm.wav 1         # ch1: 背景音乐（ch0 不受影响）
pause 0                # 暂停主音乐，背景音乐继续
stop 1                 # 停掉背景音乐
resume 0               # 恢复主音乐
```

### 效果器

效果器配置嵌入 `cs43131_t` 全局结构体，通过 `CS43131_g` 可在任何模块中修改。效果器链在 DMA 中断中执行，处理顺序为：**EQ → Compressor → Echo**。

#### 3 段 EQ（Biquad IIR 滤波器）

- **频段**：Bass 100Hz / Mid 1kHz / Treble 10kHz
- **增益范围**：-12 ~ +12 dB
- **实现**：Q13 定点 Biquad 滤波器，预计算系数，按增益线性插值

```c
// 代码中使用
Audio_EQ_Enable(1);           // 启用 EQ
Audio_EQ_SetBass(6);          // Bass +6dB
Audio_EQ_SetMid(-3);          // Mid -3dB
Audio_EQ_SetTreble(4);        // Treble +4dB

// 或直接修改结构体
CS43131_g.eq.enable = 1;
CS43131_g.eq.bass_gain_db = 6;
CS43131_g.eq.mid_gain_db = -3;
CS43131_g.eq.treble_gain_db = 4;
Audio_EQ_UpdateCoeffs();      // 修改后需调用以重新计算系数
```

#### 压缩器（Compressor）

- **阈值**：-40 ~ 0 dB
- **比率**：1:1 ~ 10:1
- **补偿增益**：0 ~ 20 dB
- **实现**：包络跟随（快攻击 / 慢释放）+ dB 域增益衰减

```c
Audio_Comp_Enable(1);
Audio_Comp_SetParams(-12, 4, 6);  // 阈值 -12dB, 比率 4:1, 补偿 +6dB

// 或直接修改结构体
CS43131_g.compressor.enable = 1;
CS43131_g.compressor.threshold_db = -12;
CS43131_g.compressor.ratio = 4;
CS43131_g.compressor.makeup_db = 6;
```

#### Echo / 延迟

- **延迟时间**：10 ~ 100 ms（最大 4410 立体声样本 = 17.6KB）
- **反馈量**：0 ~ 200（对应 0% ~ 78%）
- **混合比**：0 ~ 100（dry/wet 百分比）
- **实现**：环形延迟线 + 反馈 + 干湿混合

```c
Audio_Echo_Enable(1);
Audio_Echo_SetParams(80, 120, 40);  // 80ms 延迟, 47% 反馈, 40% 湿信号

// 或直接修改结构体
CS43131_g.echo.enable = 1;
CS43131_g.echo.delay_ms = 80;
CS43131_g.echo.feedback = 120;
CS43131_g.echo.mix = 40;
```

### CH378 共享机制

音频播放期间 CH378 **不被独占**，其他模块可正常进行文件读写操作：

1. `Audio_Process()` 在主循环中为每个通道执行 **open → seek → read 4KB → close** 完整序列，一次调用内完成，不会被其他主循环函数打断
2. 通道文件读取完毕即关闭句柄，CH378 在两次 `Audio_Process()` 调用之间完全空闲
3. 16KB 环形缓冲提供约 93ms 的安全窗口，即使 CH378 被 CLI 命令占用数毫秒也不会导致音频断流
4. `Audio_CH378_Lock()` / `Audio_CH378_Unlock()` 提供显式锁机制，供需要连续 CH378 操作的场景使用

### 编程接口速查

| 函数 | 说明 |
|------|------|
| `Audio_ChannelStart(ch, path, info)` | 在指定通道启动 WAV 流式播放 |
| `Audio_ChannelStop(ch)` | 停止指定通道 |
| `Audio_StopAll()` | 停止所有通道 |
| `Audio_ChannelPause(ch)` / `Audio_ChannelResume(ch)` | 暂停/恢复指定通道 |
| `Audio_ChannelSetVolume(ch, vol)` | 设置通道音量（0~100） |
| `Audio_ChannelSetPan(ch, pan)` | 设置通道声像（0=左, 128=中, 255=右） |
| `Audio_SetVolume(vol)` | 设置主音量（CS43131 I2C 寄存器） |
| `Audio_Pause()` / `Audio_Resume()` | 全局暂停/恢复 |
| `Audio_Process()` | 主循环中调用，从 CH378 补充通道缓冲区 |

---

## 架构演进说明

### 从双核协作到 V5F 单核管理

项目早期采用 V3F + V5F 双核协作架构：
- V3F 负责 Keyboard、Power、Submodels、CH9350 等模块
- V5F 负责 CS43131、CH378、CH585F、Display 等模块
- 两核通过 `hardware_g`（共享内存段）和 HSEM 信号量进行数据同步

该架构存在以下问题：
1. **跨核数据同步复杂**：V3F 无法直接访问 V5F 的 `CS43131_g`，需要通过共享内存中转音频状态
2. **volatile 修饰导致编译警告**：共享变量必须声明为 `volatile`，与标准库函数不兼容
3. **HSEM 信号量增加延迟**：按键事件队列需要信号量保护，Push/Pop 有额外开销
4. **调试困难**：跨核问题难以复现和定位

重构后，所有模块由 V5F 统一管理：
- `hardware_g` 从共享内存段移至 V5F 普通全局变量，不再需要 `volatile`
- 移除 `config_request_t`（V3F→V5F 配置请求）和 `core_key_queue_t`（V3F→V5F 按键队列）
- `Key_PollAndPush()` + `Key_ProcessEvents()` 合并为 `Key_PollAndProcess()`
- `Audio_SyncToSharedMemory()` 重命名为 `Audio_SyncToHardware()`
- V3F 仅保留启动 V5F 的功能，然后进入 STOP 低功耗模式
- V5F 链接脚本获得 512K FLASH 和 ~511K RAM（DTCM+SRAM），V3F 仅保留最小 16K RAM_CODE + 16K RAM

### 音频引擎 V2.0 重构

音频子系统从单文件流式播放升级为多通道混音引擎：

- **旧架构**：单文件打开保持 → 32KB Ring Buffer → DMA → I2S，播放期间 CH378 被独占
- **新架构**：2 路虚拟通道，每路 16KB Ring Buffer，CH378 时分复用（4KB 突发读取后立即关闭），Mixer + 效果器链（EQ/Compressor/Echo）在 DMA 中断中处理
- **CH378 不再被音频独占**：文件读完即关，其他模块可自由使用 CH378 进行文件读写
- **效果器配置**嵌入 `cs43131_t` 全局结构体（`CS43131_g.eq`/`compressor`/`echo`），可在任何模块中实时调整
