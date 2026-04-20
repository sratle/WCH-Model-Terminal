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
│  页面栈(16层) · 生命周期回调 · 脏区域管理 · 侧边栏调度          │
├─────────────────────────────────────────────────────────────────┤
│  Widget Kit (miniui_widget.c)                                   │
│  Label · Button · Slider · Switch · Progress · Card · ListItem │
├─────────────────────────────────────────────────────────────────┤
│  Render Engine (miniui_render.c)                                │
│  像素 · 线条 · 矩形 · 圆角矩形 · 圆形 · 文本 · 位图           │
├─────────────────────────────────────────────────────────────────┤
│  Display Driver (ssd1963.c / 自定义驱动)                        │
│  SSD1963 GRAM · FMC 8080并口 · 或任意屏驱动                    │
└─────────────────────────────────────────────────────────────────┘
```

辅助模块：
- **miniui_input.c** — 统一输入抽象（触摸/鼠标/键盘 → `ui_event_t`）
- **miniui_anim.c** — 动画系统（线性/ease-in/ease-out 插值）
- **miniui_color.c** — RGB565 颜色工具（混合/变暗/变亮）
- **miniui_font.c** — 字体引擎（1bpp/2bpp/4bpp 位图字体）

### 1.2 显存策略

- **屏幕分辨率**：800×480，RGB565（可配置）
- **无全屏帧缓冲**：800×480×2 = 768KB 超出大多数MCU SRAM
- **脏矩形局部刷新**：维护最多 8 个 dirty region，每帧只重绘变化区域
- **行缓冲区**：渲染引擎使用一行宽度的缓冲区（800×2 = 1.6KB），批量写入显示驱动

### 1.3 刷新流程

```
UI_Tick() 每33ms调用一次（~30fps）
    │
    ├─ 1. ui_input_poll() 处理输入事件
    │     └─ 事件分发：capture widget → 侧边栏 → 页面widget
    │
    ├─ 2. ui_anim_tick(33) 更新动画
    │     └─ 动画回调中标记脏区域
    │
    └─ 3. ui_page_draw() 渲染脏区域
          ├─ 合并重叠脏区域
          ├─ 侧边栏脏区域 → 调用 sidebar_draw()
          └─ 逐个脏区域：设置裁剪 → 遍历widget → draw
```

## 二、目录结构

```
Common/Common/
├── MiniUI/                     # UI库核心
│   ├── miniui.h                # 总头文件
│   ├── miniui_types.h          # 基础类型定义
│   ├── miniui_color.h/c        # RGB565颜色工具
│   ├── miniui_font.h/c         # 字体引擎
│   ├── miniui_render.h/c       # 渲染引擎
│   ├── miniui_widget.h/c       # Widget系统
│   ├── miniui_page.h/c         # 页面管理器
│   ├── miniui_input.h/c        # 输入系统
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
│   └── ui_app_common.c/h       # 全屏应用通用模板
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

typedef struct {
    const uint8_t *bitmap;       /* 灰度位图数据 */
    const uint16_t *unicode_list;/* 码点列表 */
    uint16_t unicode_count;
    uint8_t height;              /* 字体高度 */
    int8_t baseline;             /* 基线偏移 */
    uint8_t bpp;                 /* 每像素位数: 1/2/4 */
} ui_font_t;
```

### 3.2 事件类型（miniui_types.h）

```c
typedef enum {
    UI_EVENT_NONE,
    UI_EVENT_PRESS,          /* 按下 */
    UI_EVENT_RELEASE,        /* 释放 */
    UI_EVENT_DRAG,           /* 拖动 */
    UI_EVENT_SWIPE_UP,       /* 上滑 */
    UI_EVENT_SWIPE_DOWN,     /* 下滑 */
    UI_EVENT_SWIPE_LEFT,     /* 左滑 */
    UI_EVENT_SWIPE_RIGHT,    /* 右滑 */
    UI_EVENT_KEY_UP,         /* 键盘↑ */
    UI_EVENT_KEY_DOWN,       /* 键盘↓ */
    UI_EVENT_KEY_LEFT,       /* 键盘← */
    UI_EVENT_KEY_RIGHT,      /* 键盘→ */
    UI_EVENT_KEY_OK,         /* 确认/回车 */
    UI_EVENT_KEY_BACK,       /* 返回/ESC */
} ui_event_type_t;

