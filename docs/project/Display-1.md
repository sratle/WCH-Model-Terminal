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

### 架构说明（现状：V5F 单核管理）

> **架构演进**：早期设计为 V3F 负责 UART 协议栈、V5F 负责 UI 的双核分工，
> 经 HSEM/共享内存同步。当前固件已演进为 **V5F 单核统一管理**（与 Core 架构一致）：
> V3F 仅负责上电初始化时钟、唤醒 V5F，然后空转（`Hardware_V3F_Init()` 为
> `while(1);`），不执行任何业务逻辑。瓶颈在 FMC/IO 而非算力，单核避免了
> 核间同步复杂度。下文保留原双核分工描述仅作历史参考。

| 核心 | 主要职责（现状） |
|------|---------|
| **V5F** | 一切：UART1 协议栈（ISR 收帧 + 环形缓冲 + 主循环解析）、MiniUI 渲染、触摸、SSD1963 显存更新、本地应用与游戏 |
| **V3F** | 仅上电唤醒 V5F，随后空转 |

### V5F 初始化阶段

- 初始化 CH32H417 V5F 时钟、GPIO
- 初始化 FMC Bank1 NORSRAM（8080 模式）用于 SSD1963 通信
- 初始化 SSD1963：配置 PLL、显示时序、GRAM 地址、DE 模式、PWM 背光初始亮度
- 初始化 UART1（921600）：RX 中断逐字节状态机收帧 → 8KB 环形缓冲
- 调用 `UI_Init()` 初始化 MiniUI V2.0 框架：加载字体、主题、基础控件库、页面栈

### V3F 初始化阶段

- 初始化时钟、PLL，通过 `NVIC_WakeUp_V5F()` 唤醒 V5F
- 调用 `Hardware_V3F_Init()` 进入空转，不参与任何业务

### V5F 运行阶段

- **主循环**（`V5F/User/main.c`）：`Touch_Scan()` → `UART_Module_Poll()` →
  `SSD1963_WaitVSync()` → `UI_Tick()` → 25 FPS 帧率限制
- **UART_Module_Poll()**：从环形缓冲取完整帧并分发——心跳 GET_TYPE 应答、
  CLI 直通响应组装、输入事件注入、模块事件转发等
- **UI 主循环**：`UI_Tick()` 驱动 MiniUI 页面渲染、动画更新、输入事件分发
- **页面管理**：维护页面栈（深度 16），支持 push/pop/switch，脏区域跟踪实现局部刷新
- **应用管理**：运行本地应用（文件管理、音乐、设置等）与本地小游戏（俄罗斯方块、2048、贪吃蛇、打砖块），不通过 UART 与 Core 交互

### 中断与事件

- **V5F UART1 中断**：协议帧逐字节接收状态机（`WAIT_HEAD` → 完成推入环形缓冲）
- **V5F 触摸中断/轮询**：坐标采集与去抖，本地注入 MiniUI，并打包 `CMD_DISP_TOUCH_EVT` 上报 Core
- **V5F FMC / DMA 中断**：显存批量传输完成回调
- **V5F 背光 PWM 定时器**：根据自动息屏策略调节亮度

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

1. **波特率 921600**：V5F 的 UART1 中断处理必须足够轻量，ISR 中仅做字节接收与状态机推进，完整帧处理放到主循环（`UART_Module_Poll`）执行，避免丢字节。
2. **主循环节拍**：V5F 主循环约 25 FPS（~40ms）轮询一次 UART 环形缓冲（8KB）；
   Core 突发大量下行帧（如图片/文件十六进制转储）时应控制发送节奏或分帧间隔，
   防止环满丢帧。
