# LVGL + FMC8080 + SSD1963 驱动方案

## 一、整体架构

- **运行核**：V5F（主频 400MHz，负责 UI 渲染）
- **显示通信**：FMC NORSRAM Bank1，16-bit 8080 并口驱动 SSD1963
- **模块间通信**：USART1（PA9-TX / PA10-RX）与核心模块交互，接收键盘/鼠标/虚拟触摸等输入数据
- **显存策略**：SSD1963 内部集成 1215KB GRAM 作为物理显存；V5F 内部 SRAM（512KB 可用）分配约 153KB 作为 LVGL 双缓冲刷新区（约 1/10 屏高，RGB565 格式）
- **LVGL 版本**：LVGL v9.5
- **项目结构**：驱动层按功能模块化，放置在 `Common/Common/` 下的 `FMC/`、`SSD1963/`、`LVGL_LCD/`、`UI/`、`UART/`、`Touch/` 目录中
- **输入方式**：
  - **本地输入**：电容触摸屏通过 I2C2（PB10/PB11）直接读取触摸坐标
  - **远程输入**：核心模块通过 USART1 发送键盘/鼠标/虚拟触摸数据，LCD 模块解析后注入 LVGL 输入系统
  - 两种输入方式可同时存在，由应用层决定优先级或叠加策略

## 二、硬件连接与 FMC 引脚配置

根据原理图，SSD1963 采用 **8080 并行模式**（`CONF` 引脚接高电平），MCU 通过 **FMC NORSRAM Bank1** 的 16-bit 数据总线与其通信。

### 2.1 数据总线（FMC_D0~D15，16-bit）

| FMC 信号 | MCU 引脚 | 说明 |
|----------|----------|------|
| FMC_D0 | PD14 | 数据线 bit0 |
| FMC_D1 | PD15 | 数据线 bit1 |
| FMC_D2 | PD0  | 数据线 bit2 |
| FMC_D3 | PD1  | 数据线 bit3 |
| FMC_D4 | PE7  | 数据线 bit4 |
| FMC_D5 | PE8  | 数据线 bit5 |
| FMC_D6 | PE9  | 数据线 bit6 |
| FMC_D7 | PE10 | 数据线 bit7 |
| FMC_D8 | PE11 | 数据线 bit8 |
| FMC_D9 | PE12 | 数据线 bit9 |
| FMC_D10 | PE13 | 数据线 bit10 |
| FMC_D11 | PE14 | 数据线 bit11 |
| FMC_D12 | PE15 | 数据线 bit12 |
| FMC_D13 | PD8  | 数据线 bit13 |
| FMC_D14 | PD9  | 数据线 bit14 |
| FMC_D15 | PD10 | 数据线 bit15 |

### 2.2 控制总线（8080 模式）

| 信号 | MCU 引脚 | FSMC 功能 | 功能说明 |
|------|----------|-----------|----------|
| WR | PD5 | FSMC_NWE (AF12) | 写使能，FMC 硬件自动控制 |
| RD | PD4 | FSMC_NOE (AF12) | 读使能，FMC 硬件自动控制 |
| DC (RS) | PB3 | FSMC_A1 (AF12) | 数据/命令选择，通过 FMC 地址偏移自动区分 |
| CS | PD7 | FSMC_NE1 (AF12) | 片选，FMC Bank1 硬件自动控制 |
| RESET | PA0 | 普通 GPIO | SSD1963 硬件复位，普通 GPIO 推挽输出 |
| TE | PB8 | 普通 GPIO | Tearing Effect（可选，用于同步刷新，防止撕裂） |

> **优势**：WR/RD/CS/DC 全部使用 FMC 硬件复用引脚，刷新时无需软件手动翻转 GPIO，CPU 开销最小，刷新效率最高。

### 2.3 LCD 屏控制信号

原理图第三页标注屏幕采用 **DE 模式**，相关控制引脚如下：

