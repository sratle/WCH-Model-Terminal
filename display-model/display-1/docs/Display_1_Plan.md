# MiniUI + FMC8080 + SSD1963 驱动方案

## 一、整体架构

- **运行核**：V5F（主频 400MHz，负责 UI 渲染）
- **显示通信**：FMC NORSRAM Bank1，16-bit 8080 并口驱动 SSD1963
- **模块间通信**：USART1（PA9-TX / PA10-RX）与核心模块交互，接收键盘/鼠标/虚拟触摸等输入数据
- **显存策略**：SSD1963 内部集成 1215KB GRAM 作为物理显存；MiniUI 采用**脏矩形局部刷新**，无需全屏帧缓冲（800×480×2=768KB 超出 512KB SRAM）
- **UI 框架**：自主实现 MiniUI（轻量专用 UI 库），替代 LVGL v9.5
  - 分层架构：App Layer → Page Manager → Widget Kit → Render Engine → Display Driver
  - 页面栈深度：16 层，支持 push/pop/switch 和回退导航
  - 脏区域管理：最多 8 个 dirty region，每帧只重绘变化区域
  - 动画系统：线性/ease-out/ease-in 插值，支持位置/透明度/颜色动画
- **项目结构**：驱动层按功能模块化，放置在 `Common/Common/` 下的 `FMC/`、`SSD1963/`、`MiniUI/`、`UI/`、`Games/`、`Apps/`、`UART/`、`Touch/` 目录中
- **输入方式**：
  - **本地输入**：电容触摸屏通过 I2C2（PB10/PB11）直接读取触摸坐标，支持滑动（上下左右）和点击
  - **远程输入**：核心模块通过 USART1 发送键盘/鼠标数据，LCD 模块解析后映射为统一 UI 事件
  - **统一输入抽象**：触摸/鼠标/键盘三种输入源映射到同一套 `ui_event_t` 事件类型（PRESS/RELEASE/DRAG/SWIPE_*/KEY_*）

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

## 四、MiniUI 自主 UI 库

### 4.1 架构设计

MiniUI 采用分层架构，完全替代 LVGL：

```
App Layer (Apps/, Games/)
    ↓
Page Manager (miniui_page.c) — 页面栈（16层）、脏区域追踪、页面切换
    ↓
Widget Kit (miniui_widget.c) — Label, Button, Slider, Switch, Progress, Card, ListItem
    ↓
Render Engine (miniui_render.c) — 像素/线/矩形/圆角矩形/圆形/文本绘制
    ↓
Display Driver (SSD1963/) — 直接操作 GRAM，脏矩形局部刷新
```

### 4.2 核心模块

| 文件 | 功能 |
|------|------|
| `miniui_types.h` | 基础类型：颜色、点、矩形、字体、事件、widget/page/anim 前向声明 |
| `miniui_color.h` | RGB565 宏、马卡龙配色常量、颜色混合/ darken/lighten 工具函数 |
| `miniui_render.c/h` | 渲染引擎：基于 SSD1963 GRAM 的像素/线/矩形/圆角矩形/圆形/文本绘制 |
| `miniui_font.c/h` | 字体引擎：1bpp 位图字体渲染，支持 Montserrat 12/16 |
| `miniui_widget.c/h` | Widget 系统：基类 + Label, Button, Slider, Switch, **Progress**, Card, ListItem |
| `miniui_page.c/h` | 页面管理器：页面栈（16层深度）、push/pop/switch、脏区域追踪（最多8个） |
| `miniui_input.c/h` | 输入系统：触摸手势识别（滑动/点击）、鼠标、键盘统一映射到 `ui_event_t` |
| `miniui_anim.c/h` | 动画系统：线性/ease-out/ease-in 插值，支持位置/透明度/颜色动画 |
| `miniui.h` | 总头文件，统一包含所有 MiniUI 模块 |
| `ui_system.c` | UI 系统入口：`UI_Init()`、`UI_Tick()`、`UI_FullRefresh()` |

### 4.3 显存策略：脏矩形局部刷新

由于 800×480 RGB565 全屏帧缓冲需要 768KB，超出 V5F 512KB SRAM，MiniUI 采用脏矩形策略：

- 维护最多 8 个 dirty region（`ui_rect_t` 数组）
- Widget 状态变化时调用 `ui_page_invalidate(rect)` 标记脏区域
- 每帧 `ui_page_draw()` 只重绘脏区域内的 widget，直接写入 SSD1963 GRAM
- 页面切换时全屏刷新一次

