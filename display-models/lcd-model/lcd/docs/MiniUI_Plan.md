# 自主轻量UI库（MiniUI）替代LVGL方案

## 背景与决策

LVGL v9.5 移植过程中遇到大量源码裁剪、头文件依赖、汇编兼容性等问题，且项目实际UI需求相对固定，不需要LVGL的通用widget系统。自主实现专用UI库可以：
- 完全掌控代码，无外部依赖
- 编译秒过，无配置噩梦
- 内存占用精确可控
- 渲染性能针对本项目优化

## 一、整体架构

### 1.1 分层设计

```
App Layer (ui_main.c, ui_home.c, ui_apps.c, ui_games.c, ui_models.c, ui_settings.c, ui_app_common.c, app_*.c, game_*.c)
    |
Page Manager (ui_page.c)  -- 页面栈、生命周期、切换动画、回退导航、脏区域管理
    |
Widget Kit (ui_widget.c)  -- 按钮、标签、滑块、开关、进度条、卡片、Tab、列表项
    |
Render Engine (ui_render.c) -- 抗锯齿字体、矩形、圆角、线条、像素、图片位块传输
    |
Display Driver (ssd1963.c + fmc_driver.c) -- 已有的FMC+SSD1963驱动
```

### 1.2 显存策略

- **屏幕分辨率**：800x480，RGB565
- **帧缓冲**：1个全屏帧缓冲 = 800*480*2 = 768KB（放在SRAM中，512KB不够）
- **解决方案**：不使用全屏帧缓冲，改用**脏矩形局部刷新**
  - 维护一个"脏区域列表"（最多8个矩形）
  - Widget标记自身区域为dirty
  - Render Engine只重绘dirty区域，直接写入SSD1963 GRAM
  - 这样无需768KB帧缓冲，只需widget自身的临时缓冲（如字体光栅化缓冲几KB）

### 1.3 刷新机制

- **主循环**：`while(1)` 中每33ms（~30fps）调用一次 `UI_Tick()`
- `UI_Tick()` 逻辑：
  1. 处理输入（触摸/串口）
  2. 更新动画状态（页面切换淡入淡出、按钮按压缩放）
  3. 收集所有dirty区域，合并重叠矩形
  4. 对每个dirty区域：设置SSD1963窗口 → 渲染该区域内所有widget → 写入GRAM
  5. 清空dirty列表

## 二、目录结构与文件规划

在 `Common/Common/` 下新建 `MiniUI/` 目录，替换原有的 `LVGL_LCD/`：