| 信号 | MCU 引脚 | 功能说明 |
|------|----------|----------|
| LCD_MODE | PA4 | 需拉高，选择 DE 模式（SYNC 引脚可不接） |
| LCD_L/R | PA5 | 屏幕左右翻转控制 |
| LCD_U/D | PA6 | 屏幕上下翻转控制 |
| LCD_RESET | PA7 | LCD 模组复位（与 SSD1963 的 RESET 分开） |

### 2.4 模块间通信（与核心模块）

| 信号 | MCU 引脚 | 功能说明 |
|------|----------|----------|
| USART1_TX | PA9 | 向核心模块发送数据（如触摸事件、UI 状态） |
| USART1_RX | PA10 | 接收核心模块数据（如键盘/鼠标输入、虚拟触摸、显示指令） |

> **说明**：核心模块通过 USART1 向 LCD 模块发送输入数据。LCD 模块本地处理 LVGL 渲染，也可通过 USART1 上报触摸事件或 UI 状态。通信协议已由核心模块制定，LCD 模块按协议解析和封装数据。

### 2.5 背光控制

背光由 **SSD1963 内置 PWM** 控制，无需 MCU GPIO：

| 信号 | 来源 | 功能说明 |
|------|------|----------|
| BL_PWM | SSD1963 PWM 引脚 | 输出 PWM 信号到背光驱动 IC，通过 SSD1963 寄存器配置占空比 |

> **说明**：SSD1963 初始化时配置 PWM 频率和占空比即可调节背光亮度，无需占用 MCU 的 PWM 引脚。

### 2.6 触摸

| 信号 | MCU 引脚 | 功能说明 |
|------|----------|----------|
| I2C2_SCL | PB10 | 电容触摸屏 I2C 时钟 |
| I2C2_SDA | PB11 | 电容触摸屏 I2C 数据 |
| CTP_RST | PC0 | 触摸屏控制器复位 |
| CTP_INT | PC1 | 触摸屏中断信号 |

### 2.7 FMC 初始化参数
- **Bank**: `FMC_Bank1_NORSRAM1`
- **MemoryType**: `FMC_MemoryType_SRAM`
- **DataWidth**: `FMC_MemoryDataWidth_16b`
- **WriteOperation**: `FMC_WriteOperation_Enable`
- **ExtendedMode**: `FMC_ExtendedMode_Disable`
- **AccessMode**: `FMC_AccessMode_B`（8080 并口常用 Mode B）
- **时序**: 根据 V5F 主频（400MHz）和 SSD1963 datasheet 配置 `AddressSetupTime`、`DataSetupTime`

### 2.8 命令/数据地址映射
`DC` 引脚接 **PB3 (FSMC_A1)**，因此可通过 FMC 地址偏移自动区分命令和数据，无需软件手动翻转 GPIO：

```c
#define SSD1963_CMD_ADDR  (*(volatile uint16_t *)0x60000000)  // A1 = 0
#define SSD1963_DATA_ADDR (*(volatile uint16_t *)0x60000004)  // A1 = 1, 偏移 4 (2^(1+1))

static inline void SSD1963_WriteCmd(uint16_t cmd)
{
    SSD1963_CMD_ADDR = cmd;
}

static inline void SSD1963_WriteData(uint16_t data)
{
    SSD1963_DATA_ADDR = data;
}
```

> 原理：FSMC_A1 对应地址线 bit1，当访问 `0x60000004` 时，A1 自动拉高，DC = 1；访问 `0x60000000` 时，A1 自动拉低，DC = 0。FMC 硬件自动完成时序，无需 CPU 干预。

## 三、SSD1963 驱动层

