# MiniUI 实现文档

> 自主轻量UI库，面向嵌入式RGB565彩色屏与单色屏的通用GUI框架

## 一、架构总览

### 1.1 分层设计

```
┌─────────────────────────────────────────────────────────────────┐
│  App Layer                                                      │
│  ui_main.c  ui_home.c  ui_apps.c  ui_models.c  ui_settings.c  │
│  ui_games.c  ui_app_common.c  app_*.c  game_*.c                │
├─────────────────────────────────────────────────────────────────┤
│  Page Manager (miniui_page.c)                                   │
│  页面栈(16层) · 生命周期回调 · 脏区域管理(256) · 合成渲染调度  │
├─────────────────────────────────────────────────────────────────┤
│  Widget Kit (miniui_widget.c)                                   │
│  Label · Button · Slider · Switch · Progress · Card · ListItem │
├─────────────────────────────────────────────────────────────────┤
│  Compositing Render Engine (miniui_render.c)                    │
│  行缓冲区合成 · 批量刷入(UI_COMPOSE_BATCH=8) · 自动裁剪       │
│  像素 · 线条 · 矩形 · 圆角矩形 · 圆形 · 文本 · 位图           │
├─────────────────────────────────────────────────────────────────┤
│  Display Driver (ssd1963.c / 自定义驱动)                        │
│  SSD1963 GRAM · FMC 8080并口 · 或任意屏驱动                    │
└─────────────────────────────────────────────────────────────────┘
```

辅助模块：
- **miniui_input.c** — 统一输入抽象（触摸/鼠标/键盘 → `ui_event_t`，手势识别，多点触控）
- **miniui_anim.c** — 动画系统（线性/ease-in/ease-out 插值）
- **miniui_color.c** — RGB565 颜色工具（混合/变暗/变亮）
- **miniui_font.c** — 字体引擎（1bpp/2bpp/4bpp 位图字体，抗锯齿）

### 1.2 显存策略

- **屏幕分辨率**：800×480，RGB565（可配置）
- **无全屏帧缓冲**：800×480×2 = 768KB 超出大多数MCU SRAM
- **脏矩形局部刷新**：维护最多 256 个 dirty region，每帧只重绘变化区域
- **多行合成缓冲区**：渲染引擎使用 `UI_COMPOSE_BATCH(8)` 行宽度的缓冲区（800×8×2 = 12.8KB），批量写入显示驱动
- **SetWindow 开销优化**：每次 SetWindow = 11 次 FMC 写（CMD 0x2A + 4 DATA + CMD 0x2B + 4 DATA + CMD 0x2C），批量合成将开销从每行 11 次降低到每 8 行 11 次

### 1.3 刷新流程

```
UI_Tick() 每10ms调用一次
    │
    ├─ 0. ui_input_tick() 处理时间手势（长按、点击超时）
    │
    ├─ 1. ui_input_poll() 处理输入事件
    │     └─ 事件分发：capture widget → 多点触控广播(DOWN/UP) → 侧边栏 → 页面widget
    │
    ├─ 2. ui_anim_tick(10) 更新动画
    │
    ├─ 3. page->on_update() 页面逻辑更新（游戏、时间动画等）
    │     └─ 标记脏区域
    │
    └─ 4. ui_page_draw() 合成渲染脏区域
          ├─ 遍历脏区域，按 UI_COMPOSE_BATCH 行分批
          ├─ 每批：begin_rect → fill_bg → sidebar → on_draw → widgets → flush_rows
          └─ 清空脏列表
```

## 二、目录结构