typedef struct {
    ui_event_type_t type;
    ui_point_t pos;          /* 触摸/鼠标坐标 */
    ui_point_t delta;        /* 拖动偏移 */
} ui_event_t;
```

### 3.3 页面标志

```c
#define UI_PAGE_FLAG_FULLSCREEN  0x01  /* 全屏模式，隐藏侧边栏 */
```

## 四、渲染引擎 API

### 4.1 初始化与裁剪

```c
void ui_render_init(void);                        /* 初始化渲染引擎 */
void ui_render_set_clip(const ui_rect_t *rect);   /* 设置裁剪区域 */
void ui_render_reset_clip(void);                   /* 重置裁剪为全屏 */
```

### 4.2 绘图原语

```c
void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color);
void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color);
void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color);
void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color);
void ui_draw_round_rect(const ui_rect_t *rect, uint8_t radius,
                        ui_color_t fill, ui_color_t border);
void ui_draw_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color);
void ui_draw_bitmap(int16_t x, int16_t y, const uint8_t *data,
                    int16_t w, int16_t h, ui_color_t fg, ui_color_t bg);
```

### 4.3 文本渲染

```c
void ui_draw_text(int16_t x, int16_t y, const char *text,
                  const ui_font_t *font, ui_color_t color);
void ui_draw_text_bg(int16_t x, int16_t y, const char *text,
                     const ui_font_t *font, ui_color_t color, ui_color_t bg);
void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text,
                          const ui_font_t *font, ui_color_t color, uint8_t align);
void ui_draw_text_in_rect_bg(const ui_rect_t *rect, const char *text,
                             const ui_font_t *font, ui_color_t color,
                             ui_color_t bg, uint8_t align);
int16_t ui_text_width(const char *text, const ui_font_t *font);
```

**align 参数**：0=左对齐，1=居中，2=右对齐

**抗锯齿渲染**：
- 1bpp 字体：alpha=0或1，直接绘制前景色或背景色
- 2bpp/4bpp 字体：根据 alpha 值混合前景色和背景色
- 当 `bg == UI_COLOR_TRANSPARENT` 时，抗锯齿像素直接绘制前景色（无混合）

### 4.4 全局刷新

```c
void ui_full_refresh(void);       /* 全屏清屏并重绘 */
void ui_screen_clear(ui_color_t color);  /* 用纯色清屏 */
```

## 五、Widget 系统

### 5.1 基类

```c
typedef struct ui_widget {
    ui_rect_t rect;            /* 位置和尺寸 */
    uint16_t flags;            /* 可见/使能/脏标记 */
    ui_color_t bg_color;       /* 背景色 */
    void (*draw_cb)(struct ui_widget *w, const ui_rect_t *dirty);
    void (*event_cb)(struct ui_widget *w, const ui_event_t *e);
    void *user_data;           /* 用户数据 */
} ui_widget_t;
```

**标志位**：
```c
#define UI_WIDGET_FLAG_VISIBLE   0x0001
#define UI_WIDGET_FLAG_ENABLED   0x0002
#define UI_WIDGET_FLAG_DIRTY     0x0004
```

**通用操作**：
```c
void ui_widget_init(ui_widget_t *w, const ui_rect_t *rect);
void ui_widget_draw(ui_widget_t *w, const ui_rect_t *dirty);
void ui_widget_event(ui_widget_t *w, const ui_event_t *e);
void ui_widget_set_visible(ui_widget_t *w, bool visible);
void ui_widget_invalidate(ui_widget_t *w);
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