### 3.1 底层读写接口
- `SSD1963_WriteCmd(uint16_t cmd)` — 通过 `SSD1963_CMD_ADDR` (0x60000000) 写命令，FMC 硬件自动拉低 DC
- `SSD1963_WriteData(uint16_t data)` — 通过 `SSD1963_DATA_ADDR` (0x60000004) 写数据，FMC 硬件自动拉高 DC
- `SSD1963_WriteDataMultiple(uint16_t *data, uint32_t len)` — 通过 `SSD1963_DATA_ADDR` 批量写入数据，利用 FMC 并行总线提速

### 3.2 初始化序列
1. 硬件复位（GPIO 拉低 >10ms）
2. 软复位（0x01）
3. 设置 PLL（0xE0/E2）
4. 设置 LCD 时序参数（HSYNC/VSYNC/ porch，0xB0~B6）
5. 设置显示尺寸 800×480（0x04/0x05 等）
6. 设置 GPIO 方向（0xB8）
7. 设置 MCU 接口为 16-bit（0x3A = 0x55 或根据 datasheet，选择 16-bit/pixel）
8. 设置像素数据格式，使 SSD1963 内部将 RGB565 数据转换为 RGB888 输出到 LCD
9. 打开显示（0x29）

> **注意**：SSD1963 原生支持 8/9/16/18/24-bit MCU 接口。本方案中 LVGL 渲染为 **RGB565**，通过 16-bit FMC 总线写入 SSD1963 GRAM，由 SSD1963 内部完成到 RGB888 的转换后驱动 LCD。

## 四、LVGL 移植

### 4.1 源码集成与裁剪
- LVGL v9.5 源码已存在于 `V5F/LVGL/`
- 在 `V5F/User/ch32h417_conf.h` 中不修改，仅在 V5F 工程编译路径中加入 LVGL 源文件
- **已完成的源码裁剪**：
  - 删除 `src/drivers/`（PC/Linux 显示驱动，不适用）
  - 删除 `src/debugging/`（调试和测试功能）
  - 删除 `src/others/`（文件浏览器、片段、翻译等高级功能）
  - `src/libs/` 只保留：`qrcode/`、`barcode/`、`fsdrv/`、`bmp/`、`tiny_ttf/`
  - `src/font/` 只保留：`lv_font.c/h`、`lv_symbol_def.h`、`fmt_txt/`、`lv_font_montserrat_8/12/16/24.c`
  - `src/themes/` 只保留 `default/` 主题
- 配置 `lv_conf.h`（基于 `LVGL/lv_conf_template.h` 修改）：
  - `LV_COLOR_DEPTH 16`（匹配 RGB565）
  - `LV_MEM_CUSTOM 0`，设置 `LV_MEM_SIZE` 约 64KB
  - `LV_USE_OS LV_OS_NONE`（无操作系统）
  - 关闭不需要的 widgets、themes、动画效果以节省代码体积
  - 注意：LVGL v9.5 的 `lv_conf.h` 格式与 v8.x 不同，部分宏名称有变化

### 4.2 显示缓冲区配置
在 V5F 的 DTCM（0x200C0000 起，约 256KB）中分配：
```c
#define LVGL_BUF_LINES    48   // 约 1/10 屏高
#define LVGL_BUF_SIZE     (800 * LVGL_BUF_LINES)

static lv_color_t buf1[LVGL_BUF_SIZE];
static lv_color_t buf2[LVGL_BUF_SIZE];
```
双缓冲总计约 153KB（RGB565，每个像素 2 字节），加上 LVGL 内部堆 64KB，V5F 可用 SRAM 512KB，内存余量非常充足。

### 4.3 Flush Callback (LVGL v9.x 风格)
```c
void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    SSD1963_SetWindow(area->x1, area->y1, area->x2, area->y2);
    SSD1963_WriteCmd(0x2C); // Memory Write

    // 批量写入 RGB565 颜色数据到 SSD1963 GRAM
    uint16_t *color_p = (uint16_t *)px_map;
    for (uint32_t i = 0; i < w * h; i++) {
        SSD1963_DATA_ADDR = color_p[i]; // 16-bit RGB565，单次写入
    }

    lv_display_flush_ready(disp);
}
```
> **说明**：LVGL v9.x 中 `lv_display_t` 替代了 `lv_disp_drv_t`，像素数据通过 `px_map` 传入。本方案中 LVGL 渲染为 RGB565，每个像素 16-bit，通过 FMC 总线单次写入，SSD1963 内部负责转换为 RGB888 输出到 LCD。

