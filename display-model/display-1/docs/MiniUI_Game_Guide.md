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
    ├─ 1. ui_input_tick() + ui_input_poll() 处理输入、分发事件
    │     ├─ 页面切换保护：事件处理中如触发 ui_page_push，跳过后续 broadcast/capture
    │     ├─ 鼠标 capture 强制释放：mouse DOWN 到达时若 capture 被 touch 持有，强制释放
    │     └─ on_page_event 拦截：非指针事件先走页面级回调，返回 true 则跳过 widget
    ├─ 2. ui_anim_tick() 更新动画
    ├─ 3. page->on_update() 更新游戏逻辑、标记脏区域
    ├─ 4. ui_input_invalidate_cursor() 标记鼠标光标脏区域
    ├─ 5. ui_page_draw() 合成渲染
    │     ├─ 遍历脏区域列表
    │     │   └─ 每个脏区域按 UI_COMPOSE_BATCH 行分批：
    │     │       ├─ ui_render_begin_rect(&batch_target)
    │     │       ├─ ui_render_fill_line_buf() 填充背景
    │     │       ├─ sidebar_draw() (非全屏页面)
    │     │       ├─ page->on_draw(page, &batch_target)
    │     │       ├─ widget draw_cb's
    │     │       └─ ui_render_flush_rows(y, batch_h, x, w)
    │     └─ 清空脏列表
    └─ 6. ui_input_cursor_rendered() 更新光标已渲染位置跟踪
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
    uint8_t key_code;           // 键码（UI_KEY_* 或原始 HID 码）
    uint8_t key_modifiers;      // 修饰键位掩码
    uint8_t mouse_buttons;      // 鼠标按键位掩码
    int8_t scroll_delta;        // 鼠标滚轮
    uint8_t char_code;          // 可打印 ASCII 字符码（0 表示不可打印）
} ui_event_t;
```

### 多点触控支持

MiniUI 支持最多 5 点同时触控（`UI_MAX_TOUCH_POINTS = 5`）。

**多点触控事件分发规则**：
- 第一根手指（主触点）的事件走正常 capture 机制
- 第二根及以后手指的 **DOWN 和 UP** 事件广播给页面的所有 widget（支持同时按多个按钮）
- **MOVE 事件不广播**——防止一根手指移动时触发其他按钮的 hover/pressed 状态
- **统一 hit test 过滤**：`ui_widget_event` 对所有 DOWN/UP 指针事件自动执行 hit test，不在 widget 范围内的触点不会触发该 widget。这意味着广播不会误触不在触摸位置下的 widget

### Widget 事件过滤（ui_widget_event）

`ui_widget_event` 是所有事件到达 widget 回调之前的统一入口。它对 DOWN/UP 指针事件（触摸和鼠标）自动执行 hit test 过滤：

```c
void ui_widget_event(ui_widget_t *w, ui_event_t *e)
{
    if (!w || !(w->flags & UI_WIDGET_FLAG_ENABLED)) return;

    // 过滤广播的 DOWN/UP 指针事件：只有触摸位置在 widget 内才传递
    if ((e->type == UI_EVENT_DOWN || e->type == UI_EVENT_UP) &&
        (e->source == UI_INPUT_TOUCH || e->source == UI_INPUT_MOUSE)) {
        if (!ui_widget_hit_test(w, e->pos.x, e->pos.y)) return;
    }

    if (w->event_cb) w->event_cb(w, e);
}
```

**影响**：
- Capture widget 通过 UI_Tick 的独立路径接收事件，不受此过滤影响
- 广播路径（多触点 DOWN/UP）经过此过滤，确保只有被实际触摸的 widget 收到事件
- 游戏代码**不需要**在各自的 `event_cb` 中手动做 hit test 判断

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

**⚠️ 严重警告：不要同时使用 touch_area 和独立 button widget**

如果一个游戏使用了上述 per-touch 跟踪模式（通过一个全屏 touch_area widget），**不要**再为各个按钮注册独立的 button widget。原因：

1. Button widget 使用简单的 DOWN=设置 PRESSED / UP=清除 PRESSED 逻辑
2. 当 touch_area 获得 capture 后，button widget 只能通过广播接收 DOWN/UP
3. 广播虽然经过 hit test 过滤，但 button widget 的简单状态机无法正确跟踪哪个触点对应哪个按钮
4. 结果：一根手指按下方向键，另一根手指按下跳跃键时，方向键的 button widget 会被错误设置为 PRESSED 状态

**正确做法**：只使用一个 touch_area widget，所有按钮的 hit test 和状态管理在 touch_event 回调中完成。

### 事件捕获（Capture）机制

- `UI_EVENT_DOWN` 时，命中的 widget 自动获得 capture
- Capture 期间所有事件发给 capture widget
- `UI_EVENT_UP` / `UI_EVENT_CLICK` / `UI_EVENT_DOUBLE_CLICK` 后释放 capture
- 滑动手势触发 `UI_EVENT_PRESS_CANCEL` 并释放 capture
- 多点触控：只有主触点的 UP 才释放 capture
- **页面切换保护**：如果 capture widget 的事件回调中触发了页面切换（如 `ui_page_push`），UI_Tick 会检测页面变化并跳过后续的 broadcast 和 capture 设置，避免事件泄漏到新页面
- **鼠标 capture 强制释放**：当鼠标 DOWN 到达时，如果 capture 正被一个触摸触点持有，会强制释放旧 capture，让鼠标可以正常交互
- **Hit test 路径保护**：在无 capture 的 hit test 路径中，如果事件处理导致页面切换，立即停止遍历剩余 widget

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

## 4.1 输入系统鲁棒性

### 过期指针恢复（Stale Pointer Recovery）

输入系统内置了过期指针检测和恢复机制。如果某个触点超过 5 秒（`UI_POINTER_STALE_TIMEOUT_MS`）没有收到新的原始输入，系统会自动合成一个 UP 事件并释放该触点的 capture。这处理了以下异常情况：

- UART 丢包导致触摸释放事件丢失
- 事件队列溢出导致 UP 事件被丢弃
- 硬件异常导致触点卡死

### 键盘 KEY_CLICK 时序

键盘的 `UI_EVENT_KEY_CLICK` 事件采用延迟确认机制：

1. 按键释放时，如果按压时间 ≤ 300ms，设置 `click_pending = true`
2. `ui_input_tick` 中的 `check_click_timeouts` 检查 pending 状态
3. 当超过双击间隔（200ms）且没有第二次按键时，触发 `UI_EVENT_KEY_CLICK`
4. 如果在间隔内再次按下并释放相同键，触发 `UI_EVENT_KEY_DOUBLE_CLICK`

**注意**：`click_pending` 在按键释放时**不要手动清除**，否则 KEY_CLICK 永远不会触发。

### 事件队列溢出保护

当输入事件队列满时（32 个事件），对关键事件（UP、CLICK、SWIPE 等）会主动丢弃最老的 MOVE 事件来腾出空间。丢失一个 MOVE 事件无害，但丢失 UP 事件会导致 capture 卡死。

---

## 4.2 页面级事件拦截（on_page_event）

### 问题背景

`UI_Tick` 的事件分发流程为：`ui_input_poll()` 取事件 → 分发给 widget → 调用 `on_update`。如果应用/游戏在 `on_update` 中调用 `ui_input_poll()` 取键盘事件，队列已被排空，永远返回 NULL。这是文件管理器和编辑器键盘失效的根因。

### 解决方案

页面可通过 `on_page_event` 回调在 **widget 分发之前** 拦截非指针事件（键盘/核心按键）：

```c
// 注册回调
ui_page_set_event_cb(&my_page.page, my_page_event);

