# MiniUI 游戏应用编写指南

本文档总结在 MiniUI 框架上开发游戏类应用时的关键注意事项，基于实际开发经验。

---

## 1. 游戏页面架构

### 使用 `ui_app_page_init` 创建页面

游戏页面基于 `ui_app_page_t`，它提供标题栏、返回按钮等通用组件：

```c
ui_app_page_t s_game_mygame;
ui_widget_t *s_mygame_widgets[3];  // touch_area + title + back_button

void game_mygame_init(void)
{
    ui_app_page_init(&s_game_mygame, "My Game", 0x205);
    // 设置 on_update（游戏逻辑）和 on_draw（渲染）
    ui_page_set_update_cb(&s_game_mygame.page, mygame_update);
    s_game_mygame.page.on_draw = mygame_draw;
    // ...
    ui_page_set_widgets(&s_game_mygame.page, s_mygame_widgets, 3);
    ui_page_register(&s_game_mygame.page);
}
```

### on_update / on_draw 分离模式

游戏必须将逻辑更新和渲染分离为两个回调：

- **`on_update`**：每帧由 `UI_Tick` 调用，用于更新游戏逻辑、标记脏区域
- **`on_draw`**：由合成渲染器调用，只在脏区域内绘制，绘图原语自动裁剪到目标范围

```c
static void mygame_update(ui_page_t *page)
{
    // 更新游戏逻辑
    // 标记脏区域：ui_page_invalidate(&rect)
}

static void mygame_draw(ui_page_t *page, ui_rect_t *dirty)
{
    // 标题栏背景（必须！游戏覆盖了 ui_app_page_draw）
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    // 绘制游戏内容（自动裁剪到 dirty 范围）
    draw_game_elements(dirty);
}
```

### 游戏必须绘制标题栏背景

**重要**：游戏的 `on_draw` 覆盖了 `ui_app_page_draw`（后者绘制标题栏背景和 widgets），因此游戏的 `on_draw` 必须在开头手动绘制标题栏背景：

```c
ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);
```

标题栏上的 widgets（返回按钮、标题文字）由合成渲染器自动绘制，无需手动处理。

---

## 2. 合成渲染器（Compositing Renderer）

### 核心原理

MiniUI 使用**行缓冲区合成渲染器**，所有绘图原语写入行缓冲区（`g_compose_buf`），而非直接写入 SSD1963 GRAM。页面管理器负责将合成后的数据批量刷入 GRAM。

**批量合成**（`UI_COMPOSE_BATCH = 8`）：每次合成 8 行，一次 `SetWindow` 调用写入 8 行数据，将 SetWindow 开销从每行 11 次 FMC 写降低到每 8 行 11 次。

### 渲染流程

```
UI_Tick()
    │
    ├─ 1. ui_input_tick() + ui_input_poll() 处理输入
    ├─ 2. ui_anim_tick() 更新动画
    ├─ 3. page->on_update() 更新游戏逻辑、标记脏区域
    └─ 4. ui_page_draw() 合成渲染
          ├─ 遍历脏区域列表
          │   └─ 每个脏区域按 UI_COMPOSE_BATCH 行分批：
          │       ├─ ui_render_begin_rect(&batch_target)
          │       ├─ ui_render_fill_line_buf() 填充背景
          │       ├─ sidebar_draw() (非全屏页面)
          │       ├─ page->on_draw(page, &batch_target)
          │       ├─ widget draw_cb's
          │       └─ ui_render_flush_rows(y, batch_h, x, w)
          └─ 清空脏列表
```

### 绘图原语自动裁剪

所有绘图原语（`ui_draw_fill_rect`、`ui_draw_text` 等）自动裁剪到当前合成目标矩形（`batch_target`）。游戏代码**无需手动设置裁剪区域**，也不存在 `ui_render_set_clip` / `ui_render_reset_clip` 函数。

### 脏区域管理

