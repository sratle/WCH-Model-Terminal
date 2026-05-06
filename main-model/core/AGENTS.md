<!-- From: /home/sratle/workspace/embedded/WCH-Model-Terminal/main-model/core/AGENTS.md -->
# AGENTS.md

> 本文件面向 AI 编程助手。若你是第一次接触本项目，请先阅读本文档，再修改代码。

---

## 项目概述

本项目是 **WCH CH32H417** 双核 RISC-V 微控制器的固件工程，属于一款模块化终端设备的 **Core Model（核心板固件）**。

- **MCU**: CH32H417QEU6（青稞 RISC-V 双核：V3F + V5F）
- **IDE**: MounRiver Studio (MRS) / WCH Studio（基于 Eclipse CDT）
- **工具链**: RISC-V Embedded GCC（`riscv-none-elf-gcc`）
- **语言**: C（GNU99 标准），无 RTOS（NoneOS）
- **调试器**: WCH-Link，使用 OpenOCD + GDB，支持双核同步调试

### 双核分工

| 核心 | 主频 | 职责 |
|------|------|------|
| **V5F** | 400 MHz | 主执行核心，负责复杂外设与主要业务逻辑：CH378（文件管理）、CS43131（音频 DAC）、CH585F（蓝牙/无线，SPI 通信）、Display（屏幕模块，UART4） |
| **V3F** | 100 MHz | 辅助核心，负责唤醒 V5F、串口数据转发、扫描模块组成：Keyboard（UART3）、Power（UART5）、Submodels（UART6~8）、CH9350（HID 转串口） |

两核通过 `hardware.c` 中的 `volatile` 全局标志位进行初始化同步，并可通过 HSEM（硬件信号量）进行跨核互斥。两核共享同一 MCU 的外设总线，但不会在同一时间操作同一外设，因此无需复杂总线仲裁。

---

## 项目目录结构

```
core/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块（业务相关）
│   │   ├── CH378/                  # CH378 文件管理芯片驱动
│   │   ├── CH585F/                 # CH585F 蓝牙无线芯片交互
│   │   ├── CH9350/                 # CH9350 HID 转串口驱动
│   │   ├── CS43131/                # CS43131 音频 DAC 驱动
│   │   ├── Display/                # 屏幕模块驱动
│   │   ├── I2c_soft/               # 软件 I2C 基础驱动
│   │   ├── Key/                    # 按键基础驱动
│   │   ├── Keyboard/               # 键盘模块驱动
│   │   ├── Power/                  # 供电管理模块
│   │   ├── Protocol/               # 统一通信协议栈（打包 / 解析状态机）
│   │   ├── Submodels/              # 子模块/配件管理
│   │   ├── hardware.c              # 全局调度与双核初始化入口
│   │   └── hardware.h              # 全局调度头文件
│   ├── Core/                       # RISC-V 内核相关文件（core_riscv.c / .h）
│   ├── Debug/                      # 调试支持文件（debug.c / .h，printf 重定向等）
│   ├── Peripheral/                 # 芯片官方外设库（inc/ + src/）
│   │   └── inc/ch32h417_*.h        # 各外设头文件
│   │   └── src/ch32h417_*.c        # 各外设源文件
│   └── Startup/                    # 启动文件（startup_ch32h417_v3f.S / v5f.S）
│
├── V3F/                            # V3F 核心工程
│   ├── Ld/Link_v3f.ld              # V3F 链接脚本
│   ├── User/                       # V3F 用户代码
│   │   ├── main.c                  # V3F 主函数入口
│   │   ├── ch32h417_it.c/.h        # V3F 中断服务程序
│   │   ├── system_ch32h417.c/.h    # 系统时钟配置（含 SystemInit）
│   │   └── ch32h417_conf.h         # 外设库头文件包含配置
│   ├── .cproject                   # Eclipse CDT 项目配置
│   ├── .project                    # Eclipse 项目元数据
│   ├── core_V3F.wvproj             # MRS 项目配置（JSON）
│   └── obj/                        # 编译产物（.elf / .hex / .bin / .map / .lst）
│
├── V5F/                            # V5F 核心工程
│   ├── Ld/Link_v5f.ld              # V5F 链接脚本
│   ├── User/                       # V5F 用户代码（结构同 V3F）
│   ├── .cproject
│   ├── .project
│   ├── core_V5F.wvproj             # MRS 项目配置（当前为空文件）
│   └── obj/                        # 编译产物（含 Merge.bin 合并固件）
│
├── docs/                           # 项目文档与数据手册
│   ├── PROJECT.md                  # 项目结构、通信协议、命名规范等详细文档
│   ├── Protocol_Display.md         # Display 模块通信协议细则
│   ├── Protocol_Keyboard.md        # Keyboard 模块通信协议细则
│   ├── Protocol_Power.md           # Power 模块通信协议细则
│   ├── Protocol_Submodels.md       # Submodel 模块通信协议细则
│   ├── Protocol_Wireless.md        # Wireless 模块通信协议细则
│   └── Core.md / Display-*.md / Keyboard-*.md / Power-*.md / Submodel-*.md / Wireless.md
│
└── core.wvsln                      # MounRiver Studio / WCH 解决方案文件
```

