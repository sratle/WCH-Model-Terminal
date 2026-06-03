# 项目结构

## 概述

这是一个基于 **CH32H417** 的双核项目：

- **V5F**：主执行核心，负责主要业务逻辑与复杂外设交互。
- **V3F**：辅助核心，负责唤醒 V5F 以及串口数据转发。

驱动初始化相关任务统一在 `hardware.h / hardware.c` 中调度，核心外设包括 **CS43131**、**CH378** 等；**CH585F** 为独立 MCU，自主完成初始化。

---

## 双核职责

### V3F 核心

- 初始化八个串口。
- 扫描当前模块组成情况。
- 更新 `hardware.c` 中的全局配件状态。
- 设置**系统初始化标志位 0**。
- 后续任务：处理配件、供电、键盘模块的交互；如需更新屏幕状态，通过共享变量通知 V5F 代发。

### V5F 核心

- 作为主力执行核心，运行板上系统的主要初始化逻辑。
- 初始化 **CH378** 和 **CS43131**。
- 从 **CH378** 中读取用户自定义持久化配置，更新全局配置状态。
- 设置**系统初始化标志位 1**。
- 后续任务：处理屏幕模块与 **CH378**、**CS43131**、**CH585F** 的交互（文件读取显示、音频播放与状态同步、BLE 设备管理与数据透传），处理CLI相关事件。
  
  > 注：CH585F 为独立 MCU，V5F 仅负责与其通信，无需执行硬件初始化。

### 同步机制

- 两核各自独立完成初始化，但系统初始化状态标志位会进行同步。
- 标志位定义在 `hardware.c` 中作为全局变量，两核共享内存访问；为避免编译器优化导致可见性问题，需将其声明为 `volatile`。
- 两核均等待系统初始化标志位全部设置完毕后，才继续执行后续任务。
- 屏幕模块（UART4）由 **V5F** 独占访问，V3F 通过共享内存 / HSEM 向 V5F 提交显示请求。
- 由于两核位于同一 MCU，外设可被双核同时访问；屏幕在同一时间仅与一类外设交互（例如播放音乐时由 V5F 主导），因此无需复杂的总线仲裁机制，基本不会造成阻塞。

---

## 核间通信机制

CH32H417 内置 V5F（主执行核心）和 V3F（辅助核心）两个 RISC-V 核心，两者拥有各自独立的 RAM 区域，
但可以通过 **共享 SRAM** 和 **硬件信号量（HSEM）** 实现数据交换与同步。

### 内存布局

| 区域          | 起始地址       | 大小   | 归属       | 用途                  |
| ----------- | ---------- | ---- | -------- | ------------------- |
| ITCM        | 0x200A0000 | 128K | V5F 代码   | V5F 指令运行            |
| DTCM        | 0x200C0000 | 256K | V5F 数据   | V5F .data .bss 栈堆    |
| RAM_CODE    | 0x20100000 | 64K  | V3F 代码   | V3F 指令运行            |
| SRAM        | 0x20110000 | 448K | V3F 数据   | V3F .data .bss 栈堆   |
| **RAM_SHARED** | **0x20178000** | **32K** | **双核共享** | **核间通信共享数据**        |

### 共享数据实现

#### 1. 链接脚本配置

V3F 和 V5F 的链接脚本（`Link_v3f.ld` / `Link_v5f.ld`）中均定义了 `RAM_SHARED` 内存区域和 `.shared_data` 段：

```ld
/* MEMORY 区域 */
RAM_SHARED (xrw) : ORIGIN = 0x20178000, LENGTH = 32K

/* SECTIONS 中 */
.shared (NOLOAD) :
{
    . = ALIGN(4);
    KEEP(*(SORT_NONE(.shared_data)))
    . = ALIGN(4);
} >RAM_SHARED
```

两个核心的 `RAM_SHARED` 起始地址完全相同（`0x20178000`），确保物理内存一致性。

> **注意**：V3F 的 RAM 区域从 448K 缩减为 416K（448K - 32K），为共享内存腾出空间。

#### 2. 共享变量声明

使用 `__attribute__((section(".shared_data")))` 将变量放入共享内存段：

```c
/* hardware.c */
__attribute__((section(".shared_data")))
volatile hardware_t hardware_g;
```

由于 V3F 和 V5F 是独立编译的工程，各自编译 `hardware.c` 后 `hardware_g` 都会被链接器放置在 `0x20178000`，
因此两个核心访问的是同一块物理内存，实现了真正的数据共享。

#### 3. 共享数据初始化

`Shared_Init()` 在两个核心的 `main()` 中最先调用（在 `USART_Printf_Init` 之前），
将 `hardware_g` 清零。由于 V3F 先启动并唤醒 V5F，V3F 的 `Shared_Init()` 先执行，
V5F 的 `Shared_Init()` 会再次清零但此时 V3F 可能已经开始写入数据，因此 V5F 的
`Shared_Init()` 在双核模式下应跳过（由 V3F 统一初始化）。

### 硬件信号量（HSEM）保护

对共享数据的并发访问使用 CH32H417 内置的硬件信号量（HSEM）进行互斥保护：