```
Common/Common/
├── MiniUI/                     # UI库核心
│   ├── miniui.h                # 总头文件
│   ├── miniui_types.h          # 基础类型定义（事件、颜色、几何、widget/page前向声明）
│   ├── miniui_color.h          # RGB565颜色工具
│   ├── miniui_font.h/c         # 字体引擎（glyph-based，支持抗锯齿）
│   ├── miniui_render.h/c       # 合成渲染引擎（行缓冲区+批量刷入）
│   ├── miniui_widget.h/c       # Widget系统（基类+自动脏跟踪+move/resize）
│   ├── miniui_page.h/c         # 页面管理器（合成渲染调度、256脏区域）
│   ├── miniui_input.h/c        # 输入系统（触摸手势、多点触控、鼠标、键盘）
│   ├── miniui_anim.h/c         # 动画系统
│   ├── ui_system.c             # 系统入口(UI_Init/UI_Tick/UI_FullRefresh)
│   └── font/
│       ├── font_montserrat_12.c/h
│       ├── font_montserrat_16.c/h
│       └── font_symbols.c/h
├── UI/                         # 业务UI页面
│   ├── ui_main.c/h             # 主框架(侧边栏+内容区)
│   ├── ui_home.c/h             # 首页
│   ├── ui_apps.c/h             # 应用网格
│   ├── ui_models.c/h           # 模块状态
│   ├── ui_settings.c/h         # 设置列表
│   ├── ui_games.c/h            # 游戏网格
│   └── ui_app_common.c/h       # 全屏应用/游戏通用模板
├── Apps/                       # 系统应用(app_music.c等)
├── Games/                      # 本地游戏(game_tetris.c等)
├── SSD1963/                    # SSD1963驱动
└── FMC/                        # FMC底层驱动
```

## 三、核心类型

### 3.1 基础类型（miniui_types.h）

```c
typedef uint16_t ui_color_t;    /* RGB565 */

typedef struct { int16_t x, y; } ui_point_t;

typedef struct { int16_t x, y, w, h; } ui_rect_t;

/* Glyph-based font (supports anti-aliasing) */
typedef struct {
    const ui_glyph_t *glyphs;   /* Glyph descriptor array */
    uint16_t glyph_count;
    uint8_t height;
    int8_t baseline;
    uint8_t bpp;                /* 1/2/4 bpp */
} ui_font_t;
```

### 3.2 事件类型（miniui_types.h）

```c
/* Input source */
typedef enum {
    UI_INPUT_TOUCH    = 0,
    UI_INPUT_MOUSE    = 1,
    UI_INPUT_KEYBOARD = 2,
    UI_INPUT_CORE_KEY = 3,
} ui_input_source_t;

/* Event types */
typedef enum {
    /* Touch / Mouse pointer events */
    UI_EVENT_NONE         = 0,
    UI_EVENT_DOWN,              /* Finger/mouse pressed */
    UI_EVENT_UP,                /* Released (short press) */
    UI_EVENT_MOVE,              /* Moved while pressed */
    UI_EVENT_CLICK,             /* Single tap completed */
    UI_EVENT_DOUBLE_CLICK,      /* Two rapid clicks */
    UI_EVENT_LONG_PRESS,        /* Press & hold (first trigger) */
    UI_EVENT_LONG_PRESS_REPEAT, /* Continued hold, repeats */
    UI_EVENT_SWIPE_UP/DOWN/LEFT/RIGHT, /* Swipe gestures */

    /* Keyboard events */
    UI_EVENT_KEY_DOWN/UP,       /* Key pressed/released */
    UI_EVENT_KEY_CLICK,         /* Key click */
    UI_EVENT_KEY_DOUBLE_CLICK,  /* Key double click */
    UI_EVENT_KEY_LONG_PRESS,    /* Key long press */
    UI_EVENT_KEY_LONG_REPEAT,   /* Key long press repeat */

    /* Logical key events */
    UI_EVENT_KEY_UP_ARROW/DOWN_ARROW/LEFT_ARROW/RIGHT_ARROW,
    UI_EVENT_KEY_OK,
    UI_EVENT_KEY_BACK,

    UI_EVENT_PRESS_CANCEL,      /* Press cancelled (e.g. swipe) */
} ui_event_type_t;

/* Unified event structure */
typedef struct {
    ui_event_type_t type;
    ui_input_source_t source;
    ui_point_t pos;             /* Touch/mouse position */
    ui_point_t delta;           /* Movement delta */
    uint8_t touch_id;           /* 0~4 for multi-touch, UI_TOUCH_ID_NONE otherwise */
    uint8_t key_code;
    uint8_t key_modifiers;
    uint8_t mouse_buttons;
    int8_t scroll_delta;
} ui_event_t;
```

### 3.3 页面标志

```c
#define UI_PAGE_FLAG_FULLSCREEN  (1 << 0)  /* 全屏模式，隐藏侧边栏 */
#define UI_PAGE_FLAG_HAS_BACK    (1 << 1)  /* 页面有返回功能 */
```

## 四、合成渲染引擎 API

### 4.1 初始化与驱动接口