```c
// 标记单个区域为脏
void ui_page_invalidate(const ui_rect_t *rect);

// 标记全屏为脏
void ui_page_invalidate_all(void);

// 获取当前脏区域列表（只读）
const ui_dirty_list_t *ui_page_get_dirty_list(void);

// 清除脏区域列表
void ui_page_clear_dirty(void);
```

脏区域列表最多 **256 个**（`UI_MAX_DIRTY_REGIONS`），重叠区域会自动合并。超出上限时退化为全屏脏区域。

### 标记脏区域的时机

```c
// 球移动：标记旧位置和新位置
tb_invalidate_ball(old_x, old_y, r);
tb_invalidate_ball(new_x, new_y, r);

// HUD 文字变化
tb_invalidate_hud();

// 状态切换：全屏重绘
ui_page_invalidate_all();
```

---

## 3. Widget 绘制

### 合成渲染器自动绘制 Widgets

合成渲染器在每批行的合成过程中，会依次调用：
1. 侧边栏绘制（非全屏页面）
2. 页面 `on_draw` 回调
3. 所有与当前脏区域相交的 widgets 的 `draw_cb`

因此 widgets（返回按钮、标题文字）**无需手动绘制**。游戏只需在 `on_draw` 中绘制标题栏背景和游戏内容。

### 页面退出时的残留问题

`ui_page_pop` 中调用 `ui_page_invalidate_all()` 标记全屏脏区域，下一帧合成渲染器会完整重绘 sidebar 和内容区域，覆盖游戏残留内容。无需手动清屏。

---

## 4. 事件系统

### 事件类型

MiniUI 使用统一的事件模型，所有输入源（触摸/鼠标/键盘）生成相同的事件类型：

```c
typedef enum {
    // 触摸/鼠标指针事件
    UI_EVENT_NONE         = 0,
    UI_EVENT_DOWN,              // 按下
    UI_EVENT_UP,                // 释放（短按后）
    UI_EVENT_MOVE,              // 按下后移动
    UI_EVENT_CLICK,             // 单击完成（down + up 在时间和距离内）
    UI_EVENT_DOUBLE_CLICK,      // 双击
    UI_EVENT_LONG_PRESS,        // 长按（首次触发）
    UI_EVENT_LONG_PRESS_REPEAT, // 长按重复触发
    UI_EVENT_SWIPE_UP/DOWN/LEFT/RIGHT, // 滑动手势

    // 键盘事件
    UI_EVENT_KEY_DOWN/UP,       // 按键按下/释放
    UI_EVENT_KEY_CLICK,         // 按键点击
    UI_EVENT_KEY_DOUBLE_CLICK,  // 按键双击
    UI_EVENT_KEY_LONG_PRESS,    // 按键长按
    UI_EVENT_KEY_LONG_REPEAT,   // 按键长按重复

    // 逻辑键事件
    UI_EVENT_KEY_UP_ARROW/DOWN_ARROW/LEFT_ARROW/RIGHT_ARROW,
    UI_EVENT_KEY_OK,            // 确认/回车
    UI_EVENT_KEY_BACK,          // 返回/ESC

    UI_EVENT_PRESS_CANCEL,      // 按下取消（如滑动中断捕获）
} ui_event_type_t;
```

### 事件结构体

```c
typedef struct {
    ui_event_type_t type;
    ui_input_source_t source;   // UI_INPUT_TOUCH / MOUSE / KEYBOARD / CORE_KEY
    ui_point_t pos;             // 触摸/鼠标坐标
    ui_point_t delta;           // 移动偏移
    uint8_t touch_id;           // 触点 ID (0~4)，非触摸为 UI_TOUCH_ID_NONE
    uint8_t key_code;           // 键码
    uint8_t key_modifiers;      // 修饰键位掩码
    uint8_t mouse_buttons;      // 鼠标按键位掩码
    int8_t scroll_delta;        // 鼠标滚轮
} ui_event_t;
```

### 多点触控支持

MiniUI 支持最多 5 点同时触控（`UI_MAX_TOUCH_POINTS = 5`）。