| 信号量       | 用途                          |
| --------- | --------------------------- |
| HSEM_ID0  | 双核启动同步（V3F 唤醒 V5F 时使用）     |
| HSEM_ID1  | `key_queue` 按键事件队列互斥保护      |

#### 使用模式

```c
/* 获取信号量 */
if (HSEM_FastTake(HSEM_ID1) != READY)
    return;  /* 另一核心正在访问，放弃本次操作 */

/* ... 访问共享数据 ... */

/* 释放信号量 */
HSEM_ReleaseOneSem(HSEM_ID1, 0);
```

- `HSEM_FastTake()` 尝试获取信号量，返回 `READY` 表示成功，`ERROR` 表示被另一核心占用。
- `HSEM_ReleaseOneSem()` 释放信号量，第二个参数为 ProcessID（当前固定为 0）。
- 所有对 `hardware_g.key_queue` 的访问（Push/Pop）均受 HSEM_ID1 保护。

### 核间数据流

#### 按键事件传递（V3F → V5F）

V3F 核心扫描 GPIO 按键（PF8/PF9/PF10），检测到按键事件后推入共享环形队列，
V5F 核心从队列中取出事件并处理（音量调节、发送 Enter 信号等）：

```
V3F 主循环                    共享内存 (0x20178000)              V5F 主循环
┌──────────┐                ┌──────────────────┐            ┌──────────────┐
│Key_PollAndPush│ ──Push──> │  key_queue (环形)   │ ──Pop──> │Key_ProcessEvents│
│(PF8/PF9/PF10) │           │  HSEM_ID1 互斥保护  │          │(音量/Enter)    │
└──────────┘                └──────────────────┘            └──────────────┘
```

#### 心跳状态（双核各自管理）

`hardware_g.hb_slots` 和 `hardware_g.hb_tick` 由两个核心各自管理不同模块的心跳，
V5F 负责 Display，V3F 负责 Keyboard/Power/Submodels，字段无并发冲突。

### 环形队列实现

```c
#define CORE_KEY_QUEUE_SIZE 16

typedef struct {
    uint8_t key_id;   /* 按键标识: 0x01=PLUS, 0x02=SUB, 0x03=ENTER */
    uint8_t event;    /* 事件类型: 0x00=释放, 0x01=按下, 0x02=长按 */
} core_key_event_t;

typedef struct {
    core_key_event_t queue[CORE_KEY_QUEUE_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} core_key_queue_t;
```

- 队列满时 Push 静默丢弃，队列空时 Pop 返回 0。
- `head` 和 `tail` 声明为 `volatile`，确保编译器不会缓存寄存器值。

---

## 项目目录结构

```
core/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块
│   │   ├── CH378/                  # CH378 文件管理芯片驱动
│   │   ├── CH585F/                 # CH585F 蓝牙无线芯片交互
│   │   ├── CH9350L/                # CH9350L HID转串口驱动外接HID设备
│   │   ├── CS43131/                # CS43131 音频 DAC 驱动
│   │   ├── Display/                # 屏幕模块驱动
│   │   ├── I2c_soft/               # 软件 I2C 基础驱动
│   │   ├── Key/                    # 按键基础驱动
│   │   ├── Keyboard/               # 键盘模块驱动
│   │   ├── Power/                  # 供电管理模块
│   │   ├── Submodels/              # 子模块/配件管理
│   │   ├── hardware.c              # 全局调度与双核初始化入口
│   │   └── hardware.h              # 全局调度头文件
│   ├── Core/                       # RISC-V 内核相关文件
│   ├── Debug/                      # 调试支持文件
│   ├── Peripheral/                 # 芯片外设库
│   └── Startup/                    # 启动文件
│
├── V3F/                            # V3F 核心工程
│   ├── Ld/                         # V3F 链接脚本
│   ├── User/                       # V3F 用户代码（main.c / 中断 / 系统配置）
│   ├── .cproject
│   ├── .project
│   └── core_V3F.wvproj
│
├── V5F/                            # V5F 核心工程
│   ├── Ld/                         # V5F 链接脚本
│   ├── User/                       # V5F 用户代码（main.c / 中断 / 系统配置）
│   ├── .cproject
│   ├── .project
│   └── core_V5F.wvproj
│
├── docs/                           # 项目文档与数据手册
│   ├── CH32H417DS0.pdf
│   ├── CH32H417RM.pdf
│   ├── CH378DS1.PDF
│   ├── CH585DS1.PDF
│   ├── CH9329DS1.PDF
│   ├── CS43131_DS1155F2.pdf
│   └── PROJECT.md
│
└── core.wvsln                      # MounRiver Studio / WCH 解决方案文件
```

---

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`i2c_soft.h`。
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`。
- **变量**：统一小写加下划线，例如 `system_init_flag`。
- **文件夹**：下划线写法，仅首字母大写，例如 `I2c_soft`、`Submodels`。
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范。

---

## 架构规划

需要进行 **core model** 的整体架构设计，确保：

1. 双核初始化流程清晰、可维护。
2. 公共模块（`Common/Common/`）与具体核心解耦。
3. 同步原语（标志位 / 共享内存）定义明确。
4. 屏幕模块仅由 V5F 直接访问，V3F 通过跨核消息间接交互。