```c
void ui_render_init(void);

/* Portable display driver interface */
typedef struct {
    void (*set_window)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    void (*write_data16)(uint16_t data);
    void (*write_buffer)(const uint16_t *buf, uint32_t len);
    void (*clear)(uint16_t color);
} ui_display_driver_t;

void ui_render_set_driver(const ui_display_driver_t *driver);
```

### 4.2 合成引擎 API（由页面管理器调用）

```c
/* Set compositing target rectangle (typically UI_COMPOSE_BATCH rows) */
void ui_render_begin_rect(const ui_rect_t *rect);

/* Fill line buffer range with solid color */
void ui_render_fill_line_buf(int16_t x_start, int16_t width, ui_color_t color);

/* Flush multiple composited scanlines to GRAM */
void ui_render_flush_rows(int16_t y_start, int16_t row_count,
                          int16_t x_start, int16_t width);

/* Get/set line buffer pixels */
ui_color_t* ui_render_get_line_buf(void);
void ui_render_set_line_pixel(int16_t x, ui_color_t color);
```

### 4.3 绘图原语

所有绘图原语写入行缓冲区（不直接写 GRAM），自动裁剪到当前合成目标矩形：

```c
void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color);
void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color);
void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color);
void ui_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, ui_color_t color);
void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color);
void ui_draw_rect(const ui_rect_t *rect, ui_color_t fill, ui_color_t border, int16_t thickness);
void ui_draw_fill_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t color);
void ui_draw_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t fill, ui_color_t border, int16_t thickness);
void ui_draw_fill_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color);
void ui_draw_circle_border(int16_t cx, int16_t cy, int16_t r, ui_color_t color, int16_t thickness);
void ui_draw_icon(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, ui_color_t color);
```

### 4.4 文本渲染

```c
void ui_draw_text(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color);
void ui_draw_text_bg(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg);
void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, uint8_t align);
int16_t ui_text_width(const char *text, const ui_font_t *font);
```

### 4.5 全局刷新

```c
void ui_full_refresh(void);       /* 全屏清屏并重绘 */
void ui_screen_clear(ui_color_t color);  /* 用纯色清屏（直接写 GRAM） */
```

## 五、Widget 系统

### 5.1 基类

```c
typedef struct ui_widget {
    ui_rect_t rect;            /* 当前位置和尺寸 */
    ui_rect_t prev_rect;       /* 上一帧位置（用于 move/resize 脏标记） */
    uint16_t flags;            /* 可见/使能/脏/按下/焦点/不透明 */
    ui_color_t bg_color;       /* 背景色 */
    void (*draw_cb)(struct ui_widget *w, ui_rect_t *dirty);
    void (*event_cb)(struct ui_widget *w, ui_event_t *e);
    void *user_data;
} ui_widget_t;
```

**标志位**：
```c
#define UI_WIDGET_FLAG_VISIBLE   (1 << 0)
#define UI_WIDGET_FLAG_ENABLED   (1 << 1)
#define UI_WIDGET_FLAG_DIRTY     (1 << 2)
#define UI_WIDGET_FLAG_PRESSED   (1 << 3)
#define UI_WIDGET_FLAG_FOCUS     (1 << 4)
#define UI_WIDGET_FLAG_OPAQUE    (1 << 5)  /* 完全不透明，渲染器可跳过背景填充 */
```

**通用操作**：
```c
void ui_widget_init(ui_widget_t *w, const ui_rect_t *rect);
void ui_widget_draw(ui_widget_t *w, ui_rect_t *dirty);
void ui_widget_event(ui_widget_t *w, ui_event_t *e);
void ui_widget_set_visible(ui_widget_t *w, bool visible);
void ui_widget_invalidate(ui_widget_t *w);
void ui_widget_move(ui_widget_t *w, int16_t x, int16_t y);   /* 自动标记旧+新位置脏 */
void ui_widget_resize(ui_widget_t *w, int16_t w, int16_t h);  /* 自动标记旧+新位置脏 */
bool ui_widget_hit_test(const ui_widget_t *w, int16_t x, int16_t y);
```

### 5.2 Label

```c
typedef struct {
    ui_widget_t base;
    const char *text;
    const ui_font_t *font;
    ui_color_t text_color;
    uint8_t align;             /* 0=左 1=中 2=右 */
} ui_label_t;

void ui_label_init(ui_label_t *lbl, const ui_rect_t *rect,
                   const char *text, const ui_font_t *font, ui_color_t color);
void ui_label_set_text(ui_label_t *lbl, const char *text);
```