---

## 构建系统与编译命令

### IDE 构建（推荐）

本项目使用 **MounRiver Studio** 作为首选开发环境：

1. 打开 `core.wvsln` 解决方案文件。
2. 构建顺序已配置为：先 `core_V3F`，后 `core_V5F`。
3. 按 `Ctrl+B` 或点击 Build 按钮即可生成固件。
4. 编译产物位于各工程的 `obj/` 目录下：
   - `core_V3F.elf` / `.hex` / `.bin`
   - `core_V5F.elf` / `.hex` / `.bin`
   - `Merge.bin`（V5F 工程产物，用于合并烧录）

### 命令行构建

各工程的 `obj/` 目录下包含由 MRS 自动生成的 `makefile` 和 `subdir.mk`，理论上可在安装对应工具链后通过 `make` 构建，但 **不建议手动修改这些生成文件**。若需命令行编译，推荐在 MRS 中导出 Makefile 或使用其内置命令行工具。

### 关键编译参数

- **V3F 定义宏**: `Core_V3F`
- **V5F 定义宏**: `Core_V5F`
- **架构**: `-march=rv32imac_zba_zbb_zbc_zbs_xw`
- **ABI**: `-mabi=ilp32`
- **优化级别**: `-Os`（按尺寸优化）
- **关键标志**: `-ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized`
- **链接选项**: `-nostartfiles -Xlinker --gc-sections --specs=nano.specs --specs=nosys.specs`

### 时钟配置

当前配置为 **HSE 25MHz** 输入：
- 系统时钟 SYSCLK = 400 MHz
- V5F 核心时钟 = 400 MHz
- V3F 核心时钟 = 100 MHz（HCLK = SYSCLK / 4）

时钟配置代码位于 `V3F/User/system_ch32h417.c`（含 `SystemInit`）和 `V5F/User/system_ch32h417.c`（仅 `SystemAndCoreClockUpdate`）。V3F 负责上电初始化 PLL，V5F 不重复执行 `SystemInit`。

---

## 代码组织与模块划分

### 模块驱动结构

每个自定义模块驱动通常包含一个独立的子目录，内部为 `模块名.c` + `模块名.h` 的配对文件：

```
Common/Common/<Module>/
├── <module>.c
└── <module>.h
```

模块头文件通常定义：
- 该模块使用的 GPIO 引脚与复用功能（如 `CS43131_RST_GPIO_PORT`）
- 模块配置结构体（如 `cs43131_t`、`display_t`）
- 模块初始化与操作函数原型

### 统一协议栈模块（Protocol）

`Common/Common/Protocol/` 提供所有 UART 模块共享的紧凑二进制协议基础实现：

- **`Protocol_InitRxCtx()`** / **`Protocol_ResetRxCtx()`**：初始化 / 重置接收状态机上下文
- **`Protocol_PackFrame()`**：将 `src`、`dst`、`cmd`、`data[]` 打包为字节流，末尾附加固定帧尾 `A5 5A FC FD`
- **`Protocol_ParseByte()`**：逐字节状态机解析，返回 1 表示完整帧就绪

状态机流程：`WAIT_HEAD` → `WAIT_SRC` → `WAIT_DST` → `WAIT_LEN` → `WAIT_CMD` → `WAIT_DATA` → `WAIT_TAIL0` → `WAIT_TAIL1` → `WAIT_TAIL2` → `WAIT_TAIL3` → `FRAME_READY`。仅在 `WAIT_HEAD` 状态下识别 `0xAA` 为新帧起始；`LEN` 为 0 时自动丢弃；数据域溢出时静默丢弃但不影响帧完成判断；帧尾不匹配时丢弃整帧。

各模块（Display、Keyboard、Power、Submodels）应在初始化时调用 `Protocol_InitRxCtx()` 创建接收上下文，并在 UART 中断中逐字节喂入 `Protocol_ParseByte()`。

### 全局调度中心

`Common/Common/hardware.c` 与 `hardware.h` 是双核初始化的调度中心：