```
Common/Common/
├── MiniUI/
│   ├── miniui.h              -- 总头文件，包含所有模块
│   ├── miniui_types.h        -- 基础类型：颜色、点、矩形、字体句柄
│   ├── miniui_color.h        -- RGB565颜色定义、马卡龙配色常量
│   ├── miniui_font.h         -- 字体引擎接口（位图字体、抗锯齿）
│   ├── miniui_render.h/c     -- 渲染引擎：draw_rect, draw_text, draw_line, draw_round_rect
│   ├── miniui_widget.h/c     -- Widget基类与具体widget（label, button, slider, switch, progress, card, tab, list_item）
│   ├── miniui_page.h/c       -- 页面管理器：页面栈、切换动画、回退导航、脏区域追踪
│   ├── miniui_input.h/c      -- 输入抽象：触摸/鼠标/键盘→统一输入事件映射
│   ├── miniui_anim.h/c       -- 动画系统：简单的线性/缓动插值
│   └── font/
│       ├── font_montserrat_12.c  -- Montserrat 12号抗锯齿位图字体
│       ├── font_montserrat_16.c  -- Montserrat 16号抗锯齿位图字体
│       └── font_symbols.c        -- 图标字体（首页/应用/模块/设置等图标）
├── UI/                       -- 业务UI页面
│   ├── ui_main.c/h           -- 主框架：侧边栏(Home/Apps/Games/Models/Settings)+内容区
│   ├── ui_home.c/h           -- 首页
│   ├── ui_apps.c/h           -- 应用网格（15个应用，3x4翻页）
│   ├── ui_models.c/h         -- 模块状态
│   ├── ui_settings.c/h       -- 设置列表
│   ├── ui_games.c/h          -- 游戏网格（4个游戏，单页无翻页）
│   └── ui_app_common.c/h     -- 全屏应用/游戏通用模板（返回按钮+标题栏+ESC退出）
├── Apps/                     -- 系统应用页面（每个应用独立页面，通过UART与核心模块交互）
│   ├── app_music.c/h
│   ├── app_file.c/h
│   ├── app_editor.c/h
│   ├── app_images.c/h
│   ├── app_usb.c/h
│   ├── app_power.c/h
│   ├── app_bt.c/h
│   ├── app_nfc.c/h
│   ├── app_fingerprint.c/h
│   ├── app_health.c/h
│   ├── app_subdisplay.c/h
│   ├── app_lights.c/h
│   ├── app_irrange.c/h
│   ├── app_ebook.c/h
│   └── app_emusic.c/h
├── Games/                    -- 本地游戏页面（每个游戏独立页面，纯本地运行）
│   ├── game_tetris.c/h
│   ├── game_2048.c/h
│   ├── game_snake.c/h
│   └── game_breakout.c/h
├── SSD1963/                  -- 已有驱动，无需修改
├── FMC/                      -- 已有驱动，无需修改
└── hardware.c/h              -- 修改：Hardware_V5F_Init()调用UI_Init()
```

## 三、核心模块设计

### 3.1 基础类型（miniui_types.h）

```c
typedef uint16_t ui_color_t;  /* RGB565 */

typedef struct {
    int16_t x;
    int16_t y;
} ui_point_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} ui_rect_t;

typedef struct {
    const uint8_t *bitmap;    /* 抗锯齿灰度位图数据 */
    const uint16_t *unicode_list; /* 支持的unicode码点列表 */
    uint16_t unicode_count;
    uint8_t height;           /* 字体高度 */
    int8_t baseline;          /* 基线偏移 */
} ui_font_t;
```

### 3.2 渲染引擎（miniui_render.c）

核心函数：
- `void ui_draw_fill_rect(ui_rect_t *r, ui_color_t color)` -- 填充矩形
- `void ui_draw_round_rect(ui_rect_t *r, uint8_t radius, ui_color_t fill, ui_color_t border)` -- 圆角矩形
- `void ui_draw_text(ui_rect_t *r, const char *text, const ui_font_t *font, ui_color_t color)` -- 抗锯齿文本
- `void ui_draw_line(ui_point_t *p1, ui_point_t *p2, ui_color_t color, uint8_t width)` -- 线条
- `void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color)` -- 单像素

**抗锯齿字体渲染**：
- 字体数据存储为灰度位图（每个像素1字节alpha值）
- 渲染时根据alpha混合前景色和背景色：`color = bg + (fg - bg) * alpha / 255`
- 背景色从当前渲染位置"读取"（因为无帧缓冲，需要widget自己知道背景色，或限制文本只画在纯色背景上）
- **简化方案**：文本widget只能放在纯色背景上，渲染时直接用bg_color参数混合

### 3.3 Widget系统（miniui_widget.c）

所有widget继承自基类：

```c
typedef struct ui_widget {
    ui_rect_t rect;           /* 位置和尺寸 */
    uint16_t flags;           /* 可见、使能、脏标记等 */
    ui_color_t bg_color;      /* 背景色 */
    void (*draw_cb)(struct ui_widget *w, ui_rect_t *dirty); /* 绘制回调 */
    void (*event_cb)(struct ui_widget *w, ui_event_t *e);   /* 事件回调 */
} ui_widget_t;
```