> **注意**：`ui_label_set_text` 存储的是指针，不复制字符串。传入的字符串必须是静态字符串或长期有效的缓冲区。

### 5.3 Button

```c
typedef struct {
    ui_widget_t base;
    const char *text;
    const ui_font_t *font;
    ui_color_t text_color;
    ui_color_t fill_color;
    ui_color_t press_color;
    uint8_t radius;
    bool pressed;
    const uint8_t *icon_bitmap;
    int16_t icon_w, icon_h;
} ui_button_t;

void ui_button_init(ui_button_t *btn, const ui_rect_t *rect,
                    const char *text, const ui_font_t *font,
                    ui_color_t fill, ui_color_t text_color);
```

### 5.4 Slider / Switch / Progress / Card / ListItem

结构体定义和初始化函数与之前版本相同，详见 `miniui_widget.h`。

## 六、页面管理器

### 6.1 页面结构

```c
typedef struct ui_page {
    const char *name;
    uint32_t id;
    ui_rect_t content_rect;
    ui_widget_t **widgets;
    uint16_t widget_count;
    ui_page_enter_cb_t on_enter;
    ui_page_exit_cb_t on_exit;
    ui_page_draw_cb_t on_draw;      /* 自定义绘制（合成渲染器回调） */
    ui_page_back_cb_t on_back;
    ui_page_update_cb_t on_update;  /* 每帧逻辑更新（游戏、动画） */
    int16_t scroll_y;
    uint8_t flags;
} ui_page_t;
```

### 6.2 页面栈操作

```c
void ui_page_init(void);
void ui_page_register(ui_page_t *page);
void ui_page_push(ui_page_t *page);
void ui_page_pop(void);
void ui_page_switch(ui_page_t *page);
ui_page_t* ui_page_current(void);
bool ui_page_can_go_back(void);
```

**栈深度**：最大 16 层。

### 6.3 脏区域管理

```c
void ui_page_invalidate(const ui_rect_t *rect);
void ui_page_invalidate_all(void);
const ui_dirty_list_t *ui_page_get_dirty_list(void);
void ui_page_clear_dirty(void);
void ui_page_draw(void);
```

**脏区域上限**：256 个（`UI_MAX_DIRTY_REGIONS`）。超出时退化为全屏刷新。

### 6.4 合成渲染流程

`ui_page_draw()` 对所有页面类型（UI/Apps/Games）使用统一的合成渲染路径：

```c
for (each dirty region) {
    for (y = dirty.y; y < dirty.y + dirty.h; y += UI_COMPOSE_BATCH) {
        batch_h = min(UI_COMPOSE_BATCH, remaining_rows);
        ui_render_begin_rect(&batch_target);
        ui_render_fill_line_buf(dirty.x, dirty.w, bg_color);
        sidebar_draw(&batch_target);        // 非全屏页面
        page->on_draw(page, &batch_target); // 页面自定义绘制
        // 绘制相交的 widgets
        ui_render_flush_rows(y, batch_h, dirty.x, dirty.w);
    }
}
```

### 6.5 侧边栏

```c
typedef void (*ui_sidebar_draw_cb_t)(ui_rect_t *dirty);
typedef bool (*ui_sidebar_event_cb_t)(ui_event_t *e);

void ui_page_set_sidebar_callbacks(ui_sidebar_draw_cb_t draw, ui_sidebar_event_cb_t event);
void ui_page_set_sidebar_width(int16_t width);
```

侧边栏在脏区域与侧边栏区域重叠时自动重绘，每批行只绘制一次。

### 6.6 页面辅助函数

```c
void ui_page_struct_init(ui_page_t *page, const char *name, uint32_t id);
void ui_page_struct_init_fullscreen(ui_page_t *page, const char *name, uint32_t id);
void ui_page_set_widgets(ui_page_t *page, ui_widget_t **widgets, uint16_t count);
void ui_page_set_callbacks(ui_page_t *page, ...);
void ui_page_set_update_cb(ui_page_t *page, ui_page_update_cb_t update);
```

## 七、输入系统

### 7.1 初始化与轮询

```c
void ui_input_init(void);
void ui_input_tick(void);       /* 时间手势处理（长按、点击超时） */
ui_event_t* ui_input_poll(void); /* 从事件队列取出一个事件 */
```