> **注意**：`ui_label_set_text` 存储的是指针，不复制字符串。传入的字符串必须是静态字符串或长期有效的缓冲区，不能使用局部变量。

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

按钮点击通过 `base.event_cb` 回调处理，在回调中检查 `e->type == UI_EVENT_RELEASE`。

### 5.4 Slider

```c
typedef struct {
    ui_widget_t base;
    int32_t min_val, max_val, value;
    ui_color_t track_color;
    ui_color_t fill_color;
    ui_color_t knob_color;
    bool dragging;
} ui_slider_t;

void ui_slider_init(ui_slider_t *s, const ui_rect_t *rect,
                    int32_t min, int32_t max, int32_t value);
```

### 5.5 Switch

```c
typedef struct {
    ui_widget_t base;
    bool is_on;
    ui_color_t track_on_color;
    ui_color_t track_off_color;
    ui_color_t knob_color;
} ui_switch_t;

void ui_switch_init(ui_switch_t *sw, const ui_rect_t *rect, bool on);
```

### 5.6 Progress

```c
typedef struct {
    ui_widget_t base;
    int32_t min_val, max_val, value;
    ui_color_t track_color;
    ui_color_t fill_color;
    uint8_t radius;
} ui_progress_t;

void ui_progress_init(ui_progress_t *p, const ui_rect_t *rect,
                      int32_t min, int32_t max, int32_t value);
```

### 5.7 Card

```c
typedef struct {
    ui_widget_t base;
    ui_color_t border_color;
    uint8_t radius;
    uint8_t shadow_offset;
} ui_card_t;

void ui_card_init(ui_card_t *card, const ui_rect_t *rect,
                  ui_color_t fill, ui_color_t border);
```

### 5.8 ListItem

```c
typedef struct {
    ui_widget_t base;
    const char *text;
    const ui_font_t *font;
    ui_color_t text_color;
    ui_widget_t *right_widget;  /* 右侧控件(如Slider/Switch) */
    bool show_divider;
} ui_list_item_t;

void ui_list_item_init(ui_list_item_t *item, const ui_rect_t *rect,
                       const char *text, const ui_font_t *font, ui_color_t color);
```

## 六、页面管理器

### 6.1 页面结构

```c
typedef struct ui_page {
    const char *name;
    uint32_t id;
    ui_rect_t content_rect;     /* 内容区矩形(排除侧边栏) */
    ui_widget_t **widgets;      /* widget数组 */
    uint16_t widget_count;
    void (*on_enter)(void);     /* 进入回调 */
    void (*on_exit)(void);      /* 退出回调 */
    void (*on_draw)(struct ui_page *page, const ui_rect_t *dirty); /* 自定义绘制 */
    void (*on_back)(void);      /* 返回键回调 */
    int16_t scroll_y;
    uint8_t flags;              /* UI_PAGE_FLAG_FULLSCREEN 等 */
} ui_page_t;
```

### 6.2 页面栈操作

```c
void ui_page_init(void);
void ui_page_push(ui_page_t *page);     /* 压栈进入新页面 */
void ui_page_pop(void);                 /* 弹栈返回上一页 */
void ui_page_switch(ui_page_t *page);   /* 切换页面(清空栈) */
ui_page_t* ui_page_current(void);       /* 获取当前页面 */
bool ui_page_can_go_back(void);         /* 是否可以返回 */
```

**栈深度**：最大 16 层。

### 6.3 脏区域管理

```c
void ui_page_invalidate(const ui_rect_t *rect);  /* 标记脏区域 */
void ui_page_invalidate_all(void);                /* 标记全屏脏 */
void ui_page_draw(void);                          /* 渲染所有脏区域 */
```

**脏区域合并**：最多 8 个 dirty region。当超过 8 个时，合并为全屏刷新。

### 6.4 侧边栏

