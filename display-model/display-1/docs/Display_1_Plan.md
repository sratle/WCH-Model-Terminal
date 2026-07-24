# MiniUI + FMC8080 + SSD1963 驱动方案

## 一、整体架构

- **运行核**：V5F（主频 400MHz，负责 UI 渲染）
- **显示通信**：FMC NORSRAM Bank1，16-bit 8080 并口驱动 SSD1963
- **模块间通信**：USART1（PA9-TX / PA10-RX）与核心模块交互，接收键盘/鼠标/虚拟触摸等输入数据
- **显存策略**：SSD1963 内部集成 1215KB GRAM 作为物理显存；MiniUI 采用**行缓冲区合成 + 批量刷入**渲染架构，无需全屏帧缓冲（800×480×2=768KB 超出 512KB SRAM）
  - 多行合成缓冲区：800×8×2 = 12.8KB（`UI_COMPOSE_BATCH = 8`）
  - 脏区域管理：最多 256 个 dirty region，自动合并，每帧只重绘变化区域
  - 批量 SetWindow：每 8 行一次 SetWindow（11次 FMC 写），大幅降低命令开销
- **UI 框架**：自主实现 MiniUI（轻量专用 UI 库），替代 LVGL v9.5
  - 分层架构：App Layer → Page Manager → Widget Kit → Compositing Renderer → Display Driver
  - 页面栈深度：16 层，支持 push/pop/switch 和回退导航
  - on_update/on_draw 分离：逻辑更新和渲染解耦，统一合成渲染路径
  - 动画系统：线性/ease-out/ease-in 插值，支持位置/透明度/颜色动画
- **项目结构**：驱动层按功能模块化，放置在 `Common/Common/` 下的 `FMC/`、`SSD1963/`、`MiniUI/`、`UI/`、`Games/`、`Apps/`、`UART/`、`Touch/` 目录中
- **输入方式**：
  - **本地输入**：电容触摸屏（GT911）通过 I2C2（PB10/PB11）读取，支持 5 点触控、手势识别（点击/双击/长按/滑动）
  - **远程输入**：核心模块通过 USART1 发送键盘/鼠标数据，LCD 模块解析后映射为统一 UI 事件
  - **统一输入抽象**：触摸/鼠标/键盘三种输入源映射到同一套 `ui_event_t` 事件（DOWN/UP/MOVE/CLICK/DOUBLE_CLICK/LONG_PRESS/SWIPE_*/KEY_*）
  - **多点触控广播**：第二根及以后手指的 DOWN/UP 广播给所有 widget（支持同时按多个按钮），MOVE 不广播（防误触）

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
| RESET | PA0 | 普通 GPIO | SSD1963 硬件复位 |
| TE | PB8 | 普通 GPIO | Tearing Effect（VSYNC 同步，防止撕裂） |

### 2.3 LCD 屏控制信号

| 信号 | MCU 引脚 | 功能说明 |
|------|----------|----------|
| LCD_MODE | PA4 | DE 模式选择（拉高） |
| LCD_L/R | PA5 | 屏幕左右翻转控制 |
| LCD_U/D | PA6 | 屏幕上下翻转控制 |
| LCD_RESET | PA7 | LCD 模组复位 |

### 2.4 模块间通信

| 信号 | MCU 引脚 | 功能说明 |
|------|----------|----------|
| USART1_TX | PA9 | 向核心模块发送数据 |
| USART1_RX | PA10 | 接收核心模块数据（键盘/鼠标/虚拟触摸） |

### 2.5 背光控制

背光由 **SSD1963 内置 PWM** 控制，无需 MCU GPIO。

### 2.6 触摸

| 信号 | MCU 引脚 | 功能说明 |
|------|----------|----------|
| I2C2_SCL | PB10 | GT911 电容触摸屏 I2C 时钟 |
| I2C2_SDA | PB11 | GT911 电容触摸屏 I2C 数据 |
| CTP_RST | PC0 | 触摸屏控制器复位 |
| CTP_INT | PC1 | 触摸屏中断信号 |

### 2.7 FMC 初始化参数
- **Bank**: `FMC_Bank1_NORSRAM1`
- **MemoryType**: `FMC_MemoryType_SRAM`
- **DataWidth**: `FMC_MemoryDataWidth_16b`
- **AccessMode**: `FMC_AccessMode_B`

## 三、SSD1963 驱动层

### 3.1 底层读写接口
- `SSD1963_WriteCmd(uint16_t cmd)` — 通过 `SSD1963_CMD_ADDR` (0x60000000) 写命令
- `SSD1963_WriteData(uint16_t data)` — 通过 `SSD1963_DATA_ADDR` (0x60000004) 写数据
- `SSD1963_WriteBuffer(const uint16_t *data, uint32_t len)` — 批量写入
- `SSD1963_WriteRect(x1, y1, x2, y2)` — 设置窗口区域（SetWindow，供合成渲染器调用）