**多点触控事件分发规则**：
- 第一根手指（主触点）的事件走正常 capture 机制
- 第二根及以后手指的 **DOWN 和 UP** 事件广播给页面的所有 widget（支持同时按多个按钮）
- **MOVE 事件不广播**——防止一根手指移动时触发其他按钮的 hover/pressed 状态

### 需要多点触控的游戏实现模式

对于需要同时按多个按钮的游戏（如 Contra），使用以下模式：

```c
/* 每个触点跟踪它按下的按钮（位掩码） */
static uint8_t s_touch_btns[UI_MAX_TOUCH_POINTS];

#define BTN_LEFT   (1 << 0)
#define BTN_RIGHT  (1 << 1)
#define BTN_JUMP   (1 << 2)
#define BTN_SHOOT  (1 << 3)

static uint8_t hit_test(int16_t x, int16_t y) {
    uint8_t mask = 0;
    if (in_dpad_left(x, y))  mask |= BTN_LEFT;
    if (in_dpad_right(x, y)) mask |= BTN_RIGHT;
    if (in_btn_jump(x, y))   mask |= BTN_JUMP;
    if (in_btn_shoot(x, y))  mask |= BTN_SHOOT;
    return mask;
}

static void recalc_inputs(void) {
    uint8_t combined = 0;
    for (int i = 0; i < UI_MAX_TOUCH_POINTS; i++)
        combined |= s_touch_btns[i];
    s_game.input_left  = (combined & BTN_LEFT)  != 0;
    s_game.input_right = (combined & BTN_RIGHT) != 0;
    s_game.input_jump  = (combined & BTN_JUMP)  != 0;
    s_game.input_shoot = (combined & BTN_SHOOT) != 0;
}

static void touch_event(ui_widget_t *w, ui_event_t *e) {
    if (e->type == UI_EVENT_DOWN) {
        s_touch_btns[e->touch_id] = hit_test(e->pos.x, e->pos.y);
        recalc_inputs();
    } else if (e->type == UI_EVENT_MOVE) {
        s_touch_btns[e->touch_id] = hit_test(e->pos.x, e->pos.y);
        recalc_inputs();
    } else if (e->type == UI_EVENT_UP) {
        s_touch_btns[e->touch_id] = 0;
        recalc_inputs();
    }
}
```

**关键点**：
- 每个触点独立跟踪它按下的按钮
- 一个触点抬起时，只清除该触点的按钮，不影响其他触点
- `recalc_inputs` 用 OR 合并所有触点的按钮状态
- MOVE 事件只在 capture widget 内处理（不会广播），避免误触

### 事件捕获（Capture）机制

- `UI_EVENT_DOWN` 时，命中的 widget 自动获得 capture
- Capture 期间所有事件发给 capture widget
- `UI_EVENT_UP` / `UI_EVENT_CLICK` / `UI_EVENT_DOUBLE_CLICK` 后释放 capture
- 滑动手势触发 `UI_EVENT_PRESS_CANCEL` 并释放 capture
- 多点触控：只有主触点的 UP 才释放 capture

### 触摸区域 widget

游戏需要全屏触摸区域来捕获点击：

```c
ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                        UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
ui_widget_init(&s_touch_area, &touch_rect);
s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
s_touch_area.event_cb = tb_touch_event;
```

Widget 顺序影响事件分发：高 index 的 widget 先接收事件。返回按钮应在最高 index，确保它能优先处理点击。

---

## 5. 帧率控制与防撕裂

### VSYNC 同步（防撕裂）

SSD1963 是单帧缓冲架构，LCD 面板持续从帧缓冲读取数据驱动屏幕。使用 SSD1963 的 TE（Tearing Effect）信号同步写入时机。

**硬件连接**：SSD1963 TE 引脚 → CH32H417 PB8

```c
while(1) {
    Touch_Scan();          // I2C 通信，不写 GRAM
    SSD1963_WaitVSync();   // 等待消隐期开始
    UI_Tick();             // 立即合成渲染，在消隐期内完成
    // ... 帧率限制 ...
}
```

### 主循环帧率限制