具体widget：
- **ui_label_t**：文本标签，支持12/16号字体，左/中/右对齐
- **ui_button_t**：按钮，圆角矩形，按压时缩放0.95，支持图标+文字
- **ui_slider_t**：滑块，进度条+可拖动滑块（用于背光、音量）
- **ui_switch_t**：开关，圆形滑块在轨道左右滑动
- **ui_progress_t**：进度条，显示百分比或不确定进度（水平条+可选文字）
- **ui_card_t**：卡片容器，圆角+阴影，可包含子widget
- **ui_tabview_t**：Tab视图，顶部Tab按钮+内容区切换
- **ui_list_item_t**：列表项，左侧文字+右侧控件，底部分割线

### 3.4 页面管理器（miniui_page.c）

**页面栈设计**：
- 支持页面压栈（`ui_page_push`）和弹栈（`ui_page_pop`）
- 最大栈深度：16层（足够覆盖：首页→应用列表→某个应用→应用内子页面→返回）
- 首页永远在栈底，不可弹出
- 页面切换时保存/恢复页面状态（滚动位置、选中项等）

```c
typedef struct ui_page {
    const char *name;
    uint32_t id;              /* 页面唯一标识 */
    ui_rect_t content_rect;   /* 内容区矩形（排除侧边栏） */
    ui_widget_t **widgets;    /* 页面内所有widget数组 */
    uint16_t widget_count;
    void (*on_enter)(void);   /* 页面进入回调 */
    void (*on_exit)(void);    /* 页面退出回调 */
    void (*on_draw)(ui_rect_t *dirty); /* 自定义绘制（如游戏画面） */
    void (*on_back)(void);    /* 回退键/返回手势回调 */
    int16_t scroll_y;         /* 页面滚动位置（用于恢复） */
    uint8_t flags;            /* 全屏模式、有返回按钮等 */
} ui_page_t;

/* 页面栈操作 */
void ui_page_push(ui_page_t *page);   /* 进入新页面，压栈 */
void ui_page_pop(void);               /* 返回上一页面，弹栈 */
void ui_page_switch(ui_page_t *page); /* 切换到新页面，清空栈 */
bool ui_page_can_go_back(void);       /* 是否可以返回 */
```

**页面类型**：
- **主框架页面**：带侧边栏（首页、应用列表、模块、设置）
- **全屏页面**：隐藏侧边栏，占满800x480（游戏、部分应用）
- **子页面**：从应用内部进入的子界面，带顶部返回按钮

**页面切换动画**：
- 200ms淡入淡出：通过alpha混合实现（但RGB565无alpha通道，改用"滑动"或"直接切换"）
- **简化方案**：直接切换，不加动画。或做简单的"内容区向左/右滑动"（widget坐标偏移）

### 3.5 输入系统（miniui_input.c）

**统一输入抽象**：

```c
/* 输入源类型 */
typedef enum {
    UI_INPUT_TOUCH,   /* 本地电容触摸屏 */
    UI_INPUT_MOUSE,   /* 核心模块通过UART发送的鼠标数据 */
    UI_INPUT_KEYBOARD,/* 核心模块通过UART发送的键盘数据 */
} ui_input_source_t;

/* 统一输入事件类型 */
typedef enum {
    UI_EVENT_NONE,
    UI_EVENT_PRESS,       /* 按下/点击 */
    UI_EVENT_RELEASE,     /* 释放 */
    UI_EVENT_DRAG,        /* 拖动（触摸滑动） */
    UI_EVENT_SWIPE_UP,    /* 向上滑动 */
    UI_EVENT_SWIPE_DOWN,  /* 向下滑动 */
    UI_EVENT_SWIPE_LEFT,  /* 向左滑动 */
    UI_EVENT_SWIPE_RIGHT, /* 向右滑动 */
    UI_EVENT_KEY_UP,      /* 键盘方向键上 */
    UI_EVENT_KEY_DOWN,    /* 键盘方向键下 */
    UI_EVENT_KEY_LEFT,    /* 键盘方向键左 */
    UI_EVENT_KEY_RIGHT,   /* 键盘方向键右 */
    UI_EVENT_KEY_OK,      /* 键盘确认/回车 */
    UI_EVENT_KEY_BACK,    /* 键盘返回/ESC */
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    ui_input_source_t source;
    ui_point_t pos;       /* 触摸/鼠标坐标（键盘事件此字段无效） */
    ui_point_t delta;     /* 拖动偏移 */
} ui_event_t;
```