- `hardware_g`：`volatile hardware_t` 全局变量，用于双核共享初始化状态。
- `hardware_init_flag`：按位标志，V5F 设置低 4 位（`0x01` ~ `0x08`），V3F 设置高 4 位（`0x10` ~ `0x80`）。
- `Hardware_V5F_init()`：初始化 CS43131 → CH378 → CH585F → Display。
- `Hardware_V3F_init()`：初始化 Key → Power → Keyboard → CH9350 → Submodels。
- 两核均通过 `while (hardware_g.hardware_init_flag != 0xFF);` 等待对方完成初始化。

### 启动流程

1. V3F 上电，执行 `SystemInit()` 配置时钟，唤醒 V5F（`NVIC_WakeUp_V5F`）。
2. V3F 进入 `PWR_EnterSTOPMode` 低功耗等待，或继续执行（取决于 `Run_Core` 宏配置）。
3. V5F 唤醒后执行自身初始化逻辑。
4. 两核分别调用各自的 `Hardware_V*F_init()`，完成后进入主循环。

> **注意**：当前 `V3F/User/main.c` 与 `V5F/User/main.c` 中调用的是 `hardware_V3F_init()` / `hardware_V5F_init()`（首字母小写），但 `hardware.c` 中实际定义及 `hardware.h` 中声明为 `Hardware_V3F_init()` / `Hardware_V5F_init()`（首字母大写）。在 Linux 等大小写敏感环境下可能产生隐式声明警告或链接错误，修改时需保持命名一致。

---

## 通信协议规范

本项目采用统一的紧凑二进制通信协议，覆盖 Core 与 Display、Keyboard、Power、Submodels、CH585F 之间的所有数据交互。详细规范见 `docs/PROJECT.md` 第 8 章，以及各模块对应的 `Protocol_*.md` 文档。

### 帧结构

```
+--------+--------+--------+--------+--------+-------------+
|  HEAD  |  SRC   |  DST   |  LEN   |  CMD   |  DATA[0..N] |
| 1 byte | 1 byte | 1 byte | 1 byte | 1 byte |   N bytes   |
+--------+--------+--------+--------+--------+-------------+
```

- **HEAD**: 固定 `0xAA`
- **LEN**: `CMD + DATA` 的总字节数，最大 255，故最大帧长 260 字节
- **波特率**: 
  - Display 模块为 921600/8-N-1
  - 其余模块为 115200/8-N-1（异步中断驱动，不阻塞等待）

### 模块 ID

| ID | 模块 | 物理接口 | 主要交互核心 |
|----|------|----------|--------------|
| `0x00` | Core（核心） | — | V3F / V5F |
| `0x01` | CH585F（无线/蓝牙） | SPI4 | V5F |
| `0x10` | Display（屏幕模块） | UART4 | V5F / V3F |
| `0x20` | Keyboard（键盘模块） | UART3 | V3F |
| `0x30` | Power（供电模块） | UART5 | V3F |
| `0x40` ~ `0x42` | Submodel-1/2/3（配件槽） | UART6/7/8 | V3F |

> **注意**：CH9350 保持其独立的 `0x57 0xAB` 专用协议，不参与上述统一协议。

---

## 命名规范与代码风格

项目遵循以下命名约定（详见 `docs/PROJECT.md`）：

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`i2c_soft.h`。
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`、`CS43131_init()`、`Audio_PlayTriangleWave()`。
- **变量**：统一小写加下划线，例如 `hardware_init_flag`、`system_clock`。
- **结构体类型名**：小写加下划线 + `_t`，例如 `cs43131_t`、`display_t`。
- **文件夹**：下划线写法，仅首字母大写，例如 `I2c_soft`、`Submodels`。
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持 WCH 官方命名，不参与上述规范。
- **宏定义**：全大写 + 下划线，例如 `AUDIO_BUFFER_SIZE`、`CS43131_I2C_ADDR`。

### 头文件模板

每个模块头文件应包含：

```c
#ifndef __MODULE_NAME_H__
#define __MODULE_NAME_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
// ... 模块代码 ...

#ifdef __cplusplus
}
#endif