### 7.2 触摸输入

```c
void ui_input_feed_touch(int16_t x, int16_t y, bool pressed);
void ui_input_feed_touch_multi(uint8_t touch_id, int16_t x, int16_t y, bool pressed);
```

触摸手势识别：
- **点击**：按下和释放位置距离 < 20px 且时间 < 300ms → `UI_EVENT_DOWN` + `UI_EVENT_UP` + `UI_EVENT_CLICK`
- **双击**：两次快速点击 → `UI_EVENT_DOUBLE_CLICK`
- **长按**：按住超过阈值 → `UI_EVENT_LONG_PRESS`，之后重复 `UI_EVENT_LONG_PRESS_REPEAT`
- **滑动**：距离 > 20px → `UI_EVENT_SWIPE_*`（同时发送 `UI_EVENT_PRESS_CANCEL`）
- **拖动**：按下后移动 → `UI_EVENT_MOVE`

### 7.3 键盘/鼠标输入

```c
void ui_input_feed_key(uint8_t key_code, bool pressed, uint8_t modifiers);
void ui_input_feed_mouse(int16_t x, int16_t y, uint8_t buttons, int8_t scroll);
```

### 7.4 事件捕获

```c
void ui_input_set_capture(ui_widget_t *w, uint8_t touch_id);
ui_widget_t* ui_input_get_capture(void);
uint8_t ui_input_get_capture_touch_id(void);
```

- DOWN 时自动设置 capture widget 和 touch_id
- UP/CLICK/DOUBLE_CLICK 时释放 capture（仅匹配 touch_id 或无触摸）
- 滑动手势触发 PRESS_CANCEL 并释放 capture

### 7.5 事件分发顺序（UI_Tick 中）

1. **Capture widget**：如果存在，所有事件发给 capture widget
   - 多点触控广播：非主触点的 DOWN/UP 广播给所有 widget（**MOVE 不广播**）
2. **KEY_BACK**：如果可返回，执行 `ui_page_pop()`
3. **侧边栏**：非全屏页面，侧边栏事件回调优先
4. **指针事件**：从后往前遍历 widget，第一个命中测试通过的处理事件
   - DOWN 时设置 capture
5. **键盘事件**：广播给所有 widget
6. **其他事件**：广播给所有 widget

### 7.6 事件队列

32 个事件的环形缓冲区（FIFO），溢出时丢弃最早的事件。

## 八、动画系统

### 8.1 缓动类型

```c
typedef enum {
    UI_EASE_LINEAR,
    UI_EASE_OUT,       /* 减速 */
    UI_EASE_IN,        /* 加速 */
} ui_ease_type_t;
```

### 8.2 动画 API

```c
void ui_anim_init(void);
ui_anim_t* ui_anim_start(int32_t start, int32_t end, uint16_t duration_ms,
                         ui_ease_type_t ease, ui_anim_update_cb_t cb, void *user_data);
void ui_anim_stop(ui_anim_t *anim);
void ui_anim_tick(uint16_t delta_ms);
```

**最大同时动画数**：8 个。

## 九、颜色系统

### 9.1 RGB565 宏

```c
#define UI_RGB565(r, g, b)  (((r)&0xF8)<<8 | ((g)&0xFC)<<3 | ((b)&0xF8)>>3)
```

### 9.2 预定义颜色

```c
#define UI_COLOR_TRANSPARENT  0x0000
#define UI_COLOR_BLACK        0x0000
#define UI_COLOR_WHITE        0xFFFF
#define UI_COLOR_BG_MAIN      UI_RGB565(0xF5, 0xF5, 0xF5)
#define UI_COLOR_PRIMARY      UI_RGB565(0x7E, 0xC8, 0xC8)
#define UI_COLOR_SIDEBAR_BG   UI_RGB565(0xF5, 0xF5, 0xF5)
#define UI_COLOR_SIDEBAR_SEL  UI_RGB565(0xB8, 0xE0, 0xD2)
#define UI_COLOR_TEXT_DARK    UI_RGB565(0x4A, 0x4A, 0x4A)
#define UI_COLOR_TEXT_LIGHT   UI_RGB565(0x9E, 0x9E, 0x9E)
```

### 9.3 颜色工具