```c
void ui_page_invalidate(const ui_rect_t *rect)
{
    // 将 rect 合并到 dirty_regions 数组中
    // 如果 dirty region 数量超过 8 个，合并为全屏刷新
}

void ui_page_draw(void)
{
    for (每个 dirty region) {
        SSD1963_SetWindow(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1);
        // 遍历该区域内的所有 widget，调用 widget->draw()
    }
    // 清空 dirty regions
}
```

### 4.4 统一输入系统

三种输入源映射到同一套 `ui_event_t` 事件：

```c
typedef enum {
    UI_EVENT_NONE,
    UI_EVENT_PRESS,      // 按下（触摸/鼠标左键/键盘 OK）
    UI_EVENT_RELEASE,    // 释放
    UI_EVENT_DRAG,       // 拖动
    UI_EVENT_SWIPE_UP,   // 向上滑动
    UI_EVENT_SWIPE_DOWN, // 向下滑动
    UI_EVENT_SWIPE_LEFT, // 向左滑动
    UI_EVENT_SWIPE_RIGHT,// 向右滑动
    UI_EVENT_KEY_UP,     // 键盘上
    UI_EVENT_KEY_DOWN,   // 键盘下
    UI_EVENT_KEY_LEFT,   // 键盘左
    UI_EVENT_KEY_RIGHT,  // 键盘右
    UI_EVENT_KEY_OK,     // 键盘确认
    UI_EVENT_KEY_BACK,   // 键盘返回
} ui_event_type_t;
```

- **触摸输入**：本地 I2C2 读取，手势识别（滑动阈值 20px，点击阈值 10px）
- **鼠标输入**：UART 接收，映射为 PRESS/RELEASE/DRAG
- **键盘输入**：UART 接收，映射为 KEY_UP/DOWN/LEFT/RIGHT/OK/BACK

### 4.5 时基与主循环

在 `V5F/Common/Common/hardware.c` 的 `Hardware()` 中：