```c
typedef void (*ui_sidebar_draw_cb_t)(const ui_rect_t *dirty);
typedef bool (*ui_sidebar_event_cb_t)(const ui_event_t *e);

void ui_page_set_sidebar_callbacks(ui_sidebar_draw_cb_t draw,
                                   ui_sidebar_event_cb_t event);
void ui_page_set_sidebar_width(int16_t width);
ui_sidebar_event_cb_t ui_page_get_sidebar_event_cb(void);
```

侧边栏在脏区域与侧边栏区域重叠时自动重绘，且每帧只绘制一次。

### 6.5 页面结构体初始化

```c
void ui_page_struct_init(ui_page_t *page, const char *name, uint32_t id);
```

此函数自动设置 `content_rect`，根据侧边栏宽度计算内容区位置。

## 七、输入系统

### 7.1 初始化与轮询

```c
void ui_input_init(void);
ui_event_t* ui_input_poll(void);  /* 从事件队列取出一个事件 */
```

### 7.2 触摸输入

```c
void ui_input_touch_raw(int16_t x, int16_t y, bool pressed);
```

触摸手势识别：
- **点击**：按下和释放位置距离 < 20px → `UI_EVENT_PRESS` + `UI_EVENT_RELEASE`
- **滑动**：距离 > 20px → `UI_EVENT_SWIPE_*`（不再发送 RELEASE）
- **拖动**：按下后移动 → `UI_EVENT_DRAG`

### 7.3 键盘输入

```c
void ui_input_key(ui_event_type_t key_event);
```

直接映射为 `UI_EVENT_KEY_*` 事件。

### 7.4 事件捕获

```c
void ui_input_set_capture(ui_widget_t *w);  /* 设置捕获widget */
ui_widget_t* ui_input_get_capture(void);     /* 获取捕获widget */
```

按下时自动设置捕获，释放时自动清除。捕获期间所有事件发送给捕获widget。

### 7.5 事件分发顺序

1. **捕获widget**：如果存在，事件直接发给捕获widget
2. **KEY_BACK**：如果可返回，执行 `ui_page_pop()`
3. **侧边栏**：非全屏页面，侧边栏事件回调优先
4. **页面widget**：从后往前遍历，第一个命中测试通过的widget处理事件
5. **广播**：键盘等非坐标事件广播给所有widget

## 八、动画系统

### 8.1 缓动类型

```c
typedef enum {
    UI_EASE_LINEAR,    /* 线性 */
    UI_EASE_OUT,       /* 减速 */
    UI_EASE_IN,        /* 加速 */
} ui_ease_type_t;
```

### 8.2 动画结构

```c
typedef struct {
    int32_t start, end, current;
    uint16_t duration_ms, elapsed_ms;
    ui_ease_type_t ease_type;
    bool active;
    ui_anim_update_cb_t update_cb;  /* void (*)(int32_t value, void *user_data) */
    void *user_data;
} ui_anim_t;
```

### 8.3 动画 API

```c
void ui_anim_init(void);
ui_anim_t* ui_anim_start(int32_t start, int32_t end, uint16_t duration_ms,
                         ui_ease_type_t ease, ui_anim_update_cb_t cb, void *user_data);
void ui_anim_stop(ui_anim_t *anim);
void ui_anim_tick(uint16_t delta_ms);
bool ui_anim_is_active(void);
```

**最大同时动画数**：8 个（`UI_MAX_ANIMATIONS`）。

### 8.4 缓动函数