```c
uint32_t cycles_per_ms = SystemCoreClock / 1000;
uint32_t frame_cycles = cycles_per_ms * 40;  // 40ms ≈ 25fps
uint32_t last_frame = __get_UCYCLE();

while(1) {
    Touch_Scan();
    SSD1963_WaitVSync();
    UI_Tick();

    uint32_t now = __get_UCYCLE();
    uint32_t elapsed = now - last_frame;
    if (elapsed < frame_cycles) {
        uint32_t remaining_ms = (frame_cycles - elapsed) / cycles_per_ms;
        if (remaining_ms > 0) Delay_Ms(remaining_ms);
    } else {
        Delay_Ms(1);
    }
    last_frame = __get_UCYCLE();
}
```

---

## 6. DMA 与 FMC 写入

### DMA 写入 FMC NORSRAM 不可靠

**坑**：CH32H417 的 FMC NORSRAM 控制器没有写 FIFO，DMA 写入会导致 SSD1963 数据丢失。

结论：CPU 写入 + 批量合成渲染是更可靠的方案。

---

## 7. 防闪烁经验总结

### 7.1 撕裂（Tearing）

**解决方案**：使用 SSD1963 TE 信号同步写入时机（见第 5 章 VSYNC 同步）。

### 7.2 闪烁（Flickering）

合成渲染器天然防闪烁：每批行的数据在行缓冲区中完整合成后一次性刷入 GRAM，不存在"先擦后画"的中间状态。配合 VSYNC 同步，LCD 永远不会扫描到中间帧。

### 7.3 残影（Ghosting）

**原因**：脏区域标记不够大，没有覆盖物体的完整范围。

**解决方案**：标记脏区域时，确保包含物体的完整边界。宁可多标一些也不要少标。

**常见残影场景**：
1. **实体被消灭时**：同时标记 prev 位置和当前位置
2. **无敌闪烁时**：即使位置没变，可见性变化也需要重绘
3. **提前 return 导致脏区域未标记**：在 return 前手动标记旧位置

### 7.4 相机移动时的局刷策略

相机移动时不要全刷，而是标记所有移动元素的旧位置和新位置：

1. **边缘条**：相机滚动时新出现的屏幕边缘区域
2. **平台**：在旧相机偏移和新相机偏移下各标记一次
3. **视差背景**：在旧偏移和新偏移下各标记一次
4. **所有实体**：旧位置 + 新位置
5. **HUD**：仅在数据变化时标记

### 7.5 VSYNC 等待位置

**坑**：`SSD1963_WaitVSync()` 放在 `Touch_Scan()` 之前，Touch_Scan 的 I2C 通信耗时 1-2ms，等开始绘制时消隐期已过。

**解决方案**：先 `Touch_Scan()`，再 `SSD1963_WaitVSync()`，然后立即 `UI_Tick()`。

### 7.6 TE 信号超时保护

```c
void SSD1963_WaitVSync(void)
{
    uint32_t timeout = 100000;
    while (SSD1963_TE_IsHigh()) { if (--timeout == 0) return; }
    timeout = 100000;
    while (!SSD1963_TE_IsHigh()) { if (--timeout == 0) return; }
}
```

---

## 8. 游戏开发检查清单

| 序号 | 项目 | 说明 |
|------|------|------|
| 1 | 设置 on_update 回调 | 游戏逻辑更新 + 标记脏区域 |
| 2 | on_draw 绘制标题栏背景 | `ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY)` |
| 3 | on_draw 绘制游戏内容 | 绘图原语自动裁剪到 dirty 范围 |
| 4 | 脏区域标记完整 | 旧位置 + 新位置，宁多勿少 |
| 5 | 触摸区域 widget | 全屏触摸区域，事件回调处理 DOWN/MOVE/UP |
| 6 | Widget 顺序 | 返回按钮在最高 index |
| 7 | 多点触控 | 需要多按钮的游戏使用 per-touch 跟踪模式 |
| 8 | VSYNC 同步 | 先 Touch_Scan，再 WaitVSync，再 UI_Tick |