```c
void Hardware(void)
{
    LCD_Init();           // FMC + SSD1963 + 屏幕方向/模式 GPIO
    UI_Init();            // MiniUI 初始化 + 创建主界面
    UART_Module_Init();   // USART1 与核心模块通信

    while (1) {
        UI_Tick();          // 处理输入事件 + 动画更新 + 脏矩形刷新
        UART_Module_Poll(); // 轮询接收核心模块数据
        Delay_Ms(33);       // ~30fps
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
│   │   ├── MiniUI/             (新增目录) 自主 UI 库核心
│   │   │   ├── miniui_types.h  (新增) 基础类型、颜色、字体、事件定义
│   │   │   ├── miniui_color.h  (新增) RGB565 宏、马卡龙配色、颜色工具函数
│   │   │   ├── miniui_render.c/h (新增) 渲染引擎：像素/线/矩形/圆角矩形/圆形/文本
│   │   │   ├── miniui_font.c/h   (新增) 字体引擎：1bpp 位图字体渲染
│   │   │   ├── miniui_widget.c/h (新增) Widget 系统：基类 + Label/Button/Slider/Switch/Progress/Card/ListItem
│   │   │   ├── miniui_page.c/h   (新增) 页面管理器：页面栈（16层）、脏区域追踪
│   │   │   ├── miniui_input.c/h  (新增) 输入系统：触摸手势/鼠标/键盘统一映射
│   │   │   ├── miniui_anim.c/h   (新增) 动画系统：线性/ease-out/ease-in 插值
│   │   │   ├── miniui.h          (新增) 总头文件
│   │   │   ├── ui_system.c       (新增) UI 系统入口：UI_Init()、UI_Tick()、UI_FullRefresh()
│   │   │   └── font/             (新增目录) 位图字体数据
│   │   │       ├── font_montserrat_12.c/h (新增) Montserrat 12pt 字体
│   │   │       └── font_montserrat_16.c/h (新增) Montserrat 16pt 字体
│   │   ├── SSD1963/            (新增目录) SSD1963 控制器驱动
│   │   │   ├── ssd1963.c       (新增) SSD1963 寄存器操作、初始化序列、窗口设置、GRAM 读写
│   │   │   └── ssd1963.h       (新增) SSD1963 命令宏、颜色格式定义、驱动 API 声明
│   │   ├── UI/                 (新增目录) UI 框架与主界面（每个 APP/GAME 为独立页面）
│   │   │   ├── ui_main.c/h     (新增) 主界面框架：侧边栏导航(Home/Apps/Games/Models/Settings)、页面切换管理
│   │   │   ├── ui_home.c/h     (新增) 首页：日期时间、模块状态、系统状态栏
│   │   │   ├── ui_apps.c/h     (新增) 应用界面：15个应用3x4翻页网格、应用启动器
│   │   │   ├── ui_models.c/h   (新增) 模块界面：Tab 视图、模块状态卡片
│   │   │   ├── ui_settings.c/h (新增) 设置界面：设置项列表、控件绑定
│   │   │   ├── ui_games.c/h    (新增) 游戏界面：4个游戏单页网格、游戏启动器
│   │   │   └── ui_app_common.c/h (新增) 全屏应用/游戏通用模板：返回按钮+标题栏+ESC退出
│   │   ├── Games/              (新增目录) 本地小游戏（每个游戏为独立页面）
│   │   │   ├── game_tetris.c/h (新增) 俄罗斯方块游戏
│   │   │   ├── game_2048.c/h   (新增) 2048 游戏
│   │   │   ├── game_snake.c/h  (新增) 贪吃蛇游戏
│   │   │   └── game_breakout.c/h (新增) 打砖块游戏
│   │   ├── Apps/               (新增目录) 系统应用UI（每个应用为独立页面，通过UART与核心模块交互）
│   │   │   ├── app_music.c/h       (新增) 音乐播放：播放控制、进度条、播放列表
│   │   │   ├── app_file.c/h        (新增) 文件管理：目录浏览、文件列表、操作菜单
│   │   │   ├── app_editor.c/h      (新增) 文档编辑：文本输入、简单排版、保存
│   │   │   ├── app_images.c/h      (新增) 图片查看：缩略图网格、全屏查看、缩放
│   │   │   ├── app_usb.c/h         (新增) USB连接：连接状态、传输进度、设备信息
│   │   │   ├── app_power.c/h       (新增) 电源管理：电池状态、功耗曲线、省电设置
│   │   │   ├── app_bt.c/h          (新增) 蓝牙管理：设备列表、配对、文件传输
│   │   │   ├── app_nfc.c/h         (新增) NFC功能：标签读写、NFC状态
│   │   │   ├── app_fingerprint.c/h (新增) 指纹识别：录入、验证、管理指纹
│   │   │   ├── app_health.c/h      (新增) 健康监测：心率、血氧、历史数据图表
│   │   │   ├── app_subdisplay.c/h  (新增) 副屏管理：副屏状态、显示内容设置
│   │   │   ├── app_lights.c/h      (新增) 灯效控制：颜色选择、模式切换、亮度调节
│   │   │   ├── app_irrange.c/h     (新增) 红外测距：实时距离显示、历史数据
│   │   │   ├── app_ebook.c/h       (新增) 电子书阅读：翻页、书签、目录、字体设置
│   │   │   └── app_emusic.c/h      (新增) 电子音乐/合成器：键盘、音色选择、录制
│   │   ├── Touch/              (新增目录) 电容触摸屏驱动
│   │   │   ├── touch_ctp.c     (新增) I2C2 触摸屏初始化、触摸坐标读取、手势识别
│   │   │   └── touch_ctp.h     (新增) 触摸接口声明、触摸事件结构体
│   │   ├── UART/               (新增目录) 模块间串口通信
│   │   │   ├── uart_module.c   (新增) USART1 初始化、数据收发、协议帧解析
│   │   │   └── uart_module.h   (新增) 串口接口声明、协议命令宏
│   │   ├── hardware.c          (修改) 在 Hardware() 中调用 LCD_Init()、UI_Init()、UART_Module_Init()
│   │   └── hardware.h          (修改) 声明 LCD_Init()、UI_Init()、UART_Module_Init() 等接口
│   └── Peripheral/             (不修改，直接调用现有库)
├── V5F/
│   ├── User/
│   │   ├── main.c              (不修改，保持 V5F 启动逻辑)
│   │   ├── ch32h417_it.c       (不修改)
│   │   ├── system_ch32h417.c   (不修改)
│   │   └── ch32h417_conf.h     (不修改，已包含 ltdc.h/fmc.h/gpio.h 等)
│   └── Ld/
│       └── Link_v5f.ld         (不修改，SRAM 512KB 充足)
├── V3F/
│   └── User/
│       └── main.c              (不修改，负责唤醒 V5F)
└── LVGL_FMC_SSD1963_Plan.md    (本文件) 项目规划文档
```

### 各文件功能详解