```c
int32_t ui_ease_linear(int32_t start, int32_t end, float t);
int32_t ui_ease_out(int32_t start, int32_t end, float t);   /* t' = 1-(1-t)^2 */
int32_t ui_ease_in(int32_t start, int32_t end, float t);    /* t' = t^2 */
```

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
#define UI_COLOR_BG_MAIN      UI_RGB565(0xF5, 0xF5, 0xF5)  /* 主背景 #F5F5F5 */
#define UI_COLOR_PRIMARY      UI_RGB565(0x7E, 0xC8, 0xC8)  /* 主色 #7EC8C8 */
#define UI_COLOR_SIDEBAR_BG   UI_RGB565(0xF5, 0xF5, 0xF5)  /* 侧边栏背景 */
#define UI_COLOR_SIDEBAR_SEL  UI_RGB565(0xB8, 0xE0, 0xD2)  /* 侧边栏选中 */
#define UI_COLOR_TEXT_DARK    UI_RGB565(0x4A, 0x4A, 0x4A)  /* 深色文字 */
#define UI_COLOR_TEXT_LIGHT   UI_RGB565(0x9E, 0x9E, 0x9E)  /* 浅色文字 */
```

### 9.3 颜色工具

```c
ui_color_t ui_color_blend(ui_color_t fg, ui_color_t bg, uint8_t alpha);
ui_color_t ui_color_darken(ui_color_t color, uint8_t amount);
ui_color_t ui_color_lighten(ui_color_t color, uint8_t amount);
```

## 十、字体系统

### 10.1 字体数据格式

字体以位图数组形式编译进 CodeFlash：

```c
const ui_font_t font_montserrat_16 = {
    .bitmap = font_montserrat_16_bitmap,
    .unicode_list = font_montserrat_16_unicode_list,
    .unicode_count = 95,
    .height = 16,
    .baseline = 12,
    .bpp = 1,    /* 1bpp = 无抗锯齿, 2/4bpp = 抗锯齿 */
};
```

### 10.2 字体渲染

- **1bpp**：每个像素1位，0=背景色，1=前景色
- **2bpp**：每个像素2位，4级灰度alpha混合
- **4bpp**：每个像素4位，16级灰度alpha混合

### 10.3 字符查找

使用线性搜索 `unicode_list`，找到码点后计算位图偏移。ASCII字符连续排列，查找效率为 O(n)，对95个ASCII字符足够快。

## 十一、系统入口

### 11.1 UI_Init()

```c
void UI_Init(void)
{
    LCD_Init(LCD_ORIENTATION_NORMAL);
    SSD1963_Init();

    ui_render_init();
    ui_page_init();
    ui_input_init();
    ui_anim_init();

    ui_page_set_sidebar_callbacks(ui_main_draw_sidebar, ui_main_handle_event);
    ui_page_set_sidebar_width(SIDEBAR_WIDTH);

    ui_main_init();
    ui_home_init();
    ui_apps_init();
    ui_models_init();
    ui_settings_init();
    ui_games_init();

    apps_init_all();
    games_init_all();

    ui_page_switch(&page_home);
    UI_FullRefresh();
}
```

### 11.2 UI_Tick()

```c
void UI_Tick(void)
{
    /* 1. 处理输入事件 */
    ui_event_t *e = ui_input_poll();
    while (e) {
        /* 事件分发... */
        e = ui_input_poll();
    }

    /* 2. 更新动画 */
    ui_anim_tick(UI_TICK_MS);  /* UI_TICK_MS = 33 */

    /* 3. 渲染脏区域 */
    ui_page_draw();
}
```

### 11.3 主循环

```c
while (1) {
    UI_Tick();
    UART_Module_Poll();
    Delay_Ms(33);  /* ~30fps */
}
```

## 十二、业务UI实现

### 12.1 主框架（ui_main.c）

- **侧边栏**：宽200px，背景 `#F5F5F5`
  - 顶部：项目名 "ELAB"（主色 `#7EC8C8`）
  - 5个菜单项：Home / Apps / Games / Models / Settings
  - 选中项：背景 `#B8E0D2`，左侧4px竖条 `#7EC8C8`
- **内容区**：x=200, y=0, w=600, h=480

### 12.2 首页（ui_home.c）

- 日期标签（Montserrat 16，深灰 `#4A4A4A`）
- 时间标签（Montserrat 16，主色 `#7EC8C8`）
- 模块状态卡片：3~4个横向排列
- 系统状态栏