### 4.4 时基与主循环
- **Tick**：在 `SysTick_Handler`（V5F）中调用 `lv_tick_inc(1)`
- **主循环**：在 `V5F/Common/Common/hardware.c` 的 `Hardware()` 中：
```c
void Hardware(void)
{
    LCD_Init();           // FMC + SSD1963 + 屏幕方向/模式 GPIO
    LVGL_Init();          // LVGL + 显示驱动
    UART_Module_Init();   // USART1 与核心模块通信
    UI_Main_Create();     // 创建主界面

    while (1) {
        lv_timer_handler();
        UART_Module_Poll(); // 轮询接收核心模块数据
        Delay_Ms(5);
    }
}
```

## 五、双核协作修改

当前模板中 V3F 负责唤醒 V5F。由于 LVGL/UI 放在 V5F，V3F 可继续运行其他业务逻辑，或通过 HSEM/IPC 与 V5F 通信（如 V3F 处理传感器数据，通知 V5F 更新 UI）。

## 六、项目结构与文件功能

以下是当前工程目录结构及文件功能说明：

```
lcd/
├── Common/
│   ├── Common/
│   │   ├── FMC/                (新增目录) FMC 底层驱动
│   │   │   ├── fmc_driver.c    (新增) FMC NORSRAM Bank1 初始化、GPIO 复用配置、时序设置
│   │   │   └── fmc_driver.h    (新增) FMC 基地址、Bank 定义、命令/数据地址映射
│   │   ├── LVGL_LCD/           (新增目录) LVGL 显示驱动移植
│   │   │   ├── lvgl_port.c     (新增) LVGL 显示设备注册、flush_cb 实现、缓冲配置
│   │   │   └── lvgl_port.h     (新增) LVGL 初始化声明、屏幕参数宏（宽/高/颜色深度）
│   │   ├── SSD1963/            (新增目录) SSD1963 控制器驱动
│   │   │   ├── ssd1963.c       (新增) SSD1963 寄存器操作、初始化序列、窗口设置、GRAM 读写
│   │   │   └── ssd1963.h       (新增) SSD1963 命令宏、颜色格式定义、驱动 API 声明
│   │   ├── UI/                 (新增目录) UI 业务逻辑
│   │   │   ├── ui_main.c       (新增) 主界面创建、UI 组件初始化
│   │   │   └── ui_main.h       (新增) UI 接口声明
│   │   ├── Touch/              (新增目录) 电容触摸屏驱动
│   │   │   ├── touch_ctp.c     (新增) I2C2 触摸屏初始化、触摸坐标读取、手势识别
│   │   │   └── touch_ctp.h     (新增) 触摸接口声明、触摸事件结构体
│   │   ├── UART/               (新增目录) 模块间串口通信
│   │   │   ├── uart_module.c   (新增) USART1 初始化、数据收发、协议帧解析
│   │   │   └── uart_module.h   (新增) 串口接口声明、协议命令宏
│   │   ├── hardware.c          (修改) 在 Hardware() 中调用 LCD_Init()、LVGL_Init()、UART_Module_Init()
│   │   └── hardware.h          (修改) 声明 LCD_Init()、LVGL_Init()、UART_Module_Init() 等接口
│   └── Peripheral/             (不修改，直接调用现有库)
├── V5F/
│   ├── User/
│   │   ├── main.c              (不修改，保持 V5F 启动逻辑)
│   │   ├── ch32h417_it.c       (修改) 在 V5F 的 SysTick_Handler 中添加 lv_tick_inc(1)
│   │   ├── system_ch32h417.c   (不修改)
│   │   ├── ch32h417_conf.h     (不修改，已包含 ltdc.h/fmc.h/gpio.h 等)
│   │   └── lv_conf.h           (新增/修改) LVGL 全局配置文件，控制内存、颜色深度、功能裁剪
│   ├── LVGL/                   (已存在) LVGL v8.3 源码（src/ + examples/）
│   └── Ld/
│       └── Link_v5f.ld         (不修改，但需关注 DTCM 256KB 是否足够)
├── V3F/
│   └── User/
│       └── main.c              (不修改，负责唤醒 V5F)
└── LVGL_FMC_SSD1963_Plan.md    (本文件) 项目规划文档
```

