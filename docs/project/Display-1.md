# Display-1 模块（7 寸 TFT LCD）

## 概述

本模块为液晶显示方案，负责核心底板的人机交互界面渲染，运行自研 MiniUI V2.0 框架。

- **主控芯片**：CH32H417QEU6（双核：V5F + V3F）
- **屏幕规格**：7 寸 TFT，分辨率 800×480，RGB888
- **驱动芯片**：SSD1963（FMC Bank1 NORSRAM 8080 模式，内部 GRAM，PWM 背光控制）
- **与核心模块接口**：模块侧 UART1，**921600/8-N-1**
- **模块 ID**：`0x10`
- **模块类型编号**：`0x01`
- **子类型编号**：`0x01`（LCD）

## 硬件组成

- **CH32H417QEU6**：双核 RISC-V MCU
  - **V5F**：负责 UI 渲染、SSD1963 显存更新、MiniUI 主循环
  - **V3F**：负责 UART1 协议帧接收与解析、与 Core 的通信交互
- **SSD1963**：LCD 驱动芯片
  - 接口：FMC Bank1 NORSRAM 8080 模式
  - 控制信号：PA4（MODE）、PA5（L/R）、PA6（U/D）、PA7（RESET）
  - 接收 RGB565 数据，转换为 RGB888 存入 GRAM，以 DE 模式驱动 LCD
- **LCD 面板**：800×480 RGB888 液晶屏
- **背光电路**：SSD1963 PWM 输出控制背光亮度
- **触摸屏（如有）**：电阻或电容触摸芯片，触摸事件通过 UART 上报 Core

## 软件职责

### 双核分工

| 核心 | 主要职责 |
|------|---------|
| **V5F** | 初始化 FMC / SSD1963 / MiniUI，执行 UI 主循环（页面渲染、动画、输入响应），运行本地游戏与应用 |
| **V3F** | 初始化 UART1（921600），维护协议帧接收状态机，解析 Core 指令并通知 V5F 执行；上报触摸事件、屏幕状态等 |

### V5F 初始化阶段

- 初始化 CH32H417 V5F 时钟、GPIO
- 初始化 FMC Bank1 NORSRAM（8080 模式）用于 SSD1963 通信
- 初始化 SSD1963：配置 PLL、显示时序、GRAM 地址、DE 模式、PWM 背光初始亮度
- 调用 `UI_Init()` 初始化 MiniUI V2.0 框架：加载字体、主题、基础控件库、页面栈
- 设置系统初始化标志位，通知 V3F 就绪

### V3F 初始化阶段

- 初始化 CH32H417 V3F 时钟、GPIO、UART1（921600）
- 初始化 UART 接收状态机（帧头 `0xAA`），准备接收 Core 指令
- 若支持触摸，初始化触摸芯片中断
- 设置系统初始化标志位，等待 V5F 就绪
- 两核通过 HSEM 机制同步初始化完成状态

### V5F 运行阶段

- **UI 主循环**：调用 `UI_Tick()` 驱动 MiniUI 页面渲染、动画更新、输入事件分发
- **页面管理**：维护页面栈（深度 16），支持 push/pop/switch，脏区域跟踪实现局部刷新
- **应用管理**：运行本地应用（文件管理、音乐、设置等）与本地小游戏（俄罗斯方块、2048、贪吃蛇、打砖块），不通过 UART 与 Core 交互
- **跨核通信**：接收 V3F 通过共享内存 / HSEM 提交的 Core 指令（如切换页面、更新状态栏、显示通知），调用对应 UI API 执行

### V3F 运行阶段

- **协议帧接收**：在 UART1 中断中逐字节接收 Core 下发的协议帧，状态机解析为完整帧后推入消息队列
- **指令分发**：主循环中从队列取出帧，解析 CMD：
  - 页面切换、状态栏更新、通知弹窗等 UI 指令 → 通过跨核消息通知 V5F 执行
  - 背光调节、旋转设置、配置保存等本地指令 → V3F 直接处理或转发 V5F
- **事件上报**：触摸事件、屏幕唤醒/休眠事件、错误异常等通过 UART1 主动上报 Core
- **输入注入**：将触摸坐标通过 `ui_input_touch_raw()` 注入 MiniUI 输入系统；将键盘/鼠标事件通过 `ui_input_mouse_raw()` / `ui_input_keyboard_raw()` 注入

### 中断与事件

- **V3F UART1 中断**：协议帧逐字节接收状态机（`WAIT_HEAD` → `FRAME_READY`）
- **V3F 触摸中断（如有）**：坐标采集与去抖，打包为 `CMD_DISP_TOUCH_EVT` 上报 Core，同时注入 MiniUI
- **V5F FMC / DMA 中断**：显存批量传输完成回调
- **V5F 背光 PWM 定时器**：根据自动息屏策略调节亮度
- **HSEM 中断**：双核同步与跨核消息通知

## MiniUI V2.0 框架

MiniUI 已实现并集成于本工程，API 位于 `Common/Common/MiniUI/miniui_*.h`。

### 核心模块