### 3.2 初始化序列
1. 硬件复位（GPIO 拉低 >10ms）
2. 软复位（0x01）
3. 设置 PLL（0xE0/E2）
4. 设置 LCD 时序参数（0xB0~B6）
5. 设置显示尺寸 800×480
6. 设置像素数据格式（RGB565 → RGB888）
7. 使能 TE 输出（0x35）
8. 打开显示（0x29）

## 四、MiniUI 自主 UI 库

### 4.1 架构设计

```
App Layer (Apps/, Games/)
    ↓
Page Manager (miniui_page.c) — 页面栈（16层）、脏区域（256）、合成渲染调度
    ↓
Widget Kit (miniui_widget.c) — Label, Button, Slider, Switch, Progress, Card, ListItem
    ↓
Compositing Renderer (miniui_render.c) — 行缓冲区合成、批量刷入(UI_COMPOSE_BATCH=8)
    ↓
Display Driver (SSD1963/) — GRAM 写入，FMC 8080 并口
```

### 4.2 核心模块

| 文件 | 功能 |
|------|------|
| `miniui_types.h` | 基础类型：颜色、点、矩形、字体、事件（DOWN/UP/MOVE/CLICK/DOUBLE_CLICK/LONG_PRESS/SWIPE/KEY_*）、widget/page |
| `miniui_color.h` | RGB565 宏、马卡龙配色常量、颜色混合/darken/lighten |
| `miniui_render.c/h` | **合成渲染引擎**：行缓冲区（800×8行）合成、绘图原语自动裁剪、批量flush_rows |
| `miniui_font.c/h` | 字体引擎：glyph-based，1bpp/2bpp/4bpp 抗锯齿 |
| `miniui_widget.c/h` | Widget 系统：基类（rect/prev_rect/flags/callbacks）、自动脏跟踪、move/resize |
| `miniui_page.c/h` | 页面管理器：页面栈（16层）、脏区域（256个）、合成渲染调度、on_update回调 |
| `miniui_input.c/h` | 输入系统：触摸手势（点击/双击/长按/滑动）、多点触控（5点）、鼠标、键盘统一映射 |
| `miniui_anim.c/h` | 动画系统：线性/ease-out/ease-in 插值 |
| `miniui.h` | 总头文件 |
| `ui_system.c` | UI 系统入口：`UI_Init()`、`UI_Tick()`（事件分发+on_update+合成渲染）、`UI_FullRefresh()` |

### 4.3 显存策略：行缓冲区合成渲染

MiniUI 使用**行缓冲区合成**而非直接写 GRAM：

1. Widget/游戏逻辑标记脏区域（`ui_page_invalidate`）
2. `ui_page_draw()` 遍历脏区域，每 `UI_COMPOSE_BATCH(8)` 行一批：
   - `ui_render_begin_rect()` 设置合成目标
   - `ui_render_fill_line_buf()` 填充背景色
   - 调用 sidebar_draw、page on_draw、widget draw_cb（写入行缓冲区）
   - `ui_render_flush_rows()` 一次 SetWindow 刷入 8 行数据到 GRAM
3. 清空脏列表

**优势**：
- SetWindow 开销从每行 11 次 FMC 写降低到每 8 行 11 次
- 绘图原语自动裁剪，无需手动 set_clip
- 天然防闪烁（数据在缓冲区完整合成后一次刷入）

### 4.4 统一输入系统

```c
typedef enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_DOWN, UI_EVENT_UP, UI_EVENT_MOVE,
    UI_EVENT_CLICK, UI_EVENT_DOUBLE_CLICK,
    UI_EVENT_LONG_PRESS, UI_EVENT_LONG_PRESS_REPEAT,
    UI_EVENT_SWIPE_UP, UI_EVENT_SWIPE_DOWN, UI_EVENT_SWIPE_LEFT, UI_EVENT_SWIPE_RIGHT,
    UI_EVENT_KEY_DOWN, UI_EVENT_KEY_UP, UI_EVENT_KEY_CLICK,
    UI_EVENT_KEY_DOUBLE_CLICK, UI_EVENT_KEY_LONG_PRESS, UI_EVENT_KEY_LONG_REPEAT,
    UI_EVENT_KEY_UP_ARROW, UI_EVENT_KEY_DOWN_ARROW,
    UI_EVENT_KEY_LEFT_ARROW, UI_EVENT_KEY_RIGHT_ARROW,
    UI_EVENT_KEY_OK, UI_EVENT_KEY_BACK,
    UI_EVENT_PRESS_CANCEL,
} ui_event_type_t;
```