// 回调实现：返回 true 表示已消费，阻止后续传播
static bool my_page_event(ui_page_t *page, ui_event_t *e)
{
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;  // 不拦截触摸/鼠标

    // 处理键盘事件...
    return true;  // 已消费，不传给 widget
}
```

### 事件分发顺序（无 capture 路径）

```
1. Sidebar 处理（非全屏页面）
2. on_page_event 拦截（非指针事件）
3. 全局 BACK 键处理（页面可拦截 BACK 做目录导航等）
4. Widget 分发（指针 hit-test / 键盘广播）
```

### capture 路径

当有 capture widget 时，非指针事件也先走 `on_page_event`。这确保 modal 对话框（输入框、菜单）能拦截键盘事件，即使之前有触摸 capture。

### 禁止在 on_update 中轮询事件

**⚠️ 严重警告：绝对不要在 `on_update` 中调用 `ui_input_poll()`！**

```c
// ❌ 错误：on_update 中轮询，队列已被 UI_Tick 排空
static void bad_update(ui_page_t *page) {
    ui_event_t *ev = ui_input_poll();  // 永远返回 NULL
    while (ev) { handle(ev); ev = ui_input_poll(); }
}

// ✅ 正确：用 on_page_event 拦截
static bool good_event(ui_page_t *page, ui_event_t *e) {
    if (e->source == UI_INPUT_KEYBOARD) { handle(e); return true; }
    return false;
}
// 在 init 中注册：ui_page_set_event_cb(&page, good_event);
```

### 游戏 vs 应用的选择

| 场景 | 推荐方式 | 原因 |
|------|----------|------|
| 游戏（全屏 + 触摸区域 widget） | widget `event_cb` 广播 | 游戏已有全屏触摸区域，键盘事件通过广播自然到达 |
| 应用（列表、对话框、输入框） | `on_page_event` | 需要拦截键盘做导航/文本输入，且阻止 widget 重复处理 |

---

## 4.3 char_code 与字符输入

### char_code 字段

`ui_event_t.char_code` 携带 USB HID 码转换后的 **可打印 ASCII 字符**（0 表示不可打印）。输入系统在生成键盘事件时自动填充，包含 Shift 修饰（如 Shift+A → `'A'`，Shift+1 → `'!'`）。

### 使用方法

```c
// 字符输入：检查 char_code 而非 key_code
if (e->type == UI_EVENT_KEY_CLICK &&
    e->char_code >= 0x20 && e->char_code <= 0x7E) {
    insert_char((char)e->char_code);  // 直接插入 ASCII 字符
}