```c
ui_color_t ui_color_blend(ui_color_t fg, ui_color_t bg, uint8_t alpha);
ui_color_t ui_color_darken(ui_color_t color, uint8_t amount);
ui_color_t ui_color_lighten(ui_color_t color, uint8_t amount);
```

## 十、字体系统

### 10.1 字体数据格式

使用 glyph-based 字体（非统一宽度位图），每个字符有独立的 glyph 描述符：

```c
typedef struct {
    uint16_t unicode;
    uint8_t width, height;
    int8_t x_offset, y_offset;
    uint8_t advance;
    const uint8_t *bitmap;
} ui_glyph_t;
```

### 10.2 抗锯齿渲染

- **1bpp**：无抗锯齿，alpha=0或1
- **2bpp**：4级灰度alpha混合
- **4bpp**：16级灰度alpha混合
- 透明背景时，抗锯齿像素直接绘制前景色

## 十一、系统入口

### 11.1 UI_Init()

初始化所有 MiniUI 模块、注册页面和应用、切换到首页并全屏刷新。

### 11.2 UI_Tick()

```c
void UI_Tick(void)
{
    ui_input_tick();                    // 时间手势

    ui_event_t *e = ui_input_poll();
    while (e) {
        // 事件分发（capture → multi-touch broadcast → sidebar → widgets）
        e = ui_input_poll();
    }

    ui_anim_tick(UI_TICK_MS);          // UI_TICK_MS = 10

    // Per-frame page update
    if (page && page->on_update) page->on_update(page);

    // Compositing render
    ui_page_draw();
}
```

### 11.3 主循环

```c
while (1) {
    Touch_Scan();
    SSD1963_WaitVSync();
    UI_Tick();
    UART_Module_Poll();
    // 帧率限制...
}
```

## 十二、业务UI实现

### 12.1 主框架（ui_main.c）

- **侧边栏**：宽200px，背景 `#F5F5F5`
  - 5个菜单项：Home / Apps / Games / Models / Settings
  - 选中项高亮：背景 `#B8E0D2`，左侧4px竖条 `#7EC8C8`

### 12.2 全屏应用/游戏模板（ui_app_common.c）

- flags：`UI_PAGE_FLAG_FULLSCREEN`
- 顶部标题栏（40px）：返回按钮 "<" + 应用名称
- `ui_app_page_draw()` 绘制标题栏背景和 widgets
- 游戏覆盖 `on_draw` 时需自行绘制标题栏背景

## 十三、显示驱动接口

通过 `ui_display_driver_t` 结构体注册驱动，移植时只需实现 4 个函数：

```c
ui_display_driver_t ssd1963_driver = {
    .set_window = SSD1963_WriteRect,
    .write_data16 = SSD1963_WriteData,
    .write_buffer = SSD1963_WriteBuffer,
    .clear = SSD1963_Clear,
};
ui_render_set_driver(&ssd1963_driver);
```

## 十四、资源占用

| 项目 | 占用 |
|------|------|
| CodeFlash (MiniUI库) | ~30~50KB |
| CodeFlash (字体数据) | ~20~40KB |
| CodeFlash (业务UI) | ~30~50KB |
| SRAM (widget状态) | ~10~30KB |
| SRAM (合成缓冲区) | ~12.8KB (800×8×2) |
| SRAM (全屏帧缓冲) | **0** (无需) |

## 十五、移植到墨水屏/单色屏

### 15.1 移植步骤

1. 实现 `ui_display_driver_t` 的 4 个函数
2. 调用 `ui_render_set_driver()` 注册
3. 调整颜色方案（`MINIUI_MONOCHROME` 条件编译）
4. 禁用动画
5. 使用 1bpp 字体
6. 降低刷新率（`UI_TICK_MS` 改为 200~500）
7. 添加定期全刷防残影

## 十六、已知限制与注意事项

1. **Label文本指针**：`ui_label_set_text()` 不复制字符串
2. **页面栈溢出**：超过16层时，最底层页面被覆盖
3. **脏区域数量**：超过256个时退化为全屏刷新
4. **抗锯齿背景**：需要已知背景色才能正确混合
5. **动画浮点运算**：缓动函数使用 `float`，无FPU的MCU上可能较慢
6. **线程安全**：MiniUI非线程安全
7. **多点触控MOVE**：MOVE事件不广播给所有widget，只发给capture widget
8. **游戏标题栏**：游戏覆盖on_draw时必须自行绘制标题栏背景