- **触摸输入**：GT911 I2C2 读取，支持 5 点触控，手势识别（点击/双击/长按/滑动）
- **鼠标输入**：UART 接收，映射为 DOWN/UP/MOVE/CLICK
- **键盘输入**：UART 接收，映射为 KEY_DOWN/UP/CLICK/LONG_PRESS/ARROW/OK/BACK
- **多点触控**：主触点走 capture 机制；额外触点的 DOWN/UP 广播给所有 widget（MOVE 不广播）

### 4.5 时基与主循环

```c
void Hardware(void)
{
    LCD_Init();
    UI_Init();
    UART_Module_Init();

    while (1) {
        Touch_Scan();          // I2C 通信
        SSD1963_WaitVSync();   // 等待消隐期
        UI_Tick();             // 输入+on_update+合成渲染
        UART_Module_Poll();
        // 帧率限制
    }
}
```

## 五、双核协作修改

V3F 负责唤醒 V5F。V5F 运行 UI，V3F 运行其他业务逻辑。

## 六、项目结构与文件功能

```
lcd/
├── Common/
│   ├── Common/
│   │   ├── FMC/                FMC 底层驱动
│   │   │   ├── fmc_driver.c    FMC NORSRAM Bank1 初始化、GPIO 复用、时序
│   │   │   └── fmc_driver.h    FMC 基地址、命令/数据地址映射
│   │   ├── MiniUI/             自主 UI 库核心
│   │   │   ├── miniui_types.h  基础类型、事件枚举、widget/page 前向声明
│   │   │   ├── miniui_color.h  RGB565 宏、马卡龙配色、颜色工具
│   │   │   ├── miniui_render.c/h  合成渲染引擎（行缓冲区+批量flush）
│   │   │   ├── miniui_font.c/h    字体引擎（glyph-based，抗锯齿）
│   │   │   ├── miniui_widget.c/h  Widget系统（自动脏跟踪、move/resize）
│   │   │   ├── miniui_page.c/h    页面管理器（256脏区域、合成调度）
│   │   │   ├── miniui_input.c/h   输入系统（手势识别、多点触控）
│   │   │   ├── miniui_anim.c/h    动画系统
│   │   │   ├── miniui.h           总头文件
│   │   │   ├── ui_system.c        UI_Init/UI_Tick/UI_FullRefresh
│   │   │   └── font/              位图字体数据
│   │   ├── SSD1963/            SSD1963 控制器驱动
│   │   ├── UI/                 业务 UI 页面
│   │   ├── Games/              本地游戏（5个：tetris/2048/snake/airplane/minesweeper）
│   │   ├── Apps/               系统应用（16个）
│   │   ├── Touch/              GT911 电容触摸屏驱动
│   │   ├── UART/               模块间串口通信
│   │   ├── hardware.c          业务入口
│   │   └── settings.c/h        用户设置存储
│   └── Peripheral/             标准外设库
├── V5F/                        V5F 核（UI渲染）
└── V3F/                        V3F 核（业务逻辑）
```

## 七、风险与建议

1. **显存与刷新策略**：MiniUI 采用行缓冲区合成渲染，800×8×2=12.8KB 合成缓冲区 + SSD1963 1215KB GRAM。批量合成（UI_COMPOSE_BATCH=8）将 SetWindow 开销降低 8 倍。

2. **SSD1963 时序与 DE 模式**：初始化时必须配置为 DE 模式输出，且 HFP/HBP/VFP/VBP/PCLK 必须精确设置。

3. **引脚冲突**：FMC 数据线占用 PD/PE 大量引脚，需确认各功能引脚无复用冲突。

4. **代码体积**：MiniUI 轻量专用，Flash 占用预计 < 100KB（含字体）。V5F 可用 CodeFlash 960KB，余量充足。

5. **输入系统**：本地触摸（GT911 I2C2）和远程 USART1 输入统一为 `ui_event_t` 事件队列。多点触控支持 5 点，MOVE 不广播防误触。

## 八、UI 界面设计

### 8.1 配色方案

- 主背景：`#F5F5F5`（暖白）
- 侧边栏：`#F5F5F5` 背景，`#B8E0D2` 选中高亮
- 主色调：`#7EC8C8`（薄荷蓝绿）
- 文字：`#4A4A4A`（深灰）/ `#9E9E9E`（浅灰）

### 8.2 布局

- 左侧固定侧边栏（200px）+ 右侧内容区（600px）
- 全屏应用/游戏隐藏侧边栏
- Montserrat 12/16 字体，12px 圆角风格

### 8.3 游戏列表

| 游戏 | 说明 |
|------|------|
| Tetris | 俄罗斯方块 |
| 2048 | 数字合成 |
| Snake | 贪吃蛇 |
| Airplane | 飞机大战 |
| Minesweeper | 扫雷（双击扫开） |

所有游戏使用统一的 on_update/on_draw 分离模式，合成渲染器自动处理脏区域刷新。