### 各文件功能详解

| 文件路径 | 功能说明 |
|----------|----------|
| `Common/Common/hardware.c` | V5F 的业务入口。在 `Hardware()` 函数中依次调用 `LCD_Init()`、`LVGL_Init()`、`UART_Module_Init()` 和 `UI_Main_Create()`，然后进入 `while(1)` 主循环执行 `lv_timer_handler()` 和 `UART_Module_Poll()`。 |
| `Common/Common/hardware.h` | 声明 `LCD_Init()`、`LVGL_Init()`、`UART_Module_Init()`、`UI_Main_Create()` 等函数原型，供 `main.c` 和 `hardware.c` 包含。 |
| `Common/Common/FMC/fmc_driver.c` | 配置 FMC NORSRAM Bank1 为 16-bit 8080 模式；初始化 PD/PE 数据线为 FMC 复用推挽输出；初始化 PD4/PD5/PD7 为 FMC 控制线（NOE/NWE/NE1）；初始化 PB3 为 FMC 地址线 A1；设置 FMC 读写时序参数。 |
| `Common/Common/FMC/fmc_driver.h` | 定义 FMC 命令/数据地址映射宏（`SSD1963_CMD_ADDR`、`SSD1963_DATA_ADDR`），声明 `FMC_Driver_Init()`。 |
| `Common/Common/SSD1963/ssd1963.c` | 实现 SSD1963 的底层通信：通过 FMC 数据总线读写，利用地址偏移自动切换命令/数据；包含硬件复位（PA0）、初始化序列、窗口设置、纯色填充测试。 |
| `Common/Common/SSD1963/ssd1963.h` | 定义 SSD1963 寄存器命令（如 `SSD1963_SOFT_RESET`、`SSD1963_SET_PLL`），声明 `SSD1963_Init()`、`SSD1963_SetWindow()` 等 API。 |
| `Common/Common/LVGL_LCD/lvgl_port.c` | 注册 LVGL 显示设备 `lv_display_t`，配置双缓冲 `buf1`/`buf2`，实现 `my_flush_cb()` 将 LVGL 渲染好的像素通过 `ssd1963` 写入 GRAM。 |
| `Common/Common/LVGL_LCD/lvgl_port.h` | 声明 `LVGL_Init()`，定义屏幕分辨率 `LV_HOR_RES`（800）、`LV_VER_RES`（480）和颜色深度。 |
| `Common/Common/UI/ui_main.c` | 创建 LVGL 主界面、初始化 UI 组件（按钮、标签、滑块等），组织页面跳转逻辑。 |
| `Common/Common/UI/ui_main.h` | 声明 UI 创建函数，如 `UI_Main_Create()`。 |
| `Common/Common/UART/uart_module.c` | 初始化 USART1（PA9/PA10），实现与核心模块的数据收发；解析核心模块发来的键盘/鼠标/虚拟触摸数据，转换为 LVGL 输入事件；封装本地触摸事件上报给核心模块。 |
| `Common/Common/UART/uart_module.h` | 声明 UART 初始化、发送、接收接口；定义协议命令宏和数据结构。 |
| `Common/Common/Touch/touch_ctp.c` | 初始化 I2C2（PB10/PB11）和电容触摸屏控制器；读取触摸坐标和压力值；将触摸事件注入 LVGL 输入系统。 |
| `Common/Common/Touch/touch_ctp.h` | 声明触摸初始化、读取接口；定义触摸事件结构体。 |
| `V5F/User/lv_conf.h` | LVGL 核心配置文件。设置 `LV_COLOR_DEPTH 16`（RGB565）、`LV_MEM_SIZE`（64KB）、启用/禁用 widgets 和绘制功能，控制 Flash 占用。 |
| `V5F/User/ch32h417_it.c` | 修改 `SysTick_Handler`，增加 `lv_tick_inc(1)`，为 LVGL 提供 1ms 时基。 |
| `V5F/LVGL/` | 存放 LVGL v9.5 官方源码。编译时仅需 `src/` 目录，其他目录（docs/demos/tests/examples/scripts 等）可排除以节省编译时间。 |