| 文件路径 | 功能说明 |
|----------|----------|
| `Common/Common/hardware.c` | V5F 的业务入口。在 `Hardware()` 函数中依次调用 `LCD_Init()`、`UI_Init()`、`UART_Module_Init()`，然后进入 `while(1)` 主循环执行 `UI_Tick()` 和 `UART_Module_Poll()`。 |
| `Common/Common/hardware.h` | 声明 `LCD_Init()`、`UI_Init()`、`UART_Module_Init()` 等函数原型，供 `main.c` 和 `hardware.c` 包含。 |
| `Common/Common/FMC/fmc_driver.c` | 配置 FMC NORSRAM Bank1 为 16-bit 8080 模式；初始化 PD/PE 数据线为 FMC 复用推挽输出；初始化 PD4/PD5/PD7 为 FMC 控制线（NOE/NWE/NE1）；初始化 PB3 为 FMC 地址线 A1；设置 FMC 读写时序参数。 |
| `Common/Common/FMC/fmc_driver.h` | 定义 FMC 命令/数据地址映射宏（`SSD1963_CMD_ADDR`、`SSD1963_DATA_ADDR`），声明 `FMC_Driver_Init()`。 |
| `Common/Common/SSD1963/ssd1963.c` | 实现 SSD1963 的底层通信：通过 FMC 数据总线读写，利用地址偏移自动切换命令/数据；包含硬件复位（PA0）、初始化序列、窗口设置、纯色填充测试。 |
| `Common/Common/SSD1963/ssd1963.h` | 定义 SSD1963 寄存器命令（如 `SSD1963_SOFT_RESET`、`SSD1963_SET_PLL`），声明 `SSD1963_Init()`、`SSD1963_SetWindow()` 等 API。 |
| `Common/Common/MiniUI/miniui_types.h` | 基础类型定义：颜色、点、矩形、字体结构体、事件枚举、widget/page/anim 前向声明和标志位。 |
| `Common/Common/MiniUI/miniui_color.h` | RGB565 颜色宏、马卡龙配色常量、颜色混合/变暗/变亮工具函数。 |
| `Common/Common/MiniUI/miniui_render.c/h` | 渲染引擎：像素点绘制、线、矩形填充、圆角矩形、圆形、文本渲染；基于 SSD1963 GRAM 直接写入。 |
| `Common/Common/MiniUI/miniui_font.c/h` | 字体引擎：1bpp 位图字体渲染，支持 Montserrat 12/16 字体。 |
| `Common/Common/MiniUI/miniui_widget.c/h` | Widget 系统：基类（位置、大小、状态、回调）+ Label、Button、Slider、Switch、**Progress**、Card、ListItem。 |
| `Common/Common/MiniUI/miniui_page.c/h` | 页面管理器：页面栈（16层深度）、push/pop/switch 操作、脏区域追踪（最多8个）、页面绘制。 |
| `Common/Common/MiniUI/miniui_input.c/h` | 输入系统：触摸手势识别（滑动/点击）、鼠标事件、键盘事件，统一映射为 `ui_event_t`。 |
| `Common/Common/MiniUI/miniui_anim.c/h` | 动画系统：线性/ease-out/ease-in 插值，支持位置/透明度/颜色动画。 |
| `Common/Common/MiniUI/miniui.h` | 总头文件，统一包含所有 MiniUI 模块头文件。 |
| `Common/Common/MiniUI/ui_system.c` | UI 系统入口：`UI_Init()` 初始化所有模块并切换到首页；`UI_Tick()` 处理事件+动画+刷新；`UI_FullRefresh()` 全屏刷新。 |
| `Common/Common/UI/ui_main.c/h` | 主界面框架：侧边栏创建、页面切换管理、全局样式定义、主题初始化。 |
| `Common/Common/UI/ui_home.c/h` | 首页：日期时间、模块连接状态卡片、系统状态栏（FPS/内存/CPU）。 |
| `Common/Common/UI/ui_software.c/h` | 应用界面：应用网格布局、系统应用与本地游戏分类显示、应用启动器。 |
| `Common/Common/UI/ui_models.c/h` | 模块界面：Tab 视图（Display/Keyboard/Power/Core/BT）、模块状态卡片。 |
| `Common/Common/UI/ui_settings.c/h` | 设置界面：设置项列表（显示/电源/系统）、控件绑定、背光调节。 |
| `Common/Common/UI/ui_games.c/h` | 游戏框架：游戏启动/退出、全屏切换、分数管理、暂停菜单。 |
| `Common/Common/Games/game_tetris.c/h` | 俄罗斯方块：游戏逻辑、方块渲染、碰撞检测、行消除、分数计算。 |
| `Common/Common/Games/game_2048.c/h` | 2048 游戏：网格逻辑、数字方块渲染、滑动合并、分数计算。 |
| `Common/Common/Games/game_snake.c/h` | 贪吃蛇：蛇身移动、食物生成、碰撞检测、分数计算。 |
| `Common/Common/Games/game_breakout.c/h` | 打砖块：挡板控制、球体物理、砖块消除、关卡管理。 |
| `Common/Common/Apps/app_music.c/h` | 音乐播放UI：播放/暂停/切歌控制、进度条、音量调节、播放列表。 |
| `Common/Common/Apps/app_file.c/h` | 文件管理UI：目录浏览、文件列表、新建/删除/重命名操作。 |
| `Common/Common/Apps/app_editor.c/h` | 文档编辑UI：文本输入区、简单排版、保存/打开。 |
| `Common/Common/Apps/app_images.c/h` | 图片查看UI：缩略图网格、全屏查看、左右切换、缩放。 |
| `Common/Common/Apps/app_usb.c/h` | USB连接UI：连接状态、传输进度、已连接设备信息。 |
| `Common/Common/Apps/app_power.c/h` | 电源管理UI：电池状态、功耗曲线、省电模式开关。 |
| `Common/Common/Apps/app_bt.c/h` | 蓝牙管理UI：设备列表、配对/连接、文件传输进度。 |
| `Common/Common/Apps/app_nfc.c/h` | NFC功能UI：标签读写界面、NFC状态、历史记录。 |
| `Common/Common/Apps/app_fingerprint.c/h` | 指纹识别UI：指纹录入、验证、管理已录入指纹。 |
| `Common/Common/Apps/app_health.c/h` | 健康监测UI：心率/血氧实时显示、历史数据图表、趋势分析。 |
| `Common/Common/Apps/app_subdisplay.c/h` | 副屏管理UI：副屏状态、显示内容设置、分辨率配置。 |
| `Common/Common/Apps/app_lights.c/h` | 灯效控制UI：颜色选择器（RGB）、模式切换、亮度/速度调节。 |
| `Common/Common/Apps/app_irrange.c/h` | 红外测距UI：实时距离数字显示、历史数据曲线、单位切换。 |
| `Common/Common/Apps/app_ebook.c/h` | 电子书阅读UI：文本渲染、翻页动画、书签、目录跳转、字体设置。 |
| `Common/Common/Apps/app_emusic.c/h` | 电子音乐/合成器UI：虚拟键盘、音色选择、录制/播放控制。 |
| `Common/Common/UART/uart_module.c` | 初始化 USART1（PA9/PA10），实现与核心模块的数据收发；解析核心模块发来的键盘/鼠标数据，转换为统一 UI 输入事件；封装本地触摸事件上报给核心模块。 |
| `Common/Common/UART/uart_module.h` | 声明 UART 初始化、发送、接收接口；定义协议命令宏和数据结构。 |
| `Common/Common/Touch/touch_ctp.c` | 初始化 I2C2（PB10/PB11）和电容触摸屏控制器；读取触摸坐标和压力值；将触摸事件注入 MiniUI 输入系统（滑动/点击手势识别）。 |
| `Common/Common/Touch/touch_ctp.h` | 声明触摸初始化、读取接口；定义触摸事件结构体。 |