| 模块 | 功能 |
|------|------|
| `miniui_page.h` | 页面栈管理（深度 16）、页面切换/推入/弹出、脏区域跟踪、侧边栏集成 |
| `miniui_widget.h` | 控件库：Label、Button、Icon Button、Slider、Switch、Progress Bar、Card、List Item、TabView、Status Dot |
| `miniui_input.h` | 输入抽象：触摸/鼠标/键盘统一入口、输入队列、焦点管理、滑动手势识别（阈值 30 px） |
| `miniui_anim.h` | 动画系统：过渡动画、属性插值 |
| `miniui_render.h` | 渲染引擎：绘制原语、字体渲染、位图渲染 |

### 入口函数

```c
void UI_Init(void);        // 初始化 MiniUI 及显示子系统
void UI_Tick(void);        // 主循环调用，驱动渲染与动画
void UI_FullRefresh(void); // 强制全屏刷新
```

### 典型调用流程

```c
// V5F main loop
UI_Init();
while (1) {
    UI_Tick();  // 处理输入、更新动画、执行脏区域重绘
}
```

## 项目目录结构

```
display-model/display-1/
├── Common/                         # 公共代码与系统文件
│   ├── Common/                     # 自定义公共模块
│   │   ├── MiniUI/                 # MiniUI V2.0 框架（页面、控件、渲染、输入、动画）
│   │   ├── Games/                  # 本地小游戏（俄罗斯方块、2048、贪吃蛇、打砖块）
│   │   ├── Apps/                   # 本地应用（文件管理、音乐、设置、USB 等）
│   │   ├── FMC/                    # FMC Bank1 NORSRAM 8080 模式驱动
│   │   ├── hardware.c              # 全局调度与双核初始化入口
│   │   └── hardware.h              # 全局调度头文件
│   ├── Core/                       # RISC-V 内核相关文件
│   ├── Debug/                      # 调试支持文件
│   ├── Peripheral/                 # 芯片外设库（inc / src）
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
└── lcd.wvsln                       # MounRiver Studio 解决方案文件
```

## 命名规范

- **文件**：统一采用下划线加小写字母，例如 `hardware.c`、`miniui_page.h`
- **函数**：每个单词首字母大写加下划线，例如 `Hardware_Init()`、`UI_Tick()`
- **变量**：统一小写加下划线，例如 `frame_buffer`、`backlight_level`
- **文件夹**：下划线写法，仅首字母大写，例如 `MiniUI`、`Games`
- **系统目录**（`Core`、`Debug`、`Peripheral`、`Startup`）保持不动，不参与上述命名规范

## 开发要点

1. **波特率 921600**：V3F 的 UART1 中断处理必须足够轻量，建议在 ISR 中仅做字节接收与状态机推进，完整帧解析放到主循环执行，避免丢字节。
2. **双核同步**：V3F 与 V5F 通过 HSEM 同步初始化状态；V3F 解析到的 UI 相关指令需通过共享内存 / HSEM 通知 V5F，避免 V5F 在渲染期间被中断打断导致撕裂。
3. **FMC 8080 时序**：SSD1963 控制信号（PA4 MODE, PA5 L/R, PA6 U/D, PA7 RESET）与 FMC 数据/地址线需严格按数据手册时序配置；GRAM 地址指针自动递增特性可利用以减少总线开销。
4. **显存策略**：800×480 RGB565 全屏缓冲约 750KB，超出 CH32H417 片内 RAM；MiniUI 已采用脏区域跟踪机制实现局部刷新，开发者应避免不必要的整屏重绘。
5. **MiniUI 输入注入**：V3F 收到的触摸 / 键盘 / 鼠标事件需统一转换为 MiniUI 输入事件（`ui_input_touch_raw()` / `ui_input_mouse_raw()` / `ui_input_keyboard_raw()`），确保本地触摸与 Core 模拟触摸走同一套路径。
6. **错误上报**：SSD1963 通信失败、触摸屏失联、UART 帧错误等应通过 `CMD_DISP_EXT_ERROR_REPORT` 及时上报 Core，便于 Core 侧诊断。

## 鼠标与滚轮体验（V4.1）

- **右键点击**：`ui_input_feed_mouse()` 在右键抬起时产生携带 `UI_MOUSE_BTN_RIGHT` 的
  CLICK 事件（此前左键手势路径覆盖不到右键，文件应用右键菜单曾无法触发）。
- **滚轮事件合并**：快速滚动时滚轮事件在输入队列尾部累计为单个事件，避免队列被
  MOVE 事件灌满后丢轮次（队列满时 MOVE 最先被丢弃），滚动不再跳变。
- **平滑滚动**：`miniui_anim` 新增 `ui_scroller_t` 辅助器（goal + ease-out 动画，
  默认 120ms），文件列表 / 音乐播放列表 / 编辑器 / 终端 / 设置页的滚轮与滑动
  滚动均改为像素级动画滚动；命中测试使用动画中的实时像素位置，保证所见即所点。
- **纯滚轮报告不弹出光标**：仅含滚轮增量的鼠标报告（如 Core 转发的触摸圆环滚动）
  不再触发外接鼠标光标显示；光标位置默认居中，滚轮事件按光标位置命中滚动目标。