// Backspace：char_code == 0x08
if (e->type == UI_EVENT_KEY_CLICK && e->char_code == 0x08) {
    delete_back();
}

// 导航键：仍然用 key_code（UI_KEY_UP/DOWN/LEFT/RIGHT/OK/BACK）
if (e->type == UI_EVENT_KEY_DOWN && e->key_code == UI_KEY_UP) {
    move_cursor_up();
}
```

### key_code vs char_code

| 字段 | 用途 | 示例 |
|------|------|------|
| `key_code` | 导航/功能键识别 | `UI_KEY_UP`(0x01), `UI_KEY_OK`(0x05) |
| `char_code` | 可打印字符输入 | `'a'`(0x61), `'!'`(0x21), `' '`(0x20) |

对于字母/数字/符号键，`key_code` 携带原始 HID 码或 `UI_KEY_*` 常量，**不适合作为字符使用**。始终用 `char_code` 获取 ASCII 字符。

### KEY_DOWN vs KEY_CLICK 的选用

| 场景 | 推荐事件类型 | 原因 |
|------|--------------|------|
| 导航（箭头键） | `KEY_DOWN` + `KEY_LONG_REPEAT` | 即时响应 + 持续滚动 |
| 确认/返回 | `KEY_CLICK` | 防止与 KEY_DOWN 重复触发 |
| 字符输入 | `KEY_CLICK` | 等双击窗口过后再插入字符，防止误输 |

---

## 4.4 HID 码与字符映射

### 标准 US QWERTY 映射

输入系统的 `hid_to_ascii` 使用**标准 USB HID Usage Tables** (Page 0x07) 的 US QWERTY 映射，与 Core 侧 `CH9350_KeyCode_Name` 一致。USB HID 码由物理键位决定，与键盘上印刷的字符无关。

```
HID 0x04 (物理位置 A) → 'a' / Shift+'a'='A'
HID 0x14 (物理位置 Q) → 'q' / Shift+'q'='Q'
HID 0x1E (物理位置 1) → '1' / Shift+'1'='!'
HID 0x2D (物理位置 -) → '-' / Shift+'-'='_'
HID 0x33 (物理位置 ;) → ';' / Shift+';'=':'
```

### 键盘布局 vs HID 码

`keyboard-layout.json` 描述的是键盘上**印刷的字符**（应用层布局），不影响 HID 报告。例如一个自定义 40% 键盘上 Q 键位置印着 "Q 1"，但 CH9350 仍然报告 HID 0x14，`hid_to_ascii` 将其转为 `'q'`/`'Q'`。

如果应用需要将 HID 码映射到自定义布局的字符，应在应用层自行处理，而非修改输入系统的 `hid_to_ascii`。

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

相机移动时不要全刷，而是精准标记需要重绘的区域。以下是经过实战验证的优化策略：

1. **边缘条**：相机滚动时新出现的屏幕边缘区域
2. **平台**：在旧相机偏移和新相机偏移下各标记一次
3. **静态背景**：不使用视差滚动的背景元素（如固定位置的山脉），无需标记脏区域
4. **水平均匀地面**：如果地面在水平方向上是相同的（如一条连续的绿色地面），水平移动相机不会改变其在画面中的显示，**可以完全跳过**脏区域标记
5. **地面间隙**：只在间隙进入或离开可见区域时才标记脏区域（通过比较旧/新相机偏移下的可见性）
6. **所有实体**：旧位置 + 新位置
7. **HUD**：仅在数据变化时标记

**关键原则**：FMC 发送像素的开销远大于计算开销，应通过精准计算减少传输的像素量，而不是简单粗暴地标记整个区域。

```c
// 示例：地面间隙的精准标记
static bool gap_visible(int16_t gap_x, int16_t cam_x) {
    int16_t screen_x = gap_x - cam_x;
    return (screen_x + GAP_W > 0 && screen_x < SCREEN_W);
}