## 七、风险与建议

1. **显存与刷新策略**：本方案采用 **RGB565** 格式，800×480 分辨率下全屏帧缓冲需要 768KB，超出 V5F 512KB SRAM。MiniUI 采用**脏矩形局部刷新**策略，无需全屏帧缓冲，每帧只重绘变化的 widget 区域，直接写入 SSD1963 GRAM。页面切换时执行一次全屏刷新。

2. **SSD1963 时序与 DE 模式**：原理图标注屏幕采用 **DE 模式**（PA4 拉高），SSD1963 初始化时必须配置为 DE 模式输出，且 HFP/HBP/VFP/VBP/PCLK 必须根据 LCD 屏 datasheet 精确设置，否则会出现花屏、抖动或黑屏。同时需确保 SSD1963 的 `Interface Pixel Format` 配置为 16-bit，使其正确将 RGB565 数据转换为 RGB888 输出。

3. **引脚冲突**：FMC_D0~D15 占用 PD/PE 大量引脚，PD4/PD5/PD7 用于 FMC 控制线，PB3 用于 FMC_A1，PA9/PA10 用于 USART1（波特率 115200）与核心模块通信，PB10/PB11 用于触摸 I2C2，需确认各功能引脚无复用冲突。

4. **代码体积**：MiniUI 为轻量专用 UI 库，代码量远小于 LVGL v9.5，Flash 占用预计 < 100KB（含字体位图）。V5F 可用 CodeFlash 960KB，余量非常充足。