3. **FMC 8080 时序**：SSD1963 控制信号（PA4 MODE, PA5 L/R, PA6 U/D, PA7 RESET）与 FMC 数据/地址线需严格按数据手册时序配置；GRAM 地址指针自动递增特性可利用以减少总线开销。
4. **显存策略**：800×480 RGB565 全屏缓冲约 750KB，超出 CH32H417 片内 RAM；MiniUI 已采用脏区域跟踪机制实现局部刷新，开发者应避免不必要的整屏重绘。
5. **MiniUI 输入注入**：本地触摸与 Core 转发的键盘/鼠标/模拟触摸事件统一转换为 MiniUI 输入事件（`ui_input_touch_raw()` / `ui_input_mouse_raw()` / `ui_input_keyboard_raw()`），走同一套路径。
6. **错误上报**：SSD1963 通信失败、触摸屏失联、UART 帧错误等应通过 `CMD_DISP_EXT_ERROR_REPORT` 及时上报 Core，便于 Core 侧诊断。
7. **外部数据必须做边界校验（血泪教训）**：Images 应用曾因对 hex 流无完整性校验、
   对 BMP 头无上限检查，在传输丢帧/截断后解析出垃圾宽高（可达 2^31），
   `img_render_preview` 的双重像素循环实际永久卡死 V5F 主循环——表现为心跳与
   CLI 直通全部中断、只能重启。现行防御：EOF 时校验 `size=N` 与解码长度一致、
   解码时限宽/高 ≤2048 且 data_offset ≥54、渲染时按缓冲实际行数裁剪循环。
   任何解析 Core 下行数据的路径都必须假设数据可能损坏。

## ITCM 128K 物理上限与 .flashcode（重要，血泪教训）

**问题**：CH32H417 的 ITCM 物理容量为 **128K**（`0x200A0000~0x200C0000`），
与 DTCM（`0x200C0000` 起）不联动，**DTCM 区域不能取指**。早期曾把链接脚本
`RAM_CODE` 扩到 144K/192K（越界借用 DTCM），链接器不再报错，但越过
`0x200C0000` 的函数一旦执行即取指异常、MCU 死机重启。典型现象是
Display 与 Core 指令交互时死机；删除 Game 后"恢复"，本质是把 `.highcode`
压回 128K 以内。

**修复（现行链接布局）**：

1. `RAM_CODE` 严格收回物理上限 **128K**（与 main-model Core 一致）；
2. 低热度代码经 `.flashcode` 输出段直接放在 **FLASH XIP 执行**
   （启动代码本就 XIP，可行性已证），`.highcode` 用 `EXCLUDE_FILE` 排除；
3. ISR、渲染、协议解析、驱动、**游戏**等帧率敏感热路径必须留在 `.highcode`（ITCM）。

**当前 .flashcode 内容（V4.2 调整）**：

- 曾把 `game_*.o` 放入 `.flashcode`，结果游戏动画明显变慢（Flash XIP 存在
  等待周期），游戏对刷新速度要求高，**已整体移回 ITCM**；
- 取而代之的是 7 个无动画/低频交互的 APP：`app_usb.o`、`app_power.o`、
  `app_bt.o`、`app_nfc.o`、`app_fingerprint.o`、`app_subdisplay.o`、
  `app_lrange.o`——这些页面刷新慢几拍无感知。

**内存预算（2026-08-06 实测，来自 lcd_V5F.map）**：

| 段 | 大小 | 说明 |
|----|------|------|
| `.highcode`（ITCM） | 121,000 B / 131,072 B（余量 ~10 KB，7.7%） | 含全部 `game_*.o`（~22 KB） |
| `.flashcode`（FLASH XIP） | 17,264 B | 7 个低热度 APP |
| FLASH 总量 | text+data ≈ 162 KB / 512 KB | 余量充足 |

**后续代码增长时**：优先把低热度 APP（非帧率驱动）追加到 `.flashcode` 段，
不要扩大 `RAM_CODE`，也不要把游戏/MiniUI 渲染/协议解析移出 ITCM。

## RGB 游戏联动（V4.2）

Games 通过 `DISP_EXT_RGB_EFFECT`（0x1E，见 Protocol_Display.md）经 Core 转发
触发 RGB Submodel 的一次性动画（发送即忘）：

| 游戏 | 触发时机 | 效果 | 速度档位 |
|------|---------|------|---------|
| 俄罗斯方块 | 左右移动/软降生效、旋转、硬降 | wave（方向匹配输入） | 8 |
| 2048 | 方向移动生效（键盘/WASD/滑动） | wave（方向匹配输入） | 8 |
| 贪吃蛇 | 转向生效（键盘/滑动/方向按钮） | wave（方向匹配输入） | 8 |
| 飞机大战 | 子弹击中敌机 | ripple | 10 |
| 扫雷 | 挖开格子 | ripple | 6 |

入口统一封装在 `Games/games.c`：`games_rgb_wave_dir()`（逻辑方向→波浪方向映射，
向左输入 = 波浪从右向左传播）与 `games_rgb_ripple()`，底层走
`UART_SendRgbWave()` / `UART_SendRgbRipple()`（UART/uart_module.c）。

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