**输入映射与分发**：

1. **触摸输入**（本地I2C2读取）：
   - 原始数据：触摸坐标(x,y) + 按下/释放状态
   - 手势识别：记录press位置和release位置，计算delta
     - |delta.x| > |delta.y| 且 |delta.x| > 30px → SWIPE_LEFT/RIGHT
     - |delta.y| > |delta.x| 且 |delta.y| > 30px → SWIPE_UP/DOWN
     - 否则 → PRESS/RELEASE（点击）

2. **鼠标输入**（UART接收）：
   - 核心模块发送鼠标相对位移(dx,dy)和按键状态
   - 维护一个虚拟鼠标光标位置，根据dx/dy更新
   - 鼠标移动 → 更新光标位置，触发hover效果
   - 鼠标左键按下/释放 → 映射为PRESS/RELEASE（位置为光标位置）

3. **键盘输入**（UART接收）：
   - 核心模块发送按键码（方向键、回车、ESC等）
   - 直接映射为对应的UI_EVENT_KEY_*事件
   - 键盘事件不依赖坐标，由当前焦点widget处理

4. **事件分发**：
   - 触摸/鼠标事件：遍历当前页面的widget，找到包含pos的最上层widget，调用其event_cb
   - 键盘事件：发送给当前获得焦点的widget（或页面级别的on_key回调）
   - 滑动事件：先发送给当前页面，如果页面不处理（如应用列表滚动），再发送给widget

### 3.6 动画系统（miniui_anim.c）

极简动画：
```c
typedef struct {
    int32_t start;
    int32_t end;
    int32_t current;
    uint16_t duration_ms;
    uint16_t elapsed_ms;
    void (*update_cb)(int32_t value, void *user_data);
} ui_anim_t;
```

- 线性插值：`current = start + (end - start) * elapsed / duration`
- 缓动插值（ease_out）：`t = 1 - (1 - t)^2`
- 每帧在UI_Tick中更新所有活跃动画

## 四、业务UI实现

### 4.1 主框架（ui_main.c）

- **侧边栏**：宽200px，高480px，背景色 `#F5F5F5`
  - 顶部：项目名称 "ELAB"（Montserrat 16，主色 `#7EC8C8`）
  - 5个菜单项：Home/Apps/Games/Models/Settings
  - 选中项：背景 `#B8E0D2`，左侧4px竖条 `#7EC8C8`
- **内容区**：x=200, y=0, w=600, h=480
  - 根据当前页面显示不同内容

### 4.2 首页（ui_home.c）

- 日期标签（Montserrat 16，深灰 `#4A4A4A`）
- 时间标签（Montserrat 16，主色 `#7EC8C8`）-- 注意不用24号了
- 模块状态卡片：3~4个横向排列的ui_card_t
- 系统状态栏：内存、FPS、CPU

### 4.3 应用界面（ui_apps.c）

- 3x4图标网格（每页12个应用），共15个应用需要翻页（2页）
- 图标按钮：150x90px，圆角16px，图标+文字
- 翻页导航："<" ">" 按钮和页码指示器（如 "1/2"）
- 点击应用图标 → `ui_page_push(&app_xxx_page)` 进入应用全屏页面
- 不使用中文，使用图标和ASCII字符

### 4.4 游戏界面（ui_games.c）

- 3x4图标网格（与Apps页风格统一），4个游戏单页显示，无需翻页
- 点击游戏图标 → `ui_page_push(&game_xxx_page)` 进入游戏全屏页面
- 不使用中文，使用图标和ASCII字符