5. **输入系统复杂度**：本地 I2C 触摸和远程 USART1 输入可能同时存在。MiniUI 输入系统将三种输入源统一映射为 `ui_event_t` 事件队列，按 FIFO 顺序处理，避免冲突。触摸支持滑动（上下左右）和点击，鼠标映射为 PRESS/RELEASE/DRAG，键盘映射为方向键和 OK/BACK。

## 八、UI 界面设计

### 8.1 设计原则

- **配色方案**：马卡龙色系，整体视觉舒适柔和
  - 主背景：`#F8F6F3`（暖白/米白）
  - 侧边栏背景：`#FFFFFF`（纯白）
  - 选中项高亮：`#B8E0D2`（薄荷绿）
  - 主色调：`#7EC8C8`（薄荷蓝绿）
  - 强调色：`#FFB6C1`（浅粉）
  - 文字主色：`#4A4A4A`（深灰）
  - 文字次色：`#9E9E9E`（中灰）
  - 卡片背景：`#FFFFFF`（纯白）+ 浅阴影
  - 成功/在线：`#90EE90`（浅绿）
  - 警告/离线：`#FFB6C1`（浅粉）

- **布局结构**：左侧固定侧边栏（约 140px 宽）+ 右侧内容区（约 660px 宽）
- **字体**：Montserrat 16 为主标题，Montserrat 12 为正文和标签
- **圆角风格**：所有按钮、卡片、图标按钮使用 12px 圆角
- **动画**：页面切换使用 200ms 淡入淡出，按钮按压缩放 0.95

### 8.2 侧边栏（Sidebar）

固定宽度 140px，纯白背景，右侧 1px 浅灰分割线。

顶部显示 "Menu" 标题（Montserrat 16，深灰，居中）。

下方 4 个菜单项垂直排列，每个项高度 60px：

| 菜单项 | 图标 | 说明 |
|--------|------|------|
| Main | `LV_SYMBOL_HOME` | 首页，展示系统状态 |
| Software | `LV_SYMBOL_APPS` | 应用中心，启动各类应用和游戏 |
| Models | `LV_SYMBOL_MODULES` | 模块状态，查看各硬件模块连接情况 |
| Options | `LV_SYMBOL_SETTINGS` | 系统设置 |

选中项：背景 `#B8E0D2`（薄荷绿），文字 `#4A4A4A`，左侧 4px 宽 `#7EC8C8` 竖条指示器。
未选中项：背景透明，文字 `#9E9E9E`。

点击菜单项切换右侧内容区页面。

### 8.3 首页（Main）

右侧内容区顶部区域（约 200px 高）：
- **左上角**：日期标签（"2024/04/22"，Montserrat 16，深灰）
- **左中**：时间标签（"11:25:50 AM"，Montserrat 24，主色调 `#7EC8C8`）
- **右上角**：系统状态指示（网络连接、电池电量图标）

中部区域（约 200px 高）：
- **模块连接状态卡片**：横向排列 3~4 个小卡片
  - 核心模块（Core）：图标 + "Online"/"Offline" 状态
  - 蓝牙（BT）：图标 + 状态
  - WiFi：图标 + 状态
  - 触摸屏（Touch）：图标 + 状态
  - 每个卡片：圆角矩形，白色背景，浅阴影，状态用彩色圆点指示

底部区域（约 80px 高）：
- **系统状态栏**：
  - 左下角：内存使用（"5.6KB used(2%)"）、碎片率（"1% frag"）
  - 右下角：FPS（"33 FPS"）、CPU 占用（"4% CPU"）
  - 底部中央：WCH 沁恒 Logo（小尺寸，灰色）

### 8.4 应用界面（Software）

右侧内容区为**可滚动网格布局**，顶部显示 "Software" 标题。

应用分为两类：**系统应用**（与核心模块交互）和**本地游戏**（纯本地运行）。

#### 系统应用（通过 UART 与核心模块交互）