### 12.3 应用界面（ui_apps.c）

- 3×4图标网格，每页12个应用，2页翻页
- 图标按钮：150×90px，圆角16px
- 翻页导航："<" ">" 按钮和页码指示器

### 12.4 游戏界面（ui_games.c）

- 3×4图标网格，4个游戏单页
- 点击图标 → `ui_page_push(&game_xxx_page)`

### 12.5 全屏应用模板（ui_app_common.c）

- 页面flags：`UI_PAGE_FLAG_FULLSCREEN`
- 顶部标题栏（40px）：返回按钮 "<" + 应用名称
- 返回按钮点击或ESC → `ui_page_pop()`（带安全检查）

### 12.6 模块界面（ui_models.c）

- Tab视图：Display / Keyboard / Power / Core / BT-WiFi
- 每个Tab：卡片式布局，模块名称+状态圆点+参数

### 12.7 设置界面（ui_settings.c）

- 垂直列表，每项50px
- 背光：Slider
- 自动息屏：Switch
- 屏幕旋转：Button
- 恢复出厂：Button（红色）

## 十三、显示驱动接口

MiniUI 渲染引擎通过以下函数与显示驱动交互：

```c
void SSD1963_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void SSD1963_WriteData16(uint16_t data);
void SSD1963_WriteBuffer(const uint16_t *buf, uint32_t len);
void SSD1963_Clear(uint16_t color);
```

移植到其他屏幕时，只需实现这4个函数的等效接口。

## 十四、资源占用

| 项目 | 占用 |
|------|------|
| CodeFlash (MiniUI库) | ~30~50KB |
| CodeFlash (字体数据) | ~20~40KB |
| CodeFlash (业务UI) | ~30~50KB |
| SRAM (widget状态+临时缓冲) | ~10~30KB |
| SRAM (行缓冲区) | ~1.6KB (800×2) |
| SRAM (全屏帧缓冲) | **0** (无需) |

## 十五、移植到墨水屏/单色屏

### 15.1 概述

墨水屏（E-Ink）和单色LCD（如OLED、ST7920）与彩色TFT的核心区别：

| 特性 | 彩色TFT (SSD1963) | 墨水屏/单色屏 |
|------|-------------------|---------------|
| 颜色深度 | RGB565 (65536色) | 1bpp (黑白) 或 4级灰度 |
| 刷新速度 | 30fps+ | 0.5~5fps (墨水屏) / 30fps (OLED) |
| 局部刷新 | 支持 | 部分墨水屏支持，OLED支持 |
| 全刷需求 | 无 | 墨水屏需要定期全刷防残影 |
| 功耗 | 持续背光 | 墨水屏静态零功耗 |

### 15.2 移植步骤

#### 步骤1：实现显示驱动接口

在 `MiniUI/` 下新建驱动文件（如 `miniui_driver_epd.c`），实现4个核心函数：

```c
/* 以SPI接口墨水屏为例 */
static int16_t s_epd_width = 800;
static int16_t s_epd_height = 480;

void EPD_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    epd_set_partial_window(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

void EPD_WriteData16(uint16_t data)
{
    /* 单色屏：RGB565 → 黑白阈值 */
    uint8_t gray = ((data >> 11) & 0x1F) * 77 +
                   ((data >> 5) & 0x3F) * 150 +
                   (data & 0x1F) * 29;
    uint8_t bw = (gray > 128) ? 1 : 0;
    epd_write_byte(bw ? 0xFF : 0x00);
}

void EPD_WriteBuffer(const uint16_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        EPD_WriteData16(buf[i]);
    }
}

void EPD_Clear(uint16_t color)
{
    uint8_t bw = (color == UI_COLOR_WHITE) ? 0xFF : 0x00;
    epd_clear(bw);
}
```

#### 步骤2：修改渲染引擎的驱动调用