#endif
```

---

## 烧录与调试

### 烧录配置

- **调试接口**: 1-wire serial（WCH-Link）
- **V3F Flash 起始地址**: `0x08000000`
- **V5F Flash 起始地址**: `0x00010000`（链接脚本配置）
- V3F 的 `.wvproj` 中配置了 `dualCoreDebug: true`，支持同时调试两核
- V5F 工程可生成 `Merge.bin`，用于合并烧录两核固件

### 调试注意事项

- V3F 是 **Master 核心**（`isMaster: true`），GDB 端口 3333；V5F 为 Slave，端口 3334。
- 启动文件中的 `.load` 段负责在复位后将代码从 Flash 复制到 RAM 中执行（highcode / data / bss 初始化）。
- 双核共享部分内存区域，跨核访问的变量必须声明为 `volatile`。

---

## 开发注意事项

### 修改代码前必读

1. **区分双核代码**：
   - V3F 专属代码在 `V3F/User/` 和通过 `Core_V3F` 宏编译的部分。
   - V5F 专属代码在 `V5F/User/` 和通过 `Core_V5F` 宏编译的部分。
   - `Common/Common/` 中的模块通常被两核共用，但具体实例化由 `hardware.c` 控制。

2. **外设分配**：
   - 两核位于同一 MCU，外设可被双核同时访问。
   - 屏幕（UART4）虽然两核均可访问，但同一时刻仅有一核发送数据，Display 无需处理并发冲突。
   - CH585F 仅与 V5F 通过 SPI4 通信。

3. **中断处理**：
   - 中断服务函数需按 WCH 风格声明：`void XXX_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));`
   - 部分 IRQ 可通过 `NVIC_SetAllocateIRQ()` 分配到指定核心。

4. **不要修改生成的构建文件**：
   - `V3F/obj/makefile`、`subdir.mk`、`objects.mk`、`sources.mk` 等由 MRS 自动生成。
   - 如需调整编译选项，应通过 IDE 的 `.cproject` / `.wvproj` 修改，而非直接编辑 Makefile。

5. **新增模块时**：
   - 在 `Common/Common/` 下新建模块目录，放置 `module.c` 和 `module.h`。
   - 在 `hardware.c` 中包含头文件并添加全局变量和初始化调用。
   - 若涉及通信协议，需在 `docs/PROJECT.md` 及对应的 `Protocol_*.md` 中补充模块 ID 和操作码定义。
   - 若模块使用统一协议，应在模块初始化时调用 `Protocol_InitRxCtx()` 初始化接收上下文。

6. **大小写敏感环境注意事项**：
   - `Common/Common/Submodels/` 目录在文件系统中为 `Submodels`（小写 s），但 `hardware.c` 中 `#include` 写作 `"SubModels/submodels.h"`。
   - `main.c` 中调用 `hardware_V3F_init()` 而声明为 `Hardware_V3F_init()`，在大小写敏感的文件系统或严格编译器下可能出现问题。
   - 建议在 Linux / 命令行构建环境下保持文件名、包含路径、函数名的大小写完全一致。

---

## 测试策略

本项目当前 **未引入自动化单元测试框架**。验证方式主要为：

1. **硬件在环测试**：通过 WCH-Link 烧录后，在真实硬件上观察串口打印（`USART_Printf_Init(115200)`）。
2. **调试输出**：大量使用 `printf()` 输出运行状态，两核均有独立的串口日志输出。
3. **协议验证**：通过逻辑分析仪或串口助手抓取 UART/SPI 数据，核对通信协议帧格式。

如需添加测试，建议：
- 在 `Common/Common/` 下新增 `Test/` 目录存放测试桩代码。
- 利用 `hardware_init_flag` 等标志位设计初始化顺序测试。
- 对通信协议解析函数进行边界条件测试（最大帧长 264 字节、帧头 0xAA 转义、LEN=0 非法丢弃、帧尾 A5 5A FC FD 校验等）。

---

## 安全与部署注意事项

- **Flash 保护**: 生产部署时可启用读保护，防止固件被非法提取。
- **时钟安全**: `system_ch32h417.c` 中若 HSE 启动失败会静默回退，生产代码应增加错误处理分支。
- **栈大小**: 链接脚本中 `__stack_size = 2048`，若新增复杂递归或大数据局部变量，需评估栈溢出风险。
- **Volatile 使用**: 双核共享的变量（如 `hardware_g`）必须使用 `volatile`，避免编译器优化导致可见性问题。
- **CH585F 为独立 MCU**: V5F 仅通过 SPI4 与 CH585F 通信，不负责 CH585F 的固件升级。CH585F 的固件维护在独立仓库中进行。

---

## 参考文档

- `docs/PROJECT.md` — 项目结构、双核职责、通信协议 V1.0、命名规范、模块 ID 与操作码定义
- `docs/Protocol_Display.md` — Display 模块通信协议细则
- `docs/Protocol_Keyboard.md` — Keyboard 模块通信协议细则
- `docs/Protocol_Power.md` — Power 模块通信协议细则
- `docs/Protocol_Submodels.md` — Submodel 模块通信协议细则
- `docs/Protocol_Wireless.md` — Wireless 模块通信协议细则
- `docs/CH32H417DS0.pdf` / `CH32H417RM.pdf` — 芯片数据手册与参考手册
- `docs/CH378DS1.PDF` / `CS43131_DS1155F2.pdf` / `CH585DS1.PDF` — 外设芯片数据手册

---

> 最后更新：2026-04-23