| 应用 | 图标 | 功能说明 |
|------|------|----------|
| Music | `LV_SYMBOL_AUDIO` | 音乐播放控制（播放/暂停/切歌，核心模块解码） |
| File | `LV_SYMBOL_FILE` | 文件管理（浏览核心模块 TF 卡文件） |
| Editor | `LV_SYMBOL_EDIT` | 文档编辑（简单文本编辑器） |
| Images | `LV_SYMBOL_IMAGE` | 图片查看（浏览核心模块存储的图片） |
| USB | `LV_SYMBOL_USB` | USB 连接管理 |
| Power | `LV_SYMBOL_CHARGE` | 电源管理（查看电池状态、功耗） |
| BT | `LV_SYMBOL_BLUETOOTH` | 蓝牙管理（配对、连接、传输） |
| NFC | `LV_SYMBOL_NFC` | NFC 功能（读写标签） |
| Fingerprint | `LV_SYMBOL_EYE_OPEN` | 指纹识别管理 |
| Health | `LV_SYMBOL_HEART` | 健康监测（显示心率、血氧等数据） |
| Sub-display | `LV_SYMBOL_MONITOR` | 副屏管理 |
| Lights | `LV_SYMBOL_BULB` | 灯效控制（RGB 灯带控制） |
| IR Range | `LV_SYMBOL_GPS` | 红外测距（显示距离数据） |
| E-book | `LV_SYMBOL_BOOK` | 电子书阅读器 |
| E-music | `LV_SYMBOL_MUSIC` | 电子音乐/合成器 |

每个应用显示为**图标按钮**（80×80px，圆角 16px，主色调背景，白色图标）+ 下方文字标签。
点击应用后，通过 UART 发送指令给核心模块，核心模块执行相应功能。

#### 本地游戏（纯本地运行，不依赖核心模块）

| 游戏 | 图标 | 玩法说明 |
|------|------|----------|
| Tetris | `LV_SYMBOL_PLAY` | 俄罗斯方块，触屏方向键控制 |
| 2048 | `LV_SYMBOL_PLAY` | 2048 数字合成，滑动手势控制 |
| Snake | `LV_SYMBOL_PLAY` | 贪吃蛇，触屏方向键控制 |
| Breakout | `LV_SYMBOL_PLAY` | 打砖块，触屏滑动控制挡板 |

游戏界面为全屏或接近全屏，通过内容区顶部返回按钮回到应用列表。

### 8.5 模块界面（Models）

右侧内容区顶部显示 "Models" 标题，下方为**Tab 视图**（`lv_tabview`）：

| Tab 页 | 内容 |
|--------|------|
| Display | LCD 模块状态（分辨率、刷新率、背光亮度、SSD1963 温度） |
| Keyboard | 键盘模块状态（连接状态、电量、按键映射） |
| Power | 电源模块状态（电池电压/电流/容量、充电状态） |
| Core | 核心模块状态（CPU 负载、内存使用、TF 卡容量） |
| BT/WiFi | 无线模块状态（信号强度、连接设备、MAC 地址） |

每个 Tab 页内用**卡片式布局**显示模块信息：
- 模块名称（大标题）
- 连接状态（在线/离线，彩色圆点）
- 关键参数（标签 + 数值）
- 如果模块离线，显示 "Offline" 和重试连接按钮

### 8.6 设置界面（Options）

右侧内容区顶部显示 "Options" 标题，下方为**垂直滚动列表**（`lv_list` 或自定义布局）。

设置项分组：

#### 显示设置
| 设置项 | 控件类型 | 说明 |
|--------|----------|------|
| Backlight | Slider | 背光亮度 10%~100%，调节 SSD1963 PWM 占空比 |
| Auto Screen-off | Switch | 自动息屏开关 |
| Screen Timeout | Dropdown | 息屏时间：30s / 1min / 2min / 5min |
| Screen Rotation | Dropdown | 屏幕旋转：0° / 90° / 180° / 270°（通过 PA5/PA6 控制） |

#### 电源设置
| 设置项 | 控件类型 | 说明 |
|--------|----------|------|
| Power Reduce | Switch | 省电模式开关 |
| Sleep Mode | Switch | 休眠模式开关 |

#### 系统设置
| 设置项 | 控件类型 | 说明 |
|--------|----------|------|
| Sound Volume | Slider | 音量 0%~100%（通过 UART 控制核心模块音频） |
| Debug Info | Checkbox | 是否显示调试信息（FPS、内存等） |
| Device Name | Dropdown | 设备名称：ELAB / WCH / Custom |
| Factory Reset | Button | 恢复出厂设置（红色警告按钮，需确认） |
| About | Button | 关于页面（显示固件版本、LVGL 版本、硬件信息） |

每个设置项为一行：左侧文字标签，右侧控件，行高 50px，底部 1px 浅灰分割线。

### 8.7 游戏界面设计

#### 俄罗斯方块（Tetris）

全屏游戏界面（隐藏侧边栏，或侧边栏收缩为图标栏）：
- **左侧**：游戏区域（10×20 网格，每个格子约 30×30px，总约 300×600px）
- **右侧信息区**：
  - 标题 "Tetris!"（大号字体）
  - Score / Best 分数显示
  - Next 预览区（4×4 网格，显示下一个方块）
  - Stage 等级显示