在 `miniui_render.c` 中，将所有 `SSD1963_*` 调用替换为可配置的函数指针：

```c
/* miniui_render.c 顶部添加 */
static void (*s_drv_set_window)(uint16_t, uint16_t, uint16_t, uint16_t) = SSD1963_SetWindow;
static void (*s_drv_write_data16)(uint16_t) = SSD1963_WriteData16;
static void (*s_drv_write_buffer)(const uint16_t *, uint32_t) = SSD1963_WriteBuffer;
static void (*s_drv_clear)(uint16_t) = SSD1963_Clear;

void ui_render_set_driver(
    void (*set_window)(uint16_t, uint16_t, uint16_t, uint16_t),
    void (*write_data16)(uint16_t),
    void (*write_buffer)(const uint16_t *, uint32_t),
    void (*clear)(uint16_t))
{
    s_drv_set_window = set_window;
    s_drv_write_data16 = write_data16;
    s_drv_write_buffer = write_buffer;
    s_drv_clear = clear;
}
```

然后在 `UI_Init()` 中调用：

```c
ui_render_init();
ui_render_set_driver(EPD_SetWindow, EPD_WriteData16, EPD_WriteBuffer, EPD_Clear);
```

#### 步骤3：调整颜色方案

墨水屏只有黑白两色，需要调整所有UI颜色：

```c
/* 墨水屏配色 */
#undef  UI_COLOR_BG_MAIN
#define UI_COLOR_BG_MAIN      UI_COLOR_WHITE

#undef  UI_COLOR_PRIMARY
#define UI_COLOR_PRIMARY      UI_COLOR_BLACK

#undef  UI_COLOR_SIDEBAR_BG
#define UI_COLOR_SIDEBAR_BG   UI_COLOR_WHITE

#undef  UI_COLOR_SIDEBAR_SEL
#define UI_COLOR_SIDEBAR_SEL  UI_COLOR_BLACK

#undef  UI_COLOR_TEXT_DARK
#define UI_COLOR_TEXT_DARK    UI_COLOR_BLACK

#undef  UI_COLOR_TEXT_LIGHT
#define UI_COLOR_TEXT_LIGHT   UI_COLOR_BLACK
```

建议在 `miniui_color.h` 中使用条件编译：

```c
#ifdef MINIUI_MONOCHROME
#define UI_COLOR_BG_MAIN      UI_COLOR_WHITE
#define UI_COLOR_PRIMARY      UI_COLOR_BLACK
#define UI_COLOR_TEXT_DARK    UI_COLOR_BLACK
#define UI_COLOR_SIDEBAR_BG   UI_COLOR_WHITE
#define UI_COLOR_SIDEBAR_SEL  UI_COLOR_BLACK
#else
#define UI_COLOR_BG_MAIN      UI_RGB565(0xF5, 0xF5, 0xF5)
#define UI_COLOR_PRIMARY      UI_RGB565(0x7E, 0xC8, 0xC8)
#define UI_COLOR_TEXT_DARK    UI_RGB565(0x4A, 0x4A, 0x4A)
#define UI_COLOR_SIDEBAR_BG   UI_RGB565(0xF5, 0xF5, 0xF5)
#define UI_COLOR_SIDEBAR_SEL  UI_RGB565(0xB8, 0xE0, 0xD2)
#endif
```

#### 步骤4：禁用动画和抗锯齿

墨水屏刷新慢，动画和抗锯齿无意义：

```c
#ifdef MINIUI_MONOCHROME
/* 禁用动画：ui_anim_tick() 直接返回 */
void ui_anim_tick(uint16_t delta_ms) { (void)delta_ms; }

/* 使用1bpp字体，禁用抗锯齿 */
#define UI_FONT_BPP  1
#endif
```

#### 步骤5：调整刷新策略

墨水屏需要特殊刷新策略：