## 七、风险与建议

1. **带宽与显存**：本方案采用 **RGB565** 格式，800×480 分辨率下每个像素通过 16-bit FMC 总线单次写入，带宽压力相比 RGB888 降低约 33%。V5F DTCM 中双缓冲仅需约 153KB，内存余量充足。

2. **SSD1963 时序与 DE 模式**：原理图标注屏幕采用 **DE 模式**（PA4 拉高），SSD1963 初始化时必须配置为 DE 模式输出，且 HFP/HBP/VFP/VBP/PCLK 必须根据 LCD 屏 datasheet 精确设置，否则会出现花屏、抖动或黑屏。同时需确保 SSD1963 的 `Interface Pixel Format` 配置为 16-bit，使其正确将 RGB565 数据转换为 RGB888 输出。

3. **引脚冲突**：FMC_D0~D15 占用 PD/PE 大量引脚，PD4/PD5/PD7 用于 FMC 控制线，PB3 用于 FMC_A1，PA9/PA10 用于 USART1（波特率 115200）与核心模块通信，PB10/PB11 用于触摸 I2C2，需确认各功能引脚无复用冲突。

4. **代码体积**：LVGL v9.5 已完成初步源码裁剪（删除了 `src/drivers/`、`src/debugging/`、`src/others/`，精简了 `src/font/` 和 `src/libs/`），V5F 可用 CodeFlash 960KB，Flash 余量充足，但仍建议通过 `lv_conf.h` 关闭不必要的 widgets、themes、动画效果以控制编译时间和内存占用。当前保留的 `src/libs/` 目录：`qrcode/`（二维码）、`barcode/`（条形码）、`fsdrv/`（文件系统抽象）、`bmp/`（BMP 图片）、`tiny_ttf/`（轻量 TTF 字体）。

5. **输入系统复杂度**：本地 I2C 触摸和远程 USART1 输入可能同时存在，需设计合理的输入事件优先级或叠加策略，避免冲突。LVGL v9.5 的输入系统支持多设备注册，可通过 `lv_indev_t` 分别注册触摸设备和指针设备。

## 八、实施步骤概要

1. 配置 FMC GPIO 和时序，验证 8080 并口读写 SSD1963 寄存器
2. 完成 SSD1963 初始化，点亮屏幕（可先纯色填充验证）
3. 配置 LVGL v9.5：将 `V5F/LVGL/src/` 加入编译路径（已裁剪：删除 drivers/debugging/others，精简 font/libs），从 `lv_conf_template.h` 创建 `lv_conf.h`
4. 实现 LVGL v9.5 的 `flush_cb` 和 `tick` 接口
5. 实现 USART1 与核心模块通信，解析输入数据并注入 LVGL 输入系统
6. 实现 I2C2 电容触摸屏驱动，读取触摸坐标并注入 LVGL 输入系统
7. 创建简单 UI 验证刷新和触摸（本地 I2C 触摸 + 核心模块虚拟触摸）
8. 优化帧率和内存占用
