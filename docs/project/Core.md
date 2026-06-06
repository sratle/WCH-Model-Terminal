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
- `audio_status` — 音频状态（播放状态、音量、曲目名等）
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
| ITCM | 0x200A0000 | 128K | V5F 代码 | V5F 指令运行 |
| DTCM | 0x200C0000 | 288K | V5F 数据 | V5F .data .bss 栈堆（含原共享 32K） |
| RAM_CODE | 0x20100000 | 64K | V3F 代码 | V3F 指令运行 |
| SRAM | 0x20110000 | 448K | V3F 数据 | V3F .data .bss 栈堆（已恢复完整大小） |

> **注意**：原 `RAM_SHARED`（0x20178000, 32K）已移除。V5F 的 DTCM 增加了 32K，V3F 的 SRAM 恢复为完整 448K。由于 V3F 不再运行业务逻辑，其 SRAM 实际使用极少。

---

## 项目目录结构

```
core/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块
│   │   ├── CH378/                  # CH378 文件管理芯片驱动
│   │   ├── CH585F/                 # CH585F 蓝牙无线芯片交互
│   │   ├── CH9350/                 # CH9350 HID 转串口驱动
│   │   ├── CS43131/                # CS43131 音频 DAC 驱动
│   │   ├── Display/                # 屏幕模块驱动
│   │   ├── I2c_soft/               # 软件 I2C 基础驱动
│   │   ├── Key/                    # 按键基础驱动
│   │   ├── Keyboard/               # 键盘模块驱动
│   │   ├── Power/                  # 供电管理模块
│   │   ├── Protocol/               # 统一通信协议栈
│   │   ├── Submodels/              # 子模块/配件管理
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
- V5F 链接脚本增加 32K DTCM（原 RAM_SHARED），V3F 链接脚本恢复完整 448K SRAM