- **底部控制区**：方向键（上=旋转，左/右=移动，下=加速下落）
- **左上角**：Start / Pause 按钮
- **右上角**：关闭按钮（返回应用列表）

#### 2048

- **左侧**：4×4 游戏网格（每个格子约 120×120px，圆角 12px）
- **右侧**：
  - 标题 "2048"
  - New Game 按钮
  - Score / Best 分数显示
  - 方向键（上下左右滑动或点击方向键）
- 方块颜色根据数值变化（2=浅灰，4=浅黄，8=橙色，16=红色，... 2048=金色）

### 8.8 交互与动画

- **页面切换**：点击侧边栏菜单项，右侧内容区 200ms 淡入淡出切换
- **按钮按压**：所有可点击元素按下时缩放至 0.95，释放恢复
- **卡片悬停**（如果有鼠标输入）：卡片阴影加深，轻微上移 2px
- **状态更新**：模块状态变化时，状态圆点平滑过渡颜色（300ms）
- **状态指示点击**：点击状态图标，弹出小菜单查看详细信息（设备信息 / 网络状态 / 固件版本）
- **游戏退出**：游戏中点击右上角关闭按钮，弹出确认对话框 "Exit game?"

### 8.9 UI 文件规划

| 文件 | 功能 |
|------|------|
| `ui_main.c/h` | 主界面框架：侧边栏创建、页面切换管理、全局样式定义 |
| `ui_home.c/h` | 首页：日期时间、模块状态、系统状态栏 |
| `ui_software.c/h` | 应用界面：应用网格、分类显示、启动应用 |
| `ui_models.c/h` | 模块界面：Tab 视图、模块状态卡片 |
| `ui_settings.c/h` | 设置界面：设置项列表、控件绑定 |
| `ui_games.c/h` | 游戏框架：游戏启动/退出、全屏切换、分数管理 |
| `game_tetris.c/h` | 俄罗斯方块游戏逻辑和渲染 |
| `game_2048.c/h` | 2048 游戏逻辑和渲染 |
| `game_snake.c/h` | 贪吃蛇游戏逻辑和渲染 |
| `game_breakout.c/h` | 打砖块游戏逻辑和渲染 |

## 九、实施步骤概要

### Phase 1: MiniUI 核心库
1. 创建目录结构：`MiniUI/`、`UI/`、`Apps/`、`Games/`
2. 实现 `miniui_types.h`：基础类型、颜色、字体、事件、widget/page/anim 前向声明
3. 实现 `miniui_color.h`：RGB565 宏、马卡龙配色、颜色工具函数
4. 实现 `miniui_render.c/h`：渲染引擎（像素/线/矩形/圆角矩形/圆形/文本）
5. 实现 `miniui_font.c/h` + 占位符字体：1bpp 位图字体渲染

### Phase 2: Widget 系统
6. 实现 `miniui_widget.c/h`：Widget 基类 + Label、Button、Slider、Switch、**Progress**、Card、ListItem

### Phase 3: 页面与输入系统
7. 实现 `miniui_page.c/h`：页面栈（16层）、脏区域追踪（最多8个）、页面切换
8. 实现 `miniui_input.c/h`：触摸手势识别（滑动/点击）、鼠标、键盘统一事件映射

### Phase 4: 动画与系统入口
9. 实现 `miniui_anim.c/h`：线性/ease-out/ease-in 插值动画
10. 实现 `miniui.h` 总头文件和 `ui_system.c`：UI_Init、UI_Tick、UI_FullRefresh

### Phase 5: UI 框架与首页
11. 实现 `ui_main.c/h`：侧边栏导航框架（Main/Software/Models/Options）
12. 实现 `ui_home.c/h`：首页（日期、时间、模块状态卡片）

### Phase 6: 工程配置更新
13. 更新 `hardware.c/h`：替换 `LVGL_Init()` 为 `UI_Init()`
14. 更新 V5F 和 V3F 的 `wvproj`：移除 LVGL 路径，添加 MiniUI/UI/Apps/Games 路径

### Phase 7: 编译验证
15. 编译验证并修复错误

### Phase 8: 应用与游戏页面（后续迭代）
16. 实现应用界面 `ui_software.c/h`：应用网格、分类显示
17. 实现模块界面 `ui_models.c/h`：Tab 视图、模块状态卡片
18. 实现设置界面 `ui_settings.c/h`：设置项列表、控件绑定
19. 实现各系统应用页面（Apps/ 目录，每个应用独立页面）
20. 实现各游戏页面（Games/ 目录，每个游戏独立页面）