// 只在可见性变化时标记
bool old_vis = gap_visible(gap_x, old_cam_x);
bool new_vis = gap_visible(gap_x, new_cam_x);
if (old_vis != new_vis) {
    // 标记间隙相关区域为脏
}
```

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
| 5 | 脏区域精准计算 | FMC 开销远大于计算，用精准标记减少传输像素量 |
| 6 | 触摸区域 widget | 全屏触摸区域，事件回调处理 DOWN/MOVE/UP |
| 7 | Widget 顺序 | 返回按钮在最高 index |
| 8 | 多点触控 | 需要多按钮的游戏使用 per-touch 跟踪模式 |
| 9 | 不要混用输入模式 | touch_area + 独立 button widget 会冲突，只用一种 |
| 10 | 不需要手动 hit test | `ui_widget_event` 已统一处理 DOWN/UP 的 hit test 过滤 |
| 11 | VSYNC 同步 | 先 Touch_Scan，再 WaitVSync，再 UI_Tick |
| 12 | 静态背景 | 背景元素使用固定位置，避免视差滚动带来的大量重绘 |
| 13 | 禁止在 on_update 轮询事件 | 用 `on_page_event` 拦截键盘，不要用 `ui_input_poll()` |
| 14 | 字符输入用 char_code | 不要用 `key_code` 当字符，始终检查 `e->char_code` |
| 15 | HID 码是标准 US QWERTY | `hid_to_ascii` 与 `CH9350_KeyCode_Name` 一致，键盘布局不影响 HID 码 |