### 4.5 全屏应用/游戏通用模板（ui_app_common.c）

- 所有应用和游戏页面使用统一的全屏模板：
  - 页面flags标记 `UI_PAGE_FLAG_FULLSCREEN`，隐藏侧边栏，内容区扩展为800x480
  - 顶部标题栏（40px高）：左侧返回按钮 "<" + 应用名称
  - 返回按钮点击或键盘ESC → `ui_page_pop()` 回到上一页
  - 中间内容区：应用/游戏特定UI
  - 不使用中文，使用图标和ASCII字符

### 4.6 模块界面（ui_models.c）

- Tab视图：Display/Keyboard/Power/Core/BT-WiFi
- 每个Tab页：卡片式布局，显示模块名称、状态圆点、关键参数

### 4.7 设置界面（ui_settings.c）

- 垂直列表，每项50px高
- 背光：ui_slider_t
- 自动息屏：ui_switch_t
- 屏幕旋转：ui_button_t（点击循环切换0/90/180/270）
- 恢复出厂：ui_button_t（红色，点击确认）

### 4.8 系统应用页面（app_*.c）

- 每个应用一个独立页面，使用 `ui_app_common.c` 统一全屏模板
- 页面flags标记 `UI_PAGE_FLAG_FULLSCREEN`，隐藏侧边栏
- 顶部标题栏（应用名称 + 返回按钮 "<"）
- 返回按钮或键盘ESC → `ui_page_pop()` 回到应用列表
- 通过UART与核心模块通信

### 4.9 游戏页面（game_*.c）

- 游戏全屏模式：使用 `ui_app_common.c` 统一模板，页面flags标记 `UI_PAGE_FLAG_FULLSCREEN`，隐藏侧边栏，内容区扩展为800x480
- 顶部标题栏（游戏名称 + 返回按钮 "<"）
- 游戏逻辑在game_*.c中，每帧通过`ui_page->on_draw()`回调渲染
- 俄罗斯方块：10x20网格，每个格子30x30px
- 2048：4x4网格，每个格子120x120px，圆角12px
- 游戏内返回：点击左上角返回按钮 "<" 或按键盘ESC → `ui_page_pop()`

## 五、字体与资源

### 5.1 字体生成

Montserrat 12和16号字体：
- 使用LVGL的字体转换工具（`lv_font_conv`）或在线工具生成C数组
- 只包含常用字符：ASCII 32~126 + 少量中文（如果不需要中文，只包含ASCII）
- 抗锯齿：2bpp或4bpp灰度位图
- 存储在 `MiniUI/font/` 下，作为静态const数组编译进CodeFlash

### 5.2 图标

- 使用简单的位图图标（16x16或24x24）
- 或从Montserrat字体中选择符号字符（如箭头、方块等）
- 存储在 `font_symbols.c` 中

## 六、初始化与主循环

### 6.1 初始化（Hardware_V5F_Init）

```c
void Hardware_V5F_Init(void)
{
    FMC_Driver_Init();
    LCD_Init(LCD_ORIENTATION_NORMAL);
    SSD1963_Init();
    UI_Init();  /* 替代原来的LVGL_Init() */
}
```

### 6.2 UI_Init

```c
void UI_Init(void)
{
    /* 初始化渲染引擎 */
    ui_render_init();
    
    /* 初始化页面管理器 */
    ui_page_init();
    
    /* 注册所有页面 */
    ui_page_register(&page_home);
    ui_page_register(&page_software);
    ui_page_register(&page_models);
    ui_page_register(&page_settings);
    /* 注册应用页面 */
    ui_page_register(&app_music_page);
    ui_page_register(&app_file_page);
    /* ... 更多应用页面 ... */
    /* 注册游戏页面 */
    ui_page_register(&game_tetris_page);
    ui_page_register(&game_2048_page);
    /* ... 更多游戏页面 ... */
    
    /* 显示首页 */
    ui_page_switch(&page_home);
    
    /* 首次全屏刷新 */
    ui_full_refresh();
}
```