```c
#ifdef MINIUI_MONOCHROME
/* 降低刷新频率 */
#define UI_TICK_MS  200  /* 5fps，墨水屏够用 */

/* 定期全刷防残影 */
static uint16_t s_refresh_counter = 0;
#define EPD_FULL_REFRESH_INTERVAL  50  /* 每50次局部刷新后全刷一次 */

void ui_page_draw(void)
{
    /* ... 正常脏区域渲染 ... */

    s_refresh_counter++;
    if (s_refresh_counter >= EPD_FULL_REFRESH_INTERVAL) {
        s_refresh_counter = 0;
        EPD_FullRefresh();  /* 墨水屏全刷 */
    }
}
#endif
```

#### 步骤6：优化侧边栏为图标模式

墨水屏上文字渲染效果差，建议侧边栏改为纯图标模式：

```c
#ifdef MINIUI_MONOCHROME
#define SIDEBAR_WIDTH  60   /* 缩窄侧边栏 */
/* 侧边栏只显示图标，不显示文字 */
#endif
```

### 15.3 4级灰度墨水屏优化

如果目标墨水屏支持4级灰度（如某些电子书阅读器），可以进一步优化：

```c
/* RGB565 → 4级灰度 (0=黑, 3=白) */
static uint8_t rgb565_to_gray4(uint16_t color)
{
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    uint8_t gray = (r * 77 + g * 150 + b * 29) >> 8;
    if (gray < 64) return 0;
    if (gray < 128) return 1;
    if (gray < 192) return 2;
    return 3;
}
```

4级灰度可以实现简单的阴影效果和文字抗锯齿。

### 15.4 完整移植检查清单

| 序号 | 项目 | 说明 |
|------|------|------|
| 1 | 实现4个驱动函数 | SetWindow / WriteData16 / WriteBuffer / Clear |
| 2 | 注册驱动 | 调用 `ui_render_set_driver()` |
| 3 | 调整颜色方案 | 使用 `MINIUI_MONOCHROME` 条件编译 |
| 4 | 禁用动画 | `ui_anim_tick()` 空实现 |
| 5 | 使用1bpp字体 | 禁用抗锯齿 |
| 6 | 降低刷新率 | `UI_TICK_MS` 改为 200~500 |
| 7 | 添加全刷逻辑 | 定期全刷防残影 |
| 8 | 缩窄侧边栏 | 纯图标模式，宽度60px |
| 9 | 调整widget尺寸 | 墨水屏上控件需要更大更清晰 |
| 10 | 测试触摸输入 | 确保触摸坐标映射正确 |

### 15.5 常见墨水屏驱动IC参考

| 驱动IC | 分辨率 | 灰度 | 接口 | 备注 |
|--------|--------|------|------|------|
| SSD1680 | 200×200 | 4级 | SPI | 小尺寸，常见于价格标签 |
| SSD1681 | 200×200 | 1bpp | SPI | SSD1680的1bpp版本 |
| UC8176 | 800×480 | 4级 | SPI | 大尺寸，适合本项目 |
| IL3897 | 128×296 | 1bpp | SPI | 常见于电子墨水徽章 |
| ED060SC4 | 800×600 | 16级 | 并口 | 高端电子书阅读器 |

## 十六、已知限制与注意事项

1. **Label文本指针**：`ui_label_set_text()` 不复制字符串，传入的指针必须在Label使用期间有效
2. **页面栈溢出**：超过16层时，最底层页面被覆盖，不会崩溃但会丢失状态
3. **脏区域数量**：超过8个dirty region时退化为全屏刷新
4. **抗锯齿背景**：抗锯齿文本需要已知背景色才能正确混合，透明背景时直接绘制前景色
5. **动画浮点运算**：缓动函数使用 `float`，无FPU的MCU上可能较慢
6. **线程安全**：MiniUI非线程安全，所有UI操作必须在同一线程/主循环中执行
7. **字体字符集**：当前仅支持ASCII 32~126，不支持中文（可扩展unicode_list）