### 6.3 主循环（main.c）

```c
while(1)
{
    UI_Tick();           /* 处理输入、更新动画、渲染dirty区域 */
    
    /* 其他任务：UART通信、触摸读取等 */
    UART_Process();
    Touch_Process();
    
    /* 帧率控制：目标30fps，约33ms每帧 */
    uint32_t elapsed = get_tick_ms() - last_tick;
    if(elapsed < 33) Delay_Ms(33 - elapsed);
    last_tick = get_tick_ms();
}
```

## 七、与现有驱动的衔接

### 7.1 保留的驱动

- `FMC/fmc_driver.c/h` -- 完全保留
- `SSD1963/ssd1963.c/h` -- 完全保留
- `lcd_config.c/h` -- 完全保留

### 7.2 删除/替换的文件

- `LVGL_LCD/lvgl_port.c/h` -- 删除，替换为 `MiniUI/miniui_*.c/h`
- `LVGL/` 整个目录 -- 删除
- `Common/Common/lv_conf.h` -- 删除

### 7.3 wvproj工程更新

- 删除所有 `LVGL/` 相关的源文件和include path
- 添加 `MiniUI/` 和 `MiniUI/font/` 到include paths
- 添加 `UI/`、`Games/`、`Apps/` 到include paths

## 八、开发顺序建议

1. **Phase 1：渲染引擎**（miniui_render.c + miniui_font.c）
   - 实现 draw_rect, draw_text（先不支持抗锯齿，用1bpp位图）
   - 验证能正确写入SSD1963 GRAM并显示
   
2. **Phase 2：Widget基类**（miniui_widget.c）
   - 实现ui_label_t和ui_button_t
   - 验证事件分发
   
3. **Phase 3：页面管理器**（miniui_page.c）
   - 实现页面栈、压栈/弹栈、回退导航
   - 实现脏区域追踪和局部刷新
   
4. **Phase 4：输入系统**（miniui_input.c）
   - 实现触摸手势识别（点击、滑动）
   - 实现鼠标/键盘事件映射
   
5. **Phase 5：主框架**（ui_main.c）
   - 侧边栏+内容区
   - 4个主页面骨架
   
6. **Phase 6：具体页面**（ui_home.c, ui_software.c, ui_models.c, ui_settings.c）
   
7. **Phase 7：应用与游戏页面**（app_*.c, game_*.c）
   
8. **Phase 8：抗锯齿字体+动画优化**
   - 替换1bpp字体为2bpp/4bpp抗锯齿
   - 添加页面切换动画

## 九、风险评估

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 抗锯齿字体渲染性能不足 | 30fps达不到 | 先使用1bpp位图字体，后续优化或降低刷新率到20fps |
| 脏区域合并算法复杂 | 开发时间延长 | 简化：每帧最多支持1个dirty区域（全屏刷新 fallback） |
| 游戏渲染性能 | 俄罗斯方块卡顿 | 游戏页面直接操作GRAM，不经过widget系统 |
| 触摸输入精度 | 点击不准 | 预留触摸校准接口，后期可添加 |
| 代码体积失控 | 自主库反而比LVGL大 | 严格控制widget数量，不实现通用布局系统 |
| 页面栈深度不足 | 深层导航崩溃 | 栈深度16层，超过时强制弹栈并打印警告 |
| 多输入源冲突 | 触摸+鼠标同时操作混乱 | 输入系统优先级：触摸 > 鼠标 > 键盘，互斥处理 |

## 十、预期成果

- **CodeFlash占用**：MiniUI库约30~50KB + 字体约20~40KB + 业务UI约30~50KB = 总计约80~140KB
- **SRAM占用**：无全屏帧缓冲，仅widget状态和临时缓冲 = 约10~30KB
- **编译时间**：秒级（<5秒）
- **可控性**：100%，每行代码都理解其用途
